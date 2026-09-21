#include "game/PerformanceMonitor.hpp"

#include <cmath>
#include <iostream>
#include <limits>
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

bool near(const double left, const double right) noexcept
{
    return std::abs(left - right) < 0.001;
}

void testRollingStatistics()
{
    gdp::game::PerformanceMonitor monitor;
    monitor.setWorldGenerationMilliseconds(42.5);
    monitor.setAssetPreparationMilliseconds(185.0);
    for (std::uint32_t index = 1U; index <= 20U; ++index) {
        monitor.record({
            .frameMilliseconds = static_cast<double>(index),
            .updateMilliseconds = 1.0,
            .renderMilliseconds = 2.0,
            .presentMilliseconds = 3.0,
            .gpuMilliseconds = 4.0,
            .gpuTimingAvailable = true,
            .fixedUpdates = 1U,
            .drawCalls = 70U + index,
            .triangles = 900U + index,
            .visibleEnemies = 5U,
            .activeLights = 4U,
            .uniformUploads = 180U,
            .uniformSkips = 420U,
            .textureUnitActivations = 15U,
            .textureBinds = 28U,
            .textureBindSkips = 90U,
            .meshBinds = 12U,
            .meshBindSkips = 81U,
            .lowSpecShader = true,
            .backfaceCulledDraws = 72U,
            .cullStateChanges = 4U,
            .instancedDrawCalls = 9U,
            .renderedInstances = 42U,
            .avoidedDrawCalls = 33U,
        });
    }

    const gdp::game::PerformanceSnapshot snapshot = monitor.snapshot();
    expect(snapshot.sampleCount == 20U
            && near(snapshot.averageFrameMilliseconds, 10.5)
            && near(snapshot.percentile95FrameMilliseconds, 19.0)
            && near(snapshot.worstFrameMilliseconds, 20.0),
        "rolling window reports average, P95 and worst frame time");
    expect(near(snapshot.averageUpdateMilliseconds, 1.0)
            && near(snapshot.averageRenderMilliseconds, 2.0)
            && near(snapshot.averagePresentMilliseconds, 3.0)
            && near(snapshot.averageGpuMilliseconds, 4.0)
            && snapshot.gpuSampleCount == 20U,
        "frame stages are averaged independently");
    expect(near(snapshot.estimatedFramesPerSecond, 1000.0 / 10.5)
            && near(snapshot.framesOverBudgetPercent, 20.0),
        "FPS and 60 Hz budget misses are derived from frame samples");
    expect(snapshot.latest.drawCalls == 90U
            && snapshot.latest.triangles == 920U
            && snapshot.latest.visibleEnemies == 5U
            && snapshot.latest.activeLights == 4U
            && snapshot.latest.uniformUploads == 180U
            && snapshot.latest.uniformSkips == 420U
            && snapshot.latest.textureUnitActivations == 15U
            && snapshot.latest.textureBinds == 28U
            && snapshot.latest.textureBindSkips == 90U
            && snapshot.latest.meshBinds == 12U
            && snapshot.latest.meshBindSkips == 81U
            && snapshot.latest.lowSpecShader
            && snapshot.latest.backfaceCulledDraws == 72U
            && snapshot.latest.cullStateChanges == 4U
            && snapshot.latest.instancedDrawCalls == 9U
            && snapshot.latest.renderedInstances == 42U
            && snapshot.latest.avoidedDrawCalls == 33U,
        "latest workload counters remain available to the overlay");
    expect(near(snapshot.worldGenerationMilliseconds, 42.5)
            && near(snapshot.assetPreparationMilliseconds, 185.0),
        "one-shot world and asset timings survive frame aggregation");
}

void testCapacityResetAndSanitization()
{
    gdp::game::PerformanceMonitor monitor;
    for (std::size_t index = 0U;
         index < gdp::game::performanceSampleWindow + 5U; ++index) {
        monitor.record({.frameMilliseconds = static_cast<double>(index)});
    }
    expect(monitor.snapshot().sampleCount
            == gdp::game::performanceSampleWindow,
        "rolling window discards samples older than its fixed capacity");

    monitor.record({
        .frameMilliseconds = std::numeric_limits<double>::quiet_NaN(),
        .updateMilliseconds = -5.0,
        .renderMilliseconds = std::numeric_limits<double>::infinity(),
        .gpuMilliseconds = std::numeric_limits<double>::infinity(),
        .gpuTimingAvailable = true,
    });
    const gdp::game::PerformanceSnapshot sanitized = monitor.snapshot();
    expect(sanitized.latest.frameMilliseconds == 0.0
            && sanitized.latest.updateMilliseconds == 0.0
            && sanitized.latest.renderMilliseconds == 0.0
            && sanitized.latest.gpuMilliseconds == 0.0
            && !sanitized.latest.gpuTimingAvailable,
        "invalid or negative timings cannot poison diagnostics");

    gdp::game::PerformanceMonitor delayedGpuMonitor;
    delayedGpuMonitor.record({.frameMilliseconds = 10.0});
    delayedGpuMonitor.record({
        .frameMilliseconds = 10.0,
        .gpuMilliseconds = 8.0,
        .gpuTimingAvailable = true,
    });
    const gdp::game::PerformanceSnapshot delayedGpu
        = delayedGpuMonitor.snapshot();
    expect(delayedGpu.gpuSampleCount == 1U
            && near(delayedGpu.averageGpuMilliseconds, 8.0),
        "asynchronous GPU warm-up frames do not dilute the average");

    monitor.reset();
    const gdp::game::PerformanceSnapshot reset = monitor.snapshot();
    expect(reset.sampleCount == 0U
            && reset.averageFrameMilliseconds == 0.0
            && reset.worldGenerationMilliseconds == 0.0,
        "reset clears rolling and one-shot measurements");
}

} // namespace

int main()
{
    testRollingStatistics();
    testCapacityResetAndSanitization();
    if (failures != 0) {
        std::cerr << failures << " performance monitor test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP performance monitor tests passed\n";
    return 0;
}
