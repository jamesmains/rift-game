# Triffall - store text (as of 1.6.2)

Text for the Steam and itch.io pages, checked against the game as it is at 1.6.2 (first written at 1.3.0). Pictures: `press-kit/art/` (key
art, capsules, itch cover and banner) and `press-kit/screenshots/16x9_1920x1080/` (combat first). Remake both with
`.\tools\make-presskit.ps1` after a build.

What changed from the old text: no "bonus campaign" (The Gullwatch Light was removed at 1.1.1; the Tutorial is the only
extra campaign), the music line (it's now the Pixel Fantasy soundtrack, not generated), wide-screen support, the size
on disk, and the item count. The old key art (AI-made) is no longer used - see `press-kit/RIGHTS.md`.

1.6.2 (#136): the Steam capsules and the library logo carry only the art and the name - **no tagline** (Valve's capsule
rules allow only artwork, the game's name and an official subtitle; a marketing line can cost sale and event
eligibility). The tagline below is for the page text, the key art and itch.io only. The screenshots were re-shot at
1.6.2 (the 1.5 combat UI: hit and crit chances in the target box, IMMUNE / WEAK! labels; the 1.6.1 desktop and
launcher). The desktop no longer has lore files or Field Notes (#137), and the game has one difficulty (1.5.0).

## Name and tagline

Not for Steam capsules (see above).

**Triffall** - An old-school tactical RPG hiding inside a Windows 98 desktop.

## Short description (Steam, 300 characters max - this is 269)

Forty years after the sky tore open, the Rift is waking again. Lead a party of four across an open world of towns,
dungeons and pieces of other worlds, in X-COM / Fallout-style turn-based fights fought right where you stand. It all
starts on a faked Windows 98 desktop.

## About this game

Forty years ago the sky over Aldmoor tore open. When it closed, pieces of other worlds lay scattered across the land:
a city of glass and asphalt, a starship, machines that still hum. Now the Rift is waking again - and your village
elder has sent for you.

Triffall is an open-world RPG with X-COM / Fallout-style turn-based tactical combat, fought right where you stand.
Build a party of four, multiclass across five classes, light your way through dark dungeons, craft and enchant gear,
and make three story choices that decide how it ends.

And it all starts on a lovingly faked Windows 98 desktop - Minesweeper and Solitaire included.

### Features

- Turn-based tactical combat with action points, cover, flanking, overwatch and aimed shots
- Swords, bows, guns, lasers, spells and grenades; 50+ monsters and 200+ items
- 5 classes you multiclass into rank by rank, and 5 recruitable companions with banter, approval and personal quests
- A hand-made overworld, 6 towns, dungeons and an extra chapter
- A 7-stage main quest with three choices and three endings; 20 side quests and town bounties
- Darkness that matters: torches, lanterns, light spells and enemies that see in the dark
- Deep crafting with skill, failure, fine and masterwork results, enchanting and salvage
- Day and night, weather, shop hours, town reputation and fast travel by coach
- A Windows 98 desktop with its own Triffall launcher, Minesweeper, Solitaire and a screensaver
- A Tutorial campaign that sets up the story, and a built-in Editor for making your own campaigns
- Fills any screen shape, wide monitors included (a classic 4:3 mode is in Settings)
- Keyboard, mouse and gamepad, fully rebindable; one deliberately hard difficulty; 33 achievements
- Accessibility: large text, clear font, text speed, colour-blind colours, reduced flashing
- Soundtrack: the Pixel Fantasy music pack by AlkaKrab

### For modders (bottom of the page)

The Editor that made the game ships with it: maps, tile kinds, items, monsters, spells, classes, shops, quests,
dialogue (with a node graph), scripts, NPCs and sprite sheets. A new campaign starts empty and imports what it needs
from the base game.

## Demo

The free demo is the first chapter of the base game (Hollowmere, Millbrook, Greywood Cave, Hesk's Barrow, Brasswick
and the Hermit's Hut) plus the full Tutorial. Saves carry over to the full game. In the demo's `assets/data/demo.txt`,
set `store=` to the full game's page once it exists: the demo desktop then shows a "Wishlist Triffall" icon.

## System requirements (Windows)

- OS: Windows 10 or later, 64-bit
- Processor: any x64 CPU
- Memory: 2 GB RAM
- Graphics: OpenGL 3.3
- Storage: 200 MB available space
- Sound: any

## Running the download (itch.io)

Unzip and run Triffall.exe with the assets folder next to it. F11 or Alt+Enter toggles fullscreen.

## Credits for the page

- Tiles: Urizen 1-bit tileset by vurmux (CC0)
- Desktop icons: Cute Desktop Icons 32x32 by Mayke-Arth
- Music: Pixel Fantasy 30 Tracks and Loops by AlkaKrab
- Sound effects: Pixel Combat by Helton Yan (CC BY 4.0)
- Clear font: Spleen by Frederic Cambus (BSD)
- Built with raylib

## Steam content survey: AI-generated content

Answer (as of 1.3.0): **No AI-generated content in the game or on the store page.** The game's art is the Urizen
tileset, the desktop icons are Mayke-Arth's, the music is AlkaKrab's, most sound effects are Helton Yan's Pixel Combat
(its page says no generative AI was used), the rest are made by a script (`src/tools/mksounds.py`, plain synthesis, no AI). The store art in `press-kit/art/` is made by a script from the
game's own screenshots and logo. The 0.9 key art, cover and banner were AI-made: they are in
`press-kit/old/ai-generated-0.9/` and must not be used. If any AI-made picture goes on the page later, the answer
has to change.

## Tags

RPG, Turn-Based Tactics, Party-Based RPG, Open World, Pixel Graphics, Retro, Fantasy, Sci-fi, Crafting, Level Editor,
Old School, Singleplayer

## Idea Jotter's brief (paste-ready, 1.6.2)

The project's brief in Idea Jotter still had store-facing lines from before 1.5 ("Easy / Normal / Hard", "130+ items",
"a bonus campaign", generated music, lore files in My Computer). Corrected text for its fields:

**Features** (one per line):

```
Turn-based tactical combat with action points, cover, flanking, overwatch and aimed shots
Swords, bows, guns, lasers, spells and grenades; 50+ monsters and 200+ items
5 classes you multiclass into rank by rank, and 5 recruitable companions with banter, approval and personal quests
A hand-made overworld, 6 towns, dungeons and an extra chapter
A 7-stage main quest with three choices and three endings; 20 side quests and town bounties
Darkness that matters: torches, lanterns, light spells and enemies that see in the dark
Deep crafting with skill, failure, fine and masterwork results, enchanting and salvage
Day and night, weather, shop hours, town reputation and fast travel by coach
A Windows 98 desktop with its own Triffall launcher, Minesweeper, Solitaire and a screensaver
A Tutorial campaign that sets up the story, and a built-in Editor for making your own campaigns
Keyboard, mouse and gamepad, fully rebindable; one deliberately hard difficulty; 33 achievements
Accessibility: large text, clear font, text speed, colour-blind colours, reduced flashing
Soundtrack: the Pixel Fantasy music pack by AlkaKrab
```

**More info**, first line: `Requirements: Windows 10 or later (64-bit), any x64 CPU, 2 GB RAM, OpenGL 3.3, about 200 MB.`
(the rest of that field is still right).

**Brief sections** with outdated lines:
- core, step 1: "the first launch runs a setup wizard" -> "the first launch runs a setup wizard that puts the Triffall
  launcher on the desktop".
- core, step 2: "From the desktop you start a New Game (base game, the Tutorial, or another campaign)" -> "From the
  Triffall launcher you continue, load or start an adventure (the main story, the Tutorial, or another campaign)".
- core, step 7: "lore files in My Computer, achievements" -> "achievements in the launcher".
- look: "Chiptune music per area with ambient loops (wind, rain, caves, town, machines)" -> "Music per area from the
  Pixel Fantasy pack by AlkaKrab; crunchy Pixel Combat sound effects by Helton Yan".
- nice: drop "Hand-made or replaced music and sound effects (current ones are generated)" (done at 1.3.0/1.3.3).
- assets: "campaigns (Tutorial, The Gullwatch Light)" -> "campaigns (Tutorial)".
- questions: "Do the generated music and ambient loops sound good by ear?" -> "Do the music picks and the crunched
  sound effects sound right by ear?"; "the project folder is riftfall" is no longer true (it's C:\Apps\Triffall).
- ask: "at 1.0.10" -> "at 1.6.2".
