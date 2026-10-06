#pragma once

#include <cstdint>

namespace gdp::game {

enum class SeedDomain : std::uint64_t {
    World = 1,
    Graph = 2,
    Room = 3,
    RoomModule = 4,
    Corridor = 5,
    Props = 6,
    Lighting = 7,
    EnemySpawn = 8,
    EnemyVisual = 9,
    EnemyBody = 10,
    EnemyParts = 11,
    EnemyMaterial = 12,
    EnemyWear = 13,
    Loot = 14,
    Events = 15,
    Audio = 16,
    RoomMaterial = 17,
    NeuralWorker = 18,
    MapPlacement = 19,
    CampaignLayout = 20,
    CampaignMission = 21,
};

} // namespace gdp::game
