//=========================================================
// SBO 2.1 info_waypoint (docs/INFO_WAYPOINT.md): an objective locator. A HUD marker on a spot of the map: an icon
// (a sprite; sprites/waypoint.spr by default), an optional label, a color and an optional distance in meters. It shows
// through walls, and while the spot is off the screen it sits on the screen's edge with an arrow toward it.
//
// Keys: "model" (the icon sprite), "message" (the label), "rendercolor" (the color), "target" (Follow Entity: the
// marker moves with the first entity of that name, at the offset from its middle it had from the first one found). Flags: 1 Only the
// activator sees it (env_entity_glow's flag 1: the player who turns it on is the only one who gets it; turned on by
// something that is not a player, it stays off), 2 Start on, 4 Show distance. Use: On / Off / Toggle.
// trigger_changevalue changes message, rendercolor and target at run time (RuntimeKeyValueChanged).
//   "radius" (Reached Radius, units, 0 off): a marker that is on turns off when an alive player in play (never a
//     spectator) comes within it (with flag 1: its player), and fires "netname" (Target When Reached) with that player
//     as the activator. Checked every WAYPOINT_REACH_TIME seconds while on.
//   "min_range" / "max_range" (units, 0 no limit): a player closer / farther than that does not see the marker, nor
//     its edge arrow; the client decides per viewer (sent in the message).
//   "round_reset" 1 (round_hooks.h): at each round start back to its state at map load: on / off, label, color,
//     Follow Entity, position, no activator.
//   "duration" (seconds, 0 until turned off; env_entity_glow's key): each time it turns on it turns off by itself that
//     many seconds later (with flag 1 again for a player who takes it over).
//   Flag 8 No edge arrow: the client draws the marker only while its spot is in view.
//   Spectators: a marker with flag 1 also goes to the spectators watching its player (iuser2, any mode: the client
//     draws it only while it shows that player's view, in eye in the main view or the first-person inset), tagged
//     with the player's index; the set is looked at every WAYPOINT_SPEC_TIME seconds and a spectator who stops watching
//     gets it off. Nobody else gets it.
//
// The state goes to the clients in the "Waypoint" user message (game_shared/waypoint_shared.h): to everybody when it
// changes (only to its player with flag 1), and to a client whose HUD is (re)initialised (connect, map change,
// fullupdate, start of a demo) through the send queue (client_sendqueue.h, SENDQUEUE_WAYPOINT). Never per frame.
// A Follow Entity goes to the marker's clients whatever their PVS (env_entity_glow's holds, EntityGlow_Hold), so the
// client draws the marker on it as it moves; an entity the engine does not send (no model, EF_NODRAW) is followed by
// a new message when it has moved more than WAYPOINT_RESEND_DIST units, checked every WAYPOINT_THINK_TIME seconds.
//
// The client draws the marker (cl_dll/waypoint.cpp) with the HUD: none while the HUD is hidden.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"client_sendqueue.h"
#include	"waypoint_shared.h"
#include	"round_hooks.h"	// Logic_IsPlayerInPlay

extern int gmsgWaypoint;	// player.cpp

#define SF_WAYPOINT_ACTIVATOR_ONLY	1	// only the player who turns it on sees it
#define SF_WAYPOINT_START_ON		2
#define SF_WAYPOINT_DISTANCE		4	// the distance (meters) under the label
#define SF_WAYPOINT_NO_EDGE			8	// no edge clamp or arrow: shown only while its spot is in view

#define WAYPOINT_DEFAULT_SPRITE		"sprites/waypoint.spr"	// the client's own (no precache slot)
#define WAYPOINT_THINK_TIME			0.5		// seconds between two looks at the Follow Entity and the activator
#define WAYPOINT_RESEND_DIST		32.0	// units a Follow Entity the clients don't get moves before a new message
#define WAYPOINT_REACH_TIME			0.1		// seconds between two looks for a player within the Reached Radius
#define WAYPOINT_SPEC_TIME			0.2		// flag 1: seconds between two looks at who spectates its player
#define WAYPOINT_MAX_RANGE			32767	// min_range / max_range as sent (a short)

// the send queue (SENDQUEUE_WAYPOINT): after the HUD initialization, a round of messages every 0.1 s
#define WAYPOINT_QUEUE_DELAY		0.2f
#define WAYPOINT_QUEUE_INTERVAL		0.1f
#define WAYPOINT_QUEUE_BYTES		600

// the message id 0 (the world, never a marker): the client forgets every marker
#define WAYPOINT_ID_CLEAR			0

static unsigned int WaypointFloatBits( float f )
{
	union { float f; unsigned int i; } u;
	u.f = f;
	return u.i;
}

class CInfoWaypoint : public CPointEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void RoundReset( void );
	CBaseEntity *Reacher( void );			// a player within the Reached Radius, or NULL
	void EXPORT WaypointThink( void );
	void RuntimeKeyValueChanged( const char *keyName );
	void UpdateOnRemove( void );
	// Spirit of Half-Life: On while it shows
	STATE GetState( CBaseEntity *pActivator ) { return m_fOn ? STATE_ON : STATE_OFF; }

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	void SendState( CBasePlayer *pPlayer );	// NULL: everybody (or with flag 1 its player and his spectators)
	void SendTo( CBasePlayer *pPlayer, int iOwner, BOOL fOn );	// one message (pPlayer NULL: MSG_ALL)
	unsigned int Spectators( void );		// flag 1: the spectators watching its player (bit = index - 1)
	void UpdateSpectators( void );			// flag 1: the spectators who start / stop watching get it on / off
	void StartTimer( void );				// turned on: "duration"
	int MessageBytes( void );
	BOOL ActivatorOnly( void ) { return FBitSet( pev->spawnflags, SF_WAYPOINT_ACTIVATOR_ONLY ) != 0; }
	CBasePlayer *Viewer( void );			// flag 1: the player who sees it, while connected
	BOOL ResolveFollow( void );				// TRUE: the Follow Entity changed
	Vector Position( void );				// where the marker is now
	static Vector Center( CBaseEntity *pEntity );	// the middle of an entity's box (a brush entity's origin is 0 0 0)
	BOOL FollowSent( CBaseEntity *pFollow );	// the engine sends the entity to the marker's clients
	void UpdateHold( void );
	void TurnOff( void );
	void Think( BOOL fSoon );

	BOOL m_fOn;
	float m_flRadius;			// "radius": Reached Radius (0 off)
	int m_iMinRange, m_iMaxRange;	// "min_range" / "max_range" (0 no limit)
	BOOL m_fRoundReset;			// "round_reset"
	float m_flDuration;			// "duration" (0: until turned off)
	float m_flOffTime;			// on with a duration: when it turns off (0: never)
	// the state at map load (round_reset)
	string_t m_iszStartLabel, m_iszStartTarget;
	Vector m_vecStartColor, m_vecStartOrigin;
	EHANDLE m_hViewer;			// flag 1: the player who turned it on
	EHANDLE m_hFollow;			// the Follow Entity
	BOOL m_fFollowing;			// m_hFollow was set (it reads NULL once the entity is removed)
	BOOL m_fOffsetSet;			// m_vecOffset was taken from the first Follow Entity found
	Vector m_vecOffset;			// the marker's offset from the Follow Entity's middle (Center)
	Vector m_vecSent;			// the position in the last message
	Vector m_vecLast;			// with a Follow Entity: where the marker was at the last look (it stays there when the
								// entity is removed)

	// not saved: Precache sets it again after a restore; the hold is taken again by the next think
	int m_iSprite;				// the icon's model index (0: the default icon)
	int m_iHeld;				// the entity this marker holds in its clients' updates (0: none)
	int m_iHeldViewer;			// the player the hold is for (0: everybody)
	unsigned int m_iSpecSent;	// flag 1: the spectators that have it (bit = index - 1)
	BOOL m_fResolved;
};

LINK_ENTITY_TO_CLASS( info_waypoint, CInfoWaypoint );

TYPEDESCRIPTION CInfoWaypoint::m_SaveData[] =
{
	DEFINE_FIELD( CInfoWaypoint, m_fOn, FIELD_BOOLEAN ),
	DEFINE_FIELD( CInfoWaypoint, m_flRadius, FIELD_FLOAT ),
	DEFINE_FIELD( CInfoWaypoint, m_iMinRange, FIELD_INTEGER ),
	DEFINE_FIELD( CInfoWaypoint, m_iMaxRange, FIELD_INTEGER ),
	DEFINE_FIELD( CInfoWaypoint, m_fRoundReset, FIELD_BOOLEAN ),
	DEFINE_FIELD( CInfoWaypoint, m_flDuration, FIELD_FLOAT ),
	DEFINE_FIELD( CInfoWaypoint, m_flOffTime, FIELD_TIME ),
	DEFINE_FIELD( CInfoWaypoint, m_iszStartLabel, FIELD_STRING ),
	DEFINE_FIELD( CInfoWaypoint, m_iszStartTarget, FIELD_STRING ),
	DEFINE_FIELD( CInfoWaypoint, m_vecStartColor, FIELD_VECTOR ),
	DEFINE_FIELD( CInfoWaypoint, m_vecStartOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CInfoWaypoint, m_hViewer, FIELD_EHANDLE ),
	DEFINE_FIELD( CInfoWaypoint, m_hFollow, FIELD_EHANDLE ),
	DEFINE_FIELD( CInfoWaypoint, m_fFollowing, FIELD_BOOLEAN ),
	DEFINE_FIELD( CInfoWaypoint, m_fOffsetSet, FIELD_BOOLEAN ),
	DEFINE_FIELD( CInfoWaypoint, m_vecOffset, FIELD_VECTOR ),
	DEFINE_FIELD( CInfoWaypoint, m_vecSent, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CInfoWaypoint, m_vecLast, FIELD_POSITION_VECTOR ),
};

IMPLEMENT_SAVERESTORE( CInfoWaypoint, CPointEntity );

static int Waypoint_ClampRange( int i )
{
	return i < 0 ? 0 : ( i > WAYPOINT_MAX_RANGE ? WAYPOINT_MAX_RANGE : i );
}

void CInfoWaypoint::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "radius" ) )
	{
		m_flRadius = atof( pkvd->szValue );
		if ( !( m_flRadius > 0.0f ) )
			m_flRadius = 0.0f;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "min_range" ) )
	{
		m_iMinRange = Waypoint_ClampRange( atoi( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "max_range" ) )
	{
		m_iMaxRange = Waypoint_ClampRange( atoi( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "duration" ) )
	{
		m_flDuration = atof( pkvd->szValue );
		if ( !( m_flDuration > 0.0f ) )
			m_flDuration = 0.0f;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "round_reset" ) )	// round_hooks.h
	{
		m_fRoundReset = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CInfoWaypoint::Precache( void )
{
	// the default icon is the client's own file: no slot of the map's model list
	m_iSprite = 0;
	if ( !FStringNull( pev->model ) && STRING( pev->model )[0] && stricmp( STRING( pev->model ), WAYPOINT_DEFAULT_SPRITE ) )
		m_iSprite = PRECACHE_MODEL( (char *)STRING( pev->model ) );
}

void CInfoWaypoint::Spawn( void )
{
	// an Icon Sprite the server does not have would end the map at its precache (a console line says why)
	if ( !FStringNull( pev->model ) && STRING( pev->model )[0] && stricmp( STRING( pev->model ), WAYPOINT_DEFAULT_SPRITE )
		&& UTIL_MapperModelUnusable( pev, STRING( pev->model ) ) )
	{
		REMOVE_ENTITY( ENT( pev ) );
		return;
	}
	Precache();
	CPointEntity::Spawn();

	// with flag 1 it waits for a player to turn it on
	m_fOn = FBitSet( pev->spawnflags, SF_WAYPOINT_START_ON ) && !ActivatorOnly();
	m_iHeld = m_iHeldViewer = 0;
	m_iSpecSent = 0;
	m_fResolved = FALSE;
	m_flOffTime = 0.0f;
	if ( m_fOn )
		StartTimer();
	m_fFollowing = FALSE;
	m_fOffsetSet = FALSE;
	m_vecSent = m_vecLast = pev->origin;

	m_iszStartLabel = pev->message;
	m_iszStartTarget = pev->target;
	m_vecStartColor = pev->rendercolor;
	m_vecStartOrigin = pev->origin;

	// the Follow Entity spawns after or before this entity: looked up once everything has spawned
	SetThink( &CInfoWaypoint::WaypointThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

CBasePlayer *CInfoWaypoint::Viewer( void )
{
	CBaseEntity *pViewer = m_hViewer;
	if ( !pViewer || !pViewer->IsPlayer() || ( (CBasePlayer *)pViewer )->has_disconnected )
		return NULL;
	return (CBasePlayer *)pViewer;
}

// the first entity named "target" (not this one). The first one ever found sets the offset: where the mapper put the
// marker from its middle. A later one (a new name by trigger_changevalue, a new monster with the name after the first
// was removed) gets the marker at the same offset. When the entity is removed, the marker stays where it was last.
BOOL CInfoWaypoint::ResolveFollow( void )
{
	CBaseEntity *pOld = m_hFollow;
	CBaseEntity *pFollow = NULL;

	if ( !FStringNull( pev->target ) && STRING( pev->target )[0] )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->target ) ) ) != NULL )
		{
			if ( pEntity != this )
			{
				pFollow = pEntity;
				break;
			}
		}
	}

	m_fResolved = TRUE;
	if ( pFollow == pOld && ( pFollow || !m_fFollowing ) )
	{
		if ( pFollow )
			m_vecLast = Position();
		return FALSE;
	}

	if ( !pFollow )
		UTIL_SetOrigin( pev, m_fFollowing ? m_vecLast : Position() );	// lost: it stays where it was last
	else if ( !m_fOffsetSet )
	{
		m_vecOffset = ( m_fFollowing ? m_vecLast : pev->origin ) - Center( pFollow );
		m_fOffsetSet = TRUE;
	}
	m_hFollow = pFollow;
	m_fFollowing = pFollow != NULL;
	m_vecLast = Position();
	return TRUE;
}

Vector CInfoWaypoint::Center( CBaseEntity *pEntity )
{
	return pEntity->pev->origin + ( pEntity->pev->mins + pEntity->pev->maxs ) * 0.5;
}

Vector CInfoWaypoint::Position( void )
{
	CBaseEntity *pFollow = m_hFollow;
	if ( pFollow )
		return Center( pFollow ) + m_vecOffset;
	return m_fFollowing ? m_vecLast : pev->origin;	// removed, not looked up again yet: where it was last
}

// client.cpp AddToFullPack sends an entity with a model that is drawn
BOOL CInfoWaypoint::FollowSent( CBaseEntity *pFollow )
{
	return pFollow && pFollow->pev->modelindex && !FStringNull( pFollow->pev->model )
		&& pFollow->pev->effects != EF_NODRAW;
}

// the Follow Entity of a marker that is on goes to its clients whatever their PVS (env_entity_glow's holds: every
// client, or with flag 1 its player)
void CInfoWaypoint::UpdateHold( void )
{
	int iWant = 0, iViewer = 0;
	CBaseEntity *pFollow = m_hFollow;

	if ( m_fOn && pFollow )
	{
		CBasePlayer *pViewer = ActivatorOnly() ? Viewer() : NULL;
		if ( !ActivatorOnly() || pViewer )
		{
			iWant = pFollow->entindex();
			iViewer = pViewer ? pViewer->entindex() : 0;
		}
	}

	if ( iWant == m_iHeld && iViewer == m_iHeldViewer )
		return;
	if ( m_iHeld )
		EntityGlow_Release( m_iHeld, m_iHeldViewer );
	if ( iWant )
		EntityGlow_Hold( iWant, iViewer );
	m_iHeld = iWant;
	m_iHeldViewer = iViewer;
}

void CInfoWaypoint::TurnOff( void )
{
	m_fOn = FALSE;
	m_flOffTime = 0.0f;
	SendState( NULL );
	UpdateHold();
}

void CInfoWaypoint::StartTimer( void )
{
	m_flOffTime = m_flDuration > 0.0f ? gpGlobals->time + m_flDuration : 0.0f;
}

void CInfoWaypoint::Think( BOOL fSoon )
{
	// a marker that is on looks at its Follow Entity and its activator; one that is off waits for a trigger
	// the soonest of: the Follow Entity, the Reached Radius, the activator and his spectators, the duration's end
	SetThink( &CInfoWaypoint::WaypointThink );
	float flNext = 0.0f;
	if ( m_fOn )
	{
		float flWait = 1e9f;
		if ( !FStringNull( pev->target ) )
			flWait = fSoon ? 0.1f : WAYPOINT_THINK_TIME;
		if ( ActivatorOnly() && flWait > WAYPOINT_SPEC_TIME )
			flWait = WAYPOINT_SPEC_TIME;
		if ( m_flRadius > 0.0f && flWait > WAYPOINT_REACH_TIME )
			flWait = WAYPOINT_REACH_TIME;
		if ( flWait < 1e9f )
			flNext = gpGlobals->time + flWait;
		if ( m_flOffTime > 0.0f && ( !flNext || m_flOffTime < flNext ) )
			flNext = m_flOffTime;
	}
	pev->nextthink = flNext;
}

void CInfoWaypoint::WaypointThink( void )
{
	if ( m_fOn && ( ( ActivatorOnly() && !Viewer() ) || ( m_flOffTime > 0.0f && gpGlobals->time >= m_flOffTime ) ) )
	{
		TurnOff();	// its player left, or its duration is over
		Think( FALSE );
		return;
	}
	if ( m_fOn && ActivatorOnly() )
		UpdateSpectators();

	BOOL fSend = ResolveFollow();
	CBaseEntity *pFollow = m_hFollow;

	// Reached Radius: off (for everybody, or for its player), then "netname" with the player as the activator
	if ( m_fOn && m_flRadius > 0.0f )
	{
		CBaseEntity *pReacher = Reacher();
		if ( pReacher )
		{
			TurnOff();
			Think( FALSE );
			if ( !FStringNull( pev->netname ) )
				FireTargets( STRING( pev->netname ), pReacher, this, USE_TOGGLE, 0 );
			return;
		}
	}

	// an entity the clients don't get: a new position once it has moved far enough
	if ( pFollow && !FollowSent( pFollow ) && ( Position() - m_vecSent ).Length() > WAYPOINT_RESEND_DIST )
		fSend = TRUE;
	if ( fSend && m_fOn )
		SendState( NULL );
	UpdateHold();
	Think( FALSE );
}

int CInfoWaypoint::MessageBytes( void )
{
	const int iLabel = FStringNull( pev->message ) ? 0 : (int)strlen( STRING( pev->message ) );
	return 2 + 1 + 12 + 2 + 3 + 2 + 12 + 2 + 2 + 1 + min( iLabel, WAYPOINT_MAX_LABEL - 1 ) + 1;
}

// an alive player in play (never a spectator) within the Reached Radius of the marker; with flag 1 only its player
CBaseEntity *CInfoWaypoint::Reacher( void )
{
	const Vector vecPos = Position();
	if ( ActivatorOnly() )
	{
		CBasePlayer *pViewer = Viewer();
		if ( pViewer && Logic_IsPlayerInPlay( pViewer ) && pViewer->IsAlive()
			&& ( pViewer->pev->origin - vecPos ).Length() <= m_flRadius )
			return pViewer;
		return NULL;
	}
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && Logic_IsPlayerInPlay( pPlayer ) && pPlayer->IsAlive()
			&& ( pPlayer->pev->origin - vecPos ).Length() <= m_flRadius )
			return pPlayer;
	}
	return NULL;
}

// round_reset 1 (round_hooks.h RoundHooks_Reset, before the round's players spawn): back to its state at map load
void CInfoWaypoint::RoundReset( void )
{
	if ( !m_fRoundReset )
		return;

	const BOOL fWasOn = m_fOn;
	if ( fWasOn )
	{
		m_fOn = FALSE;
		SendState( NULL );	// off for whoever sees it now (with flag 1: its player)
	}
	pev->message = m_iszStartLabel;
	pev->target = m_iszStartTarget;
	pev->rendercolor = m_vecStartColor;
	UTIL_SetOrigin( pev, m_vecStartOrigin );
	m_hViewer = NULL;
	m_hFollow = NULL;
	m_fFollowing = FALSE;
	m_fOffsetSet = FALSE;
	m_vecSent = m_vecLast = pev->origin;

	m_fOn = FBitSet( pev->spawnflags, SF_WAYPOINT_START_ON ) && !ActivatorOnly();
	m_flOffTime = 0.0f;
	if ( m_fOn )
		StartTimer();
	ResolveFollow();
	if ( m_fOn )
		SendState( NULL );
	UpdateHold();
	Think( FALSE );
}

// Waypoint (game_shared/waypoint_shared.h): one message, to pPlayer (NULL: everybody). iOwner: 0 for the players who
// own it (everybody, or its activator), else the index of the player a spectator watches (flag 1). fOn FALSE: off.
void CInfoWaypoint::SendTo( CBasePlayer *pPlayer, int iOwner, BOOL fOn )
{
	if ( pPlayer )
		MESSAGE_BEGIN( MSG_ONE, gmsgWaypoint, NULL, pPlayer->pev );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgWaypoint );

	WRITE_SHORT( entindex() );
	if ( !fOn )
	{
		WRITE_BYTE( 0 );
		MESSAGE_END();
		return;
	}

	CBaseEntity *pFollow = m_hFollow;
	int iFlags = WAYPOINT_FLAG_ON;
	if ( FBitSet( pev->spawnflags, SF_WAYPOINT_DISTANCE ) )
		iFlags |= WAYPOINT_FLAG_DISTANCE;
	if ( pFollow )
		iFlags |= WAYPOINT_FLAG_FOLLOW;
	if ( FBitSet( pev->spawnflags, SF_WAYPOINT_NO_EDGE ) )
		iFlags |= WAYPOINT_FLAG_NO_EDGE;

	const Vector vecPos = Position();
	m_vecSent = vecPos;

	WRITE_BYTE( iFlags );
	WRITE_LONG( WaypointFloatBits( vecPos.x ) );
	WRITE_LONG( WaypointFloatBits( vecPos.y ) );
	WRITE_LONG( WaypointFloatBits( vecPos.z ) );
	WRITE_SHORT( m_iSprite );
	for ( int i = 0; i < 3; i++ )
	{
		const int c = (int)pev->rendercolor[i];
		WRITE_BYTE( c < 0 ? 0 : ( c > 255 ? 255 : c ) );
	}
	WRITE_SHORT( pFollow ? pFollow->entindex() : 0 );
	// the client puts the marker at the entity's origin + this (its middle + the offset)
	const Vector vecFromOrigin = pFollow ? vecPos - pFollow->pev->origin : g_vecZero;
	WRITE_LONG( WaypointFloatBits( vecFromOrigin.x ) );
	WRITE_LONG( WaypointFloatBits( vecFromOrigin.y ) );
	WRITE_LONG( WaypointFloatBits( vecFromOrigin.z ) );
	WRITE_SHORT( m_iMinRange );
	WRITE_SHORT( m_iMaxRange );
	WRITE_BYTE( iOwner );

	char szLabel[ WAYPOINT_MAX_LABEL ];
	strncpy( szLabel, FStringNull( pev->message ) ? "" : STRING( pev->message ), sizeof( szLabel ) - 1 );
	szLabel[ sizeof( szLabel ) - 1 ] = '\0';
	WRITE_STRING( szLabel );
	MESSAGE_END();
}

// the state to its clients. pPlayer NULL: everybody, or with flag 1 its player and the spectators that have it (none
// without a player); pPlayer set (the send queue): that client, if it is one of those.
void CInfoWaypoint::SendState( CBasePlayer *pPlayer )
{
	if ( !gmsgWaypoint )
		return;

	if ( !ActivatorOnly() )
	{
		SendTo( pPlayer, 0, m_fOn );
		return;
	}

	// (its player gone: only the spectators that have it get the off)
	CBasePlayer *pViewer = Viewer();
	if ( !pViewer && m_fOn )
		return;
	if ( pViewer && ( !pPlayer || pPlayer == pViewer ) )
		SendTo( pViewer, 0, m_fOn );
	for ( int i = 1; i <= gpGlobals->maxClients && i <= 32; i++ )
	{
		if ( !( m_iSpecSent & ( 1u << ( i - 1 ) ) ) )
			continue;
		CBasePlayer *pSpec = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pSpec || ( pPlayer && pPlayer != pSpec ) )
			continue;
		SendTo( pSpec, pViewer ? pViewer->entindex() : 0, m_fOn );
	}
	if ( !m_fOn && !pPlayer )
		m_iSpecSent = 0;
}

// flag 1: the spectators watching its player now (pev->iuser2, any observer mode: the client decides by its view)
unsigned int CInfoWaypoint::Spectators( void )
{
	CBasePlayer *pViewer = Viewer();
	if ( !pViewer || !m_fOn )
		return 0;
	const int iViewer = pViewer->entindex();
	unsigned int iMask = 0;
	for ( int i = 1; i <= gpGlobals->maxClients && i <= 32; i++ )
	{
		CBasePlayer *pSpec = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pSpec || pSpec == pViewer || pSpec->has_disconnected || FBitSet( pSpec->pev->flags, FL_FAKECLIENT ) )
			continue;
		if ( pSpec->pev->iuser1 && pSpec->pev->iuser2 == iViewer )
			iMask |= 1u << ( i - 1 );
	}
	return iMask;
}

void CInfoWaypoint::UpdateSpectators( void )
{
	const unsigned int iNow = Spectators();
	if ( iNow == m_iSpecSent )
		return;
	CBasePlayer *pViewer = Viewer();
	for ( int i = 1; i <= gpGlobals->maxClients && i <= 32; i++ )
	{
		const unsigned int bit = 1u << ( i - 1 );
		if ( ( iNow & bit ) == ( m_iSpecSent & bit ) )
			continue;
		CBasePlayer *pSpec = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pSpec && !pSpec->has_disconnected )
			SendTo( pSpec, pViewer ? pViewer->entindex() : 0, ( iNow & bit ) != 0 );
	}
	m_iSpecSent = iNow;
}

void CInfoWaypoint::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// flag 1: the activator is the player who sees it; another player turning it on takes it over
	if ( ActivatorOnly() && useType != USE_OFF )
	{
		if ( !pActivator || !pActivator->IsPlayer() )
		{
			ALERT( at_aiconsole, "info_waypoint \"%s\": \"Only the activator sees it\" needs a player to turn it on\n",
				STRING( pev->targetname ) );
			return;
		}
		CBasePlayer *pOldViewer = Viewer();
		if ( m_fOn && pOldViewer && pOldViewer != pActivator )
		{
			// off for the old player and his spectators, on for the new one (his own duration)
			m_fOn = FALSE;
			SendState( NULL );
			m_fOn = TRUE;
			m_hViewer = pActivator;
			StartTimer();
			ResolveFollow();
			SendState( NULL );
			UpdateSpectators();
			UpdateHold();
			Think( FALSE );
			return;
		}
	}

	if ( !ShouldToggle( useType, m_fOn ) )
		return;
	if ( m_fOn )
	{
		TurnOff();
		Think( FALSE );
		return;
	}

	if ( ActivatorOnly() )
		m_hViewer = pActivator;
	m_fOn = TRUE;
	StartTimer();
	ResolveFollow();
	SendState( NULL );
	if ( ActivatorOnly() )
		UpdateSpectators();
	UpdateHold();
	Think( FALSE );
}

// trigger_changevalue / trigger_copyvalue changed a key (keyvalue.cpp KeyValue_DispatchRuntime): message, rendercolor,
// target, origin, spawnflags, radius, min_range, max_range, duration. A marker that is on goes to its clients again. "model" is refused (it would need a
// precache).
void CInfoWaypoint::RuntimeKeyValueChanged( const char *keyName )
{
	// a new Follow Entity: the marker goes onto it at the same offset (the first one ever: from where the marker is);
	// an empty one leaves the marker where it is
	if ( !stricmp( keyName, "target" ) )
		ResolveFollow();
	if ( !stricmp( keyName, "duration" ) && m_fOn )
		StartTimer();	// a new duration counts from now
	if ( m_fOn )
		SendState( NULL );
	UpdateHold();
	Think( FALSE );
}

void CInfoWaypoint::UpdateOnRemove( void )
{
	// killtarget, a plugin's remove_entity: the clients forget it, and its Follow Entity goes back to the PVS
	if ( m_fOn )
	{
		m_fOn = FALSE;
		SendState( NULL );
	}
	UpdateHold();
	CPointEntity::UpdateOnRemove();
}

// the send queue (client_sendqueue.h, SENDQUEUE_WAYPOINT): cursor 0 the clear, then the markers that are on from
// entity index iCursor on
static int Waypoint_SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	if ( !gmsgWaypoint )
		return TRUE;

	if ( iCursor <= 0 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWaypoint, NULL, pPlayer->edict() );
			WRITE_SHORT( WAYPOINT_ID_CLEAR );
			WRITE_BYTE( 0 );
		MESSAGE_END();
		iBudget -= 3;
		iCursor = 1;
	}

	for ( int i = iCursor; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pEdict = INDEXENT( i );
		if ( !pEdict || pEdict->free || !pEdict->pvPrivateData || !FClassnameIs( pEdict, "info_waypoint" ) )
			continue;
		CInfoWaypoint *pWaypoint = (CInfoWaypoint *)CBaseEntity::Instance( pEdict );
		if ( !pWaypoint || !pWaypoint->m_fOn )
			continue;
		if ( pWaypoint->ActivatorOnly() && pWaypoint->Viewer() != pPlayer
			&& !( pWaypoint->m_iSpecSent & ( 1u << ( pPlayer->entindex() - 1 ) ) ) )
			continue;
		const int iBytes = pWaypoint->MessageBytes();
		if ( iBytes > iBudget )
		{
			iCursor = i;
			return FALSE;
		}
		if ( !pWaypoint->m_fResolved )
		{
			pWaypoint->ResolveFollow();
			pWaypoint->UpdateHold();
		}
		pWaypoint->SendState( pPlayer );
		iBudget -= iBytes;
	}
	return TRUE;
}

static SendQueue_Sender s_WaypointSender( SENDQUEUE_WAYPOINT, Waypoint_SendQueued, 0, WAYPOINT_QUEUE_DELAY,
	WAYPOINT_QUEUE_INTERVAL, WAYPOINT_QUEUE_BYTES );
