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
#include <vector>

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
    [[nodiscard]] bool prepareIndustrialRoomMaterials(wkc::core::Seed roomSeed);
    [[nodiscard]] bool prepareStorageRoomMaterials(wkc::core::Seed roomSeed);
    [[nodiscard]] bool prepareCombatRoomMaterials(wkc::core::Seed roomSeed);
    [[nodiscard]] bool prepareCafeteriaRoomMaterials(wkc::core::Seed roomSeed);
    [[nodiscard]] bool prepareLaboratoryRoomMaterials(wkc::core::Seed roomSeed);
    [[nodiscard]] bool prepareTechnicalRoomMaterials(wkc::core::Seed roomSeed);
    [[nodiscard]] bool prepareSecurityRoomMaterials(wkc::core::Seed roomSeed);
    [[nodiscard]] bool prepareCentralHubRoomMaterials(wkc::core::Seed roomSeed);
    [[nodiscard]] std::uint64_t activeRoomMaterialKey() const noexcept
    {
        return activeRoomMaterialKey_;
    }
    [[nodiscard]] bool prepareWorkerVisual(wkc::core::Seed visualSeed);
    [[nodiscard]] bool prepareGuardVisual(wkc::core::Seed visualSeed);
    [[nodiscard]] bool prepareOfficerVisual(wkc::core::Seed visualSeed);
    [[nodiscard]] bool prepareEngineerVisual(wkc::core::Seed visualSeed);
    [[nodiscard]] bool prepareIndustrialRobotVisual(wkc::core::Seed visualSeed);
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
    struct EnemyGpuPart {
        wkc::render::Mesh mesh;
        wkc::core::Vec3 pivot{};
        std::uint32_t index{0};
    };

    struct EnemyGpuVisual {
        std::vector<EnemyGpuPart> parts;
        wkc::render::Texture texture;
        float emissiveStrength{0.0F};
    };

    [[nodiscard]] bool uploadEnemyGpuVisual(
        const wkc::procedural::MeshRecipe& meshRecipe,
        const wkc::procedural::TextureRecipe& textureRecipe,
        EnemyGpuVisual& gpuVisual);

    struct IndustrialWallGpuMaterial {
        wkc::render::Texture albedo;
        wkc::render::Texture normal;
        wkc::render::Texture properties;
    };

    struct CeilingFixtureGpuMaterial {
        wkc::render::Texture albedo;
        wkc::render::Texture normal;
        wkc::render::Texture properties;
        wkc::render::Texture emission;
    };

    struct RoomGpuMaterials {
        std::array<wkc::render::Texture,
            static_cast<std::size_t>(IndustrialSurface::Count)> textures;
        std::optional<IndustrialCondition> industrialWallCondition;
        std::optional<IndustrialCondition> industrialCeilingCondition;
        wkc::core::Vec2 wallUvOffset{};
        wkc::core::Vec2 ceilingUvOffset{};
        float wallTone{1.0F};
        float ceilingTone{1.0F};
    };

    wkc::render::GlApi gl_{};
    wkc::render::Camera camera_{};
    wkc::render::Framebuffer framebuffer_{};
    wkc::render::Shader shader_{};
    wkc::render::Mesh box_{};
    wkc::render::Texture fallbackTexture_{};
    CeilingFixtureGpuMaterial ceilingFixtureMaterial_{};
    wkc::resources::AssetCache<wkc::render::MeshData> meshCache_{};
    wkc::resources::AssetCache<wkc::render::ImageData> textureCache_{};
    std::unordered_map<wkc::core::Seed, EnemyGpuVisual> workerVisuals_{};
    std::unordered_map<wkc::core::Seed, EnemyGpuVisual> guardVisuals_{};
    std::unordered_map<wkc::core::Seed, EnemyGpuVisual> officerVisuals_{};
    std::unordered_map<wkc::core::Seed, EnemyGpuVisual> engineerVisuals_{};
    std::unordered_map<wkc::core::Seed, EnemyGpuVisual> industrialRobotVisuals_{};
    std::array<std::optional<IndustrialWallGpuMaterial>,
        static_cast<std::size_t>(IndustrialCondition::Count)>
        industrialWallMaterials_{};
    std::array<std::optional<IndustrialWallGpuMaterial>,
        static_cast<std::size_t>(IndustrialCondition::Count)>
        industrialCeilingMaterials_{};
    std::unordered_map<std::uint64_t, RoomGpuMaterials> roomMaterials_{};
    std::uint64_t activeRoomMaterialKey_{0};
    std::uint64_t fallbackRoomMaterialKey_{0};
    bool initialized_{false};
};

} // namespace gdp::game::visual
