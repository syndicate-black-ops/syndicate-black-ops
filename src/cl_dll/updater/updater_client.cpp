//========= SBO: updater =========//
//
// Purpose: see updater_client.h.
//
// Order of an update: release list -> (dialog, Update) -> manifest.json -> manifest.json.sig (Ed25519 over the
// manifest's bytes) -> compare with the files on disk (worker) -> for each file to add or replace: one HTTP Range
// request on the release zip for exactly its compressed data -> inflate, size + sha256 check, write to staging
// (worker) -> Install: Installer::Apply (worker) -> quit.
//================================//

#include "updater_client.h"
#include "updater_crypto.h"
#include "updater_install.h"
#include "updater_sha256.h"

#include <stdio.h>
#include <algorithm>
#include <atomic>
#include <thread>

namespace sbo_update
{
// A worker job: run() on its own thread; the main thread reads the outputs once done is set
struct Updater::Job
{
	std::atomic<bool> done{false};
	std::atomic<int> checked{0}; //!< plan job: files compared with the disk so far (Progress::checkDone)
	bool ok = false;
	std::string error;
	// plan job
	std::vector<Download> downloads;
	std::vector<std::string> removals, kept, keptRemoved;
	bool hadInstalled = false;
	// install job
	ApplyResult apply = ApplyResult::Failed;
};

Updater::Updater(const Config &cfg, IHttp *http, std::function<void(const std::string &)> log)
    : m_Cfg(cfg)
    , m_pHttp(http)
    , m_Log(std::move(log))
    , m_State(cfg.repo.empty() ? State::Disabled : State::Idle)
{
	if (m_Cfg.apiBase.empty() && !m_Cfg.repo.empty())
		m_Cfg.apiBase = "https://api.github.com/repos/" + m_Cfg.repo;
}

void Updater::Log(const std::string &line)
{
	if (m_Log)
		m_Log(line);
}

void Updater::Startup()
{
	if (m_Cfg.gameDir.empty())
		return;
	Installer inst(m_Cfg.gameDir);
	const std::string msg = inst.Recover();
	if (!msg.empty())
		Log(msg);
}

void Updater::CleanupAfterStart()
{
	if (m_Cfg.gameDir.empty())
		return;
	Installer inst(m_Cfg.gameDir);
	const std::string msg = inst.CleanupOld();
	if (!msg.empty())
		Log(msg);
}

bool Updater::CanInstall() const
{
	uint8_t key[32];
	return m_Result.hasLatest && !m_Result.latest.noAutoUpdate && ParsePublicKey(m_Cfg.publicKeyHex.c_str(), key) &&
	       !m_Cfg.gameDir.empty();
}

bool Updater::TakeShowRequest()
{
	const bool r = m_bShowRequest;
	m_bShowRequest = false;
	return r;
}

Progress Updater::GetProgress() const
{
	Progress p = m_Progress;
	if (m_State == State::Preparing && m_pJob && p.checkTotal > 0)
		p.checkDone = std::min(p.checkTotal, m_pJob->checked.load(std::memory_order_relaxed));
	return p;
}

HttpRequest Updater::Get(const std::string &url, size_t maxBytes, int timeout)
{
	HttpRequest r;
	r.url = url;
	r.maxBytes = maxBytes;
	r.timeoutSeconds = timeout;
	return r;
}

bool Updater::Request(const HttpRequest &req)
{
	// m_Pending is set by the caller; a request that cannot start is a failed response
	std::string err;
	m_LastRequest = req;
	if (!m_pHttp || !m_pHttp->Available())
		err = "no HTTP (Steam is not running)";
	else if (m_pHttp->Start(req, err))
		return true;
	HttpResult res;
	res.error = err;
	OnHttp(res);
	return false;
}

void Updater::Fail(State st, const std::string &error)
{
	m_Pending = Pending::None;
	m_State = st;
	m_Error = error;
	if (st == State::Failed)
		Log("Update check failed: " + error);
	else
		Log("Update failed: " + error + " (nothing was changed)");
	if (st == State::InstallFailed)
	{
		// staged files stay for nothing: a new attempt downloads again
		Installer inst(m_Cfg.gameDir);
		inst.ClearStaging();
	}
}

void Updater::Check(bool manual)
{
	if (m_State == State::Disabled)
		return;
	if (m_State == State::Checking || m_State == State::Preparing || m_State == State::Downloading ||
	    m_State == State::Installing || m_State == State::ReadyToInstall || m_State == State::Installed)
	{
		if (manual)
		{
			m_bShowRequest = true; // the dialog shows what is going on
			if (m_State == State::Checking)
				m_bManual = true; // the start's check, now asked for: its result shows even for a skipped version
		}
		return;
	}
	m_bManual = manual;
	m_bShowRequest = manual; // asked for: the dialog opens at once with "Checking for updates..."; the start's check
	                         // opens it only for an update
	m_State = State::Checking;
	m_Error.clear();
	m_Pending = Pending::Releases;
	HttpRequest r = Get(m_Cfg.apiBase + "/releases?per_page=30", 4 * 1024 * 1024, 15);
	r.headers.emplace_back("Accept", "application/vnd.github+json");
	r.headers.emplace_back("X-GitHub-Api-Version", "2022-11-28");
	Request(r);
}

void Updater::Frame()
{
	if (m_Pending != Pending::None && m_pHttp)
	{
		HttpResult res;
		if (m_pHttp->Poll(res))
			OnHttp(res);
	}
	if (m_pJob && m_pJob->done.load(std::memory_order_acquire))
	{
		std::shared_ptr<Job> job = std::move(m_pJob);
		if (m_State == State::Preparing)
		{
			if (!job->ok)
			{
				Fail(State::InstallFailed, job->error);
				return;
			}
			m_Downloads = std::move(job->downloads);
			m_Removals = std::move(job->removals);
			m_Kept = std::move(job->kept);
			for (const std::string &k : job->keptRemoved)
				Log("Updater: " + k + " is no longer part of the game but was changed; it stays");
			m_Progress = Progress();
			m_Progress.filesTotal = (int)m_Downloads.size();
			for (const Download &d : m_Downloads)
				m_Progress.bytesTotal += d.file.zipSize;
			Log("Updater: " + std::to_string(m_Downloads.size()) + " file(s) to download, " +
			    std::to_string(m_Removals.size()) + " to remove" +
			    (job->hadInstalled ? "" : " (no installed.json: compared with every file)"));
			m_State = State::Downloading;
			m_iNextFile = 0;
			NextFile();
		}
		else if (m_State == State::Downloading)
		{
			if (!job->ok)
			{
				Fail(State::InstallFailed, job->error);
				return;
			}
			m_Progress.filesDone++;
			m_Progress.bytesDone += m_Downloads[m_iNextFile].file.zipSize;
			m_iNextFile++;
			NextFile();
		}
		else if (m_State == State::Installing)
		{
			if (job->apply == ApplyResult::Failed)
			{
				Fail(State::InstallFailed, job->error);
				return;
			}
			m_State = State::Installed;
			Log("Updater: " + m_Manifest.version + " installed" +
			    (job->apply == ApplyResult::Deferred ? " (some files were in use: they are installed at the next start)"
			                                         : "") +
			    "; the game quits to install it. Start it again afterwards.");
			for (const std::string &k : m_Kept)
				Log("Updater: kept your " + k + "; the new one is " + k + ".new");
		}
	}
}

void Updater::OnHttp(HttpResult &res)
{
	const Pending p = m_Pending;
	m_Pending = Pending::None;
	// redirects (GitHub's asset links go to its asset host); followed here when Steam's HTTP does not
	if (res.ok && (res.status == 301 || res.status == 302 || res.status == 303 || res.status == 307 || res.status == 308))
	{
		if (res.location.compare(0, 8, "https://") != 0 || ++m_iRedirects > 5)
		{
			res.ok = false;
			res.error = "bad redirect";
		}
		else
		{
			HttpRequest r = m_LastRequest;
			r.url = res.location;
			if (p == Pending::FileData)
				m_ZipResolved = res.location;
			m_Pending = p;
			Request(r);
			return;
		}
	}
	m_iRedirects = 0;
	switch (p)
	{
	case Pending::Releases: OnReleases(res); break;
	case Pending::Manifest: OnManifest(res); break;
	case Pending::Signature: OnSignature(res); break;
	case Pending::FileData: OnFileData(res); break;
	default: break;
	}
}

static std::string HttpError(const HttpResult &res)
{
	if (!res.ok)
		return res.error.empty() ? "no response" : res.error;
	if (res.status == 403 || res.status == 429)
		return "HTTP " + std::to_string(res.status) + " (GitHub's request limit; try again later)";
	return "HTTP " + std::to_string(res.status);
}

void Updater::OnReleases(HttpResult &res)
{
	if (!res.ok || res.status != 200)
	{
		Fail(State::Failed, HttpError(res));
		return;
	}
	m_Result = EvaluateReleases(res.body, m_Cfg.installedVersion.c_str(), m_Cfg.includePrereleases);
	if (!m_Result.ok)
	{
		Fail(State::Failed, m_Result.error);
		return;
	}
	if (!m_Result.updateAvailable)
	{
		m_State = State::UpToDate;
		Log("Update check: " + m_Cfg.installedVersion + " is up to date" +
		    (m_Result.hasLatest ? " (newest release " + VersionText(m_Result.latest.version) + ")" : ""));
		return;
	}
	m_State = State::UpdateAvailable;
	const std::string newest = VersionText(m_Result.latest.version);
	Log("Update available: " + m_Cfg.installedVersion + " -> " + newest + " (" + m_Result.latest.pageUrl + ")" +
	    (m_Result.latest.noAutoUpdate ? " [no auto-update: download it from the release page]" : "") +
	    "; type update_changelog for what is new");
	std::string skipped;
	if (!m_Cfg.gameDir.empty())
	{
		Installer inst(m_Cfg.gameDir);
		skipped = inst.ReadSkippedVersion();
	}
	if (m_bManual || skipped.empty() || CompareVersions(skipped.c_str(), newest.c_str()) != 0)
		m_bShowRequest = true;
	else
		Log("Update check: " + newest + " was skipped (Check for updates shows it again)");
}

void Updater::SkipVersion()
{
	if (!m_Result.hasLatest || m_Cfg.gameDir.empty())
		return;
	Installer inst(m_Cfg.gameDir);
	inst.WriteSkippedVersion(VersionText(m_Result.latest.version));
	Log("Updater: " + VersionText(m_Result.latest.version) + " skipped");
}

void Updater::StartUpdate()
{
	if (m_State != State::UpdateAvailable && m_State != State::InstallFailed)
		return;
	if (!CanInstall())
	{
		Fail(State::InstallFailed, m_Result.latest.noAutoUpdate ? "this release must be installed by hand"
		                                                        : "this build cannot install updates");
		return;
	}
	const Release &rel = m_Result.latest;
	const std::string url = rel.AssetUrl("manifest.json");
	if (url.empty() || rel.AssetUrl("manifest.json.sig").empty())
	{
		Fail(State::InstallFailed, "the release has no manifest.json / manifest.json.sig");
		return;
	}
	m_State = State::Preparing;
	m_Error.clear();
	m_Kept.clear();
	m_Downloads.clear();
	m_Removals.clear();
	m_Progress = Progress();
	m_ZipResolved.clear();
	m_Pending = Pending::Manifest;
	Log("Updater: downloading " + VersionText(rel.version));
	Request(Get(url, 16 * 1024 * 1024, 30));
}

void Updater::OnManifest(HttpResult &res)
{
	if (!res.ok || res.status != 200)
	{
		Fail(State::InstallFailed, "manifest.json: " + HttpError(res));
		return;
	}
	m_ManifestText = std::move(res.body);
	m_Pending = Pending::Signature;
	Request(Get(m_Result.latest.AssetUrl("manifest.json.sig"), 1024, 30));
}

void Updater::OnSignature(HttpResult &res)
{
	if (!res.ok || res.status != 200)
	{
		Fail(State::InstallFailed, "manifest.json.sig: " + HttpError(res));
		return;
	}
	uint8_t key[32];
	if (!ParsePublicKey(m_Cfg.publicKeyHex.c_str(), key) ||
	    !VerifySignature(key, m_ManifestText.data(), m_ManifestText.size(), res.body.data(), res.body.size()))
	{
		Fail(State::InstallFailed, "the release's signature does not match (not installed)");
		return;
	}
	std::string err;
	Manifest m;
	if (!ParseManifest(m_ManifestText, true, m, err))
	{
		Fail(State::InstallFailed, "manifest.json: " + err);
		return;
	}
	if (CompareVersions(m.version.c_str(), m_Result.latest.tag.c_str()) != 0)
	{
		Fail(State::InstallFailed, "manifest.json is for " + m.version + ", not " + m_Result.latest.tag);
		return;
	}
	m_ZipUrl = m_Result.latest.AssetUrl(m.zip);
	if (m_ZipUrl.empty())
	{
		Fail(State::InstallFailed, "the release has no " + m.zip);
		return;
	}
	m_Manifest = std::move(m);
	Log("Updater: manifest of " + m_Manifest.version + " signed and checked (" + std::to_string(m_Manifest.files.size()) +
	    " files)");

	// compare with the disk on the worker (hashes every game file)
	// (the window shows "Checking files N / total": every file looked at counts, the old version's and the remove
	// list's after the manifest's, so the count is clamped to the manifest's)
	auto job = std::make_shared<Job>();
	m_pJob = job;
	m_Progress.checkTotal = (int)m_Manifest.files.size();
	const std::string game = m_Cfg.gameDir;
	const Manifest next = m_Manifest;
	std::thread([job, game, next]() {
		Installer inst(game);
		Manifest installed;
		const bool have = inst.ReadInstalled(installed);
		job->hadInstalled = have;
		const std::vector<PlanItem> plan = MakePlan(have ? &installed : nullptr, next,
		    [&inst, &job](const std::string &rel) {
			    const DiskFile d = inst.Disk(rel);
			    job->checked.fetch_add(1, std::memory_order_relaxed);
			    return d;
		    },
		    [&inst](const std::string &a, const std::string &b) { return inst.SameFile(a, b); });
		for (const PlanItem &p : plan)
		{
			switch (p.action)
			{
			case Action::Add:
			case Action::Replace:
				job->downloads.push_back({*p.file, false});
				break;
			case Action::KeepUser:
				job->downloads.push_back({*p.file, true});
				job->kept.push_back(p.path);
				break;
			case Action::Remove: job->removals.push_back(p.path); break;
			case Action::KeptRemoved: job->keptRemoved.push_back(p.path); break;
			}
		}
		inst.ClearStaging();
		job->ok = true;
		job->done.store(true, std::memory_order_release);
	}).detach();
}

void Updater::NextFile()
{
	if (m_iNextFile >= m_Downloads.size())
	{
		m_State = State::ReadyToInstall;
		m_Progress.current.clear();
		Log("Updater: " + m_Manifest.version + " downloaded and checked; ready to install");
		return;
	}
	const FileEntry &f = m_Downloads[m_iNextFile].file;
	m_Progress.current = f.path;
	if (f.zipSize == 0)
	{
		// an empty file: nothing to download
		HttpResult res;
		res.ok = true;
		res.status = 206;
		OnFileData(res);
		return;
	}
	HttpRequest r = Get(m_ZipResolved.empty() ? m_ZipUrl : m_ZipResolved, (size_t)f.zipSize + 64,
	    30 + (int)(f.zipSize / (64 * 1024))); // at least 64 KB/s
	r.range = true;
	r.rangeFirst = f.zipOffset;
	r.rangeLast = f.zipOffset + f.zipSize - 1;
	m_Pending = Pending::FileData;
	Request(r);
}

void Updater::OnFileData(HttpResult &res)
{
	const FileEntry &f = m_Downloads[m_iNextFile].file;
	if (res.ok && (res.status == 403 || res.status == 404 || res.status == 410) && !m_ZipResolved.empty() &&
	    m_iRetries++ < 2)
	{
		// the asset host's link expired: from the release link again
		m_ZipResolved.clear();
		NextFile();
		return;
	}
	if (!res.ok || (res.status != 206 && !(res.status == 200 && f.zipSize == 0)))
	{
		Fail(State::InstallFailed, f.path + ": " + (res.ok && res.status == 200 ? "the server sent the whole zip" : HttpError(res)));
		return;
	}
	if (res.body.size() != f.zipSize)
	{
		Fail(State::InstallFailed, f.path + ": got " + std::to_string(res.body.size()) + " bytes, expected " +
		                               std::to_string(f.zipSize));
		return;
	}
	m_iRetries = 0;
	auto job = std::make_shared<Job>();
	m_pJob = job;
	const std::string game = m_Cfg.gameDir;
	const FileEntry entry = f;
	std::thread([job, game, entry, data = std::move(res.body)]() {
		std::string plain;
		const std::string *content = &data;
		if (entry.method == 8)
		{
			if (!InflateRaw(data.data(), data.size(), (size_t)entry.size, plain))
			{
				job->error = entry.path + ": bad compressed data";
				job->done.store(true, std::memory_order_release);
				return;
			}
			content = &plain;
		}
		Sha256 h;
		h.Update(content->data(), content->size());
		if (content->size() != entry.size || h.FinalHex() != entry.sha256)
		{
			job->error = entry.path + ": damaged download (sha256 differs)";
			job->done.store(true, std::memory_order_release);
			return;
		}
		Installer inst(game);
		job->ok = inst.WriteStaged(entry.path, content->data(), content->size(), job->error);
		job->done.store(true, std::memory_order_release);
	}).detach();
}

void Updater::CancelUpdate()
{
	if (m_State != State::Preparing && m_State != State::Downloading && m_State != State::ReadyToInstall)
		return;
	if (m_Pending != Pending::None && m_pHttp)
		m_pHttp->Cancel();
	m_Pending = Pending::None;
	m_pJob.reset(); // a running job finishes on its own; its result is dropped
	Installer inst(m_Cfg.gameDir);
	inst.ClearStaging();
	m_State = State::UpdateAvailable;
	Log("Updater: cancelled; nothing was changed");
}

void Updater::Install()
{
	if (m_State != State::ReadyToInstall)
		return;
	std::vector<InstallStep> steps;
	for (const Download &d : m_Downloads)
		steps.push_back({d.beside ? 'N' : 'F', d.file.path, d.file.size, d.file.sha256});
	for (const std::string &r : m_Removals)
		steps.push_back({'D', r, 0, ""});
	m_State = State::Installing;
	auto job = std::make_shared<Job>();
	m_pJob = job;
	const std::string game = m_Cfg.gameDir, version = m_Manifest.version, installed = InstalledJson(m_Manifest);
	std::thread([job, game, version, installed, steps]() {
		Installer inst(game);
		job->apply = inst.Apply(steps, version, installed, job->error);
		job->ok = job->apply != ApplyResult::Failed;
		job->done.store(true, std::memory_order_release);
	}).detach();
}

std::vector<TextRun> Updater::ChangelogRuns() const
{
	std::vector<TextRun> out;
	size_t budget = MAX_NOTES_BYTES * 2;
	bool cut = false;
	for (size_t i = 0; i < m_Result.newer.size(); i++)
	{
		const Release &r = m_Result.newer[i];
		if (!budget)
		{
			cut = true; // releases left that did not fit
			break;
		}
		// "2.0.1 - November 2, 2026": the version from the tag and the date it was published
		TextRun h;
		h.style = RunStyle::Heading;
		const std::string date = ReleaseDateText(r.published);
		h.text = VersionText(r.version) + (date.empty() ? "" : " - " + date) + "\n";
		out.push_back(h);
		const size_t room = std::min(budget, MAX_NOTES_BYTES);
		cut = cut || r.bodyCut || r.body.size() > room;
		const std::vector<TextRun> body = MarkdownToRuns(r.body, room);
		for (const TextRun &t : body)
		{
			budget = t.text.size() < budget ? budget - t.text.size() : 0;
			out.push_back(t);
		}
		TextRun gap;
		gap.text = "\n\n";
		out.push_back(gap);
		if (cut)
			break;
	}
	const std::string page = ReleasesPageUrl(m_Result.latest.pageUrl);
	if (cut && !page.empty())
	{
		TextRun t;
		t.text = "Older changes: ";
		out.push_back(t);
		TextRun link;
		link.style = RunStyle::Link;
		link.text = page;
		link.url = page;
		out.push_back(link);
		TextRun nl;
		nl.text = "\n";
		out.push_back(nl);
	}
	return out;
}
} // namespace sbo_update
