#include "procedural/RoomMaterialGenerator.hpp"
#include "render/Texture.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <string_view>
#include <unordered_set>

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
    const gdp::procedural::IndustrialMaterialValidationResult& result,
    const gdp::procedural::IndustrialMaterialValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

bool containsError(
    const gdp::procedural::StorageMaterialValidationResult& result,
    const gdp::procedural::StorageMaterialValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

bool containsError(
    const gdp::procedural::CombatMaterialValidationResult& result,
    const gdp::procedural::CombatMaterialValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

bool containsError(
    const gdp::procedural::CafeteriaMaterialValidationResult& result,
    const gdp::procedural::CafeteriaMaterialValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

bool containsError(
    const gdp::procedural::LaboratoryMaterialValidationResult& result,
    const gdp::procedural::LaboratoryMaterialValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

bool containsError(
    const gdp::procedural::TechnicalMaterialValidationResult& result,
    const gdp::procedural::TechnicalMaterialValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

bool containsError(
    const gdp::procedural::SecurityMaterialValidationResult& result,
    const gdp::procedural::SecurityMaterialValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

bool containsError(
    const gdp::procedural::CentralHubMaterialValidationResult& result,
    const gdp::procedural::CentralHubMaterialValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

float averageRed(
    const gdp::render::ImageData& image,
    const int minimumX,
    const int maximumX)
{
    std::uint64_t total = 0;
    std::size_t samples = 0;
    for (int y = 0; y < image.height; ++y) {
        for (int x = minimumX; x < maximumX; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y)
                * static_cast<std::size_t>(image.width)
                + static_cast<std::size_t>(x)) * 4U;
            total += image.rgba[index];
            ++samples;
        }
    }
    return static_cast<float>(total) / static_cast<float>(samples);
}

float averageRedRow(
    const gdp::render::ImageData& image,
    const int y,
    const int minimumX,
    const int maximumX)
{
    std::uint64_t total = 0;
    for (int x = minimumX; x < maximumX; ++x) {
        const std::size_t index = (static_cast<std::size_t>(y)
            * static_cast<std::size_t>(image.width)
            + static_cast<std::size_t>(x)) * 4U;
        total += image.rgba[index];
    }
    return static_cast<float>(total) / static_cast<float>(maximumX - minimumX);
}

std::uint64_t imageHash(const gdp::render::ImageData& image) noexcept
{
    std::uint64_t hash = 1'469'598'103'934'665'603ULL;
    for (const std::uint8_t channel : image.rgba) {
        hash ^= channel;
        hash *= 1'099'511'628'211ULL;
    }
    return hash;
}

bool pixelMatches(
    const gdp::render::ImageData& image,
    const int x,
    const int y,
    const std::array<std::uint8_t, 3>& color) noexcept
{
    const std::size_t pixel = (static_cast<std::size_t>(y)
        * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)) * 4U;
    return image.rgba[pixel] == color[0]
        && image.rgba[pixel + 1U] == color[1]
        && image.rgba[pixel + 2U] == color[2];
}

void testRepresentativeMaterialSet()
{
    using namespace gdp::procedural;

    constexpr gdp::core::Seed seed = 0x494E445553545259ULL;
    const IndustrialRoomMaterialSet first = generateIndustrialRoomMaterials(seed);
    const IndustrialRoomMaterialSet repeated = generateIndustrialRoomMaterials(seed);
    const IndustrialRoomMaterialSet different = generateIndustrialRoomMaterials(seed + 1U);
    expect(validateIndustrialRoomMaterials(first).isValid(),
        "representative Industrial Corridor material set validates");
    expect(first == repeated,
        "same room seed reproduces the complete material set exactly");
    expect(first.materialHash != different.materialHash,
        "different room seed changes the room material hash");
    expect(first.materialHash == 0x18131E4CBD3FEC4FULL,
        "golden Industrial Corridor material hash remains stable across builds");

    const auto& wall = first.surfaces[static_cast<std::size_t>(IndustrialSurface::Wall)];
    const auto& floor = first.surfaces[static_cast<std::size_t>(IndustrialSurface::Floor)];
    const auto& ceiling = first.surfaces[static_cast<std::size_t>(IndustrialSurface::Ceiling)];
    expect(wall.safetyStripe && wall.emissiveAmount == 0.0F,
        "wall reserves safety signage exclusively at its base");
    expect(!floor.safetyStripe && floor.family == MaterialFamily::BareSteel,
        "floor uses non-striped dark bare steel");
    expect(!ceiling.safetyStripe && ceiling.emissiveAmount > 0.0F,
        "ceiling reserves emission for regular linear lights");
    expect(wall.seed != floor.seed && floor.seed != ceiling.seed && wall.seed != ceiling.seed,
        "wall, floor and ceiling derive independent material sub-seeds");
}

void testStructuredSurfaceAlbedos()
{
    using namespace gdp::procedural;

    const IndustrialRoomMaterialSet materials = generateIndustrialRoomMaterials(12345);
    const auto wall = generateIndustrialSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(IndustrialSurface::Wall)]);
    const auto floor = generateIndustrialSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(IndustrialSurface::Floor)]);
    const auto ceiling = generateIndustrialSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(IndustrialSurface::Ceiling)]);
    expect(wall && floor && ceiling && gdp::render::validateImageData(*wall)
            && gdp::render::validateImageData(*floor)
            && gdp::render::validateImageData(*ceiling),
        "every industrial surface generates renderer-valid RGBA data");
    if (!wall || !floor || !ceiling) {
        return;
    }

    const auto& wallRecipe
        = materials.surfaces[static_cast<std::size_t>(IndustrialSurface::Wall)];
    const std::array<std::uint8_t, 3> yellow{
        static_cast<std::uint8_t>(wallRecipe.accentColor.x * 255.0F + 0.5F),
        static_cast<std::uint8_t>(wallRecipe.accentColor.y * 255.0F + 0.5F),
        static_cast<std::uint8_t>(wallRecipe.accentColor.z * 255.0F + 0.5F),
    };
    std::size_t stripePixels = 0;
    std::size_t yellowAboveBase = 0;
    const int stripeHeight = std::max(4, wall->height / 12);
    for (int y = 0; y < wall->height; ++y) {
        for (int x = 0; x < wall->width; ++x) {
            const std::size_t pixel = (static_cast<std::size_t>(y)
                * static_cast<std::size_t>(wall->width)
                + static_cast<std::size_t>(x)) * 4U;
            const bool isYellow = wall->rgba[pixel] == yellow[0]
                && wall->rgba[pixel + 1U] == yellow[1]
                && wall->rgba[pixel + 2U] == yellow[2];
            if (isYellow && y < stripeHeight) {
                ++stripePixels;
            } else if (isYellow) {
                ++yellowAboveBase;
            }
        }
    }
    expect(stripePixels > 40U && yellowAboveBase == 0U,
        "yellow safety stripe exists only in the wall base budget");

    const float floorCenter = averageRed(*floor, floor->width * 3 / 10, floor->width * 7 / 10);
    const float floorLeft = averageRed(*floor, 0, floor->width / 5);
    const float floorRight = averageRed(*floor, floor->width * 4 / 5, floor->width);
    expect(floorCenter > (floorLeft + floorRight) * 0.5F,
        "floor albedo preserves a brighter directional traffic lane");

    std::uint8_t brightestRed = 0U;
    std::uint8_t darkestRed = 255U;
    for (std::size_t pixel = 0; pixel < ceiling->rgba.size(); pixel += 4U) {
        brightestRed = std::max(brightestRed, ceiling->rgba[pixel]);
        darkestRed = std::min(darkestRed, ceiling->rgba[pixel]);
    }
    expect(brightestRed > darkestRed + 80U,
        "ceiling albedo contains high-contrast regular luminaires");

    expect(imageHash(*floor) == 0x2C3178D13AB6A494ULL,
        "Industrial floor albedo stays unchanged by the approved wall revision");
    expect(imageHash(*ceiling) == 0x785F4097723681BDULL,
        "Industrial ceiling albedo stays unchanged by the approved wall revision");
}

void testIndustrialHdMaterialMaps()
{
    using namespace gdp::procedural;

    constexpr std::array conditions{
        IndustrialCondition::Maintained,
        IndustrialCondition::Used,
        IndustrialCondition::Worn,
    };
    std::array<std::size_t, conditions.size()> metallicPixels{};
    for (std::size_t index = 0; index < conditions.size(); ++index) {
        const IndustrialSurfaceRecipe recipe = makeIndustrialWallArchetypeRecipe(
            conditions[index]);
        const auto images = generateIndustrialWallMaterialImages(recipe);
        expect(recipe.version == 2U && recipe.resolution == 1024U,
            "approved Industrial wall archetypes use the versioned 1024 HD contract");
        expect(images && images->albedo.width == 1024 && images->normal.width == 1024
                && images->properties.width == 1024
                && gdp::render::validateImageData(images->albedo)
                && gdp::render::validateImageData(images->normal)
                && gdp::render::validateImageData(images->properties),
            "HD wall produces renderer-valid Albedo, Normal and RMA maps");
        if (!images) {
            continue;
        }

        std::size_t perturbedNormals = 0U;
        std::uint8_t minimumRoughness = 255U;
        std::uint8_t maximumRoughness = 0U;
        std::uint8_t minimumOcclusion = 255U;
        for (std::size_t pixel = 0; pixel < images->normal.rgba.size(); pixel += 4U) {
            const int normalX = static_cast<int>(images->normal.rgba[pixel]) - 128;
            const int normalY = static_cast<int>(images->normal.rgba[pixel + 1U]) - 128;
            if (std::abs(normalX) + std::abs(normalY) >= 3) {
                ++perturbedNormals;
            }
            const std::uint8_t roughness = images->properties.rgba[pixel];
            const std::uint8_t metalness = images->properties.rgba[pixel + 1U];
            const std::uint8_t occlusion = images->properties.rgba[pixel + 2U];
            minimumRoughness = std::min(minimumRoughness, roughness);
            maximumRoughness = std::max(maximumRoughness, roughness);
            minimumOcclusion = std::min(minimumOcclusion, occlusion);
            if (metalness >= 180U) {
                ++metallicPixels[index];
            }
        }
        expect(perturbedNormals >= 20'000U,
            "HD normal map carries bevel, gasket, fastener and micro-surface depth");
        expect(maximumRoughness >= minimumRoughness + 70U,
            "RMA map separates painted steel, grime and exposed metal response");
        expect(minimumOcclusion <= 150U,
            "RMA map darkens physically recessed gasket joints");
    }
    expect(metallicPixels[0] < metallicPixels[1]
            && metallicPixels[1] < metallicPixels[2],
        "HD material maps increase exposed metal strictly across approved conditions");
}

void testApprovedIndustrialWallDirection()
{
    using namespace gdp::procedural;

    constexpr std::array<gdp::core::Seed, 3> seeds{2U, 3U, 1U};
    constexpr std::array expectedConditions{
        IndustrialCondition::Maintained,
        IndustrialCondition::Used,
        IndustrialCondition::Worn,
    };
    constexpr std::array<std::uint8_t, 3> exposedSteel{139U, 144U, 145U};
    constexpr std::array<std::uint8_t, 3> jointColor{21U, 26U, 28U};
    std::array<std::size_t, 3> exposedCounts{};

    for (std::size_t index = 0; index < seeds.size(); ++index) {
        const IndustrialRoomMaterialSet materials = generateIndustrialRoomMaterials(seeds[index]);
        expect(materials.condition == expectedConditions[index],
            "approved Industrial wall seeds preserve Maintained, Used and Worn ordering");
        const IndustrialSurfaceRecipe& recipe
            = materials.surfaces[static_cast<std::size_t>(IndustrialSurface::Wall)];
        const auto wall = generateIndustrialSurfaceAlbedo(recipe);
        const auto repeated = generateIndustrialSurfaceAlbedo(recipe);
        expect(wall && repeated && wall->rgba == repeated->rgba,
            "approved Industrial wall is byte-exact for a repeated seed");
        if (!wall) {
            continue;
        }

        const int cellWidth = wall->width / static_cast<int>(recipe.panelColumns);
        const int cellHeight = wall->height / static_cast<int>(recipe.panelRows);
        const int stripeHeight = std::max(4, wall->height / 12);
        std::size_t exposedInterior = 0U;
        std::size_t jointPixels = 0U;
        for (int y = 0; y < wall->height; ++y) {
            for (int x = 0; x < wall->width; ++x) {
                const int panelX = x % cellWidth;
                const int panelY = y % cellHeight;
                const int structuralWidth = std::max(32, wall->width / 32);
                const bool structuralEdge = panelX <= structuralWidth
                    || panelY <= structuralWidth
                    || panelX + structuralWidth >= cellWidth
                    || panelY + structuralWidth >= cellHeight
                    || y < stripeHeight;
                if (pixelMatches(*wall, x, y, exposedSteel)) {
                    ++exposedCounts[index];
                    if (!structuralEdge) {
                        ++exposedInterior;
                    }
                }
                if (pixelMatches(*wall, x, y, jointColor)) {
                    ++jointPixels;
                }
            }
        }
        expect(exposedInterior <= 2'000U,
            "interior exposed steel stays limited to intentional countersunk fasteners");
        expect(jointPixels >= 100U,
            "recessed dark joints and restrained fasteners remain structurally readable");
    }

    expect(exposedCounts[0] < exposedCounts[1] && exposedCounts[1] < exposedCounts[2],
        "approved seeds show strictly increasing edge wear from Maintained to Worn");
}

void testValidationFailures()
{
    using namespace gdp::procedural;

    const IndustrialRoomMaterialSet valid = generateIndustrialRoomMaterials(55);
    IndustrialSurfaceRecipe invalid = valid.surfaces[0];
    invalid.safetyStripe = false;
    expect(containsError(
               validateIndustrialSurfaceRecipe(invalid),
               IndustrialMaterialValidationError::InvalidSurfaceRule),
        "wall recipe without mandatory base stripe is rejected");
    invalid = valid.surfaces[1];
    invalid.panelColumns = 8;
    expect(containsError(
               validateIndustrialSurfaceRecipe(invalid),
               IndustrialMaterialValidationError::InvalidPanelGrid),
        "surface recipe outside the large-panel budget is rejected");
    invalid = valid.surfaces[2];
    invalid.resolution = 63;
    expect(containsError(
               validateIndustrialSurfaceRecipe(invalid),
               IndustrialMaterialValidationError::InvalidResolution),
        "non-power-of-two industrial texture is rejected");
    expect(!generateIndustrialSurfaceAlbedo(invalid),
        "invalid surface recipe never produces partial image data");

    IndustrialRoomMaterialSet corrupted = valid;
    std::swap(corrupted.surfaces[0], corrupted.surfaces[1]);
    expect(containsError(
               validateIndustrialRoomMaterials(corrupted),
               IndustrialMaterialValidationError::InvalidSurfaceOrder),
        "material set rejects swapped surface channels");
    corrupted = valid;
    corrupted.materialHash ^= 1U;
    expect(containsError(
               validateIndustrialRoomMaterials(corrupted),
               IndustrialMaterialValidationError::InvalidMaterialHash),
        "material set detects canonical hash corruption");
}

void testRoomMaterialSeedBatch()
{
    using namespace gdp::procedural;

    std::array<bool, static_cast<std::size_t>(IndustrialCondition::Count)> conditions{};
    std::unordered_set<std::uint64_t> hashes;
    bool allValid = true;
    for (gdp::core::Seed seed = 0; seed < 1'000; ++seed) {
        const IndustrialRoomMaterialSet materials = generateIndustrialRoomMaterials(seed);
        if (!validateIndustrialRoomMaterials(materials).isValid()) {
            std::cerr << "Industrial material validation failed at room seed " << seed << '\n';
            allValid = false;
            break;
        }
        for (const IndustrialSurfaceRecipe& recipe : materials.surfaces) {
            if (recipe.surface == IndustrialSurface::Wall) {
                continue;
            }
            const auto image = generateIndustrialSurfaceAlbedo(recipe);
            if (!image || !gdp::render::validateImageData(*image)) {
                std::cerr << "Industrial albedo generation failed at room seed " << seed << '\n';
                allValid = false;
                break;
            }
        }
        conditions[static_cast<std::size_t>(materials.condition)] = true;
        hashes.insert(materials.materialHash);
    }
    expect(allValid,
        "1,000 room seeds validate every recipe and preserve legacy floor/ceiling albedos");
    expect(hashes.size() == 1'000U,
        "1,000 room seeds produce unique canonical material hashes");
    expect(std::all_of(conditions.begin(), conditions.end(), [](const bool used) { return used; }),
        "room seed batch reaches every industrial maintenance condition");
}

void testRepresentativeStorageMaterialSet()
{
    using namespace gdp::procedural;

    constexpr gdp::core::Seed seed = 0x53544F524147455FULL;
    const StorageRoomMaterialSet first = generateStorageRoomMaterials(seed);
    const StorageRoomMaterialSet repeated = generateStorageRoomMaterials(seed);
    const StorageRoomMaterialSet different = generateStorageRoomMaterials(seed + 1U);
    expect(validateStorageRoomMaterials(first).isValid(),
        "representative Storage material set validates");
    expect(first == repeated,
        "same Storage room seed reproduces every recipe exactly");
    expect(first.materialHash != different.materialHash,
        "different Storage room seed changes its canonical material hash");
    expect(first.materialHash == 0x829EF82CD0438691ULL,
        "golden Storage material hash remains stable across builds");

    const auto& wall = first.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    const auto& floor = first.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const auto& ceiling = first.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    expect(wall.family == MaterialFamily::PaintedSteel && wall.oxidationAmount > 0.0F,
        "Storage wall uses locally oxidized aged painted steel");
    expect(floor.family == MaterialFamily::Ceramic && floor.oxidationAmount == 0.0F,
        "Storage floor represents non-oxidizing industrial concrete");
    expect(ceiling.family == MaterialFamily::BareSteel && ceiling.emissiveAmount > 0.0F,
        "Storage ceiling reserves weak emission for spaced fixtures");
    expect(wall.seed != floor.seed && floor.seed != ceiling.seed && wall.seed != ceiling.seed,
        "Storage surfaces use independent room-material sub-seeds");
}

void testStructuredStorageAlbedos()
{
    using namespace gdp::procedural;

    const StorageRoomMaterialSet materials = generateStorageRoomMaterials(24680);
    const auto wall = generateStorageSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)]);
    const auto floor = generateStorageSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)]);
    const auto ceiling = generateStorageSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)]);
    expect(wall && floor && ceiling && gdp::render::validateImageData(*wall)
            && gdp::render::validateImageData(*floor)
            && gdp::render::validateImageData(*ceiling),
        "every Storage surface generates renderer-valid RGBA data");
    if (!wall || !floor || !ceiling) {
        return;
    }

    const auto& wallRecipe = materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    const std::array<std::uint8_t, 3> marking{
        static_cast<std::uint8_t>(wallRecipe.accentColor.x * 1.12F * 255.0F + 0.5F),
        static_cast<std::uint8_t>(wallRecipe.accentColor.y * 1.12F * 255.0F + 0.5F),
        static_cast<std::uint8_t>(wallRecipe.accentColor.z * 1.12F * 255.0F + 0.5F),
    };
    std::size_t markingPixels = 0;
    for (std::size_t pixel = 0; pixel < wall->rgba.size(); pixel += 4U) {
        if (wall->rgba[pixel] == marking[0] && wall->rgba[pixel + 1U] == marking[1]
            && wall->rgba[pixel + 2U] == marking[2]) {
            ++markingPixels;
        }
    }
    expect(markingPixels >= 24U && markingPixels <= 40U,
        "Storage wall keeps logistics marking short instead of continuous");

    const int minimumX = floor->width / 5;
    const int maximumX = floor->width * 4 / 5;
    const int dragY = floor->height / 3;
    expect(averageRedRow(*floor, dragY, minimumX, maximumX)
            < averageRedRow(*floor, dragY + 1, minimumX, maximumX),
        "Storage concrete floor contains a controlled horizontal drag mark");

    std::uint8_t brightestRed = 0U;
    std::uint8_t darkestRed = 255U;
    for (std::size_t pixel = 0; pixel < ceiling->rgba.size(); pixel += 4U) {
        brightestRed = std::max(brightestRed, ceiling->rgba[pixel]);
        darkestRed = std::min(darkestRed, ceiling->rgba[pixel]);
    }
    expect(brightestRed > darkestRed + 70U,
        "Storage ceiling preserves readable contrast despite weaker irregular lighting");
}

void testStorageValidationFailures()
{
    using namespace gdp::procedural;

    const StorageRoomMaterialSet valid = generateStorageRoomMaterials(77);
    StorageSurfaceRecipe invalid = valid.surfaces[1];
    invalid.oxidationAmount = 0.5F;
    expect(containsError(
               validateStorageSurfaceRecipe(invalid),
               StorageMaterialValidationError::InvalidSurfaceRule),
        "Storage concrete floor rejects oxidation");
    invalid = valid.surfaces[2];
    invalid.emissiveAmount = 0.0F;
    expect(containsError(
               validateStorageSurfaceRecipe(invalid),
               StorageMaterialValidationError::InvalidSurfaceRule),
        "Storage ceiling without its navigation-light budget is rejected");
    invalid = valid.surfaces[0];
    invalid.panelRows = 5;
    expect(containsError(
               validateStorageSurfaceRecipe(invalid),
               StorageMaterialValidationError::InvalidPanelGrid),
        "Storage surface rejects a noisy small-panel grid");
    expect(!generateStorageSurfaceAlbedo(invalid),
        "invalid Storage recipe never produces partial image data");

    StorageRoomMaterialSet corrupted = valid;
    corrupted.materialHash ^= 1U;
    expect(containsError(
               validateStorageRoomMaterials(corrupted),
               StorageMaterialValidationError::InvalidMaterialHash),
        "Storage material set detects canonical hash corruption");
}

void testStorageSeedBatch()
{
    using namespace gdp::procedural;

    std::array<bool, static_cast<std::size_t>(StorageCondition::Count)> conditions{};
    std::unordered_set<std::uint64_t> hashes;
    bool allValid = true;
    for (gdp::core::Seed seed = 0; seed < 1'000; ++seed) {
        const StorageRoomMaterialSet materials = generateStorageRoomMaterials(seed);
        if (!validateStorageRoomMaterials(materials).isValid()) {
            std::cerr << "Storage material validation failed at room seed " << seed << '\n';
            allValid = false;
            break;
        }
        for (const StorageSurfaceRecipe& recipe : materials.surfaces) {
            const auto image = generateStorageSurfaceAlbedo(recipe);
            if (!image || !gdp::render::validateImageData(*image)) {
                std::cerr << "Storage albedo generation failed at room seed " << seed << '\n';
                allValid = false;
                break;
            }
        }
        conditions[static_cast<std::size_t>(materials.condition)] = true;
        hashes.insert(materials.materialHash);
    }
    expect(allValid, "1,000 room seeds pass every Storage material and albedo gate");
    expect(hashes.size() == 1'000U,
        "1,000 Storage seeds produce unique canonical material hashes");
    expect(std::all_of(conditions.begin(), conditions.end(), [](const bool used) { return used; }),
        "Storage seed batch reaches every maintenance condition");
}

void testRepresentativeCombatMaterialSet()
{
    using namespace gdp::procedural;

    constexpr gdp::core::Seed seed = 0x434F4D4241545F52ULL;
    const CombatRoomMaterialSet first = generateCombatRoomMaterials(seed);
    const CombatRoomMaterialSet repeated = generateCombatRoomMaterials(seed);
    const CombatRoomMaterialSet different = generateCombatRoomMaterials(seed + 1U);
    expect(validateCombatRoomMaterials(first).isValid(),
        "representative Combat Room material set validates");
    expect(first == repeated,
        "same Combat Room seed reproduces every recipe exactly");
    expect(first.materialHash != different.materialHash,
        "different Combat Room seed changes its canonical material hash");
    expect(first.materialHash == 0x144C416CBE295EC8ULL,
        "golden Combat Room material hash remains stable across builds");

    const auto& wall = first.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    const auto& floor = first.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const auto& ceiling = first.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    expect(wall.family == MaterialFamily::PaintedSteel && wall.impactAmount > 0.0F
            && wall.reinforcementAmount > 0.0F,
        "Combat wall combines olive painted armor with controlled impacts");
    expect(floor.family == MaterialFamily::Ceramic && floor.markingAmount > 0.0F
            && floor.reinforcementAmount == 0.0F,
        "Combat floor reserves marking for a clear arena perimeter");
    expect(ceiling.family == MaterialFamily::BareSteel && ceiling.emissiveAmount > 0.0F
            && ceiling.impactAmount == 0.0F && ceiling.markingAmount == 0.0F,
        "Combat ceiling uses reinforced dark steel with navigational lighting");
    expect(wall.seed != floor.seed && floor.seed != ceiling.seed && wall.seed != ceiling.seed,
        "Combat surfaces use independent room-material sub-seeds");
}

void testStructuredCombatAlbedos()
{
    using namespace gdp::procedural;

    const CombatRoomMaterialSet materials = generateCombatRoomMaterials(13579);
    const auto wall = generateCombatSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)]);
    const auto floor = generateCombatSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)]);
    const auto ceiling = generateCombatSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)]);
    expect(wall && floor && ceiling && gdp::render::validateImageData(*wall)
            && gdp::render::validateImageData(*floor)
            && gdp::render::validateImageData(*ceiling),
        "every Combat Room surface generates renderer-valid RGBA data");
    if (!wall || !floor || !ceiling) {
        return;
    }

    constexpr std::array<std::uint8_t, 3> impactCore{11U, 13U, 10U};
    std::size_t impactCorePixels = 0;
    for (std::size_t pixel = 0; pixel < wall->rgba.size(); pixel += 4U) {
        if (wall->rgba[pixel] == impactCore[0]
            && wall->rgba[pixel + 1U] == impactCore[1]
            && wall->rgba[pixel + 2U] == impactCore[2]) {
            ++impactCorePixels;
        }
    }
    expect(impactCorePixels >= 3U && impactCorePixels <= 15U,
        "Combat wall contains one to three localized impact cores");

    const auto& floorRecipe = materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const std::array<std::uint8_t, 3> arenaColor{
        static_cast<std::uint8_t>(floorRecipe.accentColor.x * 255.0F + 0.5F),
        static_cast<std::uint8_t>(floorRecipe.accentColor.y * 255.0F + 0.5F),
        static_cast<std::uint8_t>(floorRecipe.accentColor.z * 255.0F + 0.5F),
    };
    std::size_t arenaPixels = 0;
    for (std::size_t pixel = 0; pixel < floor->rgba.size(); pixel += 4U) {
        if (floor->rgba[pixel] == arenaColor[0]
            && floor->rgba[pixel + 1U] == arenaColor[1]
            && floor->rgba[pixel + 2U] == arenaColor[2]) {
            ++arenaPixels;
        }
    }
    expect(arenaPixels >= 180U && arenaPixels <= 192U,
        "Combat floor draws one muted and continuous arena perimeter");

    std::uint8_t brightestRed = 0U;
    std::uint8_t darkestRed = 255U;
    for (std::size_t pixel = 0; pixel < ceiling->rgba.size(); pixel += 4U) {
        brightestRed = std::max(brightestRed, ceiling->rgba[pixel]);
        darkestRed = std::min(darkestRed, ceiling->rgba[pixel]);
    }
    expect(brightestRed > darkestRed + 100U,
        "Combat ceiling keeps heavy beams readable beneath spaced fixtures");
}

void testCombatValidationFailures()
{
    using namespace gdp::procedural;

    const CombatRoomMaterialSet valid = generateCombatRoomMaterials(99);
    CombatSurfaceRecipe invalid = valid.surfaces[0];
    invalid.impactAmount = 0.0F;
    expect(containsError(
               validateCombatSurfaceRecipe(invalid),
               CombatMaterialValidationError::InvalidSurfaceRule),
        "Combat wall without impact budget is rejected");
    invalid = valid.surfaces[1];
    invalid.reinforcementAmount = 0.5F;
    expect(containsError(
               validateCombatSurfaceRecipe(invalid),
               CombatMaterialValidationError::InvalidSurfaceRule),
        "Combat floor rejects wall-style reinforcement");
    invalid = valid.surfaces[2];
    invalid.emissiveAmount = 0.0F;
    expect(containsError(
               validateCombatSurfaceRecipe(invalid),
               CombatMaterialValidationError::InvalidSurfaceRule),
        "Combat ceiling without navigation-light budget is rejected");
    invalid.resolution = 63;
    expect(containsError(
               validateCombatSurfaceRecipe(invalid),
               CombatMaterialValidationError::InvalidResolution),
        "Combat texture outside the power-of-two contract is rejected");
    expect(!generateCombatSurfaceAlbedo(invalid),
        "invalid Combat recipe never produces partial image data");

    CombatRoomMaterialSet corrupted = valid;
    corrupted.materialHash ^= 1U;
    expect(containsError(
               validateCombatRoomMaterials(corrupted),
               CombatMaterialValidationError::InvalidMaterialHash),
        "Combat material set detects canonical hash corruption");
}

void testCombatSeedBatch()
{
    using namespace gdp::procedural;

    std::array<bool, static_cast<std::size_t>(CombatCondition::Count)> conditions{};
    std::unordered_set<std::uint64_t> hashes;
    bool allValid = true;
    for (gdp::core::Seed seed = 0; seed < 1'000; ++seed) {
        const CombatRoomMaterialSet materials = generateCombatRoomMaterials(seed);
        if (!validateCombatRoomMaterials(materials).isValid()) {
            std::cerr << "Combat material validation failed at room seed " << seed << '\n';
            allValid = false;
            break;
        }
        for (const CombatSurfaceRecipe& recipe : materials.surfaces) {
            const auto image = generateCombatSurfaceAlbedo(recipe);
            if (!image || !gdp::render::validateImageData(*image)) {
                std::cerr << "Combat albedo generation failed at room seed " << seed << '\n';
                allValid = false;
                break;
            }
        }
        conditions[static_cast<std::size_t>(materials.condition)] = true;
        hashes.insert(materials.materialHash);
    }
    expect(allValid, "1,000 room seeds pass every Combat material and albedo gate");
    expect(hashes.size() == 1'000U,
        "1,000 Combat seeds produce unique canonical material hashes");
    expect(std::all_of(conditions.begin(), conditions.end(), [](const bool used) { return used; }),
        "Combat seed batch reaches every battle-wear condition");
}

void testRepresentativeCafeteriaMaterialSet()
{
    using namespace gdp::procedural;

    constexpr gdp::core::Seed seed = 0x4341464554455249ULL;
    const CafeteriaRoomMaterialSet first = generateCafeteriaRoomMaterials(seed);
    const CafeteriaRoomMaterialSet repeated = generateCafeteriaRoomMaterials(seed);
    const CafeteriaRoomMaterialSet different = generateCafeteriaRoomMaterials(seed + 1U);
    expect(validateCafeteriaRoomMaterials(first).isValid(),
        "representative Cafeteria material set validates");
    expect(first == repeated,
        "same Cafeteria seed reproduces every recipe exactly");
    expect(first.materialHash != different.materialHash,
        "different Cafeteria seed changes its canonical material hash");
    expect(first.materialHash == 0xB96EA4D463428B1BULL,
        "golden Cafeteria material hash remains stable across builds");

    const auto& wall = first.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    const auto& floor = first.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const auto& ceiling = first.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    expect(wall.family == MaterialFamily::Ceramic
            && wall.stripeAmount >= 0.08F && wall.stripeAmount <= 0.12F,
        "Cafeteria wall reserves a narrow institutional red stripe");
    expect(floor.family == MaterialFamily::Ceramic && floor.stripeAmount == 0.0F
            && floor.furnitureMarkAmount > 0.0F,
        "Cafeteria floor reserves wear for traffic and furniture contact");
    expect(ceiling.family == MaterialFamily::Aluminum && ceiling.emissiveAmount > 0.0F
            && ceiling.stripeAmount == 0.0F,
        "Cafeteria ceiling uses regular light panels with uniform emission");
    expect(wall.seed != floor.seed && floor.seed != ceiling.seed && wall.seed != ceiling.seed,
        "Cafeteria surfaces use independent room-material sub-seeds");
}

void testStructuredCafeteriaAlbedos()
{
    using namespace gdp::procedural;

    const CafeteriaRoomMaterialSet materials = generateCafeteriaRoomMaterials(97531);
    const auto wall = generateCafeteriaSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)]);
    const auto floor = generateCafeteriaSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)]);
    const auto ceiling = generateCafeteriaSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)]);
    expect(wall && floor && ceiling && gdp::render::validateImageData(*wall)
            && gdp::render::validateImageData(*floor)
            && gdp::render::validateImageData(*ceiling),
        "every Cafeteria surface generates renderer-valid RGBA data");
    if (!wall || !floor || !ceiling) {
        return;
    }

    const auto& wallRecipe = materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    const std::array<std::uint8_t, 3> red{
        static_cast<std::uint8_t>(wallRecipe.accentColor.x * 255.0F + 0.5F),
        static_cast<std::uint8_t>(wallRecipe.accentColor.y * 255.0F + 0.5F),
        static_cast<std::uint8_t>(wallRecipe.accentColor.z * 255.0F + 0.5F),
    };
    const int stripeStart = wall->height * 2 / 5;
    const int stripeHeight = std::max(5,
        static_cast<int>(std::lround(
            static_cast<float>(wall->height) * wallRecipe.stripeAmount)));
    std::size_t stripePixels = 0;
    std::size_t redOutsideStripe = 0;
    for (int y = 0; y < wall->height; ++y) {
        for (int x = 0; x < wall->width; ++x) {
            const std::size_t pixel = (static_cast<std::size_t>(y)
                * static_cast<std::size_t>(wall->width)
                + static_cast<std::size_t>(x)) * 4U;
            const bool isRed = wall->rgba[pixel] == red[0]
                && wall->rgba[pixel + 1U] == red[1]
                && wall->rgba[pixel + 2U] == red[2];
            if (isRed && y >= stripeStart && y < stripeStart + stripeHeight) {
                ++stripePixels;
            } else if (isRed) {
                ++redOutsideStripe;
            }
        }
    }
    const std::size_t stripeBudget = static_cast<std::size_t>(wall->width * stripeHeight);
    expect(stripePixels + 20U >= stripeBudget && redOutsideStripe == 0U,
        "Cafeteria red stripe remains continuous, narrow and vertically aligned");

    const float floorCenter = averageRed(*floor, floor->width * 3 / 8, floor->width * 5 / 8);
    const float floorSides = (averageRed(*floor, 0, floor->width / 4)
        + averageRed(*floor, floor->width * 3 / 4, floor->width)) * 0.5F;
    expect(floorCenter > floorSides,
        "Cafeteria floor preserves a subtly polished circulation lane");

    const auto& floorRecipe = materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const float markTone = 0.68F + floorRecipe.furnitureMarkAmount;
    const std::array<std::uint8_t, 3> furnitureMark{
        static_cast<std::uint8_t>(floorRecipe.secondaryColor.x * markTone * 255.0F + 0.5F),
        static_cast<std::uint8_t>(floorRecipe.secondaryColor.y * markTone * 255.0F + 0.5F),
        static_cast<std::uint8_t>(floorRecipe.secondaryColor.z * markTone * 255.0F + 0.5F),
    };
    std::size_t furniturePixels = 0;
    for (std::size_t pixel = 0; pixel < floor->rgba.size(); pixel += 4U) {
        if (floor->rgba[pixel] == furnitureMark[0]
            && floor->rgba[pixel + 1U] == furnitureMark[1]
            && floor->rgba[pixel + 2U] == furnitureMark[2]) {
            ++furniturePixels;
        }
    }
    expect(furniturePixels == 48U,
        "Cafeteria floor contains four controlled circular furniture contacts");

    std::uint8_t brightestRed = 0U;
    std::uint8_t darkestRed = 255U;
    for (std::size_t pixel = 0; pixel < ceiling->rgba.size(); pixel += 4U) {
        brightestRed = std::max(brightestRed, ceiling->rgba[pixel]);
        darkestRed = std::min(darkestRed, ceiling->rgba[pixel]);
    }
    expect(brightestRed > darkestRed + 80U,
        "Cafeteria ceiling keeps regular light panels clearly readable");
}

void testCafeteriaValidationFailures()
{
    using namespace gdp::procedural;

    const CafeteriaRoomMaterialSet valid = generateCafeteriaRoomMaterials(123);
    CafeteriaSurfaceRecipe invalid = valid.surfaces[0];
    invalid.stripeAmount = 0.0F;
    expect(containsError(
               validateCafeteriaSurfaceRecipe(invalid),
               CafeteriaMaterialValidationError::InvalidSurfaceRule),
        "Cafeteria wall without its institutional stripe is rejected");
    invalid = valid.surfaces[1];
    invalid.panelRows = 3;
    expect(containsError(
               validateCafeteriaSurfaceRecipe(invalid),
               CafeteriaMaterialValidationError::InvalidSurfaceRule),
        "Cafeteria floor rejects a non-square or noisy tile grid");
    invalid = valid.surfaces[2];
    invalid.emissiveAmount = 0.0F;
    expect(containsError(
               validateCafeteriaSurfaceRecipe(invalid),
               CafeteriaMaterialValidationError::InvalidSurfaceRule),
        "Cafeteria ceiling without uniform-light budget is rejected");
    invalid.resolution = 63;
    expect(containsError(
               validateCafeteriaSurfaceRecipe(invalid),
               CafeteriaMaterialValidationError::InvalidResolution),
        "Cafeteria texture outside the power-of-two contract is rejected");
    expect(!generateCafeteriaSurfaceAlbedo(invalid),
        "invalid Cafeteria recipe never produces partial image data");

    CafeteriaRoomMaterialSet corrupted = valid;
    corrupted.materialHash ^= 1U;
    expect(containsError(
               validateCafeteriaRoomMaterials(corrupted),
               CafeteriaMaterialValidationError::InvalidMaterialHash),
        "Cafeteria material set detects canonical hash corruption");
}

void testCafeteriaSeedBatch()
{
    using namespace gdp::procedural;

    std::array<bool, static_cast<std::size_t>(CafeteriaCondition::Count)> conditions{};
    std::unordered_set<std::uint64_t> hashes;
    bool allValid = true;
    for (gdp::core::Seed seed = 0; seed < 1'000; ++seed) {
        const CafeteriaRoomMaterialSet materials = generateCafeteriaRoomMaterials(seed);
        if (!validateCafeteriaRoomMaterials(materials).isValid()) {
            std::cerr << "Cafeteria validation failed at room seed " << seed << '\n';
            allValid = false;
            break;
        }
        for (const CafeteriaSurfaceRecipe& recipe : materials.surfaces) {
            const auto image = generateCafeteriaSurfaceAlbedo(recipe);
            if (!image || !gdp::render::validateImageData(*image)) {
                std::cerr << "Cafeteria albedo generation failed at room seed " << seed << '\n';
                allValid = false;
                break;
            }
        }
        conditions[static_cast<std::size_t>(materials.condition)] = true;
        hashes.insert(materials.materialHash);
    }
    expect(allValid, "1,000 room seeds pass every Cafeteria material and albedo gate");
    expect(hashes.size() == 1'000U,
        "1,000 Cafeteria seeds produce unique canonical material hashes");
    expect(std::all_of(conditions.begin(), conditions.end(), [](const bool used) { return used; }),
        "Cafeteria seed batch reaches every cleanliness condition");
}

void testRepresentativeLaboratoryMaterialSet()
{
    using namespace gdp::procedural;

    constexpr gdp::core::Seed seed = 0x4C41424F5241544FULL;
    const LaboratoryRoomMaterialSet first = generateLaboratoryRoomMaterials(seed);
    const LaboratoryRoomMaterialSet repeated = generateLaboratoryRoomMaterials(seed);
    const LaboratoryRoomMaterialSet different = generateLaboratoryRoomMaterials(seed + 1U);
    expect(validateLaboratoryRoomMaterials(first).isValid(),
        "representative Laboratory material set validates");
    expect(first == repeated,
        "same Laboratory seed reproduces every recipe exactly");
    expect(first.materialHash != different.materialHash,
        "different Laboratory seed changes its canonical material hash");
    expect(first.materialHash == 0xCC56AB4C128F0EC7ULL,
        "golden Laboratory material hash remains stable across builds");

    const auto& wall = first.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    const auto& floor = first.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const auto& ceiling = first.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    expect(wall.family == MaterialFamily::Ceramic && wall.detailAmount > 0.0F
            && wall.contaminationAmount == 0.0F,
        "Laboratory wall uses precise clean panels and organized technical detail");
    expect(floor.family == MaterialFamily::Polymer
            && floor.panelColumns == floor.panelRows,
        "Laboratory floor uses a regular cold technical grid");
    expect(ceiling.family == MaterialFamily::Aluminum
            && ceiling.lightingCoverage >= 0.25F && ceiling.lightingCoverage <= 0.34F
            && ceiling.emissiveAmount > 0.0F,
        "Laboratory ceiling reserves a controlled fraction for intense white lights");
    expect(wall.seed != floor.seed && floor.seed != ceiling.seed && wall.seed != ceiling.seed,
        "Laboratory surfaces use independent room-material sub-seeds");
}

void testStructuredLaboratoryAlbedos()
{
    using namespace gdp::procedural;

    const LaboratoryRoomMaterialSet materials = generateLaboratoryRoomMaterials(86420);
    const auto wall = generateLaboratorySurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)]);
    const auto floor = generateLaboratorySurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)]);
    const auto ceiling = generateLaboratorySurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)]);
    expect(wall && floor && ceiling && gdp::render::validateImageData(*wall)
            && gdp::render::validateImageData(*floor)
            && gdp::render::validateImageData(*ceiling),
        "every Laboratory surface generates renderer-valid RGBA data");
    if (!wall || !floor || !ceiling) {
        return;
    }

    constexpr std::array<std::uint8_t, 3> statusGreen{64U, 140U, 82U};
    std::size_t indicatorPixels = 0;
    for (std::size_t pixel = 0; pixel < wall->rgba.size(); pixel += 4U) {
        if (wall->rgba[pixel] == statusGreen[0]
            && wall->rgba[pixel + 1U] == statusGreen[1]
            && wall->rgba[pixel + 2U] == statusGreen[2]) {
            ++indicatorPixels;
        }
    }
    expect(indicatorPixels == 8U,
        "Laboratory wall limits status lights to two organized indicators");

    const auto& floorRecipe = materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const int cellHeight = floor->height / floorRecipe.panelRows;
    expect(averageRedRow(*floor, 0, 0, floor->width)
            < averageRedRow(*floor, cellHeight / 2, 0, floor->width),
        "Laboratory floor preserves thin, precise modular seams");

    gdp::core::Seed containedSeed = 0;
    LaboratoryRoomMaterialSet contained;
    for (; containedSeed < 1'000; ++containedSeed) {
        contained = generateLaboratoryRoomMaterials(containedSeed);
        if (contained.condition == LaboratoryCondition::Contained) {
            break;
        }
    }
    const auto containedFloor = generateLaboratorySurfaceAlbedo(
        contained.surfaces[static_cast<std::size_t>(RoomSurface::Floor)]);
    expect(containedSeed < 1'000 && containedFloor,
        "seed batch exposes a contained-contamination Laboratory floor");
    if (containedFloor) {
        const auto channel = [&containedFloor](const int x, const int y, const std::size_t offset) {
            const std::size_t pixel = (static_cast<std::size_t>(y)
                * static_cast<std::size_t>(containedFloor->width)
                + static_cast<std::size_t>(x)) * 4U;
            return static_cast<int>(containedFloor->rgba[pixel + offset]);
        };
        const int contaminatedBlueRed = channel(48, 16, 2U) - channel(48, 16, 0U);
        const int cleanBlueRed = channel(16, 48, 2U) - channel(16, 48, 0U);
        expect(contaminatedBlueRed + 8 < cleanBlueRed,
            "Laboratory contamination remains localized to one desaturated floor patch");
    }

    const auto& ceilingRecipe
        = materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    const std::uint32_t panelCount = static_cast<std::uint32_t>(ceilingRecipe.panelColumns)
        * static_cast<std::uint32_t>(ceilingRecipe.panelRows);
    const std::uint32_t fixtureCount = std::max(1U,
        static_cast<std::uint32_t>(std::lround(
            static_cast<float>(panelCount) * ceilingRecipe.lightingCoverage)));
    const std::uint32_t cellWidth = ceilingRecipe.resolution / ceilingRecipe.panelColumns;
    const std::uint32_t fixtureHalfWidth = std::max(5U, cellWidth / 4U);
    const std::size_t expectedBrightPixels = static_cast<std::size_t>(fixtureCount)
        * static_cast<std::size_t>((fixtureHalfWidth * 2U) + 1U) * 5U;
    std::size_t brightPixels = 0;
    for (std::size_t pixel = 0; pixel < ceiling->rgba.size(); pixel += 4U) {
        if (ceiling->rgba[pixel] > 210U && ceiling->rgba[pixel + 1U] > 210U
            && ceiling->rgba[pixel + 2U] > 210U) {
            ++brightPixels;
        }
    }
    expect(brightPixels == expectedBrightPixels
            && fixtureCount * 5U >= panelCount && fixtureCount * 5U <= panelCount * 2U,
        "Laboratory ceiling lights occupy a precise 20-40% modular budget");
}

void testLaboratoryValidationFailures()
{
    using namespace gdp::procedural;

    const LaboratoryRoomMaterialSet valid = generateLaboratoryRoomMaterials(321);
    LaboratorySurfaceRecipe invalid = valid.surfaces[0];
    invalid.contaminationAmount = 0.1F;
    expect(containsError(
               validateLaboratorySurfaceRecipe(invalid),
               LaboratoryMaterialValidationError::InvalidSurfaceRule),
        "Laboratory wall rejects contamination outside the floor budget");
    invalid = valid.surfaces[1];
    invalid.family = MaterialFamily::Ceramic;
    expect(containsError(
               validateLaboratorySurfaceRecipe(invalid),
               LaboratoryMaterialValidationError::InvalidSurfaceRule),
        "Laboratory floor rejects the wall material family");
    invalid = valid.surfaces[2];
    invalid.lightingCoverage = 0.0F;
    expect(containsError(
               validateLaboratorySurfaceRecipe(invalid),
               LaboratoryMaterialValidationError::InvalidSurfaceRule),
        "Laboratory ceiling without its modular light budget is rejected");
    invalid.resolution = 63;
    expect(containsError(
               validateLaboratorySurfaceRecipe(invalid),
               LaboratoryMaterialValidationError::InvalidResolution),
        "Laboratory texture outside the power-of-two contract is rejected");
    expect(!generateLaboratorySurfaceAlbedo(invalid),
        "invalid Laboratory recipe never produces partial image data");

    LaboratoryRoomMaterialSet corrupted = valid;
    corrupted.materialHash ^= 1U;
    expect(containsError(
               validateLaboratoryRoomMaterials(corrupted),
               LaboratoryMaterialValidationError::InvalidMaterialHash),
        "Laboratory material set detects canonical hash corruption");
}

void testLaboratorySeedBatch()
{
    using namespace gdp::procedural;

    std::array<bool, static_cast<std::size_t>(LaboratoryCondition::Count)> conditions{};
    std::unordered_set<std::uint64_t> hashes;
    bool allValid = true;
    for (gdp::core::Seed seed = 0; seed < 1'000; ++seed) {
        const LaboratoryRoomMaterialSet materials = generateLaboratoryRoomMaterials(seed);
        if (!validateLaboratoryRoomMaterials(materials).isValid()) {
            std::cerr << "Laboratory validation failed at room seed " << seed << '\n';
            allValid = false;
            break;
        }
        for (const LaboratorySurfaceRecipe& recipe : materials.surfaces) {
            const auto image = generateLaboratorySurfaceAlbedo(recipe);
            if (!image || !gdp::render::validateImageData(*image)) {
                std::cerr << "Laboratory albedo generation failed at room seed " << seed << '\n';
                allValid = false;
                break;
            }
        }
        conditions[static_cast<std::size_t>(materials.condition)] = true;
        hashes.insert(materials.materialHash);
    }
    expect(allValid, "1,000 room seeds pass every Laboratory material and albedo gate");
    expect(hashes.size() == 1'000U,
        "1,000 Laboratory seeds produce unique canonical material hashes");
    expect(std::all_of(conditions.begin(), conditions.end(), [](const bool used) { return used; }),
        "Laboratory seed batch reaches every containment condition");
}

void testRepresentativeTechnicalMaterialSet()
{
    using namespace gdp::procedural;

    constexpr gdp::core::Seed seed = 0x544543484E494341ULL;
    const TechnicalRoomMaterialSet first = generateTechnicalRoomMaterials(seed);
    const TechnicalRoomMaterialSet repeated = generateTechnicalRoomMaterials(seed);
    const TechnicalRoomMaterialSet different = generateTechnicalRoomMaterials(seed + 1U);
    expect(validateTechnicalRoomMaterials(first).isValid(),
        "representative Technical Area material set validates");
    expect(first == repeated,
        "same Technical Area seed reproduces every recipe exactly");
    expect(first.materialHash != different.materialHash,
        "different Technical Area seed changes its canonical material hash");
    expect(first.materialHash == 0x5477925CC393D786ULL,
        "golden Technical Area material hash remains stable across builds");

    const auto& wall = first.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    const auto& floor = first.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const auto& ceiling = first.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    expect(wall.family == MaterialFamily::PaintedSteel && wall.pipeAmount > 0.0F
            && wall.oilAmount == 0.0F,
        "Technical wall reserves visual density for connected pipes and machinery");
    expect(floor.family == MaterialFamily::BareSteel && floor.pipeAmount == 0.0F
            && floor.oilAmount > 0.0F,
        "Technical floor reserves a controlled oil and grate budget");
    expect(ceiling.family == MaterialFamily::BareSteel && ceiling.pipeAmount > 0.0F
            && ceiling.mechanicalAmount > 0.0F && ceiling.emissiveAmount > 0.0F,
        "Technical ceiling combines categorized mechanics with protected lighting");
    expect(wall.seed != floor.seed && floor.seed != ceiling.seed && wall.seed != ceiling.seed,
        "Technical surfaces use independent room-material sub-seeds");
}

void testStructuredTechnicalAlbedos()
{
    using namespace gdp::procedural;

    const TechnicalRoomMaterialSet materials = generateTechnicalRoomMaterials(112233);
    const auto wall = generateTechnicalSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)]);
    const auto floor = generateTechnicalSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)]);
    const auto ceiling = generateTechnicalSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)]);
    expect(wall && floor && ceiling && gdp::render::validateImageData(*wall)
            && gdp::render::validateImageData(*floor)
            && gdp::render::validateImageData(*ceiling),
        "every Technical Area surface generates renderer-valid RGBA data");
    if (!wall || !floor || !ceiling) {
        return;
    }

    constexpr std::array<std::uint8_t, 3> pipeHighlight{122U, 125U, 115U};
    std::size_t pipeHighlightPixels = 0;
    for (std::size_t pixel = 0; pixel < wall->rgba.size(); pixel += 4U) {
        if (wall->rgba[pixel] == pipeHighlight[0]
            && wall->rgba[pixel + 1U] == pipeHighlight[1]
            && wall->rgba[pixel + 2U] == pipeHighlight[2]) {
            ++pipeHighlightPixels;
        }
    }
    expect(pipeHighlightPixels >= 50U,
        "Technical wall pipes preserve a continuous three-tone volume cue");

    constexpr std::array<std::uint8_t, 3> oilCore{23U, 26U, 23U};
    std::size_t oilPixels = 0;
    for (std::size_t pixel = 0; pixel < floor->rgba.size(); pixel += 4U) {
        if (floor->rgba[pixel] == oilCore[0]
            && floor->rgba[pixel + 1U] == oilCore[1]
            && floor->rgba[pixel + 2U] == oilCore[2]) {
            ++oilPixels;
        }
    }
    expect(oilPixels >= 15U && oilPixels <= 220U,
        "Technical floor keeps one bounded non-black oil core");

    gdp::core::Seed grateSeed = 0;
    TechnicalRoomMaterialSet grateMaterials;
    for (; grateSeed < 1'000; ++grateSeed) {
        grateMaterials = generateTechnicalRoomMaterials(grateSeed);
        if (grateMaterials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)]
                .mechanicalAmount > 0.17F) {
            break;
        }
    }
    const auto grateFloor = generateTechnicalSurfaceAlbedo(
        grateMaterials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)]);
    expect(grateSeed < 1'000 && grateFloor,
        "seed batch exposes a Technical floor grate variant");
    if (grateFloor) {
        constexpr std::array<std::uint8_t, 3> grateVoid{18U, 20U, 18U};
        std::size_t gratePixels = 0;
        for (std::size_t pixel = 0; pixel < grateFloor->rgba.size(); pixel += 4U) {
            if (grateFloor->rgba[pixel] == grateVoid[0]
                && grateFloor->rgba[pixel + 1U] == grateVoid[1]
                && grateFloor->rgba[pixel + 2U] == grateVoid[2]) {
                ++gratePixels;
            }
        }
        expect(gratePixels >= 80U && gratePixels <= 240U,
            "Technical grate remains a single framed mechanical detail");
    }

    const auto& ceilingRecipe
        = materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    const std::array<std::uint8_t, 3> fixtureColor{
        static_cast<std::uint8_t>(ceilingRecipe.accentColor.x * 255.0F + 0.5F),
        static_cast<std::uint8_t>(ceilingRecipe.accentColor.y * 255.0F + 0.5F),
        static_cast<std::uint8_t>(ceilingRecipe.accentColor.z * 255.0F + 0.5F),
    };
    std::size_t fixturePixels = 0;
    for (std::size_t pixel = 0; pixel < ceiling->rgba.size(); pixel += 4U) {
        if (ceiling->rgba[pixel] == fixtureColor[0]
            && ceiling->rgba[pixel + 1U] == fixtureColor[1]
            && ceiling->rgba[pixel + 2U] == fixtureColor[2]) {
            ++fixturePixels;
        }
    }
    expect(fixturePixels == 85U,
        "Technical ceiling guarantees exactly one protected navigation fixture");
}

void testTechnicalValidationFailures()
{
    using namespace gdp::procedural;

    const TechnicalRoomMaterialSet valid = generateTechnicalRoomMaterials(654);
    TechnicalSurfaceRecipe invalid = valid.surfaces[0];
    invalid.pipeAmount = 0.0F;
    expect(containsError(
               validateTechnicalSurfaceRecipe(invalid),
               TechnicalMaterialValidationError::InvalidSurfaceRule),
        "Technical wall without its pipe budget is rejected");
    invalid = valid.surfaces[1];
    invalid.oilAmount = 0.0F;
    expect(containsError(
               validateTechnicalSurfaceRecipe(invalid),
               TechnicalMaterialValidationError::InvalidSurfaceRule),
        "Technical floor without controlled oil variation is rejected");
    invalid = valid.surfaces[2];
    invalid.emissiveAmount = 0.0F;
    expect(containsError(
               validateTechnicalSurfaceRecipe(invalid),
               TechnicalMaterialValidationError::InvalidSurfaceRule),
        "Technical ceiling without protected-light budget is rejected");
    invalid.resolution = 63;
    expect(containsError(
               validateTechnicalSurfaceRecipe(invalid),
               TechnicalMaterialValidationError::InvalidResolution),
        "Technical texture outside the power-of-two contract is rejected");
    expect(!generateTechnicalSurfaceAlbedo(invalid),
        "invalid Technical recipe never produces partial image data");

    TechnicalRoomMaterialSet corrupted = valid;
    corrupted.materialHash ^= 1U;
    expect(containsError(
               validateTechnicalRoomMaterials(corrupted),
               TechnicalMaterialValidationError::InvalidMaterialHash),
        "Technical material set detects canonical hash corruption");
}

void testTechnicalSeedBatch()
{
    using namespace gdp::procedural;

    std::array<bool, static_cast<std::size_t>(TechnicalCondition::Count)> conditions{};
    std::unordered_set<std::uint64_t> hashes;
    bool allValid = true;
    for (gdp::core::Seed seed = 0; seed < 1'000; ++seed) {
        const TechnicalRoomMaterialSet materials = generateTechnicalRoomMaterials(seed);
        if (!validateTechnicalRoomMaterials(materials).isValid()) {
            std::cerr << "Technical validation failed at room seed " << seed << '\n';
            allValid = false;
            break;
        }
        for (const TechnicalSurfaceRecipe& recipe : materials.surfaces) {
            const auto image = generateTechnicalSurfaceAlbedo(recipe);
            if (!image || !gdp::render::validateImageData(*image)) {
                std::cerr << "Technical albedo generation failed at room seed " << seed << '\n';
                allValid = false;
                break;
            }
        }
        conditions[static_cast<std::size_t>(materials.condition)] = true;
        hashes.insert(materials.materialHash);
    }
    expect(allValid, "1,000 room seeds pass every Technical material and albedo gate");
    expect(hashes.size() == 1'000U,
        "1,000 Technical seeds produce unique canonical material hashes");
    expect(std::all_of(conditions.begin(), conditions.end(), [](const bool used) { return used; }),
        "Technical seed batch reaches every maintenance-load condition");
}

void testRepresentativeSecurityMaterialSet()
{
    using namespace gdp::procedural;

    constexpr gdp::core::Seed seed = 0x5345435552495459ULL;
    const SecurityRoomMaterialSet first = generateSecurityRoomMaterials(seed);
    const SecurityRoomMaterialSet repeated = generateSecurityRoomMaterials(seed);
    const SecurityRoomMaterialSet different = generateSecurityRoomMaterials(seed + 1U);
    expect(validateSecurityRoomMaterials(first).isValid(),
        "representative Security material set validates");
    expect(first == repeated,
        "same Security seed reproduces every recipe exactly");
    expect(first.materialHash != different.materialHash,
        "different Security seed changes its canonical material hash");
    expect(first.materialHash == 0xACBC340BCE0417CDULL,
        "golden Security material hash remains stable across builds");

    const auto& wall = first.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    const auto& floor = first.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const auto& ceiling = first.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    expect(wall.family == MaterialFamily::PaintedSteel && wall.monitorAmount > 0.0F
            && wall.statusAmount > 0.0F && wall.accessAmount == 0.0F,
        "Security wall reserves one organized monitor and grouped status budget");
    expect(floor.family == MaterialFamily::BareSteel && floor.accessAmount > 0.0F
            && floor.monitorAmount == 0.0F && floor.alertAmount == 0.0F,
        "Security floor reserves clean technical access details without alerts");
    expect(ceiling.family == MaterialFamily::Aluminum && ceiling.statusAmount > 0.0F
            && ceiling.emissiveAmount > 0.0F,
        "Security ceiling combines regular luminaires with one sensor budget");
    expect(wall.seed != floor.seed && floor.seed != ceiling.seed && wall.seed != ceiling.seed,
        "Security surfaces use independent room-material sub-seeds");
}

void testStructuredSecurityAlbedos()
{
    using namespace gdp::procedural;

    const SecurityRoomMaterialSet materials = generateSecurityRoomMaterials(778899);
    const auto wall = generateSecuritySurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)]);
    const auto floor = generateSecuritySurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)]);
    const auto ceiling = generateSecuritySurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)]);
    expect(wall && floor && ceiling && gdp::render::validateImageData(*wall)
            && gdp::render::validateImageData(*floor)
            && gdp::render::validateImageData(*ceiling),
        "every Security surface generates renderer-valid RGBA data");
    if (!wall || !floor || !ceiling) {
        return;
    }

    constexpr std::array<std::uint8_t, 3> statusGreen{59U, 156U, 71U};
    std::size_t greenPixels = 0;
    for (std::size_t pixel = 0; pixel < wall->rgba.size(); pixel += 4U) {
        if (wall->rgba[pixel] == statusGreen[0]
            && wall->rgba[pixel + 1U] == statusGreen[1]
            && wall->rgba[pixel + 2U] == statusGreen[2]) {
            ++greenPixels;
        }
    }
    expect(greenPixels == 12U || greenPixels == 16U,
        "Security wall keeps exactly four grouped indicators, with alert override allowed");

    gdp::core::Seed alertSeed = 0;
    SecurityRoomMaterialSet alertMaterials;
    for (; alertSeed < 1'000; ++alertSeed) {
        alertMaterials = generateSecurityRoomMaterials(alertSeed);
        if (alertMaterials.condition == SecurityCondition::Alert) {
            break;
        }
    }
    const auto alertWall = generateSecuritySurfaceAlbedo(
        alertMaterials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)]);
    expect(alertSeed < 1'000 && alertWall,
        "seed batch exposes an alert-state Security wall");
    if (alertWall) {
        constexpr std::array<std::uint8_t, 3> alertRed{168U, 51U, 51U};
        std::size_t alertPixels = 0;
        std::size_t alertOutsideConsole = 0;
        for (int y = 0; y < alertWall->height; ++y) {
            for (int x = 0; x < alertWall->width; ++x) {
                const std::size_t pixel = (static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(alertWall->width)
                    + static_cast<std::size_t>(x)) * 4U;
                const bool red = alertWall->rgba[pixel] == alertRed[0]
                    && alertWall->rgba[pixel + 1U] == alertRed[1]
                    && alertWall->rgba[pixel + 2U] == alertRed[2];
                if (red) {
                    ++alertPixels;
                    if (x < 35 || x >= 60 || y < 12 || y >= 45) {
                        ++alertOutsideConsole;
                    }
                }
            }
        }
        expect(alertPixels == 36U && alertOutsideConsole == 0U,
            "Security red alert remains exclusive to the monitor console");
    }

    const auto& floorRecipe = materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const std::array<std::uint8_t, 3> hatchColor{
        static_cast<std::uint8_t>(floorRecipe.accentColor.x * 255.0F + 0.5F),
        static_cast<std::uint8_t>(floorRecipe.accentColor.y * 255.0F + 0.5F),
        static_cast<std::uint8_t>(floorRecipe.accentColor.z * 255.0F + 0.5F),
    };
    std::size_t hatchPixels = 0;
    for (std::size_t pixel = 0; pixel < floor->rgba.size(); pixel += 4U) {
        if (floor->rgba[pixel] == hatchColor[0]
            && floor->rgba[pixel + 1U] == hatchColor[1]
            && floor->rgba[pixel + 2U] == hatchColor[2]) {
            ++hatchPixels;
        }
    }
    expect(hatchPixels == 64U,
        "Security floor draws one precise removable access hatch");

    const auto& ceilingRecipe
        = materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    const std::array<std::uint8_t, 3> lightColor{
        static_cast<std::uint8_t>(ceilingRecipe.accentColor.x * 255.0F + 0.5F),
        static_cast<std::uint8_t>(ceilingRecipe.accentColor.y * 255.0F + 0.5F),
        static_cast<std::uint8_t>(ceilingRecipe.accentColor.z * 255.0F + 0.5F),
    };
    std::size_t lightPixels = 0;
    for (std::size_t pixel = 0; pixel < ceiling->rgba.size(); pixel += 4U) {
        if (ceiling->rgba[pixel] == lightColor[0]
            && ceiling->rgba[pixel + 1U] == lightColor[1]
            && ceiling->rgba[pixel + 2U] == lightColor[2]) {
            ++lightPixels;
        }
    }
    const float lightCoverage = static_cast<float>(lightPixels)
        / static_cast<float>(ceiling->width * ceiling->height);
    expect(lightCoverage >= 0.20F && lightCoverage <= 0.35F,
        "Security ceiling keeps regular neutral lights within the 20-35% budget");
}

void testSecurityValidationFailures()
{
    using namespace gdp::procedural;

    const SecurityRoomMaterialSet valid = generateSecurityRoomMaterials(987);
    SecuritySurfaceRecipe invalid = valid.surfaces[0];
    invalid.monitorAmount = 0.0F;
    expect(containsError(
               validateSecuritySurfaceRecipe(invalid),
               SecurityMaterialValidationError::InvalidSurfaceRule),
        "Security wall without monitor budget is rejected");
    invalid = valid.surfaces[1];
    invalid.alertAmount = 0.2F;
    expect(containsError(
               validateSecuritySurfaceRecipe(invalid),
               SecurityMaterialValidationError::InvalidSurfaceRule),
        "Security floor rejects alert color outside the console");
    invalid = valid.surfaces[2];
    invalid.emissiveAmount = 0.0F;
    expect(containsError(
               validateSecuritySurfaceRecipe(invalid),
               SecurityMaterialValidationError::InvalidSurfaceRule),
        "Security ceiling without regular-light budget is rejected");
    invalid.resolution = 63;
    expect(containsError(
               validateSecuritySurfaceRecipe(invalid),
               SecurityMaterialValidationError::InvalidResolution),
        "Security texture outside the power-of-two contract is rejected");
    expect(!generateSecuritySurfaceAlbedo(invalid),
        "invalid Security recipe never produces partial image data");

    SecurityRoomMaterialSet corrupted = valid;
    corrupted.materialHash ^= 1U;
    expect(containsError(
               validateSecurityRoomMaterials(corrupted),
               SecurityMaterialValidationError::InvalidMaterialHash),
        "Security material set detects canonical hash corruption");
}

void testSecuritySeedBatch()
{
    using namespace gdp::procedural;

    std::array<bool, static_cast<std::size_t>(SecurityCondition::Count)> conditions{};
    std::unordered_set<std::uint64_t> hashes;
    bool allValid = true;
    for (gdp::core::Seed seed = 0; seed < 1'000; ++seed) {
        const SecurityRoomMaterialSet materials = generateSecurityRoomMaterials(seed);
        if (!validateSecurityRoomMaterials(materials).isValid()) {
            std::cerr << "Security validation failed at room seed " << seed << '\n';
            allValid = false;
            break;
        }
        for (const SecuritySurfaceRecipe& recipe : materials.surfaces) {
            const auto image = generateSecuritySurfaceAlbedo(recipe);
            if (!image || !gdp::render::validateImageData(*image)) {
                std::cerr << "Security albedo generation failed at room seed " << seed << '\n';
                allValid = false;
                break;
            }
        }
        conditions[static_cast<std::size_t>(materials.condition)] = true;
        hashes.insert(materials.materialHash);
    }
    expect(allValid, "1,000 room seeds pass every Security material and albedo gate");
    expect(hashes.size() == 1'000U,
        "1,000 Security seeds produce unique canonical material hashes");
    expect(std::all_of(conditions.begin(), conditions.end(), [](const bool used) { return used; }),
        "Security seed batch reaches every control condition");
}

void testRepresentativeCentralHubMaterialSet()
{
    using namespace gdp::procedural;

    constexpr gdp::core::Seed seed = 0x43454E5452414CULL;
    const CentralHubRoomMaterialSet first = generateCentralHubRoomMaterials(seed);
    const CentralHubRoomMaterialSet repeated = generateCentralHubRoomMaterials(seed);
    const CentralHubRoomMaterialSet different = generateCentralHubRoomMaterials(seed + 1U);
    expect(validateCentralHubRoomMaterials(first).isValid(),
        "representative Central Hub material set validates");
    expect(first == repeated,
        "same Central Hub seed reproduces every recipe exactly");
    expect(first.materialHash != different.materialHash,
        "different Central Hub seed changes its canonical material hash");
    expect(first.materialHash == 0x4443DF14003D9B43ULL,
        "golden Central Hub material hash remains stable across builds");

    const auto& wall = first.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    const auto& floor = first.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const auto& ceiling = first.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    expect(wall.structureAmount > 0.0F && wall.navigationAmount > 0.0F
            && wall.focalAmount > 0.0F && wall.emissiveAmount == 0.0F,
        "Central Hub wall reserves one calm architectural landmark and sign budget");
    expect(floor.structureAmount == 0.0F && floor.navigationAmount > 0.0F
            && floor.focalAmount == 0.0F && floor.emissiveAmount == 0.0F,
        "Central Hub floor owns continuous navigation without competing focal details");
    expect(ceiling.structureAmount > 0.0F && ceiling.navigationAmount == 0.0F
            && ceiling.focalAmount == 0.0F && ceiling.emissiveAmount > 0.0F,
        "Central Hub ceiling owns route-aligned regular lighting");
    expect(wall.routeAxis == first.routeAxis && floor.routeAxis == first.routeAxis
            && ceiling.routeAxis == first.routeAxis,
        "Central Hub surfaces share one room-level orientation axis");
    expect(wall.seed != floor.seed && floor.seed != ceiling.seed && wall.seed != ceiling.seed,
        "Central Hub surfaces use independent room-material sub-seeds");
}

void testStructuredCentralHubAlbedos()
{
    using namespace gdp::procedural;

    const CentralHubRoomMaterialSet materials = generateCentralHubRoomMaterials(424242);
    const auto wall = generateCentralHubSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)]);
    const auto floor = generateCentralHubSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)]);
    const auto ceiling = generateCentralHubSurfaceAlbedo(
        materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)]);
    expect(wall && floor && ceiling && gdp::render::validateImageData(*wall)
            && gdp::render::validateImageData(*floor)
            && gdp::render::validateImageData(*ceiling),
        "every Central Hub surface generates renderer-valid RGBA data");
    if (!wall || !floor || !ceiling) {
        return;
    }

    const auto exactColorCount = [](const gdp::render::ImageData& image,
                                     const gdp::core::Vec3 color) {
        const std::array<std::uint8_t, 3> expected{
            static_cast<std::uint8_t>(color.x * 255.0F + 0.5F),
            static_cast<std::uint8_t>(color.y * 255.0F + 0.5F),
            static_cast<std::uint8_t>(color.z * 255.0F + 0.5F),
        };
        std::size_t count = 0;
        for (std::size_t pixel = 0; pixel < image.rgba.size(); pixel += 4U) {
            if (image.rgba[pixel] == expected[0] && image.rgba[pixel + 1U] == expected[1]
                && image.rgba[pixel + 2U] == expected[2]) {
                ++count;
            }
        }
        return count;
    };

    const auto& wallRecipe
        = materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    const auto& floorRecipe
        = materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    const auto& ceilingRecipe
        = materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    expect(exactColorCount(*wall, wallRecipe.accentColor) == 36U,
        "Central Hub wall contains one compact cyan wayfinding sign");

    const std::size_t routePixels = exactColorCount(*floor, floorRecipe.accentColor);
    const std::size_t expectedRoutePixels
        = materials.routeAxis == CentralHubRouteAxis::Cross ? 252U : 128U;
    expect(routePixels == expectedRoutePixels,
        "Central Hub floor contains only one main route or one controlled crossing");
    bool routeIsContinuous = true;
    const std::array<std::uint8_t, 3> routeColor{
        static_cast<std::uint8_t>(floorRecipe.accentColor.x * 255.0F + 0.5F),
        static_cast<std::uint8_t>(floorRecipe.accentColor.y * 255.0F + 0.5F),
        static_cast<std::uint8_t>(floorRecipe.accentColor.z * 255.0F + 0.5F),
    };
    if (materials.routeAxis == CentralHubRouteAxis::Horizontal
        || materials.routeAxis == CentralHubRouteAxis::Cross) {
        for (int x = 0; x < floor->width; ++x) {
            const std::size_t pixel = (31U * static_cast<std::size_t>(floor->width)
                + static_cast<std::size_t>(x)) * 4U;
            routeIsContinuous = routeIsContinuous && floor->rgba[pixel] == routeColor[0]
                && floor->rgba[pixel + 1U] == routeColor[1]
                && floor->rgba[pixel + 2U] == routeColor[2];
        }
    }
    if (materials.routeAxis == CentralHubRouteAxis::Vertical
        || materials.routeAxis == CentralHubRouteAxis::Cross) {
        for (int y = 0; y < floor->height; ++y) {
            const std::size_t pixel = (static_cast<std::size_t>(y)
                * static_cast<std::size_t>(floor->width) + 31U) * 4U;
            routeIsContinuous = routeIsContinuous && floor->rgba[pixel] == routeColor[0]
                && floor->rgba[pixel + 1U] == routeColor[1]
                && floor->rgba[pixel + 2U] == routeColor[2];
        }
    }
    expect(routeIsContinuous,
        "Central Hub cyan navigation reaches both texture edges without breaks");

    const std::size_t lightPixels = exactColorCount(*ceiling, ceilingRecipe.accentColor);
    const std::size_t expectedLightPixels
        = materials.routeAxis == CentralHubRouteAxis::Cross ? 135U : 144U;
    expect(lightPixels == expectedLightPixels,
        "Central Hub ceiling uses sparse route-aligned linear fixtures");

    constexpr gdp::core::Vec3 forbiddenYellow{0.765F, 0.545F, 0.0F};
    expect(exactColorCount(*wall, forbiddenYellow) == 0U
            && exactColorCount(*floor, forbiddenYellow) == 0U
            && exactColorCount(*ceiling, forbiddenYellow) == 0U,
        "Central Hub never uses decorative yellow-black signaling");
}

void testCentralHubValidationFailures()
{
    using namespace gdp::procedural;

    const CentralHubRoomMaterialSet valid = generateCentralHubRoomMaterials(7654);
    CentralHubSurfaceRecipe invalid = valid.surfaces[0];
    invalid.navigationAmount = 0.0F;
    expect(containsError(
               validateCentralHubSurfaceRecipe(invalid),
               CentralHubMaterialValidationError::InvalidSurfaceRule),
        "Central Hub wall without local wayfinding is rejected");
    invalid = valid.surfaces[1];
    invalid.structureAmount = 0.2F;
    expect(containsError(
               validateCentralHubSurfaceRecipe(invalid),
               CentralHubMaterialValidationError::InvalidSurfaceRule),
        "Central Hub floor rejects competing architectural detail");
    invalid = valid.surfaces[2];
    invalid.emissiveAmount = 0.0F;
    expect(containsError(
               validateCentralHubSurfaceRecipe(invalid),
               CentralHubMaterialValidationError::InvalidSurfaceRule),
        "Central Hub ceiling without directional lighting is rejected");
    invalid = valid.surfaces[0];
    invalid.accentColor = {0.765F, 0.545F, 0.0F};
    expect(containsError(
               validateCentralHubSurfaceRecipe(invalid),
               CentralHubMaterialValidationError::InvalidSurfaceRule),
        "Central Hub rejects decorative yellow as its navigation accent");
    invalid.resolution = 63;
    expect(containsError(
               validateCentralHubSurfaceRecipe(invalid),
               CentralHubMaterialValidationError::InvalidResolution),
        "Central Hub texture outside the power-of-two contract is rejected");
    expect(!generateCentralHubSurfaceAlbedo(invalid),
        "invalid Central Hub recipe never produces partial image data");

    CentralHubRoomMaterialSet corrupted = valid;
    corrupted.surfaces[0].routeAxis = corrupted.routeAxis == CentralHubRouteAxis::Horizontal
        ? CentralHubRouteAxis::Vertical
        : CentralHubRouteAxis::Horizontal;
    expect(containsError(
               validateCentralHubRoomMaterials(corrupted),
               CentralHubMaterialValidationError::InvalidSurfaceOrder),
        "Central Hub material set rejects a surface with a divergent route axis");
    corrupted = valid;
    corrupted.materialHash ^= 1U;
    expect(containsError(
               validateCentralHubRoomMaterials(corrupted),
               CentralHubMaterialValidationError::InvalidMaterialHash),
        "Central Hub material set detects canonical hash corruption");
}

void testCentralHubSeedBatch()
{
    using namespace gdp::procedural;

    std::array<bool, static_cast<std::size_t>(CentralHubCondition::Count)> conditions{};
    std::array<bool, static_cast<std::size_t>(CentralHubRouteAxis::Count)> routeAxes{};
    std::unordered_set<std::uint64_t> hashes;
    bool allValid = true;
    for (gdp::core::Seed seed = 0; seed < 1'000; ++seed) {
        const CentralHubRoomMaterialSet materials = generateCentralHubRoomMaterials(seed);
        if (!validateCentralHubRoomMaterials(materials).isValid()) {
            std::cerr << "Central Hub validation failed at room seed " << seed << '\n';
            allValid = false;
            break;
        }
        for (const CentralHubSurfaceRecipe& recipe : materials.surfaces) {
            const auto image = generateCentralHubSurfaceAlbedo(recipe);
            if (!image || !gdp::render::validateImageData(*image)) {
                std::cerr << "Central Hub albedo generation failed at room seed " << seed << '\n';
                allValid = false;
                break;
            }
        }
        conditions[static_cast<std::size_t>(materials.condition)] = true;
        routeAxes[static_cast<std::size_t>(materials.routeAxis)] = true;
        hashes.insert(materials.materialHash);
    }
    expect(allValid, "1,000 room seeds pass every Central Hub material and albedo gate");
    expect(hashes.size() == 1'000U,
        "1,000 Central Hub seeds produce unique canonical material hashes");
    expect(std::all_of(conditions.begin(), conditions.end(), [](const bool used) { return used; }),
        "Central Hub seed batch reaches every circulation condition");
    expect(std::all_of(routeAxes.begin(), routeAxes.end(), [](const bool used) { return used; }),
        "Central Hub seed batch reaches every route-axis composition");
}

void testRoomProfileHashIsolation()
{
    using namespace gdp::procedural;

    std::unordered_set<std::uint64_t> hashes;
    bool substreamsAreIsolated = true;
    for (gdp::core::Seed seed = 0; seed < 1'000; ++seed) {
        const IndustrialRoomMaterialSet industrial = generateIndustrialRoomMaterials(seed);
        const StorageRoomMaterialSet storage = generateStorageRoomMaterials(seed);
        const CombatRoomMaterialSet combat = generateCombatRoomMaterials(seed);
        const CafeteriaRoomMaterialSet cafeteria = generateCafeteriaRoomMaterials(seed);
        const LaboratoryRoomMaterialSet laboratory = generateLaboratoryRoomMaterials(seed);
        const TechnicalRoomMaterialSet technical = generateTechnicalRoomMaterials(seed);
        const SecurityRoomMaterialSet security = generateSecurityRoomMaterials(seed);
        const CentralHubRoomMaterialSet centralHub = generateCentralHubRoomMaterials(seed);
        hashes.insert(industrial.materialHash);
        hashes.insert(storage.materialHash);
        hashes.insert(combat.materialHash);
        hashes.insert(cafeteria.materialHash);
        hashes.insert(laboratory.materialHash);
        hashes.insert(technical.materialHash);
        hashes.insert(security.materialHash);
        hashes.insert(centralHub.materialHash);
        for (std::size_t surface = 0; surface < industrial.surfaces.size(); ++surface) {
            substreamsAreIsolated = substreamsAreIsolated
                && industrial.surfaces[surface].seed != storage.surfaces[surface].seed
                && industrial.surfaces[surface].seed != combat.surfaces[surface].seed
                && industrial.surfaces[surface].seed != cafeteria.surfaces[surface].seed
                && industrial.surfaces[surface].seed != laboratory.surfaces[surface].seed
                && industrial.surfaces[surface].seed != technical.surfaces[surface].seed
                && industrial.surfaces[surface].seed != security.surfaces[surface].seed
                && industrial.surfaces[surface].seed != centralHub.surfaces[surface].seed
                && storage.surfaces[surface].seed != combat.surfaces[surface].seed
                && storage.surfaces[surface].seed != cafeteria.surfaces[surface].seed
                && storage.surfaces[surface].seed != laboratory.surfaces[surface].seed
                && storage.surfaces[surface].seed != technical.surfaces[surface].seed
                && storage.surfaces[surface].seed != security.surfaces[surface].seed
                && storage.surfaces[surface].seed != centralHub.surfaces[surface].seed
                && combat.surfaces[surface].seed != cafeteria.surfaces[surface].seed
                && combat.surfaces[surface].seed != laboratory.surfaces[surface].seed
                && combat.surfaces[surface].seed != technical.surfaces[surface].seed
                && combat.surfaces[surface].seed != security.surfaces[surface].seed
                && combat.surfaces[surface].seed != centralHub.surfaces[surface].seed
                && cafeteria.surfaces[surface].seed != laboratory.surfaces[surface].seed
                && cafeteria.surfaces[surface].seed != technical.surfaces[surface].seed
                && cafeteria.surfaces[surface].seed != security.surfaces[surface].seed
                && cafeteria.surfaces[surface].seed != centralHub.surfaces[surface].seed
                && laboratory.surfaces[surface].seed != technical.surfaces[surface].seed
                && laboratory.surfaces[surface].seed != security.surfaces[surface].seed
                && laboratory.surfaces[surface].seed != centralHub.surfaces[surface].seed
                && technical.surfaces[surface].seed != security.surfaces[surface].seed
                && technical.surfaces[surface].seed != centralHub.surfaces[surface].seed
                && security.surfaces[surface].seed != centralHub.surfaces[surface].seed;
        }
    }
    expect(hashes.size() == 8'000U,
        "eight room profiles produce 8,000 cross-profile unique hashes");
    expect(substreamsAreIsolated,
        "equal room seeds use isolated material substreams in every profile");
}

} // namespace

int main()
{
    testRepresentativeMaterialSet();
    testStructuredSurfaceAlbedos();
    testIndustrialHdMaterialMaps();
    testApprovedIndustrialWallDirection();
    testValidationFailures();
    testRoomMaterialSeedBatch();
    testRepresentativeStorageMaterialSet();
    testStructuredStorageAlbedos();
    testStorageValidationFailures();
    testStorageSeedBatch();
    testRepresentativeCombatMaterialSet();
    testStructuredCombatAlbedos();
    testCombatValidationFailures();
    testCombatSeedBatch();
    testRepresentativeCafeteriaMaterialSet();
    testStructuredCafeteriaAlbedos();
    testCafeteriaValidationFailures();
    testCafeteriaSeedBatch();
    testRepresentativeLaboratoryMaterialSet();
    testStructuredLaboratoryAlbedos();
    testLaboratoryValidationFailures();
    testLaboratorySeedBatch();
    testRepresentativeTechnicalMaterialSet();
    testStructuredTechnicalAlbedos();
    testTechnicalValidationFailures();
    testTechnicalSeedBatch();
    testRepresentativeSecurityMaterialSet();
    testStructuredSecurityAlbedos();
    testSecurityValidationFailures();
    testSecuritySeedBatch();
    testRepresentativeCentralHubMaterialSet();
    testStructuredCentralHubAlbedos();
    testCentralHubValidationFailures();
    testCentralHubSeedBatch();
    testRoomProfileHashIsolation();

    if (failures != 0) {
        std::cerr << failures << " room material test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP room material tests passed\n";
    return 0;
}
