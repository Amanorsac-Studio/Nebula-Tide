; Nebula Tide — Inno Setup installer script
; Build (from project root):
;   ISCC.exe /DAppVersion=2.0.0 installer\NebulaTide.iss

#ifndef AppVersion
  #define AppVersion "2.0.0"
#endif
#define AppName "Nebula Tide V2"
#define AppPublisher "Amanorsac Studio"
#define BuildDir "..\build\NebulaTide_artefacts\Release"

[Setup]
; a different GUID from version 1, so Windows installs this beside it
; rather than treating it as an upgrade and uninstalling the old one
AppId={{C4F18A7D-52E9-4B36-8A11-9D73E0B4C215}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Nebula Tide V2
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputBaseFilename=NebulaTide-{#AppVersion}-Setup
OutputDir=output
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
SetupIconFile=..\build\NebulaTide_artefacts\JuceLibraryCode\icon.ico
UninstallDisplayIcon={app}\Nebula Tide V2.exe

[Tasks]
Name: "vst3"; Description: "Install VST3 plugin (for DAWs such as Ableton, Cubase, Reaper, FL Studio)"; GroupDescription: "Components:"
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

; ─────────────────────────────────────────────────────────────────────────
;  The sound library is 264 MB and people should be able to see where it is
;  going and put it somewhere else — a second drive, or off the system disk.
;  It also gives anyone whose install goes wrong a path to point the app at
;  afterwards, instead of a blank screen with nowhere to go.
; ─────────────────────────────────────────────────────────────────────────
[Code]
var
  SoundsPage: TInputDirWizardPage;

procedure InitializeWizard;
begin
  SoundsPage := CreateInputDirPage(wpSelectComponents,
    'Sound Library Location',
    'Where should the Nebula Tide sounds be installed?',
    'The sound library is about 264 MB. It is shared by the app and the plugin,' + #13#10 +
    'so both read it from the same place.' + #13#10 + #13#10 +
    'You can change this later in the app under SETTINGS, and the app can be' + #13#10 +
    'pointed at this folder again if anything goes wrong.',
    False, '');
  SoundsPage.Add('');
  SoundsPage.Values[0] := ExpandConstant('{commonappdata}\Nebula Tide');
end;

function SoundsDir(Param: String): String;
begin
  Result := SoundsPage.Values[0];
end;

// Written so the app finds the library wherever it was put, without the user
// having to go and locate it by hand on first launch.
procedure CurStepChanged(CurStep: TSetupStep);
var
  PointerDir: String;
begin
  if CurStep = ssPostInstall then
  begin
    // Documents\Amanorsac Studio\<Product>\ — the same place productDataDir()
    // in PluginProcessor.cpp looks. If one moves, the other moves with it.
    PointerDir := ExpandConstant('{userdocs}\Amanorsac Studio\Nebula Tide');
    ForceDirectories(PointerDir);
    SaveStringToFile(PointerDir + '\library-path.txt', SoundsPage.Values[0], False);
  end;
end;

[Files]
; standalone app
Source: "{#BuildDir}\Standalone\Nebula Tide V2.exe"; DestDir: "{app}"; Flags: ignoreversion

; the encrypted sound container, into the folder chosen on the Sound Library page
#if FileExists("..\NebulaTide.ntlib")
Source: "..\NebulaTide.ntlib"; DestDir: "{code:SoundsDir}"; Flags: ignoreversion
#endif

; attribution for the Creative Commons material in the library (CC-BY requires
; the credit to travel with the work, so it ships beside the app)
#if FileExists("..\CREDITS.txt")
Source: "..\CREDITS.txt"; DestDir: "{app}"; Flags: ignoreversion
#endif

; VST3 into the system VST3 folder
Source: "{#BuildDir}\VST3\Nebula Tide V2.vst3\*"; DestDir: "{commoncf64}\VST3\Nebula Tide V2.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: vst3

[InstallDelete]
; remove the old unencrypted sound folders left by versions up to 1.1.7
Type: filesandordirs; Name: "{commonappdata}\Nebula Tide\presets"
Type: filesandordirs; Name: "{app}\presets"
Type: filesandordirs; Name: "{commoncf64}\VST3\presets"

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\Nebula Tide V2.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\Nebula Tide V2.exe"; Tasks: desktopicon

[Run]
; runasoriginaluser: setup runs as administrator, and without this flag the app
; launched from the final page inherits that, and Windows silently refuses
; drag and drop from Explorer (a normal-rights process) into an elevated window.
Filename: "{app}\Nebula Tide V2.exe"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent runasoriginaluser
