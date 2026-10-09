#pragma once

#include "core/Math.hpp"

#include <cstdint>
#include <optional>

namespace gdp::gameplay {

enum class WeaponArchetype : std::uint8_t {
    Pistol,
    Carbine,
    Knife,
    Sniper,
    Shotgun,
    SubmachineGun,
    Count,
};

struct WeaponDefinition {
    float damage{34.0F};
    float fireInterval{0.22F};
    float range{30.0F};
    std::uint32_t magazineSize{8};
    // Full spare magazines, or individual shells for the tube-fed shotgun.
    std::uint32_t reserveMaximum{4};
    WeaponArchetype archetype{WeaponArchetype::Pistol};

    constexpr bool operator==(const WeaponDefinition&) const noexcept = default;
};

struct WeaponCampaignProgress {
    WeaponDefinition definition{};
    std::uint32_t magazineAmmo{0U};
    std::uint32_t reserveAmmo{0U};
    struct StowedWeapon {
        WeaponDefinition definition{};
        std::uint32_t magazineAmmo{0U};
        std::uint32_t reserveAmmo{0U};

        constexpr bool operator==(const StowedWeapon&) const noexcept = default;
    };
    std::optional<StowedWeapon> stowed;
    bool firearmOwned{true};

    constexpr bool operator==(const WeaponCampaignProgress&) const noexcept = default;
};

struct WeaponSpreadProfile {
    float maximumDegrees{0.55F};
    float recoverySeconds{0.34F};

    constexpr bool operator==(const WeaponSpreadProfile&) const noexcept = default;
};

enum class FireReadiness : std::uint8_t {
    Fired,
    Cooldown,
    Reloading,
    Switching,
    Empty,
};

class Weapon {
public:
    explicit Weapon(WeaponDefinition definition = {});

    void fixedUpdate(float fixedStepSeconds) noexcept;
    [[nodiscard]] FireReadiness tryFire() noexcept;
    [[nodiscard]] FireReadiness tryQuickMelee() noexcept;
    [[nodiscard]] bool reload() noexcept;
    // Reserve units: complete magazines, except shotgun shells.
    [[nodiscard]] std::uint32_t addReserveAmmo(std::uint32_t amount) noexcept;
    [[nodiscard]] std::uint32_t addReserveAmmoFor(WeaponArchetype archetype, std::uint32_t amount) noexcept;
    [[nodiscard]] std::optional<WeaponArchetype> archetypeForSlot(std::uint32_t slot) const noexcept;
    [[nodiscard]] bool increaseDamage(float amount) noexcept;
    // Loot/progression budgets remain expressed in rounds; converted per gun.
    [[nodiscard]] bool increaseReserveCapacity(
        std::uint32_t amount,
        std::uint32_t bonusAmmo = 0U) noexcept;
    [[nodiscard]] bool equip(WeaponArchetype archetype) noexcept;
    [[nodiscard]] bool equipRecovered(
        WeaponArchetype archetype,
        std::uint32_t magazineAmmo,
        std::uint32_t reserveAmmo) noexcept;
    [[nodiscard]] std::optional<WeaponCampaignProgress::StowedWeapon>
        dropSelectedFirearm() noexcept;
    [[nodiscard]] bool switchTo(WeaponArchetype archetype) noexcept;
    [[nodiscard]] bool cycleWeapon() noexcept;
    [[nodiscard]] std::optional<WeaponArchetype>
        nextWeaponArchetype() const noexcept;
    [[nodiscard]] std::optional<WeaponArchetype>
        previousWeaponArchetype() const noexcept;
    [[nodiscard]] std::optional<WeaponArchetype>
        lastWeaponArchetype() const noexcept;
    [[nodiscard]] bool ownsWeapon(WeaponArchetype archetype) const noexcept;
    [[nodiscard]] bool hasFirearm() const noexcept;
    [[nodiscard]] bool activateOverdrive(
        float durationSeconds = 12.0F) noexcept;
    [[nodiscard]] bool restoreCampaignProgress(
        const WeaponCampaignProgress& progress) noexcept;
    // Refill owned firearms without changing selection, cooldown or overdrive.
    void restockAllFirearms() noexcept;

    [[nodiscard]] const WeaponDefinition& definition() const noexcept;
    [[nodiscard]] std::uint32_t magazineAmmo() const noexcept;
    [[nodiscard]] std::uint32_t reserveAmmo() const noexcept;
    [[nodiscard]] std::uint32_t reserveCapacity() const noexcept;
    [[nodiscard]] float cooldownRemaining() const noexcept;
    [[nodiscard]] float reloadRemaining() const noexcept;
    [[nodiscard]] float reloadProgress() const noexcept;
    [[nodiscard]] std::uint32_t reloadRoundCount() const noexcept;
    [[nodiscard]] bool reloading() const noexcept;
    [[nodiscard]] float switchRemaining() const noexcept;
    [[nodiscard]] float switchStrength() const noexcept;
    [[nodiscard]] bool switching() const noexcept;
    [[nodiscard]] float damage() const noexcept;
    [[nodiscard]] float knifeDamage() const noexcept;
    [[nodiscard]] float knifeRange() const noexcept;
    [[nodiscard]] float overdriveRemaining() const noexcept;
    [[nodiscard]] bool overdriveActive() const noexcept;
    [[nodiscard]] std::uint32_t spreadShotIndex() const noexcept;
    [[nodiscard]] float spreadRecoveryRemaining() const noexcept;
    [[nodiscard]] float spreadStrength() const noexcept;
    [[nodiscard]] WeaponCampaignProgress campaignProgress() const noexcept;

private:
    WeaponDefinition definition_{};
    WeaponDefinition knifeDefinition_{
        48.0F, 0.52F, 1.65F, 1U, 0U, WeaponArchetype::Knife};
    WeaponArchetype selectedArchetype_{WeaponArchetype::Pistol};
    std::optional<WeaponArchetype> lastSelectedArchetype_;
    std::uint32_t magazineAmmo_{0};
    std::uint32_t reserveAmmo_{0};
    float cooldownRemaining_{0.0F};
    float stowedCooldownRemaining_{0.0F};
    float knifeCooldownRemaining_{0.0F};
    float reloadRemaining_{0.0F};
    float reloadDuration_{0.0F};
    std::uint32_t reloadRoundCount_{0U};
    std::uint32_t reloadInsertedRounds_{0U};
    float switchRemaining_{0.0F};
    float overdriveRemaining_{0.0F};
    std::uint32_t spreadShotCount_{0U};
    std::uint32_t spreadShotIndex_{0U};
    float spreadRecoveryRemaining_{0.0F};
    std::optional<WeaponCampaignProgress::StowedWeapon> stowedWeapon_;
    bool hasFirearm_{true};
};

[[nodiscard]] WeaponDefinition baseWeaponDefinition(
    WeaponArchetype archetype) noexcept;
[[nodiscard]] bool isFirearmArchetype(WeaponArchetype archetype) noexcept;
[[nodiscard]] bool isPrimaryWeapon(WeaponArchetype archetype) noexcept;
[[nodiscard]] bool weaponUsesSpareMagazines(WeaponArchetype archetype) noexcept;
[[nodiscard]] std::uint32_t reserveUnitsFromRounds(
    const WeaponDefinition& definition, std::uint32_t rounds) noexcept;
inline constexpr float magazineReleaseProgress = 0.32F;
inline constexpr float shotgunShellReloadSeconds = 0.35F;
inline constexpr float shotgunReadySeconds = 0.30F;
[[nodiscard]] std::uint8_t weaponProjectileCount(WeaponArchetype archetype) noexcept;
[[nodiscard]] float weaponAimDurationSeconds(WeaponArchetype archetype) noexcept;
[[nodiscard]] float weaponAimFovScale(WeaponArchetype archetype) noexcept;
[[nodiscard]] float advanceWeaponAimBlend(float current, float seconds,
    WeaponArchetype archetype, bool requested, bool blocked = false) noexcept;
[[nodiscard]] const char* weaponArchetypeName(
    WeaponArchetype archetype) noexcept;
[[nodiscard]] WeaponSpreadProfile weaponSpreadProfile(
    WeaponArchetype archetype) noexcept;
// Camera kick after an accepted shot. X is yaw; positive Y raises the aim.
[[nodiscard]] wkc::core::Vec2 weaponRecoilKickDegrees(
    WeaponArchetype archetype, std::uint32_t shotIndex,
    float aimBlend = 0.0F, bool crouching = false) noexcept;
[[nodiscard]] float weaponReloadDurationSeconds(
    WeaponArchetype archetype) noexcept;
[[nodiscard]] float weaponSwitchDurationSeconds(
    WeaponArchetype archetype) noexcept;
[[nodiscard]] std::optional<WeaponArchetype> weaponArchetypeForSlot(
    std::uint32_t slot) noexcept;
[[nodiscard]] bool weaponUsesAutomaticFire(
    WeaponArchetype archetype) noexcept;
[[nodiscard]] bool weaponFireRequested(
    const Weapon& weapon,
    bool triggerPressed,
    bool triggerHeld) noexcept;
[[nodiscard]] bool validateWeaponCampaignProgress(
    const WeaponCampaignProgress& progress) noexcept;

} // namespace gdp::gameplay
