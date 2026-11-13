//=========================================================
// Reading an entity's keyvalues at run time, and Sven Co-op's custom keyvalues. For trigger_changevalue and
// trigger_copyvalue (trigger_changevalue.cpp); docs/TRIGGER_CHANGEVALUE.md.
//
// Ported from SevenKewp (https://github.com/wootguy/SevenKewp, Half-Life 1 SDK license): dlls/util/CKeyValue.h,
// CBaseEntity::GetKeyValue / GetCustomKeyValue (dlls/CBaseEntity.cpp), GetEntvarsKeyvalue (dlls/util/util.cpp).
//
//   * CBaseEntity::GetKeyValue( name ): the current value of a key, typed. The default answers the entity's
//     custom keyvalues and every entvars field the save table knows (health, origin, target, rendercolor, ...);
//     a class answers its own keys by overriding it (game_timer does).
//   * Custom keyvalues: any key named "$i_...", "$f_...", "$v_..." or "$s_..." (integer, float, vector, string)
//     can be set on any entity, in the map or at run time, and read back. They do nothing by themselves: they
//     are variables for the logic entities.
//   * Writing is a key dispatch: KeyValue_DispatchRuntime (the entity's KeyValue(), as at map load).
//=========================================================

#ifndef KEYVALUE_H
#define KEYVALUE_H

enum keyvalue_types
{
	KEY_TYPE_NONE,		// indicates value failed to load
	KEY_TYPE_INT,
	KEY_TYPE_FLOAT,
	KEY_TYPE_VECTOR,
	KEY_TYPE_STRING,
	KEY_TYPE_EHANDLE
};

struct CKeyValue
{
	unsigned short keyType;		// simplified type from keyvalue_types. Key is invalid if this is 0.
	unsigned short keyOffset;
	const char *keyName;

	union
	{
		int iVal;
		float fVal;
		int sVal;				// string_t
		float vVal[3];
	};
};

// SevenKewp: MAX_KEY_NAME_LEN / MAX_KEY_VALUE_LEN are the engine's (256 / 2048). A value written at run time is
// cut to this (UTIL_VarArgs' buffer and the user message limits are smaller than the engine's value limit).
#define KEYVALUE_MAX_VALUE_LEN		1024
#define KEYVALUE_MAX_CUSTOM_NAME	32

extern const CKeyValue g_emptyKeyValue;

// util.cpp: an entvars field by its save-table name (case-insensitive), KEY_TYPE_NONE if there is none
CKeyValue GetEntvarsKeyvalue( entvars_t *pev, const char *keyName );
// util.cpp: the entvars field is a model or sound name (the engine needs those precached before the map starts)
BOOL EntvarsKeyNeedsPrecache( const char *keyName );
// SBO 2.1: the console line for a key refused at run time because it would need a precache (an entity's own model,
// sprite or class keys after its spawn, KeyValue_DispatchRuntime's model and sound keys)
void KeyValue_RefuseNeedsPrecache( entvars_t *pev, const char *keyName );

// "$i_name", "$f_name", "$v_name", "$s_name"
BOOL KeyValue_IsCustomName( const char *keyName );
// DispatchKeyValue: stores a custom keyvalue of the entity. FALSE: not a custom key name.
BOOL KeyValue_SetCustom( edict_t *pent, const char *keyName, const char *value );
CKeyValue KeyValue_GetCustom( edict_t *pent, const char *keyName );
// ServerDeactivate: the map's custom keyvalues
void KeyValue_ClearCustom( void );

// A key only a server plugin may set (DispatchKeyValue at run time); maps can't (headscale)
BOOL KeyValue_IsPluginOnly( const char *keyName );

// Sets a key of a live entity the way the map loader does (entvars field, custom keyvalue or the class's
// KeyValue). FALSE: refused (a model or sound key, which would need a precache, or a plugin-only key) or not
// handled by the entity.
BOOL KeyValue_DispatchRuntime( edict_t *pent, const char *keyName, const char *value );

#endif // KEYVALUE_H
