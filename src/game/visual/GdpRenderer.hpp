#pragma once

#include "game/visual/EnemyGenerator.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "game/visual/RoomMaterialGenerator.hpp"
#include "resources/AssetCache.hpp"
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

namespace gdp::game::visual {

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
        gdp::render::Mesh mesh;
        gdp::render::Texture texture;
        float emissiveStrength{0.0F};
    };

    struct IndustrialWallGpuMaterial {
        gdp::render::Texture albedo;
        gdp::render::Texture normal;
        gdp::render::Texture properties;
    };

    struct CeilingFixtureGpuMaterial {
        gdp::render::Texture albedo;
        gdp::render::Texture normal;
        gdp::render::Texture properties;
        gdp::render::Texture emission;
    };

    struct RoomGpuMaterials {
        std::array<gdp::render::Texture,
            static_cast<std::size_t>(gdp::procedural::IndustrialSurface::Count)> textures;
        std::optional<gdp::procedural::IndustrialCondition> industrialWallCondition;
        std::optional<gdp::procedural::IndustrialCondition> industrialCeilingCondition;
        gdp::core::Vec2 wallUvOffset{};
        gdp::core::Vec2 ceilingUvOffset{};
        float wallTone{1.0F};
        float ceilingTone{1.0F};
    };

    gdp::render::GlApi gl_{};
    gdp::render::Camera camera_{};
    gdp::render::Framebuffer framebuffer_{};
    gdp::render::Shader shader_{};
    gdp::render::Mesh box_{};
    gdp::render::Texture fallbackTexture_{};
    CeilingFixtureGpuMaterial ceilingFixtureMaterial_{};
    wkc::resources::AssetCache<gdp::render::MeshData> meshCache_{};
    wkc::resources::AssetCache<gdp::render::ImageData> textureCache_{};
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

} // namespace gdp::game::visual
