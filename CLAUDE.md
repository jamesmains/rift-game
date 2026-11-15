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
  Steamworks SDK on this machine). Browser build needs Emscripten (not installed here; `build-web.ps1`, see 1.3.0).
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
  Start Menu shortcut. No in-game self-update yet (follow-up): re-run install.ps1 to update. It keeps Editor edits
  made in the installed copy (see "Editor edits" below).
- The game reads `assets\` and writes `saves\` next to its exe (falls back to the working folder); `--saves <dir>`
  (in `src\src\platform\main_raylib.cpp`) overrides the saves folder.
- Headless tests (no window): from `src\`, `..\build\cmake\Release\triffall_headless.exe tests\<name>.txt <outdir>\`
  (outdir must exist; see `src\tests\README.md`). 1.3.0: all 94 tests but main_quest and fuzz_all_maps pass (8 at a
  time from a copy of assets without campaigns\test). At 1.1.3 boot_and_setup, tutorial, sprites_1010,
  side_quests_1_0, story_choices, fight_to_victory, main_quest (to the ending, ~55 min), pause_items_1_1_3 and the
  editor_* tests pass (1.2.2: every test but main_quest and fuzz_all_maps (not run), 8 at a time from a copy; 1.2.0: sprite_sheets_1_2_0, every editor_* test, boot_and_setup, sprites_1010, tutorial,
  item_color_1_1_8, npcs_1_1_9, scripts_1_1_10, text_edit_1_1_11, facing, item_details, resource_nodes, fight_to_victory,
  standing_1_1_2, story_choices; 1.1.11: text_edit_1_1_11, every editor_* test, scripts_1_1_10, npcs_1_1_9, boot_and_setup,
  new_game_name_focus, tutorial; 1.1.10: scripts_1_1_10 plus the 1.1.9 list; 1.1.4: boot_and_setup, tutorial, pause_items_1_1_3, item_details_1_1_4 pass; 1.1.5: those plus
  facing_1_1_5, music_ambient; 1.1.6: those plus every editor_* test and editor_import_1_1_6; 1.1.7: all of those plus tutorial,
  tutorial_defeat_1_1_7, sprites_1010, fight_to_victory, standing_1_1_2, resource_nodes_1_1, story_choices,
  side_quests_1_0; 1.1.9: every editor_* test, main_quest, npcs_1_1_9, companion_and_menus, companion_quest,
  keep_seal_guard, first_hour, bounty_boards, healer_services, fuzz_all_maps and the 1.1.7 list).
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
  (preview panel above Placed objects), `edObjLookKeys`. Guide topic `objtype`.
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

## Editor edits (1.1.3, owner's #23: "take into account the changes I've made")
- The owner edits content with the in-game Editor, in the installed copy or a `build\game*` copy (Run). The Editor
  writes into that copy's `assets\`, which every build / install used to replace - the edits were lost.
- `tools\pull-editor-edits.ps1` (run first by build.ps1 and install.ps1, or by hand) compares each copy with its
  `assets-manifest.txt` (SHA-256 per asset file, written by build.ps1 and shipped in releases; a copy without one is
  compared with `releases\<version.txt>\assets`). Changed files go into `src\assets` when src still has the old
  version; if src changed the file too, the copy's version is saved to `editor-edits\<time>\<copy>\` (git-ignored)
  and reported - **if `editor-edits\` exists, merge those files by hand and delete it**. Deletions are only reported.
  install.ps1 also puts edits back into the new install when the release didn't change that file.
- **Start of every session: run `.\tools\pull-editor-edits.ps1`** and commit what it pulls with the next version
  (build.ps1 does it anyway, but look at the diff: Editor saves rewrite maps' legends).
- GenericEditor files keep their comments now (`EdSection::lead` = comment lines above a section, `GenericEditor::tail`).
  Map files saved by the Editor still lose their comments and get a new legend lettering (harmless).
- At 1.1.3 the owner's edits from the installed 1.1.2 were merged by hand: warrior sprite 104,12, savepoint sprite
  36,25, templates door_secret_crypt / paper_sign, item the_two_ring (made `unique=1`), and the maps ashfall,
  bandit_camp, crypt (a secret room with a letter, a door and The 2 Ring), hollowmere (now 44x34), hollowmere_home
  (now 7 wide).

## 1.1.3 other changes
- Pause menu (`Game::pauseEntries`, game.cpp): entries by id (0 Resume, 1 Party & Items, 2 Load, 3 World Map,
  4 Journal, 5 Settings, 7 Back to Editor, 6 Exit) - tests count DOWN presses, keep Load third. In combat (`inCombat()`)
  1 and 3 are greyed ("Not during a fight."). `openPause()`.
- Combat: Esc pauses at any time (top of `updateCombat`), except on your turn with a sub-mode open (target/list/action
  menu), where it backs out. It no longer opens the action menu (that's A_ACTIONMENU). Bar has a Menu button at the
  right (`combatMenuButtonW`); the turn-order strip moved down to `TURN_STRIP_Y` 40 so its header clears the button.
- Back to Editor: `Game::playtest` (set in `App::startPlaytest`, cleared on requestDesktop) adds the entry;
  `requestEditor` makes the app reopen the Editor (`editor.open()`, the open map stays); the game keeps running.
- Item details: `Game::drawItemDetails` (menus.cpp) draws tags, a two-column stat grid and the description under a
  rule; used by the pack's Items tab, shops and the equip picker (its panel grew by up to 104 px for it).
- `unique=1` (ItemDef::unique): `makeAffixed` returns the base id, crafting skips the quality roll, enchanting
  refuses. Editor Items tab has a checkbox for it. Owner's call: unique items can still be reinforced (+1..+3) -
  reinforcing isn't an affix.

## 1.1.4 item details layout (owner's #25)
- `Game::drawItemDetails` (menus.cpp): rows carry a group (0 weapon, 1 defense + stat boosts, 2 resists, 3 use/light);
  a new group always starts a new line (+3 px), so boosts and resists never share a line. Values in each column start
  at that column's widest label + 8 (full-width rows at least the left column's); a row whose value would overflow
  its column gets its own line. Pack panel is 26 px taller: `INV_ACT_Y`/`INV_WHO_Y` (top of menus.cpp) place the
  actions and "Who?" lists, used by both the drawing and the mouse code. Test: `tests/item_details_1_1_4.txt`.

## 1.1.5 (owner's #26-#28)
- #26 text padding (owner: "nothing should sit at the bottom of text boxes"): keep text >= ~6 px above a box's bottom
  edge. Item panels (pack, shop): the item type is a grey line under the name (y 83), details start at y 98 and stop
  8 px above the panel bottom (`drawItemDetails` maxY); shop compare lines start at 180. Equip picker likewise.
- #27 facing: `Game::faceRight` (game.h) is the party's left/right look, changed only by a sideways step/turn
  (`tryMove`, `interactAt`); `facing` (0-3) is still the interact direction. Sprites face left unflipped. Not saved.
  Combat units never flip. Headless `expect faceright`. Test: `tests/facing_1_1_5.txt`.
- #28 ambience: superseded by 1.1.6 (see below).
- The Maps tab's hover-cell info moved from the right end of header row 2 (now full: bg 54 wide, ambient + v) to a
  boxed line at the bottom right of the map view, above the key hints (`ay + ah - 38`).
- #26 audit (other boxes fixed): board Standing cards (standing.cpp), Minesweeper/Solitaire footers (toys.cpp,
  Minesweeper window +4 px), hotbar hover tip (game.cpp), Editor tooltip (edui.h, 16 px), graph/text-view hints.
  Font ink spans y+2..y+11 for text drawn at y: `bottom - 17` leaves ~4 px. Not changed: the combat Spells/Items
  popup draws "1-8: put on the hotbar" just below its box (combat.cpp ~1218).

## 1.1.6 (owner's #28-#30)
- #28 (owner: the "wave" was in the soundtrack, not ambience; keep ambience, just no waves where they don't fit):
  every base-game map header has `ambient=` (overworld/frosthold/sunscar/ashfall/wreck amb_wind, hollowmere/brasswick
  amb_town, millbrook/hermit_hut amb_forest, murkwater/marsh amb_swamp, caves/mine amb_cave, crypt/barrow amb_crypt,
  bandit_camp amb_fire, lab/metro/ship amb_machine, rift_core amb_rift; hollowmere_home none). dungeons.txt entries too;
  without one a generated dungeon gets one by style (loadMap). `Game::ambientWanted`: `none` = silence; outdoors
  storm > rain > snow (amb_storm/amb_rain/amb_wind) replace the map's loop; amb_town/amb_forest -> amb_night at night.
- Sounds: `tools/mksounds.py` new loops amb_crypt/storm/forest/fire/sea (own RandomState so older loops are unchanged),
  amb_wind reworked (was a slow low swell = surf), amb_cave drips lowered (high ones chirped), crypt music has no
  "whisper" noise swell, rift's is a short breath. `AMBIENT=a,b` / `TRACKS=a,b` env vars regenerate only those.
  No ffmpeg here: the script falls back to the `soundfile` package (pip installed --user), writing in 8192-sample
  chunks (libsndfile crashes on one big vorbis write). amb_sea is used by no base map (for coastal campaigns).
- #29: `Editor::newCampaign` writes only campaign.txt (name, icon, gold=0, no start); the first map made with Maps >
  New sets the start. Import window: `editor_import.cpp` (P_IMPORT popup; `openImport(kind, gen)`, kinds IMP_GEN
  (any gens[] tab, by section name + listKey, replaces same id), IMP_MAPS/IMP_DLG (copies files at once), IMP_KINDS
  (via a temp overlay file + loadTileKindsOverlay), IMP_CAMP (campaign.txt fields), IMP_FLAGS). Button drawn by
  `listHeader` in the list headers of every content tab; Sounds/Tileset are shared, no import. An empty campaign can
  be started (no classes / missing start map -> placeholder room).
- #30: `edMatch` (edui.h, all words, case-insensitive) in every search box; `EdUI::searchField` (hint text + x);
  generic lists search every value and have a Filter button (`openFieldFilter`, `GenericEditor::filterKey/filterVal`,
  tokens from `edFilterTokens`); Maps and Dialogue lists got search boxes (dialogue searches file text).
- The owner's "The Lost Lighthouse" campaign was pulled into `src\assets\campaigns` at 1.1.6; the owner said it was
  added by accident and it was deleted at 1.1.7.
- Gotcha: the Bash tool eats backslashes inside heredocs (a doubled backslash-n becomes a newline). Write patch scripts
  with the Write tool and run them rather than piping Python through a heredoc.

## 1.1.7 the Tutorial remade (owner's #31: "more robust and more in theme")
- `src\assets\campaigns\tutorial` (id stays `tutorial`: the setup wizard and desktop look for it), version 2.0. Story:
  the day before the main game; old Wick (Hollowmere's lamplighter, a scout; NOT a Warden - the Wardens of Hollowmere
  died out long ago, his grandmother was one) and the violet light that fell into Greywood. Ends with finish= into the
  main game's opening ("Elder Mara has sent for you"). Three maps: `tut_watch` (Wick's yard: beacon lever, footlocker,
  Warden's Token, moonleaf gather + field crafting, wayshrine, gate), `tut_wood` (glass-backed rats, hotbar, Dace's
  shop: sell splinters / buy the quarry key, thornwrack obstacle needs=burn, key gate), `tut_quarry` (dark: torch,
  Wick joins as ranger `join=ranger:Old Wick:156,24`, winch lever + bridge [change], goblins, scrap drone, the shard
  chest with onuse=dialogue:tut_end).
- The maps were generated from hand-drawn rows by a throwaway script (markers for object positions); edit the .txt
  files directly now. Wick stands in front of the quarry winch so it can't be pulled before he joins.
- hints.txt rule: each lesson's hint holds while all EARLIER lessons are done (cumulative `a&b&c`), so the panel shows
  the first unfinished lesson; mode hints (combat/shop/menu/craft...) come last so they win. Undoable states become
  flags via whole-map triggers with require= (tut_equipped, tut_crafted, tut_fav, tut_lit); a "b" hint with the live
  condition covers the step before the trigger fires. Triggers fire on a step; only the first matching trigger fires
  per step (onStep returns).
- Robustness: Wick's yard talk (`tut_wick`) answers each unfinished lesson; skip option in `tut_wick_first`; Wick gives
  torches if you have none (camp + quarry); Dace gives the key if you're broke; campaign `defeat=dialogue:tut_fallen`.
- Engine added for it: campaign.txt `defeat=SCRIPT` (CampaignInfo::defeat; `combatFinish` revives + full heal, resets
  the groups, _fleegrace, runs the script; Editor Campaign tab field "If the party falls", Import, guide topic
  campaign.defeat). Condition `lit` (evalCond: lightSteps > 0 or equipped gear with light=). Doors opened by openif=
  now set their flag= (worldstate.cpp) - before, only a door the party opened did. Journal quest pins show a visited
  map's name. The Training panel sits higher on the crafting screen. Headless: expect `cond:X`, command `partywipe`.
- Bot gotcha: `choose`/`bump` settle and answer the rest of a dialogue themselves (closeDialogue). To look at a talk
  that a chest/trigger opens, stand next to it and `key E`, then answer with `key ENTER` + ~60 frames each.
- Tests: `tutorial` (whole playthrough) and `tutorial_defeat_1_1_7` (skip + losing a fight).

## 1.1.8 item colours (owner's #32: "go over the entire sprite in white then allow a color tint")
- Every item picture on the sheet is one colour (checked for all items at 1.1.8), so items.txt `color=r,g,b`
  (`ItemDef::recolor/color`) draws it with `drawRecolor` (white shape x colour). No color= = the sheet's own colour,
  so old items look the same. Draw item icons with `drawItemTile(ts, cv, def, x, y, scale)` (data.h), never
  `ts->draw(d->tile)` - every item icon in menus/craft/combat/hotbar/ground drops goes through it.
- Editor Items tab: `Editor::drawItemLook` (editor_items.cpp) is the Look card above the fields (big preview, menu +
  ground previews, tile + Pick sprite, RGB fields, Sheet colour / Lighter / Darker, palette). The `tile`/`color` rows
  are hidden in the form for items. Owner's "keep the colours as they are in the picker": the tile picker still shows
  the sheet unchanged; the RGB fields start at the picture's own colour (`tileOwnColor`, most common non-black pixel);
  the palette = colours the items use + the sheet's most used (`sheetColors`, near shades merged), in hue order.
  Guide topic `item.color`.

## 1.1.9 NPCs of their own (owner's #33: "a better NPC editor; convert all NPC objects to NPCs, companions too")
- `data/npcs.txt` `[npc]` = the character (name, sprite, color, dialogue, shop, craft, board, travel, hostile, wander,
  notes; companions: companion=CLASS, partysprite, level, stats, gear, spells). A map `[npc]` object is a placement:
  `npc=<id>`, x, y, its own `id` (kept from before: saves' hidenpc_MAP_ID and npcmove use it), show/hide/moveif/movex/
  movey; any key it sets (non-empty) wins. Order: placed > template > NPC > [type] npc (`applyObjectTemplate`,
  world.cpp; `NPC::def` = npc=). `setNpcDefs(mergedConfig("npcs.txt"))` in loadSystems. Campaigns merge base + own
  (tutorial has its own npcs.txt with `wick`).
- Converted at 1.1.9 by a throwaway script: 79 base placements -> 66 NPCs, ids from the name (slug), shared NPCs
  `notice_board` (board= town stays on each placement), `coachman`, `rook`, `ilse` (ilse_warden/ilse_cell keep own
  name+dialogue), tutorial `wick` (4 placements, each its own dialogue) + `dace`. No save format change.
- Companions: `join=<npc id>` (`parseJoin`, world.cpp; `JoinSpec`); empty join= = the NPC being talked to; the old
  `join=class:Name:col,row` still works (no NPC extras). The 6 join lines were changed to `join=bram` etc. Camp talk
  falls back to the companion NPC's dialogue (menus.cpp). Join applies level (fixed, else hero's), stats (before
  level-ups), spells, gear (equippable -> worn, else pack).
- Editor: gens[17] = npcs.txt, tab 24 "NPCs" (`editor_npcs.cpp`). drawGeneric draws the Look card (`drawLookCard`,
  editor_items.cpp, shared with items: tile key "sprite", default 104,0) and `drawNpcInfo` under the fields (placements
  from mapScan, Place on map, Open dialogue, Rename id = `renameNpc` (npcs.txt + every map's npc= + dialogue join=),
  talk summary, companion box, `npcProblems` - also in Check all). `npcSec` falls back to the base game's npcs.txt for
  inherit=1 campaigns. `resolvedObj` fills from the NPC; `joinClassOf` for the flag index / dialogue lint. Maps tab:
  Objects tool "NPC:" chooser (`placeNpc`; none = `newNpc` makes and saves one), placement row `npc` with edit /
  other..., "make NPC" for an old-style npc object (no npc=), "make template" hidden for npcs.
- Headless: `expect npc:<placement id>` (name;sprite;dialogue;npc) and `member:<n>` (name;class;sprite;level).
  Test `tests/npcs_1_1_9.txt` (also clicks through the tab: tab 24, Bram row at 60,147).
- At 1.1.9 the owner's installed edits were pulled: creatures (bear sprite, Vell sprite=203,17), item colours,
  brasswick map.

## 1.1.10 scripting (owner's #34: "something for scripting ... robust ... the coachman as an example, not only NPCs")
- Files: `data/scripts/<id>.txt` (campaign's own `scripts/` first, via dataPath). Language summary in
  `src\src\game\script.h`; compiler `script_compile.cpp` (no game state - the Editor's Check uses it), runtime
  `script.cpp` (Game methods). The owner's manual is the guide topics `tab.scripts`, `scripting.commands|values|
  actions|hooks|coachman`, `obj.npc.script`, `map.onenter`. "concepts.scripts" (the old one-line `k:v;k:v` lists) was
  retitled "Action lists" - keep the two words apart: a *script* is a file, an *action list* is `onuse=` etc.
- Compiled to a flat statement list with jumps (if/elif/else/while/for/break/continue). Identifiers are lowercased;
  lookup order local > keep var (`extra["var_NAME"]`, saved) > built-in > flag value > 0. `let`/`keep` refuse
  built-in and action names. Action lines = any `scriptActionKeys()` word + value with `{expr}` interpolation, run
  through `runAction`. Values are int64 or text; numeric text counts as a number.
- Runs (`Game::scriptRuns`, not saved; cleared in newGame/deserialize): `startScript` runs at once until a wait.
  Waits: say/ask = the hidden dialogue `__script` (updateDialogue calls `scriptDialogueDone`; consecutive says reuse
  the box); pick = world map with `wmPick` (returns a location id in `wmPickResult`); wait = seconds on the map;
  blocking actions (`isScriptBlockingAction`) wait until mode is EXPLORE (`tickScripts`, end of Game::update). A
  blocking action first closes the run's own talk box. A run started from a regular dialogue's action takes that box
  over if it says something. Limits: 200000 steps per resume (endless loop -> error), call depth 40, 64 runs.
  Errors go to the message log and `lastScriptError`.
- Hooks: NPC `script=` (interactAt, before board/travel/dialogue), action `script=id[,args]` (everywhere actions
  run), map header `onenter=` (pendingOnEnter, run once in EXPLORE after a transition or new game; not on load),
  item `effect=script:id[:keep]` (useItemOn; must be a consumable; hotbar skips the who-uses picker). New actions:
  `time`, `sound`, `encounter`, `weather`, `stat`; `teleport` takes a 4th text. `lastFightWon` -> `won`.
- Coachman: npcs.txt `coachman` has `script=coachman` instead of `travel=1` (travel=1 still works for other NPCs).
  The script copies fastTravel's fare/time/ambush rules.
- Dialogue answers scroll past 12 (6 with large text): `dialogueOptionsVisible`, `dlgOptFirst`.
- Editor: tab 25 "Scripts" (`editor_scripts.cpp`, `ste` TextEdit, `scriptFiles`, `lintScript`, `openScript`,
  textEditWidget syntax 1), Import kind IMP_SCRIPTS, Used by key `script:<id>` and flag index scan scripts
  (FlagUse tab 25 = script file + line), Check all lints scripts and missing `script:` refs, NPC `script` field,
  item `effect` is a text field + picker, onuse "+ action" lists scripts, map header "on enter" button (row 1; the
  name field shrank to 100 px).
- Headless/console: `con script CODE` (`\n` between lines), `con scripts`; expect `var:NAME`, `scripterror`,
  `scripts` (running count), `talk` (talk box text + answers). Test `tests/scripts_1_1_10.txt`.

## 1.1.11 text editing (owner's #35: "if it would appear in Notepad it needs it" + "a way to extend the text box")
- `editor/textops.h`: word helpers (`txt::wordLeft/wordRight/wordAt`, find with case / whole word). Word = alnum + `_`.
- `EdUI::textField` (edui.h): double-click = word, triple = all (`EdUI::clicks`, counted in `begin` by time + spot),
  Ctrl+arrows / Ctrl+Backspace/Delete by word, own undo/redo per focused field (`fUndo/fRedo`; in form tabs `genUndo`
  eats Ctrl+Z first, so there the form undo wins), Shift+Del / Ctrl+Ins / Shift+Ins, scroll that stays put (`fOff`).
- Right-click menu: `EdUI::openMenu(id, items)`; `menuInput()` at the top of `begin` eats its clicks/keys; the picked
  command waits in `menuCmd` until the owning widget calls `takeCmd(id)` (immediate mode: the menu never touches text).
  Drawn by `ui.drawMenu()` after the popups. The OS clipboard is now read on any left click / Insert too (main_raylib)
  so the menu's Paste works.
- Big box: a field sets `ui.bigReqId` (Ctrl+E, the "..." button on overflowing text, menu); `Editor::update` opens
  `P_BIGTEXT` (`openBigText`/`drawBigText`, editor_text.cpp, a single-line wrapped `TextEdit` `bigTE`, widget id
  7001); OK sets `ui.bigApplyId` and the field with that id takes the text next frame. `ui.bigAllowed` is false while
  popups draw (no big box from inside a popup).
- Multi-line editor: `Editor::textEditWidget` moved to `editor/editor_text.cpp` and rewritten: visual rows (`VRow`,
  word wrap breaks after spaces), `TextEdit::scroll` stays the first LINE shown (+ `scrollSub` row inside it) so old
  code that sets `te.scroll = cy - 6` still works. Undo snapshots keep the caret; `snapshot(kind)` merges a run of
  typing (kind 1, breaks at spaces) or deleting (2); `undoStep/redoStep` (the graph's Ctrl+Z uses them). Find bar
  (`t.bar` 1 find / 2 replace / 3 go to line, fields after the widget's id), autocomplete (`scriptWords`, syntax 1,
  Tab takes - Enter never does), `liveCheck` (0.5 s after the last change runs lintScript/lintDialogue -> `t.marks`).
  `TextEdit::load` no longer adds an empty last line (saving used to grow the file by a blank line each time).
- Wide: `t.wide`, toggled by Ctrl+E / the status-line or header button; drawDialogue/drawScripts then lay out only the
  editor (header has Save / Check / Narrow). Layout changes move widget ids, so toggles set `t.refocus` and the widget
  takes the focus back next frame. Dialogue starts with wrap on (`Editor::init`). `clearKeepView()` instead of
  `te = TextEdit()` keeps wrap/wide when a file closes.
- Headless: `ctrlshift K`, `alt K`, `shiftclick X Y`, `tripleclick X Y`; keys HOME END INS F3 G H U Y SLASH
  LBRACKET RBRACKET 9 0; expect `edtext:te|ste|big` (lines joined by |), `edsel:`, `edcaret:` (line,col), `edstate:`
  (wrap wide bar ac pop marks menu focus), `edfield:<gen>:<sec>:<key>`. New commands/expects go in `moreCommand` /
  `edExpect` lambdas: MSVC failed with C1061 (else-if chain nested too deeply) when they were added to the chains.
  Test `tests/text_edit_1_1_11.txt`. Guide topic `edit.text`.
- The game's own inputs (new-game name, dev console) only got Ctrl+Backspace (console: Ctrl+V too) - no caret there.

## 1.2.0 sprite sheets (owner's #36 import / "new campaigns default to no spritesheet", #37 categories + groups)
- `Tile` has a sheet index `s` (gfx.h). Data writes `col,row` (sheet 0) or `col,row@name`: always `parseTile` /
  `tileText` (data.h), `tileAt(t, dc, dr)` for size-2 neighbours, `sameTile` to compare. Saves store member tiles as text.
- `Tileset::useCampaign(baseData, campaignDir, inherit, keepOrder)` (gfx.cpp), called by App::init and
  Game::switchCampaign BEFORE data loads (parseTile resolves names through the global sheet list). Sheet 0 "tiles" =
  campaign `sheets/tiles.png` if it has one, else the base `assets/tiles.png` (the `img`/`tilesTransparent` members,
  which the font and desktop always use). Then the campaign's `sheets/*.png`, then base `assets/data/sheets/*.png`
  (always loaded; `visible()` = offered by pickers: own sheets, plus base ones for inherit=1 / the base game). Every
  sheet is converted at load to the base layout (12x12 at pitch 13 from 1,1; other tile sizes scaled nearest; see-
  through -> black) by `convertSheet`, so all draw code is unchanged. An unknown `@name` = placeholder, draws nothing.
  `keepOrder` (Editor reloads) keeps indexes so already-parsed Tiles stay valid.
- `sheets/<name>.txt`: `[sheet]` tile/offset/gap/clear(black|alpha|r,g,b)/title (game reads it), `[cells]` r<row>=
  one category letter per column (t p c m i e l x .), `[group]` name= tiles=c,r c,r. Base sheet's file is
  `assets/data/sheets/tiles.txt`, seeded at 1.2.0 by a throwaway script (regions by eye + what the data uses each
  cell for; race rows of the character block: monsters vs people; 77 groups). Edits save at once; a campaign editing
  a base sheet's categories writes its own copy (copy on write, `saveBook`).
- Editor: `editor_sheets.cpp` - books (`syncBooks`, `saveBook`), `drawTilePicker` (P_TILE: sheet buttons, category
  buttons with counts, groups list, compact grid with "Not sorted yet" after the sorted tiles, Whole sheet mode,
  right-click menu `openTileMenu`/`applyTileMenu`), the Tileset tab (select/drag/ctrl/shift, keys 1-9, groups
  buttons), the import window P_SHEET (`openSheetImport`, preview zoom, grid overlay, "As the game cuts it"),
  `sheetImport` / `copyBaseSheets` / `removeSheet` / `reloadSheets`. `openTilePicker(t, cb, cat)`: callers pass the
  category (`tileCatFor` in editor.cpp; kinds ground 't', prop 'p'; map objects npc 'c' else 'p'; Look card i/c).
  An empty category on the sheet falls back to All. `popCancel` (new) runs when a list/text popup is cancelled, so
  the picker reopens after "Add to a group...". PNG drops: `InputState::dropped` (rl_shim `rl_dropped_files`).
- Check all reports sprites/tile kinds on a sheet the campaign doesn't have. Guide topics `tab.tileset`,
  `sheet.campaigns|import|categories|groups`, `tile.picker`.
- Test `tests/sprite_sheets_1_2_0.txt` (imports assets/icons.png as a 32x32 sheet; run from a copy of assets).

## 1.2.1 window size (owner's #38: boot screen cut off after moving from an ultrawide to a smaller monitor)
- The window opened at 640x480 x settings scale with no monitor check (only the Editor's canvas change checked), and
  fullscreen was applied only when the boot screen ended. Now `rl_init` creates the window hidden (not on web);
  main_raylib's `fitWindow(cw, ch, want, center)` picks the largest scale <= wanted that fits (monitor - 80 x
  monitor - 120) and calls `rl_place_window` (rl_shim.c: size + centre between title bar and taskbar, or move only if
  off the monitor), then `rl_show_window`. Used at start, leaving fullscreen, the Settings scale buttons and canvas
  changes. settings.fullscreen is applied right after the window shows. The saved scale is not lowered.
- Checked on this PC's 1920x1080 monitor with scale=3: opens 1280x960 (2x), whole window on screen.

## 1.2.2 the game fills the screen (owner's #39; "the square aspect ratio can remain as an option")
- `UI::SCREEN_W/SCREEN_H` (ui.h) are runtime ints now (`UI::BASE_W/BASE_H` = 640x480 constexpr). Never cache them in
  a `static const` (HOTBAR_Y / HB_X became macros). `App::screenSizeFor(sw, sh, classic43, integer)`: canvas = the
  output area's shape, 480 high (up to 600 when that gives an exact whole-number scale with integer scaling), at least
  640 wide; narrower than 4:3 -> 640 wide and taller. 1920x1080 -> 960x540 at 2x; 2560x1440 -> 853x480 at 3x.
  `App::setScreen` re-inits the canvas and calls `Desktop::screenChanged` (clamps icons/windows/pointer) and
  `Game::screenChanged` (camera, weather). The Editor likewise: `Editor::W/H` are `static inline` (BASE 960x720),
  `App::editorSizeFor` / `setEditorScreen`; Editor::update re-makes its canvas on a size change.
- main_raylib computes the size every frame from the window's inside (fullscreen = the monitor) and calls setScreen
  (or setEditorScreen in the Editor). `placeWindow(scale, center)` sizes a windowed window in the monitor's shape (or
  640x480 / 960x720 in Classic 4:3). `req.canvasW = -1` = back to the game's size after the Editor.
- Setting `classic43` (settings.ini `classic43=`, Settings > Display "Classic 4:3 screen", next to Fullscreen): the old
  fixed 640x480 / 960x720 with bars.
- Sub-view: `Game::subViewMode` (MENU, SHOP, CRAFT, JOURNAL, BOUNTY, LEVELUP, WORLDMAP - the screens that cover the
  whole 640x480) are drawn by `Game::drawModeUI` into `subCv` (640x480, SCREEN_W/H temporarily 640x480, mouse moved)
  and copied to the middle; the margins get the screen's own dim (`subViewDim`). Game::update moves the mouse the
  same way for those modes. Everything else (world, HUD, combat, dialogue, pause, save/load, cutscenes, desktop)
  lays out against SCREEN_W/H. Outdoor reveal radius grows with the screen (`revealAround`).
- Headless always runs 640x480 (tests unchanged); `screen W H` / `edscreen W H` set another size. Test
  `tests/screen_fill_1_2_2.txt`. The web build (main_web.cpp) wasn't changed (no Emscripten here): still 640x480.

## 1.2.3 fights by touch (owner's #40: "mainly for overworld maps, but give the option to all maps")
- Map header `touch=1` (`Map::touchCombat`, world.h; dungeons.txt `touch=` too, copied by Bake): in `Game::onStep`
  the proximity check uses range 1 (any of the 8 neighbours) instead of aggro; extra groups join only if they touch
  the party too or share link=; mid-fight reinforcements (`combatJoinGroups`) only if next to a fighter or linked.
  Groups still walk over within aggro+3 (updateNpcs). Random encounters unaffected. Not set on any base map (owner's
  choice to tick it). Editor: "touch" checkbox on Maps header row 2 (right-click = guide `map.touch`; unticked
  removes the key); row 2 fields were narrowed so Play fits at 960 wide. Guide `map.touch`, `dungeon.touch`.
- Console `touch [0|1]` (this visit). Test `tests/touch_combat_1_2_3.txt`. Headless `expect mode` values are upper
  case (EXPLORE, COMBAT).

## 1.2.4 chases and ambushes on touch maps (owner's #41)
- On touch=1 maps every group has a leash point (homeX/homeY: placed spot; ambush = where the party stood) and range
  (`leash=` on the [enemy], default `max(6, aggro + 5)`, `Game::leashOf`). `Game::touchChase` (called by tryMove just
  before onStep) makes groups notice (aggro + LOS, stealth / 3, party inside the leash) and then step once per party
  step (`groupStepToward`: BFS in a 14-tile box, 4-way, may not leave leash + 1). Party out of the leash -> gives up,
  walks home on a timer. On touch maps updateNpcs never moves a group toward / next to the party; chasers don't move
  by timer at all ("they don't move if you don't move" - owner). No chase during `_fleegrace`.
- Random encounters (`randomEncounter`) on touch maps call `spawnAmbushers`: a temp group (`EnemyGroup::temp`,
  chasing, leash 12) 5-8 walking steps away (in sight first, else hidden, else 3-4 steps, else the old instant fight).
  One temp group at a time for step rolls. Temp groups that lose the party walk off and vanish out of sight; beaten /
  gone ones are erased in updateNpcs (only outside combat: cb uses group indexes). enterMap ends chases and temp groups.
- Instant only for the coach (owner): `randomEncounter(force, now)`; fastTravel's `_ambush` passes now, the action
  `encounter now` / `encounter:now` too (coachman.txt uses it). Plain `encounter` on a touch map spawns at a distance.
- chasing/temp aren't saved. Headless expects `groups`, `enemydist` (in the edExpect lambda). Test touch_chase_1_2_4;
  touch_combat_1_2_3 changed (standing still no longer leads to a fight). Guide topic obj.enemy.leash; map.touch,
  concepts.encounters, scripting actions updated.
- The owner's installed copy had overworld `touch=1` + `encounter=0` (pulled at 1.2.4); encounter was put back to 4
  since #41 asks for ambushes that spawn at a distance there.

## 1.3.0 (owner's #42-#45: store/press kit, rights, browser demo, music)
- #45 music: the Pixel Fantasy pack by AlkaKrab (owner bought it; licence PDF in the zip): royalty-free, games OK,
  **no redistributing the tracks as-is** (and open-source use needs permission). So the .ogg files are git-ignored
  (`src/assets/audio/music/pixel_fantasy/`); the zip is in `assets\packs\` (ignored) and `tools\unpack-music.ps1`
  (run by build.ps1) unpacks it when files are missing: `NN_<title>.ogg` from Loops (seamless), `fx_1..3.ogg` stings.
  sounds.txt: the 20 old ids (title, overworld, overworld_night, town, forest, dungeon, tech, combat, boss, fanfare,
  gameover, ending, brasswick, ashfall, murkwater, sunscar, frosthold, crypt, swamp, rift) point at pack files
  (dungeon/combat/swamp have 2 files = random pick), then every track under its own id (`old_tavern`,
  `hidden_glade`, `sting_1`...) so maps can pick them. Map music= set at 1.3.0: millbrook hidden_glade, hermit_hut
  ballad_of_ashenwood, bandit_camp tales_by_firelight, barrow call_of_the_raven, wreck tech, tut_watch twilight_march.
  Volumes were levelled by RMS (target -17 dB, capped so peaks stay < 1), so many are >100 (up to 160): volume is a
  plain gain and the Editor allows 0-200. `Audio::defaultDefsText` mirrors sounds.txt. The old generated music was
  deleted; mksounds.py only makes music when asked (`music` arg). Test `tests/music_1_3_0.txt`. Matching was by
  title + loudness/tempo analysis, nobody has listened yet - the owner may want to swap some in the Sounds tab.
- Local campaigns: `src/assets/campaigns/test/` (the owner's Editor test, EverRogue tileset of unknown licence) was
  pulled from the installed copy at 1.3.0 and is git-ignored. It stays in src\assets so builds/installs keep it, but
  build.ps1 `Remove-LocalCampaigns` drops every git-ignored campaign from -Package and the release zip (the release
  folder used by install.ps1 keeps it); make-presskit and web_assets.py skip them too.
- #42: `press-kit/STORE-TEXT.md` (store text checked against the game; Idea Jotter's idea Features still says "a
  bonus campaign" - not ours to edit, the owner was told), `press-kit/RIGHTS.md`. Screenshots: `tools\make-presskit.ps1`
  runs `src/tools/presskit_shots.txt` (headless at 960x540, Editor 1280x720) from a temp copy of assets, then
  `src/tools/promo_kit.py` (rewritten for 16:9) writes `press-kit/screenshots/16x9_1920x1080` and `press-kit/art`
  (key art, Steam capsules/hero/logo/icons, itch cover/banner/social). 4:3 shots deleted; old art moved to
  `press-kit/old/` (`old/ai-generated-0.9/` = the AI-made key art/cover/banner: never use). demo.txt `store=` is
  empty (no store page yet) and the demo's Wishlist icon only appears when it's set.
- #43: icons have only informal permission (author's itch comments); RIGHTS.md has a message for the owner to post.
  Trademark: web/Steam/itch/Trademarkia found nothing; USPTO/EUIPO/WIPO couldn't be searched (JS/captcha) - owner to
  do by hand.
- #44: Emscripten install was refused by the permission system in the 1.3.0 session (needs the owner). Prepared:
  `build-web.ps1` (finds emsdk in $env:EMSDK or %USERPROFILE%\emsdk, stages assets with `src/tools/web_assets.py`
  (demo's music only, re-encoded at compression 0.5 ~60% size, ~36 MB total; tracks left out get an empty file=),
  emcmake + Ninja into `build\cmake-web`, copies to `build\dist\web`; -Release zips `releases\triffall-v<ver>-web-demo.zip`;
  -Serve runs python http.server on 8642), CMake `TRIFFALL_WEB_ASSETS`, builds.json "Browser demo". main_web.cpp now
  calls screenSizeFor/setScreen each frame (fills the page). main_web.cpp was compile-checked with MSVC + a stub
  emscripten.h only - the real emcc build has never run since 1.0: expect fixes on first build.
- Gotcha: libsndfile Vorbis writes must be chunked (8192 samples) or Python dies with exit 127 silently.
- raylib's stb_vorbis decodes all pack files and the re-encoded web files (checked with a small C program).

## 1.1.2 standing and crafting tiers (standing.cpp)
- Owner's rule for #16/#17: approval and town rep only change through deliberate player actions, never passively
  (no decay, no chat grind - the +1 "chat" approval was removed).
- Companions: `approve=` only applies to companions in the party (`changeApproval`); `amends=CLS:N` for absent ones.
  Strained <= -15 (`Character::bond` -1: -10 accuracy, -5 evasion), leaves at -40 (`checkCompanionMoods` in
  updateExplore -> `companionLeaves`: gear to the pack, `joined_` cleared, `left_CLS` set, NPC unhidden via
  extra `home_CLS` = its hidenpc key). Loyal = flag `loyal_CLS` and approval >= 10 (bond +1). `syncBonds()` runs
  at startCombat, load, join and after set/clear. Each companion dialogue has [gifts] (once each, `gift_CLS_ITEM`),
  [strained], [loyal], [sorry]/[amends] (200 gold, cond `room` = party has space). Choice reactions were added
  to converts, goblin_parley (spare2), raider_kesh, junk_vosk.
- Towns: factions.txt `reward=REP|items|title` and `project=title|mats|rep|gold|text` (FactionDef). Board has two
  pages (Tab/PgUp/PgDn, `boardPage`): notices and Standing (amends / current project / favours). State in extra:
  `favour_TOWN_i`, `project_TOWN` (count). Bounty rep only up to 30; dropping a taken notice -2; Disliked (-10)
  hides offers; Hated (-50) `shopRefuses`. Shop `item=id:stock:rep` hides stock below rep.
- Crafting: `Recipe::skill` (recipes.txt `skill=`, gate in `canCraft`; diff 3/4/5 got skill 2/3/4). `addCraftXP` +
  dialogue `craftxp=station:n`. Tiers: mithril (formula_mithril from Gorran's trial 1, `q_smith`), riftsteel
  (formula_riftsteel, trial 2 = master_hilt), Morrow's herbal (`q_herbal`), Vosk's blueprints (`q_tinker`).
  New nodes in resources.txt (mithril_ore, rift_crystal, sunpetal, bloodroot) placed by hand on troll_cave,
  old_mine, rift_core, ship, lab, sunscar, murkwater, marsh (no styles= so generated dungeons draw as before).
  Reinforce +3 = steel + mithril ingot (tech: + rift crystal); 2nd enchant needs a rift crystal.
- New item icons reuse unused cells picked by eye (no tiles\ folder); some gear shares pictures with older items.
- Test: `tests/standing_1_1_2.txt` (leave/amends/gift/loyal, board standing, shop refusal, skill gate, trial).
  Headless `expect` knows `approval:CLS` and `cskill:STATION`.

## Where things live
- Code: `src\src\{core,game,desktop,editor,platform}`. Data: `src\assets\data\` (plain text: items, creatures,
  npcs, quests, dialogue, ASCII maps...). Campaigns: `src\assets\campaigns\` (tutorial only; the
  Gullwatch sample campaign was removed at 1.1.1 by the owner, The Lost Lighthouse at 1.1.7).
- Art: `src\assets\tiles.png` is the Urizen 1-bit sheet by vurmux (CC0; categories/groups in
  `src\assets\data\sheets\tiles.txt`, campaigns' own sheets in `<campaign>\sheets\`), `icons.png` the desktop icons
  (Cute Desktop Icons by Mayke-Arth). Music: Pixel Fantasy pack by AlkaKrab (1.3.0, see above); sound effects and
  ambient loops are generated by `src\tools\mksounds.py`.
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
