# Triffall

An old-school open-world RPG with X-COM / Fallout-style turn-based tactical combat,
wrapped in a fake Windows 98 desktop that serves as the main menu.

Forty years ago the sky over Aldmoor tore open. When it closed, pieces of other worlds
lay scattered across the land: a city of glass and asphalt, a starship, machines that
still hum. Now the Rift is waking again - and your village elder has sent for you.

* 5 classes (Warrior, Ranger, Mage, Scavenger, Cleric) you **multiclass** into rank by rank, 5 recruitable
  companions (respec at a shrine priest or with a Draught of Forgetting), party of 4
* Hand-crafted overworld and dungeons (all plain ASCII maps you can edit), 6 towns, an extra
  chapter (The Open Sky) and side areas; **campaigns** add their own desktop icon (see Campaigns)
* Darkness that matters: torches, lanterns, light spells and flares; enemies with darkvision
* World objects: rubble to dig or blast, ice to melt, levers, sluices, NPCs that move when the story says so
* Deep crafting: station skill, formulas, failure chance, fine/masterwork rolls, reinforcing, enchanting, salvage
* Easy / Normal / Hard difficulty
* Swords, bows, guns, lasers, spells; 50+ monsters; 130+ items; 20 side quests (three or four in every town), 5 companion quests and a 7-stage
  main quest with three choices that change the ending (break, bury or keep the shards)
* Proximity-triggered tactical combat fought in place: AP, movement points, hit chances,
  aimed shots, cover-free line of sight, area spells, grenades, status effects, fleeing
* Save shrines + 3 named save slots
* Keyboard, mouse and gamepad
* Chiptune soundtrack and sound effects, all data-driven and replaceable (see Audio)
* Cover, flanking, overwatch and attacks of opportunity; companions with banter and approval
* Day/night, weather, shop hours; crafting and random affixes on loot; world map with fast travel
* Journal with quests (map pins, checklists, branching endings and failure), bestiary, stats, notes; notice-board
  bounties; town reputation that moves shop prices; 33 achievements
* A Win98 desktop with Minesweeper, Solitaire, My Computer lore files and a screensaver

## 1.1.9 NPCs of their own

* **NPCs are now entries of their own** (`data/npcs.txt`), made once and placed on maps. Every person and animal in the
  game - companions too - was moved over: a map `[npc]` is now a placement (`npc=<id>`, its spot, its id on that map
  and when it is there), and everything else comes from the NPC. One NPC can stand in several places (Ilse, Rook, Old
  Wick, the coachmen and notice boards), and a placement can still give itself its own name or dialogue.
* **Companions are set up on their NPC**: `companion=<class>`, plus their picture in the party, the level they arrive at,
  their own stats, the gear they wear and extra spells. Their dialogue adds them with `join=<npc id>` (the older
  `join=class:Name:col,row` still works). Saved games carry on as before.
* **The Editor has an NPCs tab**: a Look card (picture, colour), pickers for dialogue, shop, crafting station, notice
  board and class, and a box showing every map the NPC stands on (click to open it there), what talking to them does,
  how a companion joins and anything that's wrong. Place on map... puts an NPC on a map; Rename id... renames it
  everywhere. In the Maps tab the Objects tool places NPCs from a list, a placement has edit / other... buttons for its
  NPC, and an old hand-made npc object can be turned into an NPC with make NPC.

## 1.1.8 item colours

* **Items can have their own colour.** Every item picture is a single colour on the sheet, so an item can now be drawn
  as a white shape in any colour you like: `color=r,g,b` in items.txt. Without it the picture keeps its colour from
  the sheet, so every existing item looks the same as before. The colour shows everywhere the item does (pack, shops,
  crafting, the hotbar, on the ground).
* **The Editor's Items tab has a Look card** at the top of the form: the picture large, as it looks in a menu and on
  the ground, a Pick sprite button (the sheet is shown as it is), the colour as numbers with Sheet colour / Lighter /
  Darker buttons, and swatches of the colours the items already use plus the sheet's most used ones.

## 1.1.7 a new Tutorial

* **The Tutorial is new.** It's now a short adventure set the day before the main story: last night a violet light fell
  into Greywood, and old Wick, Hollowmere's lamplighter, wants to see what it was - once you've shown him you can look
  after yourself. Light his watch beacon, open his footlocker and wear his grandmother's Warden's Token, pick moonleaf
  and make bandages, save at the wayshrine, then follow the road: glass-backed rats, Dace the pedlar and his quarry
  key, a wall of thornwrack to burn, a pitch-dark quarry, Wick joining you, a winch and a bridge, goblins behind cover,
  and a machine guarding what fell. It ends where the main game begins.
  * It teaches more than before: map objects (levers, obstacles, gather nodes), field crafting, selling as well as
    buying, darkness and torches, a companion and the journal's checklist.
  * It's harder to get stuck in. The Training panel always shows the first lesson you haven't done yet, even if you
    do things out of order. Wick answers "how do I...?" for every lesson, hands out torches if you run out, and Dace
    lets you have the key if you can't pay. Losing a fight no longer ends the game: Wick helps you up and you try again.
  * Played it before? Tell Wick so, and he hands you the yard's kit and opens the gate.
* The old training-hall tutorial and The Lost Lighthouse (an empty campaign added by mistake) are gone.
* Quest pins in the journal show a place's name instead of its id.
* Editor: the Campaign tab has **If the party falls**: leave it empty for the usual game over, or give it a script to
  run instead (see the guide). New condition `lit`: the party has a light going.

## 1.1.6 ambience that fits, empty new campaigns, search and filters in the Editor

* **Background sounds are back, where they fit.** Every place has its own loop again: wind on the open road, voices in
  the towns, birdsong at Millbrook and the hermit's hut, drips in the caves, a still, heavy air in the crypts, frogs in
  the marsh, a campfire at the bandit camp, machines in the tech ruins. Rain, storms (with thunder) and snow take over
  outdoors, and towns and woods turn to night insects after dark. No more wave-like sounds where there's no water: the
  wind was reworked to sound like wind, and the crypt music lost the swell that sounded like surf. New loops: crypt,
  storm, forest, fire and sea (the sea is there for campaigns with a coast).
* **Editor: a new campaign starts empty.** Each tab now has an **Import** button that copies entries in from the base
  game or another campaign: items, creatures, classes, quests, maps, dialogue, tile kinds, flag notes, campaign
  settings and more. Tick what you want; things you already have are marked and replaced.
* **Editor: search and filters.** Every list has a search box (all words must match, and in the data tabs it looks at
  every field, in Dialogue at the text too). The data tabs also have a **Filter** button: pick a field and a value,
  like type = weapon or faction = undead. The pickers' search works the same way.

## 1.1.5 room under the text, steady facing, no stray ambience

* **Text no longer sits on the bottom edge of its box.** In the pack and shops the item type now shows under the item's
  name, and the item details stop well above the panel's edge. The same goes for the town board's Standing cards, the
  Minesweeper and Solitaire hints, the hotbar's hover names and several Editor hints and tooltips.
* **Your character keeps looking the way they were.** Walking up or down no longer turns the party to face left; only
  a step to the side turns them.
* **No more automatic background sounds.** Waves, birds, crickets, drips and the like no longer play by themselves
  (they often didn't fit the place). A map can still have a background loop: the Editor's Maps tab has an **ambient**
  field with a list of every loop (and generated dungeons can set one in the Dungeons tab).

## 1.1.4 item details line up

* **Item details line up.** In the pack, shops and the equip picker, every value in a column of an item's stats now
  starts at the same place, and the stats come in groups that each start a new line: weapon stats, then defense and
  stat boosts, then resists, then what it does when used. A stat boost and a resist no longer share a line.
* The pack's item panel is a little taller, so more of the description fits under the stats.
* The Editor changes made in the installed game (The 2 Ring's resists, the crypt map) are part of the game now.

## 1.1.3 pause in a fight, back to the Editor, tidier item details

* **Pause during combat.** Esc (or the new **Menu** button at the right end of the combat bar) opens the pause menu at
  any moment of a fight, enemy turns included: Load Game, Journal, Settings and Exit to Desktop all work. Party & Items
  and World Map are greyed out until the fight is over. Esc still backs out of a target pick or a spell/item list first.
* **Back to Editor.** A game started with the Editor's Play has **Back to Editor** in its pause menu: it reopens the
  Editor where you left it, and the next Play carries on with the same game.
* **Item details are easier to read.** The pack, shops and the equip picker show an item as a short grid of stats
  (gains in green, losses in red; an equal resist to every damage type reads "All resists +2%"), then the description
  on its own under a line. Tags like Quest item, Unique and Two-handed sit above the stats.
* **Unique items:** `unique=1` on an item (a checkbox in the Editor's Items tab) means it never gets a random affix -
  not from loot or chests, not from a crafting roll, and enchanting won't take. Hand-made one-offs stay exactly as
  written. The 2 Ring is one.
* **Your Editor changes stick.** The Editor now keeps the comments in the data files it saves. (For the developer
  build: edits made with the Editor in a built or installed copy are carried back into the source before every build
  and kept across updates.)

## 1.1.2 companions, towns and crafting that matter

* **Companions remember what you do - and only that.** Approval no longer creeps up from small talk. It moves when
  a companion sees you make a real choice (sparing the goblins, the converts' fate, the Sunscar well, selling
  Rusty's coil...), when you take up or turn down their own quest, and when you give them something they value
  (each companion says what they'd like; every gift counts once). Companions who aren't with you don't react.
  * **Strained** (-15 or lower): they fight worse (-10 to hit, -5 evasion). At **-40 they leave**, take nothing but
    leave their gear with you, and go back to where you met them. You can make amends there (200 gold).
  * **Loyal**: at 30+ approval, with their own quest done, a new talk in the party menu (T) makes them Loyal:
    +10 to hit and +5 evasion while their approval stays 10+.
  * The Journal shows each companion's standing (Resentful, Strained, Neutral, Warm, Trusting, Loyal).
* **Towns: a Standing page on every notice board** (Tab). It shows what your reputation does for you there and offers:
  * **Favours** at Liked (10), Friendly (30) and Honoured (60), claimed once each - including a unique piece of gear
    from each town (the Elder's Mantle, the Guild Signet, the Clan Fur Mantle, Bogwalker Boots, the Sunscar Veil,
    Scrapper's Goggles, the Harvest Charm).
  * **Civic projects**: three deliveries per town (two in Millbrook), from mill parts to rift-crystal lenses. They are
    the way past Friendly - odd jobs from the notice board now raise standing only up to 30.
  * **Making amends**: below Neutral, pay fines to be a stranger again (back to 0, never higher).
  * Consequences: Disliked (-10) - nobody will hire you, the board posts no work. Hated (-50) - the town's shops
    refuse to trade. Dropping a bounty you took costs 2 standing.
  * Friendly and Honoured customers see extra stock in some shops (steel swords and chain mail in Hollowmere, katanas
    and the Steelwork folio in Brasswick, a sniper rifle and power armour in Ashfall...).
* **Crafting tiers.** Recipes now need a station skill to attempt at all (shown on the recipe), so skill is a real
  progression: iron and leather, then steel and hardened leather, then **mithril**, then **riftsteel**.
  * New materials and harvest nodes: mithril veins (Rimehollow, Ashwing's Mine), rift crystal outcrops (the Scar, the
    Wreck's lower decks, the Annex), sunpetal (around Sunscar), bloodroot (Murkwater, the Drowning Marsh).
  * **The master crafters' trials**: Smith Gorran (Hollowmere), Arcanist Morrow (Brasswick) and Junk-Dealer Vosk
    (Ashfall) each ask for a trial piece made at skill 3, then teach the next tier (mithril; sunpetal, bloodroot,
    phoenix tonic and the elixir; power cores, stimpaks and energy shields). Gorran has a second trial that unlocks
    riftsteel (Smithing 5 and 6).
  * New gear: mithril sword, spear, helm, shield and mail; riftsteel blade and plate; the hardened jerkin.
  * Reinforcing to +3 now takes a mithril ingot (tech gear: a rift crystal), and a second enchantment a rift crystal.
* **Balance**: shops have limited stock of potions, scrolls, grenades and oils (it comes back every 3 days), so gold
  alone can't buy an endless supply. Several cheap recipes give less (arrows, bullets, bandages, mana potions;
  healing potions take 3 moonleaf), the elixir and Draught of Forgetting need rare ingredients, and a few doubled
  monster drops were trimmed (slime gel, goblin ears, rift dust). Frost lichen regrows in 3 days, like other herbs.
* **The dev console only exists in debug builds** (`build.ps1 -Dev`, the "Debug" build in Idea Jotter). The Full,
  No Editor and Demo builds can't open it. Its output now wraps instead of running off the screen, and `help` is
  grouped by topic. New command: `cskill [STATION XP]`.

## 1.1.1 object types, one boot screen, clearer caves

* **Object types (Editor, Objects tab)**: the list now starts with "every savepoint", "every chest", "every sign",
  "every door" and "every lever" (`[type]` entries in `objects.txt`). What you set there is used by every object of
  that type on every map and in generated dungeons, unless the object or its template sets it itself: its picture
  (`sprite=`, `opensprite=` once opened, `onsprite=` for a pulled lever), one colour (`color=`), the sound when used
  (`sound=`), a script run when used (`onuse=`, the same actions as a trigger: `heal`, `item:ID:N`, `message:...`),
  and for save shrines the twinkle and the step-on message, for signs the window title, for doors the open message.
  A preview shows the object closed and opened and says what using it does. Pulled levers now show a pulled picture.
* **Caves no longer look unlit**: cave, dirt, sand, snow, ice and rift floors had blank cells in their pattern list,
  drawn as pure black, which read as darkness. Every walkable floor cell now shows its pattern. The Editor shows the
  same mix of patterns as the game.
* **Gem veins look like veins**: a gem vein is now a blue rock pile (like the orange iron vein), so it can't be mixed
  up with the glowing crystal clusters, which are only scenery.
* **One boot screen**: the DOS / BIOS start-up now carries the Triffall title; the second splash is gone.
* **Clear font only**: the easy-to-read font is used everywhere and the font option was removed.
* **Skipping text can't pick an answer**: a press while dialogue text is still typing only shows the rest. The answers
  then appear dim and can't be chosen for a moment, and not before the key is let go.
* Desktop icons no longer light up through an open window or the taskbar.
* The sample campaign "The Gullwatch Light" was removed.

## 1.1.0 object templates and resource nodes

* **Editor fix**: clicking in a map object's panel (the box on the right of the map view) now edits the panel. It used
  to act on the map underneath, placing or selecting objects there instead.
* **Objects tab**: object templates (`objects.txt`). A placed object with `template=<id>` takes the template's type and
  every key it doesn't set itself, so changing the template changes every copy on every map; keys on a copy are its
  own. Place copies from the Maps tab ("template:" under the object types), or turn any object into a template
  ("make template"). The tab also lists every object on every map (filter by type, template or words) and changes
  them all at once: set or remove a key, link them to a template, unlink them.
* **Resources tab**: resource node types (`resources.txt`): what a gather node gives, the tool, days to grow back, its
  name, its own picture (`sprite=`) and colours, and where generated dungeons scatter it (`styles=`). Gather nodes name
  their type with `node=` and take the rest from it; the base game's nodes were linked to their types. A preview shows
  the node ready and picked clean, and every node placed on the maps.

## 1.0.16 bridges under your feet

* **Standing on the bridge**: bridges are drawn a little lower like the trails, so on an east-west bridge your feet
  land on the planks instead of over the water, and the bridge lines up with the path at both ends.
* Saving tile kinds from the Editor no longer drops their `shift=` setting.

## 1.0.15 back to black

* **Black ground, as the tileset was drawn**: every land tile is drawn on black again with its pattern in a dim shade
  (grass, floors, streets, sand, snow...), so people, monsters and objects stand out on their own. Water, lava and the
  like keep their colour. Sand, snow, concrete and metro floors got fuller patterns so walkable ground never looks
  like empty void.
* **No more outlines**: the 1.0.14 dark ring around sprites is gone. Instead a sprite's own black details (eyes, the
  gaps between arms and body) are drawn black, so the ground or a trail no longer shows through a character.
* **Standing on the path**: trails are drawn as one layer moved 4 pixels lower, running into the cell below, so
  feet land inside the trail instead of on its bottom edge. Trails are a darker brown so the hero shows on them.

## 1.0.14 easier to see

* **People and things stand out**: party members, townsfolk, monsters, chests, signs and dropped items get a thin dark
  outline (the sheet draws them for a black background), and very dark sprites such as the warrior are drawn lighter.
* **Calmer ground**: grass, meadows, tall grass, swamp, wasteland and the town and dungeon floors (wood, tile, stone,
  flagstone, plank, marble, carpet, cave, street, rift) draw their pattern as a soft texture in a shade of their own
  colour (`recolor=1`) instead of mixed bright or dark pictures. Patterned floors no longer look like a patchwork.
* **Trails sit under your feet**: `path` and `road` pieces are drawn lower in the cell (new `shift=` in
  `tilekinds.txt`, in sheet pixels), so people walk on the trail instead of floating over it.
* **Wide roads are solid**: a trail two or more cells wide fills in instead of drawing as a lattice with holes.

## 1.0.13 every tile redrawn

* **Every tile kind redefined** from the Tilebook description of the Urizen sheet: overworld pictures (forests, hills,
  mountains, sand, snow, swamp, places) use the sheet's overworld row; towns and dungeons use its walls, floors, doors,
  furniture and plants. Before, water was half snow clouds, sand was a flower field, roads were a dungeon floor, hills
  were grass tufts and the cave entrance was a hill; town "stone" walls are now masonry instead of a bracket pattern.
* **Joined pieces**: trails, rivers, pond and sea shores, bridges and modern roads pick the right piece for their
  neighbours (`edges=`, `join=`, `link=` and `under=` in `tilekinds.txt`, see the Editor guide). The Editor's map view
  draws them the same way.
* **Every map remade** with the new tiles. Dungeons: solid rock behind their walls, each with its own stone (icy
  Rimehollow, purple Rift Core, the bandits' palisade camp), cobwebs in crypt corners. Towns: trails and cobbled
  streets, ponds with shores, windows that match their walls, Hollowmere's cottage as a little house, its stream
  flowing under the road into a pond, a fenced graveyard and garden; Millbrook's log lodge and duck pond;
  Murkwater's boardwalk and piers over real water; Frosthold's log houses in the snow; palms round Sunscar's well;
  Gullwatch's pier and boats. Every NPC, chest, exit and trigger stayed where it was.
* New tile kinds: `path`, `street`, `boardwalk`, `ford`, `asphalt`, `rock`, `floor_flag`, `floor_plank`, `floor_ice`,
  `floor_rift`, `floor_snow`, `wall_fieldstone`, `wall_log`, `palisade`, `wall_ice`, `wall_rift`, `wall_green`,
  `window_stone`/`_brick`/`_gray`/`_metal`, `torch_<wall>`, `door_stone`, `ladder`, `candle`, `lantern`, beds, tables,
  chairs, wardrobe, couch, bar, graves, cobweb corners, fences, cart, wagon, boat, anchor, crops, scarecrow, flowers,
  trees (oak, willow, pine, palm), stump, reeds, lily pad, boulder.
* The built-in tile kinds are compiled from `assets/data/tilekinds.txt` (`tools/mktilekinds.py`, run by the build).

## 1.0.10 the right sprites

* **Every sprite checked against the Tilebook** description of the Urizen sheet (its names, row/race and column/pose tags,
  and which cells are empty). About 120 fixes, in the base game, the Tutorial and The Gullwatch Light:
  * **People look like people**: the elders, the mayor, Abbot Tyr, Old Fenwick, Old Hesk and Nyx were skeletons; the
    Warrior class, Bram, Reyna, Warden Hale and the guards were genies; bandits and scavengers were satyrs; the priests
    and healers were ice genies; the smiths were kobolds. Nyx now carries a bow in a grey cloak, guards wear helmets.
  * **Monsters**: skeletons were a crowd of townsfolk, wolf/bear/bat/spider were rat-men, the boar a monkey, the
    scorpion a satyr with a hammer, robots were civilians on skateboards. Archers and rangers hold bows (the old
    "bow" column was a book), the goblin chief carries his banner, the necromancer is a skull wizard.
  * **Props that were invisible** now show: cauldron, fountain, well, levers, sparkles, street lamps and traffic lights.
    Forge, anvil, bed, coffin, chair, shelf, pillar, altar, statue and shop sign point at the right pictures, and
    sci-fi walls are walls instead of doors and windows.
  * **Icons**: keys are keys (they were heraldic crests), gold is coins (it was a banner), the coil, tape, valve,
    music box, firewater, mittens, beads and bandage have their own icons, and the five blank achievements have pictures.
* `tiles.h` row and pose names now match the sheet (ROW_SKELETON is row 10, CHAR_COL_BOW is 156...).
* `build_windows.sh`: MinGW-w64 build script (cross-compile or MSYS2/w64devkit) next to `build_windows.bat`.
* `tests/sprites_1010.txt` screenshots the towns and a fight with the new sprites.

## 1.0.9 your desktop

* **Your desktop, kept**: right-click an icon (or E / pad X on the selected one) for Open, **Change Icon...** (any picture
  in the icon sheet) and Default Icon. Icon pictures, where each icon sits and the wallpaper are saved in
  `saves/desktop.ini` and come back on the next launch; new campaigns take the first free spot. The Start menu shows the
  same pictures.
* Every campaign icon uses the New Game controllers; the editor is now just **Editor** (desktop, Start menu, its own
  screens and these docs) with the cube icon.
* Icon names too long for their spot end in "..." and show in full under the pointer or when selected.
* `tests/desktop_persist.txt` changes an icon, moves one and picks a wallpaper, then reloads the desktop and checks them.

## 1.0.8 the Tutorial

* **The Tutorial replaces the tips.** The one-time pop-up tips (and Settings > Show tips) are gone. In their place is
  a **Tutorial** campaign (`assets/campaigns/tutorial`, first of the campaign icons on the desktop, and ticked by default
  on the setup wizard's last page): ten minutes in the Wardens' training hall, one room per lesson - walking, a chest,
  equipping from the party screen, a shop, a save shrine, the journal, a rat fight, a level-up, the hotbar and a goblin
  fight with cover - each door opening when its lesson is done. A green **Training** panel says what to do next and stays
  up until it's done; it's driven by the campaign's `hints.txt` (see Campaigns). At the end Warden Hale sends you back to
  the desktop with New Game open for the main game. Achievements don't unlock in it.
* **Combat action bar**: every button carries its key the way shops and menus show them (a key chip, or the controller
  button when a pad is in use), and so does the action menu (R).
* `tests/tutorial.txt` plays the whole Tutorial the way a new player would.

## 1.0.7 playtest fixes

* **Font choice** (Settings > Access > Font, and on the setup wizard's last page): *Classic* is the tileset's own letters,
  *Clear* is Spleen 6x12 (BSD licence, `assets/LICENSE-spleen.txt`), plainer and easier to read. It applies everywhere
  at once - desktop, game and editor; the TRIFFALL logo keeps its own letters.
* **Ambience volume**: wind, rain, town and the other loops have their own slider (Settings > General) and their
  default mix in `sounds.txt` is much lower (they were about as loud as the music).
* **Quest text wraps**: the party screen's Quests tab wraps each quest to the width of the list and scrolls; the journal
  wraps checklist lines too, keeps the opened quest on screen, and closed rows end in ".." when there is more.
* **Favorites in a fight**: with the Spells or Items list open, press 1-8 to put the highlighted entry on that hotbar slot
  (the list shows which slot each entry is on).
* **Tips** only showed while the chest or person they talked about was in plain view. (1.0.8 replaced the tips with
  the Tutorial.)
* **A calm first morning**: a new game starts clear, and the first half hour of play never rolls fog or storms.
* Frost Bolt, Blizzard, Power Armor and the Reactor Plate pointed at blank tiles; they have icons again.

## 1.0: a game you can finish

* **The ending**: after the Unmade King, an epilogue (`assets/data/epilogue.txt`): the shards, every town and every
  companion you recruited, chosen by your choices, reputation and approval; then the journey in numbers, the credits
  (`assets/data/credits.txt`) and **Keep exploring** (the world stays open, autosaved first) or back to the desktop.
* **Three story choices**: Grakk the goblin chief asks for terms in Greywood (spare the tribe and a goblin peddler turns
  up at Millbrook later; Hollowmere's farmers don't forgive you), Vell's followers in Ashfall (mercy: they become the Sky
  Wardens and heal you; the court: they work Brasswick's roads), and the shards at the Scar (break, bury or keep them -
  three endings). Six new achievements follow them (hidden until earned).
* **Nine new side quests** (two each in Ashfall, Murkwater, Sunscar and Frosthold, one in Hollowmere): A Letter for
  Fenna, Rusty's Still, Signal in the Static, Lanterns on the Water, Tam's Big One, Water Rights, Spin Spin Spin, The
  Pilgrim's Sister, Cold Hands - with branches, a temptation or two, and failures. Ashfall is a reputation faction now.
* **Balance**: experience is no longer split between the party - everyone standing gets the full award (with the split a
  full party finished around level 7 in a game whose last dungeon is level 11). Bosses keep to their lairs (the Lich
  used to walk into the first fight of Dunmarrow) and take half the map's level bonus; the Lich and the Scar are a
  little gentler. Fights where nobody can reach anybody break off after three quiet rounds instead of looping forever.
  A gun or bow with no ammo can still club someone standing next to you.
* **Saves you can trust**: saves carry a format version and are upgraded on load (`assets/data/migrations.txt` lists
  renamed items, flags, maps and spells for future patches; ids that no longer exist are dropped with a note); every
  save is written to a temporary file and swapped in, and the previous save is kept as `.bak` and loaded automatically
  if a save is ever damaged; an **autosave** is made whenever you reach a new place (Load Game > Autosave); a save from a
  newer version is refused with a clear message.
* **Crashes**: a crash writes `saves/crash.log` (version, place, party, last messages) and tries an emergency
  `saves/crash_recovery.sav`; the next start says so and offers to restore it.
* **Settings > Access**: large text (dialogue, messages, cutscenes, tips, epilogue), text speed (slow / normal / fast /
  instant), colour-blind friendly colours (blue / orange for sides, hit chances and cover), reduced flashing.
  **Small screens**: integer scaling is only used when 2x or more fits; below that (a Steam Deck's 1280x800 fits only
  1x of 640x480) the frame is drawn sharp at 2x and scaled smoothly to fill the screen.
* **Combat feel**: big damage numbers, a white flash and a shove on a hit, a sidestep on a miss, a beat of stillness and a
  burst on a kill, CRITICAL with a short freeze, Round / your-turn banners, NOW and NEXT in the turn order strip,
  a coloured hit chance, and a cover bar on the side of the target that faces you ("in half cover", "FLANKED").
* **Music and sound**: a theme for Brasswick, Ashfall, Murkwater, Sunscar and Frosthold, crypt / swamp / Scar music,
  a night overworld, an ending theme, and ambient loops under the music (wind, cave drips, swamp, rain, town, machines,
  night insects, the Rift; 1.1.6: crypt, storm, forest, fire, sea) set per map (`[ambient]` in sounds.txt, `ambient=` in a
  map header; outdoors, rain/storm/snow and night replace it).
* **First impression**: a logo splash after the BIOS lines, and on the very first launch a Win98 setup wizard whose last
  page asks two real questions (tips, large text). The Readme starts with "How to play - the first ten minutes".
* **Demo**: `cmake -DTRIFFALL_DEMO=ON` (or `build_windows.bat demo`) builds the demo: no editor, the base campaign
  limited to the maps in `assets/data/demo.txt` (Hollowmere to Brasswick), the demo's ending when the Mayor sends you to
  Dunmarrow, a Wishlist icon on the desktop. Demo saves carry over to the full game. Gullwatch plays in full.
* `discord=` in `saves/settings.ini` (as this README always claimed) now really points the Discord icon somewhere.
* **Testing**: a playthrough bot (`src/platform/bot.h`) walks, talks, chooses and fights its way through
  `tests/main_quest.txt` (cottage to the ending, every story flag checked) and `tests/side_quests_1_0.txt`;
  `tools/pacing.sh` plays the main quest on several seeds with and without god mode and writes the level / gold / time
  curve; `tools/perf_scenes.txt` measures the worst scenes. See BALANCE.md and RELEASE_CHECKLIST.md.

## 0.9.0 depth, tools and the first hour

* **Reputation**: towns (`assets/data/factions.txt`) remember the party, -100 to 100, in six tiers from Hated to
  Honoured. Dialogue changes it with `rep=hollowmere:10`, conditions test it with `rep:hollowmere:30`, achievements with
  `rep:hollowmere>=60`. Shops with `faction=` price by it (up to 15% off / 25% more), paid bounties raise the board's
  town (`rep=` on the board, default 3). The journal's Stats page lists every standing; the shop shows its town's.
  Healer Wren's Moonleaf quest now earns +10 with Hollowmere, and a new achievement (Local Hero) wants Honoured.
* **Quest failure and branches**: `fail=COND | text` in a quest ends it as FAILED (red in the journal, no map pin)
  unless a DONE stage already shows; several DONE stages behind different flags give a quest several endings.
* **Summon spells**: `summon=creature[:count]` with `duration=` turns. Rangers learn Call Wolf (rank 5), Scavengers
  Deploy Turret (rank 8), Necromancers raise two skeletons. Allied summons (blue frame) are moved by the AI, leave no
  body, XP or loot, and crumble when their caster dies.
* **Smarter enemies**: monsters finish off what they can kill this turn, pile onto the target their side already hit,
  care less about summons than summoners, and archers step into cover (when they can still shoot from there) before
  firing. Summoners call help early in a fight.
* **First hour**: one-time tips (`assets/data/tips.txt`) the first time you walk, meet someone, see a chest, fight,
  shop, level up or open the map - naming the keys or buttons of the device in use (Settings > Show tips turns them
  off). The main quest is in the journal from the first minute ("go to Elder Mara", with a map pin). The world map
  now has a controller button (left stick click; overwatch keeps it in fights).
* **Rebinding**: item category next / previous and sort are normal actions in Settings > Controls (default T / G / R,
  RT / LT / Y).
* **Steam / Steam Deck**: the New Game name field opens the Deck's (or Big Picture's) own keyboard when there is one,
  falling back to the built-in one; counter achievements show Steam's progress toast at the half-way mark.
* **Editor**:
  * Dialogue **Graph** view (header toggle): boxes and arrows for every node, colour-coded links, drag to arrange,
    Shift-drag an option onto a node to relink it, double-click to jump to the text.
  * **Factions** tab with a Used-by list; shop `faction` picker; quest stage cards have **fail conditions** (and the
    preview shows FAILED); spells get a creature picker for `summon`; Check all knows all of it.
  * Campaign tab: **Play from a state** (class, level, gold, flags, items, companions for every playtest, saved in
    the campaign's `playtest.txt`) and **Export .zip** (the whole campaign plus install notes, into `saves/exports`).
  * Guide topics for every new key, `rep=` / `rep:` in the dialogue syntax panel.

## 0.8.9 controller and UI pass

* **New Game** has a campaign picker (the campaign icons still open it with theirs selected), a **Random** name button
  and an **on-screen keyboard** (A on the name field, or the Keyboard button). Space no longer starts the game while
  typing a name - only Enter does.
* **Controller on the desktop**: Down past the last icon reaches the Start button; the Start menu keeps the cursor in
  it until Start or B; the D-pad moves row by row in windows (Settings: V-Sync -> Window scale -> volumes); volume
  sliders step by 1 (hold to run); a window puts the cursor on its first control when it opens; picking a save slot
  jumps to Load; Minesweeper (A reveal, X flag) and Solitaire (A pick / drop, X send to the foundations, B cancel)
  are playable; Achievements scroll with the D-pad.
* Nothing behind an open Start menu reacts to the mouse any more.
* **In the game**: the pause menu sits centred above the party bar; walking has one constant pace (no speed-up);
  the combat Item and Spell lists scroll (ten rows, names cut to fit, three lines of description); on a controller
  LT / RT change item categories and Y sorts; the combat action bar shows controller buttons while a pad is in use;
  mouse clicks in menus make a soft tick (`ui_click`) instead of the confirm sound.
* **Resource nodes** are drawn in their own colour (ore copper, herbs green, tech cyan; `color=` on the gather object
  overrides it) and turn grey once picked; the small item icon that sat inside herb patches is gone.

## Running (Windows)

Unzip and run `Triffall.exe`. The `assets/` folder must sit next to the exe; a `saves/`
folder is created on first run (settings.ini + slot1-3.sav live there).

The boot screen shows in a window for a few seconds, then the desktop goes borderless
fullscreen. **F11** (or Alt+Enter) toggles fullscreen at any time; the Settings window
also has the option.

Edit `discord=` in `saves/settings.ini` to point the Discord icon at your own server.

## Controls

Everything is rebindable in **Settings > Controls** on the desktop (two keyboard keys + one
gamepad button per action). Defaults:

| Action | Keyboard / mouse | Gamepad |
|---|---|---|
| Move / cursor | Arrows or WASD (arrows only in combat), click a neighbouring tile | D-pad / left stick |
| Interact / confirm | E, Enter, Space, left click | A |
| Back / pause menu (save, load, settings) | Esc | B |
| Party, items, spells, quests | Tab or I | Start |
| Spell menu / item menu | B (or S outside of walking) / Q | LB / LT |
| Favorites hotbar | 1-8 (assign in the Spells/Items menu with 1-8) | - |
| Toggle normal / aimed attack (combat) | A | Y |
| Defend / Flee / Action menu (combat) | D / F / R | RT / - / Back |
| End turn (combat) | T | RB |
| Center camera | C | RS |
| Pan camera (combat) | right-drag, screen edges, click a portrait in the turn strip | right stick |
| Overwatch (combat) | O | LS |
| World map | M (also in the pause menu) | - |
| Journal (quests, bounties, bestiary, stats, notes, achievements) | J | Back |
| Dev console (debug builds) | ` (grave) | - |
| Quick save (on a shrine) | F5 | - |
| Fullscreen | F11 / Alt+Enter | - |

Saving happens at save shrines (walk onto one, or F5 while standing on it); loading is in the pause menu. The desktop
Load/Save icons still work when no game is running.

## Systems

* **Combat**: standing next to a wall, tree or other blocking tile gives **half cover** (-20% to be hit) or
  **full cover** (-40%) against attacks coming from that side; a target with an enemy adjacent on two sides is
  **flanked** (+15% to hit, +15% crit). **Overwatch** (O) spends the turn to fire at the first enemy that moves
  into range and line of sight. Walking out of a melee enemy's reach draws an **attack of opportunity**.
  Fights start with everyone exactly where they stood on the map (the leader in front, followers on their
  trail), so a narrow corridor really does keep the fighter in front. **Fleeing** needs a full turn, nobody in
  melee reach, and a roll (20% + 2 x DEX, -12% per enemy within 5 tiles, -25% against a boss; haste and
  stealth help); escaping drops 5% of your gold.
  The hit-chance readout names each modifier.
* **Companions** comment on what happens (`banter.txt`), gain or lose **approval** through choices they witness
  (`approve=class:n` only counts for companions in the party), their own quest (it opens at 15 approval) and
  one-off gifts. Strained (-15) fight worse and leave at -40 (amends at their home: `amends=class:n`); Loyal
  (talked to at 30+ with their quest done) fight better. The party tab lets you pick who walks in front (**leader**).
* **Town standing**: every notice board has a Standing page (Tab) with favours (`reward=` in `factions.txt`),
  civic projects (`project=`) and amends. Bounties raise standing up to 30 only; Disliked towns post no work for
  you, Hated towns' shops won't trade. Shops can hold stock back for friends: `item=id:stock:rep`.
* **Time and weather**: a day is 24 minutes of walking; the HUD shows the day, time and weather. Night tints the
  map and swaps encounter tables to `<table>_night` where one exists; shops keep `hours=7-20` and their front
  door is locked while closed (inns never close; anyone already inside can always walk out); rain, storms,
  fog and snow depend on the biome and change visibility. Inns (`rest=1`) skip to morning.
* **Inns and healers**: inns (`rest=1`) restore HP and MP overnight for a few coins. Healers (`healer=1` in a
  dialogue node) charge for what an inn can't do: tend wounds right now (HP only), cleanse lingering poison and
  burns, raise a fallen companion, and a blessing that shields and mends the party for its next three fights.
* **Companion chatter** frequency (off / rare / normal / often) is in Settings > General.
* **Controller / keyboard-only**: every list shows a blinking marker on the focused row; the party screen moves
  up/down through members and right into their equipment; on the desktop the D-pad walks the icon grid, the
  Start/Menu button opens the Start menu, and inside windows the D-pad (or arrow keys) hops the cursor between
  buttons, checkboxes, sliders (left/right adjusts) and key-binding boxes, A/Enter clicks. Pad buttons can be
  bound by clicking a binding box and pressing the button.
* **Classes**: every level gives a **rank point** to spend on any class (the level-up screen shows your ranks,
  what each class gives and what every stat does, with a before/after preview). Hit points and mana grow with
  the hit/mana die of the classes you rank up; spells unlock by class rank. Companions start with their class
  and can be **respecced** (shrine priests for gold, or a Draught of Forgetting).
* **Darkness and light**: dungeons are dark; you see only what your light reaches. Torches (equip or use), the
  Warden's Lantern, the Light spell and flares push the dark back, and burning braziers/torches on the map light
  areas. Attacking into darkness costs accuracy unless the attacker has darkvision (undead, machines...).
  Fire and shock flash light for a moment.
* **Combat**: whole groups join a fight when they can hear it (linked groups like a chief and his guards always
  come together; nearby groups join on later rounds). Area spells and grenades hit **everyone** in the area,
  caster included - the preview warns you. Resistances and weaknesses matter more and are recorded when
  discovered. Moving reveals the map; paths are recalculated when a body stops blocking a corridor. The cursor
  shows a tile panel (terrain, cover, light, who stands there).
* **World objects**: `[obstacle]`s (rubble, vines, ice, rusted doors) are cleared by digging (Shovel, which
  wears out), blasting (Blast spell / blasting powder - a 3-second fuse that hurts anyone too close),
  burning, melting or brute force; `[lever]`s and `[change]`s rewrite tiles when a flag is set; doors can need
  keys (`require=`) or open on a flag (`openIf=`); NPCs can move (`moveif=`). Herb/ore `[gather]` nodes sparkle.
* **Crafting**: `recipes.txt`; a smith (forge), alchemist (alchemy) or tinker (workbench) in each town opens
  the crafting screen with tabs **Craft / Reinforce / Enchant / Salvage**. Each station has its own skill that
  rises as you craft; hard recipes can fail (losing half the materials) and better skill rolls **fine** and
  **masterwork** results with extra affixes. Each recipe needs a minimum station skill (`skill=`). Many recipes
  need a **formula** (found, bought or rewarded - the master crafters' trials teach mithril, riftsteel and the
  rare brews). Dialogue `craftxp=station:n` gives practice.
  Gold is scarcer and shops stock limited gear that restocks every few days; buy/sell work in stacks
  (Left/Right, hold to accelerate, E for max). **Loot tiers**: equipment from chests and kills can roll affixes from `affixes.txt`
  ("Keen Iron Sword of Burning"); uncommon = one affix, rare = two, with coloured names.
* **World map** (M, or the pause menu): discovered locations with quest (yellow) and bounty (cyan) pins. The map
  itself is just a map; to **travel**, talk to the **coachman** in any town square: he takes you to any place you
  have visited for a fare (8 gold + 6 per league). Travel takes game time and can be interrupted by an ambush.
* **Journal** (J): quests with "show on map", bounties, a **bestiary** that fills in kills and reveals
  resistances as you fight each creature, play stats, notes picked up from signs and lore files, achievements.
* **Bounties**: each town's notice board posts a few offers per day; take up to 3 at a time, drop one from the
  journal (E), hand them in at the board. Courier jobs give you a letter and send you one way only.
* **Achievements**: `achievements.txt` (conditions over stats and flags), unlocked across all save slots,
  popped up Win98-style and listed in the desktop Achievements window. With a Steam build they are forwarded
  to Steam (see `STEAM.md`).
* **Dev console** (`, debug builds only: `build.ps1 -Dev`): `help` lists everything: teleport, give, gold, xp, level, flag, god, heal, time,
  weather, spawn, kill, join, learn, approve, achieve, reveal, stats, shop, dialogue, run, difficulty,
  auto (lets the AI fight for the party - handy for balance testing)...
* **Desktop toys**: Minesweeper, Solitaire, My Computer (lore files in `assets/data/lore`), and a starfield
  screensaver after 90 seconds idle.

## The editor (Editor)

Double-click **Editor** on the desktop. It runs at 960x720 and edits the files under `assets/data`:

* **NPCs** tab (1.1.9) - every NPC once (`npcs.txt`): a Look card, pickers for each field, and a box with every map
  it stands on (click to open it there), what talking to it does, how a companion joins and its problems. **Place on
  map...** and **Rename id...** (follows into every map and `join=`). The Maps tab's Objects tool places NPCs from a list.
* **Guide** - **? Guide** in the header (or **F1**) opens a built-in manual: every tab, every field of every data
  file, every map object key, conditions, scripts, flags, encounters and the world map, with a searchable topic list,
  Back and "see also" links. Small **?** buttons sit on every tab and next to most fields and open the guide on that
  exact page (e.g. Achievements has one for the tab and one each for id, name, desc, cond, tile, hidden). In the map
  header, click a field's name. The text lives in `assets/editor_guide.txt` and can be edited like any data file.
* **Flags** tab - every flag the campaign uses, gathered from dialogue (`set=`, `flag=`, `inc=`, `clear=`, option
  conditions), map objects (chest / enemy / trigger / door / obstacle / lever `flag=`, `show` `hide` `moveif` `openif`
  `require` `if`, scripts), quest stages, dungeon boss flags, achievements, banter, locations and the campaign's starting
  flags. Each flag lists **Set by** and **Checked by** (click to jump to the dialogue line / map object / entry), flags
  that are checked but never set are shown first in red, and you can write a note per flag (saved in `flags.txt`).
  Flag and condition fields elsewhere get a **flag** / **+ flag** picker, and hovering a condition reads it in English.
* **Quests** - stages are broken out into cards: *shows when* (with a flag picker and a plain-English reading),
  journal text, DONE, map pin, move up/down; under each stage, where its flags are set. A **Preview** box lets you set
  flag values and see which stage the journal would show.
* **Encounters** - a "Where this table is used" panel: tile kinds whose biome rolls it, maps that roll it on those
  tiles, maps and dungeons with `table=`, the night/day pairing, and an **(unused)** marker in the list. The Maps header
  `table` and the Tiles tab `biome` have pickers.
* **Check all** (header) - checks the whole campaign at once: every map, dialogue file, flag, quest, shop, class,
  creature, recipe, encounter table, dungeon and location for references to things that don't exist (and a few
  warnings, like unused tables). Click a finding to jump to it.
* **Used by** - Items, Creatures, Spells and Shops show every place that uses the selected entry (chests, shops, loot,
  recipes, dialogue, encounter tables...), and the Dialogue tab shows which NPCs and scripts open the file.
* **Rename a flag everywhere** - from the Flags tab; rewrites and saves every dialogue, map, quest, dungeon,
  location, achievement, banter line and the campaign start that use it.
* **Bounties** tab - `bounties.txt`: each notice board (`[board]`: name, map, notices per day, kinds, the dungeons /
  creatures / items it draws from, which boards its courier letters may go to, reward %) and hand-written notices
  (`[bounty]`: a fixed kill / fetch / deliver job with your own title and text, optional condition and repeat).
  Courier letters now only go to real boards whose town you have visited; without a bounties.txt every `board=` in the
  maps is found automatically. Problems (a board no NPC opens, letters to something that isn't a board) show beside
  the entry and in Check all.
* **Items**: `quest=1` marks a quest item that can't be dropped or sold (every `type=key` item is one by default),
  `unique=1` keeps an item free of random affixes (loot, crafting, enchanting), `color=r,g,b` (1.1.8) draws its picture as a white shape in that colour, and `category=` files an item under its own heading. The party menu's Items tab and shops show category chips (T / RB or
  click) and a sort order (R / Back: as found, name, value, category).
* **Undo** in every form tab: Ctrl+Z / Ctrl+Y (or the Undo button).
* **Quest objectives** - `objective=STAGE | DONE-CONDITION | TEXT` adds a checklist line under a stage in the journal
  (counters like `{itemn:herb_moon}` update live); the Quests tab cards have "+ objective".
* **World** - the overworld preview now updates live (moving or editing locations, painting `maps/overworld.txt` in the
  Maps tab, even before saving).

* **Maps** - paint tiles (paint / fill / rect), place and edit objects (NPCs, enemies, chests,
  exits, triggers, signs, doors, save points), resize, check for problems, save. Right-click picks
  the tile under the cursor; middle-drag, wheel or WASD scroll; Delete removes the selected object.
  The **Select** tool drags a region: Ctrl+C / Ctrl+X copy or cut it (tiles and objects), Ctrl+V
  then click stamps it (shift-click keeps stamping), Delete clears it with the brush, Ctrl+A selects
  all. Ctrl+Z undoes any map edit, Ctrl+S saves.
* Every text field supports mouse / Shift selection, double-click select-all and Ctrl+A/C/X/V with
  the system clipboard. The dialogue editor adds multi-line selection, Ctrl+Z undo and Ctrl+D
  (duplicate line).
* **Arrival points**: the map view marks the spawn (S) and where each overworld location drops the party (A).
  Press **N** over a tile to make it the overworld arrival point; *Check* warns when the arrival is far from the
  exit back out.
* **Map <-> Tiles**: in the map view press **K** over a tile (or click *edit* next to the brush) to jump to
  that tile kind in the Tiles tab; *< Back* or Esc returns to the map.
* **Play >** in the map header (or **P** over a tile) saves everything, leaves the editor and drops the
  party onto the open map at the spawn point (or under the cursor). With no game running a level-1
  tester with 500 gold is created; a running game is moved there instead.
* **Campaign** (header button): pick which campaign you are editing, or **New campaign**. Each campaign is a separate
  game: the editor shows only that campaign's own maps, dialogue, items, monsters and so on, never the base game's.
  A new campaign starts with a starter map, an intro dialogue, a quest and a copy of the base game's classes, spells,
  items, monsters, recipes and affixes, so it is playable at once; that copy belongs to the campaign from then on.
  Tiles, sprites and sounds are shared by every campaign.
* **Campaign tab**: the campaign's name, description and desktop icon; **where a new game begins** (start map and
  tile); the intro cutscene lines; the dialogue that opens after the intro; starting gold, items and flags; and what
  the **world map (M)** shows (see Campaigns).
  *Play from the start* saves and opens New Game for the campaign.
* **New-game start**: the Maps tab marks the start tile with a yellow **START**. Point at a tile and press **B** to
  make new games begin there; *Check* warns if it sits on a blocked tile.
* **World > Bake to paintable map** turns the overworld into `maps/overworld.txt`; from then on it's painted
  in the Maps tab like any other map (locations still come from `overworld.txt`).
* Map objects also include obstacles, levers, changes and gather nodes (with templates).
* **Dungeons > Bake map** turns a generated dungeon (with the current seed) into `maps/<id>.txt` and opens
  it in the Maps tab. A map file with a dungeon's id takes priority over the generator, so you can hand-edit
  a dungeon from then on (delete the file to go back to generating it).
* **Dialogue > Check / Check all** lints dialogue files: missing `[start]`, options and `next=` that lead
  nowhere, unknown items/creatures/classes/shops/maps, flags that nothing ever sets, unreachable nodes,
  dialogues no NPC uses. Click a finding to jump to its line.
* **Recipes, Affixes, Banter, Achievements** - form editors over the new data files.
* **Sounds** - every sound event and music track the game knows about: change which file(s) it plays,
  volume, random pitch, looping, preview it, add new events or tracks, import your own .wav/.ogg/.mp3
  (copied into `assets/audio`). Items, spells and creatures get a `sound=` field (creatures also
  `attacksound=`), maps and dungeons a `music=` field, all with pickers. Stats, resistances, damage ranges and colours are number fields; types, AI,
  targets and the like are dropdowns; sprites and tiles have a picker; loot, spells, shop stock,
  encounter groups and roads are built with "+ add" pickers.
* **Tiles** - every tile kind: ground tiles, overlay prop, walkable / blocks sight / water,
  tint and background colour, encounter weight and biome, move cost. This is where you fix a
  wrong door sprite: pick `door_wood` and change its prop.
* **World** - the overworld preview with its locations; click to move one, edit its fields.
* **Items, Creatures, Spells, Classes, Shops, Encounters, Quests, Dungeons** - form editors over
  the data files (tile/sprite fields have a picker, colours show a swatch).
* **Dialogue** - a text editor for dialogue files with a syntax cheat-sheet.
* **Tileset** - browse the sheet and read off tile coordinates.

Saved changes are reloaded by the game when you leave the editor (a running game re-enters its
current map). All shipped dungeons and the overworld are baked map files; a dungeon or overworld with no
map file still falls back to the generator.

## Building from source

Requirements: CMake 3.16+, a C++17 compiler, git (raylib 5.5 is fetched automatically).

**Without the editor (release build):**

    build_windows.bat release          (Visual Studio)
    build_windows.bat mingw release    (MinGW-w64 / w64devkit)

or with CMake directly (use a separate build folder so the editor build's cached settings aren't reused):

    cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DTRIFFALL_EDITOR=OFF
    cmake --build build-release --config Release --target Triffall

The exe lands in `build-release\Release\` (Visual Studio) or `build-release\` (MinGW) with `assets\` copied next to
it. That build has no Editor icon or start-menu entry and none of the editor code; the game still reads the same
data files, so anything you made with the editor ships as-is. Campaign folders in `assets\campaigns\` still get
their desktop icons.

**With the editor (development build):**

    build_windows.bat          (Visual Studio)
    build_windows.bat mingw    (MinGW-w64 / w64devkit)

or on any platform:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

**Windows from Linux or macOS** (cross-compiling with llvm-mingw; this is how the 1.0 exe was built):

    cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=tools/mingw-toolchain.cmake -DLLVM_MINGW=/path/to/llvm-mingw
    cmake --build build-win --target Triffall -j

**The demo:** add `-DTRIFFALL_DEMO=ON` (or `build_windows.bat demo`); see STEAM.md "The demo".

**Browser build (itch.io):** install [Emscripten](https://emscripten.org) (emsdk) and Ninja, load `emsdk_env`, then

    emcmake cmake -S . -B build-web -G Ninja -DTRIFFALL_DEMO=ON
    cmake --build build-web

Zip `index.html`, `index.js`, `index.wasm` and `index.data` from `build-web` and upload the zip to itch.io as an
HTML game. The browser build uses `src/platform/main_web.cpp` and `web/shell.html`; the game code is the same.
Saves live in the browser (IndexedDB). Browsers keep Esc for leaving fullscreen, so in the browser Backspace works as
Esc too (except while typing a name). On itch, set the embed's viewport to fit the page (960 x 720 suits the default
page width) and tick "Fullscreen button".

Targets: `Triffall` (the game) and `triffall_headless` (a scripted test driver that
renders frames to PNG without a window - see `tests/`).

## Project layout

    src/core/      software renderer (gfx), input, utils
    src/game/      data loading, world/maps, procedural generation, exploration,
                   combat, dialogue, menus, save/load
    src/desktop/   the Windows 98 desktop shell
    src/editor/    Editor (edui.h widgets + editor.cpp)
    src/platform/  raylib shell (main_raylib.cpp + rl_shim.c) and the headless driver
    assets/        tiles.png (Urizen 1-bit tileset), icons.png, and data/
    assets/data/   items, spells, creatures, classes, shops, encounters, quests,
                   dungeons, overworld locations, maps/ (ASCII maps), dialogue/
    tools/         mkmaps.py (generates the town maps), kindview / owview / mapcheck dev tools
    tests/         scripts for the headless driver

## Campaigns

A campaign is a separate game in `assets/campaigns/<id>/`: `campaign.txt` plus its own data files, in the same
layout as `assets/data` (`maps/`, `dialogue/`, `items.txt`, `creatures.txt`, `classes.txt`, `spells.txt`, `shops.txt`,
`quests.txt`...). It uses only its own folder; tiles (`tilekinds.txt` can be added to change or add tile kinds),
sprites, sounds and the My Computer lore are shared. Each campaign gets its own **desktop icon**; double-clicking it
opens New Game for that campaign, and saves remember which campaign they belong to.

The easiest way to make one is the Editor's **Campaign > New campaign**, then the **Campaign tab**. `campaign.txt`:

    [campaign]
    name=The Lighthouse Keeper
    desc=A short adventure on the storm coast.
    icon=3,4            ; icons.png cell
    start=lighthouse,12,20  ; where a new game begins: map,x,y (0,0 = that map's spawn) - B in the Maps tab sets it
    intro=...           ; opening cutscene, one line per screen ($hero = the hero's name)
    dialogue=keeper_intro ; dialogue that opens after the intro
    items=torch_item:2,potion_red:2
    gold=40
    flags=some_flag
    worldmap=current    ; what M shows: overworld (default) | off | current (automap of the map you're on) | a map id

Order on a new game: character creation -> intro lines -> the party appears on the start tile -> the opening dialogue.

**World map for indoor campaigns.** A campaign that never goes outside would leave the overworld map blank, so
`worldmap=` picks what M shows: `off` (no world map; the pause menu entry is greyed and the journal hides "show on map"),
`current` (an automap of whatever map the party is on), or a map id (that map always - e.g. a castle's great hall; the
places listed are its exits, named after where they lead, and quest pins may name a map id). To fill a map in without
walking it, dialogue and scripts can use `revealmap=<map>` (all of it) or `revealmap=<map>,x,y,radius`, and an overworld
location with `reveal=N` uncovers the overworld within N tiles of itself when its map is entered. Coach travel always
uses the overworld. A map's own header can override all of this with `worldmap=self|off|overworld|<map id>`
(a sewer level that shows the sewer hub, say), and a consumable with `effect=revealmap:<map>[,x,y,r]` is a map the
player can buy or find (the base game's Surveyor's Map at the Brasswick market reveals the overworld).
`inherit=1` (advanced) makes a campaign build on the base game instead, taking any file it lacks from `assets/data`.

**Tutorial hints** (`hints.txt`, 1.0.8): a list of `[hint]` blocks with `if=` (a condition), optional `title=` and
`text=`; the last one whose condition holds is shown in the Training panel and stays until it stops holding - quest
stages for teaching. Extra conditions for it (usable anywhere): `mode:explore|combat|shop|menu|journal|levelup|worldmap`,
`equipped:<item>`, `fav:<item or spell>` / `fav:any`, `map:<id>`, `lit` (1.1.7: a torch, light spell or light-giving
gear is on). `{interact}`, `{menu}` and the other action names
become the key (or pad button) in use - in hints, dialogue, signs and messages. `finish=<campaign>|line|line` in dialogue
ends a campaign without the epilogue: closing lines, then the desktop with New Game open for that campaign.
`order=` sorts the campaign icons (the Tutorial is 0) and `achievements=0` keeps achievements from unlocking.
`defeat=<script>` (1.1.7; Campaign tab, "If the party falls") keeps a lost fight from ending the game: the party gets
back up at full health, the enemies in that fight are whole again, and the script runs (the Tutorial opens a dialogue
that puts the party somewhere safe).

`assets/campaigns/tutorial` is the Tutorial, built only from these pieces (three maps, eight dialogues, a shop, a quest
with a checklist, `hints.txt`, four items and one monster). `triffall_headless bakeall` bakes every generated
dungeon/overworld to map files.

## Modding

Almost everything is plain text:

* `assets/data/maps/*.txt` - ASCII maps with a legend; objects (NPCs, exits, chests,
  enemies, signs, doors, save points, triggers) are listed below the rows.
* `assets/data/dialogue/*.txt` - dialogue trees: `[node]` blocks with `speaker`, `text`,
  `option=Label|target|condition` and actions (`set`, `give`, `take`, `gold`, `xp`, `join`,
  `shop`, `combat`, `teleport`, `heal`, `rest`, `message`, `cutscene`, `revealmap`...).
* `hidenpc=id` now keeps the NPC hidden for good (saved); `shownpc=id` brings it back.
* `assets/data/npcs.txt` (1.1.9) - the NPCs: name, `sprite`, `color`, `dialogue`, `shop`, `craft`, `board`, `travel`,
  `hostile`, `wander`, `notes`; companions add `companion=<class>`, `partysprite`, `level`, `stats`, `gear`, `spells`.
  A map `[npc]` object is a placement: `npc=<id>`, `x`, `y`, its own `id` on that map, `show`/`hide`/`moveif`; any
  other key it sets wins over the NPC's. Dialogue adds a companion with `join=<npc id>`.
* `assets/editor_guide.txt` - the Editor guide (`== topic.id | Title`, `see: a, b`, then the text).
* `assets/data/dungeons.txt` - procedural dungeon definitions (style, size, monsters, boss).
* `assets/data/overworld.txt` - where locations sit on the overworld and which roads link them.
* `assets/data/tilekinds.txt` - the tile kinds (what each map cell type looks like and does).
  Delete it to regenerate the built-in defaults.
* `assets/data/recipes.txt`, `affixes.txt`, `banter.txt`, `achievements.txt`, `lore/*.txt` - crafting, loot affixes,
  companion lines, achievements and the My Computer files. Each has a format comment at the top.
* `assets/data/bounties.txt`, `factions.txt` - notice boards and hand-written bounties, and the towns the party has a
  reputation with. A campaign's `hints.txt` is tutorial guidance (see Campaigns).
* `assets/data/sounds.txt` - sound events and music tracks (`[sound]` / `[music]` blocks with `id`, `file`,
  `volume`, `pitchvar`, `loop`, `desc`). Delete it to regenerate the defaults.
* Tile art is referenced by (column,row) in the tileset; `src/game/tiles.h` names the ones the
  code uses directly.

## Audio

All default sounds and music are synthesized by `tools/mksounds.py` (numpy + ffmpeg) into `assets/audio/sfx/*.wav`
and `assets/audio/music/*.ogg`; re-run it to regenerate them. `assets/data/sounds.txt` maps event ids to files,
so replacing a sound is either dropping a new file over the old one or pointing the event at a different file in
the Sounds tab. Several files on one event are picked at random; `pitchvar` adds random pitch.

Music is picked automatically: `title` on the desktop, `overworld`, `town` (kind=town/interior), `dungeon`,
`tech` (tech/ship dungeons), `forest` (other maps), `combat` / `boss` during fights, `gameover`; a map's `music=`
or a dungeon's `music=` overrides it. Volumes come from the desktop Settings sliders.

## Credits

Art: [Urizen 1-bit tileset v2.0](https://vurmux.itch.io/urizen-onebit-tilesets) by vurmux (CC0) and
[Cute Desktop Icons 32x32](https://mayke-arth.itch.io/cute-desktop-icons-32x32) by Mayke-Arth. Built on raylib.
Sound and music are generated by `tools/mksounds.py`.
