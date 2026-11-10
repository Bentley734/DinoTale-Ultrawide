#pragma once

#include "background_repeat.hpp"

#include <cstdint>
#include <span>

namespace dinotale32x9 {

struct GroundCollisionLayout {
    uint32_t background_offset = 0;
    uint32_t metadata_offset = 0;
    uint32_t chunk_map_offset = 0;
    uint32_t tilemap_offset = 0;
    uint32_t collision_offset = 0;
    uint16_t chunk_columns = 0;
    uint16_t chunk_rows = 0;
    uint16_t atlas_stride = 0;
    uint32_t width_tiles = 0;
    uint32_t height_tiles = 0;
    bool authored_repeat = false;
    bool edge_has_flat_support = false;
};

// Match the original positive-coordinate lookup. A false result means a
// missing/out-of-range chunk; a stored 0xffff attribute is still a valid read.
inline bool read_ground_collision_tile(std::span<const uint8_t> memory,
                                       const GroundCollisionLayout& layout,
                                       int32_t tile_x, int32_t tile_y,
                                       uint16_t& value) {
    using namespace background_repeat_detail;
    if (tile_x < 0 || tile_y < 0 || tile_x > 32767 || tile_y > 32767 ||
        layout.chunk_columns == 0 || layout.chunk_rows == 0 || layout.atlas_stride < 16) { return false; }
    uint32_t chunk_x = uint8_t(uint32_t(tile_x) >> 4);
    uint32_t chunk_y = uint8_t(uint32_t(tile_y) >> 4);
    if (layout.authored_repeat) {
        chunk_x %= layout.chunk_columns;
        chunk_y %= layout.chunk_rows;
    }
    else if (chunk_x >= layout.chunk_columns || chunk_y >= layout.chunk_rows) { return false; }
    const uint64_t grid_at = uint64_t(layout.chunk_map_offset) +
        uint64_t(chunk_y) * layout.chunk_columns + chunk_x;
    if (grid_at >= memory.size() || (grid_at ^ 3u) >= memory.size()) { return false; }
    const uint8_t chunk = byte(memory, uint32_t(grid_at));
    if (chunk == 0xff || uint32_t(chunk) * 16 >= layout.atlas_stride) { return false; }
    const uint64_t at = uint64_t(layout.collision_offset) + 2 *
        (uint64_t(tile_y & 15) * layout.atlas_stride + uint32_t(chunk) * 16 + uint32_t(tile_x & 15));
    if (at + 2 > memory.size() || ((at ^ 2u) + 2) > memory.size()) { return false; }
    value = half(memory, uint32_t(at));
    return true;
}

inline bool inspect_ground_collision(std::span<const uint8_t> memory,
                                     uint32_t background,
                                     GroundCollisionLayout& result) {
    using namespace background_repeat_detail;
    result = {};
    uint32_t bg, meta, grid, visual, collision;
    if ((background & 3u) != 0 || !range(background, 0x48, memory.size(), bg) ||
        word(memory, bg + 0x44) == 0) { return false; }
    const uint32_t metadata = word(memory, bg + 0x20);
    if ((metadata & 3u) != 0 || !range(metadata, 0x2c, memory.size(), meta)) { return false; }
    const uint16_t columns = half(memory, meta + 0x12);
    const uint16_t rows = half(memory, meta + 0x14);
    const uint16_t stride = half(memory, meta + 0x16);
    if (columns == 0 || columns > 256 || rows == 0 || rows > 256 ||
        stride < 16 || stride > 0x7ff0 || (stride & 15u) != 0) { return false; }
    const uint32_t visual_address = word(memory, bg + 0x1c);
    const uint32_t collision_address = word(memory, meta + 0x24);
    const uint64_t atlas_size = uint64_t(stride) * 16 * 2;
    if ((visual_address & 1u) != 0 || (collision_address & 1u) != 0 ||
        !range(word(memory, meta + 0x18), uint64_t(columns) * rows, memory.size(), grid) ||
        !range(visual_address, atlas_size, memory.size(), visual) ||
        !range(collision_address, atlas_size, memory.size(), collision)) { return false; }
    for (uint32_t i = 0; i < uint32_t(columns) * rows; ++i) {
        const uint8_t id = byte(memory, grid + i);
        if (id != 0xff && uint32_t(id) * 16 >= stride) { return false; }
    }
    result = {bg, meta, grid, visual, collision, columns, rows, stride,
              uint32_t(columns) * 16, uint32_t(rows) * 16, byte(memory, meta + 0x28) == 1, false};
    bool scripted_edge = false;
    for (uint32_t y = 0; y < result.height_tiles; ++y) {
        uint16_t attribute = 0;
        if (!read_ground_collision_tile(memory, result, 0, int32_t(y), attribute)) { continue; }
        // The low byte's upper two bits select dynamic/scripted tile actors.
        // Copying these into the added strip could create hidden objects or
        // modify the real edge through the game's terrain-writing helpers.
        if (attribute != 0 && attribute != 0xffff && (attribute & 0xc0u) != 0) { scripted_edge = true; }
        // Shape byte 1 is the existing full, flat terrain shape. Upper
        // material/direction flags are copied unchanged, never synthesized.
        if ((attribute & 0xffu) == 1) { result.edge_has_flat_support = true; }
    }
    if (scripted_edge) { result.edge_has_flat_support = false; }
    return true;
}

enum class GroundCollisionQuery { scalar, horizontal, vertical, rectangle };
struct GroundCollisionRequest {
    GroundCollisionQuery kind;
    uint32_t output;
    int32_t tile_x;
    int32_t tile_y;
    int32_t columns;
    int32_t rows;
    uint16_t stop_attribute;
};
struct GroundCollisionResult {
    bool handled = false;
    uint32_t return_value = 0;
    uint32_t entries_written = 0;
};

// Native wrappers call this only for the active foreground in Expand. The
// original code still handles every request outside the finite negative-X
// strip. Sampling column 0 preserves its full vertical collision profile;
// positive cells in a straddling request retain their own authored values.
inline GroundCollisionResult extend_ground_collision(std::span<uint8_t> memory,
                                                     const GroundCollisionLayout& layout,
                                                     const GroundCollisionRequest& request,
                                                     int32_t minimum_tile_x) {
    using namespace background_repeat_detail;
    if (!layout.edge_has_flat_support || minimum_tile_x >= 0 ||
        request.tile_x < minimum_tile_x || request.tile_x >= 0 ||
        request.columns <= 0 || request.rows <= 0 || request.columns > 32767 || request.rows > 32767 ||
        int64_t(request.tile_x) + request.columns - 1 > 32767 ||
        int64_t(request.tile_y) + request.rows - 1 > 32767) { return {}; }
    if ((request.kind == GroundCollisionQuery::scalar && (request.columns != 1 || request.rows != 1)) ||
        (request.kind == GroundCollisionQuery::horizontal && request.rows != 1) ||
        (request.kind == GroundCollisionQuery::vertical && request.columns != 1)) { return {}; }
    if (request.kind == GroundCollisionQuery::scalar) {
        uint16_t value = 0;
        const bool found = read_ground_collision_tile(memory, layout, 0, request.tile_y, value);
        return {true, found ? value : uint32_t(0xffff), 0};
    }
    const uint64_t output_size = uint64_t(request.columns) * uint32_t(request.rows) * 2;
    uint32_t output;
    if ((request.output & 1u) != 0 || !range(request.output, output_size, memory.size(), output)) { return {}; }
    const uint64_t atlas_size = uint64_t(layout.atlas_stride) * 16 * 2;
    if (overlap(output, output_size, layout.background_offset, 0x48) ||
        overlap(output, output_size, layout.metadata_offset, 0x2c) ||
        overlap(output, output_size, layout.chunk_map_offset, uint64_t(layout.chunk_columns) * layout.chunk_rows) ||
        overlap(output, output_size, layout.collision_offset, atlas_size) ||
        overlap(output, output_size, layout.tilemap_offset, atlas_size)) { return {}; }
    GroundCollisionResult result{true, 1, 0};
    for (int32_t row = 0; row < request.rows; ++row) {
        for (int32_t col = 0; col < request.columns; ++col) {
            const int32_t world_x = request.tile_x + col;
            uint16_t value = 0;
            if (!read_ground_collision_tile(memory, layout, world_x < 0 ? 0 : world_x,
                                            request.tile_y + row, value)) {
                result.return_value = 0;
                return result;
            }
            store_half(memory, output + result.entries_written * 2, value);
            ++result.entries_written;
            if (request.kind != GroundCollisionQuery::rectangle && value == request.stop_attribute) { return result; }
        }
    }
    return result;
}
} // namespace dinotale32x9
