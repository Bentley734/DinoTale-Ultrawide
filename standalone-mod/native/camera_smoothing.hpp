#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dinotale32x9 {

struct CameraSmoothingSample {
    uint32_t camera;
    uint32_t player;
    uint64_t tick_ms;
    float position_x;
    float requested_velocity_x;
    bool enabled;
};

// Apply before func_8002A148, so its original horizontal wall probe still
// limits the eased movement. func_8002AAF0 handles vertical motion only;
// func_80029C60 then integrates this X velocity through the normal path.
class HorizontalCameraSmoother {
public:
    void reset() { ready_ = false; }

    float smooth(const CameraSmoothingSample& sample) {
        if (!sample.enabled || sample.camera == 0 || sample.player == 0 ||
            !std::isfinite(sample.position_x) ||
            !std::isfinite(sample.requested_velocity_x)) {
            reset();
            return sample.requested_velocity_x;
        }
        const float requested = speed_limit(sample.requested_velocity_x);
        const bool discontinuity = ready_ &&
            (camera_ != sample.camera || player_ != sample.player ||
             sample.tick_ms < tick_ms_ || sample.tick_ms - tick_ms_ > 250 ||
             std::fabs(sample.position_x - expected_position_x_) > 0.125f);
        // Keep the first frame after a scene change/teleport authoritative.
        // Normal movement subsequently ramps from the last accepted speed.
        if (!ready_ || discontinuity) {
            velocity_x_ = requested;
        }
        else {
            velocity_x_ += (requested - velocity_x_) * 0.25f;
            if (std::fabs(velocity_x_) < 0.001f) { velocity_x_ = 0.0f; }
        }
        ready_ = true;
        camera_ = sample.camera;
        player_ = sample.player;
        tick_ms_ = sample.tick_ms;
        position_x_ = sample.position_x;
        expected_position_x_ = position_x_ + velocity_x_;
        return velocity_x_;
    }

    // Call after the original wall probe. Retain its clipped velocity rather
    // than hidden momentum, and predict the integrator's existing +/-15 cap.
    // The game's millipixel arithmetic can differ slightly from this sum;
    // the position continuity tolerance accommodates that rounding.
    void accept_clamped_velocity(float velocity_x) {
        if (!ready_) { return; }
        if (!std::isfinite(velocity_x)) { reset(); return; }
        velocity_x_ = speed_limit(velocity_x);
        expected_position_x_ = position_x_ + velocity_x_;
    }

private:
    static float speed_limit(float value) { return std::clamp(value, -15.0f, 15.0f); }
    bool ready_ = false;
    uint32_t camera_ = 0;
    uint32_t player_ = 0;
    uint64_t tick_ms_ = 0;
    float position_x_ = 0;
    float expected_position_x_ = 0;
    float velocity_x_ = 0;
};
} // namespace dinotale32x9
