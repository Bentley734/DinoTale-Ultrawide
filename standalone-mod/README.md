# DinoTale 32:9 — installable mod v0.2.4

True 32:9 Expand gameplay in the **official Windows DinoTale 0.1.8**.
The package contains a normal `.nrm` mod and a companion native DLL.
It uses DinoTale's own mod loader and does not replace or edit `DinoTale.exe`.
This mod is dedicated solely to 32:9 support.

Version 0.2.4 extends walkable ground to the left edge of the wider view at
eligible level starts. It continues the existing edge's collision profile
and uses matching ground material. The rule applies across levels with
static flat support at the edge; starts containing scripted or dynamic
edge cells retain their existing boundary. Ground art fills when the level
first loads, and static terrain contacts recognize the added ground.
The camera keeps its original level bounds.

Version 0.2.3 extends repeating decorative backdrops into the exposed left
edge of the 32:9 view. It works across levels by recognizing a repeated
chunk pattern or the layer's authored repeat flag. The shortest proven
visual period keeps the added tiles aligned with the original scenery.
Only collision-free decorative layers qualify; foreground terrain and
vertical boundaries keep their original behavior.

Transparent foreground edges reveal the extended backdrop after scrolling,
and normal horizontal camera follow eases into movement and stops. The
game's wall checks still limit camera motion.

The earlier fixes remain: wider enemy/item placement and retention checks,
plus the correction for missing rectangular background tiles. Individual
scripted and proximity-based events keep their original rules.

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
coordinate inputs are restored after each call. Camera width, existing actor
world positions and positional audio remain unchanged. Original mode
uses its original bounds.

Additional checked hooks fill negative-X background tile requests and
suppress the left-edge black mask for eligible repeating backdrops. They
also inspect the actual foreground strip in CI8, IA8 and I8 layers: when
that strip is transparent, it reveals the filled sky beneath it. The mask
decision is made before publication to the render thread, avoiding a race
between publishing a black cover and clearing it. Positive-X tiles,
vertical guards, foreground/collision data and Original mode remain intact.
Decorative layers without a proven pattern or authored repeat retain their
existing boundary.

The ground continuation is limited to a finite strip outside the authored
left edge, calculated from the extra visible width and the player's
projection scale, with room for collision probes. Its material motif uses
only columns whose complete collision profiles equal the existing edge.
Collision queries repeat that edge profile, while positive-X terrain,
interior pits, slopes and vertical guards keep their authored values.
The left-edge crossing test and position correction use the expanded
limit together, and point-validity queries recognize the same finite strip.
Right edges, void starts and Original mode keep their existing behavior.

Initial ground tile requests use their own layer descriptor and bounded
fill, independently of player/camera association. Static point-contact
readers map negative-X samples in the eligible strip to the existing edge,
then restore the input coordinate. Scripted/dynamic edge rejection keeps
this path away from collision writes and actor-spawn behavior. Native
caller checks exclude recursive, horizontal and vertical camera probes,
preserving their original boundary checks.

Horizontal velocity easing runs only in normal Expand camera follow,
before the game's original wall probe. Camera/player changes, teleports,
long update gaps and leaving Expand reset the easing state. A checked
native caller gate preserves initialization and scripted camera paths.
Vertical camera movement keeps its original behavior.

Version 0.2.1 corrects an original allocation threshold that treated the
104-byte request pool as if it held 104 two-byte tile IDs. At 32:9, 54/55-ID
requests overwrote adjacent requests and caused missing rectangular tiles.
Requests larger than 52 IDs now use the game's existing heap path.

Expand is experimental. Menus retain their original framing. The backdrop
and ground continuations reuse existing art; ground eligibility is checked
from the current level's collision data.
Existing upstream object-visibility limitations remain. Choose
fullscreen/32:9 before entering a level. After widening the window, leave
and re-enter the level. This mod retains the
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
- Background regression coverage checks repeated-pattern detection,
  aligned negative-X tile filling, preserved positive-X entries, vertical
  rejection and exclusion of collision layers.
- RT-to-background lookup tests cover fresh level associations, invalid
  pointers and the bounded five-slot background list.
- Transparent-strip tests cover CI8, IA8 and I8, opaque rejection and mask
  suppression before publication.
- Camera tests verify eased starts, stops and reversals, full running
  speed, wall-clamp ordering, reset conditions and the game's millipixel
  rounding. The complete native test suite passed.
- Verified all five hooks in regular official DinoTale. Live counters
  confirmed backdrop filling, mask suppression and camera easing ran.
- The user confirmed the background stays visible after scrolling and
  the camera moves more smoothly with the final 0.2.3 package installed.
- Ground collision regressions cover the finite extension, constant edge
  profile, preserved interior pits/slopes, vertical rejection, inclusive
  stop values, invalid pointers and source/output alias rejection.
- Ground material and runtime checks cover collision-equivalent motif
  columns, scripted/dynamic edge rejection, projection-scaled bounds and
  Original mode.
- Verified all 15 native hooks in regular official DinoTale, with live
  ground filling and collision-query counters active during gameplay.
- Independent pixel comparison matched all 207,360 foreground pixels in
  Cloud Cruising, including the added ground. Yoshi reached negative world
  X at the original ground height.
- The user confirmed the ground reaches the left edge and Yoshi can walk
  and stand on it without falling.

The shared fixes apply across eligible levels; complete level-by-level
playtesting at 32:9 is not claimed.

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
