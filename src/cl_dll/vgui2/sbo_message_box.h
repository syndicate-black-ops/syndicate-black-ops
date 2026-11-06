//========= SBO VGUI2: game_messagebox message boxes =========//
//
// Purpose: The client side of the map entity game_messagebox (server: src/dlls/game_messagebox.cpp; mapper guide
//          docs/MESSAGE_BOX.md): the MessageBox / MsgBoxPart messages and the box drawn in the middle of the screen.
//
// Port of halflife-featureful's CHudMessageBox (FreeSlave/halflife-featureful 0542b0d, HL SDK license:
// cl_dll/hud_messagebox.cpp, game_shared/window_geometry.h; the input hooks in cl_dll/hud.cpp HandleClientButton /
// HandleKeyDown, cl_dll/hud_update.cpp, cl_dll/input.cpp HUD_Key_Event):
//   * several boxes are kept, only the newest is drawn; the primary attack closes it ("close_messagebox <id>" to the
//     server; not within 0.1 s of its arrival) and the press does nothing else;
//   * a text taller than the box scrolls: mouse wheel one line, PageUp / PageDown one box;
//   * the box: 7/9 of the screen wide, 11/15 high, centered; text padding 1/14 of its width, 1/13 of its height;
//     a dark translucent background with a 1-pixel frame in the text color; nothing is drawn while the scoreboard
//     (featureful: a "top level window") is open.
// SBO changes: drawn with VGUI2 (featureful draws HUD fills and engine text); colors and fonts from
// ui/resource/sboscheme.res ("MessageBox.*"; the fonts are proportional: they grow with the screen height, with or
// without hud_scale); the text is the entity's own message, a "#token" being looked up in titles.txt (then in the
// VGUI2 language files) instead of featureful's messages.json; a long text comes in pieces (MsgBoxPart); hidden,
// and not closed or scrolled, whenever SBO's HUD is not drawn (hud_draw 0, intermission, a menu with the mouse
// cursor).
// SBO additions: an optional title bar ("title" key); a VGUI2 scroll bar instead of featureful's
// "^^^^^^" / "......" indicators; a Close button at the bottom right (#SBO_MessageBox_Close); Enter closes too; the
// box takes the mouse: while it is shown the cursor is on and mouse look is off (a popup with mouse input, as the
// scoreboard's mouse mode), the keyboard stays with the game.
//
// SDK world (hl1_source_sdk headers); SBO data through sbo_bridge.h.
//=================================================================//

#ifndef SBO_VGUI2_SBO_MESSAGE_BOX_H
#define SBO_VGUI2_SBO_MESSAGE_BOX_H

#include <wchar.h>
#include <vector>
#include <Color.h>
#include <vgui_controls/EditablePanel.h>

namespace vgui2
{
class ScrollBar;
}
class CSboCommandButton;

class CSboMessageBox : public vgui2::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE(CSboMessageBox, vgui2::EditablePanel);

	static CSboMessageBox *Create(vgui2::Panel *pParent);
	static CSboMessageBox *Get() { return s_pInstance; }

	//! A box from the server (a box with the same id is replaced and becomes the newest). Final texts; title may be
	//! empty (no title bar).
	void Open(int id, const wchar_t *title, const wchar_t *text);
	//! The server closed it (autoclose distance; the oldest of too many).
	void Close(int id);
	//! New map / connection: no boxes.
	void Reset();

	//! The box is shown (and has the mouse).
	bool IsActive() const { return m_bActive; }
	//! At least one box is open, shown or not.
	bool HasBoxes() const { return !m_Boxes.empty(); }

	//! The primary attack (or Enter, the Close button) closes the newest box. False if nothing was closed.
	bool CloseNewest();
	//! Keys from HUD_Key_Event while a box is shown (VGUI2_MSGBOX_*): true if the box took it.
	bool HandleKey(int action);

	void OnThink() override;
	void PaintBackground() override;
	void Paint() override;
	void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	void OnCommand(const char *command) override;
	void OnMouseWheeled(int delta) override;
	void PerformLayout() override;
	void OnSizeChanged(int wide, int tall) override;

	MESSAGE_FUNC_INT(OnSliderMoved, "ScrollBarSliderMoved", position);

private:
	CSboMessageBox(vgui2::Panel *pParent);

	static CSboMessageBox *s_pInstance;

	struct Line
	{
		int start = 0; //!< in the box's text
		int len = 0;
	};

	struct Box
	{
		int id = -1;
		std::vector<wchar_t> title; //!< 0-terminated, may be empty ("")
		std::vector<wchar_t> text;  //!< 0-terminated
		std::vector<Line> lines;    //!< wrapped for the text area (without / with the scroll bar)
		int scroll = 0;             //!< first line shown
		double showTime = 0.0;
	};

	std::vector<Box> m_Boxes; //!< oldest first; the last one is drawn

	vgui2::ScrollBar *m_pScrollBar = nullptr;
	CSboCommandButton *m_pCloseButton = nullptr;

	vgui2::HFont m_hFont = vgui2::INVALID_FONT;
	vgui2::HFont m_hTitleFont = vgui2::INVALID_FONT;
	Color m_TextColor, m_BgColor, m_BorderColor, m_TitleColor, m_TitleBgColor;

	bool m_bActive = false;      //!< shown, mouse input on
	double m_flKeyWheelTime = -1; //!< the last wheel step taken from HUD_Key_Event (see OnMouseWheeled)

	// layout in pixels (Layout)
	int m_iBoxX = 0, m_iBoxY = 0, m_iBoxWide = 0, m_iBoxTall = 0;
	int m_iTitleTall = 0;                                   //!< title bar height for the shown box (0 = none)
	int m_iTextX = 0, m_iTextY = 0, m_iTextWide = 0, m_iTextTall = 0; //!< text area (scroll bar included)
	int m_iScrollWide = 0;
	int m_iButtonX = 0, m_iButtonY = 0, m_iButtonWide = 0, m_iButtonTall = 0; //!< Close button
	int m_iLineTall = 1;
	int m_iRows = 1; //!< text lines that fit
	int m_iLayoutWide = 0, m_iLayoutTall = 0;
	bool m_bLayoutTitle = false; //!< the layout was made for a box with a title

	bool ShouldShow() const;
	void UpdateActive();
	void Layout(bool force);
	void PlaceControls();
	void Wrap(Box &box) const;
	void WrapLines(Box &box, int wide) const;
	int MaxScroll(const Box &box) const;
	void SetScroll(int scroll);
	void UpdateScrollBar();
	void EnableChildInput(vgui2::Panel *pPanel);
};

#endif // SBO_VGUI2_SBO_MESSAGE_BOX_H
