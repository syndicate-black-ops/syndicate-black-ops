//========= SBO VGUI2 port =========//
//
// Purpose: VGUI2 voice labels (see hud_voice_status.h). Ported from BugfixedHL-Rebased
//          src/game/client/hud/voice_status.cpp (CHudVoiceStatus, Valve CS:S code with Hidden Path's avatars).
//
// Changes vs. BHL:
//   * child of SBO's CClientViewport, created by VGui2_VoiceStatus_Init instead of BHL's HUD element list; layout
//     from ui/resource/voicestatus.res (BHL: HudLayout.res, same keys and values), scheme ClientScheme
//   * speaking state from SBO's CVoiceStatus through the bridge (sbo::IsPlayerSpeaking); SBO's CVoiceStatus keeps
//     the local player in m_bTalking only, the bridge counts him as speaking then (what BHL's
//     UpdateSpeakerStatus(-1) stores in m_VoicePlayers)
//   * RunFrame from OnThink (real time) instead of HUD_Frame -- the panel is always visible
//   * avatars: the scoreboard's code path (engine SteamID from sbo::GetScorePlayer on SteamPipe engines,
//     CScorePanel::GetValidSteamID64, CAvatarImage with ui/gfx/default_avatar.tga as fallback)
//   * label background: SBO's team colour table (sbo::GetClientColor, as chat / scoreboard / kill feed; BHL: viewport
//     team colour, "Orange" for team 0 -- SBO players always have a colour, "Orange" stays for empty slots)
//   * names: ^0..^9 / ^xRGB / ^#RRGGBB colour codes follow the chat's hud_colortext like the scoreboard and kill feed (BHL draws them
//     as typed): 1 = drawn in the code's colour (^0/^9 back to white), 2 = removed, 0 = shown as typed
//   * hidden (list kept) while the scoreboard is shown, SBO's IVoiceStatusHelper::CanShowSpeakerLabels rule that the
//     VGUI1 labels followed
//   * no speaker icon in the rows (BHL show_voice_icon / ui/gfx/hud/speaker.tga) and no CHudVoiceStatusSelf
//     (BHL voice_status_self, speaker_self.tga above the list): the local player's own row is his "talking"
//     feedback. Row: [avatar] name, the avatar at the left edge
//   * BHL's unused dead-icon variables are left out; no keyboard / mouse input
//=================================================================//

#include <string.h>
#include <wchar.h>

#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>

#include "avatar_image.h"
#include "tga_image.h"
#include "client_steam_context.h"
#include "client_vgui.h"
#include "client_viewport.h"
#include "hud_chat.h"
#include "hud_voice_status.h"
#include "score_panel.h"
#include "sbo_bridge.h"
#include "vgui2_api.h"

#define VOICESTATUS_RES VGUI2_ROOT_DIR "resource/voicestatus.res"

namespace
{
// GoldSrc's VOICE_MAX_PLAYERS (voice_common.h)
constexpr int VOICE_MAX_PLAYERS = 32;

// hud_colortext (the chat's cvar, BHL CHud::GetColorCodeAction); no chat: codes removed
ColorCodeAction GetColorCodeAction()
{
	CHudChat *pChat = CHudChat::Get();
	return pChat ? pChat->GetColorCodeAction() : ColorCodeAction::Strip;
}

// Length of the colour code at str[i] (2: ^0..^9, 5: ^xRGB, 8: ^#RRGGBB), 0 if there is none or codes aren't parsed
int GetSkippedColorCode(const wchar_t *str, int i, int len, bool bColorCodes)
{
	if (!bColorCodes)
		return 0;
	const int codeLen = GetColorCodeLength(str + i);
	return i + codeLen <= len ? codeLen : 0;
}
} // namespace

void VGui2_ApplyVoiceStatusLayout(vgui2::Panel *pPanel)
{
	if (!g_pFullFileSystem || !g_pFullFileSystem->FileExists(VOICESTATUS_RES))
	{
		char msg[128];
		Q_snprintf(msg, sizeof(msg), "%s: " VOICESTATUS_RES " not found, using defaults\n", pPanel->GetName());
		sbo::DevPrint(msg);
		return;
	}

	// BHL: the viewport's HudLayout.res is applied to its children by name
	KeyValues *pFile = new KeyValues("voicestatus.res");
	if (pFile->LoadFromFile(g_pFullFileSystem, VOICESTATUS_RES))
	{
		KeyValues *pSettings = pFile->FindKey(pPanel->GetName());
		if (pSettings)
			pPanel->ApplySettings(pSettings);
	}
	pFile->deleteThis();
}

CHudVoiceStatus *CHudVoiceStatus::m_sInstance = nullptr;

CHudVoiceStatus::CHudVoiceStatus(vgui2::Panel *pParent)
    : vgui2::Panel(pParent, "HudVoiceStatus")
{
	Assert(!m_sInstance);
	m_sInstance = this;

	SetProportional(true);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);

	// BHL: child of its viewport, ClientScheme. SBO's viewport uses ChatScheme -> load ClientScheme here.
	SetScheme(vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/clientscheme.res", "ClientScheme"));

	// BHL's HudLayoutBase.res defaults (xpos r145, ypos 0, 145x400 in 640x480 units)
	int screenWide, screenTall;
	vgui2::surface()->GetScreenSize(screenWide, screenTall);
	SetBounds(screenWide - vgui2::scheme()->GetProportionalScaledValue(145), 0,
	          vgui2::scheme()->GetProportionalScaledValue(145), vgui2::scheme()->GetProportionalScaledValue(400));

	VGui2_ApplyVoiceStatusLayout(this);

	SetVisible(true);
}

CHudVoiceStatus::~CHudVoiceStatus()
{
	ClearActiveList();

	if (m_sInstance == this)
		m_sInstance = nullptr;
}

void CHudVoiceStatus::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_NameFont = pScheme->GetFont("Default", IsProportional());
	m_NoTeamColor = pScheme->GetColor("Orange", Color(255, 255, 255, 255));

	SetBgColor(Color(0, 0, 0, 0));
}

void CHudVoiceStatus::OnThink()
{
	BaseClass::OnThink();

	double now = vgui2::system()->GetCurrentTime();
	float frameTime = 0.0f;
	if (m_flLastThinkTime > 0 && now > m_flLastThinkTime)
		frameTime = (float)(now - m_flLastThinkTime);
	m_flLastThinkTime = now;

	RunFrame(frameTime);
}

void CHudVoiceStatus::RunFrame(float fTime)
{
	for (int iPlayerIndex = 1; iPlayerIndex <= VOICE_MAX_PLAYERS; iPlayerIndex++)
	{
		int activeSpeakerIndex = FindActiveSpeaker(iPlayerIndex);
		bool bSpeaking = sbo::IsPlayerSpeaking(iPlayerIndex);

		if (activeSpeakerIndex != m_SpeakingList.InvalidIndex())
		{
			// update their speaking status
			m_SpeakingList[activeSpeakerIndex].bSpeaking = bSpeaking;
		}
		else
		{
			//=============================================================================
			// HPE_BEGIN:
			// [Forrest] Don't use UTIL_PlayerByIndex here.  It may be null for some players when
			// a match starts because the server only passes full player info as it affects
			// the client.
			//=============================================================================
			// if they are talking and not in the list, add them to the end
			if (bSpeaking)
			{
				ActiveSpeaker activeSpeaker;
				activeSpeaker.playerId = iPlayerIndex;
				activeSpeaker.bSpeaking = true;
				activeSpeaker.fAlpha = 0.0f;
				activeSpeaker.pAvatar = NULL;
				activeSpeaker.pFile = NULL;
				activeSpeaker.fAlphaMultiplier = 1.0f;

				//=============================================================================
				// HPE_BEGIN:
				// [pfreese] If a player is now talking set up their avatar
				//=============================================================================

				activeSpeaker.pAvatar = new CAvatarImage();
				activeSpeaker.pAvatar->SetDrawFriend(show_friend);

				// SBO: the scoreboard's SteamID path (no SteamID -> CAvatarImage draws ui/gfx/default_avatar.tga)
				sbo::ScorePlayer sp;
				sbo::GetScorePlayer(iPlayerIndex, sp, CClientSteamContext::IsSteamPipeEngine());
				if (sp.connected)
				{
					// a slot without a SteamID (the radio's): its sbo_avatar picture, as on the scoreboard
					activeSpeaker.pFile = CScorePanel::GetFileAvatar(sp, iPlayerIndex);
					uint64 steamID64 = CScorePanel::GetValidSteamID64(sp);
					if (ClientSteamContext().SteamUtils() && steamID64 != 0)
					{
						CSteamID steamIDForPlayer(steamID64);
						activeSpeaker.pAvatar->SetAvatarSteamID(steamIDForPlayer, k_EAvatarSize32x32);
					}
				}

				activeSpeaker.pAvatar->SetAvatarSize(avatar_wide, avatar_tall);

				//=============================================================================
				// HPE_END
				//=============================================================================

				m_SpeakingList.AddToTail(activeSpeaker);
			}
			//=============================================================================
			// HPE_END
			//=============================================================================
		}
	}

	for (int i = m_SpeakingList.Head(); i != m_SpeakingList.InvalidIndex();)
	{
		ActiveSpeaker &activeSpeaker = m_SpeakingList[i];

		if (activeSpeaker.bSpeaking)
		{
			if (fade_in_time > 0.0f)
			{
				activeSpeaker.fAlpha += fTime / fade_in_time;
				if (activeSpeaker.fAlpha > 1.0f)
					activeSpeaker.fAlpha = 1.0f;
			}
			else
			{
				activeSpeaker.fAlpha = 1.0f;
			}
		}
		else
		{
			if (fade_out_time > 0.0f)
			{
				activeSpeaker.fAlpha -= fTime / fade_out_time;
			}
			else
			{
				activeSpeaker.fAlpha = 0.0f;
			}

			if (activeSpeaker.fAlpha <= 0.0f)
			{
				// completely faded, remove them them from the list
				delete activeSpeaker.pAvatar;
				int iNext = m_SpeakingList.Next(i);
				m_SpeakingList.Remove(i);
				i = iNext;
				continue;
			}
		}
		i = m_SpeakingList.Next(i);
	}
}

void CHudVoiceStatus::Paint()
{
	// SBO: no speaker labels while the scoreboard is up (IVoiceStatusHelper)
	if (!sbo::CanShowSpeakerLabels())
		return;

	int x, y, w, h;
	GetBounds(x, y, w, h);

	// Heights to draw the current voice item at
	int ypos = h - item_tall;

	int length = m_SpeakingList.Count();

	int iFontHeight = 0;

	if (length > 0)
	{
		vgui2::surface()->DrawSetTextFont(m_NameFont);
		vgui2::surface()->DrawSetTextColor(Color(255, 255, 255, 255));
		iFontHeight = vgui2::surface()->GetFontTall(m_NameFont);
	}

	// SBO: player colour codes (hud_colortext), as the scoreboard / kill feed
	ColorCodeAction colorCodeAction = GetColorCodeAction();
	bool bColorCodes = colorCodeAction == ColorCodeAction::Handle;
	CHudChat *pChat = CHudChat::Get();

	//draw everyone in the list!
	FOR_EACH_LL(m_SpeakingList, i)
	{
		int playerId = m_SpeakingList[i].playerId;
		sbo::ScorePlayer sp;
		sbo::GetScorePlayer(playerId, sp, false);

		float newAlphaMultiplier = m_SpeakingList[i].fAlphaMultiplier * m_SpeakingList[i].fAlpha;

		Color COLOR_WHITE = Color(255, 255, 255, 255 * newAlphaMultiplier);
		Color bgColor;

		if (sp.connected)
		{
			// SBO: one team colour table (chat, scoreboard, kill feed)
			int r, g, b;
			sbo::GetClientColor(playerId, r, g, b);
			bgColor = Color(r, g, b, 255);
		}
		else
		{
			bgColor = m_NoTeamColor;
		}

		bgColor[3] = 128 * newAlphaMultiplier;

		const char *pName = sp.connected ? sp.name : "unknown";
		if (colorCodeAction == ColorCodeAction::Strip)
			pName = RemoveColorCodes(pName);
		else if (sp.connected)
			pName = CHudChat::GetColoredPlayerName(playerId, pName); // SBO: name colour (cvar namecolor), hud_colortext 1

		wchar_t szconverted[128]; // SBO: 64 in BHL; a name with its name colour's codes can be longer
		Q_UTF8ToWString(pName, szconverted, sizeof(szconverted), STRINGCONVERT_REPLACE);

		// Draw the item background
		vgui2::surface()->DrawSetColor(bgColor);
		vgui2::surface()->DrawFilledRect(0, ypos, item_wide, ypos + item_tall);

		//=============================================================================
		// HPE_BEGIN:
		// [pfreese] Draw the avatar for the given player
		//=============================================================================

		// Draw the players icon
		if (show_avatar && m_SpeakingList[i].pFile)
		{
			m_SpeakingList[i].pFile->SetPos(avatar_xpos, ypos + avatar_ypos);
			m_SpeakingList[i].pFile->SetSize(avatar_wide, avatar_tall);
			m_SpeakingList[i].pFile->SetColor(COLOR_WHITE);
			m_SpeakingList[i].pFile->Paint();
		}
		else if (show_avatar && m_SpeakingList[i].pAvatar)
		{
			m_SpeakingList[i].pAvatar->SetPos(avatar_xpos, ypos + avatar_ypos);
			m_SpeakingList[i].pAvatar->SetColor(COLOR_WHITE);
			m_SpeakingList[i].pAvatar->Paint();
		}

		//=============================================================================
		// HPE_END
		//=============================================================================

		// SBO: no voice icon (BHL show_voice_icon), the row is [avatar] name

		// Draw the player's name
		int textX = text_xpos;
		int textY = ypos + (item_tall / 2) - (iFontHeight / 2);

		int iTextSpace = item_wide - text_xpos;

		// write as much of the name as will fit, truncate the rest and add ellipses
		int iNameLength = wcslen(szconverted);
		wchar_t *pszconverted = szconverted;
		int iTextWidthCounter = 0;
		int iDrawn = 0;                 // SBO: characters that take space (colour codes don't)
		int iPrev1 = -1, iPrev2 = -1;   // SBO: where the last two of them start
		for (int j = 0; j < iNameLength; j++)
		{
			const int codeLen = GetSkippedColorCode(pszconverted, j, iNameLength, bColorCodes);
			if (codeLen > 0)
			{
				j += codeLen - 1; // SBO: colour codes take no space
				continue;
			}

			iTextWidthCounter += vgui2::surface()->GetCharacterWidth(m_NameFont, pszconverted[j]);

			if (iTextWidthCounter > iTextSpace)
			{
				// SBO: BHL writes ".." over [j - 2], [j - 1], which can be half of a colour code; the two
				// characters before this one go instead (and any code between them: nothing is drawn after)
				if (iDrawn > 3 && iPrev2 >= 0)
				{
					szconverted[iPrev2] = '.';
					szconverted[iPrev2 + 1] = '.';
					szconverted[iPrev2 + 2] = '\0';
				}
				break;
			}

			iPrev2 = iPrev1;
			iPrev1 = j;
			iDrawn++;
		}

		// SBO: BHL DrawPrintText, char by char so ^1..^8, ^xRGB and ^#RRGGBB switch to the chat's colours (^0/^9: white)
		iNameLength = wcslen(szconverted);
		vgui2::surface()->DrawSetTextColor(COLOR_WHITE);
		for (int j = 0; j < iNameLength; j++)
		{
			const int codeLen = GetSkippedColorCode(szconverted, j, iNameLength, bColorCodes);
			if (codeLen > 0)
			{
				Color c = COLOR_WHITE;
				if (pChat && pChat->GetColorCodeColor(szconverted + j, c))
					c[3] = COLOR_WHITE[3];
				vgui2::surface()->DrawSetTextColor(c);
				j += codeLen - 1;
				continue;
			}

			vgui2::surface()->DrawSetTextPos(textX, textY);
			vgui2::surface()->DrawUnicodeChar(szconverted[j]);
			textX += vgui2::surface()->GetCharacterWidth(m_NameFont, szconverted[j]);
		}

		ypos -= (item_spacing + item_tall);
	}
}

int CHudVoiceStatus::FindActiveSpeaker(int playerId)
{
	FOR_EACH_LL(m_SpeakingList, i)
	{
		if (m_SpeakingList[i].playerId == playerId)
			return i;
	}
	return m_SpeakingList.InvalidIndex();
}

void CHudVoiceStatus::ClearActiveList()
{
	FOR_EACH_LL(m_SpeakingList, i)
	{
		delete m_SpeakingList[i].pAvatar;
	}

	m_SpeakingList.RemoveAll();
}

//-----------------------------------------------------------------------------
// vgui2_api.h
//-----------------------------------------------------------------------------
bool VGui2_VoiceStatus_Init()
{
	if (CHudVoiceStatus::Get())
		return true;

	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;

	// BHL: HUD_Init -> ClientSteamContext().Activate() (avatars); does nothing if the scoreboard already did it
	ClientSteamContext().Activate();

	CHudVoiceStatus *pStatus = new CHudVoiceStatus(g_pViewport);
	pStatus->MakeReadyForUse();
	pStatus->ClearActiveList();
	return true;
}

bool VGui2_VoiceStatus_IsActive()
{
	return CHudVoiceStatus::Get() != nullptr;
}
