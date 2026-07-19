#!/usr/bin/env bash
# Builds WhirlDT (VST3 + AU + Standalone) and reports where the results
# ended up. COPY_PLUGIN_AFTER_BUILD in CMakeLists.txt already installs the
# VST3/AU into the system plugin folders (~/Library/Audio/Plug-Ins/...), so
# a DAW rescan is all that's needed afterwards - no manual copying required.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_DIR="build"
CONFIG="Release"

echo "==> Configuring ($BUILD_DIR/)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" > /dev/null

echo "==> Building VST3 + AU + Standalone ($CONFIG)..."
cmake --build "$BUILD_DIR" --config "$CONFIG" \
    --target WhirlDT_VST3 WhirlDT_AU WhirlDT_Standalone \
    -j "$(sysctl -n hw.ncpu)"

ARTEFACTS="$BUILD_DIR/WhirlDT_artefacts/$CONFIG"
[ -d "$ARTEFACTS" ] || ARTEFACTS="$BUILD_DIR/WhirlDT_artefacts"

FLAT_VST3_DIR="$BUILD_DIR/VST3"
FLAT_AU_DIR="$BUILD_DIR/AU"
FLAT_STANDALONE_DIR="$BUILD_DIR/Standalone"

rm -rf "$FLAT_VST3_DIR" "$FLAT_AU_DIR" "$FLAT_STANDALONE_DIR"
mkdir -p "$FLAT_VST3_DIR" "$FLAT_AU_DIR" "$FLAT_STANDALONE_DIR"

cp -R "$ARTEFACTS/VST3/WhirlDT.vst3" "$FLAT_VST3_DIR/"
cp -R "$ARTEFACTS/AU/WhirlDT.component" "$FLAT_AU_DIR/"
cp -R "$ARTEFACTS/Standalone/WhirlDT.app" "$FLAT_STANDALONE_DIR/"

echo
echo "==> Build complete."
echo "    VST3:       $FLAT_VST3_DIR/WhirlDT.vst3"
echo "    AU:         $FLAT_AU_DIR/WhirlDT.component"
echo "    Standalone: $FLAT_STANDALONE_DIR/WhirlDT.app"
echo
echo "    Also installed system-wide (COPY_PLUGIN_AFTER_BUILD):"
echo "    ~/Library/Audio/Plug-Ins/VST3/WhirlDT.vst3"
echo "    ~/Library/Audio/Plug-Ins/Components/WhirlDT.component"
