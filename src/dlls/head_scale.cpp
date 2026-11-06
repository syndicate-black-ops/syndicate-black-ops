//=========================================================
// SBO 2.0 player head scale (docs/MODEL_SCALE.md "Head scale"): the "headscale" key on a player draws his head and
// sets his head hitbox that many times its size (0.25 .. 4; 1: normal). Set by a server plugin only
// (DispatchKeyValue( id, "headscale", "2.5" ), AMXX engine module) or test_kv: trigger_changevalue /
// trigger_copyvalue refuse it (KeyValue_IsPluginOnly, keyvalue.cpp), and a map entity is never a connected player.
// The bones it scales: game_shared/head_scale_shared.h.
//
// - Clients: one "HeadScale" message to all on a change, and the players' head scales to a client whose HUD was
//   (re)initialised (the send queue, client_sendqueue.h).
// - Hitboxes: animation.cpp SV_StudioSetupBonesScaled (the server's bone setup) scales the head bones of
//   the player's hitbox model (valve's models/player.mdl) with the same function the client draws with.
// - The engine tests a player's hitboxes only for traces that reach his absmin / absmax: a bigger head reaches out
//   of his box, so his absmin / absmax grow by as much (SetObjectCollisionBox; his movement box, pev->mins /
//   maxs, stays).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"client_sendqueue.h"
#include	"studio.h"
#include	"../game_shared/head_scale_shared.h"

extern int gmsgHeadScale;

// how far the head of the player model reaches from its bone's origin at scale 1 (player.mdl's head hitbox: 8.4
// along the bone, about 5 across), with a margin
#define HEADSCALE_REACH			10.0f

#define HEADSCALE_QUEUE_DELAY		0.5
#define HEADSCALE_QUEUE_INTERVAL	0.1
#define HEADSCALE_QUEUE_BYTES		120
#define HEADSCALE_MSG_BYTES			6		// header + 3

static float HeadScale_Clamp( float flScale )
{
	if ( !( flScale > 0.0f ) || fabs( flScale - 1.0f ) < 0.5f / HEADSCALE_UNIT )	// 0, below 0, NaN, 1
		return 1.0f;
	if ( flScale < HEADSCALE_MIN )
		return HEADSCALE_MIN;
	if ( flScale > HEADSCALE_MAX )
		return HEADSCALE_MAX;
	// the clients get it in 1/256 steps: the server uses the same value
	return (float)(int)( flScale * HEADSCALE_UNIT + 0.5f ) / HEADSCALE_UNIT;
}

static float HeadScale_Of( CBasePlayer *pPlayer )
{
	return ( pPlayer && pPlayer->m_flHeadScale > 0.0f ) ? pPlayer->m_flHeadScale : 1.0f;
}

float HeadScale_ForEdict( const edict_t *pEdict )
{
	if ( !pEdict || !( pEdict->v.flags & FL_CLIENT ) || !pEdict->pvPrivateData )
		return 1.0f;

	return HeadScale_Of( (CBasePlayer *)pEdict->pvPrivateData );
}

static void HeadScale_Write( CBasePlayer *pPlayer )
{
	WRITE_BYTE( pPlayer->entindex() );
	WRITE_SHORT( (int)( HeadScale_Of( pPlayer ) * HEADSCALE_UNIT + 0.5f ) );
}

void CBasePlayer::SetHeadScale( float flScale )
{
	flScale = HeadScale_Clamp( flScale );
	if ( flScale == HeadScale_Of( this ) )
		return;

	m_flHeadScale = flScale;

	if ( gmsgHeadScale )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgHeadScale );
			HeadScale_Write( this );
		MESSAGE_END();
	}

	// the engine's hitbox cache, and the absmin / absmax around the head (SetObjectCollisionBox, through the relink)
	UTIL_StudioHitboxesChanged( pev, flScale );
	UTIL_SetOrigin( pev, pev->origin );
}

void CBasePlayer::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "headscale" ) )
	{
		// a connected player's slot only: a "player" entity in a map's entity list gets nothing
		int iIndex = ENTINDEX( edict() );
		if ( iIndex >= 1 && iIndex <= gpGlobals->maxClients )
			SetHeadScale( atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
		return;
	}

	CBaseMonster::KeyValue( pkvd );
}

void CBasePlayer::SetObjectCollisionBox( void )
{
	CBaseMonster::SetObjectCollisionBox();

	// a bigger head reaches out of the box by as much farther as it is bigger: traces must reach it (the engine
	// looks at the hitboxes only inside absmin / absmax). Up and to the sides; it never reaches below the neck
	float flHead = HeadScale_Of( this );
	if ( flHead > 1.0f )
	{
		float flExtra = ( flHead - 1.0f ) * HEADSCALE_REACH;
		pev->absmin.x -= flExtra;
		pev->absmin.y -= flExtra;
		pev->absmax.x += flExtra;
		pev->absmax.y += flExtra;
		pev->absmax.z += flExtra;
	}
}

// the send queue (client_sendqueue.h, SENDQUEUE_HEADSCALE): every player's head scale other than 1, from iCursor on
static int HeadScale_SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	for ( int i = max( iCursor, 1 ); i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pOther = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pOther || HeadScale_Of( pOther ) == 1.0f )
			continue;
		if ( HEADSCALE_MSG_BYTES > iBudget )
		{
			iCursor = i;
			return FALSE;
		}
		MESSAGE_BEGIN( MSG_ONE, gmsgHeadScale, NULL, pPlayer->edict() );
			HeadScale_Write( pOther );
		MESSAGE_END();
		iBudget -= HEADSCALE_MSG_BYTES;
	}
	return TRUE;
}

static SendQueue_Sender s_HeadScaleSender( SENDQUEUE_HEADSCALE, HeadScale_SendQueued, 1, HEADSCALE_QUEUE_DELAY,
	HEADSCALE_QUEUE_INTERVAL, HEADSCALE_QUEUE_BYTES );
