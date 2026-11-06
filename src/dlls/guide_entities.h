//=========================================================
// SBO 2.0: entities from the Featureful entity guide (docs/FEATUREFUL_GUIDE_ENTITIES.md): what other files call.
//=========================================================

#ifndef GUIDE_ENTITIES_H
#define GUIDE_ENTITIES_H

// guide_effects.cpp: the env_warpball named szName plays its effect at vecOrigin, its sound on pSoundEnt
// (monstermaker "warpball"). FALSE: no such env_warpball.
BOOL GuideWarpball_Play( const char *szName, const Vector &vecOrigin, edict_t *pSoundEnt );

// guide_monsters.cpp: the active monster_targets an automatic func_tank shoots at (they count as a player or a
// player ally); returns how many it put in pList
int GuideMonsterTargets_ForTank( CBaseEntity **pList, int iMax );

// guide_monsters.cpp: asks a free monster (alive, not in a script, not fighting) to walk / run at least flMinDist
// away from vecSpot (Featureful CBaseMonster::AskMoveAwayFromSpot; monstermaker "make_blocker_move_away"). FALSE: it
// is busy, or found no spot
BOOL GuideMonster_MoveAwayFromSpot( CBaseMonster *pMonster, const Vector &vecSpot, float flMinDist, BOOL fRun );

#endif // GUIDE_ENTITIES_H
