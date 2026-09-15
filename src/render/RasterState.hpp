#pragma once

#include <cstdint>

namespace wkc::render {

struct BackfaceCullingStateStats {
    std::uint64_t requests{0U};
    std::uint64_t stateChanges{0U};
    std::uint64_t unchangedRequests{0U};

    constexpr bool operator==(const BackfaceCullingStateStats&) const noexcept
        = default;
};

class BackfaceCullingStateCache {
public:
    [[nodiscard]] bool needsUpdate(bool enabled) noexcept;
    void beginFrame() noexcept;
    [[nodiscard]] BackfaceCullingStateStats stats() const noexcept;

private:
    bool enabled_{false};
    bool stateKnown_{false};
    BackfaceCullingStateStats stats_{};
};

} // namespace wkc::render
