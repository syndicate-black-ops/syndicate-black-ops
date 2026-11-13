"""Tier 5, batch 3: game_player_settings FGD items that no 1.38 map uses and no other test asserted
(docs/FEATUREFUL_GUIDE_ENTITIES.md): every weapon key with Remove / No change / Give / Give (allow duplicate), the
master, Allow OverHeal Yes, Allow armor above 100 No. A player_hasweapon probes the player's weapons."""

from etest import Suite

SUITE = Suite('fgd_settings', 'Tier 5: game_player_settings weapons, master, overheal / overcharge', players=1)
S = SUITE
DOC = 'FEATUREFUL_GUIDE_ENTITIES.md'
WEAPONS = ('weapon_ben', 'weapon_canon', 'weapon_chaingun', 'weapon_chainsaw', 'weapon_fa18', 'weapon_federal',
           'weapon_glauncher', 'weapon_handgrenade', 'weapon_k444', 'weapon_medikit', 'weapon_nm33', 'weapon_pm66',
           'weapon_rc28', 'weapon_rpg', 'weapon_satchel', 'weapon_snark', 'weapon_spa', 'weapon_sw', 'weapon_sword',
           'weapon_tripmine', 'weapon_warhammer', 'weapon_wong')


def info(t, name):
    return t.point('info_target', targetname=name)


def weapon_case(w):
    @S.test('game_player_settings %s: Remove (-1) takes it, No change (0) leaves it, Give (1) and Give (allow '
            'duplicate) (2) give it' % w, players=1, doc=DOC,
            covers=('game_player_settings.%s' % w,) + tuple('game_player_settings.%s=%d' % (w, v)
                                                            for v in (-1, 0, 1, 2)))
    def case(t):
        for name, v in (('rm', '-1'), ('none', '0'), ('give1', '1'), ('give2', '2')):
            t.point('game_player_settings', targetname=name, **{w: v})
        t.point('player_hasweapon', targetname='hw', weapon_name=w, pass_target='yes', fail_target='no')
        info(t, 'yes')
        info(t, 'no')
        expected = []

        def probe(label, has):
            t.wait(1.0)     # probes apart by more than a loaded frame (the windows widen by the frame length)
            t.mark(label)
            t.fire('hw', activator='player1')
            expected.append((label, has))

        t.fire('rm', activator='player1')
        probe('removed', False)
        t.fire('none', activator='player1')
        probe('still_without', False)
        t.fire('give1', activator='player1')
        probe('given1', True)
        t.fire('none', activator='player1')
        probe('still_with', True)
        t.fire('rm', activator='player1')
        t.wait(0.3)
        t.fire('give2', activator='player1')
        probe('given2', True)
        t.wait(0.1)
        for label, has in expected:
            t.expect.fired('yes' if has else 'no', activator='player1', count=1, after=label, within=(0, 0.3))
            t.expect.fired('no' if has else 'yes', count=0, after=label, within=(0, 0.3))
    case.__name__ = 'settings_' + w
    return case


for _w in WEAPONS:
    weapon_case(_w)


@S.test('game_player_settings master: nothing while the master is locked, applied once it passes', players=1,
        doc=DOC, covers=('game_player_settings.master',))
def settings_master(t):
    t.point('env_state', targetname='gate')
    t.point('game_player_settings', targetname='set', health='=42', master='gate')
    t.kv('player1', 'health', '90')
    t.fire('set', activator='player1')
    t.wait(0.1)
    t.snap('locked', 'player1', 'health')
    t.fire('gate', use='on')
    t.wait(0.1)
    t.fire('set', activator='player1')
    t.wait(0.1)
    t.snap('open', 'player1', 'health')
    t.expect.value('locked', 'player1', 'health', 90)
    t.expect.value('open', 'player1', 'health', 42)


@S.test('game_player_settings Allow OverHeal Yes: health set above max health stays', players=1, doc=DOC,
        covers=('game_player_settings.allow_overheal=1',))
def settings_overheal(t):
    t.point('game_player_settings', targetname='set', health='=150', allow_overheal='1')
    t.kv('player1', 'max_health', '100')
    t.kv('player1', 'health', '80')
    t.fire('set', activator='player1')
    t.wait(0.1)
    t.snap('s', 'player1', 'health', 'max_health')
    t.expect.value('s', 'player1', 'health', 150)
    t.expect.value('s', 'player1', 'max_health', 100)


@S.test('game_player_settings Allow armor above 100 No: armor set to 150 stops at 100', players=1, doc=DOC,
        covers=('game_player_settings.allow_overcharge=0', 'game_player_settings.armorvalue'))
def settings_overcharge(t):
    t.point('game_player_settings', targetname='set', armorvalue='=150', allow_overcharge='0')
    t.kv('player1', 'armorvalue', '10')
    t.fire('set', activator='player1')
    t.wait(0.1)
    t.snap('s', 'player1', 'armorvalue')
    t.expect.value('s', 'player1', 'armorvalue', 100)
