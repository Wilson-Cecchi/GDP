#include "world/WorldEntities.hpp"

#include "core/Random.hpp"
#include "game/SeedDomains.hpp"
#include "world/LootGenerator.hpp"
#include "world/PropGenerator.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace gdp::world {
namespace {

float yawForDirection(const wkc::core::Vec3 direction) noexcept
{
    constexpr float radiansToDegrees = 57.29577951308232F;
    return std::atan2(direction.x, direction.z) * radiansToDegrees;
}

} // namespace

bool InitialEntitySet::isValid() const noexcept
{
    return player.isValid() && terminal.isValid();
}

InitialEntitySet populateInitialEntities(
    World& world,
    gdp::entities::EntityManager& manager,
    gdp::entities::PlayerCreateInfo player,
    const float difficulty,
    const bool campaignWeapons)
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
    std::vector<std::uint32_t> roomDepths;
    lootLayouts.reserve(world.rooms.size());
    propLayouts.reserve(world.rooms.size());
    roomDepths.reserve(world.rooms.size());
    for (const RoomInstance& room : world.rooms) {
        const auto node = std::find_if(
            world.graph.nodes.begin(), world.graph.nodes.end(),
            [&room](const RoomNode& candidate) { return candidate.id == room.id; });
        if (node == world.graph.nodes.end()) {
            return result;
        }
        roomDepths.push_back(node->depth);
        LootLayout layout = generateLootLayout(
            room.seed, room.role, room.theme, node->depth, room.lootZones,
            safeDifficulty, campaignWeapons);
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
                const auto id = manager.createItem({
                    .transform = {.position = drop.position},
                    .roomId = room.id,
                    .type = drop.type,
                    .quantity = drop.quantity,
                });
                manager.findData<ItemData>(id)->spawnedWeaponArchetype = drop.weaponArchetype;
                addToRoom(id);
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

            const wkc::core::Vec3 workerPosition
                = positionInRoom(room, {2.4F, 0.0F, -0.5F});
            addToRoom(manager.createEnemy({
                .transform = {
                    .position = workerPosition,
                    .rotationDegrees = {0.0F,
                        yawForDirection(player.transform.position - workerPosition),
                        0.0F},
                },
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
        wkc::core::RandomStream encounter{wkc::core::deriveSeed(
            room.seed, gdp::game::SeedDomain::EnemySpawn, 0)};

        switch (room.role) {
        case RoomRole::Normal:
            if (encounter.chance(0.5F)) {
                addEnemy(EnemyArchetype::Worker, {}, 50.0F);
            } else {
                addEnemy(EnemyArchetype::Worker, {-0.75F, 0.0F, 0.0F}, 50.0F);
                const bool guarded = roomDepths[roomIndex] >= 3U;
                addEnemy(guarded ? EnemyArchetype::Guard : EnemyArchetype::Worker,
                    {0.75F, 0.0F, 0.0F}, guarded ? 75.0F : 50.0F);
            }
            break;
        case RoomRole::Hub:
            break;
        case RoomRole::Arena:
        {
            constexpr std::array archetypes{
                EnemyArchetype::Worker, EnemyArchetype::Guard,
                EnemyArchetype::Officer, EnemyArchetype::Engineer,
            };
            constexpr std::array positions{
                wkc::core::Vec3{-1.5F, 0.0F, 0.0F},
                wkc::core::Vec3{1.5F, 0.0F, 0.0F},
                wkc::core::Vec3{0.0F, 0.0F, 1.5F},
                wkc::core::Vec3{0.0F, 0.0F, -1.5F},
            };
            std::array<std::size_t, 4> order{0U, 1U, 2U, 3U};
            for (std::size_t index = 0; index < order.size(); ++index) {
                const auto selected = static_cast<std::size_t>(encounter.rangeInt(
                    static_cast<int>(index), static_cast<int>(order.size() - 1U)));
                std::swap(order[index], order[selected]);
            }
            const std::size_t count = static_cast<std::size_t>(encounter.rangeInt(2, 4));
            for (std::size_t index = 0; index < count; ++index) {
                const EnemyArchetype archetype = archetypes[order[index]];
                const float health = archetype == EnemyArchetype::Guard ? 75.0F
                    : archetype == EnemyArchetype::Officer ? 65.0F
                    : archetype == EnemyArchetype::Engineer ? 58.0F : 50.0F;
                addEnemy(archetype, positions[index], health);
            }
            break;
        }
        case RoomRole::DeadEnd:
            break;
        case RoomRole::Secret:
            break;
        case RoomRole::Boss:
            addEnemy(EnemyArchetype::IndustrialRobot, {},
                360.0F + 30.0F * static_cast<float>(encounter.rangeInt(0, 2)));
            addFixedProp(PropType::Door, {0.0F, 0.0F, 2.5F}, true);
            if (campaignWeapons) {
                // Reuse the authored teal console and its existing static solid.
                const bool sideways = room.rotation == wkc::core::QuarterTurn::Degrees90
                    || room.rotation == wkc::core::QuarterTurn::Degrees270;
                result.exitTerminal = manager.createProp({
                    .transform = {
                        .position = positionInRoom(room, {0.0F, 0.0F, 2.2F}),
                        .scale = sideways ? wkc::core::Vec3{0.3F, 2.0F, 0.8F}
                                          : wkc::core::Vec3{0.8F, 2.0F, 0.3F},
                    },
                    .roomId = room.id,
                    .type = PropType::Terminal,
                    .solid = true,
                });
                addToRoom(result.exitTerminal);
            }
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
