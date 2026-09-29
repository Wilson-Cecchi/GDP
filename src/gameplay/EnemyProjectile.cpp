#include "gameplay/EnemyProjectile.hpp"

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

} // namespace

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
    const float step, Player& player, const CollisionWorld& world)
{
    std::vector<EnemyProjectileEvent> events;
    if (!std::isfinite(step) || step <= 0.0F) {
        return events;
    }
    std::vector<EnemyProjectile> remaining;
    remaining.reserve(projectiles_.size());
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
