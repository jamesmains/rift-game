# Triffall

Old-school open-world RPG with X-COM / Fallout-style turn-based tactical combat, wrapped in a fake Windows 98 desktop
that is the main menu (Minesweeper, Solitaire, lore files, Start menu, a built-in content Editor). Windows desktop game,
aimed at Steam (and itch.io). Early builds (0.1-0.7) were called **Riftfall**; the old folder still has that name.

## Stack
- C++17 + raylib 5.5, software renderer into a 640x480 canvas. Windows build: MSVC (Visual Studio 18 Professional is
  installed) through CMake. raylib is fetched by CMake FetchContent into `build\cmake\_deps` on the first build.
- `src\` is the original project folder, layout unchanged (its own `CMakeLists.txt`, `build_windows.bat`,
  `build_windows.sh` (MinGW, no MinGW here), `Makefile` (Linux)). Its `README.md` is the player/modder manual and the
  per-version changelog; `STEAM.md`, `RELEASE_CHECKLIST.md`, `BALANCE.md` are the author's notes.
- Optional CMake switches: `TRIFFALL_EDITOR` (default ON), `TRIFFALL_DEMO`, `TRIFFALL_STEAM` (+ `STEAM_SDK`; no
  Steamworks SDK on this machine). Browser build needs Emscripten (not installed here).
- `.gitattributes` keeps every file LF as committed (ASCII maps and data files).

## Build and run
- `.\build.ps1` -> `build\game\Triffall.exe` + `assets\` (without `assets\new-art`) + `README.txt`. Builds the
  full game with the Editor, plus `triffall_headless.exe` in `build\cmake\Release\`. `build\game\saves\` is kept.
- `.\build.ps1 -NoEditor` (`build\cmake-noeditor` -> `build\game-noeditor`) and `.\build.ps1 -Demo`
  (`build\cmake-demo` -> `build\game-demo`). Each variant has its own output folder and `saves\`.
- Gotcha (fixed 1.0.12): PowerShell 5.1 passes an unquoted `-DFOO=$var` literally, so the cmake `-D` args must be
  quoted. Before, every variant got `TRIFFALL_DEMO=$demoFlag` (truthy) and built as the demo with no Editor (1.0.10
  import and 1.0.11). Check with `grep "^TRIFFALL_" build\cmake*\CMakeCache.txt`.
- Not decided yet which variant goes to Steam vs itch.io: don't label them that way.
- `-Package` (with any variant) also writes a clean hand-out copy without `saves\` to `build\dist\<full|noeditor|demo>`.
- `.\build.ps1 -Release -Notes "..."` -> `releases\<version>\`, `releases\triffall-v<version>-windows.zip`, and
  `releases\latest.json` (only the full build writes latest.json; -NoEditor/-Demo releases get a suffix).
- `.\run.ps1 [-NoEditor|-Demo]` builds that variant if sources are newer than its exe, then starts it. No Read-Host
  (Idea Jotter runs it headless). With `IDEAJOTTER_RUN_DATA` set (fresh run) it starts the game with
  `--saves <that folder>\saves` (the game's only per-user data: saves, settings.ini, desktop.ini, crash/session files).
- `builds.json` lists the three variants (Full game, No Editor, Demo) for Idea Jotter's Project tab: run with
  `run.ps1 [-NoEditor|-Demo]`, package with `build.ps1 [...] -Package`, zip `build\dist\<variant>`. A Steam build
  (`TRIFFALL_STEAM`) isn't listed: no Steamworks SDK here. Add it to builds.json once it can be built.
- A first CMake configure once failed for -NoEditor/-Demo and worked on rerun (likely the raylib download); just rerun.
- `.\install.ps1` copies the latest release to `%LOCALAPPDATA%\Programs\Triffall` (keeping its `saves\`) with a
  Start Menu shortcut. No in-game self-update yet (follow-up): re-run install.ps1 to update.
- The game reads `assets\` and writes `saves\` next to its exe (falls back to the working folder); `--saves <dir>`
  (in `src\src\platform\main_raylib.cpp`) overrides the saves folder.
- Headless tests (no window): from `src\`, `..\build\cmake\Release\triffall_headless.exe tests\<name>.txt <outdir>\`
  (outdir must exist; see `src\tests\README.md`). At 1.1.1 boot_and_setup, tutorial, sprites_1010,
  side_quests_1_0, story_choices, fight_to_victory, main_quest (to the ending, ~55 min) and the editor_* tests pass.
  Run tests that save files (Editor bulk edits, templates) from a copy of `src\assets` in another folder (the
  headless exe reads `assets\` from its working folder). `desktop_persist` expects
  an `editor` icon at 0,0 (already so in the 1.0.10 source).
- Don't pipe build.ps1 into `Select-Object -First N`: it stops reading and the build hangs. Redirect to a file.

## Version source
`TRIFFALL_VERSION` in `src\src\core\version.h` (build.ps1 reads it). `TRIFFALL_SAVE_FORMAT` next to it: bump it when
the save layout changes and add the step to `Game::migrateSave` (`src\src\game\saves.cpp`). Imported at 1.0.10.

## Tiles (1.0.13)
- Every map cell is a tile kind. `src\assets\data\tilekinds.txt` is the source of truth (sectioned, commented);
  `src\tools\mktilekinds.py` (run by build.ps1) compiles it into `src\src\game\tilekinds_builtin.h`, the built-ins.
  `tiles.h` keeps only pictures the code draws itself (chest, sign, altar, doors, brazier...).
- 1.0.13 redefined every kind from the Tilebook description of the sheet plus a cell-by-cell look at it (no `tiles\`
  folder exists for this project). Sheet facts: rows 5-7 are dark floor patterns (drawn over the kind's bg colour);
  row 11 = trail pieces, row 12 = river pieces, row 41 = a 3x3 lake/shore set + fill (9,41), row 3 cols 52-68 = a full
  asphalt road set, 10,40/11,40 = N-S/E-W bridge; rows 33-34 = overworld pictures; row 0 cols 0-10 are decorative
  blocks that tile badly (walls use rows 2-4 instead).
- Joined pieces: `edges=` (16 tiles by neighbour mask N1 E2 S4 W8), `join=`/`link=` groups, `under=1` (bg taken from
  the neighbouring ground; also on every furniture/prop kind so it stands on whatever floor is around it),
  `recolor=1`. Resolved at draw time in `tileKindLook` (world.h) via `Map::look`, and in the Editor's drawMapCell.
- Map remakes kept every map's size and every NPC/chest/exit/trigger position (quests and tests use coordinates).
  Dungeon walls not touching open floor are `rock`. Use `path` for 1-wide trails, `street`/`boardwalk` for wide ones
  (a 2-wide `path` draws as a lattice), `floor_water` for ponds.
- 1.0.15 look (owner: outlines aren't the answer, use the black background the sheet was drawn for): every land kind
  has `bg=0,0,0`; only water/lava-type kinds (water, deepwater, shallows, floor_water, ford, mire, floor_lava, bridge,
  boat, lilypad) keep a coloured bg. Ground patterns use `recolor=1` with a dim tint (luminance ~50-110) so characters
  stand out but floors still show; a mostly-empty ground list looks like void on black, so give walkable kinds a
  pattern on most cells. Map `bg=` (off-map fill) is black too. No outlines: `Tileset::drawBacking` draws a sprite's
  interior black pixels (not flood-reachable from its cell edge) before the sprite; used for party, NPCs, map
  enemies, chests, signs, drops. `drawLit` still lifts very dark sprites (the grey warrior).
- Trails: `shift=4` on `path`/`road`. In the game (`Game::drawWorld`) shifted pieces are drawn after all ground and
  before props, spilling `shift` px into the cell below (not into an opaque cell), so feet land inside the trail. The
  Editor keeps them clipped to their cell (`drawGroundPiece(..., spill=false)`). 2x2 blocks of recoloured trails fill
  solid. 1.0.16: `bridge` has `shift=3` (the E-W piece then ends where the shifted path does, under the feet; 3 keeps
  the N-S piece's 3-px plank pattern seamless). The top-of-cell fill for shifted pieces uses `MASK_RUNS_NORTH` (bit 16
  that `tileKindLook` adds to mask: the north neighbour shares the kind's join/link groups and isn't water), not
  mask bit 1, because a bridge's link= is water. `tileKindsText` (Editor save) now writes `shift=`.
- Headless screenshots: wait ~120 frames after `teleport` before `shot`, or the fade-in darkens the picture.
- Source files are mixed: some .cpp/.h are CRLF (game.cpp, editor.cpp, world.*), the rest LF. `* -text` keeps them as
  committed, so keep each file's line endings when editing with scripts.
- Known data warnings (Editor Check all, older than 1.0.13): ashfall trigger at 40,25 and marsh chest at 32,34 stand
  on blocked tiles.

## Object templates and resource nodes (1.1.0)
- `objects.txt` ([template] id, type, keys) and `resources.txt` ([resource] id, item, count, tool, respawn, name,
  sprite, color, spent, spentsprite, kind, styles) are read by `Game::loadSystems` into globals in world.cpp
  (`setObjectTemplates`, `setResourceDefs`); `loadMapText` applies them: a map object with `template=` gets the
  template's type + keys it lacks; a `[gather]` gets keys it lacks from `node=` (else the first type with its item).
  Generated dungeons scatter resource types by `styles=style[:weight[:count]]` (file order = the old built-in draw, so
  generated dungeons came out the same); the built-in table is only a fallback when no type has styles.
- A node with `sprite=` replaces its cell's picture in `Game::drawWorld` (skips ground + prop); without one the cell's
  prop/ground is recoloured as before. All base-game `[gather]` objects were migrated to `node=` at 1.1.0 (only keys
  equal to the type's were dropped).
- Editor: `editor_objects.cpp` (Objects tab 22 = gens[15], Resources tab 23 = gens[16]); `Editor::resolvedObj` mirrors
  the game's resolution; `mapScan` holds `objs` (resolved, used by the indexes/checks) and `raw` (as written).
  Bulk edits (`bulkObjects`) save every changed map; the open map is only saved if it had no unsaved changes.
- 1.1.0 fix: the map object panel's rect is computed before the map handles the mouse (`overPanel`), so clicks in it
  no longer reach the map. The tab bar now wraps into as many rows as needed (`gEdTop` = 28/50/72; 24 tabs fit in two).
- Tests: `tests/editor_objects_1_1.txt` (panel click, both tabs), `tests/resource_nodes_1_1.txt` (harvest a linked node).

## 1.1.1 changes worth knowing
- Object types: `objects.txt` `[type]` sections (id = an object type) are defaults for every object of that type:
  placed object > template > type > built-in. Empty values = built-in (skipped in the type; an empty value on the
  object/template is filled from the type). Game: `setObjectTemplates` reads both, `applyObjectTemplate` fills template
  then type; look/use keys parsed in `loadMapText` (world.cpp): savepoint sprite/color/sparkle/sparklecolor/text/sound/
  onuse, chest sprite/opensprite/color/sound/onuse, sign sprite/color/title/sound/onuse, door sprite/opensprite/sound/
  text/onuse, lever sprite/onsprite/sound/onuse. Objects made by code (worldgen chests/save points, console) call
  `applyTypeLook`. Drawn in `Game::drawWorld`, used in `Game::interactAt` / `pullLever`. Editor: gens[15] altSection
  "type" (list shows "every X"), `typeSec`, `resolvedObj` mirrors the order, `drawObjectLook` + `drawObjectTypeInfo`
  (preview panel above Placed objects), `edObjLookKeys`. Guide topic `objtype`. Saving objects.txt from the Editor
  drops its comments (all GenericEditor files do).
- Walkable ground kinds have no empty entries in `ground=` any more (an empty cell on black reads as unlit void; the
  owner reported it as a sight bug). Only grass/snow tufts, ford and crops keep them. The Editor map now picks each
  cell's ground piece by a cell hash, like the game picks one at random (it used to show ground[0] everywhere).
- Gem veins: resources.txt gem_rough is `sprite=14,5` (rock pile) in blue on `kind=rocks`, never the crystal picture.
- Boot: one screen (`Desktop::drawBoot`, title over the BIOS lines, BOOT_SECONDS 3.2); drawLogoSplash is gone.
- Font: always `Font::STYLE_CLEAR` (app.cpp; settings ignore font=). STYLE_CLASSIC only draws the TRIFFALL logo.
- Dialogue answers are "armed" only DLG_ARM_SECONDS (0.35 s) after the text is complete and all confirm keys are up
  (`dlgOptArmed`). Test scripts that press ENTER on an answer need ~25 frames after the answers appear; the bot sets
  `dlgOptArmed` itself. The skip press never picks an answer.
- Desktop icons ignore the pointer over a window or the taskbar (`hoverIcon` in `Desktop::update`).
- Gotcha: a hung `cl.exe` from an earlier build held `game.h` open (writes failed with EPERM) until it was ended.

## Where things live
- Code: `src\src\{core,game,desktop,editor,platform}`. Data: `src\assets\data\` (plain text: items, creatures,
  quests, dialogue, ASCII maps...). Campaigns: `src\assets\campaigns\` (tutorial only; the
  Gullwatch sample campaign was removed at 1.1.1 by the owner).
- Art: `src\assets\tiles.png` is the Urizen 1-bit sheet by vurmux (CC0), `icons.png` the desktop icons
  (Cute Desktop Icons by Mayke-Arth). Sound and music are generated by `src\tools\mksounds.py`.
- `assets\tilesheets\` - the original Urizen sheets (`urizen_onebit_tileset__v2d0.png` = same file as tiles.png;
  `urizen__2bit__free.png`). Tilebook's saved description of the 1-bit sheet
  (`C:\Apps\Tilebook\assets\saved-work\riftfall-urizen-onebit\tilebook.json`) was used to fix the sprites in 1.0.10.
- `assets\packs\` (zipped, not in git): Mini Medieval packs by V3X3D (collected for the abandoned 1.0.11 art
  overhaul) and the Cute Desktop Icons pack.
- `tools\RiftfallDataStudio.py` - the owner's Tkinter editor/backup tool for the `.txt` data files, written for 0.7;
  now defaults to `src\assets\data` and `data-backups\` (git-ignored). `python tools\RiftfallDataStudio.py`.
- `press-kit\` - covers, banners, 0.9.0 itch/Steam art, screenshots (16x9 and 4x3). `docs\` - "Road to Steam" plan.
- `src\tools\steam\*.vdf` - SteamPipe upload scripts; `src\tools\promo_kit.py` - promo image generator.

## History (from the old folder)
- 0.1-0.7 (2026-10-14..16, "Riftfall"): Windows builds; 0.7 had the 5 classes, companions, overworld, 9 generated
  dungeons, 6 towns, tactical combat, Win98 desktop, 20 achievements.
- 0.8-0.8.9 (2026-10-18..20): renamed Triffall; editor (then "RiftEdit"), campaigns, darkness, world objects;
  0.8.9 controller and UI pass.
- 0.9.0 (10-20): reputation, quest failure/branches, summons, smarter AI, first-hour tips, dialogue graph editor.
- 1.0.0-1.0.6 (10-22..23): "a game you can finish" - ending/epilogue, three story choices, 20 side quests, versioned
  saves, crash recovery, accessibility, demo build. A 1.0.0 Windows demo build was made.
- 1.0.7 (playtest fixes, Spleen font), 1.0.8 (Tutorial campaign), 1.0.9 (desktop icons/wallpaper saved) - 10-24.
- 1.0.10 (10-30): every sprite checked against the Tilebook description of the Urizen sheet (~120 fixes).
- A "1.0.11 every area rebuilt" (Mini Medieval art, animation, roofs) exists only as `triffall_src_1.0.11 DO NOT
  USE.zip` (dated before 1.0.10); the owner marked it not to use. Not imported. The real 1.0.11 (2026-11-11) is
  unrelated: it only added the build variants and builds.json (and, by the bug above, shipped the demo build).

Unfinished (from RELEASE_CHECKLIST / STEAM.md): human playtests, a clean-PC Windows test, Steam Deck test, the Steam
store page and Steamworks setup, listening to the generated music by ear.
