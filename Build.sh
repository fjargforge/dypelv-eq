#!/bin/bash
# Linux build. Plugins are placed in the Output folder, nothing is installed.
cd "$(dirname "$0")" || exit 1

if ! command -v cmake >/dev/null 2>&1 || ! command -v g++ >/dev/null 2>&1; then
  echo "Needs a C++ compiler and CMake. On Debian / Ubuntu:"
  echo "  sudo apt install build-essential cmake git pkg-config libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev"
  exit 1
fi

cmake -B build -DCMAKE_BUILD_TYPE=Release || { echo "Configure failed."; exit 1; }
cmake --build build --config Release -j "$(nproc)" || { echo "Build failed. Send me the error text."; exit 1; }

rm -rf Output && mkdir -p Output
find build -maxdepth 5 -type d -name "*.vst3" -exec cp -R {} Output/ \;
find build -maxdepth 5 -type f -name "Dypelv EQ" -perm -u+x -exec cp {} Output/ \;

if [ -z "$(ls -A Output)" ]; then echo "No plugins were found in build/."; exit 1; fi
echo ""
echo "Done. Files are in: $(pwd)/Output"
echo "VST3: copy 'Dypelv EQ.vst3' to ~/.vst3/ and rescan in your host."
