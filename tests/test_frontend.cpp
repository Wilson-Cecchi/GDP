#include "game/MenuFlow.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "game/visual/UiFontAtlas.hpp"
#include "persistence/CampaignCheckpointStore.hpp"
#include "persistence/SettingsStore.hpp"
#include "persistence/SpeedrunLeaderboardStore.hpp"
#include "world/LootGenerator.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>

namespace {
int failures = 0;
void expect(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void menuTests() {
    using namespace gdp::game;
    using namespace gdp::gameplay;
    MenuFlowState state;
    std::array<CampaignSlotInfo, 3> slots{};
    RuntimeSettings settings;
    auto view = [&] { return makeMenuView(state, slots, {}, settings, true); };
    expect(view().rows.size() == 5U && view().rows[0].text == "TEST MODE", "development root has five requested entries");
    expect(makeMenuView(state, slots, {}, settings, false).rows.size() == 4U, "shipping root omits Test Mode");
    state.selection = 1U; (void)activateMenu(state, view(), slots);
    expect(state.page == MenuPage::CampaignSlots && view().rows.size() == 4U, "campaign opens three independent slots");
    state.selection = 2U; (void)activateMenu(state, view(), slots);
    expect(state.slot == 2U && !view().rows[0].enabled, "empty third slot cannot continue");
    state.selection = 1U; (void)activateMenu(state, view(), slots);
    expect(state.page == MenuPage::Difficulty && state.selection == 1U, "new save selects base difficulty with Normal default");
    state.selection = 2U;
    expect(activateMenu(state, view(), slots) == MenuCommand::NewCampaign && state.difficulty == Difficulty::Hard,
        "empty slot launches chosen Hard campaign");
    slots[2].occupied = true;
    expect(activateMenu(state, view(), slots) == MenuCommand::None && state.page == MenuPage::Overwrite && state.selection == 0U,
        "occupied or unreadable save requires confirmation with Cancel selected");
    (void)activateMenu(state, view(), slots);
    expect(state.page == MenuPage::CampaignSlot, "cancel overwrite returns without changing saved data");
    state.page = MenuPage::Leaderboard; state.selection = 0U;
    moveMenuSelection(state, view(), 1);
    expect(state.selection == 1U && view().rows[1].enabled, "leaderboard navigation skips inactive rows");
    state.page = MenuPage::Settings; state.selection = 2U;
    (void)activateMenu(state, view(), slots);
    expect(state.page == MenuPage::Controls && view().rows[2].text == "REMAP KEYS / MOUSE", "settings are categorized");
    state.selection = 2U; (void)activateMenu(state, view(), slots);
    expect(view().rows.size() == 14U, "bindings fit two short pages rather than an overflowing list");
    state.selection = 0U; (void)activateMenu(state, view(), slots);
    expect(state.capture == ControlAction::Forward, "binding row starts input capture");
    (void)backMenu(state);
    expect(!state.capture && state.page == MenuPage::Bindings, "Esc cancels capture before leaving page");
    (void)backMenu(state); (void)backMenu(state);
    expect(backMenu(state) == MenuCommand::CloseSettings, "back from categories returns to settings origin");
}
void bindingsTests() {
    using namespace gdp::gameplay;
    using wkc::platform::Key;
    ControlBindings bindings;
    expect(validateControlBindings(bindings), "default bindings are valid and unique");
    for (auto binding : bindings.values) expect(bindingForName(bindingName(binding)) == binding, "binding names round trip");
    expect(!bindingForName("ESC") && !bindingForName("F3") && !bindingForName("NOT A KEY"), "reserved and unknown bindings rejected");
    auto oldForward = bindings.values[0];
    expect(assignControlBinding(bindings, ControlAction::Forward, bindings.values[1]) && bindings.values[1] == oldForward,
        "duplicate assignment swaps instead of disabling an action");
    expect(assignControlBinding(bindings, ControlAction::Crouch, {BindingKind::Key, static_cast<std::uint8_t>(Key::C)}), "C is supported as an optional custom key");
    wkc::platform::Input input;
    input.handleKey(Key::C, true);
    expect(controlDown(bindings, ControlAction::Crouch, input) && controlPressed(bindings, ControlAction::Crouch, input), "remapped key controls gameplay");
    expect(!controlDown(bindings, ControlAction::Forward, input), "unrelated key does not activate another action");
    input.beginFrame(); input.handleMouseWheel(-1.0F);
    expect(controlPressed(bindings, ControlAction::PreviousWeapon, input), "scroll bindings keep weapon cycling");
    input.beginFrame(); input.handleKey(Key::RightShift, true);
    expect(controlDown(bindings, ControlAction::QuietWalk, input), "right Shift remains an alias");
}
void layoutTests() {
    using namespace gdp::game::visual;
    expect(makeListMenuLayout(320, 50, 14U).rows.empty(), "impossibly small viewports do not create offscreen hit targets");
    std::size_t decoded = 0U;
    bool antialias = false;
    for (std::size_t i = 0U; i < uiFontAtlasRle.size(); i += 2U) {
        decoded += uiFontAtlasRle[i];
        antialias |= uiFontAtlasRle[i + 1U] > 0U && uiFontAtlasRle[i + 1U] < 255U;
    }
    expect(decoded == static_cast<std::size_t>(uiFontAtlasWidth * uiFontAtlasHeight)
        && antialias, "embedded font decodes to a complete antialiased atlas");
    for (auto dimensions : {std::array{1280, 720}, std::array{960, 540}, std::array{960, 600},
             std::array{2560, 1080}, std::array{640, 360}, std::array{320, 200}}) {
        const auto tactical = makeTacticalHudLayout(dimensions[0], dimensions[1]);
        expect(tactical.health.x + tactical.health.width < tactical.weapon.x,
            "minimal HUD keeps health left, ammo right, and the center open");
        for (const auto rect : {tactical.health, tactical.weapon}) {
            expect(rect.x >= 0 && rect.y >= 0 && rect.x + rect.width <= dimensions[0]
                && rect.y + rect.height <= dimensions[1], "tactical HUD corner groups remain in viewport");
        }
        for (std::size_t count = 2U; count <= 14U; ++count) {
            auto layout = makeListMenuLayout(dimensions[0], dimensions[1], count);
            expect(layout.rows.size() == count, "every menu row has a shared render/hit rectangle");
            for (std::size_t i = 0U; i < count; ++i) {
                const auto r = layout.rows[i];
                expect(r.x >= 0 && r.y >= 0 && r.x + r.width <= dimensions[0] && r.y + r.height <= dimensions[1], "rows remain inside viewport");
                if (i > 0U) expect(r.y + r.height < layout.rows[i - 1U].y, "refined menu hit targets do not overlap");
                float x = static_cast<float>(r.x + r.width / 2);
                float y = static_cast<float>(dimensions[1] - r.y - r.height / 2);
                expect(listMenuSelectionAt(dimensions[0], dimensions[1], count, x, y) == i, "mouse finds exactly the rendered row");
                expect(!listMenuAdjustmentDirectionAt(dimensions[0], dimensions[1], count, x, y), "clicking label only selects");
                x = static_cast<float>(r.x + r.width - 30 * layout.scale);
                expect(listMenuAdjustmentDirectionAt(dimensions[0], dimensions[1], count, x, y) == -1, "left arrow reduces setting");
                x = static_cast<float>(r.x + r.width - 10 * layout.scale);
                expect(listMenuAdjustmentDirectionAt(dimensions[0], dimensions[1], count, x, y) == 1, "right arrow increases setting");
            }
        }
    }
}
void persistenceTests(const std::filesystem::path& directory) {
    using namespace gdp::gameplay;
    using namespace gdp::persistence;
    RuntimeSettings settings;
    (void)assignControlBinding(settings.controls, ControlAction::Jump, *bindingForName("MOUSE4"));
    auto settingsPath = directory / "settings.toml";
    expect(saveRuntimeSettings(settingsPath, settings) && loadRuntimeSettings(settingsPath).settings == settings, "custom bindings persist");
    settings.mouseSensitivityPercent = 150U;
    expect(saveRuntimeSettings(settingsPath, settings) && loadRuntimeSettings(settingsPath).settings == settings, "repeated settings save replaces safely");
    {
        std::ofstream file(directory / "legacy_settings.toml");
        file << "version = 6\nmouse_sensitivity = 150\ngraphics_quality = LOW\n";
    }
    const auto legacy = loadRuntimeSettings(directory / "legacy_settings.toml");
    expect(legacy.status == SettingsLoadStatus::Migrated && legacy.settings.mouseSensitivityPercent == 150U
        && legacy.settings.graphicsQuality == GraphicsQuality::Low && legacy.settings.controls == ControlBindings{},
        "v6 settings retain graphics and sensitivity while receiving default bindings");
    {
        std::ofstream file(directory / "invalid_bindings.toml");
        file << "version = 7\nbinding_0 = S\nbinding_1 = S\n";
    }
    const auto invalid = loadRuntimeSettings(directory / "invalid_bindings.toml");
    expect(invalid.status == SettingsLoadStatus::LoadedWithRecovery && invalid.settings.controls == ControlBindings{},
        "duplicate persisted bindings recover safely to usable defaults");
    Player player; Weapon weapon; ProgressionSystem progress;
    for (unsigned i = 0U; i < 3U; ++i) {
        CampaignCheckpoint cp;
        cp.initialSeed = cp.currentSeed = 99U + i;
        cp.floorEntry = captureCampaignCarryover(player, weapon, progress);
        cp.difficulty = static_cast<Difficulty>(i);
        auto path = directory / ("slot_" + std::to_string(i) + ".toml");
        expect(saveCampaignCheckpoint(path, cp) && loadCampaignCheckpoint(path).checkpoint == cp, "each slot retains its seed and base difficulty");
    }
    SpeedrunLeaderboard board;
    SessionState result; result.mode = RunMode::Speedrun; result.phase = SessionPhase::Victory; result.difficulty = Difficulty::Hard;
    for (unsigned i = 1U; i <= 15U; ++i) { result.elapsedRunTicks = i * 60U; result.currentSeed = i; (void)addSpeedrunLeaderboardResult(board, result); }
    expect(board.size() == 10U && board.front().bestTicks == 60U && board.back().bestTicks == 600U, "local top ten keeps only fastest runs");
    result.difficulty = Difficulty::Easy;
    expect(!addSpeedrunLeaderboardResult(board, result), "leaderboard refuses non-Hard runs");
    auto path = directory / "board.txt";
    expect(saveSpeedrunLeaderboard(path, board) && loadSpeedrunLeaderboard(path) == board, "leaderboard persists");
    expect(saveSpeedrunLeaderboard(path, board), "leaderboard supports replacing an existing file");
    { std::ofstream file(directory / "corrupt_board.txt"); file << "GDP_SPEEDRUN_BOARD 1 999999999999\n"; }
    expect(loadSpeedrunLeaderboard(directory / "corrupt_board.txt").empty(), "corrupt leaderboard cannot allocate an unbounded list");
    for (Difficulty difficulty : {Difficulty::Easy, Difficulty::Normal, Difficulty::Hard}) {
        float previous = 0.0F;
        for (unsigned floor = 1U; floor <= 10U; ++floor) {
            float multiplier = campaignDifficultyMultiplier(floor, difficulty);
            expect(multiplier > previous && multiplier <= 2.0F, "every base difficulty scales across all ten floors");
            previous = multiplier;
        }
    }
    SessionSystem speedrun{99U}; (void)speedrun.selectRunMode(RunMode::Speedrun);
    expect(speedrun.state().difficulty == Difficulty::Hard && !speedrun.selectDifficulty(Difficulty::Easy), "Speedrun locks Hard before world generation");
    SessionSystem test{99U}; (void)test.selectRunMode(RunMode::TestMode);
    expect(test.update(true, gdp::world::RoomRole::Start, true, false, true).phase == SessionPhase::Playing, "test arena never ends when its robot dies");
}
void campaignLootTests() {
    using namespace gdp::world;
    const std::array zones{SpawnZone{{{-10.0F, 0.0F, -10.0F}, {10.0F, 1.0F, 10.0F}}}};
    unsigned withWeapon = 0U, pistol = 0U, carbine = 0U;
    for (unsigned seed = 1U; seed <= 100U; ++seed) {
        const auto layout = generateLootLayout(seed, RoomRole::Normal, RoomTheme::Industrial, 2U, zones, 1.0F, true);
        expect(validateLootLayout(layout, zones).isValid(), "campaign random weapons respect loot zones, separation and budget");
        expect(layout.drops == generateLootLayout(seed, RoomRole::Normal, RoomTheme::Industrial, 2U, zones, 1.0F, true).drops, "campaign weapon randomization remains deterministic");
        for (const auto& drop : layout.drops) if (drop.weaponArchetype) { ++withWeapon; *drop.weaponArchetype == 0U ? ++pistol : ++carbine; }
    }
    expect(withWeapon > 20U && pistol > 0U && carbine > 0U, "campaign includes random pistols and carbines outside secret rooms");
}
}
int main() {
    const auto directory = std::filesystem::temp_directory_path() / ("gdp_frontend_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    menuTests(); bindingsTests(); layoutTests(); persistenceTests(directory); campaignLootTests();
    std::error_code error; std::filesystem::remove_all(directory, error);
    return failures == 0 ? 0 : 1;
}
