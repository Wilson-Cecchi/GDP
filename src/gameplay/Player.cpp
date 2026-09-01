#include "gameplay/Player.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace gdp::gameplay {
namespace {

std::optional<float> rayAabbDistance(
    const wkc::core::Vec3 origin,
    const wkc::core::Vec3 direction,
    const wkc::core::Aabb bounds,
    const float maximumDistance) noexcept
{
    float minimumDistance = 0.0F;
    float maximumHitDistance = maximumDistance;
    const float origins[3]{origin.x, origin.y, origin.z};
    const float directions[3]{direction.x, direction.y, direction.z};
    const float minima[3]{bounds.minimum.x, bounds.minimum.y, bounds.minimum.z};
    const float maxima[3]{bounds.maximum.x, bounds.maximum.y, bounds.maximum.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(directions[axis]) <= 1.0e-7F) {
            if (origins[axis] < minima[axis] || origins[axis] > maxima[axis]) {
                return std::nullopt;
            }
            continue;
        }

        const float inverseDirection = 1.0F / directions[axis];
        float nearDistance = (minima[axis] - origins[axis]) * inverseDirection;
        float farDistance = (maxima[axis] - origins[axis]) * inverseDirection;
        if (nearDistance > farDistance) {
            std::swap(nearDistance, farDistance);
        }
        minimumDistance = std::max(minimumDistance, nearDistance);
        maximumHitDistance = std::min(maximumHitDistance, farDistance);
        if (minimumDistance > maximumHitDistance) {
            return std::nullopt;
        }
    }
    return minimumDistance;
}

} // namespace

void Player::spawn(const wkc::core::Vec3 feetPosition, const float yawDegrees) noexcept
{
    state_ = {};
    state_.position = feetPosition;
    state_.yawDegrees = std::remainder(yawDegrees, 360.0F);
    state_.grounded = false;
}

bool Player::relocate(
    const wkc::core::Vec3 feetPosition,
    const float yawDegrees) noexcept
{
    if (!state_.alive || !std::isfinite(feetPosition.x)
        || !std::isfinite(feetPosition.y) || !std::isfinite(feetPosition.z)
        || !std::isfinite(yawDegrees)) {
        return false;
    }
    state_.position = feetPosition;
    state_.velocity = {};
    state_.yawDegrees = std::remainder(yawDegrees, 360.0F);
    state_.pitchDegrees = 0.0F;
    state_.grounded = false;
    return true;
}

void Player::bindEntity(const gdp::entities::EntityId entity) noexcept
{
    entity_ = entity;
}

void Player::setMouseSensitivity(const float sensitivity) noexcept
{
    if (std::isfinite(sensitivity)) {
        mouseSensitivity_ = std::clamp(sensitivity, 0.036F, 0.24F);
    }
}

void Player::applyLook(const wkc::core::Vec2 mouseDelta) noexcept
{
    if (!state_.alive) {
        return;
    }
    state_.yawDegrees += mouseDelta.x * mouseSensitivity_;
    state_.pitchDegrees = std::clamp(
        state_.pitchDegrees - (mouseDelta.y * mouseSensitivity_),
        -89.0F,
        89.0F);
    state_.yawDegrees = std::remainder(state_.yawDegrees, 360.0F);
}

void Player::fixedUpdate(
    const PlayerCommand& command,
    const float fixedStepSeconds,
    const CollisionWorld& collisionWorld) noexcept
{
    if (!state_.alive || fixedStepSeconds <= 0.0F) {
        return;
    }

    wkc::core::Vec3 desired = planarForwardDirection()
            * std::clamp(command.moveForward, -1.0F, 1.0F)
        + rightDirection() * std::clamp(command.moveRight, -1.0F, 1.0F);
    const float desiredLength = wkc::core::length(desired);
    if (desiredLength > 1.0F) {
        desired = desired * (1.0F / desiredLength);
    }

    state_.velocity.x = desired.x * moveSpeed_;
    state_.velocity.z = desired.z * moveSpeed_;
    state_.velocity.y += gravity_ * fixedStepSeconds;

    const CharacterMotionResult motion = collisionWorld.moveCharacter(
        state_.position,
        state_.velocity,
        fixedStepSeconds,
        collider_);
    state_.position = motion.position;
    state_.velocity = motion.velocity;
    state_.grounded = motion.grounded;
}

std::optional<gdp::entities::EntityId> Player::findInteraction(
    const std::span<const InteractionTarget> targets,
    const float maximumDistance) const noexcept
{
    if (!state_.alive || maximumDistance <= 0.0F) {
        return std::nullopt;
    }

    std::optional<gdp::entities::EntityId> nearestId;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const InteractionTarget& target : targets) {
        const auto distance = rayAabbDistance(
            cameraPosition(),
            forwardDirection(),
            target.bounds,
            maximumDistance);
        if (distance && *distance < nearestDistance) {
            nearestDistance = *distance;
            nearestId = target.id;
        }
    }
    return nearestId;
}

void Player::applyDamage(const float amount) noexcept
{
    if (!state_.alive || !std::isfinite(amount) || amount <= 0.0F) {
        return;
    }
    const float absorbed = std::min(state_.armor, amount);
    state_.armor -= absorbed;
    const float healthDamage = amount - absorbed;
    state_.health = std::max(0.0F, state_.health - healthDamage);
    if (state_.health <= 0.0F) {
        state_.alive = false;
        state_.velocity = {};
    }
}

void Player::heal(const float amount) noexcept
{
    if (!state_.alive || !std::isfinite(amount) || amount <= 0.0F) {
        return;
    }
    state_.health = std::min(state_.maximumHealth, state_.health + amount);
}

void Player::addArmor(const float amount) noexcept
{
    if (!state_.alive || !std::isfinite(amount) || amount <= 0.0F) {
        return;
    }
    state_.armor = std::min(state_.maximumArmor, state_.armor + amount);
}

void Player::addKeys(const std::uint32_t amount) noexcept
{
    if (!state_.alive || amount == 0U) {
        return;
    }
    state_.keys += amount;
}

bool Player::increaseMaximumHealth(
    const float amount,
    const float healing) noexcept
{
    if (!std::isfinite(amount) || !std::isfinite(healing) || amount <= 0.0F) {
        return false;
    }
    state_.maximumHealth += amount;
    if (state_.alive && healing > 0.0F) {
        state_.health = std::min(state_.maximumHealth, state_.health + healing);
    }
    return true;
}

bool Player::increaseMaximumArmor(
    const float amount,
    const float armor) noexcept
{
    if (!std::isfinite(amount) || !std::isfinite(armor) || amount <= 0.0F) {
        return false;
    }
    state_.maximumArmor += amount;
    if (state_.alive && armor > 0.0F) {
        state_.armor = std::min(state_.maximumArmor, state_.armor + armor);
    }
    return true;
}

const PlayerState& Player::state() const noexcept
{
    return state_;
}

gdp::entities::EntityId Player::entity() const noexcept
{
    return entity_;
}

wkc::core::Vec3 Player::forwardDirection() const noexcept
{
    constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0F;
    const float yaw = state_.yawDegrees * degreesToRadians;
    const float pitch = state_.pitchDegrees * degreesToRadians;
    return wkc::core::normalized({
        std::cos(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::sin(yaw) * std::cos(pitch),
    });
}

wkc::core::Vec3 Player::planarForwardDirection() const noexcept
{
    const wkc::core::Vec3 forward = forwardDirection();
    return wkc::core::normalized({forward.x, 0.0F, forward.z});
}

wkc::core::Vec3 Player::rightDirection() const noexcept
{
    const wkc::core::Vec3 forward = planarForwardDirection();
    return {-forward.z, 0.0F, forward.x};
}

wkc::core::Vec3 Player::cameraPosition() const noexcept
{
    return state_.position + wkc::core::Vec3{0.0F, eyeHeight_, 0.0F};
}

float Player::mouseSensitivity() const noexcept
{
    return mouseSensitivity_;
}

} // namespace gdp::gameplay
