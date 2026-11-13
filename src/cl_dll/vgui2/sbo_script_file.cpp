//========= SBO VGUI2: GameUI's option script files (settings.scr, user.scr) =========//
//
// Purpose: see sbo_script_file.h.
//=================================================================//

#include "sbo_script_file.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace sbo_script
{
namespace
{
// GameUI's header texts (HL25 gameui.so 0x141ac4 / 0x14c0d0 and the lines after them), CRLF as it writes them
const char *const HINT_HEAD =
    "// NOTE:  THIS FILE IS AUTOMATICALLY REGENERATED, \r\n"
    "//DO NOT EDIT THIS HEADER, YOUR COMMENTS WILL BE LOST IF YOU DO\r\n";
const char *const HINT_BODY =
    "//\r\n"
    "// Format:\r\n"
    "//  Version [float]\r\n"
    "//  Options description followed by \r\n"
    "//  Options defaults\r\n"
    "//\r\n"
    "// Option description syntax:\r\n"
    "//\r\n"
    "//  \"cvar\" { \"Prompt\" { type [ type info ] } { default } }\r\n"
    "//\r\n"
    "//  type = \r\n"
    "//   BOOL   (a yes/no toggle)\r\n"
    "//   STRING\r\n"
    "//   NUMBER\r\n"
    "//   LIST\r\n"
    "//\r\n"
    "// type info:\r\n"
    "// BOOL                 no type info\r\n"
    "// NUMBER       min max range, use -1 -1 for no limits\r\n"
    "// STRING       no type info\r\n"
    "// LIST          delimited list of options value pairs\r\n"
    "//\r\n"
    "//\r\n"
    "// default depends on type\r\n"
    "// BOOL is \"0\" or \"1\"\r\n"
    "// NUMBER is \"value\"\r\n"
    "// STRING is \"value\"\r\n"
    "// LIST is \"index\", where index \"0\" is the first element of the list\r\n"
    "\r\n"
    "\r\n";

//! Tokens of the script: '{', '}', a quoted string or a bare word; // comments skipped
class Lexer
{
public:
	Lexer(const char *data, size_t size)
	    : m_p(data)
	    , m_end(data + size)
	{
	}

	//! Next token into out; false at the end. quoted: it was a quoted string
	bool Next(std::string &out, bool *quoted = nullptr)
	{
		out.clear();
		if (quoted)
			*quoted = false;
		for (;;)
		{
			while (m_p < m_end && (unsigned char)*m_p <= ' ')
				m_p++;
			if (m_p + 1 < m_end && m_p[0] == '/' && m_p[1] == '/')
			{
				while (m_p < m_end && *m_p != '\n')
					m_p++;
				continue;
			}
			break;
		}
		if (m_p >= m_end)
			return false;
		if (*m_p == '{' || *m_p == '}')
		{
			out.assign(1, *m_p++);
			return true;
		}
		if (*m_p == '"')
		{
			if (quoted)
				*quoted = true;
			m_p++;
			while (m_p < m_end && *m_p != '"')
			{
				if (out.size() < Description::MAX_STRING)
					out += *m_p;
				m_p++;
			}
			if (m_p < m_end)
				m_p++; // closing quote
			return true;
		}
		while (m_p < m_end && (unsigned char)*m_p > ' ' && *m_p != '{' && *m_p != '}' && *m_p != '"')
		{
			if (out.size() < Description::MAX_STRING)
				out += *m_p;
			m_p++;
		}
		return true;
	}

private:
	const char *m_p;
	const char *m_end;
};

bool SameNoCase(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return false;
	}
	return *a == *b;
}

bool IsWord(const std::string &tok, bool quoted, const char *word)
{
	return !quoted && !strcmp(tok.c_str(), word);
}

std::string FormatFloat(const char *fmt, double v)
{
	char buf[64];
	snprintf(buf, sizeof(buf), fmt, v);
	return buf;
}

//! GameUI's float of a value text (strtod; empty or junk = 0)
double ToNumber(const std::string &s)
{
	return strtod(s.c_str(), nullptr);
}

//! One object after its cvar name: { "prompt" { type ... } { "default" } [SetInfo] }
bool ParseObject(Lexer &lex, Object &obj)
{
	std::string tok;
	bool quoted = false;
	if (!lex.Next(tok) || tok != "{")
		return false;
	if (!lex.Next(tok, &quoted) || !quoted)
		return false;
	obj.prompt = tok;
	if (!lex.Next(tok) || tok != "{")
		return false;
	if (!lex.Next(tok, &quoted) || quoted)
		return false;
	if (tok == "BOOL")
		obj.type = TYPE_BOOL;
	else if (tok == "NUMBER")
		obj.type = TYPE_NUMBER;
	else if (tok == "STRING")
		obj.type = TYPE_STRING;
	else if (tok == "LIST")
		obj.type = TYPE_LIST;
	else
		return false;
	if (obj.type == TYPE_NUMBER)
	{
		std::string a, b;
		if (!lex.Next(a) || !lex.Next(b))
			return false;
		obj.fMin = (float)ToNumber(a);
		obj.fMax = (float)ToNumber(b);
	}
	if (obj.type == TYPE_LIST)
	{
		// "label" "value" pairs up to the closing brace
		for (;;)
		{
			if (!lex.Next(tok, &quoted))
				return false;
			if (!quoted && tok == "}")
				break;
			ListItem item;
			item.label = tok;
			if (!lex.Next(tok, &quoted) || (!quoted && tok == "}"))
				return false;
			item.value = tok;
			if (obj.items.size() < Description::MAX_ITEMS)
				obj.items.push_back(item);
		}
	}
	else if (!lex.Next(tok) || tok != "}")
		return false;
	// { "default" }
	if (!lex.Next(tok) || tok != "{")
		return false;
	if (!lex.Next(tok, &quoted))
		return false;
	if (!quoted && tok == "}")
		obj.defValue.clear();
	else
	{
		obj.defValue = tok;
		if (!lex.Next(tok) || tok != "}")
			return false;
	}
	obj.curValue = obj.defValue;
	// [SetInfo] }
	for (int guard = 0; guard < 4; guard++)
	{
		if (!lex.Next(tok, &quoted))
			return false;
		if (IsWord(tok, quoted, "}"))
			return true;
		if (IsWord(tok, quoted, "SetInfo"))
			obj.setInfo = true;
	}
	return false;
}
} // namespace

std::string StripInvalid(const std::string &s)
{
	std::string out;
	for (char c : s)
	{
		if (c != '"' && c != '%')
			out += c;
	}
	return out;
}

std::string NumberText(const std::string &value)
{
	char *end = nullptr;
	const double d = strtod(value.c_str(), &end);
	if (value.empty() || !end || *end)
		return value;
	std::string s = FormatFloat("%f", d);
	while (!s.empty() && s.back() == '0')
		s.pop_back();
	if (!s.empty() && s.back() == '.')
		s.pop_back();
	return s == "-0" ? "0" : s;
}

void Object::SetCurValue(const std::string &value)
{
	curValue = StripInvalid(value); // GameUI's GatherCurrentValues strips the value first
	if (type != TYPE_BOOL && type != TYPE_NUMBER)
		return;
	// GameUI: the number's text with trailing zeros and a trailing '.' trimmed
	std::string s = FormatFloat("%f", ToNumber(value));
	if (s.find('.') != std::string::npos)
	{
		while (!s.empty() && s.back() == '0')
			s.pop_back();
		if (!s.empty() && s.back() == '.')
			s.pop_back();
	}
	curValue = s;
}

int Object::ListRow() const
{
	if (items.empty())
		return -1;
	double d = ToNumber(defValue);
	int row = d > 0.0 && d < 100000.0 ? (int)d : 0;
	if (row >= (int)items.size())
		row = (int)items.size() - 1;
	return row;
}

std::string Object::ConfigCommand() const
{
	const std::string name = StripInvalid(cvar);
	return (setInfo ? "setinfo " : "") + name + " \"" + ConfigValue() + "\"\n";
}

std::string Object::ConfigValue() const
{
	std::string value;
	switch (type)
	{
	case TYPE_BOOL:
		value = ToNumber(curValue) != 0.0 ? "1" : "0";
		break;
	case TYPE_NUMBER:
	{
		double v = ToNumber(curValue);
		if (fMin != -1.0f && v < fMin)
			v = fMin;
		if (fMax != -1.0f && v > fMax)
			v = fMax;
		value = FormatFloat("%f", v);
		break;
	}
	case TYPE_STRING:
		value = StripInvalid(curValue);
		break;
	case TYPE_LIST:
	{
		const double d = ToNumber(curValue);
		const int index = d >= 0.0 && d < 100000.0 ? (int)d : -1;
		value = index >= 0 && index < (int)items.size() ? items[index].value : "0.0";
		break;
	}
	}
	return value;
}

bool Description::Parse(const char *data, size_t size, Kind expected)
{
	objects.clear();
	kind = expected;
	if (!data)
		return false;
	Lexer lex(data, size);
	std::string tok;
	bool quoted = false;
	// VERSION 1.0
	if (!lex.Next(tok, &quoted) || !IsWord(tok, quoted, "VERSION"))
		return false;
	if (!lex.Next(tok) || ToNumber(tok) != 1.0)
		return false;
	// DESCRIPTION <type> {
	if (!lex.Next(tok, &quoted) || !IsWord(tok, quoted, "DESCRIPTION"))
		return false;
	if (!lex.Next(tok, &quoted) || tok != (expected == KIND_SERVER ? "SERVER_OPTIONS" : "INFO_OPTIONS"))
		return false;
	if (!lex.Next(tok) || tok != "{")
		return false;
	for (;;)
	{
		if (!lex.Next(tok, &quoted))
			return false; // no closing brace
		if (!quoted && tok == "}")
			return true;
		if (!quoted)
			return false;
		Object obj;
		obj.cvar = tok;
		if (!ParseObject(lex, obj))
			return false;
		if (objects.size() < MAX_OBJECTS && !obj.cvar.empty())
			objects.push_back(obj);
	}
}

Object *Description::Find(const char *cvar)
{
	for (Object &o : objects)
	{
		if (SameNoCase(o.cvar.c_str(), cvar))
			return &o;
	}
	return nullptr;
}

std::string Description::Write(const std::string &timeText) const
{
	std::string out = HINT_HEAD;
	out += kind == KIND_SERVER ? "// Multiplayer options script\r\n" : "// User options script\r\n";
	out += HINT_BODY;
	out += kind == KIND_SERVER
	           ? "// Half-Life Server Configuration Layout Script (stores last settings chosen, too)\r\n"
	           : "// Half-Life User Info Configuration Layout Script (stores last settings chosen, too)\r\n";
	out += "// File generated:  " + timeText + "\r\n";
	out += "//\r\n//\r\n// Cvar\t-\tSetting\r\n\r\n";
	out += "VERSION 1.0\r\n\r\n";
	out += kind == KIND_SERVER ? "DESCRIPTION SERVER_OPTIONS\r\n{\r\n" : "DESCRIPTION INFO_OPTIONS\r\n{\r\n";
	for (const Object &o : objects)
	{
		out += "\t\"" + StripInvalid(o.cvar) + "\"\r\n\t{\r\n";
		out += "\t\t\"" + StripInvalid(o.prompt) + "\"\r\n";
		switch (o.type)
		{
		case TYPE_BOOL:
			out += "\t\t{ BOOL }\r\n";
			out += FormatFloat("\t\t{ \"%.0f\" }\r\n", ToNumber(o.curValue) != 0.0 ? 1.0 : 0.0);
			break;
		case TYPE_NUMBER:
		{
			char buf[96];
			snprintf(buf, sizeof(buf), "\t\t{ NUMBER %f %f }\r\n", o.fMin, o.fMax);
			out += buf;
			out += FormatFloat("\t\t{ \"%f\" }\r\n", ToNumber(o.curValue));
			break;
		}
		case TYPE_STRING:
			out += "\t\t{ STRING }\r\n";
			out += "\t\t{ \"" + o.curValue + "\" }\r\n";
			break;
		case TYPE_LIST:
			out += "\t\t{\r\n\t\t\tLIST\r\n";
			for (const ListItem &item : o.items)
				out += "\t\t\t\"" + item.label + "\" \"" + item.value + "\"\r\n";
			out += "\t\t}\r\n";
			out += FormatFloat("\t\t{ \"%f\" }\r\n", ToNumber(o.curValue));
			break;
		}
		if (o.setInfo)
			out += "\t\tSetInfo\r\n";
		out += "\t}\r\n\r\n";
	}
	out += "}\r\n";
	return out;
}

std::string FileTime(long long t, Description::Kind kind)
{
	time_t tt = (time_t)t;
	struct tm tmv;
#ifdef _WIN32
	localtime_s(&tmv, &tt);
#else
	localtime_r(&tt, &tmv);
#endif
	const char *ampm = "AM";
	if (kind == Description::KIND_SERVER)
	{
		// CServerDescription: 12-hour clock
		if (tmv.tm_hour > 12)
		{
			tmv.tm_hour -= 12;
			ampm = "PM";
		}
		else if (tmv.tm_hour == 0)
			tmv.tm_hour = 12;
	}
	// asctime's first 19 characters: "Sun Nov 01 21:05:09"
	static const char *const days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char *const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	char buf[64];
	snprintf(buf, sizeof(buf), "%.3s %.3s%3d %.2d:%.2d:%.2d %s", days[(tmv.tm_wday % 7 + 7) % 7],
	    months[(tmv.tm_mon % 12 + 12) % 12], tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ampm);
	return buf;
}
bool ConfigValue(const char *data, size_t size, const char *name, std::string &value)
{
	if (!data || !name || !name[0])
		return false;
	bool found = false;
	size_t i = 0;
	while (i < size)
	{
		// one command: its tokens up to a line break or a ';' outside quotes
		std::string tokens[2];
		int count = 0;
		while (i < size && data[i] != '\n' && data[i] != ';')
		{
			const char c = data[i];
			if (c == ' ' || c == '\t' || c == '\r' || c == '\0')
			{
				++i;
				continue;
			}
			if (c == '/' && i + 1 < size && data[i + 1] == '/')
			{
				while (i < size && data[i] != '\n')
					++i;
				break;
			}
			std::string token;
			if (c == '"')
			{
				++i;
				while (i < size && data[i] != '"' && data[i] != '\n')
				{
					if (token.size() < Description::MAX_STRING)
						token += data[i];
					++i;
				}
				if (i < size && data[i] == '"')
					++i;
			}
			else
			{
				while (i < size && data[i] != ' ' && data[i] != '\t' && data[i] != '\r' && data[i] != '\n' &&
				       data[i] != ';' && data[i] != '"' && data[i] != '\0')
				{
					if (token.size() < Description::MAX_STRING)
						token += data[i];
					++i;
				}
			}
			if (count < 2)
				tokens[count] = token;
			++count;
		}
		if (i < size)
			++i; // the line break or ';'
		bool same = count >= 2 && tokens[0].size() == strlen(name);
		for (size_t k = 0; same && k < tokens[0].size(); ++k)
			same = tolower((unsigned char)tokens[0][k]) == tolower((unsigned char)name[k]);
		if (same)
		{
			value = tokens[1];
			found = true;
		}
	}
	return found;
}
} // namespace sbo_script
