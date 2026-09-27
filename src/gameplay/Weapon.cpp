#include "gameplay/Weapon.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace gdp::gameplay {
namespace {

bool validateWeaponSlot(
    const WeaponDefinition& definition,
    const std::uint32_t magazineAmmo,
    const std::uint32_t reserveAmmo) noexcept
{
    return std::isfinite(definition.damage)
        && std::isfinite(definition.fireInterval)
        && std::isfinite(definition.range)
        && definition.damage > 0.0F && definition.fireInterval >= 0.0F
        && definition.range > 0.0F && definition.magazineSize > 0U
        && definition.archetype <= WeaponArchetype::Carbine
        && magazineAmmo <= definition.magazineSize
        && reserveAmmo <= definition.reserveMaximum;
}

} // namespace

WeaponDefinition baseWeaponDefinition(const WeaponArchetype archetype) noexcept
{
    if (archetype == WeaponArchetype::Knife) {
        return {
            .damage = 48.0F,
            .fireInterval = 0.52F,
            .range = 1.65F,
            .magazineSize = 1U,
            .reserveMaximum = 0U,
            .archetype = WeaponArchetype::Knife,
        };
    }
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
    if (archetype == WeaponArchetype::Knife) {
        return "KNIFE";
    }
    return archetype == WeaponArchetype::Carbine ? "CARBINE" : "PISTOL";
}

WeaponSpreadProfile weaponSpreadProfile(
    const WeaponArchetype archetype) noexcept
{
    if (archetype == WeaponArchetype::Knife) {
        return {0.0F, 0.0F};
    }
    return archetype == WeaponArchetype::Carbine
        ? WeaponSpreadProfile{0.90F, 0.30F}
        : WeaponSpreadProfile{};
}

float weaponReloadDurationSeconds(
    const WeaponArchetype archetype) noexcept
{
    if (archetype == WeaponArchetype::Knife) {
        return 0.0F;
    }
    return archetype == WeaponArchetype::Carbine ? 1.05F : 0.82F;
}

float weaponSwitchDurationSeconds(
    const WeaponArchetype archetype) noexcept
{
    if (archetype == WeaponArchetype::Knife) {
        return 0.18F;
    }
    return archetype == WeaponArchetype::Carbine ? 0.32F : 0.24F;
}

std::optional<WeaponArchetype> weaponArchetypeForSlot(
    const std::uint32_t slot) noexcept
{
    if (slot == 1U) {
        return WeaponArchetype::Carbine;
    }
    if (slot == 2U) {
        return WeaponArchetype::Pistol;
    }
    if (slot == 3U) {
        return WeaponArchetype::Knife;
    }
    return std::nullopt;
}

bool weaponUsesAutomaticFire(const WeaponArchetype archetype) noexcept
{
    return archetype == WeaponArchetype::Carbine;
}

bool weaponFireRequested(
    const Weapon& weapon,
    const bool triggerPressed,
    const bool triggerHeld) noexcept
{
    if (weapon.reloading() || weapon.switching()) {
        return false;
    }
    if (weapon.definition().archetype == WeaponArchetype::Knife) {
        return triggerPressed;
    }
    return triggerPressed
        || (triggerHeld && weapon.magazineAmmo() > 0U
            && weaponUsesAutomaticFire(weapon.definition().archetype));
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
    selectedArchetype_ = definition_.archetype;
    knifeDefinition_ = baseWeaponDefinition(WeaponArchetype::Knife);
    const WeaponDefinition rangedBase
        = baseWeaponDefinition(definition_.archetype);
    knifeDefinition_.damage += std::max(
        definition_.damage - rangedBase.damage, 0.0F);
    magazineAmmo_ = definition_.magazineSize;
    reserveAmmo_ = definition_.reserveMaximum;
}

void Weapon::fixedUpdate(const float fixedStepSeconds) noexcept
{
    const float step = std::max(fixedStepSeconds, 0.0F);
    cooldownRemaining_ = std::max(
        0.0F,
        cooldownRemaining_ - step);
    knifeCooldownRemaining_ = std::max(
        0.0F, knifeCooldownRemaining_ - step);
    switchRemaining_ = std::max(0.0F, switchRemaining_ - step);
    overdriveRemaining_ = std::max(0.0F, overdriveRemaining_ - step);
    spreadRecoveryRemaining_ = std::max(
        0.0F, spreadRecoveryRemaining_ - step);
    const bool reloadCompleted = reloadRemaining_ > 0.0F
        && step >= reloadRemaining_;
    reloadRemaining_ = std::max(0.0F, reloadRemaining_ - step);
    if (reloadCompleted) {
        const std::uint32_t requested
            = definition_.magazineSize - magazineAmmo_;
        const std::uint32_t transferred = std::min(requested, reserveAmmo_);
        magazineAmmo_ += transferred;
        reserveAmmo_ -= transferred;
    }
    if (spreadRecoveryRemaining_ <= 0.0F) {
        spreadShotCount_ = 0U;
        spreadShotIndex_ = 0U;
    }
}

FireReadiness Weapon::tryFire() noexcept
{
    if (switching()) {
        return FireReadiness::Switching;
    }
    if (reloading()) {
        return FireReadiness::Reloading;
    }
    if (selectedArchetype_ == WeaponArchetype::Knife) {
        if (knifeCooldownRemaining_ > 0.0F) {
            return FireReadiness::Cooldown;
        }
        knifeCooldownRemaining_ = knifeDefinition_.fireInterval
            * (overdriveActive() ? 0.65F : 1.0F);
        spreadShotCount_ = 0U;
        spreadShotIndex_ = 0U;
        spreadRecoveryRemaining_ = 0.0F;
        return FireReadiness::Fired;
    }
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
    if (selectedArchetype_ == WeaponArchetype::Knife
        || switching() || reloading() || magazineAmmo_ >= definition_.magazineSize
        || reserveAmmo_ == 0U) {
        return false;
    }
    reloadRemaining_ = weaponReloadDurationSeconds(definition_.archetype);
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
    knifeDefinition_.damage += amount;
    if (stowedWeapon_) {
        stowedWeapon_->definition.damage += amount;
    }
    return true;
}

bool Weapon::increaseReserveCapacity(
    const std::uint32_t amount,
    const std::uint32_t bonusAmmo) noexcept
{
    if (amount == 0U
        || amount > std::numeric_limits<std::uint32_t>::max()
            - definition_.reserveMaximum
        || (stowedWeapon_
            && amount > std::numeric_limits<std::uint32_t>::max()
                - stowedWeapon_->definition.reserveMaximum)) {
        return false;
    }
    definition_.reserveMaximum += amount;
    (void)addReserveAmmo(bonusAmmo);
    if (stowedWeapon_) {
        stowedWeapon_->definition.reserveMaximum += amount;
        const std::uint32_t capacity
            = stowedWeapon_->definition.reserveMaximum
            - stowedWeapon_->reserveAmmo;
        stowedWeapon_->reserveAmmo += std::min(bonusAmmo, capacity);
    }
    return true;
}

bool Weapon::equip(const WeaponArchetype archetype) noexcept
{
    if (archetype > WeaponArchetype::Carbine
        || ownsWeapon(archetype) || switching()) {
        return false;
    }
    const WeaponCampaignProgress::StowedWeapon previous{
        definition_, magazineAmmo_, reserveAmmo_};
    const WeaponDefinition currentBase = baseWeaponDefinition(definition_.archetype);
    WeaponDefinition replacement = baseWeaponDefinition(archetype);
    replacement.damage += std::max(definition_.damage - currentBase.damage, 0.0F);
    replacement.reserveMaximum += definition_.reserveMaximum > currentBase.reserveMaximum
        ? definition_.reserveMaximum - currentBase.reserveMaximum : 0U;
    definition_ = replacement;
    magazineAmmo_ = definition_.magazineSize;
    reserveAmmo_ = definition_.reserveMaximum;
    cooldownRemaining_ = 0.0F;
    reloadRemaining_ = 0.0F;
    switchRemaining_ = weaponSwitchDurationSeconds(definition_.archetype);
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
    stowedWeapon_ = previous;
    lastSelectedArchetype_ = selectedArchetype_;
    selectedArchetype_ = definition_.archetype;
    return true;
}

bool Weapon::switchTo(const WeaponArchetype archetype) noexcept
{
    if (archetype > WeaponArchetype::Knife
        || archetype == selectedArchetype_ || !ownsWeapon(archetype)) {
        return false;
    }
    if (archetype != WeaponArchetype::Knife
        && archetype != definition_.archetype) {
        const WeaponCampaignProgress::StowedWeapon previous{
            definition_, magazineAmmo_, reserveAmmo_};
        definition_ = stowedWeapon_->definition;
        magazineAmmo_ = stowedWeapon_->magazineAmmo;
        reserveAmmo_ = stowedWeapon_->reserveAmmo;
        stowedWeapon_ = previous;
        cooldownRemaining_ = 0.0F;
    }
    lastSelectedArchetype_ = selectedArchetype_;
    selectedArchetype_ = archetype;
    reloadRemaining_ = 0.0F;
    switchRemaining_ = weaponSwitchDurationSeconds(selectedArchetype_);
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
    return true;
}

bool Weapon::cycleWeapon() noexcept
{
    const std::optional<WeaponArchetype> next = nextWeaponArchetype();
    return next && switchTo(*next);
}

std::optional<WeaponArchetype> Weapon::nextWeaponArchetype() const noexcept
{
    constexpr std::array order{
        WeaponArchetype::Carbine,
        WeaponArchetype::Pistol,
        WeaponArchetype::Knife,
    };
    const auto current = std::find(
        order.begin(), order.end(), selectedArchetype_);
    const std::size_t currentIndex = current == order.end()
        ? 0U : static_cast<std::size_t>(current - order.begin());
    for (std::size_t offset = 1U; offset <= order.size(); ++offset) {
        const WeaponArchetype candidate
            = order[(currentIndex + offset) % order.size()];
        if (ownsWeapon(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::optional<WeaponArchetype> Weapon::previousWeaponArchetype() const noexcept
{
    constexpr std::array order{
        WeaponArchetype::Carbine,
        WeaponArchetype::Pistol,
        WeaponArchetype::Knife,
    };
    const auto current = std::find(
        order.begin(), order.end(), selectedArchetype_);
    const std::size_t currentIndex = current == order.end()
        ? 0U : static_cast<std::size_t>(current - order.begin());
    for (std::size_t offset = 1U; offset <= order.size(); ++offset) {
        const WeaponArchetype candidate
            = order[(currentIndex + order.size() - offset) % order.size()];
        if (ownsWeapon(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::optional<WeaponArchetype> Weapon::lastWeaponArchetype() const noexcept
{
    if (lastSelectedArchetype_
        && *lastSelectedArchetype_ != selectedArchetype_
        && ownsWeapon(*lastSelectedArchetype_)) {
        return lastSelectedArchetype_;
    }
    return std::nullopt;
}

bool Weapon::ownsWeapon(const WeaponArchetype archetype) const noexcept
{
    return archetype == WeaponArchetype::Knife
        || (archetype <= WeaponArchetype::Carbine
        && (definition_.archetype == archetype
            || (stowedWeapon_
                && stowedWeapon_->definition.archetype == archetype)));
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
    if (!validateWeaponCampaignProgress(progress)) {
        return false;
    }
    definition_ = progress.definition;
    magazineAmmo_ = progress.magazineAmmo;
    reserveAmmo_ = progress.reserveAmmo;
    stowedWeapon_ = progress.stowed;
    selectedArchetype_ = definition_.archetype;
    lastSelectedArchetype_.reset();
    knifeDefinition_ = baseWeaponDefinition(WeaponArchetype::Knife);
    const WeaponDefinition rangedBase
        = baseWeaponDefinition(definition_.archetype);
    knifeDefinition_.damage += std::max(
        definition_.damage - rangedBase.damage, 0.0F);
    cooldownRemaining_ = 0.0F;
    knifeCooldownRemaining_ = 0.0F;
    reloadRemaining_ = 0.0F;
    switchRemaining_ = 0.0F;
    overdriveRemaining_ = 0.0F;
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
    return true;
}

const WeaponDefinition& Weapon::definition() const noexcept
{
    return selectedArchetype_ == WeaponArchetype::Knife
        ? knifeDefinition_ : definition_;
}

std::uint32_t Weapon::magazineAmmo() const noexcept
{
    return selectedArchetype_ == WeaponArchetype::Knife ? 0U : magazineAmmo_;
}

std::uint32_t Weapon::reserveAmmo() const noexcept
{
    return selectedArchetype_ == WeaponArchetype::Knife ? 0U : reserveAmmo_;
}

std::uint32_t Weapon::reserveCapacity() const noexcept
{
    return selectedArchetype_ == WeaponArchetype::Knife
        ? 0U : definition_.reserveMaximum;
}

float Weapon::cooldownRemaining() const noexcept
{
    return selectedArchetype_ == WeaponArchetype::Knife
        ? knifeCooldownRemaining_ : cooldownRemaining_;
}

float Weapon::reloadRemaining() const noexcept
{
    return reloadRemaining_;
}

float Weapon::reloadProgress() const noexcept
{
    if (!reloading()) {
        return 0.0F;
    }
    return std::clamp(
        1.0F - (reloadRemaining_
            / weaponReloadDurationSeconds(definition_.archetype)),
        0.0F, 1.0F);
}

bool Weapon::reloading() const noexcept
{
    return reloadRemaining_ > 0.0F;
}

float Weapon::switchRemaining() const noexcept
{
    return switchRemaining_;
}

float Weapon::switchStrength() const noexcept
{
    if (!switching()) {
        return 0.0F;
    }
    return std::clamp(
        switchRemaining_
            / weaponSwitchDurationSeconds(selectedArchetype_),
        0.0F, 1.0F);
}

bool Weapon::switching() const noexcept
{
    return switchRemaining_ > 0.0F;
}

float Weapon::damage() const noexcept
{
    return definition().damage * (overdriveActive() ? 1.5F : 1.0F);
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
    if (selectedArchetype_ == WeaponArchetype::Knife) {
        return 0.0F;
    }
    const float maximumShots = definition_.archetype == WeaponArchetype::Carbine
        ? 11.0F : 7.0F;
    return std::clamp(
        static_cast<float>(spreadShotCount_) / maximumShots, 0.0F, 1.0F);
}

WeaponCampaignProgress Weapon::campaignProgress() const noexcept
{
    return {definition_, magazineAmmo_, reserveAmmo_, stowedWeapon_};
}

bool validateWeaponCampaignProgress(
    const WeaponCampaignProgress& progress) noexcept
{
    if (!validateWeaponSlot(
            progress.definition, progress.magazineAmmo, progress.reserveAmmo)) {
        return false;
    }
    return !progress.stowed
        || (progress.stowed->definition.archetype
                != progress.definition.archetype
            && validateWeaponSlot(
                progress.stowed->definition,
                progress.stowed->magazineAmmo,
                progress.stowed->reserveAmmo));
}

} // namespace gdp::gameplay
