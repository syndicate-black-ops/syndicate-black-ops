// SBO 2.0: a world screen layout (ui/scripts/screens/<name>.res) as the in-game world screen editor edits it
// (vgui2/sbo_worldscreen_editor.cpp; docs/WORLD_SCREENS.md "Editor").
//
// The layout's text is the document: a change rewrites only the value it touches (or adds or removes that one
// "key" "value" line, or a control's whole section), so comments, the order of the keys and keys the editor does not
// know stay as they were, and what is saved is the text the game reads. The text is read with the rules of the
// SDK's KeyValues text reader (KeyValues::LoadFromBuffer, which the game uses for the file): quoted or bare tokens,
// // comments to the end of the line, { } sections, [$CONDITION] tags after a token, no escape sequences. The first
// top-level section is the layout; its sections are the controls (the "WorldScreen" section is the screen itself).
//
// Plain C++ (no engine, no VGUI2): the offline test tests/worldscreen_editor/ runs it with AddressSanitizer / UBSan.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace wslayout
{
struct Pair
{
	std::string key;   // as written (keys are case-insensitive)
	std::string value;
	size_t keyBeg = 0; // the key's token in the text (its quotes included)
	size_t valBeg = 0; // the value's token (its quotes included)
	size_t valEnd = 0;
};

struct Section
{
	std::string name;
	size_t nameBeg = 0;  // the name's token
	size_t open = 0;     // the '{'
	size_t close = 0;    // the '}', or std::string::npos when the section is not closed
	size_t end = 0;      // after the '}' (or the end of the text)
	std::vector<Pair> pairs; // its own "key" "value" lines (not those of sections inside it)
};

// at most this many characters of a key or a value (KeyValues reads tokens into a 1024-byte buffer)
constexpr size_t MAX_TOKEN = 1023;
// a layout larger than this is not edited (the editor keeps edits cheap)
constexpr size_t MAX_TEXT = 1u << 20;

//! The section name of the screen itself
constexpr const char *SCREEN_SECTION = "WorldScreen";

class Doc
{
public:
	Doc() { Parse(); }

	//! The whole text (a new layout, or a file loaded); false when it is too large or has no layout section
	bool SetText(const std::string &text);
	const std::string &Text() const { return m_Text; }
	//! The layout section's name (the file's path by convention), "" without one
	const std::string &RootName() const { return m_RootName; }
	bool HasRoot() const { return m_bRoot; }

	int Count() const { return (int)m_Sections.size(); }
	const Section &Sec(int i) const { return m_Sections[i]; }
	//! The index of the section of that name (case-insensitive, the first), -1 if none
	int Find(const std::string &name) const;

	//! The value of key in section i, nullptr when it is not there. A control reads the FIRST key of a name
	//! (KeyValues::FindKey), so the editor reads and writes the first.
	const std::string *Get(int i, const char *key) const;
	//! Sets the key's value (the first of that key, or a new line at the end of the section)
	void Set(int i, const char *key, const std::string &value);
	//! Removes every line of that key in the section (the control then uses its default)
	void Remove(int i, const char *key);

	//! Adds a section with these lines at the end of the layout; returns its index (-1 without a layout section)
	int Add(const std::string &name, const std::vector<std::pair<std::string, std::string>> &pairs);
	//! Removes the section (and the blank space before it)
	void Delete(int i);
	//! Renames the section and its "fieldName"
	void Rename(int i, const std::string &name);
	//! Moves the section to the end of the layout (drawn last: on top of the others); returns its new index
	int MoveToEnd(int i);
	//! Moves section i so that it ends up at index `to` (0 .. Count() - 1) in the section order; returns its new index
	int Move(int i, int to);
	//! A section name not in use yet: base, base2, base3...
	std::string UniqueName(const std::string &base) const;

private:
	void Parse();
	void Replace(size_t beg, size_t end, const std::string &with);
	std::string m_Text;
	std::string m_RootName;
	bool m_bRoot = false;
	size_t m_RootClose = std::string::npos;
	std::vector<Section> m_Sections;
};

//! A value as the editor writes it: no '"' (KeyValues has no escapes here: it becomes '\''), no line breaks, at most
//! MAX_TOKEN characters
std::string CleanValue(const std::string &value);
//! A layout name for a file: 1..31 of letters, digits, '_' and '-' (the vgui_screen panelname rule); false if none
bool CleanLayoutName(const std::string &in, char *out, size_t outSize);
//! The text of a new layout called name (a 512 x 256 screen with a title)
std::string NewLayoutText(const std::string &name);
} // namespace wslayout
