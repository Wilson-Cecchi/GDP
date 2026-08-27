#include "core/Math.hpp"
#include "render/Camera.hpp"
#include "render/Mesh.hpp"
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

    const ImageData first = makeIndustrialCheckerImage(8, 8);
    const ImageData second = makeIndustrialCheckerImage(8, 8);
    expect(validateImageData(first), "generated RGBA image validates");
    expect(first.rgba == second.rgba, "procedural texture pixels are deterministic");
    expect(first.rgba.size() == 8U * 8U * 4U, "texture contains four channels per pixel");
    expect(first.rgba[0] != first.rgba[(2U * 4U)], "checker pattern contains two tones");

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

} // namespace

int main()
{
    testMesh();
    testTextureData();
    testCameraAndMatrices();

    if (failures != 0) {
        std::cerr << failures << " renderer test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP renderer CPU tests passed\n";
    return 0;
}
