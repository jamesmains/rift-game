# Triffall controller map (as of 1.8.6; layouts added in 1.8.7)

Asked for in #207: every place the game reads the controller and what each button does there, so the flow can be
rethought. Everything below is the default binding; every action can be rebound in Settings > Controls, and two actions
may share a button (the game decides by where you are). Keyboard keys are in brackets for reference.

## The buttons and the actions on them

| Button | Action(s) bound to it | Keyboard |
|---|---|---|
| A | Confirm / select | Enter, Space |
| B | Cancel / pause | Esc |
| X | Interact / talk | E |
| Y | Toggle attack / aimed (fight); Sort items (item lists) | A; R |
| LB | Spell menu; Previous tab | B / S; PgUp, [ |
| RB | End turn (fight); Next tab | T; PgDn, ] |
| LT | Item menu; Previous item category | Q; G |
| RT | **Favorites picker (new in 1.8.6)**; Next item category | (none); T |
| Start | Party menu | Tab, I |
| Back | Action menu (fight); Journal | R; J |
| LS (click) | Overwatch (fight); World map | O; M |
| RS (click) | Center camera | C |
| D-pad | Move / cursor up, down, left, right | Arrows, WASD |
| Left stick | Walk (map), the pointer (desktop) | |
| Right stick | Pan the camera | |
| (none) | Defend (moved off RT in 1.8.6), Flee, Quick save, Fullscreen, Favorites 1-8, Dev console | D, F, F5, F11, 1-8, ` |

## Context by context

### Desktop (the main menu)
- Left stick: moves the pointer. D-pad: on the desktop, jumps icon to icon (down past the last icon reaches Start);
  inside a window, jumps control to control, grid-like since 1.8.6.
- A: click (on an icon: hold and move to drag it, release to open). B: close the top window / the Start menu / the
  on-screen keyboard. Start (Party menu action): opens / closes the Start menu.
- X: right-click (Minesweeper flag, Solitaire send-up). LB / RB: scroll a text window by a page.
- Settings window: D-pad moves between controls, A clicks; a drop-down opens on A, D-pad walks it, A takes, B leaves it.

### Walking the map (explore)
- Left stick / D-pad: walk. A or X: talk / use what you face (or stand on).
- B: pause menu. Start: Party screen. LB: Spells tab. LT: Items tab. Back: Journal. LS: World map. RS: center camera.
- RT: favorites picker (D-pad picks, A uses, B or RT closes). Right stick: pan.
- Unused here: Y, RB.

### Aiming a thrown noisemaker
- D-pad: move the aim. A or X: throw. B: stop aiming.

### A fight, your turn
- D-pad: move the cursor a tile. A or X: move there / attack / cast on it (whatever the mode is).
- Y: switch normal / aimed attack. LB: open the spell list (again: close it). LT: open the item list (again: close it).
- RB: end turn. LS: overwatch. Back: the action menu (Attack, Aimed, Spell, Item, Defend, Overwatch, End turn, Flee).
- RT: favorites picker; with the spell / item list open, RT puts the highlighted one on a slot instead.
- B: backs out of a target pick / list / the action menu; with nothing open it pauses.
- Pressing a favorite that is being aimed again (its number key, its HUD slot, or picking it in the picker) goes back
  to moving (#201).
- Not reachable on the pad except through the action menu (Back): Defend, Flee.

### Pause menu
- D-pad up / down, A picks, B resumes. Start = Party & Items, LS = World Map, Back = Journal (the keys shown beside them).

### Party screen (Start)
- LB / RB: previous / next tab (Party, Items, Spells, Quests). B or Start: close (B steps back first when inside a list).
- Party tab: up / down member, right or A into the equipment, A on a slot opens the item list, LT unequips. X: make
  leader. LT on a companion: talk to them.
- Items tab: up / down, LT / RT category, Y sort, A opens the actions (Use on, Equip on, **Favorite...** new in 1.8.6,
  Drop). X: field crafting.
- Spells tab: up / down member, right or A into their spells; A casts (outside fights), **RT puts it on a favorite**.
- Back: Journal (on the Quests tab it opens the full Journal).

### Shop
- Up / down: item. Left / right: quantity. A: buy / sell. X: buy / sell as many as you can. LB / RB: buy / sell tab.
  LT / RT: category. Y: sort. B or Start: leave.

### Crafting
- Up / down: recipe. Left / right: quantity. A: make. LB / RB (or Start): station tab. B: leave.

### Journal
- LB / RB: tab. Up / down: entry. Left / right: page or column. A or LS on a quest: show it on the world map.
  B or Back: close.

### World map
- D-pad: move the selection. A or X: travel there (with a coach). B or LS: close.

### Notice board
- Up / down: notice. A: take / collect. X: drop a taken one. LB / RB / Start: notices / Standing page. Back: Journal's
  Bounties page. B: leave.

### Dialogue
- Up / down: answer. A or X: skip the text / pick. B: skip the text (or close when there are no answers).

### Message box, victory screen, cutscenes, level-up
- Message: A, B or X closes. Victory: A or B. Cutscene: A, X or B for the next line. Level-up: D-pad and A.

### Save / Load window
- Up / down: slot. Left / right (or LB / RB): Save / Load side. A: pick (then A yes / B no). B: close.

### Name and number entry
- D-pad up / down: change the letter, right: next letter, left: delete. A: done. B: cancel.

### Settings (in-game, from Pause)
- D-pad: move between controls (grid-like, 1.8.6). A: toggle / press / open a drop-down. Left / right on a choice or
  slider: change it. LB / RB: page. B: back to the game (changes are kept).
- Controls page: up / down row, left / right column, A then press the new button (B clears it).

## Things that look worth changing

1. **Two buttons do the same thing on the map and in fights.** A and X both talk / act on the map and both confirm in a
   fight. X could do something of its own there (the favorites picker, Defend, or "next target").
2. **Picking a target is slow.** The fight cursor moves one tile per press. Cycling through enemies in range with
   LB / RB (or the right stick) is what most console tactics games do; the cursor could jump straight to them.
3. **Defend and Flee have no button** (only through the action menu). With RT now on favorites, Defend could go on X,
   or the action menu could open on Y and the attack / aimed toggle move into it.
4. **End turn on RB is easy to hit by accident** and RB is also "next tab" in every menu. Holding it for half a second,
   or moving End turn to Back (and the action menu to Y), would make it deliberate.
5. **Overwatch on a stick click** is awkward for a core tactical move; a stick click suits rarer things (center camera,
   world map).
6. **Back means two different things**: the Journal on the map, the action menu in a fight. That's fine if the action
   menu moves (see 3 and 4).
7. **Quick save has no button.** Saving goes through the shrine's own menu, so it may not need one, but a shrine could
   offer it on X.
8. **LB / LT open Spells / Items from the map** but are tab / category keys inside the menu, so the same press means
   "go to Spells" and then "previous tab". Start + LB / RB to the tab you want is an alternative.

### One possible layout, for the fight (to discuss, nothing changed yet)
| Button | Now | Could be |
|---|---|---|
| A | act on the cursor | act on the cursor |
| B | back / pause | back / pause |
| X | same as A | favorites picker |
| Y | aimed toggle | action menu (aimed, defend, overwatch, flee...) |
| LB / RB | spells / end turn | previous / next target in range |
| LT / RT | items / favorites | spells / items |
| Back | action menu | end turn (hold) |
| LS / RS | overwatch / center | center / world-free camera |

Questions for the owner: should target cycling be added? Which actions should be one press in a fight, and which can
live in the action menu? Should the map and the fight use the same buttons for Spells / Items?

## 1.8.7: choose in Settings

Settings > Controls now starts with a Gamepad block, so the layout question can be settled by playing:

- **Gamepad layout: Classic / Tactics.** Classic is everything above. Tactics changes only the pad column:
  LB / RB jump the fight cursor between foes in sight (nearest first; previous / next tab in menus as before),
  LT spells, RT items (categories in item lists as before), Y the action menu (aimed shot, defend, overwatch, flee...),
  Back favorites in a fight (Journal on the map), LS end turn in a fight (world map on the map). Rebinding any pad
  button by hand shows "changed by hand"; picking a layout again puts it back.
- **Swap A and B** (confirm on B, back on A), on top of either layout.
- **Hold End turn for half a second**: the pad's End turn button needs a hold (a bar fills), keys stay instant.
- **Start the fight cursor on a foe**: each turn the cursor starts on the nearest foe in sight instead of your unit.

Keyboard: Z / X jump between foes in any layout.
