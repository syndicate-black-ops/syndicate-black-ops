//========= SBO VGUI2: the new loadout menu =========//
//
// Purpose: see sbo_loadout_menu.h. Every size below is the design's pixel on its 1440 x 900 page (the window 1320 x
//          800 at 60, 50), scaled by the screen (the window keeps the design's share of the screen).
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <ctype.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <FileSystem.h>
#include <KeyValues.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/EditablePanel.h>

#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_image_decode.h"
#include "sbo_loadout_menu.h"
#include "sbo_menus.h"
#include "sbo_server_browser.h" // CSboBrowserEntry
#include "sbo_ui_style.h"
#include "sbo_widgets.h"

using namespace sbolook;

namespace
{
//-----------------------------------------------------------------------------
// The design's colors
//-----------------------------------------------------------------------------
const Color C_PAGE(7, 10, 8, 255);           // #070a08
const Color C_WINDOW(10, 14, 11, 245);       // rgba(10,14,11,0.96)
const Color C_BORDER(46, 74, 48, 255);       // #2e4a30
const Color C_LINE(31, 50, 33, 255);         // #1f3221
const Color C_GREEN(143, 220, 122, 255);     // #8fdc7a
const Color C_TEXT(223, 232, 223, 255);      // #dfe8df
const Color C_TEXT_HI(232, 240, 232, 255);   // #e8f0e8
const Color C_DIM(138, 155, 139, 255);       // #8a9b8b
const Color C_SUB(159, 176, 160, 255);       // #9fb0a0
const Color C_SOFT(184, 198, 184, 255);      // #b8c6b8
const Color C_PICK(196, 209, 196, 255);      // #c4d1c4
const Color C_MODEV(201, 214, 201, 255);     // #c9d6c9
const Color C_FIELD(17, 25, 18, 255);        // #111912
const Color C_RAIL_ON(21, 32, 22, 255);      // #152016
const Color C_CARD(14, 20, 15, 255);         // #0e140f
const Color C_CARD_ON(19, 32, 21, 255);      // #132015
const Color C_CARD_LINE(34, 52, 36, 255);    // #223424
const Color C_IMG_BG(11, 16, 12, 255);       // #0b100c
const Color C_CHIP_LINE(44, 63, 45, 255);    // #2c3f2d
const Color C_ASIDE(12, 18, 13, 255);        // #0c120d
const Color C_PIC_BG(8, 12, 9, 255);         // #080c09
const Color C_BAR_BG(26, 38, 27, 255);       // #1a261b
const Color C_BOTTOM(10, 15, 11, 255);       // #0a0f0b
const Color C_SLOT_LINE(36, 58, 38, 255);    // #243a26
const Color C_DEPLOY(47, 107, 42, 255);      // #2f6b2a
const Color C_DEPLOY_LINE(95, 174, 82, 255); // #5fae52
const Color C_DEPLOY_TEXT(240, 255, 240, 255);
const Color C_TIMER_BG(18, 26, 16, 255);    // #121a10
const Color C_TIMER_LINE(60, 90, 42, 255);  // #3c5a2a
const Color C_GOLD(232, 193, 90, 255);      // #e8c15a
const Color C_GOLD_BG(28, 26, 16, 255);     // #1c1a10
const Color C_GOLD_LINE(107, 90, 34, 255);  // #6b5a22
const Color C_GOLD_TEXT(232, 217, 160, 255); // #e8d9a0
const Color C_LIST_BG(13, 20, 14, 255);     // #0d140e
const Color C_LIST_LINE(60, 90, 62, 255);   // #3c5a3e
const Color C_LIST_ON(26, 43, 27, 255);     // #1a2b1b
const Color C_EQUIP_TEXT(10, 18, 10, 255);  // #0a120a
const Color C_SCROLL_TRACK(11, 16, 12, 255); // #0b100c
const Color C_SCROLL_THUMB(60, 90, 62, 255); // #3c5a3e
const Color C_ZOOM_BG(3, 5, 4, 224);        // rgba(3,5,4,0.88)

//-----------------------------------------------------------------------------
// The items (the design's data; the numbers from the items' descriptions, #SBO_Item_<token>)
//-----------------------------------------------------------------------------
struct Line
{
	const char *k, *v;
	float bar = -1.0f; //!< 0..1: a stat bar under the line (armor, equipment); < 0: text only
};

struct Item
{
	const char *token; //!< #SBO_Item_<token> (the original text), as CSboBriefMenu
	const char *image; //!< gfx/vgui/640_<image>.tga
	const char *name;
	const char *info; //!< the one-line summary
	bool weapon;
	// weapons
	const char *dmg;
	int dmax, clip; //!< clip 0: none
	const char *rate;
	float rps;
	int speed; //!< percent, 0 = no penalty
	std::vector<Line> modes;
	// everything else
	std::vector<Line> rows;
	std::vector<const char *> chips;
};

Item W(const char *token, const char *image, const char *name, const char *info, const char *dmg, int dmax, int clip,
    const char *rate, float rps, int speed, std::vector<Line> modes = {})
{
	Item it{ token, image, name, info, true, dmg, dmax, clip, rate, rps, speed, modes, {}, {} };
	return it;
}

Item G(const char *token, const char *image, const char *name, const char *info, std::vector<Line> rows,
    std::vector<const char *> chips)
{
	Item it{ token, image, name, info, false, "", 0, 0, "", 0.0f, 0, {}, rows, chips };
	return it;
}

struct Slot
{
	int choice;        //!< sbo::BriefChoice
	const char *title; //!< rail / bottom bar
	const char *heading;
	std::vector<Item> items;
};

// The cyberware's speed bonus at the spawn, [0] co-op (coop.cpp), [1] Bounty Killer (dm.cpp); a level 0 player's speed
constexpr int SPEED_LEGS[2] = { 50, 150 }, SPEED_ASSASSIN[2] = { 25, 300 };
constexpr int SBO_NORMAL_SPEED = 300; // a level 0 player's speed (multiplay_gamerules.cpp InitHUD: m_iSboSpeed 300)

//! The speed bonus of cyber legs / Assassin on this map's mode, 0 for other items
int SpeedBonus(const char *token)
{
	const int dm = sbo::IsBountyMap() ? 1 : 0;
	if (!strcmp(token, "legs"))
		return SPEED_LEGS[dm];
	if (!strcmp(token, "assassin"))
		return SPEED_ASSASSIN[dm];
	return 0;
}

//! "+50 (+17%)": the bonus and its share of a level 0 player's speed
std::wstring SpeedBonusText(int bonus, bool chip)
{
	char buf[48];
	const int pct = (int)lroundf(bonus * 100.0f / SBO_NORMAL_SPEED);
	if (chip)
		snprintf(buf, sizeof(buf), "+%d%% speed", pct);
	else
		snprintf(buf, sizeof(buf), "+%d (+%d%%)", bonus, pct);
	return Widen(buf);
}

// CSyndBrief's order (and the numbers sent: item n = the n-th of its slot)
const std::vector<Slot> &Slots()
{
	static const std::vector<Slot> s = {
		{ sbo::BRIEF_BLADE, "BLADE", "Blades",
		    { W("hammer", "hammer", "Fusion warhammer", "Releases a plasma blast on hit. A force shield protects you while you fight", "280\xe2\x80\x93" "516", 516, 0, "1 / s", 1, -10),
		        W("chainsaw", "chainsaw", "Chainsaw sword", "Toothed blade used by federal troops and bounty hunters. Force shield while you fight", "44\xe2\x80\x93" "110", 110, 0, "2 / s", 2, 0),
		        W("sword", "sword", "Energy sword", "Its energy field cuts through almost anything. Force shield while you fight", "222\xe2\x80\x93" "280", 280, 0, "1.4 / s", 1.4f, -5) } },
		{ sbo::BRIEF_LIGHT, "SIDEARM", "Light weapons",
		    { W("rc28", "rc28", "Croon RC 28", "Small and hard hitting, every killer\xe2\x80\x99s favorite", "26\xe2\x80\x93" "41", 41, 30, "12 / s \xc2\xb7 12 / s", 12, 0, { { "Primary", "Semi-auto" }, { "Secondary", "Full auto" } }),
		        W("k444", "k444", ".444k Bear killer", "Large-bore pistol with the most powerful handgun round", "150\xe2\x80\x93" "270", 270, 4, "Single", 1, 0),
		        W("canon", "canon", "Canon of terror", "Sawn-off shotgun, 6 pellets a barrel", "88\xe2\x80\x93" "122 \xc3\x97" "6", 732, 2, "2 / s", 2, -5, { { "Primary", "One barrel" }, { "Secondary", "Both barrels" } }),
		        W("pm66", "pm66", "Croon PM 66", "Sub machine gun for a very high rate of fire", "20\xe2\x80\x93" "37", 37, 50, "25 / s \xc2\xb7 100 / s", 25, -5, { { "Primary", "Full auto" }, { "Secondary", "Full auto, high speed" } }) } },
		{ sbo::BRIEF_HEAVY, "PRIMARY", "Assault weapons",
		    { W("fa18", "fa18", "Croon FA 18", "Assault rifle with an under-barrel shotgun", "26\xe2\x80\x93" "43", 43, 50, "16 / s", 16, -10, { { "Primary", "Full auto" }, { "Secondary", "Shotgun, 4 pellets" } }),
		        W("federal", "federal", "Vindico Federal", "Standard rifle of the federal troops", "30\xe2\x80\x93" "52", 52, 30, "20 / s", 20, -7, { { "Primary", "Full auto" }, { "Secondary", "Zoom" } }),
		        W("sw", "sw", "Vindico plasma sniper", "Faster rounds when zoomed; unzoomed shots can rebound", "88\xe2\x80\x93" "172", 172, 5, "1\xe2\x80\x93" "2 / s", 1.5f, -15, { { "Primary", "Plasma shot" }, { "Secondary", "Zoom" } }),
		        W("nm33", "nm", "Croon NM 33 sniper", "Semi-auto sniper rifle for murder and assault", "38\xe2\x80\x93" "60", 60, 30, "10 / s", 10, -10, { { "Primary", "Semi-auto" }, { "Secondary", "Zoom" } }),
		        W("spa", "spaauto", "Saan auto-shotgun", "Military shotgun for indoor and urban fights", "30\xe2\x80\x93" "45 \xc3\x97" "4", 180, 8, "1.3 / s", 1.3f, -7, { { "Primary", "4 pellets" }, { "Secondary", "8 pellets" } }),
		        W("ben", "ben", "Benelli", "Old and roughly tested shotgun, 8 pellets", "88\xe2\x80\x93" "122 \xc3\x97" "8", 976, 8, "1.3 / s", 1.3f, -7),
		        W("glauncher", "glauncher", "Vindico grenade launcher", "Full-power or high-arc grenades", "230", 230, 2, "2 / s", 2, -20, { { "Primary", "Flat, full power" }, { "Secondary", "High arc" } }),
		        W("chaingun", "chaingun", "Vindico Avenger minigun", "Vehicle gun carried by hand", "12\xe2\x80\x93" "25", 25, 500, "75 / s", 75, -27),
		        W("lr", "lr", "Nuclear rocket launcher", "Nuclear rocket that wipes out a small area", "250\xe2\x80\x93" "400", 400, 1, "Single", 1, -27, { { "Primary", "Rocket" }, { "Secondary", "Laser guidance on / off" } }) } },
		{ sbo::BRIEF_CYBERWARE, "CYBERWARE", "Cyber tech",
		    // "Speed": SpeedBonus, drawn by the mode (the row's text and the chip are made there)
		    { G("legs", "legs", "Cyber legs", "Modified muscles and cybernetic nerves", { { "Speed", "", 0.0f }, { "Effect", "Faster movement" } }, { "Speed" }),
		        G("targeting", "targeting", "Targeting system", "Brain implant linked to your weapon", { { "Effect", "Double weapon damage" } }, { "Double damage" }), // the server's m_bTargeting (combat.cpp, player.cpp)
		        G("medikit", "medikit", "Medikit", "Heal your team and yourself", { { "Primary", "Heal a teammate" }, { "Secondary", "Heal yourself and your legs" } }, { "Healing" }),
		        // coop.cpp and dm.cpp give m_iSboHealth + 75, as #SBO_Item_adrenaline says
		        G("adrenaline", "adrenaline", "Adrenaline surge", "A pocket full of adrenaline", { { "Effect", "+75 health" } }, { "+75 HP" }),
		        // coop.cpp ~1777 / dm.cpp ~631: light, heavy and item cleared, then the kit
		        G("assassin", "assassin", "Assassin", "A full kit that replaces your sidearm, primary, equipment and armor. Your blade and agent stay your choice",
		            { { "Weapons", "Your blade, .444k" }, { "Gear", "Medikit, grenades, satchels" }, { "Cyber", "Cloak, night vision, double damage" }, { "Body", "Armor skin 50, +50 health, faster" }, { "Speed", "", 0.0f } }, { "Full kit" }),
		        G("eye", "eye", "Cyber eyes", "Implants that see in the dark", { { "Effect", "Night vision" } }, { "Night vision" }) } },
		{ sbo::BRIEF_ITEM, "EQUIPMENT", "Equipment",
		    // damage bars on a 0-300 scale; mobility as the texts say (satchels 300 - 15 in coop.cpp; mines no penalty,
		    // fix/armor-values)
		    { G("grenade", "grenade", "Grenades", "The longer you hold it, the sooner it explodes", { { "Damage", "250", 250 / 300.0f }, { "Count", "2" } }, { "250 dmg", "\xc3\x97" "2" }),
		        G("tripmine", "tripmine", "Mines", "Goes off when someone walks past", { { "Damage", "280", 280 / 300.0f }, { "Mobility", "No penalty", 1.0f } }, { "280 dmg" }),
		        G("satchel", "satchel", "Satchels", "Remote detonation for a big area", { { "Damage", "200\xe2\x80\x93" "300", 1.0f }, { "Mobility", "-5%", 0.95f } }, { "200\xe2\x80\x93" "300 dmg" }),
		        G("pit", "pit", "Pitbull", "A dog that fights at your side", { { "Effect", "Attack dog" } }, { "Companion" }) } },
		{ sbo::BRIEF_ARMOR, "ARMOR", "Armor",
		    // the items' own texts (the server matches them; armor
		    // values and the speeds of 300 lowered by 30 / 15 / 30 are coop.cpp's)
		    { G("skin", "skin", "Armor skin", "Under-skin armor, protects all but the head", { { "Armor", "50", 0.5f }, { "Absorbs", "60%", 0.6f }, { "Mobility", "-10%", 0.9f } }, { "50", "60%" }),
		        G("torso", "torso", "Cyber torso armor", "Titanium torso, stomach and back", { { "Armor", "100", 1.0f }, { "Absorbs", "50%", 0.5f }, { "Mobility", "-5%", 0.95f } }, { "100", "50%" }),
		        G("skull", "skull", "Torso + skull armor", "Titanium torso and skull", { { "Armor", "100", 1.0f }, { "Absorbs", "70%", 0.7f }, { "Mobility", "-10%", 0.9f } }, { "100", "70%" }),
		        G("noarmor", "noarmor", "No armor", "Full speed, no protection", { { "Armor", "0", 0.0f }, { "Absorbs", "0%", 0.0f }, { "Mobility", "No penalty", 1.0f } }, { "Full speed" }) } },
		{ sbo::BRIEF_MALEFEMALE, "AGENT", "Who are you",
		    { G("female", "female", "Female agent", "Welcome sister. Glory to Syndicate", { { "Model", "Female" } }, {}),
		        G("male", "male", "Male agent", "Welcome brother. Glory to Syndicate", { { "Model", "Male" } }, {}) } },
	};
	return s;
}

constexpr int NUM_SLOTS = 7;
constexpr int SLOT_CYBER = 3, ASSASSIN = 4;
constexpr int SLOT_PRIMARY = 2;

//! A primary the team limit makes full for this player (server "HeavyLimit": the others hold all of them; coop.cpp
//! CanTakeHeavyWeapon refuses it). item: the primary's index (the "heavy" command's value - 1).
bool HeavyFull(int item, int *pTaken = nullptr, int *pMax = nullptr)
{
	int taken, max;
	if (!sbo::GetHeavyLimit(item + 1, taken, max))
		return false;
	if (pTaken)
		*pTaken = taken;
	if (pMax)
		*pMax = max;
	return taken >= max;
}

//! The primary to pick instead of a full one, as the server does: a refused "heavy" leaves the loadout as it was
//! (previous); if that is full too, the first one that is not (the FA 18 has no limit)
int FreeHeavy(int wanted, int previous)
{
	if (!HeavyFull(wanted))
		return wanted;
	if (previous >= 0 && !HeavyFull(previous))
		return previous;
	const int n = (int)Slots()[SLOT_PRIMARY].items.size();
	for (int i = 0; i < n; i++)
	{
		if (!HeavyFull(i))
			return i;
	}
	return 0;
}
//! What Assassin gives in each slot it sets (the design's LOCK): sidearm, primary, equipment, armor
const char *AssassinSets(int slot)
{
	switch (slot)
	{
	case 1: return ".444k Bear killer";
	case 2: return "None";
	case 4: return "Grenades + satchels";
	case 5: return "Armor skin";
	default: return nullptr;
	}
}

//-----------------------------------------------------------------------------
// The whole loadout's numbers, as the server's spawn gives them (coop.cpp CCoop::PlayerSpawn's loadout switches;
// Bounty Killer: dm.cpp CDm's). Slot order and item numbers as Slots() (the server's value is the item + 1).
//-----------------------------------------------------------------------------
struct LoadoutStats
{
	int speed = SBO_NORMAL_SPEED; //!< the spawn's max speed (m_iSboMaxSpeed)
	bool levelKnown = false;      //!< co-op: the scoreboard's level is in the speed
	int level = 0;
	int health = 100;             //!< spawn health 100 (m_iSboHealth's default) plus the cyberware's
	int healthBonus = 0;          //!< adrenaline +75, Assassin +50 (Bounty Killer: Assassin sets 150)
	int armor = 0, absorb = 0;    //!< armor points, percent of the damage the armor takes on the parts it covers
	int damage = 1;               //!< x2: targeting system or Assassin (m_bTargeting, player.cpp TakeDamage)
	bool nightVision = false, cloak = false, medikit = false;
};

LoadoutStats ComputeStats(const int *sel)
{
	LoadoutStats s;
	const bool dm = sbo::IsBountyMap();
	const int cyber = sel[3];
	const bool assassin = cyber == 4;

	// the level speed: co-op levels are the scoreboard's score, (m_iSboSpeed - 300) / 5; below level -40 the speed
	// stays at level -40's (player.cpp SboLevelSpeed). Bounty Killer starts from m_iSboSpeed (300).
	if (!dm && sbo::IsCoopMap())
	{
		sbo::ScorePlayer me;
		sbo::GetScorePlayer(sbo::GetLocalPlayerIndex(), me, false);
		if (me.connected)
		{
			s.levelKnown = true;
			s.level = me.frags;
			s.speed = std::max(100, SBO_NORMAL_SPEED + 5 * s.level);
		}
	}

	// armor (switch on m_iLoadoutArmor): with Assassin the armor skin it gives, whatever the armor pick
	static const int armorWeight[4] = { 30, 15, 30, 0 }, armorPoints[4] = { 50, 100, 100, 0 }, armorAbsorb[4] = { 60, 50, 70, 0 };
	const int armorPick = assassin ? 0 : sel[5];
	s.speed -= armorWeight[armorPick];
	s.armor = armorPoints[armorPick];
	s.absorb = armorAbsorb[armorPick];

	// cyberware (m_iLoadoutCyberware)
	switch (cyber)
	{
	case 0: s.speed += SPEED_LEGS[dm ? 1 : 0]; break; // cyber legs
	case 1: s.damage = 2; break;                        // targeting system
	case 2: s.medikit = true; break;
	case 3: s.healthBonus = 75; break;                  // adrenaline surge
	case 4:                                             // Assassin: sidearm, primary and equipment cleared, its kit
		s.damage = 2;
		s.cloak = s.nightVision = s.medikit = true;
		s.speed += SPEED_ASSASSIN[dm ? 1 : 0];
		s.healthBonus = 50;                             // co-op m_iSboHealth + 50; Bounty Killer sets 150 (below)
		break;
	case 5: s.nightVision = true; break;                // cyber eyes
	}

	if (!assassin)
	{
		if (sel[4] == 2) // satchels
			s.speed -= 15;
		if (sel[1] == 2 || sel[1] == 3) // canon of terror, PM 66
			s.speed -= 15;
		static const int heavyWeight[9] = { 30, 20, 45, 30, 20, 20, 60, 80, 80 };
		s.speed -= heavyWeight[sel[2]];
	}

	static const int bladeWeight[3] = { 30, 0, 15 };
	s.speed -= bladeWeight[sel[0]];

	// co-op: never below SBO_MIN_MAXSPEED (player.h); Bounty Killer has no floor
	if (!dm && s.speed < 80)
		s.speed = 80;

	s.health = (dm && assassin) ? 150 : 100 + s.healthBonus;
	return s;
}

//-----------------------------------------------------------------------------
// Saved loadouts: loadouts.txt in the mod folder (KeyValues), the last one deployed and the named ones
//-----------------------------------------------------------------------------
constexpr const char LOADOUTS_FILE[] = "loadouts.txt";
constexpr const char LOADOUTS_PATH_ID[] = "GAMECONFIG"; // the mod folder, where config.cfg is written

struct Preset
{
	std::string name;
	int sel[NUM_SLOTS];
};

std::string SelText(const int *sel)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%d %d %d %d %d %d %d", sel[0], sel[1], sel[2], sel[3], sel[4], sel[5], sel[6]);
	return buf;
}

bool ParseSel(const char *text, int *sel)
{
	int v[NUM_SLOTS];
	if (!text || sscanf(text, "%d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]) != NUM_SLOTS)
		return false;
	for (int i = 0; i < NUM_SLOTS; i++)
	{
		if (v[i] < 0 || v[i] >= (int)Slots()[i].items.size())
			return false;
		sel[i] = v[i];
	}
	return true;
}

//-----------------------------------------------------------------------------
// Item pictures: the 320x320 TGA decoded once, then cut (cover) or fitted (contain) and scaled to the drawn size on
// the CPU (area average), one texture per item, size and kind
//-----------------------------------------------------------------------------
struct Picture
{
	bool tried = false;
	int w = 0, h = 0;
	std::vector<uint8_t> rgba;
};

const Picture &LoadPicture(const char *image)
{
	static std::map<std::string, Picture> s_Pictures;
	Picture &p = s_Pictures[image];
	if (p.tried)
		return p;
	p.tried = true;
	char path[96];
	snprintf(path, sizeof(path), "gfx/vgui/640_%s.tga", image);
	FileHandle_t fh = g_pFullFileSystem->Open(path, "rb");
	if (!fh)
		return p;
	std::vector<uint8_t> file;
	const unsigned size = g_pFullFileSystem->Size(fh);
	if (size > 0 && size <= sbo_image::MAX_FILE_BYTES)
	{
		file.resize(size);
		if (g_pFullFileSystem->Read(file.data(), (int)size, fh) != (int)size)
			file.clear();
	}
	g_pFullFileSystem->Close(fh);
	if (!file.empty())
		sbo_image::DecodeTgaScaled(file.data(), file.size(), 1024, 1024, p.rgba, p.w, p.h);
	return p;
}

//! Texture of the picture: cover = the part of it with the box's aspect, centred; else the whole picture. Mixed with bg
//! by alpha (the card's 0.92 opacity), opaque.
int PictureTexture(const char *image, int w, int h, bool cover, float alpha, Color bg)
{
	struct Key
	{
		std::string image;
		int w, h;
		bool cover;
		int alpha;
		bool operator<(const Key &o) const
		{
			if (image != o.image)
				return image < o.image;
			if (w != o.w)
				return w < o.w;
			if (h != o.h)
				return h < o.h;
			if (cover != o.cover)
				return cover < o.cover;
			return alpha < o.alpha;
		}
	};
	static std::map<Key, int> s_Textures;
	if (w <= 0 || h <= 0 || w > 2048 || h > 2048)
		return -1;
	const Key key{ image, w, h, cover, (int)(alpha * 100) };
	auto found = s_Textures.find(key);
	if (found != s_Textures.end())
		return found->second;
	const Picture &p = LoadPicture(image);
	int texture = -1;
	if (p.w > 0 && p.h > 0)
	{
		// the source rectangle
		float sx0 = 0, sy0 = 0, sw = (float)p.w, sh = (float)p.h;
		if (cover)
		{
			const float boxAspect = (float)w / h, picAspect = (float)p.w / p.h;
			if (boxAspect > picAspect)
			{
				sh = p.w / boxAspect;
				sy0 = (p.h - sh) * 0.5f;
			}
			else
			{
				sw = p.h * boxAspect;
				sx0 = (p.w - sw) * 0.5f;
			}
		}
		std::vector<uint8_t> out((size_t)w * h * 4);
		for (int ty = 0; ty < h; ty++)
		{
			const float fy0 = sy0 + sh * ty / h, fy1 = sy0 + sh * (ty + 1) / h;
			for (int tx = 0; tx < w; tx++)
			{
				const float fx0 = sx0 + sw * tx / w, fx1 = sx0 + sw * (tx + 1) / w;
				float r = 0, g = 0, b = 0, area = 0;
				for (int py = (int)fy0; py < (int)ceilf(fy1) && py < p.h; py++)
					for (int px = (int)fx0; px < (int)ceilf(fx1) && px < p.w; px++)
					{
						const float a = (std::min(fx1, (float)px + 1) - std::max(fx0, (float)px)) *
						                (std::min(fy1, (float)py + 1) - std::max(fy0, (float)py));
						if (a <= 0)
							continue;
						const uint8_t *c = &p.rgba[((size_t)py * p.w + px) * 4];
						r += c[0] * a;
						g += c[1] * a;
						b += c[2] * a;
						area += a;
					}
				uint8_t *t = &out[((size_t)ty * w + tx) * 4];
				if (area > 0)
				{
					r /= area;
					g /= area;
					b /= area;
				}
				t[0] = (uint8_t)(r * alpha + bg.r() * (1 - alpha) + 0.5f);
				t[1] = (uint8_t)(g * alpha + bg.g() * (1 - alpha) + 0.5f);
				t[2] = (uint8_t)(b * alpha + bg.b() * (1 - alpha) + 0.5f);
				t[3] = 255;
			}
		}
		texture = vgui2::surface()->CreateNewTextureID(true);
		vgui2::surface()->DrawSetTextureRGBA(texture, out.data(), w, h, false, true);
	}
	s_Textures[key] = texture;
	return texture;
}

void DrawPicture(const char *image, int x, int y, int w, int h, bool cover, float alpha, Color bg)
{
	const int t = PictureTexture(image, w, h, cover, alpha, bg);
	if (t < 0)
	{
		FillRect(x, y, w, h, bg);
		return;
	}
	vgui2::surface()->DrawSetTexture(t);
	vgui2::surface()->DrawSetColor(255, 255, 255, 255);
	vgui2::surface()->DrawTexturedRect(x, y, x + w, y + h);
}

//-----------------------------------------------------------------------------
// Text
//-----------------------------------------------------------------------------
//! Text cut with an ellipsis to maxWide
std::wstring Fit(vgui2::HFont font, const std::wstring &text, int maxWide)
{
	if (maxWide <= 0 || TextWide(font, text) <= maxWide)
		return text;
	std::wstring s = text;
	const std::wstring dots = L"\x2026";
	while (!s.empty() && TextWide(font, s + dots) > maxWide)
		s.pop_back();
	return s + dots;
}

//! Letter-spaced text (the design's small upper-case headings): returns the width
int DrawSpaced(vgui2::HFont font, Color color, int x, int y, const std::wstring &text, int spacing, bool draw = true)
{
	int cx = x;
	for (size_t i = 0; i < text.size(); i++)
	{
		const wchar_t ch[2] = { text[i], 0 };
		const int cw = TextWide(font, ch);
		if (draw)
		{
			vgui2::surface()->DrawSetTextFont(font);
			vgui2::surface()->DrawSetTextColor(color);
			vgui2::surface()->DrawSetTextPos(cx, y);
			vgui2::surface()->DrawPrintText(ch, 1);
		}
		cx += cw + (i + 1 < text.size() ? spacing : 0);
	}
	return cx - x;
}

//! Word-wrapped lines (a line break in the text starts a new line)
std::vector<std::wstring> Wrap(vgui2::HFont font, const std::wstring &text, int maxW)
{
	std::vector<std::wstring> out;
	size_t start = 0;
	while (start <= text.size())
	{
		size_t nl = text.find(L'\n', start);
		if (nl == std::wstring::npos)
			nl = text.size();
		const std::wstring para = text.substr(start, nl - start);
		std::wstring line;
		size_t p = 0;
		if (para.empty())
			out.push_back(L"");
		while (p < para.size())
		{
			size_t e = para.find(L' ', p);
			if (e == std::wstring::npos)
				e = para.size();
			const std::wstring word = para.substr(p, e - p);
			const std::wstring trial = line.empty() ? word : line + L" " + word;
			if (!line.empty() && TextWide(font, trial) > maxW)
			{
				out.push_back(line);
				line = word;
			}
			else
				line = trial;
			p = e + 1;
		}
		if (!para.empty())
			out.push_back(line);
		start = nl + 1;
	}
	return out;
}

std::string Upper(const char *s)
{
	std::string u = s ? s : "";
	for (char &c : u)
		c = (char)toupper((unsigned char)c);
	return u;
}

struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
	bool Has(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

//-----------------------------------------------------------------------------
// A scroll area's state and its bar (the design's: 8 px, track #0b100c, thumb #3c5a3e inside a 2 px track border,
// #8fdc7a while hovered or dragged)
//-----------------------------------------------------------------------------
struct Scroller
{
	int view = 0, content = 0, offset = 0;
	bool dragging = false;
	int dragY = 0, dragOffset = 0;
	void Clamp() { offset = std::max(0, std::min(offset, std::max(0, content - view))); }
	bool Needed() const { return content > view; }
	void ThumbRect(int barX, int barY, int barW, int &ty, int &th) const
	{
		th = std::max(barW * 2, view * view / std::max(1, content));
		ty = barY + (content > view ? (view - th) * offset / (content - view) : 0);
		(void)barX;
	}
	void Draw(int barX, int barY, int barW, bool hot) const
	{
		if (!Needed())
			return;
		FillRect(barX, barY, barW, view, C_SCROLL_TRACK);
		int ty, th;
		ThumbRect(barX, barY, barW, ty, th);
		const int b = std::max(1, barW / 4);
		FillRect(barX + b, ty + b, barW - 2 * b, th - 2 * b, hot || dragging ? C_GREEN : C_SCROLL_THUMB);
	}
	//! A press on the bar: drag the thumb (a press on the track jumps there). True if it was on the bar.
	bool Press(int px, int py, int barX, int barY, int barW)
	{
		if (!Needed() || px < barX || px >= barX + barW || py < barY || py >= barY + view)
			return false;
		int ty, th;
		ThumbRect(barX, barY, barW, ty, th);
		if (py < ty || py >= ty + th)
		{
			offset = (py - barY - th / 2) * (content - view) / std::max(1, view - th);
			Clamp();
		}
		dragging = true;
		dragY = py;
		dragOffset = offset;
		return true;
	}
	void Move(int py, int barW)
	{
		if (!dragging)
			return;
		const int th = std::max(barW * 2, view * view / std::max(1, content));
		offset = dragOffset + (py - dragY) * (content - view) / std::max(1, view - th);
		Clamp();
	}
	void Wheel(int delta, int step)
	{
		offset -= delta * step;
		Clamp();
	}
};
} // namespace

class CSboLoadoutMenu;

//-----------------------------------------------------------------------------
// A clipped part of the menu (the item grid, the INFO text, the loadout list, the large picture, the name box):
// painted and handled by the menu
//-----------------------------------------------------------------------------
class CSboLoadoutArea : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboLoadoutArea, vgui2::Panel);

public:
	enum Kind
	{
		AREA_GRID,
		AREA_INFO,
		AREA_LIST,
		AREA_ZOOM,
		AREA_NAME,
	};
	CSboLoadoutArea(CSboLoadoutMenu *pMenu, Kind kind);
	void Paint() override;
	void PaintBackground() override {}
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseReleased(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override { OnMousePressed(code); }
	void OnCursorMoved(int x, int y) override;
	void OnCursorExited() override;
	void OnMouseWheeled(int delta) override;

	Kind m_eKind;
	CSboLoadoutMenu *m_pMenu;
	int m_iMouseX = -1, m_iMouseY = -1;
};

class CSboLoadoutMenu : public CSboMenuPanel
{
	DECLARE_CLASS_SIMPLE(CSboLoadoutMenu, CSboMenuPanel);

public:
	static CSboLoadoutMenu *Get();
	void Open();
	bool SlotInput(int iSlot);

	void Paint() override;
	void PostChildPaint() override;
	void PaintBackground() override;
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override { OnMousePressed(code); }
	void OnCursorMoved(int x, int y) override;
	void OnThink() override;
	void OnCommand(const char *command) override;
	MESSAGE_FUNC(OnTextNewLine, "TextNewLine") { NameOk(); }

	// the areas
	void PaintArea(CSboLoadoutArea *pArea);
	void AreaPressed(CSboLoadoutArea *pArea, int x, int y);
	void AreaReleased(CSboLoadoutArea *pArea);
	void AreaMoved(CSboLoadoutArea *pArea, int x, int y);
	void AreaWheeled(CSboLoadoutArea *pArea, int delta);

protected:
	void LayoutMenu() override;

private:
	CSboLoadoutMenu(vgui2::Panel *pParent);
	int P(float mock) const { return (int)lroundf(mock * m_flK); }
	int X(float mock) const { return m_iWinX + P(mock); }
	int Y(float mock) const { return m_iWinY + P(mock); }
	vgui2::HFont Font(const char *face, float px, int weight = 400) const;
	//! Small text (14 px and less) in Barlow Medium: Regular's thin strokes broke up at small pixel sizes
	// Barlow (the design's face) only for the large condensed titles: in small sizes a Windows client lost
	// letters ("u", "i") from Barlow; the text is in the menus' UI face (Tahoma on Windows, Liberation Sans on Linux)
	vgui2::HFont Body(float px) const { return Font(SboUiFontFace(), px, 400); }
	//! A text line's height: the design's, at least the font's (small screens: the fonts have a minimum size)
	int LH(float mock, float px) const { return std::max(P(mock), FontTall(Body(px))); }
	vgui2::HFont Medium(float px) const { return Font(SboUiFontFace(), px, 400); }
	vgui2::HFont Semi(float px) const { return Font(SboUiFontFace(), px, 700); }
	vgui2::HFont Cond(float px) const { return Font("Barlow Condensed SemiBold", px); }

	bool Assassin() const { return m_Sel[SLOT_CYBER] == ASSASSIN; }
	bool Locked(int slot) const { return Assassin() && AssassinSets(slot); }
	const Item &Shown() const;
	//! The details' heading for Shown() (its slot's, or Cyber tech for the Assassin card on a page it sets)
	const char *ShownHeading() const;
	//! The gold notice's lines on a page Assassin sets (wrapped to the notice's width), and its height
	std::vector<std::wstring> NoticeLines() const;
	int NoticeTall() const;
	std::wstring PickName(int slot) const;
	int CurrentPreset() const;
	void Pick(int slot, int item);
	void SetCat(int cat);
	void Deploy();
	void Cancel();
	void Randomize();

	// saved loadouts
	void LoadFile();
	void SaveFile();
	void OpenNameBox(bool rename);
	void NameOk();
	void CloseNameBox();

	// layout parts
	void LayoutGrid();
	void LayoutInfo();
	std::wstring HeaderLine() const;
	//! The whole loadout's summary (ComputeStats): speed, health, armor, damage, extras, in a box h tall at x, y;
	//! returns its width (draw false: only measures)
	int PaintStats(int x, int y, int h, bool draw);

	static CSboLoadoutMenu *s_pInstance;

	float m_flK = 1.0f;
	int m_iWinX = 0, m_iWinY = 0, m_iWinW = 0, m_iWinH = 0;
	int m_Sel[NUM_SLOTS] = {};
	int m_iCat = 2;
	int m_iHover = -1;           //!< the card under the mouse (its outline)
	int m_Seen[NUM_SLOTS] = {};  //!< per slot page: the card hovered last (its details stay shown), -1 none
	bool m_bList = false, m_bZoom = false;
	int m_iMouseX = -1, m_iMouseY = -1;
	double m_flDiceHover = -1.0; //!< since when the mouse is on the dice (its tooltip after 0.5 s)
	double m_flStatsHover = -1.0; //!< since when the mouse is on the loadout summary (its tooltip after 0.5 s)
	int m_iFullHover = -1;        //!< the full primary card under the mouse (its tooltip after 0.5 s), -1 none
	double m_flFullHover = -1.0;  //!< since when

	CSboLoadoutArea *m_pGrid = nullptr, *m_pInfo = nullptr, *m_pList = nullptr, *m_pZoom = nullptr, *m_pNameBox = nullptr;
	Scroller m_GridScroll, m_InfoScroll, m_ListScroll;
	std::vector<Rect> m_Cards; //!< in the grid's own coordinates (before scrolling)
	std::vector<std::wstring> m_InfoLines;
	const Item *m_pInfoFor = nullptr;

	// main panel hit areas
	Rect m_rClose, m_rDrop, m_rDice, m_rRail[NUM_SLOTS], m_rPicture, m_rUnlock, m_rSlots[NUM_SLOTS], m_rCancel, m_rDeploy, m_rStats;

	// saved loadouts
	std::vector<Preset> m_Presets;
	bool m_bFileRead = false;
	int m_Last[NUM_SLOTS] = {};
	bool m_bHaveLast = false;
	bool m_bRenaming = false;
	CSboBrowserEntry *m_pNameEntry = nullptr;

};

CSboLoadoutMenu *CSboLoadoutMenu::s_pInstance = nullptr;

//-----------------------------------------------------------------------------
// CSboLoadoutArea
//-----------------------------------------------------------------------------
CSboLoadoutArea::CSboLoadoutArea(CSboLoadoutMenu *pMenu, Kind kind)
    : BaseClass(pMenu, "LoadoutArea")
    , m_eKind(kind)
    , m_pMenu(pMenu)
{
	SetPaintBackgroundEnabled(false);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(false);
	SetVisible(false);
}

void CSboLoadoutArea::Paint() { m_pMenu->PaintArea(this); }

void CSboLoadoutArea::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	vgui2::input()->SetMouseCapture(GetVPanel());
	m_pMenu->AreaPressed(this, x, y);
}

void CSboLoadoutArea::OnMouseReleased(vgui2::MouseCode code)
{
	vgui2::input()->SetMouseCapture(0);
	m_pMenu->AreaReleased(this);
}

void CSboLoadoutArea::OnCursorMoved(int x, int y)
{
	m_iMouseX = x;
	m_iMouseY = y;
	m_pMenu->AreaMoved(this, x, y);
}

void CSboLoadoutArea::OnCursorExited()
{
	m_iMouseX = m_iMouseY = -1;
	m_pMenu->AreaMoved(this, -1, -1);
}

void CSboLoadoutArea::OnMouseWheeled(int delta) { m_pMenu->AreaWheeled(this, delta); }

//-----------------------------------------------------------------------------
// CSboLoadoutMenu
//-----------------------------------------------------------------------------
CSboLoadoutMenu *CSboLoadoutMenu::Get()
{
	if (!s_pInstance && g_pViewport)
		s_pInstance = new CSboLoadoutMenu(g_pViewport);
	return s_pInstance;
}

CSboLoadoutMenu::CSboLoadoutMenu(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboLoadoutMenu")
{
	SetBackdropTrans(0); // the page is painted here
	SetPostChildPaintEnabled(true); // the dice's tooltip over the cards (PostChildPaint)
	AddBarlowFonts();
	m_pGrid = new CSboLoadoutArea(this, CSboLoadoutArea::AREA_GRID);
	m_pInfo = new CSboLoadoutArea(this, CSboLoadoutArea::AREA_INFO);
	m_pList = new CSboLoadoutArea(this, CSboLoadoutArea::AREA_LIST);
	m_pZoom = new CSboLoadoutArea(this, CSboLoadoutArea::AREA_ZOOM);
	m_pNameBox = new CSboLoadoutArea(this, CSboLoadoutArea::AREA_NAME);
	m_pNameEntry = new CSboBrowserEntry(m_pNameBox, "LoadoutName", "Loadout name");
	m_pNameEntry->SetMaximumCharCount(31);
	m_pNameEntry->SendNewLine(true);
	m_pNameEntry->AddActionSignalTarget(this);
	if (vgui2::HScheme h = BrowserScheme())
		m_pNameEntry->SetScheme(h);
}

vgui2::HFont CSboLoadoutMenu::Font(const char *face, float px, int weight) const
{
	// a VGUI2 font's tall is the line's height in pixels; CSS sizes are the em. No text below MIN_EM pixels (12 at
	// 768 lines): the window shrinks with the screen, the text stops shrinking there and the rows make room for it
	const float MIN_EM = 12.0f;
	const float em = std::max(px * m_flK, std::min(px, MIN_EM));
	return MakeFontFace(face, std::max(6, (int)lroundf(em * 1.2f)), weight);
}

void CSboLoadoutMenu::LoadFile()
{
	if (m_bFileRead)
		return;
	m_bFileRead = true;
	KeyValues *kv = new KeyValues("Loadouts");
	if (kv->LoadFromFile(g_pFullFileSystem, LOADOUTS_FILE, LOADOUTS_PATH_ID))
	{
		m_bHaveLast = ParseSel(kv->GetString("last", ""), m_Last);
		if (KeyValues *list = kv->FindKey("list"))
		{
			for (KeyValues *e = list->GetFirstSubKey(); e; e = e->GetNextKey())
			{
				Preset p;
				p.name = e->GetString("name", "");
				if (!p.name.empty() && p.name.size() < 64 && ParseSel(e->GetString("sel", ""), p.sel) && m_Presets.size() < 64)
					m_Presets.push_back(p);
			}
		}
	}
	// no file yet: no saved loadouts ("Unsaved loadout"; Save as... makes the first)
	kv->deleteThis();
}

void CSboLoadoutMenu::SaveFile()
{
	KeyValues *kv = new KeyValues("Loadouts");
	if (m_bHaveLast)
		kv->SetString("last", SelText(m_Last).c_str());
	KeyValues *list = kv->FindKey("list", true);
	for (size_t i = 0; i < m_Presets.size(); i++)
	{
		char key[16];
		snprintf(key, sizeof(key), "%d", (int)i + 1);
		KeyValues *e = list->FindKey(key, true);
		e->SetString("name", m_Presets[i].name.c_str());
		e->SetString("sel", SelText(m_Presets[i].sel).c_str());
	}
	kv->SaveToFile(g_pFullFileSystem, LOADOUTS_FILE, LOADOUTS_PATH_ID);
	kv->deleteThis();
}

void CSboLoadoutMenu::Open()
{
	LoadFile();
	// the last loadout deployed, once per game (the choices live in the viewport from then on)
	static bool s_bLastApplied = false;
	if (!s_bLastApplied && m_bHaveLast)
	{
		for (int i = 0; i < NUM_SLOTS; i++)
			sbo::SetBriefChoice(Slots()[i].choice, m_Last[i] + 1);
	}
	s_bLastApplied = true;
	for (int i = 0; i < NUM_SLOTS; i++)
	{
		const int n = (int)Slots()[i].items.size();
		m_Sel[i] = std::max(0, std::min(n - 1, sbo::GetBriefChoice(Slots()[i].choice) - 1));
	}
	// a remembered primary the team limit has made full: another one, as the server would leave it (FreeHeavy)
	if (HeavyFull(m_Sel[SLOT_PRIMARY]))
		Pick(SLOT_PRIMARY, FreeHeavy(m_Sel[SLOT_PRIMARY], -1));
	m_iHover = -1;
	for (int &v : m_Seen)
		v = -1;
	m_bList = m_bZoom = false;
	CloseNameBox();
	ShowMenu();
	SetKeyBoardInputEnabled(false);
}

bool CSboLoadoutMenu::SlotInput(int iSlot)
{
	// the HUD's slot commands come 0-based here (key 1 = slot1 = 0)
	if (!IsVisible() || iSlot < 0 || iSlot >= NUM_SLOTS || m_pNameBox->IsVisible())
		return false;
	SetCat(iSlot);
	LayoutMenu();
	return true;
}

const Item &CSboLoadoutMenu::Shown() const
{
	// the card hovered last on this page; else the pick, or on a page Assassin sets what Assassin gives there (its
	// .444k, grenades, armor skin; the Assassin card itself for the primary, which it leaves empty)
	const Slot &s = Slots()[m_iCat];
	const int seen = m_Seen[m_iCat];
	if (seen >= 0 && seen < (int)s.items.size())
		return s.items[seen];
	if (Locked(m_iCat))
	{
		switch (m_iCat)
		{
		case 1: return s.items[1]; // .444k
		case 4: return s.items[0]; // grenades
		case 5: return s.items[0]; // armor skin
		default: return Slots()[SLOT_CYBER].items[ASSASSIN];
		}
	}
	return s.items[m_Sel[m_iCat]];
}

const char *CSboLoadoutMenu::ShownHeading() const
{
	const Item &it = Shown();
	const Slot &cyber = Slots()[SLOT_CYBER];
	if (m_iCat != SLOT_CYBER && &it == &cyber.items[ASSASSIN])
		return cyber.heading;
	return Slots()[m_iCat].heading;
}

std::vector<std::wstring> CSboLoadoutMenu::NoticeLines() const
{
	if (!Locked(m_iCat))
		return {};
	const std::wstring t = L"Assassin sets this slot: " + Widen(AssassinSets(m_iCat)) +
	                       L". Pick another Cyberware to choose it yourself";
	const vgui2::HFont bf = Body(13);
	const int buttonW = TextWide(bf, L"Remove Assassin") + P(24);
	return Wrap(Body(14), t, P(939 - 261 - 48) - P(14) - buttonW - P(26));
}

int CSboLoadoutMenu::NoticeTall() const
{
	const size_t n = NoticeLines().size();
	return std::max(P(52), (int)n * LH(20, 14) + P(20));
}

std::wstring CSboLoadoutMenu::PickName(int slot) const
{
	if (Locked(slot))
		return Widen(AssassinSets(slot)) + L" \x00b7 Assassin";
	return Widen(Slots()[slot].items[m_Sel[slot]].name);
}

int CSboLoadoutMenu::CurrentPreset() const
{
	for (size_t i = 0; i < m_Presets.size(); i++)
	{
		if (!memcmp(m_Presets[i].sel, m_Sel, sizeof(m_Sel)))
			return (int)i;
	}
	return -1;
}

void CSboLoadoutMenu::Pick(int slot, int item)
{
	if (slot < 0 || slot >= NUM_SLOTS || item < 0 || item >= (int)Slots()[slot].items.size() || Locked(slot))
		return;
	if (slot == SLOT_PRIMARY && HeavyFull(item))
		return; // the team has all of them (the server would refuse it too)
	m_Sel[slot] = item;
	// CSboBriefMenu::Choose: the viewport keeps the choice (sent on DEPLOY)
	sbo::SetBriefChoice(Slots()[slot].choice, item + 1);
	LayoutGrid();
}

void CSboLoadoutMenu::SetCat(int cat)
{
	if (cat < 0 || cat >= NUM_SLOTS)
		return;
	m_iCat = cat;
	m_iHover = -1; // the page's last-hovered card stays (m_Seen)
	m_GridScroll.offset = 0;
	LayoutGrid();
	LayoutInfo();
}

void CSboLoadoutMenu::Randomize()
{
	const int previous = m_Sel[SLOT_PRIMARY];
	for (int i = 0; i < NUM_SLOTS; i++)
	{
		if (i == SLOT_PRIMARY)
		{
			// a random primary among those not full
			std::vector<int> free;
			for (int k = 0; k < (int)Slots()[i].items.size(); k++)
			{
				if (!HeavyFull(k))
					free.push_back(k);
			}
			m_Sel[i] = free.empty() ? FreeHeavy(previous, previous) : free[rand() % (int)free.size()];
		}
		else
			m_Sel[i] = rand() % (int)Slots()[i].items.size();
		sbo::SetBriefChoice(Slots()[i].choice, m_Sel[i] + 1);
	}
	m_iHover = -1;
	m_bList = false;
	LayoutGrid();
	LayoutInfo();
}

void CSboLoadoutMenu::Deploy()
{
	// CSboBriefMenu::Go: one command per choice, in this order
	static const struct
	{
		int choice;
		const char *format;
	} s_Commands[] = {
		{ sbo::BRIEF_BLADE, "blade %d" }, { sbo::BRIEF_LIGHT, "light %d" }, { sbo::BRIEF_HEAVY, "heavy %d" },
		{ sbo::BRIEF_ITEM, "item %d" }, { sbo::BRIEF_CYBERWARE, "cyberware %d" }, { sbo::BRIEF_ARMOR, "armor %d" },
		{ sbo::BRIEF_MALEFEMALE, "malefemale %d" },
	};
	for (int i = 0; i < NUM_SLOTS; i++)
		sbo::SetBriefChoice(Slots()[i].choice, m_Sel[i] + 1);
	for (const auto &c : s_Commands)
	{
		char sz[32];
		snprintf(sz, sizeof(sz), c.format, sbo::GetBriefChoice(c.choice));
		sbo::ExecClientCmd(sz);
	}
	// the last loadout, for the next game
	memcpy(m_Last, m_Sel, sizeof(m_Last));
	m_bHaveLast = true;
	SaveFile();
	sbo::HideTopMenu();
}

void CSboLoadoutMenu::Cancel()
{
	// CSboBriefMenu's Cancel: HideTopMenu, ClientCmd( "" ), HideTopMenu
	sbo::HideTopMenu();
	sbo::ExecClientCmd("");
	sbo::HideTopMenu();
}

void CSboLoadoutMenu::OpenNameBox(bool rename)
{
	m_bRenaming = rename;
	m_bList = false;
	const int cur = CurrentPreset();
	m_pNameEntry->SetText(rename && cur >= 0 ? m_Presets[cur].name.c_str() : "");
	m_pNameBox->SetVisible(true);
	m_pNameBox->MoveToFront();
	SetKeyBoardInputEnabled(true);
	m_pNameBox->SetKeyBoardInputEnabled(true);
	m_pNameEntry->SetKeyBoardInputEnabled(true);
	m_pNameEntry->SetMouseInputEnabled(true);
	m_pNameEntry->RequestFocus();
	m_pNameEntry->SelectAllText(true);
	LayoutMenu();
}

void CSboLoadoutMenu::CloseNameBox()
{
	if (!m_pNameBox)
		return;
	m_pNameBox->SetVisible(false);
	SetKeyBoardInputEnabled(false);
}

void CSboLoadoutMenu::NameOk()
{
	if (!m_pNameBox->IsVisible())
		return;
	char name[64];
	m_pNameEntry->GetText(name, sizeof(name));
	std::string n = name;
	while (!n.empty() && n.back() == ' ')
		n.pop_back();
	while (!n.empty() && n.front() == ' ')
		n.erase(n.begin());
	if (n.empty())
		return;
	const int cur = CurrentPreset();
	if (m_bRenaming && cur >= 0)
		m_Presets[cur].name = n;
	else
	{
		// Save as: a loadout of that name is replaced
		Preset p;
		p.name = n;
		memcpy(p.sel, m_Sel, sizeof(p.sel));
		bool replaced = false;
		for (Preset &e : m_Presets)
		{
			if (!Q_stricmp(e.name.c_str(), n.c_str()))
			{
				e = p;
				replaced = true;
			}
		}
		if (!replaced && m_Presets.size() < 64)
			m_Presets.push_back(p);
	}
	SaveFile();
	CloseNameBox();
}

void CSboLoadoutMenu::OnCommand(const char *command)
{
	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Layout
//-----------------------------------------------------------------------------
void CSboLoadoutMenu::LayoutMenu()
{
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	// the design's 1440 x 900 page on the screen: the window (1320 x 800) keeps its share of the screen
	m_flK = std::min(sw / 1440.0f, sh / 900.0f);
	m_iWinW = P(1320);
	m_iWinH = P(800);
	m_iWinX = (sw - m_iWinW) / 2;
	m_iWinY = (sh - m_iWinH) / 2;

	// the items: 22 / 24 padding inside x 261..939, under the heading (and the Assassin notice)
	const int gridTop = Y(89 + 22 + 31 + 16) + (Locked(m_iCat) ? NoticeTall() + P(16) : 0);
	m_pGrid->SetBounds(X(261 + 24), gridTop, P(939 - 261 - 48), Y(718 - 22) - gridTop);
	m_pGrid->SetVisible(true);
	LayoutGrid();
	LayoutInfo();

	// the loadout list: under the drop-down, 18 from the rail's sides
	m_pList->SetVisible(m_bList);
	if (m_bList)
	{
		const int rows = (int)m_Presets.size();
		const int listTall = std::min(5, std::max(rows, 0)) * P(38); // the design's 210: five rows, then it scrolls
		m_ListScroll.view = listTall;
		m_ListScroll.content = rows * P(38);
		m_ListScroll.Clamp();
		m_pList->SetBounds(X(18), Y(89 + 14 + 74), P(260 - 36), listTall + P(38) + 3);
		m_pList->MoveToFront();
	}
	m_pZoom->SetVisible(m_bZoom);
	if (m_bZoom)
	{
		m_pZoom->SetBounds(0, 0, sw, sh);
		m_pZoom->MoveToFront();
	}
	if (m_pNameBox->IsVisible())
	{
		m_pNameBox->SetBounds(0, 0, sw, sh);
		const int bw = P(420), bh = P(150);
		const int bx = (sw - bw) / 2, by = (sh - bh) / 2;
		m_pNameEntry->SetBounds(bx + P(20), by + P(52), bw - P(40), P(36));
		m_pNameEntry->SetFont(Body(15));
		m_pNameBox->MoveToFront();
	}
}

void CSboLoadoutMenu::LayoutGrid()
{
	const Slot &s = Slots()[m_iCat];
	const int W = m_pGrid->GetWide();
	const int gap = P(14);
	const int colW = (W - P(10) - 2 * gap) / 3; // room for the bar at the right
	const vgui2::HFont chipFont = Body(12);
	m_Cards.clear();
	int y = 0, rowTall = 0;
	std::vector<int> rowStart;
	for (int i = 0; i < (int)s.items.size(); i++)
	{
		const int col = i % 3;
		if (col == 0)
		{
			y += i ? rowTall + gap : 0;
			rowTall = 0;
		}
		// chips wrap inside the card's text area (padding 12)
		std::vector<std::wstring> chips;
		const Item &it = s.items[i];
		if (it.weapon)
		{
			char buf[48];
			chips.push_back(Widen(it.dmg) + L" dmg");
			if (it.clip)
			{
				snprintf(buf, sizeof(buf), "%d rds", it.clip);
				chips.push_back(Widen(buf));
			}
			if (it.speed)
			{
				snprintf(buf, sizeof(buf), "%d%% speed", it.speed);
				chips.push_back(Widen(buf));
			}
		}
		else
		{
			for (const char *c : it.chips)
				chips.push_back(!strcmp(c, "Speed") && SpeedBonus(it.token) ? SpeedBonusText(SpeedBonus(it.token), true) : Widen(c));
		}
		int lines = chips.empty() ? 0 : 1, lineW = 0;
		for (const std::wstring &c : chips)
		{
			const int cw = TextWide(chipFont, c) + P(14) + 2;
			if (lineW > 0 && lineW + P(6) + cw > colW - P(24))
			{
				lines++;
				lineW = cw;
			}
			else
				lineW += (lineW ? P(6) : 0) + cw;
		}
		// image 128, padding 10 / 12, name 16 px (19), gap 6, chip lines of 20 with 6 between
		const int tall = P(128) + P(10) + LH(19, 16) + (lines ? P(6) + lines * LH(20, 12) + (lines - 1) * P(6) : 0) + P(12) + 2;
		rowTall = std::max(rowTall, tall);
		Rect r;
		r.x = col * (colW + gap);
		r.y = y;
		r.w = colW;
		r.h = tall;
		m_Cards.push_back(r);
	}
	// a grid row is as tall as its tallest card
	for (size_t i = 0; i < m_Cards.size(); i++)
	{
		int maxH = 0;
		for (size_t j = i - i % 3; j < std::min(m_Cards.size(), i - i % 3 + 3); j++)
			maxH = std::max(maxH, m_Cards[j].h);
		m_Cards[i].h = maxH;
	}
	m_GridScroll.view = m_pGrid->GetTall();
	m_GridScroll.content = m_Cards.empty() ? 0 : m_Cards.back().y + m_Cards.back().h;
	m_GridScroll.Clamp();
}

void CSboLoadoutMenu::LayoutInfo()
{
	// the INFO text fills the aside below the details; its top depends on what the item shows
	const Item &it = Shown();
	const vgui2::HFont infoFont = Body(14);
	const int asideX = X(940 + 1 + 24), asideW = P(380 - 1 - 48);
	int y = Y(89 + 1 + 20) + P(190) + P(14);
	// category + name + summary
	y += LH(15, 12) + P(4) + std::max(P(36), FontTall(Cond(30)));
	(void)infoFont; // no one-line summary under the name: the room goes to INFO
	y += P(14);
	// stats
	const int nStats = it.weapon ? (it.clip ? 4 : 3) : (int)it.rows.size();
	int nBars = it.weapon ? nStats : 0;
	for (const Line &r : it.rows)
		nBars += r.bar >= 0.0f ? 1 : 0;
	y += nStats * LH(16, 13) + nBars * P(8) + std::max(0, nStats - 1) * P(10);
	if (it.weapon && !it.modes.empty())
		y += P(14) + P(11) + (int)it.modes.size() * LH(16, 13) + ((int)it.modes.size() - 1) * P(6);
	y += P(14) + P(11) + LH(15, 12) + P(6);
	const int bottom = Y(718 - 20);
	m_pInfo->SetBounds(asideX, y, asideW, std::max(P(20), bottom - y));
	m_pInfo->SetVisible(true);
	// the text: #SBO_Item_<token>, as the old menu shows it
	if (&it != m_pInfoFor || m_InfoLines.empty())
	{
		m_pInfoFor = &it;
		char token[64];
		snprintf(token, sizeof(token), "#SBO_Item_%s", it.token);
		static wchar_t s_wText[4096];
		sboui::Localize(token, s_wText, sizeof(s_wText));
		std::wstring text = s_wText;
		text.erase(std::remove(text.begin(), text.end(), L'\r'), text.end());
		// the original text as written; runs of empty lines become one, none at the start or end
		std::vector<std::wstring> lines = Wrap(Body(13), text, asideW - P(6) - P(8));
		m_InfoLines.clear();
		for (std::wstring &l : lines)
		{
			while (!l.empty() && (l.back() == L' ' || l.back() == L'\t'))
				l.pop_back();
			if (l.empty() && (m_InfoLines.empty() || m_InfoLines.back().empty()))
				continue;
			m_InfoLines.push_back(l);
		}
		while (!m_InfoLines.empty() && m_InfoLines.back().empty())
			m_InfoLines.pop_back();
		m_InfoScroll.offset = 0;
	}
	m_InfoScroll.view = m_pInfo->GetTall();
	m_InfoScroll.content = (int)m_InfoLines.size() * LH(19, 13);
	m_InfoScroll.Clamp();
}

std::wstring CSboLoadoutMenu::HeaderLine() const
{
	std::string map = sbo::GetLevelName() ? sbo::GetLevelName() : "";
	if (!Q_strnicmp(map.c_str(), "maps/", 5))
		map = map.substr(5);
	if (map.size() > 4 && !Q_stricmp(map.c_str() + map.size() - 4, ".bsp"))
		map.resize(map.size() - 4);
	std::wstring line = Widen(map.c_str());
	if (!map.empty())
	{
		const char *mode = ModeName(ModeOfMap(map.c_str()));
		if (mode && *mode && strcmp(mode, "-"))
			line += L" \x00b7 " + Widen(mode);
	}
	int type, v1, v2;
	sbo::GetModeInfo(type, v1, v2);
	if (type == sbo::MODEINFO_ROUNDS)
	{
		char buf[48];
		snprintf(buf, sizeof(buf), "Round %d / %d", v1, v2);
		line += L" \x00b7 " + Widen(buf);
	}
	return line;
}

int CSboLoadoutMenu::PaintStats(int x, int y, int h, bool draw)
{
	const LoadoutStats s = ComputeStats(m_Sel);
	const vgui2::HFont lab = Body(11), val = Cond(20), chip = Body(12);
	const int pad = P(12), gap = P(12), spacing = P(1.2f);
	const int labTall = FontTall(lab), valTall = FontTall(val);
	const int top = y + (h - labTall - valTall) / 2, vy = top + labTall;

	struct Cell
	{
		const wchar_t *label;
		std::wstring value;
		Color color;
	};
	char buf[32];
	const int pct = (int)lroundf(s.speed * 100.0f / SBO_NORMAL_SPEED);
	snprintf(buf, sizeof(buf), "%d%%", pct);
	const std::wstring speedText = Widen(buf);
	snprintf(buf, sizeof(buf), "%d", s.health);
	const Cell health{ L"HEALTH", Widen(buf), s.healthBonus ? C_GREEN : C_TEXT_HI };
	if (s.armor)
		snprintf(buf, sizeof(buf), "%d \xc2\xb7 %d%%", s.armor, s.absorb);
	const Cell armor{ L"ARMOR", s.armor ? Widen(buf) : std::wstring(L"None"), s.armor ? C_TEXT_HI : C_DIM };
	const Cell damage{ L"DAMAGE", s.damage == 2 ? std::wstring(L"\x00d7" L"2") : std::wstring(L"\x00d7" L"1"), s.damage == 2 ? C_GREEN : C_TEXT_HI };
	std::vector<const wchar_t *> tags;
	if (s.nightVision)
		tags.push_back(L"Night vision");
	if (s.cloak)
		tags.push_back(L"Cloak");
	if (s.medikit)
		tags.push_back(L"Medikit");

	int cx = x + pad;
	auto label = [&](const wchar_t *l) {
		if (draw)
			DrawSpaced(lab, C_DIM, cx, top, l, spacing);
	};
	auto divider = [&]() {
		if (draw)
			FillRect(cx + gap / 2, y + P(8), 1, h - 2 * P(8), C_LINE);
		cx += gap + 1;
	};

	// speed: the percent of a level 0 player's, and a bar (full at 100%)
	{
		const int barW = P(54), barH = std::max(3, P(4));
		const int tw = TextWide(val, speedText);
		const int w = std::max(DrawSpaced(lab, C_DIM, 0, 0, L"SPEED", spacing, false), tw + P(8) + barW);
		label(L"SPEED");
		if (draw)
		{
			DrawText(val, pct > 100 ? C_GREEN : C_TEXT_HI, cx, vy, speedText);
			const int bx = cx + tw + P(8), by = vy + (valTall - barH) / 2;
			FillRect(bx, by, barW, barH, C_BAR_BG);
			FillRect(bx, by, (int)lroundf(barW * std::min(1.0f, pct / 100.0f)), barH, pct > 100 ? C_GOLD : C_GREEN);
		}
		cx += w;
	}
	for (const Cell *c : { &health, &armor, &damage })
	{
		divider();
		const int w = std::max(DrawSpaced(lab, C_DIM, 0, 0, c->label, spacing, false), TextWide(val, c->value));
		label(c->label);
		if (draw)
			DrawText(val, c->color, cx, vy, c->value);
		cx += w;
	}
	// extras: small chips on the value line
	{
		divider();
		int w = 0;
		const int chipH = std::min(valTall, FontTall(chip) + P(6)), cy = vy + (valTall - chipH) / 2;
		if (tags.empty())
		{
			w = TextWide(val, L"\x2014");
			if (draw)
				DrawText(val, C_DIM, cx, vy, L"\x2014");
		}
		for (size_t i = 0; i < tags.size(); i++)
		{
			const int cw = TextWide(chip, tags[i]) + P(12);
			if (draw)
			{
				OutlineRect(cx + w, cy, cw, chipH, C_CHIP_LINE);
				DrawTextMid(chip, C_SOFT, cx + w + P(6), cy, chipH, tags[i]);
			}
			w += cw + (i + 1 < tags.size() ? P(5) : 0);
		}
		w = std::max(w, DrawSpaced(lab, C_DIM, 0, 0, L"EXTRAS", spacing, false));
		label(L"EXTRAS");
		cx += w;
	}
	const int total = cx + pad - x;
	if (draw)
		m_rStats = { x, y, total, h };
	return total;
}

//-----------------------------------------------------------------------------
// Painting
//-----------------------------------------------------------------------------
void CSboLoadoutMenu::PaintBackground()
{
	FillRect(0, 0, GetWide(), GetTall(), C_PAGE);
}

void CSboLoadoutMenu::Paint()
{
	const Slot &cat = Slots()[m_iCat];
	const int wx = m_iWinX, wy = m_iWinY;
	// window
	FillRect(wx, wy, m_iWinW, m_iWinH, C_WINDOW);

	// header: "Loadout", the map line; the round status and the close button at the right
	{
		const vgui2::HFont h1 = Cond(40), sub = Body(15);
		const int base = Y(22) + P(48) - P(10); // the title's baseline (40 px line in a 48 box)
		const int tw = DrawText(h1, C_GREEN, X(28), Y(22) + (P(48) - FontTall(h1)) / 2, L"Loadout");
		const int subX = X(28) + tw + P(18), subY = base - FontTall(sub) + P(3);
		FillRect(wx, Y(88), m_iWinW, 1, C_LINE);

		m_rClose = { X(1320 - 28 - 44), Y(25), P(44), P(44) };
		const bool hotClose = m_rClose.Has(m_iMouseX, m_iMouseY);
		OutlineRect(m_rClose.x, m_rClose.y, m_rClose.w, m_rClose.h, hotClose ? C_GREEN : C_BORDER);
		const vgui2::HFont closeFont = Body(20);
		const std::wstring x = L"\x00d7";
		DrawTextMid(closeFont, C_GREEN, m_rClose.x + (m_rClose.w - TextWide(closeFont, x)) / 2, m_rClose.y, m_rClose.h, x);

		// the round (server "RoundStatus"): NEXT ROUND IN m:ss, waiting for players, a round in progress; nothing from
		// an older server or on Bounty Killer maps
		int secs = 0;
		const int status = sbo::GetRoundStatus(secs);
		int headerRight = m_rClose.x - P(16); // the room left of the round status box
		if (status != sbo::ROUNDSTATUS_NONE)
		{
			const vgui2::HFont lab = Body(13), num = Cond(22);
			std::wstring l, t;
			if (status == sbo::ROUNDSTATUS_NEXT)
			{
				char buf[16];
				snprintf(buf, sizeof(buf), "%d:%02d", secs / 60, secs % 60);
				l = L"NEXT ROUND IN";
				t = Widen(buf);
			}
			else
				l = status == sbo::ROUNDSTATUS_WAITING ? L"WAITING FOR PLAYERS" : L"ROUND IN PROGRESS";
			const int lw = DrawSpaced(lab, C_SUB, 0, 0, l, P(1), false);
			// the time in fixed cells (Barlow's digits are not tabular): every digit gets the widest digit's width, so
			// the box and the label do not move while it counts; "m:ss" up to 9:59, "mm:ss" for longer waits
			int digitW = 0;
			for (wchar_t d = L'0'; d <= L'9'; d++)
			{
				const wchar_t one[2] = { d, 0 };
				digitW = std::max(digitW, TextWide(num, one));
			}
			const int colonW = TextWide(num, L":");
			const int slotW = t.empty() ? 0 : (int)(t.size() - 1) * digitW + colonW;
			const int bw = P(14) + lw + (t.empty() ? 0 : P(10) + slotW) + P(14);
			const int bx = m_rClose.x - P(16) - bw;
			headerRight = bx - P(12);
			FillRect(bx, m_rClose.y, bw, m_rClose.h, C_TIMER_BG);
			OutlineRect(bx, m_rClose.y, bw, m_rClose.h, C_TIMER_LINE);
			DrawSpaced(lab, C_SUB, bx + P(14), m_rClose.y + (m_rClose.h - FontTall(lab)) / 2, l, P(1));
			int cx = bx + P(14) + lw + P(10);
			for (wchar_t ch : t)
			{
				const wchar_t one[2] = { ch, 0 };
				const int cellW = ch == L':' ? colonW : digitW;
				DrawTextMid(num, C_GOLD, cx + (cellW - TextWide(num, one)) / 2, m_rClose.y, m_rClose.h, one);
				cx += cellW;
			}
		}

		// the loadout summary left of the round status; the map line is cut to leave it room
		const int sw = PaintStats(0, 0, m_rClose.h, false), sx = headerRight - sw;
		FillRect(sx, m_rClose.y, sw, m_rClose.h, C_FIELD);
		OutlineRect(sx, m_rClose.y, sw, m_rClose.h, C_SLOT_LINE);
		PaintStats(sx, m_rClose.y, m_rClose.h, true);
		DrawText(sub, C_SUB, subX, subY, Fit(sub, HeaderLine(), std::max(0, sx - P(16) - subX)));
	}

	// left rail
	FillRect(X(260), Y(89), 1, P(718 - 89), C_LINE);
	{
		const vgui2::HFont label = Body(12);
		DrawSpaced(label, C_DIM, X(18), Y(89 + 14), L"LOADOUT", P(1.4f));
		// drop-down and dice
		m_rDrop = { X(18), Y(89 + 14 + 15 + 8), P(260 - 36 - 46), P(40) };
		m_rDice = { m_rDrop.x + m_rDrop.w + P(6), m_rDrop.y, P(40), P(40) };
		FillRect(m_rDrop.x, m_rDrop.y, m_rDrop.w, m_rDrop.h, C_FIELD);
		OutlineRect(m_rDrop.x, m_rDrop.y, m_rDrop.w, m_rDrop.h, m_bList || m_rDrop.Has(m_iMouseX, m_iMouseY) ? C_GREEN : C_BORDER);
		const int cur = CurrentPreset();
		const vgui2::HFont dropFont = Body(15);
		DrawTextMid(dropFont, C_TEXT_HI, m_rDrop.x + P(12), m_rDrop.y, m_rDrop.h,
		    Fit(dropFont, cur >= 0 ? Widen(m_Presets[cur].name.c_str()) : std::wstring(L"Unsaved loadout"), m_rDrop.w - P(40)));
		// chevron (12 px, a 4-4 "v" of 1.6 px)
		{
			const int cx = m_rDrop.x + m_rDrop.w - P(12) - P(6), cy = m_rDrop.y + m_rDrop.h / 2;
			const int a = std::max(3, P(4)), t = std::max(1, P(1.6f));
			vgui2::surface()->DrawSetColor(C_GREEN);
			for (int i = 0; i <= a; i++)
			{
				vgui2::surface()->DrawFilledRect(cx - a + i, cy - a / 2 + i, cx - a + i + t, cy - a / 2 + i + t);
				vgui2::surface()->DrawFilledRect(cx + a - i - t + 1, cy - a / 2 + i, cx + a - i + 1, cy - a / 2 + i + t);
			}
		}
		FillRect(m_rDice.x, m_rDice.y, m_rDice.w, m_rDice.h, C_FIELD);
		OutlineRect(m_rDice.x, m_rDice.y, m_rDice.w, m_rDice.h, m_rDice.Has(m_iMouseX, m_iMouseY) ? C_GREEN : C_BORDER);
		{
			// the die: a 17 px square outline with five 2.8 px dots
			const int s = P(17), dx = m_rDice.x + (m_rDice.w - s) / 2, dy = m_rDice.y + (m_rDice.h - s) / 2;
			const int t = std::max(1, P(1.6f));
			FillRect(dx, dy, s, t, C_GREEN);
			FillRect(dx, dy + s - t, s, t, C_GREEN);
			FillRect(dx, dy, t, s, C_GREEN);
			FillRect(dx + s - t, dy, t, s, C_GREEN);
			const int r = std::max(2, P(2.8f));
			const float pts[5][2] = { { 5, 5 }, { 12, 5 }, { 8.5f, 8.5f }, { 5, 12 }, { 12, 12 } };
			for (const auto &p : pts)
				FillRect(dx + P(p[0]) - r / 2, dy + P(p[1]) - r / 2, r, r, C_GREEN);
		}
		FillRect(wx + 1, Y(89 + 14 + 15 + 8 + 40 + 12), P(259), 1, C_LINE);

		// the seven slots
		int y = Y(89 + 14 + 15 + 8 + 40 + 12 + 1 + 6);
		for (int i = 0; i < NUM_SLOTS; i++)
		{
			const bool on = i == m_iCat;
			m_rRail[i] = { wx + 1, y, P(259), P(56) };
			if (on)
				FillRect(m_rRail[i].x, y, m_rRail[i].w, m_rRail[i].h, C_RAIL_ON);
			if (on)
				FillRect(m_rRail[i].x, y, P(3), m_rRail[i].h, C_GREEN);
			const int kx = X(18), ky = y + (P(56) - P(26)) / 2;
			OutlineRect(kx, ky, P(26), P(26), on ? C_GREEN : C_BORDER);
			char key[4];
			snprintf(key, sizeof(key), "%d", i + 1);
			const vgui2::HFont kf = Body(13);
			DrawTextMid(kf, on ? C_GREEN : C_DIM, kx + (P(26) - TextWide(kf, Widen(key))) / 2, ky, P(26), Widen(key));
			const int tx = kx + P(26) + P(14);
			const int textTop = y + (P(56) - LH(15, 12) - P(2) - LH(19, 16)) / 2;
			DrawSpaced(Body(12), C_DIM, tx, textTop, Widen(Slots()[i].title), P(1.4f));
			const vgui2::HFont pf = Medium(16);
			DrawText(pf, on ? C_TEXT_HI : C_PICK, tx, textTop + LH(15, 12) + P(2), Fit(pf, PickName(i), X(260) - tx - P(18)));
			y += P(56) + P(2);
		}
	}

	// the items' heading and the Assassin notice
	{
		const vgui2::HFont h2 = Cond(26), hint = Body(13);
		const int top = Y(89 + 22);
		DrawText(h2, C_TEXT, X(261 + 24), top, Widen(cat.heading));
		const std::wstring h = L"Click to equip \x00b7 hover for details \x00b7 keys 1\x2013" L"7 switch slot";
		DrawText(hint, C_DIM, X(939 - 24) - TextWide(hint, h), top + FontTall(h2) - FontTall(hint) - P(3), h);
		m_rUnlock = Rect();
		if (Locked(m_iCat))
		{
			const int nx = X(261 + 24), ny = Y(89 + 22 + 31 + 16), nw = P(939 - 261 - 48), nh = NoticeTall();
			FillRect(nx, ny, nw, nh, C_GOLD_BG);
			OutlineRect(nx, ny, nw, nh, C_GOLD_LINE);
			const vgui2::HFont bf = Body(13);
			const std::wstring btn = L"Remove Assassin";
			const int bw = TextWide(bf, btn) + P(24);
			m_rUnlock = { nx + nw - P(14) - bw, ny + (nh - P(32)) / 2, bw, P(32) };
			OutlineRect(m_rUnlock.x, m_rUnlock.y, m_rUnlock.w, m_rUnlock.h, m_rUnlock.Has(m_iMouseX, m_iMouseY) ? C_GOLD : C_GOLD_LINE);
			DrawTextMid(bf, C_GOLD, m_rUnlock.x + P(12), m_rUnlock.y, m_rUnlock.h, btn);
			const vgui2::HFont tf = Body(14);
			const std::vector<std::wstring> lines = NoticeLines();
			int ly = ny + (nh - (int)lines.size() * LH(20, 14)) / 2;
			for (const std::wstring &l : lines)
			{
				DrawTextMid(tf, C_GOLD_TEXT, nx + P(14), ly, LH(20, 14), l);
				ly += LH(20, 14);
			}
		}
	}

	// details (right)
	{
		const Item &it = Shown();
		const int ax = X(940);
		FillRect(ax + 1, Y(89 + 1), P(380) - 1, P(718 - 90), C_ASIDE);
		FillRect(ax, Y(89), 1, P(718 - 89), C_LINE);
		const int x = X(940 + 1 + 24), w = P(380 - 1 - 48);
		int y = Y(89 + 1 + 20);
		// the whole picture, contained, with the magnifier
		m_rPicture = { x, y, w, P(190) };
		FillRect(x, y, w, P(190), C_PIC_BG);
		OutlineRect(x, y, w, P(190), C_LINE);
		const int ph = P(190) - 2;
		DrawPicture(it.image, x + (w - ph) / 2, y + 1, ph, ph, false, 1.0f, C_PIC_BG);
		{
			const int ms = P(26), mx = x + w - P(6) - ms, my = y + P(190) - P(6) - ms;
			FillRect(mx, my, ms, ms, Color(8, 12, 9, 230));
			OutlineRect(mx, my, ms, ms, C_BORDER);
			// a lens (a ring of 4.5 px) and its handle, in 1.6 px strokes, from points
			const float cxm = mx + ms / 2.0f - P(1.5f), cym = my + ms / 2.0f - P(1.5f), rad = m_flK * 4.5f;
			vgui2::surface()->DrawSetColor(C_SOFT);
			const int dot = std::max(1, P(1.6f));
			for (int a = 0; a < 48; a++)
			{
				const float t = a * 6.2831853f / 48;
				vgui2::surface()->DrawFilledRect((int)(cxm + rad * cosf(t)), (int)(cym + rad * sinf(t)),
				    (int)(cxm + rad * cosf(t)) + dot, (int)(cym + rad * sinf(t)) + dot);
			}
			for (int i = 0; i <= P(4); i++)
			{
				const int hx = (int)(cxm + rad * 0.75f) + i, hy = (int)(cym + rad * 0.75f) + i;
				vgui2::surface()->DrawFilledRect(hx, hy, hx + dot + 1, hy + dot + 1);
			}
		}
		y += P(190) + P(14);
		DrawSpaced(Body(12), C_DIM, x, y, Widen(Upper(ShownHeading()).c_str()), P(1.4f));
		y += LH(15, 12) + P(4);
		const vgui2::HFont nameFont = Cond(30);
		DrawText(nameFont, C_GREEN, x, y, Fit(nameFont, Widen(it.name), w));
		y += std::max(P(36), FontTall(nameFont));
		y += P(14);
		// stats
		const vgui2::HFont sf = Body(13), sv = Medium(13);
		auto statRow = [&](const char *label, const std::wstring &value, float bar) {
			DrawText(sf, C_DIM, x, y, Widen(label));
			DrawText(sv, C_TEXT_HI, x + w - TextWide(sv, value), y, value);
			y += LH(16, 13);
			if (bar >= 0.0f)
			{
				y += P(4);
				FillRect(x, y, w, P(4), C_BAR_BG);
				// no fill at 0; any value above 0 shows at least a 4% stub
				if (bar > 0.0f)
					FillRect(x, y, std::max(1, (int)(w * std::max(0.04f, std::min(1.0f, bar)))), P(4), C_GREEN);
				y += P(4);
			}
			y += P(10);
		};
		if (it.weapon)
		{
			statRow("Damage", Widen(it.dmg), it.dmax / 520.0f);
			if (it.clip)
			{
				char buf[16];
				snprintf(buf, sizeof(buf), "%d", it.clip);
				statRow("Magazine", Widen(buf), logf(it.clip + 1.0f) / logf(301.0f));
			}
			statRow("Fire rate", Widen(it.rate), logf(it.rps + 1.0f) / logf(101.0f));
			char sp[16];
			snprintf(sp, sizeof(sp), "%d%%", it.speed);
			statRow("Mobility", it.speed ? Widen(sp) : std::wstring(L"No penalty"), (100 + it.speed) / 100.0f);
		}
		else
		{
			for (const Line &r : it.rows)
			{
				const int bonus = !strcmp(r.k, "Speed") ? SpeedBonus(it.token) : 0;
				if (bonus)
					statRow(r.k, SpeedBonusText(bonus, false), bonus / (float)SBO_NORMAL_SPEED);
				else
					statRow(r.k, Widen(r.v), r.bar);
			}
		}
		y += P(4);
		if (it.weapon && !it.modes.empty())
		{
			FillRect(x, y, w, 1, C_LINE);
			y += P(11);
			// the key column: the design's 64, wider when the font (at its minimum size) needs it
			int keyW = P(64);
			for (const Line &m : it.modes)
				keyW = std::max(keyW, TextWide(sf, Widen(m.k)));
			for (const Line &m : it.modes)
			{
				DrawText(sf, C_DIM, x, y, Widen(m.k));
				DrawText(sf, C_MODEV, x + keyW + P(10), y, Fit(sf, Widen(m.v), w - keyW - P(10)));
				y += LH(16, 13) + P(6);
			}
			y += P(14) - P(6);
		}
		FillRect(x, y, w, 1, C_LINE);
		y += P(11);
		DrawSpaced(Body(12), C_DIM, x, y, L"INFO", P(1.4f));
	}

	// bottom bar: the seven choices (equal widths), Cancel and "Go!" (as the map briefing; 1.38's loadout said "Go ! ", vgui_SyndBrief.cpp)
	{
		const int by = Y(718);
		FillRect(wx, by, m_iWinW, 1, C_LINE);
		FillRect(wx + 1, by + 1, m_iWinW - 2, Y(800) - by - 2, C_BOTTOM);
		const int bh = P(48), top = by + 1 + P(16);
		const vgui2::HFont cf = Body(16), df = Semi(17);
		const std::wstring go = L"Go!";
		const int deployW = DrawSpaced(df, C_DEPLOY_TEXT, 0, 0, go, P(1), false) + P(68);
		const int cancelW = TextWide(cf, L"Cancel") + P(52);
		m_rDeploy = { X(1320 - 24) - deployW, top, deployW, bh };
		m_rCancel = { m_rDeploy.x - P(14) - cancelW, top, cancelW, bh };
		FillRect(m_rDeploy.x, top, deployW, bh, m_rDeploy.Has(m_iMouseX, m_iMouseY) ? Color(56, 124, 50, 255) : C_DEPLOY);
		OutlineRect(m_rDeploy.x, top, deployW, bh, C_DEPLOY_LINE);
		DrawSpaced(df, C_DEPLOY_TEXT, m_rDeploy.x + P(34), top + (bh - FontTall(df)) / 2, go, P(1));
		OutlineRect(m_rCancel.x, top, cancelW, bh, m_rCancel.Has(m_iMouseX, m_iMouseY) ? C_GREEN : C_BORDER);
		DrawTextMid(cf, C_SOFT, m_rCancel.x + P(26), top, bh, L"Cancel");
		const int left = X(24), right = m_rCancel.x - P(14);
		const int gap = P(8), sw = (right - left - gap * (NUM_SLOTS - 1)) / NUM_SLOTS;
		const vgui2::HFont lf = Body(10), nf = Body(13);
		for (int i = 0; i < NUM_SLOTS; i++)
		{
			const int sx = left + i * (sw + gap);
			m_rSlots[i] = { sx, top, sw, bh };
			FillRect(sx, top, sw, bh, C_FIELD);
			OutlineRect(sx, top, sw, bh, m_rSlots[i].Has(m_iMouseX, m_iMouseY) ? C_GREEN : C_SLOT_LINE);
			const char *image = Locked(i) ? "assassin" : Slots()[i].items[m_Sel[i]].image;
			DrawPicture(image, sx + P(4), top + P(5), P(38), P(38), true, 1.0f, C_FIELD);
			const int tx = sx + P(4) + P(38) + P(8), tw = sx + sw - P(8) - tx;
			const int ty = top + (bh - LH(12, 10) - P(1) - LH(16, 13)) / 2;
			// the slot label, letter-spaced when it fits, else plain and cut (small screens: the font's minimum size)
			const std::wstring label = Widen(Slots()[i].title);
			if (DrawSpaced(lf, C_DIM, 0, 0, label, P(1.2f), false) <= tw)
				DrawSpaced(lf, C_DIM, tx, ty, label, P(1.2f));
			else
				DrawText(lf, C_DIM, tx, ty, Fit(lf, label, tw));
			DrawText(nf, C_MODEV, tx, ty + LH(12, 10) + P(1), Fit(nf, Locked(i) ? Widen(AssassinSets(i)) : Widen(Slots()[i].items[m_Sel[i]].name), tw));
		}
	}

	// window frame last (over the parts' edges)
	OutlineRect(wx, wy, m_iWinW, m_iWinH, C_BORDER);


}

// after the children (the cards' area would cover it): the dice's tooltip
void CSboLoadoutMenu::PostChildPaint()
{
	BaseClass::PostChildPaint();
	// the dice's tooltip (the menus' tooltip look: dark box, 1 px green frame), under it after 0.5 s
	const double now = vgui2::system()->GetCurrentTime();
	if (!m_rDice.Has(m_iMouseX, m_iMouseY) || m_bList)
		m_flDiceHover = -1.0;
	else if (m_flDiceHover < 0.0)
		m_flDiceHover = now;
	if (m_flDiceHover >= 0.0 && now - m_flDiceHover >= 0.5)
	{
		const vgui2::HFont tf = Body(13);
		const std::wstring t = L"Random loadout: a random pick for every slot";
		const int tw = TextWide(tf, t) + P(16), th = FontTall(tf) + P(10);
		const int tx = std::min(m_rDice.x, GetWide() - tw), ty = m_rDice.y + m_rDice.h + P(4);
		FillRect(tx, ty, tw, th, Color(11, 14, 11, 255));
		OutlineRect(tx, ty, tw, th, Color(95, 174, 69, 255));
		DrawTextMid(tf, C_TEXT, tx + P(8), ty, th, t);
	}

	// a full primary's tooltip (the same look), under the card's name after 0.5 s
	const int fullCard = m_iCat == SLOT_PRIMARY && !Locked(m_iCat) && !m_bList && !m_bZoom && m_iHover >= 0 &&
	                             m_iHover < (int)m_Cards.size() && HeavyFull(m_iHover)
	                         ? m_iHover
	                         : -1;
	if (fullCard != m_iFullHover)
	{
		m_iFullHover = fullCard;
		m_flFullHover = fullCard >= 0 ? now : -1.0;
	}
	if (m_iFullHover >= 0 && now - m_flFullHover >= 0.5)
	{
		int taken = 0, max = 0;
		HeavyFull(m_iFullHover, &taken, &max);
		char buf[96];
		snprintf(buf, sizeof(buf), "Only %d %s can carry this; all are taken", max, max == 1 ? "player" : "players");
		const std::wstring t = Widen(buf);
		const vgui2::HFont tf = Body(13);
		const int tw = TextWide(tf, t) + P(16), th = FontTall(tf) + P(10);
		int gx, gy;
		m_pGrid->GetPos(gx, gy);
		const Rect &c = m_Cards[m_iFullHover];
		const int nameY = gy + c.y - m_GridScroll.offset + 1 + P(128) + P(10);
		const int tx = std::max(0, std::min(gx + c.x + P(12), GetWide() - tw));
		int ty = nameY + LH(19, 16) + P(4);
		if (ty + th > GetTall())
			ty = nameY - P(4) - th;
		FillRect(tx, ty, tw, th, Color(11, 14, 11, 255));
		OutlineRect(tx, ty, tw, th, Color(95, 174, 69, 255));
		DrawTextMid(tf, C_TEXT, tx + P(8), ty, th, t);
	}

	// the loadout summary's tooltip: what its numbers leave out (the same look, under or above it)
	if (!m_rStats.Has(m_iMouseX, m_iMouseY) || m_bList)
		m_flStatsHover = -1.0;
	else if (m_flStatsHover < 0.0)
		m_flStatsHover = now;
	if (m_flStatsHover >= 0.0 && now - m_flStatsHover >= 0.5)
	{
		const LoadoutStats s = ComputeStats(m_Sel);
		char buf[128];
		if (s.levelKnown)
			snprintf(buf, sizeof(buf), "Speed: at spawn, with your level (%d) included. 100%% = level 0 without a load", s.level);
		else
			snprintf(buf, sizeof(buf), "Speed: at spawn, without your level. 100%% = no load");
		const std::wstring lines[2] = { Widen(buf), L"Health: 100 plus cyberware; your own level-ups and penalties are not included" };
		const vgui2::HFont tf = Body(13);
		const int lh = FontTall(tf) + P(2);
		const int tw = std::max(TextWide(tf, lines[0]), TextWide(tf, lines[1])) + P(16), th = 2 * lh + P(10);
		const int tx = std::max(0, std::min(m_rStats.x + m_rStats.w - tw, GetWide() - tw));
		int ty = m_rStats.y + m_rStats.h + P(4);
		if (ty + th > GetTall())
			ty = m_rStats.y - P(4) - th;
		FillRect(tx, ty, tw, th, Color(11, 14, 11, 255));
		OutlineRect(tx, ty, tw, th, Color(95, 174, 69, 255));
		for (int i = 0; i < 2; i++)
			DrawText(tf, C_TEXT, tx + P(8), ty + P(5) + i * lh, lines[i]);
	}
}

void CSboLoadoutMenu::PaintArea(CSboLoadoutArea *pArea)
{
	const int W = pArea->GetWide(), H = pArea->GetTall();
	switch (pArea->m_eKind)
	{
	case CSboLoadoutArea::AREA_GRID:
	{
		const Slot &s = Slots()[m_iCat];
		const bool locked = Locked(m_iCat);
		const vgui2::HFont nameFont = Semi(16), chipFont = Body(12), eqFont = Semi(11);
		// a page Assassin sets: the whole card at the design's opacity 0.35 over the window, as mixed colors (no
		// translucent drawing: every part is drawn opaque in its dimmed color)
		const float op = locked ? 0.35f : 1.0f;
		for (size_t i = 0; i < m_Cards.size() && i < s.items.size(); i++)
		{
			const Item &it = s.items[i];
			const Rect &c = m_Cards[i];
			const int y = c.y - m_GridScroll.offset;
			if (y + c.h < 0 || y > H)
				continue;
			// a primary the team limit has made full: grayed out as a locked page, a FULL badge, not pickable
			const bool full = m_iCat == SLOT_PRIMARY && !locked && HeavyFull((int)i);
			const float cop = full ? 0.35f : op;
			auto D = [cop](Color c) {
				const Color bg(10, 14, 11, 255);
				return Color((int)(c.r() * cop + bg.r() * (1 - cop) + 0.5f), (int)(c.g() * cop + bg.g() * (1 - cop) + 0.5f),
				    (int)(c.b() * cop + bg.b() * (1 - cop) + 0.5f), 255);
			};
			const bool eq = (int)i == m_Sel[m_iCat] && !locked;
			FillRect(c.x, y, c.w, c.h, D(eq ? C_CARD_ON : C_CARD));
			// the picture: 128 high, covered, at 0.92 opacity
			DrawPicture(it.image, c.x + 1, y + 1, c.w - 2, P(128), true, 0.92f * cop, D(C_IMG_BG));
			if (full)
			{
				const std::wstring t = L"FULL";
				const int tw = DrawSpaced(eqFont, C_EQUIP_TEXT, 0, 0, t, P(1), false);
				const int bw = tw + P(16), bh = FontTall(eqFont) + P(4);
				const int bx = c.x + P(8), by = y + P(8);
				FillRect(bx, by, bw, bh, C_SOFT);
				DrawSpaced(eqFont, C_EQUIP_TEXT, bx + P(8), by + P(2), t, P(1));
			}
			if (eq)
			{
				const std::wstring t = L"EQUIPPED";
				const int tw = DrawSpaced(eqFont, C_EQUIP_TEXT, 0, 0, t, P(1), false);
				const int bw = tw + P(16), bh = FontTall(eqFont) + P(4);
				const int bx = c.x + c.w - P(8) - bw, by = y + P(8);
				FillRect(bx, by, bw, bh, C_GREEN);
				DrawSpaced(eqFont, C_EQUIP_TEXT, bx + P(8), by + P(2), t, P(1));
			}
			// what Assassin gives (coop.cpp ~1777: the .444k, medikit, grenades, satchels, night vision, the targeting
			// system's double damage, armor skin): a gold badge at the picture's bottom left while it is picked
			static const char *const s_Kit[] = { "k444", "medikit", "grenade", "satchel", "eye", "targeting", "skin" };
			bool kit = false;
			for (const char *k : s_Kit)
				kit = kit || !strcmp(it.token, k);
			if (kit && Assassin())
			{
				const std::wstring t = L"WITH ASSASSIN";
				const int tw = DrawSpaced(eqFont, C_EQUIP_TEXT, 0, 0, t, P(1), false);
				const int bw = tw + P(16), bh = FontTall(eqFont) + P(4);
				const int bx = c.x + P(8), by = y + 1 + P(128) - P(8) - bh;
				FillRect(bx, by, bw, bh, D(C_GOLD));
				DrawSpaced(eqFont, D(C_GOLD_BG), bx + P(8), by + P(2), t, P(1));
			}
			int ty = y + 1 + P(128) + P(10);
			DrawText(nameFont, D(C_TEXT_HI), c.x + P(12), ty, Fit(nameFont, Widen(it.name), c.w - P(24)));
			ty += LH(19, 16) + P(6);
			std::vector<std::wstring> chips;
			if (it.weapon)
			{
				char buf[48];
				chips.push_back(Widen(it.dmg) + L" dmg");
				if (it.clip)
				{
					snprintf(buf, sizeof(buf), "%d rds", it.clip);
					chips.push_back(Widen(buf));
				}
				if (it.speed)
				{
					snprintf(buf, sizeof(buf), "%d%% speed", it.speed);
					chips.push_back(Widen(buf));
				}
			}
			else
			{
				for (const char *ch : it.chips)
					chips.push_back(!strcmp(ch, "Speed") && SpeedBonus(it.token) ? SpeedBonusText(SpeedBonus(it.token), true) : Widen(ch));
			}
			int cx = c.x + P(12);
			for (const std::wstring &ch : chips)
			{
				const int cw = TextWide(chipFont, ch) + P(14) + 2;
				if (cx > c.x + P(12) && cx + cw > c.x + c.w - P(12))
				{
					cx = c.x + P(12);
					ty += LH(20, 12) + P(6);
				}
				OutlineRect(cx, ty, cw, LH(20, 12), D(C_CHIP_LINE));
				DrawTextMid(chipFont, D(C_SOFT), cx + P(7) + 1, ty, LH(20, 12), ch);
				cx += cw + P(6);
			}
			const bool hot = (int)i == m_iHover && !locked && !full;
			OutlineRect(c.x, y, c.w, c.h, D(eq ? C_GREEN : (hot ? C_BORDER : C_CARD_LINE)));
		}
		m_GridScroll.Draw(W - P(8), 0, P(8), pArea->m_iMouseX >= W - P(8));
		break;
	}
	case CSboLoadoutArea::AREA_INFO:
	{
		const vgui2::HFont f = Body(13);
		for (size_t i = 0; i < m_InfoLines.size(); i++)
		{
			const int y = (int)i * LH(19, 13) - m_InfoScroll.offset;
			if (y + LH(19, 13) < 0 || y > H)
				continue;
			DrawText(f, C_SOFT, 0, y, m_InfoLines[i]);
		}
		m_InfoScroll.Draw(W - P(8), 0, P(8), pArea->m_iMouseX >= W - P(8));
		break;
	}
	case CSboLoadoutArea::AREA_LIST:
	{
		FillRect(0, 0, W, H, C_LIST_BG);
		const int cur = CurrentPreset();
		const vgui2::HFont f = Body(14), hf = Body(12), bf = Body(13);
		const int rowH = P(38), listH = m_ListScroll.view;
		for (size_t i = 0; i < m_Presets.size(); i++)
		{
			// whole rows only, from the first one the scroll position reaches (it scrolls above the footer, nothing
			// under it)
			const int first = (m_ListScroll.offset + rowH / 2) / std::max(1, rowH);
			const int y = 1 + ((int)i - first) * rowH;
			if (y < 1 || y + rowH > 1 + listH)
				continue;
			const bool on = (int)i == cur;
			const bool hot = pArea->m_iMouseY >= y && pArea->m_iMouseY < y + rowH && pArea->m_iMouseY < listH && pArea->m_iMouseX >= 0;
			if (on || hot)
				FillRect(1, y, W - 2, std::min(rowH, listH + 1 - y), on ? C_LIST_ON : C_FIELD);
			if (on)
				FillRect(1, y, P(3), std::min(rowH, listH + 1 - y), C_GREEN);
			DrawTextMid(f, on ? C_TEXT_HI : C_PICK, P(12), y, rowH, Fit(f, Widen(m_Presets[i].name.c_str()), W - P(100)));
			if (on)
				DrawTextMid(hf, C_DIM, W - P(12) - TextWide(hf, L"Equipped") - (m_ListScroll.Needed() ? P(8) : 0), y, rowH, L"Equipped");
		}
		// buttons under the list (over any row scrolled below it)
		const int by = 1 + listH;
		FillRect(1, by, W - 2, H - by - 1, C_LIST_BG);
		FillRect(1, by, W - 2, 1, C_LINE);
		const wchar_t *labels[3] = { L"Save", L"Rename", L"Delete" };
		const int bw = (W - 2) / 3;
		for (int b = 0; b < 3; b++)
		{
			const int bx = 1 + b * bw;
			const bool enabled = b == 0 || cur >= 0;
			const bool hot = enabled && pArea->m_iMouseY > by && pArea->m_iMouseX >= bx && pArea->m_iMouseX < bx + bw;
			if (hot)
				FillRect(bx, by + 1, bw, H - by - 2, C_FIELD);
			if (b < 2)
				FillRect(bx + bw - 1, by + 1, 1, H - by - 2, C_LINE);
			const Color col = !enabled ? C_DIM : (b == 0 ? C_GREEN : C_SOFT);
			DrawTextMid(bf, col, bx + (bw - TextWide(bf, labels[b])) / 2, by + 1, H - by - 2, labels[b]);
		}
		m_ListScroll.Draw(W - 1 - P(8), 1, P(8), pArea->m_iMouseX >= W - 1 - P(8) && pArea->m_iMouseY < listH);
		OutlineRect(0, 0, W, H, C_LIST_LINE);
		break;
	}
	case CSboLoadoutArea::AREA_ZOOM:
	{
		FillRect(0, 0, W, H, C_ZOOM_BG);
		const Item &it = Shown();
		const int s = std::min(P(640), H - P(80));
		const vgui2::HFont cf = Body(14);
		const int total = s + P(14) + FontTall(cf);
		const int x = (W - s) / 2, y = (H - total) / 2;
		DrawPicture(it.image, x, y, s, s, false, 1.0f, C_PIC_BG);
		OutlineRect(x - 1, y - 1, s + 2, s + 2, C_BORDER);
		const std::wstring cap = Widen(it.name) + L" \x00b7 click anywhere to close";
		DrawText(cf, C_SOFT, (W - TextWide(cf, cap)) / 2, y + s + P(14), cap);
		break;
	}
	case CSboLoadoutArea::AREA_NAME:
	{
		FillRect(0, 0, W, H, C_ZOOM_BG);
		const int bw = P(420), bh = P(150), bx = (W - bw) / 2, by = (H - bh) / 2;
		FillRect(bx, by, bw, bh, C_LIST_BG);
		OutlineRect(bx, by, bw, bh, C_LIST_LINE);
		DrawSpaced(Body(12), C_DIM, bx + P(20), by + P(22), m_bRenaming ? L"RENAME LOADOUT" : L"SAVE LOADOUT AS", P(1.4f));
		const vgui2::HFont bf = Body(14);
		const int bwid = P(90), bht = P(32), byy = by + bh - P(16) - bht;
		const Rect ok{ bx + bw - P(20) - bwid, byy, bwid, bht }, cancel{ ok.x - P(10) - bwid, byy, bwid, bht };
		const int mx = pArea->m_iMouseX, my = pArea->m_iMouseY;
		FillRect(ok.x, ok.y, ok.w, ok.h, ok.Has(mx, my) ? Color(56, 124, 50, 255) : C_DEPLOY);
		OutlineRect(ok.x, ok.y, ok.w, ok.h, C_DEPLOY_LINE);
		DrawTextMid(bf, C_DEPLOY_TEXT, ok.x + (ok.w - TextWide(bf, L"Save")) / 2, ok.y, ok.h, L"Save");
		OutlineRect(cancel.x, cancel.y, cancel.w, cancel.h, cancel.Has(mx, my) ? C_GREEN : C_BORDER);
		DrawTextMid(bf, C_SOFT, cancel.x + (cancel.w - TextWide(bf, L"Cancel")) / 2, cancel.y, cancel.h, L"Cancel");
		break;
	}
	}
}

//-----------------------------------------------------------------------------
// Input
//-----------------------------------------------------------------------------
void CSboLoadoutMenu::OnCursorMoved(int x, int y)
{
	m_iMouseX = x;
	m_iMouseY = y;
}

void CSboLoadoutMenu::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	if (m_bList && !m_rDrop.Has(x, y))
	{
		// a click outside the list closes it
		m_bList = false;
		LayoutMenu();
		return;
	}
	if (m_rClose.Has(x, y) || m_rCancel.Has(x, y))
	{
		Cancel();
		return;
	}
	if (m_rDeploy.Has(x, y))
	{
		Deploy();
		return;
	}
	if (m_rDrop.Has(x, y))
	{
		m_bList = !m_bList;
		LayoutMenu();
		return;
	}
	if (m_rDice.Has(x, y))
	{
		Randomize();
		return;
	}
	if (m_rPicture.Has(x, y))
	{
		m_bZoom = true;
		LayoutMenu();
		return;
	}
	if (m_rUnlock.w && m_rUnlock.Has(x, y))
	{
		Pick(SLOT_CYBER, 0);
		LayoutMenu();
		return;
	}
	for (int i = 0; i < NUM_SLOTS; i++)
	{
		if (m_rRail[i].Has(x, y) || m_rSlots[i].Has(x, y))
		{
			SetCat(i);
			LayoutMenu();
			return;
		}
	}
}

void CSboLoadoutMenu::AreaPressed(CSboLoadoutArea *pArea, int x, int y)
{
	const int W = pArea->GetWide(), H = pArea->GetTall();
	switch (pArea->m_eKind)
	{
	case CSboLoadoutArea::AREA_GRID:
	{
		if (m_GridScroll.Press(x, y, W - P(8), 0, P(8)))
			return;
		for (size_t i = 0; i < m_Cards.size(); i++)
		{
			Rect r = m_Cards[i];
			r.y -= m_GridScroll.offset;
			if (r.Has(x, y))
			{
				Pick(m_iCat, (int)i);
				return;
			}
		}
		break;
	}
	case CSboLoadoutArea::AREA_INFO:
		m_InfoScroll.Press(x, y, W - P(8), 0, P(8));
		break;
	case CSboLoadoutArea::AREA_LIST:
	{
		if (m_ListScroll.Press(x, y, W - 1 - P(8), 1, P(8)))
			return;
		const int listH = m_ListScroll.view;
		if (y < 1 + listH)
		{
			const int i = (y - 1) / P(38) + (m_ListScroll.offset + P(38) / 2) / std::max(1, P(38));
			if (i >= 0 && i < (int)m_Presets.size())
			{
				const int previous = m_Sel[SLOT_PRIMARY];
				for (int s = 0; s < NUM_SLOTS; s++)
				{
					m_Sel[s] = m_Presets[i].sel[s];
					// a full primary: the one picked before stays, as the server keeps it (FreeHeavy)
					if (s == SLOT_PRIMARY)
						m_Sel[s] = FreeHeavy(m_Sel[s], previous);
					sbo::SetBriefChoice(Slots()[s].choice, m_Sel[s] + 1);
				}
				m_iHover = -1;
			}
			m_bList = false;
			LayoutMenu();
			return;
		}
		const int b = (x - 1) * 3 / std::max(1, W - 2);
		const int cur = CurrentPreset();
		if (b == 0)
			OpenNameBox(false);
		else if (b == 1 && cur >= 0)
			OpenNameBox(true);
		else if (b == 2 && cur >= 0)
		{
			m_Presets.erase(m_Presets.begin() + cur);
			SaveFile();
			m_bList = false;
			LayoutMenu();
		}
		break;
	}
	case CSboLoadoutArea::AREA_ZOOM:
		m_bZoom = false;
		LayoutMenu();
		break;
	case CSboLoadoutArea::AREA_NAME:
	{
		const int bw = P(420), bh = P(150), bx = (W - bw) / 2, by = (H - bh) / 2;
		const int bwid = P(90), bht = P(32), byy = by + bh - P(16) - bht;
		const Rect ok{ bx + bw - P(20) - bwid, byy, bwid, bht }, cancel{ ok.x - P(10) - bwid, byy, bwid, bht };
		if (ok.Has(x, y))
			NameOk();
		else if (cancel.Has(x, y) || !Rect{ bx, by, bw, bh }.Has(x, y))
			CloseNameBox();
		break;
	}
	}
}

void CSboLoadoutMenu::AreaReleased(CSboLoadoutArea *pArea)
{
	m_GridScroll.dragging = m_InfoScroll.dragging = m_ListScroll.dragging = false;
}

void CSboLoadoutMenu::AreaMoved(CSboLoadoutArea *pArea, int x, int y)
{
	switch (pArea->m_eKind)
	{
	case CSboLoadoutArea::AREA_GRID:
	{
		m_GridScroll.Move(y, P(8));
		int hover = -1;
		for (size_t i = 0; i < m_Cards.size() && x >= 0; i++)
		{
			Rect r = m_Cards[i];
			r.y -= m_GridScroll.offset;
			if (r.Has(x, y))
				hover = (int)i;
		}
		if (hover != m_iHover)
		{
			m_iHover = hover;
			// the details follow the card hovered last on this page and stay when the mouse leaves (equipping is a
			// click): the picture can be enlarged and INFO read without equipping
			if (hover >= 0 && hover != m_Seen[m_iCat])
			{
				m_Seen[m_iCat] = hover;
				LayoutInfo();
			}
		}
		break;
	}
	case CSboLoadoutArea::AREA_INFO:
		m_InfoScroll.Move(y, P(8));
		break;
	case CSboLoadoutArea::AREA_LIST:
		m_ListScroll.Move(y, P(8));
		break;
	default:
		break;
	}
}

void CSboLoadoutMenu::AreaWheeled(CSboLoadoutArea *pArea, int delta)
{
	switch (pArea->m_eKind)
	{
	case CSboLoadoutArea::AREA_GRID: m_GridScroll.Wheel(delta, P(60)); break;
	case CSboLoadoutArea::AREA_INFO: m_InfoScroll.Wheel(delta, LH(19, 13) * 3); break;
	case CSboLoadoutArea::AREA_LIST: m_ListScroll.Wheel(delta, P(38)); break;
	default: break;
	}
}

void CSboLoadoutMenu::OnThink()
{
	BaseClass::OnThink();
	if (!IsVisible())
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	m_iMouseX = x;
	m_iMouseY = y;
	Repaint();
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API
//-----------------------------------------------------------------------------
bool VGui2_LoadoutMenu_Wanted()
{
	return SboNewIngameMenus() && g_pViewport;
}

void VGui2_LoadoutMenu_Show()
{
	if (CSboLoadoutMenu *p = CSboLoadoutMenu::Get())
		p->Open();
}

void VGui2_LoadoutMenu_Hide()
{
	if (CSboLoadoutMenu *p = CSboLoadoutMenu::Get())
	{
		if (p->IsVisible())
			p->HideMenu();
	}
}

bool VGui2_LoadoutMenu_SlotInput(int iSlot)
{
	CSboLoadoutMenu *p = CSboLoadoutMenu::Get();
	return p && p->SlotInput(iSlot);
}
