import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components.audio_dac import AudioDac, audio_dac_ns
from esphome.const import CONF_ID

CODEOWNERS = ["@remcom"]
DEPENDENCIES = []

dac_switcher_ns = cg.esphome_ns.namespace("dac_switcher")
DacSwitcher = dac_switcher_ns.class_("DacSwitcher", AudioDac, cg.Component)

# Automation actions
SelectTas2780Action = dac_switcher_ns.class_(
    "SelectTas2780Action", automation.Action, cg.Parented.template(DacSwitcher)
)
SelectPcm5122Action = dac_switcher_ns.class_(
    "SelectPcm5122Action", automation.Action, cg.Parented.template(DacSwitcher)
)

CONF_TAS2780 = "tas2780"
CONF_PCM5122 = "pcm5122"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DacSwitcher),
            cv.Optional(CONF_TAS2780): cv.use_id(AudioDac),
            cv.Optional(CONF_PCM5122): cv.use_id(AudioDac),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

# Action schema for switching DACs
DAC_SWITCHER_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DacSwitcher),
    }
)


@automation.register_action(
    "dac_switcher.select_tas2780", SelectTas2780Action, DAC_SWITCHER_ACTION_SCHEMA
)
async def dac_switcher_select_tas2780_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "dac_switcher.select_pcm5122", SelectPcm5122Action, DAC_SWITCHER_ACTION_SCHEMA
)
async def dac_switcher_select_pcm5122_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_TAS2780 in config:
        tas2780 = await cg.get_variable(config[CONF_TAS2780])
        cg.add(var.set_tas2780(tas2780))

    if CONF_PCM5122 in config:
        pcm5122 = await cg.get_variable(config[CONF_PCM5122])
        cg.add(var.set_pcm5122(pcm5122))
