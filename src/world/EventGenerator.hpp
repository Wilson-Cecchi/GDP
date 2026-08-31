#pragma once

#include "core/Seed.hpp"
#include "entities/EntityManager.hpp"
#include "world/RoomModule.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace gdp::world {

enum class RoomEventType : std::uint8_t {
    None,
    TerminalAmbush,
    Reinforcements,
    Lockdown,
    SupplyDrop,
    Count,
};

enum class RoomEventTrigger : std::uint8_t {
    RoomEntry,
    TerminalUse,
};

struct EventEnemySpawn {
    gdp::entities::EnemyArchetype archetype{gdp::entities::EnemyArchetype::Worker};
    wkc::core::Vec3 position{};
    float health{50.0F};
    wkc::core::Seed visualSeed{0};
    std::uint32_t wave{0};

    constexpr bool operator==(const EventEnemySpawn&) const noexcept = default;
};

struct RoomEventPlan {
    wkc::core::Seed seed{0};
    RoomRole role{RoomRole::Normal};
    RoomTheme theme{RoomTheme::Industrial};
    std::uint32_t depth{0};
    RoomEventType type{RoomEventType::None};
    RoomEventTrigger trigger{RoomEventTrigger::RoomEntry};
    bool locksRoom{false};
    bool requiresRoomClear{false};
    std::uint32_t waveCount{0};
    float interWaveDelay{0.0F};
    std::vector<EventEnemySpawn> spawns;
    gdp::entities::ItemType rewardType{gdp::entities::ItemType::Ammo};
    std::uint32_t rewardQuantity{0};
    wkc::core::Vec3 rewardPosition{};
    std::uint64_t planHash{0};
};

enum class EventValidationError : std::uint8_t {
    InvalidRole,
    InvalidTheme,
    InvalidType,
    MissingZone,
    InvalidTrigger,
    InvalidWave,
    InvalidSpawn,
    OutsideZone,
    InvalidReward,
    InvalidPlanHash,
};

struct EventValidationResult {
    std::vector<EventValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

[[nodiscard]] RoomEventPlan generateRoomEventPlan(
    wkc::core::Seed roomSeed,
    RoomRole role,
    RoomTheme theme,
    std::uint32_t depth,
    std::span<const SpawnZone> eventZones,
    float difficulty = 1.0F);

[[nodiscard]] EventValidationResult validateRoomEventPlan(
    const RoomEventPlan& plan,
    std::span<const SpawnZone> eventZones);

[[nodiscard]] std::uint64_t roomEventPlanHash(const RoomEventPlan& plan) noexcept;

} // namespace gdp::world
