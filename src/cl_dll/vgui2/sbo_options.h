//========= SBO VGUI2: "Advanced" options dialog =========//
//
// Purpose: SBO's own settings dialog, opened from the main menu ("Advanced", gamedir/syndicate/resource/gamemenu.res ->
//          "engine gameui_cl_open_adv_options", BHL's command name). Modelled on BugfixedHL-Rebased's Advanced Options
//          (src/game/client/gameui/options/adv_options_dialog.cpp, cvar_check_button.cpp): a PropertyDialog parented
//          to the GameUI viewport, one PropertyPage per tab, cvar-bound controls applied with OK/Apply.
//
// Apply button (Valve's PropertyDialog / GameUI Options): disabled while every control holds the cvar's saved value.
// A control's change ("CheckButtonChecked", "TextChanged") reaches its page, the page posts "ApplyButtonEnable" to
// the PropertySheet, the sheet passes it to the dialog, and the dialog enables Apply when a page has changes (back to
// the saved values = disabled again). Apply / OK write the changes and disable it (PropertyDialog::OnCommand).
//
// Tabs: General (Discord Rich Presence, left-handed weapons, Experimental new menus), Chat (text size, name and chat colors, sound, message time), Version (build version + update
// check; BHL options_about.cpp).
// Add further pages in CSboOptionsDialog's constructor.
//
// SDK world (hl1_source_sdk headers); cvars go through sbo_bridge.h.
//=================================================================//

#ifndef SBO_VGUI2_SBO_OPTIONS_H
#define SBO_VGUI2_SBO_OPTIONS_H

#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include "hud_chat_color_picker.h"
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/PropertyDialog.h>
#include <vgui_controls/PropertyPage.h>

//-----------------------------------------------------------------------------
// Check box bound to an engine cvar (BHL CCvarCheckButton): ResetData() reads the cvar, ApplyChanges() writes it
// with a console command (so archived cvars are saved like any other setting), only after a change. Checked writes
// onValue, unchecked offValue. offValue 0: checked = any value but 0; otherwise checked = the value is onValue.
//-----------------------------------------------------------------------------
class CSboCvarCheckButton : public vgui2::CheckButton
{
	DECLARE_CLASS_SIMPLE(CSboCvarCheckButton, vgui2::CheckButton);

public:
	CSboCvarCheckButton(vgui2::Panel *parent, const char *panelName, const char *text, const char *cvarName,
	    int onValue = 1, int offValue = 0);
	void ResetData();
	void ApplyChanges();
	//! The box differs from the cvar's value at the last ResetData() / ApplyChanges().
	bool HasBeenModified() { return IsSelected() != m_bResetState; }

private:
	const char *m_pszCvar;
	int m_iOnValue;
	int m_iOffValue;
	bool m_bResetState = false;
};

//-----------------------------------------------------------------------------
// Dropdown bound to a numeric engine cvar: one item per value (values[], labels[]). ResetData() selects the item of
// the cvar's value (the nearest one if it has no item), ApplyChanges() writes the value of the selected item with a
// console command, only when the selection changed.
//-----------------------------------------------------------------------------
class CSboCvarComboBox : public vgui2::ComboBox
{
	DECLARE_CLASS_SIMPLE(CSboCvarComboBox, vgui2::ComboBox);

public:
	enum { MAX_ITEMS = 32 };
	CSboCvarComboBox(vgui2::Panel *parent, const char *panelName, const char *cvarName, const int *values,
	    const char *const *labels, int count, int defaultValue);
	void ResetData();
	void ApplyChanges();
	//! The selection differs from the one at the last ResetData() / ApplyChanges().
	bool HasBeenModified() const { return SelectedRow() != m_iResetRow; }

private:
	int SelectedRow() const;

	const char *m_pszCvar;
	int m_iDefault;
	int m_nCount = 0;
	int m_iValues[MAX_ITEMS] = {};
	int m_iItemIds[MAX_ITEMS] = {};
	int m_iResetRow = -1;
};

//-----------------------------------------------------------------------------
// A tab of the dialog (Valve GameUI options pages, e.g. OptionsSubAudio): the controls it passes to
// WatchControl() report their changes to it, and it posts "ApplyButtonEnable" to the PropertySheet for each.
// HasChanges(): a control differs from its saved value (the dialog's Apply button follows it).
//-----------------------------------------------------------------------------
class CSboOptionsPage : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(CSboOptionsPage, vgui2::PropertyPage);

public:
	CSboOptionsPage(vgui2::Panel *parent, const char *panelName) : BaseClass(parent, panelName) {}
	virtual bool HasChanges() { return false; }

protected:
	void WatchControl(vgui2::Panel *control) { control->AddActionSignalTarget(this); }

private:
	void OnControlModified();
	MESSAGE_FUNC_PTR(OnCheckButtonChecked, "CheckButtonChecked", panel) { OnControlModified(); }
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel) { OnControlModified(); }
};

class CSboGeneralOptions : public CSboOptionsPage
{
	DECLARE_CLASS_SIMPLE(CSboGeneralOptions, CSboOptionsPage);

public:
	CSboGeneralOptions(vgui2::Panel *parent);

	virtual void PerformLayout() override;
	virtual void OnResetData() override;
	virtual void OnApplyChanges() override;
	virtual bool HasChanges() override;
	//! Height the rows need at the page's width (set by PerformLayout); the dialog grows to fit it
	int GetContentTall() const { return m_iContentTall; }

private:
	int m_iContentTall = 0;
	vgui2::Label *m_pDiscordTitle = nullptr;
	CSboCvarCheckButton *m_pDiscord = nullptr;
	vgui2::Label *m_pDiscordHelp = nullptr;
	vgui2::Label *m_pWeaponsTitle = nullptr;
	CSboCvarCheckButton *m_pLeftHanded = nullptr;
	vgui2::Label *m_pMenusTitle = nullptr;
	CSboCvarCheckButton *m_pMenus = nullptr;
	vgui2::Label *m_pMenusHelp = nullptr;
	CSboCvarCheckButton *m_pIngame = nullptr; //!< cl_new_ingame_menus, in the Menus section
	vgui2::Label *m_pIngameHelp = nullptr;
};

//-----------------------------------------------------------------------------
// Chat tab: text size (hud_chat_fontsize), name and chat colors (hud_colortext: the chat, scoreboard, kill feed and
// voice list), sound (hud_saytext_sound), message time (hud_saytext_time). Name and chat colors are set in the chat
// line's color wheel.
//-----------------------------------------------------------------------------
class CSboChatOptions : public CSboOptionsPage
{
	DECLARE_CLASS_SIMPLE(CSboChatOptions, CSboOptionsPage);

public:
	CSboChatOptions(vgui2::Panel *parent);

	virtual void PerformLayout() override;
	virtual void OnResetData() override;
	virtual void OnApplyChanges() override;
	virtual bool HasChanges() override;

private:
	vgui2::Label *m_pTitle = nullptr;
	vgui2::Label *m_pFontSizeLabel = nullptr;
	CSboCvarComboBox *m_pFontSize = nullptr;
	CSboCvarCheckButton *m_pColors = nullptr;
	CSboCvarCheckButton *m_pSound = nullptr;
	vgui2::Label *m_pTimeLabel = nullptr;
	CSboCvarComboBox *m_pTime = nullptr;
	// hud_chat_color (userinfo): the chat line's color picker, written on Apply
	vgui2::Label *m_pChatColorLabel = nullptr;
	CSboColorField *m_pChatColor = nullptr;
	ChatColor m_ChatColorReset;
};

//-----------------------------------------------------------------------------
// Voice tab (GameUI's own Voice tab cannot be changed): the engine's automatic gain for the voices heard,
// voice_maxgain 1..10 ("5x") and voice_avggain 0..1 (a percentage), as Options > Voice in the new menus; Default puts
// HL25's 5 and 0.5 back. Kept in voicegain.vdf (the engine does not archive them: SboVoiceGain_*).
//-----------------------------------------------------------------------------
class CSboVoiceOptions : public CSboOptionsPage
{
	DECLARE_CLASS_SIMPLE(CSboVoiceOptions, CSboOptionsPage);

public:
	CSboVoiceOptions(vgui2::Panel *parent);

	virtual void PerformLayout() override;
	virtual void OnResetData() override;
	virtual void OnApplyChanges() override;
	virtual bool HasChanges() override;
	virtual void OnCommand(const char *command) override;

private:
	MESSAGE_FUNC(OnSliderMoved, "SliderMoved");
	void UpdateValues();
	vgui2::Label *m_pTitle = nullptr;
	vgui2::Label *m_pMaxLabel = nullptr, *m_pMaxValue = nullptr, *m_pAvgLabel = nullptr, *m_pAvgValue = nullptr;
	vgui2::Slider *m_pMax = nullptr, *m_pAvg = nullptr; //!< voice_maxgain 1..10, voice_avggain 0..100 (percent)
	vgui2::Button *m_pDefault = nullptr;
	int m_iMaxReset = 5, m_iAvgReset = 50;
};

//-----------------------------------------------------------------------------
// Version tab (BugfixedHL-Rebased gameui/options/options_about.cpp): this build's version and the update check.
//-----------------------------------------------------------------------------
class CSboLinkLabel;

// The Version tab's credits line, in both looks (this dialog and the new-style Advanced window): the credits are in
// the repository's README
#define SBO_CREDITS_TEXT "See the repo for credits:"
#define SBO_REPO_LINK_TEXT "github.com/syndicate-black-ops/syndicate-black-ops"
#define SBO_REPO_URL "https://github.com/syndicate-black-ops/syndicate-black-ops"

class CSboVersionOptions : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(CSboVersionOptions, vgui2::PropertyPage);

public:
	CSboVersionOptions(vgui2::Panel *parent);

	virtual void PerformLayout() override;
	virtual void OnThink() override;
	virtual void OnCommand(const char *command) override;

private:
	void UpdateControls();

	vgui2::Label *m_pTitle = nullptr;
	vgui2::Label *m_pVersion = nullptr;
	vgui2::Label *m_pLatest = nullptr;
	vgui2::Button *m_pCheck = nullptr;
	vgui2::Label *m_pCredits = nullptr;
	CSboLinkLabel *m_pRepoLink = nullptr;
	int m_iLastState = -1;
};

class CSboOptionsDialog : public vgui2::PropertyDialog
{
	DECLARE_CLASS_SIMPLE(CSboOptionsDialog, vgui2::PropertyDialog);

public:
	CSboOptionsDialog(vgui2::Panel *pParent);

	//! "ApplyButtonEnable" from the sheet: Apply enabled while a page has changes
	virtual void OnApplyButtonEnable() override;
	virtual void PerformLayout() override; // taller when the General page's rows need it
#ifndef _WIN32
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override; // Linux: the close button's glyph
#endif

private:
	static constexpr int DIALOG_TALL = 300;
	CSboOptionsPage *m_pPages[3] = {}; // General, Chat, Voice (Version has no settings)
};

//! Registers the gameui_cl_open_adv_options console command (HUD_Init time).
void VGui2_RegisterOptionsCommand();
//! Opens the dialog if gameui_cl_open_adv_options asked for it (from CGameUIViewport::Frame, one frame after the command).
void VGui2_Options_Frame();
//! cl_experimental_menus changed (VGui2_MenusStyle_Frame): closes the old-style Advanced dialog if it is open and
//! returns its page (0 General, 1 Chat, 2 Version), else -1
int VGui2_OldAdvanced_CloseIfOpen();
//! Opens the old-style Advanced dialog on the next frame on that page (-1: as it opens)
void VGui2_OldAdvanced_OpenNextFrame(int page);

#endif // SBO_VGUI2_SBO_OPTIONS_H
