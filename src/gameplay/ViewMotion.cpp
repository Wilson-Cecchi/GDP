#include "gameplay/ViewMotion.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace gdp::gameplay {
namespace {

bool finitePosition(const wkc::core::Vec3 position) noexcept
{
    return std::isfinite(position.x)
        && std::isfinite(position.y)
        && std::isfinite(position.z);
}

bool validConfig(const ViewMotionConfig config) noexcept
{
    return std::isfinite(config.gaitCycleDistance)
        && config.gaitCycleDistance > 0.0F
        && std::isfinite(config.maximumSegmentDistance)
        && config.maximumSegmentDistance > 0.0F
        && config.maximumSegmentDistance < config.gaitCycleDistance
        && std::isfinite(config.verticalAmplitude)
        && config.verticalAmplitude >= 0.0F
        && std::isfinite(config.lateralAmplitude)
        && config.lateralAmplitude >= 0.0F
        && std::isfinite(config.blendSpeed) && config.blendSpeed > 0.0F;
}

float approach(
    const float current,
    const float target,
    const float maximumChange) noexcept
{
    if (current < target) {
        return std::min(target, current + maximumChange);
    }
    return std::max(target, current - maximumChange);
}

} // namespace

void updateViewMotion(
    ViewMotionState& state,
    const wkc::core::Vec3 previousPosition,
    const wkc::core::Vec3 currentPosition,
    const bool grounded,
    const bool alive,
    const float fixedStepSeconds,
    const ViewMotionConfig config) noexcept
{
    if (!validConfig(config) || !finitePosition(previousPosition)
        || !finitePosition(currentPosition)
        || !std::isfinite(fixedStepSeconds) || fixedStepSeconds <= 0.0F) {
        return;
    }
    if (!std::isfinite(state.phaseRadians) || !std::isfinite(state.weight)
        || state.weight < 0.0F || state.weight > 1.0F) {
        state = {};
    }

    const float deltaX = currentPosition.x - previousPosition.x;
    const float deltaZ = currentPosition.z - previousPosition.z;
    const float distance = std::sqrt((deltaX * deltaX) + (deltaZ * deltaZ));
    if (!std::isfinite(distance) || distance > config.maximumSegmentDistance) {
        state = {};
        return;
    }

    constexpr float movementEpsilon = 1.0e-5F;
    const bool walking = alive && grounded && distance > movementEpsilon;
    const float targetWeight = walking ? 1.0F : 0.0F;
    state.weight = approach(
        state.weight, targetWeight, config.blendSpeed * fixedStepSeconds);
    if (!walking) {
        return;
    }

    constexpr float fullCycle = 2.0F * std::numbers::pi_v<float>;
    state.phaseRadians = std::fmod(
        state.phaseRadians + ((distance / config.gaitCycleDistance) * fullCycle),
        fullCycle);
}

wkc::core::Vec3 viewMotionOffset(
    const ViewMotionState& state,
    const wkc::core::Vec3 rightDirection,
    const ViewMotionConfig config) noexcept
{
    if (!validConfig(config) || !std::isfinite(state.phaseRadians)
        || !std::isfinite(state.weight) || state.weight <= 0.0F
        || state.weight > 1.0F || !finitePosition(rightDirection)) {
        return {};
    }
    const wkc::core::Vec3 planarRight
        = wkc::core::normalized({rightDirection.x, 0.0F, rightDirection.z});
    const float lateral = std::sin(state.phaseRadians)
        * config.lateralAmplitude * state.weight;
    const float vertical = -std::abs(std::cos(state.phaseRadians))
        * config.verticalAmplitude * state.weight;
    return (planarRight * lateral) + wkc::core::Vec3{0.0F, vertical, 0.0F};
}

void resetViewMotion(ViewMotionState& state) noexcept
{
    state = {};
}

} // namespace gdp::gameplay
