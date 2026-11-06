#!/usr/bin/env bash
# Automated entity tests (docs/dev/ENTITY_TESTS.md): every suite in tests/entity/suites/ (or the ones named) on a
# headless stock HLDS with an hl.so built with the test layer (off by default):
#   scripts/build.sh linux RelWithDebInfo -DSBO_TEST_COMMANDS=ON
#
#   scripts/run_entity_tests.sh [suite ...] [--hl hl.so] [--hlds DIR] [--asan] [--ci] [--timescale X] [--list] [--coverage]
#     --ci       every suite twice; a test that passes once and fails once is FLAKY (a failure)
#     --asan     an ASan+UBSan hl.so (scripts/check_sanitizers.sh asan), run in the sniper image with libasan
# Env: HLDS_DIR (stock HLDS install, default ~/hlds), SDHLT_DIR (ReSDHLT tools), ET_HL, ET_TIMESCALE (default 5).
# Output: build/entity-tests/report.json, junit.xml; per suite map, steps, traces, server consoles. Exit 1 on failure.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
exec nice -n 10 python3 "$ROOT/tests/entity/run.py" "$@"
