#!/bin/sh
# Archives the app for App Store Connect and uploads it. Needs Xcode with your Apple ID
# signed in (Xcode > Settings > Accounts) and DEVELOPMENT_TEAM in ios/Local.xcconfig.
#   ios/scripts/archive.sh            archive and upload
#   ios/scripts/archive.sh --no-upload  archive and export the .ipa to ios/build/export only
set -eu
cd "$(dirname "$0")/.."
team="$(sed -n 's/^DEVELOPMENT_TEAM *= *//p' Local.xcconfig | tr -d '[:space:]')"
[ -n "$team" ] || { echo "Put your Team ID into ios/Local.xcconfig first (see Local.xcconfig.example)." >&2; exit 1; }
destination=upload
[ "${1:-}" = "--no-upload" ] && destination=export
xcodegen generate -q
mkdir -p build
cat > build/ExportOptions.plist <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>method</key><string>app-store-connect</string>
  <key>destination</key><string>$destination</string>
  <key>teamID</key><string>$team</string>
  <key>uploadSymbols</key><true/>
</dict></plist>
PLIST
xcodebuild -project JellyFloat.xcodeproj -scheme JellyFloat -configuration Release \
  -destination 'generic/platform=iOS' -archivePath build/JellyFloat.xcarchive \
  -allowProvisioningUpdates archive
xcodebuild -exportArchive -archivePath build/JellyFloat.xcarchive \
  -exportOptionsPlist build/ExportOptions.plist -exportPath build/export -allowProvisioningUpdates
echo "Done: $destination. Version $(tr -d '[:space:]' < ../VERSION), build $(git rev-list --count HEAD)."
