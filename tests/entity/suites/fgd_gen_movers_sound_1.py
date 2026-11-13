"""Tier 5, generated: the sound choice values (part 2 of 3: scripted_sentence, env_message, ambient_generic,
env_warpball), from sdk/syndicate.fgd (tests/entity/etest/gen_movers.py: the sound each value plays, how each
class is driven)."""

from etest import Suite, gen_movers

SUITE = Suite('fgd_gen_movers_sound_1', 'Tier 5: sound choices generated from the FGD, part 2', players=1)
gen_movers.generate_sounds(SUITE, 1)
