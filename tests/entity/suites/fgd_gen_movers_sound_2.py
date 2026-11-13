"""Tier 5, generated: the sound choice values (part 3 of 3: speaker, alone: a talking speaker or monster holds every
speaker for 5 s), from sdk/syndicate.fgd (tests/entity/etest/gen_movers.py: the sentence group of each preset)."""

from etest import Suite, gen_movers

SUITE = Suite('fgd_gen_movers_sound_2', 'Tier 5: sound choices generated from the FGD, part 3', players=1)
gen_movers.generate_sounds(SUITE, 2)
