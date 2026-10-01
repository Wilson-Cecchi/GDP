#include "game/HudBoss.hpp"

#include "gameplay/Enemy.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::game {

visual::HudBossData buildBossHud(
    const entities::EntityManager& entities,
    const gameplay::SessionState& session) noexcept
{
    if (session.phase != gameplay::SessionPhase::Playing || !session.bossEngaged) {
        return {};
    }
    // Search globally: the engaged boss can pursue the player through an open
    // corridor and change its logical room without losing its encounter HUD.
    for (const entities::EntityId id : entities.query(entities::EntityKind::Enemy)) {
        const entities::EnemyData* enemy = entities.findData<entities::EnemyData>(id);
        if (enemy == nullptr || !enemy->active
            || enemy->archetype != entities::EnemyArchetype::IndustrialRobot
            || enemy->behavior == entities::EnemyBehaviorState::Dead
            || !std::isfinite(enemy->health) || enemy->health <= 0.0F
            || !std::isfinite(enemy->maximumHealth) || enemy->maximumHealth <= 0.0F) {
            continue;
        }
        visual::HudBossData hud{
            true, std::min(enemy->health, enemy->maximumHealth), enemy->maximumHealth};
        switch (gameplay::industrialRobotPhase(hud.health, hud.maximumHealth)) {
        case gameplay::IndustrialRobotPhase::LongRange:
            hud.phase = visual::HudBossPhase::LongRange;
            break;
        case gameplay::IndustrialRobotPhase::Pressure:
            hud.phase = visual::HudBossPhase::Pressure;
            break;
        case gameplay::IndustrialRobotPhase::Pursuit:
            hud.phase = visual::HudBossPhase::Pursuit;
            break;
        }
        return hud;
    }
    return {};
}

} // namespace gdp::game
