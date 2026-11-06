//========= SBO: updater, staging and install =========//
//
// Purpose: see updater_install.h.
//
// Journal (updater/apply.journal), one line each, appended and flushed as the install goes:
//   SBOUPDATE 1
//   version <x.y.z>
//   step <A|R|N|D> <size> <sha256 or -> <path>     every step, before any file is touched
//   begin
//   done <index> / deferred <index>                 after each step
//   commit                                          installed.json written
//=====================================================//

#include "updater_install.h"
#include "updater_json.h"
#include "updater_sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace sbo_update
{
#ifdef SBO_UPDATER_TESTING
int Installer::s_iTestFailStep = -1;
int Installer::s_iTestCrashAfter = -1;
std::string Installer::s_TestLockedPath;
#endif

namespace
{
const char JOURNAL[] = "updater/apply.journal";
const char INSTALLED[] = "updater/installed.json";
const char INSTALLED_NEW[] = "updater/installed.json.new";
const char DONE_LIST[] = "updater/apply.done";
const char STATE[] = "updater/state.json";
const char STAGING[] = "updater/staging";
const char OLD_SUFFIX[] = ".sbo-old";

fs::path P(const std::string &utf8)
{
	return fs::u8path(utf8);
}

bool Exists(const fs::path &p)
{
	std::error_code ec;
	return fs::is_regular_file(p, ec);
}

bool IsLockError(const std::error_code &ec)
{
#ifdef _WIN32
	return ec.category() == std::system_category() &&
	       (ec.value() == ERROR_SHARING_VIOLATION || ec.value() == ERROR_LOCK_VIOLATION || ec.value() == ERROR_ACCESS_DENIED);
#else
	(void)ec;
	return false;
#endif
}

bool ReadText(const fs::path &p, std::string &out, size_t maxBytes)
{
	std::ifstream f(p, std::ios::binary);
	if (!f)
		return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return out.size() <= maxBytes;
}

//! Writes through <p>.tmp and a rename, so a reader never sees half a file
bool WriteText(const fs::path &p, const std::string &text)
{
	std::error_code ec;
	fs::create_directories(p.parent_path(), ec);
	fs::path tmp = p;
	tmp += ".tmp";
	{
		std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
		if (!f)
			return false;
		f.write(text.data(), (std::streamsize)text.size());
		f.flush();
		if (!f)
			return false;
	}
	fs::rename(tmp, p, ec);
	return !ec;
}
} // namespace

bool HashFileUtf8(const std::string &path, uint64_t &size, std::string &sha256)
{
	std::ifstream f(P(path), std::ios::binary);
	if (!f)
		return false;
	Sha256 h;
	std::vector<char> buf(1 << 16);
	size = 0;
	while (f)
	{
		f.read(buf.data(), (std::streamsize)buf.size());
		const std::streamsize got = f.gcount();
		if (got <= 0)
			break;
		h.Update(buf.data(), (size_t)got);
		size += (uint64_t)got;
	}
	if (f.bad())
		return false;
	sha256 = h.FinalHex();
	return true;
}

std::string GameDirOfModule(const void *addressInModule)
{
	fs::path lib;
#ifdef _WIN32
	HMODULE h = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	        (LPCWSTR)addressInModule, &h))
		return "";
	wchar_t buf[MAX_PATH * 2];
	const DWORD n = GetModuleFileNameW(h, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
	if (n == 0 || n >= sizeof(buf) / sizeof(buf[0]))
		return "";
	lib = fs::path(std::wstring(buf, n));
#else
	Dl_info info;
	if (!dladdr(addressInModule, &info) || !info.dli_fname)
		return "";
	lib = fs::u8path(info.dli_fname);
#endif
	std::error_code ec;
	lib = fs::absolute(lib, ec);
	if (ec)
		return "";
	const fs::path game = lib.parent_path().parent_path(); // <game>/cl_dlls/client.*
	if (!Exists(game / "liblist.gam"))
		return "";
	return game.u8string();
}

struct Installer::Journal
{
	std::string version;
	std::vector<InstallStep> steps;
	bool begun = false, committed = false;
	std::set<int> done, deferred;
};

Installer::Installer(const std::string &gameDir)
    : m_GameDir(gameDir)
{
	while (!m_GameDir.empty() && (m_GameDir.back() == '/' || m_GameDir.back() == '\\'))
		m_GameDir.pop_back();
}

static fs::path Target(const std::string &game, const std::string &rel)
{
	return P(game + "/" + rel);
}
static fs::path Staged(const std::string &game, const std::string &rel)
{
	return P(game + "/" + STAGING + "/" + rel);
}
static fs::path Old(const std::string &game, const std::string &rel)
{
	return P(game + "/" + rel + OLD_SUFFIX);
}
static fs::path NewBeside(const std::string &game, const std::string &rel)
{
	return P(game + "/" + rel + ".new");
}

bool Installer::WriteStaged(const std::string &rel, const void *data, size_t len, std::string &err)
{
	if (!IsSafeRelPath(rel))
	{
		err = "unsafe path";
		return false;
	}
	const fs::path dst = Staged(m_GameDir, rel);
	std::error_code ec;
	fs::create_directories(dst.parent_path(), ec);
	if (ec)
	{
		err = "cannot create " + dst.parent_path().u8string() + " (" + ec.message() + ")";
		return false;
	}
	fs::path part = dst;
	part += ".part";
	{
		std::ofstream f(part, std::ios::binary | std::ios::trunc);
		if (!f)
		{
			err = "cannot write " + part.u8string();
			return false;
		}
		f.write((const char *)data, (std::streamsize)len);
		f.flush();
		if (!f)
		{
			err = "write failed: " + part.u8string();
			return false;
		}
	}
	fs::rename(part, dst, ec);
	if (ec)
	{
		err = "cannot rename " + part.u8string() + " (" + ec.message() + ")";
		return false;
	}
	return true;
}

bool Installer::StagedMatches(const std::string &rel, uint64_t size, const std::string &sha256)
{
	uint64_t s = 0;
	std::string h;
	return HashFileUtf8(Staged(m_GameDir, rel).u8string(), s, h) && s == size && h == sha256;
}

void Installer::ClearStaging()
{
	std::error_code ec;
	fs::remove_all(P(m_GameDir + "/" + STAGING), ec);
}

DiskFile Installer::Disk(const std::string &rel)
{
	DiskFile d;
	const fs::path p = Target(m_GameDir, rel);
	if (!Exists(p))
		return d;
	d.exists = HashFileUtf8(p.u8string(), d.size, d.sha256);
	if (!d.exists)
	{
		// there but unreadable: counts as different from everything
		d.exists = true;
		d.sha256 = "unreadable";
	}
	return d;
}

bool Installer::SameFile(const std::string &relA, const std::string &relB)
{
	std::error_code ec;
	const bool same = fs::equivalent(Target(m_GameDir, relA), Target(m_GameDir, relB), ec);
	return !ec && same;
}

bool Installer::ReadInstalled(Manifest &out)
{
	std::string text, err;
	if (!ReadText(P(m_GameDir + "/" + INSTALLED), text, 16 * 1024 * 1024))
		return false;
	return ParseManifest(text, false, out, err);
}

bool Installer::AppendJournal(const std::string &line)
{
	std::ofstream f(P(m_GameDir + "/" + JOURNAL), std::ios::binary | std::ios::app);
	if (!f)
		return false;
	f << line << "\n";
	f.flush();
	return (bool)f;
}

bool Installer::ReadJournal(Journal &j)
{
	std::string text;
	if (!ReadText(P(m_GameDir + "/" + JOURNAL), text, 16 * 1024 * 1024))
		return false;
	std::istringstream in(text);
	std::string line;
	if (!std::getline(in, line) || line != "SBOUPDATE 1")
		return false;
	while (std::getline(in, line))
	{
		if (line.compare(0, 8, "version ") == 0)
			j.version = line.substr(8);
		else if (line.compare(0, 5, "step ") == 0 && !j.begun)
		{
			// step <kind> <size> <sha> <path>
			InstallStep s;
			char sha[80] = {0};
			unsigned long long size = 0;
			int consumed = 0;
			char kind = 0;
			if (sscanf(line.c_str() + 5, "%c %llu %79s %n", &kind, &size, sha, &consumed) < 3 || consumed <= 0)
				return false;
			s.kind = kind;
			s.size = size;
			s.sha256 = strcmp(sha, "-") ? sha : "";
			s.path = line.substr(5 + (size_t)consumed);
			if (!strchr("ARND", kind) || !IsSafeRelPath(s.path))
				return false;
			j.steps.push_back(s);
		}
		else if (line == "begin")
			j.begun = true;
		else if (line.compare(0, 5, "done ") == 0)
			j.done.insert(atoi(line.c_str() + 5));
		else if (line.compare(0, 9, "deferred ") == 0)
			j.deferred.insert(atoi(line.c_str() + 9));
		else if (line == "commit")
			j.committed = true;
	}
	return true;
}

Installer::StepResult Installer::DoStep(const InstallStep &s, std::string &err)
{
	const fs::path t = Target(m_GameDir, s.path), o = Old(m_GameDir, s.path), st = Staged(m_GameDir, s.path);
	std::error_code ec;
#ifdef SBO_UPDATER_TESTING
	if (!s_TestLockedPath.empty() && s.path == s_TestLockedPath)
		return StepResult::Locked;
#endif
	auto moveAway = [&]() -> StepResult {
		if (Exists(o))
		{
			fs::remove(o, ec);
			if (ec)
				return IsLockError(ec) ? StepResult::Locked : (err = "cannot remove " + o.u8string() + " (" + ec.message() + ")", StepResult::Error);
		}
		fs::rename(t, o, ec);
		if (ec)
			return IsLockError(ec) ? StepResult::Locked : (err = "cannot rename " + t.u8string() + " (" + ec.message() + ")", StepResult::Error);
		return StepResult::Ok;
	};

	switch (s.kind)
	{
	case 'A':
	case 'R':
		if (!Exists(st))
		{
			uint64_t size = 0;
			std::string sha;
			if (Exists(t) && HashFileUtf8(t.u8string(), size, sha) && size == s.size && sha == s.sha256)
				return StepResult::Ok; // installed before an interruption
			err = "staged file missing: " + s.path;
			return StepResult::Error;
		}
		if (Exists(t))
		{
			const StepResult r = moveAway();
			if (r != StepResult::Ok)
				return r;
		}
		fs::create_directories(t.parent_path(), ec);
		ec.clear();
		fs::rename(st, t, ec);
		if (ec)
		{
			err = "cannot install " + s.path + " (" + ec.message() + ")";
			return StepResult::Error;
		}
		return StepResult::Ok;
	case 'N':
		if (Exists(st))
		{
			fs::rename(st, NewBeside(m_GameDir, s.path), ec);
			if (ec)
			{
				err = "cannot install " + s.path + ".new (" + ec.message() + ")";
				return IsLockError(ec) ? StepResult::Locked : StepResult::Error;
			}
		}
		return StepResult::Ok;
	case 'D':
		if (Exists(t))
			return moveAway();
		return StepResult::Ok;
	}
	err = "bad step";
	return StepResult::Error;
}

void Installer::UndoStep(const InstallStep &s)
{
	const fs::path t = Target(m_GameDir, s.path), o = Old(m_GameDir, s.path), st = Staged(m_GameDir, s.path);
	std::error_code ec;
	switch (s.kind)
	{
	case 'R':
		if (Exists(o))
		{
			if (Exists(t))
			{
				fs::create_directories(st.parent_path(), ec);
				fs::rename(t, st, ec); // the new file back to staging
			}
			fs::rename(o, t, ec);
		}
		break;
	case 'A':
		if (Exists(t) && !Exists(st))
		{
			fs::create_directories(st.parent_path(), ec);
			fs::rename(t, st, ec);
		}
		break;
	case 'D':
		if (Exists(o) && !Exists(t))
			fs::rename(o, t, ec);
		break;
	default:
		break;
	}
}

bool Installer::Commit(std::string &err)
{
	std::error_code ec;
	const fs::path inew = P(m_GameDir + "/" + INSTALLED_NEW);
	if (Exists(inew))
	{
		fs::rename(inew, P(m_GameDir + "/" + INSTALLED), ec);
		if (ec)
		{
			err = "cannot write installed.json (" + ec.message() + ")";
			return false;
		}
	}
	AppendJournal("commit");
	// the backups to delete later: replaced and removed files
	Journal j;
	std::string list;
	if (ReadJournal(j))
		for (const InstallStep &s : j.steps)
			if (s.kind == 'R' || s.kind == 'D')
				list += s.path + "\n";
	std::string existing;
	if (ReadText(P(m_GameDir + "/" + DONE_LIST), existing, 16 * 1024 * 1024))
		list = existing + list;
	WriteText(P(m_GameDir + "/" + DONE_LIST), list);
	fs::remove(P(m_GameDir + "/" + JOURNAL), ec);
	ClearStaging();
	return true;
}

ApplyResult Installer::Apply(const std::vector<InstallStep> &stepsIn, const std::string &version,
    const std::string &installedJson, std::string &err)
{
	std::error_code ec;
	if (Exists(P(m_GameDir + "/" + JOURNAL)))
	{
		err = "an earlier install is not finished (restart the game)";
		return ApplyResult::Failed;
	}
	// check everything before the first change
	std::vector<InstallStep> steps;
	for (const InstallStep &in : stepsIn)
	{
		InstallStep s = in;
		if (!IsSafeRelPath(s.path) || !strchr("FND", s.kind))
		{
			err = "bad step " + s.path;
			return ApplyResult::Failed;
		}
		if (s.kind == 'F' || s.kind == 'N')
		{
			if (!StagedMatches(s.path, s.size, s.sha256))
			{
				err = "staged file damaged: " + s.path;
				return ApplyResult::Failed;
			}
			if (s.kind == 'F')
				s.kind = Exists(Target(m_GameDir, s.path)) ? 'R' : 'A';
		}
		// a backup left from an older install
		const fs::path o = Old(m_GameDir, s.path);
		if (Exists(o))
		{
			fs::remove(o, ec);
			if (ec)
			{
				err = "cannot remove " + o.u8string() + " (" + ec.message() + ")";
				return ApplyResult::Failed;
			}
		}
		steps.push_back(s);
	}
	if (!WriteText(P(m_GameDir + "/" + INSTALLED_NEW), installedJson))
	{
		err = "cannot write to the updater folder";
		return ApplyResult::Failed;
	}
	// the journal, complete, before any game file changes
	fs::remove(P(m_GameDir + "/" + JOURNAL), ec);
	bool ok = AppendJournal("SBOUPDATE 1") && AppendJournal("version " + version);
	for (const InstallStep &s : steps)
	{
		char head[128];
		snprintf(head, sizeof(head), "step %c %llu %s ", s.kind, (unsigned long long)s.size,
		    s.sha256.empty() ? "-" : s.sha256.c_str());
		ok = ok && AppendJournal(head + s.path);
	}
	ok = ok && AppendJournal("begin");
	if (!ok)
	{
		fs::remove(P(m_GameDir + "/" + JOURNAL), ec);
		fs::remove(P(m_GameDir + "/" + INSTALLED_NEW), ec);
		err = "cannot write the install journal";
		return ApplyResult::Failed;
	}

	bool deferred = false;
	for (size_t i = 0; i < steps.size(); i++)
	{
#ifdef SBO_UPDATER_TESTING
		if (s_iTestCrashAfter >= 0 && (int)i == s_iTestCrashAfter)
		{
			err = "test crash";
			return ApplyResult::Failed; // journal left as a crash leaves it
		}
		StepResult r = s_iTestFailStep == (int)i ? (err = "test failure", StepResult::Error) : DoStep(steps[i], err);
#else
		StepResult r = DoStep(steps[i], err);
#endif
		if (r == StepResult::Ok)
		{
			AppendJournal("done " + std::to_string(i));
			continue;
		}
		if (r == StepResult::Locked)
		{
			AppendJournal("deferred " + std::to_string(i));
			deferred = true;
			continue;
		}
		// undo everything, this step included (it may be half done)
		for (size_t k = i + 1; k-- > 0;)
			UndoStep(steps[k]);
		fs::remove(P(m_GameDir + "/" + JOURNAL), ec);
		fs::remove(P(m_GameDir + "/" + INSTALLED_NEW), ec);
		return ApplyResult::Failed;
	}
	if (deferred)
		return ApplyResult::Deferred;
	if (!Commit(err))
		return ApplyResult::Failed;
	return ApplyResult::Done;
}

bool Installer::HasPendingInstall()
{
	return Exists(P(m_GameDir + "/" + JOURNAL));
}

std::string Installer::Recover()
{
	std::error_code ec;
	const fs::path jp = P(m_GameDir + "/" + JOURNAL);
	if (!Exists(jp))
		return "";
	Journal j;
	if (!ReadJournal(j))
	{
		fs::remove(jp, ec);
		return "Updater: unreadable install journal removed";
	}
	std::string err;
	if (j.committed)
	{
		Commit(err);
		return "Updater: install of " + j.version + " finished";
	}
	if (!j.begun)
	{
		fs::remove(jp, ec);
		fs::remove(P(m_GameDir + "/" + INSTALLED_NEW), ec);
		return "Updater: install of " + j.version + " had not started; nothing changed";
	}
	// forward if every step not done yet can still be done
	bool forward = true;
	for (size_t i = 0; i < j.steps.size() && forward; i++)
	{
		const InstallStep &s = j.steps[i];
		if (j.done.count((int)i) || s.kind == 'D')
			continue;
		if (s.kind == 'N')
			continue;
		if (StagedMatches(s.path, s.size, s.sha256))
			continue;
		uint64_t size = 0;
		std::string sha;
		const fs::path t = Target(m_GameDir, s.path);
		forward = Exists(t) && HashFileUtf8(t.u8string(), size, sha) && size == s.size && sha == s.sha256;
	}
	if (forward)
	{
		int stillLocked = 0;
		for (size_t i = 0; i < j.steps.size(); i++)
		{
			if (j.done.count((int)i))
				continue;
			const StepResult r = DoStep(j.steps[i], err);
			if (r == StepResult::Ok)
				AppendJournal("done " + std::to_string(i));
			else if (r == StepResult::Locked)
				stillLocked++;
			else
			{
				forward = false;
				break;
			}
		}
		if (forward && stillLocked)
			return "Updater: install of " + j.version + ": " + std::to_string(stillLocked) +
			       " file(s) still in use, finished at the next start";
		if (forward && Commit(err))
			return "Updater: install of " + j.version + " finished";
	}
	for (size_t k = j.steps.size(); k-- > 0;)
		UndoStep(j.steps[k]);
	fs::remove(jp, ec);
	fs::remove(P(m_GameDir + "/" + INSTALLED_NEW), ec);
	return "Updater: install of " + j.version + " was interrupted and has been undone" + (err.empty() ? "" : " (" + err + ")");
}

std::string Installer::CleanupOld()
{
	const fs::path dp = P(m_GameDir + "/" + DONE_LIST);
	std::string text;
	if (!ReadText(dp, text, 16 * 1024 * 1024))
		return "";
	std::istringstream in(text);
	std::string line, left;
	int removed = 0, kept = 0;
	while (std::getline(in, line))
	{
		if (line.empty() || !IsSafeRelPath(line))
			continue;
		const fs::path o = Old(m_GameDir, line);
		std::error_code ec;
		if (!Exists(o))
			continue;
		if (fs::remove(o, ec))
		{
			removed++;
			// the folders this leaves empty, up to (not including) the game folder
			for (size_t slash = line.rfind('/'); slash != std::string::npos && slash > 0; slash = line.rfind('/', slash - 1))
			{
				const fs::path dir = Target(m_GameDir, line.substr(0, slash));
				std::error_code dec;
				if (!fs::is_directory(dir, dec) || !fs::is_empty(dir, dec) || dec || !fs::remove(dir, dec))
					break;
			}
		}
		else
		{
			kept++;
			left += line + "\n";
		}
	}
	std::error_code ec;
	if (left.empty())
		fs::remove(dp, ec);
	else
		WriteText(dp, left);
	if (!removed && !kept)
		return "";
	return "Updater: removed " + std::to_string(removed) + " backup file(s) of the previous version" +
	       (kept ? ", " + std::to_string(kept) + " still in use" : "");
}

std::string Installer::ReadSkippedVersion()
{
	std::string text, err;
	if (!ReadText(P(m_GameDir + "/" + STATE), text, 64 * 1024))
		return "";
	json::Value v;
	if (!json::Parse(text.data(), text.size(), v, err))
		return "";
	return v.Str("skip");
}

void Installer::WriteSkippedVersion(const std::string &version)
{
	WriteText(P(m_GameDir + "/" + STATE), "{\n  \"skip\": " + json::Quote(version) + "\n}\n");
}
} // namespace sbo_update
