//========= SBO: updater =========//
//
// Purpose: "Check for updates" and the in-game updater (docs/UPDATER.md): the release list from GitHub, the update
//          dialog's data, download + check + stage of the changed files, install and the next-start finish. Plain
//          C++ (no SDK or SBO headers): the network goes through IHttp (Steam's HTTP in the game,
//          vgui2/sbo_update_http.cpp; a fake in tests/updater), the dialog is vgui2/sbo_update_window.cpp, the engine
//          glue (cvar, commands, frame) is updater_hooks.cpp.
//
// Never blocks the game: Frame() only polls; requests time out (Steam's network activity timeout plus a hard
// deadline here); hashing, inflating and writing run on a worker thread. A failed check is one console line.
//================================//

#ifndef SBO_UPDATER_CLIENT_H
#define SBO_UPDATER_CLIENT_H

#include <stdint.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "updater_core.h"

namespace sbo_update
{
struct HttpRequest
{
	std::string url;
	std::vector<std::pair<std::string, std::string>> headers;
	bool range = false;
	uint64_t rangeFirst = 0, rangeLast = 0; //!< inclusive byte range when range
	size_t maxBytes = 0;                    //!< larger bodies fail
	int timeoutSeconds = 15;                //!< hard deadline
};

struct HttpResult
{
	bool ok = false; //!< a response arrived (any status)
	int status = 0;
	std::string body;
	std::string location; //!< Location header of a redirect
	std::string error;    //!< when !ok
};

//! One request at a time
class IHttp
{
public:
	virtual ~IHttp() {}
	virtual bool Available() = 0;
	//! False if it cannot start (no Steam, ...): error in err
	virtual bool Start(const HttpRequest &req, std::string &err) = 0;
	//! True once finished; out filled. Never blocks.
	virtual bool Poll(HttpResult &out) = 0;
	virtual void Cancel() = 0;
};

struct Config
{
	std::string gameDir;       //!< absolute path of the game folder
	std::string repo;          //!< "owner/name"; empty: updater off
	std::string apiBase;       //!< "https://api.github.com/repos/<repo>" (launch option -update_api_url replaces it)
	std::string publicKeyHex;  //!< empty: check only, no install
	bool includePrereleases = false;
	std::string installedVersion;
};

enum class State
{
	Disabled,        //!< no repo in this build
	Idle,            //!< not checked yet
	Checking,
	UpToDate,
	UpdateAvailable,
	Failed,          //!< check failed (GetError)
	Preparing,       //!< manifest, signature, comparing files
	Downloading,
	ReadyToInstall,  //!< everything staged and checked
	Installing,
	Installed,       //!< the game quits; the player starts it again
	InstallFailed,   //!< nothing changed (GetError)
};

struct Progress
{
	int filesDone = 0, filesTotal = 0;
	uint64_t bytesDone = 0, bytesTotal = 0;
	std::string current;
	//! Preparing: files of the manifest compared with the disk so far (worker), of checkTotal; checkTotal 0 while
	//! the manifest and its signature are still on their way (no count known yet)
	int checkDone = 0, checkTotal = 0;
};

class Updater
{
public:
	Updater(const Config &cfg, IHttp *http, std::function<void(const std::string &)> log);

	//! Game start (before the UI loads): finishes or undoes an interrupted install
	void Startup();
	//! Main thread, every frame
	void Frame();

	//! manual: the player asked (Version tab, update_check): the dialog opens at once and shows the result, even for a
	//! skipped version; the start's check (false) opens it only for an update
	void Check(bool manual);
	void StartUpdate();
	void CancelUpdate();
	//! ReadyToInstall: installs (on the worker); Installed when done, then the game should quit
	void Install();
	void SkipVersion();

	State GetState() const { return m_State; }
	const std::string &GetError() const { return m_Error; }
	const CheckResult &GetResult() const { return m_Result; }
	//! Main thread; while Preparing, the worker's count of checked files is read here (atomic)
	Progress GetProgress() const;
	bool CanInstall() const; //!< not disabled by the marker, and this build can check signatures
	bool CheckWasManual() const { return m_bManual; }
	//! The dialog should open (an update found, not skipped or the player asked); cleared when read
	bool TakeShowRequest();
	//! Files the player changed that were kept (their new version is beside them as .new)
	const std::vector<std::string> &KeptFiles() const { return m_Kept; }
	//! Release notes of every newer release, newest first, as text runs (heading "name" before each)
	std::vector<TextRun> ChangelogRuns() const;
	const Config &GetConfig() const { return m_Cfg; }
	//! Main menu after a start that installed: backups of the old version go
	void CleanupAfterStart();

	//! Where the notes that show only with developer go (the start's "is up to date"); unset: the normal log
	void SetDevLog(std::function<void(const std::string &)> devLog) { m_DevLog = std::move(devLog); }

private:
	struct Job;
	void Log(const std::string &line);
	void DevLog(const std::string &line);
	bool Request(const HttpRequest &req);
	void OnHttp(HttpResult &res);
	void OnReleases(HttpResult &res);
	void OnManifest(HttpResult &res);
	void OnSignature(HttpResult &res);
	void OnFileData(HttpResult &res);
	void NextFile();
	void Fail(State st, const std::string &error);
	HttpRequest Get(const std::string &url, size_t maxBytes, int timeout);

	Config m_Cfg;
	IHttp *m_pHttp;
	std::function<void(const std::string &)> m_Log;
	std::function<void(const std::string &)> m_DevLog;
	State m_State;
	std::string m_Error;
	CheckResult m_Result;
	bool m_bManual = false, m_bShowRequest = false;

	enum class Pending
	{
		None,
		Releases,
		Manifest,
		Signature,
		FileData
	} m_Pending = Pending::None;
	HttpRequest m_LastRequest;
	int m_iRedirects = 0;
	int m_iRetries = 0; //!< asset host link expired: fetched again from the release link
	std::string m_ZipUrl;      //!< browser_download_url of the zip
	std::string m_ZipResolved; //!< the asset host URL it redirected to (reused for the next ranges)
	std::string m_ManifestText;
	Manifest m_Manifest;
	std::unique_ptr<Manifest> m_pInstalled;
	struct Download
	{
		FileEntry file;
		bool beside = false; //!< KeepUser: installed as <path>.new
	};
	std::vector<Download> m_Downloads;
	std::vector<std::string> m_Removals;
	std::vector<std::string> m_Kept;
	size_t m_iNextFile = 0;
	std::shared_ptr<Job> m_pJob;
	Progress m_Progress;
};
} // namespace sbo_update

#endif // SBO_UPDATER_CLIENT_H
