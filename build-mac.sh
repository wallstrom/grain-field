#!/bin/bash
# Builds Grain Field and installs it for Logic Pro (AU) and Ableton Live (VST3).
# Run it from Terminal:  bash build-mac.sh
set -e
cd "$(dirname "$0")"

say() { printf "\n\033[1m%s\033[0m\n" "$1"; }

say "Grain Field: building the plugin"

# 1. Apple's command line tools (compiler, git). Full Xcode is not needed.
if ! xcode-select -p >/dev/null 2>&1; then
    say "Apple's command line tools are missing."
    echo "A window will open. Click Install, wait until it finishes, then run this script again."
    xcode-select --install || true
    exit 1
fi

# 2. CMake. Uses yours if installed, otherwise downloads a private copy into this folder.
CMAKE="$(command -v cmake || true)"
if [ -z "$CMAKE" ]; then
    CMAKE_VERSION=3.30.5
    TOOLS="$PWD/.tools"
    CMAKE="$TOOLS/cmake-$CMAKE_VERSION-macos-universal/CMake.app/Contents/bin/cmake"
    if [ ! -x "$CMAKE" ]; then
        say "Downloading CMake $CMAKE_VERSION (about 50 MB, only the first time)"
        mkdir -p "$TOOLS"
        curl -L --fail -o "$TOOLS/cmake.tar.gz" "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-macos-universal.tar.gz"
        tar -xzf "$TOOLS/cmake.tar.gz" -C "$TOOLS"
        rm "$TOOLS/cmake.tar.gz"
    fi
fi

# 3. Configure and build. The first build downloads JUCE and takes 5 to 15 minutes.
say "Configuring (downloads JUCE the first time)"
"$CMAKE" -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release

say "Compiling. This takes a while the first time; later builds are quicker."
"$CMAKE" --build build --config Release -j "$(sysctl -n hw.ncpu)"

# 4. Ask macOS to rescan Audio Units so Logic sees the new version.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

say "Done."
echo "Installed:"
echo "  Logic Pro (AU):   ~/Library/Audio/Plug-Ins/Components/Grain Field.component"
echo "  Ableton (VST3):   ~/Library/Audio/Plug-Ins/VST3/Grain Field.vst3"
echo "  Standalone app:   build/GrainField_artefacts/Release/Standalone/Grain Field.app"
echo
echo "Open Logic and add a Software Instrument track: AU Instruments > Grain Field Audio > Grain Field."
echo "In Ableton: Plug-ins > VST3 > Grain Field Audio > Grain Field (turn on 'Use VST3 Plug-in System Folders' in Settings > Plug-ins)."
