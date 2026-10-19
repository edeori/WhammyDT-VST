#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef RepoRoot
  #error RepoRoot is required
#endif
[Setup]
AppId={{A91D7F50-F2B3-44D8-A976-E379855F529B}
AppName=WhirlDT
AppVersion={#AppVersion}
AppPublisher=Moth Production
AppPublisherURL=https://mothproduction.hu
DefaultDirName={autopf}\Moth Production\WhirlDT
DefaultGroupName=Moth Production
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=installer
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\WhirlDT.exe

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in"; Types: full custom
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
Source: "{#RepoRoot}\build\ci\WhirlDT_artefacts\Release\VST3\WhirlDT.vst3\*"; DestDir: "{commoncf64}\VST3\WhirlDT.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#RepoRoot}\build\ci\WhirlDT_artefacts\Release\Standalone\WhirlDT.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "{#RepoRoot}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\WhirlDT"; Filename: "{app}\WhirlDT.exe"; Components: standalone
