// 1.3.4 (#52): every sound event the game itself plays, with when it plays. The Editor's Sounds tab shows this as
// "Played when" and groups the list by it; Audio::play uses `fallback` when sounds.txt has no entry for a trigger
// (triggers split off an older shared event in 1.3.4 fall back to it, so an old sounds.txt sounds the same).
// Keep it in step with the code: every gAudio.play("...") with a fixed id belongs here.
#include "audio.h"

const std::vector<SoundTrigger>& soundTriggers() {
    static const std::vector<SoundTrigger> t = {
        // ---- interface ----
        {"ui_move", "Interface", nullptr, "the cursor moves in a game menu, the pack, a shop, save/load or dialogue answers"},
        {"ui_confirm", "Interface", nullptr, "Enter / A in a game menu; turning a page of the ending"},
        {"ui_click", "Interface", nullptr, "a mouse click in a game menu (soft, so it never sounds like a purchase)"},
        {"ui_open", "Interface", nullptr, "a game screen opens (pack, journal, shop, pause, board...); a new hint appears"},
        {"ui_close", "Interface", nullptr, "a game screen closes; a bounty notice is taken down"},
        {"ui_error", "Interface", nullptr, "you can't do that: not enough gold, no room, out of range, missing tool, desktop error boxes"},
        {"text_blip", "Interface", nullptr, "each new line of dialogue"},
        // ---- desktop ----
        {"boot", "Desktop", nullptr, "the boot screen"},
        {"shutdown", "Desktop", nullptr, "Start > Shut Down"},
        {"desktop_click", "Desktop", nullptr, "a desktop icon, the Start button or a window button is clicked"},
        {"desktop_open", "Desktop", nullptr, "a desktop window opens (Yes/No questions too)"},
        {"desktop_close", "Desktop", nullptr, "a desktop window closes"},
        {"mine_click", "Desktop", nullptr, "Minesweeper: a square is opened"},
        {"mine_boom", "Desktop", nullptr, "Minesweeper: a mine goes off"},
        {"card", "Desktop", nullptr, "Solitaire: a card is moved or dealt"},
        {"achievement", "Desktop", nullptr, "an achievement is unlocked"},
        // ---- exploring ----
        {"step", "Footsteps", nullptr, "a party step on ground whose tile kind has no step= (also used when a step= sound is missing)"},
        {"teleport", "Exploring", nullptr, "moving to another map: exits, stairs, doors into buildings, teleports, recall"},
        {"door_open", "Exploring", nullptr, "a door opens (doors with no key and no sound= of their own)"},
        {"door_unlock", "Exploring", "unlock", "a locked door opens with its key (doors without their own sound=)"},
        {"door_locked", "Exploring", nullptr, "a door needs a key you don't have"},
        {"door_sealed", "Exploring", "door_locked", "a door that opens by a mechanism or a condition won't open yet"},
        {"exit_locked", "Exploring", "door_locked", "a map exit is barred (it needs a flag or an item, or is closed in the demo)"},
        {"shop_closed", "Exploring", "door_locked", "a shop's door or shopkeeper is closed at this hour"},
        {"chest_open", "Exploring", nullptr, "a chest opens (chests without their own sound=)"},
        {"chest_locked", "Exploring", "door_locked", "a chest needs a key you don't have"},
        {"lever", "Exploring", "unlock", "a lever is pulled (levers without their own sound=)"},
        {"freeze", "Exploring", "spell_ice", "1.4.0: water freezes over (needs=freeze obstacle: a frost spell or a Frost Flask)"},
        {"power_on", "Exploring", "spell_lightning", "1.4.0: a dead machine, lift or door gets a jolt and wakes (needs=shock obstacle)"},
        {"emp_release", "Exploring", "spell_emp", "1.4.0: an EMP scrambles a sealed tech door's lock (needs=emp obstacle)"},
        {"hidden_found", "Exploring", "detect", "1.4.0: light or a detect spell shows something hidden (needs=light obstacle)"},
        {"crate_push", "Exploring", "step_hard", "1.4.0: a crate is pushed one tile"},
        {"lure", "Exploring", "block", "1.5.0: a thrown noisemaker lands (item effect=lure; the item's sound= wins)"},
        {"trap", "Exploring", "hit", "1.5.0: a trap goes off under a monster group or the party"},
        {"plate_down", "Exploring", "lever", "1.4.0: a pressure plate goes down under a crate or the party"},
        {"plate_up", "Exploring", "ui_move", "1.4.0: a pressure plate comes back up"},
        {"text_key", "Interface", "ui_move", "1.4.0: a letter is typed into a terminal / combination lock box"},
        {"pickup", "Exploring", nullptr, "an item is received (loot, rewards, give= actions)"},
        {"gold", "Exploring", nullptr, "gold is received from a chest or a gold= action; paying a town's damages"},
        {"gather", "Exploring", "pickup", "a resource node is picked by hand (nodes without sound=)"},
        {"gather_tool", "Exploring", "hit", "a resource node is worked with a tool (nodes without sound=)"},
        {"burn", "Exploring", "spell_fire", "a torch or lamp oil burns an obstacle away"},
        {"fuse_lit", "Exploring", "grenade_throw", "a fuse is lit to blast an obstacle"},
        {"equip", "Exploring", nullptr, "an item is equipped"},
        {"unequip", "Exploring", "equip", "an item is taken off"},
        {"potion", "Exploring", nullptr, "a consumable is used (items without their own sound=)"},
        {"save", "Exploring", nullptr, "the game is saved (autosave too)"},
        {"load", "Exploring", "save", "a saved game is loaded"},
        {"levelup", "Exploring", nullptr, "a party member reaches a new level"},
        {"respec", "Exploring", "levelup", "a party member's points are reset (respec)"},
        {"ability_learned", "Exploring", "levelup", "someone learns a lasting ability (teach= in dialogue and scripts: companion quests)"},
        {"quest", "Exploring", nullptr, "a quest updates (quest= in dialogue and scripts)"},
        {"rest", "Exploring", nullptr, "rest= / heal= in dialogue (inns, beds)"},
        {"healer", "Exploring", "rest", "a healer heals or cures the party"},
        {"travel", "Exploring", nullptr, "fast travel from the world map"},
        {"ambush", "Exploring", nullptr, "a random encounter or ambush starts"},
        {"thunder", "Exploring", nullptr, "thunder during a storm"},
        {"banter", "Exploring", nullptr, "a companion speaks up (banter)"},
        // ---- towns and crafting ----
        {"shop_buy", "Towns & crafting", nullptr, "an item is bought"},
        {"shop_sell", "Towns & crafting", nullptr, "an item is sold"},
        {"craft", "Towns & crafting", nullptr, "an item is crafted"},
        {"reinforce", "Towns & crafting", "craft", "an item is reinforced (+1..+3)"},
        {"enchant", "Towns & crafting", "craft", "an item is enchanted"},
        {"salvage", "Towns & crafting", "craft", "an item is salvaged for materials"},
        {"craft_skill", "Towns & crafting", "levelup", "a crafting skill (Smithing, Alchemy, Tinkering) goes up"},
        {"recipe_learned", "Towns & crafting", "quest", "a recipe book or formula is read"},
        {"bounty", "Towns & crafting", "quest", "a bounty is done or its item found - go back to the board"},
        {"bounty_paid", "Towns & crafting", "gold", "a bounty's reward is paid at the board"},
        {"town_reward", "Towns & crafting", "quest", "a town project is finished or a favour is granted"},
        // ---- combat ----
        {"combat_start", "Combat", nullptr, "a fight begins"},
        {"turn_player", "Combat", nullptr, "a party member's turn starts"},
        {"turn_enemy", "Combat", nullptr, "an enemy's turn starts"},
        {"reinforcements", "Combat", "ambush", "more enemies join a fight"},
        {"attack_melee", "Combat", nullptr, "a melee attack (weapons and monsters without their own sound= / attacksound=)"},
        {"attack_bow", "Combat", nullptr, "a ranged attack that isn't a gun (bows, slings, ranged monsters)"},
        {"attack_gun", "Combat", nullptr, "a gunshot"},
        {"attack_laser", "Combat", nullptr, "an energy gun (fire, shock or magic guns)"},
        {"hit", "Combat", nullptr, "an attack does damage"},
        {"crit", "Combat", nullptr, "a critical hit"},
        {"miss", "Combat", nullptr, "an attack misses"},
        {"block", "Combat", nullptr, "an attack does no damage"},
        {"explosion", "Combat", nullptr, "a thrown item lands (grenades and items without their own sound=)"},
        {"flee", "Combat", nullptr, "the party flees a fight"},
        {"defeat", "Combat", nullptr, "the party is wiped out (game over)"},
        {"death_enemy", "Deaths", nullptr, "an enemy dies (creatures without sound=)"},
        {"death_party", "Deaths", nullptr, "a party member is knocked out"},
        // ---- spells ----
        {"spell_cast", "Spells", nullptr, "a spell with no damage type, heal or effect (spells without sound=)"},
        {"spell_fire", "Spells", nullptr, "a fire damage spell (spells without sound=)"},
        {"spell_ice", "Spells", nullptr, "a cold damage spell (spells without sound=)"},
        {"spell_lightning", "Spells", nullptr, "a shock damage spell (spells without sound=)"},
        {"spell_poison", "Spells", nullptr, "a poison damage spell (spells without sound=)"},
        {"spell_heal", "Spells", nullptr, "a healing, revive or cure spell (spells without sound=)"},
        {"spell_buff", "Spells", nullptr, "a spell with an effect - buffs and debuffs (spells without sound=)"},
        {"light", "Spells", nullptr, "a light spell outside a fight"},
        {"stealth", "Spells", nullptr, "a stealth spell outside a fight"},
        {"detect", "Spells", nullptr, "a detect spell outside a fight"},
        {"unlock", "Spells", nullptr, "the unlock spell; also what door_unlock and lever fall back to"},
        // ---- music ----
        {"title", "Music", nullptr, "the desktop / main menu"},
        {"overworld", "Music", nullptr, "maps with music=overworld, and overworld maps without music= (by day)"},
        {"overworld_night", "Music", nullptr, "those overworld maps at night"},
        {"town", "Music", nullptr, "towns and interiors without music="},
        {"forest", "Music", nullptr, "other hand-made maps without music="},
        {"dungeon", "Music", nullptr, "dungeon maps without music= (generated dungeons: cave/castle/crypt styles)"},
        {"tech", "Music", nullptr, "generated dungeons of tech / ship style without music="},
        {"combat", "Music", nullptr, "fights (no boss, no combatmusic= on the map or campaign)"},
        {"boss", "Music", nullptr, "fights with a boss that has no music= of its own"},
        {"fanfare", "Music", nullptr, "after a won fight"},
        {"gameover", "Music", nullptr, "game over"},
        {"ending", "Music", nullptr, "the epilogue and credits"},
        // ---- not played by the game itself ----
        {"ui_cancel", "Spare", nullptr, "nothing by itself (closing plays ui_close) - free for sound= or a script"},
        {"ui_page", "Spare", nullptr, "nothing by itself - free for sound= or a script"},
        {"victory", "Spare", nullptr, "nothing by itself (a won fight plays the fanfare music) - free for sound= or a script"},
        {"reload", "Spare", nullptr, "nothing by itself - free for sound= or a script"},
    };
    return t;
}

const SoundTrigger* soundTrigger(const std::string& id) {
    for (auto& t : soundTriggers()) if (id == t.id) return &t;
    return nullptr;
}

std::vector<std::string> soundGroups() {
    return {"Interface", "Desktop", "Exploring", "Footsteps", "Towns & crafting", "Combat", "Spells", "Deaths", "Music", "Spare", "Other"};
}
