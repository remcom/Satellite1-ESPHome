# Satellite1-ESPHome (Sendspin Development Fork)

> **This is a development fork** of the original [FutureProofHomes/Satellite1-ESPHome](https://github.com/FutureProofHomes/Satellite1-ESPHome) project.

## Status: Work in Progress

This fork is actively being developed and is **not yet ready for production use**. Expect breaking changes, incomplete features, and experimental code.

---

## Purpose

The primary goals of this fork are:

### 1. Sendspin Support
Enable [Sendspin](https://github.com/esphome/esphome/pull/12284) synchronized multi-room audio playback on the Satellite1 hardware. Sendspin allows multiple ESPHome devices to play audio in perfect sync across rooms.

### 2. Upstream ESPHome Integration
Work towards getting the Satellite1's custom components merged into ESPHome's mainstream codebase. This includes:

### 3. Voice PE Feature Parity
Bring features from [Home Assistant Voice PE](https://github.com/esphome/home-assistant-voice-pe) to the Satellite1

---

## What's Different from Upstream

This fork includes experimental features not yet in the main FutureProofHomes repository:

- **Sendspin integration** for multi-room synchronized audio

---

## Requirements

- ESPHome **2026.1.1** or newer
- FutureProofHomes Satellite1 hardware (Core Board + HAT)
- Home Assistant with Voice Assistant configured

---

## Building

```bash
# Set up the build environment
source scripts/setup_build_env.sh

# Compile
esphome compile config/satellite1.yaml

# Upload to device
esphome upload config/satellite1.yaml

# View logs
esphome logs config/satellite1.yaml
```

---

## Contributing

This is an experimental fork. If you're interested in:
- **Stable firmware**: Use the official [FutureProofHomes/Satellite1-ESPHome](https://github.com/FutureProofHomes/Satellite1-ESPHome)
- **Sendspin testing**: Issues and PRs welcome here
- **Upstream contributions**: Consider contributing directly to the ESPHome PRs listed above

---

## Credits

- **[FutureProofHomes](https://futureproofhomes.net/)** - Original Satellite1 hardware and firmware
- **[Nabu Casa](https://nabucasa.com/)** - Home Assistant and ESPHome
- **ESPHome community** - Sendspin and media player improvements

---

## License

Same as the original project - see [LICENSE](LICENSE) for details.

---

## Links

- Original project: [FutureProofHomes/Satellite1-ESPHome](https://github.com/FutureProofHomes/Satellite1-ESPHome)
- FutureProofHomes docs: [docs.futureproofhomes.net](https://docs.futureproofhomes.net)
- Home Assistant Voice PE: [esphome/home-assistant-voice-pe](https://github.com/esphome/home-assistant-voice-pe)
- ESPHome: [esphome.io](https://esphome.io)
