# Headless test scripts

Build `triffall_headless` (CMake target, or `make headless`) and run from the project root:

    triffall_headless tests/fight_to_victory.txt out/

Each script is a list of commands, one per line:

    frames N            advance N frames (60 fps)
    key NAME            press and release a key (UP DOWN LEFT RIGHT ENTER ESC TAB E Q T F5 1-5 PGUP PGDN BS DEL PADSTART)
    keys A B C          several key presses with a short gap
    hold NAME N [X Y]   hold a key for N frames; optional analog stick axes (-1.0 to 1.0)
    type text           type characters (name entry)
    move/click/dblclick X Y   mouse actions in 640x480 framebuffer coordinates; warp X Y changes absolute position with zero delta
    shot file.png       save a screenshot into the output directory
    dump / combatdump / log   print game state
    teleport MAP X Y    jump to a map (debug)
    give ITEM [N] / gold N / level XP / flag NAME / god / combat a,b,c / target / fuzz N [seed]
    editor / tab N      open the Editor, switch to tab N (0 maps .. 12 sounds, 13 tileset, 14-17 recipes/affixes/banter/achievements); fuzzed N [seed] fuzzes it
    pad 1/0             pretend a gamepad is active (controller glyphs in hints)
    forceflee           test-only: force the current combat through successful flee cleanup
    interactmoving 0/1  set whether movement into interactables triggers immediately
    cheststate X Y      test-only: report whether a chest at the current map coordinates is open
    fuzzdesk N [seed]   fuzz the desktop (toys, windows); con CMD runs a dev-console command; clock MIN / weather W set time and weather
    audio               print the current music track and the sound events played since the last call

The output directory doubles as the save directory for that run (settings.ini, slotN.sav).

1.2.0 (sprite sheets, `sprite_sheets_1_2_0`): `rtap X Y` (right click, press + release), `ctrlclick X Y`, `drop PATH`
(a file dropped on the window), `sheetimport PNG NAME TW TH OX OY GX GY [black|alpha|r,g,b]`, `sheetcopy` (copy the
base game's sheet into the edited campaign), `edsetcampaign ID`; expect `sheets` (the sheets the pickers offer),
`sheetcat:SHEET:c,r` (category letter), `sheetgroups:SHEET`, `sheetdraws:SHEET:c,r`, `pickcat`, `pickshown`, `picksheet`.

The output directory must exist before running. `triffall_headless bakeall` (no script) bakes every
generated dungeon and the overworld into map files.

0.8 scenario scripts: `darkness_combat`, `shop_stacks`, `levelup_multiclass`, `fog_and_crafting`,
`keep_seal_guard` (NPC moves when the Mayor's Seal flag is set), `world_obstacles` (dig/blast/levers),
`editor_campaign_switch`, `overworld_map_file`. Use `con auto 1` to let the AI fight for
the party when checking balance.

0.8.1 scripts: `save_keeps_explored` (uses `saveload`, a save/load round trip), `spells_tab_nav`, `shop_mouse_pad`,
`menu_back_and_pause`, `healer_services`, `shop_door_hours`, `editor_arrival_check`. Extra console commands for
testing: `hurt N [poison]`, `bestiary CREATURE N`, `doors` (lists door tiles and the shop that locks them).

0.8.5 scripts: `worldmap_modes` (campaign world map off / current / a map id / overworld, `revealmap`), `editor_where_used`
(Flags tab, quest stage cards, encounter usage panel), `editor_help_tabs` (the ? buttons and the guide window). New
commands: `guide TOPIC` opens the Editor guide on a topic; key `F1`; console `worldmap MODE`.

0.8.6 scripts: `quest_objectives` (journal checklist, map item reveal), `editor_extras` (Check all, Used by, quest
objective rows, dialogue Used by). The rename/undo checks in the notes are run on a copy of assets, since renaming saves
files.

0.8.8: `item_categories` (category filter, sorting, quest items can't be dropped or sold).

0.8.9: `desktop_controller`, `desktop_controller2` (pad on the desktop: start button, start menu, New Game keyboard, settings rows, fine volume, toys, achievements, space in names), `rpg_ui_pass` (pause position, pad item categories, scrolling combat item list, pad glyphs in the combat bar). New driver keys: PADA PADB PADX PADY PADLB PADRB PADLT PADRT PADBACK.

0.9.0: `quest_fail_reputation` (fail= lines, finishing beats failing, rep= from dialogue, shop prices by reputation),
`summons_and_ai` (Call Wolf ally, a necromancer raising skeletons, summons crumbling with their caster),
`editor_playstate_export` (Campaign tab Export .zip and Play from the start), `editor_factions_quests` (Factions tab,
quest fail cards), `editor_dialogue_graph` (graph view: select, pan, re-layout, jump to text), `first_hour` (tips with
keyboard and pad keys, the opening quest stage), `controller_opening` (desktop to village with pad buttons only:
Start menu, Random name, intro, journal on Back, world map on the left stick, party menu on Start).

`expect KEY OP VALUE` checks (any failure makes the run exit with code 1). Keys: gold x y hp level members running
state bounties windows mode map campaign window log cblog (combat log) flag:X item:X quest:X (stage index)
queststatus:X (journal text) rep:X price:ITEM (buy price now) allies summons savefile:PATH (exists in the save dir).
Ops: == != >= <= > < contains. New driver keys: PADUP PADDOWN PADLEFT PADRIGHT PADLS PADRS. New console commands:
`rep [FACTION N]`, `questfail QUEST COND | TEXT` (adds a fail line for the session), `cast SPELL [DX DY]` (in a fight),
`tips` / `tips reset`.


1.0: the playthrough bot (`src/platform/bot.h`) adds commands that play the game the way a person would - real key
presses, real dialogue choices, the combat AI fighting for the party:

    newgame NAME CLASS [DIFF] [SEED]   start a game directly (seeded: the same run every time)
    goto MAP          walk there through exits and the overworld (a route is found over every map's exits)
    walk X Y / bump X Y / talk NPC_ID / fight FLAG / choose TEXT / settle / buy ITEM [N] / waituntil HOUR
    careful 0|1       (default 1) torches underground, potions, inns, restocking and better gear on the way
    checkpoint NAME / report FILE      the level / gold / kills / time curve (CSV in the output directory)
    strict 1          stop at the first failure; autoending 0 lets a script screenshot the ending pages

Scripts: `main_quest` (cottage to the ending, every story flag and the three choices checked), `side_quests_1_0` (the
nine new side quests, and the Editor's Check all with errors counted as failures), `story_choices` (the other answers to
the three choices, the ending screens), `save_safety` (.bak rescue, autosave, newer-version refusal, dropped items,
crash report), `accessibility`, `combat_feedback`, `music_1_3_1`, `boot_and_setup`, `demo_mode`.
Other new driver commands: `checkall [strict]`, `setting NAME VALUE`, `saveslot N`, `loadslot N|auto`,
`filereplace FILE OLD NEW`, `truncate FILE`, `boot T`, `setupwizard`, `perf N [LABEL]`, `segfault` (not in the suite).
New expect keys: `music`, `ambient`. Pacing and frame-time tools: `tools/pacing.sh`, `tools/perf_scenes.txt`.

1.0.7: `feedback_1_0_7` (font choice, quest wrapping, combat favorites, calm start). 1.0.8: `tutorial` (the whole
Tutorial campaign played through). New: `campaign ID` (the next newgame plays it); expect keys `weather`, `hint` (the Training
panel's text), `fav:N` (the hero's favorite slot N); op `lacks` (the opposite of `contains`).
`setting font 0/1` and `setting ambient N` set the new options.

1.1.0: `editor_objects_1_1` (clicks in the map object panel stay in the panel; the Objects tab 22 and Resources tab 23),
`resource_nodes_1_1` (harvesting a gather node that takes item, tool and amount from its resource type). Editor tabs
18-21 are Campaign, Flags, Bounties, Factions; 22 Objects, 23 Resources.

1.1.5: `facing_1_1_5` (stepping up/down keeps the party's left/right look; expect key `faceright` 0/1).
`music_ambient` now expects no ambient loop anywhere (#28: only a map header / dungeons.txt `ambient=` plays one).

1.1.6: `music_ambient` expects each map's own loop again, rain/storm replacing it outdoors and night insects in towns.
`editor_import_1_1_6` (run from a copy of assets: it writes campaign folders): a new campaign is empty, Import copies
entries / maps / dialogue / settings, search and Filter counts, an empty campaign can be started. Commands:
`newcampaign NAME`, `edimport gen|maps|dlg|kinds|camp|flags GEN SOURCE all|id,id`, `edsearch GEN [words]`,
`edfilter GEN KEY [VALUE]`, `edsave GEN`; expect keys `edcount:GEN`, `edshown:GEN`, `asset:<path under assets>`.

1.1.7: `tutorial` plays the remade Tutorial (#31) end to end; `tutorial_defeat_1_1_7` takes Wick's skip and loses a fight
on purpose (campaign `defeat=`). New: expect key `cond:CONDITION` (any game condition, e.g. `cond:lit`), command
`partywipe` (in a fight: the party loses at once). `editor_import_1_1_6` imports the Tutorial's `tut_arrive` dialogue
and `editor_playstate_export` expects `tutorial_2.0.zip` (the Tutorial is version 2.0).
1.1.8: `item_color_1_1_8` (Items tab Look card: a palette colour sets color=, saved, the pack draws it; run from a copy of assets).
1.2.4: `touch_chase_1_2_4` (touch=1: a group chases step for step, waits while the party stands still, gives up
outside its leash, catches the party walking back; a random ambush appears 4+ tiles away and chases; `encounter:now`
fights at once). Expect `groups` (x,y[,c chasing][,t ambush] of every group on the map) and `enemydist` (nearest).
1.2.3: `touch_combat_1_2_3` (map header `touch=1`: a placed group in sight does not attack until it is next to the party;
without it the same group attacks from its aggro range). Console `touch [0|1]` sets it for the current visit.

1.3.1: `music_1_3_1` replaces `music_ambient` (no ambient loops any more, #50): every map names its music (#48), fights
play the campaign's fight music or the map's own (console `combatmusic ID|-`, #47), and Editor header screenshots at
960x720, 1280x720 and 1920x1080 (#46) with a click on Exit.

1.3.2: `music_1_3_2` - a boss plays its own creatures.txt `music=` (goblin_chief: boss) even on a map with its own fight
music; an ordinary fight there plays the map's.

1.4.0 (#75-#77): `obstacles_1_4_0` (needs=freeze/shock/emp/light, force by a utility=force spell, the Frost Flask,
Charged Cell, EMP Grenade and torch; frozen water thaws when the party leaves), `puzzles_1_4_0` (the crypt's lever
sequence, the old mine's pressure plates and crates, the bandit camp's combination lock), `terminals_1_4_0` (the
Annex, metro and ship terminals; desktop lore files unlocked by the vault chests). Console: `testspell ID UTILITY
[COST] [MEMBER]` (a made-up spell with that utility=, until the class spells exist), `castout SPELL [MEMBER]` (cast it
outside a fight, as from the Spells menu), `clearmap [MAP]` (the map's enemy groups gone for this visit - use it
before teleporting there), `reset_files` (lock the desktop files again). Expect keys: `obstacle:X,Y` (1 while an
obstacle blocks it), `kind:X,Y` (tile kind), `crate:ID` (x,y), `plate:X,Y` (1 while down), `lorefile:NAME` (1 once
unlocked), `inputtext` (the text box), `message` (the open message box, "title: text"), `notes` (the Journal's Notes; 1.6.2). The text box is mode NAME; `type` fills it.

1.4.0: `dialogue_1_4_0` - stat checks in dialogue (#73: `stat:cha>=12`, `partystat:str>=14`, the `[CHA 12]` tags, script
`stat()` / `partystat()`, `stealthy`), other ways through quests (#74: the Stolen Caravan talked out and sneaked out,
Water Rights patched + the middle way, Ashwing reasoned with) and companion quest abilities (#81: `teach=`, the Loyal
version swapping in and out, a companion taught while away, save/load). Console: `setstat STAT N [MEMBER]`,
`stealth [N]`, `spellsof [MEMBER]`. Expect keys: `answers` (the answers as shown, tags included, " | " between) and
`spells:<member>` (comma list).

1.5.0 (#86, #96, #97, #99): `recipes_gated_1_5_0` (trial recipes known=0, taught by the dialogue's `recipe=`, given to
old saves by migrations.txt `[learn]`), `field_notes_1_5_0` (fieldnotes.ini survives a wipe and reload; the desktop's
Field Notes.txt), `stealth_1_5_0` (noisemakers aimed and thrown, the crypt deadfall and the bandit bear trap, darkness
against eyes that need light, a hurt group's fight), `save_points_1_5_0` (a shrine near every boss, the dungeon arrival
autosave). Commands: `throwat X Y ITEM`, `aimthrow ITEM`, `groupmove I X Y`; console `light [N]`, `reset_fieldnotes`.
Expect keys: `recipe:ID`, `fieldnote:ID`, `fieldknown:ID`, `bestiaryids`, `group:I`, `trap:X,Y`,
`aim`, `aggro:I`, `bossshrine`, `groupshrine:I`, `lastautosave`.

1.5.2 (#105, #106): `fav_craft_1_5_2` (a favourite set in a fight survives the fight; the crafting quantity stops at
what can be made). Command: `craftsel RECIPE` (select a recipe on the open crafting window's Craft tab). Expect keys:
`favs:<member>` (the 8 hotbar slots, comma list), `craftqty`.

<!-- 1.6.0 game -->

<!-- 1.6.0 rules -->

<!-- 1.6.0 story -->

<!-- 1.6.0 builders -->

<!-- 1.6.0 hub -->

<!-- 1.6.0 quests -->
- 1.6.1 desktop / launcher: commands `installed` (setup done: the Triffall icon appears), `deskaction ACTION` (launcher,
  new, load, readme, achievements, about, mycomputer...), `launchcampaign ID`, `todesktop`; expects `launcherpage`,
  `deskicons`, `launchcampaign`, `deskslots` (e.g. 0100), `campaignsave:ID:N`. Desktop icons no longer include New Game:
  tests start a game with `deskaction new` (or `newgame`). Test `launcher_saves_1_6_1`.
- 1.8.0 (#159-#162) day colours and night look: command `nightclassic 0|1` (Settings > Display "Night colours"); expects
  `daybg:<kind>` / `daytint:<kind>` ("r,g,b"), `px:X,Y` (screen pixel "r,g,b"), `pxlum:X,Y`, `pxblue:X,Y` (blue minus red:
  > 0 in the night palette). Test `night_look_1_8_0` (Hollowmere at 10:00 / 19:30 / 22:00, classic, Greywood Cave with a torch).
- 1.8.1 (#164-#166): console `sky [0|1]` (this visit); expects `blackbg` (under=1 cells of the current map whose bg is
  still pure black), `darkhides:X,Y` (1 = objects on that tile aren't drawn). Test `dark_sky_1_8_1`; `night_look_1_8_0`
  now sets `con sky 1` in Greywood Cave to check the open-sky shadow tint.

1.8.3 (UI themes): `theme frame|native|onebit` switches the look (an unknown name = frame); `hudclick KIND ARG` clicks
the HUD hot spot of that kind wherever the active theme drew it (kinds: action belt combat unit pause member craft ptab
slot pick lead; `hudclick pause -1` = the fight's Menu); `deskaction continue` / `deskaction deleteadventure` press the
launcher's Continue / Delete. Expects `theme`, `settingsini:KEY` (a value in the out folder's settings.ini) and mode
`SETTINGS` (Pause > Settings). `tools\run-tests.ps1 -Theme native` runs every script with `theme native` first; tests
that check pixels or desktop icon positions set `theme frame` themselves. New: `theme_switch_1_8_3`,
`pause_settings_ingame_1_8_3`.
