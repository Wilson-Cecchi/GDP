#pragma once

#include "core/Math.hpp"

#include <cstdint>
#include <optional>

namespace gdp::gameplay {

struct FootstepConfig {
    float stepDistance{1.60F};
    float maximumSegmentDistance{0.50F};
};

struct FootstepState {
    float accumulatedDistance{0.0F};
    std::uint64_t nextEventIndex{0U};
};

[[nodiscard]] std::optional<std::uint64_t> updateFootsteps(
    FootstepState& state,
    wkc::core::Vec3 previousPosition,
    wkc::core::Vec3 currentPosition,
    bool grounded,
    bool alive,
    FootstepConfig config = {}) noexcept;

void resetFootsteps(FootstepState& state) noexcept;

} // namespace gdp::gameplay
