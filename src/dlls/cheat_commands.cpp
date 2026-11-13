//=========================================================
// SBO 2.0: cheat and entity debug commands, ported from the Half-Life Unified SDK
// (https://github.com/twhl-community/halflife-unified-sdk, src/game/server/client.cpp SV_CreateClientCommands and
// entities/player.cpp CBasePlayer::ToggleCheat; docs/features/misc-commands.md), plus cheat_speed. Client commands,
// refused unless sv_cheats is 1 (HLU UTIL_CheatsAllowed), in every game mode. docs/CHEAT_COMMANDS.md
//
// What differs from HLU:
// - cheat_*: an optional 0 / 1 forces the state; the console line names the command ("cheat_god ON").
// - Every cheat ends when the player respawns, as impulse 210 / 211 / 212 and the engine's god / notarget do in SBO
//   (CBasePlayer::Spawn).
// - ent_create / npc_create refuse a class that would precache a model, sound, file or event the map has not loaded
//   (Valve's engines stop the server on a precache after the map has loaded): see "Precache guard" below.
// - ent_show_origin / ent_show_center / ent_show_bbox find the entity by targetname or classname, as HLU's docs say
//   (its code takes a targetname only), and the dot is the flare sprite every map loads (SBO has no laserdot.spr).
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "cheat_commands.h"

#include <stdarg.h>
#include <ctype.h>

#define CHEAT_MAX_DISTANCE	8192	// how far ahead "the entity / the place you're looking at" is looked for
#define CHEAT_PAGE_SIZE		10		// HLU NumberOfEntitiesPerPage

static void Print( CBasePlayer *pPlayer, const char *fmt, ... )
{
	char buf[ 256 ];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, args );
	va_end( args );
	buf[ sizeof( buf ) - 1 ] = '\0';
	CLIENT_PRINTF( pPlayer->edict(), print_console, buf );
}

static int Argc( void ) { return CMD_ARGC(); }
static const char *Argv( int i ) { return i < CMD_ARGC() ? CMD_ARGV( i ) : ""; }

//=========================================================
// Precache guard. A precache after the map has loaded is a Host_Error on Valve's engines (and so is SET_MODEL or
// MODEL_INDEX of a model that isn't precached), unless the name was already precached: then the engine just returns
// its index. So every name the game precaches is noted here (the wrappers stay in g_engfuncs, like the test layer's;
// the cost is one table lookup per precache call). While ent_create / npc_create spawn an entity, a precache, set
// model or model index of a name the map has not loaded doesn't reach the engine: it is noted as missing, and the
// new entity is removed after its Spawn. Brush models ("*1") are the map's own. Names precached by server plugins
// directly through the engine aren't seen here, so such a class is refused (never a crash).
//=========================================================
enum { RES_MODEL, RES_SOUND, RES_GENERIC, RES_EVENT, RES_KINDS };

#define RES_SLOTS		1024	// per kind, a power of two: the engine allows 512 models / sounds / generic, 256 events
#define RES_NAME_LEN	64		// the engine's resource name limit

static char s_resNames[ RES_KINDS ][ RES_SLOTS ][ RES_NAME_LEN ];
static int s_resCount[ RES_KINDS ];
static enginefuncs_t s_engine;			// the functions the wrappers call through to
static BOOL s_bGuard;					// an ent_create / npc_create spawn is running
static char s_szMissing[ RES_NAME_LEN ];	// the first resource that spawn wanted and the map has not loaded
static int s_iMissing;

// lower-case copy; FALSE when the name doesn't fit (never noted, so never "loaded")
static BOOL ResKey( const char *name, char *key )
{
	if ( !name )
		return FALSE;
	int i = 0;
	for ( ; name[ i ]; i++ )
	{
		if ( i >= RES_NAME_LEN - 1 )
			return FALSE;
		key[ i ] = (char)tolower( (unsigned char)name[ i ] );
	}
	key[ i ] = '\0';
	return i > 0;
}

static unsigned int ResHash( const char *key )
{
	unsigned int h = 2166136261u;	// FNV-1a
	for ( ; *key; key++ )
		h = ( h ^ (unsigned char)*key ) * 16777619u;
	return h;
}

// the slot holding key, or the free slot where it goes (-1: table full)
static int ResSlot( int kind, const char *key )
{
	unsigned int h = ResHash( key );
	for ( int n = 0; n < RES_SLOTS; n++ )
	{
		int slot = (int)( ( h + n ) & ( RES_SLOTS - 1 ) );
		if ( !s_resNames[ kind ][ slot ][ 0 ] || !strcmp( s_resNames[ kind ][ slot ], key ) )
			return slot;
	}
	return -1;
}

static void ResNote( int kind, const char *name )
{
	char key[ RES_NAME_LEN ];
	if ( !ResKey( name, key ) || s_resCount[ kind ] >= RES_SLOTS / 2 )
		return;
	int slot = ResSlot( kind, key );
	if ( slot >= 0 && !s_resNames[ kind ][ slot ][ 0 ] )
	{
		strcpy( s_resNames[ kind ][ slot ], key );	// fits: ResKey checked the length
		s_resCount[ kind ]++;
	}
}

static BOOL ResLoaded( int kind, const char *name )
{
	if ( kind == RES_MODEL && name && name[ 0 ] == '*' )
		return TRUE;	// a brush model of the map
	char key[ RES_NAME_LEN ];
	if ( !ResKey( name, key ) )
		return FALSE;
	int slot = ResSlot( kind, key );
	return slot >= 0 && s_resNames[ kind ][ slot ][ 0 ];
}

BOOL Cheat_SoundPrecached( const char *name )
{
	return ResLoaded( RES_SOUND, name );
}

// TRUE: the call may go to the engine. FALSE (guarded spawn, name not loaded): noted as missing
static BOOL ResAllowed( int kind, const char *name )
{
	if ( !s_bGuard || ResLoaded( kind, name ) )
		return TRUE;
	if ( !s_iMissing++ )
	{
		strncpy( s_szMissing, name ? name : "", sizeof( s_szMissing ) - 1 );
		s_szMissing[ sizeof( s_szMissing ) - 1 ] = '\0';
	}
	return FALSE;
}

static int G_PrecacheModel( char *s )
{
	if ( !ResAllowed( RES_MODEL, s ) )
		return 0;
	int index = s_engine.pfnPrecacheModel( s );
	ResNote( RES_MODEL, s );
	return index;
}

static int G_PrecacheSound( char *s )
{
	if ( !ResAllowed( RES_SOUND, s ) )
		return 0;
	int index = s_engine.pfnPrecacheSound( s );
	ResNote( RES_SOUND, s );
	return index;
}

static int G_PrecacheGeneric( char *s )
{
	if ( !ResAllowed( RES_GENERIC, s ) )
		return 0;
	int index = s_engine.pfnPrecacheGeneric( s );
	ResNote( RES_GENERIC, s );
	return index;
}

static unsigned short G_PrecacheEvent( int type, const char *psz )
{
	if ( !ResAllowed( RES_EVENT, psz ) )
		return 0;
	unsigned short index = s_engine.pfnPrecacheEvent( type, psz );
	ResNote( RES_EVENT, psz );
	return index;
}

static void G_SetModel( edict_t *e, const char *m )
{
	if ( m && m[ 0 ] && !ResAllowed( RES_MODEL, m ) )
		return;
	s_engine.pfnSetModel( e, m );
}

static int G_ModelIndex( const char *m )
{
	if ( !ResAllowed( RES_MODEL, m ) )
		return 0;
	return s_engine.pfnModelIndex( m );
}

void Cheat_HookEngine( enginefuncs_t *pFuncs )
{
	s_engine = *pFuncs;
	pFuncs->pfnPrecacheModel = G_PrecacheModel;
	pFuncs->pfnPrecacheSound = G_PrecacheSound;
	pFuncs->pfnPrecacheGeneric = G_PrecacheGeneric;
	pFuncs->pfnPrecacheEvent = G_PrecacheEvent;
	pFuncs->pfnSetModel = G_SetModel;
	pFuncs->pfnModelIndex = G_ModelIndex;
}

void Cheat_ServerDeactivate( void )
{
	memset( s_resNames, 0, sizeof( s_resNames ) );
	memset( s_resCount, 0, sizeof( s_resCount ) );
}

//=========================================================
// Entity lookups (HLU util.cpp)
//=========================================================

// HLU UTIL_FindEntityForward: the entity under the crosshair
static CBaseEntity *FindEntityForward( CBasePlayer *pPlayer )
{
	TraceResult tr;
	UTIL_MakeVectors( pPlayer->pev->v_angle );
	Vector vecSrc = pPlayer->pev->origin + pPlayer->pev->view_ofs;
	UTIL_TraceLine( vecSrc, vecSrc + gpGlobals->v_forward * CHEAT_MAX_DISTANCE, dont_ignore_monsters, pPlayer->edict(), &tr );
	if ( tr.flFraction != 1.0 && !FNullEnt( tr.pHit ) )
		return CBaseEntity::Instance( tr.pHit );
	return NULL;
}

static CBaseEntity *EntityAt( int index )
{
	edict_t *e = INDEXENT( index );
	if ( !e || e->free || !e->pvPrivateData )
		return NULL;
	return CBaseEntity::Instance( e );
}

// HLU UTIL_FindEntityByIdentifier: the next entity after pStart whose targetname or classname is needle
static CBaseEntity *FindEntityByIdentifier( CBaseEntity *pStart, const char *needle )
{
	for ( int i = pStart ? pStart->entindex() + 1 : 1; i < gpGlobals->maxEntities; i++ )
	{
		CBaseEntity *pEntity = EntityAt( i );
		if ( !pEntity )
			continue;
		if ( !FStringNull( pEntity->pev->targetname ) && FStrEq( needle, STRING( pEntity->pev->targetname ) ) )
			return pEntity;
		if ( FStrEq( needle, STRING( pEntity->pev->classname ) ) )
			return pEntity;
	}
	return NULL;
}

// HLU UTIL_IsRemovableEntity: never a player
static BOOL IsRemovable( CBaseEntity *pEntity )
{
	return pEntity && !pEntity->IsPlayer() && pEntity->entindex() > 0;
}

static CBaseEntity *FindNextToRemove( CBasePlayer *pPlayer, CBaseEntity *pStart, const char *needle )
{
	for ( CBaseEntity *pEntity = pStart; ( pEntity = FindEntityByIdentifier( pEntity, needle ) ) != NULL; )
	{
		if ( !IsRemovable( pEntity ) )
		{
			Print( pPlayer, "Can't remove \"%s\": not allowed to remove entities of this type\n", STRING( pEntity->pev->classname ) );
			continue;
		}
		return pEntity;
	}
	return NULL;
}

//=========================================================
// Entity lists (HLU PageBasedEntitySearch)
//=========================================================
enum { FILTER_ALL, FILTER_CLASSNAME, FILTER_TARGETNAME };

static void ListEntities( CBasePlayer *pPlayer, int filter, const char *needle, int page )
{
	if ( page < 1 )
		page = 1;

	Print( pPlayer, "entindex - classname - targetname - origin\n" );

	int total = 0;
	for ( int i = 1; i < gpGlobals->maxEntities; i++ )
	{
		CBaseEntity *pEntity = EntityAt( i );
		if ( !pEntity )
			continue;
		if ( filter == FILTER_CLASSNAME && !FStrEq( needle, STRING( pEntity->pev->classname ) ) )
			continue;
		if ( filter == FILTER_TARGETNAME && !FStrEq( needle, STRING( pEntity->pev->targetname ) ) )
			continue;

		if ( total / CHEAT_PAGE_SIZE + 1 == page )
		{
			const Vector &o = pEntity->pev->origin;
			Print( pPlayer, "%d - %s - %s - {%g %g %g}\n", i, STRING( pEntity->pev->classname ),
				STRING( pEntity->pev->targetname ), o.x, o.y, o.z );
		}
		total++;
	}

	int pages = total > 0 ? ( total + CHEAT_PAGE_SIZE - 1 ) / CHEAT_PAGE_SIZE : 1;
	if ( filter == FILTER_ALL )
		Print( pPlayer, "Total %d / %d entities (page %d / %d)\n", total, gpGlobals->maxEntities, page, pages );
	else
		Print( pPlayer, "%d entities having the %s: \"%s\" (page %d / %d)\n", total,
			filter == FILTER_CLASSNAME ? "classname" : "targetname", needle, page, pages );
}

//=========================================================
// ent_create / npc_create (HLU TryCreateEntity)
//=========================================================
static CBaseEntity *TryCreateEntity( CBasePlayer *pPlayer, const char *pszClass, float flYaw )
{
	if ( FStrEq( pszClass, "player" ) || FStrEq( pszClass, "worldspawn" ) )
	{
		Print( pPlayer, "Can't create \"%s\"\n", pszClass );
		return NULL;
	}

	UTIL_MakeVectors( pPlayer->pev->v_angle );
	TraceResult tr;
	UTIL_TraceLine( pPlayer->pev->origin, pPlayer->pev->origin + gpGlobals->v_forward * CHEAT_MAX_DISTANCE,
		dont_ignore_monsters, pPlayer->edict(), &tr );

	if ( tr.fStartSolid )
	{
		Print( pPlayer, "Cannot create entity in solid object\n" );
		return NULL;
	}

	edict_t *e = CREATE_NAMED_ENTITY( ALLOC_STRING( pszClass ) );
	if ( FNullEnt( e ) || !e->pvPrivateData )
	{
		Print( pPlayer, "No entity class \"%s\"\n", pszClass );
		if ( !FNullEnt( e ) )
			REMOVE_ENTITY( e );
		return NULL;
	}

	e->v.origin = tr.vecEndPos;
	e->v.angles = Vector( 0, flYaw, 0 );

	// the keyvalues after the classname, in pairs (a last key without a value is left out)
	for ( int i = 2; i + 1 < Argc(); i += 2 )
	{
		if ( FStrEq( Argv( i ), "classname" ) )
			continue;	// as the engine does for a map's entities
		KeyValueData kvd;
		kvd.szClassName = (char *)STRING( e->v.classname );
		kvd.szKeyName = (char *)Argv( i );
		kvd.szValue = (char *)Argv( i + 1 );
		kvd.fHandled = FALSE;
		DispatchKeyValue( e, &kvd );
	}

	s_bGuard = TRUE;
	s_iMissing = 0;
	int result = DispatchSpawn( e );
	s_bGuard = FALSE;

	CBaseEntity *pEntity = e->free ? NULL : CBaseEntity::Instance( e );

	if ( s_iMissing )
	{
		Print( pPlayer, "Can't create \"%s\": it needs \"%s\", which this map has not loaded\n", pszClass, s_szMissing );
		if ( pEntity )
			UTIL_Remove( pEntity );
		return NULL;
	}

	if ( result == -1 || !pEntity || ( e->v.flags & FL_KILLME ) )
	{
		Print( pPlayer, "Entity was removed during spawn\n" );
		return NULL;
	}

	Print( pPlayer, "Created %s:%d\n", pszClass, pEntity->entindex() );
	return pEntity;
}

//=========================================================
// ent_show_*: a 10 second marker, seen by the player who asked (HLU)
//=========================================================
static void ShowPoint( CBasePlayer *pPlayer, const Vector &vec )
{
	MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, pPlayer->edict() );
		WRITE_BYTE( TE_FIREFIELD );
		WRITE_COORD( vec.x );
		WRITE_COORD( vec.y );
		WRITE_COORD( vec.z );
		WRITE_SHORT( 0 );
		WRITE_SHORT( m_psprite );	// sprites/xflare1.spr (W_Precache)
		WRITE_BYTE( 1 );
		WRITE_BYTE( TEFIRE_FLAG_ADDITIVE );
		WRITE_BYTE( 100 );	// 10 seconds
	MESSAGE_END();
}

static CBaseEntity *ShowTarget( CBasePlayer *pPlayer, const char *pszCmd )
{
	if ( Argc() < 2 )
	{
		Print( pPlayer, "Usage: %s <targetname|classname>\n", pszCmd );
		return NULL;
	}
	CBaseEntity *pTarget = FindEntityByIdentifier( NULL, Argv( 1 ) );
	if ( !pTarget )
		Print( pPlayer, "No entity found\n" );
	return pTarget;
}

//=========================================================
// The cheats (HLU CBasePlayer::ToggleCheat)
//=========================================================

// toggles, or with an argument 0 / 1 sets; returns the new state
static BOOL CheatState( BOOL current )
{
	if ( Argc() > 1 )
		return atoi( Argv( 1 ) ) != 0;
	return !current;
}

static void PrintCheat( CBasePlayer *pPlayer, const char *pszCmd, BOOL on )
{
	Print( pPlayer, "%s %s\n", pszCmd, on ? "ON" : "OFF" );
}

// HLU SV_TestEntityPosition / FindPassableSpace: leaving noclip inside something moves the player out of it
static BOOL StuckAt( CBaseEntity *pEntity )
{
	TraceResult tr;
	TRACE_MONSTER_HULL( pEntity->edict(), pEntity->pev->origin, pEntity->pev->origin, dont_ignore_monsters, NULL, &tr );
	return tr.fStartSolid != 0;
}

static BOOL FindPassableSpace( CBaseEntity *pEntity, const Vector &direction, float step )
{
	for ( int i = 100; i > 0; --i )
	{
		pEntity->pev->origin = pEntity->pev->origin + step * direction;
		if ( !StuckAt( pEntity ) )
		{
			pEntity->pev->oldorigin = pEntity->pev->origin;
			return TRUE;
		}
	}
	return FALSE;
}

static void NoclipOff( CBasePlayer *pPlayer )
{
	entvars_t *pev = pPlayer->pev;
	pev->movetype = MOVETYPE_WALK;
	pev->oldorigin = pev->origin;
	if ( StuckAt( pPlayer ) )
	{
		UTIL_MakeVectors( pev->v_angle );
		Vector forward = gpGlobals->v_forward, right = gpGlobals->v_right, up = gpGlobals->v_up;
		if ( !FindPassableSpace( pPlayer, forward, 1 ) && !FindPassableSpace( pPlayer, right, 1 ) &&
			!FindPassableSpace( pPlayer, right, -1 ) && !FindPassableSpace( pPlayer, up, 1 ) &&
			!FindPassableSpace( pPlayer, up, -1 ) && !FindPassableSpace( pPlayer, forward, -1 ) )
			ALERT( at_console, "Can't find the world\n" );
		pev->origin = pev->oldorigin;
		UTIL_SetOrigin( pev, pev->origin );
	}
}

//=========================================================
// The commands
//=========================================================
typedef void ( *CheatFn )( CBasePlayer *pPlayer, const char *pszCmd );

static void Cmd_God( CBasePlayer *p, const char *c )
{
	BOOL on = CheatState( FBitSet( p->pev->flags, FL_GODMODE ) );
	if ( on ) SetBits( p->pev->flags, FL_GODMODE ); else ClearBits( p->pev->flags, FL_GODMODE );
	PrintCheat( p, c, on );
}

static void Cmd_Unkillable( CBasePlayer *p, const char *c )
{
	p->m_bCheatUnkillable = CheatState( p->m_bCheatUnkillable );
	PrintCheat( p, c, p->m_bCheatUnkillable );
}

static void Cmd_Notarget( CBasePlayer *p, const char *c )
{
	BOOL on = CheatState( FBitSet( p->pev->flags, FL_NOTARGET ) );
	if ( on ) SetBits( p->pev->flags, FL_NOTARGET ); else ClearBits( p->pev->flags, FL_NOTARGET );
	PrintCheat( p, c, on );
}

static void Cmd_Noclip( CBasePlayer *p, const char *c )
{
	// as impulse 200: only walking <-> noclip, so observers and other movetypes are left alone
	if ( p->pev->movetype != MOVETYPE_WALK && p->pev->movetype != MOVETYPE_NOCLIP )
	{
		Print( p, "%s: not while you aren't walking\n", c );
		return;
	}
	BOOL on = CheatState( p->pev->movetype == MOVETYPE_NOCLIP );
	if ( on )
		p->pev->movetype = MOVETYPE_NOCLIP;
	else if ( p->pev->movetype == MOVETYPE_NOCLIP )
		NoclipOff( p );
	PrintCheat( p, c, on );
}

static void Cmd_InfiniteAir( CBasePlayer *p, const char *c )
{
	p->m_bCheatInfiniteAir = CheatState( p->m_bCheatInfiniteAir );
	PrintCheat( p, c, p->m_bCheatInfiniteAir );
}

static void Cmd_InfiniteArmor( CBasePlayer *p, const char *c )
{
	p->m_bCheatInfiniteArmor = CheatState( p->m_bCheatInfiniteArmor );
	PrintCheat( p, c, p->m_bCheatInfiniteArmor );
}

static void Cmd_Speed( CBasePlayer *p, const char *c )
{
	float flScale = Argc() > 1 ? atof( Argv( 1 ) ) : 1.0f;
	if ( !( flScale > 0 && flScale <= 100 ) )
	{
		Print( p, "Usage: %s <multiplier> (more than 0; 1 or none: normal speed)\n", c );
		return;
	}
	p->CheatSpeed( flScale );
	if ( flScale == 1.0f )
		Print( p, "%s OFF (normal speed %g)\n", c, p->pev->maxspeed );
	else
		Print( p, "%s %g (max speed %g)\n", c, flScale, p->pev->maxspeed );
}

static void Cmd_FindByClassname( CBasePlayer *p, const char *c )
{
	if ( Argc() > 1 )
		ListEntities( p, FILTER_CLASSNAME, Argv( 1 ), Argc() > 2 ? atoi( Argv( 2 ) ) : 1 );
	else
		Print( p, "usage: %s <classname> [page]\n", c );
}

static void Cmd_FindByTargetname( CBasePlayer *p, const char *c )
{
	if ( Argc() > 1 )
		ListEntities( p, FILTER_TARGETNAME, Argv( 1 ), Argc() > 2 ? atoi( Argv( 2 ) ) : 1 );
	else
		Print( p, "usage: %s <targetname> [page]\n", c );
}

static void Cmd_Fire( CBasePlayer *p, const char *c )
{
	if ( Argc() < 2 )
	{
		Print( p, "usage: %s <targetname> [usetype] [value]\n", c );
		return;
	}
	int use = Argc() > 2 ? atoi( Argv( 2 ) ) : USE_TOGGLE;
	if ( use < USE_OFF ) use = USE_OFF;
	if ( use > USE_TOGGLE ) use = USE_TOGGLE;
	float value = Argc() > 3 ? atof( Argv( 3 ) ) : 0.0f;
	FireTargets( Argv( 1 ), p, p, (USE_TYPE)use, value );
}

static void Cmd_List( CBasePlayer *p, const char * )
{
	ListEntities( p, FILTER_ALL, "", Argc() > 1 ? atoi( Argv( 1 ) ) : 1 );
}

static void Cmd_Remove( CBasePlayer *p, const char *c )
{
	CBaseEntity *pEntity;
	if ( Argc() < 2 )
	{
		pEntity = FindEntityForward( p );
		if ( pEntity && !IsRemovable( pEntity ) )
		{
			Print( p, "Can't remove \"%s\": not allowed to remove entities of this type\n", STRING( pEntity->pev->classname ) );
			return;
		}
	}
	else
		pEntity = FindNextToRemove( p, NULL, Argv( 1 ) );

	if ( !pEntity )
	{
		Print( p, "No entity found\n" );
		return;
	}
	Print( p, "Removed %s:%d (%s)\n", STRING( pEntity->pev->classname ), pEntity->entindex(), STRING( pEntity->pev->targetname ) );
	UTIL_Remove( pEntity );
}

static void Cmd_RemoveAll( CBasePlayer *p, const char *c )
{
	if ( Argc() < 2 )
	{
		Print( p, "Usage: %s <classname|targetname>\n", c );
		return;
	}
	const char *needle = Argv( 1 );
	int count = 0;
	for ( CBaseEntity *pEntity = NULL; ( pEntity = FindNextToRemove( p, pEntity, needle ) ) != NULL; )
	{
		count++;
		UTIL_Remove( pEntity );
	}
	Print( p, "Removed %d entities matching \"%s\"\n", count, needle );
}

static void Cmd_SetName( CBasePlayer *p, const char *c )
{
	if ( Argc() < 2 )
	{
		Print( p, "Usage: %s <new targetname> [classname|targetname]\n", c );
		return;
	}
	CBaseEntity *pTarget = Argc() < 3 ? FindEntityForward( p ) : FindEntityByIdentifier( NULL, Argv( 2 ) );
	if ( !pTarget )
	{
		Print( p, "No entity found\n" );
		return;
	}
	Print( p, "Set the name of %s to %s\n", STRING( pTarget->pev->classname ), Argv( 1 ) );
	pTarget->pev->targetname = ALLOC_STRING( Argv( 1 ) );
}

static void Cmd_ShowOrigin( CBasePlayer *p, const char *c )
{
	CBaseEntity *pTarget = ShowTarget( p, c );
	if ( !pTarget )
		return;
	const Vector &o = pTarget->pev->origin;
	ShowPoint( p, o );
	Print( p, "Origin: %g %g %g\n", o.x, o.y, o.z );
}

static void Cmd_ShowCenter( CBasePlayer *p, const char *c )
{
	CBaseEntity *pTarget = ShowTarget( p, c );
	if ( !pTarget )
		return;
	Vector center = pTarget->Center();
	ShowPoint( p, center );
	Print( p, "Center: %g %g %g\n", center.x, center.y, center.z );
}

static void Cmd_ShowBBox( CBasePlayer *p, const char *c )
{
	CBaseEntity *pTarget = ShowTarget( p, c );
	if ( !pTarget )
		return;
	const Vector &mins = pTarget->pev->absmin, &maxs = pTarget->pev->absmax;
	MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, p->edict() );
		WRITE_BYTE( TE_BOX );
		WRITE_COORD( mins.x );
		WRITE_COORD( mins.y );
		WRITE_COORD( mins.z );
		WRITE_COORD( maxs.x );
		WRITE_COORD( maxs.y );
		WRITE_COORD( maxs.z );
		WRITE_SHORT( 100 );	// 10 seconds
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 255 );
	MESSAGE_END();
	Print( p, "BBox: min %g %g %g max %g %g %g\n", mins.x, mins.y, mins.z, maxs.x, maxs.y, maxs.z );
}

static void Cmd_Create( CBasePlayer *p, const char *c )
{
	if ( Argc() < 2 )
	{
		Print( p, "Usage: %s <classname> [<key> <value> <key2> <value2> ...]\n", c );
		return;
	}
	TryCreateEntity( p, Argv( 1 ), 0 );
}

static void Cmd_NpcCreate( CBasePlayer *p, const char *c )
{
	if ( Argc() < 2 )
	{
		Print( p, "Usage: %s <classname> [<key> <value> <key2> <value2> ...]\n", c );
		return;
	}
	CBaseEntity *pEntity = TryCreateEntity( p, Argv( 1 ), p->pev->v_angle.y );
	if ( !pEntity )
		return;

	// up by its box, so it stands on the floor it was put on
	if ( pEntity->pev->mins.z < 0 )
	{
		pEntity->pev->origin.z -= pEntity->pev->mins.z;
		UTIL_SetOrigin( pEntity->pev, pEntity->pev->origin );
	}

	// stuck in something, or nothing below it
	if ( !( pEntity->pev->flags & FL_FLY ) && pEntity->pev->movetype != MOVETYPE_FLY )
	{
		if ( DROP_TO_FLOOR( pEntity->edict() ) == 0 )
		{
			Print( p, "NPC fell out of level\n" );
			UTIL_Remove( pEntity );
		}
	}
}

static const struct { const char *name; CheatFn fn; } s_commands[] =
{
	{ "cheat_god", Cmd_God },
	{ "cheat_unkillable", Cmd_Unkillable },
	{ "cheat_notarget", Cmd_Notarget },
	{ "cheat_noclip", Cmd_Noclip },
	{ "cheat_infiniteair", Cmd_InfiniteAir },
	{ "cheat_infinitearmor", Cmd_InfiniteArmor },
	{ "cheat_speed", Cmd_Speed },
	{ "ent_create", Cmd_Create },
	{ "ent_find_by_classname", Cmd_FindByClassname },
	{ "ent_find_by_targetname", Cmd_FindByTargetname },
	{ "ent_fire", Cmd_Fire },
	{ "ent_list", Cmd_List },
	{ "ent_remove", Cmd_Remove },
	{ "ent_remove_all", Cmd_RemoveAll },
	{ "ent_setname", Cmd_SetName },
	{ "ent_show_bbox", Cmd_ShowBBox },
	{ "ent_show_center", Cmd_ShowCenter },
	{ "ent_show_origin", Cmd_ShowOrigin },
	{ "npc_create", Cmd_NpcCreate },
};

BOOL Cheat_ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	for ( int i = 0; i < (int)ARRAYSIZE( s_commands ); i++ )
	{
		if ( !FStrEq( pcmd, s_commands[ i ].name ) )
			continue;

		// sv_cheats is read on demand, as for the cheat impulses (CBasePlayer::CheatImpulseCommands)
		if ( CVAR_GET_FLOAT( "sv_cheats" ) == 0 )
			Print( pPlayer, "The command \"%s\" can only be used when cheats are enabled\n", s_commands[ i ].name );
		else
			s_commands[ i ].fn( pPlayer, s_commands[ i ].name );
		return TRUE;
	}
	return FALSE;
}
