# Getting Triffall onto Steam

What is already in the code, what Steam needs from you, and the order to do it in.
Nothing here requires a Steam account until step 2; everything in step 1 can be done today.

## 0. What the build already does

* **Achievements** are defined in `assets/data/achievements.txt` (33 shipped; the six 1.0 story ones are hidden until earned) and unlocked by the game
  itself, stored in `saves/achievements.ini`, shown in the Win98 popup and the Achievements window.
  Every unlock also goes through `PlatformServices::unlockAchievement(id)` (`src/core/platform_services.h`).
  The Steam implementation (`src/platform/steam_services.cpp`) forwards it to `SteamUserStats()->SetAchievement`.
  **The Steam-side achievement API names must equal the `id=` values in achievements.txt** (`first_blood`, …).
* **Steam overlay**: the game runs on OpenGL through raylib, which the overlay hooks fine. While the overlay is
  open the game freezes input and `dt` (see `main_raylib.cpp`), so nothing moves behind it.
* **Launch through the client**: `SteamAPI_RestartAppIfNecessary(appid)` is called first thing; a copy started
  from Explorer relaunches via Steam, which is what Valve expects.
* **Player name**: the New Game dialog pre-fills the Steam persona name.
* **Saves and settings** live in `saves/` next to the exe (nothing in AppData), which makes Steam Cloud a
  one-line auto-cloud rule (step 3).
* **Achievement progress**: an achievement whose condition is one counter (`kills:100`, `steps:10000`...) calls
  `indicateAchievementProgress(id, cur, max)` once, at the half-way mark; Steam shows its "50/100" toast. Steam
  only displays progress for achievements you also give a progress stat in Steamworks - without one the call is
  harmless and nothing shows.
* **Steam Deck keyboard**: on a controller, the New Game name field first asks Steam for its gamepad text input
  (`ShowGamepadTextInput`; it works on the Deck and in Big Picture) and reads the result from
  `GamepadTextInputDismissed_t`. Anywhere else Steam declines and the game's own on-screen keyboard opens.
  `handheld()` reports `IsSteamRunningOnSteamDeck()` for later use (bigger text, say).
* **Controller**: XInput through raylib, controller glyphs on all prompts when a pad is active, full rebinding.
  Steam Input in "Gamepad" default mode presents any controller as an Xbox pad, so nothing else is needed.
* **Steam-free by default**: with `TRIFFALL_STEAM` off the game has no Steam dependency at all (itch.io build,
  DRM-free build, your own site).

## 1. Before you have an account (do now)

1. **Pick the final name and check it.** Search the Steam store and the USPTO/EUIPO trademark databases for
   "Triffall". Steam does not police names, but a trademark owner can force a rename after launch.
2. **Decide the legal entity.** Steam pays the entity on the account (you as an individual works; a sole
   proprietorship or LLC is cleaner for taxes). You will need a tax ID (SSN/EIN in the US) and a bank account.
3. **Build the store assets** (all PNG/JPG; sizes are what Steamworks asks for as of 2026, double-check at
   upload time):
   * Header capsule 920x430, small capsule 462x174, main capsule 1232x706, vertical capsule 748x896,
     library capsule 600x900, library hero 3840x1240, library logo 1280x720 (transparent), page background
     1438x810, community icon 184x184 (jpg), client icon 32x32 (ico).
   * 5+ screenshots at 1920x1080 or 1280x720 (the game renders 640x480; capture at 3x = 1920x1440 and crop,
     or run windowed at 3x and screenshot — integer scaling keeps the pixels crisp).
   * A trailer (1080p+, 30 s to 2 min). Record at integer scale, pixel-perfect; no interpolation.
   * Short description (300 chars), long description, "About this game", system requirements
   (Triffall: Windows 10, any x64 CPU, 100 MB, OpenGL 3.3 — raylib's requirement).
4. **Write the content survey answers** you will be asked for: no gore beyond pixel blood, fantasy violence,
   no nudity, no gambling with real money (Solitaire/Minesweeper are fine), no user-generated content shared
   online (the editor edits local files only).
5. **Age rating**: Steam's questionnaire is self-service; ESRB/PEGI are not required.
6. **Playtest with strangers.** Two or three people who have never seen the game, watching them and taking notes.
   This is worth more than any feature on the list.

## 2. Steamworks account and app

1. Go to partner.steamgames.com, create a Steamworks account, pay the **$100 USD app fee** (recoupable once the
   game earns $1,000). Fill in the tax interview and bank details; they take a few days to verify.
2. Create the app. You get an **App ID**. Note it; it goes into `TRIFFALL_STEAM_APPID` and `steam_appid.txt`.
3. Download the **Steamworks SDK** (zip, no install). Extract it somewhere like `C:\steamworks_sdk`.
4. Build the Steam version:

      cmake -S . -B build-steam -DTRIFFALL_STEAM=ON -DSTEAM_SDK=C:/steamworks_sdk/sdk -DTRIFFALL_STEAM_APPID=<your app id>
        cmake --build build-steam --config Release

   The post-build step copies `steam_api64.dll` and writes `steam_appid.txt` next to the exe. Run it with the
   Steam client open and logged in; the overlay (Shift+Tab) should appear over the game. Delete
   `steam_appid.txt` from the files you upload — it is only for running outside Steam during development.
5. **Define achievements** under Steamworks > Stats & Achievements. For each `[achievement]` in
   `achievements.txt` create one with **API Name = id**, the same display name and description, and a
   64x64 icon pair (achieved / unachieved). `tools/achievement_icons.py` renders those from the tileset.
   Publish the change ("Publish" tab) or the API refuses to set them.
6. **Steam Input**: Steamworks > Application > Steam Input: enable "Gamepad" as the default configuration.
   Triffall reads XInput, which is exactly what that mode produces for any controller.

## 3. Depots, builds, cloud

1. Steamworks > SteamPipe > Depots: one Windows depot. Create the standard depot layout on disk:

      content/windows/Triffall.exe
        content/windows/steam_api64.dll
        content/windows/assets/...        (no saves/ folder, no steam_appid.txt)

2. Install `steamcmd`, write `app_build.vdf` / `depot_build.vdf` (templates are in `tools/steam/`),
   then `steamcmd +login <user> +run_app_build ..\scripts\app_build.vdf +quit`.
3. Set the build live on the **default branch** in SteamPipe > Builds. Add a `beta` branch for testers.
4. **Launch options**: executable `Triffall.exe`, OS Windows, type "Launch (Default)". Nothing else.
5. **Steam Cloud**: Steamworks > Cloud: enable Auto-Cloud, add one root: `AppInstallDirectory` path `saves`,
   pattern `*.sav` and `settings.ini` and `achievements.ini`, quota ~1 MB per user. That syncs the three slots.
   Leave `saves/exports` (Editor campaign zips) out of the patterns - they can be large and are not save data.
6. **Steam Deck**: the game is a 4:3 640x480 render scaled with letterboxing, full controller support, no
   launcher, no sign-in. Text size was the risk: 1280x800 only fits 640x480 at 1x with integer scaling, which
   would have shown 8-pixel text on a 7" screen. Since 1.0 integer scaling is only used when 2x or more fits;
   on the Deck the frame is drawn sharp at 2x and smoothed down to 1067x800 (1.67x), and Settings > Access has a
   Large text option for dialogue, messages, tutorial hints and the story. Things to check on a real Deck before asking
   for Verified: the New Game name field opens the Steam keyboard; every screen can be left with B; Large text
   is readable at arm's length; the game runs at 60 fps with the CRT effect off (and ideally on).

## 3b. The demo

Demos sell this genre, and Steam Next Fest is built around them. A Steam demo is its own app (Steamworks: "Create
a demo" on the main app page) with its own App ID, depot and store page that hangs off the main game's.

1. Build it: `build_windows.bat mingw demo` (or `cmake -DTRIFFALL_DEMO=ON`). The demo has no editor; the base
   campaign is limited to the maps in `assets/data/demo.txt` (Hollowmere, Millbrook, Greywood Cave, Hesk's Barrow,
   the hermit and Brasswick); anything further shows the "the road on is closed" message; when the Mayor sends you to
   Dunmarrow the demo's own ending plays and the player can keep exploring the demo area. Gullwatch plays in full.
2. Put the full game's store URL in `store=` in `assets/data/demo.txt` (the Wishlist icon on the demo's desktop opens it).
3. Saves are the full game's saves - a demo player's save loads in the full game, so ask Steam Cloud for the same
   paths on both apps.
4. For Next Fest: register in the Steamworks "Upcoming events" page well ahead (registration closes weeks before the
   festival), have the demo live before the press preview week, and plan one livestream.

## 4. Store page review and release

1. Fill in the store page, submit it for review (2-3 business days). You can't release until the page has
   been **public and "coming soon" for at least two weeks**, and the build has passed its review (another few
   days). Plan one month between "page submitted" and "launch".
2. Pricing: set in USD, let Steam suggest the regional prices. You can run a launch discount (up to 7 days).
3. Release date: pick a Tuesday-Thursday, avoid big launches. Press the release button yourself on the day.
4. After launch: the Steamworks "Events" tool for patch notes; `steamcmd` for updates (every upload is a full
   diff, players only download changed files, so the audio folder being 3 MB is irrelevant).

## 5. Things people get caught by

* Achievements set before `UserStatsReceived` arrives are dropped — the implementation waits for it.
* A game that writes next to its exe needs the install folder writable; Steam's library folders are, but
  "Program Files" is not. If you ever move saves, `SteamApps`' per-user data directory is the right spot.
* Keep the DRM-free build and the Steam build from the same commit; the only difference is the define.
* Patches and saves: a save written by 1.0 says `version=3` in its [game] section. If a patch renames an item, flag,
  map or spell, add a line to `assets/data/migrations.txt` so old saves follow the rename; if the save layout changes,
  bump TRIFFALL_SAVE_FORMAT in `src/core/version.h` and add the upgrade step to `Game::migrateSave`.
* Crash reports land in `saves/crash.log` next to the exe: ask players for that file (and `crash_recovery.sav`).
  `saves/session.lock` exists while the game runs; leave it out of the Steam Cloud patterns.
* The 1998 desktop joke: the store page should make it obvious within one screenshot that this is an RPG,
  not a desktop simulator. Lead with combat.
* Valve requires a working **exit** from inside the game (we have Shut Down) and no required third-party
  accounts (none).
