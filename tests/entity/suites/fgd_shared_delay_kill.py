"""Tier 5, shared keys: delay and killtarget on every FGD class that offers them, generated from sdk/syndicate.fgd
(tests/entity/etest/shared_keys.py: which classes, how each is activated, the classes left out and why)."""

from etest import Suite, shared_keys

SUITE = Suite('fgd_shared_delay_kill', 'Tier 5: shared keys generated from the FGD (delay and killtarget)', players=1)
shared_keys.generate(SUITE, ('delay', 'killtarget'))
