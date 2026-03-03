@echo off
REM Navigate to source directory
cd /d C:\Users\darek\stb

REM Remove old build directory
if exist build rmdir /s /q build

REM Create new build directory
mkdir build

REM Launch MSYS2 UCRT64 shell (tools are installed under ucrt64, not mingw64)
C:\msys64\usr\bin\bash.exe --login -i -c "export MSYSTEM=UCRT64 && source /etc/profile && cd /c/Users/darek/stb/build && cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja && bash /c/Users/darek/stb/copy_dlls.sh"
