"""Tier 5, generated: the sound choice values (part 1 of 3: func_trackautochange, func_tracktrain), from
sdk/syndicate.fgd (tests/entity/etest/gen_movers.py: the sound each value plays, how each class is driven)."""

from etest import Suite, gen_movers

SUITE = Suite('fgd_gen_movers_sound_0', 'Tier 5: sound choices generated from the FGD, part 1', players=1)
gen_movers.generate_sounds(SUITE, 0)
