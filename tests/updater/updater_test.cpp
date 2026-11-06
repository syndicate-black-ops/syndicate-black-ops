// Offline tests of the updater (src/cl_dll/updater, docs/UPDATER.md), built by run_test.sh with ASan/UBSan and
// SBO_UPDATER_TESTING: JSON (also random / truncated input), versions, the release list and the emergency marker,
// paths, manifests (hostile input), the install plan, markdown, SHA-256, Ed25519, inflate, the installer (rollback,
// crash recovery both ways, files in use, backups) and a whole update through a fake HTTP server.
// argv[1]: scratch folder, argv[2]: raw deflate file, argv[3]: its plain text

#include "updater_client.h"
#include "updater_core.h"
#include "updater_crypto.h"
#include "updater_install.h"
#include "updater_json.h"
#include "updater_sha256.h"

extern "C" {
#include "monocypher-ed25519.h"
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <chrono>
#include <thread>
#include <sstream>

namespace fs = std::filesystem;
using namespace sbo_update;

static int g_iFailures = 0;
#define CHECK(x)                                                                     \
	do                                                                               \
	{                                                                                \
		if (!(x))                                                                    \
		{                                                                            \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);             \
			g_iFailures++;                                                           \
		}                                                                            \
	} while (0)

static std::string ReadFile(const fs::path &p)
{
	std::ifstream f(p, std::ios::binary);
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

static void WriteFile(const fs::path &p, const std::string &s)
{
	fs::create_directories(p.parent_path());
	std::ofstream f(p, std::ios::binary | std::ios::trunc);
	f << s;
}

static void TestJson()
{
	json::Value v;
	std::string err;
	const std::string doc = "{\"a\": [1, -2, 3.5, true, false, null, \"x\\u00e9\\ud83d\\ude00\\n\"], \"b\": {}}";
	CHECK(json::Parse(doc.data(), doc.size(), v, err));
	CHECK(v.Get("a") && v.Get("a")->a.size() == 7);
	CHECK(v.Get("a")->a[1].isInt && v.Get("a")->a[1].i == -2);
	CHECK(v.Get("a")->a[6].s == "x\xC3\xA9\xF0\x9F\x98\x80\n");
	const char *bad[] = {"", "{", "[1,]", "{\"a\" 1}", "\"\\ud800\"", "\"\x01\"", "01", "1.", "[1] x", "{\"a\":}",
	                     "tru", "\"\\x\""};
	for (const char *b : bad)
		CHECK(!json::Parse(b, strlen(b), v, err));
	std::string deep(100, '[');
	deep += std::string(100, ']');
	CHECK(!json::Parse(deep.data(), deep.size(), v, err));
	CHECK(json::Quote("a\"b\\\n") == "\"a\\\"b\\\\\\u000a\"");
	// every prefix of a valid document, and random bytes
	for (size_t n = 0; n < doc.size(); n++)
		json::Parse(doc.data(), n, v, err);
	std::mt19937 rng(1);
	for (int i = 0; i < 20000; i++)
	{
		std::string s(rng() % 64, ' ');
		for (char &c : s)
			{
			static const char alpha[] = "{}[]\",:01-.eE\\u tfnalrs";
			c = alpha[rng() % (sizeof(alpha) - 1)];
		}
		json::Parse(s.data(), s.size(), v, err);
	}
}

static void TestVersions()
{
	Version v;
	CHECK(ParseVersion("v2.0.1", v) && v.major == 2 && v.patch == 1 && v.pre.empty());
	CHECK(ParseVersion("2.0.1-rc.1+abc", v) && v.pre == "rc.1");
	CHECK(!ParseVersion("2.0", v));
	CHECK(!ParseVersion("2.0.01", v));
	CHECK(!ParseVersion("2.0.1-", v));
	CHECK(!ParseVersion("2.0.1 x", v));
	CHECK(!ParseVersion("unknown", v));
	CHECK(CompareVersions("2.0.1", "2.0.0") > 0);
	CHECK(CompareVersions("2.0.1-rc1", "2.0.1") < 0);
	CHECK(CompareVersions("2.0.1-test2", "2.0.1-test10") > 0); // alphanumeric identifiers compare as text
	CHECK(CompareVersions("2.0.1-test.2", "2.0.1-test.10") < 0);
	CHECK(CompareVersions("2.0.1-alpha", "2.0.1-alpha.1") < 0);
	CHECK(CompareVersions("2.0.1-1", "2.0.1-alpha") < 0);
	CHECK(CompareVersions("2.0.0+abc", "2.0.0+def") == 0);
	CHECK(CompareVersions("10.0.0", "9.9.9") > 0);
	CHECK(CompareVersions("bad", "0.0.0") < 0);
}

static std::string ReleaseJson(const char *tag, const char *name, bool pre, const char *body, bool draft = false)
{
	return std::string("{\"tag_name\":\"") + tag + "\",\"name\":\"" + name + "\",\"prerelease\":" + (pre ? "true" : "false") +
	       ",\"draft\":" + (draft ? "true" : "false") + ",\"body\":\"" + body +
	       "\",\"html_url\":\"https://github.com/o/r/releases/tag/" + tag +
	       "\",\"assets\":[{\"name\":\"manifest.json\",\"browser_download_url\":\"https://x/" + tag + "/manifest.json\"}]}";
}

static void TestReleases()
{
	const std::string list = "[" + ReleaseJson("v2.0.3", "SBO 2.0.3", true, "beta") + "," +
	                         ReleaseJson("v2.0.2", "SBO 2.0.2", false, "two") + "," +
	                         ReleaseJson("v2.0.9", "draft", false, "", true) + "," +
	                         ReleaseJson("v2.0.1", "SBO 2.0.1 [No Auto-Update]", false, "one") + "," +
	                         ReleaseJson("nightly", "x", false, "") + "]";
	CheckResult r = EvaluateReleases(list, "2.0.0", false);
	CHECK(r.ok && r.updateAvailable && r.hasLatest);
	CHECK(r.latest.tag == "v2.0.2" && !r.latest.noAutoUpdate);
	CHECK(r.newer.size() == 2 && r.newer[1].noAutoUpdate);
	CHECK(r.latest.AssetUrl("manifest.json") == "https://x/v2.0.2/manifest.json");
	r = EvaluateReleases(list, "2.0.0", true);
	CHECK(r.latest.tag == "v2.0.3" && r.newer.size() == 3);
	r = EvaluateReleases(list, "2.0.2", false);
	CHECK(r.ok && !r.updateAvailable);
	r = EvaluateReleases("[]", "2.0.0", false);
	CHECK(r.ok && !r.updateAvailable && !r.hasLatest);
	r = EvaluateReleases("{\"message\":\"API rate limit exceeded\"}", "2.0.0", false);
	CHECK(!r.ok && r.error.find("rate limit") != std::string::npos);
	r = EvaluateReleases("<html>", "2.0.0", false);
	CHECK(!r.ok);
	CHECK(ReleaseDateText("2026-11-02T22:25:43Z") == "November 2, 2026");
	CHECK(ReleaseDateText("2026-13-02T00:00:00Z").empty());
	CHECK(ReleaseDateText("x").empty());
	CHECK(ReleasesPageUrl("https://github.com/o/r/releases/tag/v2.0.1") == "https://github.com/o/r/releases");
	CHECK(ReleasesPageUrl("http://x/releases/tag/v1").empty());
	// notes past the budget: cut, flagged
	{
		std::string big(MAX_NOTES_BYTES + 100, 'x');
		CheckResult c = EvaluateReleases("[" + ReleaseJson("v3.0.0", "3", false, big.c_str()) + "]", "2.0.0", false);
		CHECK(c.ok && c.newer.size() == 1 && c.newer[0].bodyCut && c.newer[0].body.size() == MAX_NOTES_BYTES);
		std::string lines;
		while (lines.size() < 2 * MAX_NOTES_BYTES)
			lines += "- a line of notes\\n";
		c = EvaluateReleases("[" + ReleaseJson("v3.0.0", "3", false, lines.c_str()) + "]", "2.0.0", false);
		CHECK(c.ok && c.newer[0].bodyCut && c.newer[0].body.size() <= MAX_NOTES_BYTES && c.newer[0].body.back() != '\n' && c.newer[0].body.substr(c.newer[0].body.size() - 4) == "otes");
	}
	// marker in the notes
	r = EvaluateReleases("[" + ReleaseJson("v3.0.0", "SBO 3", false, "bad build [no auto-update]") + "]", "2.0.0", false);
	CHECK(r.latest.noAutoUpdate);
}

static void TestPaths()
{
	CHECK(IsSafeRelPath("cl_dlls/client.dll"));
	CHECK(IsSafeRelPath("maps/co_x.bsp"));
	const char *bad[] = {"", "/etc/passwd", "../x", "a/../b", "a//b", "a/./b", "C:/x", "a\\b", "updater/installed.json",
	                     "Updater/x", "con.txt", "maps/NUL", "a/b.", "a/b ", "x\x01", "\xC3\xA9"};
	for (const char *b : bad)
		CHECK(!IsSafeRelPath(b));
	CHECK(IsSafeRelPath("updaterx/a"));
	CHECK(IsSafeRelPath(std::string(150, 'a')));
	CHECK(!IsSafeRelPath(std::string(250, 'a')));
}

static const char SHA_A[] = "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb"; // "a"
static const char SHA_B[] = "3e23e8160039594a33894f6564e1b1348bbd7a0088d42c4acb73eeaed59c009d"; // "b"

static void TestManifest()
{
	Manifest m;
	std::string err;
	const std::string ok = std::string("{\"format\":1,\"version\":\"2.0.1\",\"zip\":\"sbo.zip\",\"zip_size\":100,"
	                                   "\"files\":[{\"path\":\"a.txt\",\"size\":1,\"sha256\":\"") + SHA_A +
	                       "\",\"zip_offset\":10,\"zip_size\":1,\"method\":\"store\"}],"
	                       "\"remove\":[{\"path\":\"old.txt\"}]}";
	CHECK(ParseManifest(ok, true, m, err));
	CHECK(m.files.size() == 1 && m.remove.size() == 1 && m.zipSize == 100);
	auto bad = [&](const std::string &from, const std::string &to) {
		std::string s = ok;
		const size_t p = s.find(from);
		CHECK(p != std::string::npos);
		s.replace(p, from.size(), to);
		Manifest x;
		std::string e;
		if (ParseManifest(s, true, x, e))
			fprintf(stderr, "accepted: %s -> %s\n", from.c_str(), to.c_str());
		CHECK(!ParseManifest(s, true, x, e));
	};
	bad("\"a.txt\"", "\"../a.txt\"");
	bad("\"zip_offset\":10", "\"zip_offset\":100");                 // past the zip
	bad("\"zip_size\":1,\"method\"", "\"zip_size\":2,\"method\""); // stored but sizes differ
	bad("\"method\":\"store\"", "\"method\":\"lzma\"");
	bad(SHA_A, "XYZ");
	bad("\"format\":1", "\"format\":2");
	bad("\"version\":\"2.0.1\"", "\"version\":\"two\"");
	bad("\"sbo.zip\"", "\"../sbo.zip\"");
	bad("\"old.txt\"", "\"a.txt\""); // removing a file the release has
	{
		// the old name of a file renamed in case only: allowed (MakePlan decides per file system)
		std::string s = ok;
		s.replace(s.find("\"old.txt\""), 9, "\"A.TXT\"");
		Manifest x;
		std::string e;
		CHECK(ParseManifest(s, true, x, e) && x.remove.size() == 1 && x.remove[0].path == "A.TXT");
	}
	bad("\"size\":1,", "\"size\":-1,");
	bad("\"method\":\"store\"}", "\"method\":\"store\",\"shipped_sha256\":[\"xyz\"]}");
	{
		std::string s = ok;
		s.replace(s.find("\"method\":\"store\"}"), 17, std::string("\"method\":\"store\",\"shipped_sha256\":[\"") + SHA_B + "\"]}");
		Manifest x;
		std::string e;
		CHECK(ParseManifest(s, true, x, e) && x.files[0].shippedSha256.size() == 1);
	}
	// duplicate path (case-insensitive)
	std::string dup = ok;
	dup.insert(dup.find("]"), std::string(",{\"path\":\"A.txt\",\"size\":1,\"sha256\":\"") + SHA_A +
	                              "\",\"zip_offset\":10,\"zip_size\":1,\"method\":\"store\"}");
	CHECK(!ParseManifest(dup, true, m, err));
	// installed.json round trip
	CHECK(ParseManifest(ok, true, m, err));
	m.files[0].keepUserChanges = true;
	Manifest back;
	CHECK(ParseManifest(InstalledJson(m), false, back, err));
	CHECK(back.files.size() == 1 && back.files[0].keepUserChanges && back.files[0].sha256 == SHA_A);
	// truncated input never crashes
	for (size_t n = 0; n < ok.size(); n++)
		ParseManifest(ok.substr(0, n), true, m, err);
}

static FileEntry Entry(const char *path, const char *sha, bool keep = false)
{
	FileEntry e;
	e.path = path;
	e.size = 1;
	e.sha256 = sha;
	e.keepUserChanges = keep;
	return e;
}

static void TestPlan()
{
	Manifest oldM, newM;
	oldM.files = {Entry("same", SHA_A), Entry("changed", SHA_A), Entry("cfg_untouched", SHA_A, true),
	              Entry("cfg_edited", SHA_A, true), Entry("gone", SHA_A), Entry("gone_edited", SHA_A)};
	newM.files = {Entry("same", SHA_A), Entry("changed", SHA_B), Entry("cfg_untouched", SHA_B, true),
	              Entry("cfg_edited", SHA_B, true), Entry("added", SHA_B), Entry("repair", SHA_A)};
	newM.remove = {{"leftover", ""}, {"leftover_other", SHA_A}};
	std::map<std::string, std::string> disk = {{"same", SHA_A}, {"changed", SHA_A}, {"cfg_untouched", SHA_A},
	                                           {"cfg_edited", "edited"}, {"gone", SHA_A}, {"gone_edited", "edited"},
	                                           {"repair", "broken"}, {"leftover", "x"}, {"leftover_other", "y"}};
	auto lookup = [&](const std::string &p) {
		DiskFile d;
		auto it = disk.find(p);
		if (it != disk.end())
		{
			d.exists = true;
			d.size = 1;
			d.sha256 = it->second;
		}
		return d;
	};
	const std::vector<PlanItem> plan = MakePlan(&oldM, newM, lookup);
	std::map<std::string, Action> got;
	for (const PlanItem &p : plan)
		got[p.path] = p.action;
	CHECK(!got.count("same"));
	CHECK(got["changed"] == Action::Replace);
	CHECK(got["cfg_untouched"] == Action::Replace);
	CHECK(got["cfg_edited"] == Action::KeepUser);
	CHECK(got["added"] == Action::Add);
	CHECK(got["repair"] == Action::Replace);
	CHECK(got["gone"] == Action::Remove);
	CHECK(got["gone_edited"] == Action::KeptRemoved);
	CHECK(got["leftover"] == Action::Remove);
	CHECK(got["leftover_other"] == Action::KeptRemoved);
	// no installed.json: an edited config is kept, nothing removed; a config as an earlier release shipped it is replaced
	FileEntry shipped = Entry("cfg_shipped", SHA_B, true);
	shipped.shippedSha256 = {std::string(64, '0'), SHA_A};
	newM.files.push_back(shipped);
	disk["cfg_shipped"] = SHA_A;
	const std::vector<PlanItem> plan2 = MakePlan(nullptr, newM, lookup);
	std::map<std::string, Action> got2;
	for (const PlanItem &p : plan2)
		got2[p.path] = p.action;
	CHECK(got2["cfg_untouched"] == Action::KeepUser);
	CHECK(got2["cfg_shipped"] == Action::Replace);
	CHECK(!got2.count("gone"));
}

// A file and its folder renamed in case only (ui/resource/UI/TutorTextWindow.res -> ui/resource/ui/tutortextwindow.res):
// a case-sensitive file system (Linux) must not keep both names, a case-insensitive one (Windows) must not lose the
// new file (the old name IS the new file there)
static void TestCaseRename(const fs::path &root)
{
	const char OLD[] = "ui/resource/UI/TutorTextWindow.res", NEW[] = "ui/resource/ui/tutortextwindow.res";
	Manifest oldM, newM;
	oldM.files = {Entry(OLD, SHA_A), Entry("keep.txt", SHA_A)};
	newM.files = {Entry(NEW, SHA_B), Entry("keep.txt", SHA_A)};
	auto run = [&](bool caseSensitive, const Manifest *installed, const Manifest &next, bool withSameFile) {
		auto key = [&](const std::string &p) { return caseSensitive ? p : LowerAscii(p); };
		std::map<std::string, std::string> disk = {{key(OLD), SHA_A}, {key("keep.txt"), SHA_A}};
		auto lookup = [&](const std::string &p) {
			DiskFile d;
			auto it = disk.find(key(p));
			if (it != disk.end())
			{
				d.exists = true;
				d.size = 1;
				d.sha256 = it->second;
			}
			return d;
		};
		auto same = [&](const std::string &a, const std::string &b) {
			return disk.count(key(a)) && disk.count(key(b)) && key(a) == key(b);
		};
		std::map<std::string, Action> got;
		for (const PlanItem &p : withSameFile ? MakePlan(installed, next, lookup, same) : MakePlan(installed, next, lookup))
			got[p.path] = p.action;
		return got;
	};
	// installed.json names the old spelling
	std::map<std::string, Action> lin = run(true, &oldM, newM, true);
	CHECK(lin.size() == 2 && lin[NEW] == Action::Add && lin[OLD] == Action::Remove);
	std::map<std::string, Action> win = run(false, &oldM, newM, true);
	CHECK(win.size() == 1 && win[NEW] == Action::Replace && !win.count(OLD));
	CHECK(!run(true, &oldM, newM, false).count(OLD)); // no way to tell: never removed
	// no installed.json: the release's remove list names the old spelling
	Manifest listed = newM;
	listed.remove = {{OLD, SHA_A}};
	lin = run(true, nullptr, listed, true);
	CHECK(lin.size() == 2 && lin[NEW] == Action::Add && lin[OLD] == Action::Remove);
	win = run(false, nullptr, listed, true);
	CHECK(win.size() == 1 && win[NEW] == Action::Replace && !win.count(OLD));

	// on this file system (the test runs on Linux: case-sensitive), through the installer: plan, stage, apply, clean up
	const fs::path g = root / "case";
	fs::remove_all(g);
	WriteFile(g / OLD, "a");
	WriteFile(g / "keep.txt", "a");
	Installer inst(g.u8string());
	const bool caseSensitive = !fs::exists(g / NEW);
	const std::vector<PlanItem> plan = MakePlan(&oldM, newM, [&](const std::string &r) { return inst.Disk(r); },
	    [&](const std::string &a, const std::string &b) { return inst.SameFile(a, b); });
	std::vector<InstallStep> steps;
	std::string err;
	for (const PlanItem &p : plan)
	{
		if (p.action == Action::Add || p.action == Action::Replace)
		{
			CHECK(inst.WriteStaged(p.path, "b", 1, err));
			steps.push_back({'F', p.path, 1, SHA_B});
		}
		else if (p.action == Action::Remove)
			steps.push_back({'D', p.path, 0, ""});
	}
	CHECK(steps.size() == (caseSensitive ? 2u : 1u));
	CHECK(inst.Apply(steps, "2.0.1", "{\"installed\":1}", err) == ApplyResult::Done);
	CHECK(ReadFile(g / NEW) == "b");
	inst.CleanupOld();
	if (caseSensitive)
	{
		CHECK(!fs::exists(g / OLD));
		CHECK(!fs::exists(g / "ui/resource/UI")); // the old folder, empty now, is gone
		CHECK(fs::exists(g / "ui/resource/ui"));
	}
	CHECK(ReadFile(g / "keep.txt") == "a");
	size_t names = 0;
	for (const auto &e : fs::recursive_directory_iterator(g / "ui"))
		names += e.is_regular_file() ? 1 : 0;
	CHECK(names == 1); // one file under ui/, under its new name
}

static void TestMarkdown()
{
	const std::string md = "## What's new\r\n\r\n- **Fixed** the `K444` crosshair ([#12](https://github.com/o/r/pull/12))\n"
	                       "  - nested *item*\n1. first\n\n> quoted\n\n| a | b |\n|---|---|\n| 1 | 2 |\n"
	                       "```\ncode <b>\n```\n![pic](https://x/y.png) <b>bold</b> [js](javascript:alert(1))\n---\n";
	const std::vector<TextRun> runs = MarkdownToRuns(md);
	const std::string plain = RunsToPlain(runs);
	CHECK(plain.find("What's new\n") == 0);
	CHECK(plain.find("\xE2\x80\xA2 Fixed the K444 crosshair (#12 (https://github.com/o/r/pull/12))") != std::string::npos);
	CHECK(plain.find("  \xE2\x80\xA2 nested item") != std::string::npos);
	CHECK(plain.find("1. first") != std::string::npos);
	CHECK(plain.find("a  b\n1  2") != std::string::npos);
	CHECK(plain.find("code <b>") != std::string::npos);
	CHECK(plain.find("pic bold js") != std::string::npos);
	bool heading = false, bold = false, code = false, link = false;
	for (const TextRun &r : runs)
	{
		heading = heading || r.style == RunStyle::Heading;
		bold = bold || (r.style == RunStyle::Bold && r.text == "Fixed");
		code = code || (r.style == RunStyle::Code && r.text == "K444");
		link = link || (r.style == RunStyle::Link && r.url == "https://github.com/o/r/pull/12");
		CHECK(r.style != RunStyle::Link || r.url.compare(0, 8, "https://") == 0);
	}
	CHECK(heading && bold && code && link);
	// hostile input
	std::mt19937 rng(2);
	for (int i = 0; i < 5000; i++)
	{
		std::string s(rng() % 200, ' ');
		for (char &c : s)
			{
			static const char alpha[] = "#*_`[]()!<>|-+> \n\\~1.ab";
			c = alpha[rng() % (sizeof(alpha) - 1)];
		}
		RunsToPlain(MarkdownToRuns(s));
	}
	std::string big(MAX_NOTES_BYTES * 3, '*');
	CHECK(RunsToPlain(MarkdownToRuns(big)).size() <= MAX_NOTES_BYTES + 16);
}

static void TestCrypto(const char *deflatePath, const char *plainPath)
{
	CHECK(Sha256Hex("", 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	CHECK(Sha256Hex("abc", 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	std::string million(1000000, 'a');
	CHECK(Sha256Hex(million.data(), million.size()) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
	// RFC 8032 test 1
	uint8_t key[32];
	CHECK(ParsePublicKey("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", key));
	uint8_t sig[64];
	const char *sigHex = "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b";
	for (int i = 0; i < 64; i++)
	{
		unsigned b;
		sscanf(sigHex + i * 2, "%2x", &b);
		sig[i] = (uint8_t)b;
	}
	CHECK(VerifySignature(key, "", 0, sig, 64));
	CHECK(!VerifySignature(key, "x", 1, sig, 64));
	CHECK(!VerifySignature(key, "", 0, sig, 63));
	sig[0] ^= 1;
	CHECK(!VerifySignature(key, "", 0, sig, 64));
	CHECK(!ParsePublicKey("zz", key));
	// inflate (data from python's zlib, raw deflate)
	const std::string comp = ReadFile(deflatePath), plain = ReadFile(plainPath);
	std::string out;
	CHECK(InflateRaw(comp.data(), comp.size(), plain.size(), out) && out == plain);
	CHECK(!InflateRaw(comp.data(), comp.size(), plain.size() - 1, out));
	CHECK(!InflateRaw(comp.data(), comp.size() / 2, plain.size(), out));
	std::string junk = comp;
	for (size_t i = 0; i < junk.size(); i += 7)
		junk[i] ^= 0x5A;
	InflateRaw(junk.data(), junk.size(), plain.size(), out); // no crash
}

// a game folder with a few files
static fs::path MakeGame(const fs::path &root, const char *name)
{
	const fs::path g = root / name;
	fs::remove_all(g);
	WriteFile(g / "keep.txt", "keep");
	WriteFile(g / "cl_dlls/client.so", "old client");
	WriteFile(g / "maps/old.bsp", "old map");
	WriteFile(g / "server.cfg", "player's own");
	return g;
}

static std::vector<InstallStep> StageUpdate(Installer &inst)
{
	std::vector<InstallStep> steps;
	auto stage = [&](const char *path, const std::string &data, char kind) {
		std::string err;
		CHECK(inst.WriteStaged(path, data.data(), data.size(), err));
		steps.push_back({kind, path, data.size(), Sha256Hex(data.data(), data.size())});
	};
	stage("cl_dlls/client.so", "new client", 'F');
	stage("maps/new/added.bsp", "new map", 'F');
	stage("server.cfg", "our new cfg", 'N');
	steps.push_back({'D', "maps/old.bsp", 0, ""});
	return steps;
}

static void ExpectOriginal(const fs::path &g)
{
	CHECK(ReadFile(g / "cl_dlls/client.so") == "old client");
	CHECK(ReadFile(g / "maps/old.bsp") == "old map");
	CHECK(!fs::exists(g / "maps/new/added.bsp"));
	CHECK(!fs::exists(g / "cl_dlls/client.so.sbo-old"));
	CHECK(!fs::exists(g / "updater/apply.journal"));
	CHECK(ReadFile(g / "server.cfg") == "player's own");
}

static void ExpectUpdated(const fs::path &g)
{
	CHECK(ReadFile(g / "cl_dlls/client.so") == "new client");
	CHECK(ReadFile(g / "maps/new/added.bsp") == "new map");
	CHECK(!fs::exists(g / "maps/old.bsp"));
	CHECK(ReadFile(g / "server.cfg") == "player's own");
	CHECK(ReadFile(g / "server.cfg.new") == "our new cfg");
	CHECK(ReadFile(g / "keep.txt") == "keep");
	CHECK(!fs::exists(g / "updater/apply.journal"));
	CHECK(ReadFile(g / "updater/installed.json") == "{\"installed\":1}");
}

static void TestInstaller(const fs::path &root)
{
	std::string err;
	// 1. normal install, backups, cleanup
	{
		const fs::path g = MakeGame(root, "g1");
		Installer inst(g.u8string());
		const std::vector<InstallStep> steps = StageUpdate(inst);
		CHECK(inst.Apply(steps, "2.0.1", "{\"installed\":1}", err) == ApplyResult::Done);
		ExpectUpdated(g);
		CHECK(ReadFile(g / "cl_dlls/client.so.sbo-old") == "old client");
		CHECK(!fs::exists(g / "updater/staging"));
		CHECK(!inst.CleanupOld().empty());
		CHECK(!fs::exists(g / "cl_dlls/client.so.sbo-old"));
		CHECK(!fs::exists(g / "maps/old.bsp.sbo-old"));
		CHECK(inst.CleanupOld().empty());
		Manifest m;
		CHECK(!inst.ReadInstalled(m)); // "{"installed":1}" is not a manifest
		inst.WriteSkippedVersion("2.0.2");
		CHECK(inst.ReadSkippedVersion() == "2.0.2");
	}
	// 2. a step fails: everything undone
	for (int failAt = 0; failAt < 4; failAt++)
	{
		const fs::path g = MakeGame(root, "g2");
		Installer inst(g.u8string());
		const std::vector<InstallStep> steps = StageUpdate(inst);
		Installer::s_iTestFailStep = failAt;
		CHECK(inst.Apply(steps, "2.0.1", "{\"installed\":1}", err) == ApplyResult::Failed);
		Installer::s_iTestFailStep = -1;
		ExpectOriginal(g);
		CHECK(!fs::exists(g / "updater/installed.json"));
	}
	// 3. crash half-way, staged files intact: the next start finishes it
	for (int crashAt = 0; crashAt < 4; crashAt++)
	{
		const fs::path g = MakeGame(root, "g3");
		{
			Installer inst(g.u8string());
			const std::vector<InstallStep> steps = StageUpdate(inst);
			Installer::s_iTestCrashAfter = crashAt;
			inst.Apply(steps, "2.0.1", "{\"installed\":1}", err);
			Installer::s_iTestCrashAfter = -1;
		}
		Installer next(g.u8string());
		CHECK(next.Recover().find("finished") != std::string::npos);
		ExpectUpdated(g);
	}
	// 4. crash half-way with a damaged staged file: undone
	{
		const fs::path g = MakeGame(root, "g4");
		{
			Installer inst(g.u8string());
			const std::vector<InstallStep> steps = StageUpdate(inst);
			Installer::s_iTestCrashAfter = 1;
			inst.Apply(steps, "2.0.1", "{\"installed\":1}", err);
			Installer::s_iTestCrashAfter = -1;
		}
		WriteFile(g / "updater/staging/maps/new/added.bsp", "damaged");
		Installer next(g.u8string());
		CHECK(next.Recover().find("undone") != std::string::npos);
		ExpectOriginal(g);
	}
	// 5. a file in use: installed at the next start
	{
		const fs::path g = MakeGame(root, "g5");
		Installer inst(g.u8string());
		const std::vector<InstallStep> steps = StageUpdate(inst);
		Installer::s_TestLockedPath = "maps/old.bsp";
		CHECK(inst.Apply(steps, "2.0.1", "{\"installed\":1}", err) == ApplyResult::Deferred);
		CHECK(inst.HasPendingInstall());
		CHECK(inst.Recover().find("still in use") != std::string::npos);
		CHECK(ReadFile(g / "maps/old.bsp") == "old map");
		CHECK(ReadFile(g / "cl_dlls/client.so") == "new client");
		Installer::s_TestLockedPath.clear();
		CHECK(inst.Recover().find("finished") != std::string::npos);
		ExpectUpdated(g);
	}
	// 6. a damaged staged file is refused before anything changes
	{
		const fs::path g = MakeGame(root, "g6");
		Installer inst(g.u8string());
		const std::vector<InstallStep> steps = StageUpdate(inst);
		WriteFile(g / "updater/staging/cl_dlls/client.so", "evil");
		CHECK(inst.Apply(steps, "2.0.1", "{\"installed\":1}", err) == ApplyResult::Failed);
		CHECK(err.find("damaged") != std::string::npos);
		ExpectOriginal(g);
	}
	// 7. garbage journal
	{
		const fs::path g = MakeGame(root, "g7");
		WriteFile(g / "updater/apply.journal", "SBOUPDATE 1\nstep R 5 - ../../etc/passwd\nbegin\n");
		Installer inst(g.u8string());
		CHECK(inst.Recover().find("unreadable") != std::string::npos);
		ExpectOriginal(g);
	}
}

// A fake GitHub: the release list, assets, a redirect to an "asset host", byte ranges
class FakeHttp : public IHttp
{
public:
	std::map<std::string, std::string> files;
	std::map<std::string, std::string> redirects;
	bool online = true;
	bool ignoreRange = false;
	int requests = 0;
	bool Available() override { return true; }
	bool Start(const HttpRequest &req, std::string &err) override
	{
		requests++;
		m_Res = HttpResult();
		m_bBusy = true;
		if (!online)
		{
			m_Res.error = "timed out";
			return true;
		}
		m_Res.ok = true;
		auto r = redirects.find(req.url);
		if (r != redirects.end())
		{
			m_Res.status = 302;
			m_Res.location = r->second;
			return true;
		}
		auto f = files.find(req.url);
		if (f == files.end())
		{
			m_Res.status = 404;
			return true;
		}
		if (req.range && !ignoreRange)
		{
			m_Res.status = 206;
			m_Res.body = f->second.substr((size_t)req.rangeFirst, (size_t)(req.rangeLast - req.rangeFirst + 1));
		}
		else
		{
			m_Res.status = 200;
			m_Res.body = f->second;
		}
		(void)err;
		return true;
	}
	bool Poll(HttpResult &out) override
	{
		if (!m_bBusy)
			return false;
		m_bBusy = false;
		out = m_Res;
		return true;
	}
	void Cancel() override { m_bBusy = false; }

private:
	HttpResult m_Res;
	bool m_bBusy = false;
};

static std::string Hex(const uint8_t *b, size_t n)
{
	std::string s;
	char buf[3];
	for (size_t i = 0; i < n; i++)
	{
		snprintf(buf, sizeof(buf), "%02x", b[i]);
		s += buf;
	}
	return s;
}

static void Pump(Updater &u, int frames = 2000)
{
	for (int i = 0; i < frames; i++)
	{
		u.Frame();
		const State s = u.GetState();
		if (s != State::Checking && s != State::Preparing && s != State::Downloading && s != State::Installing)
			return;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

static void TestUpdaterFlow(const fs::path &root)
{
	// signing key
	uint8_t seed[32], secret[64], pub[32];
	for (int i = 0; i < 32; i++)
		seed[i] = (uint8_t)(i * 7 + 1);
	crypto_ed25519_key_pair(secret, pub, seed);

	const fs::path g = MakeGame(root, "flow");
	WriteFile(g / "server.cfg", "player's own");
	// the "zip": stored files back to back
	struct F
	{
		const char *path;
		std::string data;
		bool keep;
	};
	const std::vector<F> files = {{"keep.txt", "keep", false}, {"cl_dlls/client.so", "client 2.0.1", false},
	                              {"maps/new.bsp", "a new map", false}, {"server.cfg", "cfg 2.0.1", true}};
	std::string zip = "PKZIPHEADER";
	std::string manifest = "{\"format\":1,\"version\":\"2.0.1\",\"commit\":\"abc\",\"zip\":\"sbo-v2.0.1.zip\",\"zip_size\":ZIPSIZE,\"files\":[";
	for (size_t i = 0; i < files.size(); i++)
	{
		char buf[512];
		snprintf(buf, sizeof(buf), "%s{\"path\":\"%s\",\"size\":%zu,\"sha256\":\"%s\",\"zip_offset\":%zu,\"zip_size\":%zu,\"method\":\"store\"%s}",
		    i ? "," : "", files[i].path, files[i].data.size(), Sha256Hex(files[i].data.data(), files[i].data.size()).c_str(),
		    zip.size(), files[i].data.size(), files[i].keep ? ",\"keep_user_changes\":true" : "");
		manifest += buf;
		zip += files[i].data + "xx";
	}
	manifest += "],\"remove\":[{\"path\":\"maps/old.bsp\"}]}";
	manifest.replace(manifest.find("ZIPSIZE"), 7, std::to_string(zip.size()));
	uint8_t sig[64];
	crypto_ed25519_sign(sig, secret, (const uint8_t *)manifest.data(), manifest.size());

	FakeHttp http;
	const std::string base = "https://api.github.com/repos/o/r";
	const std::string dl = "https://github.com/o/r/releases/download/v2.0.1/";
	http.files[base + "/releases?per_page=30"] =
	    "[{\"tag_name\":\"v2.0.1\",\"name\":\"SBO 2.0.1\",\"prerelease\":false,\"draft\":false,\"body\":\"## New\\n- thing\","
	    "\"html_url\":\"https://github.com/o/r/releases/tag/v2.0.1\",\"published_at\":\"2026-11-02T22:25:43Z\",\"assets\":["
	    "{\"name\":\"manifest.json\",\"browser_download_url\":\"" + dl + "manifest.json\"},"
	    "{\"name\":\"manifest.json.sig\",\"browser_download_url\":\"" + dl + "manifest.json.sig\"},"
	    "{\"name\":\"sbo-v2.0.1.zip\",\"browser_download_url\":\"" + dl + "sbo-v2.0.1.zip\"}]}]";
	http.files[dl + "manifest.json"] = manifest;
	http.files[dl + "manifest.json.sig"] = std::string((const char *)sig, 64);
	http.redirects[dl + "sbo-v2.0.1.zip"] = "https://assets.example/zip?sig=1";
	http.files["https://assets.example/zip?sig=1"] = zip;

	std::vector<std::string> log;
	Config cfg;
	cfg.gameDir = g.u8string();
	cfg.repo = "o/r";
	cfg.publicKeyHex = Hex(pub, 32);
	cfg.installedVersion = "2.0.0";

	// offline: one failure, nothing else
	{
		http.online = false;
		Updater u(cfg, &http, [&](const std::string &l) { log.push_back(l); });
		u.Check(false);
		Pump(u);
		CHECK(u.GetState() == State::Failed);
		CHECK(!u.TakeShowRequest());
		CHECK(!log.empty() && log.back().find("Update check failed") == 0);
		http.online = true;
	}
	// wrong key: refused
	{
		Config bad = cfg;
		bad.publicKeyHex = std::string(64, '1');
		Updater u(bad, &http, nullptr);
		u.Check(false);
		Pump(u);
		u.StartUpdate();
		Pump(u);
		CHECK(u.GetState() == State::InstallFailed);
		CHECK(u.GetError().find("signature") != std::string::npos);
		CHECK(ReadFile(g / "cl_dlls/client.so") == "old client");
	}
	// the whole update
	Updater u(cfg, &http, [&](const std::string &l) { log.push_back(l); });
	u.Check(false);
	Pump(u);
	CHECK(u.GetState() == State::UpdateAvailable);
	CHECK(u.TakeShowRequest());
	CHECK(u.CanInstall());
	CHECK(RunsToPlain(u.ChangelogRuns()).find("2.0.1 - November 2, 2026\nNew\n\xE2\x80\xA2 thing") == 0);
	CHECK(RunsToPlain(u.ChangelogRuns()).find("Older changes") == std::string::npos);
	const int before = http.requests;
	u.StartUpdate();
	// the window's progress: no count while the manifest is on its way, then "Checking files N / 4" from the
	// worker (Frame is not called while waiting, so the state stays Preparing)
	CHECK(u.GetState() == State::Preparing && u.GetProgress().checkTotal == 0);
	for (int i = 0; i < 100 && u.GetState() == State::Preparing && u.GetProgress().checkTotal == 0; i++)
		u.Frame();
	CHECK(u.GetState() == State::Preparing && u.GetProgress().checkTotal == 4);
	CHECK(u.GetProgress().checkDone >= 0 && u.GetProgress().checkDone <= 4);
	for (int i = 0; i < 2000 && u.GetProgress().checkDone < 4; i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	CHECK(u.GetProgress().checkDone == 4);
	Pump(u);
	CHECK(u.GetState() == State::ReadyToInstall);
	CHECK(u.GetProgress().checkTotal == 0);
	// keep.txt is unchanged: 3 files fetched (client, map, cfg beside), the zip link resolved once
	CHECK(u.GetProgress().filesTotal == 3);
	CHECK(http.requests - before == 2 + 1 + 3);
	u.Install();
	Pump(u);
	CHECK(u.GetState() == State::Installed);
	CHECK(ReadFile(g / "cl_dlls/client.so") == "client 2.0.1");
	CHECK(ReadFile(g / "maps/new.bsp") == "a new map");
	CHECK(!fs::exists(g / "maps/old.bsp"));
	CHECK(ReadFile(g / "server.cfg") == "player's own");
	CHECK(ReadFile(g / "server.cfg.new") == "cfg 2.0.1");
	Manifest inst;
	Installer in(g.u8string());
	CHECK(in.ReadInstalled(inst) && inst.version == "2.0.1" && inst.files.size() == 4);
	// skip
	Config now = cfg;
	now.installedVersion = "2.0.0";
	{
		Updater s(now, &http, nullptr);
		s.Check(false);
		Pump(s);
		s.SkipVersion();
		Updater t(now, &http, nullptr);
		t.Check(false);
		Pump(t);
		CHECK(t.GetState() == State::UpdateAvailable && !t.TakeShowRequest());
		t.Check(true);
		CHECK(t.GetState() == State::Checking && t.TakeShowRequest()); // the window opens at once, while checking
		Pump(t);
		CHECK(t.TakeShowRequest());
		// the start's check under way, then asked for: the window opens, and the skipped version shows
		Updater w(now, &http, nullptr);
		w.Check(false);
		CHECK(!w.TakeShowRequest());
		w.Check(true);
		CHECK(w.GetState() == State::Checking && w.TakeShowRequest());
		Pump(w);
		CHECK(w.GetState() == State::UpdateAvailable && w.TakeShowRequest());
	}
	// the marker: no install
	{
		FakeHttp h2 = http;
		std::string &list = h2.files[base + "/releases?per_page=30"];
		list.replace(list.find("SBO 2.0.1"), 9, "SBO 2.0.1 [no auto-update]");
		Updater m(now, &h2, nullptr);
		m.Check(false);
		Pump(m);
		CHECK(m.GetState() == State::UpdateAvailable && !m.CanInstall());
	}
	// a server that ignores ranges: refused
	{
		const fs::path g2 = MakeGame(root, "flow2");
		Config c2 = cfg;
		c2.gameDir = g2.u8string();
		FakeHttp h3 = http;
		h3.ignoreRange = true;
		Updater r(c2, &h3, nullptr);
		r.Check(false);
		Pump(r);
		r.StartUpdate();
		Pump(r);
		CHECK(r.GetState() == State::InstallFailed);
		CHECK(ReadFile(g2 / "cl_dlls/client.so") == "old client");
		CHECK(!fs::exists(g2 / "updater/staging"));
	}
	// disabled build
	{
		Config off = cfg;
		off.repo.clear();
		Updater d(off, &http, nullptr);
		d.Check(true);
		CHECK(d.GetState() == State::Disabled);
	}
}

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		fprintf(stderr, "usage: updater_test <scratch> <deflate file> <plain file>\n");
		return 2;
	}
	const fs::path root = argv[1];
	TestJson();
	TestVersions();
	TestReleases();
	TestPaths();
	TestManifest();
	TestPlan();
	TestCaseRename(root);
	TestMarkdown();
	TestCrypto(argv[2], argv[3]);
	TestInstaller(root);
	TestUpdaterFlow(root);
	if (g_iFailures)
	{
		fprintf(stderr, "%d check(s) failed\n", g_iFailures);
		return 1;
	}
	printf("updater tests: all passed\n");
	return 0;
}
