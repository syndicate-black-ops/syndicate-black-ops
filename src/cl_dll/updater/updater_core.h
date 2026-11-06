//========= SBO: updater, release logic =========//
//
// Purpose: what the updater decides, without any I/O (docs/UPDATER.md), in plain C++ (no SDK or SBO headers) so
//          tests/updater/run_test.sh builds it alone with ASan/UBSan:
//            * versions: semver "2.0.1", "v2.0.1-rc1", "2.0.1+1a2b3c4d" (precedence of semver 2.0.0: a pre-release
//              comes before its release; build metadata is ignored);
//            * the release list (GitHub GET /repos/<repo>/releases): the newest release, the notes of every release
//              newer than the installed one, the emergency marker "[no auto-update]" (BugfixedHL-Rebased's marker,
//              in the newest release's title or notes: notice only, no install);
//            * manifest.json / installed.json: every game file with its size and sha256 (and, in a release, where its
//              data is in the release zip), files to remove; paths checked (relative, inside the game folder);
//            * the install plan: what to add, replace, remove or leave (files the player changed);
//            * the release notes' markdown as styled text runs for the dialog.
//===============================================//

#ifndef SBO_UPDATER_CORE_H
#define SBO_UPDATER_CORE_H

#include <stdint.h>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace sbo_update
{
//-----------------------------------------------------------------------------
// Versions
//-----------------------------------------------------------------------------
struct Version
{
	int major = 0, minor = 0, patch = 0;
	std::string pre; //!< "rc1", "test.2"; empty for a release
	bool valid = false;
};
//! "2.0.1", "v2.0.1", "2.0.1-rc.1", "2.0.1+sha" (a leading v/V is dropped, "+..." ignored)
bool ParseVersion(const char *text, Version &out);
//! Semver precedence, <0, 0, >0 like strcmp; an invalid version is older than any valid one
int CompareVersions(const Version &a, const Version &b);
int CompareVersions(const char *a, const char *b);
std::string VersionText(const Version &v);

//-----------------------------------------------------------------------------
// Releases
//-----------------------------------------------------------------------------
constexpr const char NO_AUTO_UPDATE_MARKER[] = "[no auto-update]";

struct Release
{
	std::string tag, name, body, pageUrl;
	std::string published;     //!< published_at, "2026-11-02T22:25:43Z"
	bool bodyCut = false;      //!< the notes were longer than MAX_NOTES_BYTES
	Version version;
	bool prerelease = false;
	bool noAutoUpdate = false; //!< the marker is in its title or notes
	std::vector<std::pair<std::string, std::string>> assets; //!< name, browser_download_url
	//! The asset's download URL, "" if the release has none of that name
	std::string AssetUrl(const std::string &name) const;
};

struct CheckResult
{
	bool ok = false;
	std::string error;
	bool updateAvailable = false;
	Release latest;             //!< the newest release (valid when ok and the repo has one)
	bool hasLatest = false;
	std::vector<Release> newer; //!< releases newer than the installed version, newest first
};

//! "2026-11-02T22:25:43Z" -> "November 2, 2026" (American style); "" if it is not such a date
std::string ReleaseDateText(const std::string &iso);
//! The releases page of a release's page URL (".../releases/tag/v2.0.1" -> ".../releases"); "" if it is not one
std::string ReleasesPageUrl(const std::string &releasePageUrl);

//! The release list's JSON. Drafts and releases with a tag that is not a version are skipped, pre-releases too
//! unless includePrereleases.
CheckResult EvaluateReleases(const std::string &json, const char *installedVersion, bool includePrereleases);

//-----------------------------------------------------------------------------
// Paths and manifests
//-----------------------------------------------------------------------------
//! A manifest path: relative to the game folder, '/' separated, no "." / ".." / empty parts, no drive, no '\\',
//! printable ASCII, not under updater/ (the updater's own folder), no Windows device names, under 200 characters
bool IsSafeRelPath(const std::string &path);
std::string LowerAscii(std::string s);

struct FileEntry
{
	std::string path;
	uint64_t size = 0;
	std::string sha256;
	uint64_t zipOffset = 0; //!< first byte of the file's (compressed) data in the release zip
	uint64_t zipSize = 0;   //!< its length
	int method = 0;         //!< 0 stored, 8 deflate
	bool keepUserChanges = false;
	//! keepUserChanges: sha256 of the versions earlier releases shipped ("shipped_sha256"): a file on disk with one of
	//! them is not the player's edit (installs without installed.json, e.g. from a zip before the updater)
	std::vector<std::string> shippedSha256;
};

struct RemoveEntry
{
	std::string path;
	std::string sha256; //!< empty: remove whatever the file holds
};

struct Manifest
{
	int format = 1;
	std::string version, commit, zip;
	uint64_t zipSize = 0;
	std::vector<FileEntry> files;
	std::vector<RemoveEntry> remove;
	const FileEntry *Find(const std::string &path) const; //!< case-insensitive
};

constexpr uint64_t MAX_FILE_SIZE = 256ull * 1024 * 1024;

//! manifest.json (withZip: every file must have its zip range, inside zipSize) or installed.json (no zip data)
bool ParseManifest(const std::string &json, bool withZip, Manifest &out, std::string &err);
//! installed.json for m (files without their zip data, no removals)
std::string InstalledJson(const Manifest &m);

//-----------------------------------------------------------------------------
// Install plan
//-----------------------------------------------------------------------------
struct DiskFile
{
	bool exists = false;
	uint64_t size = 0;
	std::string sha256;
};

enum class Action
{
	Add,      //!< not on disk: download
	Replace,  //!< differs: download, replace
	KeepUser, //!< the player changed it (keepUserChanges): download as <path>.new, keep theirs
	Remove,   //!< not in the new release (or listed to remove) and still as shipped
	KeptRemoved, //!< not in the new release, but the player changed it: left alone (logged)
};

struct PlanItem
{
	Action action;
	std::string path;
	const FileEntry *file = nullptr; //!< into the new manifest (Add, Replace, KeepUser)
};

//! installed: the install's installed.json (nullptr if it has none). disk: the file on disk now. sameFile(a, b):
//! true when the two game paths name one file (a case-insensitive file system), false when they are two files or
//! either does not exist. A path whose name changes only in case (installed.json or the remove list against the new
//! manifest) is removed only when sameFile says it is a file of its own (Linux); on Windows the old name IS the new
//! file. Without sameFile such a path is never removed.
std::vector<PlanItem> MakePlan(const Manifest *installed, const Manifest &next,
    const std::function<DiskFile(const std::string &)> &disk,
    const std::function<bool(const std::string &, const std::string &)> &sameFile = nullptr);

//-----------------------------------------------------------------------------
// Release notes (GitHub markdown) as text runs
//-----------------------------------------------------------------------------
enum class RunStyle
{
	Normal,
	Heading,
	Bold,
	Code,
	Link,
	Dim
};

struct TextRun
{
	RunStyle style = RunStyle::Normal;
	std::string text; //!< UTF-8; a line ends with "\n" in its last run
	std::string url;  //!< Link: https:// URL
	int indent = 0;   //!< list level of the line the run starts (0 none); set on the line's first run
};

constexpr size_t MAX_NOTES_BYTES = 64 * 1024;

//! Headings, list items, bold, inline code, links, images (their alt text), quotes, code blocks, tables (as text),
//! HTML tags dropped. At most maxBytes of input are read.
std::vector<TextRun> MarkdownToRuns(const std::string &markdown, size_t maxBytes = MAX_NOTES_BYTES);
//! The runs as plain text (console)
std::string RunsToPlain(const std::vector<TextRun> &runs);
} // namespace sbo_update

#endif // SBO_UPDATER_CORE_H
