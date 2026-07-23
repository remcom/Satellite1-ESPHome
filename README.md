# Satellite1 ESPHome Firmware — Sendspin Multi-Room Audio

**Stable, actively maintained ESPHome firmware for the FutureProofHomes Satellite1 voice assistant, tracking upstream ESPHome closely with ongoing hardware and power-management fixes.**

[![Firmware](https://img.shields.io/badge/Firmware-v1.2.0-brightgreen)](https://github.com/remcom/Satellite1-ESPHome/releases)
[![ESPHome](https://img.shields.io/badge/ESPHome-2026.7-blue)](https://esphome.io)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-Compatible-green)](https://www.home-assistant.io/)
[![License](https://img.shields.io/badge/License-Same%20as%20upstream-lightgrey)](LICENSE)

---

## What is This?

This is an actively maintained ESPHome firmware fork for the [FutureProofHomes Satellite1](https://futureproofhomes.net/) (Core Board + HAT) voice assistant hardware, built around **Sendspin** — ESPHome's native, in-tree audio synchronization protocol — for synchronized multi-room music playback alongside full local voice assistant support.

### Key Features

- ✅ **Sendspin Multi-Room Audio** — synchronized playback across multiple Satellite1 devices, no external sync server required
- ✅ **Home Assistant Voice Assistant** — full local voice control integration
- ✅ **ESPHome Dashboard Support** — one-click adoption, OTA updates from stable and beta channels
- ✅ **Presence Detection** — optional LD2410/LD2450 mmWave radar variants
- ✅ **Power-Optimized DAC Management** — TAS2780 and PCM5122 both power down automatically when idle
- ✅ **Hardware Diagnostics** — TAS2780 voltage/temperature monitoring, reworked FUSB302B USB-PD driver

---

## Why This Fork

This project tracks upstream ESPHome releases closely (currently 2026.7.2), ships fixes for LED/timer/button logic and DAC power management as they're found, and runs `clang-tidy` and pre-commit linting on every change. It's a smaller, focused codebase maintained specifically for the Satellite1 hardware, with a release cadence built around getting fixes and upstream ESPHome improvements out quickly.

Both this fork and the official [FutureProofHomes firmware](https://github.com/FutureProofHomes/Satellite1-ESPHome) target the same hardware and now both build on Sendspin — pick whichever release cadence and maintenance style fits your setup.

---

## Quick Install

Install firmware directly from your browser (Chrome or Edge required, over USB):

### [→ Install Firmware Now](https://remcom.github.io/Satellite1-ESPHome/site/)

**Available Variants:**

| Variant | Best For | Features |
|---------|----------|----------|
| **Standard** | Voice assistant + music | Base firmware without radar |
| **LD2410** | Room presence detection | + HLK-LD2410 presence sensor |
| **LD2450** | Advanced zone tracking | + HLK-LD2450 multi-zone radar |

---

## Sendspin Multi-Room Audio

This firmware uses **Sendspin** for multi-room audio synchronization:

- **Native ESPHome integration** — no external servers required
- **Lower latency** — tuned for the ESP32-S3 + XMOS audio pipeline
- **Tighter Home Assistant integration** — seamless media player control
- **Built into ESPHome core** since 2026.6.0

### Additional Enhancements

Beyond multi-room audio, this fork includes:

- **ESPHome Dashboard adoption** — one-click setup, automatic OTA updates, and remote compile support so devices can be adopted and managed without touching a build environment
- **OTA firmware updates** — stable and beta release channels, so you can track fixes early or stay on a settled release
- **Hardware diagnostics** — TAS2780 voltage and temperature monitoring surfaced as Home Assistant entities, for spotting amplifier issues before they become audible
- **Smart power management** — automatic DAC shutoff for both TAS2780 and PCM5122 when idle, cutting standby power draw without any user configuration
- **Reworked FUSB302B / USB-PD driver** — a substantial internal rework of the USB-PD stack for more reliable power negotiation
- **LED and timer fixes** — corrected mute/volume LED indication and timer auto-off behavior, found and fixed through day-to-day use of the hardware

### Upstreaming to ESPHome

A long-term goal of this fork is to get the Satellite1 hardware drivers merged into ESPHome core, so the broader community — not just Satellite1 owners — can use them directly without a fork:

- ✅ **PCM5122 DAC** — merged upstream; now used directly from ESPHome core rather than a local component
- 🚧 **TAS2780 amplifier** — in progress, currently pulled from a pinned upstream PR pending review and merge

As more drivers land upstream, this fork gets smaller and closer to stock ESPHome — the ideal end state is a thin config layer on top of components everyone can use.

---

## Use Cases

**Perfect for:**

- 🏠 **Whole-home voice control** — control Home Assistant from any room
- 🎶 **Multi-room music** — play synchronized music across your entire home
- 🛏️ **Presence-based automation** — trigger actions based on room occupancy (with radar variants)
- 🔊 **High-quality audio** — professional DAC and amplifier for excellent sound
- 🌐 **Local voice assistant** — privacy-focused, works without cloud services

---

## System Requirements

### Hardware

- **FutureProofHomes Satellite1** (Core Board + HAT)
- **Optional:** LD2410 or LD2450 radar sensor for presence detection

### Software

- **ESPHome** 2026.7.2+
- **Home Assistant** with Voice Assistant configured
- **Chrome or Edge browser** (for the web installer)

### Recommended Setup

- Multiple Satellite1 devices for multi-room audio
- Stable Wi-Fi network (2.4GHz)
- Home Assistant OS or Supervised installation

---

## Building from Source

For developers and advanced users who want to customize the firmware:

```bash
# Clone the repository
git clone https://github.com/remcom/Satellite1-ESPHome.git
cd Satellite1-ESPHome

# Set up the build environment
source scripts/setup_build_env.sh

# Compile firmware
esphome compile config/satellite1.yaml

# Upload to device
esphome upload config/satellite1.yaml

# View live logs
esphome logs config/satellite1.yaml
```

### Firmware Configuration Files

| File | Purpose |
| ---- | ------- |
| `config/satellite1.yaml` | Standard variant (no radar) |
| `config/satellite1.ld2410.yaml` | LD2410 radar variant |
| `config/satellite1.ld2450.yaml` | LD2450 radar variant |
| `config/satellite1.base.yaml` | Base configuration (shared by all variants) |
| `config/common/*.yaml` | Modular components (voice, media, LEDs, buttons) |

---

## Contributing

We welcome contributions!

### How to Help

- 🐛 **Report bugs** — [open an issue](https://github.com/remcom/Satellite1-ESPHome/issues) if something doesn't work
- 💡 **Suggest features** — share ideas for improvements
- 🧪 **Test and provide feedback** — try the latest releases and share your experience
- 📝 **Improve documentation** — help others get started

### Choosing the Right Firmware

- **Want Sendspin with a fast-moving upstream-tracking release cadence?** → you're in the right place
- **Prefer the official FutureProofHomes-supported release?** → use [FutureProofHomes/Satellite1-ESPHome](https://github.com/FutureProofHomes/Satellite1-ESPHome)

---

## FAQ

**Is this ready for daily use?**
Yes — releases are versioned and tagged (currently v1.2.0), tested against real hardware, and built with linting/CI on every change. It isn't a one-off experiment; it's a maintained alternative firmware.

**Is this affiliated with FutureProofHomes?**
No. It's an independent, community-maintained fork of their open-source firmware. Hardware design and the original firmware are FutureProofHomes' work.

**Can I switch back to the official firmware later?**
Yes. Both firmwares target the same hardware; you can reflash via USB or OTA at any time.

**How is this different from the official firmware?**
Both now build on Sendspin for multi-room audio. This fork focuses on tracking upstream ESPHome releases quickly and shipping hardware/power-management fixes as they're found, backed by CI and linting on every change.

---

## Documentation & Resources

### Project Links

- 🌐 **Web Installer** — [remcom.github.io/Satellite1-ESPHome](https://remcom.github.io/Satellite1-ESPHome/)
- 📖 **Official FPH Firmware** — [FutureProofHomes/Satellite1-ESPHome](https://github.com/FutureProofHomes/Satellite1-ESPHome)
- 📘 **FutureProofHomes Docs** — [docs.futureproofhomes.net](https://docs.futureproofhomes.net)

### ESPHome & Home Assistant

- 🏠 **Home Assistant** — [home-assistant.io](https://www.home-assistant.io/)
- ⚡ **ESPHome** — [esphome.io](https://esphome.io)
- 🔊 **Sendspin** — native multi-room audio sync (part of ESPHome since 2026.6.0)
- 🎙️ **Voice PE Project** — [home-assistant-voice-pe](https://github.com/esphome/home-assistant-voice-pe)

---

## Credits

This project builds upon excellent work from:

- **[FutureProofHomes](https://futureproofhomes.net/)** — Satellite1 hardware design and original firmware
- **[Nabu Casa](https://nabucasa.com/)** — Home Assistant and ESPHome development
- **ESPHome Community** — Sendspin protocol and media player improvements
- **Contributors** — everyone who has tested, reported issues, and contributed code

---

## License

This project uses the same license as the original FutureProofHomes/Satellite1-ESPHome project.

See [LICENSE](LICENSE) for full details.

---

## Keywords

ESPHome firmware, Satellite1, voice assistant, multi-room audio, synchronized audio, Sendspin, Home Assistant, ESP32-S3, local voice control, smart home, whole home audio, presence detection, LD2410, LD2450, TAS2780, PCM5122, FUSB302B, FutureProofHomes alternative firmware
