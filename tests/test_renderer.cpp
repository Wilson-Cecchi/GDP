#include "core/Math.hpp"
#include "game/visual/VisualAssets.hpp"
#include "render/Camera.hpp"
#include "render/Mesh.hpp"
#include "render/Renderer.hpp"
#include "render/Texture.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool isFinite(const gdp::core::Mat4& matrix)
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
    using namespace gdp::render;

    const MeshData box = makeBoxMesh();
    std::string reason;
    expect(validateMeshData(box, &reason), "box mesh validates");
    expect(box.vertices.size() == 24, "box has hard-edge vertices");
    expect(box.indices.size() == 36, "box has twelve indexed triangles");
    expect(box.bounds == gdp::core::Aabb{{-0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, 0.5F}},
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

void testTextureData()
{
    using namespace gdp::render;

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

void testCameraAndMatrices()
{
    using namespace gdp::core;
    using gdp::render::Camera;

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

void testMainMenuLayout()
{
    using namespace gdp::render;

    const MainMenuLayout wide = makeMainMenuLayout(1280, 720);
    expect(wide.scale == 2, "wide menu uses doubled bitmap scale");
    expect(wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "wide menu panel stays within the framebuffer");
    expect(wide.panel.contains(wide.startButton.x, wide.startButton.y),
        "Start Run button is inside the menu panel");
    expect(wide.panel.contains(wide.exitButton.x, wide.exitButton.y),
        "Exit Game button is inside the menu panel");

    const float startX = static_cast<float>(wide.startButton.x + wide.startButton.width / 2);
    const float startY = static_cast<float>(
        720 - 1 - (wide.startButton.y + wide.startButton.height / 2));
    const float exitX = static_cast<float>(wide.exitButton.x + wide.exitButton.width / 2);
    const float exitY = static_cast<float>(
        720 - 1 - (wide.exitButton.y + wide.exitButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, startX, startY)
            == HudMainMenuSelection::StartRun,
        "top-origin pointer maps to Start Run");
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
            && tiny.panel.contains(tiny.exitButton.x, tiny.exitButton.y),
        "tiny menu clamps both hit targets inside the framebuffer");
}

} // namespace

int main()
{
    testMesh();
    testTextureData();
    testCameraAndMatrices();
    testMainMenuLayout();

    if (failures != 0) {
        std::cerr << failures << " renderer test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP renderer CPU tests passed\n";
    return 0;
}
