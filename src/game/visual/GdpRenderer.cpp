#include "game/visual/GdpRenderer.hpp"

#include "core/Hash.hpp"
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
#include <numbers>
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
layout(location = 4) in mat4 aInstanceModel;

uniform mat4 uModel;
uniform int uInstanced;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vUv;

void main()
{
    mat4 model = uInstanced != 0 ? aInstanceModel : uModel;
    vec4 worldPosition = model * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vNormal = normalize(mat3(model) * aNormal);
    vUv = aUv;
    gl_Position = uProjection * uView * worldPosition;
}
)glsl";

constexpr std::string_view fragmentShaderSource = R"glsl(#version 330 core
#ifndef GDP_LOW_SPEC
#define GDP_LOW_SPEC 0
#endif

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUv;

uniform sampler2D uAlbedo;
uniform sampler2D uNormalMap;
uniform sampler2D uMaterialMap;
uniform sampler2D uEmissionMap;
uniform vec3 uTint;
uniform vec3 uCameraPosition;
#if GDP_LOW_SPEC
const int MAX_LIGHTS = 4;
#else
const int MAX_LIGHTS = 8;
#endif
#ifndef GDP_MAX_SHADOW_OCCLUDERS
#define GDP_MAX_SHADOW_OCCLUDERS 8
#endif
uniform int uLightCount;
uniform vec3 uLightPositions[MAX_LIGHTS];
uniform vec3 uLightColors[MAX_LIGHTS];
uniform float uLightIntensities[MAX_LIGHTS];
uniform float uLightRanges[MAX_LIGHTS];
const int MAX_SHADOW_OCCLUDERS = GDP_MAX_SHADOW_OCCLUDERS;
#if !GDP_LOW_SPEC
uniform int uShadowLightCount;
uniform int uShadowOccluderCount;
uniform vec3 uShadowOccluderMinima[MAX_SHADOW_OCCLUDERS];
uniform vec3 uShadowOccluderMaxima[MAX_SHADOW_OCCLUDERS];
#endif
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;
uniform vec2 uUvScale;
uniform vec2 uUvOffset;
uniform float uEmissiveStrength;
uniform int uDetailedMaterial;
uniform int uDetailedMaterialsEnabled;
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

#if !GDP_LOW_SPEC
bool segmentIntersectsShadowBox(
    vec3 origin,
    vec3 direction,
    float maximumDistance,
    vec3 boundsMinimum,
    vec3 boundsMaximum)
{
    vec3 safeDirection = vec3(
        abs(direction.x) < 0.0001 ? (direction.x < 0.0 ? -0.0001 : 0.0001)
                                  : direction.x,
        abs(direction.y) < 0.0001 ? (direction.y < 0.0 ? -0.0001 : 0.0001)
                                  : direction.y,
        abs(direction.z) < 0.0001 ? (direction.z < 0.0 ? -0.0001 : 0.0001)
                                  : direction.z);
    vec3 first = (boundsMinimum - origin) / safeDirection;
    vec3 second = (boundsMaximum - origin) / safeDirection;
    vec3 nearAxes = min(first, second);
    vec3 farAxes = max(first, second);
    float nearDistance = max(max(nearAxes.x, nearAxes.y), nearAxes.z);
    float farDistance = min(min(farAxes.x, farAxes.y), farAxes.z);
    return farDistance >= max(nearDistance, 0.0)
        && nearDistance > 0.025
        && nearDistance < maximumDistance - 0.025;
}

float directLightVisibility(
    vec3 position,
    vec3 normal,
    vec3 lightPosition,
    int lightIndex)
{
    if (lightIndex >= uShadowLightCount || uShadowOccluderCount <= 0) {
        return 1.0;
    }
    vec3 offset = lightPosition - position;
    float maximumDistance = length(offset);
    if (maximumDistance <= 0.05) {
        return 1.0;
    }
    vec3 origin = position + (normal * 0.035);
    vec3 direction = offset / maximumDistance;
    for (int index = 0; index < MAX_SHADOW_OCCLUDERS; ++index) {
        if (index >= uShadowOccluderCount) {
            break;
        }
        if (segmentIntersectsShadowBox(origin, direction, maximumDistance,
                uShadowOccluderMinima[index], uShadowOccluderMaxima[index])) {
            return 0.16;
        }
    }
    return 1.0;
}
#endif

void main()
{
    vec2 materialUv = (vUv * uUvScale) + uUvOffset;
    vec3 normal = normalize(vNormal);
    vec3 albedo = texture(uAlbedo, materialUv).rgb * uTint;
    float roughness = 0.72;
    float metalness = 0.0;
    float ambientOcclusion = 1.0;
#if !GDP_LOW_SPEC
    if (uDetailedMaterial != 0 && uDetailedMaterialsEnabled != 0) {
        vec3 tangentNormal = texture(uNormalMap, materialUv).xyz * 2.0 - 1.0;
        normal = normalize(cotangentFrame(normal, vWorldPosition, materialUv) * tangentNormal);
        vec3 material = texture(uMaterialMap, materialUv).rgb;
        roughness = clamp(material.r, 0.08, 1.0);
        metalness = clamp(material.g, 0.0, 1.0);
        ambientOcclusion = clamp(material.b, 0.0, 1.0);
    }
#endif
    vec3 result = albedo * uAmbientColor * uAmbientIntensity * ambientOcclusion;
#if !GDP_LOW_SPEC
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
#endif
    for (int index = 0; index < MAX_LIGHTS; ++index) {
        if (index >= uLightCount) {
            break;
        }
        if (uLightIntensities[index] <= 0.0001) {
            continue;
        }
        vec3 toLight = uLightPositions[index] - vWorldPosition;
        float distanceToLight = length(toLight);
        vec3 lightDirection = normalize(toLight);
        float diffuse = max(dot(normal, lightDirection), 0.0);
        float normalizedDistance = distanceToLight / max(uLightRanges[index], 0.001);
        float attenuation = 1.0 / (1.0 + 2.2 * normalizedDistance * normalizedDistance);
        vec3 radiance = uLightColors[index] * attenuation
            * uLightIntensities[index] * 1.9;
        float visibility = 1.0;
#if !GDP_LOW_SPEC
        visibility = diffuse > 0.0
            ? directLightVisibility(
                vWorldPosition, normal, uLightPositions[index], index)
            : 1.0;
#endif
        result += albedo * (1.0 - metalness) * diffuse * radiance * visibility;
#if !GDP_LOW_SPEC
        if (uDetailedMaterial != 0 && uDetailedMaterialsEnabled != 0
            && diffuse > 0.0) {
            vec3 halfway = normalize(lightDirection + viewDirection);
            float exponent = mix(96.0, 8.0, roughness);
            float highlight = pow(max(dot(normal, halfway), 0.0), exponent)
                * mix(0.90, 0.12, roughness);
            vec3 reflectance = mix(vec3(0.035), albedo, metalness);
            result += reflectance * highlight * diffuse * radiance * visibility;
        }
#endif
    }
    float emissiveMask = uUseEmissionMap != 0
        ? texture(uEmissionMap, materialUv).r
        : smoothstep(0.62, 0.82, max(albedo.r, max(albedo.g, albedo.b)));
    fragmentColor = vec4(result + (albedo * emissiveMask * uEmissiveStrength), 1.0);
}
)glsl";

std::string makeLowSpecFragmentShaderSource()
{
    std::string source{fragmentShaderSource};
    constexpr std::string_view versionLine{"#version 330 core\n"};
    source.insert(versionLine.size(), "#define GDP_LOW_SPEC 1\n");
    return source;
}

std::string makeShadowBudgetFragmentShaderSource(const std::uint32_t capacity)
{
    std::string source{fragmentShaderSource};
    constexpr std::string_view versionLine{"#version 330 core\n"};
    source.insert(versionLine.size(),
        "#define GDP_MAX_SHADOW_OCCLUDERS " + std::to_string(capacity) + "\n");
    return source;
}

enum class RoomMaterialChannel : std::uint8_t {
    Wall,
    Floor,
    Ceiling,
    Generic,
};

struct RoomPart {
    wkc::core::Vec3 position;
    wkc::core::Vec3 scale;
    wkc::core::Vec3 tint;
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
    case '.': return {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x06U, 0x06U};
    case '%': return {0x11U, 0x02U, 0x04U, 0x08U, 0x11U, 0x00U, 0x00U};
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
    const wkc::core::Vec3 color)
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
    const wkc::core::Vec3 color)
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

wkc::core::Mat4 basisMatrix(
    const wkc::core::Vec3 right,
    const wkc::core::Vec3 up,
    const wkc::core::Vec3 forward) noexcept
{
    wkc::core::Mat4 result = wkc::core::Mat4::identity();
    result.elements[0] = right.x;
    result.elements[1] = right.y;
    result.elements[2] = right.z;
    result.elements[4] = up.x;
    result.elements[5] = up.y;
    result.elements[6] = up.z;
    result.elements[8] = forward.x;
    result.elements[9] = forward.y;
    result.elements[10] = forward.z;
    return result;
}

class ScopedGpuTimer {
public:
    explicit ScopedGpuTimer(wkc::render::GpuTimer& timer) noexcept
        : timer_(timer)
        , active_(timer_.beginFrame())
    {
    }

    ~ScopedGpuTimer()
    {
        if (active_) {
            timer_.endFrame();
        }
    }

    ScopedGpuTimer(const ScopedGpuTimer&) = delete;
    ScopedGpuTimer& operator=(const ScopedGpuTimer&) = delete;

private:
    wkc::render::GpuTimer& timer_;
    bool active_{false};
};

std::string openGlString(const GLenum name)
{
    const GLubyte* const value = glGetString(name);
    return value == nullptr
        ? std::string{"unknown"}
        : std::string{reinterpret_cast<const char*>(value)};
}

} // namespace

bool GdpRenderer::initialize(const int width, const int height)
{
    if (!loadGlApi(gl_)) {
        return false;
    }
    gpuVendor_ = openGlString(GL_VENDOR);
    gpuRenderer_ = openGlString(GL_RENDERER);
    gpuVersion_ = openGlString(GL_VERSION);
    if (!shader_.compile(gl_, vertexShaderSource, fragmentShaderSource)
        || !shadow3Shader_.compile(gl_, vertexShaderSource,
            makeShadowBudgetFragmentShaderSource(3U))
        || !shadow6Shader_.compile(gl_, vertexShaderSource,
            makeShadowBudgetFragmentShaderSource(6U))
        || !lowSpecShader_.compile(
            gl_, vertexShaderSource, makeLowSpecFragmentShaderSource())) {
        return false;
    }
    if (!box_.upload(gl_, wkc::render::makeBoxMesh())) {
        return false;
    }
    if (!fallbackTexture_.upload(gl_, gdp::game::makeIndustrialCheckerImage())) {
        return false;
    }
    const gdp::game::CeilingFixtureMaterialImages fixtureImages
        = gdp::game::makeCeilingFixtureMaterialImages();
    if (!ceilingFixtureMaterial_.albedo.upload(
            gl_, fixtureImages.albedo, wkc::render::TextureSampling::Trilinear)
        || !ceilingFixtureMaterial_.normal.upload(
            gl_, fixtureImages.normal, wkc::render::TextureSampling::Trilinear)
        || !ceilingFixtureMaterial_.properties.upload(
            gl_, fixtureImages.properties, wkc::render::TextureSampling::Trilinear)
        || !ceilingFixtureMaterial_.emission.upload(
            gl_, fixtureImages.emission, wkc::render::TextureSampling::Trilinear)) {
        return false;
    }
    if (!framebuffer_.resize(gl_, width, height)) {
        return false;
    }
    if (!gpuTimer_.initialize(gl_)) {
        wkc::core::log(wkc::core::LogLevel::Warning,
            "OpenGL GPU timer queries unavailable; diagnostics will use CPU timings");
    }

    camera_.setPerspective(68.0F, 0.05F, 100.0F);
    camera_.lookAt({0.0F, 1.7F, 6.5F}, {0.0F, 1.5F, -1.5F});
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    initialized_ = true;
    if (!prepareIndustrialRoomMaterials(0) || !prepareWorkerVisual(0)
        || !prepareGuardVisual(0) || !prepareOfficerVisual(0)
        || !prepareEngineerVisual(0) || !prepareIndustrialRobotVisual(0)) {
        initialized_ = false;
        return false;
    }
    wkc::core::log(wkc::core::LogLevel::Info,
        "OpenGL device: vendor=" + gpuVendor_
            + ", renderer=" + gpuRenderer_ + ", version=" + gpuVersion_);
    wkc::core::log(wkc::core::LogLevel::Info,
        "Renderer initialized with fixed-room scene");
    return true;
}

bool GdpRenderer::prepareIndustrialRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const IndustrialRoomMaterialSet materials
        = generateIndustrialRoomMaterials(roomSeed);
    if (!validateIndustrialRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Industrial room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    const std::size_t conditionIndex = static_cast<std::size_t>(materials.condition);
    auto& sharedWall = industrialWallMaterials_[conditionIndex];
    if (!sharedWall) {
        const IndustrialSurfaceRecipe archetype
            = makeIndustrialWallArchetypeRecipe(materials.condition);
        const auto images = generateIndustrialWallMaterialImages(archetype);
        if (!images) {
            return false;
        }
        IndustrialWallGpuMaterial wallMaterial;
        if (!wallMaterial.albedo.upload(
                gl_, images->albedo, wkc::render::TextureSampling::Trilinear)
            || !wallMaterial.normal.upload(
                gl_, images->normal, wkc::render::TextureSampling::Trilinear)
            || !wallMaterial.properties.upload(
                gl_, images->properties, wkc::render::TextureSampling::Trilinear)) {
            return false;
        }
        sharedWall.emplace(std::move(wallMaterial));
    }

    auto& sharedCeiling = industrialCeilingMaterials_[conditionIndex];
    if (!sharedCeiling) {
        const IndustrialSurfaceRecipe archetype
            = makeIndustrialCeilingArchetypeRecipe(materials.condition);
        const auto images = generateIndustrialCeilingMaterialImages(
            archetype);
        if (!images) {
            return false;
        }
        IndustrialWallGpuMaterial ceilingMaterial;
        if (!ceilingMaterial.albedo.upload(
                gl_, images->albedo, wkc::render::TextureSampling::Trilinear)
            || !ceilingMaterial.normal.upload(
                gl_, images->normal, wkc::render::TextureSampling::Trilinear)
            || !ceilingMaterial.properties.upload(
                gl_, images->properties, wkc::render::TextureSampling::Trilinear)) {
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
        IndustrialSurface::Wall)];
    gpuMaterials.wallTone = std::clamp(wallRecipe.baseColor.x / 0.33F, 0.94F, 1.06F);
    const auto& ceilingRecipe = materials.surfaces[static_cast<std::size_t>(
        IndustrialSurface::Ceiling)];
    gpuMaterials.ceilingTone = std::clamp(
        ceilingRecipe.baseColor.x / 0.38F, 0.94F, 1.06F);
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const IndustrialSurfaceRecipe& recipe = materials.surfaces[index];
        if (recipe.surface == IndustrialSurface::Wall
            || recipe.surface == IndustrialSurface::Ceiling) {
            continue;
        }
        const auto textureData = textureCache_.acquire(
            industrialSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(
                    *generateIndustrialSurfaceAlbedo(recipe));
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

bool GdpRenderer::prepareStorageRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const StorageRoomMaterialSet materials
        = generateStorageRoomMaterials(roomSeed);
    if (!validateStorageRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Storage room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const StorageSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            storageSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateStorageSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareCombatRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const CombatRoomMaterialSet materials
        = generateCombatRoomMaterials(roomSeed);
    if (!validateCombatRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Combat room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const CombatSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            combatSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateCombatSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareCafeteriaRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const CafeteriaRoomMaterialSet materials
        = generateCafeteriaRoomMaterials(roomSeed);
    if (!validateCafeteriaRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Cafeteria room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const CafeteriaSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            cafeteriaSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateCafeteriaSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareLaboratoryRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const LaboratoryRoomMaterialSet materials
        = generateLaboratoryRoomMaterials(roomSeed);
    if (!validateLaboratoryRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Laboratory room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const LaboratorySurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            laboratorySurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateLaboratorySurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareTechnicalRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const TechnicalRoomMaterialSet materials
        = generateTechnicalRoomMaterials(roomSeed);
    if (!validateTechnicalRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Technical room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const TechnicalSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            technicalSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateTechnicalSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareSecurityRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const SecurityRoomMaterialSet materials
        = generateSecurityRoomMaterials(roomSeed);
    if (!validateSecurityRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Security room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const SecuritySurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            securitySurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateSecuritySurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareCentralHubRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const CentralHubRoomMaterialSet materials
        = generateCentralHubRoomMaterials(roomSeed);
    if (!validateCentralHubRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Central Hub material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const CentralHubSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            centralHubSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateCentralHubSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareWorkerVisual(const wkc::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (workerVisuals_.contains(visualSeed)) {
        return true;
    }

    const WorkerVisual visual
        = generateWorkerVisual(visualSeed);
    if (!validateWorkerVisual(visual).isValid()) {
        wkc::core::log(wkc::core::LogLevel::Error, "Worker visual generation failed validation");
        return false;
    }
    EnemyGpuVisual gpuVisual;
    if (!uploadEnemyGpuVisual(
            visual.meshRecipe, visual.textureRecipe, gpuVisual)) {
        return false;
    }
    workerVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool GdpRenderer::prepareGuardVisual(const wkc::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (guardVisuals_.contains(visualSeed)) {
        return true;
    }

    const GuardVisual visual
        = generateGuardVisual(visualSeed);
    if (!validateGuardVisual(visual).isValid()) {
        wkc::core::log(wkc::core::LogLevel::Error, "Guard visual generation failed validation");
        return false;
    }
    EnemyGpuVisual gpuVisual;
    if (!uploadEnemyGpuVisual(
            visual.meshRecipe, visual.textureRecipe, gpuVisual)) {
        return false;
    }
    guardVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool GdpRenderer::prepareOfficerVisual(const wkc::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (officerVisuals_.contains(visualSeed)) {
        return true;
    }

    const OfficerVisual visual
        = generateOfficerVisual(visualSeed);
    if (!validateOfficerVisual(visual).isValid()) {
        wkc::core::log(wkc::core::LogLevel::Error, "Officer visual generation failed validation");
        return false;
    }
    EnemyGpuVisual gpuVisual;
    if (!uploadEnemyGpuVisual(
            visual.meshRecipe, visual.textureRecipe, gpuVisual)) {
        return false;
    }
    officerVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool GdpRenderer::prepareEngineerVisual(const wkc::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (engineerVisuals_.contains(visualSeed)) {
        return true;
    }

    const EngineerVisual visual
        = generateEngineerVisual(visualSeed);
    if (!validateEngineerVisual(visual).isValid()) {
        wkc::core::log(wkc::core::LogLevel::Error,
            "Engineer visual generation failed validation");
        return false;
    }
    EnemyGpuVisual gpuVisual;
    if (!uploadEnemyGpuVisual(
            visual.meshRecipe, visual.textureRecipe, gpuVisual)) {
        return false;
    }
    engineerVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool GdpRenderer::prepareIndustrialRobotVisual(const wkc::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (industrialRobotVisuals_.contains(visualSeed)) {
        return true;
    }
    const IndustrialRobotVisual visual
        = generateIndustrialRobotVisual(visualSeed);
    if (!validateIndustrialRobotVisual(visual).isValid()) {
        wkc::core::log(wkc::core::LogLevel::Error,
            "IndustrialRobot visual generation failed validation");
        return false;
    }
    EnemyGpuVisual gpuVisual;
    if (!uploadEnemyGpuVisual(
            visual.meshRecipe, visual.textureRecipe, gpuVisual)) {
        return false;
    }
    industrialRobotVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool GdpRenderer::uploadEnemyGpuVisual(
    const wkc::procedural::MeshRecipe& meshRecipe,
    const wkc::procedural::TextureRecipe& textureRecipe,
    EnemyGpuVisual& gpuVisual)
{
    const std::vector<EnemyRigidPartRecipe> rigidParts
        = splitEnemyRigidParts(meshRecipe);
    if (rigidParts.empty()) {
        return false;
    }
    gpuVisual.parts.reserve(rigidParts.size());
    for (const EnemyRigidPartRecipe& rigidPart : rigidParts) {
        const auto meshData = meshCache_.acquire(
            wkc::procedural::meshRecipeHash(rigidPart.meshRecipe),
            [&rigidPart] {
                return std::move(
                    wkc::procedural::generateMesh(rigidPart.meshRecipe)->mesh);
            });
        EnemyGpuPart gpuPart;
        if (!gpuPart.mesh.upload(gl_, *meshData)) {
            return false;
        }
        gpuPart.pivot = rigidPart.pivot;
        gpuPart.index = rigidPart.index;
        gpuVisual.parts.push_back(std::move(gpuPart));
    }
    const auto textureData = textureCache_.acquire(
        wkc::procedural::textureRecipeHash(textureRecipe),
        [&textureRecipe] {
            return std::move(*wkc::procedural::generateAlbedo(textureRecipe));
        });
    if (!gpuVisual.texture.upload(gl_, *textureData)) {
        return false;
    }
    gpuVisual.emissiveStrength = textureRecipe.emissiveAmount;
    return true;
}

void GdpRenderer::drawMesh(const wkc::render::Mesh& mesh) noexcept
{
    if (!mesh.isValid()) {
        return;
    }
    const bool cullingEnabled = mesh.backfaceCullingSafe();
    if (backfaceCulling_.needsUpdate(cullingEnabled)) {
        if (cullingEnabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }
    if (cullingEnabled) {
        ++lastFrameStats_.backfaceCulledDraws;
    }
    mesh.draw(gl_, meshBindings_);
    ++lastFrameStats_.drawCalls;
    lastFrameStats_.triangles += mesh.indexCount() / 3U;
}

void GdpRenderer::drawMeshInstanced(const wkc::render::Mesh& mesh) noexcept
{
    if (!mesh.isValid() || mesh.instanceCount() == 0U) {
        return;
    }
    const bool cullingEnabled = mesh.backfaceCullingSafe();
    if (backfaceCulling_.needsUpdate(cullingEnabled)) {
        if (cullingEnabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }
    if (cullingEnabled) {
        ++lastFrameStats_.backfaceCulledDraws;
    }
    mesh.drawInstanced(gl_, meshBindings_);
    ++lastFrameStats_.drawCalls;
    ++lastFrameStats_.instancedDrawCalls;
    lastFrameStats_.renderedInstances += mesh.instanceCount();
    lastFrameStats_.avoidedDrawCalls += mesh.instanceCount() - 1U;
    lastFrameStats_.triangles += (mesh.indexCount() / 3U) * mesh.instanceCount();
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
    const HudRenderData hud,
    const std::span<const ImpactRenderData> impacts,
    const WeaponViewmodelRenderData weaponViewmodel)
{
    lastFrameStats_ = {};
    if (!initialized_ || width <= 0 || height <= 0) {
        return false;
    }
    const int renderScalePercent = std::clamp(
        static_cast<int>(hud.worldRenderScalePercent), 50, 100);
    const int renderWidth = std::max(1, (width * renderScalePercent) / 100);
    const int renderHeight = std::max(1, (height * renderScalePercent) / 100);
    const std::uint32_t lightCount = std::min<std::uint32_t>(
        lighting.lightCount, static_cast<std::uint32_t>(maxRenderedRoomLights));
    const std::uint32_t shadowLightCount = std::min({
        lighting.shadowLightCount,
        lightCount,
        static_cast<std::uint32_t>(maxShadowCastingLights)});
    const std::uint32_t shadowOccluderCount = std::min({
        lighting.shadowOccluderCount,
        static_cast<std::uint32_t>(maxActiveShadowOccluders),
        static_cast<std::uint32_t>(maxShadowOccluders)});
    activeShaderPath_ = selectRendererShaderPath(
        hud.detailedMaterials, lightCount,
        shadowLightCount, shadowOccluderCount);
    if (!shadowShaderSpecializationEnabled_
        && activeShaderPath_ != RendererShaderPath::LowSpec) {
        activeShaderPath_ = RendererShaderPath::FullShadow8;
    }
    lowSpecShaderActive_ = activeShaderPath_ == RendererShaderPath::LowSpec;
    lastFrameStats_.lowSpecShader = lowSpecShaderActive_;
    lastFrameStats_.shadowShaderOccluderCapacity
        = shadowShaderOccluderCapacity(activeShaderPath_);
    wkc::render::Shader& shader = activeShader();
    shader.beginUniformFrame();
    lastFrameStats_.meshCacheEntries = meshCache_.stats().entries;
    lastFrameStats_.textureCacheEntries = textureCache_.stats().entries;
    if (!framebuffer_.resize(gl_, renderWidth, renderHeight)) {
        return false;
    }
    textureBindings_.beginFrame();
    meshBindings_.beginFrame();
    backfaceCulling_.beginFrame();
    const ScopedGpuTimer gpuFrameTimer{gpuTimer_};
    if (const std::optional<double> gpuMilliseconds
        = gpuTimer_.latestMilliseconds()) {
        lastFrameStats_.gpuMilliseconds = *gpuMilliseconds;
        lastFrameStats_.gpuTimingAvailable = true;
    }

    framebuffer_.bind(gl_);
    glViewport(0, 0, renderWidth, renderHeight);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.018F, 0.025F, 0.03F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.use(gl_);
    camera_.setPerspective(camera.verticalFieldOfViewDegrees, 0.05F, 100.0F);
    camera_.lookAt(camera.position, camera.position + camera.forward);
    shader.setInt(gl_, "uAlbedo", 0);
    shader.setInt(gl_, "uNormalMap", 1);
    shader.setInt(gl_, "uMaterialMap", 2);
    shader.setInt(gl_, "uEmissionMap", 3);
    shader.setInt(gl_, "uUseEmissionMap", 0);
    shader.setInt(gl_, "uInstanced", 0);
    shader.setInt(gl_, "uDetailedMaterialsEnabled",
        hud.detailedMaterials ? 1 : 0);
    shader.setVec3(gl_, "uCameraPosition", camera.position);
    shader.setMat4(gl_, "uView", camera_.viewMatrix());
    shader.setMat4(
        gl_,
        "uProjection",
        camera_.projectionMatrix(
            static_cast<float>(renderWidth) / static_cast<float>(renderHeight)));
    const wkc::core::Vec3 alertColor{1.0F, 0.12F, 0.06F};
    const wkc::core::Vec3 ambientColor = lighting.alert
        ? (lighting.ambientColor * 0.28F) + (alertColor * 0.72F)
        : lighting.ambientColor;
    shader.setInt(gl_, "uLightCount", static_cast<int>(lightCount));
    shader.setVec3(gl_, "uAmbientColor", ambientColor);
    shader.setFloat(gl_, "uAmbientIntensity",
        lighting.alert ? std::max(lighting.ambientIntensity, 0.16F)
                       : lighting.ambientIntensity);
    if (!lowSpecShaderActive_) {
        shader.setInt(gl_, "uShadowLightCount",
            static_cast<int>(shadowLightCount));
        shader.setInt(gl_, "uShadowOccluderCount",
            static_cast<int>(shadowOccluderCount));
    }
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
    constexpr std::array shadowMinimumUniforms{
        "uShadowOccluderMinima[0]", "uShadowOccluderMinima[1]",
        "uShadowOccluderMinima[2]", "uShadowOccluderMinima[3]",
        "uShadowOccluderMinima[4]", "uShadowOccluderMinima[5]",
        "uShadowOccluderMinima[6]", "uShadowOccluderMinima[7]",
        "uShadowOccluderMinima[8]", "uShadowOccluderMinima[9]",
        "uShadowOccluderMinima[10]", "uShadowOccluderMinima[11]",
        "uShadowOccluderMinima[12]", "uShadowOccluderMinima[13]",
        "uShadowOccluderMinima[14]", "uShadowOccluderMinima[15]",
    };
    constexpr std::array shadowMaximumUniforms{
        "uShadowOccluderMaxima[0]", "uShadowOccluderMaxima[1]",
        "uShadowOccluderMaxima[2]", "uShadowOccluderMaxima[3]",
        "uShadowOccluderMaxima[4]", "uShadowOccluderMaxima[5]",
        "uShadowOccluderMaxima[6]", "uShadowOccluderMaxima[7]",
        "uShadowOccluderMaxima[8]", "uShadowOccluderMaxima[9]",
        "uShadowOccluderMaxima[10]", "uShadowOccluderMaxima[11]",
        "uShadowOccluderMaxima[12]", "uShadowOccluderMaxima[13]",
        "uShadowOccluderMaxima[14]", "uShadowOccluderMaxima[15]",
    };
    for (std::size_t index = 0; index < lightCount; ++index) {
        const PointLightRenderData& light = lighting.lights[index];
        const wkc::core::Vec3 lightColor = lighting.alert
            ? (light.color * 0.24F) + (alertColor * 0.76F)
            : light.color;
        shader.setVec3(gl_, positionUniforms[index], light.position);
        shader.setVec3(gl_, colorUniforms[index], lightColor);
        shader.setFloat(gl_, intensityUniforms[index],
            lighting.alert ? light.intensity * 1.08F : light.intensity);
        shader.setFloat(gl_, rangeUniforms[index], light.range);
    }
    for (std::size_t index = 0; index < shadowOccluderCount; ++index) {
        shader.setVec3(gl_, shadowMinimumUniforms[index],
            lighting.shadowOccluders[index].bounds.minimum);
        shader.setVec3(gl_, shadowMaximumUniforms[index],
            lighting.shadowOccluders[index].bounds.maximum);
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

    struct SurfaceStyle {
        const RoomGpuMaterials* roomMaterials{nullptr};
        RoomMaterialChannel material{RoomMaterialChannel::Generic};
        wkc::core::Vec3 tint{1.0F, 1.0F, 1.0F};
        wkc::core::Vec2 uvScale{1.0F, 1.0F};
        wkc::core::Vec2 uvOffset{};
        bool detailedMaterial{false};

        constexpr bool operator==(const SurfaceStyle&) const noexcept = default;
    };
    struct SurfaceCommand {
        SurfaceStyle style{};
        wkc::core::Mat4 model{wkc::core::Mat4::identity()};
    };
    std::vector<SurfaceCommand> surfaceCommands;
    const auto queueSurface = [this, &surfaceCommands](
        const RoomGpuMaterials& roomMaterials,
        const wkc::core::Vec3 position,
        const wkc::core::Vec3 scale,
        const RoomMaterialChannel material,
        const float uvScale = 1.0F,
        const wkc::core::Vec3 tint = {1.0F, 1.0F, 1.0F},
        const wkc::core::Vec2 detailedUvScale = {},
        const wkc::core::Vec2 detailedUvOffset = {}) {
        SurfaceStyle style;
        style.roomMaterials = &roomMaterials;
        style.material = material;
        bool detailedMaterial = false;
        wkc::core::Vec3 resolvedTint = tint;
        wkc::core::Vec2 materialUvOffset{};
        switch (material) {
        case RoomMaterialChannel::Wall:
            if (roomMaterials.industrialWallCondition) {
                const std::size_t conditionIndex = static_cast<std::size_t>(
                    *roomMaterials.industrialWallCondition);
                const auto& wallMaterial = industrialWallMaterials_[conditionIndex];
                if (wallMaterial) {
                    detailedMaterial = true;
                    resolvedTint = tint * roomMaterials.wallTone;
                    materialUvOffset = roomMaterials.wallUvOffset;
                }
            }
            break;
        case RoomMaterialChannel::Floor:
            break;
        case RoomMaterialChannel::Ceiling:
            if (roomMaterials.industrialCeilingCondition) {
                const std::size_t conditionIndex = static_cast<std::size_t>(
                    *roomMaterials.industrialCeilingCondition);
                const auto& ceilingMaterial = industrialCeilingMaterials_[conditionIndex];
                if (ceilingMaterial) {
                    detailedMaterial = true;
                    resolvedTint = tint * roomMaterials.ceilingTone;
                    materialUvOffset = roomMaterials.ceilingUvOffset;
                }
            }
            break;
        case RoomMaterialChannel::Generic:
            break;
        }
        const bool customUv = detailedMaterial
            && detailedUvScale.x > 0.0F && detailedUvScale.y > 0.0F;
        style.tint = resolvedTint;
        style.detailedMaterial = detailedMaterial;
        style.uvScale = customUv
            ? detailedUvScale : wkc::core::Vec2{uvScale, uvScale};
        style.uvOffset = customUv
            ? detailedUvOffset + materialUvOffset : wkc::core::Vec2{};
        surfaceCommands.push_back({
            style,
            wkc::core::translationMatrix(position)
                * wkc::core::scaleMatrix(scale),
        });
    };

    const auto drawRoomShell = [&](const RoomRenderData renderedRoom) {
        const RoomGpuMaterials& roomMaterials = resolveMaterials(renderedRoom.materialKey);
        const wkc::core::Aabb bounds = renderedRoom.worldBounds;
        const float roomWidth = bounds.maximum.x - bounds.minimum.x;
        const float roomDepth = bounds.maximum.z - bounds.minimum.z;
        const float roomHeight = bounds.maximum.y - bounds.minimum.y;
        const wkc::core::Vec3 roomCenter{
            (bounds.minimum.x + bounds.maximum.x) * 0.5F,
            bounds.minimum.y,
            (bounds.minimum.z + bounds.maximum.z) * 0.5F,
        };
        constexpr float shellThickness = 0.20F;
        queueSurface(roomMaterials,
            roomCenter + wkc::core::Vec3{0.0F, -shellThickness * 0.5F, 0.0F},
            {roomWidth + shellThickness, shellThickness,
                roomDepth + shellThickness},
            RoomMaterialChannel::Floor, 4.0F);
        queueSurface(roomMaterials,
            roomCenter
                + wkc::core::Vec3{0.0F, roomHeight + shellThickness * 0.5F, 0.0F},
            {roomWidth + shellThickness, shellThickness,
                roomDepth + shellThickness},
            RoomMaterialChannel::Ceiling, 1.0F, {1.0F, 1.0F, 1.0F},
            {roomWidth / 4.0F, roomDepth / 4.0F},
            {bounds.minimum.x / 4.0F, bounds.minimum.z / 4.0F});

        const auto drawWall = [&](const wkc::core::Vec3 normal) {
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
                    return wkc::core::dot(normal, candidate.outwardNormal) > 0.90F
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
                const wkc::core::Vec3 position = facesX
                    ? wkc::core::Vec3{wallCenter,
                        bounds.minimum.y + ((bottom + top) * 0.5F),
                        transverseCenter}
                    : wkc::core::Vec3{transverseCenter,
                        bounds.minimum.y + ((bottom + top) * 0.5F),
                        wallCenter};
                const wkc::core::Vec3 scale = facesX
                    ? wkc::core::Vec3{
                        shellThickness, top - bottom, maximum - minimum}
                    : wkc::core::Vec3{
                        maximum - minimum, top - bottom, shellThickness};
                queueSurface(roomMaterials, position, scale, RoomMaterialChannel::Wall,
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
            const wkc::core::Vec3 incoming = wkc::core::normalized(
                corridor.points[index] - corridor.points[index - 1U]);
            const wkc::core::Vec3 outgoing = wkc::core::normalized(
                corridor.points[index + 1U] - corridor.points[index]);
            if (std::abs(wkc::core::dot(incoming, outgoing)) > 0.5F) {
                continue;
            }
            const wkc::core::Vec3 corner = corridor.points[index];
            const wkc::core::Vec3 cornerScale{
                corridor.width, corridorShellThickness, corridor.width};
            queueSurface(roomMaterials,
                corner + wkc::core::Vec3{
                    0.0F, -corridorShellThickness * 0.5F, 0.0F},
                cornerScale, RoomMaterialChannel::Floor, 2.0F);
            queueSurface(roomMaterials,
                corner + wkc::core::Vec3{
                    0.0F, corridor.height + corridorShellThickness * 0.5F, 0.0F},
                cornerScale, RoomMaterialChannel::Ceiling, 1.0F,
                {1.0F, 1.0F, 1.0F},
                {corridor.width / 4.0F, corridor.width / 4.0F},
                {corner.x / 4.0F, corner.z / 4.0F});
        }
        for (std::size_t index = 0; index + 1U < corridor.points.size(); ++index) {
            const wkc::core::Vec3 first = corridor.points[index];
            const wkc::core::Vec3 second = corridor.points[index + 1U];
            const bool alongX = wkc::core::nearlyEqual(first.z, second.z);
            const float length = alongX
                ? std::abs(second.x - first.x) : std::abs(second.z - first.z);
            if (length <= 1.0e-4F) {
                continue;
            }
            const wkc::core::Vec3 center = (first + second) * 0.5F;
            const wkc::core::Vec3 floorScale = alongX
                ? wkc::core::Vec3{length, corridorShellThickness, corridor.width}
                : wkc::core::Vec3{corridor.width, corridorShellThickness, length};
            queueSurface(roomMaterials,
                center + wkc::core::Vec3{0.0F, -corridorShellThickness * 0.5F, 0.0F},
                floorScale, RoomMaterialChannel::Floor, 2.0F);
            queueSurface(roomMaterials, center + wkc::core::Vec3{
                    0.0F, corridor.height + corridorShellThickness * 0.5F, 0.0F},
                floorScale, RoomMaterialChannel::Ceiling, 1.0F,
                {1.0F, 1.0F, 1.0F},
                {floorScale.x / 4.0F, floorScale.z / 4.0F},
                {(alongX ? std::min(first.x, second.x) : center.x) / 4.0F,
                    (alongX ? center.z : std::min(first.z, second.z)) / 4.0F});
            const wkc::core::Vec3 direction = wkc::core::normalized(second - first);
            const wkc::core::Vec3 left{-direction.z, 0.0F, direction.x};
            for (const float sideSign : {-1.0F, 1.0F}) {
                const wkc::core::Vec3 side = left * sideSign;
                wkc::core::Vec3 wallStart = first;
                wkc::core::Vec3 wallEnd = second;
                if (index > 0U) {
                    const wkc::core::Vec3 previousDirection = wkc::core::normalized(
                        first - corridor.points[index - 1U]);
                    if (std::abs(wkc::core::dot(previousDirection, direction)) < 0.5F) {
                        const bool inner = wkc::core::dot(
                            side, previousDirection * -1.0F) > 0.5F;
                        wallStart = wallStart
                            + direction * (inner ? corridorHalfWidth : -corridorHalfWidth);
                    }
                }
                if (index + 2U < corridor.points.size()) {
                    const wkc::core::Vec3 nextDirection = wkc::core::normalized(
                        corridor.points[index + 2U] - second);
                    if (std::abs(wkc::core::dot(direction, nextDirection)) < 0.5F) {
                        const bool inner = wkc::core::dot(side, nextDirection) > 0.5F;
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
                const wkc::core::Vec3 wallScale = alongX
                    ? wkc::core::Vec3{
                        wallLength, corridor.height, corridorShellThickness}
                    : wkc::core::Vec3{
                        corridorShellThickness, corridor.height, wallLength};
                const wkc::core::Vec3 wallCenter = (wallStart + wallEnd) * 0.5F
                    + side * corridorHalfWidth
                    + wkc::core::Vec3{0.0F, corridor.height * 0.5F, 0.0F};
                queueSurface(roomMaterials, wallCenter,
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
            const wkc::core::Vec3 position = renderedRoom.origin
                + wkc::core::rotateQuarterTurns(part.position, renderedRoom.rotation);
            wkc::core::Vec3 scale = part.scale;
            if (renderedRoom.rotation == wkc::core::QuarterTurn::Degrees90
                || renderedRoom.rotation == wkc::core::QuarterTurn::Degrees270) {
                std::swap(scale.x, scale.z);
            }
            queueSurface(roomMaterials, position, scale, part.material,
                part.uvScale, part.tint);
        }
    };
    drawRoomParts(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawRoomParts(connectedRoom);
    }

    const auto bindSurfaceStyle = [this, &shader](const SurfaceStyle& style) {
        if (style.roomMaterials == nullptr) {
            return false;
        }
        const RoomGpuMaterials& roomMaterials = *style.roomMaterials;
        switch (style.material) {
        case RoomMaterialChannel::Wall:
            if (style.detailedMaterial) {
                if (!roomMaterials.industrialWallCondition) {
                    return false;
                }
                const auto& material = industrialWallMaterials_[
                    static_cast<std::size_t>(
                        *roomMaterials.industrialWallCondition)];
                if (!material) {
                    return false;
                }
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else {
                roomMaterials.textures[static_cast<std::size_t>(
                    IndustrialSurface::Wall)].bind(gl_, textureBindings_);
            }
            break;
        case RoomMaterialChannel::Floor:
            roomMaterials.textures[static_cast<std::size_t>(
                IndustrialSurface::Floor)].bind(gl_, textureBindings_);
            break;
        case RoomMaterialChannel::Ceiling:
            if (style.detailedMaterial) {
                if (!roomMaterials.industrialCeilingCondition) {
                    return false;
                }
                const auto& material = industrialCeilingMaterials_[
                    static_cast<std::size_t>(
                        *roomMaterials.industrialCeilingCondition)];
                if (!material) {
                    return false;
                }
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else {
                roomMaterials.textures[static_cast<std::size_t>(
                    IndustrialSurface::Ceiling)].bind(gl_, textureBindings_);
            }
            break;
        case RoomMaterialChannel::Generic:
            fallbackTexture_.bind(gl_, textureBindings_);
            break;
        }
        shader.setVec3(gl_, "uTint", style.tint);
        shader.setInt(gl_, "uDetailedMaterial",
            style.detailedMaterial ? 1 : 0);
        shader.setVec2(gl_, "uUvScale", style.uvScale);
        shader.setVec2(gl_, "uUvOffset", style.uvOffset);
        shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
        return true;
    };

    if (!surfaceInstancingEnabled_) {
        shader.setInt(gl_, "uInstanced", 0);
        for (const SurfaceCommand& command : surfaceCommands) {
            if (!bindSurfaceStyle(command.style)) {
                return false;
            }
            shader.setMat4(gl_, "uModel", command.model);
            drawMesh(box_);
        }
    } else {
        struct SurfaceBatch {
            SurfaceStyle style{};
            std::vector<wkc::core::Mat4> models;
        };
        std::vector<SurfaceBatch> surfaceBatches;
        surfaceBatches.reserve(surfaceCommands.size());
        for (const SurfaceCommand& command : surfaceCommands) {
            const auto batch = std::find_if(surfaceBatches.begin(),
                surfaceBatches.end(), [&command](const SurfaceBatch& candidate) {
                    return candidate.style == command.style;
                });
            if (batch == surfaceBatches.end()) {
                surfaceBatches.push_back({command.style, {command.model}});
            } else {
                batch->models.push_back(command.model);
            }
        }
        for (const SurfaceBatch& batch : surfaceBatches) {
            if (!bindSurfaceStyle(batch.style)) {
                return false;
            }
            if (batch.models.size() > 1U) {
                shader.setInt(gl_, "uInstanced", 1);
                if (!box_.uploadInstanceTransforms(
                        gl_, batch.models, meshBindings_)) {
                    return false;
                }
                drawMeshInstanced(box_);
            } else {
                shader.setInt(gl_, "uInstanced", 0);
                shader.setMat4(gl_, "uModel", batch.models.front());
                drawMesh(box_);
            }
        }
        shader.setInt(gl_, "uInstanced", 0);
    }

    fallbackTexture_.bind(gl_, textureBindings_);
    for (const DoorRenderData& door : doors) {
        const bool facesX = std::abs(door.outwardNormal.x) > 0.5F;
        const wkc::core::Vec3 insetPosition
            = door.position - (door.outwardNormal * 0.12F);
        const auto drawDoorPiece = [&](const wkc::core::Vec3 position,
                                       const wkc::core::Vec3 scale,
                                       const wkc::core::Vec3 tint,
                                       const float emissive) {
            shader.setMat4(gl_, "uModel", wkc::core::translationMatrix(position)
                    * wkc::core::scaleMatrix(scale));
            shader.setVec3(gl_, "uTint", tint);
            shader.setInt(gl_, "uDetailedMaterial", 0);
            shader.setVec2(gl_, "uUvScale", {2.0F, 2.0F});
            shader.setVec2(gl_, "uUvOffset", {});
            shader.setFloat(gl_, "uEmissiveStrength", emissive);
            drawMesh(box_);
        };
        constexpr float frameThickness = 0.14F;
        const wkc::core::Vec3 frameColor = door.locked
            ? wkc::core::Vec3{0.90F, 0.10F, 0.05F}
            : wkc::core::Vec3{0.10F, 0.66F, 0.72F};
        const wkc::core::Vec3 transverse = facesX
            ? wkc::core::Vec3{0.0F, 0.0F, 1.0F}
            : wkc::core::Vec3{1.0F, 0.0F, 0.0F};
        const wkc::core::Vec3 postScale = facesX
            ? wkc::core::Vec3{0.12F, door.height, frameThickness}
            : wkc::core::Vec3{frameThickness, door.height, 0.12F};
        const float postOffset = (door.width + frameThickness) * 0.5F;
        const wkc::core::Vec3 postCenter
            = insetPosition + wkc::core::Vec3{0.0F, door.height * 0.5F, 0.0F};
        drawDoorPiece(postCenter + (transverse * postOffset),
            postScale, frameColor, 0.28F);
        drawDoorPiece(postCenter - (transverse * postOffset),
            postScale, frameColor, 0.28F);
        const wkc::core::Vec3 lintelScale = facesX
            ? wkc::core::Vec3{0.12F, frameThickness,
                door.width + (frameThickness * 2.0F)}
            : wkc::core::Vec3{door.width + (frameThickness * 2.0F),
                frameThickness, 0.12F};
        drawDoorPiece(insetPosition
                + wkc::core::Vec3{0.0F, door.height + (frameThickness * 0.5F), 0.0F},
            lintelScale, frameColor, 0.28F);

        const float openFraction = std::isfinite(door.openFraction)
            ? std::clamp(door.openFraction, 0.0F, 1.0F) : 0.0F;
        const float panelWidth = std::max(0.05F, (door.width * 0.5F) - 0.02F);
        const float panelOffset = (panelWidth * 0.5F) + (panelWidth * openFraction);
        const wkc::core::Vec3 panelScale = facesX
            ? wkc::core::Vec3{0.10F, door.height - 0.10F, panelWidth}
            : wkc::core::Vec3{panelWidth, door.height - 0.10F, 0.10F};
        const wkc::core::Vec3 panelCenter
            = insetPosition + wkc::core::Vec3{0.0F, door.height * 0.5F, 0.0F};
        const wkc::core::Vec3 panelColor = door.locked
            ? wkc::core::Vec3{0.72F, 0.08F, 0.04F}
            : wkc::core::Vec3{0.22F, 0.30F, 0.34F};
        const float panelEmissive = door.locked ? 0.48F : 0.08F;
        drawDoorPiece(panelCenter + (transverse * panelOffset),
            panelScale, panelColor, panelEmissive);
        drawDoorPiece(panelCenter - (transverse * panelOffset),
            panelScale, panelColor, panelEmissive);
    }

    fallbackTexture_.bind(gl_, textureBindings_);
    shader.setInt(gl_, "uDetailedMaterial", 0);
    shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
    shader.setVec2(gl_, "uUvOffset", {});
    for (const ImpactRenderData& impact : impacts) {
        const float strength = std::clamp(
            std::isfinite(impact.strength) ? impact.strength : 0.0F,
            0.0F, 1.0F);
        if (strength <= 0.0F) {
            continue;
        }
        const wkc::core::Vec3 color
            = impactVisualColor(impact.material, strength);
        const float size = impactVisualSize(strength);
        const wkc::core::Vec3 normal = wkc::core::normalized(impact.normal);
        const wkc::core::Vec3 center
            = impact.position + (normal * 0.018F);
        shader.setMat4(gl_, "uModel", wkc::core::translationMatrix(center)
                * wkc::core::scaleMatrix({size, size, size}));
        shader.setVec3(gl_, "uTint", color);
        shader.setFloat(gl_, "uEmissiveStrength",
            impact.material == ImpactVisualMaterial::Metal ? 0.75F : 0.12F);
        drawMesh(box_);

        const std::size_t particleCount
            = impact.material == ImpactVisualMaterial::Metal ? 4U
            : impact.material == ImpactVisualMaterial::Composite ? 3U : 2U;
        constexpr float tau = 6.28318530717958647692F;
        for (std::size_t particle = 0U; particle < particleCount; ++particle) {
            const std::uint64_t phaseBits = wkc::core::mix64(
                impact.visualSeed + static_cast<std::uint64_t>(particle));
            const float phase = static_cast<float>(phaseBits & 0xffffU)
                / 65535.0F * tau;
            const float travel = (1.0F - strength)
                * (0.10F + (static_cast<float>(particle) * 0.035F));
            const wkc::core::Vec3 particleOffset{
                std::cos(phase) * travel,
                (0.04F + (0.02F * static_cast<float>(particle))) * strength,
                std::sin(phase) * travel,
            };
            const float particleSize = size * (0.34F + (strength * 0.18F));
            shader.setMat4(gl_, "uModel",
                wkc::core::translationMatrix(center + particleOffset)
                    * wkc::core::scaleMatrix(
                        {particleSize, particleSize, particleSize}));
            shader.setVec3(gl_, "uTint", color);
            drawMesh(box_);
        }
    }

    ceilingFixtureMaterial_.albedo.bind(gl_, textureBindings_, 0U);
    ceilingFixtureMaterial_.normal.bind(gl_, textureBindings_, 1U);
    ceilingFixtureMaterial_.properties.bind(gl_, textureBindings_, 2U);
    ceilingFixtureMaterial_.emission.bind(gl_, textureBindings_, 3U);
    shader.setInt(gl_, "uDetailedMaterial", 1);
    shader.setInt(gl_, "uUseEmissionMap", 1);
    constexpr wkc::core::Vec3 fixtureAlertColor{0.84F, 0.29F, 0.23F};
    for (std::size_t index = 0; index < lightCount; ++index) {
        const PointLightRenderData& light = lighting.lights[index];
        const wkc::core::Vec3 fixtureColor = lighting.alert
            ? fixtureAlertColor : light.color;
        const float baseLensEmission = lighting.alert
            ? lighting.fixtureEmissive * 1.35F : lighting.fixtureEmissive;
        const float lensEmission
            = baseLensEmission * light.fixtureEmissionScale;
        const wkc::core::Vec3 lensTint = lensEmission > 0.01F
            ? fixtureColor : wkc::core::Vec3{0.42F, 0.44F, 0.43F};
        const auto drawFixturePiece = [this, &light, &shader](
            const wkc::core::Vec3 offset,
            const wkc::core::Vec3 scale,
            const wkc::core::Vec2 uvScale,
            const wkc::core::Vec2 uvOffset,
            const wkc::core::Vec3 tint,
            const float emissiveStrength) {
            shader.setMat4(gl_, "uModel",
                wkc::core::translationMatrix(light.position + offset)
                    * wkc::core::scaleMatrix(scale));
            shader.setVec3(gl_, "uTint", tint);
            shader.setVec2(gl_, "uUvScale", uvScale);
            shader.setVec2(gl_, "uUvOffset", uvOffset);
            shader.setFloat(gl_, "uEmissiveStrength", emissiveStrength);
            drawMesh(box_);
        };
        const wkc::core::Vec3 housingTint
            = light.fixtureState == PropVisualState::Destroyed
            ? wkc::core::Vec3{0.30F, 0.32F, 0.31F}
            : light.fixtureState == PropVisualState::Damaged
                ? wkc::core::Vec3{0.70F, 0.56F, 0.40F}
                : wkc::core::Vec3{1.0F, 1.0F, 1.0F};
        drawFixturePiece({0.0F, 0.095F, 0.0F}, {1.20F, 0.12F, 0.28F},
            {0.46F, 0.96F}, {0.02F, 0.02F}, housingTint, 0.0F);
        drawFixturePiece({0.0F, 0.015F, 0.0F}, {1.17F, 0.055F, 0.255F},
            {0.14F, 0.96F}, {0.52F, 0.02F}, housingTint, 0.0F);
        drawFixturePiece({0.0F, -0.022F, 0.0F}, {1.10F, 0.040F, 0.215F},
            {0.05F, 0.96F}, {0.69F, 0.02F}, housingTint, 0.0F);
        drawFixturePiece({0.0F, -0.052F, 0.0F}, {1.03F, 0.030F, 0.170F},
            {0.21F, 0.96F}, {0.77F, 0.02F}, lensTint, lensEmission);

        constexpr std::array fastenerOffsets{
            wkc::core::Vec3{-0.525F, -0.041F, -0.105F},
            wkc::core::Vec3{0.525F, -0.041F, -0.105F},
            wkc::core::Vec3{-0.525F, -0.041F, 0.105F},
            wkc::core::Vec3{0.525F, -0.041F, 0.105F},
        };
        for (const wkc::core::Vec3 fastenerOffset : fastenerOffsets) {
            drawFixturePiece(fastenerOffset, {0.045F, 0.018F, 0.045F},
                {0.14F, 0.96F}, {0.52F, 0.02F}, housingTint, 0.0F);
        }
    }
    shader.setInt(gl_, "uUseEmissionMap", 0);
    shader.setInt(gl_, "uDetailedMaterial", 0);
    shader.setFloat(gl_, "uEmissiveStrength", 0.0F);

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
        visual->second.texture.bind(gl_, textureBindings_);
        const EnemyAnimationState currentAnimationState
            = enemy.alive ? enemy.animationState : EnemyAnimationState::Dead;
        const EnemyRootPose currentPose = sampleEnemyRootPose(
            currentAnimationState,
            enemy.animationTick, enemy.visualSeed, enemy.active, enemy.role);
        const EnemyRootPose previousPose = sampleEnemyRootPose(
            enemy.previousAnimationState,
            enemy.animationTick, enemy.visualSeed, enemy.active, enemy.role);
        const float transitionBlend = std::isfinite(enemy.animationTransitionBlend)
            ? std::clamp(enemy.animationTransitionBlend, 0.0F, 1.0F) : 1.0F;
        const EnemyRootPose pose = currentAnimationState == EnemyAnimationState::Dead
            ? sampleEnemyRagdollRootPose(
                previousPose, enemy.visualSeed, enemy.role, transitionBlend)
            : blendEnemyRootPoses(previousPose, currentPose, transitionBlend);
        const wkc::core::Vec3 position = enemy.position + pose.offset;
        constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0F;
        const wkc::core::Mat4 rootModel = wkc::core::translationMatrix(position)
            * wkc::core::rotationYMatrix(enemy.yawDegrees * degreesToRadians)
            * wkc::core::rotationZMatrix(
                pose.rotationDegrees.z * degreesToRadians)
            * wkc::core::rotationXMatrix(
                pose.rotationDegrees.x * degreesToRadians)
            * wkc::core::scaleMatrix(pose.scale);
        const bool hit = enemy.alive && enemy.active
            && enemy.animationState == EnemyAnimationState::Hit;
        const float hitStrength = std::isfinite(enemy.hitReactionStrength)
            ? std::clamp(enemy.hitReactionStrength, 0.0F, 1.0F) : 0.0F;
        const float visibleHitStrength
            = hit ? hitStrength * transitionBlend : hitStrength;
        const float deathBlend = !enemy.alive ? transitionBlend : 0.0F;
        shader.setVec3(
            gl_, "uTint", !enemy.alive
                ? wkc::core::Vec3{
                    1.0F - (deathBlend * 0.70F),
                    1.0F - (deathBlend * 0.76F),
                    1.0F - (deathBlend * 0.80F)}
                : hit
                ? wkc::core::Vec3{
                    1.0F, 1.0F - (visibleHitStrength * 0.28F),
                    1.0F - (visibleHitStrength * 0.32F)}
                : enemy.active
                ? wkc::core::Vec3{1.0F, 1.0F, 1.0F}
                : wkc::core::Vec3{0.68F, 0.72F, 0.78F});
        shader.setInt(gl_, "uDetailedMaterial", 0);
        shader.setVec2(gl_, "uUvScale", {4.0F, 4.0F});
        shader.setVec2(gl_, "uUvOffset", {});
        shader.setFloat(gl_, "uEmissiveStrength",
            enemy.alive
                ? visual->second.emissiveStrength
                    * (hit ? 1.0F + (visibleHitStrength * 0.15F)
                           : enemy.active ? 1.0F : 0.25F)
                : visual->second.emissiveStrength * (1.0F - deathBlend));
        for (const EnemyGpuPart& part : visual->second.parts) {
            const EnemyPartPose currentPartPose = sampleEnemyPartPose(
                currentAnimationState,
                enemy.animationTick, enemy.visualSeed, part.index,
                static_cast<std::uint32_t>(visual->second.parts.size()),
                hitStrength, enemy.active, enemy.role);
            const EnemyPartPose previousPartPose = sampleEnemyPartPose(
                enemy.previousAnimationState,
                enemy.animationTick, enemy.visualSeed, part.index,
                static_cast<std::uint32_t>(visual->second.parts.size()),
                hitStrength, enemy.active, enemy.role);
            const EnemyPartPose partPose
                = currentAnimationState == EnemyAnimationState::Dead
                ? sampleEnemyRagdollPartPose(
                    previousPartPose, enemy.visualSeed, enemy.role, part.index,
                    static_cast<std::uint32_t>(visual->second.parts.size()),
                    transitionBlend)
                : blendEnemyPartPoses(
                    previousPartPose, currentPartPose, transitionBlend);
            const wkc::core::Vec3 pivot = part.pivot + partPose.offset;
            const wkc::core::Mat4 partModel
                = wkc::core::translationMatrix(pivot)
                * wkc::core::rotationXMatrix(
                    partPose.rotationDegrees.x * degreesToRadians)
                * wkc::core::rotationYMatrix(
                    partPose.rotationDegrees.y * degreesToRadians)
                * wkc::core::rotationZMatrix(
                    partPose.rotationDegrees.z * degreesToRadians)
                * wkc::core::translationMatrix(
                    {-part.pivot.x, -part.pivot.y, -part.pivot.z});
            shader.setMat4(gl_, "uModel", rootModel * partModel);
            drawMesh(part.mesh);
        }
    }

    fallbackTexture_.bind(gl_, textureBindings_);
    for (const PickupRenderData& pickup : pickups) {
        wkc::core::Vec3 tint{0.20F, 0.85F, 0.32F};
        wkc::core::Vec3 scale{0.30F, 0.22F, 0.30F};
        if (pickup.role == PickupVisualRole::Ammo) {
            tint = {0.92F, 0.68F, 0.16F};
            scale = {0.34F, 0.18F, 0.24F};
        } else if (pickup.role == PickupVisualRole::Key) {
            tint = {0.18F, 0.78F, 0.92F};
            scale = {0.38F, 0.12F, 0.16F};
        } else if (pickup.role == PickupVisualRole::Armor) {
            tint = {0.18F, 0.42F, 0.96F};
            scale = {0.36F, 0.26F, 0.30F};
        } else if (pickup.role == PickupVisualRole::Weapon) {
            tint = {0.72F, 0.28F, 0.96F};
            scale = {0.62F, 0.16F, 0.20F};
        } else if (pickup.role == PickupVisualRole::Special) {
            tint = {1.0F, 0.24F, 0.52F};
            scale = {0.24F, 0.38F, 0.24F};
        }
        const wkc::core::Mat4 model
            = wkc::core::translationMatrix(
                pickup.position + wkc::core::Vec3{0.0F, scale.y * 0.5F, 0.0F})
            * wkc::core::scaleMatrix(scale);
        shader.setMat4(gl_, "uModel", model);
        shader.setVec3(gl_, "uTint", tint);
        shader.setInt(gl_, "uDetailedMaterial", 0);
        shader.setVec2(gl_, "uUvScale", {2.0F, 2.0F});
        shader.setVec2(gl_, "uUvOffset", {});
        shader.setFloat(gl_, "uEmissiveStrength", 0.16F);
        drawMesh(box_);
    }

    for (const PropRenderData& prop : props) {
        wkc::core::Vec3 tint{0.54F, 0.34F, 0.17F};
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
        wkc::core::Vec3 scale = prop.scale;
        if (prop.state == PropVisualState::Damaged) {
            tint = tint * 0.62F;
        } else if (prop.state == PropVisualState::Destroyed) {
            tint = tint * 0.30F;
            scale.y = std::min(scale.y, 0.12F);
            scale.x *= 1.12F;
            scale.z *= 1.12F;
        }
        const wkc::core::Mat4 model = wkc::core::translationMatrix(
            prop.position + wkc::core::Vec3{0.0F, scale.y * 0.5F, 0.0F})
            * wkc::core::scaleMatrix(scale);
        shader.setMat4(gl_, "uModel", model);
        shader.setVec3(gl_, "uTint", tint);
        shader.setInt(gl_, "uDetailedMaterial", 0);
        shader.setVec2(gl_, "uUvScale", {3.0F, 3.0F});
        shader.setVec2(gl_, "uUvOffset", {});
        shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
        drawMesh(box_);
    }

    if (weaponViewmodel.visible) {
        glClear(GL_DEPTH_BUFFER_BIT);
        fallbackTexture_.bind(gl_, textureBindings_);
        shader.setInt(gl_, "uDetailedMaterial", 0);
        shader.setInt(gl_, "uUseEmissionMap", 0);
        shader.setVec2(gl_, "uUvScale", {3.0F, 3.0F});
        shader.setVec2(gl_, "uUvOffset", {});

        const wkc::core::Vec3 forward = wkc::core::normalized(camera.forward);
        wkc::core::Vec3 right = wkc::core::normalized(
            wkc::core::cross(forward, {0.0F, 1.0F, 0.0F}));
        if (wkc::core::length(right) <= 1.0e-7F) {
            right = {1.0F, 0.0F, 0.0F};
        }
        const wkc::core::Vec3 up = wkc::core::normalized(
            wkc::core::cross(right, forward));
        const WeaponViewmodelPose pose
            = sampleWeaponViewmodelPose(weaponViewmodel);
        const wkc::core::Vec3 baseOffset{
            weaponViewmodel.role == HudWeaponRole::Carbine ? 0.27F : 0.32F,
            weaponViewmodel.role == HudWeaponRole::Carbine ? -0.27F : -0.25F,
            weaponViewmodel.role == HudWeaponRole::Carbine ? 0.72F : 0.62F,
        };
        const wkc::core::Vec3 localOffset = baseOffset + pose.offset;
        const wkc::core::Vec3 anchor = camera.position
            + (right * localOffset.x) + (up * localOffset.y)
            + (forward * localOffset.z);
        constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0F;
        const wkc::core::Mat4 root
            = wkc::core::translationMatrix(anchor)
            * basisMatrix(right, up, forward)
            * wkc::core::rotationZMatrix(
                pose.rotationDegrees.z * degreesToRadians)
            * wkc::core::rotationYMatrix(
                pose.rotationDegrees.y * degreesToRadians)
            * wkc::core::rotationXMatrix(
                pose.rotationDegrees.x * degreesToRadians);
        const float seedTone = static_cast<float>(
            (weaponViewmodel.visualSeed >> 9U) & 0x0fU) / 15.0F * 0.05F;
        const wkc::core::Vec3 metal{
            0.20F + seedTone, 0.25F + seedTone, 0.27F + seedTone};
        const wkc::core::Vec3 darkMetal{0.075F, 0.095F, 0.105F};
        const wkc::core::Vec3 grip{0.11F, 0.12F, 0.115F};
        const wkc::core::Vec3 accent = weaponViewmodel.overdrive
            ? wkc::core::Vec3{0.96F, 0.18F, 0.52F}
            : wkc::core::Vec3{0.12F, 0.72F, 0.80F};
        const auto drawWeaponPiece = [&](const wkc::core::Vec3 offset,
                                         const wkc::core::Vec3 scale,
                                         const wkc::core::Vec3 rotationDegrees,
                                         const wkc::core::Vec3 color,
                                         const float emissive = 0.0F) {
            const wkc::core::Mat4 model = root
                * wkc::core::translationMatrix(offset)
                * wkc::core::rotationZMatrix(rotationDegrees.z * degreesToRadians)
                * wkc::core::rotationYMatrix(rotationDegrees.y * degreesToRadians)
                * wkc::core::rotationXMatrix(rotationDegrees.x * degreesToRadians)
                * wkc::core::scaleMatrix(scale);
            shader.setMat4(gl_, "uModel", model);
            shader.setVec3(gl_, "uTint", color);
            shader.setFloat(gl_, "uEmissiveStrength", emissive);
            drawMesh(box_);
        };

        float muzzlePosition = 0.48F;
        if (weaponViewmodel.role == HudWeaponRole::Carbine) {
            drawWeaponPiece({}, {0.22F, 0.18F, 0.52F}, {}, metal);
            drawWeaponPiece({0.0F, 0.01F, 0.40F},
                {0.17F, 0.13F, 0.42F}, {}, darkMetal);
            drawWeaponPiece({0.0F, 0.0F, 0.70F},
                {0.055F, 0.055F, 0.34F}, {}, metal);
            drawWeaponPiece({0.0F, -0.01F, -0.35F},
                {0.19F, 0.16F, 0.30F}, {}, darkMetal);
            drawWeaponPiece({0.0F, -0.20F, -0.04F},
                {0.12F, 0.31F, 0.16F}, {-12.0F, 0.0F, 0.0F}, grip);
            drawWeaponPiece({0.0F, 0.14F, 0.02F},
                {0.07F, 0.09F, 0.14F}, {}, accent, 0.28F);
            drawWeaponPiece({0.0F, 0.02F, 0.30F},
                {0.235F, 0.035F, 0.18F}, {}, accent, 0.10F);
            muzzlePosition = 0.88F;
        } else {
            drawWeaponPiece({}, {0.16F, 0.14F, 0.43F}, {}, metal);
            drawWeaponPiece({0.0F, 0.075F, 0.04F},
                {0.145F, 0.085F, 0.36F}, {}, darkMetal);
            drawWeaponPiece({0.0F, -0.17F, -0.11F},
                {0.12F, 0.28F, 0.15F}, {-14.0F, 0.0F, 0.0F}, grip);
            drawWeaponPiece({0.0F, 0.0F, 0.31F},
                {0.07F, 0.07F, 0.20F}, {}, metal);
            drawWeaponPiece({0.0F, 0.13F, -0.02F},
                {0.035F, 0.045F, 0.08F}, {}, accent, 0.24F);
        }

        const float flash = std::clamp(
            std::isfinite(weaponViewmodel.muzzleFlashStrength)
                ? weaponViewmodel.muzzleFlashStrength : 0.0F,
            0.0F, 1.0F);
        if (flash > 0.0F) {
            const wkc::core::Vec3 flashColor{1.0F, 0.56F, 0.10F};
            drawWeaponPiece({0.0F, 0.0F, muzzlePosition + (0.07F * flash)},
                {0.07F + (0.08F * flash), 0.07F + (0.08F * flash),
                    0.18F + (0.12F * flash)}, {}, flashColor, 1.8F);
            drawWeaponPiece({0.0F, 0.0F, muzzlePosition + 0.04F},
                {0.20F * flash, 0.025F, 0.055F},
                {0.0F, 0.0F, 45.0F}, flashColor, 1.5F);
            drawWeaponPiece({0.0F, 0.0F, muzzlePosition + 0.04F},
                {0.20F * flash, 0.025F, 0.055F},
                {0.0F, 0.0F, -45.0F}, flashColor, 1.5F);
        }
        shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
    }

    framebuffer_.presentToDefault(gl_, width, height);
    glViewport(0, 0, width, height);
    if (backfaceCulling_.needsUpdate(false)) {
        glDisable(GL_CULL_FACE);
    }
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    const int hudScale = width >= 640 && height >= 360 ? 2 : 1;
    const int hudMargin = 6 * hudScale;
    const int hudPadding = 4 * hudScale;
    const int hudLineHeight = 9 * hudScale;
    const UiVisualTheme uiTheme = industrialUiTheme();
    const wkc::core::Vec3 panelColor = uiTheme.panel;
    const wkc::core::Vec3 textColor = uiTheme.text;
    const wkc::core::Vec3 cyanColor = uiTheme.accent;
    const auto drawIndustrialPanel = [&](const UiRect panel,
                                         const int scale,
                                         const wkc::core::Vec3 accent) {
        const int border = std::max(1, scale);
        drawSolidRect(panel.x + border, panel.y - border,
            panel.width, panel.height, {0.002F, 0.005F, 0.007F});
        drawSolidRect(panel.x, panel.y, panel.width, panel.height,
            uiTheme.panelBorder);
        drawSolidRect(panel.x + border, panel.y + border,
            std::max(0, panel.width - (border * 2)),
            std::max(0, panel.height - (border * 2)), panelColor);
        drawSolidRect(panel.x, panel.y + panel.height - (2 * border),
            panel.width, 2 * border, accent);
        const int corner = std::min(panel.height, 7 * scale);
        drawSolidRect(panel.x, panel.y, 2 * border, corner, accent);
        drawSolidRect(panel.x + panel.width - (2 * border), panel.y,
            2 * border, corner, accent);
    };
    const auto drawSelectedRow = [&](const UiRect row,
                                     const int scale,
                                     const wkc::core::Vec3 accent) {
        drawSolidRect(row.x, row.y, row.width, row.height, uiTheme.selection);
        drawSolidRect(row.x, row.y, 2 * scale, row.height, accent);
        drawSolidRect(row.x + row.width - (2 * scale), row.y,
            2 * scale, row.height, accent);
    };
    const auto drawExitConfirmation = [&]() {
        if (!hud.exitConfirmation) {
            return;
        }
        const ExitConfirmationLayout layout
            = makeExitConfirmationLayout(width, height);
        const int scale = layout.scale;
        const wkc::core::Vec3 dangerColor = uiTheme.danger;
        drawIndustrialPanel(layout.panel, scale, dangerColor);
        constexpr std::string_view title = "EXIT GAME?";
        drawBitmapText((width - bitmapTextWidth(title, 2 * scale)) / 2,
            layout.panel.y + layout.panel.height - (22 * scale),
            2 * scale, title, dangerColor);

        constexpr std::array<std::string_view, 2> options{
            "CANCEL", "EXIT",
        };
        const std::array<UiRect, 2> buttons{
            layout.cancelButton, layout.exitButton,
        };
        const std::size_t selected = hud.exitConfirmationSelection
                == HudExitConfirmationSelection::ExitGame
            ? 1U
            : 0U;
        for (std::size_t index = 0U; index < buttons.size(); ++index) {
            const UiRect button = buttons[index];
            if (index == selected) {
                drawSelectedRow(button, scale,
                    index == 0U ? cyanColor : dangerColor);
            }
            drawBitmapText(
                button.x
                    + ((button.width - bitmapTextWidth(options[index], scale)) / 2),
                button.y + (3 * scale), scale, options[index],
                index == selected
                    ? (index == 0U ? cyanColor : dangerColor)
                    : textColor);
        }
        constexpr std::string_view controls = "WASD  ARROWS  ENTER  ESC  MOUSE";
        drawBitmapText((width - bitmapTextWidth(controls, 1)) / 2,
            layout.panel.y + (5 * scale), 1, controls,
            uiTheme.mutedText);
    };

    if (hud.settingsMenu) {
        const SettingsMenuLayout layout = makeSettingsMenuLayout(width, height);
        const int scale = layout.scale;
        drawSolidRect(0, 0, width, height, uiTheme.background);
        drawIndustrialPanel(layout.panel, scale, cyanColor);
        constexpr std::string_view title = "SETTINGS";
        drawBitmapText((width - bitmapTextWidth(title, 2 * scale)) / 2,
            layout.panel.y + layout.panel.height - (22 * scale),
            2 * scale, title, cyanColor);

        constexpr std::array<std::string_view, 12> labels{
            "MASTER", "EFFECTS", "AMBIENT", "UI VOLUME",
            "SENSITIVITY", "INVERT Y", "FOV", "QUALITY", "RESOLUTION",
            "FULLSCREEN", "VSYNC", "BACK",
        };
        const std::array<std::uint16_t, 12> numbers{
            hud.masterVolumePercent,
            hud.effectsVolumePercent,
            hud.ambientVolumePercent,
            hud.uiVolumePercent,
            hud.mouseSensitivityPercent,
            0U,
            hud.fieldOfViewDegrees,
            0U,
            0U,
            0U,
            0U,
            0U,
        };
        for (std::size_t index = 0U; index < layout.rows.size(); ++index) {
            const UiRect row = layout.rows[index];
            const bool selected = index
                == static_cast<std::size_t>(hud.settingsSelection);
            if (selected) {
                drawSelectedRow(row, scale, cyanColor);
            }
            if (index == static_cast<std::size_t>(HudSettingsSelection::Back)) {
                drawBitmapText(
                    (width - bitmapTextWidth(labels[index], scale)) / 2,
                    row.y + (3 * scale), scale, labels[index],
                    selected ? cyanColor : textColor);
                continue;
            }
            std::string value;
            if (index
                == static_cast<std::size_t>(HudSettingsSelection::InvertMouseY)) {
                value = hud.invertMouseY ? "ON" : "OFF";
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::Fullscreen)) {
                value = hud.fullscreen ? "ON" : "OFF";
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::VerticalSync)) {
                value = hud.verticalSync ? "ON" : "OFF";
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::GraphicsQuality)) {
                switch (hud.graphicsQuality) {
                case HudGraphicsQuality::Potato:
                    value = "POTATO";
                    break;
                case HudGraphicsQuality::Low:
                    value = "LOW";
                    break;
                case HudGraphicsQuality::Medium:
                    value = "MEDIUM";
                    break;
                case HudGraphicsQuality::High:
                    value = "HIGH";
                    break;
                case HudGraphicsQuality::Ultra:
                    value = "ULTRA";
                    break;
                }
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::WindowResolution)) {
                value = std::to_string(hud.windowWidth) + "X"
                    + std::to_string(hud.windowHeight);
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::FieldOfView)) {
                value = std::to_string(numbers[index]);
            } else {
                value = std::to_string(numbers[index]) + "%";
            }
            drawBitmapText(row.x + (8 * scale), row.y + (3 * scale),
                scale, labels[index], selected ? cyanColor : textColor);
            const UiRect decrement = layout.decrementButtons[index];
            const UiRect increment = layout.incrementButtons[index];
            const wkc::core::Vec3 arrowPanelColor = selected
                ? wkc::core::Vec3{0.10F, 0.27F, 0.30F}
                : wkc::core::Vec3{0.035F, 0.075F, 0.086F};
            drawSolidRect(decrement.x, decrement.y,
                decrement.width, decrement.height, arrowPanelColor);
            drawSolidRect(increment.x, increment.y,
                increment.width, increment.height, arrowPanelColor);
            drawBitmapText(
                decrement.x
                    + ((decrement.width - bitmapTextWidth("<", scale)) / 2),
                decrement.y + (3 * scale), scale, "<",
                selected ? cyanColor : textColor);
            drawBitmapText(
                increment.x
                    + ((increment.width - bitmapTextWidth(">", scale)) / 2),
                increment.y + (3 * scale), scale, ">",
                selected ? cyanColor : textColor);
            const int valueLeft = decrement.x + decrement.width;
            const int valueWidth = std::max(0, increment.x - valueLeft);
            drawBitmapText(valueLeft
                    + ((valueWidth - bitmapTextWidth(value, scale)) / 2),
                row.y + (3 * scale), scale, value,
                selected ? cyanColor : textColor);
        }
        const std::string_view controls = width >= 480
            ? "W S SELECT  A D ADJUST  ENTER BACK  MOUSE"
            : "W S  A D  ENTER  MOUSE";
        drawBitmapText((width - bitmapTextWidth(controls, 1)) / 2,
            layout.panel.y + (7 * scale), 1, controls,
            uiTheme.mutedText);

        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::ostringstream message;
            message << "OpenGL settings menu render error: 0x"
                    << std::hex << error;
            wkc::core::log(wkc::core::LogLevel::Error, message.str());
            return false;
        }
        return true;
    }

    if (hud.mainMenu) {
        const MainMenuLayout layout = makeMainMenuLayout(width, height);
        const int scale = layout.scale;
        drawSolidRect(0, 0, width, height, uiTheme.background);
        const int railWidth = std::max(2, 2 * scale);
        drawSolidRect(layout.panel.x - (5 * scale), layout.panel.y,
            railWidth, layout.panel.height, uiTheme.panelBorder);
        drawSolidRect(layout.panel.x + layout.panel.width + (3 * scale),
            layout.panel.y, railWidth, layout.panel.height, uiTheme.panelBorder);
        drawIndustrialPanel(layout.panel, scale, cyanColor);

        constexpr std::string_view title = "GDP";
        constexpr std::string_view subtitle = "GENERATED DUNGEON PROTOCOL";
        drawBitmapText((width - bitmapTextWidth(title, 3 * scale)) / 2,
            layout.panel.y + layout.panel.height - (26 * scale),
            3 * scale, title, cyanColor);
        drawBitmapText((width - bitmapTextWidth(subtitle, 1)) / 2,
            layout.panel.y + layout.panel.height - (38 * scale),
            1, subtitle, {0.48F, 0.62F, 0.66F});

        const std::string bestSeedText = hud.bestSeedAvailable
            ? "BEST SEED " + runTimeText(hud.bestSpeedrunTicks)
            : "BEST SEED --";
        const std::string continueText = hud.campaignContinueAvailable
            ? "CONTINUE F" + std::to_string(hud.campaignContinueFloor) + "/3"
            : "CONTINUE --";
        const std::array<std::string_view, 7> options{
            "START RUN", continueText, "CAMPAIGN", "SPEEDRUN", bestSeedText,
            "SETTINGS", "EXIT GAME",
        };
        const std::array<UiRect, 7> buttons{
            layout.startButton, layout.continueButton, layout.campaignButton,
            layout.speedrunButton,
            layout.bestSeedButton, layout.settingsButton, layout.exitButton,
        };
        std::size_t selected = 0U;
        if (hud.mainMenuSelection == HudMainMenuSelection::ContinueCampaign) {
            selected = 1U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::Campaign) {
            selected = 2U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::Speedrun) {
            selected = 3U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::BestSeed) {
            selected = 4U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::Settings) {
            selected = 5U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::ExitGame) {
            selected = 6U;
        }
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            const UiRect button = buttons[index];
            if (index == selected) {
                drawSelectedRow(button, scale, cyanColor);
            }
            const int textWidth = bitmapTextWidth(options[index], scale);
            drawBitmapText((width - textWidth) / 2,
                button.y + (3 * scale), scale, options[index],
                (index == 1U && !hud.campaignContinueAvailable)
                        || (index == 4U && !hud.bestSeedAvailable)
                    ? wkc::core::Vec3{0.30F, 0.38F, 0.40F}
                    : index == selected ? cyanColor : textColor);
        }

        const std::string_view controls = width >= 480
            ? "W S SELECT  ENTER CONFIRM  MOUSE SELECT"
            : "W S  ENTER  MOUSE";
        drawBitmapText((width - bitmapTextWidth(controls, 1)) / 2,
            layout.panel.y + (7 * scale), 1, controls,
            uiTheme.mutedText);

        drawExitConfirmation();

        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::ostringstream message;
            message << "OpenGL main menu render error: 0x" << std::hex << error;
            wkc::core::log(wkc::core::LogLevel::Error, message.str());
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
            const wkc::core::Vec3 damageColor{
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
        const int armor = static_cast<int>(std::round(std::max(hud.armor, 0.0F)));
        const int maximumArmor = static_cast<int>(std::round(
            std::max(hud.maximumArmor, 1.0F)));
        std::ostringstream armorStream;
        armorStream << "ARMOR " << armor << '/' << maximumArmor;
        std::ostringstream ammoStream;
        ammoStream << (hud.weapon == HudWeaponRole::Carbine
                ? "CARBINE" : "PISTOL")
                   << (hud.weaponOverdrive ? " OD " : " ")
                   << hud.magazineAmmo << '/' << hud.reserveAmmo;
        std::ostringstream keysStream;
        keysStream << "KEYS " << hud.keys;
        const std::string healthText = healthStream.str();
        const std::string armorText = armorStream.str();
        const std::string ammoText = ammoStream.str();
        const std::string keysText = keysStream.str();
        const int statusTextWidth = std::max({
            bitmapTextWidth(healthText, hudScale),
            bitmapTextWidth(armorText, hudScale),
            bitmapTextWidth(ammoText, hudScale),
            bitmapTextWidth(keysText, hudScale),
        });
        const int statusWidth = statusTextWidth + (hudPadding * 2);
        const int statusHeight = (hudPadding * 2) + (hudLineHeight * 4)
            + (8 * hudScale);
        const int statusX = hudMargin;
        const int statusY = hudMargin;
        drawIndustrialPanel(
            {statusX, statusY, statusWidth, statusHeight}, hudScale, cyanColor);
        const int statusTextX = statusX + hudPadding;
        const int healthTextY = statusY + statusHeight - hudPadding - (7 * hudScale);
        const float healthRatio = std::clamp(
            std::max(hud.health, 0.0F) / std::max(hud.maximumHealth, 1.0F),
            0.0F, 1.0F);
        const int barWidth = statusWidth - (hudPadding * 2);
        const int healthBarY = healthTextY - (5 * hudScale);
        drawBitmapText(statusTextX, healthTextY, hudScale, healthText,
            healthRatio <= 0.25F
                ? wkc::core::Vec3{0.98F, 0.20F, 0.12F} : textColor);
        drawSolidRect(statusTextX, healthBarY, barWidth, 3 * hudScale,
            {0.10F, 0.14F, 0.15F});
        drawSolidRect(statusTextX, healthBarY,
            static_cast<int>(static_cast<float>(barWidth) * healthRatio),
            3 * hudScale,
            healthRatio <= 0.25F
                ? wkc::core::Vec3{0.94F, 0.16F, 0.10F}
                : wkc::core::Vec3{0.24F, 0.88F, 0.52F});
        const int armorTextY = healthTextY - hudLineHeight;
        const int armorBarY = armorTextY - (5 * hudScale);
        const float armorRatio = std::clamp(
            std::max(hud.armor, 0.0F) / std::max(hud.maximumArmor, 1.0F),
            0.0F, 1.0F);
        drawBitmapText(statusTextX, armorTextY, hudScale, armorText,
            armor > 0 ? wkc::core::Vec3{0.36F, 0.68F, 1.0F} : textColor);
        drawSolidRect(statusTextX, armorBarY, barWidth, 3 * hudScale,
            {0.10F, 0.14F, 0.15F});
        drawSolidRect(statusTextX, armorBarY,
            static_cast<int>(static_cast<float>(barWidth) * armorRatio),
            3 * hudScale, {0.16F, 0.48F, 0.98F});
        drawBitmapText(statusTextX, healthTextY - (hudLineHeight * 2),
            hudScale, ammoText,
            hud.magazineAmmo == 0U
                ? wkc::core::Vec3{0.98F, 0.42F, 0.14F} : textColor);
        drawBitmapText(statusTextX, healthTextY - (hudLineHeight * 3),
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
        drawIndustrialPanel(
            {progressX, progressY, progressWidth, progressHeight},
            hudScale, cyanColor);
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
        wkc::core::Vec3 eventColor{0.48F, 0.62F, 0.66F};
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
            drawIndustrialPanel(
                {eventX, eventY, eventWidth, eventHeight}, hudScale, eventColor);
            drawBitmapText(eventX + hudPadding, eventY + hudPadding,
                hudScale, eventText, eventColor);
        }

        std::string_view promptText;
        wkc::core::Vec3 promptColor{0.78F, 0.90F, 0.92F};
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
            drawIndustrialPanel(
                {promptX, promptY, promptWidth, promptHeight}, hudScale, promptColor);
            drawBitmapText(promptX + hudPadding, promptY + hudPadding,
                hudScale, promptText, promptColor);
        }

        const float hitStrength = std::clamp(hud.hitFeedbackStrength, 0.0F, 1.0F);
        if (!hud.paused && hud.hitFeedback != HudHitFeedback::None
            && hitStrength > 0.0F) {
            const bool killed = hud.hitFeedback == HudHitFeedback::Kill
                || hud.hitFeedback == HudHitFeedback::WeakKill;
            const bool weakPoint = hud.hitFeedback == HudHitFeedback::WeakHit
                || hud.hitFeedback == HudHitFeedback::WeakKill;
            const wkc::core::Vec3 baseColor = weakPoint
                ? wkc::core::Vec3{1.0F, 0.78F, 0.16F}
                : killed ? wkc::core::Vec3{1.0F, 0.26F, 0.10F}
                         : wkc::core::Vec3{0.82F, 0.94F, 0.96F};
            const wkc::core::Vec3 markerColor = baseColor
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
            if (weakPoint) {
                const int innerGap = 5 * hudScale;
                const int innerLength = 2 * hudScale;
                drawSolidRect(centerX - innerGap - innerLength,
                    centerY - (markerThickness / 2), innerLength,
                    markerThickness, markerColor);
                drawSolidRect(centerX + innerGap,
                    centerY - (markerThickness / 2), innerLength,
                    markerThickness, markerColor);
            }
        }

        std::string_view noticeText;
        wkc::core::Vec3 noticeColor{0.72F, 0.90F, 0.92F};
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
            drawIndustrialPanel(
                {noticeX, noticeY, noticeWidth, noticeHeight}, hudScale, noticeColor);
            drawBitmapText(noticeX + hudPadding, noticeY + hudPadding,
                hudScale, noticeText,
                noticeColor * (0.42F + (0.58F * noticeStrength)));
        }
    }

    if (hud.performanceOverlay) {
        const wkc::core::Vec3 performanceColor
            = hud.percentile95FrameMilliseconds <= 17U
            ? uiTheme.success
            : hud.percentile95FrameMilliseconds <= 34U
                ? wkc::core::Vec3{0.98F, 0.62F, 0.14F}
                : uiTheme.danger;
        std::string qualityName = "MEDIUM";
        switch (hud.graphicsQuality) {
        case HudGraphicsQuality::Potato:
            qualityName = "POTATO";
            break;
        case HudGraphicsQuality::Low:
            qualityName = "LOW";
            break;
        case HudGraphicsQuality::Medium:
            break;
        case HudGraphicsQuality::High:
            qualityName = "HIGH";
            break;
        case HudGraphicsQuality::Ultra:
            qualityName = "ULTRA";
            break;
        }
        const std::string gpuTime = hud.gpuTimingAvailable
            ? std::to_string(hud.averageGpuMilliseconds) + "MS"
            : "--";
        const std::array<std::string, 15> lines{
            "QUALITY " + qualityName + " SCALE "
                + std::to_string(hud.worldRenderScalePercent) + "%",
            "FPS " + std::to_string(hud.performanceFramesPerSecond)
                + " FRAME " + std::to_string(hud.averageFrameMilliseconds)
                + "MS P95 " + std::to_string(hud.percentile95FrameMilliseconds)
                + "MS",
            "WORST " + std::to_string(hud.worstFrameMilliseconds)
                + "MS SLOW " + std::to_string(hud.framesOverBudgetPercent)
                + "%",
            "CPU UPDATE " + std::to_string(hud.averageUpdateMilliseconds)
                + "MS RENDER " + std::to_string(hud.averageRenderMilliseconds)
                + "MS",
            "GPU " + gpuTime + " WAIT "
                + std::to_string(hud.averagePresentMilliseconds) + "MS",
            "DRAW " + std::to_string(hud.performanceDrawCalls)
                + " TRI " + std::to_string(hud.performanceTriangles),
            "VISIBLE E " + std::to_string(hud.performanceVisibleEnemies)
                + " P " + std::to_string(hud.performanceVisibleProps)
                + " R " + std::to_string(hud.performanceVisibleRooms),
            "LIGHT " + std::to_string(hud.performanceActiveLights)
                + " SHADOW " + std::to_string(hud.performanceShadowLights)
                + "X" + std::to_string(hud.performanceShadowOccluders),
            std::string{"SHADER "}
                + (hud.performanceLowSpecShader ? "LOW SPEC" : "FULL")
                + " SHADOW CAP "
                + std::to_string(
                    hud.performanceShadowShaderOccluderCapacity),
            "CULL DRAW "
                + std::to_string(hud.performanceBackfaceCulledDraws)
                + " STATE "
                + std::to_string(hud.performanceCullStateChanges),
            "INSTANCE DRAW "
                + std::to_string(hud.performanceInstancedDrawCalls)
                + " COUNT "
                + std::to_string(hud.performanceRenderedInstances)
                + " SAVE "
                + std::to_string(hud.performanceAvoidedDrawCalls),
            "UNIFORM UP " + std::to_string(hud.performanceUniformUploads)
                + " SKIP " + std::to_string(hud.performanceUniformSkips),
            "TEXTURE BIND " + std::to_string(hud.performanceTextureBinds)
                + " SKIP " + std::to_string(hud.performanceTextureBindSkips)
                + " UNIT "
                + std::to_string(hud.performanceTextureUnitActivations),
            "MESH BIND " + std::to_string(hud.performanceMeshBinds)
                + " SKIP " + std::to_string(hud.performanceMeshBindSkips),
            "CACHE M " + std::to_string(hud.performanceMeshCacheEntries)
                + " T " + std::to_string(hud.performanceTextureCacheEntries)
                + " LOAD " + std::to_string(hud.worldGenerationMilliseconds)
                + "/" + std::to_string(hud.assetPreparationMilliseconds)
                + "MS",
        };
        constexpr std::string_view title = "F3 PERFORMANCE";
        int textWidth = bitmapTextWidth(title, 1);
        for (const std::string& line : lines) {
            textWidth = std::max(textWidth, bitmapTextWidth(line, 1));
        }
        const int performancePadding = 4;
        const int lineHeight = 9;
        const int panelWidth = std::min(
            std::max(1, width - (hudMargin * 2)),
            textWidth + (performancePadding * 2));
        const int panelHeight = (static_cast<int>(lines.size()) + 1)
            * lineHeight + (performancePadding * 2);
        const int panelX = hudMargin;
        const int panelY = std::max(
            hudMargin, height - hudMargin - panelHeight);
        drawIndustrialPanel(
            {panelX, panelY, panelWidth, panelHeight}, 1, performanceColor);
        int textY = panelY + panelHeight - performancePadding - 7;
        drawBitmapText(panelX + performancePadding, textY, 1,
            title, performanceColor);
        for (const std::string& line : lines) {
            textY -= lineHeight;
            drawBitmapText(panelX + performancePadding, textY, 1,
                line, textColor);
        }
    }

    std::string_view objectiveText;
    wkc::core::Vec3 objectiveColor{0.30F, 0.88F, 0.92F};
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
    if (hud.visible && hud.runTimerVisible) {
        const std::string timerPrefix = hud.speedrunMode
            ? "SPEEDRUN "
            : hud.campaignMode
                ? "FLOOR " + std::to_string(hud.campaignFloor) + "/"
                    + std::to_string(hud.campaignFloorCount) + "  THREAT "
                    + std::to_string(hud.campaignThreatPercent) + "%  TIME "
                : "TIME ";
        const std::string timerText = timerPrefix + runTimeText(hud.elapsedRunTicks);
        const int timerWidth = bitmapTextWidth(timerText, hudScale);
        const int timerOffset = objectiveText.empty() ? 7 : 17;
        drawBitmapText(std::max(hudMargin, width - timerWidth - hudMargin),
            std::max(hudMargin,
                height - hudMargin - (timerOffset * hudScale)),
            hudScale, timerText, textColor);
        if (hud.speedrunMode && hud.speedrunRecordVisible) {
            const std::string bestText
                = "BEST " + runTimeText(hud.bestSpeedrunTicks);
            const int bestWidth = bitmapTextWidth(bestText, hudScale);
            drawBitmapText(std::max(hudMargin, width - bestWidth - hudMargin),
                std::max(hudMargin,
                    height - hudMargin - ((timerOffset + 10) * hudScale)),
                hudScale, bestText, {0.94F, 0.78F, 0.18F});
        }
    }
    if (hud.objective == ObjectiveRenderState::Victory
        || hud.objective == ObjectiveRenderState::Defeat) {
        const bool victory = hud.objective == ObjectiveRenderState::Victory;
        const std::string_view result = hud.campaignAdvanceAvailable
            ? "FLOOR CLEAR"
            : victory && hud.campaignMode ? "CAMPAIGN CLEAR"
            : victory ? "VICTORY" : "DEFEAT";
        const ResultMenuLayout layout = makeResultMenuLayout(width, height);
        const int scale = layout.scale;
        const wkc::core::Vec3 resultColor = victory
            ? uiTheme.success : uiTheme.danger;
        drawIndustrialPanel(layout.panel, scale, resultColor);
        drawBitmapText((width - bitmapTextWidth(result, 2 * scale)) / 2,
            layout.panel.y + layout.panel.height - (24 * scale),
            2 * scale, result, resultColor);

        if (hud.speedrunMode && hud.speedrunRecordVisible
            && layout.panel.height >= 140 * scale) {
            const std::string recordText
                = (hud.newSpeedrunRecord ? "NEW RECORD " : "BEST ")
                + runTimeText(hud.bestSpeedrunTicks);
            const wkc::core::Vec3 recordColor = hud.newSpeedrunRecord
                ? wkc::core::Vec3{0.98F, 0.78F, 0.16F}
                : wkc::core::Vec3{0.64F, 0.78F, 0.82F};
            drawBitmapText(
                (width - bitmapTextWidth(recordText, scale)) / 2,
                layout.panel.y + (104 * scale), scale,
                recordText, recordColor);
        }

        const std::array<std::string_view, 4> options{
            hud.campaignAdvanceAvailable
                ? "NEXT FLOOR" : victory ? "NEW RUN" : "NEXT SEED",
            hud.campaignMode ? "REPLAY FLOOR" : "REPLAY SEED",
            "MAIN MENU", "EXIT GAME",
        };
        const std::array<UiRect, 4> buttons{
            layout.playAgainButton, layout.replaySeedButton,
            layout.mainMenuButton, layout.exitButton,
        };
        std::size_t selected = 0U;
        if (hud.resultMenuSelection == HudResultMenuSelection::ReplaySeed) {
            selected = 1U;
        } else if (hud.resultMenuSelection == HudResultMenuSelection::MainMenu) {
            selected = 2U;
        } else if (hud.resultMenuSelection
            == HudResultMenuSelection::ExitGame) {
            selected = 3U;
        }
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            const UiRect button = buttons[index];
            if (index == selected) {
                drawSelectedRow(button, scale, resultColor);
            }
            const int optionWidth = bitmapTextWidth(options[index], scale);
            drawBitmapText((width - optionWidth) / 2,
                button.y + (3 * scale), scale, options[index],
                index == selected ? resultColor : textColor);
        }
        const std::string_view controls = width >= 480
            ? "W S  ENTER  MOUSE  ESC"
            : "W S  ENTER  MOUSE";
        drawBitmapText((width - bitmapTextWidth(controls, 1)) / 2,
            layout.panel.y + (7 * scale), 1, controls,
            uiTheme.mutedText);
    }
    if (hud.paused) {
        const PauseMenuLayout layout = makePauseMenuLayout(width, height);
        const int scale = layout.scale;
        drawIndustrialPanel(layout.panel, scale, cyanColor);
        constexpr std::string_view pauseTitle = "PAUSED";
        const int titleWidth = bitmapTextWidth(pauseTitle, scale);
        drawBitmapText((width - titleWidth) / 2,
            layout.panel.y + layout.panel.height - (14 * scale), scale,
            pauseTitle, cyanColor);

        constexpr std::array<std::string_view, 5> options{
            "RESUME", "RESTART RUN", "SETTINGS", "MAIN MENU", "EXIT GAME",
        };
        std::size_t selected = 0U;
        if (hud.pauseSelection == HudPauseSelection::RestartRun) {
            selected = 1U;
        } else if (hud.pauseSelection == HudPauseSelection::Settings) {
            selected = 2U;
        } else if (hud.pauseSelection == HudPauseSelection::MainMenu) {
            selected = 3U;
        } else if (hud.pauseSelection == HudPauseSelection::ExitGame) {
            selected = 4U;
        }
        const std::array<UiRect, 5> buttons{
            layout.resumeButton, layout.restartButton,
            layout.settingsButton, layout.mainMenuButton, layout.exitButton,
        };
        for (std::size_t index = 0; index < options.size(); ++index) {
            const UiRect button = buttons[index];
            const int optionWidth = bitmapTextWidth(options[index], scale);
            if (index == selected) {
                drawSelectedRow(button, scale, cyanColor);
            }
            drawBitmapText((width - optionWidth) / 2,
                button.y + (2 * scale), scale, options[index],
                index == selected ? cyanColor : textColor);
        }
        const std::string_view controls = width >= 480
            ? "W S  ENTER  MOUSE  ESC"
            : "W S  ENTER  MOUSE";
        const int controlsWidth = bitmapTextWidth(controls, 1);
        drawBitmapText((width - controlsWidth) / 2,
            layout.panel.y + (6 * scale),
            1, controls, uiTheme.mutedText);
    } else {
        glClearColor(0.78F, 0.88F, 0.90F, 1.0F);
        glScissor((width / 2) - 7, (height / 2) - 1, 15, 3);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor((width / 2) - 1, (height / 2) - 7, 3, 15);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    drawExitConfirmation();
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::ostringstream message;
        message << "OpenGL render error: 0x" << std::hex << error;
        wkc::core::log(wkc::core::LogLevel::Error, message.str());
        return false;
    }
    return true;
}

} // namespace gdp::game::visual
