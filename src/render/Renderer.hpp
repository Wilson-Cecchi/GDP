#pragma once

#include "procedural/AssetCache.hpp"
#include "procedural/EnemyGenerator.hpp"
#include "procedural/RoomMaterialGenerator.hpp"
#include "render/Camera.hpp"
#include "render/Framebuffer.hpp"
#include "render/GlApi.hpp"
#include "render/Mesh.hpp"
#include "render/Shader.hpp"
#include "render/Texture.hpp"

#include <span>
#include <array>
#include <cstdint>
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

inline constexpr std::size_t maxRenderedRoomLights = 4U;

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
        RoomLightingRenderData lighting = {});

private:
    struct EnemyGpuVisual {
        Mesh mesh;
        Texture texture;
        float emissiveStrength{0.0F};
    };

    struct RoomGpuMaterials {
        std::array<Texture,
            static_cast<std::size_t>(gdp::procedural::IndustrialSurface::Count)> textures;
        float ceilingEmissive{0.0F};
    };

    GlApi gl_{};
    Camera camera_{};
    Framebuffer framebuffer_{};
    Shader shader_{};
    Mesh box_{};
    Texture fallbackTexture_{};
    gdp::procedural::AssetCache<MeshData> meshCache_{};
    gdp::procedural::AssetCache<ImageData> textureCache_{};
    std::unordered_map<gdp::core::Seed, EnemyGpuVisual> workerVisuals_{};
    std::unordered_map<gdp::core::Seed, EnemyGpuVisual> guardVisuals_{};
    std::unordered_map<gdp::core::Seed, EnemyGpuVisual> officerVisuals_{};
    std::unordered_map<gdp::core::Seed, EnemyGpuVisual> engineerVisuals_{};
    std::unordered_map<gdp::core::Seed, EnemyGpuVisual> industrialRobotVisuals_{};
    std::unordered_map<std::uint64_t, RoomGpuMaterials> roomMaterials_{};
    std::uint64_t activeRoomMaterialKey_{0};
    std::uint64_t fallbackRoomMaterialKey_{0};
    bool initialized_{false};
};

} // namespace gdp::render
