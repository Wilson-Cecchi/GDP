#include "core/Math.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "game/visual/VisualAssets.hpp"
#include "render/Camera.hpp"
#include "render/Mesh.hpp"
#include "render/RasterState.hpp"
#include "game/visual/GdpRenderer.hpp"
#include "game/visual/WeaponViewmodelAssets.hpp"
#include "render/Texture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace {

int failures = 0;
int uniformDriverLookupCalls = 0;

GLint APIENTRY fakeGetUniformLocation(
    const GLuint program, const GLchar* const name)
{
    ++uniformDriverLookupCalls;
    if (program == 17U && std::strcmp(name, "uModel") == 0) {
        return 4;
    }
    return -1;
}

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool isFinite(const wkc::core::Mat4& matrix)
{
    for (const float element : matrix.elements) {
        if (!std::isfinite(element)) {
            return false;
        }
    }
    return true;
}

void testMesh()
{
    using namespace wkc::render;

    const MeshData box = makeBoxMesh();
    std::string reason;
    expect(validateMeshData(box, &reason), "box mesh validates");
    expect(box.vertices.size() == 24, "box has hard-edge vertices");
    expect(box.indices.size() == 36, "box has twelve indexed triangles");
    expect(box.backfaceCullingSafe,
        "closed box explicitly permits back-face culling");
    expect(box.bounds == wkc::core::Aabb{{-0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, 0.5F}},
        "box bounds are exact");

    MeshData invalidIndex = box;
    invalidIndex.indices[0] = 999;
    expect(!validateMeshData(invalidIndex, &reason), "out-of-range mesh index is rejected");

    MeshData invalidVertex = box;
    invalidVertex.vertices[0].position.x = std::numeric_limits<float>::quiet_NaN();
    expect(!validateMeshData(invalidVertex, &reason), "non-finite vertex is rejected");

    MeshData empty;
    expect(!validateMeshData(empty, &reason), "empty mesh is rejected safely");
}

void testBackfaceCullingStateCache()
{
    wkc::render::BackfaceCullingStateCache cache;
    expect(cache.needsUpdate(true) && !cache.needsUpdate(true)
            && cache.needsUpdate(false) && !cache.needsUpdate(false),
        "back-face culling cache updates only when raster state changes");
    expect(cache.stats()
            == wkc::render::BackfaceCullingStateStats{4U, 2U, 2U},
        "back-face culling cache reports changes and avoided updates");

    cache.beginFrame();
    expect(cache.stats() == wkc::render::BackfaceCullingStateStats{}
            && cache.needsUpdate(false),
        "new frame invalidates externally mutable culling state");
}

void testMeshBindingCache()
{
    wkc::render::MeshBindingCache cache;
    expect(cache.needsBind(11U) && !cache.needsBind(11U)
            && cache.needsBind(12U) && !cache.needsBind(12U),
        "mesh binding cache binds only when the vertex array changes");
    expect(cache.stats()
            == wkc::render::MeshBindingCacheStats{4U, 2U, 2U},
        "mesh binding cache exposes real and avoided VAO binds");

    cache.invalidate();
    expect(cache.needsBind(12U),
        "external VAO mutation invalidates the cached binding");

    cache.beginFrame();
    expect(cache.stats() == wkc::render::MeshBindingCacheStats{}
            && cache.needsBind(12U),
        "new frame invalidates external vertex-array state conservatively");
}

void testInstanceTransforms()
{
    using wkc::core::Mat4;
    using wkc::render::validateInstanceTransforms;

    constexpr std::array transforms{
        Mat4::identity(),
        wkc::core::translationMatrix({2.0F, 0.0F, -1.0F}),
    };
    std::string reason;
    expect(validateInstanceTransforms(transforms, &reason),
        "finite instance transforms are accepted");
    expect(validateInstanceTransforms({}, &reason),
        "an empty instance batch safely clears the active batch");

    std::array invalidTransforms{Mat4::identity()};
    invalidTransforms.front().elements[7]
        = std::numeric_limits<float>::quiet_NaN();
    expect(!validateInstanceTransforms(invalidTransforms, &reason)
            && reason == "instance transform contains a non-finite component",
        "non-finite instance transforms are rejected with a stable reason");
}

void testRendererShaderPathSelection()
{
    using gdp::game::visual::RendererShaderPath;
    using gdp::game::visual::selectRendererShaderPath;
    using gdp::game::visual::shadowShaderOccluderCapacity;

    expect(selectRendererShaderPath(false, 2U, 0U, 0U)
            == RendererShaderPath::LowSpec
            && selectRendererShaderPath(false, 4U, 0U, 0U)
                == RendererShaderPath::LowSpec,
        "low-spec shader covers the Potato and Low light budgets");
    expect(selectRendererShaderPath(true, 2U, 0U, 0U)
            == RendererShaderPath::FullShadow3
            && selectRendererShaderPath(false, 5U, 0U, 0U)
                == RendererShaderPath::FullShadow3
            && selectRendererShaderPath(false, 2U, 1U, 0U)
                == RendererShaderPath::FullShadow3
            && selectRendererShaderPath(false, 2U, 0U, 1U)
                == RendererShaderPath::FullShadow3,
        "unsupported detail, light and shadow inputs fail back to the full shader");
    expect(selectRendererShaderPath(true, 8U, 1U, 4U)
            == RendererShaderPath::FullShadow6
            && selectRendererShaderPath(true, 8U, 2U, 7U)
                == RendererShaderPath::FullShadow8
            && shadowShaderOccluderCapacity(RendererShaderPath::LowSpec) == 0U
            && shadowShaderOccluderCapacity(RendererShaderPath::FullShadow3) == 3U
            && shadowShaderOccluderCapacity(RendererShaderPath::FullShadow6) == 6U
            && shadowShaderOccluderCapacity(RendererShaderPath::FullShadow8) == 8U,
        "full shader variants select the smallest safe shadow budget");
}

void testTextureData()
{
    using namespace wkc::render;

    const ImageData first = gdp::game::makeIndustrialCheckerImage(8, 8);
    const ImageData second = gdp::game::makeIndustrialCheckerImage(8, 8);
    expect(validateImageData(first), "generated RGBA image validates");
    expect(first.rgba == second.rgba, "procedural texture pixels are deterministic");
    expect(first.rgba.size() == 8U * 8U * 4U, "texture contains four channels per pixel");
    expect(first.rgba[0] != first.rgba[(2U * 4U)], "checker pattern contains two tones");

    const gdp::game::CeilingFixtureMaterialImages fixture
        = gdp::game::makeCeilingFixtureMaterialImages();
    const gdp::game::CeilingFixtureMaterialImages repeatedFixture
        = gdp::game::makeCeilingFixtureMaterialImages();
    expect(validateImageData(fixture.albedo) && validateImageData(fixture.normal)
            && validateImageData(fixture.properties) && validateImageData(fixture.emission)
            && fixture.albedo.width == 1024 && fixture.albedo.height == 1024,
        "independent ceiling-fixture material provides four valid 1024 HD maps");
    expect(fixture.albedo.rgba == repeatedFixture.albedo.rgba
            && fixture.normal.rgba == repeatedFixture.normal.rgba
            && fixture.properties.rgba == repeatedFixture.properties.rgba
            && fixture.emission.rgba == repeatedFixture.emission.rgba,
        "ceiling-fixture HD maps are byte-exact and deterministic");
    const auto fixturePixel = [](const int x, const int y) {
        return (static_cast<std::size_t>(y) * 1024U + static_cast<std::size_t>(x)) * 4U;
    };
    const std::size_t housing = fixturePixel(256, 512);
    const std::size_t metal = fixturePixel(600, 512);
    const std::size_t lens = fixturePixel(896, 512);
    expect(fixture.albedo.rgba[housing] < 80U && fixture.albedo.rgba[lens] > 210U,
        "ceiling-fixture atlas separates graphite housing from its frosted lens");
    expect(fixture.properties.rgba[metal + 1U] > 220U
            && fixture.properties.rgba[housing + 1U] < 100U,
        "packed material map separates exposed metal from painted housing");
    expect(fixture.emission.rgba[housing] == 0U && fixture.emission.rgba[metal] == 0U
            && fixture.emission.rgba[lens] > 240U,
        "emission map isolates light output to the lens atlas region");

    ImageData malformed = first;
    malformed.rgba.pop_back();
    expect(!validateImageData(malformed), "malformed texture data is rejected");
}

void testTextureBindingCache()
{
    wkc::render::TextureBindingCache cache;
    const wkc::render::TextureBindingDecision first = cache.prepare(0U, 11U);
    const wkc::render::TextureBindingDecision repeated = cache.prepare(0U, 11U);
    const wkc::render::TextureBindingDecision changed = cache.prepare(0U, 12U);
    const wkc::render::TextureBindingDecision secondUnit = cache.prepare(1U, 12U);
    const wkc::render::TextureBindingDecision repeatedSecond
        = cache.prepare(1U, 12U);
    const wkc::render::TextureBindingDecision untracked
        = cache.prepare(17U, 31U);
    const wkc::render::TextureBindingDecision repeatedUntracked
        = cache.prepare(17U, 31U);

    expect(first == wkc::render::TextureBindingDecision{true, true}
            && repeated == wkc::render::TextureBindingDecision{false, false}
            && changed == wkc::render::TextureBindingDecision{false, true},
        "texture binding cache skips a repeated texture on the active unit");
    expect(secondUnit == wkc::render::TextureBindingDecision{true, true}
            && repeatedSecond
                == wkc::render::TextureBindingDecision{false, false},
        "texture binding cache tracks independent texture units");
    expect(untracked == wkc::render::TextureBindingDecision{true, true}
            && repeatedUntracked
                == wkc::render::TextureBindingDecision{false, true},
        "untracked high units fail safely by preserving texture binds");
    expect(cache.stats()
            == wkc::render::TextureBindingCacheStats{7U, 3U, 5U, 2U},
        "texture binding cache exposes activations, binds and avoided binds");

    cache.beginFrame();
    expect(cache.stats() == wkc::render::TextureBindingCacheStats{}
            && cache.prepare(0U, 11U)
                == wkc::render::TextureBindingDecision{true, true},
        "new frame invalidates external OpenGL texture state conservatively");
}

void testCameraAndMatrices()
{
    using namespace wkc::core;
    using wkc::render::Camera;

    expect(nearlyEqual(length(Vec3{3.0F, 4.0F, 0.0F}), 5.0F), "vector length is correct");
    expect(cross({1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}) == Vec3{0.0F, 0.0F, 1.0F},
        "cross product follows the coordinate convention");

    const Mat4 model = translationMatrix({2.0F, 3.0F, 4.0F})
        * scaleMatrix({5.0F, 6.0F, 7.0F});
    expect(nearlyEqual(model.elements[0], 5.0F), "model matrix contains X scale");
    expect(nearlyEqual(model.elements[5], 6.0F), "model matrix contains Y scale");
    expect(nearlyEqual(model.elements[10], 7.0F), "model matrix contains Z scale");
    expect(nearlyEqual(model.elements[12], 2.0F), "model matrix preserves translation");

    Camera camera;
    camera.lookAt({0.0F, 1.7F, 6.5F}, {0.0F, 1.5F, 0.0F});
    camera.setPerspective(68.0F, 0.05F, 100.0F);
    const Mat4 view = camera.viewMatrix();
    const Mat4 projection = camera.projectionMatrix(16.0F / 9.0F);
    expect(isFinite(view), "view matrix remains finite");
    expect(isFinite(projection), "projection matrix remains finite");
    expect(projection.elements[0] > 0.0F && projection.elements[5] > 0.0F,
        "perspective focal terms are positive");
    expect(nearlyEqual(projection.elements[11], -1.0F),
        "projection uses OpenGL perspective convention");
}

void testGpuTimerFallback()
{
    wkc::render::GpuTimer timer;
    expect(!timer.isSupported() && !timer.beginFrame()
            && !timer.latestMilliseconds(),
        "GPU timer remains an optional no-op before OpenGL initialization");
    timer.endFrame();
    timer.reset();
}

void testShaderUniformLocationCache()
{
    wkc::render::GlApi gl{};
    gl.getUniformLocation = fakeGetUniformLocation;
    wkc::render::ShaderUniformLocationCache cache;
    uniformDriverLookupCalls = 0;

    expect(cache.resolve(gl, 17U, "uModel") == 4
            && cache.resolve(gl, 17U, "uModel") == 4,
        "shader uniform cache preserves valid locations");
    expect(cache.resolve(gl, 17U, "uUnused") == -1
            && cache.resolve(gl, 17U, "uUnused") == -1,
        "shader uniform cache also preserves inactive locations");
    const wkc::render::ShaderUniformCacheStats warmStats = cache.stats();
    expect(uniformDriverLookupCalls == 2 && warmStats.requests == 4U
            && warmStats.hits == 2U && warmStats.driverLookups == 2U
            && warmStats.entries == 2U,
        "repeated uniforms reach the OpenGL driver only once per name");

    cache.reset();
    expect(cache.stats() == wkc::render::ShaderUniformCacheStats{},
        "shader uniform cache reset clears entries and metrics");
    expect(cache.resolve(gl, 17U, "uModel") == 4
            && uniformDriverLookupCalls == 3,
        "shader uniform cache resolves names again after program reset");
}

void testShaderUniformValueCache()
{
    wkc::render::ShaderUniformValueCache cache;
    const wkc::core::Mat4 identity = wkc::core::Mat4::identity();

    expect(cache.needsUpload(4, 1) && !cache.needsUpload(4, 1)
            && cache.needsUpload(4, 2),
        "shader uniform value cache skips only repeated integer values");
    expect(cache.needsUpload(4, 2.0F) && !cache.needsUpload(4, 2.0F),
        "shader uniform value cache distinguishes uniform types");
    expect(cache.needsUpload(5, wkc::core::Vec2{1.0F, 2.0F})
            && !cache.needsUpload(5, wkc::core::Vec2{1.0F, 2.0F})
            && cache.needsUpload(6, wkc::core::Vec3{1.0F, 2.0F, 3.0F})
            && cache.needsUpload(7, identity)
            && !cache.needsUpload(7, identity),
        "shader uniform value cache handles vectors and matrices exactly");
    const wkc::render::ShaderUniformValueCacheStats warmStats = cache.stats();
    expect(warmStats.requests == 10U && warmStats.uploads == 6U
            && warmStats.skips == 4U && warmStats.entries == 4U,
        "shader uniform value cache exposes uploads avoided per frame");

    cache.beginFrame();
    expect(cache.stats()
            == wkc::render::ShaderUniformValueCacheStats{0U, 0U, 0U, 4U}
            && !cache.needsUpload(7, identity),
        "new frame clears metrics while preserving reusable values");
    cache.reset();
    expect(cache.stats() == wkc::render::ShaderUniformValueCacheStats{},
        "shader uniform value cache reset invalidates values and metrics");
}

void testConservativeVisibilityCulling()
{
    using namespace gdp::game::visual;
    using namespace wkc::core;

    const CameraData camera{};
    constexpr float wideAspect = 16.0F / 9.0F;
    expect(isSpherePotentiallyVisible(
               camera, {0.0F, 1.0F, -4.0F}, 0.5F, wideAspect),
        "visibility culling retains geometry in front of the camera");
    expect(!isSpherePotentiallyVisible(
               camera, {0.0F, 1.0F, 20.0F}, 0.5F, wideAspect),
        "visibility culling rejects geometry fully behind the camera");
    expect(isSpherePotentiallyVisible(
               camera, camera.position, 1.0F, wideAspect),
        "visibility culling retains geometry containing the camera");
    expect(!isSpherePotentiallyVisible(
               camera, {100.0F, 1.0F, -4.0F}, 0.5F, wideAspect),
        "visibility culling rejects geometry beyond the horizontal margin");

    const Aabb front{{-1.0F, 0.0F, -5.0F}, {1.0F, 3.0F, -3.0F}};
    const Aabb behind{{-1.0F, 0.0F, 12.0F}, {1.0F, 3.0F, 14.0F}};
    expect(isAabbPotentiallyVisible(camera, front, wideAspect),
        "room bounds in front of the camera remain visible");
    expect(!isAabbPotentiallyVisible(camera, behind, wideAspect),
        "room bounds fully behind the camera are culled");
    expect(isAabbPotentiallyVisible(
               camera, {{1.0F, 1.0F, 1.0F}, {-1.0F, -1.0F, -1.0F}}, wideAspect),
        "invalid bounds fail open instead of hiding geometry");

    const std::array<Vec3, 2U> corridorPoints{
        Vec3{0.0F, 0.0F, 0.0F}, Vec3{2.0F, 0.0F, -3.0F}};
    const auto corridorBounds = corridorRenderBounds({
        std::span<const Vec3>{corridorPoints}, 2.0F, 3.0F, 0U});
    expect(corridorBounds
            && *corridorBounds
                == Aabb{{-1.0F, 0.0F, -4.0F}, {3.0F, 3.0F, 1.0F}},
        "corridor bounds include path width and height");
    expect(!corridorRenderBounds({{}, 2.0F, 3.0F, 0U}),
        "empty corridor paths cannot produce misleading bounds");
}

void testMainMenuLayout()
{
    using namespace gdp::game::visual;

    const MainMenuLayout wide = makeMainMenuLayout(1280, 720);
    expect(wide.scale == 2, "wide menu uses doubled bitmap scale");
    expect(wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "wide menu panel stays within the framebuffer");
    expect(wide.panel.contains(wide.startButton.x, wide.startButton.y),
        "Start Run button is inside the menu panel");
    expect(wide.panel.contains(wide.continueButton.x, wide.continueButton.y),
        "Continue button is inside the menu panel");
    expect(wide.panel.contains(wide.campaignButton.x, wide.campaignButton.y),
        "Campaign button is inside the menu panel");
    expect(wide.panel.contains(wide.speedrunButton.x, wide.speedrunButton.y),
        "Speedrun button is inside the menu panel");
    expect(wide.panel.contains(wide.bestSeedButton.x, wide.bestSeedButton.y),
        "Best Seed button is inside the menu panel");
    expect(wide.panel.contains(wide.settingsButton.x, wide.settingsButton.y),
        "Settings button is inside the menu panel");
    expect(wide.panel.contains(wide.controlsButton.x, wide.controlsButton.y),
        "Controls button is inside the menu panel");
    expect(wide.panel.contains(wide.exitButton.x, wide.exitButton.y),
        "Exit Game button is inside the menu panel");
    expect(wide.exitButton.y >= wide.panel.y + (16 * wide.scale)
            && wide.exitButton.y + wide.exitButton.height
                <= wide.controlsButton.y
            && wide.controlsButton.y + wide.controlsButton.height
                <= wide.settingsButton.y
            && wide.settingsButton.y + wide.settingsButton.height
                <= wide.bestSeedButton.y
            && wide.bestSeedButton.y + wide.bestSeedButton.height
                <= wide.speedrunButton.y
            && wide.speedrunButton.y + wide.speedrunButton.height
                <= wide.campaignButton.y
            && wide.campaignButton.y + wide.campaignButton.height
                <= wide.continueButton.y
            && wide.continueButton.y + wide.continueButton.height
                <= wide.startButton.y,
        "eight main-menu buttons remain separated from footer and each other");

    const float startX = static_cast<float>(wide.startButton.x + wide.startButton.width / 2);
    const float startY = static_cast<float>(
        720 - 1 - (wide.startButton.y + wide.startButton.height / 2));
    const float exitX = static_cast<float>(wide.exitButton.x + wide.exitButton.width / 2);
    const float exitY = static_cast<float>(
        720 - 1 - (wide.exitButton.y + wide.exitButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, startX, startY)
            == HudMainMenuSelection::StartRun,
        "top-origin pointer maps to Start Run");
    const float continueX = static_cast<float>(
        wide.continueButton.x + wide.continueButton.width / 2);
    const float continueY = static_cast<float>(
        720 - 1 - (wide.continueButton.y + wide.continueButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, continueX, continueY)
            == HudMainMenuSelection::ContinueCampaign,
        "top-origin pointer maps to Continue Campaign");
    const float campaignX = static_cast<float>(
        wide.campaignButton.x + wide.campaignButton.width / 2);
    const float campaignY = static_cast<float>(
        720 - 1 - (wide.campaignButton.y + wide.campaignButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, campaignX, campaignY)
            == HudMainMenuSelection::Campaign,
        "top-origin pointer maps to Campaign");
    const float speedrunX = static_cast<float>(
        wide.speedrunButton.x + wide.speedrunButton.width / 2);
    const float speedrunY = static_cast<float>(
        720 - 1 - (wide.speedrunButton.y + wide.speedrunButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, speedrunX, speedrunY)
            == HudMainMenuSelection::Speedrun,
        "top-origin pointer maps to Speedrun");
    const float bestSeedX = static_cast<float>(
        wide.bestSeedButton.x + wide.bestSeedButton.width / 2);
    const float bestSeedY = static_cast<float>(
        720 - 1 - (wide.bestSeedButton.y + wide.bestSeedButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, bestSeedX, bestSeedY)
            == HudMainMenuSelection::BestSeed,
        "top-origin pointer maps to Best Seed");
    const float settingsX = static_cast<float>(
        wide.settingsButton.x + wide.settingsButton.width / 2);
    const float settingsY = static_cast<float>(
        720 - 1 - (wide.settingsButton.y + wide.settingsButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, settingsX, settingsY)
            == HudMainMenuSelection::Settings,
        "top-origin pointer maps to Settings");
    const float controlsX = static_cast<float>(
        wide.controlsButton.x + wide.controlsButton.width / 2);
    const float controlsY = static_cast<float>(
        720 - 1 - (wide.controlsButton.y + wide.controlsButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, controlsX, controlsY)
            == HudMainMenuSelection::Controls,
        "top-origin pointer maps to Controls");
    expect(mainMenuSelectionAt(1280, 720, exitX, exitY)
            == HudMainMenuSelection::ExitGame,
        "top-origin pointer maps to Exit Game");
    expect(!mainMenuSelectionAt(1280, 720, 0.0F, 0.0F),
        "pointer outside buttons has no selection");

    const MainMenuLayout compact = makeMainMenuLayout(320, 200);
    expect(compact.scale == 1, "compact menu uses base bitmap scale");
    expect(compact.panel.x >= 0 && compact.panel.y >= 0
            && compact.panel.x + compact.panel.width <= 320
            && compact.panel.y + compact.panel.height <= 200,
        "compact menu remains within the framebuffer");

    const MainMenuLayout tiny = makeMainMenuLayout(12, 12);
    expect(tiny.panel.contains(tiny.startButton.x, tiny.startButton.y)
            && tiny.panel.contains(
                tiny.continueButton.x, tiny.continueButton.y)
            && tiny.panel.contains(
                tiny.campaignButton.x, tiny.campaignButton.y)
            && tiny.panel.contains(
                tiny.speedrunButton.x, tiny.speedrunButton.y)
            && tiny.panel.contains(
                tiny.bestSeedButton.x, tiny.bestSeedButton.y)
            && tiny.panel.contains(tiny.settingsButton.x, tiny.settingsButton.y)
            && tiny.panel.contains(tiny.controlsButton.x, tiny.controlsButton.y)
            && tiny.panel.contains(tiny.exitButton.x, tiny.exitButton.y),
        "tiny menu clamps all hit targets inside the framebuffer");
}

void testPauseMenuLayout()
{
    using namespace gdp::game::visual;

    const PauseMenuLayout wide = makePauseMenuLayout(1280, 720);
    expect(wide.scale == 2, "wide pause menu uses doubled bitmap scale");
    expect(wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "wide pause menu panel stays within the framebuffer");
    expect(wide.panel.contains(wide.resumeButton.x, wide.resumeButton.y)
            && wide.panel.contains(wide.restartButton.x, wide.restartButton.y)
            && wide.panel.contains(wide.settingsButton.x, wide.settingsButton.y)
            && wide.panel.contains(wide.controlsButton.x, wide.controlsButton.y)
            && wide.panel.contains(wide.mainMenuButton.x, wide.mainMenuButton.y)
            && wide.panel.contains(wide.exitButton.x, wide.exitButton.y),
        "pause menu hit targets stay inside the panel");

    const auto pointerX = [](const UiRect& button) {
        return static_cast<float>(button.x + button.width / 2);
    };
    const auto pointerY = [](const UiRect& button) {
        return static_cast<float>(
            720 - 1 - (button.y + button.height / 2));
    };
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.resumeButton), pointerY(wide.resumeButton))
            == HudPauseSelection::Resume,
        "top-origin pointer maps to Resume");
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.restartButton), pointerY(wide.restartButton))
            == HudPauseSelection::RestartRun,
        "top-origin pointer maps to Restart Run");
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.settingsButton), pointerY(wide.settingsButton))
            == HudPauseSelection::Settings,
        "top-origin pointer maps to pause Settings");
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.controlsButton), pointerY(wide.controlsButton))
            == HudPauseSelection::Controls,
        "top-origin pointer maps to pause Controls");
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.mainMenuButton), pointerY(wide.mainMenuButton))
            == HudPauseSelection::MainMenu,
        "top-origin pointer maps to pause Main Menu");
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.exitButton), pointerY(wide.exitButton))
            == HudPauseSelection::ExitGame,
        "top-origin pointer maps to Exit Game");
    expect(!pauseMenuSelectionAt(1280, 720, 0.0F, 0.0F),
        "pointer outside pause buttons has no selection");

    const PauseMenuLayout compact = makePauseMenuLayout(320, 200);
    expect(compact.scale == 1,
        "compact pause menu uses base bitmap scale");
    expect(compact.panel.x >= 0 && compact.panel.y >= 0
            && compact.panel.x + compact.panel.width <= 320
            && compact.panel.y + compact.panel.height <= 200,
        "compact pause menu remains within the framebuffer");

    const PauseMenuLayout tiny = makePauseMenuLayout(12, 12);
    expect(tiny.panel.contains(tiny.resumeButton.x, tiny.resumeButton.y)
            && tiny.panel.contains(tiny.restartButton.x, tiny.restartButton.y)
            && tiny.panel.contains(tiny.settingsButton.x, tiny.settingsButton.y)
            && tiny.panel.contains(tiny.controlsButton.x, tiny.controlsButton.y)
            && tiny.panel.contains(tiny.mainMenuButton.x, tiny.mainMenuButton.y)
            && tiny.panel.contains(tiny.exitButton.x, tiny.exitButton.y),
        "tiny pause menu clamps all hit targets inside the framebuffer");
}

void testControlsMenuLayout()
{
    using namespace gdp::game::visual;
    for (const auto& [width, height] :
        {std::pair{1280, 720}, std::pair{320, 200}}) {
        const ControlsMenuLayout layout = makeControlsMenuLayout(width, height);
        expect(layout.panel.contains(layout.backButton.x,
                   layout.backButton.y),
            "Controls Back button remains inside its panel");
        const float pointerX = static_cast<float>(
            layout.backButton.x + layout.backButton.width / 2);
        const float pointerY = static_cast<float>(height - 1
            - (layout.backButton.y + layout.backButton.height / 2));
        expect(controlsMenuBackAt(width, height, pointerX, pointerY),
            "Controls Back button is clickable at framebuffer scale");
        expect(!controlsMenuBackAt(width, height, 0.0F, 0.0F),
            "Controls ignores clicks outside Back");
    }
    expect(!controlsMenuBackAt(0, 0, 0.0F, 0.0F),
        "Controls rejects invalid dimensions");
    const ControlsMenuLayout tiny = makeControlsMenuLayout(12, 12);
    expect(tiny.panel.contains(tiny.backButton.x, tiny.backButton.y),
        "tiny Controls Back target is clamped inside the panel");
}

void testSettingsMenuLayout()
{
    using namespace gdp::game::visual;
    const SettingsMenuLayout wide = makeSettingsMenuLayout(1280, 720);
    expect(wide.scale == 2, "wide settings menu uses doubled bitmap scale");
    expect(wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "settings panel stays within the framebuffer");
    for (std::size_t index = 0U; index < wide.rows.size(); ++index) {
        const UiRect row = wide.rows[index];
        expect(wide.panel.contains(row.x, row.y),
            "settings row starts inside the panel");
        const float pointerX = static_cast<float>(row.x + row.width / 2);
        const float pointerY = static_cast<float>(
            720 - 1 - (row.y + row.height / 2));
        expect(settingsMenuSelectionAt(1280, 720, pointerX, pointerY)
                == static_cast<HudSettingsSelection>(index),
            "top-origin pointer maps to each settings row");
        if (index < static_cast<std::size_t>(HudSettingsSelection::Back)) {
            const UiRect decrement = wide.decrementButtons[index];
            const UiRect increment = wide.incrementButtons[index];
            expect(row.contains(decrement.x, decrement.y)
                    && row.contains(increment.x, increment.y),
                "settings arrow hit targets stay inside their row");
            const float decrementX
                = static_cast<float>(decrement.x + decrement.width / 2);
            const float decrementY = static_cast<float>(
                720 - 1 - (decrement.y + decrement.height / 2));
            const float incrementX
                = static_cast<float>(increment.x + increment.width / 2);
            const float incrementY = static_cast<float>(
                720 - 1 - (increment.y + increment.height / 2));
            expect(settingsMenuAdjustmentDirectionAt(
                       1280, 720, decrementX, decrementY)
                    == -1,
                "left settings arrow always decrements");
            expect(settingsMenuAdjustmentDirectionAt(
                       1280, 720, incrementX, incrementY)
                    == 1,
                "right settings arrow always increments");
            expect(!settingsMenuAdjustmentDirectionAt(
                1280, 720, static_cast<float>(row.x + 4), pointerY),
                "clicking a settings label selects without changing its value");
        }
    }
    expect(!settingsMenuSelectionAt(1280, 720, 0.0F, 0.0F),
        "pointer outside settings rows has no selection");
    expect(!settingsMenuAdjustmentDirectionAt(1280, 720, 0.0F, 0.0F)
            && !settingsMenuAdjustmentDirectionAt(0, 720, 1.0F, 1.0F),
        "pointer outside arrow buttons cannot adjust settings");

    const SettingsMenuLayout medium = makeSettingsMenuLayout(640, 360);
    expect(medium.scale == 1,
        "medium settings menu keeps all twelve rows at base bitmap scale");
    for (std::size_t index = 1U; index < medium.rows.size(); ++index) {
        expect(medium.rows[index].y + medium.rows[index].height
                <= medium.rows[index - 1U].y,
            "medium settings rows never overlap");
    }

    const SettingsMenuLayout compact = makeSettingsMenuLayout(320, 200);
    expect(compact.scale == 1
            && compact.panel.x + compact.panel.width <= 320
            && compact.panel.y + compact.panel.height <= 200,
        "compact settings menu remains within the framebuffer");
    for (std::size_t index = 1U; index < compact.rows.size(); ++index) {
        expect(compact.rows[index].y + compact.rows[index].height
                <= compact.rows[index - 1U].y,
            "compact settings rows remain separated when space permits");
    }
    const SettingsMenuLayout tiny = makeSettingsMenuLayout(12, 12);
    for (const UiRect row : tiny.rows) {
        expect(tiny.panel.contains(row.x, row.y),
            "tiny settings menu clamps rows inside the framebuffer");
    }
}

void testResultMenuLayout()
{
    using namespace gdp::game::visual;

    const ResultMenuLayout wide = makeResultMenuLayout(1280, 720);
    expect(wide.scale == 2, "wide result menu uses doubled bitmap scale");
    expect(wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "wide result menu panel stays within the framebuffer");
    expect(wide.panel.contains(
               wide.playAgainButton.x, wide.playAgainButton.y)
            && wide.panel.contains(
                wide.replaySeedButton.x, wide.replaySeedButton.y)
            && wide.panel.contains(wide.mainMenuButton.x, wide.mainMenuButton.y)
            && wide.panel.contains(wide.exitButton.x, wide.exitButton.y),
        "result menu hit targets stay inside the panel");
    expect(wide.exitButton.y + wide.exitButton.height
                <= wide.mainMenuButton.y
            && wide.mainMenuButton.y + wide.mainMenuButton.height
                <= wide.replaySeedButton.y
            && wide.replaySeedButton.y + wide.replaySeedButton.height
                <= wide.playAgainButton.y,
        "all result menu hit targets remain visually separated");

    const auto pointerX = [](const UiRect& button) {
        return static_cast<float>(button.x + button.width / 2);
    };
    const auto pointerY = [](const UiRect& button) {
        return static_cast<float>(
            720 - 1 - (button.y + button.height / 2));
    };
    expect(resultMenuSelectionAt(1280, 720,
               pointerX(wide.playAgainButton), pointerY(wide.playAgainButton))
            == HudResultMenuSelection::PlayAgain,
        "top-origin pointer maps to Play Again");
    expect(resultMenuSelectionAt(1280, 720,
               pointerX(wide.replaySeedButton), pointerY(wide.replaySeedButton))
            == HudResultMenuSelection::ReplaySeed,
        "top-origin pointer maps to Replay Seed");
    expect(resultMenuSelectionAt(1280, 720,
               pointerX(wide.mainMenuButton), pointerY(wide.mainMenuButton))
            == HudResultMenuSelection::MainMenu,
        "top-origin pointer maps to result Main Menu");
    expect(resultMenuSelectionAt(1280, 720,
               pointerX(wide.exitButton), pointerY(wide.exitButton))
            == HudResultMenuSelection::ExitGame,
        "top-origin pointer maps to result Exit Game");
    expect(!resultMenuSelectionAt(1280, 720, 0.0F, 0.0F),
        "pointer outside result buttons has no selection");

    const ResultMenuLayout compact = makeResultMenuLayout(320, 200);
    expect(compact.scale == 1,
        "compact result menu uses base bitmap scale");
    expect(compact.panel.x >= 0 && compact.panel.y >= 0
            && compact.panel.x + compact.panel.width <= 320
            && compact.panel.y + compact.panel.height <= 200,
        "compact result menu remains within the framebuffer");

    const ResultMenuLayout tiny = makeResultMenuLayout(12, 12);
    expect(tiny.panel.contains(
               tiny.playAgainButton.x, tiny.playAgainButton.y)
            && tiny.panel.contains(
                tiny.replaySeedButton.x, tiny.replaySeedButton.y)
            && tiny.panel.contains(tiny.mainMenuButton.x, tiny.mainMenuButton.y)
            && tiny.panel.contains(tiny.exitButton.x, tiny.exitButton.y),
        "tiny result menu clamps all hit targets inside the framebuffer");
}

void testExitConfirmationLayout()
{
    using namespace gdp::game::visual;

    const ExitConfirmationLayout wide
        = makeExitConfirmationLayout(1280, 720);
    expect(wide.scale == 2
            && wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "wide exit confirmation stays inside the framebuffer");
    expect(wide.panel.contains(wide.cancelButton.x, wide.cancelButton.y)
            && wide.panel.contains(wide.exitButton.x, wide.exitButton.y)
            && wide.cancelButton.x + wide.cancelButton.width
                <= wide.exitButton.x,
        "confirmation exposes distinct Cancel and Exit hit targets");
    const auto pointerX = [](const UiRect& button) {
        return static_cast<float>(button.x + button.width / 2);
    };
    const auto pointerY = [](const UiRect& button) {
        return static_cast<float>(
            720 - 1 - (button.y + button.height / 2));
    };
    expect(exitConfirmationSelectionAt(1280, 720,
               pointerX(wide.cancelButton), pointerY(wide.cancelButton))
            == HudExitConfirmationSelection::Cancel,
        "top-origin pointer maps to the safe Cancel choice");
    expect(exitConfirmationSelectionAt(1280, 720,
               pointerX(wide.exitButton), pointerY(wide.exitButton))
            == HudExitConfirmationSelection::ExitGame,
        "top-origin pointer maps to the explicit Exit choice");
    expect(!exitConfirmationSelectionAt(1280, 720, 0.0F, 0.0F)
            && !exitConfirmationSelectionAt(0, 720, 1.0F, 1.0F),
        "invalid or external confirmation pointers are ignored");

    const ExitConfirmationLayout compact
        = makeExitConfirmationLayout(320, 200);
    expect(compact.scale == 1
            && compact.panel.x + compact.panel.width <= 320
            && compact.panel.y + compact.panel.height <= 200,
        "compact exit confirmation remains within the framebuffer");
    const ExitConfirmationLayout tiny
        = makeExitConfirmationLayout(12, 12);
    expect(tiny.panel.contains(tiny.cancelButton.x, tiny.cancelButton.y)
            && tiny.panel.contains(tiny.exitButton.x, tiny.exitButton.y),
        "tiny confirmation clamps both choices inside its panel");
}

void testRunTimeFormatting()
{
    using gdp::game::visual::runTimeText;

    expect(runTimeText(0U) == "00:00.00"
            && runTimeText(1U) == "00:00.01"
            && runTimeText(59U) == "00:00.98"
            && runTimeText(60U) == "00:01.00"
            && runTimeText(216'000U) == "01:00:00.00",
        "run timer formatting is deterministic across tick boundaries");
}

void testEnemyRootAnimation()
{
    using namespace gdp::game::visual;

    const EnemyRootPose inactive = sampleEnemyRootPose(
        EnemyAnimationState::Walk, 30U, 111U, false);
    expect(inactive == EnemyRootPose{},
        "dormant enemy previews remain visually static");

    const EnemyRootPose dead = sampleEnemyRootPose(
        EnemyAnimationState::Dead, 999U, 111U);
    expect(dead.offset == wkc::core::Vec3{0.0F, 0.18F, 0.0F}
            && dead.scale == wkc::core::Vec3{1.0F, 1.0F, 1.0F}
            && (std::abs(dead.rotationDegrees.x) > 1.0F
                || std::abs(dead.rotationDegrees.z) > 1.0F),
        "dead Worker keeps rigid proportions with a seeded full-plane fall");
    constexpr float degreesToRadians
        = 3.14159265358979323846F / 180.0F;
    const auto fallDirection = [](const EnemyRootPose& pose) {
        constexpr float toRadians
            = 3.14159265358979323846F / 180.0F;
        const float pitch = pose.rotationDegrees.x * toRadians;
        const float roll = pose.rotationDegrees.z * toRadians;
        return wkc::core::Vec3{
            -std::sin(roll) * std::cos(pitch),
            std::cos(roll) * std::cos(pitch),
            std::sin(pitch),
        };
    };
    bool fallsLeft = false;
    bool fallsRight = false;
    bool fallsForward = false;
    bool fallsBackward = false;
    bool fallsDiagonally = false;
    for (wkc::core::Seed seed = 1U; seed <= 128U; ++seed) {
        const EnemyRootPose seededDeath = sampleEnemyRootPose(
            EnemyAnimationState::Dead, 999U, seed);
        const wkc::core::Vec3 direction = fallDirection(seededDeath);
        fallsLeft = fallsLeft || direction.x < -0.70F;
        fallsRight = fallsRight || direction.x > 0.70F;
        fallsForward = fallsForward || direction.z > 0.70F;
        fallsBackward = fallsBackward || direction.z < -0.70F;
        fallsDiagonally = fallsDiagonally
            || (std::abs(direction.x) > 0.45F
                && std::abs(direction.z) > 0.45F);
        expect(std::abs(direction.y - std::cos(88.0F * degreesToRadians))
                < 0.0001F,
            "every seeded Worker death finishes at the same grounded tilt");
    }
    expect(fallsLeft && fallsRight && fallsForward && fallsBackward
            && fallsDiagonally
            && dead == sampleEnemyRootPose(
                EnemyAnimationState::Dead, 0U, 111U),
        "death direction deterministically covers the full ground plane");
    const EnemyRootPose robotDead = sampleEnemyRootPose(
        EnemyAnimationState::Dead, 999U, 111U, true,
        EnemyVisualRole::IndustrialRobot);
    expect(robotDead.offset.y == 0.26F
            && fallDirection(robotDead).y < 0.04F,
        "IndustrialRobot settles with a heavier restrained root response");

    EnemyAnimationTransition transition;
    advanceEnemyAnimationTransition(transition, EnemyAnimationState::Idle);
    expect(transition.initialized
            && transition.previousState == EnemyAnimationState::Idle
            && transition.currentState == EnemyAnimationState::Idle
            && enemyAnimationTransitionBlend(transition) == 1.0F,
        "first observed animation state starts without a synthetic transition");
    EnemyAnimationTransition alertTransition;
    advanceEnemyAnimationTransition(
        alertTransition, EnemyAnimationState::Idle);
    advanceEnemyAnimationTransition(
        alertTransition, EnemyAnimationState::Alert);
    expect(alertTransition.previousState == EnemyAnimationState::Idle
            && alertTransition.currentState == EnemyAnimationState::Alert
            && alertTransition.durationTicks == 3U,
        "Alert raises the readiness pose with a fast fixed-tick blend");
    advanceEnemyAnimationTransition(
        alertTransition, EnemyAnimationState::Walk);
    expect(alertTransition.previousState == EnemyAnimationState::Alert
            && alertTransition.durationTicks == 4U,
        "leaving Alert blends into investigation without a visual snap");
    EnemyAnimationTransition recoverTransition;
    advanceEnemyAnimationTransition(
        recoverTransition, EnemyAnimationState::Walk);
    advanceEnemyAnimationTransition(
        recoverTransition, EnemyAnimationState::Recover);
    expect(recoverTransition.previousState == EnemyAnimationState::Walk
            && recoverTransition.currentState == EnemyAnimationState::Recover
            && recoverTransition.durationTicks == 5U,
        "Recover enters its scanning pose with a restrained blend");
    advanceEnemyAnimationTransition(
        recoverTransition, EnemyAnimationState::Idle);
    expect(recoverTransition.previousState == EnemyAnimationState::Recover
            && recoverTransition.durationTicks == 5U,
        "Recover returns to Idle without snapping");
    advanceEnemyAnimationTransition(transition, EnemyAnimationState::Walk);
    expect(transition.previousState == EnemyAnimationState::Idle
            && transition.currentState == EnemyAnimationState::Walk
            && transition.durationTicks == 6U
            && enemyAnimationTransitionBlend(transition) > 0.07F
            && enemyAnimationTransitionBlend(transition) < 0.08F,
        "Idle to Walk begins a short fixed-tick blend");
    advanceEnemyAnimationTransition(transition, EnemyAnimationState::Hit);
    expect(transition.previousState == EnemyAnimationState::Walk
            && transition.durationTicks == 3U,
        "Hit interrupts the current target with a faster visual response");
    advanceEnemyAnimationTransition(transition, EnemyAnimationState::Dead);
    expect(transition.previousState == EnemyAnimationState::Hit
            && transition.durationTicks == 48U
            && enemyAnimationTransitionBlend(transition) > 0.020F
            && enemyAnimationTransitionBlend(transition) < 0.022F,
        "Death starts a longer deterministic settling transition");
    for (std::uint32_t tick = 1U; tick < 48U; ++tick) {
        advanceEnemyAnimationTransition(transition, EnemyAnimationState::Dead);
    }
    expect(enemyAnimationTransitionBlend(transition) == 1.0F,
        "death settling clamps exactly at its terminal pose");
    EnemyAnimationTransition robotTransition;
    advanceEnemyAnimationTransition(
        robotTransition, EnemyAnimationState::Attack,
        EnemyVisualRole::IndustrialRobot);
    advanceEnemyAnimationTransition(
        robotTransition, EnemyAnimationState::Dead,
        EnemyVisualRole::IndustrialRobot);
    EnemyAnimationTransition guardTransition;
    advanceEnemyAnimationTransition(
        guardTransition, EnemyAnimationState::Attack, EnemyVisualRole::Guard);
    advanceEnemyAnimationTransition(
        guardTransition, EnemyAnimationState::Dead, EnemyVisualRole::Guard);
    EnemyAnimationTransition officerTransition;
    advanceEnemyAnimationTransition(
        officerTransition, EnemyAnimationState::Attack, EnemyVisualRole::Officer);
    advanceEnemyAnimationTransition(
        officerTransition, EnemyAnimationState::Dead, EnemyVisualRole::Officer);
    EnemyAnimationTransition engineerTransition;
    advanceEnemyAnimationTransition(
        engineerTransition, EnemyAnimationState::Attack, EnemyVisualRole::Engineer);
    advanceEnemyAnimationTransition(
        engineerTransition, EnemyAnimationState::Dead, EnemyVisualRole::Engineer);
    expect(guardTransition.durationTicks == 50U
            && officerTransition.durationTicks == 52U
            && engineerTransition.durationTicks == 48U
            && robotTransition.durationTicks == 66U,
        "each enemy role keeps enough time for its complete grounded settling");

    const EnemyRootPose halfFall = sampleEnemyRagdollRootPose(
        EnemyRootPose{}, 111U, EnemyVisualRole::Worker, 0.5F);
    expect(halfFall.offset.y > 0.10F && halfFall.offset.y < 0.13F
            && halfFall.scale == wkc::core::Vec3{1.0F, 1.0F, 1.0F}
            && halfFall != dead
            && (halfFall.offset.x != 0.0F || halfFall.offset.z != 0.0F),
        "ragdoll root accelerates after support is lost and carries momentum");
    expect(sampleEnemyRagdollRootPose(
               EnemyRootPose{}, 111U, EnemyVisualRole::Worker, 0.08F)
            == EnemyRootPose{},
        "root briefly retains inertia while the legs begin to give way");
    expect(sampleEnemyRagdollRootPose(
               EnemyRootPose{}, 111U, EnemyVisualRole::Worker, 0.0F)
            == EnemyRootPose{}
            && sampleEnemyRagdollRootPose(
                EnemyRootPose{}, 111U, EnemyVisualRole::Worker, 1.0F) == dead,
        "ragdoll root preserves its endpoints exactly");
    expect(blendEnemyRootPoses(EnemyRootPose{}, dead,
               std::numeric_limits<float>::quiet_NaN()) == dead,
        "non-finite transition blends safely select the current pose");

    const EnemyPartPose deadHead = sampleEnemyPartPose(
        EnemyAnimationState::Dead, 0U, 111U, 1U, 8U);
    const EnemyPartPose deadLeftArm = sampleEnemyPartPose(
        EnemyAnimationState::Dead, 0U, 111U, 2U, 8U);
    const EnemyPartPose deadRightArm = sampleEnemyPartPose(
        EnemyAnimationState::Dead, 0U, 111U, 3U, 8U);
    const EnemyPartPose deadRobotLeftArm = sampleEnemyPartPose(
        EnemyAnimationState::Dead, 0U, 111U, 2U, 9U, 0.0F, true,
        EnemyVisualRole::IndustrialRobot);
    expect(deadHead != EnemyPartPose{}
            && std::abs(deadLeftArm.rotationDegrees.z) > 30.0F
            && std::abs(deadRightArm.rotationDegrees.z) > 30.0F
            && deadLeftArm.rotationDegrees.z
                * deadRightArm.rotationDegrees.z < 0.0F,
        "terminal ragdoll separates head and arms around their joint pivots");
    expect(std::abs(deadRobotLeftArm.rotationDegrees.z)
            < std::abs(deadLeftArm.rotationDegrees.z) * 0.6F,
        "robot limbs retain visibly greater mass than Worker limbs");
    const EnemyPartPose earlyRagdollLeg = sampleEnemyRagdollPartPose(
        EnemyPartPose{}, 111U, EnemyVisualRole::Worker, 4U, 8U, 0.12F);
    const EnemyPartPose earlyRagdollHead = sampleEnemyRagdollPartPose(
        EnemyPartPose{}, 111U, EnemyVisualRole::Worker, 1U, 8U, 0.12F);
    expect(earlyRagdollLeg != EnemyPartPose{}
            && earlyRagdollHead == EnemyPartPose{},
        "legs lose support before the head follows the falling torso");
    const EnemyPartPose previousAttackArm = sampleEnemyPartPose(
        EnemyAnimationState::Attack, 6U, 111U, 2U, 8U);
    const EnemyPartPose halfRagdollArm = sampleEnemyRagdollPartPose(
        previousAttackArm, 111U, EnemyVisualRole::Worker, 2U, 8U, 0.5F);
    expect(halfRagdollArm != previousAttackArm
            && halfRagdollArm != deadLeftArm
            && halfRagdollArm == sampleEnemyRagdollPartPose(
                previousAttackArm, 111U, EnemyVisualRole::Worker, 2U, 8U, 0.5F),
        "ragdoll limb motion is staged, damped and deterministic");
    expect(sampleEnemyRagdollPartPose(
               previousAttackArm, 111U, EnemyVisualRole::Worker, 2U, 8U, 0.0F)
            == previousAttackArm
            && sampleEnemyRagdollPartPose(
                previousAttackArm, 111U, EnemyVisualRole::Worker, 2U, 8U, 1.0F)
                == deadLeftArm,
        "ragdoll limbs preserve their animated and settled endpoints");
    const EnemyPartPose previousAttackTool = sampleEnemyPartPose(
        EnemyAnimationState::Attack, 6U, 111U, 7U, 8U);
    const EnemyPartPose deadTool = sampleEnemyPartPose(
        EnemyAnimationState::Dead, 0U, 111U, 7U, 8U);
    expect(sampleEnemyRagdollPartPose(
               previousAttackTool, 111U, EnemyVisualRole::Worker,
               7U, 8U, 0.99F) != deadTool,
        "the final loose part is still settling before Death completes");
    const auto everyPartSettles = [](const EnemyVisualRole role,
                                      const std::uint32_t partCount) {
        for (std::uint32_t part = 0U; part < partCount; ++part) {
            const EnemyPartPose previousPart = sampleEnemyPartPose(
                EnemyAnimationState::Attack, 6U, 111U, part, partCount,
                0.0F, true, role);
            const EnemyPartPose terminalPart = sampleEnemyPartPose(
                EnemyAnimationState::Dead, 0U, 111U, part, partCount,
                0.0F, true, role);
            if (sampleEnemyRagdollPartPose(
                    previousPart, 111U, role, part, partCount, 1.0F)
                != terminalPart) {
                return false;
            }
        }
        return true;
    };
    expect(everyPartSettles(EnemyVisualRole::Worker, 8U)
            && everyPartSettles(EnemyVisualRole::Guard, 8U)
            && everyPartSettles(EnemyVisualRole::Officer, 8U)
            && everyPartSettles(EnemyVisualRole::Engineer, 8U)
            && everyPartSettles(EnemyVisualRole::IndustrialRobot, 9U),
        "Death completes only after every part of every role is grounded");

    const EnemyRootPose hit = sampleEnemyRootPose(
        EnemyAnimationState::Hit, 999U, 111U);
    expect(hit == EnemyRootPose{}
            && hit == sampleEnemyRootPose(
                EnemyAnimationState::Hit, 0U, 999U),
        "Hit keeps rigid proportions instead of applying cartoon root compression");

    const EnemyPartPose hitTorso = sampleEnemyPartPose(
        EnemyAnimationState::Hit, 0U, 111U, 0U, 8U, 1.0F);
    const EnemyPartPose fadingHitTorso = sampleEnemyPartPose(
        EnemyAnimationState::Hit, 999U, 222U, 0U, 8U, 0.5F);
    expect(hitTorso.rotationDegrees.x == -3.5F
            && hitTorso.offset.z == -0.014F
            && fadingHitTorso.rotationDegrees.x == -1.75F
            && fadingHitTorso.offset.z == -0.007F,
        "Hit produces a short strength-scaled mechanical recoil at the torso");
    expect(sampleEnemyPartPose(
               EnemyAnimationState::Hit, 0U, 111U, 1U, 8U, 1.0F)
               .rotationDegrees.x == 4.5F
            && sampleEnemyPartPose(
                EnemyAnimationState::Hit, 0U, 111U, 7U, 8U, 1.0F)
                .offset.z == -0.035F,
        "Hit transfers restrained counter-motion to the head and held tool");

    const EnemyPartPose walkLeftArm = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 0U, 0U, 2U, 8U);
    const EnemyPartPose walkRightArm = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 0U, 0U, 3U, 8U);
    const EnemyPartPose walkLeftLeg = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 0U, 0U, 4U, 8U);
    expect(walkLeftArm == EnemyPartPose{}
            && walkRightArm == EnemyPartPose{}
            && walkLeftLeg == EnemyPartPose{},
        "rigid Walk articulation returns to the neutral pose at its cycle boundary");
    const EnemyPartPose peakLeftArm = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 7U, 0U, 2U, 8U);
    const EnemyPartPose peakRightArm = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 7U, 0U, 3U, 8U);
    const EnemyPartPose peakLeftLeg = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 7U, 0U, 4U, 8U);
    expect(peakLeftArm.rotationDegrees.x > 7.0F
            && peakRightArm.rotationDegrees.x < -7.0F
            && peakLeftLeg.rotationDegrees.x < -9.0F,
        "Walk articulates opposing rigid limbs with restrained amplitudes");
    expect(sampleEnemyPartPose(
               EnemyAnimationState::Walk, 7U, 0U, 2U, 8U, 0.0F, false)
            == EnemyPartPose{}
            && sampleEnemyPartPose(
                EnemyAnimationState::Walk, 7U, 0U, 8U, 8U) == EnemyPartPose{},
        "dormant enemies and invalid rigid-part indices remain neutral");

    const EnemyPartPose guardWalk = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 7U, 0U, 2U, 8U, 0.0F, true,
        EnemyVisualRole::Guard);
    const EnemyPartPose officerWalk = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 7U, 0U, 2U, 8U, 0.0F, true,
        EnemyVisualRole::Officer);
    const EnemyPartPose engineerLeftWalk = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 7U, 0U, 2U, 8U, 0.0F, true,
        EnemyVisualRole::Engineer);
    const EnemyPartPose engineerRightWalk = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 7U, 0U, 3U, 8U, 0.0F, true,
        EnemyVisualRole::Engineer);
    const EnemyPartPose robotWalk = sampleEnemyPartPose(
        EnemyAnimationState::Walk, 7U, 0U, 2U, 9U, 0.0F, true,
        EnemyVisualRole::IndustrialRobot);
    expect(peakLeftArm.rotationDegrees.x > guardWalk.rotationDegrees.x
            && guardWalk.rotationDegrees.x > engineerLeftWalk.rotationDegrees.x
            && engineerLeftWalk.rotationDegrees.x > officerWalk.rotationDegrees.x
            && officerWalk.rotationDegrees.x > robotWalk.rotationDegrees.x,
        "enemy roles expose distinct restrained Walk signatures");
    expect(std::abs(engineerRightWalk.rotationDegrees.x)
            > engineerLeftWalk.rotationDegrees.x + 2.0F,
        "Engineer Walk keeps extra motion on the tool-side arm");

    const EnemyPartPose workerAttackTool = sampleEnemyPartPose(
        EnemyAnimationState::Attack, 6U, 0U, 7U, 8U);
    const EnemyPartPose guardAttackTool = sampleEnemyPartPose(
        EnemyAnimationState::Attack, 4U, 0U, 7U, 8U, 0.0F, true,
        EnemyVisualRole::Guard);
    const EnemyPartPose robotHitTorso = sampleEnemyPartPose(
        EnemyAnimationState::Hit, 0U, 0U, 0U, 9U, 1.0F, true,
        EnemyVisualRole::IndustrialRobot);
    expect(workerAttackTool.offset.z > 0.04F
            && guardAttackTool.offset.z < -0.04F,
        "Worker lunges its tool while ranged roles recoil their weapon");
    expect(std::abs(robotHitTorso.rotationDegrees.x)
            < std::abs(hitTorso.rotationDegrees.x) * 0.5F,
        "IndustrialRobot absorbs Hit with a heavier restrained response");

    const EnemyRootPose idle = sampleEnemyRootPose(
        EnemyAnimationState::Idle, 7U, 111U);
    const EnemyRootPose alert = sampleEnemyRootPose(
        EnemyAnimationState::Alert, 7U, 111U);
    const EnemyPartPose alertArm = sampleEnemyPartPose(
        EnemyAnimationState::Alert, 7U, 111U, 2U, 8U);
    const EnemyPartPose alertTool = sampleEnemyPartPose(
        EnemyAnimationState::Alert, 7U, 111U, 7U, 8U);
    const EnemyPartPose robotAlertArm = sampleEnemyPartPose(
        EnemyAnimationState::Alert, 7U, 111U, 2U, 9U, 0.0F, true,
        EnemyVisualRole::IndustrialRobot);
    expect(alert.scale == wkc::core::Vec3{1.0F, 1.0F, 1.0F}
            && alert.offset.z > 0.0F
            && alert.rotationDegrees.x < 0.0F
            && alertArm.rotationDegrees.x < -7.0F
            && alertTool.offset.z > 0.0F
            && std::abs(robotAlertArm.rotationDegrees.x)
                < std::abs(alertArm.rotationDegrees.x) * 0.5F,
        "Alert has a restrained role-weighted readiness pose");
    const EnemyRootPose recover = sampleEnemyRootPose(
        EnemyAnimationState::Recover, 18U, 0U);
    const EnemyPartPose recoverHead = sampleEnemyPartPose(
        EnemyAnimationState::Recover, 18U, 0U, 1U, 8U);
    const EnemyPartPose recoverArm = sampleEnemyPartPose(
        EnemyAnimationState::Recover, 18U, 0U, 2U, 8U);
    const EnemyPartPose robotRecoverHead = sampleEnemyPartPose(
        EnemyAnimationState::Recover, 18U, 0U, 1U, 9U, 0.0F, true,
        EnemyVisualRole::IndustrialRobot);
    expect(recover.scale == wkc::core::Vec3{1.0F, 1.0F, 1.0F}
            && recoverHead.rotationDegrees.y > 13.9F
            && recoverArm.rotationDegrees.x == -2.0F
            && robotRecoverHead.rotationDegrees.y
                < recoverHead.rotationDegrees.y * 0.5F,
        "Recover scans with the head while preserving role-weighted mass");
    expect(idle == sampleEnemyRootPose(
            EnemyAnimationState::Idle, 7U, 111U),
        "enemy root animation is deterministic for the same tick and visual seed");
    expect(idle != sampleEnemyRootPose(
            EnemyAnimationState::Idle, 7U, 112U),
        "visual seeds offset enemy cycles to avoid synchronized groups");

    float maximumIdleLift = 0.0F;
    float maximumWalkLift = 0.0F;
    float maximumAttackReach = 1.0F;
    bool finiteAndGrounded = true;
    for (std::uint64_t tick = 0U; tick < 120U; ++tick) {
        const EnemyRootPose idlePose = sampleEnemyRootPose(
            EnemyAnimationState::Idle, tick, 111U);
        const EnemyRootPose walkPose = sampleEnemyRootPose(
            EnemyAnimationState::Walk, tick, 111U);
        const EnemyRootPose attackPose = sampleEnemyRootPose(
            EnemyAnimationState::Attack, tick, 111U);
        maximumIdleLift = std::max(maximumIdleLift, idlePose.offset.y);
        maximumWalkLift = std::max(maximumWalkLift, walkPose.offset.y);
        maximumAttackReach = std::max(maximumAttackReach, attackPose.scale.z);
        for (const EnemyRootPose pose : {
                 idlePose, walkPose, attackPose, alert, recover, hit}) {
            finiteAndGrounded = finiteAndGrounded
                && std::isfinite(pose.offset.y)
                && std::isfinite(pose.scale.x)
                && std::isfinite(pose.scale.y)
                && std::isfinite(pose.scale.z)
                && pose.offset.y >= 0.0F
                && pose.scale.x > 0.0F
                && pose.scale.y > 0.0F
                && pose.scale.z > 0.0F;
        }
    }
    expect(finiteAndGrounded,
        "all live root poses stay finite, positive and above the floor plane");
    expect(maximumIdleLift <= 0.0121F
            && maximumWalkLift > maximumIdleLift + 0.02F,
        "Walk has a stronger readable cadence than the restrained Idle cycle");
    expect(maximumAttackReach > 1.05F,
        "Attack produces a distinct axial impulse at its peak");
}

void testImpactVisualData()
{
    using namespace gdp::game::visual;
    const wkc::core::Vec3 metal
        = impactVisualColor(ImpactVisualMaterial::Metal, 1.0F);
    const wkc::core::Vec3 composite
        = impactVisualColor(ImpactVisualMaterial::Composite, 1.0F);
    const wkc::core::Vec3 flesh
        = impactVisualColor(ImpactVisualMaterial::Flesh, 1.0F);
    expect(metal != composite && metal != flesh && composite != flesh,
        "impact materials retain distinct visual signatures");
    expect(impactVisualSize(0.0F) < impactVisualSize(1.0F)
            && impactVisualSize(-1.0F) == impactVisualSize(0.0F)
            && impactVisualSize(std::numeric_limits<float>::quiet_NaN())
                == impactVisualSize(0.0F),
        "impact size fades monotonically and sanitizes invalid strength");
    expect(impactVisualColor(ImpactVisualMaterial::Metal,
               std::numeric_limits<float>::quiet_NaN())
            == impactVisualColor(ImpactVisualMaterial::Metal, 0.0F),
        "impact color sanitizes non-finite lifetime strength");
}

void testWeaponViewmodelPose()
{
    using namespace gdp::game::visual;
    using wkc::core::nearlyEqual;

    const WeaponViewmodelPose idle
        = sampleWeaponViewmodelPose({.visible = true});
    expect(idle == WeaponViewmodelPose{},
        "idle weapon viewmodel keeps its authored camera anchor");

    const WeaponViewmodelPose pistolRecoil = sampleWeaponViewmodelPose({
        .visible = true,
        .role = HudWeaponRole::Pistol,
        .recoilStrength = 1.0F,
    });
    const WeaponViewmodelPose carbineRecoil = sampleWeaponViewmodelPose({
        .visible = true,
        .role = HudWeaponRole::Carbine,
        .recoilStrength = 1.0F,
    });
    expect(pistolRecoil.offset.z < 0.0F
            && pistolRecoil.rotationDegrees.x < 0.0F,
        "shot recoil moves and pitches the viewmodel toward the player");
    expect(std::abs(carbineRecoil.offset.z) < std::abs(pistolRecoil.offset.z)
            && std::abs(carbineRecoil.rotationDegrees.x)
                < std::abs(pistolRecoil.rotationDegrees.x),
        "carbine mass produces a more restrained recoil impulse than the pistol");

    const WeaponViewmodelPose reloadPeak = sampleWeaponViewmodelPose({
        .visible = true,
        .reloadProgress = 0.5F,
    });
    const WeaponViewmodelPose reloadEnd = sampleWeaponViewmodelPose({
        .visible = true,
        .reloadProgress = 1.0F,
    });
    expect(reloadPeak.offset.x < 0.0F && reloadPeak.offset.y > 0.0F
            && reloadPeak.rotationDegrees.z > 30.0F,
        "reload raises the magazine into view and follows a readable side arc");
    expect(nearlyEqual(reloadEnd.offset.x, idle.offset.x, 1.0e-5F)
            && nearlyEqual(reloadEnd.offset.y, idle.offset.y, 1.0e-5F)
            && nearlyEqual(reloadEnd.rotationDegrees.z,
                idle.rotationDegrees.z, 1.0e-4F),
        "reload arc returns smoothly to the idle anchor");

    const WeaponViewmodelPose equip = sampleWeaponViewmodelPose({
        .visible = true,
        .equipStrength = 1.0F,
    });
    expect(equip.offset.y < -0.3F,
        "equip animation introduces the weapon from below the frame");

    const WeaponViewmodelPose knifeSwing = sampleWeaponViewmodelPose({
        .visible = true,
        .role = HudWeaponRole::Knife,
        .meleeStrength = 0.5F,
    });
    expect(knifeSwing.offset.x < -0.15F
            && knifeSwing.offset.z > 0.15F
            && knifeSwing.rotationDegrees.z < -50.0F,
        "Knife swing follows a readable forward diagonal arc");

    const WeaponViewmodelPose movementPose = sampleWeaponViewmodelPose({
        .visible = true,
        .crouchStrength = 1.0F,
        .airborneStrength = 1.0F,
    });
    expect(movementPose.offset.y < -0.06F
            && movementPose.offset.z < -0.03F
            && movementPose.rotationDegrees.x > 6.0F,
        "crouch and airborne movement keep a restrained first-person weapon response");

    const WeaponViewmodelPose invalid = sampleWeaponViewmodelPose({
        .visible = true,
        .recoilStrength = std::numeric_limits<float>::quiet_NaN(),
        .reloadProgress = std::numeric_limits<float>::infinity(),
        .equipStrength = -4.0F,
        .meleeStrength = std::numeric_limits<float>::quiet_NaN(),
        .crouchStrength = std::numeric_limits<float>::infinity(),
        .airborneStrength = -2.0F,
    });
    expect(invalid == idle,
        "weapon pose sanitizes non-finite and out-of-range animation input");
}

void testSniperScopePresentation()
{
    using namespace gdp::game::visual;
    WeaponViewmodelRenderData model{.visible = true, .role = HudWeaponRole::Sniper};
    expect(sniperScopeStrength(model) == 0.0F, "hip fire has no scope overlay");
    model.aimStrength = 0.5F;
    expect(sniperScopeStrength(model) == 0.5F, "scope blends deterministically at half aim");
    model.aimStrength = 1.0F;
    expect(sniperScopeStrength(model) == 1.0F, "full sniper aim activates optical view");
    for (const auto role : {HudWeaponRole::Pistol, HudWeaponRole::Carbine,
             HudWeaponRole::Knife, HudWeaponRole::Shotgun, HudWeaponRole::SubmachineGun}) {
        model.role = role;
        expect(sniperScopeStrength(model) == 0.0F, "other weapons never inherit sniper scope");
    }
    model.role = HudWeaponRole::Sniper;
    model.reloadProgress = 0.2F;
    expect(sniperScopeStrength(model) == 0.0F, "reload suppresses the optical view");
    model.reloadProgress = 0.0F;
    model.equipStrength = 0.2F;
    expect(sniperScopeStrength(model) == 0.0F, "equip suppresses the optical view");
    model.equipStrength = 0.0F;
    model.visible = false;
    expect(sniperScopeStrength(model) == 0.0F, "hidden or dead player cannot leave scope active");
    model.visible = true;
    for (const float strength : {-1.0F, std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity()}) {
        model.aimStrength = strength;
        expect(sniperScopeStrength(model) == 0.0F, "invalid aim input does not create an overlay");
    }
    model.aimStrength = 2.0F;
    expect(sniperScopeStrength(model) == 1.0F, "finite aim clamps to full scope");
}

void testDroppedWeaponPose()
{
    using namespace gdp::game::visual;
    const DroppedWeaponPose beginning = sampleDroppedWeaponPose(
        123U, PickupVisualRole::Weapon, 0.0F);
    const DroppedWeaponPose middle = sampleDroppedWeaponPose(
        123U, PickupVisualRole::Weapon, 0.5F);
    const DroppedWeaponPose settled = sampleDroppedWeaponPose(
        123U, PickupVisualRole::Weapon, 1.0F);
    expect(beginning.height > middle.height && middle.height > settled.height,
        "dropped weapon falls toward the floor without changing pickup position");
    expect(std::abs(settled.rollDegrees) == 90.0F,
        "dropped weapon rests on its side");
    expect(settled == sampleDroppedWeaponPose(
            123U, PickupVisualRole::Weapon, 1.0F),
        "dropped pose is deterministic per pickup identity");
    expect(sampleDroppedWeaponPose(124U, PickupVisualRole::Weapon, 1.0F)
            != settled,
        "different pickups may settle at different angles");
    expect(sampleDroppedWeaponPose(123U, PickupVisualRole::DroppedPistol, 1.0F)
            .height < settled.height,
        "pistol uses its own floor contact height");
    expect(sampleDroppedWeaponPose(123U, PickupVisualRole::Weapon,
            std::numeric_limits<float>::quiet_NaN()) == settled,
        "non-finite drop progress settles safely");
    for (const auto& [role, visualRole, seed] : std::array{
             std::tuple{HudWeaponRole::Pistol, PickupVisualRole::DroppedPistol, 123U},
             std::tuple{HudWeaponRole::Carbine, PickupVisualRole::Weapon, 456U},
             std::tuple{HudWeaponRole::Sniper, PickupVisualRole::DroppedSniper, 123U},
             std::tuple{HudWeaponRole::Sniper, PickupVisualRole::DroppedSniper, 128U},
             std::tuple{HudWeaponRole::Shotgun, PickupVisualRole::DroppedShotgun, 123U},
             std::tuple{HudWeaponRole::Shotgun, PickupVisualRole::DroppedShotgun, 128U},
         }) {
        const DroppedWeaponPose contact
            = sampleDroppedWeaponPose(seed, visualRole, 1.0F);
        const float sine = std::sin(contact.rollDegrees
            * std::numbers::pi_v<float> / 180.0F);
        const float cosine = std::cos(contact.rollDegrees
            * std::numbers::pi_v<float> / 180.0F);
        float lowestPoint = std::numeric_limits<float>::max();
        for (const auto& mesh : makeWeaponViewmodelMeshes(role)) {
            for (const auto& vertex : mesh.vertices) {
                lowestPoint = std::min(lowestPoint,
                    contact.height + sine * vertex.position.x
                        + cosine * vertex.position.y);
            }
        }
        expect(lowestPoint >= -0.002F && lowestPoint < 0.025F,
            "settled weapon mesh touches floor without clipping");
    }
}

void testWeaponReloadProfiles()
{
    using namespace gdp::game::visual;
    constexpr std::array roles{HudWeaponRole::Pistol, HudWeaponRole::Carbine,
        HudWeaponRole::SubmachineGun, HudWeaponRole::Sniper, HudWeaponRole::Shotgun};
    for (const auto role : roles) {
        const auto idle = sampleWeaponReloadPose(role, 0.0F);
        expect(sampleWeaponReloadPose(role, 1.0F) == idle,
            "reload parts and body return exactly to their idle pose");
        expect(sampleWeaponReloadPose(role, -1.0F) == idle
                && sampleWeaponReloadPose(role, 3.0F) == idle
                && sampleWeaponReloadPose(role, std::numeric_limits<float>::quiet_NaN()) == idle,
            "reload progress is safely clamped and sanitizes NaN");
        for (int i = 0; i <= 100; ++i) {
            const auto pose = sampleWeaponReloadPose(role, float(i) / 100.0F);
            expect(wkc::core::length(pose.weapon.offset) < 0.25F
                    && wkc::core::length(pose.magazineOffset) < 0.5F
                    && wkc::core::length(pose.mechanismOffset) < 0.2F,
                "rigid reload transforms stay within a restrained finite budget");
        }
        for (const auto other : roles) if (role != other)
            expect(sampleWeaponReloadPose(role, 0.45F) != sampleWeaponReloadPose(other, 0.45F),
                "each firearm has a distinct reload profile, including the placeholder SMG");
        if (role != HudWeaponRole::Shotgun) {
            const auto outgoing = sampleWeaponReloadPose(role, 0.40F);
            const auto falling = sampleWeaponReloadPose(role, 0.60F);
            expect(outgoing.discardedMagazineVisible && !outgoing.magazineVisible
                && falling.discardedMagazineVisible && falling.magazineVisible
                && falling.discardedMagazineOffset.y < outgoing.discardedMagazineOffset.y
                && falling.discardedMagazineOffset != falling.magazineOffset,
                "old magazine falls away independently of the incoming full magazine");
            expect(!sampleWeaponReloadPose(role, 0.80F).discardedMagazineVisible,
                "discarded magazine never returns to the weapon");
        }
        const auto full = makeWeaponViewmodelMeshes(role);
        const auto parts = makeWeaponViewmodelParts(role);
        std::size_t completeIndices = 0, splitIndices = 0;
        for (std::size_t m = 0; m < weaponMaterialCount; ++m) {
            completeIndices += full[m].indices.size();
            for (std::size_t p = 0; p < weaponPartCount; ++p) {
                const auto& mesh = parts[p][m];
                if (!mesh.vertices.empty())
                    expect(wkc::render::validateMeshData(mesh), "reload part mesh is valid");
                if (role == HudWeaponRole::Shotgun
                    && p == static_cast<std::size_t>(WeaponPart::Magazine)) continue;
                splitIndices += mesh.indices.size();
                for (const auto& vertex : mesh.vertices)
                    expect((p == 1U && vertex.semanticRegion == 2U)
                            || (p == 2U && vertex.semanticRegion == 3U)
                            || (p == 0U && vertex.semanticRegion < 2U),
                        "rigid parts only contain their own authored semantic regions");
            }
        }
        expect(completeIndices == splitIndices, "rigid partition neither loses nor duplicates triangles");
        for (const auto part : {WeaponPart::Magazine, WeaponPart::Mechanism}) {
            std::size_t indices = 0;
            for (const auto& mesh : parts[static_cast<std::size_t>(part)]) indices += mesh.indices.size();
            expect(indices > 0, "firearms provide an actual magazine/cartridge and moving mechanism");
        }
    }
    const auto knife = sampleWeaponReloadPose(HudWeaponRole::Knife, 0.5F);
    expect(knife.weapon == WeaponViewmodelPose{} && !knife.magazineVisible,
        "knife never plays a fictitious reload");
    expect(sampleWeaponReloadPose(HudWeaponRole::Shotgun, 0.31F, 1U).magazineOffset
            != sampleWeaponReloadPose(HudWeaponRole::Shotgun, 0.31F, 6U).magazineOffset,
        "shotgun insertion cadence follows the actual number of rounds being loaded");
}

void testPlayerHands()
{
    using namespace gdp::game::visual;
    using namespace wkc::core;
    const auto meshes=makePlayerHandsMeshes();
    const auto repeated=makePlayerHandsMeshes();
    std::size_t triangles=0;
    for(std::size_t m=0;m<meshes.size();++m) {
        const auto& mesh=meshes[m];
        expect(wkc::render::validateMeshData(mesh),"rounded hands/sleeve mesh validates");
        triangles+=mesh.indices.size()/3U;
        expect(mesh.indices==repeated[m].indices && mesh.vertices.size()==repeated[m].vertices.size(),
            "hands topology is deterministic");
        for(std::size_t v=0;v<mesh.vertices.size();++v) {
            const auto a=mesh.vertices[v],b=repeated[m].vertices[v];
            expect(a.position==b.position && a.normal==b.normal && a.uv==b.uv,
                "hands vertices are deterministic");
            const float joint=std::floor(a.uv.x*.5F),weight=a.uv.x-2*joint;
            expect(joint>=0 && joint<float(playerHandsBoneCount) && a.uv.y>=0
                && a.uv.y<float(playerHandsBoneCount) && weight>=0 && weight<=1,
                "hands use valid two-joint skin weights");
        }
    }
    expect(triangles==4016U,"approved refined hands retain a bounded 4016-triangle budget");
    std::cout<<"Player hands triangles="<<triangles<<" materials=3 bones=36\n";
    std::size_t textureBytes=0;
    for(const auto material:{PlayerHandsMaterial::Cloth,PlayerHandsMaterial::Leather,PlayerHandsMaterial::Reinforcement}) {
        const auto images=makeUsedPlayerHandsMaterialImages(material);
        const auto repeatedImages=makeUsedPlayerHandsMaterialImages(material);
        const int resolution=material==PlayerHandsMaterial::Reinforcement ? 256 : 512;
        expect(images.albedo.width==resolution && images.albedo.height==resolution
            && images.albedo.rgba==repeatedImages.albedo.rgba
            && images.normal.rgba==repeatedImages.normal.rgba
            && images.properties.rgba==repeatedImages.properties.rgba,
            "dedicated approved glove maps have stable dimensions and deterministic pixels");
        textureBytes+=images.albedo.rgba.size()+images.normal.rgba.size()+images.properties.rgba.size();
        for(std::size_t i=0;i<images.properties.rgba.size();i+=4U)
            expect(images.properties.rgba[i]>=200U && images.properties.rgba[i+1U]==0U
                && images.albedo.rgba[i+3U]==255U && images.normal.rgba[i+3U]==255U
                && images.properties.rgba[i+3U]==255U,"gloves are opaque matte nonmetal materials");
    }
    expect(textureBytes==7077888U,"glove textures use a measured bounded 6.75 MiB before mipmaps");
    constexpr std::array roles{HudWeaponRole::Pistol,HudWeaponRole::Carbine,HudWeaponRole::Knife,
        HudWeaponRole::Sniper,HudWeaponRole::Shotgun,HudWeaponRole::SubmachineGun};
    for(const auto role:roles) {
        WeaponViewmodelRenderData model{.visible=true,.role=role};
        const auto idle=samplePlayerHandsPose(model);
        for(int i=0;i<=100;++i) {
            model.reloadProgress=float(i)/100;
            const auto pose=samplePlayerHandsPose(model);
            const auto duplicate=samplePlayerHandsPose(model);
            for(std::size_t b=0;b<playerHandsBoneCount;++b)
                expect(isFinite(pose.skin[b]) && pose.skin[b].elements==duplicate.skin[b].elements,
                    "hands pose is finite and deterministic across every reload phase");
            for(std::size_t side=0;side<2;++side) {
                expect(nearlyEqual(length(pose.shoulders[side]-pose.elbows[side]),.90F,.0001F)
                    && nearlyEqual(length(pose.elbows[side]-pose.wrists[side]),.90F,.0001F),
                    "elbow IK preserves arm lengths and contact wrist");
                const auto frame=sampleWeaponPresentationTransform(model);
                const auto& a=frame.elements;
                const auto p=pose.shoulders[side];
                const float y=a[1]*p.x+a[5]*p.y+a[9]*p.z+a[13];
                const float z=a[2]*p.x+a[6]*p.y+a[10]*p.z+a[14];
                expect(y+z*.675F<-.12F,"shoulder sleeve endpoints stay outside the bottom frame through reload");
            }
            if(role!=HudWeaponRole::Sniper || (i>=26 && i<=80) || i==0 || i==100)
                expect(pose.wrists[0]==idle.wrists[0],"dominant grip remains fixed except explicit sniper bolt windows");
            else if(i==12 || i==90)
                expect(pose.rightContact==HandContact::Mechanism && pose.leftContact==HandContact::Support
                    && pose.wrists[0]!=idle.wrists[0] && pose.wrists[1]==idle.wrists[1],
                    "sniper uses right hand on bolt while left hand supports fore-end");
            for(std::size_t side=0;side<2U;++side) {
                const auto& a=pose.skin[side*playerHandBonesPerSide+2U].elements;
                const Vec3 x{a[0],a[1],a[2]},y{a[4],a[5],a[6]},z{a[8],a[9],a[10]};
                expect(nearlyEqual(length(x),1) && nearlyEqual(length(y),1) && nearlyEqual(length(z),1)
                    && nearlyEqual(dot(cross(x,y),z),1),"palm contact orientation stays rigid and right handed");
                for(const auto flex:pose.fingerFlexion[side])
                    expect(std::isfinite(flex) && flex>=0 && flex<=1.05F,"individual finger profiles stay bounded");
            }
            for(const auto& mesh:meshes) for(const auto& vertex:mesh.vertices) {
                const auto p=deformPlayerHandsVertex(vertex,pose);
                expect(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && length(p)<4.0F,
                    "skinned fingers and sleeve remain finite within local bounds");
            }
            if(i==0 || i==100) for(std::size_t b=0;b<playerHandsBoneCount;++b)
                expect(pose.skin[b].elements==idle.skin[b].elements,"reload completion returns hands exactly to grip");
        }
        model.reloadProgress=std::numeric_limits<float>::quiet_NaN();
        const auto invalid=samplePlayerHandsPose(model);
        for(std::size_t b=0;b<playerHandsBoneCount;++b)
            expect(invalid.skin[b].elements==idle.skin[b].elements,"invalid reload gives safe idle hands");
        model.reloadProgress=.4F;
        if(role!=HudWeaponRole::Knife && role!=HudWeaponRole::Shotgun)
            expect(samplePlayerHandsPose(model).leftContact==HandContact::Fetch,
                "left hand releases old magazine rather than following falling instance");
        model.reloadProgress=.6F;
        if(role!=HudWeaponRole::Knife && role!=HudWeaponRole::Shotgun)
            expect(samplePlayerHandsPose(model).leftContact==HandContact::Magazine,
                "left hand holds the incoming fresh magazine");
        if(role!=HudWeaponRole::Knife && role!=HudWeaponRole::Shotgun) {
            const auto magazine=samplePlayerHandsPose(model);
            // Probe the inner palm against the actual incoming part bounds, not
            // just a semantic label saying that the hand is "on the magazine".
            const wkc::render::Vertex probe{{0,.132F,-.0396F},{},{41,20},0};
            const Vec3 palm=deformPlayerHandsVertex(probe,magazine);
            Vec3 minimum{100,100,100},maximum{-100,-100,-100};
            const auto parts=makeWeaponViewmodelParts(role==HudWeaponRole::SubmachineGun ? HudWeaponRole::Carbine : role);
            const auto offset=sampleWeaponReloadPose(role,.6F).magazineOffset;
            for(const auto& mesh:parts[static_cast<std::size_t>(WeaponPart::Magazine)])
                for(const auto& vertex:mesh.vertices) {
                    const auto p=vertex.position+offset;
                    minimum={std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};
                    maximum={std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};
                }
            const Vec3 nearest{std::clamp(palm.x,minimum.x,maximum.x),
                std::clamp(palm.y,minimum.y,maximum.y),std::clamp(palm.z,minimum.z,maximum.z)};
            expect(length(palm-nearest)<.04F,"incoming magazine touches the inner palm in actual weapon-part space");
            model.reloadProgress=.46F;
            const auto open=samplePlayerHandsPose(model);
            expect(open.fingerFlexion[1][0]<magazine.fingerFlexion[1][0]
                && open.fingerFlexion[1][4]<magazine.fingerFlexion[1][4],
                "discard/fetch opens fingers and incoming magazine closes them again");
            if(role!=HudWeaponRole::Sniper) {
                model.reloadProgress=.84F;
                const auto mechanism=samplePlayerHandsPose(model);
                expect(mechanism.skin[playerHandBonesPerSide+2U].elements[0]
                    !=magazine.skin[playerHandBonesPerSide+2U].elements[0]
                    || mechanism.skin[playerHandBonesPerSide+2U].elements[8]
                    !=magazine.skin[playerHandBonesPerSide+2U].elements[8],
                    "mechanism has its own orientation rather than a translated magazine pose");
            }
        }
        if(role==HudWeaponRole::Sniper) {
            const auto transformPoint=[](const Mat4& matrix,const Vec3 p) {
                const auto& a=matrix.elements;
                return Vec3{a[0]*p.x+a[4]*p.y+a[8]*p.z+a[12],
                    a[1]*p.x+a[5]*p.y+a[9]*p.z+a[13],a[2]*p.x+a[6]*p.y+a[10]*p.z+a[14]};
            };
            for(const float progress:{.12F,.18F,.87F,.94F}) {
                model.reloadProgress=progress;
                const auto pose=samplePlayerHandsPose(model);
                const auto part=sampleWeaponReloadPose(role,progress);
                const Vec3 pivot{.096F,.015F,-.08F};
                const Mat4 bolt=translationMatrix(part.mechanismOffset)*translationMatrix(pivot)
                    *rotationZMatrix(part.mechanismRotationDegrees.z*std::numbers::pi_v<float>/180)
                    *translationMatrix(pivot*-1);
                const Vec3 knob=transformPoint(bolt,{.215F,-.07F,-.08F});
                const wkc::render::Vertex probe{{0,.132F,-.0396F},{},{5,2},0};
                expect(length(deformPlayerHandsVertex(probe,pose)-knob)<.05F,
                    "dominant palm follows the actual rotating/translating sniper bolt knob");
            }
        }
        if(role==HudWeaponRole::Shotgun) {
            model.reloadProgress=.6F;
            model.reloadRoundCount=1;
            const auto one=samplePlayerHandsPose(model);
            model.reloadRoundCount=6;
            expect(samplePlayerHandsPose(model).wrists[1]!=one.wrists[1],
                "shotgun hand cadence follows planned shell count rather than magazine clock");
            model.reloadProgress=.20F;
            const auto shell=samplePlayerHandsPose(model);
            expect(shell.leftContact==HandContact::Shell && shell.fingerFlexion[1][0]<shell.fingerFlexion[1][2]
                && shell.fingerFlexion[1][4]<shell.fingerFlexion[1][3],
                "shotgun uses separate thumb/index shell pinch rather than a closed magazine fist");
        }
        model.reloadProgress=0;
        auto previous=samplePlayerHandsPose(model);
        for(unsigned int i=1;i<=1000U;++i) {
            model.reloadProgress=float(i)/1000;
            const auto next=samplePlayerHandsPose(model);
            for(std::size_t side=0;side<2U;++side) {
                expect(length(next.wrists[side]-previous.wrists[side])<.08F,
                    "both reload hands have continuous contact trajectories");
                const auto& a=next.skin[side*playerHandBonesPerSide+2U].elements;
                const auto& b=previous.skin[side*playerHandBonesPerSide+2U].elements;
                expect(dot({a[0],a[1],a[2]},{b[0],b[1],b[2]})>.99F
                    && dot({a[8],a[9],a[10]},{b[8],b[9],b[10]})>.99F,
                    "palm contact rotations interpolate without orientation snaps");
            }
            previous=next;
        }
        model.reloadProgress=0;
        model.recoilStrength=1; model.meleeStrength=1; model.crouchStrength=1;
        model.equipStrength=1; model.airborneStrength=1;
        const auto attached=samplePlayerHandsPose(model);
        expect(attached.wrists==idle.wrists,"root recoil/equip/posture transforms cannot detach hand contacts");
    }
}

void testOptimizedShadowBudget()
{
    using namespace gdp::game::visual;

    RoomLightingRenderData lighting;
    lighting.lightCount = static_cast<std::uint32_t>(maxRenderedRoomLights);
    for (std::size_t index = 0U; index < maxRenderedRoomLights; ++index) {
        lighting.lights[index] = {
            {static_cast<float>(index), 3.5F, 0.0F},
            {0.8F, 0.9F, 1.0F},
            index == 3U ? 0.0F : 1.0F,
            12.0F,
        };
    }
    std::array<ShadowOccluderRenderData, 11U> candidates{};
    for (std::size_t index = 0U; index < 9U; ++index) {
        const float x = static_cast<float>(index) * 0.4F;
        candidates[index].bounds = {
            {x, 0.0F, -0.3F}, {x + 0.25F, 1.0F, 0.3F}};
    }
    candidates[9].bounds = {
        {100.0F, 0.0F, 100.0F}, {101.0F, 1.0F, 101.0F}};
    candidates[10].bounds = {
        {std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F},
        {1.0F, 1.0F, 1.0F}};

    RoomLightingRenderData repeated = lighting;
    const ShadowBudgetResult result
        = optimizeShadowBudget(lighting, {}, candidates);
    const ShadowBudgetResult repeatedResult
        = optimizeShadowBudget(repeated, {}, candidates);
    expect(result.activeLights == 7U
            && lighting.shadowLightCount == maxShadowCastingLights,
        "shadow optimization selects only two nearest active lights");
    expect(result.candidateOccluders == candidates.size()
            && result.selectedOccluders == maxActiveShadowOccluders
            && lighting.shadowOccluderCount == maxActiveShadowOccluders,
        "shadow optimization caps valid nearby occluders at the active budget");
    expect(lighting.lights[0].position.x == 0.0F
            && lighting.lights[1].position.x == 1.0F,
        "nearest active lights are ordered before shadow evaluation");
    expect(result == repeatedResult
            && lighting.shadowOccluders == repeated.shadowOccluders,
        "shadow budget selection is deterministic for identical snapshots");

    for (std::size_t index = 0U; index < lighting.lightCount; ++index) {
        lighting.lights[index].intensity = 0.0F;
    }
    const ShadowBudgetResult dark
        = optimizeShadowBudget(lighting, {}, candidates);
    expect(dark.activeLights == 0U && dark.selectedOccluders == 0U
            && lighting.shadowLightCount == 0U
            && lighting.shadowOccluderCount == 0U,
        "fully dark lighting bypasses every shadow intersection");
}

void testIndustrialUiTheme()
{
    using namespace gdp::game::visual;
    const UiVisualTheme theme = industrialUiTheme();
    expect(theme.background.x < theme.panel.x
            && theme.panel.x < theme.panelBorder.x
            && theme.selection.x > theme.panel.x,
        "industrial UI establishes layered backdrop, panel, border and selection");
    expect(theme.accent.x < theme.accent.y
            && theme.accent.z > theme.accent.y
            && theme.danger.x > theme.danger.y * 3.0F
            && theme.success.y > theme.success.x * 2.0F,
        "industrial UI reserves cyan, danger red and success green semantics");
    expect(theme.text.x > theme.mutedText.x
            && theme.text.y > theme.mutedText.y
            && theme.text.z > theme.mutedText.z,
        "primary interface text remains brighter than secondary instructions");
}

void testWeaponViewmodelAssets()
{
    using namespace gdp::game::visual;
    constexpr std::array roles{
        HudWeaponRole::Pistol, HudWeaponRole::Carbine, HudWeaponRole::Knife,
        HudWeaponRole::Sniper, HudWeaponRole::Shotgun};
    for (const HudWeaponRole role : roles) {
        const auto meshes = makeWeaponViewmodelMeshes(role);
        std::size_t triangles = 0U;
        std::size_t materialCount = 0U;
        for (const auto& mesh : meshes) {
            if (mesh.vertices.empty()) {
                continue;
            }
            ++materialCount;
            triangles += mesh.indices.size() / 3U;
            std::string reason;
            expect(wkc::render::validateMeshData(mesh, &reason),
                "weapon viewmodel mesh validates");
        }
        expect(materialCount >= 3U && materialCount <= weaponMaterialCount,
            "weapon viewmodel groups physical materials into few draws");
        expect(triangles > 100U && triangles < 1600U,
            "weapon viewmodel stays within its triangle budget");
        if (role != HudWeaponRole::Knife) {
            const auto& grip = meshes[static_cast<std::size_t>(
                WeaponMaterial::Polymer)];
            float minimumY = std::numeric_limits<float>::max();
            float maximumY = -std::numeric_limits<float>::max();
            std::size_t gripVertices = 0U;
            for (const auto& vertex : grip.vertices) {
                if (vertex.semanticRegion != weaponGripSemanticRegion) {
                    continue;
                }
                minimumY = std::min(minimumY, vertex.position.y);
                maximumY = std::max(maximumY, vertex.position.y);
                ++gripVertices;
            }
            const float middleY = (minimumY + maximumY) * 0.5F;
            float upperZ = 0.0F;
            float lowerZ = 0.0F;
            std::size_t upperCount = 0U;
            std::size_t lowerCount = 0U;
            for (const auto& vertex : grip.vertices) {
                if (vertex.semanticRegion != weaponGripSemanticRegion) {
                    continue;
                }
                if (vertex.position.y > middleY) {
                    upperZ += vertex.position.z;
                    ++upperCount;
                } else {
                    lowerZ += vertex.position.z;
                    ++lowerCount;
                }
            }
            expect(gripVertices == 24U && upperCount > 0U && lowerCount > 0U
                    && lowerZ / static_cast<float>(lowerCount)
                        < upperZ / static_cast<float>(upperCount),
                "firearm grip base rakes toward the rear, not the muzzle");
        }
    }
    const auto sniper = makeWeaponViewmodelMeshes(HudWeaponRole::Sniper);
    const auto shotgun = makeWeaponViewmodelMeshes(HudWeaponRole::Shotgun);
    const auto carbine = makeWeaponViewmodelMeshes(HudWeaponRole::Carbine);
    const auto steel = static_cast<std::size_t>(WeaponMaterial::CoatedSteel);
    expect(sniper[steel].bounds.maximum.y > shotgun[steel].bounds.maximum.y + 0.1F
            && sniper[steel].vertices.size() != carbine[steel].vertices.size()
            && shotgun[steel].vertices.size() != carbine[steel].vertices.size(),
        "approved sniper has its optic and shotgun has a distinct non-carbine silhouette");
    for (std::size_t material = 0U; material < weaponMaterialCount; ++material) {
        const auto images = makeUsedWeaponMaterialImages(
            static_cast<WeaponMaterial>(material));
        expect(wkc::render::validateImageData(images.albedo)
                && wkc::render::validateImageData(images.normal)
                && wkc::render::validateImageData(images.properties),
            "used weapon material maps validate");
        expect(images.albedo.width == (material < 3U ? 512 : 128),
            "primary weapon surfaces retain close-view texture density");
        expect(images.albedo.rgba == makeUsedWeaponMaterialImages(
                static_cast<WeaponMaterial>(material)).albedo.rgba,
            "used weapon wear is deterministic");
    }
}

void testPlayerBody()
{
    using namespace gdp::game::visual;
    using namespace wkc::core;
    PlayerBodyMotion motion;
    updatePlayerBodyMotion(motion,{0,0,0},{0,0,-.1F},-90,true,true,1.0F/60);
    expect(motion.phaseRadians > 0 && motion.forward > .1F,
        "body gait advances by actual forward distance");
    const auto phase = motion.phaseRadians;
    for (int i = 0; i < 30; ++i)
        updatePlayerBodyMotion(motion,{0,0,0},{0,0,0},-90,true,true,1.0F/60);
    expect(motion.phaseRadians == phase && motion.weight == 0,
        "blocked movement stops body gait without advancing phase");
    for (int i = 0; i < 10; ++i)
        updatePlayerBodyMotion(motion,{0,0,0},{0,0,.1F},-90,true,true,1.0F/60);
    expect(motion.forward < -.99F,"backward gait uses actual signed direction");
    for (int i = 0; i < 10; ++i)
        updatePlayerBodyMotion(motion,{0,0,0},{.1F,0,0},-90,true,true,1.0F/60);
    expect(motion.right > .99F,"strafe gait uses yaw-local direction");
    const auto beforeAir = motion.phaseRadians;
    for (int i = 0; i < 20; ++i)
        updatePlayerBodyMotion(motion,{0,0,0},{.02F,.02F,0},-90,false,true,1.0F/60);
    expect(motion.phaseRadians == beforeAir && motion.weight == 0 && motion.airborne == 1,
        "airborne body flexes without fake walking or movement changes");
    const auto beforeInvalid = motion;
    updatePlayerBodyMotion(motion,{},{},-90,true,true,std::numeric_limits<float>::quiet_NaN());
    expect(motion == beforeInvalid,"invalid clock cannot corrupt body state");
    updatePlayerBodyMotion(motion,{},{10,0,0},-90,true,true,1.0F/60);
    expect(motion == PlayerBodyMotion{},"relocation resets transient body history");

    const auto meshes = makePlayerBodyMeshes();
    std::size_t triangles = 0;
    for (const auto& mesh : meshes) {
        expect(wkc::render::validateMeshData(mesh),"skinned lower-body mesh validates");
        triangles += mesh.indices.size()/3U;
        for (const auto& v : mesh.vertices) {
            const auto joint = std::floor(v.uv.x*.5F);
            const auto weight = v.uv.x-2*joint;
            expect(joint >= 0 && joint < 7 && v.uv.y >= 0 && v.uv.y < 7
                && weight >= 0 && weight <= 1,"GDP skin indices/weights stay within palette");
        }
    }
    expect(triangles <= 2200U,"body stays within the four-draw 2200-triangle budget");
    std::cout << "Player body triangles=" << triangles << " materials=4 bones=7\n";
    for (float crouch : {0.0F,.5F,1.0F}) for (float air : {0.0F,1.0F})
    for (float direction : {-1.0F,0.0F,1.0F}) for (float gait : {0.0F,1.1F,3.3F}) {
        const PlayerBodyRenderData body{.visible=true,.crouchAmount=crouch,
            .gaitPhase=gait,.gaitWeight=1,.forwardMovement=direction,.airborneStrength=air};
        const auto pose = samplePlayerBodyPose(body);
        const auto repeated = samplePlayerBodyPose(body);
        expect(pose.skin[1].elements == repeated.skin[1].elements,
            "player pose is deterministic and render sampling does not mutate it");
        for (std::size_t leg = 0; leg < 2; ++leg) {
            expect(nearlyEqual(length(pose.knees[leg]-pose.hips[leg]),.44F,.0001F)
                && nearlyEqual(length(pose.ankles[leg]-pose.knees[leg]),.43F,.0001F),
                "IK preserves human thigh/calf lengths in gait/crouch/air");
            expect(pose.knees[leg].z < pose.hips[leg].z+.04F,
                "knees flex forward, not inside out");
        }
        for (const auto& mesh : meshes) for (const auto& v : mesh.vertices) {
            const auto p = deformPlayerBodyVertex(v,pose);
            expect(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)
                && p.y > -.06F && p.y < 1.50F,"deformed body stays finite and above floor");
        }
    }
    PlayerBodyRenderData invalid;
    invalid.crouchAmount = invalid.gaitPhase = invalid.yawDegrees = std::numeric_limits<float>::quiet_NaN();
    const auto pose = samplePlayerBodyPose(invalid);
    for (const auto& matrix : pose.skin) for (const auto n : matrix.elements)
        expect(std::isfinite(n),"invalid body DTO produces a finite safe pose");
    for (std::size_t i = 0; i < playerBodyMaterialCount; ++i) {
        const auto images = makeUsedPlayerBodyMaterialImages(static_cast<PlayerBodyMaterial>(i));
        expect(wkc::render::validateImageData(images.albedo)
            && wkc::render::validateImageData(images.normal)
            && wkc::render::validateImageData(images.properties),"body maps validate");
        for (std::size_t offset = 1U; offset < images.properties.rgba.size(); offset+=4U)
            expect(images.properties.rgba[offset] == 0U,"cloth/leather/rubber are nonmetallic");
    }
}

} // namespace

int main()
{
    testMesh();
    testMeshBindingCache();
    testInstanceTransforms();
    testBackfaceCullingStateCache();
    testRendererShaderPathSelection();
    testTextureData();
    testTextureBindingCache();
    testCameraAndMatrices();
    testGpuTimerFallback();
    testShaderUniformLocationCache();
    testShaderUniformValueCache();
    testConservativeVisibilityCulling();
    testMainMenuLayout();
    testPauseMenuLayout();
    testControlsMenuLayout();
    testResultMenuLayout();
    testExitConfirmationLayout();
    testRunTimeFormatting();
    testEnemyRootAnimation();
    testImpactVisualData();
    testWeaponViewmodelPose();
    testSniperScopePresentation();
    testWeaponReloadProfiles();
    testDroppedWeaponPose();
    testOptimizedShadowBudget();
    testIndustrialUiTheme();
    testWeaponViewmodelAssets();
    testPlayerHands();
    testSettingsMenuLayout();
    testPlayerBody();

    if (failures != 0) {
        std::cerr << failures << " renderer test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP renderer CPU tests passed\n";
    return 0;
}
