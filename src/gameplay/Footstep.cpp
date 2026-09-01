#include "gameplay/Footstep.hpp"

#include <cmath>

namespace gdp::gameplay {
namespace {

bool finitePosition(const wkc::core::Vec3 position) noexcept
{
    return std::isfinite(position.x)
        && std::isfinite(position.y)
        && std::isfinite(position.z);
}

bool validConfig(const FootstepConfig config) noexcept
{
    return std::isfinite(config.stepDistance) && config.stepDistance > 0.0F
        && std::isfinite(config.maximumSegmentDistance)
        && config.maximumSegmentDistance > 0.0F
        && config.maximumSegmentDistance < config.stepDistance;
}

} // namespace

std::optional<std::uint64_t> updateFootsteps(
    FootstepState& state,
    const wkc::core::Vec3 previousPosition,
    const wkc::core::Vec3 currentPosition,
    const bool grounded,
    const bool alive,
    const FootstepConfig config) noexcept
{
    if (!validConfig(config) || !finitePosition(previousPosition)
        || !finitePosition(currentPosition)) {
        return std::nullopt;
    }
    if (!alive || !grounded) {
        state.accumulatedDistance = 0.0F;
        return std::nullopt;
    }
    if (!std::isfinite(state.accumulatedDistance)
        || state.accumulatedDistance < 0.0F) {
        state.accumulatedDistance = 0.0F;
    }

    const float deltaX = currentPosition.x - previousPosition.x;
    const float deltaZ = currentPosition.z - previousPosition.z;
    const float distance = std::sqrt((deltaX * deltaX) + (deltaZ * deltaZ));
    if (!std::isfinite(distance) || distance > config.maximumSegmentDistance) {
        state.accumulatedDistance = 0.0F;
        return std::nullopt;
    }

    state.accumulatedDistance += distance;
    if (state.accumulatedDistance < config.stepDistance) {
        return std::nullopt;
    }
    state.accumulatedDistance = std::fmod(
        state.accumulatedDistance, config.stepDistance);
    return state.nextEventIndex++;
}

void resetFootsteps(FootstepState& state) noexcept
{
    state = {};
}

} // namespace gdp::gameplay
