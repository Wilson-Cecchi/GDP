#include "game/MenuFlow.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "game/Localization.hpp"
#include <algorithm>
#include <array>

namespace gdp::game {
namespace {
void open(MenuFlowState& s, MenuPage p) { s.page = p; s.selection = 0U; s.capture.reset(); s.captureGamepad = false; s.notice.clear(); }
std::array<std::string, 8> feedbackPreview(const std::string_view draft)
{
    std::vector<std::string> lines(1);
    std::size_t offset = 0U;
    while (offset < draft.size()) {
        const auto start = offset;
        const auto codepoint = nextUiCodepoint(draft, offset);
        if (codepoint == '\n' || lines.back().size() >= 128U || uiTextLength(lines.back()) >= 42U)
            lines.emplace_back();
        if (codepoint != '\n') lines.back().append(draft.substr(start, offset - start));
    }
    std::array<std::string, 8> result{};
    const auto first = lines.size() > result.size() ? lines.size() - result.size() : 0U;
    for (std::size_t i = 0; i < result.size(); ++i)
        if (first + i < lines.size()) result[i] = lines[first + i];
    if (first != 0U) result[0] = "..." + result[0];
    return result;
}
}
MenuView makeMenuView(const MenuFlowState& s, const std::array<CampaignSlotInfo, 3>& slots,
    const std::vector<gameplay::SpeedrunRecord>& board, const gameplay::RuntimeSettings& settings, const bool testMode)
{
    using gameplay::SettingsMenuOption;
    MenuView view;
    auto row = [&](std::string text, bool enabled = true) { view.rows.push_back({std::move(text), enabled, {}, {}, {}, {}}); };
    auto setting = [&](const char* name, SettingsMenuOption option, std::string value) {
        view.rows.push_back({std::string{name} + "  " + value, true, option, {}, {}, {}});
    };
    switch (s.page) {
    case MenuPage::Root:
        view.title = "IRON SEED"; view.subtitle = "INDUSTRIAL COMPLEX";
        if (testMode) { row("TEST MODE"); view.rows.back().command = MenuCommand::TestMode; }
        row("CAMPAIGN / STORY"); view.rows.back().destination = MenuPage::CampaignSlots;
        row("SPEEDRUN"); view.rows.back().destination = MenuPage::Speedrun;
        row("SETTINGS"); view.rows.back().destination = MenuPage::Settings;
        view.rows.back().command = MenuCommand::OpenSettings;
        row("SEND FEEDBACK"); view.rows.back().destination = MenuPage::FeedbackCategory;
        row("QUIT GAME"); view.rows.back().command = MenuCommand::ExitGame; break;
    case MenuPage::CampaignSlots:
        view.title = "CAMPAIGN / STORY"; view.subtitle = "3 SAVES  -  10 FLOORS";
        for (std::size_t i = 0; i < slots.size(); ++i) {
            std::string label = "SLOT " + std::to_string(i + 1U) + "  ";
            if (slots[i].checkpoint) label += "FLOOR " + std::to_string(slots[i].checkpoint->floor)
                + "/10  " + gameplay::difficultyName(slots[i].checkpoint->difficulty);
            else label += slots[i].occupied ? "UNAVAILABLE SAVE" : "EMPTY";
            row(label);
        }
        row("BACK"); break;
    case MenuPage::CampaignSlot:
        view.title = "CAMPAIGN - SLOT " + std::to_string(s.slot + 1U);
        view.subtitle = "CHECKPOINT: START OF EACH FLOOR";
        row("CONTINUE", slots[s.slot].checkpoint.has_value()); row("NEW GAME"); row("BACK"); break;
    case MenuPage::Difficulty:
        view.title = "BASE DIFFICULTY"; view.subtitle = "THREAT INCREASES EACH FLOOR";
        row("EASY"); row("NORMAL"); row("HARD"); row("BACK"); break;
    case MenuPage::Overwrite:
        view.title = "REPLACE SLOT " + std::to_string(s.slot + 1U) + "?";
        view.subtitle = "THE PREVIOUS SAVE WILL BE REPLACED";
        row("CANCEL"); row("START NEW GAME"); break;
    case MenuPage::Speedrun:
        view.title = "SPEEDRUN"; view.subtitle = "HARD ONLY - TIMER RUNS WHILE PAUSED";
        row("START RUN"); row("LEADERBOARD"); row("BEST SEED", !board.empty()); row("BACK"); break;
    case MenuPage::Leaderboard:
        view.title = "LOCAL LEADERBOARD"; view.subtitle = "TOP 10 - HARD - THIS COMPUTER";
        if (board.empty()) row("NO FINISHED RUNS YET", false);
        for (std::size_t i = 0; i < board.size() && i < 10U; ++i)
            row(std::to_string(i + 1U) + ".  " + visual::runTimeText(board[i].bestTicks), false);
        row("BACK"); break;
    case MenuPage::Settings:
        view.title = "SETTINGS"; view.subtitle = "CHANGES ARE SAVED AUTOMATICALLY";
        row("DISPLAY"); row("AUDIO"); row("CONTROLS");
        setting("LANGUAGE", SettingsMenuOption::Language,
            settings.language == gameplay::Language::PortugueseBrazil ? "PORTUGUÊS (BR)" : "ENGLISH");
        row("SEND FEEDBACK");
        row("BACK"); break;
    case MenuPage::FeedbackCategory:
        view.title = "SEND FEEDBACK"; view.subtitle = "CHOOSE A CATEGORY";
        row("BUG"); row("GAMEPLAY"); row("UPDATE IDEA"); row("BACK"); break;
    case MenuPage::FeedbackCompose: {
        view.title = "SEND FEEDBACK";
        view.subtitle = s.feedbackEditing ? "TYPE MESSAGE - ENTER TO FINISH"
            : s.feedbackSending ? "SENDING - PLEASE WAIT"
            : "ONLY YOUR MESSAGE AND SYSTEM INFO ARE SENT";
        row("WRITE MESSAGE  " + std::to_string(uiTextLength(s.feedbackDraft)) + "/"
            + std::to_string(maximumFeedbackCharacters), !s.feedbackSending);
        for (auto& line : feedbackPreview(s.feedbackDraft)) {
            row(line.empty() ? " " : line, false);
            view.rows.back().rawText = true;
        }
        row("SEND", !s.feedbackSending && validPlayerFeedback(s.feedbackDraft));
        row("BACK", !s.feedbackSending);
        break;
    }
    case MenuPage::Display: {
        view.title = "DISPLAY";
        setting("FIELD OF VIEW", SettingsMenuOption::FieldOfView, std::to_string(settings.fieldOfViewDegrees));
        setting("GRAPHICS", SettingsMenuOption::GraphicsQuality, std::string{gameplay::graphicsQualityName(settings.graphicsQuality)});
        const auto size = gameplay::windowResolutionSize(settings.windowResolution);
        setting("RESOLUTION", SettingsMenuOption::WindowResolution, std::to_string(size.width) + "X" + std::to_string(size.height));
        setting("FULLSCREEN", SettingsMenuOption::Fullscreen, settings.fullscreen ? "ON" : "OFF");
        setting("VSYNC", SettingsMenuOption::VerticalSync, settings.verticalSync ? "ON" : "OFF"); row("BACK"); break;
    }
    case MenuPage::Audio:
        view.title = "AUDIO";
        setting("MASTER", SettingsMenuOption::MasterVolume, std::to_string(settings.masterVolumePercent) + "%");
        setting("EFFECTS", SettingsMenuOption::EffectsVolume, std::to_string(settings.effectsVolumePercent) + "%");
        setting("AMBIENT", SettingsMenuOption::AmbientVolume, std::to_string(settings.ambientVolumePercent) + "%");
        setting("UI", SettingsMenuOption::UiVolume, std::to_string(settings.uiVolumePercent) + "%"); row("BACK"); break;
    case MenuPage::Controls:
        view.title = "CONTROLS"; view.subtitle = "CHOOSE INPUT DEVICE";
        row("KEYBOARD / MOUSE"); row("CONTROLLER"); row("BACK"); break;
    case MenuPage::KeyboardControls:
        view.title = "KEYBOARD / MOUSE"; view.subtitle = "ESC / F3 RESERVED - MENU KEYS STAY FIXED";
        setting("SENSITIVITY", SettingsMenuOption::MouseSensitivity, std::to_string(settings.mouseSensitivityPercent) + "%");
        setting("INVERT MOUSE Y", SettingsMenuOption::InvertMouseY, settings.invertMouseY ? "ON" : "OFF");
        row("REMAP KEYS / MOUSE"); row("BACK"); break;
    case MenuPage::GamepadControls:
        view.title = "CONTROLLER"; view.subtitle = "MOVE STICK ALSO CHANGES LOOK STICK";
        setting("MOVE STICK", SettingsMenuOption::GamepadMoveStick,
            settings.gamepadSouthpaw ? "RIGHT STICK" : "LEFT STICK");
        setting("LOOK HORIZONTAL", SettingsMenuOption::GamepadLookHorizontal,
            std::to_string(settings.gamepadLookHorizontalPercent) + "%");
        setting("LOOK VERTICAL", SettingsMenuOption::GamepadLookVertical,
            std::to_string(settings.gamepadLookVerticalPercent) + "%");
        setting("AIM SENSITIVITY", SettingsMenuOption::GamepadAimSensitivity,
            std::to_string(settings.gamepadAimSensitivityPercent) + "%");
        setting("MOVE DEADZONE", SettingsMenuOption::GamepadMoveDeadzone,
            std::to_string(settings.gamepadMoveDeadzonePercent) + "%");
        setting("LOOK DEADZONE", SettingsMenuOption::GamepadLookDeadzone,
            std::to_string(settings.gamepadLookDeadzonePercent) + "%");
        setting("INVERT LOOK Y", SettingsMenuOption::GamepadInvertY,
            settings.invertGamepadY ? "ON" : "OFF");
        setting("VIBRATION", SettingsMenuOption::GamepadVibration, settings.gamepadVibration ? "ON" : "OFF");
        setting("VIBRATION STRENGTH", SettingsMenuOption::GamepadVibrationStrength,
            std::to_string(settings.gamepadVibrationPercent) + "%");
        row("REMAP BUTTONS"); row("BACK"); break;
    case MenuPage::Bindings:
    case MenuPage::MoreBindings:
    case MenuPage::GamepadBindings:
    case MenuPage::MoreGamepadBindings: {
        const bool gamepad = s.page == MenuPage::GamepadBindings || s.page == MenuPage::MoreGamepadBindings;
        const bool firstPage = s.page == MenuPage::Bindings || s.page == MenuPage::GamepadBindings;
        view.title = gamepad ? (firstPage ? "PAD BINDINGS 1/2" : "PAD BINDINGS 2/2")
            : (firstPage ? "KEY BINDINGS 1/2" : "KEY BINDINGS 2/2");
        view.subtitle = s.capture
            ? (gamepad ? "PRESS BUTTON - OPTIONS / ESC CANCEL" : "KEY / MOUSE / WHEEL - ESC CANCEL")
            : (gamepad ? "MOVE STICK IN CONTROLLER - BUTTONS OPTIONAL" : "SELECT TO REMAP - DUPLICATES SWAP");
        const std::size_t first = firstPage ? 0U : 11U;
        for (std::size_t i = first; i < first + 11U; ++i) {
            auto action = static_cast<gameplay::ControlAction>(i);
            const std::string value = s.capture == action ? "..." : std::string{gamepad
                ? gameplay::gamepadBindingName(settings.gamepadControls.values[i])
                : gameplay::bindingName(settings.controls.values[i])};
            view.rows.push_back({std::string{gameplay::controlActionName(action)} + "  [" + value + "]", true, {}, action, {}, {}});
        }
        row(firstPage ? "NEXT PAGE" : "PREVIOUS PAGE"); row("RESET DEFAULTS"); row("BACK"); break;
    }
    }
    if (!s.notice.empty()) view.subtitle = s.notice;
    view.title = localize(settings.language, view.title);
    view.subtitle = localize(settings.language, view.subtitle);
    for (auto& item : view.rows) if (!item.rawText) item.text = localize(settings.language, item.text);
    return view;
}
void moveMenuSelection(MenuFlowState& s, const MenuView& view, const int direction) noexcept
{
    if (view.rows.empty() || direction == 0 || s.capture || s.feedbackEditing) return;
    const auto count = view.rows.size();
    for (std::size_t i = 0; i < count; ++i) {
        s.selection = direction > 0 ? (s.selection + 1U) % count : (s.selection + count - 1U) % count;
        if (view.rows[s.selection].enabled) return;
    }
}
MenuCommand backMenu(MenuFlowState& s) noexcept
{
    if (s.capture) { s.capture.reset(); return MenuCommand::None; }
    if (s.feedbackEditing) { s.feedbackEditing = false; return MenuCommand::None; }
    switch (s.page) {
    case MenuPage::Root: return MenuCommand::ExitGame;
    case MenuPage::CampaignSlots: case MenuPage::Speedrun: open(s, MenuPage::Root); break;
    case MenuPage::CampaignSlot: open(s, MenuPage::CampaignSlots); s.selection = s.slot; break;
    case MenuPage::Difficulty: case MenuPage::Overwrite: open(s, MenuPage::CampaignSlot); break;
    case MenuPage::Leaderboard: open(s, MenuPage::Speedrun); break;
    case MenuPage::Settings: open(s, MenuPage::Root); return MenuCommand::CloseSettings;
    case MenuPage::FeedbackCategory: open(s, s.feedbackFromSettings ? MenuPage::Settings : MenuPage::Root); break;
    case MenuPage::FeedbackCompose: open(s, MenuPage::FeedbackCategory); break;
    case MenuPage::Display: case MenuPage::Audio: case MenuPage::Controls: open(s, MenuPage::Settings); break;
    case MenuPage::KeyboardControls: case MenuPage::GamepadControls: open(s, MenuPage::Controls); break;
    case MenuPage::Bindings: case MenuPage::MoreBindings: open(s, MenuPage::KeyboardControls); break;
    case MenuPage::GamepadBindings: case MenuPage::MoreGamepadBindings: open(s, MenuPage::GamepadControls); break;
    }
    return MenuCommand::None;
}
MenuCommand activateMenu(MenuFlowState& s, const MenuView& view, const std::array<CampaignSlotInfo, 3>& slots)
{
    if (s.selection >= view.rows.size() || !view.rows[s.selection].enabled || s.capture || s.feedbackEditing) return MenuCommand::None;
    const auto selected = s.selection;
    const auto& row = view.rows[selected];
    if (row.binding) {
        s.capture = row.binding;
        s.captureGamepad = s.page == MenuPage::GamepadBindings || s.page == MenuPage::MoreGamepadBindings;
        return MenuCommand::None;
    }
    if (row.setting) return MenuCommand::None;
    if (s.page == MenuPage::Root) {
        if (row.destination == MenuPage::FeedbackCategory) s.feedbackFromSettings = false;
        if (row.destination) open(s, *row.destination);
        return row.command;
    } else if (s.page == MenuPage::FeedbackCategory && selected < 3U) {
        s.feedbackTopic = static_cast<PlayerFeedbackTopic>(selected);
        open(s, MenuPage::FeedbackCompose);
    } else if (s.page == MenuPage::FeedbackCompose && selected == 0U) {
        s.feedbackEditing = true;
        s.notice.clear();
    } else if (s.page == MenuPage::FeedbackCompose && selected == 9U) {
        return MenuCommand::None;
    } else if (s.page == MenuPage::CampaignSlots && selected < 3U) {
        s.slot = selected; open(s, MenuPage::CampaignSlot);
    } else if (s.page == MenuPage::CampaignSlot && selected == 0U) return MenuCommand::ContinueCampaign;
    else if (s.page == MenuPage::CampaignSlot && selected == 1U) { open(s, MenuPage::Difficulty); s.selection = 1U; }
    else if (s.page == MenuPage::Difficulty && selected < 3U) {
        s.difficulty = static_cast<gameplay::Difficulty>(selected);
        if (slots[s.slot].occupied) open(s, MenuPage::Overwrite); else return MenuCommand::NewCampaign;
    } else if (s.page == MenuPage::Overwrite && selected == 1U) return MenuCommand::NewCampaign;
    else if (s.page == MenuPage::Speedrun && selected == 0U) return MenuCommand::StartSpeedrun;
    else if (s.page == MenuPage::Speedrun && selected == 1U) { open(s, MenuPage::Leaderboard); }
    else if (s.page == MenuPage::Speedrun && selected == 2U) return MenuCommand::BestSeed;
    else if (s.page == MenuPage::Settings && selected < 3U) open(s, selected == 0U ? MenuPage::Display : selected == 1U ? MenuPage::Audio : MenuPage::Controls);
    else if (s.page == MenuPage::Settings && selected == 4U) {
        s.feedbackFromSettings = true; open(s, MenuPage::FeedbackCategory);
    }
    else if (s.page == MenuPage::Controls && selected < 2U)
        open(s, selected == 0U ? MenuPage::KeyboardControls : MenuPage::GamepadControls);
    else if (s.page == MenuPage::KeyboardControls && selected == 2U) open(s, MenuPage::Bindings);
    else if (s.page == MenuPage::GamepadControls && selected == 9U) open(s, MenuPage::GamepadBindings);
    else if ((s.page == MenuPage::Bindings || s.page == MenuPage::MoreBindings) && selected == 11U)
        open(s, s.page == MenuPage::Bindings ? MenuPage::MoreBindings : MenuPage::Bindings);
    else if ((s.page == MenuPage::GamepadBindings || s.page == MenuPage::MoreGamepadBindings) && selected == 11U)
        open(s, s.page == MenuPage::GamepadBindings ? MenuPage::MoreGamepadBindings : MenuPage::GamepadBindings);
    else if ((s.page == MenuPage::Bindings || s.page == MenuPage::MoreBindings
        || s.page == MenuPage::GamepadBindings || s.page == MenuPage::MoreGamepadBindings)
        && selected == 12U) return MenuCommand::None;
    else return backMenu(s);
    return MenuCommand::None;
}
} // namespace gdp::game
