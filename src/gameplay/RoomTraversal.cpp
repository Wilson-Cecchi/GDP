#include "gameplay/RoomTraversal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

namespace gdp::gameplay {
namespace {

const gdp::world::RoomInstance* findRoom(
    const gdp::world::World& world,
    const gdp::world::RoomId id) noexcept
{
    const auto room = std::find_if(
        world.rooms.begin(), world.rooms.end(),
        [id](const gdp::world::RoomInstance& candidate) {
            return candidate.id == id;
        });
    return room == world.rooms.end() ? nullptr : &*room;
}

const gdp::world::CorridorPath* findCorridor(
    const gdp::world::World& world,
    const gdp::world::EdgeId edge) noexcept
{
    const auto corridor = std::find_if(
        world.corridors.begin(), world.corridors.end(),
        [edge](const gdp::world::CorridorPath& candidate) {
            return candidate.edge == edge;
        });
    return corridor == world.corridors.end() ? nullptr : &*corridor;
}

const gdp::world::ConnectorInstance* connectorForEdge(
    const gdp::world::RoomInstance& room,
    const gdp::world::EdgeId edge) noexcept
{
    const auto connector = std::find_if(
        room.connectors.begin(), room.connectors.end(),
        [edge](const gdp::world::ConnectorInstance& candidate) {
            return !candidate.sealed && candidate.edge == edge;
        });
    return connector == room.connectors.end() ? nullptr : &*connector;
}

float horizontalDistance(
    const gdp::core::Vec3 first,
    const gdp::core::Vec3 second) noexcept
{
    const float differenceX = first.x - second.x;
    const float differenceZ = first.z - second.z;
    return std::sqrt((differenceX * differenceX) + (differenceZ * differenceZ));
}

gdp::core::Aabb corridorSegmentVolume(
    const gdp::core::Vec3 first,
    const gdp::core::Vec3 second,
    const float width,
    const float height) noexcept
{
    // The longitudinal overlap lets a circular character pass continuously
    // between volumes after each AABB is inset by its collision radius.
    constexpr float endpointOverlap = 0.65F;
    const float halfWidth = width * 0.5F;
    const float floor = std::min(first.y, second.y);
    const float ceiling = std::max(first.y, second.y) + height;
    if (gdp::core::nearlyEqual(first.z, second.z)) {
        return {
            {std::min(first.x, second.x) - endpointOverlap,
                floor,
                first.z - halfWidth},
            {std::max(first.x, second.x) + endpointOverlap,
                ceiling,
                first.z + halfWidth},
        };
    }
    return {
        {first.x - halfWidth,
            floor,
            std::min(first.z, second.z) - endpointOverlap},
        {first.x + halfWidth,
            ceiling,
            std::max(first.z, second.z) + endpointOverlap},
    };
}

void appendFixedRoomSolids(
    const gdp::world::RoomInstance& room,
    std::vector<gdp::core::Aabb>& solids)
{
    const CollisionWorld fixed = CollisionWorld::fixedRoom(
        room.transform.position, room.rotation);
    solids.insert(solids.end(), fixed.solids().begin(), fixed.solids().end());
}

} // namespace

std::optional<RoomTransition> findRoomTransition(
    const gdp::world::World& world,
    const gdp::world::RoomId currentRoom,
    const gdp::core::Vec3 playerFeetPosition,
    gdp::core::Vec3 playerForward,
    const float maximumDistance,
    const float destinationInset) noexcept
{
    if (currentRoom == 0U || !std::isfinite(maximumDistance)
        || !std::isfinite(destinationInset) || maximumDistance <= 0.0F
        || destinationInset <= 0.0F) {
        return std::nullopt;
    }
    playerForward.y = 0.0F;
    if (gdp::core::length(playerForward) <= 1.0e-5F) {
        return std::nullopt;
    }
    playerForward = gdp::core::normalized(playerForward);

    const gdp::world::RoomInstance* source = findRoom(world, currentRoom);
    if (source == nullptr) {
        return std::nullopt;
    }

    const gdp::world::ConnectorInstance* selected = nullptr;
    float selectedDistance = std::numeric_limits<float>::max();
    for (const gdp::world::ConnectorInstance& connector : source->connectors) {
        if (connector.sealed || !connector.edge) {
            continue;
        }
        gdp::core::Vec3 direction = connector.worldPosition - playerFeetPosition;
        direction.y = 0.0F;
        const float distance = horizontalDistance(
            connector.worldPosition, playerFeetPosition);
        if (distance > maximumDistance || distance <= 1.0e-5F) {
            continue;
        }
        direction = direction * (1.0F / distance);
        if (gdp::core::dot(playerForward, direction) < 0.60F) {
            continue;
        }
        if (distance < selectedDistance) {
            selected = &connector;
            selectedDistance = distance;
        }
    }
    if (selected == nullptr || !selected->edge) {
        return std::nullopt;
    }

    const gdp::world::CorridorPath* corridor = findCorridor(world, *selected->edge);
    if (corridor == nullptr) {
        return std::nullopt;
    }
    gdp::world::RoomId destinationId = 0U;
    if (corridor->firstRoom == currentRoom) {
        destinationId = corridor->secondRoom;
    } else if (corridor->secondRoom == currentRoom) {
        destinationId = corridor->firstRoom;
    } else {
        return std::nullopt;
    }

    const gdp::world::RoomInstance* destination = findRoom(world, destinationId);
    if (destination == nullptr) {
        return std::nullopt;
    }
    const gdp::world::ConnectorInstance* entry
        = connectorForEdge(*destination, *selected->edge);
    if (entry == nullptr) {
        return std::nullopt;
    }

    gdp::core::Vec3 destinationPosition
        = entry->worldPosition - (entry->worldNormal * destinationInset);
    destinationPosition.y = destination->worldBounds.minimum.y;
    if (!destination->worldBounds.contains(destinationPosition)) {
        return std::nullopt;
    }
    const gdp::core::Vec3 inward = entry->worldNormal * -1.0F;
    constexpr float radiansToDegrees = 180.0F / std::numbers::pi_v<float>;
    const float yaw = std::atan2(inward.z, inward.x) * radiansToDegrees;
    return RoomTransition{
        *selected->edge,
        currentRoom,
        destinationId,
        destinationPosition,
        yaw,
    };
}

CollisionWorld makeRoomNavigationCollision(
    const gdp::world::World& world,
    const gdp::world::RoomId currentRoom,
    const bool doorsLocked)
{
    const gdp::world::RoomInstance* source = findRoom(world, currentRoom);
    if (source == nullptr) {
        return CollisionWorld{{}};
    }

    std::vector<gdp::core::Aabb> volumes{source->worldBounds};
    std::vector<gdp::core::Aabb> solids;
    appendFixedRoomSolids(*source, solids);
    if (doorsLocked) {
        return CollisionWorld::fromWalkableVolumes(
            std::move(volumes), std::move(solids));
    }

    for (const gdp::world::CorridorPath& corridor : world.corridors) {
        gdp::world::RoomId destinationId = 0U;
        if (corridor.firstRoom == currentRoom) {
            destinationId = corridor.secondRoom;
        } else if (corridor.secondRoom == currentRoom) {
            destinationId = corridor.firstRoom;
        } else {
            continue;
        }

        const gdp::world::RoomInstance* destination = findRoom(world, destinationId);
        if (destination == nullptr || corridor.points.size() < 2U) {
            continue;
        }
        volumes.push_back(destination->worldBounds);
        appendFixedRoomSolids(*destination, solids);
        for (std::size_t index = 0; index + 1U < corridor.points.size(); ++index) {
            volumes.push_back(corridorSegmentVolume(
                corridor.points[index], corridor.points[index + 1U],
                corridor.width, corridor.height));
        }
    }
    return CollisionWorld::fromWalkableVolumes(
        std::move(volumes), std::move(solids));
}

std::optional<gdp::world::RoomId> roomContainingPosition(
    const gdp::world::World& world,
    const gdp::core::Vec3 feetPosition) noexcept
{
    for (const gdp::world::RoomInstance& room : world.rooms) {
        if (room.worldBounds.contains(feetPosition)) {
            return room.id;
        }
    }
    return std::nullopt;
}

} // namespace gdp::gameplay
