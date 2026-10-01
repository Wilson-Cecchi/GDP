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
    Patrol,
    Investigate,
    Alert,
    Recover,
};

enum class ItemType : std::uint8_t {
    Health,
    Ammo,
    Key,
    Armor,
    Weapon,
    Special,
};

enum class PropType : std::uint8_t {
    Terminal,
    Crate,
    Door,
    Decoration,
    Canister,
    Barricade,
    CeilingFixture,
};

enum class PropState : std::uint8_t {
    Intact,
    Damaged,
    Destroyed,
};

struct PlayerData {
    float health{100.0F};
    float armor{0.0F};
    float maximumArmor{50.0F};
    std::uint32_t keys{0};
};

struct EnemyData {
    EnemyArchetype archetype{EnemyArchetype::Worker};
    std::uint32_t originRoomId{0};
    float health{50.0F};
    float maximumHealth{50.0F};
    EnemyBehaviorState behavior{EnemyBehaviorState::Idle};
    float attackCooldown{0.0F};
    wkc::core::Seed visualSeed{0};
    float supportCooldown{0.0F};
    bool active{true};
    float avoidanceTime{0.0F};
    std::int8_t avoidanceSide{0};
    float hitReactionTime{0.0F};
    wkc::core::Vec3 patrolOrigin{};
    std::uint32_t patrolWaypoint{0U};
    float patrolWaypointTime{0.0F};
    float patrolPauseTime{0.0F};
    wkc::core::Vec3 lastKnownPlayerPosition{};
    float investigationTime{0.0F};
    float alertTime{0.0F};
    float recoverTime{0.0F};
    std::uint32_t magazineAmmo{8U};
    std::uint32_t reserveAmmo{40U};
    float reloadRemaining{0.0F};
};

struct ItemData {
    ItemType type{ItemType::Health};
    std::uint32_t quantity{1};
    struct RecoveredWeapon {
        std::uint32_t archetype{0U};
        std::uint32_t magazineAmmo{0U};
        std::uint32_t reserveAmmo{0U};

        constexpr bool operator==(const RecoveredWeapon&) const noexcept = default;
    };
    std::optional<RecoveredWeapon> recoveredWeapon;
    // Transient presentation timing; gameplay and saved ammunition stay put.
    std::optional<std::uint64_t> droppedVisualStartTick;
    float droppedVisualYawDegrees{0.0F};
    // Generated pickups can specify a firearm without claiming it is a player drop.
    std::optional<std::uint32_t> spawnedWeaponArchetype;
};

struct PropData {
    PropType type{PropType::Decoration};
    PropState state{PropState::Intact};
    bool solid{false};
    float health{0.0F};
    float maximumHealth{0.0F};
    ItemType dropType{ItemType::Ammo};
    std::uint32_t dropQuantity{0};
    wkc::core::Seed visualSeed{0};
};

using EntityData = std::variant<PlayerData, EnemyData, ItemData, PropData>;

struct Entity {
    EntityId id{};
    EntityKind kind{EntityKind::Prop};
    wkc::core::Transform transform{};
    std::uint32_t roomId{0};
    EntityData data{PropData{}};
};

struct PlayerCreateInfo {
    wkc::core::Transform transform{};
    std::uint32_t roomId{0};
    float health{100.0F};
    float armor{0.0F};
    float maximumArmor{50.0F};
};

struct EnemyCreateInfo {
    wkc::core::Transform transform{};
    std::uint32_t roomId{0};
    EnemyArchetype archetype{EnemyArchetype::Worker};
    float health{50.0F};
    wkc::core::Seed visualSeed{0};
    bool active{true};
};

struct ItemCreateInfo {
    wkc::core::Transform transform{};
    std::uint32_t roomId{0};
    ItemType type{ItemType::Health};
    std::uint32_t quantity{1};
};

struct PropCreateInfo {
    wkc::core::Transform transform{};
    std::uint32_t roomId{0};
    PropType type{PropType::Decoration};
    PropState state{PropState::Intact};
    bool solid{false};
    float health{0.0F};
    float maximumHealth{0.0F};
    ItemType dropType{ItemType::Ammo};
    std::uint32_t dropQuantity{0};
    wkc::core::Seed visualSeed{0};
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
        wkc::core::Transform transform,
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
