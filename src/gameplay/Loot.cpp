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
        }
        if (!applied) {
            ++summary.blockedByCapacity;
            continue;
        }
        if (manager.destroy(id)) {
            summary.collectedItems.push_back(id);
            ++summary.collected;
        }
    }
    return summary;
}

} // namespace gdp::gameplay
