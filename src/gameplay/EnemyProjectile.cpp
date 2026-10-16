#include "gameplay/EnemyProjectile.hpp"

#include "gameplay/Combat.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::gameplay {
namespace {

bool containsPoint(const wkc::core::Aabb box,
    const wkc::core::Vec3 point) noexcept
{
    return point.x >= box.minimum.x && point.x <= box.maximum.x
        && point.y >= box.minimum.y && point.y <= box.maximum.y
        && point.z >= box.minimum.z && point.z <= box.maximum.z;
}

bool projectileCanOccupy(const wkc::core::Vec3 point,
    const CollisionWorld& world) noexcept
{
    const bool walkable = std::any_of(world.walkableVolumes().begin(),
        world.walkableVolumes().end(), [point](const auto volume) {
            return containsPoint(volume, point);
        });
    return walkable && std::none_of(world.solids().begin(),
        world.solids().end(), [point](const auto solid) {
            return containsPoint(solid, point);
        });
}

bool projectileHitsPlayer(
    const wkc::core::Vec3 point, const Player& player) noexcept
{
    const wkc::core::Vec3 feet = player.state().position;
    const float horizontal = std::hypot(point.x - feet.x, point.z - feet.z);
    const float height = player.collider().height;
    return horizontal <= player.collider().radius + 0.10F
        && point.y >= feet.y + 0.10F
        && point.y <= feet.y + height;
}

bool projectileHitsEnemy(const wkc::core::Vec3 point,
    const entities::Entity& entity,
    const entities::EnemyData& enemy) noexcept
{
    const float radius = enemy.archetype == entities::EnemyArchetype::IndustrialRobot
        ? 0.52F : 0.35F;
    const float height = enemy.archetype == entities::EnemyArchetype::IndustrialRobot
        ? 1.9F : 1.8F;
    const wkc::core::Vec3 feet = entity.transform.position;
    return std::hypot(point.x - feet.x, point.z - feet.z) <= radius + 0.10F
        && point.y >= feet.y + 0.10F && point.y <= feet.y + height;
}

bool allyOnSegment(const wkc::core::Vec3 origin,
    const wkc::core::Vec3 direction, const float distance,
    const entities::Entity& entity,
    const entities::EnemyData& enemy, const float clearance) noexcept
{
    const wkc::core::Vec3 center = entity.transform.position
        + wkc::core::Vec3{0.0F,
            enemy.archetype == entities::EnemyArchetype::IndustrialRobot
                ? 1.0F : 0.9F, 0.0F};
    const float along = wkc::core::dot(center - origin, direction);
    if (along <= 0.0F || along >= distance) {
        return false;
    }
    // Deliberately conservative: this is only a prediction, not collision.
    const float radius = enemy.archetype == entities::EnemyArchetype::IndustrialRobot
        ? 0.62F : 0.45F;
    const wkc::core::Vec3 nearest = origin + direction * along;
    return std::hypot(nearest.x - center.x, nearest.z - center.z)
            < radius + clearance
        && nearest.y >= entity.transform.position.y + 0.10F
        && nearest.y <= entity.transform.position.y + 1.9F;
}

} // namespace

bool allyInEnemyFireLane(const entities::EntityManager& enemies,
    const entities::EntityId shooter, const wkc::core::Vec3 origin,
    const wkc::core::Vec3 direction, const float distance) noexcept
{
    if (!std::isfinite(distance) || distance <= 0.0F
        || !std::isfinite(wkc::core::length(direction))
        || wkc::core::length(direction) <= 1.0e-6F) {
        return false;
    }
    const wkc::core::Vec3 ray = wkc::core::normalized(direction);
    const entities::EnemyData* shooterData
        = enemies.findData<entities::EnemyData>(shooter);
    // The robot's cannon needs room around helpers' visible arms and gear.
    // Keep the existing lane for the other ranged enemies.
    const float clearance = shooterData != nullptr
            && shooterData->archetype == entities::EnemyArchetype::IndustrialRobot
        ? 0.12F : 0.0F;
    for (const entities::EntityId id : enemies.query(entities::EntityKind::Enemy)) {
        if (id == shooter) {
            continue;
        }
        const entities::Entity* entity = enemies.find(id);
        const entities::EnemyData* enemy = enemies.findData<entities::EnemyData>(id);
        if (entity != nullptr && enemy != nullptr && enemy->active
            && enemy->behavior != entities::EnemyBehaviorState::Dead
            && allyOnSegment(origin, ray, distance, *entity, *enemy,
                clearance)) {
            return true;
        }
    }
    return false;
}

void EnemyProjectileSystem::spawn(const std::span<const EnemyProjectile> shots)
{
    for (const EnemyProjectile& shot : shots) {
        if (shot.damage > 0.0F && shot.remainingSeconds > 0.0F
            && std::isfinite(shot.damage)
            && std::isfinite(wkc::core::length(shot.velocity))
            && wkc::core::length(shot.velocity) > 0.0F) {
            projectiles_.push_back(shot);
        }
    }
}

std::vector<EnemyProjectileEvent> EnemyProjectileSystem::update(
    const float step, Player& player, const CollisionWorld& world,
    entities::EntityManager& enemies)
{
    std::vector<EnemyProjectileEvent> events;
    if (!std::isfinite(step) || step <= 0.0F) {
        return events;
    }
    std::vector<EnemyProjectile> remaining;
    remaining.reserve(projectiles_.size());
    const std::vector<entities::EntityId> enemyIds
        = enemies.query(entities::EntityKind::Enemy);
    for (EnemyProjectile shot : projectiles_) {
        const wkc::core::Vec3 displacement = shot.velocity * step;
        const float distance = wkc::core::length(displacement);
        const int samples = std::max(1, static_cast<int>(
            std::ceil(distance / 0.06F)));
        bool finished = false;
        for (int sample = 1; sample <= samples; ++sample) {
            const wkc::core::Vec3 next = shot.position
                + displacement * (1.0F / static_cast<float>(samples));
            if (!projectileCanOccupy(next, world)) {
                events.push_back({shot.source, shot.neural, false, true, false, 0.0F});
                finished = true;
                break;
            }
            shot.position = next;
            shot.traveledDistance += distance / static_cast<float>(samples);
            for (const entities::EntityId id : enemyIds) {
                if (id == shot.source) {
                    continue;
                }
                const entities::Entity* entity = enemies.find(id);
                const entities::EnemyData* enemy
                    = enemies.findData<entities::EnemyData>(id);
                if (entity == nullptr || enemy == nullptr || !enemy->active
                    || enemy->behavior == entities::EnemyBehaviorState::Dead
                    || !projectileHitsEnemy(next, *entity, *enemy)) {
                    continue;
                }
                const float healthBefore = enemy->health;
                const DamageOutcome outcome = applyDamage(enemies, id, shot.damage);
                events.push_back({shot.source, shot.neural, false, false, false,
                    healthBefore - enemy->health, id,
                    outcome == DamageOutcome::Killed});
                finished = true;
                break;
            }
            if (finished) {
                break;
            }
            if (player.state().alive && projectileHitsPlayer(next, player)) {
                const float healthAndArmorBefore
                    = player.state().health + player.state().armor;
                player.applyDamage(shot.damage);
                events.push_back({shot.source, shot.neural, true, false, false,
                    healthAndArmorBefore
                        - player.state().health - player.state().armor});
                finished = true;
                break;
            }
            if (shot.targetDistance > 0.0F
                && shot.traveledDistance >= shot.targetDistance + 0.5F) {
                events.push_back({shot.source, shot.neural, false, false,
                    shot.threatenedPlayerAtLaunch, 0.0F});
                finished = true;
                break;
            }
        }
        shot.remainingSeconds -= step;
        if (!finished && shot.remainingSeconds <= 0.0F) {
            events.push_back({shot.source, shot.neural, false, false,
                shot.threatenedPlayerAtLaunch, 0.0F});
            finished = true;
        }
        if (!finished) {
            remaining.push_back(shot);
        }
    }
    projectiles_ = std::move(remaining);
    return events;
}

} // namespace gdp::gameplay
