//=========================================================
// SBO 2.0 info_light_origin and the "lightorigin" key (Trinity renderer; docs/ENV_DLIGHT.md#info_light_origin):
// a model entity lit as if it stood at an info_light_origin marker (the engine lights a studio model from the
// lightmap under its origin). The key works on any entity with a studio model (DispatchKeyValue); env_static takes
// it in its own KeyValue and sends it with its props.
//=========================================================

#ifndef LIGHT_ORIGIN_H
#define LIGHT_ORIGIN_H

class CBaseEntity;
class CBasePlayer;

// CWorld::Spawn: a new map has no light origins until its keys name them
void LightOrigin_Reset( void );

// DispatchKeyValue: "lightorigin" on any entity. TRUE: taken.
BOOL LightOrigin_KeyValue( CBaseEntity *pEntity, const char *szKeyName, const char *szValue );

// the origin of the info_light_origin named szName (FALSE: none; a console note once per name)
BOOL LightOrigin_Find( const char *szName, Vector &vecOut );

// the send queue (env_dlight.cpp's sender, after the lights): the entities' light origins from iCursor on
int LightOrigin_SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget );

#endif // LIGHT_ORIGIN_H
