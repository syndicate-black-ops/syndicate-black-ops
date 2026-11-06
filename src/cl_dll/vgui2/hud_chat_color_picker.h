//========= SBO VGUI2 port =========//
//
// Purpose: The chat input's color picker: the collapsed round swatch left of the prompt and the popup it opens
//          (a "Chat" / "Name" switch for the edited setting, color wheel, brightness bar, "None" and the ^1..^8
//          presets).
//
// The wheel is a port of BugfixedHL-Rebased's color picker (src/game/client/gameui/options/color_picker.{h,cpp} and
// options/colorpicker/): its HSV <-> RGB conversion, the picker panel + bar panel pair (two of the HSV axes on a
// plane, the third on a bar), a round selection marker, press-and-drag mouse handling and procedural RGBA textures
// (BHL's CTextureManager; no raw OpenGL, so it works in the software renderer too). SBO changes:
//   * a compact popup inside the chat panel instead of a Frame dialog with OK / Cancel and an RGB text field: the
//     picked color is previewed while dragging and selected on release;
//   * the plane is a hue (angle) / saturation (radius) wheel and the bar is the brightness, BHL has a
//     saturation / value square and a hue bar;
//   * every color is shown as the chat will draw it, no darker than COLOR_CODE_MIN_LUMA (hud_chat.h); a picked
//     color is any 24-bit color (hue and saturation as pointed at); the bar has brightness levels and the wheel one
//     texture per level (BHL: one texture per hue);
//   * the selection marker and the swatches are drawn with generated round masks (BHL: ui/gfx/circle_*.tga).
//
// SDK world: compiled with hl1_source_sdk include paths.
//
//==================================//

#ifndef SBO_VGUI2_HUD_CHAT_COLOR_PICKER_H
#define SBO_VGUI2_HUD_CHAT_COLOR_PICKER_H

#include <vgui/MouseCode.h>
#include <vgui_controls/Panel.h>
#include <Color.h>

#include "hud_chat.h"

namespace colorpicker
{
// BHL color_picker.h
// hue 0 - 360
// sat and val 0 - 100
void HSVtoRGB(float h, float s, float v, Color &output);
void RGBtoHSV(Color rgb, float &hue, float &sat, float &val);

//! Brightness levels of the picker: value = level / MAX_LEVEL. The levels below MIN_LEVEL all end up at the
//! brightness floor (greys), so the bar doesn't offer them.
constexpr int MAX_LEVEL = 15;
constexpr int MIN_LEVEL = 5;

class CWheelPanel;
class CBrightnessPanel;
class CSwatchPanel;
class CTargetPanel;
}

//-----------------------------------------------------------------------------
// Purpose: what the picker popup edits. The chat's input line is one host (the default: the chat's "Chat" / "Name"
// switch, hud_chat_color and namecolor); a settings window's color field is another (one cvar, applied with the
// window's Apply).
//-----------------------------------------------------------------------------
class IChatColorPickerHost
{
public:
	virtual ~IChatColorPickerHost() {}
	//! The picker takes input (the chat: its input line is open)
	virtual bool IsPickerActive() = 0;
	//! The edited color (the one being picked while dragging)
	virtual ChatColor GetPickerColor() = 0;
	//! A color was selected (wheel released, quick pick)
	virtual void SetPickerColor(const ChatColor &color) = 0;
	//! A color is being dragged (shown, not selected yet) / the drag ended
	virtual void SetPickerPreview(const ChatColor &color) = 0;
	virtual void ClearPickerPreview() = 0;
	//! A quick pick closes the popup
	virtual void ClosePicker() = 0;
	//! After a click in the popup (the chat gives the focus back to its input line)
	virtual void OnPickerInput() {}
	//! Text color of "none" (a ring) and of the tooltip
	virtual Color GetPickerTextColor() = 0;
	virtual void ShowPickerTooltip(vgui2::Panel *pOwner, const wchar_t *text) {}
	virtual void HidePickerTooltip(vgui2::Panel *pOwner) {}
	//! The "Chat" / "Name" switch above the wheel (only the chat's popup edits two settings)
	virtual bool HasPickerTargets() { return false; }
	virtual ChatColorTarget GetPickerTarget() { return ChatColorTarget::Chat; }
	virtual void SetPickerTarget(ChatColorTarget target) {}
};

//! The name of a color as the picker's tooltips show it ("Red", "^x4AF", "None (default color)")
void GetChatColorDisplayName(const ChatColor &color, wchar_t *buf, int bufSizeBytes);

//! The swatch of a picker color in (x, y, size, size): a disc in the color in a dark rim (white, thicker: selected);
//! "none" is a hollow ring in noneColor
void DrawChatColorSwatch(Color noneColor, const ChatColor &color, int x, int y, int size, bool bSelected);

//-----------------------------------------------------------------------------
// Purpose: the collapsed color picker: a circle in the current color (a hollow ring for "none") plus a small
// arrow; a click opens or closes the popup
//-----------------------------------------------------------------------------
class CHudChatColorButton : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CHudChatColorButton(vgui2::Panel *parent, const char *panelName);

	//! width for a given circle size (circle + arrow)
	static int GetWideForSwatch(int swatch);

	virtual void Paint();
	virtual void OnMousePressed(vgui2::MouseCode code);
	virtual void OnMouseDoublePressed(vgui2::MouseCode code) { OnMousePressed(code); }
};

//-----------------------------------------------------------------------------
// Purpose: the color picker popup, above the input line
//-----------------------------------------------------------------------------
class CHudChatColorPicker : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	//! pHost: what it edits; null = the chat's input line
	CHudChatColorPicker(vgui2::Panel *parent, const char *panelName, IChatColorPickerHost *pHost = nullptr);
	IChatColorPickerHost *Host() const;

	//! Makes the round masks of the swatches and the marker (once; the chat input opens)
	static void PrecacheTextures();

	//! Sizes the popup: cell = the height of the input line, maxTall = the room above it
	void SetMetrics(int cell, int maxTall);

	//! Moves the wheel / bar selection to a color (the popup opens); "none" keeps the last one
	void SetColor(const ChatColor &color);

	//! Ends a drag without selecting (the popup closes)
	void CancelDrag();

	//! The wheel / bar selection: hue 0..360, saturation 0..100, brightness level MIN_LEVEL..MAX_LEVEL
	float GetHue() const { return m_flHue; }
	float GetSat() const { return m_flSat; }
	int GetLevel() const { return m_iLevel; }
	void SetHueSat(float hue, float sat);
	void SetLevel(int level);

	//! The color of a wheel / bar position: brightness floor applied (exactly the color the wheel shows there)
	static ChatColor MakeColor(float hue, float sat, int level);
	//! The selected position's color
	ChatColor GetPickedColor() const { return MakeColor(m_flHue, m_flSat, m_iLevel); }

	//! Drag feedback from the wheel / bar: previews the picked color; bSelect (button released) selects it
	void OnPicked(vgui2::Panel *pSource, bool bSelect);

	virtual void Paint();
	virtual void PerformLayout();

private:
	colorpicker::CWheelPanel *m_pWheel = nullptr;
	colorpicker::CBrightnessPanel *m_pBrightness = nullptr;
	colorpicker::CSwatchPanel *m_pSwatches[CHAT_COLOR_PRESETS] = {};
	colorpicker::CTargetPanel *m_pTargets[2] = {}; //!< "Chat", "Name": the setting the wheel and the row edit
	IChatColorPickerHost *m_pHost = nullptr;

	int m_iCell = 0;
	int m_iTargetTall = 0;
	int m_iWheelSize = 0;
	int m_iBarWide = 0;
	int m_iSwatchSize = 0;

	float m_flHue = 0;
	float m_flSat = 0;
	int m_iLevel = colorpicker::MAX_LEVEL;
};

//-----------------------------------------------------------------------------
// Purpose: a color setting in an options window (Name color: namecolor, Chat color: hud_chat_color): a box with a
// swatch of the color and its name; a click opens the chat's picker (wheel, brightness bar, none + ^1..^8) below it,
// a click outside closes it. A change posts "TextChanged" to the action targets; the window writes the cvar on Apply.
//-----------------------------------------------------------------------------
class CSboColorPopup;

class CSboColorField : public vgui2::Panel, public IChatColorPickerHost
{
	typedef vgui2::Panel BaseClass;

public:
	CSboColorField(vgui2::Panel *parent, const char *panelName);
	~CSboColorField();

	void SetColor(const ChatColor &color);
	ChatColor GetColor() const { return m_Color; }
	//! The cvar value of a color ("0".."8", "xRRGGBB"), as the chat writes hud_chat_color and namecolor
	static void FormatValue(const ChatColor &color, char *value, int valueSize);
	//! Font, text and frame colors (the window's look)
	void SetLook(vgui2::HFont font, Color text, Color frame, Color background);

	virtual void Paint();
	virtual void OnMousePressed(vgui2::MouseCode code);
	virtual void OnMouseDoublePressed(vgui2::MouseCode code) { OnMousePressed(code); }
	virtual void OnThink();
	virtual void SetVisible(bool state);

	void OpenPicker();
	bool IsPickerOpen() const;

	// IChatColorPickerHost
	bool IsPickerActive() override { return IsPickerOpen(); }
	ChatColor GetPickerColor() override { return m_bPreview ? m_Preview : m_Color; }
	void SetPickerColor(const ChatColor &color) override;
	void SetPickerPreview(const ChatColor &color) override;
	void ClearPickerPreview() override;
	void ClosePicker() override;
	Color GetPickerTextColor() override { return m_TextColor; }

private:
	ChatColor m_Color;
	ChatColor m_Preview;
	bool m_bPreview = false;
	CSboColorPopup *m_pPopup = nullptr;
	vgui2::HFont m_hFont = 0;
	Color m_TextColor = Color(255, 255, 255, 255);
	Color m_FrameColor = Color(255, 255, 255, 64);
	Color m_BgColor = Color(0, 0, 0, 160);
	bool m_bMouseWasDown = false;
};

//-----------------------------------------------------------------------------
// Purpose: the name of the hovered preset / the code of the color being picked (ChatFont, drawn by the chat itself)
//-----------------------------------------------------------------------------
class CHudChatColorTooltip : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CHudChatColorTooltip(vgui2::Panel *parent, const char *panelName);

	//! sets the text and sizes the panel to fit it
	void SetTooltipText(const wchar_t *text);

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
	virtual void Paint();

private:
	vgui2::HFont m_hFont = 0;
	wchar_t m_szText[64] = {};
};

#endif // SBO_VGUI2_HUD_CHAT_COLOR_PICKER_H
