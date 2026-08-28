#include "render/Renderer.hpp"

#include "core/Log.hpp"
#include "core/Math.hpp"
#include "procedural/EnemyGenerator.hpp"
#include "procedural/MeshGenerator.hpp"
#include "procedural/RoomMaterialGenerator.hpp"
#include "procedural/TextureGenerator.hpp"

#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <string_view>
#include <utility>

namespace gdp::render {
namespace {

constexpr std::string_view vertexShaderSource = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vUv;

void main()
{
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vNormal = normalize(mat3(uModel) * aNormal);
    vUv = aUv;
    gl_Position = uProjection * uView * worldPosition;
}
)glsl";

constexpr std::string_view fragmentShaderSource = R"glsl(#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUv;

uniform sampler2D uAlbedo;
uniform vec3 uTint;
const int MAX_LIGHTS = 4;
uniform int uLightCount;
uniform vec3 uLightPositions[MAX_LIGHTS];
uniform vec3 uLightColors[MAX_LIGHTS];
uniform float uLightIntensities[MAX_LIGHTS];
uniform float uLightRanges[MAX_LIGHTS];
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;
uniform float uUvScale;
uniform float uEmissiveStrength;

out vec4 fragmentColor;

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 albedo = texture(uAlbedo, vUv * uUvScale).rgb * uTint;
    vec3 light = uAmbientColor * uAmbientIntensity;
    for (int index = 0; index < MAX_LIGHTS; ++index) {
        if (index >= uLightCount) {
            break;
        }
        vec3 toLight = uLightPositions[index] - vWorldPosition;
        float distanceToLight = length(toLight);
        float diffuse = max(dot(normal, normalize(toLight)), 0.0);
        float normalizedDistance = distanceToLight / max(uLightRanges[index], 0.001);
        float attenuation = 1.0 / (1.0 + 2.2 * normalizedDistance * normalizedDistance);
        light += uLightColors[index] * diffuse * attenuation
            * uLightIntensities[index] * 1.9;
    }
    float emissiveMask = smoothstep(0.62, 0.82, max(albedo.r, max(albedo.g, albedo.b)));
    fragmentColor = vec4((albedo * light) + (albedo * emissiveMask * uEmissiveStrength), 1.0);
}
)glsl";

enum class RoomMaterialChannel : std::uint8_t {
    Wall,
    Floor,
    Ceiling,
    Generic,
};

struct RoomPart {
    gdp::core::Vec3 position;
    gdp::core::Vec3 scale;
    gdp::core::Vec3 tint;
    RoomMaterialChannel material;
    float uvScale;
};

constexpr std::array roomParts{
    RoomPart{{0.0F, -0.1F, 0.0F}, {8.0F, 0.2F, 10.0F}, {1.0F, 1.0F, 1.0F},
        RoomMaterialChannel::Floor, 4.0F},
    RoomPart{{0.0F, 4.1F, 0.0F}, {8.0F, 0.2F, 10.0F}, {1.0F, 1.0F, 1.0F},
        RoomMaterialChannel::Ceiling, 4.0F},
    RoomPart{{0.0F, 2.0F, -5.0F}, {8.0F, 4.0F, 0.2F}, {1.0F, 1.0F, 1.0F},
        RoomMaterialChannel::Wall, 1.0F},
    RoomPart{{-4.0F, 2.0F, 0.0F}, {0.2F, 4.0F, 10.0F}, {1.0F, 1.0F, 1.0F},
        RoomMaterialChannel::Wall, 1.0F},
    RoomPart{{4.0F, 2.0F, 0.0F}, {0.2F, 4.0F, 10.0F}, {1.0F, 1.0F, 1.0F},
        RoomMaterialChannel::Wall, 1.0F},
    RoomPart{{-2.8F, 1.0F, -3.8F}, {0.45F, 2.0F, 0.45F}, {0.75F, 0.57F, 0.22F},
        RoomMaterialChannel::Generic, 4.0F},
    RoomPart{{2.8F, 1.0F, -3.8F}, {0.45F, 2.0F, 0.45F}, {0.75F, 0.57F, 0.22F},
        RoomMaterialChannel::Generic, 4.0F},
    RoomPart{{0.0F, 0.35F, -2.2F}, {2.2F, 0.7F, 1.2F}, {0.42F, 0.48F, 0.51F},
        RoomMaterialChannel::Generic, 4.0F},
    RoomPart{{0.0F, 1.0F, 2.2F}, {0.8F, 2.0F, 0.3F}, {0.25F, 0.72F, 0.68F},
        RoomMaterialChannel::Generic, 4.0F},
};

} // namespace

bool Renderer::initialize(const int width, const int height)
{
    if (!loadGlApi(gl_)) {
        return false;
    }
    if (!shader_.compile(gl_, vertexShaderSource, fragmentShaderSource)) {
        return false;
    }
    if (!box_.upload(gl_, makeBoxMesh())) {
        return false;
    }
    if (!fallbackTexture_.upload(gl_, makeIndustrialCheckerImage())) {
        return false;
    }
    if (!framebuffer_.resize(gl_, width, height)) {
        return false;
    }

    camera_.setPerspective(68.0F, 0.05F, 100.0F);
    camera_.lookAt({0.0F, 1.7F, 6.5F}, {0.0F, 1.5F, -1.5F});
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    initialized_ = true;
    if (!prepareIndustrialRoomMaterials(0) || !prepareWorkerVisual(0)
        || !prepareGuardVisual(0) || !prepareOfficerVisual(0)
        || !prepareEngineerVisual(0) || !prepareIndustrialRobotVisual(0)) {
        initialized_ = false;
        return false;
    }
    gdp::core::log(gdp::core::LogLevel::Info, "Renderer initialized with fixed-room scene");
    return true;
}

bool Renderer::prepareIndustrialRoomMaterials(const gdp::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const gdp::procedural::IndustrialRoomMaterialSet materials
        = gdp::procedural::generateIndustrialRoomMaterials(roomSeed);
    if (!gdp::procedural::validateIndustrialRoomMaterials(materials).isValid()) {
        gdp::core::log(
            gdp::core::LogLevel::Error, "Industrial room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const gdp::procedural::IndustrialSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            gdp::procedural::industrialSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(
                    *gdp::procedural::generateIndustrialSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    gpuMaterials.ceilingEmissive = materials.surfaces[static_cast<std::size_t>(
        gdp::procedural::IndustrialSurface::Ceiling)].emissiveAmount;
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    if (roomSeed == 0U) {
        fallbackRoomMaterialKey_ = materials.materialHash;
    }
    return true;
}

bool Renderer::prepareStorageRoomMaterials(const gdp::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const gdp::procedural::StorageRoomMaterialSet materials
        = gdp::procedural::generateStorageRoomMaterials(roomSeed);
    if (!gdp::procedural::validateStorageRoomMaterials(materials).isValid()) {
        gdp::core::log(
            gdp::core::LogLevel::Error, "Storage room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const gdp::procedural::StorageSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            gdp::procedural::storageSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*gdp::procedural::generateStorageSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    gpuMaterials.ceilingEmissive = materials.surfaces[static_cast<std::size_t>(
        gdp::procedural::RoomSurface::Ceiling)].emissiveAmount;
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool Renderer::prepareCombatRoomMaterials(const gdp::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const gdp::procedural::CombatRoomMaterialSet materials
        = gdp::procedural::generateCombatRoomMaterials(roomSeed);
    if (!gdp::procedural::validateCombatRoomMaterials(materials).isValid()) {
        gdp::core::log(
            gdp::core::LogLevel::Error, "Combat room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const gdp::procedural::CombatSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            gdp::procedural::combatSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*gdp::procedural::generateCombatSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    gpuMaterials.ceilingEmissive = materials.surfaces[static_cast<std::size_t>(
        gdp::procedural::RoomSurface::Ceiling)].emissiveAmount;
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool Renderer::prepareCafeteriaRoomMaterials(const gdp::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const gdp::procedural::CafeteriaRoomMaterialSet materials
        = gdp::procedural::generateCafeteriaRoomMaterials(roomSeed);
    if (!gdp::procedural::validateCafeteriaRoomMaterials(materials).isValid()) {
        gdp::core::log(
            gdp::core::LogLevel::Error, "Cafeteria room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const gdp::procedural::CafeteriaSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            gdp::procedural::cafeteriaSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*gdp::procedural::generateCafeteriaSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    gpuMaterials.ceilingEmissive = materials.surfaces[static_cast<std::size_t>(
        gdp::procedural::RoomSurface::Ceiling)].emissiveAmount;
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool Renderer::prepareLaboratoryRoomMaterials(const gdp::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const gdp::procedural::LaboratoryRoomMaterialSet materials
        = gdp::procedural::generateLaboratoryRoomMaterials(roomSeed);
    if (!gdp::procedural::validateLaboratoryRoomMaterials(materials).isValid()) {
        gdp::core::log(
            gdp::core::LogLevel::Error, "Laboratory room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const gdp::procedural::LaboratorySurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            gdp::procedural::laboratorySurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*gdp::procedural::generateLaboratorySurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    gpuMaterials.ceilingEmissive = materials.surfaces[static_cast<std::size_t>(
        gdp::procedural::RoomSurface::Ceiling)].emissiveAmount;
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool Renderer::prepareTechnicalRoomMaterials(const gdp::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const gdp::procedural::TechnicalRoomMaterialSet materials
        = gdp::procedural::generateTechnicalRoomMaterials(roomSeed);
    if (!gdp::procedural::validateTechnicalRoomMaterials(materials).isValid()) {
        gdp::core::log(
            gdp::core::LogLevel::Error, "Technical room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const gdp::procedural::TechnicalSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            gdp::procedural::technicalSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*gdp::procedural::generateTechnicalSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    gpuMaterials.ceilingEmissive = materials.surfaces[static_cast<std::size_t>(
        gdp::procedural::RoomSurface::Ceiling)].emissiveAmount;
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool Renderer::prepareSecurityRoomMaterials(const gdp::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const gdp::procedural::SecurityRoomMaterialSet materials
        = gdp::procedural::generateSecurityRoomMaterials(roomSeed);
    if (!gdp::procedural::validateSecurityRoomMaterials(materials).isValid()) {
        gdp::core::log(
            gdp::core::LogLevel::Error, "Security room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const gdp::procedural::SecuritySurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            gdp::procedural::securitySurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*gdp::procedural::generateSecuritySurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    gpuMaterials.ceilingEmissive = materials.surfaces[static_cast<std::size_t>(
        gdp::procedural::RoomSurface::Ceiling)].emissiveAmount;
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool Renderer::prepareCentralHubRoomMaterials(const gdp::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const gdp::procedural::CentralHubRoomMaterialSet materials
        = gdp::procedural::generateCentralHubRoomMaterials(roomSeed);
    if (!gdp::procedural::validateCentralHubRoomMaterials(materials).isValid()) {
        gdp::core::log(
            gdp::core::LogLevel::Error, "Central Hub material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const gdp::procedural::CentralHubSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            gdp::procedural::centralHubSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*gdp::procedural::generateCentralHubSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    gpuMaterials.ceilingEmissive = materials.surfaces[static_cast<std::size_t>(
        gdp::procedural::RoomSurface::Ceiling)].emissiveAmount;
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool Renderer::prepareWorkerVisual(const gdp::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (workerVisuals_.contains(visualSeed)) {
        return true;
    }

    const gdp::procedural::WorkerVisual visual
        = gdp::procedural::generateWorkerVisual(visualSeed);
    if (!gdp::procedural::validateWorkerVisual(visual).isValid()) {
        gdp::core::log(gdp::core::LogLevel::Error, "Worker visual generation failed validation");
        return false;
    }
    const auto meshData = meshCache_.acquire(
        gdp::procedural::meshRecipeHash(visual.meshRecipe),
        [&visual] {
            return std::move(gdp::procedural::generateMesh(visual.meshRecipe)->mesh);
        });
    const auto textureData = textureCache_.acquire(
        gdp::procedural::textureRecipeHash(visual.textureRecipe),
        [&visual] {
            return std::move(*gdp::procedural::generateAlbedo(visual.textureRecipe));
        });
    EnemyGpuVisual gpuVisual;
    if (!gpuVisual.mesh.upload(gl_, *meshData)
        || !gpuVisual.texture.upload(gl_, *textureData)) {
        return false;
    }
    gpuVisual.emissiveStrength = visual.textureRecipe.emissiveAmount;
    workerVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool Renderer::prepareGuardVisual(const gdp::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (guardVisuals_.contains(visualSeed)) {
        return true;
    }

    const gdp::procedural::GuardVisual visual
        = gdp::procedural::generateGuardVisual(visualSeed);
    if (!gdp::procedural::validateGuardVisual(visual).isValid()) {
        gdp::core::log(gdp::core::LogLevel::Error, "Guard visual generation failed validation");
        return false;
    }
    const auto meshData = meshCache_.acquire(
        gdp::procedural::meshRecipeHash(visual.meshRecipe),
        [&visual] {
            return std::move(gdp::procedural::generateMesh(visual.meshRecipe)->mesh);
        });
    const auto textureData = textureCache_.acquire(
        gdp::procedural::textureRecipeHash(visual.textureRecipe),
        [&visual] {
            return std::move(*gdp::procedural::generateAlbedo(visual.textureRecipe));
        });
    EnemyGpuVisual gpuVisual;
    if (!gpuVisual.mesh.upload(gl_, *meshData)
        || !gpuVisual.texture.upload(gl_, *textureData)) {
        return false;
    }
    gpuVisual.emissiveStrength = visual.textureRecipe.emissiveAmount;
    guardVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool Renderer::prepareOfficerVisual(const gdp::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (officerVisuals_.contains(visualSeed)) {
        return true;
    }

    const gdp::procedural::OfficerVisual visual
        = gdp::procedural::generateOfficerVisual(visualSeed);
    if (!gdp::procedural::validateOfficerVisual(visual).isValid()) {
        gdp::core::log(gdp::core::LogLevel::Error, "Officer visual generation failed validation");
        return false;
    }
    const auto meshData = meshCache_.acquire(
        gdp::procedural::meshRecipeHash(visual.meshRecipe),
        [&visual] {
            return std::move(gdp::procedural::generateMesh(visual.meshRecipe)->mesh);
        });
    const auto textureData = textureCache_.acquire(
        gdp::procedural::textureRecipeHash(visual.textureRecipe),
        [&visual] {
            return std::move(*gdp::procedural::generateAlbedo(visual.textureRecipe));
        });
    EnemyGpuVisual gpuVisual;
    if (!gpuVisual.mesh.upload(gl_, *meshData)
        || !gpuVisual.texture.upload(gl_, *textureData)) {
        return false;
    }
    gpuVisual.emissiveStrength = visual.textureRecipe.emissiveAmount;
    officerVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool Renderer::prepareEngineerVisual(const gdp::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (engineerVisuals_.contains(visualSeed)) {
        return true;
    }

    const gdp::procedural::EngineerVisual visual
        = gdp::procedural::generateEngineerVisual(visualSeed);
    if (!gdp::procedural::validateEngineerVisual(visual).isValid()) {
        gdp::core::log(gdp::core::LogLevel::Error,
            "Engineer visual generation failed validation");
        return false;
    }
    const auto meshData = meshCache_.acquire(
        gdp::procedural::meshRecipeHash(visual.meshRecipe),
        [&visual] {
            return std::move(gdp::procedural::generateMesh(visual.meshRecipe)->mesh);
        });
    const auto textureData = textureCache_.acquire(
        gdp::procedural::textureRecipeHash(visual.textureRecipe),
        [&visual] {
            return std::move(*gdp::procedural::generateAlbedo(visual.textureRecipe));
        });
    EnemyGpuVisual gpuVisual;
    if (!gpuVisual.mesh.upload(gl_, *meshData)
        || !gpuVisual.texture.upload(gl_, *textureData)) {
        return false;
    }
    gpuVisual.emissiveStrength = visual.textureRecipe.emissiveAmount;
    engineerVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool Renderer::prepareIndustrialRobotVisual(const gdp::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (industrialRobotVisuals_.contains(visualSeed)) {
        return true;
    }
    const gdp::procedural::IndustrialRobotVisual visual
        = gdp::procedural::generateIndustrialRobotVisual(visualSeed);
    if (!gdp::procedural::validateIndustrialRobotVisual(visual).isValid()) {
        gdp::core::log(gdp::core::LogLevel::Error,
            "IndustrialRobot visual generation failed validation");
        return false;
    }
    const auto meshData = meshCache_.acquire(
        gdp::procedural::meshRecipeHash(visual.meshRecipe),
        [&visual] {
            return std::move(gdp::procedural::generateMesh(visual.meshRecipe)->mesh);
        });
    const auto textureData = textureCache_.acquire(
        gdp::procedural::textureRecipeHash(visual.textureRecipe),
        [&visual] {
            return std::move(*gdp::procedural::generateAlbedo(visual.textureRecipe));
        });
    EnemyGpuVisual gpuVisual;
    if (!gpuVisual.mesh.upload(gl_, *meshData)
        || !gpuVisual.texture.upload(gl_, *textureData)) {
        return false;
    }
    gpuVisual.emissiveStrength = visual.textureRecipe.emissiveAmount;
    industrialRobotVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool Renderer::render(
    const int width,
    const int height,
    const CameraData camera,
    const std::span<const EnemyRenderData> enemies,
    const std::span<const PickupRenderData> pickups,
    const std::span<const PropRenderData> props,
    const RoomLightingRenderData lighting,
    const RoomRenderData room,
    const std::span<const DoorRenderData> doors)
{
    if (!initialized_ || width <= 0 || height <= 0) {
        return false;
    }
    if (!framebuffer_.resize(gl_, width, height)) {
        return false;
    }

    framebuffer_.bind(gl_);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.018F, 0.025F, 0.03F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader_.use(gl_);
    camera_.setPerspective(camera.verticalFieldOfViewDegrees, 0.05F, 100.0F);
    camera_.lookAt(camera.position, camera.position + camera.forward);
    shader_.setInt(gl_, "uAlbedo", 0);
    shader_.setMat4(gl_, "uView", camera_.viewMatrix());
    shader_.setMat4(
        gl_,
        "uProjection",
        camera_.projectionMatrix(static_cast<float>(width) / static_cast<float>(height)));
    const std::uint32_t lightCount = std::min<std::uint32_t>(
        lighting.lightCount, static_cast<std::uint32_t>(maxRenderedRoomLights));
    const gdp::core::Vec3 alertColor{1.0F, 0.12F, 0.06F};
    const gdp::core::Vec3 ambientColor = lighting.alert
        ? (lighting.ambientColor * 0.28F) + (alertColor * 0.72F)
        : lighting.ambientColor;
    shader_.setInt(gl_, "uLightCount", static_cast<int>(lightCount));
    shader_.setVec3(gl_, "uAmbientColor", ambientColor);
    shader_.setFloat(gl_, "uAmbientIntensity",
        lighting.alert ? std::max(lighting.ambientIntensity, 0.16F)
                       : lighting.ambientIntensity);
    constexpr std::array positionUniforms{
        "uLightPositions[0]", "uLightPositions[1]",
        "uLightPositions[2]", "uLightPositions[3]",
    };
    constexpr std::array colorUniforms{
        "uLightColors[0]", "uLightColors[1]",
        "uLightColors[2]", "uLightColors[3]",
    };
    constexpr std::array intensityUniforms{
        "uLightIntensities[0]", "uLightIntensities[1]",
        "uLightIntensities[2]", "uLightIntensities[3]",
    };
    constexpr std::array rangeUniforms{
        "uLightRanges[0]", "uLightRanges[1]",
        "uLightRanges[2]", "uLightRanges[3]",
    };
    for (std::size_t index = 0; index < lightCount; ++index) {
        const PointLightRenderData& light = lighting.lights[index];
        const gdp::core::Vec3 lightColor = lighting.alert
            ? (light.color * 0.24F) + (alertColor * 0.76F)
            : light.color;
        shader_.setVec3(gl_, positionUniforms[index], light.position);
        shader_.setVec3(gl_, colorUniforms[index], lightColor);
        shader_.setFloat(gl_, intensityUniforms[index],
            lighting.alert ? light.intensity * 1.08F : light.intensity);
        shader_.setFloat(gl_, rangeUniforms[index], light.range);
    }

    auto roomMaterials = roomMaterials_.find(activeRoomMaterialKey_);
    if (roomMaterials == roomMaterials_.end()) {
        roomMaterials = roomMaterials_.find(fallbackRoomMaterialKey_);
    }
    if (roomMaterials == roomMaterials_.end()) {
        return false;
    }

    for (const RoomPart& part : roomParts) {
        float emissiveStrength = 0.0F;
        switch (part.material) {
        case RoomMaterialChannel::Wall:
            roomMaterials->second.textures[static_cast<std::size_t>(
                gdp::procedural::IndustrialSurface::Wall)].bind(gl_);
            break;
        case RoomMaterialChannel::Floor:
            roomMaterials->second.textures[static_cast<std::size_t>(
                gdp::procedural::IndustrialSurface::Floor)].bind(gl_);
            break;
        case RoomMaterialChannel::Ceiling:
            roomMaterials->second.textures[static_cast<std::size_t>(
                gdp::procedural::IndustrialSurface::Ceiling)].bind(gl_);
            emissiveStrength = roomMaterials->second.ceilingEmissive;
            break;
        case RoomMaterialChannel::Generic:
            fallbackTexture_.bind(gl_);
            break;
        }
        const gdp::core::Vec3 position
            = room.origin + gdp::core::rotateQuarterTurns(part.position, room.rotation);
        gdp::core::Vec3 scale = part.scale;
        if (room.rotation == gdp::core::QuarterTurn::Degrees90
            || room.rotation == gdp::core::QuarterTurn::Degrees270) {
            std::swap(scale.x, scale.z);
        }
        const gdp::core::Mat4 model = gdp::core::translationMatrix(position)
            * gdp::core::scaleMatrix(scale);
        shader_.setMat4(gl_, "uModel", model);
        shader_.setVec3(gl_, "uTint", part.tint);
        shader_.setFloat(gl_, "uUvScale", part.uvScale);
        shader_.setFloat(gl_, "uEmissiveStrength", emissiveStrength);
        box_.draw(gl_);
    }

    fallbackTexture_.bind(gl_);
    for (const DoorRenderData& door : doors) {
        const bool facesX = std::abs(door.outwardNormal.x) > 0.5F;
        const gdp::core::Vec3 scale = facesX
            ? gdp::core::Vec3{0.12F, door.height, door.width}
            : gdp::core::Vec3{door.width, door.height, 0.12F};
        const gdp::core::Vec3 position = door.position
            - (door.outwardNormal * 0.12F)
            + gdp::core::Vec3{0.0F, door.height * 0.5F, 0.0F};
        const gdp::core::Mat4 model = gdp::core::translationMatrix(position)
            * gdp::core::scaleMatrix(scale);
        shader_.setMat4(gl_, "uModel", model);
        shader_.setVec3(gl_, "uTint", door.locked
                ? gdp::core::Vec3{0.90F, 0.10F, 0.05F}
                : gdp::core::Vec3{0.10F, 0.66F, 0.72F});
        shader_.setFloat(gl_, "uUvScale", 2.0F);
        shader_.setFloat(gl_, "uEmissiveStrength", door.locked ? 0.65F : 0.28F);
        box_.draw(gl_);
    }

    fallbackTexture_.bind(gl_);
    for (std::size_t index = 0; index < lightCount; ++index) {
        const PointLightRenderData& light = lighting.lights[index];
        const gdp::core::Vec3 fixtureColor = lighting.alert
            ? alertColor : light.color;
        const gdp::core::Mat4 model = gdp::core::translationMatrix(light.position)
            * gdp::core::scaleMatrix({0.62F, 0.08F, 0.24F});
        shader_.setMat4(gl_, "uModel", model);
        shader_.setVec3(gl_, "uTint", fixtureColor);
        shader_.setFloat(gl_, "uUvScale", 1.0F);
        shader_.setFloat(gl_, "uEmissiveStrength",
            lighting.alert ? lighting.fixtureEmissive * 1.35F : lighting.fixtureEmissive);
        box_.draw(gl_);
    }

    for (const EnemyRenderData& enemy : enemies) {
        auto* visuals = &workerVisuals_;
        if (enemy.role == EnemyVisualRole::Guard) {
            visuals = &guardVisuals_;
        } else if (enemy.role == EnemyVisualRole::Officer) {
            visuals = &officerVisuals_;
        } else if (enemy.role == EnemyVisualRole::Engineer) {
            visuals = &engineerVisuals_;
        } else if (enemy.role == EnemyVisualRole::IndustrialRobot) {
            visuals = &industrialRobotVisuals_;
        }
        auto visual = visuals->find(enemy.visualSeed);
        if (visual == visuals->end()) {
            visual = visuals->find(0);
        }
        if (visual == visuals->end()) {
            return false;
        }
        visual->second.texture.bind(gl_);
        const gdp::core::Vec3 position = enemy.position
            + (enemy.alive ? gdp::core::Vec3{} : gdp::core::Vec3{0.0F, 0.04F, 0.0F});
        const gdp::core::Vec3 scale = enemy.alive
            ? gdp::core::Vec3{1.0F, 1.0F, 1.0F}
            : gdp::core::Vec3{1.15F, 0.18F, 1.15F};
        const gdp::core::Mat4 model = gdp::core::translationMatrix(position)
            * gdp::core::scaleMatrix(scale);
        shader_.setMat4(gl_, "uModel", model);
        shader_.setVec3(
            gl_, "uTint", enemy.alive
                ? gdp::core::Vec3{1.0F, 1.0F, 1.0F}
                : gdp::core::Vec3{0.30F, 0.24F, 0.20F});
        shader_.setFloat(gl_, "uUvScale", 4.0F);
        shader_.setFloat(gl_, "uEmissiveStrength",
            enemy.alive ? visual->second.emissiveStrength : 0.0F);
        visual->second.mesh.draw(gl_);
    }

    fallbackTexture_.bind(gl_);
    for (const PickupRenderData& pickup : pickups) {
        gdp::core::Vec3 tint{0.20F, 0.85F, 0.32F};
        gdp::core::Vec3 scale{0.30F, 0.22F, 0.30F};
        if (pickup.role == PickupVisualRole::Ammo) {
            tint = {0.92F, 0.68F, 0.16F};
            scale = {0.34F, 0.18F, 0.24F};
        } else if (pickup.role == PickupVisualRole::Key) {
            tint = {0.18F, 0.78F, 0.92F};
            scale = {0.38F, 0.12F, 0.16F};
        }
        const gdp::core::Mat4 model
            = gdp::core::translationMatrix(
                pickup.position + gdp::core::Vec3{0.0F, scale.y * 0.5F, 0.0F})
            * gdp::core::scaleMatrix(scale);
        shader_.setMat4(gl_, "uModel", model);
        shader_.setVec3(gl_, "uTint", tint);
        shader_.setFloat(gl_, "uUvScale", 2.0F);
        shader_.setFloat(gl_, "uEmissiveStrength", 0.16F);
        box_.draw(gl_);
    }

    for (const PropRenderData& prop : props) {
        gdp::core::Vec3 tint{0.54F, 0.34F, 0.17F};
        if (prop.role == PropVisualRole::Canister) {
            tint = {0.68F, 0.18F, 0.10F};
        } else if (prop.role == PropVisualRole::Barricade) {
            tint = {0.24F, 0.34F, 0.40F};
        }
        const float seedVariation = static_cast<float>((prop.visualSeed >> 8U) & 0xFFU)
            / 255.0F * 0.12F - 0.06F;
        tint.x = std::clamp(tint.x + seedVariation, 0.08F, 1.0F);
        tint.y = std::clamp(tint.y + seedVariation, 0.08F, 1.0F);
        tint.z = std::clamp(tint.z + seedVariation, 0.08F, 1.0F);
        gdp::core::Vec3 scale = prop.scale;
        if (prop.state == PropVisualState::Damaged) {
            tint = tint * 0.62F;
        } else if (prop.state == PropVisualState::Destroyed) {
            tint = tint * 0.30F;
            scale.y = std::min(scale.y, 0.12F);
            scale.x *= 1.12F;
            scale.z *= 1.12F;
        }
        const gdp::core::Mat4 model = gdp::core::translationMatrix(
            prop.position + gdp::core::Vec3{0.0F, scale.y * 0.5F, 0.0F})
            * gdp::core::scaleMatrix(scale);
        shader_.setMat4(gl_, "uModel", model);
        shader_.setVec3(gl_, "uTint", tint);
        shader_.setFloat(gl_, "uUvScale", 3.0F);
        shader_.setFloat(gl_, "uEmissiveStrength", 0.0F);
        box_.draw(gl_);
    }

    framebuffer_.presentToDefault(gl_, width, height);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0.78F, 0.88F, 0.90F, 1.0F);
    glScissor((width / 2) - 7, (height / 2) - 1, 15, 3);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor((width / 2) - 1, (height / 2) - 7, 3, 15);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::ostringstream message;
        message << "OpenGL render error: 0x" << std::hex << error;
        gdp::core::log(gdp::core::LogLevel::Error, message.str());
        return false;
    }
    return true;
}

} // namespace gdp::render
