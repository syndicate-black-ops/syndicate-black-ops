"""Which entity classes have their own code for a shared FGD key (the coverage gate, etest/coverage.py).

A key of a shared FGD base (Monster, TalkMonster: tests/fgd_code_paths.json) is handled once in the base's C++ class
(CBaseMonster::KeyValue stores it; TakeDamage, Killed, RunAI, ... act on it). A test on one monster proves that shared
code. It does not prove the key on a class whose C++ class overrides a method the key goes through: CBaseTurret
overrides TakeDamage and never calls CBaseMonster::Killed, so TriggerCondition "Death" never fired on turrets while a
test on an alien slave passed. This module reads src/dlls (LINK_ENTITY_TO_CLASS, class declarations and method
bodies) and says, per class and key, which of those methods the class (or a class between it and the base) replaces
(its override does not call an ancestor's version) or extends (it calls it, but runs its own code too).

`python3 tests/entity/etest/code_paths.py [class ...]` prints the overrides per class.
"""

import glob
import json
import os
import re

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
PATHS_JSON = os.path.join(ROOT, 'tests', 'fgd_code_paths.json')

_LINK = re.compile(r'^\s*LINK_ENTITY_TO_CLASS\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)', re.M)
_CLASS = re.compile(r'\b(?:class|struct)\s+(\w+)\s*:\s*(?:public|protected|private)?\s*(\w+)[^;{]*\{')
_DEF = re.compile(r'\b(\w+)\s*::\s*(\w+)\s*\(')


def _strip(text):
    """The source without comments and string literals (same length per line is not needed)."""
    text = re.sub(r'//[^\n]*', '', text)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    return re.sub(r'"(?:\\.|[^"\\\n])*"', '""', text)


def _block(text, i):
    """The text of the {...} block whose '{' is at text[i] (without the braces), and the index after it."""
    depth, j = 0, i
    while j < len(text):
        c = text[j]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return text[i + 1:j], j + 1
        j += 1
    return text[i + 1:], len(text)


class Source:
    """The game DLL's classes: map class -> C++ class, parents, declared methods and method bodies."""

    def __init__(self, src_dir=None):
        src_dir = src_dir or os.path.join(ROOT, 'src', 'dlls')
        self.link, self.parent, self.declared, self.bodies = {}, {}, {}, {}
        files = sorted(glob.glob(os.path.join(src_dir, '**', '*.cpp'), recursive=True)
                       + glob.glob(os.path.join(src_dir, '**', '*.h'), recursive=True))
        for path in files:
            text = _strip(open(path, encoding='latin-1').read())
            for m in _LINK.finditer(text):
                self.link.setdefault(m.group(1), m.group(2))
            for m in _CLASS.finditer(text):
                name, base = m.group(1), m.group(2)
                body, _ = _block(text, m.end() - 1)
                self.parent.setdefault(name, base)
                decl = self.declared.setdefault(name, set())
                # declarations at the class body's own level (inline bodies are deeper)
                depth, flat = 0, []
                for c in body:
                    if c == '{':
                        depth += 1
                    elif c == '}':
                        depth -= 1
                    elif depth == 0:
                        flat.append(c)
                for d in re.finditer(r'(~?\w+)\s*\(', ''.join(flat)):
                    decl.add(d.group(1))
                # inline method bodies: "Name( ... ) [const] {"
                for d in re.finditer(r'\b(\w+)\s*\([^;{}()]*(?:\([^()]*\)[^;{}()]*)*\)\s*(?:const\s*)?\{', body):
                    b, _ = _block(body, d.end() - 1)
                    self.bodies.setdefault((name, d.group(1)), b)
            # out-of-line definitions: "Class :: Method ( ... ) {"
            for m in re.finditer(r'\b(\w+)\s*::\s*(~?\w+)\s*\(', text):
                k = text.find('{', m.end())
                semi = text.find(';', m.end())
                if k < 0 or (0 <= semi < k):
                    continue
                if re.search(r'[=]', text[m.end():k]) and ')' not in text[m.end():k]:
                    continue
                b, _ = _block(text, k)
                self.bodies[(m.group(1), m.group(2))] = b

    def chain(self, cpp, stop):
        """cpp and its parents up to (not including) `stop`; None when `stop` is not an ancestor."""
        out, c, seen = [], cpp, set()
        while c and c != stop and c not in seen:
            seen.add(c)
            out.append(c)
            c = self.parent.get(c)
        return out if c == stop else None

    def ancestors(self, cpp):
        out, c = [], self.parent.get(cpp)
        while c and c not in out:
            out.append(c)
            c = self.parent.get(c)
        return out

    def calls(self, cls, meth, name):
        """The body of cls::meth calls `name` (any qualifier)."""
        b = self.bodies.get((cls, meth))
        return bool(b and re.search(r'\b%s\s*\(' % re.escape(name), b))

    def calls_name(self, cls, meth, name):
        """The body of cls::meth uses the identifier `name`."""
        b = self.bodies.get((cls, meth))
        return bool(b and re.search(r'\b%s\b' % re.escape(name), b))

    def calls_base(self, cls, meth):
        """The body of cls::meth calls an ancestor's meth (Base::meth(...))."""
        b = self.bodies.get((cls, meth))
        if b is None:
            return False
        anc = set(self.ancestors(cls)) | {'BaseClass'}
        return any(q in anc for q, m in _DEF.findall(b) if m == meth)

    def overrides(self, cpp, base_cpp, methods, needs_call=(), replace_only=(), uses=(), own_cases=None):
        """[(class, method, 'replaces' | 'extends' | 'handles <case>' | 'never calls')] for the classes between cpp
        and base_cpp. needs_call: methods of the base the class must reach (e.g. MonsterInit): 'never calls' when no
        class in the chain (below the base) has a body that calls it. own_cases: per method, the case labels whose
        handling in an override that calls the ancestor's still counts as its own code (a GetSchedule with its own
        "case MONSTERSTATE_ALERT:" decides that state itself, before the base's code where the key is read)."""
        chain = self.chain(cpp, base_cpp)
        if chain is None:
            return None
        out = []
        for c in chain:
            for m in methods:
                if m in self.declared.get(c, ()):
                    how = 'extends' if self.calls_base(c, m) or any(self.calls_name(c, m, u) for u in uses) \
                        else 'replaces'
                    if how == 'extends' and own_cases and m in own_cases:
                        body = self.bodies.get((c, m), '')
                        hit = [case for case in own_cases[m] if re.search(r'\bcase\s+%s\s*:' % case, body)]
                        if hit:
                            how = 'handles ' + ' / '.join(hit)
                    if how == 'extends' and m in replace_only:
                        continue
                    out.append((c, m, how))
        for m in needs_call:
            if not any(self.calls(c, b, m) for c in chain for (cc, b) in self.bodies if cc == c):
                out.append((chain[0], m, 'never calls'))
        return out


class Paths:
    """tests/fgd_code_paths.json: per shared FGD base, its C++ class and the methods each key goes through."""

    def __init__(self, path=PATHS_JSON, source=None):
        self.data = json.load(open(path)) if os.path.exists(path) else {'bases': {}}
        self.bases = self.data.get('bases', {})
        self._source = source
        self._cache = {}

    def base_for(self, fgd_base):
        """The shared base an FGD base class belongs to: itself, or the base that lists it in "parts" (the FGD's
        Monster is built from MonsterCore, MonsterTriggers, ...); None when it is none of them."""
        if fgd_base in self.bases:
            return fgd_base
        for name, b in self.bases.items():
            if fgd_base in b.get('parts', ()):
                return name
        return None

    @property
    def source(self):
        if self._source is None:
            self._source = Source()
        return self._source

    def methods(self, base, key, value=None):
        b = self.bases[base]
        keys = {k.lower(): v for k, v in b['keys'].items()}
        if value is not None and '%s=%s' % (key, value) in keys:
            m = keys['%s=%s' % (key, value)]
        else:
            m = keys.get(key, [])
        return list(dict.fromkeys(list(b.get('every_key', [])) + list(m)))

    def own_code(self, base, classname, key, value=None):
        """[(class, method, how)] the class has of its own on the key's path; [] none; None: not known (no
        LINK_ENTITY_TO_CLASS, or its C++ class is not under the base's)."""
        k = (base, classname, key, value)
        if k not in self._cache:
            b = self.bases[base]
            cpp = self.source.link.get(classname)
            if not cpp:
                self._cache[k] = None
            else:
                meths = self.methods(base, key, value)
                needs = [m for m in b.get('needs_call', {}) if set(b['needs_call'][m]) & set(meths)]
                uses = {u.lower(): v for u, v in b.get('uses', {}).items()}.get(key, ())
                cases = {u.lower(): v for u, v in b.get('own_cases', {}).items()}.get(key)
                self._cache[k] = self.source.overrides(cpp, b['cpp'], meths, needs, b.get('every_key', ()), uses,
                                                       cases)
        return self._cache[k]


def describe(own):
    return ', '.join('%s::%s %s' % (c, m, how) if how != 'never calls' else '%s never calls %s' % (c, m)
                     for c, m, how in own)


if __name__ == '__main__':
    import sys
    sys.path.insert(0, os.path.join(ROOT, 'scripts'))
    import fgd
    classes, _ = fgd.parse_fgd(os.path.join(ROOT, 'sdk', 'syndicate.fgd'))
    paths = Paths()
    want = set(sys.argv[1:])
    for base, b in sorted(paths.bases.items()):
        for cn in sorted(c for c, v in classes.items() if v['kind'] != 'baseclass' and base in fgd.base_chain(classes, c)):
            if want and cn not in want:
                continue
            for key in sorted(b['keys']):
                own = paths.own_code(base, cn, key.lower())
                if own is None:
                    print('%s %s.%s: C++ class unknown' % (base, cn, key))
                elif own:
                    print('%s %s.%s: %s' % (base, cn, key, describe(own)))
