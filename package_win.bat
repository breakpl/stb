@echo off
REM package_win.bat – launches the MSYS2 UCRT64 packaging script.
REM Adjust MSYS2_ROOT if your MSYS2 is installed elsewhere.

set MSYS2_ROOT=C:\msys64
set SCRIPT=%~dp0package_win.sh

REM Convert Windows path to MSYS2 path
for /f "tokens=*" %%i in ('"%MSYS2_ROOT%\usr\bin\cygpath.exe" -u "%SCRIPT%"') do set SCRIPT_UNIX=%%i

"%MSYS2_ROOT%\usr\bin\bash.exe" --login -i -c "export MSYSTEM=UCRT64 && source /etc/profile && bash '%SCRIPT_UNIX%'"
