"""Tier 5, shared keys: target on every FGD class that offers them, generated from sdk/syndicate.fgd
(tests/entity/etest/shared_keys.py: which classes, how each is activated, the classes left out and why)."""

from etest import Suite, shared_keys

SUITE = Suite('fgd_shared_target', 'Tier 5: shared keys generated from the FGD (target)', players=1)
shared_keys.generate(SUITE, ('target',))
