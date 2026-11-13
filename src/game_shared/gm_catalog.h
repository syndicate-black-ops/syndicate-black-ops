// SBO 2.0 Game Master mode: the spawn catalog, shared by the server (dlls/gmmode.cpp, which checks every pick
// against it) and the client's spawn window (cl_dll/vgui2/sbo_gm_spawn_menu.cpp, which lists it). docs/GM_MODE.md.
//
// The monsters, their order, tiers and loadouts are those of MrCalvin's Game Master Mode plugin
// (syndicate_gmmode.sma g_MonsterTemplates, assign_monster_budget, g_FedLoadoutVariants, g_ZombieLoadoutVariants);
// the names are the kill feed's (multiplay_gamerules.cpp s_SboKfNpcNames), which the GM kill names also use;
// its costs, unlock times and cooldowns are not part of SBO's GM mode. The tiers are only the window's groups.
//
// The client sends "gm_pick <entry> [<variant>]" (the indices below); the server takes it only from the active Game
// Master and only for a valid entry (and a valid loadout for an entry that has loadouts).
#pragma once

// window tabs (the plugin's menus: Tier I / II / III Monsters, Support: Weapons & Ammo, Teleporters)
#define GM_CAT_TIER1		0
#define GM_CAT_TIER2		1
#define GM_CAT_TIER3		2
#define GM_CAT_SUPPORT		3	// weapon / ammo drops (the plugin's Support > Weapons & Ammo)
#define GM_CAT_TELEPORTERS	4	// the plugin's Support > Teleporters
#define GM_CAT_COUNT		5

#define GM_KIND_MONSTER	0
#define GM_KIND_WEAPON	1
#define GM_KIND_TOOL	2

// tools (GM_KIND_TOOL: GmCatalogEntry::iTool)
#define GM_TOOL_TELEPORTERS	6	// the window's Teleporters tab; "gm_tp place | goto <pair> <0 entrance / 1 exit> |
								// remove <pair> | clear"

// placement orientation (the plugin's MONSTER_ORIENTATION_*; a turret's "orientation" key for floor and ceiling)
#define GM_ORIENT_DEFAULT	-1
#define GM_ORIENT_FLOOR		0
#define GM_ORIENT_CEILING	1
#define GM_ORIENT_WALL		2

// loadout groups (GmCatalogEntry::iVariants)
#define GM_VARIANTS_NONE	0
#define GM_VARIANTS_FED		1
#define GM_VARIANTS_ZOMBIE	2

struct GmVariant
{
	const char *pszName;
	const char *pszClass;	// the monster that spawns (the Fed's minigun and sniper are dark_grunt)
	int iWeapons;			// its "weapons" key
};

struct GmCatalogEntry
{
	int iKind;
	int iCategory;
	const char *pszName;
	const char *pszClass;	// monster classname (GM_KIND_MONSTER)
	int iOrientation;		// GM_ORIENT_DEFAULT or a fixed one (the plugin's TemplateOrientation)
	int iVariants;			// GM_VARIANTS_*
	int iTool;				// GM_TOOL_* (GM_KIND_TOOL)
	int iBody;				// GM_KIND_WEAPON: the item's body in models/gmmode.mdl
};

static const GmVariant g_GmFedVariants[] =
{
	{ "Minigun",			"dark_grunt",			3 },
	{ "Sniper Rifle",		"dark_grunt",			10 },
	{ "Federal Rifle",		"monster_grunt_hunter",	3 },
	{ "Saan Auto-Shotgun",	"monster_grunt_hunter",	10 },
	{ "Grenade Launcher",	"monster_grunt_hunter",	5 },
};

static const GmVariant g_GmZombieVariants[] =
{
	{ "MP5",	"monster_zombie_grunt",	3 },
	{ "SPA",	"monster_zombie_grunt",	10 },
};

// In the plugin's template order (the order inside each tier).
static const GmCatalogEntry g_GmCatalog[] =
{
	{ GM_KIND_MONSTER,	GM_CAT_TIER1,	"Fed",				"monster_grunt_hunter",		GM_ORIENT_DEFAULT,	GM_VARIANTS_FED,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER2,	"Zombie Grunt",		"monster_zombie_grunt",		GM_ORIENT_DEFAULT,	GM_VARIANTS_ZOMBIE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER1,	"Sectum",			"monster_dark",				GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER1,	"Demon Slave",		"monster_hunter",			GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER2,	"Imp",				"monster_imp_hunter",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER2,	"Werewolf",			"monster_werewolf_hunter",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER2,	"Zombie",			"monster_zombie_hunter",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER3,	"Cyber Minotaur",	"monster_cyberminotor",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER2,	"Cyborg",			"monster_cyborg",			GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER2,	"Assassin",			"monster_human_assassin",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER1,	"Cop",				"monster_barney",			GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER3,	"Apache",			"monster_apache",			GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER3,	"Gargantua",		"monster_gargantua",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER1,	"Pitbull",			"monster_snark",			GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER1,	"Scientist",		"monster_scientist",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER1,	"Ichthyosaur",		"monster_ichthyosaur",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER2,	"Mini Auto Turret",	"monster_miniturret",		GM_ORIENT_FLOOR,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER2,	"Auto Turret",		"monster_turret",			GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0 },
	{ GM_KIND_MONSTER,	GM_CAT_TIER2,	"Sentry Gun",		"monster_sentry",			GM_ORIENT_FLOOR,	GM_VARIANTS_NONE,	0 },

	// weapon and ammo drops: the plugin's Blades, Light weapons, Assault weapons and Equipment, in its order and names
	// (g_GMWeaponItems, g_iWeaponCategory*, g_szWeaponCategory*); the body is gm_get_weapon_preview_body's (the
	// merged model's manifest)
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Fusion Warhammer",		"weapon_warhammer",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	0 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Chainsaw Sword",		"weapon_chainsaw",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	1 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Energy Sword",			"weapon_sword",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	2 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Croon RC 28",			"weapon_rc28",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	3 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	".444k Bear killer",	"weapon_k444",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	4 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Canon of terror",		"weapon_canon",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	5 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Croon PM 66",			"weapon_pm66",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	6 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Croon FA18",			"weapon_fa18",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	7 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Vindico Federal",		"weapon_federal",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	8 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Saan Auto-Shotgun",	"weapon_spa",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	11 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Vindico Plasma Sniper",	"weapon_sw",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	9 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Croon NM 33 Sniper",	"weapon_nm33",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	10 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Benelli",				"weapon_ben",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	12 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Grenade Launcher",		"weapon_glauncher",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	13 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Avenger Minigun",		"weapon_chaingun",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	14 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Nuclear RPG",			"weapon_rpg",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	15 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Grenade",				"weapon_handgrenade",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	16 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Mine",					"weapon_tripmine",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	17 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Satchel",				"weapon_satchel",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	18 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Pitbull",				"weapon_snark",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	19 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Ammo Reserve",			"ammo_reserve",		GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	21 },
	{ GM_KIND_WEAPON,	GM_CAT_SUPPORT,	"Medikit",				"weapon_medikit",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	0,	20 },

	{ GM_KIND_TOOL,		GM_CAT_TELEPORTERS,	"Teleporters",		"",	GM_ORIENT_DEFAULT,	GM_VARIANTS_NONE,	GM_TOOL_TELEPORTERS },
};

#define GM_CATALOG_COUNT	( (int)( sizeof( g_GmCatalog ) / sizeof( g_GmCatalog[0] ) ) )

// The loadouts of a group; NULL / 0 for GM_VARIANTS_NONE.
inline const GmVariant *GmCatalog_Variants( int iGroup, int &iCount )
{
	switch ( iGroup )
	{
	case GM_VARIANTS_FED:
		iCount = (int)( sizeof( g_GmFedVariants ) / sizeof( g_GmFedVariants[0] ) );
		return g_GmFedVariants;
	case GM_VARIANTS_ZOMBIE:
		iCount = (int)( sizeof( g_GmZombieVariants ) / sizeof( g_GmZombieVariants[0] ) );
		return g_GmZombieVariants;
	}
	iCount = 0;
	return 0;
}

// "GmMode" user message (server -> the Game Master's client):
//   byte    state: GM_STATE_*
//   byte    event: GM_EVENT_*
//   byte    GM_EVENT_PLACE: the catalog entry being placed / used; GM_EVENT_OPEN_MENU: the entry whose tab / view
//           the window opens on; else 255
//   byte    teleporters: the Game Master's complete pairs (GM_TP_PAIRS_MASK), GM_TP_ENTRANCE_PENDING if an entrance
//           waits for its exit
//   string  GM_EVENT_PLACE: what is being placed (the hint line, with the loadout), else ""
#define GM_STATE_OFF		0
#define GM_STATE_INTRO		1	// the intro window is open (the screen behind it is black)
#define GM_STATE_ACTIVE		2

#define GM_EVENT_NONE		0
#define GM_EVENT_OPEN_MENU	1	// open the spawn window (a placement was cancelled)
#define GM_EVENT_PLACE		2	// a placement started: show the hint
#define GM_EVENT_PLACE_END	3	// the placement ended: hide the hint
#define GM_EVENT_OPEN_PLAYERS	4	// open the players window (quick menu 3); entry: the player index to highlight
#define GM_EVENT_QUICK_MENU	5	// a GM text menu went up (the client's GM key then opens the spawn window: "K K")

#define GM_TP_PAIRS_MASK		0x7f
#define GM_TP_ENTRANCE_PENDING	0x80
#define GM_TP_MAX_PAIRS			64	// sma GM_TELEPORTER_MAX_PAIRS

// Plugin hooks (dlls/gmmode.cpp, docs/GM_MODE.md "Plugin hooks"): a "pick" names a catalog entry and its loadout in
// one number, as CBasePlayer::GmCanPlace / GmPlaced and the gm_entry_text server command take it:
//   pick = entry | ( variant + 1 ) << 8      variant: the loadout (GmCatalog_Variants), -1 for none; for the
//                                            Teleporters entry GM_TP_ROLE_ENTRANCE / GM_TP_ROLE_EXIT
#define GM_PICK( entry, variant )	( ( ( entry ) & 0xff ) | ( ( ( variant ) + 1 ) & 0xff ) << 8 )
#define GM_PICK_ENTRY( pick )		( ( pick ) & 0xff )
#define GM_PICK_VARIANT( pick )		( ( ( ( pick ) >> 8 ) & 0xff ) - 1 )
#define GM_TP_ROLE_ENTRANCE			0
#define GM_TP_ROLE_EXIT				1
#define GM_PICK_MAX_VARIANT			5	// the most loadouts an entry has (the Fed); variants 0..4

// Card texts a plugin sets (gm_entry_text) per pick:
//   price    the cost in points, a number ("12"; the card shows "12 pts")
//   status   a lock: with <seconds> a countdown (the card's tag "[s]s"), else a reason ("Need 8 pts"); also the reason
//            given when the plugin refuses the pick
//   tier     the tier the pick waits for ("2": the card is locked, tag "Tier 2")
//   killpay  points a kill by this monster pays ("7": "Kills pay you 7 pts")
//   cooldown the tier's cooldown after a spawn, in seconds ("12": "cooldown 12s")
#define GM_TEXT_PRICE		0
#define GM_TEXT_STATUS		1
#define GM_TEXT_TIER		2
#define GM_TEXT_KILLPAY		3
#define GM_TEXT_COOLDOWN	4
#define GM_TEXT_SLOTS		5
#define GM_TEXT_LEN			32	// with the terminating 0
#define GM_TEXT_TOKEN		"{t}"	// replaced by the whole seconds left of a countdown

// "GmEntry" user message (server -> that Game Master's client, on change only):
//   byte    entry
//   byte    variant + 1
//   byte    slot: GM_TEXT_*
//   short   seconds: a countdown; the line goes away when it runs out (0: no countdown)
//   string  text ("" clears the line)

// Roaming speed of a Game Master without the plugin (its syndicate_gmmode_spectator_speed default), and the pmove->iuser4 bit
// that marks a roaming Game Master for pm_shared (server: pev->iuser4; client prediction: clientdata iuser4).
#define GM_ROAM_SPEED		2000
#define GM_IUSER4_BIT		1

// "GmStatus" user message (server -> that Game Master's client, when it changed): the status strip and the spawn
// window's header. Without a plugin (gm_status never sent) flags has no GM_STATUS_ECONOMY and only alive / selected
// count.
//   byte    flags: GM_STATUS_*
//   long    points
//   byte    tier (1-3, 0 none)
//   short   seconds to the next tier (0 none)
//   short   seconds to the next spawn (0 none)
//   byte    alive: this Game Master's living monsters
//   byte    selected: monsters in the selection
//   short   roaming speed for the client's prediction (pm_shared PM_GmRoamSpeed; 0: sv_spectatormaxspeed)
//   short   the energy bar's maximum points (0: no bar, the points as a number)
//   byte    points of the next refill
//   short   seconds between refills x 10
//   short   seconds to the next refill x 10 (0 none)
//   short   a kill reward just paid ("+N" over the bar; 0 none)
#define GM_STATUS_ECONOMY	1	// a plugin sends the points and tiers
#define GM_STATUS_FOLLOWING	2	// the Game Master follows a player (chase or first person)

// "GmPlayers" user message (server -> that Game Master's client, every GM_PLAYERS_INTERVAL while the players
// window or the nearest players need it): the players in play (no spectators, nobody without a loadout, no Game
// Master), in parts of at most GM_PLAYERS_PER_MSG rows:
//   byte    part flags: GM_PLAYERS_FIRST (a new list starts), GM_PLAYERS_LAST (the list is complete)
//   byte    rows
//   rows x: byte index, byte flags (GM_PLAYER_*), byte health (0-255), short distance (meters), short x, short y,
//           short z (units), byte weapon (catalog entry, 255 none), byte killed by (catalog entry of this Game
//           Master's monster that killed him this round, 255 none)
#define GM_PLAYERS_INTERVAL	0.5
#define GM_PLAYERS_PER_MSG	12
#define GM_PLAYERS_FIRST	1
#define GM_PLAYERS_LAST		2
#define GM_PLAYER_ALIVE		1

// "GmMonsters" user message (server -> that Game Master's client, with GmPlayers, while his players window is open:
// "gm_mapwatch 1"): his living monsters for the window's map, in parts of at most GM_MONSTERS_PER_MSG rows:
//   byte    part flags: GM_PLAYERS_FIRST, GM_PLAYERS_LAST (as GmPlayers)
//   byte    rows
//   rows x: short entity, short x, short y, short z, byte entry (255 none), byte health (percent), byte flags
//           (GM_MONSTER_*)
#define GM_MONSTERS_MAX			64
#define GM_MONSTERS_PER_MSG		16
#define GM_MONSTER_SELECTED		1
#define GM_MONSTER_ORDERABLE	2
#define GM_PIP_RANGE			1536	// the players window's View: what is this near its player is sent to the GM

// "GmNotice" user message (server or a plugin -> one player): a short line under the crosshair, gone after
// GM_NOTICE_TIME seconds; a refusal or an action's feedback for a Game Master (or a player who asked to be one):
//   string  text (at most GM_NOTICE_LEN - 1 characters)
#define GM_NOTICE_TIME		2.0
#define GM_NOTICE_LEN		96

// the monsters' models (the server's preview, the client's cards; sma g_MonsterModelMap)
struct GmModel
{
	const char *pszClass;
	const char *pszModel;
};

static const GmModel g_GmModels[] =
{
	{ "monster_grunt_hunter",		"models/hgrunt.mdl" },
	{ "dark_grunt",					"models/dgrunt.mdl" },
	{ "monster_zombie_grunt",		"models/zgrunt.mdl" },
	{ "monster_dark",				"models/dark.mdl" },
	{ "monster_hunter",				"models/islave.mdl" },
	{ "monster_imp_hunter",			"models/imp.mdl" },
	{ "monster_werewolf_hunter",	"models/werewolf.mdl" },
	{ "monster_zombie_hunter",		"models/zombie.mdl" },
	{ "monster_cyberminotor",		"models/minos.mdl" },
	{ "monster_cyborg",				"models/robot.mdl" },
	{ "monster_human_assassin",		"models/hassassin.mdl" },
	{ "monster_barney",				"models/barney.mdl" },
	{ "monster_apache",				"models/apache.mdl" },
	{ "monster_gargantua",			"models/garg.mdl" },
	{ "monster_snark",				"models/w_squeak.mdl" },
	{ "monster_scientist",			"models/scientist.mdl" },
	{ "monster_ichthyosaur",		"models/icky.mdl" },
	{ "monster_miniturret",			"models/miniturret.mdl" },
	{ "monster_turret",				"models/turret.mdl" },
	{ "monster_sentry",				"models/sentry.mdl" },
};

#define GM_DISPLAY_MODEL_PATH	"models/gmmode.mdl"	// weapon drops: the item's body (GmCatalogEntry::iBody); 23 the pad

inline const char *GmCatalog_ModelForClass( const char *pszClass )
{
	for ( unsigned i = 0; i < sizeof( g_GmModels ) / sizeof( g_GmModels[0] ); i++ )
	{
		const char *a = g_GmModels[i].pszClass, *b = pszClass;
		while ( *a && *a == *b )
		{
			a++;
			b++;
		}
		if ( !*a && !*b )
			return g_GmModels[i].pszModel;
	}
	return 0;
}

// A Fed loadout's look (the placement preview and the spawn window's models): body and skin of monster_grunt_hunter /
// dark_grunt for its "weapons" key (-1 / -1: the class decides). Its head: 4 the gas mask (skin 1), 8 the beret;
// its gun: dark_grunt 3 minigun, 10 sniper; monster_grunt_hunter 1/3/5 Federal (5 with the GL), 8/10 Vindico.
inline bool GmCatalog_StrEq( const char *a, const char *b )
{
	while ( *a && *a == *b )
	{
		a++;
		b++;
	}
	return *a == *b;
}

inline void GmCatalog_LoadoutLook( const char *pszClass, int iWeapons, int &iBody, int &iSkin )
{
	iBody = -1;
	iSkin = -1;
	if ( iWeapons == -1 || !( GmCatalog_StrEq( pszClass, "monster_grunt_hunter" ) || GmCatalog_StrEq( pszClass, "dark_grunt" ) ) )
		return;
	int iHead = ( iWeapons & 4 ) ? 3 : ( ( iWeapons & 8 ) ? 2 : 0 );
	int iGun = -1;
	if ( GmCatalog_StrEq( pszClass, "dark_grunt" ) )
	{
		if ( iWeapons == 3 )
			iGun = 0;
		else if ( iWeapons == 10 )
			iGun = 3;
	}
	else
	{
		if ( iWeapons == 1 || iWeapons == 3 || iWeapons == 5 )
			iGun = 0;
		else if ( iWeapons == 8 || iWeapons == 10 )
			iGun = 1;
	}
	if ( iGun != -1 )
		iBody = iHead + iGun * 4;
	if ( iWeapons & 4 )
		iSkin = 1;
}
