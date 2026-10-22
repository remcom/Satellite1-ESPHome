# 🚀 v1.3.0

The first stable release on **ESPHome 2026.8**, rolling up everything from
[v1.3.0-beta1](https://github.com/remcom/Satellite1-ESPHome/releases/tag/v1.3.0-beta1)
and [v1.3.0-beta2](https://github.com/remcom/Satellite1-ESPHome/releases/tag/v1.3.0-beta2)
into a production build.

If you have been running the betas, the only thing new here is the move onto the final
ESPHome 2026.8.0 release plus the amplifier fixes below.

## What's Changed

### ⭐ ESPHome 2026.8.0

Pinned ESPHome `2026.7.2` → **`2026.8.0`** (`min_version` bumped to match). What that means on this device:

- **Lower-overhead microphone path** — the I²S microphone read path lost a redundant conversion on every read, so always-on wake word listening costs slightly less CPU
- **Better crash reports** — an unexpected reboot now logs the fault address and raw cause, which usually turns "it randomly rebooted" into an actionable bug report

### 🔊 TAS2780 amplifier fixes

Picked up the latest round of review fixes from the [upstream TAS2780 pull request](https://github.com/esphome/esphome/pull/15708):

- **Mute and volume no longer drift out of sync with the hardware** — if an I²C write to the amplifier failed, the driver used to keep the new value in software anyway, so Home Assistant showed a volume the amplifier never received. Failed writes now revert to the last known-good state
- **Fewer I²C transactions** — the driver caches which register page is selected instead of re-selecting it on every access
- Amplifier setup now runs at the correct priority relative to the rest of the audio pipeline

### 🔇 Mute switch now survives a reboot correctly

The mute state restored from memory could disagree with the physical switch on the side of the device — so after a reboot the satellite could be "unmuted" in Home Assistant while the hardware switch said muted (or the LED ring showed the wrong thing).

- The **physical switch always wins** at boot: its real position is read after everything is initialised and published as the mute state
- The LED ring reflects the true mute state immediately after startup

### 🔊 Switching between speaker and line-out no longer goes silent

Switching outputs while the target DAC was powered down (after idle, or after unplugging the jack) could leave you with **no sound at all** until you switched back and forth.

- Both `switch_to_speaker` and `switch_to_lineout` now wake the target DAC before routing audio to it, and power down the one you left
- Idle power-down was unified onto the same code path, so the 60-second idle shutdown and a manual switch can no longer fight each other

### ⏱️ Timer fixes

- **The timer tick animation actually runs now** — an effect name mismatch meant the ticking LED effect was silently never applied
- **The correct timer is shown**: with multiple timers, the ring always tracks the shortest **running** timer. Before, a paused or finished timer could hijack the display
- Fixed the ring wrapping to a full bright circle at the exact moment a timer hit zero

### 💡 LED ring polish

- Fixed the fade math behind the thinking / error / muted / timer effects — rounding errors made brightness steps uneven and washed out. Fades are now smooth
- Removed unused effects (`Rainbow`, `Tick`, `Fast Pulse`) and de-duplicated the two spin animations, shrinking the config and freeing flash
- Migrated the LED strip off the deprecated `rgb_order` option onto `channel_colors`

### 🔈 Volume range default corrected

The developer volume-range control defaulted to a value that **silently overrode** the configured amplifier range on every boot. It now matches the amplifier configuration (`0.65`), so speaker volume behaves as designed out of the box.

### 📈 Diagnostics & logging

- **Media title** is exposed as its own sensor in Home Assistant alongside artist and album
- **Uptime** uses ESPHome's built-in uptime text sensor — no more `Starting...` placeholder, and a properly formatted value
- **XMOS Firmware** sensor is published by the connection triggers instead of polling, so it reflects the real connection state
- Default log level lowered from `debug` to `info` for the audio pipeline — less log spam and less overhead in normal use. The debug lines are still there, commented, for when you are tracing audio issues
- HTTP request timeouts and buffers moved into the base config, so **dashboard/remote builds get the same settings** as local builds

### 📦 Release builds are no longer racy

The three device variants build in parallel, and all three used to write to the same GitHub release at the same time — which could delete or overwrite each other's assets and leave a release with missing firmware.

- Builds now only **stage** their artifacts; a single publish job does every write to the release
- Firmware version is injected through ESPHome **substitutions** instead of patching the YAML with `sed`
- Added concurrency guards, job timeouts and caching, so a release build cannot be cancelled halfway through

### 🧹 CI maintenance

- All GitHub Actions updated to current versions, and two unmaintained label actions replaced
- Lint workflow consolidated onto pre-commit
- Fixed the clang-tidy runner on fresh ESP-IDF builds

### 🌐 Web installer site

- Clearer first-time install instructions — Wi-Fi credentials are entered right in the browser after flashing, and devices already on the network can be re-provisioned over Bluetooth from Home Assistant
- Light-mode styling fixes and proper `theme-color` support

---

## 🔗 Full Changelog

**Compare versions:** [v1.2.1...v1.3.0](https://github.com/remcom/Satellite1-ESPHome/compare/v1.2.1...v1.3.0)

---

## 💬 Feedback

Found a bug or have a feature request? [Open an issue](https://github.com/remcom/Satellite1-ESPHome/issues)!
