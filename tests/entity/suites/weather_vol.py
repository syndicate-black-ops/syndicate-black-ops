"""func_rainvolume / func_snowvolume (docs/WEATHER.md "Rain and snow volumes", sdk/syndicate.fgd): the "WeatherVol"
message decoded field by field: type (rain 1, snow 2, off 0), the client flags (each spawnflag the FGD offers, the
brush flag), intensity and update period with their defaults and limits, the box, the drop / flake keys; Start Active
and no name; toggling for everybody; the list with each HUD initialization. The drops drawn need the client layer."""

from etest import Suite

SUITE = Suite('weather_vol', 'Weather: rain and snow volumes (server side)', players=1)
S = SUITE
DOC = 'WEATHER.md#rain-and-snow-volumes'
BRUSH = 1 << 12      # the client's WEATHER_BRUSH_ENTITY, always set, with 1


def box(t, size, at):
    x, y, z = t._abs(at)
    # absmin / absmax: the compiler pads a brush model's bounds by a unit, and the engine widens a brush entity's box
    # by one more
    return [x - size[0] / 2 - 2, y - size[1] / 2 - 2, z - 2], [x + size[0] / 2 + 2, y + size[1] / 2 + 2, z + size[2] + 2]


@S.test('func_rainvolume without a name: on; keys left out: intensity 150, update period 0.3, the drop keys\' FGD '
        'defaults (width 2, height 80, speed 500 - 1800, life 1); the box; a respawn sends it', players=1, doc=DOC,
        covers=('func_rainvolume', 'func_rainvolume.intensity', 'func_rainvolume.update_period'))
def rain_defaults(t):
    t.brush('func_rainvolume', size=(128, 128, 128), at=(0, 0, 0), tex='AAATRIGGER')
    lo, hi = box(t, (128, 128, 128), (0, 0, 0))
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 1, 'flags': BRUSH | 1, 'intensity': 150, 'period': 0.3, 'mins': lo, 'maxs': hi,
                         'size1': 2, 'size2': 80, 'speed1': 500, 'speed2': 1800, 'life': 1})


@S.test('func_rainvolume: every rain key in the message (intensity, update period, drop width / height, min / max '
        'speed, life) and the flags 2 / 4 / 32 / 64 / 128 / 256 / 512 passed to the client', players=1, doc=DOC,
        covers=('func_rainvolume.intensity', 'func_rainvolume.update_period', 'func_rainvolume.raindrop_width',
                'func_rainvolume.raindrop_height', 'func_rainvolume.raindrop_min_speed',
                'func_rainvolume.raindrop_max_speed', 'func_rainvolume.raindrop_life', 'func_rainvolume:2',
                'func_rainvolume:4', 'func_rainvolume:32', 'func_rainvolume:64', 'func_rainvolume:128',
                'func_rainvolume:256', 'func_rainvolume:512'))
def rain_keys(t):
    t.brush('func_rainvolume', size=(128, 128, 128), at=(0, 0, 0), tex='AAATRIGGER', intensity='400',
            update_period='0.25', raindrop_width='3', raindrop_height='60', raindrop_min_speed='450',
            raindrop_max_speed='1500', raindrop_life='1.5', spawnflags=str(2 | 4 | 32 | 64 | 128 | 256 | 512))
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 1, 'flags': BRUSH | 1 | 2 | 4 | 32 | 64 | 128 | 256 | 512, 'intensity': 400,
                         'period': 0.25, 'size1': 3, 'size2': 60, 'speed1': 450, 'speed2': 1500, 'life': 1.5})


@S.test('func_snowvolume: snow (type 2), the default period 0.5, speeds, life; flags 2 / 4 / 256 / 512; intensity '
        'above 1000 gives 1000', players=1, doc=DOC,
        covers=('func_snowvolume', 'func_snowvolume.intensity', 'func_snowvolume.update_period',
                'func_snowvolume.snowflake_min_speed', 'func_snowvolume.snowflake_max_speed',
                'func_snowvolume.snowflake_life', 'func_snowvolume:2', 'func_snowvolume:4', 'func_snowvolume:256',
                'func_snowvolume:512'))
def snow_keys(t):
    t.brush('func_snowvolume', size=(128, 128, 128), at=(0, 0, 0), tex='AAATRIGGER', intensity='5000',
            snowflake_min_size='2.1', snowflake_max_size='2.6', snowflake_min_speed='131', snowflake_max_speed='210',
            snowflake_life='3', spawnflags=str(2 | 4 | 256 | 512))
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 2, 'flags': BRUSH | 1 | 2 | 4 | 256 | 512, 'intensity': 1000, 'period': 0.5,
                         'speed1': 131, 'speed2': 210, 'life': 3})


@S.test('func_snowvolume: the flake sizes are sent in tenths as given (FGD defaults 2.1 and 2.6: 21 and 26)', players=1,
        doc=DOC,
        covers=('func_snowvolume.snowflake_min_size', 'func_snowvolume.snowflake_max_size'))
def snow_sizes(t):
    t.brush('func_snowvolume', size=(128, 128, 128), at=(0, 0, 0), tex='AAATRIGGER', intensity='13',
            snowflake_min_size='2.1', snowflake_max_size='2.6')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn', fields={'intensity': 13, 'size1': 21, 'size2': 26})


@S.test('a name without flag 1: off in the list (type 0); with flag 1 Start Active: on; a trigger toggles it for '
        'everybody; On when on sends nothing', players=1, doc=DOC,
        covers=('func_rainvolume:1', 'func_rainvolume.targetname', 'func_snowvolume:1', 'func_snowvolume.targetname'))
def start_active_toggle(t):
    t.brush('func_rainvolume', targetname='r', size=(64, 64, 64), at=(-100, 0, 0), tex='AAATRIGGER', intensity='11')
    t.brush('func_snowvolume', targetname='s', size=(64, 64, 64), at=(100, 0, 0), tex='AAATRIGGER', intensity='12',
            spawnflags='1')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.mark('go')
    t.fire('r')
    t.fire('s')
    t.wait(0.2)
    t.mark('again')
    t.fire('r', use='on')
    t.wait(0.2)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn', within=(0, 0.45), fields={'type': 0, 'intensity': 11})
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn', within=(0, 0.45), fields={'type': 2, 'intensity': 12})
    t.expect.msg('WeatherVol', count=1, after='go', within=(0, 0.15), fields={'type': 1, 'intensity': 11})
    t.expect.msg('WeatherVol', count=1, after='go', within=(0, 0.15), fields={'type': 0, 'intensity': 12})
    t.expect.no_msg('WeatherVol', after='again')
