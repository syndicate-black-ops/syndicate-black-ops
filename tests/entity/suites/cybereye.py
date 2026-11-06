"""Cybereye (player.cpp PostThink): the red lights ahead of the view are sent every 0.1 s (unreliable, life 0.3 s),
not in every PostThink (playtest 2026-10-17: reliable lights of 0.1 s arrived in bursts and flashed in multiplayer).
A flash (any other ScreenFade) replaces the tint on the client; the tint is sent again when that fade ends.
Its own suite: it changes player1's loadout."""

from etest import Suite

SUITE = Suite('cybereye', 'Cybereye: lights throttled to 0.1 s, tint back after a flash', players=1)
S = SUITE


@S.test('cybereye on for 2 s: three temp-entity lights every 0.1 s (about 60), not three per frame', players=1)
def throttled(t):
    t.cmd('player1', 'cyberware', '6')     # cyberware 6: the cybereye
    t.console('test_round restart')        # respawn with the new loadout
    t.console('wait_for alive player1 20')
    t.wait(1.0)
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.cmd('player1', 'cybereye')
    t.mark('on')
    t.wait(2.0)
    t.cmd('player1', 'cybereye')
    t.expect.msg('svc:23', to='player1', min=30, max=66, after='on', within=(0, 2.0))


TINT = {'flags': 5, 'r': 64, 'g': 192, 'b': 128, 'a': 70}    # FFADE_OUT | FFADE_STAYOUT, player.cpp CybereyeTint


def cybereye_on(t):
    t.cmd('player1', 'cyberware', '6')     # cyberware 6: the cybereye
    t.console('test_round restart')        # respawn with the new loadout
    t.console('wait_for alive player1 20')
    t.wait(1.0)
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.cmd('player1', 'cybereye')
    t.wait(1.0)


@S.test('cybereye on, env_fade Fade From to the activator (1 s + 0.5 s hold): the flash plays out, then the tint is '
        'sent again once (a flash removed the tint)', players=1)
def tint_after_flash(t):
    t.point('env_fade', targetname='flash', spawnflags='5', rendercolor='255 255 255', renderamt='255',
            duration='1', holdtime='0.5')     # 1 Fade From + 4 activator only, as co_sbo_showcase's lightning
    cybereye_on(t)
    t.mark('flash')
    t.fire('flash', activator='player1')
    t.wait(2.5)
    t.cmd('player1', 'cybereye')
    t.expect.msg('ScreenFade', to='player1', count=1, after='flash', within=(0, 0.2),
                 fields={'flags': 0, 'r': 255, 'g': 255, 'b': 255, 'a': 255})
    t.expect.msg('ScreenFade', to='player1', count=0, fields=TINT, after='flash', within=(0, 1.4))
    t.expect.msg('ScreenFade', to='player1', count=1, fields=TINT, after='flash', within=(1.4, 1.8))
    t.expect.msg('ScreenFade', to='player1', count=1, fields=TINT, after='flash', within=(0, 2.4))


@S.test('cybereye turned off before an env_fade flash ends: no tint sent after the flash', players=1)
def no_tint_when_off(t):
    t.point('env_fade', targetname='flash', spawnflags='5', rendercolor='255 255 255', renderamt='255',
            duration='1', holdtime='0.5')
    cybereye_on(t)
    t.mark('flash')
    t.fire('flash', activator='player1')
    t.wait(0.5)
    t.cmd('player1', 'cybereye')           # off during the flash
    t.wait(1.5)
    t.expect.msg('ScreenFade', to='player1', count=0, fields=TINT, after='flash', within=(0, 1.9))
