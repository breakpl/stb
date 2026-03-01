@echo off
REM package_win.bat – launches the MSYS2 MinGW64 packaging script.
REM Adjust MSYS2_ROOT if your MSYS2 is installed elsewhere.

set MSYS2_ROOT=C:\msys64
set SCRIPT=%~dp0package_win.sh

REM Convert Windows path to MSYS2 path
for /f "tokens=*" %%i in ('"%MSYS2_ROOT%\usr\bin\cygpath.exe" -u "%SCRIPT%"') do set SCRIPT_UNIX=%%i

"%MSYS2_ROOT%\msys2_shell.cmd" -mingw64 -no-start -c "bash '%SCRIPT_UNIX%'"
