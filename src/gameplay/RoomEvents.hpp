#pragma once

#include "entities/EntityManager.hpp"
#include "world/EventGenerator.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace gdp::gameplay {

enum class RoomEventPhase : std::uint8_t {
    Dormant,
    Active,
    Completed,
};

struct RoomEventRuntime {
    std::uint32_t roomId{0};
    gdp::world::RoomEventPlan plan;
    RoomEventPhase phase{RoomEventPhase::Dormant};
    std::uint32_t nextWave{0};
    float cooldownRemaining{0.0F};
    bool rewardSpawned{false};
    std::vector<gdp::entities::EntityId> eventEnemies;
};

struct RoomEventUpdateSummary {
    bool activated{false};
    bool completed{false};
    std::size_t wavesSpawned{0};
    std::size_t enemiesSpawned{0};
    std::size_t rewardsSpawned{0};
    std::vector<gdp::entities::EntityId> spawnedEnemies;
    std::vector<gdp::entities::EntityId> createdEntities;
};

[[nodiscard]] RoomEventRuntime makeRoomEventRuntime(
    gdp::world::RoomEventPlan plan,
    std::uint32_t roomId);

[[nodiscard]] std::vector<gdp::entities::EntityId> stageRoomEventEnemies(
    RoomEventRuntime& runtime,
    gdp::entities::EntityManager& manager);

[[nodiscard]] RoomEventUpdateSummary updateRoomEvent(
    RoomEventRuntime& runtime,
    gdp::entities::EntityManager& manager,
    bool roomEntered,
    bool terminalUsed,
    float fixedStepSeconds);

[[nodiscard]] bool isRoomLocked(const RoomEventRuntime& runtime) noexcept;
[[nodiscard]] bool shouldSealRoomDoors(
    const RoomEventRuntime& runtime,
    const gdp::entities::EntityManager& manager);
[[nodiscard]] std::string_view roomEventTypeName(
    gdp::world::RoomEventType type) noexcept;
[[nodiscard]] std::string_view roomEventPhaseName(RoomEventPhase phase) noexcept;

} // namespace gdp::gameplay
