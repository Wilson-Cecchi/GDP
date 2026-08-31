#include "game/visual/EnemyGenerator.hpp"
#include "procedural/MeshGenerator.hpp"
#include "procedural/TextureGenerator.hpp"
#include "render/Mesh.hpp"
#include "render/Texture.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
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
    const gdp::game::visual::WorkerVisualValidationResult& result,
    const gdp::game::visual::WorkerVisualValidationError error)
{
    for (const auto candidate : result.errors) {
        if (candidate == error) {
            return true;
        }
    }
    return false;
}

bool containsError(
    const gdp::game::visual::GuardVisualValidationResult& result,
    const gdp::game::visual::GuardVisualValidationError error)
{
    for (const auto candidate : result.errors) {
        if (candidate == error) {
            return true;
        }
    }
    return false;
}

bool containsError(
    const gdp::game::visual::OfficerVisualValidationResult& result,
    const gdp::game::visual::OfficerVisualValidationError error)
{
    for (const auto candidate : result.errors) {
        if (candidate == error) {
            return true;
        }
    }
    return false;
}

bool containsError(
    const gdp::game::visual::EngineerVisualValidationResult& result,
    const gdp::game::visual::EngineerVisualValidationError error)
{
    for (const auto candidate : result.errors) {
        if (candidate == error) {
            return true;
        }
    }
    return false;
}

bool containsError(
    const gdp::game::visual::IndustrialRobotVisualValidationResult& result,
    const gdp::game::visual::IndustrialRobotVisualValidationError error)
{
    for (const auto candidate : result.errors) {
        if (candidate == error) {
            return true;
        }
    }
    return false;
}

void testPartLibrary()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    const auto library = workerPartLibrary();
    std::unordered_set<WorkerPartId> ids;
    std::array<bool, static_cast<std::size_t>(WorkerPartSlot::Count)> slots{};
    bool definitionsValid = !library.empty();
    for (const WorkerPartDefinition& part : library) {
        definitionsValid = definitionsValid && part.id != 0 && ids.insert(part.id).second
            && part.slot < WorkerPartSlot::Count && part.weight > 0
            && part.compatibility != 0 && part.semantic < SemanticRegion::Count
            && part.material < MaterialSlot::Count;
        if (part.slot < WorkerPartSlot::Count) {
            slots[static_cast<std::size_t>(part.slot)] = true;
        }
    }
    expect(definitionsValid, "Worker part library has unique valid metadata");
    bool everySlotCovered = true;
    for (const bool covered : slots) {
        everySlotCovered = everySlotCovered && covered;
    }
    expect(everySlotCovered, "Worker part library covers every required assembly slot");
}

void testDeterministicWorker()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    constexpr wkc::core::Seed seed = 0x123456789ABCDEF0ULL;
    const WorkerVisual first = generateWorkerVisual(seed);
    const WorkerVisual repeated = generateWorkerVisual(seed);
    const WorkerVisual different = generateWorkerVisual(seed + 1U);
    expect(validateWorkerVisual(first).isValid(), "representative Worker visual validates");
    expect(first.genome.visualHash == repeated.genome.visualHash
            && first.meshRecipe == repeated.meshRecipe
            && first.textureRecipe == repeated.textureRecipe,
        "same visual seed reproduces genome, mesh and texture recipes exactly");
    expect(first.genome.visualHash != different.genome.visualHash,
        "different visual seed changes the Worker genome hash");
    expect(first.fallbackCount == 0 && repeated.fallbackCount == 0,
        "built-in library generates representative Worker without fallback");

    const auto mesh = generateMesh(first.meshRecipe);
    const auto albedo = generateAlbedo(first.textureRecipe);
    expect(mesh && wkc::render::validateMeshData(mesh->mesh),
        "complete Worker genome produces renderer-valid mesh data");
    expect(albedo && wkc::render::validateImageData(*albedo),
        "complete Worker genome produces renderer-valid albedo data");
    expect(first.genome.visualHash == 0x47FC8A2095B5F922ULL,
        "golden Worker visual hash remains stable across builds");

    WorkerVisual invalid = first;
    invalid.genome.head = 999'999;
    expect(containsError(validateWorkerVisual(invalid), WorkerVisualValidationError::MissingPart),
        "assembly validation rejects missing part ID");
    invalid = first;
    invalid.genome.head = first.genome.torso;
    expect(containsError(
               validateWorkerVisual(invalid), WorkerVisualValidationError::WrongPartSlot),
        "assembly validation rejects part in the wrong slot");
    invalid = first;
    invalid.genome.visualHash ^= 1U;
    expect(containsError(
               validateWorkerVisual(invalid), WorkerVisualValidationError::InvalidVisualHash),
        "assembly validation detects genome hash corruption");
}

void testWorkerSeedBatch()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    std::array<bool, static_cast<std::size_t>(WorkerBodyProfileId::Count)> profiles{};
    std::array<bool, static_cast<std::size_t>(WearCondition::Damaged) + 1U> wearConditions{};
    std::array<bool, static_cast<std::size_t>(MaterialFamily::Count)> materials{};
    std::unordered_set<WorkerPartId> selectedParts;
    std::unordered_set<std::uint64_t> visualHashes;
    bool allValid = true;
    bool foundAsymmetry = false;
    std::uint32_t fallbackCount = 0;

    for (wkc::core::Seed seed = 0; seed < 1'000; ++seed) {
        const WorkerVisual visual = generateWorkerVisual(seed);
        const auto validation = validateWorkerVisual(visual);
        const auto mesh = generateMesh(visual.meshRecipe);
        const auto albedo = generateAlbedo(visual.textureRecipe);
        if (!validation.isValid() || !mesh || !wkc::render::validateMeshData(mesh->mesh)
            || !albedo || !wkc::render::validateImageData(*albedo)) {
            allValid = false;
            std::cerr << "Worker generation failed at visual seed " << seed << '\n';
            break;
        }
        profiles[static_cast<std::size_t>(visual.genome.bodyProfile)] = true;
        wearConditions[static_cast<std::size_t>(visual.wear.condition)] = true;
        materials[static_cast<std::size_t>(visual.materials.family)] = true;
        selectedParts.insert(visual.genome.torso);
        selectedParts.insert(visual.genome.head);
        selectedParts.insert(visual.genome.leftArm);
        selectedParts.insert(visual.genome.rightArm);
        selectedParts.insert(visual.genome.leftLeg);
        selectedParts.insert(visual.genome.rightLeg);
        selectedParts.insert(visual.genome.back);
        selectedParts.insert(visual.genome.tool);
        visualHashes.insert(visual.genome.visualHash);
        foundAsymmetry = foundAsymmetry
            || visual.genome.leftArm != visual.genome.rightArm
            || visual.genome.leftLeg != visual.genome.rightLeg;
        fallbackCount += visual.fallbackCount;
    }
    expect(allValid, "1,000 Worker visual seeds pass assembly and asset gates");
    expect(fallbackCount == 0, "built-in Worker library needs no batch fallback");
    expect(visualHashes.size() == 1'000U,
        "1,000 visual seeds produce unique canonical Worker hashes");
    expect(selectedParts.size() == workerPartLibrary().size(),
        "seed batch reaches every compatible built-in Worker part");
    expect(foundAsymmetry, "seed batch exercises allowed left/right part asymmetry");

    bool allProfiles = true;
    for (const bool selected : profiles) {
        allProfiles = allProfiles && selected;
    }
    bool allWear = true;
    for (const bool selected : wearConditions) {
        allWear = allWear && selected;
    }
    expect(allProfiles, "seed batch reaches every Worker BodyProfile");
    expect(allWear, "seed batch reaches every Worker wear condition");
    expect(materials[static_cast<std::size_t>(MaterialFamily::PaintedSteel)]
            && materials[static_cast<std::size_t>(MaterialFamily::BareSteel)]
            && materials[static_cast<std::size_t>(MaterialFamily::Aluminum)],
        "seed batch reaches every Worker material family");
}

void testGuardPartLibrary()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    const auto library = guardPartLibrary();
    std::unordered_set<GuardPartId> ids;
    std::array<bool, static_cast<std::size_t>(WorkerPartSlot::Count)> slots{};
    bool definitionsValid = library.size() == 18U;
    for (const GuardPartDefinition& part : library) {
        definitionsValid = definitionsValid && part.id >= 1'100U
            && ids.insert(part.id).second && part.slot < WorkerPartSlot::Count
            && part.weight > 0U && part.compatibility != 0U
            && part.semantic < SemanticRegion::Count && part.material < MaterialSlot::Count;
        if (part.slot < WorkerPartSlot::Count) {
            slots[static_cast<std::size_t>(part.slot)] = true;
        }
        if (part.slot == WorkerPartSlot::Tool) {
            definitionsValid = definitionsValid && part.semantic == SemanticRegion::Weapon
                && part.dimensionScale.x > part.dimensionScale.y * 3.0F;
        }
    }
    expect(definitionsValid,
        "Guard part library has 18 unique tactical definitions and long weapons");
    expect(std::all_of(slots.begin(), slots.end(), [](const bool covered) { return covered; }),
        "Guard part library covers every required assembly slot");
}

void testDeterministicGuard()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    constexpr wkc::core::Seed seed = 0x47554152445F5631ULL;
    const GuardVisual first = generateGuardVisual(seed);
    const GuardVisual repeated = generateGuardVisual(seed);
    const GuardVisual different = generateGuardVisual(seed + 1U);
    const WorkerVisual worker = generateWorkerVisual(seed);
    expect(validateGuardVisual(first).isValid(), "representative Guard visual validates");
    expect(first.genome.visualHash == repeated.genome.visualHash
            && first.meshRecipe == repeated.meshRecipe
            && first.textureRecipe == repeated.textureRecipe,
        "same Guard visual seed reproduces genome and asset recipes exactly");
    expect(first.genome.visualHash != different.genome.visualHash,
        "different visual seed changes the Guard genome hash");
    expect(first.genome.visualHash != worker.genome.visualHash
            && first.meshRecipe.seed != worker.meshRecipe.seed
            && first.textureRecipe.seed != worker.textureRecipe.seed,
        "Guard and Worker use isolated role-specific visual substreams");
    expect(first.fallbackCount == 0U,
        "built-in library generates representative Guard without fallback");

    const auto mesh = generateMesh(first.meshRecipe);
    const auto albedo = generateAlbedo(first.textureRecipe);
    expect(mesh && wkc::render::validateMeshData(mesh->mesh),
        "complete Guard genome produces renderer-valid mesh data");
    expect(albedo && wkc::render::validateImageData(*albedo),
        "complete Guard genome produces renderer-valid albedo data");
    expect(first.genome.visualHash == 0x77FA12AE04FC63C8ULL,
        "golden Guard visual hash remains stable across builds");

    GuardVisual invalid = first;
    invalid.genome.head = 999'999U;
    expect(containsError(validateGuardVisual(invalid), GuardVisualValidationError::MissingPart),
        "Guard assembly rejects missing part ID");
    invalid = first;
    invalid.genome.weapon = first.genome.torso;
    expect(containsError(
               validateGuardVisual(invalid), GuardVisualValidationError::WrongPartSlot),
        "Guard assembly rejects a torso in the weapon slot");
    expect(containsError(
               validateGuardVisual(invalid), GuardVisualValidationError::InvalidWeapon),
        "Guard assembly requires a recognizable long weapon");
    invalid = first;
    invalid.genome.visualHash ^= 1U;
    expect(containsError(
               validateGuardVisual(invalid), GuardVisualValidationError::InvalidVisualHash),
        "Guard assembly detects genome hash corruption");
}

void testGuardSeedBatch()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    std::array<bool, static_cast<std::size_t>(GuardBodyProfileId::Count)> profiles{};
    std::array<bool, static_cast<std::size_t>(WearCondition::Damaged) + 1U> wearConditions{};
    std::array<bool, static_cast<std::size_t>(MaterialFamily::Count)> materials{};
    std::unordered_set<GuardPartId> selectedParts;
    std::unordered_set<std::uint64_t> guardHashes;
    std::unordered_set<std::uint64_t> crossRoleHashes;
    bool allValid = true;
    bool foundAsymmetry = false;
    std::uint32_t fallbackCount = 0;

    for (wkc::core::Seed seed = 0; seed < 1'000; ++seed) {
        const GuardVisual visual = generateGuardVisual(seed);
        const WorkerVisual worker = generateWorkerVisual(seed);
        const auto validation = validateGuardVisual(visual);
        const auto mesh = generateMesh(visual.meshRecipe);
        const auto albedo = generateAlbedo(visual.textureRecipe);
        if (!validation.isValid() || !mesh || !wkc::render::validateMeshData(mesh->mesh)
            || !albedo || !wkc::render::validateImageData(*albedo)) {
            allValid = false;
            std::cerr << "Guard generation failed at visual seed " << seed << '\n';
            break;
        }
        profiles[static_cast<std::size_t>(visual.genome.bodyProfile)] = true;
        wearConditions[static_cast<std::size_t>(visual.wear.condition)] = true;
        materials[static_cast<std::size_t>(visual.materials.family)] = true;
        selectedParts.insert(visual.genome.torso);
        selectedParts.insert(visual.genome.head);
        selectedParts.insert(visual.genome.leftArm);
        selectedParts.insert(visual.genome.rightArm);
        selectedParts.insert(visual.genome.leftLeg);
        selectedParts.insert(visual.genome.rightLeg);
        selectedParts.insert(visual.genome.back);
        selectedParts.insert(visual.genome.weapon);
        guardHashes.insert(visual.genome.visualHash);
        crossRoleHashes.insert(visual.genome.visualHash);
        crossRoleHashes.insert(worker.genome.visualHash);
        foundAsymmetry = foundAsymmetry
            || visual.genome.leftArm != visual.genome.rightArm
            || visual.genome.leftLeg != visual.genome.rightLeg;
        fallbackCount += visual.fallbackCount;
    }
    expect(allValid, "1,000 Guard visual seeds pass assembly and asset gates");
    expect(fallbackCount == 0U, "built-in Guard library needs no batch fallback");
    expect(guardHashes.size() == 1'000U,
        "1,000 visual seeds produce unique canonical Guard hashes");
    expect(crossRoleHashes.size() == 2'000U,
        "Worker and Guard hashes remain isolated for equal visual seeds");
    expect(selectedParts.size() == guardPartLibrary().size(),
        "Guard seed batch reaches every compatible built-in part");
    expect(foundAsymmetry, "Guard seed batch exercises allowed limb asymmetry");
    expect(std::all_of(profiles.begin(), profiles.end(), [](const bool used) { return used; }),
        "Guard seed batch reaches Patrol, Tactical and Armored profiles");
    expect(std::all_of(
               wearConditions.begin(), wearConditions.end(), [](const bool used) { return used; }),
        "Guard seed batch reaches every wear condition");
    expect(materials[static_cast<std::size_t>(MaterialFamily::Polymer)]
            && materials[static_cast<std::size_t>(MaterialFamily::PaintedSteel)]
            && materials[static_cast<std::size_t>(MaterialFamily::Polycarbonate)],
        "Guard seed batch reaches every tactical material family");
}

void testOfficerPartLibrary()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    const auto library = officerPartLibrary();
    std::unordered_set<OfficerPartId> ids;
    std::array<bool, static_cast<std::size_t>(WorkerPartSlot::Count)> slots{};
    bool definitionsValid = library.size() == 18U;
    for (const OfficerPartDefinition& part : library) {
        definitionsValid = definitionsValid && part.id >= 2'100U
            && ids.insert(part.id).second && part.slot < WorkerPartSlot::Count
            && part.weight > 0U && part.compatibility != 0U
            && part.semantic < SemanticRegion::Count && part.material < MaterialSlot::Count;
        if (part.slot < WorkerPartSlot::Count) {
            slots[static_cast<std::size_t>(part.slot)] = true;
        }
        if (part.slot == WorkerPartSlot::Back) {
            definitionsValid = definitionsValid
                && part.semantic == SemanticRegion::MechanicalCore
                && part.dimensionScale.y > part.dimensionScale.x;
        } else if (part.slot == WorkerPartSlot::Tool) {
            definitionsValid = definitionsValid && part.semantic == SemanticRegion::Weapon
                && part.dimensionScale.x > part.dimensionScale.y * 2.0F
                && part.dimensionScale.x < part.dimensionScale.y * 4.0F;
        }
    }
    expect(definitionsValid,
        "Officer library has 18 organized definitions, command modules and sidearms");
    expect(std::all_of(slots.begin(), slots.end(), [](const bool covered) { return covered; }),
        "Officer part library covers every required assembly slot");
}

void testDeterministicOfficer()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    constexpr wkc::core::Seed seed = 0x4F46464943455231ULL;
    const OfficerVisual first = generateOfficerVisual(seed);
    const OfficerVisual repeated = generateOfficerVisual(seed);
    const OfficerVisual different = generateOfficerVisual(seed + 1U);
    const WorkerVisual worker = generateWorkerVisual(seed);
    const GuardVisual guard = generateGuardVisual(seed);
    expect(validateOfficerVisual(first).isValid(), "representative Officer visual validates");
    expect(first.genome.visualHash == repeated.genome.visualHash
            && first.meshRecipe == repeated.meshRecipe
            && first.textureRecipe == repeated.textureRecipe,
        "same Officer seed reproduces genome and asset recipes exactly");
    expect(first.genome.visualHash != different.genome.visualHash,
        "different visual seed changes the Officer genome hash");
    expect(first.genome.visualHash != worker.genome.visualHash
            && first.genome.visualHash != guard.genome.visualHash
            && first.meshRecipe.seed != worker.meshRecipe.seed
            && first.meshRecipe.seed != guard.meshRecipe.seed
            && first.textureRecipe.seed != worker.textureRecipe.seed
            && first.textureRecipe.seed != guard.textureRecipe.seed,
        "Officer uses isolated role-specific visual substreams");
    expect(first.fallbackCount == 0U,
        "built-in library generates representative Officer without fallback");

    const auto mesh = generateMesh(first.meshRecipe);
    const auto albedo = generateAlbedo(first.textureRecipe);
    expect(mesh && wkc::render::validateMeshData(mesh->mesh),
        "complete Officer genome produces renderer-valid mesh data");
    expect(albedo && wkc::render::validateImageData(*albedo),
        "complete Officer genome produces renderer-valid albedo data");
    constexpr std::uint64_t goldenHash = 0x6C70CF22F1AE4F61ULL;
    expect(first.genome.visualHash == goldenHash,
        "golden Officer visual hash remains stable across builds");

    OfficerVisual invalid = first;
    invalid.genome.head = 999'999U;
    expect(containsError(
               validateOfficerVisual(invalid), OfficerVisualValidationError::MissingPart),
        "Officer assembly rejects missing part ID");
    invalid = first;
    invalid.genome.commandModule = first.genome.torso;
    expect(containsError(validateOfficerVisual(invalid),
               OfficerVisualValidationError::WrongPartSlot),
        "Officer assembly rejects a torso in the command-module slot");
    expect(containsError(validateOfficerVisual(invalid),
               OfficerVisualValidationError::InvalidCommandModule),
        "Officer assembly requires a recognizable command module");
    invalid = first;
    invalid.genome.sidearm = first.genome.torso;
    expect(containsError(validateOfficerVisual(invalid),
               OfficerVisualValidationError::InvalidSidearm),
        "Officer assembly requires a recognizable short weapon");
    invalid = first;
    invalid.genome.visualHash ^= 1U;
    expect(containsError(validateOfficerVisual(invalid),
               OfficerVisualValidationError::InvalidVisualHash),
        "Officer assembly detects genome hash corruption");
}

void testOfficerSeedBatch()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    std::array<bool, static_cast<std::size_t>(OfficerBodyProfileId::Count)> profiles{};
    std::array<bool, static_cast<std::size_t>(WearCondition::Damaged) + 1U> wearConditions{};
    std::array<bool, static_cast<std::size_t>(MaterialFamily::Count)> materials{};
    std::unordered_set<OfficerPartId> selectedParts;
    std::unordered_set<std::uint64_t> officerHashes;
    std::unordered_set<std::uint64_t> crossRoleHashes;
    bool allValid = true;
    bool foundAsymmetry = false;
    std::uint32_t fallbackCount = 0;

    for (wkc::core::Seed seed = 0; seed < 1'000; ++seed) {
        const OfficerVisual visual = generateOfficerVisual(seed);
        const WorkerVisual worker = generateWorkerVisual(seed);
        const GuardVisual guard = generateGuardVisual(seed);
        const auto validation = validateOfficerVisual(visual);
        const auto mesh = generateMesh(visual.meshRecipe);
        const auto albedo = generateAlbedo(visual.textureRecipe);
        if (!validation.isValid() || !mesh || !wkc::render::validateMeshData(mesh->mesh)
            || !albedo || !wkc::render::validateImageData(*albedo)) {
            allValid = false;
            std::cerr << "Officer generation failed at visual seed " << seed << '\n';
            break;
        }
        profiles[static_cast<std::size_t>(visual.genome.bodyProfile)] = true;
        wearConditions[static_cast<std::size_t>(visual.wear.condition)] = true;
        materials[static_cast<std::size_t>(visual.materials.family)] = true;
        selectedParts.insert(visual.genome.torso);
        selectedParts.insert(visual.genome.head);
        selectedParts.insert(visual.genome.leftArm);
        selectedParts.insert(visual.genome.rightArm);
        selectedParts.insert(visual.genome.leftLeg);
        selectedParts.insert(visual.genome.rightLeg);
        selectedParts.insert(visual.genome.commandModule);
        selectedParts.insert(visual.genome.sidearm);
        officerHashes.insert(visual.genome.visualHash);
        crossRoleHashes.insert(visual.genome.visualHash);
        crossRoleHashes.insert(worker.genome.visualHash);
        crossRoleHashes.insert(guard.genome.visualHash);
        foundAsymmetry = foundAsymmetry
            || visual.genome.leftArm != visual.genome.rightArm
            || visual.genome.leftLeg != visual.genome.rightLeg;
        fallbackCount += visual.fallbackCount;
    }
    expect(allValid, "1,000 Officer visual seeds pass assembly and asset gates");
    expect(fallbackCount == 0U, "built-in Officer library needs no batch fallback");
    expect(officerHashes.size() == 1'000U,
        "1,000 visual seeds produce unique canonical Officer hashes");
    expect(crossRoleHashes.size() == 3'000U,
        "Worker, Guard and Officer hashes remain isolated for equal visual seeds");
    expect(selectedParts.size() == officerPartLibrary().size(),
        "Officer seed batch reaches every compatible built-in part");
    expect(foundAsymmetry, "Officer seed batch exercises allowed limb asymmetry");
    expect(std::all_of(profiles.begin(), profiles.end(), [](const bool used) { return used; }),
        "Officer seed batch reaches Field, Command and Veteran profiles");
    expect(std::all_of(
               wearConditions.begin(), wearConditions.end(), [](const bool used) { return used; }),
        "Officer seed batch reaches every wear condition");
    expect(materials[static_cast<std::size_t>(MaterialFamily::Fabric)]
            && materials[static_cast<std::size_t>(MaterialFamily::Polymer)]
            && materials[static_cast<std::size_t>(MaterialFamily::PaintedSteel)],
        "Officer seed batch reaches every command material family");
}

void testEngineerPartLibrary()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    const auto library = engineerPartLibrary();
    std::unordered_set<EngineerPartId> ids;
    std::array<bool, static_cast<std::size_t>(WorkerPartSlot::Count)> slots{};
    bool definitionsValid = library.size() == 18U;
    for (const EngineerPartDefinition& part : library) {
        definitionsValid = definitionsValid && part.id >= 3'100U
            && ids.insert(part.id).second && part.slot < WorkerPartSlot::Count
            && part.weight > 0U && part.compatibility != 0U
            && part.semantic < SemanticRegion::Count && part.material < MaterialSlot::Count;
        if (part.slot < WorkerPartSlot::Count) {
            slots[static_cast<std::size_t>(part.slot)] = true;
        }
        if (part.slot == WorkerPartSlot::Back) {
            definitionsValid = definitionsValid
                && part.semantic == SemanticRegion::MechanicalCore
                && part.dimensionScale.y > part.dimensionScale.x;
        } else if (part.slot == WorkerPartSlot::Tool) {
            definitionsValid = definitionsValid && part.semantic == SemanticRegion::Tool
                && part.dimensionScale.y > part.dimensionScale.x * 2.0F;
        }
    }
    expect(definitionsValid,
        "Engineer library has 18 unique diagnostic definitions and repair tools");
    expect(std::all_of(slots.begin(), slots.end(), [](const bool covered) { return covered; }),
        "Engineer part library covers every required assembly slot");
}

void testDeterministicEngineer()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    constexpr wkc::core::Seed seed = 0x454E47494E454531ULL;
    const EngineerVisual first = generateEngineerVisual(seed);
    const EngineerVisual repeated = generateEngineerVisual(seed);
    const EngineerVisual different = generateEngineerVisual(seed + 1U);
    const WorkerVisual worker = generateWorkerVisual(seed);
    const GuardVisual guard = generateGuardVisual(seed);
    const OfficerVisual officer = generateOfficerVisual(seed);
    expect(validateEngineerVisual(first).isValid(), "representative Engineer visual validates");
    expect(first.genome.visualHash == repeated.genome.visualHash
            && first.meshRecipe == repeated.meshRecipe
            && first.textureRecipe == repeated.textureRecipe,
        "same Engineer seed reproduces genome and asset recipes exactly");
    expect(first.genome.visualHash != different.genome.visualHash,
        "different visual seed changes the Engineer genome hash");
    expect(first.genome.visualHash != worker.genome.visualHash
            && first.genome.visualHash != guard.genome.visualHash
            && first.genome.visualHash != officer.genome.visualHash
            && first.meshRecipe.seed != worker.meshRecipe.seed
            && first.meshRecipe.seed != guard.meshRecipe.seed
            && first.meshRecipe.seed != officer.meshRecipe.seed
            && first.textureRecipe.seed != worker.textureRecipe.seed
            && first.textureRecipe.seed != guard.textureRecipe.seed
            && first.textureRecipe.seed != officer.textureRecipe.seed,
        "Engineer uses isolated role-specific visual substreams");
    expect(first.fallbackCount == 0U,
        "built-in library generates representative Engineer without fallback");
    const auto mesh = generateMesh(first.meshRecipe);
    const auto albedo = generateAlbedo(first.textureRecipe);
    expect(mesh && wkc::render::validateMeshData(mesh->mesh),
        "complete Engineer genome produces renderer-valid mesh data");
    expect(albedo && wkc::render::validateImageData(*albedo),
        "complete Engineer genome produces renderer-valid albedo data");
    constexpr std::uint64_t goldenHash = 0x9E65A4A016966EB0ULL;
    expect(first.genome.visualHash == goldenHash,
        "golden Engineer visual hash remains stable across builds");

    EngineerVisual invalid = first;
    invalid.genome.head = 999'999U;
    expect(containsError(
               validateEngineerVisual(invalid), EngineerVisualValidationError::MissingPart),
        "Engineer assembly rejects missing part ID");
    invalid = first;
    invalid.genome.diagnosticPack = first.genome.torso;
    expect(containsError(validateEngineerVisual(invalid),
               EngineerVisualValidationError::InvalidDiagnosticPack),
        "Engineer assembly requires a recognizable diagnostic pack");
    invalid = first;
    invalid.genome.repairTool = first.genome.torso;
    expect(containsError(validateEngineerVisual(invalid),
               EngineerVisualValidationError::InvalidRepairTool),
        "Engineer assembly requires a recognizable repair tool");
    invalid = first;
    invalid.genome.visualHash ^= 1U;
    expect(containsError(validateEngineerVisual(invalid),
               EngineerVisualValidationError::InvalidVisualHash),
        "Engineer assembly detects genome hash corruption");
}

void testEngineerSeedBatch()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;

    std::array<bool, static_cast<std::size_t>(EngineerBodyProfileId::Count)> profiles{};
    std::array<bool, static_cast<std::size_t>(WearCondition::Damaged) + 1U> wearConditions{};
    std::array<bool, static_cast<std::size_t>(MaterialFamily::Count)> materials{};
    std::unordered_set<EngineerPartId> selectedParts;
    std::unordered_set<std::uint64_t> engineerHashes;
    std::unordered_set<std::uint64_t> crossRoleHashes;
    bool allValid = true;
    bool foundAsymmetry = false;
    std::uint32_t fallbackCount = 0;

    for (wkc::core::Seed seed = 0; seed < 1'000; ++seed) {
        const EngineerVisual visual = generateEngineerVisual(seed);
        const WorkerVisual worker = generateWorkerVisual(seed);
        const GuardVisual guard = generateGuardVisual(seed);
        const OfficerVisual officer = generateOfficerVisual(seed);
        const auto validation = validateEngineerVisual(visual);
        const auto mesh = generateMesh(visual.meshRecipe);
        const auto albedo = generateAlbedo(visual.textureRecipe);
        if (!validation.isValid() || !mesh || !wkc::render::validateMeshData(mesh->mesh)
            || !albedo || !wkc::render::validateImageData(*albedo)) {
            allValid = false;
            std::cerr << "Engineer generation failed at visual seed " << seed << '\n';
            break;
        }
        profiles[static_cast<std::size_t>(visual.genome.bodyProfile)] = true;
        wearConditions[static_cast<std::size_t>(visual.wear.condition)] = true;
        materials[static_cast<std::size_t>(visual.materials.family)] = true;
        selectedParts.insert(visual.genome.torso);
        selectedParts.insert(visual.genome.head);
        selectedParts.insert(visual.genome.leftArm);
        selectedParts.insert(visual.genome.rightArm);
        selectedParts.insert(visual.genome.leftLeg);
        selectedParts.insert(visual.genome.rightLeg);
        selectedParts.insert(visual.genome.diagnosticPack);
        selectedParts.insert(visual.genome.repairTool);
        engineerHashes.insert(visual.genome.visualHash);
        crossRoleHashes.insert(visual.genome.visualHash);
        crossRoleHashes.insert(worker.genome.visualHash);
        crossRoleHashes.insert(guard.genome.visualHash);
        crossRoleHashes.insert(officer.genome.visualHash);
        foundAsymmetry = foundAsymmetry
            || visual.genome.leftArm != visual.genome.rightArm
            || visual.genome.leftLeg != visual.genome.rightLeg;
        fallbackCount += visual.fallbackCount;
    }
    expect(allValid, "1,000 Engineer visual seeds pass assembly and asset gates");
    expect(fallbackCount == 0U, "built-in Engineer library needs no batch fallback");
    expect(engineerHashes.size() == 1'000U,
        "1,000 visual seeds produce unique canonical Engineer hashes");
    expect(crossRoleHashes.size() == 4'000U,
        "all four enemy roles keep isolated hashes for equal visual seeds");
    expect(selectedParts.size() == engineerPartLibrary().size(),
        "Engineer seed batch reaches every compatible built-in part");
    expect(foundAsymmetry, "Engineer seed batch exercises allowed limb asymmetry");
    expect(std::all_of(profiles.begin(), profiles.end(), [](const bool used) { return used; }),
        "Engineer seed batch reaches Maintenance, Diagnostics and Hazmat profiles");
    expect(std::all_of(
               wearConditions.begin(), wearConditions.end(), [](const bool used) { return used; }),
        "Engineer seed batch reaches every wear condition");
    expect(materials[static_cast<std::size_t>(MaterialFamily::Aluminum)]
            && materials[static_cast<std::size_t>(MaterialFamily::Polymer)]
            && materials[static_cast<std::size_t>(MaterialFamily::PaintedSteel)],
        "Engineer seed batch reaches every technical material family");
}

void testIndustrialRobotPartLibrary()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;
    const auto library = industrialRobotPartLibrary();
    std::unordered_set<IndustrialRobotPartId> ids;
    std::array<bool, static_cast<std::size_t>(WorkerPartSlot::Count)> slots{};
    bool definitionsValid = library.size() == 18U;
    for (const IndustrialRobotPartDefinition& part : library) {
        definitionsValid = definitionsValid && part.id >= 4'100U
            && ids.insert(part.id).second && part.slot < WorkerPartSlot::Count
            && part.weight > 0U && part.compatibility != 0U
            && part.semantic < SemanticRegion::Count && part.material < MaterialSlot::Count;
        if (part.slot < WorkerPartSlot::Count) {
            slots[static_cast<std::size_t>(part.slot)] = true;
        }
        if (part.slot == WorkerPartSlot::Torso
            || part.slot == WorkerPartSlot::Back) {
            definitionsValid = definitionsValid
                && part.semantic == SemanticRegion::MechanicalCore;
        } else if (part.slot == WorkerPartSlot::Head) {
            definitionsValid = definitionsValid && part.semantic == SemanticRegion::Lens;
        } else if (part.slot == WorkerPartSlot::Arm) {
            definitionsValid = definitionsValid && part.semantic == SemanticRegion::Piston
                && part.dimensionScale.x >= 1.2F;
        } else if (part.slot == WorkerPartSlot::Tool) {
            definitionsValid = definitionsValid && part.semantic == SemanticRegion::Weapon;
        }
    }
    expect(definitionsValid,
        "IndustrialRobot library has 18 unique heavy mechanical definitions");
    expect(std::all_of(slots.begin(), slots.end(), [](const bool covered) { return covered; }),
        "IndustrialRobot part library covers every required assembly slot");
}

void testDeterministicIndustrialRobot()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;
    constexpr wkc::core::Seed seed = 0x494E44524F424F54ULL;
    const IndustrialRobotVisual first = generateIndustrialRobotVisual(seed);
    const IndustrialRobotVisual repeated = generateIndustrialRobotVisual(seed);
    const IndustrialRobotVisual different = generateIndustrialRobotVisual(seed + 1U);
    expect(validateIndustrialRobotVisual(first).isValid(),
        "representative IndustrialRobot visual validates");
    expect(first.genome.visualHash == repeated.genome.visualHash
            && first.meshRecipe == repeated.meshRecipe
            && first.textureRecipe == repeated.textureRecipe,
        "same IndustrialRobot seed reproduces genome and asset recipes exactly");
    expect(first.genome.visualHash != different.genome.visualHash,
        "different visual seed changes the IndustrialRobot genome hash");
    expect(first.fallbackCount == 0U,
        "built-in library generates representative IndustrialRobot without fallback");
    const auto mesh = generateMesh(first.meshRecipe);
    const auto albedo = generateAlbedo(first.textureRecipe);
    expect(mesh && wkc::render::validateMeshData(mesh->mesh),
        "complete IndustrialRobot genome produces renderer-valid mesh data");
    expect(albedo && wkc::render::validateImageData(*albedo),
        "complete IndustrialRobot genome produces renderer-valid albedo data");
    constexpr std::uint64_t goldenHash = 0x5D654A79E5B31648ULL;
    expect(first.genome.visualHash == goldenHash,
        "golden IndustrialRobot visual hash remains stable across builds");

    IndustrialRobotVisual invalid = first;
    invalid.genome.torsoCore = invalid.genome.sensorHead;
    expect(containsError(validateIndustrialRobotVisual(invalid),
               IndustrialRobotVisualValidationError::InvalidTorsoCore),
        "IndustrialRobot assembly requires a mechanical torso core");
    invalid = first;
    invalid.genome.leftReactor = invalid.genome.leftArm;
    expect(containsError(validateIndustrialRobotVisual(invalid),
               IndustrialRobotVisualValidationError::InvalidReactorModules),
        "IndustrialRobot assembly requires two reactor modules");
    invalid = first;
    invalid.textureRecipe.emissiveAmount = 0.0F;
    expect(containsError(validateIndustrialRobotVisual(invalid),
               IndustrialRobotVisualValidationError::InvalidEmissiveCore),
        "IndustrialRobot assembly requires a readable emissive core");
    invalid = first;
    invalid.genome.visualHash ^= 1U;
    expect(containsError(validateIndustrialRobotVisual(invalid),
               IndustrialRobotVisualValidationError::InvalidVisualHash),
        "IndustrialRobot assembly detects genome hash corruption");
}

void testIndustrialRobotSeedBatch()
{
    using namespace wkc::procedural;
    using namespace gdp::game::visual;
    std::array<bool, static_cast<std::size_t>(IndustrialRobotBodyProfileId::Count)> profiles{};
    std::array<bool, static_cast<std::size_t>(WearCondition::Damaged) + 1U> wearConditions{};
    std::array<bool, static_cast<std::size_t>(MaterialFamily::Count)> materials{};
    std::unordered_set<IndustrialRobotPartId> selectedParts;
    std::unordered_set<std::uint64_t> robotHashes;
    std::unordered_set<std::uint64_t> crossRoleHashes;
    bool allValid = true;
    bool foundAsymmetry = false;
    std::uint32_t fallbackCount = 0;
    for (wkc::core::Seed seed = 0; seed < 1'000; ++seed) {
        const IndustrialRobotVisual visual = generateIndustrialRobotVisual(seed);
        const auto validation = validateIndustrialRobotVisual(visual);
        const auto mesh = generateMesh(visual.meshRecipe);
        const auto albedo = generateAlbedo(visual.textureRecipe);
        if (!validation.isValid() || !mesh || !wkc::render::validateMeshData(mesh->mesh)
            || !albedo || !wkc::render::validateImageData(*albedo)) {
            allValid = false;
            std::cerr << "IndustrialRobot generation failed at visual seed " << seed << '\n';
            break;
        }
        profiles[static_cast<std::size_t>(visual.genome.bodyProfile)] = true;
        wearConditions[static_cast<std::size_t>(visual.wear.condition)] = true;
        materials[static_cast<std::size_t>(visual.materials.family)] = true;
        selectedParts.insert(visual.genome.torsoCore);
        selectedParts.insert(visual.genome.sensorHead);
        selectedParts.insert(visual.genome.leftArm);
        selectedParts.insert(visual.genome.rightArm);
        selectedParts.insert(visual.genome.leftLeg);
        selectedParts.insert(visual.genome.rightLeg);
        selectedParts.insert(visual.genome.leftReactor);
        selectedParts.insert(visual.genome.rightReactor);
        selectedParts.insert(visual.genome.heavyWeapon);
        robotHashes.insert(visual.genome.visualHash);
        crossRoleHashes.insert(visual.genome.visualHash);
        crossRoleHashes.insert(generateWorkerVisual(seed).genome.visualHash);
        crossRoleHashes.insert(generateGuardVisual(seed).genome.visualHash);
        crossRoleHashes.insert(generateOfficerVisual(seed).genome.visualHash);
        crossRoleHashes.insert(generateEngineerVisual(seed).genome.visualHash);
        foundAsymmetry = foundAsymmetry
            || visual.genome.leftArm != visual.genome.rightArm
            || visual.genome.leftLeg != visual.genome.rightLeg
            || visual.genome.leftReactor != visual.genome.rightReactor;
        fallbackCount += visual.fallbackCount;
    }
    expect(allValid, "1,000 IndustrialRobot visual seeds pass assembly and asset gates");
    expect(fallbackCount == 0U,
        "built-in IndustrialRobot library needs no batch fallback");
    expect(robotHashes.size() == 1'000U,
        "1,000 visual seeds produce unique canonical IndustrialRobot hashes");
    expect(crossRoleHashes.size() == 5'000U,
        "all five enemy roles keep isolated hashes for equal visual seeds");
    expect(selectedParts.size() == industrialRobotPartLibrary().size(),
        "IndustrialRobot seed batch reaches every compatible built-in part");
    expect(foundAsymmetry,
        "IndustrialRobot seed batch exercises limbs and dual-reactor asymmetry");
    expect(std::all_of(profiles.begin(), profiles.end(), [](const bool used) { return used; }),
        "IndustrialRobot seed batch reaches Sentry, Foundry and Siege profiles");
    expect(std::all_of(
               wearConditions.begin(), wearConditions.end(), [](const bool used) { return used; }),
        "IndustrialRobot seed batch reaches every wear condition");
    expect(materials[static_cast<std::size_t>(MaterialFamily::BareSteel)]
            && materials[static_cast<std::size_t>(MaterialFamily::PaintedSteel)]
            && materials[static_cast<std::size_t>(MaterialFamily::Aluminum)],
        "IndustrialRobot seed batch reaches every heavy material family");
}

} // namespace

int main()
{
    testPartLibrary();
    testDeterministicWorker();
    testWorkerSeedBatch();
    testGuardPartLibrary();
    testDeterministicGuard();
    testGuardSeedBatch();
    testOfficerPartLibrary();
    testDeterministicOfficer();
    testOfficerSeedBatch();
    testEngineerPartLibrary();
    testDeterministicEngineer();
    testEngineerSeedBatch();
    testIndustrialRobotPartLibrary();
    testDeterministicIndustrialRobot();
    testIndustrialRobotSeedBatch();

    if (failures != 0) {
        std::cerr << failures << " enemy generation test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP enemy generation tests passed\n";
    return 0;
}
