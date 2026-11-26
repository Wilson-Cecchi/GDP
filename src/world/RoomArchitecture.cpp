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
        const int bayVariant = (static_cast<int>(std::abs(zMinimum) * 10.0F)
            + (side > 0.0F ? 1 : 0) + static_cast<int>(index)) % 3;
        const float cargoLength = std::min(
            0.62F + 0.10F * static_cast<float>(bayVariant), bayLength * 0.40F);
        const float centerZ = zMinimum + bayLength
            * (0.32F + 0.17F * static_cast<float>(bayVariant));
        addPart(parts, frame,
            {{xMinimum + 0.09F, shelfTop, centerZ - cargoLength * 0.5F},
                {xMaximum - 0.09F, shelfTop + 0.44F,
                    centerZ + cargoLength * 0.5F}},
            bayVariant == 1 ? charcoalCargo : oliveCargo, true, true);
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
            + (index % 2U == 0U ? -0.31F : 0.31F);
        const float cargoHalfLength = index == 1U ? 0.27F : 0.36F;
        addPart(parts, frame,
            {{xMinimum + 0.10F, shelfTop, cargoCenterZ - cargoHalfLength},
                {xMaximum - 0.10F, shelfTop + 0.44F,
                    cargoCenterZ + cargoHalfLength}},
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
        return {0.29F, 0.28F, 0.26F};
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
    parts.reserve(560U);
    const auto addCombat = [&](const wkc::core::Aabb bounds,
                               const wkc::core::Vec3 tint,
                               const CombatArchitecturePart kind) {
        addPart(parts, frame, bounds, tint, true);
        parts.back().combatPart = kind;
    };

    // Each cover has a dark structural foot and end cheeks around a broad
    // olive steel panel. The six separated footprints leave the centre lane
    // and flank lanes open, matching the rhythm of the concept board.
    const auto addParapet = [&](const float cx, const float cz,
                                const float length, const float height,
                                const bool alongX) {
        const float hx = alongX ? length * 0.5F : 0.33F;
        const float hz = alongX ? 0.33F : length * 0.5F;
        const float coreInsetX = alongX ? 0.17F : 0.065F;
        const float coreInsetZ = alongX ? 0.065F : 0.17F;
        addCombat({{cx - hx + coreInsetX, 0.12F,
                       cz - hz + coreInsetZ},
                      {cx + hx - coreInsetX, height - 0.08F,
                          cz + hz - coreInsetZ}},
            combatCover, CombatArchitecturePart::Cover);
        addCombat({{cx - hx, 0.0F, cz - hz},
                      {cx + hx, 0.16F, cz + hz}},
            darkSteel, CombatArchitecturePart::CoverFrame);
        addCombat({{cx - hx, height - 0.12F, cz - hz},
                      {cx + hx, height, cz + hz}},
            darkSteel, CombatArchitecturePart::CoverFrame);
        for (const float end : {-1.0F, 1.0F}) {
            if (alongX) {
                const float x = cx + end * (hx - 0.11F);
                addCombat({{x - 0.11F, 0.0F, cz - hz},
                              {x + 0.11F, height, cz + hz}},
                    darkSteel, CombatArchitecturePart::CoverFrame);
            } else {
                const float z = cz + end * (hz - 0.11F);
                addCombat({{cx - hx, 0.0F, z - 0.11F},
                              {cx + hx, height, z + 0.11F}},
                    darkSteel, CombatArchitecturePart::CoverFrame);
            }
        }
        // Thin real faces sit proud of the body without extending its overall
        // collider footprint. The renderer can give these a brushed finish.
        if (alongX) {
            for (const float face : {-1.0F, 1.0F}) {
                const float z = cz + face * (hz - 0.050F);
                addCombat({{cx - hx + 0.25F, 0.28F, z - 0.015F},
                              {cx + hx - 0.25F, height - 0.22F, z + 0.015F}},
                    combatCover, CombatArchitecturePart::CoverFace);
            }
        } else {
            for (const float face : {-1.0F, 1.0F}) {
                const float x = cx + face * (hx - 0.050F);
                addCombat({{x - 0.015F, 0.28F, cz - hz + 0.25F},
                              {x + 0.015F, height - 0.22F, cz + hz - 0.25F}},
                    combatCover, CombatArchitecturePart::CoverFace);
            }
        }
        // Scale the cover's construction to its own dimensions: recessed
        // metal faces, two reinforcement ribs, and small raised fasteners.
        // Everything remains inside the original cap/cheek footprint.
        for (const float face : {-1.0F, 1.0F}) {
            for (const float offset : {-0.29F, 0.29F}) {
                if (alongX) {
                    const float x = cx + offset;
                    const float z = cz + face * (hz - 0.033F);
                    addCombat({{x - 0.025F, 0.21F, z - 0.025F},
                                  {x + 0.025F, height - 0.14F, z + 0.025F}},
                        darkSteel, CombatArchitecturePart::CoverRib);
                } else {
                    const float x = cx + face * (hx - 0.033F);
                    const float z = cz + offset;
                    addCombat({{x - 0.025F, 0.21F, z - 0.025F},
                                  {x + 0.025F, height - 0.14F, z + 0.025F}},
                        darkSteel, CombatArchitecturePart::CoverRib);
                }
            }
            for (const float along : {-0.42F, 0.42F}) {
                for (const float y : {0.25F, height - 0.18F}) {
                    const float x = alongX ? cx + along
                        : cx + face * (hx - 0.038F);
                    const float z = alongX ? cz + face * (hz - 0.038F)
                        : cz + along;
                    addCombat({{x - 0.028F, y - 0.028F, z - 0.028F},
                                  {x + 0.028F, y + 0.028F, z + 0.028F}},
                        combatAccent, CombatArchitecturePart::CoverBolt);
                }
            }
        }
    };
    for (const float side : {-1.0F, 1.0F}) {
        addParapet(side * 3.2F, side > 0.0F ? -3.6F : 3.6F,
            2.15F, 1.25F, true);
        addParapet(side * 6.9F, 0.0F, 2.35F, 1.65F, false);
        addParapet(side * 5.3F, side > 0.0F ? 5.8F : -5.8F,
            2.25F, 1.12F, true);

        // A raised catwalk grows out of the side wall. Its dark fascia and
        // repeated cantilever supports create the long silhouette in the
        // reference while retaining the existing walkable stair connection.
        const float galleryMinX = side > 0.0F ? 8.5F : -9.95F;
        const float galleryMaxX = side > 0.0F ? 9.95F : -8.5F;
        addCombat({{galleryMinX, 3.15F, -6.4F},
                      {galleryMaxX, 3.32F, 3.4F}},
            combatSteel, CombatArchitecturePart::GalleryDeck);
        addCombat({{side > 0.0F ? 8.5F : -8.68F, 2.94F, -6.4F},
                      {side > 0.0F ? 8.68F : -8.5F, 3.15F, 3.4F}},
            darkSteel, CombatArchitecturePart::GallerySupport);
        for (const float z : {-5.7F, -2.7F, 0.3F, 3.2F}) {
            addCombat({{galleryMinX, 2.76F, z - 0.13F},
                          {galleryMaxX, 3.15F, z + 0.13F}},
                darkSteel, CombatArchitecturePart::GallerySupport);
            addCombat({{side > 0.0F ? 9.58F : -9.95F, 2.32F, z - 0.12F},
                          {side > 0.0F ? 9.95F : -9.58F, 2.76F, z + 0.12F}},
                darkSteel, CombatArchitecturePart::GallerySupport);
            // A stepped steel gusset reads as a braced cantilever below the
            // catwalk. It has a wall plate, shoulder, lip and visible bolts.
            for (int step = 0; step < 3; ++step) {
                const float reach = 0.37F + static_cast<float>(step) * 0.33F;
                const float top = 2.78F + static_cast<float>(step) * 0.12F;
                const float wallX = side > 0.0F ? 9.95F : -9.95F;
                addCombat({{side > 0.0F ? wallX - reach : wallX,
                               top - 0.11F, z - 0.095F},
                              {side > 0.0F ? wallX : wallX + reach,
                                  top, z + 0.095F}},
                    combatSteel, CombatArchitecturePart::GalleryBracket);
            }
            const float anchorX = side * 9.88F;
            addCombat({{anchorX - 0.065F, 2.43F, z - 0.20F},
                          {anchorX + 0.065F, 3.05F, z + 0.20F}},
                darkSteel, CombatArchitecturePart::GalleryBracket);
            for (const float y : {2.53F, 2.94F}) {
                addCombat({{side > 0.0F ? 9.75F : -9.95F,
                               y - 0.025F, z - 0.025F},
                              {side > 0.0F ? 9.95F : -9.75F,
                                  y + 0.025F, z + 0.025F}},
                    combatAccent, CombatArchitecturePart::GalleryFastener);
            }
        }
        // Three floor-to-gallery pillars carry the open side of each deck.
        // The forward pillar sits beyond the side-door approach and before
        // the stair landing, so the ground-level doorway stays walkable.
        const float pillarX = side * 8.76F;
        for (const float z : {-5.7F, -2.7F, 2.0F}) {
            addCombat({{pillarX - 0.32F, 0.0F, z - 0.34F},
                          {pillarX + 0.32F, 0.16F, z + 0.34F}},
                darkSteel, CombatArchitecturePart::GalleryPillarTrim);
            addCombat({{pillarX - 0.22F, 0.16F, z - 0.23F},
                          {pillarX + 0.22F, 2.78F, z + 0.23F}},
                darkSteel, CombatArchitecturePart::GalleryPillar);
            addCombat({{pillarX - 0.35F, 2.78F, z - 0.34F},
                          {pillarX + 0.35F, 3.15F, z + 0.34F}},
                combatSteel, CombatArchitecturePart::GalleryPillarTrim);
            // An exposed stiffener ties the pillar face to the gallery edge.
            const float innerX = pillarX - side * 0.225F;
            addCombat({{innerX - 0.025F, 0.42F, z - 0.13F},
                          {innerX + 0.025F, 2.66F, z + 0.13F}},
                combatSteel, CombatArchitecturePart::GalleryPillarTrim);
        }
        const float railX = side * 8.53F;
        for (const float z : {-6.2F, -3.9F, -1.6F, 0.7F, 3.2F}) {
            addCombat({{railX - 0.055F, 3.32F, z - 0.055F},
                          {railX + 0.055F, 4.18F, z + 0.055F}},
                combatAccent, CombatArchitecturePart::GalleryRail);
        }
        for (const float y : {3.70F, 4.08F}) {
            addCombat({{railX - 0.044F, y, -6.2F},
                          {railX + 0.044F, y + 0.088F, 3.2F}},
                combatAccent, CombatArchitecturePart::GalleryRail);
        }

        // Open, wall-mounted treads remain the movement collider. The handrail
        // climbs in one straight line, with short collision envelopes around
        // its actual sloped segments instead of a room-long invisible AABB.
        constexpr float treadRise = 3.32F / 14.0F;
        constexpr float railRisePerMetre = 3.30F / 5.80F;
        const auto addSlopedRail = [&](const float startZ, const float endZ) {
            const float startY = 0.80F + (9.0F - startZ) * railRisePerMetre;
            const float endY = 0.80F + (9.0F - endZ) * railRisePerMetre;
            addCombat({{railX - 0.048F, startY - 0.048F, endZ - 0.048F},
                          {railX + 0.048F, endY + 0.048F, startZ + 0.048F}},
                combatAccent, CombatArchitecturePart::StairRailSlope);
            parts.back().railStart = frame.origin
                + wkc::core::rotateQuarterTurns(
                    {railX, startY, startZ}, frame.rotation);
            parts.back().railEnd = frame.origin
                + wkc::core::rotateQuarterTurns(
                    {railX, endY, endZ}, frame.rotation);
        };
        for (int tread = 0; tread < 14; ++tread) {
            const float nearZ = 9.0F - static_cast<float>(tread) * 0.4F;
            const float top = (static_cast<float>(tread) + 1.0F) * treadRise;
            addCombat({{galleryMinX, top - 0.10F, nearZ - 0.4F},
                          {galleryMaxX, top, nearZ}},
                combatSteel, CombatArchitecturePart::StairTread);
            addCombat({{side > 0.0F ? 9.20F : -9.95F, top - 0.19F,
                           nearZ - 0.25F},
                          {side > 0.0F ? 9.95F : -9.20F, top - 0.10F,
                              nearZ - 0.13F}},
                darkSteel, CombatArchitecturePart::GallerySupport);
            if (tread % 2 == 0) {
                const float postZ = nearZ - 0.25F;
                const float railY = 0.80F
                    + (9.0F - postZ) * railRisePerMetre;
                addCombat({{railX - 0.055F, top, nearZ - 0.30F},
                              {railX + 0.055F, railY + 0.04F,
                                  nearZ - 0.20F}},
                    combatAccent, CombatArchitecturePart::StairRail);
            }
            addSlopedRail(nearZ, nearZ - 0.4F);
        }
        // The last 0.2 m meets the gallery's upper rail at y=4.10 m.
        addSlopedRail(3.4F, 3.2F);

        // Side-wall columns are visibly larger than the former thin strips.
        // One is behind the gallery, the other beyond the stair foot.
        for (const float z : {-7.8F, 10.2F}) {
            const float x = side * 9.48F;
            addCombat({{x - 0.43F, 0.0F, z - 0.49F},
                          {x + 0.43F, 0.28F, z + 0.49F}},
                darkSteel, CombatArchitecturePart::ColumnTrim);
            addCombat({{x - 0.35F, 0.28F, z - 0.41F},
                          {x + 0.35F, 5.70F, z + 0.41F}},
                darkSteel, CombatArchitecturePart::ColumnShaft);
            addCombat({{x - 0.43F, 5.70F, z - 0.49F},
                          {x + 0.43F, 5.98F, z + 0.49F}},
                darkSteel, CombatArchitecturePart::ColumnTrim);
            const float insetX = x - side * 0.353F;
            for (const float y : {0.65F, 3.45F}) {
                addCombat({{insetX - 0.020F, y, z - 0.13F},
                              {insetX + 0.020F, y + 1.70F, z + 0.13F}},
                    combatSteel, CombatArchitecturePart::ColumnInset);
            }
            for (const float y : {1.22F, 3.02F, 4.82F}) {
                addCombat({{x - 0.39F, y, z - 0.44F},
                              {x + 0.39F, y + 0.095F, z + 0.44F}},
                    combatSteel, CombatArchitecturePart::ColumnBrace);
            }
        }
    }
    // End-wall columns carry the transverse ceiling spans. Door apertures
    // remain at the centre of each wall, away from these four solid feet.
    for (const float x : {-6.7F, 6.7F}) {
        for (const float z : {-10.48F, 10.48F}) {
            addCombat({{x - 0.43F, 0.0F, z - 0.43F},
                          {x + 0.43F, 0.28F, z + 0.43F}},
                darkSteel, CombatArchitecturePart::ColumnTrim);
            addCombat({{x - 0.35F, 0.28F, z - 0.35F},
                          {x + 0.35F, 5.70F, z + 0.35F}},
                darkSteel, CombatArchitecturePart::ColumnShaft);
            addCombat({{x - 0.43F, 5.70F, z - 0.43F},
                          {x + 0.43F, 5.98F, z + 0.43F}},
                darkSteel, CombatArchitecturePart::ColumnTrim);
            const float insetZ = z - std::copysign(0.353F, z);
            for (const float y : {0.65F, 3.45F}) {
                addCombat({{x - 0.13F, y, insetZ - 0.020F},
                              {x + 0.13F, y + 1.70F,
                                  insetZ + 0.020F}},
                    combatSteel, CombatArchitecturePart::ColumnInset);
            }
            for (const float y : {1.22F, 3.02F, 4.82F}) {
                addCombat({{x - 0.39F, y, z - 0.39F},
                              {x + 0.39F, y + 0.095F, z + 0.39F}},
                    combatSteel, CombatArchitecturePart::ColumnBrace);
            }
        }
    }

    // Open ceiling grid: real beams, inset panels and slatted grilles. A
    // shallow panel band at the edges never covers the central light rows.
    // Light anchors fall near (0,0) and (+/-4.4,+/-4.4); keep the beam
    // centre lines between those anchors instead of crossing the fixtures.
    for (const float z : {-8.0F, -2.3F, 2.3F, 8.0F}) {
        addCombat({{-9.35F, 5.54F, z - 0.16F},
                      {9.35F, 5.80F, z + 0.16F}},
            darkSteel, CombatArchitecturePart::CeilingBeam);
    }
    for (const float x : {-7.2F, -2.3F, 2.3F, 7.2F}) {
        addCombat({{x - 0.16F, 5.80F, -10.48F},
                      {x + 0.16F, 5.96F, 10.48F}},
            darkSteel, CombatArchitecturePart::CeilingBeam);
    }
    // Perimeter girders physically meet the column capitals and close the
    // visual line between wall columns and the suspended ceiling grid.
    for (const float x : {-9.48F, 9.48F}) {
        addCombat({{x - 0.18F, 5.72F, -8.12F},
                      {x + 0.18F, 5.92F, 10.48F}},
            darkSteel, CombatArchitecturePart::CeilingBrace);
    }
    for (const float z : {-10.48F, 10.48F}) {
        addCombat({{-7.08F, 5.72F, z - 0.18F},
                      {7.08F, 5.92F, z + 0.18F}},
            darkSteel, CombatArchitecturePart::CeilingBrace);
    }
    for (const float x : {-7.95F, 7.95F}) {
        for (const float z : {-4.0F, 4.0F}) {
            addCombat({{x - 0.48F, 5.87F, z - 1.3F},
                          {x + 0.48F, 5.96F, z + 1.3F}},
                combatSteel, CombatArchitecturePart::CeilingPanel);
        }
    }
    // Four grilles use actual individual bars, so bullets never collide with
    // an invisible filled rectangle across their open cells.
    for (const float x : {-2.0F, 2.0F}) {
        for (const float z : {-6.6F, 6.6F}) {
            for (int bar = 0; bar < 5; ++bar) {
                const float barZ = z - 0.80F + static_cast<float>(bar) * 0.40F;
                addCombat({{x - 0.65F, 5.82F, barZ - 0.045F},
                              {x + 0.65F, 5.92F, barZ + 0.045F}},
                    darkSteel, CombatArchitecturePart::CeilingGrille);
            }
        }
    }
    for (const float x : {-5.9F, 5.9F}) {
        addPart(parts, frame,
            {{x - 0.32F, 5.12F, -8.9F},
                {x + 0.32F, 5.48F, 8.9F}},
            combatSteel, true);
        parts.back().decoration = RoomDecorationPart::CeilingDuct;
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
                cafeteriaDark, true, true);
            parts.back().decoration = RoomDecorationPart::BroadleafPlanter;
        }
    }
    return parts;
}

std::vector<RoomArchitecturePart> makeCafeteriaHallArchitecture(
    const RoomInstance& room)
{
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    std::vector<RoomArchitecturePart> parts;
    parts.reserve(96U);

    // The campaign hall reserves its centre for enemies and the right half
    // for procedural props. Two dining groups fit beyond the enemy zone,
    // while still leaving both end-door approaches open.
    for (const float z : {-6.0F, 6.0F}) {
        addDiningGroup(parts, frame, -0.5F, z);
    }
    // The service counter is wall attached, on the side without a prop zone.
    // Its positive-Z placement also keeps the west door and south loot clear.
    addPart(parts, frame,
        {{-8.85F, 0.0F, 2.1F}, {-7.45F, 1.05F, 6.4F}},
        cafeteriaDark, true, false, false, StorageRackPart::None,
        CafeteriaFurniturePart::Cabinet);
    addPart(parts, frame,
        {{-8.85F, 1.05F, 2.1F}, {-7.32F, 1.19F, 6.4F}},
        cafeteriaSteel, true, false, false, StorageRackPart::None,
        CafeteriaFurniturePart::Steel);
    addPart(parts, frame,
        {{-7.43F, 0.12F, 2.3F}, {-7.38F, 0.22F, 6.2F}},
        cafeteriaSeat, true, false, false, StorageRackPart::None,
        CafeteriaFurniturePart::Seat);
    for (const float z : {2.8F, 4.1F, 5.4F}) {
        addPart(parts, frame,
            {{-7.43F, 0.30F, z}, {-7.38F, 0.87F, z + 0.86F}},
            cafeteriaDark, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Support);
        addPart(parts, frame,
            {{-8.48F, 1.19F, z + 0.10F}, {-7.91F, 1.25F, z + 0.55F}},
            cafeteriaSteel, true, false, false, StorageRackPart::None,
            CafeteriaFurniturePart::Steel);
    }
    // The loose prop pocket occupies the right half of this hall, so the
    // corner plants sit beyond it and away from the four door approaches.
    constexpr float cornerX = 8.18F;
    for (const float z : {-7.78F, 7.78F}) {
        addPart(parts, frame,
            {{cornerX, 0.0F, z - 0.32F},
                {cornerX + 0.64F, 0.64F, z + 0.32F}},
            cafeteriaDark, true, true);
        parts.back().decoration = RoomDecorationPart::BroadleafPlanter;
    }
    return parts;
}

std::vector<RoomArchitecturePart> makeCompactThemeArchitecture(
    const RoomInstance& room)
{
    const wkc::core::Vec3 span = room.worldBounds.maximum
        - room.worldBounds.minimum;
    const bool quarterTurn = room.rotation == wkc::core::QuarterTurn::Degrees90
        || room.rotation == wkc::core::QuarterTurn::Degrees270;
    const float halfX = (quarterTurn ? span.z : span.x) * 0.5F;
    const float halfZ = (quarterTurn ? span.x : span.z) * 0.5F;
    const LocalFrame frame{
        {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
            room.worldBounds.minimum.y,
            (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
        room.rotation,
    };
    std::vector<RoomArchitecturePart> parts;
    parts.reserve(64U);

    // This shallow workstation occupies the positive-Z segment of the left
    // wall, beyond the central enemy zone. It misses the west connector at
    // Z=0, the south-west loot zone and the right-hand procedural prop zone.
    const float wallX = -halfX;
    const float frontX = wallX + 0.68F;
    const float backZ = std::min(halfZ - 0.72F, 3.45F);
    const float frontZ = 2.18F;
    if (backZ - frontZ < 0.65F || halfX < 3.2F) return {};
    const auto addFurniture = [&](const wkc::core::Aabb bounds,
                                  const wkc::core::Vec3 tint,
                                  const int kind) {
        addPart(parts, frame, bounds, tint, true);
        if (room.theme == RoomTheme::Cafeteria) {
            parts.back().cafeteriaFurniturePart
                = kind == 0 ? CafeteriaFurniturePart::Cabinet
                : kind == 1 ? CafeteriaFurniturePart::Steel
                            : CafeteriaFurniturePart::Support;
        } else if (room.theme == RoomTheme::Laboratory) {
            parts.back().laboratoryFurniturePart
                = kind == 0 ? LaboratoryFurniturePart::BenchBase
                : kind == 1 ? LaboratoryFurniturePart::BenchTop
                            : LaboratoryFurniturePart::Instrument;
        } else if (room.theme == RoomTheme::Security) {
            // Kind 3 is compact trim; it must not inherit the full console
            // or monitor geometry added by the renderer.
            parts.back().securityFurniturePart
                = kind == 0 ? SecurityFurniturePart::ConsoleBase
                : kind == 1 ? SecurityFurniturePart::ConsoleTop
                : kind == 2 ? SecurityFurniturePart::Monitor
                            : SecurityFurniturePart::ConsoleTrim;
            if (room.moduleId == 601U && kind == 2) {
                // The thin status strip belongs to the monitor body and
                // disappears with it, but is not a separate dynamic prop.
                parts.back().breakable = true;
            }
        } else if (room.theme == RoomTheme::Maintenance) {
            parts.back().technicalFurniturePart
                = kind == 0 ? TechnicalFurniturePart::BenchBase
                : kind == 1 ? TechnicalFurniturePart::BenchTop
                            : TechnicalFurniturePart::Instrument;
        }
    };
    wkc::core::Vec3 body = cafeteriaDark;
    wkc::core::Vec3 top = cafeteriaSteel;
    wkc::core::Vec3 equipment = combatAccent;
    switch (room.theme) {
    case RoomTheme::Laboratory:
        body = laboratoryCabinet;
        top = laboratorySteel;
        equipment = laboratoryAccent;
        break;
    case RoomTheme::Security:
        body = darkSteel;
        top = combatSteel;
        equipment = laboratoryAccent;
        break;
    case RoomTheme::Maintenance:
        body = technicalCabinet;
        top = technicalSteel;
        equipment = technicalPipe;
        break;
    case RoomTheme::Office:
        body = {0.24F, 0.27F, 0.29F};
        top = {0.46F, 0.49F, 0.48F};
        equipment = {0.27F, 0.59F, 0.66F};
        break;
    case RoomTheme::Cafeteria:
        equipment = cafeteriaSeat;
        break;
    default:
        return {};
    }
    addFurniture({{wallX, 0.0F, frontZ},
                     {frontX, 0.86F, backZ}}, body, 0);
    addFurniture({{wallX, 0.86F, frontZ},
                     {frontX + 0.07F, 0.98F, backZ}}, top, 1);
    // A raised instrument or display makes the station legible by theme.
    // The small solid is supported by the top, with no floating collider.
    addFurniture({{wallX + 0.10F, 0.98F, frontZ + 0.16F},
                     {frontX - 0.10F, 1.39F, backZ - 0.16F}}, body, 2);
    // Module 601 keeps the breakable strip coupled to its monitor prop.
    addFurniture({{frontX - 0.12F, 1.11F, frontZ + 0.25F},
                     {frontX - 0.065F, 1.27F, backZ - 0.25F}},
        equipment, room.theme == RoomTheme::Security
                && room.moduleId != 601U ? 3 : 2);
    if (room.theme != RoomTheme::Office) {
        // Small rooms use the same assembled metal furniture vocabulary as
        // the authored halls. Drawer fronts and fasteners stay on the face
        // of the existing station, without enlarging its floor footprint.
        const float faceX = frontX + 0.014F;
        const auto addCabinetDetail = [&](const wkc::core::Aabb bounds,
                                          const wkc::core::Vec3 tint) {
            addPart(parts, frame, bounds, tint, true);
            parts.back().structuralColumn = true;
        };
        for (const float y : {0.15F, 0.38F, 0.61F}) {
            addFurniture({{faceX, y, frontZ + 0.10F},
                    {faceX + 0.022F, y + 0.18F, backZ - 0.10F}},
                top, room.theme == RoomTheme::Security ? 3 : 1);
            addCabinetDetail({{faceX + 0.022F, y + 0.075F,
                        backZ - 0.43F},
                    {faceX + 0.049F, y + 0.10F,
                        backZ - 0.20F}}, equipment);
        }
        for (const float z : {frontZ + 0.16F, backZ - 0.20F}) {
            for (const float y : {0.17F, 0.76F}) {
                addCabinetDetail({{faceX + 0.022F, y, z},
                        {faceX + 0.044F, y + 0.032F, z + 0.032F}},
                    equipment);
            }
        }
    }
    if (room.theme == RoomTheme::Cafeteria
        && (room.moduleId == 701U || room.moduleId == 702U)) {
        // Compact and medium rooms keep their right-hand prop zone. Low
        // corner planters fit outside it and away from the end doors.
        for (const float z : {-halfZ + 1.10F, halfZ - 1.10F}) {
            addPart(parts, frame,
                {{halfX - 0.64F, 0.0F, z - 0.29F},
                    {halfX - 0.08F, 0.60F, z + 0.29F}},
                cafeteriaDark, true, true);
            parts.back().decoration = RoomDecorationPart::Planter;
        }
    }
    if (room.theme == RoomTheme::Laboratory && room.moduleId == 403U) {
        // The secret laboratory's sample vaults frame the north wall rather
        // than filling its centre. They leave the possible north connector,
        // south-west key, east prop pocket and central enemy lane clear.
        const float faceZ = halfZ - 0.58F;
        const float rearZ = halfZ - 0.10F;
        const auto addLabPart = [&](const wkc::core::Aabb bounds,
                                  const wkc::core::Vec3 tint,
                                  const LaboratoryFurniturePart kind) {
            addPart(parts, frame, bounds, tint, true);
            parts.back().laboratoryFurniturePart = kind;
        };
        for (const float leftX : {-2.52F, 1.22F}) {
            const float rightX = leftX + 1.30F;
            addLabPart({{leftX, 0.0F, faceZ},
                           {rightX, 0.14F, rearZ}},
                laboratorySteel, LaboratoryFurniturePart::BenchBase);
            addLabPart({{leftX + 0.06F, 0.14F, faceZ + 0.10F},
                           {rightX - 0.06F, 2.10F, rearZ}},
                laboratoryCabinet, LaboratoryFurniturePart::ContainmentBody);
            // The recessed sample panel and its raised frame have physical
            // surfaces, keeping bullet hits aligned with the visible face.
            addLabPart({{leftX + 0.08F, 0.31F, faceZ + 0.02F},
                           {rightX - 0.08F, 1.91F, faceZ + 0.09F}},
                {0.09F, 0.20F, 0.26F}, LaboratoryFurniturePart::None);
            for (const float x : {leftX + 0.07F, rightX - 0.14F}) {
                addLabPart({{x, 0.14F, faceZ},
                               {x + 0.07F, 2.10F, faceZ + 0.11F}},
                    laboratorySteel, LaboratoryFurniturePart::None);
            }
            addLabPart({{leftX, 2.10F, faceZ},
                           {rightX, 2.22F, rearZ}},
                laboratorySteel, LaboratoryFurniturePart::BenchTop);
            addLabPart({{leftX + 0.22F, 0.70F, faceZ - 0.01F},
                           {rightX - 0.22F, 1.62F, faceZ + 0.02F}},
                {0.06F, 0.14F, 0.18F}, LaboratoryFurniturePart::None);
            addLabPart({{leftX + 0.16F, 1.77F, faceZ - 0.025F},
                           {rightX - 0.16F, 1.81F, faceZ - 0.005F}},
                laboratoryAccent, LaboratoryFurniturePart::StatusStrip);
        }
    }
    // Offices use a separate upper archive cabinet to break up the wall
    // silhouette. It has a visible body and shallow door seam.
    if (room.theme == RoomTheme::Office) {
        addFurniture({{wallX, 1.75F, frontZ},
                         {wallX + 0.36F, 2.35F, backZ}}, body, 0);
        addFurniture({{wallX + 0.36F, 1.82F, frontZ + 0.08F},
                         {wallX + 0.40F, 2.28F, backZ - 0.08F}}, top, 1);
        // Match the mechanical construction used by the approved furniture:
        // separate drawer fronts, handles, a recessed upper door and bolts.
        // Every piece stays within the workstation's existing footprint.
        const auto officeDetail = [&](const wkc::core::Aabb bounds,
                                      const wkc::core::Vec3 tint) {
            addPart(parts, frame, bounds, tint, true);
            parts.back().structuralColumn = true;
        };
        const float faceX = frontX + 0.07F;
        for (const float y : {0.20F, 0.43F, 0.66F}) {
            officeDetail({{faceX, y, frontZ + 0.12F},
                    {faceX + 0.014F, y + 0.18F, backZ - 0.12F}},
                {0.31F, 0.35F, 0.35F});
            officeDetail({{faceX + 0.014F, y + 0.075F,
                    backZ - 0.38F},
                    {faceX + 0.045F, y + 0.105F,
                        backZ - 0.20F}},
                {0.53F, 0.58F, 0.57F});
        }
        for (const float z : {frontZ + 0.14F, backZ - 0.18F}) {
            for (const float y : {0.16F, 0.91F}) {
                officeDetail({{faceX + 0.014F, y, z},
                        {faceX + 0.036F, y + 0.035F, z + 0.035F}},
                    {0.54F, 0.58F, 0.56F});
            }
        }
        officeDetail({{wallX + 0.40F, 1.88F, frontZ + 0.13F},
                {wallX + 0.425F, 2.21F, backZ - 0.13F}},
            {0.18F, 0.23F, 0.25F});
        officeDetail({{wallX + 0.425F, 2.00F, backZ - 0.42F},
                {wallX + 0.46F, 2.05F, backZ - 0.23F}},
            {0.46F, 0.54F, 0.55F});
        if (room.moduleId == 303U && halfX >= 7.75F
            && halfZ >= 8.75F && span.y >= 5.8F) {
            // The hall needs more than the compact wall station. Two low
            // archive desks occupy the west flank, leaving the central
            // encounter, east prop pocket, south-west loot and all four
            // door approaches open. Their legs, drawer banks and displays
            // share the same collision geometry as the visible pieces.
            const auto addArchiveDesk = [&](const float nearZ,
                                            const float farZ) {
                constexpr float rearX = -6.65F;
                constexpr float frontDeskX = -4.55F;
                const wkc::core::Vec3 frameTint{0.19F, 0.22F, 0.24F};
                const wkc::core::Vec3 drawerTint{0.35F, 0.39F, 0.40F};
                const wkc::core::Vec3 screenTint{0.16F, 0.48F, 0.55F};
                for (const float x : {rearX + 0.10F, frontDeskX - 0.18F}) {
                    for (const float z : {nearZ + 0.12F, farZ - 0.20F}) {
                        addFurniture({{x, 0.0F, z},
                                         {x + 0.08F, 0.86F, z + 0.08F}},
                            frameTint, 0);
                    }
                }
                addFurniture({{rearX, 0.84F, nearZ},
                                 {frontDeskX, 0.96F, farZ}}, top, 1);
                // A shallow service rail visibly joins the two leg pairs.
                addFurniture({{rearX + 0.13F, 0.18F, nearZ + 0.09F},
                                 {rearX + 0.21F, 0.30F, farZ - 0.09F}},
                    frameTint, 0);
                addFurniture({{frontDeskX - 0.86F, 0.08F, nearZ + 0.12F},
                                 {frontDeskX - 0.08F, 0.81F,
                                     nearZ + 0.72F}}, body, 0);
                for (const float y : {0.29F, 0.53F, 0.74F}) {
                    addFurniture({{frontDeskX - 0.075F, y - 0.07F,
                                      nearZ + 0.18F},
                                     {frontDeskX - 0.035F, y + 0.07F,
                                         nearZ + 0.66F}}, drawerTint, 1);
                    addFurniture({{frontDeskX - 0.030F, y - 0.008F,
                                      nearZ + 0.35F},
                                     {frontDeskX - 0.010F, y + 0.008F,
                                         nearZ + 0.49F}}, equipment, 2);
                }
                const float displayZ = (nearZ + farZ) * 0.5F + 0.18F;
                addFurniture({{rearX + 0.60F, 0.96F, displayZ - 0.08F},
                                 {rearX + 0.72F, 1.13F,
                                     displayZ + 0.08F}}, frameTint, 2);
                addFurniture({{rearX + 0.43F, 1.12F, displayZ - 0.39F},
                                 {rearX + 0.78F, 1.55F,
                                     displayZ + 0.39F}}, frameTint, 2);
                addFurniture({{rearX + 0.785F, 1.18F,
                                  displayZ - 0.33F},
                                 {rearX + 0.805F, 1.49F,
                                     displayZ + 0.33F}}, screenTint, 2);
            };
            addArchiveDesk(-5.35F, -3.65F);
            addArchiveDesk(4.15F, 6.35F);
        }
    }
    {
        // Compact rooms previously stopped at flat ceiling panels, unlike
        // the authored halls. A shallow physical service run now ties every
        // theme to the same constructed hard-surface language.
        addPart(parts, frame,
            {{halfX - 1.02F, span.y - 0.70F, -halfZ + 0.25F},
                {halfX - 0.42F, span.y - 0.35F, halfZ - 0.25F}},
            room.theme == RoomTheme::Office
                ? wkc::core::Vec3{0.29F, 0.34F, 0.35F}
                : room.theme == RoomTheme::Security
                    ? wkc::core::Vec3{0.22F, 0.31F, 0.36F}
                    : room.theme == RoomTheme::Laboratory
                        ? wkc::core::Vec3{0.38F, 0.48F, 0.50F}
                        : room.theme == RoomTheme::Cafeteria
                            ? wkc::core::Vec3{0.42F, 0.43F, 0.40F}
                    : wkc::core::Vec3{0.31F, 0.30F, 0.28F}, true);
        parts.back().decoration = RoomDecorationPart::CeilingDuctMuted;
    }
    if (room.role == RoomRole::Arena) {
        // Shallow wall equipment gives the compact arenas the same secondary
        // geometry as the authored rooms while leaving the combat floor clear.
        const wkc::core::Vec3 cabinet = room.theme == RoomTheme::Security
            ? wkc::core::Vec3{0.27F, 0.36F, 0.40F}
            : wkc::core::Vec3{0.33F, 0.32F, 0.30F};
        const wkc::core::Vec3 accent = room.theme == RoomTheme::Security
            ? wkc::core::Vec3{0.27F, 0.51F, 0.57F}
            : wkc::core::Vec3{0.54F, 0.38F, 0.18F};
        for (const float x : {-halfX + 1.50F, halfX - 2.65F}) {
            addPart(parts, frame,
                {{x, span.y - 1.47F, halfZ - 0.49F},
                    {x + 1.15F, span.y - 0.54F, halfZ - 0.12F}},
                cabinet, true);
            parts.back().structuralColumn = true;
            addPart(parts, frame,
                {{x + 0.10F, span.y - 1.35F, halfZ - 0.52F},
                    {x + 1.05F, span.y - 0.65F, halfZ - 0.48F}},
                {0.14F, 0.18F, 0.19F}, true);
            parts.back().structuralColumn = true;
            addPart(parts, frame,
                {{x + 0.18F, span.y - 0.88F, halfZ - 0.54F},
                    {x + 0.97F, span.y - 0.82F, halfZ - 0.51F}},
                accent, true);
            for (const float boltX : {x + 0.16F, x + 0.96F}) {
                for (const float y : {span.y - 1.29F, span.y - 0.73F}) {
                    addPart(parts, frame,
                        {{boltX, y, halfZ - 0.55F},
                            {boltX + 0.035F, y + 0.035F,
                                halfZ - 0.52F}},
                        {0.48F, 0.51F, 0.50F}, true);
                }
            }
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
        // Two compact specimen pots sit entirely on each top. They become
        // dynamic props so glass and pot can break in separate hits.
        for (const float offsetZ : {-0.66F, 0.66F}) {
            addPart(parts, frame,
                {{benchCenter.x - 0.22F, 0.92F,
                    benchCenter.z + offsetZ - 0.22F},
                    {benchCenter.x + 0.22F, 1.08F,
                        benchCenter.z + offsetZ + 0.22F}},
                laboratorySteel, true, true);
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
    // Rotated bounds at fractional world coordinates can round a nominal
    // 20 m span a few ULPs below 20. Match the containment tolerance below.
    constexpr float layoutTolerance = 0.001F;
    if (localWidth + layoutTolerance < 16.0F
        || localLength + layoutTolerance < 20.0F
        || span.y + layoutTolerance < 5.5F) {
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
            parts.back().technicalFurniturePart = TechnicalFurniturePart::BenchBase;
            addPart(parts, frame,
                {{minimumX, 0.84F, benchMinZ},
                    {maximumX, 0.98F, benchMaxZ}},
                technicalSteel, true);
            parts.back().technicalFurniturePart = TechnicalFurniturePart::BenchTop;

            const float toolMinX = side > 0.0F ? 7.05F : -7.75F;
            const float toolMaxX = side > 0.0F ? 7.75F : -7.05F;
            const float toolMinZ = end > 0.0F ? 3.2F : -4.7F;
            addPart(parts, frame,
                {{toolMinX, 0.98F, toolMinZ},
                    {toolMaxX, 1.50F, toolMinZ + 1.50F}},
                technicalCabinet, true);
            parts.back().technicalFurniturePart = TechnicalFurniturePart::Instrument;

            // The heavy generator is sealed and opaque. Its plinth touches
            // both bench and wall, keeping collision equal to its silhouette.
            addPart(parts, frame,
                {{minimumX, 0.0F, generatorMinZ},
                    {maximumX, 0.25F, generatorMaxZ}},
                technicalSteel, true);
            parts.back().technicalFurniturePart = TechnicalFurniturePart::Generator;
            addPart(parts, frame,
                {{minimumX, 0.25F, generatorMinZ},
                    {maximumX, 2.25F, generatorMaxZ}},
                technicalCabinet, true);
            parts.back().technicalFurniturePart = TechnicalFurniturePart::Generator;
            const float panelMinX = side > 0.0F
                ? minimumX - 0.05F : maximumX;
            addPart(parts, frame,
                {{panelMinX, 0.75F, generatorMinZ + 0.38F},
                    {panelMinX + 0.05F, 1.72F,
                        generatorMaxZ - 0.38F}},
                technicalPipe, true);
            parts.back().technicalFurniturePart = TechnicalFurniturePart::Generator;
        }

        // A pipe run remains near the wall and above player head height.
        // Two drops meet its ends, without overlapping the machinery below.
        const float pipeMinX = side > 0.0F ? 7.68F : -7.82F;
        const float pipeMaxX = pipeMinX + 0.14F;
        addPart(parts, frame,
            {{pipeMinX, 4.62F, -8.50F},
                {pipeMaxX, 4.78F, 8.50F}},
            technicalPipe, true);
        parts.back().technicalFurniturePart = TechnicalFurniturePart::ServicePipe;
        for (const float z : {-8.62F, 8.50F}) {
            addPart(parts, frame,
                {{pipeMinX, 0.0F, z},
                    {pipeMaxX, 4.78F, z + 0.12F}},
                technicalPipe, true);
            parts.back().technicalFurniturePart = TechnicalFurniturePart::ServicePipe;
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
        parts.back().technicalFurniturePart = TechnicalFurniturePart::BenchBase;
        addPart(parts, frame,
            {{center.x - 0.80F, 0.78F, center.z - 1.20F},
                {center.x + 0.80F, 0.92F, center.z + 1.20F}},
            technicalSteel, true);
        parts.back().technicalFurniturePart = TechnicalFurniturePart::BenchTop;
        const float instrumentX = center.x
            + (center.x > 0.0F ? 0.25F : -0.25F);
        addPart(parts, frame,
            {{instrumentX - 0.26F, 0.92F, center.z - 0.35F},
                {instrumentX + 0.26F, 1.32F, center.z + 0.35F}},
            center.x < 0.0F ? oliveCargo : charcoalCargo,
            true, true);
        parts.back().technicalFurniturePart = TechnicalFurniturePart::Instrument;
    }
    // Exposed service ducts flank the ceiling fixtures and meet the walls.
    // In the L variant, the right duct begins at the inner return instead
    // of crossing the removed south-east corner.
    const bool lFootprint = room.moduleId == technicalLPreviewModuleId
        || room.moduleId == maintenanceLModuleId;
    for (const float x : {-5.85F, 5.85F}) {
        const float southWall = lFootprint && x > 0.0F ? -2.0F : -10.0F;
        addPart(parts, frame,
            {{x - 0.26F, 5.05F, southWall},
                {x + 0.26F, 5.33F, 10.0F}},
            technicalSteel, true);
        parts.back().decoration = RoomDecorationPart::CeilingDuct;
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
    parts.reserve(24U);

    // The two stations use the approved Security T furniture family. They
    // face into the room and leave the centre of the far wall clear for a
    // doorway. Their collision remains the same simple base/top/monitor set.
    for (const float x : {-8.8F, 4.7F}) {
        addPart(parts, frame,
            {{x, 0.0F, -9.0F}, {x + 4.1F, 0.9F, -7.9F}},
            darkSteel, true);
        parts.back().securityFurniturePart = SecurityFurniturePart::ConsoleBase;
        addPart(parts, frame,
            {{x, 0.9F, -9.0F}, {x + 4.1F, 1.05F, -7.9F}},
            combatSteel, true);
        parts.back().securityFurniturePart = SecurityFurniturePart::ConsoleTop;
        addPart(parts, frame,
            {{x + 0.8F, 1.05F, -9.0F}, {x + 3.3F, 1.95F, -8.7F}},
            cafeteriaDark, true, true);
        parts.back().securityFurniturePart = SecurityFurniturePart::Monitor;
    }

    // The display is mounted in a narrow equipment housing, distinct from
    // the room's structural columns. The base remains solid for collision.
    for (const float z : {2.46F, 5.60F}) {
        addPart(parts, frame,
            {{8.62F, 0.0F, z}, {9.0F, 3.40F, z + 0.14F}},
            combatSteel, true);
        parts.back().securityFurniturePart = SecurityFurniturePart::ObservationSurround;
    }
    addPart(parts, frame,
        {{8.62F, 0.0F, 2.60F}, {9.0F, 0.92F, 5.60F}},
        darkSteel, true);
    parts.back().securityFurniturePart = SecurityFurniturePart::ObservationSurround;
    addPart(parts, frame,
        {{8.62F, 3.25F, 2.60F}, {9.0F, 3.40F, 5.60F}},
        combatSteel, true);
    parts.back().securityFurniturePart = SecurityFurniturePart::ObservationSurround;
    addPart(parts, frame,
        {{8.92F, 0.92F, 2.60F}, {9.0F, 3.25F, 5.60F}},
        cafeteriaDark, true);
    parts.back().securityFurniturePart = SecurityFurniturePart::ObservationPanel;

    // A wall-connected cabinet and two low, staggered barricades provide
    // cover while leaving the central and side routes open at both ends.
    addPart(parts, frame,
        {{-9.0F, 0.0F, 3.0F}, {-8.2F, 2.2F, 5.6F}},
        darkSteel, true);
    parts.back().securityFurniturePart = SecurityFurniturePart::Cabinet;
    for (const wkc::core::Vec3 center : {
             wkc::core::Vec3{-4.20F, 0.0F, 3.80F},
             wkc::core::Vec3{4.20F, 0.0F, -3.80F}}) {
        addPart(parts, frame,
            {{center.x - 0.75F, 0.0F, center.z - 0.90F},
                {center.x + 0.75F, 1.05F, center.z + 0.90F}},
            combatSteel, true);
        parts.back().securityFurniturePart = SecurityFurniturePart::Barricade;
    }
    for (const float x : {-8.3F, 8.3F}) {
        addPart(parts, frame,
            {{x - 0.35F, 0.0F, -7.0F},
                {x + 0.35F, 0.68F, -6.3F}},
            darkSteel, true);
        parts.back().decoration = RoomDecorationPart::Planter;
    }
    // Two suspended sections form a continuous L: a wall-to-wall run beside
    // the observation niche and a branch across the far side. The branch
    // meets the main run at its side instead of overlapping its volume.
    addPart(parts, frame,
        {{6.65F, 5.18F, -8.65F}, {7.35F, 5.58F, 8.65F}},
        darkSteel, true);
    parts.back().decoration = RoomDecorationPart::CeilingDuct;
    addPart(parts, frame,
        {{-8.65F, 5.18F, -7.15F}, {6.65F, 5.58F, -6.45F}},
        darkSteel, true);
    parts.back().decoration = RoomDecorationPart::CeilingDuct;
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
            cafeteriaDark, true, true);
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
        {0.25F, 0.23F, 0.20F}, true);
    parts.back().structuralColumn = true;
    parts.back().decoration = RoomDecorationPart::CentralHubColumnPlinth;
    addPart(parts, frame,
        {{-1.20F, 0.35F, -1.20F}, {1.20F, 5.55F, 1.20F}},
        combatSteel, true);
    parts.back().structuralColumn = true;
    parts.back().decoration = RoomDecorationPart::CentralHubColumnShaft;
    addPart(parts, frame,
        {{-1.45F, 5.55F, -1.45F}, {1.45F, 5.78F, 1.45F}},
        {0.25F, 0.23F, 0.20F}, true);
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
        || room.moduleId == centralHubPreviewModuleId
        || room.moduleId == combatRoomPreviewModuleId
        || room.moduleId == combatGalleryArenaModuleId) {
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

bool usesAuthoredRoomArchitecture(const RoomInstance& room) noexcept
{
    if (room.moduleId == industrialCorridorDestinationPreviewModuleId
        || room.moduleId == industrialCorridorStartPreviewModuleId) {
        return false;
    }
    return room.role == RoomRole::Boss
        || room.moduleId == bossRoomPreviewModuleId
        || room.theme == RoomTheme::Storage
        || room.theme == RoomTheme::Office
        || room.theme == RoomTheme::Cafeteria
        || room.theme == RoomTheme::Security
        || room.theme == RoomTheme::Laboratory
        || room.theme == RoomTheme::Maintenance
        || (room.theme == RoomTheme::Industrial
            && room.role != RoomRole::Hub)
        || room.moduleId == combatRoomPreviewModuleId
        || room.moduleId == combatGalleryArenaModuleId
        || room.moduleId == cafeteriaPreviewModuleId
        || room.moduleId == cafeteriaLongPreviewModuleId
        || room.moduleId == cafeteriaLongModuleId
        || room.moduleId == laboratoryPreviewModuleId
        || room.moduleId == laboratoryHallModuleId
        || room.moduleId == technicalPreviewModuleId
        || room.moduleId == technicalLPreviewModuleId
        || room.moduleId == maintenanceLModuleId
        || room.moduleId == securityPreviewModuleId
        || room.moduleId == securityHallModuleId
        || room.moduleId == securityTPreviewModuleId
        || room.moduleId == securityTModuleId
        || room.moduleId == centralHubPreviewModuleId
        || room.moduleId == centralHubModuleId;
}

std::vector<RoomArchitecturePart> makeRoomArchitecture(const RoomInstance& room)
{
    if (room.moduleId == industrialCorridorDestinationPreviewModuleId
        || room.moduleId == industrialCorridorStartPreviewModuleId) {
        return {};
    }
    if (room.role == RoomRole::Boss
        || room.moduleId == bossRoomPreviewModuleId) {
        if (!room.worldBounds.isValid()) return {};
        const LocalFrame frame{room.transform.position, room.rotation};
        const float height = room.worldBounds.maximum.y - room.worldBounds.minimum.y;
        std::vector<RoomArchitecturePart> parts;
        parts.reserve(128U);
        for (const float z : {-8.0F, 8.0F}) {
            const auto columnPart = [&](const wkc::core::Aabb bounds,
                                        const wkc::core::Vec3 tint,
                                        const bool structural,
                                        const CombatArchitecturePart kind) {
                addPart(parts, frame, bounds, tint, true);
                parts.back().structuralColumn = structural;
                parts.back().combatPart = kind;
            };
            columnPart({{6.2F, 0.0F, z - 0.8F},
                           {7.8F, height - 0.1F, z + 0.8F}},
                combatSteel, true, CombatArchitecturePart::ColumnShaft);
            columnPart({{6.0F, 0.0F, z - 1.0F},
                           {8.0F, 0.30F, z + 1.0F}},
                darkSteel, true, CombatArchitecturePart::ColumnTrim);
            columnPart({{6.0F, height - 0.30F, z - 1.0F},
                           {8.0F, height, z + 1.0F}},
                darkSteel, true, CombatArchitecturePart::ColumnTrim);
            columnPart({{6.12F, 1.45F, z - 0.88F},
                           {7.88F, 1.67F, z + 0.88F}},
                combatAccent, false, CombatArchitecturePart::ColumnBrace);
            // Shallow light armor follows the existing collision shaft. The
            // central firing lane keeps the same unobstructed width.
            for (const float x : {6.08F, 7.80F}) {
                columnPart({{x, 0.43F, z - 0.56F},
                               {x + 0.12F, height - 0.48F, z + 0.56F}},
                    {0.74F, 0.70F, 0.63F}, false,
                    CombatArchitecturePart::ColumnInset);
            }
            for (const float y : {0.62F, 3.12F, 5.67F}) {
                for (const float side : {-1.0F, 1.0F}) {
                    columnPart({{6.02F, y, z + side * 0.42F},
                                   {6.08F, y + 0.06F,
                                       z + side * 0.42F + 0.06F}},
                        darkSteel, false, CombatArchitecturePart::GalleryFastener);
                }
            }
            columnPart({{-11.8F, height - 0.38F, z - 0.17F},
                           {6.20F, height - 0.12F, z + 0.17F}},
                darkSteel, false, CombatArchitecturePart::CeilingBeam);
        }
        // The ceiling run and service grilles are above player and enemy
        // clearance. They keep door openings and the sniper lane untouched.
        addPart(parts, frame,
            {{-11.4F, height - 0.65F, -13.4F},
                {-10.65F, height - 0.25F, 13.4F}},
            darkSteel, true);
        parts.back().decoration = RoomDecorationPart::CeilingDuct;
        for (const float side : {-1.0F, 1.0F}) {
            const auto bossPart = [&](const wkc::core::Aabb bounds,
                                      const wkc::core::Vec3 tint,
                                      const CombatArchitecturePart kind) {
                addPart(parts, frame, bounds, tint, true);
                parts.back().combatPart = kind;
            };
            // Surface-mounted electrical cabinets repeat at the room's four
            // corners. Their bodies have real bullet collision, but project
            // less than half a metre into the arena and never enter a spawn
            // zone, door opening or the terminal approach.
            const float bodyInner = side < 0.0F ? -13.30F : 13.30F;
            const float bodyOuter = side < 0.0F ? -13.84F : 13.84F;
            const float faceInner = side < 0.0F ? -13.20F : 13.20F;
            const float faceOuter = side < 0.0F ? -13.30F : 13.30F;
            for (const float z : {-9.6F, 9.6F}) {
                bossPart({{std::min(bodyInner, bodyOuter), 0.90F, z - 1.14F},
                              {std::max(bodyInner, bodyOuter), 3.55F, z + 1.14F}},
                    darkSteel, CombatArchitecturePart::GallerySupport);
                for (const float bracketZ : {z - 0.76F, z + 0.76F}) {
                    bossPart({{std::min(bodyInner, bodyOuter), 0.68F,
                                   bracketZ - 0.10F},
                                  {std::max(bodyInner, bodyOuter), 0.90F,
                                      bracketZ + 0.10F}},
                        darkSteel, CombatArchitecturePart::GalleryBracket);
                }
                bossPart({{std::min(faceInner, faceOuter), 1.10F, z - 0.92F},
                              {std::max(faceInner, faceOuter), 3.32F, z + 0.92F}},
                    {0.68F, 0.65F, 0.60F}, CombatArchitecturePart::ColumnInset);
                // Paired metal doors, dark centre seam and a narrow amber
                // status strip give the cabinet its own readable silhouette.
                for (const float offset : {-0.46F, 0.46F}) {
                    bossPart({{std::min(faceInner, faceOuter) - 0.015F,
                                   1.17F, z + offset - 0.40F},
                                  {std::max(faceInner, faceOuter) + 0.015F,
                                      3.23F, z + offset + 0.40F}},
                        {0.43F, 0.42F, 0.39F},
                        CombatArchitecturePart::CeilingPanel);
                }
                bossPart({{std::min(faceInner, faceOuter) - 0.025F,
                               2.90F, z - 0.68F},
                              {std::max(faceInner, faceOuter) + 0.025F,
                                  3.02F, z + 0.68F}},
                    combatAccent, CombatArchitecturePart::ColumnBrace);
                for (const float fastenerZ : {z - 0.78F, z + 0.78F}) {
                    for (const float fastenerY : {1.28F, 3.16F}) {
                        bossPart({{std::min(faceInner, faceOuter) - 0.035F,
                                       fastenerY, fastenerZ - 0.035F},
                                      {std::max(faceInner, faceOuter) + 0.035F,
                                          fastenerY + 0.07F, fastenerZ + 0.035F}},
                            combatAccent,
                            CombatArchitecturePart::GalleryFastener);
                    }
                }
            }
            // Two continuous cable conduits run above the door heads. Short
            // risers terminate on each cabinet; their wall-side placement
            // keeps the silhouette without reducing the walking lanes.
            const float pipeX = side < 0.0F ? -13.55F : 13.55F;
            for (const float y : {4.05F, 4.28F}) {
                bossPart({{pipeX - 0.055F, y, -14.05F},
                              {pipeX + 0.055F, y + 0.11F, 14.05F}},
                    darkSteel, CombatArchitecturePart::CeilingBeam);
            }
            for (const float z : {-12.4F, -4.2F, 4.2F, 12.4F}) {
                bossPart({{std::min(pipeX, bodyOuter), 4.01F, z - 0.09F},
                              {std::max(pipeX, bodyOuter), 4.44F, z + 0.09F}},
                    darkSteel, CombatArchitecturePart::CeilingBrace);
            }
            for (const float z : {-9.6F, 9.6F}) {
                for (const float riserZ : {z - 0.66F, z + 0.66F}) {
                    bossPart({{pipeX - 0.06F, 3.52F, riserZ - 0.055F},
                                  {pipeX + 0.06F, 4.16F, riserZ + 0.055F}},
                        darkSteel, CombatArchitecturePart::CeilingBrace);
                }
            }
            // Recessed grilles sit beside the ceiling duct. The louvres are
            // thin, non-shadow-casting metal instead of a flat dark rectangle.
            const float grilleX = side < 0.0F ? -9.96F : 9.96F;
            for (const float z : {-5.5F, 5.5F}) {
                bossPart({{grilleX - 0.68F, height - 0.52F, z - 1.05F},
                              {grilleX + 0.68F, height - 0.40F, z + 1.05F}},
                    {0.11F, 0.12F, 0.12F},
                    CombatArchitecturePart::CeilingGrille);
                for (const float slat : {-0.72F, -0.36F, 0.0F, 0.36F, 0.72F}) {
                    bossPart({{grilleX - 0.56F, height - 0.535F,
                                   z + slat - 0.045F},
                                  {grilleX + 0.56F, height - 0.51F,
                                      z + slat + 0.045F}},
                        combatSteel, CombatArchitecturePart::CeilingGrille);
                }
            }
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
    if (room.moduleId == 703U) {
        return room.worldBounds.isValid()
            ? makeCafeteriaHallArchitecture(room)
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
    if (room.moduleId == securityPreviewModuleId
        || room.moduleId == securityHallModuleId) {
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
    if (room.theme == RoomTheme::Industrial
        && room.role != RoomRole::Hub && room.worldBounds.isValid()) {
        const wkc::core::Vec3 casingSteel{0.25F, 0.24F, 0.22F};
        const wkc::core::Vec3 insetSteel{0.17F, 0.17F, 0.16F};
        const wkc::core::Vec3 faceSteel{0.19F, 0.18F, 0.17F};
        const wkc::core::Vec3 edgeSteel{0.38F, 0.36F, 0.33F};
        const wkc::core::Vec3 hardwareSteel{0.47F, 0.45F, 0.42F};
        const wkc::core::Vec3 statusAmber{0.69F, 0.35F, 0.09F};
        const wkc::core::Vec3 span = room.worldBounds.maximum
            - room.worldBounds.minimum;
        const bool quarterTurn = room.rotation == wkc::core::QuarterTurn::Degrees90
            || room.rotation == wkc::core::QuarterTurn::Degrees270;
        const float halfX = (quarterTurn ? span.z : span.x) * 0.5F;
        const float halfZ = (quarterTurn ? span.x : span.z) * 0.5F;
        const float height = span.y;
        const LocalFrame frame{
            {(room.worldBounds.minimum.x + room.worldBounds.maximum.x) * 0.5F,
                room.worldBounds.minimum.y,
                (room.worldBounds.minimum.z + room.worldBounds.maximum.z) * 0.5F},
            room.rotation,
        };
        std::vector<RoomArchitecturePart> parts;
        parts.reserve(96U);
        // The service duct follows a side wall, clear of the central ceiling
        // fixture, every door and the player spawn. Its detailed ribs and
        // vents are generated by the same 3D duct path as other rooms.
        addPart(parts, frame,
            {{halfX - 1.00F, height - 0.72F, -halfZ + 0.22F},
                {halfX - 0.48F, height - 0.38F, halfZ - 0.22F}},
            casingSteel, true);
        parts.back().decoration = RoomDecorationPart::CeilingDuct;

        // A wall-mounted equipment cabinet gives the safe spawn room a
        // physical landmark without reintroducing the four checkerboard
        // blocks from the renderer fallback. All visible pieces have matching
        // collision and stay inside the door and spawn clearances.
        const float wallX = -halfX + 0.07F;
        const float frontX = -halfX + 0.58F;
        const float cabinetZ = halfZ - 2.15F;
        const auto cabinetPart = [&](const wkc::core::Aabb bounds,
                                     const wkc::core::Vec3 tint) {
            addPart(parts, frame, bounds, tint, true);
            parts.back().structuralColumn = true;
        };
        cabinetPart({{wallX, 0.08F, cabinetZ},
                {frontX, 2.26F, cabinetZ + 1.56F}},
            casingSteel);
        cabinetPart({{frontX, 0.29F, cabinetZ + 0.09F},
                {frontX + 0.045F, 2.13F, cabinetZ + 1.47F}},
            faceSteel);
        for (const float z : {cabinetZ + 0.09F, cabinetZ + 1.43F}) {
            cabinetPart({{frontX + 0.04F, 0.24F, z},
                    {frontX + 0.085F, 2.18F, z + 0.045F}},
                edgeSteel);
        }
        cabinetPart({{frontX + 0.04F, 1.46F, cabinetZ + 0.25F},
                {frontX + 0.085F, 1.55F, cabinetZ + 1.31F}},
            statusAmber);
        cabinetPart({{frontX + 0.04F, 0.67F, cabinetZ + 0.64F},
                {frontX + 0.09F, 0.87F, cabinetZ + 0.91F}},
            hardwareSteel);
        for (const float y : {1.79F, 1.89F, 1.99F}) {
            cabinetPart({{frontX + 0.084F, y,
                    cabinetZ + 0.34F},
                    {frontX + 0.096F, y + 0.026F,
                        cabinetZ + 1.22F}},
                insetSteel);
        }
        for (const float z : {cabinetZ + 0.18F, cabinetZ + 1.33F}) {
            for (const float y : {0.40F, 2.04F}) {
                cabinetPart({{frontX + 0.085F, y, z},
                        {frontX + 0.106F, y + 0.040F, z + 0.040F}},
                    hardwareSteel);
            }
        }

        // Industrial rooms need the same visible construction depth as the
        // furnished themes. Corner uprights and ceiling ties frame the room
        // without occupying the four central door approaches.
        const wkc::core::Vec3 ribSteel{0.30F, 0.29F, 0.27F};
        for (const float sideX : {-1.0F, 1.0F}) {
            const float columnX = sideX * (halfX - 0.29F);
            for (const float sideZ : {-1.0F, 1.0F}) {
                const float columnZ = sideZ * (halfZ - 1.15F);
                cabinetPart({{columnX - 0.15F, 0.0F, columnZ - 0.16F},
                        {columnX + 0.15F, height - 0.16F,
                            columnZ + 0.16F}}, ribSteel);
                cabinetPart({{columnX - 0.16F, 0.18F, columnZ - 0.08F},
                        {columnX + 0.16F, 0.25F, columnZ + 0.08F}},
                    insetSteel);
                cabinetPart({{columnX - 0.16F, height - 0.55F,
                                columnZ - 0.08F},
                        {columnX + 0.16F, height - 0.47F,
                            columnZ + 0.08F}}, insetSteel);
                for (const float boltY : {0.32F, height - 0.42F}) {
                    cabinetPart({{columnX - 0.028F, boltY,
                                    columnZ - 0.17F},
                            {columnX + 0.028F, boltY + 0.045F,
                                columnZ - 0.15F}},
                        hardwareSteel);
                }
            }
        }
        for (const float z : {-halfZ + 1.15F, halfZ - 1.15F}) {
            cabinetPart({{-halfX + 0.16F, height - 0.36F, z - 0.11F},
                    {halfX - 0.16F, height - 0.15F, z + 0.11F}}, ribSteel);
            for (const float x : {-halfX + 1.05F, halfX - 1.05F}) {
                cabinetPart({{x - 0.12F, height - 0.43F, z - 0.15F},
                        {x + 0.12F, height - 0.35F, z + 0.15F}},
                    insetSteel);
            }
        }

        // Four slim wall service banks keep the floor open but give the
        // Industrial rooms equipment at eye height, like the other themes.
        // Their depth is outside the enemy, loot and loose-prop pockets.
        for (const float side : {-1.0F, 1.0F}) {
            const float xMin = side < 0.0F
                ? -halfX + 0.10F : halfX - 0.49F;
            const float xMax = side < 0.0F
                ? -halfX + 0.49F : halfX - 0.10F;
            const float faceX = side < 0.0F
                ? xMax + 0.02F : xMin - 0.02F;
            for (const float zMin : {-2.70F, 1.35F}) {
                const float zMax = zMin + 1.35F;
                cabinetPart({{xMin, 0.62F, zMin},
                        {xMax, 2.34F, zMax}}, ribSteel);
                cabinetPart({{faceX - 0.024F, 0.78F, zMin + 0.11F},
                        {faceX + 0.024F, 2.14F, zMax - 0.11F}},
                    insetSteel);
                // Two inset access doors replace the stack of parallel bars.
                // Their raised catches make this read as equipment rather
                // than another striped steel surface.
                for (const float z : {zMin + 0.22F, zMin + 0.74F}) {
                    cabinetPart({{faceX - 0.039F, 0.96F, z},
                            {faceX + 0.039F, 1.44F, z + 0.42F}},
                        faceSteel);
                    cabinetPart({{faceX - 0.046F, 1.14F, z + 0.28F},
                            {faceX + 0.046F, 1.24F, z + 0.35F}},
                        hardwareSteel);
                }
                cabinetPart({{faceX - 0.038F, 1.75F,
                            zMin + 0.20F},
                        {faceX + 0.038F, 1.80F, zMax - 0.20F}},
                    statusAmber);
                for (const float z : {zMin + 0.16F, zMax - 0.20F}) {
                    for (const float y : {0.77F, 2.14F}) {
                        cabinetPart({{faceX - 0.044F, y, z},
                                {faceX + 0.044F, y + 0.040F,
                                    z + 0.040F}},
                            hardwareSteel);
                    }
                }
            }
        }

        // The route status assembly is above the connector height, so either
        // end can become a door without hiding the landmark or blocking it.
        for (const float side : {-1.0F, 1.0F}) {
            const float zMin = side < 0.0F
                ? -halfZ + 0.09F : halfZ - 0.25F;
            const float zMax = side < 0.0F
                ? -halfZ + 0.25F : halfZ - 0.09F;
            cabinetPart({{-1.32F, 2.61F, zMin},
                    {1.32F, 3.23F, zMax}}, ribSteel);
            cabinetPart({{-1.18F, 2.73F, zMin - 0.018F},
                    {1.18F, 3.12F, zMax + 0.018F}}, insetSteel);
            for (const float x : {-1.10F, -1.00F, -0.90F}) {
                addPart(parts, frame,
                    {{x, 2.81F, zMin - 0.030F},
                        {x + 0.046F, 3.04F, zMax + 0.030F}},
                    statusAmber, true);
                parts.back().decoration
                    = RoomDecorationPart::IndustrialStatusIndicator;
            }
            addPart(parts, frame,
                {{-0.47F, 2.90F, zMin - 0.030F},
                    {0.93F, 2.95F, zMax + 0.030F}},
                statusAmber, true);
            parts.back().decoration
                = RoomDecorationPart::IndustrialStatusIndicator;
        }
        for (const float side : {-1.0F, 1.0F}) {
            const float xMin = side < 0.0F
                ? -halfX + 0.09F : halfX - 0.25F;
            const float xMax = side < 0.0F
                ? -halfX + 0.25F : halfX - 0.09F;
            cabinetPart({{xMin, 2.61F, -1.32F},
                    {xMax, 3.23F, 1.32F}}, ribSteel);
            cabinetPart({{xMin - 0.018F, 2.73F, -1.18F},
                    {xMax + 0.018F, 3.12F, 1.18F}}, insetSteel);
            for (const float z : {-1.10F, -1.00F, -0.90F}) {
                addPart(parts, frame,
                    {{xMin - 0.030F, 2.81F, z},
                        {xMax + 0.030F, 3.04F, z + 0.046F}},
                    statusAmber, true);
                parts.back().decoration
                    = RoomDecorationPart::IndustrialStatusIndicator;
            }
            addPart(parts, frame,
                {{xMin - 0.030F, 2.90F, -0.47F},
                    {xMax + 0.030F, 2.95F, 0.93F}},
                statusAmber, true);
            parts.back().decoration
                = RoomDecorationPart::IndustrialStatusIndicator;
        }

        if (room.role == RoomRole::Start) {
            // The safe arrival bay has two shallow status cabinets beside the
            // forward exit. They read as useful infrastructure from spawn,
            // while the middle, the door and pickup zones stay open.
            const float rearZ = halfZ - 0.11F;
            const float faceZ = halfZ - 0.45F;
            for (const float x : {-2.75F, 1.65F}) {
                cabinetPart({{x, 0.28F, faceZ},
                        {x + 1.10F, 2.24F, rearZ}},
                    casingSteel);
                cabinetPart({{x + 0.08F, 0.45F, faceZ - 0.035F},
                        {x + 1.02F, 1.95F, faceZ + 0.012F}}, insetSteel);
                cabinetPart({{x + 0.15F, 1.32F, faceZ - 0.055F},
                        {x + 0.95F, 1.82F, faceZ - 0.032F}},
                    {0.12F, 0.115F, 0.105F});
                cabinetPart({{x + 0.20F, 1.68F, faceZ - 0.075F},
                        {x + 0.78F, 1.73F, faceZ - 0.050F}},
                    statusAmber);
                for (const float y : {0.60F, 0.83F, 1.06F}) {
                    cabinetPart({{x + 0.20F, y, faceZ - 0.055F},
                            {x + 0.84F, y + 0.11F, faceZ - 0.030F}},
                        edgeSteel);
                    cabinetPart({{x + 0.41F, y + 0.045F,
                                    faceZ - 0.075F},
                            {x + 0.62F, y + 0.065F,
                                faceZ - 0.052F}},
                        hardwareSteel);
                }
                for (const float edgeX : {x + 0.13F, x + 0.94F}) {
                    for (const float y : {0.43F, 2.02F}) {
                        cabinetPart({{edgeX, y, faceZ - 0.067F},
                                {edgeX + 0.038F, y + 0.038F,
                                    faceZ - 0.044F}},
                            hardwareSteel);
                    }
                }
            }
        }
        return parts;
    }
    if (room.theme == RoomTheme::Office
        || (room.theme == RoomTheme::Cafeteria
            && room.moduleId != cafeteriaLongModuleId)
        || (room.theme == RoomTheme::Security
            && room.moduleId != securityHallModuleId
            && room.moduleId != securityTModuleId)
        || (room.theme == RoomTheme::Laboratory
            && room.moduleId != laboratoryHallModuleId)
        || (room.theme == RoomTheme::Maintenance
            && room.moduleId != maintenanceLModuleId
            && room.moduleId != combatGalleryArenaModuleId)) {
        return room.worldBounds.isValid()
            ? makeCompactThemeArchitecture(room)
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
    if (room.moduleId == industrialCorridorDestinationPreviewModuleId
        || room.moduleId == industrialCorridorStartPreviewModuleId) {
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
        && room.moduleId != securityHallModuleId
        && room.moduleId != securityTPreviewModuleId
        && room.moduleId != securityTModuleId
        && room.moduleId != centralHubPreviewModuleId
        && room.moduleId != centralHubModuleId
        && room.theme != RoomTheme::Office
        && room.theme != RoomTheme::Cafeteria
        && room.theme != RoomTheme::Security
        && room.theme != RoomTheme::Laboratory
        && room.theme != RoomTheme::Maintenance
        && room.theme != RoomTheme::Industrial) {
        return parts.empty();
    }
    if (!room.worldBounds.isValid() || parts.empty()) {
        return false;
    }
    const float floor = room.worldBounds.minimum.y;
    for (const RoomArchitecturePart& part : parts) {
        // Quarter-turn transforms at fractional world coordinates can put a
        // wall-attached face a few ULPs past the same room face. Keep rejecting
        // real protrusions while accepting that rounding noise.
        constexpr float boundsTolerance = 0.001F;
        const bool contained = part.bounds.minimum.x
                >= room.worldBounds.minimum.x - boundsTolerance
            && part.bounds.minimum.y
                >= room.worldBounds.minimum.y - boundsTolerance
            && part.bounds.minimum.z
                >= room.worldBounds.minimum.z - boundsTolerance
            && part.bounds.maximum.x
                <= room.worldBounds.maximum.x + boundsTolerance
            && part.bounds.maximum.y
                <= room.worldBounds.maximum.y + boundsTolerance
            && part.bounds.maximum.z
                <= room.worldBounds.maximum.z + boundsTolerance;
        if (!part.solid || !part.bounds.isValid() || !contained
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
