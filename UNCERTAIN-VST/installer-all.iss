; ============================================================================
;  UNCERTAIN VST Suite - UN SEUL installeur pour les TROIS plugins.
;  Genere par BUILD-ALL.bat. Sortie : Installer\UNCERTAIN-VST-Suite-Setup.exe
; ============================================================================
#define Suite   "UNCERTAIN VST Suite"
#define Version "1.0.0"
#define D1 "BLOODMONEY-Master-Plugin\build\BLOODMONEYMaster_artefacts\Release\VST3"
#define D2 "UNCERTAIN-Polish-Plugin\build\UNCERTAINPolish_artefacts\Release\VST3"
#define D3 "UNCERTAIN-Spatial-Plugin\build\UNCERTAINSpatial_artefacts\Release\VST3"

[Setup]
AppId={{E5F60718-2A3B-4C5D-9E0F-1A2B3C4D5E6F}
AppName={#Suite}
AppVersion={#Version}
AppPublisher=UNCERTAIN
UninstallDisplayName={#Suite}
DefaultDirName={autopf}\UNCERTAIN
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=Installer
OutputBaseFilename=UNCERTAIN-VST-Suite-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
InfoAfterFile=POST_INSTALL.txt

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"

[Files]
Source: "{#D1}\BLOODMONEY Master.vst3\*"; DestDir: "{commoncf}\VST3\BLOODMONEY Master.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#D2}\UNCERTAIN Polish.vst3\*";  DestDir: "{commoncf}\VST3\UNCERTAIN Polish.vst3";  Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#D3}\UNCERTAIN Spatial.vst3\*"; DestDir: "{commoncf}\VST3\UNCERTAIN Spatial.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
