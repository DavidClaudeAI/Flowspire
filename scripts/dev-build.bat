@echo off
REM StreamDirector — configure, build et teste le coeur (Windows / MSVC).
REM Trouve Visual Studio via vswhere, initialise l'environnement C++ (cl, cmake,
REM ninja fournis avec VS), puis configure / compile / lance les tests.
setlocal enabledelayedexpansion

cd /d "%~dp0\.."

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo [erreur] vswhere introuvable : Visual Studio est-il installe ?
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`""%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath"`) do set "VSPATH=%%i"
if "%VSPATH%"=="" (
  echo [erreur] workload C++ "Desktop development with C++" introuvable.
  exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [erreur] vcvars64 a echoue & exit /b 1 )

cmake -S . -B build -G Ninja || exit /b 1
cmake --build build || exit /b 1
ctest --test-dir build --output-on-failure || exit /b 1

echo.
echo [OK] build + tests termines.
