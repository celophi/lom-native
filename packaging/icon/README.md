# The app icon

`lom-native.png` is the icon, 1024 by 1024 with a transparent background.
Everything else is made from it:

- The Mac app's icon, when `packaging/macos/build.sh` runs.
- `packaging/linux/io.github.celophi.lom_native.png`, the 512 by 512 copy
  Linux desktops and the Flatpak use.
- `packaging/windows/lom-native.ico`, which goes into the launcher's and the
  game's `.exe`.

After changing it, make the other two again with ImageMagick:

```sh
magick packaging/icon/lom-native.png -resize 512x512 -strip PNG32:packaging/linux/io.github.celophi.lom_native.png
for s in 16 24 32 48 64 128 256; do magick packaging/icon/lom-native.png -resize ${s}x$s PNG32:/tmp/icon-$s.png; done
magick /tmp/icon-{16,24,32,48,64,128,256}.png packaging/windows/lom-native.ico
```
