"""Tier 5, FGD defaults check part 2 of 8: generated from sdk/syndicate.fgd (tests/entity/etest/defaults.py:
which classes, how they are compared, the classes left out and why)."""

from etest import Suite, defaults

SUITE = Suite('fgd_defaults_1', 'Tier 5: FGD defaults check, part 2', players=1)
defaults.generate(SUITE, 1)
