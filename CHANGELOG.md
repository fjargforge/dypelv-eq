# Changelog

## 0.2.0
- **Dynamics reworked.** The band lives between two gains: Rest (quiet) and Active (loud). The level moves it from one to the other around the Threshold, over a Width in dB. This replaces Compress / Expand with Ratio and Knee. Active shows as a dashed curve on the graph; the dot on the node shows the live gain.
- Curve editor on the Dynamics tab: drag Rest, Active, Threshold and Width right on the curve (Shift = fine).
- Detector: Band (the band's own range), Full (the whole signal) or Sidechain. Follow keeps the detector filter on the band; switch it off to give the detector its own frequency and Q. Auto threshold follows the average level.
- Exact numbers: frequency 62.060 Hz / 1.235 kHz, Q, gain and threshold to hundredths; a typed value is kept exactly, and pressing Enter on an untouched field no longer rounds it. Type `1.2k`, `-3,5` or a note name (`C#2`); the typographic minus is accepted.
- Old sessions and user presets with Compress / Expand are converted to Rest / Active when they are opened (the curve stays within about 1 dB).
- Windows: installs with one PowerShell command (`Install-Windows.ps1`); the setup .exe is gone. `Build.bat` makes `DypelvEQ-windows.zip` for the release.
- macOS: `Build.command` now always builds a clean universal binary (Apple Silicon + Intel) and prints the architectures, so the plugin loads inside hosts running under Rosetta.
- Invert and Split respect the Active gain of a dynamic band; switching Follow off leaves the detector where it was instead of jumping to its own defaults.
- Safety net: if a band ever runs away (a Square or Saw LFO jumping the frequency of a narrow, resonant filter can pump it up), that band is cleared instead of getting louder and louder.

## 0.1.1
- Output trim (OUT field in the header)
- Interface scale 100 to 200 %, remembered between sessions
- Long spectrum averaging (5 / 15 / 60 s)
- Drift LFO shape (smooth, never repeating)
- Band colour options: Colour / Soft / Mono
- First-run hint
- Notice when Side is silent because the signal is mono
- Fixed: full-height block in the analyser when no audio is playing
- Film grain moved to the settings menu (the G key is gone)

## 0.1.0
- First public version
