#pragma once

#include "entities/EntityManager.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Weapon.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace gdp::gameplay {

struct LootCollectionSummary {
    std::size_t itemsInRange{0};
    std::size_t collected{0};
    std::size_t rewardableCollected{0};
    std::size_t blockedByCapacity{0};
    float healthRestored{0.0F};
    float armorAdded{0.0F};
    std::uint32_t ammoAdded{0};
    std::uint32_t keysAdded{0};
    std::size_t weaponsEquipped{0};
    std::size_t weaponsRecovered{0};
    std::size_t specialsActivated{0};
    std::vector<gdp::entities::EntityId> collectedItems;
};

[[nodiscard]] LootCollectionSummary collectNearbyLoot(
    gdp::entities::EntityManager& manager,
    Player& player,
    Weapon& weapon,
    std::uint32_t roomId,
    float collectionRadius = 0.85F) noexcept;

[[nodiscard]] std::optional<gdp::entities::EntityId> dropSelectedWeapon(
    gdp::entities::EntityManager& manager,
    Player& player,
    Weapon& weapon,
    const CollisionWorld& collisionWorld,
    std::uint32_t roomId);

} // namespace gdp::gameplay
