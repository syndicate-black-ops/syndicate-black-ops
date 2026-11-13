"""Weapon pickups (sdk/syndicate.fgd "Weapons"): every weapon class a mapper can place. A stripped player who walks
onto it takes it (the pickup then follows the player), and taking it fires Target after Delay with the player as
activator and removes KillTarget (CBasePlayerItem::DefaultTouch, SUB_UseTargets). The world model the FGD shows is
the one the weapon precaches (its Spawn's SET_MODEL; weapon_wong has none)."""

from etest import Suite

SUITE = Suite('weapon_pickups', 'Weapon pickups: every placeable weapon_ class, Target, Delay and KillTarget')
S = SUITE
DOC = 'FGD Weapons'

# class -> the world model its Spawn sets (None: no model)
WEAPONS = [
    ('weapon_warhammer', 'models/w_crowbar.mdl'),
    ('weapon_chainsaw', 'models/w_katana.mdl'),
    ('weapon_sword', 'models/w_crowbar.mdl'),
    ('weapon_wong', None),
    ('weapon_rc28', 'models/w_rc28.mdl'),
    ('weapon_k444', 'models/w_k444.mdl'),
    ('weapon_canon', 'models/w_canon.mdl'),
    ('weapon_pm66', 'models/w_pm66.mdl'),
    ('weapon_spa', 'models/w_shotgun.mdl'),
    ('weapon_fa18', 'models/w_fa18.mdl'),
    ('weapon_nm33', 'models/w_nm33.mdl'),
    ('weapon_federal', 'models/w_federal.mdl'),
    ('weapon_ben', 'models/w_ben.mdl'),
    ('weapon_chaingun', 'models/w_mini.mdl'),
    ('weapon_glauncher', 'models/w_glauncher.mdl'),
    ('weapon_sw', 'models/w_rifle.mdl'),
    ('weapon_rpg', 'models/w_rpg.mdl'),
    ('weapon_handgrenade', 'models/w_grenade.mdl'),
    ('weapon_tripmine', 'models/v_tripmine.mdl'),
    ('weapon_snark', 'models/w_sqknest.mdl'),
    ('weapon_satchel', 'models/w_weaponbox.mdl'),
    ('weapon_medikit', 'models/w_medikit.mdl'),
]


def _make(cls, model):
    @S.test('%s: a stripped player who walks onto it takes it; Target fires after Delay with the player as '
            'activator and KillTarget is removed' % cls, players=1, doc=DOC,
            covers=(cls, cls + '.target', cls + '.delay', cls + '.killtarget'))
    def pickup(t):
        t.point('player_weaponstrip', targetname='strip')
        t.point('info_target', targetname='got')
        t.point('info_target', targetname='victim')
        t.point(cls, targetname='gun', at=(120, 0, 0), target='got', delay='0.5', killtarget='victim')
        t.teleport('player1', (-150, 0, 0))
        t.wait(1.0)                 # the pickup lands (FallThink) and the player settles
        t.fire('strip', activator='player1')
        t.wait(0.3)
        t.mark('touch')
        t.teleport('player1', (120, 0, 0))
        t.wait(0.3)
        t.snap('taken', 'gun', 'movetype')
        t.teleport('player1', (-150, 0, 0))
        t.wait(1.0)
        t.expect.value('taken', 'gun', 'movetype', 12)                 # MOVETYPE_FOLLOW: attached to the player
        t.expect.fired('got', activator='player1', after='touch', within=(0.45, 1.0), count=1)
        t.expect.removed('victim', after='touch', within=(0.45, 1.0), count=1)
        t.expect.not_fired('got', after='touch', within=(0, 0.45))
        if model:
            t.expect.precached(model, kind='model')
    pickup.__name__ = cls[len('weapon_'):]
    return pickup


for _cls, _model in WEAPONS:
    globals()['pickup_' + _cls[len('weapon_'):]] = _make(_cls, _model)
