//========= SBO: updater, staging and install =========//
//
// Purpose: the updater's files in the game folder (docs/UPDATER.md), plain C++ (std::filesystem, no SDK or SBO
//          headers; tests/updater/run_test.sh runs it on a scratch folder with ASan/UBSan):
//            <game>/updater/staging/<path>   downloaded and checked files waiting to be installed
//            <game>/updater/apply.journal    the install in progress (every step, then "done <n>" after each)
//            <game>/updater/installed.json   the installed version's file list (sizes, sha256)
//            <game>/updater/apply.done       the replaced files' backups (<path>.sbo-old) to delete
//            <game>/updater/state.json       the version the player chose to skip
//
// Install (Apply): every step renames, so a file is never half written:
//   replace: <path> -> <path>.sbo-old, staging/<path> -> <path>   (Windows renames a loaded DLL; Linux a loaded .so)
//   add:     staging/<path> -> <path>
//   remove:  <path> -> <path>.sbo-old
//   new:     staging/<path> -> <path>.new  (a file the player changed stays as it is)
// A file Windows will not let go of (opened without delete sharing) is left for the next start ("deferred"); any
// other failure undoes the steps done so far (rollback). A crash or power loss half-way: the next start (Recover)
// finishes the install if every staged file is there and intact, else undoes it. The backups go once the new
// version has started (CleanupOld at the main menu).
//=====================================================//

#ifndef SBO_UPDATER_INSTALL_H
#define SBO_UPDATER_INSTALL_H

#include <stdint.h>
#include <string>
#include <vector>

#include "updater_core.h"

namespace sbo_update
{
struct InstallStep
{
	char kind = 'F';     //!< 'F' install the staged file (add or replace), 'N' install it as <path>.new, 'D' remove
	std::string path;    //!< relative to the game folder
	uint64_t size = 0;   //!< 'F' / 'N': the staged file's size and sha256
	std::string sha256;
};

enum class ApplyResult
{
	Done,     //!< everything installed
	Deferred, //!< installed except files in use; the next start finishes them
	Failed,   //!< nothing changed (rolled back); see err
};

class Installer
{
public:
	//! gameDir: the game folder's absolute path ("<Half-Life>/syndicate")
	explicit Installer(const std::string &gameDir);

	const std::string &GameDir() const { return m_GameDir; }

	//! Writes a checked file to staging/<rel> (through a .part file)
	bool WriteStaged(const std::string &rel, const void *data, size_t len, std::string &err);
	//! The staged file is there with this size and sha256
	bool StagedMatches(const std::string &rel, uint64_t size, const std::string &sha256);
	void ClearStaging();

	//! The game file now: exists, size, sha256
	DiskFile Disk(const std::string &rel);
	//! True when both game paths exist and name one file (a case-insensitive file system: "UI/a.res" is "ui/a.res")
	bool SameFile(const std::string &relA, const std::string &relB);

	//! updater/installed.json; false if there is none or it is unreadable
	bool ReadInstalled(Manifest &out);

	//! Installs the steps (all files staged and checked first). installedJson: the new installed.json.
	ApplyResult Apply(const std::vector<InstallStep> &steps, const std::string &version, const std::string &installedJson,
	    std::string &err);

	//! Game start: finishes or undoes an interrupted or deferred install. Returns what it did for the log ("" if
	//! there was nothing to do).
	std::string Recover();
	//! True while an install waits for the next start (files that were in use)
	bool HasPendingInstall();
	//! Main menu after an install: deletes the replaced files' backups, and the folders that leaves empty (a removed
	//! file's folder, e.g. the old name of a folder renamed in case only). Returns a log line ("" if nothing).
	std::string CleanupOld();

	//! state.json: the skipped version ("" none)
	std::string ReadSkippedVersion();
	void WriteSkippedVersion(const std::string &version);

#ifdef SBO_UPDATER_TESTING
	// tests only (tests/updater): step index that fails, step count after which Apply stops as if the game had
	// crashed (journal left, nothing undone), a path that reports "in use"
	static int s_iTestFailStep;
	static int s_iTestCrashAfter;
	static std::string s_TestLockedPath;
#endif

private:
	struct Journal;
	enum class StepResult
	{
		Ok,
		Locked,
		Error
	};
	StepResult DoStep(const InstallStep &s, std::string &err);
	void UndoStep(const InstallStep &s);
	bool Commit(std::string &err);
	bool ReadJournal(Journal &j);
	bool AppendJournal(const std::string &line);

	std::string m_GameDir;
};

//! sha256 and size of a file; false if it cannot be read
bool HashFileUtf8(const std::string &path, uint64_t &size, std::string &sha256);
//! The game folder of the library holding addressInModule (client.dll / client.so is <game>/cl_dlls/client.*):
//! absolute, UTF-8; "" if it cannot be told
std::string GameDirOfModule(const void *addressInModule);
} // namespace sbo_update

#endif // SBO_UPDATER_INSTALL_H
