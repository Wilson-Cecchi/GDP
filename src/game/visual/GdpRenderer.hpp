#pragma once

#include "game/visual/EnemyGenerator.hpp"
#include "game/visual/RoomMaterialGenerator.hpp"
#include "procedural/AssetCache.hpp"
#include "render/Camera.hpp"
#include "render/Framebuffer.hpp"
#include "render/GlApi.hpp"
#include "render/Mesh.hpp"
#include "render/Shader.hpp"
#include "render/Texture.hpp"

#include <span>
#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace gdp::render {

struct CameraData {
    gdp::core::Vec3 position{0.0F, 1.62F, 4.0F};
    gdp::core::Vec3 forward{0.0F, 0.0F, -1.0F};
    float verticalFieldOfViewDegrees{68.0F};
};

enum class EnemyVisualRole : std::uint8_t {
    Worker,
    Guard,
    Officer,
    Engineer,
    IndustrialRobot,
};

enum class PickupVisualRole : std::uint8_t {
    Health,
    Ammo,
    Key,
};

enum class PropVisualRole : std::uint8_t {
    Crate,
    Canister,
    Barricade,
};

enum class PropVisualState : std::uint8_t {
    Intact,
    Damaged,
    Destroyed,
};

struct EnemyRenderData {
    gdp::core::Vec3 position{};
    bool alive{true};
    gdp::core::Seed visualSeed{0};
    EnemyVisualRole role{EnemyVisualRole::Worker};
    bool active{true};
};

struct PickupRenderData {
    gdp::core::Vec3 position{};
    PickupVisualRole role{PickupVisualRole::Health};
};

struct PropRenderData {
    gdp::core::Vec3 position{};
    gdp::core::Vec3 scale{1.0F, 1.0F, 1.0F};
    PropVisualRole role{PropVisualRole::Crate};
    PropVisualState state{PropVisualState::Intact};
    gdp::core::Seed visualSeed{0};
};

struct RoomRenderData {
    gdp::core::Vec3 origin{};
    gdp::core::QuarterTurn rotation{gdp::core::QuarterTurn::Degrees0};
    std::uint64_t materialKey{0};
    gdp::core::Aabb worldBounds{
        {-3.85F, 0.0F, -4.85F},
        {3.85F, 4.0F, 4.85F},
    };
};

struct DoorRenderData {
    gdp::core::Vec3 position{};
    gdp::core::Vec3 outwardNormal{0.0F, 0.0F, 1.0F};
    float width{1.2F};
    float height{2.2F};
    bool locked{false};
    float openFraction{1.0F};
};

struct CorridorRenderData {
    std::span<const gdp::core::Vec3> points{};
    float width{1.2F};
    float height{2.2F};
    std::uint64_t materialKey{0};
};

enum class ObjectiveRenderState : std::uint8_t {
    None,
    ReachBoss,
    DefeatBoss,
    Victory,
    Defeat,
};

enum class HudEventState : std::uint8_t {
    None,
    Dormant,
    Active,
    Completed,
};

enum class HudPrompt : std::uint8_t {
    None,
    OpenDoor,
    DoorLocked,
    UseTerminal,
    Reload,
};

enum class HudHitFeedback : std::uint8_t {
    None,
    Hit,
    Kill,
};

enum class HudNotice : std::uint8_t {
    None,
    Pickup,
    EventStart,
    Wave,
    EventClear,
    LevelUp,
};

enum class HudPauseSelection : std::uint8_t {
    Resume,
    RestartRun,
    ExitGame,
};

enum class HudMainMenuSelection : std::uint8_t {
    StartRun,
    ExitGame,
};

struct UiRect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    [[nodiscard]] bool contains(int pointX, int pointY) const noexcept;
};

struct MainMenuLayout {
    int scale{1};
    UiRect panel{};
    UiRect startButton{};
    UiRect exitButton{};
};

[[nodiscard]] MainMenuLayout makeMainMenuLayout(int width, int height) noexcept;
[[nodiscard]] std::optional<HudMainMenuSelection> mainMenuSelectionAt(
    int width,
    int height,
    float pointerX,
    float pointerY) noexcept;

struct HudRenderData {
    ObjectiveRenderState objective{ObjectiveRenderState::None};
    bool visible{false};
    float health{100.0F};
    float maximumHealth{100.0F};
    std::uint32_t magazineAmmo{0};
    std::uint32_t reserveAmmo{0};
    std::uint32_t keys{0};
    std::uint32_t level{1};
    std::uint32_t experience{0};
    std::uint32_t experienceForNextLevel{0};
    HudEventState event{HudEventState::None};
    bool eventLocked{false};
    HudPrompt prompt{HudPrompt::None};
    float damageFeedback{0.0F};
    HudHitFeedback hitFeedback{HudHitFeedback::None};
    float hitFeedbackStrength{0.0F};
    HudNotice notice{HudNotice::None};
    float noticeFeedbackStrength{0.0F};
    bool paused{false};
    HudPauseSelection pauseSelection{HudPauseSelection::Resume};
    bool mainMenu{false};
    HudMainMenuSelection mainMenuSelection{HudMainMenuSelection::StartRun};
};

inline constexpr std::size_t maxRenderedRoomLights = 8U;

struct PointLightRenderData {
    gdp::core::Vec3 position{};
    gdp::core::Vec3 color{1.0F, 1.0F, 1.0F};
    float intensity{1.0F};
    float range{6.0F};
};

struct RoomLightingRenderData {
    gdp::core::Vec3 ambientColor{0.70F, 0.78F, 0.88F};
    float ambientIntensity{0.24F};
    std::array<PointLightRenderData, maxRenderedRoomLights> lights{
        PointLightRenderData{{0.0F, 3.4F, 1.0F}, {0.78F, 0.88F, 1.0F}, 1.0F, 6.0F},
    };
    std::uint32_t lightCount{1U};
    float fixtureEmissive{0.75F};
    bool alert{false};
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    [[nodiscard]] bool initialize(int width, int height);
    [[nodiscard]] bool prepareIndustrialRoomMaterials(gdp::core::Seed roomSeed);
    [[nodiscard]] bool prepareStorageRoomMaterials(gdp::core::Seed roomSeed);
    [[nodiscard]] bool prepareCombatRoomMaterials(gdp::core::Seed roomSeed);
    [[nodiscard]] bool prepareCafeteriaRoomMaterials(gdp::core::Seed roomSeed);
    [[nodiscard]] bool prepareLaboratoryRoomMaterials(gdp::core::Seed roomSeed);
    [[nodiscard]] bool prepareTechnicalRoomMaterials(gdp::core::Seed roomSeed);
    [[nodiscard]] bool prepareSecurityRoomMaterials(gdp::core::Seed roomSeed);
    [[nodiscard]] bool prepareCentralHubRoomMaterials(gdp::core::Seed roomSeed);
    [[nodiscard]] std::uint64_t activeRoomMaterialKey() const noexcept
    {
        return activeRoomMaterialKey_;
    }
    [[nodiscard]] bool prepareWorkerVisual(gdp::core::Seed visualSeed);
    [[nodiscard]] bool prepareGuardVisual(gdp::core::Seed visualSeed);
    [[nodiscard]] bool prepareOfficerVisual(gdp::core::Seed visualSeed);
    [[nodiscard]] bool prepareEngineerVisual(gdp::core::Seed visualSeed);
    [[nodiscard]] bool prepareIndustrialRobotVisual(gdp::core::Seed visualSeed);
    [[nodiscard]] bool render(
        int width,
        int height,
        CameraData camera = {},
        std::span<const EnemyRenderData> enemies = {},
        std::span<const PickupRenderData> pickups = {},
        std::span<const PropRenderData> props = {},
        RoomLightingRenderData lighting = {},
        RoomRenderData room = {},
        std::span<const DoorRenderData> doors = {},
        std::span<const CorridorRenderData> corridors = {},
        std::span<const RoomRenderData> connectedRooms = {},
        HudRenderData hud = {});

private:
    struct EnemyGpuVisual {
        Mesh mesh;
        Texture texture;
        float emissiveStrength{0.0F};
    };

    struct IndustrialWallGpuMaterial {
        Texture albedo;
        Texture normal;
        Texture properties;
    };

    struct CeilingFixtureGpuMaterial {
        Texture albedo;
        Texture normal;
        Texture properties;
        Texture emission;
    };

    struct RoomGpuMaterials {
        std::array<Texture,
            static_cast<std::size_t>(gdp::procedural::IndustrialSurface::Count)> textures;
        std::optional<gdp::procedural::IndustrialCondition> industrialWallCondition;
        std::optional<gdp::procedural::IndustrialCondition> industrialCeilingCondition;
        gdp::core::Vec2 wallUvOffset{};
        gdp::core::Vec2 ceilingUvOffset{};
        float wallTone{1.0F};
        float ceilingTone{1.0F};
    };

    GlApi gl_{};
    Camera camera_{};
    Framebuffer framebuffer_{};
    Shader shader_{};
    Mesh box_{};
    Texture fallbackTexture_{};
    CeilingFixtureGpuMaterial ceilingFixtureMaterial_{};
    gdp::procedural::AssetCache<MeshData> meshCache_{};
    gdp::procedural::AssetCache<ImageData> textureCache_{};
    std::unordered_map<gdp::core::Seed, EnemyGpuVisual> workerVisuals_{};
    std::unordered_map<gdp::core::Seed, EnemyGpuVisual> guardVisuals_{};
    std::unordered_map<gdp::core::Seed, EnemyGpuVisual> officerVisuals_{};
    std::unordered_map<gdp::core::Seed, EnemyGpuVisual> engineerVisuals_{};
    std::unordered_map<gdp::core::Seed, EnemyGpuVisual> industrialRobotVisuals_{};
    std::array<std::optional<IndustrialWallGpuMaterial>,
        static_cast<std::size_t>(gdp::procedural::IndustrialCondition::Count)>
        industrialWallMaterials_{};
    std::array<std::optional<IndustrialWallGpuMaterial>,
        static_cast<std::size_t>(gdp::procedural::IndustrialCondition::Count)>
        industrialCeilingMaterials_{};
    std::unordered_map<std::uint64_t, RoomGpuMaterials> roomMaterials_{};
    std::uint64_t activeRoomMaterialKey_{0};
    std::uint64_t fallbackRoomMaterialKey_{0};
    bool initialized_{false};
};

} // namespace gdp::render
