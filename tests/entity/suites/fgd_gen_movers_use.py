"""Tier 5, generated: every Fire On use type and Obey Trigger Mode choice of the movers with a recipe, from
sdk/syndicate.fgd (tests/entity/etest/gen_movers.py: which classes, how each is driven, the classes left out and
why)."""

from etest import Suite, gen_movers

SUITE = Suite('fgd_gen_movers_use', 'Tier 5: Fire On use types and Obey Trigger Mode generated from the FGD', players=1)
gen_movers.generate_use_types(SUITE)
