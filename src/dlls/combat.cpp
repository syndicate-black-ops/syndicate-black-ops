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
/*

===== combat.cpp ========================================================

  functions dealing with damage infliction & death

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "soundent.h"
#include "decals.h"
#include "animation.h"
#include "weapons.h"
#include "func_break.h"
#include "player.h"
#include "sbo_test.h"	// SBO_TEST_COMMANDS builds: test_shotluck (docs/dev/ENTITY_TESTS.md)
#include "gamerules.h"
#include "scripted.h"	// SBO 2.0: the scripted_sequence damage policies

extern DLL_GLOBAL short m_blastglow;	// SBO effect sprites (weapons.cpp)
extern DLL_GLOBAL short m_blastballs;
extern DLL_GLOBAL short m_pWallSmoke;
extern DLL_GLOBAL short m_psmoke;
extern DLL_GLOBAL short m_iPTrail;
extern DLL_GLOBAL short m_psprite;
extern DLL_GLOBAL short g_sSmogo;

extern DLL_GLOBAL Vector		g_vecAttackDir;
extern DLL_GLOBAL int			g_iSkillLevel;

extern Vector VecBModelOrigin( entvars_t* pevBModel );
extern entvars_t *g_pevLastInflictor;

#define GERMAN_GIB_COUNT		4
#define	HUMAN_GIB_COUNT			6
#define ALIEN_GIB_COUNT			4


// HACKHACK -- The gib velocity equations don't work
void CGib :: LimitVelocity( void )
{
	float length = pev->velocity.Length();

	// ceiling at 1500.  The gib velocity equation is not bounded properly.  Rather than tune it
	// in 3 separate places again, I'll just limit it here.
	if ( length > 1500.0 )
		pev->velocity = pev->velocity.Normalize() * 1500;		// This should really be sv_maxvelocity * 0.75 or something
}


void CGib :: SpawnStickyGibs( entvars_t *pevVictim, Vector vecOrigin, int cGibs )
{
	int i;

	if ( g_Language == LANGUAGE_GERMAN )
	{
		// no sticky gibs in germany right now!
		return; 
	}

	for ( i = 0 ; i < cGibs ; i++ )
	{
		CGib *pGib = GetClassPtr( (CGib *)NULL );

		pGib->Spawn( "models/stickygib.mdl" );
		pGib->pev->body = RANDOM_LONG(0,2);

		if ( pevVictim )
		{
			pGib->pev->origin.x = vecOrigin.x + RANDOM_FLOAT( -3, 3 );
			pGib->pev->origin.y = vecOrigin.y + RANDOM_FLOAT( -3, 3 );
			pGib->pev->origin.z = vecOrigin.z + RANDOM_FLOAT( -3, 3 );

			/*
			pGib->pev->origin.x = pevVictim->absmin.x + pevVictim->size.x * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.y = pevVictim->absmin.y + pevVictim->size.y * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.z = pevVictim->absmin.z + pevVictim->size.z * (RANDOM_FLOAT ( 0 , 1 ) );
			*/

			// make the gib fly away from the attack vector
			pGib->pev->velocity = g_vecAttackDir * -1;

			// mix in some noise
			pGib->pev->velocity.x += RANDOM_FLOAT ( -0.15, 0.15 );
			pGib->pev->velocity.y += RANDOM_FLOAT ( -0.15, 0.15 );
			pGib->pev->velocity.z += RANDOM_FLOAT ( -0.15, 0.15 );

			pGib->pev->velocity = pGib->pev->velocity * 900;

			pGib->pev->avelocity.x = RANDOM_FLOAT ( 250, 400 );
			pGib->pev->avelocity.y = RANDOM_FLOAT ( 250, 400 );

			// copy owner's blood color
			pGib->m_bloodColor = (CBaseEntity::Instance(pevVictim))->BloodColor();
		
			if ( pevVictim->health > -50)
			{
				pGib->pev->velocity = pGib->pev->velocity * 0.7;
			}
			else if ( pevVictim->health > -200)
			{
				pGib->pev->velocity = pGib->pev->velocity * 2;
			}
			else
			{
				pGib->pev->velocity = pGib->pev->velocity * 4;
			}

			
			pGib->pev->movetype = MOVETYPE_TOSS;
			pGib->pev->solid = SOLID_BBOX;
			UTIL_SetSize ( pGib->pev, Vector ( 0, 0 ,0 ), Vector ( 0, 0, 0 ) );
			pGib->SetTouch ( &CGib::StickyGibTouch );
			pGib->SetThink (NULL);
		}
		pGib->LimitVelocity();
	}
}

void CGib :: SpawnHeadGib( entvars_t *pevVictim )
{
	CGib *pGib = GetClassPtr( (CGib *)NULL );

	if ( g_Language == LANGUAGE_GERMAN )
	{
		pGib->Spawn( "models/germangibs.mdl" );// throw one head
		pGib->pev->body = 0;
	}
	else
	{
		pGib->Spawn( "models/hgibs.mdl" );// throw one head
		pGib->pev->body = 0;
	}

	if ( pevVictim )
	{
		pGib->pev->origin = pevVictim->origin + pevVictim->view_ofs;
		
		edict_t		*pentPlayer = FIND_CLIENT_IN_PVS( pGib->edict() );
		
		if ( RANDOM_LONG ( 0, 100 ) <= 5 && pentPlayer )
		{
			// 5% chance head will be thrown at player's face.
			entvars_t	*pevPlayer;

			pevPlayer = VARS( pentPlayer );
			pGib->pev->velocity = ( ( pevPlayer->origin + pevPlayer->view_ofs ) - pGib->pev->origin ).Normalize() * 300;
			pGib->pev->velocity.z += 100;
		}
		else
		{
			pGib->pev->velocity = Vector (RANDOM_FLOAT(-100,100), RANDOM_FLOAT(-100,100), RANDOM_FLOAT(200,300));
		}


		pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
		pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 300 );

		// copy owner's blood color
		pGib->m_bloodColor = (CBaseEntity::Instance(pevVictim))->BloodColor();
	
		if ( pevVictim->health > -50)
		{
			pGib->pev->velocity = pGib->pev->velocity * 0.7;
		}
		else if ( pevVictim->health > -200)
		{
			pGib->pev->velocity = pGib->pev->velocity * 2;
		}
		else
		{
			pGib->pev->velocity = pGib->pev->velocity * 4;
		}
	}
	pGib->LimitVelocity();
}

void CGib :: SpawnRandomGibs( entvars_t *pevVictim, int cGibs, int human, const char *szModel )
{
	int cSplat;

	for ( cSplat = 0 ; cSplat < cGibs ; cSplat++ )
	{
		CGib *pGib = GetClassPtr( (CGib *)NULL );

		if ( szModel )
		{
			pGib->Spawn( szModel );
			int cBodies = MODEL_FRAMES( pGib->pev->modelindex );
			pGib->pev->body = cBodies > 1 ? RANDOM_LONG( 0, cBodies - 1 ) : 0;
		}
		else if ( g_Language == LANGUAGE_GERMAN )
		{
			pGib->Spawn( "models/germangibs.mdl" );
			pGib->pev->body = RANDOM_LONG(0,GERMAN_GIB_COUNT-1);
		}
		else
		{
			if ( human )
			{
				// human pieces
				pGib->Spawn( "models/hgibs.mdl" );
				pGib->pev->body = RANDOM_LONG(1,HUMAN_GIB_COUNT-1);// start at one to avoid throwing random amounts of skulls (0th gib)
			}
			else
			{
				// aliens
				pGib->Spawn( "models/agibs.mdl" );
				pGib->pev->body = RANDOM_LONG(0,ALIEN_GIB_COUNT-1);
			}
		}

		if ( pevVictim )
		{
			// spawn the gib somewhere in the monster's bounding volume
			pGib->pev->origin.x = pevVictim->absmin.x + pevVictim->size.x * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.y = pevVictim->absmin.y + pevVictim->size.y * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.z = pevVictim->absmin.z + pevVictim->size.z * (RANDOM_FLOAT ( 0 , 1 ) ) + 1;	// absmin.z is in the floor because the engine subtracts 1 to enlarge the box

			// make the gib fly away from the attack vector
			pGib->pev->velocity = g_vecAttackDir * -1;

			// mix in some noise
			pGib->pev->velocity.x += RANDOM_FLOAT ( -0.25, 0.25 );
			pGib->pev->velocity.y += RANDOM_FLOAT ( -0.25, 0.25 );
			pGib->pev->velocity.z += RANDOM_FLOAT ( -0.25, 0.25 );

			pGib->pev->velocity = pGib->pev->velocity * RANDOM_FLOAT ( 300, 400 );

			pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
			pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 300 );

			// copy owner's blood color
			pGib->m_bloodColor = (CBaseEntity::Instance(pevVictim))->BloodColor();
			
			if ( pevVictim->health > -50)
			{
				pGib->pev->velocity = pGib->pev->velocity * 0.7;
			}
			else if ( pevVictim->health > -200)
			{
				pGib->pev->velocity = pGib->pev->velocity * 2;
			}
			else
			{
				pGib->pev->velocity = pGib->pev->velocity * 4;
			}

			pGib->pev->solid = SOLID_BBOX;
			UTIL_SetSize ( pGib->pev, Vector( 0 , 0 , 0 ), Vector ( 0, 0, 0 ) );
		}
		pGib->LimitVelocity();
	}
}


BOOL CBaseMonster :: HasHumanGibs( void )
{
	int myClass = Classify();

	if ( myClass == CLASS_HUMAN_MILITARY ||
		 myClass == CLASS_PLAYER_ALLY	||
		 myClass == CLASS_HUMAN_PASSIVE  ||
		 myClass == CLASS_PLAYER )

		 return TRUE;

	return FALSE;
}


BOOL CBaseMonster :: HasAlienGibs( void )
{
	int myClass = Classify();

	if ( myClass == CLASS_ALIEN_MILITARY ||
		 myClass == CLASS_ALIEN_MONSTER	||
		 myClass == CLASS_ALIEN_PASSIVE  ||
		 myClass == CLASS_INSECT  ||
		 myClass == CLASS_ALIEN_PREDATOR  ||
		 myClass == CLASS_ALIEN_PREY )

		 return TRUE;

	return FALSE;
}


void CBaseMonster::FadeMonster( void )
{
	StopAnimation();
	pev->velocity = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
	pev->avelocity = g_vecZero;
	pev->animtime = gpGlobals->time;
	pev->effects |= EF_NOINTERP;
	SUB_StartFadeOut();
}

//=========================================================
// GibMonster - create some gore and get rid of a monster's
// model.
//=========================================================
void CBaseMonster :: GibMonster( void )
{
	TraceResult	tr;
	BOOL		gibbed = FALSE;

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "common/bodysplat.wav", 1, ATTN_NORM);		

	// SBO 2.0 (Featureful GibMonster): a custom gib model throws 4 random gibs of it, no skull; a monster
	// without stock gibs throws them too
	const char *szGibModel = FStringNull( m_iszGibModelPrecached ) ? NULL : STRING( m_iszGibModelPrecached );
	if ( szGibModel )
	{
		if ( HasHumanGibs() )
		{
			if ( CVAR_GET_FLOAT("violence_hgibs") != 0 )
				CGib::SpawnRandomGibs( pev, 4, 1, szGibModel );
		}
		else if ( HasAlienGibs() )
		{
			if ( CVAR_GET_FLOAT("violence_agibs") != 0 )
				CGib::SpawnRandomGibs( pev, 4, 0, szGibModel );
		}
		else
			CGib::SpawnRandomGibs( pev, 4, 0, szGibModel );
		gibbed = TRUE;
	}
	// only humans throw skulls !!!UNDONE - eventually monsters will have their own sets of gibs
	else if ( HasHumanGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_hgibs") != 0 )	// Only the player will ever get here
		{
			CGib::SpawnHeadGib( pev );
			CGib::SpawnRandomGibs( pev, 4, 1 );	// throw some human gibs.
		}
		gibbed = TRUE;
	}
	else if ( HasAlienGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_agibs") != 0 )	// Should never get here, but someone might call it directly
		{
			CGib::SpawnRandomGibs( pev, 4, 0 );	// Throw alien gibs
		}
		gibbed = TRUE;
	}

	if ( !IsPlayer() )
	{
		if ( gibbed )
		{
			// don't remove players!
			SetThink ( &CBaseMonster::SUB_Remove );
			pev->nextthink = gpGlobals->time;
		}
		else
		{
			FadeMonster();
		}
	}
}

//=========================================================
// GetDeathActivity - determines the best type of death
// anim to play.
//=========================================================
Activity CBaseMonster :: GetDeathActivity ( void )
{
	Activity	deathActivity;
	BOOL		fTriedDirection;
	float		flDot;
	TraceResult	tr;
	Vector		vecSrc;

	if ( pev->deadflag != DEAD_NO )
	{
		// don't run this while dying.
		return m_IdealActivity;
	}

	vecSrc = Center();

	fTriedDirection = FALSE;
	deathActivity = ACT_DIESIMPLE;// in case we can't find any special deaths to do.

	UTIL_MakeVectors ( pev->angles );
	flDot = DotProduct ( gpGlobals->v_forward, g_vecAttackDir * -1 );

	switch ( m_LastHitGroup )
	{
		// try to pick a region-specific death.
	case HITGROUP_HEAD:
		deathActivity = ACT_DIE_HEADSHOT;
		break;

	case HITGROUP_STOMACH:
		deathActivity = ACT_DIE_GUTSHOT;
		break;

	case HITGROUP_GENERIC:
		// try to pick a death based on attack direction
		fTriedDirection = TRUE;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
		break;

	default:
		// try to pick a death based on attack direction
		fTriedDirection = TRUE;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
		break;
	}


	// can we perform the prescribed death?
	if ( LookupActivity ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		// no! did we fail to perform a directional death? 
		if ( fTriedDirection )
		{
			// if yes, we're out of options. Go simple.
			deathActivity = ACT_DIESIMPLE;
		}
		else
		{
			// cannot perform the ideal region-specific death, so try a direction.
			if ( flDot > 0.3 )
			{
				deathActivity = ACT_DIEFORWARD;
			}
			else if ( flDot <= -0.3 )
			{
				deathActivity = ACT_DIEBACKWARD;
			}
		}
	}

	if ( LookupActivity ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		// if we're still invalid, simple is our only option.
		deathActivity = ACT_DIESIMPLE;
	}

	if ( deathActivity == ACT_DIEFORWARD )
	{
			// make sure there's room to fall forward
			UTIL_TraceHull ( vecSrc, vecSrc + gpGlobals->v_forward * 64, dont_ignore_monsters, head_hull, edict(), &tr );

			if ( tr.flFraction != 1.0 )
			{
				deathActivity = ACT_DIESIMPLE;
			}
	}

	if ( deathActivity == ACT_DIEBACKWARD )
	{
			// make sure there's room to fall backward
			UTIL_TraceHull ( vecSrc, vecSrc - gpGlobals->v_forward * 64, dont_ignore_monsters, head_hull, edict(), &tr );

			if ( tr.flFraction != 1.0 )
			{
				deathActivity = ACT_DIESIMPLE;
			}
	}

	return deathActivity;
}

//=========================================================
// GetSmallFlinchActivity - determines the best type of flinch
// anim to play.
//=========================================================
Activity CBaseMonster :: GetSmallFlinchActivity ( void )
{
	Activity	flinchActivity;
	BOOL		fTriedDirection;
	float		flDot;

	fTriedDirection = FALSE;
	UTIL_MakeVectors ( pev->angles );
	flDot = DotProduct ( gpGlobals->v_forward, g_vecAttackDir * -1 );
	
	switch ( m_LastHitGroup )
	{
		// pick a region-specific flinch
	case HITGROUP_HEAD:
		flinchActivity = ACT_FLINCH_HEAD;
		break;
	case HITGROUP_STOMACH:
		flinchActivity = ACT_FLINCH_STOMACH;
		break;
	case HITGROUP_LEFTARM:
		flinchActivity = ACT_FLINCH_LEFTARM;
		break;
	case HITGROUP_RIGHTARM:
		flinchActivity = ACT_FLINCH_RIGHTARM;
		break;
	case HITGROUP_LEFTLEG:
		flinchActivity = ACT_FLINCH_LEFTLEG;
		break;
	case HITGROUP_RIGHTLEG:
		flinchActivity = ACT_FLINCH_RIGHTLEG;
		break;
	case HITGROUP_GENERIC:
	default:
		// just get a generic flinch.
		flinchActivity = ACT_SMALL_FLINCH;
		break;
	}


	// do we have a sequence for the ideal activity?
	if ( LookupActivity ( flinchActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		flinchActivity = ACT_SMALL_FLINCH;
	}

	return flinchActivity;
}


void CBaseMonster::BecomeDead( void )
{
	pev->takedamage = DAMAGE_YES;// don't let autoaim aim at corpses.
	
	// give the corpse half of the monster's original maximum health. 
	pev->health = pev->max_health / 2;
	pev->max_health = 5; // max_health now becomes a counter for how many blood decals the corpse can place.

	// make the corpse fly away from the attack vector
	pev->movetype = MOVETYPE_TOSS;
	//pev->flags &= ~FL_ONGROUND;
	//pev->origin.z += 2;
	//pev->velocity = g_vecAttackDir * -1;
	//pev->velocity = pev->velocity * RANDOM_FLOAT( 300, 400 );
}


BOOL CBaseMonster::ShouldGibMonster( int iGib )
{
	// SBO 2.0, from Featureful: "gib_policy" 1 prefer gibbing, 2 prefer not to gib (GIB_NEVER / GIB_ALWAYS still win)
	if ( iGib != GIB_NEVER && m_iGibPolicy == 1 )
		return TRUE;
	if ( iGib != GIB_ALWAYS && m_iGibPolicy == 2 )
		return FALSE;

	if ( ( iGib == GIB_NORMAL && pev->health < GIB_HEALTH_VALUE ) || ( iGib == GIB_ALWAYS ) )
		return TRUE;
	
	return FALSE;
}


void CBaseMonster::CallGibMonster( void )
{
	BOOL fade = FALSE;

	if ( HasHumanGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_hgibs") == 0 )
			fade = TRUE;
	}
	else if ( HasAlienGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_agibs") == 0 )
			fade = TRUE;
	}

	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;// do something with the body. while monster blows up

	if ( fade )
	{
		FadeMonster();
	}
	else
	{
		pev->effects = EF_NODRAW; // make the model invisible.
		GibMonster();
	}

	pev->deadflag = DEAD_DEAD;
	FCheckAITrigger();

	// don't let the status bar glitch for players.with <0 health.
	if (pev->health < -99)
	{
		pev->health = 0;
	}
	
	if ( ShouldFadeOnDeath() && !fade )
		UTIL_Remove(this);
}


/*
============
Killed
============
*/
void CBaseMonster :: Killed( entvars_t *pevAttacker, int iGib )
{
	unsigned int	cCount = 0;
	BOOL			fDone = FALSE;

	if ( HasMemory( bits_MEMORY_KILLED ) )
	{
		if ( ShouldGibMonster( iGib ) )
			CallGibMonster();
		return;
	}

	Remember( bits_MEMORY_KILLED );

	SboKillfeed_NpcKilled( this, pevAttacker );	// chat-vgui2: kill feed (a player killed this NPC)

	// clear the deceased's sound channels.(may have been firing or reloading when killed)
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM);
	m_IdealMonsterState = MONSTERSTATE_DEAD;
	// Make sure this condition is fired too (TakeDamage breaks out before this happens on death)
	SetConditions( bits_COND_LIGHT_DAMAGE );
	
	// tell owner ( if any ) that we're dead.This is mostly for MonsterMaker functionality.
	CBaseEntity *pOwner = CBaseEntity::Instance(pev->owner);
	if ( pOwner )
	{
		pOwner->DeathNotice( pev );
	}

	FireTriggerOnDeath( pevAttacker );	// SBO 2.0 (Featureful TriggerOnDeath)

	if	( ShouldGibMonster( iGib ) )
	{
		CallGibMonster();
		return;
	}
	else if ( pev->flags & FL_MONSTER )
	{
		SetTouch( NULL );
		BecomeDead();
	}
	
	// don't let the status bar glitch for players.with <0 health.
	if (pev->health < -99)
	{
		pev->health = 0;
	}
	
	//pev->enemy = ENT( pevAttacker );//why? (sjb)
	
	m_IdealMonsterState = MONSTERSTATE_DEAD;
}

//=========================================================
// UpdateOnRemove - SBO: a monster removed while still alive
// (killtarget, UTIL_Remove) never gets to Killed(), so its
// monstermaker would keep counting it as a live child and
// stop spawning (Valve issue #1746). Tell the owner here the
// same way Killed() does. Monsters that already died or are
// dying (corpses, crashing apache/osprey) were reported by
// Killed() (apache/osprey: NotifyOwnerOfDeath), so skip them, and so
// is a second UTIL_Remove() in the same frame (FL_KILLME).
//=========================================================
void CBaseMonster :: UpdateOnRemove( void )
{
	if ( pev->deadflag == DEAD_NO && !HasMemory( bits_MEMORY_KILLED ) && !FBitSet( pev->flags, FL_KILLME ) )
	{
		CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
		if ( pOwner )
		{
			pOwner->DeathNotice( pev );
		}
	}

	CBaseToggle::UpdateOnRemove();
}

//=========================================================
// NotifyOwnerOfDeath - SBO 2.0: what CBaseMonster::Killed does
// for the monstermaker, for the Killed overrides that don't
// call it (CApache, COsprey: a shot-down one never freed its
// maker's live-child slot, so a maker with "Max live
// children" stopped spawning; HL SDK bug). Once: the
// bits_MEMORY_KILLED memory, as Killed() and UpdateOnRemove()
// test it.
//=========================================================
void CBaseMonster :: NotifyOwnerOfDeath( void )
{
	if ( HasMemory( bits_MEMORY_KILLED ) )
		return;

	Remember( bits_MEMORY_KILLED );

	CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
	if ( pOwner )
	{
		pOwner->DeathNotice( pev );
	}
}

//
// fade out - slowly fades a entity out, then removes it.
//
// DON'T USE ME FOR GIBS AND STUFF IN MULTIPLAYER! 
// SET A FUTURE THINK AND A RENDERMODE!!
void CBaseEntity :: SUB_StartFadeOut ( void )
{
	if (pev->rendermode == kRenderNormal)
	{
		pev->renderamt = 255;
		pev->rendermode = kRenderTransTexture;
	}

	pev->solid = SOLID_NOT;
	pev->avelocity = g_vecZero;

	pev->nextthink = gpGlobals->time + 0.1;
	SetThink ( &CBaseMonster::SUB_FadeOut );
}

void CBaseEntity :: SUB_FadeOut ( void  )
{
	if ( pev->renderamt > 7 )
	{
		pev->renderamt -= 7;
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else 
	{
		pev->renderamt = 0;
		pev->nextthink = gpGlobals->time + 0.2;
		SetThink ( &CBaseMonster::SUB_Remove );
	}
}

//=========================================================
// WaitTillLand - in order to emit their meaty scent from
// the proper location, gibs should wait until they stop 
// bouncing to emit their scent. That's what this function
// does.
//=========================================================
void CGib :: WaitTillLand ( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	// SBO 2.0 (Featureful 24651272, 503130c0): a gib that never comes to rest (rolling or spinning on an edge) fades
	// all the same 10 s after its lifetime, so its edict is freed
	if ( pev->velocity == g_vecZero || m_bornTime + m_lifeTime + 10 <= gpGlobals->time )
	{
		SetThink (&CBaseMonster::SUB_StartFadeOut);
		if ( pev->velocity == g_vecZero )
			pev->nextthink = gpGlobals->time + m_lifeTime;
		else
			pev->nextthink = gpGlobals->time;

		// If you bleed, you stink!
		if ( m_bloodColor != DONT_BLEED )
		{
			// ok, start stinkin!
			CSoundEnt::InsertSound ( bits_SOUND_MEAT, pev->origin, 384, 25 );
		}
	}
	else
	{
		// wait and check again in another half second.
		pev->nextthink = gpGlobals->time + 0.5;
	}
}

//
// Gib bounces on the ground or wall, sponges some blood down, too!
//
void CGib :: BounceGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	TraceResult	tr;
	
	//if ( RANDOM_LONG(0,1) )
	//	return;// don't bleed everytime

	if (pev->flags & FL_ONGROUND)
	{
		pev->velocity = pev->velocity * 0.9;
		pev->angles.x = 0;
		pev->angles.z = 0;
		pev->avelocity.x = 0;
		pev->avelocity.z = 0;
	}
	else
	{
		if ( g_Language != LANGUAGE_GERMAN && m_cBloodDecals > 0 && m_bloodColor != DONT_BLEED )
		{
			vecSpot = pev->origin + Vector ( 0 , 0 , 8 );//move up a bit, and trace down.
			UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -24 ),  ignore_monsters, ENT(pev), & tr);

			UTIL_BloodDecalTrace( &tr, m_bloodColor );

			m_cBloodDecals--; 
		}

		if ( m_material != matNone && RANDOM_LONG(0,2) == 0 )
		{
			float volume;
			float zvel = fabs(pev->velocity.z);
		
			volume = 0.8 * min(1.0, ((float)zvel) / 450.0);

			CBreakable::MaterialSoundRandom( edict(), (Materials)m_material, volume );
		}
	}
}

//
// Sticky gib puts blood on the wall and stays put. 
//
void CGib :: StickyGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	TraceResult	tr;
	
	SetThink ( &CBaseMonster::SUB_Remove );
	pev->nextthink = gpGlobals->time + 10;

	if ( !FClassnameIs( pOther->pev, "worldspawn" ) )
	{
		pev->nextthink = gpGlobals->time;
		return;
	}

	UTIL_TraceLine ( pev->origin, pev->origin + pev->velocity * 32,  ignore_monsters, ENT(pev), & tr);

	UTIL_BloodDecalTrace( &tr, m_bloodColor );

	pev->velocity = tr.vecPlaneNormal * -1;
	pev->angles = UTIL_VecToAngles ( pev->velocity );
	pev->velocity = g_vecZero; 
	pev->avelocity = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
}

//
// Throw a chunk
//
void CGib :: Spawn( const char *szGibModel )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->friction = 0.55; // deading the bounce a bit
	
	// sometimes an entity inherits the edict from a former piece of glass,
	// and will spawn using the same render FX or rendermode! bad!
	pev->renderamt = 255;
	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxNone;
	pev->solid = SOLID_SLIDEBOX;/// hopefully this will fix the VELOCITY TOO LOW crap
	pev->classname = MAKE_STRING("gib");

	SET_MODEL(ENT(pev), szGibModel);
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));

	pev->nextthink = gpGlobals->time + 4;
	m_lifeTime = 25;
	m_bornTime = gpGlobals->time;
	SetThink ( &CGib::WaitTillLand );
	SetTouch ( &CGib::BounceGibTouch );

	m_material = matNone;
	m_cBloodDecals = 5;// how many blood decals this gib can place (1 per bounce until none remain). 
}

// take health
int CBaseMonster :: TakeHealth (float flHealth, int bitsDamageType)
{
	if (!pev->takedamage)
		return 0;

	// clear out any damage types we healed.
	// UNDONE: generic health should not heal any
	// UNDONE: time-based damage

	m_bitsDamageType &= ~(bitsDamageType & ~DMG_TIMEBASED);
	
	// SBO 2.0, from Featureful: AI trigger condition 13 "got healed"
	int iHealed = CBaseEntity::TakeHealth(flHealth, bitsDamageType);
	if ( iHealed )
		Remember( bits_MEMORY_GOT_HEALED_RECENTLY );
	return iHealed;
}

// SBO 2.0 (Featureful "m_takeDamagePolicy" 1): a script holding the monster makes it invulnerable. The monsters'
// own TakeDamage overrides check it first too, so a hit on it gives no XP either
BOOL CBaseMonster :: ScriptInvulnerable( void )
{
	return m_pCine && m_pCine->m_iTakeDamagePolicy == SCRIPT_TAKEDAMAGE_INVULNERABLE;
}

/*
============
TakeDamage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
bitsDamageType indicates the type of damage sustained, ie: DMG_SHOCK

Time-based damage: only occurs while the monster is within the trigger_hurt.
When a monster is poisoned via an arrow etc it takes all the poison damage at once.



GLOBALS ASSUMED SET:  g_iSkillLevel
============
*/
int CBaseMonster :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	float	flTake;
	Vector	vecDir;

	if (!pev->takedamage)
		return 0;

	// SBO 2.0 (Featureful "m_takeDamagePolicy" Invulnerable during script): no damage while a script holds it
	if ( ScriptInvulnerable() )
		return 0;

	if ( !IsAlive() )
	{
		return DeadTakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
	}

	// SBO: random perfect/critical/failed shots
	// CBaseEntity::Instance( pevAttacker ) written out (unused result): the inline call uses up MSVC's inline
	// budget so the later vecDir.Normalize() would not inline as in hl.dll
	edict_t *pentAttacker = ENT( pevAttacker );
	if ( !pentAttacker )
		pentAttacker = ENT( 0 );
	CBaseEntity *pAttacker = (CBaseEntity *)GET_PRIVATE( pentAttacker );
	int iShot = RANDOM_LONG( 1, 100 );	// SBO 2.0: whole numbers 1-100, each 1 in 100 (1.38: RANDOM_FLOAT truncated, 100 never came up)
#ifdef SBO_TEST_COMMANDS
	if ( Test_ShotLuckOff() )	// test builds only: test_shotluck 0, an exact hit (the draw above still happens)
		iShot = 50;
#endif

	if ( iShot == 1 )
	{
		flDamage *= 2;
		if ( !strcmp( STRING( pevAttacker->classname ), "player" ) )
			ClientPrint( pevAttacker, HUD_PRINTCENTER, "PERFECT SHOT\n" );
	}
	else if ( iShot == 2 )
	{
		flDamage *= 1.5;
		if ( !strcmp( STRING( pevAttacker->classname ), "player" ) )
			ClientPrint( pevAttacker, HUD_PRINTCENTER, "CRITICAL SHOT\n" );
	}
	else if ( iShot == 99 )
	{
		flDamage *= 0.5f;
		if ( !strcmp( STRING( pevAttacker->classname ), "player" ) )
			ClientPrint( pevAttacker, HUD_PRINTCENTER, "FAIL SHOT\n" );
	}
	else if ( iShot == 100 )
	{
		flDamage *= 0.25f;
		if ( !strcmp( STRING( pevAttacker->classname ), "player" ) )
			ClientPrint( pevAttacker, HUD_PRINTCENTER, "VERY BAD SHOT\n" );
	}

	// SBO: players with this flag deal double damage
	if ( !strcmp( STRING( pevAttacker->classname ), "player" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pevAttacker );
		if ( pPlayer->m_bTargeting == 1 )
			flDamage *= 2;
	}

	if ( pev->deadflag == DEAD_NO )
	{
		// no pain sound during death animation.
		PainSound();// "Ouch!"
	}

	//!!!LATER - make armor consideration here!
	flTake = flDamage;

	// set damage type sustained
	m_bitsDamageType |= bitsDamageType;

	// grab the vector of the incoming attack. ( pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	vecDir = Vector( 0, 0, 0 );
	if (!FNullEnt( pevInflictor ))
	{
		CBaseEntity *pInflictor = CBaseEntity :: Instance( pevInflictor );
		if (pInflictor)
		{
			vecDir = ( pInflictor->Center() - Vector ( 0, 0, 10 ) - Center() ).Normalize();
			vecDir = g_vecAttackDir = vecDir.Normalize();
		}
	}

	// add to the damage total for clients, which will be sent as a single
	// message at the end of the frame
	// todo: remove after combining shotgun blasts?
	if ( IsPlayer() )
	{
		if ( pevInflictor )
			pev->dmg_inflictor = ENT(pevInflictor);

		pev->dmg_take += flTake;

		// check for godmode or invincibility
		if ( pev->flags & FL_GODMODE )
		{
			return 0;
		}
	}

	// if this is a player, move him around!
	if ( ( !FNullEnt( pevInflictor ) ) && (pev->movetype == MOVETYPE_WALK) && (!pevAttacker || pevAttacker->solid != SOLID_TRIGGER) )
	{
		pev->velocity = pev->velocity + vecDir * -DamageForce( flDamage );
	}

	// SBO 2.0 (Featureful "m_takeDamagePolicy" Non-lethal damage): playing the script, it keeps at least 1 health
	if ( m_MonsterState == MONSTERSTATE_SCRIPT && m_pCine && m_pCine->m_iTakeDamagePolicy == SCRIPT_TAKEDAMAGE_NONLETHAL &&
		 flTake >= pev->health )
		flTake = pev->health > 1 ? pev->health - 1 : 0;

	// SBO 2.0 cheat_unkillable (Half-Life Unified SDK, cheat_commands.cpp): the player keeps at least 1 health. Here,
	// not in CBasePlayer::TakeDamage, because the random shot multipliers above come after the player's own code.
	if ( IsPlayer() && ( (CBasePlayer *)this )->m_bCheatUnkillable && flTake >= pev->health )
		flTake = pev->health > 1 ? pev->health - 1 : 0;

	// do the damage
	pev->health -= flTake;

	// SBO 2.1 (Half-Life Unified SDK): "unkillable": the damage leaves it barely alive
	if ( m_fUnkillable && pev->health < 1 )
		pev->health = 1;

	// SBO 2.0 (Featureful "Allow Take Damage TriggerCondition"): the hit fires a Take Damage TriggerTarget even
	// while the script ignores damage
	if ( m_MonsterState == MONSTERSTATE_SCRIPT && m_pCine && flTake > 0 &&
		 FBitSet( m_pCine->pev->spawnflags, SF_SCRIPT_ALLOW_TAKEDAMAGE_TRIGGER ) )
		m_fForceTakeDamageTrigger = TRUE;
	
	// SBO: no script exception (SDK: monsters in a script only get bits_COND_LIGHT_DAMAGE)

	if ( pev->health <= 0 )
	{
		g_pevLastInflictor = pevInflictor;

		if ( bitsDamageType & DMG_ALWAYSGIB )
		{
			Killed( pevAttacker, GIB_ALWAYS );
		}
		else if ( bitsDamageType & DMG_NEVERGIB )
		{
			Killed( pevAttacker, GIB_NEVER );
		}
		else
		{
			Killed( pevAttacker, GIB_NORMAL );
		}

		g_pevLastInflictor = NULL;

		return 0;
	}

	// react to the damage (get mad)
	if ( (pev->flags & FL_MONSTER) && !FNullEnt(pevAttacker) )
	{
		if ( pevAttacker->flags & (FL_MONSTER | FL_CLIENT) )
		{// only if the attack was a monster or client!
			
			// enemy's last known position is somewhere down the vector that the attack came from.
			if (pevInflictor)
			{
				if (m_hEnemy == NULL || pevInflictor == m_hEnemy->pev || !HasConditions(bits_COND_SEE_ENEMY))
				{
					m_vecEnemyLKP = pevInflictor->origin;
				}
			}
			else
			{
				m_vecEnemyLKP = pev->origin + ( g_vecAttackDir * 64 ); 
			}

			MakeIdealYaw( m_vecEnemyLKP );

			// add pain to the conditions 
			// !!!HACKHACK - fudged for now. Do we want to have a virtual function to determine what is light and 
			// heavy damage per monster class?
			if ( flDamage > 0 )
			{
				SetConditions(bits_COND_LIGHT_DAMAGE);
			}

			if ( flDamage >= 20 )
			{
				SetConditions(bits_COND_HEAVY_DAMAGE);
			}
		}
	}

	return 1;
}

//=========================================================
// DeadTakeDamage - takedamage function called when a monster's
// corpse is damaged.
//=========================================================
int CBaseMonster :: DeadTakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	Vector			vecDir;

	// grab the vector of the incoming attack. ( pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	vecDir = Vector( 0, 0, 0 );
	if (!FNullEnt( pevInflictor ))
	{
		CBaseEntity *pInflictor = CBaseEntity :: Instance( pevInflictor );
		if (pInflictor)
		{
			vecDir = ( pInflictor->Center() - Vector ( 0, 0, 10 ) - Center() ).Normalize();
			vecDir = g_vecAttackDir = vecDir.Normalize();
		}
	}

#if 0// turn this back on when the bounding box issues are resolved.

	pev->flags &= ~FL_ONGROUND;
	pev->origin.z += 1;
	
	// let the damage scoot the corpse around a bit.
	if ( !FNullEnt(pevInflictor) && (pevAttacker->solid != SOLID_TRIGGER) )
	{
		pev->velocity = pev->velocity + vecDir * -DamageForce( flDamage );
	}

#endif

	// kill the corpse if enough damage was done to destroy the corpse and the damage is of a type that is allowed to destroy the corpse.
	if ( bitsDamageType & DMG_GIB_CORPSE )
	{
		if ( pev->health <= flDamage )
		{
			pev->health = -50;
			Killed( pevAttacker, GIB_ALWAYS );
			return 0;
		}
		// Accumulate corpse gibbing damage, so you can gib with multiple hits
		pev->health -= flDamage * 0.1;
	}
	
	return 1;
}


float CBaseMonster :: DamageForce( float damage )
{ 
	float force = damage * ((32 * 32 * 72.0) / (pev->size.x * pev->size.y * pev->size.z)) * 5;
	
	if ( force > 1000.0) 
	{
		force = 1000.0;
	}

	return force;
}

//
// RadiusDamage - this entity is exploding, or otherwise needs to inflict damage upon entities within a certain range.
// 
// only damage ents that can clearly be seen by the explosion!

	
void RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType )
{
	CBaseEntity *pEntity = NULL;
	TraceResult	tr;
	float		flAdjustedDamage, falloff;
	Vector		vecSpot;

	if ( flRadius )
		falloff = flDamage / flRadius;
	else
		falloff = 1.0;

	vecSrc.z += 1;// in case grenade is lying on the ground

	// halflife-updated PR #279: test the water contents after the + 1, i.e. at the point the blast traces
	// start from. Before, a grenade on the floor of a pool (CGrenade::Explode passes origin.z - 1 to cancel
	// this + 1) sampled the brush below the water and hurt nothing underwater.
	int bInWater = (UTIL_PointContents ( vecSrc ) == CONTENTS_WATER);

	if ( !pevAttacker )
		pevAttacker = pevInflictor;

	// iterate on all entities in the vicinity.
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, vecSrc, flRadius )) != NULL)
	{
		if ( pEntity->pev->takedamage != DAMAGE_NO )
		{
			// UNDONE: this should check a damage mask, not an ignore
			if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
			{// houndeyes don't hurt other houndeyes with their attack
				continue;
			}

			// blast's don't tavel into or out of water
			if (bInWater && pEntity->pev->waterlevel == 0)
				continue;
			if (!bInWater && pEntity->pev->waterlevel == 3)
				continue;

			vecSpot = pEntity->BodyTarget( vecSrc );
			
			UTIL_TraceLine ( vecSrc, vecSpot, dont_ignore_monsters, ENT(pevInflictor), &tr );

			if ( tr.flFraction == 1.0 || tr.pHit == pEntity->edict() )
			{// the explosion can 'see' this entity, so hurt them!
				if (tr.fStartSolid)
				{
					// if we're stuck inside them, fixup the position and distance
					tr.vecEndPos = vecSrc;
					tr.flFraction = 0.0;
				}
				
				// decrease damage for an ent that's farther from the bomb.
				flAdjustedDamage = ( vecSrc - tr.vecEndPos ).Length() * falloff;
				flAdjustedDamage = flDamage - flAdjustedDamage;
			
				if ( flAdjustedDamage < 0 )
				{
					flAdjustedDamage = 0;
				}
			
				// ALERT( at_console, "hit %s\n", STRING( pEntity->pev->classname ) );
				if (tr.flFraction != 1.0)
				{
					ClearMultiDamage( );
					pEntity->TraceAttack( pevInflictor, flAdjustedDamage, (tr.vecEndPos - vecSrc).Normalize( ), &tr, bitsDamageType );
					ApplyMultiDamage( pevInflictor, pevAttacker );
				}
				else
				{
					pEntity->TakeDamage ( pevInflictor, pevAttacker, flAdjustedDamage, bitsDamageType );
				}
			}
		}
	}
}


void CBaseMonster :: RadiusDamage(entvars_t* pevInflictor, entvars_t*	pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType )
{
	::RadiusDamage( pev->origin, pevInflictor, pevAttacker, flDamage, flDamage * 2.5, iClassIgnore, bitsDamageType );
}


void CBaseMonster :: RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType )
{
	::RadiusDamage( vecSrc, pevInflictor, pevAttacker, flDamage, flDamage * 2.5, iClassIgnore, bitsDamageType );
}


//=========================================================
// CheckTraceHullAttack - expects a length to trace, amount 
// of damage to do, and damage type. Returns a pointer to
// the damaged entity in case the monster wishes to do
// other stuff to the victim (punchangle, etc)
//
// Used for many contact-range melee attacks. Bites, claws, etc.
//=========================================================
CBaseEntity* CBaseMonster :: CheckTraceHullAttack( float flDist, int iDamage, int iDmgType )
{
	TraceResult tr;

	if (IsPlayer())
		UTIL_MakeVectors( pev->angles );
	else
		UTIL_MakeAimVectors( pev->angles );

	// SBO 2.0 model scale: from the height of the box at scale 1, reaching as much farther as the scaled box is
	// wider (UTIL_ScaledHalfWidthExtra; both unchanged when unscaled)
	Vector vecStart = pev->origin;
	vecStart.z += UTIL_UnscaledHeight( pev ) * 0.5;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * ( flDist + UTIL_ScaledHalfWidthExtra( pev ) ) );

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );
	
	if ( tr.pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if ( !pEntity )
			return NULL;

		if ( iDamage > 0 )
		{
			// TakeDamage can free the victim at once (a TakeDamage hook that removes it with REMOVE_ENTITY): the
			// handle checks the edict's serial number and private data, so a victim freed there comes back NULL
			// (a miss) instead of a pointer to freed memory
			EHANDLE hEntity;
			hEntity = pEntity;
			pEntity->TakeDamage( pev, pev, iDamage, iDmgType );
			return hEntity;
		}

		return pEntity;
	}

	return NULL;
}


//=========================================================
// FInViewCone - returns true is the passed ent is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
BOOL CBaseMonster :: FInViewCone ( CBaseEntity *pEntity )
{
	Vector2D	vec2LOS;
	float	flDot;

	UTIL_MakeVectors ( pev->angles );
	
	vec2LOS = ( pEntity->pev->origin - pev->origin ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );

	if ( flDot > m_flFieldOfView )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//=========================================================
// FInViewCone - returns true is the passed vector is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
BOOL CBaseMonster :: FInViewCone ( Vector *pOrigin )
{
	Vector2D	vec2LOS;
	float		flDot;

	UTIL_MakeVectors ( pev->angles );
	
	vec2LOS = ( *pOrigin - pev->origin ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );

	if ( flDot > m_flFieldOfView )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//=========================================================
// FVisible - returns true if a line can be traced from
// the caller's eyes to the target
//=========================================================
BOOL CBaseEntity :: FVisible ( CBaseEntity *pEntity )
{
	TraceResult tr;
	Vector		vecLookerOrigin;
	Vector		vecTargetOrigin;
	
	if (FBitSet( pEntity->pev->flags, FL_NOTARGET ))
		return FALSE;

	// don't look through water
	if ((pev->waterlevel != 3 && pEntity->pev->waterlevel == 3) 
		|| (pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
		return FALSE;

	vecLookerOrigin = pev->origin + pev->view_ofs;//look through the caller's 'eyes'
	vecTargetOrigin = pEntity->EyePosition();

	UTIL_TraceLine(vecLookerOrigin, vecTargetOrigin, ignore_monsters, ignore_glass, ENT(pev)/*pentIgnore*/, &tr);
	
	if (tr.flFraction != 1.0)
	{
		return FALSE;// Line of sight is not established
	}
	else
	{
		return TRUE;// line of sight is valid.
	}
}

//=========================================================
// FVisible - returns true if a line can be traced from
// the caller's eyes to the target vector
//=========================================================
BOOL CBaseEntity :: FVisible ( const Vector &vecOrigin )
{
	TraceResult tr;
	Vector		vecLookerOrigin;
	
	vecLookerOrigin = EyePosition();//look through the caller's 'eyes'

	UTIL_TraceLine(vecLookerOrigin, vecOrigin, ignore_monsters, ignore_glass, ENT(pev)/*pentIgnore*/, &tr);
	
	if (tr.flFraction != 1.0)
	{
		return FALSE;// Line of sight is not established
	}
	else
	{
		return TRUE;// line of sight is valid.
	}
}

/*
================
TraceAttack
================
*/
void CBaseEntity::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	Vector vecOrigin = ptr->vecEndPos - vecDir * 4;

	if ( pev->takedamage )
	{
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );

		int blood = BloodColor();
		
		if ( blood != DONT_BLEED )
		{
			SpawnBlood(vecOrigin, blood, flDamage);// a little surface blood.
			TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		}
	}
}


/*
//=========================================================
// TraceAttack
//=========================================================
void CBaseMonster::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	Vector vecOrigin = ptr->vecEndPos - vecDir * 4;

	ALERT ( at_console, "%d\n", ptr->iHitgroup );


	if ( pev->takedamage )
	{
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );

		int blood = BloodColor();
		
		if ( blood != DONT_BLEED )
		{
			SpawnBlood(vecOrigin, blood, flDamage);// a little surface blood.
		}
	}
}
*/

//=========================================================
// TraceAttack
//=========================================================
void CBaseMonster :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if ( pev->takedamage )
	{
		m_LastHitGroup = ptr->iHitgroup;

		switch ( ptr->iHitgroup )
		{
		case HITGROUP_GENERIC:
			break;
		case HITGROUP_HEAD:
			if ( !strcmp( STRING( pevAttacker->classname ), "player" ) )
				ClientPrint( pevAttacker, HUD_PRINTCENTER, "Head Shot!!!!" );
			flDamage *= 5;
			break;
		case HITGROUP_CHEST:
			flDamage *= 2;
			break;
		case HITGROUP_STOMACH:
			flDamage *= 3;
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			flDamage *= 0.5f;
			break;
		default:
			break;
		}

		SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage);// a little surface blood.
		TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	}
}

/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.

This version is used by Monsters.
================
*/
void CBaseEntity::FireBullets(ULONG cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t *pevAttacker, int bitsGib )
{
	static int tracerCount;
	int tracer;
	TraceResult tr;
	Vector vecSmoke = tr.vecEndPos + tr.vecPlaneNormal * 10;	// SBO: computed before any trace (uninitialized tr)
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	if ( pevAttacker == NULL )
		pevAttacker = pev;  // the default attacker is ourselves

	ClearMultiDamage();
	// SBO 2.1: with bitsGib (func_tank "bullet_gib_policy" 1 Normal: 0) the register does not start as DMG_NEVERGIB, so
	// a Normal hit gibs by the health it leaves (GIB_NORMAL), as in Featureful; the NEVERGIB start made it never gib
	gMultiDamage.type = DMG_BULLET | ( bitsGib >= 0 ? 0 : DMG_NEVERGIB );

	for (ULONG iShot = 1; iShot <= cShots; iShot++)
	{
		// get circular gaussian spread
		float x, y, z;
		do {
			x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			z = x*x+y*y;
		} while (z > 1);

		Vector vecDir = vecDirShooting +
						x * vecSpread.x * vecRight +
						y * vecSpread.y * vecUp;
		Vector vecEnd;

		vecEnd = vecSrc + vecDir * flDistance;
		UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev)/*pentIgnore*/, &tr);

		tracer = 0;
		if (iTracerFreq != 0 && (tracerCount++ % iTracerFreq) == 0)
		{
			Vector vecTracerSrc;

			if ( IsPlayer() )
			{// adjust tracer position for player
				vecTracerSrc = vecSrc + Vector ( 0 , 0 , -4 ) + gpGlobals->v_right * 2 + gpGlobals->v_forward * 16;
			}
			else
			{
				vecTracerSrc = vecSrc;
			}
			
			if ( iTracerFreq != 1 )		// guns that always trace also always decal
				tracer = 1;
			switch( iBulletType )
			{
			case BULLET_MONSTER_MP5:
			case BULLET_MONSTER_9MM:
			case BULLET_MONSTER_12MM:
			default:
				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecTracerSrc );
					WRITE_BYTE( TE_TRACER );
					WRITE_COORD( vecTracerSrc.x );
					WRITE_COORD( vecTracerSrc.y );
					WRITE_COORD( vecTracerSrc.z );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
				MESSAGE_END();
				break;
			}
		}
		// do damage, paint decals
		if (tr.flFraction != 1.0)
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

			if ( iDamage )
			{
				// SBO 2.0: bitsGib (func_tank "bullet_gib_policy") replaces the gib bits
				pEntity->TraceAttack(pevAttacker, iDamage, vecDir, &tr, DMG_BULLET | ( bitsGib >= 0 ? bitsGib : ((iDamage > 16) ? DMG_ALWAYSGIB : DMG_NEVERGIB) ) );
				
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				DecalGunshot( &tr, iBulletType );
			} 
			else switch(iBulletType)
			{
			// SBO: random damage per bullet type, stored in (reused) gSkillData fields
			default:
			case BULLET_MONSTER_9MM:
				gSkillData.slaveHealth = RANDOM_FLOAT( 8, 22 );
				pEntity->TraceAttack(pevAttacker, gSkillData.slaveHealth, vecDir, &tr, DMG_BULLET | ( bitsGib > 0 ? bitsGib : 0 ));
				
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				DecalGunshot( &tr, iBulletType );

				break;

			case BULLET_MONSTER_MP5:
				gSkillData.slaveDmgClaw = RANDOM_FLOAT( 12, 29 );
				pEntity->TraceAttack(pevAttacker, gSkillData.slaveDmgClaw, vecDir, &tr, DMG_BULLET | ( bitsGib > 0 ? bitsGib : 0 ));
				
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				DecalGunshot( &tr, iBulletType );

				break;

			case BULLET_MONSTER_12MM:		
				gSkillData.slaveDmgClawrake = RANDOM_FLOAT( 21, 56 );
				pEntity->TraceAttack(pevAttacker, gSkillData.slaveDmgClawrake, vecDir, &tr, DMG_BULLET | ( bitsGib > 0 ? bitsGib : 0 )); 
				if ( !tracer )
				{
					TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
					DecalGunshot( &tr, iBulletType );
				}
				break;

			case BULLET_PLAYER_FA18:
				gSkillData.bullsquidDmgBite = RANDOM_FLOAT( 26, 43 );
				pEntity->TraceAttack(pevAttacker, gSkillData.bullsquidDmgBite, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_SPA:
				gSkillData.hgruntGrenadeSpeed = RANDOM_FLOAT( 30, 45 );
				pEntity->TraceAttack(pevAttacker, gSkillData.hgruntGrenadeSpeed, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_SW2:
				gSkillData.bullsquidDmgWhip = RANDOM_FLOAT( 88, 172 );
				pEntity->TraceAttack(pevAttacker, gSkillData.bullsquidDmgWhip, vecDir, &tr, DMG_BULLET);
				UTIL_DecalTrace( &tr, DECAL_BIGSHOT4 );

				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_GLOWSPRITE );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
					WRITE_SHORT( m_blastglow );
					WRITE_BYTE( 30 );
					WRITE_BYTE( 2 );
					WRITE_BYTE( 3000 );
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_SPRITETRAIL );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
					WRITE_COORD( tr.vecEndPos.x + tr.vecPlaneNormal.x );
					WRITE_COORD( tr.vecEndPos.y + tr.vecPlaneNormal.y );
					WRITE_COORD( tr.vecEndPos.z + tr.vecPlaneNormal.z );
					WRITE_SHORT( m_blastballs );
					WRITE_BYTE( 5 ); // count
					WRITE_BYTE( 5 ); // life
					WRITE_BYTE( RANDOM_LONG( 1, 2 ) ); // scale
					WRITE_BYTE( 10 ); // velocity
					WRITE_BYTE( 5 ); // randomness
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSmoke );
					WRITE_BYTE( TE_SPRITE );
					WRITE_COORD( vecSmoke.x );
					WRITE_COORD( vecSmoke.y );
					WRITE_COORD( vecSmoke.z + 16 );
					WRITE_SHORT( m_pWallSmoke );
					WRITE_BYTE( 10 ); // scale
					WRITE_BYTE( 255 ); // brightness
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
					WRITE_BYTE( TE_DLIGHT );
					WRITE_COORD( pev->origin.x );
					WRITE_COORD( pev->origin.y );
					WRITE_COORD( pev->origin.z );
					WRITE_BYTE( 20 ); // radius
					WRITE_BYTE( 138 ); // r
					WRITE_BYTE( 249 ); // g
					WRITE_BYTE( 227 ); // b
					WRITE_BYTE( 2 ); // life
					WRITE_BYTE( 0 ); // decay
				MESSAGE_END();
				break;

			case BULLET_PLAYER_CHAINGUN:
				gSkillData.bullsquidDmgSpit = RANDOM_FLOAT( 12, 25 );
				pEntity->TraceAttack(pevAttacker, gSkillData.bullsquidDmgSpit, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_RC28:
				gSkillData.gargantuaDmgStomp = RANDOM_FLOAT( 26, 41 );
				pEntity->TraceAttack(pevAttacker, gSkillData.gargantuaDmgStomp, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_444:
				gSkillData.hassassinHealth = RANDOM_FLOAT( 150, 270 );
				pEntity->TraceAttack(pevAttacker, gSkillData.hassassinHealth, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_BEN:
				gSkillData.headcrabHealth = RANDOM_FLOAT( 98, 122 );
				pEntity->TraceAttack(pevAttacker, gSkillData.headcrabHealth, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_NM33:
				gSkillData.headcrabDmgBite = RANDOM_FLOAT( 38, 60 );
				pEntity->TraceAttack(pevAttacker, gSkillData.headcrabDmgBite, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_FEDERAL:
				gSkillData.hgruntHealth = RANDOM_FLOAT( 30, 52 );
				pEntity->TraceAttack(pevAttacker, gSkillData.hgruntHealth, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_PM66:
				gSkillData.hgruntDmgKick = RANDOM_FLOAT( 20, 37 );
				pEntity->TraceAttack(pevAttacker, gSkillData.hgruntDmgKick, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_15:
				if ( m_Mode == MODE_COOP )
					gSkillData.hgruntShotgunPellets = RANDOM_FLOAT( 16, 29 );
				else
					gSkillData.hgruntShotgunPellets = RANDOM_FLOAT( 24, 38 );
				pEntity->TraceAttack(pevAttacker, gSkillData.hgruntShotgunPellets, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_NONE: // FIX 
				pEntity->TraceAttack(pevAttacker, 50, vecDir, &tr, DMG_CLUB);
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				// only decal glass
				if ( !FNullEnt(tr.pHit) && VARS(tr.pHit)->rendermode != 0)
				{
					UTIL_DecalTrace( &tr, DECAL_GLASSBREAK1 + RANDOM_LONG(0,2) );
				}

				break;
			}
		}
		// make bullet trails
		UTIL_BubbleTrail( vecSrc, tr.vecEndPos, (flDistance * tr.flFraction) / 64.0 );
	}
	ApplyMultiDamage(pev, pevAttacker);
}


/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.

This version is used by Players, uses the random seed generator to sync client and server side shots.
================
*/
Vector CBaseEntity::FireBulletsPlayer ( ULONG cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t *pevAttacker, int shared_rand )
{
	static int tracerCount;
	TraceResult tr;
	Vector vecRight = gpGlobals->v_right;
	Vector vecSmoke = tr.vecEndPos + tr.vecPlaneNormal * 10;	// SBO: computed before any trace (uninitialized tr)
	Vector vecUp = gpGlobals->v_up;
	float x, y;

	if ( pevAttacker == NULL )
		pevAttacker = pev;  // the default attacker is ourselves

	ClearMultiDamage();
	gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB;

	for ( ULONG iShot = 1; iShot <= cShots; iShot++ )
	{
		//Use player's random seed.
		// get circular gaussian spread
		x = UTIL_SharedRandomFloat( shared_rand + iShot, -0.5, 0.5 ) + UTIL_SharedRandomFloat( shared_rand + ( 1 + iShot ) , -0.5, 0.5 );
		y = UTIL_SharedRandomFloat( shared_rand + ( 2 + iShot ), -0.5, 0.5 ) + UTIL_SharedRandomFloat( shared_rand + ( 3 + iShot ), -0.5, 0.5 );

		Vector vecDir = vecDirShooting +
						x * vecSpread.x * vecRight +
						y * vecSpread.y * vecUp;
		Vector vecEnd;

		vecEnd = vecSrc + vecDir * flDistance;
		UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev)/*pentIgnore*/, &tr);
		
		// do damage, paint decals
		if (tr.flFraction != 1.0)
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

			if ( iDamage )
			{
				pEntity->TraceAttack(pevAttacker, iDamage, vecDir, &tr, DMG_BULLET | ((iDamage > 16) ? DMG_ALWAYSGIB : DMG_NEVERGIB) );
				
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				DecalGunshot( &tr, iBulletType );
			} 
			else switch(iBulletType)
			{
			// SBO: random damage per bullet type, stored in (reused) gSkillData fields
			default:
			case BULLET_PLAYER_FA18:
				gSkillData.bullsquidDmgBite = RANDOM_FLOAT( 26, 43 );
				pEntity->TraceAttack(pevAttacker, gSkillData.bullsquidDmgBite, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_SPA:
				gSkillData.hgruntGrenadeSpeed = RANDOM_FLOAT( 30, 45 );
				pEntity->TraceAttack(pevAttacker, gSkillData.hgruntGrenadeSpeed, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_SW2:
				gSkillData.bullsquidDmgWhip = RANDOM_FLOAT( 88, 172 );
				pEntity->TraceAttack(pevAttacker, gSkillData.bullsquidDmgWhip, vecDir, &tr, DMG_BULLET);
				UTIL_DecalTrace( &tr, DECAL_BIGSHOT4 );

				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_GLOWSPRITE );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
					WRITE_SHORT( m_blastglow );
					WRITE_BYTE( 30 );
					WRITE_BYTE( 2 );
					WRITE_BYTE( 3000 );
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_SPRITETRAIL );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
					WRITE_COORD( tr.vecEndPos.x + tr.vecPlaneNormal.x );
					WRITE_COORD( tr.vecEndPos.y + tr.vecPlaneNormal.y );
					WRITE_COORD( tr.vecEndPos.z + tr.vecPlaneNormal.z );
					WRITE_SHORT( m_blastballs );
					WRITE_BYTE( 5 ); // count
					WRITE_BYTE( 5 ); // life
					WRITE_BYTE( RANDOM_LONG( 1, 2 ) ); // scale
					WRITE_BYTE( 10 ); // velocity
					WRITE_BYTE( 5 ); // randomness
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSmoke );
					WRITE_BYTE( TE_SPRITE );
					WRITE_COORD( vecSmoke.x );
					WRITE_COORD( vecSmoke.y );
					WRITE_COORD( vecSmoke.z + 16 );
					WRITE_SHORT( m_pWallSmoke );
					WRITE_BYTE( 10 ); // scale
					WRITE_BYTE( 255 ); // brightness
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
					WRITE_BYTE( TE_DLIGHT );
					WRITE_COORD( pev->origin.x );
					WRITE_COORD( pev->origin.y );
					WRITE_COORD( pev->origin.z );
					WRITE_BYTE( 20 ); // radius
					WRITE_BYTE( 138 ); // r
					WRITE_BYTE( 249 ); // g
					WRITE_BYTE( 227 ); // b
					WRITE_BYTE( 2 ); // life
					WRITE_BYTE( 0 ); // decay
				MESSAGE_END();
				break;

			case BULLET_PLAYER_CHAINGUN:
				gSkillData.bullsquidDmgSpit = RANDOM_FLOAT( 12, 25 );
				pEntity->TraceAttack(pevAttacker, gSkillData.bullsquidDmgSpit, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_RC28:
				gSkillData.gargantuaDmgStomp = RANDOM_FLOAT( 26, 41 );
				pEntity->TraceAttack(pevAttacker, gSkillData.gargantuaDmgStomp, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_444:
				gSkillData.hassassinHealth = RANDOM_FLOAT( 150, 270 );
				pEntity->TraceAttack(pevAttacker, gSkillData.hassassinHealth, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_BEN:
				gSkillData.headcrabHealth = RANDOM_FLOAT( 88, 122 );
				pEntity->TraceAttack(pevAttacker, gSkillData.headcrabHealth, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_NM33:
				gSkillData.headcrabDmgBite = RANDOM_FLOAT( 38, 60 );
				pEntity->TraceAttack(pevAttacker, gSkillData.headcrabDmgBite, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_FEDERAL:
				gSkillData.hgruntHealth = RANDOM_FLOAT( 30, 52 );
				pEntity->TraceAttack(pevAttacker, gSkillData.hgruntHealth, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_PM66:
				gSkillData.hgruntDmgKick = RANDOM_FLOAT( 20, 37 );
				pEntity->TraceAttack(pevAttacker, gSkillData.hgruntDmgKick, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_PLAYER_15:
				if ( m_Mode == MODE_COOP )
					gSkillData.hgruntShotgunPellets = RANDOM_FLOAT( 16, 29 );
				else
					gSkillData.hgruntShotgunPellets = RANDOM_FLOAT( 24, 38 );
				pEntity->TraceAttack(pevAttacker, gSkillData.hgruntShotgunPellets, vecDir, &tr, DMG_BULLET);
				break;

			case BULLET_NONE: // FIX 
				pEntity->TraceAttack(pevAttacker, 50, vecDir, &tr, DMG_CLUB);
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				// only decal glass
				if ( !FNullEnt(tr.pHit) && VARS(tr.pHit)->rendermode != 0)
				{
					UTIL_DecalTrace( &tr, DECAL_GLASSBREAK1 + RANDOM_LONG(0,2) );
				}

				break;
			}
		}
		// make bullet trails
		UTIL_BubbleTrail( vecSrc, tr.vecEndPos, (flDistance * tr.flFraction) / 64.0 );
	}
	ApplyMultiDamage(pev, pevAttacker);

	return Vector( x * vecSpread.x, y * vecSpread.y, 0.0 );
}

//=========================================================
// SBO: CPlasma - plasma bolt fired by the SW2 (m_iType 1..3:
// 1 bounces off walls and burns, 2 burns, other types just vanish)
//=========================================================
LINK_ENTITY_TO_CLASS( plasma, CPlasma );

void CPlasma::Spawn( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "plasma" );
	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;

	// SBO: no breaks (each type also runs the following ones), and Shoot sets m_iType only after Spawn
	switch ( m_iType )
	{
	case 1:
		SET_MODEL( ENT(pev), "sprites/tele1.spr" );
		pev->frame = 0;
		pev->scale = 0.1;
		pev->rendermode = kRenderTransAdd;
		pev->rendercolor.x = 138;
		pev->rendercolor.y = 249;
		pev->rendercolor.z = 227;
		pev->renderamt = 255;
		UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMFOLLOW );
			WRITE_SHORT( entindex() );	// entity
			WRITE_SHORT( m_iPTrail );	// model
			WRITE_BYTE( 1 ); // life
			WRITE_BYTE( 1 );  // width
			WRITE_BYTE( 138 );   // r, g, b
			WRITE_BYTE( 249 );   // r, g, b
			WRITE_BYTE( 227 );   // r, g, b
			WRITE_BYTE( 255 );	// brightness
		MESSAGE_END();
	case 2:
		SET_MODEL( ENT(pev), "sprites/tele1.spr" );
		pev->frame = 0;
		pev->scale = 2.0;
		pev->rendermode = kRenderTransAdd;
		pev->rendercolor.x = 250;
		pev->rendercolor.y = 200;
		pev->rendercolor.z = 0;
		pev->renderamt = 255;
		UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMFOLLOW );
			WRITE_SHORT( entindex() );	// entity
			WRITE_SHORT( m_iPTrail );	// model
			WRITE_BYTE( 1 ); // life
			WRITE_BYTE( 1 );  // width
			WRITE_BYTE( 250 );   // r, g, b
			WRITE_BYTE( 200 );   // r, g, b
			WRITE_BYTE( 0 );   // r, g, b
			WRITE_BYTE( 255 );	// brightness
		MESSAGE_END();
	case 3:
		SET_MODEL( ENT(pev), "sprites/tele1.spr" );
		pev->frame = 0;
		pev->scale = 0.5;
		pev->rendermode = kRenderTransAdd;
		pev->rendercolor.x = 100;
		pev->rendercolor.y = 255;
		pev->rendercolor.z = 200;
		pev->renderamt = 255;
		UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMFOLLOW );
			WRITE_SHORT( entindex() );	// entity
			WRITE_SHORT( m_iPTrail );	// model
			WRITE_BYTE( 1 ); // life
			WRITE_BYTE( 1 );  // width
			WRITE_BYTE( 138 );   // r, g, b
			WRITE_BYTE( 249 );   // r, g, b
			WRITE_BYTE( 227 );   // r, g, b
			WRITE_BYTE( 255 );	// brightness
		MESSAGE_END();
	default:
		SET_MODEL( ENT(pev), "sprites/tele1.spr" );
		pev->frame = 0;
		pev->scale = 0.1;
		pev->rendermode = kRenderTransAdd;
		pev->rendercolor.x = 138;
		pev->rendercolor.y = 249;
		pev->rendercolor.z = 227;
		pev->renderamt = 255;
		UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMFOLLOW );
			WRITE_SHORT( entindex() );	// entity
			WRITE_SHORT( m_iPTrail );	// model
			WRITE_BYTE( 1 ); // life
			WRITE_BYTE( 1 );  // width
			WRITE_BYTE( 138 );   // r, g, b
			WRITE_BYTE( 249 );   // r, g, b
			WRITE_BYTE( 227 );   // r, g, b
			WRITE_BYTE( 255 );	// brightness
		MESSAGE_END();
	}

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	m_flNextSound = 0;
}


void CPlasma::Animate( void )
{
	if ( m_iType == 1 || m_iType == 3 )
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_BYTE( 15 ); // radius
			WRITE_BYTE( 138 ); // r
			WRITE_BYTE( 249 ); // g
			WRITE_BYTE( 227 ); // b
			WRITE_BYTE( 0 ); // life
			WRITE_BYTE( 0 ); // decay
		MESSAGE_END();
	}

	pev->nextthink = gpGlobals->time;

	if ( m_flNextSound < gpGlobals->time )
	{
		m_flNextSound = gpGlobals->time + 0.1;
		Vector vecVelocity;
		int pitch = RANDOM_FLOAT( 90, 110 );

		if ( pev->frame++ && pev->frame > m_maxFrame )
			pev->frame = 0;

		if ( m_fBounced )
		{
			pev->velocity = m_vecVelocity;
			m_fBounced = 0;
		}

		vecVelocity = pev->velocity;
		if ( vecVelocity.Length() < 1000 )
		{
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/zong.wav", 1.0, 0.8, 0, pitch );

			Vector vecSrc = pev->origin;
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
				WRITE_BYTE( TE_SPRITE );
				WRITE_COORD( vecSrc.x );
				WRITE_COORD( vecSrc.y );
				WRITE_COORD( vecSrc.z + 6 );
				WRITE_SHORT( m_psmoke );
				WRITE_BYTE( 3 ); // scale
				WRITE_BYTE( 255 ); // brightness
			MESSAGE_END();

			SetThink( &CPlasma::SUB_Remove );
			pev->nextthink = gpGlobals->time;
		}
	}
}


void CPlasma::Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, int iType )
{
	CPlasma *pPlasma = GetClassPtr( (CPlasma *)NULL );
	pPlasma->Spawn();
	pPlasma->m_iType = iType;

	UTIL_SetOrigin( pPlasma->pev, vecStart );
	pPlasma->pev->velocity = vecVelocity;
	pPlasma->pev->owner = ENT(pevOwner);

	pPlasma->SetThink( &CPlasma::Animate );
	pPlasma->pev->nextthink = gpGlobals->time + 0.1;
}


void CPlasma::Touch( CBaseEntity *pOther )
{
	TraceResult tr;
	Vector vecDir;
	Vector vecEnd;	// SBO: unused

	int pitch = RANDOM_FLOAT( 90, 110 );

	entvars_t *pevOwner = NULL;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	if ( !pev->owner )
		pevOwner = pev;

	switch ( m_iType )
	{
	case 1:
		if ( pOther->pev->takedamage == DAMAGE_NO )
		{
			int iBounce = RANDOM_FLOAT( 1, 20 );
			if ( iBounce <= 13 )
			{
				// bounce off the wall
				UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, ENT(pev), &tr );
				EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/zong.wav", 1.0, 0.8, 0, pitch );
				UTIL_DecalTrace( &tr, DECAL_BIGSHOT4 );

				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_GLOWSPRITE );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
					WRITE_SHORT( m_blastglow );
					WRITE_BYTE( 30 );
					WRITE_BYTE( 2 );
					WRITE_BYTE( 3000 );
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_SPRITETRAIL );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
					WRITE_COORD( tr.vecEndPos.x + tr.vecPlaneNormal.x );
					WRITE_COORD( tr.vecEndPos.y + tr.vecPlaneNormal.y );
					WRITE_COORD( tr.vecEndPos.z + tr.vecPlaneNormal.z );
					WRITE_SHORT( m_blastballs );
					WRITE_BYTE( 5 ); // count
					WRITE_BYTE( 5 ); // life
					WRITE_BYTE( RANDOM_LONG( 1, 2 ) ); // scale
					WRITE_BYTE( 10 ); // velocity
					WRITE_BYTE( 5 ); // randomness
				MESSAGE_END();

				Vector vecSmoke = tr.vecEndPos + tr.vecPlaneNormal * 10;
				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSmoke );
					WRITE_BYTE( TE_SPRITE );
					WRITE_COORD( vecSmoke.x );
					WRITE_COORD( vecSmoke.y );
					WRITE_COORD( vecSmoke.z + 16 );
					WRITE_SHORT( m_pWallSmoke );
					WRITE_BYTE( 10 ); // scale
					WRITE_BYTE( 255 ); // brightness
				MESSAGE_END();

				float flSpeed = pev->velocity.Length();
				m_vecVelocity = pev->velocity.Normalize();
				float flDot = -DotProduct( tr.vecPlaneNormal, m_vecVelocity );
				m_vecVelocity = 2 * tr.vecPlaneNormal * flDot + m_vecVelocity;
				m_vecVelocity.Normalize();
				m_vecVelocity = m_vecVelocity * flSpeed * 1;	// (bounce factor 1)

				UTIL_SetOrigin( pev, pev->origin + tr.vecPlaneNormal * 3 );
				pev->velocity = m_vecVelocity;
				m_fBounced = 1;
				return;
			}
			else
			{
				UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, ENT(pev), &tr );
				EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/zong.wav", 1.0, 0.8, 0, pitch );
				UTIL_DecalTrace( &tr, DECAL_BIGSHOT4 );

				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_GLOWSPRITE );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
					WRITE_SHORT( m_blastglow );
					WRITE_BYTE( 30 );
					WRITE_BYTE( 2 );
					WRITE_BYTE( 3000 );
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_SPRITETRAIL );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
					WRITE_COORD( tr.vecEndPos.x + tr.vecPlaneNormal.x );
					WRITE_COORD( tr.vecEndPos.y + tr.vecPlaneNormal.y );
					WRITE_COORD( tr.vecEndPos.z + tr.vecPlaneNormal.z );
					WRITE_SHORT( m_blastballs );
					WRITE_BYTE( 5 ); // count
					WRITE_BYTE( 5 ); // life
					WRITE_BYTE( RANDOM_LONG( 1, 2 ) ); // scale
					WRITE_BYTE( 10 ); // velocity
					WRITE_BYTE( 5 ); // randomness
				MESSAGE_END();

				Vector vecSmoke = tr.vecEndPos + tr.vecPlaneNormal * 10;
				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSmoke );
					WRITE_BYTE( TE_SPRITE );
					WRITE_COORD( vecSmoke.x );
					WRITE_COORD( vecSmoke.y );
					WRITE_COORD( vecSmoke.z + 16 );
					WRITE_SHORT( m_pWallSmoke );
					WRITE_BYTE( 10 ); // scale
					WRITE_BYTE( 255 ); // brightness
				MESSAGE_END();

				SetThink( &CPlasma::SUB_Remove );
				pev->nextthink = gpGlobals->time;
			}
		}
		else
		{
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/zong.wav", 1.0, 0.8, 0, pitch );
			Vector vecSrc = pev->origin;
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
				WRITE_BYTE( TE_SPRITE );
				WRITE_COORD( vecSrc.x );
				WRITE_COORD( vecSrc.y );
				WRITE_COORD( vecSrc.z + 6 );
				WRITE_SHORT( m_psmoke );
				WRITE_BYTE( 3 ); // scale
				WRITE_BYTE( 255 ); // brightness
			MESSAGE_END();

			UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, ENT(pev), &tr );
			vecDir = pev->velocity;
			vecDir.Normalize();

			ClearMultiDamage();
			gSkillData.bullsquidDmgWhip = RANDOM_FLOAT( 88, 172 );
			pOther->TraceAttack( pevOwner, gSkillData.bullsquidDmgWhip, vecDir, &tr, DMG_ENERGYBEAM );
			ApplyMultiDamage( pev, pevOwner );
		}

	case 2:
		if ( pOther->pev->takedamage == DAMAGE_NO )
		{
			UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, ENT(pev), &tr );
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/burn_fire1.wav", 1.0, 0.8, 0, pitch );
			UTIL_DecalTrace( &tr, DECAL_BIGSHOT4 );

			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
				WRITE_BYTE( TE_GLOWSPRITE );
				WRITE_COORD( tr.vecEndPos.x );
				WRITE_COORD( tr.vecEndPos.y );
				WRITE_COORD( tr.vecEndPos.z );
				WRITE_SHORT( m_blastglow );
				WRITE_BYTE( 30 );
				WRITE_BYTE( 2 );
				WRITE_BYTE( 3000 );
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
				WRITE_BYTE( TE_SPRITETRAIL );
				WRITE_COORD( tr.vecEndPos.x );
				WRITE_COORD( tr.vecEndPos.y );
				WRITE_COORD( tr.vecEndPos.z );
				WRITE_COORD( tr.vecEndPos.x + tr.vecPlaneNormal.x );
				WRITE_COORD( tr.vecEndPos.y + tr.vecPlaneNormal.y );
				WRITE_COORD( tr.vecEndPos.z + tr.vecPlaneNormal.z );
				WRITE_SHORT( m_blastballs );
				WRITE_BYTE( 5 ); // count
				WRITE_BYTE( 5 ); // life
				WRITE_BYTE( RANDOM_LONG( 1, 2 ) ); // scale
				WRITE_BYTE( 10 ); // velocity
				WRITE_BYTE( 5 ); // randomness
			MESSAGE_END();

			Vector vecSmoke = tr.vecEndPos + tr.vecPlaneNormal * 10;
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSmoke );
				WRITE_BYTE( TE_SPRITE );
				WRITE_COORD( vecSmoke.x );
				WRITE_COORD( vecSmoke.y );
				WRITE_COORD( vecSmoke.z + 16 );
				WRITE_SHORT( m_pWallSmoke );
				WRITE_BYTE( 10 ); // scale
				WRITE_BYTE( 255 ); // brightness
			MESSAGE_END();

			SetThink( &CPlasma::SUB_Remove );
			pev->nextthink = gpGlobals->time;
		}
		else
		{
			Vector vecSrc = pev->origin;
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
				WRITE_BYTE( TE_SPRITE );
				WRITE_COORD( vecSrc.x );
				WRITE_COORD( vecSrc.y );
				WRITE_COORD( vecSrc.z + 6 );
				WRITE_SHORT( m_psmoke );
				WRITE_BYTE( 3 ); // scale
				WRITE_BYTE( 255 ); // brightness
			MESSAGE_END();

			UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, ENT(pev), &tr );
			vecDir = pev->velocity;
			vecDir.Normalize();

			ClearMultiDamage();
			pOther->TraceAttack( pevOwner, RANDOM_FLOAT( 30, 50 ), vecDir, &tr, DMG_ENERGYBEAM );
			ApplyMultiDamage( pev, pevOwner );
		}

	default:
		SetThink( &CPlasma::SUB_Remove );
		pev->nextthink = gpGlobals->time;
	}
}


//=========================================================
// SBO: FireTWalls - shot that goes through walls (like the gauss), losing damage
//=========================================================
void CBasePlayer::FireTWalls( float flDamage )
{
	UTIL_MakeVectors( pev->v_angle + pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = GetGunPosition( );
	Vector vecDest = vecSrc + vecAiming * 8192;

	gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB;

	edict_t *pentIgnore = ENT( pev );
	TraceResult tr, beam_tr;

	UTIL_TraceLine( vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr );
	DecalGunshot( &tr, BULLET_MONSTER_9MM );
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
		WRITE_BYTE( TE_SPRITE );
		WRITE_COORD( tr.vecEndPos.x );
		WRITE_COORD( tr.vecEndPos.y );
		WRITE_COORD( tr.vecEndPos.z );
		WRITE_SHORT( g_sSmogo );
		WRITE_BYTE( 3 ); // scale
		WRITE_BYTE( 50 ); // brightness
	MESSAGE_END();

	CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
	if ( pEntity->pev->takedamage )
	{
		ClearMultiDamage();
		pEntity->TraceAttack( pev, flDamage, vecAiming, &tr, DMG_BULLET );
		ApplyMultiDamage( pev, pev );
	}

	// try to punch through the wall
	UTIL_TraceLine( tr.vecEndPos + vecAiming * 8, vecDest, dont_ignore_monsters, pentIgnore, &beam_tr );
	if ( !beam_tr.fAllSolid )
	{
		// trace backwards to find exit point
		UTIL_TraceLine( beam_tr.vecEndPos, tr.vecEndPos, dont_ignore_monsters, pentIgnore, &beam_tr );
		DecalGunshot( &beam_tr, BULLET_MONSTER_9MM );
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( tr.vecEndPos.x );
			WRITE_COORD( tr.vecEndPos.y );
			WRITE_COORD( tr.vecEndPos.z );
			WRITE_SHORT( g_sSmogo );
			WRITE_BYTE( 3 ); // scale
			WRITE_BYTE( 50 ); // brightness
		MESSAGE_END();

		flDamage -= (beam_tr.vecEndPos - tr.vecEndPos).Length();
		flDamage *= 0.5f;

		if ( !(flDamage < 1) )
		{
			UTIL_TraceLine( beam_tr.vecEndPos, vecDest, dont_ignore_monsters, pentIgnore, &beam_tr );
			DecalGunshot( &beam_tr, BULLET_MONSTER_9MM );
			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
				WRITE_BYTE( TE_SPRITE );
				WRITE_COORD( tr.vecEndPos.x );
				WRITE_COORD( tr.vecEndPos.y );
				WRITE_COORD( tr.vecEndPos.z );
				WRITE_SHORT( g_sSmogo );
				WRITE_BYTE( 3 ); // scale
				WRITE_BYTE( 50 ); // brightness
			MESSAGE_END();

			pEntity = CBaseEntity::Instance( beam_tr.pHit );
			if ( pEntity->pev->takedamage )
			{
				ClearMultiDamage();
				pEntity->TraceAttack( pev, flDamage, vecAiming, &beam_tr, DMG_BULLET );
				ApplyMultiDamage( pev, pev );
			}
		}
	}
}

void CBaseEntity :: TraceBleed( float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if (BloodColor() == DONT_BLEED)
		return;
	
	if (flDamage == 0)
		return;

	if (! (bitsDamageType & (DMG_CRUSH | DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_MORTAR)))
		return;
	
	// make blood decal on the wall! 
	TraceResult Bloodtr;
	Vector vecTraceDir; 
	float flNoise;
	int cCount;
	int i;

/*
	if ( !IsAlive() )
	{
		// dealing with a dead monster. 
		if ( pev->max_health <= 0 )
		{
			// no blood decal for a monster that has already decalled its limit.
			return; 
		}
		else
		{
			pev->max_health--;
		}
	}
*/

	if (flDamage < 10)
	{
		flNoise = 0.1;
		cCount = 1;
	}
	else if (flDamage < 25)
	{
		flNoise = 0.2;
		cCount = 2;
	}
	else
	{
		flNoise = 0.3;
		cCount = 4;
	}

	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir * -1;// trace in the opposite direction the shot came from (the direction the shot is going)

		vecTraceDir.x += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.y += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.z += RANDOM_FLOAT( -flNoise, flNoise );

		UTIL_TraceLine( ptr->vecEndPos, ptr->vecEndPos + vecTraceDir * -172, ignore_monsters, ENT(pev), &Bloodtr);

		if ( Bloodtr.flFraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}

//=========================================================
//=========================================================
void CBaseMonster :: MakeDamageBloodDecal ( int cCount, float flNoise, TraceResult *ptr, const Vector &vecDir )
{
	// make blood decal on the wall! 
	TraceResult Bloodtr;
	Vector vecTraceDir; 
	int i;

	if ( !IsAlive() )
	{
		// dealing with a dead monster. 
		if ( pev->max_health <= 0 )
		{
			// no blood decal for a monster that has already decalled its limit.
			return; 
		}
		else
		{
			pev->max_health--;
		}
	}

	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir;

		vecTraceDir.x += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.y += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.z += RANDOM_FLOAT( -flNoise, flNoise );

		UTIL_TraceLine( ptr->vecEndPos, ptr->vecEndPos + vecTraceDir * 172, ignore_monsters, ENT(pev), &Bloodtr);

/*
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_SHOWLINE);
			WRITE_COORD( ptr->vecEndPos.x );
			WRITE_COORD( ptr->vecEndPos.y );
			WRITE_COORD( ptr->vecEndPos.z );
			
			WRITE_COORD( Bloodtr.vecEndPos.x );
			WRITE_COORD( Bloodtr.vecEndPos.y );
			WRITE_COORD( Bloodtr.vecEndPos.z );
		MESSAGE_END();
*/

		if ( Bloodtr.flFraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}
