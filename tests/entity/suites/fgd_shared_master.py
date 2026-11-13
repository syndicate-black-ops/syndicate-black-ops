"""Tier 5, shared keys: master and triggerstate on every FGD class that offers them, generated from sdk/syndicate.fgd
(tests/entity/etest/shared_keys.py: which classes, how each is activated, the classes left out and why)."""

from etest import Suite, shared_keys

SUITE = Suite('fgd_shared_master', 'Tier 5: shared keys generated from the FGD (master and triggerstate)', players=1)
shared_keys.generate(SUITE, ('master', 'triggerstate'))
