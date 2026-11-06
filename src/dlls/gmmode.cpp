// SBO 2.0 Game Master mode. See gmmode.h and docs/GM_MODE.md.
//
// Port of MrCalvin's Game Master Mode plugin (amxmodx-plugins server/syndicate/gmmode/scripting/syndicate_gmmode.sma,
// "sma" below) as the owner wanted it in SBO 2.0:
//   * no Energy Points, costs, tier unlock times, cooldowns, max-alive / max-spawned limits or spawn_overrides.cfg
//     (spawning works as it always did in SBO's GM mode); a free-edict guard keeps the server from running out of
//     entities (a crash guard, not a game rule); a plugin can add a budget through the hooks CBasePlayer::GmCanPlace
//     / GmPlaced and the gm_entry_text server command (docs/GM_MODE.md "Plugin hooks");
//   * the spawn window is the client's VGUI2 window (cl_dll/vgui2/sbo_gm_spawn_menu.cpp) instead of the plugin's
//     text menus: it sends "gm_pick", "gm_goto", "gm_tp", "gm_help", "gm_leave", "gm_cancel";
//   * the intro is the VGUI2 GM window (ShowVGUI 19) over a black screen drawn by the client (no black box model);
//   * entering and leaving are co-op's (CCoop::GameMasterOn / GameMasterOff, "gmon", the gm_toggle server command).
// The plugin's engine/game hooks are direct calls here: StartObserver, UTIL_PrecacheOther, the PM_Move speed (pm_shared:
// GM_IUSER4_BIT), CCoop::CheckWho and the round counters (co-op's IsRoundCandidate already leaves Game Masters out).

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"game.h"
#include	"coop.h"
#include	"entity_state.h"
#include	"pm_shared.h"	// OBS_ROAMING
#include	"gmmode.h"
#include	"gm_catalog.h"
#include	"effects.h"	// CSprite (teleporter markers)
#include	"doors.h"	// SF_DOOR_* (doors on an order's way)
#include	"soundent.h"	// bits_SOUND_* (the GM move schedule)

#ifndef _WIN32
#define _snprintf snprintf
#endif

extern int gmsgShowMenu;
extern int gmsgGmMode;
extern int gmsgGmEntry;
extern int gmsgGmStatus;
extern int gmsgGmPlayers;
extern int gmsgGmNotice;
extern int gmsgGmMonsters;
extern void ShowVGUI( CBasePlayer *pPlayer, int iMenu );
extern int GmEngage;

#define GM_DISPLAY_MODEL			"models/gmmode.mdl"
#define GM_DISPLAY_BODY_ARROW		22
#define GM_DISPLAY_BODY_BASE		23

#define GM_SPAWN_SOUND				"debris/beamstart7.wav"
#define GM_SPAWN_SPRITE				"sprites/enter1.spr"
#define GM_LIFEBAR_SPRITE			"sprites/gm_lifebar.spr"	// the plugin's sprites/mrcalvin/lifebar_def.spr: 100 frames, empty to full

#define GM_SPAWN_TRACE_DISTANCE		220.0	// sma SPAWN_TRACE_DISTANCE: a floor monster within reach
#define GM_SURFACE_TRACE_DISTANCE	4096.0	// wall / ceiling placements
#define GM_TURRET_TRACE_DISTANCE	72.0	// sma TURRET_TRACE_DISTANCE: the floor / ceiling probe of a turret
#define GM_ROTATE_STEP				2.5		// sma PLACEMENT_ROTATE_STEP, degrees per frame with the secondary attack held
#define GM_TELEPORT_SPRITE_BASE_Z	28.0	// sma GM_TELEPORT_SPRITE_BASE_Z (wall effects)
#define GM_HELPER_Z					24.0	// sma GM_TURRET_PREVIEW_HELPER_Z
#define GM_SELECT_MAX				16		// monsters in a Game Master's selection

//=========================================================
// The GM plugin's cvars the game follows (MrCalvin's GM mode plugin; the plugin creates them, the game reads them by
// name; without the plugin the defaults below): syndicate_gmmode_spectator_speed (the roaming speed, 0:
// sv_spectatormaxspeed), _lifebars (0: none), _lifebar_update_interval, _kills_per_level, _intro_title (the guide's
// title)
//=========================================================
static float GmPluginFloat( const char *pszName, float flDefault )
{
	cvar_t *pCvar = CVAR_GET_POINTER( pszName );
	return pCvar ? pCvar->value : flDefault;
}

static const char *GmPluginString( const char *pszName )
{
	cvar_t *pCvar = CVAR_GET_POINTER( pszName );
	return ( pCvar && pCvar->string ) ? pCvar->string : "";
}

static float s_flGmRoamSpeed = GM_ROAM_SPEED;	// GmMode_StartFrame, from syndicate_gmmode_spectator_speed
static float s_flNextPluginCvars;

extern "C" float PM_GmRoamSpeed( void )
{
	return s_flGmRoamSpeed;
}
#define GM_KILLS_PER_LEVEL			2		// sma syndicate_gmmode_kills_per_level default
#define GM_EDICT_MARGIN				64		// free edicts a placement leaves (crash guard)
#define GM_MAX_PLAYERS				32

enum
{
	GM_PLACE_NONE = 0,
	GM_PLACE_MONSTER,
	GM_PLACE_WEAPON,		// a weapon / ammo drop
	GM_PLACE_TELEPORTER,	// a teleporter pad (iTeleRole)
};

// the Game Master's text menu on screen (GmPlayerState::iMenu)
enum
{
	GM_MENU_NONE = 0,
	GM_MENU_QUICK,
	GM_MENU_MISC,
	GM_MENU_LEAVE,
	GM_MENU_ORDERS,
	GM_MENU_HUNT,
	GM_MENU_SPAWN,			// classic: the tabs
	GM_MENU_SPAWN_LIST,		// classic: a tab's entries
	GM_MENU_VARIANTS,		// classic: a monster's loadouts
	GM_MENU_TELEPORTERS,
	GM_MENU_TP_PAIRS,
	GM_MENU_TP_PAIR,
	GM_MENU_TP_REMOVE,
	GM_MENU_TP_CLEAR,
	GM_MENU_PLAYERS,
	GM_MENU_PLAYER,
};

enum
{
	GM_TELE_ENTRANCE = 1,
	GM_TELE_EXIT,
};

#define GM_WEAPON_ITEM_Z			44.0	// sma GM_WEAPON_PREVIEW_ITEM_Z: the spinning item above its base
#define GM_WEAPON_TOUCH_DELAY		0.2		// sma GM_WEAPON_ITEM_TOUCH_DELAY
#define GM_FADE_INTERVAL			0.05	// sma WEAPON_DROP_FADE_INTERVAL
#define GM_FADE_STEP				36		// sma WEAPON_DROP_FADE_DECREMENT
#define GM_TELEPORTER_SKIN			2		// green
#define GM_TELEPORTER_TOUCH_DELAY	0.25	// sma GM_TELEPORTER_TOUCH_DELAY
#define GM_TELEPORTER_COOLDOWN		0.7		// sma GM_TELEPORTER_PLAYER_COOLDOWN
#define GM_TELEPORTER_HEALTH		2500	// production's spawn_overrides.cfg teleporter_health
#define GM_TELEPORTER_HIT_FLASH_TIME	0.12	// sma GM_TELEPORTER_HIT_FLASH_TIME
#define GM_TELEPORTER_HIT_FLASH_AMT	12		// sma GM_TELEPORTER_HIT_FLASH_AMT
#define GM_TELEPORTER_FADE_INTERVAL	0.05	// sma TELEPORTER_FADE_INTERVAL
#define GM_TELEPORTER_FADE_STEP		34		// sma TELEPORTER_FADE_DECREMENT
#define GM_TELEPORTER_OFFSET		28.0	// sma GM_TELEPORTER_OFFSET_Z
#define GM_TELEPORTER_MARKER_Z		48.0	// sma GM_TELEPORTER_MARKER_Z
#define GM_TELEPORTER_EXIT_PUSH		48.0	// sma GM_TELEPORTER_EXIT_PUSH
#define GM_TELEPORTER_TRAIN_LIFT	32.0	// sma GM_TELEPORTER_TRAIN_EXIT_LIFT
#define GM_TELEPORTER_CLEARANCE		16.0	// sma GM_TELEPORTER_PLAYER_CLEARANCE
#define GM_TELEPORTER_SAFE_STEP		8.0		// sma GM_TELEPORTER_SAFE_STEP_Z
#define GM_TELEPORTER_SAFE_STEPS	12		// sma GM_TELEPORTER_SAFE_STEPS
#define GM_TRAIN_ATTACH_DIST		64.0	// sma GM_TRAIN_ATTACH_TRACE_DIST
#define GM_TRAIN_SNAP_DIST			128.0	// sma GM_TRAIN_SNAP_DIST
#define GM_TRAIN_SNAP_HOLD			0.1		// sma GM_TRAIN_SNAP_HOLD
#define GM_MAX_RIDERS				512		// sma GM_MAX_TRAIN_RIDERS

//=========================================================
// A GM-only display entity: the placement preview and the turret direction arrow. Only its owner gets it
// (GmMode_ShouldSend), as the plugin's per-packet EF_NODRAW did, but not even sent to the others.
//=========================================================
class CGmDisplay : public CBaseEntity
{
public:
	void Spawn( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
};

LINK_ENTITY_TO_CLASS( gm_display, CGmDisplay );

void CGmDisplay::Spawn( void )
{
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 90;
	pev->renderfx = kRenderFxGlowShell;
	pev->sequence = 0;	// sma set_preview_animation
	pev->frame = 0;
	pev->framerate = 1.0;
	pev->animtime = gpGlobals->time;
}

//=========================================================
// Per-player state
//=========================================================
struct GmPlayerState
{
	int iState;			// GM_STATE_*
	int iKillProgress;	// player kills by this GM's monsters towards the next level

	// placement (sma g_bPlacementActive, g_iPlacement*)
	int iPlace;			// GM_PLACE_*
	int iEntry;			// catalog entry
	int iVariant;		// loadout, -1 = none
	BOOL bAttackHeld;	// the press that started the placement (or the last placement) must be released first
	float flYawOffset;
	int iOrientation;	// GM_ORIENT_*
	Vector vecOrigin;
	Vector vecAngles;
	Vector vecNormal;	// the surface's normal (toward the Game Master)
	BOOL bHasNormal;
	Vector vecLastNormal;
	int iBody, iSkin;	// -1 = the class decides
	EHANDLE hPreview;
	EHANDLE hHelper;
	EHANDLE hItem;		// a weapon drop's spinning item
	int iTeleRole;		// GM_TELE_* being placed
	EHANDLE hEntrance;	// an entrance waiting for its exit (sma g_iGMTeleporterEntrance)

	// NPC selection and orders
	EHANDLE hSelected[GM_SELECT_MAX];
	int iSelected;
	BOOL bSelecting;		// the box is being drawn
	BOOL bOrderWaitRelease;	// a placement's (or the window's) held button first
	float flNextOrder;
	float flNextFailNotice;
	float flTeleportCooldown;	// this player's next teleporter use (sma g_fTeleporterPlayerCooldown)

	// the design's UI (docs/GM_MODE.md): the last pick (quick menu 1), the text menu on screen
	int iLastEntry;			// the last monster picked (-1 none) and its loadout
	int iLastVariant;
	int iMenu;				// GM_MENU_* on screen (m_iTextMenu TEXTMENU_GM)
	int iMenuPage;
	int iMenuArg;			// the menu's subject (tab, pair, player index)
	int iMenuRows[8];		// the rows of a list menu: catalog picks, pairs or player indices
	int iMenuRowCount;
	int iHuntTarget;		// the player "3. Hunt player" hunted last (index, 0 none)
	int iFollowTarget;		// the player quick menu 5 follows (index, 0 none)
	int iGotoLast;			// the player quick menu 4 went to last

	// the plugin's numbers for the status strip and the spawn window (gm_status), and what the client has
	BOOL bEconomy;			// a plugin sent gm_status for this Game Master
	int iPoints;
	int iMaxPoints;			// the energy bar's maximum (0: no bar, the points as a number)
	int iRefill;			// points of the next refill
	float flRefillInterval;	// seconds between refills
	float flRefillEnd;		// the next refill (gpGlobals->time), 0 none
	int iKillPop;			// a kill reward just paid: "+N" over the bar (sent once)
	int iPipTarget;			// the players window's View: the player its picture follows (0 none)
	BOOL bMapWatch;			// the players window is open: its map gets this Game Master's monsters (GmMonsters)
	int iTier;				// the highest open tier (1-3)
	float flTierEnd;		// the next tier opens (gpGlobals->time), 0 none
	float flSpawnEnd;		// the next spawn is allowed (gpGlobals->time), 0 none
	float flNextHud;		// the next GmStatus / GmPlayers check
	int iSentHash;			// what the last GmStatus carried
};

static GmPlayerState s_Gm[GM_MAX_PLAYERS + 1];

// entity indices of the GM display entities (for the per-client check in AddToFullPack)
#define GM_MAX_PRIVATE	( GM_MAX_PLAYERS * 3 )
static int s_iPrivate[GM_MAX_PRIVATE];
static int s_iPrivateOwner[GM_MAX_PRIVATE];
static int s_iPrivateCount;

static int s_iSpawnSprite;
static int s_iBeamSprite;
static BOOL s_bCatalogPrecached;	// GmMode_Precache ran on this co-op map

// The status line a plugin set for each pick of each Game Master (gm_entry_text): the reason given when the plugin
// refuses that pick. The price line is only the client's.
struct GmStatusText
{
	char szText[GM_TEXT_LEN];
	float flEnd;	// countdown end (gpGlobals->time), 0 = none
};
static GmStatusText s_GmStatus[GM_MAX_PLAYERS + 1][GM_CATALOG_COUNT][GM_PICK_MAX_VARIANT + 1];
// The other card texts (price, tier, kill pay, cooldown): the classic text menus show them (the client has its own copy)
static char s_GmCardText[GM_MAX_PLAYERS + 1][GM_CATALOG_COUNT][GM_PICK_MAX_VARIANT + 1][GM_TEXT_SLOTS][GM_TEXT_LEN];

static const char *GmModelForClass( const char *pszClass )
{
	return GmCatalog_ModelForClass( pszClass );
}

static BOOL IsTurretClass( const char *pszClass )
{
	return FStrEq( pszClass, "monster_turret" ) || FStrEq( pszClass, "monster_miniturret" ) || FStrEq( pszClass, "monster_sentry" );
}

// sma supports_wall_surface_orientation: the two turrets that also go on walls
static BOOL IsWallTurretClass( const char *pszClass )
{
	return FStrEq( pszClass, "monster_turret" ) || FStrEq( pszClass, "monster_miniturret" );
}

static float NormalizeAngle( float flAngle )
{
	while ( flAngle > 180 )
		flAngle -= 360;
	while ( flAngle < -180 )
		flAngle += 360;
	return flAngle;
}

//=========================================================
// Messages
//=========================================================
static int GmTeleporterInfo( CBasePlayer *pPlayer );

static void GmSendState( CBasePlayer *pPlayer, int iEvent, const char *pszLabel = "", int iEntry = 255 )
{
	if ( !gmsgGmMode )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgGmMode, NULL, pPlayer->pev );
		WRITE_BYTE( s_Gm[pPlayer->entindex()].iState );
		WRITE_BYTE( iEvent );
		WRITE_BYTE( iEntry );
		WRITE_BYTE( GmTeleporterInfo( pPlayer ) );
		WRITE_STRING( pszLabel );
	MESSAGE_END();
}

// "GmNotice": a short line under the player's crosshair for GM_NOTICE_TIME seconds (no GM line goes to the chat)
void GmMode_Notice( CBasePlayer *pPlayer, const char *pszText )
{
	if ( !pPlayer || !gmsgGmNotice || FBitSet( pPlayer->pev->flags, FL_FAKECLIENT ) )
		return;
	char szBuf[GM_NOTICE_LEN];
	_snprintf( szBuf, sizeof( szBuf ), "%s", pszText );
	szBuf[sizeof( szBuf ) - 1] = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsgGmNotice, NULL, pPlayer->pev );
		WRITE_STRING( szBuf );
	MESSAGE_END();
}

static void GmPrint( CBasePlayer *pPlayer, const char *pszText )
{
	GmMode_Notice( pPlayer, pszText );
}

static void GmRemoveTeleporters( CBasePlayer *pPlayer );
static void GmResetRiders( void );
static void GmResetLifebars( void );
static void GmClearSelection( CBasePlayer *pPlayer );
static void GmResetOrders( void );
static void GmCancelOrders( int iOwner );
static void GmUpdateOrders( void );
static void GmUpdateLifebars( void );
static int GmToolEntry( int iTool );
static void GmPlaceWeapon( CBasePlayer *pPlayer );
static void GmPlaceTeleporter( CBasePlayer *pPlayer );
static void GmQuickMenu( CBasePlayer *pPlayer );
static void GmBackToQuickMenu( CBasePlayer *pPlayer );
static void GmOrdersMenu( CBasePlayer *pPlayer );
static CBaseMonster *GmOrderable( CBasePlayer *pPlayer, CBaseEntity *pEntity );
static BOOL GmSelectable( CBasePlayer *pPlayer, CBaseEntity *pEntity );
static void GmDestroySelected( CBasePlayer *pPlayer );
static void GmPick( CBasePlayer *pPlayer, int iEntry, int iVariant );
static void GmShowMenu( CBasePlayer *pPlayer, int iMenu, int iKeys, const char *pszText );
static void GmUpdateHud( CBasePlayer *pPlayer );
static void GmFollowThink( CBasePlayer *pPlayer );
static BOOL GmFollowing( CBasePlayer *pPlayer );
static void GmFollow( CBasePlayer *pPlayer, int iTarget, int iMode );
static void GmStopFollowing( CBasePlayer *pPlayer );
static void GmSpawnNear( CBasePlayer *pPlayer, int iTarget );
static void GmSelectScreen( CBasePlayer *pPlayer, float x0, float y0, float x1, float y1, float flTanX, float flTanY,
	float flMaxDist );
static void GmTextMenuCommand( CBasePlayer *pPlayer, const char *pszWhat, int iArg );
static void GmTpAction( CBasePlayer *pPlayer, const char *pszAction, int iPair, int iWhich );
struct GmOrder;
class CBaseMonster;
static BOOL GmPersistentUpdate( GmOrder &order, CBasePlayer *pOwner, CBaseMonster *pMonster );

//=========================================================
// Display entities
//=========================================================
static void GmPrivateAdd( CBaseEntity *pEntity, CBasePlayer *pOwner )
{
	if ( s_iPrivateCount >= GM_MAX_PRIVATE )
		return;
	s_iPrivate[s_iPrivateCount] = pEntity->entindex();
	s_iPrivateOwner[s_iPrivateCount] = pOwner->entindex();
	s_iPrivateCount++;
}

static void GmRemoveDisplay( EHANDLE &h )
{
	CBaseEntity *pEntity = h;

	h = NULL;
	if ( !pEntity )
		return;

	int iIndex = pEntity->entindex();
	for ( int i = 0; i < s_iPrivateCount; i++ )
	{
		if ( s_iPrivate[i] == iIndex )
		{
			s_iPrivateCount--;
			s_iPrivate[i] = s_iPrivate[s_iPrivateCount];
			s_iPrivateOwner[i] = s_iPrivateOwner[s_iPrivateCount];
			break;
		}
	}
	UTIL_Remove( pEntity );
}

static CBaseEntity *GmCreateDisplay( CBasePlayer *pOwner, const char *pszModel, int iBody, float flAmount )
{
	if ( !s_bCatalogPrecached )
		return NULL;	// never a model the map didn't precache

	CBaseEntity *pEntity = CBaseEntity::Create( (char *)"gm_display", pOwner->pev->origin, g_vecZero, NULL );
	if ( !pEntity )
		return NULL;

	SET_MODEL( pEntity->edict(), pszModel );
	pEntity->pev->body = iBody;
	pEntity->pev->renderamt = flAmount;
	GmPrivateAdd( pEntity, pOwner );
	return pEntity;
}

static BOOL GmLifebarShouldSend( int iEntity, edict_t *pHost );

BOOL GmMode_ShouldSend( int iEntity, edict_t *pHost )
{
	if ( !GmLifebarShouldSend( iEntity, pHost ) )
		return FALSE;

	if ( !s_iPrivateCount )
		return TRUE;

	for ( int i = 0; i < s_iPrivateCount; i++ )
	{
		if ( s_iPrivate[i] == iEntity )
			return s_iPrivateOwner[i] == ENTINDEX( pHost );
	}
	return TRUE;
}

//=========================================================
// Setup
//=========================================================
// A co-op map. mp_gmmode only gates a player's own "gmon" (CCoop::GameMasterOn): an admin's gm_toggle (the plugin's
// amx_gm, the dice reward) makes a Game Master on any co-op map, as the plugin did without a server cvar.
BOOL GmMode_Allowed( void )
{
	return m_Mode == MODE_COOP;
}

static void GmMode_ToggleCommand( void );
static void GmMode_EntryTextCommand( void );
static void GmMode_StatusCommand( void );

void GmMode_Init( void )
{
	g_engfuncs.pfnAddServerCommand( (char *)"gm_toggle", GmMode_ToggleCommand );
	g_engfuncs.pfnAddServerCommand( (char *)"gm_status", GmMode_StatusCommand );
	g_engfuncs.pfnAddServerCommand( (char *)"gm_entry_text", GmMode_EntryTextCommand );
}

// Every class of the catalog (with the loadout classes) and the display assets; only where GM mode can be used. The
// plugin precached them on every map it ran on.
void GmMode_Precache( void )
{
	int i;

	memset( s_Gm, 0, sizeof( s_Gm ) );
	s_iPrivateCount = 0;
	GmResetRiders();
	GmResetLifebars();
	GmResetOrders();

	s_bCatalogPrecached = FALSE;

	if ( m_Mode != MODE_COOP )
		return;

	// Everything GM mode can show or spawn, on every co-op map whatever mp_gmmode is, so it can be turned on at any
	// time (the plugin precached its catalog on every map it ran on; SBO's own GM mode precached none of its monsters
	// and could only spawn the ones the map had). The Game Master's own assets first.
	PRECACHE_MODEL( (char *)GM_DISPLAY_MODEL );
	PRECACHE_MODEL( (char *)GM_LIFEBAR_SPRITE );
	s_iBeamSprite = PRECACHE_MODEL( (char *)"sprites/laserbeam.spr" );	// selection box, route lines, goal ring
	PRECACHE_SOUND( (char *)GM_SPAWN_SOUND );
	s_iSpawnSprite = PRECACHE_MODEL( (char *)GM_SPAWN_SPRITE );

	for ( i = 0; i < GM_CATALOG_COUNT; i++ )
	{
		const GmCatalogEntry &entry = g_GmCatalog[i];
		if ( entry.iKind != GM_KIND_MONSTER )
			continue;

		UTIL_PrecacheOther( entry.pszClass );

		int iCount;
		const GmVariant *pVariants = GmCatalog_Variants( entry.iVariants, iCount );
		for ( int j = 0; j < iCount; j++ )
			UTIL_PrecacheOther( pVariants[j].pszClass );
	}

	for ( i = 0; i < (int)ARRAYSIZE( g_GmModels ); i++ )
		PRECACHE_MODEL( (char *)g_GmModels[i].pszModel );

	// the weapon drops' weapons (W_Precache has most of them already)
	for ( i = 0; i < GM_CATALOG_COUNT; i++ )
	{
		if ( g_GmCatalog[i].iKind == GM_KIND_WEAPON )
			UTIL_PrecacheOther( g_GmCatalog[i].pszClass );
	}

	s_bCatalogPrecached = TRUE;
}

// A co-op map: the catalog is precached (GmMode_Precache). The spawn paths check it again, as a crash guard only (a
// model that isn't precached is a Host_Error for everybody).
BOOL GmMode_Ready( void )
{
	return GmMode_Allowed() && s_bCatalogPrecached;
}

//=========================================================
// State
//=========================================================
int GmMode_State( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->m_bGameMaster )
		return GM_STATE_OFF;
	return s_Gm[pPlayer->entindex()].iState;
}

static void GmEndPlacement( CBasePlayer *pPlayer, BOOL bNotify )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	BOOL bWasPlacing = gm.iPlace != GM_PLACE_NONE;

	GmRemoveDisplay( gm.hPreview );
	GmRemoveDisplay( gm.hHelper );
	GmRemoveDisplay( gm.hItem );
	gm.iPlace = GM_PLACE_NONE;
	gm.bOrderWaitRelease = TRUE;	// the button that placed doesn't select (sma g_GMOrderWaitRelease)
	gm.iEntry = -1;
	gm.iVariant = -1;

	if ( bWasPlacing && bNotify )
		GmSendState( pPlayer, GM_EVENT_PLACE_END );
}

// sma force_roaming_mode: the Game Master always roams
static void GmForceRoaming( CBasePlayer *pPlayer )
{
	pPlayer->m_hObserverTarget = NULL;
	pPlayer->m_bWasFollowing = FALSE;
	pPlayer->pev->iuser1 = OBS_ROAMING;
	pPlayer->pev->iuser2 = 0;
	pPlayer->pev->iuser3 = 0;
}

void GmMode_Start( CBasePlayer *pPlayer )
{
	if ( !GmMode_Ready() )
		return;	// CCoop::GameMasterOn checks first

	int iIndex = pPlayer->entindex();
	GmPlayerState &gm = s_Gm[iIndex];

	memset( &gm, 0, sizeof( gm ) );
	memset( s_GmStatus[iIndex], 0, sizeof( s_GmStatus[iIndex] ) );
	memset( s_GmCardText[iIndex], 0, sizeof( s_GmCardText[iIndex] ) );
	gm.iEntry = -1;
	gm.iVariant = -1;
	gm.iState = GM_STATE_INTRO;
	gm.iLastEntry = -1;
	gm.iLastVariant = -1;

	// sma complete_enter_gm_mode: an observer where the player is (the plugin waited for the intro to close; here
	// the player is out of the game at once and only waits frozen, unseen behind the black screen)
	if ( !pPlayer->IsObserver() )
		pPlayer->StartObserver( pPlayer->pev->origin + pPlayer->pev->view_ofs, pPlayer->pev->v_angle );

	GmForceRoaming( pPlayer );
	pPlayer->pev->iuser4 |= GM_IUSER4_BIT;	// GM_ROAM_SPEED (pm_shared)
	pPlayer->pev->flags |= FL_FROZEN;		// until the intro closes (sma enable_gm_intro_protection)

	ShowVGUI( pPlayer, 19 );	// the GM intro window (VGUI2 CSboGmMenu)
	GmSendState( pPlayer, GM_EVENT_NONE, GmPluginString( "syndicate_gmmode_intro_title" ) );	// the guide's title ("": the game's)
}

// "gm": the intro window was closed (its Return button, or Esc)
static void GmIntroClosed( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];

	if ( gm.iState == GM_STATE_INTRO )
	{
		gm.iState = GM_STATE_ACTIVE;
		pPlayer->pev->flags &= ~FL_FROZEN;
	}

	// sma cleanup_intro_state: the main menu follows the intro (and the help)
	if ( gm.iState == GM_STATE_ACTIVE && gm.iPlace == GM_PLACE_NONE )
		GmSendState( pPlayer, GM_EVENT_OPEN_MENU );
}

void GmMode_Stop( CBasePlayer *pPlayer )
{
	int iIndex = pPlayer->entindex();
	GmPlayerState &gm = s_Gm[iIndex];

	GmEndPlacement( pPlayer, FALSE );
	GmClearSelection( pPlayer );	// sma: a placement suspends the NPC controls and clears the selection
	GmRemoveTeleporters( pPlayer );	// sma exit_gm_mode: gm_remove_owner_teleporters (the weapon drops stay)
	GmCancelOrders( iIndex );		// sma: leaving / disconnecting cancels the orders and the selection
	if ( !pPlayer->has_disconnected )
		GmShowMenu( pPlayer, GM_MENU_NONE, 0, "" );	// a GM text menu goes with GM mode
	gm.iMenu = GM_MENU_NONE;
	gm.bEconomy = FALSE;
	gm.iSentHash = 0;
	gm.iState = GM_STATE_OFF;
	gm.iKillProgress = 0;
	memset( s_GmStatus[iIndex], 0, sizeof( s_GmStatus[iIndex] ) );	// the client drops its texts on GM_STATE_OFF
	memset( s_GmCardText[iIndex], 0, sizeof( s_GmCardText[iIndex] ) );

	pPlayer->pev->iuser4 &= ~GM_IUSER4_BIT;
	pPlayer->pev->flags &= ~FL_FROZEN;

	if ( !pPlayer->has_disconnected )
		GmSendState( pPlayer, GM_EVENT_NONE );
}

void GmMode_StopAll( void )
{
	if ( m_Mode != MODE_COOP )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->m_bGameMaster && !pPlayer->has_disconnected )
			( (CCoop *)g_pGameRules )->GameMasterOff( pPlayer, TRUE );
	}
}

//=========================================================
// Placement (sma compute_spawn_origin, determine_*_orientation, update_preview_position / _angles)
//=========================================================
static Vector GmEyePosition( CBasePlayer *pPlayer )
{
	return pPlayer->pev->origin + pPlayer->pev->view_ofs;
}

// The normal pointing toward the Game Master's eye (sma gm_align_normal_to_view)
static void GmAlignNormalToView( CBasePlayer *pPlayer, const Vector &vecHit, Vector &vecNormal )
{
	if ( DotProduct( vecNormal, GmEyePosition( pPlayer ) - vecHit ) < 0 )
		vecNormal = -vecNormal;
}

static const char *GmPlacementClass( const GmPlayerState &gm )
{
	if ( gm.iEntry < 0 || gm.iEntry >= GM_CATALOG_COUNT )
		return "";

	const GmCatalogEntry &entry = g_GmCatalog[gm.iEntry];
	if ( gm.iVariant >= 0 )
	{
		int iCount;
		const GmVariant *pVariants = GmCatalog_Variants( entry.iVariants, iCount );
		if ( pVariants && gm.iVariant < iCount )
			return pVariants[gm.iVariant].pszClass;
	}
	return entry.pszClass;
}

// wall turrets, weapon drops and teleporters go on any surface (sma should_use_surface_trace_for_placement)
static BOOL GmUsesSurfaceTrace( const GmPlayerState &gm )
{
	if ( gm.iPlace == GM_PLACE_WEAPON || gm.iPlace == GM_PLACE_TELEPORTER )
		return TRUE;
	return gm.iPlace == GM_PLACE_MONSTER && IsWallTurretClass( GmPlacementClass( gm ) );
}

static BOOL GmComputeSpawnOrigin( CBasePlayer *pPlayer, Vector &vecOrigin )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	BOOL bSurface = GmUsesSurfaceTrace( gm );
	TraceResult tr;

	gm.bHasNormal = FALSE;
	gm.vecLastNormal = g_vecZero;

	Vector vecStart = GmEyePosition( pPlayer );
	UTIL_MakeVectors( pPlayer->pev->v_angle );
	Vector vecForward = gpGlobals->v_forward;
	Vector vecEnd = vecStart + vecForward * ( bSurface ? GM_SURFACE_TRACE_DISTANCE : GM_SPAWN_TRACE_DISTANCE );

	UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, pPlayer->edict(), &tr );
	if ( tr.flFraction < 1.0 )
	{
		vecOrigin = tr.vecEndPos;
		gm.vecLastNormal = tr.vecPlaneNormal;
		GmAlignNormalToView( pPlayer, vecOrigin, gm.vecLastNormal );
		gm.bHasNormal = TRUE;
		return TRUE;
	}

	// wall placements: exactly on the view ray (sma: the aim fallback drifts at long distances)
	if ( bSurface )
	{
		vecOrigin = vecEnd;
		return TRUE;
	}

	// fakemeta_util fm_get_aim_origin: where the crosshair points, then the plane normal of that point again
	UTIL_TraceLine( vecStart, vecStart + vecForward * 9999, dont_ignore_monsters, pPlayer->edict(), &tr );
	vecOrigin = tr.vecEndPos;
	if ( vecOrigin == g_vecZero )
	{
		vecOrigin = vecEnd;
		return TRUE;
	}

	UTIL_TraceLine( vecStart, vecOrigin, ignore_monsters, pPlayer->edict(), &tr );
	if ( tr.flFraction < 1.0 )
	{
		vecOrigin = tr.vecEndPos;
		gm.vecLastNormal = tr.vecPlaneNormal;
		GmAlignNormalToView( pPlayer, vecOrigin, gm.vecLastNormal );
		gm.bHasNormal = TRUE;
	}
	return TRUE;
}

// sma apply_class_spawn_adjustments
static void GmClassAdjust( const char *pszClass, Vector &vecOrigin )
{
	if ( FStrEq( pszClass, "monster_apache" ) || FStrEq( pszClass, "monster_osprey" ) )
		vecOrigin.z += 200;
	else if ( FStrEq( pszClass, "monster_ichthyosaur" ) )
		vecOrigin.z += 96;	// headroom in water
}

// sma determine_surface_orientation
static int GmSurfaceOrientation( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];

	if ( gm.bHasNormal )
	{
		if ( gm.vecLastNormal.z <= -0.5 )
			return GM_ORIENT_CEILING;
		if ( gm.vecLastNormal.z >= 0.5 )
			return GM_ORIENT_FLOOR;
		return GM_ORIENT_WALL;
	}

	UTIL_MakeVectors( pPlayer->pev->v_angle );
	if ( gpGlobals->v_forward.z >= 0.25 )
		return GM_ORIENT_CEILING;
	if ( gpGlobals->v_forward.z <= -0.25 )
		return GM_ORIENT_FLOOR;
	return GM_ORIENT_WALL;
}

// sma determine_turret_orientation: the normal, else whichever of floor and ceiling is nearer within 72 units
static int GmTurretOrientation( CBasePlayer *pPlayer, const Vector &vecOrigin )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	TraceResult tr;

	if ( gm.bHasNormal )
	{
		if ( gm.vecLastNormal.z <= -0.5 )
			return GM_ORIENT_CEILING;
		if ( gm.vecLastNormal.z >= 0.5 )
			return GM_ORIENT_FLOOR;
	}

	float flDown = GM_TURRET_TRACE_DISTANCE + 1, flUp = GM_TURRET_TRACE_DISTANCE + 1;
	BOOL bDown = FALSE, bUp = FALSE;

	UTIL_TraceLine( vecOrigin, vecOrigin - Vector( 0, 0, GM_TURRET_TRACE_DISTANCE ), ignore_monsters, NULL, &tr );
	if ( tr.flFraction < 1.0 )
	{
		bDown = TRUE;
		flDown = tr.flFraction * GM_TURRET_TRACE_DISTANCE;
	}
	UTIL_TraceLine( vecOrigin, vecOrigin + Vector( 0, 0, GM_TURRET_TRACE_DISTANCE ), ignore_monsters, NULL, &tr );
	if ( tr.flFraction < 1.0 )
	{
		bUp = TRUE;
		flUp = tr.flFraction * GM_TURRET_TRACE_DISTANCE;
	}

	if ( bUp && ( !bDown || flUp <= flDown ) )
		return GM_ORIENT_CEILING;
	return GM_ORIENT_FLOOR;
}

// sma gm_rotate_vector_about_axis (Rodrigues)
static Vector GmRotateAboutAxis( const Vector &vec, const Vector &vecAxis, float flDegrees )
{
	float flLen = vecAxis.Length();
	if ( flLen <= 0.0001 )
		return vec;

	Vector n = vecAxis / flLen;
	float flCos = cos( flDegrees * M_PI / 180.0 );
	float flSin = sin( flDegrees * M_PI / 180.0 );
	float flDot = DotProduct( n, vec );
	Vector vecCross = CrossProduct( n, vec );

	return vec * flCos + vecCross * flSin + n * ( flDot * ( 1 - flCos ) );
}

// sma gm_angles_from_forward_up
static Vector GmAnglesFromForwardUp( const Vector &vecForward, const Vector &vecUp )
{
	Vector vecAngles = UTIL_VecToAngles( vecForward );

	UTIL_MakeVectors( vecAngles );
	// the plugin's sign: GoldSrc's right vector inverted for a stable roll direction
	float flSinRoll = -DotProduct( vecUp, gpGlobals->v_right );
	float flCosRoll = DotProduct( vecUp, gpGlobals->v_up );
	vecAngles.z = NormalizeAngle( atan2( flSinRoll, flCosRoll ) * 180 / M_PI );
	return vecAngles;
}

// a point off a placed base along its surface's normal (floor: up, ceiling: down, wall: the normal)
static Vector GmOffAlongSurface( const Vector &vecOrigin, int iOrientation, const Vector &vecNormal, float flDist )
{
	if ( iOrientation == GM_ORIENT_CEILING )
		return vecOrigin - Vector( 0, 0, flDist );
	if ( iOrientation == GM_ORIENT_WALL )
		return vecOrigin + vecNormal * flDist;
	return vecOrigin + Vector( 0, 0, flDist );
}

static Vector GmItemOrigin( const Vector &vecOrigin, int iOrientation, const Vector &vecNormal )
{
	return GmOffAlongSurface( vecOrigin, iOrientation, vecNormal, GM_WEAPON_ITEM_Z );
}

static void GmUpdatePreviewAngles( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	Vector vecAngles;

	vecAngles.y = NormalizeAngle( pPlayer->pev->v_angle.y + gm.flYawOffset );
	vecAngles.z = 0;

	switch ( gm.iOrientation )
	{
	case GM_ORIENT_CEILING:
		vecAngles.x = 180;
		break;
	case GM_ORIENT_WALL:
	{
		// the plugin's known-good wall snap, then the rotation about the wall's normal
		float flNormalYaw = atan2( gm.vecNormal.y, gm.vecNormal.x ) * 180 / M_PI;
		Vector vecBase( 90, NormalizeAngle( flNormalYaw + 180 ), 0 );
		UTIL_MakeVectors( vecBase );
		Vector vecForward = gpGlobals->v_forward;
		Vector vecUp = gpGlobals->v_up;
		Vector vecRotForward = GmRotateAboutAxis( vecForward, vecUp, gm.flYawOffset );
		vecAngles = GmAnglesFromForwardUp( vecRotForward, vecUp );
		break;
	}
	default:
		vecAngles.x = 0;
		break;
	}

	gm.vecAngles = vecAngles;

	CBaseEntity *pPreview = gm.hPreview;
	if ( pPreview )
		pPreview->pev->angles = vecAngles;

	// sma gm_refresh_preview_helper: the arrow shows a turret's facing, turned round on the ceiling (not the sentry)
	CBaseEntity *pHelper = gm.hHelper;
	if ( pHelper )
	{
		Vector vecHelper = gm.vecOrigin;
		if ( gm.iOrientation == GM_ORIENT_CEILING )
			vecHelper.z -= GM_HELPER_Z;
		else if ( gm.iOrientation == GM_ORIENT_WALL )
			vecHelper = vecHelper + gm.vecNormal * GM_HELPER_Z;
		else
			vecHelper.z += GM_HELPER_Z;
		UTIL_SetOrigin( pHelper->pev, vecHelper );

		Vector vecHelperAngles = vecAngles;
		if ( gm.iPlace == GM_PLACE_MONSTER && gm.iOrientation == GM_ORIENT_CEILING
			&& !FStrEq( GmPlacementClass( gm ), "monster_sentry" ) )
			vecHelperAngles.y = NormalizeAngle( vecHelperAngles.y + 180 );
		pHelper->pev->angles = vecHelperAngles;
	}

	// sma gm_refresh_weapon_preview_entity: the weapon floats 44 units off its base
	CBaseEntity *pItem = gm.hItem;
	if ( pItem )
	{
		UTIL_SetOrigin( pItem->pev, GmItemOrigin( gm.vecOrigin, gm.iOrientation, gm.vecNormal ) );
		pItem->pev->angles = vecAngles;
	}
}

static void GmUpdatePreview( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	Vector vecOrigin;

	if ( gm.iPlace != GM_PLACE_MONSTER && gm.iPlace != GM_PLACE_WEAPON && gm.iPlace != GM_PLACE_TELEPORTER )
		return;

	if ( !GmComputeSpawnOrigin( pPlayer, vecOrigin ) )
		return;

	gm.vecNormal = gm.bHasNormal ? gm.vecLastNormal : Vector( 0, 0, 1 );

	if ( gm.iPlace == GM_PLACE_MONSTER )
	{
		const char *pszClass = GmPlacementClass( gm );
		GmClassAdjust( pszClass, vecOrigin );

		if ( IsTurretClass( pszClass ) )
		{
			if ( IsWallTurretClass( pszClass ) )
				gm.iOrientation = GmSurfaceOrientation( pPlayer );
			else
				gm.iOrientation = GmTurretOrientation( pPlayer, vecOrigin );
		}
	}
	else
		gm.iOrientation = GmSurfaceOrientation( pPlayer );	// sma update_preview_position: weapons, teleporters

	gm.vecOrigin = vecOrigin;

	CBaseEntity *pPreview = gm.hPreview;
	if ( pPreview )
		UTIL_SetOrigin( pPreview->pev, vecOrigin );

	GmUpdatePreviewAngles( pPlayer );
}

// sma select_monster_appearance: the loadout's head and weapon bodies on the Feds, a random head / skin on others
static void GmSelectAppearance( const char *pszClass, int iWeapons, int &iBody, int &iSkin )
{
	iBody = -1;
	iSkin = -1;

	if ( FStrEq( pszClass, "monster_grunt_hunter" ) || FStrEq( pszClass, "dark_grunt" ) )
	{
		GmCatalog_LoadoutLook( pszClass, iWeapons, iBody, iSkin );	// the spawn window's models show the same
	}
	else if ( FStrEq( pszClass, "monster_scientist" ) )
	{
		iBody = RANDOM_LONG( 0, 3 );
		iSkin = RANDOM_LONG( 0, 1 );
	}
	else if ( FStrEq( pszClass, "monster_dark" ) )
	{
		iBody = RANDOM_LONG( 0, 3 );
	}
	else if ( FStrEq( pszClass, "monster_zombie_hunter" ) || FStrEq( pszClass, "monster_zombie" ) )
	{
		iBody = RANDOM_LONG( 0, 1 );
	}
}

static int GmPlacementWeapons( const GmPlayerState &gm )
{
	if ( gm.iEntry < 0 || gm.iVariant < 0 )
		return -1;

	int iCount;
	const GmVariant *pVariants = GmCatalog_Variants( g_GmCatalog[gm.iEntry].iVariants, iCount );
	return ( pVariants && gm.iVariant < iCount ) ? pVariants[gm.iVariant].iWeapons : -1;
}

static void GmApplyPreviewAppearance( GmPlayerState &gm )
{
	CBaseEntity *pPreview = gm.hPreview;
	if ( !pPreview )
		return;
	pPreview->pev->body = ( gm.iBody != -1 ) ? gm.iBody : 0;
	pPreview->pev->skin = ( gm.iSkin != -1 ) ? gm.iSkin : 0;
}

static void GmStartMonsterPlacement( CBasePlayer *pPlayer, int iEntry, int iVariant )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];

	GmEndPlacement( pPlayer, FALSE );
	GmClearSelection( pPlayer );	// sma: a placement suspends the NPC controls and clears the selection

	gm.iPlace = GM_PLACE_MONSTER;
	gm.iEntry = iEntry;
	gm.iVariant = iVariant;
	gm.flYawOffset = 0;
	gm.iOrientation = g_GmCatalog[iEntry].iOrientation;
	gm.bAttackHeld = TRUE;	// the click that picked it in the window doesn't place it

	const char *pszClass = GmPlacementClass( gm );
	const char *pszModel = GmModelForClass( pszClass );
	if ( pszModel )
		gm.hPreview = GmCreateDisplay( pPlayer, pszModel, 0, 90 );
	if ( IsTurretClass( pszClass ) )
		gm.hHelper = GmCreateDisplay( pPlayer, GM_DISPLAY_MODEL, GM_DISPLAY_BODY_ARROW, 180 );

	GmSelectAppearance( pszClass, GmPlacementWeapons( gm ), gm.iBody, gm.iSkin );
	GmApplyPreviewAppearance( gm );
	GmUpdatePreview( pPlayer );

	char szLabel[96];
	int iCount;
	const GmVariant *pVariants = GmCatalog_Variants( g_GmCatalog[iEntry].iVariants, iCount );
	if ( pVariants && iVariant >= 0 && iVariant < iCount )
		_snprintf( szLabel, sizeof( szLabel ), "%s (%s)", g_GmCatalog[iEntry].pszName, pVariants[iVariant].pszName );
	else
		_snprintf( szLabel, sizeof( szLabel ), "%s", g_GmCatalog[iEntry].pszName );
	szLabel[sizeof( szLabel ) - 1] = 0;
	GmSendState( pPlayer, GM_EVENT_PLACE, szLabel, iEntry );
}

//=========================================================
// Plugin hooks (docs/GM_MODE.md "Plugin hooks"): CBasePlayer::GmCanPlace before a pick and each placing click,
// CBasePlayer::GmPlaced after a placement. Without a plugin they allow everything and do nothing.
//=========================================================
static int GmTeleporterPick( int iRole )
{
	int iEntry = GmToolEntry( GM_TOOL_TELEPORTERS );
	return GM_PICK( ( iEntry >= 0 ) ? iEntry : 255, ( iRole == GM_TELE_EXIT ) ? GM_TP_ROLE_EXIT : GM_TP_ROLE_ENTRANCE );
}

// A status line with its countdown resolved: GM_TEXT_TOKEN -> the whole seconds left; "" when none or run out
static void GmResolveStatus( const GmStatusText &status, char *pszOut, int iOutLen )
{
	pszOut[0] = 0;
	if ( !status.szText[0] || ( status.flEnd > 0 && gpGlobals->time >= status.flEnd ) )
		return;

	int iLeft = ( status.flEnd > 0 ) ? (int)ceil( status.flEnd - gpGlobals->time ) : 0;
	const char *pszToken = strstr( status.szText, GM_TEXT_TOKEN );
	if ( !pszToken )
	{
		_snprintf( pszOut, iOutLen, "%s", status.szText );
	}
	else
	{
		_snprintf( pszOut, iOutLen, "%.*s%d%s", (int)( pszToken - status.szText ), status.szText, iLeft,
			pszToken + strlen( GM_TEXT_TOKEN ) );
	}
	pszOut[iOutLen - 1] = 0;
}

// Asks the plugins (CBasePlayer::GmCanPlace). Refused: "[ GM ] <name>: <reason>", the reason being the pick's status
// line (else the entry's) that the plugin set with gm_entry_text.
static BOOL GmAskPlace( CBasePlayer *pPlayer, int iPick )
{
	if ( pPlayer->GmCanPlace( iPick ) )
		return TRUE;

	int iEntry = GM_PICK_ENTRY( iPick );
	int iVariant = GM_PICK_VARIANT( iPick );
	char szReason[GM_TEXT_LEN + 16];
	szReason[0] = 0;
	if ( iEntry >= 0 && iEntry < GM_CATALOG_COUNT )
	{
		const GmStatusText ( &status )[GM_PICK_MAX_VARIANT + 1] = s_GmStatus[pPlayer->entindex()][iEntry];
		if ( iVariant >= 0 && iVariant < GM_PICK_MAX_VARIANT )
			GmResolveStatus( status[iVariant + 1], szReason, sizeof( szReason ) );
		if ( !szReason[0] )
			GmResolveStatus( status[0], szReason, sizeof( szReason ) );
	}

	const char *pszName = "Teleporter";
	if ( iEntry >= 0 && iEntry < GM_CATALOG_COUNT && g_GmCatalog[iEntry].iKind != GM_KIND_TOOL )
		pszName = g_GmCatalog[iEntry].pszName;

	char szText[128];
	_snprintf( szText, sizeof( szText ), "%s: %s", pszName, szReason[0] ? szReason : "not available" );
	szText[sizeof( szText ) - 1] = 0;
	GmPrint( pPlayer, szText );
	return FALSE;
}

//=========================================================
// Spawning (sma spawn_monster_for_player)
//=========================================================
extern void Turret_GmActivate( CBaseEntity *pEntity );

static void GmSpawnEffect( const Vector &vecOrigin )
{
	// sma create_spawn_sprite: enter1.spr played once (a temporary entity instead of an env_sprite removed after 1 s)
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_SPRITE );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_SHORT( s_iSpawnSprite );
		WRITE_BYTE( 7 );	// scale 0.7
		WRITE_BYTE( 255 );
	MESSAGE_END();
}

static BOOL GmEdictsLeft( void )
{
	int iUsed = 0;
	for ( int i = 1; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pEdict = INDEXENT( i );
		if ( pEdict && !pEdict->free )
			iUsed++;
	}
	return gpGlobals->maxEntities - iUsed > GM_EDICT_MARGIN;
}

static void GmKeyValue( edict_t *pent, const char *pszKey, int iValue )
{
	char szValue[16];
	KeyValueData kvd;

	_snprintf( szValue, sizeof( szValue ), "%d", iValue );
	szValue[sizeof( szValue ) - 1] = 0;
	kvd.szClassName = (char *)STRING( pent->v.classname );
	kvd.szKeyName = (char *)pszKey;
	kvd.szValue = szValue;
	kvd.fHandled = FALSE;
	DispatchKeyValue( pent, &kvd );
}

static BOOL GmSpawnMonster( CBasePlayer *pPlayer )
{
	if ( !s_bCatalogPrecached )
		return FALSE;

	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	const char *pszClass = GmPlacementClass( gm );

	if ( !pszClass[0] )
		return FALSE;

	if ( !GmEdictsLeft() )
	{
		GmPrint( pPlayer, "Too many entities on the map: nothing more can be placed." );
		return FALSE;
	}

	int iPick = GM_PICK( gm.iEntry, gm.iVariant );
	if ( !GmAskPlace( pPlayer, iPick ) )
		return FALSE;	// refused: the reason under the crosshair, the placing goes on (click again when allowed)

	GmUpdatePreview( pPlayer );

	edict_t *pent = CREATE_NAMED_ENTITY( MAKE_STRING( pszClass ) );
	if ( FNullEnt( pent ) )
	{
		ALERT( at_console, "GM mode: can't create %s\n", pszClass );
		return FALSE;
	}

	int iOrientation = gm.iOrientation;
	Vector vecOrigin = gm.vecOrigin;
	Vector vecFx = vecOrigin;

	// sma: the effect 48 units off the floor / ceiling, or 28 off the wall
	if ( iOrientation == GM_ORIENT_WALL )
		vecFx = vecFx + gm.vecNormal * GM_TELEPORT_SPRITE_BASE_Z;
	else if ( iOrientation == GM_ORIENT_CEILING )
		vecFx.z -= 48;
	else
		vecFx.z += 48;

	pent->v.origin = vecOrigin;
	pent->v.angles = gm.vecAngles;

	if ( IsTurretClass( pszClass ) )
	{
		pent->v.spawnflags |= SF_MONSTER_TURRET_AUTOACTIVATE;
		pent->v.spawnflags &= ~SF_MONSTER_FALL_TO_GROUND;
	}
	else
		pent->v.spawnflags |= SF_MONSTER_FALL_TO_GROUND;

	int iWeapons = GmPlacementWeapons( gm );
	if ( iWeapons != -1 )
		GmKeyValue( pent, "weapons", iWeapons );
	if ( IsTurretClass( pszClass ) && iOrientation != GM_ORIENT_DEFAULT )
		GmKeyValue( pent, "orientation", iOrientation );

	if ( gm.iBody != -1 )
		pent->v.body = gm.iBody;
	if ( gm.iSkin != -1 )
		pent->v.skin = gm.iSkin;

	// the owner before the spawn, so a plugin's Spawn hook sees it too
	CBaseEntity *pCreated = CBaseEntity::Instance( pent );
	CBaseMonster *pCreatedMonster = pCreated ? pCreated->MyMonsterPointer() : NULL;
	if ( pCreatedMonster )
	{
		pCreatedMonster->m_hGmOwner = pPlayer;
		pCreatedMonster->m_iGmOwnerUserId = GETPLAYERUSERID( pPlayer->edict() );
		pCreatedMonster->m_iGmEntry = gm.iEntry;
	}

	if ( DispatchSpawn( pent ) == -1 )
	{
		REMOVE_ENTITY( pent );
		ALERT( at_console, "GM mode: %s didn't spawn\n", pszClass );
		return FALSE;
	}

	CBaseEntity *pEntity = CBaseEntity::Instance( pent );
	if ( !pEntity )
		return FALSE;

	if ( IsTurretClass( pszClass ) )
		Turret_GmActivate( pEntity );

	GmSpawnEffect( vecFx );
	EMIT_SOUND( pent, CHAN_AUTO, GM_SPAWN_SOUND, VOL_NORM, ATTN_NORM );

	char szText[96];
	_snprintf( szText, sizeof( szText ), "Spawned %s", g_GmCatalog[gm.iEntry].pszName );
	szText[sizeof( szText ) - 1] = 0;
	GmPrint( pPlayer, szText );

	pPlayer->GmPlaced( pEntity, iPick );

	// it joins the selection (it glows): an order right away, the orders menu open as after a box
	if ( GmSelectable( pPlayer, pEntity ) && gm.iSelected < GM_SELECT_MAX )
	{
		gm.hSelected[gm.iSelected++] = pEntity;
		gm.flNextHud = 0;
		GmOrdersMenu( pPlayer );
	}

	// sma finalize_placement_preview: a new random look for the next one
	GmSelectAppearance( pszClass, iWeapons, gm.iBody, gm.iSkin );
	GmApplyPreviewAppearance( gm );
	return TRUE;
}

// The monster a Game Master placed, given an entity that may be it or something it fired (sma
// resolve_gm_monster_entity: up to three owners up); its owner must still be the same connected player
static CBaseMonster *GmOwnedMonster( entvars_t *pev, CBasePlayer **ppOwner )
{
	for ( int i = 0; i < 3 && pev; i++ )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( pev );
		CBaseMonster *pMonster = pEntity ? pEntity->MyMonsterPointer() : NULL;

		if ( pMonster && !pMonster->IsPlayer() && (CBaseEntity *)pMonster->m_hGmOwner )
		{
			CBaseEntity *pOwner = pMonster->m_hGmOwner;
			if ( !pOwner || !pOwner->IsPlayer() || ( (CBasePlayer *)pOwner )->has_disconnected
				|| GETPLAYERUSERID( pOwner->edict() ) != pMonster->m_iGmOwnerUserId )
				return NULL;
			if ( ppOwner )
				*ppOwner = (CBasePlayer *)pOwner;
			return pMonster;
		}

		if ( FNullEnt( pev->owner ) )
			break;
		pev = VARS( pev->owner );
	}
	return NULL;
}

//=========================================================
// The orders menu's "Destroy selected": kills one of the Game Master's own monsters (damage every monster takes in full:
// the gargantua and the turrets cut other damage types; from the world, so nobody gets the kill)
//=========================================================
static void GmKillMonster( CBaseEntity *pEntity )
{
	if ( !pEntity || pEntity->IsPlayer() || !( pEntity->pev->flags & FL_MONSTER ) || !pEntity->IsAlive()
		|| pEntity->pev->takedamage == DAMAGE_NO )
		return;

	entvars_t *pevWorld = VARS( INDEXENT( 0 ) );
	pEntity->TakeDamage( pevWorld, pevWorld, pEntity->pev->health + 100000, DMG_BLAST | DMG_ALWAYSGIB );
}

//=========================================================
// Riders: weapon drops and teleporter pads placed on a func_train / func_tracktrain / func_plat / func_platrot move
// with it (sma gm_try_attach_train_rider, gm_sync_train_rider; synced every frame in GmMode_StartFrame)
//=========================================================
struct GmRider
{
	EHANDLE hEntity;
	EHANDLE hPlatform;
	Vector vecLocal;	// offset in the platform's yaw frame
	float flLastYaw;
	float flSnapHold;	// EF_NOINTERP until then (the platform jumped)
};

static GmRider s_Riders[GM_MAX_RIDERS];
static int s_iRiderCount;

static void GmResetRiders( void )
{
	memset( s_Riders, 0, sizeof( s_Riders ) );
	s_iRiderCount = 0;
}

static BOOL GmIsTrainEntity( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return FALSE;
	return FClassnameIs( pEntity->pev, "func_train" ) || FClassnameIs( pEntity->pev, "func_tracktrain" )
		|| FClassnameIs( pEntity->pev, "func_plat" ) || FClassnameIs( pEntity->pev, "func_platrot" );
}

static Vector GmRotateYaw( const Vector &vec, float flYaw )
{
	float c = cos( flYaw * M_PI / 180.0 ), s = sin( flYaw * M_PI / 180.0 );
	return Vector( vec.x * c - vec.y * s, vec.x * s + vec.y * c, vec.z );
}

static void GmAttachRider( CBaseEntity *pEntity, CBaseEntity *pPlatform )
{
	if ( !pEntity || !GmIsTrainEntity( pPlatform ) || s_iRiderCount >= GM_MAX_RIDERS )
		return;

	GmRider &rider = s_Riders[s_iRiderCount++];
	rider.hEntity = pEntity;
	rider.hPlatform = pPlatform;
	rider.vecLocal = GmRotateYaw( pEntity->pev->origin - pPlatform->pev->origin, -pPlatform->pev->angles.y );
	rider.flLastYaw = NormalizeAngle( pPlatform->pev->angles.y );
	rider.flSnapHold = 0;
}

// the platform under (or above, or behind) a placed entity, from its surface (sma gm_try_attach_train_rider)
static CBaseEntity *GmFindPlatform( CBaseEntity *pEntity, int iOrientation, const Vector &vecNormal )
{
	Vector vecDir;
	if ( iOrientation == GM_ORIENT_CEILING )
		vecDir = Vector( 0, 0, 1 );
	else if ( iOrientation == GM_ORIENT_WALL )
		vecDir = -vecNormal;
	else
		vecDir = Vector( 0, 0, -1 );

	TraceResult tr;
	UTIL_TraceLine( pEntity->pev->origin, pEntity->pev->origin + vecDir * GM_TRAIN_ATTACH_DIST, ignore_monsters,
		pEntity->edict(), &tr );
	CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
	return GmIsTrainEntity( pHit ) ? pHit : NULL;
}

static void GmRiderMoved( CBaseEntity *pEntity, float flYawDelta );

void GmMode_StartFrame( void )
{
	if ( gpGlobals->time >= s_flNextPluginCvars || gpGlobals->time < s_flNextPluginCvars - 2 )
	{
		s_flNextPluginCvars = gpGlobals->time + 1;
		s_flGmRoamSpeed = max( 0.0f, GmPluginFloat( "syndicate_gmmode_spectator_speed", GM_ROAM_SPEED ) );
	}
	GmUpdateLifebars();
	GmUpdateOrders();

	for ( int i = 0; i < s_iRiderCount; )
	{
		GmRider &rider = s_Riders[i];
		CBaseEntity *pEntity = rider.hEntity;
		CBaseEntity *pPlatform = rider.hPlatform;

		if ( !pEntity || !GmIsTrainEntity( pPlatform ) )
		{
			if ( pEntity )
				pEntity->pev->effects &= ~EF_NOINTERP;
			s_Riders[i] = s_Riders[--s_iRiderCount];
			continue;
		}

		float flYaw = NormalizeAngle( pPlatform->pev->angles.y );
		float flYawDelta = NormalizeAngle( flYaw - rider.flLastYaw );
		Vector vecOrigin = pPlatform->pev->origin + GmRotateYaw( rider.vecLocal, flYaw );
		Vector vecMove = vecOrigin - pEntity->pev->origin;

		UTIL_SetOrigin( pEntity->pev, vecOrigin );
		pEntity->pev->velocity = g_vecZero;

		// EF_NOINTERP only while the platform jumps (path corners with teleport): every frame it would also stop the
		// client's studio animation (the weapon spinner)
		if ( vecMove.Length() > GM_TRAIN_SNAP_DIST )
			rider.flSnapHold = gpGlobals->time + GM_TRAIN_SNAP_HOLD;
		if ( rider.flSnapHold > gpGlobals->time )
			pEntity->pev->effects |= EF_NOINTERP;
		else
			pEntity->pev->effects &= ~EF_NOINTERP;

		// small turns add up: the last yaw only moves when one is applied
		if ( fabs( flYawDelta ) > 0.001 )
		{
			pEntity->pev->angles.y = NormalizeAngle( pEntity->pev->angles.y + flYawDelta );
			GmRiderMoved( pEntity, flYawDelta );
			rider.flLastYaw = flYaw;
		}
		i++;
	}
}

//=========================================================
// Weapon / ammo drops (sma gm_spawn_weapon_item, gm_handle_weapon_drop_touch, gm_award_weapon_item): a base (the
// display model's teleporter body) with the weapon spinning above it; the first living player who touches it and
// can use it gets it, then it fades out. They stay when the Game Master leaves.
//=========================================================
class CGmWeaponDrop : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
	void EXPORT DropTouch( CBaseEntity *pOther );
	void EXPORT AwardThink( void );
	void EXPORT FadeThink( void );
	void UpdateOnRemove( void );

	// the Game Master who placed it (with that player's userid) and its catalog entry, as CBaseMonster's
	EHANDLE m_hGmOwner;
	int m_iGmOwnerUserId;
	int m_iGmEntry;
	EHANDLE m_hItem;
	float m_flNextUse;
	BOOL m_bFading;
	EHANDLE m_hToucher;	// the player who touched it, given the item in AwardThink
};

LINK_ENTITY_TO_CLASS( gm_weapon_drop, CGmWeaponDrop );

// the spinning weapon: touching it is touching its drop
class CGmWeaponItem : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
	void EXPORT ItemTouch( CBaseEntity *pOther );

	EHANDLE m_hDrop;
};

LINK_ENTITY_TO_CLASS( gm_weapon_item, CGmWeaponItem );

// its own model too (GmMode_Precache has it on co-op maps): a map that places the class elsewhere gets no
// unprecached SET_MODEL (Host_Error on stock engines); at run time the model is already in the list
void CGmWeaponDrop::Precache( void )
{
	PRECACHE_MODEL( (char *)GM_DISPLAY_MODEL );
}

void CGmWeaponDrop::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL( edict(), GM_DISPLAY_MODEL );
	pev->body = GM_DISPLAY_BODY_BASE;
	pev->skin = 0;
	pev->sequence = 0;
	pev->framerate = 1.0;
	pev->animtime = gpGlobals->time;
	m_flNextUse = gpGlobals->time + GM_WEAPON_TOUCH_DELAY;
	SetTouch( &CGmWeaponDrop::DropTouch );
}

// its own model too (GmMode_Precache has it on co-op maps): a map that places the class elsewhere gets no
// unprecached SET_MODEL (Host_Error on stock engines); at run time the model is already in the list
void CGmWeaponItem::Precache( void )
{
	PRECACHE_MODEL( (char *)GM_DISPLAY_MODEL );
}

void CGmWeaponItem::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL( edict(), GM_DISPLAY_MODEL );
	pev->sequence = 0;
	pev->framerate = 1.0;
	pev->animtime = gpGlobals->time;
	UTIL_SetSize( pev, Vector( -12, -12, -12 ), Vector( 12, 12, 12 ) );
	SetTouch( &CGmWeaponItem::ItemTouch );
}

void CGmWeaponItem::ItemTouch( CBaseEntity *pOther )
{
	CBaseEntity *pDrop = m_hDrop;
	if ( pDrop && FClassnameIs( pDrop->pev, "gm_weapon_drop" ) )
		( (CGmWeaponDrop *)pDrop )->DropTouch( pOther );
}

void CGmWeaponDrop::UpdateOnRemove( void )
{
	CBaseEntity *pItem = m_hItem;
	if ( pItem )
		UTIL_Remove( pItem );
	m_hItem = NULL;
	CBaseEntity::UpdateOnRemove();
}

static CBasePlayerItem *GmFindPlayerItem( CBasePlayer *pPlayer, const char *pszClass )
{
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		for ( CBasePlayerItem *pItem = pPlayer->m_rgpPlayerItems[i]; pItem; pItem = pItem->m_pNext )
		{
			if ( FClassnameIs( pItem->pev, pszClass ) )
				return pItem;
		}
	}
	return NULL;
}

// sma gm_refill_weapon_limited: two clips more in reserve (up to the maximum); a weapon without a clip (grenades,
// mines, satchels, pitbulls) is filled up
static void GmRefillLimited( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if ( !pItem || !pItem->pszAmmo1() || pItem->iMaxAmmo1() <= 0 )
		return;

	int iAmmo = pPlayer->GetAmmoIndex( pItem->pszAmmo1() );
	if ( iAmmo < 0 )
		return;

	int iMax = pItem->iMaxAmmo1();
	int iAdd = ( pItem->iMaxClip() > WEAPON_NOCLIP ) ? min( pItem->iMaxClip() * 2, iMax ) : iMax;
	pPlayer->m_rgAmmo[iAmmo] = min( pPlayer->m_rgAmmo[iAmmo] + iAdd, iMax );
}

// CBasePlayer::GiveNamedItem, but nothing is left in the world if the player didn't take it (a weapon the game rules
// refuse, an ammo box the player has no room for): it would lie at the player's feet, and the drop would make a new
// one at every touch
static void GmGiveItem( CBasePlayer *pPlayer, const char *pszClass )
{
	if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
		return;

	edict_t *pent = CREATE_NAMED_ENTITY( MAKE_STRING( pszClass ) );
	if ( FNullEnt( pent ) )
		return;

	pent->v.origin = pPlayer->pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;

	if ( DispatchSpawn( pent ) == -1 )
	{
		REMOVE_ENTITY( pent );
		return;
	}
	DispatchTouch( pent, pPlayer->edict() );

	// a weapon the player took follows the player (AttachToPlayer: aiment); anything else goes now: an ammo box has
	// given what it could, a refused weapon must not stay behind
	CBaseEntity *pEntity = CBaseEntity::Instance( pent );
	if ( pEntity && pent->v.aiment != pPlayer->edict() && !( pent->v.flags & FL_KILLME ) )
		UTIL_Remove( pEntity );
}

// sma gm_award_weapon_item: FALSE if the player can't use it (it stays for someone else)
static BOOL GmAwardItem( CBasePlayer *pPlayer, int iEntry )
{
	const GmCatalogEntry &entry = g_GmCatalog[iEntry];

	if ( FStrEq( entry.pszClass, "ammo_reserve" ) )
	{
		GmGiveItem( pPlayer, entry.pszClass );
		return TRUE;
	}

	CBasePlayerItem *pExisting = GmFindPlayerItem( pPlayer, entry.pszClass );
	if ( pExisting )
	{
		// melee weapons and the medikit only once (sma gm_is_single_use_weapon); a full reserve takes nothing
		if ( FStrEq( entry.pszClass, "weapon_warhammer" ) || FStrEq( entry.pszClass, "weapon_chainsaw" )
			|| FStrEq( entry.pszClass, "weapon_sword" ) || FStrEq( entry.pszClass, "weapon_medikit" ) )
			return FALSE;

		if ( pExisting->pszAmmo1() && pExisting->iMaxAmmo1() > 0 )
		{
			int iAmmo = pPlayer->GetAmmoIndex( pExisting->pszAmmo1() );
			if ( iAmmo >= 0 && pPlayer->m_rgAmmo[iAmmo] >= pExisting->iMaxAmmo1() )
				return FALSE;
		}

		GmRefillLimited( pPlayer, pExisting );
		return TRUE;
	}

	GmGiveItem( pPlayer, entry.pszClass );
	CBasePlayerItem *pItem = GmFindPlayerItem( pPlayer, entry.pszClass );
	if ( !pItem )
		return FALSE;

	GmRefillLimited( pPlayer, pItem );
	pPlayer->SelectItem( entry.pszClass );	// a new weapon: in the hands at once
	return TRUE;
}

void CGmWeaponDrop::DropTouch( CBaseEntity *pOther )
{
	if ( m_bFading || !pOther || !pOther->IsPlayer() || !pOther->IsAlive() || gpGlobals->time < m_flNextUse )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	if ( pPlayer->IsObserver() || pPlayer->m_bGameMaster || m_iGmEntry < 0 || m_iGmEntry >= GM_CATALOG_COUNT )
		return;

	// The engine calls this while it walks its list of triggers: creating the item (a new entity, an ammo box that
	// is a trigger itself) or unlinking the spinner here would change that list under it. The item is given at the
	// next think, outside the touch.
	if ( (CBaseEntity *)m_hToucher )
		return;
	m_hToucher = pPlayer;
	SetThink( &CGmWeaponDrop::AwardThink );
	pev->nextthink = gpGlobals->time + 0.01;
}

void CGmWeaponDrop::AwardThink( void )
{
	CBaseEntity *pToucher = m_hToucher;
	m_hToucher = NULL;

	if ( m_bFading || !pToucher || !pToucher->IsPlayer() || !pToucher->IsAlive() )
		return;
	CBasePlayer *pPlayer = (CBasePlayer *)pToucher;
	if ( pPlayer->has_disconnected || pPlayer->IsObserver() || pPlayer->m_bGameMaster )
		return;

	if ( !GmAwardItem( pPlayer, m_iGmEntry ) )
		return;	// the next touch tries again (for this player, or another one)

	// sma: no more touches, a fade (renderamt -36 every 0.05 s), then gone with its item
	m_bFading = TRUE;
	SetTouch( NULL );
	pev->solid = SOLID_NOT;
	UTIL_SetOrigin( pev, pev->origin );
	CBaseEntity *pItem = m_hItem;
	if ( pItem )
	{
		pItem->SetTouch( NULL );
		pItem->pev->solid = SOLID_NOT;
		UTIL_SetOrigin( pItem->pev, pItem->pev->origin );
	}
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 255;
	SetThink( &CGmWeaponDrop::FadeThink );
	pev->nextthink = gpGlobals->time + GM_FADE_INTERVAL;
}

void CGmWeaponDrop::FadeThink( void )
{
	pev->renderamt -= GM_FADE_STEP;
	if ( pev->renderamt <= 0 )
	{
		UTIL_Remove( this );
		return;
	}

	CBaseEntity *pItem = m_hItem;
	if ( pItem )
	{
		pItem->pev->rendermode = kRenderTransTexture;
		pItem->pev->renderamt = pev->renderamt;
	}
	pev->nextthink = gpGlobals->time + GM_FADE_INTERVAL;
}

//=========================================================
// Teleporter pads (sma gm_spawn_teleporter_pad, gm_handle_teleporter_touch): an entrance and its exit, placed by a
// Game Master and removed when the Game Master leaves. A living player touching a pad comes out of the other one.
//=========================================================
class CGmTeleporter : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
	void EXPORT PadTouch( CBaseEntity *pOther );
	void UpdateOnRemove( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void EXPORT FlashThink( void );
	void EXPORT DestroyThink( void );
	void StartDestroy( void );

	// the Game Master who placed it (with that player's userid) and its catalog entry (Teleporters), as
	// CBaseMonster's
	EHANDLE m_hGmOwner;
	int m_iGmOwnerUserId;
	int m_iGmEntry;
	int m_iRole;			// GM_TELE_*
	EHANDLE m_hLinked;		// the other pad of the pair
	int m_iOrientation;
	Vector m_vecNormal;
	EHANDLE m_hMarker;		// enter1.spr above it
	EHANDLE m_hPlatform;	// riding on it
	float m_flNextUse;
	BOOL m_bDestroying;		// broken: fading out
};

LINK_ENTITY_TO_CLASS( gm_teleporter_pad, CGmTeleporter );

// its own model too (GmMode_Precache has it on co-op maps): a map that places the class elsewhere gets no
// unprecached SET_MODEL (Host_Error on stock engines); at run time the model is already in the list
void CGmTeleporter::Precache( void )
{
	PRECACHE_MODEL( (char *)GM_DISPLAY_MODEL );
}

void CGmTeleporter::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL( edict(), GM_DISPLAY_MODEL );
	pev->body = GM_DISPLAY_BODY_BASE;
	pev->skin = GM_TELEPORTER_SKIN;
	pev->sequence = 0;
	pev->framerate = 1.0;
	pev->animtime = gpGlobals->time;
	// sma gm_spawn_teleporter_pad with production's teleporter_health 2500: shootable and solid. A plugin can change
	// health / takedamage / solid in its GmPlaced hook (takedamage DAMAGE_NO: unbreakable).
	pev->takedamage = DAMAGE_AIM;
	pev->solid = SOLID_BBOX;
	pev->health = GM_TELEPORTER_HEALTH;
	pev->max_health = GM_TELEPORTER_HEALTH;
	m_flNextUse = gpGlobals->time + GM_TELEPORTER_TOUCH_DELAY;
	SetTouch( &CGmTeleporter::PadTouch );
}

// sma ham_info_target_takedamage_pre: only a connected player's damage counts; a red flash on each hit; at 0 both
// pads of the pair fade out and go. No engine damage handling (no blood, sounds or gibs), as the plugin superseded it.
int CGmTeleporter::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( flDamage <= 0 || pev->takedamage == DAMAGE_NO )
		return 0;

	int iAttacker = pevAttacker ? ENTINDEX( ENT( pevAttacker ) ) : 0;
	if ( iAttacker < 1 || iAttacker > gpGlobals->maxClients )
		return 0;
	CBasePlayer *pAttacker = (CBasePlayer *)UTIL_PlayerByIndex( iAttacker );
	if ( !pAttacker || pAttacker->has_disconnected )
		return 0;

	float flHealth = pev->health;
	if ( flHealth <= 0 )
		flHealth = ( pev->max_health > 0 ) ? pev->max_health : GM_TELEPORTER_HEALTH;
	flHealth -= flDamage;

	// sma gm_flash_teleporter_hit: a red glow shell for 0.12 s
	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = Vector( 255, 32, 32 );
	pev->renderamt = GM_TELEPORTER_HIT_FLASH_AMT;
	SetThink( &CGmTeleporter::FlashThink );
	pev->nextthink = gpGlobals->time + GM_TELEPORTER_HIT_FLASH_TIME;

	if ( flHealth <= 0 )
	{
		// sma gm_queue_teleporter_destroy: the paired pad goes with it
		CGmTeleporter *pLinked = (CGmTeleporter *)(CBaseEntity *)m_hLinked;
		if ( pLinked && pLinked != this && FClassnameIs( pLinked->pev, "gm_teleporter_pad" ) )
			pLinked->StartDestroy();
		StartDestroy();
	}
	else
		pev->health = flHealth;
	return 1;
}

void CGmTeleporter::FlashThink( void )
{
	// sma task_teleporter_hit_flash
	pev->renderfx = kRenderFxNone;
	pev->rendercolor = Vector( 255, 255, 255 );
	pev->renderamt = 255;
}

// sma gm_start_teleporter_destroy_fade: no more touches or damage, then a fade (renderamt -34 every 0.05 s)
void CGmTeleporter::StartDestroy( void )
{
	if ( m_bDestroying )
		return;
	m_bDestroying = TRUE;

	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetOrigin( pev, pev->origin );
	SetTouch( NULL );
	pev->rendermode = kRenderTransTexture;
	pev->renderfx = kRenderFxNone;
	pev->rendercolor = Vector( 255, 255, 255 );
	pev->renderamt = 255;
	SetThink( &CGmTeleporter::DestroyThink );
	pev->nextthink = gpGlobals->time + GM_TELEPORTER_FADE_INTERVAL;
}

void CGmTeleporter::DestroyThink( void )
{
	// sma task_teleporter_destroy, then gm_remove_teleporter_pad (UpdateOnRemove: its marker)
	pev->renderamt -= GM_TELEPORTER_FADE_STEP;
	if ( pev->renderamt <= 0 )
	{
		CBaseEntity *pOwner = m_hGmOwner;
		UTIL_Remove( this );	// UpdateOnRemove unlinks the pair, so it no longer counts
		// the Game Master's window: the pairs that are left
		if ( pOwner && pOwner->IsPlayer() && ( (CBasePlayer *)pOwner )->m_bGameMaster )
			GmSendState( (CBasePlayer *)pOwner, GM_EVENT_NONE );
		return;
	}
	pev->nextthink = gpGlobals->time + GM_TELEPORTER_FADE_INTERVAL;
}

void CGmTeleporter::UpdateOnRemove( void )
{
	CBaseEntity *pMarker = m_hMarker;
	if ( pMarker )
		UTIL_Remove( pMarker );
	m_hMarker = NULL;

	CBaseEntity *pLinked = m_hLinked;
	if ( pLinked )
		( (CGmTeleporter *)pLinked )->m_hLinked = NULL;
	m_hLinked = NULL;

	CBaseEntity::UpdateOnRemove();
}

static void GmRiderMoved( CBaseEntity *pEntity, float flYawDelta )
{
	// a pad on a turning platform: its wall normal turns with it
	if ( FClassnameIs( pEntity->pev, "gm_teleporter_pad" ) )
	{
		CGmTeleporter *pPad = (CGmTeleporter *)pEntity;
		pPad->m_vecNormal = GmRotateYaw( pPad->m_vecNormal, flYawDelta );
	}
}

static BOOL GmHullClear( CBasePlayer *pPlayer, const Vector &vecOrigin )
{
	TraceResult tr;
	UTIL_TraceHull( vecOrigin, vecOrigin, ignore_monsters, human_hull, pPlayer->edict(), &tr );
	return !tr.fAllSolid && !tr.fStartSolid;
}

// sma gm_adjust_teleport_destination: up to 12 steps of 8 units away from the surface, then 6 towards it
static void GmAdjustDestination( CBasePlayer *pPlayer, int iOrientation, Vector &vecDest )
{
	if ( GmHullClear( pPlayer, vecDest ) )
		return;

	float flDir = ( iOrientation == GM_ORIENT_CEILING ) ? -1 : 1;
	int i;
	for ( i = 1; i <= GM_TELEPORTER_SAFE_STEPS; i++ )
	{
		Vector vecTest = vecDest + Vector( 0, 0, flDir * i * GM_TELEPORTER_SAFE_STEP );
		if ( GmHullClear( pPlayer, vecTest ) )
		{
			vecDest = vecTest;
			return;
		}
	}
	for ( i = 1; i <= GM_TELEPORTER_SAFE_STEPS / 2; i++ )
	{
		Vector vecTest = vecDest - Vector( 0, 0, flDir * i * GM_TELEPORTER_SAFE_STEP );
		if ( GmHullClear( pPlayer, vecTest ) )
		{
			vecDest = vecTest;
			return;
		}
	}
}

extern void Semiclip_PassUntilApart( edict_t *pA, edict_t *pB );

void CGmTeleporter::PadTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsPlayer() || !pOther->IsAlive() || gpGlobals->time < m_flNextUse )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	GmPlayerState &gmPlayer = s_Gm[pPlayer->entindex()];
	if ( pPlayer->IsObserver() || gmPlayer.flTeleportCooldown > gpGlobals->time )
		return;

	CGmTeleporter *pExit = (CGmTeleporter *)(CBaseEntity *)m_hLinked;
	if ( !pExit || !FClassnameIs( pExit->pev, "gm_teleporter_pad" ) )
		return;

	// out of the exit pad, 28 units off its surface, 48 units along its facing
	int iOrientation = pExit->m_iOrientation;
	Vector vecDest = GmOffAlongSurface( pExit->pev->origin, iOrientation, pExit->m_vecNormal, GM_TELEPORTER_OFFSET );

	Vector vecForward;
	if ( iOrientation == GM_ORIENT_WALL )
	{
		vecForward = pExit->m_vecNormal;
		if ( vecForward.Length() > 0.001 )
			vecForward = vecForward.Normalize();
		else
			vecForward = Vector( 1, 0, 0 );
	}
	else
	{
		UTIL_MakeVectors( pExit->pev->angles );
		vecForward = gpGlobals->v_forward;
		vecForward.z = 0;
		if ( vecForward.Length() > 0.001 )
			vecForward = vecForward.Normalize();
		else
			vecForward = Vector( 1, 0, 0 );
	}
	vecDest = vecDest + vecForward * GM_TELEPORTER_EXIT_PUSH;

	GmAdjustDestination( pPlayer, iOrientation, vecDest );
	// clear of the pad and the surface
	vecDest.z += ( iOrientation == GM_ORIENT_CEILING ) ? -GM_TELEPORTER_CLEARANCE : GM_TELEPORTER_CLEARANCE;

	// an exit riding a platform: further off it, so the player drops onto it instead of into its way
	CBaseEntity *pPlatform = pExit->m_hPlatform;
	if ( !GmIsTrainEntity( pPlatform ) )
		pPlatform = NULL;
	if ( pPlatform )
		vecDest.z += ( iOrientation == GM_ORIENT_CEILING ) ? -GM_TELEPORTER_TRAIN_LIFT : GM_TELEPORTER_TRAIN_LIFT;
	GmAdjustDestination( pPlayer, iOrientation, vecDest );

	pPlayer->pev->velocity = pPlatform ? pPlatform->pev->velocity : g_vecZero;
	pPlayer->pev->basevelocity = g_vecZero;
	UTIL_SetOrigin( pPlayer->pev, vecDest );

	// facing out of the exit
	float flYaw = NormalizeAngle( atan2( vecForward.y, vecForward.x ) * 180 / M_PI );
	pPlayer->pev->angles = Vector( 0, flYaw, 0 );
	pPlayer->pev->v_angle = Vector( 0, flYaw, 0 );
	pPlayer->pev->fixangle = TRUE;

	// players coming out inside each other pass through each other until apart (sma: a 2 s semiclip pair override)
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pOtherPlayer = UTIL_PlayerByIndex( i );
		if ( pOtherPlayer && pOtherPlayer != pPlayer && pOtherPlayer->IsAlive() )
			Semiclip_PassUntilApart( pPlayer->edict(), pOtherPlayer->edict() );
	}

	float flNext = gpGlobals->time + GM_TELEPORTER_COOLDOWN;
	gmPlayer.flTeleportCooldown = flNext;
	m_flNextUse = flNext;
	pExit->m_flNextUse = flNext;
}

// The Game Master's complete pairs, by their entrances, in entity order (sma gm_collect_teleporter_pairs)
static int GmCollectPairs( CBasePlayer *pPlayer, CGmTeleporter **ppEntrances, int iMax )
{
	int iCount = 0;
	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "gm_teleporter_pad" ) ) != NULL && iCount < iMax )
	{
		CGmTeleporter *pPad = (CGmTeleporter *)pEntity;
		CGmTeleporter *pExit = (CGmTeleporter *)(CBaseEntity *)pPad->m_hLinked;
		if ( pPad->m_iRole == GM_TELE_ENTRANCE && (CBaseEntity *)pPad->m_hGmOwner == pPlayer && pExit
			&& pExit->m_iRole == GM_TELE_EXIT && (CBaseEntity *)pExit->m_hLinked == pPad )
			ppEntrances[iCount++] = pPad;
	}
	return iCount;
}

static int GmTeleporterInfo( CBasePlayer *pPlayer )
{
	CGmTeleporter *pPairs[GM_TP_MAX_PAIRS];
	int iInfo = GmCollectPairs( pPlayer, pPairs, GM_TP_MAX_PAIRS ) & GM_TP_PAIRS_MASK;
	if ( (CBaseEntity *)s_Gm[pPlayer->entindex()].hEntrance )
		iInfo |= GM_TP_ENTRANCE_PENDING;
	return iInfo;
}

static void GmRemovePad( CGmTeleporter *pPad )
{
	if ( pPad )
		UTIL_Remove( pPad );	// UpdateOnRemove: its marker
}

// What a Game Master can select (click, box, the players window's map): everything he placed: monsters of every kind
// (turrets, the Apache, the Pitbull too: they take no orders but can be destroyed), weapon drops, teleporter pads
static BOOL GmSelectable( CBasePlayer *pPlayer, CBaseEntity *pEntity )
{
	if ( !pPlayer || !pEntity || pEntity->IsPlayer() )
		return FALSE;
	const int iUserId = GETPLAYERUSERID( pPlayer->edict() );
	if ( pEntity->pev->flags & FL_MONSTER )
	{
		CBaseMonster *pMonster = pEntity->MyMonsterPointer();
		return pMonster && pMonster->pev->deadflag == DEAD_NO && (CBaseEntity *)pMonster->m_hGmOwner == pPlayer
			&& pMonster->m_iGmOwnerUserId == iUserId;
	}
	if ( FClassnameIs( pEntity->pev, "gm_weapon_drop" ) )
	{
		CGmWeaponDrop *pDrop = (CGmWeaponDrop *)pEntity;
		return !pDrop->m_bFading && (CBaseEntity *)pDrop->m_hGmOwner == pPlayer && pDrop->m_iGmOwnerUserId == iUserId;
	}
	if ( FClassnameIs( pEntity->pev, "gm_teleporter_pad" ) )
	{
		CGmTeleporter *pPad = (CGmTeleporter *)pEntity;
		return !pPad->m_bDestroying && (CBaseEntity *)pPad->m_hGmOwner == pPlayer && pPad->m_iGmOwnerUserId == iUserId;
	}
	return FALSE;
}

// the spinning weapon of a drop stands for its drop
static CBaseEntity *GmSelectTarget( CBaseEntity *pEntity )
{
	if ( pEntity && FClassnameIs( pEntity->pev, "gm_weapon_item" ) )
		return ( (CGmWeaponItem *)pEntity )->m_hDrop;
	return pEntity;
}

static void GmRemoveTeleporters( CBasePlayer *pPlayer )
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "gm_teleporter_pad" ) ) != NULL )
	{
		if ( (CBaseEntity *)( (CGmTeleporter *)pEntity )->m_hGmOwner == pPlayer )
			GmRemovePad( (CGmTeleporter *)pEntity );
	}
	s_Gm[pPlayer->entindex()].hEntrance = NULL;
}

// sma gm_create_teleporter_marker: enter1.spr 48 units off the pad, hidden on an entrance until its exit is placed
static CBaseEntity *GmCreateMarker( CGmTeleporter *pPad )
{
	CSprite *pSprite = CSprite::SpriteCreate( GM_SPAWN_SPRITE,
		GmOffAlongSurface( pPad->pev->origin, pPad->m_iOrientation, pPad->m_vecNormal, GM_TELEPORTER_MARKER_Z ), FALSE );
	if ( !pSprite )
		return NULL;
	pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
	pSprite->SetScale( 0.7 );
	pSprite->pev->framerate = 12;
	pSprite->TurnOn();	// animated, looping (sma SF_SPRITE_STARTON)
	pSprite->pev->classname = MAKE_STRING( "gm_teleporter_marker" );
	if ( pPad->m_iRole == GM_TELE_ENTRANCE )
		pSprite->pev->effects |= EF_NODRAW;
	return pSprite;
}

//=========================================================
// Placement of weapon drops and teleporters
//=========================================================
static int GmToolEntry( int iTool )
{
	for ( int i = 0; i < GM_CATALOG_COUNT; i++ )
	{
		if ( g_GmCatalog[i].iKind == GM_KIND_TOOL && g_GmCatalog[i].iTool == iTool )
			return i;
	}
	return -1;
}

// CBaseEntity::Create without the spawn: the caller sets the Game Master fields, then DispatchSpawn
static CBaseEntity *GmCreateOwned( const char *pszClass, const Vector &vecOrigin, const Vector &vecAngles )
{
	edict_t *pent = CREATE_NAMED_ENTITY( MAKE_STRING( pszClass ) );
	if ( FNullEnt( pent ) )
		return NULL;
	CBaseEntity *pEntity = CBaseEntity::Instance( pent );
	if ( !pEntity )
		return NULL;
	pEntity->pev->origin = vecOrigin;
	pEntity->pev->angles = vecAngles;
	return pEntity;
}

static void GmStartWeaponPlacement( CBasePlayer *pPlayer, int iEntry )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];

	GmEndPlacement( pPlayer, FALSE );
	GmClearSelection( pPlayer );	// sma: a placement suspends the NPC controls and clears the selection
	gm.iPlace = GM_PLACE_WEAPON;
	gm.iEntry = iEntry;
	gm.iVariant = -1;
	gm.flYawOffset = 0;
	gm.iOrientation = GM_ORIENT_FLOOR;
	gm.bAttackHeld = TRUE;

	// sma start_weapon_preview: the base (90) and the weapon above it (130)
	gm.hPreview = GmCreateDisplay( pPlayer, GM_DISPLAY_MODEL, GM_DISPLAY_BODY_BASE, 90 );
	gm.hItem = GmCreateDisplay( pPlayer, GM_DISPLAY_MODEL, g_GmCatalog[iEntry].iBody, 130 );
	GmUpdatePreview( pPlayer );
	GmSendState( pPlayer, GM_EVENT_PLACE, g_GmCatalog[iEntry].pszName, iEntry );
}

static void GmStartTeleporterPlacement( CBasePlayer *pPlayer, int iRole )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	int iEntry = GmToolEntry( GM_TOOL_TELEPORTERS );

	GmEndPlacement( pPlayer, FALSE );
	GmClearSelection( pPlayer );	// sma: a placement suspends the NPC controls and clears the selection
	gm.iPlace = GM_PLACE_TELEPORTER;
	gm.iEntry = iEntry;
	gm.iVariant = -1;
	gm.iTeleRole = iRole;
	gm.flYawOffset = 0;
	gm.iOrientation = GM_ORIENT_FLOOR;
	gm.bAttackHeld = TRUE;

	// sma start_teleporter_preview: the pad and the arrow showing where the player will face
	gm.hPreview = GmCreateDisplay( pPlayer, GM_DISPLAY_MODEL, GM_DISPLAY_BODY_BASE, 90 );
	CBaseEntity *pPreview = gm.hPreview;
	if ( pPreview )
		pPreview->pev->skin = GM_TELEPORTER_SKIN;
	gm.hHelper = GmCreateDisplay( pPlayer, GM_DISPLAY_MODEL, GM_DISPLAY_BODY_ARROW, 180 );
	GmUpdatePreview( pPlayer );
	GmSendState( pPlayer, GM_EVENT_PLACE, iRole == GM_TELE_EXIT ? "Teleporter Exit" : "Teleporter Entrance",
		( iEntry >= 0 ) ? iEntry : 255 );
}

static void GmPlaceWeapon( CBasePlayer *pPlayer )
{
	if ( !s_bCatalogPrecached )
		return;

	GmPlayerState &gm = s_Gm[pPlayer->entindex()];

	if ( gm.iEntry < 0 || gm.iEntry >= GM_CATALOG_COUNT || g_GmCatalog[gm.iEntry].iKind != GM_KIND_WEAPON )
		return;
	if ( !GmEdictsLeft() )
	{
		GmPrint( pPlayer, "Too many entities on the map: nothing more can be placed." );
		return;
	}

	int iPick = GM_PICK( gm.iEntry, -1 );
	if ( !GmAskPlace( pPlayer, iPick ) )
		return;	// refused: the placing goes on

	GmUpdatePreview( pPlayer );

	CGmWeaponDrop *pDrop = (CGmWeaponDrop *)GmCreateOwned( "gm_weapon_drop", gm.vecOrigin, gm.vecAngles );
	if ( !pDrop )
		return;
	pDrop->m_hGmOwner = pPlayer;
	pDrop->m_iGmOwnerUserId = GETPLAYERUSERID( pPlayer->edict() );
	pDrop->m_iGmEntry = gm.iEntry;
	DispatchSpawn( pDrop->edict() );

	// sma: 36 x 36 x 18 over the base; on the ceiling the volume reaches down past the weapon below it
	if ( gm.iOrientation == GM_ORIENT_CEILING )
		UTIL_SetSize( pDrop->pev, Vector( -18, -18, -( 18 + GM_WEAPON_ITEM_Z + 8 ) ), Vector( 18, 18, 0 ) );
	else
		UTIL_SetSize( pDrop->pev, Vector( -18, -18, 0 ), Vector( 18, 18, 18 ) );
	UTIL_SetOrigin( pDrop->pev, gm.vecOrigin );

	CGmWeaponItem *pItem = (CGmWeaponItem *)CBaseEntity::Create( (char *)"gm_weapon_item",
		GmItemOrigin( gm.vecOrigin, gm.iOrientation, gm.vecNormal ), gm.vecAngles, NULL );
	if ( pItem )
	{
		pItem->pev->body = g_GmCatalog[gm.iEntry].iBody;
		pItem->m_hDrop = pDrop;
		pDrop->m_hItem = pItem;
	}

	CBaseEntity *pPlatform = GmFindPlatform( pDrop, gm.iOrientation, gm.vecNormal );
	if ( pPlatform )
	{
		GmAttachRider( pDrop, pPlatform );
		if ( pItem )
			GmAttachRider( pItem, pPlatform );
	}

	GmSpawnEffect( GmOffAlongSurface( gm.vecOrigin, gm.iOrientation, gm.vecNormal, GM_TELEPORT_SPRITE_BASE_Z ) );
	EMIT_SOUND( pDrop->edict(), CHAN_AUTO, GM_SPAWN_SOUND, VOL_NORM, ATTN_NORM );

	char szText[96];
	_snprintf( szText, sizeof( szText ), "Placed %s", g_GmCatalog[gm.iEntry].pszName );
	szText[sizeof( szText ) - 1] = 0;
	GmPrint( pPlayer, szText );

	pPlayer->GmPlaced( pDrop, iPick );
}

static void GmPlaceTeleporter( CBasePlayer *pPlayer )
{
	if ( !s_bCatalogPrecached )
		return;

	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	int iRole = gm.iTeleRole;
	CGmTeleporter *pEntrance = (CGmTeleporter *)(CBaseEntity *)gm.hEntrance;

	if ( iRole == GM_TELE_EXIT && !pEntrance )
	{
		GmPrint( pPlayer, "Place Entrance first" );
		return;
	}
	if ( iRole == GM_TELE_ENTRANCE )
	{
		if ( pEntrance )
		{
			GmPrint( pPlayer, "Place Exit first" );	// sma gm_place_teleporter_from_preview
			return;
		}
		CGmTeleporter *pPairs[GM_TP_MAX_PAIRS];
		if ( GmCollectPairs( pPlayer, pPairs, GM_TP_MAX_PAIRS ) >= GM_TP_MAX_PAIRS )
		{
			GmPrint( pPlayer, "No more teleporter pairs can be placed." );
			return;
		}
	}
	if ( !GmEdictsLeft() )
	{
		GmPrint( pPlayer, "Too many entities on the map: nothing more can be placed." );
		return;
	}

	// a pair's exit is never refused: the pair is the plugin's unit, asked at its entrance
	int iPick = GmTeleporterPick( iRole );
	if ( iRole == GM_TELE_ENTRANCE && !GmAskPlace( pPlayer, iPick ) )
		return;	// refused: the placing goes on

	GmUpdatePreview( pPlayer );

	CGmTeleporter *pPad = (CGmTeleporter *)GmCreateOwned( "gm_teleporter_pad", gm.vecOrigin, gm.vecAngles );
	if ( !pPad )
		return;

	pPad->m_hGmOwner = pPlayer;
	pPad->m_iGmOwnerUserId = GETPLAYERUSERID( pPlayer->edict() );
	pPad->m_iGmEntry = GM_PICK_ENTRY( iPick );
	DispatchSpawn( pPad->edict() );
	pPad->m_iRole = iRole;
	pPad->m_iOrientation = gm.iOrientation;
	pPad->m_vecNormal = gm.vecNormal;
	if ( gm.iOrientation == GM_ORIENT_CEILING )
		UTIL_SetSize( pPad->pev, Vector( -20, -20, -20 ), Vector( 20, 20, 0 ) );
	else
		UTIL_SetSize( pPad->pev, Vector( -20, -20, 0 ), Vector( 20, 20, 20 ) );
	UTIL_SetOrigin( pPad->pev, gm.vecOrigin );

	if ( iRole == GM_TELE_EXIT )
	{
		pPad->m_hLinked = pEntrance;
		pEntrance->m_hLinked = pPad;
		// the entrance's effect shows once the pair can be used
		CBaseEntity *pMarker = pEntrance->m_hMarker;
		if ( pMarker )
			pMarker->pev->effects &= ~EF_NODRAW;
		gm.hEntrance = NULL;
	}
	else
		gm.hEntrance = pPad;

	EMIT_SOUND( pPad->edict(), CHAN_AUTO, GM_SPAWN_SOUND, VOL_NORM, ATTN_NORM );
	CBaseEntity *pMarker = GmCreateMarker( pPad );
	pPad->m_hMarker = pMarker;

	CBaseEntity *pPlatform = GmFindPlatform( pPad, gm.iOrientation, gm.vecNormal );
	if ( pPlatform )
	{
		pPad->m_hPlatform = pPlatform;
		GmAttachRider( pPad, pPlatform );
		if ( pMarker )
			GmAttachRider( pMarker, pPlatform );
	}

	GmPrint( pPlayer, iRole == GM_TELE_EXIT ? "Placed Teleporter Exit" : "Placed Teleporter Entrance" );

	pPlayer->GmPlaced( pPad, iPick );
	if ( !pPlayer->m_bGameMaster || gm.iState != GM_STATE_ACTIVE )
		return;

	if ( iRole == GM_TELE_ENTRANCE )
	{
		// sma finalize_teleporter_preview: straight on to the exit (the button has to be let go first)
		GmStartTeleporterPlacement( pPlayer, GM_TELE_EXIT );
		return;
	}

	// the pair is done: the placement ends, the quick menu comes back
	GmBackToQuickMenu( pPlayer );
}

// sma gm_focus_teleporter_pad: the camera 80 units off the pad (wall: 16 higher), looking at it
static void GmFocusPad( CBasePlayer *pPlayer, CGmTeleporter *pPad )
{
	Vector vecNormal;
	if ( pPad->m_iOrientation == GM_ORIENT_WALL )
		vecNormal = ( pPad->m_vecNormal.Length() > 0.001 ) ? pPad->m_vecNormal.Normalize() : Vector( 1, 0, 0 );
	else if ( pPad->m_iOrientation == GM_ORIENT_CEILING )
		vecNormal = Vector( 0, 0, -1 );
	else
		vecNormal = Vector( 0, 0, 1 );

	Vector vecDest = pPad->pev->origin + vecNormal * 80;
	if ( pPad->m_iOrientation == GM_ORIENT_WALL )
		vecDest.z += 16;

	GmForceRoaming( pPlayer );
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->basevelocity = g_vecZero;
	UTIL_SetOrigin( pPlayer->pev, vecDest );

	Vector vecLook = pPad->pev->origin - vecDest;
	Vector vecAngles = UTIL_VecToAngles( vecLook );
	vecAngles.x = -vecAngles.x;	// VecToAngles pitch is a model's; the view's is the other way round
	vecAngles.z = 0;
	pPlayer->pev->v_angle = vecAngles;
	pPlayer->pev->angles = vecAngles;
	pPlayer->pev->fixangle = TRUE;
}

// "gm_tp place | goto <pair> <0 / 1> | remove <pair> | clear" and the classic Teleporters menu; pairs count from 1 in
// the window's order
static void GmTpAction( CBasePlayer *pPlayer, const char *pszAction, int iPairNumber, int iWhich )
{
	CGmTeleporter *pPairs[GM_TP_MAX_PAIRS];
	int iPairs = GmCollectPairs( pPlayer, pPairs, GM_TP_MAX_PAIRS );

	if ( FStrEq( pszAction, "place" ) )
	{
		// an entrance waiting for its exit: the exit next (sma show_teleporter_place_menu)
		int iRole = (CBaseEntity *)s_Gm[pPlayer->entindex()].hEntrance ? GM_TELE_EXIT : GM_TELE_ENTRANCE;
		if ( iRole == GM_TELE_ENTRANCE && !GmAskPlace( pPlayer, GmTeleporterPick( iRole ) ) )
		{
			GmBackToQuickMenu( pPlayer );
			return;
		}
		GmShowMenu( pPlayer, GM_MENU_NONE, 0, "" );
		GmStartTeleporterPlacement( pPlayer, iRole );
		return;
	}

	if ( FStrEq( pszAction, "clear" ) )
	{
		if ( !iPairs && !(CBaseEntity *)s_Gm[pPlayer->entindex()].hEntrance )
			GmPrint( pPlayer, "No active teleporter pairs" );
		else
		{
			GmRemoveTeleporters( pPlayer );
			GmPrint( pPlayer, "Cleared all teleporters" );
		}
		GmSendState( pPlayer, GM_EVENT_NONE );
		return;
	}

	int iPair = iPairNumber - 1;
	if ( iPair < 0 || iPair >= iPairs )
		return;
	CGmTeleporter *pEntrance = pPairs[iPair];
	CGmTeleporter *pExit = (CGmTeleporter *)(CBaseEntity *)pEntrance->m_hLinked;

	if ( FStrEq( pszAction, "goto" ) )
	{
		CGmTeleporter *pPad = iWhich ? pExit : pEntrance;
		if ( pPad )
		{
			GmStopFollowing( pPlayer );
			GmFocusPad( pPlayer, pPad );
		}
		return;
	}

	if ( FStrEq( pszAction, "remove" ) )
	{
		GmRemovePad( pExit );
		GmRemovePad( pEntrance );
		GmPrint( pPlayer, "Removed teleporter pair" );
		GmSendState( pPlayer, GM_EVENT_NONE );
	}
}

static void GmTeleporterCommand( CBasePlayer *pPlayer )
{
	GmTpAction( pPlayer, CMD_ARGV( 1 ), atoi( CMD_ARGV( 2 ) ), atoi( CMD_ARGV( 3 ) ) );
}

//=========================================================
// GM-only lifebars (sma gm_create_lifebar_sprite, gm_update_lifebar_sprite, task_update_lifebars): a health bar over
// every player and every Game Master's monster, seen only by the Game Masters. The bars exist only while a Game Master
// is active (the plugin kept one per player all the time and hid them per packet), are sent to nobody else
// (GmLifebarShouldSend), and move and change every 0.25 s (sma syndicate_gmmode_lifebar_update_interval).
//=========================================================
#define GM_LIFEBAR_INTERVAL			0.25
#define GM_LIFEBAR_SCALE			0.2
#define GM_LIFEBAR_PLAYER_OFFSET	36.0	// sma GM_LIFEBAR_PLAYER_OFFSET
#define GM_LIFEBAR_MONSTER_OFFSET	45.0	// sma GM_LIFEBAR_MONSTER_OFFSET
#define GM_MAX_LIFEBARS				512
#define GM_MAX_EDICTS				2048	// com_model.h MAX_EDICTS: entity indices the clients can get

class CGmLifebar : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
};

LINK_ENTITY_TO_CLASS( gm_lifebar, CGmLifebar );

// its own model too (GmMode_Precache has it on co-op maps): a map that places the class elsewhere gets no
// unprecached SET_MODEL (Host_Error on stock engines); at run time the model is already in the list
void CGmLifebar::Precache( void )
{
	PRECACHE_MODEL( (char *)GM_LIFEBAR_SPRITE );
}

void CGmLifebar::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SET_MODEL( edict(), GM_LIFEBAR_SPRITE );
	pev->scale = GM_LIFEBAR_SCALE;
	pev->rendermode = kRenderNormal;
	pev->renderamt = 255;
	pev->frame = 99;
}

struct GmLifebar
{
	EHANDLE hBar;
	EHANDLE hTarget;
	float flHeight;
};

static GmLifebar s_Lifebars[GM_MAX_LIFEBARS];
static int s_iLifebarCount;
static unsigned char s_bIsLifebar[GM_MAX_EDICTS];	// by entity index: a bar (for AddToFullPack)
static unsigned char s_bHasLifebar[GM_MAX_EDICTS];	// by entity index: this target has a bar
static float s_flNextLifebarUpdate;

static void GmResetLifebars( void )
{
	memset( s_Lifebars, 0, sizeof( s_Lifebars ) );
	s_iLifebarCount = 0;
	memset( s_bIsLifebar, 0, sizeof( s_bIsLifebar ) );
	memset( s_bHasLifebar, 0, sizeof( s_bHasLifebar ) );
	s_flNextLifebarUpdate = 0;
}

static BOOL GmAnyActiveGameMaster( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients && i <= GM_MAX_PLAYERS; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pPlayer && !pPlayer->has_disconnected && pPlayer->m_bGameMaster && s_Gm[i].iState == GM_STATE_ACTIVE )
			return TRUE;
	}
	return FALSE;
}

static BOOL GmLifebarShouldSend( int iEntity, edict_t *pHost )
{
	if ( !s_iLifebarCount || iEntity < 0 || iEntity >= GM_MAX_EDICTS || !s_bIsLifebar[iEntity] )
		return TRUE;

	int iHost = ENTINDEX( pHost );
	if ( iHost < 1 || iHost > GM_MAX_PLAYERS )
		return FALSE;
	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( iHost );
	return pPlayer && pPlayer->m_bGameMaster && s_Gm[iHost].iState == GM_STATE_ACTIVE;
}

static void GmRemoveLifebar( int iSlot )
{
	GmLifebar &bar = s_Lifebars[iSlot];
	CBaseEntity *pBar = bar.hBar;
	CBaseEntity *pTarget = bar.hTarget;

	if ( pBar )
	{
		s_bIsLifebar[pBar->entindex()] = 0;
		UTIL_Remove( pBar );
	}
	if ( pTarget )
		s_bHasLifebar[pTarget->entindex()] = 0;

	s_Lifebars[iSlot] = s_Lifebars[--s_iLifebarCount];
}

static void GmAddLifebar( CBaseEntity *pTarget, float flHeight )
{
	if ( !s_bCatalogPrecached )
		return;

	int iTarget = pTarget->entindex();
	if ( s_iLifebarCount >= GM_MAX_LIFEBARS || iTarget >= GM_MAX_EDICTS || s_bHasLifebar[iTarget] || !GmEdictsLeft() )
		return;

	CBaseEntity *pBar = CBaseEntity::Create( (char *)"gm_lifebar", pTarget->pev->origin + Vector( 0, 0, flHeight ),
		g_vecZero, NULL );
	if ( !pBar )
		return;

	GmLifebar &bar = s_Lifebars[s_iLifebarCount++];
	bar.hBar = pBar;
	bar.hTarget = pTarget;
	bar.flHeight = flHeight;
	s_bIsLifebar[pBar->entindex()] = 1;
	s_bHasLifebar[iTarget] = 1;
}

// sma gm_update_lifebar_sprite
static void GmPlaceLifebar( GmLifebar &bar )
{
	CBaseEntity *pBar = bar.hBar;
	CBaseEntity *pTarget = bar.hTarget;
	float flHeight = bar.flHeight;

	if ( !pTarget->IsPlayer() )
	{
		// a turret on the ceiling (Spawn turns it over with EF_INVLIGHT): below it, away from the ceiling
		if ( IsTurretClass( STRING( pTarget->pev->classname ) ) && ( pTarget->pev->effects & EF_INVLIGHT ) )
			flHeight = ( pTarget->pev->absmin.z - pTarget->pev->origin.z ) - 64;

		// the big ones: over their head
		if ( FClassnameIs( pTarget->pev, "monster_cyborg" ) || FClassnameIs( pTarget->pev, "monster_cyberminotor" )
			|| FClassnameIs( pTarget->pev, "monster_gargantua" ) )
		{
			float flTop = ( pTarget->pev->absmax.z - pTarget->pev->origin.z ) + 12;
			if ( flTop > flHeight )
				flHeight = flTop;
		}
	}

	UTIL_SetOrigin( pBar->pev, pTarget->pev->origin + Vector( 0, 0, flHeight ) );

	float flHealth = pTarget->pev->health;
	float flMax = pTarget->pev->max_health;
	if ( flMax <= 0 )
		flMax = max( flHealth, 1.0f );

	// dead (or an observer: health 1, DEAD_RESPAWNABLE): no bar
	if ( flHealth <= 0 || pTarget->pev->deadflag >= DEAD_DYING )
	{
		pBar->pev->effects |= EF_NODRAW;
		return;
	}
	pBar->pev->effects &= ~EF_NODRAW;

	float flRatio = min( flHealth, flMax ) / flMax;
	pBar->pev->frame = max( flRatio * 100 - 1, 0.0f );
}

static void GmUpdateLifebars( void )
{
	int i;

	if ( gpGlobals->time < s_flNextLifebarUpdate )
		return;
	// sma syndicate_gmmode_lifebar_update_interval (0.05-2 s), syndicate_gmmode_lifebars 0: none
	s_flNextLifebarUpdate = gpGlobals->time
		+ min( 2.0f, max( 0.05f, GmPluginFloat( "syndicate_gmmode_lifebar_update_interval", GM_LIFEBAR_INTERVAL ) ) );

	if ( !GmAnyActiveGameMaster() || GmPluginFloat( "syndicate_gmmode_lifebars", 1 ) < 1 )
	{
		while ( s_iLifebarCount > 0 )
			GmRemoveLifebar( s_iLifebarCount - 1 );
		return;
	}

	// the targets that went away take their bars with them
	for ( i = 0; i < s_iLifebarCount; )
	{
		CBaseEntity *pTarget = s_Lifebars[i].hTarget;
		CBaseEntity *pBar = s_Lifebars[i].hBar;
		BOOL bGone = !pTarget || !pBar || ( pTarget->IsPlayer() && ( (CBasePlayer *)pTarget )->has_disconnected );
		if ( bGone )
			GmRemoveLifebar( i );
		else
			i++;
	}

	// every player, every Game Master's monster (the bars of new ones come at the next update, within 0.25 s)
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pPlayer && !pPlayer->has_disconnected && !FStringNull( pPlayer->pev->netname ) )
			GmAddLifebar( pPlayer, GM_LIFEBAR_PLAYER_OFFSET );
	}
	for ( i = gpGlobals->maxClients + 1; i < gpGlobals->maxEntities && i < GM_MAX_EDICTS; i++ )
	{
		edict_t *pEdict = INDEXENT( i );
		if ( !pEdict || pEdict->free || s_bHasLifebar[i] || !( pEdict->v.flags & FL_MONSTER ) )
			continue;
		if ( GmOwnedMonster( &pEdict->v, NULL ) && pEdict->v.deadflag == DEAD_NO )
			GmAddLifebar( CBaseEntity::Instance( pEdict ), GM_LIFEBAR_MONSTER_OFFSET );
	}

	for ( i = 0; i < s_iLifebarCount; i++ )
		GmPlaceLifebar( s_Lifebars[i] );
}

//=========================================================
// NPC selection and move orders. The plugin's selection API and its A* were experimental, so
// these are the game's own: the box is the Game Master's input read here (no selection entity, no edict scan of a
// selection API), and the move is a real schedule of the monster's AI (SCHED "GM Move": the path to the spot from
// BuildRoute: local move, triangulation, the node graph; then run), which leaves it the moment combat starts. The
// requirements are the plugin's (gm_orders.inc): a box selects up to 16 of the Game Master's own living NPCs (not
// Pitbulls, turrets or the Apache), Reload clears it, the secondary attack orders them to the ground under the
// crosshair (at most every 0.5 s), a GM-only glow on the selection, a ring at the goal, the route's first 16 legs and
// an arrow for the leg being walked; a new order replaces the old one; leaving, disconnecting or the round ending
// cancels orders; 120 s per order, 8 s without progress; "NPC cannot find a route" at most every 5 s per Game Master.
//=========================================================
#define GM_ORDER_MAX			64
#define GM_ORDER_INTERVAL		0.5		// between two orders of a Game Master
#define GM_ORDER_TIMEOUT		120.0
#define GM_ORDER_STALL			8.0
#define GM_ORDER_ARRIVED		48.0
#define GM_ORDER_FAIL_NOTICE	5.0
#define GM_MAX_ROUTE_BEAMS		16

static const int s_GmOrderColor[3] = { 80, 200, 255 };
static const int s_GmSelectColor[3] = { 166, 236, 138 };	// the design's selection glow (#a6ec8a)

// the plugin used the native idle-walk schedule (its interrupts) with the path to the spot; here a schedule of its own
static Task_t tlGmMove[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_IDLE_STAND	},
	{ TASK_GET_PATH_TO_SPOT,	(float)0				},	// m_vecMoveGoal
	{ TASK_RUN_PATH,			(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0				},
};

static Schedule_t slGmMove[] =
{
	{
		tlGmMove,
		ARRAYSIZE( tlGmMove ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND	|
		bits_COND_SMELL_FOOD	|
		bits_COND_SMELL			|
		bits_COND_PROVOKED,

		bits_SOUND_COMBAT		|
		bits_SOUND_MEAT			|
		bits_SOUND_CARCASS		|
		bits_SOUND_GARBAGE,
		"GM Move"
	},
};

enum
{
	GM_ORDER_MOVE = 0,		// 1 Move here (aim): to the spot, then over
	GM_ORDER_ATTACK,		// 2 Attack nearest player: that player as the enemy until he dies
	GM_ORDER_HUNT,			// 3 Hunt <player>
	GM_ORDER_HOLD,			// 4 Hold position: back to the spot when it has no enemy
};

struct GmOrder
{
	EHANDLE hMonster;
	int iOwner;				// the Game Master's player index
	int iOwnerUserId;
	Vector vecGoal;
	float flExpires;
	float flLastProgress;
	float flBestDist;
	BOOL bRouteShown;
	int iLastRouteIndex;
	EHANDLE hDoor;			// a door it opened and waits for
	float flDoorDeadline;
	int iType;				// GM_ORDER_*
	int iTarget;			// attack / hunt: the player's index
	float flNextRefresh;	// attack / hunt / hold / follow: the next check
};

static GmOrder s_Orders[GM_ORDER_MAX];
static float s_flNextOrderTick;

static void GmResetOrders( void )
{
	memset( s_Orders, 0, sizeof( s_Orders ) );
	s_flNextOrderTick = 0;
}

// a Game Master's own NPC that can take orders (sma gm_orders_owned)
static CBaseMonster *GmOrderable( CBasePlayer *pPlayer, CBaseEntity *pEntity )
{
	if ( !pEntity || pEntity->IsPlayer() || !( pEntity->pev->flags & FL_MONSTER ) || pEntity->pev->deadflag != DEAD_NO )
		return NULL;

	CBaseMonster *pMonster = pEntity->MyMonsterPointer();
	if ( !pMonster || (CBaseEntity *)pMonster->m_hGmOwner != pPlayer
		|| pMonster->m_iGmOwnerUserId != GETPLAYERUSERID( pPlayer->edict() ) )
		return NULL;

	// Pitbulls, turrets and the Apache: by the entry they were placed from and by what they are now
	if ( pMonster->m_iGmEntry >= 0 && pMonster->m_iGmEntry < GM_CATALOG_COUNT )
	{
		const char *pszClass = g_GmCatalog[pMonster->m_iGmEntry].pszClass;
		if ( FStrEq( pszClass, "monster_snark" ) || FStrEq( pszClass, "monster_apache" ) || IsTurretClass( pszClass ) )
			return NULL;
	}
	const char *pszNow = STRING( pMonster->pev->classname );
	if ( FStrEq( pszNow, "monster_snark" ) || FStrEq( pszNow, "monster_apache" ) || IsTurretClass( pszNow ) )
		return NULL;

	return pMonster;
}

static void GmBeam( CBasePlayer *pViewer, const Vector &vecStart, const Vector &vecEnd, int iLife, int iWidth, int iBrightness )
{
	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, pViewer->pev );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( vecStart.x );
		WRITE_COORD( vecStart.y );
		WRITE_COORD( vecStart.z );
		WRITE_COORD( vecEnd.x );
		WRITE_COORD( vecEnd.y );
		WRITE_COORD( vecEnd.z );
		WRITE_SHORT( s_iBeamSprite );
		WRITE_BYTE( 0 );	// start frame
		WRITE_BYTE( 0 );	// frame rate
		WRITE_BYTE( iLife );
		WRITE_BYTE( iWidth );
		WRITE_BYTE( 0 );	// noise
		WRITE_BYTE( s_GmOrderColor[0] );
		WRITE_BYTE( s_GmOrderColor[1] );
		WRITE_BYTE( s_GmOrderColor[2] );
		WRITE_BYTE( iBrightness );
		WRITE_BYTE( 0 );	// speed
	MESSAGE_END();
}

// sma gm_orders_arrow: a line from the NPC to the leg's end and two strokes for the head
static void GmArrow( CBasePlayer *pViewer, const Vector &vecFrom, const Vector &vecTo )
{
	Vector vecDir = vecTo - vecFrom;
	vecDir.z = 0;
	float flLen = vecDir.Length();
	if ( flLen < 1 )
		return;
	vecDir = vecDir / flLen;

	Vector vecStart = vecFrom + Vector( 0, 0, 12 );
	Vector vecEnd = vecTo + Vector( 0, 0, 12 );
	GmBeam( pViewer, vecStart, vecEnd, 20, 3, 200 );
	for ( int iSide = -1; iSide <= 1; iSide += 2 )
	{
		Vector vecTip( vecEnd.x - vecDir.x * 24 - vecDir.y * 12 * iSide, vecEnd.y - vecDir.y * 24 + vecDir.x * 12 * iSide, vecEnd.z );
		GmBeam( pViewer, vecTip, vecEnd, 20, 3, 200 );
	}
}

// sma gm_orders_destination_ring: a ring expanding to 32 units in 0.5 s
static void GmRing( CBasePlayer *pViewer, const Vector &vecGoal )
{
	MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, pViewer->pev );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( vecGoal.x );
		WRITE_COORD( vecGoal.y );
		WRITE_COORD( vecGoal.z + 2 );
		WRITE_COORD( vecGoal.x );
		WRITE_COORD( vecGoal.y );
		WRITE_COORD( vecGoal.z + 2 + 64 );
		WRITE_SHORT( s_iBeamSprite );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 5 );	// 0.5 s
		WRITE_BYTE( 8 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( s_GmOrderColor[0] );
		WRITE_BYTE( s_GmOrderColor[1] );
		WRITE_BYTE( s_GmOrderColor[2] );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 0 );
	MESSAGE_END();
}

static void GmClearSelection( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	for ( int i = 0; i < GM_SELECT_MAX; i++ )
		gm.hSelected[i] = NULL;
	gm.iSelected = 0;
	gm.bSelecting = FALSE;
}

// the selection without the NPCs that died, went or changed hands
static void GmPruneSelection( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	int iKeep = 0;
	for ( int i = 0; i < gm.iSelected; i++ )
	{
		CBaseEntity *pEntity = gm.hSelected[i];
		if ( GmSelectable( pPlayer, pEntity ) )
			gm.hSelected[iKeep++] = pEntity;
	}
	for ( int i = iKeep; i < GM_SELECT_MAX; i++ )
		gm.hSelected[i] = NULL;
	gm.iSelected = iKeep;
}

static Vector GmCursor( CBasePlayer *pPlayer, IGNORE_MONSTERS igmon, TraceResult *ptr )
{
	TraceResult tr;
	Vector vecStart = GmEyePosition( pPlayer );
	UTIL_MakeVectors( pPlayer->pev->v_angle );
	UTIL_TraceLine( vecStart, vecStart + gpGlobals->v_forward * 8192, igmon, pPlayer->edict(), &tr );
	if ( ptr )
		*ptr = tr;
	return tr.vecEndPos;
}

static void GmCancelOrder( int iSlot, BOOL bStop )
{
	GmOrder &order = s_Orders[iSlot];
	CBaseEntity *pEntity = order.hMonster;
	CBaseMonster *pMonster = pEntity ? pEntity->MyMonsterPointer() : NULL;

	// still walking our order: stand (sma gm_nav_stop: the idle schedule)
	if ( bStop && pMonster && pMonster->m_pSchedule == slGmMove && pMonster->pev->deadflag == DEAD_NO )
	{
		pMonster->RouteClear();
		pMonster->ChangeSchedule( pMonster->GetScheduleOfType( SCHED_IDLE_STAND ) );
	}
	memset( &order, 0, sizeof( order ) );
}

static void GmCancelOrders( int iOwner )
{
	for ( int i = 0; i < GM_ORDER_MAX; i++ )
	{
		if ( (CBaseEntity *)s_Orders[i].hMonster && ( iOwner == 0 || s_Orders[i].iOwner == iOwner ) )
			GmCancelOrder( i, TRUE );
	}
}

static void GmOrderFailed( CBasePlayer *pPlayer, const char *pszReason )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];

	ALERT( at_aiconsole, "GM order: %s\n", pszReason );
	if ( gpGlobals->time < gm.flNextFailNotice )
		return;
	gm.flNextFailNotice = gpGlobals->time + GM_ORDER_FAIL_NOTICE;
	GmPrint( pPlayer, "NPC cannot find a route" );
}

static void GmIssueOrdersTo( CBasePlayer *pPlayer, Vector vecGoal );

static void GmIssueOrders( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	TraceResult tr;

	GmPruneSelection( pPlayer );
	if ( !gm.iSelected )
		return;

	// the ground under the crosshair (not a wall, a steep slope or the sky)
	Vector vecGoal = GmCursor( pPlayer, ignore_monsters, &tr );
	if ( tr.fStartSolid || tr.fAllSolid || tr.flFraction >= 1.0 || tr.vecPlaneNormal.z < 0.7
		|| UTIL_PointContents( vecGoal ) == CONTENTS_SKY )
		return;
	GmIssueOrdersTo( pPlayer, vecGoal );
}

// the players window's map: a right click at (x, y) on the level shown, whose top is zTop: the floor under it
static void GmIssueOrdersAt( CBasePlayer *pPlayer, float x, float y, float zTop )
{
	TraceResult tr;
	UTIL_TraceLine( Vector( x, y, zTop ), Vector( x, y, -8192 ), ignore_monsters, pPlayer->edict(), &tr );
	if ( tr.fAllSolid || tr.flFraction >= 1.0 || tr.vecPlaneNormal.z < 0.7 || UTIL_PointContents( tr.vecEndPos ) == CONTENTS_SKY )
	{
		GmPrint( pPlayer, "No floor there" );
		return;
	}
	if ( tr.fStartSolid )
	{
		// the point starts in a wall or a ceiling: from just under the hit instead
		UTIL_TraceLine( tr.vecEndPos - Vector( 0, 0, 2 ), Vector( x, y, -8192 ), ignore_monsters, pPlayer->edict(), &tr );
		if ( tr.fStartSolid || tr.flFraction >= 1.0 || tr.vecPlaneNormal.z < 0.7 )
		{
			GmPrint( pPlayer, "No floor there" );
			return;
		}
	}
	GmIssueOrdersTo( pPlayer, tr.vecEndPos );
}

// a move order for the selection to a floor point
static void GmIssueOrdersTo( CBasePlayer *pPlayer, Vector vecGoal )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	GmPruneSelection( pPlayer );
	if ( !gm.iSelected )
		return;
	vecGoal.z += 1;

	int iMoved = 0;
	for ( int i = 0; i < gm.iSelected; i++ )
	{
		CBaseMonster *pMonster = GmOrderable( pPlayer, gm.hSelected[i] );
		// ground NPCs only (aircraft and turrets have other AI), not in a scripted sequence
		if ( !pMonster || pMonster->pev->movetype != MOVETYPE_STEP || pMonster->m_pCine
			|| pMonster->m_MonsterState == MONSTERSTATE_SCRIPT )
			continue;

		// a new order replaces the old one
		int iSlot = -1;
		for ( int j = 0; j < GM_ORDER_MAX; j++ )
		{
			CBaseEntity *pOrdered = s_Orders[j].hMonster;
			if ( pOrdered == pMonster )
			{
				iSlot = j;
				break;
			}
			if ( !pOrdered && iSlot == -1 )
				iSlot = j;
		}
		if ( iSlot == -1 )
		{
			ALERT( at_aiconsole, "GM order: too many orders\n" );
			continue;
		}

		GmOrder &order = s_Orders[iSlot];
		memset( &order, 0, sizeof( order ) );
		order.hMonster = pMonster;
		order.iOwner = pPlayer->entindex();
		order.iOwnerUserId = GETPLAYERUSERID( pPlayer->edict() );
		order.vecGoal = vecGoal - Vector( 0, 0, pMonster->pev->mins.z );	// the crosshair is a floor point, routes use origins
		order.flExpires = gpGlobals->time + GM_ORDER_TIMEOUT;
		order.flLastProgress = gpGlobals->time;
		order.flBestDist = ( order.vecGoal - pMonster->pev->origin ).Length();
		order.iLastRouteIndex = -1;

		pMonster->m_vecMoveGoal = order.vecGoal;
		pMonster->ChangeSchedule( slGmMove );
		iMoved++;
	}

	if ( iMoved )
		GmRing( pPlayer, vecGoal );
}

// every 0.1 s: progress, the route lines, the end of an order
static void GmUpdateOrders( void )
{
	if ( gpGlobals->time < s_flNextOrderTick )
		return;
	s_flNextOrderTick = gpGlobals->time + 0.1;

	for ( int i = 0; i < GM_ORDER_MAX; i++ )
	{
		GmOrder &order = s_Orders[i];
		CBaseEntity *pEntity = order.hMonster;
		if ( !pEntity )
			continue;

		CBasePlayer *pOwner = (CBasePlayer *)UTIL_PlayerByIndex( order.iOwner );
		if ( !pOwner || pOwner->has_disconnected || !pOwner->m_bGameMaster
			|| GETPLAYERUSERID( pOwner->edict() ) != order.iOwnerUserId || s_Gm[order.iOwner].iState != GM_STATE_ACTIVE )
		{
			GmCancelOrder( i, TRUE );
			continue;
		}

		CBaseMonster *pMonster = GmOrderable( pOwner, pEntity );
		if ( !pMonster )
		{
			memset( &order, 0, sizeof( order ) );
			continue;
		}

		// attack / hunt / hold / follow: kept until the target is gone, a new order or the round end
		if ( order.iType != GM_ORDER_MOVE )
		{
			if ( !GmPersistentUpdate( order, pOwner, pMonster ) )
				memset( &order, 0, sizeof( order ) );
			continue;
		}

		float flDist = ( order.vecGoal - pMonster->pev->origin ).Length2D();

		// the schedule is over: arrived, or something else took over (combat interrupts, as in the plugin), or the
		// AI found no way (the path to the spot failed)
		if ( pMonster->m_pSchedule != slGmMove )
		{
			if ( flDist > GM_ORDER_ARRIVED && pMonster->m_hEnemy == NULL
				&& !pMonster->HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY ) )
				GmOrderFailed( pOwner, "no route" );
			memset( &order, 0, sizeof( order ) );
			continue;
		}

		if ( gpGlobals->time > order.flExpires )
		{
			GmOrderFailed( pOwner, "order timed out" );
			GmCancelOrder( i, TRUE );
			continue;
		}

		if ( flDist < order.flBestDist - 4 )
		{
			order.flBestDist = flDist;
			order.flLastProgress = gpGlobals->time;
		}
		else if ( gpGlobals->time - order.flLastProgress > GM_ORDER_STALL )
		{
			GmOrderFailed( pOwner, "movement blocked" );
			GmCancelOrder( i, TRUE );
			continue;
		}

		// the route the monster will walk (its own, not a planner's guess): the first 16 legs once, then an arrow for
		// the leg being walked whenever it moves on to the next
		if ( pMonster->m_movementGoal == MOVEGOAL_LOCATION && pMonster->m_Route[pMonster->m_iRouteIndex].iType )
		{
			if ( !order.bRouteShown )
			{
				order.bRouteShown = TRUE;
				Vector vecPrev = pMonster->pev->origin;
				for ( int j = pMonster->m_iRouteIndex, n = 0; j < ROUTE_SIZE && n < GM_MAX_ROUTE_BEAMS; j++, n++ )
				{
					if ( !pMonster->m_Route[j].iType )
						break;
					GmBeam( pOwner, vecPrev + Vector( 0, 0, 12 ), pMonster->m_Route[j].vecLocation + Vector( 0, 0, 12 ), 20, 3, 200 );
					vecPrev = pMonster->m_Route[j].vecLocation;
				}
			}
			if ( order.iLastRouteIndex != pMonster->m_iRouteIndex )
			{
				order.iLastRouteIndex = pMonster->m_iRouteIndex;
				GmArrow( pOwner, pMonster->pev->origin, pMonster->m_Route[pMonster->m_iRouteIndex].vecLocation );
			}
		}
	}
}

// Without a placement (the default keys): Fire held and dragged draws the box on the client's screen
// (gm_select), Alt Fire sends the selected monsters to the floor under the crosshair (a move order), Reload clears
// the selection
static void GmSelectionThink( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	int iButtons = pPlayer->pev->button;

	// a button still held from a placement (or the window) does nothing until let go
	if ( gm.bOrderWaitRelease )
	{
		if ( iButtons & ( IN_ATTACK | IN_ATTACK2 | IN_RELOAD ) )
			return;
		gm.bOrderWaitRelease = FALSE;
	}

	if ( ( pPlayer->m_afButtonPressed & IN_RELOAD ) && gm.iSelected )
	{
		GmClearSelection( pPlayer );
		gm.flNextHud = 0;
		if ( gm.iMenu == GM_MENU_ORDERS || gm.iMenu == GM_MENU_HUNT )
			GmQuickMenu( pPlayer );	// the orders are over: back to the quick menu
	}

	if ( ( pPlayer->m_afButtonPressed & IN_ATTACK2 ) && gm.iSelected && gpGlobals->time >= gm.flNextOrder )
	{
		gm.flNextOrder = gpGlobals->time + GM_ORDER_INTERVAL;
		GmIssueOrders( pPlayer );
	}
}

//=========================================================
// Doors on an order's way (sma gm_order_doors.inc, as requirements): a monster under a Game Master's order may open
// an ordinary door - func_door / func_door_rotating, no name (no button or trigger opens it), solid, not damage-
// activated, not start-open, use-only, no-monsters or client-only, its master triggered for the monster - that is
// closed: through the node graph (the order routes with the door capabilities: CBaseMonster::RouteCapability, and
// OpenDoorAndWait on the link as for any monster that can open doors) or when its local move runs into one within 96
// units. It waits up to 8 s for the door to open, then the order fails. Other monsters keep their own door behavior;
// a door the rules don't allow stays closed (OpenDoorAndWait does nothing for it, as the plugin's hook did).
//=========================================================
#define GM_DOOR_REACH			96.0
#define GM_DOOR_WAIT			8.0
#define GM_SF_DOOR_TOUCH_ONLY_CLIENTS	1024	// the SDK's flag (SBO's doors don't read it; the plugin excluded it)

BOOL GmMode_IsOrdered( CBaseMonster *pMonster )
{
	return pMonster && pMonster->m_pSchedule == slGmMove;
}

int GmMode_RouteCapability( CBaseMonster *pMonster, int afCapability )
{
	if ( GmMode_IsOrdered( pMonster ) )
		return afCapability | bits_CAP_OPEN_DOORS | bits_CAP_AUTO_DOORS;
	return afCapability;
}

// sma gm_nav_door_allowed
BOOL GmMode_DoorAllowed( CBaseMonster *pMonster, CBaseEntity *pDoor )
{
	if ( !pDoor || !( FClassnameIs( pDoor->pev, "func_door" ) || FClassnameIs( pDoor->pev, "func_door_rotating" ) ) )
		return FALSE;
	if ( pDoor->pev->spawnflags & ( SF_DOOR_START_OPEN | SF_DOOR_USE_ONLY | SF_DOOR_NOMONSTERS | GM_SF_DOOR_TOUCH_ONLY_CLIENTS ) )
		return FALSE;
	if ( !FStringNull( pDoor->pev->targetname ) || pDoor->pev->solid != SOLID_BSP || pDoor->pev->takedamage != DAMAGE_NO )
		return FALSE;

	CBaseToggle *pToggle = (CBaseToggle *)pDoor;
	return FStringNull( pToggle->m_sMaster ) || UTIL_IsMasterTriggered( pToggle->m_sMaster, pMonster );
}

// CBaseMonster::OpenDoorAndWait: under an order, only an allowed door that is closed (a toggle door already open is
// not used again: it would close)
BOOL GmMode_MayOpenDoor( CBaseMonster *pMonster, CBaseEntity *pDoor )
{
	if ( !GmMode_IsOrdered( pMonster ) )
		return TRUE;
	return GmMode_DoorAllowed( pMonster, pDoor ) && pDoor->GetToggleState() == TS_AT_BOTTOM;
}

// CBaseMonster::Move: the local move is blocked by pBlocker. TRUE: a door of the order is opening (the monster waits).
BOOL GmMode_OpenDoorInWay( CBaseMonster *pMonster, CBaseEntity *pBlocker )
{
	if ( !GmMode_IsOrdered( pMonster ) || !pBlocker )
		return FALSE;

	GmOrder *pOrder = NULL;
	int iSlot;
	for ( iSlot = 0; iSlot < GM_ORDER_MAX; iSlot++ )
	{
		if ( (CBaseEntity *)s_Orders[iSlot].hMonster == pMonster )
		{
			pOrder = &s_Orders[iSlot];
			break;
		}
	}
	if ( !pOrder )
		return FALSE;

	// the door this order is already waiting for
	if ( (CBaseEntity *)pOrder->hDoor == pBlocker )
	{
		if ( gpGlobals->time > pOrder->flDoorDeadline )
		{
			CBasePlayer *pOwner = (CBasePlayer *)UTIL_PlayerByIndex( pOrder->iOwner );
			if ( pOwner )
				GmOrderFailed( pOwner, "door did not open in time" );
			GmCancelOrder( iSlot, TRUE );
			return TRUE;
		}
		pOrder->flLastProgress = gpGlobals->time;	// waiting for a door is no stall
		pMonster->m_flMoveWaitFinished = gpGlobals->time + 0.25;
		return TRUE;
	}

	if ( !GmMode_DoorAllowed( pMonster, pBlocker ) || pBlocker->GetToggleState() != TS_AT_BOTTOM )
		return FALSE;

	// within reach: the nearest point of the door's bounds (wide and rotating doors too)
	Vector vecPoint;
	for ( int i = 0; i < 3; i++ )
		vecPoint[i] = max( pBlocker->pev->absmin[i] + 1, min( pMonster->pev->origin[i], pBlocker->pev->absmax[i] - 1 ) );
	if ( ( vecPoint - pMonster->pev->origin ).Length() > GM_DOOR_REACH )
		return FALSE;

	ALERT( at_aiconsole, "GM order: %s opens door %d\n", STRING( pMonster->pev->classname ), pBlocker->entindex() );
	pOrder->hDoor = pBlocker;
	pOrder->flDoorDeadline = gpGlobals->time + GM_DOOR_WAIT;
	pOrder->flLastProgress = gpGlobals->time;
	// the door's own Use checks its master again, with the monster as the activator
	pBlocker->Use( pMonster, pMonster, USE_ON, 0 );
	pMonster->m_flMoveWaitFinished = gpGlobals->time + 0.25;
	return TRUE;
}

// AddToFullPack, after the state is filled: the GM's selected NPCs glow for that GM only (sma gm_orders_highlight)
void GmMode_AddToFullPack( struct entity_state_s *state, int e, edict_t *host )
{
	int iHost = ENTINDEX( host );
	if ( iHost < 1 || iHost > GM_MAX_PLAYERS )
		return;

	GmPlayerState &gm = s_Gm[iHost];
	if ( !gm.iSelected || gm.iState != GM_STATE_ACTIVE || gm.iPlace != GM_PLACE_NONE )
		return;

	for ( int i = 0; i < gm.iSelected; i++ )
	{
		CBaseEntity *pEntity = gm.hSelected[i];
		if ( pEntity && pEntity->entindex() == e && pEntity->pev->deadflag == DEAD_NO )
		{
			state->renderfx = kRenderFxGlowShell;
			state->rendermode = kRenderNormal;
			state->renderamt = 16;
			state->rendercolor.r = s_GmSelectColor[0];
			state->rendercolor.g = s_GmSelectColor[1];
			state->rendercolor.b = s_GmSelectColor[2];
			return;
		}
	}
}

// CCoop::RoundWinner: the round is over: orders go, selections stay (sma gm_orders_round_end)
void GmMode_RoundEnd( void )
{
	GmCancelOrders( 0 );
	for ( int i = 1; i <= GM_MAX_PLAYERS; i++ )
		s_Gm[i].bSelecting = FALSE;
}

//=========================================================
// Think (CBasePlayer::PreThink, observer branch)
//=========================================================
void GmMode_Think( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];

	pPlayer->pev->iuser4 |= GM_IUSER4_BIT;

	if ( gm.iState != GM_STATE_ACTIVE )
	{
		if ( pPlayer->pev->iuser1 != OBS_ROAMING )
			GmForceRoaming( pPlayer );
		pPlayer->pev->flags |= FL_FROZEN;
		return;
	}

	GmUpdateHud( pPlayer );

	// a spectator mode (the players window, quick menu 5, Jump): the spectator keys, no placing or orders
	if ( GmFollowing( pPlayer ) )
	{
		GmFollowThink( pPlayer );
		return;
	}
	if ( pPlayer->pev->iuser1 != OBS_ROAMING )
		GmForceRoaming( pPlayer );

	if ( gm.iPlace == GM_PLACE_NONE )
	{
		// Jump: the next spectator mode, as for any spectator (free flying -> the free overview)
		if ( ( pPlayer->m_afButtonPressed & IN_JUMP ) && pPlayer->m_flNextObserverInput <= gpGlobals->time )
		{
			pPlayer->m_flNextObserverInput = gpGlobals->time + 0.2;
			GmShowMenu( pPlayer, GM_MENU_NONE, 0, "" );
			pPlayer->Observer_SetMode( OBS_MAP_FREE );
			return;
		}
		GmSelectionThink( pPlayer );
		return;
	}

	int iButtons = pPlayer->pev->button;

	if ( pPlayer->m_afButtonPressed & IN_RELOAD )
	{
		// cancel (Reload, or Esc: gm_cancel): back to the quick menu, not the spawn window
		GmBackToQuickMenu( pPlayer );
		return;
	}

	if ( gm.iPlace == GM_PLACE_MONSTER || gm.iPlace == GM_PLACE_WEAPON || gm.iPlace == GM_PLACE_TELEPORTER )
	{
		GmUpdatePreview( pPlayer );

		// held Alt Fire turns it (sma PLACEMENT_ROTATE_STEP)
		if ( iButtons & IN_ATTACK2 )
		{
			gm.flYawOffset = NormalizeAngle( gm.flYawOffset + GM_ROTATE_STEP );
			GmUpdatePreviewAngles( pPlayer );
		}
	}

	if ( iButtons & IN_ATTACK )
	{
		if ( !gm.bAttackHeld )
		{
			gm.bAttackHeld = TRUE;
			if ( gm.iPlace == GM_PLACE_MONSTER )
				GmSpawnMonster( pPlayer );
			else if ( gm.iPlace == GM_PLACE_WEAPON )
				GmPlaceWeapon( pPlayer );
			else if ( gm.iPlace == GM_PLACE_TELEPORTER )
				GmPlaceTeleporter( pPlayer );
		}
	}
	else
		gm.bAttackHeld = FALSE;
}

void GmMode_UpdateClientData( CBasePlayer *pPlayer, struct clientdata_s *cd )
{
	// the Game Master's roaming speed for the client's prediction (pm_shared; delta.lst: iuser4 has 2 bits)
	cd->iuser4 = ( pPlayer->m_bGameMaster && ( pPlayer->pev->iuser4 & GM_IUSER4_BIT ) ) ? GM_IUSER4_BIT : 0;
}

//=========================================================
// Commands
//=========================================================
static void GmGotoPlayer( CBasePlayer *pPlayer, int iTarget )
{
	CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( iTarget );

	if ( !pTarget || pTarget == pPlayer || pTarget->has_disconnected || !pTarget->IsAlive() || pTarget->IsObserver()
		|| pTarget->m_bOutOfPlay || pTarget->m_bGameMaster )
	{
		GmPrint( pPlayer, "Selected player is not available" );
		return;
	}

	// sma teleport_observer_to_player: the target's position, view and velocity
	GmForceRoaming( pPlayer );
	pPlayer->pev->velocity = pTarget->pev->velocity;
	pPlayer->pev->basevelocity = pTarget->pev->velocity;
	UTIL_SetOrigin( pPlayer->pev, pTarget->pev->origin + pTarget->pev->view_ofs );
	pPlayer->pev->v_angle = pTarget->pev->v_angle;
	pPlayer->pev->angles = pTarget->pev->v_angle;
	pPlayer->pev->fixangle = TRUE;

	char szText[GM_NOTICE_LEN];
	_snprintf( szText, sizeof( szText ), "Teleported near %s", STRING( pTarget->pev->netname ) );
	szText[sizeof( szText ) - 1] = 0;
	GmPrint( pPlayer, szText );
}

// a pick: the spawn window's gm_pick, the quick menu's "Spawn again", a classic list's row
static void GmPick( CBasePlayer *pPlayer, int iEntry, int iVariant )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	if ( iEntry < 0 || iEntry >= GM_CATALOG_COUNT || gm.iState != GM_STATE_ACTIVE )
		return;

	GmShowMenu( pPlayer, GM_MENU_NONE, 0, "" );
	GmStopFollowing( pPlayer );

	const GmCatalogEntry &entry = g_GmCatalog[iEntry];
	switch ( entry.iKind )
	{
	case GM_KIND_MONSTER:
	{
		int iCount;
		const GmVariant *pVariants = GmCatalog_Variants( entry.iVariants, iCount );
		if ( pVariants && ( iVariant < 0 || iVariant >= iCount ) )
			return;	// a monster with loadouts needs one
		if ( !pVariants )
			iVariant = -1;
		if ( !GmAskPlace( pPlayer, GM_PICK( iEntry, iVariant ) ) )
		{
			GmBackToQuickMenu( pPlayer );
			return;
		}
		gm.iLastEntry = iEntry;
		gm.iLastVariant = iVariant;
		GmStartMonsterPlacement( pPlayer, iEntry, iVariant );
		break;
	}
	case GM_KIND_WEAPON:
		if ( !GmAskPlace( pPlayer, GM_PICK( iEntry, -1 ) ) )
		{
			GmBackToQuickMenu( pPlayer );
			return;
		}
		GmStartWeaponPlacement( pPlayer, iEntry );
		break;
	case GM_KIND_TOOL:
		if ( entry.iTool == GM_TOOL_TELEPORTERS )
			GmTpAction( pPlayer, "place", 0, 0 );
		break;
	}
}

BOOL GmMode_ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if ( strncmp( pcmd, "gm", 2 ) )
		return FALSE;

	// not mp_gmmode: a Game Master stays one (and can leave) if the cvar goes off during the map
	BOOL bGameMaster = m_Mode == MODE_COOP && pPlayer->m_bGameMaster && GmEngage == 1;
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];

	if ( FStrEq( pcmd, "gm" ) )
	{
		// the intro (or help) window was closed; an old client's "Return" is harmless
		if ( bGameMaster )
			GmIntroClosed( pPlayer );
		return TRUE;
	}

	if ( FStrEq( pcmd, "gm_pick" ) )
	{
		// gm_pick <entry> [<loadout>]
		if ( !bGameMaster || gm.iState != GM_STATE_ACTIVE || CMD_ARGC() < 2 )
			return TRUE;
		int iEntry = atoi( CMD_ARGV( 1 ) );
		int iVariant = ( CMD_ARGC() >= 3 ) ? atoi( CMD_ARGV( 2 ) ) : -1;
		GmPick( pPlayer, iEntry, iVariant );
		return TRUE;
	}

	// the GM key (gmon on a Game Master's client): a GM text menu up -> the spawn window ("K K"), else the quick menu
	if ( FStrEq( pcmd, "gm_key" ) )
	{
		if ( !bGameMaster || gm.iState != GM_STATE_ACTIVE )
			return TRUE;
		// the orders menu: "K back to quick menu"; another GM menu up: "K K" the spawn window; else the quick menu
		if ( ( gm.iMenu == GM_MENU_ORDERS || gm.iMenu == GM_MENU_HUNT ) && pPlayer->m_iTextMenu == TEXTMENU_GM )
			GmQuickMenu( pPlayer );
		else if ( gm.iMenu != GM_MENU_NONE && pPlayer->m_iTextMenu == TEXTMENU_GM )
		{
			GmShowMenu( pPlayer, GM_MENU_NONE, 0, "" );
			GmSendState( pPlayer, GM_EVENT_OPEN_MENU );
		}
		else if ( gm.iSelected )
			GmOrdersMenu( pPlayer );	// something selected: its orders menu
		else
			GmQuickMenu( pPlayer );
		return TRUE;
	}

	// /gmspawn, /gmmenu (sma: the main menu) and the quick menu's own command
	// the plugin's gm_spawnmenu / gm_menu / gmmenu (its root menu): the quick menu
	if ( FStrEq( pcmd, "gm_quick" ) || FStrEq( pcmd, "gm_spawnmenu" ) || FStrEq( pcmd, "gm_menu" ) || FStrEq( pcmd, "gmmenu" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE )
			GmQuickMenu( pPlayer );
		return TRUE;
	}

	// classic menus: the windows' text menus ("gm_textmenu spawn [tab] | players")
	if ( FStrEq( pcmd, "gm_textmenu" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE && CMD_ARGC() >= 2 )
			GmTextMenuCommand( pPlayer, CMD_ARGV( 1 ), ( CMD_ARGC() >= 3 ) ? atoi( CMD_ARGV( 2 ) ) : -1 );
		return TRUE;
	}

	// gm_select <x0> <y0> <x1> <y1> <tanx> <tany>: the client's box (Fire drag)
	if ( FStrEq( pcmd, "gm_select" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE && gm.iPlace == GM_PLACE_NONE && !GmFollowing( pPlayer )
			&& CMD_ARGC() >= 7 )
		{
			float v[6];
			for ( int i = 0; i < 6; i++ )
				v[i] = atof( CMD_ARGV( i + 1 ) );
			if ( v[4] > 0.05 && v[4] < 20 && v[5] > 0.05 && v[5] < 20 )
				GmSelectScreen( pPlayer, max( -2.0f, min( 2.0f, v[0] ) ), max( -2.0f, min( 2.0f, v[1] ) ),
					max( -2.0f, min( 2.0f, v[2] ) ), max( -2.0f, min( 2.0f, v[3] ) ), v[4], v[5], 8192 );
		}
		return TRUE;
	}

	// the players window (new in-game menus) or its text menu (classic): /gmplayers, the budget plugin
	if ( FStrEq( pcmd, "gm_players" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE )
			GmSendState( pPlayer, GM_EVENT_OPEN_PLAYERS );
		return TRUE;
	}

	// gm_follow <player> <0 chase | 1 first person>, gm_spawnnear <player> (the players window)
	if ( FStrEq( pcmd, "gm_follow" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE && CMD_ARGC() >= 2 )
			GmFollow( pPlayer, atoi( CMD_ARGV( 1 ) ), ( CMD_ARGC() >= 3 && atoi( CMD_ARGV( 2 ) ) ) ? OBS_IN_EYE : OBS_CHASE_LOCKED );
		return TRUE;
	}
	// the players window: "gm_mapwatch 1|0" its map wants this Game Master's monsters; "gm_pip <player>" its View;
	// "gm_mapselect <entity>..." a click or a box on its map; "gm_mapmove <x> <y> <top of the level>" a right click
	if ( FStrEq( pcmd, "gm_mapwatch" ) )
	{
		gm.bMapWatch = bGameMaster && atoi( CMD_ARGV( 1 ) ) != 0;
		gm.flNextHud = 0;
		return TRUE;
	}
	if ( FStrEq( pcmd, "gm_pip" ) )
	{
		const int i = atoi( CMD_ARGV( 1 ) );
		gm.iPipTarget = ( bGameMaster && i >= 1 && i <= gpGlobals->maxClients ) ? i : 0;
		return TRUE;
	}
	if ( FStrEq( pcmd, "gm_mapselect" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE )
		{
			GmClearSelection( pPlayer );
			for ( int a = 1; a < CMD_ARGC() && gm.iSelected < GM_SELECT_MAX; a++ )
			{
				const int e = atoi( CMD_ARGV( a ) );
				if ( e <= gpGlobals->maxClients || e >= gpGlobals->maxEntities )
					continue;
				edict_t *pent = INDEXENT( e );
				CBaseEntity *pEntity = ( pent && !pent->free ) ? CBaseEntity::Instance( pent ) : NULL;
				if ( GmSelectable( pPlayer, pEntity ) )
					gm.hSelected[gm.iSelected++] = pEntity;
			}
			gm.flNextHud = 0;
			if ( gm.iSelected )
				GmOrdersMenu( pPlayer );
		}
		return TRUE;
	}
	if ( FStrEq( pcmd, "gm_mapmove" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE && CMD_ARGC() >= 4 )
			GmIssueOrdersAt( pPlayer, atof( CMD_ARGV( 1 ) ), atof( CMD_ARGV( 2 ) ), atof( CMD_ARGV( 3 ) ) );
		return TRUE;
	}

	if ( FStrEq( pcmd, "gm_spawnnear" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE && CMD_ARGC() >= 2 )
			GmSpawnNear( pPlayer, atoi( CMD_ARGV( 1 ) ) );
		return TRUE;
	}

	// the client's text menu went away (another menu took the screen, Esc): no GM menu on screen any more
	if ( FStrEq( pcmd, "gm_menuclosed" ) )
	{
		if ( bGameMaster && pPlayer->m_iTextMenu == TEXTMENU_GM )
		{
			gm.iMenu = GM_MENU_NONE;
			pPlayer->m_iTextMenu = TEXTMENU_NONE;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "gm_cancel" ) )
	{
		// the window opened during a placement: the placement ends (sma: opening the menu clears the preview); Esc
		// while placing ("gm_cancel quick"): back to the quick menu, as Reload
		if ( bGameMaster && FStrEq( CMD_ARGV( 1 ), "quick" ) && s_Gm[pPlayer->entindex()].iPlace != GM_PLACE_NONE )
			GmBackToQuickMenu( pPlayer );
		else if ( bGameMaster )
			GmEndPlacement( pPlayer, TRUE );
		return TRUE;
	}

	if ( FStrEq( pcmd, "gm_tp" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE && CMD_ARGC() >= 2 )
			GmTeleporterCommand( pPlayer );
		return TRUE;
	}

	if ( FStrEq( pcmd, "gm_goto" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE && CMD_ARGC() >= 2 )
		{
			GmStopFollowing( pPlayer );
			GmGotoPlayer( pPlayer, atoi( CMD_ARGV( 1 ) ) );
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "gm_help" ) )
	{
		if ( bGameMaster && gm.iState == GM_STATE_ACTIVE )
		{
			GmEndPlacement( pPlayer, TRUE );
			ShowVGUI( pPlayer, 19 );
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "gm_leave" ) )
	{
		if ( bGameMaster && m_Mode == MODE_COOP )
			( (CCoop *)g_pGameRules )->GameMasterOff( pPlayer, FALSE );
		return TRUE;
	}

	return FALSE;
}

// A player by "#<userid>", the whole name or a unique part of it (any case); iMatches: how many matched
static CBasePlayer *GmFindPlayer( const char *pszArg, int &iMatches )
{
	int iUserId = ( pszArg[0] == '#' ) ? atoi( pszArg + 1 ) : -1;
	CBasePlayer *pFound = NULL;
	iMatches = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pPlayer || pPlayer->has_disconnected || FStringNull( pPlayer->pev->netname ) )
			continue;

		if ( iUserId >= 0 )
		{
			if ( GETPLAYERUSERID( pPlayer->edict() ) == iUserId )
			{
				pFound = pPlayer;
				iMatches = 1;
				break;
			}
			continue;
		}

		const char *pszName = STRING( pPlayer->pev->netname );
		if ( !stricmp( pszName, pszArg ) )
		{
			pFound = pPlayer;
			iMatches = 1;
			break;
		}

		// a part of the name, any case
		int iLen = strlen( pszArg );
		for ( const char *p = pszName; *p; p++ )
		{
			if ( !strnicmp( p, pszArg, iLen ) )
			{
				pFound = pPlayer;
				iMatches++;
				break;
			}
		}
	}

	return ( iMatches == 1 ) ? pFound : NULL;
}

//=========================================================
// gm_toggle <name | #userid>: an admin (server console / rcon, or a plugin's server command) turns a player's GM mode
// on or off (sma amx_gm; the dice reward). Co-op, whatever mp_gmmode is (the plugin had no such cvar); otherwise the
// rules of "gmon": a living player, one Game Master, not in the mission end.
//=========================================================
static void GmMode_ToggleCommand( void )
{
	if ( CMD_ARGC() < 2 )
	{
		g_engfuncs.pfnServerPrint( "usage: gm_toggle <name | #userid>\n" );
		return;
	}

	if ( !g_pGameRules || !GmMode_Allowed() )
	{
		g_engfuncs.pfnServerPrint( "gm_toggle: Game Master mode needs a co-op map\n" );
		return;
	}


	int iMatches;
	CBasePlayer *pFound = GmFindPlayer( CMD_ARGV( 1 ), iMatches );
	if ( !pFound || iMatches != 1 )
	{
		g_engfuncs.pfnServerPrint( iMatches > 1 ? "gm_toggle: more than one player matches\n" : "gm_toggle: no such player\n" );
		return;
	}

	CCoop *pCoop = (CCoop *)g_pGameRules;
	if ( pFound->m_bGameMaster )
	{
		pCoop->GameMasterOff( pFound, FALSE );
	}
	else if ( !pFound->IsAlive() || pFound->IsObserver() )
	{
		// sma cmd_toggle_gmmode's text for the console; the player sees it under his crosshair
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( "[GM] %s must be alive to enter GM Mode\n", STRING( pFound->pev->netname ) ) );
		GmMode_Notice( pFound, "You must be alive to enter GM Mode" );
	}
	else if ( !pCoop->GameMasterOn( pFound, TRUE ) )	// as sma cmd_toggle_gmmode: nothing printed when it worked
		g_engfuncs.pfnServerPrint( "gm_toggle: not possible now (there is already a Game Master, a spectator, or the mission end)\n" );
}

//=========================================================
// gm_entry_text <#userid | name> <pick> <price | status> "<text>" [<seconds>]: a plugin's line on a spawn window card
// (docs/GM_MODE.md "Plugin hooks"). price: a permanent line (a cost); status: a lock line, also the reason given
// when the plugin refuses the pick; <seconds>: a countdown ({t} = whole seconds left), the line goes when it runs
// out. "" clears the line. Only for a Game Master; sent to that player only, the texts go with GM mode.
//=========================================================
static void GmMode_EntryTextCommand( void )
{
	if ( CMD_ARGC() < 5 )
	{
		g_engfuncs.pfnServerPrint( "usage: gm_entry_text <name | #userid> <pick> <price | status> \"<text>\" [<seconds>]\n" );
		return;
	}

	int iMatches;
	CBasePlayer *pPlayer = GmFindPlayer( CMD_ARGV( 1 ), iMatches );
	if ( !pPlayer || !pPlayer->m_bGameMaster || s_Gm[pPlayer->entindex()].iState == GM_STATE_OFF )
	{
		g_engfuncs.pfnServerPrint( "gm_entry_text: no such Game Master\n" );
		return;
	}

	int iPick = atoi( CMD_ARGV( 2 ) );
	int iEntry = GM_PICK_ENTRY( iPick );
	int iVariant = GM_PICK_VARIANT( iPick );
	if ( iPick < 0 || iPick > 0xffff || iEntry >= GM_CATALOG_COUNT || iVariant < -1 || iVariant >= GM_PICK_MAX_VARIANT )
	{
		g_engfuncs.pfnServerPrint( "gm_entry_text: no such pick\n" );
		return;
	}

	static const char *s_pszSlots[GM_TEXT_SLOTS] = { "price", "status", "tier", "killpay", "cooldown" };
	const char *pszSlot = CMD_ARGV( 3 );
	int iSlot = -1;
	for ( int i = 0; i < GM_TEXT_SLOTS; i++ )
	{
		if ( !stricmp( pszSlot, s_pszSlots[i] ) )
			iSlot = i;
	}
	if ( iSlot < 0 )
	{
		g_engfuncs.pfnServerPrint( "gm_entry_text: the line is price, status, tier, killpay or cooldown\n" );
		return;
	}

	char szText[GM_TEXT_LEN];
	_snprintf( szText, sizeof( szText ), "%s", CMD_ARGV( 4 ) );
	szText[sizeof( szText ) - 1] = 0;

	int iSeconds = ( CMD_ARGC() >= 6 ) ? atoi( CMD_ARGV( 5 ) ) : 0;
	if ( iSeconds < 0 )
		iSeconds = 0;
	if ( iSeconds > 32767 )
		iSeconds = 32767;

	if ( iSlot == GM_TEXT_STATUS )
	{
		GmStatusText &status = s_GmStatus[pPlayer->entindex()][iEntry][iVariant + 1];
		memcpy( status.szText, szText, sizeof( status.szText ) );
		status.flEnd = iSeconds ? gpGlobals->time + iSeconds : 0;
	}
	else
		memcpy( s_GmCardText[pPlayer->entindex()][iEntry][iVariant + 1][iSlot], szText, GM_TEXT_LEN );

	if ( !gmsgGmEntry || pPlayer->has_disconnected )
		return;
	MESSAGE_BEGIN( MSG_ONE, gmsgGmEntry, NULL, pPlayer->pev );
		WRITE_BYTE( iEntry );
		WRITE_BYTE( iVariant + 1 );
		WRITE_BYTE( iSlot );
		WRITE_SHORT( iSeconds );
		WRITE_STRING( szText );
	MESSAGE_END();
}

//=========================================================
// The GM screens (docs/GM_MODE.md "Screen and menus"): the status strip and the players' data for the
// client (GmStatus, GmPlayers), the text menus (quick menu, Misc., orders, the classic menus' spawn and players
// lists), following a player, box selection and the orders.
//=========================================================

#define GM_MENU_ROWS		7	// list rows per page; 8 More, 9 Back, 0 Close
#define GM_HUNT_REFRESH		2.0
#define GM_HOLD_RANGE		64.0

static int s_iKilledBy[GM_MAX_PLAYERS + 1][GM_MAX_PLAYERS + 1];	// [GM][victim]: catalog entry + 1 of the monster, this round
static Vector s_vecDeathPos[GM_MAX_PLAYERS + 1];

static void GmShowMenu( CBasePlayer *pPlayer, int iMenu, int iKeys, const char *pszText );
static void GmQuickMenu( CBasePlayer *pPlayer );
static void GmOrdersMenu( CBasePlayer *pPlayer );

// a player in play: not a spectator, with a loadout, no Game Master, no proxy (the players window, nearest players,
// the orders' targets)
static BOOL GmListedPlayer( CBasePlayer *pTarget )
{
	return pTarget && !pTarget->has_disconnected && !FStringNull( pTarget->pev->netname ) && !pTarget->m_bSpectating
		&& !pTarget->m_bGameMaster && pTarget->m_bLoadoutReady && !( pTarget->pev->flags & FL_PROXY );
}

static BOOL GmLivingPlayer( CBasePlayer *pTarget )
{
	return GmListedPlayer( pTarget ) && pTarget->IsAlive() && !pTarget->IsObserver()
		&& !( pTarget->m_afPhysicsFlags & PFLAG_OBSERVER ) && !pTarget->m_bOutOfPlay;
}

static int GmWeaponEntry( CBasePlayer *pTarget )
{
	CBasePlayerItem *pItem = pTarget->m_pActiveItem;
	if ( !pItem )
		return 255;
	for ( int i = 0; i < GM_CATALOG_COUNT; i++ )
	{
		if ( g_GmCatalog[i].iKind == GM_KIND_WEAPON && FStrEq( g_GmCatalog[i].pszClass, STRING( pItem->pev->classname ) ) )
			return i;
	}
	return 255;
}

static int GmAliveMonsters( CBasePlayer *pPlayer )
{
	int iCount = 0;
	int iUserId = GETPLAYERUSERID( pPlayer->edict() );
	for ( int i = gpGlobals->maxClients + 1; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pent = INDEXENT( i );
		if ( !pent || pent->free || !( pent->v.flags & FL_MONSTER ) || pent->v.deadflag != DEAD_NO )
			continue;
		CBaseEntity *pEntity = CBaseEntity::Instance( pent );
		CBaseMonster *pMonster = pEntity ? pEntity->MyMonsterPointer() : NULL;
		if ( pMonster && !pMonster->IsPlayer() && (CBaseEntity *)pMonster->m_hGmOwner == pPlayer
			&& pMonster->m_iGmOwnerUserId == iUserId )
			iCount++;
	}
	return iCount;
}

static int GmSecondsLeft( float flEnd )
{
	if ( flEnd <= 0 || gpGlobals->time >= flEnd )
		return 0;
	int iLeft = (int)ceil( flEnd - gpGlobals->time );
	return ( iLeft > 32767 ) ? 32767 : iLeft;
}

// a spectator mode other than free flying (chase, first person, the overview modes): the Game Master is a full
// spectator then, as in the plugin; free flying is where the GM tools work
static BOOL GmFollowing( CBasePlayer *pPlayer )
{
	return pPlayer->pev->iuser1 != 0 && pPlayer->pev->iuser1 != OBS_ROAMING;
}

// GmStatus when it changed; GmPlayers every GM_PLAYERS_INTERVAL (GmMode_Think)
static void GmUpdateHud( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	if ( gpGlobals->time < gm.flNextHud || pPlayer->has_disconnected )
		return;
	gm.flNextHud = gpGlobals->time + GM_PLAYERS_INTERVAL;

	GmPruneSelection( pPlayer );
	int iFlags = ( gm.bEconomy ? GM_STATUS_ECONOMY : 0 ) | ( GmFollowing( pPlayer ) ? GM_STATUS_FOLLOWING : 0 );
	int iAlive = GmAliveMonsters( pPlayer );
	if ( iAlive > 255 )
		iAlive = 255;
	int iTierLeft = GmSecondsLeft( gm.flTierEnd ), iSpawnLeft = GmSecondsLeft( gm.flSpawnEnd );
	// the countdowns run on the client: only their end times matter, so they are hashed as end times
	int iHash = iFlags ^ ( gm.iPoints * 31 ) ^ ( gm.iTier << 20 ) ^ ( (int)gm.flTierEnd << 8 ) ^ ( (int)gm.flSpawnEnd << 14 )
		^ ( iAlive << 24 ) ^ ( gm.iSelected << 4 ) ^ ( (int)s_flGmRoamSpeed << 2 ) ^ ( gm.iMaxPoints << 11 )
		^ ( gm.iRefill << 17 ) ^ (int)( gm.flRefillEnd * 10 ) ^ ( (int)( gm.flRefillInterval * 10 ) << 6 );
	if ( ( iHash != gm.iSentHash || gm.iKillPop ) && gmsgGmStatus )
	{
		gm.iSentHash = iHash;
		const int iRefillLeft = gm.flRefillEnd > gpGlobals->time ? (int)ceil( ( gm.flRefillEnd - gpGlobals->time ) * 10 ) : 0;
		MESSAGE_BEGIN( MSG_ONE, gmsgGmStatus, NULL, pPlayer->pev );
			WRITE_BYTE( iFlags );
			WRITE_LONG( gm.iPoints );
			WRITE_BYTE( gm.iTier );
			WRITE_SHORT( iTierLeft );
			WRITE_SHORT( iSpawnLeft );
			WRITE_BYTE( iAlive );
			WRITE_BYTE( gm.iSelected );
			WRITE_SHORT( (int)min( s_flGmRoamSpeed, 32767.0f ) );
			WRITE_SHORT( min( gm.iMaxPoints, 32767 ) );
			WRITE_BYTE( min( gm.iRefill, 255 ) );
			WRITE_SHORT( min( (int)( gm.flRefillInterval * 10 ), 32767 ) );
			WRITE_SHORT( min( iRefillLeft, 32767 ) );
			WRITE_SHORT( min( gm.iKillPop, 32767 ) );
		MESSAGE_END();
		gm.iKillPop = 0;
	}

	if ( !gmsgGmPlayers )
		return;

	int iRows[GM_MAX_PLAYERS], iCount = 0;
	for ( int i = 1; i <= gpGlobals->maxClients && iCount < GM_MAX_PLAYERS; i++ )
	{
		CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( GmListedPlayer( pTarget ) )
			iRows[iCount++] = i;
	}

	int iGm = pPlayer->entindex();
	int iSent = 0;
	do
	{
		int iPart = min( iCount - iSent, GM_PLAYERS_PER_MSG );
		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgGmPlayers, NULL, pPlayer->pev );
			WRITE_BYTE( ( iSent == 0 ? GM_PLAYERS_FIRST : 0 ) | ( iSent + iPart >= iCount ? GM_PLAYERS_LAST : 0 ) );
			WRITE_BYTE( iPart );
			for ( int r = 0; r < iPart; r++ )
			{
				int i = iRows[iSent + r];
				CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( i );
				BOOL bAlive = GmLivingPlayer( pTarget );
				Vector vecPos = bAlive ? pTarget->pev->origin : s_vecDeathPos[i];
				int iHealth = bAlive ? (int)ceil( pTarget->pev->health ) : 0;
				int iMeters = (int)( ( vecPos - pPlayer->pev->origin ).Length() / 39.37 );
				WRITE_BYTE( i );
				WRITE_BYTE( bAlive ? GM_PLAYER_ALIVE : 0 );
				WRITE_BYTE( iHealth < 0 ? 0 : ( iHealth > 255 ? 255 : iHealth ) );
				WRITE_SHORT( iMeters > 32767 ? 32767 : iMeters );
				WRITE_SHORT( (int)vecPos.x );
				WRITE_SHORT( (int)vecPos.y );
				WRITE_SHORT( (int)vecPos.z );
				WRITE_BYTE( bAlive ? GmWeaponEntry( pTarget ) : 255 );
				WRITE_BYTE( s_iKilledBy[iGm][i] ? s_iKilledBy[iGm][i] - 1 : 255 );
			}
		MESSAGE_END();
		iSent += iPart;
	} while ( iSent < iCount );

	// the players window's map: this Game Master's living monsters, while it is open
	if ( !gm.bMapWatch || !gmsgGmMonsters )
		return;
	static int s_iMonsters[GM_MONSTERS_MAX];
	int iMonsters = 0;
	for ( int e = gpGlobals->maxClients + 1; e < gpGlobals->maxEntities && iMonsters < GM_MONSTERS_MAX; e++ )
	{
		edict_t *pent = INDEXENT( e );
		if ( !pent || pent->free || !( pent->v.flags & FL_MONSTER ) || pent->v.deadflag != DEAD_NO )
			continue;
		CBaseEntity *pEntity = CBaseEntity::Instance( pent );
		CBaseMonster *pMonster = pEntity ? pEntity->MyMonsterPointer() : NULL;
		if ( !pMonster || (CBaseEntity *)pMonster->m_hGmOwner != pPlayer
			|| pMonster->m_iGmOwnerUserId != GETPLAYERUSERID( pPlayer->edict() ) )
			continue;
		s_iMonsters[iMonsters++] = e;
	}
	iSent = 0;
	do
	{
		int iPart = min( iMonsters - iSent, GM_MONSTERS_PER_MSG );
		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgGmMonsters, NULL, pPlayer->pev );
			WRITE_BYTE( ( iSent == 0 ? GM_PLAYERS_FIRST : 0 ) | ( iSent + iPart >= iMonsters ? GM_PLAYERS_LAST : 0 ) );
			WRITE_BYTE( iPart );
			for ( int r = 0; r < iPart; r++ )
			{
				edict_t *pent = INDEXENT( s_iMonsters[iSent + r] );
				CBaseMonster *pMonster = CBaseEntity::Instance( pent )->MyMonsterPointer();
				BOOL bSelected = FALSE;
				for ( int k = 0; k < gm.iSelected; k++ )
					if ( (CBaseEntity *)gm.hSelected[k] == pMonster )
						bSelected = TRUE;
				const float flMax = pMonster->pev->max_health > 0 ? pMonster->pev->max_health : 100;
				int iPercent = (int)ceil( pMonster->pev->health * 100 / flMax );
				WRITE_SHORT( s_iMonsters[iSent + r] );
				WRITE_SHORT( (int)pent->v.origin.x );
				WRITE_SHORT( (int)pent->v.origin.y );
				WRITE_SHORT( (int)pent->v.origin.z );
				WRITE_BYTE( pMonster->m_iGmEntry >= 0 && pMonster->m_iGmEntry < GM_CATALOG_COUNT ? pMonster->m_iGmEntry : 255 );
				WRITE_BYTE( iPercent < 0 ? 0 : ( iPercent > 100 ? 100 : iPercent ) );
				WRITE_BYTE( ( bSelected ? GM_MONSTER_SELECTED : 0 ) | ( GmOrderable( pPlayer, pMonster ) ? GM_MONSTER_ORDERABLE : 0 ) );
			}
		MESSAGE_END();
		iSent += iPart;
	} while ( iSent < iMonsters );
}

// AddToFullPack: the players window's View shows the area around its player: what is near him goes to this Game
// Master even out of his own sight (the engine's PVS is the Game Master's)
BOOL GmMode_PipSend( edict_t *pEnt, edict_t *pHost )
{
	const int iHost = ENTINDEX( pHost );
	if ( iHost < 1 || iHost > gpGlobals->maxClients )
		return FALSE;
	const GmPlayerState &gm = s_Gm[iHost];
	if ( !gm.iPipTarget || gm.iState != GM_STATE_ACTIVE )
		return FALSE;
	CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( gm.iPipTarget );
	if ( !pTarget || pTarget->has_disconnected )
		return FALSE;
	const Vector vecCenter = ( pEnt->v.absmin + pEnt->v.absmax ) * 0.5f;
	return ( vecCenter - pTarget->pev->origin ).Length() < GM_PIP_RANGE;
}

//---------------------------------------------------------
// Following a player (the players window's Follow / First Person View, quick menu 4 and 5)
//---------------------------------------------------------
static void GmFollow( CBasePlayer *pPlayer, int iTarget, int iMode )
{
	CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( iTarget );
	if ( !GmLivingPlayer( pTarget ) )
	{
		GmPrint( pPlayer, "Selected player is not available" );
		return;
	}

	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	GmEndPlacement( pPlayer, TRUE );
	gm.bSelecting = FALSE;
	gm.iFollowTarget = iTarget;

	pPlayer->m_hObserverTarget = pTarget;
	pPlayer->m_bWasFollowing = FALSE;
	pPlayer->m_flNextObserverInput = gpGlobals->time + 0.2;
	if ( pPlayer->pev->iuser1 == iMode )
	{
		pPlayer->pev->iuser2 = iTarget;	// the same mode, another player
		return;
	}
	pPlayer->Observer_SetMode( iMode );
}

// back to free flying where the view is now
static void GmStopFollowing( CBasePlayer *pPlayer )
{
	if ( !GmFollowing( pPlayer ) )
		return;

	Vector vecOrigin = pPlayer->pev->origin, vecAngles = pPlayer->pev->v_angle;
	CBaseEntity *pTarget = pPlayer->m_hObserverTarget;
	if ( pTarget )
	{
		vecOrigin = pTarget->pev->origin + pTarget->pev->view_ofs;
		vecAngles = pTarget->pev->v_angle;
	}
	pPlayer->Observer_SetMode( OBS_ROAMING );
	GmForceRoaming( pPlayer );
	UTIL_SetOrigin( pPlayer->pev, vecOrigin );
	pPlayer->pev->v_angle = vecAngles;
	pPlayer->pev->angles = vecAngles;
	pPlayer->pev->fixangle = TRUE;
}

// in a spectator mode: the spectator's own keys and checks (Observer_HandleButtons: Jump the next mode, back to free
// flying after first person, Attack / Attack2 the next / previous player), as for any spectator
static void GmFollowThink( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];

	pPlayer->Observer_HandleButtons();
	if ( pPlayer->pev->iuser1 == OBS_ROAMING )
	{
		pPlayer->m_bWasFollowing = FALSE;	// a Game Master flies on from here; no re-follow later
		GmForceRoaming( pPlayer );
		return;
	}
	pPlayer->Observer_CheckTarget();
	if ( pPlayer->pev->iuser1 == OBS_ROAMING )
	{
		pPlayer->m_bWasFollowing = FALSE;
		GmForceRoaming( pPlayer );
		return;
	}
	if ( pPlayer->m_hObserverTarget != NULL && pPlayer->m_hObserverTarget->IsPlayer() )
		gm.iFollowTarget = ENTINDEX( pPlayer->m_hObserverTarget->edict() );
	pPlayer->Observer_CheckProperties();
	pPlayer->Observer_SendTargetInfo();
}

// quick menu 4: the next living player after the last one, free flying at his position (GmGotoPlayer)
static void GmGotoNext( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	if ( GmFollowing( pPlayer ) && pPlayer->pev->iuser2 >= 1 && pPlayer->pev->iuser2 <= gpGlobals->maxClients )
		gm.iGotoLast = pPlayer->pev->iuser2;	// the next after the one watched
	for ( int n = 1; n <= gpGlobals->maxClients; n++ )
	{
		int i = ( gm.iGotoLast + n - 1 ) % gpGlobals->maxClients + 1;
		if ( GmLivingPlayer( (CBasePlayer *)UTIL_PlayerByIndex( i ) ) )
		{
			gm.iGotoLast = i;
			// watching a player: the next one in the same mode (chase, first person); flying: fly to him
			if ( GmFollowing( pPlayer ) )
				GmFollow( pPlayer, i, pPlayer->pev->iuser1 );
			else
				GmGotoPlayer( pPlayer, i );
			return;
		}
	}
	GmPrint( pPlayer, "Selected player is not available" );
}

static int GmNearestPlayer( const Vector &vecFrom )
{
	int iBest = 0;
	float flBest = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !GmLivingPlayer( pTarget ) )
			continue;
		float flDist = ( pTarget->pev->origin - vecFrom ).Length();
		if ( !iBest || flDist < flBest )
		{
			iBest = i;
			flBest = flDist;
		}
	}
	return iBest;
}

// quick menu 5: follow the last followed (else the nearest) player, or stop following
static void GmToggleFollow( CBasePlayer *pPlayer )
{
	if ( GmFollowing( pPlayer ) )
	{
		GmStopFollowing( pPlayer );
		return;
	}
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	int iTarget = GmLivingPlayer( (CBasePlayer *)UTIL_PlayerByIndex( gm.iFollowTarget ) ) ? gm.iFollowTarget
		: GmNearestPlayer( pPlayer->pev->origin );
	if ( !iTarget )
	{
		GmPrint( pPlayer, "Selected player is not available" );
		return;
	}
	GmFollow( pPlayer, iTarget, OBS_CHASE_LOCKED );
}

// the players window's "Spawn near": the camera 128 units behind the player, looking at him, and the spawn window
static void GmSpawnNear( CBasePlayer *pPlayer, int iTarget )
{
	CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( iTarget );
	if ( !GmLivingPlayer( pTarget ) )
	{
		GmPrint( pPlayer, "Selected player is not available" );
		return;
	}
	GmStopFollowing( pPlayer );
	GmForceRoaming( pPlayer );

	UTIL_MakeVectors( Vector( 0, pTarget->pev->v_angle.y, 0 ) );
	Vector vecEye = pTarget->pev->origin + pTarget->pev->view_ofs;
	TraceResult tr;
	UTIL_TraceLine( vecEye, vecEye - gpGlobals->v_forward * 128 + Vector( 0, 0, 48 ), ignore_monsters, pTarget->edict(), &tr );
	Vector vecDest = tr.vecEndPos + ( vecEye - tr.vecEndPos ).Normalize() * 8;

	pPlayer->pev->velocity = g_vecZero;
	UTIL_SetOrigin( pPlayer->pev, vecDest );
	Vector vecAngles = UTIL_VecToAngles( vecEye - vecDest );
	vecAngles.x = -vecAngles.x;
	vecAngles.z = 0;
	pPlayer->pev->v_angle = vecAngles;
	pPlayer->pev->angles = vecAngles;
	pPlayer->pev->fixangle = TRUE;
	GmSendState( pPlayer, GM_EVENT_OPEN_MENU );
}

//---------------------------------------------------------
// Box selection from the client's screen rectangle ("gm_select x0 y0 x1 y1 tanx tany": -1..1 from the screen's
// center, y up; tanx / tany the view's half widths at distance 1)
//---------------------------------------------------------
static void GmSelectScreen( CBasePlayer *pPlayer, float x0, float y0, float x1, float y1, float flTanX, float flTanY,
	float flMaxDist )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	if ( x0 > x1 ) { float t = x0; x0 = x1; x1 = t; }
	if ( y0 > y1 ) { float t = y0; y0 = y1; y1 = t; }
	// a click: the monster of his under the click (its body, not only its middle), else a small box around it
	const BOOL bClick = ( x1 - x0 < 0.03 ) && ( y1 - y0 < 0.03 );
	if ( x1 - x0 < 0.03 ) { x0 -= 0.03; x1 += 0.03; }
	if ( y1 - y0 < 0.03 ) { y0 -= 0.03; y1 += 0.03; }

	GmClearSelection( pPlayer );
	Vector vecEye = GmEyePosition( pPlayer );
	UTIL_MakeVectors( pPlayer->pev->v_angle );
	Vector vecF = gpGlobals->v_forward, vecR = gpGlobals->v_right, vecU = gpGlobals->v_up;

	if ( bClick )
	{
		// the ray through the clicked point of the screen
		const float cx = ( x0 + x1 ) * 0.5f, cy = ( y0 + y1 ) * 0.5f;
		Vector vecDir = ( vecF + vecR * ( cx * flTanX ) + vecU * ( cy * flTanY ) ).Normalize();
		TraceResult tr;
		UTIL_TraceLine( vecEye, vecEye + vecDir * flMaxDist, dont_ignore_monsters, pPlayer->edict(), &tr );
		CBaseEntity *pHit = tr.pHit ? GmSelectTarget( CBaseEntity::Instance( tr.pHit ) ) : NULL;
		if ( pHit && GmSelectable( pPlayer, pHit ) )
		{
			gm.hSelected[gm.iSelected++] = pHit;
			gm.flNextHud = 0;
			GmOrdersMenu( pPlayer );
			return;
		}
	}

	for ( int i = gpGlobals->maxClients + 1; i < gpGlobals->maxEntities && gm.iSelected < GM_SELECT_MAX; i++ )
	{
		edict_t *pent = INDEXENT( i );
		if ( !pent || pent->free )
			continue;
		CBaseEntity *pEntity = CBaseEntity::Instance( pent );
		if ( !GmSelectable( pPlayer, pEntity ) )
			continue;

		Vector vecCenter = ( pEntity->pev->absmin + pEntity->pev->absmax ) * 0.5;
		Vector vecDir = vecCenter - vecEye;
		float z = DotProduct( vecDir, vecF );
		if ( z < 1 || vecDir.Length() > flMaxDist )
			continue;
		float sx = DotProduct( vecDir, vecR ) / ( z * flTanX );
		float sy = DotProduct( vecDir, vecU ) / ( z * flTanY );
		if ( sx < x0 || sx > x1 || sy < y0 || sy > y1 )
			continue;

		TraceResult tr;
		UTIL_TraceLine( vecEye, vecCenter, ignore_monsters, pPlayer->edict(), &tr );
		if ( tr.flFraction < 1.0 && tr.pHit != pent )
			continue;	// behind a wall (a solid pad stops the trace itself)

		gm.hSelected[gm.iSelected++] = pEntity;
	}

	gm.flNextHud = 0;
	if ( gm.iSelected )
		GmOrdersMenu( pPlayer );
	else if ( gm.iMenu == GM_MENU_ORDERS || gm.iMenu == GM_MENU_HUNT )
		GmQuickMenu( pPlayer );
}

//---------------------------------------------------------
// Orders 2-6 (the orders menu); 1 "Move here" is GmIssueOrders
//---------------------------------------------------------
static int GmOrderSlot( CBaseMonster *pMonster )
{
	int iSlot = -1;
	for ( int j = 0; j < GM_ORDER_MAX; j++ )
	{
		CBaseEntity *pOrdered = s_Orders[j].hMonster;
		if ( pOrdered == pMonster )
			return j;
		if ( !pOrdered && iSlot == -1 )
			iSlot = j;
	}
	return iSlot;
}

static void GmSetEnemy( CBaseMonster *pMonster, CBasePlayer *pTarget )
{
	pMonster->m_hEnemy = pTarget;
	pMonster->m_vecEnemyLKP = pTarget->pev->origin;
	pMonster->SetConditions( bits_COND_NEW_ENEMY );
	if ( pMonster->m_MonsterState != MONSTERSTATE_COMBAT )
		pMonster->m_IdealMonsterState = MONSTERSTATE_COMBAT;
	pMonster->ChangeSchedule( pMonster->GetScheduleOfType( SCHED_CHASE_ENEMY ) );
}

static void GmPersistentOrder( CBasePlayer *pPlayer, int iType, int iTarget )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	GmPruneSelection( pPlayer );

	for ( int i = 0; i < gm.iSelected; i++ )
	{
		CBaseMonster *pMonster = GmOrderable( pPlayer, gm.hSelected[i] );
		if ( !pMonster || pMonster->m_pCine || pMonster->m_MonsterState == MONSTERSTATE_SCRIPT )
			continue;

		int iSlot = GmOrderSlot( pMonster );
		if ( iSlot == -1 )
			continue;

		GmOrder &order = s_Orders[iSlot];
		memset( &order, 0, sizeof( order ) );
		order.hMonster = pMonster;
		order.iOwner = pPlayer->entindex();
		order.iOwnerUserId = GETPLAYERUSERID( pPlayer->edict() );
		order.iType = iType;
		order.iLastRouteIndex = -1;
		order.vecGoal = pMonster->pev->origin;

		int iVictim = iTarget;
		if ( iType == GM_ORDER_ATTACK )
			iVictim = GmNearestPlayer( pMonster->pev->origin );
		if ( iType == GM_ORDER_ATTACK || iType == GM_ORDER_HUNT )
		{
			CBasePlayer *pVictim = (CBasePlayer *)UTIL_PlayerByIndex( iVictim );
			if ( !GmLivingPlayer( pVictim ) )
			{
				memset( &order, 0, sizeof( order ) );
				continue;
			}
			order.iTarget = iVictim;
			GmSetEnemy( pMonster, pVictim );
		}
		else if ( iType == GM_ORDER_HOLD )
		{
			pMonster->RouteClear();
			pMonster->ChangeSchedule( pMonster->GetScheduleOfType( SCHED_IDLE_STAND ) );
		}
		order.flNextRefresh = gpGlobals->time;
	}

	gm.flNextHud = 0;
}

// hunt / attack: keep the player as the enemy; hold: back to the spot
static BOOL GmPersistentUpdate( GmOrder &order, CBasePlayer *pOwner, CBaseMonster *pMonster )
{
	if ( gpGlobals->time < order.flNextRefresh )
		return TRUE;
	order.flNextRefresh = gpGlobals->time + GM_HUNT_REFRESH;

	if ( order.iType == GM_ORDER_ATTACK || order.iType == GM_ORDER_HUNT )
	{
		CBasePlayer *pVictim = (CBasePlayer *)UTIL_PlayerByIndex( order.iTarget );
		if ( !GmLivingPlayer( pVictim ) )
			return FALSE;	// he died or left: the order is over
		if ( (CBaseEntity *)pMonster->m_hEnemy != pVictim )
			GmSetEnemy( pMonster, pVictim );
		else
			pMonster->m_vecEnemyLKP = pVictim->pev->origin;
		return TRUE;
	}

	if ( pMonster->m_hEnemy != NULL || pMonster->m_pSchedule == slGmMove )
		return TRUE;	// fighting, or on its way

	const Vector vecGoal = order.vecGoal;
	if ( ( vecGoal - pMonster->pev->origin ).Length2D() <= GM_HOLD_RANGE )
		return TRUE;
	if ( pMonster->pev->movetype != MOVETYPE_STEP )
		return TRUE;

	pMonster->m_vecMoveGoal = vecGoal;
	pMonster->ChangeSchedule( slGmMove );
	return TRUE;
}

//---------------------------------------------------------
// Text menus (ShowMenu; the client draws a Game Master's text menus in the design's look)
//---------------------------------------------------------
static void GmShowMenu( CBasePlayer *pPlayer, int iMenu, int iKeys, const char *pszText )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	gm.iMenu = iMenu;

	if ( iMenu == GM_MENU_NONE )
	{
		if ( pPlayer->m_iTextMenu == TEXTMENU_GM )
		{
			pPlayer->m_iTextMenu = TEXTMENU_NONE;
			pPlayer->ShowMenu( 0, 0, FALSE, (char *)"" );
		}
		return;
	}

	pPlayer->m_iTextMenu = TEXTMENU_GM;
	// a text menu message holds at most ~180 characters: longer ones go in parts (the client joins them)
	const int iChunk = 175;
	int iLen = (int)strlen( pszText );
	for ( int iPos = 0; iPos < iLen || iPos == 0; iPos += iChunk )
	{
		char szPart[iChunk + 1];
		int n = min( iChunk, iLen - iPos );
		memcpy( szPart, pszText + iPos, n );
		szPart[n] = 0;
		pPlayer->ShowMenu( iKeys, -1, iPos + iChunk < iLen, szPart );
		if ( !iLen )
			break;
	}
	GmSendState( pPlayer, GM_EVENT_QUICK_MENU );
}

static void GmPickLabel( int iEntry, int iVariant, char *pszOut, int iLen )
{
	int iCount;
	const GmVariant *pVariants = GmCatalog_Variants( g_GmCatalog[iEntry].iVariants, iCount );
	if ( pVariants && iVariant >= 0 && iVariant < iCount )
		_snprintf( pszOut, iLen, "%s (%s)", g_GmCatalog[iEntry].pszName, pVariants[iVariant].pszName );
	else
		_snprintf( pszOut, iLen, "%s", g_GmCatalog[iEntry].pszName );
	pszOut[iLen - 1] = 0;
}

// a placement cancelled or refused: it ends, the quick menu comes up (never the spawn window)
static void GmBackToQuickMenu( CBasePlayer *pPlayer )
{
	GmEndPlacement( pPlayer, TRUE );
	GmQuickMenu( pPlayer );
}

static void GmQuickMenu( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	char szLast[64] = "";
	if ( gm.iLastEntry >= 0 && gm.iLastEntry < GM_CATALOG_COUNT )
		GmPickLabel( gm.iLastEntry, gm.iLastVariant, szLast, sizeof( szLast ) );

	// 5: "Follow a player", or "Stop following <name>" while following one
	char szFollow[64] = "Follow a player";
	CBaseEntity *pTarget = pPlayer->m_hObserverTarget;
	if ( GmFollowing( pPlayer ) && pTarget && pTarget->IsPlayer() )
		_snprintf( szFollow, sizeof( szFollow ), "Stop following %s", STRING( pTarget->pev->netname ) );
	szFollow[sizeof( szFollow ) - 1] = 0;

	char szText[512];
	_snprintf( szText, sizeof( szText ),
		"\\yGM QUICK MENU\n\n%s\n\\d2. Spawn menu\n\\w3. Players\n4. Go to next player\n5. %s\n"
		"6. Support\n7. Teleporters\n8. Misc\n\n\\d0. Close",
		szLast[0] ? UTIL_VarArgs( "\\w1. Spawn again: %s", szLast ) : "\\d1. Spawn again", szFollow );
	szText[sizeof( szText ) - 1] = 0;
	GmShowMenu( pPlayer, GM_MENU_QUICK, 0x3ff, szText );
}

static void GmMiscMenu( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	// sma show_misc_menu: Help, Budget Info, Leave GM Mode (Budget Info is the budget plugin's)
	if ( gm.bEconomy )
		GmShowMenu( pPlayer, GM_MENU_MISC, ( 1 << 0 ) | ( 1 << 1 ) | ( 1 << 2 ) | ( 1 << 9 ),
			"\\yGM MODE: MISC.\n\n\\w1. Help\n2. Budget Info\n\\r3. Leave GM Mode\n\n\\d0. Back" );
	else
		GmShowMenu( pPlayer, GM_MENU_MISC, ( 1 << 0 ) | ( 1 << 2 ) | ( 1 << 9 ),
			"\\yGM MODE: MISC.\n\n\\w1. Help\n\\d2. Budget Info\n\\r3. Leave GM Mode\n\n\\d0. Back" );
}

static void GmOrdersMenu( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	// only the lines that apply to something selected: the orders for monsters that take orders ("3. Hunt player...":
	// the list of the living players in play), Destroy selected for everything
	BOOL bOrderable = FALSE;
	for ( int i = 0; i < gm.iSelected && !bOrderable; i++ )
		bOrderable = GmOrderable( pPlayer, gm.hSelected[i] ) != NULL;
	char szText[512];
	_snprintf( szText, sizeof( szText ), "\\yORDERS \xC2\xB7 %d SELECTED\n\n%s\\w5. Destroy selected\n\n\\d9. Clear selection\n0. Close",
		gm.iSelected, bOrderable ? "\\w1. Move here (aim)\n2. Attack nearest player\n3. Hunt player\n4. Hold position\n" : "" );
	szText[sizeof( szText ) - 1] = 0;
	GmShowMenu( pPlayer, GM_MENU_ORDERS, ( bOrderable ? ( 1 << 0 ) | ( 1 << 1 ) | ( 1 << 2 ) | ( 1 << 3 ) : 0 ) | ( 1 << 4 )
		| ( 1 << 8 ) | ( 1 << 9 ), szText );
}

// Destroy selected: the monsters die (no kill credit), the weapon drops go, a teleporter pad takes its pair with it
static void GmDestroySelected( CBasePlayer *pPlayer )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	GmPruneSelection( pPlayer );
	int iDone = 0;
	for ( int i = 0; i < gm.iSelected; i++ )
	{
		CBaseEntity *pEntity = gm.hSelected[i];
		if ( !pEntity )
			continue;
		if ( pEntity->pev->flags & FL_MONSTER )
		{
			if ( pEntity->pev->takedamage == DAMAGE_NO )
				UTIL_Remove( pEntity );	// a turret folded up takes no damage
			else
				GmKillMonster( pEntity );
		}
		else if ( FClassnameIs( pEntity->pev, "gm_teleporter_pad" ) )
		{
			CGmTeleporter *pPad = (CGmTeleporter *)pEntity;
			GmRemovePad( (CGmTeleporter *)(CBaseEntity *)pPad->m_hLinked );
			GmRemovePad( pPad );
		}
		else
			UTIL_Remove( pEntity );	// a weapon drop (UpdateOnRemove: its weapon)
		iDone++;
	}
	GmClearSelection( pPlayer );
	gm.flNextHud = 0;
	GmSendState( pPlayer, GM_EVENT_NONE );	// the teleporter pairs' count
	if ( iDone )
		GmPrint( pPlayer, UTIL_VarArgs( "Destroyed %d", iDone ) );
}

// a list menu: GM_MENU_ROWS rows of pszRows[] (with keys), 8 More, 9 Back, 0 Close
static void GmListMenu( CBasePlayer *pPlayer, int iMenu, const char *pszTitle, const char **ppszRows, const int *piRows,
	const BOOL *pbEnabled, int iCount )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	int iPages = max( 1, ( iCount + GM_MENU_ROWS - 1 ) / GM_MENU_ROWS );
	if ( gm.iMenuPage >= iPages )
		gm.iMenuPage = 0;
	int iFirst = gm.iMenuPage * GM_MENU_ROWS;

	char szText[1024];
	int iPos = _snprintf( szText, sizeof( szText ), "\\y%s\n\n", pszTitle );
	int iKeys = ( 1 << 8 ) | ( 1 << 9 );
	gm.iMenuRowCount = 0;
	for ( int r = 0; r < GM_MENU_ROWS && iFirst + r < iCount && iPos < (int)sizeof( szText ) - 96; r++ )
	{
		BOOL bOn = pbEnabled ? pbEnabled[iFirst + r] : TRUE;
		iPos += _snprintf( szText + iPos, sizeof( szText ) - iPos, "%s%d. %s\n", bOn ? "\\w" : "\\d", r + 1, ppszRows[iFirst + r] );
		gm.iMenuRows[r] = piRows[iFirst + r];
		gm.iMenuRowCount++;
		if ( bOn )
			iKeys |= 1 << r;
	}
	if ( iCount == 0 )
		iPos += _snprintf( szText + iPos, sizeof( szText ) - iPos, "\\dNothing here\n" );
	if ( iPages > 1 )
	{
		iPos += _snprintf( szText + iPos, sizeof( szText ) - iPos, "\n\\w8. More\n" );
		iKeys |= 1 << 7;
	}
	_snprintf( szText + iPos, sizeof( szText ) - iPos, "\n\\d9. Back\n0. Close" );
	szText[sizeof( szText ) - 1] = 0;
	GmShowMenu( pPlayer, iMenu, iKeys, szText );
}

// "<name> · <cost> pts · <tag>" as the spawn window's card (classic menus)
static void GmCardLine( CBasePlayer *pPlayer, int iEntry, int iVariant, const char *pszName, char *pszOut, int iLen )
{
	int iIndex = pPlayer->entindex();
	const char *pszPrice = s_GmCardText[iIndex][iEntry][iVariant + 1][GM_TEXT_PRICE];
	if ( !pszPrice[0] )
		pszPrice = s_GmCardText[iIndex][iEntry][0][GM_TEXT_PRICE];
	const char *pszTier = s_GmCardText[iIndex][iEntry][iVariant + 1][GM_TEXT_TIER];
	if ( !pszTier[0] )
		pszTier = s_GmCardText[iIndex][iEntry][0][GM_TEXT_TIER];
	char szStatus[GM_TEXT_LEN];
	GmResolveStatus( s_GmStatus[iIndex][iEntry][iVariant + 1], szStatus, sizeof( szStatus ) );
	if ( !szStatus[0] )
		GmResolveStatus( s_GmStatus[iIndex][iEntry][0], szStatus, sizeof( szStatus ) );
	const GmStatusText &st = s_GmStatus[iIndex][iEntry][iVariant + 1].szText[0] ? s_GmStatus[iIndex][iEntry][iVariant + 1]
		: s_GmStatus[iIndex][iEntry][0];

	char szTag[48];
	if ( pszTier[0] )
		_snprintf( szTag, sizeof( szTag ), "Tier %s", pszTier );
	else if ( szStatus[0] && st.flEnd > 0 )
		_snprintf( szTag, sizeof( szTag ), "%ds", GmSecondsLeft( st.flEnd ) );
	else if ( szStatus[0] )
		_snprintf( szTag, sizeof( szTag ), "%s", szStatus );
	else
		_snprintf( szTag, sizeof( szTag ), "Ready" );
	szTag[sizeof( szTag ) - 1] = 0;

	if ( pszPrice[0] )
		_snprintf( pszOut, iLen, "%s \xC2\xB7 %s pts \xC2\xB7 %s", pszName, pszPrice, szTag );
	else
		_snprintf( pszOut, iLen, "%s \xC2\xB7 %s", pszName, szTag );
	pszOut[iLen - 1] = 0;
}

static const char *s_pszTabNames[GM_CAT_COUNT] = { "Tier I", "Tier II", "Tier III", "Support", "Teleporters" };

static void GmTeleportersMenu( CBasePlayer *pPlayer );

static void GmSpawnListMenu( CBasePlayer *pPlayer, int iTab )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	if ( iTab == GM_CAT_TELEPORTERS )
	{
		GmTeleportersMenu( pPlayer );
		return;
	}
	static char s_szRows[GM_CATALOG_COUNT][96];
	const char *ppszRows[GM_CATALOG_COUNT];
	int iRows[GM_CATALOG_COUNT];
	int iCount = 0;
	for ( int i = 0; i < GM_CATALOG_COUNT; i++ )
	{
		if ( g_GmCatalog[i].iCategory != iTab )
			continue;
		if ( g_GmCatalog[i].iVariants != GM_VARIANTS_NONE )
			_snprintf( s_szRows[iCount], sizeof( s_szRows[iCount] ), "%s", g_GmCatalog[i].pszName );
		else
			GmCardLine( pPlayer, i, -1, g_GmCatalog[i].pszName, s_szRows[iCount], sizeof( s_szRows[iCount] ) );
		s_szRows[iCount][sizeof( s_szRows[iCount] ) - 1] = 0;
		ppszRows[iCount] = s_szRows[iCount];
		iRows[iCount] = i;
		iCount++;
	}
	gm.iMenuArg = iTab;
	GmListMenu( pPlayer, GM_MENU_SPAWN_LIST, s_pszTabNames[iTab], ppszRows, iRows, NULL, iCount );
}

static void GmVariantsMenu( CBasePlayer *pPlayer, int iEntry )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	int iCount;
	const GmVariant *pVariants = GmCatalog_Variants( g_GmCatalog[iEntry].iVariants, iCount );
	static char s_szRows[GM_PICK_MAX_VARIANT][96];
	const char *ppszRows[GM_PICK_MAX_VARIANT];
	int iRows[GM_PICK_MAX_VARIANT];
	for ( int v = 0; v < iCount && v < GM_PICK_MAX_VARIANT; v++ )
	{
		char szName[64];
		GmPickLabel( iEntry, v, szName, sizeof( szName ) );
		GmCardLine( pPlayer, iEntry, v, szName, s_szRows[v], sizeof( s_szRows[v] ) );
		ppszRows[v] = s_szRows[v];
		iRows[v] = v;
	}
	gm.iMenuArg = iEntry;
	gm.iMenuPage = 0;
	GmListMenu( pPlayer, GM_MENU_VARIANTS, g_GmCatalog[iEntry].pszName, ppszRows, iRows, NULL, min( iCount, GM_PICK_MAX_VARIANT ) );
}

static void GmSpawnTabsMenu( CBasePlayer *pPlayer )
{
	GmShowMenu( pPlayer, GM_MENU_SPAWN, ( 1 << 0 ) | ( 1 << 1 ) | ( 1 << 2 ) | ( 1 << 3 ) | ( 1 << 4 ) | ( 1 << 9 ),
		"\\ySPAWN\n\n\\w1. Tier I\n2. Tier II\n3. Tier III\n4. Support\n5. Teleporters\n\n\\d0. Close" );
}

static void GmTeleportersMenu( CBasePlayer *pPlayer )
{
	// sma show_teleporter_menu: Place Teleporter, Manage Teleporters, Clear All Teleporters
	GmShowMenu( pPlayer, GM_MENU_TELEPORTERS, ( 1 << 0 ) | ( 1 << 1 ) | ( 1 << 2 ) | ( 1 << 8 ) | ( 1 << 9 ),
		(CBaseEntity *)s_Gm[pPlayer->entindex()].hEntrance
		? "\\yTELEPORTERS\n\n\\w1. Place Teleporter Exit\n2. Manage Teleporters\n\\r3. Clear All Teleporters\n\n\\d9. Back\n0. Close"
		: "\\yTELEPORTERS\n\n\\w1. Place Teleporter\n2. Manage Teleporters\n\\r3. Clear All Teleporters\n\n\\d9. Back\n0. Close" );
}

static void GmTpPairsMenu( CBasePlayer *pPlayer )
{
	CGmTeleporter *pPairs[GM_TP_MAX_PAIRS];
	int iPairs = GmCollectPairs( pPlayer, pPairs, GM_TP_MAX_PAIRS );
	if ( !iPairs )
	{
		GmPrint( pPlayer, "No active teleporter pairs" );
		GmTeleportersMenu( pPlayer );
		return;
	}
	static char s_szRows[GM_TP_MAX_PAIRS][32];
	const char *ppszRows[GM_TP_MAX_PAIRS];
	int iRows[GM_TP_MAX_PAIRS];
	for ( int i = 0; i < iPairs; i++ )
	{
		_snprintf( s_szRows[i], sizeof( s_szRows[i] ), "Teleporter Pair %d", i + 1 );
		ppszRows[i] = s_szRows[i];
		iRows[i] = i + 1;
	}
	GmListMenu( pPlayer, GM_MENU_TP_PAIRS, "MANAGE TELEPORTERS", ppszRows, iRows, NULL, iPairs );
}

static void GmTpPairMenu( CBasePlayer *pPlayer, int iPair )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	gm.iMenuArg = iPair;
	// sma show_teleporter_pair_menu
	GmShowMenu( pPlayer, GM_MENU_TP_PAIR, ( 1 << 0 ) | ( 1 << 1 ) | ( 1 << 2 ) | ( 1 << 8 ) | ( 1 << 9 ),
		UTIL_VarArgs( "\\yTELEPORTER PAIR %d\n\n\\w1. Go to Entrance\n2. Go to Exit\n\\r3. Remove Teleporters\n\n\\d9. Back\n0. Close", iPair ) );
}

static void GmPlayersMenu( CBasePlayer *pPlayer )
{
	// the players window's rows: the living (nearest first), then the dead
	static char s_szRows[GM_MAX_PLAYERS][96];
	const char *ppszRows[GM_MAX_PLAYERS];
	int iRows[GM_MAX_PLAYERS];
	BOOL bEnabled[GM_MAX_PLAYERS];
	float flDist[GM_MAX_PLAYERS];
	int iCount = 0, iGm = pPlayer->entindex();

	for ( int pass = 0; pass < 2; pass++ )
	{
		int iStart = iCount;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( !GmListedPlayer( pTarget ) || GmLivingPlayer( pTarget ) != ( pass == 0 ) )
				continue;
			float d = ( pTarget->pev->origin - pPlayer->pev->origin ).Length();
			int k = iCount;
			while ( pass == 0 && k > iStart && flDist[k - 1] > d )
			{
				iRows[k] = iRows[k - 1];
				flDist[k] = flDist[k - 1];
				k--;
			}
			iRows[k] = i;
			flDist[k] = d;
			iCount++;
		}
	}
	for ( int r = 0; r < iCount; r++ )
	{
		CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( iRows[r] );
		bEnabled[r] = GmLivingPlayer( pTarget );
		if ( bEnabled[r] )
			_snprintf( s_szRows[r], sizeof( s_szRows[r] ), "%s \xC2\xB7 %d HP \xC2\xB7 %d m", STRING( pTarget->pev->netname ),
				(int)ceil( pTarget->pev->health ), (int)( flDist[r] / 39.37 ) );
		else if ( s_iKilledBy[iGm][iRows[r]] )
			_snprintf( s_szRows[r], sizeof( s_szRows[r] ), "%s \xC2\xB7 dead \xC2\xB7 Killed by your %s", STRING( pTarget->pev->netname ),
				g_GmCatalog[s_iKilledBy[iGm][iRows[r]] - 1].pszName );
		else
			_snprintf( s_szRows[r], sizeof( s_szRows[r] ), "%s \xC2\xB7 dead", STRING( pTarget->pev->netname ) );
		s_szRows[r][sizeof( s_szRows[r] ) - 1] = 0;
		ppszRows[r] = s_szRows[r];
	}
	GmListMenu( pPlayer, GM_MENU_PLAYERS, "PLAYERS", ppszRows, iRows, bEnabled, iCount );
}

static void GmPlayerMenu( CBasePlayer *pPlayer, int iTarget )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( iTarget );
	if ( !GmLivingPlayer( pTarget ) )
	{
		GmPlayersMenu( pPlayer );
		return;
	}
	gm.iMenuArg = iTarget;
	char szText[256];
	_snprintf( szText, sizeof( szText ), "\\y%s\n\n\\w1. Go to\n2. Follow\n3. First Person View\n4. Spawn near\n\n\\d9. Back\n0. Close",
		STRING( pTarget->pev->netname ) );
	szText[sizeof( szText ) - 1] = 0;
	GmShowMenu( pPlayer, GM_MENU_PLAYER, ( 1 << 0 ) | ( 1 << 1 ) | ( 1 << 2 ) | ( 1 << 3 ) | ( 1 << 8 ) | ( 1 << 9 ), szText );
}

static void GmHuntMenu( CBasePlayer *pPlayer )
{
	static char s_szRows[GM_MAX_PLAYERS][64];
	const char *ppszRows[GM_MAX_PLAYERS];
	int iRows[GM_MAX_PLAYERS];
	int iCount = 0;
	// the nearest first
	int iOrder[GM_MAX_PLAYERS], iOrdered = 0;
	float flDist[GM_MAX_PLAYERS];
	for ( int i = 1; i <= gpGlobals->maxClients && iOrdered < GM_MAX_PLAYERS; i++ )
	{
		CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !GmLivingPlayer( pTarget ) )
			continue;
		float d = ( pTarget->pev->origin - pPlayer->pev->origin ).Length();
		int k = iOrdered++;
		while ( k > 0 && flDist[k - 1] > d )
		{
			iOrder[k] = iOrder[k - 1];
			flDist[k] = flDist[k - 1];
			k--;
		}
		iOrder[k] = i;
		flDist[k] = d;
	}
	for ( int n = 0; n < iOrdered; n++ )
	{
		int i = iOrder[n];
		CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( i );
		_snprintf( s_szRows[iCount], sizeof( s_szRows[iCount] ), "%s", STRING( pTarget->pev->netname ) );
		s_szRows[iCount][sizeof( s_szRows[iCount] ) - 1] = 0;
		ppszRows[iCount] = s_szRows[iCount];
		iRows[iCount] = i;
		iCount++;
	}
	GmListMenu( pPlayer, GM_MENU_HUNT, "HUNT A PLAYER", ppszRows, iRows, NULL, iCount );
}

// the client asks for a window's text menu (classic menus): "gm_textmenu spawn [tab] | players"
static void GmTextMenuCommand( CBasePlayer *pPlayer, const char *pszWhat, int iArg )
{
	s_Gm[pPlayer->entindex()].iMenuPage = 0;
	if ( FStrEq( pszWhat, "players" ) )
		GmPlayersMenu( pPlayer );
	else if ( iArg >= 0 && iArg < GM_CAT_COUNT )
		GmSpawnListMenu( pPlayer, iArg );
	else
		GmSpawnTabsMenu( pPlayer );
}

// a pick from the quick menu ("Spawn again") or a classic list: the window's gm_pick
static void GmPick( CBasePlayer *pPlayer, int iEntry, int iVariant );

static void GmMenuSelect( CBasePlayer *pPlayer, int iSlot )
{
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	int iMenu = gm.iMenu;
	gm.iMenu = GM_MENU_NONE;
	if ( gm.iState != GM_STATE_ACTIVE )
		return;

	int iKey = iSlot;	// 1..9, 10 = 0
	int iRow = iKey - 1;
	BOOL bListRow = iRow >= 0 && iRow < gm.iMenuRowCount && iRow < GM_MENU_ROWS;

	switch ( iMenu )
	{
	case GM_MENU_QUICK:
		switch ( iKey )
		{
		case 1:
			if ( gm.iLastEntry >= 0 )
				GmPick( pPlayer, gm.iLastEntry, gm.iLastVariant );
			else
				GmQuickMenu( pPlayer );
			break;
		case 2: GmSendState( pPlayer, GM_EVENT_OPEN_MENU ); break;
		case 3: GmSendState( pPlayer, GM_EVENT_OPEN_PLAYERS ); break;
		// the quick menu stays up after its actions; a selection brings the orders menu instead
		case 4: GmGotoNext( pPlayer ); GmQuickMenu( pPlayer ); break;
		case 5: GmToggleFollow( pPlayer ); GmQuickMenu( pPlayer ); break;
		case 6: GmSendState( pPlayer, GM_EVENT_OPEN_MENU, "", GM_CATALOG_COUNT + GM_CAT_SUPPORT ); break;
		case 7: GmSendState( pPlayer, GM_EVENT_OPEN_MENU, "", GmToolEntry( GM_TOOL_TELEPORTERS ) ); break;
		case 8: GmMiscMenu( pPlayer ); break;
		}
		break;

	case GM_MENU_MISC:
		if ( iKey == 1 )
		{
			GmEndPlacement( pPlayer, TRUE );
			ShowVGUI( pPlayer, 19 );
		}
		else if ( iKey == 2 && gm.bEconomy )
			CLIENT_COMMAND( pPlayer->edict(), (char *)"gm_budget\n" );	// the budget plugin's list (sma Budget Info)
		else if ( iKey == 3 )
			GmShowMenu( pPlayer, GM_MENU_LEAVE, ( 1 << 0 ) | ( 1 << 1 ), "\\yLEAVE GM MODE?\n\n\\w1. Yes\n2. No" );
		else if ( iKey == 10 )
			GmQuickMenu( pPlayer );
		break;

	case GM_MENU_LEAVE:
		if ( iKey == 1 && m_Mode == MODE_COOP )
			( (CCoop *)g_pGameRules )->GameMasterOff( pPlayer, FALSE );
		else
			GmMiscMenu( pPlayer );
		break;

	case GM_MENU_ORDERS:
		switch ( iKey )
		{
		case 1: GmIssueOrders( pPlayer ); GmOrdersMenu( pPlayer ); break;
		case 2: GmPersistentOrder( pPlayer, GM_ORDER_ATTACK, 0 ); GmOrdersMenu( pPlayer ); break;
		case 3:
			gm.iMenuPage = 0;
			GmHuntMenu( pPlayer );
			break;
		case 4: GmPersistentOrder( pPlayer, GM_ORDER_HOLD, 0 ); GmOrdersMenu( pPlayer ); break;
		case 5: GmDestroySelected( pPlayer ); GmQuickMenu( pPlayer ); break;	// nothing selected now
		case 9:
			GmClearSelection( pPlayer );
			gm.flNextHud = 0;
			GmQuickMenu( pPlayer );
			break;
		}
		break;

	case GM_MENU_HUNT:
		if ( bListRow )
		{
			gm.iHuntTarget = gm.iMenuRows[iRow];
			GmPersistentOrder( pPlayer, GM_ORDER_HUNT, gm.iHuntTarget );
			GmOrdersMenu( pPlayer );
		}
		else if ( iKey == 8 )
		{
			gm.iMenuPage++;
			GmHuntMenu( pPlayer );
		}
		else if ( iKey == 9 )
			GmOrdersMenu( pPlayer );
		break;

	case GM_MENU_SPAWN:
		if ( iKey >= 1 && iKey <= GM_CAT_COUNT )
		{
			gm.iMenuPage = 0;
			GmSpawnListMenu( pPlayer, iKey - 1 );
		}
		break;

	case GM_MENU_SPAWN_LIST:
		if ( bListRow )
		{
			int iEntry = gm.iMenuRows[iRow];
			if ( g_GmCatalog[iEntry].iVariants != GM_VARIANTS_NONE )
				GmVariantsMenu( pPlayer, iEntry );
			else
				GmPick( pPlayer, iEntry, -1 );
		}
		else if ( iKey == 8 )
		{
			gm.iMenuPage++;
			GmSpawnListMenu( pPlayer, gm.iMenuArg );
		}
		else if ( iKey == 9 )
			GmSpawnTabsMenu( pPlayer );
		break;

	case GM_MENU_VARIANTS:
		if ( bListRow )
			GmPick( pPlayer, gm.iMenuArg, gm.iMenuRows[iRow] );
		else if ( iKey == 9 )
		{
			gm.iMenuPage = 0;
			GmSpawnListMenu( pPlayer, g_GmCatalog[gm.iMenuArg].iCategory );
		}
		break;

	case GM_MENU_TELEPORTERS:
		if ( iKey == 1 )
			GmTpAction( pPlayer, "place", 0, 0 );
		else if ( iKey == 2 )
		{
			gm.iMenuPage = 0;
			GmTpPairsMenu( pPlayer );
		}
		else if ( iKey == 3 )
			GmShowMenu( pPlayer, GM_MENU_TP_CLEAR, ( 1 << 0 ) | ( 1 << 1 ), "\\yCLEAR ALL TELEPORTERS?\n\n\\w1. Yes\n2. No" );
		else if ( iKey == 9 )
			GmSpawnTabsMenu( pPlayer );
		break;

	case GM_MENU_TP_PAIRS:
		if ( bListRow )
			GmTpPairMenu( pPlayer, gm.iMenuRows[iRow] );
		else if ( iKey == 8 )
		{
			gm.iMenuPage++;
			GmTpPairsMenu( pPlayer );
		}
		else if ( iKey == 9 )
			GmTeleportersMenu( pPlayer );
		break;

	case GM_MENU_TP_PAIR:
		if ( iKey == 1 || iKey == 2 )
		{
			GmTpAction( pPlayer, "goto", gm.iMenuArg, iKey - 1 );
			GmTpPairMenu( pPlayer, gm.iMenuArg );
		}
		else if ( iKey == 3 )
			GmShowMenu( pPlayer, GM_MENU_TP_REMOVE, ( 1 << 0 ) | ( 1 << 1 ), "\\yREMOVE TELEPORTERS?\n\n\\w1. Yes\n2. No" );
		else if ( iKey == 9 )
			GmTpPairsMenu( pPlayer );
		break;

	case GM_MENU_TP_REMOVE:
		if ( iKey == 1 )
		{
			GmTpAction( pPlayer, "remove", gm.iMenuArg, 0 );
			gm.iMenuPage = 0;
			GmTeleportersMenu( pPlayer );
		}
		else
			GmTpPairMenu( pPlayer, gm.iMenuArg );
		break;

	case GM_MENU_TP_CLEAR:
		if ( iKey == 1 )
			GmTpAction( pPlayer, "clear", 0, 0 );
		GmTeleportersMenu( pPlayer );
		break;

	case GM_MENU_PLAYERS:
		if ( bListRow )
			GmPlayerMenu( pPlayer, gm.iMenuRows[iRow] );
		else if ( iKey == 8 )
		{
			gm.iMenuPage++;
			GmPlayersMenu( pPlayer );
		}
		else if ( iKey == 9 )
			GmQuickMenu( pPlayer );
		break;

	case GM_MENU_PLAYER:
		switch ( iKey )
		{
		case 1: GmStopFollowing( pPlayer ); GmGotoPlayer( pPlayer, gm.iMenuArg ); break;
		case 2: GmFollow( pPlayer, gm.iMenuArg, OBS_CHASE_LOCKED ); break;
		case 3: GmFollow( pPlayer, gm.iMenuArg, OBS_IN_EYE ); break;
		case 4: GmSpawnNear( pPlayer, gm.iMenuArg ); break;
		case 9: gm.iMenuPage = 0; GmPlayersMenu( pPlayer ); break;
		}
		break;
	}
}

// CCoop's "menuselect" for TEXTMENU_GM
void GmMode_MenuSelect( CBasePlayer *pPlayer, int iSlot )
{
	if ( pPlayer && pPlayer->m_bGameMaster )
		GmMenuSelect( pPlayer, iSlot );
}

// round start (CCoop): "Killed by your <monster>" is this round's
void GmMode_RoundStart( void )
{
	memset( s_iKilledBy, 0, sizeof( s_iKilledBy ) );
}

// gm_status <#userid | name> <points> <tier> <seconds to the next tier> <seconds to the next spawn>: the budget
// plugin's numbers for a Game Master's status strip and spawn window (docs/GM_MODE.md "Plugin hooks")
static void GmMode_StatusCommand( void )
{
	if ( CMD_ARGC() < 6 )
	{
		g_engfuncs.pfnServerPrint( "usage: gm_status <name | #userid> <points (-1: no budget)> <tier> <seconds to next tier> <seconds to next spawn> [<max> <refill> <refill interval x10> <to next refill x10> <kill reward>]\n" );
		return;
	}
	int iMatches;
	CBasePlayer *pPlayer = GmFindPlayer( CMD_ARGV( 1 ), iMatches );
	if ( !pPlayer || !pPlayer->m_bGameMaster || s_Gm[pPlayer->entindex()].iState == GM_STATE_OFF )
	{
		g_engfuncs.pfnServerPrint( "gm_status: no such Game Master\n" );
		return;
	}
	GmPlayerState &gm = s_Gm[pPlayer->entindex()];
	// points below 0: no budget to show (the plugin's admin-free mode, or its economy off): the screens show no points,
	// tiers or costs, as without the plugin
	gm.iPoints = atoi( CMD_ARGV( 2 ) );
	gm.bEconomy = gm.iPoints >= 0;
	if ( !gm.bEconomy )
		gm.iPoints = 0;
	gm.iTier = max( 0, min( 3, atoi( CMD_ARGV( 3 ) ) ) );
	int iTier = atoi( CMD_ARGV( 4 ) ), iSpawn = atoi( CMD_ARGV( 5 ) );
	gm.flTierEnd = ( iTier > 0 ) ? gpGlobals->time + iTier : 0;
	gm.flSpawnEnd = ( iSpawn > 0 ) ? gpGlobals->time + iSpawn : 0;
	// the energy bar (gm_status_energy in gm_mode.inc): <max> <refill points> <refill interval> <to the next refill>
	// (tenths of a second) <kill reward just paid>; without them the points show as a number
	gm.iMaxPoints = CMD_ARGC() > 6 ? max( 0, atoi( CMD_ARGV( 6 ) ) ) : 0;
	gm.iRefill = CMD_ARGC() > 7 ? max( 0, atoi( CMD_ARGV( 7 ) ) ) : 0;
	gm.flRefillInterval = CMD_ARGC() > 8 ? max( 0, atoi( CMD_ARGV( 8 ) ) ) / 10.0f : 0;
	const int iRefillLeft = CMD_ARGC() > 9 ? max( 0, atoi( CMD_ARGV( 9 ) ) ) : 0;
	gm.flRefillEnd = iRefillLeft > 0 ? gpGlobals->time + iRefillLeft / 10.0f : 0;
	if ( CMD_ARGC() > 10 && atoi( CMD_ARGV( 10 ) ) > 0 )
		gm.iKillPop = atoi( CMD_ARGV( 10 ) );
	gm.flNextHud = 0;	// at once
}

//=========================================================
// Kills
//=========================================================
void GmMode_PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CBasePlayer *pOwner = NULL;

	if ( !GmMode_Allowed() || !pVictim )
		return;

	int iVictim = pVictim->entindex();
	if ( iVictim >= 1 && iVictim <= GM_MAX_PLAYERS )
		s_vecDeathPos[iVictim] = pVictim->pev->origin;	// the dead player's dot on the players window's map

	CBaseMonster *pMonster = GmOwnedMonster( pKiller, &pOwner );
	if ( !pMonster )
		pMonster = GmOwnedMonster( pInflictor, &pOwner );
	if ( !pMonster )
		return;
	if ( !pOwner || !pOwner->m_bGameMaster || GmMode_State( pOwner ) != GM_STATE_ACTIVE || pOwner == pVictim )
		return;

	// the players window's "Killed by your <monster>" (this round; GmMode_RoundStart clears it)
	if ( iVictim >= 1 && iVictim <= GM_MAX_PLAYERS && pMonster->m_iGmEntry >= 0 && pMonster->m_iGmEntry < GM_CATALOG_COUNT )
		s_iKilledBy[pOwner->entindex()][iVictim] = pMonster->m_iGmEntry + 1;

	// sma award_gm_kill_level: with the XP system on, a level (point) every two players the GM's monsters kill
	if ( XP.value < 1 )
		return;

	GmPlayerState &gm = s_Gm[pOwner->entindex()];
	// sma syndicate_gmmode_kills_per_level (1-100)
	const int iKillsPerLevel = (int)min( 100.0f, max( 1.0f, GmPluginFloat( "syndicate_gmmode_kills_per_level", GM_KILLS_PER_LEVEL ) ) );
	if ( ++gm.iKillProgress < iKillsPerLevel )
		return;
	gm.iKillProgress = 0;

	pOwner->AddPoints( 1, TRUE );

	hudtextparms_t params;
	memset( &params, 0, sizeof( params ) );
	params.x = -1;
	params.y = 0.25;
	params.r1 = params.g1 = params.b1 = 255;
	params.a1 = 255;
	params.holdTime = 1.5;
	params.channel = 3;
	UTIL_HudMessage( pOwner, params, "LEVEL UP" );
}

// the kill feed draws "<GM>" of "<GM>'s <monster>" in that player's colours, as a player killer's name
int GmMode_KillerOwner( entvars_t *pKiller, entvars_t *pInflictor )
{
	CBasePlayer *pOwner = NULL;
	if ( m_Mode != MODE_COOP )
		return 0;
	CBaseMonster *pMonster = GmOwnedMonster( pKiller, &pOwner );
	if ( !pMonster )
		pMonster = GmOwnedMonster( pInflictor, &pOwner );
	return ( pMonster && pOwner ) ? pOwner->entindex() : 0;
}

const char *GmMode_KillerName( entvars_t *pKiller, entvars_t *pInflictor, char *pszBuf, int iBufLen )
{
	CBasePlayer *pOwner = NULL;
	CBaseMonster *pMonster;

	if ( m_Mode != MODE_COOP )
		return NULL;

	pMonster = GmOwnedMonster( pKiller, &pOwner );
	if ( !pMonster )
		pMonster = GmOwnedMonster( pInflictor, &pOwner );
	if ( !pMonster || !pOwner || pMonster->m_iGmEntry < 0 || pMonster->m_iGmEntry >= GM_CATALOG_COUNT )
		return NULL;

	// sma send_killfeed_override: "<GM>'s <monster>"
	_snprintf( pszBuf, iBufLen, "%s's %s", STRING( pOwner->pev->netname ), g_GmCatalog[pMonster->m_iGmEntry].pszName );
	pszBuf[iBufLen - 1] = 0;
	return pszBuf;
}
