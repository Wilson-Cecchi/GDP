#pragma once

#include "game/visual/EnemyGenerator.hpp"
#include "game/visual/GdpRenderData.hpp"
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

class GdpRenderer {
public:
    GdpRenderer() = default;
    ~GdpRenderer() = default;

    GdpRenderer(const GdpRenderer&) = delete;
    GdpRenderer& operator=(const GdpRenderer&) = delete;
    GdpRenderer(GdpRenderer&&) = delete;
    GdpRenderer& operator=(GdpRenderer&&) = delete;

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
        gdp::game::visual::CameraData camera = {},
        std::span<const gdp::game::visual::EnemyRenderData> enemies = {},
        std::span<const gdp::game::visual::PickupRenderData> pickups = {},
        std::span<const gdp::game::visual::PropRenderData> props = {},
        gdp::game::visual::RoomLightingRenderData lighting = {},
        gdp::game::visual::RoomRenderData room = {},
        std::span<const gdp::game::visual::DoorRenderData> doors = {},
        std::span<const gdp::game::visual::CorridorRenderData> corridors = {},
        std::span<const gdp::game::visual::RoomRenderData> connectedRooms = {},
        gdp::game::visual::HudRenderData hud = {});

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
