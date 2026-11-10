#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "dt_bg_arena.hpp"
#include "recomp.h"

extern "C" void dinotale_bg_arena_allocate(uint8_t*, recomp_context*);
extern "C" void dinotale_bg_arena_release_view(uint8_t*, uint32_t);
extern "C" void dinotale_bg_arena_reset();

int main() {
    constexpr uint32_t kseg0 = 0x80000000U;
    std::vector<uint8_t> memory(0x800000, 0xA5);
    recomp_context context{};
    context.r4 = 320;

    dinotale_bg_arena_reset();
    // The constructor gets a sign-extended, aligned pointer, cleared without
    // touching the game heap. Direct-call ra values are deliberately unused.
    context.r31 = 0x12345678;
    dinotale_bg_arena_allocate(memory.data(), &context);
    assert(context.r2 == static_cast<gpr>(static_cast<int64_t>(static_cast<int32_t>(dinotale::BackgroundArena::begin))));
    const uint32_t allocation = uint32_t(context.r2);
    const auto ring = memory.begin() + (allocation - kseg0);
    assert(std::all_of(ring, ring + 320, [](uint8_t v) { return v == 0; }));
    assert(*(ring - 1) == 0xA5);
    assert(*(ring + 320) == 0xA5);
    assert(memory[0] == 0xA5);

    constexpr uint32_t view = 0x802078B0U;
    auto& raw = *reinterpret_cast<uint32_t*>(memory.data() + (view - kseg0) + 0x64U);
    raw = allocation;
    dinotale_bg_arena_release_view(memory.data(), view);
    assert(raw == 0);
    dinotale_bg_arena_allocate(memory.data(), &context);
    assert(uint32_t(context.r2) == allocation);

    // Existing ordinary-heap views and invalid view addresses are unchanged.
    raw = 0x80210000U;
    dinotale_bg_arena_release_view(memory.data(), view);
    assert(raw == 0x80210000U);
    dinotale_bg_arena_release_view(memory.data(), 0);
    dinotale_bg_arena_release_view(memory.data(), view + 1);
    dinotale_bg_arena_release_view(memory.data(), 0x803FFFA0U);
    assert(raw == 0x80210000U);

    // A game reset can reuse the same RDRAM mapping with no leaked arena state.
    dinotale_bg_arena_reset();
    dinotale_bg_arena_allocate(memory.data(), &context);
    assert(uint32_t(context.r2) == dinotale::BackgroundArena::begin);
    return 0;
}
