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


def objective(r):
    # trigger_objective.cpp CTriggerObjective::Send / Objective_SendQueued (game_shared/objective_shared.h)
    slot = r.byte()
    out = {'slot': slot, 'clear': slot == 255}
    if slot == 255:
        return out
    f = r.byte()
    out.update(flags=f, state=f & 3, new=bool(f & 0x10), done=bool(f & 0x20), counted=bool(f & 0x40),
               silent=bool(f & 0x80))
    if f & 3 == 0:
        return out
    out.update(order=r.short(), seq=r.short(), count=r.byte(), target=r.byte())
    p = r.byte()
    out.update(parent=-1 if p == 255 else p, text=r.string())
    return out


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
    # intensity, short period x 100, coord x 6 (absmin, absmax), short size1, size2, speed1, speed2, life x 100, signed
    # short wind x, wind y (flag 8192: the fixed wind), short spread x, spread y
    out = {'ent': r.short(), 'type': r.byte(), 'flags': r.short() & 0xffff, 'intensity': r.short(),
           'period': r.short() / 100.0}
    c = [r.short() / 8.0 for _ in range(6)]
    out.update(mins=c[:3], maxs=c[3:])
    out.update(size1=r.short(), size2=r.short(), speed1=r.short(), speed2=r.short(), life=r.short() / 100.0)
    out.update(wind_x=r.short(), wind_y=r.short(), rand_x=r.short(), rand_y=r.short())
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


def overlay(r):
    # env_overlay.cpp CEnvOverlay::Send / SendOff (game_shared/overlay_shared.h, docs/ENV_OVERLAY.md)
    out = {'channel': r.byte()}
    flags = r.byte()
    out.update(flags=flags, on=int(bool(flags & 1)), fullscreen=int(bool(flags & 2)), loop=int(bool(flags & 4)),
               additive=int(bool(flags & 8)), stay=int(bool(flags & 16)), now=int(bool(flags & 32)))
    if not flags & 1:
        return out
    out['image'] = r.string()
    x, y = r.short(), r.short()
    out.update(x=-1.0 if x <= -10000 else round(x / 10000.0, 4), y=-1.0 if y <= -10000 else round(y / 10000.0, 4),
               scale=round(r.short() / 100.0, 2), r=r.byte(), g=r.byte(), b=r.byte(), a=r.byte(),
               framerate=round((r.short() & 0xffff) / 100.0, 2), fadein=round((r.short() & 0xffff) / 100.0, 2),
               hold=round((r.short() & 0xffff) / 100.0, 2), fadeout=round((r.short() & 0xffff) / 100.0, 2),
               elapsed=round((r.short() & 0xffff) / 10.0, 1))
    return out


def color_corr(r):
    # colorcorrection.cpp CColorCorrectionBase::SendTo (game_shared/colorcorrection_shared.h, docs/COLOR_CORRECTION.md)
    out = {'ent': r.short()}
    flags = r.byte()
    out.update(flags=flags, enabled=int(bool(flags & 1)), volume=int(bool(flags & 2)), exclusive=int(bool(flags & 4)),
               first=int(bool(flags & 8)), removed=int(bool(flags & 16)))
    if flags & 16:
        return out
    out.update(maxweight=round(r.short() / 10000.0, 4), weight=round(r.short() / 10000.0, 4),
               fadein=round((r.short() & 0xffff) / 100.0, 2), fadeout=round((r.short() & 0xffff) / 100.0, 2))
    if flags & 2:
        out.update(model=r.short(), x=r.short(), y=r.short(), z=r.short())
    else:
        out.update(x=r.short(), y=r.short(), z=r.short(), minfalloff=r.short(), maxfalloff=r.short())
    out.update(saturation=round(r.short() / 1000.0, 3), contrast=round(r.short() / 1000.0, 3),
               brightness=round(r.short() / 1000.0, 3), gamma=round(r.short() / 1000.0, 3),
               tint_r=r.byte(), tint_g=r.byte(), tint_b=r.byte(), tintamount=round(r.byte() / 255.0, 2),
               file=r.string())
    return out


def tutor_text(r):
    # tutor.cpp SendTutorText: short entity, short type, byte flags (1 dead, 2 text color, 4 icon color, 8 background,
    # 16 border), byte align, short x, short y (-1 or 0..10000), byte icon [string icon path if 255], string sound,
    # the colors the flags name (text rgb, icon rgb, background rgba, border rgba), string text (its end)
    out = {'ent': r.short(), 'type': r.short(), 'flags': r.byte(), 'align': r.byte(), 'x': r.short(), 'y': r.short(),
           'icon': r.byte()}
    if r.left() == 0:
        return out
    if out['icon'] == 255:
        out['iconfile'] = r.string()
    out['sound'] = r.string()
    f = out['flags']
    if f & 2:
        out['color'] = [r.byte(), r.byte(), r.byte()]
    if f & 4:
        out['iconcolor'] = [r.byte(), r.byte(), r.byte()]
    if f & 8:
        out['bgcolor'] = [r.byte(), r.byte(), r.byte(), r.byte()]
    if f & 16:
        out['bordercolor'] = [r.byte(), r.byte(), r.byte(), r.byte()]
    out['text'] = r.string()
    return out


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
    if op == 2:     # CONTROL: byte control op (0 text, 1 enabled, 2 visible, 3 animation), string name, string value
        d.update(cop=r.byte(), control=r.string(), value=r.string())
        return d
    if op == 3:     # PARENT (movewith): short parent (index, attachment << 11), local origin (3 floats), local angles
        d['parent_attachment'] = (r.short() & 0xffff) >> 11
        d['local_origin'] = [round(r.float(), 3) for _ in range(3)]
        d['local_angles'] = [round((r.short() & 0xffff) * 360.0 / 65536.0, 2) for _ in range(3)]
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


def _te_gunshotdecal(r, out, coord):    # TE_GUNSHOTDECAL: pos, short entity, byte decal
    out['pos'] = [coord() for _ in range(3)]
    out['ent'] = r.short()
    out['decal'] = r.byte()


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


def _te_worlddecal(r, out, coord):      # TE_WORLDDECAL: pos, byte decal index (decals.wad order: {blood1 27)
    out.update(pos=[coord() for _ in range(3)], index=r.byte())


def _te_fizz(r, out, coord):            # TE_FIZZ: short entity, short sprite, byte density
    out.update(ent=r.short(), model=r.short(), density=r.byte())


def _te_bloodsprite(r, out, coord):     # TE_BLOODSPRITE: pos, short spray sprite, short drop sprite, byte color, byte scale
    out.update(pos=[coord() for _ in range(3)], model=r.short(), model2=r.short(), color=r.byte(), scale=r.byte())


def _te_textmessage(r, out, coord):    # TE_TEXTMESSAGE (util.cpp UTIL_HudMessage, game_text)
    # byte channel, short x and y (1/8192, -1 centers), byte effect, rgba color1, rgba color2, short fade-in, fade-out,
    # hold (1/256 s), short fx time only for effect 2, string text
    out.update(channel=r.byte(), x=round(r.short() / 8192.0, 4), y=round(r.short() / 8192.0, 4), effect=r.byte(),
               color=[r.byte() for _ in range(3)], a1=r.byte(), color2=[r.byte() for _ in range(3)], a2=r.byte(),
               fadein=round((r.short() & 0xffff) / 256.0, 3), fadeout=round((r.short() & 0xffff) / 256.0, 3),
               hold=round((r.short() & 0xffff) / 256.0, 3))
    if out['effect'] == 2:
        out['fxtime'] = round((r.short() & 0xffff) / 256.0, 3)
    out['text'] = r.string()


GUIDE_TE = {29: _te_textmessage, 0: _te_beampoints, 19: _te_ring, 20: _te_ring, 21: _te_ring, 22: _te_beamfollow, 99: _te_killbeam,
            13: _te_bspdecal, 108: _te_breakmodel, 4: _te_pos, 10: _te_pos, 11: _te_pos, 12: _te_pos,
            14: _te_implosion, 122: _te_burst, 27: _te_dlight, 5: _te_smoke, 100: _te_funnel, 101: _te_blood,
            103: _te_blood, 105: _te_fizz, 115: _te_bloodsprite, 9: _te_pos,
            109: _te_gunshotdecal, 116: _te_worlddecal}


def aurora(r):
    # env_particle.cpp WriteState / SpawnCopy / UpdateOnRemove (game_shared/aurora_shared.h): byte flags (1 on,
    # 2 one-shot copy, 4 removed, 8 follows an entity), short entity; not removed: origin (3 floats as long bits),
    # angles (3 coords), with 8: short target entity, char attachment; string script
    flags = r.byte()
    out = {'flags': flags, 'on': int(bool(flags & 1)), 'oneshot': int(bool(flags & 2)), 'removed': int(bool(flags & 4)),
           'follow': int(bool(flags & 8)), 'ent': r.short()}
    if flags & 4:
        return out
    out['origin'] = [round(r.float(), 3) for _ in range(3)]
    out['angles'] = [r.short() / 8.0 for _ in range(3)]
    if flags & 8:
        out['target'] = r.short()
        a = r.byte()
        out['attachment'] = a - 256 if a > 127 else a
    out['script'] = r.string()
    return out


def mirror(r):
    # env_mirror.cpp SendState: short entity, byte flags (1 on, 2 players drawn); on: byte axis, char side, coord
    # mins x3, coord maxs x3, coord plane, short radius
    out = {'ent': r.short()}
    flags = r.byte()
    out.update(flags=flags, on=int(bool(flags & 1)), players=int(bool(flags & 2)))
    if not flags & 1:
        return out
    out['axis'] = r.byte()
    s = r.byte()
    out['side'] = s - 256 if s > 127 else s
    c = [r.short() / 8.0 for _ in range(7)]
    out.update(mins=c[:3], maxs=c[3:6], plane=c[6], radius=r.short())
    return out


def set_sky(r):
    # effects.cpp CEnvSky::SendState (SoHL "SetSky"): byte mode (1 on), coord x y z, byte scale
    return {'mode': r.byte(), 'origin': [r.short() / 8.0 for _ in range(3)], 'scale': r.byte()}


def world_text(r):
    # point_worldtext.cpp SendTo (game_shared/worldtext_shared.h): byte flags (1 on, 2 rainbow, 4 centered, 8 parent),
    # short id;
    # on: origin (3 floats), angles (3 shorts, 65536 = 360), size, spacing x, spacing y (floats), r g b a,
    # orientation, font, string (the text or its end)
    flags = r.byte()
    out = {'flags': flags, 'on': int(bool(flags & 1)), 'rainbow': int(bool(flags & 2)), 'center': int(bool(flags & 4)),
           'id': r.short()}
    if not flags & 1:
        return out
    out['origin'] = [round(r.float(), 3) for _ in range(3)]
    out['angles'] = [round((r.short() & 0xffff) * 360.0 / 65536.0, 2) for _ in range(3)]
    out.update(size=round(r.float(), 3), spacing_x=round(r.float(), 3), spacing_y=round(r.float(), 3))
    out['color'] = [r.byte(), r.byte(), r.byte(), r.byte()]
    out.update(orientation=r.byte(), font=r.byte())
    # 8: a parent (movewith): short parent (index, attachment << 11), local origin (3 floats), local angles (3 shorts)
    out['parented'] = int(bool(flags & 8))
    if flags & 8:
        p = r.short() & 0xffff
        out.update(parent_attachment=p >> 11)
        out['local_origin'] = [round(r.float(), 3) for _ in range(3)]
        out['local_angles'] = [round((r.short() & 0xffff) * 360.0 / 65536.0, 2) for _ in range(3)]
    out['text'] = r.string()
    return out


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
    # on: origin (coords), radius, r g b, the entity to follow (0: none); position: origin.
    # env_elight: state 0x81 (on + the elight flag 0x80), then the on fields and the attachment byte
    out = {'key': r.short(), 'state': r.byte()}
    base = out['state'] & 0x7f
    out['elight'] = 1 if out['state'] & 0x80 else 0
    if base in (1, 2):
        out['pos'] = [r.short() / 8.0 for _ in range(3)]
    if base == 1:
        out.update(radius=r.short(), r=r.byte(), g=r.byte(), b=r.byte(), follow=r.short())
        if out['elight']:
            out['attachment'] = r.byte()
    return out


def light_origin(r):
    # light_origin.cpp (game_shared/light_origin_shared.h): entity index, its model index, the marker's origin
    return {'ent': r.short(), 'modelindex': r.short(), 'pos': [r.short() / 8.0 for _ in range(3)]}


def static_prop(r):
    # env_static.cpp (game_shared/static_props_shared.h): byte type; MODEL (1): byte id, string file; PROP (2): short
    # prop id, byte model id, 3 long origin (1/8), 3 angle bytes, byte rendermode, byte renderfx, 3 byte color, byte
    # renderamt, byte body, byte skin, short sequence, short scale (1/256), short framerate (1/256), byte visible;
    # VISIBLE (3): short prop id, byte visible; LIGHTORIGIN (4): short prop id, 3 long origin (1/8)
    out = {'type': r.byte()}
    if out['type'] == 1:
        out.update(model_id=r.byte(), file=r.string())
    elif out['type'] == 2:
        out['prop'] = r.short()
        try:
            out.update(model_id=r.byte(), origin=[r.long() / 8.0 for _ in range(3)],
                       angles=[r.byte() * 360.0 / 256 for _ in range(3)], rendermode=r.byte(), renderfx=r.byte(),
                       color=[r.byte() for _ in range(3)], renderamt=r.byte(), body=r.byte(), skin=r.byte(),
                       sequence=r.short(), scale=round(r.short() / 256.0, 3), framerate=round(r.short() / 256.0, 3),
                       visible=r.byte())
        except Exception:
            pass
    elif out['type'] == 3:
        out['prop'] = r.short()
        out['visible'] = r.byte()
    elif out['type'] == 4:
        out['prop'] = r.short()
        out['pos'] = [r.long() / 8.0 for _ in range(3)]
    return out


def break_model(r):
    # func_break.cpp CBreakable::BreakModel with "gibscale": TE_BREAKMODEL's fields, then short scale x 100
    coord = lambda: r.short() / 8.0
    return {'pos': [coord() for _ in range(3)], 'size': [coord() for _ in range(3)],
            'velocity': [coord() for _ in range(3)], 'random': r.byte(), 'model': r.short(), 'count': r.byte(),
            'life': r.byte(), 'flags': r.byte(), 'scale': r.short()}


def show_menu(r):
    out = {'keys': r.short(), 'time': r.byte(), 'more': r.byte(), 'text': r.string()}
    title = out['text'].split('\n')[0]
    if title.startswith('\\y'):
        title = title[2:]
    out['title'] = title.split('\xb7 ')[-1]
    return out


DECODERS = {'Aurora': aurora, 'Mirror': mirror, 'SetSky': set_sky, 'WorldText': world_text,
            'WorldTextP': lambda r: {'id': r.short(), 'text': r.string()},
            'BreakModel': break_model, 'KeyedDLight': keyed_dlight, 'LightOrigin': light_origin, 'StaticProp': static_prop, 'ObjectHint': object_hint, 'PlayVideo': play_video, 'WScreen': wscreen, 'svc:23': temp_entity, 'MapMusic': map_music, 'DeathMsg': death_msg, 'SboDeathMsg': sbo_death_msg, 'GameTimer': game_timer, 'Fog': fog, 'FogZone': fog_zone, 'WeatherVol': weather_vol, 'MessageBox': message_box, 'Overlay': overlay, 'ColorCorr': color_corr, 'TutorText': tutor_text,
            'Objective': objective,
            # sound.cpp TEXTURETYPE_SendGrassInfo (CS:CZ DS GrassInfo): string sprite, coord fade, min, max, long density
            'GrassInfo': lambda r: {'sprite': r.string(), 'fade': r.short() / 8.0, 'min': r.short() / 8.0,
                                    'max': r.short() / 8.0, 'density': r.long()},
            'MatFile': lambda r: {'file': r.string()},     # player.cpp: the map's materials file ("" none)
            # player.cpp UpdateClientData: the HIDEHUD_* bits (4 HIDEHUD_ALL: the whole HUD)
            'HideWeapon': lambda r: (lambda f: {'flags': f, 'all': bool(f & 4)})(r.byte()),
            'TextMsg': text_msg, 'TutorPart': lambda r: {'text': r.string()},
            # inventory.cpp Inv_Write (game_shared/inventory_shared.h): short count, string item, string icon, byte flags
            'Inventory': lambda r: {'count': r.short(), 'item': r.string(), 'icon': r.string(), 'shared': r.byte() & 1},
            # coop.cpp HeavyLimit_Update: grenade launchers the other players hold, their limit, nuclear launchers, limit
            'HeavyLimit': lambda r: {'gl': r.byte(), 'gl_max': r.byte(), 'nuke': r.byte(), 'nuke_max': r.byte()},
            'Capability': lambda r: {'bits': r.long()},
            # player.cpp UpdateClientData: the server's sv_cheats value (the client's in-game editors need 2)
            'Cheats': lambda r: {'value': r.byte()},
            'MsgBoxPart': lambda r: {'which': r.byte(), 'text': r.string()},
            # weapons.cpp UpdateClientData / player.cpp: state (1 = active), weapon id, clip
            'CurWeapon': lambda r: {'state': r.byte(), 'id': r.byte(), 'clip': r.byte()},
            # weapons.cpp AddToPlayer: the weapon id picked up; player.cpp UpdateClientData: weapon ids 32..63 he holds
            'WeapPickup': lambda r: {'id': r.byte()},
            'WeaponsHigh': lambda r: {'bits': r.long() & 0xffffffff},
            'SetFOV': lambda r: {'fov': r.byte()},
            # SVC_WEAPONANIM (35, weapons.cpp SendWeaponAnim): the view model's sequence, its body
            'svc:35': lambda r: {'anim': r.byte(), 'body': r.byte()},
            # SVC_ROOMTYPE (37, player.cpp UpdateClientData: the env_sound room type the client has): short
            'svc:37': lambda r: {'room': r.short()},
            # SVC_SPAWNSTATICSOUND (29, sound.cpp AmbientSoundMsg: one client's own volume of an ambient_generic linear
            # play mode): coord origin x 3 (a short of 1/8 units), short sound, byte volume x 255, byte attenuation x 64,
            # short entity, byte pitch, byte flags
            'svc:29': lambda r: {'origin': [r.short() / 8.0 for _ in range(3)], 'sound': r.short(), 'vol': r.byte(),
                                 'attn': r.byte(), 'ent': r.short(), 'pitch': r.byte(), 'flags': r.byte()},
            # util.cpp UTIL_ScreenFadeWrite: duration, hold (4.12 fixed, unsigned), flags (FFADE_*), r, g, b, alpha
            'ScreenFade': lambda r: {'duration': (r.short() & 0xffff) / 4096.0, 'hold': (r.short() & 0xffff) / 4096.0,
                                     'flags': r.short(), 'r': r.byte(), 'g': r.byte(), 'b': r.byte(), 'a': r.byte()},
            # util.cpp UTIL_ScreenShake: amplitude, duration (4.12 fixed), frequency (8.8 fixed), all unsigned
            'ScreenShake': lambda r: {'amplitude': (r.short() & 0xffff) / 4096.0, 'duration': (r.short() & 0xffff) / 4096.0,
                                      'frequency': (r.short() & 0xffff) / 256.0},
            # multiplay_gamerules.cpp SboScoreboard_WriteScoreInfo: client, score (co-op: the level), deaths, class, team
            'ScoreInfo': lambda r: {'client': r.byte(), 'score': r.short(), 'deaths': r.short(), 'class': r.short(),
                                    'team': r.short()},
            # multiplay_gamerules.cpp SboScoreboard_WriteXP: client, XP (signed long)
            'SboXP': lambda r: {'client': r.byte(), 'xp': r.long()},
            # player.cpp SendWeatherInfo: 0 none, 1 rain, 2 snow
            'ReceiveW': lambda r: {'weather': r.byte()},
            # items.cpp: the HUD pickup icon's hud.txt name; util.cpp UTIL_ShowMessage: the text (titles.txt name)
            'ItemPickup': lambda r: {'name': r.string()},
            'HudText': lambda r: {'text': r.string()},
            # gmmode.cpp GmMode_Notice: the line under the Game Master's crosshair
            'GmNotice': lambda r: {'text': r.string()},
            # player.cpp / gmmode.cpp text menus: keys, time, more, text; 'title' the first line after its color code
            # and up to a " \xb7 " (the GM menus' "ORDERS \xb7 2 SELECTED": '2 SELECTED')
            'ShowMenu': show_menu}


def decode(name, data, type_=None):
    """name: the user message's name; an engine message has none and is decoded by its number ('svc:<type>')."""
    f = DECODERS.get(name or 'svc:%s' % type_)
    if not f:
        return {'size': len(data)}
    try:
        return f(Reader(data))
    except (IndexError, ValueError, struct.error):
        return None
