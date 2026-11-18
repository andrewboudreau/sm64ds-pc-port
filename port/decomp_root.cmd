@echo off
rem THE DECOMP ROOT (run catchup1, Task 2.1). Called by build-port.cmd and
rem build-port-trace.cmd with their own arguments:  call decomp_root.cmd %*
rem Sets DECOMP_ROOT in the caller (no setlocal here) and exports it as
rem SM64DS_DECOMP_ROOT, so configure (DECOMP_ROOT in port/CMakeLists.txt) and
rem every tool (port/tools/decomp_root.py) read the same checkout. The build
rem output stays in the port tree's build\ either way. First match wins:
rem   1. -DDECOMP_ROOT=X (also -DDECOMP_ROOT:PATH=X, -D DECOMP_ROOT=X) among the
rem      arguments: what the caller asked CMake for is what every tool reads.
rem   2. SM64DS_DECOMP_ROOT from the environment.
rem   3. the port tree's decomp\ folder when it holds src\ and include\ (the
rem      public repo: port\ beside a decomp\ submodule).
rem   4. the directory holding port\ (the in-tree layout).
set "DECOMP_ROOT=%~dp0.." & rem decomp-root-check: definition
if exist "%~dp0..\decomp\src\" if exist "%~dp0..\decomp\include\" set "DECOMP_ROOT=%~dp0..\decomp" & rem decomp-root-check: definition
if defined SM64DS_DECOMP_ROOT set "DECOMP_ROOT=%SM64DS_DECOMP_ROOT%"
set "_pdr_from="
:pdr_args
if "%~1"=="" goto pdr_done
set "_pdr_a=%~1"
rem cmd splits an unquoted -DDECOMP_ROOT=X at the "=", so the value is the next
rem argument; a quoted "-DDECOMP_ROOT=X" arrives whole.
if /i "%_pdr_a%"=="-DDECOMP_ROOT" goto pdr_next
if /i "%_pdr_a%"=="-DDECOMP_ROOT:PATH" goto pdr_next
if /i "%_pdr_a%"=="-D" if /i "%~2"=="DECOMP_ROOT" (shift & goto pdr_next)
if /i "%~1"=="-D" if /i "%~2"=="DECOMP_ROOT:PATH" (shift & goto pdr_next)
if /i "%_pdr_a:~0,14%"=="-DDECOMP_ROOT=" (set "DECOMP_ROOT=%_pdr_a:~14%" & set "_pdr_from=-D")
if /i "%_pdr_a:~0,19%"=="-DDECOMP_ROOT:PATH=" (set "DECOMP_ROOT=%_pdr_a:~19%" & set "_pdr_from=-D")
shift
goto pdr_args
:pdr_next
set "DECOMP_ROOT=%~2"
set "_pdr_from=-D"
shift
shift
goto pdr_args
:pdr_done
if defined _pdr_from if defined SM64DS_DECOMP_ROOT if /i not "%SM64DS_DECOMP_ROOT%"=="%DECOMP_ROOT%" echo decomp_root: -DDECOMP_ROOT=%DECOMP_ROOT% given; it wins over SM64DS_DECOMP_ROOT=%SM64DS_DECOMP_ROOT%
set "SM64DS_DECOMP_ROOT=%DECOMP_ROOT%"
set "_pdr_a="
set "_pdr_from="
exit /b 0
