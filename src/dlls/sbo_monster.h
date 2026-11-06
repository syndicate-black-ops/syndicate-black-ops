//=========================================================
// SBO: shared declarations for SBO's own monsters
// (cyborg, dark, darkgrunt, grunt_hunter, hunter, hunter2).
//=========================================================
#ifndef SBO_MONSTER_H
#define SBO_MONSTER_H

// game.cpp cvars
extern cvar_t	XP;
extern cvar_t	juniorpowmonst;		// monster damage level
extern cvar_t	juniorpvmonst;		// monster health level
extern cvar_t	junioracmar;		// monster accuracy level

// number of players the monsters scale by (coop.cpp; SBO: the round counter m_iParticip1)
int MonsterScalePlayers( void );

extern int		gmsgScoreInfo;

// XP fields monsters' TakeDamage updates (CBasePlayer *p). Members, not raw offsets: the offsets differ between
// egcs (syndicate.so: 0x308, 0x30c, 0x310, 0x2e4, 0x65c) and MSVC (hl.dll: 0x2f4, 0x2f8, 0x2fc, 0x2d0, 0x648).
#define SBO_XP( p )		( (p)->m_iXP )		// experience; level up at 5000
// chat-vgui2: XP gain; also adds the same (truncated) amount to the cumulative total shown on the scoreboard
// (m_iSboTotalXP: not reduced by level-ups or the death halving; lowered by 5000 per level a teamkill / suicide
// penalty takes, down into the negative (CCoop::PlayerKilled); cleared in InitHUD on join / map change).
// lvl is the caller's level-up XP (5000 monsters, 7500 medikit): a gain that crosses it stops there, so the excess
// of a big hit is discarded instead of counted on the scoreboard, one gain is at most one level, and the next gain
// counts from the level-up (the caller's lvl check then resets SBO_XP to 0). INT_MAX: no level-up, no clamp.
#define SBO_XP_GAIN( p, x, lvl )	do { int _iOldXP = SBO_XP( p ); SBO_XP( p ) += (x); if ( SBO_XP( p ) > (lvl) ) SBO_XP( p ) = ( _iOldXP > (lvl) ) ? _iOldXP : (lvl); (p)->m_iSboTotalXP += SBO_XP( p ) - _iOldXP; } while ( 0 )
// A level-up: CBasePlayer::SboLevelUp (player.cpp: level speed m_iSboSpeed +5, spawn health m_iSboHealth +2,
// the current max speed m_iSboMaxSpeed +10 while the level is above -40).
#define SBO_DEATHS( p )	( (p)->m_iDeaths )

#endif // SBO_MONSTER_H
