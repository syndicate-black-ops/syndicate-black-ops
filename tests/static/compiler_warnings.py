#!/usr/bin/env python3
"""Compiler warnings that point at bugs (clang -fsyntax-only with a chosen -W set) over the analysis compile database
(scripts/check_sanitizers.sh static). The normal builds use -w; this is the list of the warnings worth reading.

  compiler_warnings.py <compile_commands.json> [jobs]   -> warnings on stdout, "file:line:col: warning: msg [-Wflag]"
"""
import json, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

FLAGS = ['-Wno-everything',
         '-Wformat', '-Wformat-extra-args', '-Wformat-insufficient-args', '-Wformat-invalid-specifier',
         '-Wreturn-type', '-Wuninitialized', '-Wsometimes-uninitialized', '-Wconditional-uninitialized',
         '-Warray-bounds', '-Warray-bounds-pointer-arithmetic', '-Wsizeof-pointer-memaccess', '-Wsizeof-array-argument',
         '-Wsizeof-array-div', '-Wfortify-source', '-Wmemset-transposed-args', '-Wstring-plus-int', '-Wself-assign',
         '-Wunsequenced', '-Wvarargs', '-Wnon-pod-varargs', '-Wpointer-bool-conversion', '-Wtautological-pointer-compare',
         '-Wtautological-constant-out-of-range-compare', '-Wdangling', '-Wreturn-stack-address', '-Wint-to-pointer-cast',
         '-Wsizeof-pointer-div', '-Wstrncat-size', '-Wuser-defined-warnings', '-Wbitwise-instead-of-logical',
         '-Wlogical-not-parentheses', '-Wshift-count-overflow', '-Wshift-count-negative', '-Wdivision-by-zero',
         '-Wimplicit-function-declaration', '-Wincompatible-pointer-types', '-Wdelete-non-virtual-dtor',
         '-Wdelete-incomplete', '-Wmismatched-new-delete', '-Wstatic-in-inline', '-Wundefined-bool-conversion',
         '-Wnull-dereference', '-Wnonnull', '-Wstring-compare', '-Wcomma', '-Wsuspicious-memaccess', '-Wempty-body',
         '-Wmisleading-indentation', '-Wenum-compare', '-Wswitch-bool', '-Wloop-analysis', '-Wfor-loop-analysis',
         '-Wrange-loop-analysis', '-Wassign-enum', '-Wimplicit-fallthrough-per-function']

def run(x):
    a, out, skip = list(x['arguments']), [], False
    for t in a:
        if skip:
            skip = False; continue
        if t in ('-o', '-MF', '-MT'):
            skip = True; continue
        if t in ('-c', '-MD'):
            continue
        out.append(t)
    out += ['-fsyntax-only'] + FLAGS
    r = subprocess.run(out, cwd=x['directory'], capture_output=True, text=True, errors='replace')
    return [l for l in r.stderr.splitlines() if ': warning: ' in l]

d = json.load(open(sys.argv[1]))
seen = set()
with ThreadPoolExecutor(int(sys.argv[2]) if len(sys.argv) > 2 else 2) as ex:
    for lines in ex.map(run, d):
        for l in lines:
            if l not in seen:
                seen.add(l); print(l, flush=True)
