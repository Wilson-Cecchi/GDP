#include "entities/EntityManager.hpp"

#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testCreationAndTypedData()
{
    using namespace gdp::entities;

    EntityManager manager;
    const EntityId player = manager.createPlayer({
        .transform = {.position = {1.0F, 2.0F, 3.0F}},
        .roomId = 10,
        .health = 82.0F,
    });
    const EntityId enemy = manager.createEnemy({
        .roomId = 20,
        .archetype = EnemyArchetype::Guard,
        .health = 75.0F,
        .visualSeed = 9876,
    });
    const EntityId item = manager.createItem({
        .roomId = 20,
        .type = ItemType::Ammo,
        .quantity = 0,
    });
    const EntityId prop = manager.createProp({
        .roomId = 10,
        .type = PropType::Terminal,
        .solid = true,
    });

    expect(player == EntityId{0, 1} && enemy == EntityId{1, 1},
        "fresh manager assigns stable sequential handles");
    expect(player.packed() == 0x1'0000'0000ULL,
        "packed handle preserves generation and index");
    expect(manager.activeCount() == 4 && manager.capacity() == 4,
        "creation updates active count and slot capacity");
    expect(manager.find(player) != nullptr
            && manager.find(player)->kind == EntityKind::Player
            && manager.find(player)->transform.position == gdp::core::Vec3{1.0F, 2.0F, 3.0F},
        "created entity preserves kind and transform");

    const PlayerData* playerData = manager.findData<PlayerData>(player);
    const EnemyData* enemyData = manager.findData<EnemyData>(enemy);
    const ItemData* itemData = manager.findData<ItemData>(item);
    const PropData* propData = manager.findData<PropData>(prop);
    expect(playerData != nullptr && playerData->health == 82.0F,
        "player payload is available through typed lookup");
    expect(enemyData != nullptr && enemyData->archetype == EnemyArchetype::Guard
            && enemyData->health == 75.0F && enemyData->maximumHealth == 75.0F
            && enemyData->behavior == EnemyBehaviorState::Idle
            && enemyData->visualSeed == 9876,
        "enemy payload preserves archetype, health, behavior and visual seed");
    expect(itemData != nullptr && itemData->type == ItemType::Ammo && itemData->quantity == 1,
        "item payload clamps invalid zero quantity");
    expect(propData != nullptr && propData->type == PropType::Terminal && propData->solid,
        "prop payload preserves type and collision flag");
    expect(manager.findData<EnemyData>(player) == nullptr,
        "typed lookup rejects a mismatched payload");

    expect(manager.query(EntityKind::Enemy) == std::vector<EntityId>{enemy},
        "kind query returns only matching live entities");
    expect(manager.queryRoom(10) == std::vector<EntityId>({player, prop}),
        "room query preserves deterministic slot order");
}

void testDeferredDestructionAndReuse()
{
    using namespace gdp::entities;

    EntityManager manager;
    const EntityId first = manager.createEnemy({.roomId = 7});
    const EntityId survivor = manager.createProp({.roomId = 8});

    expect(manager.destroy(first), "live entity can be scheduled for destruction");
    expect(!manager.destroy(first), "entity cannot be scheduled twice");
    expect(!manager.isAlive(first) && manager.find(first) == nullptr,
        "pending entity becomes unavailable immediately");
    expect(manager.activeCount() == 1 && manager.pendingDestroyCount() == 1,
        "deferred destruction updates both counters");
    expect(manager.flushDestroyed() == 1 && manager.pendingDestroyCount() == 0,
        "flush releases each pending slot once");

    const EntityId replacement = manager.createItem({.roomId = 9});
    expect(replacement.index == first.index && replacement.generation != first.generation,
        "released slot is reused with a new generation");
    expect(!manager.isAlive(first) && manager.isAlive(replacement),
        "stale handle cannot resolve to its replacement");
    expect(manager.isAlive(survivor), "unrelated live entity survives a flush");
}

void testRoomDestructionAndClear()
{
    using namespace gdp::entities;

    EntityManager manager;
    const EntityId first = manager.createEnemy({.roomId = 4});
    const EntityId second = manager.createItem({.roomId = 4});
    const EntityId other = manager.createProp({.roomId = 5});

    expect(manager.destroyInRoom(4) == 2,
        "room destruction schedules every live entity in the room");
    expect(manager.destroyInRoom(4) == 0,
        "room destruction ignores entities already pending");
    expect(manager.queryRoom(4).empty() && manager.queryRoom(5) == std::vector<EntityId>{other},
        "queries exclude pending entities without affecting other rooms");
    expect(manager.flushDestroyed() == 2 && manager.activeCount() == 1,
        "room destruction flush preserves entities in other rooms");

    manager.clear();
    expect(manager.activeCount() == 0 && manager.pendingDestroyCount() == 0,
        "clear resets all lifecycle counters");
    expect(!manager.isAlive(first) && !manager.isAlive(second) && !manager.isAlive(other),
        "clear invalidates every previously issued handle");
}

void testLifecycleStress()
{
    using namespace gdp::entities;

    constexpr std::uint32_t entityCount = 10'000;
    EntityManager manager;
    std::vector<EntityId> handles;
    handles.reserve(entityCount);
    for (std::uint32_t index = 0; index < entityCount; ++index) {
        handles.push_back(manager.createEnemy({.roomId = index % 32U}));
    }
    for (std::uint32_t index = 0; index < entityCount; index += 2U) {
        expect(manager.destroy(handles[index]), "stress entity can be scheduled");
    }
    expect(manager.flushDestroyed() == entityCount / 2U,
        "stress flush releases all scheduled entities");

    for (std::uint32_t index = 0; index < entityCount / 2U; ++index) {
        (void)manager.createProp({.roomId = index % 32U});
    }
    expect(manager.activeCount() == entityCount && manager.capacity() == entityCount,
        "stress reuse does not grow storage when free slots exist");
    for (std::uint32_t index = 0; index < entityCount; index += 2U) {
        expect(!manager.isAlive(handles[index]), "stress reuse keeps every stale handle invalid");
    }
}

} // namespace

int main()
{
    testCreationAndTypedData();
    testDeferredDestructionAndReuse();
    testRoomDestructionAndClear();
    testLifecycleStress();

    if (failures != 0) {
        std::cerr << failures << " entity manager test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP entity manager tests passed\n";
    return 0;
}
