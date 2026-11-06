/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"

#include "usercmd.h"
#include "entity_state.h"
#include "demo_api.h"
#include "pm_defs.h"
#include "pm_shared.h"	// SBO 2.0: PLAYER_SUPPRESS_*, PM_GetSuppressedCapabilities
#include "event_api.h"
#include "r_efx.h"

#include "../hud_iface.h"
#include "../com_weapons.h"
#include "../demo.h"

extern globalvars_t *gpGlobals;
extern int g_iUser1;

// Pool of client side entities/entvars_t
static entvars_t	ev[ MAX_WEAPONS + 1 ];	// halflife-updated (#97): every weapon + the local player
static int			num_ents = 0;

// The entity we'll use to represent the local client
static CBasePlayer	player;

// Local version of game .dll global variables ( time, etc. )
static globalvars_t	Globals; 

static CBasePlayerWeapon *g_pWpns[ 32 ];

// SBO's client has no g_flApplyVel / g_irunninggausspred (gauss prediction; not in client.dll's .bss)


vec3_t previousorigin;

// HLDM Weapon placeholder entities.
CRpg g_Rpg;
CHandGrenade g_HandGren;
CSatchel g_Satchel;
CTripmine g_Tripmine;
CSqueak g_Snark;
CWong g_Wong;
CMedikit g_Medikit;
CRc28 g_Rc28;
CK444 g_K444;
CCanon g_Canon;
CPm66 g_Pm66;
CFa18 g_Fa18;
CFederal g_Federal;
CNm33 g_Nm33;
CSpa g_Spa;
CBen g_Ben;
CGlauncher g_Glauncher;
CChaingun g_Chaingun;
CSw g_Sw;


/*
======================
AlertMessage

Print debug messages to console
======================
*/
void AlertMessage( ALERT_TYPE atype, char *szFmt, ... )
{
	va_list		argptr;
	static char	string[1024];
	
	va_start (argptr, szFmt);
	vsprintf (string, szFmt,argptr);
	va_end (argptr);

	gEngfuncs.Con_Printf( "cl:  " );
	gEngfuncs.Con_Printf( string );
}

//Returns if it's multiplayer.
//Mostly used by the client side weapons.
bool bIsMultiplayer ( void )
{
	// SBO: every game mode has multiplayer rules (the server's g_pGameRules->IsMultiplayer() is always true), also
	// in a one-player session ("maxplayers 1"), so the weapon prediction never takes the single-player branches
	return true;
}
//Just loads a v_ model.
void LoadVModel ( char *szViewModel, CBasePlayer *m_pPlayer )
{
	gEngfuncs.CL_LoadModel( szViewModel, &m_pPlayer->pev->viewmodel );
}

/*
=====================
HUD_PrepEntity

Links the raw entity to an entvars_s holder.  If a player is passed in as the owner, then
we set up the m_pPlayer field.
=====================
*/
void HUD_PrepEntity( CBaseEntity *pEntity, CBasePlayer *pWeaponOwner )
{
	memset( &ev[ num_ents ], 0, sizeof( entvars_t ) );
	pEntity->pev = &ev[ num_ents++ ];

	pEntity->Precache();
	pEntity->Spawn();

	if ( pWeaponOwner )
	{
		ItemInfo info;
		memset( &info, 0, sizeof( info ) );	// halflife-updated
		
		((CBasePlayerWeapon *)pEntity)->m_pPlayer = pWeaponOwner;
		
		((CBasePlayerWeapon *)pEntity)->GetItemInfo( &info );

		if ( info.iId >= 0 && info.iId < MAX_WEAPONS )	// Robustness: g_pWpns has MAX_WEAPONS slots
			g_pWpns[ info.iId ] = (CBasePlayerWeapon *)pEntity;
	}
}

/*
=====================
CBaseEntity :: Killed

If weapons code "kills" an entity, just set its effects to EF_NODRAW
=====================
*/
void CBaseEntity :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->effects |= EF_NODRAW;
}

// sv_infinite_ammo as the server sent it in the "ia" physinfo key (CBasePlayer::Spawn, StartFrame); read for
// every predicted frame in HUD_WeaponsPostThink
static BOOL g_fInfiniteAmmo = FALSE;

BOOL InfiniteAmmo( void )
{
	return g_fInfiniteAmmo;
}

/*
=====================
CBasePlayerWeapon :: DefaultReload
=====================
*/
BOOL CBasePlayerWeapon :: DefaultReload( int iClipSize, int iAnim, float fDelay, int body )
{
	// sv_infinite_ammo: nothing to reload (as on the server)
	if ( InfiniteAmmo() )
		return FALSE;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return FALSE;

	int j = min(iClipSize - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);	

	if (j == 0)
		return FALSE;

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + fDelay;

	//!!UNDONE -- reload sound goes here !!!
	SendWeaponAnim( iAnim, UseDecrement(), body );

	m_fInReload = TRUE;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
	return TRUE;
}

/*
=====================
CBasePlayerWeapon :: CanDeploy
=====================
*/
BOOL CBasePlayerWeapon :: CanDeploy( void ) 
{
	BOOL bHasAmmo = 0;

	if ( !pszAmmo1() )
	{
		// this weapon doesn't use ammo, can always deploy.
		return TRUE;
	}

	if ( pszAmmo1() )
	{
		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);
	}
	if ( pszAmmo2() )
	{
		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] != 0);
	}
	if (m_iClip > 0)
	{
		bHasAmmo |= 1;
	}
	if (!bHasAmmo)
	{
		return FALSE;
	}

	return TRUE;
}

/*
=====================
CBasePlayerWeapon :: DefaultDeploy

=====================
*/
BOOL CBasePlayerWeapon :: DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal, int	body )
{
	if ( !CanDeploy() )
		return FALSE;

	gEngfuncs.CL_LoadModel( szViewModel, &m_pPlayer->pev->viewmodel );
	
	SendWeaponAnim( iAnim, skiplocal, body );

	m_pPlayer->m_flNextAttack = 0.5;
	m_flTimeWeaponIdle = 1.0;
	return TRUE;
}

/*
=====================
CBasePlayerWeapon :: PlayEmptySound

=====================
*/
BOOL CBasePlayerWeapon :: PlayEmptySound( void )
{
	if (m_iPlayEmptySound)
	{
		HUD_PlaySound( "weapons/357_cock1.wav", 0.8 );
		m_iPlayEmptySound = 0;
		return 0;
	}
	return 0;
}

/*
=====================
CBasePlayerWeapon :: ResetEmptySound

=====================
*/
void CBasePlayerWeapon :: ResetEmptySound( void )
{
	m_iPlayEmptySound = 1;
}

/*
=====================
CBasePlayerWeapon::Holster

Put away weapon
=====================
*/
void CBasePlayerWeapon::Holster( int skiplocal /* = 0 */ )
{ 
	m_fInReload = FALSE; // cancel any reload in progress.
	m_pPlayer->pev->viewmodel = 0; 
}

/*
=====================
CBasePlayerWeapon::SendWeaponAnim

Animate weapon model
=====================
*/
void CBasePlayerWeapon::SendWeaponAnim( int iAnim, int skiplocal, int body )
{
	m_pPlayer->pev->weaponanim = iAnim;
	
	HUD_SendWeaponAnim( iAnim, body, 0 );
}

/*
=====================
CBaseEntity::FireBulletsPlayer

Only produces random numbers to match the server ones.
=====================
*/
Vector CBaseEntity::FireBulletsPlayer ( ULONG cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t *pevAttacker, int shared_rand )
{
	float x, y, z;

	for ( ULONG iShot = 1; iShot <= cShots; iShot++ )
	{
		if ( pevAttacker == NULL )
		{
			// get circular gaussian spread
			do {
					x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
					y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
					z = x*x+y*y;
			} while (z > 1);
		}
		else
		{
			//Use player's random seed.
			// get circular gaussian spread
			x = UTIL_SharedRandomFloat( shared_rand + iShot, -0.5, 0.5 ) + UTIL_SharedRandomFloat( shared_rand + ( 1 + iShot ) , -0.5, 0.5 );
			y = UTIL_SharedRandomFloat( shared_rand + ( 2 + iShot ), -0.5, 0.5 ) + UTIL_SharedRandomFloat( shared_rand + ( 3 + iShot ), -0.5, 0.5 );
			z = x * x + y * y;
		}
			
	}

    return Vector ( x * vecSpread.x, y * vecSpread.y, 0.0 );
}

/*
=====================
CBasePlayerWeapon::ItemPostFrame

Handles weapon firing, reloading, etc.
=====================
*/
void CBasePlayerWeapon::ItemPostFrame( void )
{
	if ((m_fInReload) && (m_pPlayer->m_flNextAttack <= 0.0))
	{
#if 0 // FIXME, need ammo on client to make this work right
		// complete the reload. 
		int j = min( iMaxClip() - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);	

		// Add them to the clip
		m_iClip += j;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= j;
#else	
		m_iClip += 10;
#endif
		m_fInReload = FALSE;
	}

	// SBO 2.0: a frozen player (trigger_playerfreeze, a trigger_camera that takes control: FL_FROZEN) neither
	// attacks nor reloads, on the server and in the client's prediction alike (Featureful weapons_shared.cpp)
	const BOOL fFrozen = FBitSet( m_pPlayer->pev->flags, FL_FROZEN ) ? TRUE : FALSE;

	// halflife-updated (halflife #1621): the server's CanAttack test (weapons.cpp), millisecond resolution
	if ((m_pPlayer->pev->button & IN_ATTACK2) && ((static_cast<int>(floor(m_flNextSecondaryAttack * 1000.0)) * 1000.0) <= 0.0))
	{
		if ( pszAmmo2() && !m_pPlayer->m_rgAmmo[SecondaryAmmoIndex()] )
		{
			m_fFireOnEmpty = TRUE;
		}

		if ( !fFrozen )
			SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ((m_pPlayer->pev->button & IN_ATTACK) && ((static_cast<int>(floor(m_flNextPrimaryAttack * 1000.0)) * 1000.0) <= 0.0))
	{
		// no ammo type (melee, medikit: PrimaryAmmoIndex() -1): no m_rgAmmo[-1] read; only the python reads m_fFireOnEmpty
		if ( (m_iClip == 0 && pszAmmo1()) || (iMaxClip() == -1 && pszAmmo1() && !m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] ) )
		{
			m_fFireOnEmpty = TRUE;
		}

		if ( !fFrozen )
			PrimaryAttack();
	}
	else if ( m_pPlayer->pev->button & IN_RELOAD && iMaxClip() != WEAPON_NOCLIP && !m_fInReload && !fFrozen ) 
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else if ( !(m_pPlayer->pev->button & (IN_ATTACK|IN_ATTACK2) ) )
	{
		// no fire buttons down

		m_fFireOnEmpty = FALSE;

		// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
		if ( m_iClip == 0 && !(iFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < 0.0 )
		{
			Reload();
			return;
		}

		WeaponIdle( );
		return;
	}
	
	// catch all
	if ( ShouldWeaponIdle() )
	{
		WeaponIdle();
	}
}

/*
=====================
CBasePlayer::SelectItem

  Switch weapons
=====================
*/
void CBasePlayer::SelectItem(const char *pstr)
{
	if (!pstr)
		return;

	CBasePlayerItem *pItem = NULL;

	if (!pItem)
		return;

	
	if (pItem == m_pActiveItem)
		return;

	if (m_pActiveItem)
		m_pActiveItem->Holster( );
	
	m_pLastItem = m_pActiveItem;
	m_pActiveItem = pItem;

	if (m_pActiveItem)
	{
		m_pActiveItem->Deploy( );
	}
}

/*
=====================
CBasePlayer::SelectLastItem

=====================
*/
void CBasePlayer::SelectLastItem(void)
{
	if (!m_pLastItem)
	{
		return;
	}

	if ( m_pActiveItem && !m_pActiveItem->CanHolster() )
	{
		return;
	}

	if (m_pActiveItem)
		m_pActiveItem->Holster( );
	
	CBasePlayerItem *pTemp = m_pActiveItem;
	m_pActiveItem = m_pLastItem;
	m_pLastItem = pTemp;
	m_pActiveItem->Deploy( );
}

/*
=====================
CBasePlayer::Killed

=====================
*/
void CBasePlayer::Killed( entvars_t *pevAttacker, int iGib )
{
	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
		 m_pActiveItem->Holster( );
}

/*
=====================
CBasePlayer::Spawn

=====================
*/
void CBasePlayer::Spawn( void )
{
	if (m_pActiveItem)
		m_pActiveItem->Deploy( );
}

/*
=====================
UTIL_TraceLine

Don't actually trace, but act like the trace didn't hit anything.
=====================
*/
void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr )
{
	memset( ptr, 0, sizeof( *ptr ) );
	ptr->flFraction = 1.0;
}

/*
=====================
UTIL_ParticleBox

For debugging, draw a box around a player made out of particles
=====================
*/
void UTIL_ParticleBox( CBasePlayer *player, float *mins, float *maxs, float life, unsigned char r, unsigned char g, unsigned char b )
{
	int i;
	vec3_t mmin, mmax;

	for ( i = 0; i < 3; i++ )
	{
		mmin[ i ] = player->pev->origin[ i ] + mins[ i ];
		mmax[ i ] = player->pev->origin[ i ] + maxs[ i ];
	}

	gEngfuncs.pEfxAPI->R_ParticleBox( (float *)&mmin, (float *)&mmax, 5.0, 0, 255, 0 );
}

/*
=====================
UTIL_ParticleBoxes

For debugging, draw boxes for other collidable players
=====================
*/
void UTIL_ParticleBoxes( void )
{
	int idx;
	physent_t *pe;
	cl_entity_t *player;
	vec3_t mins, maxs;
	
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	player = gEngfuncs.GetLocalPlayer();
	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( player->index - 1 );	

	for ( idx = 1; idx < 100; idx++ )
	{
		pe = gEngfuncs.pEventAPI->EV_GetPhysent( idx );
		if ( !pe )
			break;

		if ( pe->info >= 1 && pe->info <= gEngfuncs.GetMaxClients() )
		{
			mins = pe->origin + pe->mins;
			maxs = pe->origin + pe->maxs;

			gEngfuncs.pEfxAPI->R_ParticleBox( (float *)&mins, (float *)&maxs, 0, 0, 255, 2.0 );
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

/*
=====================
UTIL_ParticleLine

For debugging, draw a line made out of particles
=====================
*/
void UTIL_ParticleLine( CBasePlayer *player, float *start, float *end, float life, unsigned char r, unsigned char g, unsigned char b )
{
	gEngfuncs.pEfxAPI->R_ParticleLine( start, end, r, g, b, life );
}

/*
=====================
CBasePlayerWeapon::PrintState

For debugging, print out state variables to log file
=====================
*/
void CBasePlayerWeapon::PrintState( void )
{
	COM_Log( "c:\\hl.log", "%.4f ", gpGlobals->time );
	COM_Log( "c:\\hl.log", "%.4f ", m_pPlayer->m_flNextAttack );
	COM_Log( "c:\\hl.log", "%.4f ", m_flNextPrimaryAttack );
	COM_Log( "c:\\hl.log", "%.4f ", m_flTimeWeaponIdle - gpGlobals->time);
	COM_Log( "c:\\hl.log", "%i ", m_iClip );
}

/*
=====================
HUD_InitClientWeapons

Set up weapons, player and functions needed to run weapons code client-side.
=====================
*/
void HUD_InitClientWeapons( void )
{
	static int initialized = 0;
	if ( initialized )
		return;

	initialized = 1;

	// Set up pointer ( dummy object )
	gpGlobals = &Globals;

	// Fill in current time ( probably not needed )
	gpGlobals->time = gEngfuncs.GetClientTime();

	// Fake functions
	g_engfuncs.pfnPrecacheModel		= stub_PrecacheModel;
	g_engfuncs.pfnPrecacheSound		= stub_PrecacheSound;
	g_engfuncs.pfnPrecacheEvent		= stub_PrecacheEvent;
	g_engfuncs.pfnNameForFunction	= stub_NameForFunction;
	g_engfuncs.pfnSetModel			= stub_SetModel;
	g_engfuncs.pfnSetClientMaxspeed = HUD_SetMaxSpeed;

	// Handled locally
	g_engfuncs.pfnPlaybackEvent		= HUD_PlaybackEvent;
	g_engfuncs.pfnAlertMessage		= AlertMessage;

	// Pass through to engine
	g_engfuncs.pfnPrecacheEvent		= gEngfuncs.pfnPrecacheEvent;
	g_engfuncs.pfnRandomFloat		= gEngfuncs.pfnRandomFloat;
	g_engfuncs.pfnRandomLong		= gEngfuncs.pfnRandomLong;
	// HL25/halflife-updated: CVAR_GET_* in shared weapons code
	g_engfuncs.pfnCVarGetPointer	= gEngfuncs.pfnGetCvarPointer;
	g_engfuncs.pfnCVarGetString		= (const char *(*)( const char * ))gEngfuncs.pfnGetCvarString;
	g_engfuncs.pfnCVarGetFloat		= (float (*)( const char * ))gEngfuncs.pfnGetCvarFloat;

	// Allocate a slot for the local player
	HUD_PrepEntity( &player		, NULL );

	// Allocate slot(s) for each weapon that we are going to be predicting
	HUD_PrepEntity( &g_Rpg	, &player );
	HUD_PrepEntity( &g_HandGren	, &player );
	HUD_PrepEntity( &g_Satchel	, &player );
	HUD_PrepEntity( &g_Tripmine	, &player );
	HUD_PrepEntity( &g_Snark	, &player );
	HUD_PrepEntity( &g_Medikit	, &player );
	HUD_PrepEntity( &g_Rc28	, &player );
	HUD_PrepEntity( &g_K444	, &player );
	HUD_PrepEntity( &g_Canon	, &player );
	HUD_PrepEntity( &g_Pm66	, &player );
	HUD_PrepEntity( &g_Fa18	, &player );
	HUD_PrepEntity( &g_Federal	, &player );
	HUD_PrepEntity( &g_Nm33	, &player );
	HUD_PrepEntity( &g_Spa	, &player );
	HUD_PrepEntity( &g_Ben	, &player );
	HUD_PrepEntity( &g_Chaingun	, &player );
	HUD_PrepEntity( &g_Glauncher	, &player );
	HUD_PrepEntity( &g_Wong	, &player );
	HUD_PrepEntity( &g_Sw	, &player );
}

/*
=====================
HUD_GetLastOrg

Retruns the last position that we stored for egon beam endpoint.
=====================
*/
void HUD_GetLastOrg( float *org )
{
	int i;
	
	// Return last origin
	for ( i = 0; i < 3; i++ )
	{
		org[i] = previousorigin[i];
	}
}

/*
=====================
HUD_SetLastOrg

Remember our exact predicted origin so we can draw the egon to the right position.
=====================
*/
void HUD_SetLastOrg( void )
{
	int i;
	
	// Offset final origin by view_offset
	for ( i = 0; i < 3; i++ )
	{
		previousorigin[i] = g_finalstate->playerstate.origin[i] + g_finalstate->client.view_ofs[ i ];
	}
}

/*
=====================
HUD_WeaponsPostThink

Run Weapon firing code on client
=====================
*/
void HUD_WeaponsPostThink( local_state_s *from, local_state_s *to, usercmd_t *cmd, double time, unsigned int random_seed )
{
	// SBO layout: under /YX, VC6 emits pending inline functions after each function in proportion to that
	// function's per-line scope/declaration bookkeeping. SBO's original body evidently had more of it; these 70
	// empty scopes (one per line; no code is generated) reproduce the original object layout. They come after
	// the declarations: lasthealth must stay in scope 2 (its decorated name sets its .bss slot, NOTES.md CL2).
	// See NOTES.md "Layout matching".
	int i;
	int buttonsChanged;
	CBasePlayerWeapon *pWeapon = NULL;
	CBasePlayerWeapon *pCurrent;
	weapon_data_t nulldata, *pfrom, *pto;
	static int lasthealth;

	memset( &nulldata, 0, sizeof( nulldata ) );

	HUD_InitClientWeapons();	

	// Get current clock
	gpGlobals->time = time;

	const char *pszInfiniteAmmo = gEngfuncs.PhysInfo_ValueForKey( "ia" );
	g_fInfiniteAmmo = ( pszInfiniteAmmo && atoi( pszInfiniteAmmo ) != 0 ) ? TRUE : FALSE;

	// Fill in data based on selected weapon
	// FIXME, make this a method in each weapon?  where you pass in an entity_state_t *?
	switch ( from->client.m_iId )
	{
		case WEAPON_RPG:
			pWeapon = &g_Rpg;
			break;

		case WEAPON_HANDGRENADE:
			pWeapon = &g_HandGren;
			break;

		case WEAPON_SATCHEL:
			pWeapon = &g_Satchel;
			break;

		case WEAPON_TRIPMINE:
			pWeapon = &g_Tripmine;
			break;

		case WEAPON_SNARK:
			pWeapon = &g_Snark;
			break;

		case WEAPON_RC28:
			pWeapon = &g_Rc28;
			break;

		case WEAPON_K444:
			pWeapon = &g_K444;
			break;

		case WEAPON_CANON:
			pWeapon = &g_Canon;
			break;

		case WEAPON_PM66:
			pWeapon = &g_Pm66;
			break;

		case WEAPON_FA18:
			pWeapon = &g_Fa18;
			break;

		case WEAPON_FEDERAL:
			pWeapon = &g_Federal;
			break;

		case WEAPON_MEDIKIT:
			pWeapon = &g_Medikit;
			break;

		case WEAPON_SPA:
			pWeapon = &g_Spa;
			break;

		case WEAPON_BEN:
			pWeapon = &g_Ben;
			break;

		case WEAPON_NM33:
			pWeapon = &g_Nm33;
			break;

		case WEAPON_CHAINGUN:
			pWeapon = &g_Chaingun;
			break;

		case WEAPON_GLAUNCHER:
			pWeapon = &g_Glauncher;
			break;

		case WEAPON_WONG:
			pWeapon = &g_Wong;
			break;

		case WEAPON_SW:
			pWeapon = &g_Sw;
			break;
	}

	// Store pointer to our destination entity_state_t so we can get our origin, etc. from it
	//  for setting up events on the client
	g_finalstate = to;

	// If we are running events/etc. go ahead and see if we
	//  managed to die between last frame and this one
	// If so, run the appropriate player killed or spawn function
	if ( g_runfuncs )
	{
		if ( to->client.health <= 0 && lasthealth > 0 )
		{
			player.Killed( NULL, 0 );
			
		}
		else if ( to->client.health > 0 && lasthealth <= 0 )
		{
			player.Spawn();
		}

		lasthealth = to->client.health;
	}

	// We are not predicting the current weapon, just bow out here.
	if ( !pWeapon )
		return;

	for ( i = 0; i < 32; i++ )
	{
		pCurrent = g_pWpns[ i ];
		if ( !pCurrent )
		{
			continue;
		}

		pfrom = &from->weapondata[ i ];
		
		pCurrent->m_fInReload			= pfrom->m_fInReload;
		pCurrent->m_fInSpecialReload	= pfrom->m_fInSpecialReload;
//		pCurrent->m_flPumpTime			= pfrom->m_flPumpTime;
		pCurrent->m_iClip				= pfrom->m_iClip;
		pCurrent->m_flNextPrimaryAttack	= pfrom->m_flNextPrimaryAttack;
		pCurrent->m_flNextSecondaryAttack = pfrom->m_flNextSecondaryAttack;
		pCurrent->m_flTimeWeaponIdle	= pfrom->m_flTimeWeaponIdle;
		pCurrent->pev->fuser1			= pfrom->fuser1;
		pCurrent->m_flStartThrow		= pfrom->fuser2;
		pCurrent->m_flReleaseThrow		= pfrom->fuser3;
		pCurrent->m_chargeReady			= pfrom->iuser1;
		pCurrent->m_fInAttack			= pfrom->iuser2;
		pCurrent->m_fireState			= pfrom->iuser3;

		pCurrent->m_iSecondaryAmmoType		= (int)from->client.vuser3[ 2 ];
		pCurrent->m_iPrimaryAmmoType		= (int)from->client.vuser4[ 0 ];
		// Robustness: weapons without ammo have ammo type -1 (wrote before m_rgAmmo)
		if ( pCurrent->m_iPrimaryAmmoType >= 0 && pCurrent->m_iPrimaryAmmoType < MAX_AMMO_SLOTS )
			player.m_rgAmmo[ pCurrent->m_iPrimaryAmmoType ]	= (int)from->client.vuser4[ 1 ];
		if ( pCurrent->m_iSecondaryAmmoType >= 0 && pCurrent->m_iSecondaryAmmoType < MAX_AMMO_SLOTS )
			player.m_rgAmmo[ pCurrent->m_iSecondaryAmmoType ]	= (int)from->client.vuser4[ 2 ];
	}

	// For random weapon events, use this seed to seed random # generator
	player.random_seed = random_seed;

	// Get old buttons from previous state.
	player.m_afButtonLast = from->playerstate.oldbuttons;

	// SBO 2.0: player_capabilities "Attack" off (the Capability message): the attack buttons do nothing, as on the
	// server (CBasePlayer::PreThink)
	int iButtons = cmd->buttons;
	if ( PM_GetSuppressedCapabilities( 0 ) & PLAYER_SUPPRESS_ATTACK )
		iButtons &= ~( IN_ATTACK | IN_ATTACK2 );

	// Which buttsons chave changed
	buttonsChanged = (player.m_afButtonLast ^ iButtons);	// These buttons have changed this frame
	
	// Debounced button codes for pressed/released
	// The changed ones still down are "pressed"
	player.m_afButtonPressed =  buttonsChanged & iButtons;	
	// The ones not down are "released"
	player.m_afButtonReleased = buttonsChanged & (~iButtons);

	// HL25: weapons code that traces from the player needs the current aim and origin
	player.pev->v_angle = cmd->viewangles;
	player.pev->origin = from->client.origin;

	// Set player variables that weapons code might check/alter
	player.pev->button = iButtons;	// SBO 2.0: cmd->buttons without suppressed attack buttons

	player.pev->velocity = from->client.velocity;
	player.pev->flags = from->client.flags;

	player.pev->deadflag = from->client.deadflag;
	player.pev->waterlevel = from->client.waterlevel;
	player.pev->maxspeed    = from->client.maxspeed;
	player.pev->fov = from->client.fov;
	player.pev->weaponanim = from->client.weaponanim;
	player.pev->viewmodel = from->client.viewmodel;
	player.m_flNextAttack = from->client.m_flNextAttack;
	player.m_flNextAmmoBurn = from->client.fuser2;
	player.m_flAmmoStartCharge = from->client.fuser3;

	//Stores all our ammo info, so the client side weapons can use them.
	player.ammo_rockets		= (int)from->client.ammo_rockets;

	
	// Point to current weapon object
	if ( from->client.m_iId )
	{
		player.m_pActiveItem = g_pWpns[ from->client.m_iId ];
	}

	if ( player.m_pActiveItem->m_iId == WEAPON_RPG )
	{
		 ( ( CRpg * )player.m_pActiveItem)->m_fSpotActive = (int)from->client.vuser2[ 1 ];
		 ( ( CRpg * )player.m_pActiveItem)->m_cActiveRockets = (int)from->client.vuser2[ 2 ];
	}
	
	// Don't go firing anything if we have died or are spectating (halflife SDK 2.3: in first person the server
	// sends the watched player's weapon state, which must not fire here)
	// Or if we don't have a weapon model deployed
	if ( ( player.pev->deadflag != ( DEAD_DISCARDBODY + 1 ) ) && 
		 !CL_IsDead() && player.pev->viewmodel && !g_iUser1 )
	{
		if ( player.m_flNextAttack <= 0 )
		{
			// sv_infinite_ammo: the server puts fired rounds back into the clip at once (CBasePlayer::ItemPostFrame),
			// so the predicted clip must not run down either
			int iClip = pWeapon->m_iClip;

			pWeapon->ItemPostFrame();

			if ( InfiniteAmmo() )
				pWeapon->m_iClip = iClip;
		}
	}

	// Assume that we are not going to switch weapons
	to->client.m_iId					= from->client.m_iId;

	// Now see if we issued a changeweapon command ( and we're not dead )
	if ( cmd->weaponselect && ( player.pev->deadflag != ( DEAD_DISCARDBODY + 1 ) ) )
	{
		// Switched to a different weapon?
		if ( from->weapondata[ cmd->weaponselect ].m_iId == cmd->weaponselect )
		{
			CBasePlayerWeapon *pNew = g_pWpns[ cmd->weaponselect ];
			if ( pNew && ( pNew != pWeapon ) )
			{
				// Put away old weapon
				if (player.m_pActiveItem)
					player.m_pActiveItem->Holster( );
				
				player.m_pLastItem = player.m_pActiveItem;
				player.m_pActiveItem = pNew;

				// Deploy new weapon
				if (player.m_pActiveItem)
				{
					player.m_pActiveItem->Deploy( );
				}

				// Update weapon id so we can predict things correctly.
				to->client.m_iId = cmd->weaponselect;
			}
		}
	}

	// Copy in results of prediction code
	to->client.viewmodel				= player.pev->viewmodel;
	to->client.fov						= player.pev->fov;
	to->client.weaponanim				= player.pev->weaponanim;
	to->client.m_flNextAttack			= player.m_flNextAttack;
	to->client.fuser2					= player.m_flNextAmmoBurn;
	to->client.fuser3					= player.m_flAmmoStartCharge;
	to->client.maxspeed					= player.pev->maxspeed;

	//HL Weapons
	to->client.ammo_rockets				= player.ammo_rockets;

	if ( player.m_pActiveItem->m_iId == WEAPON_RPG )
	{
		// Valve halflife PR #3165 / halflife-updated: prediction results go to 'to', not 'from'
		 to->client.vuser2[ 1 ] = ( ( CRpg * )player.m_pActiveItem)->m_fSpotActive;
		 to->client.vuser2[ 2 ] = ( ( CRpg * )player.m_pActiveItem)->m_cActiveRockets;
	}

	// Make sure that weapon animation matches what the game .dll is telling us
	//  over the wire ( fixes some animation glitches )
	if ( g_runfuncs && ( HUD_GetWeaponAnim() != to->client.weaponanim ) )
	{
		int body = 2;

		//Pop the model to body 0.
		if ( pWeapon == &g_Tripmine )
			 body = 0;

		// Force a fixed anim down to viewmodel
		HUD_SendWeaponAnim( to->client.weaponanim, body, 1 );
	}

	for ( i = 0; i < 32; i++ )
	{
		pCurrent = g_pWpns[ i ];

		pto = &to->weapondata[ i ];

		if ( !pCurrent )
		{
			memset( pto, 0, sizeof( weapon_data_t ) );
			continue;
		}
	
		pto->m_fInReload				= pCurrent->m_fInReload;
		pto->m_fInSpecialReload			= pCurrent->m_fInSpecialReload;
//		pto->m_flPumpTime				= pCurrent->m_flPumpTime;
		pto->m_iClip					= pCurrent->m_iClip; 
		pto->m_flNextPrimaryAttack		= pCurrent->m_flNextPrimaryAttack;
		pto->m_flNextSecondaryAttack	= pCurrent->m_flNextSecondaryAttack;
		pto->m_flTimeWeaponIdle			= pCurrent->m_flTimeWeaponIdle;
		pto->fuser1						= pCurrent->pev->fuser1;
		pto->fuser2						= pCurrent->m_flStartThrow;
		pto->fuser3						= pCurrent->m_flReleaseThrow;
		pto->iuser1						= pCurrent->m_chargeReady;
		pto->iuser2						= pCurrent->m_fInAttack;
		pto->iuser3						= pCurrent->m_fireState;

		// Decrement weapon counters, server does this at same time ( during post think, after doing everything else )
		pto->m_flNextReload				-= cmd->msec / 1000.0;
		pto->m_fNextAimBonus			-= cmd->msec / 1000.0;
		pto->m_flNextPrimaryAttack		-= cmd->msec / 1000.0;
		pto->m_flNextSecondaryAttack	-= cmd->msec / 1000.0;
		pto->m_flTimeWeaponIdle			-= cmd->msec / 1000.0;
		pto->fuser1						-= cmd->msec / 1000.0;

		to->client.vuser3[2]				= pCurrent->m_iSecondaryAmmoType;
		to->client.vuser4[0]				= pCurrent->m_iPrimaryAmmoType;
		to->client.vuser4[1]				= ( pCurrent->m_iPrimaryAmmoType >= 0 && pCurrent->m_iPrimaryAmmoType < MAX_AMMO_SLOTS ) ? player.m_rgAmmo[ pCurrent->m_iPrimaryAmmoType ] : 0;
		to->client.vuser4[2]				= ( pCurrent->m_iSecondaryAmmoType >= 0 && pCurrent->m_iSecondaryAmmoType < MAX_AMMO_SLOTS ) ? player.m_rgAmmo[ pCurrent->m_iSecondaryAmmoType ] : 0;

/*		if ( pto->m_flPumpTime != -9999 )
		{
			pto->m_flPumpTime -= cmd->msec / 1000.0;
			if ( pto->m_flPumpTime < -0.001 )
				pto->m_flPumpTime = -0.001;
		}*/

		if ( pto->m_fNextAimBonus < -1.0 )
		{
			pto->m_fNextAimBonus = -1.0;
		}

		if ( pto->m_flNextPrimaryAttack < -1.0 )
		{
			pto->m_flNextPrimaryAttack = -1.0;
		}

		if ( pto->m_flNextSecondaryAttack < -0.001 )
		{
			pto->m_flNextSecondaryAttack = -0.001;
		}

		if ( pto->m_flTimeWeaponIdle < -0.001 )
		{
			pto->m_flTimeWeaponIdle = -0.001;
		}

		if ( pto->m_flNextReload < -0.001 )
		{
			pto->m_flNextReload = -0.001;
		}

		if ( pto->fuser1 < -0.001 )
		{
			pto->fuser1 = -0.001;
		}
	}

	// m_flNextAttack is now part of the weapons, but is part of the player instead
	to->client.m_flNextAttack -= cmd->msec / 1000.0;
	if ( to->client.m_flNextAttack < -0.001 )
	{
		to->client.m_flNextAttack = -0.001;
	}

	to->client.fuser2 -= cmd->msec / 1000.0;
	if ( to->client.fuser2 < -0.001 )
	{
		to->client.fuser2 = -0.001;
	}
	
	to->client.fuser3 -= cmd->msec / 1000.0;
	if ( to->client.fuser3 < -0.001 )
	{
		to->client.fuser3 = -0.001;
	}

	// Store off the last position from the predicted state.
	HUD_SetLastOrg();

	// Wipe it so we can't use it after this frame
	g_finalstate = NULL;
}

/*
=====================
HUD_PostRunCmd

Client calls this during prediction, after it has moved the player and updated any info changed into to->
time is the current client clock based on prediction
cmd is the command that caused the movement, etc
runfuncs is 1 if this is the first time we've predicted this command.  If so, sounds and effects should play, otherwise, they should
be ignored
=====================
*/
Vector g_vPlayerVelocity;	// cs16-client (cs_weapons.cpp): the local player's velocity, used by the weather (environment.cpp)

void _DLLEXPORT HUD_PostRunCmd( struct local_state_s *from, struct local_state_s *to, struct usercmd_s *cmd, int runfuncs, double time, unsigned int random_seed )
{
	g_runfuncs = runfuncs;
	g_vPlayerVelocity = from->client.velocity;

#if defined( CLIENT_WEAPONS )
	if ( cl_lw && cl_lw->value )
	{
		HUD_WeaponsPostThink( from, to, cmd, time, random_seed );
	}
	else
#endif
	{
		to->client.fov = g_lastFOV;
	}

	// All games can use FOV state
	g_lastFOV = to->client.fov;
}

// SBO: CBasePlayer morph powers are server-side; the client only needs empty stubs.
// Order matches SBO's code layout (FireTWalls lives in the server's combat.cpp, the rest in player.cpp).
void CBasePlayer::FireTWalls( float flTime ) { }
void CBasePlayer::Cloak( void ) { }
void CBasePlayer::Cybereye( void ) { }
// SBO: unreferenced empty free function. Original name unknown: this one is hash-tuned to SBO's incremental-link
// thunk slot (see NOTES.md "ILT order").
void StubWeaponFunction( void ) { }
