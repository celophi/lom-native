# Modding

You can change how the game plays with mods. It's early days, so there isn't a lot a mod can do yet, and what's here may still change. A mod can turn on some of the port's switches, and it can run Lua scripts that hook into the game.

## Using mods

The launcher's Mods button opens the Mods window. It lists every mod in your mods folder, with a checkbox to turn each one on or off, and buttons to move them up and down, add a mod, remove one, and open the mods folder. Mods load when the game starts, so a change counts from the next time you press Play.

A few mods come with lom-native: Always run, Quick fights, No natural healing, Skip movies, Skip battles and Japanese balance. They're turned off to start with. You can turn them on and off, but not remove them.

Order matters. When two mods set the same thing, the one lower in the list wins, and the Mods window says which mod overrides which.

Some mods can also be turned off and on while you play, from the Mods page in the game's menu (F1). The ones that come with lom-native all can, except Japanese balance, which is on or off for the whole time you play. Only mods you turned on in the launcher show up there. Turning one off in F1 lasts until you close the game; the next time you press Play, the launcher's Mods window decides again.

If the game won't load a mod, the Mods window grays it out and says why. The same goes for the game: it says so on screen when it starts, and the reason is in the log.

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
| `game` | needed | The disc it's for: `"SLUS-01013"` (the US version), `"SLPS-02170"` (the Japanese one), or a list of both. |
| `api` | needed | The mod API it was made for. See below. |
| `author` | | Who made it. |
| `description` | | A sentence or two about it, for the Mods window. |
| `switches` | | Switches it turns on or off. |
| `scripts` | | The Lua files it runs, in order, each one a path inside the mod's folder. |
| `in_game_toggle` | | `true` to give it a switch in the Mods page of the game's menu (F1). |
| `requires`, `optional`, `incompatible`, `load_after` | | How it goes with other mods. See below. |

### Ids and versions

An id is lowercase letters, digits, dashes and dots, up to 64 characters, and it starts and ends with a letter or a digit. If you're going to share your mod, I'd name it `yourname.modname`, so it doesn't clash with someone else's mod that happens to have the same name.

A version is three numbers, like `1.2.0`. Other mods can ask for a version of yours, so raise the first number when you change something they might depend on, the second when you add something, and the third when you fix something.

### Switches

A switch changes one rule of the game. A mod sets it with `true` or `false`, and that beats the player's own setting in F1 while the mod is on.

| Switch | What it does |
|---|---|
| `always_run` | You run without holding the run button, and holding it makes you walk. |
| `skip_battle_results` | Fights go straight back to the field, skipping the results window, except when someone learns an ability or technique. |
| `no_natural_healing` | Hit points don't come back after fights or over time, only from items, techniques, pets and leaving a dungeon. |
| `skip_movies` | Every movie is skipped, the full-screen ones and the ones that play inside a scene. The game carries on as if it had played. |
| `skip_battles` | Every fight is won as soon as it starts, bosses too. The monsters are defeated the way the game's own scripts defeat them, so drops and story events still happen. |
| `japanese_balance` | The US version plays with the Japanese version's combat and item numbers: monsters level up faster, have the Japanese stats and drops, stats count for more in attack and defense, weaknesses count for less, techniques hit harder, pets' armor protects them, and forging and tempering use the Japanese numbers and costs. It only works on the US version. |

A mod that only sets switches doesn't need any scripts. The ones that come with lom-native are like that. A mod can set more than one: Skip battles turns on `skip_battles` and `skip_battle_results`, so a story-only playthrough doesn't stop for a results window after every fight.

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

`math.random` gives the game's own random numbers, and `math.randomseed` isn't there. That way a recording of a play session replays your mod exactly as it happened.

Each mod gets 32 MB of memory, and each call into it can run for 10 million Lua instructions. If a script goes past either, or throws an error, that mod's scripts stop, the game says so on screen, and the reason goes in the log. The other mods keep going.

#### Hooks

`lom.on(hook, function)` runs the function at that point in the game. You can add as many as you like, and they run in the order you added them.

| Hook | When | What it gets |
|---|---|---|
| `scene_enter` | The game has entered a scene. | The scene's number. |
| `battle_start` | A fight has started. | Nothing. |
| `battle_end` | A fight has ended, before the results window. | Nothing. |
| `frame` | Every frame the field is running. | Nothing. |

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

The calls that read or change the game (`money`, `add_money`, `spend_money`, `party` and `heal`) only work inside a hook, while the field is running. Calling one when a script first runs is an error.

### A switch in F1

With `"in_game_toggle": true`, your mod gets a switch in the Mods page of the game's menu, named after it, with its description as the help line. It needs `"api": 3`. When the player turns it off there, your switches stop counting and your hooks stop being called, until they turn it back on. Your scripts keep their state in between, they just don't get called. The game records it, so a replay of a play session plays it back the same way.

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

`api` says which mod API your mod was made for. Each new version of the API only adds things, so a mod made for an older one keeps working. A mod that asks for a newer one than the lom-native it's on gets refused, with a note that it needs a newer lom-native. Right now there are three:

- `1`: switches, scripts, the hooks and the lom calls above.
- `2`: also `requires`, `optional`, `incompatible` and `load_after`.
- `3`: also `in_game_toggle`.

Use the lowest one that has everything your mod needs.

## When something's wrong

Everything about mods goes in the game's log, which Help > Open logs folder in the launcher opens. Each mod's line starts with its id: what was loaded and at which version, why one was refused, why a script stopped, and whatever your scripts wrote with `lom.log` or `print`.

The Mods window shows the same reasons before you play, so it's the quickest place to check a mod's `mod.json` is right.

## Sharing a mod

Zip up the mod's folder. Whoever gets it can unzip it into their mods folder, or use Add mod... in the Mods window and pick the unzipped folder. If they already have a mod with that id, the launcher asks before replacing it.
