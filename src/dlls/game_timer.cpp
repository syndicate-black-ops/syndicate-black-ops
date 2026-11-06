//=========================================================
// game_timer: a HUD countdown timer for level designers, see game_timer.h and docs/GAME_TIMER.md.
//
// The timing code follows SBO Source's CGameTimer (src/game/shared/game_timer.cpp) function by function; what
// differs is how a map talks to it. GoldSrc has no inputs and outputs, so:
//   * Use: ON = Enable, OFF = Disable, TOGGLE = one or the other, SET = SetTime (the value is the seconds; what
//     game_counter does with a game_counter_set).
//   * Every other input that carries a value is the keyvalue of the same setting, which may be set again on a
//     live timer (KeyValue after Spawn): "max_length" is SetMaxTime, "timer_caption" SetTimerCaption, "x" SetX, ...
//     and the keys "time_remaining", "add_time", "remove_time" and "paused", which only exist at run time, are
//     SetTime, AddTime, RemoveTime and Pause / Resume. trigger_changevalue and trigger_copyvalue set and read them
//     (trigger_changevalue.cpp; GetKeyValue answers every key).
//   * The outputs are target keys: "target" (OnFinished), "warntarget", "pausetarget", "resumetarget".
//
// Source queues an entity's outputs and delivers them after the function that fired them has returned. The
// targets here are fired the same way: once the timer's state is complete and the clients have it (FireEvents), so
// a target that triggers the timer again finds it in its final state.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"game_timer.h"

extern int gmsgGameTimer;

// Source: the timer thinks every 0.05 s while it runs
#define GAME_TIMER_THINK_INTERVAL	0.05f

// ApplyKey's results
#define KEY_UNKNOWN		0	// not a key of the timer
#define KEY_QUIET		1	// nothing the clients show
#define KEY_TIME		2	// the time changed
#define KEY_FULL		3	// a display setting changed

LINK_ENTITY_TO_CLASS( game_timer, CGameTimer );

TYPEDESCRIPTION	CGameTimer::m_SaveData[] =
{
	DEFINE_FIELD( CGameTimer, m_flTimerInitialLength, FIELD_FLOAT ),
	DEFINE_FIELD( CGameTimer, m_flTimerMaxLength, FIELD_FLOAT ),
	DEFINE_FIELD( CGameTimer, m_flWarnTime, FIELD_FLOAT ),
	DEFINE_FIELD( CGameTimer, m_bIsDisabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGameTimer, m_bStartPaused, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGameTimer, m_bShowTimeRemaining, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGameTimer, m_bDisableOnFinish, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGameTimer, m_bShowInHUD, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGameTimer, m_nProgressBarMaxSegments, FIELD_INTEGER ),
	DEFINE_FIELD( CGameTimer, m_nProgressBarOverride, FIELD_INTEGER ),
	DEFINE_FIELD( CGameTimer, m_flOverrideX, FIELD_FLOAT ),
	DEFINE_FIELD( CGameTimer, m_flOverrideY, FIELD_FLOAT ),
	DEFINE_FIELD( CGameTimer, m_iszTimerCaption, FIELD_STRING ),
	DEFINE_FIELD( CGameTimer, m_nCaptionWidth, FIELD_INTEGER ),
	DEFINE_FIELD( CGameTimer, m_nCaptionMaxLines, FIELD_INTEGER ),
	DEFINE_FIELD( CGameTimer, m_iszPlayerFilterName, FIELD_STRING ),
	DEFINE_FIELD( CGameTimer, m_iszPauseTarget, FIELD_STRING ),
	DEFINE_FIELD( CGameTimer, m_iszResumeTarget, FIELD_STRING ),
	DEFINE_FIELD( CGameTimer, m_iszWarnTarget, FIELD_STRING ),
	DEFINE_FIELD( CGameTimer, m_flTimerEndTime, FIELD_TIME ),
	DEFINE_FIELD( CGameTimer, m_flTimeRemaining, FIELD_FLOAT ),
	DEFINE_FIELD( CGameTimer, m_bTimerPaused, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGameTimer, m_bStartedWarn, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGameTimer, m_bSpawned, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CGameTimer, CPointEntity );

// Runs before the keyvalues: the defaults of a key the map doesn't set, Source's constructor (and the FGD's)
CGameTimer::CGameTimer( void )
{
	m_bIsDisabled = TRUE;
	m_bTimerPaused = TRUE;
	m_flTimeRemaining = 0;
	m_flTimerEndTime = 0;
	m_flWarnTime = -1.0f;
	m_bStartPaused = FALSE;
	m_bShowTimeRemaining = TRUE;
	m_flTimerInitialLength = 0;
	m_flTimerMaxLength = 0;
	m_nProgressBarMaxSegments = -1;
	m_nProgressBarOverride = -1;
	m_nCaptionWidth = 260;
	m_nCaptionMaxLines = 2;
	m_flOverrideX = m_flOverrideY = -1.0f;
	m_bDisableOnFinish = TRUE;
	m_bShowInHUD = TRUE;
	m_bStartedWarn = FALSE;
	m_bSpawned = FALSE;
	m_iEventCount = 0;
}

void CGameTimer::Spawn( void )
{
	CPointEntity::Spawn();

	// Source: the time is set whether the timer starts disabled or not, and a timer that starts enabled runs
	// unless it starts paused
	SetTimeRemaining( m_flTimerInitialLength );

	if ( !m_bIsDisabled && !m_bStartPaused )
		ResumeTimer( NULL );

	if ( !FStringNull( m_iszPlayerFilterName ) && STRING( m_iszPlayerFilterName )[0] )
		ALERT( at_console, "game_timer \"%s\": PlayerFilter \"%s\" is not supported, every player sees the timer\n",
			STRING( pev->targetname ), STRING( m_iszPlayerFilterName ) );

	// No message and no targets from here: the other entities of the map don't exist yet and no client is in the
	// game (they get the timer with their HUD initialisation). A timer that runs from the start fires its
	// "resumetarget" with its first think (and one without any time its "target" and "pausetarget").
	if ( m_iEventCount > 0 )
	{
		SetThink( &CGameTimer::TimerThink );
		pev->nextthink = gpGlobals->time + GAME_TIMER_THINK_INTERVAL;
	}

	m_bSpawned = TRUE;
}

//---------------------------------------------------------
// Keyvalues: the map's, and changes to a live timer
//---------------------------------------------------------

void CGameTimer::KeyValue( KeyValueData *pkvd )
{
	const BOOL wasShown = IsShown();

	const int result = ApplyKey( pkvd->szKeyName, pkvd->szValue );
	if ( result == KEY_UNKNOWN )
	{
		CPointEntity::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;

	if ( !m_bSpawned )
		return;

	if ( result != KEY_QUIET || wasShown != IsShown() )
		SyncClients( wasShown, result == KEY_FULL );

	FireEvents();
}

int CGameTimer::ApplyKey( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "timer_length" ) )
	{
		// what Enable and a restart count from; the time left doesn't change
		m_flTimerInitialLength = atof( szValue );
		return KEY_FULL;	// the length shown (bar, elapsed time, default warning) without a "max_length"
	}
	else if ( FStrEq( szKeyName, "max_length" ) )
	{
		if ( m_bSpawned )
			SetMaxTime( atof( szValue ) );
		else
			m_flTimerMaxLength = atof( szValue );
		return KEY_FULL;
	}
	else if ( FStrEq( szKeyName, "warn_time" ) )
	{
		m_flWarnTime = atof( szValue );
		return KEY_FULL;
	}
	else if ( FStrEq( szKeyName, "StartDisabled" ) )
	{
		// the map's start state only: a live timer is enabled and disabled by triggering it
		if ( !m_bSpawned )
			m_bIsDisabled = atoi( szValue ) != 0;
		return KEY_QUIET;
	}
	else if ( FStrEq( szKeyName, "start_paused" ) )
	{
		m_bStartPaused = atoi( szValue ) != 0;
		return KEY_QUIET;
	}
	else if ( FStrEq( szKeyName, "show_time_remaining" ) )
	{
		m_bShowTimeRemaining = atoi( szValue ) != 0;
		return KEY_FULL;
	}
	else if ( FStrEq( szKeyName, "disable_on_finish" ) )
	{
		m_bDisableOnFinish = atoi( szValue ) != 0;
		return KEY_QUIET;
	}
	else if ( FStrEq( szKeyName, "show_in_hud" ) )
	{
		// KeyValue sends the timer to the clients, or takes it away, when this changes what IsShown says
		m_bShowInHUD = atoi( szValue ) != 0;
		return KEY_QUIET;
	}
	else if ( FStrEq( szKeyName, "progress_bar_max" ) )
	{
		m_nProgressBarMaxSegments = atoi( szValue );
		return KEY_FULL;
	}
	else if ( FStrEq( szKeyName, "progress_bar_override" ) )
	{
		m_nProgressBarOverride = atoi( szValue );
		return KEY_FULL;
	}
	else if ( FStrEq( szKeyName, "x" ) )
	{
		m_flOverrideX = atof( szValue );
		return KEY_FULL;
	}
	else if ( FStrEq( szKeyName, "y" ) )
	{
		m_flOverrideY = atof( szValue );
		return KEY_FULL;
	}
	else if ( FStrEq( szKeyName, "timer_caption" ) )
	{
		m_iszTimerCaption = ALLOC_STRING( szValue );
		return KEY_FULL;
	}
	else if ( FStrEq( szKeyName, "caption_width" ) )
	{
		m_nCaptionWidth = max( 0, atoi( szValue ) );
		return KEY_FULL;
	}
	else if ( FStrEq( szKeyName, "caption_lines" ) )
	{
		m_nCaptionMaxLines = max( 1, min( MAX_GAME_TIMER_CAPTION_LINES, atoi( szValue ) ) );
		return KEY_FULL;
	}
	else if ( FStrEq( szKeyName, "PlayerFilter" ) )
	{
		m_iszPlayerFilterName = ALLOC_STRING( szValue );
		return KEY_QUIET;
	}
	else if ( FStrEq( szKeyName, "pausetarget" ) )
	{
		m_iszPauseTarget = ALLOC_STRING( szValue );
		return KEY_QUIET;
	}
	else if ( FStrEq( szKeyName, "resumetarget" ) )
	{
		m_iszResumeTarget = ALLOC_STRING( szValue );
		return KEY_QUIET;
	}
	else if ( FStrEq( szKeyName, "warntarget" ) )
	{
		m_iszWarnTarget = ALLOC_STRING( szValue );
		return KEY_QUIET;
	}
	// Run time only (a map's own value would be replaced by Spawn's "timer_length"): Source's SetTime, AddTime
	// and RemoveTime
	else if ( FStrEq( szKeyName, "time_remaining" ) )
	{
		if ( m_bSpawned )
			SetTimeRemaining( atof( szValue ) );
		return KEY_TIME;
	}
	else if ( FStrEq( szKeyName, "add_time" ) )
	{
		if ( m_bSpawned )
			AddTimerSeconds( atof( szValue ) );
		return KEY_TIME;
	}
	else if ( FStrEq( szKeyName, "remove_time" ) )
	{
		if ( m_bSpawned )
			AddTimerSeconds( -atof( szValue ) );
		return KEY_TIME;
	}
	// Run time only: Source's Pause (1) and Resume (0). Like the inputs, nothing on a disabled timer.
	else if ( FStrEq( szKeyName, "paused" ) )
	{
		if ( m_bSpawned )
		{
			if ( atoi( szValue ) != 0 )
				PauseTimer( NULL );
			else
				ResumeTimer( NULL );
		}
		return KEY_TIME;
	}

	return KEY_UNKNOWN;
}

CKeyValue CGameTimer::GetKeyValue( const char *keyName )
{
	static const char *s_floatKeys[] = { "time_remaining", "timer_length", "max_length", "warn_time", "x", "y",
		"add_time", "remove_time" };
	static const char *s_intKeys[] = { "paused", "StartDisabled", "start_paused", "show_time_remaining",
		"disable_on_finish", "show_in_hud", "progress_bar_max", "progress_bar_override", "caption_width",
		"caption_lines" };
	static const char *s_stringKeys[] = { "timer_caption", "PlayerFilter", "pausetarget", "resumetarget",
		"warntarget" };

	CKeyValue keyvalue = g_emptyKeyValue;

	const float floats[] = { GetTimeRemaining(), m_flTimerInitialLength, m_flTimerMaxLength, m_flWarnTime,
		m_flOverrideX, m_flOverrideY,
		0.0f, 0.0f };	// add_time / remove_time: amounts, not states (so that "add 10" gives 10)
	const int ints[] = { m_bTimerPaused ? 1 : 0, m_bIsDisabled ? 1 : 0, m_bStartPaused ? 1 : 0,
		m_bShowTimeRemaining ? 1 : 0, m_bDisableOnFinish ? 1 : 0, m_bShowInHUD ? 1 : 0, m_nProgressBarMaxSegments,
		m_nProgressBarOverride, m_nCaptionWidth, m_nCaptionMaxLines };
	const string_t strings[] = { m_iszTimerCaption, m_iszPlayerFilterName, m_iszPauseTarget, m_iszResumeTarget,
		m_iszWarnTarget };

	for ( int i = 0; i < (int)ARRAYSIZE( s_floatKeys ); i++ )
	{
		if ( FStrEq( keyName, s_floatKeys[i] ) )
		{
			keyvalue.keyType = KEY_TYPE_FLOAT;
			keyvalue.keyName = s_floatKeys[i];
			keyvalue.fVal = floats[i];
			return keyvalue;
		}
	}

	for ( int i = 0; i < (int)ARRAYSIZE( s_intKeys ); i++ )
	{
		if ( FStrEq( keyName, s_intKeys[i] ) )
		{
			keyvalue.keyType = KEY_TYPE_INT;
			keyvalue.keyName = s_intKeys[i];
			keyvalue.iVal = ints[i];
			return keyvalue;
		}
	}

	for ( int i = 0; i < (int)ARRAYSIZE( s_stringKeys ); i++ )
	{
		if ( FStrEq( keyName, s_stringKeys[i] ) )
		{
			keyvalue.keyType = KEY_TYPE_STRING;
			keyvalue.keyName = s_stringKeys[i];
			keyvalue.sVal = strings[i];
			return keyvalue;
		}
	}

	// target, targetname, custom keyvalues, ...
	return CPointEntity::GetKeyValue( keyName );
}

//---------------------------------------------------------
// Source's accessors
//---------------------------------------------------------

float CGameTimer::GetTimeRemaining( void )
{
	float flSecondsRemaining;

	if ( m_bTimerPaused )
		flSecondsRemaining = m_flTimeRemaining;
	else
		flSecondsRemaining = m_flTimerEndTime - gpGlobals->time;

	if ( flSecondsRemaining < 0 )
		flSecondsRemaining = 0.0f;

	return flSecondsRemaining;
}

float CGameTimer::GetWarnTime( void )
{
	if ( m_flWarnTime < 0 )
		return GetTimerMaxLength() * GAME_TIMER_DEFAULT_WARN_FRAC;

	return m_flWarnTime;
}

float CGameTimer::GetTimerMaxLength( void )
{
	if ( m_flTimerMaxLength )
		return m_flTimerMaxLength;

	return m_flTimerInitialLength;
}

//---------------------------------------------------------
// Triggering
//---------------------------------------------------------

void CGameTimer::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	const BOOL wasShown = IsShown();
	const BOOL wasPaused = m_bTimerPaused;
	const float oldEndTime = m_flTimerEndTime;
	const float oldRemaining = m_flTimeRemaining;

	switch ( useType )
	{
	case USE_ON:
		Enable( pActivator );
		break;

	case USE_OFF:
		Disable( pActivator );
		break;

	case USE_SET:
		// game_counter's "set": the value is the new time (Source: SetTime)
		SetTimeRemaining( value );
		break;

	default:
		if ( m_bIsDisabled )
			Enable( pActivator );
		else
			Disable( pActivator );
		break;
	}

	// e.g. ON for a timer that already runs: nothing to tell
	if ( wasShown != IsShown() || wasPaused != m_bTimerPaused || oldEndTime != m_flTimerEndTime
		|| oldRemaining != m_flTimeRemaining )
		SyncClients( wasShown, FALSE );

	FireEvents();
}

// Source: InputEnable
void CGameTimer::Enable( CBaseEntity *pActivator )
{
	// a finished timer starts over
	if ( GetTimeRemaining() == 0.0f )
		SetTimeRemaining( m_flTimerInitialLength );

	m_bIsDisabled = FALSE;
	if ( !m_bStartPaused )
		ResumeTimer( pActivator );
}

// Source: InputDisable
void CGameTimer::Disable( CBaseEntity *pActivator )
{
	PauseTimer( pActivator );
	m_bIsDisabled = TRUE;
}

// Source: SetTimeRemaining (InputSetTime, InputRestart). Works on a disabled timer too.
void CGameTimer::SetTimeRemaining( float flTime )
{
	// make sure we don't go over our max length
	if ( m_flTimerMaxLength > 0 && flTime > m_flTimerMaxLength )
		flTime = m_flTimerMaxLength;

	m_flTimeRemaining = flTime;
	m_flTimerEndTime = gpGlobals->time + m_flTimeRemaining;

	// Source compares with the "warn_time" key itself, not with GetWarnTime
	if ( m_flTimeRemaining > m_flWarnTime )
		m_bStartedWarn = FALSE;
}

// Source: AddTimerSeconds (InputAddTime, InputRemoveTime). Not on a disabled timer.
void CGameTimer::AddTimerSeconds( float flTimeToAdd )
{
	if ( m_bIsDisabled )
		return;

	if ( m_flTimerMaxLength > 0 )
	{
		// will adding this many seconds push us over our max length?
		if ( GetTimeRemaining() + flTimeToAdd > m_flTimerMaxLength )
			flTimeToAdd = m_flTimerMaxLength - GetTimeRemaining();
	}

	if ( m_bTimerPaused )
		m_flTimeRemaining += flTimeToAdd;
	else
		m_flTimerEndTime += flTimeToAdd;
}

// Source: InputSetMaxTime
void CGameTimer::SetMaxTime( float flTime )
{
	m_flTimerMaxLength = flTime;

	if ( m_flTimerMaxLength > 0 )
	{
		// make sure our current time is not above the max length
		if ( GetTimeRemaining() > m_flTimerMaxLength )
			SetTimeRemaining( m_flTimerMaxLength );
	}
}

// Source: PauseTimer
void CGameTimer::PauseTimer( CBaseEntity *pActivator )
{
	if ( m_bIsDisabled )
		return;

	if ( !m_bTimerPaused )
	{
		m_bTimerPaused = TRUE;
		m_flTimeRemaining = m_flTimerEndTime - gpGlobals->time;

		QueueEvent( GAME_TIMER_EVENT_PAUSED, pActivator );

		SetThink( NULL );
		pev->nextthink = 0;
	}
}

// Source: ResumeTimer
void CGameTimer::ResumeTimer( CBaseEntity *pActivator )
{
	if ( m_bIsDisabled )
		return;

	if ( m_bTimerPaused )
	{
		m_bTimerPaused = FALSE;
		m_flTimerEndTime = gpGlobals->time + m_flTimeRemaining;

		QueueEvent( GAME_TIMER_EVENT_RESUMED, pActivator );

		// Source thinks at once: a timer resumed without time left finishes here
		if ( !RunTimer() )
		{
			SetThink( &CGameTimer::TimerThink );
			pev->nextthink = gpGlobals->time + GAME_TIMER_THINK_INTERVAL;
		}
	}
}

// Source: TimerThink, without the rescheduling. TRUE: the timer finished (and is paused, and disabled with
// "disable_on_finish").
BOOL CGameTimer::RunTimer( void )
{
	const float flTime = GetTimeRemaining();

	if ( flTime <= 0.0f )
	{
		QueueEvent( GAME_TIMER_EVENT_FINISHED, this );
		PauseTimer( NULL );
		if ( m_bDisableOnFinish )
			m_bIsDisabled = TRUE;
		return TRUE;
	}
	else if ( flTime <= GetWarnTime() && !m_bStartedWarn )
	{
		QueueEvent( GAME_TIMER_EVENT_WARNED, this );
		m_bStartedWarn = TRUE;
	}

	return FALSE;
}

void CGameTimer::TimerThink( void )
{
	if ( !m_bIsDisabled && !m_bTimerPaused )
	{
		const BOOL wasShown = IsShown();

		if ( RunTimer() )
			SyncClients( wasShown, FALSE );
		else
			pev->nextthink = gpGlobals->time + GAME_TIMER_THINK_INTERVAL;
	}

	FireEvents();
}

//---------------------------------------------------------
// Targets (Source's outputs)
//---------------------------------------------------------

void CGameTimer::QueueEvent( int event, CBaseEntity *pActivator )
{
	if ( m_iEventCount >= GAME_TIMER_MAX_EVENTS )
		return;

	m_iEvents[ m_iEventCount ] = event;
	m_hEventActivators[ m_iEventCount ] = pActivator ? pActivator : this;
	m_iEventCount++;
}

void CGameTimer::FireEvents( void )
{
	// during Spawn: the first think fires them
	if ( !m_bSpawned )
		return;

	// A target may trigger the timer again, which queues and fires its own events: take ours out first
	int events[ GAME_TIMER_MAX_EVENTS ];
	EHANDLE activators[ GAME_TIMER_MAX_EVENTS ];
	const int count = m_iEventCount;

	for ( int i = 0; i < count; i++ )
	{
		events[i] = m_iEvents[i];
		activators[i] = m_hEventActivators[i];
	}
	m_iEventCount = 0;

	for ( int i = 0; i < count; i++ )
	{
		string_t target;

		switch ( events[i] )
		{
		case GAME_TIMER_EVENT_FINISHED:	target = pev->target; break;
		case GAME_TIMER_EVENT_PAUSED:	target = m_iszPauseTarget; break;
		case GAME_TIMER_EVENT_RESUMED:	target = m_iszResumeTarget; break;
		default:						target = m_iszWarnTarget; break;
		}

		if ( FStringNull( target ) || !STRING( target )[0] )
			continue;

		CBaseEntity *pActivator = activators[i];
		FireTargets( STRING( target ), pActivator ? pActivator : this, this, USE_TOGGLE, 0 );
	}
}

//---------------------------------------------------------
// Clients
//---------------------------------------------------------

void CGameTimer::SyncClients( BOOL wasShown, BOOL full )
{
	const BOOL isShown = IsShown();

	// a timer the clients don't know and still needn't know
	if ( !wasShown && !isShown )
		return;

	// new to the clients: everything
	SendState( NULL, full || !wasShown );
}

static int ScreenScalar( float value )
{
	// Source: exactly -1 is "the default place"; anything else is a part of the screen
	if ( value == -1.0f )
		return -32768;

	const float scaled = value * 10000.0f;
	if ( scaled <= -32767.0f )
		return -32767;
	if ( scaled >= 32767.0f )
		return 32767;
	return (int)( scaled + ( scaled < 0 ? -0.5f : 0.5f ) );
}

static int ClampShort( int value )
{
	return value < -32768 ? -32768 : ( value > 32767 ? 32767 : value );
}

static void WriteFloat( float value )
{
	union
	{
		float f;
		int i;
	} bits;

	bits.f = value;
	WRITE_LONG( bits.i );
}

// GameTimer (docs/GAME_TIMER.md):
//   short id (entity index), byte flags (GAME_TIMER_FLAG_*)
//   not ENABLED: nothing more (the client forgets the timer)
//   float time: PAUSED the seconds remaining, else the server time (gpGlobals->time) the timer ends at; the
//     client's clock follows the server's, so the countdown needs no further messages
//   FULL: float length (GetTimerMaxLength), float warn time (GetWarnTime), short progress_bar_max, short
//     progress_bar_override, short x, short y (-32768 = default, else the part of the screen * 10000), short
//     caption_width, byte caption_lines, string timer_caption
void CGameTimer::SendState( CBasePlayer *pPlayer, BOOL full )
{
	if ( !gmsgGameTimer )
		return;

	if ( pPlayer )
		MESSAGE_BEGIN( MSG_ONE, gmsgGameTimer, NULL, pPlayer->pev );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgGameTimer );

	WRITE_SHORT( entindex() );

	if ( !IsShown() )
	{
		WRITE_BYTE( 0 );
		MESSAGE_END();
		return;
	}

	int flags = GAME_TIMER_FLAG_ENABLED;
	if ( m_bTimerPaused )
		flags |= GAME_TIMER_FLAG_PAUSED;
	if ( m_bShowTimeRemaining )
		flags |= GAME_TIMER_FLAG_SHOW_REMAINING;
	if ( full )
		flags |= GAME_TIMER_FLAG_FULL;

	WRITE_BYTE( flags );
	WriteFloat( m_bTimerPaused ? GetTimeRemaining() : m_flTimerEndTime );

	if ( full )
	{
		char caption[ MAX_GAME_TIMER_CAPTION ];
		strncpy( caption, FStringNull( m_iszTimerCaption ) ? "" : STRING( m_iszTimerCaption ), sizeof( caption ) - 1 );
		caption[ sizeof( caption ) - 1 ] = '\0';

		WriteFloat( GetTimerMaxLength() );
		WriteFloat( GetWarnTime() );
		WRITE_SHORT( ClampShort( m_nProgressBarMaxSegments ) );
		WRITE_SHORT( ClampShort( m_nProgressBarOverride ) );
		WRITE_SHORT( ScreenScalar( m_flOverrideX ) );
		WRITE_SHORT( ScreenScalar( m_flOverrideY ) );
		WRITE_SHORT( ClampShort( m_nCaptionWidth ) );
		WRITE_BYTE( m_nCaptionMaxLines );
		WRITE_STRING( caption );
	}

	MESSAGE_END();
}

void GameTimer_SendAll( CBasePlayer *pPlayer )
{
	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "game_timer" ) ) != NULL )
	{
		CGameTimer *pTimer = (CGameTimer *)pEntity;
		if ( pTimer->IsShown() )
			pTimer->SendState( pPlayer, TRUE );
	}
}
