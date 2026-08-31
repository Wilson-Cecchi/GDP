#include "entities/EntityManager.hpp"
#include "gameplay/Loot.hpp"
#include "world/LootGenerator.hpp"

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
    const gdp::world::LootValidationResult& result,
    const gdp::world::LootValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

void testDeterministicLootLayout()
{
    using namespace gdp::world;
    constexpr std::array zones{
        SpawnZone{{{-3.0F, 0.0F, -3.0F}, {-2.0F, 0.0F, -2.0F}}},
    };
    constexpr wkc::core::Seed seed = 0x4C4F4F545F563031ULL;
    const LootLayout first = generateLootLayout(
        seed, RoomRole::Secret, RoomTheme::Laboratory, 4, zones);
    const LootLayout repeated = generateLootLayout(
        seed, RoomRole::Secret, RoomTheme::Laboratory, 4, zones);
    const LootLayout different = generateLootLayout(
        seed + 1U, RoomRole::Secret, RoomTheme::Laboratory, 4, zones);
    expect(validateLootLayout(first, zones).isValid(),
        "representative procedural loot layout validates");
    expect(first.drops == repeated.drops && first.layoutHash == repeated.layoutHash,
        "same room inputs reproduce loot types, quantities and placement exactly");
    expect(first.layoutHash != different.layoutHash,
        "different room seed changes the canonical loot layout");
    expect(first.drops.size() >= 2U
            && first.drops.front().type == gdp::entities::ItemType::Key
            && first.drops.front().quantity == 1U,
        "Secret loot always starts with one objective key and useful support loot");
    constexpr std::uint64_t goldenHash = 0x23E65CDDD8252D83ULL;
    expect(first.layoutHash == goldenHash,
        "golden procedural loot hash remains stable across builds");

    LootLayout invalid = first;
    invalid.drops.front().position = {99.0F, 0.0F, 99.0F};
    expect(containsError(validateLootLayout(invalid, zones), LootValidationError::OutsideZone),
        "loot validation rejects placement outside authored zones");
    invalid = first;
    invalid.spent = invalid.budget + 1U;
    expect(containsError(validateLootLayout(invalid, zones), LootValidationError::BudgetExceeded),
        "loot validation rejects budget overflow");
    invalid = first;
    invalid.drops.front().type = gdp::entities::ItemType::Ammo;
    expect(containsError(
               validateLootLayout(invalid, zones), LootValidationError::InvalidKeyDistribution),
        "Secret validation requires exactly one objective key");
    invalid = first;
    invalid.layoutHash ^= 1U;
    expect(containsError(
               validateLootLayout(invalid, zones), LootValidationError::InvalidLayoutHash),
        "loot validation detects canonical hash corruption");
}

void testRoleBudgetsAndBatch()
{
    using namespace gdp::world;
    constexpr std::array zones{
        SpawnZone{{{-3.0F, 0.0F, -3.0F}, {-2.0F, 0.0F, -2.0F}}},
    };
    const LootLayout start = generateLootLayout(
        11, RoomRole::Start, RoomTheme::Industrial, 0, zones);
    const LootLayout boss = generateLootLayout(
        11, RoomRole::Boss, RoomTheme::Security, 5, zones);
    const LootLayout easy = generateLootLayout(
        11, RoomRole::DeadEnd, RoomTheme::Storage, 3, zones, 0.5F);
    const LootLayout hard = generateLootLayout(
        11, RoomRole::DeadEnd, RoomTheme::Storage, 3, zones, 2.0F);
    expect(start.drops.size() == 1U
            && start.drops.front().type == gdp::entities::ItemType::Ammo,
        "START receives one immediately testable tutorial ammo pickup");
    expect(boss.drops.empty() && boss.budget == 0U,
        "Boss room does not place pre-fight procedural loot");
    expect(easy.budget > hard.budget && easy.spent <= easy.budget
            && hard.spent <= hard.budget,
        "difficulty scales resource budget without allowing overspend");

    std::unordered_set<std::uint64_t> hashes;
    bool allValid = true;
    bool foundHealth = false;
    bool foundAmmo = false;
    for (wkc::core::Seed seed = 0; seed < 1'000; ++seed) {
        const LootLayout layout = generateLootLayout(
            seed, RoomRole::Normal, RoomTheme::Industrial, 4, zones);
        if (!validateLootLayout(layout, zones).isValid()) {
            allValid = false;
            std::cerr << "Loot generation failed at room seed " << seed << '\n';
            for (const LootValidationError error : validateLootLayout(layout, zones).errors) {
                std::cerr << "  validation error " << static_cast<int>(error) << '\n';
            }
            break;
        }
        hashes.insert(layout.layoutHash);
        for (const LootDrop& drop : layout.drops) {
            foundHealth = foundHealth || drop.type == gdp::entities::ItemType::Health;
            foundAmmo = foundAmmo || drop.type == gdp::entities::ItemType::Ammo;
        }
    }
    expect(allValid, "1,000 room seeds pass loot budget, zone and spacing gates");
    expect(hashes.size() == 1'000U,
        "1,000 room seeds produce unique canonical loot hashes");
    expect(foundHealth && foundAmmo,
        "normal-room batch reaches both functional resource categories");

    const LootLayout missingZone = generateLootLayout(
        12, RoomRole::Normal, RoomTheme::Industrial, 2, {});
    expect(containsError(
               validateLootLayout(missingZone, {}), LootValidationError::MissingZone),
        "positive loot budget requires an authored candidate zone");
}

void testAutomaticLootCollection()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    player.applyDamage(40.0F);
    Weapon weapon({
        .fireInterval = 0.0F,
        .magazineSize = 4,
        .reserveMaximum = 10,
    });
    (void)weapon.tryFire();
    (void)weapon.tryFire();
    expect(weapon.reload() && weapon.reserveAmmo() == 8U,
        "pickup test creates finite ammo capacity before collection");

    EntityManager manager;
    const EntityId health = manager.createItem({
        .transform = {.position = {0.20F, 0.0F, 0.0F}},
        .roomId = 2,
        .type = ItemType::Health,
        .quantity = 25,
    });
    const EntityId ammo = manager.createItem({
        .transform = {.position = {0.30F, 0.0F, 0.0F}},
        .roomId = 2,
        .type = ItemType::Ammo,
        .quantity = 8,
    });
    const EntityId key = manager.createItem({
        .transform = {.position = {0.40F, 0.0F, 0.0F}},
        .roomId = 2,
        .type = ItemType::Key,
        .quantity = 1,
    });
    const EntityId otherRoom = manager.createItem({
        .transform = {.position = {0.10F, 0.0F, 0.0F}},
        .roomId = 3,
        .type = ItemType::Health,
        .quantity = 20,
    });
    const EntityId farAway = manager.createItem({
        .transform = {.position = {4.0F, 0.0F, 0.0F}},
        .roomId = 2,
        .type = ItemType::Health,
        .quantity = 20,
    });
    const LootCollectionSummary collected
        = collectNearbyLoot(manager, player, weapon, 2, 0.85F);
    expect(collected.itemsInRange == 3U && collected.collected == 3U
            && collected.healthRestored == 25.0F && collected.ammoAdded == 2U
            && collected.keysAdded == 1U,
        "nearby loot applies health, available ammo capacity and objective key");
    expect(player.state().health == 85.0F && weapon.reserveAmmo() == 10U
            && player.state().keys == 1U,
        "collected resources update the authoritative player and weapon state");
    expect(manager.find(health) == nullptr && manager.find(ammo) == nullptr
            && manager.find(key) == nullptr && manager.find(otherRoom) != nullptr
            && manager.find(farAway) != nullptr,
        "collection consumes only applied pickups in range and in the active room");
    expect(manager.flushDestroyed() == 3U,
        "collected pickups complete the deferred entity lifecycle cleanly");

    player.heal(100.0F);
    const EntityId fullHealth = manager.createItem({
        .transform = {.position = {0.10F, 0.0F, 0.0F}},
        .roomId = 2,
        .type = ItemType::Health,
        .quantity = 20,
    });
    const EntityId fullAmmo = manager.createItem({
        .transform = {.position = {0.20F, 0.0F, 0.0F}},
        .roomId = 2,
        .type = ItemType::Ammo,
        .quantity = 6,
    });
    const LootCollectionSummary blocked
        = collectNearbyLoot(manager, player, weapon, 2, 0.85F);
    expect(blocked.collected == 0U && blocked.blockedByCapacity == 2U
            && manager.find(fullHealth) != nullptr && manager.find(fullAmmo) != nullptr,
        "full health and ammo leave pickups available instead of wasting them");

    player.applyDamage(500.0F);
    expect(collectNearbyLoot(manager, player, weapon, 2, 0.85F).itemsInRange == 0U,
        "dead player cannot collect procedural loot");
}

} // namespace

int main()
{
    testDeterministicLootLayout();
    testRoleBudgetsAndBatch();
    testAutomaticLootCollection();
    if (failures != 0) {
        std::cerr << failures << " loot test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP loot tests passed\n";
    return 0;
}
