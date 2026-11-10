#include "background_repeat.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using dinotale32x9::BackgroundRepeatLayout;
using dinotale32x9::BackgroundTileRequest;
using dinotale32x9::detect_background_repeat;
using dinotale32x9::fill_negative_background_tiles;
namespace detail = dinotale32x9::background_repeat_detail;

namespace {
constexpr uint32_t bg = 0x80000100;
constexpr uint32_t meta = 0x80000200;
constexpr uint32_t grid = 0x80000300;
constexpr uint32_t atlas = 0x80001000;
constexpr uint32_t output = 0x80008000;
constexpr uint32_t physical(uint32_t address) { return address & 0x1fffffff; }
void byte(std::vector<uint8_t>& memory, uint32_t address, uint8_t value) {
    memory[physical(address) ^ 3u] = value;
}
void half(std::vector<uint8_t>& memory, uint32_t address, uint16_t value) {
    detail::store_half(memory, physical(address), value);
}
void word(std::vector<uint8_t>& memory, uint32_t address, uint32_t value) {
    const uint32_t o = physical(address);
    for (uint32_t i = 0; i < 4; ++i) { memory[o + i] = uint8_t(value >> (8 * i)); }
}
uint16_t half(const std::vector<uint8_t>& memory, uint32_t address) {
    return detail::half(memory, physical(address));
}

std::vector<uint8_t> fixture(uint16_t columns = 7, uint16_t rows = 2) {
    std::vector<uint8_t> memory(0x10000, 0xa7);
    word(memory, bg + 0x20, meta);
    word(memory, bg + 0x1c, atlas);
    word(memory, bg + 0x44, 0);
    half(memory, meta + 0x12, columns);
    half(memory, meta + 0x14, rows);
    half(memory, meta + 0x16, 96);
    word(memory, meta + 0x18, grid);
    byte(memory, meta + 0x28, 0);
    for (uint16_t y = 0; y < rows; ++y) {
        for (uint16_t x = 0; x < columns; ++x) { byte(memory, grid + y * columns + x, uint8_t((y % 2) * 3 + x % 3)); }
    }
    for (uint32_t y = 0; y < 16; ++y) {
        for (uint32_t chunk = 0; chunk < 6; ++chunk) {
            for (uint32_t x = 0; x < 16; ++x) {
                half(memory, atlas + (y * 96 + chunk * 16 + x) * 2,
                     uint16_t(chunk * 256 + y * 16 + x));
            }
        }
    }
    return memory;
}

void test_period_and_fill() {
    auto memory = fixture();
    BackgroundRepeatLayout layout;
    assert(detect_background_repeat(memory, bg, atlas, layout));
    assert(layout.period_chunks == 3 && !layout.authored_repeat);
    const auto inspection = dinotale32x9::inspect_background_repeat(memory, bg);
    assert(inspection.eligible && inspection.period_chunks == 3);
    // The map is seven chunks wide and ends partway through its period.
    // Negative X must use chunk2, not the full-map final chunk0.
    const BackgroundTileRequest request{bg, output, atlas, 7, -2, 15, 5, 2};
    auto expected = memory;
    half(expected, output, 2 * 256 + 15 * 16 + 14);
    half(expected, output + 2, 2 * 256 + 15 * 16 + 15);
    half(expected, output + 14, 5 * 256 + 14);
    half(expected, output + 16, 5 * 256 + 15);
    const auto result = fill_negative_background_tiles(memory, request);
    assert(result.eligible && result.period_chunks == 3 && result.entries_written == 4);
    assert(memory == expected); // Positive columns, row padding, and guards intact.

    // Preserve animated tile IDs verbatim for the game's normal resolver.
    memory = fixture();
    half(memory, atlas + (2 * 16 + 15) * 2, 0x8123);
    const auto animated = fill_negative_background_tiles(memory, {bg, output, atlas, 2, -1, 0, 2, 1});
    assert(animated.entries_written == 1 && half(memory, output) == 0x8123);
    const auto zero_stride = fill_negative_background_tiles(memory, {bg, output, atlas, 0, -1, 0, 1, 1});
    assert(zero_stride.eligible && zero_stride.entries_written == 1);
}

void test_multiple_periods_and_y_guards() {
    auto memory = fixture();
    auto expected = memory;
    const BackgroundTileRequest request{bg, output, atlas, 55, -50, -1, 55, 3};
    for (uint32_t row = 1; row < 3; ++row) {
        for (int32_t col = 0; col < 50; ++col) {
            const int32_t x = ((-50 + col) % 48 + 48) % 48;
            const uint16_t tile = uint16_t((x / 16) * 256 + (row - 1) * 16 + x % 16);
            half(expected, output + (row * 55 + uint32_t(col)) * 2, tile);
        }
    }
    const auto result = fill_negative_background_tiles(memory, request);
    assert(result.entries_written == 100 && memory == expected);

    memory = fixture();
    const auto before = memory;
    const auto below = fill_negative_background_tiles(memory, {bg, output, atlas, 2, -2, 32, 2, 1});
    assert(below.eligible && below.entries_written == 0 && memory == before);
    const auto original = fill_negative_background_tiles(memory, {bg, output, atlas, 2, 0, 0, 2, 1});
    assert(original.eligible && original.entries_written == 0 && memory == before);

    // Blank chunks remain transparent tile0 rather than sampling atlas ID255.
    for (uint32_t x = 0; x < 7; ++x) { byte(memory, grid + 7 + x, 0xff); }
    const auto blank = fill_negative_background_tiles(memory, {bg, output, atlas, 1, -1, 16, 1, 1});
    assert(blank.eligible && blank.entries_written == 1 && half(memory, output) == 0);
}

void test_authored_repeat() {
    auto memory = fixture(5, 1);
    for (uint32_t x = 0; x < 5; ++x) { byte(memory, grid + x, uint8_t(x)); }
    BackgroundRepeatLayout layout;
    assert(!detect_background_repeat(memory, bg, atlas, layout));
    byte(memory, meta + 0x28, 1);
    assert(detect_background_repeat(memory, bg, atlas, layout));
    assert(layout.authored_repeat && layout.period_chunks == 5);
    const auto result = fill_negative_background_tiles(memory, {bg, output, atlas, 1, -1, 16, 1, 1});
    assert(result.entries_written == 1 && half(memory, output) == 4 * 256 + 15);

    // Authored repeat still preserves the original negative-Y guard.
    const auto before = memory;
    const auto negative_y = fill_negative_background_tiles(memory, {bg, output, atlas, 1, -1, -1, 1, 1});
    assert(negative_y.entries_written == 0 && memory == before);
}

void test_rejections_are_atomic() {
    const BackgroundTileRequest request{bg, output, atlas, 2, -2, 0, 2, 1};
    for (int scenario = 0; scenario < 10; ++scenario) {
        auto memory = fixture();
        auto invalid = request;
        switch (scenario) {
        case 0: word(memory, bg + 0x44, 0x80005000); break; // Foreground collision.
        case 1: byte(memory, grid + 6, 5); break; // Nonperiodic decorative layer.
        case 2: for (uint32_t i = 0; i < 14; ++i) { byte(memory, grid + i, 0xff); } break;
        case 3: byte(memory, grid, 6); break; // Chunk outside atlas.
        case 4: word(memory, meta + 0x18, 0x8000ffff); break;
        case 5: invalid.output = 0x8000fffe; break;
        case 6: invalid.output = atlas; break; // Source/output alias.
        case 7: invalid.output_stride = 1; invalid.rows = 2; break;
        case 8: half(memory, meta + 0x12, 0); break;
        case 9: invalid.tilemap = 0x8000fff0; break;
        }
        const auto before = memory;
        const auto result = fill_negative_background_tiles(memory, invalid);
        assert(!result.eligible && result.entries_written == 0 && memory == before);
    }

    // All rows must share the same period, not only the first visible row.
    auto memory = fixture(6, 2);
    for (uint32_t x = 0; x < 6; ++x) { byte(memory, grid + 6 + x, uint8_t(3 + x % 2)); }
    BackgroundRepeatLayout layout;
    assert(!detect_background_repeat(memory, bg, atlas, layout));
}
}

int main() {
    test_period_and_fill();
    test_multiple_periods_and_y_guards();
    test_authored_repeat();
    test_rejections_are_atomic();
    std::cout << "Background period detection and negative-X tile request checks passed.\n";
}
