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
inline constexpr float grenadePinPullSeconds = 0.32F;
inline constexpr float grenadeFuseSeconds = 4.87F;
inline constexpr float grenadeBlastRadius = 3.8F;

[[nodiscard]] float grenadeThrowSpeed(float chargeSeconds) noexcept;

enum class GrenadeCookOutcome : std::uint8_t {
    None,
    Throw,
    DetonateInHand,
};

class GrenadeCookState {
public:
    [[nodiscard]] bool begin() noexcept;
    void reset() noexcept;
    [[nodiscard]] GrenadeCookOutcome update(float step, bool fireHeld) noexcept;
    [[nodiscard]] bool active() const noexcept { return phase_ == Phase::PinPull || phase_ == Phase::Cooking; }
    [[nodiscard]] float pinProgress() const noexcept;
    [[nodiscard]] float windupProgress() const noexcept;
    [[nodiscard]] float chargeSeconds() const noexcept { return windupSeconds_; }
    [[nodiscard]] float remainingFuseSeconds() const noexcept { return remainingFuseSeconds_; }

private:
    enum class Phase : std::uint8_t { Idle, PinPull, Cooking, Spent };
    Phase phase_{Phase::Idle};
    float pinSeconds_{0.0F};
    float windupSeconds_{0.0F};
    float remainingFuseSeconds_{grenadeFuseSeconds};
    bool releasePending_{false};
};

struct PlayerGrenade {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 velocity{};
    float fuseSeconds{grenadeFuseSeconds};
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
        std::span<const std::uint32_t> reachableRooms = {},
        float remainingFuseSeconds = grenadeFuseSeconds);
    [[nodiscard]] std::vector<GrenadeDetonation> update(
        float step, const entities::EntityManager& entities);
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
