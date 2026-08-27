#pragma once

#include "world/World.hpp"

#include <cstdint>
#include <vector>

namespace gdp::world {

enum class GenerationFailure : std::uint8_t {
    None,
    GraphInvalid,
    ModuleUnavailable,
    RoomOverlap,
    ConnectorUnresolved,
    CorridorInvalid,
};

struct WorldValidationResult {
    std::vector<GenerationFailure> failures;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] GenerationFailure primaryFailure() const noexcept;
};

[[nodiscard]] WorldValidationResult validateWorld(const World& world);

} // namespace gdp::world
