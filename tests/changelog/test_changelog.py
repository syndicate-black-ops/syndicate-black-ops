"""Tests for CHANGELOG.md and the changelog functions of scripts/updater_release.py (run by run_test.sh)."""
import importlib.util
import os
import subprocess
import sys
import tempfile

ROOT = sys.argv[1]
SCRIPT = os.path.join(ROOT, "scripts", "updater_release.py")
spec = importlib.util.spec_from_file_location("updater_release", SCRIPT)
ur = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ur)

failed = 0


def check(name, cond):
    global failed
    print(("ok   " if cond else "FAIL ") + name)
    failed += 0 if cond else 1


# the real file
with open(os.path.join(ROOT, "CHANGELOG.md"), encoding="utf-8") as f:
    real = f.read()
errs = ur.check_changelog_text(real, ur.project_version())
for e in errs:
    print("     " + e)
check("CHANGELOG.md parses, newest = project(VERSION) %s" % ur.project_version(), not errs)
check("CHANGELOG.md has no tabs or CR", "\t" not in real and "\r" not in real)

GOOD = "# Changelog\n\nIntro.\n\n## [2.1.0]\n\n### New\n\n- a\n\n### Fixed\n\n- b\n\n## [2.0.0] - 2026-11-06\n\n### Changed\n\n- c\n"
check("good sample", ur.check_changelog_text(GOOD, "2.1.0") == [])
check("wrong packaged version", any("build packages" in e for e in ur.check_changelog_text(GOOD, "2.2.0")))
bad = {
    "bad heading (no brackets)": GOOD.replace("## [2.1.0]", "## 2.1.0"),
    "bad version": GOOD.replace("## [2.1.0]", "## [2.1]"),
    "bad date": GOOD.replace("- 2026-11-06", "- Nov 6"),
    "empty group": GOOD.replace("### Fixed\n\n- b\n", "### Fixed\n\n"),
    "unknown group": GOOD.replace("### Fixed", "### Bugfixes"),
    "empty section": GOOD.replace("### Changed\n\n- c\n", ""),
    "older version first": GOOD.replace("[2.0.0]", "[2.2.0]"),
    "duplicate version": GOOD.replace("[2.0.0]", "[2.1.0]"),
    "entry before a group": GOOD.replace("## [2.1.0]\n\n", "## [2.1.0]\n\n- stray\n\n"),
    "prose line in a section": GOOD.replace("- b\n", "- b\nloose text\n"),
}
for name, text in bad.items():
    check("rejects: " + name, ur.check_changelog_text(text, "2.1.0") != [])


def notes(*args, path=None):
    env = dict(os.environ)
    cmd = [sys.executable, "-I", "-c",
           "import importlib.util,sys;s=importlib.util.spec_from_file_location('u',sys.argv[1]);m=importlib.util."
           "module_from_spec(s);s.loader.exec_module(m);m.CHANGELOG=sys.argv[2] or m.CHANGELOG;"
           "sys.argv=['updater_release.py']+sys.argv[3:];m.main()", SCRIPT, path or ""] + list(args)
    return subprocess.run(cmd, capture_output=True, text=True, env=env)


r = notes("notes", "v2.1.0")
link = "[Full changelog](https://github.com/syndicate-black-ops/syndicate-black-ops/blob/main/CHANGELOG.md)"
check("notes v2.1.0: ok", r.returncode == 0)
check("notes: no version heading, starts with a group", r.stdout.startswith("### ") and "## [" not in r.stdout)
check("notes: ends with the full changelog link", r.stdout.rstrip("\n").endswith(link))
check("notes: stops before the next version", "Released" not in r.stdout and r.stdout.count("### New") == 1)
check("notes 2.1.0 = notes v2.1.0", notes("notes", "2.1.0").stdout == r.stdout)
check("notes --no-link", link not in notes("notes", "2.1.0", "--no-link").stdout)
check("notes 2.1.0-rc1 uses 2.1.0", notes("notes", "2.1.0-rc1").stdout == r.stdout)

with tempfile.TemporaryDirectory() as t:
    p = os.path.join(t, "CHANGELOG.md")
    with open(p, "w") as f:
        f.write("# Changelog\n\n## [3.0.0]\n\n### New\n\n## [2.0.0]\n\n### New\n\n- x\n")
    r = notes("notes", "3.0.0", path=p)
    check("notes: empty section fails clearly", r.returncode != 0 and "empty" in r.stderr)
    r = notes("notes", "4.0.0", path=p)
    check("notes: missing section fails clearly", r.returncode != 0 and "no section" in r.stderr)
    r = notes("notes", "2.0.0", "--repo", "o/r", path=p)
    check("notes: --repo in the link", r.returncode == 0 and r.stdout == "### New\n\n- x\n\n[Full changelog](https://github.com/o/r/blob/main/CHANGELOG.md)\n")
    # publish: --notes is optional, a missing section stops it before gh runs; a stray --notes still parses
    rel = os.path.join(t, "rel")
    os.mkdir(rel)
    for n, body in (("manifest.json", '{"version": "4.0.0", "zip": "sbo-v4.0.0.zip"}'), ("manifest.json.sig", "x"),
                    ("sbo-v4.0.0.zip", "z")):
        with open(os.path.join(rel, n), "w") as f:
            f.write(body)
    r = notes("publish", "v4.0.0", rel, "--repo", "o/r", path=p)
    check("publish without --notes: missing section stops before gh", r.returncode != 0 and "no section" in r.stderr
          and "gh release create" not in r.stdout)
    r = notes("publish", "v4.0.0", rel, "--repo", "o/r", "--notes", p, "--help", path=p)
    check("publish accepts --notes", r.returncode == 0)

print("RESULT: %s" % ("FAIL (%d)" % failed if failed else "PASS"))
sys.exit(1 if failed else 0)
