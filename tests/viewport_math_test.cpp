#include "../src/game/viewport_math.hpp"

#include <cassert>
#include <limits>

int main() {
    using namespace dinotale::viewport;

    assert(width_for_aspect(4.0 / 3.0) == 320);
    assert(width_for_aspect(16.0 / 9.0) == 432);
    assert(width_for_aspect(21.0 / 9.0) == 560);
    assert(width_for_aspect(32.0 / 9.0) == 864);
    assert(width_for_aspect(64.0 / 9.0) == 864);
    assert(width_for_aspect(1.0) == 320);
    assert(width_for_aspect(0.0) == 432);
    assert(width_for_aspect(std::numeric_limits<double>::quiet_NaN()) == 432);
    assert(width_for_aspect(std::numeric_limits<double>::infinity()) == 432);

    // Zoomed areas (4-1) must stream enough columns for their larger layers.
    assert(scale_layer_width(480, 432) == 656);
    assert(scale_layer_width(480, 864) == 1296);
    assert(scale_layer_width(320, 864) == 864);

    // Resizing cannot expose unallocated columns. Shrinking brings the HUD
    // and player edge constraints back inside the smaller visible frame.
    assert(visible_width(432, 32.0 / 9.0) == 432);
    assert(visible_width(864, 16.0 / 9.0) == 432);
    assert(visible_width(864, 32.0 / 9.0) == 864);
    assert(hud_width(864, HudPlacement::Original) == 320);
    assert(hud_width(864, HudPlacement::Clamp16x9) == 432);
    assert(hud_width(864, HudPlacement::Expand) == 864);
    assert(hud_width(320, HudPlacement::Clamp16x9) == 320);

    assert(render_aspect_limit(320) == float(4.0 / 3.0));
    assert(render_aspect_limit(432) >= float(16.0 / 9.0));
    assert(render_aspect_limit(864) == float(32.0 / 9.0));
}
