#include "gameplay/Combat.hpp"

#include "gameplay/Props.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <string_view>
#include <vector>

namespace gdp::gameplay {
namespace {

struct RayInterval {
    float nearDistance{0.0F};
    float farDistance{0.0F};
};

std::optional<RayInterval> rayAabbInterval(
    const wkc::core::Vec3 origin,
    const wkc::core::Vec3 direction,
    const wkc::core::Aabb bounds,
    const float maximumDistance) noexcept
{
    float nearDistance = 0.0F;
    float farDistance = std::numeric_limits<float>::max();
    const float origins[3]{origin.x, origin.y, origin.z};
    const float directions[3]{direction.x, direction.y, direction.z};
    const float minima[3]{bounds.minimum.x, bounds.minimum.y, bounds.minimum.z};
    const float maxima[3]{bounds.maximum.x, bounds.maximum.y, bounds.maximum.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(directions[axis]) <= 1.0e-7F) {
            if (origins[axis] < minima[axis] || origins[axis] > maxima[axis]) {
                return std::nullopt;
            }
            continue;
        }
        const float inverseDirection = 1.0F / directions[axis];
        float axisNear = (minima[axis] - origins[axis]) * inverseDirection;
        float axisFar = (maxima[axis] - origins[axis]) * inverseDirection;
        if (axisNear > axisFar) {
            std::swap(axisNear, axisFar);
        }
        nearDistance = std::max(nearDistance, axisNear);
        farDistance = std::min(farDistance, axisFar);
        if (nearDistance > farDistance) {
            return std::nullopt;
        }
    }
    if (farDistance < 0.0F || nearDistance > maximumDistance) {
        return std::nullopt;
    }
    return RayInterval{std::max(nearDistance, 0.0F), farDistance};
}

wkc::core::Aabb enemyBounds(const gdp::entities::Entity& enemy) noexcept
{
    const wkc::core::Vec3 position = enemy.transform.position;
    return {
        {position.x - 0.38F, position.y, position.z - 0.30F},
        {position.x + 0.38F, position.y + 1.80F, position.z + 0.30F},
    };
}

wkc::core::Aabb enemyWeakPointBounds(
    const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy) noexcept
{
    const wkc::core::Vec3 position = entity.transform.position;
    if (enemy.archetype == gdp::entities::EnemyArchetype::IndustrialRobot) {
        return {
            {position.x - 0.20F, position.y + 1.30F, position.z - 0.22F},
            {position.x + 0.20F, position.y + 1.68F, position.z + 0.22F},
        };
    }
    return {
        {position.x - 0.22F, position.y + 1.42F, position.z - 0.24F},
        {position.x + 0.22F, position.y + 1.80F, position.z + 0.24F},
    };
}

// Use the accepted mesh-aligned hit profile by default. An explicit zero keeps
// the legacy geometry available for comparison and diagnosis.
bool referenceEnemyHitboxesEnabledImpl() noexcept
{
    const char* const value = std::getenv("GDP_ENEMY_REFERENCE_MODELS");
    return value == nullptr || std::string_view{value} != "0";
}

using EnemyZones = std::array<wkc::core::Aabb, 6>;

// Assembled-space bounds of parts 0..5 (torso, head, arms, legs), expanded by
// a few centimetres for movement. Parts 6+ are backpacks, weapons and tools.
const EnemyZones& referenceEnemyZones(
    const gdp::entities::EnemyArchetype archetype) noexcept
{
    using B = wkc::core::Aabb;
    static const EnemyZones worker{{
        B{{-.43F, .76F, -.20F}, {.43F, 1.47F, .33F}},
        B{{-.20F, 1.47F, -.13F}, {.20F, 1.86F, .25F}},
        B{{.30F, .44F, -.17F}, {.61F, 1.54F, .23F}},
        B{{-.61F, .72F, -.17F}, {-.34F, 1.54F, .23F}},
        B{{.06F, 0.F, -.24F}, {.41F, .98F, .37F}},
        B{{-.41F, 0.F, -.24F}, {-.06F, .98F, .37F}},
    }};
    static const EnemyZones guard{{
        B{{-.39F, .79F, -.17F}, {.39F, 1.46F, .31F}},
        B{{-.21F, 1.46F, -.13F}, {.21F, 1.81F, .24F}},
        B{{.18F, .79F, -.15F}, {.51F, 1.53F, .33F}},
        B{{-.51F, .79F, -.15F}, {-.18F, 1.53F, .33F}},
        B{{.03F, 0.F, -.17F}, {.38F, .99F, .33F}},
        B{{-.38F, 0.F, -.17F}, {-.03F, .99F, .33F}},
    }};
    static const EnemyZones officer{{
        B{{-.29F, .79F, -.17F}, {.29F, 1.49F, .27F}},
        B{{-.22F, 1.49F, -.14F}, {.22F, 1.82F, .25F}},
        B{{.24F, .66F, -.17F}, {.55F, 1.53F, .21F}},
        B{{-.55F, .66F, -.17F}, {-.24F, 1.53F, .21F}},
        B{{.02F, 0.F, -.18F}, {.36F, .99F, .33F}},
        B{{-.36F, 0.F, -.18F}, {-.02F, .99F, .33F}},
    }};
    static const EnemyZones engineer{{
        B{{-.34F, .80F, -.16F}, {.34F, 1.46F, .29F}},
        B{{-.23F, 1.46F, -.12F}, {.23F, 1.88F, .23F}},
        B{{.25F, .72F, -.15F}, {.52F, 1.54F, .21F}},
        B{{-.52F, .27F, -.15F}, {-.25F, 1.54F, .27F}},
        B{{.03F, 0.F, -.19F}, {.34F, 1.00F, .35F}},
        B{{-.34F, 0.F, -.19F}, {-.03F, 1.00F, .35F}},
    }};
    static const EnemyZones robot{{
        B{{-.49F, .94F, -.30F}, {.49F, 1.90F, .39F}},
        B{{-.26F, 1.90F, -.13F}, {.26F, 2.18F, .33F}},
        B{{.36F, .47F, -.21F}, {.90F, 1.99F, .36F}},
        B{{-.83F, 1.06F, -.21F}, {-.36F, 1.99F, .31F}},
        B{{.04F, 0.F, -.26F}, {.51F, 1.26F, .49F}},
        B{{-.51F, 0.F, -.26F}, {-.04F, 1.26F, .49F}},
    }};
    using gdp::entities::EnemyArchetype;
    switch (archetype) {
    case EnemyArchetype::Worker: return worker;
    case EnemyArchetype::Guard: return guard;
    case EnemyArchetype::Officer: return officer;
    case EnemyArchetype::Engineer: return engineer;
    case EnemyArchetype::IndustrialRobot: return robot;
    }
    return worker;
}

struct EnemyZoneHit {
    float distance{0.0F};
    HitZone zone{HitZone::Standard};
};

std::optional<EnemyZoneHit> referenceEnemyHit(
    const wkc::core::Vec3 origin, const wkc::core::Vec3 direction,
    const float maximumDistance, const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy) noexcept
{
    const wkc::core::Vec3 relative = origin - entity.transform.position;
    const float radians = entity.transform.rotationDegrees.y
        * (std::numbers::pi_v<float> / 180.0F);
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const auto toLocal = [cosine, sine](const wkc::core::Vec3 value) {
        return wkc::core::Vec3{
            cosine * value.x - sine * value.z,
            value.y,
            sine * value.x + cosine * value.z,
        };
    };
    const wkc::core::Vec3 localOrigin = toLocal(relative);
    const wkc::core::Vec3 localDirection = toLocal(direction);
    std::optional<EnemyZoneHit> nearest;
    const EnemyZones& zones = referenceEnemyZones(enemy.archetype);
    for (std::size_t part = 0; part < zones.size(); ++part) {
        const auto interval = rayAabbInterval(
            localOrigin, localDirection, zones[part], maximumDistance);
        if (interval && (!nearest || interval->nearDistance < nearest->distance)) {
            nearest = EnemyZoneHit{interval->nearDistance,
                part == 1U ? HitZone::WeakPoint : HitZone::Standard};
        }
    }
    return nearest;
}

float firstWorldDistance(
    const wkc::core::Vec3 origin,
    const wkc::core::Vec3 direction,
    const float maximumDistance,
    const CollisionWorld& collisionWorld) noexcept
{
    float nearest = maximumDistance + 1.0F;
    for (const wkc::core::Aabb solid : collisionWorld.solids()) {
        if (const auto interval = rayAabbInterval(origin, direction, solid, maximumDistance)) {
            nearest = std::min(nearest, interval->nearDistance);
        }
    }

    std::vector<RayInterval> walkableIntervals;
    walkableIntervals.reserve(collisionWorld.walkableVolumes().size());
    for (const wkc::core::Aabb volume : collisionWorld.walkableVolumes()) {
        if (const auto interval = rayAabbInterval(
                origin, direction, volume, maximumDistance)) {
            walkableIntervals.push_back(*interval);
        }
    }
    std::sort(walkableIntervals.begin(), walkableIntervals.end(),
        [](const RayInterval first, const RayInterval second) {
            return first.nearDistance < second.nearDistance;
        });
    float continuousFar = 0.0F;
    bool startsInsideWalkable = false;
    constexpr float intervalEpsilon = 1.0e-4F;
    for (const RayInterval interval : walkableIntervals) {
        if (interval.nearDistance > continuousFar + intervalEpsilon) {
            break;
        }
        startsInsideWalkable = true;
        continuousFar = std::max(continuousFar, interval.farDistance);
    }
    if (startsInsideWalkable) {
        nearest = std::min(nearest, continuousFar);
    }
    return nearest;
}

ImpactMaterial enemyImpactMaterial(
    const gdp::entities::EnemyData& enemy) noexcept
{
    return enemy.archetype == gdp::entities::EnemyArchetype::IndustrialRobot
        ? ImpactMaterial::Metal : ImpactMaterial::Flesh;
}

ImpactMaterial propImpactMaterial(
    const gdp::entities::PropData& prop) noexcept
{
    using gdp::entities::PropType;
    switch (prop.type) {
    case PropType::Crate:
    case PropType::Barricade:
    case PropType::Decoration:
        return ImpactMaterial::Composite;
    case PropType::Terminal:
    case PropType::Door:
    case PropType::Canister:
    case PropType::CeilingFixture:
        return ImpactMaterial::Metal;
    }
    return ImpactMaterial::Composite;
}

} // namespace

bool referenceEnemyHitboxesEnabled() noexcept
{
    return referenceEnemyHitboxesEnabledImpl();
}

bool referenceEnemyBodyContainsPoint(
    const wkc::core::Vec3 point,
    const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy,
    const float padding) noexcept
{
    const float radians = entity.transform.rotationDegrees.y
        * (std::numbers::pi_v<float> / 180.0F);
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const wkc::core::Vec3 relative = point - entity.transform.position;
    const wkc::core::Vec3 local{
        cosine * relative.x - sine * relative.z,
        relative.y,
        sine * relative.x + cosine * relative.z};
    const float margin = std::max(0.0F, padding);
    for (const wkc::core::Aabb& zone : referenceEnemyZones(enemy.archetype)) {
        if (local.x >= zone.minimum.x - margin
            && local.x <= zone.maximum.x + margin
            && local.y >= zone.minimum.y - margin
            && local.y <= zone.maximum.y + margin
            && local.z >= zone.minimum.z - margin
            && local.z <= zone.maximum.z + margin) {
            return true;
        }
    }
    return false;
}

wkc::core::Vec3 applyDeterministicWeaponSpread(
    wkc::core::Vec3 direction,
    const WeaponArchetype archetype,
    const std::uint32_t shotIndex,
    const float spreadScale,
    const float movementInaccuracyDegrees) noexcept
{
    direction = wkc::core::normalized(direction);
    const float movement = std::isfinite(movementInaccuracyDegrees)
        ? std::clamp(movementInaccuracyDegrees, 0.0F, 1.5F) : 0.0F;
    if (wkc::core::length(direction) <= 1.0e-7F
        || (shotIndex == 0U && movement <= 0.0F)) {
        return direction;
    }

    constexpr std::array<wkc::core::Vec2, 12> pattern{{
        {0.32F, -0.18F},
        {0.30F, 0.10F},
        {-0.22F, 0.36F},
        {0.45F, -0.20F},
        {-0.52F, -0.28F},
        {0.12F, 0.62F},
        {0.63F, 0.18F},
        {-0.38F, 0.58F},
        {-0.70F, 0.05F},
        {0.42F, -0.61F},
        {0.68F, 0.52F},
        {-0.58F, -0.62F},
    }};
    const wkc::core::Vec2 sample
        = pattern[std::min<std::size_t>(shotIndex, pattern.size() - 1U)];
    const float safeScale = std::isfinite(spreadScale)
        ? std::clamp(spreadScale, 0.0F, 1.0F) : 1.0F;
    const float spreadRadians = (weaponSpreadProfile(archetype).maximumDegrees
        * safeScale + movement)
        * (std::numbers::pi_v<float> / 180.0F);
    const float horizontal = std::tan(sample.x * spreadRadians);
    const float vertical = std::tan(sample.y * spreadRadians);

    const wkc::core::Vec3 referenceUp = std::abs(direction.y) < 0.999F
        ? wkc::core::Vec3{0.0F, 1.0F, 0.0F}
        : wkc::core::Vec3{0.0F, 0.0F, 1.0F};
    const wkc::core::Vec3 right
        = wkc::core::normalized(wkc::core::cross(direction, referenceUp));
    const wkc::core::Vec3 up
        = wkc::core::normalized(wkc::core::cross(right, direction));
    return wkc::core::normalized(
        direction + (right * horizontal) + (up * vertical));
}

float weaponMovementInaccuracyDegrees(
    const WeaponArchetype archetype, const wkc::core::Vec3 velocity,
    const bool grounded, const bool crouching, const float aimBlend) noexcept
{
    if (!isFirearmArchetype(archetype)
        || !std::isfinite(velocity.x) || !std::isfinite(velocity.z)) return 0.0F;
    const float speed = std::hypot(velocity.x, velocity.z);
    const float movement = 0.50F * std::clamp(speed / 4.5F, 0.0F, 1.6F)
        + (grounded ? 0.0F : 0.80F);
    const float safeAim = std::isfinite(aimBlend)
        ? std::clamp(aimBlend, 0.0F, 1.0F) : 0.0F;
    return std::clamp(movement * (1.0F - 0.55F * safeAim)
        * (crouching ? 0.80F : 1.0F), 0.0F, 1.5F);
}

DamageOutcome applyDamage(
    gdp::entities::EntityManager& manager,
    const gdp::entities::EntityId target,
    const float amount) noexcept
{
    gdp::entities::EnemyData* enemy = manager.findData<gdp::entities::EnemyData>(target);
    if (enemy == nullptr) {
        const PropDamageOutcome prop = damageProp(manager, target, amount).outcome;
        if (prop == PropDamageOutcome::Damaged) {
            return DamageOutcome::Damaged;
        }
        if (prop == PropDamageOutcome::Destroyed) {
            return DamageOutcome::Killed;
        }
        return DamageOutcome::Ignored;
    }
    if (!enemy->active
        || enemy->behavior == gdp::entities::EnemyBehaviorState::Dead
        || amount <= 0.0F) {
        return DamageOutcome::Ignored;
    }
    enemy->health = std::max(0.0F, enemy->health - amount);
    if (enemy->health > 0.0F) {
        enemy->hitReactionTime = enemyHitReactionDuration;
        return DamageOutcome::Damaged;
    }
    enemy->behavior = gdp::entities::EnemyBehaviorState::Dead;
    enemy->attackCooldown = 0.0F;
    enemy->supportCooldown = 0.0F;
    enemy->avoidanceTime = 0.0F;
    enemy->avoidanceSide = 0;
    enemy->hitReactionTime = 0.0F;
    enemy->lastKnownPlayerPosition = {};
    enemy->knowledgeSource = gdp::entities::EnemyKnowledgeSource::None;
    enemy->knowledgeAgeSeconds = 0.0F;
    enemy->directSightPreviousUpdate = false;
    enemy->investigationTime = 0.0F;
    enemy->alertTime = 0.0F;
    enemy->recoverTime = 0.0F;
    return DamageOutcome::Killed;
}

void updateEnemyHitReactions(
    gdp::entities::EntityManager& manager,
    const float fixedStepSeconds) noexcept
{
    const float step = std::max(fixedStepSeconds, 0.0F);
    for (const gdp::entities::EntityId id
        : manager.query(gdp::entities::EntityKind::Enemy)) {
        gdp::entities::EnemyData* enemy
            = manager.findData<gdp::entities::EnemyData>(id);
        if (enemy == nullptr) {
            continue;
        }
        enemy->hitReactionTime
            = std::max(0.0F, enemy->hitReactionTime - step);
    }
}

CombatSignalBatch combatSignalsForShot(const ShotResult& shot) noexcept
{
    CombatSignalBatch batch;
    if (shot.outcome == ShotOutcome::Cooldown
        || shot.outcome == ShotOutcome::Reloading
        || shot.outcome == ShotOutcome::Switching
        || shot.outcome == ShotOutcome::Empty) {
        return batch;
    }

    batch.signals[batch.count++] = {.kind = CombatSignalKind::WeaponFired,
        .outcome = shot.outcome, .impact = std::nullopt};
    const auto appendContact = [&](const ShotContact& contact) {
        if (contact.hit) {
            batch.signals[batch.count++] = {
                .kind = CombatSignalKind::EntityHit,
                .outcome = contact.outcome,
                .target = contact.hit->target,
                .zone = contact.hit->zone,
                .point = contact.hit->point,
                .impact = contact.impact,
            };
            if (contact.outcome == ShotOutcome::Hit
                || contact.outcome == ShotOutcome::Killed
                || contact.outcome == ShotOutcome::Destroyed) {
                batch.signals[batch.count++] = {
                    .kind = contact.outcome == ShotOutcome::Hit
                        ? CombatSignalKind::EntityDamaged
                        : CombatSignalKind::EntityKilled,
                    .outcome = contact.outcome,
                    .target = contact.hit->target,
                    .zone = contact.hit->zone,
                    .point = contact.hit->point,
                    .impact = std::nullopt,
                    .spawnedItem = contact.spawnedItem,
                };
            }
        } else if (contact.outcome == ShotOutcome::ImpactWorld) {
            batch.signals[batch.count++] = {
                .kind = CombatSignalKind::ImpactWorld,
                .outcome = contact.outcome,
                .point = contact.impact ? contact.impact->point : wkc::core::Vec3{},
                .impact = contact.impact,
            };
        }
    };
    appendContact({shot.outcome, shot.hit, shot.spawnedItem, shot.impact});
    for (std::size_t i = 0; i < std::min<std::size_t>(shot.extraContactCount, shot.extraContacts.size()); ++i)
        appendContact(shot.extraContacts[i]);
    return batch;
}

static ShotContact traceHitscan(
    const float range,
    const float baseDamage,
    const WeaponArchetype archetype,
    const wkc::core::Vec3 origin,
    wkc::core::Vec3 direction,
    const std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    gdp::entities::EntityManager& manager,
    const std::span<const std::uint32_t> additionalRoomIds) noexcept
{
    direction = wkc::core::normalized(direction);
    if (wkc::core::length(direction) <= 1.0e-7F || range <= 0.0F) {
        return {ShotOutcome::Miss, std::nullopt, {}, std::nullopt};
    }

    const float worldDistance = firstWorldDistance(origin, direction, range, collisionWorld);
    const auto targetRoomAllowed = [roomId, additionalRoomIds](const std::uint32_t targetRoom) {
        return targetRoom == roomId
            || std::find(additionalRoomIds.begin(), additionalRoomIds.end(), targetRoom)
                != additionalRoomIds.end();
    };
    std::optional<HitResult> nearestHit;
    for (const gdp::entities::EntityId id : manager.query(gdp::entities::EntityKind::Enemy)) {
        const gdp::entities::Entity* entity = manager.find(id);
        const gdp::entities::EnemyData* enemy = manager.findData<gdp::entities::EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || !targetRoomAllowed(entity->roomId)
            || !enemy->active
            || enemy->behavior == gdp::entities::EnemyBehaviorState::Dead) {
            continue;
        }
        float hitDistance = 0.0F;
        HitZone hitZone = HitZone::Standard;
        if (referenceEnemyHitboxesEnabled()) {
            const auto hit = referenceEnemyHit(
                origin, direction, range, *entity, *enemy);
            if (!hit) continue;
            hitDistance = hit->distance;
            hitZone = hit->zone;
        } else {
            const auto interval = rayAabbInterval(
                origin, direction, enemyBounds(*entity), range);
            if (!interval) continue;
            const auto weakPointInterval = rayAabbInterval(
                origin, direction, enemyWeakPointBounds(*entity, *enemy), range);
            hitDistance = weakPointInterval
                ? weakPointInterval->nearDistance : interval->nearDistance;
            hitZone = weakPointInterval ? HitZone::WeakPoint : HitZone::Standard;
        }
        if (hitDistance >= worldDistance
            || (nearestHit && hitDistance >= nearestHit->distance)) {
            continue;
        }
        nearestHit = HitResult{
            id,
            origin + (direction * hitDistance),
            hitDistance,
            hitZone,
        };
    }

    for (const gdp::entities::EntityId id : manager.query(gdp::entities::EntityKind::Prop)) {
        const gdp::entities::Entity* entity = manager.find(id);
        const gdp::entities::PropData* prop
            = manager.findData<gdp::entities::PropData>(id);
        const bool targetable = prop != nullptr
            && (prop->solid
                || prop->type == gdp::entities::PropType::CeilingFixture);
        if (entity == nullptr || prop == nullptr || !targetRoomAllowed(entity->roomId)
            || !targetable
            || prop->state == gdp::entities::PropState::Destroyed) {
            continue;
        }
        const auto interval = rayAabbInterval(
            origin, direction, propCollisionBounds(*entity, *prop), range);
        if (!interval || interval->nearDistance > worldDistance + 1.0e-4F
            || (nearestHit && interval->nearDistance >= nearestHit->distance)) {
            continue;
        }
        nearestHit = HitResult{
            id,
            origin + (direction * interval->nearDistance),
            interval->nearDistance,
        };
    }

    if (nearestHit) {
        const gdp::entities::Entity* target = manager.find(nearestHit->target);
        const bool targetIsProp = target != nullptr
            && target->kind == gdp::entities::EntityKind::Prop;
        ImpactMaterial material = ImpactMaterial::Flesh;
        if (targetIsProp) {
            const gdp::entities::PropData* prop
                = manager.findData<gdp::entities::PropData>(nearestHit->target);
            material = prop == nullptr
                ? ImpactMaterial::Composite : propImpactMaterial(*prop);
        } else {
            const gdp::entities::EnemyData* enemy
                = manager.findData<gdp::entities::EnemyData>(nearestHit->target);
            material = enemy == nullptr
                ? ImpactMaterial::Flesh : enemyImpactMaterial(*enemy);
        }
        const ImpactResult impact{
            nearestHit->point,
            direction * -1.0F,
            material,
        };
        const float damageAmount = baseDamage
            * (archetype == WeaponArchetype::Shotgun
                ? std::clamp(1.0F - nearestHit->distance / 20.0F, 0.25F, 1.0F) : 1.0F);
        if (targetIsProp) {
            const PropDamageResult damage
                = damageProp(manager, nearestHit->target, damageAmount);
            const ShotOutcome outcome = damage.outcome == PropDamageOutcome::Destroyed
                ? ShotOutcome::Destroyed
                : damage.outcome == PropDamageOutcome::Damaged ? ShotOutcome::Hit
                                                               : ShotOutcome::ImpactProp;
            return {outcome, nearestHit, damage.spawnedDrop, impact};
        }
        const DamageOutcome damage = applyDamage(
            manager, nearestHit->target,
            damageAmount
                * (nearestHit->zone == HitZone::WeakPoint
                        ? weakPointDamageMultiplier : 1.0F));
        return {
            damage == DamageOutcome::Killed ? ShotOutcome::Killed : ShotOutcome::Hit,
            nearestHit,
            {},
            impact,
        };
    }
    if (worldDistance <= range) {
        return {
            ShotOutcome::ImpactWorld,
            std::nullopt,
            {},
            ImpactResult{
                origin + (direction * worldDistance),
                direction * -1.0F,
                ImpactMaterial::Metal,
            },
        };
    }
    return {ShotOutcome::Miss, std::nullopt, {}, std::nullopt};
}

ShotResult fireHitscan(
    Weapon& weapon, const wkc::core::Vec3 origin, const wkc::core::Vec3 direction,
    const std::uint32_t roomId, const CollisionWorld& collisionWorld,
    gdp::entities::EntityManager& manager, const float spreadScale,
    const std::span<const std::uint32_t> additionalRoomIds,
    const float movementInaccuracyDegrees) noexcept
{
    const auto readiness = weapon.tryFire();
    if (readiness != FireReadiness::Fired) {
        const auto outcome = readiness == FireReadiness::Cooldown ? ShotOutcome::Cooldown
            : readiness == FireReadiness::Reloading ? ShotOutcome::Reloading
            : readiness == FireReadiness::Switching ? ShotOutcome::Switching : ShotOutcome::Empty;
        return {outcome, std::nullopt, {}, std::nullopt};
    }
    std::array<ShotContact, 6> contacts{};
    const auto count = weaponProjectileCount(weapon.definition().archetype);
    std::size_t representative = 0U;
    for (std::uint8_t i = 0U; i < count; ++i) {
        const auto index = count > 1U ? static_cast<std::uint32_t>(i + 1U) : weapon.spreadShotIndex();
        const auto ray = applyDeterministicWeaponSpread(direction,
            weapon.definition().archetype, index, spreadScale,
            movementInaccuracyDegrees);
        contacts[i] = traceHitscan(weapon.definition().range, weapon.damage(),
            weapon.definition().archetype, origin, ray, roomId, collisionWorld,
            manager, additionalRoomIds);
        // Keep the most significant contact in the legacy summary fields.
        if (contacts[i].outcome > contacts[representative].outcome) representative = i;
    }
    const auto& primary = contacts[representative];
    ShotResult result{primary.outcome, primary.hit, primary.spawnedItem, primary.impact};
    for (std::uint8_t i = 0U; i < count; ++i) if (i != representative)
        result.extraContacts[result.extraContactCount++] = contacts[i];
    return result;
}

ShotResult fireQuickMelee(
    Weapon& weapon, const wkc::core::Vec3 origin, const wkc::core::Vec3 direction,
    const std::uint32_t roomId, const CollisionWorld& collisionWorld,
    gdp::entities::EntityManager& manager,
    const std::span<const std::uint32_t> additionalRoomIds) noexcept
{
    const auto readiness = weapon.tryQuickMelee();
    if (readiness != FireReadiness::Fired) {
        const auto outcome = readiness == FireReadiness::Cooldown ? ShotOutcome::Cooldown
            : readiness == FireReadiness::Reloading ? ShotOutcome::Reloading
            : ShotOutcome::Switching;
        return {outcome, std::nullopt, {}, std::nullopt};
    }
    const ShotContact contact = traceHitscan(weapon.knifeRange(), weapon.knifeDamage(),
        WeaponArchetype::Knife, origin, direction, roomId, collisionWorld,
        manager, additionalRoomIds);
    return {contact.outcome, contact.hit, contact.spawnedItem, contact.impact};
}

bool hasLineOfSight(
    const wkc::core::Vec3 origin,
    const wkc::core::Vec3 target,
    const CollisionWorld& collisionWorld) noexcept
{
    const wkc::core::Vec3 offset = target - origin;
    const float distance = wkc::core::length(offset);
    if (distance <= 1.0e-7F) {
        return true;
    }
    return firstWorldDistance(
        origin,
        offset * (1.0F / distance),
        distance,
        collisionWorld) >= distance;
}

bool hasLineOfSightToProp(
    const wkc::core::Vec3 origin,
    const gdp::entities::EntityManager& manager,
    const gdp::entities::EntityId target,
    const CollisionWorld& collisionWorld) noexcept
{
    const gdp::entities::Entity* entity = manager.find(target);
    const gdp::entities::PropData* prop
        = manager.findData<gdp::entities::PropData>(target);
    if (entity == nullptr || prop == nullptr
        || prop->state == gdp::entities::PropState::Destroyed) {
        return false;
    }
    const wkc::core::Vec3 targetPoint = entity->transform.position
        + wkc::core::Vec3{0.0F, entity->transform.scale.y * 0.5F, 0.0F};
    const wkc::core::Vec3 offset = targetPoint - origin;
    const float distance = wkc::core::length(offset);
    if (distance <= 1.0e-7F) {
        return true;
    }
    const wkc::core::Vec3 direction = offset * (1.0F / distance);
    const auto targetInterval = rayAabbInterval(
        origin, direction, propCollisionBounds(*entity, *prop), distance);
    return targetInterval
        && firstWorldDistance(origin, direction, distance, collisionWorld)
            + 1.0e-4F >= targetInterval->nearDistance;
}

} // namespace gdp::gameplay
