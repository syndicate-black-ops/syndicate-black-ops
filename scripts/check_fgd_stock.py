#!/usr/bin/env python3
"""The shipped FGD (sdk/syndicate.fgd) must load completely in the stock editors (Valve Hammer 3.x, J.A.C.K.).

    scripts/check_fgd_stock.py [fgd]          default: sdk/syndicate.fgd

The editors stop reading an FGD at a class header with a helper they do not know (e.g. a particles() helper for an
editor plugin): every class after it is missing from the editor, with no error a mapper sees. This check reads the
file the way the editors do, in order, and stops where they stop. It fails when
  - a class header uses a helper outside STOCK_HELPERS (the helpers stock Hammer and J.A.C.K. read), or
  - the classes read differ in number from the @BaseClass / @PointClass / @SolidClass declarations in the file
    (counted line by line, without the parser), so nothing after a bad line is skipped silently.
Helpers for editor plugins that are not released stay out of the shipped FGD.

Tokens and syntax: scripts/fgd.py's FGD tokenizer. Python 3 standard library only. Exit 0 = pass, 1 = fail.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fgd  # noqa: E402  (scripts/fgd.py: tokenize, _Parser, FgdError)

# Class header helpers that stock Valve Hammer 3.x and J.A.C.K. read (GoldSrc FGDs)
STOCK_HELPERS = (
    "base",        # base(Class, ...)
    "size",        # size(x y z) or size(x1 y1 z1, x2 y2 z2)
    "color",       # color(r g b)
    "iconsprite",  # iconsprite("sprites/....spr")
    "sprite",      # sprite() or sprite("sprites/....spr")
    "studio",      # studio() or studio("models/....mdl")
    "decal",       # decal()
    "sequence",    # sequence(n): the studio() model's sequence
    "body",        # body(n): the studio() model's body group
    "offset",      # offset(x y z)
    "flags",       # flags(Angle, Path, Light, ...)
)
CLASS_TYPES = ("baseclass", "pointclass", "solidclass")
_DECL = re.compile(r"@\s*(\w*class)\b", re.I)


def count_declarations(text):
    """@...Class declarations, counted line by line outside // comments and quoted strings."""
    n = 0
    for line in text.splitlines():
        line = re.sub(r'"[^"\n]*"', '""', line).split("//", 1)[0]
        n += sum(1 for m in _DECL.finditer(line) if m.group(1).lower() in CLASS_TYPES)
    return n


def read_like_editor(text):
    """Classes in file order until the point where a stock editor stops reading.
    Returns (classes, stop): classes = [(name, line)], stop = None or (line, reason)."""
    p = fgd._Parser(fgd.tokenize(text))
    classes = []
    try:
        while p.peek()[0] != "E":
            p.expect("@")
            kind = p.word("a class type")
            low = kind[1].lower()
            if low == "include":
                p.string()
                continue
            if low == "mapsize":
                p.expect("(")
                while not p.is_p(")"):
                    p.next()
                p.next()
                continue
            if low not in CLASS_TYPES:
                return classes, (kind[2], "unknown declaration @%s" % kind[1])
            while not p.is_p("="):
                helper = p.word("a helper or '='")
                if helper[1].lower() not in STOCK_HELPERS:
                    return classes, (helper[2], "helper %s() is not a stock Hammer / J.A.C.K. helper" % helper[1])
                if p.is_p("("):
                    p.next()
                    while not p.is_p(")"):
                        tok = p.next()
                        if tok[0] == "P" and tok[1] != ",":
                            raise fgd.FgdError("line %d: unexpected %r in %s()" % (tok[2], tok[1], helper[1]))
                    p.next()
            p.expect("=")
            name = p.word("a class name")
            if p.is_p(":"):
                p.next()
                p.string()
            p.expect("[")
            while not p.is_p("]"):
                p.word("a key name")
                p.expect("(")
                p.word("a key type")
                p.expect(")")
                if p.peek()[0] == "W" and p.peek()[1].lower() == "readonly":
                    p.next()
                while p.is_p(":"):
                    p.next()
                    tok = p.peek()
                    if tok[0] == "S":
                        p.string()
                    elif tok[0] == "W" and not p.is_p("(", 1):
                        p.next()
                if p.is_p("="):
                    p.next()
                    p.expect("[")
                    while not p.is_p("]"):
                        value = p.next()
                        if value[0] == "P":
                            raise fgd.FgdError("line %d: expected a value, found %r" % (value[2], value[1]))
                        p.expect(":")
                        p.string()
                        if p.is_p(":"):
                            p.next()
                            p.next()
                    p.next()
            p.next()
            classes.append((name[1], kind[2]))
    except fgd.FgdError as e:
        m = re.match(r"line (\d+): (.*)", str(e))
        return classes, (int(m.group(1)), m.group(2)) if m else (0, str(e))
    return classes, None


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else fgd.FGD_PATH
    with open(path, "rb") as fh:
        text = fh.read().decode("latin-1")
    declared = count_declarations(text)
    classes, stop = read_like_editor(text)
    shown = os.path.relpath(path, fgd.REPO) if os.path.abspath(path).startswith(fgd.REPO + os.sep) else path
    fails = []
    if stop:
        fails.append("line %d: %s: a stock editor stops reading %s here" % (stop[0], stop[1], shown))
    if len(classes) != declared:
        fails.append("%d of %d declared classes read (%d not read: %s)"
                     % (len(classes), declared, declared - len(classes),
                        "everything after line %d" % stop[0] if stop else "a declaration the parser does not see"))
    if fails:
        for f in fails:
            print("check_fgd_stock: FAIL: " + f)
        if classes:
            print("check_fgd_stock: last class read: %s (line %d)" % classes[-1])
        return 1
    print("check_fgd_stock: OK: %s, %d of %d classes read, helpers all stock (%s)"
          % (shown, len(classes), declared, ", ".join(STOCK_HELPERS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
