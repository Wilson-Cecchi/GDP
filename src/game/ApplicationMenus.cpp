#include "game/Application.hpp"
#include "game/audio/AudioSystem.hpp"
#include "core/Log.hpp"
#include <SDL3/SDL.h>
#include <algorithm>

namespace gdp::game {
MenuView Application::frontendMenuView() const {
    return makeMenuView(menuFlow_, campaignSlots_, speedrunLeaderboard_, runtimeSettings_, GDP_ENABLE_TEST_MODE != 0);
}
void Application::processFrontendMenu()
{
    using wkc::platform::Key;
    using wkc::platform::MouseButton;
    using wkc::platform::GamepadButton;
    playerCommand_ = {}; interactionPending_ = false; jumpPending_ = false;
    firePending_ = false; fireHeld_ = false; reloadPending_ = false;
    weaponSwitchPending_.reset(); dropPending_ = false;
    auto play = [&](gdp::audio::AudioEventKind kind) {
        if (audio_) audio_->playUi(kind, session_.state().currentSeed, uiAudioEventCounter_++);
    };
    if (menuFlow_.capture) {
        if (input_.wasKeyPressed(Key::Escape)
            || input_.wasGamepadButtonPressed(
                menuFlow_.captureGamepad ? GamepadButton::Start : GamepadButton::East)) {
            menuFlow_.capture.reset(); menuFlow_.captureGamepad = false;
            play(gdp::audio::AudioEventKind::UiBack);
        }
        else if (menuFlow_.captureGamepad) {
            const auto button = input_.wasKeyPressed(Key::Delete)
                ? std::optional{GamepadButton::Count}
                : gameplay::pressedGamepadBinding(input_);
            if (button) {
                if (gameplay::assignGamepadBinding(runtimeSettings_.gamepadControls,
                    *menuFlow_.capture, *button)) {
                    settingsDirty_ = true; (void)savePersistedSettings();
                }
                menuFlow_.capture.reset(); menuFlow_.captureGamepad = false;
                play(gdp::audio::AudioEventKind::UiConfirm);
            }
        } else if (auto binding = gameplay::pressedControlBinding(input_)) {
            if (gameplay::assignControlBinding(runtimeSettings_.controls, *menuFlow_.capture, *binding)) {
                settingsDirty_ = true; (void)savePersistedSettings();
            }
            menuFlow_.capture.reset(); play(gdp::audio::AudioEventKind::UiConfirm);
        }
        return;
    }
    auto view = frontendMenuView();
    if (view.rows.empty()) return;
    if (menuFlow_.selection >= view.rows.size() || !view.rows[menuFlow_.selection].enabled) {
        menuFlow_.selection = view.rows.size() - 1U;
        if (!view.rows[menuFlow_.selection].enabled) moveMenuSelection(menuFlow_, view, 1);
    }
    std::optional<std::size_t> pointed;
    float pointerX = 0.0F;
    float pointerY = 0.0F;
    int dw = 0, dh = 0;
    if (window_) {
        int ww = 0, wh = 0;
        if (SDL_GetWindowSize(window_, &ww, &wh) && SDL_GetWindowSizeInPixels(window_, &dw, &dh) && ww > 0 && wh > 0) {
            const auto pointer = input_.mousePosition();
            pointerX = pointer.x * static_cast<float>(dw) / static_cast<float>(ww);
            pointerY = pointer.y * static_cast<float>(dh) / static_cast<float>(wh);
            pointed = visual::listMenuSelectionAt(dw, dh, view.rows.size(), pointerX,
                pointerY, menuFlow_.page == MenuPage::Root);
        }
    }
    if (pointed && view.rows[*pointed].enabled && input_.mouseDelta() != wkc::core::Vec2{}) {
        if (menuFlow_.selection != *pointed) play(gdp::audio::AudioEventKind::UiNavigate);
        menuFlow_.selection = *pointed;
    }
    const bool up = input_.wasKeyPressed(Key::Up) || input_.wasKeyPressed(Key::W)
        || input_.wasGamepadButtonPressed(GamepadButton::DpadUp)
        || input_.wasGamepadButtonPressed(GamepadButton::StickUp);
    const bool down = input_.wasKeyPressed(Key::Down) || input_.wasKeyPressed(Key::S)
        || input_.wasGamepadButtonPressed(GamepadButton::DpadDown)
        || input_.wasGamepadButtonPressed(GamepadButton::StickDown);
    if (up != down) { moveMenuSelection(menuFlow_, view, up ? -1 : 1); play(gdp::audio::AudioEventKind::UiNavigate); }
    const bool click = pointed && view.rows[*pointed].enabled && input_.wasMouseButtonPressed(MouseButton::Primary);
    if (click) menuFlow_.selection = *pointed;
    auto& row = view.rows[menuFlow_.selection];
    const bool left = input_.wasKeyPressed(Key::Left) || input_.wasKeyPressed(Key::A)
        || input_.wasGamepadButtonPressed(GamepadButton::DpadLeft)
        || input_.wasGamepadButtonPressed(GamepadButton::StickLeft);
    const bool right = input_.wasKeyPressed(Key::Right) || input_.wasKeyPressed(Key::D)
        || input_.wasGamepadButtonPressed(GamepadButton::DpadRight)
        || input_.wasGamepadButtonPressed(GamepadButton::StickRight);
    const auto mouseDirection = click ? visual::listMenuAdjustmentDirectionAt(dw, dh,
        view.rows.size(), pointerX, pointerY, menuFlow_.page == MenuPage::Root) : std::nullopt;
    if (row.setting && (left != right || mouseDirection)) {
        const int direction = mouseDirection.value_or(left ? -1 : 1);
        if (gameplay::adjustRuntimeSetting(runtimeSettings_, *row.setting, direction)) {
            if ((!runtimeSettings_.gamepadVibration || runtimeSettings_.gamepadVibrationPercent == 0U)
                && (*row.setting == gameplay::SettingsMenuOption::GamepadVibration
                    || *row.setting == gameplay::SettingsMenuOption::GamepadVibrationStrength))
                gamepad_.stopRumble();
            settingsDirty_ = true; applyRuntimeSettings();
            (void)savePersistedSettings(); play(gdp::audio::AudioEventKind::UiNavigate);
        }
    }
    MenuCommand command = MenuCommand::None;
    if (input_.wasKeyPressed(Key::Escape)
        || input_.wasGamepadButtonPressed(GamepadButton::East)) {
        command = backMenu(menuFlow_); play(gdp::audio::AudioEventKind::UiBack);
    } else if ((input_.wasKeyPressed(Key::Enter)
        || input_.wasGamepadButtonPressed(GamepadButton::South) || click)
        && !row.setting) {
        if ((menuFlow_.page == MenuPage::Bindings || menuFlow_.page == MenuPage::MoreBindings
            || menuFlow_.page == MenuPage::GamepadBindings || menuFlow_.page == MenuPage::MoreGamepadBindings)
            && menuFlow_.selection == 12U) {
            if (menuFlow_.page == MenuPage::GamepadBindings || menuFlow_.page == MenuPage::MoreGamepadBindings)
                runtimeSettings_.gamepadControls = {};
            else runtimeSettings_.controls = {};
            settingsDirty_ = true; (void)savePersistedSettings();
            menuFlow_.notice = "DEFAULT BINDINGS RESTORED";
        } else command = activateMenu(menuFlow_, view, campaignSlots_);
        play(gdp::audio::AudioEventKind::UiConfirm);
    }
    if (command == MenuCommand::OpenSettings) {
        gameplay::openSettingsMenu(settingsMenu_, gameplay::SettingsMenuOrigin::MainMenu);
    } else if (command == MenuCommand::CloseSettings) {
        gameplay::closeSettingsMenu(settingsMenu_); (void)savePersistedSettings();
    } else if (command == MenuCommand::ExitGame) {
        gameplay::openExitConfirmation(exitConfirmation_);
    } else if (command == MenuCommand::TestMode || command == MenuCommand::ContinueCampaign
        || command == MenuCommand::NewCampaign || command == MenuCommand::StartSpeedrun || command == MenuCommand::BestSeed) {
        campaignCarryover_.reset(); campaignFloorEntry_.reset();
        session_ = gameplay::SessionSystem{config_.worldSeed};
        bool accepted = false;
        if (command == MenuCommand::ContinueCampaign) {
            const auto checkpoint = campaignSlots_[menuFlow_.slot].checkpoint;
            if (!checkpoint) return;
            accepted = session_.restoreCampaignCheckpoint(checkpoint->initialSeed, checkpoint->currentSeed,
                checkpoint->runIndex, checkpoint->seedIndex, checkpoint->floor, checkpoint->difficulty);
            if (accepted) { config_.worldSeed = checkpoint->currentSeed; campaignCarryover_ = checkpoint->floorEntry; }
        } else {
            auto mode = command == MenuCommand::TestMode ? gameplay::RunMode::TestMode
                : command == MenuCommand::NewCampaign ? gameplay::RunMode::Campaign : gameplay::RunMode::Speedrun;
            accepted = session_.selectRunMode(mode);
            if (command == MenuCommand::NewCampaign) accepted = accepted && session_.selectDifficulty(menuFlow_.difficulty);
            if (command == MenuCommand::BestSeed) {
                if (speedrunLeaderboard_.empty()) return;
                accepted = accepted && session_.selectRunSeed(speedrunLeaderboard_.front().seed);
                config_.worldSeed = session_.state().currentSeed;
            }
        }
        if (session_.state().mode == gameplay::RunMode::Campaign) {
            activeCampaignSlot_ = menuFlow_.slot;
            campaignCheckpointPath_ = campaignSlotPaths_[menuFlow_.slot];
            campaignCheckpoint_ = campaignSlots_[menuFlow_.slot].checkpoint;
        }
        if (!accepted || !initializeRun()) { fatalError_ = true; running_ = false; }
    }
}
}
