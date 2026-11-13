// SBO 2.0: an Aurora script (.aur) as the in-game particle editor edits it (vgui2/sbo_particle_editor.cpp;
// docs/AURORA.md "Editor").
//
// The script's text is the document: a change rewrites only the value it touches (or adds or removes that one
// "key" "value" pair, or a type's { } block), so comments, the order of the keys and keys the editor does not know
// stay as they were, and what is saved is the text the game reads. The text is read with the game's rules
// (particlesys.cpp: Aurora_Token, the block structure, a key with no value before a brace is skipped, a type with no
// closing } ends at the next { or the end), so the editor sees the keys and types the game sees.
//
// Plain C++ (no engine, no VGUI2): the offline test tests/aurora_editor/ runs it with AddressSanitizer / UBSan.
#pragma once

#include <string>
#include <vector>

namespace auroradoc
{
struct Pair
{
	std::string key;		// as written (keys are case-insensitive)
	std::string value;
	size_t keyBeg = 0;		// the key's token in the text (its quotes included)
	size_t valBeg = 0;		// the value's token (its quotes included)
	size_t valEnd = 0;
};

struct Block
{
	size_t open = 0;		// the '{'
	size_t close = 0;		// the '}', or std::string::npos when the block is not closed
	size_t end = 0;			// after the '}', or where the unclosed block ends (the next '{' or the text's end)
	std::vector<Pair> pairs;
};

// at most this many characters of a key or a value (the game's tokens are cut at 1023)
constexpr size_t MAX_TOKEN = 1023;
// a script larger than this is not edited (the game reads any size; the editor keeps edits cheap)
constexpr size_t MAX_TEXT = 1u << 20;

class Doc
{
public:
	Doc() { Parse(); }

	//! The whole text (a new script, or a file loaded)
	void SetText(const std::string &text);
	const std::string &Text() const { return m_Text; }

	//! Type blocks; block -1 is the top level
	int TypeCount() const { return (int)m_Blocks.size(); }
	const Block &Type(int i) const { return m_Blocks[i]; }
	const std::vector<Pair> &TopPairs() const { return m_Top; }

	//! The value of key in the block (the last one, as the game keeps the last), or nullptr when it is not there
	const std::string *Get(int block, const char *key) const;
	//! Sets the key's value (the last of that key, or a new pair at the end of the block); "" is a value too
	void Set(int block, const char *key, const std::string &value);
	//! Removes every pair of that key in the block (the game then uses its default)
	void Remove(int block, const char *key);

	//! The type's name (its last "name"), "" without one
	std::string TypeName(int i) const;
	//! The index of the type of that name (case-insensitive), -1 if none
	int FindType(const std::string &name) const;
	//! Adds a type block at the end with this name; returns its index
	int AddType(const std::string &name);
	//! Removes the type's block (and the blank line before it)
	void DeleteType(int i);
	//! Swaps the type's block with the next one (i, i + 1)
	void SwapTypes(int i);
	//! Renames the type, and every maintype / spraytype / overlaytype that named it
	void RenameType(int i, const std::string &name);
	//! A name no type has yet: base, base2, base3...
	std::string UniqueTypeName(const std::string &base) const;

private:
	void Parse();
	void Replace(size_t beg, size_t end, const std::string &with);
	void RemoveSpan(size_t beg, size_t end);
	const std::vector<Pair> &PairsOf(int block) const { return block < 0 ? m_Top : m_Blocks[block].pairs; }

	std::string m_Text;
	std::vector<Pair> m_Top;
	std::vector<Block> m_Blocks;
};

//! A value as the script writes it: no quote, no line break, at most MAX_TOKEN characters
std::string CleanValue(const std::string &value);
//! A number as the editor writes it: decimals at most, no trailing zeros ("0.5", "40", "-300")
std::string FormatNumber(float value, int decimals);
//! A range value: "min" or "min..max"; false (and min = max = 0) when the text has no number
bool ParseRange(const std::string &text, float &min, float &max, bool &isRange);
//! A script name a player may save: 1 to 39 letters, digits, '_' and '-' (lower case); false otherwise
bool CleanScriptName(const char *in, char *out, int outSize);
//! The text a new script starts with: a puff of smoke rising from the emitter
std::string NewScriptText();
} // namespace auroradoc
