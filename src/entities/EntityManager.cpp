#include "entities/EntityManager.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace gdp::entities {
namespace {

float defaultPropHealth(const PropType type) noexcept
{
    switch (type) {
    case PropType::Crate:
        return 60.0F;
    case PropType::Canister:
        return 35.0F;
    case PropType::Barricade:
        return 90.0F;
    case PropType::Terminal:
    case PropType::Door:
    case PropType::Decoration:
        return 0.0F;
    }
    return 0.0F;
}

} // namespace

EntityId EntityManager::createPlayer(const PlayerCreateInfo& info)
{
    return create(EntityKind::Player, info.transform, info.roomId, PlayerData{info.health});
}

EntityId EntityManager::createEnemy(const EnemyCreateInfo& info)
{
    const float health = std::max(info.health, 1.0F);
    return create(
        EntityKind::Enemy,
        info.transform,
        info.roomId,
        EnemyData{info.archetype, health, health,
            EnemyBehaviorState::Idle, 0.0F, info.visualSeed, 0.0F, info.active});
}

EntityId EntityManager::createItem(const ItemCreateInfo& info)
{
    return create(
        EntityKind::Item,
        info.transform,
        info.roomId,
        ItemData{info.type, std::max(info.quantity, 1U)});
}

EntityId EntityManager::createProp(const PropCreateInfo& info)
{
    const float fallbackHealth = defaultPropHealth(info.type);
    const float requestedMaximum = info.maximumHealth > 0.0F
        ? info.maximumHealth : fallbackHealth;
    const float requestedHealth = info.health > 0.0F
        ? info.health
        : info.state == PropState::Damaged ? requestedMaximum * 0.5F
                                           : requestedMaximum;
    const float health = std::clamp(requestedHealth, 0.0F, requestedMaximum);
    const float maximumHealth = std::max(requestedMaximum, health);
    return create(
        EntityKind::Prop,
        info.transform,
        info.roomId,
        PropData{info.type, info.state, info.solid, health, maximumHealth,
            info.dropType, info.dropQuantity, info.visualSeed});
}

bool EntityManager::destroy(const EntityId id) noexcept
{
    Slot* slot = findSlot(id);
    if (slot == nullptr || slot->pendingDestroy) {
        return false;
    }
    slot->pendingDestroy = true;
    --activeCount_;
    ++pendingDestroyCount_;
    return true;
}

std::size_t EntityManager::destroyInRoom(const std::uint32_t roomId) noexcept
{
    std::size_t destroyed = 0;
    for (Slot& slot : slots_) {
        if (slot.entity && !slot.pendingDestroy && slot.entity->roomId == roomId) {
            slot.pendingDestroy = true;
            --activeCount_;
            ++pendingDestroyCount_;
            ++destroyed;
        }
    }
    return destroyed;
}

std::size_t EntityManager::flushDestroyed() noexcept
{
    std::size_t flushed = 0;
    for (std::uint32_t index = 0; index < slots_.size(); ++index) {
        Slot& slot = slots_[index];
        if (!slot.pendingDestroy) {
            continue;
        }
        slot.entity.reset();
        slot.pendingDestroy = false;
        ++slot.generation;
        if (slot.generation == 0) {
            slot.generation = 1;
        }
        freeSlots_.push_back(index);
        ++flushed;
    }
    pendingDestroyCount_ = 0;
    return flushed;
}

void EntityManager::clear() noexcept
{
    freeSlots_.clear();
    freeSlots_.reserve(slots_.size());
    for (std::uint32_t index = 0; index < slots_.size(); ++index) {
        Slot& slot = slots_[index];
        slot.entity.reset();
        slot.pendingDestroy = false;
        ++slot.generation;
        if (slot.generation == 0) {
            slot.generation = 1;
        }
        freeSlots_.push_back(index);
    }
    activeCount_ = 0;
    pendingDestroyCount_ = 0;
}

Entity* EntityManager::find(const EntityId id) noexcept
{
    Slot* slot = findSlot(id);
    return slot == nullptr || slot->pendingDestroy ? nullptr : &*slot->entity;
}

const Entity* EntityManager::find(const EntityId id) const noexcept
{
    const Slot* slot = findSlot(id);
    return slot == nullptr || slot->pendingDestroy ? nullptr : &*slot->entity;
}

bool EntityManager::isAlive(const EntityId id) const noexcept
{
    return find(id) != nullptr;
}

std::vector<EntityId> EntityManager::query(const EntityKind kind) const
{
    std::vector<EntityId> result;
    result.reserve(activeCount_);
    for (const Slot& slot : slots_) {
        if (slot.entity && !slot.pendingDestroy && slot.entity->kind == kind) {
            result.push_back(slot.entity->id);
        }
    }
    return result;
}

std::vector<EntityId> EntityManager::queryRoom(const std::uint32_t roomId) const
{
    std::vector<EntityId> result;
    for (const Slot& slot : slots_) {
        if (slot.entity && !slot.pendingDestroy && slot.entity->roomId == roomId) {
            result.push_back(slot.entity->id);
        }
    }
    return result;
}

std::size_t EntityManager::activeCount() const noexcept
{
    return activeCount_;
}

std::size_t EntityManager::pendingDestroyCount() const noexcept
{
    return pendingDestroyCount_;
}

std::size_t EntityManager::capacity() const noexcept
{
    return slots_.size();
}

EntityId EntityManager::create(
    const EntityKind kind,
    const wkc::core::Transform transform,
    const std::uint32_t roomId,
    EntityData data)
{
    std::uint32_t index = 0;
    if (freeSlots_.empty()) {
        if (slots_.size() >= std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error{"EntityManager exhausted its EntityId index space"};
        }
        index = static_cast<std::uint32_t>(slots_.size());
        slots_.push_back({});
    } else {
        index = freeSlots_.back();
        freeSlots_.pop_back();
    }
    Slot& slot = slots_[index];
    const EntityId id{index, slot.generation};
    slot.entity = Entity{id, kind, transform, roomId, std::move(data)};
    slot.pendingDestroy = false;
    ++activeCount_;
    return id;
}

EntityManager::Slot* EntityManager::findSlot(const EntityId id) noexcept
{
    if (!id.isValid() || id.index >= slots_.size()) {
        return nullptr;
    }
    Slot& slot = slots_[id.index];
    return slot.entity && slot.generation == id.generation ? &slot : nullptr;
}

const EntityManager::Slot* EntityManager::findSlot(const EntityId id) const noexcept
{
    if (!id.isValid() || id.index >= slots_.size()) {
        return nullptr;
    }
    const Slot& slot = slots_[id.index];
    return slot.entity && slot.generation == id.generation ? &slot : nullptr;
}

} // namespace gdp::entities
