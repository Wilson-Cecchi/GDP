#pragma once

#include "render/GlApi.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace wkc::render {

class GpuTimer {
public:
    GpuTimer() = default;
    ~GpuTimer();

    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;
    GpuTimer(GpuTimer&&) = delete;
    GpuTimer& operator=(GpuTimer&&) = delete;

    [[nodiscard]] bool initialize(const GlApi& gl) noexcept;
    [[nodiscard]] bool beginFrame() noexcept;
    void endFrame() noexcept;
    void reset() noexcept;

    [[nodiscard]] bool isSupported() const noexcept;
    [[nodiscard]] std::optional<double> latestMilliseconds() const noexcept;

private:
    static constexpr std::size_t queryBufferSize = 4U;

    const GlApi* gl_{nullptr};
    std::array<GLuint, queryBufferSize> queries_{};
    std::array<bool, queryBufferSize> pending_{};
    std::size_t nextQuery_{0U};
    std::optional<std::size_t> activeQuery_{};
    std::optional<double> latestMilliseconds_{};
};

} // namespace wkc::render
