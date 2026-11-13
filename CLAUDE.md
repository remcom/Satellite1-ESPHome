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

# Lint (same tool versions as upstream ESPHome)
pre-commit run --all-files
pre-commit run --hook-stage manual clang-tidy             # slow, regenerates compile_commands.json
python3 scripts/run_clang_tidy.py --skip-regen <files>   # fast once compile_commands.json is fresh
```

**Firmware variants:**
- `config/satellite1.yaml` - Default (no radar)
- `config/satellite1.ld2410.yaml` - With LD2410 radar
- `config/satellite1.ld2450.yaml` - With LD2450 radar
- `config/satellite1.secure.yaml` - NVS encryption + signed OTA (needs `secure_boot_signing_key.pem`)

## Code Architecture

### Custom Components (local)

**esphome/components/** - the only non-core components:
- `satellite1/` - SPI link to the XMOS (audio settings, DoA, wake word steering, GPIO expander); platforms `sensor` (DoA), `update` (XMOS update entity), `memory_flasher`
- `memory_flasher/` - XMOS flash writer (embedded or downloaded image, md5-checked)

Everything else (i2s_audio full duplex bus, fusb302b, tas2780, pcm5122, resampler microphone, mixer, sendspin, media sources) comes from ESPHome core; the full duplex i2s_audio and fusb302b are new in 2026.11, hence `min_version: 2026.11.0`. fusb302b exposes the PD contract and voltage through `text_sensor`/`sensor` platforms, not inline keys.

### Configuration Structure

**config/** - Device YAML configurations:
- `satellite1.yaml` - Standard build: includes `satellite1.common.yaml` + `dashboard_import`
- `satellite1.common.yaml` - Shared by local builds: external_components, OTA/beta channel, boot-time XMOS update (`on_xmos_connected`)
- `satellite1-dashboard.yaml` / `satellite1-remote.yaml` - dashboard adoption / remote compile
- `satellite1.base.yaml` - Base configuration included by all variants
- `common/` - Modular YAML includes (voice_assistant, media_player, led_ring, buttons, etc.)

**YAML pattern:** Uses `!include` for modular configs. The `packages:` key in base config includes components from `common/`.

### XMOS firmware variants

- Two variants per XMOS release: Standard (`fixed_delay`, embedded in the ESP build) and AEC4 (downloaded at install). User-facing explanation in `docs/XMOS.md`.
- `common/xmos_firmware.yaml`: variant select, `xmos_update` entity, per-variant settings tuples, install scripts; `common/xmos_audio.yaml`: mic settings, DoA, beam steering.
- Images are fetched by tag-carrying names (`satellite1_xmos-${xmos_fw_version}.factory.bin`, `satellite1_xmos_aec4-...`) from `xmos_fw_release_url`, so the build needs internet.
- The XMOS persists nothing: every setting is re-sent from `on_xmos_connected` (`apply_xmos_mic_settings`). New settings must be added there and, if per-variant, to the 5-value tuples (`xmos_settings_*_default`, `xmos_sync_variant_settings`, `xmos_set_audio_settings`).

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
- ESPHome version pinned in `requirements.txt` (the single source of truth; CI's build workflow passes that version to build-action as the image tag. A dev commit SHA also works if its trailing comment names the matching nightly image tag, e.g. `2026.11.0-dev20261101`; `min_version` in `satellite1.base.yaml` is the floor for dashboard builds. After changing the pin, reinstall with `pip install --force-reinstall --no-deps`)
- Python 3.12-3.14 required (ESPHome constraint: `requires-python = ">=3.12.0,<3.15"`)
- XMOS firmware version defined in `satellite1.base.yaml` (`xmos_fw_version` substitution)
- User docs: `README.md` (feature overview), `docs/XMOS.md` (XMOS settings/variants). Release notes are drafted as `RELEASE_NOTES_vX.Y.Z-betaN.md` in the repo root (untracked) and follow the previous release's structure

## Gotchas

- **`@automation.register_action()` requires `synchronous=`** — use `synchronous=True` when `play()` completes inline (GPIO, I2C); use `synchronous=False` when work is deferred to a FreeRTOS task. Missing this produces a WARNING and disables StringRef optimization.
- **`status_set_error()` takes `LOG_STR()`** — wrap string literals: `status_set_error(LOG_STR("msg"))`. Plain `const char*` is deprecated as of 2026.3 and breaks in 2026.6.
- **`audio_dac` platform pattern** — TAS2780/PCM5122 schemas live in `audio_dac.py`; `__init__.py` is intentionally empty. Action classes must use `cg.Parented.template(...)` and `await cg.register_parented(...)`.
- **XMOS is I2S primary (clock source)** — ESP32 runs as secondary: `i2s_mode: secondary` is set on the microphone and on each i2s speaker (not on the bus), and the `i2s_shared` bus uses `full_duplex: true`. The full duplex TX channel is fixed at 32-bit, so the mixer and router run at `bits_per_sample: 32`. The `add_data_callback()` on `sat1_mics_raw` delivers 48kHz stereo 32-bit, not the 16kHz mono resampler output.
- **Dashboard remote compile** — `config/satellite1-remote.yaml` uses `type: git` for local component paths (ESPHome resolves `type: local` relative to user config dir, breaking GitHub-fetched packages). Local builds use `satellite1.yaml` unchanged.
