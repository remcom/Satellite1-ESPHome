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
Satellite1StateForwarder = namespace.class_("Satellite1StateForwarder")
XMOSDetachedForwarder = namespace.class_("XMOSDetachedForwarder")
XMOSNoResponseForwarder = namespace.class_("XMOSNoResponseForwarder")


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


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_XMOS_CONNECTED,
        "add_on_state_callback",
        forwarder=Satellite1StateForwarder.template(
            Satellite1State.SAT_XMOS_CONNECTED_STATE
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_FLASH_CONNECTED,
        "add_on_state_callback",
        forwarder=Satellite1StateForwarder.template(
            Satellite1State.SAT_FLASH_CONNECTED_STATE
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_XMOS_NO_RESPONSE,
        "add_on_state_callback",
        forwarder=XMOSNoResponseForwarder,
    ),
    automation.CallbackAutomation(
        CONF_ON_XMOS_DETACHED,
        "add_on_state_callback",
        forwarder=XMOSDetachedForwarder,
    ),
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
