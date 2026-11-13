"""Featureful button keys (sdk/syndicate.fgd) on func_rot_button (options checks them on func_button) and every
"Use type" choice on both: trigger_before_move, trigger_on_return, target_on_locked, usetype -1 / 0 / 1 / 2 / 3,
wait_toggle."""

from etest import Suite

SUITE = Suite('button_keys', 'Button keys: use type, before move / on return / on locked targets, wait before toggling',
              players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
USE_OFF, USE_ON, USE_TOGGLE = 0, 1, 3


def info(t, name):
    t.point('info_target', targetname=name)


def button(t, cls, name, x, **kv):
    kv.setdefault('speed', '400')
    if cls == 'func_rot_button':
        return t.brush(cls, targetname=name, size=(16, 32, 32), at=(x, 0, 40), origin=True, distance='30', **kv)
    return t.brush(cls, targetname=name, size=(16, 32, 32), at=(x, 0, 40), **kv)


@S.test('func_rot_button: trigger_before_move when it starts in, trigger_on_return when it is back out, usetype 1 (its '
        'target gets On); locked by its master: target_on_locked fires (once within 2 s) and nothing else', players=1,
        doc=DOC, covers=('func_rot_button', 'func_rot_button.trigger_before_move', 'func_rot_button.trigger_on_return',
                         'func_rot_button.usetype=1', 'func_rot_button.target_on_locked'))
def rot_options(t):
    for n in ('bmove', 'bret', 'btgt', 'block', 'b2tgt'):
        info(t, n)
    t.point('env_global', targetname='closed', globalstate='tier3_closed', initialstate='0', spawnflags='5')
    button(t, 'func_rot_button', 'bt', -100, target='btgt', wait='0.3', trigger_before_move='bmove',
           trigger_on_return='bret', usetype='1', names=('trigger_before_move', 'trigger_on_return'))
    button(t, 'func_rot_button', 'bl', 100, target='b2tgt', wait='0.3', master='closed', target_on_locked='block',
           names=('target_on_locked', 'master'))
    t.wait(0.2)
    t.mark('go')
    t.fire('bt', activator='player1')
    t.fire('bl', activator='player1')
    t.wait(0.1)
    t.fire('bl', activator='player1')
    t.wait(1.0)
    t.expect.used('bmove', count=1, after='go')
    t.expect.used('btgt', use=USE_ON, count=1, after='go')
    t.expect.used('bret', count=1, after='go')
    t.expect.order('bmove', 'bret')
    t.expect.used('block', count=1, after='go')
    t.expect.used('b2tgt', count=0, after='go')


def usetype_test(cls):
    def f(t):
        rows = [('m1', '-1', '0', USE_OFF), ('z0', '0', '0', USE_TOGGLE), ('p1', '1', '0', USE_ON)]
        for i, (n, ut, sf, _u) in enumerate(rows):
            info(t, n + '_t')
            button(t, cls, n, -150 + 150 * i, target=n + '_t', usetype=ut, wait='0.2')
        t.mark('go')
        for n, *_r in rows:
            t.fire(n, activator='player1')
        t.wait(0.6)
        for n, _ut, _sf, use in rows:
            t.expect.used(n + '_t', use=use, count=1, after='go')
    f.__name__ = 'usetype_' + cls
    S.test('%s usetype -1 Off, 0 Toggle, 1 On: the target gets that use type' % cls, players=1, doc=DOC,
           covers=(cls, cls + '.usetype=-1', cls + '.usetype=0', cls + '.usetype=1'))(f)

    def g(t):
        info(t, 'a_t')
        info(t, 'b_t')
        button(t, cls, 'a', -100, target='a_t', spawnflags='32', usetype='2', wait_toggle='1')
        button(t, cls, 'b', 100, target='b_t', spawnflags='32', usetype='3')
        t.mark('in')
        t.fire('a', activator='player1')
        t.fire('b', activator='player1')
        t.wait(0.3)
        t.fire('a', activator='player1')        # within wait_toggle: ignored
        t.wait(1.0)
        t.mark('out')
        t.fire('a', activator='player1')
        t.fire('b', activator='player1')
        t.wait(0.4)
        # the use within wait_toggle adds nothing: one On going in, one Off coming out, nothing else
        t.expect.used('a_t', count=2, after='in')
        t.expect.used('a_t', use=USE_ON, count=1, after='in')
        t.expect.used('a_t', use=USE_OFF, count=1, after='out')
        t.expect.used('b_t', use=USE_OFF, count=1, after='in', within=(0, 0.3))
        t.expect.used('b_t', use=USE_ON, count=1, after='out')
    g.__name__ = 'toggle_' + cls
    S.test('%s Toggle (32) usetype 2 On in / Off out and 3 Off in / On out; wait_toggle 1: a second use within 1 s does '
           'nothing' % cls, players=1, doc=DOC,
           covers=(cls, cls + ':32', cls + '.usetype=2', cls + '.usetype=3', cls + '.wait_toggle'))(g)


for _c in ('func_button', 'func_rot_button'):
    usetype_test(_c)
