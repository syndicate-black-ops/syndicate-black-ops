#!/usr/bin/env python3
"""Turn the analysis build tree's compile_commands.json (clang -m32, same CMake flags as the sniper build) into the one
clang-tidy and cppcheck read: only SBO's own sources (src/, not the SDK's VGUI2 libraries), and the few flags clang
needs to parse code that GCC accepts with -fpermissive (run by scripts/check_sanitizers.sh static).

  compdb.py <in compile_commands.json> <out compile_commands.json> [path filter...]
"""
import json, shlex, sys

ADD = [
    '-Wno-c++11-narrowing',          # GCC -fpermissive: narrowing in braced initializers is a warning there
    '-include', 'string.h',          # game_shared/bitvec.h uses memcpy without <string.h>
    '-Wno-unknown-warning-option', '-Wno-ignored-optimization-argument', '-Wno-register',
    '-fdelayed-template-parsing',     # SDK tier1 templates (utlblockmemory.h) that only MSVC-mode clang accepts
]
DROP = {'-fno-gnu-unique', '-w'}

src, dst, *filters = sys.argv[1:]
out = []
for e in json.load(open(src)):
    f = e['file']
    if '/src/' not in f or 'hl1_source_sdk' in f:
        continue
    if filters and not any(p in f for p in filters):
        continue
    args = [a for a in shlex.split(e['command']) if a not in DROP]
    # after the compiler: insert the extra flags
    args[1:1] = ADD
    if '/src/cl_dll/' in f:
        # extra qualification on members (cl_dll/hud.h), accepted as in the clang-cl build (not on the server:
        # util.cpp defines its own _rotr, an MS builtin)
        args[1:1] = ['-fms-extensions']
    e = dict(e); e.pop('command'); e['arguments'] = args
    out.append(e)
json.dump(out, open(dst, 'w'), indent=1)
print(f'{len(out)} translation units -> {dst}')
