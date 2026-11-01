#!/usr/bin/env bash
set -euo pipefail

stage="$1"
source_dir="$2"
version="$3"
out="$4"
platform="$5"

stage="$(cd "$stage" && pwd)"
source_dir="$(cd "$source_dir" && pwd)"
mkdir -p "$out"
out="$(cd "$out" && pwd)"

if [ ! -x "$stage/bin/orchard" ]; then
    echo "$stage/bin/orchard is missing; make the stage with stage.sh first" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
app="$work/root/Orchard.app"
contents="$app/Contents"
mkdir -p "$contents/MacOS" "$contents/Resources"

cp -R "$stage/bin/." "$contents/MacOS/"
for part in lib include runtime fizmo toolchain docs; do
    if [ -d "$stage/$part" ]; then cp -R "$stage/$part" "$contents/"; fi
done
cp "$source_dir/Packaging/macos/orchard.icns" "$contents/Resources/orchard.icns"

sed -e "s/@ORCHARD_VERSION@/$version/g" "$source_dir/Packaging/macos/Info.plist.in" > "$contents/Info.plist"
printf 'APPL????' > "$contents/PkgInfo"
plutil -lint "$contents/Info.plist"

if ! codesign --force --sign - "$app" 2> "$work/codesign.txt"; then
    echo "::warning::couldn't ad-hoc sign Orchard.app: $(cat "$work/codesign.txt")"
fi

ln -s /Applications "$work/root/Applications"
dmg="$out/Orchard-$version-$platform.dmg"
rm -f "$dmg"
hdiutil create -volname "Orchard $version" -srcfolder "$work/root" -fs HFS+ -format UDZO -ov "$dmg"
hdiutil verify "$dmg"

rm -f "$work/root/Applications"
(cd "$work/root" && ditto -c -k --sequesterRsrc --keepParent Orchard.app "$out/Orchard-$version-$platform-app.zip")

ls -la "$dmg" "$out/Orchard-$version-$platform-app.zip"
