# Satellite1 ESPHome Firmware — Sendspin Multi-Room Audio

**Stable, actively maintained ESPHome firmware for the FutureProofHomes Satellite1 voice assistant, tracking upstream ESPHome closely with ongoing hardware and power-management fixes.**

[![Firmware](https://img.shields.io/badge/Firmware-v1.3.0-brightgreen)](https://github.com/remcom/Satellite1-ESPHome/releases)
[![ESPHome](https://img.shields.io/badge/ESPHome-2026.11--dev-blue)](https://esphome.io)
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

This project tracks upstream ESPHome closely (currently a pinned 2026.11 dev commit, ahead of the 2026.11.0 release), ships fixes for LED/timer/button logic and DAC power management as they're found, and runs `clang-tidy` and pre-commit linting on every change. It's a smaller, focused codebase maintained specifically for the Satellite1 hardware, with a release cadence built around getting fixes and upstream ESPHome improvements out quickly.

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

## XMOS Firmware: Standard or True Beamforming

The XMOS audio chip on the HAT runs its own firmware, and you can choose between two variants of it:

| Variant | What it does |
|---------|--------------|
| **Standard (fixed delay)** | The default. Echo cancellation runs on the selected mic pair. Built into the ESP firmware, so it can always be reinstalled, even offline. |
| **True beamforming (AEC4, experimental)** | Echo cancellation runs on **all four mics before the beamformer**. The beam then gets echo-cancelled audio from every mic, and steering it no longer makes the echo canceller start over. Downloaded from the [XMOS release](https://github.com/remcom/Satellite1-XMOS/releases) when installed. **Not yet tested on hardware.** |

> [!WARNING]
> **Switching variants always reflashes the XMOS chip.** It's installed like an update, takes about a minute, the LED ring shows the progress, and voice and audio don't work until it finishes. Don't unplug the device while it runs.

**To switch**, in Home Assistant, on the device page:

1. Set **XMOS Firmware Variant** to the variant you want. On its own this changes nothing, but **XMOS Firmware Update** now shows an update is available, e.g. from `v2.0.0-beta.2 (fixed_delay)` to `v2.0.0-beta.2 (aec4)`.
2. Open **XMOS Firmware Update** (also listed under *Settings → Updates*) and press **Install**.
3. Home Assistant shows the progress bar, and the LED ring shows it too. When it's done, the update shows as up to date.

To go back, select **Standard (fixed delay)** and install again.

**The audio settings switch along with the firmware.** Each variant keeps its own Mic Mode, Beam Steering, Wake Word Channel, Fast Speech Hold and Mic High-Pass Filter. When the XMOS comes up with the other variant, your settings for the old one are saved and the new one's are loaded. The first time AEC4 runs it starts with **Mic Mode "Beam"**. Your standard settings stay as they were. Mic Gain is shared by both. **Reset XMOS Audio Settings** loads the recommended settings for the running variant.

Good to know:

- Your choice is kept across ESP firmware updates. When an update brings a new XMOS release, the device installs that release in the variant you selected. If the AEC4 download fails, for example without internet, the device installs the standard variant instead so it keeps working, and **XMOS Firmware Update** shows an update again, with the reason in its release notes. Press **Install** to retry.
- If an install fails, **XMOS Firmware Update** shows why, e.g. *the firmware image couldn't be downloaded*.
- If a download breaks off halfway, the standard variant is installed automatically, so the XMOS is never left without firmware.
- AEC4 cancels against mono speaker audio. With the speaker set to "Left only" or "Right only", or on the stereo line-out, stereo music is only partly cancelled. Spoken replies and announcements are mono and unaffected.

### XMOS audio settings per firmware

These are the starting values for each variant. Once you change a setting, the variant keeps your value, and **Reset XMOS Audio Settings** brings back the values below.

| Setting | Standard | AEC4 | What it does and why it helps | Top cover removed? |
|---------|----------|------|-------------------------------|--------------------|
| **Mic Mode** | East/West pair, or what you had set before | **Beam** | Which of the four mics feed the voice pipeline. See the table below | Depends on the mode |
| **Beam Steering** | Wake word only | Wake word only | What may turn the beam (in Beam) or switch the mic pair (in Follow talker). **Wake word only** turns towards you when you say the wake word and then holds, so a TV or radio can't pull the beam away. It stays in the blocking beam, where it gets cancelled. **Follow speech** also turns towards anyone talking. **Fixed** never turns. Has no effect in the fixed pair modes or Average | **Yes**, it steers by the voice direction (except Fixed) |
| **Wake Word Channel** | Noise suppressed | Noise suppressed | Which signal the wake word engine listens to. *Noise suppressed* is the cleanest. *Before noise suppression* and *Echo cancelled only* keep more of your voice's character, which can help if the wake word gets missed. Hidden by default | No |
| **Fast Speech Hold** | On | On | The interference canceller stops adapting within ~30 ms of speech, instead of ~400 ms, so it doesn't learn to cancel the start of your wake word. Hidden by default | No |
| **Mic High-Pass Filter** | On | On | Cuts rumble below 100 Hz (traffic, appliances, footsteps) before the echo canceller. Speech isn't affected. Hidden by default | No |
| **Mic Gain** | Shared, 20 dB | Shared, 20 dB | Microphone level after echo cancellation, 0–30 dB. Not stored per variant, since the mics are the same. Raise it if the wake word only works up close | No |
| **Direction Indicator** | Shared, off | Shared, off | LED on the ring that points at where the device listens: the beam in Beam mode, otherwise the talker. Not stored per variant | **Yes**, it shows the voice direction |

**Mic Mode options:**

| Mic Mode | What it does | Benefit | Top cover removed? |
|----------|--------------|---------|--------------------|
| **East/West pair** | Uses the east and west mics | The proven default. Predictable, because nothing depends on the voice direction | No |
| **North/South pair** | Uses the north and south mics | Try it if the east/west mics face a wall or a noise source | No |
| **Average** | Averages the mics in pairs, (N+E) and (S+W) | Picks up from all sides a bit more evenly, still without using the voice direction | No |
| **Follow talker** | Picks the mic facing you and the one opposite | Uses the best-placed mics for wherever you stand | **Yes** |
| **Beam** | Uses all four mics: one beam points at you, a second beam hears the rest of the room and serves as the noise reference | The most noise rejection, from any direction. **Recommended with AEC4**: there, echo is removed from every mic before the beam, so turning the beam doesn't make echo cancellation start over. On the standard firmware the echo canceller re-adapts each time the beam turns | **Yes** |

> [!IMPORTANT]
> **Everything that uses the voice direction needs the top cover removed for now.** That's **Beam**, **Follow talker**, **Beam Steering** (except *Fixed*) and the **Direction Indicator**. With the cover on, sound reflects inside the enclosure, so the direction is unreliable and the beam may point the wrong way. With the cover on, use **East/West pair**, **North/South pair** or **Average**. Those work the same with or without the cover. This needs more tuning, so expect it to change.

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
- **Full-duplex I2S** — the microphones and the speaker share one I2S bus and run at the same time, so wake word detection keeps listening during music playback; the speaker amplifier and line-out DAC take turns on the bus through the router speaker
- **Reworked FUSB302B / USB-PD driver** — a substantial internal rework of the USB-PD stack for more reliable power negotiation
- **LED and timer fixes** — corrected mute/volume LED indication and timer auto-off behavior, found and fixed through day-to-day use of the hardware

### Upstreaming to ESPHome

A long-term goal of this fork is to get the Satellite1 hardware drivers merged into ESPHome core, so the broader community — not just Satellite1 owners — can use them directly without a fork:

- ✅ **PCM5122 DAC** — merged upstream; now used directly from ESPHome core rather than a local component
- ✅ **TAS2780 amplifier** — merged upstream ([esphome#15708](https://github.com/esphome/esphome/pull/15708)); now used directly from ESPHome core
- ✅ **Full-duplex I2S** — merged upstream ([esphome#19959](https://github.com/esphome/esphome/pull/19959)); the full-duplex `i2s_audio` bus is used directly from ESPHome core (2026.11), replacing the former local shared-bus fork
- ✅ **Microphone resampler** — merged upstream; the `resampler` microphone platform is used directly from ESPHome core (2026.10)
- ✅ **FUSB302B USB-PD** — merged upstream; `fusb302b` is used directly from ESPHome core (2026.11), replacing the former local component

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

- **ESPHome** 2026.11.0 or newer — until 2026.11.0 is released that means ESPHome `dev`; 2026.10 and older refuse to build this firmware
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
Yes — releases are versioned and tagged (currently v1.3.0), tested against real hardware, and built with linting/CI on every change. It isn't a one-off experiment; it's a maintained alternative firmware.

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
