@echo off
REM Flowspire — installe le plugin construit dans OBS (per-machine).
REM Copie la DLL + les data depuis build_x64\rundir vers le dossier plugins
REM d'OBS (%ProgramData%\obs-studio\plugins\flowspire). OBS doit etre FERME.
REM
REM Structure reelle du rundir (constatee) :
REM   build_x64\rundir\RelWithDebInfo\flowspire.dll
REM   build_x64\rundir\RelWithDebInfo\flowspire.pdb
REM   build_x64\rundir\RelWithDebInfo\flowspire\locale\en-US.ini   (data)
setlocal

cd /d "%~dp0\.."

set "SRC=build_x64\rundir\RelWithDebInfo"
set "DEST=%ProgramData%\obs-studio\plugins\flowspire"

if not exist "%SRC%\flowspire.dll" (
  echo [erreur] DLL introuvable : lance d'abord scripts\build-plugin.bat
  exit /b 1
)

REM OBS doit etre ferme : sinon la DLL est verrouillee et l'install se corrompt
REM (data effacee mais DLL non remplacee). On refuse plutot que de casser.
REM Chemins ABSOLUS pour tasklist/find : evite qu'un PATH (ex: git-bash) ne shadow
REM `find` par sa version Unix -> le garde-fou serait silencieusement contourne.
"%SystemRoot%\System32\tasklist.exe" /fi "imagename eq obs64.exe" /nh 2>nul | "%SystemRoot%\System32\find.exe" /i "obs64.exe" >nul
if not errorlevel 1 (
  echo [erreur] OBS est ouvert. Ferme OBS avant d'installer le plugin.
  exit /b 1
)

echo Installation vers "%DEST%" ...
if exist "%DEST%" rmdir /s /q "%DEST%"
mkdir "%DEST%\bin\64bit" 2>nul
mkdir "%DEST%\data" 2>nul

copy /y "%SRC%\flowspire.dll" "%DEST%\bin\64bit\" >nul || ( echo [erreur] copie DLL & exit /b 1 )
if exist "%SRC%\flowspire.pdb" copy /y "%SRC%\flowspire.pdb" "%DEST%\bin\64bit\" >nul
xcopy /e /i /y "%SRC%\flowspire\*" "%DEST%\data\" >nul || ( echo [erreur] copie data & exit /b 1 )

echo SD_INSTALL_OK
echo Plugin installe. Ouvre OBS : le dock "Flowspire" doit etre disponible
echo via le menu Affichage ^> Docks.
