# Balance notes

## 1.4.0 re-run (after the combat changes of #55-#72, #78-#80)

Same method as 1.0 below: `tools/pacing.sh warrior 1 1 2 3` (run here through a PowerShell copy of it), Normal, three
seeds, with and without god mode, on the 1.4.0 build (cover preview, exposed targets, spells vs cover, combos, wind-ups,
roles, morale, traits, perks, the reworked side quests...).

| Checkpoint | Hero level | Gold carried | Kills | Bot time (seeds 1-3) |
|---|---|---|---|---|
| Goblin chief dead | 2-3 | 90-105 | 18-23 | 3 min |
| After Pell's caravan | 4 | 420-480 | 37-40 | 7.5-8 min |
| Lich dead | 5 | 650-680 | 45-62 | 13-16 min |
| Golem dead | 6 | 850-890 | 63-77 | 17-21 min |
| Hierophant dead | 7 | 930-1,020 | 104-114 | 24-27.5 min |
| Hull Walker dead | 8 | 930-1,020 | 115-124 | 30-36 min |
| Entering the Scar | 8 | 1,180-1,300 | 125-135 | 32-38 min |
| After the Unmade King | 9 | 1,600-1,790 | 140-155 | 44.5-50 min |

- The curve is about the same as 1.0, one level lower at the end (9 instead of 10) and a few minutes longer: fights take
  more turns now that cover, wards and wind-ups matter and the bot doesn't play the tricks.
- Survival runs: all three seeds are wiped by the Lich again, as in 1.0 - he is still the first wall for the bot. The
  bot now goes for the necromancer that wards him (the party AI hits whatever wards a boss first), but it doesn't bring
  fire oil or dodge Grave Nova. A person who reads the note by the crypt's stairs should do better than the bot.
- Changed during the re-run: a boss's healing draught on Normal heals 20% (35% on Hard) - the goblin chief's was
  turning the first boss fight; the Bandit Captain's own group is back to plain bandits (shield-bearers stay with the
  archers of the two groups linked to him) - a level-3 bot party met all three at once and lost; casters and shooters
  with no path to their target now walk to a tile they can hit it from (a lab turret behind a gap stalled a fight).
- Knobs if testers bounce off: crypt group `level=`, the Lich's `ward=necromancer` (remove it to make him a plain
  fight), `windup=` on any boss, the new bandit creatures' stats, `aiSmart()` in combat.cpp for how hard Hard plays.

# Balance notes for 1.0

How the difficulty and reward curve was checked, what changed, and what still needs a person.

## How it was measured

* The playthrough bot (`src/platform/bot.h`) plays the main quest plus the side content most players pick up early
  (Bram, Elowen, Tessa, Pell's caravan, Reyna's ears) as a careful player: torches underground, potions, rests at
  inns, buys potions and torches, equips better loot. The combat AI fights for the party.
* `tools/pacing.sh warrior 1 1 2 3` runs it on three seeds, Normal difficulty, twice each: with god mode (always
  finishes, gives the level / gold / kills / time curve, CSV in `out/pacing/<seed>-god/pacing.csv`) and without
  (shows where the party gets wiped).
* A content audit of every fixed fight and quest reward (all of it, split 1 to 4 ways).

The bot is a weaker player than a person: no tactics beyond "a good target", no consumables except healing, and it
never buys weapons or armour. Treat its wipes as "this is where it gets hard", not "this is impossible".

## What was wrong

1. **Experience was divided between the party.** Every fixed fight and quest reward in the game added together, split
   four ways, came to level 7 - and the last dungeon's monsters are level 11 or more with a +10 map bonus on top. A
   full party could never reach the level the game was built for. **Fix:** every member standing gets the whole
   award. (All fixed content, unsplit: level 12, which matches the creature levels.)
2. **Boss levels did not ramp.** Goblin chief 4, bandit leader 5, then the Lich 9 (only the second main-quest boss),
   golem 9, Hierophant 9, Hull Walker 9, Unmade King 15 - each with the map's level bonus on top.
   **Fix:** bosses take half the map's level bonus; the Lich is now level 7 (170 HP and 9-16 damage, was 220 HP).
3. **The Lich joined the first fight of Dunmarrow.** Monsters that hear a fight join it; his room is in sight of the
   antechamber, and the antechamber guards were linked to him. **Fix:** bosses keep to their lair (they only join a
   fight within 3 tiles of them), and the antechamber is no longer linked to him.
4. **The Scar's map bonus was +10** on monsters already level 9-11. **Fix:** +6.
5. AI and fight bugs that made some fights endless: see the README's 1.0 section.

## The curve now (god-mode runs, seeds 1-3, Normal)

| Checkpoint | Hero level | Boss level | Gold carried | Kills | Bot time |
|---|---|---|---|---|---|
| Goblin chief dead | 2-3 | 4 | ~100 | ~24 | 3 min |
| Brasswick, letter delivered | 3 | - | ~300 | ~40 | 5 min |
| After Pell's caravan | 4-5 | bandit leader 5 | 450-570 | ~60 | 9-10 min |
| Lich dead | 5-6 | 7 | ~770 | ~80 | 14-15 min |
| Golem dead | 6-7 | 9 | ~1,000 | ~100 | 18-22 min |
| Hierophant dead | 7-8 | 9 | 1,100-1,250 | ~160 | 27-31 min |
| Hull Walker dead | 8 | 9 | ~1,250 | ~175 | 32-36 min |
| Entering the Scar | 8-9 | Unmade King 15 (+3) | 1,350-1,550 | ~200 | 36-38 min |
| After the Unmade King | 10 | | ~1,780 | ~220 | 45-47 min |

Before the change the same route ended at level 7.

**Time:** the bot reads nothing, plays fights at AI speed and skips most side content. 45 minutes of bot time for
the main quest suggests several hours for a person reading, exploring and thinking, and the 4-8 hour target looks
plausible with 20 side quests - but only a stopwatch and a real player can say.

**Gold:** about 1,000 by Ashfall and 1,800 at the end, with mid-tier gear at 300-1,500 gold. A party of four can buy one
or two good pieces per town, so gold stays scarce as intended. The bot never sells loot; players who do will have more.

## Where it is still hard

Without god mode the bot party (level 4-5, three members, no bought gear) is wiped out by the Lich in all three seeds,
after clearing the rest of Dunmarrow. Even after the changes, the Lich (170 HP, heals himself, a necromancer raising
skeletons beside him) is the first real wall. A person who brings fire (undead are weak to it; Elowen has fire spells;
fire oil is sold in Brasswick) at level 5-6 should manage; whether it is fun is a playtest question. If testers bounce
off it, lower `level=4` on the crypt's enemy groups to 3, or take Dark Mending away from the Lich.

Also watch in playtests: the Scar (a level 9 party against an 18th-level final boss with the bonus), and whether
level 10-12 by the end makes the late side dungeons (Ashwing's mine, +8) too easy.
