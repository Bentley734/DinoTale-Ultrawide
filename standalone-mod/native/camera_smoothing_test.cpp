#include "camera_smoothing.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
using dinotale32x9::CameraSmoothingSample;
using dinotale32x9::HorizontalCameraSmoother;

struct Follow {
    HorizontalCameraSmoother smoother;
    float position = 1000.0f;
    uint64_t tick = 0;
    float step(float requested, float left = -100000, float right = 100000) {
        tick += 16;
        float velocity = smoother.smooth({0x80010000, 0x80020000, tick,
                                          position, requested, true});
        // Model the ordering in the game: smoothing, original wall probe,
        // then integration. The wall probe remains the position authority.
        velocity = std::clamp(velocity, left - position, right - position);
        smoother.accept_clamped_velocity(velocity);
        position += std::clamp(velocity, -15.0f, 15.0f);
        return velocity;
    }
};

bool near(float a, float b, float tolerance = 0.001f) { return std::fabs(a - b) <= tolerance; }

void test_run_stop_and_reverse() {
    Follow follow;
    assert(follow.step(0) == 0);
    float previous = 0;
    for (int i = 0; i < 120; ++i) {
        const float velocity = follow.step(4);
        assert(velocity > previous && velocity <= 4 || near(velocity, 4));
        assert(velocity - previous <= 1.0001f);
        previous = velocity;
    }
    assert(near(previous, 4)); // Full running speed is reached, without drift.
    for (int i = 0; i < 60; ++i) {
        const float velocity = follow.step(0);
        assert(velocity >= 0 && velocity <= previous);
        previous = velocity;
    }
    assert(previous == 0);
    // The eased tail recovers the initial ramp distance; a complete run has
    // the same travel as the original commands, within rounding tolerance.
    assert(near(follow.position, 1480.0f, 0.01f));
    previous = follow.step(4);
    for (int i = 0; i < 20; ++i) { previous = follow.step(4); }
    const float reversed = follow.step(-4);
    assert(reversed < previous && reversed > -4);
    for (int i = 0; i < 100; ++i) { previous = follow.step(-4); }
    assert(near(previous, -4));
}

void test_walls_clear_momentum() {
    Follow follow;
    follow.position = 0;
    follow.step(0);
    for (int i = 0; i < 40; ++i) {
        follow.step(4, 0, 10);
        assert(follow.position >= 0 && follow.position <= 10);
    }
    assert(follow.position == 10);
    // A stop at the wall must not release an old eased velocity next frame.
    assert(follow.step(0, 0, 100) == 0);
    assert(follow.position == 10);
    assert(near(follow.step(-4, 0, 100), -1));
    for (int i = 0; i < 40; ++i) {
        follow.step(-4, 0, 100);
        assert(follow.position >= 0 && follow.position <= 100);
    }
    assert(follow.position == 0);
    assert(follow.step(0, -100, 100) == 0);
}

void test_native_position_rounding() {
    // func_80065388 truncates each operand to millipixels before adding.
    // That rounding must not be mistaken for a teleport and reset easing.
    for (float initial : {-50000.0f, 50000.0f}) {
        HorizontalCameraSmoother smoother;
        float position = initial;
        smoother.smooth({1, 2, 0, position, 0, true});
        smoother.accept_clamped_velocity(0);
        float previous = 0;
        for (uint64_t frame = 1; frame <= 30; ++frame) {
            const float velocity = smoother.smooth({1, 2, frame * 16, position, 4, true});
            assert(near(velocity, previous + (4 - previous) * 0.25f));
            smoother.accept_clamped_velocity(velocity);
            const int32_t old_millipixels = int32_t(position * 1000.0f);
            const int32_t step_millipixels = int32_t(velocity * 1000.0f);
            position = float(old_millipixels + step_millipixels) / 1000.0f;
            previous = velocity;
        }
    }
}

void test_limits_and_resets() {
    Follow follow;
    follow.step(0);
    // Clamp before blending: a huge input starts at 3.75, not the max15.
    assert(near(follow.step(1000), 3.75f));
    for (int i = 0; i < 100; ++i) { assert(std::fabs(follow.step(1000)) <= 15); }
    assert(near(follow.step(1000), 15));

    HorizontalCameraSmoother smoother;
    CameraSmoothingSample sample{1, 2, 100, 1000, 0, true};
    assert(smoother.smooth(sample) == 0);
    smoother.accept_clamped_velocity(0);
    sample.tick_ms += 16;
    sample.requested_velocity_x = 4;
    assert(smoother.smooth(sample) == 1);
    smoother.accept_clamped_velocity(0); // The original probe rejected it.
    sample.tick_ms += 16;
    sample.requested_velocity_x = 0;
    assert(smoother.smooth(sample) == 0);

    // New identity, long gaps, time discontinuity and position jumps must
    // not blend old motion into a new camera scene or scripted relocation.
    for (int scenario = 0; scenario < 5; ++scenario) {
        smoother.reset();
        sample = {1, 2, 1000, 1000, 0, true};
        smoother.smooth(sample);
        smoother.accept_clamped_velocity(0);
        sample.requested_velocity_x = 4;
        sample.tick_ms += 16;
        if (scenario == 0) { ++sample.camera; }
        if (scenario == 1) { ++sample.player; }
        if (scenario == 2) { sample.tick_ms += 251; }
        if (scenario == 3) { sample.tick_ms = 1; }
        if (scenario == 4) { sample.position_x += 500; }
        assert(smoother.smooth(sample) == 4);
    }
    // Original mode and other callbacks pass their velocity through and
    // discard state; reentering follow starts with no stale interpolation.
    sample.enabled = false;
    sample.requested_velocity_x = 7;
    assert(smoother.smooth(sample) == 7);
    sample.enabled = true;
    sample.requested_velocity_x = -4;
    assert(smoother.smooth(sample) == -4);
    sample.position_x = std::numeric_limits<float>::quiet_NaN();
    assert(smoother.smooth(sample) == -4);
    sample.position_x = 1000;
    sample.requested_velocity_x = std::numeric_limits<float>::infinity();
    assert(std::isinf(smoother.smooth(sample)));
    sample.requested_velocity_x = std::numeric_limits<float>::quiet_NaN();
    assert(std::isnan(smoother.smooth(sample)));
    smoother.accept_clamped_velocity(std::numeric_limits<float>::quiet_NaN());
}
} // namespace

int main() {
    test_run_stop_and_reverse();
    test_walls_clear_momentum();
    test_native_position_rounding();
    test_limits_and_resets();
    std::cout << "Horizontal camera easing and wall-order checks passed.\n";
}
