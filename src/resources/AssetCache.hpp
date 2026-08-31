#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

namespace gdp::procedural {

struct AssetCacheStats {
    std::size_t hits{0};
    std::size_t misses{0};
    std::size_t entries{0};
};

template<typename Asset>
class AssetCache {
public:
    template<typename Factory>
    [[nodiscard]] std::shared_ptr<const Asset> acquire(
        const std::uint64_t key,
        Factory&& factory)
    {
        const auto existing = assets_.find(key);
        if (existing != assets_.end()) {
            ++hits_;
            return existing->second;
        }
        auto asset = std::make_shared<const Asset>(std::forward<Factory>(factory)());
        assets_.emplace(key, asset);
        ++misses_;
        return asset;
    }

    [[nodiscard]] std::shared_ptr<const Asset> find(const std::uint64_t key) const noexcept
    {
        const auto existing = assets_.find(key);
        return existing == assets_.end() ? nullptr : existing->second;
    }

    void clear() noexcept
    {
        assets_.clear();
        hits_ = 0;
        misses_ = 0;
    }

    [[nodiscard]] AssetCacheStats stats() const noexcept
    {
        return {hits_, misses_, assets_.size()};
    }

private:
    std::unordered_map<std::uint64_t, std::shared_ptr<const Asset>> assets_;
    std::size_t hits_{0};
    std::size_t misses_{0};
};

} // namespace gdp::procedural
