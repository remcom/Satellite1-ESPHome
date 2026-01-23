import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import i2s_audio

CODEOWNERS = ["@custom"]
DEPENDENCIES = ["esp32"]
MULTI_CONF = True

CONF_I2S_PORT = "i2s_port"
CONF_BCLK_PIN = "bclk_pin"
CONF_LRCLK_PIN = "lrclk_pin"
CONF_MCLK_PIN = "mclk_pin"
CONF_SAMPLE_RATE = "sample_rate"
CONF_BITS_PER_SAMPLE = "bits_per_sample"
CONF_CHANNEL_FORMAT = "channel_format"
CONF_USE_APLL = "use_apll"
CONF_BUFFER_COUNT = "buffer_count"
CONF_BUFFER_LENGTH = "buffer_length"

shared_i2s_audio_ns = cg.esphome_ns.namespace("shared_i2s_audio")
SharedI2SAudioComponent = shared_i2s_audio_ns.class_(
    "SharedI2SAudioComponent", cg.Component
)

i2s_port_t = cg.global_ns.enum("i2s_port_t")
I2S_PORTS = {
    0: i2s_port_t.I2S_NUM_0,
    1: i2s_port_t.I2S_NUM_1,
}

i2s_bits_per_sample_t = cg.global_ns.enum("i2s_bits_per_sample_t")
BITS_PER_SAMPLE = {
    16: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_16BIT,
    24: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_24BIT,
    32: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_32BIT,
}

i2s_channel_fmt_t = cg.global_ns.enum("i2s_channel_fmt_t")
CHANNEL_FORMATS = {
    "right": i2s_channel_fmt_t.I2S_CHANNEL_FMT_ONLY_RIGHT,
    "left": i2s_channel_fmt_t.I2S_CHANNEL_FMT_ONLY_LEFT,
    "right_left": i2s_channel_fmt_t.I2S_CHANNEL_FMT_RIGHT_LEFT,
    "all_right": i2s_channel_fmt_t.I2S_CHANNEL_FMT_ALL_RIGHT,
    "all_left": i2s_channel_fmt_t.I2S_CHANNEL_FMT_ALL_LEFT,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SharedI2SAudioComponent),
        cv.Optional(CONF_I2S_PORT, default=0): cv.enum(I2S_PORTS, int=True),
        cv.Required(CONF_BCLK_PIN): cv.int_range(min=0, max=39),
        cv.Required(CONF_LRCLK_PIN): cv.int_range(min=0, max=39),
        cv.Optional(CONF_MCLK_PIN): cv.int_range(min=0, max=39),
        cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(min=8000, max=96000),
        cv.Optional(CONF_BITS_PER_SAMPLE, default=16): cv.enum(BITS_PER_SAMPLE, int=True),
        cv.Optional(CONF_CHANNEL_FORMAT, default="right"): cv.enum(CHANNEL_FORMATS),
        cv.Optional(CONF_USE_APLL, default=False): cv.boolean,
        cv.Optional(CONF_BUFFER_COUNT, default=8): cv.int_range(min=2, max=128),
        cv.Optional(CONF_BUFFER_LENGTH, default=256): cv.int_range(min=64, max=1024),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_i2s_port(config[CONF_I2S_PORT]))
    cg.add(var.set_bclk_pin(config[CONF_BCLK_PIN]))
    cg.add(var.set_lrclk_pin(config[CONF_LRCLK_PIN]))

    if CONF_MCLK_PIN in config:
        cg.add(var.set_mclk_pin(config[CONF_MCLK_PIN]))

    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))
    cg.add(var.set_channel_format(config[CONF_CHANNEL_FORMAT]))
    cg.add(var.set_use_apll(config[CONF_USE_APLL]))
    cg.add(var.set_buffer_count(config[CONF_BUFFER_COUNT]))
    cg.add(var.set_buffer_length(config[CONF_BUFFER_LENGTH]))
