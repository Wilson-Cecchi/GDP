#include "game/visual/GdpRenderData.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::game::visual {

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
    const int panelWidth = std::max(1, std::min(safeWidth - 16, 210 * scale));
    const int panelHeight = std::max(1, std::min(safeHeight - 16, 142 * scale));
    const int panelX = (safeWidth - panelWidth) / 2;
    const int panelY = (safeHeight - panelHeight) / 2;
    const int buttonWidth = std::max(1,
        std::min(panelWidth - (20 * scale), 112 * scale));
    const int buttonHeight = std::max(1, std::min(13 * scale, panelHeight));
    const int buttonX = (safeWidth - buttonWidth) / 2;
    const int maximumButtonY = panelY + panelHeight - buttonHeight;
    const int startY = std::clamp(
        panelY + (58 * scale), panelY, maximumButtonY);
    const int settingsY = std::clamp(
        startY - (20 * scale), panelY, maximumButtonY);
    const int exitY = std::clamp(
        startY - (40 * scale), panelY, maximumButtonY);
    return {
        scale,
        {panelX, panelY, panelWidth, panelHeight},
        {buttonX, startY, buttonWidth, buttonHeight},
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
        std::min(safeHeight - 16, 122 * scale));
    const int panelX = (safeWidth - panelWidth) / 2;
    const int panelY = (safeHeight - panelHeight) / 2;
    const int buttonWidth = std::max(1,
        std::min(panelWidth - (20 * scale), 112 * scale));
    const int buttonHeight = std::max(1,
        std::min(13 * scale, panelHeight));
    const int buttonX = (safeWidth - buttonWidth) / 2;
    const int maximumButtonY = panelY + panelHeight - buttonHeight;
    const int playAgainY = std::clamp(
        panelY + (54 * scale), panelY, maximumButtonY);
    const int mainMenuY = std::clamp(
        playAgainY - (18 * scale), panelY, maximumButtonY);
    const int exitY = std::clamp(
        playAgainY - (36 * scale), panelY, maximumButtonY);
    return {
        scale,
        {panelX, panelY, panelWidth, panelHeight},
        {buttonX, playAgainY, buttonWidth, buttonHeight},
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
    if (layout.mainMenuButton.contains(pixelX, pixelY)) {
        return HudResultMenuSelection::MainMenu;
    }
    if (layout.exitButton.contains(pixelX, pixelY)) {
        return HudResultMenuSelection::ExitGame;
    }
    return std::nullopt;
}

SettingsMenuLayout makeSettingsMenuLayout(const int width, const int height) noexcept
{
    const int safeWidth = std::max(width, 1);
    const int safeHeight = std::max(height, 1);
    const int scale = safeWidth >= 640 && safeHeight >= 480 ? 2 : 1;
    const int panelWidth = std::max(1,
        std::min(safeWidth - 16, 300 * scale));
    const int panelHeight = std::max(1,
        std::min(safeHeight - 16, 226 * scale));
    const int panelX = (safeWidth - panelWidth) / 2;
    const int panelY = (safeHeight - panelHeight) / 2;
    const int rowWidth = std::max(1,
        std::min(panelWidth - (20 * scale), 250 * scale));
    const int rowHeight = std::max(1, std::min(13 * scale, panelHeight));
    const int rowX = (safeWidth - rowWidth) / 2;
    const int maximumRowY = panelY + panelHeight - rowHeight;

    SettingsMenuLayout layout;
    layout.scale = scale;
    layout.panel = {panelX, panelY, panelWidth, panelHeight};
    for (std::size_t index = 0U; index < layout.rows.size(); ++index) {
        const int rowY = std::clamp(
            panelY + ((168 - (static_cast<int>(index) * 18)) * scale),
            panelY, maximumRowY);
        layout.rows[index] = {rowX, rowY, rowWidth, rowHeight};
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

} // namespace gdp::game::visual
