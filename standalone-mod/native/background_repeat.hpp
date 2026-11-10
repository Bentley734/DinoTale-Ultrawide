#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dinotale32x9 {

struct BackgroundTileRequest {
    uint32_t background;
    uint32_t output;
    uint32_t tilemap;
    int32_t output_stride;
    int32_t start_x;
    int32_t start_y;
    int32_t columns;
    int32_t rows;
};

struct BackgroundRepeatLayout {
    uint32_t background_offset = 0;
    uint32_t metadata_offset = 0;
    uint32_t chunk_map_offset = 0;
    uint32_t tilemap_offset = 0;
    uint16_t chunk_columns = 0;
    uint16_t chunk_rows = 0;
    uint16_t atlas_stride = 0;
    uint16_t period_chunks = 0;
    bool authored_repeat = false;
};

struct RepeatFillResult {
    bool eligible = false;
    uint32_t entries_written = 0;
    uint16_t period_chunks = 0;
};

namespace background_repeat_detail {
inline bool range(uint32_t address, uint64_t length, size_t memory_size,
                  uint32_t& offset) {
    const uint32_t segment = address & 0xe0000000u;
    offset = address & 0x1fffffffu;
    return (segment == 0x80000000u || segment == 0xa0000000u) &&
           memory_size <= 0x800000 && (memory_size & 3u) == 0 &&
           offset <= memory_size && length <= memory_size - offset;
}

// The host stores each N64 word byte-swapped. Spell out the access so
// these diagnostics and tests do not depend on the compiler's byte order.
inline uint8_t byte(std::span<const uint8_t> memory, uint32_t offset) {
    return memory[offset ^ 3u];
}
inline uint16_t half(std::span<const uint8_t> memory, uint32_t offset) {
    offset ^= 2u;
    return uint16_t(uint16_t(memory[offset]) | uint16_t(memory[offset + 1]) << 8);
}
inline uint32_t word(std::span<const uint8_t> memory, uint32_t offset) {
    return uint32_t(memory[offset]) | uint32_t(memory[offset + 1]) << 8 |
           uint32_t(memory[offset + 2]) << 16 | uint32_t(memory[offset + 3]) << 24;
}
inline void store_half(std::span<uint8_t> memory, uint32_t offset, uint16_t value) {
    offset ^= 2u;
    memory[offset] = uint8_t(value);
    memory[offset + 1] = uint8_t(value >> 8);
}
inline bool overlap(uint32_t first, uint64_t first_size,
                    uint32_t second, uint64_t second_size) {
    return uint64_t(first) < uint64_t(second) + second_size &&
           uint64_t(second) < uint64_t(first) + first_size;
}
} // namespace background_repeat_detail

// The tilemap is a horizontal atlas of 16x16-tile chunks. A byte grid
// chooses a chunk at each world position; 0xff means empty. Detect a
// proven period across every grid row, rather than wrapping the entire
// level width (which can end partway through the repeating pattern).
inline bool detect_background_repeat(std::span<const uint8_t> memory,
                                     uint32_t background, uint32_t tilemap,
                                     BackgroundRepeatLayout& result) {
    using namespace background_repeat_detail;
    result = {};
    uint32_t bg, meta, grid, atlas;
    if ((background & 3u) != 0 || (tilemap & 1u) != 0 ||
        !range(background, 0x48, memory.size(), bg)) { return false; }
    // The playable terrain layer has a collision map. Never extend it.
    if (word(memory, bg + 0x44) != 0) { return false; }
    const uint32_t metadata = word(memory, bg + 0x20);
    if ((metadata & 3u) != 0 || !range(metadata, 0x2c, memory.size(), meta)) { return false; }
    const uint16_t columns = half(memory, meta + 0x12);
    const uint16_t rows = half(memory, meta + 0x14);
    const uint16_t stride = half(memory, meta + 0x16);
    // The original lookup stores chunk coordinates and IDs in bytes.
    if (columns == 0 || columns > 256 || rows == 0 || rows > 256 ||
        stride < 16 || stride > 0x7ff0 || (stride & 15u) != 0) { return false; }
    const uint32_t chunk_map = word(memory, meta + 0x18);
    const uint64_t grid_size = uint64_t(columns) * rows;
    const uint64_t atlas_size = uint64_t(stride) * 16 * 2;
    if (!range(chunk_map, grid_size, memory.size(), grid) ||
        !range(tilemap, atlas_size, memory.size(), atlas)) { return false; }
    bool any_art = false;
    const uint16_t atlas_chunks = stride / 16;
    for (uint32_t index = 0; index < grid_size; ++index) {
        const uint8_t id = byte(memory, grid + index);
        if (id != 0xff) {
            if (id >= atlas_chunks) { return false; }
            any_art = true;
        }
    }
    if (!any_art) { return false; }
    const bool authored = byte(memory, meta + 0x28) == 1;
    uint16_t period = 0;
    for (uint16_t candidate = 1; candidate <= columns / 2; ++candidate) {
        bool matches = true;
        for (uint16_t y = 0; y < rows && matches; ++y) {
            for (uint16_t x = candidate; x < columns; ++x) {
                if (byte(memory, grid + uint32_t(y) * columns + x) !=
                    byte(memory, grid + uint32_t(y) * columns + x % candidate)) {
                    matches = false;
                    break;
                }
            }
        }
        if (matches) { period = candidate; break; }
    }
    if (period == 0 && authored) { period = columns; }
    if (period == 0) { return false; }
    result = {bg, meta, grid, atlas, columns, rows, stride, period, authored};
    return true;
}

// Draw-time query for mask selection. The normal layer tilemap pointer is
// background +0x1c; no request or per-level registry is needed.
inline RepeatFillResult inspect_background_repeat(std::span<const uint8_t> memory,
                                                  uint32_t background) {
    uint32_t bg;
    if ((background & 3u) != 0 ||
        !background_repeat_detail::range(background, 0x48, memory.size(), bg)) { return {}; }
    BackgroundRepeatLayout layout;
    if (!detect_background_repeat(memory, background,
            background_repeat_detail::word(memory, bg + 0x1c), layout)) { return {}; }
    return {true, 0, layout.period_chunks};
}

// Invoke after func_8006B7D8 has built its normal request. Only negative-X
// output entries are replaced. The original positive-X tiles, animated
// tile flags, foreground/collision data, and negative-Y guard stay intact.
inline RepeatFillResult fill_negative_background_tiles(std::span<uint8_t> memory,
                                                       const BackgroundTileRequest& request) {
    using namespace background_repeat_detail;
    BackgroundRepeatLayout layout;
    if (!detect_background_repeat(memory, request.background, request.tilemap, layout) ||
        request.columns <= 0 || request.rows <= 0 || request.output_stride < 0 ||
        (request.rows > 1 && request.output_stride < request.columns) ||
        (request.output & 1u) != 0) { return {}; }
    const uint64_t output_size = (uint64_t(request.rows - 1) * uint32_t(request.output_stride) +
                                  uint32_t(request.columns)) * 2;
    uint32_t output;
    if (!range(request.output, output_size, memory.size(), output)) { return {}; }
    // Requests are heap/pool allocations, separate from source metadata.
    // Reject aliases before writing so invalid input cannot partially
    // overwrite the atlas that later rows still need to read.
    if (overlap(output, output_size, layout.background_offset, 0x48) ||
        overlap(output, output_size, layout.metadata_offset, 0x2c) ||
        overlap(output, output_size, layout.chunk_map_offset,
                uint64_t(layout.chunk_columns) * layout.chunk_rows) ||
        overlap(output, output_size, layout.tilemap_offset,
                uint64_t(layout.atlas_stride) * 16 * 2)) { return {}; }
    RepeatFillResult result{true, 0, layout.period_chunks};
    if (request.start_x >= 0) { return result; }
    const int32_t negative_columns = int32_t(std::min<int64_t>(request.columns, -int64_t(request.start_x)));
    const int32_t period_tiles = int32_t(layout.period_chunks) * 16;
    const int64_t world_rows = int64_t(layout.chunk_rows) * 16;
    for (int32_t row = 0; row < request.rows; ++row) {
        int64_t world_y = int64_t(request.start_y) + row;
        if (world_y < 0) { continue; }
        if (layout.authored_repeat) { world_y %= world_rows; }
        else if (world_y >= world_rows) { continue; }
        for (int32_t col = 0; col < negative_columns; ++col) {
            const int64_t world_x = int64_t(request.start_x) + col;
            const int32_t wrapped_x = int32_t((world_x % period_tiles + period_tiles) % period_tiles);
            const uint32_t chunk_index = uint32_t(world_y / 16) * layout.chunk_columns + uint32_t(wrapped_x / 16);
            const uint8_t chunk = byte(memory, layout.chunk_map_offset + chunk_index);
            uint16_t tile = 0;
            if (chunk != 0xff) {
                const uint32_t atlas_index = uint32_t(chunk) * 16 + uint32_t(wrapped_x & 15) +
                                             uint32_t(world_y & 15) * layout.atlas_stride;
                tile = half(memory, layout.tilemap_offset + atlas_index * 2);
            }
            const uint64_t output_index = uint64_t(row) * uint32_t(request.output_stride) + uint32_t(col);
            store_half(memory, output + uint32_t(output_index * 2), tile);
            ++result.entries_written;
        }
    }
    return result;
}

} // namespace dinotale32x9
