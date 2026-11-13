//========= SBO VGUI2 controls: a picture picker =========//
//
// Purpose: CSboPicturePicker, a tool window (sbo_tool_window.h) that lists files as a grid of pictures with their names
//          under them (CSboOrderedList::SetGrid): a search box above it that filters the list as you type, more
//          columns and lines as the window grows, the whole path as the tip of a picture, and a double click, Enter
//          or its Use button to choose one. The pictures are made by the function given to SetThumbMaker (decoded from the
//          file by the tool, never through an engine slot) when a cell is first drawn, and freed when the window
//          closes. Some files can be marked filtered: they are left out until the "show filtered" box is ticked
//          (SetFilteredToggle). The particle editor's sprite list and the world screen editor's picture list use it.
//
// No tool logic here. SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_CONTROLS_SBO_PICTURE_PICKER_H
#define SBO_VGUI2_CONTROLS_SBO_PICTURE_PICKER_H

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <Color.h>

#include "sbo_tool_window.h"

class CSboBrowserCheck;
class CSboOrderedList;
class CSboTextField;

class CSboPicturePicker : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CSboPicturePicker, CSboToolWindow);

public:
	//! Decodes the file's picture to fit size x size (rgba, top row first); false when it can't
	typedef std::function<bool(const std::string &path, int size, std::vector<uint8_t> &rgba, int &wide, int &tall)>
	    ThumbMaker;
	//! A file was chosen; return "" to close the window, or a sentence saying why it can't be used (shown, stays open)
	typedef std::function<std::string(const std::string &path)> PickFunc;

	//! key: the window's entry in sbotools.vdf; noun: what the count line calls the files ("pictures")
	CSboPicturePicker(vgui2::Panel *pParent, const char *pName, const char *title, const char *key, const char *noun);

	void SetThumbMaker(ThumbMaker maker) { m_Maker = maker; }
	//! The color behind each picture (black by default)
	void SetCellColor(Color c) { m_CellColor = c; }
	//! The tips of the search box, the list and the Use button
	void SetTips(const char *search, const char *list, const char *use);
	//! The files, as the list shows them (the thumbnails of the old ones go); filtered: per file, left out unless the
	//! filtered box is ticked (empty: none)
	void SetFiles(const std::vector<std::string> &files, const std::vector<bool> &filtered = std::vector<bool>());
	//! A check box under the search box that shows the filtered files too (label and tip)
	void SetFilteredToggle(const char *label, const char *tip);
	bool HasFiles() const { return !m_All.empty(); }
	//! The status line under the list
	void SetStatus(const std::string &status) { m_Status = status; Repaint(); }

	//! Shows the window with a status line; pick gets the chosen file
	void OpenFor(const std::string &status, PickFunc pick);

	MESSAGE_FUNC_INT(OnListActivated, "ListActivated", index);
	MESSAGE_FUNC_INT(OnListIconClicked, "ListIconClicked", index);
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
	MESSAGE_FUNC_PTR(OnButtonToggled, "ButtonToggled", panel);
	void OnCommand(const char *command) override;

protected:
	void LayoutContent(int x, int y, int w, int h) override;
	void Paint() override;
	void OnToolClosed() override;

private:
	struct Thumb
	{
		int texture = -1; // -1: the file could not be read
		int wide = 0, tall = 0;
	};
	void PaintThumb(int index, int x, int y, int size);
	void FreeThumbs();
	void Fill();
	void Use(int index);

	CSboTextField *m_pFilter;
	CSboBrowserCheck *m_pShowFiltered = nullptr;
	CSboOrderedList *m_pList;
	vgui2::Panel *m_pUse;
	std::vector<std::string> m_All, m_Shown;
	std::vector<bool> m_Filtered; // by m_All
	int m_iFilteredOut = 0;       // left out by the filter now
	std::map<std::string, Thumb> m_Thumbs;
	std::string m_Filter, m_Status, m_Noun;
	ThumbMaker m_Maker;
	PickFunc m_Pick;
	Color m_CellColor = Color(0, 0, 0, 255);
	int m_iTextY = 0;
};

#endif // SBO_VGUI2_CONTROLS_SBO_PICTURE_PICKER_H
