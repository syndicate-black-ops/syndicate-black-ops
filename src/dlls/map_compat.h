//=========================================================
// SBO 2.0 map compatibility file (docs/MAP_COMPAT.md): maps/map_compat.txt, shipped with the game, drops or
// overrides keyvalues of specific entities of specific custom maps while the map loads, so a custom map keeps its
// 1.38 behavior when SBO 2.0 starts to read a key the map carries by accident. Each entry names the map AND the
// CRC-32 of its .bsp: a map its author has updated no longer matches.
//=========================================================

#ifndef MAP_COMPAT_H
#define MAP_COMPAT_H

// DispatchKeyValue, before anything else: TRUE when the map's compat entry drops this key (the caller returns,
// fHandled is set). An entry that overrides the value points pkvd->szValue at its own text.
BOOL MapCompat_KeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd );

// ServerActivate: the map is loaded, keys set from now on (trigger_changevalue) are never filtered
// DispatchSpawn, first: the entity's keys are all in (the next key is a new entity's, even in the same edict)
void MapCompat_Spawn( edict_t *pent );

void MapCompat_MapLoaded( void );
// ServerDeactivate: the next map's keys are filtered again
void MapCompat_MapUnloaded( void );

#endif // MAP_COMPAT_H
