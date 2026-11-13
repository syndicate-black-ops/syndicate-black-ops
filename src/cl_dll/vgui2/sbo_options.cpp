//========= SBO VGUI2: "Advanced" options dialog =========//
//
// Purpose: see sbo_options.h.
//=================================================================//

#include <algorithm>
#include <stdio.h>
#include <string.h>

#include <IBaseUI.h>
#include <KeyValues.h>
#include <tier2/tier2.h>
#include <vgui/ISurface.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/Tooltip.h>
#include <vgui_controls/Button.h>

#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_options.h"
#include "sbo_options_window.h"
#include "sbo_ui_style.h"
#include "sbo_widgets.h"
#include "../updater/update_checker.h"
#include "../version/sbo_version.h"

static vgui2::DHANDLE<CSboOptionsDialog> s_hDialog;
static bool s_bOpenNextFrame = false;
static int s_iOpenPage = -1; //!< the page to show when the dialog opens (VGui2_OldAdvanced_OpenNextFrame), -1 none
static int s_iOpenDelay = 0; //!< frames to wait before opening (VGui2_OldAdvanced_OpenNextFrame)

//-----------------------------------------------------------------------------
// CSboCvarCheckButton
//-----------------------------------------------------------------------------
CSboCvarCheckButton::CSboCvarCheckButton(vgui2::Panel *parent, const char *panelName, const char *text, const char *cvarName,
    int onValue, int offValue)
    : BaseClass(parent, panelName, text)
    , m_pszCvar(cvarName)
    , m_iOnValue(onValue)
    , m_iOffValue(offValue)
{
	ResetData();
}

void CSboCvarCheckButton::ResetData()
{
	sbo::CvarHandle cvar = sbo::FindCvar(m_pszCvar);
	if (!cvar)
		return;
	const float value = sbo::CvarGetFloat(cvar);
	m_bResetState = m_iOffValue == 0 ? value != 0.0f : (int)value == m_iOnValue;
	SetSelected(m_bResetState);
}

void CSboCvarCheckButton::ApplyChanges()
{
	if (!HasBeenModified() || !sbo::FindCvar(m_pszCvar))
		return;
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "%s %d\n", m_pszCvar, IsSelected() ? m_iOnValue : m_iOffValue);
	sbo::ExecClientCmd(cmd);
	m_bResetState = IsSelected();
}

//-----------------------------------------------------------------------------
// CSboCvarComboBox
//-----------------------------------------------------------------------------
CSboCvarComboBox::CSboCvarComboBox(vgui2::Panel *parent, const char *panelName, const char *cvarName, const int *values,
    const char *const *labels, int count, int defaultValue)
    : BaseClass(parent, panelName, count < 12 ? count : 12, false)
    , m_pszCvar(cvarName)
    , m_iDefault(defaultValue)
{
	m_nCount = count < MAX_ITEMS ? count : MAX_ITEMS;
	for (int i = 0; i < m_nCount; i++)
	{
		m_iValues[i] = values[i];
		m_iItemIds[i] = AddItem(labels[i], nullptr);
	}
	ResetData();
}

int CSboCvarComboBox::SelectedRow() const
{
	const int item = const_cast<CSboCvarComboBox *>(this)->GetActiveItem();
	for (int i = 0; i < m_nCount; i++)
	{
		if (m_iItemIds[i] == item)
			return i;
	}
	return -1;
}

void CSboCvarComboBox::ResetData()
{
	if (m_nCount <= 0)
		return;
	float value = (float)m_iDefault;
	if (sbo::CvarHandle cvar = sbo::FindCvar(m_pszCvar))
		value = sbo::CvarGetFloat(cvar);
	int row = 0;
	for (int i = 1; i < m_nCount; i++)
	{
		const float d = value - (float)m_iValues[i], best = value - (float)m_iValues[row];
		if (d * d < best * best)
			row = i;
	}
	SilentActivateItemByRow(row);
	m_iResetRow = row;
}

void CSboCvarComboBox::ApplyChanges()
{
	const int row = SelectedRow();
	if (row < 0 || row == m_iResetRow || !sbo::FindCvar(m_pszCvar))
		return;
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "%s %d\n", m_pszCvar, m_iValues[row]);
	sbo::ExecClientCmd(cmd);
	m_iResetRow = row;
}

//-----------------------------------------------------------------------------
// CSboOptionsPage
//-----------------------------------------------------------------------------
// Valve GameUI options pages (OnControlModified): a control changed -> "ApplyButtonEnable" to the PropertySheet,
// which passes it to the dialog (PropertySheet::OnApplyButtonEnable)
void CSboOptionsPage::OnControlModified()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

//-----------------------------------------------------------------------------
// General tab
//-----------------------------------------------------------------------------
CSboGeneralOptions::CSboGeneralOptions(vgui2::Panel *parent)
    : BaseClass(parent, "SboGeneralOptions")
{
	m_pDiscordTitle = new vgui2::Label(this, "DiscordTitle", "Discord");
	m_pDiscord = new CSboCvarCheckButton(this, "DiscordCheck", "Discord Rich Presence", "cl_discord_rpc");
	m_pDiscordHelp = new vgui2::Label(this, "DiscordHelp", "Shows your server and map in your Discord profile");
	m_pDiscordHelp->SetWrap(true);
	// from the top: a wrapped Label measures its text before it knows its width (one line per character) and
	// centers that height, which put the text outside the label
	m_pDiscordHelp->SetContentAlignment(vgui2::Label::a_northwest);
	WatchControl(m_pDiscord);

	// cl_righthand (weapon_hand.cpp, docs/WEAPON_HAND.md): 1 = the first-person weapon in the left hand
	m_pWeaponsTitle = new vgui2::Label(this, "WeaponsTitle", "Weapons");
	m_pLeftHanded = new CSboCvarCheckButton(this, "LeftHandedCheck", "Left-handed weapons", "cl_righthand");
	WatchControl(m_pLeftHanded);
	// hud_objectives (hud_objectives.cpp, docs/OBJECTIVES.md): 0 hidden, 1 compact, 2 full; checked for 1 and 2, a
	// change writes 1 or 0 (ApplyChanges only writes a box that changed, so 2 stays 2)
	m_pHudTitle = new vgui2::Label(this, "HudTitle", "HUD");
	m_pObjectives = new CSboCvarCheckButton(this, "ObjectivesCheck", "Enable on-screen objectives", "hud_objectives");
	WatchControl(m_pObjectives);
	m_pMenusTitle = new vgui2::Label(this, "MenusTitle", "Menus");
	m_pMenus = new CSboCvarCheckButton(this, "MenusCheck", "New GameUI menus", "cl_experimental_menus");
	m_pMenusHelp = new vgui2::Label(this, "MenusHelp", "Use the new server browser, joining screen and settings windows");
	m_pMenusHelp->SetWrap(true);
	m_pMenusHelp->SetContentAlignment(vgui2::Label::a_northwest); // as m_pDiscordHelp
	WatchControl(m_pMenus);
	m_pIngame = new CSboCvarCheckButton(this, "IngameCheck", "New in-game menus", "cl_new_ingame_menus");
	m_pIngameHelp = new vgui2::Label(this, "IngameHelp", "New loadout, MOTD and map info windows");
	m_pIngameHelp->SetWrap(true);
	m_pIngameHelp->SetContentAlignment(vgui2::Label::a_northwest);
	WatchControl(m_pIngame);
}

namespace
{
//! Height of a wrapped label's text at the given width: its lines (word wrap, as TextImage) times the font height
int WrappedTextTall(vgui2::Label *pLabel, int wide)
{
	const vgui2::HFont font = pLabel->GetFont();
	const int lineTall = vgui2::surface()->GetFontTall(font);
	wchar_t text[512];
	pLabel->GetText(text, sizeof(text));
	int lines = 0, lineWide = 0;
	const wchar_t *p = text;
	while (*p)
	{
		const wchar_t *end = p;
		while (*end && *end != L' ')
			end++;
		int wordWide = 0;
		for (const wchar_t *c = p; c < end; c++)
			wordWide += vgui2::surface()->GetCharacterWidth(font, *c);
		const int spaceWide = vgui2::surface()->GetCharacterWidth(font, L' ');
		if (lines == 0 || (lineWide > 0 && lineWide + spaceWide + wordWide > wide))
		{
			lines++;
			lineWide = wordWide;
		}
		else
			lineWide += (lineWide > 0 ? spaceWide : 0) + wordWide;
		p = *end ? end + 1 : end;
	}
	return lines * lineTall;
}
} // namespace

// Every row from the fonts' heights (GameUI's scheme fonts grow with the resolution): heading, check box, help text
// wrapped at the page width, the same gap after each section; CSboOptionsDialog makes the page tall enough for them.
void CSboGeneralOptions::PerformLayout()
{
	BaseClass::PerformLayout();
	const int wide = GetWide();
	const int titleTall = std::max(20, vgui2::surface()->GetFontTall(m_pDiscordTitle->GetFont()) + 4);
	const int checkTall = std::max(24, vgui2::surface()->GetFontTall(m_pDiscord->GetFont()) + 8);
	const int sectionGap = 12;
	int y = 12;
	auto section = [&](vgui2::Label *pTitle, vgui2::CheckButton *pCheck, vgui2::Label *pHelp) {
		if (pTitle)
		{
			pTitle->SetBounds(16, y, wide - 32, titleTall);
			y += titleTall + 4;
		}
		else
			y -= sectionGap - 4; // a second setting of the section above
		pCheck->SetBounds(12, y, wide - 24, checkTall);
		y += checkTall;
		if (pHelp)
		{
			const int helpTall = WrappedTextTall(pHelp, wide - 52 - 8) + 4; // the label's text inset
			pHelp->SetBounds(36, y, wide - 52, helpTall);
			y += helpTall;
		}
		y += sectionGap;
	};
	section(m_pDiscordTitle, m_pDiscord, m_pDiscordHelp);
	section(m_pWeaponsTitle, m_pLeftHanded, nullptr);
	section(m_pHudTitle, m_pObjectives, nullptr);
	section(m_pMenusTitle, m_pMenus, m_pMenusHelp);
	section(nullptr, m_pIngame, m_pIngameHelp);
	m_iContentTall = y;
}

void CSboGeneralOptions::OnResetData()
{
	m_pDiscord->ResetData();
	m_pLeftHanded->ResetData();
	m_pObjectives->ResetData();
	m_pMenus->ResetData();
	m_pIngame->ResetData();
}

void CSboGeneralOptions::OnApplyChanges()
{
	m_pDiscord->ApplyChanges();
	m_pLeftHanded->ApplyChanges();
	m_pObjectives->ApplyChanges();
	m_pMenus->ApplyChanges();
	m_pIngame->ApplyChanges();
}

bool CSboGeneralOptions::HasChanges()
{
	return m_pDiscord->HasBeenModified() || m_pLeftHanded->HasBeenModified() || m_pObjectives->HasBeenModified() ||
	       m_pMenus->HasBeenModified() ||
	       m_pIngame->HasBeenModified();
}

//-----------------------------------------------------------------------------
// Voice tab
//-----------------------------------------------------------------------------
namespace
{
float CvarOr(const char *name, float fallback)
{
	sbo::CvarHandle h = sbo::FindCvar(name);
	return h ? sbo::CvarGetFloat(h) : fallback;
}
} // namespace

CSboVoiceOptions::CSboVoiceOptions(vgui2::Panel *parent)
    : BaseClass(parent, "SboVoiceOptions")
{
	m_pTitle = new vgui2::Label(this, "VoiceTitle", "Voice gain");
	m_pMaxLabel = new vgui2::Label(this, "MaxGainLabel", "Max voice boost");
	m_pMax = new vgui2::Slider(this, "MaxGain");
	m_pMax->SetRange(1, 10);
	m_pMax->AddActionSignalTarget(this);
	m_pMaxValue = new vgui2::Label(this, "MaxGainValue", "");
	m_pMaxLabel->GetTooltip()->SetText("How much quiet microphones are boosted");
	m_pAvgLabel = new vgui2::Label(this, "AvgGainLabel", "Voice leveling");
	m_pAvg = new vgui2::Slider(this, "AvgGain");
	m_pAvg->SetRange(0, 100);
	m_pAvg->AddActionSignalTarget(this);
	m_pAvgValue = new vgui2::Label(this, "AvgGainValue", "");
	m_pAvgLabel->GetTooltip()->SetText("How strongly loud and quiet voices are evened out");
	m_pDefault = new vgui2::Button(this, "VoiceDefault", "Default", this, "VoiceGainDefault");
}

void CSboVoiceOptions::PerformLayout()
{
	BaseClass::PerformLayout();
	const int wide = GetWide();
	const int rowTall = std::max(24, vgui2::surface()->GetFontTall(m_pMaxLabel->GetFont()) + 8);
	int y = 12;
	m_pTitle->SetBounds(16, y, wide - 32, rowTall);
	y += rowTall + 4;
	const int labelW = 150, valueW = 50;
	auto row = [&](vgui2::Label *pLabel, vgui2::Slider *pSlider, vgui2::Label *pValue) {
		pLabel->SetBounds(16, y, labelW, rowTall);
		pSlider->SetBounds(16 + labelW, y, wide - 32 - labelW - valueW - 8, rowTall);
		pValue->SetBounds(wide - 16 - valueW, y, valueW, rowTall);
		y += rowTall + 8;
	};
	row(m_pMaxLabel, m_pMax, m_pMaxValue);
	row(m_pAvgLabel, m_pAvg, m_pAvgValue);
	m_pDefault->SetBounds(16 + labelW, y, 90, rowTall);
}

void CSboVoiceOptions::UpdateValues()
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%dx", m_pMax->GetValue());
	m_pMaxValue->SetText(buf);
	snprintf(buf, sizeof(buf), "%d%%", m_pAvg->GetValue());
	m_pAvgValue->SetText(buf);
}

void CSboVoiceOptions::OnSliderMoved()
{
	UpdateValues();
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void CSboVoiceOptions::OnResetData()
{
	m_iMaxReset = std::max(1, std::min(10, (int)(CvarOr("voice_maxgain", SBO_VOICE_MAXGAIN_DEFAULT) + 0.5f)));
	m_iAvgReset = std::max(0, std::min(100, (int)(CvarOr("voice_avggain", SBO_VOICE_AVGGAIN_DEFAULT) * 100.0f + 0.5f)));
	m_pMax->SetValue(m_iMaxReset, false);
	m_pAvg->SetValue(m_iAvgReset, false);
	UpdateValues();
}

void CSboVoiceOptions::OnApplyChanges()
{
	if (sbo::FindCvar("voice_maxgain"))
		sbo::CvarSetFloat("voice_maxgain", (float)m_pMax->GetValue());
	if (sbo::FindCvar("voice_avggain"))
		sbo::CvarSetFloat("voice_avggain", m_pAvg->GetValue() / 100.0f);
	m_iMaxReset = m_pMax->GetValue();
	m_iAvgReset = m_pAvg->GetValue();
	SboVoiceGain_Save();
}

bool CSboVoiceOptions::HasChanges()
{
	return m_pMax->GetValue() != m_iMaxReset || m_pAvg->GetValue() != m_iAvgReset;
}

void CSboVoiceOptions::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "VoiceGainDefault"))
	{
		m_pMax->SetValue((int)SBO_VOICE_MAXGAIN_DEFAULT);
		m_pAvg->SetValue((int)(SBO_VOICE_AVGGAIN_DEFAULT * 100.0f));
		UpdateValues();
		PostActionSignal(new KeyValues("ApplyButtonEnable"));
		return;
	}
	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Chat tab
//-----------------------------------------------------------------------------
// hud_chat_fontsize: pixels, 14..34, 17 = the chat's size since the VGUI2 chat (hud_chat.h CHudChat::CHAT_FONT_MIN /
// CHAT_FONT_MAX / CHAT_FONT_DEFAULT)
static constexpr int CHAT_FONT_MIN = 14, CHAT_FONT_MAX = 34, CHAT_FONT_DEFAULT = 17;
static constexpr int CHAT_FONT_COUNT = CHAT_FONT_MAX - CHAT_FONT_MIN + 1;
// hud_saytext_time: seconds; 12 is the default (hud_chat.cpp)
static constexpr int CHAT_TIMES[] = { 5, 8, 10, 12, 15, 20, 30, 60 };
static constexpr int CHAT_TIME_COUNT = (int)(sizeof(CHAT_TIMES) / sizeof(CHAT_TIMES[0]));
static constexpr int CHAT_TIME_DEFAULT = 12;

CSboChatOptions::CSboChatOptions(vgui2::Panel *parent)
    : BaseClass(parent, "SboChatOptions")
{
	m_pTitle = new vgui2::Label(this, "ChatTitle", "Chat");

	static char s_szSizeText[CHAT_FONT_COUNT][16];
	const char *sizeLabels[CHAT_FONT_COUNT];
	int sizeValues[CHAT_FONT_COUNT];
	for (int i = 0; i < CHAT_FONT_COUNT; i++)
	{
		sizeValues[i] = CHAT_FONT_MIN + i;
		snprintf(s_szSizeText[i], sizeof(s_szSizeText[i]), sizeValues[i] == CHAT_FONT_DEFAULT ? "%d (default)" : "%d",
		    sizeValues[i]);
		sizeLabels[i] = s_szSizeText[i];
	}
	m_pFontSizeLabel = new vgui2::Label(this, "ChatFontLabel", "Text size");
	m_pFontSize = new CSboCvarComboBox(this, "ChatFontSize", "hud_chat_fontsize", sizeValues, sizeLabels,
	    CHAT_FONT_COUNT, CHAT_FONT_DEFAULT);

	// hud_colortext: 1 draws color codes and name colors, 2 removes the codes (0 would show them as typed). It covers
	// the chat, the scoreboard, the kill feed and the voice list.
	m_pColors = new CSboCvarCheckButton(this, "ChatColors", "Name and chat colors", "hud_colortext", 1, 2);
	// hud_saytext_sound: the new-message sound's volume (0 = none)
	m_pSound = new CSboCvarCheckButton(this, "ChatSound", "Message sound", "hud_saytext_sound");

	static char s_szTimeText[CHAT_TIME_COUNT][32];
	const char *timeLabels[CHAT_TIME_COUNT];
	for (int i = 0; i < CHAT_TIME_COUNT; i++)
	{
		snprintf(s_szTimeText[i], sizeof(s_szTimeText[i]),
		    CHAT_TIMES[i] == CHAT_TIME_DEFAULT ? "%d seconds (default)" : "%d seconds", CHAT_TIMES[i]);
		timeLabels[i] = s_szTimeText[i];
	}
	m_pTimeLabel = new vgui2::Label(this, "ChatTimeLabel", "Message time");
	m_pTime = new CSboCvarComboBox(this, "ChatTime", "hud_saytext_time", CHAT_TIMES, timeLabels, CHAT_TIME_COUNT,
	    CHAT_TIME_DEFAULT);

	m_pChatColorLabel = new vgui2::Label(this, "ChatColorLabel", "Chat color");
	m_pChatColor = new CSboColorField(this, "ChatColor");

	WatchControl(m_pFontSize);
	WatchControl(m_pChatColor);
	WatchControl(m_pColors);
	WatchControl(m_pSound);
	WatchControl(m_pTime);
}

void CSboChatOptions::PerformLayout()
{
	BaseClass::PerformLayout();
	int wide = GetWide();
	m_pTitle->SetBounds(16, 12, wide - 32, 20);
	m_pFontSizeLabel->SetBounds(36, 38, 200, 24);
	m_pFontSize->SetBounds(240, 38, 160, 24);
	m_pColors->SetBounds(12, 68, wide - 24, 24);
	m_pSound->SetBounds(12, 94, wide - 24, 24);
	m_pTimeLabel->SetBounds(36, 124, 200, 24);
	m_pTime->SetBounds(240, 124, 160, 24);
	m_pChatColorLabel->SetBounds(36, 154, 200, 24);
	m_pChatColor->SetBounds(240, 154, 160, 24);
	m_pChatColor->SetLook(m_pTimeLabel->GetFont(), m_pTimeLabel->GetFgColor(), Color(255, 255, 255, 64), Color(0, 0, 0, 80));
}

void CSboChatOptions::OnResetData()
{
	m_pFontSize->ResetData();
	m_pColors->ResetData();
	m_pSound->ResetData();
	m_pTime->ResetData();
	m_ChatColorReset = ParseChatColor(sbo::FindCvar("hud_chat_color") ? sbo::CvarGetString(sbo::FindCvar("hud_chat_color")) : "0");
	m_pChatColor->SetColor(m_ChatColorReset);
}

void CSboChatOptions::OnApplyChanges()
{
	m_pFontSize->ApplyChanges();
	m_pColors->ApplyChanges();
	m_pSound->ApplyChanges();
	m_pTime->ApplyChanges();
	if (m_pChatColor->GetColor() != m_ChatColorReset)
	{
		char v[16];
		CSboColorField::FormatValue(m_pChatColor->GetColor(), v, sizeof(v));
		sbo::CvarSetString("hud_chat_color", v);
		m_ChatColorReset = m_pChatColor->GetColor();
	}
}

bool CSboChatOptions::HasChanges()
{
	return m_pFontSize->HasBeenModified() || m_pColors->HasBeenModified() || m_pSound->HasBeenModified() ||
	       m_pTime->HasBeenModified() || m_pChatColor->GetColor() != m_ChatColorReset;
}

//-----------------------------------------------------------------------------
// Version tab
//-----------------------------------------------------------------------------
CSboVersionOptions::CSboVersionOptions(vgui2::Panel *parent)
    : BaseClass(parent, "SboVersionOptions")
{
	char buf[192];
	m_pTitle = new vgui2::Label(this, "Title", "Syndicate Black Ops");
	snprintf(buf, sizeof(buf), "Version: %s", SboVersion_Full());
	m_pVersion = new vgui2::Label(this, "Version", buf);
	m_pLatest = new vgui2::Label(this, "Latest", "");
	m_pCheck = new vgui2::Button(this, "CheckUpdates", "Check for updates", this, "CheckUpdates");
	m_pCredits = new vgui2::Label(this, "Credits", SBO_CREDITS_TEXT);
	m_pRepoLink = new CSboLinkLabel(this, "RepoLink", SBO_REPO_LINK_TEXT, SBO_REPO_URL);
	UpdateControls();
}

void CSboVersionOptions::PerformLayout()
{
	BaseClass::PerformLayout();
	int wide = GetWide();
	m_pTitle->SetBounds(16, 12, wide - 32, 20);
	m_pVersion->SetBounds(16, 40, wide - 32, 20);
	m_pLatest->SetBounds(16, 64, wide - 32, 20);
	m_pCheck->SetBounds(16, 92, 140, 24);
	// the link right after the text, or on the next line where the page is too narrow for both
	int textWide, textTall, linkWide, linkTall;
	m_pCredits->GetContentSize(textWide, textTall);
	m_pRepoLink->GetContentSize(linkWide, linkTall);
	m_pCredits->SetBounds(16, 132, textWide, 20);
	const bool sameLine = textWide + 4 + linkWide <= wide - 32;
	const int linkX = sameLine ? 16 + textWide + 4 : 16;
	m_pRepoLink->SetBounds(linkX, sameLine ? 132 : 152, std::min(linkWide, std::max(0, wide - 16 - linkX)), 20);
}

void CSboVersionOptions::OnThink()
{
	BaseClass::OnThink();
	if ((int)sbo_update::GetState() != m_iLastState)
		UpdateControls();
}

void CSboVersionOptions::OnCommand(const char *command)
{
	if (!stricmp(command, "CheckUpdates"))
	{
		sbo_update::CheckForUpdates();
		UpdateControls();
	}
	else
		BaseClass::OnCommand(command);
}

void CSboVersionOptions::UpdateControls()
{
	sbo_update::State state = sbo_update::GetState();
	m_iLastState = (int)state;
	m_pLatest->SetText(sbo_update::StatusText().c_str());
	m_pCheck->SetEnabled(state != sbo_update::State::Disabled && state != sbo_update::State::Checking);
}

//-----------------------------------------------------------------------------
// Dialog
//-----------------------------------------------------------------------------
CSboOptionsDialog::CSboOptionsDialog(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboOptionsDialog")
{
	SetBounds(0, 0, 512, DIALOG_TALL);
	SetSizeable(false);
	SetDeleteSelfOnClose(true);
	SetTitle("Advanced", true);

	m_pPages[0] = new CSboGeneralOptions(this);
	m_pPages[1] = new CSboChatOptions(this);
	m_pPages[2] = new CSboVoiceOptions(this);
	AddPage(m_pPages[0], "General");
	AddPage(m_pPages[1], "Chat");
	AddPage(m_pPages[2], "Voice");
	AddPage(new CSboVersionOptions(this), "Version");

	SetApplyButtonVisible(true);
	EnableApplyButton(false); // nothing changed yet (OnApplyButtonEnable)
	MoveToCenterOfScreen();
}

#ifndef _WIN32
// Linux (docs/dev/LINUX_CLIENT.md "Symbol font"): the title bar's close button, Marlett's "r", is not drawn there. The
// base class sets the Marlett font each time the scheme is applied, so the substitute goes in after it.
void CSboOptionsDialog::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	sboui::ApplySymbolSubstitutes(this);
}
#endif

// The dialog is DIALOG_TALL high; the General page's rows (three sections, fonts that grow with the resolution) can
// need more: the dialog is made as tall as those rows plus the frame around the page (title, tabs, buttons). The
// sheet is laid out here first, so the page has the height of this pass (it sizes its pages in its own layout, after
// the dialog's). Never smaller than DIALOG_TALL: the other pages fit in it.
void CSboOptionsDialog::PerformLayout()
{
	BaseClass::PerformLayout();
	CSboGeneralOptions *pGeneral = static_cast<CSboGeneralOptions *>(m_pPages[0]);
	if (!pGeneral || !GetPropertySheet())
		return;
	GetPropertySheet()->InvalidateLayout(true); // the active page's bounds for the sheet's bounds of this pass
	if (pGeneral->GetTall() <= 0)
		return;
	const int frameTall = GetTall() - pGeneral->GetTall();
	pGeneral->InvalidateLayout(true); // its rows at its current width
	const int want = std::max(DIALOG_TALL, pGeneral->GetContentTall() + frameTall);
	if (want != GetTall())
	{
		SetTall(want);
		MoveToCenterOfScreen();
	}
}

// PropertyDialog::OnApplyButtonEnable only enables; here a control set back to its saved value disables Apply again
void CSboOptionsDialog::OnApplyButtonEnable()
{
	bool bChanged = false;
	for (CSboOptionsPage *pPage : m_pPages)
		bChanged = bChanged || (pPage && pPage->HasChanges());
	EnableApplyButton(bChanged);
}

//-----------------------------------------------------------------------------
// gameui_cl_open_adv_options: the game menu runs it as "engine gameui_cl_open_adv_options", which hides GameUI. Show GameUI again and open
// the dialog on the next frame (BHL gameui_cl_open_adv_options), otherwise the console may cover it.
//-----------------------------------------------------------------------------
static void SboOptions_f()
{
	if (!g_pGameUIViewport)
		return;
	// cl_experimental_menus 1: SBO's new-style Advanced window (same tabs and settings, the new menus' look)
	if (SboExperimentalMenus())
	{
		VGui2_OpenAdvancedWindow();
		return;
	}
	g_pBaseUI->ActivateGameUI();
	s_bOpenNextFrame = true;
}

void VGui2_Options_Frame()
{
	if (!s_bOpenNextFrame || !g_pGameUIViewport)
		return;
	if (s_iOpenDelay > 0)
	{
		s_iOpenDelay--;
		return;
	}
	s_bOpenNextFrame = false;
	if (!s_hDialog.Get())
		s_hDialog = new CSboOptionsDialog(g_pGameUIViewport);
	s_hDialog->Activate();
	vgui2::PropertySheet *pSheet = s_hDialog->GetPropertySheet();
	if (s_iOpenPage >= 0 && pSheet && s_iOpenPage < pSheet->GetNumPages())
		pSheet->SetActivePage(pSheet->GetPage(s_iOpenPage));
	s_iOpenPage = -1;
}

int VGui2_OldAdvanced_CloseIfOpen()
{
	CSboOptionsDialog *p = s_hDialog.Get();
	if (!p || !p->IsVisible())
		return -1;
	int page = 0;
	if (vgui2::PropertySheet *pSheet = p->GetPropertySheet())
	{
		for (int i = 0; i < pSheet->GetNumPages(); i++)
		{
			if (pSheet->GetPage(i) == pSheet->GetActivePage())
				page = i;
		}
	}
	s_hDialog = nullptr; // it deletes itself on close; the next open makes a new one
	p->Close();
	return page;
}

void VGui2_OldAdvanced_OpenNextFrame(int page)
{
	s_iOpenPage = page;
	// after GameUI's dialogs that the same style change opens (their commands run on the next frame), so the window
	// the change was made in ends up in front
	s_iOpenDelay = 2;
	if (g_pBaseUI)
		g_pBaseUI->ActivateGameUI();
	s_bOpenNextFrame = true;
}

void VGui2_RegisterOptionsCommand()
{
	static bool s_bDone = false;
	if (s_bDone)
		return;
	s_bDone = true;
	sbo::AddCommand("gameui_cl_open_adv_options", SboOptions_f); // BHL's command name
}
