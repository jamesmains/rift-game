# Cross-compile Triffall.exe for Windows from Linux or macOS with llvm-mingw (github.com/mstorsjo/llvm-mingw):
#   cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=tools/mingw-toolchain.cmake -DLLVM_MINGW=/path/to/llvm-mingw
#   cmake --build build-win --target Triffall -j
# (add -DTRIFFALL_DEMO=ON for the demo). The exe needs only system DLLs and the Universal C Runtime (Windows 10+).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
if(NOT LLVM_MINGW)
  set(LLVM_MINGW $ENV{LLVM_MINGW})
endif()
set(CMAKE_C_COMPILER ${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER ${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER ${LLVM_MINGW}/bin/x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH ${LLVM_MINGW}/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
