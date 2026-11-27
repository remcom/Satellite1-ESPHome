import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_DECIBEL,
    UNIT_DEGREES,
    UNIT_PERCENT,
)

from .. import satellite1 as sat

CODEOWNERS = ["@remcom", "@gnumpi"]

DEPENDENCIES = ["satellite1"]

Satellite1DoaSensor = sat.namespace.class_(
    "Satellite1DoaSensor", cg.PollingComponent, cg.Parented.template(sat.Satellite1)
)
Satellite1AudioQualitySensor = sat.namespace.class_(
    "Satellite1AudioQualitySensor", cg.PollingComponent, cg.Parented.template(sat.Satellite1)
)

TYPE_DOA = "doa"
TYPE_AUDIO_QUALITY = "audio_quality"

CONF_DIRECTION = "direction"
CONF_CONFIDENCE = "confidence"
CONF_ECHO_REDUCTION = "echo_reduction"
CONF_ECHO_REDUCTION_WORST = "echo_reduction_worst"
CONF_REFERENCE_DROPS = "reference_drops"
CONF_CLIPPED_SAMPLES_CH0 = "clipped_samples_ch0"
CONF_CLIPPED_SAMPLES_CH1 = "clipped_samples_ch1"


def _counter(icon):
    return sensor.sensor_schema(
        icon=icon,
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


def _echo_reduction():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_DECIBEL,
        icon="mdi:speaker-off",
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


DOA_SCHEMA = cv.Schema(
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

AUDIO_QUALITY_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Satellite1AudioQualitySensor),
        cv.GenerateID(sat.CONF_SATELLITE1): cv.use_id(sat.Satellite1),
        cv.Optional(CONF_ECHO_REDUCTION): _echo_reduction(),
        cv.Optional(CONF_ECHO_REDUCTION_WORST): _echo_reduction(),
        cv.Optional(CONF_REFERENCE_DROPS): _counter("mdi:speaker-message"),
        cv.Optional(CONF_CLIPPED_SAMPLES_CH0): _counter("mdi:waveform"),
        cv.Optional(CONF_CLIPPED_SAMPLES_CH1): _counter("mdi:waveform"),
    }
).extend(cv.polling_component_schema("10s"))

# Without a type the platform is the direction of arrival sensor, as before.
CONFIG_SCHEMA = cv.typed_schema(
    {TYPE_DOA: DOA_SCHEMA, TYPE_AUDIO_QUALITY: AUDIO_QUALITY_SCHEMA},
    key=CONF_TYPE,
    default_type=TYPE_DOA,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[sat.CONF_SATELLITE1])

    sensors = sensor.sub_sensors(config)
    if config.get(CONF_TYPE, TYPE_DOA) == TYPE_AUDIO_QUALITY:
        await sensors(CONF_ECHO_REDUCTION, var.set_echo_reduction_sensor)
        await sensors(CONF_ECHO_REDUCTION_WORST, var.set_echo_reduction_worst_sensor)
        await sensors(CONF_REFERENCE_DROPS, var.set_reference_drops_sensor)
        await sensors(CONF_CLIPPED_SAMPLES_CH0, var.set_clipped_samples_ch0_sensor)
        await sensors(CONF_CLIPPED_SAMPLES_CH1, var.set_clipped_samples_ch1_sensor)
    else:
        await sensors(CONF_DIRECTION, var.set_direction_sensor)
        await sensors(CONF_CONFIDENCE, var.set_confidence_sensor)
