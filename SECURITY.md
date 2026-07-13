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

## Security Considerations

- **Wi-Fi credentials** are provided at flash time and stored on-device. Use a dedicated IoT network segment.
- **OTA updates** are accepted from the local network without a password in the default configuration. Run the device on a trusted network segment, or add a `password:` to the `ota:` block in your local config if your network is shared.
- **API access** to the device should be restricted to your local network. Enable the ESPHome API encryption key.
- This firmware is intended for home network environments and is **not hardened for untrusted network exposure**.
