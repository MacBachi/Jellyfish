#!/bin/sh
# After the CI has published a release: sign its .bin files for over-the-air updates and
# attach the .sig files. Runs on the maintainer's Mac, where the private key lives.
#
#   tools/ota/release_sign.sh v1.2.0
set -eu
tag="${1:?usage: $0 <tag, e.g. v1.2.0>}"
here="$(cd "$(dirname "$0")" && pwd)"
dir="$(mktemp -d)"
gh release download "$tag" --pattern '*.bin' --dir "$dir"
for f in "$dir"/*.bin; do
  python3 "$here/ota_sign.py" sign "$f"
done
gh release upload "$tag" "$dir"/*.sig --clobber
echo "Signed and attached: $(ls "$dir"/*.sig | xargs -n1 basename | tr '\n' ' ')"
