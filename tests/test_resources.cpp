#include "resources/AssetCache.hpp"

#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    wkc::resources::AssetCache<int> cache;
    int factoryCalls = 0;
    const auto first = cache.acquire(41U, [&factoryCalls] {
        ++factoryCalls;
        return 7;
    });
    const auto reused = cache.acquire(41U, [&factoryCalls] {
        ++factoryCalls;
        return 9;
    });

    const wkc::resources::AssetCacheStats stats = cache.stats();
    expect(first == reused && *first == 7 && factoryCalls == 1,
        "WKC Forge cache reuses immutable resources");
    expect(stats.hits == 1U && stats.misses == 1U && stats.entries == 1U,
        "WKC Forge cache exposes deterministic metrics");

    cache.clear();
    expect(!cache.find(41U) && cache.stats().entries == 0U,
        "WKC Forge cache clears resources and metrics");

    if (failures != 0) {
        std::cerr << failures << " resource cache test(s) failed\n";
        return 1;
    }
    std::cout << "WKC Forge resource cache tests passed\n";
    return 0;
}
