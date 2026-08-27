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

            const bool renderedEnemies = renderer.prepareWorkerVisual(111)
                && renderer.prepareWorkerVisual(222)
                && renderer.prepareGuardVisual(333)
                && renderer.prepareOfficerVisual(444)
                && renderer.render(width, height, {}, enemies);
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
            std::size_t workerChangedChannels = 0;
            std::size_t guardChangedChannels = 0;
            std::size_t officerChangedChannels = 0;
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
                    if (workerPixels[index + channel] != centralHubPixels[index + channel]) {
                        ++workerChangedChannels;
                    }
                    if (guardPixels[index + channel] != workerPixels[index + channel]) {
                        ++guardChangedChannels;
                    }
                    if (officerPixels[index + channel] != guardPixels[index + channel]) {
                        ++officerChangedChannels;
                    }
                    if (pixels[index + channel] != officerPixels[index + channel]) {
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
                && renderedWorker && workerChangedChannels > 100U
                && renderedGuard && guardChangedChannels > 100U
                && renderedOfficer && officerChangedChannels > 100U
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
