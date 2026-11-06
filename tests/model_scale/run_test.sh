#!/usr/bin/env bash
# Offline test of the server's model scale bone setup (src/dlls/animation.cpp Server_GetBlendingInterface;
# docs/MODEL_SCALE.md "Collision box and hitboxes"): builds animation.cpp with tests/model_scale/hitbox_test.cpp as
# a stand-in engine and checks, on the game's models, that the bones at scale 1 are the engine's and that at other
# scales the hitbox planes, attachments and bone positions are where the client draws them. Needs g++.
#   tests/model_scale/run_test.sh [model.mdl...]     (default: a few of gamedir/syndicate/models)
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
FLAGS=(-g -fsanitize=address,undefined -fno-strict-aliasing -I"$ROOT/src/dlls" -I"$ROOT/src/engine" -I"$ROOT/src/common"
  -I"$ROOT/src/pm_shared" -Dstricmp=strcasecmp -w)
g++ -std=c++17 "${FLAGS[@]}" -c -o "$T/animation.o" "$ROOT/src/dlls/animation.cpp"
g++ -std=c++17 "${FLAGS[@]}" -c -o "$T/hitbox_test.o" "$ROOT/tests/model_scale/hitbox_test.cpp"
g++ -fsanitize=address,undefined -o "$T/hitbox_test" "$T/animation.o" "$T/hitbox_test.o"
M=$ROOT/gamedir/syndicate/models
if [ $# -eq 0 ]; then set -- "$M/hgrunt.mdl" "$M/barney.mdl" "$M/zombie.mdl" "$M/dgrunt.mdl"; fi
"$T/hitbox_test" "$@"
