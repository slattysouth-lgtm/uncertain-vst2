; ============================================================================
;  BLOODMONEY Master - Installeur Windows partageable (Inno Setup)
;
;  Produit "BLOODMONEY-Master-Setup.exe" : un installeur que TU compiles une
;  seule fois, puis que tu partages librement. Le destinataire double-clique,
;  le plugin VST3 se pose au bon endroit, et il n'a plus qu'a ouvrir son DAW.
;  Aucun outil requis cote destinataire.
;
;  POUR FABRIQUER LE .EXE (une fois) :
;    1. Avoir compile le plugin (build.bat le fait).
;    2. Installer Inno Setup 6.3+ : https://jrsoftware.org/isdl.php
;    3. Ouvrir ce fichier -> Compile (F9). Le .exe sort dans .\Installer\
; ============================================================================

#define MyName      "BLOODMONEY Master"
#define MyVersion   "1.0.0"
#define MyPublisher "BLOODMONEY"
#define BuildDir    "build\BLOODMONEYMaster_artefacts\Release"

[Setup]
AppId={{B10D0E1A-2C3D-4E5F-8A9B-0C1D2E3F4A5B}
AppName={#MyName}
AppVersion={#MyVersion}
AppPublisher={#MyPublisher}
UninstallDisplayName={#MyName}
DefaultDirName={autopf}\BLOODMONEY
DefaultGroupName=BLOODMONEY
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=Installer
OutputBaseFilename=BLOODMONEY-Master-Setup
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
; VST3 = dossier-bundle : on copie tout son contenu
Source: "{#BuildDir}\VST3\BLOODMONEY Master.vst3\*"; \
    DestDir: "{commoncf}\VST3\BLOODMONEY Master.vst3"; \
    Flags: recursesubdirs createallsubdirs ignoreversion; \
    Components: vst3
; Application autonome (optionnelle)
Source: "{#BuildDir}\Standalone\BLOODMONEY Master.exe"; \
    DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{group}\BLOODMONEY Master";              Filename: "{app}\BLOODMONEY Master.exe"; Components: standalone
Name: "{group}\Desinstaller BLOODMONEY Master"; Filename: "{uninstallexe}"
