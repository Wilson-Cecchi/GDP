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
};

struct EnemyRenderData {
    gdp::core::Vec3 position{};
    bool alive{true};
    gdp::core::Seed visualSeed{0};
    EnemyVisualRole role{EnemyVisualRole::Worker};
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
    [[nodiscard]] bool render(
        int width,
        int height,
        CameraData camera = {},
        std::span<const EnemyRenderData> enemies = {});

private:
    struct EnemyGpuVisual {
        Mesh mesh;
        Texture texture;
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
    std::unordered_map<std::uint64_t, RoomGpuMaterials> roomMaterials_{};
    std::uint64_t activeRoomMaterialKey_{0};
    std::uint64_t fallbackRoomMaterialKey_{0};
    bool initialized_{false};
};

} // namespace gdp::render
