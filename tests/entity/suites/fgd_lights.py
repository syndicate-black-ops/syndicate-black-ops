"""Tier 5, final pass: "Custom Appearance" (pattern) on a named light_spot and light_environment (lights.cpp CLight:
a named light gets a switchable style 32-63 from the compiler, and its pattern is its own appearance). A
trigger_lightstyle holds another pattern for 1 s; then the light's own pattern comes back (CLight::SetStyle)."""

from etest import Suite

SUITE = Suite('fgd_lights', 'Tier 5: light_spot / light_environment custom appearance', players=1)
S = SUITE
DOC = 'LIGHT_STYLES.md'
NAMED = range(32, 64)


def pattern_case(cls, pattern):
    @S.test('%s "pattern" %s: held at zzz for 1 s by a trigger_lightstyle, its style goes back to %s' % (
                cls, pattern, pattern), players=1, doc=DOC, covers=('%s.pattern' % cls,))
    def case(t):
        kv = {'_light': '255 255 255 100', 'pattern': pattern}
        if cls == 'light_spot':
            kv.update(_cone='30', _cone2='45', pitch='-90')
        t.point(cls, targetname='lamp', at=(0, 0, 100), **kv)
        t.point('trigger_lightstyle', targetname='ls', target='lamp', pattern='zzz', m_iWait='1')
        t.mark('go')
        t.fire('ls')
        t.wait(2.0)
        t.expect.lightstyle('zzz', style=NAMED, count=1, after='go', within=(0, 0.05))
        t.expect.lightstyle(pattern, style=NAMED, count=1, after='go', within=(0.95, 1.2))
    case.__name__ = 'pattern_' + cls
    return case


pattern_case('light_spot', 'mno')
pattern_case('light_environment', 'opq')
