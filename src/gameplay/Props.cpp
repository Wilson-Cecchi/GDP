#include "gameplay/Props.hpp"

#include <algorithm>

namespace gdp::gameplay {

wkc::core::Aabb propCollisionBounds(
    const gdp::entities::Entity& entity,
    const gdp::entities::PropData& prop) noexcept
{
    wkc::core::Vec3 scale = entity.transform.scale;
    if (prop.type == gdp::entities::PropType::CeilingFixture) {
        const wkc::core::Vec3 half = scale * 0.5F;
        return {
            entity.transform.position - half,
            entity.transform.position + half,
        };
    }
    if (prop.state == gdp::entities::PropState::Destroyed) {
        scale.y = std::min(scale.y, 0.12F);
    }
    const wkc::core::Vec3 half{scale.x * 0.5F, 0.0F, scale.z * 0.5F};
    return {
        {entity.transform.position.x - half.x,
            entity.transform.position.y,
            entity.transform.position.z - half.z},
        {entity.transform.position.x + half.x,
            entity.transform.position.y + scale.y,
            entity.transform.position.z + half.z},
    };
}

CollisionWorld collisionWorldWithProps(
    const CollisionWorld& base,
    const gdp::entities::EntityManager& manager,
    const std::uint32_t roomId)
{
    std::vector<wkc::core::Aabb> solids = base.solids();
    for (const gdp::entities::EntityId id : manager.query(gdp::entities::EntityKind::Prop)) {
        const gdp::entities::Entity* entity = manager.find(id);
        const gdp::entities::PropData* prop
            = manager.findData<gdp::entities::PropData>(id);
        if (entity == nullptr || prop == nullptr || entity->roomId != roomId
            || !prop->solid || prop->state == gdp::entities::PropState::Destroyed
            || prop->maximumHealth <= 0.0F) {
            continue;
        }
        solids.push_back(propCollisionBounds(*entity, *prop));
    }
    return CollisionWorld::fromWalkableVolumes(
        base.walkableVolumes(), std::move(solids));
}

PropDamageResult damageProp(
    gdp::entities::EntityManager& manager,
    const gdp::entities::EntityId target,
    const float amount) noexcept
{
    using namespace gdp::entities;
    Entity* entity = manager.find(target);
    PropData* prop = manager.findData<PropData>(target);
    if (entity == nullptr || prop == nullptr || amount <= 0.0F
        || prop->maximumHealth <= 0.0F || prop->state == PropState::Destroyed) {
        return {};
    }

    prop->health = std::max(0.0F, prop->health - amount);
    if (prop->health > 0.0F) {
        prop->state = prop->health <= prop->maximumHealth * 0.5F
            ? PropState::Damaged : PropState::Intact;
        return {PropDamageOutcome::Damaged, {}};
    }

    prop->state = PropState::Destroyed;
    prop->solid = false;
    const std::uint32_t dropQuantity = prop->dropQuantity;
    const ItemType dropType = prop->dropType;
    prop->dropQuantity = 0U;
    if (dropQuantity == 0U) {
        return {PropDamageOutcome::Destroyed, {}};
    }
    const EntityId drop = manager.createItem({
        .transform = {.position = entity->transform.position},
        .roomId = entity->roomId,
        .type = dropType,
        .quantity = dropQuantity,
    });
    return {PropDamageOutcome::Destroyed, drop};
}

bool repairProp(
    gdp::entities::EntityManager& manager,
    const gdp::entities::EntityId target,
    const float amount) noexcept
{
    using namespace gdp::entities;
    PropData* prop = manager.findData<PropData>(target);
    if (prop == nullptr || amount <= 0.0F || prop->maximumHealth <= 0.0F
        || prop->state == PropState::Destroyed || prop->health >= prop->maximumHealth) {
        return false;
    }
    prop->health = std::min(prop->maximumHealth, prop->health + amount);
    prop->state = prop->health >= prop->maximumHealth
        ? PropState::Intact : PropState::Damaged;
    return true;
}

} // namespace gdp::gameplay
