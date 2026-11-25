# Installing a release

The [README](../README.md) has what you need and where to get it. This is the
rest: getting each package to open the first time, the Steam Deck, and
keeping everything on a USB stick.

## Linux

The AppImage runs on most distros from the last few years, anything with glibc 2.35 or newer, like Ubuntu 22.04, Debian 12 or Fedora 36. Downloaded files usually aren't allowed to run, so make it executable first:

```sh
chmod +x lom-native-*.AppImage
./lom-native-*.AppImage
```

You can also do that in your file manager: right-click it, open Properties, and allow it to run as a program.

The Flatpak isn't on Flathub, so you install the file yourself. The first time, Flatpak also downloads the KDE runtime it needs from Flathub. After that it's in your app menu:

```sh
flatpak install --user lom-native-*.flatpak
flatpak run io.github.celophi.lom_native
```

The Flatpak can only see Steam's main library. If you have the Remaster in a library on another drive, choose its folder in the launcher's Game files window.

### Steam Deck

Use the AppImage. In Desktop Mode, put it somewhere like your home folder, make it executable as above, then right-click it and choose Add to Steam. Start it from Game Mode after that. Use the touchscreen in the launcher; the game itself plays with the Deck's controls. I haven't tried this on a Deck myself yet, so let me know how it goes.

## macOS

It's for Apple Silicon Macs with macOS 13 or newer. Drag Legend of Mana Native into Applications and open it from there.

The first time, macOS won't open it, because the app isn't signed with an Apple Developer ID. Sorry, I don't want to pay Apple $99 a year just to sign an app at this time. To open it anyway, click Done on the warning, then go to System Settings > Privacy & Security, scroll down to Security, and click Open Anyway next to Legend of Mana Native. You only have to do this once for each new version.

If there's no Open Anyway button, or the launcher opens but the game won't start, run this in Terminal and open the app again:

```sh
xattr -dr com.apple.quarantine "/Applications/Legend of Mana Native.app"
```

The `.dmg` has the same steps in `Read me first.txt`.

## Windows

It's for 64-bit Windows 10 or 11. Unzip the Legend of Mana Native folder anywhere and run `Legend of Mana Native.exe`. Nothing needs installing.

The first time, Windows may say "Windows protected your PC", because the app isn't signed with a code signing certificate. Sorry, I don't want to pay for one right now. Click More info, then Run anyway. To skip the warning, right-click the zip before unzipping it, choose Properties, tick Unblock and click OK.

## Portable mode

Normally the launcher and the game keep your settings, saves, game files and logs in your own user folders ([troubleshooting.md](troubleshooting.md#logs) says where). To keep all of that next to the game instead, say on a USB stick:

- With the Windows zip, put an empty file called `portable.txt` next to `Legend of Mana Native.exe`. Everything then goes in a `user` folder there.
- With the AppImage, run it once with `--appimage-portable-home`. That makes a folder next to it with the same name plus `.home`, and from then on everything goes in there. Making that folder yourself works too.

The folder has to be one you can write to, so not somewhere like Program Files. The Flatpak and the Mac app don't have a portable mode. Your existing saves don't move over by themselves; copy the `cards` folder across if you want them.
