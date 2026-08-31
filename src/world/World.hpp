#pragma once

#include "core/Seed.hpp"
#include "world/Corridor.hpp"
#include "world/MapGraph.hpp"
#include "world/RoomModule.hpp"

#include <cstdint>
#include <vector>

namespace gdp::world {

struct World {
    wkc::core::Seed masterSeed{0};
    wkc::core::Seed generationSeed{0};
    wkc::core::GenerationVersion generationVersion{};
    std::uint32_t acceptedAttempt{0};
    MapGraph graph;
    std::vector<RoomInstance> rooms;
    std::vector<CorridorPath> corridors;
};

[[nodiscard]] std::uint64_t worldFingerprint(const World& world) noexcept;

} // namespace gdp::world
