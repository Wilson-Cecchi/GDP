#include "render/Renderer.hpp"

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
        gdp::render::Renderer renderer;
        constexpr std::array enemies{
            gdp::render::EnemyRenderData{{1.5F, 0.0F, -0.5F}, true, 111},
            gdp::render::EnemyRenderData{{-1.5F, 0.0F, -1.0F}, false, 222},
            gdp::render::EnemyRenderData{
                {0.0F, 0.0F, -2.0F}, true, 333, gdp::render::EnemyVisualRole::Guard},
            gdp::render::EnemyRenderData{
                {0.0F, 0.0F, 0.5F}, true, 444, gdp::render::EnemyVisualRole::Officer},
            gdp::render::EnemyRenderData{
                {-0.5F, 0.0F, 1.0F}, true, 555, gdp::render::EnemyVisualRole::Engineer},
            gdp::render::EnemyRenderData{
                {0.75F, 0.0F, 1.0F}, true, 666,
                gdp::render::EnemyVisualRole::IndustrialRobot},
        };
        constexpr std::array workerOnly{
            gdp::render::EnemyRenderData{{1.5F, 0.0F, -0.5F}, true, 111},
        };
        constexpr std::array guardOnly{
            gdp::render::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 333, gdp::render::EnemyVisualRole::Guard},
        };
        constexpr std::array officerOnly{
            gdp::render::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 444, gdp::render::EnemyVisualRole::Officer},
        };
        constexpr std::array engineerOnly{
            gdp::render::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 555, gdp::render::EnemyVisualRole::Engineer},
        };
        constexpr std::array industrialRobotOnly{
            gdp::render::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 666,
                gdp::render::EnemyVisualRole::IndustrialRobot},
        };
        constexpr std::array pickups{
            gdp::render::PickupRenderData{
                {-0.60F, 0.0F, -0.5F}, gdp::render::PickupVisualRole::Health},
            gdp::render::PickupRenderData{
                {0.0F, 0.0F, -0.5F}, gdp::render::PickupVisualRole::Ammo},
            gdp::render::PickupRenderData{
                {0.60F, 0.0F, -0.5F}, gdp::render::PickupVisualRole::Key},
        };
        constexpr std::array props{
            gdp::render::PropRenderData{
                {-1.35F, 0.0F, -1.2F}, {0.78F, 0.72F, 0.78F},
                gdp::render::PropVisualRole::Crate,
                gdp::render::PropVisualState::Intact, 7001},
            gdp::render::PropRenderData{
                {0.0F, 0.0F, -1.2F}, {0.46F, 1.02F, 0.46F},
                gdp::render::PropVisualRole::Canister,
                gdp::render::PropVisualState::Damaged, 7002},
            gdp::render::PropRenderData{
                {1.35F, 0.0F, -1.2F}, {1.16F, 0.92F, 0.34F},
                gdp::render::PropVisualRole::Barricade,
                gdp::render::PropVisualState::Destroyed, 7003},
        };
        gdp::render::RoomLightingRenderData proceduralLighting;
        proceduralLighting.ambientColor = {0.52F, 0.68F, 0.82F};
        proceduralLighting.ambientIntensity = 0.18F;
        proceduralLighting.lightCount = 3U;
        proceduralLighting.fixtureEmissive = 0.84F;
        proceduralLighting.lights[0] = {
            {-2.0F, 3.82F, -2.0F}, {0.48F, 0.74F, 1.0F}, 1.05F, 5.4F};
        proceduralLighting.lights[1] = {
            {2.0F, 3.82F, -2.0F}, {0.60F, 0.82F, 1.0F}, 0.95F, 5.8F};
        proceduralLighting.lights[2] = {
            {0.0F, 3.82F, 2.0F}, {0.42F, 0.68F, 0.94F}, 1.10F, 5.2F};
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
                std::cerr << "Framebuffer lacks expected scene or enemy contrast\n";
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
