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

// number of participants (players) in the game
extern int		m_iParticip1;

extern int		gmsgScoreInfo;

// XP fields monsters' TakeDamage updates (CBasePlayer *p). Members, not raw offsets: the offsets differ between
// egcs (syndicate.so: 0x308, 0x30c, 0x310, 0x2e4, 0x65c) and MSVC (hl.dll: 0x2f4, 0x2f8, 0x2fc, 0x2d0, 0x648).
#define SBO_XP( p )		( (p)->m_iSbo308 )		// experience; level up at 5000
#define SBO_XP2( p )	( (p)->m_iSboSpeed )	// += 5 per level
#define SBO_XP3( p )	( (p)->m_iSboHealth )	// += 2 per level
#define SBO_XP4( p )	( (p)->m_iSboMaxSpeed )	// += 10 per level
#define SBO_DEATHS( p )	( (p)->m_iDeaths )

#endif // SBO_MONSTER_H
