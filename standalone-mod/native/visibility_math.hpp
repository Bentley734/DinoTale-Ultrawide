#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

namespace dinotale32x9 {

// Matches the existing DinoTale actor_screen_x helper. Inputs and output
// use the original 320-pixel camera coordinates; each additional strip is
// mapped to an original edge column before the game's own screen test.
inline int32_t map_visibility_x(int32_t x, int32_t view_width) {
    const int32_t extra = std::max(0, view_width - 320) / 2;
    if (extra == 0) { return x; }
    if (x < 0) { return int32_t(std::min<int64_t>(1, int64_t(x) + extra)); }
    if (x >= 320) { return std::max(319, x - extra); }
    return x;
}

// func_80065BC8 includes the boundary: width + actor margin - abs(dx)
// must be >= 0, followed by the same test on Y. Callers reserve 0x7fff
// horizontal actor margin as an unconditional pass before reaching it.
inline bool visibility_aabb(int32_t x, int32_t y, int16_t margin_x,
                            int16_t margin_y, int32_t half_width,
                            int32_t half_height) {
    if (margin_x == 0x7fff) { return true; }
    const int64_t dx = int64_t(x) - 160;
    const int64_t dy = int64_t(y) - 120;
    const int64_t abs_x = dx < 0 ? -dx : dx;
    const int64_t abs_y = dy < 0 ? -dy : dy;
    return abs_x <= int64_t(half_width) + margin_x &&
           abs_y <= int64_t(half_height) + margin_y;
}

// The predicates receive a pointer to two signed screen coordinates in
// emulated RDRAM, not X in register a0. Preserve that pointer and Y, call
// the original once, and restore the input even if it alters its context.
// Host word-swapped RDRAM stores whole MIPS words in native byte order.
template <class Mapper, class Original>
decltype(auto) dispatch_mapped_visibility(std::span<uint8_t> memory,
                                         uint32_t screen_position,
                                         Mapper&& mapper,
                                         Original&& original) {
    const uint32_t segment = screen_position & 0xe0000000u;
    const uint32_t offset = screen_position & 0x1fffffffu;
    if ((segment != 0x80000000u && segment != 0xa0000000u) ||
        (offset & 3u) != 0 || offset > memory.size() ||
        memory.size() - offset < 8) {
        return std::forward<Original>(original)();
    }
    int32_t saved_x;
    std::memcpy(&saved_x, memory.data() + offset, sizeof(saved_x));
    struct Restore {
        uint8_t* target;
        int32_t value;
        ~Restore() { std::memcpy(target, &value, sizeof(value)); }
    } restore{memory.data() + offset, saved_x};
    const int32_t mapped_x = std::forward<Mapper>(mapper)(saved_x);
    std::memcpy(memory.data() + offset, &mapped_x, sizeof(mapped_x));
    return std::forward<Original>(original)();
}

} // namespace dinotale32x9
