"""Tier 5, generated: trigger, game, path, button and train flags (part 1 of 2: trigger_inout and trigger_jump,
each flag with its kind of toucher), from sdk/syndicate.fgd
(tests/entity/etest/gen_movers.py)."""

from etest import Suite, gen_movers

SUITE = Suite('fgd_gen_triggers_flags_0', 'Tier 5: trigger and game flags generated from the FGD, part 1', players=1)
gen_movers.generate_flags(SUITE, 0)
