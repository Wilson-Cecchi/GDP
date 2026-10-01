#include "game/Application.hpp"
#include "persistence/CampaignCheckpointStore.hpp"
#include "persistence/SettingsStore.hpp"
#include "persistence/SpeedrunLeaderboardStore.hpp"
#include "platform/SdlInput.hpp"
#include <SDL3/SDL.h>
#include <chrono>
#include <iostream>

namespace gdp::game {
struct ApplicationMenuTestAccess {
    static int run(const std::filesystem::path& directory) {
        using wkc::platform::Key;
        using wkc::platform::MouseButton;
        int failures = 0;
        auto expect = [&](bool condition, const char* message) {
            if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
        };
        ApplicationConfig config;
        config.worldSeed = 12345U;
        Application app{config};
        expect(app.initialize(), "graphical application boots before a world exists");
        if (!app.window_) return 1;
        app.running_ = true;
        auto release = [&] { app.input_ = wkc::platform::Input{}; };
        auto key = [&](Key value) {
            release(); app.input_.handleKey(value, true); app.processEvents(); app.render();
        };
        auto choose = [&](std::string_view label) {
            if (app.pauseMenu_.paused && !app.settingsMenu_.active) {
                if (label == "SETTINGS") app.pauseMenu_.selection = gameplay::PauseMenuOption::Settings;
                else if (label == "CONTROLS") app.pauseMenu_.selection = gameplay::PauseMenuOption::Controls;
                else if (label == "RESUME") app.pauseMenu_.selection = gameplay::PauseMenuOption::Resume;
                else { expect(false, "requested pause row exists"); return; }
                key(Key::Enter); return;
            }
            auto view = app.frontendMenuView();
            for (std::size_t i = 0; i < view.rows.size(); ++i) if (view.rows[i].text.starts_with(label)) {
                app.menuFlow_.selection = i; key(Key::Enter); return;
            }
            expect(false, "requested row exists");
        };
        expect(app.mainMenu_.active && !app.world_, "menu defers world generation");
        const auto titleSeed = app.session_.state().currentSeed;
        const auto titleTicks = app.session_.state().elapsedRunTicks;
        for (unsigned tick = 0U; tick < 60U; ++tick) app.fixedUpdate(1.0 / 60.0);
        app.render();
        expect(app.menuBackdropSeconds_ > 0.99 && app.menuBackdropSeconds_ < 1.01,
            "title animates its independent presentation clock");
        expect(!app.world_ && app.entities_.query(entities::EntityKind::Enemy).empty()
            && app.session_.state().currentSeed == titleSeed
            && app.session_.state().elapsedRunTicks == titleTicks && app.fixedUpdates_ == 0U
            && !app.fatalError_, "animated title cannot advance the session or spawn gameplay entities");
        choose("SETTINGS"); choose("CONTROLS"); choose("REMAP"); choose("FORWARD");
        expect(app.menuFlow_.capture == gameplay::ControlAction::Forward, "binding begins capture without assigning activation Enter");
        key(Key::I);
        expect(app.runtimeSettings_.controls.values[0] == gameplay::bindingForName("I"), "captured key is persisted as Forward");
        choose("CROUCH"); key(Key::Escape);
        expect(!app.menuFlow_.capture && app.menuFlow_.page == MenuPage::Bindings, "Esc cancels capture without leaving page");
        key(Key::Escape); key(Key::Escape); choose("AUDIO");
        auto view = app.frontendMenuView();
        int width = 0, height = 0, ww = 0, wh = 0;
        (void)SDL_GetWindowSizeInPixels(app.window_, &width, &height);
        (void)SDL_GetWindowSize(app.window_, &ww, &wh);
        const auto layout = visual::makeListMenuLayout(width, height, view.rows.size());
        auto click = [&](int x, int y) {
            release(); const wkc::core::Vec2 pointer{static_cast<float>(x) * static_cast<float>(ww) / static_cast<float>(width),
                static_cast<float>(height - y) * static_cast<float>(wh) / static_cast<float>(height)};
            app.input_.handleMouseButton(MouseButton::Primary, true, pointer); app.processEvents(); app.render();
        };
        auto r = layout.rows[0];
        click(r.x + r.width - 30 * layout.scale, r.y + r.height / 2);
        expect(app.runtimeSettings_.masterVolumePercent == 90U, "mouse left arrow reduces existing configuration");
        click(r.x + r.width / 2, r.y + r.height / 2);
        expect(app.runtimeSettings_.masterVolumePercent == 90U, "mouse label does not change setting");
        click(r.x + r.width - 10 * layout.scale, r.y + r.height / 2);
        expect(app.runtimeSettings_.masterVolumePercent == 100U, "mouse right arrow increases existing configuration");
        key(Key::Escape); key(Key::Escape);
        expect(!app.settingsMenu_.active && app.mainMenu_.active, "settings return to title");
        const auto root = visual::makeListMenuLayout(width, height, app.frontendMenuView().rows.size(), true);
        const auto settingsRow = root.rows[3];
        click(settingsRow.x + settingsRow.width + 1, settingsRow.y + settingsRow.height / 2);
        expect(app.menuFlow_.page == MenuPage::Root, "click outside compact title does not launch an option");
        click(settingsRow.x + settingsRow.width / 2, settingsRow.y + settingsRow.height / 2);
        expect(app.settingsMenu_.active && app.menuFlow_.page == MenuPage::Settings,
            "compact title mouse target opens the displayed Settings row");
        const auto menuTime = app.menuBackdropSeconds_;
        release(); app.fixedUpdate(1.0 / 60.0);
        expect(app.menuBackdropSeconds_ > menuTime && !app.world_,
            "settings from title retain the animated background without starting gameplay");
        key(Key::Escape);

        for (std::size_t slot = 0U; slot < 3U; ++slot) {
            choose("CAMPAIGN"); choose("SLOT " + std::to_string(slot + 1U)); choose("NEW GAME");
            choose(slot == 0U ? "EASY" : slot == 1U ? "NORMAL" : "HARD");
            expect(app.session_.state().mode == gameplay::RunMode::Campaign
                && app.session_.state().difficulty == static_cast<gameplay::Difficulty>(slot)
                && app.activeCampaignSlot_ == slot, "campaign launches selected slot and difficulty");
            expect(app.campaignSlots_[slot].checkpoint && app.campaignSlots_[slot].checkpoint->difficulty == static_cast<gameplay::Difficulty>(slot), "selected checkpoint saved to independent file");
            release(); app.input_.handleKey(Key::I, true); app.processEvents();
            expect(app.playerCommand_.moveForward == 1.0F, "custom Forward key drives live gameplay");
            release(); app.input_.handleKey(Key::W, true); app.processEvents();
            expect(app.playerCommand_.moveForward == 0.0F, "old W binding no longer moves player");
            key(Key::Escape); choose("SETTINGS"); key(Key::Escape);
            const auto pausedTime = app.menuBackdropSeconds_;
            release(); app.fixedUpdate(1.0 / 60.0);
            expect(app.menuBackdropSeconds_ == pausedTime, "pause freezes the demonstration clock as well as gameplay");
            expect(app.pauseMenu_.paused && !app.settingsMenu_.active && app.activeCampaignSlot_ == slot,
                "pause settings return preserves active campaign slot");
            expect(app.saveCurrentCampaignCheckpoint(), "settings cannot redirect subsequent checkpoint to another slot");
            expect(app.returnToMainMenu(), "campaign returns to title without deleting checkpoint");
        }
        choose("CAMPAIGN"); choose("SLOT 3"); choose("NEW GAME"); choose("EASY");
        expect(app.menuFlow_.page == MenuPage::Overwrite && app.menuFlow_.selection == 0U, "occupied slot requires safe overwrite confirmation");
        key(Key::Escape); choose("CONTINUE");
        expect(app.session_.state().difficulty == gameplay::Difficulty::Hard && app.activeCampaignSlot_ == 2U, "cancelled overwrite preserves Hard save and continue restores it");
        (void)app.returnToMainMenu(); choose("SPEEDRUN"); choose("START RUN");
        expect(app.session_.state().mode == gameplay::RunMode::Speedrun && app.session_.state().difficulty == gameplay::Difficulty::Hard && app.runDifficulty_ == 1.25F,
            "Speedrun generates and populates world at Hard");
        for (unsigned tick = 0U; tick < 120U; ++tick) app.session_.advanceRunTimer();
        (void)app.session_.update(true, world::RoomRole::Boss, true, true);
        for (const auto id : app.entities_.query(entities::EntityKind::Enemy)) {
            auto* enemy = app.entities_.findData<entities::EnemyData>(id);
            enemy->health = 0.0F; enemy->behavior = entities::EnemyBehaviorState::Dead;
        }
        release(); app.fixedUpdate(1.0 / 60.0);
        expect(app.session_.state().phase == gameplay::SessionPhase::Victory && app.speedrunLeaderboard_.size() == 1U,
            "Speedrun victory enters persistent leaderboard");
        (void)app.returnToMainMenu(); choose("TEST MODE");
        expect(app.session_.state().mode == gameplay::RunMode::TestMode && app.world_ && app.world_->rooms.size() == 1U
            && app.entities_.query(entities::EntityKind::Enemy).size() == 5U, "Test Mode creates dedicated arena with all enemy types");
        expect(app.weapon_.ownsWeapon(gameplay::WeaponArchetype::Carbine), "arena starts with firearms and knife available");
        release(); app.fixedUpdate(1.0 / 60.0); app.render();
        expect(!app.fatalError_, "arena supports simulation and rendering");
        (void)app.returnToMainMenu();
        app.shutdown();
        for (unsigned slot = 1U; slot <= 3U; ++slot) {
            auto cp = persistence::loadCampaignCheckpoint(directory / "WKC" / "GDP" / ("campaign_slot_" + std::to_string(slot) + ".toml"));
            expect(cp.checkpoint.has_value(), "independent slot remains valid after all modes");
        }
        config.worldSeed = 999U;
        Application reopened{config};
        expect(reopened.initialize(), "second process session boots");
        expect(reopened.campaignSlots_[2].checkpoint && reopened.campaignSlots_[2].checkpoint->difficulty == gameplay::Difficulty::Hard,
            "slots reload on subsequent launch");
        expect(reopened.runtimeSettings_.controls.values[0] == gameplay::bindingForName("I"), "remapped controls reload on subsequent launch");
        expect(reopened.speedrunLeaderboard_.size() == 1U, "leaderboard reloads on subsequent launch");
        reopened.shutdown();
        const auto pref = directory / "WKC" / "GDP";
        const auto slotOne = pref / "campaign_slot_1.toml";
        auto old = persistence::loadCampaignCheckpoint(slotOne).checkpoint;
        if (old) {
            expect(persistence::saveCampaignCheckpoint(pref / "campaign_checkpoint.toml", *old), "legacy fixture created");
            std::filesystem::remove(slotOne);
            Application migrated{config};
            expect(migrated.initialize() && migrated.campaignSlots_[0].checkpoint == old
                && std::filesystem::exists(pref / "campaign_checkpoint.legacy.toml"),
                "single legacy save imports into slot 1 with recoverable backup");
            expect(migrated.clearPersistedCampaignCheckpoint(), "completed imported slot clears only its checkpoint");
            migrated.shutdown();
            Application afterCompletion{config};
            expect(afterCompletion.initialize() && !afterCompletion.campaignSlots_[0].checkpoint,
                "legacy backup cannot resurrect a completed campaign");
            afterCompletion.shutdown();
        } else expect(false, "slot 1 exists for legacy import fixture");
        return failures == 0 ? 0 : 1;
    }
};
}
int main() {
    const auto directory = std::filesystem::temp_directory_path() / ("gdp_menu_integration_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    (void)SDL_setenv_unsafe("XDG_DATA_HOME", directory.string().c_str(), 1);
    const int result = gdp::game::ApplicationMenuTestAccess::run(directory);
    std::error_code error; std::filesystem::remove_all(directory, error);
    return result;
}
