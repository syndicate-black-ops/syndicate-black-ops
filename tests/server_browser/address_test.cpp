// Unit and random-input test of sbo_server_address.cpp (run_test.sh)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbo_server_address.h"

static int s_iFailed = 0;

static void Check(bool ok, const char *what)
{
	if (!ok)
	{
		printf("FAIL: %s\n", what);
		s_iFailed++;
	}
}

static void ExpectSplit(const char *text, bool ok, const char *host, unsigned port)
{
	char h[64];
	uint16_t p = 0;
	const bool r = sbo_address::Split(text, h, sizeof(h), p);
	char what[256];
	snprintf(what, sizeof(what), "Split(\"%s\")", text);
	Check(r == ok, what);
	if (r && ok)
	{
		Check(!strcmp(h, host), what);
		Check(p == port, what);
	}
}

static void ExpectIP(const char *text, bool ok, uint32_t ip)
{
	uint32_t v = 0;
	const bool r = sbo_address::ParseIPv4(text, v);
	char what[256];
	snprintf(what, sizeof(what), "ParseIPv4(\"%s\")", text);
	Check(r == ok, what);
	if (r && ok)
		Check(v == ip, what);
}

int main()
{
	ExpectSplit("203.0.113.148:27060", true, "203.0.113.148", 27060);
	ExpectSplit("  example.org  ", true, "example.org", 27015);
	ExpectSplit("example.org:1", true, "example.org", 1);
	ExpectSplit("example.org:65535", true, "example.org", 65535);
	ExpectSplit("example.org:65536", false, "", 0);
	ExpectSplit("example.org:0", false, "", 0);
	ExpectSplit("example.org:", false, "", 0);
	ExpectSplit("example.org:27015x", false, "", 0);
	ExpectSplit("example.org:270150", false, "", 0);
	ExpectSplit("a b", false, "", 0);
	ExpectSplit("", false, "", 0);
	ExpectSplit(":27015", false, "", 0);
	{
		// a host longer than the buffer fails instead of being cut
		char longHost[200];
		memset(longHost, 'a', sizeof(longHost) - 1);
		longHost[sizeof(longHost) - 1] = 0;
		ExpectSplit(longHost, false, "", 0);
	}

	ExpectIP("203.0.113.4", true, 0xcb007104u);
	ExpectIP("255.255.255.255", true, 0xffffffffu);
	ExpectIP("0.0.0.0", true, 0);
	ExpectIP("256.1.1.1", false, 0);
	ExpectIP("1.2.3", false, 0);
	ExpectIP("1.2.3.4.5", false, 0);
	ExpectIP("1..3.4", false, 0);
	ExpectIP("0001.2.3.4", false, 0);
	ExpectIP("a.b.c.d", false, 0);
	ExpectIP("", false, 0);

	uint32_t ip = 0;
	uint16_t port = 0;
	Check(sbo_address::Resolve("203.0.113.148:27060", ip, port) && ip == 0xcb007194u && port == 27060, "Resolve ipv4");
	Check(!sbo_address::Resolve("0.0.0.0", ip, port), "Resolve 0.0.0.0");
	Check(!sbo_address::Resolve("bad;name", ip, port), "Resolve with ';' (no lookup)");
	Check(!sbo_address::Resolve(nullptr, ip, port), "Resolve null");

	// random and hostile text: must not crash or overrun (ASan / UBSan)
	srand(12345);
	const char alphabet[] = "0123456789.:abcXYZ -_\t;\"\\%\xff\x80";
	for (int n = 0; n < 200000; n++)
	{
		char text[96];
		const int len = rand() % (int)sizeof(text);
		for (int i = 0; i < len; i++)
			text[i] = alphabet[rand() % (int)(sizeof(alphabet) - 1)];
		text[len] = 0;
		char host[16];
		uint16_t p;
		uint32_t v;
		sbo_address::Split(text, host, (int)(rand() % (int)sizeof(host)), p);
		sbo_address::ParseIPv4(text, v);
		// Resolve only on text that can't reach a lookup (a digit-and-dot host never does)
		bool digitsOnly = true;
		for (int i = 0; i < len; i++)
			digitsOnly = digitsOnly && ((text[i] >= '0' && text[i] <= '9') || text[i] == '.' || text[i] == ':');
		if (digitsOnly)
			sbo_address::Resolve(text, v, p);
	}

	if (s_iFailed)
	{
		printf("%d check(s) failed\n", s_iFailed);
		return 1;
	}
	printf("PASS\n");
	return 0;
}
