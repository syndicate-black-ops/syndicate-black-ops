//=========================================================
// SBO 2.0: entities from the Featureful entity guide (docs/FEATUREFUL_GUIDE_ENTITIES.md): what other files call.
//=========================================================

#ifndef GUIDE_ENTITIES_H
#define GUIDE_ENTITIES_H

// guide_effects.cpp: the env_warpball named szName plays its effect at vecOrigin, its sound on pSoundEnt
// (monstermaker "warpball"). FALSE: no such env_warpball.
BOOL GuideWarpball_Play( const char *szName, const Vector &vecOrigin, edict_t *pSoundEnt );

// guide_monsters.cpp: the next active monster_target an automatic func_tank shoots at (it counts as a player or a
// player ally), from iCursor (start at 0; it moves past the one returned). NULL: no more
CBaseEntity *GuideMonsterTargets_NextForTank( int &iCursor );

// guide_monsters.cpp: empties the monster_target list (worldspawn: the new map's ones add themselves at spawn)
void GuideMonsterTargets_Reset( void );

// guide_monsters.cpp: asks a free monster (alive, not in a script, not fighting) to walk / run at least flMinDist
// away from vecSpot (Featureful CBaseMonster::AskMoveAwayFromSpot; monstermaker "make_blocker_move_away"). FALSE: it
// is busy, or found no spot
BOOL GuideMonster_MoveAwayFromSpot( CBaseMonster *pMonster, const Vector &vecSpot, float flMinDist, BOOL fRun );

#endif // GUIDE_ENTITIES_H
