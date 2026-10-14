#!/bin/sh
# One time per jelly, over USB: install the A/B partition table and put the firmware into
# partition A. From then on the jelly takes updates over the air (app or web page).
# Keeps the Find My key and the remembered settings (they sit above the partitions).
#
#   tools/ota/ota_prepare.sh JellyFloatOS-v1.2.0-ring94-GRB-tRGB.uf2
#
# The jelly must be plugged into the Mac with a data cable; running or in BOOTSEL both work.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
img="${1:?usage: $0 <JellyFloatOS .uf2 for this jelly's variant>}"
pt="$(mktemp -d)/pt.uf2"
picotool=$(command -v picotool || echo /opt/homebrew/bin/picotool)

"$picotool" partition create "$here/../../firmware_cpp/ota/pt.json" "$pt"
# Into BOOTSEL (a running JellyFloatOS answers the USB reset request), then the table.
"$picotool" reboot -u -f >/dev/null 2>&1 || true
sleep 3
"$picotool" load -v "$pt"
"$picotool" reboot -u >/dev/null 2>&1 || true
sleep 3
"$picotool" partition info
"$picotool" load -v -p 0 "$img"
"$picotool" reboot
echo "Done: partition table installed, firmware in partition A. The console should say"
echo "\"ota: running from partition 0\"."
