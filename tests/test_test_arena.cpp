#include "game/TestArena.hpp"

#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Props.hpp"

#include <array>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void expect(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testPracticeAreaAndBossApproach()
{
    using namespace gdp;
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    for (const wkc::core::Vec3 playerPosition : std::array{
             wkc::core::Vec3{0.0F, 0.0F, 0.0F},
             wkc::core::Vec3{4.0F, 0.0F, 0.0F},
             wkc::core::Vec3{12.0F, 0.0F, -12.0F}}) {
        auto arena = game::makeTestArena(12345U);
        EntityManager manager;
        const auto initial = game::populateTestArena(arena, manager, {
            .transform = {.position = playerPosition}, .health = 100.0F});
        expect(initial.isValid(), "test arena populates");
        const CollisionWorld collision = collisionWorldWithProps(
            CollisionWorld{arena.rooms.front().worldBounds}, manager, 1U);
        expect(collision.canOccupy(playerPosition),
            "preparation and target areas remain walkable");
        Player player;
        player.spawn(playerPosition);
        std::vector<EnemyProjectile> shots;
        const EnemyPerception perception{.projectiles = &shots};
        for (int tick = 0; tick < 600; ++tick) {
            (void)updateIndustrialRobots(manager, player, 1U, collision,
                1.0F / 60.0F, {}, perception);
        }
        expect(shots.empty(),
            "boss cannot cannon the preparation and target areas from afar");
    }

    auto arena = game::makeTestArena(54321U);
    EntityManager manager;
    const auto initial = game::populateTestArena(arena, manager, {
        .transform = {.position = {0.0F, 0.0F, 0.0F}}, .health = 100.0F});
    expect(initial.isValid(), "boss approach arena populates");
    const CollisionWorld collision = collisionWorldWithProps(
        CollisionWorld{arena.rooms.front().worldBounds}, manager, 1U);
    expect(collision.canOccupy({0.0F, 0.0F, -18.0F})
            && collision.canOccupy({0.0F, 0.0F, -23.0F}),
        "central passage connects preparation and combat zones");
    Player practicePlayer;
    practicePlayer.spawn({0.0F, 0.0F, 0.0F});
    std::size_t targetCount = 0U;
    for (const EntityId id : manager.query(EntityKind::Prop)) {
        const Entity* entity = manager.find(id);
        const PropData* prop = manager.findData<PropData>(id);
        if (entity == nullptr || prop == nullptr
            || prop->type != PropType::Canister
            || prop->maximumHealth != 1'000'000.0F) {
            continue;
        }
        ++targetCount;
        Weapon gun(baseWeaponDefinition(WeaponArchetype::Carbine));
        const auto origin = practicePlayer.cameraPosition();
        const auto direction = wkc::core::normalized(
            entity->transform.position + wkc::core::Vec3{0.0F, 0.9F, 0.0F}
            - origin);
        const ShotResult shot = fireHitscan(gun, origin, direction,
            1U, collision, manager);
        expect(shot.hit && shot.hit->target == id,
            "each practice target remains reachable from the safe area");
    }
    expect(targetCount == 3U, "test arena retains three practice targets");
    Player player;
    player.spawn({0.0F, 0.0F, -23.0F});
    std::vector<EnemyProjectile> shots;
    const EnemyPerception perception{.projectiles = &shots};
    for (int tick = 0; tick < 120 && shots.empty(); ++tick) {
        (void)updateIndustrialRobots(manager, player, 1U, collision,
            1.0F / 60.0F, {}, perception);
    }
    expect(!shots.empty(),
        "boss can still attack after the player enters the combat zone");
}

} // namespace

int main()
{
    testPracticeAreaAndBossApproach();
    if (failures != 0) {
        std::cerr << failures << " test arena test(s) failed\n";
        return 1;
    }
    std::cout << "All test arena tests passed\n";
    return 0;
}
