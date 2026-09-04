#!/bin/sh
# Builds the app signed for development and installs it on the iPhone plugged into the Mac
# (unlocked, "Trust this computer" answered). Nothing goes through App Store Connect.
#   ios/scripts/install-device.sh            first connected iPhone
#   ios/scripts/install-device.sh <UDID>     a specific one (ios-deploy -c lists them)
set -eu
cd "$(dirname "$0")/.."
xcodegen generate -q
xcodebuild -project JellyFloat.xcodeproj -scheme JellyFloat -configuration Debug \
  -destination 'generic/platform=iOS' -derivedDataPath build-dev \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration build 2>&1 | grep -E "error|BUILD (SUCCEEDED|FAILED)"
app=build-dev/Build/Products/Debug-iphoneos/JellyFloat.app
# ios-deploy (brew install ios-deploy) talks to every iOS version Xcode still supports;
# Apple's devicectl only from iOS 17 on.
command -v ios-deploy >/dev/null || { echo "brew install ios-deploy first." >&2; exit 1; }
ios-deploy -c --timeout 10 >/dev/null 2>&1 || { echo "No iPhone connected. Plug it in, unlock it, trust the Mac, try again." >&2; exit 1; }
ios-deploy --bundle "$app" --justlaunch ${1:+--id "$1"} 2>&1 | grep -E "Installed|Found|error" || true
echo "Installed and launched. Version $(tr -d '[:space:]' < ../VERSION)."
