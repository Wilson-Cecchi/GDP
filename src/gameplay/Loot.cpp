#include "gameplay/Loot.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::gameplay {

LootCollectionSummary collectNearbyLoot(
    gdp::entities::EntityManager& manager,
    Player& player,
    Weapon& weapon,
    const std::uint32_t roomId,
    const float collectionRadius) noexcept
{
    using gdp::entities::Entity;
    using gdp::entities::EntityKind;
    using gdp::entities::ItemData;
    using gdp::entities::ItemType;

    LootCollectionSummary summary;
    const float radius = std::max(collectionRadius, 0.0F);
    if (!player.state().alive || radius <= 0.0F) {
        return summary;
    }
    for (const gdp::entities::EntityId id : manager.query(EntityKind::Item)) {
        const Entity* entity = manager.find(id);
        const ItemData* item = manager.findData<ItemData>(id);
        if (entity == nullptr || item == nullptr || entity->roomId != roomId) {
            continue;
        }
        const float x = entity->transform.position.x - player.state().position.x;
        const float z = entity->transform.position.z - player.state().position.z;
        if (std::sqrt((x * x) + (z * z)) > radius) {
            continue;
        }
        ++summary.itemsInRange;
        bool applied = false;
        switch (item->type) {
        case ItemType::Health: {
            const float before = player.state().health;
            player.heal(static_cast<float>(item->quantity));
            const float restored = player.state().health - before;
            if (restored > 0.0F) {
                summary.healthRestored += restored;
                applied = true;
            }
            break;
        }
        case ItemType::Ammo: {
            const std::uint32_t added = weapon.addReserveAmmo(item->quantity);
            if (added > 0U) {
                summary.ammoAdded += added;
                applied = true;
            }
            break;
        }
        case ItemType::Key:
            player.addKeys(item->quantity);
            summary.keysAdded += item->quantity;
            applied = item->quantity > 0U;
            break;
        case ItemType::Armor: {
            const float before = player.state().armor;
            player.addArmor(static_cast<float>(item->quantity));
            const float added = player.state().armor - before;
            if (added > 0.0F) {
                summary.armorAdded += added;
                applied = true;
            }
            break;
        }
        case ItemType::Weapon: {
            bool ammunitionPickup = false;
            if (item->recoveredWeapon) {
                applied = item->recoveredWeapon->archetype
                        < static_cast<std::uint32_t>(WeaponArchetype::Count)
                    && weapon.equipRecovered(
                        static_cast<WeaponArchetype>(
                            item->recoveredWeapon->archetype),
                        item->recoveredWeapon->magazineAmmo,
                        item->recoveredWeapon->reserveAmmo);
                summary.weaponsRecovered += applied ? 1U : 0U;
            } else {
                const auto archetype = item->spawnedWeaponArchetype.value_or(
                    static_cast<std::uint32_t>(WeaponArchetype::Carbine));
                if (archetype < static_cast<std::uint32_t>(WeaponArchetype::Count)) {
                    const auto type = static_cast<WeaponArchetype>(archetype);
                    if (weapon.ownsWeapon(type) && isFirearmArchetype(type)) {
                        ammunitionPickup = true;
                        const auto added = weapon.addReserveAmmoFor(type, baseWeaponDefinition(type).magazineSize);
                        summary.ammoAdded += added;
                        applied = added > 0U;
                    } else applied = weapon.equip(type);
                }
            }
            summary.weaponsEquipped += applied && !ammunitionPickup ? 1U : 0U;
            break;
        }
        case ItemType::Special:
            applied = weapon.activateOverdrive();
            summary.specialsActivated += applied ? 1U : 0U;
            break;
        }
        if (!applied) {
            ++summary.blockedByCapacity;
            continue;
        }
        const bool recoveredWeapon = item->recoveredWeapon.has_value();
        if (manager.destroy(id)) {
            summary.collectedItems.push_back(id);
            ++summary.collected;
            summary.rewardableCollected += recoveredWeapon ? 0U : 1U;
        }
    }
    return summary;
}

std::optional<gdp::entities::EntityId> dropSelectedWeapon(
    gdp::entities::EntityManager& manager,
    Player& player,
    Weapon& weapon,
    const CollisionWorld& collisionWorld,
    const std::uint32_t roomId)
{
    if (!player.state().alive || weapon.switching()
        || weapon.definition().archetype == WeaponArchetype::Knife) {
        return std::nullopt;
    }
    const WeaponCampaignProgress current = weapon.campaignProgress();

    constexpr float dropDistance = 1.35F;
    constexpr float minimumClearDistance = 1.05F;
    const wkc::core::Vec3 position = player.state().position;
    const wkc::core::Vec3 velocity
        = player.planarForwardDirection() * dropDistance;
    const CharacterMotionResult motion = collisionWorld.moveCharacter(
        position, velocity, 1.0F, player.collider());
    const float deltaX = motion.position.x - position.x;
    const float deltaZ = motion.position.z - position.z;
    if ((deltaX * deltaX) + (deltaZ * deltaZ)
        < minimumClearDistance * minimumClearDistance) {
        return std::nullopt;
    }

    const gdp::entities::EntityId item = manager.createItem({
        .transform = {.position = motion.position},
        .roomId = roomId,
        .type = gdp::entities::ItemType::Weapon,
        .quantity = 1U,
    });
    gdp::entities::ItemData* itemData
        = manager.findData<gdp::entities::ItemData>(item);
    if (itemData == nullptr) {
        (void)manager.destroy(item);
        return std::nullopt;
    }
    itemData->recoveredWeapon = gdp::entities::ItemData::RecoveredWeapon{
        static_cast<std::uint32_t>(current.definition.archetype),
        current.magazineAmmo,
        current.reserveAmmo,
    };
    if (!weapon.dropSelectedFirearm()) {
        (void)manager.destroy(item);
        return std::nullopt;
    }
    return item;
}

} // namespace gdp::gameplay
