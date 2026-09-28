#include "game/HudMap.hpp"

#include <algorithm>

namespace gdp::game {

visual::HudMapData buildHudMap(
    const world::World& world,
    const std::span<const world::RoomId> visitedRooms,
    const world::RoomId currentRoom,
    const wkc::core::Vec3 playerPosition,
    const wkc::core::Vec3 playerForward) noexcept
{
    visual::HudMapData map;
    if (currentRoom == 0U || world.rooms.empty()
        || world.rooms.size() > visual::maximumHudMapRooms
        || world.graph.edges.size() > visual::maximumHudMapLinks) {
        return map;
    }
    map.playerPosition = {playerPosition.x, playerPosition.z};
    map.playerForward = {playerForward.x, playerForward.z};
    map.roomCount = world.rooms.size();
    for (std::size_t index = 0; index < map.roomCount; ++index) {
        const world::RoomInstance& room = world.rooms[index];
        const bool visited = room.id == currentRoom
            || std::find(visitedRooms.begin(), visitedRooms.end(), room.id)
                != visitedRooms.end();
        map.rooms[index] = {
            room.id,
            {room.transform.position.x, room.transform.position.z},
            visited,
            visited,
            room.id == currentRoom,
        };
    }
    const auto roomIndex = [&map](const world::RoomId id) {
        for (std::size_t index = 0; index < map.roomCount; ++index) {
            if (map.rooms[index].id == id) {
                return index;
            }
        }
        return map.roomCount;
    };
    for (const world::MapEdge& edge : world.graph.edges) {
        const std::size_t first = roomIndex(edge.first);
        const std::size_t second = roomIndex(edge.second);
        if (first == map.roomCount || second == map.roomCount) {
            continue;
        }
        const bool known = map.rooms[first].visited || map.rooms[second].visited;
        map.links[map.linkCount++] = {
            static_cast<std::uint8_t>(first),
            static_cast<std::uint8_t>(second),
            known,
        };
        if (known) {
            map.rooms[first].known = true;
            map.rooms[second].known = true;
        }
    }
    map.visible = roomIndex(currentRoom) != map.roomCount;
    return map;
}

} // namespace gdp::game
