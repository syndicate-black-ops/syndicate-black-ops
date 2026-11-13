"""FGD defaults check generated from sdk/syndicate.fgd (suites fgd_defaults_*).

A map that leaves a key out must behave as the FGD default says (the editor writes the default; other editors, old
maps and keys added later leave it out). For every class with keys that have an FGD default, one room holds:
- `a`: the class with every one of those keys set to its FGD default
- `b`, `n`: the class with none of them (n: a second copy, so fields that differ at random are left out)
all at the same spot, and checks that `a` and `b` save the same fields (CBaseEntity::Save: the entvars and every
member of the class, through the test layer's snapshot field `save`). A field that differs means a key left out
does not give what the FGD default says.

PER_KEY: classes where that check differs get one test per key instead (`a` with only that key), so each mismatch
names its key; a key whose left-out value differs on purpose is in LEFT_OUT (its help says what it gives).

The check sees the state the entity keeps after it spawned; a default applied later (at the first Use, say) is
not seen. Keys for the compiler (scripts/fgd_rules.json tool_keys) and spawnflags are left out.

`python3 tests/entity/etest/defaults.py` prints the plan.
"""

import fnmatch
import json
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
FGD_PATH = os.path.join(ROOT, 'sdk', 'syndicate.fgd')
PARTS = 8

# Classes left out, and why
SKIP = {
    'worldspawn': 'the map itself',
    'game_end': 'a map ender',
    'mission_failed': 'a map ender',
    'trigger_changelevel': 'changes the map when touched',
    'env_smoker': "made by the gargantua's death: it puffs and removes itself",
    'func_tanklaser': 'removes itself without its env_laser (laserentity)',
    'monster_osprey': 'removes itself when the map has no human grunts to carry',
    'monster_tripmine': 'needs a wall to stick to',
    'monster_satchel': 'a placed satchel charge is gone before the test runs',
    'monster_snark': 'a placed snark is gone before the test runs',
    'trigger_condition': 'the snapshot does not find it by its name (none of the three copies)',
}

# Keys every copy gets (left out of the check), and why
KEEP = {
    # keys without an FGD default that the class needs to exist at all (else it removes itself at spawn)
    'ambient_generic': ({'message': 'ambience/drips.wav'}, 'needs a sound'),
    'ambient_random': ({'message': 'ambience/drips.wav'}, 'needs a sound'),
    'ambient_extraspeaker': ({'message': 'ambience/drips.wav'}, 'needs a sound'),
    'speaker': ({'message': 'ambience/drips.wav'}, 'needs a sound'),
    'env_global': ({'globalstate': 'et_defaults'}, 'needs a global state name'),
    'trigger_condition': ({'m_iszSourceName': 'x', 'm_iszSourceKey': 'health', 'm_iszCheckValue': '1'},
                          'needs a value to check'),
    'env_shooter': ({'shootmodel': 'models/hgibs.mdl'}, 'needs a shot model'),
    'cycler': ({'model': 'models/barney.mdl'}, 'needs a model'),
    # the FGD default model is the editor model, which the game refuses (UTIL_MapperModelUnusable): a model left
    # out and the default both remove the entity
    'env_model': ({'model': 'models/barney.mdl'}, 'the default editor model is refused in game'),
    'env_static': ({'model': 'models/barney.mdl'}, 'the default editor model is refused in game'),
    'monster_generic': ({'model': 'models/barney.mdl'}, 'the default editor model is refused in game'),
    'monster_furniture': ({'model': 'models/barney.mdl'}, 'the default editor model is refused in game'),
}

# Saved fields that differ between two copies for their names, not their keys
IGNORE = ('targetname', 'model', 'modelindex',
          # a monster dropped onto another copy at the same spot settles a little higher or lower
          'origin', 'absmin', 'absmax')

# Monsters: the run-time AI state (squads, schedules, idle animation, sight times) depends on the other copies in
# the room, and grunt heads / skins are drawn at random in spawn order; each copy gets its own squad name (netname)
MONSTER_IGNORE = ('netname', 'sequence', 'frame', 'animtime', 'm_flFrameRate', 'm_fSequenceLoops',
                  'm_fSequenceFinished', 'm_flLastEnemySightTime', 'm_afConditions', 'm_failSchedule',
                  'm_iScheduleIndex', 'm_flWaitFinished', 'm_flNextGrenadeCheck', 'm_iMySlot', 'm_voicePitch',
                  'm_hSquadMember', 'm_hSquadLeader', 'm_flNextIdleTime', 'm_flLastEventCheck', 'controller',
                  'm_headTurn', 'm_iSentence')

# Saved fields a class sets at random in Spawn (the Xen plants: their first think 0.1 .. 0.4 s, to spread the load):
# two copies differ there whatever their keys
RANDOM_IGNORE = {cn: ('animtime', 'nextthink', 'frame', 'framerate') for cn in
                 ('xen_spore_small', 'xen_spore_medium', 'xen_spore_large', 'xen_hair', 'xen_plantlight', 'xen_tree')}

# Classes checked key by key (the class-level check differed): filled from the class-level run
PER_KEY = {'env_elight', 'aiscripted_sequence', 'ambient_extraspeaker', 'ambient_generic', 'ambient_random', 'calc_numfromvec',
    'calc_position', 'calc_ratio', 'calc_subvelocity', 'calc_velocity_path', 'calc_velocity_polar', 'change_touch',
    'cycler_wreckage', 'dark_grunt', 'env_beam', 'env_beamtrail', 'env_blood', 'env_bubbles', 'env_dlight',
    'env_explosion', 'env_fade', 'env_fog', 'env_fogzone', 'env_glow', 'env_laser', 'env_mirror', 'env_model',
    'env_modeltrain', 'env_overlay', 'env_particle', 'env_quakefx', 'env_shake', 'env_shockwave', 'env_shooter',
    'env_sky', 'env_sound', 'env_soundmark', 'env_sprite', 'env_spritetrain', 'env_static', 'env_warpball',
    'func_breakable', 'func_button', 'func_door', 'func_door_rotating', 'func_friction', 'func_illusionary',
    'func_illusionary_toggle', 'func_mortar_field', 'func_pendulum', 'func_plat', 'func_platrot', 'func_pushable',
    'func_rainvolume', 'func_rot_button', 'func_rotating', 'func_snowvolume', 'func_tank', 'func_tankmortar',
    'func_tankrocket', 'func_tracktrain', 'func_train', 'func_vehicle', 'func_water', 'game_counter',
    'game_counter_set', 'game_player_hurt', 'game_score', 'game_text', 'gibshooter', 'light_environment',
    'locus_beam', 'locus_variable', 'momentary_rot_button', 'monster_dark', 'monster_grunt_hunter',
    'monster_grunt_hunter_repel', 'monster_grunt_repel', 'monster_human_grunt', 'monster_hunter',
    'monster_scientist', 'monster_scientist_dead', 'monster_sitting_scientist', 'monster_target',
    'monster_zombie_grunt', 'monster_zombie_hunter', 'monstermaker', 'scripted_action', 'scripted_following',
    'scripted_schedule', 'scripted_sentence', 'scripted_sequence', 'speaker', 'target_cdaudio', 'trigger_auto',
    'trigger_camera', 'trigger_cdaudio', 'trigger_compare', 'trigger_entity_iterator', 'trigger_gravity',
    'trigger_hurt', 'trigger_hurt_remote', 'trigger_look', 'trigger_multiple', 'trigger_push', 'trigger_relay',
    'trigger_sensor', 'trigger_timer', 'vgui_screen'}

# Keys a class needs to exist at all: given to every copy in the other keys' tests (its own test shows the finding)
REQUIRED = {'monstermaker': 'monstertype', 'env_sprite': 'model', 'env_glow': 'model', 'vgui_screen': 'panelname'}

# Stored differently, but the code gives the FGD default when the value is 0 / blank: (class, key) -> (saved fields
# left out of the comparison, where the code does it)
ACCEPT = {
    ('env_beamtrail', 'renderamt'): (('renderamt',), 'CEnvBeamTrail::RenderAmount: 0 gives 255'),
    ('env_shockwave', 'renderamt'): (('renderamt',), 'CEnvShockwave::RenderAmount: 0 gives 255'),
    ('env_warpball', 'amplitude'): (('friction',), 'CEnvWarpBall::Amplitude: 0 gives 6'),
    ('env_warpball', 'duration'): (('dmg_take',), 'CEnvWarpBall::Duration: 0 gives 1'),
    ('env_warpball', 'frequency'): (('dmg_save',), 'CEnvWarpBall::Frequency: 0 gives 160'),
    ('env_warpball', 'framerate'): (('framerate',), 'CEnvWarpBall::SpriteFramerate: 0 gives 12'),
    ('env_soundmark', 'type'): (('impulse',), 'env_soundmark: 0 gives bits_SOUND_DANGER (32)'),
    ('ambient_extraspeaker', 'target'): (('target',), 'ambient_extraspeaker: blank is the same as *player'),
    # body -1 (random head): a key left out always gives head 0 (a finding), but one random draw can be 0 too, so the
    # field is not compared
    ('monster_dark', 'body'): (('body',), 'random head: not comparable in one draw'),
    ('monster_scientist', 'body'): (('body',), 'random head: not comparable in one draw'),
    ('monster_scientist_dead', 'body'): (('body', 'skin'), 'random head: not comparable in one draw'),
    ('monster_sitting_scientist', 'body'): (('body',), 'random head: not comparable in one draw'),
}

# (class, key) -> the saved fields with the FGD default and with the key left out, for keys whose left-out value
# maps rely on (stock classes, and the few 2.x keys some map leaves out): the code keeps it and the key's FGD help
# says what leaving it out gives ("If left empty, ..."). The test then checks that the copy without the key differs from
# the one with the default (or is gone); without that sentence in the help it checks they are the same, and fails. The 2.x classes no map leaves a key out of get their FGD defaults
# in the game (cbase.cpp s_FgdDefaults) and are checked like every other key.
LEFT_OUT = {
    ('monster_grunt_repel', 'sequence'): 'Preview Sequence (editor only): saved sequence 48 (default) / 0 (left out); '
                                         'the repel entity is not drawn and the Fed it makes plays its own animation',
    ('monster_grunt_hunter_repel', 'sequence'): 'Preview Sequence (editor only): saved sequence 48 (default) / 0 (left '
                                                'out); the repel entity is not drawn and the Fed it makes plays its '
                                                'own animation',
    ('monstermaker', 'monstertype'): 'left out, the entity removes itself at spawn; with the FGD default '
                                     '"monster_apache" it stays',
    ('vgui_screen', 'health'): 'saved: health 10 (default) / 0 (left out)',
    ('env_sprite', 'framerate'): 'saved: framerate 10 (default) / 0 (left out)',
    ('env_glow', 'scale'): 'saved: scale 1 (default) / 0 (left out)',
    ('change_touch', 'model'): 'left out, the entity removes itself at spawn; with the FGD default "models/gman.mdl" it stays',
    ('vgui_screen', 'panelname'): 'left out, the entity removes itself at spawn; with the FGD default "example_screen" it stays',
    ('func_friction', 'modifier'): 'saved: m_frictionFraction 0.15 (default) / 0 (left out)',
    ('func_pushable', 'health'): 'saved: health 1 (default) / 0 (left out)',
    ('func_pushable', 'friction'): 'saved: m_maxSpeed 350 (default) / 400 (left out)',
    ('func_pushable', 'buoyancy'): 'saved: skin 11 (default) / 0 (left out)',
    ('momentary_rot_button', 'speed'): 'saved: speed 50 (default) / 100 (left out)',
    ('momentary_rot_button', 'distance'): 'saved: m_end 0,90,0 (default) / 0 (left out); m_flMoveDistance 90 (default) / 0 (left out)',
    ('light_environment', 'pitch'): 'saved: angles -90,0,0 (default) / 0 (left out)',
    ('target_cdaudio', 'health'): 'saved: health -1 (default) / 0 (left out)',
    ('env_sprite', 'model'): 'left out, the entity removes itself at spawn; with the FGD default "sprites/glow01.spr" it stays',
    ('func_tracktrain', 'wheels'): 'saved: m_length 50 (default) / 0 (left out)',
    ('func_tracktrain', 'height'): 'saved: m_height 4 (default) / 0 (left out)',
    ('func_tracktrain', 'speed'): 'saved: impulse 64 (default) / 100 (left out); m_speed 64 (default) / 100 (left out)',
    ('game_counter', 'health'): 'saved: health 10 (default) / 0 (left out)',
    ('trigger_multiple', 'wait'): 'saved: m_flWait 10 (default) / 0.2 (left out)',
    ('ambient_generic', 'health'): 'saved: health 10 (default) / 0 (left out); m_dpv 0,0,0,0,0,100,0,0,0,0,0,0,0,0,1,100,0,0,25600,100,0,0,25600,0,0 (default) / 0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,100,0,0,25600,0,0,0,0,0,0 (left out)',
    ('ambient_generic', 'pitch'): 'saved: m_dpv 0,100,0,0,0,0,0,0,0,0,0,0,0,0,1,100,0,0,25600,0,0,0,0,0,0 (default) / 0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,100,0,0,25600,0,0,0,0,0,0 (left out)',
    ('ambient_generic', 'pitchstart'): 'saved: m_dpv 0,0,100,0,0,0,0,0,0,0,0,0,0,0,1,100,0,0,25600,0,0,0,0,0,0 (default) / 0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,100,0,0,25600,0,0,0,0,0,0 (left out)',
    ('env_blood', 'color'): 'saved: impulse 247 (default) / 0 (left out)',
    ('env_blood', 'amount'): 'saved: dmg 100 (default) / 0 (left out)',
    ('env_glow', 'model'): 'left out, the entity removes itself at spawn; with the FGD default "sprites/glow01.spr" it stays',
    ('func_breakable', 'health'): 'saved: health 1 (default) / 0 (left out)',
    ('func_illusionary', 'skin'): 'saved: skin -1 (default) / 0 (left out)',
    ('func_rot_button', 'speed'): 'saved: speed 50 (default) / 40 (left out)',
    ('func_rot_button', 'sounds'): 'saved: m_sounds 21 (default) / 0 (left out); noise "buttons/lever1.wav" (default) / "common/null.wav" (left out)',
    ('func_rot_button', 'wait'): 'saved: m_flWait 3 (default) / 1 (left out)',
    ('func_rot_button', 'distance'): 'saved: m_flMoveDistance 90 (default) / 0 (left out); m_vecAngle2 0,90,0 (default) / 0 (left out)',
    ('func_train', 'speed'): 'saved: speed 64 (default) / 100 (left out)',
    ('game_counter_set', 'frags'): 'saved: frags 10 (default) / 0 (left out)',
    ('dark_grunt', 'weapons'): 'saved: weapons 1 (default) / 3 (left out)',
    ('aiscripted_sequence', 'm_flRadius'): 'saved: m_flRadius 512 (default) / 0 (left out)',
    ('env_bubbles', 'density'): 'saved: m_density 2 (default) / 0 (left out)',
    ('env_bubbles', 'frequency'): 'saved: m_frequency 2 (default) / 0 (left out); nextthink 7.6 (default) / 8 (left out)',
    ('env_laser', 'renderamt'): 'saved: renderamt 100 (default) / 0 (left out)',
    ('env_laser', 'width'): 'saved: scale 20 (default) / 0 (left out)',
    ('env_laser', 'texture'): 'saved: angles -2220,1852,0 (default) / 0 (left out); effects 128 (default) / 0 (left out); flags 1.0842e-19 (default) / 0 (left out); m_flSizeScale 1 (default) / 0 (left out); m_pfnThink "_ZN6CLaser11StrikeThinkEv" (default) / ...',
    ('env_laser', 'TextureScroll'): 'saved: animtime 35 (default) / 0 (left out)',
    ('env_laser', 'damage'): 'saved: dmg 100 (default) / 0 (left out)',
    ('func_rotating', 'fanfriction'): 'saved: m_flFanFriction 0.2 (default) / 1 (left out)',
    ('game_player_hurt', 'dmg'): 'saved: dmg 999 (default) / 0 (left out)',
    ('monster_human_grunt', 'weapons'): 'saved: weapons 1 (default) / 3 (left out)',
    ('monster_zombie_grunt', 'weapons'): 'saved: weapons 1 (default) / 3 (left out)',
    ('scripted_sequence', 'm_flRadius'): 'saved: m_flRadius 512 (default) / 0 (left out)',
    ('trigger_gravity', 'gravity'): 'saved: gravity 1 (default) / 0 (left out)',
    ('env_sound', 'radius'): 'saved: m_flRadius 128 (default) / 0 (left out)',
    ('env_shake', 'amplitude'): 'saved: scale 4 (default) / 0 (left out)',
    ('env_shake', 'radius'): 'saved: dmg 500 (default) / 0 (left out)',
    ('env_shake', 'duration'): 'saved: dmg_take 1 (default) / 0 (left out)',
    ('env_shake', 'frequency'): 'saved: dmg_save 2.5 (default) / 0 (left out)',
    ('game_text', 'x'): 'saved: m_textParms -1,0,0,0,0,0,0,0,0,0 (default) / 0 (left out)',
    ('game_text', 'y'): 'saved: m_textParms 0,-1,0,0,0,0,0,0,0,0 (default) / 0 (left out)',
    ('game_text', 'color'): 'saved: m_textParms 0,0,0,9.21956e-39,0,0,0,0,0,0 (default) / 0 (left out)',
    ('game_text', 'color2'): 'saved: m_textParms 0,0,0,0,28400,0,0,0,0,0 (default) / 0 (left out)',
    ('game_text', 'fadein'): 'saved: m_textParms 0,0,0,0,0,1.5,0,0,0,0 (default) / 0 (left out)',
    ('game_text', 'fadeout'): 'saved: m_textParms 0,0,0,0,0,0,0.5,0,0,0 (default) / 0 (left out)',
    ('game_text', 'holdtime'): 'saved: m_textParms 0,0,0,0,0,0,0,1.2,0,0 (default) / 0 (left out)',
    ('game_text', 'fxtime'): 'saved: m_textParms 0,0,0,0,0,0,0,0,0.25,0 (default) / 0 (left out)',
    ('game_text', 'channel'): 'saved: m_textParms 0,0,0,0,0,0,0,0,0,1 (default) / 0 (left out)',
    ('func_button', 'speed'): 'saved: speed 5 (default) / 40 (left out)',
    ('func_button', 'wait'): 'saved: m_flWait 3 (default) / 1 (left out)',
    ('func_mortar_field', 'm_flSpread'): 'saved: m_flSpread 64 (default) / 0 (left out)',
    ('func_mortar_field', 'm_iCount'): 'saved: m_iCount 1 (default) / 0 (left out)',
    ('func_tank', 'yawrate'): 'saved: m_yawRate 30 (default) / 0 (left out)',
    ('func_tank', 'yawrange'): 'saved: m_yawRange 180 (default) / 0 (left out)',
    ('func_tank', 'yawtolerance'): 'saved: m_yawTolerance 15 (default) / 0 (left out)',
    ('func_tank', 'pitchtolerance'): 'saved: m_pitchTolerance 5 (default) / 0 (left out)',
    ('func_tank', 'spritescale'): 'saved: m_spriteScale 1 (default) / 0 (left out)',
    ('func_tank', 'persistence'): 'saved: m_persist 1 (default) / 0 (left out)',
    ('scripted_sentence', 'duration'): 'saved: m_flDuration 3 (default) / 0 (left out)',
    ('scripted_sentence', 'radius'): 'saved: m_flRadius 512 (default) / 0 (left out)',
    ('scripted_sentence', 'refire'): 'saved: m_flRepeat 3 (default) / 0 (left out)',
    ('trigger_hurt', 'dmg'): 'saved: dmg 10 (default) / 0 (left out)',
    ('trigger_push', 'speed'): 'saved: speed 40 (default) / 100 (left out)',
    ('speaker', 'health'): 'saved: health 5 (default) / 0 (left out)',
    ('func_pendulum', 'speed'): 'saved: speed 100 (default) / 0 (left out)',
    ('func_pendulum', 'distance'): 'saved: m_accel 55.5556 (default) / 0 (left out); m_center 0,45,0 (default) / 0 (left out); m_distance 90 (default) / 0 (left out); m_maxSpeed 100 (default) / 0 (left out); m_pfnUse "_ZN9CPendulum11PendulumUseEP11CBaseEntityS1_8...',
    ('func_tankmortar', 'yawrate'): 'saved: m_yawRate 30 (default) / 0 (left out)',
    ('func_tankmortar', 'yawrange'): 'saved: m_yawRange 180 (default) / 0 (left out)',
    ('func_tankmortar', 'yawtolerance'): 'saved: m_yawTolerance 15 (default) / 0 (left out)',
    ('func_tankmortar', 'pitchtolerance'): 'saved: m_pitchTolerance 5 (default) / 0 (left out)',
    ('func_tankmortar', 'spritescale'): 'saved: m_spriteScale 1 (default) / 0 (left out)',
    ('func_tankmortar', 'persistence'): 'saved: m_persist 1 (default) / 0 (left out)',
    ('func_tankmortar', 'iMagnitude'): 'saved: impulse 100 (default) / 0 (left out)',
    ('game_score', 'points'): 'saved: frags 1 (default) / 0 (left out)',
    ('monster_grunt_hunter', 'weapons'): 'saved: weapons 1 (default) / 3 (left out)',
    ('monstermaker', 'Ff'): 'saved: m_iszChildFf "0" (default) / 0 (left out)',
    ('monstermaker', 'monstercount'): 'saved: m_cNumMonsters -1 (default) / 0 (left out)',
    ('monstermaker', 'delay'): 'saved: m_flDelay 5 (default) / 0 (left out)',
    ('monstermaker', 'm_imaxlivechildren'): 'saved: m_iMaxLiveChildren 5 (default) / 0 (left out)',
    ('scripted_action', 'm_flRadius'): 'saved: m_flRadius 512 (default) / 0 (left out)',
    ('trigger_hurt_remote', 'delay'): 'saved: frags 1 (default) / 0 (left out)',
    ('cycler_wreckage', 'model'): 'saved: maxs 16,16,48 (default) / 0 (left out); mins -16,-16,-48 (default) / 0 (left out); size 32,32,96 (default) / 0 (left out)',
    ('cycler_wreckage', 'scale'): 'saved: scale 1 (default) / 0 (left out)',
    ('env_explosion', 'iMagnitude'): 'saved: m_iMagnitude 100 (default) / 0 (left out); m_spriteScale 30 (default) / 10 (left out)',
    ('env_model', 'scale'): 'saved: scale 1 (default) / 0 (left out)',
    ('env_shooter', 'm_iGibs'): 'saved: m_iGibCapacity 3 (default) / 0 (left out); m_iGibs 3 (default) / 0 (left out)',
    ('env_shooter', 'm_flVelocity'): 'saved: m_bVelocityKey 1 (default) / 0 (left out); m_flGibVelocity 200 (default) / 0 (left out)',
    ('env_shooter', 'm_flVariance'): 'saved: m_flVariance 0.15 (default) / 0 (left out)',
    ('env_shooter', 'm_flGibLife'): 'saved: m_flGibLife 4 (default) / 25 (left out)',
    ('env_shooter', 'm_fFriction'): 'saved: m_fFriction 0.55 (default) / 0 (left out)',
    ('func_door', 'wait'): 'saved: m_flWait 4 (default) / 0 (left out)',
    ('func_plat', 'speed'): 'saved: speed 50 (default) / 150 (left out)',
    ('func_tankrocket', 'yawrate'): 'saved: m_yawRate 30 (default) / 0 (left out)',
    ('func_tankrocket', 'yawrange'): 'saved: m_yawRange 180 (default) / 0 (left out)',
    ('func_tankrocket', 'yawtolerance'): 'saved: m_yawTolerance 15 (default) / 0 (left out)',
    ('func_tankrocket', 'pitchtolerance'): 'saved: m_pitchTolerance 5 (default) / 0 (left out)',
    ('func_tankrocket', 'spritescale'): 'saved: m_spriteScale 1 (default) / 0 (left out)',
    ('func_tankrocket', 'persistence'): 'saved: m_persist 1 (default) / 0 (left out)',
    ('func_water', 'wait'): 'saved: m_flWait 4 (default) / 0 (left out)',
    ('func_water', 'skin'): 'saved: skin -3 (default) / 0 (left out); solid 0 (default) / 4 (left out); spawnflags -0 (default) / 0 (left out)',
    ('func_water', 'WaveHeight'): 'saved: scale 0.4 (default) / 0 (left out)',
    ('trigger_camera', 'wait'): 'saved: m_flWait 10 (default) / 0 (left out)',
    ('trigger_camera', 'turnspeed'): 'saved: m_turnspeed 40 (default) / 0 (left out)',
    ('env_beam', 'renderamt'): 'saved: renderamt 100 (default) / 0 (left out)',
    ('env_beam', 'Radius'): 'saved: m_radius 256 (default) / 0 (left out)',
    ('env_beam', 'life'): 'saved: aiment 0 (default) / -1 (left out); flags 0 (default) / 1.0842e-19 (left out); m_flSizeScale 0 (default) / 1 (left out); m_life 1 (default) / 0 (left out); m_vecSizeMaxs 0 (default) / 300,-572,0 (left out); m_vecSizeMins...',
    ('env_beam', 'BoltWidth'): 'saved: m_boltWidth 20 (default) / 0 (left out); scale 20 (default) / 0 (left out)',
    ('env_beam', 'texture'): 'saved: dmgtime 1 (default) / 0 (left out); effects 128 (default) / 0 (left out); m_iszSpriteName "sprites/laserbeam.spr" (default) / 0 (left out); m_pfnThink 0 (default) / "_ZN11CBaseEntity10SUB_RemoveEv" (left out); m_pfnUse "...',
    ('env_beam', 'TextureScroll'): 'saved: animtime 35 (default) / 0 (left out); m_speed 35 (default) / 0 (left out)',
    ('env_beam', 'StrikeTime'): 'saved: m_restrike 1 (default) / 0 (left out)',
    ('env_fade', 'duration'): 'saved: dmg_take 2 (default) / 0 (left out)',
    ('env_fade', 'renderamt'): 'saved: renderamt 255 (default) / 0 (left out)',
    ('env_sky', 'frags'): 'saved: m_fActive 1 (default) / 0 (left out)',
    ('gibshooter', 'm_iGibs'): 'saved: m_iGibCapacity 3 (default) / 0 (left out); m_iGibs 3 (default) / 0 (left out)',
    ('gibshooter', 'm_flVelocity'): 'saved: m_bVelocityKey 1 (default) / 0 (left out); m_flGibVelocity 200 (default) / 0 (left out)',
    ('gibshooter', 'm_flVariance'): 'saved: m_flVariance 0.15 (default) / 0 (left out)',
    ('gibshooter', 'm_flGibLife'): 'saved: m_flGibLife 4 (default) / 25 (left out)',
    ('func_door_rotating', 'wait'): 'saved: m_flWait 4 (default) / 0 (left out)',
    ('func_door_rotating', 'distance'): 'saved: m_flMoveDistance 90 (default) / 0 (left out); m_vecAngle2 0,90,0 (default) / 0 (left out)',
    ('func_platrot', 'speed'): 'saved: speed 50 (default) / 150 (left out)',
    ('trigger_cdaudio', 'health'): 'saved: health -1 (default) / 0 (left out)',
}


def _help(cn, key):
    """The FGD help text of the class's key (the line that defines it for the class), '' without one."""
    sys.path.insert(0, os.path.join(ROOT, 'scripts'))
    import fgd
    classes, _ = fgd.parse_fgd(FGD_PATH)
    props = fgd.effective_props(classes, cn)
    prop = props.get(key) or props.get(key.lower())
    if not prop:
        return ''
    line = open(FGD_PATH, encoding='utf-8', errors='replace').read().split('\n')[prop['line'] - 1]
    m = re.match(r'\s*[^(]+\([^)]*\)\s*:\s*"[^"]*"\s*:\s*(?:"[^"]*"|[^:"]*)\s*:\s*"([^"]*)"', line)
    return m.group(1) if m else ''


def _tool_key(key):
    rules = json.load(open(os.path.join(ROOT, 'scripts', 'fgd_rules.json')))
    return any(fnmatch.fnmatch(key, pat) for pat in rules['tool_keys']['keys'])


def _monsters():
    sys.path.insert(0, os.path.join(ROOT, 'scripts'))
    import fgd
    classes, _ = fgd.parse_fgd(FGD_PATH)
    # the tripmine (a grenade) and the sitting scientist offer no Monster keys but keep a monster's run-time state
    return {cn for cn, c in classes.items() if c['kind'] != 'baseclass' and
            ('MonsterCore' in fgd.base_chain(classes, cn) or cn in ('monster_tripmine', 'monster_sitting_scientist'))}


MONSTERS = _monsters()


def plan():
    """[(class, kind, {key: default})] in FGD order, and the skipped classes [(class, reason)]."""
    sys.path.insert(0, os.path.join(ROOT, 'scripts'))
    import fgd
    classes, _ = fgd.parse_fgd(FGD_PATH)
    rules = json.load(open(os.path.join(ROOT, 'scripts', 'fgd_rules.json')))
    tool = rules['tool_keys']['keys']
    out, skipped = [], []
    for cn, c in classes.items():
        if c['kind'] == 'baseclass':
            continue
        keys = {}
        for k, p in fgd.effective_props(classes, cn).items():
            if p['default'] in (None, '') or p['type'] == 'flags' or k in ('targetname', 'origin'):
                continue
            if any(fnmatch.fnmatch(p['name'], pat) for pat in tool):
                continue
            keys[p['name']] = p['default']
        if not keys:
            continue
        if cn in SKIP:
            skipped.append((cn, SKIP[cn]))
            continue
        out.append((cn, c['kind'], keys))
    return out, skipped


def _place(t, cn, kind, name, keys):
    if kind == 'solidclass':
        e = t.brush(cn, size=(32, 32, 32), at=(0, 60, 0), targetname=name)
    else:
        e = t.point(cn, at=(0, 60, 0), targetname=name)
    e.kv.update(keys)       # the default values as the FGD writes them (no test name prefix)


def _make(suite, cn, kind, keys, key=None):
    sub = {key: keys[key]} if key else keys

    keep = dict(KEEP.get(cn, ({}, ''))[0])
    if key and cn in REQUIRED and key != REQUIRED[cn]:
        keep[REQUIRED[cn]] = keys[REQUIRED[cn]]
    sub = {k: v for k, v in sub.items() if k not in keep}
    covers = tuple('%s.%s' % (cn, k) for k in sub)

    monster = cn in MONSTERS
    ignore = IGNORE + RANDOM_IGNORE.get(cn, ())
    if monster:
        ignore = ignore + MONSTER_IGNORE + tuple(f for f in ('body', 'skin') if f != key)
        ignore = ignore + ('angles', 'ideal_yaw')    # idle turning (a monster's angles key is checked nowhere here)
    ignore = ignore + ACCEPT.get((cn, key), ((), ''))[0]
    # a LEFT_OUT key: its help must say what leaving it out gives, and the copy without it must differ (as recorded)
    differ = (cn, key) in LEFT_OUT and re.search(r'\bleft empty\b|\bleaving it empty\b', _help(cn, key).lower()) is not None

    def body(t):
        for name, keys_ in (('a', dict(sub, **keep)), ('b', dict(keep)), ('n', dict(keep))):
            _place(t, cn, kind, name, dict(keys_, **({'netname': 'sq_' + name} if monster else {})))
        t.wait(0.1)
        t.snap('s', 'a', 'save')
        t.snap('s', 'b', 'save')
        t.snap('s', 'n', 'save')
        t.expect.save_same('s', 'a', 'b', noise=('n', 'b'), ignore=ignore, differ=differ)

    if key:
        title = '%s %s left out = FGD default %s' % (cn, key, json.dumps(keys[key]))
        body.__name__ = '%s__%s' % (cn, key)
    else:
        title = '%s: every key left out = its FGD default (%d keys)' % (cn, len(keys))
        body.__name__ = cn
    suite.test(title, doc='FGD %s defaults' % cn, players=0, covers=covers)(body)


def generate(suite, part):
    """Adds part `part` (0 .. PARTS-1) of the generated tests to `suite`."""
    rows, _ = plan()
    for i, (cn, kind, keys) in enumerate(rows):
        if i % PARTS != part:
            continue
        if cn in PER_KEY:
            for k in keys:
                if k not in KEEP.get(cn, ({}, ''))[0]:
                    _make(suite, cn, kind, keys, k)
        else:
            _make(suite, cn, kind, keys)


if __name__ == '__main__':
    import signal
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    rows, skipped = plan()
    print('%d classes, %d keys with an FGD default' % (len(rows), sum(len(k) for _, _, k in rows)))
    for cn, kind, keys in rows:
        print('  %s%s: %s' % (cn, ' (key by key)' if cn in PER_KEY else '', ' '.join(sorted(keys))))
    print('left out:')
    for cn, why in skipped:
        print('  %s: %s' % (cn, why))
