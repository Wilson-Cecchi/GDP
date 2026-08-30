#pragma once

#include <cstdint>
#include <type_traits>

namespace gdp::core {

using Seed = std::uint64_t;
using SeedDomainId = std::uint64_t;

struct GenerationVersion {
    std::uint32_t major{1};
    std::uint32_t minor{0};

    constexpr bool operator==(const GenerationVersion&) const noexcept = default;
};

inline constexpr GenerationVersion currentGenerationVersion{1, 0};

[[nodiscard]] Seed deriveSeed(
    Seed parent,
    SeedDomainId domain,
    std::uint64_t localId,
    GenerationVersion version = currentGenerationVersion) noexcept;

template<typename Domain>
    requires std::is_enum_v<Domain>
[[nodiscard]] Seed deriveSeed(
    const Seed parent,
    const Domain domain,
    const std::uint64_t localId,
    const GenerationVersion version = currentGenerationVersion) noexcept
{
    return deriveSeed(
        parent,
        static_cast<SeedDomainId>(domain),
        localId,
        version);
}

} // namespace gdp::core
