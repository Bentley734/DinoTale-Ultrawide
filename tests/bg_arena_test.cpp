#include <cassert>
#include <cstdint>
#include <vector>

#include "dt_bg_arena.hpp"

int main() {
    using dinotale::BackgroundArena;
    BackgroundArena arena;
    assert(arena.allocate(0) == 0);
    assert(arena.allocate(UINT32_MAX) == 0);
    assert(arena.allocate(BackgroundArena::capacity + 1) == 0);
    assert(!arena.release(0));
    assert(!arena.release(BackgroundArena::begin));

    // Six simultaneous zoomed-out 32:9 layers cover both sides of a level
    // transition. Each ring is (1296+16)*256 bytes, plus constructor slack.
    constexpr uint32_t zoomed_ring = (1296U + 16U) * 256U + 64U;
    std::vector<uint32_t> rings;
    for (int i = 0; i < 6; ++i) {
        const uint32_t address = arena.allocate(zoomed_ring);
        assert(address != 0);
        assert((address % BackgroundArena::alignment) == 0);
        assert(address >= BackgroundArena::begin);
        assert(address + zoomed_ring <= BackgroundArena::end);
        if (!rings.empty()) {
            assert(address >= rings.back() + zoomed_ring);
        }
        rings.push_back(address);
    }

    // Interior and stale pointers cannot release a live allocation. A real
    // hole is reused without moving or overlapping its surviving neighbors.
    assert(!arena.release(rings[2] + 64));
    assert(arena.release(rings[2]));
    assert(!arena.release(rings[2]));
    assert(arena.allocate(zoomed_ring) == rings[2]);

    arena.reset();
    const uint32_t first = arena.allocate(1);
    const uint32_t second = arena.allocate(65);
    const uint32_t third = arena.allocate(1);
    assert(first == BackgroundArena::begin);
    assert(second == first + 64);
    assert(third == second + 128);
    assert(arena.release(first));
    assert(arena.release(second));
    assert(arena.allocate(192) == BackgroundArena::begin);
    assert(arena.release(third));

    // Exhaustion and overflowing requests leave existing allocations intact.
    arena.reset();
    assert(arena.allocate(BackgroundArena::capacity) == BackgroundArena::begin);
    assert(arena.allocate(1) == 0);
    assert(arena.allocate(UINT32_MAX) == 0);
    assert(arena.release(BackgroundArena::begin));
    assert(arena.allocate(BackgroundArena::capacity - 1) == BackgroundArena::begin);
    arena.reset();
    assert(arena.allocate(BackgroundArena::capacity) == BackgroundArena::begin);
    return 0;
}
