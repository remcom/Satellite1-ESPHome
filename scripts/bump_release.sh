#!/usr/bin/env bash
# Update every hard-coded ESPHome / firmware version string in one go.
#
#   scripts/bump_release.sh --esphome 2026.10.0
#   scripts/bump_release.sh --esphome 2026.10.0 --firmware v1.4.1
#
# --esphome   new ESPHome pin (requirements.txt, min_version, README, site/index.html)
# --firmware  new firmware release tag (README badge + FAQ, site badges); only pass
#             this on the commit that precedes tagging the release
#
# Run it from anywhere inside the repo, then review `git diff`.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

ESPHOME_VERSION=""
FIRMWARE_VERSION=""
while [ $# -gt 0 ]; do
  case "$1" in
    --esphome) ESPHOME_VERSION="${2:-}"; shift 2 ;;
    --firmware) FIRMWARE_VERSION="${2:-}"; shift 2 ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
done

if [ -z "$ESPHOME_VERSION" ] && [ -z "$FIRMWARE_VERSION" ]; then
  echo "Usage: $0 [--esphome X.Y.Z[bN]] [--firmware vX.Y.Z]" >&2
  exit 2
fi

if [ -n "$ESPHOME_VERSION" ]; then
  [[ "$ESPHOME_VERSION" =~ ^20[0-9]{2}\.[0-9]+\.[0-9]+(b[0-9]+)?$ ]] || {
    echo "ESPHome version must look like 2026.10.0 or 2026.10.0b5" >&2; exit 2; }
  ESPHOME_MINOR="${ESPHOME_VERSION%.*}"  # 2026.10.0b5 -> 2026.10

  sed -i -E "s/^esphome==.*/esphome==${ESPHOME_VERSION}/" requirements.txt
  sed -i -E "s/^(  min_version: ).*/\1${ESPHOME_VERSION}/" config/satellite1.base.yaml
  sed -i -E \
    -e "s#(img\.shields\.io/badge/ESPHome-)[0-9.]+(-blue)#\1${ESPHOME_MINOR}\2#" \
    -e "s/(upstream ESPHome releases closely \(currently )[^)]*\)/\1${ESPHOME_VERSION})/" \
    -e "s/^(- \*\*ESPHome\*\* )[^ ]+( or newer)/\1${ESPHOME_VERSION}\2/" \
    README.md
  sed -i -E \
    -e "s/(<span class=\"badge\">ESPHome )[^<]*(<\/span>)/\1${ESPHOME_VERSION}\2/" \
    -e "s/(upstream ESPHome releases closely \(currently )[^)]*\)/\1${ESPHOME_VERSION})/" \
    -e "s/(<li>ESPHome )[^ ]+( or newer)/\1${ESPHOME_VERSION}\2/" \
    site/index.html
fi

if [ -n "$FIRMWARE_VERSION" ]; then
  [[ "$FIRMWARE_VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "Firmware version must look like v1.4.1 (stable tags only)" >&2; exit 2; }

  sed -i -E \
    -e "s#(img\.shields\.io/badge/Firmware-)v[0-9.]+(-brightgreen)#\1${FIRMWARE_VERSION}\2#" \
    -e "s/(tagged \(currently )v[0-9.]+\)/\1${FIRMWARE_VERSION})/" \
    README.md
  sed -i -E "s/(<span class=\"badge live\">)v[0-9.]+( · Stable)/\1${FIRMWARE_VERSION}\2/" site/*.html
fi

git --no-pager diff --stat
