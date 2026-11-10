// Enlarged level backgrounds must not consume the US game's original 4 MB
// heap. Only their ring allocations use this arena; view objects, tile
// requests, actors and other allocations keep the game's own allocator.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dt_bg_arena.hpp"
#include "recomp.h"

namespace {
    dinotale::BackgroundArena background_arena;

    [[noreturn]] void arena_error(const char* message, uint32_t value) {
        std::fprintf(stderr, "DinoTale background arena: %s (0x%08X)\n", message, value);
        std::fflush(stderr);
        std::abort();
    }
}

// Only the ring constructor's jal at 0x8004EF78 is replaced by a nop. This
// hook runs at 0x8004EF80, after the original delay slot has set a0 to the
// buffer size plus the game's alignment slack. Other calls to the game's
// allocation/clear wrapper are untouched. Do not infer the caller from ra:
// N64Recomp does not materialize ra on ordinary direct calls.
extern "C" void dinotale_bg_arena_allocate(uint8_t* rdram, recomp_context* ctx) {
    const uint32_t size = uint32_t(ctx->r4);
    const uint32_t address = background_arena.allocate(size);
    if (address == 0) {
        // The game's constructor does not safely handle a failed allocation.
        // Fail explicitly rather than letting it use a null/overlapping ring.
        arena_error("cannot allocate background ring", size);
    }
    std::memset(rdram + (address - 0x80000000U), 0, size);
    ctx->r2 = static_cast<gpr>(static_cast<int64_t>(static_cast<int32_t>(address)));
}

// func_8004EEB8 frees a view's original allocation at +0x64, then the view.
// Release only an exact owned arena allocation and replace that first
// pointer with NULL. The original destructor then safely frees NULL and the
// ordinary game-heap view, without passing an arena pointer to the game heap.
extern "C" void dinotale_bg_arena_release_view(uint8_t* rdram, uint32_t view) {
    if ((view < 0x80000000U) || (view > 0x80400000U - 0x68U) || ((view & 3U) != 0)) {
        return;
    }
    auto& allocation = *reinterpret_cast<uint32_t*>(rdram + (view - 0x80000000U) + 0x64U);
    if (!dinotale::BackgroundArena::contains(allocation)) {
        return;
    }
    if (!background_arena.release(allocation)) {
        arena_error("invalid background ring release", allocation);
    }
    allocation = 0;
}

// The game reinitializes its own heap on boot/reset. Start a fresh arena at
// that same lifecycle boundary, including a restart that reuses the RDRAM
// mapping, rather than guessing from time elapsed between levels.
extern "C" void dinotale_bg_arena_reset() {
    background_arena.reset();
}
