#include "gameplay/PlayerGrenade.hpp"

#include "gameplay/Combat.hpp"
#include "gameplay/Props.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace gdp::gameplay {
namespace {

constexpr CharacterCollider grenadeCollider{0.12F, 0.24F};
constexpr float grenadeRadius = 0.12F;

bool allowedRoom(const GrenadeDetonation& blast, const std::uint32_t roomId)
{
    return roomId == blast.roomId
        || std::find(blast.reachableRooms.begin(), blast.reachableRooms.end(), roomId)
            != blast.reachableRooms.end();
}

bool allowedRoom(const PlayerGrenade& grenade, const std::uint32_t roomId)
{
    return roomId == grenade.roomId
        || std::find(grenade.reachableRooms.begin(), grenade.reachableRooms.end(), roomId)
            != grenade.reachableRooms.end();
}

struct EnemyContact {
    float fraction{1.0F};
    wkc::core::Vec3 normal{};
};

// Sweep the grenade center against an enemy body expanded by the grenade radius.
// A segment test matters at full charge: one fixed step can travel farther than
// an enemy's entire width.
std::optional<EnemyContact> sweptEnemyContact(
    const wkc::core::Vec3 start, const wkc::core::Vec3 end,
    const wkc::core::Vec3 feet, const float bodyRadius,
    const float bodyHeight) noexcept
{
    const wkc::core::Vec3 minimum{feet.x - bodyRadius - grenadeRadius,
        feet.y - grenadeRadius, feet.z - bodyRadius - grenadeRadius};
    const wkc::core::Vec3 maximum{feet.x + bodyRadius + grenadeRadius,
        feet.y + bodyHeight + grenadeRadius,
        feet.z + bodyRadius + grenadeRadius};
    const wkc::core::Vec3 movement = end - start;
    const float origins[]{start.x, start.y, start.z};
    const float directions[]{movement.x, movement.y, movement.z};
    const float minima[]{minimum.x, minimum.y, minimum.z};
    const float maxima[]{maximum.x, maximum.y, maximum.z};
    float entry = 0.0F;
    float exit = 1.0F;
    wkc::core::Vec3 normal{};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(directions[axis]) <= 1.0e-7F) {
            if (origins[axis] < minima[axis] || origins[axis] > maxima[axis])
                return std::nullopt;
            continue;
        }
        float near = (minima[axis] - origins[axis]) / directions[axis];
        float far = (maxima[axis] - origins[axis]) / directions[axis];
        wkc::core::Vec3 axisNormal{};
        if (axis == 0) axisNormal.x = -1.0F;
        if (axis == 1) axisNormal.y = -1.0F;
        if (axis == 2) axisNormal.z = -1.0F;
        if (near > far) {
            std::swap(near, far);
            axisNormal = axisNormal * -1.0F;
        }
        if (near > entry) {
            entry = near;
            normal = axisNormal;
        }
        exit = std::min(exit, far);
        if (entry > exit) return std::nullopt;
    }
    // Starting inside the expanded body has no well-defined entry face; do not
    // repeatedly reverse a grenade that is already separating from an enemy.
    if (wkc::core::length(normal) == 0.0F || entry > 1.0F)
        return std::nullopt;
    return EnemyContact{entry, normal};
}

std::optional<EnemyContact> firstEnemyContact(
    const PlayerGrenade& grenade, const wkc::core::Vec3 start,
    const wkc::core::Vec3 end, const entities::EntityManager& entities)
{
    std::optional<EnemyContact> nearest;
    for (const auto id : entities.query(entities::EntityKind::Enemy)) {
        const auto* entity = entities.find(id);
        const auto* enemy = entities.findData<entities::EnemyData>(id);
        if (!entity || !enemy || !enemy->active || enemy->health <= 0.0F
            || enemy->behavior == entities::EnemyBehaviorState::Dead
            || !allowedRoom(grenade, entity->roomId)) continue;
        const bool robot = enemy->archetype
            == entities::EnemyArchetype::IndustrialRobot;
        const auto contact = sweptEnemyContact(start, end,
            entity->transform.position, robot ? 0.52F : 0.35F,
            robot ? 1.9F : 1.8F);
        if (contact && (!nearest || contact->fraction < nearest->fraction))
            nearest = contact;
    }
    return nearest;
}

float blastDamage(const wkc::core::Vec3 origin,
    const wkc::core::Vec3 target, const float maximum) noexcept
{
    const float distance = wkc::core::length(target - origin);
    return maximum * std::clamp(1.0F - distance / grenadeBlastRadius, 0.0F, 1.0F);
}

} // namespace

float grenadeThrowSpeed(const float chargeSeconds) noexcept
{
    const float charge = std::isfinite(chargeSeconds)
        ? std::clamp(chargeSeconds / grenadeMaximumChargeSeconds, 0.0F, 1.0F)
        : 0.0F;
    return 8.0F + charge * 16.0F;
}

bool PlayerGrenadeSystem::throwGrenade(
    const wkc::core::Vec3 origin, const wkc::core::Vec3 direction,
    const float chargeSeconds, const std::uint32_t roomId,
    const CollisionWorld& collision,
    const std::span<const std::uint32_t> reachableRooms)
{
    if (!std::isfinite(wkc::core::length(origin))
        || !std::isfinite(wkc::core::length(direction))
        || wkc::core::length(direction) <= 1.0e-5F || roomId == 0U) {
        return false;
    }
    const wkc::core::Vec3 forward = wkc::core::normalized(direction);
    wkc::core::Vec3 position = origin + forward * 0.25F;
    if (!collision.canOccupy(position - wkc::core::Vec3{0.0F, grenadeRadius, 0.0F},
            grenadeCollider)) {
        position = origin;
        if (!collision.canOccupy(position - wkc::core::Vec3{0.0F, grenadeRadius, 0.0F},
                grenadeCollider)) return false;
    }
    grenades_.push_back({position,
        forward * grenadeThrowSpeed(chargeSeconds)
            + wkc::core::Vec3{0.0F, 2.5F, 0.0F},
        2.4F, roomId, collision,
        {reachableRooms.begin(), reachableRooms.end()}});
    return true;
}

std::vector<GrenadeDetonation> PlayerGrenadeSystem::update(
    const float step, const entities::EntityManager& entities)
{
    std::vector<GrenadeDetonation> detonations;
    if (!std::isfinite(step) || step <= 0.0F) return detonations;
    const float seconds = std::min(step, 0.05F);
    for (PlayerGrenade& grenade : grenades_) {
        const wkc::core::Vec3 start = grenade.position;
        grenade.velocity.y -= 15.0F * seconds;
        const auto oldVelocity = grenade.velocity;
        const auto motion = grenade.collision.moveCharacter(
            grenade.position - wkc::core::Vec3{0.0F, grenadeRadius, 0.0F},
            grenade.velocity, seconds, grenadeCollider);
        grenade.position = motion.position + wkc::core::Vec3{0.0F, grenadeRadius, 0.0F};
        if (motion.velocity.x == 0.0F && oldVelocity.x != 0.0F)
            grenade.velocity.x = -oldVelocity.x * 0.35F;
        if (motion.velocity.z == 0.0F && oldVelocity.z != 0.0F)
            grenade.velocity.z = -oldVelocity.z * 0.35F;
        if (motion.velocity.y == 0.0F && oldVelocity.y != 0.0F)
            grenade.velocity.y = motion.grounded && std::abs(oldVelocity.y) < 1.0F
                ? 0.0F : -oldVelocity.y * 0.25F;
        if (const auto contact = firstEnemyContact(
                grenade, start, grenade.position, entities)) {
            grenade.position = start + (grenade.position - start)
                * std::max(0.0F, contact->fraction - 0.001F);
            const float incoming = wkc::core::dot(grenade.velocity, contact->normal);
            if (incoming < 0.0F)
                grenade.velocity = grenade.velocity
                    - contact->normal * (incoming * 1.35F);
        }
        grenade.velocity.x *= std::max(0.0F, 1.0F - 0.7F * seconds);
        grenade.velocity.z *= std::max(0.0F, 1.0F - 0.7F * seconds);
        grenade.fuseSeconds -= seconds;
        if (grenade.fuseSeconds <= 0.0F) {
            detonations.push_back({grenade.position, grenade.roomId,
                grenade.collision, grenade.reachableRooms});
        }
    }
    std::erase_if(grenades_, [](const PlayerGrenade& grenade) {
        return grenade.fuseSeconds <= 0.0F;
    });
    return detonations;
}

GrenadeBlastResult resolveGrenadeBlast(
    const GrenadeDetonation& blast, Player& player,
    entities::EntityManager& entities)
{
    GrenadeBlastResult result;
    const auto origin = blast.position + wkc::core::Vec3{0.0F, 0.10F, 0.0F};
    for (const auto id : entities.query(entities::EntityKind::Enemy)) {
        const auto* entity = entities.find(id);
        const auto* enemy = entities.findData<entities::EnemyData>(id);
        if (!entity || !enemy || !enemy->active
            || enemy->behavior == entities::EnemyBehaviorState::Dead
            || !allowedRoom(blast, entity->roomId)) continue;
        const auto target = entity->transform.position
            + wkc::core::Vec3{0.0F, 0.9F, 0.0F};
        const float damage = blastDamage(origin, target, 100.0F);
        if (damage <= 0.0F || !hasLineOfSight(origin, target, blast.collision)) continue;
        const auto outcome = applyDamage(entities, id, damage);
        if (outcome != DamageOutcome::Ignored)
            result.hits.push_back({id, outcome == DamageOutcome::Killed, false, {}});
    }
    for (const auto id : entities.query(entities::EntityKind::Prop)) {
        const auto* entity = entities.find(id);
        const auto* prop = entities.findData<entities::PropData>(id);
        if (!entity || !prop || !allowedRoom(blast, entity->roomId)
            || prop->state == entities::PropState::Destroyed) continue;
        const auto bounds = propCollisionBounds(*entity, *prop);
        const auto target = (bounds.minimum + bounds.maximum) * 0.5F;
        const float damage = blastDamage(origin, target, 100.0F);
        if (damage <= 0.0F || !hasLineOfSightToProp(origin, entities, id, blast.collision)) continue;
        const auto outcome = damageProp(entities, id, damage);
        if (outcome.outcome != PropDamageOutcome::Ignored) {
            result.hits.push_back({id,
                outcome.outcome == PropDamageOutcome::Destroyed, true,
                outcome.spawnedDrop});
        }
    }
    if (player.state().alive) {
        const auto target = player.state().position
            + wkc::core::Vec3{0.0F, player.collider().height * 0.5F, 0.0F};
        const float damage = blastDamage(origin, target, 70.0F);
        if (damage > 0.0F && hasLineOfSight(origin, target, blast.collision)) {
            player.applyDamage(damage);
            result.playerDamaged = true;
        }
    }
    return result;
}

} // namespace gdp::gameplay
