#!/bin/bash
# Double-click this file to build Dypelv EQ. Plugins and the standalone app are placed in the Output folder, nothing is installed.
cd "$(dirname "$0")" || exit 1

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Installing Xcode Command Line Tools. Finish that installer, then double-click this file again."
  xcode-select --install
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  if command -v brew >/dev/null 2>&1; then
    brew install cmake
  else
    echo "CMake is missing. Download the macOS installer from https://cmake.org/download/ , install it, then run this again."
    exit 1
  fi
fi

cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release || { echo "Configure failed."; read -p "Press Enter"; exit 1; }
cmake --build build --config Release -j 8 || { echo "Build failed. Send me the red error text."; read -p "Press Enter"; exit 1; }

rm -rf Output && mkdir -p Output
echo ""
echo "Looking for built plugins..."
find build \( -name "*.vst3" -o -name "*.component" -o -name "*.app" \) -maxdepth 6 -type d | while read -r f; do
  echo "Found: $f"
  cp -R "$f" Output/
done

if [ -z "$(ls -A Output)" ]; then
  echo ""
  echo "No plugins were found. Build folder contents:"
  find build -maxdepth 3 -name "*artefacts*" -o -maxdepth 4 -name "*.vst3" -o -maxdepth 4 -name "*.component"
  echo "Please send me everything printed in this window."
  read -p "Press Enter to close"
  exit 1
fi

for f in Output/*; do codesign --force --deep -s - "$f"; done
xattr -dr com.apple.quarantine Output 2>/dev/null

echo ""
echo "Done. Your files are in: $(pwd)/Output"
open Output
read -p "Press Enter to close"
