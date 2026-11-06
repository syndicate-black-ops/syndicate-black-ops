//=========================================================
// SBO 2.0 object hints (Featureful, docs/OBJECT_HINTS.md): sprites that mark objects a player might want to use or
// pick up. A mapper names a hint template in an entity's "objecthint" key (any entity, DispatchKeyValue) and may set
// the distance its faint brackets show from in "objecthint_distance"; every
// OBJECTHINT_INTERVAL the server sends each living player the hints he sees (game_shared/objecthint_shared.h).
// The templates and visuals are Featureful's objecthint_example.json; SBO maps no class to a template, so only
// entities with the key show hints.
//=========================================================

#ifndef OBJECTHINT_H
#define OBJECTHINT_H

class CBaseEntity;
class CBasePlayer;

// CWorld::Precache: a new map has no hinted entity until its keys name one
void ObjectHint_Reset( void );

// DispatchKeyValue: the "objecthint" key (stored in m_objectHint) and "objecthint_distance" (m_flObjectHintDistance,
// clamped to 0-4096). TRUE: taken.
BOOL ObjectHint_KeyValue( CBaseEntity *pEntity, const char *szKeyName, const char *szValue );

// CBasePlayer::PostThink: sends the player's hints when his interval has passed
void ObjectHint_PlayerPostThink( CBasePlayer *pPlayer );

#endif // OBJECTHINT_H
