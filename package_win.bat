@echo off
REM package_win.bat – launches the MSYS2 UCRT64 packaging script.
REM Adjust MSYS2_ROOT if your MSYS2 is installed elsewhere.

set MSYS2_ROOT=C:\msys64

REM Build the MSYS2/Unix path from the batch file's own directory.
REM  %~dp0  e.g.  C:\Users\darek\stb\   →  /c/Users/darek/stb/
set "SCRIPT_WIN=%~dp0package_win.sh"
set "SCRIPT_UNIX=%SCRIPT_WIN:\=/%"
set "SCRIPT_UNIX=/%SCRIPT_UNIX:~0,1%%SCRIPT_UNIX:~2%"

"%MSYS2_ROOT%\usr\bin\bash.exe" --login -c "export MSYSTEM=UCRT64 && source /etc/profile && bash '%SCRIPT_UNIX%'"
