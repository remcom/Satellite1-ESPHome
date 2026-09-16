# Security Policy

## Supported Versions

This is an experimental firmware fork for the FutureProofHomes Satellite1 hardware. Security fixes are applied to the latest release only.

| Version | Supported |
| ------- | --------- |
| Latest  | Yes       |
| Older   | No        |

## Reporting a Vulnerability

Please **do not** report security vulnerabilities through public GitHub issues.

Instead, report them privately via [GitHub Security Advisories](../../security/advisories/new). Provide as much detail as possible:

- Description of the vulnerability
- Steps to reproduce
- Potential impact (e.g. remote code execution, credential exposure, network access)
- Any suggested mitigations

You can expect an initial response within a few days. If the vulnerability is confirmed, a fix will be prioritized and a patched release made available. You will be credited in the advisory unless you prefer to remain anonymous.

## Scope

This repository contains:
- Custom ESPHome components for the Satellite1 hardware
- Firmware configuration YAML files
- Build and test scripts

Out of scope: vulnerabilities in upstream ESPHome, third-party components, or the home automation platforms this device integrates with (Home Assistant, etc.). Please report those to their respective projects.

## Hardened firmware variant

`config/satellite1.secure.yaml` builds the default firmware with two additions:

- **NVS encryption** (HMAC scheme). The AES-XTS keys are derived at runtime from an
  HMAC key in eFuse and never written to flash, so no flash encryption and no
  `nvs_keys` partition are needed — which is what keeps the variant deliverable
  over OTA. Wi-Fi credentials, the API key and saved preferences stop being
  readable from a flash dump.
- **Signed OTA verification** (`CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT`). The
  running app verifies any image offered to it against the public key in its own
  signature block. No eFuses are burned for this, and USB flashing is unrestricted.

Nothing is removed: provisioning, the beta channel, push OTA and the serial
console all behave as in the standard variant.

Operational notes:

- The eFuse key burn on first boot is **permanent and irreversible**, and moving
  to or from this variant wipes saved preferences once (the device needs
  re-provisioning).
- **Downgrading to a standard build is USB-only.** A hardened device installs only
  signed images and the standard releases are unsigned, so it refuses them over
  the air. `dashboard_import` is therefore omitted from this variant as well:
  ESPHome-dashboard adoption would hand the device the standard configuration and
  silently undo both protections.
- Neither change is hardware-enforced. Someone who can flash the device can still
  run code that asks the chip to decrypt NVS, so this raises the bar from "read
  the flash chip" to "flash custom firmware to that device" — it is not a defence
  against an attacker who can execute code on it. Closing that gap requires Secure
  Boot or a disabled download mode, both irreversible with no recovery path, and
  neither is enabled.
- Releases are signed in CI from the `SECURE_BOOT_SIGNING_KEY` secret. Only **one**
  trusted key is supported in this mode: if it is lost, field devices can only be
  updated over USB. Keep an offline backup. Key rotation requires switching to
  external signing with a `verification_keys:` trust list (up to three keys).
- A locally built hardened image needs your own signing key; an unsigned build has
  no public key to verify against and its OTA will not work.

## Security Considerations

- **Wi-Fi credentials** are provided at flash time and stored on-device. Use a dedicated IoT network segment. The hardened variant above stores them encrypted.
- **OTA updates** are accepted from the local network without a password in the default configuration. Run the device on a trusted network segment, or add a `password:` to the `ota:` block in your local config if your network is shared.
- **API access** to the device should be restricted to your local network. Enable the ESPHome API encryption key.
- This firmware is intended for home network environments and is **not hardened for untrusted network exposure**.
