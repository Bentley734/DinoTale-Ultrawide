# DinoTale Ultrawide

## Current release: installable 32:9 mod

[Download v0.2.3](https://github.com/Bentley734/DinoTale-Ultrawide/releases/latest)
for the **official Windows DinoTale 0.1.8**. Copy both
`DinoTale-32x9.nrm` and `DinoTale32x9.dll` into
`%LOCALAPPDATA%\dinotale\mods` (or the portable game's `mods` folder),
enable **DinoTale 32:9** before gameplay, choose **Expand**, and enter a level
in fullscreen on your 32:9 display. The official executable stays unchanged.

See [standalone-mod/README.md](standalone-mod/README.md) for compatibility,
installation, validation and source build instructions. This mod only
supports its checked official Windows 0.1.8 executable; other versions are
rejected. Set fullscreen before entering a level and reload the level after
widening the window.

Version 0.2.1 fixes the missing rectangular platform and wave tiles in
Cloud Cruising by correcting an undersized tile-request pool allocation.
Version 0.2.2 extends the common enemy/item placement and retention checks
to the expanded view, so placed objects load before entering its edges.
Version 0.2.3 tiles repeating decorative backdrops backward into the exposed
left edge. The rule applies across levels to collision-free layers with a
proven repeated chunk pattern or authored repeat, using the shortest visual
period to align the seam. The backdrop fill changes only negative-X tiles and
preserves vertical guards, foreground terrain and Original mode.
Transparent foreground edges reveal the filled backdrop after scrolling,
and normal horizontal camera follow eases movement before the game's wall
checks. Initialization and scripted camera paths retain their behavior.
The user confirmed that the background stays visible after scrolling and
the camera moves more smoothly. Complete level-by-level testing is not claimed.

## Earlier custom build

A dedicated **32:9** build of
[DinoTale by wootbeer](https://github.com/wootbeer/dinotale), based on 0.1.8.
Yoshi and the scenery keep their proportions while the game streams a wider
view of each level. This project focuses solely on ultrawide support.

Download the Windows build from
[this fork's releases](https://github.com/Bentley734/DinoTale-Ultrawide/releases).

## Ultrawide setup

1. Extract the Windows ZIP into its own folder and run `DinoTale.exe`.
2. Select your own US Yoshi's Story game file when asked.
3. Before entering a level, set **Settings > Graphics > Aspect Ratio > Expand**
   and use fullscreen on your ultrawide monitor.
4. Choose **HUD Placement > 16:9** for a centered HUD, **Original** for 4:3,
   or **Expand** for the full width.

The level allocates its backgrounds when it starts. If you widen the window
after entering a level, leave and re-enter it to use the extra width. Until
then, the renderer limits the view to the allocated background size.
Menus and storybook screens retain their original framing. Original,
Stretch and Hybrid remain available.

This build uses its own `dinotale-ultrawide` settings/save folder. To carry
over progress, close both builds and copy the `saves` folder from
`%LOCALAPPDATA%\dinotale` to `%LOCALAPPDATA%\dinotale-ultrawide`. A blank
`portable.txt` beside the executable keeps this build's data in its folder.

Expand remains experimental. The original game's art can end before the
extra view at level boundaries; black areas there are not new scenery.
Some upstream actor-culling issues, including 4-1's blue blocks, remain.

## Upstream project

Yoshi's Story (N64, 1997/98), running natively on PC. The game is statically
recompiled with [N64Recomp](https://github.com/N64Recomp/N64Recomp), runs on
[N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) and is
rendered with [RT64](https://github.com/rt64/rt64). For Windows and Linux,
including the Steam Deck.

Unofficial, and not affiliated with the rights holders. **No game files are
included:** you need your own copy of the game.

## Features

- Runs natively on Windows and Linux.
- **Widescreen:** play in 4:3, stretched, or true widescreen (Expand) that
  shows more of each level, up to 32:9 in this build.
- HUD at the edges of the window in widescreen, or kept in its original place.
- Screen filters: CRT, scanlines, LCD, and the xBR, xBRZ and HQ2x upscalers.
- Keyboard and controller support with remappable controls.
- Saves and sound.

## Download and play

Get the latest build from
[Releases](https://github.com/wootbeer/dinotale/releases):

- **Windows:** unzip it anywhere and run `DinoTale.exe`.
- **Linux / Steam Deck:** extract the `.tar.gz` and run `DinoTale`. On the
  Deck, run it from Desktop Mode, or add it to Steam as a non-Steam game to
  play in Game Mode.

On first launch, pick your copy of the game. It's checked against the hash
below and never modified. Settings (graphics, controls, sound) are in the
in-game menu.

### Portable mode

Settings, saves and mods normally go in `%LOCALAPPDATA%\dinotale-ultrawide` (Windows)
or `~/.config/dinotale-ultrawide` (Linux). Add a blank `portable.txt` next to
`DinoTale.exe` (or `DinoTale`) to keep them in the game's own folder instead,
for example on a USB stick. Copy existing settings and saves over by hand.

### Game files

Yoshi's Story (USA) (En,Ja). Any byte order (`.z64`, `.n64`, `.v64`) works,
and it can be picked inside a `.zip` or `.7z` (extract a `.rar` first).
SHA-1 of the `.z64`: `B13072FEF6C6DF48C07D8822C01E5BC59036F6DA`

## Display options

Settings > Graphics > Aspect Ratio:

- **Original:** the game's 4:3 picture, with black bars at the sides.
- **Stretch:** the 4:3 picture stretched to fill the window.
- **Hybrid** (experimental): backgrounds stretched to the window, sprites kept in shape in
  the middle 4:3 area; the stretched sides are dimmed (Hybrid Dimming sets
  how much, down to not at all).
- **Expand** (experimental): true widescreen. The game shows more of each level. Choose it
  before starting a level. HUD Placement sets whether the HUD moves out to
  the window edges.

### Lossless Scaling frame generation

Settings > Graphics > Frame Generation can use
[Lossless Scaling](https://store.steampowered.com/app/993090/Lossless_Scaling/)'s
frame generation (LSFG 3.1) instead of RT64's own, built in, with no
separate app running. It needs your own copy of Lossless Scaling's
`Lossless.dll`, from its install folder: press **Load Lossless.dll** and pick
it (it's copied into DinoTale's folder; nothing from Lossless Scaling comes
with DinoTale). Then set Frame Generation to **Lossless Scaling** and restart:
it runs on the Vulkan graphics API, which it switches to. Multiplier sets 2x,
3x or 4x the game's frame rate; Performance mode is lighter on the GPU. It
adds about one game frame of input delay. Windows and Linux.

## Reporting issues

Found something wrong? Open an
[issue](https://github.com/wootbeer/dinotale/issues) with a screenshot, the
level and roughly where it happened, and which aspect ratio you were using.

## Building from source

See [BUILDING.md](BUILDING.md). What changed in each version is in
[CHANGELOG.md](CHANGELOG.md); [STATUS.md](STATUS.md) is the full development
log.

## Credits

- [N64Recomp and N64ModernRuntime](https://github.com/N64Recomp) by Mr-Wiseguy
  and contributors, which this port is built with
- [RT64](https://github.com/rt64/rt64) by Dario and contributors, the renderer
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) by the N64Recomp
  contributors, the launcher and input layer
- The [Yoshi's Story decompilation](https://github.com/decompals/yoshis-story)
  by decompals, whose ROM layout, symbol names and overlay loader research
  this project builds on
- [splat](https://github.com/ethteck/splat), `spimdisasm` and `rabbitizer`,
  used to find the game's functions

## License

The project's own code is GPL-3.0; see [COPYING](COPYING). The game itself
remains the property of its rights holders.
