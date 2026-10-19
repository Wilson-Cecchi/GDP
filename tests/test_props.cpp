#include "gameplay/Combat.hpp"
#include "gameplay/Props.hpp"
#include "world/PropGenerator.hpp"
#include "world/WorldGenerator.hpp"

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
    const gdp::world::PropValidationResult& result,
    const gdp::world::PropValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

void testDeterministicPropLayout()
{
    using namespace gdp::world;
    constexpr std::array zones{
        SpawnZone{{{1.5F, 0.0F, -3.0F}, {3.0F, 0.0F, 1.5F}}},
    };
    constexpr wkc::core::Seed seed = 0x50524F505F563031ULL;
    const PropLayout first = generatePropLayout(
        seed, RoomRole::DeadEnd, RoomTheme::Storage, 4, zones);
    const PropLayout repeated = generatePropLayout(
        seed, RoomRole::DeadEnd, RoomTheme::Storage, 4, zones);
    const PropLayout different = generatePropLayout(
        seed + 1U, RoomRole::DeadEnd, RoomTheme::Storage, 4, zones);
    expect(validatePropLayout(first, zones).isValid(),
        "representative procedural prop layout validates");
    expect(first.props == repeated.props && first.layoutHash == repeated.layoutHash,
        "same room inputs reproduce prop type, state, dimensions and placement");
    expect(first.layoutHash != different.layoutHash,
        "different room seed changes the canonical prop layout");
    constexpr std::uint64_t goldenHash = 0x28BB5CB31BBAF612ULL;
    expect(first.layoutHash == goldenHash,
        "golden procedural prop hash remains stable across builds");
    expect(first.props.size() == 3U
            && first.props.front().type == gdp::entities::PropType::Crate,
        "DeadEnd spends its full budget and guarantees a destructible crate");

    PropLayout invalid = first;
    invalid.props.front().position = {99.0F, 0.0F, 99.0F};
    expect(containsError(
               validatePropLayout(invalid, zones), PropValidationError::OutsideZone),
        "prop validation rejects footprints outside authored zones");
    invalid = first;
    invalid.spent = 0U;
    expect(containsError(
               validatePropLayout(invalid, zones), PropValidationError::BudgetMismatch),
        "prop validation rejects inconsistent budget accounting");
    invalid = first;
    invalid.layoutHash ^= 1U;
    expect(containsError(
               validatePropLayout(invalid, zones), PropValidationError::InvalidLayoutHash),
        "prop validation detects canonical hash corruption");
}

void testPropSeedBatch()
{
    using namespace gdp::world;
    constexpr std::array zones{
        SpawnZone{{{1.5F, 0.0F, -3.0F}, {3.0F, 0.0F, 1.5F}}},
    };
    std::unordered_set<std::uint64_t> hashes;
    bool allValid = true;
    bool foundCrate = false;
    bool foundCanister = false;
    bool foundBarricade = false;
    for (wkc::core::Seed seed = 0; seed < 1'000; ++seed) {
        const RoomTheme theme = static_cast<RoomTheme>(seed % 7U);
        const PropLayout layout = generatePropLayout(
            seed, RoomRole::Normal, theme, 4, zones);
        const PropValidationResult validation = validatePropLayout(layout, zones);
        if (!validation.isValid()) {
            allValid = false;
            std::cerr << "Prop generation failed at room seed " << seed << '\n';
            for (const PropValidationError error : validation.errors) {
                std::cerr << "  validation error " << static_cast<int>(error) << '\n';
            }
            break;
        }
        hashes.insert(layout.layoutHash);
        for (const PropSpawn& prop : layout.props) {
            foundCrate = foundCrate || prop.type == gdp::entities::PropType::Crate;
            foundCanister = foundCanister
                || prop.type == gdp::entities::PropType::Canister;
            foundBarricade = foundBarricade
                || prop.type == gdp::entities::PropType::Barricade;
        }
    }
    expect(allValid, "1,000 room seeds pass prop budget, footprint and spacing gates");
    expect(hashes.size() == 1'000U,
        "1,000 room seeds produce unique canonical prop hashes");
    expect(foundCrate && foundCanister && foundBarricade,
        "seed batch reaches all three functional prop categories");

    const PropLayout missingZone = generatePropLayout(
        12, RoomRole::Normal, RoomTheme::Industrial, 2, {});
    expect(containsError(
               validatePropLayout(missingZone, {}), PropValidationError::MissingZone),
        "positive prop budget requires an authored candidate zone");

    for (const wkc::core::Seed worldSeed : {wkc::core::Seed{12345},
             wkc::core::Seed{0x4744505F574F524CULL}}) {
        const WorldGenerationResult generated = generateWorld(worldSeed);
        bool worldPropsValid = generated.world.has_value();
        if (generated.world) {
            for (const RoomInstance& room : generated.world->rooms) {
                const auto node = std::find_if(
                    generated.world->graph.nodes.begin(), generated.world->graph.nodes.end(),
                    [&room](const RoomNode& candidate) { return candidate.id == room.id; });
                worldPropsValid = worldPropsValid && node != generated.world->graph.nodes.end();
                if (node != generated.world->graph.nodes.end()) {
                    const PropLayout layout = generatePropLayout(
                        room.seed, room.role, room.theme, node->depth, room.propZones);
                    const PropValidationResult validation
                        = validatePropLayout(layout, room.propZones);
                    if (!validation.isValid()) {
                        std::cerr << "World prop layout failed for seed " << worldSeed
                                  << " room " << room.id << '\n';
                        for (const PropValidationError error : validation.errors) {
                            std::cerr << "  validation error "
                                      << static_cast<int>(error) << '\n';
                        }
                    }
                    worldPropsValid = worldPropsValid && validation.isValid();
                }
            }
        }
        expect(worldPropsValid,
            "default and documented world seeds produce valid room prop layouts");
    }
}

void testPropDamageCollisionAndDrops()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    EntityManager manager;
    const EntityId crate = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, -2.0F}, .scale = {0.8F, 0.8F, 0.8F}},
        .roomId = 4,
        .type = PropType::Crate,
        .solid = true,
        .health = 60.0F,
        .maximumHealth = 60.0F,
        .dropType = ItemType::Health,
        .dropQuantity = 20U,
        .visualSeed = 99,
    });
    const CollisionWorld base{{{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    expect(collisionWorldWithProps(base, manager, 4).solids().size() == 1U,
        "intact solid prop contributes collision in its active room");
    const EntityId neighboringCover = manager.createProp({
        .transform = {.position = {2.0F, 0.0F, -2.0F},
            .scale = {1.0F, 2.0F, 1.0F}},
        .roomId = 5,
        .type = PropType::Barricade,
        .solid = true,
        .health = 100.0F,
        .maximumHealth = 100.0F,
    });
    const std::array neighboringRoom{5U};
    expect(collisionWorldWithProps(base, manager, 4).solids().size() == 1U
            && collisionWorldWithProps(base, manager, 4, neighboringRoom)
                .solids().size() == 2U,
        "cross-room enemy navigation includes cover from a connected room only");
    (void)manager.destroy(neighboringCover);
    (void)manager.flushDestroyed();

    const PropDamageResult damaged = damageProp(manager, crate, 34.0F);
    const PropData* data = manager.findData<PropData>(crate);
    expect(damaged.outcome == PropDamageOutcome::Damaged && data != nullptr
            && data->state == PropState::Damaged && data->health == 26.0F,
        "non-lethal prop damage enters Damaged state at half health");
    expect(repairProp(manager, crate, 60.0F)
            && manager.findData<PropData>(crate)->state == PropState::Intact
            && manager.findData<PropData>(crate)->health == 60.0F,
        "Engineer-compatible repair restores prop health and Intact state");

    const PropDamageResult destroyed = damageProp(manager, crate, 80.0F);
    data = manager.findData<PropData>(crate);
    expect(destroyed.outcome == PropDamageOutcome::Destroyed
            && destroyed.spawnedDrop.isValid() && data != nullptr
            && data->state == PropState::Destroyed && !data->solid,
        "lethal prop damage destroys collision and creates its controlled drop");
    const ItemData* drop = manager.findData<ItemData>(destroyed.spawnedDrop);
    expect(drop != nullptr && drop->type == ItemType::Health && drop->quantity == 20U,
        "destroyed crate preserves deterministic drop type and quantity");
    const std::size_t itemCount = manager.query(EntityKind::Item).size();
    expect(damageProp(manager, crate, 1.0F).outcome == PropDamageOutcome::Ignored
            && manager.query(EntityKind::Item).size() == itemCount,
        "destroyed prop ignores repeated damage and cannot duplicate its drop");
    expect(collisionWorldWithProps(base, manager, 4).solids().empty(),
        "destroyed prop is removed from the rebuilt collision world");
}

void testHitscanPropPriority()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    EntityManager manager;
    const EntityId crate = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, -2.0F}, .scale = {0.8F, 0.8F, 0.8F}},
        .roomId = 7,
        .type = PropType::Crate,
        .solid = true,
        .health = 60.0F,
        .maximumHealth = 60.0F,
        .dropType = ItemType::Ammo,
        .dropQuantity = 8U,
    });
    const EntityId enemy = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -4.0F}},
        .roomId = 7,
        .health = 100.0F,
    });
    const CollisionWorld base{{{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    CollisionWorld collision = collisionWorldWithProps(base, manager, 7);
    Weapon weapon({.damage = 70.0F, .fireInterval = 0.0F, .range = 10.0F});
    // Aim through the crate and a visible leg, rather than the gap between
    // the segmented legs of the accepted enemy model.
    const ShotResult first = fireHitscan(
        weapon, {0.2F, 0.4F, 0.0F}, {0.0F, 0.0F, -1.0F}, 7, collision, manager);
    expect(first.outcome == ShotOutcome::Destroyed && first.hit
            && first.hit->target == crate && first.spawnedItem.isValid()
            && first.impact
            && first.impact->material == ImpactMaterial::Composite,
        "hitscan destroys the nearer composite prop before an enemy behind it");
    expect(manager.findData<EnemyData>(enemy)->health == 100.0F,
        "prop cover prevents damage to the enemy behind it");

    collision = collisionWorldWithProps(base, manager, 7);
    const ShotResult second = fireHitscan(
        weapon, {0.2F, 0.4F, 0.0F}, {0.0F, 0.0F, -1.0F}, 7, collision, manager);
    expect(second.outcome == ShotOutcome::Hit && second.hit
            && second.hit->target == enemy,
        "destroyed prop no longer blocks the following hitscan");

    const EntityId terminal = manager.createProp({
        .transform = {.position = {2.0F, 0.0F, -2.0F}},
        .roomId = 7,
        .type = PropType::Terminal,
        .solid = true,
    });
    const ShotResult protectedHit = fireHitscan(
        weapon, {2.0F, 0.4F, 0.0F}, {0.0F, 0.0F, -1.0F}, 7, base, manager);
    expect(protectedHit.outcome == ShotOutcome::ImpactProp && protectedHit.hit
            && protectedHit.hit->target == terminal && protectedHit.impact
            && protectedHit.impact->material == ImpactMaterial::Metal,
        "protected metal terminal absorbs hits and exposes its impact material");
}

void testTallCoverSupportsHiddenMovement()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    EntityManager manager;
    (void)manager.createProp({
        .transform = {.scale = {0.46F, 2.25F, 2.80F}},
        .roomId = 9,
        .type = PropType::Barricade,
        .solid = true,
        .health = 180.0F,
        .maximumHealth = 180.0F,
        .visualSeed = 123U,
    });
    const CollisionWorld base{{{-4.0F, 0.0F, -4.0F}, {4.0F, 4.0F, 4.0F}}};
    const CollisionWorld cover = collisionWorldWithProps(base, manager, 9);
    const wkc::core::Vec3 enemyEye{1.0F, 1.45F, 0.0F};
    bool hiddenAcrossMovement = true;
    for (const float z : {-1.0F, 0.0F, 1.0F}) {
        hiddenAcrossMovement = hiddenAcrossMovement
            && !hasLineOfSight(enemyEye, {-1.0F, 1.62F, z}, cover);
    }
    expect(hiddenAcrossMovement,
        "tall cover conceals the player while moving two metres behind it");
    expect(hasLineOfSight(
               {1.0F, 1.45F, 1.60F}, {-1.0F, 1.62F, 1.60F}, cover),
        "leaving the wide cover edge restores a direct observation line");

    const CharacterMotionResult movement = cover.moveCharacter(
        {-0.75F, 0.0F, -1.0F}, {0.0F, 0.0F, 1.0F}, 2.0F, {});
    expect(!movement.collided && movement.position.z == 1.0F,
        "the player can traverse the concealed side without colliding with cover");
}

void testDestructibleCeilingFixture()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    EntityManager manager;
    const EntityId fixture = manager.createProp({
        .transform = {
            .position = {0.0F, 3.80F, -2.0F},
            .scale = {1.20F, 0.18F, 0.28F},
        },
        .roomId = 12,
        .type = PropType::CeilingFixture,
        .solid = false,
        .health = 24.0F,
        .maximumHealth = 24.0F,
        .visualSeed = 991U,
    });
    const CollisionWorld room{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    expect(collisionWorldWithProps(room, manager, 12).solids().empty(),
        "ceiling fixture is targetable without blocking character navigation");

    Weapon weapon({.damage = 12.0F, .fireInterval = 0.0F, .range = 10.0F});
    const ShotResult damaged = fireHitscan(
        weapon, {0.0F, 3.80F, 0.0F}, {0.0F, 0.0F, -1.0F},
        12, room, manager);
    expect(damaged.outcome == ShotOutcome::Hit && damaged.hit
            && damaged.hit->target == fixture && damaged.impact
            && damaged.impact->material == ImpactMaterial::Metal
            && manager.findData<PropData>(fixture)->state == PropState::Damaged,
        "first fixture hit produces a metallic impact and persistent damage");

    const ShotResult destroyed = fireHitscan(
        weapon, {0.0F, 3.80F, 0.0F}, {0.0F, 0.0F, -1.0F},
        12, room, manager);
    expect(destroyed.outcome == ShotOutcome::Destroyed && destroyed.hit
            && destroyed.hit->target == fixture
            && !destroyed.spawnedItem.isValid()
            && manager.findData<PropData>(fixture)->state == PropState::Destroyed,
        "second fixture hit permanently disables it without generating loot");

    const ShotResult afterDestruction = fireHitscan(
        weapon, {0.0F, 3.80F, 0.0F}, {0.0F, 0.0F, -1.0F},
        12, room, manager);
    expect(afterDestruction.outcome == ShotOutcome::ImpactWorld
            && (!afterDestruction.hit || afterDestruction.hit->target != fixture),
        "destroyed fixture no longer intercepts subsequent shots");
}

} // namespace

int main()
{
    testDeterministicPropLayout();
    testPropSeedBatch();
    testPropDamageCollisionAndDrops();
    testHitscanPropPriority();
    testTallCoverSupportsHiddenMovement();
    testDestructibleCeilingFixture();
    if (failures != 0) {
        std::cerr << failures << " prop test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP prop tests passed\n";
    return 0;
}
