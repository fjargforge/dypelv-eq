# Dypelv EQ: installs the VST3 (and the standalone app) on Windows. No installer program needed.
#
# Open PowerShell as Administrator (Start, type "PowerShell", right-click, Run as administrator), then paste:
#   irm https://raw.githubusercontent.com/fjargforge/dypelv-eq/main/Install-Windows.ps1 | iex
#
# It downloads DypelvEQ-windows.zip from the latest release. If that zip lies next to this script, the local copy is used instead.
# The VST3 goes to C:\Program Files\Common Files\VST3, the standalone app to C:\Program Files\Fjarg\Dypelv EQ.

$ErrorActionPreference = 'Stop'

$principal = New-Object Security.Principal.WindowsPrincipal ([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host 'Open PowerShell as Administrator (right-click, Run as administrator) and run this again.' -ForegroundColor Yellow
    return
}

$repo = 'fjargforge/dypelv-eq'
$work = Join-Path $env:TEMP 'DypelvEQ-install'
$zip  = Join-Path $work 'DypelvEQ-windows.zip'

try {
    if (Test-Path $work) { Remove-Item $work -Recurse -Force }
    New-Item -ItemType Directory -Path $work | Out-Null

    $local = if ($PSScriptRoot) { Join-Path $PSScriptRoot 'DypelvEQ-windows.zip' } else { '' }
    if ($local -and (Test-Path $local)) {
        Copy-Item $local $zip
    } else {
        Write-Host 'Downloading the latest release...'
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest "https://github.com/$repo/releases/latest/download/DypelvEQ-windows.zip" -OutFile $zip -UseBasicParsing
    }

    Expand-Archive -Path $zip -DestinationPath $work -Force
    Get-ChildItem $work -Recurse -File | Unblock-File

    $src = Join-Path $work 'Dypelv EQ.vst3'
    if (-not (Test-Path $src)) { throw 'Dypelv EQ.vst3 was not found inside the zip.' }

    # 64-bit folders, also when this is a 32-bit PowerShell
    $common = if ($env:CommonProgramW6432) { $env:CommonProgramW6432 } else { $env:CommonProgramFiles }
    $progs  = if ($env:ProgramW6432) { $env:ProgramW6432 } else { $env:ProgramFiles }

    $vst3Dir = Join-Path $common 'VST3'
    $dst = Join-Path $vst3Dir 'Dypelv EQ.vst3'
    New-Item -ItemType Directory -Path $vst3Dir -Force | Out-Null
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }      # fails if a DAW still has the old plugin loaded
    Copy-Item $src $dst -Recurse -Force
    Write-Host "VST3 installed: $dst" -ForegroundColor Green

    $exe = Join-Path $work 'Dypelv EQ.exe'
    if (Test-Path $exe) {
        $app = Join-Path $progs 'Fjarg\Dypelv EQ'
        New-Item -ItemType Directory -Path $app -Force | Out-Null
        Copy-Item $exe $app -Force
        $lnk = Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\Dypelv EQ.lnk'
        $shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
        $shortcut.TargetPath = Join-Path $app 'Dypelv EQ.exe'
        $shortcut.Save()
        Write-Host "Standalone app installed: $app (Start menu: Dypelv EQ)" -ForegroundColor Green
    }

    Write-Host 'Done. Rescan plugins in your DAW.'
}
catch {
    Write-Host "Install failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'If the old plugin could not be replaced, close your DAW and try again.'
}
finally {
    if (Test-Path $work) { Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue }
}
