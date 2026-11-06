// tests/html_motd/run_test.sh: src/cl_dll/vgui2/sbo_html.h (IsURL, IsHtml, CopyURL).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbo_html.h"

static int s_fails = 0;

#define CHECK(cond)                                                       \
	do                                                                    \
	{                                                                     \
		if (!(cond))                                                      \
		{                                                                 \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			s_fails++;                                                    \
		}                                                                 \
	} while (0)

int main()
{
	using namespace sbohtml;

	// HTML
	CHECK(IsHtml("<html><body>hi</body></html>"));
	CHECK(IsHtml("<HTML>"));
	CHECK(IsHtml("  \r\n\t<html lang=\"en\">"));
	CHECK(IsHtml("\xEF\xBB\xBF<html>"));
	CHECK(IsHtml("<!DOCTYPE html><html>"));
	CHECK(IsHtml("<!doctype HTML>"));
	CHECK(IsHtml("<html/>"));
	CHECK(!IsHtml("<htmlx>"));
	CHECK(!IsHtml("<html"));
	CHECK(!IsHtml("Welcome to <html> land"));
	CHECK(!IsHtml("Welcome"));
	CHECK(!IsHtml(""));
	CHECK(!IsHtml(nullptr));
	CHECK(!IsHtml("<!doctype htm"));
	CHECK(!IsHtml("\xEF\xBB"));

	// URL
	CHECK(IsURL("http://example.com/motd.html"));
	CHECK(IsURL("HTTPS://example.com"));
	CHECK(IsURL("  https://example.com\n"));
	CHECK(IsURL("\xEF\xBB\xBFhttp://a"));
	CHECK(!IsURL("http://"));
	CHECK(!IsURL("http:///x"));
	CHECK(!IsURL("http:// x"));
	CHECK(!IsURL("ftp://example.com"));
	CHECK(!IsURL("Visit http://example.com"));
	CHECK(!IsURL("htt"));
	CHECK(!IsURL(""));
	CHECK(!IsURL(nullptr));

	// CopyURL
	char buf[32];
	CHECK(CopyURL("  http://example.com/x \r\n", buf, sizeof(buf)) && !strcmp(buf, "http://example.com/x"));
	CHECK(!CopyURL("http://example.com/a-very-long-path-that-does-not-fit", buf, sizeof(buf)) && buf[0] == 0);
	CHECK(CopyURL("", buf, sizeof(buf)) && buf[0] == 0);
	CHECK(!CopyURL("x", buf, 0));
	CHECK(!CopyURL(nullptr, buf, sizeof(buf)) && buf[0] == 0);
	char one[1];
	CHECK(CopyURL("   ", one, sizeof(one)) && one[0] == 0);
	CHECK(!CopyURL("a", one, sizeof(one)));

	// random and truncated input: every prefix of every case, in exactly-sized heap buffers (ASan sees overreads)
	const char *cases[] = {"<!DOCTYPE html>", "<html>", "\xEF\xBB\xBF  <html>", "https://x.y/z", "http://", "<!doc"};
	for (const char *c : cases)
	{
		size_t len = strlen(c);
		for (size_t n = 0; n <= len; n++)
		{
			char *p = (char *)malloc(n + 1);
			memcpy(p, c, n);
			p[n] = 0;
			(void)IsHtml(p);
			(void)IsURL(p);
			char out[64];
			(void)CopyURL(p, out, sizeof(out));
			free(p);
		}
	}
	srand(1);
	for (int i = 0; i < 200000; i++)
	{
		size_t n = (size_t)(rand() % 24);
		char *p = (char *)malloc(n + 1);
		for (size_t k = 0; k < n; k++)
		{
			static const char alphabet[] = "<>!/ \t\r\nhtmlHTMLdoctypeDOCTYPEs:\xEF\xBB\xBF";
			p[k] = alphabet[rand() % (sizeof(alphabet) - 1)];
		}
		p[n] = 0;
		(void)IsHtml(p);
		(void)IsURL(p);
		char out[8];
		(void)CopyURL(p, out, sizeof(out));
		free(p);
	}

	if (s_fails)
	{
		fprintf(stderr, "%d check(s) failed\n", s_fails);
		return 1;
	}
	printf("PASS\n");
	return 0;
}
