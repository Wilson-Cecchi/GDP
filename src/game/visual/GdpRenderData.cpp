#include "game/visual/GdpRenderData.hpp"

#include "core/Hash.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <vector>

namespace gdp::game::visual {
namespace {

struct EnemyMotionProfile {
    std::uint64_t idlePeriod{120U};
    std::uint64_t walkPeriod{30U};
    std::uint64_t attackPeriod{26U};
    float idleHeadYaw{0.8F};
    float leftArmWalk{8.0F};
    float rightArmWalk{8.0F};
    float leftLegWalk{10.0F};
    float rightLegWalk{10.0F};
    float attackArmPitch{-10.0F};
    float attackToolTravel{0.045F};
    float hitTorsoTravel{-0.014F};
    float hitTorsoPitch{-3.5F};
    float hitHeadPitch{4.5F};
    float hitArmPitch{7.0F};
    float hitToolTravel{-0.035F};
};

struct EnemyDeathProfile {
    std::uint32_t durationTicks{48U};
    float rootTilt{88.0F};
    float rootLift{0.18F};
    float articulation{1.0F};
    float settlingAmplitude{10.0F};
};

struct EnemyFallFrame {
    wkc::core::Vec3 direction{};
    float pitchDegrees{0.0F};
    float rollDegrees{0.0F};
    float handedness{1.0F};
};

struct EnemyPartDeathTiming {
    float delay{0.0F};
    float contact{0.78F};
    float settled{0.96F};
};

EnemyMotionProfile motionProfileFor(const EnemyVisualRole role) noexcept
{
    switch (role) {
    case EnemyVisualRole::Worker:
        return {};
    case EnemyVisualRole::Guard:
        return {150U, 34U, 18U, 0.45F,
            5.5F, 5.5F, 7.5F, 7.5F,
            -2.5F, -0.045F, -0.010F, -2.5F, 3.0F, 4.0F, -0.028F};
    case EnemyVisualRole::Officer:
        return {180U, 40U, 20U, 0.35F,
            3.5F, 3.5F, 6.0F, 6.0F,
            -2.0F, -0.035F, -0.008F, -2.0F, 2.5F, 3.0F, -0.024F};
    case EnemyVisualRole::Engineer:
        return {105U, 36U, 24U, 1.0F,
            4.5F, 7.0F, 7.5F, 8.5F,
            -5.5F, -0.030F, -0.011F, -3.0F, 4.0F, 6.0F, -0.032F};
    case EnemyVisualRole::IndustrialRobot:
        return {210U, 48U, 36U, 0.22F,
            3.0F, 3.0F, 5.0F, 5.0F,
            -2.0F, -0.065F, -0.006F, -1.3F, 1.8F, 2.5F, -0.020F};
    }
    return {};
}

float alertReadinessScale(const EnemyVisualRole role) noexcept
{
    switch (role) {
    case EnemyVisualRole::Worker:
        return 1.0F;
    case EnemyVisualRole::Guard:
        return 0.82F;
    case EnemyVisualRole::Officer:
        return 0.68F;
    case EnemyVisualRole::Engineer:
        return 0.90F;
    case EnemyVisualRole::IndustrialRobot:
        return 0.48F;
    }
    return 1.0F;
}

EnemyDeathProfile deathProfileFor(const EnemyVisualRole role) noexcept
{
    switch (role) {
    case EnemyVisualRole::Worker:
        return {};
    case EnemyVisualRole::Guard:
        return {50U, 88.0F, 0.20F, 0.82F, 7.0F};
    case EnemyVisualRole::Officer:
        return {52U, 88.0F, 0.21F, 0.70F, 6.0F};
    case EnemyVisualRole::Engineer:
        return {48U, 88.0F, 0.19F, 1.05F, 11.0F};
    case EnemyVisualRole::IndustrialRobot:
        return {66U, 88.0F, 0.26F, 0.48F, 3.5F};
    }
    return {};
}

float seededSignedUnit(
    const wkc::core::Seed visualSeed,
    const std::uint64_t channel) noexcept
{
    constexpr std::uint64_t salt = 0x9e3779b97f4a7c15ULL;
    const std::uint64_t mixed = wkc::core::mix64(
        visualSeed ^ (salt * (channel + 1U)));
    const float unit = static_cast<float>(mixed & 0xffffU) / 65535.0F;
    return (unit * 2.0F) - 1.0F;
}

float seededUnit(
    const wkc::core::Seed visualSeed,
    const std::uint64_t channel) noexcept
{
    return (seededSignedUnit(visualSeed, channel) + 1.0F) * 0.5F;
}

EnemyFallFrame enemyFallFrame(
    const wkc::core::Seed visualSeed,
    const EnemyDeathProfile& profile) noexcept
{
    constexpr float pi = 3.14159265358979323846F;
    constexpr float radiansToDegrees = 180.0F / pi;
    const float azimuth = seededUnit(visualSeed, 4U) * (2.0F * pi);
    const wkc::core::Vec3 direction{
        std::cos(azimuth), 0.0F, std::sin(azimuth)};
    const float tilt = profile.rootTilt / radiansToDegrees;
    const float horizontal = std::sin(tilt);
    const float vertical = std::cos(tilt);
    const float pitch = std::asin(std::clamp(
        direction.z * horizontal, -1.0F, 1.0F));
    const float roll = std::atan2(-direction.x * horizontal, vertical);
    return {
        direction,
        pitch * radiansToDegrees,
        roll * radiansToDegrees,
        seededSignedUnit(visualSeed, 5U) < 0.0F ? -1.0F : 1.0F,
    };
}

EnemyPartDeathTiming partDeathTiming(
    const wkc::core::Seed visualSeed,
    const std::uint32_t partIndex,
    const std::uint32_t partCount) noexcept
{
    EnemyPartDeathTiming timing;
    if (partIndex == 4U) {
        timing = {0.00F, 0.67F, 0.86F};
    } else if (partIndex == 5U) {
        timing = {0.03F, 0.69F, 0.89F};
    } else if (partIndex == 0U) {
        timing = {0.08F, 0.73F, 0.92F};
    } else if (partIndex == 2U) {
        timing = {0.14F, 0.75F, 0.95F};
    } else if (partIndex == 1U) {
        timing = {0.18F, 0.77F, 0.97F};
    } else if (partIndex == 3U) {
        timing = {0.22F, 0.79F, 0.98F};
    } else if (partIndex + 1U == partCount) {
        timing = {0.24F, 0.81F, 1.00F};
    } else {
        timing = {0.24F, 0.80F, 0.96F};
    }
    const float variation = seededSignedUnit(visualSeed, partIndex + 29U);
    timing.delay = std::clamp(
        timing.delay + (variation * 0.018F), 0.0F, 0.26F);
    timing.contact = std::clamp(
        timing.contact + (variation * 0.012F), timing.delay + 0.40F, 0.83F);
    if (timing.settled < 1.0F) {
        timing.settled = std::clamp(
            timing.settled + (variation * 0.008F), timing.contact + 0.12F, 0.99F);
    }
    return timing;
}

float dampedContact(
    const float progress,
    const float contact,
    const float settled,
    const float cycles) noexcept
{
    if (progress <= contact || progress >= settled) {
        return 0.0F;
    }
    constexpr float tau = 6.28318530717958647692F;
    const float local = (progress - contact) / (settled - contact);
    const float damping = (1.0F - local) * (1.0F - local);
    return std::sin(local * tau * cycles) * damping;
}

float smoothProgress(const float progress) noexcept
{
    const float safeProgress = std::isfinite(progress)
        ? std::clamp(progress, 0.0F, 1.0F) : 1.0F;
    return safeProgress * safeProgress * (3.0F - (2.0F * safeProgress));
}

EnemyPartPose terminalDeathPartPose(
    const wkc::core::Seed visualSeed,
    const EnemyVisualRole role,
    const std::uint32_t partIndex,
    const std::uint32_t partCount) noexcept
{
    if (partIndex >= partCount || partCount == 0U) {
        return {};
    }
    const EnemyDeathProfile profile = deathProfileFor(role);
    const EnemyFallFrame fall = enemyFallFrame(visualSeed, profile);
    const float variation = seededSignedUnit(visualSeed, partIndex + 7U);
    const float articulation = profile.articulation;
    if (partIndex == 0U) {
        return {{}, {-3.0F * articulation, 0.0F,
            -fall.handedness * 4.0F * articulation}};
    }
    if (partIndex == 1U) {
        return {{}, {
            (12.0F + (variation * 5.0F)) * articulation,
            variation * 7.0F * articulation,
            -fall.handedness * (18.0F + (variation * 4.0F)) * articulation}};
    }
    if (partIndex == 2U) {
        return {{}, {
            (28.0F + (variation * 10.0F)) * articulation,
            variation * 6.0F * articulation,
            -fall.handedness * (42.0F + (variation * 7.0F)) * articulation}};
    }
    if (partIndex == 3U) {
        return {{}, {
            (-22.0F + (variation * 10.0F)) * articulation,
            variation * 6.0F * articulation,
            fall.handedness * (48.0F - (variation * 7.0F)) * articulation}};
    }
    if (partIndex == 4U) {
        return {{}, {
            (-16.0F + (variation * 7.0F)) * articulation,
            0.0F,
            -fall.handedness * (10.0F + (variation * 4.0F)) * articulation}};
    }
    if (partIndex == 5U) {
        return {{}, {
            (20.0F + (variation * 7.0F)) * articulation,
            0.0F,
            fall.handedness * (13.0F - (variation * 4.0F)) * articulation}};
    }
    if (partIndex + 1U == partCount) {
        return {{0.0F, -0.012F * articulation, 0.0F}, {
            (32.0F + (variation * 12.0F)) * articulation,
            variation * 9.0F * articulation,
            fall.handedness * (24.0F + (variation * 8.0F)) * articulation}};
    }
    return {{}, {
        variation * 7.0F * articulation,
        0.0F,
        -fall.handedness * 8.0F * articulation}};
}

std::uint32_t transitionDurationTicks(
    const EnemyAnimationState previous,
    const EnemyAnimationState current,
    const EnemyVisualRole role) noexcept
{
    if (current == EnemyAnimationState::Dead) {
        return deathProfileFor(role).durationTicks;
    }
    if (current == EnemyAnimationState::Hit) {
        return 3U;
    }
    if (current == EnemyAnimationState::Alert) {
        return 3U;
    }
    if (previous == EnemyAnimationState::Alert) {
        return 4U;
    }
    if (current == EnemyAnimationState::Recover
        || previous == EnemyAnimationState::Recover) {
        return 5U;
    }
    if (previous == EnemyAnimationState::Hit
        || current == EnemyAnimationState::Attack) {
        return 4U;
    }
    return 6U;
}

float mix(const float previous, const float current, const float blend) noexcept
{
    return previous + ((current - previous) * blend);
}

wkc::core::Vec3 mix(
    const wkc::core::Vec3 previous,
    const wkc::core::Vec3 current,
    const float blend) noexcept
{
    return {
        mix(previous.x, current.x, blend),
        mix(previous.y, current.y, blend),
        mix(previous.z, current.z, blend),
    };
}

} // namespace

void advanceEnemyAnimationTransition(
    EnemyAnimationTransition& transition,
    const EnemyAnimationState requestedState,
    const EnemyVisualRole role) noexcept
{
    if (!transition.initialized) {
        transition.previousState = requestedState;
        transition.currentState = requestedState;
        transition.elapsedTicks = 0U;
        transition.durationTicks = 0U;
        transition.initialized = true;
        return;
    }
    if (requestedState != transition.currentState) {
        transition.previousState = transition.currentState;
        transition.currentState = requestedState;
        transition.elapsedTicks = 0U;
        transition.durationTicks = transitionDurationTicks(
            transition.previousState, transition.currentState, role);
    }
    if (transition.elapsedTicks < transition.durationTicks) {
        ++transition.elapsedTicks;
    }
}

float enemyAnimationTransitionBlend(
    const EnemyAnimationTransition& transition) noexcept
{
    if (!transition.initialized || transition.durationTicks == 0U) {
        return 1.0F;
    }
    const float progress = std::clamp(
        static_cast<float>(transition.elapsedTicks)
            / static_cast<float>(transition.durationTicks),
        0.0F, 1.0F);
    if (transition.currentState == EnemyAnimationState::Dead) {
        return progress;
    }
    return smoothProgress(progress);
}

EnemyRootPose blendEnemyRootPoses(
    const EnemyRootPose& previous,
    const EnemyRootPose& current,
    const float blend) noexcept
{
    const float safeBlend = std::isfinite(blend)
        ? std::clamp(blend, 0.0F, 1.0F) : 1.0F;
    return {
        mix(previous.offset, current.offset, safeBlend),
        mix(previous.scale, current.scale, safeBlend),
        mix(previous.rotationDegrees, current.rotationDegrees, safeBlend),
    };
}

EnemyPartPose blendEnemyPartPoses(
    const EnemyPartPose& previous,
    const EnemyPartPose& current,
    const float blend) noexcept
{
    const float safeBlend = std::isfinite(blend)
        ? std::clamp(blend, 0.0F, 1.0F) : 1.0F;
    return {
        mix(previous.offset, current.offset, safeBlend),
        mix(previous.rotationDegrees, current.rotationDegrees, safeBlend),
    };
}

EnemyRootPose sampleEnemyRagdollRootPose(
    const EnemyRootPose& previous,
    const wkc::core::Seed visualSeed,
    const EnemyVisualRole role,
    const float progress) noexcept
{
    const float safeProgress = std::isfinite(progress)
        ? std::clamp(progress, 0.0F, 1.0F) : 1.0F;
    const EnemyRootPose terminal = sampleEnemyRootPose(
        EnemyAnimationState::Dead, 0U, visualSeed, true, role);
    const EnemyDeathProfile profile = deathProfileFor(role);
    const EnemyFallFrame fall = enemyFallFrame(visualSeed, profile);
    const float fallProgress = smoothProgress(
        (safeProgress - 0.10F) / 0.68F);
    EnemyRootPose pose = blendEnemyRootPoses(previous, terminal, fallProgress);
    const float fallArc = std::sin(fallProgress * 3.14159265358979323846F)
        * (1.0F - fallProgress);
    pose.offset.x += fallArc * fall.direction.x * 0.085F;
    pose.offset.z += fallArc * fall.direction.z * 0.085F;
    const float contactOscillation = dampedContact(
        safeProgress, 0.72F, 0.94F, 1.0F);
    pose.offset.y += std::abs(contactOscillation) * 0.025F;
    pose.rotationDegrees.x += contactOscillation * fall.direction.z
        * profile.settlingAmplitude * 0.55F;
    pose.rotationDegrees.z -= contactOscillation * fall.direction.x
        * profile.settlingAmplitude * 0.55F;
    return pose;
}

EnemyPartPose sampleEnemyRagdollPartPose(
    const EnemyPartPose& previous,
    const wkc::core::Seed visualSeed,
    const EnemyVisualRole role,
    const std::uint32_t partIndex,
    const std::uint32_t partCount,
    const float progress) noexcept
{
    if (partIndex >= partCount || partCount == 0U) {
        return {};
    }
    const float safeProgress = std::isfinite(progress)
        ? std::clamp(progress, 0.0F, 1.0F) : 1.0F;
    const EnemyPartDeathTiming timing = partDeathTiming(
        visualSeed, partIndex, partCount);
    const float variation = seededSignedUnit(visualSeed, partIndex + 19U);
    const float localProgress = smoothProgress(
        (safeProgress - timing.delay)
            / std::max(timing.contact - timing.delay, 0.001F));
    const EnemyPartPose terminal = terminalDeathPartPose(
        visualSeed, role, partIndex, partCount);
    EnemyPartPose pose = blendEnemyPartPoses(previous, terminal, localProgress);
    const EnemyDeathProfile profile = deathProfileFor(role);
    const EnemyFallFrame fall = enemyFallFrame(visualSeed, profile);
    const float inertia = std::sin(localProgress * 3.14159265358979323846F)
        * (1.0F - localProgress) * profile.articulation;
    const float inertiaScale = partIndex == 1U ? 13.0F
        : partIndex == 2U || partIndex == 3U ? 20.0F
        : partIndex == 4U || partIndex == 5U ? 9.0F
        : 15.0F;
    pose.rotationDegrees.x += inertia
        * ((fall.direction.z * 0.75F) + (variation * 0.25F))
        * inertiaScale;
    pose.rotationDegrees.z -= inertia
        * ((fall.direction.x * 0.75F) + (fall.handedness * 0.25F))
        * inertiaScale;
    const float settling = dampedContact(
        safeProgress, timing.contact, timing.settled, 2.0F)
        * profile.settlingAmplitude;
    pose.rotationDegrees.x += settling
        * ((fall.direction.z * 0.75F) + (variation * 0.25F));
    pose.rotationDegrees.z -= settling
        * ((fall.direction.x * 0.75F) + (fall.handedness * 0.25F));
    return pose;
}

EnemyRootPose sampleEnemyRootPose(
    const EnemyAnimationState state,
    const std::uint64_t animationTick,
    const wkc::core::Seed visualSeed,
    const bool active,
    const EnemyVisualRole role) noexcept
{
    if (state == EnemyAnimationState::Dead) {
        const EnemyDeathProfile profile = deathProfileFor(role);
        const EnemyFallFrame fall = enemyFallFrame(visualSeed, profile);
        return {
            {0.0F, profile.rootLift, 0.0F},
            {1.0F, 1.0F, 1.0F},
            {fall.pitchDegrees, 0.0F, fall.rollDegrees},
        };
    }
    if (!active) {
        return {};
    }
    if (state == EnemyAnimationState::Hit) {
        return {};
    }
    if (state == EnemyAnimationState::Alert) {
        const float readiness = alertReadinessScale(role);
        return {
            {0.0F, 0.006F * readiness, 0.012F * readiness},
            {1.0F, 1.0F, 1.0F},
            {-2.0F * readiness, 0.0F, 0.0F},
        };
    }

    constexpr float tau = 6.28318530717958647692F;
    std::uint64_t period = 120U;
    if (state == EnemyAnimationState::Walk) {
        period = 30U;
    } else if (state == EnemyAnimationState::Attack) {
        period = 24U;
    } else if (state == EnemyAnimationState::Recover) {
        period = 72U;
    }
    const std::uint64_t phaseTick
        = (animationTick + (visualSeed % period)) % period;
    const float phase = static_cast<float>(phaseTick)
        / static_cast<float>(period) * tau;
    const float wave = std::sin(phase);

    if (state == EnemyAnimationState::Recover) {
        const float readiness = alertReadinessScale(role);
        return {
            {0.0F, ((wave + 1.0F) * 0.003F) * readiness, 0.0F},
            {1.0F, 1.0F, 1.0F},
        };
    }
    if (state == EnemyAnimationState::Walk) {
        const float stride = std::abs(wave);
        return {
            {0.0F, stride * 0.035F, 0.0F},
            {1.0F + (stride * 0.015F),
                1.0F - (stride * 0.018F),
                1.0F + (stride * 0.015F)},
        };
    }
    if (state == EnemyAnimationState::Attack) {
        const float impulse = std::max(wave, 0.0F);
        return {
            {0.0F, impulse * 0.01F, 0.0F},
            {1.0F + (impulse * 0.02F),
                1.0F - (impulse * 0.025F),
                1.0F + (impulse * 0.055F)},
        };
    }

    const float breath = (wave + 1.0F) * 0.5F;
    return {
        {0.0F, breath * 0.012F, 0.0F},
        {1.0F + (wave * 0.004F),
            1.0F - (wave * 0.004F),
            1.0F + (wave * 0.004F)},
    };
}

EnemyPartPose sampleEnemyPartPose(
    const EnemyAnimationState state,
    const std::uint64_t animationTick,
    const wkc::core::Seed visualSeed,
    const std::uint32_t partIndex,
    const std::uint32_t partCount,
    const float hitReactionStrength,
    const bool active,
    const EnemyVisualRole role) noexcept
{
    if (partIndex >= partCount || partCount == 0U) {
        return {};
    }
    if (state == EnemyAnimationState::Dead) {
        return terminalDeathPartPose(visualSeed, role, partIndex, partCount);
    }
    if (!active) {
        return {};
    }

    const float safeHitStrength = std::isfinite(hitReactionStrength)
        ? std::clamp(hitReactionStrength, 0.0F, 1.0F) : 0.0F;
    const EnemyMotionProfile profile = motionProfileFor(role);
    if (state == EnemyAnimationState::Hit) {
        if (partIndex == 0U) {
            return {{0.0F, 0.0F, profile.hitTorsoTravel * safeHitStrength},
                {profile.hitTorsoPitch * safeHitStrength, 0.0F, 0.0F}};
        }
        if (partIndex == 1U) {
            return {{}, {profile.hitHeadPitch * safeHitStrength, 0.0F, 0.0F}};
        }
        if (partIndex == 2U || partIndex == 3U) {
            return {{}, {profile.hitArmPitch * safeHitStrength, 0.0F, 0.0F}};
        }
        if (partIndex + 1U == partCount) {
            return {{0.0F, 0.0F,
                profile.hitToolTravel * safeHitStrength}, {}};
        }
        return {};
    }
    if (state == EnemyAnimationState::Alert) {
        const float readiness = alertReadinessScale(role);
        if (partIndex == 0U) {
            return {{}, {-2.0F * readiness, 0.0F, 0.0F}};
        }
        if (partIndex == 1U) {
            return {{}, {-4.0F * readiness, 0.0F, 0.0F}};
        }
        if (partIndex == 2U || partIndex == 3U) {
            return {{}, {-8.0F * readiness, 0.0F, 0.0F}};
        }
        if (partIndex + 1U == partCount) {
            return {{0.0F, 0.0F,
                profile.attackToolTravel * 0.35F * readiness}, {}};
        }
        return {};
    }

    constexpr float tau = 6.28318530717958647692F;
    std::uint64_t period = profile.idlePeriod;
    if (state == EnemyAnimationState::Walk) {
        period = profile.walkPeriod;
    } else if (state == EnemyAnimationState::Attack) {
        period = profile.attackPeriod;
    } else if (state == EnemyAnimationState::Recover) {
        period = 72U;
    }
    const std::uint64_t phaseTick
        = (animationTick + (visualSeed % period)) % period;
    const float phase = static_cast<float>(phaseTick)
        / static_cast<float>(period) * tau;
    const float wave = std::sin(phase);

    if (state == EnemyAnimationState::Recover) {
        const float readiness = alertReadinessScale(role);
        if (partIndex == 0U) {
            return {{}, {-0.8F * readiness, 0.0F, 0.0F}};
        }
        if (partIndex == 1U) {
            return {{}, {0.0F, wave * 14.0F * readiness, 0.0F}};
        }
        if (partIndex == 2U || partIndex == 3U) {
            return {{}, {-2.0F * readiness, 0.0F, 0.0F}};
        }
        if (partIndex + 1U == partCount) {
            return {{0.0F, 0.0F,
                profile.attackToolTravel * 0.10F * readiness}, {}};
        }
        return {};
    }

    if (state == EnemyAnimationState::Walk) {
        if (partIndex == 2U) {
            return {{}, {wave * profile.leftArmWalk, 0.0F, 0.0F}};
        }
        if (partIndex == 3U) {
            return {{}, {-wave * profile.rightArmWalk, 0.0F, 0.0F}};
        }
        if (partIndex == 4U) {
            return {{}, {-wave * profile.leftLegWalk, 0.0F, 0.0F}};
        }
        if (partIndex == 5U) {
            return {{}, {wave * profile.rightLegWalk, 0.0F, 0.0F}};
        }
        return {};
    }
    if (state == EnemyAnimationState::Attack) {
        const float impulse = std::max(wave, 0.0F);
        if (partIndex == 2U || partIndex == 3U) {
            return {{}, {profile.attackArmPitch * impulse, 0.0F, 0.0F}};
        }
        if (partIndex + 1U == partCount) {
            return {{0.0F, 0.0F, profile.attackToolTravel * impulse}, {}};
        }
        return {};
    }
    if (state == EnemyAnimationState::Idle && partIndex == 1U) {
        return {{}, {0.0F, wave * profile.idleHeadYaw, 0.0F}};
    }
    return {};
}

bool UiRect::contains(const int pointX, const int pointY) const noexcept
{
    return pointX >= x && pointX < x + width
        && pointY >= y && pointY < y + height;
}

UiVisualTheme industrialUiTheme() noexcept
{
    return {
        .background = {0.004F, 0.010F, 0.014F},
        .panel = {0.014F, 0.028F, 0.035F},
        .panelBorder = {0.075F, 0.145F, 0.165F},
        .selection = {0.055F, 0.205F, 0.225F},
        .text = {0.78F, 0.88F, 0.90F},
        .mutedText = {0.42F, 0.56F, 0.60F},
        .accent = {0.18F, 0.88F, 0.94F},
        .warning = {0.98F, 0.62F, 0.14F},
        .danger = {0.96F, 0.20F, 0.10F},
        .success = {0.24F, 0.92F, 0.56F},
    };
}

MainMenuLayout makeMainMenuLayout(const int width, const int height) noexcept
{
    const int safeWidth = std::max(width, 1);
    const int safeHeight = std::max(height, 1);
    const int scale = safeWidth >= 640 && safeHeight >= 360 ? 2 : 1;
    const int panelWidth = std::max(1, std::min(safeWidth - 16, 240 * scale));
    const int panelHeight = std::max(1, std::min(safeHeight - 16, 222 * scale));
    const int panelX = (safeWidth - panelWidth) / 2;
    const int panelY = (safeHeight - panelHeight) / 2;
    const int buttonWidth = std::max(1,
        std::min(panelWidth - (20 * scale), 180 * scale));
    const int buttonHeight = std::max(1, std::min(13 * scale, panelHeight));
    const int buttonX = (safeWidth - buttonWidth) / 2;
    const int maximumButtonY = panelY + panelHeight - buttonHeight;
    const int startY = std::clamp(
        panelY + (138 * scale), panelY, maximumButtonY);
    const int continueY = std::clamp(
        startY - (20 * scale), panelY, maximumButtonY);
    const int campaignY = std::clamp(
        startY - (40 * scale), panelY, maximumButtonY);
    const int speedrunY = std::clamp(
        startY - (60 * scale), panelY, maximumButtonY);
    const int bestSeedY = std::clamp(
        startY - (80 * scale), panelY, maximumButtonY);
    const int settingsY = std::clamp(
        startY - (100 * scale), panelY, maximumButtonY);
    const int exitY = std::clamp(
        startY - (120 * scale), panelY, maximumButtonY);
    return {
        scale,
        {panelX, panelY, panelWidth, panelHeight},
        {buttonX, startY, buttonWidth, buttonHeight},
        {buttonX, continueY, buttonWidth, buttonHeight},
        {buttonX, campaignY, buttonWidth, buttonHeight},
        {buttonX, speedrunY, buttonWidth, buttonHeight},
        {buttonX, bestSeedY, buttonWidth, buttonHeight},
        {buttonX, settingsY, buttonWidth, buttonHeight},
        {buttonX, exitY, buttonWidth, buttonHeight},
    };
}

std::optional<HudMainMenuSelection> mainMenuSelectionAt(
    const int width,
    const int height,
    const float pointerX,
    const float pointerY) noexcept
{
    if (width <= 0 || height <= 0 || !std::isfinite(pointerX)
        || !std::isfinite(pointerY)) {
        return std::nullopt;
    }
    const int pixelX = static_cast<int>(pointerX);
    const int pixelY = height - 1 - static_cast<int>(pointerY);
    const MainMenuLayout layout = makeMainMenuLayout(width, height);
    if (layout.startButton.contains(pixelX, pixelY)) {
        return HudMainMenuSelection::StartRun;
    }
    if (layout.continueButton.contains(pixelX, pixelY)) {
        return HudMainMenuSelection::ContinueCampaign;
    }
    if (layout.campaignButton.contains(pixelX, pixelY)) {
        return HudMainMenuSelection::Campaign;
    }
    if (layout.speedrunButton.contains(pixelX, pixelY)) {
        return HudMainMenuSelection::Speedrun;
    }
    if (layout.bestSeedButton.contains(pixelX, pixelY)) {
        return HudMainMenuSelection::BestSeed;
    }
    if (layout.settingsButton.contains(pixelX, pixelY)) {
        return HudMainMenuSelection::Settings;
    }
    if (layout.exitButton.contains(pixelX, pixelY)) {
        return HudMainMenuSelection::ExitGame;
    }
    return std::nullopt;
}

PauseMenuLayout makePauseMenuLayout(const int width, const int height) noexcept
{
    const int safeWidth = std::max(width, 1);
    const int safeHeight = std::max(height, 1);
    const int scale = safeWidth >= 640 && safeHeight >= 360 ? 2 : 1;
    const int panelWidth = std::max(1,
        std::min(safeWidth - 16, 180 * scale));
    const int panelHeight = std::max(1,
        std::min(safeHeight - 16, 123 * scale));
    const int panelX = (safeWidth - panelWidth) / 2;
    const int panelY = (safeHeight - panelHeight) / 2;
    const int buttonWidth = std::max(1,
        std::min(panelWidth - (20 * scale), 112 * scale));
    const int buttonHeight = std::max(1,
        std::min(11 * scale, panelHeight));
    const int buttonX = (safeWidth - buttonWidth) / 2;
    const int maximumButtonY = panelY + panelHeight - buttonHeight;
    const int resumeY = std::clamp(
        panelY + panelHeight - (36 * scale), panelY, maximumButtonY);
    const int restartY = std::clamp(
        resumeY - (14 * scale), panelY, maximumButtonY);
    const int settingsY = std::clamp(
        resumeY - (28 * scale), panelY, maximumButtonY);
    const int mainMenuY = std::clamp(
        resumeY - (42 * scale), panelY, maximumButtonY);
    const int exitY = std::clamp(
        resumeY - (56 * scale), panelY, maximumButtonY);
    return {
        scale,
        {panelX, panelY, panelWidth, panelHeight},
        {buttonX, resumeY, buttonWidth, buttonHeight},
        {buttonX, restartY, buttonWidth, buttonHeight},
        {buttonX, settingsY, buttonWidth, buttonHeight},
        {buttonX, mainMenuY, buttonWidth, buttonHeight},
        {buttonX, exitY, buttonWidth, buttonHeight},
    };
}

std::optional<HudPauseSelection> pauseMenuSelectionAt(
    const int width,
    const int height,
    const float pointerX,
    const float pointerY) noexcept
{
    if (width <= 0 || height <= 0 || !std::isfinite(pointerX)
        || !std::isfinite(pointerY)) {
        return std::nullopt;
    }
    const int pixelX = static_cast<int>(pointerX);
    const int pixelY = height - 1 - static_cast<int>(pointerY);
    const PauseMenuLayout layout = makePauseMenuLayout(width, height);
    if (layout.resumeButton.contains(pixelX, pixelY)) {
        return HudPauseSelection::Resume;
    }
    if (layout.restartButton.contains(pixelX, pixelY)) {
        return HudPauseSelection::RestartRun;
    }
    if (layout.settingsButton.contains(pixelX, pixelY)) {
        return HudPauseSelection::Settings;
    }
    if (layout.mainMenuButton.contains(pixelX, pixelY)) {
        return HudPauseSelection::MainMenu;
    }
    if (layout.exitButton.contains(pixelX, pixelY)) {
        return HudPauseSelection::ExitGame;
    }
    return std::nullopt;
}

ResultMenuLayout makeResultMenuLayout(const int width, const int height) noexcept
{
    const int safeWidth = std::max(width, 1);
    const int safeHeight = std::max(height, 1);
    const int scale = safeWidth >= 640 && safeHeight >= 360 ? 2 : 1;
    const int panelWidth = std::max(1,
        std::min(safeWidth - 16, 220 * scale));
    const int panelHeight = std::max(1,
        std::min(safeHeight - 16, 162 * scale));
    const int panelX = (safeWidth - panelWidth) / 2;
    const int panelY = (safeHeight - panelHeight) / 2;
    const int buttonWidth = std::max(1,
        std::min(panelWidth - (20 * scale), 112 * scale));
    const int buttonHeight = std::max(1,
        std::min(13 * scale, panelHeight));
    const int buttonX = (safeWidth - buttonWidth) / 2;
    const int maximumButtonY = panelY + panelHeight - buttonHeight;
    const int playAgainY = std::clamp(
        panelY + (78 * scale), panelY, maximumButtonY);
    const int replaySeedY = std::clamp(
        playAgainY - (18 * scale), panelY, maximumButtonY);
    const int mainMenuY = std::clamp(
        playAgainY - (36 * scale), panelY, maximumButtonY);
    const int exitY = std::clamp(
        playAgainY - (54 * scale), panelY, maximumButtonY);
    return {
        scale,
        {panelX, panelY, panelWidth, panelHeight},
        {buttonX, playAgainY, buttonWidth, buttonHeight},
        {buttonX, replaySeedY, buttonWidth, buttonHeight},
        {buttonX, mainMenuY, buttonWidth, buttonHeight},
        {buttonX, exitY, buttonWidth, buttonHeight},
    };
}

std::optional<HudResultMenuSelection> resultMenuSelectionAt(
    const int width,
    const int height,
    const float pointerX,
    const float pointerY) noexcept
{
    if (width <= 0 || height <= 0 || !std::isfinite(pointerX)
        || !std::isfinite(pointerY)) {
        return std::nullopt;
    }
    const int pixelX = static_cast<int>(pointerX);
    const int pixelY = height - 1 - static_cast<int>(pointerY);
    const ResultMenuLayout layout = makeResultMenuLayout(width, height);
    if (layout.playAgainButton.contains(pixelX, pixelY)) {
        return HudResultMenuSelection::PlayAgain;
    }
    if (layout.replaySeedButton.contains(pixelX, pixelY)) {
        return HudResultMenuSelection::ReplaySeed;
    }
    if (layout.mainMenuButton.contains(pixelX, pixelY)) {
        return HudResultMenuSelection::MainMenu;
    }
    if (layout.exitButton.contains(pixelX, pixelY)) {
        return HudResultMenuSelection::ExitGame;
    }
    return std::nullopt;
}

ExitConfirmationLayout makeExitConfirmationLayout(
    const int width,
    const int height) noexcept
{
    const int safeWidth = std::max(width, 1);
    const int safeHeight = std::max(height, 1);
    const int scale = safeWidth >= 640 && safeHeight >= 360 ? 2 : 1;
    const int panelWidth = std::max(1,
        std::min(safeWidth - 16, 210 * scale));
    const int panelHeight = std::max(1,
        std::min(safeHeight - 16, 84 * scale));
    const int panelX = (safeWidth - panelWidth) / 2;
    const int panelY = (safeHeight - panelHeight) / 2;
    const int buttonWidth = std::max(1,
        std::min((panelWidth - (30 * scale)) / 2, 76 * scale));
    const int buttonHeight = std::max(1,
        std::min(13 * scale, panelHeight));
    const int maximumButtonX = panelX + panelWidth - buttonWidth;
    const int maximumButtonY = panelY + panelHeight - buttonHeight;
    const int gap = 8 * scale;
    const int cancelX = std::clamp(
        (safeWidth / 2) - buttonWidth - (gap / 2),
        panelX, maximumButtonX);
    const int exitX = std::clamp(
        (safeWidth / 2) + (gap / 2), panelX, maximumButtonX);
    const int buttonY = std::clamp(
        panelY + (16 * scale), panelY, maximumButtonY);
    return {
        scale,
        {panelX, panelY, panelWidth, panelHeight},
        {cancelX, buttonY, buttonWidth, buttonHeight},
        {exitX, buttonY, buttonWidth, buttonHeight},
    };
}

std::optional<HudExitConfirmationSelection> exitConfirmationSelectionAt(
    const int width,
    const int height,
    const float pointerX,
    const float pointerY) noexcept
{
    if (width <= 0 || height <= 0 || !std::isfinite(pointerX)
        || !std::isfinite(pointerY)) {
        return std::nullopt;
    }
    const int pixelX = static_cast<int>(pointerX);
    const int pixelY = height - 1 - static_cast<int>(pointerY);
    const ExitConfirmationLayout layout
        = makeExitConfirmationLayout(width, height);
    if (layout.cancelButton.contains(pixelX, pixelY)) {
        return HudExitConfirmationSelection::Cancel;
    }
    if (layout.exitButton.contains(pixelX, pixelY)) {
        return HudExitConfirmationSelection::ExitGame;
    }
    return std::nullopt;
}

SettingsMenuLayout makeSettingsMenuLayout(const int width, const int height) noexcept
{
    const int safeWidth = std::max(width, 1);
    const int safeHeight = std::max(height, 1);
    const int scale = safeWidth >= 640 && safeHeight >= 520 ? 2 : 1;
    const int panelWidth = std::max(1,
        std::min(safeWidth - 16, 300 * scale));
    const int panelHeight = std::max(1,
        std::min(safeHeight - 16, 244 * scale));
    const int panelX = (safeWidth - panelWidth) / 2;
    const int panelY = (safeHeight - panelHeight) / 2;
    const int rowWidth = std::max(1,
        std::min(panelWidth - (20 * scale), 250 * scale));
    const int desiredRowHeight = std::max(1, std::min(13 * scale, panelHeight));
    const int rowX = (safeWidth - rowWidth) / 2;
    const int maximumRowY = panelY + panelHeight - desiredRowHeight;
    const int bottomRowY = std::clamp(
        panelY + (24 * scale), panelY, maximumRowY);
    const int topRowLimit = std::clamp(
        panelY + panelHeight - (26 * scale) - desiredRowHeight,
        bottomRowY, maximumRowY);
    constexpr int rowCount
        = static_cast<int>(HudSettingsSelection::Count);
    const int rowPitch = rowCount > 1
        ? std::min(18 * scale,
            (topRowLimit - bottomRowY) / (rowCount - 1))
        : 0;
    const int rowHeight = rowCount > 1
        ? std::min(desiredRowHeight, std::max(1, rowPitch))
        : desiredRowHeight;

    SettingsMenuLayout layout;
    layout.scale = scale;
    layout.panel = {panelX, panelY, panelWidth, panelHeight};
    for (std::size_t index = 0U; index < layout.rows.size(); ++index) {
        const int rowY = bottomRowY
            + ((rowCount - 1 - static_cast<int>(index)) * rowPitch);
        layout.rows[index] = {rowX, rowY, rowWidth, rowHeight};
        if (index < static_cast<std::size_t>(HudSettingsSelection::Back)
            && rowWidth >= 3) {
            const int adjustmentWidth = std::min(rowWidth, 104 * scale);
            const int arrowWidth = std::max(1,
                std::min(rowHeight, adjustmentWidth / 4));
            const int adjustmentX = rowX + rowWidth - adjustmentWidth;
            layout.decrementButtons[index]
                = {adjustmentX, rowY, arrowWidth, rowHeight};
            layout.incrementButtons[index]
                = {rowX + rowWidth - arrowWidth, rowY, arrowWidth, rowHeight};
        }
    }
    return layout;
}

std::optional<HudSettingsSelection> settingsMenuSelectionAt(
    const int width,
    const int height,
    const float pointerX,
    const float pointerY) noexcept
{
    if (width <= 0 || height <= 0 || !std::isfinite(pointerX)
        || !std::isfinite(pointerY)) {
        return std::nullopt;
    }
    const int pixelX = static_cast<int>(pointerX);
    const int pixelY = height - 1 - static_cast<int>(pointerY);
    const SettingsMenuLayout layout = makeSettingsMenuLayout(width, height);
    for (std::size_t index = 0U; index < layout.rows.size(); ++index) {
        if (layout.rows[index].contains(pixelX, pixelY)) {
            return static_cast<HudSettingsSelection>(index);
        }
    }
    return std::nullopt;
}

std::optional<int> settingsMenuAdjustmentDirectionAt(
    const int width,
    const int height,
    const float pointerX,
    const float pointerY) noexcept
{
    if (width <= 0 || height <= 0 || !std::isfinite(pointerX)
        || !std::isfinite(pointerY)) {
        return std::nullopt;
    }
    const int pixelX = static_cast<int>(pointerX);
    const int pixelY = height - 1 - static_cast<int>(pointerY);
    const SettingsMenuLayout layout = makeSettingsMenuLayout(width, height);
    for (std::size_t index = 0U; index < layout.rows.size(); ++index) {
        if (layout.decrementButtons[index].contains(pixelX, pixelY)) {
            return -1;
        }
        if (layout.incrementButtons[index].contains(pixelX, pixelY)) {
            return 1;
        }
    }
    return std::nullopt;
}

bool isSpherePotentiallyVisible(
    const CameraData& camera,
    const wkc::core::Vec3 center,
    const float radius,
    const float aspectRatio) noexcept
{
    const auto finiteVec3 = [](const wkc::core::Vec3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y)
            && std::isfinite(value.z);
    };
    if (!finiteVec3(camera.position) || !finiteVec3(camera.forward)
        || !finiteVec3(center) || !std::isfinite(radius) || radius < 0.0F
        || !std::isfinite(aspectRatio) || aspectRatio <= 0.0F
        || !std::isfinite(camera.verticalFieldOfViewDegrees)) {
        return true;
    }

    const wkc::core::Vec3 forward = wkc::core::normalized(camera.forward);
    if (wkc::core::dot(forward, forward) <= 1.0e-6F) {
        return true;
    }
    const wkc::core::Vec3 offset = center - camera.position;
    const float distance = wkc::core::length(offset);
    if (distance <= radius + 1.0e-4F) {
        return true;
    }

    constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0F;
    constexpr float safetyMarginDegrees = 14.0F;
    const float verticalHalfAngle = std::clamp(
        camera.verticalFieldOfViewDegrees * 0.5F, 1.0F, 89.0F)
        * degreesToRadians;
    const float horizontalHalfAngle = std::atan(
        std::tan(verticalHalfAngle) * aspectRatio);
    const float viewHalfAngle = std::min(
        std::max(verticalHalfAngle, horizontalHalfAngle)
            + (safetyMarginDegrees * degreesToRadians),
        0.75F * std::numbers::pi_v<float>);
    const float angularRadius = std::asin(std::clamp(radius / distance, 0.0F, 1.0F));
    const float directionCosine = std::clamp(
        wkc::core::dot(offset * (1.0F / distance), forward), -1.0F, 1.0F);
    const float centerAngle = std::acos(directionCosine);
    return centerAngle <= viewHalfAngle + angularRadius;
}

bool isAabbPotentiallyVisible(
    const CameraData& camera,
    const wkc::core::Aabb& bounds,
    const float aspectRatio) noexcept
{
    if (!bounds.isValid()) {
        return true;
    }
    const wkc::core::Vec3 center
        = (bounds.minimum + bounds.maximum) * 0.5F;
    const float radius = wkc::core::length(
        (bounds.maximum - bounds.minimum) * 0.5F);
    return isSpherePotentiallyVisible(camera, center, radius, aspectRatio);
}

std::optional<wkc::core::Aabb> corridorRenderBounds(
    const CorridorRenderData& corridor) noexcept
{
    if (corridor.points.empty() || !std::isfinite(corridor.width)
        || !std::isfinite(corridor.height) || corridor.width <= 0.0F
        || corridor.height <= 0.0F) {
        return std::nullopt;
    }
    const auto finiteVec3 = [](const wkc::core::Vec3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y)
            && std::isfinite(value.z);
    };
    if (!finiteVec3(corridor.points.front())) {
        return std::nullopt;
    }
    wkc::core::Vec3 minimum = corridor.points.front();
    wkc::core::Vec3 maximum = corridor.points.front();
    for (const wkc::core::Vec3 point : corridor.points) {
        if (!finiteVec3(point)) {
            return std::nullopt;
        }
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        minimum.z = std::min(minimum.z, point.z);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
        maximum.z = std::max(maximum.z, point.z);
    }
    const float halfWidth = corridor.width * 0.5F;
    return wkc::core::Aabb{
        {minimum.x - halfWidth, minimum.y, minimum.z - halfWidth},
        {maximum.x + halfWidth, maximum.y + corridor.height,
            maximum.z + halfWidth},
    };
}

std::string runTimeText(const std::uint64_t elapsedTicks)
{
    constexpr std::uint64_t ticksPerSecond = 60U;
    const std::uint64_t totalSeconds = elapsedTicks / ticksPerSecond;
    const std::uint64_t centiseconds
        = ((elapsedTicks % ticksPerSecond) * 100U) / ticksPerSecond;
    const std::uint64_t seconds = totalSeconds % 60U;
    const std::uint64_t totalMinutes = totalSeconds / 60U;
    const std::uint64_t minutes = totalMinutes % 60U;
    const std::uint64_t hours = totalMinutes / 60U;

    std::ostringstream text;
    text << std::setfill('0');
    if (hours > 0U) {
        text << std::setw(2) << hours << ':';
    }
    text << std::setw(2) << (hours > 0U ? minutes : totalMinutes)
         << ':' << std::setw(2) << seconds
         << '.' << std::setw(2) << centiseconds;
    return text.str();
}

wkc::core::Vec3 impactVisualColor(
    const ImpactVisualMaterial material,
    const float strength) noexcept
{
    const float safeStrength = std::clamp(
        std::isfinite(strength) ? strength : 0.0F, 0.0F, 1.0F);
    wkc::core::Vec3 color;
    switch (material) {
    case ImpactVisualMaterial::Metal:
        color = {1.0F, 0.68F, 0.22F};
        break;
    case ImpactVisualMaterial::Composite:
        color = {0.72F, 0.48F, 0.25F};
        break;
    case ImpactVisualMaterial::Flesh:
        color = {0.78F, 0.10F, 0.07F};
        break;
    }
    return color * (0.35F + (safeStrength * 0.65F));
}

float impactVisualSize(const float strength) noexcept
{
    const float safeStrength = std::clamp(
        std::isfinite(strength) ? strength : 0.0F, 0.0F, 1.0F);
    return 0.025F + (safeStrength * 0.055F);
}

WeaponViewmodelPose sampleWeaponViewmodelPose(
    const WeaponViewmodelRenderData& weapon) noexcept
{
    const auto sanitized = [](const float value) {
        return std::clamp(std::isfinite(value) ? value : 0.0F, 0.0F, 1.0F);
    };
    const float recoil = sanitized(weapon.recoilStrength);
    const float reload = sanitized(weapon.reloadProgress);
    const float equip = sanitized(weapon.equipStrength);
    const float melee = sanitized(weapon.meleeStrength);
    const float reloadArc = std::sin(reload * 3.14159265358979323846F);
    const float meleeArc = melee > 0.0F
        ? std::sin((1.0F - melee) * 3.14159265358979323846F)
        : 0.0F;
    const float mass = weapon.role == HudWeaponRole::Carbine ? 0.72F : 1.0F;
    return {
        .offset = {
            (-0.09F * reloadArc) - (0.18F * meleeArc),
            (-0.34F * equip) - (0.18F * reloadArc) + (0.025F * recoil),
            (-0.12F * recoil * mass) - (0.05F * reloadArc)
                + (0.20F * meleeArc),
        },
        .rotationDegrees = {
            (-8.0F * recoil * mass) + (12.0F * reloadArc)
                - (22.0F * meleeArc),
            (-8.0F * reloadArc) + (16.0F * meleeArc),
            (34.0F * reloadArc) - (58.0F * meleeArc),
        },
    };
}

ShadowBudgetResult optimizeShadowBudget(
    RoomLightingRenderData& lighting,
    const wkc::core::Vec3 observerPosition,
    const std::span<const ShadowOccluderRenderData> candidates) noexcept
{
    const std::size_t lightCount = std::min<std::size_t>(
        lighting.lightCount, maxRenderedRoomLights);
    std::stable_sort(
        lighting.lights.begin(), lighting.lights.begin() + lightCount,
        [observerPosition](const PointLightRenderData& left,
            const PointLightRenderData& right) {
            const bool leftActive = std::isfinite(left.intensity)
                && left.intensity > 1.0e-4F;
            const bool rightActive = std::isfinite(right.intensity)
                && right.intensity > 1.0e-4F;
            if (leftActive != rightActive) {
                return leftActive;
            }
            const wkc::core::Vec3 leftOffset = left.position - observerPosition;
            const wkc::core::Vec3 rightOffset = right.position - observerPosition;
            return wkc::core::dot(leftOffset, leftOffset)
                < wkc::core::dot(rightOffset, rightOffset);
        });

    std::size_t activeLights = 0U;
    while (activeLights < lightCount
        && std::isfinite(lighting.lights[activeLights].intensity)
        && lighting.lights[activeLights].intensity > 1.0e-4F) {
        ++activeLights;
    }
    lighting.shadowLightCount = static_cast<std::uint32_t>(
        std::min(activeLights, maxShadowCastingLights));

    struct RankedOccluder {
        ShadowOccluderRenderData occluder{};
        float distanceSquared{0.0F};
    };
    std::vector<RankedOccluder> ranked;
    ranked.reserve(candidates.size());
    const auto finiteVec3 = [](const wkc::core::Vec3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y)
            && std::isfinite(value.z);
    };
    for (const ShadowOccluderRenderData candidate : candidates) {
        if (!candidate.bounds.isValid()
            || !finiteVec3(candidate.bounds.minimum)
            || !finiteVec3(candidate.bounds.maximum)) {
            continue;
        }
        const wkc::core::Vec3 center
            = (candidate.bounds.minimum + candidate.bounds.maximum) * 0.5F;
        bool influencesShadowLight = false;
        for (std::size_t lightIndex = 0U;
             lightIndex < lighting.shadowLightCount; ++lightIndex) {
            const PointLightRenderData& light = lighting.lights[lightIndex];
            const wkc::core::Vec3 lightOffset = center - light.position;
            const float extentRadius = wkc::core::length(
                (candidate.bounds.maximum - candidate.bounds.minimum) * 0.5F);
            const float influenceRange = std::max(light.range, 0.0F) + extentRadius;
            if (wkc::core::dot(lightOffset, lightOffset)
                <= influenceRange * influenceRange) {
                influencesShadowLight = true;
                break;
            }
        }
        if (!influencesShadowLight) {
            continue;
        }
        const wkc::core::Vec3 observerOffset = center - observerPosition;
        ranked.push_back({candidate,
            wkc::core::dot(observerOffset, observerOffset)});
    }
    std::stable_sort(ranked.begin(), ranked.end(),
        [](const RankedOccluder& left, const RankedOccluder& right) {
            return left.distanceSquared < right.distanceSquared;
        });

    const std::size_t selected = std::min({
        ranked.size(), maxActiveShadowOccluders, maxShadowOccluders});
    for (std::size_t index = 0U; index < selected; ++index) {
        lighting.shadowOccluders[index] = ranked[index].occluder;
    }
    lighting.shadowOccluderCount = static_cast<std::uint32_t>(selected);
    return {
        static_cast<std::uint32_t>(activeLights),
        static_cast<std::uint32_t>(candidates.size()),
        static_cast<std::uint32_t>(selected),
    };
}

} // namespace gdp::game::visual
