#pragma once

#include "core/Seed.hpp"
#include "world/RoomModule.hpp"
#include "world/World.hpp"
#include "world/WorldValidation.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace gdp::world {

struct WorldGenerationConfig {
    std::uint32_t roomCount{8};
    std::uint32_t loopBudget{1};
    std::uint32_t maximumAttempts{128};
    float roomSpacing{12.0F};
};

struct WorldGenerationResult {
    std::optional<World> world;
    GenerationFailure lastFailure{GenerationFailure::None};
    std::uint32_t attempts{0};

    [[nodiscard]] bool succeeded() const noexcept;
};

[[nodiscard]] WorldGenerationResult generateWorld(
    wkc::core::Seed masterSeed,
    WorldGenerationConfig config = {},
    std::span<const RoomModule> moduleLibrary = {});

} // namespace gdp::world
