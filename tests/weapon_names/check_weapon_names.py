"""Checks the console kill lines' weapon names (tests/weapon_names/run_test.sh)."""
import re
import sys
from pathlib import Path

ROOT = Path(sys.argv[1])
LANG = ROOT / "gamedir/syndicate/ui/resource/language/sbo_ui_english.txt"

# Half-Life weapons the game still links but no SBO map gives out (only the HLDM test maps), and the test layer's
HL_ONLY = {"357", "9mm", "9mmAR", "9mmhandgun", "crossbow", "crowbar", "egon", "gauss", "glock", "hornetgun",
           "mp5", "python", "shotgun", "test_hi"}

# What SBO weapons fire or set, sent as the weapon code when it kills (multiplay_gamerules.cpp
# SboKillfeed_InflictorCode, DeathNotice): the SW's plasma bolt, the rocket launcher's rocket, a thrown hand grenade,
# a contact grenade, satchels, mines, pitbulls, and the func_tank turrets a player mans
INFLICTOR_CODES = {"plasma", "rpg_rocket", "grenade", "glauncher", "satchel", "tripmine", "snark",
                   "tank", "tanklaser", "tankrocket", "tankmortar"}

failures = []

raw = LANG.read_bytes()
text = raw.decode("utf-16")
tokens = dict(re.findall(r'^"SBO_Weapon_([A-Za-z0-9_]+)"\s+"([^"]*)"', text, re.M))

weapons = set()
for src in (ROOT / "src/dlls").rglob("*.cpp"):
    for m in re.finditer(rb"LINK_ENTITY_TO_CLASS\(\s*weapon_([A-Za-z0-9_]+)", src.read_bytes()):
        weapons.add(m.group(1).decode())
if "wong" not in weapons:
    failures.append("positive control: weapon_wong not found in src/dlls")

for code in sorted((weapons - HL_ONLY) | INFLICTOR_CODES):
    name = tokens.get(code)
    if not name:
        failures.append(f"no #SBO_Weapon_{code} token: the console would print '{code}'")
    elif name == code:
        failures.append(f"#SBO_Weapon_{code} is the code itself")

if tokens.get("wong") != "Fists":
    failures.append(f"#SBO_Weapon_wong is {tokens.get('wong')!r}, expected 'Fists'")
if "Vindicio" in text:
    failures.append("'Vindicio' in sbo_ui_english.txt: the brand is Vindico")

for rel in ("src/cl_dll/death.cpp", "src/cl_dll/vgui2/sbo_bridge.cpp"):
    body = (ROOT / rel).read_bytes().decode("latin-1")
    if "VGui2_WeaponDisplayName(" not in body:
        failures.append(f"{rel}: the console kill line prints the weapon code without VGui2_WeaponDisplayName")

if failures:
    for f in failures:
        print("FAIL:", f)
    sys.exit(1)
print(f"PASS: {len((weapons - HL_ONLY) | INFLICTOR_CODES)} weapon codes named, both console lines use the names")
