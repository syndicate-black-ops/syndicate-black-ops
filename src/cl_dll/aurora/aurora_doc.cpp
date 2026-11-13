// SBO 2.0: an Aurora script as the in-game particle editor edits it. See aurora_doc.h.

#include "aurora_doc.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace auroradoc
{
namespace
{
// case-insensitive compare (ASCII, as the game's stricmp)
int CaseCmp(const char *a, const char *b)
{
	for (;; a++, b++)
	{
		const int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
		if (ca != cb || !ca)
			return ca - cb;
	}
}

bool SameKey(const std::string &a, const char *b)
{
	return CaseCmp(a.c_str(), b) == 0;
}

// the characters that are a token on their own outside quotes (particlesys.cpp Aurora_IsBreak)
bool IsBreak(char c)
{
	return c == '{' || c == '}' || c == '(' || c == ')' || c == '\'' || c == ',' || c == ':';
}

struct Token
{
	size_t beg = 0, end = 0; // in the text, quotes included
	std::string text;
	bool quoted = false;
};

// One token by the game's rules (particlesys.cpp Aurora_Token); false at the end of the text
bool NextToken(const std::string &s, size_t &pos, Token &tok)
{
	const size_t n = s.size();
	for (;;)
	{
		while (pos < n && (signed char)s[pos] <= ' ')
			pos++;
		if (pos >= n)
			return false;
		if (s[pos] == '/' && pos + 1 < n && s[pos + 1] == '/')
		{
			while (pos < n && s[pos] != '\n')
				pos++;
			continue;
		}
		break;
	}
	tok = Token();
	tok.beg = pos;
	if (s[pos] == '"')
	{
		tok.quoted = true;
		pos++;
		while (pos < n && s[pos] != '"')
		{
			if (tok.text.size() < MAX_TOKEN)
				tok.text += s[pos];
			pos++;
		}
		if (pos < n)
			pos++;
		tok.end = pos;
		return true;
	}
	if (IsBreak(s[pos]))
	{
		tok.text = s[pos];
		tok.end = ++pos;
		return true;
	}
	do
	{
		if (tok.text.size() < MAX_TOKEN)
			tok.text += s[pos];
		pos++;
	} while (pos < n && (signed char)s[pos] > ' ' && !IsBreak(s[pos]));
	tok.end = pos;
	return true;
}

bool IsBrace(const Token &t, char c)
{
	return !t.quoted && t.text.size() == 1 && t.text[0] == c;
}

std::string Quote(const std::string &s)
{
	return "\"" + s + "\"";
}

size_t LineStart(const std::string &s, size_t pos)
{
	while (pos > 0 && s[pos - 1] != '\n')
		pos--;
	return pos;
}

bool OnlyBlanks(const std::string &s, size_t beg, size_t end)
{
	for (size_t i = beg; i < end; i++)
		if (s[i] != ' ' && s[i] != '\t' && s[i] != '\r')
			return false;
	return true;
}

// the line's end after pos: the position after its '\n' (or the text's end)
size_t LineEnd(const std::string &s, size_t pos)
{
	while (pos < s.size() && s[pos] != '\n')
		pos++;
	return pos < s.size() ? pos + 1 : pos;
}
} // namespace

void Doc::SetText(const std::string &text)
{
	m_Text = text.size() > MAX_TEXT ? text.substr(0, MAX_TEXT) : text;
	Parse();
}

// The structure as the game reads it (ParticleSystem::ParticleSystem, ParseType, Aurora_Value)
void Doc::Parse()
{
	m_Top.clear();
	m_Blocks.clear();
	size_t pos = 0;
	Token key, val;
	for (;;)
	{
		const size_t at = pos;
		if (!NextToken(m_Text, pos, key))
			break;
		if (IsBrace(key, '{'))
		{
			Block b;
			b.open = key.beg;
			b.close = std::string::npos;
			for (;;)
			{
				const size_t before = pos;
				if (!NextToken(m_Text, pos, key))
				{
					b.end = m_Text.size();
					break;
				}
				if (IsBrace(key, '}'))
				{
					b.close = key.beg;
					b.end = key.end;
					break;
				}
				if (IsBrace(key, '{'))
				{
					// no closing brace: the next type starts here
					pos = before;
					b.end = key.beg;
					break;
				}
				if (key.text.empty())
					continue;
				size_t vpos = pos;
				if (!NextToken(m_Text, vpos, val) || IsBrace(val, '{') || IsBrace(val, '}'))
					continue; // no value: the brace stays for the block structure
				pos = vpos;
				Pair p;
				p.key = key.text;
				p.value = val.text;
				p.keyBeg = key.beg;
				p.valBeg = val.beg;
				p.valEnd = val.end;
				b.pairs.push_back(p);
			}
			m_Blocks.push_back(b);
			continue;
		}
		(void)at;
		if (IsBrace(key, '}') || key.text.empty())
			continue;
		size_t vpos = pos;
		if (!NextToken(m_Text, vpos, val) || IsBrace(val, '{') || IsBrace(val, '}'))
			continue;
		pos = vpos;
		Pair p;
		p.key = key.text;
		p.value = val.text;
		p.keyBeg = key.beg;
		p.valBeg = val.beg;
		p.valEnd = val.end;
		m_Top.push_back(p);
	}
}

void Doc::Replace(size_t beg, size_t end, const std::string &with)
{
	if (beg > m_Text.size() || end > m_Text.size() || beg > end)
		return;
	m_Text.replace(beg, end - beg, with);
	Parse();
}

// a span out of the text; a line left with nothing else on it goes as a whole
void Doc::RemoveSpan(size_t beg, size_t end)
{
	const size_t ls = LineStart(m_Text, beg);
	size_t le = end;
	while (le < m_Text.size() && (m_Text[le] == ' ' || m_Text[le] == '\t' || m_Text[le] == '\r'))
		le++;
	if (OnlyBlanks(m_Text, ls, beg) && (le >= m_Text.size() || m_Text[le] == '\n'))
	{
		beg = ls;
		end = le < m_Text.size() ? le + 1 : le;
	}
	Replace(beg, end, "");
}

const std::string *Doc::Get(int block, const char *key) const
{
	if (block >= (int)m_Blocks.size())
		return nullptr;
	const std::vector<Pair> &pairs = PairsOf(block);
	for (size_t i = pairs.size(); i-- > 0;)
		if (SameKey(pairs[i].key, key))
			return &pairs[i].value;
	return nullptr;
}

void Doc::Set(int block, const char *key, const std::string &value)
{
	if (block >= (int)m_Blocks.size())
		return;
	const std::string v = Quote(CleanValue(value));
	const std::vector<Pair> &pairs = PairsOf(block);
	for (size_t i = pairs.size(); i-- > 0;)
		if (SameKey(pairs[i].key, key))
		{
			Replace(pairs[i].valBeg, pairs[i].valEnd, v);
			return;
		}
	const std::string pair = Quote(CleanValue(key)) + " " + v;
	if (block < 0)
	{
		if (!m_Top.empty())
		{
			// after the last top-level pair, on a line of its own
			Replace(m_Top.back().valEnd, m_Top.back().valEnd, "\n" + pair);
			return;
		}
		// before the first type (its line), or at the end
		const size_t at = m_Blocks.empty() ? m_Text.size() : LineStart(m_Text, m_Blocks[0].open);
		if (at == m_Text.size() && !m_Text.empty() && m_Text.back() != '\n')
			Replace(at, at, "\n" + pair + "\n");
		else
			Replace(at, at, pair + "\n" + (m_Blocks.empty() ? "" : "\n"));
		return;
	}
	const Block &b = m_Blocks[block];
	if (b.close == std::string::npos)
	{
		Replace(b.end, b.end, "\n\t" + pair + "\n");
		return;
	}
	const size_t ls = LineStart(m_Text, b.close);
	if (OnlyBlanks(m_Text, ls, b.close))
		Replace(ls, ls, "\t" + pair + "\n"); // the '}' on its own line: a new line before it
	else
		Replace(b.close, b.close, " " + pair + " ");
}

void Doc::Remove(int block, const char *key)
{
	for (int guard = 0; guard < 256; guard++)
	{
		if (block >= (int)m_Blocks.size())
			return;
		const std::vector<Pair> &pairs = PairsOf(block);
		size_t i = pairs.size();
		while (i-- > 0 && !SameKey(pairs[i].key, key))
			;
		if (i == (size_t)-1)
			return;
		RemoveSpan(pairs[i].keyBeg, pairs[i].valEnd);
	}
}

std::string Doc::TypeName(int i) const
{
	const std::string *name = Get(i, "name");
	return name ? *name : std::string();
}

int Doc::FindType(const std::string &name) const
{
	if (name.empty())
		return -1;
	for (int i = (int)m_Blocks.size(); i-- > 0;)
		if (CaseCmp(TypeName(i).c_str(), name.c_str()) == 0)
			return i;
	return -1;
}

int Doc::AddType(const std::string &name)
{
	std::string add;
	if (!m_Text.empty() && m_Text.back() != '\n')
		add += "\n";
	if (!m_Text.empty())
		add += "\n";
	add += "{\n\t\"name\" " + Quote(CleanValue(name)) + "\n}\n";
	Replace(m_Text.size(), m_Text.size(), add);
	return (int)m_Blocks.size() - 1;
}

void Doc::DeleteType(int i)
{
	if (i < 0 || i >= (int)m_Blocks.size())
		return;
	const Block &b = m_Blocks[i];
	size_t beg = b.open, end = b.end;
	const size_t ls = LineStart(m_Text, beg);
	if (OnlyBlanks(m_Text, ls, beg))
	{
		beg = ls;
		// and the blank line before it
		if (beg > 0)
		{
			const size_t prev = LineStart(m_Text, beg - 1);
			if (OnlyBlanks(m_Text, prev, beg - 1))
				beg = prev;
		}
	}
	if (b.close != std::string::npos)
	{
		const size_t le = LineEnd(m_Text, end);
		if (OnlyBlanks(m_Text, end, le > end && m_Text[le - 1] == '\n' ? le - 1 : le))
			end = le;
	}
	Replace(beg, end, "");
}

void Doc::SwapTypes(int i)
{
	if (i < 0 || i + 1 >= (int)m_Blocks.size())
		return;
	// each block from its line's start (when the '{' starts it) to its end
	auto chunk = [this](int k, size_t &beg, size_t &end) {
		const Block &b = m_Blocks[k];
		beg = b.open;
		const size_t ls = LineStart(m_Text, beg);
		if (OnlyBlanks(m_Text, ls, beg))
			beg = ls;
		end = b.end;
	};
	size_t ab, ae, bb, be;
	chunk(i, ab, ae);
	chunk(i + 1, bb, be);
	if (ae > bb)
		return;
	const std::string a = m_Text.substr(ab, ae - ab), mid = m_Text.substr(ae, bb - ae), b = m_Text.substr(bb, be - bb);
	Replace(ab, be, b + mid + a);
}

void Doc::RenameType(int i, const std::string &name)
{
	if (i < 0 || i >= (int)m_Blocks.size())
		return;
	const std::string old = TypeName(i);
	Set(i, "name", name);
	if (old.empty() || !CaseCmp(old.c_str(), name.c_str()))
		return;
	static const char *const refs[] = { "maintype", "spraytype", "overlaytype" };
	// every reference to the old name, one at a time (each change moves the text after it)
	for (int guard = 0; guard < 4096; guard++)
	{
		bool changed = false;
		for (int blk = -1; blk < (int)m_Blocks.size() && !changed; blk++)
			for (const Pair &p : PairsOf(blk))
			{
				bool ref = false;
				for (const char *r : refs)
					ref = ref || SameKey(p.key, r);
				if (ref && !CaseCmp(p.value.c_str(), old.c_str()))
				{
					Replace(p.valBeg, p.valEnd, Quote(CleanValue(name)));
					changed = true;
					break;
				}
			}
		if (!changed)
			break;
	}
}

std::string Doc::UniqueTypeName(const std::string &base) const
{
	if (FindType(base) < 0)
		return base;
	for (int n = 2; n < 10000; n++)
	{
		char s[64];
		snprintf(s, sizeof(s), "%s%d", base.c_str(), n);
		if (FindType(s) < 0)
			return s;
	}
	return base;
}

std::string CleanValue(const std::string &value)
{
	std::string out;
	for (char c : value)
	{
		if (c == '"' || c == '\n' || c == '\r' || c == 0)
			continue;
		if (out.size() >= MAX_TOKEN)
			break;
		out += c;
	}
	return out;
}

std::string FormatNumber(float value, int decimals)
{
	if (!(value > -1e30f && value < 1e30f))
		value = 0;
	if (decimals < 0)
		decimals = 0;
	if (decimals > 4)
		decimals = 4;
	char s[64];
	snprintf(s, sizeof(s), "%.*f", decimals, value);
	std::string t = s;
	if (t.find('.') != std::string::npos)
	{
		while (!t.empty() && t.back() == '0')
			t.pop_back();
		if (!t.empty() && t.back() == '.')
			t.pop_back();
	}
	if (t == "-0")
		t = "0";
	return t;
}

namespace
{
// the game's number (particlesys.cpp Aurora_Number): atof, out of range or not a number is 0
bool Number(const std::string &s, float &out)
{
	const char *p = s.c_str();
	while (*p == ' ' || *p == '\t')
		p++;
	char *end = nullptr;
	const double d = strtod(p, &end);
	out = (d > -1e30 && d < 1e30) ? (float)d : 0.0f;
	return end != p;
}
} // namespace

bool ParseRange(const std::string &text, float &min, float &max, bool &isRange)
{
	min = max = 0;
	isRange = false;
	const size_t dots = text.find("..");
	if (dots != std::string::npos)
	{
		const bool a = Number(text.substr(0, dots), min), b = Number(text.substr(dots + 2), max);
		isRange = true;
		return a || b;
	}
	const bool ok = Number(text, min);
	max = min;
	return ok;
}

bool CleanScriptName(const char *in, char *out, int outSize)
{
	if (!in || outSize < 2)
		return false;
	int n = 0;
	for (const char *p = in; *p; p++)
	{
		char c = *p;
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
		if (!ok || n >= 39 || n >= outSize - 1)
			return false;
		out[n++] = c;
	}
	out[n] = 0;
	return n > 0;
}

std::string NewScriptText()
{
	return "// Made with the particle editor (particleeditorui)\n"
	       "\"particles\" \"60\"\n"
	       "\"maintype\" \"source\"\n"
	       "\n"
	       "{\n"
	       "\t\"name\" \"source\"\n"
	       "\t\"spraytype\" \"puff\"\n"
	       "\t\"sprayrate\" \"20\"\n"
	       "\t\"sprayforce\" \"40..60\"\n"
	       "\t\"spraypitch\" \"-90\"\n"
	       "}\n"
	       "\n"
	       "{\n"
	       "\t\"name\" \"puff\"\n"
	       "\t\"sprite\" \"sprites/steam1.spr\"\n"
	       "\t\"rendermode\" \"additive\"\n"
	       "\t\"lifetime\" \"2\"\n"
	       "\t\"startsize\" \"4\"\n"
	       "\t\"endsize\" \"24\"\n"
	       "\t\"startalpha\" \"0.6\"\n"
	       "\t\"endalpha\" \"0\"\n"
	       "\t\"startangle\" \"0..360\"\n"
	       "\t\"angledelta\" \"-30..30\"\n"
	       "\t\"gravity\" \"20\"\n"
	       "}\n";
}
} // namespace auroradoc
