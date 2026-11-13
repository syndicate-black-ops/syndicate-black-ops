"""Weather wind keys and run-time key changes (docs/WEATHER.md "Wind" and "Changing keys while the map runs",
sdk/syndicate.fgd): wind_x / wind_y (a fixed wind: flag 8192 in WeatherVol and the two speeds) and wind_rand_x /
wind_rand_y (the spread) on env_rain, env_snow, func_rainvolume and func_snowvolume, Spirit of Half-Life's key names
as aliases, all left out = the message as before (no flag, zeros); a trigger_changevalue / trigger_copyvalue writing a
key or the spawnflags sends the weather again to every client with Spawn's defaults and limits, and the saved fields
keep the new values. The drops drawn need the client layer (a game client run; this suite checks the server side only)."""

from etest import Suite

SUITE = Suite('weather_wind', 'Weather: wind keys and keys changed at run time', players=2)
S = SUITE
DOC = 'WEATHER.md#wind'
LIVE = 'WEATHER.md#changing-keys-while-the-map-runs'
FIXED = 1 << 13     # the client's WEATHER_FIXED_WIND
BRUSH = 1 << 12     # the client's WEATHER_BRUSH_ENTITY
WIND_KEYS = ('wind_x', 'wind_y', 'wind_rand_x', 'wind_rand_y')


def covers(*classes):
    return tuple('%s.%s' % (c, k) for c in classes for k in WIND_KEYS)


@S.test('env_rain: Wind X 100 / Wind Y 200 (the Spirit wiki\'s oblique rain) and spreads 30 / 40: the fixed-wind flag '
        'and the four values; env_snow: Wind X -50 (a negative wind), Wind Y 0, the flag; a spread of -25 is 25',
        players=2, doc=DOC, covers=covers('env_rain', 'env_snow'))
def wind_keys(t):
    t.point('env_rain', targetname='r', at=(0, 0, 200), spawnflags='1', intensity='31', wind_x='100', wind_y='200',
            wind_rand_x='30', wind_rand_y='40')
    t.point('env_snow', targetname='s', at=(50, 0, 200), spawnflags='1', intensity='32', wind_x='-50', wind_y='0',
            wind_rand_x='0', wind_rand_y='-25')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 1, 'flags': FIXED | 1, 'intensity': 31, 'wind_x': 100, 'wind_y': 200, 'rand_x': 30,
                         'rand_y': 40})
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 2, 'flags': FIXED | 1, 'intensity': 32, 'wind_x': -50, 'wind_y': 0, 'rand_x': 0,
                         'rand_y': 25})


@S.test('no wind keys: the message as before (no fixed-wind flag, wind and spread 0); a spread alone keeps the random '
        'wind (no flag) and sends the spread; "Not affected by wind" (4) is sent with the wind keys', players=2,
        doc=DOC)
def wind_empty(t):
    t.point('env_rain', targetname='plain', at=(-50, 0, 200), spawnflags='1', intensity='41')
    t.point('env_rain', targetname='spread', at=(-100, 0, 200), spawnflags='1', intensity='42', wind_rand_x='60')
    t.point('env_snow', targetname='calm', at=(-150, 0, 200), spawnflags=str(1 | 4), intensity='43', wind_y='70')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 1, 'flags': 1, 'intensity': 41, 'period': 0.3, 'size1': 2, 'size2': 80,
                         'speed1': 500, 'speed2': 1800, 'life': 1, 'wind_x': 0, 'wind_y': 0, 'rand_x': 0, 'rand_y': 0})
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 1, 'flags': 1, 'intensity': 42, 'wind_x': 0, 'rand_x': 60})
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 2, 'flags': FIXED | 1 | 4, 'intensity': 43, 'wind_y': 70})


@S.test('func_rainvolume and func_snowvolume: the brush flag, the fixed-wind flag and the values; an env_rain with '
        'Spirit\'s key names (m_flWindX / m_flWindY / m_flRandX / m_flRandY) sends the same', players=2, doc=DOC,
        covers=covers('func_rainvolume', 'func_snowvolume'))
def wind_volumes(t):
    t.brush('func_rainvolume', targetname='rv', size=(96, 96, 96), at=(-150, -100, 0), tex='AAATRIGGER',
            spawnflags='1', intensity='51', wind_x='100', wind_y='200', wind_rand_x='10', wind_rand_y='20')
    t.point('env_rain', targetname='spirit', at=(0, 150, 200), spawnflags='1', intensity='53', m_flWindX='100',
            m_flWindY='200', m_flRandX='10', m_flRandY='20')
    t.brush('func_snowvolume', targetname='sv', size=(96, 96, 96), at=(100, -100, 0), tex='AAATRIGGER',
            spawnflags='1', intensity='52', wind_x='15', wind_y='-35', wind_rand_x='5', wind_rand_y='6')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 1, 'flags': BRUSH | FIXED | 1, 'intensity': 51, 'wind_x': 100, 'wind_y': 200,
                         'rand_x': 10, 'rand_y': 20})
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 2, 'flags': BRUSH | FIXED | 1, 'intensity': 52, 'wind_x': 15, 'wind_y': -35,
                         'rand_x': 5, 'rand_y': 6})
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn',
                 fields={'type': 1, 'flags': FIXED | 1, 'intensity': 53, 'wind_x': 100, 'wind_y': 200, 'rand_x': 10,
                         'rand_y': 20})


@S.test('env_rain changed while the map runs: trigger_changevalue on intensity, raindrop_height, wind_x (Add 50) and '
        'the spawnflags (Or 64) each send the rain again to both players at once, with the new value; intensity 5000 '
        'gives 1000 and 0 the default 150; the saved fields keep the new values', players=2, doc=LIVE)
def live_rain(t):
    t.point('env_rain', targetname='lr', at=(0, 0, 200), spawnflags='1', intensity='61', wind_x='100')
    t.point('trigger_changevalue', targetname='c_int', target='lr', m_iszValueName='intensity', m_iszNewValue='300')
    t.point('trigger_changevalue', targetname='c_h', target='lr', m_iszValueName='raindrop_height',
            m_iszNewValue='40')
    t.point('trigger_changevalue', targetname='c_wind', target='lr', m_iszValueName='wind_x', m_iszNewValue='50',
            m_iszValueType='1')
    t.point('trigger_changevalue', targetname='c_flag', target='lr', m_iszValueName='spawnflags',
            m_iszNewValue='64', m_iszValueType='6')
    t.point('trigger_changevalue', targetname='c_big', target='lr', m_iszValueName='intensity', m_iszNewValue='5000')
    t.point('trigger_changevalue', targetname='c_zero', target='lr', m_iszValueName='intensity', m_iszNewValue='0')
    t.wait(0.3)
    for name in ('c_int', 'c_h', 'c_wind', 'c_flag', 'c_big', 'c_zero'):
        t.mark(name)
        t.fire(name)
        t.wait(0.3)
        if name == 'c_flag':
            t.snap('saved', 'lr', 'save', 'spawnflags')
    for p in ('player1', 'player2'):
        t.expect.msg('WeatherVol', to=p, count=1, after='c_int', within=(0, 0.15),
                     fields={'type': 1, 'flags': FIXED | 1, 'intensity': 300, 'size2': 80, 'wind_x': 100})
        t.expect.msg('WeatherVol', to=p, count=1, after='c_h', within=(0, 0.15),
                     fields={'type': 1, 'intensity': 300, 'size2': 40})
        t.expect.msg('WeatherVol', to=p, count=1, after='c_wind', within=(0, 0.15),
                     fields={'type': 1, 'flags': FIXED | 1, 'size2': 40, 'wind_x': 150})
        t.expect.msg('WeatherVol', to=p, count=1, after='c_flag', within=(0, 0.15),
                     fields={'type': 1, 'flags': FIXED | 1 | 64, 'wind_x': 150})
        t.expect.msg('WeatherVol', to=p, count=1, after='c_big', within=(0, 0.15), fields={'intensity': 1000})
        t.expect.msg('WeatherVol', to=p, count=1, after='c_zero', within=(0, 0.15), fields={'intensity': 150})
    t.expect.value('saved', 'lr', 'save.m_iIntensity', 300)
    t.expect.value('saved', 'lr', 'save.m_flSize2', 40)
    t.expect.value('saved', 'lr', 'save.m_flWindX', 150)
    t.expect.value('saved', 'lr', 'save.m_iWindSet', 1)
    t.expect.value('saved', 'lr', 'spawnflags', 1 | 64, op='&')


@S.test('a trigger_changevalue on a switched-off env_snow\'s Start Active flag switches it on for both players; the '
        'map\'s unnamed env_rain stays on when its spawnflags are set to 0; trigger_copyvalue copies Wind Y from one '
        'snow to another, which is sent again', players=2, doc=LIVE)
def live_flags_copy(t):
    t.point('env_rain', at=(0, 0, 200), intensity='71')
    t.point('env_snow', targetname='ls', at=(50, 0, 200), intensity='72')
    t.point('env_snow', targetname='src', at=(100, 0, 200), spawnflags='1', intensity='73', wind_y='-120')
    t.point('trigger_changevalue', targetname='c_on', target='ls', m_iszValueName='spawnflags', m_iszNewValue='1')
    t.point('trigger_changevalue', targetname='c_unnamed', target='!activator', m_iszValueName='spawnflags',
            m_iszNewValue='0')
    t.point('trigger_copyvalue', targetname='c_copy', netname='src', m_iszSrcValueName='wind_y', target='ls',
            m_iszDstValueName='wind_y')
    t.wait(0.3)
    t.mark('on')
    t.fire('c_on')
    t.wait(0.3)
    t.mark('unnamed')
    t.fire('c_unnamed', activator='@env_rain[5]')     # the suite's sixth env_rain, the only unnamed one
    t.wait(0.3)
    t.mark('copy')
    t.fire('c_copy')
    t.wait(0.3)
    for p in ('player1', 'player2'):
        t.expect.msg('WeatherVol', to=p, count=1, after='on', within=(0, 0.15), fields={'type': 2, 'intensity': 72})
        t.expect.msg('WeatherVol', to=p, count=1, after='unnamed', within=(0, 0.15),
                     fields={'type': 1, 'flags': 1, 'intensity': 71})
        t.expect.msg('WeatherVol', to=p, count=1, after='copy', within=(0, 0.15),
                     fields={'type': 2, 'flags': FIXED | 1, 'intensity': 72, 'wind_y': -120})


@S.test('func_snowvolume changed while the map runs: snowflake_max_size and wind_y each send it again to both '
        'players (brush flag, new value); func_rainvolume: its origin moved 64 units along X sends the new box, and '
        'its spawnflags set to 0 switch it off', players=2, doc=LIVE)
def live_volumes(t):
    t.brush('func_snowvolume', targetname='lsv', size=(96, 96, 96), at=(-100, 0, 0), tex='AAATRIGGER',
            spawnflags='1', intensity='81')
    t.brush('func_rainvolume', targetname='lrv', size=(96, 96, 96), at=(100, 0, 0), tex='AAATRIGGER',
            spawnflags='1', intensity='82')
    t.point('trigger_changevalue', targetname='c_size', target='lsv', m_iszValueName='snowflake_max_size',
            m_iszNewValue='6')
    t.point('trigger_changevalue', targetname='c_wy', target='lsv', m_iszValueName='wind_y', m_iszNewValue='-40')
    t.point('trigger_changevalue', targetname='c_move', target='lrv', m_iszValueName='origin',
            m_iszNewValue='64', m_iszValueType='1', spawnflags='6')     # Don't use Y / Z: X + 64
    t.point('trigger_changevalue', targetname='c_off', target='lrv', m_iszValueName='spawnflags', m_iszNewValue='0')
    x, y, z = t._abs((100, 0, 0))
    t.wait(0.3)
    for name in ('c_size', 'c_wy', 'c_move', 'c_off'):
        t.mark(name)
        t.fire(name)
        t.wait(0.3)
    for p in ('player1', 'player2'):
        t.expect.msg('WeatherVol', to=p, count=1, after='c_size', within=(0, 0.15),
                     fields={'type': 2, 'flags': BRUSH | 1, 'intensity': 81, 'size2': 60})
        t.expect.msg('WeatherVol', to=p, count=1, after='c_wy', within=(0, 0.15),
                     fields={'type': 2, 'flags': BRUSH | FIXED | 1, 'size2': 60, 'wind_y': -40})
        t.expect.msg('WeatherVol', to=p, count=1, after='c_move', within=(0, 0.15),
                     fields={'type': 1, 'intensity': 82, 'mins': [x - 48 - 2 + 64, y - 48 - 2, z - 2]})
        t.expect.msg('WeatherVol', to=p, count=1, after='c_off', within=(0, 0.15), fields={'type': 0, 'intensity': 82})
