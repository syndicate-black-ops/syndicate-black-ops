//=========================================================
// Reading an entity's keyvalues at run time, and Sven Co-op's custom keyvalues: see keyvalue.h.
// Ported from SevenKewp (Half-Life 1 SDK license).
//
// SBO: SevenKewp keeps the custom keyvalues in a hash map per entity index and takes them in
// CBaseEntity::KeyValue, which only classes that pass unknown keys down to it reach. Here they are kept in one
// table for the map and taken in DispatchKeyValue, so every entity has them.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"movewith.h"

const CKeyValue g_emptyKeyValue = { KEY_TYPE_NONE, 0, NULL, { 0 } };

// The map's custom keyvalues
#define MAX_CUSTOM_KEYVALUES	1024

typedef struct
{
	int			entindex;		// 0 = free
	int			serialnumber;	// of the edict: another entity may get the index later
	char		name[ KEYVALUE_MAX_CUSTOM_NAME ];
	CKeyValue	value;
} customkeyvalue_t;

static customkeyvalue_t s_CustomKeys[ MAX_CUSTOM_KEYVALUES ];
static int s_iCustomKeyCount = 0;	// slots in use from the start of the table

BOOL KeyValue_IsCustomName( const char *keyName )
{
	// SevenKewp: at least "$x_n"
	if ( !keyName || strlen( keyName ) <= 3 || keyName[0] != '$' || keyName[2] != '_' )
		return FALSE;

	return keyName[1] == 'i' || keyName[1] == 'f' || keyName[1] == 'v' || keyName[1] == 's';
}

static customkeyvalue_t *FindCustom( edict_t *pent, const char *keyName )
{
	const int index = ENTINDEX( pent );

	for ( int i = 0; i < s_iCustomKeyCount; i++ )
	{
		customkeyvalue_t *pKey = &s_CustomKeys[i];
		if ( pKey->entindex != index )
			continue;

		// the entity that owned it is gone
		if ( pKey->serialnumber != pent->serialnumber )
		{
			pKey->entindex = 0;
			continue;
		}

		if ( !strcmp( pKey->name, keyName ) )
			return pKey;
	}

	return NULL;
}

BOOL KeyValue_SetCustom( edict_t *pent, const char *keyName, const char *value )
{
	if ( !pent || !KeyValue_IsCustomName( keyName ) )
		return FALSE;

	if ( strlen( keyName ) >= KEYVALUE_MAX_CUSTOM_NAME )
	{
		ALERT( at_console, "Custom keyvalue \"%s\": name too long (%d characters at most)\n", keyName,
			KEYVALUE_MAX_CUSTOM_NAME - 1 );
		return TRUE;
	}

	customkeyvalue_t *pKey = FindCustom( pent, keyName );
	if ( !pKey )
	{
		for ( int i = 0; i < MAX_CUSTOM_KEYVALUES; i++ )
		{
			if ( i >= s_iCustomKeyCount || !s_CustomKeys[i].entindex )
			{
				pKey = &s_CustomKeys[i];
				if ( i >= s_iCustomKeyCount )
					s_iCustomKeyCount = i + 1;
				break;
			}
		}

		if ( !pKey )
		{
			ALERT( at_console, "Custom keyvalue \"%s\": the map has too many (%d)\n", keyName, MAX_CUSTOM_KEYVALUES );
			return TRUE;
		}

		pKey->entindex = ENTINDEX( pent );
		pKey->serialnumber = pent->serialnumber;
		strcpy( pKey->name, keyName );
		pKey->value = g_emptyKeyValue;
		pKey->value.keyName = pKey->name;
	}

	if ( !value )
		value = "";

	switch ( keyName[1] )
	{
	case 'i':
		pKey->value.keyType = KEY_TYPE_INT;
		pKey->value.iVal = atoi( value );
		break;
	case 'f':
		pKey->value.keyType = KEY_TYPE_FLOAT;
		pKey->value.fVal = atof( value );
		break;
	case 'v':
		pKey->value.keyType = KEY_TYPE_VECTOR;
		UTIL_StringToVector( pKey->value.vVal, value );
		break;
	default:
		pKey->value.keyType = KEY_TYPE_STRING;
		pKey->value.sVal = ALLOC_STRING( value );
		break;
	}

	return TRUE;
}

CKeyValue KeyValue_GetCustom( edict_t *pent, const char *keyName )
{
	if ( !pent || !KeyValue_IsCustomName( keyName ) )
		return g_emptyKeyValue;

	customkeyvalue_t *pKey = FindCustom( pent, keyName );
	if ( pKey )
		return pKey->value;

	ALERT( at_console, "%s (%s) has no custom key named %s\n", STRING( pent->v.targetname ),
		STRING( pent->v.classname ), keyName );
	return g_emptyKeyValue;
}

void KeyValue_ClearCustom( void )
{
	memset( s_CustomKeys, 0, sizeof( s_CustomKeys ) );
	s_iCustomKeyCount = 0;
}

// SevenKewp: CBaseEntity::GetKeyValue
CKeyValue CBaseEntity::GetKeyValue( const char *keyName )
{
	CKeyValue customKey = KeyValue_GetCustom( edict(), keyName );
	if ( customKey.keyType )
		return customKey;

	// SBO 2.1: a movewith child's velocity and angular velocity are its own, on top of its parent's (movewith.cpp)
	Vector vecOwn;
	if ( MoveWith_GetKeyValue( this, keyName, vecOwn ) )
	{
		CKeyValue key = GetEntvarsKeyvalue( pev, keyName );
		if ( key.keyType == KEY_TYPE_VECTOR )
		{
			key.vVal[0] = vecOwn.x;
			key.vVal[1] = vecOwn.y;
			key.vVal[2] = vecOwn.z;
		}
		return key;
	}

	return GetEntvarsKeyvalue( pev, keyName );
}

// SBO 2.0: keys only a server plugin may set (docs/MODEL_SCALE.md "Head scale")
BOOL KeyValue_IsPluginOnly( const char *keyName )
{
	return keyName && !stricmp( keyName, "headscale" );
}

void KeyValue_RefuseNeedsPrecache( entvars_t *pev, const char *keyName )
{
	ALERT( at_console, "Key \"%s\" of %s \"%s\" can't be changed at run time (it would need a precache)\n",
		keyName, STRING( pev->classname ), STRING( pev->targetname ) );
}

BOOL KeyValue_DispatchRuntime( edict_t *pent, const char *keyName, const char *value )
{
	if ( !pent || !keyName || !keyName[0] )
		return FALSE;

	// SBO: SevenKewp writes the string and leaves the entity with a model or sound the engine doesn't know.
	// Nothing can be precached once the map runs, so these keys are refused (SBO 2.1: the entity decides, for the
	// classes that keep numbers or names in the entvars sound fields).
	CBaseEntity *pTarget = CBaseEntity::Instance( pent );
	if ( pTarget ? pTarget->RuntimeKeyNeedsPrecache( keyName ) : EntvarsKeyNeedsPrecache( keyName ) )
	{
		KeyValue_RefuseNeedsPrecache( &pent->v, keyName );
		return FALSE;
	}

	// SBO 2.0: keys for server plugins only (DispatchKeyValue from AMXX / Metamod reaches the entity through the
	// engine's pfnKeyValue, not through here): maps can't set them
	if ( KeyValue_IsPluginOnly( keyName ) )
	{
		ALERT( at_console, "Key \"%s\" of %s \"%s\" is for server plugins only\n",
			keyName, STRING( pent->v.classname ), STRING( pent->v.targetname ) );
		return FALSE;
	}

	char szValue[ KEYVALUE_MAX_VALUE_LEN ];
	strncpy( szValue, value ? value : "", sizeof( szValue ) - 1 );
	szValue[ sizeof( szValue ) - 1 ] = '\0';

	// SBO 2.1: "origin", "angles", "velocity", "avelocity" of a movewith child set its own pose and motion on its
	// parent (movewith.cpp)
	if ( MoveWith_RuntimeKeyValue( pent, keyName, szValue ) )
	{
		if ( pTarget )
			pTarget->RuntimeKeyValueChanged( keyName );	// point_worldtext, vgui_screen: resent with the new pose
		return TRUE;
	}

	KeyValueData kvd;
	kvd.szClassName = (char *)STRING( pent->v.classname );
	kvd.szKeyName = (char *)keyName;
	kvd.szValue = szValue;
	kvd.fHandled = FALSE;

	DispatchKeyValue( pent, &kvd );

	// SBO 2.0 model scale: the collision box follows the new scale (UTIL_SetSize)
	if ( !stricmp( keyName, "scale" ) )
		UTIL_ModelScaleChanged( &pent->v );

	// SBO 2.0: tell the entity (point_worldtext sends its new state to the clients)
	if ( kvd.fHandled )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( pent );
		if ( pEntity )
			pEntity->RuntimeKeyValueChanged( keyName );
	}

	// SBO 2.1: a new "movewith" / "parent" / "parentflags" re-links the entity, as trigger_changeparent does
	if ( kvd.fHandled )
		MoveWith_RuntimeKeyChanged( pent, keyName );

	return kvd.fHandled;
}
