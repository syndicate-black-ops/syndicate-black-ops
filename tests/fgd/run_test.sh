#!/usr/bin/env bash
# scripts/fgd.py: the spawnflag scan (operands, constants, reachability through the class chain, merged base
# flags, missing_flag with its exemptions) on small inputs, then the real check of sdk/syndicate.fgd against
# src/dlls. Needs python3.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
python3 -I "$ROOT/tests/fgd/test_fgd.py" "$ROOT"
python3 -I "$ROOT/scripts/fgd.py" check
