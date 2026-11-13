// Offline test of wslayout::Doc (tests/worldscreen_editor/run_test.sh).
#include "worldscreen_layout_doc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <random>
#include <sstream>
#include <string>

static int s_iFailed = 0;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); s_iFailed++; } } while (0)

static std::string ReadFile(const char *path)
{
	std::ifstream f(path, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

static void TestShipped(const char *path)
{
	const std::string text = ReadFile(path);
	wslayout::Doc d;
	CHECK(d.SetText(text));
	CHECK(d.Text() == text);
	CHECK(d.Find(wslayout::SCREEN_SECTION) >= 0);
	for (int i = 0; i < d.Count(); i++)
	{
		CHECK(d.Sec(i).close != std::string::npos);
		CHECK(d.Get(i, "ControlName") != nullptr);
	}
	// every edit keeps everything else: set and remove a key in each section
	for (int i = 0; i < d.Count(); i++)
	{
		wslayout::Doc e = d;
		const std::string name = e.Sec(i).name;
		e.Set(i, "xpos", "77");
		CHECK(e.Count() == d.Count());
		CHECK(e.Get(i, "xpos") && *e.Get(i, "xpos") == "77");
		e.Remove(i, "xpos");
		CHECK(!e.Get(i, "xpos"));
		CHECK(e.Count() == d.Count());
		CHECK(e.Sec(i).name == name);
	}
	printf("ok %s (%d controls)\n", path, d.Count());
}

static void TestCases()
{
	const std::string base =
	    "\"ui/scripts/screens/t.res\"\n"
	    "{\n"
	    "\t// a comment that stays\n"
	    "\t\"WorldScreen\"\n"
	    "\t{\n"
	    "\t\t\"ControlName\"\t\"EditablePanel\"\n"
	    "\t\t\"wide\"\t\"512\" [$WIN32]\n"
	    "\t}\n"
	    "\t\"A\" { \"ControlName\" \"Label\" \"xpos\" \"1\" }\n"
	    "\tB\n"
	    "\t{\n"
	    "\t\tControlName Button // bare tokens\n"
	    "\t\tcommand hello\n"
	    "\t\tsub { x y }\n"
	    "\t\tlabelText \"after sub\"\n"
	    "\t}\n"
	    "}\n";
	wslayout::Doc d;
	CHECK(d.SetText(base));
	CHECK(d.RootName() == "ui/scripts/screens/t.res");
	CHECK(d.Count() == 3);
	CHECK(d.Find("a") == 1 && d.Find("B") == 2);
	CHECK(d.Get(0, "wide") && *d.Get(0, "wide") == "512");
	CHECK(d.Get(2, "command") && *d.Get(2, "command") == "hello");
	CHECK(d.Get(2, "labeltext") && *d.Get(2, "labeltext") == "after sub");
	CHECK(!d.Get(2, "x"));

	// set on a one-line section, on bare tokens, a new key
	d.Set(1, "xpos", "5");
	CHECK(*d.Get(1, "xpos") == "5");
	d.Set(1, "ypos", "6");
	CHECK(*d.Get(1, "ypos") == "6");
	d.Set(2, "command", "bye \"quoted\"\n");
	CHECK(*d.Get(2, "command") == "bye 'quoted' ");
	d.Set(0, "tall", "300");
	CHECK(*d.Get(0, "tall") == "300");
	CHECK(d.Text().find("// a comment that stays") != std::string::npos);
	CHECK(d.Text().find("// bare tokens") != std::string::npos);

	// remove with its conditional
	d.Remove(0, "wide");
	CHECK(!d.Get(0, "wide"));
	CHECK(d.Text().find("[$WIN32]") == std::string::npos);

	// add, rename, move, delete
	int n = d.Add("NewLabel", { { "ControlName", "Label" }, { "fieldName", "NewLabel" }, { "labelText", "hi" } });
	CHECK(n == 3 && d.Count() == 4);
	CHECK(*d.Get(n, "labelText") == "hi");
	d.Rename(n, "Renamed");
	CHECK(d.Find("Renamed") == 3 && *d.Get(3, "fieldName") == "Renamed");
	CHECK(d.UniqueName("Renamed") == "Renamed2");
	n = d.MoveToEnd(1);
	CHECK(n == 3 && d.Sec(3).name == "A" && d.Count() == 4);
	CHECK(*d.Get(3, "xpos") == "5");
	d.Delete(0);
	CHECK(d.Count() == 3 && d.Find("WorldScreen") < 0);
	CHECK(d.Text().find("// a comment that stays") != std::string::npos);

	// Move: up, down, to the end, to the start, and every pair on a shipped-like layout keeps every section once
	{
		wslayout::Doc m;
		CHECK(m.SetText("\"r\"\n{\n\t\"S\" { \"a\" \"1\" }\n\t\"A\"\n\t{\n\t\t\"k\"\t\"1\"\n\t}\n\t\"B\"\n\t{\n\t}\n"
		                "\t// keep\n\t\"C\"\n\t{\n\t\t\"k\"\t\"3\"\n\t}\n}\n"));
		CHECK(m.Count() == 4);
		CHECK(m.Move(3, 1) == 1 && m.Sec(1).name == "C" && m.Sec(2).name == "A" && m.Sec(3).name == "B");
		CHECK(m.Move(1, 2) == 2 && m.Sec(1).name == "A" && m.Sec(2).name == "C" && m.Sec(3).name == "B");
		CHECK(m.Move(1, 3) == 3 && m.Sec(3).name == "A" && m.Count() == 4);
		CHECK(m.Move(3, 0) == 0 && m.Sec(0).name == "A" && m.Sec(1).name == "S");
		CHECK(m.Text().find("// keep") != std::string::npos);
		CHECK(*m.Get(m.Find("C"), "k") == "3");
		for (int a = 0; a < 4; a++)
			for (int b = 0; b < 4; b++)
			{
				wslayout::Doc t = m;
				t.Move(a, b);
				CHECK(t.Count() == 4);
				CHECK(t.Find("A") >= 0 && t.Find("B") >= 0 && t.Find("C") >= 0 && t.Find("S") >= 0);
				CHECK(t.Sec(b).name == m.Sec(a).name);
			}
	}

	// a new layout
	wslayout::Doc nd;
	CHECK(nd.SetText(wslayout::NewLayoutText("my_screen")));
	CHECK(nd.RootName() == "ui/scripts/screens/my_screen.res");
	CHECK(nd.Count() == 2 && nd.Find("Title") == 1);

	// names
	char name[32];
	CHECK(wslayout::CleanLayoutName(" my-screen_2.res ", name, sizeof(name)) && !strcmp(name, "my-screen_2"));
	CHECK(!wslayout::CleanLayoutName("a/b", name, sizeof(name)));
	CHECK(!wslayout::CleanLayoutName("", name, sizeof(name)));
	CHECK(!wslayout::CleanLayoutName(std::string(32, 'a'), name, sizeof(name)));

	// broken text: no root, an unclosed layout, an unclosed section
	CHECK(!d.SetText(""));
	CHECK(!d.SetText("\"x\""));
	CHECK(d.SetText("\"x\" { \"A\" { \"k\" \"v\""));
	CHECK(d.Count() == 1 && d.Sec(0).close == std::string::npos);
	d.Set(0, "k2", "v2");
	CHECK(d.Get(0, "k2") && *d.Get(0, "k2") == "v2");
	CHECK(d.SetText("\"x\" {"));
	CHECK(d.Add("S", { { "a", "b" } }) == 0);
	CHECK(d.Sec(0).close != std::string::npos);
	printf("ok cases\n");
}

static void TestRandom()
{
	std::mt19937 rng(12345);
	const char pieces[][12] = { "\"", "{", "}", "//", "\n", " ", "a", "[$X]", "\"k\"", "\"v\"", "\t", "x{", "}}" };
	for (int round = 0; round < 3000; round++)
	{
		std::string t = "\"r\" { ";
		const int len = rng() % 60;
		for (int i = 0; i < len; i++)
			t += pieces[rng() % (sizeof(pieces) / sizeof(pieces[0]))];
		wslayout::Doc d;
		d.SetText(t);
		for (int k = 0; k < 6 && d.HasRoot(); k++)
		{
			const int c = d.Count();
			const int i = c ? (int)(rng() % c) : 0;
			switch (rng() % 8)
			{
			case 0: d.Set(i, "k", "val"); break;
			case 1: d.Remove(i, "k"); break;
			case 2: d.Add(d.UniqueName("n"), { { "ControlName", "Label" } }); break;
			case 3: d.Delete(i); break;
			case 4: d.Rename(i, "zz"); break;
			case 5: d.MoveToEnd(i); break;
			case 6: d.Move(i, c ? (int)(rng() % c) : 0); break;
			default: d.Get(i, "v"); break;
			}
		}
	}
	printf("ok random\n");
}

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++)
		TestShipped(argv[i]);
	TestCases();
	TestRandom();
	if (s_iFailed)
	{
		fprintf(stderr, "%d checks failed\n", s_iFailed);
		return 1;
	}
	printf("PASS\n");
	return 0;
}
