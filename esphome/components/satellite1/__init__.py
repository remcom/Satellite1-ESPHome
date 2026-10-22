# sat_gpio is imported for its side effect: it registers the satellite1 pin schema.
from . import sat_gpio, satellite1 as sat  # noqa: F401

CODEOWNERS = ["@gnumpi"]


DEPENDENCIES = ["spi"]


CONFIG_SCHEMA = sat.SAT1_CONFIG_SCHEMA


async def to_code(config):
    return await sat.register_satellite1(config)
