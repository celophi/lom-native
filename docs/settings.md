# Settings

F1 or the controller's Guide button opens the port's settings while you play.
Out of the box it's the PS1 game, apart from the Remaster if you have it
(below), which starts all on. Everything else here is off until you turn it on.

The pages are in the same order as here: Display, Audio, Controls, General,
Remaster and Mods.

## Display

**Scaling filter** is how the PS1's picture is stretched to fill your window. F5 switches between them while you play.

- Sharp, the default, keeps the pixels crisp and softens the edges between them a little.
- Integer scales by whole pixels, so every pixel is the same size. That leaves a border.
- Smooth is bilinear, softer than the original.
- xBR rounds off diagonal edges and keeps the shapes of the pixel art.

**Internal resolution** is 1x, 2x or 4x, and 1x by default. It draws the game's polygons at a higher resolution, so their edges are cleaner. The game's own artwork keeps its pixels. F7 switches it while you play.

**Fullscreen** is off by default. F4 does the same.

**Widescreen** is off by default. With it on, towns, dungeons, fights and the world map show more of the world on each side, at 16:9. The world map keeps its corners where they were, the way the Remaster does. Places without the art for it, the menus and the movies stay 4:3 (apart from the Remaster's movies, below), and during a fight nobody can walk into the extra space. The title screen and the wall map you place your home on at the start are wide only with the Remaster's art (below), and so are name entry and the game's menu, which then take the Remaster's layout.

**Restore display defaults** puts these back: Sharp, 1x, 4:3, in a window.

## Audio

**Volume** is everything, movies included, and each sound setting has its own mute that keeps the volume where you left it.

**Music** and **Sound effects** each have their own volume, under the main Volume. The original game only has the one. Movies follow the main Volume only, since their music and sound come mixed together.

**Restore audio defaults** puts everything back at full volume, not muted.

## Controls

Pick Player 1 or Player 2 at the top, then each PS1 button has its key and its gamepad button. Select a button with a key to change its key, or with a gamepad button to change its gamepad button, then press the new one. F2 clears it, and Esc cancels. The keyboard and a gamepad work at the same time.

Some keys are kept for the port and can't be used: Esc, F1, F2 and F4 to F7. The Guide button is kept for opening the settings.

Player 2 is for the game's own two-player mode. Recruit a companion in the game, then give player 2 a controller or some keys.

**Restore player defaults** puts that player's keys and gamepad buttons back the way they started.

## General

**Pause when unfocused** is off by default. With it on, the game and its sound pause when you switch to another window, and carry on when you come back.

## Mods

The Mods page lists the mods you turned on in the launcher that can be switched off and on while you play, like the ones that come with lom-native. Turning one off there lasts until you close the game. Which mods load at all is up to the launcher's Mods window.

Always run is one of those mods, not a setting. Turn on Always run in the launcher's Mods window. The original game walks unless you hold the run button (Circle or L1 on the North American version, Cross or L1 on the Japanese one). With the mod on it's the other way round: you run, and holding the button makes you walk.

Some mods also have options, shown under them on the Mods page. Left and right change them, and they're kept for next time.

Skip or win battles has two. Regular fights, the ones that start when you walk into monsters, can be fought, skipped or won as soon as they start. Boss fights can be fought or won as soon as they start; they can't be skipped, since the story needs them won. Skipped fights give no experience or drops, so you may find a boss harder.

Quick fights and No natural healing are mods too, not settings. Quick fights takes you straight back to the field after a fight instead of showing the items and the party summary. The window still opens when someone learns an ability or technique, since you can't see that anywhere else right away, and you still get everything. No natural healing stops your hit points coming back over time, after fights and when you go on to the next screen, so they only come back from items, techniques, pets and leaving a dungeon, and a long dungeon wears you down. Its option, After a game over, says what Continue does. Back to the entrance, the default, takes you back to where you came in from the world map with everything since then undone. No Continue sends you to the title screen. Retry without healing tries the screen again with the hit points you came into it with.

[modding.md](modding.md) has the rest about mods.

## Language

The launcher and the game's own settings (F1) follow your system's language if they have it: English, German, French, Spanish, Japanese, Korean or Chinese (Simplified or Traditional). You can pick another in the launcher's Settings, and F1 follows it. That's only the port's own screens; the game's text is the Remaster's Text switch below.

## The Remaster

If you also own the Legend of Mana Remaster on Steam, the game can use its art, music, sound and text. The launcher finds it in your Steam libraries, or you can choose its folder or turn it off in the launcher's Game files window. Nothing is copied; the game reads it where it's installed. The Flatpak can only look in Steam's main library, so if yours is on another drive, choose its folder there.

With it, F1's Remaster page has a switch for all of it, **Use the Remaster**, and under it a switch for each part, like a mod's options. They all start on, and the game remembers them. Turning the top switch off turns everything back to the PS1's without losing how the parts are set.

- **Backgrounds**. The Remaster's repainted backgrounds, from the next room you enter. Set Internal resolution to 4x to see all of their detail.
- **Faces**. The repainted faces in conversations.
- **Encyclopedia**. The encyclopedia's repainted pictures.
- **Title screen**. The title screen's background and logo, wide with Widescreen on, and its menu, cursor and copyright line. With the Japanese disc the menu's words stay its own, since the Remaster's are in English.
- **Name entry**. Naming your hero, pets and golems on the Remaster's screen: its background, with the buttons in a column (the Remaster's keyboard button deletes a letter, as the PS1's arrow does, and shows the arrow), its typewriter and paper, and the tabs moved onto the wood. Up and down move through the buttons there. It's painted for a wide screen, so only with Widescreen on; in 4:3 it's the PS1's.
- **Wall map**. The wall map you place your home on at the start, wide with Widescreen on.
- **World map**. The world map's Mana Tree panel, spirits, and day and land names. The lands stay the PS1's.
- **Game menu**. The game's menu with the Remaster's tab icons. With Widescreen on it also takes the Remaster's background and layout: the characters and item lists in a column down the side, the open one's pages in a second column next to it, Return in its own box below, and the pages laid out wider. Up and down move through a column, right goes from a tab into its pages and left goes back. The L1 and R1, and L2 and R2, prompts are the Remaster's: key caps named as it names them (LB and RB, LT and RT) with what the buttons do. That's with the North American disc only, since the Remaster's words for them can't be shown on the Japanese one.
- **Save screens**. Loading and saving on the Remaster's screen: its parchment background, with a row for each save showing the hero's face, play time, place and name, and Newest on the latest, all in the Remaster's own lettering, as are its questions and messages, which come up in the Remaster's dark window. Japanese, Korean and Chinese text keeps the PS1's letters. The two memory card tabs are still there, below the rows. It's painted for a wide screen, so only with Widescreen on; in 4:3 it's the PS1's.
- **Movies**. The Remaster's high-definition opening, endings and staff roll in place of the PS1's, from the next movie. They're 16:9, so with Widescreen off they're letterboxed. The short movies inside scenes stay the PS1's.
- **Music**. The Remaster's recordings of the soundtrack, from the next song. Under it, **Recordings** picks its original soundtrack or the arranged one.
- **Sound effects**. The Remaster's recordings of the sound effects.
- **Text**. The Remaster's revised text, from the next room or menu. Under it, **Language** picks English, German, French, Spanish, Japanese, Korean or Chinese. Only with the North American disc; the Japanese one keeps its own text, since it's stored differently.
- **Restore Remaster defaults** turns everything back on, with the original recordings and English.

Settings from before these switches carry over: HD pictures turned off turns off every picture's switch, and a soundtrack, sound effects or text you'd picked from the Remaster stays picked.

With the Remaster in use, fading to black between screens darkens the whole picture evenly, as the Remaster's fades do. The PS1 takes the same amount off every colour, which turns the Remaster's brighter art red and leaves white text showing after the rest has gone.

With Widescreen on too, rooms the Remaster painted wider show its extra art at the sides. In a few rooms the Remaster moved walls a little to fit its new art, and with Backgrounds on you play those rooms its way.
