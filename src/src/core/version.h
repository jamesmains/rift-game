// The game's version and the save-file format it writes. Bump SAVE_FORMAT whenever the save layout changes and add the
// upgrade step to Game::migrateSave (src/game/saves.cpp); item/flag/map renames go in assets/data/migrations.txt.
#pragma once
#define TRIFFALL_VERSION "1.0.15"
#define TRIFFALL_SAVE_FORMAT 3
#define TRIFFALL_VERSION_LABEL "v" TRIFFALL_VERSION
