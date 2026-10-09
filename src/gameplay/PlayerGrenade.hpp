#pragma once

#include "core/Math.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Player.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace gdp::gameplay {

inline constexpr float grenadeMaximumChargeSeconds = 1.2F;
inline constexpr float grenadeBlastRadius = 3.8F;

[[nodiscard]] float grenadeThrowSpeed(float chargeSeconds) noexcept;

struct PlayerGrenade {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 velocity{};
    float fuseSeconds{2.4F};
    std::uint32_t roomId{0U};
    CollisionWorld collision;
    std::vector<std::uint32_t> reachableRooms;
};

struct GrenadeDetonation {
    wkc::core::Vec3 position{};
    std::uint32_t roomId{0U};
    CollisionWorld collision;
    std::vector<std::uint32_t> reachableRooms;
};

struct GrenadeHit {
    entities::EntityId target{};
    bool killed{false};
    bool prop{false};
    entities::EntityId spawnedDrop{};
};

struct GrenadeBlastResult {
    std::vector<GrenadeHit> hits;
    bool playerDamaged{false};
};

class PlayerGrenadeSystem {
public:
    [[nodiscard]] bool throwGrenade(wkc::core::Vec3 origin,
        wkc::core::Vec3 direction, float chargeSeconds,
        std::uint32_t roomId, const CollisionWorld& collision,
        std::span<const std::uint32_t> reachableRooms = {});
    [[nodiscard]] std::vector<GrenadeDetonation> update(float step);
    void clear() noexcept { grenades_.clear(); }
    [[nodiscard]] std::span<const PlayerGrenade> grenades() const noexcept
    {
        return grenades_;
    }

private:
    std::vector<PlayerGrenade> grenades_;
};

[[nodiscard]] GrenadeBlastResult resolveGrenadeBlast(
    const GrenadeDetonation& blast, Player& player,
    entities::EntityManager& entities);

} // namespace gdp::gameplay
