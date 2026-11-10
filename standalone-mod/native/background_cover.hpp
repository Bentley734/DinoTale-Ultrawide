#pragma once

#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

namespace dinotale32x9 {
namespace background_cover_detail {
inline bool range(uint32_t address, uint64_t length, size_t size, uint32_t& offset) {
    const uint32_t segment = address & 0xe0000000u;
    offset = address & 0x1fffffffu;
    return (segment == 0x80000000u || segment == 0xa0000000u) &&
           size <= 0x800000 && (size & 3u) == 0 &&
           offset <= size && length <= size - offset;
}
inline uint32_t word(std::span<const uint8_t> memory, uint32_t at) {
    return uint32_t(memory[at]) | uint32_t(memory[at + 1]) << 8 |
           uint32_t(memory[at + 2]) << 16 | uint32_t(memory[at + 3]) << 24;
}
inline uint16_t half(std::span<const uint8_t> memory, uint32_t at) {
    at ^= 2u;
    return uint16_t(uint16_t(memory[at]) | uint16_t(memory[at + 1]) << 8);
}
inline uint8_t byte(std::span<const uint8_t> memory, uint32_t at) { return memory[at ^ 3u]; }
inline bool transparent(uint8_t texel, uint8_t format,
                        std::span<const uint8_t> memory, uint32_t palette) {
    if (format == 2) { return (half(memory, palette + uint32_t(texel) * 2) & 1u) == 0; } // CI8 / RGBA16 TLUT.
    if (format == 3) { return (texel & 15u) == 0; } // IA8: I4/A4.
    return texel == 0; // I8 replicates intensity into alpha.
}
} // namespace background_cover_detail

// Only a genuinely transparent empty tile may reveal the backdrop behind
// terrain. Graphics come from BG +0x0c; the active CI palette comes from
// RT +4. Format is RT +8 (CI=2, IA=3, I=4), and size +9 must be 8-bit=1.
inline bool background_tile_zero_transparent(std::span<const uint8_t> memory,
                                             uint32_t background, uint32_t render_target) {
    using namespace background_cover_detail;
    uint32_t bg, rt, graphics, palette = 0;
    if ((background & 3u) || (render_target & 3u) ||
        !range(background, 0x10, memory.size(), bg) ||
        !range(render_target, 0x0c, memory.size(), rt)) { return false; }
    const uint8_t format = byte(memory, rt + 8);
    if (byte(memory, rt + 9) != 1 || format < 2 || format > 4 ||
        !range(word(memory, bg + 0x0c), 256, memory.size(), graphics)) { return false; }
    if (format == 2) {
        const uint32_t address = word(memory, rt + 4);
        if ((address & 1u) || !range(address, 512, memory.size(), palette)) { return false; }
    }
    for (uint32_t i = 0; i < 256; ++i) {
        if (!transparent(byte(memory, graphics + i), format, memory, palette)) { return false; }
    }
    return true;
}

// Check the current ring pixels that the native helper would cover, not
// merely tile0: a reused ring must not expose stale opaque pixels. Image
// origins are linear, carrying a horizontal wrap into the following row.
// Columns at the right edge of the mask are excluded from this proof, so
// legitimate ground at world X=0 may still form its normal filtered edge.
inline bool background_left_strip_transparent(std::span<const uint8_t> memory,
                                              uint32_t render_target, uint32_t object) {
    using namespace background_cover_detail;
    uint32_t rt, obj, buffer, palette = 0;
    if ((render_target & 3u) || (object & 3u) ||
        !range(render_target, 0x38, memory.size(), rt) ||
        !range(object, 0x1c, memory.size(), obj)) { return false; }
    const uint8_t format = byte(memory, rt + 8);
    if (byte(memory, rt + 9) != 1 || format < 2 || format > 4) { return false; }
    const uint32_t width = half(memory, rt + 0x0a);
    const uint32_t height = half(memory, rt + 0x0c);
    const uint32_t frame_width = half(memory, rt + 0x18);
    const uint32_t frame_height = half(memory, rt + 0x1a);
    const uint32_t origin_x = word(memory, rt + 0x10);
    const uint32_t origin_y = word(memory, rt + 0x14);
    if (width == 0 || height == 0 || frame_width == 0 || frame_height == 0 ||
        frame_width > width || frame_height > height || origin_x >= width || origin_y >= height ||
        !range(word(memory, rt), uint64_t(width) * height, memory.size(), buffer)) { return false; }
    if (format == 2) {
        const uint32_t address = word(memory, rt + 4);
        if ((address & 1u) || !range(address, 512, memory.size(), palette)) { return false; }
    }
    const float left = std::bit_cast<float>(word(memory, obj + 0x18));
    if (!std::isfinite(left) || left >= 0.0f) { return false; }
    double scale_x = 1.0;
    // func_8007D5E0 ignores scales in modes0/1; other modes scale frameW.
    if (word(memory, rt + 0x34) > 1) {
        scale_x = std::bit_cast<float>(word(memory, rt + 0x28));
        if (!std::isfinite(scale_x) || scale_x <= 0.0 ||
            double(frame_width) * scale_x * 4.0 > 65535.0) { return false; }
    }
    const double screen_width = std::floor(double(frame_width) * scale_x * 4.0) / 4.0;
    const double masked_screen_width = std::min(std::ceil(-double(left)), screen_width);
    // The command quantizes source step to10.10, so a computed scale just
    // below1 still samples exactly one texel per pixel. Using raw division
    // here would incorrectly include opaque boundary art one column over.
    const double source_step = std::trunc(1024.0f / float(scale_x));
    if (source_step <= 0.0 || source_step > 65535.0) { return false; }
    const uint32_t columns = uint32_t(std::min(std::ceil(masked_screen_width * source_step / 1024.0), double(frame_width)));
    if (columns == 0) { return false; }
    const uint64_t capacity = uint64_t(width) * height;
    const uint64_t start = uint64_t(origin_y) * width + origin_x;
    for (uint32_t y = 0; y < frame_height; ++y) {
        for (uint32_t x = 0; x < columns; ++x) {
            const uint32_t offset = uint32_t((start + uint64_t(y) * width + x) % capacity);
            if (!transparent(byte(memory, buffer + offset), format, memory, palette)) { return false; }
        }
    }
    return true;
}

// Prevent the native helper from publishing an opaque black mask at all.
// Its only input changed here is the ephemeral draw object's left float.
// Restore exact bits immediately after the original call, including NaNs
// and signed zero; all other fields and the original result are preserved.
template <class Original>
decltype(auto) dispatch_background_cover(std::span<uint8_t> memory,
                                         uint32_t object, bool inhibit,
                                         Original&& original) {
    uint32_t at;
    if (!inhibit || (object & 3u) ||
        !background_cover_detail::range(object, 0x1c, memory.size(), at)) {
        return std::forward<Original>(original)();
    }
    at += 0x18;
    struct Restore {
        uint8_t* target;
        std::array<uint8_t, 4> value;
        ~Restore() { std::memcpy(target, value.data(), value.size()); }
    } restore{memory.data() + at, {memory[at], memory[at + 1], memory[at + 2], memory[at + 3]}};
    std::memset(memory.data() + at, 0, 4);
    return std::forward<Original>(original)();
}

} // namespace dinotale32x9
