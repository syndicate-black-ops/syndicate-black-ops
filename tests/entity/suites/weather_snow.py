"""env_snow (docs/WEATHER.md "Server", sdk/syndicate.fgd): a map with an unnamed env_snow with no keys (and no rain)
sends ReceiveW 0, then a WeatherVol of type 2 (snow) without the brush flag, with CS's snow (512 flakes every 0.5 s,
size 2.1 - 2.6, 131 - 210 units/s, 3 s), with each HUD initialization. One suite per map: the weather is map-wide. The
snow drawn needs the client layer."""

from etest import Suite

SUITE = Suite('weather_snow', 'Weather: env_snow on the map', players=1)
S = SUITE


@S.test('env_snow without a name and keys: a respawn sends ReceiveW 0, then snow with CS\'s settings (512 per 0.5 s, '
        'size 2.1 - 2.6, speed 131 - 210, life 3), always on', players=1, doc='WEATHER.md#server',
        covers=('env_snow', 'env_snow.intensity', 'env_snow.update_period', 'env_snow.snowflake_min_size',
                'env_snow.snowflake_max_size', 'env_snow.snowflake_min_speed', 'env_snow.snowflake_max_speed',
                'env_snow.snowflake_life'))
def snow(t):
    t.point('env_snow', at=(0, 0, 200))
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('ReceiveW', to='player1', count=1, fields={'weather': 0}, after='spawn')
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 2, 'flags': 1, 'intensity': 512, 'period': 0.5, 'size1': 21, 'size2': 26,
                         'speed1': 131, 'speed2': 210, 'life': 3})
