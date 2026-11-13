#!/usr/bin/env bash
# The console's kill lines name weapons the way players know them ("killed Scientist with Fists", not "with wong"):
# every weapon code the server can send in DeathMsg / SboDeathMsg for an SBO weapon has a #SBO_Weapon_<code> token in
# sbo_ui_english.txt, and both console print paths (death.cpp, sbo_bridge.cpp) go through VGui2_WeaponDisplayName.
# Needs python3.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
exec python3 -I "$ROOT/tests/weapon_names/check_weapon_names.py" "$ROOT"
