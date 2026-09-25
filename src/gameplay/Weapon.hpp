#pragma once

#include <cstdint>
#include <optional>

namespace gdp::gameplay {

enum class WeaponArchetype : std::uint8_t {
    Pistol,
    Carbine,
};

struct WeaponDefinition {
    float damage{34.0F};
    float fireInterval{0.22F};
    float range{30.0F};
    std::uint32_t magazineSize{8};
    std::uint32_t reserveMaximum{32};
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
    Empty,
};

class Weapon {
public:
    explicit Weapon(WeaponDefinition definition = {});

    void fixedUpdate(float fixedStepSeconds) noexcept;
    [[nodiscard]] FireReadiness tryFire() noexcept;
    [[nodiscard]] bool reload() noexcept;
    [[nodiscard]] std::uint32_t addReserveAmmo(std::uint32_t amount) noexcept;
    [[nodiscard]] bool increaseDamage(float amount) noexcept;
    [[nodiscard]] bool increaseReserveCapacity(
        std::uint32_t amount,
        std::uint32_t bonusAmmo = 0U) noexcept;
    [[nodiscard]] bool equip(WeaponArchetype archetype) noexcept;
    [[nodiscard]] bool switchTo(WeaponArchetype archetype) noexcept;
    [[nodiscard]] bool cycleWeapon() noexcept;
    [[nodiscard]] bool ownsWeapon(WeaponArchetype archetype) const noexcept;
    [[nodiscard]] bool activateOverdrive(
        float durationSeconds = 12.0F) noexcept;
    [[nodiscard]] bool restoreCampaignProgress(
        const WeaponCampaignProgress& progress) noexcept;

    [[nodiscard]] const WeaponDefinition& definition() const noexcept;
    [[nodiscard]] std::uint32_t magazineAmmo() const noexcept;
    [[nodiscard]] std::uint32_t reserveAmmo() const noexcept;
    [[nodiscard]] std::uint32_t reserveCapacity() const noexcept;
    [[nodiscard]] float cooldownRemaining() const noexcept;
    [[nodiscard]] float reloadRemaining() const noexcept;
    [[nodiscard]] float reloadProgress() const noexcept;
    [[nodiscard]] bool reloading() const noexcept;
    [[nodiscard]] float damage() const noexcept;
    [[nodiscard]] float overdriveRemaining() const noexcept;
    [[nodiscard]] bool overdriveActive() const noexcept;
    [[nodiscard]] std::uint32_t spreadShotIndex() const noexcept;
    [[nodiscard]] float spreadRecoveryRemaining() const noexcept;
    [[nodiscard]] float spreadStrength() const noexcept;
    [[nodiscard]] WeaponCampaignProgress campaignProgress() const noexcept;

private:
    WeaponDefinition definition_{};
    std::uint32_t magazineAmmo_{0};
    std::uint32_t reserveAmmo_{0};
    float cooldownRemaining_{0.0F};
    float reloadRemaining_{0.0F};
    float overdriveRemaining_{0.0F};
    std::uint32_t spreadShotCount_{0U};
    std::uint32_t spreadShotIndex_{0U};
    float spreadRecoveryRemaining_{0.0F};
    std::optional<WeaponCampaignProgress::StowedWeapon> stowedWeapon_;
};

[[nodiscard]] WeaponDefinition baseWeaponDefinition(
    WeaponArchetype archetype) noexcept;
[[nodiscard]] const char* weaponArchetypeName(
    WeaponArchetype archetype) noexcept;
[[nodiscard]] WeaponSpreadProfile weaponSpreadProfile(
    WeaponArchetype archetype) noexcept;
[[nodiscard]] float weaponReloadDurationSeconds(
    WeaponArchetype archetype) noexcept;
[[nodiscard]] bool weaponUsesAutomaticFire(
    WeaponArchetype archetype) noexcept;
[[nodiscard]] bool weaponFireRequested(
    const Weapon& weapon,
    bool triggerPressed,
    bool triggerHeld) noexcept;
[[nodiscard]] bool validateWeaponCampaignProgress(
    const WeaponCampaignProgress& progress) noexcept;

} // namespace gdp::gameplay
