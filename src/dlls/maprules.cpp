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

//	-------------------------------------------
//
//	maprules.cpp
//
//	This module contains entities for implementing/changing game
//	rules dynamically within each map (.BSP)
//
//	-------------------------------------------

#include "extdll.h"
#include "eiface.h"
#include "util.h"
#include "gamerules.h"
#include "maprules.h"
#include "cbase.h"
#include "player.h"
#include "video_shared.h"	// game_shared: game_playvideo names

extern int gmsgPlayVideo;	// player.cpp: game_playvideo
#include "weapons.h"		// SBO 2.0: game_player_settings
#include "round_hooks.h"	// SBO 2.0: Logic_IsPlayerInPlay

class CRuleEntity : public CBaseEntity
{
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void	SetMaster( int iszMaster ) { m_iszMaster = iszMaster; }

protected:
	BOOL	CanFireForActivator( CBaseEntity *pActivator );

private:
	string_t	m_iszMaster;
};

// SBO: coop mission state (read by coop.cpp)
int g_bMissionOutcomePending = 0;
int g_bMissionFailed = 0;

int m_SpawnLocate;	// SBO: multispawn rotation (global .bss, owned here)

TYPEDESCRIPTION	CRuleEntity::m_SaveData[] = 
{
	DEFINE_FIELD( CRuleEntity, m_iszMaster, FIELD_STRING),
};

IMPLEMENT_SAVERESTORE( CRuleEntity, CBaseEntity );


void CRuleEntity::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= EF_NODRAW;
}


void CRuleEntity::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "master"))
	{
		SetMaster( ALLOC_STRING(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

BOOL CRuleEntity::CanFireForActivator( CBaseEntity *pActivator )
{
	if ( m_iszMaster )
	{
		if ( UTIL_IsMasterTriggered( m_iszMaster, pActivator ) )
			return TRUE;
		else
			return FALSE;
	}
	
	return TRUE;
}

// 
// CRulePointEntity -- base class for all rule "point" entities (not brushes)
//
class CRulePointEntity : public CRuleEntity
{
public:
	void		Spawn( void );
};

void CRulePointEntity::Spawn( void )
{
	CRuleEntity::Spawn();
	pev->frame			= 0;
	pev->model			= 0;
}

// 
// CRuleBrushEntity -- base class for all rule "brush" entities (not brushes)
// Default behavior is to set up like a trigger, invisible, but keep the model for volume testing
//
class CRuleBrushEntity : public CRuleEntity
{
public:
	void		Spawn( void );
	// halflife-updated #241: don't carry brush rule entities (game_zone_player) across level transitions
	int			ObjectCaps( void ) { return CRuleEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

private:
};

void CRuleBrushEntity::Spawn( void )
{
	SET_MODEL( edict(), STRING(pev->model) );
	CRuleEntity::Spawn();
}


// CGameScore / game_score	-- award points to player / team 
//	Points +/- total
//	Flag: Allow negative scores					SF_SCORE_NEGATIVE
//	Flag: Award points to team in teamplay		SF_SCORE_TEAM

#define SF_SCORE_NEGATIVE			0x0001
#define SF_SCORE_TEAM				0x0002

class CGameScore : public CRulePointEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	inline	int		Points( void ) { return pev->frags; }
	inline	BOOL	AllowNegativeScore( void ) { return pev->spawnflags & SF_SCORE_NEGATIVE; }
	inline	BOOL	AwardToTeam( void ) { return pev->spawnflags & SF_SCORE_TEAM; }

	inline	void	SetPoints( int points ) { pev->frags = points; }

private:
};

LINK_ENTITY_TO_CLASS( game_score, CGameScore );


void CGameScore::Spawn( void )
{
	CRulePointEntity::Spawn();
}


void CGameScore::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "points"))
	{
		SetPoints( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CRulePointEntity::KeyValue( pkvd );
}



void CGameScore::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	// Only players can use this
	if ( pActivator && pActivator->IsPlayer() )	// halflife-updated #246: null activator
	{
		if ( AwardToTeam() )
		{
			pActivator->AddPointsToTeam( Points(), AllowNegativeScore() );
		}
		else
		{
			pActivator->AddPoints( Points(), AllowNegativeScore() );
		}
	}
}


// CGameEnd / game_end	-- Ends the game in MP

class CGameEnd : public CRulePointEntity
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
private:
};

LINK_ENTITY_TO_CLASS( game_end, CGameEnd );


void CGameEnd::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	// SBO: in coop, game_end means the mission was accomplished
	if ( m_Mode == MODE_COOP )
	{
		g_bMissionOutcomePending = 1;
		g_bMissionFailed = 0;
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "SUCCESSFULL MISSION\n" );
	}
}


//
// CMissionFail / mission_failed	-- SBO: ends a coop mission as failed
//
class CMissionFail : public CRulePointEntity
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
private:
};

LINK_ENTITY_TO_CLASS( mission_failed, CMissionFail );


void CMissionFail::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( m_Mode == MODE_COOP )
	{
		g_bMissionOutcomePending = 1;
		g_bMissionFailed = 1;
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED MISSION\n" );
	}
}


//
// CMultiSpawn / multispawn	-- SBO: moves the coop spawn location on to the next set
//
class CMultiSpawn : public CRulePointEntity
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
private:
};

LINK_ENTITY_TO_CLASS( multispawn, CMultiSpawn );


void CMultiSpawn::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( m_Mode == MODE_COOP )
	{
		switch ( m_SpawnLocate )
		{
		case 1:
			m_SpawnLocate = 2;
			break;
		case 2:
			m_SpawnLocate = 3;
			break;
		case 3:
			m_SpawnLocate = 4;
			break;
		case 4:
		case 5:
			m_SpawnLocate = 5;
			break;
		default:
			m_SpawnLocate = 1;
			break;
		}
	}
}


//
// CGameText / game_text	-- NON-Localized HUD Message (use env_message to display a titles.txt message)
//	Flag: All players					SF_ENVTEXT_ALLPLAYERS
//


#define SF_ENVTEXT_ALLPLAYERS			0x0001


class CGameText : public CRulePointEntity
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	inline	BOOL	MessageToAll( void ) { return (pev->spawnflags & SF_ENVTEXT_ALLPLAYERS); }
	inline	void	MessageSet( const char *pMessage ) { pev->message = ALLOC_STRING(pMessage); }
	inline	const char *MessageGet( void )	{ return STRING(pev->message); }

private:

	hudtextparms_t	m_textParms;
};

LINK_ENTITY_TO_CLASS( game_text, CGameText );

// Save parms as a block.  Will break save/restore if the structure changes, but this entity didn't ship with Half-Life, so
// it can't impact saved Half-Life games.
TYPEDESCRIPTION	CGameText::m_SaveData[] = 
{
	DEFINE_ARRAY( CGameText, m_textParms, FIELD_CHARACTER, sizeof(hudtextparms_t) ),
};

IMPLEMENT_SAVERESTORE( CGameText, CRulePointEntity );


void CGameText::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "channel"))
	{
		m_textParms.channel = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "x"))
	{
		m_textParms.x = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "y"))
	{
		m_textParms.y = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "effect"))
	{
		m_textParms.effect = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "color"))
	{
		int color[4];
		UTIL_StringToIntArray( color, 4, pkvd->szValue );
		m_textParms.r1 = color[0];
		m_textParms.g1 = color[1];
		m_textParms.b1 = color[2];
		m_textParms.a1 = color[3];
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "color2"))
	{
		int color[4];
		UTIL_StringToIntArray( color, 4, pkvd->szValue );
		m_textParms.r2 = color[0];
		m_textParms.g2 = color[1];
		m_textParms.b2 = color[2];
		m_textParms.a2 = color[3];
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fadein"))
	{
		m_textParms.fadeinTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fadeout"))
	{
		m_textParms.fadeoutTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		m_textParms.holdTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fxtime"))
	{
		m_textParms.fxTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CRulePointEntity::KeyValue( pkvd );
}


void CGameText::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( MessageToAll() )
	{
		UTIL_HudMessageAll( m_textParms, MessageGet() );
	}
	else
	{
		if ( pActivator && pActivator->IsNetClient() )	// same null-activator guard as game_score (halflife-updated #246)
		{
			UTIL_HudMessage( pActivator, m_textParms, MessageGet() );
		}
	}
}


//
// CGameTeamMaster / game_team_master	-- "Masters" like multisource, but based on the team of the activator
// Only allows mastered entity to fire if the team matches my team
//
// team index (pulled from server team list "mp_teamlist"
// Flag: Remove on Fire
// Flag: Any team until set?		-- Any team can use this until the team is set (otherwise no teams can use it)
//

#define SF_TEAMMASTER_FIREONCE			0x0001
#define SF_TEAMMASTER_ANYTEAM			0x0002

class CGameTeamMaster : public CRulePointEntity
{
public:
	void		KeyValue( KeyValueData *pkvd );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int			ObjectCaps( void ) { return CRulePointEntity:: ObjectCaps() | FCAP_MASTER; }

	BOOL		IsTriggered( CBaseEntity *pActivator );
	const char	*TeamID( void );
	inline BOOL RemoveOnFire( void ) { return (pev->spawnflags & SF_TEAMMASTER_FIREONCE) ? TRUE : FALSE; }
	inline BOOL AnyTeam( void ) { return (pev->spawnflags & SF_TEAMMASTER_ANYTEAM) ? TRUE : FALSE; }

private:
	BOOL		TeamMatch( CBaseEntity *pActivator );

	int			m_teamIndex;
	USE_TYPE	triggerType;
};

LINK_ENTITY_TO_CLASS( game_team_master, CGameTeamMaster );

void CGameTeamMaster::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "teamindex"))
	{
		m_teamIndex = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		int type = atoi( pkvd->szValue );
		switch( type )
		{
		case 0:
			triggerType = USE_OFF;
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else
		CRulePointEntity::KeyValue( pkvd );
}


void CGameTeamMaster::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( useType == USE_SET )
	{
		if ( value < 0 )
		{
			m_teamIndex = -1;
		}
		else
		{
			m_teamIndex = g_pGameRules->GetTeamIndex( pActivator->TeamID() );
		}
		return;
	}

	if ( TeamMatch( pActivator ) )
	{
		SUB_UseTargets( pActivator, triggerType, value );
		if ( RemoveOnFire() )
			UTIL_Remove( this );
	}
}


BOOL CGameTeamMaster::IsTriggered( CBaseEntity *pActivator )
{
	return TeamMatch( pActivator );
}


const char *CGameTeamMaster::TeamID( void )
{
	if ( m_teamIndex < 0 )		// Currently set to "no team"
		return "";

	return g_pGameRules->GetIndexedTeamName( m_teamIndex );		// UNDONE: Fill this in with the team from the "teamlist"
}


BOOL CGameTeamMaster::TeamMatch( CBaseEntity *pActivator )
{
	if ( m_teamIndex < 0 && AnyTeam() )
		return TRUE;

	if ( !pActivator )
		return FALSE;

	return UTIL_TeamsMatch( pActivator->TeamID(), TeamID() );
}


//
// CGameTeamSet / game_team_set	-- Changes the team of the entity it targets to the activator's team
// Flag: Fire once
// Flag: Clear team				-- Sets the team to "NONE" instead of activator

#define SF_TEAMSET_FIREONCE			0x0001
#define SF_TEAMSET_CLEARTEAM		0x0002

class CGameTeamSet : public CRulePointEntity
{
public:
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	inline BOOL RemoveOnFire( void ) { return (pev->spawnflags & SF_TEAMSET_FIREONCE) ? TRUE : FALSE; }
	inline BOOL ShouldClearTeam( void ) { return (pev->spawnflags & SF_TEAMSET_CLEARTEAM) ? TRUE : FALSE; }

private:
};

LINK_ENTITY_TO_CLASS( game_team_set, CGameTeamSet );


void CGameTeamSet::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( ShouldClearTeam() )
	{
		SUB_UseTargets( pActivator, USE_SET, -1 );
	}
	else
	{
		SUB_UseTargets( pActivator, USE_SET, 0 );
	}

	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}


//
// CGamePlayerZone / game_player_zone -- players in the zone fire my target when I'm fired
//
// Needs master?
class CGamePlayerZone : public CRuleBrushEntity
{
public:
	void		KeyValue( KeyValueData *pkvd );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	string_t	m_iszInTarget;
	string_t	m_iszOutTarget;
	string_t	m_iszInCount;
	string_t	m_iszOutCount;
};

LINK_ENTITY_TO_CLASS( game_zone_player, CGamePlayerZone );
TYPEDESCRIPTION	CGamePlayerZone::m_SaveData[] = 
{
	DEFINE_FIELD( CGamePlayerZone, m_iszInTarget, FIELD_STRING ),
	DEFINE_FIELD( CGamePlayerZone, m_iszOutTarget, FIELD_STRING ),
	DEFINE_FIELD( CGamePlayerZone, m_iszInCount, FIELD_STRING ),
	DEFINE_FIELD( CGamePlayerZone, m_iszOutCount, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CGamePlayerZone, CRuleBrushEntity );

void CGamePlayerZone::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "intarget"))
	{
		m_iszInTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "outtarget"))
	{
		m_iszOutTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "incount"))
	{
		m_iszInCount = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "outcount"))
	{
		m_iszOutCount = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CRuleBrushEntity::KeyValue( pkvd );
}

void CGamePlayerZone::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int playersInCount = 0;
	int playersOutCount = 0;

	if ( !CanFireForActivator( pActivator ) )
		return;

	CBaseEntity *pPlayer = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );
		// SBO 2.0: spectators (spectator mode, observer.cpp) are not in the game: neither in nor out of the zone
		if ( pPlayer && !((CBasePlayer *)pPlayer)->m_bSpectating )
		{
			TraceResult trace;
			int			hullNumber;

			hullNumber = human_hull;
			if ( pPlayer->pev->flags & FL_DUCKING )
			{
				hullNumber = head_hull;
			}

			UTIL_TraceModel( pPlayer->pev->origin, pPlayer->pev->origin, hullNumber, edict(), &trace );

			if ( trace.fStartSolid )
			{
				playersInCount++;
				if ( m_iszInTarget )
				{
					FireTargets( STRING(m_iszInTarget), pPlayer, pActivator, useType, value );
				}
			}
			else
			{
				playersOutCount++;
				if ( m_iszOutTarget )
				{
					FireTargets( STRING(m_iszOutTarget), pPlayer, pActivator, useType, value );
				}
			}
		}
	}

	if ( m_iszInCount )
	{
		FireTargets( STRING(m_iszInCount), pActivator, this, USE_SET, playersInCount );
	}

	if ( m_iszOutCount )
	{
		FireTargets( STRING(m_iszOutCount), pActivator, this, USE_SET, playersOutCount );
	}
}



//
// CGamePlayerHurt / game_player_hurt	-- Damages the player who fires it
// Flag: Fire once

#define SF_PKILL_FIREONCE			0x0001
class CGamePlayerHurt : public CRulePointEntity
{
public:
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	inline BOOL RemoveOnFire( void ) { return (pev->spawnflags & SF_PKILL_FIREONCE) ? TRUE : FALSE; }

private:
};

LINK_ENTITY_TO_CLASS( game_player_hurt, CGamePlayerHurt );


void CGamePlayerHurt::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( pActivator && pActivator->IsPlayer() )	// same null-activator guard as game_score (halflife-updated #246)
	{
		if ( pev->dmg < 0 )
			pActivator->TakeHealth( -pev->dmg, DMG_GENERIC );
		else
			pActivator->TakeDamage( pev, pev, pev->dmg, DMG_GENERIC );
	}
	
	SUB_UseTargets( pActivator, useType, value );

	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}



//
// CGameCounter / game_counter	-- Counts events and fires target
// Flag: Fire once
// Flag: Reset on Fire

#define SF_GAMECOUNT_FIREONCE			0x0001
#define SF_GAMECOUNT_RESET				0x0002
#define SF_GAMECOUNT_FIREOVERLIMIT		0x0004	// SBO 2.0, from Featureful: fire at the limit and above it; no map sets it

class CGameCounter : public CRulePointEntity
{
public:
	void		Spawn( void );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	inline BOOL RemoveOnFire( void ) { return (pev->spawnflags & SF_GAMECOUNT_FIREONCE) ? TRUE : FALSE; }
	inline BOOL ResetOnFire( void ) { return (pev->spawnflags & SF_GAMECOUNT_RESET) ? TRUE : FALSE; }

	inline void CountUp( void ) { pev->frags++; }
	inline void CountDown( void ) { pev->frags--; }
	inline void ResetCount( void ) { pev->frags = pev->dmg; }
	inline int  CountValue( void ) { return pev->frags; }
	inline int	LimitValue( void ) { return pev->health; }
	
	// SBO 2.0 (Featureful): "Fire if over limit" also fires each time the count goes past the limit
	inline BOOL HitLimit( void ) { return ( pev->spawnflags & SF_GAMECOUNT_FIREOVERLIMIT ) ? CountValue() >= LimitValue() : CountValue() == LimitValue(); }

private:

	inline void SetCountValue( int value ) { pev->frags = value; }
	inline void SetInitialValue( int value ) { pev->dmg = value; }

public:
	// SBO 2.0 Locus (Featureful): as a [LR] number, a game_counter is its count
	virtual BOOL CalcRatio( CBaseEntity *pLocus, float *outResult ) { *outResult = CountValue(); return TRUE; }
};

LINK_ENTITY_TO_CLASS( game_counter, CGameCounter );

void CGameCounter::Spawn( void )
{
	// Save off the initial count
	SetInitialValue( CountValue() );
	CRulePointEntity::Spawn();
}


void CGameCounter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	switch( useType )
	{
	case USE_ON:
	case USE_TOGGLE:
		CountUp();
		break;
	
	case USE_OFF:
		CountDown();
		break;

	case USE_SET:
		SetCountValue( (int)value );
		break;
	}
	
	if ( HitLimit() )
	{
		SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
		if ( RemoveOnFire() )
		{
			UTIL_Remove( this );
		}
		
		if ( ResetOnFire() )
		{
			ResetCount();
		}
	}
}



//
// CGameCounterSet / game_counter_set	-- Sets the counter's value
// Flag: Fire once

#define SF_GAMECOUNTSET_FIREONCE			0x0001

class CGameCounterSet : public CRulePointEntity
{
public:
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	inline BOOL RemoveOnFire( void ) { return (pev->spawnflags & SF_GAMECOUNTSET_FIREONCE) ? TRUE : FALSE; }

private:
};

LINK_ENTITY_TO_CLASS( game_counter_set, CGameCounterSet );


void CGameCounterSet::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	SUB_UseTargets( pActivator, USE_SET, pev->frags );

	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}


//
// CGamePlayerEquip / game_playerequip	-- Sets the default player equipment
// Flag: USE Only

#define SF_PLAYEREQUIP_USEONLY			0x0001
#define MAX_EQUIP		32

class CGamePlayerEquip : public CRulePointEntity
{
public:
	void		KeyValue( KeyValueData *pkvd );
	void		Touch( CBaseEntity *pOther );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	inline BOOL	UseOnly( void ) { return (pev->spawnflags & SF_PLAYEREQUIP_USEONLY) ? TRUE : FALSE; }

private:

	void		EquipPlayer( CBaseEntity *pPlayer );

	string_t	m_weaponNames[MAX_EQUIP];
	int			m_weaponCount[MAX_EQUIP];
};

LINK_ENTITY_TO_CLASS( game_player_equip, CGamePlayerEquip );

#define EQUIP_MAX_COUNT	100	// items of one kind per game_player_equip key (well above the maps' 20)


void CGamePlayerEquip::KeyValue( KeyValueData *pkvd )
{
	CRulePointEntity::KeyValue( pkvd );

	if ( !pkvd->fHandled )
	{
		// SBO 2.0: only items are given (weapon_, ammo_, item_); any other key is taken and ignored, silently (a
		// misspelled class gave nothing before either). Before, every key was a class created on the player: a "player"
		// key made a non-client player whose spawn ran the game rules' PlayerSpawn, which touched this equip again
		// (endless recursion, stack overflow). The stock and custom maps use weapon_ / ammo_ keys only.
		if ( strncmp( pkvd->szKeyName, "weapon_", 7 ) && strncmp( pkvd->szKeyName, "ammo_", 5 ) && strncmp( pkvd->szKeyName, "item_", 5 ) )
		{
			pkvd->fHandled = TRUE;
			return;
		}
		for ( int i = 0; i < MAX_EQUIP; i++ )
		{
			if ( !m_weaponNames[i] )
			{
				char tmp[128];

				UTIL_StripToken( pkvd->szKeyName, tmp, sizeof( tmp ) );

				m_weaponNames[i] = ALLOC_STRING(tmp);
				m_weaponCount[i] = atoi(pkvd->szValue);
				m_weaponCount[i] = max(1,m_weaponCount[i]);
				// SBO 2.0: a huge count ("2147483647") made EquipPlayer give items until the server hung or ran out of
				// edicts. The maps use at most 20 (map-entities scan, 2026-10-16).
				if ( m_weaponCount[i] > EQUIP_MAX_COUNT )
				{
					ALERT( at_console, "game_player_equip \"%s\": %s count %d, using %d\n", STRING( pev->targetname ), tmp,
						m_weaponCount[i], EQUIP_MAX_COUNT );
					m_weaponCount[i] = EQUIP_MAX_COUNT;
				}
				pkvd->fHandled = TRUE;
				break;
			}
		}
	}
}


void CGamePlayerEquip::Touch( CBaseEntity *pOther )
{
	if ( !CanFireForActivator( pOther ) )
		return;

	if ( UseOnly() )
		return;

	EquipPlayer( pOther );
}

void CGamePlayerEquip::EquipPlayer( CBaseEntity *pEntity )
{
	CBasePlayer *pPlayer = NULL;

	if ( pEntity && pEntity->IsPlayer() )	// halflife-updated #189: null activator
	{
		pPlayer = (CBasePlayer *)pEntity;
	}

	if ( !pPlayer )
		return;

	for ( int i = 0; i < MAX_EQUIP; i++ )
	{
		if ( !m_weaponNames[i] )
			break;
		for ( int j = 0; j < m_weaponCount[i]; j++ )
		{
 			pPlayer->GiveNamedItem( STRING(m_weaponNames[i]) );
		}
	}
}


void CGamePlayerEquip::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	EquipPlayer( pActivator );
}


//
// CGamePlayerTeam / game_player_team	-- Changes the team of the player who fired it
// Flag: Fire once
// Flag: Kill Player
// Flag: Gib Player

#define SF_PTEAM_FIREONCE			0x0001
#define SF_PTEAM_KILL    			0x0002
#define SF_PTEAM_GIB     			0x0004

class CGamePlayerTeam : public CRulePointEntity
{
public:
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:

	inline BOOL RemoveOnFire( void ) { return (pev->spawnflags & SF_PTEAM_FIREONCE) ? TRUE : FALSE; }
	inline BOOL ShouldKillPlayer( void ) { return (pev->spawnflags & SF_PTEAM_KILL) ? TRUE : FALSE; }
	inline BOOL ShouldGibPlayer( void ) { return (pev->spawnflags & SF_PTEAM_GIB) ? TRUE : FALSE; }
	
	const char *TargetTeamName( const char *pszTargetName );
};

LINK_ENTITY_TO_CLASS( game_player_team, CGamePlayerTeam );


const char *CGamePlayerTeam::TargetTeamName( const char *pszTargetName )
{
	CBaseEntity *pTeamEntity = NULL;

	while ((pTeamEntity = UTIL_FindEntityByTargetname( pTeamEntity, pszTargetName )) != NULL)
	{
		if ( FClassnameIs( pTeamEntity->pev, "game_team_master" ) )
			return pTeamEntity->TeamID();
	}

	return NULL;
}


void CGamePlayerTeam::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( pActivator && pActivator->IsPlayer() )	// same null-activator guard as game_score (halflife-updated #246)
	{
		const char *pszTargetTeam = TargetTeamName( STRING(pev->target) );
		if ( pszTargetTeam )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)pActivator;
			g_pGameRules->ChangePlayerTeam( pPlayer, pszTargetTeam, ShouldKillPlayer(), ShouldGibPlayer() );
		}
	}
	
	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}


//
// CNuke / nuclear_explosion -- SBO: triggered explosion effect (sprite, smoke, shockwave ring, glow),
// radius damage and screen shake. All effect parameters come from the keyvalues v1..v16.
//
extern DLL_GLOBAL short g_sModelIndexSmoke;
extern void RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType );

class CNuke : public CRulePointEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT AfterGlow( void );

	int	m_iSpriteTexture;
	int	m_iExplode;
	int	m_iGlow;

	int	v1;		// explosion sprite scale
	int	v2;		// explosion sprite brightness
	int	v3;		// smoke scale
	int	v4;		// ring life
	int	v5;		// ring width
	int	v6;		// ring brightness
	int	v7;		// ring speed
	int	v8;		// glow lifetime (0.1 s)
	int	v9;		// glow size
	int	v10;	// glow brightness
	int	v12;	// damage radius
	int	v13;	// shake radius
	int	v14;	// afterglow lifetime (0.1 s)
	int	v15;	// afterglow size
	int	v16;	// afterglow brightness
	int	v11;	// damage
};

LINK_ENTITY_TO_CLASS( nuclear_explosion, CNuke );


void CNuke::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
}


void CNuke::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "v1"))
	{
		v1 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v2"))
	{
		v2 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v3"))
	{
		v3 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v4"))
	{
		v4 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v5"))
	{
		v5 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v6"))
	{
		v6 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v7"))
	{
		v7 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v8"))
	{
		v8 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v9"))
	{
		v9 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v10"))
	{
		v10 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v11"))
	{
		v11 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v12"))
	{
		v12 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v13"))
	{
		v13 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v14"))
	{
		v14 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v15"))
	{
		v15 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "v16"))
	{
		v16 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}

	CRuleEntity::KeyValue( pkvd );
}


void CNuke::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	// explosion sprite
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 128 );
		WRITE_SHORT( m_iExplode );
		WRITE_BYTE( v1 );
		WRITE_BYTE( v2 );
	MESSAGE_END();

	// smoke
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 256 );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( v3 );
		WRITE_BYTE( 5 );
	MESSAGE_END();

	// shockwave ring
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 320 );
		WRITE_SHORT( m_iSpriteTexture );
		WRITE_BYTE( 0 ); // startframe
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( v4 ); // life
		WRITE_BYTE( v5 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 192 );   // r, g, b
		WRITE_BYTE( v6 ); // brightness
		WRITE_BYTE( v7 );		// speed
	MESSAGE_END();

	// glow
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_GLOWSPRITE );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( m_iGlow );
		WRITE_BYTE( v8 );
		WRITE_BYTE( v9 );
		WRITE_BYTE( v10 );
	MESSAGE_END();

	EMIT_SOUND( ENT(pev), CHAN_STATIC, "weapons/mortarhit.wav", 1.0, 0.3 );

	RadiusDamage( pev->origin, pev, pev, v11, v12, CLASS_NONE, DMG_BLAST );
	UTIL_ScreenShake( pev->origin, 25.0, 200.0, 1.0, v13 );

	// SBO 2.0: the afterglow follows when the glow's lifetime (v8, tenths of a second) ends. 1.38 set the think
	// function without a nextthink, so the afterglow never showed.
	SetThink( &CNuke::AfterGlow );
	pev->nextthink = gpGlobals->time + max( v8, 1 ) * 0.1f;
}


void CNuke::AfterGlow( void )
{
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_GLOWSPRITE );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( m_iGlow );
		WRITE_BYTE( v14 );
		WRITE_BYTE( v15 );
		WRITE_BYTE( v16 );
	MESSAGE_END();

	// SBO 2.0: the entity stays, so the map can fire it again (co_zylom's garg_boom button, wait 3)
	SetThink( NULL );
}


void CNuke::Precache( void )
{
	m_iSpriteTexture = PRECACHE_MODEL( "sprites/white.spr" );
	m_iExplode = PRECACHE_MODEL( "sprites/fexplo1.spr" );
	m_iGlow = PRECACHE_MODEL( "sprites/glow04.spr" );
	PRECACHE_SOUND( "weapons/mortarhit.wav" );
}
//
// CGamePlayVideo / game_playvideo -- SBO 2.0 (docs/VIDEO_PLAYER.md), port of Aura-SE's CGamePlayVideo
// (SourceCode/dlls/maprules.cpp, The Phoenix Project Software). Sends "PlayVideo" to the activating player, or to
// everybody (flag 1): their clients play the video full screen (cl_dll/video/video_player.cpp).
//
// Flag: SF_GAME_PLAYVIDEO_ALL_PLAYERS
// "video" (or "message") = video file name in the mod's video/ folder (Ogg Theora, .ogv)
// "audio"                = optional Ogg Vorbis file in video/ (.ogg) played instead of the video's own sound
// "master"               = as the other game_ entities
//
// Changes from Aura: both files are precached (PRECACHE_GENERIC), so players download them (FastDL); the names are
// checked when the map loads (game_shared/video_shared.h: inside video/, no ".."), a bad name is reported and the
// entity does nothing.
//
#define SF_GAME_PLAYVIDEO_ALL_PLAYERS		0x0001

class CGamePlayVideo : public CRulePointEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	string_t	m_iszAudio;
	BOOL		m_fValid;		// the names passed the check (Precache)
};

LINK_ENTITY_TO_CLASS( game_playvideo, CGamePlayVideo );

TYPEDESCRIPTION	CGamePlayVideo::m_SaveData[] = 
{
	DEFINE_FIELD( CGamePlayVideo, m_iszAudio, FIELD_STRING ),
	DEFINE_FIELD( CGamePlayVideo, m_fValid, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CGamePlayVideo, CRulePointEntity );

void CGamePlayVideo::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "video" ) )
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "audio" ) )
	{
		m_iszAudio = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CRulePointEntity::KeyValue( pkvd );
}

void CGamePlayVideo::Spawn( void )
{
	Precache();
	CRulePointEntity::Spawn();
}

void CGamePlayVideo::Precache( void )
{
	char path[VIDEO_MAX_PATH];

	m_fValid = FALSE;
	if ( !VideoShared_MakePath( STRING( pev->message ), true, path, sizeof( path ) ) )
	{
		// printed at every developer level: the mapper must see it
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( "Error: game_playvideo \"%s\": video \"%s\" refused (%s): nothing plays\n",
			STRING( pev->targetname ), STRING( pev->message ), VideoShared_NameError( STRING( pev->message ), true ) ) );
		return;
	}
	PRECACHE_GENERIC( (char *)STRING( ALLOC_STRING( path ) ) );

	if ( !FStringNull( m_iszAudio ) && STRING( m_iszAudio )[0] )
	{
		if ( !VideoShared_MakePath( STRING( m_iszAudio ), false, path, sizeof( path ) ) )
		{
			g_engfuncs.pfnServerPrint( UTIL_VarArgs( "Error: game_playvideo \"%s\": audio \"%s\" refused (%s): nothing plays\n",
				STRING( pev->targetname ), STRING( m_iszAudio ), VideoShared_NameError( STRING( m_iszAudio ), false ) ) );
			return;
		}
		PRECACHE_GENERIC( (char *)STRING( ALLOC_STRING( path ) ) );
	}
	m_fValid = TRUE;
}

void CGamePlayVideo::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !m_fValid || !gmsgPlayVideo || !CanFireForActivator( pActivator ) )
		return;

	const BOOL fAllPlayers = ( pev->spawnflags & SF_GAME_PLAYVIDEO_ALL_PLAYERS ) != 0;
	if ( !fAllPlayers && ( !pActivator || !pActivator->IsPlayer() ) )
		return;

	const char *pszAudio = FStringNull( m_iszAudio ) ? "" : STRING( m_iszAudio );

	if ( fAllPlayers )
		MESSAGE_BEGIN( MSG_ALL, gmsgPlayVideo );
	else
		MESSAGE_BEGIN( MSG_ONE, gmsgPlayVideo, NULL, pActivator->edict() );
	WRITE_STRING( STRING( pev->message ) );
	WRITE_STRING( pszAudio );
	MESSAGE_END();
}

//=========================================================
// SBO 2.0, from Featureful (halflife-featureful dlls/maprules.cpp CGamePlayerSettings, Half-Life 1 SDK license;
// docs/FEATUREFUL_GUIDE_ENTITIES.md): game_player_settings gives or takes weapons and ammo and sets health, max
// health and armor of the player who fires it (!activator, a player in play). Its master is "master". Then fires its
// target with that player as activator.
// Ammo keys: an ammo name with '_' for spaces and a leading '_' before a digit ("Hand_Grenade"); negative takes.
// Weapon keys: the weapon's classname, -1 remove, 1 give, 2 give even if he has it. Only the game's own weapons and
// ammo (registered by W_Precache) count: anything else would precache during the game. Health and armor values
// may start with '=' set, '+' add, '-' subtract, '<' set if higher, '>' set if lower; no prefix sets a value above 0.
// Left out (SBO has no such system): suit and suit light, armor maximum and strength, Featureful's canisters, and
// the singleplayer "equip at spawn".
//=========================================================
#define PLAYERSETTINGS_MAX_WEAPONS	32

enum
{
	VALUE_SETTING_DEFAULT = 0,
	VALUE_SETTING_SET,
	VALUE_SETTING_ADD,
	VALUE_SETTING_SUBTRACT,
	VALUE_SETTING_ATMAX,	// '<': set if the current value is higher
	VALUE_SETTING_ATLEAST,	// '>': set if the current value is lower
};

class CGamePlayerSettings : public CRulePointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL PreEntvarsKeyValue( KeyValueData *pkvd );

private:
	void EquipPlayer( CBasePlayer *pPlayer );
	static short ParseValue( const char *szValue, float &result );

	int		m_ammoCounts[MAX_AMMO_SLOTS];	// by the ammo's registry index (CBasePlayer::GetAmmoIndex)
	string_t m_weapons[PLAYERSETTINGS_MAX_WEAPONS];
	short	m_weaponSettings[PLAYERSETTINGS_MAX_WEAPONS];
	float	m_health, m_maxHealth, m_armor;
	short	m_healthSetting, m_maxHealthSetting, m_armorSetting;
	BOOL	m_allowOverheal, m_allowOvercharge;
	int		m_longjump;
};

LINK_ENTITY_TO_CLASS( game_player_settings, CGamePlayerSettings );

short CGamePlayerSettings::ParseValue( const char *szValue, float &result )
{
	short setting = VALUE_SETTING_DEFAULT;
	switch ( szValue[0] )
	{
	case '=': setting = VALUE_SETTING_SET; szValue++; break;
	case '+': setting = VALUE_SETTING_ADD; szValue++; break;
	case '-': setting = VALUE_SETTING_SUBTRACT; szValue++; break;
	case '<': setting = VALUE_SETTING_ATMAX; szValue++; break;
	case '>': setting = VALUE_SETTING_ATLEAST; szValue++; break;
	default: break;
	}
	result = atof( szValue );
	if ( !( result > -1e6f && result < 1e6f ) )
		result = 0;	// NaN or out of range
	return setting;
}

// health, max_health and armorvalue are entity fields the engine would read as plain numbers (the prefix lost):
// DispatchKeyValue gives them here first
BOOL CGamePlayerSettings::PreEntvarsKeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "health" ) )
		m_healthSetting = ParseValue( pkvd->szValue, m_health );
	else if ( FStrEq( pkvd->szKeyName, "max_health" ) )
		m_maxHealthSetting = ParseValue( pkvd->szValue, m_maxHealth );
	else if ( FStrEq( pkvd->szKeyName, "armorvalue" ) )
		m_armorSetting = ParseValue( pkvd->szValue, m_armor );
	else
		return FALSE;
	pkvd->fHandled = TRUE;
	return TRUE;
}

BOOL GamePlayerSettings_PreEntvarsKeyValue( edict_t *pent, KeyValueData *pkvd )
{
	if ( !pkvd->szClassName || strcmp( pkvd->szClassName, "game_player_settings" ) )
		return FALSE;
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( pent );
	if ( !pEntity || !FClassnameIs( pEntity->pev, "game_player_settings" ) )
		return FALSE;
	return ( (CGamePlayerSettings *)pEntity )->PreEntvarsKeyValue( pkvd );
}

void CGamePlayerSettings::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "allow_overheal" ) )
	{
		m_allowOverheal = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "allow_overcharge" ) )
	{
		m_allowOvercharge = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "longjump" ) )
	{
		m_longjump = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( !strncmp( pkvd->szKeyName, "weapon_", 7 ) )
	{
		for ( int i = 0; i < PLAYERSETTINGS_MAX_WEAPONS; i++ )
		{
			if ( FStringNull( m_weapons[i] ) || FStrEq( pkvd->szKeyName, STRING( m_weapons[i] ) ) )
			{
				m_weapons[i] = ALLOC_STRING( pkvd->szKeyName );
				m_weaponSettings[i] = (short)atoi( pkvd->szValue );
				break;
			}
		}
		pkvd->fHandled = TRUE;
	}
	else
	{
		// an ammo name: '_' for spaces, a leading '_' for names that start with a digit
		char name[64];
		const char *src = pkvd->szKeyName;
		if ( *src == '_' )
			src++;
		int n = 0;
		for ( ; *src && n < (int)sizeof( name ) - 1; src++ )
			name[n++] = ( *src == '_' ) ? ' ' : *src;
		name[n] = '\0';
		int index = n ? CBasePlayer::GetAmmoIndex( name ) : -1;
		if ( index > 0 && index < MAX_AMMO_SLOTS )
		{
			m_ammoCounts[index] = atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else
			CRulePointEntity::KeyValue( pkvd );
	}
}

void CGamePlayerSettings::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;
	if ( !Logic_IsPlayerInPlay( pActivator ) )
		return;	// a player in play only: never a spectator, a dead or a waiting player
	EquipPlayer( (CBasePlayer *)pActivator );
}

// the most of this ammo any weapon carries (its weapons' iMaxAmmo1), for CBasePlayer::GiveAmmo
static int PlayerSettings_MaxAmmo( const char *szAmmo )
{
	int iMax = 0;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		ItemInfo &II = CBasePlayerItem::ItemInfoArray[i];
		if ( II.pszAmmo1 && !stricmp( II.pszAmmo1, szAmmo ) && II.iMaxAmmo1 > iMax )
			iMax = II.iMaxAmmo1;
		if ( II.pszAmmo2 && !stricmp( II.pszAmmo2, szAmmo ) && II.iMaxAmmo2 > iMax )
			iMax = II.iMaxAmmo2;
	}
	return iMax;
}

// a weapon the game registered at map start (W_Precache: precached, in ItemInfoArray)
static BOOL PlayerSettings_IsGameWeapon( const char *szWeapon )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		const char *name = CBasePlayerItem::ItemInfoArray[i].pszName;
		if ( name && !stricmp( name, szWeapon ) )
			return TRUE;
	}
	return FALSE;
}

extern int gmsgCurWeapon;

// the weapon with the highest weight that can be deployed (as CHalfLifeMultiplay::GetNextBestWeapon, without a
// current weapon); with none left the HUD is told there is no weapon (as RemoveAllItems)
static void PlayerSettings_SwitchToBest( CBasePlayer *player )
{
	CBasePlayerItem *pBest = NULL;
	int iBestWeight = -1;
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		for ( CBasePlayerItem *pCheck = player->m_rgpPlayerItems[i]; pCheck; pCheck = pCheck->m_pNext )
		{
			if ( pCheck->iFlags() & ITEM_FLAG_NOAUTOSWITCHTO )
				continue;
			if ( pCheck->iWeight() > iBestWeight && pCheck->CanDeploy() )
			{
				iBestWeight = pCheck->iWeight();
				pBest = pCheck;
			}
		}
	}
	if ( pBest )
	{
		player->SwitchWeapon( pBest );
		return;
	}
	if ( player->m_pActiveItem == NULL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, player->pev );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

static float PlayerSettings_Apply( short setting, float current, float value, BOOL zeroIsSet )
{
	switch ( setting )
	{
	case VALUE_SETTING_SET:		return value;
	case VALUE_SETTING_ADD:		return current + value;
	case VALUE_SETTING_SUBTRACT:	return current - value;
	case VALUE_SETTING_ATMAX:	return current > value ? value : current;
	case VALUE_SETTING_ATLEAST:	return current < value ? value : current;
	default:					return ( value > 0 || ( zeroIsSet && value == 0 && 0 ) ) ? value : current;
	}
}

void CGamePlayerSettings::EquipPlayer( CBasePlayer *player )
{
	// max health first: a lower one clamps the health (unless overheal is allowed)
	if ( m_maxHealthSetting != VALUE_SETTING_DEFAULT || m_maxHealth > 0 )
	{
		float maxHealth = PlayerSettings_Apply( m_maxHealthSetting, player->pev->max_health, m_maxHealth, FALSE );
		if ( maxHealth < 1 )
			maxHealth = 1;
		player->pev->max_health = maxHealth;
		if ( !m_allowOverheal && player->pev->health > maxHealth )
			player->pev->health = maxHealth;
	}

	if ( m_healthSetting == VALUE_SETTING_SUBTRACT )
	{
		// exactly that much (SBO's damage would scale it at random); taking all he has kills him, as damage
		if ( m_health > 0 )
		{
			if ( player->pev->health - m_health >= 1 )
				player->pev->health -= m_health;
			else
				player->TakeDamage( pev, pev, player->pev->health + player->pev->armorvalue * 4 + 1000, DMG_GENERIC );
		}
	}
	else if ( m_healthSetting != VALUE_SETTING_DEFAULT || m_health > 0 )
	{
		float health = PlayerSettings_Apply( m_healthSetting, player->pev->health, m_health, FALSE );
		if ( !m_allowOverheal && health > player->pev->max_health )
			health = player->pev->max_health;
		if ( health < 1 )
			health = 1;	// '=0' or '<0' never kills; '-' does (above)
		if ( health > player->pev->health || m_healthSetting != VALUE_SETTING_ADD )
			player->pev->health = health;
	}

	if ( m_armorSetting != VALUE_SETTING_DEFAULT || m_armor > 0 )
	{
		float armor = PlayerSettings_Apply( m_armorSetting, player->pev->armorvalue, m_armor, TRUE );
		if ( !m_allowOvercharge && armor > MAX_NORMAL_BATTERY )
			armor = ( player->pev->armorvalue > MAX_NORMAL_BATTERY && m_armorSetting != VALUE_SETTING_SET ) ? player->pev->armorvalue : MAX_NORMAL_BATTERY;
		if ( armor < 0 )
			armor = 0;
		player->pev->armorvalue = armor;
	}

	if ( m_longjump > 0 && !player->m_fLongJump )
	{
		player->m_fLongJump = TRUE;
		g_engfuncs.pfnSetPhysicsKeyValue( player->edict(), "slj", "1" );
	}
	else if ( m_longjump < 0 && player->m_fLongJump )
	{
		player->m_fLongJump = FALSE;
		g_engfuncs.pfnSetPhysicsKeyValue( player->edict(), "slj", "0" );
	}

	const BOOL hadWeapons = player->m_pActiveItem != NULL;

	for ( int i = 1; i < MAX_AMMO_SLOTS; i++ )
	{
		const char *szAmmo = CBasePlayerItem::AmmoInfoArray[i].pszName;
		if ( !m_ammoCounts[i] || !szAmmo )
			continue;
		if ( m_ammoCounts[i] > 0 )
			player->GiveAmmo( m_ammoCounts[i], (char *)szAmmo, PlayerSettings_MaxAmmo( szAmmo ) );
		else
		{
			int index = CBasePlayer::GetAmmoIndex( szAmmo );
			if ( index > 0 && index < MAX_AMMO_SLOTS )
				player->m_rgAmmo[index] = max( 0, player->m_rgAmmo[index] + m_ammoCounts[i] );
		}
	}

	for ( int i = 0; i < PLAYERSETTINGS_MAX_WEAPONS && !FStringNull( m_weapons[i] ); i++ )
	{
		const char *szWeapon = STRING( m_weapons[i] );
		if ( !PlayerSettings_IsGameWeapon( szWeapon ) )
		{
			// SBO: only the weapons the game registers (W_Precache): another class would precache its model during
			// the game (the engine stops the map)
			ALERT( at_console, "game_player_settings \"%s\": %s is not one of the game's weapons\n", STRING( pev->targetname ), szWeapon );
			continue;
		}
		if ( m_weaponSettings[i] < 0 )
		{
			for ( int slot = 0; slot < MAX_ITEM_TYPES; slot++ )
			{
				CBasePlayerItem *pItem = player->m_rgpPlayerItems[slot];
				while ( pItem )
				{
					CBasePlayerItem *pNext = pItem->m_pNext;
					if ( FClassnameIs( pItem->pev, szWeapon ) )
					{
						player->pev->weapons &= ~( 1 << pItem->m_iId );
						pItem->DestroyItem();
					}
					pItem = pNext;
				}
			}
		}
		else if ( m_weaponSettings[i] == 1 )
		{
			if ( !player->HasNamedPlayerItem( szWeapon ) )
				player->GiveNamedItem( szWeapon );
		}
		else if ( m_weaponSettings[i] == 2 )
			player->GiveNamedItem( szWeapon );
	}

	// SBO: a removed active weapon (or the first one given) leaves no active item: pick the best weapon the way the
	// game's auto-switch does. (GetNextBestWeapon needs the current weapon: called with NULL it crashed the server.)
	if ( !hadWeapons || player->m_pActiveItem == NULL )
		PlayerSettings_SwitchToBest( player );

	SUB_UseTargets( player, USE_TOGGLE, 0 );
}
