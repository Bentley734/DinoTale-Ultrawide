#pragma once

#include "ground_extension.hpp"
#include "background_lookup.hpp"
#include <bit>
#include <cmath>

namespace dinotale32x9 {
struct GroundRuntimeState {
    uint32_t background = 0;
    int32_t minimum_tile_x = 0;
    int32_t extra_screen_pixels = 0;
    GroundCollisionLayout collision{};
    GroundExtensionLayout visual{};
    bool eligible() const { return background != 0 && minimum_tile_x < 0; }
};

inline int32_t ground_minimum_tile_x(int32_t extra, float scale) {
    if (extra <= 0 || !std::isfinite(scale) || scale <= 0) { return 0; }
    // Include body/probe and tile-rounding room beyond the visible edge.
    // Some game queries use signed 16-bit world pixels, so never extend
    // beyond that coordinate domain.
    const double tiles = std::ceil((double(extra)+64.0)/(16.0*double(scale)));
    return tiles > 0 && tiles <= 2047 ? -int32_t(tiles) : 0;
}

inline GroundRuntimeState read_ground_runtime(std::span<const uint8_t> memory, int32_t extra) {
    using namespace background_lookup_detail;
    GroundRuntimeState result;
    uint32_t global, list, slots, actor, work, bg, camera, player;
    if (extra <= 0 || !range(0x800F8810,4,memory.size(),global)
        || !range(word(memory,global),0x14,memory.size(),list)
        || !range(word(memory,list+0x10),8,memory.size(),slots)
        || !range(word(memory,slots+4),0x170,memory.size(),actor)
        || !range(word(memory,actor+0x16C),0x1C0,memory.size(),work)) { return result; }
    const uint32_t background = word(memory,work+0x1B0);
    if (!range(background,0x48,memory.size(),bg)
        || !inspect_ground_collision(memory,background,result.collision)
        || !result.collision.edge_has_flat_support) { return {}; }
    result.visual = inspect_ground_extension(memory,background);
    if (!result.visual.eligible || !range(0x800F8318,4,memory.size(),global)
        || !range(word(memory,global),0xEC,memory.size(),camera)
        || !range(word(memory,camera+0xE8),0xB8,memory.size(),player)) { return {}; }
    const float scale = std::bit_cast<float>(word(memory,player+0xB4));
    const int32_t minimum = ground_minimum_tile_x(extra,scale);
    if (!minimum) { return {}; }
    result.background = background;
    result.minimum_tile_x = minimum;
    result.extra_screen_pixels = extra;
    return result;
}
} // namespace dinotale32x9
