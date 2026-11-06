//=========================================================
// SBO 2.0 object hints, server side: see objecthint.h and docs/OBJECT_HINTS.md.
//
// Ported from Featureful (FreeSlave/halflife-featureful 30fc2f7: dlls/objecthint_spec.cpp, dlls/player.cpp
// GetInteractiveEntity / CalcHintOrigin / GatherAndSendObjectHints). Featureful reads its catalog from
// templates/objecthint.json; SBO builds in the catalog of Featureful's templates/objecthint_example.json, with an empty
// entity_mapping and pickup_mapping: no entity shows a hint unless its "objecthint" key names a template. SBO gives
// "button", "charger" and "eyescanner" the scan stage of "pickup" (HUD-colored brackets within the hint distance:
// the entity's "objecthint_distance" key, else OBJECTHINT_DEFAULT_DISTANCE).
// The interaction target is the one SBO's CBasePlayer::PlayerUse picks (64 units, view cone), so the highlighted
// object is the one +use uses. The sends are unreliable: a hint lives OBJECTHINT_LIFE on the client and is sent
// again every OBJECTHINT_INTERVAL, so a lost packet costs nothing and the reliable channel never fills.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"objecthint.h"
#include	"../game_shared/objecthint_shared.h"

extern int gmsgObjectHint;

#define OBJECTHINT_USE_RADIUS	64.0f	// SBO's PLAYER_SEARCH_RADIUS (player.cpp PlayerUse)
#define OBJECTHINT_DEFAULT_DISTANCE	512.0f	// scan distance of every template but hidden, without "objecthint_distance"
#define OBJECTHINT_MAX_DISTANCE		4096.0f	// "objecthint_distance" is clamped to this
#define OBJECTHINT_SEARCH_MARGIN	256.0f	// the sphere search measures to the box center, the distance to the box

//---------------------------------------------------------
// Catalog (Featureful templates/objecthint_example.json)
//---------------------------------------------------------

struct ObjectHintVisual
{
	const char	*sprite;
	int			r, g, b;		// 0 0 0: the HUD color
	float		scale;
};

struct ObjectHintVisualSet
{
	const ObjectHintVisual *defaultVisual;
	const ObjectHintVisual *unusableVisual;
	const ObjectHintVisual *lockedVisual;

	BOOL HasAnySpriteDefined( void ) const { return defaultVisual || unusableVisual || lockedVisual; }
};

struct ObjectHintSpec
{
	const char			*name;
	ObjectHintVisualSet	scanVisualSet;
	ObjectHintVisualSet	interactionVisualSet;
	float				distance;		// scan distance; 0: the use radius
	float				verticalOffset;
};

#define OBJECTHINT_SPRITE	"sprites/hud_brackets.spr"

static const ObjectHintVisual s_VisualBase			= { OBJECTHINT_SPRITE,   0,   0,  0, 1.0f };
static const ObjectHintVisual s_VisualInteractable	= { OBJECTHINT_SPRITE, 255, 200, 50, 1.0f };
static const ObjectHintVisual s_VisualCharged		= { OBJECTHINT_SPRITE,   0, 255,  0, 1.0f };
static const ObjectHintVisual s_VisualDepleted		= { OBJECTHINT_SPRITE, 255,   0,  0, 1.0f };
static const ObjectHintVisual s_VisualLock			= { OBJECTHINT_SPRITE, 255,  50,  0, 1.0f };

// A visual set key that the JSON leaves out takes "default"; one set to null shows nothing (Featureful
// ReadFromDocument). The pointers below are what that reading gives for the example file, plus SBO's scan stage
// { default base, unusable null } on "button", "charger" and "eyescanner"; every template but hidden scans within
// OBJECTHINT_DEFAULT_DISTANCE unless the entity's "objecthint_distance" key sets its own.
static const ObjectHintSpec s_Templates[] =
{
	// "button": scan { default base, unusable null }, interaction { default interactable, unusable null, locked lock },
	// distance OBJECTHINT_DEFAULT_DISTANCE
	{ "button",
		{ &s_VisualBase, NULL, &s_VisualBase },
		{ &s_VisualInteractable, NULL, &s_VisualLock },
		OBJECTHINT_DEFAULT_DISTANCE, 0 },
	// "charger": scan { default base, unusable null }, interaction { default charged, unusable depleted }, distance OBJECTHINT_DEFAULT_DISTANCE
	{ "charger",
		{ &s_VisualBase, NULL, &s_VisualBase },
		{ &s_VisualCharged, &s_VisualDepleted, &s_VisualCharged },
		OBJECTHINT_DEFAULT_DISTANCE, 0 },
	// "eyescanner": scan { default base, unusable null }, interaction { default interactable, unusable null },
	// distance OBJECTHINT_DEFAULT_DISTANCE
	{ "eyescanner",
		{ &s_VisualBase, NULL, &s_VisualBase },
		{ &s_VisualInteractable, NULL, &s_VisualInteractable },
		OBJECTHINT_DEFAULT_DISTANCE, 0 },
	// "pickup": scan { default base, unusable null }, interaction { default interactable, unusable null }, distance OBJECTHINT_DEFAULT_DISTANCE
	{ "pickup",
		{ &s_VisualBase, NULL, &s_VisualBase },
		{ &s_VisualInteractable, NULL, &s_VisualInteractable },
		OBJECTHINT_DEFAULT_DISTANCE, 0 },
	// "hidden": {}
	{ "hidden",
		{ NULL, NULL, NULL },
		{ NULL, NULL, NULL },
		0, 0 },
};

static float ObjectHint_MaxDistance( void )
{
	float flMax = 0;
	for ( int i = 0; i < (int)ARRAYSIZE( s_Templates ); i++ )
	{
		if ( s_Templates[i].distance > flMax )
			flMax = s_Templates[i].distance;
	}
	return flMax;
}

static const ObjectHintSpec *ObjectHint_GetSpec( const char *szName )
{
	if ( !szName || !*szName )
		return NULL;
	for ( int i = 0; i < (int)ARRAYSIZE( s_Templates ); i++ )
	{
		if ( !strcmp( s_Templates[i].name, szName ) )
			return &s_Templates[i];
	}
	return NULL;
}

// Featureful GetHintSpecForEntity: the entity's own template; SBO maps no classname or pickup name to one
static const ObjectHintSpec *ObjectHint_SpecForEntity( CBaseEntity *pEntity )
{
	if ( FStringNull( pEntity->m_objectHint ) )
		return NULL;
	return ObjectHint_GetSpec( STRING( pEntity->m_objectHint ) );
}

//---------------------------------------------------------
// Map state
//---------------------------------------------------------

static BOOL s_bMapHasHints = FALSE;		// an entity of this map has (had) an "objecthint" key: else no scans at all
static float s_flMaxEntityDistance = 0;	// the largest "objecthint_distance" this map has (had): the search covers it
static float s_flNextSend[ 33 ];		// per player index: the next send time

void ObjectHint_Reset( void )
{
	s_bMapHasHints = FALSE;
	s_flMaxEntityDistance = 0;
	for ( int i = 0; i < (int)ARRAYSIZE( s_flNextSend ); i++ )
		s_flNextSend[i] = 0;
}

// "objecthint_distance": the entity's scan distance in units; empty or 0 (or less): the template's
static void ObjectHint_SetDistance( CBaseEntity *pEntity, const char *szValue )
{
	float flDistance = ( szValue && *szValue ) ? (float)atoi( szValue ) : 0.0f;
	if ( flDistance < 0 )
		flDistance = 0;
	else if ( flDistance > OBJECTHINT_MAX_DISTANCE )
		flDistance = OBJECTHINT_MAX_DISTANCE;

	pEntity->m_flObjectHintDistance = flDistance;
	if ( flDistance > s_flMaxEntityDistance )
		s_flMaxEntityDistance = flDistance;
}

BOOL ObjectHint_KeyValue( CBaseEntity *pEntity, const char *szKeyName, const char *szValue )
{
	if ( !pEntity || !szKeyName )
		return FALSE;

	if ( !stricmp( szKeyName, "objecthint_distance" ) )
	{
		ObjectHint_SetDistance( pEntity, szValue );
		return TRUE;
	}

	if ( stricmp( szKeyName, "objecthint" ) )
		return FALSE;

	if ( !szValue || !*szValue )
	{
		pEntity->m_objectHint = iStringNull;
		return TRUE;
	}

	pEntity->m_objectHint = ALLOC_STRING( szValue );
	s_bMapHasHints = TRUE;
	if ( !ObjectHint_GetSpec( szValue ) )
		ALERT( at_console, "%s: objecthint \"%s\" is not a template (button, charger, eyescanner, pickup, hidden)\n",
			STRING( pEntity->pev->classname ), szValue );
	return TRUE;
}

//---------------------------------------------------------
// Gathering (Featureful player.cpp)
//---------------------------------------------------------

static BOOL ObjectInRadius( CBaseEntity *pEntity, const Vector &origin, float radius )
{
	radius *= radius;
	float distSquared = 0.0f;

	entvars_t *pev = pEntity->pev;
	for ( int i = 0; i < 3 && distSquared <= radius; i++ )
	{
		float axisDist = 0.0f;
		if ( origin[i] < pev->absmin[i] )
			axisDist = origin[i] - pev->absmin[i];
		else if ( origin[i] > pev->absmax[i] )
			axisDist = origin[i] - pev->absmax[i];

		distSquared += axisDist * axisDist;
	}

	return distSquared < radius;
}

// What SBO's CBasePlayer::PlayerUse would use now: the usable entity within 64 units most in front of the view
static CBaseEntity *ObjectHint_UseTarget( CBasePlayer *pPlayer )
{
	CBaseEntity *pObject = NULL;
	CBaseEntity *pClosest = NULL;
	float flMaxDot = VIEW_FIELD_NARROW;

	while ( ( pObject = UTIL_FindEntityInSphere( pObject, pPlayer->pev->origin, OBJECTHINT_USE_RADIUS ) ) != NULL )
	{
		if ( pObject->ObjectCaps() & ( FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE ) )
		{
			Vector vecLOS = VecBModelOrigin( pObject->pev ) - ( pPlayer->pev->origin + pPlayer->pev->view_ofs );
			vecLOS = UTIL_ClampVectorToBox( vecLOS, pObject->pev->size * 0.5 );

			float flDot = DotProduct( vecLOS, gpGlobals->v_forward );
			if ( flDot > flMaxDot )
			{
				pClosest = pObject;
				flMaxDot = flDot;
			}
		}
	}
	return pClosest;
}

struct HintedEntity
{
	CBaseEntity				*pEntity;
	const ObjectHintSpec	*pSpec;
};

#define OBJECTHINT_MAX_GATHERED	64	// scan hints sent to one player per interval

// Featureful GetInteractiveEntity, the gatherHints part: hinted entities in their scan distance, in front of the
// view (60 degrees from it, to the nearest point of the object's box) and not behind a wall (a see-through brush in
// between is allowed). SBO: the search starts at the eyes, where the distance is measured, and reaches
// OBJECTHINT_SEARCH_MARGIN further, because the engine's sphere search measures to the box center.
static int ObjectHint_GatherScan( CBasePlayer *pPlayer, HintedEntity *pOut, int iMax )
{
	int iCount = 0;
	const Vector eyePosition = pPlayer->EyePosition();
	float searchRadius = ObjectHint_MaxDistance();
	if ( s_flMaxEntityDistance > searchRadius )
		searchRadius = s_flMaxEntityDistance;
	if ( OBJECTHINT_USE_RADIUS > searchRadius )
		searchRadius = OBJECTHINT_USE_RADIUS;
	searchRadius += OBJECTHINT_SEARCH_MARGIN;

	CBaseEntity *pObject = NULL;
	while ( iCount < iMax && ( pObject = UTIL_FindEntityInSphere( pObject, eyePosition, searchRadius ) ) != NULL )
	{
		if ( !pObject->CanShowObjectHint() )
			continue;
		const ObjectHintSpec *hintSpec = ObjectHint_SpecForEntity( pObject );
		if ( !hintSpec || !hintSpec->scanVisualSet.HasAnySpriteDefined() )
			continue;

		float radius = pObject->m_flObjectHintDistance > 0 ? pObject->m_flObjectHintDistance : hintSpec->distance;
		if ( radius <= 0 )
			radius = OBJECTHINT_USE_RADIUS;
		if ( !ObjectInRadius( pObject, eyePosition, radius ) )
			continue;

		Vector vecLOS = VecBModelOrigin( pObject->pev ) - eyePosition;
		vecLOS = UTIL_ClampVectorToBox( vecLOS, pObject->pev->size * 0.5f );
		if ( DotProduct( vecLOS, gpGlobals->v_forward ) <= 0.5f )
			continue;

		TraceResult objectTr;
		UTIL_TraceLine( eyePosition, pObject->Center(), dont_ignore_monsters, pPlayer->edict(), &objectTr );

		BOOL traceOk = objectTr.flFraction == 1.0f || objectTr.pHit == pObject->edict();
		if ( !traceOk && objectTr.pHit && ENTINDEX( objectTr.pHit ) != 0 )
		{
			entvars_t *objectPev = VARS( objectTr.pHit );
			const char *model = FStringNull( objectPev->model ) ? NULL : STRING( objectPev->model );
			if ( model && *model == '*' && objectPev->rendermode != kRenderNormal && objectPev->renderamt > 0 )
			{
				TraceResult reversedTr;
				UTIL_TraceLine( pObject->Center(), eyePosition, dont_ignore_monsters, pObject->edict(), &reversedTr );
				traceOk = reversedTr.pHit == objectTr.pHit;
			}
		}

		if ( traceOk )
		{
			pOut[iCount].pEntity = pObject;
			pOut[iCount].pSpec = hintSpec;
			iCount++;
		}
	}
	return iCount;
}

// Featureful CalcHintOrigin: on a brush, the hint sits on the face the player looks at
static Vector CalcHintOrigin( CBaseEntity *pEntity, CBaseEntity *pLooker, int flags )
{
	if ( pEntity->IsBSPModel() && !FStringNull( pEntity->pev->model ) && ( STRING( pEntity->pev->model ) )[0] == '*' )
	{
		Vector center = pEntity->Center();

		TraceResult tr;
		UTIL_TraceLine( pLooker->EyePosition(), center, ignore_monsters, pLooker->edict(), &tr );

		if ( tr.pHit == pEntity->edict() || FBitSet( flags, OBJECTHINT_FLAG_CLOSEST ) )
		{
			float maxDist = 0.0f;
			int axis = 0;
			for ( int i = 0; i < 3; ++i )
			{
				float dist = center[i] - tr.vecEndPos[i];
				if ( fabs( dist ) > fabs( maxDist ) )
				{
					maxDist = dist;
					axis = i;
				}
			}
			center[axis] = tr.vecEndPos[axis];
		}
		return center;
	}
	return pEntity->Center();
}

static const ObjectHintVisual *ChooseHintVisual( CBasePlayer *pPlayer, CBaseEntity *pEntity, const ObjectHintVisualSet &visualSet )
{
	// no point in choosing if these are the same
	if ( visualSet.defaultVisual != visualSet.unusableVisual )
	{
		if ( !pEntity->IsUsefulToDisplayHint( pPlayer ) )
			return visualSet.unusableVisual;
	}
	if ( visualSet.defaultVisual != visualSet.lockedVisual )
	{
		// Featureful makes IsLockedByMaster virtual; here CBaseEntity's stays the HL SDK's non-virtual FALSE (monsters
		// open doors through it, monsters.cpp) and the toggles' (buttons, doors, chargers) is asked directly
		CBaseToggle *pToggle = pEntity->MyTogglePointer();
		if ( pToggle && pToggle->IsLockedByMaster() )
			return visualSet.lockedVisual;
	}
	return visualSet.defaultVisual;
}

static inline void WriteCoordClamped( float f )
{
	if ( f > 4095.0f ) f = 4095.0f;
	else if ( f < -4095.0f ) f = -4095.0f;
	WRITE_COORD( f );
}

static void SendObjectHint( CBasePlayer *pPlayer, const ObjectHintSpec *spec, const ObjectHintVisual *hintVisual, CBaseEntity *pEntity, int flags )
{
	const Vector center = CalcHintOrigin( pEntity, pPlayer, flags ) + Vector( 0, 0, spec->verticalOffset );

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgObjectHint, NULL, pPlayer->pev );
		WRITE_BYTE( flags );
		WRITE_SHORT( pEntity->entindex() );
		WRITE_BYTE( hintVisual->r );
		WRITE_BYTE( hintVisual->g );
		WRITE_BYTE( hintVisual->b );
		WRITE_COORD( hintVisual->scale );
		WriteCoordClamped( center.x );
		WriteCoordClamped( center.y );
		WriteCoordClamped( center.z );
		WriteCoordClamped( pEntity->pev->size.x );
		WriteCoordClamped( pEntity->pev->size.y );
		WriteCoordClamped( pEntity->pev->size.z );
		WRITE_STRING( hintVisual->sprite );
	MESSAGE_END();
}

// Featureful GatherAndSendObjectHints
static void GatherAndSendObjectHints( CBasePlayer *pPlayer )
{
	UTIL_MakeVectors( pPlayer->pev->v_angle );

	CBaseEntity *pClosest = ObjectHint_UseTarget( pPlayer );
	if ( pClosest && pClosest->pev->modelindex && FBitSet( pClosest->pev->effects, EF_NODRAW ) )
		pClosest = NULL;
	const ObjectHintSpec *closestHintSpec = pClosest ? ObjectHint_SpecForEntity( pClosest ) : NULL;
	const ObjectHintVisual *closestHintVisual = NULL;
	BOOL shownInteraction = FALSE;
	if ( closestHintSpec )
		closestHintVisual = ChooseHintVisual( pPlayer, pClosest, closestHintSpec->interactionVisualSet );

	if ( pClosest && closestHintVisual )
	{
		SendObjectHint( pPlayer, closestHintSpec, closestHintVisual, pClosest, OBJECTHINT_FLAG_CLOSEST );
		shownInteraction = TRUE;
	}
	else
	{
		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgObjectHint, NULL, pPlayer->pev );
			WRITE_BYTE( OBJECTHINT_FLAG_CLOSEST );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	HintedEntity hinted[ OBJECTHINT_MAX_GATHERED ];
	const int iCount = ObjectHint_GatherScan( pPlayer, hinted, OBJECTHINT_MAX_GATHERED );
	for ( int i = 0; i < iCount; i++ )
	{
		CBaseEntity *pEntity = hinted[i].pEntity;
		if ( shownInteraction && pEntity == pClosest )
			continue; // already handled

		const ObjectHintVisual *hintVisual = ChooseHintVisual( pPlayer, pEntity, hinted[i].pSpec->scanVisualSet );
		if ( hintVisual )
			SendObjectHint( pPlayer, hinted[i].pSpec, hintVisual, pEntity, 0 );
	}
}

void ObjectHint_PlayerPostThink( CBasePlayer *pPlayer )
{
	if ( !s_bMapHasHints || !gmsgObjectHint || !pPlayer )
		return;

	const int iIndex = pPlayer->entindex();
	if ( iIndex < 1 || iIndex >= (int)ARRAYSIZE( s_flNextSend ) )
		return;

	// a time from a previous map or a changelevel back in time: send now
	if ( s_flNextSend[iIndex] > gpGlobals->time + OBJECTHINT_INTERVAL )
		s_flNextSend[iIndex] = 0;
	if ( s_flNextSend[iIndex] > gpGlobals->time )
		return;
	s_flNextSend[iIndex] = gpGlobals->time + OBJECTHINT_INTERVAL;

	// Featureful: living players only. SBO: spectators and the dead (observers) see no hints; bots have no client.
	// m_fGameHUDInitialized: the client has its HUD (InitHUD sent), so it knows the message.
	if ( !pPlayer->IsAlive() || pPlayer->IsObserver() || ( pPlayer->pev->flags & FL_FAKECLIENT ) || !pPlayer->m_fGameHUDInitialized )
		return;

	GatherAndSendObjectHints( pPlayer );
}
