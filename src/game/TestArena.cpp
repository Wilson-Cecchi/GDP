#include "game/TestArena.hpp"
#include "gameplay/Weapon.hpp"

#include <array>
#include <utility>

namespace gdp::game {
world::World makeTestArena(const wkc::core::Seed seed)
{
    world::World arena;
    arena.masterSeed = seed; arena.generationSeed = seed;
    arena.graph.nodes.push_back({1U, world::RoomRole::Start, world::RoomTheme::Industrial, 0U});
    arena.graph.mainPath.push_back(1U);
    world::RoomInstance room;
    room.id = 1U; room.role = world::RoomRole::Start; room.seed = seed;
    room.sizeClass = world::RoomSizeClass::Large;
    room.worldBounds = {{-16.0F, 0.0F, -36.0F}, {16.0F, 5.0F, 12.0F}};
    arena.rooms.push_back(std::move(room));
    return arena;
}
world::InitialEntitySet populateTestArena(world::World& arena, entities::EntityManager& manager, entities::PlayerCreateInfo player)
{
    using namespace entities;
    if (arena.rooms.size() != 1U || manager.activeCount() != 0U) return {};
    auto& room = arena.rooms.front();
    player.roomId = room.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    result.terminal = manager.createProp({.transform = {.position = {0.0F, 0.0F, 8.0F}}, .roomId = 1U, .type = PropType::Terminal, .solid = true});
    room.entities = {result.player, result.terminal};
    // Two tall bulkheads shelter the weapon pickups and target lane from the
    // robot's long-range cannon. The four-metre center gap stays walkable.
    for (const float x : {-9.0F, 9.0F}) {
        room.entities.push_back(manager.createProp({
            .transform = {.position = {x, 0.0F, -18.0F},
                .scale = {14.0F, 3.5F, 1.5F}},
            .roomId = room.id, .type = PropType::Barricade, .solid = true,
            .health = 1'000'000.0F, .maximumHealth = 1'000'000.0F,
            .visualSeed = arena.masterSeed + (x < 0.0F ? 50U : 51U)}));
    }
    for (unsigned i = 0U; i < 5U; ++i) {
        room.entities.push_back(manager.createEnemy({
            .transform = {.position = {-10.0F + static_cast<float>(i) * 5.0F, 0.0F, -28.0F}},
            .roomId = 1U, .archetype = static_cast<EnemyArchetype>(i),
            .health = i == 4U ? 260.0F : 80.0F, .visualSeed = arena.masterSeed + i}));
    }
    for (unsigned i = 0U; i < 4U; ++i) {
        room.entities.push_back(manager.createProp({
            .transform = {.position = {-9.0F + static_cast<float>(i) * 6.0F, 0.0F, -6.0F},
                .scale = {2.0F, i % 2U == 0U ? 2.5F : 0.6F, 2.0F}},
            .roomId = 1U, .type = PropType::Barricade, .solid = true,
            .health = 2000.0F, .maximumHealth = 2000.0F, .visualSeed = i + 10U}));
        room.entities.push_back(manager.createItem({.transform = {.position = {-6.0F + static_cast<float>(i) * 4.0F, 0.0F, 7.0F}},
            .roomId = 1U, .type = i == 0U ? ItemType::Health : ItemType::Ammo, .quantity = 100U}));
    }
    for (unsigned i = 0U; i < 3U; ++i) {
        const auto id = manager.createItem({
            .transform = {.position = {-4.0F + static_cast<float>(i) * 4.0F, 0.0F, 0.0F}},
            .roomId = room.id, .type = ItemType::Weapon, .quantity = 1U});
        manager.findData<ItemData>(id)->spawnedWeaponArchetype
            = static_cast<std::uint32_t>(gameplay::WeaponArchetype::Sniper) + i;
        room.entities.push_back(id);
    }
    // Durable plates remain on the preparation side of the divider.
    // The existing canister role gives them a red metallic hit material.
    constexpr std::array<float, 3> targetDepths{1.0F, -7.0F, -14.0F};
    for (unsigned i = 0U; i < targetDepths.size(); ++i) {
        room.entities.push_back(manager.createProp({
            .transform = {.position = {14.5F, 0.0F, targetDepths[i]},
                .scale = {0.30F, 1.8F, 1.6F}},
            .roomId = room.id, .type = PropType::Canister, .solid = true,
            .health = 1'000'000.0F, .maximumHealth = 1'000'000.0F,
            .visualSeed = arena.masterSeed + 40U + i}));
    }
    return result;
}
}
