"""Tier 5, generated: trigger, game, path, button and train flags (part 2 of 2: trigger_teleport, trigger_copyvalue,
trigger_camera, trigger_sequence, game_*, path_track, pendulum, buttons, trains), from sdk/syndicate.fgd
(tests/entity/etest/gen_movers.py)."""

from etest import Suite, gen_movers

SUITE = Suite('fgd_gen_triggers_flags_1', 'Tier 5: trigger and game flags generated from the FGD, part 2', players=1)
gen_movers.generate_flags(SUITE, 1)
