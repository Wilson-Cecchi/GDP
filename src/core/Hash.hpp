#pragma once

#include <cstdint>
#include <string_view>

namespace wkc::core {

[[nodiscard]] std::uint64_t mix64(std::uint64_t value) noexcept;
[[nodiscard]] std::uint64_t hashCombine(std::uint64_t seed, std::uint64_t value) noexcept;
[[nodiscard]] std::uint64_t fnv1a64(std::string_view text) noexcept;

} // namespace wkc::core
