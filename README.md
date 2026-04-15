# Satellite1 ESPHome Firmware - Sendspin Multi-Room Audio

**ESPHome firmware for FutureProofHomes Satellite1 with synchronized multi-room audio using Sendspin**

> **Beta Release** | Based on the original [FutureProofHomes/Satellite1-ESPHome](https://github.com/FutureProofHomes/Satellite1-ESPHome) project

[![ESPHome](https://img.shields.io/badge/ESPHome-2026.4.0-blue)](https://esphome.io)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-Compatible-green)](https://www.home-assistant.io/)
[![License](https://img.shields.io/badge/License-Same%20as%20upstream-lightgrey)](LICENSE)

---

## 🎯 What is This?

This firmware enables **synchronized multi-room audio** on the [FutureProofHomes Satellite1](https://futureproofhomes.net/) voice assistant hardware using **Sendspin** - a next-generation ESPHome audio synchronization protocol. Perfect for whole-home voice control and perfectly synchronized music playback across multiple rooms.

### Key Features

- ✅ **Sendspin Multi-Room Audio** - Play music in perfect sync across multiple Satellite1 devices
- ✅ **Home Assistant Voice Assistant** - Full integration with Home Assistant's local voice control
- ✅ **ESPHome Dashboard Support** - Easy adoption and automatic updates
- ✅ **Presence Detection** - Optional LD2410/LD2450 radar sensor support
- ✅ **Power Optimized** - Smart DAC power management for reduced energy consumption
- ✅ **Professional Hardware Drivers** - Optimized TAS2780, PCM5122, and FUSB302B drivers

---

## ⚠️ Important Notice

- ❌ **No Snapcast support** - This fork uses [Sendspin](https://github.com/esphome/esphome/pull/12284) instead
- ✅ **For Snapcast users** - Please use the official [FutureProofHomes firmware](https://github.com/FutureProofHomes/Satellite1-ESPHome)

---

## 🚀 Quick Install

Install firmware directly from your browser (Chrome/Edge required):

### **[→ Install Firmware Now](https://remcom.github.io/Satellite1-ESPHome/site/)**

**Available Variants:**

| Variant | Best For | Features |
|---------|----------|----------|
| **Standard** | Voice assistant + music | Base firmware without radar |
| **LD2410** | Room presence detection | + HLK-LD2410 presence sensor |
| **LD2450** | Advanced zone tracking | + HLK-LD2450 multi-zone radar |

---

## 🎵 What Makes This Different?

### Sendspin vs. Snapcast

This firmware uses **Sendspin** for multi-room audio synchronization instead of Snapcast:

- **Native ESPHome integration** - No external servers required
- **Lower latency** - Optimized for ESP32 hardware
- **Better Home Assistant integration** - Seamless media player control
- **Future-proof** - Built for ESPHome's audio pipeline architecture

### Additional Enhancements

Beyond multi-room audio, this fork includes:

- **ESPHome Dashboard adoption** - One-click setup and automatic updates
- **Hardware diagnostics** - Optional TAS2780 voltage and temperature monitoring
- **Improved GPIO handling** - Enhanced reliability and null safety
- **Smart power management** - Automatic DAC shutoff when idle
- **Optimized memory usage** - Better SPI buffer allocation

---

## 💡 Use Cases

**Perfect for:**

- 🏠 **Whole-home voice control** - Control Home Assistant from any room
- 🎶 **Multi-room music** - Play synchronized music across your entire home
- 🛏️ **Presence-based automation** - Trigger actions based on room occupancy (with radar variants)
- 🔊 **High-quality audio** - Professional DAC and amplifier for excellent sound
- 🌐 **Local voice assistant** - Privacy-focused, works without cloud services

---

## 📋 System Requirements

### Hardware

- **FutureProofHomes Satellite1** (Core Board + HAT)
- **Optional:** LD2410 or LD2450 radar sensor for presence detection

### Software

- **ESPHome** 2026.4.0 or newer
- **Home Assistant** with Voice Assistant configured
- **Chrome or Edge browser** (for web installer)

### Recommended Setup

- Multiple Satellite1 devices for multi-room audio
- Stable Wi-Fi network (2.4GHz)
- Home Assistant OS or Supervised installation

---

## 🛠️ Building from Source

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

## 🤝 Contributing

We welcome contributions!

### How to Help

- 🐛 **Report bugs** - [Open an issue](https://github.com/remcom/Satellite1-ESPHome/issues) if something doesn't work
- 💡 **Suggest features** - Share ideas for improvements
- 🧪 **Test and provide feedback** - Try the latest beta releases
- 📝 **Improve documentation** - Help others get started

### Choosing the Right Firmware

- **Want stable, production-ready firmware?** → Use the official [FutureProofHomes/Satellite1-ESPHome](https://github.com/FutureProofHomes/Satellite1-ESPHome)
- **Need Snapcast support?** → Use the official FutureProofHomes firmware
- **Want Sendspin multi-room audio?** → You're in the right place!

---

## 📚 Documentation & Resources

### Project Links

- 🌐 **Web Installer** - [remcom.github.io/Satellite1-ESPHome](https://remcom.github.io/Satellite1-ESPHome/)
- 📖 **Official FPH Firmware** - [FutureProofHomes/Satellite1-ESPHome](https://github.com/FutureProofHomes/Satellite1-ESPHome)
- 📘 **FutureProofHomes Docs** - [docs.futureproofhomes.net](https://docs.futureproofhomes.net)

### ESPHome & Home Assistant

- 🏠 **Home Assistant** - [home-assistant.io](https://www.home-assistant.io/)
- ⚡ **ESPHome** - [esphome.io](https://esphome.io)
- 🔊 **Sendspin PR** - [esphome/esphome#12284](https://github.com/esphome/esphome/pull/12284)
- 🎙️ **Voice PE Project** - [home-assistant-voice-pe](https://github.com/esphome/home-assistant-voice-pe)

---

## 🙏 Credits

This project builds upon excellent work from:

- **[FutureProofHomes](https://futureproofhomes.net/)** - Satellite1 hardware design and original firmware
- **[Nabu Casa](https://nabucasa.com/)** - Home Assistant and ESPHome development
- **ESPHome Community** - Sendspin protocol and media player improvements
- **Contributors** - Everyone who has tested, reported issues, and contributed code

---

## 📄 License

This project uses the same license as the original FutureProofHomes/Satellite1-ESPHome project.

See [LICENSE](LICENSE) for full details.

---

## 🔍 Keywords

ESPHome firmware, Satellite1, voice assistant, multi-room audio, synchronized audio, Sendspin, Home Assistant, ESP32, local voice control, smart home, whole home audio, presence detection, LD2410, LD2450, TAS2780, PCM5122, FutureProofHomes
