//=========================================================
// env_dlight -- a dynamic light (the engine's dlight, as a muzzle flash lights the world and the models) at a point,
// at a Locus position or on an entity, switched by triggers, for a while or until switched off (docs/ENV_DLIGHT.md).
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) commit 30fc2f71
// (2026-10-30), dlls/effects.cpp CEnvDLight (LRC's env_dlight from Spirit of Half-Life, made multiplayer: one keyed
// light per entity on the clients, resent to players who join later, positions sent while it tracks). The same class
// name, keys and spawnflags. Client side: cl_dll/dlights.cpp; the message: game_shared/dlight_shared.h.
//
// SBO changes (marked "SBO" below):
//   * Lights that are on reach players who join later through the shared send queue (client_sendqueue.h,
//     SENDQUEUE_DLIGHT) instead of Featureful's per-client SendMessages pass.
//   * Position updates while tracking go out unreliably (MSG_BROADCAST) and only when the position changed; the next
//     update makes up for a lost one. Featureful sent every 0.1 s on the reliable channel to everyone.
//   * "Always send" for a tracked entity uses SBO's g_bAlwaysSend (env_model.cpp, Featureful's EFLAG_ALWAYS_SEND),
//     only while the light follows it.
//   * The radius is clamped to what the message carries (0-1023 units, Featureful wrote any number as a short).
//   * Radius and Light color left out (or blank) are the FGD defaults, 80 and "255 255 255" (Featureful read them as
//     0: no light); a value written in the map, 0 included, is kept.
//
// env_elight (SBO 2.0, docs/ENV_DLIGHT.md#env_elight): the same entity as an entity light (the engine's elight, as
// Spirit of Half-Life's env_elight sent with TE_ELIGHT): it lights models only, never the world. Spirit of Half-Life
// 1.8 (Laurie Cheers) dlls/effects.cpp CEnvELight, its keys: "target" an entity to follow ([LE], blank: here),
// "impulse" the attachment on that entity (0: its origin, 1-4); "renderamt" the engine's elight radius in units, as
// Spirit wrote it into TE_ELIGHT. Here it is env_dlight's keyed light with the elight flag in the message (the client
// allocates CL_AllocElight instead of CL_AllocDlight), so it stays on, reaches late joiners and follows its target.
// The target removed: the light goes out. Radius and Light color left out (or blank) are the FGD defaults, 64 and
// "255 255 255" (Spirit read them as 0: no light); a value written in the map, 0 included, is kept.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"com_model.h"	// MAX_EDICTS
#include	"locus.h"
#include	"client_sendqueue.h"
#include	"light_origin.h"	// SBO 2.0: the light origins go out with the lights
#include	"../game_shared/dlight_shared.h"

extern int gmsgKeyedDLight;
extern BOOL g_bAlwaysSend[ MAX_EDICTS ];	// env_model.cpp

#define SF_DLIGHT_ONLYONCE					1
#define SF_DLIGHT_STARTON					2
#define SF_DLIGHT_TRACK_POSITION_UPDATE		4
#define SF_DLIGHT_POS_IS_LOCUS_ENTITY		8
#define SF_DLIGHT_POS_VALID					( 1 << 24 )

#define ELIGHT_MAX_ATTACHMENT	4		// env_elight "impulse": the studio model's attachments 1-4 (0: origin)
#define DLIGHT_DEFAULT_RADIUS	80		// env_dlight "renderamt" left out (FGD default)
#define ELIGHT_DEFAULT_RADIUS	64		// env_elight "renderamt" left out (FGD default)

#define DLIGHT_MAX_RADIUS		1023	// SBO

// SBO: the send queue (client_sendqueue.h) for players who join later
#define DLIGHT_QUEUE_DELAY		1.0
#define DLIGHT_QUEUE_INTERVAL	0.1
#define DLIGHT_QUEUE_BYTES		256

class CEnvDLight : public CPointEntity
{
public:
	void	Spawn( void );
	void	Activate( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	EXPORT TurnOffThink( void );
	// SBO 2.1 (Spirit of Half-Life): On while the light is on, Off while it is off (env_elight too)
	STATE	GetState( CBaseEntity *pActivator ) { return IsActive() ? STATE_ON : STATE_OFF; }
	void	EXPORT TrackPositionThink( void );
	void	CalcMyPosition( CBaseEntity *pActivator );
	void	TurnOn( CBasePlayer *pClient = NULL );
	void	TurnOff( void );
	void	SetupThink( void );
	void	UpdateOnRemove( void );
	void	RuntimeKeyValueChanged( const char *keyName );	// SBO 2.1
	void	ApplyLimits( void );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	BOOL	IsActive( void ) { return ( pev->spawnflags & SF_DLIGHT_STARTON ) != 0; }
	BOOL	IsELight( void ) { return FClassnameIs( pev, "env_elight" ); }	// SBO 2.0: env_elight
	BOOL	FollowsEntity( void );
	BOOL	TracksPosition( void );

	static int SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget );

	Vector	m_vecPos;
	BOOL	m_activated;
	EHANDLE	m_hActivator;
	EHANDLE	m_posEntity;
	Vector	m_vecSent;			// SBO: the position the clients have
	int		m_iAlwaysSend;		// SBO: the entity index this light set g_bAlwaysSend for (0: none)
	// SBO 2.0: the map gave "renderamt" / "rendercolor" (DLight_PreEntvarsKeyValue); read by Spawn only,
	// so not saved
	BOOL	m_bRadiusSet;
	BOOL	m_bColorSet;

private:
	static BOOL EntityPositionIsKnownOnClient( CBaseEntity *pEntity );
	void	HoldAlwaysSend( CBaseEntity *pEntity );
	void	ReleaseAlwaysSend( void );
};

LINK_ENTITY_TO_CLASS( env_dlight, CEnvDLight );
LINK_ENTITY_TO_CLASS( env_elight, CEnvDLight );	// SBO 2.0: the entity light (Spirit of Half-Life)

TYPEDESCRIPTION CEnvDLight::m_SaveData[] =
{
	DEFINE_FIELD( CEnvDLight, m_vecPos, FIELD_VECTOR ),
	DEFINE_FIELD( CEnvDLight, m_activated, FIELD_BOOLEAN ),
	DEFINE_FIELD( CEnvDLight, m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( CEnvDLight, m_posEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( CEnvDLight, m_vecSent, FIELD_VECTOR ),
	DEFINE_FIELD( CEnvDLight, m_iAlwaysSend, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CEnvDLight, CPointEntity );

// SBO 2.0: env_dlight's and env_elight's "renderamt" and "rendercolor" are entity fields, which the game's KeyValue
// never sees and which read a key left out the same as 0. DispatchKeyValue (cbase.cpp) gives them here first: a value
// is noted (the field still reads it), a blank one is dropped, so Spawn gives the FGD defaults to the keys the map did
// not set.
BOOL DLight_PreEntvarsKeyValue( edict_t *pent, KeyValueData *pkvd )
{
	if ( !pkvd->szClassName
		|| ( strcmp( pkvd->szClassName, "env_dlight" ) && strcmp( pkvd->szClassName, "env_elight" ) ) )
		return FALSE;
	const BOOL bRadius = FStrEq( pkvd->szKeyName, "renderamt" );
	if ( !bRadius && !FStrEq( pkvd->szKeyName, "rendercolor" ) )
		return FALSE;
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( pent );
	if ( !pEntity || ( !FClassnameIs( pEntity->pev, "env_dlight" ) && !FClassnameIs( pEntity->pev, "env_elight" ) ) )
		return FALSE;
	const char *pValue = pkvd->szValue ? pkvd->szValue : "";
	while ( *pValue == ' ' || *pValue == '\t' )
		pValue++;
	if ( !*pValue )
	{
		pkvd->fHandled = TRUE;	// blank: as left out
		return TRUE;
	}
	if ( bRadius )
		( (CEnvDLight *)pEntity )->m_bRadiusSet = TRUE;
	else
		( (CEnvDLight *)pEntity )->m_bColorSet = TRUE;
	return FALSE;
}

void CEnvDLight::Spawn( void )
{
	CPointEntity::Spawn();
	// SBO 2.0: the FGD defaults for the keys left out
	if ( !m_bRadiusSet )
		pev->renderamt = IsELight() ? ELIGHT_DEFAULT_RADIUS : DLIGHT_DEFAULT_RADIUS;
	if ( !m_bColorSet )
		pev->rendercolor = Vector( 255, 255, 255 );
	ApplyLimits();
}

// Spawn's limits (SBO 2.1: also after a run-time change)
void CEnvDLight::ApplyLimits( void )
{
	// SBO: what the message carries
	if ( pev->renderamt < 0 )
		pev->renderamt = 0;
	else if ( pev->renderamt > DLIGHT_MAX_RADIUS )
		pev->renderamt = DLIGHT_MAX_RADIUS;

	// SBO 2.0: env_elight's attachment
	if ( pev->impulse < 0 || pev->impulse > ELIGHT_MAX_ATTACHMENT )
		pev->impulse = 0;
}

// SBO 2.1: a key changed at run time (trigger_changevalue / trigger_copyvalue; radius "renderamt", color
// "rendercolor", env_elight's attachment "impulse", ...): Spawn's limits, and a light that is on goes to every client
// again with them (a light at its own place also moves to a new "origin"). Its time ("health") and position keys
// apply from its next turn-on.
void CEnvDLight::RuntimeKeyValueChanged( const char *keyName )
{
	ApplyLimits();

	if ( !m_activated || !IsActive() || !FBitSet( pev->spawnflags, SF_DLIGHT_POS_VALID ) )
		return;

	if ( !stricmp( keyName, "origin" ) && FStringNull( pev->message ) && !( IsELight() && !FStringNull( pev->target ) ) )
		m_vecPos = pev->origin;

	TurnOn();
}

// env_elight with a target follows that entity; env_dlight follows its Position with "Position is Locus Entity"
BOOL CEnvDLight::FollowsEntity( void )
{
	if ( IsELight() && !FStringNull( pev->target ) )
		return TRUE;
	return !FStringNull( pev->message ) && FBitSet( pev->spawnflags, SF_DLIGHT_POS_IS_LOCUS_ENTITY );
}

// the position is checked every 0.1 s while the light is on
BOOL CEnvDLight::TracksPosition( void )
{
	if ( IsELight() && !FStringNull( pev->target ) )
		return TRUE;
	return !FStringNull( pev->message ) && FBitSet( pev->spawnflags, SF_DLIGHT_TRACK_POSITION_UPDATE );
}

void CEnvDLight::Activate( void )
{
	if ( !m_activated )
	{
		// each env_dlight uses its own key to reference the light on the client
		m_activated = TRUE;

		if ( FStringNull( pev->targetname ) || IsActive() )
		{
			CalcMyPosition( this );
			// SBO: on from the start: the clients get it from the send queue when they join (Featureful:
			// SendMessages); here only the state
			if ( FBitSet( pev->spawnflags, SF_DLIGHT_POS_VALID ) )
				SetBits( pev->spawnflags, SF_DLIGHT_STARTON );
			else if ( IsELight() )
				ClearBits( pev->spawnflags, SF_DLIGHT_STARTON );	// SBO 2.0: env_elight without its target: off
			SetupThink();
		}
	}
}

void CEnvDLight::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, IsActive() ) )
		return;

	if ( IsActive() )
	{
		TurnOff();
		SetThink( NULL );
		pev->nextthink = -1.0f;
		return;
	}

	m_hActivator = pActivator;
	CalcMyPosition( pActivator );
	TurnOn();
	SetupThink();
}

void CEnvDLight::CalcMyPosition( CBaseEntity *pActivator )
{
	if ( IsELight() && !FStringNull( pev->target ) )
	{
		// SBO 2.0: env_elight on its target (Spirit: "can't find target": no light)
		m_posEntity = Locus_FindEntity( NULL, STRING( pev->target ), pActivator );
		if ( m_posEntity == NULL )
		{
			ALERT( at_console, "env_elight \"%s\" can't find target %s\n", STRING( pev->targetname ), STRING( pev->target ) );
			ClearBits( pev->spawnflags, SF_DLIGHT_POS_VALID );
			return;
		}
		m_vecPos = m_posEntity->pev->origin;
	}
	else if ( !FStringNull( pev->message ) )
	{
		if ( FBitSet( pev->spawnflags, SF_DLIGHT_POS_IS_LOCUS_ENTITY ) )
		{
			m_posEntity = Locus_FindEntity( NULL, STRING( pev->message ), pActivator );
			if ( m_posEntity != NULL )
				m_vecPos = m_posEntity->pev->origin;
			else
				return;
		}
		else
		{
			if ( !TryCalcLocus_Position( this, pActivator, STRING( pev->message ), m_vecPos ) )
				return;
		}
	}
	else
	{
		m_vecPos = pev->origin;
	}
	pev->spawnflags |= SF_DLIGHT_POS_VALID;
}

// pClient: one player (the send queue); NULL: everyone
void CEnvDLight::TurnOn( CBasePlayer *pClient )
{
	if ( !FBitSet( pev->spawnflags, SF_DLIGHT_POS_VALID ) || !gmsgKeyedDLight )
		return;

	int iFollow = 0;
	if ( m_posEntity != NULL && EntityPositionIsKnownOnClient( m_posEntity ) )
		iFollow = m_posEntity->entindex();

	const BOOL bELight = IsELight();
	SetBits( pev->spawnflags, SF_DLIGHT_STARTON );
	if ( pClient )
		MESSAGE_BEGIN( MSG_ONE, gmsgKeyedDLight, NULL, pClient->edict() );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgKeyedDLight );
		WRITE_SHORT( entindex() );
		WRITE_BYTE( bELight ? ( DLIGHT_MSG_ON | DLIGHT_MSG_ELIGHT ) : DLIGHT_MSG_ON );	// SBO 2.0: env_elight
		WRITE_COORD( m_vecPos.x );
		WRITE_COORD( m_vecPos.y );
		WRITE_COORD( m_vecPos.z );
		WRITE_SHORT( (int)max( 0.0f, min( pev->renderamt, (float)DLIGHT_MAX_RADIUS ) ) );	// radius (SBO: clamped, also after a trigger_changevalue)
		WRITE_BYTE( (int)pev->rendercolor.x );
		WRITE_BYTE( (int)pev->rendercolor.y );
		WRITE_BYTE( (int)pev->rendercolor.z );
		WRITE_SHORT( iFollow );
		if ( bELight )
			WRITE_BYTE( iFollow ? pev->impulse : 0 );	// SBO 2.0: env_elight: the attachment on the followed entity
	MESSAGE_END();

	if ( !pClient )
	{
		m_vecSent = m_vecPos;
		// SBO 2.0: env_elight: every client keeps the followed entity's position, even outside its view
		if ( bELight && iFollow )
			HoldAlwaysSend( m_posEntity );
	}
}

void CEnvDLight::TurnOff( void )
{
	if ( !FBitSet( pev->spawnflags, SF_DLIGHT_POS_VALID ) )
		return;

	ClearBits( pev->spawnflags, SF_DLIGHT_STARTON );
	if ( gmsgKeyedDLight )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgKeyedDLight );
			WRITE_SHORT( entindex() );
			WRITE_BYTE( DLIGHT_MSG_OFF );
		MESSAGE_END();
	}

	m_posEntity = NULL;
	ReleaseAlwaysSend();
}

void CEnvDLight::UpdateOnRemove( void )
{
	// SBO: a light removed while on ("Only once" ends with SUB_Remove after TurnOff) leaves no "always send" behind
	ReleaseAlwaysSend();
	CPointEntity::UpdateOnRemove();
}

void CEnvDLight::HoldAlwaysSend( CBaseEntity *pEntity )
{
	const int index = pEntity->entindex();
	if ( index > 0 && index < MAX_EDICTS && !g_bAlwaysSend[ index ] )
	{
		ReleaseAlwaysSend();
		g_bAlwaysSend[ index ] = TRUE;
		m_iAlwaysSend = index;
	}
}

void CEnvDLight::ReleaseAlwaysSend( void )
{
	if ( m_iAlwaysSend > 0 && m_iAlwaysSend < MAX_EDICTS )
	{
		edict_t *pEdict = INDEXENT( m_iAlwaysSend );
		// an env_model with "Always send" keeps its own
		if ( !pEdict || pEdict->free || !FClassnameIs( pEdict, "env_model" ) )
			g_bAlwaysSend[ m_iAlwaysSend ] = FALSE;
	}
	m_iAlwaysSend = 0;
}

void CEnvDLight::TurnOffThink( void )
{
	TurnOff();

	if ( pev->spawnflags & SF_DLIGHT_ONLYONCE )
	{
		SetThink( &CEnvDLight::SUB_Remove );
		pev->nextthink = gpGlobals->time;
	}
}

void CEnvDLight::TrackPositionThink( void )
{
	if ( TracksPosition() )
	{
		BOOL shouldSendPosition = TRUE;
		if ( FollowsEntity() )
		{
			CBaseEntity *pPosEntity = m_posEntity;
			if ( pPosEntity != NULL )
			{
				m_vecPos = pPosEntity->pev->origin;
				if ( EntityPositionIsKnownOnClient( pPosEntity ) )
				{
					// For visible entities the position is updated automatically on the client
					HoldAlwaysSend( pPosEntity );
					shouldSendPosition = FALSE;
				}
			}
			else if ( IsELight() )
			{
				// SBO 2.0: env_elight: its target was removed: the light goes out
				TurnOffThink();
				return;
			}
		}
		else
		{
			CalcMyPosition( m_hActivator );
		}
		// SBO: unreliable, and only a change
		if ( shouldSendPosition && gmsgKeyedDLight && ( m_vecPos - m_vecSent ).Length() > 0.5f )
		{
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgKeyedDLight );
				WRITE_SHORT( entindex() );
				WRITE_BYTE( DLIGHT_MSG_POS );
				WRITE_COORD( m_vecPos.x );
				WRITE_COORD( m_vecPos.y );
				WRITE_COORD( m_vecPos.z );
			MESSAGE_END();
			m_vecSent = m_vecPos;
		}
	}
	if ( pev->health > 0.0f && pev->dmgtime + pev->health <= gpGlobals->time )
	{
		TurnOffThink();
	}
	else
	{
		pev->nextthink = gpGlobals->time + 0.1f;
	}
}

void CEnvDLight::SetupThink( void )
{
	const BOOL shouldTrackPosition = TracksPosition();
	if ( pev->health > 0 )
	{
		if ( shouldTrackPosition )
		{
			SetThink( &CEnvDLight::TrackPositionThink );
			pev->dmgtime = gpGlobals->time;
			pev->nextthink = gpGlobals->time + 0.1f;
		}
		else
		{
			SetThink( &CEnvDLight::TurnOffThink );
			pev->nextthink = gpGlobals->time + pev->health;
		}
	}
	else
	{
		if ( shouldTrackPosition )
		{
			SetThink( &CEnvDLight::TrackPositionThink );
			pev->nextthink = gpGlobals->time + 0.1f;
		}
	}
}

BOOL CEnvDLight::EntityPositionIsKnownOnClient( CBaseEntity *pEntity )
{
	return pEntity->pev->modelindex && !FBitSet( pEntity->pev->effects, EF_NODRAW );
}

// SBO: the send queue (client_sendqueue.h, SENDQUEUE_DLIGHT): the lights that are on, in entity order from iCursor
int CEnvDLight::SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	for ( int index = max( iCursor, gpGlobals->maxClients + 1 ); index < gpGlobals->maxEntities; index++ )
	{
		edict_t *pEdict = INDEXENT( index );
		if ( FNullEnt( pEdict ) || pEdict->free || ( !FClassnameIs( pEdict, "env_dlight" ) && !FClassnameIs( pEdict, "env_elight" ) ) )
			continue;
		CEnvDLight *pLight = (CEnvDLight *)CBaseEntity::Instance( pEdict );
		if ( !pLight || !pLight->IsActive() )
			continue;
		if ( DLIGHT_MSG_BYTES > iBudget && iBudget < DLIGHT_QUEUE_BYTES )
		{
			iCursor = index;
			return FALSE;
		}
		pLight->TurnOn( pPlayer );
		iBudget -= DLIGHT_MSG_BYTES;
	}
	return TRUE;
}

// SBO 2.0: the lights (cursor: entity index), then the entities' light origins (light_origin.cpp; cursor from
// maxEntities on), one sender for both
static int Lights_SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	if ( iCursor < gpGlobals->maxEntities )
	{
		if ( !CEnvDLight::SendQueued( pPlayer, iCursor, iBudget ) )
			return FALSE;
		iCursor = gpGlobals->maxEntities;
	}
	int iSub = iCursor - gpGlobals->maxEntities;
	const int bDone = LightOrigin_SendQueued( pPlayer, iSub, iBudget );
	iCursor = gpGlobals->maxEntities + iSub;
	return bDone;
}

static SendQueue_Sender s_DLightSender( SENDQUEUE_DLIGHT, Lights_SendQueued, 0, DLIGHT_QUEUE_DELAY,
	DLIGHT_QUEUE_INTERVAL, DLIGHT_QUEUE_BYTES );
