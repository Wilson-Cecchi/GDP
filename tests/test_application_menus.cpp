#include "game/Application.hpp"
#include "game/Localization.hpp"
#include "persistence/CampaignCheckpointStore.hpp"
#include "persistence/SettingsStore.hpp"
#include "persistence/SpeedrunLeaderboardStore.hpp"
#include "platform/SdlInput.hpp"
#include "gameplay/Props.hpp"
#include "gameplay/Loot.hpp"
#include "gameplay/RoomTraversal.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>

namespace gdp::game {
struct ApplicationMenuTestAccess {
    static int run(const std::filesystem::path& directory) {
        using wkc::platform::Key;
        using wkc::platform::MouseButton;
        using wkc::platform::GamepadButton;
        int failures = 0;
        auto expect = [&](bool condition, const char* message) {
            if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
        };
        ApplicationConfig config;
        config.worldSeed = 12345U;
        config.enableGamepad = false;
        Application app{config};
        expect(app.initialize(), "graphical application boots before a world exists");
        if (!app.window_) return 1;
        app.running_ = true;
        auto release = [&] { app.input_ = wkc::platform::Input{}; };
        auto key = [&](Key value) {
            release(); app.input_.handleKey(value, true); app.processEvents(); app.render();
        };
        auto pad = [&](GamepadButton value) {
            release(); app.input_.handleGamepadButton(value, true);
            app.processEvents(); app.render();
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
            const auto translatedLabel = localize(app.runtimeSettings_.language, label);
            for (std::size_t i = 0; i < view.rows.size(); ++i) if (view.rows[i].text.starts_with(translatedLabel)) {
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
        pad(GamepadButton::DpadDown);
        expect(app.menuFlow_.selection == 1U,
            "gamepad D-pad navigates the title menu");
        pad(GamepadButton::South);
        expect(app.menuFlow_.page == MenuPage::CampaignSlots,
            "gamepad south face button confirms a menu choice");
        pad(GamepadButton::East);
        expect(app.menuFlow_.page == MenuPage::Root,
            "gamepad east face button returns to the previous menu");
        choose("SETTINGS"); choose("CONTROLS"); choose("KEYBOARD / MOUSE");
        choose("REMAP"); choose("FORWARD");
        expect(app.menuFlow_.capture == gameplay::ControlAction::Forward, "binding begins capture without assigning activation Enter");
        key(Key::I);
        expect(app.runtimeSettings_.controls.values[0] == gameplay::bindingForName("I"), "captured key is persisted as Forward");
        choose("CROUCH"); key(Key::Escape);
        expect(!app.menuFlow_.capture && app.menuFlow_.page == MenuPage::Bindings, "Esc cancels capture without leaving page");
        key(Key::Escape); key(Key::Escape);
        choose("CONTROLLER");
        expect(app.menuFlow_.page == MenuPage::GamepadControls,
            "controller settings are separate from keyboard and mouse");
        app.menuFlow_.selection = 0U; key(Key::Left);
        expect(!app.runtimeSettings_.gamepadVibration, "vibration can be disabled");
        key(Key::Right);
        expect(app.runtimeSettings_.gamepadVibration, "vibration can be enabled again");
        app.menuFlow_.selection = 1U; key(Key::Left);
        expect(app.runtimeSettings_.gamepadVibrationPercent == 40U,
            "controller vibration intensity adjusts in ten-percent steps");
        choose("REMAP BUTTONS"); choose("FIRE");
        pad(GamepadButton::West);
        expect(app.runtimeSettings_.gamepadControls.values[static_cast<std::size_t>(gameplay::ControlAction::Fire)]
                == GamepadButton::West
            && app.runtimeSettings_.gamepadControls.values[static_cast<std::size_t>(gameplay::ControlAction::Reload)]
                == GamepadButton::RightTrigger,
            "controller capture swaps duplicate buttons without affecting keyboard bindings");
        choose("JUMP"); pad(GamepadButton::Start);
        expect(!app.menuFlow_.capture && app.menuFlow_.page == MenuPage::GamepadBindings,
            "Options cancels gamepad capture without remapping pause");
        choose("RESET DEFAULTS");
        expect(app.runtimeSettings_.gamepadControls == gameplay::GamepadBindings{}
                && app.runtimeSettings_.controls.values[0] == gameplay::bindingForName("I"),
            "controller reset restores only controller bindings");
        choose("FIRE"); pad(GamepadButton::West);
        key(Key::Escape); key(Key::Escape); key(Key::Escape); choose("AUDIO");
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
        app.menuFlow_.selection = 3U; key(Key::Right);
        expect(app.runtimeSettings_.language == gameplay::Language::PortugueseBrazil
            && app.frontendMenuView().title == "CONFIGURAÇÕES", "keyboard switches language instantly without closing settings");
        key(Key::Left);
        expect(app.runtimeSettings_.language == gameplay::Language::English, "keyboard can return to English");
        const auto languageLayout = visual::makeListMenuLayout(width, height, app.frontendMenuView().rows.size());
        const auto languageRow = languageLayout.rows[3];
        click(languageRow.x + languageRow.width - 10 * languageLayout.scale, languageRow.y + languageRow.height / 2);
        expect(app.runtimeSettings_.language == gameplay::Language::PortugueseBrazil
            && !app.fatalError_, "mouse switches to Portuguese using the rendered language arrow");
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
            release();
            app.input_.handleGamepadSticks({0.75F, -0.65F}, {});
            app.input_.handleGamepadButton(GamepadButton::West, true);
            app.processEvents();
            expect(app.playerCommand_.moveForward > 0.0F
                    && app.playerCommand_.moveRight > 0.0F
                    && app.fireHeld_,
                "analog movement and remapped controller fire drive live gameplay");
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

        // Exercise real interaction/update/render rather than calling the session
        // victory method directly. The original slot remains isolated.
        const auto testExit = [&](const bool finalFloor) {
            const auto* terminal = app.entities_.find(app.exitTerminalEntity_);
            expect(terminal != nullptr, "campaign populates one Boss exit terminal");
            if (!terminal) return;
            const auto bossRoom = std::find_if(app.world_->rooms.begin(), app.world_->rooms.end(),
                [terminal](const auto& room) { return room.id == terminal->roomId; });
            expect(bossRoom != app.world_->rooms.end(), "exit console belongs to Boss room");
            if (bossRoom == app.world_->rooms.end()) return;
            const auto position = bossRoom->transform.position
                + wkc::core::rotateQuarterTurns({0.0F, 0.0F, 1.0F}, bossRoom->rotation);
            const auto direction = wkc::core::rotateQuarterTurns({0.0F, 0.0F, 1.0F}, bossRoom->rotation);
            constexpr float radiansToDegrees = 57.295779513F;
            app.player_.spawn(position, std::atan2(direction.z, direction.x) * radiansToDegrees);
            auto* playerEntity = app.entities_.find(app.player_.entity());
            playerEntity->roomId = bossRoom->id;
            playerEntity->transform.position = position;
            app.baseCollisionWorld_ = gameplay::makeRoomNavigationCollision(*app.world_, bossRoom->id);
            app.collisionWorld_ = gameplay::collisionWorldWithProps(app.baseCollisionWorld_, app.entities_, bossRoom->id);
            expect(app.isCampaignExitTargeted(), "rotated console is accessible at its visible surface");
            app.playerCommand_ = {.interact = true}; release(); app.fixedUpdate(1.0 / 60.0);
            expect(!app.session_.state().campaignExitReady && !app.restartRequested_
                && app.session_.state().phase == gameplay::SessionPhase::Playing,
                "terminal interaction before Boss death neither advances nor queues exit");
            for (const auto id : app.entities_.query(entities::EntityKind::Enemy)) {
                auto* enemy = app.entities_.findData<entities::EnemyData>(id);
                enemy->health = 0.0F; enemy->behavior = entities::EnemyBehaviorState::Dead;
            }
            app.playerCommand_ = {}; app.fixedUpdate(1.0 / 60.0); app.render();
            expect(!app.session_.state().campaignExitReady && !app.restartRequested_
                && !app.resultMenu_.active && app.session_.state().phase == gameplay::SessionPhase::Playing,
                "Boss death alone does not bypass the current floor mission");
            app.playerCommand_ = {.interact = true}; app.fixedUpdate(1.0 / 60.0);
            expect(!app.restartRequested_ && app.session_.state().phase == gameplay::SessionPhase::Playing,
                "real terminal interaction remains blocked while the mission is incomplete");
            if (app.campaignMission_.kind == gameplay::CampaignMissionKind::SecureTerminal) {
                expect(!gameplay::advanceCampaignMission(app.campaignMission_,
                        app.campaignMission_.targetRoomId, {.terminalActivated = true}),
                    "terminal mission activation alone does not release the Boss exit");
            }
            expect(gameplay::advanceCampaignMission(app.campaignMission_,
                    app.campaignMission_.targetRoomId, {
                        .keysCollected = app.campaignMission_.kind
                            == gameplay::CampaignMissionKind::RecoverKey ? 1U : 0U,
                        .arenaCleared = app.campaignMission_.kind
                            == gameplay::CampaignMissionKind::SecureArena,
                        .terminalAmbushCleared = app.campaignMission_.kind
                            == gameplay::CampaignMissionKind::SecureTerminal,
                    }),
                "fixture completes the seeded mission at its assigned room");
            app.playerCommand_ = {}; app.fixedUpdate(1.0 / 60.0); app.render();
            expect(app.session_.state().campaignExitReady && !app.restartRequested_,
                "mission completion after Boss death unlocks the return trip to the console");
            expect(app.saveCurrentCampaignCheckpoint(), "unlocked exit keeps the recoverable floor-entry save");
            const auto storedBefore = app.campaignSlots_[app.activeCampaignSlot_].checkpoint;
            expect(storedBefore.has_value(), "campaign checkpoint survives Boss death until terminal use");
            const auto blocker = app.entities_.createProp({
                .transform = {.position = position + direction * 0.4F, .scale = {0.5F, 2.0F, 0.5F}},
                .roomId = bossRoom->id, .type = entities::PropType::Barricade,
                .solid = true, .health = 60.0F, .maximumHealth = 60.0F});
            app.collisionWorld_ = gameplay::collisionWorldWithProps(app.baseCollisionWorld_, app.entities_, bossRoom->id);
            expect(!app.isCampaignExitTargeted(), "solid cover blocks exit activation through objects");
            (void)app.entities_.destroy(blocker); (void)app.entities_.flushDestroyed();
            app.player_.spawn(position - direction * 4.0F, std::atan2(direction.z, direction.x) * radiansToDegrees);
            expect(!app.isCampaignExitTargeted(), "exit cannot be activated remotely");
            app.player_.spawn(position, std::atan2(direction.z, direction.x) * radiansToDegrees);
            app.player_.applyDamage(17.0F);
            const auto carryover = gameplay::captureCampaignCarryover(app.player_, app.weapon_, app.progression_);
            app.playerCommand_ = {.interact = true}; app.fixedUpdate(1.0 / 60.0); app.render();
            expect(app.session_.state().phase == gameplay::SessionPhase::Victory && !app.fatalError_,
                "validated terminal use completes the floor");
            if (finalFloor) {
                expect(app.resultMenu_.active && !app.restartRequested_
                    && !app.campaignSlots_[app.activeCampaignSlot_].checkpoint,
                    "final terminal displays campaign victory and clears only its slot");
            } else {
                expect(app.restartRequested_ && !app.resultMenu_.active
                    && app.restartMode_ == gameplay::RunRestartMode::NextCampaignFloor
                    && app.campaignCarryover_ == carryover,
                    "intermediate terminal requests next floor with exact resources, without results menu");
                const auto restartMode = app.restartMode_;
                app.shutdown();
                const auto nextSeed = app.session_.restart(restartMode);
                expect(nextSeed.has_value(), "terminal advances deterministic floor sequence");
                if (nextSeed) app.config_.worldSeed = *nextSeed;
                app.restartRequested_ = false; app.running_ = true;
                expect(app.initialize() && app.initializeRun(), "next floor is generated through the normal gameplay initialization");
                expect(app.session_.state().campaignFloor == 2U && !app.session_.state().campaignExitReady
                    && gameplay::captureCampaignCarryover(app.player_, app.weapon_, app.progression_) == carryover
                    && app.campaignSlots_[app.activeCampaignSlot_].checkpoint->floor == 2U,
                    "next floor relocks its console, restores resources and persists floor two");
            }
        };
        testExit(false);
        (void)app.returnToMainMenu(); choose("CAMPAIGN"); choose("SLOT 3"); choose("CONTINUE");
        // Keep the fixture's existing seed/checkpoint format and move only its floor.
        app.shutdown();
        app.session_ = gameplay::SessionSystem{12345U};
        app.config_.worldSeed = 12345U;
        expect(app.session_.restoreCampaignCheckpoint(12345U, 12345U, 0U, 0U,
            gameplay::campaignFloorCount, gameplay::Difficulty::Hard), "final-floor fixture restores a valid campaign session");
        expect(app.initialize() && app.initializeRun(), "final campaign floor initializes normally");
        app.running_ = true;
        testExit(true);
        // Recreate the cleared fixture slot for the original persistence checks below.
        (void)app.returnToMainMenu(); choose("CAMPAIGN"); choose("SLOT 3"); choose("NEW GAME"); choose("HARD");
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
        for (const auto type : {gameplay::WeaponArchetype::Sniper, gameplay::WeaponArchetype::Shotgun, gameplay::WeaponArchetype::SubmachineGun}) {
            const auto items = app.entities_.query(entities::EntityKind::Item);
            const auto item = std::find_if(items.begin(), items.end(), [&](const auto id) {
                return app.entities_.findData<entities::ItemData>(id)->spawnedWeaponArchetype
                    == static_cast<std::uint32_t>(type);
            });
            expect(item != items.end(), "arena exposes a pickup for each new primary");
            if (item == items.end()) continue;
            app.weapon_ = gameplay::Weapon{};
            const auto* pickup = app.entities_.find(*item);
            app.player_.spawn(pickup->transform.position);
            const auto collected = gameplay::collectNearbyLoot(app.entities_, app.player_, app.weapon_, 1U);
            expect(collected.weaponsEquipped == 1U && app.weapon_.archetypeForSlot(1U) == type,
                "arena pickup equips new primary beside pistol");
            app.weapon_.fixedUpdate(1.0F);
            key(Key::Digit3); release(); app.fixedUpdate(1.0 / 60.0);
            key(Key::Digit1); release(); app.fixedUpdate(1.0 / 60.0); app.render();
            expect(app.weapon_.definition().archetype == type && !app.fatalError_,
                "live primary key and rendering support each new firearm after selecting Knife");
            // Live reload state feeds the new hands, without a second visual clock.
            app.weapon_.fixedUpdate(1.0F);
            expect(app.weapon_.tryFire()==gameplay::FireReadiness::Fired, "primary can fire before hands reload fixture");
            app.reloadPending_=true; release(); app.fixedUpdate(1.0/60.0); app.render();
            expect(app.weapon_.reloading() && !app.fatalError_,
                "live application renders hands while the authoritative weapon is reloading");
            const float progressBeforePause=app.weapon_.reloadProgress();
            const auto magsBeforePause=app.weapon_.reserveAmmo();
            app.setPaused(true); app.fixedUpdate(.20); app.render();
            expect(app.weapon_.reloadProgress()==progressBeforePause
                && app.weapon_.reserveAmmo()==magsBeforePause && !app.fatalError_,
                "pause/render cannot advance a hand reload or spend ammunition");
            app.setPaused(false);
            release();
            for(unsigned int tick=0;tick<160U;++tick) app.fixedUpdate(1.0/60.0);
            app.render();
            expect(!app.weapon_.reloading() && !app.fatalError_,
                "completed magazine or per-shell reload returns the live hands to rest");
            if (type == gameplay::WeaponArchetype::Sniper) {
                // Isolate presentation transitions from combat and physical input.
                for (const auto id : app.entities_.query(entities::EntityKind::Enemy)) {
                    auto* enemy = app.entities_.findData<entities::EnemyData>(id);
                    enemy->health = 0.0F;
                    enemy->behavior = entities::EnemyBehaviorState::Dead;
                }
                app.weapon_.fixedUpdate(1.0F);
                app.weaponEquipRemaining_ = 0.0F;
                release(); app.input_.handleMouseButton(MouseButton::Secondary, true, {});
                for (int tick = 0; tick < 22; ++tick) app.fixedUpdate(1.0 / 60.0);
                expect(app.aimBlend_ > 0.999F, "Mouse2 fully aims the live sniper");
                app.render();
                release(); app.fixedUpdate(0.175);
                expect(std::abs(app.aimBlend_ - 0.5F) < 0.001F,
                    "Mouse2 release preserves the existing smooth zoom-out");
                const auto aimIndex = static_cast<std::size_t>(gameplay::ControlAction::Aim);
                const auto originalAim = app.runtimeSettings_.controls.values[aimIndex];
                app.runtimeSettings_.controls.values[aimIndex] = gameplay::bindingForName("T").value();
                release(); app.input_.handleKey(Key::T, true); app.fixedUpdate(0.175);
                expect(app.aimBlend_ > 0.999F, "aim uses remapped action, not hard-coded Mouse2");
                app.runtimeSettings_.controls.values[aimIndex] = originalAim;
                release(); app.input_.handleMouseButton(MouseButton::Secondary, true, {});
                app.reloadPending_ = true; app.fixedUpdate(1.0 / 60.0);
                expect(!app.weapon_.reloading() && app.aimBlend_ > 0.999F,
                    "rejected reload with a full magazine does not blink the scope");
                (void)app.weapon_.tryFire();
                app.reloadPending_ = true; app.fixedUpdate(1.0 / 60.0);
                expect(app.weapon_.reloading() && app.aimBlend_ == 0.0F,
                    "starting reload clears scope immediately");
                app.weapon_.fixedUpdate(3.0F);
                app.aimBlend_ = 1.0F;
                app.setPaused(true);
                expect(app.aimBlend_ == 0.0F, "opening pause clears sniper zoom");
                app.setPaused(false);
                app.aimBlend_ = 1.0F;
                app.weaponSwitchPending_ = gameplay::WeaponArchetype::Pistol;
                release(); app.fixedUpdate(1.0 / 60.0);
                expect(app.aimBlend_ == 0.0F
                    && app.weapon_.definition().archetype == gameplay::WeaponArchetype::Pistol,
                    "switching away cannot carry scope to the pistol");
                (void)app.weapon_.switchTo(type); app.weapon_.fixedUpdate(1.0F);
                app.aimBlend_ = 1.0F; app.dropPending_ = true;
                app.fixedUpdate(1.0 / 60.0);
                expect(app.aimBlend_ == 0.0F && !app.weapon_.ownsWeapon(type),
                    "dropping sniper removes its optical state");
            }
        }
        release(); app.fixedUpdate(1.0 / 60.0); app.render();
        expect(!app.fatalError_, "arena supports simulation and rendering");
        // Isolate actual locomotion in an unobstructed part of the test arena.
        app.player_.spawn({0,0,3}); app.playerBodyMotion_ = {};
        release(); app.input_.handleKey(Key::I,true);
        app.processEvents();
        const auto bodyStart = app.player_.state().position;
        for (int tick = 0; tick < 8; ++tick) app.fixedUpdate(1.0/60.0);
        expect(app.player_.state().position != bodyStart && app.playerBodyMotion_.phaseRadians > 0,
            "live application advances body gait after actual collision-resolved movement");
        const auto bodyMoving = app.playerBodyMotion_;
        app.setPaused(true);
        app.fixedUpdate(1.0/60.0); app.render();
        expect(app.playerBodyMotion_ == bodyMoving,"pause/render sampling freeze the body pose");
        app.setPaused(false); release();
        app.processEvents();
        for (int tick = 0; tick < 40; ++tick) app.fixedUpdate(1.0/60.0);
        expect(app.playerBodyMotion_.weight == 0,"stopping in live game blends legs to rest");
        (void)app.returnToMainMenu();
        expect(app.playerBodyMotion_ == visual::PlayerBodyMotion{},"return/restart clears lower-body pose history");
        app.shutdown();
        for (unsigned slot = 1U; slot <= 3U; ++slot) {
            auto cp = persistence::loadCampaignCheckpoint(directory / "WKC" / "GDP" / ("campaign_slot_" + std::to_string(slot) + ".toml"));
            expect(cp.checkpoint.has_value(), "independent slot remains valid after all modes");
        }
        config.worldSeed = 999U;
        Application reopened{config};
        expect(reopened.initialize(), "second process session boots");
        expect(reopened.runtimeSettings_.language == gameplay::Language::PortugueseBrazil
            && reopened.frontendMenuView().rows[0].text == "MODO DE TESTE", "selected language persists into a subsequent launch");
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
