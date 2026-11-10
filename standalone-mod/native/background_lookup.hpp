#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dinotale32x9 {
namespace background_lookup_detail {
inline bool range(uint32_t address, size_t size, size_t memory_size,
                  uint32_t& offset) {
    if ((address & 3u) != 0 || (address & 0xff800000u) != 0x80000000u) {
        return false;
    }
    offset = address - 0x80000000u;
    const size_t limit = std::min<size_t>(memory_size, 0x800000);
    return offset <= limit && size <= limit - offset;
}

// The host's word-swapped RDRAM stores a whole MIPS word low byte first.
// Explicit bytes keep the lookup portable and avoid unaligned host loads.
inline uint32_t word(std::span<const uint8_t> memory, uint32_t offset) {
    return uint32_t(memory[offset]) | uint32_t(memory[offset + 1]) << 8 |
           uint32_t(memory[offset + 2]) << 16 | uint32_t(memory[offset + 3]) << 24;
}
} // namespace background_lookup_detail

// The draw helper receives an ephemeral render object, without a backpointer
// to the actor's WORK. Resolve its RT through the same five background slots
// used by func_800500AC. Recompute this association so level changes cannot
// leave a cached RT mapped to a freed descriptor. Art/format eligibility is
// checked by the caller after a descriptor has been found.
inline uint32_t find_background_for_render_target(std::span<const uint8_t> memory,
                                                  uint32_t render_target) {
    using namespace background_lookup_detail;
    uint32_t unused, globals, list, slots;
    if (!range(render_target, 0x44, memory.size(), unused) ||
        !range(0x800f8810u, 4, memory.size(), globals)) { return 0; }
    const uint32_t list_address = word(memory, globals);
    if (!range(list_address, 0x14, memory.size(), list)) { return 0; }
    const uint32_t slots_address = word(memory, list + 0x10);
    if (!range(slots_address, 5 * sizeof(uint32_t), memory.size(), slots)) { return 0; }
    for (uint32_t index = 0; index < 5; ++index) {
        uint32_t actor, work, background;
        if (!range(word(memory, slots + index * 4), 0x170, memory.size(), actor)) { continue; }
        const uint32_t work_address = word(memory, actor + 0x16c);
        if (!range(work_address, 0x1c0, memory.size(), work)) { continue; }
        if (word(memory, work + 0x1bc) != render_target) { continue; }
        const uint32_t background_address = word(memory, work + 0x1b0);
        if (!range(background_address, 0x48, memory.size(), background)) { continue; }
        return background_address;
    }
    return 0;
}
} // namespace dinotale32x9
