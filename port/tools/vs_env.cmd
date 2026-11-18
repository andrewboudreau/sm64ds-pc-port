@echo off
rem FIND VISUAL STUDIO. Called by port\build-port.cmd and port\build-port-trace.cmd
rem (call "%~dp0tools\vs_env.cmd"), port\tools\gen_disasm.cmd, and the build
rem scripts battery.py's shipcfg arm and portable_kit\package_kit.ps1 write:
rem     call "<this file>"
rem     if errorlevel 1 exit /b 1
rem Runs in the caller (no setlocal) and leaves behind:
rem   VSINSTALL   the Visual Studio installation it chose
rem   the 32-bit MSVC environment (its VC\Auxiliary\Build\vcvars32.bat)
rem   CMAKEBIN    that installation's bundled CMake folder
rem   NINJA_EXE   the ninja.exe to give CMake as CMAKE_MAKE_PROGRAM
rem   PATH        with cmake and ninja on it
rem The installation, first match wins:
rem   1. vswhere (it ships with every Visual Studio since 2017, in
rem      %ProgramFiles(x86)%\Microsoft Visual Studio\Installer): the newest
rem      Visual Studio 2022 with the x86 C++ tools, any edition (Build Tools,
rem      Community, Professional, Enterprise). 2022 is the toolset the port is
rem      built and tested with.
rem   2. vswhere: the newest installation of any version with those tools.
rem   3. %ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools, the fixed
rem      path the build scripts used before.
rem (The for /f commands start with "call" so cmd keeps the quotes around a
rem vswhere path that holds spaces and a quoted -version range.)
rem cmake and ninja come from the installation's "C++ CMake tools for Windows"
rem component; without it, from PATH.
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
set "VSINSTALL="
set "_vse_where=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%_vse_where%" goto vse_fixed
for /f "usebackq delims=" %%i in (`call "%_vse_where%" -latest -products * -version "[17.0,18.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if defined VSINSTALL goto vse_found
for /f "usebackq delims=" %%i in (`call "%_vse_where%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if defined VSINSTALL goto vse_found
:vse_fixed
set "VSINSTALL=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
:vse_found
set "_vse_where="
if exist "%VSINSTALL%\VC\Auxiliary\Build\vcvars32.bat" goto vse_vcvars
echo vs_env: no Visual Studio with the x86 C++ tools: vswhere found none, and there is
echo vs_env: no %VSINSTALL%\VC\Auxiliary\Build\vcvars32.bat either.
echo vs_env: install Visual Studio 2022 or its Build Tools with "Desktop development with C++".
exit /b 1
:vse_vcvars
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
set "CMAKEBIN=%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "NINJA_EXE=%CMAKEBIN%\Ninja\ninja.exe"
if not exist "%NINJA_EXE%" goto vse_path_ninja
set "PATH=%CMAKEBIN%\CMake\bin;%CMAKEBIN%\Ninja;%PATH%"
exit /b 0
:vse_path_ninja
set "NINJA_EXE="
for /f "delims=" %%n in ('where ninja 2^>nul') do if not defined NINJA_EXE set "NINJA_EXE=%%n"
if defined NINJA_EXE goto vse_cmake
echo vs_env: no ninja: neither %CMAKEBIN%\Ninja\ninja.exe (the "C++ CMake tools for Windows"
echo vs_env: component) nor a ninja on PATH.
exit /b 1
:vse_cmake
where cmake >nul 2>nul
if not errorlevel 1 exit /b 0
echo vs_env: no cmake: neither %CMAKEBIN%\CMake\bin\cmake.exe nor a cmake on PATH.
exit /b 1
