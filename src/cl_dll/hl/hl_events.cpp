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
#include "../hud.h"
#include "../cl_util.h"
#include "event_api.h"

extern "C"
{
// HLDM
void EV_FireGlock1( struct event_args_s *args  );
void EV_FireGlock2( struct event_args_s *args  );
void EV_FireShotGunSingle( struct event_args_s *args  );
void EV_FireShotGunDouble( struct event_args_s *args  );
void EV_FireMP5( struct event_args_s *args  );
void EV_FireMP52( struct event_args_s *args  );
void EV_FirePython( struct event_args_s *args  );
void EV_FireGauss( struct event_args_s *args  );
void EV_SpinGauss( struct event_args_s *args  );
void EV_Wong( struct event_args_s *args );
void EV_FireCrossbow( struct event_args_s *args );
void EV_FireCrossbow2( struct event_args_s *args );
void EV_FireRpg( struct event_args_s *args );
void EV_EgonFire( struct event_args_s *args );
void EV_EgonStop( struct event_args_s *args );
void EV_HornetGunFire( struct event_args_s *args );
void EV_TripmineFire( struct event_args_s *args );
void EV_SnarkFire( struct event_args_s *args );

// SBO (defined in ev_hldm.cpp; names chosen by P2, the originals are unknown)
void EV_FireNm33( struct event_args_s *args );
void EV_FireSw( struct event_args_s *args );
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
void EV_FireChaingun( struct event_args_s *args );
void EV_Medikit( struct event_args_s *args );



void EV_TrainPitchAdjust( struct event_args_s *args );
void EV_VehiclePitchAdjust( struct event_args_s *args );
}

/*
======================
Game_HookEvents

Associate script file name with callback functions.  Callback's must be extern "C" so
 the engine doesn't get confused about name mangling stuff.  Note that the format is
 always the same.  Of course, a clever mod team could actually embed parameters, behavior
 into the actual .sc files and create a .sc file parser and hook their functionality through
 that.. i.e., a scripting system.

That was what we were going to do, but we ran out of time...oh well.
======================
*/
void Game_HookEvents( void )
{
	gEngfuncs.pfnHookEvent( "events/train.sc",		EV_TrainPitchAdjust );
	gEngfuncs.pfnHookEvent( "events/vehicle.sc",		EV_VehiclePitchAdjust );	// halflife-updated af9952e: HL25 func_vehicle
	gEngfuncs.pfnHookEvent( "events/rpg.sc",		EV_FireRpg );
	gEngfuncs.pfnHookEvent( "events/tripfire.sc",		EV_TripmineFire );
	gEngfuncs.pfnHookEvent( "events/snarkfire.sc",		EV_SnarkFire );
	gEngfuncs.pfnHookEvent( "events/nm33.sc",		EV_FireNm33 );
	gEngfuncs.pfnHookEvent( "events/sw.sc",		EV_FireSw );
	gEngfuncs.pfnHookEvent( "events/rc28_1.sc",		EV_FireRc28_1 );
	gEngfuncs.pfnHookEvent( "events/rc28_2.sc",		EV_FireRc28_2 );
	gEngfuncs.pfnHookEvent( "events/k444.sc",		EV_FireK444 );
	gEngfuncs.pfnHookEvent( "events/canon1.sc",		EV_FireCanon1 );
	gEngfuncs.pfnHookEvent( "events/canon2.sc",		EV_FireCanon2 );
	gEngfuncs.pfnHookEvent( "events/spa1.sc",		EV_FireSpa1 );
	gEngfuncs.pfnHookEvent( "events/spa2.sc",		EV_FireSpa2 );
	gEngfuncs.pfnHookEvent( "events/ben.sc",		EV_FireBen );
	gEngfuncs.pfnHookEvent( "events/pm66.sc",		EV_FirePm66 );
	gEngfuncs.pfnHookEvent( "events/pm66_2.sc",		EV_FirePm66_2 );
	gEngfuncs.pfnHookEvent( "events/federal.sc",		EV_FireFederal );
	gEngfuncs.pfnHookEvent( "events/fa18_1.sc",		EV_FireFa18_1 );
	gEngfuncs.pfnHookEvent( "events/fa18_2.sc",		EV_FireFa18_2 );
	gEngfuncs.pfnHookEvent( "events/chaingun.sc",		EV_FireChaingun );
	gEngfuncs.pfnHookEvent( "events/medikit.sc",		EV_Medikit );
	gEngfuncs.pfnHookEvent( "events/wong.sc",		EV_Wong );
}
