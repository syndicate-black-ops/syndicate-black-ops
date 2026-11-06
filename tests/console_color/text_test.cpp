// Offline test of the console color text splitter (src/cl_dll/vgui2/sbo_console_text.h, docs/CONSOLE_COLOR.md):
// runs between color codes, UTF-8-safe pieces, and random / hostile input under ASan/UBSan.
#include "sbo_console_text.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int s_fails = 0;
#define CHECK(c)                                                                 \
	do                                                                           \
	{                                                                            \
		if (!(c))                                                                \
		{                                                                        \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);         \
			s_fails++;                                                           \
		}                                                                        \
	} while (0)

static int Hex(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// The chat's rule (hud_chat.h GetColorCodeLength): ^0..^9, ^xRGB, ^#RRGGBB; reads no further than the terminator
static int CodeLen(const char *s)
{
	if (s[0] != '^')
		return 0;
	if (s[1] >= '0' && s[1] <= '9')
		return 2;
	if ((s[1] == 'x' || s[1] == 'X') && Hex(s[2]) && Hex(s[3]) && Hex(s[4]))
		return 5;
	if (s[1] == '#')
	{
		for (int i = 2; i < 8; i++)
			if (!Hex(s[i]))
				return 0;
		return 8;
	}
	return 0;
}

// "T:text" / "C:code" tokens
static std::vector<std::string> Split(const char *s)
{
	std::vector<std::string> out;
	SboConsole_ForEachRun(
		s, CodeLen, [&](const char *p, int n) { out.push_back("T:" + std::string(p, n)); },
		[&](const char *p, int n) { out.push_back("C:" + std::string(p, n)); });
	return out;
}

static void Expect(const char *s, std::vector<std::string> want)
{
	std::vector<std::string> got = Split(s);
	if (got != want)
	{
		fprintf(stderr, "FAIL split \"%s\":", s);
		for (auto &g : got)
			fprintf(stderr, " [%s]", g.c_str());
		fprintf(stderr, "\n");
		s_fails++;
	}
}

int main()
{
	// runs and codes
	Expect("", {});
	Expect("plain", { "T:plain" });
	Expect("^1red", { "C:^1", "T:red" });
	Expect("a^1b^0c", { "T:a", "C:^1", "T:b", "C:^0", "T:c" });
	Expect("^1^2", { "C:^1", "C:^2" });
	Expect("^x4AFname^0 rest", { "C:^x4AF", "T:name", "C:^0", "T: rest" });
	Expect("^#3FA2E1x", { "C:^#3FA2E1", "T:x" });
	// not codes: literal text
	Expect("^", { "T:^" });
	Expect("end^", { "T:end^" });
	Expect("^x4A", { "T:^x4A" });
	Expect("^#44AAF", { "T:^#44AAF" });
	Expect("^a^", { "T:^a^" });
	Expect("^10", { "C:^1", "T:0" });
	Expect("^x4AFF", { "C:^x4AF", "T:F" });

	// pieces: whole UTF-8 characters
	const char *u = "ab\xC3\xA9" "cd"; // a b e-acute c d
	CHECK(SboConsole_PieceLength(u, 6, 10) == 6);
	CHECK(SboConsole_PieceLength(u, 6, 3) == 2);  // would cut the e-acute after its lead byte
	CHECK(SboConsole_PieceLength(u, 6, 4) == 4);
	CHECK(SboConsole_PieceLength(u, 6, 0) == 0);
	const char *e = "\xE2\x82\xAC\xE2\x82\xAC"; // two euro signs
	CHECK(SboConsole_PieceLength(e, 6, 4) == 3);
	CHECK(SboConsole_PieceLength(e, 6, 1) == 1); // never 0: one byte at least
	const char *bad = "\x80\x80\x80\x80\x80\x80"; // continuation bytes only
	CHECK(SboConsole_PieceLength(bad, 6, 4) == 4);

	// random / hostile input: every byte lands in exactly one run or code, in order; pieces stay in bounds
	srand(12345);
	const char alphabet[] = "^^^^x#X0123456789abcdefABCDEFg \xC3\xA9\x80\xE2";
	for (int iter = 0; iter < 200000; iter++)
	{
		int len = rand() % 40;
		char *buf = (char *)malloc(len + 1); // exact size: ASan sees any read past the terminator
		for (int i = 0; i < len; i++)
			buf[i] = alphabet[rand() % (sizeof(alphabet) - 1)];
		buf[len] = 0;
		std::string joined;
		SboConsole_ForEachRun(
			buf, CodeLen,
			[&](const char *p, int n) {
				CHECK(n > 0);
				int max = 1 + rand() % 8;
				while (n > 0)
				{
					int k = SboConsole_PieceLength(p, n, max);
					CHECK(k >= 1 && k <= n && k <= max);
					joined.append(p, k);
					p += k;
					n -= k;
				}
			},
			[&](const char *p, int n) {
				CHECK(n == CodeLen(p));
				joined.append(p, n);
			});
		CHECK(joined == std::string(buf, len));
		free(buf);
		if (s_fails > 20)
			break;
	}

	if (s_fails)
	{
		fprintf(stderr, "%d failure(s)\n", s_fails);
		return 1;
	}
	printf("PASS\n");
	return 0;
}
