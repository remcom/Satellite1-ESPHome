from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
)
from esphome.types import ConfigType

from . import satellite1 as sat

Satellite1GPIOPin = sat.namespace.class_(
    "Satellite1GPIOPin",
    cg.GPIOPin,
    cg.Parented.template(sat.Satellite1),
)

# 8 pins per XMOS port: 0-7 INPUT_A (buttons), 8-15 INPUT_B (rotary encoder),
# 16-23 OUTPUT_A (explorer board LEDs). Matches XMOS_GPIO_PINS_PER_PORT in satellite1.h.
XMOS_GPIO_FIRST_OUTPUT_PIN = 16


def _validate_output_pin(config: ConfigType) -> ConfigType:
    # The input ports are read only, writes would only log an error at runtime
    if (
        config[CONF_MODE][CONF_OUTPUT]
        and config[CONF_NUMBER] < XMOS_GPIO_FIRST_OUTPUT_PIN
    ):
        raise cv.Invalid(
            f"Pin {config[CONF_NUMBER]} is on a read only port, only pins "
            f"{XMOS_GPIO_FIRST_OUTPUT_PIN}-23 can be used as output",
            path=[CONF_NUMBER],
        )
    return config


PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        Satellite1GPIOPin,
        cv.int_range(min=0, max=23),
        modes=(CONF_INPUT, CONF_OUTPUT),
    ).extend(
        {
            cv.Required(sat.CONF_SATELLITE1): cv.use_id(sat.Satellite1),
        }
    ),
    _validate_output_pin,
)


@pins.PIN_SCHEMA_REGISTRY.register(sat.CONF_SATELLITE1, PIN_SCHEMA)
async def satellite1_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[sat.CONF_SATELLITE1])

    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))

    return var
