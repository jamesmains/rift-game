# Linux/macOS developer Makefile. `make headless` needs no dependencies.
# `make game` expects raylib 5.x installed (pkg-config raylib).
CXX ?= g++
CC ?= gcc
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-misleading-indentation -Ithird_party
GAME_SRC := src/app.cpp src/platform/steam_services.cpp $(wildcard src/core/*.cpp src/game/*.cpp src/desktop/*.cpp src/editor/*.cpp)

headless: $(GAME_SRC) src/platform/main_headless.cpp
	$(CXX) $(CXXFLAGS) -o triffall_headless src/platform/main_headless.cpp $(GAME_SRC)

game: $(GAME_SRC) src/platform/main_raylib.cpp src/platform/rl_shim.c
	$(CC) -O2 $(shell pkg-config --cflags raylib) -c src/platform/rl_shim.c -o rl_shim.o
	$(CXX) $(CXXFLAGS) -o triffall src/platform/main_raylib.cpp $(GAME_SRC) rl_shim.o $(shell pkg-config --libs raylib)

tools: tools/kindview.cpp tools/owview.cpp tools/mapcheck.cpp
	$(CXX) $(CXXFLAGS) -o mapcheck tools/mapcheck.cpp src/core/gfx.cpp src/game/world.cpp src/game/worldgen.cpp src/game/data.cpp
	$(CXX) $(CXXFLAGS) -o kindview tools/kindview.cpp src/core/gfx.cpp src/game/world.cpp src/game/data.cpp
	$(CXX) $(CXXFLAGS) -o owview tools/owview.cpp src/core/gfx.cpp src/game/world.cpp src/game/worldgen.cpp src/game/data.cpp

clean:
	rm -f triffall triffall_headless rl_shim.o kindview owview mapcheck

.PHONY: headless game tools clean
