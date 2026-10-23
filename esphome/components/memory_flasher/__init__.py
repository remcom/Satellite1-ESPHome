import hashlib
import logging
from pathlib import Path
import re

from esphome import automation, external_files
import esphome.codegen as cg
from esphome.components.http_request import CONF_HTTP_REQUEST_ID, HttpRequestComponent
import esphome.config_validation as cv
from esphome.const import (
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_URL,
)
from esphome.core import CORE, EsphomeError, HexInt
from esphome.external_files import download_content
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@gnumpi"]

IS_PLATFORM_COMPONENT = True

AUTO_LOAD = ["md5"]
DEPENDENCIES = ["network", "http_request"]
DOMAIN = "file"

TYPE_LOCAL = "local"
TYPE_WEB = "web"
TYPE_FILE = "file"
TYPE_STRING = "string"

CONF_MD5 = "md5"
CONF_MD5_FILE = "md5_file"
CONF_MD5_URL = "md5_url"
CONF_IMAGE_FILE = "image_file"

CONF_EMBED_FLASH_IMAGE = "embed_flash_image"
CONF_FLASH_IMAGE_VERSION = "image_version"
CONF_FLASHER_ID = "flasher_id"

CONF_ON_PROGRESS_UPDATE = "on_progress_update"
CONF_ON_FLASHING_START = "on_flashing_start"
CONF_ON_FLASHING_SUCCESS = "on_flashing_success"
CONF_ON_FLASHING_FAILED = "on_flashing_failed"
CONF_ON_ERASING_DONE = "on_erasing_done"

MD5_REGEX = re.compile(r"^[a-fA-F0-9]{32}$")
VERSION_REGEX = re.compile(
    r"^v?(\d+)\.(\d+)\.(\d+)(-(alpha|beta|rc|dev)(\.(\d+))?)?$", re.IGNORECASE
)

flasher_ns = cg.esphome_ns.namespace("memory_flasher")
FlashImage = flasher_ns.struct("FlashImage")
MemoryFlasher = flasher_ns.class_("MemoryFlasher", cg.Component)


EraseAction = flasher_ns.class_("EraseMemoryAction", automation.Action)

FlashAction = flasher_ns.class_("FlashAction", automation.Action)

FlashEmbeddedAction = flasher_ns.class_("FlashEmbeddedAction", automation.Action)

FlashingStartedTrigger = flasher_ns.class_("FlashingStartedTrigger", automation.Trigger)

FlashProgressUpdateTrigger = flasher_ns.class_(
    "FlashingProgressUpdateTrigger", automation.Trigger
)
FlashProgressSuccessTrigger = flasher_ns.class_(
    "FlasherSuccessTrigger", automation.Trigger
)
FlashProgressFailedTrigger = flasher_ns.class_(
    "FlasherFailedTrigger", automation.Trigger
)

ErasingDoneTrigger = flasher_ns.class_("ErasingDoneTrigger", automation.Trigger)


InProgressCondition = flasher_ns.class_("InProgressCondition", automation.Condition)


def _generate_local_file_path(value: dict) -> Path:
    """
    Generates a local file path for a downloadable file by hashing the provided URL.
    """
    url = value[CONF_URL]
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    return base_dir / key


def _resolve_local_file(file_conf: dict) -> Path:
    """
    Checks if a local file exists, and if not, downloads it from a provided URL.
    Returning the local file path if successful.

    """
    if file_conf[CONF_TYPE] == TYPE_LOCAL:
        local_file = Path(CORE.relative_config_path(file_conf[CONF_PATH]))
        if not local_file.exists():
            raise cv.Invalid(f"Can't locate {local_file}.")
        return local_file
    if file_conf[CONF_TYPE] == TYPE_WEB:
        local_file = _generate_local_file_path(file_conf)
        if not local_file.exists():
            download_content(file_conf[CONF_URL], local_file)
            if not local_file.exists():
                raise cv.Invalid(
                    f"Couldn't download {file_conf[CONF_URL]} to {local_file}."
                )
        return local_file
    raise cv.Invalid(f"Couldn't get file for: {file_conf}")


def _validate_md5_str(value: str) -> str:
    """
    Validates whether the given string is a valid MD5 checksum.
    Returns it in lower case, as produced by hashlib and the on-device MD5.
    """
    value = cv.string_strict(value).strip()
    if MD5_REGEX.match(value) is None:
        raise cv.Invalid(f"Invalid md5 string: {value}")
    return value.lower()


def _get_expected_md5(flash_config: ConfigType) -> str:
    """
    Returns the expected MD5 checksum from the configuration, which can either be
    provided directly as a string or as a local file / URL containing the checksum.
    """
    if (md5 := flash_config.get(CONF_MD5)) is not None:
        return _validate_md5_str(md5)
    if (md5_url := flash_config.get(CONF_MD5_URL)) is not None:
        file_conf = {CONF_TYPE: TYPE_WEB, CONF_URL: md5_url}
    else:
        file_conf = flash_config[CONF_MD5_FILE]
    local_file_path = _resolve_local_file(file_conf)
    with open(local_file_path, encoding="utf-8") as f:
        # md5sum format is "<md5>  <file name>", only the checksum is of interest
        tokens = f.readline().split()
    if not tokens:
        raise cv.Invalid(f"MD5 file {local_file_path} is empty")
    return _validate_md5_str(tokens[0])


LOCAL_FILE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): cv.file_,
    }
)

WEB_FILE_SCHEMA = cv.All(
    {
        cv.Required(CONF_URL): cv.url,
    },
)

TYPED_FILE_SCHEMA = cv.typed_schema(
    {
        TYPE_LOCAL: LOCAL_FILE_SCHEMA,
        TYPE_WEB: WEB_FILE_SCHEMA,
    },
)


def _validate_file_shorthand(value):
    value = cv.string_strict(value)
    if value.startswith("http://") or value.startswith("https://"):
        return _file_schema(
            {
                CONF_TYPE: TYPE_WEB,
                CONF_URL: value,
            }
        )
    return _file_schema(
        {
            CONF_TYPE: TYPE_LOCAL,
            CONF_PATH: value,
        }
    )


def _file_schema(value):
    if isinstance(value, str):
        return _validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


def _validate_embed_image_schema(image_config: dict) -> dict:
    """
    Makes sure the image is available locally and resolves md5_file / md5_url into md5,
    so the checksum is only read once. The (large) image itself is read and checked
    once, when it gets embedded, see _prepare_flash_image_pgm().
    """
    _resolve_local_file(image_config[CONF_IMAGE_FILE])
    image_config[CONF_MD5] = _get_expected_md5(image_config)
    image_config.pop(CONF_MD5_FILE, None)
    image_config.pop(CONF_MD5_URL, None)
    return image_config


def _validate_image_version_string(val: str) -> str:
    val = cv.string_strict(val)
    match = VERSION_REGEX.match(val)
    if not match:
        raise cv.Invalid(f"Not a valid version string: {val}")
    # Each part is stored in a single byte, see _version_to_bytes()
    for part in (match[1], match[2], match[3], match[7]):
        if part is not None and int(part) > 255:
            raise cv.Invalid(f"Version parts must be between 0 and 255: {val}")
    return val


FLASH_IMAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FLASH_IMAGE_VERSION): _validate_image_version_string,
        cv.Required(CONF_IMAGE_FILE): cv.templatable(cv.url),
        cv.Optional(CONF_MD5_URL): cv.templatable(cv.url),
        cv.Optional(CONF_MD5): cv.templatable(_validate_md5_str),
    }
)


EMBED_FLASH_IMAGE_SCHEMA = cv.All(
    FLASH_IMAGE_SCHEMA.extend(
        {
            # Resolved at compile time, so these can't be templates
            cv.Required(CONF_IMAGE_FILE): _file_schema,
            cv.Optional(CONF_MD5_URL): cv.url,
            cv.Optional(CONF_MD5): _validate_md5_str,
            cv.Optional(CONF_MD5_FILE): _file_schema,
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        }
    ),
    cv.has_exactly_one_key(CONF_MD5, CONF_MD5_FILE, CONF_MD5_URL),
    _validate_embed_image_schema,
)


FLASHER_CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MemoryFlasher),
        cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
        cv.Optional(CONF_EMBED_FLASH_IMAGE): EMBED_FLASH_IMAGE_SCHEMA,
        cv.Optional(CONF_ON_FLASHING_START): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FlashingStartedTrigger),
            }
        ),
        cv.Optional(CONF_ON_PROGRESS_UPDATE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    FlashProgressUpdateTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_FLASHING_SUCCESS): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    FlashProgressSuccessTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_FLASHING_FAILED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    FlashProgressFailedTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_ERASING_DONE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ErasingDoneTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _prepare_flash_image_pgm(
    flash_config: ConfigType,
) -> tuple[str, tuple[HexInt, ...], int]:
    local_flash_image = _resolve_local_file(flash_config[CONF_IMAGE_FILE])
    md5_sum = flash_config[CONF_MD5]
    with open(local_flash_image, "rb") as f:
        image = f.read()
    # Checked on the bytes that get embedded
    if hashlib.md5(image).hexdigest() != md5_sum:
        raise EsphomeError(
            f"Flash-Image: md5 sum of {local_flash_image} does not match {md5_sum}."
        )
    hex_strings = tuple(map(HexInt, image))
    return md5_sum, hex_strings, len(hex_strings)


# Index matches FlashImagePreRelease in memory_flasher.h
PRE_RELEASE_NAMES = ["none", "alpha", "beta", "rc", "dev"]


def _version_to_bytes(vers: str) -> tuple[int, int, int, int, int]:
    """Returns (major, minor, patch, pre_release, counter) as stored by the XMOS."""
    match = VERSION_REGEX.match(vers)
    major = int(match[1])
    minor = int(match[2])
    patch = int(match[3])
    pre_idx = 0
    pre_cnter = 0
    if match[4]:
        pre_idx = PRE_RELEASE_NAMES.index(match[5].lower())
        pre_cnter = int(match[7]) if match[6] else 0
    return major, minor, patch, pre_idx, pre_cnter


async def register_memory_flasher(var, flasher_config):
    http_comp = await cg.get_variable(flasher_config[CONF_HTTP_REQUEST_ID])
    cg.add(var.set_http_request_component(http_comp))
    await cg.register_component(var, flasher_config)

    if image_conf := flasher_config.get(CONF_EMBED_FLASH_IMAGE):
        expected_md5_sum, hex_strings, num_of_bytes = _prepare_flash_image_pgm(
            image_conf
        )
        prog_arr = cg.progmem_array(image_conf[CONF_RAW_DATA_ID], hex_strings)
        cg.add(var.set_embedded_image(prog_arr, num_of_bytes, expected_md5_sum))
        # Passed as integers: a string literal would be UTF-8 encoded, turning any
        # version byte >= 0x80 into two bytes.
        cg.add(
            var.set_embedded_image_version(
                *_version_to_bytes(image_conf[CONF_FLASH_IMAGE_VERSION])
            )
        )

    for conf in flasher_config.get(CONF_ON_FLASHING_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in flasher_config.get(CONF_ON_PROGRESS_UPDATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in flasher_config.get(CONF_ON_FLASHING_SUCCESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in flasher_config.get(CONF_ON_FLASHING_FAILED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in flasher_config.get(CONF_ON_ERASING_DONE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


FLASH_ACTION_SCHEMA = cv.All(
    FLASH_IMAGE_SCHEMA.extend(
        {cv.GenerateID(CONF_FLASHER_ID): cv.use_id(MemoryFlasher)}
    ),
    # Without an expected checksum the flash would be erased before failing
    cv.has_exactly_one_key(CONF_MD5, CONF_MD5_URL),
)


@automation.register_action(
    "memory_flasher.write_image",
    FlashAction,
    FLASH_ACTION_SCHEMA,
    synchronous=True,
)
async def flash_image_action_to_code(config, action_id, template_arg, args):
    flasher = await cg.get_variable(config[CONF_FLASHER_ID])
    var = cg.new_Pvariable(action_id, template_arg, flasher)

    if md5_url := config.get(CONF_MD5_URL):
        template_ = await cg.templatable(md5_url, args, cg.std_string)
        cg.add(var.set_md5_url(template_))

    if md5_str := config.get(CONF_MD5):
        template_ = await cg.templatable(md5_str, args, cg.std_string)
        cg.add(var.set_md5(template_))

    template_ = await cg.templatable(config[CONF_IMAGE_FILE], args, cg.std_string)
    cg.add(var.set_url(template_))

    return var


FLASH_EMBEDDED_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(CONF_FLASHER_ID): cv.use_id(MemoryFlasher)}
)


@automation.register_action(
    "memory_flasher.write_embedded_image",
    FlashEmbeddedAction,
    FLASH_EMBEDDED_ACTION_SCHEMA,
    synchronous=True,
)
async def flash_embedded_image_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_FLASHER_ID])
    return var


ERASE_MEMORY_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(CONF_FLASHER_ID): cv.use_id(MemoryFlasher)}
)


@automation.register_action(
    "memory_flasher.erase", EraseAction, ERASE_MEMORY_ACTION_SCHEMA, synchronous=True
)
async def erase_memory_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_FLASHER_ID])
    return var


CONDITION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(CONF_FLASHER_ID): cv.use_id(MemoryFlasher)}
)


@automation.register_condition(
    "memory_flasher.in_progress", InProgressCondition, CONDITION_SCHEMA
)
async def in_progress_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_FLASHER_ID])
    return var
