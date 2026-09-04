#!/bin/sh
# Builds the app signed for development and installs it on the iPhone plugged into the Mac
# (unlocked, "Trust this computer" answered). Nothing goes through App Store Connect.
#   ios/scripts/install-device.sh            first connected iPhone
#   ios/scripts/install-device.sh <UDID>     a specific one (xcrun devicectl list devices)
set -eu
cd "$(dirname "$0")/.."
xcodegen generate -q
xcodebuild -project JellyFloat.xcodeproj -scheme JellyFloat -configuration Debug \
  -destination 'generic/platform=iOS' -derivedDataPath build-dev \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration build 2>&1 | grep -E "error|BUILD (SUCCEEDED|FAILED)"
app=build-dev/Build/Products/Debug-iphoneos/JellyFloat.app
dev="${1:-$(xcrun devicectl list devices 2>/dev/null | awk '/iPhone/ && /available|connected/ {print $(NF-2); exit}')}"
[ -n "$dev" ] || { echo "No iPhone connected. Plug it in, unlock it, trust the Mac, try again." >&2; exit 1; }
xcrun devicectl device install app --device "$dev" "$app"
xcrun devicectl device process launch --device "$dev" at.guggug.jellyfloat
echo "Installed and launched on $dev. Version $(tr -d '[:space:]' < ../VERSION)."
