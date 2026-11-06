#!/usr/bin/env bash
# game_playvideo decoder test (docs/VIDEO_PLAYER.md): builds the client's decoder (src/cl_dll/video/video_decoder.cpp)
# with the vendored libogg, libtheora and stb_vorbis (external/VIDEO_LIBRARIES.md) alone, with AddressSanitizer and
# UndefinedBehaviorSanitizer, plays the sample and test files and feeds it truncated, bit-flipped and random data
# (tests/video/video_test.cpp). Needs gcc/g++. Optional: an output folder for the first picture (sample_first.ppm).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
OUT=${1:-}
SAN="-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all"
INC="-I$ROOT/external/libogg/include -I$ROOT/external/libtheora/include -I$ROOT/external/stb_vorbis"
OBJS=()
for c in "$ROOT"/external/libogg/src/{bitwise,framing}.c \
         "$ROOT"/external/libtheora/lib/{bitpack,decinfo,decode,dequant,fragment,huffdec,idct,info,internal,quant,state}.c \
         "$ROOT"/src/cl_dll/video/stb_vorbis_impl.c; do
  o="$T/$(basename "$c" .c).o"
  gcc -std=gnu99 -w $SAN $INC -c "$c" -o "$o"
  OBJS+=("$o")
done
# no ulimit -v: AddressSanitizer reserves terabytes of address space (it uses a few hundred MB)
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter $SAN $INC -I"$ROOT/src/cl_dll/video" -I"$ROOT/src/game_shared" \
  -o "$T/video_test" "$ROOT/tests/video/video_test.cpp" "$ROOT/src/cl_dll/video/video_decoder.cpp" "${OBJS[@]}"
timeout 600 "$T/video_test" "$ROOT/sdk/examples/video" "$ROOT/tests/video/data" ${OUT:+"$OUT"}
