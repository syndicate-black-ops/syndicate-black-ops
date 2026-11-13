//========= SBO VGUI2 controls: curves and pictures =========//
//
// Purpose: see sbo_graph_controls.h.
//=================================================================//

#include <math.h>
#include <string.h>
#include <wchar.h>
#include <algorithm>

#include <KeyValues.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "../sbo_ui_style.h"
#include "sbo_graph_controls.h"

using namespace sbolook;

//-----------------------------------------------------------------------------
// CSboCurveEditor
//-----------------------------------------------------------------------------
CSboCurveEditor::CSboCurveEditor(vgui2::Panel *pParent, const char *pName)
    : BaseClass(pParent, pName)
{
	SetMouseInputEnabled(true);
	m_Points = { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
}

void CSboCurveEditor::SetPoints(const sboctl::CurvePoint *points, int count)
{
	m_Points.clear();
	for (int i = 0; points && i < count && i < sboctl::CURVE_MAX_POINTS; i++)
		m_Points.push_back({ std::max(0.0f, std::min(1.0f, points[i].x)), std::max(0.0f, std::min(1.0f, points[i].y)) });
	if (m_Points.size() < 2)
		m_Points = { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
	m_Points.front().x = 0.0f;
	m_Points.back().x = 1.0f;
	m_iDrag = -1;
	m_iSelected = -1;
	Repaint();
}

int CSboCurveEditor::GetPoints(sboctl::CurvePoint *out, int maxCount) const
{
	const int n = std::min((int)m_Points.size(), maxCount);
	for (int i = 0; i < n; i++)
		out[i] = m_Points[i];
	return n;
}

int CSboCurveEditor::Inset()
{
	return S().M(5);
}

void CSboCurveEditor::ToScreen(float fx, float fy, int &x, int &y)
{
	const int in = Inset(), w = GetWide() - 2 * in, h = GetTall() - 2 * in;
	x = in + (int)(fx * w + 0.5f);
	y = in + (int)((1.0f - fy) * h + 0.5f);
}

void CSboCurveEditor::FromScreen(int x, int y, float &fx, float &fy)
{
	const int in = Inset(), w = std::max(1, GetWide() - 2 * in), h = std::max(1, GetTall() - 2 * in);
	fx = (float)(x - in) / w;
	fy = 1.0f - (float)(y - in) / h;
}

int CSboCurveEditor::PointAt(int x, int y)
{
	const int reach = S().M(6);
	int best = -1, bestD = reach * reach + 1;
	for (int i = 0; i < (int)m_Points.size(); i++)
	{
		int px, py;
		ToScreen(m_Points[i].x, m_Points[i].y, px, py);
		const int d = (px - x) * (px - x) + (py - y) * (py - y);
		if (d < bestD)
		{
			bestD = d;
			best = i;
		}
	}
	return best;
}

void CSboCurveEditor::Changed()
{
	Repaint();
	PostActionSignal(new KeyValues("CurveChanged"));
}

void CSboCurveEditor::OnMousePressed(vgui2::MouseCode code)
{
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	const int hit = PointAt(x, y);
	if (code == vgui2::MOUSE_RIGHT)
	{
		if (hit > 0 && hit < (int)m_Points.size() - 1)
		{
			m_Points.erase(m_Points.begin() + hit);
			m_iSelected = -1;
			Changed();
		}
		return;
	}
	if (code != vgui2::MOUSE_LEFT)
		return;
	if (hit >= 0)
		m_iDrag = hit;
	else
	{
		// a new point where the click is (between its neighbours), if there is room for one
		float fx, fy;
		FromScreen(x, y, fx, fy);
		if ((int)m_Points.size() >= sboctl::CURVE_MAX_POINTS || fx <= 0.01f || fx >= 0.99f)
			return;
		int at = 1;
		while (at < (int)m_Points.size() && m_Points[at].x < fx)
			at++;
		if (fx - m_Points[at - 1].x < 0.01f || m_Points[at].x - fx < 0.01f)
			return;
		m_Points.insert(m_Points.begin() + at, { fx, std::max(0.0f, std::min(1.0f, fy)) });
		m_iDrag = at;
		Changed();
	}
	m_iSelected = m_iDrag;
	m_bDragOut = false;
	vgui2::input()->SetMouseCapture(GetVPanel());
	Repaint();
}

void CSboCurveEditor::OnCursorMoved(int x, int y)
{
	if (m_iDrag < 0 || m_iDrag >= (int)m_Points.size())
		return;
	float fx, fy;
	FromScreen(x, y, fx, fy);
	const bool end = m_iDrag == 0 || m_iDrag == (int)m_Points.size() - 1;
	// a middle point dragged well out of the box goes away on release (drawn hollow meanwhile)
	m_bDragOut = !end && (fy < -0.15f || fy > 1.15f || fx < -0.15f || fx > 1.15f);
	sboctl::CurvePoint &p = m_Points[m_iDrag];
	if (!end)
	{
		const float lo = m_Points[m_iDrag - 1].x + 0.01f, hi = m_Points[m_iDrag + 1].x - 0.01f;
		p.x = std::max(lo, std::min(hi, fx));
	}
	p.y = std::max(0.0f, std::min(1.0f, fy));
	Changed();
}

void CSboCurveEditor::OnMouseReleased(vgui2::MouseCode code)
{
	if (m_iDrag < 0)
		return;
	if (m_bDragOut && m_iDrag > 0 && m_iDrag < (int)m_Points.size() - 1)
	{
		m_Points.erase(m_Points.begin() + m_iDrag);
		m_iSelected = -1;
		Changed();
	}
	m_iDrag = -1;
	m_bDragOut = false;
	vgui2::input()->SetMouseCapture(0);
	Repaint();
}

void CSboCurveEditor::PaintBackground()
{
	FillRect(0, 0, GetWide(), GetTall(), COL_BG_INPUT);
	OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER);
}

void CSboCurveEditor::Paint()
{
	const int in = Inset(), w = GetWide() - 2 * in, h = GetTall() - 2 * in;
	// quarter grid and the straight line (no change)
	for (int i = 1; i < 4; i++)
	{
		FillRect(in + w * i / 4, in, 1, h, Color(44, 90, 37, 120));
		FillRect(in, in + h * i / 4, w, 1, Color(44, 90, 37, 120));
	}
	vgui2::surface()->DrawSetColor(Color(92, 111, 87, 160));
	vgui2::surface()->DrawLine(in, in + h, in + w, in);

	// the curve, one segment per two pixels
	sboctl::CurvePoint pts[sboctl::CURVE_MAX_POINTS];
	const int n = GetPoints(pts, sboctl::CURVE_MAX_POINTS);
	vgui2::surface()->DrawSetColor(m_LineColor);
	int lx = -1, ly = -1;
	for (int px = 0; px <= w; px += 2)
	{
		const float fx = (float)px / std::max(1, w);
		const float fy = std::max(0.0f, std::min(1.0f, sboctl::EvalMonotoneCurve(pts, n, fx)));
		int x, y;
		ToScreen(fx, fy, x, y);
		if (lx >= 0)
		{
			vgui2::surface()->DrawLine(lx, ly, x, y);
			vgui2::surface()->DrawLine(lx, ly + 1, x, y + 1); // 2 px thick
		}
		lx = x;
		ly = y;
	}

	// the points
	const int r = std::max(2, S().M(3));
	for (int i = 0; i < n; i++)
	{
		int x, y;
		ToScreen(pts[i].x, pts[i].y, x, y);
		const bool sel = i == m_iSelected;
		if (sel && m_bDragOut)
			OutlineRect(x - r, y - r, 2 * r + 1, 2 * r + 1, COL_TEXT_FAINT);
		else
		{
			FillRect(x - r, y - r, 2 * r + 1, 2 * r + 1, sel ? COL_TEXT_PRIMARY : COL_BG_INPUT);
			OutlineRect(x - r, y - r, 2 * r + 1, 2 * r + 1, m_LineColor);
		}
	}
}

//-----------------------------------------------------------------------------
// CSboRgbaImage
//-----------------------------------------------------------------------------
CSboRgbaImage::CSboRgbaImage(vgui2::Panel *pParent, const char *pName)
    : BaseClass(pParent, pName)
{
	SetMouseInputEnabled(true);
}

CSboRgbaImage::~CSboRgbaImage()
{
	if (m_iTexture != -1)
		vgui2::surface()->DeleteTextureByID(m_iTexture);
}

void CSboRgbaImage::SetEmptyText(const wchar_t *text)
{
	wcsncpy(m_szEmpty, text ? text : L"", sizeof(m_szEmpty) / sizeof(m_szEmpty[0]) - 1);
	Repaint();
}

void CSboRgbaImage::SetImage(const unsigned char *rgba, int wide, int tall)
{
	if (!rgba || wide <= 0 || tall <= 0)
	{
		m_iW = m_iH = 0;
		Repaint();
		return;
	}
	if (m_iTexture == -1)
		m_iTexture = vgui2::surface()->CreateNewTextureID(true);
	vgui2::surface()->DrawSetTextureRGBA(m_iTexture, rgba, wide, tall, m_bFiltered ? 1 : 0, true);
	m_iW = wide;
	m_iH = tall;
	Repaint();
}

void CSboRgbaImage::DrawRect(int &x, int &y, int &w, int &h)
{
	const int pw = GetWide() - 2, ph = GetTall() - 2;
	if (m_iW <= 0 || m_iH <= 0 || pw <= 0 || ph <= 0)
	{
		x = y = w = h = 0;
		return;
	}
	if ((long long)pw * m_iH <= (long long)ph * m_iW)
	{
		w = pw;
		h = (int)((long long)pw * m_iH / m_iW);
	}
	else
	{
		h = ph;
		w = (int)((long long)ph * m_iW / m_iH);
	}
	x = 1 + (pw - w) / 2;
	y = 1 + (ph - h) / 2;
}

void CSboRgbaImage::PaintBackground()
{
	FillRect(0, 0, GetWide(), GetTall(), COL_BG_INPUT);
	OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER);
}

void CSboRgbaImage::Paint()
{
	if (m_iW <= 0 || m_iTexture == -1)
	{
		if (m_szEmpty[0])
		{
			const int tw = TextWide(S().small, m_szEmpty);
			DrawTextMid(S().small, COL_TEXT_DIM, std::max(0, (GetWide() - tw) / 2), 0, GetTall(), m_szEmpty, GetWide());
		}
		return;
	}
	int x, y, w, h;
	DrawRect(x, y, w, h);
	vgui2::surface()->DrawSetColor(255, 255, 255, 255);
	vgui2::surface()->DrawSetTexture(m_iTexture);
	vgui2::surface()->DrawTexturedRect(x, y, x + w, y + h);
}

void CSboRgbaImage::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT || m_iW <= 0)
		return;
	int cx, cy, x, y, w, h;
	vgui2::input()->GetCursorPos(cx, cy);
	ScreenToLocal(cx, cy);
	DrawRect(x, y, w, h);
	if (w <= 0 || h <= 0 || cx < x || cy < y || cx >= x + w || cy >= y + h)
		return;
	PostActionSignal(new KeyValues("ImageClicked", "x", (cx - x) * m_iW / w, "y", (cy - y) * m_iH / h));
}
