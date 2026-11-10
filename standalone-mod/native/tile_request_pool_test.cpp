#include "patch_plan.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
// func_8004FCB0 returns base + pool_index * (13 << 3), with no header.
// Both callers pass a tile count and write uint16_t entries. Request mode
// metadata is stored in the caller's request structure, outside this pool.
constexpr size_t slot_bytes = 13U << 3;
constexpr size_t entries_per_slot = slot_bytes / sizeof(uint16_t);
using Pool = std::array<uint8_t, slot_bytes * 2>;

bool write_request(Pool& pool, size_t tile_count, size_t threshold) {
    std::vector<uint16_t> entries(tile_count);
    for (size_t i=0; i<entries.size(); ++i) { entries[i] = uint16_t(0x1200U + i); }
    if (tile_count < threshold) {
        assert(entries.size() * sizeof(uint16_t) <= pool.size());
        std::memcpy(pool.data(), entries.data(), entries.size() * sizeof(uint16_t));
        return false;
    }
    // This is the game's other path: allocate tile_count * 2 bytes on the
    // heap, so the neighboring pooled request remains intact.
    auto* allocation = new uint16_t[tile_count];
    std::copy(entries.begin(), entries.end(), allocation);
    assert(std::equal(entries.begin(), entries.end(), allocation));
    delete[] allocation;
    return true;
}

bool neighbor_unchanged(const Pool& pool) {
    return std::all_of(pool.begin() + slot_bytes, pool.end(), [](uint8_t value) { return value == 0xCC; });
}
}

int main() {
    const auto found = std::find_if(dinotale32x9::patches.begin(), dinotale32x9::patches.end(),
        [](const auto& patch) { return patch.rva == 0x1613F0; });
    assert(found != dinotale32x9::patches.end());
    constexpr std::array<uint8_t,4> compare_opcode{0x48,0x83,0x7A,0x28};
    assert(found->size == 5);
    assert(std::equal(compare_opcode.begin(), compare_opcode.end(), found->original.begin()));
    assert(std::equal(compare_opcode.begin(), compare_opcode.end(), found->replacement.begin()));
    const size_t old_threshold = found->original[4];
    const size_t new_threshold = found->replacement[4];
    assert(old_threshold == 105);
    assert(new_threshold == entries_per_slot + 1);

    // Reproduce the actual failure: a 32:9 row changes tile IDs belonging
    // to the NEXT outstanding request, even though all writes stay within
    // this simulation's two-slot storage.
    for (size_t count : {53U,54U,55U}) {
        Pool legacy;
        legacy.fill(0xCC);
        assert(!write_request(legacy, count, old_threshold));
        assert(!neighbor_unchanged(legacy));
        const size_t overwritten = (count - entries_per_slot) * sizeof(uint16_t);
        assert(std::all_of(legacy.begin() + slot_bytes + overwritten, legacy.end(),
            [](uint8_t value) { return value == 0xCC; }));

        Pool corrected;
        corrected.fill(0xCC);
        const auto before = corrected;
        assert(write_request(corrected, count, new_threshold));
        assert(corrected == before);
    }

    // Original and 16:9 requests still fit their own slot; larger normal
    // and zoomed requests all use the existing heap allocation path.
    for (size_t count : {15U,20U,21U,28U,41U,52U}) {
        Pool pool;
        pool.fill(0xCC);
        assert(!write_request(pool, count, new_threshold));
        assert(neighbor_unchanged(pool));
    }
    for (size_t count : {53U,54U,55U,82U,83U,104U,105U,880U}) {
        Pool pool;
        pool.fill(0xCC);
        const auto before = pool;
        assert(write_request(pool, count, new_threshold));
        assert(pool == before);
    }
    return 0;
}
