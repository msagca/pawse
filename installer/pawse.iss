; Inno Setup script for Pawse. Build the Release preset first, then compile this script:
;   iscc /DAppVersion=0.6.1 installer\pawse.iss
; CI passes the version from CMakeLists.txt, so the installer and pawse.exe always agree.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define RootDir ".."
#define RunKey "Software\Microsoft\Windows\CurrentVersion\Run"

[Setup]
AppId={{45A314BE-0A82-4DB9-865B-B945E12DF5E3}
AppName=Pawse
AppVersion={#AppVersion}
AppVerName=Pawse {#AppVersion}
AppPublisherURL=https://github.com/msagca/pawse
AppSupportURL=https://github.com/msagca/pawse/issues
AppUpdatesURL=https://github.com/msagca/pawse/releases
; Pawse runs as the current user and keeps its autostart entry in HKCU, so install per user
; (%LOCALAPPDATA%\Programs\Pawse) without asking for administrator rights.
PrivilegesRequired=lowest
DefaultDirName={autopf}\Pawse
DefaultGroupName=Pawse
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; Matches kSingleInstanceMutexName in src/app.cpp: setup and uninstall ask the user to exit
; Pawse first, so pawse.exe isn't in use.
AppMutex=Local\Pawse.SingleInstance
SetupIconFile={#RootDir}\resources\pawse.ico
UninstallDisplayIcon={app}\pawse.exe
UninstallDisplayName=Pawse
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
OutputDir={#RootDir}\build\installer
OutputBaseFilename=pawse-{#AppVersion}-x64-setup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart"; Description: "Start Pawse when I sign in"; GroupDescription: "Startup:"; Flags: unchecked

[Files]
Source: "{#RootDir}\build\Release\pawse.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RootDir}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Pawse"; Filename: "{app}\pawse.exe"
Name: "{autodesktop}\Pawse"; Filename: "{app}\pawse.exe"; Tasks: desktopicon

[Registry]
; Same value SetStartWithWindows() in src/settings.cpp writes, so the Settings checkbox stays in sync.
Root: HKCU; Subkey: "{#RunKey}"; ValueType: string; ValueName: "Pawse"; ValueData: """{app}\pawse.exe"" --autostart"; Tasks: autostart

[Run]
Filename: "{app}\pawse.exe"; Description: "{cm:LaunchProgram,Pawse}"; Flags: nowait postinstall skipifsilent
; Pawse runs this installer silently to update itself, then exits. Start the new version.
Filename: "{app}\pawse.exe"; Parameters: "--updated"; Flags: nowait; Check: IsPawseUpdate

[Code]
// Pawse passes /PAWSEUPDATE=yes when it runs this installer to update itself.
function IsPawseUpdate: Boolean;
begin
  Result := ExpandConstant('{param:PAWSEUPDATE|no}') = 'yes';
end;

// The autostart entry can also be turned on later from Pawse's Settings, so remove it on
// uninstall whenever it points at this installation, not only when the task was selected.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Command: String;
begin
  if (CurUninstallStep = usPostUninstall) and
     RegQueryStringValue(HKCU, '{#RunKey}', 'Pawse', Command) and
     (Pos(Lowercase(ExpandConstant('{app}\pawse.exe')), Lowercase(Command)) > 0) then
    RegDeleteValue(HKCU, '{#RunKey}', 'Pawse');
end;
