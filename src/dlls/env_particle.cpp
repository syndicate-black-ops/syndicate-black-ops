//=========================================================
// env_particle: a Spirit of Half-Life Aurora particle system (aurora/<name>.aur), drawn by the client
// (cl_dll/aurora). Keyvalues and spawnflags: sdk/syndicate.fgd; mapper guide: docs/AURORA.md.
//
// Spirit of Half-Life's CParticle (LRC; halflife-updated-spirit, LogicAndTrick, spirit-1.8-bug-fixes, 2482306,
// dlls/effects.cpp) sent its "Particle" message once, to everybody, a second after the map started, and switched
// the effect by the entity's "body" (a networked entity with sprites/null.spr). Clients that connected later
// (every client after a map change on a dedicated server), spectators and demos started mid-map got nothing, and
// each Spawn Use made a new entity that was never removed.
//
// SBO 2.0 sends the whole state (aurora_shared.h) instead:
//  - to each client after its HUD initialised (first InitHUD on the map, "fullupdate", demo start), a few
//    entities per tenth of a second (EnvParticle_SendQueued, client_sendqueue.h), so a map
//    with many emitters doesn't fill the reliable channel;
//  - to everybody when it is switched (at most every tenth of a second; the last state wins) or removed.
//  The client keeps one system per entity and updates it, so state sent again changes nothing on screen.
//  The entity itself is not sent to the clients (no model, EF_NODRAW).
// New keys (written for SBO; the idea from XDM's env_rendersystem): "target", an entity the effect moves with,
// and "attachment", one of that model's attachment points. Spawn Use copies are one-shot client systems.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"aurora_shared.h"
#include	"client_sendqueue.h"

#include	<string.h>

extern int gmsgAurora;

#define SF_PARTICLE_ON			1	// with a name: on at the start (without a name it is always on)
#define SF_PARTICLE_SPAWNUSE	2	// each trigger shows a one-shot copy at the activator

#define AURORA_SEND_INTERVAL	0.1f	// seconds between two state messages of one entity to everybody
#define AURORA_QUEUE_DELAY		0.5f	// after the client's InitHUD, before its first systems
#define AURORA_QUEUE_BYTES		600		// state messages to one client per AURORA_SEND_INTERVAL (bytes)
#define AURORA_SPAWNUSE_PER_SEC	10		// Spawn Use copies per second and entity; more are ignored
#define AURORA_BROADCAST_BYTES	600		// state messages to everybody per AURORA_SEND_INTERVAL, all entities together

// what went to everybody in this interval (many env_particle switched by one name wait for the next one)
static float s_flBroadcastStart = -1;
static int s_iBroadcastBytes = 0;

class CEnvParticle : public CPointEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think( void );
	void UpdateOnRemove( void );

	void ResolveTarget( BOOL bFirst );	// finds "target"; bFirst: measures the offset from it
	BOOL IsValid( void ) const { return m_bValid; }
	int MessageSize( void ) const;
	void WriteState( int iDest, edict_t *pClient );

private:
	void StateChanged( void );
	void SpawnCopy( CBaseEntity *pActivator );
	void PrecacheScript( void );

	BOOL m_bValid;			// a usable script name
	BOOL m_bOn;
	BOOL m_bDirty;			// a state change not sent yet
	float m_flNextSend;

	int m_iAttachment;		// "attachment" (0 .. 3), -1 = the target's origin
	BOOL m_bAttachmentKey;	// the key was given (a missing key reads as 0, the first attachment, otherwise)
	BOOL m_bTargetResolved;	// the offset was measured (once, from where the target was placed)
	EHANDLE m_hTarget;
	Vector m_vecOffset;		// from the target's origin, in its frame
	Vector m_vecAngleOffset;

	float m_flCopyWindow;	// Spawn Use rate limit
	int m_iCopies;
};

LINK_ENTITY_TO_CLASS( env_particle, CEnvParticle );

// the float's bits in a long (the client's READ_FLOAT): full precision, any map size
static void WRITE_FLOAT_BITS( float f )
{
	int i;
	memcpy( &i, &f, sizeof( i ) );
	WRITE_LONG( i );
}

void CEnvParticle::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "attachment" ) )
	{
		m_iAttachment = atoi( pkvd->szValue );
		m_bAttachmentKey = TRUE;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CEnvParticle::Spawn( void )
{
	// "attachment": 0 .. 3, the model's attachment as model viewers number them; -1 or no key: the target's origin
	if ( !m_bAttachmentKey )
		m_iAttachment = -1;
	else if ( m_iAttachment < -1 || m_iAttachment > 3 )
	{
		ALERT( at_console, "env_particle \"%s\": attachment %d, only 0 to 3 (or -1, none) exist\n", STRING( pev->targetname ), m_iAttachment );
		m_iAttachment = -1;
	}

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;		// the client draws it from the Aurora message; the entity is never sent
	UTIL_SetOrigin( pev, pev->origin );

	const char *szScript = STRING( pev->message );
	m_bValid = szScript[0] && strlen( szScript ) < AURORA_MAX_SCRIPT && !strstr( szScript, ".." )
		&& szScript[0] != '/' && szScript[0] != '\\' && !strchr( szScript, ':' );
	if ( !m_bValid )
		ALERT( at_console, "env_particle \"%s\": no usable particle script (\"message\", e.g. aurora/smoke.aur, at most %d characters)\n",
			STRING( pev->targetname ), AURORA_MAX_SCRIPT - 1 );

	// Spirit: the "Start On" flag counts only with a name; without one nothing can switch it on. With Spawn Use the
	// entity's own effect follows the same rule (Spirit: its Use makes copies and never switches it).
	m_bOn = FStringNull( pev->targetname ) ? TRUE : ( ( pev->spawnflags & SF_PARTICLE_ON ) != 0 );

	Precache();
}

// The script and its sprites go in the download list (sv_allowdownload / sv_downloadurl), like env_tutor's icon.
// The client loads them itself (COM_LoadFile, SPR_Load); nothing here is a model precache.
void CEnvParticle::PrecacheScript( void )
{
	const char *szScript = STRING( pev->message );

	int iLength = 0;
	char *pFile = (char *)LOAD_FILE_FOR_ME( (char *)szScript, &iLength );
	if ( !pFile )
	{
		ALERT( at_console, "env_particle \"%s\": particle script %s not found\n", STRING( pev->targetname ), szScript );
		return;
	}

	PRECACHE_GENERIC( (char *)szScript );

	// "sprite" "<file>" pairs: a small tokenizer (quoted strings, words, // comments), as the client's
	// COM_ParseFile reads the file
	char szToken[256];
	bool bSprite = false;
	int i = 0;
	while ( i < iLength )
	{
		const char c = pFile[i];
		if ( c == '/' && i + 1 < iLength && pFile[i + 1] == '/' )
		{
			while ( i < iLength && pFile[i] != '\n' )
				i++;
			continue;
		}
		if ( (unsigned char)c <= ' ' )
		{
			i++;
			continue;
		}

		int n = 0;
		if ( c == '"' )
		{
			i++;
			while ( i < iLength && pFile[i] != '"' )
			{
				if ( n < (int)sizeof( szToken ) - 1 )
					szToken[n++] = pFile[i];
				i++;
			}
			i++;
		}
		else
		{
			while ( i < iLength && (unsigned char)pFile[i] > ' ' )
			{
				if ( n < (int)sizeof( szToken ) - 1 )
					szToken[n++] = pFile[i];
				i++;
			}
		}
		szToken[n] = 0;

		if ( bSprite )
		{
			bSprite = false;

			// a sprite that isn't there would stop the download of the rest; the client says it's missing
			int iSpriteLength = 0;
			byte *pSprite = LOAD_FILE_FOR_ME( szToken, &iSpriteLength );
			if ( pSprite )
			{
				FREE_FILE( pSprite );
				if ( n < AURORA_MAX_SCRIPT )
					PRECACHE_GENERIC( (char *)STRING( ALLOC_STRING( szToken ) ) );
			}
			else
				ALERT( at_console, "env_particle \"%s\": %s: sprite %s not found\n", STRING( pev->targetname ), szScript, szToken );
		}
		else if ( !stricmp( szToken, "sprite" ) )
		{
			bSprite = true;
		}
	}

	FREE_FILE( pFile );
}

void CEnvParticle::Precache( void )
{
	if ( m_bValid )
		PrecacheScript();
}

// ServerActivate, before any entity's Activate (a func_train moves to its first path_corner in its Activate): the
// offset from the target is measured where the mapper placed both. Later (the target was removed and an entity of
// that name appears) the same offset is used on the new one.
void CEnvParticle::ResolveTarget( BOOL bFirst )
{
	if ( FStringNull( pev->target ) )
		return;

	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
	if ( pTarget == this )
		pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ) );

	m_hTarget = pTarget;

	if ( !pTarget )
	{
		if ( bFirst )
			ALERT( at_console, "env_particle \"%s\": target \"%s\" not found, the effect is hidden until it exists\n",
				STRING( pev->targetname ), STRING( pev->target ) );
		return;
	}

	if ( m_bTargetResolved )
		return;
	m_bTargetResolved = TRUE;

	// an attachment needs a studio model
	if ( m_iAttachment >= 0 )
	{
		const char *szModel = STRING( pTarget->pev->model );
		const size_t len = strlen( szModel );
		if ( len < 4 || stricmp( szModel + len - 4, ".mdl" ) )
		{
			ALERT( at_console, "env_particle \"%s\": target \"%s\" is not a studio model, attachment ignored\n",
				STRING( pev->targetname ), STRING( pev->target ) );
			m_iAttachment = -1;
		}
	}

	// the offset in the target's frame (the client turns it back with the target's angles)
	Vector forward, right, up;
	UTIL_MakeVectorsPrivate( pTarget->pev->angles, forward, right, up );
	const Vector vecDelta = pev->origin - pTarget->pev->origin;
	m_vecOffset = Vector( DotProduct( vecDelta, forward ), -DotProduct( vecDelta, right ), DotProduct( vecDelta, up ) );
	m_vecAngleOffset = pev->angles - pTarget->pev->angles;
}

int CEnvParticle::MessageSize( void ) const
{
	return 1 + 2 + 12 + 6 + 3 + (int)strlen( STRING( pev->message ) ) + 1;
}

void CEnvParticle::WriteState( int iDest, edict_t *pClient )
{
	CBaseEntity *pTarget = m_hTarget;
	const BOOL bFollow = !FStringNull( pev->target );

	int iFlags = 0;
	// a target that doesn't exist (yet, or any more): hidden
	if ( m_bOn && ( !bFollow || pTarget ) )
		iFlags |= AURORA_MSG_ON;
	if ( bFollow && pTarget )
		iFlags |= AURORA_MSG_FOLLOW;

	const Vector &vecOrigin = ( iFlags & AURORA_MSG_FOLLOW ) ? m_vecOffset : pev->origin;
	const Vector &vecAngles = ( iFlags & AURORA_MSG_FOLLOW ) ? m_vecAngleOffset : pev->angles;

	MESSAGE_BEGIN( iDest, gmsgAurora, NULL, pClient );
		WRITE_BYTE( iFlags );
		WRITE_SHORT( entindex() );
		WRITE_FLOAT_BITS( vecOrigin.x );
		WRITE_FLOAT_BITS( vecOrigin.y );
		WRITE_FLOAT_BITS( vecOrigin.z );
		WRITE_COORD( vecAngles.x );
		WRITE_COORD( vecAngles.y );
		WRITE_COORD( vecAngles.z );
		if ( iFlags & AURORA_MSG_FOLLOW )
		{
			WRITE_SHORT( pTarget->entindex() );
			WRITE_CHAR( m_iAttachment );
		}
		WRITE_STRING( STRING( pev->message ) );
	MESSAGE_END();
}

// to everybody, at most every AURORA_SEND_INTERVAL: a trigger firing every frame sends the last state only
void CEnvParticle::StateChanged( void )
{
	m_bDirty = TRUE;

	if ( gpGlobals->time >= m_flNextSend )
		Think();
	else
		pev->nextthink = m_flNextSend;
}

void CEnvParticle::Think( void )
{
	pev->nextthink = 0;

	// a followed entity that went away: one with the same name may come (a monstermaker's next monster)
	if ( !FStringNull( pev->target ) )
	{
		CBaseEntity *pOld = m_hTarget;
		if ( !pOld )
		{
			ResolveTarget( FALSE );
			if ( (CBaseEntity *)m_hTarget )
				m_bDirty = TRUE;
		}
		pev->nextthink = gpGlobals->time + 1.0f;
	}

	if ( m_bDirty && m_bValid )
	{
		if ( gpGlobals->time < m_flNextSend )
		{
			pev->nextthink = m_flNextSend;
			return;
		}

		if ( gpGlobals->time < s_flBroadcastStart || gpGlobals->time - s_flBroadcastStart >= AURORA_SEND_INTERVAL )
		{
			s_flBroadcastStart = gpGlobals->time;
			s_iBroadcastBytes = 0;
		}
		if ( s_iBroadcastBytes + MessageSize() > AURORA_BROADCAST_BYTES )
		{
			pev->nextthink = s_flBroadcastStart + AURORA_SEND_INTERVAL;
			return;
		}
		s_iBroadcastBytes += MessageSize();

		WriteState( MSG_ALL, NULL );
		m_bDirty = FALSE;
		m_flNextSend = gpGlobals->time + AURORA_SEND_INTERVAL;
	}
}

void CEnvParticle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !m_bValid )
		return;

	if ( pev->spawnflags & SF_PARTICLE_SPAWNUSE )
	{
		SpawnCopy( pActivator );
		return;
	}

	if ( !ShouldToggle( useType, m_bOn ) )
		return;

	m_bOn = !m_bOn;
	StateChanged();
}

// Spirit made a new env_particle (an edict, never removed) at the activator for each Use; SBO sends a one-shot
// system that the clients run for its main type's "lifetime" and then drop. Clients that join later don't get it.
void CEnvParticle::SpawnCopy( CBaseEntity *pActivator )
{
	if ( gpGlobals->time - m_flCopyWindow >= 1.0f )
	{
		m_flCopyWindow = gpGlobals->time;
		m_iCopies = 0;
	}
	if ( ++m_iCopies > AURORA_SPAWNUSE_PER_SEC )
		return;

	CBaseEntity *pWhere = pActivator ? pActivator : this;

	MESSAGE_BEGIN( MSG_ALL, gmsgAurora );
		WRITE_BYTE( AURORA_MSG_ON | AURORA_MSG_ONESHOT );
		WRITE_SHORT( 0 );
		WRITE_FLOAT_BITS( pWhere->pev->origin.x );
		WRITE_FLOAT_BITS( pWhere->pev->origin.y );
		WRITE_FLOAT_BITS( pWhere->pev->origin.z );
		WRITE_COORD( pWhere->pev->angles.x );
		WRITE_COORD( pWhere->pev->angles.y );
		WRITE_COORD( pWhere->pev->angles.z );
		WRITE_STRING( STRING( pev->message ) );
	MESSAGE_END();
}

void CEnvParticle::UpdateOnRemove( void )
{
	if ( m_bValid )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgAurora );
			WRITE_BYTE( AURORA_MSG_REMOVE );
			WRITE_SHORT( entindex() );
		MESSAGE_END();
	}

	CPointEntity::UpdateOnRemove();
}

//=========================================================
// Delivery to each client
//=========================================================

// client.cpp ServerActivate, before the entities' Activate
void EnvParticle_ServerActivate( void )
{
	s_flBroadcastStart = -1;	// the clock starts over with the map
	s_iBroadcastBytes = 0;

	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "env_particle" ) ) != NULL )
	{
		CEnvParticle *pParticle = (CEnvParticle *)pEntity;
		pParticle->ResolveTarget( TRUE );

		// keeps looking for a target that isn't there (yet)
		if ( !FStringNull( pParticle->pev->target ) )
			pParticle->pev->nextthink = gpGlobals->time + 1.0f;
	}
}

// the client's HUD was (re)initialised (client_sendqueue.cpp): every env_particle, from the first entity index on,
// AURORA_QUEUE_BYTES every AURORA_SEND_INTERVAL
static int EnvParticle_SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	for ( int i = iCursor; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pEdict = INDEXENT( i );
		if ( !pEdict || pEdict->free || !pEdict->pvPrivateData || !FClassnameIs( &pEdict->v, "env_particle" ) )
			continue;

		CEnvParticle *pParticle = (CEnvParticle *)CBaseEntity::Instance( pEdict );
		if ( !pParticle || !pParticle->IsValid() )
			continue;

		const int iSize = pParticle->MessageSize();
		if ( iSize > iBudget )
		{
			// the rest in the next round
			iCursor = i;
			return FALSE;
		}

		pParticle->WriteState( MSG_ONE, pPlayer->edict() );
		iBudget -= iSize;
	}

	return TRUE;
}

static SendQueue_Sender s_AuroraSender( SENDQUEUE_AURORA, EnvParticle_SendQueued, 1, AURORA_QUEUE_DELAY,
	AURORA_SEND_INTERVAL, AURORA_QUEUE_BYTES );
