#include "gameplay/Weapon.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gdp::gameplay {

WeaponDefinition baseWeaponDefinition(const WeaponArchetype archetype) noexcept
{
    if (archetype == WeaponArchetype::Carbine) {
        return {
            .damage = 26.0F,
            .fireInterval = 0.13F,
            .range = 34.0F,
            .magazineSize = 12U,
            .reserveMaximum = 48U,
            .archetype = WeaponArchetype::Carbine,
        };
    }
    return {};
}

const char* weaponArchetypeName(const WeaponArchetype archetype) noexcept
{
    return archetype == WeaponArchetype::Carbine ? "CARBINE" : "PISTOL";
}

WeaponSpreadProfile weaponSpreadProfile(
    const WeaponArchetype archetype) noexcept
{
    return archetype == WeaponArchetype::Carbine
        ? WeaponSpreadProfile{0.90F, 0.30F}
        : WeaponSpreadProfile{};
}

Weapon::Weapon(WeaponDefinition definition)
    : definition_(definition)
{
    if (definition_.archetype > WeaponArchetype::Carbine) {
        definition_.archetype = WeaponArchetype::Pistol;
    }
    definition_.damage = std::max(definition_.damage, 0.0F);
    definition_.fireInterval = std::max(definition_.fireInterval, 0.0F);
    definition_.range = std::max(definition_.range, 0.0F);
    definition_.magazineSize = std::max(definition_.magazineSize, 1U);
    magazineAmmo_ = definition_.magazineSize;
    reserveAmmo_ = definition_.reserveMaximum;
}

void Weapon::fixedUpdate(const float fixedStepSeconds) noexcept
{
    const float step = std::max(fixedStepSeconds, 0.0F);
    cooldownRemaining_ = std::max(
        0.0F,
        cooldownRemaining_ - step);
    overdriveRemaining_ = std::max(0.0F, overdriveRemaining_ - step);
    spreadRecoveryRemaining_ = std::max(
        0.0F, spreadRecoveryRemaining_ - step);
    if (spreadRecoveryRemaining_ <= 0.0F) {
        spreadShotCount_ = 0U;
        spreadShotIndex_ = 0U;
    }
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
    cooldownRemaining_ = definition_.fireInterval
        * (overdriveActive() ? 0.65F : 1.0F);
    spreadShotIndex_ = spreadShotCount_;
    if (spreadShotCount_ < std::numeric_limits<std::uint32_t>::max()) {
        ++spreadShotCount_;
    }
    spreadRecoveryRemaining_
        = weaponSpreadProfile(definition_.archetype).recoverySeconds;
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
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
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

bool Weapon::equip(const WeaponArchetype archetype) noexcept
{
    if (archetype > WeaponArchetype::Carbine
        || archetype == definition_.archetype) {
        return false;
    }
    const WeaponDefinition currentBase = baseWeaponDefinition(definition_.archetype);
    WeaponDefinition replacement = baseWeaponDefinition(archetype);
    replacement.damage += std::max(definition_.damage - currentBase.damage, 0.0F);
    replacement.reserveMaximum += definition_.reserveMaximum > currentBase.reserveMaximum
        ? definition_.reserveMaximum - currentBase.reserveMaximum : 0U;
    definition_ = replacement;
    magazineAmmo_ = definition_.magazineSize;
    reserveAmmo_ = definition_.reserveMaximum;
    cooldownRemaining_ = 0.0F;
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
    return true;
}

bool Weapon::activateOverdrive(const float durationSeconds) noexcept
{
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0F
        || overdriveActive()) {
        return false;
    }
    overdriveRemaining_ = durationSeconds;
    return true;
}

bool Weapon::restoreCampaignProgress(
    const WeaponCampaignProgress& progress) noexcept
{
    const WeaponDefinition& definition = progress.definition;
    if (!std::isfinite(definition.damage)
        || !std::isfinite(definition.fireInterval)
        || !std::isfinite(definition.range)
        || definition.damage <= 0.0F || definition.fireInterval < 0.0F
        || definition.range <= 0.0F || definition.magazineSize == 0U
        || definition.archetype > WeaponArchetype::Carbine
        || progress.magazineAmmo > definition.magazineSize
        || progress.reserveAmmo > definition.reserveMaximum) {
        return false;
    }
    definition_ = definition;
    magazineAmmo_ = progress.magazineAmmo;
    reserveAmmo_ = progress.reserveAmmo;
    cooldownRemaining_ = 0.0F;
    overdriveRemaining_ = 0.0F;
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
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

float Weapon::damage() const noexcept
{
    return definition_.damage * (overdriveActive() ? 1.5F : 1.0F);
}

float Weapon::overdriveRemaining() const noexcept
{
    return overdriveRemaining_;
}

bool Weapon::overdriveActive() const noexcept
{
    return overdriveRemaining_ > 0.0F;
}

std::uint32_t Weapon::spreadShotIndex() const noexcept
{
    return spreadShotIndex_;
}

float Weapon::spreadRecoveryRemaining() const noexcept
{
    return spreadRecoveryRemaining_;
}

float Weapon::spreadStrength() const noexcept
{
    const float maximumShots = definition_.archetype == WeaponArchetype::Carbine
        ? 11.0F : 7.0F;
    return std::clamp(
        static_cast<float>(spreadShotCount_) / maximumShots, 0.0F, 1.0F);
}

} // namespace gdp::gameplay
