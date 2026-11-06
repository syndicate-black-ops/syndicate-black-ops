//========= SBO VGUI2: windows and setting rows in the server browser's look =========//
//
// Purpose: the parts SBO's Options and Create server windows share (docs/MENUS.md),
//          drawn like the server browser (sbo_ui_style.h):
//            * CSboWindow: a Frame on the GameUI viewport (never a child of GameUI's panels) with the title, the close
//              button, a tab row and buttons at the bottom right; it opens beside the game menu, can be moved by its
//              header and resized, and its place is kept in sbomenus.vdf (mod folder, as serverbrowser.vdf);
//            * CSboSlider: a horizontal slider;
//            * CSboSetting: one setting (a label and a control) that reads its value from the engine (Reset), knows
//              whether it was changed and writes it the way GameUI's dialog does (Apply);
//            * CSboSettingsPage: a tab's content, cards of setting rows laid out in one or two columns, scrolled with
//              the mouse wheel.
//
// SDK world (hl1_source_sdk headers); engine calls through sbo_bridge.h.
//=================================================================//

#ifndef SBO_VGUI2_SBO_SETTINGS_WINDOW_H
#define SBO_VGUI2_SBO_SETTINGS_WINDOW_H

#include <functional>
#include <string>
#include <vector>

#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Frame.h>

#include "sbo_server_browser.h" // CSboBrowserButton, CSboBrowserCheck, CSboBrowserSelect, CSboBrowserEntry

class CSboLinkLabel;

//-----------------------------------------------------------------------------
// Slider: value in [min, max]; dragging or clicking posts "SliderMoved" to its action targets
//-----------------------------------------------------------------------------
class CSboSlider : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboSlider, vgui2::Panel);

public:
	CSboSlider(vgui2::Panel *pParent, const char *pName, float min, float max);
	void SetValue(float value);
	float GetValue() const { return m_flValue; }
	void Paint() override;
	void PaintBackground() override {}
	void PaintBorder() override {}
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseReleased(vgui2::MouseCode code) override;
	void OnCursorMoved(int x, int y) override;

private:
	void SetFromX(int x);
	float m_flMin, m_flMax, m_flValue;
	bool m_bDragging = false;
};

//! A slider with its value as text at its right (posts "SliderMoved" to pPage); SboValueSliderOf gives its slider
vgui2::Panel *SboMakeValueSlider(vgui2::Panel *pPage, float min, float max, const char *format);
CSboSlider *SboValueSliderOf(vgui2::Panel *pValueSlider);

//-----------------------------------------------------------------------------
// One setting
//-----------------------------------------------------------------------------
class CSboSetting
{
public:
	virtual ~CSboSetting() {}
	//! The row's label ("" for a full-width control)
	std::string label;
	//! A short line under the row (where the value comes from, a restart note); may be empty
	std::string note;
	//! What the setting does, one line: shown after a short hover over the row's label or control; may be empty
	std::string tooltip;
	//! The control, created by the setting with the page as its parent
	vgui2::Panel *control = nullptr;
	//! Preferred control width in design pixels (0 = the column's control width)
	int controlWide = 0;
	//! Row height in design pixels
	int rowTall = 30;
	//! Left out of the layout (no space, label not drawn); the owner hides the control itself
	bool hidden = false;

	//! Reads the value from the engine into the control
	virtual void Reset() = 0;
	//! The control differs from what Reset read
	virtual bool Changed() = 0;
	//! Writes the value as GameUI does (only called when Changed())
	virtual void Apply() = 0;
};

//! Check box on a cvar. GameUI's CCvarToggleCheckButton (onValue 1, offValue 0) or a cvar with two values
class CSboCvarToggle : public CSboSetting
{
public:
	CSboCvarToggle(vgui2::Panel *pPage, const char *text, const char *cvar, float onValue = 1.0f, float offValue = 0.0f);
	void Reset() override;
	bool Changed() override;
	void Apply() override;

private:
	std::string m_Cvar;
	float m_flOn, m_flOff;
	bool m_bReset = false;
};

//! GameUI's CCvarNegateCheckButton (Reverse mouse: m_pitch below 0); keeps the magnitude, 0.022 when it was 0
class CSboCvarNegate : public CSboSetting
{
public:
	CSboCvarNegate(vgui2::Panel *pPage, const char *text, const char *cvar);
	void Reset() override;
	bool Changed() override;
	void Apply() override;

private:
	std::string m_Cvar;
	bool m_bReset = false;
};

//! GameUI's CCvarSlider: Cvar_SetValue with the slider's value when it was moved. format: printf text of the value
class CSboCvarSlider : public CSboSetting
{
public:
	CSboCvarSlider(vgui2::Panel *pPage, const char *text, const char *cvar, float min, float max, const char *format);
	void Reset() override;
	bool Changed() override;
	void Apply() override;
	CSboSlider *Slider();

private:
	std::string m_Cvar;
	float m_flReset = 0.0f;
};

//! Drop-down of fixed values on a numeric cvar (Cvar_SetValue); a value not in the list selects the nearest
class CSboCvarChoice : public CSboSetting
{
public:
	CSboCvarChoice(vgui2::Panel *pPage, const char *text, const char *cvar, const std::vector<float> &values,
	    const std::vector<std::string> &labels);
	void Reset() override;
	bool Changed() override;
	void Apply() override;

private:
	std::string m_Cvar;
	std::vector<float> m_Values;
	int m_iReset = 0;
};

//! GameUI's CLabeledCommandComboBox: items with a console command each; the initial item from a function
class CSboCommandChoice : public CSboSetting
{
public:
	CSboCommandChoice(vgui2::Panel *pPage, const char *text, const std::vector<std::string> &labels,
	    const std::vector<std::string> &commands, std::function<int()> current);
	void Reset() override;
	bool Changed() override;
	void Apply() override;
	int Selected();

private:
	std::vector<std::string> m_Commands;
	std::function<int()> m_Current;
	int m_iReset = 0;
};

//! GameUI's CCvarTextEntry (Name): `cvar "text"` as a console command when the text changed
class CSboCvarText : public CSboSetting
{
public:
	CSboCvarText(vgui2::Panel *pPage, const char *text, const char *cvar, int maxChars);
	void Reset() override;
	bool Changed() override;
	void Apply() override;

private:
	std::string m_Cvar;
	std::string m_Reset;
};

//! Any control: the page's own functions for the three steps
class CSboCustomSetting : public CSboSetting
{
public:
	std::function<void()> onReset;
	std::function<bool()> onChanged;
	std::function<void()> onApply;
	void Reset() override
	{
		if (onReset)
			onReset();
	}
	bool Changed() override { return onChanged && onChanged(); }
	void Apply() override
	{
		if (onApply)
			onApply();
	}
};

//-----------------------------------------------------------------------------
// A tab's content: cards (a heading and rows), one or two columns, wheel scrolling
//-----------------------------------------------------------------------------
class CSboSettingsPage : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CSboSettingsPage, vgui2::EditablePanel);

public:
	explicit CSboSettingsPage(vgui2::Panel *pParent, const char *pName);
	~CSboSettingsPage();

	//! A new card; column -1 = the shorter column (or the only one), 0 left, 1 right; wide: across both columns
	void AddCard(const char *heading, int column = -1, bool wide = false);
	//! Adds a setting to the last card (the page owns it)
	CSboSetting *Add(CSboSetting *pSetting);
	//! A line of text in the last card (no control); the result names it for SetText
	int AddText(const char *text);
	//! Changes the text of a line added with AddText
	void SetText(int textId, const char *text);
	//! Changes the label left of a button row's button
	void SetButtonLabel(vgui2::Panel *pButton, const char *label);
	//! A button row in the last card; the command goes to the page's OnCommand
	CSboBrowserButton *AddButton(const char *text, const char *command, const char *label = "");
	//! A line of text in the last card followed by a web link (CSboLinkLabel: opened as chat links are)
	CSboLinkLabel *AddLink(const char *text, const char *linkText, const char *url);

	virtual void ResetAll();
	virtual bool AnyChanged();
	virtual void ApplyAll();
	//! A control changed: tells the window (Apply button)
	void NotifyChanged();

	void PerformLayout() override;
	void PaintBackground() override;
	void Paint() override;
	void PaintBorder() override {}
	void OnMouseWheeled(int delta) override;
	//! The row under the mouse: its tooltip after TOOLTIP_DELAY seconds
	void OnThink() override;
	//! A button row's command goes to the window (VGUI2's default only posts it to the page's own listeners, which
	//! has none: the buttons did nothing)
	void OnCommand(const char *command) override;

	MESSAGE_FUNC(OnControlChanged, "SelectChanged") { NotifyChanged(); }
	MESSAGE_FUNC_PTR(OnButtonToggled, "ButtonToggled", panel) { NotifyChanged(); }
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel) { NotifyChanged(); }
	MESSAGE_FUNC(OnSliderMoved, "SliderMoved") { NotifyChanged(); }

protected:
	struct Row
	{
		CSboSetting *setting = nullptr; //!< or null for a text line
		std::string text;               //!< text line
		vgui2::Panel *panel = nullptr;  //!< a button row's button, or a link row's link
		bool link = false;              //!< a link row: text, then the link (panel)
		std::string buttonLabel;
		int y = 0, tall = 0; //!< laid out, page coordinates (before scrolling)
		std::vector<std::wstring> labelLines, noteLines; //!< wrapped by the layout (a text row's lines in noteLines)
	};
	struct Card
	{
		std::string heading;
		int column = -1;
		bool wide = false;
		std::vector<Row> rows;
		int x = 0, y = 0, w = 0, h = 0; //!< laid out
		int labelW = 0;                 //!< the label column (fits the longest label, at most 55% of the card)
	};
	std::vector<Card> m_Cards;
	std::vector<CSboSetting *> m_Settings;
	int m_iScroll = 0, m_iContentTall = 0;
	const CSboSetting *m_pHover = nullptr; //!< the row under the mouse (its tooltip)
	double m_flHoverSince = 0.0;
	//! Two columns when the page is at least this wide (design pixels)
	int m_iTwoColumnWide = 760;
};

//-----------------------------------------------------------------------------
// The window
//-----------------------------------------------------------------------------
class CSboWindow : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(CSboWindow, vgui2::Frame);

public:
	//! key: the window's entry in sbomenus.vdf; mockWide / mockTall: its size on the design's 1280 x 800 page
	CSboWindow(vgui2::Panel *pParent, const char *pName, const char *title, const char *key, int mockWide, int mockTall);

	virtual void Open();
	void AddTab(const char *text);
	void SetTab(int tab);
	int GetTab() const { return m_iTab; }
	//! A button at the bottom right, added right to left in the order of the calls
	CSboBrowserButton *AddBottomButton(const char *text, const char *command, bool primary);

	void PerformLayout() override;
	void PaintBackground() override;
	void PaintBorder() override;
	void Paint() override;
	void OnThink() override;
	void OnCommand(const char *command) override;
	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	void OnScreenSizeChanged(int oldWide, int oldTall) override;
	void OnClose() override;

protected:
	//! The tab changed (show its page)
	virtual void OnTabChanged(int tab) {}
	//! Lay out the content in this rectangle (window coordinates)
	virtual void LayoutContent(int x, int y, int w, int h) {}

	std::string m_Title;
	std::string m_Key;
	int m_iMockWide, m_iMockTall;
	std::vector<CSboBrowserButton *> m_Tabs;
	std::vector<CSboBrowserButton *> m_Bottom;
	int m_iTab = 0;
	int m_iTabLineY = -1;
	CSboBrowserButton *m_pClose = nullptr;

private:
	void KeepOnScreen();
	void SaveBounds();
	double m_flSaveAt = 0.0;
	int m_iSavedX = -1, m_iSavedY = -1, m_iSavedW = -1, m_iSavedH = -1;
};

//! Bounds of GameUI's game menu (a visible panel named or classed "...GameMenu..." under the GameUI panel)
bool SboFindGameMenuBounds(int &x, int &y, int &w, int &h);

#endif // SBO_VGUI2_SBO_SETTINGS_WINDOW_H
