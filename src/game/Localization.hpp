#pragma once

#include "gameplay/Language.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace gdp::game {
// English catalog keys remain stable internal data, never translated commands/save tokens.
[[nodiscard]] std::string localize(gameplay::Language language, std::string_view source);
[[nodiscard]] std::uint32_t nextUiCodepoint(std::string_view text, std::size_t& offset) noexcept;
[[nodiscard]] std::size_t uiTextLength(std::string_view text) noexcept;
void popUiCodepoint(std::string& text) noexcept;
} // namespace gdp::game
