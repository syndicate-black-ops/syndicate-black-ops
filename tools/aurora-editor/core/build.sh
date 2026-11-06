#!/bin/sh
# Aurora editor core: builds SBO's particle code (unchanged) with the engine shim.
#   build.sh stage <repo-root> <stage-dir>   copy the game sources it compiles into <stage-dir>
#   build.sh wasm <stage-dir> <out-dir>      aurora-core.mjs + aurora-core.wasm (emcc)
#   build.sh native <stage-dir> <out-dir>    aurora-native (the parity test's reference; g++)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
SOURCES="particlesys.cpp particlemgr.cpp parsemsg.cpp"
case "$1" in
stage)
	mkdir -p "$3"
	cp "$2"/src/cl_dll/aurora/particlesys.h "$2"/src/cl_dll/aurora/particlesys.cpp \
	   "$2"/src/cl_dll/aurora/particlemgr.h "$2"/src/cl_dll/aurora/particlemgr.cpp \
	   "$2"/src/cl_dll/parsemsg.h "$2"/src/cl_dll/parsemsg.cpp \
	   "$2"/src/game_shared/aurora_shared.h "$3"/
	;;
wasm)
	mkdir -p "$3"
	FILES=""
	for f in $SOURCES; do FILES="$FILES $2/$f"; done
	# -include: the stubs replace the SDK headers; the game's .cpp files are compiled as they are
	emcc -O2 -std=c++14 -Wall -Wno-unused-function -Wno-sign-compare -fno-exceptions -fno-rtti \
		-I"$HERE/include" -I"$2" \
		$FILES "$HERE/shim.cpp" \
		-o "$3/aurora-core.mjs" \
		-sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=AuroraCore -sENVIRONMENT=web,node \
		-sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
		-sEXPORTED_FUNCTIONS=_malloc,_free \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToUTF8,lengthBytesUTF8,HEAPF32,HEAPU8
	;;
native)
	mkdir -p "$3"
	FILES=""
	for f in $SOURCES; do FILES="$FILES $2/$f"; done
	${CXX:-g++} -O2 -std=c++14 -Wall -Wno-unused-function -Wno-class-memaccess -Wno-sign-compare -I"$HERE/include" -I"$2" \
		$FILES "$HERE/shim.cpp" "$HERE/native_main.cpp" -o "$3/aurora-native" -lm
	;;
*)
	echo "usage: build.sh stage|wasm|native ..." >&2
	exit 2
	;;
esac
