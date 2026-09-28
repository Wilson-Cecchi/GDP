#include "game/HudMap.hpp"
#include "world/WorldGenerator.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

const gdp::game::visual::HudMapRoom* findRoom(
    const gdp::game::visual::HudMapData& map,
    const gdp::world::RoomId id)
{
    const auto room = std::find_if(map.rooms.begin(),
        map.rooms.begin() + static_cast<std::ptrdiff_t>(map.roomCount),
        [id](const auto& candidate) { return candidate.id == id; });
    return room == map.rooms.begin() + static_cast<std::ptrdiff_t>(map.roomCount)
        ? nullptr : &*room;
}

} // namespace

int main()
{
    using namespace gdp::world;
    const WorldGenerationResult generated = generateWorld(12345U);
    expect(generated.succeeded(), "representative world generates for minimap test");
    if (!generated.world) {
        return 1;
    }
    const World& world = *generated.world;
    const auto start = std::find_if(world.rooms.begin(), world.rooms.end(),
        [](const RoomInstance& room) { return room.role == RoomRole::Start; });
    const auto secret = std::find_if(world.rooms.begin(), world.rooms.end(),
        [](const RoomInstance& room) { return room.role == RoomRole::Secret; });
    expect(start != world.rooms.end() && secret != world.rooms.end(),
        "map has a start and a secret room");
    if (start == world.rooms.end() || secret == world.rooms.end()) {
        return 1;
    }

    const std::array visited{start->id};
    const auto initial = gdp::game::buildHudMap(world, visited, start->id,
        start->transform.position, {0.0F, 0.0F, -1.0F});
    expect(initial.visible && initial.roomCount == world.rooms.size()
            && initial.linkCount == world.graph.edges.size(),
        "map snapshot keeps bounded world topology");
    expect(initial.playerPosition
            == wkc::core::Vec2{start->transform.position.x,
                start->transform.position.z}
            && initial.playerForward == wkc::core::Vec2{0.0F, -1.0F},
        "map receives the player's live position and direction");
    const auto* startMarker = findRoom(initial, start->id);
    const auto* secretMarker = findRoom(initial, secret->id);
    expect(startMarker != nullptr && startMarker->current && startMarker->visited,
        "start room is the initial current room");
    expect(secretMarker != nullptr && !secretMarker->known,
        "secret room is not revealed from the starting room");

    bool fogValid = true;
    RoomId nextRoom = 0U;
    for (const MapEdge& edge : world.graph.edges) {
        if (edge.first == start->id || edge.second == start->id) {
            nextRoom = edge.first == start->id ? edge.second : edge.first;
            break;
        }
    }
    for (const auto& room : world.rooms) {
        const bool adjacent = std::any_of(world.graph.edges.begin(),
            world.graph.edges.end(), [&room, start](const MapEdge& edge) {
                return (edge.first == start->id && edge.second == room.id)
                    || (edge.second == start->id && edge.first == room.id);
            });
        const auto* marker = findRoom(initial, room.id);
        fogValid = fogValid && marker != nullptr
            && marker->known == (room.id == start->id || adjacent)
            && marker->visited == (room.id == start->id);
    }
    for (std::size_t index = 0; index < initial.linkCount; ++index) {
        const auto& link = initial.links[index];
        fogValid = fogValid && link.known
            == (initial.rooms[link.first].id == start->id
                || initial.rooms[link.second].id == start->id);
        const auto corridor = std::find_if(world.corridors.begin(),
            world.corridors.end(), [&world, index](const CorridorPath& path) {
                return path.edge == world.graph.edges[index].id;
            });
        fogValid = fogValid && corridor != world.corridors.end()
            && link.pointCount == corridor->points.size();
        if (corridor != world.corridors.end()) {
            for (std::size_t point = 0; point < link.pointCount; ++point) {
                fogValid = fogValid && link.points[point]
                    == wkc::core::Vec2{corridor->points[point].x,
                        corridor->points[point].z};
            }
        }
    }
    expect(fogValid && nextRoom != 0U,
        "fog reveals neighboring exits and uses each corridor's actual path");

    const std::array visitedNext{start->id, nextRoom};
    const auto moved = gdp::game::buildHudMap(world, visitedNext, nextRoom,
        {2.0F, 0.0F, -3.0F}, {1.0F, 0.0F, 0.0F});
    const auto* oldMarker = findRoom(moved, start->id);
    const auto* nextMarker = findRoom(moved, nextRoom);
    expect(oldMarker != nullptr && oldMarker->visited && !oldMarker->current
            && nextMarker != nullptr && nextMarker->visited && nextMarker->current,
        "entering another room preserves explored rooms and moves current marker");
    expect(moved.playerPosition == wkc::core::Vec2{2.0F, -3.0F}
            && moved.playerForward == wkc::core::Vec2{1.0F, 0.0F},
        "player marker updates without regenerating the world");

    const auto reset = gdp::game::buildHudMap(world, visited, start->id,
        start->transform.position, {0.0F, 0.0F, -1.0F});
    expect(findRoom(reset, nextRoom) != nullptr
            && !findRoom(reset, nextRoom)->visited,
        "new run visitation does not retain rooms from a previous run");
    expect(!gdp::game::buildHudMap(world, visited, 0U, {}, {}).visible,
        "map stays hidden without an active player room");

    if (failures != 0) {
        std::cerr << failures << " minimap test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP minimap tests passed\n";
    return 0;
}
