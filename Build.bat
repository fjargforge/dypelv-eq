@echo off
setlocal
cd /d "%~dp0"
echo ============================================
echo   Dypelv EQ - Windows build
echo ============================================
echo.

rem CMake: use the one on PATH, otherwise the one that comes with Visual Studio / Build Tools (found with vswhere)
where cmake >nul 2>nul
if not errorlevel 1 goto havecmake
set "VSDIR="
set "VSW=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSW%" for /f "usebackq delims=" %%i in (`"%VSW%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath`) do set "VSDIR=%%i"
if defined VSDIR if exist "%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
where cmake >nul 2>nul
if errorlevel 1 goto nocmake
:havecmake

rem JUCE is inside the project folder, git is only needed if that folder was deleted
if exist "JUCE\CMakeLists.txt" goto havejuce
where git >nul 2>nul
if errorlevel 1 goto nogit
:havejuce

echo [1/3] Configuring...
cmake -B build -A x64
if errorlevel 1 goto cfgfail

echo.
echo [2/3] Building (the first build takes 10 to 20 minutes)...
cmake --build build --config Release --parallel
if errorlevel 1 goto buildfail

echo.
echo [3/3] Collecting the plugin...
if exist Output rmdir /s /q Output
mkdir Output
set "ART=build\DypelvEQ_artefacts\Release"
if exist "%ART%\VST3\Dypelv EQ.vst3" xcopy /e /i /y /q "%ART%\VST3\Dypelv EQ.vst3" "Output\Dypelv EQ.vst3" >nul
if exist "%ART%\Standalone\Dypelv EQ.exe" copy /y "%ART%\Standalone\Dypelv EQ.exe" "Output\" >nul

dir /b Output >nul 2>nul
if errorlevel 1 goto nofiles

rem ---- DypelvEQ-windows.zip: what Install-Windows.ps1 downloads from the GitHub release ----
powershell -NoProfile -Command "Get-ChildItem 'Output' | Where-Object { $_.Extension -ne '.zip' -and $_.Name -notlike 'DypelvEQ-Setup*' } | Compress-Archive -DestinationPath 'Output\DypelvEQ-windows.zip' -Force"

rem ---- installer (needs Inno Setup, free) ----
set "ISCC="
where iscc >nul 2>nul && set "ISCC=iscc"
if not defined ISCC if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not defined ISCC goto noiscc
echo.
echo Building the installer...
"%ISCC%" /Qp "Installer\DypelvEQ.iss"
if errorlevel 1 goto issfail
goto finish

:noiscc
echo.
echo Inno Setup was not found, so only the plain files were made (no installer).
echo To get the installer (Output\DypelvEQ-Setup-*.exe): install Inno Setup 6 from https://jrsoftware.org/isdl.php
echo (or run: winget install JRSoftware.InnoSetup) and run this file again.
goto finish

:issfail
echo.
echo The installer script failed. Send me the text above. The plain files in Output are fine.

:finish
echo.
echo Done. Your files are in: %cd%\Output
echo.
echo For the GitHub release: Output\DypelvEQ-windows.zip (Install-Windows.ps1 downloads it).
echo.
echo By hand: copy the folder "Dypelv EQ.vst3" from Output to
echo   C:\Program Files\Common Files\VST3
echo (needs administrator rights), then rescan plugins in the DAW.
start "" Output
pause
exit /b 0

:nocmake
echo CMake was not found.
echo Fix 1: open Visual Studio Installer, press Modify on Build Tools and tick "C++ CMake tools for Windows".
echo Fix 2: install CMake from https://cmake.org/download/ (choose "Add CMake to the system PATH").
echo Then open a NEW window and run this file again.
pause
exit /b 1

:nogit
echo Git is missing. Install it from https://git-scm.com/download/win
echo then run this file again.
pause
exit /b 1

:cfgfail
echo.
echo Configure failed. You need Visual Studio 2022 (Community is free) or the
echo Build Tools, with the "Desktop development with C++" workload.
echo Install it, then run this file again. Otherwise send me the text above.
pause
exit /b 1

:buildfail
echo.
echo Build failed. Send me the red error text above.
pause
exit /b 1

:nofiles
echo.
echo No plugin files were found in the build folder. Send me everything printed in this window.
pause
exit /b 1
