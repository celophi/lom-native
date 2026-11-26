# Modding

You can change how the game plays with mods. It's early days, so there isn't a lot a mod can do yet, and what's here may still change. A mod can turn on some of the port's switches, and it can run Lua scripts that hook into the game.

## Using mods

The launcher's Mods button opens the Mods window. It lists every mod in your mods folder, with a checkbox to turn each one on or off, and buttons to move them up and down, add a mod, remove one, and open the mods folder. Mods load when the game starts, so a change counts from the next time you press Play.

A few mods come with lom-native: Always run, Quick fights, No natural healing, Skip movies, Skip or win battles, Japanese balance and Quick room changes. They're turned off to start with. You can turn them on and off, but not remove them.

Order matters. When two mods set the same thing, the one lower in the list wins, and the Mods window says which mod overrides which.

Some mods can also be turned off and on while you play, from the Mods page in the game's menu (F1). The ones that come with lom-native all can, except Japanese balance, which is on or off for the whole time you play. Only mods you turned on in the launcher show up there. Turning one off in F1 lasts until you close the game; the next time you press Play, the launcher's Mods window decides again.

If the game won't load a mod, the Mods window grays it out and says why. The same goes for the game: it says so on screen when it starts, and the reason is in the log.

Some mods use a number called a seed, so the same seed always gives the same game. While one of those is on, the Mods window has a Seed box. A new game starts with the seed in it, so you can play one someone gave you, and if you leave it empty each new game gets its own. A game keeps its seed in its saves, so changing the box never changes a game you've already started. The Mods page in the game's menu (F1) shows the seed of the game you're playing.

A mod that runs scripts can change the game in ways your normal saves shouldn't see. So while one is on, your saves go in a folder of their own inside your save folder, one for each set of those mods. Turn them off and you're back to your normal saves.

## Making a mod

A mod is a folder with a `mod.json` in it, and the folder's name has to be the mod's id. To try one out, put the folder in your mods folder (or use Add mod... in the Mods window, which copies it in) and turn it on.

```
author.hard-mode/
    mod.json
    scripts/
        main.lua
        helpers.lua
```

### mod.json

```json
{
  "id": "author.hard-mode",
  "name": "Hard mode",
  "version": "1.0.0",
  "author": "Your name",
  "description": "No healing after fights, and every fight costs you 50 lucre.",
  "game": ["SLUS-01013", "SLPS-02170"],
  "api": 1,
  "switches": {
    "no_natural_healing": true
  },
  "scripts": ["scripts/main.lua"]
}
```

| Field | | What it is |
|---|---|---|
| `id` | needed | The mod's id. It has to match the folder's name. |
| `name` | needed | The name the Mods window shows. |
| `version` | needed | Three numbers, like `1.0.0`. |
| `game` | needed | The disc it's for: `"SLUS-01013"` (the North American version), `"SLPS-02170"` (the Japanese one), or a list of both. |
| `api` | needed | The mod API it was made for. See below. |
| `author` | | Who made it. |
| `description` | | A sentence or two about it, for the Mods window. |
| `switches` | | Switches it turns on or off. |
| `scripts` | | The Lua files it runs, in order, each one a path inside the mod's folder. |
| `in_game_toggle` | | `true` to give it a switch in the Mods page of the game's menu (F1). |
| `options` | | Settings the player can choose in the Mods page of the game's menu (F1). See below. |
| `seed` | | `true` if it needs a seed. See below. |
| `requires`, `optional`, `incompatible`, `load_after` | | How it goes with other mods. See below. |

### Ids and versions

An id is lowercase letters, digits, dashes and dots, up to 64 characters, and it starts and ends with a letter or a digit. If you're going to share your mod, I'd name it `yourname.modname`, so it doesn't clash with someone else's mod that happens to have the same name.

A version is three numbers, like `1.2.0`. Other mods can ask for a version of yours, so raise the first number when you change something they might depend on, the second when you add something, and the third when you fix something.

### Switches

A switch changes one rule of the game. A mod sets it with `true` or `false`, and it stays that way while the mod is on. Switches aren't settings, so the only way to change one is with a mod.

| Switch | What it does |
|---|---|
| `always_run` | You run without holding the run button, and holding it makes you walk. |
| `skip_battle_results` | Fights go straight back to the field, skipping the results window, except when someone learns an ability or technique. |
| `no_natural_healing` | Hit points don't come back after fights, over time or on the next screen, only from items, techniques, pets and leaving a dungeon. Someone who fell in a fight gets back up with 1 HP. |
| `skip_movies` | Every movie is skipped, the full-screen ones and the ones that play inside a scene. The game carries on as if it had played. |
| `skip_battles` | Every fight is won as soon as it starts, bosses too. The monsters are defeated the way the game's own scripts defeat them, so drops and story events still happen. |
| `win_regular_battles` | Like `skip_battles`, but only for fights without a boss in them. |
| `win_boss_battles` | Like `skip_battles`, but only for fights with a boss in them. |
| `no_regular_battles` | Walking into monsters doesn't start a fight, unless a boss is with them. Fights the story starts still happen. |
| `no_continue` | The game over screen has no Continue, only the title screen. |
| `continue_from_entrance` | Continue takes you back to the first screen since you came in from the world map or loaded your save, with everything since then undone and the party's hit points full. |
| `continue_without_healing` | Continue tries the screen again with the hit points the party came into it with, not full ones. |
| `japanese_balance` | The North American version plays with the Japanese version's combat and item numbers: monsters level up faster, have the Japanese stats and drops, stats count for more in attack and defense, weaknesses count for less, techniques hit harder, pets' armor protects them, and forging and tempering use the Japanese numbers and costs. It only works on the North American version. |
| `quick_room_changes` | Walking onto an exit to the next screen doesn't fade through black: the last picture of the old room dissolves straight into the new one. With widescreen off it's a quick fade instead. Cutscenes, conversations and the world map keep their own fades. |
| `get_up_with_quarter_hp` | With `no_natural_healing`, someone who fell in a fight gets back up with a quarter of their hit points instead of 1. |

A mod that only sets switches doesn't need any scripts. The ones that come with lom-native are like that. A mod, or one of its choices, can set more than one: in Skip or win battles, winning regular fights instantly turns on `win_regular_battles` and `skip_battle_results`, so those fights don't stop for a results window.

### Scripts

Scripts are Lua 5.4. Each one in `scripts` runs once when the game starts, and that's where you set up what should happen later, with `lom.on`.

```lua
local helpers = require("helpers")

lom.on("battle_end", function()
    if not lom.spend_money(50) then
        lom.log("couldn't pay for the fight")
    end
end)

lom.on("scene_enter", function(scene)
    lom.log("entered scene", scene)
end)
```

They run in a box. They get Lua's basic functions and its `string`, `table`, `math`, `utf8` and `coroutine` libraries, but not `io`, `os`, `debug`, `package`, `load`, `loadfile`, `dofile` or `string.dump`. So a mod can't read or write your files or start programs. `require("a.b")` loads `scripts/a/b.lua` from your own mod and nowhere else. `print` writes to the game's log, like `lom.log`.

`math.random` gives the game's own random numbers, and `math.randomseed` isn't there. That way a recording of a play session replays your mod exactly as it happened. If you need random numbers of your own, use `lom.random(seed)` instead (see Seeds below), so you don't change the game's own rolls.

Each mod gets 32 MB of memory, and each call into it can run for 10 million Lua instructions, except `game_start`, which gets 1,000 million. If a script goes past either, or throws an error, that mod's scripts stop, the game says so on screen, and the reason goes in the log. The other mods keep going.

#### Hooks

`lom.on(hook, function)` runs the function at that point in the game. You can add as many as you like, and they run in the order you added them.

| Hook | When | What it gets |
|---|---|---|
| `scene_enter` | The game has entered a scene. | The scene's number. |
| `battle_start` | A fight has started. | Nothing. |
| `battle_end` | A fight has ended, before the results window. | Nothing. |
| `frame` | Every frame the field is running. | Nothing. |
| `game_start` | A new game has started, or a save was loaded, before the field loads anything. | The game's seed, for a mod with `"seed": true`; otherwise `nil`. |
| `file_load` | The game has read a file from the disc, before it uses it. | The file's path and its bytes. Return changed bytes to change it. |
| `artifact_award` | The game is about to give or unlock a land's artifact. | The land. Return another land to give that one instead. |

The last two can change what the game does. Return `nil`, or nothing, to leave it alone. When more than one mod has the same hook, each gets what the one above it returned.

#### The lom calls

| Call | What it does |
|---|---|
| `lom.api` | The mod API this lom-native has, a number. |
| `lom.on(hook, function)` | Runs the function at a hook. |
| `lom.log(...)` | Writes to the game's log, after your mod's id. |
| `lom.switch(name)` | Whether a switch is on right now, `true` or `false`. |
| `lom.scene()` | The scene the game last entered, or `nil` before the first one. |
| `lom.money()` | The party's lucre. |
| `lom.add_money(amount)` | Gives the party lucre, up to the game's maximum. Returns whether any was added. |
| `lom.spend_money(amount)` | Takes lucre when the party has more than that, like a shop does. Returns whether it was spent. |
| `lom.party()` | The party members who are there, each a table with `slot` (0 to 2), `hp` and `max_hp`. |
| `lom.heal(slot, amount)` | Gives a party member back hit points, up to their maximum. Someone at 0 stays down. Returns whether they were healed. |
| `lom.game()` | Which disc it is: `"SLUS-01013"` or `"SLPS-02170"`. |
| `lom.seed()` | The seed of the game being played, for a mod with `"seed": true`, or `nil`. |
| `lom.disc_file(path)` | A file from the player's disc, as a string of bytes, or `nil` when there's no such file. |
| `lom.disc_files(folder)` | The paths of the files in a disc folder and the folders under it, sorted. Leave out the folder for the whole disc. |
| `lom.unpack(bytes)` | Unpacks a file the game keeps compressed on the disc, the way the game does. |
| `lom.bytes(bytes)` | A copy of some bytes that you can change in place. See below. |
| `lom.random(seed)` | A random number generator of your own. See Seeds below. |

The calls that read or change the game (`money`, `add_money`, `spend_money`, `party` and `heal`) only work inside a hook, while the field is running. Calling one when a script first runs is an error.

### Changing the game's data

Most of what's in Legend of Mana, like what's in the chests, what monsters drop, what shops sell, which artifacts events give and where doors lead, is data the game reads from the disc. A mod can change it as the game reads it, with the `file_load` hook. Nothing on the disc changes, and turning the mod off gives you the game back.

```lua
lom.on("file_load", function(path, bytes)
    if path == "ANA/MAPINFO/BON_1F00.IMG" then
        local file = lom.bytes(bytes)
        file:u16(0x1234, 21)
        return file
    end
end)
```

A few things to know:

- `file_load` gets each file the way the game gets it. Most files are just as they are on the disc. The ones the game keeps compressed, like the overlays in `BIN`, come unpacked. `lom.disc_file` always gives you the disc's own bytes, so use `lom.unpack` on one of those to match.
- A file has to keep its length. If you return bytes of another length, your mod stops.
- Mods see the disc's own layout. If you use the Remaster's text, it's put in after the mods.
- Paths are as they are on the disc, with forward slashes, like `ANA/MAPINFO/BON_1F00.IMG`. Scripts can only read the disc, nothing else.
- The North American and Japanese discs don't have everything in the same place, so check `lom.game()` if you use offsets.

`lom.bytes` is for the editing. `b:u8(offset)`, `b:u16(offset)` and `b:u32(offset)` read a number, and `b:s8`, `b:s16` and `b:s32` read one that can be negative. Give a second number to write it instead: `b:u16(offset, value)`. They're little-endian, like the PS1, and offsets start at 0, the way file formats are written down, not at 1 like Lua's. `#b` is its length and `b:string()` gives you the bytes back. You can return either the buffer or a string from `file_load`.

The game's code doesn't come from the disc, so a few things it does in code have hooks of their own. Right now that's `artifact_award`, which covers every artifact the game gives or unlocks: the ones events give, and the ones a game starts with. Give the same answer for the same land each time, so the event that gives an artifact names the one it really gives. More hooks like this come when a mod needs them.

### Seeds

A mod that makes random choices of its own, like where things go, needs a seed, so the same seed always gives the same game. Put `"seed": true` in `mod.json`, and lom-native looks after it. It needs `"api": 5`.

- A new game gets the seed the player put in the launcher's Mods window, or a new one if they left it empty.
- The seed is kept in the game's saves, so loading a save gives you its seed back. A game never changes seed.
- Every mod that asks gets the same one. Use it however you like.
- `game_start` gets it, and so does `lom.seed()` from then on.

`game_start` runs before the field loads anything, so it's the place to read what you need from the disc and work out what you'll change. It has a much bigger budget than other hooks for that.

```lua
local plan = {}

lom.on("game_start", function(seed)
    local random = lom.random(seed)
    for _, path in ipairs(lom.disc_files("ANA/MAPINFO")) do
        -- read lom.disc_file(path), and decide what goes where
    end
end)
```

`lom.random(seed)` makes a random number generator of your own, separate from the game's. `r:int(n)` gives a whole number from 1 to n, and `r:int(m, n)` from m to n. `r:float()` gives one from 0 up to 1. `r:shuffle(list)` shuffles a list in place and `r:pick(list)` gives one of its items. It's PCG32, and it'll stay PCG32, so a seed gives the same numbers on every computer and every version of lom-native.

Moving things around can break the story. An event might need an artifact that's now somewhere you can't reach, or a door might lead somewhere you can't get out of. lom-native doesn't know the story's rules yet, so that's up to your mod.

### A switch in F1

With `"in_game_toggle": true`, your mod gets a switch in the Mods page of the game's menu, named after it, with its description as the help line. It needs `"api": 3`. When the player turns it off there, your switches stop counting and your hooks stop being called, until they turn it back on. Your scripts keep their state in between, they just don't get called. The game records it, so a replay of a play session plays it back the same way.

### Options in F1

A mod can give the player settings to choose in the Mods page of the game's menu. Each option is a row there, under the mod's switch if it has one, showing the choice it's on. Left and right go through the choices. Each choice turns on its own switches, on top of the mod's, so you build an option out of switches. The player's choice is saved with their other settings and kept for next time. Options need `"api": 4`.

```json
"options": [
  {
    "id": "game_over",
    "name": "After a game over",
    "description": "What Continue does when the party falls.",
    "default": "entrance",
    "choices": [
      { "id": "entrance", "name": "Back to the entrance", "switches": { "continue_from_entrance": true } },
      { "id": "no_continue", "name": "No Continue", "switches": { "no_continue": true } }
    ]
  }
]
```

An option needs an `id`, a `name` and two or more `choices`, each with its own `id` and `name`. Ids are lowercase letters, digits and underscores, starting with a letter. `description`, on the option or a choice, is the help line F1 shows for it. `default` is the id of the choice it starts on, the first one if you leave it out. If the player turns your mod off in F1, its options stop counting too.

No natural healing, which comes with lom-native, has two: what Continue does after a game over, and how much a party member who fell in a fight gets back up with.

### Working with other mods

A mod can say how it goes with other mods. The game and the launcher both check this, and the order in the Mods window never changes because of it. The player decides the order, and gets told when something's off.

```json
{
  "id": "author.hard-mode",
  "name": "Hard mode",
  "version": "1.2.0",
  "game": "SLUS-01013",
  "api": 2,
  "requires":     { "author.core": ">=1.0.0 <2.0.0" },
  "optional":     { "someone.better-ui": ">=2.1.0" },
  "incompatible": { "someone.easy-mode": "*" },
  "load_after":   ["someone.music-pack"]
}
```

- `requires`: mods yours can't run without. If one isn't on, or its version isn't in the range, your mod is refused, and the player sees why.
- `optional`: mods yours works with when they're on. If one is on at a version outside the range, the player gets a note that they may not work together.
- `incompatible`: mods yours can't be on with. If both are on, the launcher won't start the game until the player turns one of them off.
- `load_after`: mods that should be above yours in the list. If one is below it, the player gets a note.

A version range is `*` for any version, or one or more comparisons separated by spaces, all of which have to be true: `>=1.2.0 <2.0.0`, `>1.0.0`, `<=1.9.9`, or just `1.2.3` for exactly that version.

These four need `"api": 2`. A mod that uses them with `"api": 1` is refused, so an older lom-native can't load it and quietly skip its rules.

### The mod API version

`api` says which mod API your mod was made for. Each new version of the API only adds things, so a mod made for an older one keeps working. A mod that asks for a newer one than the lom-native it's on gets refused, with a note that it needs a newer lom-native. Right now there are five:

- `1`: switches, scripts, the first four hooks and the first ten lom calls above.
- `2`: also `requires`, `optional`, `incompatible` and `load_after`.
- `3`: also `in_game_toggle`.
- `4`: also `options`.
- `5`: also `seed`, the `game_start`, `file_load` and `artifact_award` hooks, and `lom.game`, `lom.seed`, `lom.disc_file`, `lom.disc_files`, `lom.unpack`, `lom.bytes` and `lom.random`.

Use the lowest one that has everything your mod needs.

## When something's wrong

Everything about mods goes in the game's log, which Help > Open logs folder in the launcher opens. Each mod's line starts with its id: what was loaded and at which version, why one was refused, why a script stopped, and whatever your scripts wrote with `lom.log` or `print`.

The Mods window shows the same reasons before you play, so it's the quickest place to check a mod's `mod.json` is right.

## Sharing a mod

Zip up the mod's folder. Whoever gets it can unzip it into their mods folder, or use Add mod... in the Mods window and pick the unzipped folder. If they already have a mod with that id, the launcher asks before replacing it.
