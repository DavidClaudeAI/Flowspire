@echo off
REM Flowspire — build RAPIDE du coeur + tests (sans OBS).
REM Initialise l'env C++ de VS (cl, cmake, ninja fournis avec VS) puis
REM configure / compile / teste le dossier tests/ (boucle de test rapide).
setlocal enabledelayedexpansion

cd /d "%~dp0\.."

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo [erreur] vswhere introuvable : Visual Studio est-il installe ?
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" (
  echo [erreur] workload C++ "Desktop development with C++" introuvable.
  exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [erreur] vcvars64 a echoue & exit /b 1 )

cmake -S tests -B build_tests -G Ninja || exit /b 1
cmake --build build_tests || exit /b 1
ctest --test-dir build_tests --output-on-failure || exit /b 1

echo.
echo [OK] coeur + tests : build et tests termines.
