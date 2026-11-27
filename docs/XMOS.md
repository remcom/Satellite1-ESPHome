# XMOS Audio Front End — Guide

The Satellite1 HAT has an **XMOS XU316** chip that processes the four microphones before the ESP32 sees any audio. It cleans up your voice for the wake word and Home Assistant, works out where you're talking from, and points a beam at you.

This guide covers:

- [What the XMOS does](#what-the-xmos-does)
- [The two XMOS firmware variants: Standard and Beamforming (AEC4)](#the-two-xmos-firmware-variants)
- [Microphone Setup: choosing the variant](#microphone-setup)
- [Audio settings reference](#audio-settings-reference)
- [Direction of arrival and the Direction Indicator](#direction-of-arrival-and-the-direction-indicator)
- [Recommended setups](#recommended-setups)
- [Firmware updates, failures and recovery](#firmware-updates-failures-and-recovery)
- [Diagnostic entities and buttons](#diagnostic-entities-and-buttons)
- [Troubleshooting](#troubleshooting)
- [For people with their own YAML](#for-people-with-their-own-yaml)

> [!CAUTION]
> The XMOS firmware is **v2.0.2** from [remcom/Satellite1-XMOS](https://github.com/remcom/Satellite1-XMOS/releases). It comes from a fork and is **not an official FutureProofHomes release**. Direction of arrival was tuned mostly in simulation.

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

Every XMOS release comes in two variants. They share the same version number (e.g. `v2.0.2`), so the variant is shown separately, e.g. `v2.0.2 (fixed_delay)` or `v2.0.2 (aec4)`.

| | **Standard (fixed delay)** | **Beamforming (AEC4)** |
|---|---|---|
| **Microphone Setup** | Cover on (Standard) | Cover off (Beamforming, AEC4) |
| **Internal name** | `fixed_delay` | `aec4` |
| **Echo cancellation runs on** | The selected mic pair (2 channels) | **All four mics, before the beamformer** |
| **When the beam turns or the mic pair changes** | The echo canceller has to re-adapt, so echo leaks through for a moment | Nothing changes for the echo canceller, it already covers every mic |
| **Best Mic Mode** | East/West pair (works with the cover on) | **Beam** (needs the top cover removed for now) |
| **Where the image comes from** | **Embedded in the ESP firmware.** Can always be (re)installed, also without internet | **Downloaded** from the [XMOS release](https://github.com/remcom/Satellite1-XMOS/releases) when installed. Needs internet |
| **Echo reference** | **Mono**: what the speaker plays (see below) | **Mono**: what the speaker plays (see below) |
| **Voice direction while music plays** | Paused | **Keeps working**: measured on the echo cancelled mics |
| **Status** | Default. Same audio pipeline as earlier 2.0 betas | Tested on hardware. Recommended with the top cover removed |

### Which one should I pick?

- **Cover on (Standard)** if the top cover is on, or you have no reliable internet.
- **Cover off (Beamforming, AEC4)** if you removed the top cover. It gives the best wake word detection, also **while music is playing**: the beam can follow you without the echo canceller starting over each time.

### Echo reference and stereo

Both variants cancel against **one** (mono) reference: exactly what the single speaker plays. The ESP32 tells the XMOS what that is from `Speaker channel output`: *Mono (Left + Right)* (default), *Left Channel Only* or *Right Channel Only*, so stereo music is cancelled in every setting.

Standard firmware before this change modelled the left and right channels separately. With stereo music on the mono speaker that never settles on one solution, and in the XMOS simulator it removed ~10 dB less echo (see [voice-simulation.md](https://github.com/remcom/Satellite1-XMOS/blob/develop/docs/voice-simulation.md)).

- With the **3.5 mm line-out** and stereo speakers, the echo canceller only partly removes stereo music.
- Spoken replies, announcements and timer sounds are mono and are always fully cancelled.

---

## Microphone Setup

**Microphone Setup** (under **Configuration** on the device page) is the one setting to choose. Each option picks the firmware variant and its recommended audio settings:

| Option | Firmware | Mic Mode | Beam Steering | Wake Word Channel | Fast Speech Hold, High-Pass Filter, Wake Word Level Control |
|---|---|---|---|---|---|
| **Cover on (Standard)** *(default)* | Standard | East/West pair | Wake word only | Noise suppressed | On |
| **Cover off (Beamforming, AEC4)** | AEC4 | Beam | Wake word only | Noise suppressed | On |
| **Custom** | Stays as installed | Stay as they are | | | |

> [!WARNING]
> **Switching between Cover on and Cover off reflashes the XMOS chip right away.** It takes about a minute. The LED ring shows the progress and voice and audio don't work until it's done. **Don't unplug the device while it runs.**

- Picking a preset loads its audio settings. If the preset needs the other firmware variant, the XMOS is reflashed straight away; **XMOS Firmware Update** shows the progress, and the reason if it fails. Press **Install** there to retry.
- Cover off downloads the AEC4 image, so it needs internet. Going back to Cover on never does, that image is inside the ESP firmware.
- **Changing one of the preset's audio settings by hand** (they're hidden by default, see below) switches Microphone Setup to **Custom**. Custom keeps the installed firmware and your settings. Pick a preset again to go back to its recommended settings.
- **Mic Gain** and **Direction Indicator** aren't part of the presets: they stay as you set them.
- **Updating from an older release** keeps what you had: Microphone Setup shows the preset that matches your installed variant and settings, or Custom if you changed any of them.

---

## Audio settings reference

All settings are on the device page in Home Assistant under **Configuration**. Most are hidden by default, because **Microphone Setup** sets them; enable them under the device's *entities not shown* if you want to tune them. Hidden entities still work on the device. Changing a setting that the preset sets makes Microphone Setup **Custom**.

| Setting | Type | Cover on | Cover off | Set by the preset | Hidden by default | Needs top cover removed |
|---------|------|----------|-----------|:-----------------:|:-----------------:|:-----------------------:|
| [Mic Mode](#mic-mode) | select | East/West pair | **Beam** | ✅ | ✅ | Depends on the mode |
| [Beam Steering](#beam-steering) | select | Wake word only | Wake word only | ✅ | ✅ | Yes (except *Fixed*) |
| [Wake Word Channel](#wake-word-channel) | select | Noise suppressed | Noise suppressed | ✅ | ✅ | No |
| [Fast Speech Hold](#fast-speech-hold) | switch | On | On | ✅ | ✅ | No |
| [Mic High-Pass Filter](#mic-high-pass-filter) | switch | On | On | ✅ | ✅ | No |
| [Wake Word Level Control](#wake-word-level-control) | switch | On | On | ✅ | ✅ | No |
| [Mic Gain](#mic-gain) | number | 20 dB | 20 dB | | | No |
| [Direction Indicator](#direction-of-arrival-and-the-direction-indicator) | switch | Off | Off | | | Yes |

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
| **Beam** | Uses all four mics: one beam points at you, a second beam hears the rest of the room and serves as the noise reference for the interference canceller | The most noise rejection, from any direction. Used by **Cover off (AEC4)**. On Standard, the echo canceller re-adapts each time the beam turns | **Yes** |

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

### Wake Word Level Control

Keeps the wake word at about the same level for the wake word engine whether you say it softly or loudly. Without it the wake word channel has a fixed gain: a wake word ~5 dB louder than usual (speaking up, or half the distance) clipped, and a soft one reached the engine very quietly. On (default), the XMOS raises the level of soft speech by up to 18 dB and lowers loud speech by up to 20 dB, and softly limits peaks so they never clip.

It adapts while you talk and holds its level in silence, so it doesn't boost background noise. It can't make soft speech in a noisy room any cleaner, and right after you spoke loudly, the first soft wake word gets little of the extra gain. Needs XMOS firmware v2.0.2 or newer; older firmware ignores it. Turn it off to compare.

### Mic Gain

Microphone level after echo cancellation, **0–30 dB**, default **20 dB**. Not part of the presets. Raise it if the wake word only works up close; lower it if you get false triggers in a loud room.

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

| Situation | Microphone Setup | Direction Indicator |
|-----------|------------------|---------------------|
| **Cover on** | **Cover on (Standard)**. If the east or west mic faces a wall: Custom with Mic Mode *North/South pair* | Off |
| **Cover off** | **Cover off (Beamforming, AEC4)** | Optional |
| **TV in the room** | Either preset: both use Beam Steering *Wake word only* | — |
| **Several people talking in turn, cover off** | Cover off, then Custom with Beam Steering *Follow speech* | Optional |

---

## Firmware updates, failures and recovery

### Automatic updates

Each ESP firmware release is built for one XMOS release (currently `v2.0.2`). On the first boot after an ESP update that brings a new XMOS release, the device updates the XMOS by itself, **in the variant of your Microphone Setup**:

- **Cover on (Standard)**: installed from the image embedded in the ESP firmware.
- **Cover off (AEC4)**: the device waits up to 60 s for WiFi and downloads the image. If WiFi doesn't come up or the download fails, it installs **Standard** instead so the device keeps working. **XMOS Firmware Update** then shows an update again, with the reason in its release notes. Press **Install** to retry.

With **Custom** it keeps the variant that is installed.

If the XMOS doesn't respond at all at boot (e.g. erased), the embedded Standard image is installed automatically.

### When an install fails

- **XMOS Firmware Update** shows why in its release summary, e.g. *the firmware image couldn't be downloaded*. The message stays until the next install, or until the XMOS runs the firmware of the selected Microphone Setup.
- **If the AEC4 download fails before anything was erased**, the XMOS keeps the firmware it had.
- **If the download breaks off after the XMOS flash was erased**, the embedded Standard image is installed automatically. The XMOS is never left without firmware.
- The LED ring progress is blue while flashing, turns green on success and red when flashing fails.

### Things to know

- **Your Microphone Setup survives reboots and ESP updates.**
- **Don't unplug the device while the XMOS is flashed.** If it happens anyway, the device reinstalls the embedded image on the next boot.
- **The factory reset (hold the action button 22 s) also erases the XMOS**, which then gets the embedded Standard image on the next boot. A factory reset is skipped while the XMOS is being flashed.

### Going back to older ESP firmware

- **v2.0.2 or older** has **XMOS Firmware Variant** and **Reset XMOS Audio Settings** instead of Microphone Setup. The variant select comes back with the value it had before you updated: if that's not the variant that runs, **XMOS Firmware Update** offers to switch, and pressing Install there does. Your audio settings move with the running variant.
- **v2.0.0-beta6 or older** flashes the XMOS back to its own release (Standard) on the first boot, also if you had AEC4. The variant entities show as unavailable and can be removed. If you were on AEC4, check **Mic Mode** afterwards.
- **v2.0.0-beta1** doesn't know **Beam**: set Mic Mode to another option first.
- **v1.4.x** flashes XMOS firmware v1.0.3, which has none of these features. The XMOS entities show as unavailable.

---

## Diagnostic entities and buttons

All hidden by default, under **Diagnostic** on the device page.

| Entity | What it does |
|--------|--------------|
| **XMOS Firmware** (text sensor, visible) | The running XMOS release and variant, e.g. `v2.0.2 (aec4)`, or a status if the XMOS isn't reachable |
| **XMOS Hardware Reset** | Resets the XMOS. The settings are sent again once it's back |
| **XMOS Flash v2.0.2** | Downloads and flashes the **Standard** image of this release. Mainly for testing; prefer the update entity |
| **XMOS Flash Embedded FW** | Flashes the Standard image embedded in the ESP firmware |
| **XMOS Erase Chip** | Erases the XMOS flash. The XMOS stops working until something is flashed again (the next boot does that automatically) |
| **XMOS Echo Reduction** | How much of the speaker's sound the echo canceller removes (ERLE), in dB, averaged over its mics. Meaningful **while something plays**; when you talk or the room is quiet it drops, that's expected. Higher is better: roughly 20 dB and up is good |
| **XMOS Echo Reduction Worst Mic** | The same for the mic that removes the least echo |
| **XMOS Echo Reference Drops** | Speaker audio frames the echo canceller missed because it fell behind, since the XMOS started. Should stay 0; if it rises the XMOS is overloaded |
| **XMOS Clipped Samples Voice Channel** | Output samples at full scale on channel 0 (to Home Assistant) since the XMOS started |
| **XMOS Clipped Samples Wake Word Channel** | The same on channel 1 (wake word). If it rises while you talk, lower **Mic Gain** |

The XMOS sensors update every 10 s and need XMOS firmware with audio quality telemetry (v2.0.1 or newer); older firmware logs one warning and leaves them unknown. The counters restart when the XMOS restarts.

### Measuring the wake word timing

The XMOS finds the wake word's direction from the DOA results of a short window before the ESP32 reports it. It leaves out the last ~300 ms, which is about how long the wake word takes to get there (I2S, the wake word engine, the SPI write). If the real delay is different, the window catches speech after the wake word (a TV) or misses part of it. To measure it:

1. Use XMOS firmware that reports the timing (v2.0.2 or newer); older firmware notes this once at DEBUG level and logs nothing.
2. Pick a quiet room (no TV, fan or music) and open the device log.
3. Say the wake word ~20 times from 1–3 m, at different speeds, with a few seconds of silence after each.
4. After each wake word the log shows a line like:

   ```
   [I][satellite1]: Wake timing #12: last located speech 285 ms (19 frames) before the report; the window skips 300 ms; 41 DOA results in the window, located
   ```

The first number is the delay: the time from the end of the wake word to the report. Note the median and the highest of your ~20 values; reverb makes it read a few frames short, and values near 0 mean the engine fired before the word ended (or a sound was heard after it). "The window skips" should be about the highest usual value. If it's well off, report the numbers in an issue.

> [!TIP]
> The debug buttons always install **Standard**, whatever **Microphone Setup** says. **XMOS Firmware Update** will then show an update if Cover off is selected.

---

## Troubleshooting

| Problem | What to try |
|---------|-------------|
| **Install only flashes red LEDs** | The AEC4 download failed. Check the device has internet and can reach GitHub, then look at the **XMOS Firmware Update** release summary and the device log |
| **Beam points the wrong way / indicator jumps around** | Remove the top cover, or use East/West pair, North/South pair or Average |
| **Wake word missed from a distance** | Raise **Mic Gain** (e.g. 24–26 dB). Try **Wake Word Channel** = *Before noise suppression*. Check **Wake word sensitivity** |
| **Wake word missed while music plays** | Lower the volume to check it's echo. Try **Microphone Setup = Cover off** with the cover removed. Keep `Speaker channel output` on *Mono* |
| **False wake words from a TV** | **Beam Steering = Wake word only**, lower **Mic Gain**, or **Wake word sensitivity** = *Slightly sensitive* |
| **Microphone Setup shows Custom** | One of the preset's audio settings was changed. Pick a preset again to load its recommended settings |
| **XMOS Firmware shows no version** | Press **XMOS Hardware Reset**. If it stays empty, reboot; the embedded image is reinstalled if the XMOS doesn't answer at boot |

When reporting an issue, include the **XMOS Firmware** value, your **Microphone Setup** (and **Mic Mode** / **Beam Steering** if Custom), whether the top cover is on, and a device log.

---

## For people with their own YAML

The XMOS features live in two packages, both included from [config/satellite1.base.yaml](../config/satellite1.base.yaml):

- [config/common/xmos_audio.yaml](../config/common/xmos_audio.yaml) — the audio settings, DoA sensor, Direction Indicator and wake word steering
- [config/common/xmos_firmware.yaml](../config/common/xmos_firmware.yaml) — Microphone Setup (presets), the update entity and the install scripts

The boot-time XMOS update is in `on_xmos_connected` in [config/satellite1.common.yaml](../config/satellite1.common.yaml). If you copied that block, take over the current version, or the XMOS falls back to Standard on every XMOS update.

### Substitutions

| Substitution | Default | Where | What it does |
|--------------|---------|-------|--------------|
| `xmos_fw_version` | `"v2.0.2"` | `satellite1.base.yaml` | XMOS release this build installs |
| `xmos_fw_release_url` | `https://github.com/remcom/Satellite1-XMOS/releases/download/${xmos_fw_version}` | `satellite1.base.yaml` | Where images are downloaded from |
| `xmos_preset_cover_on` | `"0, 1, 0, 1, 1, 1"` | `xmos_firmware.yaml` | Audio settings of Cover on (Standard) |
| `xmos_preset_cover_off` | `"4, 1, 0, 1, 1, 1"` | `xmos_firmware.yaml` | Audio settings of Cover off (AEC4) |
| `doa_led_offset` | `"0"` | `xmos_audio.yaml` | LED index facing the North mic |
| `doa_led_clockwise` | `"true"` | `xmos_audio.yaml` | Whether azimuth increases clockwise over the LED indices |

The preset tuples are, in order: Mic Mode index, Beam Steering index, Wake Word Channel index, Fast Speech Hold (1 = on), High-Pass Filter (1 = on), Wake Word Level Control (1 = on). Indices follow the option order of the selects.

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
| `xmos_mic_setup` | Microphone Setup (select, index 0 = Cover on, 1 = Cover off, 2 = Custom) |
| `xmos_update` | XMOS Firmware Update (`update: platform: satellite1`) |
| `xmos_mic_mode` | Mic Mode |
| `xmos_steer_policy` | Beam Steering |
| `xmos_ch1_tap` | Wake Word Channel |
| `xmos_ic_fast_hold` | Fast Speech Hold |
| `xmos_hpf` | Mic High-Pass Filter |
| `xmos_ch1_agc` | Wake Word Level Control |
| `xmos_mic_gain` | Mic Gain |
| `doa_indicator` | Direction Indicator |
| `sat1_doa` | DoA sensor (`voice_direction`, `voice_direction_confidence`) |
| `sat1_audio_quality` | XMOS audio quality sensors (`sensor: platform: satellite1`, `type: audio_quality`) |
| `xflash` | XMOS memory flasher |

### Scripts

| Script | What it does |
|--------|--------------|
| `xmos_install_selected_firmware` (`fallback: int`) | Installs the variant the update entity targets. `fallback: 1` installs the embedded image if a download fails after erasing; `fallback: 2` also if it fails before |
| `xmos_apply_mic_setup` | Loads the selected preset's audio settings and reflashes the XMOS if it needs another variant |
| `xmos_update_target_variant` | Points the update entity at the selected preset's variant (Custom: the running one) |
| `xmos_mic_setup_check` | Switches Microphone Setup to Custom when a preset setting changed; on the first XMOS connect, matches it to what the device ran before |
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
id(satellite1_id).set_ch1_agc(bool);                              // Wake Word Level Control
id(satellite1_id).notify_wake_word(bool detected);
id(satellite1_id).set_ref_mix(satellite1::REF_MIX_DOWNMIX);   // DOWNMIX, LEFT, RIGHT: match the TAS2780
id(satellite1_id).read_beam_azimuth(uint16_t &azimuth);        // XMOS_NO_BEAM outside Beam mode
id(satellite1_id).log_wake_timing();                            // logs how long the last wake word took to reach the XMOS
id(satellite1_id).get_xmos_fw_variant();                       // XMOS_FW_VARIANT_FIXED_DELAY or XMOS_FW_VARIANT_AEC4
id(sat1_doa).read_direction(); id(sat1_doa).get_azimuth(); id(sat1_doa).is_valid();
```

All setters return `false` when the XMOS isn't connected or the firmware doesn't support the command. The XMOS doesn't persist anything, so set values through the entities (which are restored and re-applied on every connect) rather than calling the setters directly.

### Compatibility

The audio settings need 2.0 XMOS firmware with the audio servicer (device-control resource 230). **Beam** needs v2.0.0-alpha.5 or newer. Beam Steering, the wake word channel, Fast Speech Hold and the high-pass filter need v2.0.0-alpha.6 or newer. The echo reference mix (`set_ref_mix`) needs v2.0.1 or newer. The wake word timing (`log_wake_timing`, `read_wake_timing`) and Wake Word Level Control (`set_ch1_agc`) need v2.0.2 or newer. Older firmware ignores them, and the DoA sensor logs a single warning. Firmware older than v2.0.0-beta.2 can't report its variant and is shown as `fixed_delay`.
