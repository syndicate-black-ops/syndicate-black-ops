//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: VGUI2 scoreboard. Port of BugfixedHL-Rebased src/game/client/vgui/score_panel.{h,cpp}
//          (layout, Steam avatars, mute icons, ping/loss, killer highlight, right-click player menu,
//          hud_scoreboard_* cvars), replacing SBO's VGUI1 ScorePanel. SBO: a mute button at the end of each row
//          (left-click toggles the voice mute, as the VGUI1 ScorePanel's rows did).
//
// SDK world; SBO data comes through sbo_bridge.h (players, TeamNames labels, team colours).
//
// Grouping (see docs/dev/SCOREBOARD_PORT.md):
//   * SBO category mode -- the server sends SBO's five scoreboard categories as teams (TeamNames:
//     1 "Game Master", 2 "Dead Ops", 3 "No Loadout", 4 "Alive Ops", 5 "Spectators";
//     ScoreInfo carries the team number): sections in the fixed order Game Master, Alive, Dead,
//     No Loadout, Spectators; empty sections are hidden. Bounty Killer maps: Alive, Dead, Spectators (the server puts
//     nobody in No Loadout there); the Hunted (ScoreInfo playerclass 1) has no section of its own: it is the first
//     row of the alive section (4), or of the dead one while dead, with a "(Hunted)" tag and a tinted row.
//   * otherwise (old SBO server, teamplay maps): BHL's grouping -- teams by ScoreInfo team number,
//     sorted by score, players without a TeamInfo team as spectators when others have one.
// Section headers and names use sbo::GetTeamColor / sbo::GetClientColor, the table the chat uses.
//=================================================================//

#ifndef SBO_VGUI2_SCORE_PANEL_H
#define SBO_VGUI2_SCORE_PANEL_H

#include <array>
#include <map>
#include <steam/steam_api.h>
#include <vgui_controls/Frame.h>

#include "sbo_bridge.h"

namespace vgui2
{
class Label;
class Menu;
class SectionedListPanel;
class ImageList;
}

class CAvatarImage;
enum class ColorCodeAction; // hud_chat.h

class CScorePanel : public vgui2::Frame
{
public:
	DECLARE_CLASS_SIMPLE(CScorePanel, vgui2::Frame);

	CScorePanel(vgui2::Panel *pParent);

	static CScorePanel *Get() { return m_sInstance; }

	/**
	 * Updates server name label. Called on ServerName message.
	 */
	void UpdateServerName();

	/**
	 * Shows or hides mouse pointer.
	 */
	void EnableMousePointer(bool bEnable);

	/**
	 * Updates player's score in the scoreboard. client 0: full refresh (TeamNames changed).
	 */
	void UpdateOnPlayerInfo(int client);

	/**
	 * Highlightes the killer of they killed this player.
	 */
	void DeathMsg(int killer, int victim);

	//! New map: hide, forget the killer highlight.
	void Reset();

	//! SBO: round / frags left line under the player count ("SboModeInfo", sbo::GetModeInfo; also called when the
	//! scores change).
	void UpdateModeInfo();
	void ShowPanel(bool state);

	//! A valid individual-account SteamID64 for the player, 0 if unknown. Also used by the voice labels' avatars.
	static uint64 GetValidSteamID64(const sbo::ScorePlayer &sp);
	//! SBO: the picture a slot without a SteamID names in its userinfo key "sbo_avatar" (gfx/avatars/*.tga), loaded
	//! once per path for every user (scoreboard, voice status); nullptr: none (a SteamID, no key, a bad path, no file)
	static class CTGAImage *GetFileAvatar(const sbo::ScorePlayer &sp, int client);

	// Frame overrides
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void OnThink() override;
	virtual void OnCommand(const char *command) override;
	virtual void PostChildPaint() override; //!< SBO: the badges and their tooltip, on top of the rows
	virtual void OnMousePressed(vgui2::MouseCode code) override; //!< SBO 2.0: a click on the objective line

	// Messages
	MESSAGE_FUNC_INT(OnItemContextMenu, "ItemContextMenu", itemID);
	MESSAGE_FUNC_INT(OnItemLeftClick, "ItemLeftClick", itemID); //!< SBO: the row's mute button, a badge's link

private:
	static constexpr int MAX_PLAYERS = sbo::SCORE_MAX_PLAYERS;
	//! Team numbers above this are shown in the unassigned (header) section.
	static constexpr int MAX_TEAMS = 16;
	static constexpr int TEAM_SPECTATOR = MAX_TEAMS + 1;
	static constexpr int HEADER_SECTION_ID = 0;
	//! SBO category mode: connected slots not in the game (GetPlayerTeam, IsGhost), the last section
	static constexpr int GHOST_SECTION_ID = MAX_TEAMS + 2;
	static constexpr int SECTION_COUNT = MAX_TEAMS + 3;
	static constexpr float HIGHLIGHT_KILLER_TIME = 10.f;
	static constexpr float UPDATE_PERIOD = 0.5f;
	//! SBO category numbers (server SboScoreboard_GetTeam) the Bounty Killer rows need.
	static constexpr int SBO_TEAM_HUNTED = 1; //!< "Game Master" on the other maps
	static constexpr int SBO_TEAM_DEAD = 2;
	static constexpr int SBO_TEAM_ALIVE = 4;
	static constexpr int SBO_TEAM_SPECTATORS = 5; //!< the TeamNames "Spectators" section
	//! ScoreInfo playerclass bits in category mode (server SboScoreboard_GetClass)
	static constexpr int SBO_CLASS_HUNTED = 1;
	static constexpr int SBO_CLASS_LOST = 2; //!< no command from the player's client for a few seconds
	static constexpr int GHOST_ALPHA = 120;  //!< of a ghost's row text, avatar and section header
	static constexpr int HUNTED_BG_ALPHA = 48; //!< of the Hunted's row tint (GetPlayerBgColor)

	enum class MenuAction
	{
		Mute,
		SteamProfile,
		SteamURL,
		CopyName,
		CopyNameRaw,
		CopySteamID,
		CopySteamID64,
	};

	enum class SizeMode
	{
		Auto = 0,
		Normal = 1,
		Compact = 2,
	};

	struct TeamData
	{
		int iFrags = 0;
		int iDeaths = 0;
		int iPlayerCount = 0;
	};

	struct PlayerData
	{
		bool bIsConnected = false;
		int nItemID = -1;
		int nTeamID = 0;
	};

	struct MenuData
	{
		// Menu items
		int nMuteItemID;
		int nProfilePageItemID;
		int nProfileUrlItemID;

		// Selected player info
		int nItemID;
		int nClient;
		uint64 nSteamID64;
	};

	static CScorePanel *m_sInstance;

	vgui2::SectionedListPanel *m_pPlayerList = nullptr;
	vgui2::Label *m_pServerNameLabel = nullptr;
	vgui2::Label *m_pMapNameLabel = nullptr;
	vgui2::Label *m_pPlayerCountLabel = nullptr;
	vgui2::Label *m_pModeInfoLabel = nullptr; //!< SBO: "Round: 2 / 7" (co-op) / "Frags left: 12", across from the map name
	vgui2::ImageList *m_pImageList = nullptr;
	vgui2::Menu *m_pPlayerMenu = nullptr;

	std::array<TeamData, SECTION_COUNT> m_TeamData;
	std::array<bool, SECTION_COUNT> m_IsTeamSectionCreated;
	std::array<PlayerData, MAX_PLAYERS + 1> m_PlayerData;
	std::array<int, SECTION_COUNT> m_SortedTeamIDs;
	std::map<CSteamID, CAvatarImage *> m_PlayerAvatars;
	MenuData m_MenuData;

	//! Player info cache, refreshed from the engine/SBO by UpdatePlayer().
	std::array<sbo::ScorePlayer, MAX_PLAYERS + 1> m_Players;

	int m_iKillerIndex = 0;
	float m_flKillerHighlightStart = 0;
	float m_flLastUpdateTime = 0;

	Color m_ThisPlayerBgColor = Color(0, 0, 0, 0);
	Color m_KillerBgColor = Color(0, 0, 0, 0);

	char m_szSpectatorTag[32] = "(spectator)";
	char m_szHuntedTag[32] = "(Hunted)"; //!< SBO: after the Hunted's name (Bounty Killer maps), "SBO_Scores_PlayerHunted"

	//! Coop XP column present in the current sections (ShouldShowXP() at the last RefreshItems).
	bool m_bShowXP = false;

	//! SBO mute buttons: frames (OnThink) left in which a row click doesn't toggle: the click that turned the cursor
	//! on (EnableMousePointer) may reach VGUI2 too, with the cursor already on a row.
	int m_iIgnoreMuteClickThinks = 0;
	//! SBO "Frags left": the value the intermission began with, -1 = none (not in intermission).
	int m_iIntermissionFragsLeft = -1;

	//! Client whose mute button the cursor is over, 0 = none.
	int m_iMuteHoverClient = 0;

	//! SBO: client whose badge (GetBadge) the cursor is over, 0 = none (OnThink): PostChildPaint draws its tooltip, a
	//! click opens its link.
	int m_iBadgeHoverClient = 0;

	//! SBO 2.0 trigger_objective: the objective list's revision the board was sized for (Resize); the list goes under
	//! the players (ObjectivesTall, PostChildPaint)
	int m_iObjectivesRevision = -1;
	int ObjectivesTall();
	//! the objective line's place in the board (false: no line); a click on it opens / closes the full list drawn
	//! upwards over the players (closed whenever the board closes)
	bool GetObjectivesLineBounds(int &x, int &y, int &wide, int &tall);
	bool m_bObjectivesOpen = false;
	int m_iObjectivesTop = 0; //!< the open list's top (PostChildPaint), for its clicks
	void SetObjectivesOpen(bool bOpen);

	//! Index of the "mute" column (the last one) in every section, -1 until a section exists (CreateSection).
	int m_iMuteColumn = -1;

	// Column widths
	CPanelAnimationVarAliasType(int, m_iColumnWidthAvatar, "column_avatar", "26", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthName, "column_name", "170", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthMute, "column_mute", "26", "proportional_int"); //!< SBO: the last column, as wide as the avatar column; taken from the name column
	CPanelAnimationVarAliasType(int, m_iColumnWidthSteamID, "column_steamid", "88", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthEff, "column_eff", "60", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthFrags, "column_frags", "60", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthXP, "column_xp", "60", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthDeaths, "column_deaths", "60", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthPing, "column_ping", "80", "proportional_int");

	CPanelAnimationVarAliasType(int, m_iMutedIconTexture, "muted_icon", "ui/gfx/muted_icon32", "textureid");
	CPanelAnimationVarAliasType(int, m_iUnmutedIconTexture, "unmuted_icon", "ui/gfx/unmuted_icon32", "textureid"); //!< SBO: mute button, not muted
	CPanelAnimationVarAliasType(int, m_iMinHeight, "min_height", "200", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iVerticalMargin, "vertical_margin", "30", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBottomPadding, "bottom_padding", "4", "proportional_int");

	//! Refreshes m_Players[client] from the engine / SBO.
	sbo::ScorePlayer &UpdatePlayer(int client);

	//! True when the server sends SBO's scoreboard categories as teams (see the file header).
	bool IsSboCategoryMode();

	//! SBO: Bounty Killer map and the server has the player as the Hunted (ScoreInfo playerclass 1, alive or dead).
	bool IsHunted(int client);

	//! Coop map and the server sent "SboXP" (this branch's server): show the XP column (taken from the name column).
	bool ShouldShowXP();

	//! Section label for a team (TeamNames label, else a member's TeamInfo name, else "Team N").
	void GetTeamLabel(int nTeamID, wchar_t *buf, int bufSizeInBytes);

	//! "0:Y:Z" (BHL GetSteamID without "STEAM_"), "" if unknown.
	void GetSteamIDString(const sbo::ScorePlayer &sp, char *buf, int bufSize);

	//! Player name for display. bNoColorCodes: colour codes removed, else kept unless hud_colortext 2 (BHL
	//! CPlayerInfo::GetDisplayName).
	const char *GetDisplayName(const sbo::ScorePlayer &sp, bool bNoColorCodes = true);

	//! Font of the rows (the name cells are drawn by CPlayerNameImage, see score_panel.cpp).
	vgui2::HFont GetNameFont();

	//! hud_colortext (the chat's cvar), Strip without the chat.
	ColorCodeAction GetColorCodeAction();

	/**
	 * Updates all labels, recreates all sections, recalculates all data.
	 * Called when panel becomes visible.
	 */
	void FullUpdate();

	/**
	 * Updates map name label.
	 */
	void UpdateMapName();

	/**
	 * Clears and recreates all sections and rows.
	 */
	void RefreshItems();

	/**
	 * Creates a section for specified team.
	 */
	void CreateSection(int nTeamID);

	/**
	 * Adds/removes/updates rows of all clients.
	 * Updates team player counts.
	 * Resizes the panel.
	 */
	void UpdateAllClients();

	/**
	 * Updates client's row in the scoreboard.
	 */
	void UpdateClientInfo(int client);

	/**
	 * Updates m_pImageList[i]: changes muted state and updates Steam avatar.
	 */
	void UpdateClientIcon(int client);

	//! SBO: frags the leading player (teamplay maps: team) still needs to reach the frag limit, never below 0.
	int GetFragsLeft(int fragLimit);

	//! SBO: the player has a mute button (not the local player, has a ban list key of his own: sbo::CanPlayerBeBlocked).
	bool CanMutePlayer(int client);

	//! SBO: mutes / unmutes the player's voice and says so in the chat (right-click menu and the mute buttons).
	void TogglePlayerMute(int client);

	//! SBO: client whose mute button is under the cursor, 0 = none (itemID -1: look at every row).
	int GetMuteButtonClientAtCursor(int itemID = -1);

	//! SBO: the mute button drawn as "under the cursor" (0 = none).
	void SetMuteHoverClient(int client);

	//! SBO: the player's badge (index into BADGES in score_panel.cpp, by SteamID64; test builds: test_cl_devicon), -1 =
	//! none.
	int GetBadge(int client);

	//! SBO: space a badge takes from its player's name cell (badge + gaps), 0 without a badge.
	int GetBadgeReserve(int client);

	//! SBO: a badge's size on a row: the row's height less a margin (scales with the scoreboard).
	int GetBadgeSlot();

	//! SBO: the largest size a badge is drawn at: 5/3 of the name font's height (about twice its letters); 0 = no cap
	//! (no font yet).
	int GetBadgeCap();

	/**
	 * SBO: where the player's badge is drawn, in this panel's coordinates: after the name (test builds:
	 * test_cl_devicon may put it after the score). False when the player has no visible row.
	 */
	bool GetBadgeBounds(int client, int &x, int &y, int &wide, int &tall);

	/**
	 * Updates team scores and player counts.
	 */
	void UpdateScoresAndCounts();

	/**
	 * Returns width of name column + widths of disabled columns.
	 */
	int GetNameColumnWidth();

	/**
	 * Returns player team [0; MAX_TEAMS] or TEAM_SPECTATOR (SBO category mode: GHOST_SECTION_ID too).
	 */
	int GetPlayerTeam(int client);
	//! SBO: a slot with a name but no player behind it as the server tells the client (no TeamInfo team, no SteamID:
	//! a plugin's virtual slot such as the radio's, HLTV): the Spectators section, left out of the counts. A real
	//! player who just connected has his SteamID already (HL25 engine), so he is not one.
	bool IsVirtualSlot(int client);

	/**
	 * SBO category mode: a connected slot that is not in the game: no category from the server yet (connecting,
	 * downloading, reconnecting after a crash or time-out) or the server has had no command from its client for a
	 * few seconds (SBO_CLASS_LOST). Listed under Ghosts, greyed, without score or ping.
	 */
	bool IsGhost(int client);

	/**
	 * Returns bright color if this is this player, fading red if it's the last killer, the Hunted's tint (Bounty
	 * Killer) or (0,0,0,0).
	 */
	Color GetPlayerBgColor(int client);

	/**
	 * Calculates efficiency based on hud_scoreboard_efftype.
	 */
	float CalculateEfficiency(int kills, int deaths);

	/**
	 * Returns client icon size.
	 */
	int GetClientIconSize();

	/**
	 * Creates player context menu that is shown on right-click.
	 */
	void CreatePlayerMenu();

	/**
	 * Opens context menu for specified item.
	 */
	void OpenPlayerMenu(int itemID);

	/**
	 * Handles player menu action.
	 */
	void OnPlayerMenuCommand(MenuAction command);

	/**
	 * Returns current size mode.
	 */
	SizeMode GetSizeMode();

	/**
	 * Returns compact line spacing for given screen height
	 */
	int GetLineSpacingForHeight(int h);

	/**
	 * Returns line spacing override value for normal size.
	 */
	int GetLineSpacingForNormal();

	/**
	 * Returns line spacing override value for compact size.
	 */
	int GetLineSpacingForCompact();

	/**
	 * Restores default size.
	 */
	void RestoreSize();

	/**
	 * Resize and reposition the scoreboard.
	 */
	void Resize();

	/**
	 * Sorts rows: Bounty Killer's Hunted first, then in DESC order by frags (coop: level), then XP, then fewer deaths,
	 * then by name.
	 */
	static bool StaticPlayerSortFuncByFrags(vgui2::SectionedListPanel *list, int itemID1, int itemID2);
};

#endif // SBO_VGUI2_SCORE_PANEL_H
