#include "core/Seed.hpp"

#include "core/Hash.hpp"

namespace gdp::core {

Seed deriveSeed(
    const Seed parent,
    const SeedDomainId domain,
    const std::uint64_t localId,
    const GenerationVersion version) noexcept
{
    const auto packedVersion = (static_cast<std::uint64_t>(version.major) << 32U)
        | static_cast<std::uint64_t>(version.minor);

    auto result = hashCombine(parent, domain);
    result = hashCombine(result, localId);
    return hashCombine(result, packedVersion);
}

} // namespace gdp::core
