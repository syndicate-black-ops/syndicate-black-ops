//========= SBO: updater, release logic =========//
//
// Purpose: see updater_core.h.
//===============================================//

#include "updater_core.h"
#include "updater_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <unordered_map>

namespace sbo_update
{
//-----------------------------------------------------------------------------
// Versions
//-----------------------------------------------------------------------------
namespace
{
bool ParseNumber(const char *&s, int &out)
{
	if (*s < '0' || *s > '9')
		return false;
	if (*s == '0' && s[1] >= '0' && s[1] <= '9')
		return false; // no leading zeros (semver)
	long v = 0;
	while (*s >= '0' && *s <= '9')
	{
		v = v * 10 + (*s - '0');
		if (v > 1000000)
			return false;
		s++;
	}
	out = (int)v;
	return true;
}

bool IsIdentChar(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
}

bool AllDigits(const std::string &s)
{
	if (s.empty())
		return false;
	for (char c : s)
		if (c < '0' || c > '9')
			return false;
	return true;
}

std::vector<std::string> SplitDots(const std::string &s)
{
	std::vector<std::string> out;
	size_t start = 0;
	for (;;)
	{
		const size_t dot = s.find('.', start);
		out.push_back(s.substr(start, dot == std::string::npos ? std::string::npos : dot - start));
		if (dot == std::string::npos)
			break;
		start = dot + 1;
	}
	return out;
}

bool ContainsNoCase(const std::string &hay, const char *needle)
{
	const size_t n = strlen(needle);
	if (n == 0 || hay.size() < n)
		return false;
	for (size_t i = 0; i + n <= hay.size(); i++)
	{
		size_t j = 0;
		while (j < n && tolower((unsigned char)hay[i + j]) == tolower((unsigned char)needle[j]))
			j++;
		if (j == n)
			return true;
	}
	return false;
}
} // namespace

bool ParseVersion(const char *text, Version &out)
{
	out = Version();
	if (!text)
		return false;
	const char *s = text;
	if (*s == 'v' || *s == 'V')
		s++;
	Version v;
	if (!ParseNumber(s, v.major) || *s++ != '.' || !ParseNumber(s, v.minor) || *s++ != '.' || !ParseNumber(s, v.patch))
		return false;
	if (*s == '-')
	{
		s++;
		const char *start = s;
		while (IsIdentChar(*s) || *s == '.')
			s++;
		v.pre.assign(start, s);
		if (v.pre.empty() || v.pre.size() > 64)
			return false;
		for (const std::string &id : SplitDots(v.pre))
			if (id.empty())
				return false;
	}
	if (*s == '+')
	{
		s++;
		if (!*s)
			return false;
		while (IsIdentChar(*s) || *s == '.')
			s++;
	}
	if (*s)
		return false;
	v.valid = true;
	out = v;
	return true;
}

int CompareVersions(const Version &a, const Version &b)
{
	if (a.valid != b.valid)
		return a.valid ? 1 : -1;
	if (!a.valid)
		return 0;
	if (a.major != b.major)
		return a.major < b.major ? -1 : 1;
	if (a.minor != b.minor)
		return a.minor < b.minor ? -1 : 1;
	if (a.patch != b.patch)
		return a.patch < b.patch ? -1 : 1;
	if (a.pre.empty() || b.pre.empty())
		return a.pre.empty() == b.pre.empty() ? 0 : (a.pre.empty() ? 1 : -1);
	const std::vector<std::string> ia = SplitDots(a.pre), ib = SplitDots(b.pre);
	for (size_t i = 0; i < ia.size() && i < ib.size(); i++)
	{
		const bool na = AllDigits(ia[i]), nb = AllDigits(ib[i]);
		if (na && nb)
		{
			if (ia[i].size() != ib[i].size())
				return ia[i].size() < ib[i].size() ? -1 : 1;
			const int c = ia[i].compare(ib[i]);
			if (c)
				return c < 0 ? -1 : 1;
		}
		else if (na != nb)
			return na ? -1 : 1; // numeric identifiers come first
		else
		{
			const int c = ia[i].compare(ib[i]);
			if (c)
				return c < 0 ? -1 : 1;
		}
	}
	if (ia.size() != ib.size())
		return ia.size() < ib.size() ? -1 : 1;
	return 0;
}

int CompareVersions(const char *a, const char *b)
{
	Version va, vb;
	ParseVersion(a, va);
	ParseVersion(b, vb);
	return CompareVersions(va, vb);
}

std::string VersionText(const Version &v)
{
	char buf[48];
	snprintf(buf, sizeof(buf), "%d.%d.%d", v.major, v.minor, v.patch);
	std::string s = buf;
	if (!v.pre.empty())
		s += "-" + v.pre;
	return s;
}

//-----------------------------------------------------------------------------
// Releases
//-----------------------------------------------------------------------------
std::string Release::AssetUrl(const std::string &name) const
{
	for (const auto &a : assets)
		if (a.first == name)
			return a.second;
	return "";
}

std::string ReleaseDateText(const std::string &iso)
{
	static const char *const months[] = {"January", "February", "March", "April", "May", "June", "July", "August",
	                                     "September", "October", "November", "December"};
	int y = 0, m = 0, d = 0;
	if (iso.size() < 10 || sscanf(iso.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3 || iso[4] != '-' || iso[7] != '-' ||
	    y < 2000 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31)
		return "";
	char buf[48];
	snprintf(buf, sizeof(buf), "%s %d, %d", months[m - 1], d, y);
	return buf;
}

std::string ReleasesPageUrl(const std::string &page)
{
	const size_t p = page.rfind("/releases/tag/");
	if (page.compare(0, 8, "https://") != 0 || p == std::string::npos)
		return "";
	return page.substr(0, p + 9); // ".../releases"
}

CheckResult EvaluateReleases(const std::string &text, const char *installedVersion, bool includePrereleases)
{
	CheckResult r;
	json::Value root;
	std::string err;
	if (!json::Parse(text.data(), text.size(), root, err))
	{
		r.error = "bad release list (" + err + ")";
		return r;
	}
	if (root.type != json::Value::Array)
	{
		r.error = root.type == json::Value::Object && !root.Str("message").empty() ? "GitHub: " + root.Str("message")
		                                                                            : "bad release list";
		if (r.error.size() > 160)
			r.error.resize(160);
		return r;
	}
	std::vector<Release> list;
	for (const json::Value &rel : root.a)
	{
		if (rel.type != json::Value::Object || rel.Flag("draft"))
			continue;
		Release x;
		x.tag = rel.Str("tag_name");
		if (!ParseVersion(x.tag.c_str(), x.version))
			continue;
		x.prerelease = rel.Flag("prerelease");
		if (x.prerelease && !includePrereleases)
			continue;
		x.name = rel.Str("name");
		if (x.name.empty())
			x.name = x.tag;
		x.body = rel.Str("body");
		if (x.body.size() > MAX_NOTES_BYTES)
		{
			// at a line end, not in the middle of a word
			const size_t nl = x.body.rfind('\n', MAX_NOTES_BYTES);
			x.body.resize(nl != std::string::npos && nl > 0 ? nl : MAX_NOTES_BYTES);
			x.bodyCut = true;
		}
		x.published = rel.Str("published_at");
		x.pageUrl = rel.Str("html_url");
		x.noAutoUpdate = ContainsNoCase(x.name, NO_AUTO_UPDATE_MARKER) || ContainsNoCase(x.body, NO_AUTO_UPDATE_MARKER);
		if (const json::Value *assets = rel.Get("assets"))
			if (assets->type == json::Value::Array)
				for (const json::Value &a : assets->a)
					if (a.type == json::Value::Object && !a.Str("name").empty())
						x.assets.emplace_back(a.Str("name"), a.Str("browser_download_url"));
		list.push_back(std::move(x));
	}
	// GitHub lists by date; the newest version is what counts
	std::stable_sort(list.begin(), list.end(),
	    [](const Release &a, const Release &b) { return CompareVersions(a.version, b.version) > 0; });
	r.ok = true;
	if (list.empty())
		return r;
	Version installed;
	ParseVersion(installedVersion, installed);
	r.latest = list.front();
	r.hasLatest = true;
	for (const Release &x : list)
		if (CompareVersions(x.version, installed) > 0)
			r.newer.push_back(x);
	r.updateAvailable = !r.newer.empty();
	return r;
}

//-----------------------------------------------------------------------------
// Paths and manifests
//-----------------------------------------------------------------------------
std::string LowerAscii(std::string s)
{
	for (char &c : s)
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
	return s;
}

bool IsSafeRelPath(const std::string &path)
{
	if (path.empty() || path.size() >= 200 || path[0] == '/')
		return false;
	for (unsigned char c : path)
		if (c < 0x20 || c >= 0x7F || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
		    c == '>' || c == '|')
			return false;
	static const char *const devices[] = {"con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5", "com6",
	                                      "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6",
	                                      "lpt7", "lpt8", "lpt9"};
	size_t start = 0;
	bool first = true;
	for (;;)
	{
		const size_t slash = path.find('/', start);
		const std::string part = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
		if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
			return false;
		const std::string lower = LowerAscii(part);
		if (first && lower == "updater")
			return false;
		const std::string stem = lower.substr(0, lower.find('.'));
		for (const char *d : devices)
			if (stem == d)
				return false;
		first = false;
		if (slash == std::string::npos)
			break;
		start = slash + 1;
	}
	return true;
}

namespace
{
bool IsSha256(const std::string &s)
{
	if (s.size() != 64)
		return false;
	for (char c : s)
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return false;
	return true;
}
} // namespace

const FileEntry *Manifest::Find(const std::string &path) const
{
	const std::string lower = LowerAscii(path);
	for (const FileEntry &f : files)
		if (LowerAscii(f.path) == lower)
			return &f;
	return nullptr;
}

bool ParseManifest(const std::string &text, bool withZip, Manifest &out, std::string &err)
{
	out = Manifest();
	json::Value root;
	if (!json::Parse(text.data(), text.size(), root, err))
		return false;
	if (root.type != json::Value::Object)
	{
		err = "not an object";
		return false;
	}
	Manifest m;
	m.format = (int)root.Int("format", -1);
	if (m.format != 1)
	{
		err = "unknown manifest format";
		return false;
	}
	m.version = root.Str("version");
	Version v;
	if (!ParseVersion(m.version.c_str(), v))
	{
		err = "bad version";
		return false;
	}
	m.commit = root.Str("commit");
	if (withZip)
	{
		m.zip = root.Str("zip");
		const int64_t zs = root.Int("zip_size");
		if (m.zip.empty() || m.zip.size() > 128 || m.zip.find('/') != std::string::npos ||
		    m.zip.find('\\') != std::string::npos || zs <= 0)
		{
			err = "bad zip";
			return false;
		}
		m.zipSize = (uint64_t)zs;
	}
	const json::Value *files = root.Get("files");
	if (!files || files->type != json::Value::Array || files->a.size() > 50000)
	{
		err = "bad file list";
		return false;
	}
	std::unordered_map<std::string, int> seen; // lower-cased: no two files whose names differ only in case
	std::unordered_map<std::string, int> exact;
	for (const json::Value &f : files->a)
	{
		FileEntry e;
		e.path = f.Str("path");
		const int64_t size = f.Int("size");
		e.sha256 = f.Str("sha256");
		e.keepUserChanges = f.Flag("keep_user_changes");
		if (const json::Value *shipped = f.Get("shipped_sha256"))
		{
			if (shipped->type != json::Value::Array || shipped->a.size() > 256)
			{
				err = "bad shipped_sha256 for \"" + e.path.substr(0, 80) + "\"";
				return false;
			}
			for (const json::Value &h : shipped->a)
			{
				if (h.type != json::Value::String || !IsSha256(h.s))
				{
					err = "bad shipped_sha256 for \"" + e.path.substr(0, 80) + "\"";
					return false;
				}
				e.shippedSha256.push_back(h.s);
			}
		}
		if (!IsSafeRelPath(e.path) || size < 0 || (uint64_t)size > MAX_FILE_SIZE || !IsSha256(e.sha256))
		{
			err = "bad file entry \"" + e.path.substr(0, 80) + "\"";
			return false;
		}
		e.size = (uint64_t)size;
		if (withZip)
		{
			const int64_t off = f.Int("zip_offset"), zsz = f.Int("zip_size");
			const std::string method = f.Str("method");
			e.method = method == "deflate" ? 8 : method == "store" ? 0 : -1;
			if (off < 0 || zsz < 0 || e.method < 0 || (uint64_t)off > m.zipSize || (uint64_t)zsz > m.zipSize - (uint64_t)off ||
			    (e.method == 0 && (uint64_t)zsz != e.size) || (uint64_t)zsz > MAX_FILE_SIZE)
			{
				err = "bad zip range for \"" + e.path.substr(0, 80) + "\"";
				return false;
			}
			e.zipOffset = (uint64_t)off;
			e.zipSize = (uint64_t)zsz;
		}
		if (!seen.emplace(LowerAscii(e.path), 1).second)
		{
			err = "duplicate file \"" + e.path.substr(0, 80) + "\"";
			return false;
		}
		exact.emplace(e.path, 1);
		m.files.push_back(std::move(e));
	}
	if (const json::Value *rem = root.Get("remove"))
	{
		if (rem->type != json::Value::Array || rem->a.size() > 50000)
		{
			err = "bad remove list";
			return false;
		}
		for (const json::Value &r : rem->a)
		{
			RemoveEntry e;
			e.path = r.Str("path");
			e.sha256 = r.Str("sha256");
			// a file of the release under another case (a case-only rename) is allowed: MakePlan removes it only
			// where it is a file of its own
			if (!IsSafeRelPath(e.path) || (!e.sha256.empty() && !IsSha256(e.sha256)) || exact.count(e.path))
			{
				err = "bad remove entry \"" + e.path.substr(0, 80) + "\"";
				return false;
			}
			m.remove.push_back(std::move(e));
		}
	}
	out = std::move(m);
	return true;
}

std::string InstalledJson(const Manifest &m)
{
	std::string s = "{\n  \"format\": 1,\n  \"version\": " + json::Quote(m.version) + ",\n  \"commit\": " +
	                json::Quote(m.commit) + ",\n  \"files\": [\n";
	for (size_t i = 0; i < m.files.size(); i++)
	{
		const FileEntry &f = m.files[i];
		char size[32];
		snprintf(size, sizeof(size), "%llu", (unsigned long long)f.size);
		s += "    {\"path\": " + json::Quote(f.path) + ", \"size\": " + size + ", \"sha256\": \"" + f.sha256 + "\"";
		if (f.keepUserChanges)
			s += ", \"keep_user_changes\": true";
		s += i + 1 < m.files.size() ? "},\n" : "}\n";
	}
	s += "  ]\n}\n";
	return s;
}

//-----------------------------------------------------------------------------
// Install plan
//-----------------------------------------------------------------------------
std::vector<PlanItem> MakePlan(const Manifest *installed, const Manifest &next,
    const std::function<DiskFile(const std::string &)> &disk,
    const std::function<bool(const std::string &, const std::string &)> &sameFile)
{
	std::unordered_map<std::string, const FileEntry *> old, now;
	if (installed)
		for (const FileEntry &f : installed->files)
			old.emplace(LowerAscii(f.path), &f);
	for (const FileEntry &f : next.files)
		now.emplace(LowerAscii(f.path), &f);

	std::vector<PlanItem> plan;
	for (const FileEntry &f : next.files)
	{
		const DiskFile d = disk(f.path);
		if (d.exists && d.size == f.size && d.sha256 == f.sha256)
			continue;
		if (!d.exists)
		{
			plan.push_back({Action::Add, f.path, &f});
			continue;
		}
		if (f.keepUserChanges)
		{
			// as an earlier release shipped it (installed.json, else the release's list of shipped versions):
			// replaced, so new example lines arrive; else the player's edit stays and ours goes beside it
			auto it = old.find(LowerAscii(f.path));
			bool asShipped = it != old.end() && d.sha256 == it->second->sha256;
			for (const std::string &h : f.shippedSha256)
				asShipped = asShipped || d.sha256 == h;
			plan.push_back({asShipped ? Action::Replace : Action::KeepUser, f.path, &f});
			continue;
		}
		plan.push_back({Action::Replace, f.path, &f});
	}
	// a path the new release ships under the same name, case aside: kept, unless the name differs in case and the
	// old name is a file of its own (case-sensitive file system: both names would stay)
	auto shippedAs = [&](const std::string &path) {
		auto it = now.find(LowerAscii(path));
		if (it == now.end())
			return false;
		if (it->second->path == path)
			return true;
		return !sameFile || sameFile(path, it->second->path);
	};
	if (installed)
		for (const FileEntry &o : installed->files)
		{
			if (shippedAs(o.path))
				continue;
			const DiskFile d = disk(o.path);
			if (!d.exists)
				continue;
			plan.push_back({d.sha256 == o.sha256 ? Action::Remove : Action::KeptRemoved, o.path, nullptr});
		}
	for (const RemoveEntry &r : next.remove)
	{
		bool listed = false;
		for (const PlanItem &p : plan)
			listed = listed || p.path == r.path;
		if (listed || shippedAs(r.path))
			continue;
		const DiskFile d = disk(r.path);
		if (!d.exists)
			continue;
		plan.push_back({r.sha256.empty() || d.sha256 == r.sha256 ? Action::Remove : Action::KeptRemoved, r.path, nullptr});
	}
	return plan;
}

//-----------------------------------------------------------------------------
// Markdown
//-----------------------------------------------------------------------------
namespace
{
struct RunBuilder
{
	std::vector<TextRun> runs;
	int lineIndent = 0;
	bool lineStart = true;

	void Add(RunStyle style, const std::string &text, const std::string &url = "")
	{
		if (text.empty())
			return;
		if (!lineStart && !runs.empty() && runs.back().style == style && runs.back().url == url)
		{
			runs.back().text += text;
			return;
		}
		TextRun r;
		r.style = style;
		r.text = text;
		r.url = url;
		r.indent = lineStart ? lineIndent : 0;
		runs.push_back(std::move(r));
		lineStart = false;
	}

	void EndLine()
	{
		if (lineStart)
		{
			// an empty line: one newline (two blank lines in a row become one)
			if (!runs.empty() && runs.back().text.size() >= 2 &&
			    runs.back().text.compare(runs.back().text.size() - 2, 2, "\n\n") == 0)
				return;
			if (runs.empty())
				return;
			Newline();
			return;
		}
		Newline();
		lineStart = true;
		lineIndent = 0;
	}

	//! "\n" after the last run (a link's text stays only its label)
	void Newline()
	{
		if (runs.back().style == RunStyle::Link)
		{
			TextRun r;
			r.text = "\n";
			runs.push_back(std::move(r));
		}
		else
			runs.back().text += "\n";
	}
};

bool SafeUrl(const std::string &u)
{
	if (u.compare(0, 8, "https://") != 0 || u.size() > 512)
		return false;
	for (unsigned char c : u)
		if (c <= 0x20 || c >= 0x7F || c == '"')
			return false;
	return true;
}

//! Inline markdown of one line (or a heading's text) in base style
void Inline(RunBuilder &b, const std::string &s, RunStyle base)
{
	std::string text;
	auto flush = [&](RunStyle st) {
		b.Add(st, text);
		text.clear();
	};
	size_t i = 0;
	const size_t n = s.size();
	while (i < n)
	{
		const char c = s[i];
		if (c == '\\' && i + 1 < n && strchr("\\`*_{}[]()#+-.!<>|~", s[i + 1]))
		{
			text += s[i + 1];
			i += 2;
			continue;
		}
		if (c == '`')
		{
			const size_t close = s.find('`', i + 1);
			if (close != std::string::npos)
			{
				flush(base);
				b.Add(RunStyle::Code, s.substr(i + 1, close - i - 1));
				i = close + 1;
				continue;
			}
		}
		if ((c == '*' || c == '_') && i + 1 < n && s[i + 1] == c)
		{
			const char delim[3] = {c, c, 0};
			const size_t close = s.find(delim, i + 2);
			if (close != std::string::npos && close > i + 2)
			{
				flush(base);
				RunBuilder inner;
				Inline(inner, s.substr(i + 2, close - i - 2), RunStyle::Bold);
				for (TextRun &r : inner.runs)
					b.Add(r.style == RunStyle::Normal ? RunStyle::Bold : r.style, r.text, r.url);
				i = close + 2;
				continue;
			}
		}
		if (c == '*' && i + 1 < n && s[i + 1] != ' ' && s[i + 1] != '*')
		{
			// *emphasis*: the markers go
			const size_t close = s.find('*', i + 1);
			if (close != std::string::npos && s[close - 1] != ' ')
			{
				text += s.substr(i + 1, close - i - 1);
				i = close + 1;
				continue;
			}
		}
		if (c == '~' && i + 1 < n && s[i + 1] == '~')
		{
			i += 2; // ~~strike~~: markers dropped
			continue;
		}
		if ((c == '[' || (c == '!' && i + 1 < n && s[i + 1] == '[')))
		{
			const bool image = c == '!';
			const size_t open = image ? i + 1 : i;
			const size_t closeText = s.find(']', open + 1);
			if (closeText != std::string::npos && closeText + 1 < n && s[closeText + 1] == '(')
			{
				const size_t closeUrl = s.find(')', closeText + 2);
				if (closeUrl != std::string::npos)
				{
					const std::string label = s.substr(open + 1, closeText - open - 1);
					std::string url = s.substr(closeText + 2, closeUrl - closeText - 2);
					const size_t sp = url.find(' ');
					if (sp != std::string::npos)
						url.resize(sp); // [text](url "title")
					flush(base);
					if (image)
						b.Add(RunStyle::Dim, label.empty() ? "[image]" : label);
					else if (SafeUrl(url))
						b.Add(RunStyle::Link, label.empty() ? url : label, url);
					else
						b.Add(base, label);
					i = closeUrl + 1;
					continue;
				}
			}
		}
		if (c == '<')
		{
			const size_t close = s.find('>', i + 1);
			if (close != std::string::npos && close - i < 600)
			{
				const std::string inner = s.substr(i + 1, close - i - 1);
				if (SafeUrl(inner))
				{
					flush(base);
					b.Add(RunStyle::Link, inner, inner);
				}
				else if (LowerAscii(inner) == "br" || LowerAscii(inner) == "br/" || LowerAscii(inner) == "br /")
				{
					flush(base);
					b.EndLine();
				}
				// any other tag: dropped
				i = close + 1;
				continue;
			}
		}
		text += c;
		i++;
	}
	flush(base);
}

std::string TrimLeft(const std::string &s, size_t &spaces)
{
	spaces = 0;
	while (spaces < s.size() && (s[spaces] == ' ' || s[spaces] == '\t'))
		spaces++;
	return s.substr(spaces);
}

bool IsRule(const std::string &t)
{
	if (t.size() < 3)
		return false;
	const char c = t[0];
	if (c != '-' && c != '*' && c != '_')
		return false;
	int count = 0;
	for (char x : t)
	{
		if (x == c)
			count++;
		else if (x != ' ')
			return false;
	}
	return count >= 3;
}
} // namespace

std::vector<TextRun> MarkdownToRuns(const std::string &markdownIn, size_t maxBytes)
{
	std::string md = markdownIn.substr(0, std::min(markdownIn.size(), maxBytes));
	if (markdownIn.size() > maxBytes)
	{
		// cut at a line end, not in the middle of a word
		const size_t nl = md.rfind('\n');
		if (nl != std::string::npos && nl > 0)
			md.resize(nl);
	}
	RunBuilder b;
	bool inCode = false;
	size_t pos = 0;
	while (pos <= md.size())
	{
		size_t eol = md.find('\n', pos);
		if (eol == std::string::npos)
			eol = md.size();
		std::string line = md.substr(pos, eol - pos);
		pos = eol + 1;
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		size_t spaces;
		const std::string t = TrimLeft(line, spaces);

		if (t.compare(0, 3, "```") == 0 || t.compare(0, 3, "~~~") == 0)
		{
			inCode = !inCode;
			continue;
		}
		if (inCode)
		{
			b.Add(RunStyle::Code, line.empty() ? " " : line);
			b.EndLine();
			continue;
		}
		if (t.empty())
		{
			b.EndLine();
			continue;
		}
		if (IsRule(t))
		{
			b.EndLine();
			continue;
		}
		size_t hashes = 0;
		while (hashes < t.size() && t[hashes] == '#')
			hashes++;
		if (hashes >= 1 && hashes <= 6 && (hashes == t.size() || t[hashes] == ' '))
		{
			std::string h = hashes < t.size() ? t.substr(hashes + 1) : "";
			while (!h.empty() && (h.back() == '#' || h.back() == ' '))
				h.pop_back();
			Inline(b, h, RunStyle::Heading);
			b.EndLine();
			continue;
		}
		if (t[0] == '>')
		{
			Inline(b, TrimLeft(t.substr(1), spaces), RunStyle::Dim);
			b.EndLine();
			continue;
		}
		if (t[0] == '|')
		{
			// table: a separator row goes, cells joined with "  "
			bool sep = true;
			for (char c : t)
				sep = sep && (c == '|' || c == '-' || c == ':' || c == ' ');
			if (sep)
				continue;
			std::string row;
			size_t start = 1;
			while (start < t.size())
			{
				size_t bar = t.find('|', start);
				if (bar == std::string::npos)
					bar = t.size();
				std::string cell = t.substr(start, bar - start);
				size_t sp;
				cell = TrimLeft(cell, sp);
				while (!cell.empty() && cell.back() == ' ')
					cell.pop_back();
				if (!cell.empty())
					row += (row.empty() ? "" : "  ") + cell;
				start = bar + 1;
			}
			Inline(b, row, RunStyle::Normal);
			b.EndLine();
			continue;
		}
		const int level = (int)std::min<size_t>(spaces / 2, 3) + 1;
		if ((t[0] == '-' || t[0] == '*' || t[0] == '+') && t.size() > 1 && t[1] == ' ')
		{
			b.lineIndent = level;
			std::string item = t.substr(2);
			if (item.compare(0, 4, "[ ] ") == 0 || item.compare(0, 4, "[x] ") == 0 || item.compare(0, 4, "[X] ") == 0)
				item = item.substr(4); // task list boxes
			b.Add(RunStyle::Normal, "\xE2\x80\xA2 "); // "• "
			Inline(b, item, RunStyle::Normal);
			b.EndLine();
			continue;
		}
		size_t digits = 0;
		while (digits < t.size() && digits < 4 && t[digits] >= '0' && t[digits] <= '9')
			digits++;
		if (digits > 0 && digits + 1 < t.size() && (t[digits] == '.' || t[digits] == ')') && t[digits + 1] == ' ')
		{
			b.lineIndent = level;
			b.Add(RunStyle::Normal, t.substr(0, digits + 1) + " ");
			Inline(b, t.substr(digits + 2), RunStyle::Normal);
			b.EndLine();
			continue;
		}
		Inline(b, t, RunStyle::Normal);
		b.EndLine();
	}
	// no trailing blank lines
	while (!b.runs.empty())
	{
		std::string &last = b.runs.back().text;
		while (last.size() >= 2 && last[last.size() - 1] == '\n' && last[last.size() - 2] == '\n')
			last.pop_back();
		break;
	}
	return b.runs;
}

std::string RunsToPlain(const std::vector<TextRun> &runs)
{
	std::string out;
	for (const TextRun &r : runs)
	{
		if (r.indent > 0 && (out.empty() || out.back() == '\n'))
			out.append((size_t)(r.indent - 1) * 2, ' ');
		out += r.text;
		if (r.style == RunStyle::Link && r.text != r.url)
			out += " (" + r.url + ")";
	}
	return out;
}
} // namespace sbo_update
