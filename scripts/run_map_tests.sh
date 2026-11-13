#!/usr/bin/env bash
# Map regression tests (docs/dev/MAP_TESTS.md): the 1.38 server .so and the hl.so under test run the same map logic on
# the stub engine (tests/fuzz/server's case runner), side by side; every difference must be in tests/maps/baseline.toml.
# REQUIRED before merging any change to entity, trigger, monster or game-rule code.
#
#   scripts/run_map_tests.sh [all|replay|cases] [map or case ...] [--update-baseline] [--keep-logs] [--list]
#     replay   every map in tests/maps/maps.txt: all entities spawned, every targetname fired, triggers touched
#     cases    tests/maps/cases/*.txt: monsters, monstermakers, scripts, round rules
#     all      both (default)
# Env: SBO_138_SO (default dist/release-1.38/syndicate/dlls/hl_i386.so), SBO_DEVELOP_SO (default build/linux/hl.so),
#      SBO_MAP_DIRS, SBO_CONTENT_PATH, HLDS_DIR, MAP_TESTS_OUT, MAP_TESTS_DOCKER (tests/maps/map_tests.py --help).
# Output: build/map-tests/report.txt, logs per suite and binary. Exit 0 = only the baseline's differences; 1 = any other
# difference, a listed one that no longer shows, or a crash / hang; 2 = setup error.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
exec python3 "$ROOT/tests/maps/map_tests.py" "$@"
