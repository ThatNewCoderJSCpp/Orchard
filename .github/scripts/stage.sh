#!/usr/bin/env bash
set -euo pipefail

walnut_package="$1"
orchard_exe="$2"
source_dir="$3"
stage="$4"
extra_libs="${5:-}"

walnut_package="$(cd "$(dirname "$walnut_package")" && pwd)/$(basename "$walnut_package")"
orchard_exe="$(cd "$(dirname "$orchard_exe")" && pwd)/$(basename "$orchard_exe")"
source_dir="$(cd "$source_dir" && pwd)"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

case "$walnut_package" in
    *.zip) unzip -q "$walnut_package" -d "$work" ;;
    *) tar -xzf "$walnut_package" -C "$work" ;;
esac
root="$(find "$work" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
if [ -z "$root" ] || [ ! -d "$root/bin" ]; then
    echo "$walnut_package doesn't hold a Walnut package folder" >&2
    exit 1
fi

rm -rf "$stage"
mkdir -p "$stage"
stage="$(cd "$stage" && pwd)"
for part in bin runtime fizmo toolchain; do
    if [ -d "$root/$part" ]; then cp -R "$root/$part" "$stage/"; fi
done
if [ -d "$root/docs" ]; then
    mkdir -p "$stage/docs"
    cp -R "$root/docs" "$stage/docs/walnut"
fi

cp "$orchard_exe" "$stage/bin/"
chmod +x "$stage/bin/$(basename "$orchard_exe")"
mkdir -p "$stage/include/orchard" "$stage/docs"
cp -R "$source_dir/Include/orchard/script" "$source_dir/Include/orchard/walnut" "$stage/include/orchard/"
cp "$source_dir/README.md" "$stage/docs/README.md"

case "$orchard_exe" in
    *.exe) ;;
    *)
        mkdir -p "$stage/share/orchard"
        cp "$source_dir/Packaging/linux/orchard.desktop" "$source_dir/Packaging/linux/orchard-mime.xml" "$source_dir/Packaging/icons/orchard.svg" "$source_dir/Packaging/icons/application-x-orchard-project.svg" "$stage/share/orchard/"
        ;;
esac

if [ -n "$extra_libs" ] && [ -d "$extra_libs" ]; then
    mkdir -p "$stage/lib"
    cp -R "$extra_libs/." "$stage/lib/"
fi

echo "$stage"
