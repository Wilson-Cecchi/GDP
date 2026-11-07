#include "world/RoomArchitecture.hpp"
#include "world/RoomFootprint.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace gdp::world {
namespace {

constexpr wkc::core::Vec3 darkSteel{0.27F, 0.28F, 0.28F};
constexpr wkc::core::Vec3 shelfSteel{0.39F, 0.35F, 0.30F};
constexpr wkc::core::Vec3 rackLipSteel{0.35F, 0.36F, 0.35F};
constexpr wkc::core::Vec3 oliveCargo{0.44F, 0.43F, 0.30F};
constexpr wkc::core::Vec3 charcoalCargo{0.34F, 0.35F, 0.34F};
constexpr wkc::core::Vec3 combatSteel{0.25F, 0.29F, 0.26F};
constexpr wkc::core::Vec3 combatCover{0.42F, 0.44F, 0.31F};
constexpr wkc::core::Vec3 combatAccent{0.65F, 0.52F, 0.20F};
constexpr wkc::core::Vec3 cafeteriaSteel{0.49F, 0.50F, 0.48F};
constexpr wkc::core::Vec3 cafeteriaSeat{0.48F, 0.18F, 0.16F};
constexpr wkc::core::Vec3 cafeteriaDark{0.24F, 0.26F, 0.26F};
constexpr wkc::core::Vec3 laboratoryCabinet{0.78F, 0.84F, 0.86F};
constexpr wkc::core::Vec3 laboratorySteel{0.31F, 0.43F, 0.50F};
constexpr wkc::core::Vec3 laboratoryAccent{0.34F, 0.72F, 0.78F};
constexpr wkc::core::Vec3 technicalCabinet{0.28F, 0.30F, 0.30F};
constexpr wkc::core::Vec3 technicalSteel{0.18F, 0.21F, 0.23F};
constexpr wkc::core::Vec3 technicalPipe{0.49F, 0.35F, 0.17F};

struct LocalFrame {
    wkc::core::Vec3 origin{};
    wkc::core::QuarterTurn rotation{wkc::core::QuarterTurn::Degrees0};
};

wkc::core::Aabb toWorldBounds(
    const wkc::core::Aabb local,
    const LocalFrame frame) noexcept
{
    const std::array corners{
        wkc::core::Vec3{local.minimum.x, local.minimum.y, local.minimum.z},
        wkc::core::Vec3{local.minimum.x, local.minimum.y, local.maximum.z},
        wkc::core::Vec3{local.maximum.x, local.minimum.y, local.minimum.z},
        wkc::core::Vec3{local.maximum.x, local.minimum.y, local.maximum.z},
    };
    wkc::core::Vec3 minimum = frame.origin
        + wkc::core::rotateQuarterTurns(corners.front(), frame.rotation);
    wkc::core::Vec3 maximum = minimum;
    for (const wkc::core::Vec3 corner : corners) {
        const wkc::core::Vec3 world = frame.origin
            + wkc::core::rotateQuarterTurns(corner, frame.rotation);
        minimum.x = std::min(minimum.x, world.x);
        minimum.z = std::min(minimum.z, world.z);
        maximum.x = std::max(maximum.x, world.x);
        maximum.z = std::max(maximum.z, world.z);
    }
    minimum.y = frame.origin.y + local.minimum.y;
    maximum.y = frame.origin.y + local.maximum.y;
    return {minimum, maximum};
}

void addPart(
    std::vector<RoomArchitecturePart>& result,
    const LocalFrame frame,
    const wkc::core::Aabb local,
    const wkc::core::Vec3 tint,
    const bool solid,
    const bool breakable = false,
    const bool storageRack = false,
    const StorageRackPart storageRackPart = StorageRackPart::None,
    const CafeteriaFurniturePart cafeteriaFurniturePart
        = CafeteriaFurniturePart::None)
{
    result.push_back({toWorldBounds(local, frame), tint, solid, breakable,
        storageRack, storageRackPart, cafeteriaFurniturePart});
}

void addRackStructure(
    std::vector<RoomArchitecturePart>& parts,
    const LocalFrame frame,
    const float xMinimum,
    const float xMaximum,
    const float zMinimum,
    const float zMaximum,
    const float top,
    const bool frontAtMinimumX,
    const bool twoSided)
{
    constexpr float postThickness = 0.12F;
    constexpr float lipThickness = 0.055F;
    constexpr std::array<float, 3> levels{0.46F, 1.42F, 2.38F};

    // Four square corner posts replace the old full-depth end panels. Shelves
    // meet their inside Z faces, without an open-volume overlap or a gap.
    for (const float x : {xMinimum, xMaximum - postThickness}) {
        for (const float z : {zMinimum, zMaximum - postThickness}) {
            addPart(parts, frame,
                {{x, 0.0F, z},
                    {x + postThickness, top, z + postThickness}},
                darkSteel, true, false, true, StorageRackPart::Upright);
        }
    }

    const float innerZMinimum = zMinimum + postThickness;
    const float innerZMaximum = zMaximum - postThickness;
    const float shelfXMinimum = xMinimum
        + ((frontAtMinimumX || twoSided) ? lipThickness : 0.0F);
    const float shelfXMaximum = xMaximum
        - ((!frontAtMinimumX || twoSided) ? lipThickness : 0.0F);
    for (std::size_t index = 0U; index < levels.size(); ++index) {
        const float level = levels[index];
        const float shelfTop = level + 0.11F;
        if (shelfTop + 0.45F > top) {
            break;
        }
        addPart(parts, frame,
            {{shelfXMinimum, level, innerZMinimum},
                {shelfXMaximum, shelfTop, innerZMaximum}},
            shelfSteel, true, false, true, StorageRackPart::Shelf);

        // The lip is a separate deeper beam. Its upper edge is flush with the
        // tray, so the material seam reads without z fighting.
        const auto addLip = [&](const float firstX, const float lastX) {
            addPart(parts, frame,
                {{firstX, level - 0.08F, innerZMinimum},
                    {lastX, shelfTop, innerZMaximum}},
                rackLipSteel, true, false, true, StorageRackPart::Lip);
        };
        if (frontAtMinimumX || twoSided) {
            addLip(xMinimum, xMinimum + lipThickness);
        }
        if (!frontAtMinimumX || twoSided) {
            addLip(xMaximum - lipThickness, xMaximum);
        }

        // Two short brackets at the middle tier connect the tray underside
        // to the end posts. Their outer faces meet the post and tray faces.
        if (index == 1U) {
            const float bracketXMinimum = frontAtMinimumX
                ? xMinimum + lipThickness
                : xMaximum - lipThickness - 0.13F;
            const float bracketXMaximum = bracketXMinimum + 0.13F;
            for (const float z : {innerZMinimum,
                     innerZMaximum - 0.15F}) {
                addPart(parts, frame,
                    {{bracketXMinimum, level - 0.13F, z},
                        {bracketXMaximum, level, z + 0.15F}},
                    rackLipSteel, true, false, true,
                    StorageRackPart::Bracket);
            }
        }
    }
}

void addRackBay(
    std::vector<RoomArchitecturePart>& parts,
    const LocalFrame frame,
    const float halfX,
    const float height,
    const float zMinimum,
    const float zMaximum,
    const float side)
{
    constexpr float wallGap = 0.20F;
    constexpr float rackDepth = 0.58F;
    const float inner = halfX - wallGap - rackDepth;
    const float outer = halfX - wallGap;
    const float xMinimum = side > 0.0F ? inner : -outer;
    const float xMaximum = side > 0.0F ? outer : -inner;
    const float top = std::min(height - 0.55F, 3.45F);

    addRackStructure(parts, frame, xMinimum, xMaximum, zMinimum, zMaximum,
        top, side > 0.0F, false);
    constexpr std::array<float, 3> levels{0.46F, 1.42F, 2.38F};
    for (std::size_t index = 0U; index < levels.size(); ++index) {
        const float shelfTop = levels[index] + 0.11F;
        if (shelfTop + 0.45F > top) {
            break;
        }
        // Offset crates by tier so the silhouette reads as stocked shelving,
        // without filling the entire bay with geometry or blocking sightlines.
        const float bayLength = zMaximum - zMinimum;
        const float cargoLength = std::min(0.82F, bayLength * 0.40F);
        const float centerZ = zMinimum + bayLength
            * (index % 2U == 0U ? 0.38F : 0.62F);
        addPart(parts, frame,
            {{xMinimum + 0.09F, shelfTop, centerZ - cargoLength * 0.5F},
                {xMaximum - 0.09F, shelfTop + 0.44F,
                    centerZ + cargoLength * 0.5F}},
            index == 1U ? charcoalCargo : oliveCargo, true, true);
    }
}

void addIslandBay(
    std::vector<RoomArchitecturePart>& parts,
    const LocalFrame frame,
    const float centerX,
    const float centerZ,
    const float height)
{
    constexpr float halfWidth = 0.50F;
    constexpr float halfLength = 1.05F;
    const float xMinimum = centerX - halfWidth;
    const float xMaximum = centerX + halfWidth;
    const float zMinimum = centerZ - halfLength;
    const float zMaximum = centerZ + halfLength;
    const float top = std::min(height - 0.55F, 3.15F);

    addRackStructure(parts, frame, xMinimum, xMaximum, zMinimum, zMaximum,
        top, true, true);
    constexpr std::array<float, 3> levels{0.46F, 1.42F, 2.38F};
    for (std::size_t index = 0U; index < levels.size(); ++index) {
        const float shelfTop = levels[index] + 0.11F;
        const float cargoCenterZ = centerZ
            + (index % 2U == 0U ? -0.35F : 0.35F);
        addPart(parts, frame,
            {{xMinimum + 0.10F, shelfTop, cargoCenterZ - 0.36F},
                {xMaximum - 0.10F, shelfTop + 0.44F,
                    cargoCenterZ + 0.36F}},
            index == 1U ? charcoalCargo : oliveCargo, true, true);
    }
}

bool overlapsOpen(const wkc::core::Aabb a, const wkc::core::Aabb b) noexcept
{
    return a.minimum.x < b.maximum.x && a.maximum.x > b.minimum.x
        && a.minimum.y < b.maximum.y && a.maximum.y > b.minimum.y
        && a.minimum.z < b.maximum.z && a.maximum.z > b.minimum.z;
}

bool clearOfZones(
    const wkc::core::Aabb solid,
    const std::span<const SpawnZone> zones,
    const float margin,
    const float floor) noexcept
{
    for (const SpawnZone& zone : zones) {
        const wkc::core::Aabb occupied{
            {zone.bounds.minimum.x - margin, floor,
                zone.bounds.minimum.z - margin},
            {zone.bounds.maximum.x + margin, floor + 2.2F,
                zone.bounds.maximum.z + margin},
        };
        if (overlapsOpen(solid, occupied)) {
            return false;
        }
    }
    return true;
}

bool clearOfConnectors(
    const wkc::core::Aabb solid,
    const std::span<const ConnectorInstance> connectors) noexcept
{
    for (const ConnectorInstance& connector : connectors) {
        const auto& p = connector.worldPosition;
        const auto& n = connector.worldNormal;
        const float halfWidth = connector.width * 0.5F + 0.30F;
        constexpr float depth = 0.95F;
        const wkc::core::Aabb opening = std::abs(n.x) > 0.5F
            ? wkc::core::Aabb{
                {p.x - (n.x > 0.0F ? depth : 0.0F), p.y,
                    p.z - halfWidth},
                {p.x + (n.x < 0.0F ? depth : 0.0F),
                    p.y + connector.height, p.z + halfWidth}}
            : wkc::core::Aabb{
                {p.x - halfWidth, p.y,
                    p.z - (n.z > 0.0F ? depth : 0.0F)},
                {p.x + halfWidth, p.y + connector.height,
                    p.z + (n.z < 0.0F ? depth : 0.0F)}};
        if (overlapsOpen(solid, opening)) {
            return false;
        }
    }
    return true;
}

wkc::core::Vec3 wallColumnTint(const RoomTheme theme) noexcept
{
    switch (theme) {
    case RoomTheme::Laboratory:
        return laboratorySteel;
    case RoomTheme::Cafeteria:
        // Warm concrete/painted steel keeps the structural texture legible
        // without introducing a cold blue-grey stripe into the dining room.
        return {0.48F, 0.45F, 0.42F};
    case RoomTheme::Office:
        return cafeteriaDark;
    case RoomTheme::Maintenance:
        return technicalSteel;
    case RoomTheme::Security:
        return combatSteel;
    case RoomTheme::Industrial:
    case RoomTheme::Storage:
        return darkSteel;
    }
    return darkSteel;
}

std::vector<RoomArchitecturePart> makeCombatPreviewArchitecture(
    const RoomInstance& room)
{
    const wkc::core::Vec3 span = room.worldBounds.maximum
        - room.worldBounds.minimum;
    if (std::min(span.x, span.z) < 19.5F
        || std::max(span.x, span.z) < 21.5F || span.y < 5.8F) {
        return {};
    }
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    std::vector<RoomArchitecturePart> parts;
    parts.reserve(64U);

    // Low, staggered cover leaves a 4 m central lane and two wider flanks.
    for (const float side : {-1.0F, 1.0F}) {
        const float frontZ = side > 0.0F ? -3.6F : 3.6F;
        addPart(parts, frame,
            {{side * 3.2F - 0.85F, 0.0F, frontZ - 0.95F},
                {side * 3.2F + 0.85F, 1.25F, frontZ + 0.95F}},
            combatCover, true);
        addPart(parts, frame,
            {{side * 6.9F - 0.75F, 0.0F, -1.15F},
                {side * 6.9F + 0.75F, 1.85F, 1.15F}},
            combatSteel, true);
        // Elevated side galleries echo the reference silhouette. They sit
        // above the door head and remain outside the ground combat lanes.
        const float galleryMinX = side > 0.0F ? 8.5F : -9.95F;
        const float galleryMaxX = side > 0.0F ? 9.95F : -8.5F;
        addPart(parts, frame,
            {{galleryMinX, 3.15F, -6.4F},
                {galleryMaxX, 3.32F, 3.4F}},
            combatSteel, true);
        const float railX = side * 8.5F;
        for (const float railZ : {-5.8F, 3.15F}) {
            addPart(parts, frame,
                {{railX - 0.06F, 3.33F, railZ - 0.06F},
                    {railX + 0.06F, 4.05F, railZ + 0.06F}},
                combatAccent, true);
        }
        addPart(parts, frame,
            {{railX - 0.05F, 3.95F, -5.8F},
                {railX + 0.05F, 4.05F, 3.15F}},
            combatAccent, true);
        // A shorter run follows the same wall as the gallery. Thin treads
        // and narrow wall brackets leave visible open space below the stairs.
        constexpr float treadRise = 3.32F / 14.0F;
        for (int tread = 0; tread < 14; ++tread) {
            const float nearZ = 9.0F - static_cast<float>(tread) * 0.4F;
            const float top = (static_cast<float>(tread) + 1.0F) * treadRise;
            addPart(parts, frame,
                {{galleryMinX, top - 0.10F, nearZ - 0.4F},
                    {galleryMaxX, top, nearZ}},
                combatSteel, true);
            addPart(parts, frame,
                {{side > 0.0F ? 9.1F : -10.0F, top - 0.18F,
                     nearZ - 0.26F},
                    {side > 0.0F ? 10.0F : -9.1F, top - 0.10F,
                        nearZ - 0.14F}},
                combatSteel, true);
        }
        // Wall-side supports and overhead ribs provide height without
        // narrowing the central sightline or blocking a doorway.
        addPart(parts, frame,
            {{side * 9.35F - 0.18F, 0.0F, -8.02F},
                {side * 9.35F + 0.18F, 5.65F, -7.58F}},
            combatSteel, true);
    }
    for (const float z : {-7.8F, 7.8F}) {
        addPart(parts, frame,
            {{-9.35F, 5.42F, z - 0.16F},
                {9.35F, 5.65F, z + 0.16F}},
            combatAccent, true);
    }
    return parts;
}

void addDiningGroup(
    std::vector<RoomArchitecturePart>& parts,
    const LocalFrame frame,
    const float x,
    const float z)
{
    // A shared steel table with two continuous benches; legs and seat
    // supports are collidable so shots cannot pass through their silhouette.
    addPart(parts, frame,
        {{x - 1.05F, 0.70F, z - 0.66F},
            {x + 1.05F, 0.80F, z + 0.66F}},
        cafeteriaSteel, true, false, false, StorageRackPart::None,
        CafeteriaFurniturePart::Steel);
    for (const float railZ : {z - 0.43F, z + 0.43F}) {
        addPart(parts, frame,
            {{x - 0.72F, 0.62F, railZ - 0.04F},
                {x + 0.72F, 0.70F, railZ + 0.04F}},
            cafeteriaDark, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Support);
    }
    for (const float legX : {x - 0.78F, x + 0.78F}) {
        for (const float legZ : {z - 0.43F, z + 0.43F}) {
            addPart(parts, frame,
                {{legX - 0.06F, 0.0F, legZ - 0.06F},
                    {legX + 0.06F, 0.70F, legZ + 0.06F}},
                cafeteriaDark, true, false, false, StorageRackPart::None,
                CafeteriaFurniturePart::Support);
        }
    }
    for (const float side : {-1.0F, 1.0F}) {
        const float seatZ = z + side * 1.05F;
        addPart(parts, frame,
            {{x - 1.10F, 0.43F, seatZ - 0.19F},
                {x + 1.10F, 0.52F, seatZ + 0.19F}},
            cafeteriaSeat, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Seat);
        addPart(parts, frame,
            {{x - 0.70F, 0.36F, seatZ - 0.04F},
                {x + 0.70F, 0.43F, seatZ + 0.04F}},
            cafeteriaDark, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Support);
        for (const float legX : {x - 0.76F, x + 0.76F}) {
            addPart(parts, frame,
                {{legX - 0.06F, 0.0F, seatZ - 0.06F},
                    {legX + 0.06F, 0.43F, seatZ + 0.06F}},
                cafeteriaDark, true, false, false, StorageRackPart::None,
                CafeteriaFurniturePart::Support);
            // A raised washer, bolt head and recessed slot mark the physical
            // connection between the bench plank and each support.
            addPart(parts, frame,
                {{legX - 0.035F, 0.52F, seatZ - 0.035F},
                    {legX + 0.035F, 0.528F, seatZ + 0.035F}},
                cafeteriaDark, true, false, false, StorageRackPart::None,
                CafeteriaFurniturePart::Support);
            addPart(parts, frame,
                {{legX - 0.025F, 0.528F, seatZ - 0.025F},
                    {legX + 0.025F, 0.543F, seatZ + 0.025F}},
                cafeteriaSteel, true, false, false, StorageRackPart::None,
                CafeteriaFurniturePart::Steel);
            addPart(parts, frame,
                {{legX - 0.005F, 0.543F, seatZ - 0.018F},
                    {legX + 0.005F, 0.546F, seatZ + 0.018F}},
                cafeteriaDark, true, false, false, StorageRackPart::None,
                CafeteriaFurniturePart::Support);
        }
    }
}

std::vector<RoomArchitecturePart> makeCafeteriaPreviewArchitecture(
    const RoomInstance& room)
{
    const wkc::core::Vec3 span = room.worldBounds.maximum
        - room.worldBounds.minimum;
    if (std::min(span.x, span.z) < 17.5F
        || std::max(span.x, span.z) < 19.5F || span.y < 5.8F) {
        return {};
    }
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    std::vector<RoomArchitecturePart> parts;
    parts.reserve(224U);
    const bool longRoom = room.moduleId == cafeteriaLongPreviewModuleId
        || room.moduleId == cafeteriaLongModuleId;
    for (const float x : {-4.6F, 3.0F}) {
        if (longRoom) {
            for (const float z : {-7.0F, 0.0F, 7.0F}) {
                addDiningGroup(parts, frame, x, z);
            }
        } else {
            for (const float z : {-4.3F, 4.3F}) {
                addDiningGroup(parts, frame, x, z);
            }
        }
    }

    // Split the entire service line at the side door. A continuous counter
    // trapped a player who reached the pocket behind its middle section.
    // Cabinets meet the counter and the wall on each side of this aisle.
    for (const float side : {-1.0F, 1.0F}) {
        const float minimumZ = side < 0.0F
            ? (longRoom ? -8.5F : -6.1F) : 1.8F;
        const float maximumZ = side < 0.0F
            ? -1.8F : (longRoom ? 8.5F : 6.1F);
        addPart(parts, frame,
            {{6.0F, 0.0F, minimumZ},
                {7.3F, 1.15F, maximumZ}},
            cafeteriaDark, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Cabinet);
        addPart(parts, frame,
            {{5.85F, 1.15F, minimumZ},
                {7.45F, 1.26F, maximumZ}},
            cafeteriaSteel, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Steel);
        addPart(parts, frame,
            {{7.3F, 0.0F, minimumZ},
                {8.85F, 1.15F, maximumZ}},
            cafeteriaDark, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Cabinet);

        // An open service niche replaces the featureless upper block. The
        // short rear casing, shelf and canopy all remain inside its footprint.
        addPart(parts, frame,
            {{8.21F, 1.15F, minimumZ},
                {8.85F, 2.14F, maximumZ}},
            cafeteriaDark, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Cabinet);
        addPart(parts, frame,
            {{7.35F, 2.14F, minimumZ},
                {8.85F, 2.25F, maximumZ}},
            cafeteriaSteel, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Steel);
        addPart(parts, frame,
            {{7.46F, 1.70F, minimumZ + 0.15F},
                {8.21F, 1.77F, maximumZ - 0.15F}},
            cafeteriaSteel, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Steel);
        for (const float z : {minimumZ + 0.13F, maximumZ - 0.19F}) {
            addPart(parts, frame,
                {{7.37F, 1.26F, z},
                    {7.43F, 2.14F, z + 0.06F}},
                cafeteriaSteel, true, false, false, StorageRackPart::None,
                CafeteriaFurniturePart::Steel);
        }

        // The front service face is built from short physical details, not
        // stretched artwork. Each segment still stops before the side door.
        addPart(parts, frame,
            {{5.95F, 0.08F, minimumZ + 0.05F},
                {6.01F, 0.20F, maximumZ - 0.05F}},
            cafeteriaSteel, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Steel);
        addPart(parts, frame,
            {{5.95F, 1.02F, minimumZ + 0.05F},
                {6.01F, 1.10F, maximumZ - 0.05F}},
            cafeteriaSeat, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Seat);
        constexpr float doorPitch = 1.04F;
        for (int door = 0; door < (longRoom ? 6 : 4); ++door) {
            const float doorStart = minimumZ + 0.09F
                + static_cast<float>(door) * doorPitch;
            const float doorEnd = doorStart + 0.95F;
            addPart(parts, frame,
                {{5.94F, 0.25F, doorStart},
                    {6.01F, 0.97F, doorEnd}},
                cafeteriaDark, true, false, false, StorageRackPart::None,
                CafeteriaFurniturePart::Support);
            addPart(parts, frame,
                {{5.90F, 0.76F, doorEnd - 0.19F},
                    {5.94F, 0.80F, doorEnd - 0.06F}},
                cafeteriaSteel, true, false, false, StorageRackPart::None,
                CafeteriaFurniturePart::Steel);
        }

        // A shallow serving tray gives the steel worktop a readable purpose.
        // Its rim and inset have the same solid geometry used for hit tests.
        const float trayStart = minimumZ + 0.87F;
        const float trayEnd = maximumZ - 0.87F;
        addPart(parts, frame,
            {{6.27F, 1.26F, trayStart},
                {7.06F, 1.29F, trayEnd}},
            cafeteriaDark, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Support);
        for (const float x : {6.25F, 7.04F}) {
            addPart(parts, frame,
                {{x, 1.29F, trayStart + 0.02F},
                    {x + 0.04F, 1.34F, trayEnd - 0.02F}},
                cafeteriaSteel, true, false, false, StorageRackPart::None,
                CafeteriaFurniturePart::Steel);
        }
        for (const float z : {trayStart - 0.02F, trayEnd - 0.02F}) {
            addPart(parts, frame,
                {{6.25F, 1.29F, z},
                    {7.08F, 1.34F, z + 0.04F}},
                cafeteriaSteel, true, false, false, StorageRackPart::None,
                CafeteriaFurniturePart::Steel);
        }
    }
    if (longRoom) {
        // Kitchen exhaust follows the service line, clear of the four
        // suspended luminaires and the doorway in the middle of the counter.
        addPart(parts, frame,
            {{7.05F, 5.18F, -11.4F}, {7.75F, 5.58F, 11.4F}},
            cafeteriaDark, true);
        parts.back().decoration = RoomDecorationPart::CeilingDuct;
        // Broad, low foliage softens the dining end corners. The solid pot
        // stays outside the table and doorway circulation lanes.
        for (const float z : {-10.7F, 10.7F}) {
            addPart(parts, frame,
                {{-8.15F, 0.0F, z - 0.35F},
                    {-7.45F, 0.68F, z + 0.35F}},
                cafeteriaDark, true);
            parts.back().decoration = RoomDecorationPart::BroadleafPlanter;
        }
    }
    return parts;
}

std::vector<RoomArchitecturePart> makeLaboratoryPreviewArchitecture(
    const RoomInstance& room)
{
    const wkc::core::Vec3 span = room.worldBounds.maximum
        - room.worldBounds.minimum;
    const bool quarterTurn = room.rotation == wkc::core::QuarterTurn::Degrees90
        || room.rotation == wkc::core::QuarterTurn::Degrees270;
    const float localWidth = quarterTurn ? span.z : span.x;
    const float localLength = quarterTurn ? span.x : span.z;
    if (localWidth < 18.0F || localLength < 20.0F || span.y < 5.8F) {
        return {};
    }
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    std::vector<RoomArchitecturePart> parts;
    parts.reserve(44U);

    // Each side installation touches the side wall. There is no narrow pocket
    // behind the benches or between a bench and its sealed equipment cabinet.
    // The split around Z=0 gives both side doors a clear approach to the
    // broad central route, which also reaches the two end doors.
    for (const float side : {-1.0F, 1.0F}) {
        const float wallSideX = side > 0.0F ? 9.0F : -9.0F;
        const float aisleSideX = side > 0.0F ? 7.10F : -7.10F;
        const float minimumX = std::min(wallSideX, aisleSideX);
        const float maximumX = std::max(wallSideX, aisleSideX);
        const float innerFaceX = side > 0.0F ? minimumX : maximumX;
        for (const float end : {-1.0F, 1.0F}) {
            const float benchMinimumZ = end > 0.0F ? 2.5F : -5.6F;
            const float benchMaximumZ = end > 0.0F ? 5.6F : -2.5F;
            const float tankMinimumZ = end > 0.0F ? 5.6F : -8.3F;
            const float tankMaximumZ = end > 0.0F ? 8.3F : -5.6F;

            addPart(parts, frame,
                {{minimumX, 0.0F, benchMinimumZ},
                    {maximumX, 0.86F, benchMaximumZ}},
                laboratoryCabinet, true);
            parts.back().laboratoryFurniturePart = LaboratoryFurniturePart::BenchBase;
            addPart(parts, frame,
                {{minimumX, 0.86F, benchMinimumZ},
                    {maximumX, 0.98F, benchMaximumZ}},
                laboratorySteel, true);
            parts.back().laboratoryFurniturePart = LaboratoryFurniturePart::BenchTop;

            // Opaque bench instruments are short enough to keep the long
            // central sightline open and never imply nonexistent glass.
            const float instrumentMinimumX = side > 0.0F
                ? 8.08F : -8.92F;
            const float instrumentMaximumX = side > 0.0F
                ? 8.92F : -8.08F;
            const float instrumentMinimumZ = end > 0.0F ? 3.15F : -4.70F;
            addPart(parts, frame,
                {{instrumentMinimumX, 0.98F, instrumentMinimumZ},
                    {instrumentMaximumX, 1.57F, instrumentMinimumZ + 1.55F}},
                laboratoryCabinet, true);
            parts.back().laboratoryFurniturePart = LaboratoryFurniturePart::Instrument;
            // A solid status strip on the aisle face distinguishes the
            // instrument from a plain block without pretending transparency.
            const float stripMinimumX = side > 0.0F
                ? instrumentMinimumX - 0.045F : instrumentMaximumX;
            addPart(parts, frame,
                {{stripMinimumX, 1.27F, instrumentMinimumZ + 0.20F},
                    {stripMinimumX + 0.045F, 1.41F,
                        instrumentMinimumZ + 1.35F}},
                laboratoryAccent, true);
            parts.back().laboratoryFurniturePart = LaboratoryFurniturePart::StatusStrip;

            // The containment unit is explicitly sealed and opaque. It
            // meets the bench at Z=5.6 and the wall at X=9, eliminating the
            // small dead-end gap a player could otherwise enter.
            addPart(parts, frame,
                {{minimumX, 0.0F, tankMinimumZ},
                    {maximumX, 0.24F, tankMaximumZ}},
                laboratorySteel, true);
            parts.back().laboratoryFurniturePart = LaboratoryFurniturePart::ContainmentBase;
            addPart(parts, frame,
                {{minimumX, 0.24F, tankMinimumZ},
                    {maximumX, 2.40F, tankMaximumZ}},
                laboratoryCabinet, true);
            parts.back().laboratoryFurniturePart = LaboratoryFurniturePart::ContainmentBody;
            const float panelMinimumX = side > 0.0F
                ? innerFaceX - 0.055F : innerFaceX;
            addPart(parts, frame,
                {{panelMinimumX, 0.78F, tankMinimumZ + 0.40F},
                    {panelMinimumX + 0.055F, 1.80F,
                        tankMaximumZ - 0.40F}},
                laboratorySteel, true);
            parts.back().laboratoryFurniturePart = LaboratoryFurniturePart::ContainmentPanel;
        }
    }

    // Two low, staggered island benches provide central cover. Their tops
    // remain outside the door-to-door crossing (|Z| < 1.8), the central
    // corridor and both side lanes (at least 1.75 m clear, including the
    // equipment face panels). Solid bases rule out a crawl-space
    // pocket under a tabletop; the separate tops meet them without overlap.
    for (const wkc::core::Vec3 benchCenter : {
             wkc::core::Vec3{-4.40F, 0.0F, -3.90F},
             wkc::core::Vec3{4.40F, 0.0F, 3.90F}}) {
        addPart(parts, frame,
            {{benchCenter.x - 0.65F, 0.0F, benchCenter.z - 1.05F},
                {benchCenter.x + 0.65F, 0.78F,
                    benchCenter.z + 1.05F}},
            laboratoryCabinet, true);
        parts.back().laboratoryFurniturePart = LaboratoryFurniturePart::BenchBase;
        addPart(parts, frame,
            {{benchCenter.x - 0.80F, 0.78F, benchCenter.z - 1.20F},
                {benchCenter.x + 0.80F, 0.92F,
                    benchCenter.z + 1.20F}},
            laboratorySteel, true);
        parts.back().laboratoryFurniturePart = LaboratoryFurniturePart::BenchTop;
        // Two compact specimen pots sit entirely on each top. Their bases
        // touch the tabletop and share the authored collision; glass is visual.
        for (const float offsetZ : {-0.66F, 0.66F}) {
            addPart(parts, frame,
                {{benchCenter.x - 0.22F, 0.92F,
                    benchCenter.z + offsetZ - 0.22F},
                    {benchCenter.x + 0.22F, 1.08F,
                        benchCenter.z + offsetZ + 0.22F}},
                laboratorySteel, true);
            parts.back().decoration = RoomDecorationPart::SmallGlassTestPlanter;
        }
    }

    // High transverse ribs give the 6 m room a readable ceiling profile.
    // They sit above connector heads and leave the entire floor route open.
    for (const float z : {-8.75F, 8.75F}) {
        addPart(parts, frame,
            {{-8.85F, 5.38F, z - 0.12F},
                {8.85F, 5.62F, z + 0.12F}},
            laboratorySteel, true);
        parts.back().laboratoryFurniturePart = LaboratoryFurniturePart::CeilingRib;
    }
    // Two suspended extraction runs keep the lab's floor routes clear. The
    // renderer adds grilles underneath each duct using the shared 3D detail.
    for (const float x : {-5.85F, 5.85F}) {
        addPart(parts, frame,
            {{x - 0.38F, 5.08F, -7.15F},
                {x + 0.38F, 5.42F, 7.15F}},
            laboratorySteel, true);
        parts.back().decoration = RoomDecorationPart::CeilingDuct;
    }
    // The test specimen is sealed in the rear corner. Its base meets the
    // wall and the containment bank, so it cannot create a narrow pocket.
    addPart(parts, frame,
        {{-9.0F, 0.0F, 8.31F}, {-7.8F, 0.42F, 10.0F}},
        laboratorySteel, true);
    parts.back().decoration = RoomDecorationPart::GlassTestPlanter;
    return parts;
}

std::vector<RoomArchitecturePart> makeTechnicalPreviewArchitecture(
    const RoomInstance& room)
{
    const wkc::core::Vec3 span = room.worldBounds.maximum
        - room.worldBounds.minimum;
    const bool quarterTurn = room.rotation == wkc::core::QuarterTurn::Degrees90
        || room.rotation == wkc::core::QuarterTurn::Degrees270;
    const float localWidth = quarterTurn ? span.z : span.x;
    const float localLength = quarterTurn ? span.x : span.z;
    if (localWidth < 16.0F || localLength < 20.0F || span.y < 5.5F) {
        return {};
    }
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    std::vector<RoomArchitecturePart> parts;
    parts.reserve(40U);

    // Wall-connected bench and generator runs leave no gap behind them. Their
    // split at |Z|=2.4 preserves the two side entrances and a transverse
    // route; the end doors feed the full-length central corridor.
    for (const float side : {-1.0F, 1.0F}) {
        const float minimumX = side > 0.0F ? 6.4F : -8.0F;
        const float maximumX = side > 0.0F ? 8.0F : -6.4F;
        for (const float end : {-1.0F, 1.0F}) {
            const float benchMinZ = end > 0.0F ? 2.4F : -5.4F;
            const float benchMaxZ = end > 0.0F ? 5.4F : -2.4F;
            const float generatorMinZ = end > 0.0F ? 5.4F : -8.2F;
            const float generatorMaxZ = end > 0.0F ? 8.2F : -5.4F;
            addPart(parts, frame,
                {{minimumX, 0.0F, benchMinZ},
                    {maximumX, 0.84F, benchMaxZ}},
                technicalCabinet, true);
            addPart(parts, frame,
                {{minimumX, 0.84F, benchMinZ},
                    {maximumX, 0.98F, benchMaxZ}},
                technicalSteel, true);

            const float toolMinX = side > 0.0F ? 7.05F : -7.75F;
            const float toolMaxX = side > 0.0F ? 7.75F : -7.05F;
            const float toolMinZ = end > 0.0F ? 3.2F : -4.7F;
            addPart(parts, frame,
                {{toolMinX, 0.98F, toolMinZ},
                    {toolMaxX, 1.50F, toolMinZ + 1.50F}},
                technicalCabinet, true);

            // The heavy generator is sealed and opaque. Its plinth touches
            // both bench and wall, keeping collision equal to its silhouette.
            addPart(parts, frame,
                {{minimumX, 0.0F, generatorMinZ},
                    {maximumX, 0.25F, generatorMaxZ}},
                technicalSteel, true);
            addPart(parts, frame,
                {{minimumX, 0.25F, generatorMinZ},
                    {maximumX, 2.25F, generatorMaxZ}},
                technicalCabinet, true);
            const float panelMinX = side > 0.0F
                ? minimumX - 0.05F : maximumX;
            addPart(parts, frame,
                {{panelMinX, 0.75F, generatorMinZ + 0.38F},
                    {panelMinX + 0.05F, 1.72F,
                        generatorMaxZ - 0.38F}},
                technicalPipe, true);
        }

        // A pipe run remains near the wall and above player head height.
        // Two drops meet its ends, without overlapping the machinery below.
        const float pipeMinX = side > 0.0F ? 7.68F : -7.82F;
        const float pipeMaxX = pipeMinX + 0.14F;
        addPart(parts, frame,
            {{pipeMinX, 4.62F, -8.50F},
                {pipeMaxX, 4.78F, 8.50F}},
            technicalPipe, true);
        for (const float z : {-8.62F, 8.50F}) {
            addPart(parts, frame,
                {{pipeMinX, 0.0F, z},
                    {pipeMaxX, 4.78F, z + 0.12F}},
                technicalPipe, true);
        }
    }

    // Two staggered maintenance stations are closer to the room centre but
    // leave a 4.4 m straight route between their tops. Solid bases avoid an
    // under-bench pocket; each top and instrument meets the part below it.
    for (const wkc::core::Vec3 center : {
             wkc::core::Vec3{-3.0F, 0.0F, -3.80F},
             wkc::core::Vec3{3.0F, 0.0F, 3.80F}}) {
        addPart(parts, frame,
            {{center.x - 0.65F, 0.0F, center.z - 1.05F},
                {center.x + 0.65F, 0.78F, center.z + 1.05F}},
            technicalCabinet, true);
        addPart(parts, frame,
            {{center.x - 0.80F, 0.78F, center.z - 1.20F},
                {center.x + 0.80F, 0.92F, center.z + 1.20F}},
            technicalSteel, true);
        const float instrumentX = center.x
            + (center.x > 0.0F ? 0.25F : -0.25F);
        addPart(parts, frame,
            {{instrumentX - 0.26F, 0.92F, center.z - 0.35F},
                {instrumentX + 0.26F, 1.32F, center.z + 0.35F}},
            center.x < 0.0F ? oliveCargo : charcoalCargo,
            true, true);
    }
    return parts;
}

std::vector<RoomArchitecturePart> makeSecurityPreviewArchitecture(
    const RoomInstance& room)
{
    const wkc::core::Vec3 span = room.worldBounds.maximum
        - room.worldBounds.minimum;
    if (span.x < 18.0F || span.z < 18.0F || span.y < 6.0F) {
        return {};
    }
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    std::vector<RoomArchitecturePart> parts;
    parts.reserve(20U);

    // Two rear-wall stations flank the Z=-9 door with a 6.8 m opening. The
    // control desk uses separate base, top, screen and input panel solids.
    addPart(parts, frame,
        {{-8.85F, 0.0F, -9.0F}, {-3.40F, 0.82F, -7.50F}},
        darkSteel, true);
    addPart(parts, frame,
        {{-8.85F, 0.82F, -9.0F}, {-3.40F, 0.96F, -7.50F}},
        combatSteel, true);
    addPart(parts, frame,
        {{-7.50F, 0.96F, -8.80F}, {-5.40F, 1.68F, -8.65F}},
        cafeteriaDark, true);
    addPart(parts, frame,
        {{-5.00F, 0.96F, -8.80F}, {-3.90F, 1.58F, -8.65F}},
        cafeteriaDark, true);
    addPart(parts, frame,
        {{-5.00F, 0.96F, -8.30F}, {-3.90F, 1.04F, -7.80F}},
        cafeteriaDark, true);

    addPart(parts, frame,
        {{3.40F, 0.0F, -9.0F}, {8.85F, 1.35F, -7.50F}},
        darkSteel, true);
    addPart(parts, frame,
        {{3.40F, 1.35F, -9.0F}, {8.85F, 1.47F, -7.50F}},
        combatSteel, true);
    addPart(parts, frame,
        {{4.50F, 0.80F, -7.50F}, {6.50F, 1.20F, -7.44F}},
        cafeteriaDark, true);

    // The side observation niche is a recessed, opaque wall feature. Low
    // base, upper header, two piers and a deep backing panel define its form
    // without a translucent surface or a floor-level pocket to enter.
    for (const float z : {2.40F, 5.60F}) {
        addPart(parts, frame,
            {{8.45F, 0.0F, z}, {9.0F, 3.50F, z + 0.20F}},
            combatSteel, true);
    }
    addPart(parts, frame,
        {{8.45F, 0.0F, 2.60F}, {9.0F, 1.15F, 5.60F}},
        darkSteel, true);
    addPart(parts, frame,
        {{8.45F, 3.25F, 2.60F}, {9.0F, 3.50F, 5.60F}},
        combatSteel, true);
    addPart(parts, frame,
        {{8.92F, 1.15F, 2.60F}, {9.0F, 3.25F, 5.60F}},
        cafeteriaDark, true);

    // A wall-connected cabinet and two low, staggered barricades provide
    // cover while leaving the central and side routes open at both ends.
    addPart(parts, frame,
        {{-9.0F, 0.0F, 3.0F}, {-7.35F, 1.55F, 5.0F}},
        darkSteel, true);
    for (const wkc::core::Vec3 center : {
             wkc::core::Vec3{-4.20F, 0.0F, 3.80F},
             wkc::core::Vec3{4.20F, 0.0F, -3.80F}}) {
        addPart(parts, frame,
            {{center.x - 0.75F, 0.0F, center.z - 0.90F},
                {center.x + 0.75F, 1.05F, center.z + 0.90F}},
            combatSteel, true);
    }
    return parts;
}

std::vector<RoomArchitecturePart> makeSecurityTPreviewArchitecture(
    const RoomInstance& room)
{
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    std::vector<RoomArchitecturePart> parts;
    parts.reserve(20U);
    // The narrow approach stays open. Its equipment is attached to the sides.
    for (const float side : {-1.0F, 1.0F}) {
        const float x = side < 0.0F ? -3.9F : 3.1F;
        addPart(parts, frame,
            {{x, 0.0F, -8.0F}, {x + 0.8F, 2.2F, -5.4F}},
            darkSteel, true);
        parts.back().securityFurniturePart = SecurityFurniturePart::Cabinet;
    }
    // A pair of wall-mounted control stations faces the wide gallery.
    for (const float side : {-1.0F, 1.0F}) {
        const float x = side < 0.0F ? -8.4F : 4.3F;
        addPart(parts, frame,
            {{x, 0.0F, 8.3F}, {x + 4.1F, 0.9F, 9.4F}},
            darkSteel, true);
        parts.back().securityFurniturePart = SecurityFurniturePart::ConsoleBase;
        addPart(parts, frame,
            {{x, 0.9F, 8.3F}, {x + 4.1F, 1.05F, 9.4F}},
            combatSteel, true);
        parts.back().securityFurniturePart = SecurityFurniturePart::ConsoleTop;
        addPart(parts, frame,
            {{x + 0.8F, 1.05F, 9.1F},
                {x + 3.3F, 1.95F, 9.4F}},
            cafeteriaDark, true);
        parts.back().securityFurniturePart = SecurityFurniturePart::Monitor;
    }
    // Low cover creates two lateral choices without blocking the T junction.
    for (const float x : {-6.3F, 6.3F}) {
        addPart(parts, frame,
            {{x - 0.8F, 0.0F, 1.7F}, {x + 0.8F, 1.1F, 3.5F}},
            combatSteel, true);
        parts.back().securityFurniturePart = SecurityFurniturePart::Barricade;
    }
    // Planters occupy the two broad-gallery corners, away from the approach,
    // control stations and cover. Only the low vessel is solid.
    for (const float x : {-8.9F, 8.9F}) {
        addPart(parts, frame,
            {{x - 0.35F, 0.0F, -1.9F},
                {x + 0.35F, 0.68F, -1.2F}},
            darkSteel, true);
        parts.back().decoration = RoomDecorationPart::Planter;
    }
    // These two sections meet at an L joint. The gallery lights are at
    // (+/-5.5, 4.0) and the approach light is at (0, -6.0).
    addPart(parts, frame,
        {{-9.65F, 5.18F, 7.0F}, {9.65F, 5.58F, 7.7F}},
        darkSteel, true);
    parts.back().decoration = RoomDecorationPart::CeilingDuct;
    addPart(parts, frame,
        {{2.45F, 5.18F, -9.65F}, {3.15F, 5.58F, 7.0F}},
        darkSteel, true);
    parts.back().decoration = RoomDecorationPart::CeilingDuct;
    return parts;
}

std::vector<RoomArchitecturePart> makeCentralHubPreviewArchitecture(
    const RoomInstance& room)
{
    const wkc::core::Vec3 span = room.worldBounds.maximum
        - room.worldBounds.minimum;
    if (span.x < 20.0F || span.z < 20.0F || span.y < 6.0F) {
        return {};
    }
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    std::vector<RoomArchitecturePart> parts;
    parts.reserve(12U);

    // The broad plinth, tall shaft and cap form one legible landmark. Their
    // faces meet without overlapping volumes, so rendering and collision
    // agree. Routes at least 2 m wide pass on all four sides of the plinth.
    addPart(parts, frame,
        {{-1.55F, 0.0F, -1.55F}, {1.55F, 0.35F, 1.55F}},
        darkSteel, true);
    parts.back().structuralColumn = true;
    parts.back().decoration = RoomDecorationPart::CentralHubColumnPlinth;
    addPart(parts, frame,
        {{-1.20F, 0.35F, -1.20F}, {1.20F, 5.55F, 1.20F}},
        combatSteel, true);
    parts.back().structuralColumn = true;
    parts.back().decoration = RoomDecorationPart::CentralHubColumnShaft;
    addPart(parts, frame,
        {{-1.45F, 5.55F, -1.45F}, {1.45F, 5.78F, 1.45F}},
        darkSteel, true);
    parts.back().structuralColumn = true;

    // Four compact direction plates borrow the existing cyan tint. They
    // touch the shaft faces and do not become a separate obstacle at floor
    // level. Navigation remains clear from every possible door.
    for (const float side : {-1.0F, 1.0F}) {
        const float panelMinX = side < 0.0F ? -1.34F : 1.20F;
        addPart(parts, frame,
            {{panelMinX, 1.85F, -0.45F},
                {panelMinX + 0.14F, 3.05F, 0.45F}},
            laboratoryAccent, true);
        parts.back().decoration = RoomDecorationPart::CentralHubRouteDisplay;
        const float panelMinZ = side < 0.0F ? -1.34F : 1.20F;
        addPart(parts, frame,
            {{-0.45F, 1.85F, panelMinZ},
                {0.45F, 3.05F, panelMinZ + 0.14F}},
            laboratoryAccent, true);
        parts.back().decoration = RoomDecorationPart::CentralHubRouteDisplay;
    }

    // Wall-attached pilasters echo the reference without creating a pocket
    // behind a free-standing support or obscuring the four door centres.
    if (room.walkableOutline.empty()) {
        for (const float side : {-1.0F, 1.0F}) {
            const float minimumX = side < 0.0F ? -10.0F : 9.65F;
            for (const float end : {-1.0F, 1.0F}) {
                const float minimumZ = end < 0.0F ? -6.95F : 6.65F;
                addPart(parts, frame,
                    {{minimumX, 0.0F, minimumZ},
                        {minimumX + 0.35F, 5.55F, minimumZ + 0.30F}},
                    darkSteel, true);
                parts.back().structuralColumn = true;
            }
        }
    }
    return parts;
}

} // namespace

std::vector<RoomArchitecturePart> makeRoomWallColumns(const RoomInstance& room)
{
    if (!room.worldBounds.isValid()
        || !room.walkableOutline.empty()
        || room.moduleId == centralHubPreviewModuleId) {
        return {};
    }
    const wkc::core::Vec3 span = room.worldBounds.maximum
        - room.worldBounds.minimum;
    const bool quarterTurn = room.rotation == wkc::core::QuarterTurn::Degrees90
        || room.rotation == wkc::core::QuarterTurn::Degrees270;
    const float halfX = (quarterTurn ? span.z : span.x) * 0.5F;
    const float halfZ = (quarterTurn ? span.x : span.z) * 0.5F;
    if (halfX < 3.0F || halfZ < 3.0F || span.y < 2.5F) {
        return {};
    }
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    const bool laboratoryColumn = room.moduleId == laboratoryPreviewModuleId
        || room.moduleId == laboratoryHallModuleId;
    const float width = laboratoryColumn ? 0.62F : 0.38F;
    const float depth = laboratoryColumn ? 0.32F : 0.26F;
    const auto authored = makeRoomArchitecture(room);
    std::vector<RoomArchitecturePart> columns;
    columns.reserve(24U);
    const float floor = room.worldBounds.minimum.y;
    for (int wall = 0; wall < 4; ++wall) {
        const bool xWall = wall < 2;
        const float halfLength = xWall ? halfZ : halfX;
        const float wallLength = halfLength * 2.0F;
        if (wallLength < 8.0F) {
            continue;
        }
        const int count = std::clamp(
            static_cast<int>(std::floor(wallLength / 4.0F)), 2, 6);
        for (int index = 0; index < count; ++index) {
            // Small seed-based offsets avoid identical mechanical spacing
            // across rooms while preserving the wall rhythm and rotations.
            const auto sample = wkc::core::deriveSeed(room.seed,
                0x57414C4C434F4CULL,
                static_cast<std::uint64_t>(wall * 16 + index));
            const float jitter = laboratoryColumn ? 0.0F
                : (static_cast<float>(sample % 101U) / 100.0F
                    - 0.5F) * 0.40F;
            const float along = -halfLength
                + wallLength * static_cast<float>(index + 1)
                    / static_cast<float>(count + 1)
                + jitter;
            const float side = wall % 2 == 0 ? -1.0F : 1.0F;
            wkc::core::Aabb local{};
            if (xWall) {
                const float nearWall = side < 0.0F
                    ? -halfX : halfX - depth;
                local = {{nearWall, 0.0F, along - width * 0.5F},
                    {nearWall + depth, span.y, along + width * 0.5F}};
            } else {
                const float nearWall = side < 0.0F
                    ? -halfZ : halfZ - depth;
                local = {{along - width * 0.5F, 0.0F, nearWall},
                    {along + width * 0.5F, span.y, nearWall + depth}};
            }
            const wkc::core::Aabb bounds = toWorldBounds(local, frame);
            const bool overlapsArchitecture = std::any_of(authored.begin(),
                authored.end(), [&](const RoomArchitecturePart& part) {
                    return overlapsOpen(bounds, part.bounds);
                });
            if (!room.worldBounds.contains(bounds.minimum)
                || !room.worldBounds.contains(bounds.maximum)
                || overlapsArchitecture
                || !clearOfZones(bounds, room.enemyZones, 0.35F, floor)
                || !clearOfZones(bounds, room.propZones, 0.15F, floor)
                || !clearOfZones(bounds, room.lootZones, 0.10F, floor)
                || !clearOfConnectors(bounds, room.connectors)) {
                continue;
            }
            columns.push_back({bounds,
                laboratoryColumn ? wkc::core::Vec3{0.24F, 0.38F, 0.48F}
                    : wallColumnTint(room.theme), true});
            columns.back().structuralColumn = true;
            if (!laboratoryColumn) continue;

            // The reference's navy stripe is a full-height wall pier. Its
            // collar, edge rails and status light are separate 3D pieces,
            // while the shaft alone provides the collision volume.
            const auto detail = [&](const wkc::core::Vec3 center,
                const wkc::core::Vec3 size, const wkc::core::Vec3 tint,
                const bool indicator = false) {
                RoomArchitecturePart piece{
                    toWorldBounds({center - size * 0.5F,
                        center + size * 0.5F}, frame), tint, false};
                if (indicator) {
                    piece.decoration = RoomDecorationPart::LaboratoryColumnIndicator;
                }
                columns.push_back(piece);
            };
            const float face = xWall
                ? (side < 0.0F ? local.maximum.x : local.minimum.x)
                : (side < 0.0F ? local.maximum.z : local.minimum.z);
            const float inward = side < 0.0F ? 1.0F : -1.0F;
            const float front = face + inward * 0.018F;
            const auto frontCenter = [&](const float y, const float offset) {
                return xWall
                    ? wkc::core::Vec3{front, y, along + offset}
                    : wkc::core::Vec3{along + offset, y, front};
            };
            const auto frontSize = [&](const float transverseWidth,
                const float verticalHeight, const float faceDepth) {
                return xWall
                    ? wkc::core::Vec3{faceDepth, verticalHeight, transverseWidth}
                    : wkc::core::Vec3{transverseWidth, verticalHeight, faceDepth};
            };
            const wkc::core::Vec3 darkBlue{0.10F, 0.21F, 0.29F};
            const wkc::core::Vec3 paleEdge{0.42F, 0.58F, 0.65F};
            for (const float y : {0.14F, span.y - 0.14F}) {
                detail(frontCenter(y, 0.0F),
                    frontSize(width + 0.06F, 0.28F, 0.045F), darkBlue);
            }
            for (const float offset : {-width * 0.5F + 0.045F,
                    width * 0.5F - 0.045F}) {
                detail(frontCenter(span.y * 0.5F, offset),
                    frontSize(0.035F, span.y - 0.56F, 0.045F), paleEdge);
            }
            detail(frontCenter(1.63F, 0.0F),
                frontSize(0.18F, 0.34F, 0.050F),
                {0.045F, 0.085F, 0.105F});
            for (const float y : {1.57F, 1.68F}) {
                detail(frontCenter(y, 0.0F) + (xWall
                        ? wkc::core::Vec3{inward * 0.028F, 0.0F, 0.0F}
                        : wkc::core::Vec3{0.0F, 0.0F, inward * 0.028F}),
                    frontSize(0.055F, 0.024F, 0.012F),
                    {0.17F, 0.90F, 0.49F}, true);
            }
        }
    }
    return columns;
}

std::vector<RoomArchitecturePart> makeRoomArchitecture(const RoomInstance& room)
{
    if (room.moduleId == industrialCorridorDestinationPreviewModuleId) {
        return {};
    }
    if (room.role == RoomRole::Boss
        || room.moduleId == bossRoomPreviewModuleId) {
        if (!room.worldBounds.isValid()) return {};
        const LocalFrame frame{room.transform.position, room.rotation};
        const float height = room.worldBounds.maximum.y - room.worldBounds.minimum.y;
        std::vector<RoomArchitecturePart> parts;
        parts.reserve(8U);
        for (const float z : {-8.0F, 8.0F}) {
            const auto columnPart = [&](const wkc::core::Aabb bounds,
                                        const wkc::core::Vec3 tint,
                                        const bool structural) {
                addPart(parts, frame, bounds, tint, true);
                parts.back().structuralColumn = structural;
            };
            columnPart({{6.2F, 0.0F, z - 0.8F},
                           {7.8F, height - 0.1F, z + 0.8F}},
                combatSteel, true);
            columnPart({{6.0F, 0.0F, z - 1.0F},
                           {8.0F, 0.30F, z + 1.0F}},
                darkSteel, true);
            columnPart({{6.0F, height - 0.30F, z - 1.0F},
                           {8.0F, height, z + 1.0F}},
                darkSteel, true);
            columnPart({{6.12F, 1.45F, z - 0.88F},
                           {7.88F, 1.67F, z + 0.88F}},
                combatAccent, false);
        }
        return parts;
    }
    if (room.moduleId == combatRoomPreviewModuleId
        || room.moduleId == combatGalleryArenaModuleId) {
        return room.worldBounds.isValid()
            ? makeCombatPreviewArchitecture(room)
            : std::vector<RoomArchitecturePart>{};
    }
    if (room.moduleId == cafeteriaPreviewModuleId
        || room.moduleId == cafeteriaLongPreviewModuleId
        || room.moduleId == cafeteriaLongModuleId) {
        return room.worldBounds.isValid()
            ? makeCafeteriaPreviewArchitecture(room)
            : std::vector<RoomArchitecturePart>{};
    }
    if (room.moduleId == laboratoryPreviewModuleId
        || room.moduleId == laboratoryHallModuleId) {
        return room.worldBounds.isValid()
            ? makeLaboratoryPreviewArchitecture(room)
            : std::vector<RoomArchitecturePart>{};
    }
    if (room.moduleId == technicalPreviewModuleId) {
        return room.worldBounds.isValid()
            ? makeTechnicalPreviewArchitecture(room)
            : std::vector<RoomArchitecturePart>{};
    }
    if (room.moduleId == technicalLPreviewModuleId
        || room.moduleId == maintenanceLModuleId) {
        auto parts = makeTechnicalPreviewArchitecture(room);
        // Keep existing furniture only where its entire footprint fits the
        // new L, including the recessed corner and its inner walls.
        std::erase_if(parts, [&room](const RoomArchitecturePart& part) {
            const float cx = (part.bounds.minimum.x + part.bounds.maximum.x) * 0.5F;
            const float cz = (part.bounds.minimum.z + part.bounds.maximum.z) * 0.5F;
            for (const float x : {part.bounds.minimum.x, part.bounds.maximum.x}) {
                for (const float z : {part.bounds.minimum.z, part.bounds.maximum.z}) {
                    const wkc::core::Vec3 sample{
                        x * 0.98F + cx * 0.02F, 0.0F,
                        z * 0.98F + cz * 0.02F};
                    if (!footprintContainsDisk(room.walkableOutline, sample)) {
                        return true;
                    }
                }
            }
            return false;
        });
        return parts;
    }
    if (room.moduleId == securityPreviewModuleId) {
        return room.worldBounds.isValid()
            ? makeSecurityPreviewArchitecture(room)
            : std::vector<RoomArchitecturePart>{};
    }
    if (room.moduleId == securityTPreviewModuleId
        || room.moduleId == securityTModuleId) {
        return room.worldBounds.isValid()
            ? makeSecurityTPreviewArchitecture(room)
            : std::vector<RoomArchitecturePart>{};
    }
    if (room.moduleId == centralHubPreviewModuleId
        || room.moduleId == centralHubModuleId) {
        return room.worldBounds.isValid()
            ? makeCentralHubPreviewArchitecture(room)
            : std::vector<RoomArchitecturePart>{};
    }
    if (room.theme != RoomTheme::Storage || !room.worldBounds.isValid()) {
        return {};
    }
    const wkc::core::Vec3 span = room.worldBounds.maximum
        - room.worldBounds.minimum;
    const bool quarterTurn = room.rotation == wkc::core::QuarterTurn::Degrees90
        || room.rotation == wkc::core::QuarterTurn::Degrees270;
    const float halfX = (quarterTurn ? span.z : span.x) * 0.5F;
    const float halfZ = (quarterTurn ? span.x : span.z) * 0.5F;
    const float height = span.y;
    if (halfX < 2.8F || halfZ < 3.2F || height < 3.3F) {
        return {};
    }
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    std::vector<RoomArchitecturePart> parts;
    parts.reserve(256U);

    // Vertical supports and high crossbeams make the larger warehouse rooms
    // feel taller. The beams remain above head clearance but still block shots.
    for (const float side : {-1.0F, 1.0F}) {
        for (const float end : {-1.0F, 1.0F}) {
            const float centerX = side * (halfX - 0.42F);
            const float centerZ = end * (halfZ - 1.00F);
            addPart(parts, frame,
                {{centerX - 0.18F, 0.0F, centerZ - 0.18F},
                    {centerX + 0.18F, height - 0.15F,
                        centerZ + 0.18F}},
                darkSteel, true);
        }
        const float beamZ = side * (halfZ - 1.00F);
        addPart(parts, frame,
            {{-halfX + 0.18F, height - 0.40F, beamZ - 0.12F},
                {halfX - 0.18F, height - 0.15F, beamZ + 0.12F}},
            darkSteel, true);
    }

    const float rackStart = 1.55F;
    const float rackEnd = halfZ - 0.85F;
    const float rackSpan = rackEnd - rackStart;
    const int bayCount = std::clamp(
        static_cast<int>(std::floor(rackSpan / 2.35F)), 1, 3);
    const float bayStep = rackSpan / static_cast<float>(bayCount);
    for (int index = 0; index < bayCount; ++index) {
        const float positiveStart = rackStart
            + static_cast<float>(index) * bayStep + 0.08F;
        const float positiveEnd = rackStart
            + static_cast<float>(index + 1) * bayStep - 0.08F;
        for (const float side : {-1.0F, 1.0F}) {
            addRackBay(parts, frame, halfX, height,
                positiveStart, positiveEnd, side);
            addRackBay(parts, frame, halfX, height,
                -positiveEnd, -positiveStart, side);
        }
    }

    // The hall has two free-standing racks, staggered along the long axis.
    // Their X footprint is [3.00, 4.00] on either side; the wall racks begin
    // at 8.07 in the campaign hall, leaving a 4.07 m side aisle.
    if (room.sizeClass == RoomSizeClass::Large
        && halfX >= 8.8F && halfZ >= 9.8F && height >= 5.8F) {
        addIslandBay(parts, frame, -3.50F, 5.55F, height);
        addIslandBay(parts, frame, 3.50F, -5.55F, height);
    }
    if ((room.moduleId == 204U || room.moduleId == 0U)
        && halfX >= 8.8F && halfZ >= 9.8F && height >= 5.8F) {
        // A central overhead run stays between the storage light rows.
        addPart(parts, frame,
            {{-0.36F, height - 0.82F, -halfZ + 0.55F},
                {0.36F, height - 0.42F, halfZ - 0.55F}},
            darkSteel, true);
        parts.back().decoration = RoomDecorationPart::CeilingDuct;
    }
    return parts;
}

bool validateRoomArchitecture(
    const RoomInstance& room,
    const std::span<const RoomArchitecturePart> parts) noexcept
{
    if (room.moduleId == industrialCorridorDestinationPreviewModuleId) {
        return parts.empty();
    }
    if (room.theme != RoomTheme::Storage
        && room.role != RoomRole::Boss
        && room.moduleId != bossRoomPreviewModuleId
        && room.moduleId != combatRoomPreviewModuleId
        && room.moduleId != combatGalleryArenaModuleId
        && room.moduleId != cafeteriaPreviewModuleId
        && room.moduleId != cafeteriaLongPreviewModuleId
        && room.moduleId != cafeteriaLongModuleId
        && room.moduleId != laboratoryPreviewModuleId
        && room.moduleId != laboratoryHallModuleId
        && room.moduleId != technicalPreviewModuleId
        && room.moduleId != technicalLPreviewModuleId
        && room.moduleId != maintenanceLModuleId
        && room.moduleId != securityPreviewModuleId
        && room.moduleId != securityTPreviewModuleId
        && room.moduleId != securityTModuleId
        && room.moduleId != centralHubPreviewModuleId
        && room.moduleId != centralHubModuleId) {
        return parts.empty();
    }
    if (!room.worldBounds.isValid() || parts.empty()) {
        return false;
    }
    const float floor = room.worldBounds.minimum.y;
    for (const RoomArchitecturePart& part : parts) {
        if (!part.solid || !part.bounds.isValid()
            || !room.worldBounds.contains(part.bounds.minimum)
            || !room.worldBounds.contains(part.bounds.maximum)
            || !clearOfZones(part.bounds, room.enemyZones, 0.35F, floor)
            || !clearOfZones(part.bounds, room.propZones, 0.15F, floor)
            || !clearOfZones(part.bounds, room.lootZones, 0.10F, floor)) {
            return false;
        }
        for (const ConnectorInstance& connector : room.connectors) {
            const auto& p = connector.worldPosition;
            const auto& n = connector.worldNormal;
            const float halfWidth = connector.width * 0.5F + 0.30F;
            const float depth = 0.95F;
            const wkc::core::Aabb opening = std::abs(n.x) > 0.5F
                ? wkc::core::Aabb{
                    {p.x - (n.x > 0.0F ? depth : 0.0F), p.y,
                        p.z - halfWidth},
                    {p.x + (n.x < 0.0F ? depth : 0.0F),
                        p.y + connector.height, p.z + halfWidth}}
                : wkc::core::Aabb{
                    {p.x - halfWidth, p.y,
                        p.z - (n.z > 0.0F ? depth : 0.0F)},
                    {p.x + halfWidth, p.y + connector.height,
                        p.z + (n.z < 0.0F ? depth : 0.0F)}};
            if (overlapsOpen(part.bounds, opening)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace gdp::world
