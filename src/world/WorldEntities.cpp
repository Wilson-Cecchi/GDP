#include "world/WorldEntities.hpp"

#include "game/SeedDomains.hpp"
#include "world/LootGenerator.hpp"
#include "world/PropGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::world {

bool InitialEntitySet::isValid() const noexcept
{
    return player.isValid() && terminal.isValid();
}

InitialEntitySet populateInitialEntities(
    World& world,
    gdp::entities::EntityManager& manager,
    gdp::entities::PlayerCreateInfo player,
    const float difficulty)
{
    using namespace gdp::entities;

    InitialEntitySet result;
    const bool hasStartRoom = std::any_of(
        world.rooms.begin(), world.rooms.end(),
        [](const RoomInstance& room) { return room.role == RoomRole::Start; });
    if (!hasStartRoom || manager.activeCount() != 0) {
        return result;
    }
    const float safeDifficulty = std::clamp(
        std::isfinite(difficulty) ? difficulty : 1.0F, 0.5F, 2.0F);
    std::vector<LootLayout> lootLayouts;
    std::vector<PropLayout> propLayouts;
    lootLayouts.reserve(world.rooms.size());
    propLayouts.reserve(world.rooms.size());
    for (const RoomInstance& room : world.rooms) {
        const auto node = std::find_if(
            world.graph.nodes.begin(), world.graph.nodes.end(),
            [&room](const RoomNode& candidate) { return candidate.id == room.id; });
        if (node == world.graph.nodes.end()) {
            return result;
        }
        LootLayout layout = generateLootLayout(
            room.seed, room.role, room.theme, node->depth, room.lootZones,
            safeDifficulty);
        if (!validateLootLayout(layout, room.lootZones).isValid()) {
            return result;
        }
        lootLayouts.push_back(std::move(layout));
        PropLayout props = generatePropLayout(
            room.seed, room.role, room.theme, node->depth, room.propZones,
            safeDifficulty);
        if (!validatePropLayout(props, room.propZones).isValid()) {
            return result;
        }
        propLayouts.push_back(std::move(props));
    }
    const auto positionInRoom = [](const RoomInstance& room, const wkc::core::Vec3 offset) {
        return room.transform.position
            + wkc::core::rotateQuarterTurns(offset, room.rotation);
    };

    for (std::size_t roomIndex = 0; roomIndex < world.rooms.size(); ++roomIndex) {
        RoomInstance& room = world.rooms[roomIndex];
        room.entities.clear();
        const auto addToRoom = [&room](const EntityId id) {
            room.entities.push_back(id);
        };
        const auto addLoot = [&] {
            for (const LootDrop& drop : lootLayouts[roomIndex].drops) {
                addToRoom(manager.createItem({
                    .transform = {.position = drop.position},
                    .roomId = room.id,
                    .type = drop.type,
                    .quantity = drop.quantity,
                }));
            }
        };
        const auto addProps = [&] {
            for (const PropSpawn& prop : propLayouts[roomIndex].props) {
                addToRoom(manager.createProp({
                    .transform = {.position = prop.position, .scale = prop.scale},
                    .roomId = room.id,
                    .type = prop.type,
                    .state = prop.state,
                    .solid = prop.solid,
                    .health = prop.health,
                    .maximumHealth = prop.maximumHealth,
                    .dropType = prop.dropType,
                    .dropQuantity = prop.dropQuantity,
                    .visualSeed = prop.visualSeed,
                }));
            }
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
                .health = 68.0F * safeDifficulty,
                .visualSeed = wkc::core::deriveSeed(
                    room.seed, gdp::game::SeedDomain::EnemyVisual, 0),
            }));
            constexpr wkc::core::Vec3 localCoverScale{0.46F, 2.25F, 2.80F};
            const wkc::core::Vec3 rotatedCoverScale
                = wkc::core::rotateQuarterTurns(localCoverScale, room.rotation);
            addToRoom(manager.createProp({
                .transform = {
                    .position = positionInRoom(room, {-2.55F, 0.0F, 0.0F}),
                    .scale = {
                        std::abs(rotatedCoverScale.x),
                        localCoverScale.y,
                        std::abs(rotatedCoverScale.z),
                    },
                },
                .roomId = room.id,
                .type = PropType::Barricade,
                .solid = true,
                .health = 180.0F,
                .maximumHealth = 180.0F,
                .visualSeed = wkc::core::deriveSeed(
                    room.seed, gdp::game::SeedDomain::Props, 900U),
            }));
            addProps();
            addLoot();
            continue;
        }

        std::uint64_t enemyVisualOrdinal = 0;
        const auto addEnemy = [&](const EnemyArchetype archetype,
                                  const wkc::core::Vec3 offset,
                                  const float health) {
            addToRoom(manager.createEnemy({
                .transform = {.position = positionInRoom(room, offset)},
                .roomId = room.id,
                .archetype = archetype,
                .health = health * safeDifficulty,
                .visualSeed = wkc::core::deriveSeed(
                    room.seed,
                    gdp::game::SeedDomain::EnemyVisual,
                    enemyVisualOrdinal++),
            }));
        };
        const auto addFixedProp = [&](const PropType type,
                                      const wkc::core::Vec3 offset,
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
            break;
        case RoomRole::Hub:
            break;
        case RoomRole::Arena:
            addEnemy(EnemyArchetype::Worker, {-1.5F, 0.0F, 0.0F}, 50.0F);
            addEnemy(EnemyArchetype::Guard, {1.5F, 0.0F, 0.0F}, 75.0F);
            addEnemy(EnemyArchetype::Officer, {0.0F, 0.0F, 1.5F}, 65.0F);
            addEnemy(EnemyArchetype::Engineer, {0.0F, 0.0F, -1.5F}, 58.0F);
            break;
        case RoomRole::DeadEnd:
            break;
        case RoomRole::Secret:
            break;
        case RoomRole::Boss:
            addEnemy(EnemyArchetype::IndustrialRobot, {}, 250.0F);
            addFixedProp(PropType::Door, {0.0F, 0.0F, 2.5F}, true);
            break;
        case RoomRole::Start:
        case RoomRole::Count:
            break;
        }
        addProps();
        addLoot();
    }
    return result;
}

} // namespace gdp::world
