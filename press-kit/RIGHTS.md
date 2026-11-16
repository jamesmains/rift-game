# Triffall - rights check (1.3.0, 2026-11-14)

What every third-party or generated piece of the game and its store material is, what licence it has, and what is
still open. Keep it up to date when an asset comes in or goes out.

## In the game

| What | Where | Licence | Status |
|---|---|---|---|
| Urizen 1-bit tileset v2.0 by vurmux | `src/assets/tiles.png` | CC0 | Clear. Credit given anyway. |
| Cute Desktop Icons 32x32 by Mayke-Arth | `src/assets/icons.png` | No written licence (see below) | **Open: get a written OK** |
| Pixel Fantasy 30 Tracks and Loops by AlkaKrab | `src/assets/audio/music/pixel_fantasy/` | Purchased royalty-free licence (PDF in the pack zip) | Clear for the game. See the music notes below. |
| Spleen font by Frederic Cambus | the Clear font | BSD 2-clause, `src/assets/LICENSE-spleen.txt` | Clear |
| raylib | engine | zlib | Clear |
| Pixel Combat SFX by Helton Yan (1.3.3) | `src/assets/audio/sfx/pixel_combat/` | CC BY 4.0 (heltonyan.itch.io/pixelcombat, checked 2026-11-15) | Clear. Credit required: given (end credits, About, README). Converted/trimmed, and since 1.3.4 played through a lo-fi filter in the game (CC BY allows changes; README says so). |
| Other sound effects | `src/assets/audio/sfx/*.wav` | Made by `src/tools/mksounds.py` (plain synthesis, no AI) | Clear (own work) |
| `src/assets/new-art/*.png` | not shipped (build.ps1 leaves it out) | Unknown | Check before using it |
| The owner's "Test" campaign (EverRogue tileset) | `src/assets/campaigns/test/` | Unknown | Kept out of git and out of every hand-out build |

### Desktop icons (Mayke-Arth)

Checked 2026-11-14 on https://mayke-arth.itch.io/cute-desktop-icons-32x32 : the page has no licence field. The
description says "Feel free to use it on your computer, phone, or anything else". In the comments the author wrote
"I don't think I put a license in the readme file, but it is Creative Commons", "So, Creative Commons, I think?",
"feel free to use the assets however you want in your projects", "do what you want, just don't resell it (since
it's free!)" and "if you want to mention me in your project, I will be glad! But it's not necessary!".

So commercial use in a game looks allowed and reselling the icons themselves is not, but there is no written licence
that a store or a lawyer would accept. The only contact is itch.io (comments on the page, or the profile). Suggested
message to post (the owner sends it, from their own account):

> Hi Mayke! I'm using your Cute Desktop Icons 32x32 in Triffall, a commercial RPG for Steam and itch.io (the icons are
> on its retro PC desktop; you're in the credits). Could you confirm in writing that I may use the icons in a
> paid game, as part of the game only (not resold as icons)? A reply here is enough. A licence line on the page
> (for example CC BY 4.0) would help everyone else too. Thank you for the lovely icons!

Keep a screenshot of the reply in `press-kit/permissions/`. If there's no answer before the Steam page goes live,
replace `src/assets/icons.png` with icons you own (same 32x32 grid, same cells, so nothing else has to change).

### Music (AlkaKrab)

The licence (PDF in `assets/packs/Pixel Fantasy 30 Tracks and Loops Music Pack.zip`): royalty-free, commercial use
in games allowed, credit appreciated but not required; **no redistribution of the tracks as they are**, and open-source
games need the author's permission. So:
- The .ogg files are not in git (`.gitignore`); `tools/unpack-music.ps1` (run by build.ps1) unpacks them from the zip.
  If this repository is ever made public, the music stays out of it.
- Shipping them inside the game (the download, the browser demo's packed data) is the use the licence allows.
- Don't upload the tracks on their own (a soundtrack download, Spotify and the like) without the author's OK.
- The game credits AlkaKrab (end credits, About, README).

## Store and marketing material

| What | Where | Status |
|---|---|---|
| Key art, Steam capsules, hero, logo, icons, itch cover/banner/social | `press-kit/art/` | Made by `src/tools/promo_kit.py` from the game's own screenshots and logo. No AI. |
| Store screenshots | `press-kit/screenshots/16x9_1920x1080/` | Real game screenshots (headless game, scaled 2x). |
| 0.9 key art, cover, banner (glossy gold logo) | `press-kit/old/ai-generated-0.9/` | **AI-made. Do not use** on a store page, or answer Steam's AI disclosure for it. |
| 0.9 promo kit | `press-kit/old/0.9.0/` | Made by the old promo_kit.py from 4:3 screenshots; out of date, not AI. |
| `triffall-art-gemini-generated-image-...jpg` | Idea Jotter's files for this idea (Art) | AI-made (Gemini). Not in the project; don't use it on a store page. |

Steam's content survey: with the material above, the honest answer is "no AI-generated content" (see
`press-kit/STORE-TEXT.md`). That changes if any AI-made picture, text or sound is used.

## The name "Triffall"

Searched 2026-11-14:
- Steam store: no game called Triffall or Trifall. Near names: Trifox (2022), Trifoil (puzzle game).
- itch.io: nothing with the name.
- Web search: no game, app, company or trademark named Triffall.
- Trademarkia (a mirror of the USPTO register): "triffall" 0 results; "trifall" one near match, TRIFALLA (US
  78552371, class 5, herbal supplement), dead since 2013. Other near marks found: TRIFFA (India, class 2),
  Triffit, Inc. (US, class 9), TRIFFT (cancelled), TRIFF AG (US class 15, cancelled 2000).

**Not searched (these sites need a browser):** the USPTO's own search (https://tmsearch.uspto.gov/), EUIPO / TMview
(https://www.tmdn.org/tmview/), WIPO Global Brand Database (https://branddb.wipo.int/) and the UK IPO. Nothing found
so far suggests a conflict in class 9 (software, games) or class 41 (entertainment), but this is not a clearance:
search those three by hand for "Triffall" and "Trifall" in classes 9, 28 and 41 before the store page goes up, and
consider a trademark attorney's search if the budget allows. Registering the mark (a few hundred dollars per class at the USPTO) is optional for a
small release.
