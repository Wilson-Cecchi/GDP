#include "gameplay/PlayerGrenade.hpp"

#include "gameplay/Combat.hpp"
#include "gameplay/Props.hpp"

#include <algorithm>
#include <cmath>

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

std::vector<GrenadeDetonation> PlayerGrenadeSystem::update(const float step)
{
    std::vector<GrenadeDetonation> detonations;
    if (!std::isfinite(step) || step <= 0.0F) return detonations;
    const float seconds = std::min(step, 0.05F);
    for (PlayerGrenade& grenade : grenades_) {
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
