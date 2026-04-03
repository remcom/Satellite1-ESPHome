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
- `memory_flasher/` - XMOS firmware flasher
- `audio_visualizer/` - Audio level visualizer for LED ring

**components/** - Hardware drivers and audio pipeline (local overrides):
- `i2s_audio/` - I2S audio with shared bus support, includes `speaker/` and `microphone/`
- `resampler/` - Audio resampler
- `fusb302b/` - USB-PD controller
- `pcm5122/` - Audio DAC
- `tas2780/` - Audio amplifier
- `dac_switcher/` - DAC switching logic

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

## Testing

### Microphone Pipeline Test

Tests mic by streaming audio via UDP from the device to this machine.

```bash
# Setup
tests/mic_streaming/setup_testdata.sh
source scripts/setup_build_env.sh
pip install -r tests/mic_streaming/requirements.txt

# Enable developer mode in config/satellite1.base.yaml:
# developer: !include common/developer.yaml

# Compile, upload, then run test
python tests/mic_streaming/run_test.py
# Or live streaming:
python tests/mic_streaming/run_live_streaming.py
```

Recordings saved to `testdata/mic_streaming/`.

## Key Component IDs

Important IDs used across YAML configs:
- `external_media_player` — main media player entity
- `sat1_mics_raw` — raw I2S microphone (48kHz stereo 32-bit)
- `led_ring` — user-facing LED ring (24 LEDs, partition)
- `voice_assistant_leds` — internal partition used for all VA LED effects
- `speaker_dac_tas2780` — TAS2780 amplifier
- `control_leds` — master LED dispatcher script (call whenever state changes)

## Key Conventions

- ESPHome version pinned in `requirements.txt` (currently 2026.3.2)
- Python 3.11-3.13 required (ESPHome constraint)
- XMOS firmware version defined in `satellite1.base.yaml` (`xmos_fw_version` substitution)
- External components reference specific git commits for stability
