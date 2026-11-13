"""meteo_particules (1.38's weather, removed in SBO 2.0; docs/WEATHER.md "meteo_particules"): it makes an unnamed
env_rain (iType not 0) or env_snow in its place, with no keys, so a 1.38 map gets CS's rain, always on; its 1.38 tuning
keys are ignored, and a developer console message names the replacement. One suite per map: the weather is
map-wide."""

from etest import Suite

SUITE = Suite('weather_meteo', 'Weather: meteo_particules (1.38) becomes env_rain', players=1)
S = SUITE
S.server_args = ('+developer', '1')    # the replacement's developer console message


@S.test('meteo_particules iType 1 (with a name and its 1.38 keys): a respawn sends ReceiveW 0, then CS\'s rain (150 '
        'per 0.3 s, width 2, height 80, speed 500 - 1800, life 1), always on; the developer console names the '
        'replacement', players=1,
        doc='WEATHER.md#meteo_particules-removed-in-sbo-20')
def meteo_rain(t):
    t.point('meteo_particules', targetname='m', at=(0, 0, 200), iType='1', iParticules='600', iSize='3',
            iVitesse='9', iHauteur='400')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('ReceiveW', to='player1', count=1, fields={'weather': 0}, after='spawn')
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 1, 'flags': 1, 'intensity': 150, 'period': 0.3, 'size1': 2, 'size2': 80,
                         'speed1': 500, 'speed2': 1800, 'life': 1})
    t.expect.console('was removed in SBO 2.0; using env_rain instead')
