#pragma once

#include "ground_collision.hpp"

#include <algorithm>
#include <cstdint>
#include <span>

namespace dinotale32x9 {

struct GroundExtensionLayout : GroundCollisionLayout {
    bool eligible = false;
    uint16_t stable_columns = 0;
    uint16_t material_period_tiles = 0;
    uint16_t material_first_column = 0;
};

struct GroundFillResult {
    bool eligible = false;
    uint32_t entries_written = 0;
    uint16_t material_period_tiles = 0;
};

namespace ground_extension_detail {
using namespace background_repeat_detail;

inline uint16_t tile(std::span<const uint8_t> memory,
                     const GroundExtensionLayout& layout, uint32_t table,
                     uint32_t x, uint32_t y) {
    const uint8_t chunk = byte(memory, layout.chunk_map_offset +
        (y / 16) * layout.chunk_columns + x / 16);
    if (chunk == 0xff) { return 0; }
    return half(memory, table + 2 * (uint32_t(chunk) * 16 + x % 16 +
                                   (y % 16) * layout.atlas_stride));
}

inline bool same_column(std::span<const uint8_t> memory,
                        const GroundExtensionLayout& layout, uint32_t table,
                        uint32_t first, uint32_t second) {
    for (uint32_t y = 0; y < layout.height_tiles; ++y) {
        if (tile(memory, layout, table, first, y) !=
            tile(memory, layout, table, second, y)) { return false; }
    }
    return true;
}

inline bool same_collision_column(std::span<const uint8_t> memory,
                                  const GroundExtensionLayout& layout,
                                  uint32_t first, uint32_t second) {
    for (uint32_t y = 0; y < layout.height_tiles; ++y) {
        uint16_t a = 0, b = 0;
        const bool found_a = read_ground_collision_tile(memory, layout, int32_t(first), int32_t(y), a);
        const bool found_b = read_ground_collision_tile(memory, layout, int32_t(second), int32_t(y), b);
        if (found_a != found_b || (found_a && a != b)) { return false; }
    }
    return true;
}
} // namespace ground_extension_detail

// Inspect only the external left edge. Every selected material column
// must have exactly the same complete collision profile as column zero;
// this prevents the visual continuation from introducing a pit, slope,
// wall, or platform absent from the collision continuation.
inline GroundExtensionLayout inspect_ground_extension(
    std::span<const uint8_t> memory, uint32_t background, uint32_t tilemap) {
    using namespace ground_extension_detail;
    GroundExtensionLayout result;
    GroundCollisionLayout collision_layout;
    if (!inspect_ground_collision(memory, background, collision_layout) ||
        !collision_layout.edge_has_flat_support || (tilemap & 1u) != 0) { return result; }
    uint32_t art;
    if (!range(tilemap, uint64_t(collision_layout.atlas_stride) * 16 * 2,
               memory.size(), art)) { return result; }
    static_cast<GroundCollisionLayout&>(result) = collision_layout;
    result.tilemap_offset = art;

    // Bound work independently of the level's width. Four chunks are
    // enough to establish an edge material motif without scanning the
    // authored level or crossing a later terrain feature.
    const uint32_t scan_columns = std::min(result.width_tiles, 64u);
    uint16_t stable = 1;
    while (stable < scan_columns &&
           same_collision_column(memory, result, 0, stable)) { ++stable; }
    result.stable_columns = stable;

    // The first art column can be a unique edge cap. Look for an exact
    // repeated column profile within the remaining stable run. Require
    // four echoes and at least half a period of overlap, so a single
    // accidental repeated tile cannot choose a material period.
    for (uint16_t period = 1; period < stable; ++period) {
        const uint16_t overlap = uint16_t(stable - 1 - period);
        if (overlap < 4 || overlap < (period + 1) / 2) { continue; }
        bool matches = true;
        for (uint16_t x = 1 + period; x < stable; ++x) {
            if (!same_column(memory, result, art, x, 1 + (x - 1) % period)) {
                matches = false;
                break;
            }
        }
        if (matches) {
            result.material_period_tiles = period;
            result.material_first_column = 1;
            break;
        }
    }
    if (result.material_period_tiles == 0) {
        // A complete stable first chunk is a useful material fallback.
        // A shorter run must use column zero to preserve its edge shape.
        result.material_period_tiles = stable >= 16 ? 16 : 1;
        result.material_first_column = 0;
    }
    result.eligible = true;
    return result;
}

inline GroundExtensionLayout inspect_ground_extension(
    std::span<const uint8_t> memory, uint32_t background) {
    uint32_t bg;
    if ((background & 3u) != 0 ||
        !background_repeat_detail::range(background, 0x48, memory.size(), bg)) { return {}; }
    return inspect_ground_extension(memory, background,
        background_repeat_detail::word(memory, bg + 0x1c));
}

// Invoke after the original tile request builder. Extend only the added
// negative-X margin; positive authored tiles, interior holes, negative Y,
// below-map Y, row padding, and requests farther left remain unchanged.
inline GroundFillResult fill_negative_ground_tiles(
    std::span<uint8_t> memory, const BackgroundTileRequest& request,
    int32_t minimum_tile_x) {
    using namespace ground_extension_detail;
    const auto layout = inspect_ground_extension(memory, request.background, request.tilemap);
    if (!layout.eligible || minimum_tile_x >= 0 || request.columns <= 0 ||
        request.rows <= 0 || request.output_stride < 0 ||
        (request.rows > 1 && request.output_stride < request.columns) ||
        (request.output & 1u) != 0) { return {}; }
    const uint64_t output_size = (uint64_t(request.rows - 1) * uint32_t(request.output_stride) +
                                  uint32_t(request.columns)) * 2;
    uint32_t output;
    if (!range(request.output, output_size, memory.size(), output)) { return {}; }
    if (overlap(output, output_size, layout.background_offset, 0x48) ||
        overlap(output, output_size, layout.metadata_offset, 0x2c) ||
        overlap(output, output_size, layout.chunk_map_offset,
                uint64_t(layout.chunk_columns) * layout.chunk_rows) ||
        overlap(output, output_size, layout.tilemap_offset,
                uint64_t(layout.atlas_stride) * 16 * 2) ||
        overlap(output, output_size, layout.collision_offset,
                uint64_t(layout.atlas_stride) * 16 * 2)) { return {}; }
    uint32_t default_art;
    if (!range(word(memory, layout.background_offset + 0x1c),
               uint64_t(layout.atlas_stride) * 16 * 2, memory.size(), default_art) ||
        overlap(output, output_size, default_art,
                uint64_t(layout.atlas_stride) * 16 * 2)) { return {}; }

    GroundFillResult result{true, 0, layout.material_period_tiles};
    const int64_t first = std::max<int64_t>(0, int64_t(minimum_tile_x) - request.start_x);
    const int64_t end = std::min<int64_t>(request.columns, -int64_t(request.start_x));
    if (first >= end) { return result; }
    for (int32_t row = 0; row < request.rows; ++row) {
        const int64_t y = int64_t(request.start_y) + row;
        if (y < 0 || y >= layout.height_tiles) { continue; }
        for (int64_t col = first; col < end; ++col) {
            const int64_t x = int64_t(request.start_x) + col;
            const int64_t period = layout.material_period_tiles;
            const int64_t phase = (x - layout.material_first_column) % period;
            const uint32_t source_x = uint32_t(layout.material_first_column +
                                               (phase + period) % period);
            const uint16_t value = tile(memory, layout, layout.tilemap_offset,
                                        source_x, uint32_t(y));
            const uint64_t index = uint64_t(row) * uint32_t(request.output_stride) + uint64_t(col);
            store_half(memory, output + uint32_t(index * 2), value);
            ++result.entries_written;
        }
    }
    return result;
}

} // namespace dinotale32x9
