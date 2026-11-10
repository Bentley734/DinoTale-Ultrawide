#include "background_lookup.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <vector>

namespace {
constexpr uint32_t list = 0x80012340;
constexpr uint32_t slots = 0x80023450;
constexpr uint32_t actor = 0x80034560;
constexpr uint32_t work = 0x80045670;
constexpr uint32_t rt = 0x80056780;
constexpr uint32_t background = 0x80067890;

void put(std::vector<uint8_t>& memory, uint32_t address, uint32_t value) {
    const uint32_t offset = address - 0x80000000u;
    for (uint32_t i = 0; i < 4; ++i) { memory[offset + i] = uint8_t(value >> (8 * i)); }
}

std::vector<uint8_t> fixture(uint32_t slot = 0) {
    std::vector<uint8_t> memory(0x800000);
    put(memory, 0x800f8810, list); // world 0x800f7ea0 + 0x970
    put(memory, list + 0x10, slots);
    put(memory, slots + slot * 4, actor);
    put(memory, actor + 0x16c, work);
    put(memory, work + 0x1bc, rt);
    put(memory, work + 0x1b0, background);
    return memory;
}

void test_complete_chain() {
    auto memory = fixture();
    const auto before = memory;
    assert(dinotale32x9::find_background_for_render_target(memory, rt) == background);
    assert(dinotale32x9::find_background_for_render_target(memory, rt + 0x100) == 0);
    assert(memory == before);
    // An association is resolved afresh after a level replaces its WORK.
    put(memory, work + 0x1b0, background + 0x100);
    assert(dinotale32x9::find_background_for_render_target(memory, rt) == background + 0x100);
    put(memory, work + 0x1bc, rt + 0x100);
    assert(dinotale32x9::find_background_for_render_target(memory, rt) == 0);
}

void test_bounded_slots() {
    auto memory = fixture(4);
    // Null, non-pointer sentinel, misaligned actor and truncated WORK slots
    // must not prevent resolving the last valid one.
    put(memory, slots + 4, 1);
    put(memory, slots + 8, actor + 1);
    put(memory, slots + 12, actor + 0x400);
    put(memory, actor + 0x400 + 0x16c, 0x807ffffc);
    assert(dinotale32x9::find_background_for_render_target(memory, rt) == background);
    memory = fixture(5);
    // The sixth slot is deliberately valid but is outside the game list.
    assert(dinotale32x9::find_background_for_render_target(memory, rt) == 0);
    memory = fixture();
    put(memory, work + 0x1b0, 0x807ffffc);
    put(memory, slots + 4, actor + 0x400);
    put(memory, actor + 0x400 + 0x16c, work + 0x400);
    put(memory, work + 0x400 + 0x1bc, rt);
    put(memory, work + 0x400 + 0x1b0, background);
    assert(dinotale32x9::find_background_for_render_target(memory, rt) == background);
}

void test_invalid_links() {
    const std::array<uint32_t, 5> links{
        0x800f8810, list + 0x10, slots, actor + 0x16c, work + 0x1b0
    };
    for (uint32_t link : links) {
        for (uint32_t invalid : {0u, 1u, 0x80000001u, 0x807ffffcu,
                                 0x80800000u, 0xa0012340u, 0xffffffffu}) {
            auto memory = fixture();
            put(memory, link, invalid);
            const auto before = memory;
            assert(dinotale32x9::find_background_for_render_target(memory, rt) == 0);
            assert(memory == before);
        }
    }
    auto memory = fixture();
    for (uint32_t invalid : {0u, 1u, rt + 1, 0x807ffffcu, 0x80800000u, 0xffffffffu}) {
        assert(dinotale32x9::find_background_for_render_target(memory, invalid) == 0);
    }
    assert(dinotale32x9::find_background_for_render_target({}, rt) == 0);
    assert(dinotale32x9::find_background_for_render_target(
        {memory.data(), 0xf8813}, rt) == 0);
    // Every read must respect the supplied span, even with a valid KSEG0 link.
    put(memory, work + 0x1b0, 0x807fffe0);
    assert(dinotale32x9::find_background_for_render_target(memory, rt) == 0);
}
} // namespace

int main() {
    test_complete_chain();
    test_bounded_slots();
    test_invalid_links();
    std::cout << "Background RT association checks passed.\n";
}
