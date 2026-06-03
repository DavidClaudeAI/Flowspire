; Flowspire - installeur Windows (Inno Setup).
;
; Installe le plugin PAR UTILISATEUR dans le dossier plugins d'OBS (sans droits admin) :
;   %AppData%\obs-studio\plugins\flowspire\bin\64bit\flowspire.dll
;   %AppData%\obs-studio\plugins\flowspire\data\...   (locales, etc.)
;
; Version + dossiers source/sortie sont passes par Package-Windows.ps1 via iscc /D... .
; Les valeurs #ifndef ci-dessous ne servent qu'a un test local manuel.

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0-dev"
#endif
#ifndef SourceDir
  ; arbre produit par `cmake --install ... --prefix release/<config>` (voir Build-Windows.ps1)
  #define SourceDir "..\..\..\release\RelWithDebInfo\flowspire"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\..\release"
#endif
#ifndef MyModuleName
  ; Nom du module OBS = nom EXACT de la DLL (minuscule). Sert au nom de la DLL et du
  ; dossier d'install per-user (...\obs-studio\plugins\<MyModuleName>\).
  #define MyModuleName "flowspire"
#endif

#define MyAppName "Flowspire"
#define MyAppPublisher "Flowspire"
#define MyAppURL "https://github.com/DavidClaudeAI/Flowspire"

[Setup]
; AppId stable (NE PAS changer entre versions : sert a detecter/mettre a jour l'install).
AppId={{7A3E9B12-4C8D-4F6A-B1E2-9D0C3F5A6B7E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
WizardStyle=modern
; Install PAR UTILISATEUR dans le dossier plugins d'OBS : pas de droits admin, et pas
; d'avertissement "dossier existe deja" (flowspire est un sous-dossier dedie et neuf).
DefaultDirName={userappdata}\obs-studio\plugins\{#MyModuleName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename={#MyModuleName}-{#MyAppVersion}-windows-x64
UninstallDisplayName={#MyAppName} (plugin OBS)
UninstallDisplayIcon={app}\bin\64bit\{#MyModuleName}.dll
Compression=lzma2
SolidCompression=yes

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Layout "par utilisateur" : on depose le plugin tel quel ({app} = ...\plugins\flowspire),
; donc OBS charge ...\plugins\flowspire\bin\64bit\flowspire.dll + ...\plugins\flowspire\data\ .
Source: "{#SourceDir}\bin\64bit\{#MyModuleName}.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#SourceDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs
