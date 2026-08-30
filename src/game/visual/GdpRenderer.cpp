#include "game/visual/GdpRenderer.hpp"

#include "core/Log.hpp"
#include "core/Math.hpp"
#include "game/visual/EnemyGenerator.hpp"
#include "game/visual/RoomMaterialGenerator.hpp"
#include "game/visual/VisualAssets.hpp"
#include "procedural/MeshGenerator.hpp"
#include "procedural/TextureGenerator.hpp"

#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace gdp::game::visual {
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
uniform sampler2D uNormalMap;
uniform sampler2D uMaterialMap;
uniform sampler2D uEmissionMap;
uniform vec3 uTint;
uniform vec3 uCameraPosition;
const int MAX_LIGHTS = 8;
uniform int uLightCount;
uniform vec3 uLightPositions[MAX_LIGHTS];
uniform vec3 uLightColors[MAX_LIGHTS];
uniform float uLightIntensities[MAX_LIGHTS];
uniform float uLightRanges[MAX_LIGHTS];
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;
uniform vec2 uUvScale;
uniform vec2 uUvOffset;
uniform float uEmissiveStrength;
uniform int uDetailedMaterial;
uniform int uUseEmissionMap;

out vec4 fragmentColor;

mat3 cotangentFrame(vec3 normal, vec3 position, vec2 uv)
{
    vec3 positionX = dFdx(position);
    vec3 positionY = dFdy(position);
    vec2 uvX = dFdx(uv);
    vec2 uvY = dFdy(uv);
    vec3 perpendicularX = cross(positionY, normal);
    vec3 perpendicularY = cross(normal, positionX);
    vec3 tangent = perpendicularX * uvX.x + perpendicularY * uvY.x;
    vec3 bitangent = perpendicularX * uvX.y + perpendicularY * uvY.y;
    float scale = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1.0e-8));
    return mat3(tangent * scale, bitangent * scale, normal);
}

void main()
{
    vec2 materialUv = (vUv * uUvScale) + uUvOffset;
    vec3 normal = normalize(vNormal);
    vec3 albedo = texture(uAlbedo, materialUv).rgb * uTint;
    float roughness = 0.72;
    float metalness = 0.0;
    float ambientOcclusion = 1.0;
    if (uDetailedMaterial != 0) {
        vec3 tangentNormal = texture(uNormalMap, materialUv).xyz * 2.0 - 1.0;
        normal = normalize(cotangentFrame(normal, vWorldPosition, materialUv) * tangentNormal);
        vec3 material = texture(uMaterialMap, materialUv).rgb;
        roughness = clamp(material.r, 0.08, 1.0);
        metalness = clamp(material.g, 0.0, 1.0);
        ambientOcclusion = clamp(material.b, 0.0, 1.0);
    }
    vec3 result = albedo * uAmbientColor * uAmbientIntensity * ambientOcclusion;
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    for (int index = 0; index < MAX_LIGHTS; ++index) {
        if (index >= uLightCount) {
            break;
        }
        vec3 toLight = uLightPositions[index] - vWorldPosition;
        float distanceToLight = length(toLight);
        vec3 lightDirection = normalize(toLight);
        float diffuse = max(dot(normal, lightDirection), 0.0);
        float normalizedDistance = distanceToLight / max(uLightRanges[index], 0.001);
        float attenuation = 1.0 / (1.0 + 2.2 * normalizedDistance * normalizedDistance);
        vec3 radiance = uLightColors[index] * attenuation
            * uLightIntensities[index] * 1.9;
        result += albedo * (1.0 - metalness) * diffuse * radiance;
        if (uDetailedMaterial != 0 && diffuse > 0.0) {
            vec3 halfway = normalize(lightDirection + viewDirection);
            float exponent = mix(96.0, 8.0, roughness);
            float highlight = pow(max(dot(normal, halfway), 0.0), exponent)
                * mix(0.90, 0.12, roughness);
            vec3 reflectance = mix(vec3(0.035), albedo, metalness);
            result += reflectance * highlight * diffuse * radiance;
        }
    }
    float emissiveMask = uUseEmissionMap != 0
        ? texture(uEmissionMap, materialUv).r
        : smoothstep(0.62, 0.82, max(albedo.r, max(albedo.g, albedo.b)));
    fragmentColor = vec4(result + (albedo * emissiveMask * uEmissiveStrength), 1.0);
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
    RoomPart{{-2.8F, 1.0F, -3.8F}, {0.45F, 2.0F, 0.45F}, {0.75F, 0.57F, 0.22F},
        RoomMaterialChannel::Generic, 4.0F},
    RoomPart{{2.8F, 1.0F, -3.8F}, {0.45F, 2.0F, 0.45F}, {0.75F, 0.57F, 0.22F},
        RoomMaterialChannel::Generic, 4.0F},
    RoomPart{{0.0F, 0.35F, -2.2F}, {2.2F, 0.7F, 1.2F}, {0.42F, 0.48F, 0.51F},
        RoomMaterialChannel::Generic, 4.0F},
    RoomPart{{0.0F, 1.0F, 2.2F}, {0.8F, 2.0F, 0.3F}, {0.25F, 0.72F, 0.68F},
        RoomMaterialChannel::Generic, 4.0F},
};

using Glyph = std::array<std::uint8_t, 7U>;

constexpr Glyph glyphFor(const char character) noexcept
{
    switch (character) {
    case '0': return {0x0EU, 0x11U, 0x13U, 0x15U, 0x19U, 0x11U, 0x0EU};
    case '1': return {0x04U, 0x0CU, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU};
    case '2': return {0x0EU, 0x11U, 0x01U, 0x02U, 0x04U, 0x08U, 0x1FU};
    case '3': return {0x1EU, 0x01U, 0x01U, 0x0EU, 0x01U, 0x01U, 0x1EU};
    case '4': return {0x02U, 0x06U, 0x0AU, 0x12U, 0x1FU, 0x02U, 0x02U};
    case '5': return {0x1FU, 0x10U, 0x10U, 0x1EU, 0x01U, 0x01U, 0x1EU};
    case '6': return {0x0EU, 0x10U, 0x10U, 0x1EU, 0x11U, 0x11U, 0x0EU};
    case '7': return {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x08U, 0x08U};
    case '8': return {0x0EU, 0x11U, 0x11U, 0x0EU, 0x11U, 0x11U, 0x0EU};
    case '9': return {0x0EU, 0x11U, 0x11U, 0x0FU, 0x01U, 0x01U, 0x0EU};
    case 'A': return {0x0EU, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U};
    case 'B': return {0x1EU, 0x11U, 0x11U, 0x1EU, 0x11U, 0x11U, 0x1EU};
    case 'C': return {0x0FU, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x0FU};
    case 'D': return {0x1EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x1EU};
    case 'E': return {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x1FU};
    case 'F': return {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x10U};
    case 'G': return {0x0FU, 0x10U, 0x10U, 0x17U, 0x11U, 0x11U, 0x0FU};
    case 'H': return {0x11U, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U};
    case 'I': return {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x1FU};
    case 'J': return {0x07U, 0x02U, 0x02U, 0x02U, 0x12U, 0x12U, 0x0CU};
    case 'K': return {0x11U, 0x12U, 0x14U, 0x18U, 0x14U, 0x12U, 0x11U};
    case 'L': return {0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x1FU};
    case 'M': return {0x11U, 0x1BU, 0x15U, 0x15U, 0x11U, 0x11U, 0x11U};
    case 'N': return {0x11U, 0x19U, 0x19U, 0x15U, 0x13U, 0x13U, 0x11U};
    case 'O': return {0x0EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU};
    case 'P': return {0x1EU, 0x11U, 0x11U, 0x1EU, 0x10U, 0x10U, 0x10U};
    case 'Q': return {0x0EU, 0x11U, 0x11U, 0x11U, 0x15U, 0x12U, 0x0DU};
    case 'R': return {0x1EU, 0x11U, 0x11U, 0x1EU, 0x14U, 0x12U, 0x11U};
    case 'S': return {0x0FU, 0x10U, 0x10U, 0x0EU, 0x01U, 0x01U, 0x1EU};
    case 'T': return {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U};
    case 'U': return {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU};
    case 'V': return {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0AU, 0x04U};
    case 'W': return {0x11U, 0x11U, 0x11U, 0x15U, 0x15U, 0x15U, 0x0AU};
    case 'X': return {0x11U, 0x11U, 0x0AU, 0x04U, 0x0AU, 0x11U, 0x11U};
    case 'Y': return {0x11U, 0x11U, 0x0AU, 0x04U, 0x04U, 0x04U, 0x04U};
    case 'Z': return {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x1FU};
    case '-': return {0x00U, 0x00U, 0x00U, 0x0EU, 0x00U, 0x00U, 0x00U};
    case '/': return {0x01U, 0x02U, 0x02U, 0x04U, 0x08U, 0x08U, 0x10U};
    case ':': return {0x00U, 0x04U, 0x04U, 0x00U, 0x04U, 0x04U, 0x00U};
    default: return {};
    }
}

int bitmapTextWidth(const std::string_view text, const int scale) noexcept
{
    return static_cast<int>(text.size()) * 6 * scale;
}

void drawSolidRect(
    const int x,
    const int y,
    const int width,
    const int height,
    const gdp::core::Vec3 color)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    glClearColor(color.x, color.y, color.z, 1.0F);
    glScissor(x, y, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
}

void drawBitmapText(
    const int x,
    const int y,
    const int scale,
    const std::string_view text,
    const gdp::core::Vec3 color)
{
    glClearColor(color.x, color.y, color.z, 1.0F);
    int cursor = x;
    for (const char character : text) {
        const Glyph glyph = glyphFor(character);
        for (std::size_t row = 0; row < glyph.size(); ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((glyph[row] & (1U << (4 - column))) == 0U) {
                    continue;
                }
                glScissor(cursor + (column * scale),
                    y + (static_cast<int>(glyph.size() - 1U - row) * scale),
                    scale, scale);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }
        cursor += 6 * scale;
    }
}

} // namespace

bool GdpRenderer::initialize(const int width, const int height)
{
    if (!loadGlApi(gl_)) {
        return false;
    }
    if (!shader_.compile(gl_, vertexShaderSource, fragmentShaderSource)) {
        return false;
    }
    if (!box_.upload(gl_, gdp::render::makeBoxMesh())) {
        return false;
    }
    if (!fallbackTexture_.upload(gl_, gdp::game::makeIndustrialCheckerImage())) {
        return false;
    }
    const gdp::game::CeilingFixtureMaterialImages fixtureImages
        = gdp::game::makeCeilingFixtureMaterialImages();
    if (!ceilingFixtureMaterial_.albedo.upload(
            gl_, fixtureImages.albedo, gdp::render::TextureSampling::Trilinear)
        || !ceilingFixtureMaterial_.normal.upload(
            gl_, fixtureImages.normal, gdp::render::TextureSampling::Trilinear)
        || !ceilingFixtureMaterial_.properties.upload(
            gl_, fixtureImages.properties, gdp::render::TextureSampling::Trilinear)
        || !ceilingFixtureMaterial_.emission.upload(
            gl_, fixtureImages.emission, gdp::render::TextureSampling::Trilinear)) {
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

bool GdpRenderer::prepareIndustrialRoomMaterials(const gdp::core::Seed roomSeed)
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

    const std::size_t conditionIndex = static_cast<std::size_t>(materials.condition);
    auto& sharedWall = industrialWallMaterials_[conditionIndex];
    if (!sharedWall) {
        const gdp::procedural::IndustrialSurfaceRecipe archetype
            = gdp::procedural::makeIndustrialWallArchetypeRecipe(materials.condition);
        const auto images = gdp::procedural::generateIndustrialWallMaterialImages(archetype);
        if (!images) {
            return false;
        }
        IndustrialWallGpuMaterial wallMaterial;
        if (!wallMaterial.albedo.upload(
                gl_, images->albedo, gdp::render::TextureSampling::Trilinear)
            || !wallMaterial.normal.upload(
                gl_, images->normal, gdp::render::TextureSampling::Trilinear)
            || !wallMaterial.properties.upload(
                gl_, images->properties, gdp::render::TextureSampling::Trilinear)) {
            return false;
        }
        sharedWall.emplace(std::move(wallMaterial));
    }

    auto& sharedCeiling = industrialCeilingMaterials_[conditionIndex];
    if (!sharedCeiling) {
        const gdp::procedural::IndustrialSurfaceRecipe archetype
            = gdp::procedural::makeIndustrialCeilingArchetypeRecipe(materials.condition);
        const auto images = gdp::procedural::generateIndustrialCeilingMaterialImages(
            archetype);
        if (!images) {
            return false;
        }
        IndustrialWallGpuMaterial ceilingMaterial;
        if (!ceilingMaterial.albedo.upload(
                gl_, images->albedo, gdp::render::TextureSampling::Trilinear)
            || !ceilingMaterial.normal.upload(
                gl_, images->normal, gdp::render::TextureSampling::Trilinear)
            || !ceilingMaterial.properties.upload(
                gl_, images->properties, gdp::render::TextureSampling::Trilinear)) {
            return false;
        }
        sharedCeiling.emplace(std::move(ceilingMaterial));
    }

    RoomGpuMaterials gpuMaterials;
    gpuMaterials.industrialWallCondition = materials.condition;
    gpuMaterials.industrialCeilingCondition = materials.condition;
    gpuMaterials.wallUvOffset = {
        static_cast<float>((roomSeed >> 8U) & 1U) * 0.5F,
        0.0F,
    };
    gpuMaterials.ceilingUvOffset = {
        static_cast<float>((roomSeed >> 10U) & 1U) * 0.5F,
        static_cast<float>((roomSeed >> 11U) & 1U) * 0.5F,
    };
    const auto& wallRecipe = materials.surfaces[static_cast<std::size_t>(
        gdp::procedural::IndustrialSurface::Wall)];
    gpuMaterials.wallTone = std::clamp(wallRecipe.baseColor.x / 0.33F, 0.94F, 1.06F);
    const auto& ceilingRecipe = materials.surfaces[static_cast<std::size_t>(
        gdp::procedural::IndustrialSurface::Ceiling)];
    gpuMaterials.ceilingTone = std::clamp(
        ceilingRecipe.baseColor.x / 0.38F, 0.94F, 1.06F);
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const gdp::procedural::IndustrialSurfaceRecipe& recipe = materials.surfaces[index];
        if (recipe.surface == gdp::procedural::IndustrialSurface::Wall
            || recipe.surface == gdp::procedural::IndustrialSurface::Ceiling) {
            continue;
        }
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
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    if (roomSeed == 0U) {
        fallbackRoomMaterialKey_ = materials.materialHash;
    }
    return true;
}

bool GdpRenderer::prepareStorageRoomMaterials(const gdp::core::Seed roomSeed)
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
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareCombatRoomMaterials(const gdp::core::Seed roomSeed)
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
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareCafeteriaRoomMaterials(const gdp::core::Seed roomSeed)
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
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareLaboratoryRoomMaterials(const gdp::core::Seed roomSeed)
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
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareTechnicalRoomMaterials(const gdp::core::Seed roomSeed)
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
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareSecurityRoomMaterials(const gdp::core::Seed roomSeed)
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
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareCentralHubRoomMaterials(const gdp::core::Seed roomSeed)
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
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareWorkerVisual(const gdp::core::Seed visualSeed)
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

bool GdpRenderer::prepareGuardVisual(const gdp::core::Seed visualSeed)
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

bool GdpRenderer::prepareOfficerVisual(const gdp::core::Seed visualSeed)
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

bool GdpRenderer::prepareEngineerVisual(const gdp::core::Seed visualSeed)
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

bool GdpRenderer::prepareIndustrialRobotVisual(const gdp::core::Seed visualSeed)
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

bool GdpRenderer::render(
    const int width,
    const int height,
    const CameraData camera,
    const std::span<const EnemyRenderData> enemies,
    const std::span<const PickupRenderData> pickups,
    const std::span<const PropRenderData> props,
    const RoomLightingRenderData lighting,
    const RoomRenderData room,
    const std::span<const DoorRenderData> doors,
    const std::span<const CorridorRenderData> corridors,
    const std::span<const RoomRenderData> connectedRooms,
    const HudRenderData hud)
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
    shader_.setInt(gl_, "uNormalMap", 1);
    shader_.setInt(gl_, "uMaterialMap", 2);
    shader_.setInt(gl_, "uEmissionMap", 3);
    shader_.setInt(gl_, "uUseEmissionMap", 0);
    shader_.setVec3(gl_, "uCameraPosition", camera.position);
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
        "uLightPositions[4]", "uLightPositions[5]",
        "uLightPositions[6]", "uLightPositions[7]",
    };
    constexpr std::array colorUniforms{
        "uLightColors[0]", "uLightColors[1]",
        "uLightColors[2]", "uLightColors[3]",
        "uLightColors[4]", "uLightColors[5]",
        "uLightColors[6]", "uLightColors[7]",
    };
    constexpr std::array intensityUniforms{
        "uLightIntensities[0]", "uLightIntensities[1]",
        "uLightIntensities[2]", "uLightIntensities[3]",
        "uLightIntensities[4]", "uLightIntensities[5]",
        "uLightIntensities[6]", "uLightIntensities[7]",
    };
    constexpr std::array rangeUniforms{
        "uLightRanges[0]", "uLightRanges[1]",
        "uLightRanges[2]", "uLightRanges[3]",
        "uLightRanges[4]", "uLightRanges[5]",
        "uLightRanges[6]", "uLightRanges[7]",
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

    const auto fallbackMaterials = roomMaterials_.find(fallbackRoomMaterialKey_);
    if (fallbackMaterials == roomMaterials_.end()) {
        return false;
    }
    const auto resolveMaterials = [this, &fallbackMaterials](const std::uint64_t key)
        -> const RoomGpuMaterials& {
        const std::uint64_t requestedKey = key == 0U ? activeRoomMaterialKey_ : key;
        const auto materials = roomMaterials_.find(requestedKey);
        return materials == roomMaterials_.end()
            ? fallbackMaterials->second : materials->second;
    };

    const auto drawSurface = [this](
        const RoomGpuMaterials& roomMaterials,
        const gdp::core::Vec3 position,
        const gdp::core::Vec3 scale,
        const RoomMaterialChannel material,
        const float uvScale = 1.0F,
        const gdp::core::Vec3 tint = {1.0F, 1.0F, 1.0F},
        const gdp::core::Vec2 detailedUvScale = {},
        const gdp::core::Vec2 detailedUvOffset = {}) {
        float emissiveStrength = 0.0F;
        bool detailedMaterial = false;
        gdp::core::Vec3 resolvedTint = tint;
        gdp::core::Vec2 materialUvOffset{};
        switch (material) {
        case RoomMaterialChannel::Wall:
            if (roomMaterials.industrialWallCondition) {
                const std::size_t conditionIndex = static_cast<std::size_t>(
                    *roomMaterials.industrialWallCondition);
                const auto& wallMaterial = industrialWallMaterials_[conditionIndex];
                if (wallMaterial) {
                    wallMaterial->albedo.bind(gl_, 0U);
                    wallMaterial->normal.bind(gl_, 1U);
                    wallMaterial->properties.bind(gl_, 2U);
                    detailedMaterial = true;
                    resolvedTint = tint * roomMaterials.wallTone;
                    materialUvOffset = roomMaterials.wallUvOffset;
                }
            }
            if (!detailedMaterial) {
                roomMaterials.textures[static_cast<std::size_t>(
                    gdp::procedural::IndustrialSurface::Wall)].bind(gl_);
            }
            break;
        case RoomMaterialChannel::Floor:
            roomMaterials.textures[static_cast<std::size_t>(
                gdp::procedural::IndustrialSurface::Floor)].bind(gl_);
            break;
        case RoomMaterialChannel::Ceiling:
            if (roomMaterials.industrialCeilingCondition) {
                const std::size_t conditionIndex = static_cast<std::size_t>(
                    *roomMaterials.industrialCeilingCondition);
                const auto& ceilingMaterial = industrialCeilingMaterials_[conditionIndex];
                if (ceilingMaterial) {
                    ceilingMaterial->albedo.bind(gl_, 0U);
                    ceilingMaterial->normal.bind(gl_, 1U);
                    ceilingMaterial->properties.bind(gl_, 2U);
                    detailedMaterial = true;
                    resolvedTint = tint * roomMaterials.ceilingTone;
                    materialUvOffset = roomMaterials.ceilingUvOffset;
                }
            }
            if (!detailedMaterial) {
                roomMaterials.textures[static_cast<std::size_t>(
                    gdp::procedural::IndustrialSurface::Ceiling)].bind(gl_);
            }
            break;
        case RoomMaterialChannel::Generic:
            fallbackTexture_.bind(gl_);
            break;
        }
        shader_.setMat4(gl_, "uModel", gdp::core::translationMatrix(position)
                * gdp::core::scaleMatrix(scale));
        shader_.setVec3(gl_, "uTint", resolvedTint);
        shader_.setInt(gl_, "uDetailedMaterial", detailedMaterial ? 1 : 0);
        const bool customUv = detailedMaterial
            && detailedUvScale.x > 0.0F && detailedUvScale.y > 0.0F;
        shader_.setVec2(gl_, "uUvScale",
            customUv ? detailedUvScale : gdp::core::Vec2{uvScale, uvScale});
        shader_.setVec2(gl_, "uUvOffset",
            customUv
                ? detailedUvOffset + materialUvOffset
                : gdp::core::Vec2{});
        shader_.setFloat(gl_, "uEmissiveStrength", emissiveStrength);
        box_.draw(gl_);
    };

    const auto drawRoomShell = [&](const RoomRenderData renderedRoom) {
        const RoomGpuMaterials& roomMaterials = resolveMaterials(renderedRoom.materialKey);
        const gdp::core::Aabb bounds = renderedRoom.worldBounds;
        const float roomWidth = bounds.maximum.x - bounds.minimum.x;
        const float roomDepth = bounds.maximum.z - bounds.minimum.z;
        const float roomHeight = bounds.maximum.y - bounds.minimum.y;
        const gdp::core::Vec3 roomCenter{
            (bounds.minimum.x + bounds.maximum.x) * 0.5F,
            bounds.minimum.y,
            (bounds.minimum.z + bounds.maximum.z) * 0.5F,
        };
        constexpr float shellThickness = 0.20F;
        drawSurface(roomMaterials,
            roomCenter + gdp::core::Vec3{0.0F, -shellThickness * 0.5F, 0.0F},
            {roomWidth + shellThickness, shellThickness,
                roomDepth + shellThickness},
            RoomMaterialChannel::Floor, 4.0F);
        drawSurface(roomMaterials,
            roomCenter
                + gdp::core::Vec3{0.0F, roomHeight + shellThickness * 0.5F, 0.0F},
            {roomWidth + shellThickness, shellThickness,
                roomDepth + shellThickness},
            RoomMaterialChannel::Ceiling, 1.0F, {1.0F, 1.0F, 1.0F},
            {roomWidth / 4.0F, roomDepth / 4.0F},
            {bounds.minimum.x / 4.0F, bounds.minimum.z / 4.0F});

        const auto drawWall = [&](const gdp::core::Vec3 normal) {
            const bool facesX = std::abs(normal.x) > 0.5F;
            const float wallCenter = facesX
                ? (normal.x > 0.0F ? bounds.maximum.x : bounds.minimum.x)
                : (normal.z > 0.0F ? bounds.maximum.z : bounds.minimum.z);
            const float transverseMinimum = facesX
                ? bounds.minimum.z
                : bounds.minimum.x;
            const float transverseMaximum = facesX
                ? bounds.maximum.z
                : bounds.maximum.x;
            const auto door = std::find_if(
                doors.begin(), doors.end(), [&](const DoorRenderData& candidate) {
                    const float candidatePlane
                        = facesX ? candidate.position.x : candidate.position.z;
                    const float candidateTransverse
                        = facesX ? candidate.position.z : candidate.position.x;
                    return gdp::core::dot(normal, candidate.outwardNormal) > 0.90F
                        && std::abs(candidatePlane - wallCenter) < 0.25F
                        && candidateTransverse >= transverseMinimum - 0.01F
                        && candidateTransverse <= transverseMaximum + 0.01F;
                });
            const auto drawWallSpan = [&](const float minimum, const float maximum,
                                          const float bottom, const float top) {
                if (maximum - minimum <= 1.0e-4F || top - bottom <= 1.0e-4F) {
                    return;
                }
                const float transverseCenter = (minimum + maximum) * 0.5F;
                const gdp::core::Vec3 position = facesX
                    ? gdp::core::Vec3{wallCenter,
                        bounds.minimum.y + ((bottom + top) * 0.5F),
                        transverseCenter}
                    : gdp::core::Vec3{transverseCenter,
                        bounds.minimum.y + ((bottom + top) * 0.5F),
                        wallCenter};
                const gdp::core::Vec3 scale = facesX
                    ? gdp::core::Vec3{
                        shellThickness, top - bottom, maximum - minimum}
                    : gdp::core::Vec3{
                        maximum - minimum, top - bottom, shellThickness};
                drawSurface(roomMaterials, position, scale, RoomMaterialChannel::Wall,
                    1.0F, {1.0F, 1.0F, 1.0F},
                    {(maximum - minimum) / 4.0F, (top - bottom) / roomHeight},
                    {minimum / 4.0F, bottom / roomHeight});
            };
            if (door == doors.end()) {
                drawWallSpan(transverseMinimum, transverseMaximum, 0.0F, roomHeight);
                return;
            }
            const float doorCenter = facesX ? door->position.z : door->position.x;
            const float openingMinimum = std::clamp(
                doorCenter - (door->width * 0.5F),
                transverseMinimum, transverseMaximum);
            const float openingMaximum = std::clamp(
                doorCenter + (door->width * 0.5F),
                transverseMinimum, transverseMaximum);
            drawWallSpan(transverseMinimum, openingMinimum, 0.0F, roomHeight);
            drawWallSpan(openingMaximum, transverseMaximum, 0.0F, roomHeight);
            drawWallSpan(openingMinimum, openingMaximum,
                std::min(door->height, roomHeight), roomHeight);
        };
        drawWall({1.0F, 0.0F, 0.0F});
        drawWall({-1.0F, 0.0F, 0.0F});
        drawWall({0.0F, 0.0F, 1.0F});
        drawWall({0.0F, 0.0F, -1.0F});
    };
    drawRoomShell(room);
    for (const RoomRenderData connectedRoom : connectedRooms) {
        drawRoomShell(connectedRoom);
    }

    constexpr float corridorShellThickness = 0.12F;
    for (const CorridorRenderData& corridor : corridors) {
        const RoomGpuMaterials& roomMaterials = resolveMaterials(corridor.materialKey);
        const float corridorHalfWidth = corridor.width * 0.5F;
        for (std::size_t index = 1U; index + 1U < corridor.points.size(); ++index) {
            const gdp::core::Vec3 incoming = gdp::core::normalized(
                corridor.points[index] - corridor.points[index - 1U]);
            const gdp::core::Vec3 outgoing = gdp::core::normalized(
                corridor.points[index + 1U] - corridor.points[index]);
            if (std::abs(gdp::core::dot(incoming, outgoing)) > 0.5F) {
                continue;
            }
            const gdp::core::Vec3 corner = corridor.points[index];
            const gdp::core::Vec3 cornerScale{
                corridor.width, corridorShellThickness, corridor.width};
            drawSurface(roomMaterials,
                corner + gdp::core::Vec3{
                    0.0F, -corridorShellThickness * 0.5F, 0.0F},
                cornerScale, RoomMaterialChannel::Floor, 2.0F);
            drawSurface(roomMaterials,
                corner + gdp::core::Vec3{
                    0.0F, corridor.height + corridorShellThickness * 0.5F, 0.0F},
                cornerScale, RoomMaterialChannel::Ceiling, 1.0F,
                {1.0F, 1.0F, 1.0F},
                {corridor.width / 4.0F, corridor.width / 4.0F},
                {corner.x / 4.0F, corner.z / 4.0F});
        }
        for (std::size_t index = 0; index + 1U < corridor.points.size(); ++index) {
            const gdp::core::Vec3 first = corridor.points[index];
            const gdp::core::Vec3 second = corridor.points[index + 1U];
            const bool alongX = gdp::core::nearlyEqual(first.z, second.z);
            const float length = alongX
                ? std::abs(second.x - first.x) : std::abs(second.z - first.z);
            if (length <= 1.0e-4F) {
                continue;
            }
            const gdp::core::Vec3 center = (first + second) * 0.5F;
            const gdp::core::Vec3 floorScale = alongX
                ? gdp::core::Vec3{length, corridorShellThickness, corridor.width}
                : gdp::core::Vec3{corridor.width, corridorShellThickness, length};
            drawSurface(roomMaterials,
                center + gdp::core::Vec3{0.0F, -corridorShellThickness * 0.5F, 0.0F},
                floorScale, RoomMaterialChannel::Floor, 2.0F);
            drawSurface(roomMaterials, center + gdp::core::Vec3{
                    0.0F, corridor.height + corridorShellThickness * 0.5F, 0.0F},
                floorScale, RoomMaterialChannel::Ceiling, 1.0F,
                {1.0F, 1.0F, 1.0F},
                {floorScale.x / 4.0F, floorScale.z / 4.0F},
                {(alongX ? std::min(first.x, second.x) : center.x) / 4.0F,
                    (alongX ? center.z : std::min(first.z, second.z)) / 4.0F});
            const gdp::core::Vec3 direction = gdp::core::normalized(second - first);
            const gdp::core::Vec3 left{-direction.z, 0.0F, direction.x};
            for (const float sideSign : {-1.0F, 1.0F}) {
                const gdp::core::Vec3 side = left * sideSign;
                gdp::core::Vec3 wallStart = first;
                gdp::core::Vec3 wallEnd = second;
                if (index > 0U) {
                    const gdp::core::Vec3 previousDirection = gdp::core::normalized(
                        first - corridor.points[index - 1U]);
                    if (std::abs(gdp::core::dot(previousDirection, direction)) < 0.5F) {
                        const bool inner = gdp::core::dot(
                            side, previousDirection * -1.0F) > 0.5F;
                        wallStart = wallStart
                            + direction * (inner ? corridorHalfWidth : -corridorHalfWidth);
                    }
                }
                if (index + 2U < corridor.points.size()) {
                    const gdp::core::Vec3 nextDirection = gdp::core::normalized(
                        corridor.points[index + 2U] - second);
                    if (std::abs(gdp::core::dot(direction, nextDirection)) < 0.5F) {
                        const bool inner = gdp::core::dot(side, nextDirection) > 0.5F;
                        wallEnd = wallEnd
                            + direction * (inner ? -corridorHalfWidth : corridorHalfWidth);
                    }
                }
                const float wallLength = alongX
                    ? std::abs(wallEnd.x - wallStart.x)
                    : std::abs(wallEnd.z - wallStart.z);
                if (wallLength <= 1.0e-4F) {
                    continue;
                }
                const gdp::core::Vec3 wallScale = alongX
                    ? gdp::core::Vec3{
                        wallLength, corridor.height, corridorShellThickness}
                    : gdp::core::Vec3{
                        corridorShellThickness, corridor.height, wallLength};
                const gdp::core::Vec3 wallCenter = (wallStart + wallEnd) * 0.5F
                    + side * corridorHalfWidth
                    + gdp::core::Vec3{0.0F, corridor.height * 0.5F, 0.0F};
                drawSurface(roomMaterials, wallCenter,
                    wallScale, RoomMaterialChannel::Wall,
                    1.0F, {1.0F, 1.0F, 1.0F},
                    {wallLength / 4.0F, 1.0F},
                    {(alongX ? std::min(wallStart.x, wallEnd.x)
                             : std::min(wallStart.z, wallEnd.z)) / 4.0F, 0.0F});
            }
        }
    }

    const auto drawRoomParts = [&](const RoomRenderData& renderedRoom) {
        const RoomGpuMaterials& roomMaterials = resolveMaterials(renderedRoom.materialKey);
        for (const RoomPart& part : roomParts) {
            const gdp::core::Vec3 position = renderedRoom.origin
                + gdp::core::rotateQuarterTurns(part.position, renderedRoom.rotation);
            gdp::core::Vec3 scale = part.scale;
            if (renderedRoom.rotation == gdp::core::QuarterTurn::Degrees90
                || renderedRoom.rotation == gdp::core::QuarterTurn::Degrees270) {
                std::swap(scale.x, scale.z);
            }
            drawSurface(roomMaterials, position, scale, part.material,
                part.uvScale, part.tint);
        }
    };
    drawRoomParts(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawRoomParts(connectedRoom);
    }

    fallbackTexture_.bind(gl_);
    for (const DoorRenderData& door : doors) {
        const bool facesX = std::abs(door.outwardNormal.x) > 0.5F;
        const gdp::core::Vec3 insetPosition
            = door.position - (door.outwardNormal * 0.12F);
        const auto drawDoorPiece = [&](const gdp::core::Vec3 position,
                                       const gdp::core::Vec3 scale,
                                       const gdp::core::Vec3 tint,
                                       const float emissive) {
            shader_.setMat4(gl_, "uModel", gdp::core::translationMatrix(position)
                    * gdp::core::scaleMatrix(scale));
            shader_.setVec3(gl_, "uTint", tint);
            shader_.setInt(gl_, "uDetailedMaterial", 0);
            shader_.setVec2(gl_, "uUvScale", {2.0F, 2.0F});
            shader_.setVec2(gl_, "uUvOffset", {});
            shader_.setFloat(gl_, "uEmissiveStrength", emissive);
            box_.draw(gl_);
        };
        constexpr float frameThickness = 0.14F;
        const gdp::core::Vec3 frameColor = door.locked
            ? gdp::core::Vec3{0.90F, 0.10F, 0.05F}
            : gdp::core::Vec3{0.10F, 0.66F, 0.72F};
        const gdp::core::Vec3 transverse = facesX
            ? gdp::core::Vec3{0.0F, 0.0F, 1.0F}
            : gdp::core::Vec3{1.0F, 0.0F, 0.0F};
        const gdp::core::Vec3 postScale = facesX
            ? gdp::core::Vec3{0.12F, door.height, frameThickness}
            : gdp::core::Vec3{frameThickness, door.height, 0.12F};
        const float postOffset = (door.width + frameThickness) * 0.5F;
        const gdp::core::Vec3 postCenter
            = insetPosition + gdp::core::Vec3{0.0F, door.height * 0.5F, 0.0F};
        drawDoorPiece(postCenter + (transverse * postOffset),
            postScale, frameColor, 0.28F);
        drawDoorPiece(postCenter - (transverse * postOffset),
            postScale, frameColor, 0.28F);
        const gdp::core::Vec3 lintelScale = facesX
            ? gdp::core::Vec3{0.12F, frameThickness,
                door.width + (frameThickness * 2.0F)}
            : gdp::core::Vec3{door.width + (frameThickness * 2.0F),
                frameThickness, 0.12F};
        drawDoorPiece(insetPosition
                + gdp::core::Vec3{0.0F, door.height + (frameThickness * 0.5F), 0.0F},
            lintelScale, frameColor, 0.28F);

        const float openFraction = std::isfinite(door.openFraction)
            ? std::clamp(door.openFraction, 0.0F, 1.0F) : 0.0F;
        const float panelWidth = std::max(0.05F, (door.width * 0.5F) - 0.02F);
        const float panelOffset = (panelWidth * 0.5F) + (panelWidth * openFraction);
        const gdp::core::Vec3 panelScale = facesX
            ? gdp::core::Vec3{0.10F, door.height - 0.10F, panelWidth}
            : gdp::core::Vec3{panelWidth, door.height - 0.10F, 0.10F};
        const gdp::core::Vec3 panelCenter
            = insetPosition + gdp::core::Vec3{0.0F, door.height * 0.5F, 0.0F};
        const gdp::core::Vec3 panelColor = door.locked
            ? gdp::core::Vec3{0.72F, 0.08F, 0.04F}
            : gdp::core::Vec3{0.22F, 0.30F, 0.34F};
        const float panelEmissive = door.locked ? 0.48F : 0.08F;
        drawDoorPiece(panelCenter + (transverse * panelOffset),
            panelScale, panelColor, panelEmissive);
        drawDoorPiece(panelCenter - (transverse * panelOffset),
            panelScale, panelColor, panelEmissive);
    }

    ceilingFixtureMaterial_.albedo.bind(gl_, 0U);
    ceilingFixtureMaterial_.normal.bind(gl_, 1U);
    ceilingFixtureMaterial_.properties.bind(gl_, 2U);
    ceilingFixtureMaterial_.emission.bind(gl_, 3U);
    shader_.setInt(gl_, "uDetailedMaterial", 1);
    shader_.setInt(gl_, "uUseEmissionMap", 1);
    constexpr gdp::core::Vec3 fixtureAlertColor{0.84F, 0.29F, 0.23F};
    for (std::size_t index = 0; index < lightCount; ++index) {
        const PointLightRenderData& light = lighting.lights[index];
        const gdp::core::Vec3 fixtureColor = lighting.alert
            ? fixtureAlertColor : light.color;
        const float lensEmission = lighting.alert
            ? lighting.fixtureEmissive * 1.35F : lighting.fixtureEmissive;
        const gdp::core::Vec3 lensTint = lensEmission > 0.01F
            ? fixtureColor : gdp::core::Vec3{0.42F, 0.44F, 0.43F};
        const auto drawFixturePiece = [this, &light](
            const gdp::core::Vec3 offset,
            const gdp::core::Vec3 scale,
            const gdp::core::Vec2 uvScale,
            const gdp::core::Vec2 uvOffset,
            const gdp::core::Vec3 tint,
            const float emissiveStrength) {
            shader_.setMat4(gl_, "uModel",
                gdp::core::translationMatrix(light.position + offset)
                    * gdp::core::scaleMatrix(scale));
            shader_.setVec3(gl_, "uTint", tint);
            shader_.setVec2(gl_, "uUvScale", uvScale);
            shader_.setVec2(gl_, "uUvOffset", uvOffset);
            shader_.setFloat(gl_, "uEmissiveStrength", emissiveStrength);
            box_.draw(gl_);
        };
        constexpr gdp::core::Vec3 neutralTint{1.0F, 1.0F, 1.0F};
        drawFixturePiece({0.0F, 0.095F, 0.0F}, {1.20F, 0.12F, 0.28F},
            {0.46F, 0.96F}, {0.02F, 0.02F}, neutralTint, 0.0F);
        drawFixturePiece({0.0F, 0.015F, 0.0F}, {1.17F, 0.055F, 0.255F},
            {0.14F, 0.96F}, {0.52F, 0.02F}, neutralTint, 0.0F);
        drawFixturePiece({0.0F, -0.022F, 0.0F}, {1.10F, 0.040F, 0.215F},
            {0.05F, 0.96F}, {0.69F, 0.02F}, neutralTint, 0.0F);
        drawFixturePiece({0.0F, -0.052F, 0.0F}, {1.03F, 0.030F, 0.170F},
            {0.21F, 0.96F}, {0.77F, 0.02F}, lensTint, lensEmission);

        constexpr std::array fastenerOffsets{
            gdp::core::Vec3{-0.525F, -0.041F, -0.105F},
            gdp::core::Vec3{0.525F, -0.041F, -0.105F},
            gdp::core::Vec3{-0.525F, -0.041F, 0.105F},
            gdp::core::Vec3{0.525F, -0.041F, 0.105F},
        };
        for (const gdp::core::Vec3 fastenerOffset : fastenerOffsets) {
            drawFixturePiece(fastenerOffset, {0.045F, 0.018F, 0.045F},
                {0.14F, 0.96F}, {0.52F, 0.02F}, neutralTint, 0.0F);
        }
    }
    shader_.setInt(gl_, "uUseEmissionMap", 0);
    shader_.setInt(gl_, "uDetailedMaterial", 0);
    shader_.setFloat(gl_, "uEmissiveStrength", 0.0F);

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
            gl_, "uTint", !enemy.alive
                ? gdp::core::Vec3{0.30F, 0.24F, 0.20F}
                : enemy.active
                ? gdp::core::Vec3{1.0F, 1.0F, 1.0F}
                : gdp::core::Vec3{0.68F, 0.72F, 0.78F});
        shader_.setInt(gl_, "uDetailedMaterial", 0);
        shader_.setVec2(gl_, "uUvScale", {4.0F, 4.0F});
        shader_.setVec2(gl_, "uUvOffset", {});
        shader_.setFloat(gl_, "uEmissiveStrength",
            enemy.alive
                ? visual->second.emissiveStrength * (enemy.active ? 1.0F : 0.25F)
                : 0.0F);
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
        shader_.setInt(gl_, "uDetailedMaterial", 0);
        shader_.setVec2(gl_, "uUvScale", {2.0F, 2.0F});
        shader_.setVec2(gl_, "uUvOffset", {});
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
        shader_.setInt(gl_, "uDetailedMaterial", 0);
        shader_.setVec2(gl_, "uUvScale", {3.0F, 3.0F});
        shader_.setVec2(gl_, "uUvOffset", {});
        shader_.setFloat(gl_, "uEmissiveStrength", 0.0F);
        box_.draw(gl_);
    }

    framebuffer_.presentToDefault(gl_, width, height);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    const int hudScale = width >= 640 && height >= 360 ? 2 : 1;
    const int hudMargin = 6 * hudScale;
    const int hudPadding = 4 * hudScale;
    const int hudLineHeight = 9 * hudScale;
    const gdp::core::Vec3 panelColor{0.018F, 0.032F, 0.040F};
    const gdp::core::Vec3 textColor{0.76F, 0.86F, 0.90F};
    const gdp::core::Vec3 cyanColor{0.22F, 0.84F, 0.90F};

    if (hud.mainMenu) {
        const MainMenuLayout layout = makeMainMenuLayout(width, height);
        const int scale = layout.scale;
        drawSolidRect(0, 0, width, height, {0.006F, 0.012F, 0.016F});
        drawSolidRect(layout.panel.x, layout.panel.y,
            layout.panel.width, layout.panel.height, panelColor);
        drawSolidRect(layout.panel.x,
            layout.panel.y + layout.panel.height - (2 * scale),
            layout.panel.width, 2 * scale, cyanColor);

        constexpr std::string_view title = "GDP";
        constexpr std::string_view subtitle = "GENERATED DUNGEON PROTOCOL";
        drawBitmapText((width - bitmapTextWidth(title, 3 * scale)) / 2,
            layout.panel.y + layout.panel.height - (26 * scale),
            3 * scale, title, cyanColor);
        drawBitmapText((width - bitmapTextWidth(subtitle, 1)) / 2,
            layout.panel.y + layout.panel.height - (38 * scale),
            1, subtitle, {0.48F, 0.62F, 0.66F});

        constexpr std::array<std::string_view, 2> options{
            "START RUN", "EXIT GAME",
        };
        const std::array<UiRect, 2> buttons{
            layout.startButton, layout.exitButton,
        };
        const std::size_t selected
            = hud.mainMenuSelection == HudMainMenuSelection::ExitGame ? 1U : 0U;
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            const UiRect button = buttons[index];
            if (index == selected) {
                drawSolidRect(button.x, button.y, button.width, button.height,
                    {0.06F, 0.18F, 0.21F});
                drawSolidRect(button.x, button.y, 2 * scale,
                    button.height, cyanColor);
            }
            const int textWidth = bitmapTextWidth(options[index], scale);
            drawBitmapText((width - textWidth) / 2,
                button.y + (3 * scale), scale, options[index],
                index == selected ? cyanColor : textColor);
        }

        const std::string_view controls = width >= 480
            ? "W S SELECT  ENTER CONFIRM  MOUSE SELECT"
            : "W S  ENTER  MOUSE";
        drawBitmapText((width - bitmapTextWidth(controls, 1)) / 2,
            layout.panel.y + (7 * scale), 1, controls,
            {0.48F, 0.62F, 0.66F});

        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::ostringstream message;
            message << "OpenGL main menu render error: 0x" << std::hex << error;
            gdp::core::log(gdp::core::LogLevel::Error, message.str());
            return false;
        }
        return true;
    }

    if (hud.visible) {
        const float damageStrength = std::clamp(hud.damageFeedback, 0.0F, 1.0F);
        if (!hud.paused && damageStrength > 0.0F) {
            const int edgeThickness = std::max(1,
                static_cast<int>((2.0F + (4.0F * damageStrength))
                    * static_cast<float>(hudScale)));
            const gdp::core::Vec3 damageColor{
                0.16F + (0.78F * damageStrength),
                0.012F + (0.045F * damageStrength),
                0.008F,
            };
            drawSolidRect(0, 0, width, edgeThickness, damageColor);
            drawSolidRect(0, height - edgeThickness, width, edgeThickness, damageColor);
            drawSolidRect(0, edgeThickness, edgeThickness,
                height - (edgeThickness * 2), damageColor);
            drawSolidRect(width - edgeThickness, edgeThickness, edgeThickness,
                height - (edgeThickness * 2), damageColor);
        }

        const int health = static_cast<int>(std::round(std::max(hud.health, 0.0F)));
        const int maximumHealth = static_cast<int>(std::round(
            std::max(hud.maximumHealth, 1.0F)));
        std::ostringstream healthStream;
        healthStream << "HP " << health << '/' << maximumHealth;
        std::ostringstream ammoStream;
        ammoStream << "AMMO " << hud.magazineAmmo << '/' << hud.reserveAmmo;
        std::ostringstream keysStream;
        keysStream << "KEYS " << hud.keys;
        const std::string healthText = healthStream.str();
        const std::string ammoText = ammoStream.str();
        const std::string keysText = keysStream.str();
        const int statusTextWidth = std::max({
            bitmapTextWidth(healthText, hudScale),
            bitmapTextWidth(ammoText, hudScale),
            bitmapTextWidth(keysText, hudScale),
        });
        const int statusWidth = statusTextWidth + (hudPadding * 2);
        const int statusHeight = (hudPadding * 2) + (hudLineHeight * 3)
            + (4 * hudScale);
        const int statusX = hudMargin;
        const int statusY = hudMargin;
        drawSolidRect(statusX, statusY, statusWidth, statusHeight, panelColor);
        drawSolidRect(statusX, statusY, 2 * hudScale, statusHeight, cyanColor);
        const int statusTextX = statusX + hudPadding;
        const int healthTextY = statusY + statusHeight - hudPadding - (7 * hudScale);
        const float healthRatio = std::clamp(
            std::max(hud.health, 0.0F) / std::max(hud.maximumHealth, 1.0F),
            0.0F, 1.0F);
        const int barWidth = statusWidth - (hudPadding * 2);
        const int healthBarY = healthTextY - (5 * hudScale);
        drawBitmapText(statusTextX, healthTextY, hudScale, healthText,
            healthRatio <= 0.25F
                ? gdp::core::Vec3{0.98F, 0.20F, 0.12F} : textColor);
        drawSolidRect(statusTextX, healthBarY, barWidth, 3 * hudScale,
            {0.10F, 0.14F, 0.15F});
        drawSolidRect(statusTextX, healthBarY,
            static_cast<int>(static_cast<float>(barWidth) * healthRatio),
            3 * hudScale,
            healthRatio <= 0.25F
                ? gdp::core::Vec3{0.94F, 0.16F, 0.10F}
                : gdp::core::Vec3{0.24F, 0.88F, 0.52F});
        drawBitmapText(statusTextX, healthTextY - hudLineHeight,
            hudScale, ammoText,
            hud.magazineAmmo == 0U
                ? gdp::core::Vec3{0.98F, 0.42F, 0.14F} : textColor);
        drawBitmapText(statusTextX, healthTextY - (hudLineHeight * 2),
            hudScale, keysText, cyanColor);

        std::ostringstream levelStream;
        levelStream << "LV " << hud.level;
        std::ostringstream experienceStream;
        experienceStream << "XP " << hud.experience;
        if (hud.experienceForNextLevel > 0U) {
            experienceStream << '/' << hud.experienceForNextLevel;
        } else {
            experienceStream << " MAX";
        }
        const std::string levelText = levelStream.str();
        const std::string experienceText = experienceStream.str();
        const int progressWidth = std::max(
            bitmapTextWidth(levelText, hudScale),
            bitmapTextWidth(experienceText, hudScale)) + (hudPadding * 2);
        const int progressHeight = (hudPadding * 2) + (hudLineHeight * 2)
            + (4 * hudScale);
        const int progressX = std::max(hudMargin, width - hudMargin - progressWidth);
        const int progressY = hudMargin;
        drawSolidRect(progressX, progressY, progressWidth, progressHeight, panelColor);
        drawSolidRect(progressX + progressWidth - (2 * hudScale), progressY,
            2 * hudScale, progressHeight, cyanColor);
        const int progressTextX = progressX + hudPadding;
        const int levelTextY = progressY + progressHeight - hudPadding - (7 * hudScale);
        drawBitmapText(progressTextX, levelTextY, hudScale, levelText, cyanColor);
        drawBitmapText(progressTextX, levelTextY - hudLineHeight,
            hudScale, experienceText, textColor);
        const float experienceRatio = hud.experienceForNextLevel == 0U
            ? 1.0F
            : std::clamp(
                static_cast<float>(hud.experience)
                    / static_cast<float>(hud.experienceForNextLevel),
                0.0F, 1.0F);
        const int experienceBarWidth = progressWidth - (hudPadding * 2);
        const int experienceBarY = progressY + (2 * hudScale);
        drawSolidRect(progressTextX, experienceBarY, experienceBarWidth,
            2 * hudScale, {0.10F, 0.14F, 0.15F});
        drawSolidRect(progressTextX, experienceBarY,
            static_cast<int>(static_cast<float>(experienceBarWidth) * experienceRatio),
            2 * hudScale, cyanColor);

        std::string_view eventText;
        gdp::core::Vec3 eventColor{0.48F, 0.62F, 0.66F};
        if (hud.eventLocked) {
            eventText = "LOCKDOWN";
            eventColor = {0.98F, 0.18F, 0.10F};
        } else if (hud.event == HudEventState::Dormant) {
            eventText = "EVENT STANDBY";
        } else if (hud.event == HudEventState::Active) {
            eventText = "EVENT ACTIVE";
            eventColor = {0.98F, 0.62F, 0.14F};
        } else if (hud.event == HudEventState::Completed) {
            eventText = "EVENT CLEAR";
            eventColor = {0.28F, 0.92F, 0.56F};
        }
        if (!eventText.empty()) {
            const int eventWidth = bitmapTextWidth(eventText, hudScale) + (hudPadding * 2);
            const int eventHeight = (7 * hudScale) + (hudPadding * 2);
            const int eventX = std::max(hudMargin, width - hudMargin - eventWidth);
            const int eventY = std::max(hudMargin,
                height - hudMargin - eventHeight - (13 * hudScale));
            drawSolidRect(eventX, eventY, eventWidth, eventHeight, panelColor);
            drawBitmapText(eventX + hudPadding, eventY + hudPadding,
                hudScale, eventText, eventColor);
        }

        std::string_view promptText;
        gdp::core::Vec3 promptColor{0.78F, 0.90F, 0.92F};
        if (hud.prompt == HudPrompt::OpenDoor) {
            promptText = "E OPEN DOOR";
        } else if (hud.prompt == HudPrompt::DoorLocked) {
            promptText = "DOOR LOCKED";
            promptColor = {0.98F, 0.24F, 0.12F};
        } else if (hud.prompt == HudPrompt::UseTerminal) {
            promptText = "E USE TERMINAL";
        } else if (hud.prompt == HudPrompt::Reload) {
            promptText = "R RELOAD";
            promptColor = {0.98F, 0.62F, 0.14F};
        }
        if (!hud.paused && !promptText.empty()) {
            const int promptWidth = bitmapTextWidth(promptText, hudScale)
                + (hudPadding * 2);
            const int promptHeight = (7 * hudScale) + (hudPadding * 2);
            const int promptX = (width - promptWidth) / 2;
            const int promptY = std::max(hudMargin, height / 5);
            drawSolidRect(promptX, promptY, promptWidth, promptHeight, panelColor);
            drawBitmapText(promptX + hudPadding, promptY + hudPadding,
                hudScale, promptText, promptColor);
        }

        const float hitStrength = std::clamp(hud.hitFeedbackStrength, 0.0F, 1.0F);
        if (!hud.paused && hud.hitFeedback != HudHitFeedback::None
            && hitStrength > 0.0F) {
            const bool killed = hud.hitFeedback == HudHitFeedback::Kill;
            const gdp::core::Vec3 baseColor = killed
                ? gdp::core::Vec3{1.0F, 0.26F, 0.10F}
                : gdp::core::Vec3{0.82F, 0.94F, 0.96F};
            const gdp::core::Vec3 markerColor = baseColor
                * (0.32F + (0.68F * hitStrength));
            const int centerX = width / 2;
            const int centerY = height / 2;
            const int gap = 10 * hudScale;
            const int markerLength = 5 * hudScale;
            const int markerThickness = std::max(1, hudScale);
            drawSolidRect(centerX - gap - markerLength,
                centerY - (markerThickness / 2), markerLength,
                markerThickness, markerColor);
            drawSolidRect(centerX + gap,
                centerY - (markerThickness / 2), markerLength,
                markerThickness, markerColor);
            drawSolidRect(centerX - (markerThickness / 2),
                centerY - gap - markerLength, markerThickness,
                markerLength, markerColor);
            drawSolidRect(centerX - (markerThickness / 2), centerY + gap,
                markerThickness, markerLength, markerColor);
        }

        std::string_view noticeText;
        gdp::core::Vec3 noticeColor{0.72F, 0.90F, 0.92F};
        if (hud.notice == HudNotice::Pickup) {
            noticeText = "SUPPLIES ACQUIRED";
            noticeColor = {0.28F, 0.92F, 0.56F};
        } else if (hud.notice == HudNotice::EventStart) {
            noticeText = "EVENT START";
            noticeColor = {0.98F, 0.58F, 0.14F};
        } else if (hud.notice == HudNotice::Wave) {
            noticeText = "NEW WAVE";
            noticeColor = {0.98F, 0.38F, 0.12F};
        } else if (hud.notice == HudNotice::EventClear) {
            noticeText = "SECTOR CLEAR";
            noticeColor = {0.28F, 0.92F, 0.56F};
        } else if (hud.notice == HudNotice::LevelUp) {
            noticeText = "LEVEL UP";
            noticeColor = {0.32F, 0.88F, 1.0F};
        }
        const float noticeStrength = std::clamp(
            hud.noticeFeedbackStrength, 0.0F, 1.0F);
        if (!hud.paused && !noticeText.empty() && noticeStrength > 0.0F) {
            const int noticeWidth = bitmapTextWidth(noticeText, hudScale)
                + (hudPadding * 2);
            const int noticeHeight = (7 * hudScale) + (hudPadding * 2);
            const int noticeX = (width - noticeWidth) / 2;
            const int noticeY = std::max(hudMargin,
                height - hudMargin - noticeHeight - (18 * hudScale));
            drawSolidRect(noticeX, noticeY, noticeWidth, noticeHeight, panelColor);
            drawBitmapText(noticeX + hudPadding, noticeY + hudPadding,
                hudScale, noticeText,
                noticeColor * (0.42F + (0.58F * noticeStrength)));
        }
    }

    std::string_view objectiveText;
    gdp::core::Vec3 objectiveColor{0.30F, 0.88F, 0.92F};
    if (hud.objective == ObjectiveRenderState::ReachBoss) {
        objectiveText = "REACH BOSS";
    } else if (hud.objective == ObjectiveRenderState::DefeatBoss) {
        objectiveText = "DEFEAT BOSS";
        objectiveColor = {0.96F, 0.56F, 0.18F};
    }
    if (!objectiveText.empty()) {
        const int textWidth = bitmapTextWidth(objectiveText, hudScale);
        drawBitmapText(std::max(hudMargin, width - textWidth - hudMargin),
            std::max(hudMargin, height - hudMargin - (7 * hudScale)),
            hudScale, objectiveText, objectiveColor);
    }
    if (hud.objective == ObjectiveRenderState::Victory
        || hud.objective == ObjectiveRenderState::Defeat) {
        const bool victory = hud.objective == ObjectiveRenderState::Victory;
        const std::string_view result = victory ? "VICTORY" : "DEFEAT";
        const std::string_view prompt = victory ? "ENTER NEW RUN" : "ENTER RETRY";
        const int bannerWidth = std::max(1, std::min(width - 16, 260));
        const int bannerHeight = std::max(1, std::min(height - 16, 72));
        const int bannerX = (width - bannerWidth) / 2;
        const int bannerY = (height - bannerHeight) / 2;
        glClearColor(0.015F, 0.025F, 0.030F, 1.0F);
        glScissor(bannerX, bannerY, bannerWidth, bannerHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        const gdp::core::Vec3 resultColor = victory
            ? gdp::core::Vec3{0.22F, 0.92F, 0.62F}
            : gdp::core::Vec3{0.96F, 0.18F, 0.10F};
        const int resultWidth = bitmapTextWidth(result, hudScale);
        const int promptWidth = bitmapTextWidth(prompt, 1);
        drawBitmapText((width - resultWidth) / 2, bannerY + 42,
            hudScale, result, resultColor);
        drawBitmapText((width - promptWidth) / 2, bannerY + 16,
            1, prompt, {0.76F, 0.84F, 0.88F});
    }
    if (hud.paused) {
        const int menuWidth = std::max(1, std::min(width - 16, 180 * hudScale));
        const int menuHeight = std::max(1, std::min(height - 16, 95 * hudScale));
        const int menuX = (width - menuWidth) / 2;
        const int menuY = (height - menuHeight) / 2;
        drawSolidRect(menuX, menuY, menuWidth, menuHeight,
            {0.012F, 0.024F, 0.030F});
        drawSolidRect(menuX, menuY + menuHeight - (2 * hudScale),
            menuWidth, 2 * hudScale, cyanColor);
        constexpr std::string_view pauseTitle = "PAUSED";
        const int titleWidth = bitmapTextWidth(pauseTitle, hudScale);
        drawBitmapText((width - titleWidth) / 2,
            menuY + menuHeight - (14 * hudScale), hudScale,
            pauseTitle, cyanColor);

        constexpr std::array<std::string_view, 3> options{
            "RESUME", "RESTART RUN", "EXIT GAME",
        };
        const std::size_t selected = hud.pauseSelection == HudPauseSelection::RestartRun
            ? 1U : hud.pauseSelection == HudPauseSelection::ExitGame ? 2U : 0U;
        const int firstOptionY = menuY + menuHeight - (34 * hudScale);
        for (std::size_t index = 0; index < options.size(); ++index) {
            const int optionWidth = bitmapTextWidth(options[index], hudScale);
            const int optionY = firstOptionY
                - (static_cast<int>(index) * 14 * hudScale);
            if (index == selected) {
                drawSolidRect((width - optionWidth) / 2 - (4 * hudScale),
                    optionY - (2 * hudScale), optionWidth + (8 * hudScale),
                    11 * hudScale, {0.06F, 0.18F, 0.21F});
            }
            drawBitmapText((width - optionWidth) / 2, optionY,
                hudScale, options[index],
                index == selected ? cyanColor : textColor);
        }
        const std::string_view controls = width >= 480
            ? "W S SELECT  ENTER CONFIRM  ESC RESUME"
            : "W S  ENTER  ESC";
        const int controlsWidth = bitmapTextWidth(controls, 1);
        drawBitmapText((width - controlsWidth) / 2, menuY + (6 * hudScale),
            1, controls, {0.48F, 0.62F, 0.66F});
    } else {
        glClearColor(0.78F, 0.88F, 0.90F, 1.0F);
        glScissor((width / 2) - 7, (height / 2) - 1, 15, 3);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor((width / 2) - 1, (height / 2) - 7, 3, 15);
        glClear(GL_COLOR_BUFFER_BIT);
    }
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

} // namespace gdp::game::visual
