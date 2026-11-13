// Unit and hostile-input test of sbo_script_file.cpp (run_test.sh): SBO's settings.scr / user.scr parse, the commands
// GameUI would send, the file written back parses again to the same objects, every truncation and random damage.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "sbo_script_file.h"

using namespace sbo_script;
static int s_iFailed = 0;
static void Check(bool ok, const char *what)
{
	if (!ok)
	{
		printf("FAIL: %s\n", what);
		s_iFailed++;
	}
}

static std::string ReadFile(const char *path)
{
	std::string s;
	FILE *f = fopen(path, "rb");
	if (!f)
		return s;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		s.append(buf, n);
	fclose(f);
	return s;
}

static void Hostile(const std::string &text, Description::Kind kind)
{
	for (size_t cut = 0; cut <= text.size(); cut++)
	{
		Description d;
		d.Parse(text.data(), cut, kind);
		for (const Object &o : d.objects)
		{
			(void)o.ConfigCommand();
			(void)o.ListRow();
		}
		(void)d.Write("x");
	}
	srand(7);
	for (int round = 0; round < 3000; round++)
	{
		std::string t = text;
		const int changes = 1 + rand() % 8;
		for (int i = 0; i < changes && !t.empty(); i++)
			t[rand() % t.size()] = (char)(rand() % 256);
		Description d;
		d.Parse(t.data(), t.size(), kind);
		for (const Object &o : d.objects)
			(void)o.ConfigCommand();
		(void)d.Write("x");
	}
	for (int round = 0; round < 500; round++)
	{
		std::string t(rand() % 600, ' ');
		for (char &c : t)
			c = "{}\" \nVERSIONDESCRIPTION_ab1.LISTBOOLNUMBERSTRING/"[rand() % 47];
		Description d;
		d.Parse(t.data(), t.size(), kind);
	}
}

int main(int argc, char **argv)
{
	if (argc < 3)
		return 2;
	const std::string settings = ReadFile(argv[1]), user = ReadFile(argv[2]);
	Check(!settings.empty() && !user.empty(), "files read");

	Description s;
	Check(s.Parse(settings.data(), settings.size(), Description::KIND_SERVER), "settings.scr parses");
	Check(s.objects.size() >= 17, "settings.scr objects");
	Object *host = s.Find("hostname");
	Check(host && host->type == TYPE_STRING && host->defValue == "Syndicate Black Ops Server", "hostname");
	Object *lan = s.Find("sv_lan");
	Check(lan && lan->type == TYPE_LIST && lan->items.size() == 2 && lan->ListRow() == 0, "sv_lan list");
	if (lan)
	{
		lan->SetCurValue("1");
		Check(lan->ConfigCommand() == "sv_lan \"1\"\n", "sv_lan command");
	}
	Object *frag = s.Find("mp_fraglimit");
	Check(frag && frag->type == TYPE_NUMBER, "mp_fraglimit number");
	if (frag)
	{
		frag->SetCurValue("25.000000");
		Check(frag->curValue == "25", "SetCurValue trims");
		Check(frag->ConfigCommand() == "mp_fraglimit \"25.000000\"\n", "number command");
	}
	Object *hard = s.Find("mp_hardcore");
	if (hard)
	{
		hard->SetCurValue("250");
		Check(hard->ConfigCommand() == "mp_hardcore \"100.000000\"\n", "number clamped to max");
	}
	// a LIST's value is the row; the command carries that row's item value (mp_speedmon "Off" "-1" is the fifth row)
	Object *speed = s.Find("mp_speedmon");
	Check(speed && speed->type == TYPE_LIST && speed->items.size() == 5 && speed->ListRow() == 1, "mp_speedmon list");
	if (speed)
	{
		speed->SetCurValue("4");
		Check(speed->ConfigCommand() == "mp_speedmon \"-1\"\n", "list row 4 sends -1");
	}
	Object *ff = s.Find("mp_friendlyfire");
	Check(ff && ff->type == TYPE_BOOL, "friendly fire bool");
	if (host)
	{
		host->SetCurValue("My \"best\" 100% server");
		Check(host->ConfigCommand() == "hostname \"My best 100 server\"\n", "string stripped");
	}
	// written back: parses to the same objects and values
	const std::string out = s.Write(FileTime(1793566000, Description::KIND_SERVER));
	Check(out.find("\r\n") != std::string::npos && out.rfind("// NOTE:  THIS FILE IS AUTOMATICALLY REGENERATED, \r\n", 0) == 0, "header");
	Description s2;
	Check(s2.Parse(out.data(), out.size(), Description::KIND_SERVER), "written file parses");
	Check(s2.objects.size() == s.objects.size(), "same object count");
	if (Object *f2 = s2.Find("mp_fraglimit"))
		Check(f2->defValue == "25.000000", "new default written");
	if (Object *l2 = s2.Find("sv_lan"))
		Check(l2->ListRow() == 1, "list default written");
	if (Object *sp2 = s2.Find("mp_speedmon"))
		Check(sp2->ListRow() == 4, "list row written");

	Description u;
	Check(u.Parse(user.data(), user.size(), Description::KIND_INFO), "user.scr parses");
	Check(u.Find("mp_decals") && u.Find("cl_effectsdetail"), "user.scr objects");
	// Multiplayer Advanced: view options (halflife-updated's user.scr ranges)
	Check(u.Find("cl_rollangle") && u.Find("cl_rollangle")->type == TYPE_NUMBER && u.Find("cl_rollangle")->fMax == 360.0f,
	      "user.scr cl_rollangle 0-360");
	Check(u.Find("cl_rollspeed") && u.Find("cl_rollspeed")->fMin == 0.0f && u.Find("cl_rollspeed")->fMax == -1.0f,
	      "user.scr cl_rollspeed 0-no limit");
	Check(u.Find("cl_bobtilt") && u.Find("cl_bobtilt")->type == TYPE_BOOL, "user.scr cl_bobtilt toggle");
	Check(u.Find("default_fov") && u.Find("default_fov")->fMin == 1.0f && u.Find("default_fov")->fMax == 179.0f &&
	          u.Find("default_fov")->defValue == "90.000000",
	      "user.scr default_fov 1-179, 90");
	Description wrong;
	Check(!wrong.Parse(user.data(), user.size(), Description::KIND_SERVER), "kind checked");
	const std::string uout = u.Write(FileTime(1793566000, Description::KIND_INFO));
	Check(uout.find("DESCRIPTION INFO_OPTIONS\r\n{\r\n") != std::string::npos, "user header");
	Check(FileTime(0, Description::KIND_SERVER).size() == 22, "time text length");

	// config.cfg: the value the engine would set (the last command wins; quotes, ';', comments, case)
	{
		const std::string cfg = "// comment cl_experimental_menus \"7\"\r\nunbindall\r\nbind \"w\" \"+forward\"\r\n"
		                        "cl_experimental_menus \"1\"\r\nCL_Experimental_Menus \"0\"\r\n"
		                        "echo cl_experimental_menus 5; cl_new_ingame_menus 1 // \"2\"\r\nhud_chat_x \"a;b\"\r\n";
		std::string v;
		Check(ConfigValue(cfg.data(), cfg.size(), "cl_experimental_menus", v) && v == "0", "config value: last wins");
		Check(ConfigValue(cfg.data(), cfg.size(), "cl_new_ingame_menus", v) && v == "1", "config value after ';'");
		Check(ConfigValue(cfg.data(), cfg.size(), "hud_chat_x", v) && v == "a;b", "config value quoted ';'");
		Check(!ConfigValue(cfg.data(), cfg.size(), "volume", v), "config value missing");
		Check(!ConfigValue(cfg.data(), cfg.size(), "unbindall", v), "config command without value");
		Check(!ConfigValue(cfg.data(), 0, "cl_experimental_menus", v) && !ConfigValue(nullptr, 5, "x", v), "config empty");
		for (size_t n = 0; n <= cfg.size(); ++n)
		{
			std::string part(cfg.data(), n); // exact-size buffer: ASan sees any read past the end
			ConfigValue(part.data(), part.size(), "cl_experimental_menus", v);
		}
		srand(11);
		for (int k = 0; k < 3000; ++k)
		{
			std::string t(rand() % 400, ' ');
			for (char &c : t)
				c = "cl_experimental_menus \"01;/\r\n\t\0x"[rand() % 31];
			std::vector<char> buf(t.begin(), t.end());
			ConfigValue(buf.data(), buf.size(), "cl_experimental_menus", v);
			Check(v.size() <= Description::MAX_STRING, "config value bounded");
		}
	}

	Hostile(settings, Description::KIND_SERVER);
	Hostile(user, Description::KIND_INFO);
	printf("%s (%d failed)\n", s_iFailed ? "FAIL" : "PASS", s_iFailed);
	return s_iFailed ? 1 : 0;
}
