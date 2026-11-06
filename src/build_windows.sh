#!/usr/bin/env bash
# Builds Triffall.exe with a MinGW-w64 compiler (cross-compiling from Linux/macOS, or from MSYS2 / w64devkit / Git Bash
# on Windows). Uses the prebuilt raylib 5.5 for MinGW: https://github.com/raysan5/raylib/releases (raylib-5.5_win64_mingw-w64.zip).
# (build_windows.bat does the same through CMake and downloads raylib itself.)
# Usage:  ./build_windows.sh                  full build (with the Editor)
#         ./build_windows.sh release          release build WITHOUT the Editor
#         ./build_windows.sh demo             the demo (no editor, first chapter only - see STEAM.md)
# Env:    CXX / CC   compilers (default x86_64-w64-mingw32-g++ / -gcc)
#         RAYLIB     folder of the unzipped raylib-5.5_win64_mingw-w64 (default ./third_party/raylib-5.5_win64_mingw-w64)
set -euo pipefail
cd "$(dirname "$0")"
CXX="${CXX:-x86_64-w64-mingw32-g++}"; CC="${CC:-x86_64-w64-mingw32-gcc}"
RAYLIB="${RAYLIB:-third_party/raylib-5.5_win64_mingw-w64}"
MODE="${1:-full}"; OUT=build-windows; DEFS=""
SRC="src/app.cpp src/core/*.cpp src/game/*.cpp src/desktop/*.cpp src/platform/steam_services.cpp"
case "$MODE" in
  release) OUT=build-windows-release; DEFS="-DTRIFFALL_NO_EDITOR=1" ;;
  demo)    OUT=build-windows-demo;    DEFS="-DTRIFFALL_NO_EDITOR=1 -DTRIFFALL_DEMO=1" ;;
  *)       SRC="$SRC src/editor/*.cpp" ;;
esac
[ -f "$RAYLIB/lib/libraylib.a" ] || { echo "raylib not found in $RAYLIB (set RAYLIB=...)"; exit 1; }
mkdir -p "$OUT/obj"
"$CC" -O2 -I"$RAYLIB/include" -c src/platform/rl_shim.c -o "$OUT/obj/rl_shim.o"
# shellcheck disable=SC2086
"$CXX" -std=c++17 -O2 -Ithird_party -I"$RAYLIB/include" $DEFS -Wno-unused-parameter -Wno-sign-compare \
  -o "$OUT/Triffall.exe" src/platform/main_raylib.cpp $SRC "$OUT/obj/rl_shim.o" \
  "$RAYLIB/lib/libraylib.a" -lopengl32 -lgdi32 -lwinmm -lshell32 -luser32 -mwindows -static
rm -rf "$OUT/assets"; cp -r assets "$OUT/assets"; rm -rf "$OUT/assets/new-art"
echo "Done: $OUT/Triffall.exe (assets copied next to it)"
