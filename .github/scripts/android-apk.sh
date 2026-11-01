#!/usr/bin/env bash
set -euo pipefail

abis="${1:-arm64-v8a,x86_64}"
out_dir="${2:-.}"
ndk="${ANDROID_NDK_LATEST_HOME:-${ANDROID_NDK_HOME:-}}"
if [ -z "$ndk" ] || [ ! -f "$ndk/build/cmake/android.toolchain.cmake" ]; then
    echo "::error::set ANDROID_NDK_HOME to the Android NDK"
    exit 1
fi
export ANDROID_NDK_HOME="$ndk"

root=$(pwd)
libs="$root/build-android-libs"
kits="$root/build-android-kits"
jobs="${JOBS:-4}"

cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Release -DFIZMO_TUNE=OFF \
    -DORCHARD_BUNDLE=OFF -DORCHARD_BUILD_TESTS=OFF -DORCHARD_WITH_JAVA=OFF
cmake --build build-host --target grove walnut -j "$jobs"
grove="$root/build-host/bin/grove"
test -x "$grove"

rm -rf "$libs" "$kits"
for abi in ${abis//,/ }; do
    cmake -S . -B "build-android-$abi" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static \
        -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
        -DFIZMO_TUNE=OFF -DFIZMO_BUILD_SHARED=OFF -DFIZMO_BUILD_SHADER_TOOL=OFF \
        -DORCHARD_ANDROID_LIBS_DIR="$libs"
    cmake --build "build-android-$abi" --target orchard_android grove_android_kit -j "$jobs"
    test -f "$libs/$abi/liborchard_android.so"
    kit="build-android-$abi/kit/lib/grove/android/$abi"
    test -f "$kit/kit.txt"
    mkdir -p "$kits"
    cp -R "$kit" "$kits/$abi"
done

version=$(sed -n 's/^project(orchard VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | head -n 1)
IFS=. read -r major minor patch <<< "$version"
code=$(( ${major:-0} * 1000000 + ${minor:-0} * 1000 + ${patch:-0} ))

sign=()
if [ -n "${ORCHARD_KEYSTORE_BASE64:-}" ]; then
    printf '%s' "$ORCHARD_KEYSTORE_BASE64" | base64 -d > "${RUNNER_TEMP:-/tmp}/orchard.keystore"
    sign=(--keystore "${RUNNER_TEMP:-/tmp}/orchard.keystore" --ks-pass env:ORCHARD_KEYSTORE_PASS)
    if [ -n "${ORCHARD_KEY_ALIAS:-}" ]; then sign+=(--key-alias "$ORCHARD_KEY_ALIAS"); fi
else
    echo "::warning::ORCHARD_KEYSTORE_BASE64 isn't set, so the APK is signed with a new debug key; phones that have an older Orchard APK will need to uninstall it first"
fi

mkdir -p "$out_dir"
apk="$(cd "$out_dir" && pwd)/Orchard-$version-android.apk"
"$grove" build UI/orchard.grove --target android \
    --android-kit "$kits" \
    --runtime-lib "$libs" --lib-name orchard_android --abi "$abis" \
    --ndk "$ndk" \
    --set package=io.github.thatnewcoderjscpp.orchard \
    --set label=Orchard \
    --set "version=$version" \
    --set "version-code=$code" \
    --set icon=../Packaging/icons/orchard-512.png \
    "${sign[@]}" \
    -o "$apk"
ls -la "$apk"
unzip -l "$apk" | grep -E "lib/.*/liborchard_android\.so|assets/grove/app\.bundle"
