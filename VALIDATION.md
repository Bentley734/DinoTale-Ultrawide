# 0.1.8-ultrawide.1 validation

- Windows Release build completed with Clang 23.1.5 and the Visual Studio
  2026 x64 build tools.
- `tools/test-ultrawide.ps1` passed all three native test programs:
  viewport math, background arena, and native allocation/free integration.
- Viewport checks include 4:3, 16:9, 21:9 and 32:9, invalid aspect fallback,
  zoomed layer widths, resize limits, and all three HUD placements.
- Allocation checks include six simultaneous zoomed 32:9 layers, alignment,
  non-overlap, reuse, exhaustion, invalid frees, zeroing and reset.
- Regenerated constructor code was inspected: the original allocation call
  is removed, the size argument is preserved, and the native allocation
  helper runs before the returned pointer is stored. Destructor and heap
  reset hooks are present.
- The local build launched and the user confirmed that it works.

The automated checks do not establish full level coverage at 32:9. Desktop
game capture timed out during visual inspection, so no screenshot-based
gameplay verification is claimed. Upstream Expand limitations remain as
documented in README.md.
