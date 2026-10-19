#include "gameplay/PlayerGrenade.hpp"

#include "gameplay/Combat.hpp"
#include "gameplay/Props.hpp"

#include <algorithm>
#include <array>
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

std::optional<EnemyContact> sweptReferenceEnemyContact(
    const wkc::core::Vec3 start, const wkc::core::Vec3 end,
    const entities::Entity& entity,
    const entities::EnemyData& enemy) noexcept
{
    const wkc::core::Vec3 movement = end - start;
    const float distance = wkc::core::length(movement);
    if (distance <= 1.0e-6F
        || referenceEnemyBodyContainsPoint(start, entity, enemy, grenadeRadius)) {
        return std::nullopt;
    }
    const int samples = std::max(1, static_cast<int>(
        std::ceil(distance / 0.04F)));
    float previous = 0.0F;
    for (int sample = 1; sample <= samples; ++sample) {
        const float fraction = static_cast<float>(sample)
            / static_cast<float>(samples);
        if (referenceEnemyBodyContainsPoint(start + movement * fraction,
                entity, enemy, grenadeRadius)) {
            float outside = previous;
            float inside = fraction;
            for (int iteration = 0; iteration < 6; ++iteration) {
                const float midpoint = (outside + inside) * 0.5F;
                if (referenceEnemyBodyContainsPoint(start + movement * midpoint,
                        entity, enemy, grenadeRadius)) {
                    inside = midpoint;
                } else {
                    outside = midpoint;
                }
            }
            // A swept contact has a defined incoming direction. Reversing that
            // direction gives a stable approximate normal on segmented bodies.
            return EnemyContact{inside, movement * (-1.0F / distance)};
        }
        previous = fraction;
    }
    return std::nullopt;
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
        std::optional<EnemyContact> contact;
        if (referenceEnemyHitboxesEnabled()) {
            contact = sweptReferenceEnemyContact(start, end, *entity, *enemy);
        } else {
            const bool robot = enemy->archetype
                == entities::EnemyArchetype::IndustrialRobot;
            contact = sweptEnemyContact(start, end,
                entity->transform.position, robot ? 0.52F : 0.35F,
                robot ? 1.9F : 1.8F);
        }
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

float referenceEnemyBlastDamage(const wkc::core::Vec3 origin,
    const entities::Entity& entity, const entities::EnemyData& enemy,
    const CollisionWorld& collision) noexcept
{
    const bool robot = enemy.archetype
        == entities::EnemyArchetype::IndustrialRobot;
    const std::array<wkc::core::Vec3, 4> bodySamples{{
        {0.0F, robot ? 1.20F : 1.05F, 0.10F},
        {0.0F, robot ? 2.03F : 1.65F, 0.12F},
        {-0.18F, 0.45F, 0.05F},
        {0.18F, 0.45F, 0.05F},
    }};
    const float yaw = entity.transform.rotationDegrees.y
        * 0.01745329251994329577F;
    const float cosine = std::cos(yaw);
    const float sine = std::sin(yaw);
    float damage = 0.0F;
    for (const auto local : bodySamples) {
        const wkc::core::Vec3 point = entity.transform.position
            + wkc::core::Vec3{
                cosine * local.x + sine * local.z, local.y,
                -sine * local.x + cosine * local.z};
        if (referenceEnemyBodyContainsPoint(point, entity, enemy)
            && hasLineOfSight(origin, point, collision)) {
            damage = std::max(damage, blastDamage(origin, point, 100.0F));
        }
    }
    return damage;
}

} // namespace

bool GrenadeCookState::begin() noexcept
{
    if (phase_ != Phase::Idle) return false;
    phase_ = Phase::PinPull;
    return true;
}

void GrenadeCookState::reset() noexcept
{
    *this = GrenadeCookState{};
}

GrenadeCookOutcome GrenadeCookState::update(
    const float step, const bool fireHeld) noexcept
{
    if (!active() || !std::isfinite(step) || step <= 0.0F)
        return GrenadeCookOutcome::None;
    if (!fireHeld) releasePending_ = true;

    float cookingSeconds = step;
    if (phase_ == Phase::PinPull) {
        const float pinRemaining = grenadePinPullSeconds - pinSeconds_;
        const float pinStep = std::min(cookingSeconds, pinRemaining);
        pinSeconds_ += pinStep;
        cookingSeconds -= pinStep;
        if (pinSeconds_ < grenadePinPullSeconds) return GrenadeCookOutcome::None;
        pinSeconds_ = grenadePinPullSeconds;
        phase_ = Phase::Cooking;
    }

    // Input is observed at the start of this fixed step. A released grenade
    // leaves the hand as soon as its pin is out, before this step burns fuse.
    if (releasePending_) {
        phase_ = Phase::Spent;
        return GrenadeCookOutcome::Throw;
    }

    windupSeconds_ = std::min(grenadeMaximumChargeSeconds,
        windupSeconds_ + cookingSeconds);
    remainingFuseSeconds_ = std::max(0.0F,
        remainingFuseSeconds_ - cookingSeconds);
    if (remainingFuseSeconds_ <= 1.0e-5F) {
        remainingFuseSeconds_ = 0.0F;
        phase_ = Phase::Spent;
        return GrenadeCookOutcome::DetonateInHand;
    }
    return GrenadeCookOutcome::None;
}

float GrenadeCookState::pinProgress() const noexcept
{
    return pinSeconds_ / grenadePinPullSeconds;
}

float GrenadeCookState::windupProgress() const noexcept
{
    return windupSeconds_ / grenadeMaximumChargeSeconds;
}

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
    const std::span<const std::uint32_t> reachableRooms,
    const float remainingFuseSeconds)
{
    if (!std::isfinite(wkc::core::length(origin))
        || !std::isfinite(wkc::core::length(direction))
        || wkc::core::length(direction) <= 1.0e-5F || roomId == 0U
        || !std::isfinite(chargeSeconds)
        || !std::isfinite(remainingFuseSeconds)
        || remainingFuseSeconds <= 0.0F
        || remainingFuseSeconds > grenadeFuseSeconds) {
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
        remainingFuseSeconds, roomId, collision,
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
        // Physics limits a large step, but the fuse must still consume real time.
        grenade.fuseSeconds -= step;
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
        float damage = 0.0F;
        if (referenceEnemyHitboxesEnabled()) {
            damage = referenceEnemyBlastDamage(origin, *entity, *enemy,
                blast.collision);
        } else {
            const auto target = entity->transform.position
                + wkc::core::Vec3{0.0F, 0.9F, 0.0F};
            damage = blastDamage(origin, target, 100.0F);
            if (!hasLineOfSight(origin, target, blast.collision)) continue;
        }
        if (damage <= 0.0F) continue;
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
