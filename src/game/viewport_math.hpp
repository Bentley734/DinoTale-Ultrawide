#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dinotale::viewport {
    constexpr int32_t original_width = 320;
    constexpr double original_aspect = 4.0 / 3.0;
    constexpr double fallback_aspect = 16.0 / 9.0;
    constexpr double max_aspect = 32.0 / 9.0;

    // Background images wrap on 16-pixel tile boundaries. Round up so the
    // allocated image covers the whole requested view, including fractional
    // columns at ratios such as 16:9 and 32:9.
    inline int32_t width_for_aspect(double aspect) {
        if (!std::isfinite(aspect) || aspect <= 0.0) {
            aspect = fallback_aspect;
        }
        aspect = std::clamp(aspect, original_aspect, max_aspect);
        // 240 screen lines / 16 pixels per tile = 15. Avoid dividing by
        // 4:3 again: that can turn an exact tile boundary (21:9 = 35
        // columns) into 35.00000000000001 and allocate a spurious column.
        const double columns = std::nextafter(aspect * 15.0, 0.0);
        return int32_t(std::ceil(columns)) * 16;
    }

    inline int32_t scale_layer_width(int32_t width, int32_t view_width) {
        const int64_t columns = (int64_t(width) * view_width + original_width * 16 - 1) / (original_width * 16);
        return std::max(int32_t(columns * 16), width);
    }

    // A smaller window can use the existing background allocation. A larger
    // window must wait for the next level load before exposing more scenery.
    inline int32_t visible_width(int32_t allocated_width, double window_aspect) {
        return std::min(allocated_width, width_for_aspect(window_aspect));
    }

    enum class HudPlacement { Original, Clamp16x9, Expand };

    inline int32_t hud_width(int32_t visible_width, HudPlacement placement) {
        switch (placement) {
        case HudPlacement::Original:
            return original_width;
        case HudPlacement::Clamp16x9:
            return std::min(visible_width, width_for_aspect(fallback_aspect));
        case HudPlacement::Expand:
        default:
            return visible_width;
        }
    }

    inline float render_aspect_limit(int32_t allocated_width) {
        return float(std::clamp(double(allocated_width) / 240.0, original_aspect, max_aspect));
    }
}
