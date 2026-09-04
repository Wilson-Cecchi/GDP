#pragma once

#include <cstdint>

namespace gdp::gameplay {

struct WeaponDefinition {
    float damage{34.0F};
    float fireInterval{0.22F};
    float range{30.0F};
    std::uint32_t magazineSize{8};
    std::uint32_t reserveMaximum{32};
};

struct WeaponCampaignProgress {
    WeaponDefinition definition{};
    std::uint32_t magazineAmmo{0U};
    std::uint32_t reserveAmmo{0U};
};

enum class FireReadiness : std::uint8_t {
    Fired,
    Cooldown,
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
    [[nodiscard]] bool restoreCampaignProgress(
        const WeaponCampaignProgress& progress) noexcept;

    [[nodiscard]] const WeaponDefinition& definition() const noexcept;
    [[nodiscard]] std::uint32_t magazineAmmo() const noexcept;
    [[nodiscard]] std::uint32_t reserveAmmo() const noexcept;
    [[nodiscard]] std::uint32_t reserveCapacity() const noexcept;
    [[nodiscard]] float cooldownRemaining() const noexcept;

private:
    WeaponDefinition definition_{};
    std::uint32_t magazineAmmo_{0};
    std::uint32_t reserveAmmo_{0};
    float cooldownRemaining_{0.0F};
};

} // namespace gdp::gameplay
