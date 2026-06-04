@echo off
REM Flowspire — installe le plugin construit dans OBS (per-machine).
REM Copie la DLL + les data (depuis build_x64\rundir) ET le backend TLS Qt (depuis .deps)
REM vers le dossier plugins d'OBS (%ProgramData%\obs-studio\plugins\flowspire). OBS doit etre
REM FERME. Le backend TLS rend l'install locale FIDELE a la release (verif de MAJ HTTPS OK).
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

REM Backend TLS Qt (Schannel) : la vraie install (regle install() de CMake) le place sous
REM bin\64bit\tls pour que la verification de MAJ HTTPS marche. Le rundir ne le contient PAS
REM -> on le prend dans les deps Qt (meme fichier que $<TARGET_FILE:Qt6::QSchannelBackendPlugin>)
REM pour une install locale FIDELE a la release (sinon : "No functional TLS backend").
set "QT_TLS="
for /d %%d in (".deps\obs-deps-qt6-*") do (
  if exist "%%d\plugins\tls\qschannelbackend.dll" set "QT_TLS=%%d\plugins\tls\qschannelbackend.dll"
)
if defined QT_TLS (
  mkdir "%DEST%\bin\64bit\tls" 2>nul
  copy /y "%QT_TLS%" "%DEST%\bin\64bit\tls\" >nul && echo Backend TLS Qt copie - verif MAJ testable en local.
) else (
  echo [avertissement] backend TLS Qt introuvable dans .deps - verif MAJ non testable en local, release OK.
)

echo SD_INSTALL_OK
echo Plugin installe. Ouvre OBS : le dock "Flowspire" doit etre disponible
echo via le menu Affichage ^> Docks.
