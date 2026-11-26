#pragma once

#include "world/RoomModule.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace gdp::world {

// Isolated layout previews use separate IDs from campaign room modules.
inline constexpr RoomModuleId combatRoomPreviewModuleId = 900001U;
inline constexpr RoomModuleId cafeteriaPreviewModuleId = 900002U;
inline constexpr RoomModuleId cafeteriaLongPreviewModuleId = 900008U;
inline constexpr RoomModuleId laboratoryPreviewModuleId = 900003U;
inline constexpr RoomModuleId technicalPreviewModuleId = 900004U;
inline constexpr RoomModuleId securityPreviewModuleId = 900005U;
inline constexpr RoomModuleId centralHubPreviewModuleId = 900006U;
inline constexpr RoomModuleId technicalLPreviewModuleId = 900007U;
inline constexpr RoomModuleId bossRoomPreviewModuleId = 900009U;
inline constexpr RoomModuleId securityTPreviewModuleId = 900010U;
inline constexpr RoomModuleId industrialCorridorDestinationPreviewModuleId = 900011U;
inline constexpr RoomModuleId industrialCorridorStartPreviewModuleId = 900012U;

enum class StorageRackPart : std::uint8_t {
    None,
    Upright,
    Shelf,
    Lip,
    Bracket,
};

enum class CafeteriaFurniturePart : std::uint8_t {
    None,
    Steel,
    Seat,
    Support,
    Cabinet,
};

enum class RoomDecorationPart : std::uint8_t {
    None,
    Planter,
    CeilingDuct,
    BroadleafPlanter,
    GlassTestPlanter,
    SmallGlassTestPlanter,
    LaboratoryColumnIndicator,
    CentralHubColumnShaft,
    CentralHubColumnPlinth,
    CentralHubRouteDisplay,
    CeilingDuctMuted,
    IndustrialStatusIndicator,
};

enum class SecurityFurniturePart : std::uint8_t {
    None,
    ConsoleBase,
    ConsoleTop,
    Monitor,
    Cabinet,
    Barricade,
    ObservationPanel,
    ObservationSurround,
    ConsoleTrim,
};

enum class LaboratoryFurniturePart : std::uint8_t {
    None,
    BenchBase,
    BenchTop,
    Instrument,
    StatusStrip,
    ContainmentBase,
    ContainmentBody,
    ContainmentPanel,
    CeilingRib,
};

enum class TechnicalFurniturePart : std::uint8_t {
    None,
    BenchBase,
    BenchTop,
    Generator,
    Instrument,
    ServicePipe,
};

enum class CombatArchitecturePart : std::uint8_t {
    None,
    Cover,
    CoverFace,
    CoverFrame,
    CoverRib,
    CoverBolt,
    ColumnShaft,
    ColumnTrim,
    ColumnInset,
    ColumnBrace,
    GalleryDeck,
    GallerySupport,
    GalleryPillar,
    GalleryPillarTrim,
    GalleryBracket,
    GalleryFastener,
    GalleryRail,
    StairTread,
    StairRail,
    StairRailSlope,
    CeilingBeam,
    CeilingBrace,
    CeilingPanel,
    CeilingGrille,
};

// World-space authored geometry. Breakable parts are spawned as dynamic props;
// the remaining parts share their geometry with navigation collision.
struct RoomArchitecturePart {
    wkc::core::Aabb bounds{};
    wkc::core::Vec3 tint{1.0F, 1.0F, 1.0F};
    bool solid{false};
    bool breakable{false};
    bool storageRack{false};
    StorageRackPart storageRackPart{StorageRackPart::None};
    CafeteriaFurniturePart cafeteriaFurniturePart{CafeteriaFurniturePart::None};
    bool structuralColumn{false};
    RoomDecorationPart decoration{RoomDecorationPart::None};
    SecurityFurniturePart securityFurniturePart{SecurityFurniturePart::None};
    LaboratoryFurniturePart laboratoryFurniturePart{LaboratoryFurniturePart::None};
    TechnicalFurniturePart technicalFurniturePart{TechnicalFurniturePart::None};
    CombatArchitecturePart combatPart{CombatArchitecturePart::None};
    // World-space centre line used to draw the inclined stair rail. Its
    // compact AABB remains the bullet/navigation collision volume.
    wkc::core::Vec3 railStart{};
    wkc::core::Vec3 railEnd{};

    constexpr bool operator==(const RoomArchitecturePart&) const noexcept = default;
};

// Returns authored architecture for the room's current theme. Empty means the
// renderer/navigation should keep their existing generic room composition.
[[nodiscard]] bool usesAuthoredRoomArchitecture(
    const RoomInstance& room) noexcept;

[[nodiscard]] std::vector<RoomArchitecturePart> makeRoomArchitecture(
    const RoomInstance& room);

// Full-height, wall-attached structural columns. Kept separate from authored
// room furniture so callers can render and collide with the same added parts.
[[nodiscard]] std::vector<RoomArchitecturePart> makeRoomWallColumns(
    const RoomInstance& room);

// Rejects authored geometry that would obstruct a connector or reserved
// enemy, loot or prop placement area. The game checks this before a run starts.
[[nodiscard]] bool validateRoomArchitecture(
    const RoomInstance& room,
    std::span<const RoomArchitecturePart> parts) noexcept;

} // namespace gdp::world
