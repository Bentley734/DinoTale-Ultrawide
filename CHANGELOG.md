# Changelog

## 0.2.1 — missing background tile fix

- Fix rectangular holes in platforms, waves and other background layers at
  32:9. The game's 104-byte tile-request pool holds 52 two-byte tile IDs,
  but its threshold admitted requests of up to 104 IDs. Wider 54/55-entry
  rows overwrote neighboring requests. Requests above 52 now use the
  existing heap allocation and free path.
- Add a regression that reproduces adjacent-slot corruption and verifies
  the corrected allocation boundary, including 54/55-entry 32:9 rows.
- The user confirmed the missing Cloud Cruising tiles are fixed after
  installing this update in regular DinoTale.

## 0.2.0 — standalone mod

- Deliver 32:9 through the official Windows DinoTale 0.1.8 mod loader as
  `DinoTale-32x9.nrm` plus `DinoTale32x9.dll`, without replacing the executable.
- Verify the whole executable identity and four exact patch sites before
  applying in-memory aspect, tile-column and heap-limit changes.
- Reject unsupported executables and block mod toggling during gameplay.
- Confirm loading and all four patch values in an isolated official game;
  verify that its on-disk executable remains unchanged.
- Preserve stock HUD and resizing behavior. The earlier custom build's
  separate centered 16:9 HUD and allocation-aware resize cap are not part
  of this standalone implementation.

## 0.1.8-ultrawide.1

- Expand gameplay up to 32:9, including wider background streaming, camera
  bounds, player edge checks and existing visibility hooks.
- Keep background ring buffers in a dedicated 4 MB expansion-memory arena
  so the wider view does not consume the original game's heap.
- Preserve separate Original, centered 16:9 and full-width HUD placements.
- Limit the rendered aspect to the level's allocated background width when
  the window is widened; reload the level to allocate a larger view.
- Separate settings and saves under `dinotale-ultrawide`.
- Add viewport and allocation regression checks, plus an optional portable
  compiler path for Windows builds.


## Unreleased

- Lossless Scaling frame generation: a Vulkan correctness fix (from
  BattleTanx: Global Assault Recompiled), so it doesn't rely on the graphics
  driver accepting empty shader inputs. The generated frames are the same.

## 0.1.8

- New setting, Graphics > Frame Generation: Lossless Scaling's frame
  generation (LSFG 3.1), built in, with your own Lossless.dll (Load
  Lossless.dll). 2x, 3x or 4x the frame rate, Quality or Performance. Needs
  Vulkan, which choosing it switches to. RT64's own interpolation stays the
  default.
- No more empty `rt64` folder in AppData\Local (Windows) or `.rt64` in
  ~/.config (Linux): the renderer kept nothing there, and in portable mode
  everything now stays in the game's folder.

## 0.1.7

- New setting, Graphics > Texture Filtering: Sharp draws sprites, the HUD and
  backgrounds with sharp pixels instead of the game's smooth filtering
  (3D scenery, like the title screen's backdrop, stays smooth).
- Expand: after the page turn into 4-1's second area (and other areas whose
  view starts left of the layer's edge), the left of the screen is no longer
  black.

## 0.1.6

- Expand: 5-4's stilt Shy Guys keep their stilts at the left edge of the
  wider screen too (things no longer vanish in the left strip).
- The green of chalkboards and message boxes (results, options) reaches the
  bottom of the frame instead of leaving a black band.

## 0.1.5

- Expand: water reflections (4-2), goo (2-3) and the page-turn transition
  cover the whole wide screen.
- The Graphics API setting (Auto, D3D12, Vulkan) is shown in the graphics
  options. If the game crashes in the graphics driver on Windows, try Vulkan.
- Expand: no more flashing strip at the right edge after a teleport (and
  at a level's start): the backgrounds are filled all the way across.
- Message boxes and clouds (images drawn in strips) have no lines at the
  joins, and their edges no longer wiggle as they move.
- Fixed lines through the clouds on the continue / game over screen.
- Less wobble in moving scenery: 1-1's picket fence and fruit are calmer,
  and hills and clouds move smoothly when Yoshi jumps.
- Expand: things no longer vanish at the edge of the 4:3 area: enemies like
  5-4's bob-ombs stay until they leave the wider screen, and the stilt Shy
  Guys' stilts show across it.

## 0.1.4

- Windows: larger thread stacks, for random crashes in NVIDIA's graphics
  driver (`nvwgf2umx.dll`).
- Expand in a window wider than 16:9: black bars at the sides instead of
  smeared, flickering edges.
- Fixed lines through rotated sprites (3-3's tilting platforms, 4-3's balls,
  6-1's cogwheels).

## 0.1.3

- Expand: Yoshi walks smoothly to the edge of the wider screen instead of
  jumping out from the 4:3 edge, and can no longer walk off the left end of
  a level at its start.

## 0.1.2

- Fixed blocks of wrong background tiles in 4-1 (and anywhere the game
  draws background tiles with the RDP), in every aspect ratio.
- Expand: the zoomed-out treehouse in 4-1 fills the whole wider screen.

## 0.1.1

- The game can be picked inside a `.zip` or `.7z` archive.
- Expand: Yoshi can walk all the way to the edge of the screen where the
  camera stops (an invisible wall used to stop him short, e.g. in 3-2).
- Hybrid and Expand are marked experimental in the settings.
- The Resolution setting's description says what each option renders at.
- Screen filters (Settings > Graphics > Screen Filter): CRT (flat, no
  curve), Scanlines, LCD, and the xBR, xBRZ and HQ2x pixel-art upscalers.
- Hybrid Dimming slider (Settings > Graphics): how dark Hybrid's stretched
  sides are, from 0% (not dimmed) up; the default is the 0.1.0 look.
- crash_log.txt names the module and thread a crash happened in.

## 0.1.0

First release.

- Yoshi's Story (USA) runs natively on PC, recompiled with N64Recomp
  and rendered with RT64. Bring your own copy of the game.
- Aspect ratios: Original, Stretch, Hybrid and Expand (true widescreen up
  to 16:9, showing more of each level).
- HUD Placement setting for Expand.
- Saves, keyboard and controller input, audio.
- Windows and Linux builds. The Linux build is new; Steam Deck reports welcome.

Still being tested: later pages and boss fights, especially in Expand.
