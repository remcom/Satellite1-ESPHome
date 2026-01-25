# Copilot Instructions for Satellite1-ESPHome

## Project Overview
- **Purpose:** Experimental fork of Satellite1-ESPHome to enable [Sendspin](https://github.com/esphome/esphome/pull/12284) multi-room audio and upstream custom components to ESPHome.
- **Hardware:** FutureProofHomes Satellite1 (Core Board + HAT).
- **Key Features:**
  - Synchronized multi-room audio (Sendspin)
  - Voice Assistant integration
  - Custom audio, microphone, and light components

## Architecture & Structure
- **Firmware Source:**
  - Main custom components in `esphome/components/satellite1/` (audio, light, microphone, memory flasher, etc.)
  - Additional hardware drivers: `esphome/components/fusb302b/`, `esphome/components/pcm5122/`, `esphome/components/tas2780/`
  - Audio pipeline: `components/i2s_audio/` and `components/i2s_audio/speaker/`
  - Resampling: `components/i2s_audio/resampler/`
- **Configuration:**
  - Device YAMLs in `config/` (see `satellite1.yaml`, `satellite1.base.yaml`)
  - Common includes in `config/common/`
- **Testing:**
  - Microphone streaming tests: `tests/mic_streaming/` (see `README.md` for workflow)

## Developer Workflows
- **Build & Flash:**
  1. `source scripts/setup_build_env.sh` (sets up Python venv and dependencies)
  2. `esphome compile config/satellite1.yaml`
  3. `esphome upload config/satellite1.yaml`
  4. `esphome logs config/satellite1.yaml`
- **Test Microphone Pipeline:**
  1. `tests/mic_streaming/setup_testdata.sh`
  2. `source scripts/setup_build_env.sh`
  3. `source .venv/bin/activate`
  4. `pip install -r tests/mic_streaming/requirements.txt`
  5. Edit `config/satellite1.base.yaml` to include `developer.yaml`
  6. Compile & upload firmware
  7. Run: `python tests/mic_streaming/run_test.py` or `run_live_streaming.py`
- **Virtual Environment:** Always activate `.venv` before running Python scripts.

## Project-Specific Conventions
- **YAML includes:** Use `!include` for modular config in `config/`.
- **Component Structure:** Each hardware feature has its own subdirectory with C++ and Python glue code.
- **Experimental Features:** This fork may have breaking changes and incomplete features.
- **Sendspin:** Integration is experimental and not in upstream ESPHome yet.

## Integration Points
- **Home Assistant:** Device expects HA with Voice Assistant enabled.
- **Sendspin:** See upstream PR for protocol details.
- **Audio/UDP:** Microphone data can be streamed via UDP for testing.

## References
- [README.md](../README.md) — project goals, build steps
- [tests/mic_streaming/README.md](../tests/mic_streaming/README.md) — test pipeline
- [scripts/setup_build_env.sh](../scripts/setup_build_env.sh) — venv and dependency setup
- [config/satellite1.yaml](../config/satellite1.yaml) — main device config

---
For new features, follow the structure of existing components and update relevant YAMLs and test scripts. When in doubt, check for similar patterns in `esphome/components/satellite1/` and `components/i2s_audio/`.
