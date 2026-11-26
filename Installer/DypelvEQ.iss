; Dypelv EQ - Windows installer (Inno Setup 6.3 or newer, free: https://jrsoftware.org/isinfo.php)
; Build.bat runs it automatically when Inno Setup is installed. Do not change AppId, it identifies the app for updates and uninstall.

#ifndef AppVer
  #define AppVer "0.2.0"
#endif
#define AppName "Dypelv EQ"
#define Pub     "Fjarg"

[Setup]
AppId={{23BB0239-6C39-4C7A-8EE7-BB0AFB7C07F9}
AppName={#AppName}
AppVersion={#AppVer}
AppPublisher={#Pub}
VersionInfoVersion={#AppVer}
DefaultDirName={autopf}\{#Pub}\{#AppName}
DefaultGroupName={#Pub}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\Output
OutputBaseFilename=DypelvEQ-Setup-{#AppVer}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "vst3";       Description: "VST3 plugin (for your DAW)";  Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Standalone application";       Types: full

[Files]
; VST3 goes to the shared folder every DAW scans: C:\Program Files\Common Files\VST3
Source: "..\Output\Dypelv EQ.vst3\*"; DestDir: "{commoncf64}\VST3\Dypelv EQ.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\Output\Dypelv EQ.exe";    DestDir: "{app}"; Components: standalone; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\Dypelv EQ.exe"; Components: standalone

[Run]
Filename: "{app}\Dypelv EQ.exe"; Description: "Start {#AppName}"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked
