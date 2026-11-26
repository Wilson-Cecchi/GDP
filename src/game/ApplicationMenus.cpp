#include "game/Application.hpp"
#include "game/Localization.hpp"
#include "game/audio/AudioSystem.hpp"
#include "core/Log.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

namespace {
bool displaySupportsResolution(SDL_Window* window,
    const gdp::gameplay::WindowResolution resolution, const bool fullscreen)
{
    if (!window) return true;
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (!display) return true;
    const auto size = gdp::gameplay::windowResolutionSize(resolution);
    if (!fullscreen) {
        const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(display);
        return !desktop || (size.width <= desktop->w && size.height <= desktop->h);
    }
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
    bool supported = false;
    for (int i = 0; modes && i < count; ++i)
        supported |= modes[i]->w == size.width && modes[i]->h == size.height;
    SDL_free(modes);
    return supported;
}
}

namespace gdp::game {
MenuView Application::frontendMenuView() const {
    return makeMenuView(menuFlow_, campaignSlots_, speedrunLeaderboard_,
        runtimeSettings_, GDP_ENABLE_TEST_MODE != 0,
        !gamepad_.connected() || gamepad_.supportsRumble());
}
void Application::processFrontendMenu()
{
    using wkc::platform::Key;
    using wkc::platform::MouseButton;
    using wkc::platform::GamepadButton;
    playerCommand_ = {}; interactionPending_ = false; jumpPending_ = false;
    firePending_ = false; fireHeld_ = false; reloadPending_ = false;
    weaponSwitchPending_.reset(); dropPending_ = false;
    if (numericAdjustmentPendingSave_
        && !input_.isMouseButtonDown(MouseButton::Primary)
        && !input_.isKeyDown(Key::Left) && !input_.isKeyDown(Key::Right)
        && !input_.isKeyDown(Key::A) && !input_.isKeyDown(Key::D)
        && !input_.isGamepadButtonDown(GamepadButton::DpadLeft)
        && !input_.isGamepadButtonDown(GamepadButton::DpadRight)
        && !input_.isGamepadButtonDown(GamepadButton::StickLeft)
        && !input_.isGamepadButtonDown(GamepadButton::StickRight)) {
        (void)savePersistedSettings();
        numericAdjustmentPendingSave_ = false;
    }
    auto play = [&](gdp::audio::AudioEventKind kind) {
        if (audio_) audio_->playUi(kind, session_.state().currentSeed, uiAudioEventCounter_++);
    };
    if (playerFeedbackTransport_.sending()) {
        const auto result = playerFeedbackTransport_.poll();
        if (result != PlayerFeedbackResult::Pending) {
            menuFlow_.feedbackSending = false;
            if (result == PlayerFeedbackResult::Sent) {
                menuFlow_.feedbackDraft.clear();
                menuFlow_.notice = "FEEDBACK SENT - THANK YOU";
                menuFlow_.selection = 0U;
            } else menuFlow_.notice = "SEND FAILED - TRY AGAIN";
        }
    }
    if (menuFlow_.page == MenuPage::FeedbackCompose && menuFlow_.feedbackEditing) {
        if (input_.wasKeyPressed(Key::Backspace)) popUiCodepoint(menuFlow_.feedbackDraft);
        if (input_.wasKeyPressed(Key::Enter) || input_.wasKeyPressed(Key::Escape)
            || input_.wasGamepadButtonPressed(GamepadButton::East)) {
            SDL_StopTextInput(window_);
            menuFlow_.feedbackEditing = false;
            play(gdp::audio::AudioEventKind::UiConfirm);
        }
        return;
    }
    if (menuFlow_.feedbackSending) return;
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
                std::optional<gameplay::ControlAction> conflict;
                if (*button != GamepadButton::Count) {
                    for (std::size_t index = 0U;
                         index < runtimeSettings_.gamepadControls.values.size();
                         ++index) {
                        if (runtimeSettings_.gamepadControls.values[index] == *button
                            && index != static_cast<std::size_t>(*menuFlow_.capture)) {
                            conflict = static_cast<gameplay::ControlAction>(index);
                            break;
                        }
                    }
                }
                if (gameplay::assignGamepadBinding(runtimeSettings_.gamepadControls,
                    *menuFlow_.capture, *button)) {
                    settingsDirty_ = true; (void)savePersistedSettings();
                    if (conflict) menuFlow_.notice = "BINDING SWAPPED: "
                        + std::string{gameplay::controlActionName(*conflict)};
                }
                menuFlow_.capture.reset(); menuFlow_.captureGamepad = false;
                play(gdp::audio::AudioEventKind::UiConfirm);
            }
        } else if (auto binding = gameplay::pressedControlBinding(input_)) {
            std::optional<gameplay::ControlAction> conflict;
            for (std::size_t index = 0U;
                 index < runtimeSettings_.controls.values.size(); ++index) {
                if (runtimeSettings_.controls.values[index] == *binding
                    && index != static_cast<std::size_t>(*menuFlow_.capture)) {
                    conflict = static_cast<gameplay::ControlAction>(index);
                    break;
                }
            }
            if (gameplay::assignControlBinding(runtimeSettings_.controls, *menuFlow_.capture, *binding)) {
                settingsDirty_ = true; (void)savePersistedSettings();
                if (conflict) menuFlow_.notice = "BINDING SWAPPED: "
                    + std::string{gameplay::controlActionName(*conflict)};
            }
            menuFlow_.capture.reset(); play(gdp::audio::AudioEventKind::UiConfirm);
        }
        return;
    }
    if (settingsMenu_.active) {
        const bool backward = input_.wasGamepadButtonPressed(GamepadButton::LeftShoulder)
            || (input_.wasKeyPressed(Key::Tab)
                && input_.isKeyDown(Key::LeftShift));
        const bool forward = input_.wasGamepadButtonPressed(GamepadButton::RightShoulder)
            || (input_.wasKeyPressed(Key::Tab) && !backward);
        if (forward != backward) {
            const int current = configurationSection(menuFlow_);
            const int next = (current + (forward ? 1 : 3)) % 4;
            openConfigurationSection(menuFlow_, static_cast<std::uint8_t>(next));
            menuAdjustmentRepeat_.reset();
            sliderDraggingRow_.reset();
            (void)savePersistedSettings();
            numericAdjustmentPendingSave_ = false;
            play(gdp::audio::AudioEventKind::UiNavigate);
            return;
        }
        if (configurationSection(menuFlow_) == 2U) {
            const bool previousPage = input_.wasKeyPressed(Key::PageUp)
                || input_.wasGamepadButtonPressed(GamepadButton::LeftTrigger);
            const bool nextPage = input_.wasKeyPressed(Key::PageDown)
                || input_.wasGamepadButtonPressed(GamepadButton::RightTrigger);
            if (previousPage != nextPage) {
                const int current = configurationControlPage(menuFlow_);
                const int next = (current + (nextPage ? 1 : 2)) % 3;
                openConfigurationControlPage(menuFlow_,
                    static_cast<std::uint8_t>(next));
                menuAdjustmentRepeat_.reset();
                sliderDraggingRow_.reset();
                (void)savePersistedSettings();
                numericAdjustmentPendingSave_ = false;
                play(gdp::audio::AudioEventKind::UiNavigate);
                return;
            }
        }
    }
    auto view = frontendMenuView();
    if (view.rows.empty()) return;
    if (menuFlow_.selection >= view.rows.size() || !view.rows[menuFlow_.selection].enabled) {
        menuFlow_.selection = 0U;
        if (!view.rows[0U].enabled) moveMenuSelection(menuFlow_, view, 1);
    }
    const std::size_t layoutSelection = menuFlow_.selection;
    const bool controlSidebar = settingsMenu_.active
        && configurationSection(menuFlow_) == 2U;
    std::optional<std::size_t> pointed;
    float pointerX = 0.0F;
    float pointerY = 0.0F;
    int dw = 0, dh = 0;
    std::uint16_t uiScalePercent = 100U;
    if (window_) {
        int ww = 0, wh = 0;
        if (SDL_GetWindowSize(window_, &ww, &wh) && SDL_GetWindowSizeInPixels(window_, &dw, &dh) && ww > 0 && wh > 0) {
            const auto pointer = input_.mousePosition();
            pointerX = pointer.x * static_cast<float>(dw) / static_cast<float>(ww);
            pointerY = pointer.y * static_cast<float>(dh) / static_cast<float>(wh);
            uiScalePercent = gameplay::resolvedUiScalePercent(runtimeSettings_,
                dw, dh, ww, wh);
            pointed = visual::listMenuSelectionAt(dw, dh, view.rows.size(), pointerX,
                pointerY, menuFlow_.page == MenuPage::Root, uiScalePercent,
                layoutSelection, controlSidebar);
        }
    }
    if (settingsMenu_.active && window_
        && input_.wasMouseButtonPressed(MouseButton::Primary)) {
        if (const auto tab = visual::configurationTabAt(dw, dh, view.rows.size(),
                pointerX, pointerY, uiScalePercent, layoutSelection)) {
            openConfigurationSection(menuFlow_, *tab);
            menuAdjustmentRepeat_.reset();
            sliderDraggingRow_.reset();
            (void)savePersistedSettings();
            numericAdjustmentPendingSave_ = false;
            play(gdp::audio::AudioEventKind::UiNavigate);
            return;
        }
        if (controlSidebar) {
            if (const auto tab = visual::configurationControlTabAt(dw, dh,
                    view.rows.size(), pointerX, pointerY,
                    uiScalePercent, layoutSelection)) {
                openConfigurationControlPage(menuFlow_, *tab);
                menuAdjustmentRepeat_.reset();
                sliderDraggingRow_.reset();
                (void)savePersistedSettings();
                numericAdjustmentPendingSave_ = false;
                play(gdp::audio::AudioEventKind::UiNavigate);
                return;
            }
        }
    }
    if (!sliderDraggingRow_ && pointed && view.rows[*pointed].enabled
        && input_.mouseDelta() != wkc::core::Vec2{}) {
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
    const float wheel = input_.mouseWheelDelta();
    if (wheel != 0.0F) {
        const int direction = wheel > 0.0F ? -1 : 1;
        const int count = std::clamp(static_cast<int>(std::ceil(std::abs(wheel))), 1, 6);
        for (int i = 0; i < count; ++i)
            moveMenuSelection(menuFlow_, view, direction);
        menuAdjustmentRepeat_.reset();
        sliderDraggingRow_.reset();
        play(gdp::audio::AudioEventKind::UiNavigate);
    }
    const bool click = pointed && view.rows[*pointed].enabled && input_.wasMouseButtonPressed(MouseButton::Primary);
    if (click) menuFlow_.selection = *pointed;
    auto& row = view.rows[menuFlow_.selection];
    const bool left = input_.wasKeyPressed(Key::Left) || input_.wasKeyPressed(Key::A)
        || input_.wasGamepadButtonPressed(GamepadButton::DpadLeft)
        || input_.wasGamepadButtonPressed(GamepadButton::StickLeft);
    const bool right = input_.wasKeyPressed(Key::Right) || input_.wasKeyPressed(Key::D)
        || input_.wasGamepadButtonPressed(GamepadButton::DpadRight)
        || input_.wasGamepadButtonPressed(GamepadButton::StickRight);
    const bool mouseHeld = input_.isMouseButtonDown(MouseButton::Primary);
    if (!mouseHeld) sliderDraggingRow_.reset();
    int mouseDirection = 0;
    if (mouseHeld && pointed && *pointed == menuFlow_.selection) {
        const auto direction = visual::listMenuAdjustmentDirectionAt(dw, dh,
            view.rows.size(), pointerX, pointerY,
            menuFlow_.page == MenuPage::Root, uiScalePercent, layoutSelection,
            controlSidebar);
        if (direction) mouseDirection = *direction;
    }
    bool settingChanged = false;
    bool numericAdjustment = false;
    if (row.setting) {
        const auto descriptor = gameplay::numericSettingDescriptor(
            runtimeSettings_, *row.setting);
        numericAdjustment = descriptor.has_value();
        if (descriptor && click) {
            if (visual::listMenuSliderFractionAt(dw, dh, view.rows.size(),
                    menuFlow_.selection, pointerX, pointerY,
                    uiScalePercent, layoutSelection, controlSidebar)) {
                sliderDraggingRow_ = menuFlow_.selection;
                menuAdjustmentRepeat_.reset();
            }
        }
        if (descriptor && sliderDraggingRow_ == menuFlow_.selection && mouseHeld) {
            const auto layout = visual::makeListMenuLayout(dw, dh,
                view.rows.size(), false, uiScalePercent, menuFlow_.selection,
                controlSidebar);
            const auto track = visual::listMenuSliderTrack(layout, menuFlow_.selection);
            if (track.width > 0) {
                const float fraction = std::clamp(
                    (pointerX - static_cast<float>(track.x))
                        / static_cast<float>(track.width), 0.0F, 1.0F);
                const int value = static_cast<int>(std::lround(
                    static_cast<float>(descriptor->minimum)
                        + fraction * static_cast<float>(
                            descriptor->maximum - descriptor->minimum)));
                settingChanged = gameplay::setRuntimeNumericSetting(
                    runtimeSettings_, *row.setting, value);
            }
        } else if (descriptor) {
            const bool leftHeld = input_.isKeyDown(Key::Left)
                || input_.isKeyDown(Key::A)
                || input_.isGamepadButtonDown(GamepadButton::DpadLeft)
                || input_.isGamepadButtonDown(GamepadButton::StickLeft);
            const bool rightHeld = input_.isKeyDown(Key::Right)
                || input_.isKeyDown(Key::D)
                || input_.isGamepadButtonDown(GamepadButton::DpadRight)
                || input_.isGamepadButtonDown(GamepadButton::StickRight);
            const int direction = mouseDirection != 0 ? mouseDirection
                : leftHeld != rightHeld ? (leftHeld ? -1 : 1) : 0;
            const bool pressed = mouseDirection != 0
                ? click : direction < 0 ? left : right;
            const bool held = mouseDirection != 0
                ? mouseHeld : leftHeld != rightHeld;
            const std::uint64_t token =
                (static_cast<std::uint64_t>(menuFlow_.page) << 32U)
                | (static_cast<std::uint64_t>(menuFlow_.selection) << 16U)
                | (mouseDirection != 0 ? 0x100U : 0U)
                | (direction > 0 ? 1U : 2U);
            const int steps = direction == 0 ? 0
                : menuAdjustmentRepeat_.advance(token,
                    MenuAdjustmentRepeat::Clock::now(), pressed, held);
            if (direction == 0) menuAdjustmentRepeat_.reset();
            for (int i = 0; i < steps; ++i) {
                if (!gameplay::adjustRuntimeSetting(runtimeSettings_,
                        *row.setting, direction)) {
                    menuAdjustmentRepeat_.reset();
                    break;
                }
                settingChanged = true;
            }
        } else {
            menuAdjustmentRepeat_.reset();
            if (left != right || (click && mouseDirection != 0)) {
                const int direction = mouseDirection != 0 ? mouseDirection : left ? -1 : 1;
                if (*row.setting == gameplay::SettingsMenuOption::WindowResolution) {
                    gameplay::RuntimeSettings candidate = runtimeSettings_;
                    for (int tries = 0;
                         tries < static_cast<int>(gameplay::WindowResolution::Count);
                         ++tries) {
                        if (!gameplay::adjustRuntimeSetting(candidate,
                                *row.setting, direction)) break;
                        if (displaySupportsResolution(window_, candidate.windowResolution,
                                candidate.fullscreen)) {
                            runtimeSettings_.windowResolution = candidate.windowResolution;
                            settingChanged = true;
                            break;
                        }
                    }
                } else {
                    settingChanged = gameplay::adjustRuntimeSetting(
                        runtimeSettings_, *row.setting, direction);
                }
            }
        }
        if (settingChanged) {
            if ((!runtimeSettings_.gamepadVibration || runtimeSettings_.gamepadVibrationPercent == 0U)
                && (*row.setting == gameplay::SettingsMenuOption::GamepadVibration
                    || *row.setting == gameplay::SettingsMenuOption::GamepadVibrationStrength))
                gamepad_.stopRumble();
            settingsDirty_ = true; applyRuntimeSettings();
            const auto updatedView = frontendMenuView();
            if (menuFlow_.selection < updatedView.rows.size()) {
                log(wkc::core::LogLevel::Info,
                    "Setting changed: " + updatedView.rows[menuFlow_.selection].text);
            }
            if (numericAdjustment) numericAdjustmentPendingSave_ = true;
            else (void)savePersistedSettings();
            play(gdp::audio::AudioEventKind::UiNavigate);
        }
    } else menuAdjustmentRepeat_.reset();
    MenuCommand command = MenuCommand::None;
    if (input_.wasKeyPressed(Key::Escape)
        || input_.wasGamepadButtonPressed(GamepadButton::East)) {
        menuAdjustmentRepeat_.reset();
        sliderDraggingRow_.reset();
        (void)savePersistedSettings();
        numericAdjustmentPendingSave_ = false;
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
        } else if (menuFlow_.page == MenuPage::FeedbackCompose
            && menuFlow_.selection + 2U == view.rows.size()) {
            const auto json = playerFeedbackJson(menuFlow_.feedbackTopic, menuFlow_.feedbackDraft,
                GDP_VERSION_STRING, SDL_GetPlatform());
            if (!json.empty() && playerFeedbackTransport_.start(json)) {
                menuFlow_.feedbackSending = true;
                menuFlow_.notice = "SENDING - PLEASE WAIT";
            } else menuFlow_.notice = "SEND FAILED - CURL UNAVAILABLE";
        } else {
            command = activateMenu(menuFlow_, view, campaignSlots_);
            if (menuFlow_.feedbackEditing) {
                if (SDL_StartTextInput(window_)) menuFlow_.notice.clear();
                else {
                    menuFlow_.feedbackEditing = false;
                    menuFlow_.notice = "TEXT INPUT UNAVAILABLE";
                }
            }
        }
        play(gdp::audio::AudioEventKind::UiConfirm);
    }
    if (command == MenuCommand::OpenSettings) {
        gameplay::openSettingsMenu(settingsMenu_, gameplay::SettingsMenuOrigin::MainMenu);
        openConfigurationSection(menuFlow_, 0U);
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
