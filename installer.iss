﻿﻿﻿; SOCD_Bridge Inno Setup 安装脚本
; 版权所有 (c) 2026 Bichuntea

#define MyAppName "SOCD_Bridge"
#define MyAppVersion "1.0"
#define MyAppPublisher "Bichuntea"
#define MyAppURL "https://github.com/Bichuntea/SOCD_Bridge"
#define MyAppExeName "SOCD_Bridge.exe"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
LicenseFile=EULA_CN.txt
OutputDir=installer_output
OutputBaseFilename={#MyAppName}_Setup_{#MyAppVersion}
SetupIconFile=socd_bridge.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
VersionInfoVersion={#MyAppVersion}.0.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} 安装程序
VersionInfoCopyright=Copyright (c) 2026 {#MyAppPublisher}
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}.0.0
WizardSizePercent=100

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "config.cfg"; DestDir: "{app}"; Flags: ignoreversion onlyifdoesntexist
Source: "icon.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "icon_off.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "socd_bridge.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyAppExeName}.manifest"; DestDir: "{app}"; Flags: ignoreversion
Source: "meta\*"; DestDir: "{app}\meta"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{app}\meta\profiles"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\编辑配置文件"; Filename: "{app}\config.cfg"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: quicklaunchicon

[Registry]
Root: HKCU; Subkey: "Software\{#MyAppName}"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\{#MyAppName}"; ValueType: string; ValueName: "Version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 SOCD_Bridge"; Flags: nowait postinstall skipifsilent runascurrentuser

[UninstallRun]
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM {#MyAppExeName}"; Flags: runhidden

[UninstallDelete]
Type: filesandordirs; Name: "{app}\meta"
Type: files; Name: "{app}\config.cfg"

[Code]
function InitializeSetup(): Boolean;
var
  ResultCode: Integer;
  UninstallString: String;
begin
  Result := True;
  
  if RegValueExists(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1', 'UninstallString') then
  begin
    if MsgBox('检测到已安装的旧版本，是否先卸载？', mbConfirmation, MB_YESNO) = IDYES then
    begin
      if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1', 'UninstallString', UninstallString) then
      begin
        Exec(RemoveQuotes(UninstallString), '/SILENT', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
      end;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  AppDir: String;
begin
  if CurStep = ssPostInstall then
  begin
    AppDir := ExpandConstant('{app}');
    
    if not FileExists(AppDir + '\config.cfg') then
    begin
      if FileExists(AppDir + '\meta\backup.socd_bridge') then
        FileCopy(AppDir + '\meta\backup.socd_bridge', AppDir + '\config.cfg', False);
    end;
  end;
end;

function NeedsRestart(): Boolean;
begin
  Result := False;
end;
