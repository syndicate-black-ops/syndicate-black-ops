//========= SBO VGUI2 port =========//
//
// Purpose: The chat input's color picker (see hud_chat_color_picker.h). The wheel is ported from
//          BugfixedHL-Rebased's color picker (gameui/options/color_picker.cpp, options/colorpicker/).
//
// SDK world: compiled with hl1_source_sdk include paths.
//
//==================================//

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <vector>

#include <tier1/strtools.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include <KeyValues.h>

#include "client_vgui.h"
#include "hud_chat.h"
#include "hud_chat_color_picker.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//-----------------------------------------------------------------------------
// BHL color_picker.cpp
//-----------------------------------------------------------------------------
void colorpicker::HSVtoRGB(float h, float s, float v, Color &output)
{
	Assert(h >= 0.f && h < 360.f);
	Assert(s >= 0.f && s <= 100.f);
	Assert(v >= 0.f && v <= 100.f);

	int i;
	float f, p, q, t;
	unsigned char r, g, b;

	s /= 100;
	v /= 100;

	if (s == 0)
	{
		// Achromatic (gray)
		output[0] = output[1] = output[2] = (unsigned char)round(v * 255);
		output[3] = 255;
		return;
	}

	h /= 60.0;
	i = (int)floor(h); // sector 0 to 5
	f = h - (float)i; // factorial part of h
	p = (float)(v * (1.0 - s));
	q = (float)(v * (1.0 - s * f));
	t = (float)(v * (1.0 - s * (1 - f)));
	switch (i)
	{
	case 0:
		r = (unsigned char)round(255 * v);
		g = (unsigned char)round(255 * t);
		b = (unsigned char)round(255 * p);
		break;
	case 1:
		r = (unsigned char)round(255 * q);
		g = (unsigned char)round(255 * v);
		b = (unsigned char)round(255 * p);
		break;
	case 2:
		r = (unsigned char)round(255 * p);
		g = (unsigned char)round(255 * v);
		b = (unsigned char)round(255 * t);
		break;
	case 3:
		r = (unsigned char)round(255 * p);
		g = (unsigned char)round(255 * q);
		b = (unsigned char)round(255 * v);
		break;
	case 4:
		r = (unsigned char)round(255 * t);
		g = (unsigned char)round(255 * p);
		b = (unsigned char)round(255 * v);
		break;
	default: // case 5:
		r = (unsigned char)round(255 * v);
		g = (unsigned char)round(255 * p);
		b = (unsigned char)round(255 * q);
	}

	output[0] = r;
	output[1] = g;
	output[2] = b;
	output[3] = 255;
}

void colorpicker::RGBtoHSV(Color rgb, float &hue, float &sat, float &val)
{
	double delta, min;
	double h = 0, s, v;

	min = (std::min)((std::min)(rgb.r(), rgb.g()), rgb.b());
	v = (std::max)((std::max)(rgb.r(), rgb.g()), rgb.b());
	delta = v - min;

	if (v == 0.0)
		s = 0;
	else
		s = delta / v;

	if (s == 0)
		h = 0.0;

	else
	{
		if (rgb.r() == v)
			h = (rgb.g() - rgb.b()) / delta;
		else if (rgb.g() == v)
			h = 2 + (rgb.b() - rgb.r()) / delta;
		else if (rgb.b() == v)
			h = 4 + (rgb.r() - rgb.g()) / delta;

		h *= 60;

		if (h < 0.0)
			h = h + 360;
	}

	hue = h;
	sat = s * 100;
	val = v / 255 * 100;
}

namespace colorpicker
{
constexpr int PICKER_PADDING = 4;
constexpr int PICKER_GAP = 4;
constexpr int SWATCH_GAP = 2;

//! value (0..100) of a brightness level
inline float LevelToValue(int level)
{
	return 100.f * (float)level / (float)MAX_LEVEL;
}

inline float ClampHue(float hue)
{
	// HSVtoRGB wants [0, 360)
	if (!(hue >= 0.f) || hue >= 360.f)
		return 0.f;
	return hue;
}

//----------------------------------------------------------
// colorpicker::CTextureManager
// BHL (texture_manager.cpp) builds its picker textures (one saturation / value square per hue) on a worker thread
// and uploads them with ISurface::DrawSetTextureRGBA. SBO's are small enough to make on first use: one hue /
// saturation wheel per brightness level, plus the round masks of the swatches and the selection marker (BHL:
// ui/gfx/circle_bg.tga, circle_fg.tga). Every texture is uploaded once and kept; sizes are powers of two.
//----------------------------------------------------------
class CTextureManager
{
public:
	static constexpr int WHEEL_SIZE = 128;
	static constexpr int PIXEL_SIZE = 4;

	//! the wheel at a brightness level: hue = angle (0 = right, counter-clockwise), saturation = radius
	int GetWheelTexture(int level)
	{
		if (level < 0 || level > MAX_LEVEL)
			level = MAX_LEVEL;
		return MakeWheel(m_iWheelTextures[level], level, true);
	}

	//! the plain picker's wheel: full brightness, no floor; drawn in the grey of the brightness (RGB scales with the
	//! HSV value, so that is exactly the wheel at that value)
	int GetPlainWheelTexture()
	{
		return MakeWheel(m_iPlainWheelTexture, MAX_LEVEL, false);
	}

private:
	int MakeWheel(int &texture, int level, bool bFloor)
	{
		if (texture == -1)
		{
			std::vector<unsigned char> data(WHEEL_SIZE * WHEEL_SIZE * PIXEL_SIZE);
			const float center = WHEEL_SIZE / 2.0f;
			const float radius = center - 0.5f;

			for (int y = 0; y < WHEEL_SIZE; y++)
			{
				for (int x = 0; x < WHEEL_SIZE; x++)
				{
					const float dx = x + 0.5f - center;
					const float dy = y + 0.5f - center;
					const float dist = sqrtf(dx * dx + dy * dy);

					float hue = atan2f(-dy, dx) * (float)(180.0 / M_PI);
					if (hue < 0.f)
						hue += 360.f;
					float sat = 100.f * dist / radius;
					if (sat > 100.f)
						sat = 100.f; // outside the disc: the rim's color, so filtering has no dark fringe

					// as the chat will draw it: no darker than the brightness floor
					Color rgb;
					HSVtoRGB(ClampHue(hue), sat, LevelToValue(level), rgb);
					if (bFloor)
						rgb = ClampColorCodeBrightness(rgb);

					float alpha = radius - dist + 0.5f; // one soft pixel at the rim
					alpha = alpha < 0.f ? 0.f : (alpha > 1.f ? 1.f : alpha);

					unsigned char *p = &data[(y * WHEEL_SIZE + x) * PIXEL_SIZE];
					p[0] = (unsigned char)rgb.r();
					p[1] = (unsigned char)rgb.g();
					p[2] = (unsigned char)rgb.b();
					p[3] = (unsigned char)(alpha * 255.f + 0.5f);
				}
			}

			texture = vgui2::surface()->CreateNewTextureID(true);
			vgui2::surface()->DrawSetTextureRGBA(texture, data.data(), WHEEL_SIZE, WHEEL_SIZE, true, false);
		}

		return texture;
	}

public:

	//! a white disc (bRing: a ring an eighth of the size thick) for a circle drawn size pixels wide
	int GetCircleTexture(int size, bool bRing)
	{
		int idx = 0;
		while (idx < NUM_CIRCLE_SIZES - 1 && (MIN_CIRCLE_SIZE << idx) < size)
			idx++;
		return GetCircleTextureByIndex(idx, bRing);
	}

	//! makes the round masks (all sizes), so drawing never has to
	void PrecacheCircles()
	{
		for (int i = 0; i < NUM_CIRCLE_SIZES; i++)
		{
			GetCircleTextureByIndex(i, false);
			GetCircleTextureByIndex(i, true);
		}
	}

private:
	int GetCircleTextureByIndex(int idx, bool bRing)
	{

		int &texture = m_iCircleTextures[bRing ? 1 : 0][idx];
		if (texture == -1)
		{
			const int n = MIN_CIRCLE_SIZE << idx;
			std::vector<unsigned char> data(n * n * PIXEL_SIZE);
			const float center = n / 2.0f;
			const float radius = center - 0.5f;
			const float inner = radius - n / 8.0f;

			for (int y = 0; y < n; y++)
			{
				for (int x = 0; x < n; x++)
				{
					const float dx = x + 0.5f - center;
					const float dy = y + 0.5f - center;
					const float dist = sqrtf(dx * dx + dy * dy);

					float alpha = radius - dist + 0.5f;
					alpha = alpha < 0.f ? 0.f : (alpha > 1.f ? 1.f : alpha);
					if (bRing)
					{
						float in = dist - inner + 0.5f;
						in = in < 0.f ? 0.f : (in > 1.f ? 1.f : in);
						alpha *= in;
					}

					unsigned char *p = &data[(y * n + x) * PIXEL_SIZE];
					p[0] = p[1] = p[2] = 255;
					p[3] = (unsigned char)(alpha * 255.f + 0.5f);
				}
			}

			texture = vgui2::surface()->CreateNewTextureID(true);
			vgui2::surface()->DrawSetTextureRGBA(texture, data.data(), n, n, true, false);
		}

		return texture;
	}

	static constexpr int MIN_CIRCLE_SIZE = 16;
	static constexpr int NUM_CIRCLE_SIZES = 4; // 16, 32, 64, 128

	int m_iWheelTextures[MAX_LEVEL + 1] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	int m_iPlainWheelTexture = -1;
	int m_iCircleTextures[2][NUM_CIRCLE_SIZES] = { { -1, -1, -1, -1 }, { -1, -1, -1, -1 } };
};

CTextureManager gTexMgr;

void DrawCircle(int x, int y, int size, Color color, bool bRing)
{
	if (size <= 0)
		return;
	vgui2::surface()->DrawSetTexture(gTexMgr.GetCircleTexture(size, bRing));
	vgui2::surface()->DrawSetColor(color);
	vgui2::surface()->DrawTexturedRect(x, y, x + size, y + size);
}

} // namespace colorpicker

// The swatch of a picker color in (x, y, size, size): a disc in the color inside a dark rim (white and thicker when
// bSelected); "none" is a hollow ring in noneColor (the chat: its default text color).
void DrawChatColorSwatch(Color noneColor, const ChatColor &color, int x, int y, int size, bool bSelected)
{
	using namespace colorpicker;
	const int rim = bSelected ? 2 : 1;
	DrawCircle(x, y, size, bSelected ? Color(255, 255, 255, 255) : Color(0, 0, 0, 160), false);

	if (color.IsNone())
	{
		if (bSelected)
			DrawCircle(x + rim, y + rim, size - 2 * rim, Color(0, 0, 0, 255), false);
		DrawCircle(x + rim, y + rim, size - 2 * rim, noneColor, true);
	}
	else
	{
		DrawCircle(x + rim, y + rim, size - 2 * rim, CHudChat::GetChatColorDisplay(color), false);
	}
}

void GetChatColorDisplayName(const ChatColor &color, wchar_t *buf, int bufSizeBytes)
{
	if (CHudChat *pChat = CHudChat::Get())
	{
		pChat->GetChatColorName(color, buf, bufSizeBytes);
		return;
	}
	char name[COLOR_CODE_MAX_LEN + 1];
	WriteColorCode(color, name);
	V_snwprintf(buf, bufSizeBytes / (int)sizeof(wchar_t), L"%hs", color.IsNone() ? "None (default color)" : name);
}

namespace colorpicker
{
void DrawChatColorSwatch(CHudChat *pChat, const ChatColor &color, int x, int y, int size, bool bSelected)
{
	::DrawChatColorSwatch(pChat->GetDefaultTextColor(), color, x, y, size, bSelected);
}

// BHL sel_circle_panel.cpp: the selection marker, a ring over a darker one (BHL: circle_fg over circle_bg)
void DrawSelectionMarker(int cx, int cy, int size)
{
	DrawCircle(cx - size / 2 - 1, cy - size / 2 - 1, size + 2, Color(0, 0, 0, 200), true);
	DrawCircle(cx - size / 2, cy - size / 2, size, Color(255, 255, 255, 255), true);
}

// Width of the arrow next to the collapsed swatch (odd, so the tip is one pixel)
int GetColorArrowWide(int swatch)
{
	int w = swatch / 2;
	if (w < 5)
		w = 5;
	return w | 1;
}

void ReturnFocusToChatInput(CHudChat *pChat)
{
	// keep typing in the input line (TextEntry keeps its caret when it gets the focus back)
	if (pChat && pChat->GetInputPanel())
		pChat->GetInputPanel()->RequestFocus();
}

CHudChatColorPicker *GetPicker(vgui2::Panel *pPanel)
{
	return static_cast<CHudChatColorPicker *>(pPanel->GetParent());
}

IChatColorPickerHost *GetHost(vgui2::Panel *pPanel)
{
	return GetPicker(pPanel)->Host();
}

// The chat's input line as the picker's host: the "Chat" / "Name" switch edits hud_chat_color / namecolor
class CChatPickerHost : public IChatColorPickerHost
{
public:
	bool IsPickerActive() override
	{
		CHudChat *pChat = CHudChat::Get();
		return pChat && pChat->GetMessageMode() != MM_NONE;
	}
	ChatColor GetPickerColor() override
	{
		CHudChat *pChat = CHudChat::Get();
		return pChat ? pChat->GetPickerColor() : ChatColor();
	}
	void SetPickerColor(const ChatColor &color) override
	{
		if (CHudChat *pChat = CHudChat::Get())
			pChat->SetPickerColor(color);
	}
	void SetPickerPreview(const ChatColor &color) override
	{
		if (CHudChat *pChat = CHudChat::Get())
			pChat->SetChatColorPreview(color);
	}
	void ClearPickerPreview() override
	{
		if (CHudChat *pChat = CHudChat::Get())
			pChat->ClearChatColorPreview();
	}
	void ClosePicker() override
	{
		if (CHudChat *pChat = CHudChat::Get())
			pChat->CloseColorPicker();
	}
	void OnPickerInput() override { ReturnFocusToChatInput(CHudChat::Get()); }
	Color GetPickerTextColor() override
	{
		CHudChat *pChat = CHudChat::Get();
		return pChat ? pChat->GetDefaultTextColor() : Color(255, 255, 255, 255);
	}
	void ShowPickerTooltip(vgui2::Panel *pOwner, const wchar_t *text) override
	{
		if (CHudChat *pChat = CHudChat::Get())
			pChat->ShowColorTooltip(pOwner, text);
	}
	void HidePickerTooltip(vgui2::Panel *pOwner) override
	{
		if (CHudChat *pChat = CHudChat::Get())
			pChat->HideColorTooltip(pOwner);
	}
	bool HasPickerTargets() override { return true; }
	ChatColorTarget GetPickerTarget() override
	{
		CHudChat *pChat = CHudChat::Get();
		return pChat ? pChat->GetColorPickerTarget() : ChatColorTarget::Chat;
	}
	void SetPickerTarget(ChatColorTarget target) override
	{
		if (CHudChat *pChat = CHudChat::Get())
			pChat->SetColorPickerTarget(target);
	}
};
CChatPickerHost s_ChatPickerHost;

//----------------------------------------------------------
// colorpicker::CDragPanel
// BHL's CPickerMousePanel / CBarMousePanel: the left button pressed and moved over the panel picks. SBO: the panel
// keeps the mouse while the button is down (BHL stops at the panel's edge) and releasing it selects.
//----------------------------------------------------------
class CDragPanel : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CDragPanel(vgui2::Panel *parent, const char *panelName)
	    : BaseClass(parent, panelName)
	{
		SetPaintBackgroundEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(true); // gated by the popup (only takes the mouse while open)
	}

	bool IsDragging() const { return m_bIsMousePressed; }

	void CancelDrag()
	{
		if (!m_bIsMousePressed)
			return;
		m_bIsMousePressed = false;
		vgui2::input()->SetMouseCapture(NULL_HANDLE);
	}

	virtual void OnMousePressed(vgui2::MouseCode code)
	{
		IChatColorPickerHost *pHost = GetHost(this);
		if (!pHost->IsPickerActive())
			return;

		if (code == vgui2::MOUSE_LEFT)
		{
			int x, y;
			vgui2::input()->GetCursorPos(x, y);
			ScreenToLocal(x, y);
			if (HitTest(x, y))
			{
				m_bIsMousePressed = true;
				vgui2::input()->SetMouseCapture(GetVPanel());
				Pick(x, y);
				GetPicker(this)->OnPicked(this, false);
			}
		}

		pHost->OnPickerInput();
	}

	virtual void OnMouseDoublePressed(vgui2::MouseCode code)
	{
		OnMousePressed(code);
	}

	virtual void OnCursorMoved(int x, int y)
	{
		BaseClass::OnCursorMoved(x, y);
		if (!m_bIsMousePressed)
			return;
		Pick(x, y);
		GetPicker(this)->OnPicked(this, false);
	}

	virtual void OnMouseReleased(vgui2::MouseCode code)
	{
		if (code != vgui2::MOUSE_LEFT || !m_bIsMousePressed)
			return;

		m_bIsMousePressed = false;
		vgui2::input()->SetMouseCapture(NULL_HANDLE);
		GetPicker(this)->OnPicked(this, true);
		GetHost(this)->OnPickerInput();
	}

	virtual void OnMouseCaptureLost()
	{
		BaseClass::OnMouseCaptureLost();

		// taken away without a release: nothing is selected
		if (m_bIsMousePressed)
		{
			m_bIsMousePressed = false;
			GetHost(this)->ClearPickerPreview();
		}
	}

protected:
	//! true if a press at (x, y) starts picking
	virtual bool HitTest(int x, int y) = 0;
	//! moves the selection to (x, y), which may be outside the panel while dragging
	virtual void Pick(int x, int y) = 0;

private:
	bool m_bIsMousePressed = false;
};

//----------------------------------------------------------
// colorpicker::CWheelPanel
// BHL's CPickerPanel (picker_panel.cpp), as a hue / saturation wheel
//----------------------------------------------------------
class CWheelPanel : public CDragPanel
{
	typedef CDragPanel BaseClass;

public:
	CWheelPanel(vgui2::Panel *parent, const char *panelName)
	    : BaseClass(parent, panelName)
	{
	}

	virtual void Paint()
	{
		CHudChatColorPicker *pPicker = GetPicker(this);
		const int size = GetWide() < GetTall() ? GetWide() : GetTall();

		if (pPicker->IsPlain())
		{
			const int grey = (int)(255.f * pPicker->GetLevel() / (float)PLAIN_MAX_LEVEL + 0.5f);
			vgui2::surface()->DrawSetTexture(gTexMgr.GetPlainWheelTexture());
			vgui2::surface()->DrawSetColor(Color(grey, grey, grey, 255));
		}
		else
		{
			vgui2::surface()->DrawSetTexture(gTexMgr.GetWheelTexture(pPicker->GetLevel()));
			vgui2::surface()->DrawSetColor(Color(255, 255, 255, 255));
		}
		vgui2::surface()->DrawTexturedRect(0, 0, size, size);

		// the marker, while a wheel color is selected (or being picked)
		if (pPicker->Host()->GetPickerColor().IsCustom())
		{
			const float radius = size / 2.0f - 0.5f;
			const float angle = pPicker->GetHue() * (float)(M_PI / 180.0);
			const float dist = radius * pPicker->GetSat() / 100.f;
			int marker = size / 12;
			if (marker < 7)
				marker = 7;
			DrawSelectionMarker((int)(size / 2.0f + cosf(angle) * dist), (int)(size / 2.0f - sinf(angle) * dist), marker | 1);
		}
	}

protected:
	virtual bool HitTest(int x, int y)
	{
		const float center = (GetWide() < GetTall() ? GetWide() : GetTall()) / 2.0f;
		const float dx = x - center, dy = y - center;
		return dx * dx + dy * dy <= (center + 2.f) * (center + 2.f);
	}

	virtual void Pick(int x, int y)
	{
		const float center = (GetWide() < GetTall() ? GetWide() : GetTall()) / 2.0f;
		const float radius = center - 0.5f;
		if (radius <= 0.f)
			return;

		const float dx = x - center, dy = y - center;
		float hue = atan2f(-dy, dx) * (float)(180.0 / M_PI);
		if (hue < 0.f)
			hue += 360.f;
		float sat = 100.f * sqrtf(dx * dx + dy * dy) / radius;
		if (sat > 100.f)
			sat = 100.f;
		GetPicker(this)->SetHueSat(hue, sat);
	}
};

//----------------------------------------------------------
// colorpicker::CBrightnessPanel
// BHL's CBarPanel (bar_panel.cpp), upright and for the brightness: one cell per level, the brightest on top, each
// in the color the wheel's selection has at that level
//----------------------------------------------------------
class CBrightnessPanel : public CDragPanel
{
	typedef CDragPanel BaseClass;

public:
	CBrightnessPanel(vgui2::Panel *parent, const char *panelName)
	    : BaseClass(parent, panelName)
	{
	}

	virtual void Paint()
	{
		CHudChatColorPicker *pPicker = GetPicker(this);
		int wide, tall;
		GetSize(wide, tall);

		for (int level = pPicker->GetMaxLevel(); level >= pPicker->GetMinLevel(); level--)
		{
			int y0, y1;
			GetCell(level, tall, y0, y1);
			vgui2::surface()->DrawSetColor(pPicker->GetDisplayColor(pPicker->MakePickerColor(pPicker->GetHue(), pPicker->GetSat(), level)));
			vgui2::surface()->DrawFilledRect(0, y0, wide, y1);
		}

		vgui2::surface()->DrawSetColor(Color(0, 0, 0, 160));
		vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);

		// the selected level (a plain picker's levels are thin: a marker a few pixels tall around it)
		int y0, y1;
		GetCell(pPicker->GetLevel(), tall, y0, y1);
		if (y1 - y0 < 5)
		{
			const int mid = (y0 + y1) / 2;
			y0 = std::max(0, mid - 2);
			y1 = std::min(tall, y0 + 5);
		}
		vgui2::surface()->DrawSetColor(Color(0, 0, 0, 255));
		vgui2::surface()->DrawOutlinedRect(0, y0, wide, y1);
		vgui2::surface()->DrawSetColor(Color(255, 255, 255, 255));
		vgui2::surface()->DrawOutlinedRect(1, y0 + 1, wide - 1, y1 - 1);
	}

protected:
	virtual bool HitTest(int x, int y)
	{
		return x >= 0 && y >= 0 && x < GetWide() && y < GetTall();
	}

	virtual void Pick(int x, int y)
	{
		const int tall = GetTall();
		if (tall <= 0)
			return;

		CHudChatColorPicker *pPicker = GetPicker(this);
		const int cells = pPicker->GetMaxLevel() - pPicker->GetMinLevel() + 1;
		int cell = y * cells / tall; // 0 = top = brightest
		if (y < 0)
			cell = 0;
		if (cell > cells - 1)
			cell = cells - 1;
		pPicker->SetLevel(pPicker->GetMaxLevel() - cell);
	}

private:
	void GetCell(int level, int tall, int &y0, int &y1)
	{
		CHudChatColorPicker *pPicker = GetPicker(this);
		const int cells = pPicker->GetMaxLevel() - pPicker->GetMinLevel() + 1;
		const int cell = pPicker->GetMaxLevel() - level;
		y0 = cell * tall / cells;
		y1 = (cell + 1) * tall / cells;
	}
};

//----------------------------------------------------------
// colorpicker::CSwatchPanel
// One of the quick picks under the wheel: "none" (0) or the ^1..^8 colors
//----------------------------------------------------------
class CSwatchPanel : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CSwatchPanel(vgui2::Panel *parent, const char *panelName, int code)
	    : BaseClass(parent, panelName)
	    , m_iCode(code)
	{
		SetPaintBackgroundEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(true); // gated by the popup (only takes the mouse while open)
	}

	virtual void Paint()
	{
		IChatColorPickerHost *pHost = GetHost(this);
		int wide, tall;
		GetSize(wide, tall);
		const int size = wide < tall ? wide : tall;

		// hover: a faint highlight behind the swatch
		if (IsCursorOver())
			DrawCircle(0, 0, size, Color(255, 255, 255, 96), false);

		::DrawChatColorSwatch(pHost->GetPickerTextColor(), ChatColor::FromCode(m_iCode), 1, 1, size - 2, pHost->GetPickerColor() == ChatColor::FromCode(m_iCode));
	}

	virtual void OnMousePressed(vgui2::MouseCode code)
	{
		IChatColorPickerHost *pHost = GetHost(this);
		if (!pHost->IsPickerActive())
			return;

		if (code == vgui2::MOUSE_LEFT)
		{
			pHost->SetPickerColor(ChatColor::FromCode(m_iCode));
			pHost->ClosePicker();
		}

		pHost->OnPickerInput();
	}

	virtual void OnMouseDoublePressed(vgui2::MouseCode code)
	{
		OnMousePressed(code);
	}

	virtual void OnCursorEntered()
	{
		BaseClass::OnCursorEntered();
		wchar_t text[64];
		GetChatColorDisplayName(ChatColor::FromCode(m_iCode), text, sizeof(text));
		GetHost(this)->ShowPickerTooltip(this, text);
	}

	virtual void OnCursorExited()
	{
		BaseClass::OnCursorExited();
		GetHost(this)->HidePickerTooltip(this);
	}

private:
	int m_iCode; //!< 0 = none, 1..8 = ^1..^8
};

//----------------------------------------------------------
// colorpicker::CTargetPanel
// One half of the "Chat" / "Name" switch above the wheel: the setting the wheel and the quick picks edit, the chat
// text color (hud_chat_color) or the player's name color (namecolor). The selected half is lit.
//----------------------------------------------------------
class CTargetPanel : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CTargetPanel(vgui2::Panel *parent, const char *panelName, ChatColorTarget target)
	    : BaseClass(parent, panelName)
	    , m_Target(target)
	{
		SetPaintBackgroundEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(true); // gated by the popup (only takes the mouse while open)
	}

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme)
	{
		BaseClass::ApplySchemeSettings(pScheme);
		m_hFont = pScheme->GetFont("ChatFont");
	}

	virtual void Paint()
	{
		IChatColorPickerHost *pHost = GetHost(this);
		if (!m_hFont)
			return;

		int wide, tall;
		GetSize(wide, tall);
		const bool bSelected = pHost->GetPickerTarget() == m_Target;

		if (bSelected || IsCursorOver())
		{
			vgui2::surface()->DrawSetColor(Color(255, 255, 255, bSelected ? 64 : 28));
			vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
		}
		vgui2::surface()->DrawSetColor(Color(255, 255, 255, bSelected ? 200 : 64));
		vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);

		const wchar_t *text = m_Target == ChatColorTarget::Name ? L"Name" : L"Chat";
		int textWide = 0, textTall = 0;
		vgui2::surface()->GetTextSize(m_hFont, text, textWide, textTall);

		Color textColor = pHost->GetPickerTextColor();
		textColor[3] = bSelected ? 255 : 150;
		vgui2::surface()->DrawSetTextFont(m_hFont);
		vgui2::surface()->DrawSetTextColor(textColor);
		vgui2::surface()->DrawSetTextPos((wide - textWide) / 2, (tall - textTall) / 2);
		vgui2::surface()->DrawPrintText(text, (int)wcslen(text));
	}

	virtual void OnMousePressed(vgui2::MouseCode code)
	{
		IChatColorPickerHost *pHost = GetHost(this);
		if (!pHost->IsPickerActive())
			return;

		if (code == vgui2::MOUSE_LEFT)
			pHost->SetPickerTarget(m_Target);

		pHost->OnPickerInput();
	}

	virtual void OnMouseDoublePressed(vgui2::MouseCode code)
	{
		OnMousePressed(code);
	}

	virtual void OnCursorEntered()
	{
		BaseClass::OnCursorEntered();
		GetHost(this)->ShowPickerTooltip(this, m_Target == ChatColorTarget::Name ? L"Color of your name" : L"Color of your chat text");
	}

	virtual void OnCursorExited()
	{
		BaseClass::OnCursorExited();
		GetHost(this)->HidePickerTooltip(this);
	}

private:
	ChatColorTarget m_Target;
	vgui2::HFont m_hFont = 0;
};
} // namespace colorpicker

using namespace colorpicker;

//-----------------------------------------------------------------------------
// Purpose: collapsed color picker
//-----------------------------------------------------------------------------
CHudChatColorButton::CHudChatColorButton(vgui2::Panel *parent, const char *panelName)
    : BaseClass(parent, panelName)
{
	SetPaintBackgroundEnabled(false);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
}

int CHudChatColorButton::GetWideForSwatch(int swatch)
{
	return swatch + 2 + GetColorArrowWide(swatch);
}

void CHudChatColorButton::Paint()
{
	CHudChat *pChat = CHudChat::Get();
	if (!pChat)
		return;

	int wide, tall;
	GetSize(wide, tall);
	const bool bOpen = pChat->IsColorPickerOpen();

	// hover (or popup open): a faint highlight behind circle and arrow
	if (bOpen || (IsMouseInputEnabled() && IsCursorOver()))
	{
		vgui2::surface()->DrawSetColor(Color(255, 255, 255, bOpen ? 56 : 36));
		vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
	}

	// the current color (the one being picked while the wheel is dragged)
	const int swatch = tall;
	DrawChatColorSwatch(pChat, pChat->GetActiveChatColor(), 0, 0, swatch, false);

	// the arrow: points down (opens the popup upwards), up while the popup is open; prompt color
	Color arrowColor = pChat->GetDefaultTextColor();
	if (CHudChatInputLine *pLine = pChat->GetChatInput())
		arrowColor = pLine->GetPrompt()->GetFgColor();
	vgui2::surface()->DrawSetColor(arrowColor);

	const int aw = GetColorArrowWide(swatch);
	const int ah = (aw + 1) / 2;
	const int ax = swatch + 1;
	const int ay = (tall - ah) / 2;
	for (int r = 0; r < ah; r++)
	{
		const int row = bOpen ? ah - 1 - r : r; // row 0 = widest
		vgui2::surface()->DrawFilledRect(ax + row, ay + r, ax + aw - row, ay + r + 1);
	}
}

void CHudChatColorButton::OnMousePressed(vgui2::MouseCode code)
{
	CHudChat *pChat = CHudChat::Get();
	if (!pChat || pChat->GetMessageMode() == MM_NONE)
		return;

	if (code == vgui2::MOUSE_LEFT)
		pChat->ToggleColorPicker();

	ReturnFocusToChatInput(pChat);
}

//-----------------------------------------------------------------------------
// Purpose: color picker popup
//-----------------------------------------------------------------------------
CHudChatColorPicker::CHudChatColorPicker(vgui2::Panel *parent, const char *panelName, IChatColorPickerHost *pHost)
    : BaseClass(parent, panelName)
    , m_pHost(pHost)
{
	SetPaintBackgroundEnabled(false);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetVisible(false);

	m_pWheel = new CWheelPanel(this, "ChatColorWheel");
	m_pBrightness = new CBrightnessPanel(this, "ChatColorBrightness");

	for (int i = 0; i < CHAT_COLOR_PRESETS; i++)
	{
		char name[32];
		snprintf(name, sizeof(name), "ChatColorSwatch%d", i);
		m_pSwatches[i] = new CSwatchPanel(this, name, i);
	}

	m_pTargets[0] = new CTargetPanel(this, "ChatColorTargetChat", ChatColorTarget::Chat);
	m_pTargets[1] = new CTargetPanel(this, "ChatColorTargetName", ChatColorTarget::Name);
}

IChatColorPickerHost *CHudChatColorPicker::Host() const
{
	return m_pHost ? m_pHost : &s_ChatPickerHost;
}

ChatColor CHudChatColorPicker::MakeColor(float hue, float sat, int level)
{
	Color rgb;
	HSVtoRGB(ClampHue(hue), sat < 0.f ? 0.f : (sat > 100.f ? 100.f : sat), LevelToValue(level < 0 ? 0 : (level > MAX_LEVEL ? MAX_LEVEL : level)), rgb);
	return ChatColor::FromRGB(ClampColorCodeBrightness(rgb));
}

ChatColor CHudChatColorPicker::MakePickerColor(float hue, float sat, int level) const
{
	if (!IsPlain())
		return MakeColor(hue, sat, level);
	level = level < 0 ? 0 : (level > PLAIN_MAX_LEVEL ? PLAIN_MAX_LEVEL : level);
	Color rgb;
	HSVtoRGB(ClampHue(hue), sat < 0.f ? 0.f : (sat > 100.f ? 100.f : sat), 100.f * level / (float)PLAIN_MAX_LEVEL, rgb);
	return ChatColor::FromRGB(rgb);
}

Color CHudChatColorPicker::GetDisplayColor(const ChatColor &color) const
{
	return IsPlain() ? color.GetRGB() : CHudChat::GetChatColorDisplay(color);
}

int CHudChatColorPicker::GetMinLevel() const
{
	return IsPlain() ? 0 : MIN_LEVEL;
}

int CHudChatColorPicker::GetMaxLevel() const
{
	return IsPlain() ? PLAIN_MAX_LEVEL : MAX_LEVEL;
}

void CHudChatColorPicker::SetMetrics(int cell, int maxTall)
{
	if (cell < 8)
		cell = 8;

	if (IsPlain())
	{
		// [ wheel ][ brightness bar ], nothing else
		int wheel = 7 * cell;
		const int room = maxTall - 2 * PICKER_PADDING;
		if (wheel > room)
			wheel = room;
		if (wheel < 3 * cell)
			wheel = 3 * cell;
		int barWide = cell * 3 / 4;
		if (barWide < 10)
			barWide = 10;
		m_iCell = cell;
		m_iWheelSize = wheel;
		m_iBarWide = barWide;
		m_iSwatchSize = 0;
		m_iTargetTall = 0;
		SetSize(2 * PICKER_PADDING + wheel + PICKER_GAP + barWide, 2 * PICKER_PADDING + wheel);
		InvalidateLayout();
		return;
	}

	// [ Chat ][ Name ]       (o)   the edited setting; the circle: the name color, while "Name" is selected
	// [ wheel ][ brightness bar ]
	// [ none + ^1..^8, one row  ]
	int wheel = 7 * cell;
	const int room = maxTall - 2 * PICKER_PADDING - 2 * PICKER_GAP - 2 * cell;
	if (wheel > room)
		wheel = room;
	if (wheel < 3 * cell)
		wheel = 3 * cell;

	int barWide = cell * 3 / 4;
	if (barWide < 10)
		barWide = 10;

	const int inner = wheel + PICKER_GAP + barWide;
	int swatch = (inner - (CHAT_COLOR_PRESETS - 1) * SWATCH_GAP) / CHAT_COLOR_PRESETS;
	if (swatch > cell + 2)
		swatch = cell + 2;
	if (swatch < 6)
		swatch = 6;

	if (cell == m_iCell && wheel == m_iWheelSize && GetWide() > 0)
		return;

	m_iCell = cell;
	m_iWheelSize = wheel;
	m_iBarWide = barWide;
	m_iSwatchSize = swatch;
	m_iTargetTall = cell;
	SetSize(2 * PICKER_PADDING + inner, 2 * PICKER_PADDING + m_iTargetTall + PICKER_GAP + wheel + PICKER_GAP + swatch);
	InvalidateLayout();
}

void CHudChatColorPicker::PerformLayout()
{
	BaseClass::PerformLayout();

	const int inner = m_iWheelSize + PICKER_GAP + m_iBarWide;

	// the switch: two halves side by side, the room of a circle (the name color, Paint) kept free right of them
	const int targetWide = (inner - m_iTargetTall - PICKER_GAP) / 2;
	m_pTargets[0]->SetBounds(PICKER_PADDING, PICKER_PADDING, targetWide, m_iTargetTall);
	m_pTargets[1]->SetBounds(PICKER_PADDING + targetWide - 1, PICKER_PADDING, targetWide, m_iTargetTall); // shared edge
	// one setting: no switch (the circle right of it still shows the edited color)
	for (CTargetPanel *pTarget : m_pTargets)
		pTarget->SetVisible(Host()->HasPickerTargets() && !IsPlain());

	const int top = IsPlain() ? PICKER_PADDING : PICKER_PADDING + m_iTargetTall + PICKER_GAP;
	m_pWheel->SetBounds(PICKER_PADDING, top, m_iWheelSize, m_iWheelSize);
	m_pBrightness->SetBounds(PICKER_PADDING + m_iWheelSize + PICKER_GAP, top, m_iBarWide, m_iWheelSize);

	// a plain color: no quick picks
	for (CSwatchPanel *pSwatch : m_pSwatches)
		pSwatch->SetVisible(!IsPlain());
	if (IsPlain())
		return;

	// the row, spread over the popup's width
	const int y = top + m_iWheelSize + PICKER_GAP;
	for (int i = 0; i < CHAT_COLOR_PRESETS; i++)
	{
		const int x = PICKER_PADDING + (CHAT_COLOR_PRESETS > 1 ? i * (inner - m_iSwatchSize) / (CHAT_COLOR_PRESETS - 1) : 0);
		m_pSwatches[i]->SetBounds(x, y, m_iSwatchSize, m_iSwatchSize);
	}
}

void CHudChatColorPicker::Paint()
{
	int wide, tall;
	GetSize(wide, tall);

	vgui2::surface()->DrawSetColor(Color(0, 0, 0, 220));
	vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
	vgui2::surface()->DrawSetColor(Color(255, 255, 255, 64));
	vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);
	if (IsPlain())
		return; // the color field shows the color itself

	// the edited setting's color - the chat color with "Chat" selected, the name color with "Name" (the one being
	// picked while dragging; a ring for none) - in a small circle right of the switch, the same on both tabs;
	// the collapsed circle left of the prompt always shows the chat color
	if (m_iTargetTall > 4)
	{
		const int size = m_iTargetTall - 2;
		::DrawChatColorSwatch(Host()->GetPickerTextColor(), Host()->GetPickerColor(), wide - PICKER_PADDING - size, PICKER_PADDING + 1, size, false);
	}
}

void CHudChatColorPicker::PrecacheTextures()
{
	gTexMgr.PrecacheCircles();
}

void CHudChatColorPicker::SetColor(const ChatColor &color)
{
	if (!color.IsNone())
	{
		float hue, sat, val;
		RGBtoHSV(GetDisplayColor(color), hue, sat, val);
		// a grey (or black) has no hue: the wheel keeps the one it had
		if (sat > 0.f && val > 0.f)
		{
			m_flHue = ClampHue(hue);
			m_flSat = sat;
		}
		else if (val > 0.f || !IsPlain())
			m_flSat = sat;
		m_iLevel = (int)(val * GetMaxLevel() / 100.f + 0.5f);
	}

	SetLevel(m_iLevel);
}

void CHudChatColorPicker::SetHueSat(float hue, float sat)
{
	m_flHue = ClampHue(hue);
	m_flSat = sat < 0.f ? 0.f : (sat > 100.f ? 100.f : sat);
}

void CHudChatColorPicker::SetLevel(int level)
{
	m_iLevel = level < GetMinLevel() ? GetMinLevel() : (level > GetMaxLevel() ? GetMaxLevel() : level);

	// the level's wheel is made here (opening the popup, moving the bar), not while painting
	if (IsPlain())
		gTexMgr.GetPlainWheelTexture();
	else
		gTexMgr.GetWheelTexture(m_iLevel);
}

void CHudChatColorPicker::CancelDrag()
{
	m_pWheel->CancelDrag();
	m_pBrightness->CancelDrag();
}

void CHudChatColorPicker::OnPicked(vgui2::Panel *pSource, bool bSelect)
{
	IChatColorPickerHost *pHost = Host();
	const ChatColor color = GetPickedColor();
	if (bSelect)
	{
		pHost->SetPickerColor(color);
		pHost->ClearPickerPreview();
		pHost->HidePickerTooltip(pSource);
	}
	else
	{
		pHost->SetPickerPreview(color);

		// the code of the color, as it can be typed in a message or a name
		wchar_t text[64];
		GetChatColorDisplayName(color, text, sizeof(text));
		pHost->ShowPickerTooltip(pSource, text);
	}
}

//-----------------------------------------------------------------------------
// Purpose: color picker tooltip
//-----------------------------------------------------------------------------
CHudChatColorTooltip::CHudChatColorTooltip(vgui2::Panel *parent, const char *panelName)
    : BaseClass(parent, panelName)
{
	SetPaintBackgroundEnabled(false);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetVisible(false);
}

void CHudChatColorTooltip::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	m_hFont = pScheme->GetFont("ChatFont");
	SetTooltipText(m_szText);
}

void CHudChatColorTooltip::SetTooltipText(const wchar_t *text)
{
	if (text != m_szText)
		V_wcsncpy(m_szText, text ? text : L"", sizeof(m_szText));

	int textWide = 0, textTall = 0;
	if (m_hFont)
		vgui2::surface()->GetTextSize(m_hFont, m_szText, textWide, textTall);
	SetSize(textWide + 8, textTall + 4);
}

void CHudChatColorTooltip::Paint()
{
	CHudChat *pChat = CHudChat::Get();
	if (!pChat || !m_hFont)
		return;

	int wide, tall;
	GetSize(wide, tall);

	vgui2::surface()->DrawSetColor(Color(0, 0, 0, 220));
	vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
	vgui2::surface()->DrawSetColor(Color(255, 255, 255, 64));
	vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);

	vgui2::surface()->DrawSetTextFont(m_hFont);
	vgui2::surface()->DrawSetTextColor(pChat->GetDefaultTextColor());
	vgui2::surface()->DrawSetTextPos(4, 2);
	vgui2::surface()->DrawPrintText(m_szText, (int)wcslen(m_szText));
}

//-----------------------------------------------------------------------------
// Purpose: color field of the options windows (Name color, Chat color)
//-----------------------------------------------------------------------------
class CSboColorPopup : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CSboColorPopup(vgui2::Panel *pWindow, CSboColorField *pField)
	    : BaseClass(pWindow, "ColorPopup")
	{
		SetPaintBackgroundEnabled(false);
		SetMouseInputEnabled(true);
		SetVisible(false);
		m_pPicker = new CHudChatColorPicker(this, "ColorPicker", pField);
	}

	CHudChatColorPicker *Picker() { return m_pPicker; }

private:
	CHudChatColorPicker *m_pPicker;
};

CSboColorField::CSboColorField(vgui2::Panel *parent, const char *panelName)
    : BaseClass(parent, panelName)
{
	SetPaintBackgroundEnabled(false);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(false);
}

CSboColorField::~CSboColorField()
{
	if (m_pPopup)
		m_pPopup->MarkForDeletion();
}

void CSboColorField::FormatValue(const ChatColor &color, char *value, int valueSize)
{
	// the chat's FormatChatColorValue (hud_chat.cpp)
	if (color.IsCustom())
		snprintf(value, valueSize, "x%06X", color.rgb & 0xFFFFFF);
	else
		snprintf(value, valueSize, "%d", ChatColor::FromCode(color.code).code);
}

void CSboColorField::SetLook(vgui2::HFont font, Color text, Color frame, Color background)
{
	m_hFont = font;
	m_TextColor = text;
	m_FrameColor = frame;
	m_BgColor = background;
	Repaint();
}

void CSboColorField::SetColor(const ChatColor &color)
{
	m_Color = color;
	m_bPreview = false;
	Repaint();
}

void CSboColorField::Paint()
{
	int wide, tall;
	GetSize(wide, tall);
	vgui2::surface()->DrawSetColor(m_BgColor);
	vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
	vgui2::surface()->DrawSetColor((IsCursorOver() || IsPickerOpen()) ? m_TextColor : m_FrameColor);
	vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);

	const ChatColor shown = GetPickerColor();
	const int pad = tall / 6 > 2 ? tall / 6 : 2;
	const int swatch = tall - 2 * pad;
	::DrawChatColorSwatch(m_TextColor, shown, pad + 1, pad, swatch, false);

	if (!m_hFont)
		m_hFont = vgui2::scheme()->GetIScheme(GetScheme())->GetFont("Default", IsProportional());
	wchar_t text[64];
	if (shown.IsNone())
		V_wcsncpy(text, L"Default", sizeof(text));
	else
		GetChatColorDisplayName(shown, text, sizeof(text));
	int tw = 0, th = 0;
	vgui2::surface()->GetTextSize(m_hFont, text, tw, th);
	vgui2::surface()->DrawSetTextFont(m_hFont);
	vgui2::surface()->DrawSetTextColor(m_TextColor);
	vgui2::surface()->DrawSetTextPos(pad + swatch + pad * 2 + 1, (tall - th) / 2);
	vgui2::surface()->DrawPrintText(text, (int)wcslen(text));
}

bool CSboColorField::IsPickerOpen() const
{
	return m_pPopup && m_pPopup->IsVisible();
}

void CSboColorField::OpenPicker()
{
	// the popup goes on the field's window, above every row
	vgui2::Panel *pWindow = this;
	while (pWindow->GetParent() && pWindow->GetParent()->GetParent())
	{
		pWindow = pWindow->GetParent();
		if (dynamic_cast<vgui2::Frame *>(pWindow))
			break;
	}
	if (!m_pPopup)
		m_pPopup = new CSboColorPopup(pWindow, this);

	CHudChatColorPicker::PrecacheTextures();
	CHudChatColorPicker *pPicker = m_pPopup->Picker();
	int winW, winH;
	pWindow->GetSize(winW, winH);
	int fx = 0, fy = 0;
	LocalToScreen(fx, fy);
	pWindow->ScreenToLocal(fx, fy);
	const int below = winH - (fy + GetTall()) - 4, above = fy - 4;
	pPicker->SetMetrics(GetTall(), below >= above ? below : above);
	pPicker->SetColor(m_Color);
	pPicker->SetPos(0, 0);
	pPicker->SetVisible(true);
	pPicker->SetMouseInputEnabled(true);
	int pw, ph;
	pPicker->GetSize(pw, ph);
	m_pPopup->SetSize(pw, ph);
	int x = fx, y = fy + GetTall() + 2;
	if (y + ph > winH && fy - ph - 2 >= 0)
		y = fy - ph - 2;
	if (x + pw > winW)
		x = winW - pw;
	m_pPopup->SetPos(x < 0 ? 0 : x, y < 0 ? 0 : y);
	m_pPopup->SetVisible(true);
	m_pPopup->MoveToFront();
	// the press that opened it is still down: only a later press closes it
	m_bMouseWasDown = true;
	Repaint();
}

void CSboColorField::ClosePicker()
{
	if (!m_pPopup)
		return;
	m_pPopup->Picker()->CancelDrag();
	m_bPreview = false;
	m_pPopup->SetVisible(false);
	Repaint();
}

void CSboColorField::SetVisible(bool state)
{
	if (!state)
		ClosePicker();
	BaseClass::SetVisible(state);
}

void CSboColorField::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	if (IsPickerOpen())
		ClosePicker();
	else
		OpenPicker();
}

void CSboColorField::OnThink()
{
	BaseClass::OnThink();
	if (!IsPickerOpen())
		return;
	// a press outside the popup and the field closes it; so does the field's window going away
	const bool down = vgui2::input()->IsMouseDown(vgui2::MOUSE_LEFT);
	if (down && !m_bMouseWasDown)
	{
		int mx, my;
		vgui2::input()->GetCursorPos(mx, my);
		int px = 0, py = 0, pw, ph;
		m_pPopup->LocalToScreen(px, py);
		m_pPopup->GetSize(pw, ph);
		int fx = 0, fy = 0;
		LocalToScreen(fx, fy);
		const bool inPopup = mx >= px && my >= py && mx < px + pw && my < py + ph;
		const bool inField = mx >= fx && my >= fy && mx < fx + GetWide() && my < fy + GetTall();
		if (!inPopup && !inField)
			ClosePicker();
	}
	m_bMouseWasDown = down;
	if (!IsVisible() || !GetParent() || !GetParent()->IsVisible())
		ClosePicker();
}

void CSboColorField::SetPickerColor(const ChatColor &color)
{
	m_bPreview = false;
	if (color != m_Color)
	{
		m_Color = color;
		PostActionSignal(new KeyValues("TextChanged"));
	}
	Repaint();
}

void CSboColorField::SetPickerPreview(const ChatColor &color)
{
	m_Preview = color;
	m_bPreview = true;
	Repaint();
}

void CSboColorField::ClearPickerPreview()
{
	m_bPreview = false;
	Repaint();
}
