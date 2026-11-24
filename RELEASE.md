# Dypelv EQ: release checklist

None of this has been run yet. The commands are the standard ones for each tool; run them on your machines and fix what they report.
Codes of this plugin: manufacturer `Fjrg`, plugin `Dypl`, type `aufx` (effect). Bundle names: `Dypelv EQ.vst3`, `Dypelv EQ.component`, `Dypelv EQ.app`.

## 0. Before anything is given to other people

- **JUCE licence.** JUCE 8 is dual-licensed: AGPLv3 (then the whole plugin's source must be published under AGPLv3), or a JUCE licence.
  The free JUCE 8 tier ("Starter") covers closed-source releases while revenue / funding stays under 20 000 USD a year; there is no splash screen
  any more. Register on juce.com and read the current EULA before the first public release (this note comes from JUCE's forum, not from the EULA itself).
- **Version.** `project(... VERSION ...)` in `CMakeLists.txt`, and `AppVer` in `Installer/DypelvEQ.iss` (or `iscc /DAppVer=0.2.0`).
- **Clean build** from an empty `build` folder: `Build.command` (Mac), `Build.bat` (Windows). Zero warnings that matter.

## 1. pluginval (works on Mac and Windows)

Download from https://github.com/Tracktion/pluginval/releases. Strictness 5 is the usual bar for hosts; 10 is the strictest.

```
# Mac
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --validate-in-process "Output/Dypelv EQ.vst3"
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --validate-in-process "Output/Dypelv EQ.component"

# Windows (PowerShell)
.\pluginval.exe --strictness-level 10 --validate-in-process "C:\Program Files\Common Files\VST3\Dypelv EQ.vst3"
```

Things most likely to be reported here: parameter changes from the host thread while the editor is open, state restore with
old sessions (parameters were only ever added, never renamed), latency changes with the phase mode (that parameter is not automatable on purpose).

## 2. auval (Mac, Audio Units only)

```
auval -a | grep -i dypl           # is it registered?
auval -v aufx Dypl Fjrg           # full validation
```

If it is not found: copy `Dypelv EQ.component` to `~/Library/Audio/Plug-Ins/Components`, then `killall -9 AudioComponentRegistrar`
and try again.

## 3. macOS: signing and notarization (needs the Apple Developer Program, 99 USD / year)

```
# one time: store the credentials (app-specific password from appleid.apple.com)
xcrun notarytool store-credentials "dypelv" --apple-id "you@example.com" --team-id "TEAMID" --password "xxxx-xxxx-xxxx-xxxx"

# sign every bundle with the hardened runtime and a timestamp (replaces the ad-hoc signature that Build.command makes)
for b in "Output/Dypelv EQ.vst3" "Output/Dypelv EQ.component" "Output/Dypelv EQ.app"; do
  codesign --force --deep --options runtime --timestamp --sign "Developer ID Application: YOUR NAME (TEAMID)" "$b"
  codesign --verify --deep --strict --verbose=2 "$b"
done

# an installer package puts the plugins where hosts look for them
pkgbuild --component "Output/Dypelv EQ.vst3"      --install-location "/Library/Audio/Plug-Ins/VST3"       vst3.pkg
pkgbuild --component "Output/Dypelv EQ.component" --install-location "/Library/Audio/Plug-Ins/Components" au.pkg
pkgbuild --component "Output/Dypelv EQ.app"       --install-location "/Applications"                      app.pkg
productbuild --package vst3.pkg --package au.pkg --package app.pkg --sign "Developer ID Installer: YOUR NAME (TEAMID)" DypelvEQ-0.1.0.pkg

# notarize, wait for the verdict, staple the ticket into the .pkg
xcrun notarytool submit DypelvEQ-0.1.0.pkg --keychain-profile "dypelv" --wait
xcrun stapler staple DypelvEQ-0.1.0.pkg
spctl --assess --type install -vv DypelvEQ-0.1.0.pkg
```

If notarization is rejected, `xcrun notarytool log <submission-id> --keychain-profile dypelv` says which file is the problem
(usually a binary without the hardened runtime or without a timestamp).

## 4. Windows: build, installer, signing

```
# Visual Studio 2022 with the "Desktop development with C++" workload, CMake in PATH
Build.bat                                             # builds Release, runs Inno Setup when it is installed
iscc Installer\DypelvEQ.iss                           # or by hand: output goes to Output\DypelvEQ-Setup-<version>.exe
```

The VST3 goes to `C:\Program Files\Common Files\VST3` (the installer does it). Signing avoids the SmartScreen warning;
it needs a code-signing certificate (an EV certificate removes the warning at once, a normal one builds reputation over time):

```
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a "Output\Dypelv EQ.vst3\Contents\x86_64-win\Dypelv EQ.vst3"
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a "Output\DypelvEQ-Setup-0.1.0.exe"
```

## 5. By hand, in real hosts (Reaper, Logic, Ableton, FL Studio, Cubase, Bitwig)

- [ ] Opens and closes 20 times in a row without a crash; two instances at once; the window resizes and goes full screen
- [ ] Project save / load: bands, LFOs, OUT gain, MIDI mappings, preset name come back; the card keeps its place
- [ ] Automation: gain, frequency, Q, threshold, mix, LFO rate / amount, OUT gain are listed and move the plugin
      (display settings and mode switches are not automatable on purpose)
- [ ] Sample rates 44.1 / 48 / 96 / 192 kHz, buffer sizes 32 to 2048, mono and stereo tracks, sidechain input
- [ ] Phase modes report their latency and the host compensates; offline render matches real-time playback
- [ ] Bypass and delta do not click; OUT gain does not click when dragged
- [ ] Interface scale 100 / 125 / 150 / 200 % in each host (the window must follow the new size)
- [ ] High-DPI Windows monitor, and a window moved between a Retina and a normal screen
- [ ] CPU: Reaper's Performance Meter with 10 active bands and 2 LFOs, spectrum on, against the earlier build

## 6. What this build has never had

Compiled and run on Windows; pluginval; auval; a signed / notarized package; the new interface scale, spectrum averaging, Drift LFO,
OUT gain and band colour options (all were only syntax-checked in the sandbox where the code is written).
