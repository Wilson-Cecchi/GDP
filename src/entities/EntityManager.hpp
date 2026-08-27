#pragma once

#include "core/Math.hpp"
#include "core/Seed.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

namespace gdp::entities {

struct EntityId {
    std::uint32_t index{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t generation{0};

    constexpr bool operator==(const EntityId&) const noexcept = default;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return index != std::numeric_limits<std::uint32_t>::max() && generation != 0;
    }

    [[nodiscard]] constexpr std::uint64_t packed() const noexcept
    {
        return (static_cast<std::uint64_t>(generation) << 32U) | index;
    }
};

enum class EntityKind : std::uint8_t {
    Player,
    Enemy,
    Item,
    Prop,
};

enum class EnemyArchetype : std::uint8_t {
    Worker,
    Guard,
    Officer,
    Engineer,
    IndustrialRobot,
};

enum class EnemyBehaviorState : std::uint8_t {
    Idle,
    Chase,
    Attack,
    Dead,
    Reposition,
};

enum class ItemType : std::uint8_t {
    Health,
    Ammo,
    Key,
};

enum class PropType : std::uint8_t {
    Terminal,
    Crate,
    Door,
    Decoration,
};

enum class PropState : std::uint8_t {
    Intact,
    Damaged,
    Destroyed,
};

struct PlayerData {
    float health{100.0F};
};

struct EnemyData {
    EnemyArchetype archetype{EnemyArchetype::Worker};
    float health{50.0F};
    float maximumHealth{50.0F};
    EnemyBehaviorState behavior{EnemyBehaviorState::Idle};
    float attackCooldown{0.0F};
    gdp::core::Seed visualSeed{0};
    float supportCooldown{0.0F};
};

struct ItemData {
    ItemType type{ItemType::Health};
    std::uint32_t quantity{1};
};

struct PropData {
    PropType type{PropType::Decoration};
    PropState state{PropState::Intact};
    bool solid{false};
};

using EntityData = std::variant<PlayerData, EnemyData, ItemData, PropData>;

struct Entity {
    EntityId id{};
    EntityKind kind{EntityKind::Prop};
    gdp::core::Transform transform{};
    std::uint32_t roomId{0};
    EntityData data{PropData{}};
};

struct PlayerCreateInfo {
    gdp::core::Transform transform{};
    std::uint32_t roomId{0};
    float health{100.0F};
};

struct EnemyCreateInfo {
    gdp::core::Transform transform{};
    std::uint32_t roomId{0};
    EnemyArchetype archetype{EnemyArchetype::Worker};
    float health{50.0F};
    gdp::core::Seed visualSeed{0};
};

struct ItemCreateInfo {
    gdp::core::Transform transform{};
    std::uint32_t roomId{0};
    ItemType type{ItemType::Health};
    std::uint32_t quantity{1};
};

struct PropCreateInfo {
    gdp::core::Transform transform{};
    std::uint32_t roomId{0};
    PropType type{PropType::Decoration};
    PropState state{PropState::Intact};
    bool solid{false};
};

class EntityManager {
public:
    [[nodiscard]] EntityId createPlayer(const PlayerCreateInfo& info);
    [[nodiscard]] EntityId createEnemy(const EnemyCreateInfo& info);
    [[nodiscard]] EntityId createItem(const ItemCreateInfo& info);
    [[nodiscard]] EntityId createProp(const PropCreateInfo& info);

    [[nodiscard]] bool destroy(EntityId id) noexcept;
    [[nodiscard]] std::size_t destroyInRoom(std::uint32_t roomId) noexcept;
    [[nodiscard]] std::size_t flushDestroyed() noexcept;
    void clear() noexcept;

    [[nodiscard]] Entity* find(EntityId id) noexcept;
    [[nodiscard]] const Entity* find(EntityId id) const noexcept;
    [[nodiscard]] bool isAlive(EntityId id) const noexcept;
    [[nodiscard]] std::vector<EntityId> query(EntityKind kind) const;
    [[nodiscard]] std::vector<EntityId> queryRoom(std::uint32_t roomId) const;

    template<typename Data>
    [[nodiscard]] Data* findData(const EntityId id) noexcept
    {
        Entity* entity = find(id);
        return entity == nullptr ? nullptr : std::get_if<Data>(&entity->data);
    }

    template<typename Data>
    [[nodiscard]] const Data* findData(const EntityId id) const noexcept
    {
        const Entity* entity = find(id);
        return entity == nullptr ? nullptr : std::get_if<Data>(&entity->data);
    }

    [[nodiscard]] std::size_t activeCount() const noexcept;
    [[nodiscard]] std::size_t pendingDestroyCount() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;

private:
    struct Slot {
        std::uint32_t generation{1};
        std::optional<Entity> entity;
        bool pendingDestroy{false};
    };

    [[nodiscard]] EntityId create(
        EntityKind kind,
        gdp::core::Transform transform,
        std::uint32_t roomId,
        EntityData data);
    [[nodiscard]] Slot* findSlot(EntityId id) noexcept;
    [[nodiscard]] const Slot* findSlot(EntityId id) const noexcept;

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> freeSlots_;
    std::size_t activeCount_{0};
    std::size_t pendingDestroyCount_{0};
};

} // namespace gdp::entities
