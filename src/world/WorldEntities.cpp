#include "world/WorldEntities.hpp"

#include <algorithm>

namespace gdp::world {

bool InitialEntitySet::isValid() const noexcept
{
    return player.isValid() && terminal.isValid();
}

InitialEntitySet populateInitialEntities(
    World& world,
    gdp::entities::EntityManager& manager,
    gdp::entities::PlayerCreateInfo player)
{
    using namespace gdp::entities;

    InitialEntitySet result;
    const bool hasStartRoom = std::any_of(
        world.rooms.begin(), world.rooms.end(),
        [](const RoomInstance& room) { return room.role == RoomRole::Start; });
    if (!hasStartRoom || manager.activeCount() != 0) {
        return result;
    }
    const auto positionInRoom = [](const RoomInstance& room, const gdp::core::Vec3 offset) {
        return room.transform.position + offset;
    };

    for (RoomInstance& room : world.rooms) {
        room.entities.clear();
        const auto addToRoom = [&room](const EntityId id) {
            room.entities.push_back(id);
        };

        if (room.role == RoomRole::Start) {
            player.roomId = room.id;
            result.player = manager.createPlayer(player);
            addToRoom(result.player);

            result.terminal = manager.createProp({
                .transform = {.position = positionInRoom(room, {0.0F, 0.0F, 2.2F})},
                .roomId = room.id,
                .type = PropType::Terminal,
                .solid = true,
            });
            addToRoom(result.terminal);

            addToRoom(manager.createEnemy({
                .transform = {.position = positionInRoom(room, {2.4F, 0.0F, -0.5F})},
                .roomId = room.id,
                .archetype = EnemyArchetype::Worker,
                .health = 68.0F,
                .visualSeed = gdp::core::deriveSeed(
                    room.seed, gdp::core::SeedDomain::EnemyVisual, 0),
            }));
            continue;
        }

        std::uint64_t enemyVisualOrdinal = 0;
        const auto addEnemy = [&](const EnemyArchetype archetype,
                                  const gdp::core::Vec3 offset,
                                  const float health) {
            addToRoom(manager.createEnemy({
                .transform = {.position = positionInRoom(room, offset)},
                .roomId = room.id,
                .archetype = archetype,
                .health = health,
                .visualSeed = gdp::core::deriveSeed(
                    room.seed,
                    gdp::core::SeedDomain::EnemyVisual,
                    enemyVisualOrdinal++),
            }));
        };
        const auto addItem = [&](const ItemType type, const std::uint32_t quantity) {
            addToRoom(manager.createItem({
                .transform = {.position = positionInRoom(room, {1.5F, 0.0F, 0.0F})},
                .roomId = room.id,
                .type = type,
                .quantity = quantity,
            }));
        };
        const auto addProp = [&](const PropType type,
                                 const gdp::core::Vec3 offset,
                                 const bool solid) {
            addToRoom(manager.createProp({
                .transform = {.position = positionInRoom(room, offset)},
                .roomId = room.id,
                .type = type,
                .solid = solid,
            }));
        };

        switch (room.role) {
        case RoomRole::Normal:
            addEnemy(EnemyArchetype::Worker, {}, 50.0F);
            addProp(PropType::Crate, {-1.5F, 0.0F, 0.0F}, true);
            break;
        case RoomRole::Hub:
            addItem(ItemType::Health, 25);
            addProp(PropType::Decoration, {-1.5F, 0.0F, 0.0F}, false);
            break;
        case RoomRole::Arena:
            addEnemy(EnemyArchetype::Worker, {-1.5F, 0.0F, 0.0F}, 50.0F);
            addEnemy(EnemyArchetype::Guard, {1.5F, 0.0F, 0.0F}, 75.0F);
            addEnemy(EnemyArchetype::Officer, {0.0F, 0.0F, 1.5F}, 65.0F);
            break;
        case RoomRole::DeadEnd:
            addItem(ItemType::Ammo, 12);
            addProp(PropType::Crate, {-1.5F, 0.0F, 0.0F}, true);
            break;
        case RoomRole::Secret:
            addItem(ItemType::Key, 1);
            addItem(ItemType::Health, 50);
            break;
        case RoomRole::Boss:
            addEnemy(EnemyArchetype::IndustrialRobot, {}, 250.0F);
            addProp(PropType::Door, {0.0F, 0.0F, 2.5F}, true);
            break;
        case RoomRole::Start:
        case RoomRole::Count:
            break;
        }
    }
    return result;
}

} // namespace gdp::world
