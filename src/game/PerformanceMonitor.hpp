#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace gdp::game {

inline constexpr std::size_t performanceSampleWindow = 120U;
inline constexpr double sixtyFpsFrameBudgetMilliseconds = 1000.0 / 60.0;

struct FramePerformanceSample {
    double frameMilliseconds{0.0};
    double updateMilliseconds{0.0};
    double renderMilliseconds{0.0};
    double presentMilliseconds{0.0};
    double gpuMilliseconds{0.0};
    bool gpuTimingAvailable{false};
    std::uint32_t fixedUpdates{0U};
    std::uint64_t drawCalls{0U};
    std::uint64_t triangles{0U};
    std::uint32_t visibleEnemies{0U};
    std::uint32_t visibleProps{0U};
    std::uint32_t visibleRooms{0U};
    std::uint32_t activeLights{0U};
    std::uint32_t shadowLights{0U};
    std::uint32_t shadowOccluders{0U};
    std::uint32_t meshCacheEntries{0U};
    std::uint32_t textureCacheEntries{0U};
    std::uint64_t uniformUploads{0U};
    std::uint64_t uniformSkips{0U};
    std::uint64_t textureUnitActivations{0U};
    std::uint64_t textureBinds{0U};
    std::uint64_t textureBindSkips{0U};
    std::uint64_t meshBinds{0U};
    std::uint64_t meshBindSkips{0U};

    constexpr bool operator==(const FramePerformanceSample&) const noexcept
        = default;
};

struct PerformanceSnapshot {
    std::size_t sampleCount{0U};
    FramePerformanceSample latest{};
    double averageFrameMilliseconds{0.0};
    double percentile95FrameMilliseconds{0.0};
    double worstFrameMilliseconds{0.0};
    double averageUpdateMilliseconds{0.0};
    double averageRenderMilliseconds{0.0};
    double averagePresentMilliseconds{0.0};
    double averageGpuMilliseconds{0.0};
    std::size_t gpuSampleCount{0U};
    double estimatedFramesPerSecond{0.0};
    double framesOverBudgetPercent{0.0};
    double worldGenerationMilliseconds{0.0};
    double assetPreparationMilliseconds{0.0};
};

class PerformanceMonitor {
public:
    void reset() noexcept;
    void record(FramePerformanceSample sample) noexcept;
    void setWorldGenerationMilliseconds(double milliseconds) noexcept;
    void setAssetPreparationMilliseconds(double milliseconds) noexcept;

    [[nodiscard]] PerformanceSnapshot snapshot() const noexcept;

private:
    std::array<FramePerformanceSample, performanceSampleWindow> samples_{};
    std::size_t nextSample_{0U};
    std::size_t sampleCount_{0U};
    FramePerformanceSample latest_{};
    double worldGenerationMilliseconds_{0.0};
    double assetPreparationMilliseconds_{0.0};
};

} // namespace gdp::game
