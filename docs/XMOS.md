# XMOS Audio Front End — Guide

The Satellite1 HAT has an **XMOS XU316** chip that processes the four microphones before the ESP32 sees any audio. It cleans up your voice for the wake word and Home Assistant, works out where you're talking from, and points a beam at you.

This guide covers:

- [What the XMOS does](#what-the-xmos-does)
- [The two XMOS firmware variants: Standard and True beamforming (AEC4)](#the-two-xmos-firmware-variants)
- [Switching between the variants](#switching-between-the-variants)
- [Audio settings reference](#audio-settings-reference)
- [Direction of arrival and the Direction Indicator](#direction-of-arrival-and-the-direction-indicator)
- [Recommended setups](#recommended-setups)
- [Firmware updates, failures and recovery](#firmware-updates-failures-and-recovery)
- [Diagnostic entities and buttons](#diagnostic-entities-and-buttons)
- [Troubleshooting](#troubleshooting)
- [For people with their own YAML](#for-people-with-their-own-yaml)

> [!CAUTION]
> The XMOS firmware is **v2.0.0-beta.2** from [remcom/Satellite1-XMOS](https://github.com/remcom/Satellite1-XMOS/releases). It's a beta from a fork, **not an official FutureProofHomes release**. Direction of arrival was tuned mostly in simulation, and the **AEC4 variant has not been tested on hardware yet**.

---

## What the XMOS does

```
 4 mics ──► high-pass ──► echo canceller (AEC) ──► mic selection / beam ──► interference ──► noise ──► AGC ──► channel 0 ──► Home Assistant
 (N,E,S,W)   filter         (uses speaker audio      (Mic Mode)               canceller (IC)   suppression          │
                             as reference)                                                   (NS)        └─────► channel 1 ──► wake word + Home Assistant
                                    │                                                                            (no AGC, tap point selectable)
                                    └──► direction of arrival (DoA) ──► beam steering, Voice Direction sensor, LED indicator
```

| Stage | What it does |
|-------|--------------|
| **High-pass filter** | Removes rumble below 100 Hz (traffic, appliances, footsteps). Speech isn't affected |
| **Echo canceller (AEC)** | Removes what the Satellite1's own speaker plays from the mic signal, so it can hear you over its music |
| **Mic selection / beam** | Picks which mics are used, or combines all four into a beam pointed at you (see [Mic Mode](#mic-mode)) |
| **Interference canceller (IC)** | Removes steady sound sources that aren't in the direction of your voice, e.g. a TV |
| **Noise suppression (NS)** | Removes background hiss and noise |
| **AGC** | Brings speech up to a steady level for Home Assistant |
| **Direction of arrival (DoA)** | Estimates where speech comes from (0–359°, 0° = North mic, 90° = East mic) |

The XMOS sends **two channels** to the ESP32:

- **Channel 0** — the fully processed signal, with AGC
- **Channel 1** — the same signal **without AGC** (the tap point is selectable, see [Wake Word Channel](#wake-word-channel)). The wake word engine listens here, because the AGC drives speech so close to full scale that any extra gain clipped the wake word

Both channels go to Home Assistant. Home Assistant 2026.6 and newer picks the better one per pipeline stage; older versions use channel 0.

The XMOS doesn't remember its settings. The ESP32 stores them and sends them every time the XMOS (re)connects, so a reboot, a reflash or an XMOS reset never loses them.

---

## The two XMOS firmware variants

Every XMOS release comes in two variants. They share the same version number (e.g. `v2.0.0-beta.2`), so the variant is shown separately, e.g. `v2.0.0-beta.2 (fixed_delay)` or `v2.0.0-beta.2 (aec4)`.

| | **Standard (fixed delay)** | **True beamforming (AEC4, experimental)** |
|---|---|---|
| **Internal name** | `fixed_delay` | `aec4` |
| **Echo cancellation runs on** | The selected mic pair (2 channels) | **All four mics, before the beamformer** |
| **When the beam turns or the mic pair changes** | The echo canceller has to re-adapt, so echo leaks through for a moment | Nothing changes for the echo canceller, it already covers every mic |
| **Best Mic Mode** | East/West pair (works with the cover on) | **Beam** (needs the top cover removed for now) |
| **Where the image comes from** | **Embedded in the ESP firmware.** Can always be (re)installed, also without internet | **Downloaded** from the [XMOS release](https://github.com/remcom/Satellite1-XMOS/releases) when installed. Needs internet |
| **Echo reference** | Speaker audio | **Mono** speaker audio |
| **Status** | Default. Same audio pipeline as earlier 2.0 betas | **Not yet tested on hardware**, built and measured in the XMOS simulator only |

### Which one should I pick?

- **Keep Standard** if your device is in daily use, the top cover is on, or you have no reliable internet. It's the proven setup.
- **Try AEC4** if you remove the top cover and want the best wake word detection **while music is playing**. That's exactly where AEC4 should do better: the beam can follow you without the echo canceller starting over each time.

### AEC4 and stereo

AEC4 cancels against a **mono** mix of what the speaker plays. That's also what the TAS2780 amplifier plays by default (`Speaker channel output` = *Mono (Left + Right)*).

- With `Speaker channel output` on **Left Channel Only** or **Right Channel Only**, or when you use the **3.5 mm line-out** with stereo music, the echo canceller only partly removes stereo music.
- Spoken replies, announcements and timer sounds are mono and are always fully cancelled.

---

## Switching between the variants

> [!WARNING]
> **Switching variants always reflashes the XMOS chip.** It takes about a minute. The LED ring shows the progress and voice and audio don't work until it's done. **Don't unplug the device while it runs.**

Picking a variant on its own changes nothing. The reflash only starts when you press **Install**.

On the device page in Home Assistant:

1. Under **Configuration**, set **XMOS Firmware Variant** to the variant you want.
   **XMOS Firmware Update** now shows an update, e.g. from `v2.0.0-beta.2 (fixed_delay)` to `v2.0.0-beta.2 (aec4)`.
2. Open **XMOS Firmware Update** (also under *Settings → Updates*) and press **Install**.
3. Home Assistant shows a progress bar, and the LED ring shows it too (blue). When it's done, **XMOS Firmware Update** shows as up to date and the audio settings for that variant are loaded.

To go back, select **Standard (fixed delay)** and install again. Going back to Standard never needs internet, the image is inside the ESP firmware.

### Each variant has its own audio settings

These settings are saved **per variant**:

- Mic Mode
- Beam Steering
- Wake Word Channel
- Fast Speech Hold
- Mic High-Pass Filter

When the XMOS comes up running a different variant than last time, the current settings are saved for the old variant and the new variant's settings are loaded. That works however the variant got installed: from the update entity, from the automatic update after an ESP update, or from the debug buttons.

- **The first time AEC4 runs**, it starts with **Mic Mode = Beam**, Beam Steering *Wake word only*, Wake Word Channel *Noise suppressed*, Fast Speech Hold and High-Pass Filter on.
- **Your Standard settings stay as they were**, including anything you set before variants existed.
- **Mic Gain** and **Direction Indicator** are **shared** by both variants: the mics and the LEDs are the same.
- **Reset XMOS Audio Settings** puts the running variant back on its recommended settings (see the table below).

---

## Audio settings reference

All settings are on the device page in Home Assistant under **Configuration**. Some are hidden by default to keep the page tidy; enable them under the device's *entities not shown* if you want to tune them. Hidden entities still work on the device.

| Setting | Type | Standard default | AEC4 default | Per variant | Hidden by default | Needs top cover removed |
|---------|------|------------------|--------------|:-----------:|:-----------------:|:-----------------------:|
| [Mic Mode](#mic-mode) | select | East/West pair | **Beam** | ✅ | | Depends on the mode |
| [Beam Steering](#beam-steering) | select | Wake word only | Wake word only | ✅ | | Yes (except *Fixed*) |
| [Wake Word Channel](#wake-word-channel) | select | Noise suppressed | Noise suppressed | ✅ | ✅ | No |
| [Fast Speech Hold](#fast-speech-hold) | switch | On | On | ✅ | ✅ | No |
| [Mic High-Pass Filter](#mic-high-pass-filter) | switch | On | On | ✅ | ✅ | No |
| [Mic Gain](#mic-gain) | number | 20 dB | 20 dB (shared) | | | No |
| [Direction Indicator](#direction-of-arrival-and-the-direction-indicator) | switch | Off | Off (shared) | | | Yes |

> [!IMPORTANT]
> **Everything that uses the voice direction needs the top cover removed for now.** That's **Beam**, **Follow talker**, **Beam Steering** (except *Fixed*) and the **Direction Indicator**. With the cover on, sound reflects inside the enclosure, the measured direction is unreliable and the beam may point the wrong way. With the cover on, use **East/West pair**, **North/South pair** or **Average** — those don't depend on the direction and work the same either way. This needs more tuning, so expect it to change.

### Mic Mode

Which of the four microphones feed the voice pipeline.

| Option | What it does | When to use it | Cover removed? |
|--------|--------------|----------------|:--------------:|
| **East/West pair** | Uses the east and west mics | The proven default. Predictable, nothing depends on the voice direction | No |
| **North/South pair** | Uses the north and south mics | When the east or west mic faces a wall or a noise source | No |
| **Average** | Averages the mics in pairs, (N+E) and (S+W) | Picks up from all sides a bit more evenly, still without using the direction | No |
| **Follow talker** | Picks the mic facing you and the one opposite | Uses the best-placed pair for wherever you stand | **Yes** |
| **Beam** | Uses all four mics: one beam points at you, a second beam hears the rest of the room and serves as the noise reference for the interference canceller | The most noise rejection, from any direction. **Recommended with AEC4.** On Standard, the echo canceller re-adapts each time the beam turns | **Yes** |

### Beam Steering

What may turn the beam (in **Beam**) or switch the mic pair (in **Follow talker**). Has no effect in the fixed pair modes or **Average**.

| Option | Behaviour |
|--------|-----------|
| **Wake word only** *(default)* | Turns towards you when you say the wake word, then holds. A TV or radio can't pull the beam away from you; it stays in the blocking beam, where it gets cancelled |
| **Follow speech** | Also turns towards anyone who talks, not just after a wake word. Wake words still steer immediately |
| **Fixed** | Never turns |

How wake word steering works: when the ESP32 detects the wake word, it tells the XMOS straight away (before the wake sound plays, because the XMOS pauses direction finding during playback). The XMOS steers to the direction the wake word came from within one 15 ms audio frame.

### Wake Word Channel

Which point in the XMOS pipeline channel 1 (the wake word channel) is taken from. Channel 0 is not affected.

| Option | Signal |
|--------|--------|
| **Noise suppressed** *(default)* | Echo cancelled + interference cancelled + noise suppressed. The cleanest |
| **Before noise suppression** | Echo cancelled + interference cancelled. Keeps more of your voice's character |
| **Echo cancelled only** | Only the echo canceller. The most natural, but also the most room noise |

Try the other two if the wake word gets missed and the room is quiet.

### Fast Speech Hold

The interference canceller learns what to remove, and must stop learning while you talk, or it learns to cancel your voice. With **Fast Speech Hold on** (default) it stops within **~30 ms** of speech starting, instead of ~400 ms. That keeps it from cancelling the start of your wake word. Only turn it off to compare.

### Mic High-Pass Filter

A 100 Hz high-pass filter on the mics, before the echo canceller. Removes rumble (traffic, appliances, footsteps) without touching speech. On by default.

### Mic Gain

Microphone level after echo cancellation, **0–30 dB**, default **20 dB**. Shared by both variants. Raise it if the wake word only works up close; lower it if you get false triggers in a loud room.

### Reset XMOS Audio Settings

A button that loads the recommended settings for the variant that's **currently running**:

| | Mic Mode | Beam Steering | Wake Word Channel | Fast Speech Hold | High-Pass Filter |
|---|---|---|---|---|---|
| **Standard** | East/West pair | Wake word only | Noise suppressed | On | On |
| **AEC4** | Beam | Wake word only | Noise suppressed | On | On |

Mic Gain and Direction Indicator are left alone.

---

## Direction of arrival and the Direction Indicator

The XMOS estimates where speech comes from, using frequencies up to 3000 Hz (higher frequencies alias over the 7.1 cm between opposite mics and blur the estimate in rooms with echo).

- **Direction Indicator** (switch, off by default): while the voice assistant waits for or listens to a command, one LED on the ring points at where the device is listening — the beam direction in **Beam** mode, otherwise the talker. It keeps the last direction during silence or playback.
- **Voice Direction** (sensor, degrees, hidden by default): the last measured direction, 0° = North mic, 90° = East mic. Updated every second.
- **Voice Direction Confidence** (sensor, hidden by default): how sure the XMOS is, 0–255.

> [!NOTE]
> The LED position isn't calibrated yet. If the LED points the wrong way, see [Calibrating the indicator](#calibrating-the-direction-indicator) below (needs your own build).

---

## Recommended setups

| Situation | Variant | Mic Mode | Beam Steering | Direction Indicator |
|-----------|---------|----------|---------------|---------------------|
| **Daily use, cover on** | Standard | East/West pair (or North/South if E/W faces a wall) | — (no effect) | Off |
| **Cover off, mostly voice, little music** | Standard | Beam or Follow talker | Wake word only | Optional |
| **Cover off, wake word during music** | **AEC4** | **Beam** | Wake word only | Optional |
| **TV in the room** | Either | Beam (cover off) or a fixed pair | **Wake word only** | — |
| **Several people talking in turn** | AEC4 | Beam | Follow speech | Optional |

---

## Firmware updates, failures and recovery

### Automatic updates

Each ESP firmware release is built for one XMOS release (currently `v2.0.0-beta.2`). On the first boot after an ESP update that brings a new XMOS release, the device updates the XMOS by itself, **in the variant you selected**:

- **Standard**: installed from the image embedded in the ESP firmware.
- **AEC4**: the device waits up to 60 s for WiFi and downloads the image. If WiFi doesn't come up or the download fails, it installs **Standard** instead so the device keeps working. **XMOS Firmware Update** then shows an update again, with the reason in its release notes. Press **Install** to retry.

If the XMOS doesn't respond at all at boot (e.g. erased), the embedded Standard image is installed automatically.

### When an install fails

- **XMOS Firmware Update** shows why in its release summary, e.g. *the firmware image couldn't be downloaded*. The message stays until the next install, or until the XMOS runs the selected firmware.
- **If the AEC4 download fails before anything was erased**, the XMOS keeps the firmware it had.
- **If the download breaks off after the XMOS flash was erased**, the embedded Standard image is installed automatically. The XMOS is never left without firmware.
- The LED ring progress is blue while flashing, turns green on success and red when flashing fails.

### Things to know

- **Your variant choice survives reboots and ESP updates.**
- **Don't unplug the device while the XMOS is flashed.** If it happens anyway, the device reinstalls the embedded image on the next boot.
- **The factory reset (hold the action button 22 s) also erases the XMOS**, which then gets the embedded Standard image on the next boot. A factory reset is skipped while the XMOS is being flashed.

### Going back to older ESP firmware

- **v2.0.0-beta6 or older** flashes the XMOS back to its own release (Standard) on the first boot, also if you had AEC4. The variant entities show as unavailable and can be removed. If you were on AEC4, check **Mic Mode** afterwards.
- **v2.0.0-beta1** doesn't know **Beam**: set Mic Mode to another option first.
- **v1.4.x** flashes XMOS firmware v1.0.3, which has none of these features. The XMOS entities show as unavailable.

---

## Diagnostic entities and buttons

All hidden by default, under **Diagnostic** on the device page.

| Entity | What it does |
|--------|--------------|
| **XMOS Firmware** (text sensor, visible) | The running XMOS release and variant, e.g. `v2.0.0-beta.2 (aec4)`, or a status if the XMOS isn't reachable |
| **XMOS Hardware Reset** | Resets the XMOS. The settings are sent again once it's back |
| **XMOS Flash v2.0.0-beta.2** | Downloads and flashes the **Standard** image of this release. Mainly for testing; prefer the update entity |
| **XMOS Flash Embedded FW** | Flashes the Standard image embedded in the ESP firmware |
| **XMOS Erase Chip** | Erases the XMOS flash. The XMOS stops working until something is flashed again (the next boot does that automatically) |

> [!TIP]
> The debug buttons always install **Standard**, whatever **XMOS Firmware Variant** says. **XMOS Firmware Update** will then show an update if you selected AEC4. The per-variant settings follow whichever variant actually runs.

---

## Troubleshooting

| Problem | What to try |
|---------|-------------|
| **Install only flashes red LEDs** | The AEC4 download failed. Check the device has internet and can reach GitHub, then look at the **XMOS Firmware Update** release summary and the device log |
| **Beam points the wrong way / indicator jumps around** | Remove the top cover, or use East/West pair, North/South pair or Average |
| **Wake word missed from a distance** | Raise **Mic Gain** (e.g. 24–26 dB). Try **Wake Word Channel** = *Before noise suppression*. Check **Wake word sensitivity** |
| **Wake word missed while music plays** | Lower the volume to check it's echo. Try **AEC4 + Beam** with the cover off. Keep `Speaker channel output` on *Mono* |
| **False wake words from a TV** | **Beam Steering = Wake word only**, lower **Mic Gain**, or **Wake word sensitivity** = *Slightly sensitive* |
| **Settings look different after switching variant** | Expected: each variant has its own. Use **Reset XMOS Audio Settings** to get the recommended ones |
| **XMOS Firmware shows no version** | Press **XMOS Hardware Reset**. If it stays empty, reboot; the embedded image is reinstalled if the XMOS doesn't answer at boot |

When reporting an issue, include the **XMOS Firmware** value, your **Mic Mode** / **Beam Steering**, whether the top cover is on, and a device log.

---

## For people with their own YAML

The XMOS features live in two packages, both included from [config/satellite1.base.yaml](../config/satellite1.base.yaml):

- [config/common/xmos_audio.yaml](../config/common/xmos_audio.yaml) — the audio settings, DoA sensor, Direction Indicator and wake word steering
- [config/common/xmos_firmware.yaml](../config/common/xmos_firmware.yaml) — the variant select, update entity, reset button, per-variant settings and install scripts

The boot-time XMOS update is in `on_xmos_connected` in [config/satellite1.common.yaml](../config/satellite1.common.yaml). If you copied that block, take over the current version, or the XMOS falls back to Standard on every XMOS update.

### Substitutions

| Substitution | Default | Where | What it does |
|--------------|---------|-------|--------------|
| `xmos_fw_version` | `"v2.0.0-beta.2"` | `satellite1.base.yaml` | XMOS release this build installs |
| `xmos_fw_release_url` | `https://github.com/remcom/Satellite1-XMOS/releases/download/${xmos_fw_version}` | `satellite1.base.yaml` | Where images are downloaded from |
| `xmos_settings_standard_default` | `"0, 1, 0, 1, 1"` | `xmos_firmware.yaml` | Recommended Standard settings |
| `xmos_settings_aec4_default` | `"4, 1, 0, 1, 1"` | `xmos_firmware.yaml` | Recommended AEC4 settings |
| `doa_led_offset` | `"0"` | `xmos_audio.yaml` | LED index facing the North mic |
| `doa_led_clockwise` | `"true"` | `xmos_audio.yaml` | Whether azimuth increases clockwise over the LED indices |

The settings tuples are, in order: Mic Mode index, Beam Steering index, Wake Word Channel index, Fast Speech Hold (1 = on), High-Pass Filter (1 = on). Indices follow the option order of the selects.

Image file names include the release tag, so the version and the image can't point at different releases:

- Standard: `satellite1_xmos-${xmos_fw_version}.factory.bin` (+ `.md5`) — embedded at build time, so **the build needs internet**
- AEC4: `satellite1_xmos_aec4-${xmos_fw_version}.factory.bin` (+ `.md5`) — downloaded at install time

### Calibrating the Direction Indicator

Turn on **Direction Indicator**, remove the top cover, say the wake word from a known direction and watch which LED lights up. Then set `doa_led_offset` to the LED index that faces the North mic, and flip `doa_led_clockwise` if the LED moves the wrong way when you walk around the device.

```yaml
substitutions:
  doa_led_offset: "6"
  doa_led_clockwise: "false"
```

### Entities and IDs

| ID | Entity |
|----|--------|
| `xmos_fw_variant` | XMOS Firmware Variant (select, index 0 = Standard, 1 = AEC4) |
| `xmos_update` | XMOS Firmware Update (`update: platform: satellite1`) |
| `xmos_audio_settings_reset` | Reset XMOS Audio Settings |
| `xmos_mic_mode` | Mic Mode |
| `xmos_steer_policy` | Beam Steering |
| `xmos_ch1_tap` | Wake Word Channel |
| `xmos_ic_fast_hold` | Fast Speech Hold |
| `xmos_hpf` | Mic High-Pass Filter |
| `xmos_mic_gain` | Mic Gain |
| `doa_indicator` | Direction Indicator |
| `sat1_doa` | DoA sensor (`voice_direction`, `voice_direction_confidence`) |
| `xflash` | XMOS memory flasher |

### Scripts

| Script | What it does |
|--------|--------------|
| `xmos_install_selected_firmware` (`fallback: int`) | Installs the selected variant. `fallback: 1` installs the embedded image if a download fails after erasing; `fallback: 2` also if it fails before |
| `xmos_set_audio_settings` (`values: int[]`) | Applies a 5-value settings tuple; `255` leaves a setting alone |
| `xmos_sync_variant_settings` | Called from `on_xmos_connected`: saves/loads the per-variant settings when the running variant changed |
| `apply_xmos_mic_settings` | Sends all settings to the XMOS (after every (re)connect) |
| `report_wake_word` | Tells the XMOS a wake word was heard so it steers, then updates the indicator |
| `update_doa_led` | Points the indicator LED at the beam or talker |

### C++ API (`satellite1_id`)

Usable from lambdas:

```cpp
id(satellite1_id).set_mic_gain(uint8_t gain_db);               // 0..30
id(satellite1_id).set_mic_mode(satellite1::MIC_MODE_BEAM);     // PAIR_EW, PAIR_NS, AVERAGE, AUTO_PAIR, BEAM
id(satellite1_id).set_steer_policy(satellite1::STEER_POLICY_WAKE_WORD);  // FOLLOW_SPEECH, WAKE_WORD, FIXED
id(satellite1_id).set_ch1_tap(satellite1::CH1_TAP_NS);         // NS, IC, AEC
id(satellite1_id).set_ic_fast_hold(bool);
id(satellite1_id).set_hpf(bool);
id(satellite1_id).notify_wake_word(bool detected);
id(satellite1_id).read_beam_azimuth(uint16_t &azimuth);        // XMOS_NO_BEAM outside Beam mode
id(satellite1_id).get_xmos_fw_variant();                       // XMOS_FW_VARIANT_FIXED_DELAY or XMOS_FW_VARIANT_AEC4
id(sat1_doa).read_direction(); id(sat1_doa).get_azimuth(); id(sat1_doa).is_valid();
```

All setters return `false` when the XMOS isn't connected or the firmware doesn't support the command. The XMOS doesn't persist anything, so set values through the entities (which are restored and re-applied on every connect) rather than calling the setters directly.

### Compatibility

The audio settings need 2.0 XMOS firmware with the audio servicer (device-control resource 230). **Beam** needs v2.0.0-alpha.5 or newer. Beam Steering, the wake word channel, Fast Speech Hold and the high-pass filter need v2.0.0-alpha.6 or newer. Older firmware ignores them, and the DoA sensor logs a single warning. Firmware older than v2.0.0-beta.2 can't report its variant and is shown as `fixed_delay`.
