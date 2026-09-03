#!/bin/sh
# Writes Generated/Info.plist from Info.template.plist, filling in the version from the
# repository's VERSION file and the git revision, so app and firmware carry one and the
# same version. Xcode runs this before compiling (see project.yml); the build system
# processes the generated plist after it because it is declared as this script's output.
set -eu
here="${SRCROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
root="$(cd "$here/.." && pwd)"
version="$(tr -d '[:space:]' < "$root/VERSION")"
rev="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
build="$(git -C "$root" rev-list --count HEAD 2>/dev/null || echo 1)"
mkdir -p "$here/Generated"
sed -e "s/__VERSION__/$version/" -e "s/__BUILD__/$build/" -e "s/__GITREV__/$rev/" \
    "$here/Info.template.plist" > "$here/Generated/Info.plist.tmp"
mv "$here/Generated/Info.plist.tmp" "$here/Generated/Info.plist"
echo "JellyFloat $version ($rev), build $build"
