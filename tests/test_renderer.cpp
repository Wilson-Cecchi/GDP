#include "core/Math.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "game/visual/VisualAssets.hpp"
#include "render/Camera.hpp"
#include "render/Mesh.hpp"
#include "game/visual/GdpRenderer.hpp"
#include "render/Texture.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
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

bool isFinite(const wkc::core::Mat4& matrix)
{
    for (const float element : matrix.elements) {
        if (!std::isfinite(element)) {
            return false;
        }
    }
    return true;
}

void testMesh()
{
    using namespace wkc::render;

    const MeshData box = makeBoxMesh();
    std::string reason;
    expect(validateMeshData(box, &reason), "box mesh validates");
    expect(box.vertices.size() == 24, "box has hard-edge vertices");
    expect(box.indices.size() == 36, "box has twelve indexed triangles");
    expect(box.bounds == wkc::core::Aabb{{-0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, 0.5F}},
        "box bounds are exact");

    MeshData invalidIndex = box;
    invalidIndex.indices[0] = 999;
    expect(!validateMeshData(invalidIndex, &reason), "out-of-range mesh index is rejected");

    MeshData invalidVertex = box;
    invalidVertex.vertices[0].position.x = std::numeric_limits<float>::quiet_NaN();
    expect(!validateMeshData(invalidVertex, &reason), "non-finite vertex is rejected");

    MeshData empty;
    expect(!validateMeshData(empty, &reason), "empty mesh is rejected safely");
}

void testTextureData()
{
    using namespace wkc::render;

    const ImageData first = gdp::game::makeIndustrialCheckerImage(8, 8);
    const ImageData second = gdp::game::makeIndustrialCheckerImage(8, 8);
    expect(validateImageData(first), "generated RGBA image validates");
    expect(first.rgba == second.rgba, "procedural texture pixels are deterministic");
    expect(first.rgba.size() == 8U * 8U * 4U, "texture contains four channels per pixel");
    expect(first.rgba[0] != first.rgba[(2U * 4U)], "checker pattern contains two tones");

    const gdp::game::CeilingFixtureMaterialImages fixture
        = gdp::game::makeCeilingFixtureMaterialImages();
    const gdp::game::CeilingFixtureMaterialImages repeatedFixture
        = gdp::game::makeCeilingFixtureMaterialImages();
    expect(validateImageData(fixture.albedo) && validateImageData(fixture.normal)
            && validateImageData(fixture.properties) && validateImageData(fixture.emission)
            && fixture.albedo.width == 1024 && fixture.albedo.height == 1024,
        "independent ceiling-fixture material provides four valid 1024 HD maps");
    expect(fixture.albedo.rgba == repeatedFixture.albedo.rgba
            && fixture.normal.rgba == repeatedFixture.normal.rgba
            && fixture.properties.rgba == repeatedFixture.properties.rgba
            && fixture.emission.rgba == repeatedFixture.emission.rgba,
        "ceiling-fixture HD maps are byte-exact and deterministic");
    const auto fixturePixel = [](const int x, const int y) {
        return (static_cast<std::size_t>(y) * 1024U + static_cast<std::size_t>(x)) * 4U;
    };
    const std::size_t housing = fixturePixel(256, 512);
    const std::size_t metal = fixturePixel(600, 512);
    const std::size_t lens = fixturePixel(896, 512);
    expect(fixture.albedo.rgba[housing] < 80U && fixture.albedo.rgba[lens] > 210U,
        "ceiling-fixture atlas separates graphite housing from its frosted lens");
    expect(fixture.properties.rgba[metal + 1U] > 220U
            && fixture.properties.rgba[housing + 1U] < 100U,
        "packed material map separates exposed metal from painted housing");
    expect(fixture.emission.rgba[housing] == 0U && fixture.emission.rgba[metal] == 0U
            && fixture.emission.rgba[lens] > 240U,
        "emission map isolates light output to the lens atlas region");

    ImageData malformed = first;
    malformed.rgba.pop_back();
    expect(!validateImageData(malformed), "malformed texture data is rejected");
}

void testCameraAndMatrices()
{
    using namespace wkc::core;
    using wkc::render::Camera;

    expect(nearlyEqual(length(Vec3{3.0F, 4.0F, 0.0F}), 5.0F), "vector length is correct");
    expect(cross({1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}) == Vec3{0.0F, 0.0F, 1.0F},
        "cross product follows the coordinate convention");

    const Mat4 model = translationMatrix({2.0F, 3.0F, 4.0F})
        * scaleMatrix({5.0F, 6.0F, 7.0F});
    expect(nearlyEqual(model.elements[0], 5.0F), "model matrix contains X scale");
    expect(nearlyEqual(model.elements[5], 6.0F), "model matrix contains Y scale");
    expect(nearlyEqual(model.elements[10], 7.0F), "model matrix contains Z scale");
    expect(nearlyEqual(model.elements[12], 2.0F), "model matrix preserves translation");

    Camera camera;
    camera.lookAt({0.0F, 1.7F, 6.5F}, {0.0F, 1.5F, 0.0F});
    camera.setPerspective(68.0F, 0.05F, 100.0F);
    const Mat4 view = camera.viewMatrix();
    const Mat4 projection = camera.projectionMatrix(16.0F / 9.0F);
    expect(isFinite(view), "view matrix remains finite");
    expect(isFinite(projection), "projection matrix remains finite");
    expect(projection.elements[0] > 0.0F && projection.elements[5] > 0.0F,
        "perspective focal terms are positive");
    expect(nearlyEqual(projection.elements[11], -1.0F),
        "projection uses OpenGL perspective convention");
}

void testMainMenuLayout()
{
    using namespace gdp::game::visual;

    const MainMenuLayout wide = makeMainMenuLayout(1280, 720);
    expect(wide.scale == 2, "wide menu uses doubled bitmap scale");
    expect(wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "wide menu panel stays within the framebuffer");
    expect(wide.panel.contains(wide.startButton.x, wide.startButton.y),
        "Start Run button is inside the menu panel");
    expect(wide.panel.contains(wide.continueButton.x, wide.continueButton.y),
        "Continue button is inside the menu panel");
    expect(wide.panel.contains(wide.campaignButton.x, wide.campaignButton.y),
        "Campaign button is inside the menu panel");
    expect(wide.panel.contains(wide.speedrunButton.x, wide.speedrunButton.y),
        "Speedrun button is inside the menu panel");
    expect(wide.panel.contains(wide.bestSeedButton.x, wide.bestSeedButton.y),
        "Best Seed button is inside the menu panel");
    expect(wide.panel.contains(wide.settingsButton.x, wide.settingsButton.y),
        "Settings button is inside the menu panel");
    expect(wide.panel.contains(wide.exitButton.x, wide.exitButton.y),
        "Exit Game button is inside the menu panel");
    expect(wide.exitButton.y >= wide.panel.y + (16 * wide.scale)
            && wide.exitButton.y + wide.exitButton.height
                <= wide.settingsButton.y
            && wide.settingsButton.y + wide.settingsButton.height
                <= wide.bestSeedButton.y
            && wide.bestSeedButton.y + wide.bestSeedButton.height
                <= wide.speedrunButton.y
            && wide.speedrunButton.y + wide.speedrunButton.height
                <= wide.campaignButton.y
            && wide.campaignButton.y + wide.campaignButton.height
                <= wide.continueButton.y
            && wide.continueButton.y + wide.continueButton.height
                <= wide.startButton.y,
        "seven main-menu buttons remain separated from controls and each other");

    const float startX = static_cast<float>(wide.startButton.x + wide.startButton.width / 2);
    const float startY = static_cast<float>(
        720 - 1 - (wide.startButton.y + wide.startButton.height / 2));
    const float exitX = static_cast<float>(wide.exitButton.x + wide.exitButton.width / 2);
    const float exitY = static_cast<float>(
        720 - 1 - (wide.exitButton.y + wide.exitButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, startX, startY)
            == HudMainMenuSelection::StartRun,
        "top-origin pointer maps to Start Run");
    const float continueX = static_cast<float>(
        wide.continueButton.x + wide.continueButton.width / 2);
    const float continueY = static_cast<float>(
        720 - 1 - (wide.continueButton.y + wide.continueButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, continueX, continueY)
            == HudMainMenuSelection::ContinueCampaign,
        "top-origin pointer maps to Continue Campaign");
    const float campaignX = static_cast<float>(
        wide.campaignButton.x + wide.campaignButton.width / 2);
    const float campaignY = static_cast<float>(
        720 - 1 - (wide.campaignButton.y + wide.campaignButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, campaignX, campaignY)
            == HudMainMenuSelection::Campaign,
        "top-origin pointer maps to Campaign");
    const float speedrunX = static_cast<float>(
        wide.speedrunButton.x + wide.speedrunButton.width / 2);
    const float speedrunY = static_cast<float>(
        720 - 1 - (wide.speedrunButton.y + wide.speedrunButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, speedrunX, speedrunY)
            == HudMainMenuSelection::Speedrun,
        "top-origin pointer maps to Speedrun");
    const float bestSeedX = static_cast<float>(
        wide.bestSeedButton.x + wide.bestSeedButton.width / 2);
    const float bestSeedY = static_cast<float>(
        720 - 1 - (wide.bestSeedButton.y + wide.bestSeedButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, bestSeedX, bestSeedY)
            == HudMainMenuSelection::BestSeed,
        "top-origin pointer maps to Best Seed");
    const float settingsX = static_cast<float>(
        wide.settingsButton.x + wide.settingsButton.width / 2);
    const float settingsY = static_cast<float>(
        720 - 1 - (wide.settingsButton.y + wide.settingsButton.height / 2));
    expect(mainMenuSelectionAt(1280, 720, settingsX, settingsY)
            == HudMainMenuSelection::Settings,
        "top-origin pointer maps to Settings");
    expect(mainMenuSelectionAt(1280, 720, exitX, exitY)
            == HudMainMenuSelection::ExitGame,
        "top-origin pointer maps to Exit Game");
    expect(!mainMenuSelectionAt(1280, 720, 0.0F, 0.0F),
        "pointer outside buttons has no selection");

    const MainMenuLayout compact = makeMainMenuLayout(320, 200);
    expect(compact.scale == 1, "compact menu uses base bitmap scale");
    expect(compact.panel.x >= 0 && compact.panel.y >= 0
            && compact.panel.x + compact.panel.width <= 320
            && compact.panel.y + compact.panel.height <= 200,
        "compact menu remains within the framebuffer");

    const MainMenuLayout tiny = makeMainMenuLayout(12, 12);
    expect(tiny.panel.contains(tiny.startButton.x, tiny.startButton.y)
            && tiny.panel.contains(
                tiny.continueButton.x, tiny.continueButton.y)
            && tiny.panel.contains(
                tiny.campaignButton.x, tiny.campaignButton.y)
            && tiny.panel.contains(
                tiny.speedrunButton.x, tiny.speedrunButton.y)
            && tiny.panel.contains(
                tiny.bestSeedButton.x, tiny.bestSeedButton.y)
            && tiny.panel.contains(tiny.settingsButton.x, tiny.settingsButton.y)
            && tiny.panel.contains(tiny.exitButton.x, tiny.exitButton.y),
        "tiny menu clamps all hit targets inside the framebuffer");
}

void testPauseMenuLayout()
{
    using namespace gdp::game::visual;

    const PauseMenuLayout wide = makePauseMenuLayout(1280, 720);
    expect(wide.scale == 2, "wide pause menu uses doubled bitmap scale");
    expect(wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "wide pause menu panel stays within the framebuffer");
    expect(wide.panel.contains(wide.resumeButton.x, wide.resumeButton.y)
            && wide.panel.contains(wide.restartButton.x, wide.restartButton.y)
            && wide.panel.contains(wide.settingsButton.x, wide.settingsButton.y)
            && wide.panel.contains(wide.mainMenuButton.x, wide.mainMenuButton.y)
            && wide.panel.contains(wide.exitButton.x, wide.exitButton.y),
        "pause menu hit targets stay inside the panel");

    const auto pointerX = [](const UiRect& button) {
        return static_cast<float>(button.x + button.width / 2);
    };
    const auto pointerY = [](const UiRect& button) {
        return static_cast<float>(
            720 - 1 - (button.y + button.height / 2));
    };
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.resumeButton), pointerY(wide.resumeButton))
            == HudPauseSelection::Resume,
        "top-origin pointer maps to Resume");
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.restartButton), pointerY(wide.restartButton))
            == HudPauseSelection::RestartRun,
        "top-origin pointer maps to Restart Run");
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.settingsButton), pointerY(wide.settingsButton))
            == HudPauseSelection::Settings,
        "top-origin pointer maps to pause Settings");
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.mainMenuButton), pointerY(wide.mainMenuButton))
            == HudPauseSelection::MainMenu,
        "top-origin pointer maps to pause Main Menu");
    expect(pauseMenuSelectionAt(1280, 720,
               pointerX(wide.exitButton), pointerY(wide.exitButton))
            == HudPauseSelection::ExitGame,
        "top-origin pointer maps to Exit Game");
    expect(!pauseMenuSelectionAt(1280, 720, 0.0F, 0.0F),
        "pointer outside pause buttons has no selection");

    const PauseMenuLayout compact = makePauseMenuLayout(320, 200);
    expect(compact.scale == 1,
        "compact pause menu uses base bitmap scale");
    expect(compact.panel.x >= 0 && compact.panel.y >= 0
            && compact.panel.x + compact.panel.width <= 320
            && compact.panel.y + compact.panel.height <= 200,
        "compact pause menu remains within the framebuffer");

    const PauseMenuLayout tiny = makePauseMenuLayout(12, 12);
    expect(tiny.panel.contains(tiny.resumeButton.x, tiny.resumeButton.y)
            && tiny.panel.contains(tiny.restartButton.x, tiny.restartButton.y)
            && tiny.panel.contains(tiny.settingsButton.x, tiny.settingsButton.y)
            && tiny.panel.contains(tiny.mainMenuButton.x, tiny.mainMenuButton.y)
            && tiny.panel.contains(tiny.exitButton.x, tiny.exitButton.y),
        "tiny pause menu clamps all hit targets inside the framebuffer");
}

void testSettingsMenuLayout()
{
    using namespace gdp::game::visual;
    const SettingsMenuLayout wide = makeSettingsMenuLayout(1280, 720);
    expect(wide.scale == 2, "wide settings menu uses doubled bitmap scale");
    expect(wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "settings panel stays within the framebuffer");
    for (std::size_t index = 0U; index < wide.rows.size(); ++index) {
        const UiRect row = wide.rows[index];
        expect(wide.panel.contains(row.x, row.y),
            "settings row starts inside the panel");
        const float pointerX = static_cast<float>(row.x + row.width / 2);
        const float pointerY = static_cast<float>(
            720 - 1 - (row.y + row.height / 2));
        expect(settingsMenuSelectionAt(1280, 720, pointerX, pointerY)
                == static_cast<HudSettingsSelection>(index),
            "top-origin pointer maps to each settings row");
        if (index < static_cast<std::size_t>(HudSettingsSelection::Back)) {
            const UiRect decrement = wide.decrementButtons[index];
            const UiRect increment = wide.incrementButtons[index];
            expect(row.contains(decrement.x, decrement.y)
                    && row.contains(increment.x, increment.y),
                "settings arrow hit targets stay inside their row");
            const float decrementX
                = static_cast<float>(decrement.x + decrement.width / 2);
            const float decrementY = static_cast<float>(
                720 - 1 - (decrement.y + decrement.height / 2));
            const float incrementX
                = static_cast<float>(increment.x + increment.width / 2);
            const float incrementY = static_cast<float>(
                720 - 1 - (increment.y + increment.height / 2));
            expect(settingsMenuAdjustmentDirectionAt(
                       1280, 720, decrementX, decrementY)
                    == -1,
                "left settings arrow always decrements");
            expect(settingsMenuAdjustmentDirectionAt(
                       1280, 720, incrementX, incrementY)
                    == 1,
                "right settings arrow always increments");
            expect(!settingsMenuAdjustmentDirectionAt(
                1280, 720, static_cast<float>(row.x + 4), pointerY),
                "clicking a settings label selects without changing its value");
        }
    }
    expect(!settingsMenuSelectionAt(1280, 720, 0.0F, 0.0F),
        "pointer outside settings rows has no selection");
    expect(!settingsMenuAdjustmentDirectionAt(1280, 720, 0.0F, 0.0F)
            && !settingsMenuAdjustmentDirectionAt(0, 720, 1.0F, 1.0F),
        "pointer outside arrow buttons cannot adjust settings");

    const SettingsMenuLayout medium = makeSettingsMenuLayout(640, 360);
    expect(medium.scale == 1,
        "medium settings menu keeps all eleven rows at base bitmap scale");
    for (std::size_t index = 1U; index < medium.rows.size(); ++index) {
        expect(medium.rows[index].y + medium.rows[index].height
                <= medium.rows[index - 1U].y,
            "medium settings rows never overlap");
    }

    const SettingsMenuLayout compact = makeSettingsMenuLayout(320, 200);
    expect(compact.scale == 1
            && compact.panel.x + compact.panel.width <= 320
            && compact.panel.y + compact.panel.height <= 200,
        "compact settings menu remains within the framebuffer");
    for (std::size_t index = 1U; index < compact.rows.size(); ++index) {
        expect(compact.rows[index].y + compact.rows[index].height
                <= compact.rows[index - 1U].y,
            "compact settings rows remain separated when space permits");
    }
    const SettingsMenuLayout tiny = makeSettingsMenuLayout(12, 12);
    for (const UiRect row : tiny.rows) {
        expect(tiny.panel.contains(row.x, row.y),
            "tiny settings menu clamps rows inside the framebuffer");
    }
}

void testResultMenuLayout()
{
    using namespace gdp::game::visual;

    const ResultMenuLayout wide = makeResultMenuLayout(1280, 720);
    expect(wide.scale == 2, "wide result menu uses doubled bitmap scale");
    expect(wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "wide result menu panel stays within the framebuffer");
    expect(wide.panel.contains(
               wide.playAgainButton.x, wide.playAgainButton.y)
            && wide.panel.contains(
                wide.replaySeedButton.x, wide.replaySeedButton.y)
            && wide.panel.contains(wide.mainMenuButton.x, wide.mainMenuButton.y)
            && wide.panel.contains(wide.exitButton.x, wide.exitButton.y),
        "result menu hit targets stay inside the panel");
    expect(wide.exitButton.y + wide.exitButton.height
                <= wide.mainMenuButton.y
            && wide.mainMenuButton.y + wide.mainMenuButton.height
                <= wide.replaySeedButton.y
            && wide.replaySeedButton.y + wide.replaySeedButton.height
                <= wide.playAgainButton.y,
        "all result menu hit targets remain visually separated");

    const auto pointerX = [](const UiRect& button) {
        return static_cast<float>(button.x + button.width / 2);
    };
    const auto pointerY = [](const UiRect& button) {
        return static_cast<float>(
            720 - 1 - (button.y + button.height / 2));
    };
    expect(resultMenuSelectionAt(1280, 720,
               pointerX(wide.playAgainButton), pointerY(wide.playAgainButton))
            == HudResultMenuSelection::PlayAgain,
        "top-origin pointer maps to Play Again");
    expect(resultMenuSelectionAt(1280, 720,
               pointerX(wide.replaySeedButton), pointerY(wide.replaySeedButton))
            == HudResultMenuSelection::ReplaySeed,
        "top-origin pointer maps to Replay Seed");
    expect(resultMenuSelectionAt(1280, 720,
               pointerX(wide.mainMenuButton), pointerY(wide.mainMenuButton))
            == HudResultMenuSelection::MainMenu,
        "top-origin pointer maps to result Main Menu");
    expect(resultMenuSelectionAt(1280, 720,
               pointerX(wide.exitButton), pointerY(wide.exitButton))
            == HudResultMenuSelection::ExitGame,
        "top-origin pointer maps to result Exit Game");
    expect(!resultMenuSelectionAt(1280, 720, 0.0F, 0.0F),
        "pointer outside result buttons has no selection");

    const ResultMenuLayout compact = makeResultMenuLayout(320, 200);
    expect(compact.scale == 1,
        "compact result menu uses base bitmap scale");
    expect(compact.panel.x >= 0 && compact.panel.y >= 0
            && compact.panel.x + compact.panel.width <= 320
            && compact.panel.y + compact.panel.height <= 200,
        "compact result menu remains within the framebuffer");

    const ResultMenuLayout tiny = makeResultMenuLayout(12, 12);
    expect(tiny.panel.contains(
               tiny.playAgainButton.x, tiny.playAgainButton.y)
            && tiny.panel.contains(
                tiny.replaySeedButton.x, tiny.replaySeedButton.y)
            && tiny.panel.contains(tiny.mainMenuButton.x, tiny.mainMenuButton.y)
            && tiny.panel.contains(tiny.exitButton.x, tiny.exitButton.y),
        "tiny result menu clamps all hit targets inside the framebuffer");
}

void testExitConfirmationLayout()
{
    using namespace gdp::game::visual;

    const ExitConfirmationLayout wide
        = makeExitConfirmationLayout(1280, 720);
    expect(wide.scale == 2
            && wide.panel.x >= 0 && wide.panel.y >= 0
            && wide.panel.x + wide.panel.width <= 1280
            && wide.panel.y + wide.panel.height <= 720,
        "wide exit confirmation stays inside the framebuffer");
    expect(wide.panel.contains(wide.cancelButton.x, wide.cancelButton.y)
            && wide.panel.contains(wide.exitButton.x, wide.exitButton.y)
            && wide.cancelButton.x + wide.cancelButton.width
                <= wide.exitButton.x,
        "confirmation exposes distinct Cancel and Exit hit targets");
    const auto pointerX = [](const UiRect& button) {
        return static_cast<float>(button.x + button.width / 2);
    };
    const auto pointerY = [](const UiRect& button) {
        return static_cast<float>(
            720 - 1 - (button.y + button.height / 2));
    };
    expect(exitConfirmationSelectionAt(1280, 720,
               pointerX(wide.cancelButton), pointerY(wide.cancelButton))
            == HudExitConfirmationSelection::Cancel,
        "top-origin pointer maps to the safe Cancel choice");
    expect(exitConfirmationSelectionAt(1280, 720,
               pointerX(wide.exitButton), pointerY(wide.exitButton))
            == HudExitConfirmationSelection::ExitGame,
        "top-origin pointer maps to the explicit Exit choice");
    expect(!exitConfirmationSelectionAt(1280, 720, 0.0F, 0.0F)
            && !exitConfirmationSelectionAt(0, 720, 1.0F, 1.0F),
        "invalid or external confirmation pointers are ignored");

    const ExitConfirmationLayout compact
        = makeExitConfirmationLayout(320, 200);
    expect(compact.scale == 1
            && compact.panel.x + compact.panel.width <= 320
            && compact.panel.y + compact.panel.height <= 200,
        "compact exit confirmation remains within the framebuffer");
    const ExitConfirmationLayout tiny
        = makeExitConfirmationLayout(12, 12);
    expect(tiny.panel.contains(tiny.cancelButton.x, tiny.cancelButton.y)
            && tiny.panel.contains(tiny.exitButton.x, tiny.exitButton.y),
        "tiny confirmation clamps both choices inside its panel");
}

void testRunTimeFormatting()
{
    using gdp::game::visual::runTimeText;

    expect(runTimeText(0U) == "00:00.00"
            && runTimeText(1U) == "00:00.01"
            && runTimeText(59U) == "00:00.98"
            && runTimeText(60U) == "00:01.00"
            && runTimeText(216'000U) == "01:00:00.00",
        "run timer formatting is deterministic across tick boundaries");
}

void testEnemyRootAnimation()
{
    using namespace gdp::game::visual;

    const EnemyRootPose inactive = sampleEnemyRootPose(
        EnemyAnimationState::Walk, 30U, 111U, false);
    expect(inactive == EnemyRootPose{},
        "dormant enemy previews remain visually static");

    const EnemyRootPose dead = sampleEnemyRootPose(
        EnemyAnimationState::Dead, 999U, 111U);
    expect(dead == EnemyRootPose{
            {0.0F, 0.04F, 0.0F}, {1.15F, 0.18F, 1.15F}},
        "dead enemies keep the established flattened terminal pose");

    const EnemyRootPose idle = sampleEnemyRootPose(
        EnemyAnimationState::Idle, 7U, 111U);
    expect(idle == sampleEnemyRootPose(
            EnemyAnimationState::Idle, 7U, 111U),
        "enemy root animation is deterministic for the same tick and visual seed");
    expect(idle != sampleEnemyRootPose(
            EnemyAnimationState::Idle, 7U, 112U),
        "visual seeds offset enemy cycles to avoid synchronized groups");

    float maximumIdleLift = 0.0F;
    float maximumWalkLift = 0.0F;
    float maximumAttackReach = 1.0F;
    bool finiteAndGrounded = true;
    for (std::uint64_t tick = 0U; tick < 120U; ++tick) {
        const EnemyRootPose idlePose = sampleEnemyRootPose(
            EnemyAnimationState::Idle, tick, 111U);
        const EnemyRootPose walkPose = sampleEnemyRootPose(
            EnemyAnimationState::Walk, tick, 111U);
        const EnemyRootPose attackPose = sampleEnemyRootPose(
            EnemyAnimationState::Attack, tick, 111U);
        maximumIdleLift = std::max(maximumIdleLift, idlePose.offset.y);
        maximumWalkLift = std::max(maximumWalkLift, walkPose.offset.y);
        maximumAttackReach = std::max(maximumAttackReach, attackPose.scale.z);
        for (const EnemyRootPose pose : {idlePose, walkPose, attackPose}) {
            finiteAndGrounded = finiteAndGrounded
                && std::isfinite(pose.offset.y)
                && std::isfinite(pose.scale.x)
                && std::isfinite(pose.scale.y)
                && std::isfinite(pose.scale.z)
                && pose.offset.y >= 0.0F
                && pose.scale.x > 0.0F
                && pose.scale.y > 0.0F
                && pose.scale.z > 0.0F;
        }
    }
    expect(finiteAndGrounded,
        "all live root poses stay finite, positive and above the floor plane");
    expect(maximumIdleLift <= 0.0121F
            && maximumWalkLift > maximumIdleLift + 0.02F,
        "Walk has a stronger readable cadence than the restrained Idle cycle");
    expect(maximumAttackReach > 1.05F,
        "Attack produces a distinct axial impulse at its peak");
}

} // namespace

int main()
{
    testMesh();
    testTextureData();
    testCameraAndMatrices();
    testMainMenuLayout();
    testPauseMenuLayout();
    testResultMenuLayout();
    testExitConfirmationLayout();
    testRunTimeFormatting();
    testEnemyRootAnimation();
    testSettingsMenuLayout();

    if (failures != 0) {
        std::cerr << failures << " renderer test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP renderer CPU tests passed\n";
    return 0;
}
