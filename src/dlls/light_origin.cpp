//=========================================================
// SBO 2.0 info_light_origin and the "lightorigin" key: see light_origin.h and game_shared/light_origin_shared.h.
//
// From the Trinity renderer SDK (Half-Life 1 SDK license; "SDK - Entities.txt" info_light_origin and item_generic
// "lightorigin", cl_dll/propmanager.cpp, cl_dll/StudioModelRenderer.cpp): the model is lit from the marker's
// position. Trinity did it for its client-side props only; here every entity with a studio model takes the key,
// and the clients get the marker's position per entity at HUD init (the send queue, as the lights).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"light_origin.h"
#include	"../game_shared/light_origin_shared.h"

extern int gmsgLightOrigin;

#define LIGHTORIGIN_MAX		512		// entities with the key per map

struct light_origin_t
{
	int			entindex;
	int			serial;		// the edict's serial number: the same entity, not a later one in its slot
	string_t	marker;
	BOOL		resolved;
	BOOL		found;
	Vector		origin;
};

static light_origin_t s_List[ LIGHTORIGIN_MAX ];
static int s_iCount;
static BOOL s_bFullNoted;

// a marker: a point entity the map keeps, found by name
LINK_ENTITY_TO_CLASS( info_light_origin, CPointEntity );

void LightOrigin_Reset( void )
{
	s_iCount = 0;
	s_bFullNoted = FALSE;
}

BOOL LightOrigin_KeyValue( CBaseEntity *pEntity, const char *szKeyName, const char *szValue )
{
	if ( !pEntity || !szKeyName || stricmp( szKeyName, "lightorigin" ) )
		return FALSE;
	if ( !szValue || !*szValue )
		return TRUE;

	edict_t *pEdict = pEntity->edict();
	const int index = ENTINDEX( pEdict );
	for ( int i = 0; i < s_iCount; i++ )
	{
		if ( s_List[i].entindex == index && s_List[i].serial == pEdict->serialnumber )
		{
			s_List[i].marker = ALLOC_STRING( szValue );		// the key twice: the last one
			s_List[i].resolved = FALSE;
			return TRUE;
		}
	}
	if ( s_iCount >= LIGHTORIGIN_MAX )
	{
		if ( !s_bFullNoted )
			ALERT( at_console, "lightorigin: more than %d entities with the key, the others are lit as usual\n", LIGHTORIGIN_MAX );
		s_bFullNoted = TRUE;
		return TRUE;
	}
	light_origin_t *l = &s_List[ s_iCount++ ];
	l->entindex = index;
	l->serial = pEdict->serialnumber;
	l->marker = ALLOC_STRING( szValue );
	l->resolved = FALSE;
	l->found = FALSE;
	return TRUE;
}

BOOL LightOrigin_Find( const char *szName, Vector &vecOut )
{
	if ( !szName || !*szName )
		return FALSE;
	edict_t *pFound = NULL;
	while ( !FNullEnt( pFound = FIND_ENTITY_BY_TARGETNAME( pFound, szName ) ) )
	{
		if ( FClassnameIs( pFound, "info_light_origin" ) )
		{
			vecOut = pFound->v.origin;
			return TRUE;
		}
	}
	ALERT( at_console, "lightorigin: no info_light_origin named \"%s\"\n", szName );
	return FALSE;
}

// the entity of entry i, if it is still the one that had the key
static CBaseEntity *LightOrigin_Entity( const light_origin_t *l )
{
	if ( l->entindex <= 0 || l->entindex >= gpGlobals->maxEntities )
		return NULL;
	edict_t *pEdict = INDEXENT( l->entindex );
	if ( !pEdict || pEdict->free || pEdict->serialnumber != l->serial )
		return NULL;
	return CBaseEntity::Instance( pEdict );
}

int LightOrigin_SendQueued( CBasePlayer *pPlayer, int &iCursor, int &iBudget )
{
	if ( !gmsgLightOrigin )
		return TRUE;
	for ( int i = max( iCursor, 0 ); i < s_iCount; i++ )
	{
		light_origin_t *l = &s_List[i];
		if ( !l->resolved )
		{
			l->resolved = TRUE;
			l->found = LightOrigin_Find( STRING( l->marker ), l->origin );
		}
		CBaseEntity *pEntity = l->found ? LightOrigin_Entity( l ) : NULL;
		if ( !pEntity )
			continue;
		if ( LIGHTORIGIN_MSG_BYTES > iBudget )
		{
			iCursor = i;
			return FALSE;
		}
		MESSAGE_BEGIN( MSG_ONE, gmsgLightOrigin, NULL, pPlayer->edict() );
			WRITE_SHORT( l->entindex );
			WRITE_SHORT( pEntity->pev->modelindex );
			WRITE_COORD( l->origin.x );
			WRITE_COORD( l->origin.y );
			WRITE_COORD( l->origin.z );
		MESSAGE_END();
		iBudget -= LIGHTORIGIN_MSG_BYTES;
	}
	return TRUE;
}
