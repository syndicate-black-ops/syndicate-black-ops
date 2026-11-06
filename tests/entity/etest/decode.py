"""Decoders for SBO's user messages (the server's writers; the client parsers must read the same).
Each returns a dict, or None when the payload does not fit the format."""

import struct


class Reader:
    def __init__(self, data):
        self.d, self.p = data, 0

    def left(self):
        return len(self.d) - self.p

    def byte(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def short(self):
        v = struct.unpack_from('<h', self.d, self.p)[0]
        self.p += 2
        return v

    def long(self):
        v = struct.unpack_from('<i', self.d, self.p)[0]
        self.p += 4
        return v

    def float(self):
        v = struct.unpack_from('<f', self.d, self.p)[0]
        self.p += 4
        return v

    def string(self):
        end = self.d.index(b'\0', self.p)
        v = self.d[self.p:end].decode('latin-1')
        self.p = end + 1
        return v


def game_timer(r):
    # game_timer.cpp SendState (docs/GAME_TIMER.md "Message GameTimer")
    out = {'id': r.short()}
    flags = r.byte()
    out.update(flags=flags, enabled=bool(flags & 1), paused=bool(flags & 2), show_remaining=bool(flags & 4),
               full=bool(flags & 8))
    if r.left() == 0:
        return out
    out['time'] = r.float()
    if r.left() == 0:
        return out
    out.update(length=r.float(), warn=r.float(), bar_max=r.short(), bar_override=r.short(), x=r.short(), y=r.short(),
               caption_width=r.short(), caption_lines=r.byte(), caption=r.string())
    return out


def fog(r):
    # player.cpp SendFog: r g b, density (float, byte by byte)
    red, green, blue = r.byte(), r.byte(), r.byte()
    return {'color': [red, green, blue], 'density': r.float()}


def weather_vol(r):
    # triggers.cpp CWeatherVolume::Send: short entity, byte type (0 removed, 1 rain, 2 snow), short flags, short
    # intensity, short period x 100, coord x 6 (absmin, absmax), short size1, size2, speed1, speed2, life x 100
    out = {'ent': r.short(), 'type': r.byte(), 'flags': r.short() & 0xffff, 'intensity': r.short(),
           'period': r.short() / 100.0}
    c = [r.short() / 8.0 for _ in range(6)]
    out.update(mins=c[:3], maxs=c[3:])
    out.update(size1=r.short(), size2=r.short(), speed1=r.short(), speed2=r.short(), life=r.short() / 100.0)
    return out


def fog_zone(r):
    # triggers.cpp FogZone_Send: byte index, byte flags (1 active, 2 first of a full list), coord x 6 (absmin,
    # absmax; a coord is a short of 1/8 units), r g b, density (float, byte by byte)
    out = {'index': r.byte(), 'flags': r.byte()}
    c = [r.short() / 8.0 for _ in range(6)]
    out.update(mins=c[:3], maxs=c[3:])
    red, green, blue = r.byte(), r.byte(), r.byte()
    out.update(color=[red, green, blue], density=r.float())
    return out


def message_box(r):
    # game_messagebox.cpp: byte open, short id, [string title, string text]
    out = {'open': r.byte(), 'id': r.short()}
    if out['open'] and r.left():
        out['title'] = r.string()
        out['text'] = r.string()
    return out


def tutor_text(r):
    # tutor.cpp: short entity, short type, byte flags, byte align, short x, short y, byte icon, ...
    return {'ent': r.short(), 'type': r.short(), 'flags': r.byte(), 'align': r.byte(), 'x': r.short(), 'y': r.short(),
            'icon': r.byte()}


def text_msg(r):
    out = {'dest': r.byte()}
    out['text'] = r.string() if r.left() else ''
    return out


def map_music(r):
    # sound.cpp GlobalMp3_Send (game_shared/map_music_shared.h): byte state, byte serial, string path
    return {'state': r.byte(), 'serial': r.byte(), 'path': r.string()}


def play_video(r):
    # maprules.cpp CGamePlayVideo::Use (game_shared/video_shared.h): string video, string audio
    return {'video': r.string(), 'audio': r.string()}


def wscreen(r):
    # vgui_screen.cpp (game_shared/vgui_screen_shared.h): byte op; DEFINE: short entindex, coord origin[3], short
    # angles[3], coord width, coord height, short inputdistance, byte flags, string panel, [bound: short carrier,
    # short submodel], [movie: string moviefilename, long ms since it started, byte volume, short radius];
    # STATE: short entindex, byte flags
    op = r.byte()
    d = {'op': op, 'ent': r.short()}
    if op == 1:
        d['flags'] = r.byte()
        return d
    if op != 0:
        return d
    d['origin'] = [r.short() / 8.0 for _ in range(3)]
    d['angles'] = [r.short() for _ in range(3)]
    d['width'] = r.short() / 8.0
    d['height'] = r.short() / 8.0
    d['inputdistance'] = r.short()
    d['flags'] = r.byte()
    d['panel'] = r.string()
    if d['flags'] & 4:
        d['carrier'] = r.short()
        d['submodel'] = r.short()
    if d['flags'] & 16:
        d['movie'] = r.string()
        d['elapsed_ms'] = r.long()
        d['volume'] = r.byte()
        d['radius'] = r.short()
    return d


def death_msg(r):
    # multiplay_gamerules.cpp DeathNotice: byte killer, byte victim, string weapon code (d_<code> in hud.txt),
    # byte headshot (SBO 2.0, CS 1.6)
    out = {'killer': r.byte(), 'victim': r.byte(), 'weapon': r.string()}
    if r.left():
        out['headshot'] = r.byte()
    return out


def sbo_death_msg(r):
    # multiplay_gamerules.cpp SboKillfeed_Send: byte flags, byte killer, string killer name, byte victim, string victim
    # name, string weapon code, string weapon name, byte headshot (only if present)
    out = {'flags': r.byte(), 'killer': r.byte(), 'killer_name': r.string(), 'victim': r.byte(),
           'victim_name': r.string(), 'weapon': r.string(), 'weapon_name': r.string()}
    if r.left():
        out['headshot'] = r.byte()
    return out


def temp_entity(r):
    # SVC_TEMPENTITY (23, an engine message: name 'svc:23'): the TE type, and for the types with a model or sprite its
    # position and index (common/const.h; the trace writes a coord as a short of 1/8 units, an angle as a byte)
    te = r.byte()
    out = {'te': te}
    coord = lambda: r.short() / 8.0
    if te == 23:        # TE_GLOWSPRITE: pos, short sprite, byte life, byte scale, byte brightness
        out.update(pos=[coord() for _ in range(3)], model=r.short())
    elif te == 15:      # TE_SPRITETRAIL: start, end, short sprite, count, life, scale, velocity, randomness
        out.update(pos=[coord() for _ in range(3)], end=[coord() for _ in range(3)], model=r.short())
    elif te == 106:     # TE_MODEL: pos, velocity, angle yaw, short model, byte bounce sound, byte life
        out.update(pos=[coord() for _ in range(3)], velocity=[coord() for _ in range(3)], yaw=r.byte(),
                   model=r.short(), sound=r.byte(), life=r.byte())
    elif te == 17:      # TE_SPRITE: pos, short sprite, byte scale, byte brightness
        out.update(pos=[coord() for _ in range(3)], model=r.short())
    elif te == 3:       # TE_EXPLOSION: pos, short sprite, byte scale, byte framerate, byte flags (TE_EXPLFLAG_*)
        out.update(pos=[coord() for _ in range(3)], model=r.short(), scale=r.byte(), framerate=r.byte(),
                   flags=r.byte())
    elif te == 5:       # TE_SMOKE: pos, short sprite, byte scale, byte framerate
        out.update(pos=[coord() for _ in range(3)], model=r.short(), scale=r.byte(), framerate=r.byte())
    elif te in GUIDE_TE:
        GUIDE_TE[te](r, out, coord)
    return out


def _te_pos(r, out, coord):
    out['pos'] = [coord() for _ in range(3)]


def _te_beampoints(r, out, coord):     # TE_BEAMPOINTS: start, end, sprite, frame, rate, life, width, noise, rgb, a, speed
    out.update(pos=[coord() for _ in range(3)], end=[coord() for _ in range(3)], model=r.short(), frame=r.byte(),
               rate=r.byte(), life=r.byte(), width=r.byte(), noise=r.byte(), color=[r.byte() for _ in range(3)],
               alpha=r.byte(), speed=r.byte())


def _te_ring(r, out, coord):           # TE_BEAMTORUS / DISK / CYLINDER: center, axis end, the beam's fields
    _te_beampoints(r, out, coord)
    out['radius'] = out['end'][2] - out['pos'][2]


def _te_beamfollow(r, out, coord):     # TE_BEAMFOLLOW: short entity, sprite, life, width, rgb, brightness
    out.update(ent=r.short(), model=r.short(), life=r.byte(), width=r.byte(), color=[r.byte() for _ in range(3)],
               alpha=r.byte())


def _te_killbeam(r, out, coord):       # TE_KILLBEAM: short entity
    out['ent'] = r.short()


def _te_bspdecal(r, out, coord):       # TE_BSPDECAL: pos, short texture, short entity (+ short model if entity)
    out.update(pos=[coord() for _ in range(3)], texture=r.short(), ent=r.short())


def _te_breakmodel(r, out, coord):     # TE_BREAKMODEL: pos, size, velocity, random, model, count, life, flags
    out.update(pos=[coord() for _ in range(3)], size=[coord() for _ in range(3)],
               velocity=[coord() for _ in range(3)], random=r.byte(), model=r.short(), count=r.byte(), life=r.byte(),
               flags=r.byte())


def _te_burst(r, out, coord):          # TE_PARTICLEBURST: pos, short radius, byte color, byte duration
    out.update(pos=[coord() for _ in range(3)], radius=r.short(), color=r.byte(), duration=r.byte())


def _te_implosion(r, out, coord):      # TE_IMPLOSION: pos, byte radius, byte count, byte life
    out.update(pos=[coord() for _ in range(3)], radius=r.byte(), count=r.byte(), duration=r.byte())


def _te_dlight(r, out, coord):         # TE_DLIGHT: pos, radius, rgb, life, decay
    out.update(pos=[coord() for _ in range(3)], radius=r.byte(), color=[r.byte() for _ in range(3)], life=r.byte(),
               decay=r.byte())


# feat/featureful-guide-entities: the temp entities of its effects (common/const.h)
def _te_smoke(r, out, coord):           # TE_SMOKE: pos, short sprite, byte scale, byte framerate
    out.update(pos=[coord() for _ in range(3)], model=r.short(), scale=r.byte(), framerate=r.byte())


def _te_funnel(r, out, coord):          # TE_LARGEFUNNEL: pos, short sprite, short flags
    out.update(pos=[coord() for _ in range(3)], model=r.short(), flags=r.short())


def _te_blood(r, out, coord):           # TE_BLOODSTREAM / TE_BLOOD: pos, direction (coords), byte color, byte speed
    out.update(pos=[coord() for _ in range(3)], dir=[coord() for _ in range(3)], color=r.byte(), speed=r.byte())


def _te_fizz(r, out, coord):            # TE_FIZZ: short entity, short sprite, byte density
    out.update(ent=r.short(), model=r.short(), density=r.byte())


def _te_bloodsprite(r, out, coord):     # TE_BLOODSPRITE: pos, short spray sprite, short drop sprite, byte color, byte scale
    out.update(pos=[coord() for _ in range(3)], model=r.short(), model2=r.short(), color=r.byte(), scale=r.byte())


GUIDE_TE = {0: _te_beampoints, 19: _te_ring, 20: _te_ring, 21: _te_ring, 22: _te_beamfollow, 99: _te_killbeam,
            13: _te_bspdecal, 108: _te_breakmodel, 4: _te_pos, 10: _te_pos, 11: _te_pos, 12: _te_pos,
            14: _te_implosion, 122: _te_burst, 27: _te_dlight, 5: _te_smoke, 100: _te_funnel, 101: _te_blood,
            103: _te_blood, 105: _te_fizz, 115: _te_bloodsprite, 9: _te_pos}


def object_hint(r):
    # objecthint.cpp SendObjectHint (game_shared/objecthint_shared.h): flags, entity index; with an index: color,
    # scale factor, position, size (coords: 1/8 unit), sprite
    out = {'flags': r.byte(), 'ent': r.short()}
    if out['ent'] > 0:
        out.update({'r': r.byte(), 'g': r.byte(), 'b': r.byte(), 'scale': r.short() / 8.0})
        out['pos'] = [r.short() / 8.0 for _ in range(3)]
        out['size'] = [r.short() / 8.0 for _ in range(3)]
        out['sprite'] = r.string()
    return out


def keyed_dlight(r):
    # env_dlight.cpp (game_shared/dlight_shared.h): key (the env_dlight's index), state 0 off / 1 on / 2 position;
    # on: origin (coords), radius, r g b, the entity to follow (0: none); position: origin
    out = {'key': r.short(), 'state': r.byte()}
    if out['state'] in (1, 2):
        out['pos'] = [r.short() / 8.0 for _ in range(3)]
    if out['state'] == 1:
        out.update(radius=r.short(), r=r.byte(), g=r.byte(), b=r.byte(), follow=r.short())
    return out


def break_model(r):
    # func_break.cpp CBreakable::BreakModel with "gibscale": TE_BREAKMODEL's fields, then short scale x 100
    coord = lambda: r.short() / 8.0
    return {'pos': [coord() for _ in range(3)], 'size': [coord() for _ in range(3)],
            'velocity': [coord() for _ in range(3)], 'random': r.byte(), 'model': r.short(), 'count': r.byte(),
            'life': r.byte(), 'flags': r.byte(), 'scale': r.short()}


DECODERS = {'BreakModel': break_model, 'KeyedDLight': keyed_dlight, 'ObjectHint': object_hint, 'PlayVideo': play_video, 'WScreen': wscreen, 'svc:23': temp_entity, 'MapMusic': map_music, 'DeathMsg': death_msg, 'SboDeathMsg': sbo_death_msg, 'GameTimer': game_timer, 'Fog': fog, 'FogZone': fog_zone, 'WeatherVol': weather_vol, 'MessageBox': message_box, 'TutorText': tutor_text,
            'TextMsg': text_msg, 'TutorPart': lambda r: {'text': r.string()},
            # coop.cpp HeavyLimit_Update: grenade launchers the other players hold, their limit, nuclear launchers, limit
            'HeavyLimit': lambda r: {'gl': r.byte(), 'gl_max': r.byte(), 'nuke': r.byte(), 'nuke_max': r.byte()},
            'Capability': lambda r: {'bits': r.long()},
            'MsgBoxPart': lambda r: {'which': r.byte(), 'text': r.string()},
            # weapons.cpp UpdateClientData / player.cpp: state (1 = active), weapon id, clip
            'CurWeapon': lambda r: {'state': r.byte(), 'id': r.byte(), 'clip': r.byte()},
            'SetFOV': lambda r: {'fov': r.byte()},
            # util.cpp UTIL_ScreenFadeWrite: duration, hold (4.12 fixed, unsigned), flags (FFADE_*), r, g, b, alpha
            'ScreenFade': lambda r: {'duration': (r.short() & 0xffff) / 4096.0, 'hold': (r.short() & 0xffff) / 4096.0,
                                     'flags': r.short(), 'r': r.byte(), 'g': r.byte(), 'b': r.byte(), 'a': r.byte()},
            # multiplay_gamerules.cpp SboScoreboard_WriteScoreInfo: client, score (co-op: the level), deaths, class, team
            'ScoreInfo': lambda r: {'client': r.byte(), 'score': r.short(), 'deaths': r.short(), 'class': r.short(),
                                    'team': r.short()},
            # multiplay_gamerules.cpp SboScoreboard_WriteXP: client, XP (signed long)
            'SboXP': lambda r: {'client': r.byte(), 'xp': r.long()},
            # player.cpp SendWeatherInfo: 0 none, 1 rain, 2 snow
            'ReceiveW': lambda r: {'weather': r.byte()},
            # items.cpp: the HUD pickup icon's hud.txt name; util.cpp UTIL_ShowMessage: the text (titles.txt name)
            'ItemPickup': lambda r: {'name': r.string()},
            'HudText': lambda r: {'text': r.string()}}


def decode(name, data, type_=None):
    """name: the user message's name; an engine message has none and is decoded by its number ('svc:<type>')."""
    f = DECODERS.get(name or 'svc:%s' % type_)
    if not f:
        return {'size': len(data)}
    try:
        return f(Reader(data))
    except (IndexError, ValueError, struct.error):
        return None
