// SBO 2.0: a world screen layout as the in-game world screen editor edits it (worldscreen_layout_doc.h).
#include "worldscreen_layout_doc.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace wslayout
{
namespace
{
bool SameName(const std::string &a, const char *b)
{
	const size_t n = strlen(b);
	if (a.size() != n)
		return false;
	for (size_t i = 0; i < n; i++)
		if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
			return false;
	return true;
}

// One token of the text, read as KeyValues::ReadToken reads it
struct Token
{
	bool ok = false;     // false: the end of the text
	bool quoted = false;
	bool conditional = false; // a bare token with [ ... ] in it
	bool brace = false;  // a bare { or }
	std::string text;
	size_t beg = 0, end = 0;
};

class Reader
{
public:
	explicit Reader(const std::string &t) : m_T(t) {}

	Token Next()
	{
		Token tok;
		const size_t n = m_T.size();
		for (;;)
		{
			while (m_Pos < n && isspace((unsigned char)m_T[m_Pos]))
				m_Pos++;
			if (m_Pos + 1 < n && m_T[m_Pos] == '/' && m_T[m_Pos + 1] == '/')
			{
				while (m_Pos < n && m_T[m_Pos] != '\n')
					m_Pos++;
				continue;
			}
			break;
		}
		if (m_Pos >= n || m_T[m_Pos] == 0)
			return tok;
		tok.ok = true;
		tok.beg = m_Pos;
		const char c = m_T[m_Pos];
		if (c == '"')
		{
			tok.quoted = true;
			size_t i = m_Pos + 1;
			while (i < n && m_T[i] != '"')
				i++;
			tok.text = m_T.substr(m_Pos + 1, (i < n ? i : n) - (m_Pos + 1));
			if (tok.text.size() > MAX_TOKEN)
				tok.text.resize(MAX_TOKEN);
			m_Pos = i < n ? i + 1 : n;
			tok.end = m_Pos;
			return tok;
		}
		if (c == '{' || c == '}')
		{
			tok.brace = true;
			tok.text.assign(1, c);
			m_Pos++;
			tok.end = m_Pos;
			return tok;
		}
		bool condStart = false;
		while (m_Pos < n)
		{
			const char d = m_T[m_Pos];
			if (d == 0 || d == '"' || d == '{' || d == '}' || isspace((unsigned char)d))
				break;
			if (d == '[')
				condStart = true;
			if (d == ']' && condStart)
				tok.conditional = true;
			if (tok.text.size() < MAX_TOKEN)
				tok.text += d;
			m_Pos++;
		}
		tok.end = m_Pos;
		return tok;
	}

	size_t Pos() const { return m_Pos; }
	void Seek(size_t p) { m_Pos = p; }

private:
	const std::string &m_T;
	size_t m_Pos = 0;
};

bool IsOpen(const Token &t) { return t.ok && t.brace && t.text[0] == '{'; }
bool IsClose(const Token &t) { return t.ok && t.brace && t.text[0] == '}'; }

// The start of the line holding pos when only spaces and tabs come before pos on it, else npos
size_t LineStartIfBlank(const std::string &t, size_t pos)
{
	size_t i = pos;
	while (i > 0 && (t[i - 1] == ' ' || t[i - 1] == '\t'))
		i--;
	return (i == 0 || t[i - 1] == '\n') ? i : std::string::npos;
}

// After pos: the spaces and tabs, then the line break (included) when nothing else is on the line, else pos
size_t LineEndIfBlank(const std::string &t, size_t pos)
{
	size_t i = pos;
	while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\r'))
		i++;
	if (i >= t.size())
		return i;
	return t[i] == '\n' ? i + 1 : pos;
}

std::string LeadingSpace(const std::string &t, size_t lineStart)
{
	size_t i = lineStart;
	while (i < t.size() && (t[i] == ' ' || t[i] == '\t'))
		i++;
	return t.substr(lineStart, i - lineStart);
}

std::string Quote(const std::string &s)
{
	return "\"" + s + "\"";
}
} // namespace

bool Doc::SetText(const std::string &text)
{
	if (text.size() > MAX_TEXT)
		return false;
	m_Text = text;
	Parse();
	return m_bRoot;
}

void Doc::Parse()
{
	m_Sections.clear();
	m_RootName.clear();
	m_bRoot = false;
	m_RootClose = std::string::npos;
	Reader r(m_Text);

	// top level: #include / #base lines, then the first section is the layout
	Token name;
	for (;;)
	{
		name = r.Next();
		if (!name.ok || name.text.empty())
			return;
		if (!name.quoted && (SameName(name.text, "#include") || SameName(name.text, "#base")))
		{
			r.Next();
			continue;
		}
		break;
	}
	Token open = r.Next();
	if (open.conditional)
		open = r.Next();
	if (!IsOpen(open))
		return;
	m_bRoot = true;
	m_RootName = name.text;

	// the layout's body: depth 1 = its own keys and the control sections, depth 2 = a control's keys, deeper: kept
	// as they are (KeyValues::RecursiveLoadFromBuffer's order of reads, stopping where it stops)
	int depth = 1;
	int current = -1; // the control section being read (depth >= 2)
	for (;;)
	{
		Token key = r.Next();
		if (!key.ok || (key.quoted && key.text.empty()) || (!key.quoted && key.text.empty()))
			return; // the end, or an error: KeyValues stops reading here too
		if (IsClose(key))
		{
			if (depth == 2 && current >= 0)
			{
				m_Sections[current].close = key.beg;
				m_Sections[current].end = key.end;
				current = -1;
			}
			if (depth == 1)
			{
				m_RootClose = key.beg;
				return;
			}
			depth--;
			continue;
		}
		Token value = r.Next();
		if (value.ok && value.conditional)
			value = r.Next();
		if (!value.ok || IsClose(value))
			return;
		if (IsOpen(value))
		{
			if (depth == 1)
			{
				Section s;
				s.name = key.text;
				s.nameBeg = key.beg;
				s.open = value.beg;
				s.close = std::string::npos;
				s.end = m_Text.size();
				m_Sections.push_back(s);
				current = (int)m_Sections.size() - 1;
			}
			depth++;
			continue;
		}
		if (depth == 2 && current >= 0)
		{
			Pair p;
			p.key = key.text;
			p.value = value.text;
			p.keyBeg = key.beg;
			p.valBeg = value.beg;
			p.valEnd = value.end;
			m_Sections[current].pairs.push_back(p);
		}
		// a conditional tag after the value belongs to it
		const size_t back = r.Pos();
		Token peek = r.Next();
		if (!peek.ok || !peek.conditional)
			r.Seek(back);
	}
}

int Doc::Find(const std::string &name) const
{
	for (int i = 0; i < Count(); i++)
		if (SameName(m_Sections[i].name, name.c_str()))
			return i;
	return -1;
}

const std::string *Doc::Get(int i, const char *key) const
{
	if (i < 0 || i >= Count())
		return nullptr;
	for (const Pair &p : m_Sections[i].pairs)
		if (SameName(p.key, key))
			return &p.value;
	return nullptr;
}

void Doc::Replace(size_t beg, size_t end, const std::string &with)
{
	if (beg > m_Text.size() || end > m_Text.size() || end < beg)
		return;
	m_Text.replace(beg, end - beg, with);
	Parse();
}

void Doc::Set(int i, const char *key, const std::string &value)
{
	if (i < 0 || i >= Count())
		return;
	const std::string v = CleanValue(value);
	const Section &s = m_Sections[i];
	for (const Pair &p : s.pairs)
		if (SameName(p.key, key))
		{
			Replace(p.valBeg, p.valEnd, Quote(v));
			return;
		}
	const std::string line = Quote(CleanValue(key)) + "\t\t" + Quote(v);
	if (s.close == std::string::npos)
	{
		Replace(m_Text.size(), m_Text.size(), "\n" + line + "\n");
		return;
	}
	const size_t closeLine = LineStartIfBlank(m_Text, s.close);
	if (closeLine == std::string::npos)
	{
		// the } shares its line with something else: the pair goes just before it
		Replace(s.close, s.close, " " + line + " ");
		return;
	}
	std::string indent;
	if (!s.pairs.empty())
	{
		size_t ls = LineStartIfBlank(m_Text, s.pairs.back().keyBeg);
		indent = ls != std::string::npos ? LeadingSpace(m_Text, ls) : "\t\t";
	}
	else
		indent = LeadingSpace(m_Text, closeLine) + "\t";
	Replace(closeLine, closeLine, indent + line + "\n");
}

void Doc::Remove(int i, const char *key)
{
	if (i < 0 || i >= Count())
		return;
	for (;;)
	{
		const Section &s = m_Sections[i];
		const Pair *found = nullptr;
		for (const Pair &p : s.pairs)
			if (SameName(p.key, key))
				found = &p;
		if (!found)
			return;
		size_t beg = found->keyBeg, end = found->valEnd;
		// a conditional tag right after it goes too
		Reader r(m_Text);
		r.Seek(end);
		Token peek = r.Next();
		if (peek.ok && peek.conditional)
			end = peek.end;
		const size_t ls = LineStartIfBlank(m_Text, beg);
		const size_t le = LineEndIfBlank(m_Text, end);
		if (ls != std::string::npos && le != end)
		{
			beg = ls;
			end = le;
		}
		const size_t before = m_Text.size();
		Replace(beg, end, "");
		if (m_Text.size() == before)
			return;
	}
}

int Doc::Add(const std::string &name, const std::vector<std::pair<std::string, std::string>> &pairs)
{
	if (!m_bRoot)
		return -1;
	std::string block = "\t" + Quote(CleanValue(name)) + "\n\t{\n";
	for (const auto &p : pairs)
		block += "\t\t" + Quote(CleanValue(p.first)) + "\t\t" + Quote(CleanValue(p.second)) + "\n";
	block += "\t}\n";
	size_t at = m_RootClose;
	if (at == std::string::npos)
	{
		// no closing } for the layout: the section goes at the end, and the layout is closed
		block += "}\n";
		at = m_Text.size();
		if (at > 0 && m_Text[at - 1] != '\n')
			block = "\n" + block;
		Replace(at, at, block);
	}
	else
	{
		const size_t ls = LineStartIfBlank(m_Text, at);
		if (ls != std::string::npos)
			Replace(ls, ls, block);
		else
			Replace(at, at, "\n" + block);
	}
	for (int i = Count() - 1; i >= 0; i--)
		if (SameName(m_Sections[i].name, name.c_str()))
			return i;
	return -1;
}

void Doc::Delete(int i)
{
	if (i < 0 || i >= Count())
		return;
	const Section &s = m_Sections[i];
	size_t beg = s.nameBeg, end = s.end;
	const size_t ls = LineStartIfBlank(m_Text, beg);
	if (ls != std::string::npos)
		beg = ls;
	end = LineEndIfBlank(m_Text, end);
	Replace(beg, end, "");
}

void Doc::Rename(int i, const std::string &name)
{
	if (i < 0 || i >= Count())
		return;
	const Section &s = m_Sections[i];
	Reader r(m_Text);
	r.Seek(s.nameBeg);
	Token t = r.Next();
	if (!t.ok)
		return;
	Replace(t.beg, t.end, Quote(CleanValue(name)));
	if (Get(i, "fieldName"))
		Set(i, "fieldName", name);
}

int Doc::MoveToEnd(int i)
{
	if (i < 0 || i >= Count() || i == Count() - 1 || m_RootClose == std::string::npos)
		return i;
	const Section &s = m_Sections[i];
	if (s.close == std::string::npos)
		return i;
	size_t beg = s.nameBeg;
	const size_t ls = LineStartIfBlank(m_Text, beg);
	if (ls != std::string::npos)
		beg = ls;
	size_t end = LineEndIfBlank(m_Text, s.end);
	std::string block = m_Text.substr(beg, end - beg);
	if (block.empty() || block.back() != '\n')
		block += "\n";
	const size_t rootClose = m_RootClose;
	// insert first (after the section: positions before it stay), then cut
	size_t at = LineStartIfBlank(m_Text, rootClose);
	if (at == std::string::npos)
	{
		at = rootClose;
		block = "\n" + block;
	}
	m_Text.insert(at, block);
	m_Text.erase(beg, end - beg);
	Parse();
	return Count() - 1;
}

int Doc::Move(int i, int to)
{
	if (i < 0 || i >= Count() || to < 0 || to >= Count() || i == to || m_RootClose == std::string::npos)
		return i;
	if (to == Count() - 1)
		return MoveToEnd(i);
	const Section &s = m_Sections[i];
	if (s.close == std::string::npos)
		return i;
	// the section's lines
	size_t beg = s.nameBeg;
	const size_t ls = LineStartIfBlank(m_Text, beg);
	if (ls != std::string::npos)
		beg = ls;
	const size_t end = LineEndIfBlank(m_Text, s.end);
	std::string block = m_Text.substr(beg, end - beg);
	if (block.empty() || block.back() != '\n')
		block += "\n";
	// where it goes: before the section that is at `to` now when moving up, after it when moving down
	const int anchor = to < i ? to : to + 1;
	const Section &a = m_Sections[anchor];
	size_t at = a.nameBeg;
	const size_t als = LineStartIfBlank(m_Text, at);
	if (als != std::string::npos)
		at = als;
	else
		block = "\n" + block;
	// insert and cut in the order that keeps the other position valid
	if (at > beg)
	{
		m_Text.insert(at, block);
		m_Text.erase(beg, end - beg);
	}
	else
	{
		m_Text.erase(beg, end - beg);
		m_Text.insert(at, block);
	}
	Parse();
	return to;
}

std::string Doc::UniqueName(const std::string &base) const
{
	if (Find(base) < 0)
		return base;
	for (int n = 2; n < 10000; n++)
	{
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", n);
		if (Find(base + buf) < 0)
			return base + buf;
	}
	return base;
}

std::string CleanValue(const std::string &value)
{
	std::string out;
	out.reserve(value.size());
	for (char c : value)
	{
		if (c == '"')
			c = '\'';
		else if (c == '\n' || c == '\r' || c == '\t')
			c = ' ';
		else if (c == 0)
			continue;
		out += c;
		if (out.size() >= MAX_TOKEN)
			break;
	}
	return out;
}

bool CleanLayoutName(const std::string &in, char *out, size_t outSize)
{
	if (!out || outSize == 0)
		return false;
	out[0] = 0;
	size_t b = 0, e = in.size();
	while (b < e && isspace((unsigned char)in[b]))
		b++;
	while (e > b && isspace((unsigned char)in[e - 1]))
		e--;
	std::string name = in.substr(b, e - b);
	if (name.size() > 4 && SameName(name.substr(name.size() - 4), ".res"))
		name.resize(name.size() - 4);
	if (name.empty() || name.size() > 31 || name.size() + 1 > outSize)
		return false;
	for (char c : name)
		if (!isalnum((unsigned char)c) && c != '_' && c != '-')
			return false;
	snprintf(out, outSize, "%s", name.c_str());
	return true;
}

std::string NewLayoutText(const std::string &name)
{
	const std::string path = "ui/scripts/screens/" + name + ".res";
	return "\"" + path + "\"\n"
	       "{\n"
	       "\t// Made with the world screen editor (type worldscreeneditorui in the console). To use it, set a "
	       "vgui_screen's Panel Name to " + name + "\n"
	       "\t\"WorldScreen\"\n"
	       "\t{\n"
	       "\t\t\"ControlName\"\t\t\"EditablePanel\"\n"
	       "\t\t\"fieldName\"\t\t\"WorldScreen\"\n"
	       "\t\t\"wide\"\t\t\"512\"\n"
	       "\t\t\"tall\"\t\t\"256\"\n"
	       "\t\t\"bgcolor_override\"\t\t\"12 22 16 255\"\n"
	       "\t}\n"
	       "\t\"Title\"\n"
	       "\t{\n"
	       "\t\t\"ControlName\"\t\t\"Label\"\n"
	       "\t\t\"fieldName\"\t\t\"Title\"\n"
	       "\t\t\"xpos\"\t\t\"24\"\n"
	       "\t\t\"ypos\"\t\t\"16\"\n"
	       "\t\t\"wide\"\t\t\"464\"\n"
	       "\t\t\"tall\"\t\t\"44\"\n"
	       "\t\t\"labelText\"\t\t\"NEW SCREEN\"\n"
	       "\t\t\"font\"\t\t\"WorldScreenTitle\"\n"
	       "\t\t\"fgcolor_override\"\t\t\"255 200 40 255\"\n"
	       "\t\t\"textAlignment\"\t\t\"west\"\n"
	       "\t}\n"
	       "}\n";
}
} // namespace wslayout
