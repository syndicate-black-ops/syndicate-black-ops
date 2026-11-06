//========= SBO: HTML MOTD and map briefings, text checks =========//
//
// Purpose: what decides whether a MOTD is shown as a web page (docs/HTML_MOTD.md), in plain C++ (no SDK or SBO
//          headers) so tests/html_motd/run_test.sh can build it alone with ASan/UBSan.
//            * IsURL:  the MOTD is a web address (Counter-Strike 1.6: the MOTD window loads that page)
//            * IsHtml: the MOTD is HTML (it starts with "<!DOCTYPE html" or "<html", as an HTML MOTD written for
//                      Counter-Strike 1.6 does)
//          Anything else is plain text, shown as before.
//=================================================================//

#ifndef SBO_VGUI2_SBO_HTML_H
#define SBO_VGUI2_SBO_HTML_H

#include <stddef.h>

namespace sbohtml
{
//! Skips an UTF-8 byte order mark and white space (space, tab, CR, LF, FF, VT).
inline const char *SkipLeadingSpace(const char *text)
{
	if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
		text += 3;
	while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n' || *text == '\f' || *text == '\v')
		text++;
	return text;
}

//! True if text starts with prefix, ignoring ASCII case. Stops at text's terminating 0.
inline bool StartsWithNoCase(const char *text, const char *prefix)
{
	for (; *prefix; text++, prefix++)
	{
		char a = *text, b = *prefix;
		if (a >= 'A' && a <= 'Z')
			a = (char)(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z')
			b = (char)(b - 'A' + 'a');
		if (a != b) // also the end of text (a == 0, b != 0)
			return false;
	}
	return true;
}

//! True if text (after a BOM and white space) starts with "http://" or "https://" and has a host after it.
inline bool IsURL(const char *text)
{
	if (!text)
		return false;
	text = SkipLeadingSpace(text);
	size_t n;
	if (StartsWithNoCase(text, "https://"))
		n = 8;
	else if (StartsWithNoCase(text, "http://"))
		n = 7;
	else
		return false;
	char c = text[n];
	return c != 0 && c != '/' && c != ' ' && c != '\t' && c != '\r' && c != '\n';
}

//! True if text (after a BOM and white space) starts with "<!DOCTYPE html" or with "<html" followed by '>', '/'
//! or white space (ASCII case ignored).
inline bool IsHtml(const char *text)
{
	if (!text)
		return false;
	text = SkipLeadingSpace(text);
	if (StartsWithNoCase(text, "<!doctype html"))
		return true;
	if (!StartsWithNoCase(text, "<html"))
		return false;
	char c = text[5];
	return c == '>' || c == '/' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

//! The URL to give the browser: text without its leading BOM / white space and without trailing white space,
//! copied into out (always 0-terminated). False (out = "") if it doesn't fit.
inline bool CopyURL(const char *text, char *out, size_t outSize)
{
	if (!out || outSize == 0)
		return false;
	out[0] = 0;
	if (!text)
		return false;
	text = SkipLeadingSpace(text);
	size_t len = 0;
	while (text[len])
		len++;
	while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t' || text[len - 1] == '\r' || text[len - 1] == '\n'))
		len--;
	if (len + 1 > outSize)
		return false;
	for (size_t i = 0; i < len; i++)
		out[i] = text[i];
	out[len] = 0;
	return true;
}
} // namespace sbohtml

#endif // SBO_VGUI2_SBO_HTML_H
