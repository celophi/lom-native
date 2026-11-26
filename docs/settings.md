# Settings

F1 or the controller's Guide button opens the port's settings while you play.
Out of the box it's the PS1 game, apart from the Remaster's art if you have it
(below). Everything else here is off until you turn it on.

The pages are in the same order as here: Display, Audio, Controls, General and
Mods.

## Display

**Scaling filter** is how the PS1's picture is stretched to fill your window. F5 switches between them while you play.

- Sharp, the default, keeps the pixels crisp and softens the edges between them a little.
- Integer scales by whole pixels, so every pixel is the same size. That leaves a border.
- Smooth is bilinear, softer than the original.
- xBR rounds off diagonal edges and keeps the shapes of the pixel art.

**Internal resolution** is 1x, 2x or 4x, and 1x by default. It draws the game's polygons at a higher resolution, so their edges are cleaner. The game's own artwork keeps its pixels. F7 switches it while you play.

**Fullscreen** is off by default. F4 does the same.

**Widescreen** is off by default. With it on, towns, dungeons and fights show more of the world on each side, at 16:9. Places without the art for it, the world map, the menus and the movies stay 4:3, and during a fight nobody can walk into the extra space.

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

The launcher and the game's own settings (F1) follow your system's language if they have it: English, German, French, Spanish, Japanese, Korean or Chinese (Simplified or Traditional). You can pick another in the launcher's Settings, and F1 follows it. That's only the port's own screens; the game's text is the Remaster's Text setting below.

## The Remaster

If you also own the Legend of Mana Remaster on Steam, the game can use its art, music, sound and text. The launcher finds it in your Steam libraries, or you can choose its folder or turn it off in the launcher's Game files window. Nothing is copied; the game reads it where it's installed. The Flatpak can only look in Steam's main library, so if yours is on another drive, choose its folder there.

With it, the settings have these:

- **HD backgrounds** (Display), on by default. The Remaster's repainted backgrounds, from the next room you enter. Set Internal resolution to 4x to see all of their detail.
- **HD pictures** (Display), on by default. The Remaster's repainted faces in conversations, and its encyclopedia pictures.
- **Text** (Display). The game's text, or the Remaster's revised English or its German, French, Spanish, Japanese, Korean or Chinese. Only with the North American disc; the Japanese one keeps its own text, since it's stored differently.
- **Soundtrack** (Audio). The PS1's, the Remaster's, or the Remaster's arranged one. It changes from the next song.
- **Effects** (Audio). The PS1's sound effects, or the Remaster's recordings of them.

With Widescreen on too, rooms the Remaster painted wider show its extra art at the sides. In a few rooms the Remaster moved walls a little to fit its new art, and with HD backgrounds on you play those rooms its way.
