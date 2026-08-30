#include "core/Math.hpp"
#include "game/visual/VisualAssets.hpp"
#include "procedural/AssetCache.hpp"
#include "procedural/AssetRecipe.hpp"
#include "procedural/MeshGenerator.hpp"
#include "procedural/TextureGenerator.hpp"
#include "render/Mesh.hpp"
#include "render/Texture.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
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

bool containsError(
    const gdp::procedural::RecipeValidationResult& result,
    const gdp::procedural::RecipeValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error) != result.errors.end();
}

void testRecipeValidationAndHashing()
{
    using namespace gdp::procedural;

    const MeshRecipe worker = gdp::game::makeWorkerFoundationMeshRecipe(12345);
    expect(validateMeshRecipe(worker).isValid(), "Worker foundation mesh recipe validates");
    expect(worker.primitives.size() == 6U,
        "Worker foundation recipe preserves its six readable body regions");
    expect(meshRecipeHash(worker)
            == meshRecipeHash(gdp::game::makeWorkerFoundationMeshRecipe(12345)),
        "equivalent mesh recipes have the same canonical hash");
    expect(meshRecipeHash(worker) == 0x8AD8E9BAACB817BCULL,
        "golden Worker mesh recipe hash remains stable across builds");

    MeshRecipe changed = worker;
    changed.primitives[0].semantic = SemanticRegion::Joint;
    expect(meshRecipeHash(changed) != meshRecipeHash(worker),
        "semantic region participates in the mesh recipe hash");
    changed = worker;
    ++changed.seed;
    expect(meshRecipeHash(changed) != meshRecipeHash(worker),
        "visual seed participates in the mesh recipe hash");

    MeshRecipe empty;
    expect(containsError(validateMeshRecipe(empty), RecipeValidationError::EmptyMesh),
        "empty mesh recipe is rejected");
    changed = worker;
    changed.primitives[0].type = PrimitiveType::RoundedBox;
    expect(containsError(
               validateMeshRecipe(changed), RecipeValidationError::UnsupportedPrimitive),
        "not-yet-implemented primitive is rejected explicitly");
    changed = worker;
    changed.primitives[0].dimensions.x = 0.0F;
    expect(containsError(validateMeshRecipe(changed), RecipeValidationError::InvalidDimensions),
        "non-positive primitive dimensions are rejected");

    const TextureRecipe texture = gdp::game::makeWorkerFoundationTextureRecipe(54321);
    expect(validateTextureRecipe(texture).isValid(),
        "Worker foundation texture recipe validates");
    expect(textureRecipeHash(texture)
            == textureRecipeHash(gdp::game::makeWorkerFoundationTextureRecipe(54321)),
        "equivalent texture recipes have the same canonical hash");
    expect(textureRecipeHash(texture) == 0x409FFD1A4CA6BCAFULL,
        "golden Worker texture recipe hash remains stable across builds");
    TextureRecipe changedTexture = texture;
    changedTexture.dirtAmount += 0.01F;
    expect(textureRecipeHash(changedTexture) != textureRecipeHash(texture),
        "texture parameters participate in the canonical hash");
    changedTexture = texture;
    changedTexture.resolution = 63;
    expect(containsError(
               validateTextureRecipe(changedTexture),
               RecipeValidationError::InvalidTextureResolution),
        "non-power-of-two texture resolution is rejected");
    changedTexture = texture;
    changedTexture.baseColor.x = 1.1F;
    expect(containsError(
               validateTextureRecipe(changedTexture), RecipeValidationError::InvalidColor),
        "texture color outside normalized range is rejected");
}

void testMeshGeneration()
{
    using namespace gdp::procedural;

    const auto worker = generateMesh(gdp::game::makeWorkerFoundationMeshRecipe(12345));
    expect(worker.has_value(), "valid Worker recipe generates CPU mesh data");
    if (!worker) {
        return;
    }
    expect(gdp::render::validateMeshData(worker->mesh),
        "generated Worker mesh passes renderer validation");
    expect(worker->mesh.vertices.size() == 144U && worker->mesh.indices.size() == 216U,
        "six boxes combine into exact vertex and index counts");
    expect(worker->ranges.size() == 6U
            && worker->ranges[0].material == MaterialSlot::Armor
            && worker->ranges[0].semantic == SemanticRegion::ArmorPlate,
        "generated primitive ranges preserve material and semantic metadata");
    expect(gdp::core::nearlyEqual(worker->mesh.bounds.minimum, {-0.59F, 0.0F, -0.24F})
            && gdp::core::nearlyEqual(worker->mesh.bounds.maximum, {0.59F, 1.79F, 0.24F}),
        "combined Worker bounds match all transformed primitives");
    expect(worker->mesh.vertices.front().semanticRegion
            == static_cast<std::uint32_t>(SemanticRegion::ArmorPlate),
        "semantic region survives into generated vertices");

    MeshRecipe rotated;
    rotated.primitives.push_back({
        .type = PrimitiveType::Box,
        .localTransform = {
            .position = {1.0F, 0.0F, 0.0F},
            .rotationDegrees = {0.0F, 90.0F, 0.0F},
        },
        .dimensions = {2.0F, 1.0F, 1.0F},
        .semantic = SemanticRegion::Piston,
    });
    const auto builtRotated = generateMesh(rotated);
    expect(builtRotated
            && gdp::core::nearlyEqual(builtRotated->mesh.bounds.minimum, {0.5F, -0.5F, -1.0F})
            && gdp::core::nearlyEqual(builtRotated->mesh.bounds.maximum, {1.5F, 0.5F, 1.0F}),
        "mesh generation applies local scale, rotation and translation");

    MeshRecipe cylinder;
    cylinder.primitives.push_back({
        .type = PrimitiveType::Cylinder,
        .dimensions = {1.0F, 2.0F, 1.0F},
        .semantic = SemanticRegion::Piston,
    });
    const auto builtCylinder = generateMesh(cylinder);
    expect(builtCylinder && gdp::render::validateMeshData(builtCylinder->mesh)
            && builtCylinder->mesh.vertices.size() == 54U
            && builtCylinder->mesh.indices.size() == 144U,
        "twelve-segment Cylinder generates valid side and cap geometry");
    expect(builtCylinder
            && gdp::core::nearlyEqual(
                builtCylinder->mesh.bounds.minimum, {-0.5F, -1.0F, -0.5F})
            && gdp::core::nearlyEqual(
                builtCylinder->mesh.bounds.maximum, {0.5F, 1.0F, 0.5F}),
        "generated Cylinder bounds follow requested dimensions");

    MeshRecipe unsupported = rotated;
    unsupported.primitives[0].type = PrimitiveType::Sphere;
    expect(!generateMesh(unsupported), "invalid recipe never produces partial mesh data");
}

void testTextureGeneration()
{
    using namespace gdp::procedural;

    TextureRecipe recipe = gdp::game::makeWorkerFoundationTextureRecipe(12345);
    const auto first = generateAlbedo(recipe);
    const auto repeated = generateAlbedo(recipe);
    expect(first && repeated && gdp::render::validateImageData(*first),
        "valid texture recipe generates valid RGBA data");
    expect(first && repeated && first->rgba == repeated->rgba,
        "same texture recipe produces identical pixels");
    ++recipe.seed;
    const auto different = generateAlbedo(recipe);
    expect(first && different && first->rgba != different->rgba,
        "different visual seed changes texture pixels");
    if (first) {
        const auto [darkest, brightest] = std::minmax_element(first->rgba.begin(), first->rgba.end());
        expect(*brightest > *darkest, "generated albedo contains deterministic variation");
        bool opaque = true;
        for (std::size_t index = 3; index < first->rgba.size(); index += 4U) {
            opaque = opaque && first->rgba[index] == 255U;
        }
        expect(opaque, "foundation albedo remains fully opaque");
    }

    TextureRecipe fabric = recipe;
    fabric.seed = 99;
    fabric.family = MaterialFamily::Fabric;
    fabric.oxidationAmount = 0.0F;
    const auto cleanFabric = generateAlbedo(fabric);
    fabric.oxidationAmount = 1.0F;
    const auto requestedOxidizedFabric = generateAlbedo(fabric);
    expect(cleanFabric && requestedOxidizedFabric
            && cleanFabric->rgba == requestedOxidizedFabric->rgba,
        "oxidation request cannot place rust on an incompatible fabric material");

    bool batchValid = true;
    recipe.resolution = 8;
    for (gdp::core::Seed seed = 0; seed < 1'000; ++seed) {
        recipe.seed = seed;
        const auto image = generateAlbedo(recipe);
        if (!image || !gdp::render::validateImageData(*image)) {
            batchValid = false;
            break;
        }
    }
    expect(batchValid, "1,000 visual seeds generate valid deterministic albedo data");

    recipe.resolution = 7;
    expect(!generateAlbedo(recipe), "invalid texture recipe never produces partial image data");
}

void testMaterialRulesAndCache()
{
    using namespace gdp::procedural;

    const MaterialProperties steel = materialProperties(MaterialFamily::BareSteel);
    const MaterialProperties fabric = materialProperties(MaterialFamily::Fabric);
    const MaterialProperties lens = materialProperties(MaterialFamily::Polycarbonate);
    expect(steel.metallic > 0.8F && steel.oxidationAllowed,
        "bare steel exposes metallic and oxidation properties");
    expect(fabric.metallic == 0.0F && !fabric.oxidationAllowed,
        "fabric remains dielectric and cannot oxidize");
    expect(lens.emissiveAllowed, "polycarbonate can represent an emissive lens");

    AssetCache<int> cache;
    int factoryCalls = 0;
    const auto first = cache.acquire(77, [&factoryCalls] {
        ++factoryCalls;
        return 42;
    });
    const auto reused = cache.acquire(77, [&factoryCalls] {
        ++factoryCalls;
        return 99;
    });
    const auto second = cache.acquire(88, [&factoryCalls] {
        ++factoryCalls;
        return 7;
    });
    const AssetCacheStats stats = cache.stats();
    expect(first == reused && *reused == 42 && factoryCalls == 2,
        "cache reuses shared immutable asset without rerunning its factory");
    expect(*second == 7 && stats.hits == 1 && stats.misses == 2 && stats.entries == 2,
        "cache reports deterministic hit, miss and entry counts");
    expect(cache.find(77) == first && !cache.find(999),
        "cache lookup finds existing key and safely rejects missing key");
    cache.clear();
    expect(cache.stats().entries == 0 && cache.stats().hits == 0
            && cache.stats().misses == 0,
        "cache clear releases entries and resets statistics");
}

} // namespace

int main()
{
    testRecipeValidationAndHashing();
    testMeshGeneration();
    testTextureGeneration();
    testMaterialRulesAndCache();

    if (failures != 0) {
        std::cerr << failures << " procedural asset test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP procedural asset tests passed\n";
    return 0;
}
