#include "game/HudBoss.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void expect(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    using namespace gdp;
    entities::EntityManager manager;
    const entities::EntityId guard = manager.createEnemy({
        .roomId = 8U,
        .archetype = entities::EnemyArchetype::Guard,
    });
    const entities::EntityId boss = manager.createEnemy({
        .roomId = 8U,
        .archetype = entities::EnemyArchetype::IndustrialRobot,
        .health = 200.0F,
    });
    gameplay::SessionSystem session{12345U};
    expect(!game::buildBossHud(manager, session.state()).visible,
        "an undiscovered boss does not reveal its health");
    (void)session.update(true, world::RoomRole::Normal, true, true, true);
    const game::visual::HudBossData initial = game::buildBossHud(manager, session.state());
    expect(initial.visible && initial.health == 200.0F
            && initial.maximumHealth == 200.0F
            && initial.phase == game::visual::HudBossPhase::LongRange,
        "opening the boss corridor reveals the live boss instead of a guard");

    entities::EnemyData* const enemy = manager.findData<entities::EnemyData>(boss);
    enemy->health = enemy->maximumHealth * 0.66F;
    expect(game::buildBossHud(manager, session.state()).phase
            == game::visual::HudBossPhase::Pressure,
        "the HUD uses the real second-phase threshold");
    enemy->health = enemy->maximumHealth * 0.33F;
    manager.find(boss)->roomId = 3U;
    expect(game::buildBossHud(manager, session.state()).phase
            == game::visual::HudBossPhase::Pursuit,
        "third phase stays visible when the boss crosses to another room");
    enemy->health = 12.0F;
    expect(game::buildBossHud(manager, session.state()).health == 12.0F
            && enemy->maximumHealth == 200.0F
            && manager.findData<entities::EnemyData>(guard)->health > 0.0F,
        "the snapshot follows damage without mutating combat data");
    enemy->maximumHealth = 240.0F;
    enemy->health = 120.0F;
    const auto scaled = game::buildBossHud(manager, session.state());
    expect(scaled.health == 120.0F && scaled.maximumHealth == 240.0F
            && scaled.phase == game::visual::HudBossPhase::Pressure,
        "Campaign-scaled health uses the instance capacity for phases and HP");
    enemy->maximumHealth = std::numeric_limits<float>::infinity();
    expect(!game::buildBossHud(manager, session.state()).visible,
        "invalid maximum health cannot reach the HUD");
    enemy->maximumHealth = 200.0F;
    enemy->active = false;
    expect(!game::buildBossHud(manager, session.state()).visible,
        "inactive staged robots cannot supply the encounter HUD");
    enemy->active = true;
    enemy->health = std::numeric_limits<float>::quiet_NaN();
    expect(!game::buildBossHud(manager, session.state()).visible,
        "invalid health cannot reach the HUD");
    enemy->health = 220.0F;
    expect(game::buildBossHud(manager, session.state()).health == 200.0F,
        "presentation clamps health above capacity");
    enemy->health = 0.0F;
    expect(!game::buildBossHud(manager, session.state()).visible,
        "zero-health bosses do not leave a stale meter");
    enemy->health = 12.0F;
    enemy->behavior = entities::EnemyBehaviorState::Dead;
    expect(!game::buildBossHud(manager, session.state()).visible,
        "death hides the meter before victory is processed");
    enemy->behavior = entities::EnemyBehaviorState::Idle;
    (void)session.update(false, world::RoomRole::Normal, true, true);
    expect(!game::buildBossHud(manager, session.state()).visible,
        "defeat hides a surviving boss meter");
    expect(session.restart(gameplay::RunRestartMode::SameSeed).has_value()
            && !game::buildBossHud(manager, session.state()).visible,
        "replay starts without carrying the previous engagement HUD");
    (void)session.update(true, world::RoomRole::Boss, true, true);
    (void)session.update(true, world::RoomRole::Boss, true, false);
    expect(!game::buildBossHud(manager, session.state()).visible,
        "victory always hides the combat meter");
    std::cout << "Boss HUD tracks encounter health and real combat phases\n";
    return 0;
}
