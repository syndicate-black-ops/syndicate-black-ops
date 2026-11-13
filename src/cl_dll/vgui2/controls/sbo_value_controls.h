//========= SBO VGUI2 controls: values =========//
//
// Purpose: controls that edit one value, in the server browser's look (sbo_ui_style.h), for SBO's tool windows
//          (sbo_tool_window.h):
//            * CSboTextField: a text field; typing takes the keyboard until Enter, Esc or a click elsewhere, then
//              "TextEntered" (text; enter: 1 when Enter ended it);
//            * CSboNumberEntry: a number field; typing takes the keyboard until Enter, Esc or a click elsewhere, then
//              "NumberEntered" (value) when the text is a number (clamped to its range);
//            * CSboTickSlider: a horizontal slider with tick marks under its track and its minimum and maximum under
//              the ends; dragging, a click or the wheel post "SliderMoved" (value), a right click sets its default
//              (posted with reset 1, also when the value was the default already);
//            * CSboLabeledSlider: a label, a tick slider and a number field on one row, kept in step (drag or type);
//              "ValueChanged" (value; reset 1 when a right click set the default) on every change made by the user;
//            * CSboColorSwatch: a box in a color (and its alpha, over a checker board); a click opens the color window beside it:
//              the chat's color wheel and brightness bar (hud_chat_color_picker.h, as a plain color picker) and, for a
//              color with alpha, an alpha slider under them. "ColorPickerOpened" when the window opens for it,
//              "ColorChanged" (r, g, b 0..255, a 0..1) on every change made by the user (live while dragging); Esc
//              puts the color back as it was when the window opened, closes it and posts "ColorChanged" with that
//              color and cancel 1.
//
// Values are floats with a step (1 for whole numbers) and a number of decimals to show. No tool logic here.
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_CONTROLS_SBO_VALUE_CONTROLS_H
#define SBO_VGUI2_CONTROLS_SBO_VALUE_CONTROLS_H

#include <string>

#include <Color.h>
#include <vgui_controls/Panel.h>

#include "../sbo_server_browser.h" // CSboBrowserEntry
#include "sbo_tool_window.h"

//-----------------------------------------------------------------------------
// Range of a value: min .. max in steps of step (0: any), shown with decimals
//-----------------------------------------------------------------------------
struct SboValueRange
{
	float min = 0.0f, max = 1.0f, step = 1.0f;
	int decimals = 0;
	//! Clamped and snapped to the step
	float Fix(float v) const;
	//! The value as text ("-24", "1.50")
	void Format(float v, char *out, int outSize) const;
};

class CSboTextField : public CSboBrowserEntry
{
	DECLARE_CLASS_SIMPLE(CSboTextField, CSboBrowserEntry);

public:
	CSboTextField(vgui2::Panel *pParent, const char *pName, const char *placeholder);
	//! Shows the text (no message)
	void SetValue(const char *text);
	const char *GetValue() const { return m_Value.c_str(); }
	//! Takes the keyboard for typing, as a click on it does (a window that opens on its field)
	void StartEditing();

	void OnMousePressed(vgui2::MouseCode code) override;
	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	void OnKillFocus() override;

private:
	void Commit(bool enter);
	std::string m_Value;
	bool m_bEditing = false;
};

class CSboNumberEntry : public CSboBrowserEntry
{
	DECLARE_CLASS_SIMPLE(CSboNumberEntry, CSboBrowserEntry);

public:
	CSboNumberEntry(vgui2::Panel *pParent, const char *pName);
	void SetRange(const SboValueRange &range) { m_Range = range; }
	//! Shows the value (no message)
	void SetValue(float value);
	float GetValue() const { return m_flValue; }

	void OnMousePressed(vgui2::MouseCode code) override;
	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	void OnKillFocus() override;

private:
	void Commit();
	CSboToolWindow *Window();
	SboValueRange m_Range;
	float m_flValue = 0.0f;
	bool m_bEditing = false;
};

class CSboTickSlider : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboTickSlider, vgui2::Panel);

public:
	CSboTickSlider(vgui2::Panel *pParent, const char *pName);
	void SetRange(const SboValueRange &range);
	const SboValueRange &GetRange() const { return m_Range; }
	//! Shows the value (no message)
	void SetValue(float value);
	float GetValue() const { return m_flValue; }
	//! Spaces between tick marks (0: none)
	void SetTickCount(int ticks) { m_iTicks = ticks; Repaint(); }
	//! The minimum and maximum under the ends (on by default)
	void SetShowRangeLabels(bool show) { m_bRangeLabels = show; Repaint(); }
	//! The value a right click sets (the range's minimum until set)
	void SetDefaultValue(float value) { m_flDefault = value; m_bHasDefault = true; }
	float GetDefaultValue() const { return m_bHasDefault ? m_flDefault : m_Range.min; }

	void Paint() override;
	void PaintBackground() override {}
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override { OnMousePressed(code); }
	void OnMouseReleased(vgui2::MouseCode code) override;
	void OnCursorMoved(int x, int y) override;
	void OnMouseWheeled(int delta) override;

private:
	void SetFromX(int x);
	void Change(float v);
	int TrackY();
	SboValueRange m_Range;
	float m_flValue = 0.0f;
	int m_iTicks = 10;
	bool m_bRangeLabels = true;
	bool m_bDragging = false;
	float m_flDefault = 0.0f;
	bool m_bHasDefault = false;
};

class CSboLabeledSlider : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboLabeledSlider, vgui2::Panel);

public:
	CSboLabeledSlider(vgui2::Panel *pParent, const char *pName, const char *label, const SboValueRange &range);
	void SetLabel(const char *label);
	void SetRange(const SboValueRange &range);
	//! Shows the value in both (no message)
	void SetValue(float value);
	float GetValue() const { return m_flValue; }
	//! The value a right click on the slider sets (sent as a change, like a drag)
	void SetDefaultValue(float value) { m_pSlider->SetDefaultValue(value); }
	//! The label column and the number field, design pixels
	void SetColumns(int labelWide, int entryWide);
	CSboTickSlider *Slider() { return m_pSlider; }
	//! Its height for its width, design pixels (the row with the range labels)
	static int DesignTall() { return 34; }

	void PerformLayout() override;
	void Paint() override;
	void PaintBackground() override {}

	MESSAGE_FUNC_PARAMS(OnSliderMoved, "SliderMoved", data);
	MESSAGE_FUNC_FLOAT(OnNumberEntered, "NumberEntered", value);

private:
	void Changed(float v, bool reset = false);
	std::string m_Label;
	SboValueRange m_Range;
	float m_flValue = 0.0f;
	int m_iLabelWide = 80, m_iEntryWide = 44;
	CSboTickSlider *m_pSlider;
	CSboNumberEntry *m_pEntry;
};

class CSboColorWindow;

class CSboColorSwatch : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboColorSwatch, vgui2::Panel);

public:
	//! title: the color window's title ("Color")
	CSboColorSwatch(vgui2::Panel *pParent, const char *pName, const char *title);
	~CSboColorSwatch();
	//! The color has an alpha: the swatch shows it, the color window has an alpha slider (off by default)
	void SetHasAlpha(bool hasAlpha);
	bool HasAlpha() const { return m_bHasAlpha; }
	//! Shows the color (no message)
	void SetColor(int r, int g, int b);
	void GetColor(int &r, int &g, int &b) const;
	//! Shows the alpha, 0..1 (no message)
	void SetAlpha(float a);
	float GetAlpha() const { return m_flAlpha; }
	//! Closes its color window (the color stays as it is)
	void ClosePicker();
	bool IsPickerOpen() const;

	void Paint() override;
	void PaintBackground() override {}
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override { OnMousePressed(code); }
	void OnCursorEntered() override { Repaint(); }
	void OnCursorExited() override { Repaint(); }

	//! From the color window
	void PickerChanged(int r, int g, int b, float a, bool cancel);
	void PickerDetached() { m_pWindow = nullptr; Repaint(); }

private:
	std::string m_Title;
	int m_Color[3] = { 255, 255, 255 };
	float m_flAlpha = 1.0f;
	bool m_bHasAlpha = false;
	CSboColorWindow *m_pWindow = nullptr; // the color window while it is open for this swatch
};

#endif // SBO_VGUI2_CONTROLS_SBO_VALUE_CONTROLS_H
