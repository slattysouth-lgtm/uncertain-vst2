; ============================================================================
;  UNCERTAIN Polish - Installeur Windows partageable (Inno Setup)
;  Compile une fois -> "UNCERTAIN-Polish-Setup.exe" diffusable a volonte.
;  PREREQUIS : avoir compile le plugin (build.bat) + Inno Setup 6.3+.
;  Ouvre ce fichier dans Inno Setup -> Compile (F9). Sortie : .\Installer\
; ============================================================================

#define MyName      "UNCERTAIN Polish"
#define MyVersion   "1.0.0"
#define MyPublisher "UNCERTAIN"
#define BuildDir    "build\UNCERTAINPolish_artefacts\Release"

[Setup]
AppId={{C2D1E3F4-5A6B-4C7D-8E9F-0A1B2C3D4E5F}
AppName={#MyName}
AppVersion={#MyVersion}
AppPublisher={#MyPublisher}
UninstallDisplayName={#MyName}
DefaultDirName={autopf}\UNCERTAIN
DefaultGroupName=UNCERTAIN
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=Installer
OutputBaseFilename=UNCERTAIN-Polish-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
InfoAfterFile=POST_INSTALL.txt

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"

[Components]
Name: "vst3";       Description: "Plugin VST3 (Mixcraft, FL Studio, etc.)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Application autonome (sans DAW) - optionnel"; Types: full

[Types]
Name: "full";   Description: "Tout installer"
Name: "custom"; Description: "Personnalise"; Flags: iscustom

[Files]
Source: "{#BuildDir}\VST3\UNCERTAIN Polish.vst3\*"; \
    DestDir: "{commoncf}\VST3\UNCERTAIN Polish.vst3"; \
    Flags: recursesubdirs createallsubdirs ignoreversion; Components: vst3
Source: "{#BuildDir}\Standalone\UNCERTAIN Polish.exe"; \
    DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{group}\UNCERTAIN Polish";              Filename: "{app}\UNCERTAIN Polish.exe"; Components: standalone
Name: "{group}\Desinstaller UNCERTAIN Polish"; Filename: "{uninstallexe}"
