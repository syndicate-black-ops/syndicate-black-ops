//========= SBO VGUI2 controls: in-game tool windows =========//
//
// Purpose: the base of SBO's in-game tool windows (the color correction editor; later the particle and world screen
//          editors), in the server browser's look (sbo_ui_style.h), small and on top of the game:
//            * CSboToolWindow: a Frame on the client viewport with a title, a close button and its content below;
//              moved by its header, sized by any edge or corner (at least two thirds of its design size), folded to its
//              header by the arrow left of the title or a double click on the header (until it closes: it always
//              opens unfolded); its place and size are kept in sbotools.vdf (mod folder, as sbomenus.vdf). SetToolTip gives a control a short tip, shown
//              next to the cursor after it rests on the control. While any tool window is open the cursor is shown, the mouse buttons and
//              wheel belong to the windows (VGui2_ToolWindows_HaveMouse: input.cpp sends them no binding) and the
//              keyboard stays the game's, so the player can still walk; a text field takes the keyboard while it is
//              edited (BeginTextInput / EndTextInput). Mouse look with the tools open: a window's "Mouse look"
//              button (ShowMouseLookButton) or the command tool_mouselook turns it on; the windows then take no mouse
//              input, so the cursor hides and the game turns the view. A hint at the bottom of the screen says Esc
//              stops it: Esc or tool_mouselook again gives the cursor back where it was. Esc also ends a pick on the
//              screen and typing in a field or a list, and it closes a window that asks for it (SetEscapeCloses: the
//              color window): while one of these waits, the game menu's Esc guard (CGameUIViewport::SetMenuEscapeGuard)
//              keeps the menu shut and hands the press to the tools.
//            * CSboScreenPicker: a click anywhere on the game's screen (outside the windows): "ScreenPicked" with the
//              point, for picking a color or a place in the world.
//
// No tool logic here. SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_CONTROLS_SBO_TOOL_WINDOW_H
#define SBO_VGUI2_CONTROLS_SBO_TOOL_WINDOW_H

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <vgui_controls/Frame.h>

class CSboBrowserButton;

//! The in-game tools (color correction, particle and world screen editors) may run: single player, a game this client
//! hosts, or a server with sv_cheats 2 or more (its "Cheats" message, sbo::GetServerCheats; sv_cheats 1 is not enough)
bool SboToolsAllowed();

class CSboToolWindow : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(CSboToolWindow, vgui2::Frame);

public:
	//! key: the window's entry in sbotools.vdf; mockWide / mockTall: its size on the design's 1280 x 800 screen;
	//! the first time it opens at (placeX, placeY), fractions of the screen for its top left corner
	CSboToolWindow(vgui2::Panel *pParent, const char *pName, const char *title, const char *key, int mockWide,
	    int mockTall, float placeX = 0.5f, float placeY = 0.2f);
	~CSboToolWindow();

	void SetToolTitle(const char *title);
	const char *GetToolTitle() const { return m_Title.c_str(); }
	//! Can be sized by its edges and corners (on by default; off: the size follows the design)
	void SetToolSizeable(bool sizeable);
	//! Folded to its header (true) or open (false); kept with its place
	void SetCollapsed(bool collapsed);
	bool IsCollapsed() const { return m_bCollapsed; }
	//! A tip for a control of this window (or one of its children): one short plain sentence; nullptr removes it
	void SetToolTip(vgui2::Panel *pControl, const char *tip);
	//! A tip that depends on where the cursor is on the control (a list's row): called while the cursor rests there;
	//! "" shows no tip
	void SetToolTipFunc(vgui2::Panel *pControl, std::function<std::string()> tip);
	//! A "Mouse look" button in the header, left of the close button (off by default)
	void ShowMouseLookButton(bool show);
	//! Esc closes the window while it is open (OnEscape), once a pick on the screen, typing and mouse look are over
	void SetEscapeCloses(bool closes);

	//! Shows the window at its kept place (or its first place), in front, with the cursor
	virtual void Open();
	//! Hides it (the place is kept)
	void CloseTool();
	bool IsOpen() const { return m_bOpen; }

	//! A text field takes the keyboard (the window's keyboard input on until EndTextInput)
	void BeginTextInput(vgui2::Panel *pField);
	void EndTextInput();
	bool HasTextInput() const { return m_pTextField != nullptr; }

	//! Tool windows open now
	static int OpenCount();
	//! Closes every open tool window
	static void CloseAll();
	//! Mouse look with the tools open: the windows give up the mouse (true) or take it back (false)
	static void SetMouseLook(bool on);
	static bool IsMouseLook();

	void PerformLayout() override;
	void PaintBackground() override;
	void PaintBorder() override;
	void Paint() override;
	void PostChildPaint() override;
	void OnThink() override;
	void OnClose() override;
	void OnCommand(const char *command) override;
	void OnScreenSizeChanged(int oldWide, int oldTall) override;
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override;
	void OnMouseReleased(vgui2::MouseCode code) override;
	void OnCursorMoved(int x, int y) override;

protected:
	//! Lay out the content in this rectangle (window coordinates)
	virtual void LayoutContent(int x, int y, int w, int h) {}
	//! The window was closed (close button, CloseTool, CloseAll)
	virtual void OnToolClosed() {}
	//! Esc with SetEscapeCloses on: closes the window
	virtual void OnEscape() { CloseTool(); }
	//! Design pixels at the current scale
	int M(int px) const;

private:
	int HeaderTall() const;
	void UpdateToolTip();
	static void EscapeTick();
	static void UpdateResizeCursor();
	void SetGripCursors();
	//! While folded the content's controls are hidden (shown again when it opens)
	void HideContent();
	bool IsHeaderControl(vgui2::Panel *p) const;
	void KeepOnScreen();
	void SaveBounds();
	void SetOpen(bool open);

	std::string m_Title;
	std::string m_Key;
	int m_iMockWide, m_iMockTall;
	float m_flPlaceX, m_flPlaceY;
	bool m_bOpen = false;
	bool m_bSizeable = false;
	CSboBrowserButton *m_pClose = nullptr;
	CSboBrowserButton *m_pLook = nullptr;
	vgui2::Panel *m_pArrow = nullptr;
	bool m_bCollapsed = false;
	int m_iExpandedTall = 0;
	bool m_bDragging = false;
	int m_iDragX = 0, m_iDragY = 0;
	std::map<vgui2::VPANEL, std::string> m_Tips;
	std::map<vgui2::VPANEL, std::function<std::string()>> m_TipFuncs;
	int m_iArrowKind = -1, m_iArrowX = 0, m_iArrowY = 0; // the resize arrow drawn over the window (-1: none)
	bool m_bEscCloses = false;
	bool m_bEscCloseHeld = false; // open with SetEscapeCloses: holds the Esc guard
	std::vector<vgui2::VPANEL> m_HiddenContent; // the content's controls folding hid
	vgui2::Panel *m_pTextField = nullptr;
	bool m_bEscHeld = false; // typing holds the Esc guard
	bool m_bFieldHadFocus = false;
	double m_flTextInputSince = 0.0;
	double m_flSaveAt = 0.0;
	int m_iSavedX = -1, m_iSavedY = -1, m_iSavedW = -1, m_iSavedH = -1;
};

//-----------------------------------------------------------------------------
// A question with buttons in a small tool window ("Save changes to x.aur?" Save / Don't save / Cancel): Ask shows it in
// front of the tools; a button (or the close button: the last choice) closes it and calls done with the button's index.
//-----------------------------------------------------------------------------
class CSboChoiceDialog : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CSboChoiceDialog, CSboToolWindow);

public:
	//! The one dialog of the tools (made on first use)
	static CSboChoiceDialog *Get();
	void Ask(const char *title, const char *message, const std::vector<std::string> &buttons,
	    std::function<void(int)> done);

	void OnCommand(const char *command) override;
	void Paint() override;

protected:
	void LayoutContent(int x, int y, int w, int h) override;
	void OnToolClosed() override;

private:
	CSboChoiceDialog();
	void Finish(int choice);
	std::string m_Message;
	std::vector<CSboBrowserButton *> m_Buttons;
	std::function<void(int)> m_Done;
	int m_iMessageY = 0;
	bool m_bFinishing = false;
};

//-----------------------------------------------------------------------------
// A click on the game's screen outside the tool windows: a full-screen, paint-less popup behind them while it waits.
// Posts "ScreenPicked" (x, y: screen pixels) to its action target on a left click; Esc or a right click posts
// "ScreenPickCancelled".
//-----------------------------------------------------------------------------
class CSboScreenPicker : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboScreenPicker, vgui2::Panel);

public:
	CSboScreenPicker(vgui2::Panel *pParent);
	//! Waits for a click; pTarget gets the result
	void Start(vgui2::Panel *pTarget);
	void Cancel();
	bool IsPicking() { return IsVisible(); }

	void OnMousePressed(vgui2::MouseCode code) override;
	void OnKeyCodePressed(vgui2::KeyCode code) override;
	void Paint() override;
	void PaintBackground() override {}

private:
	void Done();
	vgui2::VPANEL m_Target = 0;
};

#endif // SBO_VGUI2_CONTROLS_SBO_TOOL_WINDOW_H
