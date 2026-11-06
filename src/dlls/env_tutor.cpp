//=========================================================
// env_tutor: a CS:CZ tutor window message, triggered by name. Keyvalues and spawnflags: sdk/syndicate.fgd,
// docs/TUTOR_PORT.md ("Mapper guide"). Follows game_text (maprules.cpp): "message", "x"/"y", "color", "holdtime",
// "master", activator or all players; the queue and the window are the tutor's (tutor.cpp).
//
// Use: ON / TOGGLE (and SET) shows the message (to the activator or every player, per spawnflags); OFF closes it
// for those it is shown to and takes it out of their queues.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"tutor.h"

LINK_ENTITY_TO_CLASS( env_tutor, CEnvTutor );

TYPEDESCRIPTION	CEnvTutor::m_SaveData[] =
{
	DEFINE_ARRAY( CEnvTutor, m_parms, FIELD_CHARACTER, sizeof(tutorparms_t) ),
	DEFINE_FIELD( CEnvTutor, m_iszIconFile, FIELD_STRING ),
	DEFINE_FIELD( CEnvTutor, m_iszSoundFile, FIELD_STRING ),
	DEFINE_FIELD( CEnvTutor, m_iszCloseTarget, FIELD_STRING ),
	DEFINE_FIELD( CEnvTutor, m_iszMaster, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CEnvTutor, CPointEntity );

// The client's sounds (CZ: CBaseTutor::DisplayMessageToPlayer), "sound" 2..4
static const char *s_TutorSounds[] =
{
	"events/tutor_msg.wav",
	"events/friend_died.wav",
	"events/enemy_died.wav",
};

// Runs before the keyvalues (the entity is created, then its keyvalues dispatched, then it spawns): the defaults
// of a key the map doesn't set, the same as the FGD's
CEnvTutor::CEnvTutor( void )
{
	m_parms.type = TUTORMESSAGETYPE_DEFAULT;
	m_parms.icon = -1;
	m_parms.sound = 0;
	m_parms.align = TUTOR_ALIGN_TOP_RIGHT;
	m_parms.x = -1;
	m_parms.y = -1;
	m_parms.holdtime = 6;
	m_parms.lifetime = 0;
	m_parms.priority = 0;
	m_parms.interrupt = 0;
	m_parms.maxshows = 0;
	m_parms.colorFlags = 0;
}

// "r g b" or "r g b a" (alpha 255 if missing)
static void ParseColor( const char *value, byte *color )
{
	int c[4] = { 255, 255, 255, 255 };
	sscanf( value, "%d %d %d %d", &c[0], &c[1], &c[2], &c[3] );

	for ( int i = 0; i < 4; i++ )
		color[i] = (byte)( c[i] < 0 ? 0 : ( c[i] > 255 ? 255 : c[i] ) );
}

void CEnvTutor::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "type" ) )
	{
		m_parms.type = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "icon" ) )
	{
		// 0 = the type's, 1 = none, 2.. = TutorIcon 1..
		m_parms.icon = atoi( pkvd->szValue ) - 1;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "iconfile" ) )
	{
		m_iszIconFile = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sound" ) )
	{
		m_parms.sound = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "soundfile" ) )
	{
		m_iszSoundFile = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "align" ) )
	{
		m_parms.align = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "x" ) )
	{
		m_parms.x = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "y" ) )
	{
		m_parms.y = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "holdtime" ) )
	{
		m_parms.holdtime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "lifetime" ) )
	{
		m_parms.lifetime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "priority" ) )
	{
		m_parms.priority = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "interrupt" ) )
	{
		m_parms.interrupt = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "maxshows" ) )
	{
		m_parms.maxshows = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "color" ) || FStrEq( pkvd->szKeyName, "iconcolor" )
		|| FStrEq( pkvd->szKeyName, "bgcolor" ) || FStrEq( pkvd->szKeyName, "bordercolor" ) )
	{
		int flag;
		byte *color;

		if ( FStrEq( pkvd->szKeyName, "color" ) )
		{
			flag = TUTOR_FLAG_TEXT_COLOR;
			color = m_parms.textColor;
		}
		else if ( FStrEq( pkvd->szKeyName, "iconcolor" ) )
		{
			flag = TUTOR_FLAG_ICON_COLOR;
			color = m_parms.iconColor;
		}
		else if ( FStrEq( pkvd->szKeyName, "bgcolor" ) )
		{
			flag = TUTOR_FLAG_BG_COLOR;
			color = m_parms.bgColor;
		}
		else
		{
			flag = TUTOR_FLAG_BORDER_COLOR;
			color = m_parms.borderColor;
		}

		// empty: the type's / the scheme's colour
		if ( pkvd->szValue[0] )
		{
			ParseColor( pkvd->szValue, color );
			m_parms.colorFlags |= flag;
		}
		else
			m_parms.colorFlags &= ~flag;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "closetarget" ) )
	{
		m_iszCloseTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "master" ) )
	{
		m_iszMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CEnvTutor::Spawn( void )
{
	Precache();

	if ( m_parms.align < 0 || m_parms.align >= TUTOR_ALIGN_COUNT )
		m_parms.align = TUTOR_ALIGN_TOP_RIGHT;
	if ( m_parms.type <= 0 )
		m_parms.type = TUTORMESSAGETYPE_DEFAULT;
	if ( m_parms.priority < 0 )
		m_parms.priority = 0;

	memset( m_timesShown, 0, sizeof( m_timesShown ) );

	CPointEntity::Spawn();
}

void CEnvTutor::Precache( void )
{
	// a map's files: precached, so clients download them (the preset icons and sounds are SBO's)
	if ( !FStringNull( m_iszSoundFile ) )
		PRECACHE_SOUND( (char *)STRING( m_iszSoundFile ) );

	if ( !FStringNull( m_iszIconFile ) )
		PRECACHE_GENERIC( (char *)STRING( m_iszIconFile ) );
}

void CEnvTutor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !FStringNull( m_iszMaster ) && !UTIL_IsMasterTriggered( m_iszMaster, pActivator ) )
		return;

	const BOOL close = ( useType == USE_OFF );

	if ( pev->spawnflags & SF_TUTOR_ACTIVATOR_ONLY )
	{
		if ( pActivator && pActivator->IsPlayer() )
		{
			if ( close )
				Tutor_Close( (CBasePlayer *)pActivator, this );
			else
				Tutor_Show( (CBasePlayer *)pActivator, this );
		}
		return;
	}

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;

		if ( close )
			Tutor_Close( pPlayer, this );
		else
			Tutor_Show( pPlayer, this );
	}
}

BOOL CEnvTutor::CanShowTo( CBasePlayer *pPlayer )
{
	// SBO 2.0: only for a player who is alive and in play now (the scoreboard's notion, SboScoreboard_InPlay): not
	// for the dead, players waiting for the next round, players still in the join / loadout menus (the spawn at
	// connect time: "Show on player spawn" comes with the spawn that puts the player in play) or observers of any
	// kind. A plain rule, no cvar; SF_TUTOR_ALIVE_ONLY, which asked for this per entity, is always on now.
	if ( !SboScoreboard_InPlay( pPlayer ) )
		return FALSE;

	int playerIndex = pPlayer->entindex();
	if ( playerIndex < 1 || playerIndex > TUTOR_MAX_PLAYERS )
		return FALSE;

	if ( !( pev->spawnflags & SF_TUTOR_REPEATABLE ) )
		return m_timesShown[ playerIndex ] < 1;

	return m_parms.maxshows <= 0 || m_timesShown[ playerIndex ] < m_parms.maxshows;
}

void CEnvTutor::RegisterShown( int playerIndex )
{
	if ( playerIndex >= 1 && playerIndex <= TUTOR_MAX_PLAYERS )
		m_timesShown[ playerIndex ]++;
}

void CEnvTutor::ForgetPlayer( int playerIndex )
{
	if ( playerIndex >= 1 && playerIndex <= TUTOR_MAX_PLAYERS )
		m_timesShown[ playerIndex ] = 0;
}

int CEnvTutor::IconId( void )
{
	if ( !FStringNull( m_iszIconFile ) && STRING( m_iszIconFile )[0] )
		return TUTOR_ICON_CUSTOM;

	if ( m_parms.icon >= TUTOR_ICON_NONE && m_parms.icon <= TUTOR_ICON_EXCLAMATION_BIGGER )
		return m_parms.icon;

	// CZ: skulls for the death types, else the "i"
	if ( m_parms.type == TUTORMESSAGETYPE_FRIEND_DEATH || m_parms.type == TUTORMESSAGETYPE_ENEMY_DEATH )
		return TUTOR_ICON_SKULLS;

	return TUTOR_ICON_I;
}

const char *CEnvTutor::IconPath( void )
{
	if ( FStringNull( m_iszIconFile ) )
		return "";

	// without the extension (the VGUI2 surface adds ".tga")
	static char path[128];
	strncpy( path, STRING( m_iszIconFile ), sizeof( path ) - 1 );
	path[ sizeof( path ) - 1 ] = '\0';

	int len = (int)strlen( path );
	if ( len > 4 && !stricmp( path + len - 4, ".tga" ) )
		path[ len - 4 ] = '\0';

	return path;
}

const char *CEnvTutor::SoundPath( void )
{
	if ( !FStringNull( m_iszSoundFile ) && STRING( m_iszSoundFile )[0] )
		return STRING( m_iszSoundFile );

	switch ( m_parms.sound )
	{
	case 1:
		return "";
	case 2:
	case 3:
	case 4:
		return s_TutorSounds[ m_parms.sound - 2 ];
	default:
		// CZ: the death types have their own sounds
		if ( m_parms.type == TUTORMESSAGETYPE_FRIEND_DEATH )
			return s_TutorSounds[1];
		if ( m_parms.type == TUTORMESSAGETYPE_ENEMY_DEATH )
			return s_TutorSounds[2];
		return s_TutorSounds[0];
	}
}
