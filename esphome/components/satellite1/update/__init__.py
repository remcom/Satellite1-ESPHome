from esphome import automation
import esphome.codegen as cg
from esphome.components import update
from esphome.components.memory_flasher import CONF_FLASHER_ID, MemoryFlasher
import esphome.config_validation as cv
from esphome.const import CONF_VERSION, DEVICE_CLASS_FIRMWARE

from .. import satellite1 as sat

CODEOWNERS = ["@remcom"]
DEPENDENCIES = ["satellite1", "memory_flasher"]

CONF_RELEASE_URL = "release_url"
CONF_ON_INSTALL = "on_install"

XMOSUpdate = sat.namespace.class_(
    "XMOSUpdate",
    update.UpdateEntity,
    cg.Component,
    cg.Parented.template(sat.Satellite1),
)

CONFIG_SCHEMA = (
    update.update_schema(XMOSUpdate, device_class=DEVICE_CLASS_FIRMWARE)
    .extend(
        {
            cv.GenerateID(sat.CONF_SATELLITE1): cv.use_id(sat.Satellite1),
            cv.GenerateID(CONF_FLASHER_ID): cv.use_id(MemoryFlasher),
            # XMOS release this build installs, e.g. v2.0.0-beta.2
            cv.Required(CONF_VERSION): cv.string_strict,
            cv.Optional(CONF_RELEASE_URL): cv.url,
            # Installs the selected variant; called when Install is pressed in Home Assistant
            cv.Required(CONF_ON_INSTALL): automation.validate_automation({}),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_INSTALL, "add_on_install_callback"),
)


async def to_code(config):
    var = await update.new_update(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[sat.CONF_SATELLITE1])

    flasher = await cg.get_variable(config[CONF_FLASHER_ID])
    cg.add(var.set_flasher(flasher))
    cg.add(var.set_target_version(config[CONF_VERSION]))
    if release_url := config.get(CONF_RELEASE_URL):
        cg.add(var.set_release_url(release_url))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
