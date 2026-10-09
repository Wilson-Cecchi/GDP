#include "gameplay/Weapon.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace gdp::gameplay {
namespace {

bool inheritReserveCapacity(const WeaponDefinition& source, WeaponDefinition& target) noexcept
{
    const auto base = baseWeaponDefinition(source.archetype).reserveMaximum;
    const auto bonus = source.reserveMaximum > base ? source.reserveMaximum - base : 0U;
    const auto sourceStep = source.archetype == WeaponArchetype::Shotgun ? 4U : 1U;
    const auto targetStep = target.archetype == WeaponArchetype::Shotgun ? 4U : 1U;
    const std::uint64_t ranks = bonus / sourceStep + (bonus % sourceStep != 0U ? 1U : 0U);
    const auto capacity = static_cast<std::uint64_t>(target.reserveMaximum) + ranks * targetStep;
    if (capacity > std::numeric_limits<std::uint32_t>::max()) return false;
    target.reserveMaximum = static_cast<std::uint32_t>(capacity);
    return true;
}

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
        && isFirearmArchetype(definition.archetype)
        && magazineAmmo <= definition.magazineSize
        && reserveAmmo <= definition.reserveMaximum;
}

} // namespace

bool isFirearmArchetype(const WeaponArchetype archetype) noexcept
{
    return archetype < WeaponArchetype::Count && archetype != WeaponArchetype::Knife;
}

bool isPrimaryWeapon(const WeaponArchetype archetype) noexcept
{
    return isFirearmArchetype(archetype) && archetype != WeaponArchetype::Pistol;
}

bool weaponUsesSpareMagazines(const WeaponArchetype archetype) noexcept
{
    return isFirearmArchetype(archetype) && archetype != WeaponArchetype::Shotgun;
}

std::uint32_t reserveUnitsFromRounds(
    const WeaponDefinition& definition, const std::uint32_t rounds) noexcept
{
    if (!weaponUsesSpareMagazines(definition.archetype)) return rounds;
    const auto size = std::max(definition.magazineSize, 1U);
    return rounds / size + (rounds % size != 0U ? 1U : 0U);
}

std::uint8_t weaponProjectileCount(const WeaponArchetype archetype) noexcept
{
    return archetype == WeaponArchetype::Shotgun ? 6U : 1U;
}

float weaponAimDurationSeconds(const WeaponArchetype archetype) noexcept
{
    return archetype == WeaponArchetype::Sniper ? 0.35F : 0.125F;
}

float weaponAimFovScale(const WeaponArchetype archetype) noexcept
{
    return archetype == WeaponArchetype::Sniper ? 0.35F : 0.78F;
}

float advanceWeaponAimBlend(const float current, const float seconds,
    const WeaponArchetype archetype, const bool requested, const bool blocked) noexcept
{
    if (blocked || !isFirearmArchetype(archetype)) return 0.0F;
    const float blend = std::isfinite(current) ? std::clamp(current, 0.0F, 1.0F) : 0.0F;
    if (!std::isfinite(seconds) || seconds <= 0.0F) return blend;
    return std::clamp(blend + (requested ? 1.0F : -1.0F)
        * seconds / weaponAimDurationSeconds(archetype), 0.0F, 1.0F);
}

WeaponDefinition baseWeaponDefinition(const WeaponArchetype archetype) noexcept
{
    if (archetype == WeaponArchetype::Sniper) return {65.0F, 1.20F, 90.0F, 5U, 3U, archetype};
    // Shotgun damage is per pellet; one shell emits six rays.
    if (archetype == WeaponArchetype::Shotgun) return {14.0F, 0.85F, 18.0F, 6U, 24U, archetype};
    if (archetype == WeaponArchetype::SubmachineGun) return {16.0F, 0.085F, 28.0F, 24U, 4U, archetype};
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
            .reserveMaximum = 4U,
            .archetype = WeaponArchetype::Carbine,
        };
    }
    return {};
}

const char* weaponArchetypeName(const WeaponArchetype archetype) noexcept
{
    if (archetype == WeaponArchetype::Sniper) return "SNIPER";
    if (archetype == WeaponArchetype::Shotgun) return "SHOTGUN";
    if (archetype == WeaponArchetype::SubmachineGun) return "SMG";
    if (archetype == WeaponArchetype::Knife) {
        return "KNIFE";
    }
    return archetype == WeaponArchetype::Carbine ? "CARBINE" : "PISTOL";
}

WeaponSpreadProfile weaponSpreadProfile(
    const WeaponArchetype archetype) noexcept
{
    if (archetype == WeaponArchetype::Sniper) return {0.12F, 1.20F};
    if (archetype == WeaponArchetype::Shotgun) return {5.0F, 0.85F};
    if (archetype == WeaponArchetype::SubmachineGun) return {1.8F, 0.4F};
    if (archetype == WeaponArchetype::Knife) {
        return {0.0F, 0.0F};
    }
    return archetype == WeaponArchetype::Carbine
        ? WeaponSpreadProfile{0.90F, 0.30F}
        : WeaponSpreadProfile{};
}

wkc::core::Vec2 weaponRecoilKickDegrees(
    const WeaponArchetype archetype, const std::uint32_t shotIndex,
    const float aimBlend, const bool crouching) noexcept
{
    if (!isFirearmArchetype(archetype)) return {};

    // A repeatable side-to-side spray: the weapon climbs on every shot, while
    // horizontal corrections alternate during a sustained burst.
    constexpr std::array<float, 12> horizontalPattern{
        0.0F, 0.22F, 0.35F, 0.25F, -0.10F, -0.35F,
        -0.42F, -0.20F, 0.20F, 0.45F, 0.35F, -0.15F};
    float vertical = 0.76F;
    float horizontal = 0.22F;
    switch (archetype) {
    case WeaponArchetype::Carbine: vertical = 0.61F; horizontal = 0.48F; break;
    case WeaponArchetype::SubmachineGun: vertical = 0.45F; horizontal = 0.36F; break;
    case WeaponArchetype::Shotgun: vertical = 1.85F; horizontal = 0.42F; break;
    case WeaponArchetype::Sniper: vertical = 2.10F; horizontal = 0.34F; break;
    default: break;
    }
    const float safeAim = std::isfinite(aimBlend)
        ? std::clamp(aimBlend, 0.0F, 1.0F) : 0.0F;
    const float scale = (1.0F - 0.24F * safeAim) * (crouching ? 0.85F : 1.0F);
    return {horizontal * horizontalPattern[shotIndex % horizontalPattern.size()] * scale,
        vertical * scale};
}

float weaponReloadDurationSeconds(
    const WeaponArchetype archetype) noexcept
{
    if (archetype == WeaponArchetype::Sniper) return 2.0F;
    if (archetype == WeaponArchetype::Shotgun) return 6.0F * shotgunShellReloadSeconds + shotgunReadySeconds;
    if (archetype == WeaponArchetype::SubmachineGun) return 1.4F;
    if (archetype == WeaponArchetype::Knife) {
        return 0.0F;
    }
    return archetype == WeaponArchetype::Carbine ? 1.05F : 0.82F;
}

float weaponSwitchDurationSeconds(
    const WeaponArchetype archetype) noexcept
{
    if (archetype == WeaponArchetype::Sniper) return 0.45F;
    if (archetype == WeaponArchetype::Shotgun) return 0.38F;
    if (archetype == WeaponArchetype::SubmachineGun) return 0.28F;
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
    return archetype == WeaponArchetype::Carbine || archetype == WeaponArchetype::SubmachineGun;
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
    if (!isFirearmArchetype(definition_.archetype)) {
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
        definition_.damage - rangedBase.damage, 0.0F) * weaponProjectileCount(definition_.archetype);
    magazineAmmo_ = definition_.magazineSize;
    reserveAmmo_ = definition_.reserveMaximum;
}

void Weapon::fixedUpdate(const float fixedStepSeconds) noexcept
{
    const float step = std::max(fixedStepSeconds, 0.0F);
    cooldownRemaining_ = std::max(
        0.0F,
        cooldownRemaining_ - step);
    stowedCooldownRemaining_ = std::max(0.0F, stowedCooldownRemaining_ - step);
    knifeCooldownRemaining_ = std::max(
        0.0F, knifeCooldownRemaining_ - step);
    switchRemaining_ = std::max(0.0F, switchRemaining_ - step);
    overdriveRemaining_ = std::max(0.0F, overdriveRemaining_ - step);
    spreadRecoveryRemaining_ = std::max(
        0.0F, spreadRecoveryRemaining_ - step);
    const bool wasReloading = reloadRemaining_ > 0.0F;
    const bool reloadCompleted = wasReloading
        && step >= reloadRemaining_;
    reloadRemaining_ = std::max(0.0F, reloadRemaining_ - step);
    if (wasReloading && definition_.archetype == WeaponArchetype::Shotgun) {
        const double elapsed = static_cast<double>(reloadDuration_ - reloadRemaining_);
        const auto inserted = reloadCompleted ? reloadRoundCount_
            : static_cast<std::uint32_t>(std::clamp(
                std::floor((elapsed + 0.000001) / shotgunShellReloadSeconds),
                0.0, static_cast<double>(reloadRoundCount_)));
        if (inserted > reloadInsertedRounds_) {
            const auto transferred = std::min(inserted - reloadInsertedRounds_, reserveAmmo_);
            magazineAmmo_ += transferred;
            reserveAmmo_ -= transferred;
            reloadInsertedRounds_ += transferred;
        }
    }
    // Ejection is irreversible, even if a switch/drop interrupts insertion.
    if (wasReloading && weaponUsesSpareMagazines(definition_.archetype)
        && reloadRemaining_ <= weaponReloadDurationSeconds(definition_.archetype)
            * (1.0F - magazineReleaseProgress)) {
        magazineAmmo_ = 0U;
    }
    if (reloadCompleted && weaponUsesSpareMagazines(definition_.archetype)
        && reserveAmmo_ > 0U) {
        --reserveAmmo_;
        magazineAmmo_ = definition_.magazineSize;
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
    reloadInsertedRounds_ = 0U;
    reloadRoundCount_ = definition_.archetype == WeaponArchetype::Shotgun
        ? std::min(definition_.magazineSize - magazineAmmo_, reserveAmmo_) : 1U;
    reloadDuration_ = definition_.archetype == WeaponArchetype::Shotgun
        ? static_cast<float>(reloadRoundCount_) * shotgunShellReloadSeconds + shotgunReadySeconds
        : weaponReloadDurationSeconds(definition_.archetype);
    reloadRemaining_ = reloadDuration_;
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
    return true;
}

std::uint32_t Weapon::addReserveAmmo(const std::uint32_t amount) noexcept
{
    if (!hasFirearm_) {
        return 0U;
    }
    const std::uint32_t capacity = definition_.reserveMaximum - reserveAmmo_;
    const std::uint32_t accepted = std::min(amount, capacity);
    reserveAmmo_ += accepted;
    return accepted;
}

std::uint32_t Weapon::addReserveAmmoFor(const WeaponArchetype archetype, const std::uint32_t amount) noexcept
{
    if (!hasFirearm_) return 0U;
    if (definition_.archetype == archetype) return addReserveAmmo(amount);
    if (!stowedWeapon_ || stowedWeapon_->definition.archetype != archetype) return 0U;
    const auto accepted = std::min(amount,
        stowedWeapon_->definition.reserveMaximum - stowedWeapon_->reserveAmmo);
    stowedWeapon_->reserveAmmo += accepted;
    return accepted;
}

std::optional<WeaponArchetype> Weapon::archetypeForSlot(const std::uint32_t slot) const noexcept
{
    if (slot != 1U) return weaponArchetypeForSlot(slot);
    if (hasFirearm_ && isPrimaryWeapon(definition_.archetype)) return definition_.archetype;
    if (stowedWeapon_ && isPrimaryWeapon(stowedWeapon_->definition.archetype))
        return stowedWeapon_->definition.archetype;
    return std::nullopt;
}

bool Weapon::increaseDamage(const float amount) noexcept
{
    if (!std::isfinite(amount) || amount <= 0.0F) {
        return false;
    }
    definition_.damage += amount / weaponProjectileCount(definition_.archetype);
    knifeDefinition_.damage += amount;
    if (stowedWeapon_) {
        stowedWeapon_->definition.damage += amount / weaponProjectileCount(stowedWeapon_->definition.archetype);
    }
    return true;
}

bool Weapon::increaseReserveCapacity(
    const std::uint32_t amount,
    const std::uint32_t bonusAmmo) noexcept
{
    const auto activeAmount = reserveUnitsFromRounds(definition_, amount);
    const auto stowedAmount = stowedWeapon_
        ? reserveUnitsFromRounds(stowedWeapon_->definition, amount) : 0U;
    if (amount == 0U
        || activeAmount > std::numeric_limits<std::uint32_t>::max()
            - definition_.reserveMaximum
        || (stowedWeapon_
            && stowedAmount > std::numeric_limits<std::uint32_t>::max()
                - stowedWeapon_->definition.reserveMaximum)) {
        return false;
    }
    definition_.reserveMaximum += activeAmount;
    (void)addReserveAmmo(reserveUnitsFromRounds(definition_, bonusAmmo));
    if (stowedWeapon_) {
        stowedWeapon_->definition.reserveMaximum += stowedAmount;
        const std::uint32_t capacity
            = stowedWeapon_->definition.reserveMaximum
            - stowedWeapon_->reserveAmmo;
        stowedWeapon_->reserveAmmo += std::min(
            reserveUnitsFromRounds(stowedWeapon_->definition, bonusAmmo), capacity);
    }
    return true;
}

bool Weapon::equip(const WeaponArchetype archetype) noexcept
{
    if (!isFirearmArchetype(archetype)
        || ownsWeapon(archetype) || switching()
        || (isPrimaryWeapon(archetype) && archetypeForSlot(1U))) {
        return false;
    }
    const WeaponCampaignProgress::StowedWeapon previous{
        definition_, magazineAmmo_, reserveAmmo_};
    const WeaponDefinition currentBase = baseWeaponDefinition(definition_.archetype);
    WeaponDefinition replacement = baseWeaponDefinition(archetype);
    replacement.damage += std::max(definition_.damage - currentBase.damage, 0.0F)
        * weaponProjectileCount(definition_.archetype) / weaponProjectileCount(archetype);
    if (!inheritReserveCapacity(definition_, replacement)) return false;
    stowedCooldownRemaining_ = hasFirearm_ ? cooldownRemaining_ : 0.0F;
    definition_ = replacement;
    magazineAmmo_ = definition_.magazineSize;
    reserveAmmo_ = definition_.reserveMaximum;
    cooldownRemaining_ = 0.0F;
    reloadRemaining_ = 0.0F;
    switchRemaining_ = weaponSwitchDurationSeconds(definition_.archetype);
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
    stowedWeapon_ = hasFirearm_
        ? std::optional<WeaponCampaignProgress::StowedWeapon>{previous}
        : std::nullopt;
    hasFirearm_ = true;
    lastSelectedArchetype_ = selectedArchetype_;
    selectedArchetype_ = definition_.archetype;
    return true;
}

bool Weapon::equipRecovered(
    const WeaponArchetype archetype,
    const std::uint32_t magazineAmmo,
    const std::uint32_t reserveAmmo) noexcept
{
    if (!isFirearmArchetype(archetype)
        || ownsWeapon(archetype) || switching()) {
        return false;
    }
    const WeaponDefinition currentBase = baseWeaponDefinition(
        definition_.archetype);
    WeaponDefinition recovered = baseWeaponDefinition(archetype);
    recovered.damage += std::max(
        definition_.damage - currentBase.damage, 0.0F)
        * weaponProjectileCount(definition_.archetype) / weaponProjectileCount(archetype);
    if (!inheritReserveCapacity(definition_, recovered)) return false;
    if (!validateWeaponSlot(recovered, magazineAmmo, reserveAmmo)
        || !equip(archetype)) {
        return false;
    }
    magazineAmmo_ = magazineAmmo;
    reserveAmmo_ = reserveAmmo;
    return true;
}

std::optional<WeaponCampaignProgress::StowedWeapon>
Weapon::dropSelectedFirearm() noexcept
{
    if (selectedArchetype_ == WeaponArchetype::Knife
        || !hasFirearm_ || switching()) {
        return std::nullopt;
    }
    const WeaponCampaignProgress::StowedWeapon dropped{
        definition_, magazineAmmo_, reserveAmmo_};
    if (stowedWeapon_) {
        definition_ = stowedWeapon_->definition;
        magazineAmmo_ = stowedWeapon_->magazineAmmo;
        reserveAmmo_ = stowedWeapon_->reserveAmmo;
        stowedWeapon_.reset();
        selectedArchetype_ = definition_.archetype;
    } else {
        hasFirearm_ = false;
        magazineAmmo_ = 0U;
        reserveAmmo_ = 0U;
        selectedArchetype_ = WeaponArchetype::Knife;
    }
    lastSelectedArchetype_.reset();
    cooldownRemaining_ = hasFirearm_ ? stowedCooldownRemaining_ : 0.0F;
    stowedCooldownRemaining_ = 0.0F;
    reloadRemaining_ = 0.0F;
    switchRemaining_ = weaponSwitchDurationSeconds(selectedArchetype_);
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
    return dropped;
}

bool Weapon::switchTo(const WeaponArchetype archetype) noexcept
{
    if (archetype >= WeaponArchetype::Count
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
        std::swap(cooldownRemaining_, stowedCooldownRemaining_);
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
        WeaponArchetype::Sniper,
        WeaponArchetype::Shotgun,
        WeaponArchetype::SubmachineGun,
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
        WeaponArchetype::Sniper,
        WeaponArchetype::Shotgun,
        WeaponArchetype::SubmachineGun,
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
        || (hasFirearm_ && isFirearmArchetype(archetype)
        && (definition_.archetype == archetype
            || (stowedWeapon_
                && stowedWeapon_->definition.archetype == archetype)));
}

bool Weapon::hasFirearm() const noexcept
{
    return hasFirearm_;
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
    hasFirearm_ = progress.firearmOwned;
    selectedArchetype_ = hasFirearm_
        ? definition_.archetype : WeaponArchetype::Knife;
    lastSelectedArchetype_.reset();
    knifeDefinition_ = baseWeaponDefinition(WeaponArchetype::Knife);
    const WeaponDefinition rangedBase
        = baseWeaponDefinition(definition_.archetype);
    knifeDefinition_.damage += std::max(
        definition_.damage - rangedBase.damage, 0.0F) * weaponProjectileCount(definition_.archetype);
    cooldownRemaining_ = 0.0F;
    stowedCooldownRemaining_ = 0.0F;
    knifeCooldownRemaining_ = 0.0F;
    reloadRemaining_ = 0.0F;
    switchRemaining_ = 0.0F;
    overdriveRemaining_ = 0.0F;
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
    return true;
}

void Weapon::restockAllFirearms() noexcept
{
    if (!hasFirearm_) return;
    magazineAmmo_ = definition_.magazineSize;
    reserveAmmo_ = definition_.reserveMaximum;
    if (stowedWeapon_) {
        stowedWeapon_->magazineAmmo = stowedWeapon_->definition.magazineSize;
        stowedWeapon_->reserveAmmo = stowedWeapon_->definition.reserveMaximum;
    }
    reloadRemaining_ = 0.0F;
    reloadDuration_ = 0.0F;
    reloadRoundCount_ = 0U;
    reloadInsertedRounds_ = 0U;
    spreadShotCount_ = 0U;
    spreadShotIndex_ = 0U;
    spreadRecoveryRemaining_ = 0.0F;
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
        1.0F - (reloadRemaining_ / reloadDuration_),
        0.0F, 1.0F);
}

std::uint32_t Weapon::reloadRoundCount() const noexcept
{
    return reloading() ? reloadRoundCount_ : 0U;
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
    return {definition_, magazineAmmo_, reserveAmmo_, stowedWeapon_, hasFirearm_};
}

bool validateWeaponCampaignProgress(
    const WeaponCampaignProgress& progress) noexcept
{
    if (!validateWeaponSlot(
            progress.definition, progress.magazineAmmo, progress.reserveAmmo)) {
        return false;
    }
    if (!progress.firearmOwned) {
        return !progress.stowed && progress.magazineAmmo == 0U
            && progress.reserveAmmo == 0U;
    }
    return !progress.stowed
        || (progress.stowed->definition.archetype
                != progress.definition.archetype
            && isPrimaryWeapon(progress.stowed->definition.archetype)
                != isPrimaryWeapon(progress.definition.archetype)
            && validateWeaponSlot(
                progress.stowed->definition,
                progress.stowed->magazineAmmo,
                progress.stowed->reserveAmmo));
}

} // namespace gdp::gameplay
