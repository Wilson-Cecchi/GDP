#pragma once

#include "core/Math.hpp"

namespace gdp::gameplay {

struct ViewMotionConfig {
    float gaitCycleDistance{3.20F};
    float maximumSegmentDistance{0.50F};
    float verticalAmplitude{0.035F};
    float lateralAmplitude{0.018F};
    float blendSpeed{7.0F};
};

struct ViewMotionState {
    float phaseRadians{0.0F};
    float weight{0.0F};
};

void updateViewMotion(
    ViewMotionState& state,
    wkc::core::Vec3 previousPosition,
    wkc::core::Vec3 currentPosition,
    bool grounded,
    bool alive,
    float fixedStepSeconds,
    ViewMotionConfig config = {}) noexcept;

[[nodiscard]] wkc::core::Vec3 viewMotionOffset(
    const ViewMotionState& state,
    wkc::core::Vec3 rightDirection,
    ViewMotionConfig config = {}) noexcept;

void resetViewMotion(ViewMotionState& state) noexcept;

} // namespace gdp::gameplay
