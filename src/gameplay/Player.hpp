#pragma once

#include "core/Math.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace gdp::gameplay {

struct PlayerCommand {
    float moveForward{0.0F};
    float moveRight{0.0F};
    bool interact{false};
};

struct InteractionTarget {
    gdp::entities::EntityId id{};
    gdp::core::Aabb bounds{};
};

struct PlayerState {
    gdp::core::Vec3 position{0.0F, 0.0F, 4.0F};
    gdp::core::Vec3 velocity{};
    float yawDegrees{-90.0F};
    float pitchDegrees{0.0F};
    float health{100.0F};
    float maximumHealth{100.0F};
    bool grounded{true};
    bool alive{true};
};

class Player {
public:
    Player() = default;

    void spawn(gdp::core::Vec3 feetPosition, float yawDegrees = -90.0F) noexcept;
    void bindEntity(gdp::entities::EntityId entity) noexcept;
    void applyLook(gdp::core::Vec2 mouseDelta) noexcept;
    void fixedUpdate(
        const PlayerCommand& command,
        float fixedStepSeconds,
        const CollisionWorld& collisionWorld) noexcept;

    [[nodiscard]] std::optional<gdp::entities::EntityId> findInteraction(
        std::span<const InteractionTarget> targets,
        float maximumDistance = 2.25F) const noexcept;

    void applyDamage(float amount) noexcept;
    void heal(float amount) noexcept;

    [[nodiscard]] const PlayerState& state() const noexcept;
    [[nodiscard]] gdp::entities::EntityId entity() const noexcept;
    [[nodiscard]] gdp::core::Vec3 forwardDirection() const noexcept;
    [[nodiscard]] gdp::core::Vec3 planarForwardDirection() const noexcept;
    [[nodiscard]] gdp::core::Vec3 rightDirection() const noexcept;
    [[nodiscard]] gdp::core::Vec3 cameraPosition() const noexcept;

private:
    PlayerState state_{};
    gdp::entities::EntityId entity_{};
    CharacterCollider collider_{};
    float moveSpeed_{4.5F};
    float gravity_{-20.0F};
    float mouseSensitivity_{0.12F};
    float eyeHeight_{1.62F};
};

} // namespace gdp::gameplay
