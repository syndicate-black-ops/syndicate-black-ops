//========= SBO VGUI2 controls: values =========//
//
// Purpose: see sbo_value_controls.h.
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include <KeyValues.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "../client_viewport.h"
#include "../hud_chat_color_picker.h" // the color wheel
#include "../sbo_ui_style.h"
#include "sbo_value_controls.h"

#ifdef PostMessage
#undef PostMessage
#endif

using namespace sbolook;

//-----------------------------------------------------------------------------
// SboValueRange
//-----------------------------------------------------------------------------
float SboValueRange::Fix(float v) const
{
	if (!(v == v)) // NaN
		v = min;
	if (step > 0.0f)
		v = min + floorf((v - min) / step + 0.5f) * step;
	return std::max(min, std::min(max, v));
}

void SboValueRange::Format(float v, char *out, int outSize) const
{
	snprintf(out, outSize, "%.*f", std::max(0, std::min(4, decimals)), v);
	// "-0" reads as a minus sign on a zero
	if (!strcmp(out, "-0"))
		snprintf(out, outSize, "0");
}

namespace
{
CSboToolWindow *ToolWindowOf(vgui2::Panel *p)
{
	for (p = p ? p->GetParent() : nullptr; p; p = p->GetParent())
		if (CSboToolWindow *w = dynamic_cast<CSboToolWindow *>(p))
			return w;
	return nullptr;
}
} // namespace

//-----------------------------------------------------------------------------
// CSboTextField
//-----------------------------------------------------------------------------
CSboTextField::CSboTextField(vgui2::Panel *pParent, const char *pName, const char *placeholder)
    : BaseClass(pParent, pName, placeholder)
{
	SetKeyBoardInputEnabled(true);
}

void CSboTextField::SetValue(const char *text)
{
	m_Value = text ? text : "";
	if (!m_bEditing)
		SetText(m_Value.c_str());
}

void CSboTextField::StartEditing()
{
	if (m_bEditing)
		return;
	m_bEditing = true;
	if (CSboToolWindow *w = ToolWindowOf(this))
		w->BeginTextInput(this);
}

void CSboTextField::OnMousePressed(vgui2::MouseCode code)
{
	StartEditing();
	BaseClass::OnMousePressed(code);
}

void CSboTextField::Commit(bool enter)
{
	if (!m_bEditing)
		return;
	m_bEditing = false;
	char text[256];
	GetText(text, sizeof(text));
	m_Value = text;
	KeyValues *kv = new KeyValues("TextEntered", "text", text);
	kv->SetInt("enter", enter ? 1 : 0);
	PostActionSignal(kv);
}

void CSboTextField::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ENTER || code == vgui2::KEY_PAD_ENTER)
	{
		Commit(true);
		if (CSboToolWindow *w = ToolWindowOf(this))
			w->EndTextInput();
		return;
	}
	if (code == vgui2::KEY_ESCAPE)
	{
		m_bEditing = false;
		SetText(m_Value.c_str());
		if (CSboToolWindow *w = ToolWindowOf(this))
			w->EndTextInput();
		return;
	}
	BaseClass::OnKeyCodeTyped(code);
}

void CSboTextField::OnKillFocus()
{
	Commit(false);
	BaseClass::OnKillFocus();
}

//-----------------------------------------------------------------------------
// CSboNumberEntry
//-----------------------------------------------------------------------------
CSboNumberEntry::CSboNumberEntry(vgui2::Panel *pParent, const char *pName)
    : BaseClass(pParent, pName, "")
{
	SetAllowNumericInputOnly(false); // the minus sign and the decimal point
	SetMaximumCharCount(10);
	SetKeyBoardInputEnabled(true);
}

CSboToolWindow *CSboNumberEntry::Window()
{
	return ToolWindowOf(this);
}

void CSboNumberEntry::SetValue(float value)
{
	m_flValue = m_Range.Fix(value);
	if (m_bEditing)
		return; // not over what is being typed
	char text[32];
	m_Range.Format(m_flValue, text, sizeof(text));
	SetText(text);
}

void CSboNumberEntry::OnMousePressed(vgui2::MouseCode code)
{
	if (!m_bEditing)
	{
		m_bEditing = true;
		if (CSboToolWindow *w = Window())
			w->BeginTextInput(this);
		SelectAllOnFocusAlways(true);
	}
	BaseClass::OnMousePressed(code);
}

void CSboNumberEntry::Commit()
{
	if (!m_bEditing)
		return;
	m_bEditing = false;
	char text[32];
	GetText(text, sizeof(text));
	char *end = nullptr;
	const float v = strtof(text, &end);
	const bool number = end && end != text && !*end;
	if (number)
		m_flValue = m_Range.Fix(v);
	SetValue(m_flValue);
	if (number)
	{
		KeyValues *kv = new KeyValues("NumberEntered");
		kv->SetFloat("value", m_flValue);
		PostActionSignal(kv);
	}
}

void CSboNumberEntry::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ENTER || code == vgui2::KEY_PAD_ENTER)
	{
		Commit();
		if (CSboToolWindow *w = Window())
			w->EndTextInput();
		return;
	}
	if (code == vgui2::KEY_ESCAPE)
	{
		m_bEditing = false;
		SetValue(m_flValue); // the text as it was
		if (CSboToolWindow *w = Window())
			w->EndTextInput();
		return;
	}
	BaseClass::OnKeyCodeTyped(code);
}

void CSboNumberEntry::OnKillFocus()
{
	Commit();
	BaseClass::OnKillFocus();
}

//-----------------------------------------------------------------------------
// CSboTickSlider
//-----------------------------------------------------------------------------
CSboTickSlider::CSboTickSlider(vgui2::Panel *pParent, const char *pName)
    : BaseClass(pParent, pName)
{
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(false);
}

void CSboTickSlider::SetRange(const SboValueRange &range)
{
	m_Range = range;
	if (m_Range.max <= m_Range.min)
		m_Range.max = m_Range.min + 1.0f;
	m_flValue = m_Range.Fix(m_flValue);
	Repaint();
}

void CSboTickSlider::SetValue(float value)
{
	m_flValue = m_Range.Fix(value);
	Repaint();
}

int CSboTickSlider::TrackY()
{
	return S().M(7);
}

void CSboTickSlider::Change(float v)
{
	v = m_Range.Fix(v);
	if (v == m_flValue)
		return;
	m_flValue = v;
	Repaint();
	KeyValues *kv = new KeyValues("SliderMoved");
	kv->SetFloat("value", v);
	PostActionSignal(kv);
}

void CSboTickSlider::SetFromX(int x)
{
	const int knob = S().M(6);
	const int w = std::max(1, GetWide() - knob);
	const float f = std::max(0.0f, std::min(1.0f, (float)(x - knob / 2) / (float)w));
	Change(m_Range.min + f * (m_Range.max - m_Range.min));
}

void CSboTickSlider::OnMousePressed(vgui2::MouseCode code)
{
	if (!IsEnabled())
		return;
	if (code == vgui2::MOUSE_RIGHT)
	{
		// back to its default, posted like a drag (the number field and the owner follow), marked as a reset
		m_flValue = m_Range.Fix(GetDefaultValue());
		Repaint();
		KeyValues *kv = new KeyValues("SliderMoved");
		kv->SetFloat("value", m_flValue);
		kv->SetInt("reset", 1);
		PostActionSignal(kv);
		return;
	}
	if (code != vgui2::MOUSE_LEFT)
		return;
	m_bDragging = true;
	vgui2::input()->SetMouseCapture(GetVPanel());
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	SetFromX(x);
}

void CSboTickSlider::OnMouseReleased(vgui2::MouseCode code)
{
	if (m_bDragging)
	{
		m_bDragging = false;
		vgui2::input()->SetMouseCapture(0);
	}
}

void CSboTickSlider::OnCursorMoved(int x, int y)
{
	if (m_bDragging)
		SetFromX(x);
}

void CSboTickSlider::OnMouseWheeled(int delta)
{
	if (!IsEnabled())
		return;
	const float step = m_Range.step > 0.0f ? m_Range.step : (m_Range.max - m_Range.min) / 100.0f;
	Change(m_flValue + delta * step);
}

void CSboTickSlider::Paint()
{
	const int w = GetWide(), knob = S().M(6), track = std::max(2, S().M(3));
	const int ty = TrackY();
	const int x0 = knob / 2, x1 = w - knob / 2;
	const float f = (m_flValue - m_Range.min) / (m_Range.max - m_Range.min);
	const int kx = x0 + (int)(f * (x1 - x0));
	FillRect(x0, ty - track / 2, x1 - x0, track, COL_BG_INPUT);
	OutlineRect(x0, ty - track / 2, x1 - x0, track, COL_BORDER);
	// ticks under the track
	const int tickTop = ty + S().M(6), tickLen = std::max(2, S().M(3));
	if (m_iTicks > 0)
		for (int i = 0; i <= m_iTicks; i++)
		{
			const int tx = x0 + (x1 - x0) * i / m_iTicks;
			FillRect(tx, tickTop, 1, tickLen, COL_TEXT_FAINT);
		}
	// the knob
	const int kh = S().M(12);
	FillRect(kx - knob / 2, ty - kh / 2, knob, kh, IsEnabled() ? COL_BORDER_PRIMARY : COL_BORDER);
	OutlineRect(kx - knob / 2, ty - kh / 2, knob, kh, COL_BG_INPUT);
	// minimum and maximum under the ends
	if (m_bRangeLabels)
	{
		char lo[32], hi[32];
		m_Range.Format(m_Range.min, lo, sizeof(lo));
		m_Range.Format(m_Range.max, hi, sizeof(hi));
		const std::wstring wlo = Widen(lo), whi = Widen(hi);
		const int ly = tickTop + tickLen;
		DrawText(S().small, COL_TEXT_DIM, std::max(0, x0 - TextWide(S().small, wlo) / 2), ly, wlo);
		DrawText(S().small, COL_TEXT_DIM, std::min(w - TextWide(S().small, whi), x1 - TextWide(S().small, whi) / 2), ly,
		    whi);
	}
}

//-----------------------------------------------------------------------------
// CSboLabeledSlider
//-----------------------------------------------------------------------------
CSboLabeledSlider::CSboLabeledSlider(vgui2::Panel *pParent, const char *pName, const char *label,
    const SboValueRange &range)
    : BaseClass(pParent, pName)
    , m_Label(label ? label : "")
{
	m_pSlider = new CSboTickSlider(this, "Slider");
	m_pSlider->AddActionSignalTarget(this);
	m_pEntry = new CSboNumberEntry(this, "Number");
	m_pEntry->AddActionSignalTarget(this);
	SetRange(range);
}

void CSboLabeledSlider::SetLabel(const char *label)
{
	m_Label = label ? label : "";
	Repaint();
}

void CSboLabeledSlider::SetRange(const SboValueRange &range)
{
	m_Range = range;
	m_pSlider->SetRange(range);
	m_pEntry->SetRange(range);
	SetValue(m_flValue);
}

void CSboLabeledSlider::SetValue(float value)
{
	m_flValue = m_Range.Fix(value);
	m_pSlider->SetValue(m_flValue);
	m_pEntry->SetValue(m_flValue);
}

void CSboLabeledSlider::SetColumns(int labelWide, int entryWide)
{
	m_iLabelWide = labelWide;
	m_iEntryWide = entryWide;
	InvalidateLayout();
}

void CSboLabeledSlider::Changed(float v, bool reset)
{
	m_flValue = m_Range.Fix(v);
	m_pSlider->SetValue(m_flValue);
	m_pEntry->SetValue(m_flValue);
	KeyValues *kv = new KeyValues("ValueChanged");
	kv->SetFloat("value", m_flValue);
	if (reset)
		kv->SetInt("reset", 1);
	PostActionSignal(kv);
}

void CSboLabeledSlider::OnSliderMoved(KeyValues *data)
{
	Changed(data->GetFloat("value"), data->GetInt("reset") != 0);
}

void CSboLabeledSlider::OnNumberEntered(float value)
{
	Changed(value);
}

void CSboLabeledSlider::PerformLayout()
{
	BaseClass::PerformLayout();
	const int w = GetWide(), h = GetTall();
	const int labelW = m_Label.empty() ? 0 : S().M(m_iLabelWide);
	const int entryW = S().M(m_iEntryWide), gap = S().M(8);
	const int entryH = S().M(20);
	m_pEntry->SetBounds(w - entryW, 0, entryW, entryH);
	m_pSlider->SetBounds(labelW, 0, std::max(S().M(20), w - labelW - entryW - gap), h);
}

void CSboLabeledSlider::Paint()
{
	if (!m_Label.empty())
		DrawTextMid(S().body, COL_TEXT_DIM, 0, 0, S().M(16), Widen(m_Label.c_str()), S().M(m_iLabelWide) - S().M(4));
}

//-----------------------------------------------------------------------------
// The color window of the swatches: the chat's wheel and brightness bar as a plain color picker, and an alpha slider
// under them for a color with alpha. One window for colors with alpha and one for colors without; opening it for
// another swatch leaves the color of the first as it is.
//-----------------------------------------------------------------------------
class CSboColorWindow : public CSboToolWindow, public IChatColorPickerHost
{
	DECLARE_CLASS_SIMPLE(CSboColorWindow, CSboToolWindow);

public:
	//! The window for colors with alpha (true) or without
	static CSboColorWindow *Get(bool alpha)
	{
		static CSboColorWindow *s_pWindows[2] = {};
		CSboColorWindow *&w = s_pWindows[alpha ? 1 : 0];
		if (!w)
			w = new CSboColorWindow(alpha);
		return w;
	}

	CSboColorSwatch *Swatch() const { return m_pSwatch; }

	void OpenFor(CSboColorSwatch *pSwatch, const char *title)
	{
		if (m_pSwatch && m_pSwatch != pSwatch)
			m_pSwatch->PickerDetached();
		m_pSwatch = pSwatch;
		SetToolTitle(title);
		CHudChatColorPicker::PrecacheTextures();
		Open();
		// beside the swatch (right of it, else left), so the window covers neither the swatch nor its fields
		int sx = 0, sy = 0, scrW, scrH;
		pSwatch->LocalToScreen(sx, sy);
		vgui2::surface()->GetScreenSize(scrW, scrH);
		const int w = GetWide(), h = GetTall(), gap = M(8);
		int x = sx + pSwatch->GetWide() + gap;
		if (x + w > scrW)
			x = sx - w - gap;
		SetPos(std::max(0, std::min(x, scrW - w)), std::max(0, std::min(sy, scrH - h)));
		m_pPicker->SetVisible(true);
		m_pPicker->SetMouseInputEnabled(true);
		int r, g, b;
		pSwatch->GetColor(r, g, b);
		m_Start = Color(r, g, b, 255);
		m_flStartAlpha = pSwatch->GetAlpha();
		Show(m_Start, m_flStartAlpha);
		MoveToFront();
	}

	//! The swatch's color changed outside the window (its number fields): the wheel and the slider follow
	void Show(Color c, float a)
	{
		m_Color = c;
		m_flAlpha = a;
		m_pPicker->SetColor(ChatColor::FromRGB(c));
		if (m_pAlpha)
			m_pAlpha->SetValue(a);
		Repaint();
	}

	void Detach(CSboColorSwatch *pSwatch)
	{
		if (m_pSwatch != pSwatch)
			return;
		m_pSwatch = nullptr;
		CloseTool();
	}

	// IChatColorPickerHost
	bool IsPickerActive() override { return IsOpen() && m_pSwatch; }
	ChatColor GetPickerColor() override { return ChatColor::FromRGB(m_Color); }
	void SetPickerColor(const ChatColor &color) override { Changed(color.GetRGB(), m_flAlpha); }
	void SetPickerPreview(const ChatColor &color) override { Changed(color.GetRGB(), m_flAlpha); }
	void ClearPickerPreview() override {}
	void ClosePicker() override {}
	Color GetPickerTextColor() override { return COL_TEXT; }
	bool IsPlainColorPicker() override { return true; }

	MESSAGE_FUNC_PARAMS(OnValueChanged, "ValueChanged", data)
	{
		if (m_pAlpha)
			Changed(m_Color, m_pAlpha->GetValue());
	}

protected:
	void LayoutContent(int x, int y, int w, int h) override
	{
		m_pPicker->SetMetrics(M(20), M(400));
		const int pw = m_pPicker->GetWide();
		m_pPicker->SetPos(x + std::max(0, (w - pw) / 2), y);
		if (m_pAlpha)
			m_pAlpha->SetBounds(x, y + m_pPicker->GetTall() + M(8), w, M(CSboLabeledSlider::DesignTall()));
	}
	void OnEscape() override
	{
		// back to the color it had when the window opened
		if (m_pSwatch)
			m_pSwatch->PickerChanged(m_Start.r(), m_Start.g(), m_Start.b(), m_flStartAlpha, true);
		CloseTool();
	}
	void OnToolClosed() override
	{
		m_pPicker->CancelDrag();
		if (m_pSwatch)
			m_pSwatch->PickerDetached();
		m_pSwatch = nullptr;
	}

private:
	CSboColorWindow(bool alpha)
	    : BaseClass(g_pViewport, "SboColorWindow", "Color", alpha ? "color_window_alpha" : "color_window", 200,
	          alpha ? 236 : 194, 0.4f, 0.4f)
	{
		SetToolSizeable(false);
		SetEscapeCloses(true);
		m_pPicker = new CHudChatColorPicker(this, "ColorWheel", this);
		SetToolTip(m_pPicker, "Drag in the wheel to choose a color and on the bar to set how bright it is, or press Esc to close without changing it");
		if (alpha)
		{
			SboValueRange range;
			range.min = 0.0f;
			range.max = 1.0f;
			range.step = 0.01f;
			range.decimals = 2;
			m_pAlpha = new CSboLabeledSlider(this, "Alpha", "Alpha", range);
			m_pAlpha->SetDefaultValue(1.0f);
			m_pAlpha->SetColumns(44, 40);
			m_pAlpha->AddActionSignalTarget(this);
			SetToolTip(m_pAlpha, "How solid the color is, from 0 for invisible to 1 for solid");
		}
	}

	void Changed(Color c, float a)
	{
		m_Color = c;
		m_flAlpha = a;
		Repaint();
		if (m_pSwatch)
			m_pSwatch->PickerChanged(c.r(), c.g(), c.b(), a, false);
	}

	CSboColorSwatch *m_pSwatch = nullptr;
	CHudChatColorPicker *m_pPicker = nullptr;
	CSboLabeledSlider *m_pAlpha = nullptr;
	Color m_Color = Color(255, 255, 255, 255);
	float m_flAlpha = 1.0f;
	Color m_Start = Color(255, 255, 255, 255); // when the window opened (Esc puts it back)
	float m_flStartAlpha = 1.0f;
};

//-----------------------------------------------------------------------------
// CSboColorSwatch
//-----------------------------------------------------------------------------
CSboColorSwatch::CSboColorSwatch(vgui2::Panel *pParent, const char *pName, const char *title)
    : BaseClass(pParent, pName)
    , m_Title(title ? title : "Color")
{
	SetMouseInputEnabled(true);
}

CSboColorSwatch::~CSboColorSwatch()
{
	if (m_pWindow)
		m_pWindow->Detach(this);
}

void CSboColorSwatch::SetHasAlpha(bool hasAlpha)
{
	if (hasAlpha == m_bHasAlpha)
		return;
	ClosePicker();
	m_bHasAlpha = hasAlpha;
	Repaint();
}

void CSboColorSwatch::SetColor(int r, int g, int b)
{
	r = std::max(0, std::min(255, r));
	g = std::max(0, std::min(255, g));
	b = std::max(0, std::min(255, b));
	if (r == m_Color[0] && g == m_Color[1] && b == m_Color[2])
		return; // the wheel keeps its place (a gray has no hue of its own)
	m_Color[0] = r;
	m_Color[1] = g;
	m_Color[2] = b;
	if (m_pWindow)
		m_pWindow->Show(Color(r, g, b, 255), m_flAlpha);
	Repaint();
}

void CSboColorSwatch::GetColor(int &r, int &g, int &b) const
{
	r = m_Color[0];
	g = m_Color[1];
	b = m_Color[2];
}

void CSboColorSwatch::SetAlpha(float a)
{
	a = std::max(0.0f, std::min(1.0f, a));
	if (a == m_flAlpha)
		return;
	m_flAlpha = a;
	if (m_pWindow)
		m_pWindow->Show(Color(m_Color[0], m_Color[1], m_Color[2], 255), a);
	Repaint();
}

void CSboColorSwatch::ClosePicker()
{
	if (m_pWindow)
		m_pWindow->Detach(this);
	m_pWindow = nullptr;
	Repaint();
}

bool CSboColorSwatch::IsPickerOpen() const
{
	return m_pWindow != nullptr;
}

void CSboColorSwatch::PickerChanged(int r, int g, int b, float a, bool cancel)
{
	m_Color[0] = r;
	m_Color[1] = g;
	m_Color[2] = b;
	m_flAlpha = a;
	Repaint();
	KeyValues *kv = new KeyValues("ColorChanged", "r", r, "g", g);
	kv->SetInt("b", b);
	kv->SetFloat("a", a);
	if (cancel)
		kv->SetInt("cancel", 1);
	PostActionSignal(kv);
}

void CSboColorSwatch::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT || !IsEnabled())
		return;
	if (m_pWindow)
	{
		ClosePicker(); // a second click closes it, keeping the color
		return;
	}
	m_pWindow = CSboColorWindow::Get(m_bHasAlpha);
	m_pWindow->OpenFor(this, m_Title.c_str());
	PostActionSignal(new KeyValues("ColorPickerOpened"));
	Repaint();
}

void CSboColorSwatch::Paint()
{
	const int w = GetWide(), h = GetTall();
	if (m_bHasAlpha && m_flAlpha < 1.0f)
	{
		// a checker board under the color shows how see-through it is
		const int cell = std::max(3, S().M(5));
		for (int y = 0; y < h; y += cell)
			for (int x = 0; x < w; x += cell)
				FillRect(x, y, std::min(cell, w - x), std::min(cell, h - y),
				    ((x / cell + y / cell) & 1) ? Color(150, 150, 150, 255) : Color(90, 90, 90, 255));
	}
	FillRect(0, 0, w, h, Color(m_Color[0], m_Color[1], m_Color[2], m_bHasAlpha ? (int)(m_flAlpha * 255.0f + 0.5f) : 255));
	OutlineRect(0, 0, w, h, (IsCursorOver() || m_pWindow) ? COL_TEXT_BRIGHT : COL_BORDER);
}
