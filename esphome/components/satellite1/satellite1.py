from esphome import automation, pins
import esphome.codegen as cg
from esphome.components.spi import SPIDevice, register_spi_device, spi_device_schema
import esphome.config_validation as cv
from esphome.const import CONF_ID

namespace = cg.esphome_ns.namespace("satellite1")
Satellite1 = namespace.class_("Satellite1", SPIDevice, cg.Component)
Satellite1SPIService = namespace.class_(
    "Satellite1SPIService", cg.Parented.template(Satellite1)
)


XMOSHardwareResetAction = namespace.class_("XMOSHardwareResetAction", automation.Action)

Satellite1State = namespace.enum("Satellite1State")


CONF_SATELLITE1 = "satellite1"
CONF_XMOS_RST_PIN = "xmos_rst_pin"
CONF_ON_XMOS_NO_RESPONSE = "on_xmos_no_response"
CONF_ON_XMOS_CONNECTED = "on_xmos_connected"
CONF_ON_FLASH_CONNECTED = "on_flash_connected"
CONF_ON_XMOS_DETACHED = "on_xmos_detached"

SAT1_CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Satellite1),
        cv.Optional(CONF_XMOS_RST_PIN, default="GPIO12"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_ON_XMOS_CONNECTED): automation.validate_automation({}),
        cv.Optional(CONF_ON_FLASH_CONNECTED): automation.validate_automation({}),
        cv.Optional(CONF_ON_XMOS_NO_RESPONSE): automation.validate_automation({}),
        cv.Optional(CONF_ON_XMOS_DETACHED): automation.validate_automation({}),
    }
).extend(spi_device_schema(True, "1Hz"))


_STATE_PARAMS = [(Satellite1State, "state"), (cg.uint8, "connection_attempts")]
_DETACHED = "state == satellite1::SAT_DETACHED_STATE"

# Each state trigger fires on the state change described by its filter.
_STATE_TRIGGERS = (
    (CONF_ON_XMOS_CONNECTED, "state == satellite1::SAT_XMOS_CONNECTED_STATE"),
    (CONF_ON_FLASH_CONNECTED, "state == satellite1::SAT_FLASH_CONNECTED_STATE"),
    # Detection gave up.
    (
        CONF_ON_XMOS_NO_RESPONSE,
        f"{_DETACHED} && connection_attempts == satellite1::MAX_CONNECTION_ATTEMPTS",
    ),
    # The XMOS dropped off (reset, leaving flash access mode) and detection restarts.
    (CONF_ON_XMOS_DETACHED, f"{_DETACHED} && connection_attempts == 0"),
)

_CALLBACK_AUTOMATIONS = tuple(
    automation.CallbackAutomation(
        key, "add_on_state_callback", params=_STATE_PARAMS, forward=[], when=when
    )
    for key, when in _STATE_TRIGGERS
)


async def register_satellite1(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_spi_device(var, config)

    rst_pin = await cg.gpio_pin_expression(config[CONF_XMOS_RST_PIN])
    cg.add(var.set_xmos_rst_pin(rst_pin))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)

    return var


RESET_XMOS_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(CONF_SATELLITE1): cv.use_id(Satellite1)}
)


@automation.register_action(
    "satellite1.xmos_hardware_reset",
    XMOSHardwareResetAction,
    RESET_XMOS_ACTION_SCHEMA,
    synchronous=False,
)
async def xmos_hardware_reset_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_SATELLITE1])
    return var
