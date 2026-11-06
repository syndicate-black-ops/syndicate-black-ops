//========= SBO 2.0: console colors (docs/CONSOLE_COLOR.md) =========//
//
// Purpose: The text side of the colored console: splitting a line into the runs between its color codes, and cutting
//          a run into console-sized pieces without splitting a UTF-8 character. No SDK or engine dependencies
//          (tests/console_color/ builds it alone).
//=================================================================//

#ifndef SBO_CONSOLE_TEXT_H
#define SBO_CONSOLE_TEXT_H

/**
 * Walks the terminated string s: onText(run, len) for each piece of text between color codes (len > 0), onCode(code,
 * len) for each code, in order. codeLen(p) is the length of the color code at p, 0 if there is none; it reads no
 * further than the terminator (GetColorCodeLength stops at it: the terminator is no hex digit).
 */
template <typename CodeLenFn, typename TextFn, typename CodeFn>
inline void SboConsole_ForEachRun(const char *s, CodeLenFn codeLen, TextFn onText, CodeFn onCode)
{
	const char *run = s;
	while (*s)
	{
		const int n = codeLen(s);
		if (n > 0)
		{
			if (s > run)
				onText(run, (int)(s - run));
			onCode(s, n);
			s += n;
			run = s;
		}
		else
			s++;
	}
	if (s > run)
		onText(run, (int)(s - run));
}

/**
 * How many of the len bytes at s fit in a piece of at most maxLen bytes without cutting a UTF-8 character: maxLen,
 * moved back to the start of the character it would cut. Text that is not UTF-8 (no lead byte found within 3 bytes)
 * is cut at maxLen. Always at least 1 when len > 0 and maxLen > 0.
 */
inline int SboConsole_PieceLength(const char *s, int len, int maxLen)
{
	if (len <= maxLen)
		return len;
	if (maxLen <= 0)
		return 0;
	int cut = maxLen;
	// s[cut] is the first byte of the next piece: back up while it is a continuation byte (10xxxxxx)
	for (int i = 0; i < 3 && cut > 1 && (((unsigned char)s[cut]) & 0xC0) == 0x80; i++)
		cut--;
	if ((((unsigned char)s[cut]) & 0xC0) == 0x80)
		return maxLen; // no lead byte close by: not UTF-8
	return cut;
}

#endif
