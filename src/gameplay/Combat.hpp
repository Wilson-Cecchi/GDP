#pragma once

#include "core/Math.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Weapon.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace gdp::gameplay {

inline constexpr float enemyHitReactionDuration = 0.12F;
inline constexpr float weakPointDamageMultiplier = 1.75F;

enum class DamageOutcome : std::uint8_t {
    Ignored,
    Damaged,
    Killed,
};

enum class ShotOutcome : std::uint8_t {
    Cooldown,
    Reloading,
    Switching,
    Empty,
    Miss,
    ImpactWorld,
    ImpactProp,
    Hit,
    Killed,
    Destroyed,
};

enum class HitZone : std::uint8_t {
    Standard,
    WeakPoint,
};

struct HitResult {
    gdp::entities::EntityId target{};
    wkc::core::Vec3 point{};
    float distance{0.0F};
    HitZone zone{HitZone::Standard};
};

enum class ImpactMaterial : std::uint8_t {
    Metal,
    Composite,
    Flesh,
};

struct ImpactResult {
    wkc::core::Vec3 point{};
    wkc::core::Vec3 normal{};
    ImpactMaterial material{ImpactMaterial::Metal};
};

struct ShotContact {
    ShotOutcome outcome{ShotOutcome::Miss};
    std::optional<HitResult> hit;
    gdp::entities::EntityId spawnedItem{};
    std::optional<ImpactResult> impact;
};

struct ShotResult {
    ShotOutcome outcome{ShotOutcome::Miss};
    std::optional<HitResult> hit;
    gdp::entities::EntityId spawnedItem{};
    std::optional<ImpactResult> impact;
    std::array<ShotContact, 5> extraContacts{};
    std::uint8_t extraContactCount{0U};
};

enum class CombatSignalKind : std::uint8_t {
    WeaponFired,
    EntityHit,
    EntityDamaged,
    EntityKilled,
    ImpactWorld,
};

struct CombatSignal {
    CombatSignalKind kind{CombatSignalKind::WeaponFired};
    ShotOutcome outcome{ShotOutcome::Miss};
    gdp::entities::EntityId target{};
    HitZone zone{HitZone::Standard};
    wkc::core::Vec3 point{};
    std::optional<ImpactResult> impact;
    gdp::entities::EntityId spawnedItem{};
};

// One firing event, followed by contact/damage for at most six shotgun pellets.
struct CombatSignalBatch {
    std::array<CombatSignal, 13> signals{};
    std::uint8_t count{0};
};

[[nodiscard]] CombatSignalBatch combatSignalsForShot(
    const ShotResult& shot) noexcept;

[[nodiscard]] DamageOutcome applyDamage(
    gdp::entities::EntityManager& manager,
    gdp::entities::EntityId target,
    float amount) noexcept;

void updateEnemyHitReactions(
    gdp::entities::EntityManager& manager,
    float fixedStepSeconds) noexcept;

[[nodiscard]] wkc::core::Vec3 applyDeterministicWeaponSpread(
    wkc::core::Vec3 direction,
    WeaponArchetype archetype,
    std::uint32_t shotIndex,
    float spreadScale = 1.0F,
    float movementInaccuracyDegrees = 0.0F) noexcept;

[[nodiscard]] float weaponMovementInaccuracyDegrees(
    WeaponArchetype archetype, wkc::core::Vec3 velocity,
    bool grounded, bool crouching, float aimBlend) noexcept;

[[nodiscard]] ShotResult fireHitscan(
    Weapon& weapon,
    wkc::core::Vec3 origin,
    wkc::core::Vec3 direction,
    std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    gdp::entities::EntityManager& manager,
    float spreadScale = 1.0F,
    std::span<const std::uint32_t> additionalRoomIds = {},
    float movementInaccuracyDegrees = 0.0F) noexcept;

[[nodiscard]] bool hasLineOfSight(
    wkc::core::Vec3 origin,
    wkc::core::Vec3 target,
    const CollisionWorld& collisionWorld) noexcept;

[[nodiscard]] bool hasLineOfSightToProp(
    wkc::core::Vec3 origin,
    const gdp::entities::EntityManager& manager,
    gdp::entities::EntityId target,
    const CollisionWorld& collisionWorld) noexcept;

} // namespace gdp::gameplay
