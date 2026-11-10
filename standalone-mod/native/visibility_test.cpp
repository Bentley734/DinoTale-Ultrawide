#include "visibility_math.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <vector>

using dinotale32x9::map_visibility_x;
using dinotale32x9::visibility_aabb;

namespace {
void check_bounds(int32_t width, int32_t half_width, int32_t half_height,
                  int16_t margin_x = 0, int16_t margin_y = 0) {
    const int32_t extra = (width - 320) / 2;
    const int32_t left = 160 - half_width - margin_x - extra;
    const int32_t right = 160 + half_width + margin_x + extra;
    const auto test = [&](int32_t x, int32_t y) {
        return visibility_aabb(map_visibility_x(x, width), y, margin_x,
                               margin_y, half_width, half_height);
    };
    assert(test(left, 120));
    assert(test(right, 120));
    assert(!test(left - 1, 120));
    assert(!test(right + 1, 120));
    assert(test(-extra, 120));
    assert(test(320 + extra, 120));
    assert(test(160, 120 - half_height - margin_y));
    assert(test(160, 120 + half_height + margin_y));
    assert(!test(160, 119 - half_height - margin_y));
    assert(!test(160, 121 + half_height + margin_y));

    // Equivalent to widening the original horizontal half-width by extra.
    // This checks the entire useful viewport plus its preload/retain band.
    for (int32_t x = left - 64; x <= right + 64; ++x) {
        assert(test(x, 120) == visibility_aabb(x, 120, margin_x, margin_y,
                                              half_width + extra, half_height));
    }
}

void test_viewports() {
    for (const int32_t width : {320, 432, 560, 864}) {
        const int32_t extra = (width - 320) / 2;
        check_bounds(width, 165, 125); // Original five-pixel screen band.
        check_bounds(width, 240, 200); // Original 80-pixel retain band.
        check_bounds(width, 340, 300); // Original 180-pixel zoom retain band.
        check_bounds(width, 165, 125, 16, 12);
        check_bounds(width, 240, 200, 16, 12);
        check_bounds(width, 340, 300, 16, 12);
        // Placement loader supplies 64 on both axes when metadata is zero.
        check_bounds(width, 165, 125, 64, 64);
        check_bounds(width, 240, 200, 64, 64);
        for (int32_t x = 0; x < 320; ++x) {
            assert(map_visibility_x(x, width) == x);
        }
        if (extra != 0) {
            assert(map_visibility_x(-extra, width) == 0);
            assert(map_visibility_x(-extra + 1, width) == 1);
            assert(map_visibility_x(-1, width) == 1);
            assert(map_visibility_x(320, width) == 319);
            assert(map_visibility_x(319 + extra, width) == 319);
            assert(map_visibility_x(320 + extra, width) == 320);
        }
        assert(visibility_aabb(-100000, -100000, 0x7fff, 0, 165, 125));
    }
    for (int32_t x = -2000; x <= 2000; ++x) {
        assert(map_visibility_x(x, 320) == x);
    }
}

struct MockContext {
    std::array<uint64_t, 32> registers{};
    bool result = false;
};

void test_dispatch() {
    std::vector<uint8_t> memory(0x800000, 0xa7);
    const uint32_t offset = 0x12340;
    const int32_t x = -260;
    const int32_t y = 90;
    std::memcpy(memory.data() + offset, &x, 4);
    std::memcpy(memory.data() + offset + 4, &y, 4);
    const auto before = memory;
    MockContext context;
    context.registers[4] = 0x80000000u + offset;
    context.registers[5] = 0x80005678u;
    const auto input_registers = context.registers;
    int map_calls = 0;
    int original_calls = 0;
    dinotale32x9::dispatch_mapped_visibility(memory, uint32_t(context.registers[4]),
        [&](int32_t value) {
            ++map_calls;
            assert(value == x);
            return map_visibility_x(value, 864);
        },
        [&]() {
            ++original_calls;
            assert(context.registers == input_registers);
            int32_t current_x, current_y;
            std::memcpy(&current_x, memory.data() + offset, 4);
            std::memcpy(&current_y, memory.data() + offset + 4, 4);
            assert(current_x == 1);
            assert(current_y == y);
            context.result = visibility_aabb(current_x, current_y, 0, 0, 165, 125);
            // Native MIPS functions are allowed to alter argument registers.
            context.registers[4] = 0x80006543u;
            context.registers[2] = context.result ? 1 : 0;
        });
    assert(map_calls == 1 && original_calls == 1);
    assert(context.result && context.registers[2] == 1);
    assert(context.registers[4] == 0x80006543u);
    assert(memory == before);

    // Disabled/Original mode leaves the original screen coordinates intact.
    int original_mode_calls = 0;
    const bool result = dinotale32x9::dispatch_mapped_visibility(memory, 0x80000000u + offset,
        [](int32_t value) { return map_visibility_x(value, 320); },
        [&]() {
            ++original_mode_calls;
            int32_t current_x;
            std::memcpy(&current_x, memory.data() + offset, 4);
            assert(current_x == x);
            return visibility_aabb(current_x, y, 0, 0, 165, 125);
        });
    assert(!result && original_mode_calls == 1 && memory == before);

    // Invalid pointers use the original once without reading or modifying RAM.
    for (const uint32_t pointer : {0u, 0x80000001u, 0x807ffffcu, 0x80800000u}) {
        int called = 0;
        dinotale32x9::dispatch_mapped_visibility(memory, pointer,
            [](int32_t) -> int32_t { assert(false); return 0; },
            [&]() { ++called; });
        assert(called == 1 && memory == before);
    }
}
}

int main() {
    test_viewports();
    test_dispatch();
    std::cout << "Visibility bounds and pointer-aware dispatch checks passed.\n";
}
