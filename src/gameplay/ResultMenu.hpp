#pragma once

#include <cstdint>

namespace gdp::gameplay {

enum class ResultMenuOption : std::uint8_t {
    PlayAgain,
    ExitGame,
    Count,
};

enum class ResultMenuCommand : std::uint8_t {
    None,
    PlayAgain,
    ExitGame,
};

struct ResultMenuState {
    bool active{false};
    ResultMenuOption selection{ResultMenuOption::PlayAgain};

    constexpr bool operator==(const ResultMenuState&) const noexcept = default;
};

void openResultMenu(ResultMenuState& state) noexcept;
void closeResultMenu(ResultMenuState& state) noexcept;
void moveResultMenuSelection(ResultMenuState& state, int direction) noexcept;
void selectResultMenuOption(
    ResultMenuState& state,
    ResultMenuOption option) noexcept;
[[nodiscard]] ResultMenuCommand activateResultMenuSelection(
    ResultMenuState& state) noexcept;

} // namespace gdp::gameplay
