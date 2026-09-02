#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if grep -q 'DualDisplay.h' FlockDetection_Cardputer_ADV.ino; then
  echo "Already patched."
  exit 0
fi
patch -p1 < docs/dual-screen-ino.patch
echo "Patched. Set PLUME_DUAL_SCREEN to 1 in DualDisplay.h when the ILI9341 is wired."
