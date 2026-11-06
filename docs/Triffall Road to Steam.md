# Triffall: Road to Steam

Oct 22, 2026 · @Jim

## How to use this guide

This guide takes Triffall from a finished 1.0 on your drive to a game selling on Steam and being looked after, in eight phases you work through roughly in order. The phases overlap, and the calendar is set mostly by one thing: how long your store page is public before you launch, because that is when people wishlist it.

Each phase says what it is for, the steps, what the Triffall repo already does for you, and a **Done when** line. Steps happen in one of two places: on the **Steamworks** website (partner.steamgames.com, Valve's back office for developers) or in **the repo** (code, builds, scripts). The master checklist near the end repeats every step as a tick box.

&#91;embedded content: Triffall's road to Steam · 8 phases, suggested timing from October 2026\]

The timing above assumes you start this week and aim for the February 2027 Steam Next Fest. If the store page can't be public by mid-December, aim for the June 2027 Next Fest instead and push everything after Phase 4 back by four months. Nothing else changes.

### Words you will see

| Term | What it means |
| --- | --- |
| Steamworks | Valve's partner website and the SDK (code library) of the same name. |
| App ID | The number Steam gives your game. The demo gets its own App ID. |
| Package | What a customer buys: a bundle of one or more apps with a price. Steam makes one for you. |
| Depot | A set of files for one platform or language, such as "Windows game files". |
| Build | One upload of your depots. You set which build is live. |
| Branch | A named slot a build can be set live on. **default** is what players get; you can add a private beta branch. |
| SteamPipe | Valve's upload system. You drive it with steamcmd and the .vdf files in tools/steam. |
| Coming Soon | Your store page is public but the game can't be bought yet. People can wishlist it. |
| Wishlist | A player's "tell me when this comes out" list. Steam emails them on release day. |
| Next Fest | A week-long Steam festival of demos for unreleased games, held three times a year. |

## Phase 0: Decisions and paperwork

Before you touch Steamworks, settle who is selling the game and where the money goes, because Steam's forms ask for exactly that and the tax check behind them can take a month.

1. **Check the name.** Search Steam, itch.io, the app stores and your country's trademark register (USPTO TESS in the US, the UK IPO, EUIPO) for "Triffall". Also check the domain and the social handles you want. Renaming after the store page is up costs you everything you built under the old name.
2. **Decide who signs.** Steam needs the *legal name* of whoever signs the agreements, not a studio name you just like. You can sign as yourself (simplest) or set up a company first (an LLC in the US, a limited company in the UK). A company keeps the business separate from you; it costs money and paperwork every year. Whichever you choose, the bank account must be in **that exact name**. If you're unsure, ask an accountant for an hour; it's cheap compared with redoing tax forms.
3. **Get a tax ID ready.** US individuals use their SSN (or an EIN, free from the IRS, if you'd rather not hand out your SSN). Outside the US you fill in a W-8BEN-style form and may need your country's tax number to claim a treaty rate.
4. **Open or pick a bank account** that can receive international transfers in the signer's name.
5. **Pick a working price.** Look at 10 to 15 recent retro and old-school RPGs on Steam with a similar length (Triffall's target is 4 to 8 hours) and see where they sit. Write the number down now; you can change it until launch.
6. **Pick a target window, not a date.** "Spring 2027" is enough for the store page. The calendar in the diagram above is one realistic plan.
7. **Decide your public identity:** the developer name on the store page, a contact email that isn't your personal one, and how you want to be credited (credits.txt currently says *Jim Sims*).

**Done when:** you know who signs, the bank account and tax ID are in hand, and the name is clear.

## Phase 1: The Steamworks account

Signing up costs $100 per game and is mostly form-filling, but start now: the identity check on your tax details can take up to 30 days, and nothing goes live until it's done.

1. **Make a normal Steam account** for the business if you don't want to use your personal one. Turn on the Steam Guard mobile authenticator: this account will control your game and your money.
2. **Go to partner.steamgames.com and join Steam Direct.** You'll sign two agreements electronically: a Non-Disclosure Agreement and the Steam Distribution Agreement.
3. **Pay the app fee: $100 per product**, non-refundable. Valve pays it back to you once the game has made $1,000 in adjusted gross revenue. The demo is added from the full game's page (Phase 5), not bought as a separate product.
4. **Enter your bank details** (account number, routing or SWIFT/IBAN, bank address). The account holder must match the legal name from Phase 0.
5. **Do the tax interview.** It's an online questionnaire: US people get a W-9-style form, everyone else a W-8BEN-style form that claims any tax-treaty rate your country has with the US.
6. **Pass identity verification.** A third-party service checks your tax details; Valve says this "may take up to 30 days" and you can't change the details while it runs. Answer any request for documents quickly.
7. **Note the 21-day clock.** For your first titles there's a 21-day wait between paying the fee and being allowed to release. It starts when you pay, so paying early costs you nothing.
8. **Add anyone else** who will help (an artist uploading capsules, say) under Users & Permissions, with only the rights they need.

**Done when:** Steamworks shows your company page with verification complete and the app fee paid.

## Phase 2: Create the app and wire Triffall to Steam

Most of the Steam integration is already in the code behind the TRIFFALL\_STEAM switch; this phase is about filling in Valve's side and doing one real upload, so you learn the pipeline long before launch week. STEAM.md in the repo has the commands in full.

**Create the app**

1. In Steamworks, create the app. You get an **App ID** (a number). Write it down; it goes into the build.
2. Download the **Steamworks SDK** (a zip from the Steamworks site, no installer) and unzip it, e.g. to C:\\steamworks\_sdk.

**Build the Steam version** (repo)

3. Build with the switch on: `cmake -S . -B build-steam -DTRIFFALL_STEAM=ON -DSTEAM_SDK=C:/steamworks_sdk/sdk -DTRIFFALL_STEAM_APPID=<your App ID>`, then `cmake --build build-steam --config Release`.
4. Run it with Steam open and logged in. Shift+Tab should bring up the Steam overlay. The build writes a steam\_appid.txt next to the exe for testing; it must **not** be uploaded (the depot script already excludes it).

**Achievements** (Steamworks > Stats & Achievements)

5. Run `python3 tools/achievement_icons.py assets out/` to render all 33 icon pairs (64x64, achieved and unachieved).
6. For each entry in assets/data/achievements.txt, create an achievement whose **API Name is exactly the id** in that file, with the same name and description, and upload its two icons. Mark the six new 1.0 ones as hidden.
7. Press **Publish** in Steamworks. Unpublished changes don't exist as far as the game is concerned; this catches everyone once.

**Saves in Steam Cloud** (Steamworks > Cloud)

8. Turn on Auto-Cloud with one root: AppInstallDirectory, path saves, patterns `*.sav`, `settings.ini`, `achievements.ini`. Leave out saves/exports, crash.log and session.lock. Use the same rule on the demo app so demo saves carry over.

**Controllers** (Steamworks > Application > Steam Input)

9. Set the default configuration to **Gamepad**. Triffall reads XInput, which is what that mode hands it for any controller.

**Upload a build with SteamPipe**

10. In SteamPipe > Depots, note the Windows depot's ID. Put the App ID and depot ID into tools/steam/app\_build.vdf and depot\_build.vdf.
11. Lay out the files as content/windows/ (Triffall.exe, steam\_api64.dll, assets/; no saves folder, no steam\_appid.txt).
12. Get **steamcmd** (in the SDK's tools folder) and run `steamcmd +login <your account> +run_app_build <path>\app_build.vdf +quit`.
13. In SteamPipe > Builds, set the new build live on a private **beta** branch with a password, install it through your Steam library and play it. Only set builds live on **default** once the store page is approved.
14. Under Installation > General, add the launch option: executable Triffall.exe, Windows, "Launch (Default)".
15. Under Associated Packages & DLC, use **Add Demo** now so the demo has its own App ID ready for Phase 5.

**Done when:** you can install Triffall from a Steam beta branch on another PC, earn an achievement, see the overlay, and find your save on a second machine through Steam Cloud.

## Phase 3: A trustworthy, tested Windows build

Valve's build review only checks that the game starts and does what the store page says, so the things that make players trust the exe (no antivirus scare, a named publisher, it works on a plain PC and a Deck) are on you.

**Make the exe look like a real program**

1. **Build release copies on Windows with MSVC** (build\_windows.bat), from a clean checkout of the release commit. Cross-compiled builds work, but the MSVC toolchain is what antivirus models have seen most.
2. **Add version information and a manifest.** These are a small .rc file compiled into the exe: product name, version 1.0.0, company name, copyright, and a manifest declaring "run as the normal user" and DPI awareness. They don't stop a false positive by themselves, but an exe with no publisher, no version and no manifest is exactly the shape heuristic scanners distrust, and Windows shows the details in file properties and crash dialogs.
3. **Sign the exe.** This is the real fix for "Unknown publisher" and most Defender false alarms. Options:
   - **Azure Artifact Signing** (formerly Trusted Signing): about $9.99 a month. Open to organisations and individuals in the US and Canada, and to *organisations* in the EU and UK; individuals elsewhere can't use it.
   - A **code-signing certificate** from a certificate authority (OV or EV). Costs more per year and now needs a hardware key or cloud signing service.
   - Sign Triffall.exe and the demo's exe every release, after the build and before upload.
4. **If Defender still flags a build**, submit the exe as a false positive at the Microsoft Security Intelligence submission portal. Never tell players to turn Defender off.

**Test on machines that aren't yours**

5. **Clean Windows 10 and 11 PCs** (a friend's laptop or a fresh VM, no compilers installed): install from the Steam beta branch, play 20 minutes, check sound, fullscreen and windowed, a gamepad, Alt+Tab, saving and loading, and quitting with Shut Down.
6. **Steam Deck**: Valve tests for Deck Verified after your build review. What it checks: every screen works with the Deck's controls, on-screen prompts show controller buttons when a controller is in use, the name field opens the on-screen keyboard, no launcher, a supported resolution (1280x800 preferred), the smallest text is at least 9 px tall at 1280x800 (12 recommended), and at least 30 fps. STEAM.md section 3 lists Triffall-specific checks; test them on a real Deck if you can borrow one, then request the review from the app's Technical Tools. You can request it before release.
7. Run `tests/run_all.sh` on the exact commit you build from.

**Done when:** a signed exe from the release commit runs on two clean PCs and a Deck without a warning.

## Phase 4: The store page, public as Coming Soon

The store page is your most important marketing asset and the thing everything else links to, so get it public as early as you can show the game honestly. Valve says early wishlisters are about as likely to buy as late ones, and the page must be public for at least two weeks before you can release.

**Art you need** (Store Admin > Graphical Assets)

| Asset | Size (px) | Notes |
| --- | --- | --- |
| Header capsule | 920 x 430 | Shown everywhere; must have a readable title or logo. |
| Small capsule | 462 x 174 | Search results and lists; the logo must still read at this size. |
| Main capsule | 1232 x 706 | The front-page carousel. |
| Vertical capsule | 748 x 896 | Sales pages. |
| Page background | 1438 x 810 | Optional; Steam makes one from a screenshot if you skip it. |
| Screenshots | at least 5, 1920 x 1080 or larger, 16:9 | Gameplay only. Mark at least four as suitable for all ages. |
| Library assets | see Steamworks | Capsule, hero and logo for the Steam library; required before release. |

Triffall renders at 640x480 in a 4:3 desktop, so screenshots need framing: tools/promo\_kit.py and tools/promo\_shots.txt build them, but re-shoot with the 1.0 screens (combat feedback, the ending's first slide without spoilers). Valve rejects screenshots that aren't gameplay, and the 1998 desktop must not hide that this is an RPG: **lead with combat**.

**Words**

1. A one-sentence short description: what you do in the game, not the lore. Something like "Lead a party of four through a turn-based, old-school RPG, played inside a 1998 PC that may not be what it seems."
2. A long description with a few short sections and animated GIFs of combat. No links to other websites in it.
3. Only list features that are in the build at launch; the build review checks them. Don't tick Steam Deck Verified or controller support you haven't tested.
4. Tags: start with the genre ones players filter by (RPG, Turn-Based Tactics, Party-Based RPG, Retro, Pixel Graphics, Fantasy, Singleplayer) and the hook (the desktop). Steam suggests tags; players can add more later.

**Trailer**

5. 60 to 90 seconds. Gameplay in the first five seconds, combat early, the desktop twist as the hook, the logo and "Wishlist now" at the end. Capture at 1080p or above with the CRT effect on and off.

**Paperwork on the page**

6. **Content survey** (required before review). Three parts: general content, which generates regional age ratings; mature content (violence etc.); and **generative AI**. If any art, sound, text or translation that ships in the game was made with AI tools, you must describe it. The music and ambient loops made by tools/mksounds.py are procedurally generated code, not generative AI; describe them accurately if asked.
7. Release date: set it to "Coming soon" or a season. You choose how exact it looks.
8. Pricing: enter your base price in USD and accept Steam's suggested regional prices unless you have a reason not to.

**Submit**

9. Submit the store page for review at least **7 business days** before you want it public; the review usually takes 3 to 5 business days and may come back with changes.
10. When approved, press **Post as Coming Soon**. From that moment the two-week clock and the wishlists start.

**Done when:** the page is public, the trailer plays, and you have shared the link everywhere you exist online.

## Phase 5: Community, the demo and Next Fest

For a small game the months between the store page and launch decide how it sells: every channel below exists to send people to the store page to wishlist, and the demo plus Next Fest is the biggest single push you will get.

**Places for players to gather**

1. **Discord.** Make sure the invite built into the game (DISCORD\_INVITE in src/desktop/desktop.cpp) is a permanent invite to the right server. Set up a few channels (announcements, general, bug reports, screenshots), write short rules, and turn on slow mode and a verification level before you share it widely. Pin a post saying where crash.log lives.
2. **Steam community hub.** It exists once the page is public. Pin a FAQ and a "report a bug" thread, and read the discussions weekly.
3. **Pick one or two social channels** you will actually keep up (for a retro RPG: Bluesky or X, Reddit, TikTok or YouTube Shorts for short combat clips). Post a GIF or clip every week with the store link. Subreddits such as r/IndieGaming and r/rpg\_gamers have self-promotion rules: read them first.
4. **A press kit page**: logo, capsules, screenshots, trailer, a fact sheet, your contact email. promo\_kit.py's output is most of it.
5. **A mailing list** (optional but cheap): Steam only emails wishlisters at set moments (launch, a new demo, a discount); your own list reaches people whenever you have news.

**The demo** (its own App ID, created in Phase 2)

6. Build it with `build_windows.bat demo` (TRIFFALL\_DEMO on). Set store= in assets/data/demo.txt to the full game's store URL first, so the Wishlist icon works.
7. In the demo app: type Demo, base game App ID entered, its own depot and Cloud rule (the same save paths), a build uploaded and reviewed. Valve recommends no achievements in a demo.
8. A separate demo store page is optional; without it the demo is a "Download Demo" button on Triffall's page. After the demo goes live, re-publish Triffall's store page so the button appears.
9. Wishlisters can be emailed about the demo once, within two weeks of its first release, so release it when you can push it.

**Steam Next Fest**

10. Next Fest is a week of demos for unreleased games. Your store page must be public, the demo publicly playable by the start, and Triffall must release *after* the festival ends.
11. **February 2027:** February 22 to March 1. Register by **January 10, 2027**; to be in the press preview from February 11, submit the demo build and store page for review by January 25.
12. **June 2027 (fallback):** June 14 to 21. Register by **April 25, 2027**; submit by May 17 for the press preview from June 3.
13. During the festival: stream on your store page at least once, answer every comment, post daily on Discord and socials.

**Press, streamers, playtests**

14. Make a list of 50 to 100 YouTubers and streamers who cover retro or old-school RPGs and their size. Email them personally a month before launch with a Steam key (Steamworks > Request Keys) and the press kit link; follow up once.
15. Keys for curators: use Steam's **Curator Connect** rather than sending keys to anyone who emails you, because key resellers pose as press.
16. Run a **Steam Playtest** or a closed beta branch for 10 to 30 strangers before launch. This is also where the "real person plays start to finish" check from RELEASE\_CHECKLIST.md gets done.

**Done when:** the demo is live, you are registered for a Next Fest, and you have a list of contacts and a few hundred people in Discord or on wishlists.

## Phase 6: Release

Release is a button you press yourself, after two checklists in Steamworks are green; nothing happens automatically on your release date, so the work is getting everything approved a week or more early and keeping launch day calm.

**Two to four weeks before**

1. Upload the release build to the **default** branch (it can still be updated afterwards) and submit it for **build review**. Plan on 7 business days; it usually takes 3 to 5. The store page must already be approved: Steamworks won't take the build review before it.
2. Check the two checklists on the app's landing page, **Store Presence** and **Game Build**, are complete.
3. **Price:** final USD price and regional prices. You can't change the base price while a discount is scheduled.
4. **Launch discount:** optional, up to 40% (Valve suggests 10 to 15%), lasting 7 to 14 days. It must be set up *before* release; it is the only discount allowed in the first 30 days.
5. **Pick the day.** A Tuesday to Thursday, not on top of a big release or during a major Steam sale, at a time you can be awake for the next 12 hours.
6. Make sure whoever presses the button has both permissions: *Publish app changes to Steam* and *Manage pricing and discounts*.
7. Line up the launch post (Steamworks > Events & Announcements), the Discord and social posts, and the emails to press and streamers with keys.

**Launch day**

8. Press the green **Release App** button on the landing page, check the summary (store package published, launch discount live, game released "NOW"), then **Publish Now** and **Release Now**. Steam emails everyone who wishlisted.
9. Buy it on a second account and install it from scratch to make sure the live version works.
10. Post everywhere, stream for an hour if you can, and stay in Discord and the Steam discussions all day. Fix nothing big on day one unless it's a crash or a lost save; write things down instead.
11. If the build is broken, roll back: set the previous good build live on default in SteamPipe > Builds. That's why you never delete old builds.

**Done when:** the game is buyable, the live build is the one you tested, and you've answered the first day's questions.

## Phase 7: Post-launch support

The first month decides your review score, and the review score decides whether Steam keeps showing the game, so plan to spend launch week to launch month fixing, answering and patching rather than starting the next project.

**Patching safely** (repo)

1. Fix on a branch, run `tests/run_all.sh`, build, sign, upload to the **beta** branch, play it, then set it live on **default**.
2. **Old saves must keep loading.** If a patch renames an item, flag, map or spell, add a line to assets/data/migrations.txt. If the save layout changes, bump TRIFFALL\_SAVE\_FORMAT in src/core/version.h and add the step to Game::migrateSave. Keep a folder of real 1.0 saves (yours and players') and load them in every patch before release.
3. Bump TRIFFALL\_VERSION (1.0.1, 1.0.2…) so bug reports tell you which build someone has.
4. Patch the demo too when a fix touches the demo area; its saves carry into the full game.
5. Post patch notes as a Steamworks **event** ("Small update" or "Patch notes" type), and in Discord.

**Hearing about problems**

6. **Crashes:** players' saves/crash.log (and crash\_recovery.sav) are next to the exe; ask for both. Keep a pinned "how to send a crash log" post on Steam and Discord, and a bug-report template asking for the version, Windows or Deck, and what they were doing.
7. Read Steam reviews and discussions every day for the first two weeks, then weekly. Triage into: crash or lost save (patch within days), blocker or bug (next patch), balance (gather, then decide; BALANCE.md says the Lich and the Scar are the ones to watch), wish (a list).

**Reviews and players**

8. Reply to negative reviews that report a bug, saying it's fixed and in which version. Don't argue with opinions, never ask for review changes in exchange for anything, and don't offer keys for reviews.
9. **Refunds** happen: Steam refunds within 14 days of purchase with under two hours played. A short demo helps people decide before buying; watch the refund reasons in your sales data for patterns.

**Keeping the game visible**

10. **Discounts:** after launch, no discount for 30 days. Then at least 10% off, at most 95%, 1 to 14 days long, and 30 days between discounts, except Steam's seasonal sales, which are exempt. Raising the base price starts another 30-day lock.
11. Join the big seasonal sales (Steam announces them in Steamworks > Upcoming Events) and themed festivals that fit (RPG, retro, turn-based).
12. Post an event or update every few weeks for the first months: patches, a dev note, a behind-the-scenes post. Each one reaches people who follow or own the game.
13. Plan one content update (a new side quest area, a harder mode, RiftEdit campaigns from players) for two to three months after launch and announce it with a sale.
14. **Money:** Valve pays monthly into your bank once you pass the payment threshold; download the sales reports and keep them for tax. The $100 fee is paid back once you pass $1,000 in adjusted gross revenue.

**Done when:** never fully, but a good first milestone is a month with no new crash reports and every bug review answered.

## Master checklist

Every step above as a tick box, in order; tick them here as you go.

**Phase 0: Decisions**

- [x] Name checked on Steam, trademark registers, domain and socials
- [ ] Decided who signs (you or a company); company set up if needed
- [ ] Tax ID ready (SSN/EIN, or your country's for a W-8BEN)
- [ ] Bank account in the signer's exact legal name
- [ ] Working price and target window written down
- [ ] Developer name, contact email and credits name decided (credits.txt)

**Phase 1: Steamworks account**

- [ ] Steam account with the Steam Guard mobile authenticator
- [ ] NDA and Steam Distribution Agreement signed
- [ ] $100 app fee paid (21-day clock starts)
- [ ] Bank details entered
- [ ] Tax interview done
- [ ] Identity verification passed (up to 30 days)

**Phase 2: App and integration**

- [ ] App created, App ID noted
- [ ] Steamworks SDK downloaded; Steam build made with TRIFFALL\_STEAM=ON
- [ ] Overlay works; steam\_appid.txt not in upload
- [ ] 33 achievements created (API Name = id), icons uploaded, hidden ones marked, published
- [ ] Steam Cloud Auto-Cloud rule set
- [ ] Steam Input default set to Gamepad
- [ ] Depot and app IDs in tools/steam/\*.vdf; first steamcmd upload done
- [ ] Build installed from a password-protected beta branch
- [ ] Launch option set
- [ ] Demo app added

**Phase 3: Windows build**

- [ ] Release built with MSVC from a clean checkout
- [ ] Version info and manifest compiled in
- [ ] Code signing set up; exe signed
- [ ] Clean Windows 10 PC test
- [ ] Clean Windows 11 PC test
- [ ] Steam Deck test; Deck compatibility review requested
- [ ] tests/run\_all.sh passes on the release commit

**Phase 4: Store page**

- [ ] Header, small, main and vertical capsules
- [ ] Library assets
- [ ] 5+ gameplay screenshots at 1920x1080, 4 marked all ages
- [ ] Short and long descriptions, no outside links
- [ ] Tags
- [ ] Trailer
- [ ] Content survey including the generative AI section
- [ ] Price and regional prices
- [ ] Submitted for review 7+ business days ahead
- [ ] Posted as Coming Soon

**Phase 5: Community and demo**

- [ ] Discord set up; built-in invite checked as permanent
- [ ] Steam hub FAQ and bug thread pinned
- [ ] One or two social channels, posting weekly
- [ ] Press kit page
- [ ] Demo built with store= set; demo app configured and reviewed
- [ ] Triffall's store page re-published after the demo went live
- [ ] Registered for Next Fest (Jan 10 for February, Apr 25 for June)
- [ ] Demo and page submitted for the press preview
- [ ] Streamer and press list; keys sent; Curator Connect set up
- [ ] Playtest with strangers, including a full playthrough

**Phase 6: Release**

- [ ] Release build on default; build review passed
- [ ] Store Presence and Game Build checklists complete
- [ ] Launch discount set (optional)
- [ ] Release day chosen; both permissions on the account that presses the button
- [ ] Launch announcement, posts and press emails ready
- [ ] Release App pressed; live build checked on a second account

**Phase 7: Post-launch**

- [ ] Beta-branch-first patch routine
- [ ] Folder of real 1.0 saves tested against every patch
- [ ] Crash-log FAQ and bug template pinned
- [ ] Daily review and discussion reading for two weeks
- [ ] First discount planned 30+ days after launch
- [ ] Seasonal sales joined
- [ ] First content update planned

## Sources

Valve's rules change, so check these pages again before each phase. Checked on October 22, 2026.

- [Steam Direct](https://partner.steamgames.com/steamdirect): the $100 fee and the $1,000 recoupment.
- [Onboarding](https://partner.steamgames.com/doc/gettingstarted/onboarding): agreements, bank, tax, identity check (up to 30 days), the 21-day wait.
- [Releasing your game](https://partner.steamgames.com/doc/store/releasing): checklists, permissions, the Release App button.
- [Review process](https://partner.steamgames.com/doc/store/review_process): what store and build reviews check, 3 to 5 business days.
- [Coming Soon](https://partner.steamgames.com/doc/store/coming_soon): the two-week minimum and wishlists.
- [Graphical assets](https://partner.steamgames.com/doc/store/assets/standard): capsule and screenshot sizes.
- [Content survey](https://partner.steamgames.com/doc/gettingstarted/contentsurvey): mature and generative AI disclosure.
- [Demos](https://partner.steamgames.com/doc/store/application/demos): demo apps, store pages and wishlist emails.
- [Discounts](https://partner.steamgames.com/doc/marketing/discounts): launch discount, cooldowns and limits.
- [Next Fest February 2027](https://partner.steamgames.com/doc/marketing/upcoming_events/nextfest/feb_2027) and [June 2027](https://partner.steamgames.com/doc/marketing/upcoming_events/nextfest/june_2027): dates and deadlines.
- [Steam Deck compatibility](https://partner.steamgames.com/doc/steamdeck/compat): Verified criteria and review.
- [Steam refunds](https://store.steampowered.com/steam_refunds/): 14 days, under two hours.
- [DevClass on Azure Artifact Signing](https://devclass.com/2026/01/14/code-signing-windows-apps-may-be-easier-and-more-secure-with-new-azure-artifact-service/): eligibility and price.
- In the repo: STEAM.md (commands and Steamworks settings), RELEASE\_CHECKLIST.md, BALANCE.md.
