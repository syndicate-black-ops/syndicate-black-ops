//========= SBO: updater, JSON reader =========//
//
// Purpose: see updater_json.h.
//=============================================//

#include "updater_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace sbo_update
{
namespace json
{
namespace
{
struct Parser
{
	const char *begin;
	const char *p;
	const char *end;
	std::string err;

	bool Fail(const char *what)
	{
		if (err.empty())
		{
			char buf[96];
			snprintf(buf, sizeof(buf), "%s at offset %ld", what, (long)(p - begin));
			err = buf;
		}
		return false;
	}

	void SkipSpace()
	{
		while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;
	}

	bool Literal(const char *word)
	{
		const size_t n = strlen(word);
		if ((size_t)(end - p) < n || memcmp(p, word, n) != 0)
			return Fail("bad literal");
		p += n;
		return true;
	}

	static int Hex(char c)
	{
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		return -1;
	}

	bool Hex4(unsigned &out)
	{
		if (end - p < 4)
			return Fail("short \\u escape");
		out = 0;
		for (int i = 0; i < 4; i++)
		{
			const int h = Hex(p[i]);
			if (h < 0)
				return Fail("bad \\u escape");
			out = out * 16 + (unsigned)h;
		}
		p += 4;
		return true;
	}

	static void Utf8(std::string &s, unsigned cp)
	{
		if (cp < 0x80)
			s += (char)cp;
		else if (cp < 0x800)
		{
			s += (char)(0xC0 | (cp >> 6));
			s += (char)(0x80 | (cp & 0x3F));
		}
		else if (cp < 0x10000)
		{
			s += (char)(0xE0 | (cp >> 12));
			s += (char)(0x80 | ((cp >> 6) & 0x3F));
			s += (char)(0x80 | (cp & 0x3F));
		}
		else
		{
			s += (char)(0xF0 | (cp >> 18));
			s += (char)(0x80 | ((cp >> 12) & 0x3F));
			s += (char)(0x80 | ((cp >> 6) & 0x3F));
			s += (char)(0x80 | (cp & 0x3F));
		}
	}

	bool String(std::string &s)
	{
		// at '"'
		p++;
		for (;;)
		{
			if (p >= end)
				return Fail("unterminated string");
			const unsigned char c = (unsigned char)*p;
			if (c == '"')
			{
				p++;
				return true;
			}
			if (c < 0x20)
				return Fail("control character in string");
			if (c != '\\')
			{
				s += (char)c;
				p++;
				continue;
			}
			p++;
			if (p >= end)
				return Fail("unterminated escape");
			const char e = *p++;
			switch (e)
			{
			case '"': s += '"'; break;
			case '\\': s += '\\'; break;
			case '/': s += '/'; break;
			case 'b': s += '\b'; break;
			case 'f': s += '\f'; break;
			case 'n': s += '\n'; break;
			case 'r': s += '\r'; break;
			case 't': s += '\t'; break;
			case 'u':
			{
				unsigned cp;
				if (!Hex4(cp))
					return false;
				if (cp >= 0xD800 && cp <= 0xDBFF)
				{
					unsigned lo;
					if (end - p < 2 || p[0] != '\\' || p[1] != 'u')
						return Fail("lone surrogate");
					p += 2;
					if (!Hex4(lo))
						return false;
					if (lo < 0xDC00 || lo > 0xDFFF)
						return Fail("bad surrogate pair");
					cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
				}
				else if (cp >= 0xDC00 && cp <= 0xDFFF)
					return Fail("lone surrogate");
				Utf8(s, cp);
				break;
			}
			default: return Fail("bad escape");
			}
		}
	}

	bool Number(Value &v)
	{
		const char *start = p;
		if (p < end && *p == '-')
			p++;
		if (p >= end || *p < '0' || *p > '9')
			return Fail("bad number");
		if (*p == '0')
			p++;
		else
			while (p < end && *p >= '0' && *p <= '9')
				p++;
		bool isInt = true;
		if (p < end && *p == '.')
		{
			isInt = false;
			p++;
			if (p >= end || *p < '0' || *p > '9')
				return Fail("bad number");
			while (p < end && *p >= '0' && *p <= '9')
				p++;
		}
		if (p < end && (*p == 'e' || *p == 'E'))
		{
			isInt = false;
			p++;
			if (p < end && (*p == '+' || *p == '-'))
				p++;
			if (p >= end || *p < '0' || *p > '9')
				return Fail("bad number");
			while (p < end && *p >= '0' && *p <= '9')
				p++;
		}
		const size_t len = (size_t)(p - start);
		if (len > 40)
			return Fail("number too long");
		char buf[48];
		memcpy(buf, start, len);
		buf[len] = 0;
		v.type = Value::Number;
		v.n = strtod(buf, nullptr);
		// integers: digit by digit (no locale, exact), up to 2^53
		if (isInt)
		{
			const char *d = buf[0] == '-' ? buf + 1 : buf;
			int64_t x = 0;
			bool ok = strlen(d) <= 16;
			for (; ok && *d; d++)
				x = x * 10 + (*d - '0');
			if (ok && x <= ((int64_t)1 << 53))
			{
				v.isInt = true;
				v.i = buf[0] == '-' ? -x : x;
				v.n = (double)v.i;
			}
		}
		return true;
	}

	bool Parse(Value &v, int depth)
	{
		if (depth > MAX_DEPTH)
			return Fail("nested too deep");
		SkipSpace();
		if (p >= end)
			return Fail("unexpected end");
		switch (*p)
		{
		case '{':
		{
			v.type = Value::Object;
			p++;
			SkipSpace();
			if (p < end && *p == '}')
			{
				p++;
				return true;
			}
			for (;;)
			{
				SkipSpace();
				if (p >= end || *p != '"')
					return Fail("expected key");
				std::string key;
				if (!String(key))
					return false;
				SkipSpace();
				if (p >= end || *p != ':')
					return Fail("expected ':'");
				p++;
				v.o.emplace_back(std::move(key), Value());
				if (!Parse(v.o.back().second, depth + 1))
					return false;
				SkipSpace();
				if (p < end && *p == ',')
				{
					p++;
					continue;
				}
				if (p < end && *p == '}')
				{
					p++;
					return true;
				}
				return Fail("expected ',' or '}'");
			}
		}
		case '[':
		{
			v.type = Value::Array;
			p++;
			SkipSpace();
			if (p < end && *p == ']')
			{
				p++;
				return true;
			}
			for (;;)
			{
				v.a.emplace_back();
				if (!Parse(v.a.back(), depth + 1))
					return false;
				SkipSpace();
				if (p < end && *p == ',')
				{
					p++;
					continue;
				}
				if (p < end && *p == ']')
				{
					p++;
					return true;
				}
				return Fail("expected ',' or ']'");
			}
		}
		case '"':
			v.type = Value::String;
			return String(v.s);
		case 't':
			v.type = Value::Bool;
			v.b = true;
			return Literal("true");
		case 'f':
			v.type = Value::Bool;
			v.b = false;
			return Literal("false");
		case 'n':
			v.type = Value::Null;
			return Literal("null");
		default:
			return Number(v);
		}
	}
};
} // namespace

const Value *Value::Get(const char *key) const
{
	if (type != Object)
		return nullptr;
	for (const auto &kv : o)
		if (kv.first == key)
			return &kv.second;
	return nullptr;
}

std::string Value::Str(const char *key, const char *def) const
{
	const Value *v = Get(key);
	return v && v->type == String ? v->s : std::string(def);
}

bool Value::Flag(const char *key, bool def) const
{
	const Value *v = Get(key);
	return v && v->type == Value::Bool ? v->b : def;
}

int64_t Value::Int(const char *key, int64_t def) const
{
	const Value *v = Get(key);
	return v && v->type == Number && v->isInt ? v->i : def;
}

bool Parse(const char *text, size_t len, Value &out, std::string &err)
{
	out = Value();
	Parser ps{text, text, text + len, std::string()};
	// a UTF-8 byte order mark
	if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
		ps.p += 3;
	bool ok = ps.Parse(out, 0);
	if (ok)
	{
		ps.SkipSpace();
		if (ps.p != ps.end)
			ok = ps.Fail("trailing data");
	}
	if (!ok)
	{
		err = ps.err;
		out = Value();
	}
	return ok;
}

std::string Quote(const std::string &s)
{
	std::string out = "\"";
	for (unsigned char c : s)
	{
		if (c == '"' || c == '\\')
		{
			out += '\\';
			out += (char)c;
		}
		else if (c < 0x20)
		{
			char buf[8];
			snprintf(buf, sizeof(buf), "\\u%04x", c);
			out += buf;
		}
		else
			out += (char)c;
	}
	out += '"';
	return out;
}
} // namespace json
} // namespace sbo_update
