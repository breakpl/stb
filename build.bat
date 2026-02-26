@echo off
REM Navigate to source directory
cd /d C:\Users\darek\Documents\stb

REM Remove old build directory
if exist build rmdir /s /q build

REM Create new build directory
mkdir build
cd build

REM Launch MSYS2 with proper environment and run cmake
C:\msys64\msys2_shell.cmd -mingw64 -no-start -c "cd /c/Users/darek/Documents/stb/build && cmake .. -G 'MinGW Makefiles' && cmake --build . --config Release"
