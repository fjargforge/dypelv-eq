# Dypelv EQ

A free, open-source parametric equaliser for ambient and dark ambient, by Fjarg. VST3, Audio Unit and standalone.
Slow, smooth, and quiet to look at: one dark plot, round nodes, no knobs.

![Dypelv EQ](docs/main.png)

| Movement: LFO per band | Bands, stereo modes, dynamics |
|---|---|
| ![LFO](docs/lfo.png) | ![Menu](docs/menu.png) |

[Download](../../releases/latest) · [Changelog](CHANGELOG.md) · [Report a bug](../../issues/new/choose)

**Status: early version (0.2.0).** Built and used on an Apple Silicon Mac. On Windows the standalone app has been tested; the VST3 has not yet been tried in a host. Expect rough edges.

## What it does

- Up to 24 bands: peak, low / high shelf, high / low pass (up to steep slopes), notch, band pass, tilt shelf
- Stereo, Left, Right, Mid or Side per band; a **linked pair** (L|R or M|S, the second side mirrored) or **two independent bands**
- Dynamic bands: the band moves between two gains, **Rest** (quiet) and **Active** (loud), around a threshold. Drag the points on the curve; the detector listens to the band, the whole signal, or the sidechain
- Exact numbers: frequency 62.060 Hz or 1.235 kHz, Q and gain to hundredths (what you type is kept exactly). Type `1.2k`, `62.06` or a note name such as `C#2`
- Two LFOs per band (frequency, and gain or Q): sine, triangle, square, saws, random, and **Drift**, a smooth wander that never repeats. Free-running or locked to the host tempo
- Zero-latency and linear-phase modes
- Spectrum analyser with input and output curves, slope compensation, and **long averaging (5 / 15 / 60 s)** for slow material
- Solo and mute per band (S and M keys), delta listen, auto gain match, output gain
- MIDI learn on any parameter, undo / redo, A / B, presets, optional LUFS meter
- Interface scale 100 to 200 %, band colours Colour / Soft / Mono, optional film grain

## Install

### Build it yourself (macOS, recommended)

1. Install the Xcode command line tools if you do not have them (`xcode-select --install`).
2. Download this repository ("Code" -> "Download ZIP"), unzip it.
3. Double-click `Build.command`. The first build downloads JUCE and takes a while.
4. The plugins appear in the `Output` folder. Copy them:
   - `Dypelv EQ.vst3` to `~/Library/Audio/Plug-Ins/VST3/`
   - `Dypelv EQ.component` to `~/Library/Audio/Plug-Ins/Components/`

A plugin you built yourself is not blocked by macOS.

### Ready-made zip (macOS)

Download `DypelvEQ-*-mac.zip` from the Releases page, open it and copy the files as above. The plugins are not signed with an Apple Developer certificate,
so macOS blocks a downloaded copy. Remove the block once, in Terminal:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Dypelv EQ.vst3"
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Dypelv EQ.component"
```

Then rescan plugins in your host.

### Windows

Open PowerShell as administrator (Start, type "PowerShell", right-click, **Run as administrator**) and paste:

```
irm https://raw.githubusercontent.com/fjargforge/dypelv-eq/main/Install-Windows.ps1 | iex
```

The script (about 60 lines, read it first if you like: `Install-Windows.ps1`) downloads `DypelvEQ-windows.zip` from the latest release, puts the VST3 into
`C:\Program Files\Common Files\VST3` and the standalone app into `C:\Program Files\Fjarg\Dypelv EQ` (with a Start menu entry). Then rescan plugins in your DAW.
Close the DAW first when you update, or the old plugin cannot be replaced. To remove it:

```
Remove-Item "$env:CommonProgramFiles\VST3\Dypelv EQ.vst3" -Recurse -Force
```

To build it yourself: install Visual Studio with the "Desktop development with C++" workload, CMake and Git, then run `Build.bat`.
The standalone app works; the VST3 is untested in DAWs, so reports and fixes are welcome.

### Linux

Untested. Install the build tools and libraries (Debian / Ubuntu):

```
sudo apt install build-essential cmake git pkg-config libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev
```

Then run `./Build.sh`. Copy `Dypelv EQ.vst3` from `Output` to `~/.vst3/` and rescan in your host. Reports and fixes are welcome.

## Licence

Copyright (C) 2026 Fjarg. Released under the **GNU Affero General Public Licence v3** (see `LICENSE`).
Built with [JUCE](https://juce.com), used under its AGPLv3 option.

## Feedback

Bugs, ideas and screenshots: open an issue.
