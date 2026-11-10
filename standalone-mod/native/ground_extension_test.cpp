#include "ground_extension.hpp"

#include <cassert>
#include <iostream>
#include <vector>

namespace {
namespace detail = dinotale32x9::background_repeat_detail;
constexpr uint32_t bg = 0x80000100;
constexpr uint32_t meta = 0x80000200;
constexpr uint32_t grid = 0x80000300;
constexpr uint32_t atlas = 0x80001000;
constexpr uint32_t collision = 0x80003000;
constexpr uint32_t output = 0x80008000;
constexpr uint32_t physical(uint32_t address) { return address & 0x1fffffff; }

void byte(std::vector<uint8_t>& memory, uint32_t address, uint8_t value) {
    memory[physical(address) ^ 3u] = value;
}
void half(std::vector<uint8_t>& memory, uint32_t address, uint16_t value) {
    detail::store_half(memory, physical(address), value);
}
void word(std::vector<uint8_t>& memory, uint32_t address, uint32_t value) {
    const uint32_t offset = physical(address);
    for (uint32_t i = 0; i < 4; ++i) { memory[offset + i] = uint8_t(value >> (i * 8)); }
}
uint16_t half(const std::vector<uint8_t>& memory, uint32_t address) {
    return detail::half(memory, physical(address));
}
uint32_t tile_address(uint32_t table, uint32_t x, uint32_t y) {
    const uint32_t chunk = (y / 16) * 4 + x / 16;
    return table + 2 * (chunk * 16 + x % 16 + (y % 16) * 128);
}
uint16_t art(uint32_t x, uint32_t y) {
    if (y < 28) { return 0; }
    if (x == 0) { return uint16_t(900 + y); }
    if (x < 20) { return uint16_t(1000 + (x - 1) % 12 + y * 20); }
    return uint16_t(2000 + x + y * 20);
}

std::vector<uint8_t> fixture(uint32_t stable_columns = 20) {
    std::vector<uint8_t> memory(0x10000, 0xa7);
    word(memory, bg + 0x1c, atlas);
    word(memory, bg + 0x20, meta);
    word(memory, bg + 0x44, collision);
    word(memory, meta + 0x18, grid);
    word(memory, meta + 0x24, collision);
    half(memory, meta + 0x12, 4);
    half(memory, meta + 0x14, 2);
    half(memory, meta + 0x16, 128);
    byte(memory, meta + 0x28, 0);
    for (uint32_t index = 0; index < 8; ++index) { byte(memory, grid + index, uint8_t(index)); }
    for (uint32_t y = 0; y < 32; ++y) {
        for (uint32_t x = 0; x < 64; ++x) {
            half(memory, tile_address(atlas, x, y), art(x, y));
            const uint16_t attribute = (y == 28 || y == 29) ?
                uint16_t(x < stable_columns ? 0x6f01 : 0x6f02) : uint16_t(0);
            half(memory, tile_address(collision, x, y), attribute);
        }
    }
    return memory;
}

void test_period_and_bounded_request() {
    auto memory = fixture();
    const auto layout = dinotale32x9::inspect_ground_extension(memory, bg);
    assert(layout.eligible && layout.stable_columns == 20);
    assert(layout.material_period_tiles == 12 && layout.material_first_column == 1);
    assert(layout.width_tiles == 64 && layout.height_tiles == 32);
    const dinotale32x9::BackgroundTileRequest request{bg, output, atlas, 24, -22, 27, 24, 4};
    auto expected = memory;
    for (uint32_t y = 0; y < 4; ++y) {
        for (uint32_t column = 4; column < 22; ++column) {
            const int32_t x = -22 + int32_t(column);
            const uint32_t source = uint32_t(1 + ((x - 1) % 12 + 12) % 12);
            half(expected, output + 2 * (y * 24 + column), art(source, y + 27));
        }
    }
    const auto result = dinotale32x9::fill_negative_ground_tiles(memory, request, -18);
    assert(result.eligible && result.material_period_tiles == 12 && result.entries_written == 72);
    assert(memory == expected); // Farther left, x0+, padding, and all source memory preserved.
}

void test_fallbacks() {
    auto memory = fixture(16);
    const auto chunk = dinotale32x9::inspect_ground_extension(memory, bg);
    assert(chunk.eligible && chunk.stable_columns == 16);
    assert(chunk.material_period_tiles == 16 && chunk.material_first_column == 0);
    auto result = dinotale32x9::fill_negative_ground_tiles(memory,
        {bg, output, atlas, 3, -2, 28, 3, 1}, -18);
    assert(result.entries_written == 2);
    assert(half(memory, output) == art(14, 28) && half(memory, output + 2) == art(15, 28));

    memory = fixture(8);
    const auto column = dinotale32x9::inspect_ground_extension(memory, bg);
    assert(column.eligible && column.stable_columns == 8 && column.material_period_tiles == 1);
    result = dinotale32x9::fill_negative_ground_tiles(memory,
        {bg, output, atlas, 0, -1, 28, 1, 1}, -18);
    assert(result.entries_written == 1 && half(memory, output) == art(0, 28));

    // Changing any collision row makes that material column unsafe,
    // including an offscreen ceiling or a hole below the visible floor.
    memory = fixture();
    half(memory, tile_address(collision, 5, 2), 0x6f01);
    const auto changed = dinotale32x9::inspect_ground_extension(memory, bg);
    assert(changed.eligible && changed.stable_columns == 5 && changed.material_period_tiles == 1);

    // Missing chunks and stored empty attributes are distinct to the
    // collision reader, even though both art paths draw blank tile zero.
    memory = fixture();
    byte(memory, grid, 0xff);
    const auto missing = dinotale32x9::inspect_ground_extension(memory, bg);
    assert(missing.eligible && missing.stable_columns == 16 && missing.material_period_tiles == 16);
}

void test_y_and_animation_guards() {
    auto memory = fixture();
    half(memory, tile_address(atlas, 11, 28), 0x8123);
    // This unique animation may break period detection; use a stable
    // complete chunk fallback to prove the raw animation flag survives.
    for (uint32_t x = 20; x < 64; ++x) {
        half(memory, tile_address(collision, x, 28), 0x6f01);
        half(memory, tile_address(collision, x, 29), 0x6f01);
    }
    half(memory, tile_address(atlas, 15, 28), 0x8456);
    auto result = dinotale32x9::fill_negative_ground_tiles(memory,
        {bg, output, atlas, 0, -1, 28, 1, 1}, -18);
    assert(result.eligible && result.entries_written == 1 && half(memory, output) == 0x8456);

    memory = fixture();
    const auto before = memory;
    result = dinotale32x9::fill_negative_ground_tiles(memory,
        {bg, output, atlas, 2, -2, -2, 2, 2}, -18);
    assert(result.eligible && result.entries_written == 0 && memory == before);
    result = dinotale32x9::fill_negative_ground_tiles(memory,
        {bg, output, atlas, 2, -2, 32, 2, 2}, -18);
    assert(result.eligible && result.entries_written == 0 && memory == before);
    result = dinotale32x9::fill_negative_ground_tiles(memory,
        {bg, output, atlas, 2, 0, 28, 2, 2}, -18);
    assert(result.eligible && result.entries_written == 0 && memory == before);
}

void test_void_edges_and_atomic_rejections() {
    for (uint16_t attribute : {uint16_t(0), uint16_t(0xffff), uint16_t(0x6f00)}) {
        auto memory = fixture();
        for (uint32_t y = 0; y < 32; ++y) {
            half(memory, tile_address(collision, 0, y), attribute);
        }
        assert(!dinotale32x9::inspect_ground_extension(memory, bg).eligible);
    }
    for (int scenario = 0; scenario < 11; ++scenario) {
        auto memory = fixture();
        dinotale32x9::BackgroundTileRequest request{bg, output, atlas, 4, -2, 28, 4, 2};
        switch (scenario) {
        case 0: word(memory, bg + 0x44, 0); break;
        case 1: word(memory, meta + 0x24, 0); break;
        case 2: half(memory, meta + 0x16, 127); break;
        case 3: byte(memory, grid + 7, 8); break;
        case 4: word(memory, meta + 0x18, 0x8000ffff); break;
        case 5: request.output = 0x8000fffc; break;
        case 6: request.output = atlas; break;
        case 7: request.output = collision; break;
        case 8: request.output_stride = 3; break;
        case 9: request.tilemap = 0x8000fff0; break;
        case 10: request.output |= 1; break;
        }
        const auto before = memory;
        const auto result = dinotale32x9::fill_negative_ground_tiles(memory, request, -18);
        assert(!result.eligible && result.entries_written == 0 && memory == before);
    }
}
} // namespace

int main() {
    test_period_and_bounded_request();
    test_fallbacks();
    test_y_and_animation_guards();
    test_void_edges_and_atomic_rejections();
    std::cout << "Ground material extension checks passed\n";
}
