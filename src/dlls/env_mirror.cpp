//=========================================================
// env_mirror: Spirit of Half-Life 1.8's model mirror (G-Cont, dlls/effects.cpp CEnvMirror). Keyvalues and
// spawnflags: sdk/syndicate.fgd; mapper guide: docs/ENV_MIRROR.md.
//
// Only studio models are reflected: the client draws every model near an active mirror a second time, flipped
// across the mirror's plane (cl_dll/mirror.cpp, StudioModelRenderer.cpp). The world is not reflected: the mapper
// builds a mirrored copy of the room behind a translucent mirror brush.
//
// Differences from Spirit:
//   - the state goes to the clients in the "Mirror" user message, to every player whenever his HUD is (re)initialised
//     (connect, map change, fullupdate / start of a demo) and to everybody when the mirror is switched; Spirit
//     played a reliable global event once, when player 1 existed, so later players never got it
//   - the reflection plane and the side the reflection is meant for are sent (Spirit reflected about the brush's
//     center, and a floor about each model's own origin); no player_marker entity (the client reflects the local
//     player itself, see cl_dll/mirror.cpp)
//   - spawnflag 2 "Start Off" and the "side" key are SBO's
//
// Use: toggles (ON / OFF as asked). Off, the brush is drawn with the normal render mode (opaque), so the mirrored
// room behind it is hidden and no copies are drawn.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"saverestore.h"

#define SF_MIRROR_DRAWPLAYER	0x01	// Spirit: the players are reflected too (all of them, the local one included)
#define SF_MIRROR_STARTOFF		0x02	// SBO

// "Mirror" message flags (client: cl_dll/mirror.cpp)
#define MIRROR_FLAG_ON			0x01
#define MIRROR_FLAG_PLAYERS		0x02

#define MIRROR_DEFAULT_RADIUS	330	// Spirit's

extern int gmsgMirror;

class CEnvMirror : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	// SBO 2.1 (Spirit of Half-Life): On while it reflects, Off while it is off
	STATE GetState( CBaseEntity *pActivator ) { return m_fActive ? STATE_ON : STATE_OFF; }
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	void SendState( CBasePlayer *pPlayer );	// NULL: everybody

	BOOL m_fActive;
	float m_flRadius;
	int m_iSide;				// 1 = the positive side of the axis, -1 = the negative one, 0 = both
	int m_iInitialRenderMode;
};

LINK_ENTITY_TO_CLASS( env_mirror, CEnvMirror );

TYPEDESCRIPTION CEnvMirror::m_SaveData[] =
{
	DEFINE_FIELD( CEnvMirror, m_fActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CEnvMirror, m_flRadius, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvMirror, m_iSide, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvMirror, m_iInitialRenderMode, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CEnvMirror, CBaseEntity );

void CEnvMirror::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "radius" ) )
	{
		m_flRadius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "side" ) )
	{
		// 0 = both sides, 1 = positive (+X east, +Y north, top), 2 = negative
		int i = atoi( pkvd->szValue );
		m_iSide = ( i == 1 ) ? 1 : ( ( i == 2 ) ? -1 : 0 );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CEnvMirror::Spawn( void )
{
	// G-Cont: so that monsters walk on a shiny floor
	pev->flags |= FL_WORLDBRUSH;

	pev->angles = g_vecZero;
	pev->movetype = MOVETYPE_PUSH;	// so it doesn't get pushed by anything
	pev->solid = SOLID_BSP;
	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	m_iInitialRenderMode = pev->rendermode;

	if ( m_flRadius <= 0 )
		m_flRadius = MIRROR_DEFAULT_RADIUS;

	// "frags": the mirror's axis, 0 = X, 1 = Y, 2 = Z (a floor). Spirit: 0 (or no key) = the brush's thinnest side.
	if ( !pev->frags )
	{
		if ( pev->size.y > pev->size.x && pev->size.z > pev->size.x )
			pev->frags = 0;
		if ( pev->size.x > pev->size.y && pev->size.z > pev->size.y )
			pev->frags = 1;
		if ( pev->size.y > pev->size.z && pev->size.x > pev->size.z )
			pev->frags = 2;
	}
	if ( pev->frags < 0 || pev->frags > 2 )
		pev->frags = 0;

	// a floor reflects for the players above it
	if ( (int)pev->frags == 2 && !m_iSide )
		m_iSide = 1;

	m_fActive = !( pev->spawnflags & SF_MIRROR_STARTOFF );
	if ( !m_fActive )
		pev->rendermode = kRenderNormal;
}

// Mirror (cl_dll/mirror.cpp MsgFunc_Mirror):
//   short id (entity index), byte flags (MIRROR_FLAG_*)
//   not ON: nothing more (the client forgets the mirror)
//   byte axis (0 X, 1 Y, 2 Z), char side (1, -1, 0 = both), coord mins x y z, coord maxs x y z (the brush: the
//   radius is measured from its center, and a reflection is only drawn where it is seen through the brush),
//   coord plane (the reflection plane on the axis: the face on the reflecting side, or the brush's middle when both
//   sides reflect), short radius
void CEnvMirror::SendState( CBasePlayer *pPlayer )
{
	if ( !gmsgMirror )
		return;

	if ( pPlayer )
		MESSAGE_BEGIN( MSG_ONE, gmsgMirror, NULL, pPlayer->pev );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgMirror );

	WRITE_SHORT( entindex() );

	if ( !m_fActive )
	{
		WRITE_BYTE( 0 );
		MESSAGE_END();
		return;
	}

	int axis = (int)pev->frags;
	// not absmin / absmax: the engine widens those by a unit. The brush model's own bounds are a unit larger than
	// its faces too (the compiler pads them): without that unit the plane is the glass' face itself (mirror_debug
	// showed it a unit off: a model resting on a shiny floor was reflected a unit into the room, over itself)
	Vector vecMins = pev->origin + pev->mins;
	Vector vecMaxs = pev->origin + pev->maxs;
	for ( int i = 0; i < 3; i++ )
	{
		if ( vecMaxs[i] - vecMins[i] > 2 )
		{
			vecMins[i] += 1;
			vecMaxs[i] -= 1;
		}
	}
	Vector vecCenter = ( vecMins + vecMaxs ) * 0.5;
	float flPlane = ( m_iSide > 0 ) ? vecMaxs[axis] : ( ( m_iSide < 0 ) ? vecMins[axis] : vecCenter[axis] );

	int flags = MIRROR_FLAG_ON;
	if ( pev->spawnflags & SF_MIRROR_DRAWPLAYER )
		flags |= MIRROR_FLAG_PLAYERS;

	int radius = (int)m_flRadius;
	if ( radius > 32767 )
		radius = 32767;

	WRITE_BYTE( flags );
	WRITE_BYTE( axis );
	WRITE_CHAR( m_iSide );
	WRITE_COORD( vecMins.x );
	WRITE_COORD( vecMins.y );
	WRITE_COORD( vecMins.z );
	WRITE_COORD( vecMaxs.x );
	WRITE_COORD( vecMaxs.y );
	WRITE_COORD( vecMaxs.z );
	WRITE_COORD( flPlane );
	WRITE_SHORT( radius );
	MESSAGE_END();
}

void CEnvMirror::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_fActive ) )
		return;

	m_fActive = !m_fActive;

	// Spirit: off, the brush is opaque (it hides the mirrored room)
	pev->rendermode = m_fActive ? m_iInitialRenderMode : kRenderNormal;

	SendState( NULL );
}

// every player whose HUD is (re)initialised (CBasePlayer::UpdateClientData); the client forgets all mirrors on
// InitHUD and on a new map, so an inactive mirror needs no message
void Mirror_SendAll( CBasePlayer *pPlayer )
{
	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "env_mirror" ) ) != NULL )
	{
		CEnvMirror *pMirror = (CEnvMirror *)pEntity;
		if ( pMirror->m_fActive )
			pMirror->SendState( pPlayer );
	}
}
