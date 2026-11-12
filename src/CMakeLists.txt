cmake_minimum_required(VERSION 3.16)
project(Triffall C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Browser build: configure with emcmake (Emscripten), e.g.
#   emcmake cmake -S . -B build-web -DTRIFFALL_DEMO=ON && cmake --build build-web
# It uses src/platform/main_web.cpp instead of main_raylib.cpp and leaves the Editor out. See README "Browser build".
if(EMSCRIPTEN)
  set(PLATFORM Web CACHE STRING "raylib platform" FORCE)
  set(TRIFFALL_EDITOR OFF CACHE BOOL "" FORCE)
endif()

# raylib: use an installed copy if present, otherwise fetch 5.5 from GitHub.
find_package(raylib 5.0 QUIET)
if(NOT raylib_FOUND)
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.5)
  FetchContent_MakeAvailable(raylib)
endif()

# Release builds can leave the Editor out entirely: cmake -DTRIFFALL_EDITOR=OFF (the desktop icon and start-menu entry disappear too)
option(TRIFFALL_EDITOR "Compile the content Editor into the game" ON)
# The demo (1.0): cmake -DTRIFFALL_DEMO=ON - no editor, the base campaign limited to assets/data/demo.txt, a Wishlist icon
option(TRIFFALL_DEMO "Build the demo (implies TRIFFALL_EDITOR=OFF)" OFF)
if(TRIFFALL_DEMO)
  set(TRIFFALL_EDITOR OFF CACHE BOOL "" FORCE)
  add_compile_definitions(TRIFFALL_DEMO=1)
endif()
# Debug builds: cmake -DTRIFFALL_DEV_CONSOLE=ON (build.ps1 -Dev) turns on the dev console (~). A Debug configuration
# turns it on too; Release builds of every variant leave it out. The headless test runner always has it.
option(TRIFFALL_DEV_CONSOLE "Debug build: the developer console (~) can be opened" OFF)
if(TRIFFALL_DEV_CONSOLE)
  add_compile_definitions(TRIFFALL_DEV_CONSOLE=1)
else()
  add_compile_definitions($<$<CONFIG:Debug>:TRIFFALL_DEV_CONSOLE=1>)
endif()
if(TRIFFALL_EDITOR)
  file(GLOB GAME_SRC src/app.cpp src/core/*.cpp src/game/*.cpp src/desktop/*.cpp src/editor/*.cpp src/platform/steam_services.cpp)
else()
  file(GLOB GAME_SRC src/app.cpp src/core/*.cpp src/game/*.cpp src/desktop/*.cpp src/platform/steam_services.cpp)
  add_compile_definitions(TRIFFALL_NO_EDITOR=1)
endif()

# Steamworks (optional): cmake -DTRIFFALL_STEAM=ON -DSTEAM_SDK=/path/to/sdk -DTRIFFALL_STEAM_APPID=480
option(TRIFFALL_STEAM "Build with Steamworks achievements/overlay support" OFF)
set(STEAM_SDK "" CACHE PATH "Path to the extracted Steamworks SDK (the folder containing public/ and redistributable_bin/)")
set(TRIFFALL_STEAM_APPID "480" CACHE STRING "Steam App ID (480 = Spacewar test app)")

if(EMSCRIPTEN)
  add_executable(Triffall src/platform/main_web.cpp src/platform/rl_shim.c ${GAME_SRC})
  set_target_properties(Triffall PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  target_link_options(Triffall PRIVATE -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sSTACK_SIZE=4194304
    -sFORCE_FILESYSTEM=1 -lidbfs.js -sEXPORTED_FUNCTIONS=_main,_web_start,_web_saves_persist_again -sEXPORTED_RUNTIME_METHODS=ccall,HEAPF32,UTF8ToString
    "--preload-file=${CMAKE_SOURCE_DIR}/assets@/assets" "--exclude-file=${CMAKE_SOURCE_DIR}/assets/new-art"
    "--shell-file=${CMAKE_SOURCE_DIR}/web/shell.html")
  set_property(TARGET Triffall APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/web/shell.html)
else()
add_executable(Triffall WIN32 src/platform/main_raylib.cpp src/platform/rl_shim.c ${GAME_SRC})
endif()
target_include_directories(Triffall PRIVATE third_party)
target_link_libraries(Triffall PRIVATE raylib)
if(TRIFFALL_STEAM)
  target_compile_definitions(Triffall PRIVATE TRIFFALL_STEAM=1 TRIFFALL_STEAM_APPID=${TRIFFALL_STEAM_APPID})
  target_include_directories(Triffall PRIVATE ${STEAM_SDK}/public)
  if(WIN32)
    target_link_libraries(Triffall PRIVATE ${STEAM_SDK}/redistributable_bin/win64/steam_api64.lib)
    add_custom_command(TARGET Triffall POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${STEAM_SDK}/redistributable_bin/win64/steam_api64.dll $<TARGET_FILE_DIR:Triffall>/)
    add_custom_command(TARGET Triffall POST_BUILD COMMAND ${CMAKE_COMMAND} -E echo ${TRIFFALL_STEAM_APPID} > $<TARGET_FILE_DIR:Triffall>/steam_appid.txt)
  elseif(APPLE)
    target_link_libraries(Triffall PRIVATE ${STEAM_SDK}/redistributable_bin/osx/libsteam_api.dylib)
  else()
    target_link_libraries(Triffall PRIVATE ${STEAM_SDK}/redistributable_bin/linux64/libsteam_api.so)
  endif()
endif()
if(MSVC)
  target_compile_options(Triffall PRIVATE /W3 /utf-8)
  # keep a console-less app but still allow main()
  set_target_properties(Triffall PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
else()
  target_compile_options(Triffall PRIVATE -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-misleading-indentation)
endif()
if(WIN32 AND NOT MSVC)
  target_link_options(Triffall PRIVATE -static)
endif()

if(NOT EMSCRIPTEN)   # (the browser build packs its assets into index.data instead)
# headless test driver (no raylib) - handy for scripted testing
add_executable(triffall_headless src/platform/main_headless.cpp ${GAME_SRC})
target_include_directories(triffall_headless PRIVATE third_party)
target_compile_definitions(triffall_headless PRIVATE TRIFFALL_DEV_CONSOLE=1)

# copy assets next to the executable
add_custom_command(TARGET Triffall POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Triffall>/assets)
endif()
