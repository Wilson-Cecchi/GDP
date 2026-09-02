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
    wkc::core::Aabb bounds{};
};

struct PlayerState {
    wkc::core::Vec3 position{0.0F, 0.0F, 4.0F};
    wkc::core::Vec3 velocity{};
    float yawDegrees{-90.0F};
    float pitchDegrees{0.0F};
    float health{100.0F};
    float maximumHealth{100.0F};
    float armor{0.0F};
    float maximumArmor{50.0F};
    std::uint32_t keys{0};
    bool grounded{true};
    bool alive{true};
};

class Player {
public:
    Player() = default;

    void spawn(wkc::core::Vec3 feetPosition, float yawDegrees = -90.0F) noexcept;
    [[nodiscard]] bool relocate(
        wkc::core::Vec3 feetPosition,
        float yawDegrees) noexcept;
    void bindEntity(gdp::entities::EntityId entity) noexcept;
    void setMouseSensitivity(float sensitivity) noexcept;
    void setInvertMouseY(bool inverted) noexcept;
    void applyLook(wkc::core::Vec2 mouseDelta) noexcept;
    void fixedUpdate(
        const PlayerCommand& command,
        float fixedStepSeconds,
        const CollisionWorld& collisionWorld) noexcept;

    [[nodiscard]] std::optional<gdp::entities::EntityId> findInteraction(
        std::span<const InteractionTarget> targets,
        float maximumDistance = 2.25F) const noexcept;

    void applyDamage(float amount) noexcept;
    void heal(float amount) noexcept;
    void addArmor(float amount) noexcept;
    void addKeys(std::uint32_t amount) noexcept;
    [[nodiscard]] bool increaseMaximumHealth(
        float amount,
        float healing = 0.0F) noexcept;
    [[nodiscard]] bool increaseMaximumArmor(
        float amount,
        float armor = 0.0F) noexcept;

    [[nodiscard]] const PlayerState& state() const noexcept;
    [[nodiscard]] gdp::entities::EntityId entity() const noexcept;
    [[nodiscard]] wkc::core::Vec3 forwardDirection() const noexcept;
    [[nodiscard]] wkc::core::Vec3 planarForwardDirection() const noexcept;
    [[nodiscard]] wkc::core::Vec3 rightDirection() const noexcept;
    [[nodiscard]] wkc::core::Vec3 cameraPosition() const noexcept;
    [[nodiscard]] float mouseSensitivity() const noexcept;
    [[nodiscard]] bool invertMouseY() const noexcept;

private:
    PlayerState state_{};
    gdp::entities::EntityId entity_{};
    CharacterCollider collider_{};
    float moveSpeed_{4.5F};
    float gravity_{-20.0F};
    float mouseSensitivity_{0.12F};
    bool invertMouseY_{false};
    float eyeHeight_{1.62F};
};

} // namespace gdp::gameplay
