import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREES,
    UNIT_PERCENT,
)

from .. import satellite1 as sat

CODEOWNERS = ["@remcom", "@gnumpi"]

DEPENDENCIES = ["satellite1"]

Satellite1DoaSensor = sat.namespace.class_(
    "Satellite1DoaSensor", cg.PollingComponent, cg.Parented.template(sat.Satellite1)
)

CONF_DIRECTION = "direction"
CONF_CONFIDENCE = "confidence"


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Satellite1DoaSensor),
        cv.GenerateID(sat.CONF_SATELLITE1): cv.use_id(sat.Satellite1),
        cv.Optional(CONF_DIRECTION): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            icon="mdi:compass-outline",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_CONFIDENCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:target",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[sat.CONF_SATELLITE1])

    if direction_config := config.get(CONF_DIRECTION):
        sens = await sensor.new_sensor(direction_config)
        cg.add(var.set_direction_sensor(sens))
    if confidence_config := config.get(CONF_CONFIDENCE):
        sens = await sensor.new_sensor(confidence_config)
        cg.add(var.set_confidence_sensor(sens))
