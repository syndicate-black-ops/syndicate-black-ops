"""Weather (docs/WEATHER.md "Server"): a map without env_rain / func_rain / env_snow / func_snow sends ReceiveW 0
with the HUD initialisation (connect, every respawn), so the client drops the previous map's weather (playtest
2026-10-17: the showcase's rain stayed on the next map). The suite's map has no weather entities."""

from etest import Suite

SUITE = Suite('weather', 'Weather: ReceiveW 0 on a map without weather', players=1)
S = SUITE
DOC = 'WEATHER.md#server'


@S.test('a respawn on a map without weather sends ReceiveW 0 to the player', players=1, doc=DOC)
def receivew_none(t):
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('ReceiveW', to='player1', min=1, fields={'weather': 0}, after='spawn')
