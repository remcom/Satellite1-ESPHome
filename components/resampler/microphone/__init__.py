import esphome.codegen as cg
from esphome.components import audio, esp32, microphone
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_DURATION,
    CONF_CHANNELS,
    CONF_FILTERS,
    CONF_ID,
    CONF_MICROPHONE,
    CONF_SAMPLE_RATE,
    CONF_TASK_STACK_IN_PSRAM,
    PLATFORM_ESP32,
)

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@kahrendt"]

resampler_ns = cg.esphome_ns.namespace("resampler")
ResamplerMicrophone = resampler_ns.class_(
    "ResamplerMicrophone", cg.Component, microphone.Microphone
)

CONF_TAPS = "taps"


def _set_stream_limits(config):
    # Set the max channels to the length of the the channels lists for downstream components to validate against.
    # The specifically listed channels will be validated in this component's FINAL_VALIDATE_SCHEMA
    audio.set_stream_limits(
        max_channels=len(config.get(CONF_MICROPHONE).get(CONF_CHANNELS)),
        min_sample_rate=config.get(CONF_SAMPLE_RATE),
        max_sample_rate=config.get(CONF_SAMPLE_RATE),
    )(config)

    return config


def _validate_taps(taps):
    value = cv.int_range(min=16, max=128)(taps)
    if value % 4 != 0:
        raise cv.Invalid("Number of taps must be divisible by 4")
    return value


CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ResamplerMicrophone),
            cv.Optional(CONF_MICROPHONE): microphone.microphone_source_schema(
                min_channels=1,
                max_channels=3,  # Not a technical limit but practical one for computational load
            ),
            cv.Optional(
                CONF_BUFFER_DURATION, default="100ms"
            ): cv.positive_time_period_milliseconds,
            cv.SplitDefault(CONF_TASK_STACK_IN_PSRAM, esp32_idf=False): cv.All(
                cv.boolean, cv.only_on_esp32
            ),
            cv.Optional(CONF_FILTERS, default=16): cv.int_range(min=2, max=1024),
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(8000, 48000),
            cv.Optional(CONF_TAPS, default=16): _validate_taps,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32]),
    _set_stream_limits,
)


FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(
                CONF_MICROPHONE
            ): microphone.final_validate_microphone_source_schema(
                "resampler_microphone"
            ),
        },
        extra=cv.ALLOW_EXTRA,
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)

    mic_source = await microphone.microphone_source_to_code(config[CONF_MICROPHONE])
    cg.add(var.set_microphone_source(mic_source))

    cg.add(var.set_buffer_duration(config[CONF_BUFFER_DURATION]))

    if task_stack_in_psram := config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(task_stack_in_psram))
        if task_stack_in_psram:
            esp32.add_idf_sdkconfig_option(
                "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
            )

    cg.add(var.set_target_sample_rate(config[CONF_SAMPLE_RATE]))

    cg.add(var.set_filters(config[CONF_FILTERS]))
    cg.add(var.set_taps(config[CONF_TAPS]))
