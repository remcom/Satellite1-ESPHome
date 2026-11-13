# Satellite1 ESPHome Firmware — Sendspin Multi-Room Audio

**Actively maintained ESPHome firmware for the FutureProofHomes Satellite1 voice assistant: local voice control, synchronized 24-bit multi-room audio, a four-mic XMOS beamforming front end, and presence detection — tracking upstream ESPHome closely.**

[![Stable](https://img.shields.io/badge/Stable-v1.4.1-brightgreen)](https://github.com/remcom/Satellite1-ESPHome/releases/tag/v1.4.1)
[![Beta](https://img.shields.io/badge/Beta-v2.0.0--beta8-orange)](https://github.com/remcom/Satellite1-ESPHome/releases)
[![ESPHome](https://img.shields.io/badge/ESPHome-2026.11.0b4-blue)](https://esphome.io)
[![XMOS](https://img.shields.io/badge/XMOS-v2.0.0--beta.2-purple)](https://github.com/remcom/Satellite1-XMOS/releases/tag/v2.0.0-beta.2)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-Compatible-green)](https://www.home-assistant.io/)
[![License](https://img.shields.io/badge/License-Same%20as%20upstream-lightgrey)](LICENSE)

---

## What is This?

This is an actively maintained ESPHome firmware fork for the [FutureProofHomes Satellite1](https://futureproofhomes.net/) (Core Board + HAT) voice assistant hardware, built around **Sendspin** — ESPHome's native, in-tree audio synchronization protocol — for synchronized multi-room music playback alongside full local voice assistant support.

> [!CAUTION]
> **The 2.0 line (v2.0.0-betaN) is an early beta.** It runs on an ESPHome beta (2026.11.0b4) and on beta XMOS firmware from a fork, and much of it hasn't been tested widely on hardware yet. If you rely on your Satellite1 every day, **stay on [v1.4.1](https://github.com/remcom/Satellite1-ESPHome/releases/tag/v1.4.1)** for now.

### Highlights

- 🎶 **Sendspin multi-room audio** — synchronized playback across Satellite1s and other Sendspin players, streamed in **24-bit** FLAC or PCM from Music Assistant
- 🎙️ **Local voice assistant** — on-device wake words, Home Assistant Assist, timers, announcements
- 🎯 **XMOS four-mic front end** — echo cancellation, noise suppression, beamforming, direction of arrival and wake word beam steering, with a choice of **two XMOS firmware variants** ([guide](docs/XMOS.md))
- 📡 **Presence detection** — optional LD2410 or LD2450 mmWave radar builds
- 🔊 **Speaker and line-out** — TAS2780 amplifier and PCM5122 line-out DAC, auto power-down when idle
- 🔁 **OTA updates** — stable and beta channels, XMOS updates as a Home Assistant update entity
- 🔐 **Secure build** — encrypted credential storage and signature-verified OTA

---

## Quick Install

Install firmware directly from your browser (Chrome or Edge required, over USB):

### [→ Install Firmware Now](https://remcom.github.io/Satellite1-ESPHome/site/)

| Variant | Best For | Features |
|---------|----------|----------|
| **Standard** (`satellite1`) | Voice assistant + music | Base firmware without radar |
| **LD2410** (`satellite1.ld2410`) | Room presence detection | + HLK-LD2410 presence sensor |
| **LD2450** (`satellite1.ld2450`) | Zone and target tracking | + HLK-LD2450 multi-target radar |
| **Secure** (`satellite1.secure`) | Hardened installs | Encrypted credential storage + signature-verified OTA |

After installing, the device starts a **BLE Improv** session so you can set up WiFi from the Home Assistant app, then it's discovered by Home Assistant and the ESPHome Dashboard.

---

## Features

### 🎶 Music and Sendspin multi-room audio

- **Sendspin media source** — the Satellite1 is a Sendspin player: group it with other Satellite1s and Sendspin players (e.g. from Music Assistant) for sample-accurate synchronized playback, no separate sync server needed
- **24-bit streaming** — FLAC (preferred) or PCM at 24 bits, through a 32-bit mixer to the speaker and the line-out. Set `media_bits_per_sample: "16"` to save memory
- **Line-out latency compensation** — Sendspin's static delay adjustment switches on automatically when a 3.5 mm jack is plugged in
- **Now-playing info** — **Title**, **Artist**, **Track Progress**, **Track Duration**, **Track Number** and **Year** sensors, plus the **Sendspin Pairing Code**
- **Media player entity** — play any URL or Home Assistant media, with separate **Announcement Volume** so TTS and alerts stay audible on a quiet device
- **Speaker channel output** — *Mono (Left + Right)*, *Left Channel Only* or *Right Channel Only*, to build stereo pairs from two devices
- **Line-out jack detection** — **Line-Out Connected** sensor; the device switches between the TAS2780 speaker and the PCM5122 line-out automatically, with a sound and LED animation
- **Ducking** — music lowers during announcements and voice assistant replies

### 🎙️ Voice assistant

- **On-device wake words** with microWakeWord: **Okay Nabu**, **Hey Jarvis**, **Hey Mycroft**, plus **Stop** to end a timer or a reply
- **Wake word sensitivity** — *Slightly*, *Moderately* or *Very sensitive*
- **Wake word runs during music** — full-duplex I2S keeps the mics live while the speaker plays, and the XMOS removes the device's own audio
- **Both XMOS channels to Home Assistant** — HA 2026.6+ picks the better one per pipeline stage
- **Timers** — voice timers with LED countdown, a ringing sound and auto-stop
- **Wake sound** switch, **Mute Microphones** switch (follows the hardware mute switch), configurable **TTS start timeout**

### 🎯 XMOS audio front end

The XMOS chip on the HAT processes all four microphones: high-pass filter, echo cancellation, beamforming, interference cancellation, noise suppression, AGC and direction of arrival.

| Feature | Entity |
|---------|--------|
| Choose the mics or a beam | **Mic Mode** — East/West pair, North/South pair, Average, Follow talker, Beam |
| Turn the beam towards a wake word, and hold it there | **Beam Steering** — Wake word only, Follow speech, Fixed |
| Mic level | **Mic Gain** (0–30 dB) |
| LED pointing at where it listens | **Direction Indicator** |
| Voice direction for automations | **Voice Direction**, **Voice Direction Confidence** |
| Fine tuning | **Wake Word Channel**, **Fast Speech Hold**, **Mic High-Pass Filter** |
| Two XMOS firmware variants | **XMOS Firmware Variant**, **XMOS Firmware Update**, **Reset XMOS Audio Settings** |

**Two XMOS firmware variants:**

| Variant | Summary |
|---------|---------|
| **Standard (fixed delay)** | The default. Echo cancellation on the selected mic pair. Embedded in the ESP firmware, so it can always be reinstalled offline |
| **True beamforming (AEC4, experimental)** | Echo cancellation on all four mics before the beamformer, so steering the beam doesn't make echo cancellation start over. Downloaded when installed. Not yet tested on hardware |

Each variant keeps its own audio settings, and switching is a one-click install from the **XMOS Firmware Update** entity.

> [!IMPORTANT]
> Beam, Follow talker, Beam Steering and the Direction Indicator need the **top cover removed** for now; with the cover on, use a fixed mic pair or Average.

📖 **Full guide: [docs/XMOS.md](docs/XMOS.md)** — how each setting works, how to switch variants, recommended setups, recovery and troubleshooting.

### 💡 LED ring

- **24-LED ring** with animations for every voice assistant phase (waiting, listening, thinking, replying), errors, mute, volume, timers, jack plug/unplug, XMOS flashing and factory reset
- **LED Ring** light entity for your own use when the device is idle
- **Current limit** — the ring is dimmed just enough to stay under 1000 mA (`led_ring_max_current_ma`), with an **LED Ring Current** diagnostic sensor

### 🔘 Buttons

| Button | Action |
|--------|--------|
| **Up / Down** | Volume up / down, shown on the LED ring |
| **Left (hardware switch)** | Mute the microphones |
| **Action — single press** | Stops, in order: a ringing timer, the voice assistant, an announcement, music. Otherwise starts the voice assistant |
| **Action — double / triple / long press** | Exposed as the **Action Button Press** event entity for your own automations |
| **Action — hold 10 s** | Factory reset warning (sound + LEDs); release to cancel |
| **Action — hold 22 s** | Factory reset (also erases the XMOS, which is reinstalled on the next boot) |

### 📡 Presence detection (radar variants)

**LD2410** — presence, moving/still target, distances and energies, per-gate thresholds, light sensor, and **background noise correction** (needs LD2410 firmware V2.44+).

**LD2450** — up to three tracked targets with X/Y, speed, angle, distance and direction; three rectangle zones; three **polygon zones** (3–23 points, checked on the device) with presence sensors; **LD2450 Connected** watchdog.

### 🌡️ Sensors and diagnostics

- **Temperature** and **Humidity** (AHT20), with humidity recomputed for the board's self-heating (Magnus formula); **Offset Temperature** / **Offset Humidity** sliders for calibration
- **Ambient Light** (LTR-303), **Wi-Fi Signal RSSI**, **Internal Temperature**, **Uptime**
- **USB-C Power Contract** — the USB-PD contract negotiated by the FUSB302B
- **XMOS Firmware** version and variant
- Debug entities (hidden by default): heap, PSRAM, loop time, reset reason, XMOS reset/flash/erase buttons, safe-mode restart

### ⚙️ System

- **OTA updates** from GitHub releases, with a **Beta firmware** switch to follow the beta channel
- **XMOS firmware updates** as a regular Home Assistant update with progress, automatic fallback to the embedded image, and failure reasons
- **ESPHome Dashboard adoption** and remote compile (`satellite1-dashboard.yaml`, `satellite1-remote.yaml`)
- **Fast WiFi reconnect** after a reboot (DHCP lease cache)
- **Power management** — TAS2780 and PCM5122 power down when idle
- **Opt-in: share the microphone with Sendspin** — stream the XMOS-processed mic to a Sendspin server (intercom, room audio). Build-it-yourself only, off by default, see `common/sendspin_source.yaml`

---

## Why This Fork

This project tracks upstream ESPHome closely (currently the **2026.11.0b4** beta), ships fixes for LED/timer/button logic and DAC power management as they're found, and runs `clang-tidy` and pre-commit linting on every change. It's a smaller, focused codebase maintained specifically for the Satellite1 hardware, with a release cadence built around getting fixes and upstream ESPHome improvements out quickly.

Both this fork and the official [FutureProofHomes firmware](https://github.com/FutureProofHomes/Satellite1-ESPHome) target the same hardware and both build on Sendspin. The main differences are the release cadence and the XMOS firmware: this fork runs its own [XMOS firmware](https://github.com/remcom/Satellite1-XMOS) with direction of arrival, beamforming and the AEC4 variant.

### Upstreaming to ESPHome

A long-term goal of this fork is to get the Satellite1 hardware drivers merged into ESPHome core, so the broader community can use them without a fork:

- ✅ **PCM5122 DAC** — merged upstream; used directly from ESPHome core
- ✅ **TAS2780 amplifier** — merged upstream ([esphome#15708](https://github.com/esphome/esphome/pull/15708))
- ✅ **Full-duplex I2S** — merged upstream ([esphome#19959](https://github.com/esphome/esphome/pull/19959)), ESPHome 2026.11
- ✅ **Microphone resampler** — merged upstream, ESPHome 2026.10
- ✅ **FUSB302B USB-PD** — merged upstream, ESPHome 2026.11

What's left locally is the `satellite1` component (SPI link to the XMOS, its settings, DoA sensor and update entity) and the `memory_flasher` that flashes the XMOS.

---

## System Requirements

### Hardware

- **FutureProofHomes Satellite1** (Core Board + HAT)
- **Optional:** LD2410 or LD2450 radar sensor

### Software

- **Home Assistant** with a voice assistant pipeline configured (2026.6+ recommended for dual-channel audio)
- **Music Assistant** with Sendspin for multi-room music (optional)
- **Chrome or Edge** for the web installer
- **To build yourself: ESPHome 2026.11.0b1 or newer** (2026.11.0b4 is what releases are built with). ESPHome 2026.10 and older, including the current Home Assistant add-on release, refuse to build this firmware. The build downloads the XMOS image, so it needs internet

### Recommended Setup

- Stable 2.4 GHz Wi-Fi
- Home Assistant OS or Supervised installation

---

## Building from Source

```bash
# Clone the repository
git clone https://github.com/remcom/Satellite1-ESPHome.git
cd Satellite1-ESPHome

# Set up the build environment (creates .venv, installs the pinned ESPHome)
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
| `config/satellite1.secure.yaml` | Secure variant (NVS encryption, signed OTA; needs your own signing key) |
| `config/satellite1.common.yaml` | Shared by the local builds: OTA, beta switch, boot-time XMOS update |
| `config/satellite1.base.yaml` | Base configuration: substitutions, `satellite1` component, XMOS flasher, packages |
| `config/satellite1-dashboard.yaml` | ESPHome Dashboard adoption |
| `config/satellite1-remote.yaml` | Remote compile from GitHub |

### Packages (`config/common/`)

| Package | Contents |
| ------- | -------- |
| `core_board.yaml` | ESP32-S3, I2S bus, microphones, core sensors |
| `wifi_improv.yaml` | WiFi and BLE Improv |
| `hat_sensors.yaml` | Temperature, humidity, illuminance |
| `buttons.yaml` | Buttons, action button events, factory reset |
| `home_assistant.yaml` | Restart/reset, wake sound, mute, XMOS Firmware sensor |
| `media_player.yaml` | Media player, Sendspin, mixer, DACs, now-playing sensors |
| `voice_assistant.yaml` | Voice assistant and microWakeWord |
| `timer.yaml` | Voice timers |
| `led_ring.yaml` | LED ring and effects |
| `xmos_audio.yaml` | XMOS audio settings, DoA, beam steering |
| `xmos_firmware.yaml` | XMOS variant select, update entity, per-variant settings |
| `debug.yaml` | Diagnostic sensors and XMOS debug buttons |
| `mmwave_ld2410.yaml` / `mmwave_ld2450.yaml` | Radar (optional) |
| `sendspin_source.yaml` | Share the mic with Sendspin (optional) |
| `developer.yaml` | Amplifier level and volume range tuning (optional) |

### Useful substitutions

| Substitution | Default | What it does |
| ------------ | ------- | ------------ |
| `friendly_name` | `Satellite1` | Device name |
| `temp_offset` / `humidity_offset` | `-3` / `0` | Sensor calibration |
| `media_bits_per_sample` | `"24"` | Sendspin stream bit depth (`"16"` saves memory) |
| `led_ring_max_current_ma` | `"1000"` | LED ring current limit |
| `micro_wake_word_task_core` | `"1"` | CPU core for wake word inference |
| `micro_wake_word_ring_buffer_duration` | `250ms` | How far the wake word engine may fall behind |
| `tts_playback_start_timeout` | `5s` | How long a spoken reply may take to start |
| `xmos_fw_version` | `"v2.0.0-beta.2"` | XMOS release to install |
| `doa_led_offset` / `doa_led_clockwise` | `"0"` / `"true"` | Direction Indicator calibration |

More XMOS-related options are in the [XMOS guide](docs/XMOS.md#for-people-with-their-own-yaml).

---

## FAQ

**Is this ready for daily use?**
The stable line (v1.4.1) is. The 2.0 betas bring a lot of new audio and XMOS features and are for testers; each release has detailed notes on what changed and how to go back.

**Is this affiliated with FutureProofHomes?**
No. It's an independent, community-maintained fork of their open-source firmware. Hardware design and the original firmware are FutureProofHomes' work.

**Can I switch back to the official firmware later?**
Yes. Both target the same hardware; reflash over USB or OTA at any time. The XMOS is reflashed by whichever firmware you install.

**Which XMOS variant should I use?**
Standard, unless you've removed the top cover and want better wake word detection while music plays — then try AEC4 with Mic Mode *Beam*. See [docs/XMOS.md](docs/XMOS.md).

**Does it work without internet?**
Voice control works fully locally with a local Home Assistant pipeline. Installing the AEC4 XMOS variant and OTA updates need internet; the Standard XMOS variant is always available offline.

---

## Contributing

- 🐛 **Report bugs** — [open an issue](https://github.com/remcom/Satellite1-ESPHome/issues)
- 💡 **Suggest features** — share ideas for improvements
- 🧪 **Test betas** — each release note lists what feedback is most useful
- 📝 **Improve documentation** — help others get started

---

## Documentation & Resources

- 📖 **XMOS guide** — [docs/XMOS.md](docs/XMOS.md)
- 🌐 **Web Installer** — [remcom.github.io/Satellite1-ESPHome](https://remcom.github.io/Satellite1-ESPHome/)
- 🎛️ **XMOS firmware** — [remcom/Satellite1-XMOS](https://github.com/remcom/Satellite1-XMOS)
- 📦 **Releases and release notes** — [GitHub releases](https://github.com/remcom/Satellite1-ESPHome/releases)
- 📘 **Official FPH firmware** — [FutureProofHomes/Satellite1-ESPHome](https://github.com/FutureProofHomes/Satellite1-ESPHome) · [docs.futureproofhomes.net](https://docs.futureproofhomes.net)
- 🏠 [Home Assistant](https://www.home-assistant.io/) · ⚡ [ESPHome](https://esphome.io) · 🎙️ [Voice PE](https://github.com/esphome/home-assistant-voice-pe)

---

## Credits

- **[FutureProofHomes](https://futureproofhomes.net/)** — Satellite1 hardware design and original firmware
- **[Nabu Casa](https://nabucasa.com/)** — Home Assistant and ESPHome development
- **ESPHome Community** — Sendspin protocol and media player improvements
- **Contributors** — everyone who has tested, reported issues, and contributed code

---

## License

This project uses the same license as the original FutureProofHomes/Satellite1-ESPHome project. See [LICENSE](LICENSE) for details.

---

## Keywords

ESPHome firmware, Satellite1, voice assistant, multi-room audio, synchronized audio, Sendspin, Music Assistant, Home Assistant, ESP32-S3, XMOS, beamforming, echo cancellation, direction of arrival, local voice control, presence detection, LD2410, LD2450, TAS2780, PCM5122, FUSB302B, FutureProofHomes alternative firmware
