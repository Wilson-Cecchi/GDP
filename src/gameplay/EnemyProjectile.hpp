#pragma once

#include "core/Math.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Player.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gdp::gameplay {

// A shot keeps its launch direction: moving after the muzzle flash can avoid it.
struct EnemyProjectile {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 velocity{};
    entities::EntityId source{};
    std::uint32_t roomId{0U};
    float damage{0.0F};
    float remainingSeconds{0.0F};
    bool neural{false};
    bool threatenedPlayerAtLaunch{false};
    float targetDistance{0.0F};
    float traveledDistance{0.0F};
};

struct EnemyProjectileEvent {
    entities::EntityId source{};
    bool neural{false};
    bool hitPlayer{false};
    bool blocked{false};
    bool dodged{false};
    float damageApplied{0.0F};
    entities::EntityId hitEnemy{};
    bool killedEnemy{false};
    entities::EntityId hitProp{};
    bool destroyedProp{false};
    entities::EntityId spawnedDrop{};
};

// Predict whether a living ally obstructs a shot. Physical projectile
// collision remains authoritative if either character moves after launch.
[[nodiscard]] bool allyInEnemyFireLane(
    const entities::EntityManager& enemies,
    entities::EntityId shooter,
    wkc::core::Vec3 origin,
    wkc::core::Vec3 direction,
    float distance) noexcept;

class EnemyProjectileSystem {
public:
    void spawn(std::span<const EnemyProjectile> shots);
    // world contains architecture and doors; this system resolves dynamic
    // props from entities so destructible cover can change between bullets.
    [[nodiscard]] std::vector<EnemyProjectileEvent> update(
        float step, Player& player, const CollisionWorld& world,
        entities::EntityManager& enemies);
    void clear() noexcept { projectiles_.clear(); }
    [[nodiscard]] std::span<const EnemyProjectile> projectiles() const noexcept
    {
        return projectiles_;
    }

private:
    std::vector<EnemyProjectile> projectiles_;
};

} // namespace gdp::gameplay
