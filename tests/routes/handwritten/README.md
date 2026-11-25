# Hand-written routes

Most of the route tests are made by the generators in `tools/testing/lom_routes.py`. They read the scene data off your disc and write a test for each exit, person, battle and so on, trying other ways in until one works on the game. Their route files aren't committed. What is committed is what each one recorded, in `../generated/<area>/<generator>.json`: one file per generator per area, with each route's starting save, the plan the run replays, and fingerprints of what it should see. So a generated route always plays the way it did when it was recorded, whatever the generators would choose today. Those files only keep fingerprints, so when one fails, `lom_routes.py compare` runs it on two builds and tells you what's different.

The ones in here are different. These are custom tests for things the generators can't produce on their own, because they don't follow the usual pattern. Things like starting a new game and getting through the opening, saving at the desk and reading the save back, walking a tour through town, hearing the church's second song, or the world map's events. Each one needs a particular starting point or a particular set of steps, so they were written one at a time.

A few were made by one-off scripts instead: the `events/` routes (a scene's own event, from a save built so it runs) and the `story-chain/` routes (a story write that needs the state another route ends in). Those scripts aren't in the tools, so they count as hand-written here.

Nothing can make them again, so they're committed. If you change one, treat it like changing any other test.

- `saves/` has the starting saves: a new game in the bedroom, the same after the opening (built, and the card the game itself wrote at the end of `newgame/opening-save`), and the `chain-*` saves the story-chain routes start from. The generators start from the new-game one too.
- `home/`, `newgame/`, `domina/`, `wmap/events/`, `wmap/esl/` (the world map coming back from Lucemia), `<area>/events/` and `<area>/story-chain/` are the tests themselves, and so are `always-run/` and `widescreen/`, which test those features, and `mods/`, where each route runs with a set of mods from `tests/mods/`.

The Japanese game's routes are in `../jp/`, hand-written too. They only run when the build knows where your Japanese disc is.

Every test has a `.baseline.json` next to it. That's the recorded result it gets checked against, and it says in plain lines what changed at each checkpoint. You don't edit it by hand. Run `lom_routes.py record` to record it again, and review the diff before committing it.
