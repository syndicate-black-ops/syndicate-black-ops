"""XP for hits on monsters a script makes invulnerable (scripted_sequence "m_takeDamagePolicy" 1): the monsters whose
own TakeDamage gives the attacker XP give none for such a hit, and take no damage (fix/xp-script-invulnerable)."""

from etest import Suite

SUITE = S = Suite('xp_script', 'XP for hits on script-invulnerable monsters', players=1)
DOC = 'FEATUREFUL_OPTIONS.md'
MONSTERS = ('monster_human_grunt', 'monster_imp', 'monster_hunter', 'monster_imp_hunter', 'monster_cyberminotor',
            'monster_apache', 'monster_barney', 'monster_cyborg', 'monster_dark', 'dark_grunt', 'monster_gargantua',
            'monster_grunt_hunter', 'monster_human_assassin', 'monster_alien_slave', 'monster_scientist',
            'monster_zombie_grunt', 'monster_zombie', 'monster_zombie_hunter', 'monster_werewolf',
            'monster_werewolf_hunter')


@S.test('a player\'s hits on monsters held invulnerable by a scripted_sequence (m_takeDamagePolicy 1) give no XP and '
        'no damage; a hit on the same monster without the script gives XP', players=1, size=(512, 512, 256), doc=DOC,
        covers=('scripted_sequence.m_takeDamagePolicy=1',))
def no_xp_for_invulnerable(t):
    t.console('mp_XP 1')
    t.flags('player1', '+FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')
    for i, cls in enumerate(MONSTERS):
        x = -225 + 50 * ( i % 10 )
        y = 120 if i < 10 else 220
        t.point(cls, targetname='m%d' % i, at=(x, y, 0), angles='0 270 0', spawnflags='16')
        t.point('scripted_sequence', targetname='s%d' % i, at=(x, y, 0), angles='0 270 0', m_iszEntity='m%d' % i,
                m_fMoveTo='0', m_flRadius='512', m_iszIdle='idle1', m_takeDamagePolicy='1',
                names=('m_iszEntity', 'm_takeDamagePolicy'))
    t.point('monster_human_grunt', targetname='free', at=(0, -150, 0), angles='0 90 0', spawnflags='16')
    t.teleport('player1', (0, -250, 0), yaw=90)
    t.wait(2.0)
    for i in range(len(MONSTERS)):
        t.snap('a%d' % i, 'player1', 'xp')
        t.snap('a%d' % i, 'm%d' % i, 'health')
        t.damage('m%d' % i, 20, attacker='player1')
        t.wait(0.2)
        t.snap('b%d' % i, 'player1', 'xp')
        t.snap('b%d' % i, 'm%d' % i, 'health')
    t.snap('c', 'player1', 'xp')
    t.damage('free', 20, attacker='player1')
    t.wait(0.2)
    t.snap('d', 'player1', 'xp')
    for i in range(len(MONSTERS)):
        t.expect.same('a%d' % i, 'b%d' % i, 'player1', 'xp')
        t.expect.same('a%d' % i, 'b%d' % i, 'm%d' % i, 'health')
    t.expect.value('d', 'player1', 'xp', 100, op='>=')    # 20 damage x 8 on the grunt without a script
