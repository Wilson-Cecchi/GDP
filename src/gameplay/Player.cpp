#include "gameplay/Player.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace gdp::gameplay {
namespace {

constexpr CharacterCollider standingCollider{0.3F, 1.8F};
constexpr CharacterCollider crouchingCollider{0.3F, 1.15F};
constexpr float crouchTransitionSpeed = 7.5F;
constexpr float jumpBufferDuration = 0.12F;
constexpr float bunnyHopSpeedMultiplier = 1.6F;
constexpr float airAcceleration = 18.0F;
constexpr float airDragPerSecond = 0.12F;

wkc::core::Vec3 steerInAir(
    wkc::core::Vec3 horizontal,
    const wkc::core::Vec3 desired,
    const float targetSpeed,
    const float maximumSpeed,
    const float step) noexcept
{
    horizontal = horizontal * std::max(0.0F, 1.0F - airDragPerSecond * step);
    const float speedToAdd = targetSpeed - wkc::core::dot(horizontal, desired);
    if (wkc::core::length(desired) > 1.0e-6F && speedToAdd > 0.0F) {
        horizontal = horizontal + desired
            * std::min(speedToAdd, airAcceleration * step);
    }
    const float speed = wkc::core::length(horizontal);
    if (speed > maximumSpeed) {
        horizontal = horizontal * (maximumSpeed / speed);
    }
    return horizontal;
}

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

wkc::core::Vec2 gamepadLookDegrees(
    const wkc::core::Vec2 stick, const float elapsedSeconds,
    const float horizontalScale, const float verticalScale) noexcept
{
    if (!std::isfinite(stick.x) || !std::isfinite(stick.y)
        || !std::isfinite(elapsedSeconds) || !std::isfinite(horizontalScale)
        || !std::isfinite(verticalScale)) return {};
    constexpr float degreesPerSecond = 300.0F;
    const float seconds = std::clamp(elapsedSeconds, 0.0F, 0.05F);
    return {stick.x * degreesPerSecond * seconds * horizontalScale,
        stick.y * degreesPerSecond * seconds * verticalScale};
}

void Player::spawn(const wkc::core::Vec3 feetPosition, const float yawDegrees) noexcept
{
    state_ = {};
    jumpBufferSeconds_ = 0.0F;
    state_.position = feetPosition;
    state_.yawDegrees = std::remainder(yawDegrees, 360.0F);
    state_.grounded = false;
    collider_ = standingCollider;
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
    jumpBufferSeconds_ = 0.0F;
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

void Player::setInvertMouseY(const bool inverted) noexcept
{
    invertMouseY_ = inverted;
}

void Player::applyLook(const wkc::core::Vec2 mouseDelta) noexcept
{
    if (!state_.alive) {
        return;
    }
    state_.yawDegrees += mouseDelta.x * mouseSensitivity_;
    const float verticalDelta = mouseDelta.y * mouseSensitivity_
        * (invertMouseY_ ? 1.0F : -1.0F);
    state_.pitchDegrees = std::clamp(
        state_.pitchDegrees + verticalDelta,
        -89.0F,
        89.0F);
    state_.yawDegrees = std::remainder(state_.yawDegrees, 360.0F);
}

void Player::applyGamepadLook(const wkc::core::Vec2 degrees, const bool invertY) noexcept
{
    if (!state_.alive || !std::isfinite(degrees.x) || !std::isfinite(degrees.y)) return;
    state_.yawDegrees = std::remainder(state_.yawDegrees + degrees.x, 360.0F);
    state_.pitchDegrees = std::clamp(state_.pitchDegrees + degrees.y * (invertY ? 1.0F : -1.0F),
        -89.0F, 89.0F);
}

void Player::fixedUpdate(
    const PlayerCommand& command,
    const float fixedStepSeconds,
    const CollisionWorld& collisionWorld) noexcept
{
    if (!state_.alive || fixedStepSeconds <= 0.0F) {
        return;
    }

    if (command.crouch) {
        state_.crouching = true;
    } else if (collisionWorld.canOccupy(state_.position, standingCollider)) {
        state_.crouching = false;
    }
    const float crouchTarget = state_.crouching ? 1.0F : 0.0F;
    const float crouchStep = crouchTransitionSpeed * fixedStepSeconds;
    if (state_.crouchAmount < crouchTarget) {
        state_.crouchAmount = std::min(
            state_.crouchAmount + crouchStep, crouchTarget);
    } else {
        state_.crouchAmount = std::max(
            state_.crouchAmount - crouchStep, crouchTarget);
    }
    collider_ = {
        standingCollider.radius,
        standingCollider.height
            + ((crouchingCollider.height - standingCollider.height)
                * state_.crouchAmount),
    };

    wkc::core::Vec3 desired = planarForwardDirection()
            * std::clamp(command.moveForward, -1.0F, 1.0F)
        + rightDirection() * std::clamp(command.moveRight, -1.0F, 1.0F);
    const float desiredLength = wkc::core::length(desired);
    if (desiredLength > 1.0F) {
        desired = desired * (1.0F / desiredLength);
    }

    const float stanceSpeed = moveSpeed_
        * (state_.crouching ? crouchMoveMultiplier_ : 1.0F)
        * (command.quietWalk ? quietWalkMultiplier_ : 1.0F);
    const bool canJump = !state_.crouching
        && state_.crouchAmount <= 1.0e-4F;
    jumpBufferSeconds_ = canJump
        ? (command.jump ? jumpBufferDuration
                        : std::max(0.0F, jumpBufferSeconds_ - fixedStepSeconds))
        : 0.0F;
    bool jumpGrounded = state_.grounded;
    if (!jumpGrounded && (jumpBufferSeconds_ > 0.0F
            || state_.velocity.y == 0.0F)) {
        jumpGrounded = collisionWorld.moveCharacter(
            state_.position, {0.0F, -1.0F, 0.0F},
            1.0e-4F, collider_).grounded;
    }
    const bool launchJump = jumpBufferSeconds_ > 0.0F && jumpGrounded;
    wkc::core::Vec3 horizontal{
        state_.velocity.x, 0.0F, state_.velocity.z};
    if (launchJump) {
        // A buffered landing launches before ground movement can erase the
        // horizontal speed earned by an earlier air strafe.
        if (wkc::core::length(horizontal) < stanceSpeed) {
            horizontal = desired * stanceSpeed;
        }
        state_.velocity.y = jumpSpeed_;
        state_.grounded = false;
        jumpBufferSeconds_ = 0.0F;
    }
    if (launchJump || !jumpGrounded) {
        const float maximumSpeed = command.quietWalk || state_.crouching
            ? stanceSpeed : moveSpeed_ * bunnyHopSpeedMultiplier;
        horizontal = steerInAir(horizontal, desired,
            stanceSpeed, maximumSpeed, fixedStepSeconds);
    } else {
        // Ordinary walking retains its original immediate response and speed.
        horizontal = desired * stanceSpeed;
    }
    state_.velocity.x = horizontal.x;
    state_.velocity.z = horizontal.z;
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
    const float maximumDistance,
    const std::span<const wkc::core::Aabb> blockers) const noexcept
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
    if (nearestId) {
        for (const auto& blocker : blockers) {
            const auto distance = rayAabbDistance(cameraPosition(), forwardDirection(),
                blocker, maximumDistance);
            // The console's own static solid is allowed at the same surface.
            if (distance && *distance + 1.0e-4F < nearestDistance) return std::nullopt;
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
        jumpBufferSeconds_ = 0.0F;
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

bool Player::restoreCampaignProgress(
    const PlayerCampaignProgress& progress) noexcept
{
    if (!std::isfinite(progress.health)
        || !std::isfinite(progress.maximumHealth)
        || !std::isfinite(progress.armor)
        || !std::isfinite(progress.maximumArmor)
        || progress.health <= 0.0F || progress.maximumHealth <= 0.0F
        || progress.health > progress.maximumHealth
        || progress.armor < 0.0F || progress.maximumArmor < 0.0F
        || progress.armor > progress.maximumArmor) {
        return false;
    }
    state_.health = progress.health;
    state_.maximumHealth = progress.maximumHealth;
    state_.armor = progress.armor;
    state_.maximumArmor = progress.maximumArmor;
    state_.keys = progress.keys;
    state_.alive = true;
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
    const float eyeHeight = standingEyeHeight_
        + ((crouchingEyeHeight_ - standingEyeHeight_)
            * std::clamp(state_.crouchAmount, 0.0F, 1.0F));
    return state_.position + wkc::core::Vec3{0.0F, eyeHeight, 0.0F};
}

CharacterCollider Player::collider() const noexcept
{
    return collider_;
}

float Player::mouseSensitivity() const noexcept
{
    return mouseSensitivity_;
}

bool Player::invertMouseY() const noexcept
{
    return invertMouseY_;
}

} // namespace gdp::gameplay
