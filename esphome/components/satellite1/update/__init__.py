import esphome.codegen as cg
from esphome.components import micro_wake_word, update
from esphome.components.http_request.update import HttpRequestUpdate
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE_ID

CONF_MICRO_WAKE_WORD_ID = "micro_wake_word_id"

DEPENDENCIES = ["update.http_request", "micro_wake_word"]

Satellite1Update = cg.esphome_ns.namespace("satellite1").class_(
    "Satellite1Update", update.UpdateEntity, cg.Component
)

CONFIG_SCHEMA = (
    update.update_schema(Satellite1Update)
    .extend(
        {
            cv.Required(CONF_SOURCE_ID): cv.use_id(HttpRequestUpdate),
            cv.Required(CONF_MICRO_WAKE_WORD_ID): cv.use_id(micro_wake_word.MicroWakeWord),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await update.new_update(config)
    await cg.register_component(var, config)
    source = await cg.get_variable(config[CONF_SOURCE_ID])
    cg.add(var.set_source(source))
    mww = await cg.get_variable(config[CONF_MICRO_WAKE_WORD_ID])
    cg.add(var.set_micro_wake_word(mww))
