"""Tests of scripts/fgd.py check: the code-to-FGD direction for spawnflags (missing_flag)."""
import os
import sys
import tempfile
import unittest

ROOT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..")
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import fgd  # noqa: E402

FGD_TEXT = r'''
@BaseClass = Appearflags [ spawnflags(flags) = [ 2048 : "Not in Deathmatch" : 0 ] ]
@BaseClass base(Appearflags) = Monster [ spawnflags(flags) = [ 1 : "WaitTillSeen" : 0 ] ]
@PointClass base(Monster) = monster_a : "A" [ spawnflags(flags) = [ 8 : "Own" : 0 ] ]
@PointClass base(Monster) = monster_b : "B" []
@PointClass = func_f : "F" [ amount(integer) : "Amount" : 0 dmg(integer) : "Damage" : 0 ]
'''

CODE = {
    "monsters.h": "#define SF_ONE 1\n#define SF_BIG ( 1 << 20 )\n#define SF_BOTH ( SF_ONE | 4 )\n",
    "base.cpp": """
class CBase : public CBaseEntity { public: virtual void Think( void ); void Helper( void ); virtual void Init( void ); };
void CBase :: Think( void ) { if ( pev->spawnflags & SF_BIG ) {} }
void CBase :: Helper( void ) { if ( FBitSet( pev->spawnflags, 16 ) ) {} }
void CBase :: Init( void ) { }
""",
    "a.cpp": """
class CA : public CBase { public: void Spawn( void ); };
void CA :: Spawn( void ) { Init(); if ( pev->spawnflags & SF_BOTH ) {} if ( pOther->pev->spawnflags & 64 ) {} pev->spawnflags |= 128; }
LINK_ENTITY_TO_CLASS( monster_a, CA );
""",
    "b.cpp": """
class CB : public CBase { public: void Spawn( void ); };
void CB :: Spawn( void ) { SetThink( &CBase::Helper ); }
LINK_ENTITY_TO_CLASS( monster_b, CB );
""",
    "f.cpp": """
class CF : public CBaseEntity { public: void Spawn( void ); void KeyValue( KeyValueData *pkvd ); void Use( void ); };
void CF :: Spawn( void ) { if ( pev->speed == 0 ) pev->speed = 100; pev->health = 5; if ( pev->health ) {} }
void CF :: KeyValue( KeyValueData *pkvd ) { if ( FStrEq( pkvd->szKeyName, "amount" ) ) pev->impulse = atoi( pkvd->szValue ); }
void CF :: Use( void ) { if ( pev->impulse ) {} if ( pev->dmg > 1 ) {} pev->frags = 3; }
LINK_ENTITY_TO_CLASS( func_f, CF );
""",
    "util.cpp": "TYPEDESCRIPTION gEntvarsDescription[] = { DEFINE_ENTITY_FIELD( classname, FIELD_STRING ), };\n",
}


def rules(**extra):
    out = {"tool_keys": {"keys": []}, "shared_code_classes": {"classes": []}, "not_for_mappers": [],
           "stock_assets": {"paths": []}, "left_out_flags": {}, "left_out_keys": {}}
    out.update(extra)
    return out


class FieldScan(unittest.TestCase):
    def test_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name, text in CODE.items():
                with open(os.path.join(tmp, name), "w") as fh:
                    fh.write(text)
            entities, _ = fgd.scan_code(tmp)
            # speed: Spawn reads it before setting it (a key); health: Spawn sets it first (its state); impulse:
            # KeyValue fills it from "amount"; dmg: read; frags: only written
            self.assertEqual(sorted(entities["func_f"]["fields"]), ["dmg", "speed"])
            path = os.path.join(tmp, "t.fgd")
            with open(path, "w") as fh:
                fh.write(FGD_TEXT)
            classes, _ = fgd.parse_fgd(path)
            got = [i for c, i, _ in fgd.find_problems(classes, [], entities, set(), rules()) if c == "missing_field"]
            self.assertEqual(got, ["func_f.speed"])


class FlagScan(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        for name, text in CODE.items():
            with open(os.path.join(self.tmp.name, name), "w") as fh:
                fh.write(text)
        path = os.path.join(self.tmp.name, "t.fgd")
        with open(path, "w") as fh:
            fh.write(FGD_TEXT)
        self.classes, _ = fgd.parse_fgd(path)

    def tearDown(self):
        self.tmp.cleanup()

    def flags(self, scopes=None):
        entities, _ = fgd.scan_code(self.tmp.name, scopes)
        return entities, {n: sorted(e["flags"]) for n, e in entities.items()}

    def test_operands(self):
        consts = {"A": 1, "B": 4}
        self.assertEqual(fgd._flag_operand("( A | B ) )", 0, consts), {1, 4})
        self.assertEqual(fgd._flag_operand("0x80000000 )", 0, consts), {0x80000000})
        self.assertEqual(fgd._flag_operand("(1<<20) && x", 0, consts), {1 << 20})
        self.assertIsNone(fgd._flag_operand("UNKNOWN )", 0, consts))

    def test_constants_resolve_through_names(self):
        self.assertEqual(fgd._constants(CODE)["SF_BOTH"], 5)

    def test_own_flags_only_reads(self):
        _, got = self.flags()
        # monster_a: its own 1 and 4, the base's virtual Think (1<<20); not another entity's 64, not the |= 128,
        # not CBase::Helper, which it never calls
        self.assertEqual(got["monster_a"], [1, 4, 1 << 20])
        # monster_b hands out &CBase::Helper: 16 counts
        self.assertEqual(got["monster_b"], [16, 1 << 20])

    def test_scope_needs_the_call(self):
        _, got = self.flags({"CBase": "Init"})
        self.assertEqual(got["monster_a"], [1, 4, 1 << 20])     # calls Init: CBase's virtuals count
        self.assertEqual(got["monster_b"], [16])                 # never calls Init: Think does not count

    def test_merged_base_flags(self):
        self.assertEqual(sorted(fgd.effective_flags(self.classes, "monster_a"), key=int), ["1", "8", "2048"])

    def problems(self, **extra):
        entities, entvars = fgd.scan_code(self.tmp.name)
        return sorted(i for c, i, _ in fgd.find_problems(self.classes, [], entities, entvars, rules(**extra))
                      if c == "missing_flag")

    def test_missing_flag(self):
        self.assertEqual(self.problems(), ["monster_a:1048576", "monster_a:4", "monster_b:1048576",
                                           "monster_b:16"])

    def test_left_out_on_a_base_exempts(self):
        self.assertEqual(self.problems(left_out_flags={"Monster:1048576": "set by code"}),
                         ["monster_a:4", "monster_b:16"])


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]], verbosity=1)
