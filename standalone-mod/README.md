# DinoTale 32:9 — installable mod v0.2.2

True 32:9 Expand gameplay in the **official Windows DinoTale 0.1.8**.
The package contains a normal `.nrm` mod and a companion native DLL.
It uses DinoTale's own mod loader and does not replace or edit `DinoTale.exe`.
This mod is dedicated solely to 32:9 support.

Version 0.2.2 expands the shared placement checks so standard enemies,
items and other placed objects load around the wider view's edges and
remain active across it. It preserves actor margins, vertical limits and
respawn rules. Individual scripted and proximity-based events keep their
original rules.

## Install

1. Close DinoTale.
2. Copy **both** `DinoTale-32x9.nrm` and `DinoTale32x9.dll` into
   `%LOCALAPPDATA%\dinotale\mods`.
   In portable mode, use the `mods` folder beside `DinoTale.exe` instead.
3. Start official DinoTale 0.1.8 and enable **DinoTale 32:9** in its Mods menu
   before starting the game.
4. Set **Graphics > Aspect Ratio > Expand**, use fullscreen on your 32:9
   display, and then enter a level.

Keep the two files together. Importing the `.nrm` by itself through the UI
does not install the companion DLL. The mod can only be toggled before
gameplay. To remove it, close DinoTale and remove the two files.

## Compatibility and behavior

This version supports the official Windows 0.1.8 executable with SHA-256:

`2cb1a2ee3f01ef75eaaae98485b0022e3b6991a5181aa71f824a7641f362cf8b`

Other versions and custom builds are rejected before any changes are made.
It is not a Linux release. A future DinoTale update needs a compatible mod
update; there is no wildcard scanning or guessing of replacement locations.

The native companion changes five checked locations in process memory: the
game's aspect limit, its precomputed maximum tile-column count, the renderer
limit, the original game's heap end, and its tile-request allocation
threshold. The heap can use the otherwise
unused upper 4 MB while staying below the runtime's reserved addresses.
Changes are applied before game startup. The executable on disk remains
unchanged.

Two checked native hooks adapt the common placement and retention
predicates using DinoTale's existing mode-aware screen mapping. Their
coordinate inputs are restored after each call. Camera width, world
positions, drawing and positional audio remain unchanged. Original mode
uses its original bounds.

Version 0.2.1 corrects an original allocation threshold that treated the
104-byte request pool as if it held 104 two-byte tile IDs. At 32:9, 54/55-ID
requests overwrote adjacent requests and caused missing rectangular tiles.
Requests larger than 52 IDs now use the game's existing heap path.

Expand is experimental. Menus retain their original framing, and scenery
ends where the original level art ends. Existing upstream object-visibility
limitations remain. Choose fullscreen/32:9 before entering a level; after
widening the window, leave and re-enter the level. This mod retains the
official build's HUD options and resize behavior.

## Validation

- Verified against a freshly downloaded official Windows 0.1.8 package.
- Mod loader accepted the `.nrm` and loaded its companion DLL.
- Read back the checked patch locations in the running official game and confirmed
  their replacement values; the game booted and its audio task ran.
- Verified the executable's SHA-256 stayed unchanged after loading the mod.
- Patch-plan, bounds, byte-mismatch, hash-streaming, restoration-plan and
  unsupported-host rejection tests passed.
- The tile-request regression reproduces neighboring-slot corruption for
  53/54/55-entry requests and verifies that the corrected threshold avoids it.
- After installation in regular DinoTale, the user confirmed that the
  missing Cloud Cruising platform and wave tiles are fixed.
- Visibility tests cover exact spawn/retain edges at 320, 432, 560 and
  864-pixel view widths, actor margins, vertical rejection, Original mode,
  and restoration of coordinate inputs despite argument-register changes.
- Native-entry guards and both installed placement hooks are checked in
  the running official game.
- Live counters confirmed the expanded checks ran during gameplay, and
  the user confirmed enemies/items now appear across the wider view.

Full level coverage at 32:9 is not claimed.

## Source and build

Source: https://github.com/Bentley734/DinoTale-Ultrawide

Run `build-native.ps1`, then `package-nrm.ps1` from this directory. The native
build needs Visual Studio x64 C++ tools and clang-cl. Pass `-ClangCl` for a
separate compiler installation. The `.nrm` packager needs Python and the
bundled N64Recomp `RecompModTool` target built first. `make_bootstrap.py`
generates an inert function from our own two instructions; no ROM is used
to build or distribute the mod.

Upstream: https://github.com/wootbeer/dinotale

GPL-3.0; see COPYING. No game data is included.
