#include "game/visual/GdpRenderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    constexpr int width = 160;
    constexpr int height = 100;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)
        || !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)) {
        std::cerr << "SDL_GL_SetAttribute failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Window* const window = SDL_CreateWindow(
        "GDP renderer test",
        width,
        height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr || !SDL_GL_MakeCurrent(window, context)) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        if (context != nullptr) {
            SDL_GL_DestroyContext(context);
        }
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool passed = false;
    {
        gdp::game::visual::GdpRenderer renderer;
        constexpr std::array enemies{
            gdp::game::visual::EnemyRenderData{{1.5F, 0.0F, -0.5F}, true, 111},
            gdp::game::visual::EnemyRenderData{{-1.5F, 0.0F, -1.0F}, false, 222},
            gdp::game::visual::EnemyRenderData{
                {0.0F, 0.0F, -2.0F}, true, 333, gdp::game::visual::EnemyVisualRole::Guard},
            gdp::game::visual::EnemyRenderData{
                {0.0F, 0.0F, 0.5F}, true, 444, gdp::game::visual::EnemyVisualRole::Officer},
            gdp::game::visual::EnemyRenderData{
                {-0.5F, 0.0F, 1.0F}, true, 555, gdp::game::visual::EnemyVisualRole::Engineer},
            gdp::game::visual::EnemyRenderData{
                {0.75F, 0.0F, 1.0F}, true, 666,
                gdp::game::visual::EnemyVisualRole::IndustrialRobot},
            gdp::game::visual::EnemyRenderData{
                {-0.75F, 0.0F, 0.5F}, true, 111,
                gdp::game::visual::EnemyVisualRole::Worker, false},
        };
        constexpr std::array workerOnly{
            gdp::game::visual::EnemyRenderData{{1.5F, 0.0F, -0.5F}, true, 111},
        };
        constexpr std::array guardOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 333, gdp::game::visual::EnemyVisualRole::Guard},
        };
        constexpr std::array officerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 444, gdp::game::visual::EnemyVisualRole::Officer},
        };
        constexpr std::array engineerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 555, gdp::game::visual::EnemyVisualRole::Engineer},
        };
        constexpr std::array industrialRobotOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 666,
                gdp::game::visual::EnemyVisualRole::IndustrialRobot},
        };
        constexpr std::array pickups{
            gdp::game::visual::PickupRenderData{
                {-0.60F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Health},
            gdp::game::visual::PickupRenderData{
                {0.0F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Ammo},
            gdp::game::visual::PickupRenderData{
                {0.60F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Key},
            gdp::game::visual::PickupRenderData{
                {1.20F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Armor},
        };
        constexpr std::array props{
            gdp::game::visual::PropRenderData{
                {-1.35F, 0.0F, -1.2F}, {0.78F, 0.72F, 0.78F},
                gdp::game::visual::PropVisualRole::Crate,
                gdp::game::visual::PropVisualState::Intact, 7001},
            gdp::game::visual::PropRenderData{
                {0.0F, 0.0F, -1.2F}, {0.46F, 1.02F, 0.46F},
                gdp::game::visual::PropVisualRole::Canister,
                gdp::game::visual::PropVisualState::Damaged, 7002},
            gdp::game::visual::PropRenderData{
                {1.35F, 0.0F, -1.2F}, {1.16F, 0.92F, 0.34F},
                gdp::game::visual::PropVisualRole::Barricade,
                gdp::game::visual::PropVisualState::Destroyed, 7003},
        };
        gdp::game::visual::RoomLightingRenderData proceduralLighting;
        proceduralLighting.ambientColor = {0.52F, 0.68F, 0.82F};
        proceduralLighting.ambientIntensity = 0.18F;
        proceduralLighting.lightCount = 8U;
        proceduralLighting.fixtureEmissive = 0.84F;
        proceduralLighting.lights[0] = {
            {-2.0F, 3.82F, -2.0F}, {0.48F, 0.74F, 1.0F}, 1.05F, 5.4F};
        proceduralLighting.lights[1] = {
            {2.0F, 3.82F, -2.0F}, {0.60F, 0.82F, 1.0F}, 0.95F, 5.8F};
        proceduralLighting.lights[2] = {
            {0.0F, 3.82F, 2.0F}, {0.42F, 0.68F, 0.94F}, 1.10F, 5.2F};
        proceduralLighting.lights[3] = {
            {-3.0F, 3.82F, 2.0F}, {0.72F, 0.80F, 1.0F}, 0.72F, 4.8F};
        proceduralLighting.lights[4] = {
            {3.0F, 3.82F, 2.0F}, {0.72F, 0.80F, 1.0F}, 0.72F, 4.8F};
        proceduralLighting.lights[5] = {
            {-2.5F, 3.82F, -5.0F}, {0.55F, 0.72F, 0.96F}, 0.82F, 5.1F};
        proceduralLighting.lights[6] = {
            {2.5F, 3.82F, -5.0F}, {0.55F, 0.72F, 0.96F}, 0.82F, 5.1F};
        proceduralLighting.lights[7] = {
            {0.0F, 3.82F, -7.0F}, {0.48F, 0.66F, 0.92F}, 0.88F, 5.4F};
        if (!renderer.initialize(width, height) || !renderer.render(width, height)) {
            std::cerr << "Renderer failed to produce a frame\n";
        } else {
            std::vector<std::uint8_t> baseline(
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, baseline.data());

            const bool renderedRoomVariant = renderer.prepareIndustrialRoomMaterials(12345)
                && renderer.render(width, height);
            std::vector<std::uint8_t> roomPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, roomPixels.data());
            const bool repeatedRoom = renderer.prepareIndustrialRoomMaterials(12345)
                && renderer.render(width, height);
            std::vector<std::uint8_t> repeatedRoomPixels(baseline.size());
            glReadPixels(
                0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, repeatedRoomPixels.data());

            const bool renderedStorageVariant = renderer.prepareStorageRoomMaterials(24680)
                && renderer.render(width, height);
            const std::uint64_t storageMaterialKey = renderer.activeRoomMaterialKey();
            std::vector<std::uint8_t> storagePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, storagePixels.data());

            const bool renderedCombatVariant = renderer.prepareCombatRoomMaterials(13579)
                && renderer.render(width, height);
            std::vector<std::uint8_t> combatPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, combatPixels.data());

            const bool renderedCafeteriaVariant
                = renderer.prepareCafeteriaRoomMaterials(97531)
                && renderer.render(width, height);
            std::vector<std::uint8_t> cafeteriaPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, cafeteriaPixels.data());

            const bool renderedLaboratoryVariant
                = renderer.prepareLaboratoryRoomMaterials(86420)
                && renderer.render(width, height);
            std::vector<std::uint8_t> laboratoryPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, laboratoryPixels.data());

            const bool renderedTechnicalVariant
                = renderer.prepareTechnicalRoomMaterials(112233)
                && renderer.render(width, height);
            std::vector<std::uint8_t> technicalPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, technicalPixels.data());

            const bool renderedSecurityVariant
                = renderer.prepareSecurityRoomMaterials(778899)
                && renderer.render(width, height);
            std::vector<std::uint8_t> securityPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, securityPixels.data());

            const bool renderedCentralHubVariant
                = renderer.prepareCentralHubRoomMaterials(424242)
                && renderer.render(width, height);
            const std::uint64_t centralHubMaterialKey = renderer.activeRoomMaterialKey();
            std::vector<std::uint8_t> centralHubPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, centralHubPixels.data());

            const bool renderedProceduralLighting = renderer.render(
                width, height, {}, {}, {}, {}, proceduralLighting);
            std::vector<std::uint8_t> lightingPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                lightingPixels.data());

            proceduralLighting.alert = true;
            const bool renderedAlertLighting = renderer.render(
                width, height, {}, {}, {}, {}, proceduralLighting);
            std::vector<std::uint8_t> alertLightingPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                alertLightingPixels.data());

            gdp::game::visual::RoomLightingRenderData translatedLighting;
            translatedLighting.lightCount = 1U;
            translatedLighting.lights[0] = {
                {16.0F, 3.82F, -14.0F}, {0.52F, 0.78F, 1.0F}, 1.10F, 5.6F};
            constexpr gdp::game::visual::CameraData translatedCamera{
                {18.0F, 1.62F, -14.0F}, {1.0F, 0.0F, 0.0F}, 68.0F};
            constexpr gdp::game::visual::RoomRenderData translatedRoom{
                {14.0F, 0.0F, -14.0F}, wkc::core::QuarterTurn::Degrees90, 0U,
                {{9.15F, 0.0F, -17.85F}, {18.85F, 4.0F, -10.15F}}};
            std::array translatedDoors{
                gdp::game::visual::DoorRenderData{
                    {18.85F, 0.0F, -14.0F}, {1.0F, 0.0F, 0.0F},
                    1.2F, 2.2F, false},
            };
            const bool renderedTranslatedDoor = renderer.render(
                width, height, translatedCamera, {}, {}, {}, translatedLighting,
                translatedRoom, translatedDoors);
            std::vector<std::uint8_t> translatedDoorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                translatedDoorPixels.data());
            translatedDoors.front().locked = true;
            translatedDoors.front().openFraction = 0.0F;
            const bool renderedLockedDoor = renderer.render(
                width, height, translatedCamera, {}, {}, {}, translatedLighting,
                translatedRoom, translatedDoors);
            std::vector<std::uint8_t> lockedDoorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                lockedDoorPixels.data());
            translatedDoors.front().locked = false;
            translatedDoors.front().openFraction = 0.45F;
            const bool renderedMovingDoor = renderer.render(
                width, height, translatedCamera, {}, {}, {}, translatedLighting,
                translatedRoom, translatedDoors);
            std::vector<std::uint8_t> movingDoorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                movingDoorPixels.data());

            constexpr std::array corridorDoors{
                gdp::game::visual::DoorRenderData{
                    {0.0F, 0.0F, -4.85F}, {0.0F, 0.0F, -1.0F},
                    1.2F, 2.2F, false},
                gdp::game::visual::DoorRenderData{
                    {0.0F, 0.0F, -10.0F}, {0.0F, 0.0F, 1.0F},
                    1.2F, 2.2F, false},
            };
            constexpr gdp::game::visual::CameraData corridorCamera{
                {0.0F, 1.62F, -7.0F}, {0.0F, 0.0F, -1.0F}, 68.0F};
            const bool renderedOpenDoor = renderer.render(
                width, height, corridorCamera, {}, {}, {}, {}, {}, corridorDoors);
            std::vector<std::uint8_t> openDoorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                openDoorPixels.data());
            constexpr std::array corridorPoints{
                wkc::core::Vec3{0.0F, 0.0F, -4.85F},
                wkc::core::Vec3{0.0F, 0.0F, -10.0F},
            };
            const std::array corridorGeometry{
                gdp::game::visual::CorridorRenderData{corridorPoints, 1.2F, 2.2F},
            };
            const std::array connectedRoomGeometry{
                gdp::game::visual::RoomRenderData{
                    {0.0F, 0.0F, -14.85F}, wkc::core::QuarterTurn::Degrees0,
                    storageMaterialKey,
                    {{-5.85F, 0.0F, -21.70F}, {5.85F, 5.0F, -10.0F}}},
            };
            const bool renderedCorridor = renderer.render(
                width, height, corridorCamera, {}, {}, {}, {}, {}, corridorDoors,
                corridorGeometry, connectedRoomGeometry);
            std::vector<std::uint8_t> corridorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                corridorPixels.data());
            const std::array connectedRoomCentralHub{
                gdp::game::visual::RoomRenderData{
                    {0.0F, 0.0F, -14.85F}, wkc::core::QuarterTurn::Degrees0,
                    centralHubMaterialKey,
                    {{-5.85F, 0.0F, -21.70F}, {5.85F, 5.0F, -10.0F}}},
            };
            const bool renderedConnectedRoomMaterial = renderer.render(
                width, height, corridorCamera, {}, {}, {}, {}, {}, corridorDoors,
                corridorGeometry, connectedRoomCentralHub);
            std::vector<std::uint8_t> connectedRoomMaterialPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                connectedRoomMaterialPixels.data());

            constexpr std::array cornerPoints{
                wkc::core::Vec3{0.0F, 0.0F, 0.0F},
                wkc::core::Vec3{4.0F, 0.0F, 0.0F},
                wkc::core::Vec3{4.0F, 0.0F, -5.0F},
            };
            const std::array cornerCorridor{
                gdp::game::visual::CorridorRenderData{
                    cornerPoints, 1.2F, 2.2F, centralHubMaterialKey},
            };
            constexpr std::array cornerDoors{
                gdp::game::visual::DoorRenderData{
                    {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
                    1.2F, 2.2F, false, 1.0F},
                gdp::game::visual::DoorRenderData{
                    {4.0F, 0.0F, -5.0F}, {0.0F, 0.0F, 1.0F},
                    1.2F, 2.2F, false, 1.0F},
            };
            const std::array cornerConnectedRoom{
                gdp::game::visual::RoomRenderData{
                    {4.0F, 0.0F, -9.85F}, wkc::core::QuarterTurn::Degrees0,
                    storageMaterialKey,
                    {{0.15F, 0.0F, -14.70F}, {7.85F, 4.0F, -5.0F}}},
            };
            const gdp::game::visual::RoomRenderData cornerRoom{
                {-4.85F, 0.0F, 0.0F}, wkc::core::QuarterTurn::Degrees90,
                centralHubMaterialKey,
                {{-9.70F, 0.0F, -3.85F}, {0.0F, 4.0F, 3.85F}}};
            constexpr gdp::game::visual::CameraData cornerCamera{
                {-0.35F, 1.62F, 0.0F}, {0.94F, 0.0F, -0.34F}, 68.0F};
            gdp::game::visual::RoomLightingRenderData cornerLighting;
            cornerLighting.ambientColor = {0.62F, 0.72F, 0.82F};
            cornerLighting.ambientIntensity = 0.30F;
            const bool renderedCornerCorridor = renderer.render(
                width, height, cornerCamera, {}, {}, {}, cornerLighting,
                cornerRoom, cornerDoors, cornerCorridor, cornerConnectedRoom);
            std::vector<std::uint8_t> cornerCorridorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                cornerCorridorPixels.data());

            const bool renderedHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                gdp::game::visual::HudRenderData{gdp::game::visual::ObjectiveRenderState::Victory});
            std::vector<std::uint8_t> hudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                hudPixels.data());

            gdp::game::visual::HudRenderData resultExitHud{
                gdp::game::visual::ObjectiveRenderState::Victory};
            resultExitHud.resultMenuSelection
                = gdp::game::visual::HudResultMenuSelection::ExitGame;
            const bool renderedResultExit = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, resultExitHud);
            std::vector<std::uint8_t> resultExitPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                resultExitPixels.data());

            const gdp::game::visual::HudRenderData functionalHud{
                .objective = gdp::game::visual::ObjectiveRenderState::ReachBoss,
                .visible = true,
                .health = 24.0F,
                .maximumHealth = 115.0F,
                .armor = 18.0F,
                .maximumArmor = 60.0F,
                .magazineAmmo = 0U,
                .reserveAmmo = 28U,
                .keys = 2U,
                .level = 4U,
                .experience = 35U,
                .experienceForNextLevel = 80U,
                .event = gdp::game::visual::HudEventState::Active,
                .eventLocked = true,
                .prompt = gdp::game::visual::HudPrompt::DoorLocked,
                .damageFeedback = 1.0F,
                .hitFeedback = gdp::game::visual::HudHitFeedback::Kill,
                .hitFeedbackStrength = 1.0F,
                .notice = gdp::game::visual::HudNotice::LevelUp,
                .noticeFeedbackStrength = 1.0F,
            };
            const bool renderedFunctionalHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, functionalHud);
            std::vector<std::uint8_t> functionalHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                functionalHudPixels.data());

            gdp::game::visual::HudRenderData pausedHud = functionalHud;
            pausedHud.paused = true;
            pausedHud.pauseSelection = gdp::game::visual::HudPauseSelection::RestartRun;
            const bool renderedPauseMenu = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, pausedHud);
            std::vector<std::uint8_t> pauseMenuPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                pauseMenuPixels.data());

            gdp::game::visual::HudRenderData mainMenuHud;
            mainMenuHud.mainMenu = true;
            const bool renderedMainMenu = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mainMenuHud);
            std::vector<std::uint8_t> mainMenuPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuPixels.data());

            mainMenuHud.mainMenuSelection
                = gdp::game::visual::HudMainMenuSelection::ExitGame;
            const bool renderedMainMenuExit = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mainMenuHud);
            std::vector<std::uint8_t> mainMenuExitPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuExitPixels.data());

            gdp::game::visual::HudRenderData settingsHud;
            settingsHud.settingsMenu = true;
            const bool renderedSettings = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, settingsHud);
            std::vector<std::uint8_t> settingsPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                settingsPixels.data());

            settingsHud.settingsSelection
                = gdp::game::visual::HudSettingsSelection::FieldOfView;
            settingsHud.fieldOfViewDegrees = 90U;
            const bool renderedSettingsFov = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, settingsHud);
            std::vector<std::uint8_t> settingsFovPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                settingsFovPixels.data());

            const bool renderedWorker = renderer.prepareWorkerVisual(111)
                && renderer.render(width, height, {}, workerOnly);
            std::vector<std::uint8_t> workerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, workerPixels.data());

            const bool renderedGuard = renderer.prepareGuardVisual(333)
                && renderer.render(width, height, {}, guardOnly);
            std::vector<std::uint8_t> guardPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, guardPixels.data());

            const bool renderedOfficer = renderer.prepareOfficerVisual(444)
                && renderer.render(width, height, {}, officerOnly);
            std::vector<std::uint8_t> officerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, officerPixels.data());

            const bool renderedEngineer = renderer.prepareEngineerVisual(555)
                && renderer.render(width, height, {}, engineerOnly);
            std::vector<std::uint8_t> engineerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, engineerPixels.data());

            const bool renderedIndustrialRobot
                = renderer.prepareIndustrialRobotVisual(666)
                && renderer.render(width, height, {}, industrialRobotOnly);
            std::vector<std::uint8_t> industrialRobotPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                industrialRobotPixels.data());

            const bool renderedPickups = renderer.render(width, height, {}, {}, pickups);
            std::vector<std::uint8_t> pickupPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                pickupPixels.data());

            const bool renderedProps = renderer.render(width, height, {}, {}, {}, props);
            std::vector<std::uint8_t> propPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                propPixels.data());

            const bool renderedEnemies = renderer.prepareWorkerVisual(111)
                && renderer.prepareWorkerVisual(222)
                && renderer.prepareGuardVisual(333)
                && renderer.prepareOfficerVisual(444)
                && renderer.prepareEngineerVisual(555)
                && renderer.prepareIndustrialRobotVisual(666)
                && renderer.render(width, height, {}, enemies, pickups, props);
            if (!renderedEnemies) {
                std::cerr << "Renderer failed to draw enemy placeholders\n";
            }
            std::vector<std::uint8_t> pixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

            std::uint8_t darkest = 255U;
            std::uint8_t brightest = 0U;
            std::size_t roomChangedChannels = 0;
            std::size_t themeChangedChannels = 0;
            std::size_t combatChangedChannels = 0;
            std::size_t cafeteriaChangedChannels = 0;
            std::size_t laboratoryChangedChannels = 0;
            std::size_t technicalChangedChannels = 0;
            std::size_t securityChangedChannels = 0;
            std::size_t centralHubChangedChannels = 0;
            std::size_t lightingChangedChannels = 0;
            std::size_t alertLightingChangedChannels = 0;
            std::size_t doorStateChangedChannels = 0;
            std::size_t doorMotionChangedChannels = 0;
            std::size_t corridorChangedChannels = 0;
            std::size_t connectedRoomMaterialChangedChannels = 0;
            std::size_t cornerCyanPixels = 0;
            std::size_t hudChangedChannels = 0;
            std::size_t resultSelectionChangedChannels = 0;
            std::size_t functionalHudChangedChannels = 0;
            std::size_t pauseMenuChangedChannels = 0;
            std::size_t mainMenuChangedChannels = 0;
            std::size_t mainMenuSelectionChangedChannels = 0;
            std::size_t settingsMenuChangedChannels = 0;
            std::size_t settingsSelectionChangedChannels = 0;
            std::size_t workerChangedChannels = 0;
            std::size_t guardChangedChannels = 0;
            std::size_t officerChangedChannels = 0;
            std::size_t engineerChangedChannels = 0;
            std::size_t industrialRobotChangedChannels = 0;
            std::size_t pickupChangedChannels = 0;
            std::size_t propChangedChannels = 0;
            std::size_t enemyChangedChannels = 0;
            for (std::size_t index = 0; index < pixels.size(); index += 4U) {
                darkest = std::min(darkest, pixels[index]);
                brightest = std::max(brightest, pixels[index]);
                if (cornerCorridorPixels[index + 1U]
                        > cornerCorridorPixels[index] + 8U
                    && cornerCorridorPixels[index + 2U]
                        > cornerCorridorPixels[index] + 8U
                    && cornerCorridorPixels[index + 2U] > 24U) {
                    ++cornerCyanPixels;
                }
                for (std::size_t channel = 0; channel < 3U; ++channel) {
                    if (roomPixels[index + channel] != baseline[index + channel]) {
                        ++roomChangedChannels;
                    }
                    if (storagePixels[index + channel] != roomPixels[index + channel]) {
                        ++themeChangedChannels;
                    }
                    if (combatPixels[index + channel] != storagePixels[index + channel]) {
                        ++combatChangedChannels;
                    }
                    if (cafeteriaPixels[index + channel] != combatPixels[index + channel]) {
                        ++cafeteriaChangedChannels;
                    }
                    if (laboratoryPixels[index + channel] != cafeteriaPixels[index + channel]) {
                        ++laboratoryChangedChannels;
                    }
                    if (technicalPixels[index + channel] != laboratoryPixels[index + channel]) {
                        ++technicalChangedChannels;
                    }
                    if (securityPixels[index + channel] != technicalPixels[index + channel]) {
                        ++securityChangedChannels;
                    }
                    if (centralHubPixels[index + channel] != securityPixels[index + channel]) {
                        ++centralHubChangedChannels;
                    }
                    if (lightingPixels[index + channel]
                        != centralHubPixels[index + channel]) {
                        ++lightingChangedChannels;
                    }
                    if (alertLightingPixels[index + channel]
                        != lightingPixels[index + channel]) {
                        ++alertLightingChangedChannels;
                    }
                    if (lockedDoorPixels[index + channel]
                        != translatedDoorPixels[index + channel]) {
                        ++doorStateChangedChannels;
                    }
                    if (movingDoorPixels[index + channel]
                        != translatedDoorPixels[index + channel]) {
                        ++doorMotionChangedChannels;
                    }
                    if (corridorPixels[index + channel]
                        != openDoorPixels[index + channel]) {
                        ++corridorChangedChannels;
                    }
                    if (connectedRoomMaterialPixels[index + channel]
                        != corridorPixels[index + channel]) {
                        ++connectedRoomMaterialChangedChannels;
                    }
                    if (hudPixels[index + channel]
                        != centralHubPixels[index + channel]) {
                        ++hudChangedChannels;
                    }
                    if (resultExitPixels[index + channel]
                        != hudPixels[index + channel]) {
                        ++resultSelectionChangedChannels;
                    }
                    if (functionalHudPixels[index + channel]
                        != centralHubPixels[index + channel]) {
                        ++functionalHudChangedChannels;
                    }
                    if (pauseMenuPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++pauseMenuChangedChannels;
                    }
                    if (mainMenuPixels[index + channel] != baseline[index + channel]) {
                        ++mainMenuChangedChannels;
                    }
                    if (mainMenuExitPixels[index + channel]
                        != mainMenuPixels[index + channel]) {
                        ++mainMenuSelectionChangedChannels;
                    }
                    if (settingsPixels[index + channel]
                        != baseline[index + channel]) {
                        ++settingsMenuChangedChannels;
                    }
                    if (settingsFovPixels[index + channel]
                        != settingsPixels[index + channel]) {
                        ++settingsSelectionChangedChannels;
                    }
                    if (workerPixels[index + channel] != centralHubPixels[index + channel]) {
                        ++workerChangedChannels;
                    }
                    if (guardPixels[index + channel] != workerPixels[index + channel]) {
                        ++guardChangedChannels;
                    }
                    if (officerPixels[index + channel] != guardPixels[index + channel]) {
                        ++officerChangedChannels;
                    }
                    if (engineerPixels[index + channel] != officerPixels[index + channel]) {
                        ++engineerChangedChannels;
                    }
                    if (industrialRobotPixels[index + channel]
                        != engineerPixels[index + channel]) {
                        ++industrialRobotChangedChannels;
                    }
                    if (pickupPixels[index + channel]
                        != industrialRobotPixels[index + channel]) {
                        ++pickupChangedChannels;
                    }
                    if (propPixels[index + channel] != pickupPixels[index + channel]) {
                        ++propChangedChannels;
                    }
                    if (pixels[index + channel] != propPixels[index + channel]) {
                        ++enemyChangedChannels;
                    }
                }
            }
            passed = renderedRoomVariant && repeatedRoom
                && roomPixels == repeatedRoomPixels && roomChangedChannels > 100U
                && renderedStorageVariant && themeChangedChannels > 100U
                && renderedCombatVariant && combatChangedChannels > 100U
                && renderedCafeteriaVariant && cafeteriaChangedChannels > 100U
                && renderedLaboratoryVariant && laboratoryChangedChannels > 100U
                && renderedTechnicalVariant && technicalChangedChannels > 100U
                && renderedSecurityVariant && securityChangedChannels > 100U
                && renderedCentralHubVariant && centralHubChangedChannels > 100U
                && renderedProceduralLighting && lightingChangedChannels > 100U
                && renderedAlertLighting && alertLightingChangedChannels > 100U
                && renderedTranslatedDoor && renderedLockedDoor && renderedMovingDoor
                && doorStateChangedChannels > 100U
                && doorMotionChangedChannels > 100U
                && renderedOpenDoor && renderedCorridor
                && corridorChangedChannels > 100U
                && renderedConnectedRoomMaterial
                && connectedRoomMaterialChangedChannels > 100U
                && renderedCornerCorridor && cornerCyanPixels > 20U
                && renderedHud && hudChangedChannels > 100U
                && renderedResultExit && resultSelectionChangedChannels > 100U
                && renderedFunctionalHud && functionalHudChangedChannels > 500U
                && renderedPauseMenu && pauseMenuChangedChannels > 500U
                && renderedMainMenu && mainMenuChangedChannels > 500U
                && renderedMainMenuExit && mainMenuSelectionChangedChannels > 100U
                && renderedSettings && settingsMenuChangedChannels > 500U
                && renderedSettingsFov
                && settingsSelectionChangedChannels > 100U
                && renderedWorker && workerChangedChannels > 100U
                && renderedGuard && guardChangedChannels > 100U
                && renderedOfficer && officerChangedChannels > 100U
                && renderedEngineer && engineerChangedChannels > 100U
                && renderedIndustrialRobot && industrialRobotChangedChannels > 100U
                && renderedPickups && pickupChangedChannels > 100U
                && renderedProps && propChangedChannels > 100U
                && renderedEnemies && brightest > darkest + 8U && enemyChangedChannels > 100U
                && glGetError() == GL_NO_ERROR;
            if (!passed) {
                std::cerr << "Framebuffer lacks expected scene or enemy contrast"
                          << " | corridor rendered=" << renderedCorridor
                          << " openDoor=" << renderedOpenDoor
                          << " changed=" << corridorChangedChannels
                          << " cornerCyan=" << cornerCyanPixels
                          << " doorState=" << doorStateChangedChannels << '\n';
            }
        }
    }

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    if (!passed) {
        return 1;
    }
    std::cout << "OpenGL framebuffer contains the fixed-room scene\n";
    return 0;
}
