#include "ground_collision.hpp"

#include <cassert>
#include <iostream>
#include <vector>

namespace {
using namespace dinotale32x9;
using namespace dinotale32x9::background_repeat_detail;
constexpr uint32_t bg_address = 0x80010000;
constexpr uint32_t meta_address = 0x80011000;
constexpr uint32_t grid_address = 0x80012000;
constexpr uint32_t visual_address = 0x80020000;
constexpr uint32_t collision_address = 0x80030000;
constexpr uint32_t output_address = 0x80040000;
constexpr uint16_t solid = 0x6f01;
constexpr uint16_t slope = 0x6f02;
constexpr uint16_t canary = 0xa55a;

void put_word(std::vector<uint8_t>& memory, uint32_t address, uint32_t value) {
    const uint32_t offset = address - 0x80000000;
    for (uint32_t i = 0; i < 4; ++i) { memory[offset + i] = uint8_t(value >> (8 * i)); }
}
void put_byte(std::vector<uint8_t>& memory, uint32_t address, uint8_t value) {
    memory[(address - 0x80000000) ^ 3u] = value;
}
void put_half(std::vector<uint8_t>& memory, uint32_t address, uint16_t value) {
    store_half(memory, address - 0x80000000, value);
}
uint16_t output_at(const std::vector<uint8_t>& memory, uint32_t index) {
    return half(memory, output_address - 0x80000000 + index * 2);
}
std::vector<uint8_t> fixture() {
    std::vector<uint8_t> memory(0x800000);
    put_word(memory, bg_address + 0x1c, visual_address);
    put_word(memory, bg_address + 0x20, meta_address);
    put_word(memory, bg_address + 0x44, collision_address);
    put_half(memory, meta_address + 0x12, 2);
    put_half(memory, meta_address + 0x14, 2);
    put_half(memory, meta_address + 0x16, 32);
    put_word(memory, meta_address + 0x18, grid_address);
    put_word(memory, meta_address + 0x24, collision_address);
    for (uint32_t i = 0; i < 4; ++i) { put_byte(memory, grid_address + i, uint8_t(i & 1)); }
    for (uint32_t y = 0; y < 16; ++y) {
        for (uint32_t x = 0; x < 32; ++x) {
            const uint16_t attribute = y < 4 ? 0 : y == 4 && x == 1 ? 0 : y == 4 && x == 3 ? slope : solid;
            put_half(memory, collision_address + (y * 32 + x) * 2, attribute);
            put_half(memory, visual_address + (y * 32 + x) * 2, uint16_t(0x100 + y * 32 + x));
        }
    }
    for (uint32_t i = 0; i < 64; ++i) { put_half(memory, output_address + i * 2, canary); }
    return memory;
}
GroundCollisionLayout inspect(const std::vector<uint8_t>& memory) {
    GroundCollisionLayout layout;
    assert(inspect_ground_collision(memory, bg_address, layout));
    assert(layout.edge_has_flat_support);
    assert(layout.width_tiles == 32 && layout.height_tiles == 32);
    return layout;
}

void test_lookup_semantics() {
    auto memory = fixture();
    auto layout = inspect(memory);
    uint16_t value = 0x7777;
    assert(read_ground_collision_tile(memory, layout, 0, 4, value) && value == solid);
    assert(read_ground_collision_tile(memory, layout, 1, 4, value) && value == 0);
    assert(read_ground_collision_tile(memory, layout, 3, 4, value) && value == slope);
    assert(!read_ground_collision_tile(memory, layout, -1, 4, value));
    assert(!read_ground_collision_tile(memory, layout, 0, -1, value));
    assert(!read_ground_collision_tile(memory, layout, 32, 4, value));
    // Native lookup casts chunk coordinates to bytes before range/modulo.
    assert(read_ground_collision_tile(memory, layout, 4096, 4, value) && value == solid);
    put_byte(memory, meta_address + 0x28, 1);
    layout = inspect(memory);
    assert(read_ground_collision_tile(memory, layout, 32, 4, value) && value == solid);
    put_half(memory, collision_address + 4 * 32 * 2, 0xffff);
    layout = inspect(memory);
    assert(read_ground_collision_tile(memory, layout, 0, 4, value) && value == 0xffff);
    put_byte(memory, grid_address, 0xff);
    assert(inspect_ground_collision(memory, bg_address, layout));
    assert(!read_ground_collision_tile(memory, layout, 0, 4, value));
}

void test_scalar_and_horizontal() {
    auto memory = fixture();
    const auto layout = inspect(memory);
    auto result = extend_ground_collision(memory, layout,
        {GroundCollisionQuery::scalar, 0, -17, 4, 1, 1, 0}, -17);
    assert(result.handled && result.return_value == solid && result.entries_written == 0);
    result = extend_ground_collision(memory, layout,
        {GroundCollisionQuery::scalar, 0, -17, -1, 1, 1, 0}, -17);
    assert(result.handled && result.return_value == 0xffff);
    result = extend_ground_collision(memory, layout,
        {GroundCollisionQuery::horizontal, output_address, -3, 4, 8, 1, 0x7777}, -17);
    assert(result.handled && result.return_value == 1 && result.entries_written == 8);
    const uint16_t expected[]{solid, solid, solid, solid, 0, solid, slope, solid};
    for (uint32_t i = 0; i < 8; ++i) { assert(output_at(memory, i) == expected[i]); }
    assert(output_at(memory, 8) == canary);
    // A real interior pit and slope remain authored; only negative cells
    // sample the edge. Matching stop values are copied before returning.
    memory = fixture();
    result = extend_ground_collision(memory, layout,
        {GroundCollisionQuery::horizontal, output_address, -3, 4, 8, 1, 0}, -17);
    assert(result.handled && result.return_value == 1 && result.entries_written == 5);
    assert(output_at(memory, 4) == 0 && output_at(memory, 5) == canary);
}

void test_vertical_and_rectangle() {
    auto memory = fixture();
    const auto layout = inspect(memory);
    auto result = extend_ground_collision(memory, layout,
        {GroundCollisionQuery::vertical, output_address, -3, 2, 1, 6, solid}, -17);
    assert(result.handled && result.return_value == 1 && result.entries_written == 3);
    assert(output_at(memory, 0) == 0 && output_at(memory, 1) == 0);
    assert(output_at(memory, 2) == solid && output_at(memory, 3) == canary);
    memory = fixture();
    result = extend_ground_collision(memory, layout,
        {GroundCollisionQuery::rectangle, output_address, -2, 3, 6, 3, solid}, -17);
    assert(result.handled && result.return_value == 1 && result.entries_written == 18);
    for (uint32_t i = 0; i < 6; ++i) { assert(output_at(memory, i) == 0); }
    const uint16_t middle[]{solid, solid, solid, 0, solid, slope};
    for (uint32_t i = 0; i < 6; ++i) { assert(output_at(memory, 6 + i) == middle[i]); }
    for (uint32_t i = 12; i < 18; ++i) { assert(output_at(memory, i) == solid); }
    assert(output_at(memory, 18) == canary);
    // Original readers return0 after their valid prefix if a chunk is absent.
    put_byte(memory, grid_address + 1, 0xff);
    result = extend_ground_collision(memory, layout,
        {GroundCollisionQuery::horizontal, output_address, -2, 4, 20, 1, 0x7777}, -17);
    assert(result.handled && result.return_value == 0 && result.entries_written == 18);
}

void test_rejections_leave_memory_intact() {
    auto memory = fixture();
    auto layout = inspect(memory);
    const auto before = memory;
    const GroundCollisionRequest base{GroundCollisionQuery::rectangle, output_address, -2, 3, 6, 3, 0};
    auto request = base;
    for (int32_t x : {-18, 0, 1}) {
        request.tile_x = x;
        assert(!extend_ground_collision(memory, layout, request, -17).handled);
        assert(memory == before);
    }
    for (uint32_t output : {0u, output_address + 1, 0x807ffffeu, bg_address,
                            meta_address, grid_address, visual_address, collision_address}) {
        request = base;
        request.output = output;
        assert(!extend_ground_collision(memory, layout, request, -17).handled);
        assert(memory == before);
    }
    request = base;
    request.columns = 0;
    assert(!extend_ground_collision(memory, layout, request, -17).handled);
    request = base;
    request.rows = 40000;
    assert(!extend_ground_collision(memory, layout, request, -17).handled);
    layout.edge_has_flat_support = false;
    assert(!extend_ground_collision(memory, layout, base, -17).handled);
    assert(memory == before);
    put_word(memory, meta_address + 0x24, 0x807ffffe);
    assert(!inspect_ground_collision(memory, bg_address, layout));
    memory = fixture();
    put_byte(memory, grid_address, 2); // Atlas contains only chunks0 and1.
    assert(!inspect_ground_collision(memory, bg_address, layout));
    memory = fixture();
    put_word(memory, bg_address + 0x44, 0);
    assert(!inspect_ground_collision(memory, bg_address, layout));
}

void test_scripted_edges_are_not_extended() {
    GroundCollisionLayout layout;
    for (uint16_t dynamic : {uint16_t(0x0041), uint16_t(0x0082), uint16_t(0x6fc3)}) {
        auto memory = fixture();
        // Put the event below a known flat row: the entire edge profile
        // must be inspected rather than stopping at the first support tile.
        put_half(memory, collision_address + 12 * 32 * 2, dynamic);
        assert(inspect_ground_collision(memory, bg_address, layout));
        assert(!layout.edge_has_flat_support);
        const auto before = memory;
        const auto result = extend_ground_collision(memory, layout,
            {GroundCollisionQuery::horizontal, output_address, -3, 4, 8, 1, 0}, -17);
        assert(!result.handled && memory == before);
    }
    for (uint16_t empty : {uint16_t(0), uint16_t(0xffff)}) {
        auto memory = fixture();
        put_half(memory, collision_address + 12 * 32 * 2, empty);
        assert(inspect_ground_collision(memory, bg_address, layout));
        assert(layout.edge_has_flat_support);
    }
}
} // namespace

int main() {
    test_lookup_semantics();
    test_scalar_and_horizontal();
    test_vertical_and_rectangle();
    test_rejections_leave_memory_intact();
    test_scripted_edges_are_not_extended();
    std::cout << "Bounded ground collision extrusion checks passed.\n";
}
