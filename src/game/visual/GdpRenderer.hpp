#pragma once

#include "game/visual/EnemyGenerator.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "game/visual/RoomMaterialGenerator.hpp"
#include "game/visual/WeaponViewmodelAssets.hpp"
#include "resources/AssetCache.hpp"
#include "render/Camera.hpp"
#include "render/Framebuffer.hpp"
#include "render/GlApi.hpp"
#include "render/GpuTimer.hpp"
#include "render/Mesh.hpp"
#include "render/RasterState.hpp"
#include "render/Shader.hpp"
#include "render/Texture.hpp"

#include <span>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gdp::game::visual {

struct RendererFrameStats {
    std::uint64_t drawCalls{0U};
    std::uint64_t triangles{0U};
    std::size_t meshCacheEntries{0U};
    std::size_t textureCacheEntries{0U};
    std::uint64_t uniformUploads{0U};
    std::uint64_t uniformSkips{0U};
    std::uint64_t textureUnitActivations{0U};
    std::uint64_t textureBinds{0U};
    std::uint64_t textureBindSkips{0U};
    std::uint64_t meshBinds{0U};
    std::uint64_t meshBindSkips{0U};
    bool lowSpecShader{false};
    std::uint64_t backfaceCulledDraws{0U};
    std::uint64_t cullStateChanges{0U};
    std::uint64_t instancedDrawCalls{0U};
    std::uint64_t renderedInstances{0U};
    std::uint64_t avoidedDrawCalls{0U};
    std::uint32_t shadowShaderOccluderCapacity{0U};
    double gpuMilliseconds{0.0};
    bool gpuTimingAvailable{false};

    constexpr bool operator==(const RendererFrameStats&) const noexcept
        = default;
};

enum class RendererShaderPath : std::uint8_t {
    LowSpec,
    FullShadow3,
    FullShadow6,
    FullShadow8,
};

[[nodiscard]] constexpr RendererShaderPath selectRendererShaderPath(
    const bool detailedMaterials,
    const std::uint32_t lightCount,
    const std::uint32_t shadowLightCount,
    const std::uint32_t shadowOccluderCount) noexcept
{
    if (!detailedMaterials && lightCount <= 4U
        && shadowLightCount == 0U && shadowOccluderCount == 0U) {
        return RendererShaderPath::LowSpec;
    }
    if (shadowOccluderCount <= 3U) {
        return RendererShaderPath::FullShadow3;
    }
    if (shadowOccluderCount <= 6U) {
        return RendererShaderPath::FullShadow6;
    }
    return RendererShaderPath::FullShadow8;
}

[[nodiscard]] constexpr std::uint32_t shadowShaderOccluderCapacity(
    const RendererShaderPath path) noexcept
{
    switch (path) {
    case RendererShaderPath::LowSpec:
        return 0U;
    case RendererShaderPath::FullShadow3:
        return 3U;
    case RendererShaderPath::FullShadow6:
        return 6U;
    case RendererShaderPath::FullShadow8:
        return 8U;
    }
    return 8U;
}

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
    [[nodiscard]] std::string_view gpuVendor() const noexcept
    {
        return gpuVendor_;
    }
    [[nodiscard]] std::string_view gpuRenderer() const noexcept
    {
        return gpuRenderer_;
    }
    [[nodiscard]] std::string_view gpuVersion() const noexcept
    {
        return gpuVersion_;
    }
    void setSurfaceInstancingEnabled(const bool enabled) noexcept
    {
        surfaceInstancingEnabled_ = enabled;
    }
    void setShadowShaderSpecializationEnabled(const bool enabled) noexcept
    {
        shadowShaderSpecializationEnabled_ = enabled;
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
        HudRenderData hud = {},
        std::span<const ImpactRenderData> impacts = {},
        WeaponViewmodelRenderData weaponViewmodel = {},
        std::span<const EnemyProjectileRenderData> enemyProjectiles = {});
    [[nodiscard]] RendererFrameStats lastFrameStats() const noexcept
    {
        RendererFrameStats stats = lastFrameStats_;
        const wkc::render::ShaderUniformValueCacheStats uniformStats
            = activeShader().uniformValueCacheStats();
        stats.uniformUploads = uniformStats.uploads;
        stats.uniformSkips = uniformStats.skips;
        const wkc::render::TextureBindingCacheStats textureStats
            = textureBindings_.stats();
        stats.textureUnitActivations = textureStats.unitActivations;
        stats.textureBinds = textureStats.textureBinds;
        stats.textureBindSkips = textureStats.skippedTextureBinds;
        const wkc::render::MeshBindingCacheStats meshStats
            = meshBindings_.stats();
        stats.meshBinds = meshStats.vertexArrayBinds;
        stats.meshBindSkips = meshStats.skippedVertexArrayBinds;
        const wkc::render::BackfaceCullingStateStats cullingStats
            = backfaceCulling_.stats();
        stats.cullStateChanges = cullingStats.stateChanges;
        return stats;
    }

private:
    void drawMesh(const wkc::render::Mesh& mesh) noexcept;
    void drawMeshInstanced(const wkc::render::Mesh& mesh) noexcept;
    [[nodiscard]] wkc::render::Shader& activeShader() noexcept
    {
        switch (activeShaderPath_) {
        case RendererShaderPath::LowSpec:
            return lowSpecShader_;
        case RendererShaderPath::FullShadow3:
            return shadow3Shader_;
        case RendererShaderPath::FullShadow6:
            return shadow6Shader_;
        case RendererShaderPath::FullShadow8:
            return shader_;
        }
        return shader_;
    }
    [[nodiscard]] const wkc::render::Shader& activeShader() const noexcept
    {
        switch (activeShaderPath_) {
        case RendererShaderPath::LowSpec:
            return lowSpecShader_;
        case RendererShaderPath::FullShadow3:
            return shadow3Shader_;
        case RendererShaderPath::FullShadow6:
            return shadow6Shader_;
        case RendererShaderPath::FullShadow8:
            return shader_;
        }
        return shader_;
    }

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

    struct WeaponGpuMaterial {
        wkc::render::Texture albedo;
        wkc::render::Texture normal;
        wkc::render::Texture properties;
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
    wkc::render::GpuTimer gpuTimer_{};
    wkc::render::Camera camera_{};
    wkc::render::Framebuffer framebuffer_{};
    wkc::render::Shader shader_{};
    wkc::render::Shader shadow3Shader_{};
    wkc::render::Shader shadow6Shader_{};
    wkc::render::Shader lowSpecShader_{};
    wkc::render::Shader uiShader_{};
    wkc::render::Mesh uiQuad_{};
    wkc::render::Texture uiFont_{};
    wkc::render::Mesh box_{};
    std::array<std::array<wkc::render::Mesh, weaponMaterialCount>,
        weaponRoleCount> weaponMeshes_{};
    std::array<std::array<std::array<wkc::render::Mesh, weaponMaterialCount>,
        weaponPartCount>, weaponRoleCount> weaponPartMeshes_{};
    std::array<WeaponGpuMaterial, weaponMaterialCount> weaponMaterials_{};
    wkc::render::MeshBindingCache meshBindings_{};
    wkc::render::BackfaceCullingStateCache backfaceCulling_{};
    wkc::render::Texture fallbackTexture_{};
    wkc::render::TextureBindingCache textureBindings_{};
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
    std::string gpuVendor_{"unknown"};
    std::string gpuRenderer_{"unknown"};
    std::string gpuVersion_{"unknown"};
    RendererFrameStats lastFrameStats_{};
    RendererShaderPath activeShaderPath_{RendererShaderPath::FullShadow8};
    bool lowSpecShaderActive_{false};
    bool surfaceInstancingEnabled_{true};
    bool shadowShaderSpecializationEnabled_{true};
    bool initialized_{false};
};

} // namespace gdp::game::visual
