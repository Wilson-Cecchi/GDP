#include "procedural/EnemyGenerator.hpp"

#include "core/Hash.hpp"
#include "core/Random.hpp"
#include "procedural/MeshGenerator.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <optional>

namespace gdp::procedural {
namespace {

constexpr std::array parts{
    WorkerPartDefinition{100, WorkerPartSlot::Torso, SocketType::Root,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.92F, 1.0F, 0.90F}, MaterialSlot::Armor, SemanticRegion::ArmorPlate, 4},
    WorkerPartDefinition{101, WorkerPartSlot::Torso, SocketType::Root,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Box,
        {1.08F, 0.96F, 1.08F}, MaterialSlot::Armor, SemanticRegion::ArmorPlate, 3},
    WorkerPartDefinition{102, WorkerPartSlot::Torso, SocketType::Root,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Cylinder,
        {1.04F, 1.0F, 1.04F}, MaterialSlot::Primary, SemanticRegion::MechanicalCore, 2},
    WorkerPartDefinition{200, WorkerPartSlot::Head, SocketType::Neck,
        compatibilityLight | compatibilityStandard | compatibilityHeavy,
        PrimitiveType::Box, {1.0F, 0.92F, 0.92F}, MaterialSlot::Primary,
        SemanticRegion::MechanicalCore, 4},
    WorkerPartDefinition{201, WorkerPartSlot::Head, SocketType::Neck,
        compatibilityStandard | compatibilityHeavy | compatibilityMechanical,
        PrimitiveType::Cylinder, {0.92F, 1.0F, 0.92F}, MaterialSlot::Lens,
        SemanticRegion::Lens, 2},
    WorkerPartDefinition{202, WorkerPartSlot::Head, SocketType::Neck,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.86F, 1.05F, 1.05F}, MaterialSlot::Secondary, SemanticRegion::Tool, 2},
    WorkerPartDefinition{300, WorkerPartSlot::Arm, SocketType::ShoulderLeft,
        compatibilityLight | compatibilityStandard | compatibilityHeavy,
        PrimitiveType::Box, {0.90F, 1.0F, 0.90F}, MaterialSlot::Secondary,
        SemanticRegion::Tool, 4},
    WorkerPartDefinition{301, WorkerPartSlot::Arm, SocketType::ShoulderLeft,
        compatibilityStandard | compatibilityHeavy | compatibilityMechanical,
        PrimitiveType::Cylinder, {0.78F, 1.0F, 0.78F}, MaterialSlot::Primary,
        SemanticRegion::Piston, 3},
    WorkerPartDefinition{302, WorkerPartSlot::Arm, SocketType::ShoulderLeft,
        compatibilityHeavy, PrimitiveType::Box, {1.18F, 0.94F, 1.12F},
        MaterialSlot::Armor, SemanticRegion::ArmorPlate, 2},
    WorkerPartDefinition{400, WorkerPartSlot::Leg, SocketType::HipLeft,
        compatibilityLight | compatibilityStandard | compatibilityHeavy,
        PrimitiveType::Box, {0.92F, 1.0F, 1.0F}, MaterialSlot::Primary,
        SemanticRegion::FootContact, 4},
    WorkerPartDefinition{401, WorkerPartSlot::Leg, SocketType::HipLeft,
        compatibilityLight | compatibilityStandard | compatibilityMechanical,
        PrimitiveType::Cylinder, {0.82F, 1.0F, 0.82F}, MaterialSlot::Secondary,
        SemanticRegion::Piston, 3},
    WorkerPartDefinition{402, WorkerPartSlot::Leg, SocketType::HipLeft,
        compatibilityHeavy, PrimitiveType::Box, {1.18F, 1.0F, 1.15F},
        MaterialSlot::Armor, SemanticRegion::FootContact, 2},
    WorkerPartDefinition{500, WorkerPartSlot::Back, SocketType::Back,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.20F, 0.28F, 0.11F}, MaterialSlot::Secondary,
        SemanticRegion::MechanicalCore, 3},
    WorkerPartDefinition{501, WorkerPartSlot::Back, SocketType::Back,
        compatibilityStandard | compatibilityHeavy | compatibilityMechanical,
        PrimitiveType::Cylinder, {0.18F, 0.32F, 0.18F}, MaterialSlot::Primary,
        SemanticRegion::MechanicalCore, 3},
    WorkerPartDefinition{502, WorkerPartSlot::Back, SocketType::Back,
        compatibilityHeavy, PrimitiveType::Box, {0.30F, 0.34F, 0.16F},
        MaterialSlot::Armor, SemanticRegion::ArmorPlate, 2},
    WorkerPartDefinition{600, WorkerPartSlot::Tool, SocketType::Tool,
        compatibilityLight | compatibilityStandard | compatibilityHeavy,
        PrimitiveType::Box, {0.10F, 0.28F, 0.10F}, MaterialSlot::Secondary,
        SemanticRegion::Tool, 4},
    WorkerPartDefinition{601, WorkerPartSlot::Tool, SocketType::Tool,
        compatibilityStandard | compatibilityHeavy | compatibilityMechanical,
        PrimitiveType::Cylinder, {0.10F, 0.32F, 0.10F}, MaterialSlot::Primary,
        SemanticRegion::Tool, 3},
    WorkerPartDefinition{602, WorkerPartSlot::Tool, SocketType::Tool,
        compatibilityHeavy, PrimitiveType::Box, {0.14F, 0.30F, 0.12F},
        MaterialSlot::Armor, SemanticRegion::Weapon, 2},
};

constexpr std::array guardParts{
    GuardPartDefinition{1'100, WorkerPartSlot::Torso, SocketType::Root,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.96F, 1.0F, 0.92F}, MaterialSlot::Fabric, SemanticRegion::ArmorPlate, 4},
    GuardPartDefinition{1'101, WorkerPartSlot::Torso, SocketType::Root,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Box,
        {1.05F, 0.98F, 1.04F}, MaterialSlot::Armor, SemanticRegion::ArmorPlate, 3},
    GuardPartDefinition{1'102, WorkerPartSlot::Torso, SocketType::Root,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Box,
        {1.14F, 1.02F, 1.10F}, MaterialSlot::Armor, SemanticRegion::ArmorPlate, 2},
    GuardPartDefinition{1'200, WorkerPartSlot::Head, SocketType::Neck,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {1.02F, 0.90F, 1.04F}, MaterialSlot::Armor, SemanticRegion::ArmorPlate, 4},
    GuardPartDefinition{1'201, WorkerPartSlot::Head, SocketType::Neck,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Cylinder,
        {0.96F, 0.94F, 0.96F}, MaterialSlot::Lens, SemanticRegion::Lens, 3},
    GuardPartDefinition{1'202, WorkerPartSlot::Head, SocketType::Neck,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Box,
        {1.08F, 1.0F, 1.08F}, MaterialSlot::Armor, SemanticRegion::Lens, 2},
    GuardPartDefinition{1'300, WorkerPartSlot::Arm, SocketType::ShoulderLeft,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.88F, 1.0F, 0.88F}, MaterialSlot::Fabric, SemanticRegion::Tool, 4},
    GuardPartDefinition{1'301, WorkerPartSlot::Arm, SocketType::ShoulderLeft,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Cylinder,
        {0.94F, 0.98F, 0.94F}, MaterialSlot::Secondary, SemanticRegion::Tool, 3},
    GuardPartDefinition{1'302, WorkerPartSlot::Arm, SocketType::ShoulderLeft,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Box,
        {1.12F, 0.96F, 1.08F}, MaterialSlot::Armor, SemanticRegion::ArmorPlate, 2},
    GuardPartDefinition{1'400, WorkerPartSlot::Leg, SocketType::HipLeft,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.90F, 1.0F, 0.94F}, MaterialSlot::Fabric, SemanticRegion::FootContact, 4},
    GuardPartDefinition{1'401, WorkerPartSlot::Leg, SocketType::HipLeft,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Box,
        {1.0F, 1.0F, 1.0F}, MaterialSlot::Secondary, SemanticRegion::FootContact, 3},
    GuardPartDefinition{1'402, WorkerPartSlot::Leg, SocketType::HipLeft,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Box,
        {1.14F, 1.0F, 1.10F}, MaterialSlot::Armor, SemanticRegion::FootContact, 2},
    GuardPartDefinition{1'500, WorkerPartSlot::Back, SocketType::Back,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.18F, 0.24F, 0.10F}, MaterialSlot::Secondary,
        SemanticRegion::MechanicalCore, 4},
    GuardPartDefinition{1'501, WorkerPartSlot::Back, SocketType::Back,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Box,
        {0.24F, 0.30F, 0.13F}, MaterialSlot::Secondary,
        SemanticRegion::MechanicalCore, 3},
    GuardPartDefinition{1'502, WorkerPartSlot::Back, SocketType::Back,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Cylinder,
        {0.18F, 0.34F, 0.18F}, MaterialSlot::Armor,
        SemanticRegion::MechanicalCore, 2},
    GuardPartDefinition{1'600, WorkerPartSlot::Tool, SocketType::Tool,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.34F, 0.065F, 0.075F}, MaterialSlot::Primary, SemanticRegion::Weapon, 4},
    GuardPartDefinition{1'601, WorkerPartSlot::Tool, SocketType::Tool,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Box,
        {0.39F, 0.075F, 0.085F}, MaterialSlot::Primary, SemanticRegion::Weapon, 3},
    GuardPartDefinition{1'602, WorkerPartSlot::Tool, SocketType::Tool,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Box,
        {0.44F, 0.09F, 0.10F}, MaterialSlot::Armor, SemanticRegion::Weapon, 2},
};

constexpr std::array officerParts{
    OfficerPartDefinition{2'100, WorkerPartSlot::Torso, SocketType::Root,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.88F, 1.02F, 0.88F}, MaterialSlot::Fabric, SemanticRegion::ArmorPlate, 4},
    OfficerPartDefinition{2'101, WorkerPartSlot::Torso, SocketType::Root,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Box,
        {0.98F, 1.0F, 0.94F}, MaterialSlot::Primary, SemanticRegion::ArmorPlate, 3},
    OfficerPartDefinition{2'102, WorkerPartSlot::Torso, SocketType::Root,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Box,
        {1.08F, 1.0F, 1.04F}, MaterialSlot::Armor, SemanticRegion::ArmorPlate, 2},
    OfficerPartDefinition{2'200, WorkerPartSlot::Head, SocketType::Neck,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.92F, 1.02F, 0.94F}, MaterialSlot::Primary, SemanticRegion::Generic, 4},
    OfficerPartDefinition{2'201, WorkerPartSlot::Head, SocketType::Neck,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Cylinder,
        {0.90F, 0.96F, 0.90F}, MaterialSlot::Lens, SemanticRegion::Lens, 3},
    OfficerPartDefinition{2'202, WorkerPartSlot::Head, SocketType::Neck,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Box,
        {1.02F, 0.98F, 1.02F}, MaterialSlot::Armor, SemanticRegion::ArmorPlate, 2},
    OfficerPartDefinition{2'300, WorkerPartSlot::Arm, SocketType::ShoulderLeft,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.80F, 1.0F, 0.82F}, MaterialSlot::Fabric, SemanticRegion::Tool, 4},
    OfficerPartDefinition{2'301, WorkerPartSlot::Arm, SocketType::ShoulderLeft,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Box,
        {0.88F, 0.98F, 0.88F}, MaterialSlot::Secondary, SemanticRegion::Tool, 3},
    OfficerPartDefinition{2'302, WorkerPartSlot::Arm, SocketType::ShoulderLeft,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Box,
        {1.02F, 0.96F, 1.0F}, MaterialSlot::Armor, SemanticRegion::ArmorPlate, 2},
    OfficerPartDefinition{2'400, WorkerPartSlot::Leg, SocketType::HipLeft,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.84F, 1.0F, 0.88F}, MaterialSlot::Fabric, SemanticRegion::FootContact, 4},
    OfficerPartDefinition{2'401, WorkerPartSlot::Leg, SocketType::HipLeft,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Box,
        {0.92F, 1.0F, 0.94F}, MaterialSlot::Secondary, SemanticRegion::FootContact, 3},
    OfficerPartDefinition{2'402, WorkerPartSlot::Leg, SocketType::HipLeft,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Box,
        {1.06F, 1.0F, 1.04F}, MaterialSlot::Armor, SemanticRegion::FootContact, 2},
    OfficerPartDefinition{2'500, WorkerPartSlot::Back, SocketType::Back,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.16F, 0.26F, 0.09F}, MaterialSlot::Secondary,
        SemanticRegion::MechanicalCore, 4},
    OfficerPartDefinition{2'501, WorkerPartSlot::Back, SocketType::Back,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Box,
        {0.20F, 0.31F, 0.11F}, MaterialSlot::Primary,
        SemanticRegion::MechanicalCore, 3},
    OfficerPartDefinition{2'502, WorkerPartSlot::Back, SocketType::Back,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Cylinder,
        {0.16F, 0.35F, 0.16F}, MaterialSlot::Armor,
        SemanticRegion::MechanicalCore, 2},
    OfficerPartDefinition{2'600, WorkerPartSlot::Tool, SocketType::Tool,
        compatibilityLight | compatibilityStandard, PrimitiveType::Box,
        {0.20F, 0.075F, 0.07F}, MaterialSlot::Primary, SemanticRegion::Weapon, 4},
    OfficerPartDefinition{2'601, WorkerPartSlot::Tool, SocketType::Tool,
        compatibilityStandard | compatibilityHeavy, PrimitiveType::Box,
        {0.24F, 0.085F, 0.08F}, MaterialSlot::Primary, SemanticRegion::Weapon, 3},
    OfficerPartDefinition{2'602, WorkerPartSlot::Tool, SocketType::Tool,
        compatibilityHeavy | compatibilityMechanical, PrimitiveType::Box,
        {0.28F, 0.095F, 0.09F}, MaterialSlot::Armor, SemanticRegion::Weapon, 2},
};

const WorkerPartDefinition* findPart(const WorkerPartId id) noexcept
{
    const auto found = std::find_if(
        parts.begin(), parts.end(),
        [id](const WorkerPartDefinition& part) { return part.id == id; });
    return found == parts.end() ? nullptr : &*found;
}

const GuardPartDefinition* findGuardPart(const GuardPartId id) noexcept
{
    const auto found = std::find_if(
        guardParts.begin(), guardParts.end(),
        [id](const GuardPartDefinition& part) { return part.id == id; });
    return found == guardParts.end() ? nullptr : &*found;
}

const OfficerPartDefinition* findOfficerPart(const OfficerPartId id) noexcept
{
    const auto found = std::find_if(
        officerParts.begin(), officerParts.end(),
        [id](const OfficerPartDefinition& part) { return part.id == id; });
    return found == officerParts.end() ? nullptr : &*found;
}

BodyProfile makeBodyProfile(const WorkerBodyProfileId id)
{
    BodyProfile body;
    body.id = id;
    switch (id) {
    case WorkerBodyProfileId::Standard:
        body = {id, 1.0F, 0.38F, 0.26F, 0.58F, 0.40F, 0.42F, 0.15F,
            0.0F, 1.0F, 1.0F, compatibilityStandard, {}};
        break;
    case WorkerBodyProfileId::Compact:
        body = {id, 0.92F, 0.44F, 0.30F, 0.62F, 0.36F, 0.39F, 0.17F,
            2.0F, 1.05F, 0.95F, compatibilityHeavy, {}};
        break;
    case WorkerBodyProfileId::Lanky:
        body = {id, 1.0F, 0.32F, 0.22F, 0.54F, 0.45F, 0.46F, 0.14F,
            4.0F, 0.92F, 1.08F, compatibilityLight, {}};
        break;
    case WorkerBodyProfileId::Heavy:
        body = {id, 0.98F, 0.46F, 0.32F, 0.68F, 0.37F, 0.39F, 0.16F,
            6.0F, 1.08F, 1.12F, compatibilityHeavy | compatibilityMechanical, {}};
        break;
    case WorkerBodyProfileId::Count:
        break;
    }

    constexpr float baseHeight = 1.80F;
    const float height = baseHeight * body.heightScale;
    const float legHeight = height * body.legLength;
    const float headHeight = height * body.headScale;
    const float torsoHeight = height - legHeight - headHeight;
    const float shoulderX = height * body.shoulderWidth * 0.5F;
    const float hipX = height * body.torsoWidth * 0.22F;
    const float shoulderY = legHeight + (torsoHeight * 0.62F);
    body.sockets = {
        {SocketType::Root, {0.0F, legHeight + (torsoHeight * 0.5F), 0.0F},
            {0.0F, 1.0F, 0.0F}, torsoHeight * 0.5F, body.compatibility},
        {SocketType::Neck, {0.0F, legHeight + torsoHeight, 0.0F}, {0.0F, 1.0F, 0.0F},
            headHeight * 0.5F, body.compatibility},
        {SocketType::ShoulderLeft, {-shoulderX, shoulderY, 0.0F}, {-1.0F, 0.0F, 0.0F},
            0.12F, body.compatibility},
        {SocketType::ShoulderRight, {shoulderX, shoulderY, 0.0F}, {1.0F, 0.0F, 0.0F},
            0.12F, body.compatibility},
        {SocketType::HipLeft, {-hipX, legHeight, 0.0F}, {0.0F, -1.0F, 0.0F},
            0.13F, body.compatibility},
        {SocketType::HipRight, {hipX, legHeight, 0.0F}, {0.0F, -1.0F, 0.0F},
            0.13F, body.compatibility},
        {SocketType::Back, {0.0F, legHeight + (torsoHeight * 0.58F),
             -(height * body.torsoDepth * 0.5F)}, {0.0F, 0.0F, -1.0F},
            0.16F, body.compatibility},
        {SocketType::Tool, {shoulderX, shoulderY - (height * body.armLength), 0.0F},
            {0.0F, -1.0F, 0.0F}, 0.10F, body.compatibility},
    };
    return body;
}

WorkerBodyProfileId guardBaseProfile(const GuardBodyProfileId id) noexcept
{
    switch (id) {
    case GuardBodyProfileId::Patrol:
        return WorkerBodyProfileId::Standard;
    case GuardBodyProfileId::Tactical:
        return WorkerBodyProfileId::Lanky;
    case GuardBodyProfileId::Armored:
        return WorkerBodyProfileId::Heavy;
    case GuardBodyProfileId::Count:
        break;
    }
    return WorkerBodyProfileId::Count;
}

WorkerBodyProfileId officerBaseProfile(const OfficerBodyProfileId id) noexcept
{
    switch (id) {
    case OfficerBodyProfileId::Field:
        return WorkerBodyProfileId::Standard;
    case OfficerBodyProfileId::Command:
        return WorkerBodyProfileId::Lanky;
    case OfficerBodyProfileId::Veteran:
        return WorkerBodyProfileId::Heavy;
    case OfficerBodyProfileId::Count:
        break;
    }
    return WorkerBodyProfileId::Count;
}

const SocketDefinition* findSocket(const BodyProfile& body, const SocketType type) noexcept
{
    const auto found = std::find_if(
        body.sockets.begin(), body.sockets.end(),
        [type](const SocketDefinition& socket) { return socket.type == type; });
    return found == body.sockets.end() ? nullptr : &*found;
}

const WorkerPartDefinition* selectPart(
    const WorkerPartSlot slot,
    const BodyProfile& body,
    const gdp::core::Seed seed,
    const std::uint64_t localId) noexcept
{
    std::array<const WorkerPartDefinition*, parts.size()> candidates{};
    std::size_t candidateCount = 0;
    std::uint32_t totalWeight = 0;
    for (const WorkerPartDefinition& part : parts) {
        if (part.slot != slot || (part.compatibility & body.compatibility) == 0) {
            continue;
        }
        candidates[candidateCount++] = &part;
        totalWeight += part.weight;
    }
    if (candidateCount == 0 || totalWeight == 0) {
        return nullptr;
    }
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::core::SeedDomain::EnemyParts, localId)};
    std::uint32_t choice = static_cast<std::uint32_t>(random.rangeInt(
        0, static_cast<int>(totalWeight - 1U)));
    for (std::size_t index = 0; index < candidateCount; ++index) {
        if (choice < candidates[index]->weight) {
            return candidates[index];
        }
        choice -= candidates[index]->weight;
    }
    return candidates[0];
}

const GuardPartDefinition* selectGuardPart(
    const WorkerPartSlot slot,
    const BodyProfile& body,
    const gdp::core::Seed seed,
    const std::uint64_t localId) noexcept
{
    std::array<const GuardPartDefinition*, guardParts.size()> candidates{};
    std::size_t candidateCount = 0;
    std::uint32_t totalWeight = 0;
    for (const GuardPartDefinition& part : guardParts) {
        if (part.slot != slot || (part.compatibility & body.compatibility) == 0) {
            continue;
        }
        candidates[candidateCount++] = &part;
        totalWeight += part.weight;
    }
    if (candidateCount == 0 || totalWeight == 0) {
        return nullptr;
    }
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::core::SeedDomain::EnemyParts, localId)};
    std::uint32_t choice = static_cast<std::uint32_t>(random.rangeInt(
        0, static_cast<int>(totalWeight - 1U)));
    for (std::size_t index = 0; index < candidateCount; ++index) {
        if (choice < candidates[index]->weight) {
            return candidates[index];
        }
        choice -= candidates[index]->weight;
    }
    return candidates[0];
}

const OfficerPartDefinition* selectOfficerPart(
    const WorkerPartSlot slot,
    const BodyProfile& body,
    const gdp::core::Seed seed,
    const std::uint64_t localId) noexcept
{
    std::array<const OfficerPartDefinition*, officerParts.size()> candidates{};
    std::size_t candidateCount = 0;
    std::uint32_t totalWeight = 0;
    for (const OfficerPartDefinition& part : officerParts) {
        if (part.slot != slot || (part.compatibility & body.compatibility) == 0) {
            continue;
        }
        candidates[candidateCount++] = &part;
        totalWeight += part.weight;
    }
    if (candidateCount == 0 || totalWeight == 0) {
        return nullptr;
    }
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::core::SeedDomain::EnemyParts, localId)};
    std::uint32_t choice = static_cast<std::uint32_t>(random.rangeInt(
        0, static_cast<int>(totalWeight - 1U)));
    for (std::size_t index = 0; index < candidateCount; ++index) {
        if (choice < candidates[index]->weight) {
            return candidates[index];
        }
        choice -= candidates[index]->weight;
    }
    return candidates[0];
}

PrimitiveRecipe makePrimitive(
    const WorkerPartDefinition& part,
    const gdp::core::Vec3 position,
    const gdp::core::Vec3 dimensions,
    const gdp::core::Vec3 rotation = {})
{
    return {
        part.primitive,
        {.position = position, .rotationDegrees = rotation},
        {dimensions.x * part.dimensionScale.x,
            dimensions.y * part.dimensionScale.y,
            dimensions.z * part.dimensionScale.z},
        part.material,
        part.semantic,
    };
}

WearProfile generateWear(const gdp::core::Seed seed)
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::core::SeedDomain::EnemyWear, 0)};
    const int roll = random.rangeInt(0, 99);
    WearProfile wear;
    if (roll < 12) {
        wear.condition = WearCondition::Clean;
        wear.wear = random.rangeFloat(0.02F, 0.10F);
        wear.dirt = random.rangeFloat(0.01F, 0.08F);
    } else if (roll < 55) {
        wear.condition = WearCondition::Used;
        wear.wear = random.rangeFloat(0.12F, 0.32F);
        wear.dirt = random.rangeFloat(0.10F, 0.30F);
    } else if (roll < 85) {
        wear.condition = WearCondition::Worn;
        wear.wear = random.rangeFloat(0.34F, 0.58F);
        wear.dirt = random.rangeFloat(0.28F, 0.52F);
    } else {
        wear.condition = WearCondition::Damaged;
        wear.wear = random.rangeFloat(0.60F, 0.84F);
        wear.dirt = random.rangeFloat(0.42F, 0.70F);
    }
    wear.scratch = std::clamp(
        wear.wear * random.rangeFloat(0.55F, 0.95F), 0.0F, 1.0F);
    wear.oxidation = std::clamp(
        wear.dirt * random.rangeFloat(0.15F, 0.48F), 0.0F, 1.0F);
    return wear;
}

WorkerMaterialSet generateMaterials(const gdp::core::Seed seed)
{
    constexpr std::array families{
        MaterialFamily::PaintedSteel,
        MaterialFamily::BareSteel,
        MaterialFamily::Aluminum,
    };
    constexpr std::array primaryColors{
        gdp::core::Vec3{0.72F, 0.38F, 0.12F},
        gdp::core::Vec3{0.18F, 0.48F, 0.46F},
        gdp::core::Vec3{0.72F, 0.58F, 0.12F},
        gdp::core::Vec3{0.52F, 0.16F, 0.12F},
    };
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::core::SeedDomain::EnemyMaterial, 0)};
    WorkerMaterialSet materials;
    materials.family = families[static_cast<std::size_t>(random.rangeInt(
        0, static_cast<int>(families.size() - 1U)))];
    materials.primaryColor = primaryColors[static_cast<std::size_t>(random.rangeInt(
        0, static_cast<int>(primaryColors.size() - 1U)))];
    const float secondaryScale = random.rangeFloat(0.42F, 0.64F);
    materials.secondaryColor = materials.primaryColor * secondaryScale;
    return materials;
}

WearProfile generateGuardWear(const gdp::core::Seed seed)
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::core::SeedDomain::EnemyWear, 100)};
    const int roll = random.rangeInt(0, 99);
    WearProfile wear;
    if (roll < 20) {
        wear.condition = WearCondition::Clean;
        wear.wear = random.rangeFloat(0.01F, 0.07F);
        wear.dirt = random.rangeFloat(0.01F, 0.06F);
    } else if (roll < 64) {
        wear.condition = WearCondition::Used;
        wear.wear = random.rangeFloat(0.08F, 0.24F);
        wear.dirt = random.rangeFloat(0.06F, 0.20F);
    } else if (roll < 90) {
        wear.condition = WearCondition::Worn;
        wear.wear = random.rangeFloat(0.25F, 0.44F);
        wear.dirt = random.rangeFloat(0.20F, 0.38F);
    } else {
        wear.condition = WearCondition::Damaged;
        wear.wear = random.rangeFloat(0.45F, 0.64F);
        wear.dirt = random.rangeFloat(0.34F, 0.54F);
    }
    wear.scratch = std::clamp(
        wear.wear * random.rangeFloat(0.48F, 0.82F), 0.0F, 1.0F);
    wear.oxidation = std::clamp(
        wear.dirt * random.rangeFloat(0.05F, 0.22F), 0.0F, 1.0F);
    return wear;
}

WorkerMaterialSet generateGuardMaterials(const gdp::core::Seed seed)
{
    constexpr std::array families{
        MaterialFamily::Polymer,
        MaterialFamily::PaintedSteel,
        MaterialFamily::Polycarbonate,
    };
    constexpr std::array primaryColors{
        gdp::core::Vec3{0.13F, 0.22F, 0.27F},
        gdp::core::Vec3{0.22F, 0.28F, 0.20F},
        gdp::core::Vec3{0.28F, 0.21F, 0.17F},
        gdp::core::Vec3{0.20F, 0.20F, 0.22F},
    };
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::core::SeedDomain::EnemyMaterial, 100)};
    WorkerMaterialSet materials;
    materials.family = families[static_cast<std::size_t>(random.rangeInt(
        0, static_cast<int>(families.size() - 1U)))];
    materials.primaryColor = primaryColors[static_cast<std::size_t>(random.rangeInt(
        0, static_cast<int>(primaryColors.size() - 1U)))];
    materials.secondaryColor = materials.primaryColor * random.rangeFloat(0.44F, 0.62F);
    return materials;
}

WearProfile generateOfficerWear(const gdp::core::Seed seed)
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::core::SeedDomain::EnemyWear, 200)};
    const int roll = random.rangeInt(0, 99);
    WearProfile wear;
    if (roll < 28) {
        wear.condition = WearCondition::Clean;
        wear.wear = random.rangeFloat(0.01F, 0.06F);
        wear.dirt = random.rangeFloat(0.01F, 0.05F);
    } else if (roll < 70) {
        wear.condition = WearCondition::Used;
        wear.wear = random.rangeFloat(0.07F, 0.20F);
        wear.dirt = random.rangeFloat(0.05F, 0.16F);
    } else if (roll < 93) {
        wear.condition = WearCondition::Worn;
        wear.wear = random.rangeFloat(0.21F, 0.38F);
        wear.dirt = random.rangeFloat(0.16F, 0.32F);
    } else {
        wear.condition = WearCondition::Damaged;
        wear.wear = random.rangeFloat(0.39F, 0.58F);
        wear.dirt = random.rangeFloat(0.28F, 0.48F);
    }
    wear.scratch = std::clamp(
        wear.wear * random.rangeFloat(0.42F, 0.76F), 0.0F, 1.0F);
    wear.oxidation = std::clamp(
        wear.dirt * random.rangeFloat(0.03F, 0.18F), 0.0F, 1.0F);
    return wear;
}

WorkerMaterialSet generateOfficerMaterials(const gdp::core::Seed seed)
{
    constexpr std::array families{
        MaterialFamily::Fabric,
        MaterialFamily::Polymer,
        MaterialFamily::PaintedSteel,
    };
    constexpr std::array primaryColors{
        gdp::core::Vec3{0.10F, 0.16F, 0.24F},
        gdp::core::Vec3{0.19F, 0.18F, 0.22F},
        gdp::core::Vec3{0.25F, 0.12F, 0.13F},
        gdp::core::Vec3{0.17F, 0.22F, 0.15F},
    };
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::core::SeedDomain::EnemyMaterial, 200)};
    WorkerMaterialSet materials;
    materials.family = families[static_cast<std::size_t>(random.rangeInt(
        0, static_cast<int>(families.size() - 1U)))];
    materials.primaryColor = primaryColors[static_cast<std::size_t>(random.rangeInt(
        0, static_cast<int>(primaryColors.size() - 1U)))];
    materials.secondaryColor = materials.primaryColor * random.rangeFloat(0.48F, 0.68F);
    return materials;
}

void addPartError(
    WorkerVisualValidationResult& result,
    const BodyProfile& body,
    const WorkerPartId id,
    const WorkerPartSlot expectedSlot)
{
    const WorkerPartDefinition* part = findPart(id);
    if (part == nullptr) {
        result.errors.push_back(WorkerVisualValidationError::MissingPart);
        return;
    }
    if (part->slot != expectedSlot) {
        result.errors.push_back(WorkerVisualValidationError::WrongPartSlot);
    }
    const SocketDefinition* socket = findSocket(body, part->attachment);
    if (socket == nullptr) {
        result.errors.push_back(WorkerVisualValidationError::MissingSocket);
    } else if ((part->compatibility & body.compatibility) == 0
        || (part->compatibility & socket->compatibility) == 0) {
        result.errors.push_back(WorkerVisualValidationError::IncompatiblePart);
    }
}

void addGuardPartError(
    GuardVisualValidationResult& result,
    const BodyProfile& body,
    const GuardPartId id,
    const WorkerPartSlot expectedSlot)
{
    const GuardPartDefinition* part = findGuardPart(id);
    if (part == nullptr) {
        result.errors.push_back(GuardVisualValidationError::MissingPart);
        return;
    }
    if (part->slot != expectedSlot) {
        result.errors.push_back(GuardVisualValidationError::WrongPartSlot);
    }
    const SocketDefinition* socket = findSocket(body, part->attachment);
    if (socket == nullptr) {
        result.errors.push_back(GuardVisualValidationError::MissingSocket);
    } else if ((part->compatibility & body.compatibility) == 0
        || (part->compatibility & socket->compatibility) == 0) {
        result.errors.push_back(GuardVisualValidationError::IncompatiblePart);
    }
}

void addOfficerPartError(
    OfficerVisualValidationResult& result,
    const BodyProfile& body,
    const OfficerPartId id,
    const WorkerPartSlot expectedSlot)
{
    const OfficerPartDefinition* part = findOfficerPart(id);
    if (part == nullptr) {
        result.errors.push_back(OfficerVisualValidationError::MissingPart);
        return;
    }
    if (part->slot != expectedSlot) {
        result.errors.push_back(OfficerVisualValidationError::WrongPartSlot);
    }
    const SocketDefinition* socket = findSocket(body, part->attachment);
    if (socket == nullptr) {
        result.errors.push_back(OfficerVisualValidationError::MissingSocket);
    } else if ((part->compatibility & body.compatibility) == 0
        || (part->compatibility & socket->compatibility) == 0) {
        result.errors.push_back(OfficerVisualValidationError::IncompatiblePart);
    }
}

std::uint64_t addFloat(const std::uint64_t hash, const float value) noexcept
{
    return gdp::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
}

} // namespace

bool WorkerVisualValidationResult::isValid() const noexcept
{
    return errors.empty();
}

bool GuardVisualValidationResult::isValid() const noexcept
{
    return errors.empty();
}

bool OfficerVisualValidationResult::isValid() const noexcept
{
    return errors.empty();
}

std::span<const WorkerPartDefinition> workerPartLibrary() noexcept
{
    return parts;
}

WorkerVisual generateWorkerVisual(const gdp::core::Seed visualSeed)
{
    WorkerVisual visual;
    visual.genome.seed = visualSeed;
    gdp::core::RandomStream bodyRandom{gdp::core::deriveSeed(
        visualSeed, gdp::core::SeedDomain::EnemyBody, 0)};
    visual.genome.bodyProfile = static_cast<WorkerBodyProfileId>(bodyRandom.rangeInt(0, 3));
    visual.body = makeBodyProfile(visual.genome.bodyProfile);

    const auto choose = [&](const WorkerPartSlot slot, const std::uint64_t localId) {
        const WorkerPartDefinition* selected = selectPart(slot, visual.body, visualSeed, localId);
        if (selected != nullptr) {
            return selected;
        }
        ++visual.fallbackCount;
        for (const WorkerPartDefinition& part : parts) {
            if (part.slot == slot) {
                return &part;
            }
        }
        return selected;
    };

    const WorkerPartDefinition* torso = choose(WorkerPartSlot::Torso, 0);
    const WorkerPartDefinition* head = choose(WorkerPartSlot::Head, 1);
    const WorkerPartDefinition* leftArm = choose(WorkerPartSlot::Arm, 2);
    const WorkerPartDefinition* rightArm = choose(WorkerPartSlot::Arm, 3);
    const WorkerPartDefinition* leftLeg = choose(WorkerPartSlot::Leg, 4);
    const WorkerPartDefinition* rightLeg = choose(WorkerPartSlot::Leg, 5);
    const WorkerPartDefinition* back = choose(WorkerPartSlot::Back, 6);
    const WorkerPartDefinition* tool = choose(WorkerPartSlot::Tool, 7);
    visual.genome.torso = torso->id;
    visual.genome.head = head->id;
    visual.genome.leftArm = leftArm->id;
    visual.genome.rightArm = rightArm->id;
    visual.genome.leftLeg = leftLeg->id;
    visual.genome.rightLeg = rightLeg->id;
    visual.genome.back = back->id;
    visual.genome.tool = tool->id;

    constexpr float baseHeight = 1.80F;
    const float height = baseHeight * visual.body.heightScale;
    const float legHeight = height * visual.body.legLength;
    const float headHeight = height * visual.body.headScale;
    const float torsoHeight = height - legHeight - headHeight;
    const float torsoWidth = height * visual.body.torsoWidth;
    const float torsoDepth = height * visual.body.torsoDepth;
    const float armHeight = height * visual.body.armLength;
    const float armWidth = height * 0.13F;
    const float legWidth = torsoWidth * 0.34F;
    const SocketDefinition* leftShoulder = findSocket(visual.body, SocketType::ShoulderLeft);
    const SocketDefinition* rightShoulder = findSocket(visual.body, SocketType::ShoulderRight);
    const SocketDefinition* leftHip = findSocket(visual.body, SocketType::HipLeft);
    const SocketDefinition* rightHip = findSocket(visual.body, SocketType::HipRight);
    const SocketDefinition* backSocket = findSocket(visual.body, SocketType::Back);
    const SocketDefinition* toolSocket = findSocket(visual.body, SocketType::Tool);

    visual.meshRecipe.seed = visualSeed;
    visual.meshRecipe.primitives = {
        makePrimitive(*torso, {0.0F, legHeight + (torsoHeight * 0.5F), 0.0F},
            {torsoWidth, torsoHeight, torsoDepth}, {visual.body.forwardLean, 0.0F, 0.0F}),
        makePrimitive(*head, {0.0F, legHeight + torsoHeight + (headHeight * 0.5F), 0.0F},
            {headHeight, headHeight, headHeight}),
        makePrimitive(*leftArm,
            {leftShoulder->position.x, leftShoulder->position.y - (armHeight * 0.5F), 0.0F},
            {armWidth * visual.body.leftMassBias, armHeight, armWidth}),
        makePrimitive(*rightArm,
            {rightShoulder->position.x, rightShoulder->position.y - (armHeight * 0.5F), 0.0F},
            {armWidth * visual.body.rightMassBias, armHeight, armWidth}),
        makePrimitive(*leftLeg, {leftHip->position.x, legHeight * 0.5F, 0.0F},
            {legWidth * visual.body.leftMassBias, legHeight, torsoDepth * 0.72F}),
        makePrimitive(*rightLeg, {rightHip->position.x, legHeight * 0.5F, 0.0F},
            {legWidth * visual.body.rightMassBias, legHeight, torsoDepth * 0.72F}),
        makePrimitive(*back, backSocket->position,
            {height, height, height}),
        makePrimitive(*tool,
            toolSocket->position + gdp::core::Vec3{armWidth * 0.55F, 0.0F, 0.0F},
            {height, height, height},
            {0.0F, 0.0F, -8.0F}),
    };

    visual.materials = generateMaterials(visualSeed);
    visual.wear = generateWear(visualSeed);
    visual.textureRecipe = {
        .version = 1,
        .seed = gdp::core::deriveSeed(
            visualSeed, gdp::core::SeedDomain::EnemyMaterial, 1),
        .family = visual.materials.family,
        .baseColor = visual.materials.primaryColor,
        .secondaryColor = visual.materials.secondaryColor,
        .macroVariation = 0.24F,
        .microVariation = 0.16F,
        .wearAmount = visual.wear.wear,
        .dirtAmount = visual.wear.dirt,
        .scratchAmount = visual.wear.scratch,
        .oxidationAmount = materialProperties(visual.materials.family).oxidationAllowed
            ? visual.wear.oxidation : 0.0F,
        .emissiveAmount = 0.0F,
        .region = SemanticRegion::ArmorPlate,
        .resolution = 64,
    };
    visual.genome.visualHash = workerVisualHash(visual);
    return visual;
}

WorkerVisualValidationResult validateWorkerVisual(const WorkerVisual& visual)
{
    WorkerVisualValidationResult result;
    if (visual.body.id >= WorkerBodyProfileId::Count
        || visual.body.id != visual.genome.bodyProfile) {
        result.errors.push_back(WorkerVisualValidationError::InvalidBodyProfile);
    }
    constexpr std::array requiredSockets{
        SocketType::Root,
        SocketType::Neck,
        SocketType::ShoulderLeft,
        SocketType::ShoulderRight,
        SocketType::HipLeft,
        SocketType::HipRight,
        SocketType::Back,
        SocketType::Tool,
    };
    for (const SocketType socket : requiredSockets) {
        if (findSocket(visual.body, socket) == nullptr) {
            result.errors.push_back(WorkerVisualValidationError::MissingSocket);
        }
    }
    addPartError(result, visual.body, visual.genome.torso, WorkerPartSlot::Torso);
    addPartError(result, visual.body, visual.genome.head, WorkerPartSlot::Head);
    addPartError(result, visual.body, visual.genome.leftArm, WorkerPartSlot::Arm);
    addPartError(result, visual.body, visual.genome.rightArm, WorkerPartSlot::Arm);
    addPartError(result, visual.body, visual.genome.leftLeg, WorkerPartSlot::Leg);
    addPartError(result, visual.body, visual.genome.rightLeg, WorkerPartSlot::Leg);
    addPartError(result, visual.body, visual.genome.back, WorkerPartSlot::Back);
    addPartError(result, visual.body, visual.genome.tool, WorkerPartSlot::Tool);
    if (!validateMeshRecipe(visual.meshRecipe).isValid()) {
        result.errors.push_back(WorkerVisualValidationError::InvalidMeshRecipe);
    }
    if (!validateTextureRecipe(visual.textureRecipe).isValid()) {
        result.errors.push_back(WorkerVisualValidationError::InvalidTextureRecipe);
    }
    if (const auto generated = generateMesh(visual.meshRecipe)) {
        if (generated->mesh.bounds.minimum.y < -0.001F
            || generated->mesh.bounds.maximum.y > 2.05F) {
            result.errors.push_back(WorkerVisualValidationError::InvalidBounds);
        }
    } else {
        result.errors.push_back(WorkerVisualValidationError::InvalidBounds);
    }
    if (visual.genome.visualHash == 0
        || visual.genome.visualHash != workerVisualHash(visual)) {
        result.errors.push_back(WorkerVisualValidationError::InvalidVisualHash);
    }
    return result;
}

std::uint64_t workerVisualHash(const WorkerVisual& visual) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x574F524B45525F56ULL, visual.genome.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(visual.genome.bodyProfile));
    hash = gdp::core::hashCombine(hash, visual.genome.torso);
    hash = gdp::core::hashCombine(hash, visual.genome.head);
    hash = gdp::core::hashCombine(hash, visual.genome.leftArm);
    hash = gdp::core::hashCombine(hash, visual.genome.rightArm);
    hash = gdp::core::hashCombine(hash, visual.genome.leftLeg);
    hash = gdp::core::hashCombine(hash, visual.genome.rightLeg);
    hash = gdp::core::hashCombine(hash, visual.genome.back);
    hash = gdp::core::hashCombine(hash, visual.genome.tool);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(visual.materials.family));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(visual.wear.condition));
    hash = addFloat(hash, visual.wear.wear);
    hash = addFloat(hash, visual.wear.dirt);
    hash = addFloat(hash, visual.wear.scratch);
    hash = addFloat(hash, visual.wear.oxidation);
    hash = gdp::core::hashCombine(hash, meshRecipeHash(visual.meshRecipe));
    return gdp::core::hashCombine(hash, textureRecipeHash(visual.textureRecipe));
}

std::span<const GuardPartDefinition> guardPartLibrary() noexcept
{
    return guardParts;
}

GuardVisual generateGuardVisual(const gdp::core::Seed visualSeed)
{
    GuardVisual visual;
    visual.genome.seed = visualSeed;
    gdp::core::RandomStream bodyRandom{gdp::core::deriveSeed(
        visualSeed, gdp::core::SeedDomain::EnemyBody, 100)};
    visual.genome.bodyProfile = static_cast<GuardBodyProfileId>(bodyRandom.rangeInt(0, 2));
    visual.body = makeBodyProfile(guardBaseProfile(visual.genome.bodyProfile));

    const auto choose = [&](const WorkerPartSlot slot, const std::uint64_t localId) {
        const GuardPartDefinition* selected
            = selectGuardPart(slot, visual.body, visualSeed, localId);
        if (selected != nullptr) {
            return selected;
        }
        ++visual.fallbackCount;
        for (const GuardPartDefinition& part : guardParts) {
            if (part.slot == slot) {
                return &part;
            }
        }
        return selected;
    };

    const GuardPartDefinition* torso = choose(WorkerPartSlot::Torso, 100);
    const GuardPartDefinition* head = choose(WorkerPartSlot::Head, 101);
    const GuardPartDefinition* leftArm = choose(WorkerPartSlot::Arm, 102);
    const GuardPartDefinition* rightArm = choose(WorkerPartSlot::Arm, 103);
    const GuardPartDefinition* leftLeg = choose(WorkerPartSlot::Leg, 104);
    const GuardPartDefinition* rightLeg = choose(WorkerPartSlot::Leg, 105);
    const GuardPartDefinition* back = choose(WorkerPartSlot::Back, 106);
    const GuardPartDefinition* weapon = choose(WorkerPartSlot::Tool, 107);
    visual.genome.torso = torso->id;
    visual.genome.head = head->id;
    visual.genome.leftArm = leftArm->id;
    visual.genome.rightArm = rightArm->id;
    visual.genome.leftLeg = leftLeg->id;
    visual.genome.rightLeg = rightLeg->id;
    visual.genome.back = back->id;
    visual.genome.weapon = weapon->id;

    constexpr float baseHeight = 1.80F;
    const float height = baseHeight * visual.body.heightScale;
    const float legHeight = height * visual.body.legLength;
    const float headHeight = height * visual.body.headScale;
    const float torsoHeight = height - legHeight - headHeight;
    const float torsoWidth = height * visual.body.torsoWidth;
    const float torsoDepth = height * visual.body.torsoDepth;
    const float armHeight = height * visual.body.armLength;
    const float armWidth = height * 0.13F;
    const float legWidth = torsoWidth * 0.34F;
    const SocketDefinition* leftShoulder = findSocket(visual.body, SocketType::ShoulderLeft);
    const SocketDefinition* rightShoulder = findSocket(visual.body, SocketType::ShoulderRight);
    const SocketDefinition* leftHip = findSocket(visual.body, SocketType::HipLeft);
    const SocketDefinition* rightHip = findSocket(visual.body, SocketType::HipRight);
    const SocketDefinition* backSocket = findSocket(visual.body, SocketType::Back);

    visual.meshRecipe.seed = gdp::core::deriveSeed(
        visualSeed, gdp::core::SeedDomain::EnemyParts, 199);
    visual.meshRecipe.primitives = {
        makePrimitive(*torso, {0.0F, legHeight + (torsoHeight * 0.5F), 0.0F},
            {torsoWidth, torsoHeight, torsoDepth}),
        makePrimitive(*head, {0.0F, legHeight + torsoHeight + (headHeight * 0.5F), 0.0F},
            {headHeight, headHeight, headHeight}),
        makePrimitive(*leftArm,
            {leftShoulder->position.x, leftShoulder->position.y - (armHeight * 0.5F), 0.05F},
            {armWidth * visual.body.leftMassBias, armHeight, armWidth},
            {0.0F, 0.0F, -5.0F}),
        makePrimitive(*rightArm,
            {rightShoulder->position.x,
                rightShoulder->position.y - (armHeight * 0.5F), 0.05F},
            {armWidth * visual.body.rightMassBias, armHeight, armWidth},
            {0.0F, 0.0F, 5.0F}),
        makePrimitive(*leftLeg, {leftHip->position.x, legHeight * 0.5F, 0.0F},
            {legWidth * visual.body.leftMassBias, legHeight, torsoDepth * 0.72F}),
        makePrimitive(*rightLeg, {rightHip->position.x, legHeight * 0.5F, 0.0F},
            {legWidth * visual.body.rightMassBias, legHeight, torsoDepth * 0.72F}),
        makePrimitive(*back, backSocket->position, {height, height, height}),
        makePrimitive(*weapon,
            {0.0F, rightShoulder->position.y - (armHeight * 0.72F), torsoDepth * 0.72F},
            {height, height, height},
            {0.0F, 0.0F, -4.0F}),
    };

    visual.materials = generateGuardMaterials(visualSeed);
    visual.wear = generateGuardWear(visualSeed);
    visual.textureRecipe = {
        .version = 1,
        .seed = gdp::core::deriveSeed(
            visualSeed, gdp::core::SeedDomain::EnemyMaterial, 101),
        .family = visual.materials.family,
        .baseColor = visual.materials.primaryColor,
        .secondaryColor = visual.materials.secondaryColor,
        .macroVariation = 0.16F,
        .microVariation = 0.10F,
        .wearAmount = visual.wear.wear,
        .dirtAmount = visual.wear.dirt,
        .scratchAmount = visual.wear.scratch,
        .oxidationAmount = materialProperties(visual.materials.family).oxidationAllowed
            ? visual.wear.oxidation : 0.0F,
        .emissiveAmount = 0.0F,
        .region = SemanticRegion::ArmorPlate,
        .resolution = 64,
    };
    visual.genome.visualHash = guardVisualHash(visual);
    return visual;
}

GuardVisualValidationResult validateGuardVisual(const GuardVisual& visual)
{
    GuardVisualValidationResult result;
    if (visual.genome.bodyProfile >= GuardBodyProfileId::Count
        || visual.body.id != guardBaseProfile(visual.genome.bodyProfile)) {
        result.errors.push_back(GuardVisualValidationError::InvalidBodyProfile);
    }
    constexpr std::array requiredSockets{
        SocketType::Root,
        SocketType::Neck,
        SocketType::ShoulderLeft,
        SocketType::ShoulderRight,
        SocketType::HipLeft,
        SocketType::HipRight,
        SocketType::Back,
        SocketType::Tool,
    };
    for (const SocketType socket : requiredSockets) {
        if (findSocket(visual.body, socket) == nullptr) {
            result.errors.push_back(GuardVisualValidationError::MissingSocket);
        }
    }
    addGuardPartError(result, visual.body, visual.genome.torso, WorkerPartSlot::Torso);
    addGuardPartError(result, visual.body, visual.genome.head, WorkerPartSlot::Head);
    addGuardPartError(result, visual.body, visual.genome.leftArm, WorkerPartSlot::Arm);
    addGuardPartError(result, visual.body, visual.genome.rightArm, WorkerPartSlot::Arm);
    addGuardPartError(result, visual.body, visual.genome.leftLeg, WorkerPartSlot::Leg);
    addGuardPartError(result, visual.body, visual.genome.rightLeg, WorkerPartSlot::Leg);
    addGuardPartError(result, visual.body, visual.genome.back, WorkerPartSlot::Back);
    addGuardPartError(result, visual.body, visual.genome.weapon, WorkerPartSlot::Tool);
    const GuardPartDefinition* weapon = findGuardPart(visual.genome.weapon);
    if (weapon == nullptr || weapon->semantic != SemanticRegion::Weapon
        || weapon->dimensionScale.x <= weapon->dimensionScale.y * 3.0F) {
        result.errors.push_back(GuardVisualValidationError::InvalidWeapon);
    }
    if (!validateMeshRecipe(visual.meshRecipe).isValid()) {
        result.errors.push_back(GuardVisualValidationError::InvalidMeshRecipe);
    }
    if (!validateTextureRecipe(visual.textureRecipe).isValid()) {
        result.errors.push_back(GuardVisualValidationError::InvalidTextureRecipe);
    }
    if (const auto generated = generateMesh(visual.meshRecipe)) {
        if (generated->mesh.bounds.minimum.y < -0.001F
            || generated->mesh.bounds.maximum.y > 2.05F) {
            result.errors.push_back(GuardVisualValidationError::InvalidBounds);
        }
    } else {
        result.errors.push_back(GuardVisualValidationError::InvalidBounds);
    }
    if (visual.genome.visualHash == 0U
        || visual.genome.visualHash != guardVisualHash(visual)) {
        result.errors.push_back(GuardVisualValidationError::InvalidVisualHash);
    }
    return result;
}

std::uint64_t guardVisualHash(const GuardVisual& visual) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x47554152445F5653ULL, visual.genome.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(visual.genome.bodyProfile));
    hash = gdp::core::hashCombine(hash, visual.genome.torso);
    hash = gdp::core::hashCombine(hash, visual.genome.head);
    hash = gdp::core::hashCombine(hash, visual.genome.leftArm);
    hash = gdp::core::hashCombine(hash, visual.genome.rightArm);
    hash = gdp::core::hashCombine(hash, visual.genome.leftLeg);
    hash = gdp::core::hashCombine(hash, visual.genome.rightLeg);
    hash = gdp::core::hashCombine(hash, visual.genome.back);
    hash = gdp::core::hashCombine(hash, visual.genome.weapon);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(visual.materials.family));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(visual.wear.condition));
    hash = addFloat(hash, visual.wear.wear);
    hash = addFloat(hash, visual.wear.dirt);
    hash = addFloat(hash, visual.wear.scratch);
    hash = addFloat(hash, visual.wear.oxidation);
    hash = gdp::core::hashCombine(hash, meshRecipeHash(visual.meshRecipe));
    return gdp::core::hashCombine(hash, textureRecipeHash(visual.textureRecipe));
}

std::span<const OfficerPartDefinition> officerPartLibrary() noexcept
{
    return officerParts;
}

OfficerVisual generateOfficerVisual(const gdp::core::Seed visualSeed)
{
    OfficerVisual visual;
    visual.genome.seed = visualSeed;
    gdp::core::RandomStream bodyRandom{gdp::core::deriveSeed(
        visualSeed, gdp::core::SeedDomain::EnemyBody, 200)};
    visual.genome.bodyProfile
        = static_cast<OfficerBodyProfileId>(bodyRandom.rangeInt(0, 2));
    visual.body = makeBodyProfile(officerBaseProfile(visual.genome.bodyProfile));

    const auto choose = [&](const WorkerPartSlot slot, const std::uint64_t localId) {
        const OfficerPartDefinition* selected
            = selectOfficerPart(slot, visual.body, visualSeed, localId);
        if (selected != nullptr) {
            return selected;
        }
        ++visual.fallbackCount;
        for (const OfficerPartDefinition& part : officerParts) {
            if (part.slot == slot) {
                return &part;
            }
        }
        return selected;
    };

    const OfficerPartDefinition* torso = choose(WorkerPartSlot::Torso, 200);
    const OfficerPartDefinition* head = choose(WorkerPartSlot::Head, 201);
    const OfficerPartDefinition* leftArm = choose(WorkerPartSlot::Arm, 202);
    const OfficerPartDefinition* rightArm = choose(WorkerPartSlot::Arm, 203);
    const OfficerPartDefinition* leftLeg = choose(WorkerPartSlot::Leg, 204);
    const OfficerPartDefinition* rightLeg = choose(WorkerPartSlot::Leg, 205);
    const OfficerPartDefinition* commandModule = choose(WorkerPartSlot::Back, 206);
    const OfficerPartDefinition* sidearm = choose(WorkerPartSlot::Tool, 207);
    visual.genome.torso = torso->id;
    visual.genome.head = head->id;
    visual.genome.leftArm = leftArm->id;
    visual.genome.rightArm = rightArm->id;
    visual.genome.leftLeg = leftLeg->id;
    visual.genome.rightLeg = rightLeg->id;
    visual.genome.commandModule = commandModule->id;
    visual.genome.sidearm = sidearm->id;

    constexpr float baseHeight = 1.80F;
    const float height = baseHeight * visual.body.heightScale;
    const float legHeight = height * visual.body.legLength;
    const float headHeight = height * visual.body.headScale;
    const float torsoHeight = height - legHeight - headHeight;
    const float torsoWidth = height * visual.body.torsoWidth;
    const float torsoDepth = height * visual.body.torsoDepth;
    const float armHeight = height * visual.body.armLength;
    const float armWidth = height * 0.12F;
    const float legWidth = torsoWidth * 0.32F;
    const SocketDefinition* leftShoulder = findSocket(visual.body, SocketType::ShoulderLeft);
    const SocketDefinition* rightShoulder = findSocket(visual.body, SocketType::ShoulderRight);
    const SocketDefinition* leftHip = findSocket(visual.body, SocketType::HipLeft);
    const SocketDefinition* rightHip = findSocket(visual.body, SocketType::HipRight);
    const SocketDefinition* backSocket = findSocket(visual.body, SocketType::Back);

    visual.meshRecipe.seed = gdp::core::deriveSeed(
        visualSeed, gdp::core::SeedDomain::EnemyParts, 299);
    visual.meshRecipe.primitives = {
        makePrimitive(*torso, {0.0F, legHeight + (torsoHeight * 0.5F), 0.0F},
            {torsoWidth, torsoHeight, torsoDepth}),
        makePrimitive(*head, {0.0F, legHeight + torsoHeight + (headHeight * 0.5F), 0.0F},
            {headHeight, headHeight, headHeight}),
        makePrimitive(*leftArm,
            {leftShoulder->position.x, leftShoulder->position.y - (armHeight * 0.5F), 0.0F},
            {armWidth * visual.body.leftMassBias, armHeight, armWidth},
            {0.0F, 0.0F, -3.0F}),
        makePrimitive(*rightArm,
            {rightShoulder->position.x,
                rightShoulder->position.y - (armHeight * 0.5F), 0.04F},
            {armWidth * visual.body.rightMassBias, armHeight, armWidth},
            {0.0F, 0.0F, 8.0F}),
        makePrimitive(*leftLeg, {leftHip->position.x, legHeight * 0.5F, 0.0F},
            {legWidth * visual.body.leftMassBias, legHeight, torsoDepth * 0.68F}),
        makePrimitive(*rightLeg, {rightHip->position.x, legHeight * 0.5F, 0.0F},
            {legWidth * visual.body.rightMassBias, legHeight, torsoDepth * 0.68F}),
        makePrimitive(*commandModule, backSocket->position, {height, height, height}),
        makePrimitive(*sidearm,
            {rightShoulder->position.x + (armWidth * 0.25F),
                rightShoulder->position.y - (armHeight * 0.82F), torsoDepth * 0.58F},
            {height, height, height}, {0.0F, 0.0F, -8.0F}),
    };

    visual.materials = generateOfficerMaterials(visualSeed);
    visual.wear = generateOfficerWear(visualSeed);
    visual.textureRecipe = {
        .version = 1,
        .seed = gdp::core::deriveSeed(
            visualSeed, gdp::core::SeedDomain::EnemyMaterial, 201),
        .family = visual.materials.family,
        .baseColor = visual.materials.primaryColor,
        .secondaryColor = visual.materials.secondaryColor,
        .macroVariation = 0.12F,
        .microVariation = 0.10F,
        .wearAmount = visual.wear.wear,
        .dirtAmount = visual.wear.dirt,
        .scratchAmount = visual.wear.scratch,
        .oxidationAmount = materialProperties(visual.materials.family).oxidationAllowed
            ? visual.wear.oxidation : 0.0F,
        .emissiveAmount = 0.02F,
        .region = SemanticRegion::ArmorPlate,
        .resolution = 64,
    };
    visual.genome.visualHash = officerVisualHash(visual);
    return visual;
}

OfficerVisualValidationResult validateOfficerVisual(const OfficerVisual& visual)
{
    OfficerVisualValidationResult result;
    if (visual.genome.bodyProfile >= OfficerBodyProfileId::Count
        || visual.body.id != officerBaseProfile(visual.genome.bodyProfile)) {
        result.errors.push_back(OfficerVisualValidationError::InvalidBodyProfile);
    }
    constexpr std::array requiredSockets{
        SocketType::Root,
        SocketType::Neck,
        SocketType::ShoulderLeft,
        SocketType::ShoulderRight,
        SocketType::HipLeft,
        SocketType::HipRight,
        SocketType::Back,
        SocketType::Tool,
    };
    for (const SocketType socket : requiredSockets) {
        if (findSocket(visual.body, socket) == nullptr) {
            result.errors.push_back(OfficerVisualValidationError::MissingSocket);
        }
    }
    addOfficerPartError(result, visual.body, visual.genome.torso, WorkerPartSlot::Torso);
    addOfficerPartError(result, visual.body, visual.genome.head, WorkerPartSlot::Head);
    addOfficerPartError(result, visual.body, visual.genome.leftArm, WorkerPartSlot::Arm);
    addOfficerPartError(result, visual.body, visual.genome.rightArm, WorkerPartSlot::Arm);
    addOfficerPartError(result, visual.body, visual.genome.leftLeg, WorkerPartSlot::Leg);
    addOfficerPartError(result, visual.body, visual.genome.rightLeg, WorkerPartSlot::Leg);
    addOfficerPartError(
        result, visual.body, visual.genome.commandModule, WorkerPartSlot::Back);
    addOfficerPartError(result, visual.body, visual.genome.sidearm, WorkerPartSlot::Tool);
    const OfficerPartDefinition* commandModule
        = findOfficerPart(visual.genome.commandModule);
    if (commandModule == nullptr
        || commandModule->semantic != SemanticRegion::MechanicalCore
        || commandModule->dimensionScale.y <= commandModule->dimensionScale.x) {
        result.errors.push_back(OfficerVisualValidationError::InvalidCommandModule);
    }
    const OfficerPartDefinition* sidearm = findOfficerPart(visual.genome.sidearm);
    if (sidearm == nullptr || sidearm->semantic != SemanticRegion::Weapon
        || sidearm->dimensionScale.x <= sidearm->dimensionScale.y * 2.0F
        || sidearm->dimensionScale.x >= sidearm->dimensionScale.y * 4.0F) {
        result.errors.push_back(OfficerVisualValidationError::InvalidSidearm);
    }
    if (!validateMeshRecipe(visual.meshRecipe).isValid()) {
        result.errors.push_back(OfficerVisualValidationError::InvalidMeshRecipe);
    }
    if (!validateTextureRecipe(visual.textureRecipe).isValid()) {
        result.errors.push_back(OfficerVisualValidationError::InvalidTextureRecipe);
    }
    if (const auto generated = generateMesh(visual.meshRecipe)) {
        if (generated->mesh.bounds.minimum.y < -0.001F
            || generated->mesh.bounds.maximum.y > 2.05F) {
            result.errors.push_back(OfficerVisualValidationError::InvalidBounds);
        }
    } else {
        result.errors.push_back(OfficerVisualValidationError::InvalidBounds);
    }
    if (visual.genome.visualHash == 0U
        || visual.genome.visualHash != officerVisualHash(visual)) {
        result.errors.push_back(OfficerVisualValidationError::InvalidVisualHash);
    }
    return result;
}

std::uint64_t officerVisualHash(const OfficerVisual& visual) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x4F46464943455256ULL, visual.genome.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(visual.genome.bodyProfile));
    hash = gdp::core::hashCombine(hash, visual.genome.torso);
    hash = gdp::core::hashCombine(hash, visual.genome.head);
    hash = gdp::core::hashCombine(hash, visual.genome.leftArm);
    hash = gdp::core::hashCombine(hash, visual.genome.rightArm);
    hash = gdp::core::hashCombine(hash, visual.genome.leftLeg);
    hash = gdp::core::hashCombine(hash, visual.genome.rightLeg);
    hash = gdp::core::hashCombine(hash, visual.genome.commandModule);
    hash = gdp::core::hashCombine(hash, visual.genome.sidearm);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(visual.materials.family));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(visual.wear.condition));
    hash = addFloat(hash, visual.wear.wear);
    hash = addFloat(hash, visual.wear.dirt);
    hash = addFloat(hash, visual.wear.scratch);
    hash = addFloat(hash, visual.wear.oxidation);
    hash = gdp::core::hashCombine(hash, meshRecipeHash(visual.meshRecipe));
    return gdp::core::hashCombine(hash, textureRecipeHash(visual.textureRecipe));
}

} // namespace gdp::procedural
