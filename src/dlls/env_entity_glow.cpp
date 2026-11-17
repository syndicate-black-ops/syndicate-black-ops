//=========================================================
// env_entity_glow: an outline or a halo around map entities (Source SDK 2013 env_entity_glow / TF2's tf_glow:
// game/server/env_entity_glow.cpp, game/client/glow_outline_effect.cpp). Keyvalues: sdk/syndicate.fgd; developer
// notes: docs/ENV_ENTITY_GLOW.md.
//
// Keys as in Source: "target" (the entities to glow), "GlowColor" (R G B A, 0-255), "Mode" (0 always, 1 only where
// the target is hidden, 2 only where it is seen), "Style" (0 outline, 1 halo), "StartDisabled" (0 / 1). Use switches
// it (USE_ON / USE_OFF / toggle); trigger_changevalue changes GlowColor, Mode, Style and target at run time
// (RuntimeKeyValueChanged).
//
// SBO additions:
//   - flag 1 "Only the activator sees it": the player who turns it on is the only one who gets it (a reward of a
//     server plugin, a mapper's per-player hint); turned on by something that is not a player, it stays off
//   - "duration": seconds it stays on after it turns on (0: until turned off); an entity without a name (made by a
//     plugin) removes itself at the end
//   - target "!all": every alive monster and every other player in play (never spectators, never the player who sees
//     it), nearest first; "PlayerColor" gives the players a color of their own
//
// Differences from Source:
//   - every entity with the target name glows (Source took the first and warned), at most ENTGLOW_MAX_TARGETS (64
//     for !all); the names are looked up again every half second while it is on (a monster that dies and is removed
//     drops out, a monstermaker child with the name comes in)
//   - the state goes to the clients in the "EntGlow" user message: to every player whenever his HUD is
//     (re)initialised (connect, map change, fullupdate, start of a demo) and to everybody when it changes (only to
//     its player with flag 1); Source networked the entity itself (FL_EDICT_ALWAYS)
//   - Mode 0 and 1 draw through walls: their targets go to every client whatever its PVS (env_model's "Always send",
//     g_bAlwaysSend), with flag 1 only to its player (g_iGlowViewers, read in AddToFullPack), so a target on another
//     floor glows too. Mode 2 targets follow the PVS as usual.
//
// The client draws the glow (cl_dll/entity_glow.cpp, entity_glow_gl.cpp); the software renderer draws none.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"round_hooks.h"	// Logic_IsPlayerInPlay
#include	"com_model.h"	// MAX_EDICTS

#define ENTGLOW_MAX_TARGETS		32		// targets of one env_entity_glow with a name as its target
#define ENTGLOW_MAX_ALL			64		// targets of "!all" (nearest first; the message carries their indexes)
#define ENTGLOW_RESOLVE_TIME	0.5		// seconds between two looks for the targets while on
#define ENTGLOW_PLAYER_ID		2048	// added to the entity index: the client's id of the "!all" players' glow

#define SF_ENTGLOW_ACTIVATOR_ONLY	1	// only the player who turns it on sees it

// "EntGlow" message flags (client: cl_dll/entity_glow.cpp)
#define ENTGLOW_FLAG_ON			0x01

// Mode (Source's values)
#define ENTGLOW_MODE_ALWAYS		0
#define ENTGLOW_MODE_OCCLUDED	1
#define ENTGLOW_MODE_VISIBLE	2

extern int gmsgEntGlow;
extern BOOL g_bAlwaysSend[ MAX_EDICTS ];	// env_model.cpp

// "Always send" held by glows that every player sees: how many glows that draw through walls name the entity, and
// whether a glow set the entity's g_bAlwaysSend (an env_model or env_dlight that set it first keeps its own)
static unsigned char s_iGlowHolds[ MAX_EDICTS ];
static BOOL s_bGlowSetAlwaysSend[ MAX_EDICTS ];

// glows that one player sees: the entity goes to these players whatever their PVS (bit = player index - 1; client.cpp
// AddToFullPack), with a count per entity and player
unsigned int g_iGlowViewers[ MAX_EDICTS ];
static unsigned char s_iViewerHolds[ MAX_EDICTS ][ 32 ];

// also info_waypoint's Follow Entity (info_waypoint.cpp; player.h)
void EntityGlow_Hold( int index, int viewer )
{
	if ( index <= 0 || index >= MAX_EDICTS )
		return;
	if ( viewer )
	{
		if ( viewer < 1 || viewer > 32 )
			return;
		if ( s_iViewerHolds[ index ][ viewer - 1 ] < 255 )
			s_iViewerHolds[ index ][ viewer - 1 ]++;
		g_iGlowViewers[ index ] |= 1u << ( viewer - 1 );
		return;
	}
	if ( s_iGlowHolds[ index ] < 255 )
		s_iGlowHolds[ index ]++;
	if ( !g_bAlwaysSend[ index ] )
	{
		g_bAlwaysSend[ index ] = TRUE;
		s_bGlowSetAlwaysSend[ index ] = TRUE;
	}
}

void EntityGlow_Release( int index, int viewer )
{
	if ( index <= 0 || index >= MAX_EDICTS )
		return;
	if ( viewer )
	{
		if ( viewer < 1 || viewer > 32 || !s_iViewerHolds[ index ][ viewer - 1 ] )
			return;
		if ( !--s_iViewerHolds[ index ][ viewer - 1 ] )
			g_iGlowViewers[ index ] &= ~( 1u << ( viewer - 1 ) );
		return;
	}
	if ( !s_iGlowHolds[ index ] || --s_iGlowHolds[ index ] )
		return;
	if ( s_bGlowSetAlwaysSend[ index ] )
	{
		edict_t *pEdict = INDEXENT( index );
		// an env_model with "Always send" keeps its own
		if ( !pEdict || pEdict->free || !FClassnameIs( pEdict, "env_model" ) )
			g_bAlwaysSend[ index ] = FALSE;
		s_bGlowSetAlwaysSend[ index ] = FALSE;
	}
}

// a new map (ServerDeactivate, next to g_bAlwaysSend's reset)
void EntityGlow_ServerDeactivate( void )
{
	memset( s_iGlowHolds, 0, sizeof( s_iGlowHolds ) );
	memset( s_bGlowSetAlwaysSend, 0, sizeof( s_bGlowSetAlwaysSend ) );
	memset( g_iGlowViewers, 0, sizeof( g_iGlowViewers ) );
	memset( s_iViewerHolds, 0, sizeof( s_iViewerHolds ) );
}

class CEnvEntityGlow : public CPointEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ResolveThink( void );
	void RuntimeKeyValueChanged( const char *keyName );
	void UpdateOnRemove( void );
	// Spirit of Half-Life: On while it glows
	STATE GetState( CBaseEntity *pActivator ) { return m_fOn ? STATE_ON : STATE_OFF; }

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	void SendState( CBasePlayer *pPlayer );	// NULL: everybody (or its player with flag 1)
	BOOL ResolveTargets( void );			// TRUE: the list changed
	void UpdateHolds( void );
	void ApplyLimits( void );
	void TurnOff( void );
	BOOL DrawsHidden( void ) { return m_iMode != ENTGLOW_MODE_VISIBLE; }
	BOOL ActivatorOnly( void ) { return FBitSet( pev->spawnflags, SF_ENTGLOW_ACTIVATOR_ONLY ) != 0; }
	BOOL TargetsAll( void ) { return !FStringNull( pev->target ) && !stricmp( STRING( pev->target ), "!all" ); }
	CBasePlayer *Viewer( void );			// flag 1: the player who sees it, while connected
	void WriteGlow( int id, const int *color, int count, const int *targets );

	BOOL m_fOn;
	int m_iMode;
	int m_iStyle;
	int m_iColor[4];				// r g b a, 0-255
	int m_iPlayerColor[4];			// "!all": the players' color (alpha 0: GlowColor for players too)
	float m_flDuration;
	float m_flOffTime;				// on with a duration: when it turns off (0: never)
	EHANDLE m_hViewer;				// flag 1: the player who turned it on

	// keys read before Spawn (not saved: Spawn does not run after a restore)
	BOOL m_fColorKey;
	BOOL m_fStartDisabled;
	BOOL m_fWarned;

	// not saved: looked up again after a restore (indexes of the targets, and the ones this glow holds)
	BOOL m_fResolved;
	int m_iTargets;
	int m_iTarget[ ENTGLOW_MAX_ALL ];
	int m_iPlayers;					// "!all" with PlayerColor: the players, sent as a second glow
	int m_iPlayer[ ENTGLOW_MAX_ALL ];
	int m_iHeld;
	int m_iHold[ ENTGLOW_MAX_ALL * 2 ];
	int m_iHeldViewer;				// the player the holds are for (0: everybody)
};

LINK_ENTITY_TO_CLASS( env_entity_glow, CEnvEntityGlow );

TYPEDESCRIPTION CEnvEntityGlow::m_SaveData[] =
{
	DEFINE_FIELD( CEnvEntityGlow, m_fOn, FIELD_BOOLEAN ),
	DEFINE_FIELD( CEnvEntityGlow, m_iMode, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvEntityGlow, m_iStyle, FIELD_INTEGER ),
	DEFINE_ARRAY( CEnvEntityGlow, m_iColor, FIELD_INTEGER, 4 ),
	DEFINE_ARRAY( CEnvEntityGlow, m_iPlayerColor, FIELD_INTEGER, 4 ),
	DEFINE_FIELD( CEnvEntityGlow, m_flDuration, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvEntityGlow, m_flOffTime, FIELD_TIME ),
	DEFINE_FIELD( CEnvEntityGlow, m_hViewer, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CEnvEntityGlow, CPointEntity );

static int EntityGlow_ClampByte( int i )
{
	return i < 0 ? 0 : ( i > 255 ? 255 : i );
}

// "R G B A" (Source's color255 with alpha); a value without the alpha is fully strong
static void EntityGlow_ParseColor( const char *value, int *out )
{
	int c[4] = { 255, 255, 255, 255 };
	sscanf( value, "%d %d %d %d", &c[0], &c[1], &c[2], &c[3] );
	for ( int i = 0; i < 4; i++ )
		out[i] = EntityGlow_ClampByte( c[i] );
}

void CEnvEntityGlow::KeyValue( KeyValueData *pkvd )
{
	// Source's key names; matched without case (a .map or a trigger_changevalue may write them in lower case)
	if ( !stricmp( pkvd->szKeyName, "GlowColor" ) )
	{
		EntityGlow_ParseColor( pkvd->szValue, m_iColor );
		m_fColorKey = TRUE;
		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "PlayerColor" ) )
	{
		if ( pkvd->szValue[0] )
			EntityGlow_ParseColor( pkvd->szValue, m_iPlayerColor );
		else
			memset( m_iPlayerColor, 0, sizeof( m_iPlayerColor ) );
		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "Mode" ) )
	{
		m_iMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "Style" ) )
	{
		m_iStyle = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "StartDisabled" ) )
	{
		m_fStartDisabled = atoi( pkvd->szValue ) ? TRUE : FALSE;
		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "duration" ) )
	{
		m_flDuration = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

// Source's values only: anything else is the default (Mode 0 Always, Style 0 Outline)
void CEnvEntityGlow::ApplyLimits( void )
{
	if ( m_iMode < ENTGLOW_MODE_ALWAYS || m_iMode > ENTGLOW_MODE_VISIBLE )
		m_iMode = ENTGLOW_MODE_ALWAYS;
	if ( m_iStyle < 0 || m_iStyle > 1 )
		m_iStyle = 0;
	if ( !( m_flDuration >= 0.0f ) )
		m_flDuration = 0.0f;
}

void CEnvEntityGlow::Spawn( void )
{
	// the keys come before Spawn: without a GlowColor key the glow is white (Source's default "255 255 255 255");
	// without StartDisabled it is on; with flag 1 it waits for a player to turn it on
	CPointEntity::Spawn();
	if ( !m_fColorKey )
	{
		for ( int i = 0; i < 4; i++ )
			m_iColor[i] = 255;
	}
	m_fOn = !m_fStartDisabled && !ActivatorOnly();
	m_flOffTime = ( m_fOn && m_flDuration > 0.0f ) ? gpGlobals->time + m_flDuration : 0.0f;
	ApplyLimits();

	m_fResolved = FALSE;
	m_iTargets = m_iPlayers = 0;
	m_iHeld = 0;
	m_iHeldViewer = 0;

	// the targets spawn after or before this entity: looked up once everything has spawned
	SetThink( &CEnvEntityGlow::ResolveThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

CBasePlayer *CEnvEntityGlow::Viewer( void )
{
	CBaseEntity *pViewer = m_hViewer;
	if ( !pViewer || !pViewer->IsPlayer() || ( (CBasePlayer *)pViewer )->has_disconnected )
		return NULL;
	return (CBasePlayer *)pViewer;
}

struct entglow_candidate_t
{
	int index;
	float dist;
	BOOL player;
};

static int EntityGlow_CompareCandidates( const void *a, const void *b )
{
	const float d = ( (const entglow_candidate_t *)a )->dist - ( (const entglow_candidate_t *)b )->dist;
	return d < 0 ? -1 : ( d > 0 ? 1 : 0 );
}

BOOL CEnvEntityGlow::ResolveTargets( void )
{
	int iTarget[ ENTGLOW_MAX_ALL ], iPlayer[ ENTGLOW_MAX_ALL ];
	int iCount = 0, iPlayers = 0;

	if ( TargetsAll() )
	{
		// every alive monster and every other player in play, nearest to the player who sees it first (the glow's
		// own position without one)
		static entglow_candidate_t s_Candidates[ MAX_EDICTS ];
		int iCandidates = 0;
		CBasePlayer *pViewer = Viewer();
		const Vector vecFrom = pViewer ? pViewer->pev->origin : pev->origin;
		const BOOL fPlayerColor = m_iPlayerColor[3] > 0;

		for ( int i = 1; i < gpGlobals->maxEntities && iCandidates < MAX_EDICTS; i++ )
		{
			edict_t *pEdict = INDEXENT( i );
			if ( !pEdict || pEdict->free || !pEdict->pvPrivateData )
				continue;
			CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
			if ( !pEntity || pEntity == (CBaseEntity *)pViewer )
				continue;
			BOOL fPlayer = FALSE;
			if ( pEntity->IsPlayer() )
			{
				if ( !Logic_IsPlayerInPlay( pEntity ) || !pEntity->IsAlive() )
					continue;
				fPlayer = TRUE;
			}
			else if ( !FBitSet( pEntity->pev->flags, FL_MONSTER ) || !pEntity->IsAlive()
				|| pEntity->pev->deadflag != DEAD_NO || FBitSet( pEntity->pev->effects, EF_NODRAW ) )
				continue;
			entglow_candidate_t &c = s_Candidates[ iCandidates++ ];
			c.index = i;
			c.dist = ( pEntity->pev->origin - vecFrom ).Length();
			c.player = fPlayer && fPlayerColor;
		}
		qsort( s_Candidates, iCandidates, sizeof( s_Candidates[0] ), EntityGlow_CompareCandidates );
		for ( int i = 0; i < iCandidates && iCount + iPlayers < ENTGLOW_MAX_ALL; i++ )
		{
			if ( s_Candidates[i].player )
				iPlayer[ iPlayers++ ] = s_Candidates[i].index;
			else
				iTarget[ iCount++ ] = s_Candidates[i].index;
		}
		// the same order every time for the same set (the client gets a message only when the set changes)
		qsort( iTarget, iCount, sizeof( int ), []( const void *a, const void *b ) { return *(const int *)a - *(const int *)b; } );
		qsort( iPlayer, iPlayers, sizeof( int ), []( const void *a, const void *b ) { return *(const int *)a - *(const int *)b; } );
	}
	else if ( !FStringNull( pev->target ) )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->target ) ) ) != NULL )
		{
			if ( pEntity == this )
				continue;
			if ( iCount >= ENTGLOW_MAX_TARGETS )
			{
				if ( !m_fWarned )
					ALERT( at_console, "env_entity_glow \"%s\": more than %d entities named \"%s\", the rest do not glow\n",
						STRING( pev->targetname ), ENTGLOW_MAX_TARGETS, STRING( pev->target ) );
				m_fWarned = TRUE;
				break;
			}
			iTarget[ iCount++ ] = pEntity->entindex();
		}
	}

	BOOL fChanged = !m_fResolved || iCount != m_iTargets || iPlayers != m_iPlayers;
	for ( int i = 0; i < iCount && !fChanged; i++ )
		fChanged = ( iTarget[i] != m_iTarget[i] );
	for ( int i = 0; i < iPlayers && !fChanged; i++ )
		fChanged = ( iPlayer[i] != m_iPlayer[i] );

	m_iTargets = iCount;
	memcpy( m_iTarget, iTarget, sizeof( int ) * iCount );
	m_iPlayers = iPlayers;
	memcpy( m_iPlayer, iPlayer, sizeof( int ) * iPlayers );
	m_fResolved = TRUE;
	return fChanged;
}

// the targets of a glow that is on and draws through walls are sent to every client (g_bAlwaysSend), or with flag 1
// to its player only (g_iGlowViewers); also put back when another entity cleared the flag of a target (an env_dlight
// that let go of it)
void CEnvEntityGlow::UpdateHolds( void )
{
	int iWant[ ENTGLOW_MAX_ALL * 2 ];
	int iWanted = 0;
	int iViewer = 0;

	if ( m_fOn && DrawsHidden() )
	{
		CBasePlayer *pViewer = ActivatorOnly() ? Viewer() : NULL;
		if ( !ActivatorOnly() || pViewer )
		{
			iViewer = pViewer ? pViewer->entindex() : 0;
			for ( int i = 0; i < m_iTargets; i++ )
				iWant[ iWanted++ ] = m_iTarget[i];
			for ( int i = 0; i < m_iPlayers; i++ )
				iWant[ iWanted++ ] = m_iPlayer[i];
		}
	}

	for ( int i = 0; i < m_iHeld; i++ )
		EntityGlow_Release( m_iHold[i], m_iHeldViewer );
	for ( int i = 0; i < iWanted; i++ )
		EntityGlow_Hold( iWant[i], iViewer );

	m_iHeld = iWanted;
	m_iHeldViewer = iViewer;
	memcpy( m_iHold, iWant, sizeof( int ) * iWanted );
}

void CEnvEntityGlow::TurnOff( void )
{
	m_fOn = FALSE;
	m_flOffTime = 0.0f;
	SendState( NULL );
	UpdateHolds();
	pev->nextthink = 0;
}

void CEnvEntityGlow::ResolveThink( void )
{
	if ( m_fOn && ActivatorOnly() && !Viewer() )
	{
		TurnOff();	// its player left
		return;
	}
	if ( m_fOn && m_flOffTime > 0.0f && gpGlobals->time >= m_flOffTime )
	{
		TurnOff();
		// made by a plugin (no name): nothing can turn it on again
		if ( FStringNull( pev->targetname ) )
			UTIL_Remove( this );
		return;
	}

	if ( ResolveTargets() && m_fOn )
		SendState( NULL );
	UpdateHolds();

	if ( !m_fOn )
		pev->nextthink = 0;
	else if ( m_flOffTime > 0.0f && m_flOffTime < gpGlobals->time + ENTGLOW_RESOLVE_TIME )
		pev->nextthink = m_flOffTime;
	else
		pev->nextthink = gpGlobals->time + ENTGLOW_RESOLVE_TIME;
}

// one glow's state: id, flags; on: mode, style, color, count, targets
void CEnvEntityGlow::WriteGlow( int id, const int *color, int count, const int *targets )
{
	WRITE_SHORT( id );
	if ( !m_fOn || count < 0 )
	{
		WRITE_BYTE( 0 );
		return;
	}
	WRITE_BYTE( ENTGLOW_FLAG_ON );
	WRITE_BYTE( m_iMode );
	WRITE_BYTE( m_iStyle );
	for ( int i = 0; i < 4; i++ )
		WRITE_BYTE( color[i] );
	WRITE_BYTE( count );
	for ( int i = 0; i < count; i++ )
		WRITE_SHORT( targets[i] );
}

// EntGlow (cl_dll/entity_glow.cpp MsgFunc_EntGlow):
//   short id (this entity's index; + ENTGLOW_PLAYER_ID: the "!all" players' glow), byte flags (ENTGLOW_FLAG_ON)
//   not ON: nothing more (the client forgets the glow)
//   byte mode (0 always, 1 hidden only, 2 seen only), byte style (0 outline, 1 halo), byte r g b a, byte count,
//   count x short entity index
// At most 64 targets: 138 bytes. pPlayer NULL: everybody, or with flag 1 the player who sees it (nobody without one).
void CEnvEntityGlow::SendState( CBasePlayer *pPlayer )
{
	if ( !gmsgEntGlow )
		return;

	if ( ActivatorOnly() )
	{
		CBasePlayer *pViewer = Viewer();
		if ( !pViewer || ( pPlayer && pPlayer != pViewer ) )
			return;
		pPlayer = pViewer;
	}

	// the "!all" players' glow has a message of its own (its own color); off when it has none
	for ( int part = 0; part < 2; part++ )
	{
		if ( part == 1 && !TargetsAll() )
			break;
		if ( pPlayer )
			MESSAGE_BEGIN( MSG_ONE, gmsgEntGlow, NULL, pPlayer->pev );
		else
			MESSAGE_BEGIN( MSG_ALL, gmsgEntGlow );
		if ( part == 0 )
			WriteGlow( entindex(), m_iColor, m_iTargets, m_iTarget );
		else
			WriteGlow( entindex() + ENTGLOW_PLAYER_ID, m_iPlayerColor, m_iPlayers ? m_iPlayers : -1, m_iPlayer );
		MESSAGE_END();
	}
}

void CEnvEntityGlow::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// flag 1: the activator is the player who sees it; another player turning it on takes it over
	CBasePlayer *pOldViewer = ActivatorOnly() ? Viewer() : NULL;
	BOOL fNewViewer = FALSE;
	if ( ActivatorOnly() && useType != USE_OFF )
	{
		if ( !pActivator || !pActivator->IsPlayer() )
		{
			ALERT( at_aiconsole, "env_entity_glow \"%s\": \"Only the activator sees it\" needs a player to turn it on\n",
				STRING( pev->targetname ) );
			return;
		}
		fNewViewer = m_fOn && pOldViewer && pOldViewer != pActivator;
	}

	if ( fNewViewer )
	{
		// off for the old player, on for the new one
		m_fOn = FALSE;
		SendState( NULL );
		m_fOn = TRUE;
		m_hViewer = pActivator;
	}
	else
	{
		if ( !ShouldToggle( useType, m_fOn ) )
			return;
		if ( !m_fOn && ActivatorOnly() )
			m_hViewer = pActivator;
		if ( m_fOn )
		{
			TurnOff();
			return;
		}
		m_fOn = TRUE;
	}

	m_flOffTime = m_flDuration > 0.0f ? gpGlobals->time + m_flDuration : 0.0f;
	ResolveTargets();
	SendState( NULL );
	UpdateHolds();

	SetThink( &CEnvEntityGlow::ResolveThink );
	pev->nextthink = gpGlobals->time + ( ( m_flOffTime > 0.0f && m_flDuration < ENTGLOW_RESOLVE_TIME ) ? m_flDuration
		: ENTGLOW_RESOLVE_TIME );
}

// trigger_changevalue / trigger_copyvalue changed a key (keyvalue.cpp KeyValue_DispatchRuntime): GlowColor,
// PlayerColor, Mode, Style, target, duration. A glow that is on goes to its clients again (a new duration counts from
// now); StartDisabled does nothing once the map runs.
void CEnvEntityGlow::RuntimeKeyValueChanged( const char *keyName )
{
	ApplyLimits();

	if ( !stricmp( keyName, "target" ) )
		ResolveTargets();
	if ( !stricmp( keyName, "duration" ) && m_fOn )
	{
		m_flOffTime = m_flDuration > 0.0f ? gpGlobals->time + m_flDuration : 0.0f;
		pev->nextthink = gpGlobals->time + 0.01;
	}

	if ( m_fOn )
		SendState( NULL );
	UpdateHolds();
}

void CEnvEntityGlow::UpdateOnRemove( void )
{
	// killtarget, a plugin's remove_entity: the clients forget it, and its targets go back to the PVS
	if ( m_fOn )
	{
		m_fOn = FALSE;
		SendState( NULL );
	}
	UpdateHolds();
	CPointEntity::UpdateOnRemove();
}

// every player whose HUD is (re)initialised (CBasePlayer::UpdateClientData); the client forgets all glows on
// InitHUD and on a new map, so a glow that is off needs no message
void EntityGlow_SendAll( CBasePlayer *pPlayer )
{
	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "env_entity_glow" ) ) != NULL )
	{
		CEnvEntityGlow *pGlow = (CEnvEntityGlow *)pEntity;
		if ( !pGlow->m_fOn )
			continue;
		if ( !pGlow->m_fResolved )
		{
			pGlow->ResolveTargets();
			pGlow->UpdateHolds();
		}
		pGlow->SendState( pPlayer );
	}
}
