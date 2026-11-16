# Triffall image kit

Pictures for Triffall's public pages (itch.io, Steam, social links, press). Every picture is made from the game's
own pixels and letters, scaled up by whole numbers (no blur, no painted or AI-made art): the TRIFFALL logo is the
boot screen's lettering, the text is the game's own font, the window and taskbar are the game's Windows 98 desktop.

**Tagline:** An old-school tactical RPG hiding inside a Windows 98 desktop.
**Tags:** RPG, turn-based, tactical, open world, party-based, pixel art, retro, Windows 98, fantasy, sci-fi, crafting, moddable

Store and page text: [STORE-TEXT.md](STORE-TEXT.md). Rights and credits for the art, music and sounds: [RIGHTS.md](RIGHTS.md).

## Files

| File | Size | Use |
|---|---|---|
| `cover.png` | 630x500 | itch.io cover (the thumbnail on browse pages); also the project cover in Idea Jotter |
| `art/itch/banner_960x300.png` | 960x300 | itch.io banner (header at the top of the page) |
| `art/icons/icon_16.png` | 16x16 | icon |
| `art/icons/icon_32.png` | 32x32 | icon |
| `art/icons/icon_48.png` | 48x48 | icon |
| `art/icons/icon_64.png` | 64x64 | icon |
| `art/icons/icon_128.png` | 128x128 | icon |
| `art/icons/icon_256.png` | 256x256 | icon |
| `art/icons/icon_512.png` | 512x512 | icon (large) |
| `art/icons/app.ico` | 16-256 | app / window icon and favicon (all six sizes in one file); the game's exe uses the same picture |
| `art/social_card_1200x630.png` | 1200x630 | social card (link previews on Discord, Bluesky, X...) |
| `art/avatar_512x512.png` | 512x512 | square avatar (profile pictures, small tiles) |
| `art/background_1920x1080.png` | 1920x1080 | itch.io page background / press-kit wallpaper (the game's desktop) |
| `art/key_art_1920x1080.png` | 1920x1080 | key art with the tagline (press, trailers) |
| `screenshots/native/01_combat.png` | 960x540 | screenshot: a fight (hit and crit chances, cover) |
| `screenshots/native/04_overworld.png` | 960x540 | screenshot: the open world |
| `screenshots/native/06_dialogue.png` | 960x540 | screenshot: talking to Elder Mara in Hollowmere |
| `screenshots/native/11_launcher.png` | 960x540 | screenshot: the Windows 98 desktop and the Triffall launcher |
| `screenshots/native/16_editor_maps.png` | 1280x720 | screenshot: the built-in Editor (it draws at 1280x720 on a 1920x1080 screen) |
| `screenshots/16x9_1920x1080/*.png` | 1920x1080 | all 17 store screenshots, 2x (combat first) |
| `art/steam/header_capsule_920x430.png` | 920x430 | Steam header capsule |
| `art/steam/small_capsule_462x174.png` | 462x174 | Steam small capsule |
| `art/steam/main_capsule_1232x706.png` | 1232x706 | Steam main capsule |
| `art/steam/library_capsule_600x900.png` | 600x900 | Steam library capsule |
| `art/steam/vertical_capsule_748x896.png` | 748x896 | Steam vertical capsule |
| `art/steam/library_hero_3840x1240.png` | 3840x1240 | Steam library hero (no logo: Steam lays the library logo over it) |
| `art/steam/library_logo_1280x720.png` | 1280x720 | Steam library logo (transparent) |
| `art/steam/page_background_1438x810.jpg` | 1438x810 | Steam store page background |
| `art/steam/community_icon_184x184.jpg` | 184x184 | Steam community icon |
| `art/steam/client_icon_32x32.ico` | 32x32 | Steam client icon |

"Native" screenshots are the size the game draws on a 1920x1080 screen before it scales up to fill it (the game
draws at 640x480 or wider and scales by whole numbers). Steam capsules carry only the logo, no tagline (Valve's rule).

`old/` holds earlier art kept for reference: `old/ai-generated-0.9/` is AI-made and must not be used.

## Making them again

1. `.\build.ps1` (the screenshots come from the headless game).
2. `.\tools\make-presskit.ps1` (needs Python with Pillow).

`tools/make-presskit.ps1` runs `src/tools/presskit_shots.txt` in the headless game (960x540, the Editor at
1280x720) and then `src/tools/promo_kit.py`, which lays out every picture above. Change the layouts, the tagline or
the icon (a 16x16 pixel grid, `ICON16`) in `promo_kit.py`. It also writes the game's own icon
(`src/src/platform/app.ico`, built into the exe) and the browser build's favicon (`src/web/shell.html`).
