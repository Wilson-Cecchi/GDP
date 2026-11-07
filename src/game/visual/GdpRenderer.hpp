#pragma once

#include "game/visual/EnemyGenerator.hpp"
#include "game/visual/EnemyModelAsset.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "game/visual/RoomMaterialGenerator.hpp"
#include "game/visual/WeaponViewmodelAssets.hpp"
#include "game/visual/PlayerHandsAssets.hpp"
#include "game/visual/PlayerBodyAssets.hpp"
#include "game/visual/PlayerRigAsset.hpp"
#include "resources/AssetCache.hpp"
#include "render/Camera.hpp"
#include "render/Framebuffer.hpp"
#include "render/GlApi.hpp"
#include "render/GpuTimer.hpp"
#include "render/Mesh.hpp"
#include "render/RasterState.hpp"
#include "render/Shader.hpp"
#include "render/ShadowCubeMap.hpp"
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
    std::uint32_t geometryShadowMaps{0U};
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
    [[nodiscard]] bool prepareStorageRoomMaterials(
        wkc::core::Seed roomSeed, bool approvedHallMaterials = false);
    [[nodiscard]] bool prepareCombatRoomMaterials(wkc::core::Seed roomSeed);
    [[nodiscard]] bool prepareCafeteriaRoomMaterials(wkc::core::Seed roomSeed);
    [[nodiscard]] bool prepareLaboratoryRoomMaterials(
        wkc::core::Seed roomSeed, bool approvedPreviewMaterials = false);
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
    void setGeometryShadowsEnabled(const bool enabled) noexcept
    {
        geometryShadowsEnabled_ = enabled;
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
        std::span<const EnemyProjectileRenderData> enemyProjectiles = {},
        PlayerBodyRenderData playerBody = {},
        std::span<const GrenadeRenderData> grenades = {},
        std::span<const GrenadeBlastRenderData> grenadeBlasts = {});
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
        wkc::core::Vec3 tint{1.0F, 1.0F, 1.0F};
        bool workerPaint{false};
        bool workerFrontPanel{false};
        bool workerEyeCore{false};
        bool workerEyeHalo{false};
        bool workerSawBlade{false};
        wkc::core::Vec3 workerSawCenter{};
        std::uint8_t referenceMaterialId{255U};
        std::uint32_t index{0};
    };

    struct EnemyGpuVisual {
        std::vector<EnemyGpuPart> parts;
        wkc::render::Texture texture;
        float emissiveStrength{0.0F};
        float wearAmount{0.0F};
        std::uint32_t basePartCount{0U};
    };

    [[nodiscard]] bool uploadEnemyGpuVisual(
        const wkc::procedural::MeshRecipe& meshRecipe,
        const wkc::procedural::TextureRecipe& textureRecipe,
        EnemyGpuVisual& gpuVisual);
    [[nodiscard]] bool uploadEnemyReferenceVisual(
        const EnemyModelAsset& asset, std::string_view roleName,
        EnemyGpuVisual& gpuVisual);
    [[nodiscard]] bool tryPrepareEnemyReferenceVisual(
        std::string_view roleName, std::size_t roleIndex,
        std::unordered_map<wkc::core::Seed, EnemyGpuVisual>& visuals);

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

    struct CeilingFixtureGpuPart {
        wkc::render::Mesh mesh;
        std::uint32_t materialId{0U};
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
        bool storageHallV1{false};
        std::uint64_t storageHallSeed{0};
        std::optional<StorageCondition> storageRackCondition{};
        bool cafeteriaV1{false};
        std::uint8_t cafeteriaCondition{0};
        bool laboratoryPreviewV1{false};
        std::optional<CentralHubCondition> centralHubWallCondition{};
        bool securityV1{false};
        float securityFloorSeed{0.0F};
        float securityFloorTone{1.0F};
        std::uint8_t columnCondition{0};
    };

    wkc::render::GlApi gl_{};
    wkc::render::GpuTimer gpuTimer_{};
    wkc::render::Camera camera_{};
    wkc::render::Framebuffer framebuffer_{};
    wkc::render::Shader shader_{};
    wkc::render::Shader shadow3Shader_{};
    wkc::render::Shader shadow6Shader_{};
    wkc::render::Shader lowSpecShader_{};
    wkc::render::Shader shadowDepthShader_{};
    std::array<wkc::render::ShadowCubeMap, maxRenderedRoomLights> shadowCubes_{};
    bool geometryShadowsEnabled_{false};
    std::uint32_t shadowCubeCount_{0U};
    std::array<wkc::core::Vec3, maxRenderedRoomLights> shadowCubeLightPositions_{};
    wkc::render::Shader uiShader_{};
    wkc::render::Mesh uiQuad_{};
    wkc::render::Texture uiFont_{};
    wkc::render::Mesh box_{};
    wkc::render::Mesh centralHubColumnShaft_{};
    wkc::render::Mesh planterLeafMesh_{};
    wkc::render::Mesh glassTestDome_{};
    // Static world-space shell meshes for shaped preview rooms.
    std::unordered_map<std::uint64_t, wkc::render::Mesh> footprintMeshes_{};
    wkc::render::Mesh grenadeBody_{};
    wkc::render::Mesh grenadeArmor_{};
    wkc::render::Mesh grenadeLights_{};
    wkc::render::Mesh grenadeRing_{};
    struct GrenadeGpuPart {
        std::uint32_t partId{0U};
        std::uint32_t materialId{0U};
        wkc::render::Mesh mesh{};
    };
    std::vector<GrenadeGpuPart> grenadeModelParts_{};
    wkc::render::Mesh smokePuff_{};
    std::array<std::array<wkc::render::Mesh, weaponMaterialCount>,
        weaponRoleCount> weaponMeshes_{};
    std::array<std::array<std::array<wkc::render::Mesh, weaponMaterialCount>,
        weaponPartCount>, weaponRoleCount> weaponPartMeshes_{};
    std::array<WeaponGpuMaterial, weaponMaterialCount> weaponMaterials_{};
    std::array<wkc::render::Mesh, playerBodyMaterialCount> playerBodyMeshes_{};
    std::array<WeaponGpuMaterial, playerBodyMaterialCount> playerBodyMaterials_{};
    bool playerRigPreview_{false};
    std::optional<PlayerRigAsset> playerRigAsset_{};
    std::array<wkc::render::Mesh, playerExosuitMeshCount> playerRigMeshes_{};
    std::array<wkc::render::Texture, playerExosuitMaterialCount> playerRigAlbedo_{};
    std::array<wkc::render::Mesh, playerHandsMaterialCount> playerHandsMeshes_{};
    std::array<wkc::render::Mesh, playerHandsMaterialCount> playerSingleHandMeshes_{};
    std::array<WeaponGpuMaterial, playerHandsMaterialCount> playerHandsMaterials_{};
    wkc::render::MeshBindingCache meshBindings_{};
    wkc::render::BackfaceCullingStateCache backfaceCulling_{};
    wkc::render::Texture fallbackTexture_{};
    wkc::render::Texture solidColorTexture_{};
    wkc::render::Texture workerPaintTexture_{};
    wkc::render::Texture workerFrontPanelTexture_{};
    wkc::render::Texture referenceWhiteTexture_{};
    wkc::render::Texture referenceNeutralPaintTexture_{};
    std::array<wkc::render::Texture, 5U> referencePaintTextures_{};
    std::array<bool, 5U> referencePaintReady_{};
    std::array<bool, 5U> referenceModelReady_{};
    wkc::render::TextureBindingCache textureBindings_{};
    CeilingFixtureGpuMaterial ceilingFixtureMaterial_{};
    std::array<std::vector<CeilingFixtureGpuPart>,
        static_cast<std::size_t>(CeilingFixtureVariant::Count)>
        ceilingFixtureModels_{};
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
    std::array<wkc::render::Texture, 6> storageWallTextures_{};
    std::array<wkc::render::Texture, 3> storageFloorTextures_{};
    std::array<wkc::render::Texture, 3> storageCeilingTextures_{};
    std::array<wkc::render::Texture, 3> storageRackUprightTextures_{};
    std::array<wkc::render::Texture, 3> storageRackShelfTextures_{};
    std::array<wkc::render::Texture, 3> cafeteriaWallTextures_{};
    std::array<wkc::render::Texture, 3> cafeteriaFloorTextures_{};
    std::array<wkc::render::Texture, 3> cafeteriaCeilingTextures_{};
    std::array<std::array<wkc::render::Texture, 3>, 4> cafeteriaFurnitureTextures_{};
    wkc::render::Texture laboratoryFurnitureTexture_{};
    wkc::render::Texture securityFurnitureTexture_{};
    std::array<wkc::render::Texture, 3> structuralColumnTextures_{};
    wkc::render::Texture centralHubColumnTexture_{};
    wkc::render::Texture centralHubFloorDetailTexture_{};
    std::array<wkc::render::Texture, 2> storageBoxTextures_{};
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
