; Nebula Tide — Inno Setup installer script
; Build (from project root):
;   ISCC.exe /DAppVersion=1.0.0-beta1 installer\NebulaTide.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define AppName "Nebula Tide"
#define AppPublisher "Nebula Tide"
#define BuildDir "..\build\NebulaTide_artefacts\Release"

[Setup]
AppId={{7E2B9C41-0A63-4E5B-9B1D-4A2C6F1E8B30}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Nebula Tide
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputBaseFilename=NebulaTide-{#AppVersion}-Setup
OutputDir=output
SetupIconFile=..\build\NebulaTide_artefacts\JuceLibraryCode\icon.ico
UninstallDisplayIcon={app}\Nebula Tide.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

[Tasks]
Name: "vst3"; Description: "Install VST3 plugin (for DAWs such as Ableton, Cubase, Reaper, FL Studio)"; GroupDescription: "Components:"
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
; standalone app
Source: "{#BuildDir}\Standalone\Nebula Tide.exe"; DestDir: "{app}"; Flags: ignoreversion
; ONE shared sound library (pads, fx, textures, manifest) used by both the app
; and the VST3 — C:\ProgramData\Nebula Tide\presets
Source: "..\presets\*"; DestDir: "{commonappdata}\Nebula Tide\presets"; Flags: ignoreversion recursesubdirs createallsubdirs
; VST3 into the system VST3 folder
Source: "{#BuildDir}\VST3\Nebula Tide.vst3\*"; DestDir: "{commoncf64}\VST3\Nebula Tide.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: vst3

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\Nebula Tide.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\Nebula Tide.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Nebula Tide.exe"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
