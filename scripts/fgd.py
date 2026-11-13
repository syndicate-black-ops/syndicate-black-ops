#!/usr/bin/env python3
"""SBO map-editor definition file (FGD): check against the game code, copy into a dist folder.

    scripts/fgd.py check                 compare sdk/syndicate.fgd with src/dlls and the editor assets
    scripts/fgd.py sync <dir>            copy the FGD and the editor models and sprites into the dist folder <dir>
    scripts/fgd.py sync --check <dir>    report what sync would change; writes nothing, exit 1 on a difference
    scripts/fgd.py nohelper              list the classes that have no editor model or sprite

check fails (exit 1, one line per problem) on:

    parse           the file does not parse
    duplicate       a class defined twice, a key defined twice in one class
    no_code         a point/solid class with no LINK_ENTITY_TO_CLASS in src/dlls
    unread_key      a key neither the entity's KeyValue chain nor the engine reads
    ignored_key     a key the code accepts and throws away
    key_case        a key spelled with other letter case than the code compares with
    missing_key     a key the entity's own code reads that the class does not offer
    missing_field   an entity field a mapper sets as a key (speed, dmg, message, health, frags, netname, noise...)
                    that the entity's code reads, not assigned first by its Spawn / Precache or filled by its
                    KeyValue from another key, and that the class does not offer
    missing_flag    a spawnflag bit the entity's code tests on its own spawnflags (its class chain: the methods its
                    code can run) that neither the class nor its bases offer (editors merge the bases' flags)
    missing_class   a code classname with no FGD class
    asset           a studio()/iconsprite()/sprite() file that is neither in gamedir/syndicate/ nor in the list
                    of stock Half-Life files
    asset_case      such a file whose name differs in letter case
    left_out_flag   a spawnflag bit the rules file lists as left out ("left_out_flags": unused bits, a reason each)
                    that the class or a class based on it offers
    left_out_key    a key or a choice value the rules file lists as left out ("left_out_keys": "class.key" or
                    "class.key=value", a reason each) that the class or a class based on it offers
    stale_rule      an entry of the rules file that no longer matches anything

Known deviations live in scripts/fgd_rules.json: "accepted" entries (a reason each) are silent,
"tracked" entries (known, not yet fixed) are printed as warnings with a count and do not fail.
Anything not listed there fails, so a new entity needs its FGD class in the same commit.

sync writes <dir>/syndicate/syndicate.fgd and mirrors gamedir/syndicate/models/editor/ and sprites/editor/ to
<dir>/syndicate/ (adds, updates, removes what is gone from the source; inside those two directories only). Leftovers
are removed: <dir>/syndicate/dark.fgd (the old name) and the editor files of an older build in
<dir>/syndicate_addon/models/editor/ and sprites/editor/ (they would override the game folder's). <dir>/syndicate_addon/
gets gamedir/syndicate_addon/readme.txt. <dir> must contain syndicate/liblist.gam.

Python 3 standard library only. The code scan is regex based (KeyValue comparisons, class
declarations), not a C++ parser. Spawnflag tests are "pev->spawnflags & X" and "FBitSet( pev->spawnflags, X )"
with X a number or a #define / const int; a base class method counts when the chain can reach it (see _reached).
Choice values are checked only against "left_out_keys".
"""

import argparse
import collections
import filecmp
import fnmatch
import json
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
FGD_PATH = os.path.join(REPO, "sdk", "syndicate.fgd")
GAME_DIR = os.path.join(REPO, "gamedir", "syndicate")
ADDON_README = os.path.join(REPO, "gamedir", "syndicate_addon", "readme.txt")
SRC_DIR = os.path.join(REPO, "src", "dlls")
RULES_PATH = os.path.join(HERE, "fgd_rules.json")

FGD_NAME = "syndicate.fgd"
OLD_FGD_NAME = "dark.fgd"
ADDON_NAME = "syndicate_addon"
EDITOR_DIRS = ("models/editor", "sprites/editor")    # in the game folder, mirrored by sync
ASSET_HELPERS = ("studio", "iconsprite", "sprite")


def die(msg):
    sys.stderr.write("fgd: " + msg + "\n")
    sys.exit(2)


# ---------------------------------------------------------------------------
# FGD parser (GoldSrc dialect: // comments only)
# ---------------------------------------------------------------------------

class FgdError(Exception):
    pass


_TOKEN = re.compile(r'''[ \t\r\n]+|//[^\n]*|"([^"\n]*)"|([@=:\[\](),+])|([^\s@=:\[\](),+"]+)''')


def tokenize(text):
    """List of (kind, value, line); kind is S (string), P (punctuation), W (word)."""
    out = []
    pos = 0
    line = 1
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m:
            raise FgdError("line %d: unexpected character %r" % (line, text[pos]))
        if m.group(1) is not None:
            out.append(("S", m.group(1), line))
        elif m.group(2):
            out.append(("P", m.group(2), line))
        elif m.group(3):
            if m.group(3).startswith("/*"):
                raise FgdError("line %d: /* */ block comments are not FGD syntax, use //" % line)
            out.append(("W", m.group(3), line))
        line += m.group(0).count("\n")
        pos = m.end()
    return out


class _Parser:
    def __init__(self, tokens):
        self.t = tokens
        self.i = 0

    def peek(self, k=0):
        j = self.i + k
        return self.t[j] if j < len(self.t) else ("E", "", self.t[-1][2] if self.t else 0)

    def next(self):
        tok = self.peek()
        if tok[0] == "E":
            raise FgdError("line %d: unexpected end of file" % tok[2])
        self.i += 1
        return tok

    def is_p(self, char, k=0):
        tok = self.peek(k)
        return tok[0] == "P" and tok[1] == char

    def expect(self, char):
        tok = self.next()
        if not (tok[0] == "P" and tok[1] == char):
            raise FgdError("line %d: expected %r, found %r" % (tok[2], char, tok[1]))

    def word(self, what):
        tok = self.next()
        if tok[0] != "W":
            raise FgdError("line %d: expected %s, found %r" % (tok[2], what, tok[1]))
        return tok

    def string(self):
        tok = self.next()
        if tok[0] != "S":
            raise FgdError("line %d: expected a quoted string, found %r" % (tok[2], tok[1]))
        parts = [tok[1]]
        while self.is_p("+"):
            self.next()
            tok = self.next()
            if tok[0] != "S":
                raise FgdError("line %d: expected a quoted string after +" % tok[2])
            parts.append(tok[1])
        return "".join(parts)


def parse_fgd(path):
    """Returns (classes, duplicates). classes: name -> dict(kind, line, bases, helpers, props);
    helpers: list of (name, [string/word arguments]); props: lower-case key -> dict(name, type,
    default, line, choices). duplicates: list of (line, text)."""
    with open(path, "rb") as fh:
        text = fh.read().decode("latin-1")
    p = _Parser(tokenize(text))
    classes = collections.OrderedDict()
    duplicates = []
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
        if low not in ("baseclass", "pointclass", "solidclass"):
            raise FgdError("line %d: unknown class type @%s" % (kind[2], kind[1]))
        cls = dict(kind=low, line=kind[2], bases=[], helpers=[], props=collections.OrderedDict())
        while not p.is_p("="):
            helper = p.word("a helper or '='")
            args = []
            if p.is_p("("):
                p.next()
                while not p.is_p(")"):
                    tok = p.next()
                    if tok[0] != "P":
                        args.append(tok[1])
                    elif tok[1] != ",":
                        raise FgdError("line %d: unexpected %r in %s()" % (tok[2], tok[1], helper[1]))
                p.next()
            if helper[1].lower() == "base":
                cls["bases"] += args
            else:
                cls["helpers"].append((helper[1].lower(), args))
        p.expect("=")
        name = p.word("a class name")[1]
        if p.is_p(":"):
            p.next()
            p.string()
        p.expect("[")
        while not p.is_p("]"):
            key = p.word("a key name")
            p.expect("(")
            typ = p.word("a key type")[1].lower()
            p.expect(")")
            prop = dict(name=key[1], type=typ, default=None, line=key[2], choices=None)
            if p.peek()[0] == "W" and p.peek()[1].lower() == "readonly":
                p.next()
            fields = []
            while p.is_p(":"):
                p.next()
                tok = p.peek()
                if tok[0] == "S":
                    fields.append(p.string())
                elif tok[0] == "W" and not p.is_p("(", 1):
                    fields.append(p.next()[1])
                else:
                    fields.append(None)
            if len(fields) > 3:
                raise FgdError("line %d: too many ':' fields on key %s" % (key[2], key[1]))
            if len(fields) > 1:
                prop["default"] = fields[1]
            if p.is_p("="):
                p.next()
                p.expect("[")
                if typ not in ("choices", "flags"):
                    raise FgdError("line %d: value list on key %s of type %s" % (key[2], key[1], typ))
                choices = collections.OrderedDict()
                while not p.is_p("]"):
                    value = p.next()
                    if value[0] == "P":
                        raise FgdError("line %d: expected a value, found %r" % (value[2], value[1]))
                    p.expect(":")
                    p.string()
                    if p.is_p(":"):
                        p.next()
                        p.next()
                    if value[1] in choices:
                        duplicates.append((value[2], "%s.%s: value %s listed twice" % (name, key[1], value[1])))
                    choices[value[1]] = True
                p.next()
                prop["choices"] = choices
            elif typ in ("choices", "flags"):
                raise FgdError("line %d: key %s of type %s has no value list" % (key[2], key[1], typ))
            if key[1].lower() in cls["props"]:
                duplicates.append((key[2], "%s.%s: key defined twice in the class" % (name, key[1])))
            cls["props"][key[1].lower()] = prop
        p.next()
        if name in classes:
            duplicates.append((cls["line"], "%s: class defined twice" % name))
        classes[name] = cls
    for name, cls in classes.items():
        for base in cls["bases"]:
            if base not in classes:
                raise FgdError("line %d: %s: unknown base class %s" % (cls["line"], name, base))
    return classes, duplicates


def base_chain(classes, name, seen=None):
    """name and every class it inherits from, bases first."""
    seen = [] if seen is None else seen
    if name in seen:
        return seen
    for base in classes[name]["bases"]:
        base_chain(classes, base, seen)
    seen.append(name)
    return seen


def effective_props(classes, name):
    out = collections.OrderedDict()
    for cname in base_chain(classes, name):
        for key, prop in classes[cname]["props"].items():
            out[key] = prop
    return out


def effective_flags(classes, name):
    """The spawnflag bits (strings) the editor offers on the class: the editors merge the spawnflags lists of the
    class and all its bases (trigger_once's own 8 plus Trigger's 1, 2, 4 and Appearflags' 2048)."""
    out = collections.OrderedDict()
    for cname in base_chain(classes, name):
        prop = classes[cname]["props"].get("spawnflags")
        if prop and prop["choices"]:
            out.update(prop["choices"])
    return out


def effective_helpers(classes, name):
    out = []
    for cname in base_chain(classes, name):
        out += classes[cname]["helpers"]
    return out


# ---------------------------------------------------------------------------
# Code scan (src/dlls)
# ---------------------------------------------------------------------------

_STRIP = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|/\*.*?\*/|//[^\n]*', re.S)
_LINK = re.compile(r'^[ \t]*LINK_ENTITY_TO_CLASS\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)', re.M)
_ACCESS = r'(?:(?:public|private|protected|virtual)\s+)*'
_CLASS = re.compile(r'\bclass\s+(?:\w+\s+)?(\w+)\s*:\s*(' + _ACCESS + r'\w+(?:\s*,\s*' + _ACCESS + r'\w+)*)\s*\{')
_CLASS_BODY = re.compile(r'\bclass\s+(?:\w+\s+)?(\w+)\s*(?::[^{;]*)?\{')
_METHOD = re.compile(r'\b(\w+)\s*::\s*(\w+)\s*\([^;{}()]*\)\s*(?:const\s*)?\{')
_INLINE_KV = re.compile(r'\bKeyValue\s*\(\s*KeyValueData[^)]*\)\s*\{')
_KEY = re.compile(r'(?:FStrEq|FStrnEq|strcmp|stricmp|strncmp|strnicmp|_stricmp)\s*\(\s*'
                  r'(?:pkvd\s*->\s*szKeyName|szKeyName|pszKey|key)\s*,\s*"([^"]*)"')
_ENTVAR = re.compile(r'DEFINE_ENTITY_(?:FIELD|GLOBAL_FIELD)\w*\s*\(\s*(\w+)')
_DEFINE = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+([^\n]+)$', re.M)
_CONST = re.compile(r'\bconst\s+int\s+(\w+)\s*=\s*([^;]+);')
# a test of the entity's own spawnflags: "pev->spawnflags & X" (not "&=") or "FBitSet( pev->spawnflags, X )";
# another entity's ("pOther->pev->spawnflags") is that entity's business
_OWN_FLAGS = r'(?<![\w>.])pev\s*->\s*spawnflags'
_FLAG_AND = re.compile(_OWN_FLAGS + r'\s*&(?![&=])\s*')
_FLAG_BITSET = re.compile(r'\bFBitSet\s*\(\s*' + _OWN_FLAGS + r'\s*,\s*')
_FLAG_EXPR = re.compile(r'\(|\)|\||<<|0[xX][0-9a-fA-F]+[uUlL]*|\d+[uUlL]*|\w+|\s+')


def _flag_operand(text, pos, constants):
    """The bits of the operand at text[pos] (up to a ')' that closes nothing, '&&', '||', '?', ',', ';' or '=='):
    a set of ints, or None when a name in it is not a known constant."""
    depth = 0
    out = []
    while pos < len(text):
        if text.startswith(("&&", "||", "==", "!="), pos) or text[pos] in "?,;:" \
                or (text[pos] == ")" and depth == 0):
            break
        m = _FLAG_EXPR.match(text, pos)
        if not m:
            return None
        tok = m.group(0)
        if tok == "(":
            depth += 1
        elif tok == ")":
            depth -= 1
        if not tok.isspace():
            out.append(tok)
        pos = m.end()
    expr = []
    for tok in out:
        if re.match(r'\d|0[xX]', tok):
            expr.append(str(int(tok.rstrip("uUlL"), 0)))
        elif re.match(r'\w', tok):
            if tok not in constants:
                return None
            expr.append(str(constants[tok]))
        else:
            expr.append(tok)
    try:
        value = eval(" ".join(expr), {"__builtins__": {}})
    except Exception:
        return None
    if not isinstance(value, int) or value <= 0:
        return None
    return set(1 << i for i in range(32) if value & (1 << i))


# entity fields a mapper sets as keys (the engine fills them by name) that an entity's code reads: "pev->speed" not
# followed by an assignment
MAPPER_FIELDS = ("speed", "dmg", "message", "health", "frags", "armorvalue", "max_health", "netname", "noise",
                 "noise1", "noise2", "noise3", "impulse")
_FIELD_READ = re.compile(r'(?<![\w>.])pev\s*->\s*(' + "|".join(MAPPER_FIELDS) +
                         r')\b(?!\s*(?:=(?!=)|\+=|-=|\*=|/=|\|=|&=))')
_CALLS = re.compile(r'(?:\b(\w+)\s*::\s*)?\b(\w+)\s*\(|&\s*(\w+)\s*::\s*(\w+)')
_VIRTUAL = re.compile(r'\bvirtual\b[^;{(]*?\b(\w+)\s*\(')


def _reached(chain, methods, virtuals, scopes=None):
    """(class, method) pairs of the entity's class chain (most derived first) its code can run: every method of
    the entity's own class, the virtual methods the chain does not override (the engine and the framework call
    them), the methods those call or hand out (SetTouch( &CBaseTrigger::HurtTouch )), and the inline code of
    each class body (method None). A method of a base class that nothing in the chain calls is left out, so a
    base class's code for other entities (CBaseToggle::AxisDir for rotating entities) does not count.
    scopes: class -> method: that class's virtual methods count only when the rest of the chain calls the method
    (CBaseMonster's AI runs only after MonsterInit; scripted_sequence and cycler derive from it and never call it)."""
    scopes = scopes or {}
    seen = _reach(chain, methods, virtuals, set(c for c in chain if c in scopes))
    late = set(c for c in chain if c in scopes and (c, scopes[c]) not in seen)
    return _reach(chain, methods, virtuals, late) if late != set(c for c in chain if c in scopes) else seen


def _reach(chain, methods, virtuals, unseeded):
    owner = {}
    for c in chain:
        for name in methods.get(c, {}):
            owner.setdefault(name, c)
    todo = [(chain[0], name) for name in methods.get(chain[0], {})] if chain else []
    todo += [(c, name) for name, c in owner.items() if name in virtuals and c not in unseeded]
    seen = set((c, None) for c in chain if c not in unseeded)
    while todo:
        c, name = todo.pop()
        if (c, name) in seen:
            continue
        seen.add((c, name))
        for qual, call, aqual, aname in methods.get(c, {}).get(name, ()):
            if aname:
                qual, call = aqual, aname
            definer = [x for x in chain[chain.index(qual):] if call in methods.get(x, {})] if qual in chain else []
            if definer:
                todo.append((definer[0], call))
            elif call in owner:
                todo.append((owner[call], call))
    return sorted(seen, key=lambda x: (chain.index(x[0]), x[1] or ""))


def _constants(files):
    """Integer #defines and const ints of the code: name -> value (an expression of numbers, '|', '<<'
    and other such names)."""
    raw = {}
    for text in files.values():
        for m in _DEFINE.finditer(text):
            raw.setdefault(m.group(1), m.group(2).strip())
        for m in _CONST.finditer(text):
            raw.setdefault(m.group(1), m.group(2).strip())
    values = {}
    for _ in range(4):
        for name, expr in raw.items():
            if name in values:
                continue
            bits = _flag_operand(expr + ";", 0, values)
            if bits:
                values[name] = sum(bits)
    return values


def _flag_reads(body, constants):
    """[(bit, the operand's text)] the code tests on its own spawnflags, and the operands it could not resolve."""
    reads = []
    unresolved = []
    for rx in (_FLAG_AND, _FLAG_BITSET):
        for m in rx.finditer(body):
            stop = re.search(r'[;\n]', body[m.end():])
            text = body[m.end():m.end() + (stop.start() if stop else 80)].strip()
            bits = _flag_operand(body, m.end(), constants)
            if bits is None:
                unresolved.append(text[:60])
                continue
            name = re.match(r'\(?\s*(\w+)', text)
            for bit in bits:
                reads.append((bit, name.group(1) if name else str(bit)))
    return reads, unresolved


def _block_end(text, start):
    """text[start] is '{': index after the matching '}'."""
    depth = 0
    in_string = False
    i = start
    while i < len(text):
        ch = text[i]
        if in_string:
            if ch == "\\":
                i += 1
            elif ch == '"':
                in_string = False
        elif ch == '"':
            in_string = True
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return len(text)


def dispatch_keys(files):
    """Keys DispatchKeyValue (cbase.cpp) hands to a helper for every entity, e.g. ObjectHint_KeyValue( pEntity,
    pkvd->szKeyName, ... ): the literals each helper compares szKeyName with."""
    cbase = files.get("cbase.cpp", "")
    start = cbase.find("void DispatchKeyValue(")
    if start < 0:
        return set()
    body = cbase[start:_block_end(cbase, cbase.index("{", start))]
    keys = set()
    for helper in re.findall(r'\b(\w+_KeyValue)\s*\(\s*pEntity\s*,\s*pkvd->szKeyName', body):
        for text in files.values():
            m = re.search(r'\bBOOL\s+%s\s*\([^;{]*\)\s*\{' % helper, text)
            if not m:
                continue
            hbody = text[m.end() - 1:_block_end(text, m.end() - 1)]
            keys |= set(k.lower() for k in re.findall(r'szKeyName\s*,\s*"(\w+)"', hbody))
    return keys


def scan_code(src_dir, flag_scopes=None, field_skip=None):
    """Returns (entities, entvars). entities: classname -> dict(cls, file, keys, ignored, flags);
    keys: key -> the C++ class of the entity's chain that reads it; ignored: keys a handler only
    marks as handled; flags: spawnflag bit -> (the C++ class of the chain that tests it, the operand's name).
    entvars: lower-case names of the entity fields the engine fills by name."""
    files = {}
    for root, dirs, names in os.walk(src_dir):
        dirs.sort()
        for name in sorted(names):
            if name.endswith((".cpp", ".h")):
                path = os.path.join(root, name)
                with open(path, "rb") as fh:
                    text = fh.read().decode("latin-1")
                # comments out, strings kept
                text = _STRIP.sub(lambda m: m.group(0) if m.group(0)[0] in "\"'"
                                  else ("\n" * m.group(0).count("\n") or " "), text)
                files[os.path.relpath(path, src_dir)] = text

    links = {}
    parents = {}
    keys = {}
    ignored = {}
    flags = {}          # (class, method or None for the class body's inline code) -> {bit: where}
    methods = {}        # class -> {method: [names the body calls or takes the address of]}
    virtuals = set()    # method names declared virtual in some class
    constants = _constants(files)

    fields = {}         # (class, method) -> {entity field: where}
    spawn_set = {}      # class -> fields its Spawn assigns before reading them

    def add_flags(cls, method, text, where):
        reads, _ = _flag_reads(text, constants)
        for bit, operand in reads:
            flags.setdefault((cls, method), {}).setdefault(bit, "%s, %s" % (operand, where))
        for m in _FIELD_READ.finditer(text):
            fields.setdefault((cls, method), {}).setdefault(m.group(1), where)
        # a field KeyValue fills from another key name ("m_iMode" into pev->impulse) is that key's
        for f in MAPPER_FIELDS:
            if (method == "KeyValue" and re.search(r'(?<![\w>.])pev\s*->\s*%s\s*=(?!=)' % f, text)) or \
                    re.search(r'(?<![\w>.])pev\s*->\s*%s\s*=[^;]*pkvd\s*->\s*szValue' % f, text):
                spawn_set.setdefault(cls, set()).add(f)
        if method in ("Spawn", "Precache"):
            # a field Spawn assigns before it reads it is the entity's own state, not a key (pev->health = 100)
            for f in MAPPER_FIELDS:
                w = re.search(r'(?<![\w>.])pev\s*->\s*%s\s*=(?!=)' % f, text)
                first_read = min([m.start() for m in _FIELD_READ.finditer(text) if m.group(1) == f] or [len(text)])
                if w and w.start() < first_read:
                    spawn_set.setdefault(cls, set()).add(f)

    def add_keys(cls, text):
        matches = list(_KEY.finditer(text))
        pending = []
        for i, m in enumerate(matches):
            keys.setdefault(cls, set()).add(m.group(1))
            end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
            segment = text[m.end():end]
            if re.match(r'^\s*\)\s*\|\|\s*\(?\s*$', segment):
                pending.append(m.group(1))
                continue
            stop = re.search(r'\belse\b', segment)
            block = re.sub(r'[\s{})]', '', segment[:stop.start()] if stop else segment)
            if i + 1 == len(matches) and block.startswith("pkvd->fHandled=TRUE;") \
                    and "KeyValue" not in segment.split("else")[0]:
                block = block.split(";")[0] + ";"
            if block == "pkvd->fHandled=TRUE;":
                ignored.setdefault(cls, set()).update(pending + [m.group(1)])
            pending = []

    for name in sorted(files):
        text = files[name]
        for m in _LINK.finditer(text):
            links[m.group(1)] = (m.group(2), name)
        for m in _CLASS.finditer(text):
            parents[m.group(1)] = [re.sub(r'\b(public|private|protected|virtual)\b', '', x).strip()
                                   for x in m.group(2).split(",")]
        for m in _METHOD.finditer(text):
            body = text[m.end() - 1:_block_end(text, m.end() - 1)]
            if "szKeyName" in body or m.group(2) in ("KeyValue", "ApplyKey"):
                add_keys(m.group(1), body)
            add_flags(m.group(1), m.group(2), body, "%s::%s, %s" % (m.group(1), m.group(2), name))
            methods.setdefault(m.group(1), {})[m.group(2)] = _CALLS.findall(body)
        for m in _CLASS_BODY.finditer(text):
            body = text[m.end() - 1:_block_end(text, m.end() - 1)]
            add_flags(m.group(1), None, body, "%s, %s" % (m.group(1), name))
            virtuals.update(_VIRTUAL.findall(body))
            # inline methods: their names, so a call reaches the class body's code
            for mm in re.finditer(r'\b(\w+)\s*\([^;{}()]*\)\s*(?:const\s*)?\{', body[1:]):
                methods.setdefault(m.group(1), {}).setdefault(mm.group(1), [])
            for mm in _INLINE_KV.finditer(body):
                add_keys(m.group(1), body[mm.end() - 1:_block_end(body, mm.end() - 1)])

    util = files.get("util.cpp", "")
    start = util.find("gEntvarsDescription[]")
    if start < 0:
        die("gEntvarsDescription not found in src/dlls/util.cpp")
    entvars = set(x.lower() for x in _ENTVAR.findall(util[start:util.index("};", start)]))
    entvars |= dispatch_keys(files)

    def chain(cls, seen):
        if cls not in seen:
            seen.append(cls)
            for parent in parents.get(cls, []):
                chain(parent, seen)
        return seen

    entities = {}
    for name, (cls, fname) in links.items():
        ent_keys = {}
        ent_ignored = set()
        ent_flags = {}
        ent_fields = {}
        for c, method in _reached(chain(cls, []), methods, virtuals, flag_scopes):
            for bit, where in sorted(flags.get((c, method), {}).items()):
                ent_flags.setdefault(bit, (c, where))
            if c in (field_skip or ()):
                continue
            for field, where in sorted(fields.get((c, method), {}).items()):
                ent_fields.setdefault(field, (c, where))
        for c in chain(cls, []):
            for f in spawn_set.get(c, ()):
                ent_fields.pop(f, None)
        for c in chain(cls, []):
            for key in sorted(keys.get(c, ())):
                if key not in ent_keys:
                    ent_keys[key] = c
                    if key in ignored.get(c, ()):
                        ent_ignored.add(key)
        entities[name] = dict(cls=cls, file=fname, keys=ent_keys, ignored=ent_ignored, flags=ent_flags,
                              fields=ent_fields)
    return entities, entvars


# ---------------------------------------------------------------------------
# check
# ---------------------------------------------------------------------------

def load_rules():
    with open(RULES_PATH, encoding="utf-8") as fh:
        return json.load(fh)


def file_index(*roots):
    """Relative paths (forward slashes) of the .mdl/.spr files under the roots."""
    out = set()
    for root in roots:
        for sub in ("models", "sprites"):
            for dirpath, dirs, names in os.walk(os.path.join(root, sub)):
                dirs.sort()
                for name in names:
                    if name.lower().endswith((".mdl", ".spr")):
                        out.add(os.path.relpath(os.path.join(dirpath, name), root).replace(os.sep, "/"))
    return out


def find_problems(classes, duplicates, entities, entvars, rules):
    """List of (category, id, text)."""
    problems = []
    for line, text in duplicates:
        problems.append(("duplicate", text.split(":")[0], "line %d: %s" % (line, text)))

    tool_keys = rules["tool_keys"]["keys"] + rules.get("dispatch_keys", {}).get("keys", [])
    shared = set(rules["shared_code_classes"]["classes"])
    concrete = [n for n, c in classes.items() if c["kind"] != "baseclass"]
    by_lower = {n.lower(): n for n in entities}

    for name in concrete:
        if name not in entities:
            other = by_lower.get(name.lower())
            problems.append(("no_code", name, "line %d: no LINK_ENTITY_TO_CLASS( %s, ... ) in src/dlls%s"
                             % (classes[name]["line"], name, " (the code has %s)" % other if other else "")))
            continue
        ent = entities[name]
        code_lower = {k.lower(): k for k in ent["keys"]}
        props = effective_props(classes, name)
        for low, prop in props.items():
            key = prop["name"]
            ident = "%s.%s" % (name, key)
            if low == "spawnflags":
                continue
            if key in ent["keys"]:
                if key in ent["ignored"]:
                    problems.append(("ignored_key", ident, "%s accepts the key and does not use it"
                                     % ent["keys"][key]))
                continue
            if low in entvars or any(fnmatch.fnmatchcase(key, pat) for pat in tool_keys):
                continue
            if low in code_lower:
                problems.append(("key_case", ident, "%s compares with \"%s\" (case-sensitive): the key is not read"
                                 % (ent["keys"][code_lower[low]], code_lower[low])))
            else:
                problems.append(("unread_key", ident, "not read by %s's KeyValue chain, not an entity field"
                                 % ent["cls"]))
        offered = effective_flags(classes, name)
        chain_names = base_chain(classes, name)
        for bit in sorted(ent["flags"]):
            reader, where = ent["flags"][bit]
            if str(bit) in offered:
                continue
            left_out = [c for c in chain_names if "%s:%d" % (c, bit) in rules.get("left_out_flags", {})]
            if left_out:
                continue
            problems.append(("missing_flag", "%s:%d" % (name, bit), "the code tests it (%s), the FGD does not "
                             "offer it" % where))
        for field in sorted(ent.get("fields", {})):
            if field not in props:
                reader, where = ent["fields"][field]
                problems.append(("missing_field", "%s.%s" % (name, field), "the code reads pev->%s (%s), the FGD does "
                                 "not offer the key" % (field, where)))
        for key in sorted(ent["keys"]):
            reader = ent["keys"][key]
            if key in ent["ignored"] or reader in shared or key.lower() in props:
                continue
            problems.append(("missing_key", "%s.%s" % (name, key), "read by %s::KeyValue (%s), not in the class"
                             % (reader, ent["file"])))

    have = set(concrete)
    hidden = [pat for group in rules["not_for_mappers"] for pat in group["names"]]
    used_hidden = set()
    for name in sorted(entities):
        if name in have:
            continue
        match = [pat for pat in hidden if fnmatch.fnmatchcase(name, pat)]
        if match:
            used_hidden.update(match)
            continue
        problems.append(("missing_class", name, "LINK_ENTITY_TO_CLASS in %s (%s), no FGD class"
                         % (entities[name]["file"], entities[name]["cls"])))
    for pat in hidden:
        if pat not in used_hidden:
            reason = "an FGD class exists" if pat in have else "no such classname in src/dlls"
            problems.append(("stale_rule", "not_for_mappers:" + pat, reason))

    for ident in sorted(rules.get("left_out_flags", {})):
        name, _, bit = ident.partition(":")
        if name not in classes:
            problems.append(("stale_rule", "left_out_flags:" + ident, "no such FGD class"))
            continue
        for other in sorted(classes):
            if name not in base_chain(classes, other):
                continue
            if bit in effective_flags(classes, other):
                problems.append(("left_out_flag", "%s:%s" % (other, bit), "the FGD offers it; fgd_rules.json "
                                 "left_out_flags: %s" % rules["left_out_flags"][ident]))

    # FGD items that do something only under the teamplay rules: kept in the FGD as they are, listed so the reason is
    # on record; an entry the FGD no longer has is stale
    for ident in sorted(rules.get("teamplay_only", {}).get("items", {})):
        name, sep, bit = ident.partition(":")
        if not sep:
            name, _, key = ident.partition(".")
        if name not in classes or (sep and bit not in effective_flags(classes, name)) or \
                (not sep and key.lower() not in effective_props(classes, name)):
            problems.append(("stale_rule", "teamplay_only:" + ident, "the FGD does not offer it"))

    for ident in sorted(rules.get("left_out_keys", {})):
        target, _, value = ident.partition("=")
        name, _, key = target.partition(".")
        if name not in classes:
            problems.append(("stale_rule", "left_out_keys:" + ident, "no such FGD class"))
            continue
        for other in sorted(classes):
            if name not in base_chain(classes, other):
                continue
            prop = effective_props(classes, other).get(key.lower())
            if prop is None or (value and not (prop["choices"] and value in prop["choices"])):
                continue
            problems.append(("left_out_key", "%s.%s%s" % (other, prop["name"], "=" + value if value else ""),
                             "the FGD offers it; fgd_rules.json left_out_keys: %s" % rules["left_out_keys"][ident]))

    local = file_index(GAME_DIR)
    local_lower = {p.lower(): p for p in local}
    stock = set(rules["stock_assets"]["paths"])
    stock_lower = {p.lower(): p for p in stock}
    used_stock = set()
    for name, cls in classes.items():
        for helper, args in cls["helpers"]:
            if helper not in ASSET_HELPERS or not args:
                continue
            path = args[0]
            ident = "%s:%s" % (name, path)
            where = "line %d: %s(\"%s\")" % (cls["line"], helper, path)
            if "\\" in path or not path.lower().endswith((".mdl", ".spr")):
                problems.append(("asset", ident, where + ": not a models/....mdl or sprites/....spr path"))
            elif path in local:
                pass
            elif path.lower() in local_lower:
                problems.append(("asset_case", ident, where + ": the file is " + local_lower[path.lower()]))
            elif path in stock:
                used_stock.add(path)
            elif path.lower() in stock_lower:
                problems.append(("asset_case", ident, where + ": the stock file is " + stock_lower[path.lower()]))
            else:
                problems.append(("asset", ident, where + ": not in gamedir/syndicate or the stock list"))
    for path in sorted(stock - used_stock):
        problems.append(("stale_rule", "stock_assets:" + path,
                         "the game repo has the file" if path in local else "no helper references it"))
    return problems


def cmd_check(args):
    rules = load_rules()
    try:
        classes, duplicates = parse_fgd(FGD_PATH)
    except (OSError, FgdError) as err:
        print("parse: sdk/%s: %s" % (FGD_NAME, err))
        print("fgd check: FAILED (1 problem)")
        return 1
    entities, entvars = scan_code(SRC_DIR, rules["flag_scopes"]["classes"],
                                  set(rules["shared_code_classes"]["classes"]))
    problems = find_problems(classes, duplicates, entities, entvars, rules)

    accepted = rules.get("accepted", {})
    tracked = rules.get("tracked", {})
    seen = set((cat, ident) for cat, ident, _ in problems)
    errors = []
    warnings = []
    for cat, ident, text in problems:
        if ident in accepted.get(cat, {}):
            continue
        if ident in tracked.get(cat, {}).get("items", ()):
            warnings.append((cat, ident, text))
        else:
            errors.append((cat, ident, text))
    for section, table in (("accepted", accepted), ("tracked", tracked)):
        for cat in sorted(table):
            items = table[cat] if section == "accepted" else table[cat]["items"]
            for ident in items:
                if (cat, ident) not in seen:
                    errors.append(("stale_rule", "%s.%s:%s" % (section, cat, ident),
                                   "listed in fgd_rules.json but no longer a difference: remove the entry"))

    for cat, ident, text in warnings:
        print("warning (tracked): %s: %s: %s" % (cat, ident, text))
    for cat, ident, text in errors:
        print("%s: %s: %s" % (cat, ident, text))
    kinds = collections.Counter(c["kind"] for c in classes.values())
    print("fgd check: %d classes (%d base, %d point, %d solid), %d code classnames"
          % (len(classes), kinds["baseclass"], kinds["pointclass"], kinds["solidclass"], len(entities)))
    if warnings:
        counts = collections.Counter(cat for cat, _, _ in warnings)
        print("fgd check: %d tracked, not yet fixed (%s)"
              % (len(warnings), ", ".join("%s %d" % kv for kv in sorted(counts.items()))))
    if errors:
        print("fgd check: FAILED (%d problem%s)" % (len(errors), "" if len(errors) == 1 else "s"))
        return 1
    print("fgd check: OK")
    return 0


def cmd_nohelper(args):
    try:
        classes, _ = parse_fgd(FGD_PATH)
    except (OSError, FgdError) as err:
        die("sdk/%s: %s" % (FGD_NAME, err))
    for name, cls in classes.items():
        if cls["kind"] == "baseclass":
            continue
        if not any(h in ASSET_HELPERS for h, _ in effective_helpers(classes, name)):
            print("%s %s" % (name, "PointClass" if cls["kind"] == "pointclass" else "SolidClass"))
    return 0


# ---------------------------------------------------------------------------
# sync
# ---------------------------------------------------------------------------

def tree_files(root):
    out = []
    for dirpath, dirs, names in os.walk(root):
        dirs.sort()
        for name in sorted(names):
            out.append(os.path.relpath(os.path.join(dirpath, name), root))
    return out


def same_file(a, b):
    return os.path.isfile(b) and not os.path.islink(b) and filecmp.cmp(a, b, shallow=False)


def cmd_sync(args):
    dest = os.path.abspath(args.dir)
    game = os.path.join(dest, "syndicate")
    if not os.path.isfile(os.path.join(game, "liblist.gam")):
        die("%s is not a dist build folder (no syndicate/liblist.gam)" % dest)
    if not os.path.isfile(FGD_PATH) or not os.path.isfile(ADDON_README):
        die("sdk/%s or gamedir/%s/readme.txt is missing" % (FGD_NAME, ADDON_NAME))
    addon = os.path.join(dest, ADDON_NAME)
    if os.path.islink(addon) or os.path.islink(game):
        die("%s: syndicate or %s is a symbolic link, not touched" % (dest, ADDON_NAME))

    actions = []    # (verb, path relative to dest, source or None)

    def put(src, rel):
        target = os.path.join(dest, rel)
        if not same_file(src, target):
            actions.append(("update" if os.path.lexists(target) else "add", rel, src))

    put(FGD_PATH, os.path.join("syndicate", FGD_NAME))
    if os.path.lexists(os.path.join(game, OLD_FGD_NAME)):
        actions.append(("remove stale", os.path.join("syndicate", OLD_FGD_NAME), None))
    put(ADDON_README, os.path.join(ADDON_NAME, "readme.txt"))
    for sub in EDITOR_DIRS:
        src_dir = os.path.join(GAME_DIR, sub)
        source = tree_files(src_dir)
        for rel in source:
            put(os.path.join(src_dir, rel), os.path.join("syndicate", sub, rel))
        dest_dir = os.path.join(game, sub)
        if os.path.isdir(dest_dir):
            wanted = set(source)
            for rel in tree_files(dest_dir):
                if rel not in wanted:
                    actions.append(("remove", os.path.join("syndicate", sub, rel), None))
        old_dir = os.path.join(addon, sub)
        if os.path.isdir(old_dir):
            for rel in tree_files(old_dir):
                actions.append(("remove stale", os.path.join(ADDON_NAME, sub, rel), None))

    for verb, rel, _ in actions:
        print("%s%s: %s" % ("would " if args.check else "", verb, rel))
    if args.check:
        print("fgd sync --check: %s" % ("%d difference%s" % (len(actions), "" if len(actions) == 1 else "s")
                                        if actions else "in sync"))
        return 1 if actions else 0

    for verb, rel, src in actions:
        target = os.path.join(dest, rel)
        if src is None:
            os.remove(target)
        else:
            os.makedirs(os.path.dirname(target), exist_ok=True)
            if os.path.lexists(target):
                os.remove(target)
            shutil.copyfile(src, target)
    # directories left empty by removals (inside the mirrored folders and the old addon editor folders only)
    for top in [os.path.join(game, sub) for sub in EDITOR_DIRS] + [os.path.join(addon, sub) for sub in EDITOR_DIRS]:
        if os.path.isdir(top):
            for dirpath, dirs, names in os.walk(top, topdown=False):
                if not os.listdir(dirpath):
                    os.rmdir(dirpath)
    for parent in (os.path.join(addon, "models"), os.path.join(addon, "sprites")):
        if os.path.isdir(parent) and not os.listdir(parent):
            os.rmdir(parent)
    print("fgd sync: %d file%s changed in %s" % (len(actions), "" if len(actions) == 1 else "s", dest))
    return 0


def main():
    ap = argparse.ArgumentParser(description="SBO FGD: check against the game code, copy into a dist folder")
    sub = ap.add_subparsers(dest="cmd")
    sub.add_parser("check", help="compare sdk/syndicate.fgd with src/dlls and the editor assets")
    sp = sub.add_parser("sync", help="copy the FGD and the editor models and sprites into a dist folder")
    sp.add_argument("--check", action="store_true", help="report differences, write nothing")
    sp.add_argument("dir", help="dist build folder (contains syndicate/liblist.gam)")
    sub.add_parser("nohelper", help="list the classes without an editor model or sprite")
    args = ap.parse_args()
    if args.cmd == "check":
        return cmd_check(args)
    if args.cmd == "sync":
        return cmd_sync(args)
    if args.cmd == "nohelper":
        return cmd_nohelper(args)
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
