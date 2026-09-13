#include "game/PerformanceMonitor.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::game {
namespace {

double sanitizeMilliseconds(const double milliseconds) noexcept
{
    constexpr double maximumRecordedMilliseconds = 60'000.0;
    if (!std::isfinite(milliseconds) || milliseconds < 0.0) {
        return 0.0;
    }
    return std::min(milliseconds, maximumRecordedMilliseconds);
}

FramePerformanceSample sanitizeSample(FramePerformanceSample sample) noexcept
{
    sample.frameMilliseconds = sanitizeMilliseconds(sample.frameMilliseconds);
    sample.updateMilliseconds = sanitizeMilliseconds(sample.updateMilliseconds);
    sample.renderMilliseconds = sanitizeMilliseconds(sample.renderMilliseconds);
    sample.presentMilliseconds = sanitizeMilliseconds(sample.presentMilliseconds);
    if (!std::isfinite(sample.gpuMilliseconds)
        || sample.gpuMilliseconds < 0.0) {
        sample.gpuMilliseconds = 0.0;
        sample.gpuTimingAvailable = false;
    } else {
        sample.gpuMilliseconds = sanitizeMilliseconds(sample.gpuMilliseconds);
    }
    return sample;
}

} // namespace

void PerformanceMonitor::reset() noexcept
{
    samples_ = {};
    nextSample_ = 0U;
    sampleCount_ = 0U;
    latest_ = {};
    worldGenerationMilliseconds_ = 0.0;
    assetPreparationMilliseconds_ = 0.0;
}

void PerformanceMonitor::record(FramePerformanceSample sample) noexcept
{
    sample = sanitizeSample(sample);
    samples_[nextSample_] = sample;
    nextSample_ = (nextSample_ + 1U) % samples_.size();
    sampleCount_ = std::min(sampleCount_ + 1U, samples_.size());
    latest_ = sample;
}

void PerformanceMonitor::setWorldGenerationMilliseconds(
    const double milliseconds) noexcept
{
    worldGenerationMilliseconds_ = sanitizeMilliseconds(milliseconds);
}

void PerformanceMonitor::setAssetPreparationMilliseconds(
    const double milliseconds) noexcept
{
    assetPreparationMilliseconds_ = sanitizeMilliseconds(milliseconds);
}

PerformanceSnapshot PerformanceMonitor::snapshot() const noexcept
{
    PerformanceSnapshot result;
    result.sampleCount = sampleCount_;
    result.latest = latest_;
    result.worldGenerationMilliseconds = worldGenerationMilliseconds_;
    result.assetPreparationMilliseconds = assetPreparationMilliseconds_;
    if (sampleCount_ == 0U) {
        return result;
    }

    std::array<double, performanceSampleWindow> frameTimes{};
    std::size_t framesOverBudget = 0U;
    for (std::size_t index = 0U; index < sampleCount_; ++index) {
        const FramePerformanceSample& sample = samples_[index];
        frameTimes[index] = sample.frameMilliseconds;
        result.averageFrameMilliseconds += sample.frameMilliseconds;
        result.averageUpdateMilliseconds += sample.updateMilliseconds;
        result.averageRenderMilliseconds += sample.renderMilliseconds;
        result.averagePresentMilliseconds += sample.presentMilliseconds;
        if (sample.gpuTimingAvailable) {
            result.averageGpuMilliseconds += sample.gpuMilliseconds;
            ++result.gpuSampleCount;
        }
        result.worstFrameMilliseconds = std::max(
            result.worstFrameMilliseconds, sample.frameMilliseconds);
        if (sample.frameMilliseconds > sixtyFpsFrameBudgetMilliseconds) {
            ++framesOverBudget;
        }
    }

    const double divisor = static_cast<double>(sampleCount_);
    result.averageFrameMilliseconds /= divisor;
    result.averageUpdateMilliseconds /= divisor;
    result.averageRenderMilliseconds /= divisor;
    result.averagePresentMilliseconds /= divisor;
    if (result.gpuSampleCount > 0U) {
        result.averageGpuMilliseconds
            /= static_cast<double>(result.gpuSampleCount);
    }
    result.estimatedFramesPerSecond = result.averageFrameMilliseconds > 0.0
        ? 1000.0 / result.averageFrameMilliseconds : 0.0;
    result.framesOverBudgetPercent
        = (static_cast<double>(framesOverBudget) * 100.0) / divisor;

    std::sort(frameTimes.begin(), frameTimes.begin() + sampleCount_);
    const std::size_t percentileIndex = std::min(
        sampleCount_ - 1U,
        static_cast<std::size_t>(
            std::ceil(static_cast<double>(sampleCount_) * 0.95)) - 1U);
    result.percentile95FrameMilliseconds = frameTimes[percentileIndex];
    return result;
}

} // namespace gdp::game
