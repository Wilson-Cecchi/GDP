#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace gdp::gameplay {
enum class Language : std::uint8_t { English, PortugueseBrazil, Count };
[[nodiscard]] std::string_view languageName(Language language) noexcept;
[[nodiscard]] std::optional<Language> languageForName(std::string_view name) noexcept;
} // namespace gdp::gameplay
