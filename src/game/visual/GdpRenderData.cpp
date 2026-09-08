#include "game/visual/GdpRenderData.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

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

std::uint32_t transitionDurationTicks(
    const EnemyAnimationState previous,
    const EnemyAnimationState current) noexcept
{
    if (current == EnemyAnimationState::Dead) {
        return 18U;
    }
    if (current == EnemyAnimationState::Hit) {
        return 3U;
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
    const EnemyAnimationState requestedState) noexcept
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
            transition.previousState, transition.currentState);
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
    return progress * progress * (3.0F - (2.0F * progress));
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

EnemyRootPose sampleEnemyRootPose(
    const EnemyAnimationState state,
    const std::uint64_t animationTick,
    const wkc::core::Seed visualSeed,
    const bool active) noexcept
{
    if (state == EnemyAnimationState::Dead) {
        const float fallDirection = (visualSeed & 1U) == 0U ? -1.0F : 1.0F;
        return {
            {0.0F, 0.22F, 0.0F},
            {1.0F, 1.0F, 1.0F},
            {0.0F, 0.0F, fallDirection * 82.0F},
        };
    }
    if (!active) {
        return {};
    }
    if (state == EnemyAnimationState::Hit) {
        return {};
    }

    constexpr float tau = 6.28318530717958647692F;
    std::uint64_t period = 120U;
    if (state == EnemyAnimationState::Walk) {
        period = 30U;
    } else if (state == EnemyAnimationState::Attack) {
        period = 24U;
    }
    const std::uint64_t phaseTick
        = (animationTick + (visualSeed % period)) % period;
    const float phase = static_cast<float>(phaseTick)
        / static_cast<float>(period) * tau;
    const float wave = std::sin(phase);

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
    if (!active || state == EnemyAnimationState::Dead
        || partIndex >= partCount || partCount == 0U) {
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

    constexpr float tau = 6.28318530717958647692F;
    std::uint64_t period = profile.idlePeriod;
    if (state == EnemyAnimationState::Walk) {
        period = profile.walkPeriod;
    } else if (state == EnemyAnimationState::Attack) {
        period = profile.attackPeriod;
    }
    const std::uint64_t phaseTick
        = (animationTick + (visualSeed % period)) % period;
    const float phase = static_cast<float>(phaseTick)
        / static_cast<float>(period) * tau;
    const float wave = std::sin(phase);

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

} // namespace gdp::game::visual
