#include "gameplay/Door.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::gameplay {
namespace {

bool validConfig(const DoorConfig config) noexcept
{
    return std::isfinite(config.openingSeconds) && config.openingSeconds > 0.0F
        && std::isfinite(config.openHoldSeconds) && config.openHoldSeconds >= 0.0F
        && std::isfinite(config.closingSeconds) && config.closingSeconds > 0.0F
        && std::isfinite(config.passableOpenness)
        && config.passableOpenness >= 0.0F && config.passableOpenness <= 1.0F;
}

} // namespace

DoorRuntime makeDoorRuntime(const gdp::world::EdgeId edge) noexcept
{
    return {.edge = edge};
}

bool requestDoorOpen(
    DoorRuntime& door,
    const bool locked,
    const DoorConfig config) noexcept
{
    if (door.edge == 0U || locked || !validConfig(config)) {
        return false;
    }
    door.holdRemaining = config.openHoldSeconds;
    if (door.state == gdp::world::DoorState::Open) {
        return true;
    }
    door.state = gdp::world::DoorState::Opening;
    return true;
}

DoorUpdate updateDoor(
    DoorRuntime& door,
    const float fixedStepSeconds,
    const bool locked,
    const bool occupied,
    const DoorConfig config) noexcept
{
    DoorUpdate result;
    if (door.edge == 0U || !std::isfinite(fixedStepSeconds)
        || fixedStepSeconds <= 0.0F || !validConfig(config)) {
        return result;
    }

    if (locked) {
        door.holdRemaining = 0.0F;
        if (door.openness > 0.0F) {
            if (door.state != gdp::world::DoorState::Closing) {
                result.startedClosing = true;
            }
            door.state = gdp::world::DoorState::Closing;
            door.openness = std::max(
                0.0F, door.openness - (fixedStepSeconds / config.closingSeconds));
        }
        if (door.openness <= 0.0F) {
            result.fullyClosed = door.state != gdp::world::DoorState::Locked;
            door.openness = 0.0F;
            door.state = gdp::world::DoorState::Locked;
        }
        return result;
    }

    if (door.state == gdp::world::DoorState::Locked) {
        door.state = gdp::world::DoorState::Closed;
    }
    if (occupied && door.state == gdp::world::DoorState::Closing) {
        door.state = gdp::world::DoorState::Opening;
        door.holdRemaining = config.openHoldSeconds;
        result.startedOpening = true;
    }

    switch (door.state) {
    case gdp::world::DoorState::Opening:
        door.openness = std::min(
            1.0F, door.openness + (fixedStepSeconds / config.openingSeconds));
        if (door.openness >= 1.0F) {
            door.openness = 1.0F;
            door.state = gdp::world::DoorState::Open;
            door.holdRemaining = config.openHoldSeconds;
            result.fullyOpened = true;
        }
        break;
    case gdp::world::DoorState::Open:
        if (occupied) {
            door.holdRemaining = config.openHoldSeconds;
        } else {
            door.holdRemaining = std::max(0.0F, door.holdRemaining - fixedStepSeconds);
            if (door.holdRemaining <= 0.0F) {
                door.state = gdp::world::DoorState::Closing;
                result.startedClosing = true;
            }
        }
        break;
    case gdp::world::DoorState::Closing:
        door.openness = std::max(
            0.0F, door.openness - (fixedStepSeconds / config.closingSeconds));
        if (door.openness <= 0.0F) {
            door.openness = 0.0F;
            door.state = gdp::world::DoorState::Closed;
            result.fullyClosed = true;
        }
        break;
    case gdp::world::DoorState::Closed:
    case gdp::world::DoorState::Disabled:
    case gdp::world::DoorState::Locked:
        break;
    }
    return result;
}

bool isDoorPassable(
    const DoorRuntime& door,
    const DoorConfig config) noexcept
{
    return validConfig(config)
        && door.state != gdp::world::DoorState::Locked
        && door.state != gdp::world::DoorState::Disabled
        && door.openness >= config.passableOpenness;
}

} // namespace gdp::gameplay
