#!/bin/sh
# Builds the app signed for development and installs it on the iPhone plugged into the Mac
# (unlocked, "Trust this computer" answered). Nothing goes through App Store Connect.
#   ios/scripts/install-device.sh            first connected iPhone
#   ios/scripts/install-device.sh <UDID>     a specific one
# iOS 17 and later go through Apple's devicectl; older phones through ios-deploy
# (brew install ios-deploy). Building for the phone itself registers it in the
# development profile the first time, which a generic build does not.
set -eu
cd "$(dirname "$0")/.."
udid="${1:-}"
if [ -z "$udid" ]; then
  udid=$(xcrun devicectl list devices 2>/dev/null | awk '/connected/ && /physical/ {for (i=1;i<=NF;i++) if ($i ~ /^[0-9A-F]{8}-[0-9A-F]{16}$|^[0-9a-f]{40}$/) {print $i; exit}}')
fi
[ -n "$udid" ] || { echo "No iPhone connected. Plug it in, unlock it, trust the Mac, try again." >&2; exit 1; }
xcodegen generate -q
xcodebuild -project JellyFloat.xcodeproj -scheme JellyFloat -configuration Debug \
  -destination "id=$udid" -derivedDataPath build-dev \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration build 2>&1 | grep -E "error|BUILD (SUCCEEDED|FAILED)"
app=build-dev/Build/Products/Debug-iphoneos/JellyFloat.app
if xcrun devicectl device install app --device "$udid" "$app" >/dev/null 2>&1; then
  xcrun devicectl device process launch --device "$udid" at.guggug.jellyfloat >/dev/null 2>&1 || true
else
  command -v ios-deploy >/dev/null || { echo "brew install ios-deploy first." >&2; exit 1; }
  ios-deploy --id "$udid" --bundle "$app" --justlaunch 2>&1 | grep -E "Installed|error" || true
fi
echo "Installed and launched on $udid. Version $(tr -d '[:space:]' < ../VERSION)."
