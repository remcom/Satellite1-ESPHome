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

(i2s_audio (full duplex bus) comes from esphome/esphome PR #19959 (`remcom/esphome` `i2s_audio-full-duplex` branch), and the FUSB302B USB-PD controller from the `remcom/esphome` `fusb302b` branch — see `external_components` in `config/satellite1.common.yaml`; TAS2780 amplifier, PCM5122 DAC and the resampler microphone come from ESPHome core.)

### Components from ESPHome core vs. external

Everything except the Satellite1-specific drivers above comes from the pinned ESPHome release: `mixer`, `audio`, `speaker_source`, `media_player`, `mdns`, `sendspin`, `http_request`, `media_source`, `tas2780`, `pcm5122` and the `resampler` microphone. Only two components are fetched from `remcom/esphome` at a pinned commit (see `external_components` in `config/satellite1.common.yaml`, mirrored in `config/satellite1-remote.yaml`): `i2s_audio` (full duplex, merged upstream only for 2026.11) and `fusb302b`. Drop each pin once the component ships in the ESPHome release we pin.

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

- ESPHome coding standards, C++ style, component patterns, and embedded-systems guidelines: see `.ai/instructions.md` (a copy of upstream ESPHome's `AGENTS.md` — repo-layout specifics there may not apply here)
- ESPHome version pinned in `requirements.txt` (check that file for the current pin; `min_version` in `satellite1.base.yaml` is the floor for dashboard builds)
- Python 3.12-3.14 required (ESPHome constraint: `requires-python = ">=3.12.0,<3.15"`)
- XMOS firmware version defined in `satellite1.base.yaml` (`xmos_fw_version` substitution)
- External components reference specific git commits for stability

## Gotchas

- **`@automation.register_action()` requires `synchronous=`** — use `synchronous=True` when `play()` completes inline (GPIO, I2C); use `synchronous=False` when work is deferred to a FreeRTOS task. Missing this produces a WARNING and disables StringRef optimization.
- **`status_set_error()` takes `LOG_STR()`** — wrap string literals: `status_set_error(LOG_STR("msg"))`. Plain `const char*` is deprecated as of 2026.3 and breaks in 2026.6.
- **XMOS is I2S primary (clock source)** — ESP32 runs as secondary: `i2s_mode: secondary` is set on the microphone and on each i2s speaker (not on the bus), and the `i2s_shared` bus uses `full_duplex: true`. The full duplex TX channel is fixed at 32-bit, so the mixer and router run at `bits_per_sample: 32`. The `add_data_callback()` on `sat1_mics_raw` delivers 48kHz stereo 32-bit, not the 16kHz mono resampler output.
- **Dashboard remote compile** — `config/satellite1-remote.yaml` uses `type: git` for local component paths (ESPHome resolves `type: local` relative to user config dir, breaking GitHub-fetched packages). Local builds use `satellite1.yaml` unchanged.

## Upgrading ESPHome and cutting a release

Work lands on `develop`; `main` only gets merges from it at release time. `requirements.txt` is the single source of truth for the ESPHome version (the release workflow reads it), and Dependabot deliberately does not touch it.

1. Read the upstream changelog (`https://esphome.io/changelog/<version>.html`) for breaking changes in components we use: `speaker`, `mixer`, `sendspin`, `i2s_audio`, `microphone`, `tas2780`, `improv_ble`, `light`, `wifi`, `ota`.
2. `scripts/bump_release.sh --esphome <version>` updates `requirements.txt`, `min_version` and the README/site version strings.
3. Run `esphome config` for all four variants (`satellite1`, `.ld2410`, `.ld2450`, `.secure`; the secure one needs a throwaway `secure_boot_signing_key.pem`, which is gitignored) and `esphome compile config/satellite1.yaml`. Fix every new WARNING: deprecations are removed a few releases later.
4. If a pinned external component (`i2s_audio`, `fusb302b`) was merged or released upstream, drop the pin in both `satellite1.common.yaml` and `satellite1-remote.yaml`.
5. Check logger levels in `satellite1.common.yaml` are back to `info`/`INFO`, compare RAM/flash against the previous release, and test on hardware (audio in/out, wake word during playback, Sendspin, line-out switching, OTA).
6. Release commit: `scripts/bump_release.sh --esphome <stable> --firmware vX.Y.Z`, merge `develop` into `main`, then publish a GitHub release tagged `vX.Y.Z` (use `-betaN` and "pre-release" for betas). CI builds all four variants, publishes the `latest`/`beta` channel and, for stable releases only, commits `site/manifests/*.bin` back to `main`.
7. Mention in the release notes anything users must do first (e.g. a Music Assistant update when the Sendspin protocol version changes).
