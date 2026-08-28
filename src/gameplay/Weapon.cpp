#include "gameplay/Weapon.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gdp::gameplay {

Weapon::Weapon(WeaponDefinition definition)
    : definition_(definition)
{
    definition_.damage = std::max(definition_.damage, 0.0F);
    definition_.fireInterval = std::max(definition_.fireInterval, 0.0F);
    definition_.range = std::max(definition_.range, 0.0F);
    definition_.magazineSize = std::max(definition_.magazineSize, 1U);
    magazineAmmo_ = definition_.magazineSize;
    reserveAmmo_ = definition_.reserveMaximum;
}

void Weapon::fixedUpdate(const float fixedStepSeconds) noexcept
{
    cooldownRemaining_ = std::max(
        0.0F,
        cooldownRemaining_ - std::max(fixedStepSeconds, 0.0F));
}

FireReadiness Weapon::tryFire() noexcept
{
    if (cooldownRemaining_ > 0.0F) {
        return FireReadiness::Cooldown;
    }
    if (magazineAmmo_ == 0) {
        return FireReadiness::Empty;
    }
    --magazineAmmo_;
    cooldownRemaining_ = definition_.fireInterval;
    return FireReadiness::Fired;
}

bool Weapon::reload() noexcept
{
    if (magazineAmmo_ >= definition_.magazineSize || reserveAmmo_ == 0) {
        return false;
    }
    const std::uint32_t requested = definition_.magazineSize - magazineAmmo_;
    const std::uint32_t transferred = std::min(requested, reserveAmmo_);
    magazineAmmo_ += transferred;
    reserveAmmo_ -= transferred;
    return true;
}

std::uint32_t Weapon::addReserveAmmo(const std::uint32_t amount) noexcept
{
    const std::uint32_t capacity = definition_.reserveMaximum - reserveAmmo_;
    const std::uint32_t accepted = std::min(amount, capacity);
    reserveAmmo_ += accepted;
    return accepted;
}

bool Weapon::increaseDamage(const float amount) noexcept
{
    if (!std::isfinite(amount) || amount <= 0.0F) {
        return false;
    }
    definition_.damage += amount;
    return true;
}

bool Weapon::increaseReserveCapacity(
    const std::uint32_t amount,
    const std::uint32_t bonusAmmo) noexcept
{
    if (amount == 0U
        || amount > std::numeric_limits<std::uint32_t>::max()
            - definition_.reserveMaximum) {
        return false;
    }
    definition_.reserveMaximum += amount;
    (void)addReserveAmmo(bonusAmmo);
    return true;
}

const WeaponDefinition& Weapon::definition() const noexcept
{
    return definition_;
}

std::uint32_t Weapon::magazineAmmo() const noexcept
{
    return magazineAmmo_;
}

std::uint32_t Weapon::reserveAmmo() const noexcept
{
    return reserveAmmo_;
}

std::uint32_t Weapon::reserveCapacity() const noexcept
{
    return definition_.reserveMaximum;
}

float Weapon::cooldownRemaining() const noexcept
{
    return cooldownRemaining_;
}

} // namespace gdp::gameplay
