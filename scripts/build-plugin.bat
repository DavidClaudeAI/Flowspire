@echo off
REM StreamDirector — build du PLUGIN OBS complet (lourd).
REM 1ere execution : telecharge libobs + Qt6 (~1 Go) et compile libobs depuis
REM les sources (long). Les fois suivantes sont rapides (deps en cache .deps/).
REM Initialise l'env C++ de VS puis configure + build via les presets template.
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

cmake --preset windows-x64 || exit /b 1
cmake --build --preset windows-x64 --config RelWithDebInfo || exit /b 1

echo.
echo [OK] plugin build. Binaire dans build_x64\rundir\RelWithDebInfo\
