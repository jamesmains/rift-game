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
crash report), `accessibility`, `combat_feedback`, `music_ambient`, `boot_and_setup`, `demo_mode`.
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
