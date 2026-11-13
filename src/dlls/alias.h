//=========================================================
// Aliases: info_alias, multi_alias, trigger_changealias, info_group (alias.cpp, docs/ALIASES.md; Spirit of Half-Life)
//=========================================================
#ifndef ALIAS_H
#define ALIAS_H

// "*alias" and "group.member" references: TRUE when szName is one (*ppResult: the next entity after entStart it
// names, or NULL); FALSE for every other name. Called by UTIL_FindEdictByString for targetname searches.
BOOL	Alias_FindEdict( edict_t *entStart, const char *szName, edict_t **ppResult );

// FireTargets: alias changes made while it fires take effect when the outermost FireTargets is done
void	Alias_FireTargetsBegin( void );
void	Alias_FireTargetsDone( void );

void	Alias_Reset( void );	// a new map

#endif
