#pragma once

#include <cstdint>

namespace gdp::gameplay {

enum class ExitConfirmationOption : std::uint8_t {
    Cancel,
    ExitGame,
    Count,
};

enum class ExitConfirmationCommand : std::uint8_t {
    None,
    Cancel,
    ExitGame,
};

struct ExitConfirmationState {
    bool active{false};
    ExitConfirmationOption selection{ExitConfirmationOption::Cancel};

    constexpr bool operator==(const ExitConfirmationState&) const noexcept = default;
};

void openExitConfirmation(ExitConfirmationState& state) noexcept;
void closeExitConfirmation(ExitConfirmationState& state) noexcept;
void moveExitConfirmationSelection(
    ExitConfirmationState& state,
    int direction) noexcept;
void selectExitConfirmationOption(
    ExitConfirmationState& state,
    ExitConfirmationOption option) noexcept;
[[nodiscard]] ExitConfirmationCommand activateExitConfirmationSelection(
    ExitConfirmationState& state) noexcept;

} // namespace gdp::gameplay
