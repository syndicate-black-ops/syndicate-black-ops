"""monster_apache (branch fix/apache, docs/APACHE.md): the "Fire Rockets" flag, monstermaker children that start
flying when they spawn, "Wait For Trigger"."""

from etest import Suite

SUITE = Suite('apache', 'monster_apache: rockets flag, monstermaker children, wait for trigger (fix/apache)',
              requires=('childspawnflags',))
S = SUITE
DOC = 'APACHE.md'
ROOM = (512, 512, 1024)
SF_WAITFORTRIGGER = 4
SF_START_INACTIVE = 64
SF_FIRE_ROCKETS = 32


def tough(t, player='player1'):
    """A test player the gun and rockets don't kill (a dead last player would end the co-op round)."""
    t.kv(player, 'health', '100000')
    t.kv(player, 'max_health', '100000')


def gunship(t, spawnflags=None, maker=False):
    """An apache (or a monstermaker making one) 600 up in one corner of the room, and a test player on a
    550-high pillar in the opposite corner, 410 units away and nearly level with it. Its rockets start 105 units to
    its side: closer to a wall they would start in it and blow up at once; a rocket that hits within ~375 units of
    it hurts it too (CGrenade::Explode clears the owner, as in HL). With no path the apache hovers, climbs slowly and turns, so its
    nose sweeps across the player: HL's rocket test wants it aimed within ~15 degrees. (Lower, its rockets start
    under the floor and blow it up; on the floor below it, the player is too steep for its nose.) It starts inactive
    and is used at the test's start: hovering in place it climbs into the ceiling in about 20 s, and every ceiling
    touch costs it health (FlyTouch), so one waiting through the earlier tests would be dead by its own."""
    if maker:
        t.point('monstermaker', at=(-140, 140, 600), targetname='maker', monstertype='monster_apache', monstercount='1',
                m_imaxlivechildren='1', netname='child', angles='0 315 0', childspawnflags=str(SF_FIRE_ROCKETS),
                names=('netname',))
    else:
        t.point('monster_apache', at=(-140, 140, 600), targetname='heli', angles='0 315 0',
                spawnflags=str(spawnflags | SF_START_INACTIVE))
    t.brush('func_wall', size=(128, 128, 550), at=(150, -150, 0))
    t.teleport('player1', (150, -150, 550), yaw=135)
    tough(t)
    if not maker:
        t.fire('heli', use='on')


@S.test('"Fire Rockets" (32): the apache fires hvr_rockets at a test player', players=1, size=ROOM,
        doc=DOC + '#fire-rockets')
def rockets_on(t):
    gunship(t, SF_FIRE_ROCKETS)
    t.wait(12)
    t.expect.spawned('@hvr_rocket', min=1)
    t.expect.sound('rocket1.wav', ent='@hvr_rocket')


@S.test('without "Fire Rockets" the apache never fires a rocket (chaingun only, as 1.38)', players=1, size=ROOM,
        doc=DOC + '#fire-rockets')
def rockets_off(t):
    gunship(t, 0)
    t.wait(12)
    t.expect.spawned('@hvr_rocket', count=0)
    t.expect.sound('gat_fire1.wav', ent='heli', min=1)     # its chaingun fired: it saw and hunted the player


def maker(t, **keys):
    kv = dict(monstercount='1', m_imaxlivechildren='1')
    kv.update(keys)
    t.point('monstermaker', at=(0, 0, 400), targetname='maker', monstertype='monster_apache', netname='child',
            names=('netname',), **kv)
    t.point('info_target', at=(0, 0, 400), targetname='spot')


@S.test('a monstermaker apache flies and thinks right after it spawns', size=ROOM, doc=DOC + '#monstermaker')
def maker_child_flies(t):
    maker(t)
    t.fire('maker')
    t.wait(0.1)
    t.snap('born', 'child', 'origin', 'nextthink', 'spawnflags')
    t.snap('born', 'spot', 'origin')
    t.wait(1.0)
    t.snap('later', 'child', 'origin', 'angles', 'nextthink')
    t.snap('later', 'spot', 'origin')
    t.expect.spawned('@monster_apache', count=1)
    t.expect.distance('later', 'child', 'spot', 2, 2000, flat=False)    # it moved in its first second


@S.test('a monstermaker apache with "Fire Rockets" in childspawnflags fires rockets', players=1, size=ROOM,
        doc=DOC + '#monstermaker')
def maker_child_rockets(t):
    gunship(t, maker=True)
    t.fire('maker')
    t.wait(0.5)
    t.snap('s', 'child', 'spawnflags')
    t.wait(12)
    t.expect.value('s', 'child', 'spawnflags', SF_FIRE_ROCKETS - 2 ** 31, tol=0)   # + the maker's SF_MONSTER_FALL_TO_GROUND
    t.expect.spawned('@hvr_rocket', min=1)


@S.test('a monstermaker with "MonsterClip" (8): its apache still flies (bit 4 is not "Wait For Trigger" for it)',
        size=ROOM, doc=DOC + '#monstermaker')
def maker_monsterclip(t):
    maker(t, spawnflags='8')
    t.fire('maker')
    t.wait(1.1)
    t.snap('later', 'child', 'origin')
    t.snap('later', 'spot', 'origin')
    t.expect.distance('later', 'child', 'spot', 2, 2000, flat=False)


@S.test('"Wait For Trigger" (4): the apache stays where it is until used, then flies', size=ROOM,
        doc=DOC + '#wait-for-trigger')
def wait_for_trigger(t):
    t.point('monster_apache', at=(0, 0, 400), targetname='heli', angles='0 90 0', spawnflags=str(SF_WAITFORTRIGGER))
    t.point('info_target', at=(0, 0, 400), targetname='spot')
    t.wait(3)
    t.snap('idle', 'heli', 'origin', 'velocity')
    t.snap('idle', 'spot', 'origin')
    t.mark('go')
    t.fire('heli', use='on')
    t.wait(1.5)
    t.snap('flying', 'heli', 'origin')
    t.snap('flying', 'spot', 'origin')
    t.expect.used('heli', after='go', min=1)
    t.expect.distance('idle', 'heli', 'spot', 0, 0.5, flat=False)
    t.expect.distance('flying', 'heli', 'spot', 2, 2000, flat=False)


@S.test('a monstermaker apache with childspawnflags 64 ("Start Inactive") waits until its name is fired', size=ROOM,
        doc=DOC + '#monstermaker')
def maker_child_waits(t):
    maker(t, childspawnflags=str(SF_START_INACTIVE))
    t.fire('maker')
    t.wait(2)
    t.snap('idle', 'child', 'origin')
    t.snap('idle', 'spot', 'origin')
    t.fire('child', use='on')
    t.wait(1.5)
    t.snap('flying', 'child', 'origin')
    t.snap('flying', 'spot', 'origin')
    t.expect.distance('idle', 'child', 'spot', 0, 0.5, flat=False)
    t.expect.distance('flying', 'child', 'spot', 2, 2000, flat=False)


@S.test('a monstermaker with "Max live children" 1 spawns its next apache once the first is shot down', size=ROOM,
        doc=DOC + '#monstermaker')
def maker_next_after_death(t):
    # HL SDK bug: CApache::Killed never sent the maker its DeathNotice, so the slot stayed taken forever
    maker(t, monstercount='2', delay='1')
    t.fire('maker', use='on')
    t.wait(1.5)
    t.mark('kill')
    t.damage('child', 1000)
    t.wait(12)
    t.expect.spawned('@monster_apache', count=2)


@S.test('AI trigger "Take Damage" (TriggerCondition 2) fires TriggerTarget when the apache is hurt', size=ROOM,
        doc=DOC)
def ai_trigger_take_damage(t):
    # halflife-updated (halflife #3262): CApache::TakeDamage sets the damage conditions FCheckAITrigger reads
    t.point('monster_apache', at=(0, 0, 400), targetname='heli', spawnflags=str(SF_START_INACTIVE),
            TriggerCondition='2', TriggerTarget='hurt', names=('TriggerTarget',))
    t.point('info_target', targetname='hurt')
    t.wait(1.0)
    t.mark('hit')
    t.damage('heli', 10)
    t.wait(1.5)                 # NullThink checks the AI trigger every 0.5 s
    t.expect.fired('hurt', count=1)                 # once, and not before the hit:
    t.expect.fired('hurt', after='hit', count=1)
