; Flowspire - installeur Windows (Inno Setup).
;
; Empaquete le plugin dans le dossier OBS Studio detecte, au layout "bundled" d'OBS :
;   {OBS}\obs-plugins\64bit\flowspire.dll
;   {OBS}\data\obs-plugins\flowspire\...   (locales, etc.)
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
  ; Nom du module OBS = nom EXACT de la DLL (minuscule). OBS cherche les donnees dans
  ; data\obs-plugins\<MyModuleName>\ -> doit correspondre, sinon locales introuvables.
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
; Dossier OBS : lu dans le registre (cle posee par l'installeur officiel OBS),
; repli sur Program Files\obs-studio. La page de selection reste visible (Parcourir).
DefaultDirName={reg:HKLM\SOFTWARE\OBS Studio,|{commonpf}\obs-studio}
DisableProgramGroupPage=yes
; Ecrit dans Program Files -> droits administrateur (demande une fois a l'UAC).
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename={#MyModuleName}-{#MyAppVersion}-windows-x64
UninstallDisplayName={#MyAppName} (plugin OBS)
UninstallDisplayIcon={app}\bin\64bit\obs64.exe
Compression=lzma2
SolidCompression=yes

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; La DLL -> dossier des modules 64 bits d'OBS.
Source: "{#SourceDir}\bin\64bit\{#MyModuleName}.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
; Les donnees (locales...) -> data\obs-plugins\<module> (nom EXACT de la DLL, minuscule).
Source: "{#SourceDir}\data\*"; DestDir: "{app}\data\obs-plugins\{#MyModuleName}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Code]
{ Garde-fou : si le dossier choisi ne contient pas OBS (pas d'obs64.exe), on previent
  l'utilisateur (mauvais dossier) plutot que d'installer dans le vide. }
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssInstall) and (not FileExists(ExpandConstant('{app}\bin\64bit\obs64.exe'))) then
  begin
    if MsgBox('OBS Studio ne semble pas installe dans :' + #13#10
              + ExpandConstant('{app}') + #13#10 + #13#10
              + 'Installer le plugin ici quand meme ?', mbConfirmation, MB_YESNO) = IDNO then
      Abort;
  end;
end;
