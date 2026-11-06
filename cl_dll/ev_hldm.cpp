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
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_materials.h"

#include "eventscripts.h"
#include "ev_hldm.h"

#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "in_defs.h"

#include <string.h>

#include "r_studioint.h"
#include "com_model.h"

extern engine_studio_api_t IEngineStudio;

static int tracerCount[ 32 ];

extern "C" char PM_FindTextureType( char *name );

void V_PunchAxis( int axis, float punch );
void VectorAngles( const float *forward, float *angles );

extern cvar_t *cl_lw;

extern "C"
{

// HLDM
void EV_Wong( struct event_args_s *args  );
void EV_FireRpg( struct event_args_s *args  );
void EV_TripmineFire( struct event_args_s *args  );
void EV_SnarkFire( struct event_args_s *args  );

// SBO
void EV_FireRc28_1( struct event_args_s *args );
void EV_FireRc28_2( struct event_args_s *args );
void EV_FireK444( struct event_args_s *args );
void EV_FireCanon1( struct event_args_s *args );
void EV_FireCanon2( struct event_args_s *args );
void EV_FireSpa1( struct event_args_s *args );
void EV_FireSpa2( struct event_args_s *args );
void EV_FireBen( struct event_args_s *args );
void EV_FirePm66( struct event_args_s *args );
void EV_FirePm66_2( struct event_args_s *args );
void EV_FireFederal( struct event_args_s *args );
void EV_FireFa18_1( struct event_args_s *args );
void EV_FireFa18_2( struct event_args_s *args );
void EV_FireNm33( struct event_args_s *args );
void EV_FireSw( struct event_args_s *args );
void EV_FireChaingun( struct event_args_s *args );
void EV_Medikit( struct event_args_s *args );


void EV_TrainPitchAdjust( struct event_args_s *args );
}

#define VECTOR_CONE_1DEGREES Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES Vector( 0.06105, 0.06105, 0.06105 )	
#define VECTOR_CONE_8DEGREES Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES Vector( 0.17365, 0.17365, 0.17365 )

#define SND_CHANGE_PITCH	(1<<7)		// duplicated in protocol.h change sound pitch

// SBO: short yellowish dynamic light at the muzzle
void EV_HLDM_MuzzleFlash( vec3_t origin, float flScale )
{
	dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );

	dl->origin = origin;
	dl->color.r = 255;
	dl->color.g = 255;
	dl->color.b = 128;
	dl->radius = flScale * 100;
	dl->die = gEngfuncs.GetClientTime() + 0.01;
}

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play
float EV_HLDM_PlayTextureSound( int idx, pmtrace_t *ptr, float *vecSrc, float *vecEnd, int iBulletType )
{
	// hit the world, try to play sound based on texture material type
	char chTextureType = CHAR_TEX_CONCRETE;
	float fvol;
	float fvolbar;
	char *rgsz[4];
	int cnt;
	float fattn = ATTN_NORM;
	int entity;
	char *pTextureName;
	char texname[ 64 ];
	char szbuffer[ 64 ];

	entity = gEngfuncs.pEventAPI->EV_IndexFromTrace( ptr );

	// FIXME check if playtexture sounds movevar is set
	//

	chTextureType = 0;

	// Player
	if ( entity >= 1 && entity <= gEngfuncs.GetMaxClients() )
	{
		// hit body
		chTextureType = CHAR_TEX_FLESH;
	}
	else if ( entity == 0 )
	{
		// get texture from entity or world (world is ent(0))
		pTextureName = (char *)gEngfuncs.pEventAPI->EV_TraceTexture( ptr->ent, vecSrc, vecEnd );
		
		if ( pTextureName )
		{
			strcpy( texname, pTextureName );
			pTextureName = texname;

			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
			{
				pTextureName += 2;
			}

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
			{
				pTextureName++;
			}
			
			// '}}'
			strcpy( szbuffer, pTextureName );
			szbuffer[ CBTEXTURENAMEMAX - 1 ] = 0;
				
			// get texture type
			chTextureType = PM_FindTextureType( szbuffer );	
		}
	}
	
	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE: fvol = 0.9;	fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_METAL: fvol = 0.9; fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_DIRT:	fvol = 0.9; fvolbar = 0.1;
		rgsz[0] = "player/pl_dirt1.wav";
		rgsz[1] = "player/pl_dirt2.wav";
		rgsz[2] = "player/pl_dirt3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_VENT:	fvol = 0.5; fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt = 2;
		break;
	case CHAR_TEX_GRATE: fvol = 0.9; fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt = 2;
		break;
	case CHAR_TEX_TILE:	fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_SLOSH: fvol = 0.9; fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_WOOD: fvol = 0.9; fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_FLESH:
		if (iBulletType == BULLET_PLAYER_CROWBAR)
			return 0.0; // crowbar already makes this sound
		fvol = 1.0;	fvolbar = 0.2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		fattn = 1.0;
		cnt = 2;
		break;
	}

	// play material hit sound
	gEngfuncs.pEventAPI->EV_PlaySound( 0, ptr->endpos, CHAN_STATIC, rgsz[gEngfuncs.pfnRandomLong(0,cnt-1)], fvol, fattn, 0, 96 + gEngfuncs.pfnRandomLong(0,0xf) );
	return fvolbar;
}

char *EV_HLDM_DamageDecal( physent_t *pe )
{
	static char decalname[ 32 ];
	int idx;

	if ( pe->classnumber == 1 )
	{
		idx = gEngfuncs.pfnRandomLong( 0, 2 );
		sprintf( decalname, "{break%i", idx + 1 );
	}
	else if ( pe->rendermode != kRenderNormal )
	{
		sprintf( decalname, "{bproof1" );
	}
	else
	{
		idx = gEngfuncs.pfnRandomLong( 0, 4 );
		sprintf( decalname, "{shot%i", idx + 1 );
	}
	return decalname;
}

void EV_HLDM_GunshotDecalTrace( pmtrace_t *pTrace, char *decalName )
{
	int iRand;
	physent_t *pe;

	gEngfuncs.pEfxAPI->R_BulletImpactParticles( pTrace->endpos );

	iRand = gEngfuncs.pfnRandomLong(0,0x7FFF);
	if ( iRand < (0x7fff/2) )// not every bullet makes a sound.
	{
		switch( iRand % 5)
		{
		case 0:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); break;
		case 1:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); break;
		case 2:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); break;
		case 3:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric4.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); break;
		case 4:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric5.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); break;
		}
	}

	pe = gEngfuncs.pEventAPI->EV_GetPhysent( pTrace->ent );

	// Only decal brush models such as the world etc.
	if (  decalName && decalName[0] && pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
	{
		if ( CVAR_GET_FLOAT( "r_decals" ) )
		{
			gEngfuncs.pEfxAPI->R_DecalShoot( 
				gEngfuncs.pEfxAPI->Draw_DecalIndex( gEngfuncs.pEfxAPI->Draw_DecalIndexFromName( decalName ) ), 
				gEngfuncs.pEventAPI->EV_IndexFromTrace( pTrace ), 0, pTrace->endpos, 0 );
		}
	}
}

void EV_HLDM_DecalGunshot( pmtrace_t *pTrace, int iBulletType )
{
	physent_t *pe;
	int iSprite;

	pe = gEngfuncs.pEventAPI->EV_GetPhysent( pTrace->ent );

	if ( pe && pe->solid == SOLID_BSP )
	{
		switch( iBulletType )
		{
		default:
			// smoke and decal
			EV_HLDM_GunshotDecalTrace( pTrace, EV_HLDM_DamageDecal( pe ) );
			gEngfuncs.pEfxAPI->R_SparkStreaks( pTrace->endpos, 25, -200, 200 );
			iSprite = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/stmbal1.spr" );
			gEngfuncs.pEfxAPI->R_TempSprite( pTrace->endpos, pTrace->plane.normal, 0.3, iSprite, kRenderTransAdd, kRenderFxNoDissipation, 10, 0, FTENT_SPRANIMATE );
			break;
		}
	}
}

int EV_HLDM_CheckTracer( int idx, float *vecSrc, float *end, float *forward, float *right, int iBulletType, int iTracerFreq, int *tracerCount )
{
	int tracer = 0;
	int i;
	qboolean player = idx >= 1 && idx <= gEngfuncs.GetMaxClients() ? true : false;

	if ( iTracerFreq != 0 && ( (*tracerCount)++ % iTracerFreq) == 0 )
	{
		vec3_t vecTracerSrc;

		if ( player )
		{
			vec3_t offset( 0, 0, -4 );

			// adjust tracer position for player
			for ( i = 0; i < 3; i++ )
			{
				vecTracerSrc[ i ] = vecSrc[ i ] + offset[ i ] + right[ i ] * 2 + forward[ i ] * 16;
			}
		}
		else
		{
			VectorCopy( vecSrc, vecTracerSrc );
		}
		
		if ( iTracerFreq != 1 )		// guns that always trace also always decal
			tracer = 1;

		switch( iBulletType )
		{
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_MONSTER_9MM:
		case BULLET_MONSTER_12MM:
		default:
			EV_CreateTracer( vecTracerSrc, end );
			break;
		}
	}

	return tracer;
}


/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.
================
*/
void EV_HLDM_FireBullets( int idx, float *forward, float *right, float *up, int cShots, float *vecSrc, float *vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int *tracerCount, float flSpreadX, float flSpreadY )
{
	int i;
	pmtrace_t tr;
	int iShot;
	int tracer;
	
	for ( iShot = 1; iShot <= cShots; iShot++ )	
	{
		vec3_t vecDir, vecEnd;
			
		float x, y, z;
		//We randomize for the Shotgun.
		if ( iBulletType == 15 || iBulletType == 10 || iBulletType == 13 || iBulletType == 7 || iBulletType == 14 )
		{
			do {
				x = gEngfuncs.pfnRandomFloat(-0.5,0.5) + gEngfuncs.pfnRandomFloat(-0.5,0.5);
				y = gEngfuncs.pfnRandomFloat(-0.5,0.5) + gEngfuncs.pfnRandomFloat(-0.5,0.5);
				z = x*x+y*y;
			} while (z > 1);

			for ( i = 0 ; i < 3; i++ )
			{
				vecDir[i] = vecDirShooting[i] + x * flSpreadX * right[ i ] + y * flSpreadY * up [ i ];
				vecEnd[i] = vecSrc[ i ] + flDistance * vecDir[ i ];
			}
		}//But other guns already have their spread randomized in the synched spread.
		else
		{

			for ( i = 0 ; i < 3; i++ )
			{
				vecDir[i] = vecDirShooting[i] + flSpreadX * right[ i ] + flSpreadY * up [ i ];
				vecEnd[i] = vecSrc[ i ] + flDistance * vecDir[ i ];
			}
		}

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
	
		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();
	
		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );

		tracer = EV_HLDM_CheckTracer( idx, vecSrc, tr.endpos, forward, right, iBulletType, iTracerFreq, tracerCount );

		// do damage, paint decals
		if ( tr.fraction != 1.0 )
		{
			switch(iBulletType)
			{
			default:
			case BULLET_PLAYER_9MM:		
				
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
			
					break;
			case BULLET_PLAYER_MP5:		
				
				if ( !tracer )
				{
					EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
					EV_HLDM_DecalGunshot( &tr, iBulletType );
				}
				break;
			case BULLET_PLAYER_BUCKSHOT:
				
				EV_HLDM_DecalGunshot( &tr, iBulletType );
			
				break;
			case BULLET_PLAYER_357:
				
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				
				break;
			case 6:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case 8:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case 10:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case 15:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case 14:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case 11:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case 16:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case 13:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case 12:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case 9:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case 7:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;

			}
		}

		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
}

//======================
//	    GLOCK START
//======================
//======================
//	    SBO EVENTS
//======================

void EV_FireRc28_1( event_args_t *args )
{
	// ARTIFICIAL layout filler (no code): VC6 emits pending inline functions (here vgui::Dar<T>::operator=, Border/Button::operator=) after a function in proportion to accumulated front-end work; SBO's source had more of it before this point. See NOTES.md "Client whole-file layout (CL2)".
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 2, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.3 ) + 2.0 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, 8, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 3;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -3, 3 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 2;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}


void EV_FireRc28_2( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.3 ) + 2.0 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, 8, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 3;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -3, 3 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 2;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 2;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}


void EV_FireK444( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 2, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.5, 0.5 ) + 3.0 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, 9, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 6;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 3;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 4;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}


void EV_FireCanon1( event_args_t *args )
{
	// ARTIFICIAL layout filler (no code), see EV_FireRc28_1.
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	{ }
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 0, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shotgunshell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.5 ) + 3.0 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 6, vecSrc, vecAiming, 8192, 10, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 7;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 5;
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 6;
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}

void EV_FireCanon2( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 0, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shotgunshell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.5 ) + 3.5 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 12, vecSrc, vecAiming, 8192, 10, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 10;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 7;
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 8;
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}

void EV_FireSpa1( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shotgunshell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.5 ) + 4.0 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 4, vecSrc, vecAiming, 8192, 15, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 5;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 4;
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 5;
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}

void EV_FireSpa2( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 2, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shotgunshell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.5 ) + 4.2 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 8, vecSrc, vecAiming, 8192, 15, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 8;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 6;
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 7;
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}

void EV_FireBen( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shotgunshell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.4, 0.4 ) + 4.0 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 8, vecSrc, vecAiming, 8192, 14, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 4;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 3;
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 4;
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}

void EV_FirePm66( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 3, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.15, 0.2 ) + 2.5 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, 11, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -3, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}


void EV_FirePm66_2( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 3, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.15, 0.2 ) + 2.5 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 2, vecSrc, vecAiming, 8192, 11, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -5, 4 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -4, 3 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}


void EV_FireFederal( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 3, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.3 ) + 2.4 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, 12, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 0.5;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -0.5, 0.5 );
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 0.8;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}


void EV_FireFa18_1( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 5, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.3 ) + 2.5 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, 13, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -3, 3 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 0.8;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -0.8, 0.8 );
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}


void EV_FireFa18_2( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.3 ) + 3.5 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 4, vecSrc, vecAiming, 8192, 13, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 3;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -2, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 3;
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 3;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}

void EV_FireNm33( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.3 ) + 3.0 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, 6, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 1;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -3, 2 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 0.5;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -0.5, 0.5 );
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 0.7;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}


void EV_FireSw( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 2, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, 16, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 6;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -6, 6 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 3;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -3, 3 );
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= 3;
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -3, 3 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}

void EV_FireChaingun( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 4, 2 );

		shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 12, -10, 7 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

		EV_GetGunPosition( args, vecSrc, origin );

		EV_HLDM_MuzzleFlash( vecSrc, gEngfuncs.pfnRandomFloat( -0.2, 0.3 ) + 1.5 );

		VectorCopy( forward, vecAiming );

		EV_HLDM_FireBullets( idx, forward, right, up, 3, vecSrc, vecAiming, 8192, 7, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );

		if ( gHUD.m_iKeyBits & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= gEngfuncs.pfnRandomFloat( 0, 1 );
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -3, 3 );
			gEngfuncs.SetViewAngles( viewangles );
		}

		if ( gHUD.m_iKeyBits & IN_DUCK )
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
		else
		{
			vec3_t viewangles;
			gEngfuncs.GetViewAngles( viewangles );
			viewangles[PITCH] -= gEngfuncs.pfnRandomFloat( 0, 1 );
			viewangles[YAW] -= gEngfuncs.pfnRandomFloat( -1, 1 );
			gEngfuncs.SetViewAngles( viewangles );
		}
	}
}

int g_iSwing2;	// SBO name unknown; hash-tuned: .bss order puts it after g_iSwing1 (C1XX symbol-table order)

void EV_Medikit( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 2, 1 );

		switch( (g_iSwing2++) % 3 )
		{
			case 0:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( 2, 1 ); break;
			case 1:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( 2, 1 ); break;
			case 2:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( 2, 1 ); break;
		}
	}
}

//======================
//	    SBO EVENTS END
//======================

//======================
//	   CROWBAR START
//======================

enum crowbar_e {
	CROWBAR_IDLE = 0,
	CROWBAR_DRAW,
	CROWBAR_HOLSTER,
	CROWBAR_ATTACK1HIT,
	CROWBAR_ATTACK1MISS,
	CROWBAR_ATTACK2MISS,
	CROWBAR_ATTACK2HIT,
	CROWBAR_ATTACK3MISS,
	CROWBAR_ATTACK3HIT
};

int g_iSwing1;	// SBO name unknown (not the SDK g_iSwing); hash-tuned: .bss order is after decalname

//Only predict the miss sounds, hit sounds are still played 
//server side, so players don't get the wrong idea.
void EV_Wong( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	
	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM); 

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( CROWBAR_ATTACK1MISS, 1 );

		switch( (g_iSwing1++) % 3 )
		{
			case 0:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( CROWBAR_ATTACK1MISS, 1 ); break;
			case 1:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( CROWBAR_ATTACK2MISS, 1 ); break;
			case 2:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( CROWBAR_ATTACK3MISS, 1 ); break;
		}
	}
}
//======================
//	   CROWBAR END 
//======================

//======================
//	    RPG START 
//======================
enum rpg_e {
	RPG_IDLE = 0,
	RPG_FIDGET,
	RPG_RELOAD,		// to reload
	RPG_FIRE2,		// to empty
	RPG_HOLSTER1,	// loaded
	RPG_DRAW1,		// loaded
	RPG_HOLSTER2,	// unloaded
	RPG_DRAW_UL,	// unloaded
	RPG_IDLE_UL,	// unloaded idle
	RPG_FIDGET_UL,	// unloaded fidget
};

void EV_FireRpg( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM );

	//Only play the weapon anims if I shot it. 
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( RPG_FIRE2, 1 );
	
		V_PunchAxis( 0, -5.0 );
	}
}
//======================
//	     RPG END 
//======================

//======================
//	    EGON END 
//======================
enum egon_e {
	EGON_IDLE1 = 0,
	EGON_FIDGET1,
	EGON_ALTFIREON,
	EGON_ALTFIRECYCLE,
	EGON_ALTFIREOFF,
	EGON_FIRE1,
	EGON_FIRE2,
	EGON_FIRE3,
	EGON_FIRE4,
	EGON_DRAW,
	EGON_HOLSTER
};

// SBO's client has no g_fireAnims1/g_fireAnims2 (no trace of them in client.dll's .data)

enum EGON_FIRESTATE { FIRE_OFF, FIRE_CHARGE };
enum EGON_FIREMODE { FIRE_NARROW, FIRE_WIDE};

#define	EGON_PRIMARY_VOLUME		450
#define EGON_BEAM_SPRITE		"sprites/xbeam1.spr"
#define EGON_FLARE_SPRITE		"sprites/XSpark1.spr"
#define EGON_SOUND_OFF			"weapons/egon_off1.wav"
#define EGON_SOUND_RUN			"weapons/egon_run3.wav"
#define EGON_SOUND_STARTUP		"weapons/egon_windup2.wav"

#define ARRAYSIZE(p)		(sizeof(p)/sizeof(p[0]))

BEAM *pBeam;
BEAM *pBeam2;



//======================
//	   TRIPMINE START
//======================
enum tripmine_e {
	TRIPMINE_IDLE1 = 0,
	TRIPMINE_IDLE2,
	TRIPMINE_ARM1,
	TRIPMINE_ARM2,
	TRIPMINE_FIDGET,
	TRIPMINE_HOLSTER,
	TRIPMINE_DRAW,
	TRIPMINE_WORLD,
	TRIPMINE_GROUND,
};

//We only check if it's possible to put a trip mine
//and if it is, then we play the animation. Server still places it.
void EV_TripmineFire( event_args_t *args )
{
	int idx;
	vec3_t vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy( args->origin, vecSrc );
	VectorCopy( args->angles, angles );

	AngleVectors ( angles, forward, NULL, NULL );
		
	if ( !EV_IsLocal ( idx ) )
		return;

	// Grab predicted result for local player
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );

	vecSrc = vecSrc + view_ofs;

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecSrc + forward * 128, PM_NORMAL, -1, &tr );

	//Hit something solid
	if ( tr.fraction < 1.0 )
		 gEngfuncs.pEventAPI->EV_WeaponAnimation ( TRIPMINE_DRAW, 0 );
	
	gEngfuncs.pEventAPI->EV_PopPMStates();
}
//======================
//	   TRIPMINE END
//======================

//======================
//	   SQUEAK START
//======================
enum squeak_e {
	SQUEAK_IDLE1 = 0,
	SQUEAK_FIDGETFIT,
	SQUEAK_FIDGETNIP,
	SQUEAK_DOWN,
	SQUEAK_UP,
	SQUEAK_THROW
};

#define VEC_HULL_MIN		Vector(-16, -16, -36)
#define VEC_DUCK_HULL_MIN	Vector(-16, -16, -18 )

void EV_SnarkFire( event_args_t *args )
{
	int idx;
	vec3_t vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy( args->origin, vecSrc );
	VectorCopy( args->angles, angles );

	AngleVectors ( angles, forward, NULL, NULL );
		
	if ( !EV_IsLocal ( idx ) )
		return;
	
	if ( args->ducking )
		vecSrc = vecSrc - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
	
	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc + forward * 20, vecSrc + forward * 64, PM_NORMAL, -1, &tr );

	//Find space to drop the thing.
	if ( tr.allsolid == 0 && tr.startsolid == 0 && tr.fraction > 0.25 )
		 gEngfuncs.pEventAPI->EV_WeaponAnimation ( SQUEAK_THROW, 0 );
	
	gEngfuncs.pEventAPI->EV_PopPMStates();
}
//======================
//	   SQUEAK END
//======================

void EV_TrainPitchAdjust( event_args_t *args )
{
	int idx;
	vec3_t origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	int stop;
	
	char sz[ 256 ];

	idx = args->entindex;
	
	VectorCopy( args->origin, origin );

	us_params = (unsigned short)args->iparam1;
	stop	  = args->bparam1;

	m_flVolume	= (float)(us_params & 0x003f)/40.0;
	noise		= (int)(((us_params) >> 12 ) & 0x0007);
	pitch		= (int)( 10.0 * (float)( ( us_params >> 6 ) & 0x003f ) );

	switch ( noise )
	{
	case 1: strcpy( sz, "plats/ttrain1.wav"); break;
	case 2: strcpy( sz, "plats/ttrain2.wav"); break;
	case 3: strcpy( sz, "plats/ttrain3.wav"); break; 
	case 4: strcpy( sz, "plats/ttrain4.wav"); break;
	case 5: strcpy( sz, "plats/ttrain6.wav"); break;
	case 6: strcpy( sz, "plats/ttrain7.wav"); break;
	default:
		// no sound
		strcpy( sz, "" );
		return;
	}

	if ( stop )
	{
		gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, sz );
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch );
	}
}

int EV_TFC_IsAllyTeam( int iTeam1, int iTeam2 )
{
	return 0;
}