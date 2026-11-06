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
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"
#include	"com_model.h"	// MAX_EDICTS
#include	"locus.h"
#include	"client_sendqueue.h"
#include	"../game_shared/dlight_shared.h"

extern int gmsgKeyedDLight;
extern BOOL g_bAlwaysSend[ MAX_EDICTS ];	// env_model.cpp

#define SF_DLIGHT_ONLYONCE					1
#define SF_DLIGHT_STARTON					2
#define SF_DLIGHT_TRACK_POSITION_UPDATE		4
#define SF_DLIGHT_POS_IS_LOCUS_ENTITY		8
#define SF_DLIGHT_POS_VALID					( 1 << 24 )

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
	void	EXPORT TrackPositionThink( void );
	void	CalcMyPosition( CBaseEntity *pActivator );
	void	TurnOn( CBasePlayer *pClient = NULL );
	void	TurnOff( void );
	void	SetupThink( void );
	void	UpdateOnRemove( void );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	BOOL	IsActive( void ) { return ( pev->spawnflags & SF_DLIGHT_STARTON ) != 0; }

	static int SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget );

	Vector	m_vecPos;
	BOOL	m_activated;
	EHANDLE	m_hActivator;
	EHANDLE	m_posEntity;
	Vector	m_vecSent;			// SBO: the position the clients have
	int		m_iAlwaysSend;		// SBO: the entity index this light set g_bAlwaysSend for (0: none)

private:
	static BOOL EntityPositionIsKnownOnClient( CBaseEntity *pEntity );
	void	ReleaseAlwaysSend( void );
};

LINK_ENTITY_TO_CLASS( env_dlight, CEnvDLight );

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

void CEnvDLight::Spawn( void )
{
	CPointEntity::Spawn();
	// SBO: what the message carries
	if ( pev->renderamt < 0 )
		pev->renderamt = 0;
	else if ( pev->renderamt > DLIGHT_MAX_RADIUS )
		pev->renderamt = DLIGHT_MAX_RADIUS;
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
	if ( !FStringNull( pev->message ) )
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

	SetBits( pev->spawnflags, SF_DLIGHT_STARTON );
	if ( pClient )
		MESSAGE_BEGIN( MSG_ONE, gmsgKeyedDLight, NULL, pClient->edict() );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgKeyedDLight );
		WRITE_SHORT( entindex() );
		WRITE_BYTE( DLIGHT_MSG_ON );
		WRITE_COORD( m_vecPos.x );
		WRITE_COORD( m_vecPos.y );
		WRITE_COORD( m_vecPos.z );
		WRITE_SHORT( (int)max( 0.0f, min( pev->renderamt, (float)DLIGHT_MAX_RADIUS ) ) );	// radius (SBO: clamped, also after a trigger_changevalue)
		WRITE_BYTE( (int)pev->rendercolor.x );
		WRITE_BYTE( (int)pev->rendercolor.y );
		WRITE_BYTE( (int)pev->rendercolor.z );
		WRITE_SHORT( iFollow );
	MESSAGE_END();

	if ( !pClient )
		m_vecSent = m_vecPos;
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
	if ( !FStringNull( pev->message ) )
	{
		BOOL shouldSendPosition = TRUE;
		if ( FBitSet( pev->spawnflags, SF_DLIGHT_POS_IS_LOCUS_ENTITY ) )
		{
			CBaseEntity *pPosEntity = m_posEntity;
			if ( pPosEntity != NULL )
			{
				m_vecPos = pPosEntity->pev->origin;
				if ( EntityPositionIsKnownOnClient( pPosEntity ) )
				{
					// For visible entities the position is updated automatically on the client
					const int index = pPosEntity->entindex();
					if ( index > 0 && index < MAX_EDICTS && !g_bAlwaysSend[ index ] )
					{
						ReleaseAlwaysSend();
						g_bAlwaysSend[ index ] = TRUE;
						m_iAlwaysSend = index;
					}
					shouldSendPosition = FALSE;
				}
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
	const BOOL shouldTrackPosition = !FStringNull( pev->message ) && FBitSet( pev->spawnflags, SF_DLIGHT_TRACK_POSITION_UPDATE );
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
		if ( FNullEnt( pEdict ) || pEdict->free || !FClassnameIs( pEdict, "env_dlight" ) )
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

static SendQueue_Sender s_DLightSender( SENDQUEUE_DLIGHT, CEnvDLight::SendQueued, 0, DLIGHT_QUEUE_DELAY,
	DLIGHT_QUEUE_INTERVAL, DLIGHT_QUEUE_BYTES );
