//=========================================================
// SBO 2.0 env_overlay: see env_overlay.h and docs/ENV_OVERLAY.md.
//
// One entity for hint pictures, logos, full-screen tints and flashes: a picture (.spr, animated or not, or a .tga)
// or a plain color, placed like game_text (x / y, -1 centered) or stretched over the screen, with game_text's fade
// in / hold / fade out. The design follows Next21's HudSprite message (NextClientServerApi, src/services/HudSprite.cpp:
// channel, picture, color, fades, position, scale, render mode, full screen); the recipients follow game_messagebox
// (the activator, or every player with "All Players").
//
//   * Only players in play see overlays (Logic_IsPlayerInPlay: alive, not waiting, not a spectator or Game Master).
//     A player who leaves play loses his overlays at once; one who enters play gets those that are on for every
//     player, where they are now (the message says how long they have been shown).
//   * 8 channels per player (game_text has 4): an overlay replaces the one on its channel. Turning an entity off
//     only clears a channel that still shows that entity.
//   * Timed (a hold time, no Loop): every Use starts it again; it ends by itself. Lasting (hold time 0 = until turned
//     off, or Loop): Use toggles it; On / Off as the trigger says.
//   * The server keeps what each player's channels show, so a client that gets its HUD again (fullupdate, a demo
//     started mid-map) gets them again. Bots and HLTV get nothing.
//   * The picture is precached by the entity only (a .spr as a model, a .tga as a generic file the client downloads),
//     and can't be changed at run time (it would need a precache).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"round_hooks.h"
#include	"env_overlay.h"
#include	"overlay_shared.h"

extern int gmsgOverlay;

#define OVERLAY_MAX_PLAYERS		32

typedef struct
{
	EHANDLE		hEntity;	// the env_overlay this channel shows (NULL: nothing)
	float		flStart;	// when it started for this player (server time)
	float		flEnd;		// when a timed one ends, 0 = until turned off
} overlayslot_t;

typedef struct
{
	overlayslot_t	slots[ OVERLAY_CHANNELS ];
	BOOL			bInPlay;	// in play at the last frame
} overlayplayer_t;

static overlayplayer_t s_players[ OVERLAY_MAX_PLAYERS + 1 ];
static int s_iOverlays;		// env_overlay entities on the map (Overlay_StartFrame does nothing without)

static BOOL IsValidSlot( int playerIndex )
{
	return playerIndex >= 1 && playerIndex <= OVERLAY_MAX_PLAYERS && playerIndex <= gpGlobals->maxClients;
}

// A human player in the game (bots and HLTV proxies have no screen)
static CBasePlayer *OverlayPlayer( int playerIndex )
{
	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( playerIndex );

	if ( !pPlayer || FNullEnt( pPlayer->edict() ) || FStringNull( pPlayer->pev->netname ) )
		return NULL;

	if ( !( pPlayer->pev->flags & FL_CLIENT ) || ( pPlayer->pev->flags & ( FL_FAKECLIENT | FL_PROXY ) ) )
		return NULL;

	return pPlayer;
}

static BOOL SlotActive( overlayslot_t &slot )
{
	return (CBaseEntity *)slot.hEntity != NULL && ( slot.flEnd == 0.0f || slot.flEnd > gpGlobals->time );
}

static void ResetPlayer( int playerIndex )
{
	s_players[ playerIndex ] = overlayplayer_t();
}

// unsigned 16-bit value as WRITE_SHORT writes it (the client reads it back unsigned)
static int UnsignedShort( float value, float unit )
{
	float v = value * unit + 0.5f;
	if ( !( v > 0.0f ) )
		return 0;
	if ( v > 65535.0f )
		v = 65535.0f;
	return (int)(short)(unsigned short)(int)v;
}

static int SignedShort( float value, float unit, float limit )
{
	if ( !( value == value ) )
		value = 0.0f;
	if ( value > limit )
		value = limit;
	else if ( value < -limit )
		value = -limit;
	float v = value * unit;
	return (int)( v < 0.0f ? v - 0.5f : v + 0.5f );
}

static int ColorByte( float value )
{
	if ( !( value > 0.0f ) )
		return 0;
	if ( value > 255.0f )
		return 255;
	return (int)value;
}

static void SendOff( CBasePlayer *pPlayer, int channel, BOOL bNow )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgOverlay, NULL, pPlayer->pev );
		WRITE_BYTE( channel + 1 );
		WRITE_BYTE( bNow ? OVERLAY_FLAG_NOW : 0 );
	MESSAGE_END();
}

//---------------------------------------------------------
// env_overlay
//---------------------------------------------------------
class CEnvOverlay : public CPointEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	UpdateOnRemove( void );
	void	EXPORT EndThink( void );
	// SBO 2.1: keys changed at run time (trigger_changevalue / trigger_copyvalue) reach the screens that show it
	CKeyValue GetKeyValue( const char *keyName );
	void	RuntimeKeyValueChanged( const char *keyName );
	void	ApplyDefaults( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL	ForAllPlayers( void ) { return ( pev->spawnflags & ( SF_OVERLAY_ALLPLAYERS | SF_OVERLAY_STARTON ) ) != 0; }
	// until turned off: Loop, or no hold time
	BOOL	Lasting( void ) { return ( pev->spawnflags & SF_OVERLAY_LOOP ) || m_flHoldTime <= 0.0f; }
	float	Duration( void ) { return m_flFadeIn + m_flHoldTime + m_flFadeOut; }
	int		Channel( void ) { return m_iChannel; }
	BOOL	IsOnForAll( void ) { return m_bOn; }
	float	StartTime( void ) { return m_flStartTime; }

	void	Give( CBasePlayer *pPlayer, float flElapsed );

private:
	void	TurnOnForAll( void );
	void	TurnOffForAll( void );
	void	TakeOff( CBasePlayer *pPlayer );
	void	Send( CBasePlayer *pPlayer, float flElapsed );
	BOOL	CheckImage( void );

	string_t	m_iszImage;		// "sprite": .spr or .tga, empty = a plain color
	float		m_flX;			// "x": game_text's x (-1 centered)
	float		m_flY;			// "y"
	float		m_flFadeIn;		// "fadein"
	float		m_flHoldTime;	// "holdtime": 0 = until turned off
	float		m_flFadeOut;	// "fadeout"
	int			m_iChannel;		// "channel" 1 .. 8, kept as 0 .. 7
	BOOL		m_bOn;			// "All Players": on for every player in play
	float		m_flStartTime;	// "All Players": when it was turned on
	BOOL		m_bUsable;		// a valid picture (or a full-screen color)
	BOOL		m_bSpawned;		// "sprite" can no longer change (it is precached)
};

LINK_ENTITY_TO_CLASS( env_overlay, CEnvOverlay );

TYPEDESCRIPTION CEnvOverlay::m_SaveData[] =
{
	DEFINE_FIELD( CEnvOverlay, m_iszImage, FIELD_STRING ),
	DEFINE_FIELD( CEnvOverlay, m_flX, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvOverlay, m_flY, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvOverlay, m_flFadeIn, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvOverlay, m_flHoldTime, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvOverlay, m_flFadeOut, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvOverlay, m_iChannel, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvOverlay, m_bOn, FIELD_BOOLEAN ),
	DEFINE_FIELD( CEnvOverlay, m_flStartTime, FIELD_TIME ),
	DEFINE_FIELD( CEnvOverlay, m_bUsable, FIELD_BOOLEAN ),
	DEFINE_FIELD( CEnvOverlay, m_bSpawned, FIELD_BOOLEAN ),
};

int CEnvOverlay::Save( CSave &save )
{
	if ( !CPointEntity::Save( save ) )
		return 0;
	return save.WriteFields( "CEnvOverlay", this, m_SaveData, ARRAYSIZE( m_SaveData ) );
}

int CEnvOverlay::Restore( CRestore &restore )
{
	if ( !CPointEntity::Restore( restore ) )
		return 0;
	if ( !restore.ReadFields( "CEnvOverlay", this, m_SaveData, ARRAYSIZE( m_SaveData ) ) )
		return 0;

	// a restored game loads the picture again and counts the entity (Spawn is not called)
	Precache();
	s_iOverlays++;
	return 1;
}

static float ClampTime( float f )
{
	if ( !( f > 0.0f ) )
		return 0.0f;
	return f > 655.0f ? 655.0f : f;
}

void CEnvOverlay::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "sprite" ) )
	{
		if ( m_bSpawned )
			ALERT( at_console, "env_overlay \"%s\": its picture can't be changed at run time (it would need a precache)\n",
				STRING( pev->targetname ) );
		else
			m_iszImage = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "x" ) )
	{
		m_flX = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "y" ) )
	{
		m_flY = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fadein" ) )
	{
		m_flFadeIn = ClampTime( atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "holdtime" ) )
	{
		m_flHoldTime = ClampTime( atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fadeout" ) )
	{
		m_flFadeOut = ClampTime( atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "channel" ) )
	{
		int channel = atoi( pkvd->szValue );
		if ( channel < 1 || channel > OVERLAY_CHANNELS )
			channel = 1;
		// SBO 2.1: refused at run time: the players' channels hold this overlay by its channel number
		if ( m_bSpawned )
			ALERT( at_console, "Key \"channel\" of env_overlay \"%s\" can't be changed at run time\n",
				STRING( pev->targetname ) );
		else
			m_iChannel = channel - 1;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

// a path under the game folder: .spr or .tga, short enough for the precache and the message
BOOL CEnvOverlay::CheckImage( void )
{
	if ( FStringNull( m_iszImage ) )
		return TRUE;

	const char *image = STRING( m_iszImage );
	const size_t len = strlen( image );

	if ( len < 5 || len >= OVERLAY_MAX_PATH || strstr( image, ".." ) || strchr( image, ':' ) || image[0] == '/'
		|| image[0] == '\\' )
		return FALSE;

	const char *ext = image + len - 4;
	return !stricmp( ext, ".spr" ) || !stricmp( ext, ".tga" );
}

void CEnvOverlay::Precache( void )
{
	m_bUsable = CheckImage();
	if ( !m_bUsable )
	{
		ALERT( at_error, "env_overlay \"%s\": bad picture \"%s\" (a .spr or .tga under the game folder, at most %d characters)\n",
			STRING( pev->targetname ), STRING( m_iszImage ), OVERLAY_MAX_PATH - 1 );
		return;
	}

	if ( FStringNull( m_iszImage ) )
		return;

	// the mapper's picture, precached by the entity that uses it: a sprite as a model, a .tga as a file the client
	// downloads
	const char *image = STRING( m_iszImage );
	if ( !stricmp( image + strlen( image ) - 4, ".spr" ) )
		PRECACHE_MODEL( (char *)image );
	else
		PRECACHE_GENERIC( (char *)image );
}

void CEnvOverlay::Spawn( void )
{
	Precache();
	m_bSpawned = TRUE;

	if ( m_bUsable && FStringNull( m_iszImage ) && !( pev->spawnflags & SF_OVERLAY_FULLSCREEN ) )
	{
		ALERT( at_error, "env_overlay \"%s\": no picture: a plain color needs \"Full Screen\"\n", STRING( pev->targetname ) );
		m_bUsable = FALSE;
	}

	ApplyDefaults();

	s_iOverlays++;

	CPointEntity::Spawn();

	if ( m_bUsable && ( pev->spawnflags & SF_OVERLAY_STARTON ) )
		TurnOnForAll();
}

// Spawn's defaults (SBO 2.1: also after a run-time change)
void CEnvOverlay::ApplyDefaults( void )
{
	// "scale" and "framerate" are entity variables (pev->scale, pev->framerate): the engine sets them
	if ( pev->scale <= 0.0f )
		pev->scale = 1.0f;
	if ( pev->framerate < 0.0f )
		pev->framerate = 0.0f;

	// FX Amount 0 or not set: fully visible (the FGD's range is 1 - 255)
	if ( pev->renderamt <= 0.0f )
		pev->renderamt = 255.0f;
}

// SBO 2.1: the readable class keys (trigger_changevalue's operations, trigger_copyvalue)
CKeyValue CEnvOverlay::GetKeyValue( const char *keyName )
{
	static const char *s_names[] = { "x", "y", "fadein", "holdtime", "fadeout" };
	const float values[] = { m_flX, m_flY, m_flFadeIn, m_flHoldTime, m_flFadeOut };

	CKeyValue keyvalue = g_emptyKeyValue;
	for ( int i = 0; i < (int)ARRAYSIZE( s_names ); i++ )
	{
		if ( FStrEq( keyName, s_names[i] ) )
		{
			keyvalue.keyType = KEY_TYPE_FLOAT;
			keyvalue.keyName = s_names[i];
			keyvalue.fVal = values[i];
			return keyvalue;
		}
	}
	if ( FStrEq( keyName, "channel" ) )
	{
		keyvalue.keyType = KEY_TYPE_INT;
		keyvalue.keyName = "channel";
		keyvalue.iVal = m_iChannel + 1;
		return keyvalue;
	}
	return CPointEntity::GetKeyValue( keyName );
}

// SBO 2.1: a key changed at run time: Spawn's defaults, then every player whose screen shows it gets it again where
// it is now (the message carries the time already shown, so the fades go on), with a timed end moved by a new
// fade or hold time. An "All Players" overlay that is on ends at its new time.
void CEnvOverlay::RuntimeKeyValueChanged( const char *keyName )
{
	ApplyDefaults();

	if ( m_bOn )
	{
		if ( Lasting() )
			SetThink( NULL );
		else
		{
			SetThink( &CEnvOverlay::EndThink );
			pev->nextthink = max( gpGlobals->time, m_flStartTime + Duration() );
		}
	}

	if ( !m_bUsable )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients && i <= OVERLAY_MAX_PLAYERS; i++ )
	{
		overlayslot_t &slot = s_players[i].slots[ m_iChannel ];
		CBasePlayer *pPlayer = OverlayPlayer( i );
		if ( pPlayer && (CBaseEntity *)slot.hEntity == this && SlotActive( slot ) )
			Give( pPlayer, gpGlobals->time - slot.flStart );
	}
}

void CEnvOverlay::UpdateOnRemove( void )
{
	// killtarget: off on every screen that shows it
	for ( int i = 1; i <= gpGlobals->maxClients && i <= OVERLAY_MAX_PLAYERS; i++ )
	{
		overlayslot_t &slot = s_players[i].slots[ m_iChannel ];
		if ( (CBaseEntity *)slot.hEntity == this )
		{
			CBasePlayer *pPlayer = OverlayPlayer( i );
			if ( pPlayer && SlotActive( slot ) )
				SendOff( pPlayer, m_iChannel, FALSE );
			slot = overlayslot_t();
		}
	}

	if ( s_iOverlays > 0 )
		s_iOverlays--;

	CPointEntity::UpdateOnRemove();
}

void CEnvOverlay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !m_bUsable )
		return;

	if ( ForAllPlayers() )
	{
		BOOL bOn;
		if ( useType == USE_ON )
			bOn = TRUE;
		else if ( useType == USE_OFF )
			bOn = FALSE;
		else
			bOn = Lasting() ? !m_bOn : TRUE;	// a timed one starts again

		if ( bOn )
			TurnOnForAll();
		else
			TurnOffForAll();
		return;
	}

	// the activator, if it is a player in play (Featureful's EffectivePlayer in multiplayer)
	if ( !Logic_IsPlayerInPlay( pActivator ) || !OverlayPlayer( pActivator->entindex() ) )
	{
		ALERT( at_console, "env_overlay \"%s\": the activator is not a player in play\n", STRING( pev->targetname ) );
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;
	overlayslot_t &slot = s_players[ pPlayer->entindex() ].slots[ m_iChannel ];
	const BOOL bShowing = (CBaseEntity *)slot.hEntity == this && SlotActive( slot );

	BOOL bOn;
	if ( useType == USE_ON )
		bOn = TRUE;
	else if ( useType == USE_OFF )
		bOn = FALSE;
	else
		bOn = Lasting() ? !bShowing : TRUE;

	if ( bOn )
		Give( pPlayer, 0.0f );
	else if ( bShowing )
		TakeOff( pPlayer );
}

void CEnvOverlay::TurnOnForAll( void )
{
	m_bOn = TRUE;
	m_flStartTime = gpGlobals->time;

	if ( Lasting() )
		SetThink( NULL );
	else
	{
		SetThink( &CEnvOverlay::EndThink );
		pev->nextthink = gpGlobals->time + Duration();
	}

	for ( int i = 1; i <= gpGlobals->maxClients && i <= OVERLAY_MAX_PLAYERS; i++ )
	{
		CBasePlayer *pPlayer = OverlayPlayer( i );
		if ( pPlayer && Logic_IsPlayerInPlay( pPlayer ) )
			Give( pPlayer, 0.0f );
	}
}

void CEnvOverlay::TurnOffForAll( void )
{
	m_bOn = FALSE;
	SetThink( NULL );

	for ( int i = 1; i <= gpGlobals->maxClients && i <= OVERLAY_MAX_PLAYERS; i++ )
	{
		CBasePlayer *pPlayer = OverlayPlayer( i );
		if ( pPlayer )
			TakeOff( pPlayer );
	}
}

// a timed "All Players" overlay has ended: players who enter play from now on don't get it
void CEnvOverlay::EndThink( void )
{
	m_bOn = FALSE;
	SetThink( NULL );
}

void CEnvOverlay::Give( CBasePlayer *pPlayer, float flElapsed )
{
	const int playerIndex = pPlayer->entindex();
	if ( !IsValidSlot( playerIndex ) )
		return;

	overlayslot_t &slot = s_players[ playerIndex ].slots[ m_iChannel ];
	slot.hEntity = this;
	slot.flStart = gpGlobals->time - flElapsed;
	slot.flEnd = Lasting() ? 0.0f : slot.flStart + Duration();

	// a timed overlay without any time: nothing to show
	if ( slot.flEnd != 0.0f && slot.flEnd <= gpGlobals->time )
	{
		slot = overlayslot_t();
		return;
	}

	Send( pPlayer, flElapsed );
}

// off on this player's screen, if his channel still shows this entity (fades out)
void CEnvOverlay::TakeOff( CBasePlayer *pPlayer )
{
	const int playerIndex = pPlayer->entindex();
	if ( !IsValidSlot( playerIndex ) )
		return;

	overlayslot_t &slot = s_players[ playerIndex ].slots[ m_iChannel ];
	if ( (CBaseEntity *)slot.hEntity != this )
		return;

	if ( SlotActive( slot ) )
		SendOff( pPlayer, m_iChannel, FALSE );
	slot = overlayslot_t();
}

void CEnvOverlay::Send( CBasePlayer *pPlayer, float flElapsed )
{
	int flags = OVERLAY_FLAG_ON;
	if ( pev->spawnflags & SF_OVERLAY_FULLSCREEN )
		flags |= OVERLAY_FLAG_FULLSCREEN;
	if ( pev->spawnflags & SF_OVERLAY_LOOP )
		flags |= OVERLAY_FLAG_LOOP;
	else if ( m_flHoldTime <= 0.0f )
		flags |= OVERLAY_FLAG_STAY;
	// Glow and Additive add light; the others are blended by the picture's own transparency
	if ( pev->rendermode == kRenderTransAdd || pev->rendermode == kRenderGlow )
		flags |= OVERLAY_FLAG_ADDITIVE;

	// x / y: -1 centers exactly (any other value is a position)
	const int x = ( m_flX == -1.0f ) ? -(int)OVERLAY_POS_UNIT : SignedShort( m_flX, OVERLAY_POS_UNIT, 1.0f );
	const int y = ( m_flY == -1.0f ) ? -(int)OVERLAY_POS_UNIT : SignedShort( m_flY, OVERLAY_POS_UNIT, 1.0f );

	int scale = SignedShort( pev->scale, OVERLAY_SCALE_UNIT, 327.0f );
	if ( scale < 1 )
		scale = 1;

	MESSAGE_BEGIN( MSG_ONE, gmsgOverlay, NULL, pPlayer->pev );
		WRITE_BYTE( m_iChannel + 1 );
		WRITE_BYTE( flags );
		WRITE_STRING( FStringNull( m_iszImage ) ? "" : STRING( m_iszImage ) );
		WRITE_SHORT( x );
		WRITE_SHORT( y );
		WRITE_SHORT( scale );
		WRITE_BYTE( ColorByte( pev->rendercolor.x ) );
		WRITE_BYTE( ColorByte( pev->rendercolor.y ) );
		WRITE_BYTE( ColorByte( pev->rendercolor.z ) );
		WRITE_BYTE( ColorByte( pev->renderamt ) );
		WRITE_SHORT( UnsignedShort( pev->framerate, OVERLAY_TIME_UNIT ) );
		WRITE_SHORT( UnsignedShort( m_flFadeIn, OVERLAY_TIME_UNIT ) );
		WRITE_SHORT( UnsignedShort( m_flHoldTime, OVERLAY_TIME_UNIT ) );
		WRITE_SHORT( UnsignedShort( m_flFadeOut, OVERLAY_TIME_UNIT ) );
		WRITE_SHORT( UnsignedShort( flElapsed, OVERLAY_ELAPSED_UNIT ) );
	MESSAGE_END();
}

//---------------------------------------------------------
// The players' overlays
//---------------------------------------------------------

// every channel this player's client shows off at once (he left play)
static void DropAll( CBasePlayer *pPlayer, overlayplayer_t &state )
{
	for ( int ch = 0; ch < OVERLAY_CHANNELS; ch++ )
	{
		overlayslot_t &slot = state.slots[ch];
		if ( SlotActive( slot ) )
			SendOff( pPlayer, ch, TRUE );
		slot = overlayslot_t();
	}
}

// the overlays that are on for every player, as they are now (the one turned on last of each channel)
static void GiveAllPlayersOverlays( CBasePlayer *pPlayer )
{
	CEnvOverlay *pBest[ OVERLAY_CHANNELS ] = {};

	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "env_overlay" ) ) != NULL )
	{
		CEnvOverlay *pOverlay = (CEnvOverlay *)pEntity;
		if ( !pOverlay->IsOnForAll() )
			continue;

		CEnvOverlay *&pSlot = pBest[ pOverlay->Channel() ];
		if ( !pSlot || pOverlay->StartTime() >= pSlot->StartTime() )
			pSlot = pOverlay;
	}

	for ( int ch = 0; ch < OVERLAY_CHANNELS; ch++ )
	{
		if ( pBest[ch] )
			pBest[ch]->Give( pPlayer, gpGlobals->time - pBest[ch]->StartTime() );
	}
}

void Overlay_SendAll( CBasePlayer *pPlayer )
{
	const int playerIndex = pPlayer->entindex();
	if ( !IsValidSlot( playerIndex ) || !OverlayPlayer( playerIndex ) )
		return;

	overlayplayer_t &state = s_players[ playerIndex ];
	for ( int ch = 0; ch < OVERLAY_CHANNELS; ch++ )
	{
		overlayslot_t &slot = state.slots[ch];
		if ( !SlotActive( slot ) )
		{
			slot = overlayslot_t();
			continue;
		}

		CEnvOverlay *pOverlay = (CEnvOverlay *)(CBaseEntity *)slot.hEntity;
		pOverlay->Give( pPlayer, gpGlobals->time - slot.flStart );
	}
}

void Overlay_StartFrame( void )
{
	if ( s_iOverlays <= 0 )
		return;

	for ( int playerIndex = 1; playerIndex <= OVERLAY_MAX_PLAYERS && playerIndex <= gpGlobals->maxClients; playerIndex++ )
	{
		overlayplayer_t &state = s_players[ playerIndex ];

		CBasePlayer *pPlayer = OverlayPlayer( playerIndex );
		if ( !pPlayer )
		{
			ResetPlayer( playerIndex );
			continue;
		}

		if ( !Logic_IsPlayerInPlay( pPlayer ) )
		{
			// dead, waiting, spectating or a Game Master: nothing on his screen
			if ( state.bInPlay )
				DropAll( pPlayer, state );
			state.bInPlay = FALSE;
			continue;
		}

		// timed overlays that have ended (the client ends them by itself)
		for ( int ch = 0; ch < OVERLAY_CHANNELS; ch++ )
		{
			if ( (CBaseEntity *)state.slots[ch].hEntity != NULL && !SlotActive( state.slots[ch] ) )
				state.slots[ch] = overlayslot_t();
		}

		if ( !state.bInPlay )
		{
			state.bInPlay = TRUE;
			GiveAllPlayersOverlays( pPlayer );
		}
	}
}

void Overlay_ClientPutInServer( CBasePlayer *pPlayer )
{
	const int playerIndex = pPlayer->entindex();
	if ( IsValidSlot( playerIndex ) )
		ResetPlayer( playerIndex );	// a new connection or a new map: the client shows nothing
}

void Overlay_ClientDisconnected( edict_t *pClient )
{
	const int playerIndex = ENTINDEX( pClient );
	if ( IsValidSlot( playerIndex ) )
		ResetPlayer( playerIndex );
}

void Overlay_ServerDeactivate( void )
{
	for ( int i = 1; i <= OVERLAY_MAX_PLAYERS; i++ )
		ResetPlayer( i );
	s_iOverlays = 0;
}
