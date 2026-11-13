//========= SBO VGUI2 controls: lists =========//
//
// Purpose: CSboOrderedList, an ordered list for SBO's tool windows (sbo_tool_window.h) in the server browser's look:
//          rows of an icon and a text, one selected, scrolled with the wheel, the scroll bar (drag its thumb, click
//          above or below it for a page) or the keyboard (after a click in the list it takes the keyboard until Esc or
//          a click elsewhere: arrows, Page Up / Down, Home, End, Enter activates the row), with the buttons New, Delete,
//          Up and Down under it (and any the tool adds). The list moves rows itself (Up / Down) and tells its owner; the
//          owner keeps its own data in the same order with the messages:
//            "ListSelected"    (index)          a row was selected (-1: none)
//            "ListActivated"   (index)          a row was double clicked, or Enter pressed on it
//            "ListIconClicked" (index)          a row's icon was clicked
//            "ListMoved"       (from, to)       a row moved (Up / Down)
//            "ListNew"                          New was pressed (the owner adds a row, or opens a menu)
//            "ListDelete"      (index)          Delete was pressed on the selected row (the owner removes it)
//          A button the tool adds sends its command to the list's action targets.
//          Icons are drawn by an IconPainter the owner gives (a function of the row's icon id), so the list has no
//          images of its own.
//          As a grid (SetGrid) the rows are cells that fill the list: as many columns as its width holds and as many
//          lines as its height holds, laid out again when it is sized. A cell is the icon with the part of its text
//          after the last '/' under it (cut with "..."); the arrows move by cell and by line, and a click on a cell's
//          icon selects it like a click on its text.
//
// No tool logic here. SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_CONTROLS_SBO_LIST_CONTROLS_H
#define SBO_VGUI2_CONTROLS_SBO_LIST_CONTROLS_H

#include <functional>
#include <string>
#include <vector>

#include <vgui_controls/Panel.h>

class CSboBrowserButton;

class CSboOrderedList : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboOrderedList, vgui2::Panel);

public:
	//! Draws icon id `icon` in the square (x, y, size, size); `on`: the row's state (e.g. enabled)
	typedef std::function<void(int icon, bool on, int x, int y, int size)> IconPainter;

	CSboOrderedList(vgui2::Panel *pParent, const char *pName);

	void SetIconPainter(IconPainter painter) { m_Painter = painter; }
	//! The rows' height, design pixels (20 by default; taller for pictures)
	void SetRowTall(int designPx) { m_iRowDesignTall = designPx; InvalidateLayout(); Repaint(); }
	//! Shows the rows as a grid of cells at least cellWide x cellTall design pixels, with icons iconSize pixels
	//! square (0, 0, 0: a list again)
	void SetGrid(int cellWide, int cellTall, int iconSize);
	bool IsGrid() const { return m_iCellDesignWide > 0; }
	//! The row under a point of the list (-1: none)
	int RowAtPoint(int x, int y);
	//! A button after the standard four (right aligned); its command goes to the action targets
	CSboBrowserButton *AddButton(const char *text, const char *command);
	//! Shows or hides the standard buttons (New, Delete, Up, Down)
	void SetStandardButtons(bool show);
	//! The standard buttons (for their tips)
	CSboBrowserButton *NewButton() { return m_pNew; }
	CSboBrowserButton *DeleteButton() { return m_pDelete; }
	CSboBrowserButton *UpButton() { return m_pUp; }
	CSboBrowserButton *DownButton() { return m_pDown; }

	int AddRow(const char *text, int icon, bool on);
	void InsertRow(int index, const char *text, int icon, bool on);
	void RemoveRow(int index);
	void ClearRows();
	void SetRowText(int index, const char *text);
	void SetRowOn(int index, bool on);
	int RowCount() const { return (int)m_Rows.size(); }

	//! Selects a row (-1: none); no message
	void SetSelected(int index);
	int GetSelected() const { return m_iSelected; }

	void PerformLayout() override;
	void Paint() override;
	void PaintBackground() override;
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override;
	void OnMouseWheeled(int delta) override;
	void OnMouseReleased(vgui2::MouseCode code) override;
	void OnCursorMoved(int x, int y) override;
	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	void OnCommand(const char *command) override;

private:
	struct Row
	{
		std::string text;
		int icon = 0;
		bool on = true;
	};
	int RowTall();  //!< a line: a row, or a line of cells
	int Columns();  //!< cells on a line (1 as a list)
	int CellWide(); //!< a cell's width (the list's width as a list)
	int Lines();    //!< lines of rows / cells
	int ListTall();
	int RowsFit();  //!< lines that fit
	int RowAt(int y);
	bool OnIcon(int x, int y, int row);
	void UpdateButtons();
	void ScrollTo(int index);
	void ClampFirst();
	int BarWide();
	void ThumbSpan(int &y, int &h);
	void Select(int index, bool post);
	void Post(const char *name, const char *key = nullptr, int value = 0, const char *key2 = nullptr, int value2 = 0);

	std::vector<Row> m_Rows;
	IconPainter m_Painter;
	int m_iSelected = -1;
	int m_iFirst = 0; //!< the first line shown
	int m_iRowDesignTall = 20;
	int m_iCellDesignWide = 0, m_iCellDesignTall = 0, m_iIconDesign = 0; // a grid
	bool m_bThumbDrag = false;
	int m_iThumbGrab = 0;
	bool m_bStandard = true;
	CSboBrowserButton *m_pNew, *m_pDelete, *m_pUp, *m_pDown;
	std::vector<CSboBrowserButton *> m_Extra;
};

#endif // SBO_VGUI2_CONTROLS_SBO_LIST_CONTROLS_H
