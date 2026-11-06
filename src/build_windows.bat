@echo off
REM Builds Triffall.exe on Windows with CMake + any C++17 compiler (Visual Studio 2019/2022, or MinGW-w64 / w64devkit).
REM raylib 5.5 is downloaded automatically from GitHub the first time (needs internet + git).
REM Usage:  build_windows.bat            (Visual Studio generator, if installed)
REM         build_windows.bat mingw      (MinGW Makefiles; needs g++ and mingw32-make on PATH)
REM         build_windows.bat [mingw] release   (release build WITHOUT the Editor)
REM         build_windows.bat [mingw] demo      (the demo: no editor, the first chapter only - see STEAM.md)
setlocal
set GEN=
set EDITOR=ON
set DEMO=OFF
set OUT=build
if /I "%1"=="mingw" set GEN=-G "MinGW Makefiles"
if /I "%1"=="release" set EDITOR=OFF
if /I "%2"=="release" set EDITOR=OFF
if /I "%1"=="demo" set DEMO=ON
if /I "%2"=="demo" set DEMO=ON
if "%EDITOR%"=="OFF" set OUT=build-release
if "%DEMO%"=="ON" set OUT=build-demo
if "%DEMO%"=="ON" set EDITOR=OFF
cmake -S . -B %OUT% %GEN% -DCMAKE_BUILD_TYPE=Release -DTRIFFALL_EDITOR=%EDITOR% -DTRIFFALL_DEMO=%DEMO% || goto :err
cmake --build %OUT% --config Release --target Triffall || goto :err
echo.
echo Done. Triffall.exe is in %OUT%\Release\ (Visual Studio) or %OUT%\ (MinGW), with the assets folder next to it.
if "%EDITOR%"=="OFF" echo This is the release build: no Editor icon, no editor code.
goto :eof
:err
echo Build failed. Make sure CMake and a C++ compiler are installed and on PATH.
exit /b 1
