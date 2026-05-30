@echo off
REM StreamDirector — installe le plugin construit dans OBS (per-machine).
REM Copie la DLL + les data depuis build_x64\rundir vers le dossier plugins
REM d'OBS (%ProgramData%\obs-studio\plugins\streamdirector). OBS doit etre FERME.
REM
REM Structure reelle du rundir (constatee) :
REM   build_x64\rundir\RelWithDebInfo\streamdirector.dll
REM   build_x64\rundir\RelWithDebInfo\streamdirector.pdb
REM   build_x64\rundir\RelWithDebInfo\streamdirector\locale\en-US.ini   (data)
setlocal

cd /d "%~dp0\.."

set "SRC=build_x64\rundir\RelWithDebInfo"
set "DEST=%ProgramData%\obs-studio\plugins\streamdirector"

if not exist "%SRC%\streamdirector.dll" (
  echo [erreur] DLL introuvable : lance d'abord scripts\build-plugin.bat
  exit /b 1
)

echo Installation vers "%DEST%" ...
if exist "%DEST%" rmdir /s /q "%DEST%"
mkdir "%DEST%\bin\64bit" 2>nul
mkdir "%DEST%\data" 2>nul

copy /y "%SRC%\streamdirector.dll" "%DEST%\bin\64bit\" >nul || ( echo [erreur] copie DLL & exit /b 1 )
if exist "%SRC%\streamdirector.pdb" copy /y "%SRC%\streamdirector.pdb" "%DEST%\bin\64bit\" >nul
xcopy /e /i /y "%SRC%\streamdirector\*" "%DEST%\data\" >nul || ( echo [erreur] copie data & exit /b 1 )

echo SD_INSTALL_OK
echo Plugin installe. Ouvre OBS : le dock "StreamDirector" doit etre disponible
echo via le menu Affichage ^> Docks.
