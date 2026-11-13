"""env_rain (docs/WEATHER.md "Server", sdk/syndicate.fgd): the map's unnamed env_rain with no keys sends what the CS
weather draws, with each HUD initialization: ReceiveW 0 (the whole-map weather the client had goes), then a WeatherVol
of type 1 (rain) without the brush flag, with CS's rain (150 drops every 0.3 s, 2 x 80 units, 500 - 1800 units/s, 1 s).
With an unnamed env_snow too, rain only; a second unnamed env_rain sends nothing. One suite per map: the unnamed
weather is map-wide. The rain drawn around the view needs the client layer."""

from etest import Suite

SUITE = Suite('weather_rain', 'Weather: env_rain (and env_snow) on the map', players=1)
S = SUITE
DOC = 'WEATHER.md#server'


@S.test('env_rain without a name and keys (with an env_snow and a second env_rain too): a respawn sends ReceiveW 0, '
        'then one rain with CS\'s settings (150 per 0.3 s, width 2, height 80, speed 500 - 1800, life 1), always on; '
        'no snow, nothing for the second one', players=1, doc=DOC,
        covers=('env_rain', 'env_rain.intensity', 'env_rain.update_period', 'env_rain.raindrop_width',
                'env_rain.raindrop_height', 'env_rain.raindrop_min_speed', 'env_rain.raindrop_max_speed',
                'env_rain.raindrop_life'))
def rain(t):
    t.point('env_rain', at=(0, 0, 200))
    t.point('env_snow', at=(50, 0, 200))
    t.point('env_rain', at=(-50, 0, 200), intensity='77')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('ReceiveW', to='player1', count=1, fields={'weather': 0}, after='spawn')
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 1, 'flags': 1, 'intensity': 150, 'period': 0.3, 'size1': 2, 'size2': 80,
                         'speed1': 500, 'speed2': 1800, 'life': 1})
    t.expect.msg('WeatherVol', min=0, count=0, after='spawn', fields={'type': 2})
    t.expect.msg('WeatherVol', min=0, count=0, after='spawn', fields={'intensity': 77})
