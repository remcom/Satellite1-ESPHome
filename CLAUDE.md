# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an experimental ESPHome fork for the FutureProofHomes Satellite1 hardware (Core Board + HAT). It replaces Snapcast with Sendspin for synchronized multi-room audio.

**Key difference from official firmware:** Uses Sendspin (not Snapcast) for multi-room audio sync.

## Build Commands

```bash
# Set up build environment (creates .venv, installs ESPHome)
source scripts/setup_build_env.sh

# Build firmware
esphome compile config/satellite1.yaml

# Upload to device
esphome upload config/satellite1.yaml

# View logs
esphome logs config/satellite1.yaml
```

**Firmware variants:**
- `config/satellite1.yaml` - Default (no radar)
- `config/satellite1.ld2410.yaml` - With LD2410 radar
- `config/satellite1.ld2450.yaml` - With LD2450 radar

## Code Architecture

### Custom Components (local)

**esphome/components/** - FutureProofHomes hardware drivers:
- `satellite1/` - Main board component (SPI communication with XMOS, GPIO control, audio DAC)
- `memory_flasher/` - XMOS firmware flasher (also `satellite1/memory_flasher/` platform)

**components/** - Hardware drivers and audio pipeline (local overrides):
- `i2s_audio/` - I2S audio with shared bus support, includes `speaker/` and `microphone/`
- `resampler/` - Audio resampler
- `fusb302b/` - USB-PD controller

(TAS2780 amplifier comes from a pinned git source — see `external_components` in `config/satellite1.yaml`; PCM5122 DAC and dac_switcher come from ESPHome core.)

### External Components (from ESPHome PRs)

The firmware pulls experimental components from ESPHome PRs for Sendspin support:
- `mixer` - Audio mixer
- `audio` - Audio handling
- `media_player` - Media player improvements
- `mdns`, `sendspin` - Multi-room sync
- `file`, `http_request`, `media_source`, `speaker_source` - Media sources

### Configuration Structure

**config/** - Device YAML configurations:
- `satellite1.yaml` - Main config, defines external_components and device-specific overrides
- `satellite1.base.yaml` - Base configuration included by all variants
- `common/` - Modular YAML includes (voice_assistant, media_player, led_ring, buttons, etc.)

**YAML pattern:** Uses `!include` for modular configs. The `packages:` key in base config includes components from `common/`.

## Key Component IDs

Important IDs used across YAML configs:
- `external_media_player` — main media player entity
- `sat1_mics_raw` — raw I2S microphone (48kHz stereo 32-bit)
- `led_ring` — user-facing LED ring (24 LEDs, partition)
- `voice_assistant_leds` — internal partition used for all VA LED effects
- `speaker_dac_tas2780` — TAS2780 amplifier
- `control_leds` — master LED dispatcher script (call whenever state changes)

## Key Conventions

- ESPHome coding standards, C++ style, component patterns, and embedded-systems guidelines: see `.ai/instructions.md` (adapted from upstream ESPHome — repo-layout specifics there may not apply here)
- ESPHome version pinned in `requirements.txt` (check that file for the current pin; `min_version` in `satellite1.base.yaml` is the floor for dashboard builds)
- Python 3.11-3.13 required (ESPHome constraint)
- XMOS firmware version defined in `satellite1.base.yaml` (`xmos_fw_version` substitution)
- External components reference specific git commits for stability

## Gotchas

- **`@automation.register_action()` requires `synchronous=`** — use `synchronous=True` when `play()` completes inline (GPIO, I2C); use `synchronous=False` when work is deferred to a FreeRTOS task. Missing this produces a WARNING and disables StringRef optimization.
- **`status_set_error()` takes `LOG_STR()`** — wrap string literals: `status_set_error(LOG_STR("msg"))`. Plain `const char*` is deprecated as of 2026.3 and breaks in 2026.6.
- **`audio_dac` platform pattern** — TAS2780/PCM5122 schemas live in `audio_dac.py`; `__init__.py` is intentionally empty. Action classes must use `cg.Parented.template(...)` and `await cg.register_parented(...)`.
- **XMOS is I2S primary (clock source)** — ESP32 runs as secondary (`i2s_mode: secondary`). The `add_data_callback()` on `sat1_mics_raw` delivers 48kHz stereo 32-bit, not the 16kHz mono resampler output.
- **Dashboard remote compile** — `config/satellite1-remote.yaml` uses `type: git` for local component paths (ESPHome resolves `type: local` relative to user config dir, breaking GitHub-fetched packages). Local builds use `satellite1.yaml` unchanged.
