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
    if (prop.type == gdp::entities::PropType::LaboratoryPlanter
        && prop.state == gdp::entities::PropState::Damaged) {
        scale.y = 0.16F;
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

PropSolidBounds terminalPhysicalBounds(
    const wkc::core::Vec3 position,
    const wkc::core::Vec3 scale) noexcept
{
    PropSolidBounds result;
    const bool sideways = scale.z > scale.x;
    const auto part = [&](const float centerX, const float lowY,
                          const float highY, const float halfX,
                          const float halfZ) {
        const float cx = sideways ? 0.0F : centerX;
        const float cz = sideways ? centerX : 0.0F;
        const float hx = sideways ? halfZ : halfX;
        const float hz = sideways ? halfX : halfZ;
        return wkc::core::Aabb{
            position + wkc::core::Vec3{cx-hx, lowY, cz-hz},
            position + wkc::core::Vec3{cx+hx, highY, cz+hz},
        };
    };
    result.parts = {{
        part(-0.43F, 0.0F, 1.80F, 0.145F, 0.26F),
        part( 0.43F, 0.0F, 1.80F, 0.145F, 0.26F),
        part(0.0F, 1.63F, 1.79F, 0.385F, 0.12F),
        part(0.0F, 0.22F, 0.36F, 0.385F, 0.12F),
        part(0.0F, 0.79F, 1.41F, 0.375F, 0.30F),
    }};
    result.count = result.parts.size();
    return result;
}

PropSolidBounds propPhysicalBounds(
    const gdp::entities::Entity& entity,
    const gdp::entities::PropData& prop) noexcept
{
    if (prop.type == gdp::entities::PropType::Terminal) {
        return terminalPhysicalBounds(
            entity.transform.position, entity.transform.scale);
    }
    PropSolidBounds result;
    result.parts[0] = propCollisionBounds(entity, prop);
    result.count = 1U;
    return result;
}

CollisionWorld collisionWorldWithProps(
    const CollisionWorld& base,
    const gdp::entities::EntityManager& manager,
    const std::uint32_t roomId,
    const std::span<const std::uint32_t> additionalRoomIds)
{
    std::vector<wkc::core::Aabb> solids = base.solids();
    for (const gdp::entities::EntityId id : manager.query(gdp::entities::EntityKind::Prop)) {
        const gdp::entities::Entity* entity = manager.find(id);
        const gdp::entities::PropData* prop
            = manager.findData<gdp::entities::PropData>(id);
        const bool roomIncluded = entity != nullptr
            && (entity->roomId == roomId
                || std::find(additionalRoomIds.begin(), additionalRoomIds.end(),
                       entity->roomId) != additionalRoomIds.end());
        if (entity == nullptr || prop == nullptr || !roomIncluded
            || !prop->solid || prop->state == gdp::entities::PropState::Destroyed
            || (prop->maximumHealth <= 0.0F
                && prop->type != gdp::entities::PropType::Terminal)) {
            continue;
        }
        const PropSolidBounds parts = propPhysicalBounds(*entity, *prop);
        solids.insert(solids.end(), parts.parts.begin(),
            parts.parts.begin() + static_cast<std::ptrdiff_t>(parts.count));
    }
    return CollisionWorld::fromWalkableVolumes(
        base.walkableVolumes(), std::move(solids), base.walkableOutline(),
        base.volumeOutlines(), base.octagonalSolids());
}

PropDamageResult damageProp(
    gdp::entities::EntityManager& manager,
    const gdp::entities::EntityId target,
    const float amount,
    const bool directPotHit) noexcept
{
    using namespace gdp::entities;
    Entity* entity = manager.find(target);
    PropData* prop = manager.findData<PropData>(target);
    if (entity == nullptr || prop == nullptr || amount <= 0.0F
        || prop->maximumHealth <= 0.0F || prop->state == PropState::Destroyed) {
        return {};
    }

    if (prop->type == PropType::LaboratoryPlanter
        && prop->state == PropState::Intact && !directPotHit) {
        prop->health = prop->maximumHealth * 0.5F;
        prop->state = PropState::Damaged;
        return {PropDamageOutcome::Damaged, {}};
    }

    prop->health = directPotHit && prop->type == PropType::LaboratoryPlanter
        ? 0.0F : std::max(0.0F, prop->health - amount);
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
