"""GM mode (docs/GM_MODE.md "Ordering your monsters"): a Game Master selects the map's monsters too (click, box); they
take no orders, and Destroy selected kills them as any death does (their death trigger fires, nobody gets the kill).
His own monsters keep their orders and their destroy. Requested 2026-11-06."""

from etest import Suite

SUITE = Suite('gm_map_monsters', 'GM mode: the map\'s monsters can be selected and destroyed', players=2,
              requires=('GmMode_RoundStart',))
S = SUITE

GM_ENTRY_DEMON_SLAVE = '3'      # game_shared/gm_catalog.h: monster_hunter


def look(t, pitch, yaw):
    t.look('player1', pitch, yaw)
    t.move('player1', 0, 0, 3)
    t.wait(0.2)


def press(t, button):
    t.move('player1', 0, 0, 3, button)
    t.wait(0.3)
    t.move('player1', 0, 0, 3)
    t.wait(0.2)


@S.test('map monsters: selected and destroyed, no orders (mixed: only his own move); scripted one dies; furniture never',
        players=2, size=(512, 512, 256), doc='GM_MODE.md',
        covers=('monster_zombie_hunter', 'monster_zombie_hunter.TriggerCondition=4',
                'monster_zombie_hunter.TriggerTarget', 'monster_furniture'))
def map_monster(t):
    t.console('mp_gmmode 1')
    # the map's monster, its death trigger (AITRIGGER_DEATH)
    t.point('monster_zombie_hunter', at=(150, 150, 0), angles='0 180 0', targetname='mapmon', TriggerTarget='died',
            TriggerCondition='4', names=('TriggerTarget',))
    t.point('info_target', targetname='died')
    # map scenery right beside it, inside every box below: never selected, never destroyed
    t.point('monster_furniture', at=(150, 180, 0), angles='0 180 0', targetname='furn', model='models/zombie.mdl')
    # a map monster held in a scripted_sequence's idle (in its script when destroyed), behind the Game Master's view
    t.point('monster_zombie_hunter', at=(-230, -220, 0), angles='0 90 0', targetname='scrmon', TriggerTarget='died2',
            TriggerCondition='4', names=('TriggerTarget',))
    t.point('scripted_sequence', at=(-230, -200, 0), angles='0 90 0', targetname='seq', m_iszEntity='scrmon',
            m_iszIdle='slideidle', m_iszPlay='soda', m_fMoveTo='4', names=('m_iszEntity',))
    t.point('info_target', targetname='died2')
    t.wait(1.0)
    t.flags('player2', '+FL_NOTARGET')
    t.teleport('player2', (-220, -220, 0))
    t.cmd('player1', 'gmon')
    t.wait(0.3)
    t.cmd('player1', 'gm')
    t.wait(0.3)
    t.teleport('player1', (-200, 0, 28), yaw=0)

    # his own monster, placed on the floor ahead (it joins his selection)
    look(t, 30, 0)
    t.cmd('player1', 'gm_pick', GM_ENTRY_DEMON_SLAVE)
    t.wait(0.3)
    press(t, '+attack')
    t.cmd('player1', 'gm_cancel')
    t.wait(1.5)
    t.snap('placed', '@monster_hunter', 'origin', 'deadflag')
    t.snap('placed', 'mapmon', 'origin', 'deadflag', 'health')

    # a small box on the map's monster alone: selected, the orders menu without orders
    look(t, 5, 23)
    t.mark('alone')
    t.cmd('player1', 'gm_select', '-0.1', '-0.1', '0.1', '0.1', '1', '1')
    t.wait(0.3)
    t.expect.msg('ShowMenu', to='player1', fields={'title': '1 SELECTED'}, after='alone')
    # Alt Fire with only the map's monster selected: nothing moves
    look(t, 30, 23)
    press(t, '+attack2')
    t.wait(1.0)

    # a box over both: 2 selected, 1 takes orders; Alt Fire sends only his own
    look(t, 15, 12)
    t.mark('both')
    t.cmd('player1', 'gm_select', '-0.9', '-0.9', '0.9', '0.9', '1', '1')
    t.wait(0.3)
    t.expect.msg('ShowMenu', to='player1', fields={'title': '2 SELECTED, 1 TAKE ORDERS'}, after='both')
    look(t, 30, 90)
    press(t, '+attack2')
    t.wait(4.0)
    t.snap('ordered', '@monster_hunter', 'origin', 'deadflag')
    t.snap('ordered', 'mapmon', 'origin', 'deadflag')
    t.expect.moved('placed', 'ordered', '@monster_hunter', min_dist=32)
    t.expect.same('placed', 'ordered', 'mapmon', 'origin')

    # Destroy selected (orders menu 5): both die, the map's death trigger fires, no kill to anybody
    t.mark('destroy')
    t.cmd('player1', 'menuselect', '5')
    t.wait(0.5)
    t.expect.fired('died', after='destroy')
    t.expect.msg('GmNotice', to='player1', fields={'text': 'Destroyed 2'}, after='destroy')
    t.snap('gone', 'player1', 'frags')
    t.snap('gone', 'player2', 'frags')
    t.expect.value('gone', 'player1', 'frags', 0)
    t.expect.value('gone', 'player2', 'frags', 0)
    t.expect.removed('mapmon')
    t.snap('gone', 'furn', 'deadflag', 'health')
    t.expect.value('gone', 'furn', 'deadflag', 0)
    t.expect.present('furn')

    # the scripted one: in its script's idle, selected and destroyed: its death trigger fires, the script ends
    t.snap('script', 'scrmon', 'origin', 'deadflag', 'sequence')
    t.expect.value('script', 'scrmon', 'sequence', 25)     # zombie.mdl "slideidle": the script's idle
    look(t, 10, -95)
    t.mark('scripted')
    t.cmd('player1', 'gm_select', '-0.9', '-0.9', '0.9', '0.9', '1', '1')
    t.wait(0.3)
    t.expect.msg('ShowMenu', to='player1', fields={'title': '1 SELECTED'}, after='scripted')
    t.mark('destroy2')
    t.cmd('player1', 'menuselect', '5')
    t.wait(1.0)
    t.expect.fired('died2', after='destroy2')
    t.expect.msg('GmNotice', to='player1', fields={'text': 'Destroyed 1'}, after='destroy2')
    t.expect.removed('scrmon')
