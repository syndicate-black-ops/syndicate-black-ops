//========= SBO VGUI2 controls: curves and pictures =========//
//
// Purpose: controls that show or edit a picture-like value, for SBO's tool windows (sbo_tool_window.h), in the server
//          browser's look (sbo_ui_style.h):
//            * CSboCurveEditor: a curve from 0 to 1 across and up (sbo_curve.h: a monotone cubic through its points).
//              A click on the line adds a point, a point is dragged, a right click (or dragging it out of the box)
//              removes it; the two end points move only up and down. "CurveChanged" while it changes.
//            * CSboRgbaImage: an RGBA picture (made by the tool, e.g. a lookup table or a copy of the view) drawn to fit
//              the panel, its aspect kept; SetImage again replaces it. "ImageClicked" (x, y: picture pixels).
//
// No tool logic here. SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_CONTROLS_SBO_GRAPH_CONTROLS_H
#define SBO_VGUI2_CONTROLS_SBO_GRAPH_CONTROLS_H

#include <vector>

#include <Color.h>
#include <vgui_controls/Panel.h>

#include "sbo_curve.h"

class CSboCurveEditor : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboCurveEditor, vgui2::Panel);

public:
	CSboCurveEditor(vgui2::Panel *pParent, const char *pName);
	//! Shows these points (2 .. CURVE_MAX_POINTS, sorted by x; the first at x 0, the last at x 1); no message
	void SetPoints(const sboctl::CurvePoint *points, int count);
	int GetPoints(sboctl::CurvePoint *out, int maxCount) const;
	//! The line's color (e.g. a color channel's)
	void SetLineColor(Color c) { m_LineColor = c; Repaint(); }

	void Paint() override;
	void PaintBackground() override;
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseReleased(vgui2::MouseCode code) override;
	void OnCursorMoved(int x, int y) override;

private:
	void ToScreen(float fx, float fy, int &x, int &y);
	void FromScreen(int x, int y, float &fx, float &fy);
	int PointAt(int x, int y);
	void Changed();
	int Inset();

	std::vector<sboctl::CurvePoint> m_Points;
	Color m_LineColor = Color(198, 242, 176, 255);
	int m_iDrag = -1;
	int m_iSelected = -1;
	bool m_bDragOut = false; //!< the dragged point is outside the box: removed on release
};

class CSboRgbaImage : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboRgbaImage, vgui2::Panel);

public:
	CSboRgbaImage(vgui2::Panel *pParent, const char *pName);
	~CSboRgbaImage();
	//! rgba: wide x tall pixels, top row first; null or 0 size: no picture
	void SetImage(const unsigned char *rgba, int wide, int tall);
	//! Pixel-sharp (a table's cells) or filtered (a photo)
	void SetFiltered(bool filtered) { m_bFiltered = filtered; }
	//! Text in the middle while there is no picture
	void SetEmptyText(const wchar_t *text);
	int ImageWide() const { return m_iW; }
	int ImageTall() const { return m_iH; }

	void Paint() override;
	void PaintBackground() override;
	void OnMousePressed(vgui2::MouseCode code) override;

private:
	void DrawRect(int &x, int &y, int &w, int &h);
	int m_iTexture = -1;
	int m_iW = 0, m_iH = 0;
	bool m_bFiltered = true;
	wchar_t m_szEmpty[96] = {};
};

#endif // SBO_VGUI2_CONTROLS_SBO_GRAPH_CONTROLS_H
