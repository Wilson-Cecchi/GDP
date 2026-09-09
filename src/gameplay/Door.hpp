#pragma once

#include "world/Connector.hpp"

namespace gdp::gameplay {

struct DoorConfig {
    float openingSeconds{0.45F};
    float openHoldSeconds{1.60F};
    float closingSeconds{0.60F};
    float passableOpenness{0.90F}; // fração visual já suavizada do vão
};

struct DoorRuntime {
    gdp::world::EdgeId edge{0};
    gdp::world::DoorState state{gdp::world::DoorState::Closed};
    float openness{0.0F};
    float holdRemaining{0.0F};
};

struct DoorUpdate {
    bool startedOpening{false};
    bool fullyOpened{false};
    bool startedClosing{false};
    bool fullyClosed{false};
};

[[nodiscard]] DoorRuntime makeDoorRuntime(gdp::world::EdgeId edge) noexcept;
[[nodiscard]] bool requestDoorOpen(
    DoorRuntime& door,
    bool locked,
    DoorConfig config = {}) noexcept;
[[nodiscard]] DoorUpdate updateDoor(
    DoorRuntime& door,
    float fixedStepSeconds,
    bool locked,
    bool occupied,
    DoorConfig config = {}) noexcept;
[[nodiscard]] float doorPanelTravel(float openness) noexcept;
[[nodiscard]] bool isDoorPassable(
    const DoorRuntime& door,
    DoorConfig config = {}) noexcept;

} // namespace gdp::gameplay
