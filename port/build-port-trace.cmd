@echo off
rem TRACING BUILD ONLY (lane TRACEPORT). Configures a SEPARATE build directory
rem build/port-trace with -DPORT_FN_TRACE=ON so the shipped build/port tree is
rem never poisoned by the cache variable, and builds walk_window alone. The guard
rem wave is deliberately NOT run here: the guards belong to the plain build, and
rem tailjump_guard in particular cannot read a /Gh build (every frame opens with
rem an E8 to _penter). Extra -D arguments are passed straight through, which is
rem how the pre-fix arm is selected: build-port-trace.cmd -DTITLEQUAD_GX_PLAIN=ON
setlocal
rem Visual Studio through vswhere (any edition; the old fixed Build Tools path is
rem the fallback): port\tools\vs_env.cmd sets up MSVC x86, cmake and NINJA_EXE.
call "%~dp0tools\vs_env.cmd"
if errorlevel 1 exit /b 1
rem THE DECOMP ROOT (run catchup1, Task 2.1). The decomp checkout this port
rem compiles: the directory holding port\ unless SM64DS_DECOMP_ROOT names another
rem one. Exported, so configure (DECOMP_ROOT in port/CMakeLists.txt) and every
rem tool below (port/tools/decomp_root.py) read the same checkout. The build
rem output stays in this tree's build\ either way.
rem port\decomp_root.cmd picks it: an explicit -DDECOMP_ROOT= among this
rem script's arguments, else SM64DS_DECOMP_ROOT, else the tree's decomp\ folder,
rem else the directory holding port\.
call "%~dp0decomp_root.cmd" %*
cmake -S "%~dp0." -B "%~dp0..\build\port-trace" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DPORT_FN_TRACE=ON %*
if errorlevel 1 exit /b 1
ninja -j10 -C "%~dp0..\build\port-trace" walk_window
if errorlevel 1 exit /b 1
echo TRACE_BUILD_OK
