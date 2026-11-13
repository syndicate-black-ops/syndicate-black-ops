"""env_rain / env_snow keys and flags (docs/WEATHER.md "Entities and keys", sdk/syndicate.fgd): the keys and
spawnflags of func_rainvolume / func_snowvolume on the whole-map weather, in the same WeatherVol message (without the
brush flag): every key and flag of the unnamed env_rain, named ones off at the start unless Start Active, switched for
everybody by a trigger, the intensity limit, and func_snow (the same class). The drops drawn need the client layer
(a game client run; this suite checks the server side only)."""

from etest import Suite

SUITE = Suite('weather_keys', 'Weather: env_rain / env_snow keys, flags and switching', players=1)
S = SUITE
DOC = 'WEATHER.md#entities-and-keys'
RAIN_FLAGS = 2 | 4 | 32 | 64 | 128 | 256 | 512
SNOW_FLAGS = 2 | 4 | 256 | 512


@S.test('env_rain without a name: every rain key (intensity, update period, drop width / height, min / max speed, '
        'life) and the flags 2 / 4 / 32 / 64 / 128 / 256 / 512 in its message, always on', players=1, doc=DOC,
        covers=('env_rain.intensity', 'env_rain.update_period', 'env_rain.raindrop_width', 'env_rain.raindrop_height',
                'env_rain.raindrop_min_speed', 'env_rain.raindrop_max_speed', 'env_rain.raindrop_life',
                'env_rain:2', 'env_rain:4', 'env_rain:32', 'env_rain:64', 'env_rain:128', 'env_rain:256',
                'env_rain:512'))
def rain_keys(t):
    t.point('env_rain', at=(0, 0, 200), intensity='400', update_period='0.25', raindrop_width='3',
            raindrop_height='60', raindrop_min_speed='450', raindrop_max_speed='1500', raindrop_life='1.5',
            spawnflags=str(RAIN_FLAGS))
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 1, 'flags': 1 | RAIN_FLAGS, 'intensity': 400, 'period': 0.25, 'size1': 3,
                         'size2': 60, 'speed1': 450, 'speed2': 1500, 'life': 1.5})


@S.test('env_snow with a name and Start Active: on at the start; every snow key (sizes in tenths) and the flags 2 / 4 '
        '/ 256 / 512; a trigger switches it off for everybody', players=1, doc=DOC,
        covers=('env_snow:1', 'env_snow.targetname', 'env_snow.intensity', 'env_snow.update_period',
                'env_snow.snowflake_min_size', 'env_snow.snowflake_max_size', 'env_snow.snowflake_min_speed',
                'env_snow.snowflake_max_speed', 'env_snow.snowflake_life', 'env_snow:2', 'env_snow:4',
                'env_snow:256', 'env_snow:512'))
def snow_named(t):
    t.point('env_snow', targetname='s1', at=(50, 0, 200), intensity='21', update_period='0.4',
            snowflake_min_size='3', snowflake_max_size='4.5', snowflake_min_speed='100', snowflake_max_speed='150',
            snowflake_life='5', spawnflags=str(1 | SNOW_FLAGS))
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.mark('go')
    t.fire('s1')
    t.wait(0.2)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn', within=(0, 0.45),
                 fields={'type': 2, 'flags': 1 | 1 | SNOW_FLAGS, 'intensity': 21, 'period': 0.4, 'size1': 30,
                         'size2': 45, 'speed1': 100, 'speed2': 150, 'life': 5})
    t.expect.msg('WeatherVol', count=1, after='go', within=(0, 0.15), fields={'type': 0, 'intensity': 21})


@S.test('env_rain with a name and no Start Active: off in the list (type 0), with Start Active: on; a trigger '
        'switches the first on for everybody, the next one off; On when on sends nothing', players=1, doc=DOC,
        covers=('env_rain:1', 'env_rain.targetname'))
def rain_named_toggle(t):
    t.point('env_rain', targetname='r1', at=(-50, 0, 200), intensity='11')
    t.point('env_rain', targetname='r2', at=(-100, 0, 200), intensity='12', spawnflags='1')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.mark('on')
    t.fire('r1')
    t.wait(0.2)
    t.mark('again')
    t.fire('r1', use='on')
    t.wait(0.2)
    t.mark('off')
    t.fire('r1')
    t.wait(0.2)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn', within=(0, 0.45),
                 fields={'type': 0, 'flags': 1, 'intensity': 11})
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn', within=(0, 0.45),
                 fields={'type': 1, 'flags': 1, 'intensity': 12})
    t.expect.msg('WeatherVol', count=1, after='on', within=(0, 0.15), fields={'type': 1, 'intensity': 11})
    t.expect.msg('WeatherVol', min=0, count=0, after='again', within=(0, 0.15), fields={'intensity': 11})
    t.expect.msg('WeatherVol', count=1, after='off', within=(0, 0.15), fields={'type': 0, 'intensity': 11})


@S.test('intensity above 1000 gives 1000; func_snow (CS\'s brush snow, the same class) with a name and Start Active '
        'sends its snow and keys too', players=1, doc=DOC)
def limit_func_snow(t):
    t.point('env_snow', targetname='s2', at=(100, 0, 200), intensity='5000', spawnflags='1')
    t.brush('func_snow', targetname='fs', size=(64, 64, 64), at=(150, 0, 0), tex='AAATRIGGER', intensity='13',
            snowflake_life='2', spawnflags='1')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn', fields={'type': 2, 'intensity': 1000})
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 2, 'flags': 1, 'intensity': 13, 'life': 2})
