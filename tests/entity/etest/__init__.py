"""Automated entity tests (docs/dev/ENTITY_TESTS.md): suites of small entity setups run on a headless stock HLDS
with the game's test layer (SBO_TEST_COMMANDS), checked against the trace it writes."""

from .dsl import Suite  # noqa: F401
