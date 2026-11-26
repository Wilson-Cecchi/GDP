#include "game/visual/GdpRenderer.hpp"
#include "game/MenuFlow.hpp"
#include "game/visual/MenuBackdrop.hpp"
#include "gameplay/PlayerGrenade.hpp"
#include "world/RoomArchitecture.hpp"
#include "world/RoomFootprint.hpp"
#include "world/LightingGenerator.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "WeaponReloadValidation.hpp"
#include "PlayerHandsValidation.hpp"
#include "PlayerBodyValidation.hpp"

namespace {

// Hidden OpenGL room captures are technical pixel evidence; the played room
// still needs visual acceptance from the user.
enum class RoomPreviewKind {
    Storage, Cafeteria, CafeteriaLong, Laboratory, Technical, TechnicalL,
    Security, SecurityT, CentralHub, Boss, Combat, Industrial
};

bool validateTerminalVisual(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    constexpr int width = 1280;
    constexpr int height = 720;
    CameraData camera;
    camera.position = {0.0F, 1.35F, 2.2F};
    camera.forward = wkc::core::normalized(
        wkc::core::Vec3{0.0F, -0.45F, -2.2F});
    HudRenderData quality;
    quality.graphicsQuality = HudGraphicsQuality::Ultra;
    quality.detailedMaterials = true;
    const auto pixelsFor = [&](const std::span<const PropRenderData> props) {
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width * height * 4));
        if (!renderer.render(width, height, camera, {}, {}, props,
                {}, {}, {}, {}, {}, quality)) {
            return std::vector<std::uint8_t>{};
        }
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
            pixels.data());
        return pixels;
    };
    const auto empty = pixelsFor({});
    const std::array terminal{PropRenderData{
        {0.0F, 0.0F, 0.0F}, {1.15F, 1.8F, 0.6F},
        PropVisualRole::Terminal, PropVisualState::Intact,
        20261119U, gdp::world::RoomTheme::Laboratory}};
    auto visible = pixelsFor(terminal);
    if (empty.empty() || visible.size() != empty.size()) return false;
    if (const char* capturePath = std::getenv("GDP_TERMINAL_CAPTURE_PATH")) {
        const std::size_t stride = static_cast<std::size_t>(width * 4);
        std::vector<std::uint8_t> topDown(visible.size());
        for (int row = 0; row < height; ++row) {
            std::copy_n(visible.data()
                    + static_cast<std::size_t>(height - row - 1) * stride,
                stride, topDown.data() + static_cast<std::size_t>(row) * stride);
        }
        auto* surface = SDL_CreateSurfaceFrom(width, height,
            SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
        if (surface) SDL_SaveBMP(surface, capturePath);
        SDL_DestroySurface(surface);
    }
    std::size_t changedChannels = 0U;
    for (std::size_t index = 0U; index < empty.size(); ++index) {
        changedChannels += empty[index] != visible[index] ? 1U : 0U;
    }
    if (changedChannels < 1000U) {
        std::cerr << "Mission terminal is absent from the rendered frame (changed "
                  << changedChannels << " channels)\n";
        return false;
    }
    return true;
}

bool captureSupplyPickups(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* directory = std::getenv("GDP_SUPPLY_PICKUP_CAPTURE_DIR");
    if (!directory || !*directory) {
        std::cerr << "Set GDP_SUPPLY_PICKUP_CAPTURE_DIR for pickup captures\n";
        return false;
    }
    std::filesystem::create_directories(directory);
    constexpr int width = 1280;
    constexpr int height = 720;
    CameraData camera;
    camera.position = {0.0F, 0.72F, 1.20F};
    camera.forward = wkc::core::normalized(
        wkc::core::Vec3{0.0F, -0.57F, -1.20F});
    HudRenderData quality;
    quality.graphicsQuality = HudGraphicsQuality::Ultra;
    quality.detailedMaterials = true;
    for (const auto& [role, name] : std::array{
             std::pair{PickupVisualRole::Health, "health"},
             std::pair{PickupVisualRole::Ammo, "ammo"},
             std::pair{PickupVisualRole::Key, "key"},
             std::pair{PickupVisualRole::Armor, "armor"},
             std::pair{PickupVisualRole::Special, "special"}}) {
        const std::array pickup{PickupRenderData{
            {0.0F, 0.0F, 0.0F}, role, 0U}};
        if (!renderer.render(width, height, camera, {}, pickup,
                {}, {}, {}, {}, {}, {}, quality)) {
            return false;
        }
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width * height * 4));
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
            pixels.data());
        const auto stride = static_cast<std::size_t>(width * 4);
        std::vector<std::uint8_t> topDown(pixels.size());
        for (int y = 0; y < height; ++y) {
            std::copy_n(pixels.data()
                    + static_cast<std::size_t>(height - y - 1) * stride,
                stride, topDown.data() + static_cast<std::size_t>(y) * stride);
        }
        auto* surface = SDL_CreateSurfaceFrom(width, height,
            SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
        const auto path = std::filesystem::path{directory}
            / (std::string{name} + ".bmp");
        const bool saved = surface
            && SDL_SaveBMP(surface, path.string().c_str());
        SDL_DestroySurface(surface);
        if (!saved) return false;
    }
    return true;
}

bool captureSupplyCrates(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    using gdp::world::RoomTheme;
    const char* directory = std::getenv("GDP_SUPPLY_CRATE_CAPTURE_DIR");
    if (!directory || !*directory) {
        std::cerr << "Set GDP_SUPPLY_CRATE_CAPTURE_DIR for crate captures\n";
        return false;
    }
    std::filesystem::create_directories(directory);
    constexpr int width = 1280;
    constexpr int height = 720;
    CameraData camera;
    camera.position = {0.0F, 1.0F, 1.8F};
    camera.forward = wkc::core::normalized(
        wkc::core::Vec3{0.0F, -0.55F, -1.8F});
    HudRenderData quality;
    quality.graphicsQuality = HudGraphicsQuality::Ultra;
    quality.detailedMaterials = true;
    struct CrateCase {
        RoomTheme theme;
        PickupVisualRole drop;
        const char* name;
    };
    for (const auto& [theme, drop, name] : std::array{
             CrateCase{RoomTheme::Industrial, PickupVisualRole::Ammo, "industrial"},
             CrateCase{RoomTheme::Storage, PickupVisualRole::Ammo, "storage"},
             CrateCase{RoomTheme::Storage, PickupVisualRole::Ammo, "storage_angle"},
             CrateCase{RoomTheme::Storage, PickupVisualRole::Health, "storage_health"},
             CrateCase{RoomTheme::Office, PickupVisualRole::Ammo, "office"},
             CrateCase{RoomTheme::Laboratory, PickupVisualRole::Ammo, "laboratory"},
             CrateCase{RoomTheme::Maintenance, PickupVisualRole::Ammo, "maintenance"},
             CrateCase{RoomTheme::Security, PickupVisualRole::Ammo, "security"},
             CrateCase{RoomTheme::Cafeteria, PickupVisualRole::Ammo, "cafeteria"}}) {
        const std::array crate{PropRenderData{
            {0.0F, 0.0F, 0.0F}, {0.78F, 0.72F, 0.78F},
            PropVisualRole::Crate, PropVisualState::Intact, 20261114U,
            theme, drop}};
        CameraData caseCamera = camera;
        if (std::string_view{name} == "storage_angle") {
            caseCamera.position = {0.95F, 1.0F, 1.8F};
            caseCamera.forward = wkc::core::normalized(
                wkc::core::Vec3{-0.95F, -0.55F, -1.8F});
        }
        if (!renderer.render(width, height, caseCamera, {}, {}, crate,
                {}, {}, {}, {}, {}, quality)) {
            return false;
        }
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width * height * 4));
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
            pixels.data());
        const auto stride = static_cast<std::size_t>(width * 4);
        std::vector<std::uint8_t> topDown(pixels.size());
        for (int y = 0; y < height; ++y) {
            std::copy_n(pixels.data()
                    + static_cast<std::size_t>(height - y - 1) * stride,
                stride, topDown.data() + static_cast<std::size_t>(y) * stride);
        }
        auto* surface = SDL_CreateSurfaceFrom(width, height,
            SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
        const auto path = std::filesystem::path{directory}
            / (std::string{name} + ".bmp");
        const bool saved = surface
            && SDL_SaveBMP(surface, path.string().c_str());
        SDL_DestroySurface(surface);
        if (!saved) return false;
    }
    return true;
}

bool captureCanisters(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* directory = std::getenv("GDP_CANISTER_CAPTURE_DIR");
    if (!directory || !*directory) return false;
    std::filesystem::create_directories(directory);
    constexpr int width = 1280;
    constexpr int height = 720;
    struct CanisterCase {
        gdp::world::RoomTheme theme;
        PropVisualState state;
        bool angled;
        const char* name;
    };
    for (const auto& [theme, state, angled, name] : std::array{
             CanisterCase{gdp::world::RoomTheme::Industrial,
                 PropVisualState::Intact, false, "industrial_front"},
             CanisterCase{gdp::world::RoomTheme::Industrial,
                 PropVisualState::Intact, true, "industrial_angle"},
             CanisterCase{gdp::world::RoomTheme::Laboratory,
                 PropVisualState::Intact, false, "laboratory_front"},
             CanisterCase{gdp::world::RoomTheme::Industrial,
                 PropVisualState::Damaged, false, "industrial_damaged"}}) {
        CameraData camera;
        camera.position = angled
            ? wkc::core::Vec3{0.72F, 1.18F, 1.70F}
            : wkc::core::Vec3{0.0F, 1.18F, 1.70F};
        camera.forward = wkc::core::normalized(angled
            ? wkc::core::Vec3{-0.72F, -0.68F, -1.70F}
            : wkc::core::Vec3{0.0F, -0.68F, -1.70F});
        HudRenderData quality;
        quality.graphicsQuality = HudGraphicsQuality::Ultra;
        quality.detailedMaterials = true;
        const std::array canister{PropRenderData{
            {0.0F, 0.0F, 0.0F}, {0.46F, 1.02F, 0.46F},
            PropVisualRole::Canister, state, 20261115U, theme}};
        if (!renderer.render(width, height, camera, {}, {}, canister,
                {}, {}, {}, {}, {}, quality)) {
            return false;
        }
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width * height * 4));
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
            pixels.data());
        const auto stride = static_cast<std::size_t>(width * 4);
        std::vector<std::uint8_t> topDown(pixels.size());
        for (int y = 0; y < height; ++y) {
            std::copy_n(pixels.data()
                    + static_cast<std::size_t>(height - y - 1) * stride,
                stride, topDown.data() + static_cast<std::size_t>(y) * stride);
        }
        auto* surface = SDL_CreateSurfaceFrom(width, height,
            SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
        const auto path = std::filesystem::path{directory}
            / (std::string{name} + ".bmp");
        const bool saved = surface
            && SDL_SaveBMP(surface, path.string().c_str());
        SDL_DestroySurface(surface);
        if (!saved) return false;
    }
    return true;
}

bool captureBarricades(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    using gdp::world::RoomTheme;
    const char* directory = std::getenv("GDP_BARRICADE_CAPTURE_DIR");
    if (!directory || !*directory) return false;
    std::filesystem::create_directories(directory);
    constexpr int width = 1280;
    constexpr int height = 720;
    struct CoverCase {
        RoomTheme theme;
        PropVisualState state;
        bool rotated;
        const char* name;
    };
    for (const auto& [theme, state, rotated, name] : std::array{
             CoverCase{RoomTheme::Industrial, PropVisualState::Intact,
                 false, "industrial"},
             CoverCase{RoomTheme::Storage, PropVisualState::Intact,
                 false, "storage"},
             CoverCase{RoomTheme::Cafeteria, PropVisualState::Intact,
                 false, "cafeteria"},
             CoverCase{RoomTheme::Security, PropVisualState::Intact,
                 false, "security"},
             CoverCase{RoomTheme::Laboratory, PropVisualState::Intact,
                 false, "laboratory"},
             CoverCase{RoomTheme::Maintenance, PropVisualState::Intact,
                 false, "maintenance"},
             CoverCase{RoomTheme::Office, PropVisualState::Intact,
                 false, "office"},
             CoverCase{RoomTheme::Industrial, PropVisualState::Intact,
                 true, "industrial_rotated"},
             CoverCase{RoomTheme::Industrial, PropVisualState::Damaged,
                 false, "industrial_damaged"},
             CoverCase{RoomTheme::Industrial, PropVisualState::Destroyed,
                 false, "industrial_destroyed"}}) {
        CameraData camera;
        camera.position = rotated
            ? wkc::core::Vec3{1.9F, 1.03F, 0.65F}
            : wkc::core::Vec3{0.7F, 1.03F, 1.95F};
        camera.forward = wkc::core::normalized(
            wkc::core::Vec3{-camera.position.x,
                -0.58F, -camera.position.z});
        HudRenderData quality;
        quality.graphicsQuality = HudGraphicsQuality::Ultra;
        quality.detailedMaterials = true;
        const std::array cover{PropRenderData{
            {0.0F, 0.0F, 0.0F},
            rotated ? wkc::core::Vec3{0.34F, 0.92F, 1.16F}
                : wkc::core::Vec3{1.16F, 0.92F, 0.34F},
            PropVisualRole::Barricade, state, 20261115U, theme}};
        if (!renderer.render(width, height, camera, {}, {}, cover,
                {}, {}, {}, {}, {}, quality)) {
            return false;
        }
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width * height * 4));
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
            pixels.data());
        const auto stride = static_cast<std::size_t>(width * 4);
        std::vector<std::uint8_t> topDown(pixels.size());
        for (int y = 0; y < height; ++y) {
            std::copy_n(pixels.data()
                    + static_cast<std::size_t>(height - y - 1) * stride,
                stride, topDown.data() + static_cast<std::size_t>(y) * stride);
        }
        auto* surface = SDL_CreateSurfaceFrom(width, height,
            SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
        const auto path = std::filesystem::path{directory}
            / (std::string{name} + ".bmp");
        const bool saved = surface
            && SDL_SaveBMP(surface, path.string().c_str());
        SDL_DestroySurface(surface);
        if (!saved) return false;
    }
    return true;
}

bool captureRoomPreview(gdp::game::visual::GdpRenderer& renderer,
    const RoomPreviewKind kind, const bool blackout = false,
    const bool shadowPilot = false)
{
    using namespace gdp::game::visual;
    const bool cafeteriaLong = kind == RoomPreviewKind::CafeteriaLong;
    const bool cafeteria = kind == RoomPreviewKind::Cafeteria || cafeteriaLong;
    const bool laboratory = kind == RoomPreviewKind::Laboratory;
    const bool technicalL = kind == RoomPreviewKind::TechnicalL;
    const bool technical = kind == RoomPreviewKind::Technical || technicalL;
    const bool securityT = kind == RoomPreviewKind::SecurityT;
    const bool security = kind == RoomPreviewKind::Security || securityT;
    const bool securityCompact = security && !securityT
        && std::getenv("GDP_SECURITY_COMPACT_CAPTURE") != nullptr;
    const bool centralHub = kind == RoomPreviewKind::CentralHub;
    const bool boss = kind == RoomPreviewKind::Boss;
    const bool combat = kind == RoomPreviewKind::Combat;
    const bool industrial = kind == RoomPreviewKind::Industrial;
    const char* directory = shadowPilot
        ? std::getenv(centralHub
            ? "GDP_CENTRAL_HUB_SHADOW_CAPTURE_DIR"
            : "GDP_ROOM_SHADOW_CAPTURE_DIR") : blackout
        ? std::getenv("GDP_ROOM_BLACKOUT_CAPTURE_DIR")
        : std::getenv(industrial
            ? "GDP_INDUSTRIAL_ROOM_CAPTURE_DIR" : combat
            ? "GDP_COMBAT_ARENA_CAPTURE_DIR" : boss
            ? "GDP_BOSS_COLUMNS_CAPTURE_DIR" : cafeteriaLong
            ? "GDP_CAFETERIA_LONG_CAPTURE_DIR" : cafeteria
            ? "GDP_CAFETERIA_FURNITURE_CAPTURE_DIR" : centralHub
            ? "GDP_CENTRAL_HUB_CAPTURE_DIR" : securityT
            ? "GDP_SECURITY_T_CAPTURE_DIR" : security
            ? "GDP_SECURITY_CAPTURE_DIR" : technicalL
            ? "GDP_TECHNICAL_L_CAPTURE_DIR" : technical
            ? "GDP_TECHNICAL_CAPTURE_DIR" : laboratory
            ? "GDP_LABORATORY_CAPTURE_DIR" : "GDP_STORAGE_RACK_CAPTURE_DIR");
    if (!directory || !*directory) {
        std::cerr << "Set the matching room capture directory\n";
        return false;
    }
    std::filesystem::create_directories(directory);
    gdp::world::RoomInstance worldRoom;
    worldRoom.theme = boss || combat ? gdp::world::RoomTheme::Maintenance
        : cafeteria ? gdp::world::RoomTheme::Cafeteria
        : centralHub || industrial ? gdp::world::RoomTheme::Industrial
        : security ? gdp::world::RoomTheme::Security
        : technical ? gdp::world::RoomTheme::Maintenance
        : laboratory ? gdp::world::RoomTheme::Laboratory
            : gdp::world::RoomTheme::Storage;
    worldRoom.moduleId = securityCompact ? 601U : industrial ? 103U
        : combat ? gdp::world::combatRoomPreviewModuleId
        : boss ? gdp::world::bossRoomPreviewModuleId
        : cafeteriaLong ? gdp::world::cafeteriaLongPreviewModuleId
        : cafeteria ? gdp::world::cafeteriaPreviewModuleId
        : centralHub ? gdp::world::centralHubPreviewModuleId
        : securityT ? gdp::world::securityTPreviewModuleId
        : security ? gdp::world::securityPreviewModuleId
        : technicalL ? gdp::world::technicalLPreviewModuleId
        : technical ? gdp::world::technicalPreviewModuleId
        : laboratory ? gdp::world::laboratoryPreviewModuleId : 0U;
    worldRoom.worldBounds = {{securityCompact ? -3.5F : boss ? -13.85F
                : combat ? -10.0F
                : industrial ? -4.5F
                : centralHub || securityT ? -10.0F : technical ? -8.0F : -9.0F,
            0.0F, securityCompact ? -4.5F : boss ? -15.85F
                : combat ? -11.0F : industrial ? -5.0F
                : cafeteriaLong ? -12.0F : security && !securityT ? -9.0F : -10.0F},
        {securityCompact ? 3.5F : boss ? 13.85F : combat ? 10.0F
                : industrial ? 4.5F
                : centralHub || securityT ? 10.0F : technical ? 8.0F : 9.0F,
            securityCompact ? 4.0F : industrial ? 4.0F : boss ? 7.0F
                : technical ? 5.5F : 6.0F,
            securityCompact ? 4.5F : boss ? 15.85F : combat ? 11.0F
                : industrial ? 5.0F : cafeteriaLong ? 12.0F
                : security && !securityT ? 9.0F : 10.0F}};
    if (boss) worldRoom.role = gdp::world::RoomRole::Boss;
    if (combat) worldRoom.role = gdp::world::RoomRole::Arena;
    if (industrial) worldRoom.role = gdp::world::RoomRole::Start;
    worldRoom.seed = industrial ? 4U : 20261031U;
    if (securityT) {
        worldRoom.walkableOutline = gdp::world::securityTPreviewFootprint();
    } else if (technicalL) {
        worldRoom.walkableOutline = gdp::world::technicalLPreviewFootprint();
    } else if (centralHub) {
        worldRoom.walkableOutline = gdp::world::centralHubPreviewFootprint();
    }
    auto parts = gdp::world::makeRoomArchitecture(worldRoom);
    const auto columns = gdp::world::makeRoomWallColumns(worldRoom);
    parts.insert(parts.end(), columns.begin(), columns.end());
    if (parts.empty()) return false;
    std::vector<PropRenderData> breakableProps;
    for (const gdp::world::RoomArchitecturePart& part : parts) {
        if (!part.breakable) continue;
        const bool laboratoryPlanter = part.decoration
            == gdp::world::RoomDecorationPart::SmallGlassTestPlanter;
        const wkc::core::Vec3 partScale = part.bounds.maximum - part.bounds.minimum;
        const bool securityMonitor = part.securityFurniturePart
            == gdp::world::SecurityFurniturePart::Monitor;
        if (securityMonitor && partScale.y < 0.30F) continue;
        breakableProps.push_back({
            {(part.bounds.minimum.x + part.bounds.maximum.x) * 0.5F,
                part.bounds.minimum.y,
                (part.bounds.minimum.z + part.bounds.maximum.z) * 0.5F},
            laboratoryPlanter
                ? wkc::core::Vec3{partScale.x, 0.42F, partScale.z} : partScale,
            securityMonitor ? PropVisualRole::SecurityMonitor
                : laboratoryPlanter ? PropVisualRole::LaboratoryPlanter
                : part.technicalFurniturePart
                    == gdp::world::TechnicalFurniturePart::Instrument
                ? PropVisualRole::TableBox : PropVisualRole::StorageBox,
            PropVisualState::Intact,
            part.tint.x < 0.4F ? 1U : 0U,
            worldRoom.theme,
        });
    }
    if (boss) {
        breakableProps.push_back({{0.0F, 0.0F, 2.2F},
            {1.15F, 1.8F, 0.6F}, PropVisualRole::Terminal,
            PropVisualState::Intact, 20261119U,
            gdp::world::RoomTheme::Maintenance, {}, 0.0F,
            TerminalVisualState::Standby, true});
        breakableProps.push_back({{-8.5F, 0.0F, -8.5F},
            {2.4F, 1.35F, 0.6F}, PropVisualRole::Barricade,
            PropVisualState::Intact, 901U,
            gdp::world::RoomTheme::Maintenance, {}, 0.0F,
            TerminalVisualState::Standby, true});
        breakableProps.push_back({{-8.5F, 0.0F, 8.5F},
            {0.6F, 2.25F, 2.4F}, PropVisualRole::Barricade,
            PropVisualState::Intact, 902U,
            gdp::world::RoomTheme::Maintenance, {}, 0.0F,
            TerminalVisualState::Standby, true});
    }
    if (shadowPilot) {
        breakableProps.push_back({{4.5F, 0.0F, -1.5F},
            {1.8F, 1.25F, 0.55F}, PropVisualRole::Barricade,
            PropVisualState::Intact, 20261114U});
    }
    const std::array<EnemyRenderData, 1U> pilotEnemy{{
        {{-5.2F, 0.0F, -4.0F}, true, 20261113U,
            EnemyVisualRole::Worker, true, EnemyAnimationState::Walk, 36U}
    }};
    const std::span<const EnemyRenderData> pilotEnemies = shadowPilot
        ? std::span<const EnemyRenderData>{pilotEnemy}
        : std::span<const EnemyRenderData>{};
    renderer.setGeometryShadowsEnabled(shadowPilot || combat || boss);
    constexpr int width = 1280;
    constexpr int height = 720;
    RoomLightingRenderData lighting;
    lighting.ambientColor = boss || combat
        ? wkc::core::Vec3{0.77F, 0.81F, 0.78F}
        : cafeteria
        ? wkc::core::Vec3{0.89F, 0.83F, 0.75F}
        : centralHub
        ? wkc::core::Vec3{0.73F, 0.72F, 0.68F}
        : security
        ? wkc::core::Vec3{0.71F, 0.75F, 0.81F}
        : technical
        ? wkc::core::Vec3{0.78F, 0.77F, 0.74F}
        : laboratory ? wkc::core::Vec3{0.77F, 0.84F, 0.92F}
        : wkc::core::Vec3{0.80F, 0.77F, 0.70F};
    lighting.ambientIntensity = 0.34F;
    lighting.lightCount = 2U;
    lighting.lights[0] = {{3.5F, 5.1F, -5.0F},
        {1.0F, 0.86F, 0.70F}, 1.2F, 8.5F};
    lighting.lights[1] = {{-3.5F, 5.1F, -5.0F},
        {0.84F, 0.89F, 1.0F}, 1.0F, 8.5F};
    if (boss || industrial) {
        const auto plan = gdp::world::generateRoomLightingPlan(worldRoom.seed,
            worldRoom.role, worldRoom.theme, worldRoom.worldBounds);
        lighting.ambientColor = plan.ambientColor;
        lighting.ambientIntensity = plan.ambientIntensity;
        lighting.fixtureEmissive = plan.fixtureEmissive;
        lighting.lightCount = static_cast<std::uint32_t>(plan.lights.size());
        for (std::size_t index = 0; index < plan.lights.size(); ++index) {
            const auto& light = plan.lights[index];
            lighting.lights[index] = {light.position, light.color,
                light.intensity, light.range};
        }
    }
    if (centralHub) {
        // Match the four-corner coverage of the playable 20 m Hub rather
        // than validating its column with two artificial rear lights.
        lighting.ambientIntensity = 0.19F;
        lighting.lightCount = 5U;
        constexpr std::array positions{
            wkc::core::Vec3{-5.0F, 5.52F, -5.0F},
            wkc::core::Vec3{5.0F, 5.52F, 5.0F},
            wkc::core::Vec3{5.0F, 5.52F, -5.0F},
            wkc::core::Vec3{-5.0F, 5.52F, 5.0F},
            wkc::core::Vec3{5.0F, 5.52F, 0.0F},
        };
        for (std::size_t index = 0U; index < positions.size(); ++index) {
            lighting.lights[index] = {positions[index],
                {1.0F, 0.87F, 0.74F}, index == 4U ? 0.55F : 1.05F,
                index == 4U ? 6.5F : 9.0F};
        }
    }
    if (combat) {
        // Match the playable Arena's ambient and five suspended work lights.
        const auto plan = gdp::world::generateRoomLightingPlan(worldRoom.seed,
            gdp::world::RoomRole::Arena,
            gdp::world::RoomTheme::Maintenance, worldRoom.worldBounds);
        lighting.ambientColor = plan.ambientColor;
        lighting.ambientIntensity = 0.22F;
        lighting.fixtureEmissive = plan.fixtureEmissive;
        lighting.lightCount = 5U;
        constexpr std::array positions{
            wkc::core::Vec3{-4.7F, 5.18F, -5.0F},
            wkc::core::Vec3{4.7F, 5.18F, -5.0F},
            wkc::core::Vec3{-4.7F, 5.18F, 5.0F},
            wkc::core::Vec3{4.7F, 5.18F, 5.0F},
            wkc::core::Vec3{0.0F, 5.18F, 1.2F},
        };
        for (std::size_t index = 0U; index < positions.size(); ++index) {
            lighting.lights[index] = {positions[index],
                gdp::world::combatGalleryFixtureColor,
                index == 4U ? 0.80F : 1.42F,
                index == 4U ? 7.8F : 10.5F};
        }
    }
    if (securityT) {
        lighting.lightCount = 3U;
        lighting.lights[0].position = {0.0F, 5.56F, -6.0F};
        lighting.lights[1].position = {-5.5F, 5.56F, 4.0F};
        lighting.lights[2] = {{5.5F, 5.56F, 4.0F},
            {0.88F, 0.93F, 1.0F}, 1.0F, 8.5F};
    }
    if (technicalL) lighting.lights[0].position = {0.0F, 5.1F, -5.0F};
    const CeilingFixtureVariant fixtureVariant = industrial
        ? CeilingFixtureVariant::Industrial : boss || combat
        ? CeilingFixtureVariant::BossArena : cafeteria
        ? CeilingFixtureVariant::Cafeteria : centralHub
        ? CeilingFixtureVariant::CentralHub : security
        ? CeilingFixtureVariant::Security : technical
        ? CeilingFixtureVariant::Maintenance : laboratory
        ? CeilingFixtureVariant::Laboratory : CeilingFixtureVariant::Storage;
    for (std::size_t index = 0; index < lighting.lightCount; ++index) {
        lighting.lights[index].fixtureVariant = fixtureVariant;
        lighting.lights[index].fixtureCondition = index == 0U
            ? CeilingFixtureCondition::Used
            : CeilingFixtureCondition::Worn;
        lighting.lights[index].sourceRoomBounds = worldRoom.worldBounds;
        lighting.lights[index].boundedToRoom = true;
    }
    bool passed = true;
    for (const bool potato : {false, true}) {
        for (const float fov : {60.0F, 100.0F}) {
            const wkc::core::Seed seed = industrial ? 4U : combat ? 20261028U
                : centralHub ? 20261030U
                : securityT ? 20261102U : 20261029U;
            passed &= (industrial
                ? renderer.prepareIndustrialRoomMaterials(seed)
                : combat
                ? renderer.prepareCombatRoomMaterials(seed, true)
                : boss ? renderer.prepareCombatRoomMaterials(seed, false, true)
                : cafeteria
                ? renderer.prepareCafeteriaRoomMaterials(seed)
                : centralHub
                ? renderer.prepareCentralHubRoomMaterials(seed)
                : security
                ? renderer.prepareSecurityRoomMaterials(seed)
                : technical
                ? renderer.prepareTechnicalRoomMaterials(seed)
                : laboratory ? renderer.prepareLaboratoryRoomMaterials(seed, true)
                : renderer.prepareStorageRoomMaterials(seed, true));
            RoomRenderData room;
            room.worldBounds = worldRoom.worldBounds;
            room.walkableOutline = worldRoom.walkableOutline;
            room.materialKey = renderer.activeRoomMaterialKey();
            room.architectureParts = parts;
            room.customArchitecture = true;
            std::array<float, 3> hubShaftBrightness{};
            std::vector<std::uint8_t> laboratoryMarkedWallPixels;
            for (int variant = 0; variant < (
                    blackout ? 3 : securityCompact ? 1
                    : centralHub ? (shadowPilot ? 10 : 8)
                    : securityT ? 16 : cafeteriaLong ? 7
                    : cafeteria ? 5 : laboratory ? 9
                    : security ? 4 : combat ? 4
                    : kind == RoomPreviewKind::Storage || technical || industrial ? 3 : 2);
                 ++variant) {
            const bool fixtureDetail = !blackout && variant == 1;
            const bool industrialSteelDetail = industrial && variant == 2;
            const bool technicalTableBoxDetail = technical && variant == 2;
            const bool laboratoryBenchDetail = laboratory && variant == 2;
            const bool laboratoryContainmentDetail = laboratory && variant == 3;
            const bool laboratoryDomeDetail = laboratory && variant == 4;
            const bool laboratoryDuctDetail = laboratory && variant == 5;
            const bool laboratoryTabletopDetail = laboratory && variant == 6;
            const bool laboratoryBulletMarkDetail = laboratory && variant == 7;
            const bool laboratoryBulletMarkBaseline = laboratory && variant == 8;
            const bool floorDetail = securityT && variant == 2;
            const bool floorOverview = securityT && variant == 3;
            const bool planterDetail = securityT && variant == 4;
            const bool ductDetail = securityT && variant == 5;
            const bool trimCornerDetail = securityT && variant == 6;
            const bool securityConsoleDetail = securityT && variant == 7;
            const bool securityCabinetDetail = securityT && variant == 8;
            const bool securityCoverDetail = securityT && variant == 9;
            const bool securityCoverCornerDetail = securityT && variant == 10;
            const bool bulletMarkDetail = securityT && variant == 11;
            const bool securityIndicatorDetail = securityT && variant >= 12;
            const bool observationDetail = security && !securityT
                && variant == 2;
            const bool securityDuctDetail = security && !securityT
                && variant == 3;
            const bool counterDetail = cafeteria && !blackout && variant == 2;
            const bool counterTopDetail = cafeteria && !blackout && variant == 3;
            const bool counterGrazingDetail = cafeteria && !blackout && variant == 4;
            const bool storageDuctDetail = kind == RoomPreviewKind::Storage
                && !blackout && variant == 2;
            const bool hubHatchDetail = centralHub && !blackout && variant == 2;
            const bool hubVentDetail = centralHub && !blackout && variant == 3;
            const bool hubCeilingDetail = centralHub && !blackout && variant == 4;
            const bool hubDuctDetail = centralHub && !blackout && variant == 5;
            const bool hubWallPanelDetail = centralHub && !blackout && variant == 6;
            const bool hubWallVentDetail = centralHub && !blackout && variant == 7;
            const bool cafeteriaPlantDetail = cafeteriaLong && variant == 5;
            const bool cafeteriaDuctDetail = cafeteriaLong && variant == 6;
            const bool combatCeilingDetail = combat && variant == 1;
            const bool combatStairDetail = combat && variant == 2;
            const bool combatCoverDetail = combat && variant == 3;
            CameraData camera;
            camera.position = industrial
                ? wkc::core::Vec3{0.0F, 1.62F, -2.0F}
                : combat
                ? wkc::core::Vec3{0.0F, 1.62F, 8.4F}
                : boss
                ? wkc::core::Vec3{1.4F, 1.62F, 13.2F}
                : cafeteria
                ? wkc::core::Vec3{2.8F, 1.62F, cafeteriaLong ? 10.2F : 8.2F}
                : centralHub
                ? wkc::core::Vec3{2.6F, 1.62F, 7.6F}
                : securityT
                ? wkc::core::Vec3{0.0F, 1.62F, -8.0F}
                : security
                ? wkc::core::Vec3{-0.5F, 1.62F, 6.6F}
                : technical
                ? wkc::core::Vec3{1.0F, 1.62F, 8.2F}
                : laboratory ? wkc::core::Vec3{0.4F, 1.62F, 8.0F}
                : wkc::core::Vec3{4.7F, 1.62F, -0.8F};
            camera.forward = industrial
                ? wkc::core::Vec3{0.0F, -0.02F, 1.0F}
                : combat
                ? wkc::core::Vec3{0.1F, -0.03F, -1.0F}
                : boss
                ? wkc::core::Vec3{0.28F, -0.02F, -1.0F}
                : cafeteria
                ? wkc::core::Vec3{-0.18F, -0.025F, -1.0F}
                : centralHub
                ? wkc::core::Vec3{-0.25F, -0.025F, -1.0F}
                : securityT
                ? wkc::core::Vec3{0.0F, 0.08F, 1.0F}
                : security
                ? wkc::core::Vec3{-0.12F, -0.025F, -1.0F}
                : technical
                ? wkc::core::Vec3{0.0F, -0.04F, -1.0F}
                : laboratory ? wkc::core::Vec3{0.2F, -0.02F, -1.0F}
                : wkc::core::Vec3{0.65F, -0.02F, -0.76F};
            if (securityCompact) {
                camera.position = {1.0F, 1.62F, 2.9F};
                camera.forward = {-1.0F, -0.07F, 0.0F};
            }
            if (shadowPilot && centralHub && variant == 8) {
                camera.position = {-7.4F, 1.62F, 5.8F};
                camera.forward = {1.0F, -0.02F, -0.75F};
            }
            if (shadowPilot && centralHub && variant == 9) {
                camera.position = {4.7F, 1.55F, 2.6F};
                camera.forward = {-0.05F, -0.16F, -1.0F};
            }
            if (fixtureDetail) {
                camera.position = cafeteria
                    ? wkc::core::Vec3{3.0F, 1.55F, 6.45F}
                    : wkc::core::Vec3{3.5F, 3.0F, -3.0F};
                camera.forward = cafeteria
                    ? wkc::core::Vec3{0.0F, -0.29F, -1.0F}
                    : wkc::core::Vec3{0.0F, 0.72F, -0.70F};
                if (securityT) {
                    camera.position = {0.0F, 2.4F, -8.5F};
                    camera.forward = {0.0F, 0.78F, 0.62F};
                }
            }
            if (industrialSteelDetail) {
                camera.position = {2.65F, 1.64F, 0.35F};
                camera.forward = wkc::core::normalized(
                    wkc::core::Vec3{1.0F, -0.10F, 0.42F});
            }
            if (technicalTableBoxDetail) {
                camera.position = {1.1F, 1.72F, 5.75F};
                camera.forward = wkc::core::normalized(
                    wkc::core::Vec3{1.9F, -0.55F, -1.95F});
            }
            if (floorDetail) {
                camera.position = {0.0F, 1.7F, -3.5F};
                camera.forward = {0.0F, -0.78F, 0.63F};
            }
            if (floorOverview) {
                camera.position = {0.0F, 5.15F, 1.5F};
                camera.forward = {0.0F, -1.0F, 0.1F};
            }
            if (planterDetail) {
                camera.position = {4.7F, 1.6F, -0.5F};
                camera.forward = {1.0F, -0.04F, -0.10F};
            }
            if (ductDetail) {
                camera.position = {1.0F, 2.2F, -4.0F};
                camera.forward = {0.25F, 0.65F, 1.0F};
            }
            if (trimCornerDetail) {
                camera.position = {6.4F, 1.45F, 0.5F};
                camera.forward = {-0.77F, -0.30F, -0.62F};
            }
            if (securityConsoleDetail || securityIndicatorDetail) {
                camera.position = {5.6F, 1.65F, 4.4F};
                camera.forward = {0.0F, -0.02F, 1.0F};
            }
            if (observationDetail) {
                camera.position = {4.6F, 1.75F, 4.1F};
                camera.forward = {1.0F, -0.025F, 0.0F};
            }
            if (securityDuctDetail) {
                camera.position = {0.0F, 1.7F, 4.0F};
                camera.forward = {0.34F, 0.46F, -1.0F};
            }
            if (securityCabinetDetail) {
                camera.position = {0.0F, 1.60F, -6.8F};
                camera.forward = {0.95F, -0.04F, 0.28F};
            }
            if (securityCoverDetail) {
                camera.position = {6.3F, 1.60F, -0.8F};
                camera.forward = {0.0F, -0.10F, 1.0F};
            }
            if (securityCoverCornerDetail) {
                camera.position = {7.8F, 1.55F, 0.8F};
                camera.forward = {-0.68F, -0.25F, 0.72F};
            }
            if (laboratoryBenchDetail) {
                camera.position = {2.7F, 1.65F, 2.3F};
                camera.forward = {1.0F, -0.27F, 0.90F};
            }
            if (laboratoryContainmentDetail) {
                camera.position = {5.4F, 1.65F, 6.7F};
                camera.forward = {1.0F, -0.03F, 0.18F};
            }
            if (laboratoryDomeDetail) {
                camera.position = {-6.4F, 1.36F, 9.05F};
                camera.forward = {-1.0F, -0.27F, -0.08F};
            }
            if (laboratoryDuctDetail) {
                camera.position = {-3.1F, 1.75F, 5.3F};
                camera.forward = {-0.30F, 0.78F, -1.0F};
            }
            if (laboratoryTabletopDetail) {
                camera.position = {2.65F, 1.62F, 3.9F};
                camera.forward = {1.0F, -0.24F, 0.0F};
            }
            if (laboratoryBulletMarkDetail || laboratoryBulletMarkBaseline) {
                camera.position = {0.0F, 1.62F, -7.0F};
                camera.forward = {0.0F, 0.0F, -1.0F};
            }
            if (bulletMarkDetail) {
                camera.position = {-7.0F, 1.62F, 4.0F};
                camera.forward = {-1.0F, 0.0F, 0.0F};
            }
            if (counterDetail) {
                camera.position = {2.4F, 1.65F, 2.4F};
                camera.forward = {0.9F, -0.12F, -0.4F};
            }
            if (counterTopDetail) {
                camera.position = {7.0F, 3.15F, 4.0F};
                camera.forward = {0.63F, -0.76F, -0.18F};
            }
            if (counterGrazingDetail) {
                camera.position = {6.7F, 2.75F, 7.2F};
                camera.forward = {0.36F, -0.64F, -0.8F};
            }
            if (storageDuctDetail) {
                camera.position = {1.8F, 1.7F, 5.0F};
                camera.forward = {-0.18F, 0.60F, -1.0F};
            }
            if (hubHatchDetail) {
                camera.position = {-2.5F, 1.7F, 6.3F};
                camera.forward = {-0.65F, -0.55F, -0.8F};
            }
            if (hubVentDetail) {
                camera.position = {6.0F, 1.7F, -1.6F};
                camera.forward = {-0.55F, -0.60F, -0.8F};
            }
            if (hubCeilingDetail) {
                camera.position = {2.6F, 2.1F, 5.5F};
                camera.forward = {0.10F, 1.0F, -0.45F};
            }
            if (hubDuctDetail) {
                camera.position = {2.6F, 2.1F, 5.5F};
                camera.forward = {0.35F, 0.30F, -1.0F};
            }
            if (hubWallPanelDetail) {
                camera.position = {4.2F, 1.62F, -4.2F};
                camera.forward = {1.0F, 0.12F, -1.0F};
            }
            if (hubWallVentDetail) {
                camera.position = {4.2F, 1.62F, 4.2F};
                camera.forward = {1.0F, 0.12F, 1.0F};
            }
            if (cafeteriaPlantDetail) {
                camera.position = {-5.9F, 1.55F, 8.6F};
                camera.forward = {-0.72F, -0.22F, 1.0F};
            }
            if (cafeteriaDuctDetail) {
                camera.position = {5.6F, 1.8F, 6.0F};
                camera.forward = {0.28F, 0.70F, -1.0F};
            }
            if (combatCeilingDetail) {
                camera.position = {1.2F, 1.7F, 1.0F};
                camera.forward = {0.28F, 0.68F, -1.0F};
            }
            if (combatStairDetail) {
                camera.position = {2.7F, 1.62F, 5.7F};
                camera.forward = {1.0F, 0.08F, 0.05F};
            }
            if (combatCoverDetail) {
                camera.position = {1.0F, 1.62F, 1.4F};
                camera.forward = {0.78F, -0.12F, -0.65F};
            }
            camera.verticalFieldOfViewDegrees = fov;
            HudRenderData hud;
            hud.graphicsQuality = potato
                ? HudGraphicsQuality::Potato : HudGraphicsQuality::Ultra;
            hud.geometryShadowLightBudget = (shadowPilot || combat || boss) && !potato
                ? 8U : 0U;
            hud.detailedMaterials = !potato;
            hud.worldRenderScalePercent = potato ? 50U : 100U;
            RoomLightingRenderData frameLighting = lighting;
            if (industrial) {
                frameLighting.flashlightEnabled = std::getenv(
                    "GDP_INDUSTRIAL_FLASHLIGHT") != nullptr;
                frameLighting.flashlightPosition = camera.position;
                frameLighting.flashlightDirection = camera.forward;
            }
            if (centralHub) {
                const auto shaft = std::find_if(parts.begin(), parts.end(),
                    [](const gdp::world::RoomArchitecturePart& part) {
                        return part.decoration
                            == gdp::world::RoomDecorationPart::CentralHubColumnShaft;
                    });
                if (shaft == parts.end()) return false;
                const auto plinth = std::find_if(parts.begin(), parts.end(),
                    [](const gdp::world::RoomArchitecturePart& part) {
                        return part.decoration
                            == gdp::world::RoomDecorationPart::CentralHubColumnPlinth;
                    });
                if (plinth == parts.end()) return false;
                const std::array shadowCandidates{
                    ShadowOccluderRenderData{shaft->bounds, true, true},
                    ShadowOccluderRenderData{plinth->bounds, false, false}};
                (void)optimizeShadowBudget(
                    frameLighting, camera.position, shadowCandidates);
            }
            if (securityIndicatorDetail) {
                room.securityIndicatorPhase = variant <= 13
                    ? SecurityIndicatorPhase::Alarm
                    : SecurityIndicatorPhase::Recovery;
                room.securityIndicatorSeconds = variant == 13 ? 1.1F
                    : variant == 15 ? 3.1F : 0.0F;
                frameLighting.alert = variant <= 13;
            }
            if (blackout && variant > 0) {
                for (std::size_t index = 0U; index < frameLighting.lightCount; ++index) {
                    frameLighting.lights[index].intensity = 0.0F;
                    frameLighting.lights[index].fixtureState
                        = PropVisualState::Destroyed;
                    frameLighting.lights[index].fixtureEmissionScale = 0.0F;
                }
                frameLighting.ambientIntensity = fixtureDrivenAmbientIntensity(
                    frameLighting, lighting.ambientIntensity);
                frameLighting.flashlightEnabled = variant == 2;
                frameLighting.flashlightPosition = camera.position;
                frameLighting.flashlightDirection = camera.forward;
            }
            constexpr std::array bulletMarks{
                ImpactRenderData{{-10.0F, 1.62F, 3.7F}, {1.0F, 0.0F, 0.0F},
                    ImpactVisualMaterial::Metal, 1.0F, 101U,
                    ImpactVisualKind::BulletMark},
                ImpactRenderData{{-10.0F, 1.69F, 4.0F}, {1.0F, 0.0F, 0.0F},
                    ImpactVisualMaterial::Metal, 1.0F, 102U,
                    ImpactVisualKind::BulletMark},
                ImpactRenderData{{-10.0F, 1.57F, 4.3F}, {1.0F, 0.0F, 0.0F},
                    ImpactVisualMaterial::Metal, 1.0F, 103U,
                    ImpactVisualKind::BulletMark},
            };
            constexpr std::array laboratoryBulletMarks{
                ImpactRenderData{{0.0F, 1.62F, -10.0F}, {0.0F, 0.0F, 1.0F},
                    ImpactVisualMaterial::Metal, 1.0F, 104U,
                    ImpactVisualKind::BulletMark},
            };
            const std::span<const ImpactRenderData> marks = bulletMarkDetail
                ? std::span<const ImpactRenderData>{bulletMarks}
                : laboratoryBulletMarkDetail
                    ? std::span<const ImpactRenderData>{laboratoryBulletMarks}
                : std::span<const ImpactRenderData>{};
            if (shadowPilot && !potato) {
                passed &= renderer.render(width, height, camera, pilotEnemies, {},
                    breakableProps, frameLighting, room, {}, {}, {}, hud, marks);
            }
            passed &= renderer.render(width, height, camera, pilotEnemies, {},
                breakableProps, frameLighting, room, {}, {}, {}, hud, marks);
            if (shadowPilot && !potato) {
                passed &= renderer.lastFrameStats().geometryShadowMaps
                    == frameLighting.lightCount;
            }
            glFinish();
            std::vector<std::uint8_t> pixels(
                static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                pixels.data());
            passed &= glGetError() == GL_NO_ERROR;
            if (shadowPilot && !potato && fov == 60.0F && variant == 0) {
                renderer.setGeometryShadowsEnabled(false);
                passed &= renderer.render(width, height, camera, pilotEnemies, {},
                    breakableProps, frameLighting, room, {}, {}, {}, hud, marks);
                glFinish();
                std::vector<std::uint8_t> legacyPixels(pixels.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    legacyPixels.data());
                std::size_t changedPixels = 0U;
                for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4U) {
                    const int difference = std::abs(int(pixels[pixel])
                            - int(legacyPixels[pixel]))
                        + std::abs(int(pixels[pixel + 1U])
                            - int(legacyPixels[pixel + 1U]))
                        + std::abs(int(pixels[pixel + 2U])
                            - int(legacyPixels[pixel + 2U]));
                    if (difference > 30) ++changedPixels;
                }
                std::cout << "Geometry shadows versus legacy: "
                    << changedPixels << " changed pixels\n";
                passed &= changedPixels > 500U && glGetError() == GL_NO_ERROR;
                renderer.setGeometryShadowsEnabled(true);
            }
            if (shadowPilot && centralHub && !potato && fov == 60.0F
                && variant == 8) {
                hud.geometryShadowLightBudget = 1U;
                passed &= renderer.render(width, height, camera, pilotEnemies, {},
                    breakableProps, frameLighting, room, {}, {}, {}, hud, marks);
                passed &= renderer.render(width, height, camera, pilotEnemies, {},
                    breakableProps, frameLighting, room, {}, {}, {}, hud, marks);
                glFinish();
                std::vector<std::uint8_t> oneLightPixels(pixels.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    oneLightPixels.data());
                std::vector<std::uint8_t> oneLightTopDown(pixels.size());
                const std::size_t rowBytes = static_cast<std::size_t>(width * 4);
                for (int row = 0; row < height; ++row) {
                    std::copy_n(oneLightPixels.data()
                            + static_cast<std::size_t>(height - row - 1) * rowBytes,
                        rowBytes,
                        oneLightTopDown.data()
                            + static_cast<std::size_t>(row) * rowBytes);
                }
                const auto oneLightPath = std::filesystem::path{directory}
                    / "shadow_one_light_opposite_fov60.bmp";
                SDL_Surface* oneLightSurface = SDL_CreateSurfaceFrom(
                    width, height, SDL_PIXELFORMAT_RGBA32,
                    oneLightTopDown.data(), width * 4);
                passed &= oneLightSurface
                    && SDL_SaveBMP(oneLightSurface,
                        oneLightPath.string().c_str());
                if (oneLightSurface) SDL_DestroySurface(oneLightSurface);
                std::size_t changedPixels = 0U;
                for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4U) {
                    const int difference = std::abs(int(pixels[pixel])
                            - int(oneLightPixels[pixel]))
                        + std::abs(int(pixels[pixel + 1U])
                            - int(oneLightPixels[pixel + 1U]))
                        + std::abs(int(pixels[pixel + 2U])
                            - int(oneLightPixels[pixel + 2U]));
                    if (difference > 30) ++changedPixels;
                }
                std::cout << "Opposite-side all-lights versus one-light: "
                    << changedPixels << " changed pixels\n";
                passed &= changedPixels > 500U && glGetError() == GL_NO_ERROR;
            }
            if (shadowPilot && centralHub && !potato && fov == 60.0F
                && variant == 9) {
                renderer.setGeometryShadowsEnabled(false);
                RoomLightingRenderData unshadowed = frameLighting;
                unshadowed.shadowLightCount = 0U;
                unshadowed.shadowOccluderCount = 0U;
                passed &= renderer.render(width, height, camera, pilotEnemies,
                    {}, breakableProps, unshadowed, room, {}, {}, {}, hud,
                    marks);
                glFinish();
                std::vector<std::uint8_t> unshadowedPixels(pixels.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    unshadowedPixels.data());
                std::vector<std::uint8_t> unshadowedTopDown(pixels.size());
                const std::size_t unshadowedRowBytes
                    = static_cast<std::size_t>(width * 4);
                for (int row = 0; row < height; ++row) {
                    std::copy_n(unshadowedPixels.data()
                            + static_cast<std::size_t>(height - row - 1)
                                * unshadowedRowBytes,
                        unshadowedRowBytes, unshadowedTopDown.data()
                            + static_cast<std::size_t>(row)
                                * unshadowedRowBytes);
                }
                const auto unshadowedPath = std::filesystem::path{directory}
                    / "shadow_box_disabled_fov60.bmp";
                SDL_Surface* unshadowedSurface = SDL_CreateSurfaceFrom(
                    width, height, SDL_PIXELFORMAT_RGBA32,
                    unshadowedTopDown.data(), width * 4);
                passed &= unshadowedSurface && SDL_SaveBMP(unshadowedSurface,
                    unshadowedPath.string().c_str());
                if (unshadowedSurface) SDL_DestroySurface(unshadowedSurface);
                renderer.setGeometryShadowsEnabled(true);
                passed &= renderer.render(width, height, camera, pilotEnemies,
                    {}, breakableProps, frameLighting, room, {}, {}, {}, hud,
                    marks);
                passed &= renderer.render(width, height, camera, pilotEnemies,
                    {}, breakableProps, frameLighting, room, {}, {}, {}, hud,
                    marks);
                std::uint64_t previousBoxBrightness = 0U;
                std::vector<std::uint8_t> previousPixels = pixels;
                for (const std::uint32_t remaining : {4U, 2U, 1U, 0U}) {
                    RoomLightingRenderData fewerLights = frameLighting;
                    for (std::uint32_t index = remaining;
                         index < fewerLights.lightCount; ++index) {
                        fewerLights.lights[index].intensity = 0.0F;
                        fewerLights.lights[index].fixtureEmissionScale = 0.0F;
                        fewerLights.lights[index].fixtureState
                            = PropVisualState::Destroyed;
                    }
                    fewerLights.ambientIntensity = fixtureDrivenAmbientIntensity(
                        fewerLights, lighting.ambientIntensity);
                    passed &= renderer.render(width, height, camera, pilotEnemies,
                        {}, breakableProps, fewerLights, room, {}, {}, {}, hud,
                        marks);
                    passed &= renderer.render(width, height, camera, pilotEnemies,
                        {}, breakableProps, fewerLights, room, {}, {}, {}, hud,
                        marks);
                    passed &= renderer.lastFrameStats().geometryShadowMaps
                        == remaining;
                    glFinish();
                    std::vector<std::uint8_t> fewerPixels(pixels.size());
                    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                        fewerPixels.data());
                    std::uint64_t boxBrightness = 0U;
                    std::size_t changedPixels = 0U;
                    for (int y = 220; y < 415; ++y) {
                        for (int x = 500; x < 780; ++x) {
                            const std::size_t offset
                                = static_cast<std::size_t>((y * width + x) * 4);
                            for (int channel = 0; channel < 3; ++channel) {
                                boxBrightness += fewerPixels[offset + channel];
                            }
                        }
                    }
                    for (std::size_t offset = 0; offset < pixels.size();
                         offset += 4U) {
                        const int delta = std::abs(int(previousPixels[offset])
                                - int(fewerPixels[offset]))
                            + std::abs(int(previousPixels[offset + 1U])
                                - int(fewerPixels[offset + 1U]))
                            + std::abs(int(previousPixels[offset + 2U])
                                - int(fewerPixels[offset + 2U]));
                        if (delta > 30) ++changedPixels;
                    }
                    if (remaining == 4U) {
                        for (int y = 220; y < 415; ++y) {
                            for (int x = 500; x < 780; ++x) {
                                const std::size_t offset = static_cast<std::size_t>(
                                    (y * width + x) * 4);
                                for (int channel = 0; channel < 3; ++channel) {
                                    previousBoxBrightness += pixels[offset + channel];
                                }
                            }
                        }
                    }
                    std::cout << "Hub box with " << remaining
                        << " lights: box brightness " << boxBrightness
                        << ", changed pixels " << changedPixels << '\n';
                    passed &= previousBoxBrightness > boxBrightness
                        && changedPixels > 500U && glGetError() == GL_NO_ERROR;
                    previousBoxBrightness = boxBrightness;
                    previousPixels = fewerPixels;
                    std::vector<std::uint8_t> topDown(fewerPixels.size());
                    const std::size_t rowBytes
                        = static_cast<std::size_t>(width * 4);
                    for (int row = 0; row < height; ++row) {
                        std::copy_n(fewerPixels.data()
                                + static_cast<std::size_t>(height - row - 1)
                                    * rowBytes,
                            rowBytes, topDown.data()
                                + static_cast<std::size_t>(row) * rowBytes);
                    }
                    const auto path = std::filesystem::path{directory}
                        / ("shadow_box_" + std::to_string(remaining)
                            + "_lights_fov60.bmp");
                    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height,
                        SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                    passed &= surface
                        && SDL_SaveBMP(surface, path.string().c_str());
                    if (surface) SDL_DestroySurface(surface);
                }
            }
            if (centralHub && !shadowPilot && !blackout && !potato && fov == 60.0F
                && variant == 0) {
                RoomLightingRenderData withoutShadows = frameLighting;
                withoutShadows.shadowLightCount = 0U;
                withoutShadows.shadowOccluderCount = 0U;
                passed &= renderer.render(width, height, camera, {}, {},
                    breakableProps, withoutShadows, room, {}, {}, {}, hud, marks);
                glFinish();
                std::vector<std::uint8_t> withoutShadowPixels(pixels.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    withoutShadowPixels.data());
                std::size_t shadowedFloorPixels = 0U;
                for (int y = 0; y < height / 2; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const std::size_t pixel
                            = static_cast<std::size_t>((y * width + x) * 4);
                        const int lit = withoutShadowPixels[pixel]
                            + withoutShadowPixels[pixel + 1U]
                            + withoutShadowPixels[pixel + 2U];
                        const int shadowed = pixels[pixel] + pixels[pixel + 1U]
                            + pixels[pixel + 2U];
                        if (lit > shadowed + 30) ++shadowedFloorPixels;
                    }
                }
                if (shadowedFloorPixels <= 500U) {
                    std::cerr << "Central Hub column casts no visible floor shadow: "
                        << shadowedFloorPixels << " pixels\n";
                }
                passed &= shadowedFloorPixels > 500U;
                RoomLightingRenderData squareColumnShadow = frameLighting;
                squareColumnShadow.shadowOccluders[0].octagonalPrism = false;
                passed &= renderer.render(width, height, camera, {}, {},
                    breakableProps, squareColumnShadow, room, {}, {}, {}, hud, marks);
                glFinish();
                std::vector<std::uint8_t> squareShadowPixels(pixels.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    squareShadowPixels.data());
                std::size_t correctedShadowPixels = 0U;
                for (int y = 0; y < height / 2; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const std::size_t pixel
                            = static_cast<std::size_t>((y * width + x) * 4);
                        const int square = squareShadowPixels[pixel]
                            + squareShadowPixels[pixel + 1U]
                            + squareShadowPixels[pixel + 2U];
                        const int octagonal = pixels[pixel] + pixels[pixel + 1U]
                            + pixels[pixel + 2U];
                        if (octagonal > square + 20) ++correctedShadowPixels;
                    }
                }
                if (correctedShadowPixels <= 20U) {
                    std::cerr << "Central Hub bevels did not correct the floor shadow: "
                        << correctedShadowPixels << " pixels\n";
                }
                passed &= correctedShadowPixels > 20U;
            }
            std::vector<std::uint8_t> topDown(pixels.size());
            const std::size_t stride = static_cast<std::size_t>(width * 4);
            for (int row = 0; row < height; ++row) {
                std::copy_n(pixels.data()
                        + static_cast<std::size_t>(height - row - 1) * stride,
                    stride,
                    topDown.data() + static_cast<std::size_t>(row) * stride);
            }
            if (laboratoryBulletMarkDetail) {
                laboratoryMarkedWallPixels = topDown;
            }
            if (laboratoryBulletMarkBaseline) {
                std::size_t changedChannels = 0U;
                for (std::size_t index = 0U; index < topDown.size(); ++index) {
                    changedChannels += topDown[index]
                        != laboratoryMarkedWallPixels[index];
                }
                passed &= changedChannels > 30U;
            }
            if (blackout && centralHub && !potato && fov == 60.0F) {
                // An upper shaft patch excludes the route screen and light
                // strip. It must go dark with the room, then respond to the
                // flashlight instead of retaining a fixed unlit fill.
                std::uint64_t channelSum = 0U;
                for (int y = 120; y < 200; ++y) {
                    for (int x = 520; x < 580; ++x) {
                        const std::size_t pixel
                            = static_cast<std::size_t>((y * width + x) * 4);
                        channelSum += topDown[pixel] + topDown[pixel + 1U]
                            + topDown[pixel + 2U];
                    }
                }
                hubShaftBrightness[static_cast<std::size_t>(variant)]
                    = static_cast<float>(channelSum) / (60.0F * 80.0F * 3.0F);
            }
            const std::string prefix = securityCompact
                ? "compact_console_" : shadowPilot
                ? std::string{"shadow_pilot_"} + std::to_string(variant) + "_"
                : blackout
                ? std::string{centralHub ? "central_hub_"
                    : cafeteria ? "cafeteria_" : "storage_"}
                    + (variant == 0 ? "lit_"
                        : variant == 1 ? "dark_" : "flashlight_")
                : floorOverview ? "floor_overview_"
                : floorDetail ? "floor_detail_"
                : hubHatchDetail ? "hub_hatch_detail_"
                : hubVentDetail ? "hub_vent_detail_"
                : hubCeilingDetail ? "hub_ceiling_detail_"
                : hubDuctDetail ? "hub_duct_detail_"
                : hubWallPanelDetail ? "hub_wall_panel_detail_"
                : hubWallVentDetail ? "hub_wall_vent_detail_"
                : planterDetail ? "planter_detail_"
                : ductDetail ? "duct_detail_"
                : trimCornerDetail ? "trim_corner_detail_"
                : securityConsoleDetail ? "security_console_detail_"
                : securityCabinetDetail ? "security_cabinet_detail_"
                : securityCoverDetail ? "security_cover_detail_"
                : securityCoverCornerDetail ? "security_cover_corner_detail_"
                : observationDetail ? "observation_detail_"
                : securityDuctDetail ? "security_square_duct_detail_"
                : bulletMarkDetail ? "bullet_mark_detail_"
                : securityIndicatorDetail
                    ? (variant == 12 ? "security_alarm_on_"
                        : variant == 13 ? "security_alarm_off_"
                        : variant == 14 ? "security_reboot_"
                        : "security_ready_")
                : industrialSteelDetail ? "structural_steel_detail_"
                : technicalTableBoxDetail ? "table_box_detail_"
                : laboratoryBenchDetail ? "bench_detail_"
                : laboratoryContainmentDetail ? "containment_detail_"
                : laboratoryDomeDetail ? "dome_detail_"
                : laboratoryDuctDetail ? "lab_duct_detail_"
                : laboratoryTabletopDetail ? "tabletop_domes_"
                : laboratoryBulletMarkDetail ? "lab_bullet_mark_"
                : laboratoryBulletMarkBaseline ? "lab_bullet_mark_baseline_"
                : counterGrazingDetail ? "counter_grazing_detail_"
                : counterTopDetail ? "counter_top_detail_"
                : counterDetail ? "counter_detail_"
                : storageDuctDetail ? "storage_duct_detail_"
                : cafeteriaPlantDetail ? "cafeteria_plant_detail_"
                : cafeteriaDuctDetail ? "cafeteria_duct_detail_"
                : combatCeilingDetail ? "ceiling_detail_"
                : combatStairDetail ? "stair_detail_"
                : combatCoverDetail ? "cover_detail_"
                : fixtureDetail
                    ? (cafeteria ? "furniture_detail_" : "fixture_detail_")
                    : "";
            const auto path = std::filesystem::path{directory}
                / (prefix
                    + (potato ? "potato" : "ultra")
                    + "_fov" + std::to_string(static_cast<int>(fov)) + ".bmp");
            SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height,
                SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
            passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
            if (surface) SDL_DestroySurface(surface);
            }
            if (blackout && centralHub && !potato && fov == 60.0F) {
                const bool shaftFollowsLighting
                    = hubShaftBrightness[0] > hubShaftBrightness[1] + 40.0F
                    && hubShaftBrightness[2] > hubShaftBrightness[1] + 40.0F;
                if (!shaftFollowsLighting) {
                    std::cerr << "Hub shaft ignores room lighting: lit="
                        << hubShaftBrightness[0] << " dark="
                        << hubShaftBrightness[1] << " flashlight="
                        << hubShaftBrightness[2] << '\n';
                }
                passed &= shaftFollowsLighting;
            }
        }
    }
    return passed;
}

bool captureIndustrialCorridorPreview(
    gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* directory = std::getenv("GDP_INDUSTRIAL_CORRIDOR_CAPTURE_DIR");
    if (!directory || !*directory) return false;
    std::filesystem::create_directories(directory);
    constexpr int width = 1280;
    constexpr int height = 720;
    if (!renderer.prepareIndustrialRoomMaterials(20261031U)) return false;
    const std::uint64_t materialKey = renderer.activeRoomMaterialKey();
    RoomRenderData startRoom;
    startRoom.worldBounds = {{-3.85F, 0.0F, -4.85F},
        {3.85F, 4.0F, 4.85F}};
    startRoom.materialKey = materialKey;
    RoomRenderData destination;
    destination.worldBounds = {{-3.85F, 0.0F, -24.85F},
        {3.85F, 4.0F, -15.15F}};
    if (!renderer.prepareOfficeRoomMaterials(20261031U)) return false;
    destination.materialKey = renderer.activeRoomMaterialKey();
    const std::array points{
        wkc::core::Vec3{0.0F, 0.0F, -4.85F},
        wkc::core::Vec3{0.0F, 0.0F, -15.15F},
    };
    RoomLightingRenderData lighting;
    lighting.ambientColor = {0.76F, 0.79F, 0.82F};
    lighting.ambientIntensity = 0.37F;
    lighting.lightCount = 2U;
    lighting.lights[0] = {{0.0F, 2.45F, -7.0F},
        {0.95F, 0.89F, 0.78F}, 1.15F, 7.0F};
    lighting.lights[1] = {{0.0F, 2.45F, -11.0F},
        {0.95F, 0.89F, 0.78F}, 1.15F, 7.0F};
    bool passed = true;
    const auto saveFrame = [&](const std::string& fileName) {
        glFinish();
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width * height * 4));
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
            pixels.data());
        if (glGetError() != GL_NO_ERROR) return false;
        std::vector<std::uint8_t> topDown(pixels.size());
        const std::size_t stride = static_cast<std::size_t>(width * 4);
        for (int row = 0; row < height; ++row) {
            std::copy_n(pixels.data()
                    + static_cast<std::size_t>(height - row - 1) * stride,
                stride,
                topDown.data() + static_cast<std::size_t>(row) * stride);
        }
        const auto path = std::filesystem::path{directory} / fileName;
        SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height,
            SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
        const bool saved = surface
            && SDL_SaveBMP(surface, path.string().c_str());
        if (surface) SDL_DestroySurface(surface);
        return saved;
    };
    for (const bool wide : {false, true}) {
        const float passageWidth = wide ? 2.4F : 1.2F;
        const float passageHeight = wide ? 2.8F : 2.2F;
        if (!renderer.prepareCorridorRoomMaterials(20261031U, wide)) return false;
        const std::array corridor{
            CorridorRenderData{points, passageWidth, passageHeight,
                renderer.activeRoomMaterialKey()},
        };
        const std::array doors{
            DoorRenderData{{0.0F, 0.0F, -4.85F}, {0.0F, 0.0F, -1.0F},
                passageWidth, passageHeight, false, 1.0F},
            DoorRenderData{{0.0F, 0.0F, -15.15F}, {0.0F, 0.0F, 1.0F},
                passageWidth, passageHeight, false, 1.0F},
        };
        for (const bool potato : {false, true}) {
            for (const float fov : {60.0F, 100.0F}) {
                CameraData camera;
                camera.position = {0.0F, 1.62F, -6.5F};
                camera.forward = {0.0F, -0.02F, -1.0F};
                camera.verticalFieldOfViewDegrees = fov;
                HudRenderData hud;
                hud.graphicsQuality = potato
                    ? HudGraphicsQuality::Potato : HudGraphicsQuality::Ultra;
                hud.detailedMaterials = !potato;
                hud.worldRenderScalePercent = potato ? 50U : 100U;
                passed &= renderer.render(width, height, camera, {}, {}, {},
                    lighting, startRoom, doors, corridor,
                    std::span<const RoomRenderData>{&destination, 1U}, hud);
                passed &= saveFrame(std::string{wide ? "wide_" : "narrow_"}
                    + (potato ? "potato" : "ultra")
                    + "_fov" + std::to_string(static_cast<int>(fov)) + ".bmp");
            }
        }
        for (const bool inspectionWindow : {false, true}) {
            auto closedDoors = doors;
            closedDoors[0].openFraction = 0.0F;
            closedDoors[0].inspectionWindow = inspectionWindow;
            CameraData doorCamera;
            doorCamera.position = {0.0F, 1.62F, -2.2F};
            doorCamera.forward = {0.0F, 0.0F, -1.0F};
            RoomLightingRenderData doorLighting = lighting;
            doorLighting.lights[0].position = {0.0F, 2.7F, -3.0F};
            HudRenderData doorHud;
            doorHud.graphicsQuality = HudGraphicsQuality::Ultra;
            doorHud.detailedMaterials = true;
            passed &= renderer.render(width, height, doorCamera, {}, {}, {},
                doorLighting, startRoom, closedDoors, corridor,
                std::span<const RoomRenderData>{&destination, 1U}, doorHud);
            passed &= saveFrame(std::string{wide ? "wide" : "narrow"}
                + (inspectionWindow ? "_door_b.bmp" : "_door_a.bmp"));
        }
    }
    return passed;
}

struct InstancingPixelComparison {
    unsigned maximumDelta{0U};
    std::size_t changedChannels{0U};
    bool alphaMatches{true};
    bool equivalent{false};
};

InstancingPixelComparison compareInstancingPixels(
    const std::vector<std::uint8_t>& actual, const std::vector<std::uint8_t>& reference)
{
    InstancingPixelComparison result;
    if (actual.empty() || actual.size() != reference.size() || actual.size() % 4U != 0U)
        return result;
    for (std::size_t i = 0U; i < actual.size(); ++i) {
        const auto delta = static_cast<unsigned>(std::abs(int(actual[i]) - int(reference[i])));
        if (i % 4U == 3U) result.alphaMatches &= delta == 0U;
        else {
            result.maximumDelta = std::max(result.maximumDelta, delta);
            result.changedChannels += delta != 0U;
        }
    }
    // llvmpipe: 11/192,000 RGB channels differ, maximum 7/255. NVIDIA is exact.
    // Permit <=8/255 only in <=1/8192 RGB channels; alpha remains exact.
    result.equivalent = result.alphaMatches && result.maximumDelta <= 8U
        && result.changedChannels <= (actual.size() / 4U * 3U) / 8192U + 1U;
    return result;
}

bool validateInstancingTolerance()
{
    std::vector<std::uint8_t> reference(320U * 200U * 4U, 100U);
    auto actual = reference;
    bool passed = compareInstancingPixels(actual, reference).equivalent;
    actual[0] += 7U;
    passed &= compareInstancingPixels(actual, reference).equivalent;
    actual[0] = 109U;
    passed &= !compareInstancingPixels(actual, reference).equivalent;
    actual = reference; actual[3] = 101U;
    passed &= !compareInstancingPixels(actual, reference).equivalent;
    actual = reference;
    for (std::size_t i = 0U; i < 400U; i += 4U) ++actual[i];
    passed &= !compareInstancingPixels(actual, reference).equivalent;
    actual.pop_back();
    passed &= !compareInstancingPixels(actual, reference).equivalent;
    if (!passed) std::cerr << "Instancing tolerance accepts a meaningful pixel regression\n";
    return passed;
}

// Manual renderer capture of the approved Worker limb proposal. These frames
// support technical review; final appearance is judged in a played build.
bool captureWorkerLimbVariation(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* directory = std::getenv("GDP_WORKER_LIMB_CAPTURE_DIR");
    if (!directory || !*directory) {
        std::cerr << "Set GDP_WORKER_LIMB_CAPTURE_DIR for Worker captures\n";
        return false;
    }
    std::filesystem::create_directories(directory);
    constexpr int width = 1280;
    constexpr int height = 720;
    RoomRenderData room;
    room.origin = {0.0F, 0.0F, -20.0F};
    room.worldBounds = {{-5.0F, 0.0F, -4.0F}, {5.0F, 4.0F, 9.0F}};
    RoomLightingRenderData lighting;
    lighting.ambientColor = {1.0F, 1.0F, 1.0F};
    const bool darkEyeCapture = std::getenv("GDP_WORKER_EYE_DARK_CAPTURE") != nullptr;
    lighting.ambientIntensity = darkEyeCapture ? 0.12F : 0.48F;
    lighting.lights[0] = {
        {0.0F, 3.3F, 2.0F}, {1.0F, 1.0F, 1.0F}, 1.25F, 8.0F};
    if (darkEyeCapture) lighting.lightCount = 0U;
    bool passed = true;
    for (const bool potato : {false, true}) {
        HudRenderData hud;
        // Suppress the central reticle so it cannot cover the eye in the
        // manual Worker review captures.
        hud.weapon = HudWeaponRole::Sniper;
        hud.graphicsQuality = potato
            ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
        hud.detailedMaterials = !potato;
        hud.worldRenderScalePercent = potato ? 50U : 100U;
        for (const wkc::core::Seed seed : {101U, 202U, 303U}) {
            passed &= renderer.prepareWorkerVisual(seed);
            for (const bool near : {true, false}) {
                for (const bool side : {false, true}) {
                    if (side && !near) continue;
                    CameraData camera;
                    camera.position = side
                        ? wkc::core::Vec3{3.3F, 1.55F, 0.0F}
                        : wkc::core::Vec3{0.0F, 1.55F, near ? 2.7F : 7.5F};
                    camera.forward = side
                        ? wkc::core::Vec3{-1.0F, 0.0F, 0.0F}
                        : wkc::core::Vec3{0.0F, 0.0F, -1.0F};
                    for (const EnemyAnimationState state : {
                             EnemyAnimationState::Idle, EnemyAnimationState::Walk,
                             EnemyAnimationState::Attack}) {
                        if (!near && state != EnemyAnimationState::Idle) continue;
                        if (side && state == EnemyAnimationState::Walk) continue;
                        if (potato && state != EnemyAnimationState::Idle) continue;
                        EnemyRenderData worker{{0.0F, 0.0F, 0.0F}, true, seed};
                        worker.animationState = state;
                        worker.animationTick = 15U;
                        const std::array enemies{worker};
                        passed &= renderer.render(width, height, camera, enemies,
                            {}, {}, lighting, room, {}, {}, {}, hud);
                        glFinish();
                        std::vector<std::uint8_t> pixels(
                            static_cast<std::size_t>(width * height * 4));
                        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                            pixels.data());
                        passed &= glGetError() == GL_NO_ERROR;
                        std::vector<std::uint8_t> topDown(pixels.size());
                        const std::size_t stride = static_cast<std::size_t>(width * 4);
                        for (int row = 0; row < height; ++row) {
                            std::copy_n(pixels.data()
                                    + static_cast<std::size_t>(height - row - 1) * stride,
                                stride, topDown.data() + static_cast<std::size_t>(row) * stride);
                        }
                        const char* phase = state == EnemyAnimationState::Walk ? "walk"
                            : state == EnemyAnimationState::Attack ? "attack" : "idle";
                        const auto path = std::filesystem::path{directory}
                            / (std::string{potato ? "potato" : "medium"}
                                + "_worker_seed" + std::to_string(seed)
                                + (side ? "_side_" : near ? "_near_" : "_far_")
                                + phase + ".bmp");
                        SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height,
                            SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                        passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
                        if (surface) SDL_DestroySurface(surface);
                    }
                }
            }
        }
    }
    return passed;
}

// Review-only orthographic comparisons for the reconstructed enemy models.
bool captureEnemyReferenceModels(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* const directory = std::getenv("GDP_ENEMY_REFERENCE_CAPTURE_DIR");
    const char* const enabled = std::getenv("GDP_ENEMY_REFERENCE_MODELS");
    if (directory == nullptr || *directory == '\0' || enabled == nullptr
        || std::string_view{enabled} != "1") {
        std::cerr << "Set GDP_ENEMY_REFERENCE_MODELS=1 and "
                     "GDP_ENEMY_REFERENCE_CAPTURE_DIR for model captures\n";
        return false;
    }
    std::filesystem::create_directories(directory);
    constexpr int width = 1280;
    constexpr int height = 720;
    RoomRenderData room;
    room.origin = {0.0F, 0.0F, -20.0F};
    room.worldBounds = {{-5.0F, 0.0F, -4.0F}, {5.0F, 4.0F, 9.0F}};
    RoomLightingRenderData lighting;
    lighting.ambientColor = {1.0F, 1.0F, 1.0F};
    lighting.ambientIntensity = 0.34F;
    lighting.lights[0] = {{0.0F, 3.3F, 2.0F},
        {1.0F, 0.95F, 0.85F}, 0.85F, 8.0F};
    lighting.lightCount = 1U;
    constexpr std::array roles{
        EnemyVisualRole::Worker, EnemyVisualRole::Guard,
        EnemyVisualRole::Officer, EnemyVisualRole::Engineer,
        EnemyVisualRole::IndustrialRobot};
    constexpr std::array names{"worker", "guard", "officer", "engineer", "robot"};
    constexpr std::array angles{
        "front", "left", "back", "right", "oblique_right"};
    constexpr std::array positions{
        wkc::core::Vec3{0.0F, 1.55F, 2.9F},
        wkc::core::Vec3{-2.9F, 1.55F, 0.0F},
        wkc::core::Vec3{0.0F, 1.55F, -2.9F},
        wkc::core::Vec3{2.9F, 1.55F, 0.0F},
        wkc::core::Vec3{2.3F, 1.55F, 2.3F}};
    bool passed = true;
    for (std::size_t roleIndex = 0U; roleIndex < roles.size(); ++roleIndex) {
        for (const bool potato : {false, true}) {
            HudRenderData hud;
            hud.weapon = HudWeaponRole::Sniper;
            hud.graphicsQuality = potato
                ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
            hud.detailedMaterials = !potato;
            hud.worldRenderScalePercent = potato ? 50U : 100U;
            for (std::size_t angle = 0U; angle <= angles.size(); ++angle) {
                if (potato && angle != 0U) continue;
                CameraData camera;
                const bool detailFront = angle == angles.size();
                camera.position = positions[detailFront ? 0U : angle];
                camera.forward = wkc::core::Vec3{} - camera.position;
                camera.forward.y = 0.0F;
                if (detailFront || angle == 4U) {
                    camera.verticalFieldOfViewDegrees = 48.0F;
                }
                const int variants = detailFront && !potato
                    ? roles[roleIndex] == EnemyVisualRole::IndustrialRobot ? 3 : 2
                    : !potato && roles[roleIndex] == EnemyVisualRole::Guard
                        && (angle == 1U || angle == 3U || angle == 4U) ? 2 : 1;
                for (int variant = 0; variant < variants; ++variant) {
                    EnemyRenderData enemy{{0.0F, 0.0F, 0.0F}, true,
                        static_cast<wkc::core::Seed>(101U + roleIndex), roles[roleIndex]};
                    if (variant != 0) {
                        enemy.animationState = EnemyAnimationState::Attack;
                        enemy.previousAnimationState = EnemyAnimationState::Attack;
                        enemy.attackVisualKind = variant == 2
                                || roles[roleIndex] == EnemyVisualRole::Worker
                            ? EnemyAttackVisualKind::Melee
                            : EnemyAttackVisualKind::Ranged;
                        enemy.attackPulse = 1.0F;
                    }
                    const std::array enemies{enemy};
                    passed &= renderer.render(width, height, camera, enemies,
                        {}, {}, lighting, room, {}, {}, {}, hud);
                    glFinish();
                    std::vector<std::uint8_t> pixels(
                        static_cast<std::size_t>(width * height * 4));
                    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixels.data());
                    passed &= glGetError() == GL_NO_ERROR;
                    std::vector<std::uint8_t> topDown(pixels.size());
                    const std::size_t stride = static_cast<std::size_t>(width * 4);
                    for (int row = 0; row < height; ++row) {
                        std::copy_n(pixels.data()
                                + static_cast<std::size_t>(height - row - 1) * stride,
                            stride, topDown.data() + static_cast<std::size_t>(row) * stride);
                    }
                    const std::string view = variant == 2 ? "melee_front"
                        : variant == 1 ? (detailFront ? "attack_front"
                            : std::string{"attack_"} + angles[angle])
                        : potato ? "potato_front" : detailFront
                            ? "detail_front" : angles[angle];
                    const auto path = std::filesystem::path{directory}
                        / (std::string{names[roleIndex]} + "_" + view + ".bmp");
                    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height,
                        SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                    passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
                    if (surface) SDL_DestroySurface(surface);
                }
            }
        }
    }
    return passed;
}

// Manual offscreen capture; excluded from the normal renderer pixel suite.
bool captureGrenadeSmoke(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    constexpr int width = 1280;
    constexpr int height = 720;
    const char* directory = std::getenv("GDP_GRENADE_SMOKE_CAPTURE_DIR");
    if (!directory || !*directory) {
        std::cerr << "Set GDP_GRENADE_SMOKE_CAPTURE_DIR for manual grenade capture\n";
        return false;
    }
    std::filesystem::create_directories(directory);
    bool passed = true;
    for (const bool potato : {false, true}) {
        HudRenderData hud;
        hud.graphicsQuality = potato
            ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
        hud.detailedMaterials = !potato;
        hud.worldRenderScalePercent = potato ? 50U : 100U;
        for (const float fov : {60.0F, 90.0F}) {
            CameraData camera;
            camera.position.x = 1.5F;
            camera.verticalFieldOfViewDegrees = fov;
            for (const float distance : {1.2F, 4.0F}) {
                const bool near = distance < 2.0F;
                const auto position = camera.position + camera.forward * distance;
                const std::string prefix = std::string{potato ? "potato" : "medium"}
                    + "_fov" + std::to_string(static_cast<int>(fov))
                    + (near ? "_near" : "_far");
                const auto capture = [&](const std::string& phase,
                                         const std::span<const GrenadeRenderData> grenades,
                                         const std::span<const GrenadeBlastRenderData> blasts) {
                    passed &= renderer.render(width, height, camera, {}, {}, {}, {},
                        {}, {}, {}, {}, hud, {}, {}, {}, {}, grenades, blasts);
                    const auto stats = renderer.lastFrameStats();
                    glFinish();
                    std::vector<std::uint8_t> pixels(
                        static_cast<std::size_t>(width * height * 4));
                    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixels.data());
                    passed &= glGetError() == GL_NO_ERROR;
                    std::vector<std::uint8_t> topDown(pixels.size());
                    const std::size_t stride = static_cast<std::size_t>(width * 4);
                    for (int row = 0; row < height; ++row)
                        std::copy_n(pixels.data()
                            + static_cast<std::size_t>(height - row - 1) * stride,
                            stride, topDown.data() + static_cast<std::size_t>(row) * stride);
                    const auto path = std::filesystem::path{directory}
                        / (prefix + "_" + phase + ".bmp");
                    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height,
                        SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                    passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
                    if (surface) SDL_DestroySurface(surface);
                    std::cout << "GRENADE_SMOKE_FRAME quality="
                        << (potato ? "POTATO" : "MEDIUM")
                        << " fov=" << fov << " distance=" << distance
                        << " phase=" << phase << " draws=" << stats.drawCalls
                        << " triangles=" << stats.triangles << '\n';
                    return pixels;
                };
                const auto changedPixels = [](const std::vector<std::uint8_t>& a,
                                              const std::vector<std::uint8_t>& b) {
                    std::size_t count = 0U;
                    for (std::size_t pixel = 0U; pixel < a.size(); pixel += 4U)
                        count += a[pixel] != b[pixel]
                            || a[pixel + 1U] != b[pixel + 1U]
                            || a[pixel + 2U] != b[pixel + 2U];
                    return count;
                };
                const auto baseline = capture("baseline", {}, {});
                const std::array grenade{GrenadeRenderData{position}};
                const auto projectile = capture("in_flight", grenade, {});
                const auto projectileChanged = changedPixels(projectile, baseline);
                passed &= projectileChanged > 0U;
                for (const auto& [phase, strength] : {
                         std::pair{"blast_start", 1.0F},
                         std::pair{"blast_mid", 0.5F},
                         std::pair{"blast_end", 0.1F}}) {
                    const std::array blast{GrenadeBlastRenderData{position, strength}};
                    const auto effect = capture(phase, {}, blast);
                    const auto changed = changedPixels(effect, baseline);
                    passed &= changed > 0U;
                    std::cout << "GRENADE_SMOKE quality="
                        << (potato ? "POTATO" : "MEDIUM")
                        << " fov=" << fov << " distance=" << distance
                        << " phase=" << phase << " changed_pixels=" << changed << '\n';
                }
                std::cout << "GRENADE_SMOKE quality="
                    << (potato ? "POTATO" : "MEDIUM")
                    << " fov=" << fov << " distance=" << distance
                    << " phase=in_flight changed_pixels=" << projectileChanged << '\n';
            }
        }
    }
    return passed;
}

bool profileGrenadeBlast(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    constexpr int width = 1280;
    constexpr int height = 720;
    constexpr unsigned int warmup = 8U;
    constexpr unsigned int samples = 32U;
    const CameraData camera{};
    const auto median = [](std::vector<double> values) {
        std::sort(values.begin(), values.end());
        return values[values.size() / 2U];
    };
    bool passed = true;
    for (const bool potato : {false, true}) {
        HudRenderData hud;
        hud.graphicsQuality = potato
            ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
        hud.detailedMaterials = !potato;
        hud.worldRenderScalePercent = potato ? 50U : 100U;
        const auto render = [&](const std::span<const GrenadeBlastRenderData> blasts) {
            return renderer.render(width, height, camera, {}, {}, {}, {}, {},
                {}, {}, {}, hud, {}, {}, {}, {}, {}, blasts);
        };
        const auto pixels = [&](const std::span<const GrenadeBlastRenderData> blasts) {
            passed &= render(blasts);
            glFinish();
            std::vector<std::uint8_t> result(
                static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                result.data());
            passed &= glGetError() == GL_NO_ERROR;
            return result;
        };
        const auto baselinePixels = pixels({});
        struct Case {
            const char* name;
            bool enabled;
            wkc::core::Vec3 position;
            float strength;
        };
        const std::array cases{
            Case{"none", false, {}, 0.0F},
            Case{"near_start", true, camera.position + camera.forward * .25F, 1.0F},
            Case{"near_mid", true, camera.position + camera.forward * .25F, .50F},
            Case{"near_late", true, camera.position + camera.forward * .25F, .10F},
            Case{"far_late", true, camera.position + camera.forward * 3.0F, .10F},
            Case{"none_after", false, {}, 0.0F},
        };
        for (const auto& entry : cases) {
            const std::array blast{
                GrenadeBlastRenderData{entry.position, entry.strength}};
            const std::span<const GrenadeBlastRenderData> span = entry.enabled
                ? std::span<const GrenadeBlastRenderData>{blast}
                : std::span<const GrenadeBlastRenderData>{};
            const auto effectPixels = pixels(span);
            std::size_t changed = 0U;
            for (std::size_t i = 0; i < effectPixels.size(); i += 4U) {
                const int delta = std::abs(int(effectPixels[i]) - int(baselinePixels[i]))
                    + std::abs(int(effectPixels[i + 1U]) - int(baselinePixels[i + 1U]))
                    + std::abs(int(effectPixels[i + 2U]) - int(baselinePixels[i + 2U]));
                changed += delta > 9;
            }
            std::vector<double> renderMilliseconds;
            std::vector<double> finishMilliseconds;
            std::vector<double> gpuMilliseconds;
            RendererFrameStats stats{};
            for (unsigned int frame = 0U; frame < warmup + samples; ++frame) {
                const auto begin = std::chrono::steady_clock::now();
                passed &= render(span);
                const auto submitted = std::chrono::steady_clock::now();
                glFinish();
                const auto finished = std::chrono::steady_clock::now();
                passed &= glGetError() == GL_NO_ERROR;
                if (frame < warmup) continue;
                stats = renderer.lastFrameStats();
                renderMilliseconds.push_back(
                    std::chrono::duration<double, std::milli>(submitted - begin).count());
                finishMilliseconds.push_back(
                    std::chrono::duration<double, std::milli>(finished - submitted).count());
                if (stats.gpuTimingAvailable)
                    gpuMilliseconds.push_back(stats.gpuMilliseconds);
            }
            std::cout << "GRENADE_BLAST_PROFILE quality="
                << (potato ? "POTATO" : "MEDIUM")
                << " case=" << entry.name
                << " render_ms=" << median(renderMilliseconds)
                << " finish_ms=" << median(finishMilliseconds)
                << " gpu_ms="
                << (gpuMilliseconds.empty() ? -1.0 : median(gpuMilliseconds))
                << " gpu_samples=" << gpuMilliseconds.size()
                << " draws=" << stats.drawCalls
                << " triangles=" << stats.triangles
                << " changed_pixels=" << changed
                << " total_pixels=" << width * height << '\n';
        }
    }
    return passed;
}

bool validateFrontendMenus(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game;
    using namespace gdp::game::visual;
    bool passed = true;
    const char* captureDirectory = std::getenv("GDP_MENU_CAPTURE_DIR");
    const char* configurationCapture = std::getenv("GDP_CONFIG_CAPTURE_DIR");
    std::array<CampaignSlotInfo, 3> slots{};
    gdp::gameplay::CampaignCheckpoint checkpoint;
    checkpoint.floor = 7U; checkpoint.difficulty = gdp::gameplay::Difficulty::Hard;
    slots[1] = {checkpoint, true};
    for (const auto language : {gdp::gameplay::Language::English, gdp::gameplay::Language::PortugueseBrazil}) {
    for (auto dimensions : {std::array{1280, 720}, std::array{960, 540}, std::array{640, 360}, std::array{320, 200}}) {
        const int width = dimensions[0], height = dimensions[1];
        for (auto page : {MenuPage::Root, MenuPage::CampaignSlots, MenuPage::CampaignSlot,
                 MenuPage::Difficulty, MenuPage::Overwrite, MenuPage::Speedrun,
                 MenuPage::Leaderboard, MenuPage::Settings, MenuPage::Display,
                 MenuPage::Audio, MenuPage::Controls, MenuPage::KeyboardControls,
                 MenuPage::GamepadControls, MenuPage::Bindings,
                 MenuPage::MoreBindings, MenuPage::GamepadBindings,
                 MenuPage::FeedbackCategory, MenuPage::FeedbackCompose}) {
            MenuFlowState state; state.page = page;
            state.feedbackFromSettings = page == MenuPage::FeedbackCategory
                || page == MenuPage::FeedbackCompose;
            gdp::gameplay::RuntimeSettings settings; settings.language = language;
            const auto view = makeMenuView(state, slots, {}, settings, true);
            HudRenderData hud;
            hud.language = language;
            hud.listMenu.active = true; hud.listMenu.title = view.title; hud.listMenu.subtitle = view.subtitle;
            hud.listMenu.titleScreen = page == MenuPage::Root;
            hud.listMenu.configuration = page == MenuPage::Settings || page == MenuPage::Display
                || page == MenuPage::Audio || page == MenuPage::Controls
                || page == MenuPage::KeyboardControls
                || page == MenuPage::GamepadControls
                || page == MenuPage::Bindings || page == MenuPage::MoreBindings
                || page == MenuPage::GamepadBindings
                || page == MenuPage::FeedbackCategory
                || page == MenuPage::FeedbackCompose;
            hud.listMenu.configurationSection = configurationSection(state);
            hud.listMenu.configurationControlPage
                = configurationControlPage(state);
            hud.listMenu.selection = page == MenuPage::Display ? 1U : 0U;
            for (const auto& row : view.rows) {
                hud.listMenu.rows.push_back(row.text); hud.listMenu.enabled.push_back(row.enabled);
                hud.listMenu.adjustable.push_back(row.setting.has_value());
                const auto numeric = row.setting
                    ? gdp::gameplay::numericSettingDescriptor(settings, *row.setting)
                    : std::nullopt;
                hud.listMenu.sliderFractions.push_back(numeric
                    ? static_cast<float>(numeric->value - numeric->minimum)
                        / static_cast<float>(numeric->maximum - numeric->minimum)
                    : -1.0F);
            }
            const auto renderBackdrop = [&](const double time) {
                const auto scene = sampleMenuBackdrop(time);
                return renderer.render(width, height, scene.camera, scene.enemies, {}, scene.props,
                    scene.lighting, scene.room, {}, {}, {}, hud);
            };
            passed &= renderBackdrop(0.0);
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            if (captureDirectory) {
                std::vector<std::uint8_t> topDown(pixels.size());
                const auto stride = static_cast<std::size_t>(width * 4);
                for (int row = 0; row < height; ++row) std::copy_n(pixels.data() + static_cast<std::size_t>(height - row - 1) * stride,
                    stride, topDown.data() + static_cast<std::size_t>(row) * stride);
                std::filesystem::create_directories(captureDirectory);
                const auto path = std::filesystem::path{captureDirectory} / (std::string{gdp::gameplay::languageName(language)} + "-menu-" + std::to_string(static_cast<unsigned>(page))
                    + "-" + std::to_string(width) + "x" + std::to_string(height) + ".bmp");
                auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
                SDL_DestroySurface(surface);
            }
            hud.listMenu.selection = view.rows.size() - 1U;
            passed &= renderBackdrop(0.0);
            std::vector<std::uint8_t> changed(pixels.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, changed.data());
            passed &= pixels != changed;
            if (page == MenuPage::Root) {
                passed &= renderBackdrop(8.0);
                std::vector<std::uint8_t> animated(pixels.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, animated.data());
                std::size_t differences = 0U;
                for (int y = 0; y < height; ++y) for (int x = width * 3 / 4; x < width; ++x) {
                    const auto index = static_cast<std::size_t>((y * width + x) * 4);
                    differences += animated[index] != changed[index];
                }
                passed &= differences > 100U;
            }
        }
    }
    }
    if (configurationCapture) {
        for (const auto dimensions : {std::array{1280, 720}}) {
            const int width = dimensions[0], height = dimensions[1];
            for (const auto page : {MenuPage::Display, MenuPage::Audio,
                    MenuPage::KeyboardControls, MenuPage::GamepadControls}) {
            MenuFlowState state;
            state.page = page;
            state.selection = page == MenuPage::Display ? 1U : 0U;
            gdp::gameplay::RuntimeSettings settings;
            settings.language = gdp::gameplay::Language::PortugueseBrazil;
            const auto view = makeMenuView(state, slots, {}, settings, true);
            HudRenderData hud;
            hud.language = settings.language;
            hud.listMenu.active = true;
            hud.listMenu.configuration = true;
            hud.listMenu.configurationSection = configurationSection(state);
            hud.listMenu.configurationControlPage
                = configurationControlPage(state);
            hud.listMenu.uiScalePercent = 200U;
            hud.listMenu.title = view.title;
            hud.listMenu.subtitle = view.subtitle;
            hud.listMenu.selection = state.selection;
            for (const auto& row : view.rows) {
                hud.listMenu.rows.push_back(row.text);
                hud.listMenu.enabled.push_back(row.enabled);
                hud.listMenu.adjustable.push_back(row.setting.has_value());
                const auto numeric = row.setting
                    ? gdp::gameplay::numericSettingDescriptor(settings, *row.setting)
                    : std::nullopt;
                hud.listMenu.sliderFractions.push_back(numeric
                    ? static_cast<float>(numeric->value - numeric->minimum)
                        / static_cast<float>(numeric->maximum - numeric->minimum)
                    : -1.0F);
            }
            const auto scene = sampleMenuBackdrop(0.0);
            passed &= renderer.render(width, height, scene.camera, scene.enemies,
                {}, scene.props, scene.lighting, scene.room, {}, {}, {}, hud);
            std::vector<std::uint8_t> pixels(
                static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                pixels.data());
            std::vector<std::uint8_t> topDown(pixels.size());
            const auto stride = static_cast<std::size_t>(width * 4);
            for (int row = 0; row < height; ++row)
                std::copy_n(pixels.data()
                        + static_cast<std::size_t>(height - row - 1) * stride,
                    stride, topDown.data() + static_cast<std::size_t>(row) * stride);
            std::filesystem::create_directories(configurationCapture);
            const std::string_view pageName = page == MenuPage::Display ? "video"
                : page == MenuPage::Audio ? "audio"
                : page == MenuPage::GamepadControls ? "gamepad" : "keyboard";
            const auto path = std::filesystem::path{configurationCapture}
                / ("configuration-200-"
                    + std::string{pageName}
                    + "-" + std::to_string(width) + "x"
                    + std::to_string(height) + ".bmp");
            auto* surface = SDL_CreateSurfaceFrom(width, height,
                SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
            passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
            SDL_DestroySurface(surface);
            }
        }
    }
    if (!passed) std::cerr << "Frontend menu pixel render failed\n";
    return passed;
}

bool validateHighResolutionSettingsTransition(gdp::game::visual::GdpRenderer& renderer)
{
    if (!std::getenv("GDP_HIGH_RES_SETTINGS_TEST")) return true;
    using namespace gdp::game::visual;
    const auto scene = sampleMenuBackdrop(0.0);
    HudRenderData hud;
    hud.graphicsQuality = HudGraphicsQuality::Ultra;
    hud.detailedMaterials = true;
    hud.geometryShadowLightBudget = 8U;
    for (int scale = 100; scale <= 150; ++scale) {
        hud.worldRenderScalePercent = static_cast<std::uint16_t>(scale);
        if (!renderer.render(1920, 1080, scene.camera, scene.enemies, {},
                scene.props, scene.lighting, scene.room, {}, {}, {}, hud)) {
            std::cerr << "High-resolution settings transition failed at "
                << scale << "% render scale\n";
            return false;
        }
    }
    hud.worldRenderScalePercent = 100U;
    if (!renderer.render(1920, 1080, scene.camera, scene.enemies, {},
            scene.props, scene.lighting, scene.room, {}, {}, {}, hud)) return false;
    glFinish();
    return glGetError() == GL_NO_ERROR;
}

bool validateTacticalHud(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    bool passed = true;
    const char* capture = std::getenv("GDP_TACTICAL_CAPTURE_DIR");
    for (const auto language : {gdp::gameplay::Language::English, gdp::gameplay::Language::PortugueseBrazil}) {
    for (auto dimensions : {std::array{1280, 720}, std::array{960, 540},
             std::array{960, 600}, std::array{640, 360}, std::array{320, 200}}) {
        const int width = dimensions[0], height = dimensions[1];
        HudRenderData hud;
        hud.language = language;
        hud.visible = true;
        hud.health = 100.0F; hud.maximumHealth = 115.0F;
        hud.armor = 40.0F; hud.magazineAmmo = 24U; hud.reserveAmmo = 96U;
        hud.weapon = HudWeaponRole::Carbine; hud.level = 3U;
        hud.experience = 80U; hud.experienceForNextLevel = 175U;
        const auto pixelsFor = [&](const HudRenderData& state) {
            passed &= renderer.render(width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, state);
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            return pixels;
        };
        const auto original = pixelsFor(hud);
        hud.uiScalePercent = 75U;
        const auto smallerHud = pixelsFor(hud);
        hud.uiScalePercent = 150U;
        const auto largerHud = pixelsFor(hud);
        hud.uiScalePercent = 200U;
        const auto largestHud = pixelsFor(hud);
        hud.uiScalePercent = 100U;
        hud.health = 12.0F;
        const auto damaged = pixelsFor(hud);
        hud.health = 100.0F; hud.magazineAmmo = 0U;
        const auto empty = pixelsFor(hud);
        const auto differences = [&](const auto& other, int firstX, int lastX) {
            std::size_t count = 0;
            for (int y = 0; y < height; ++y) for (int x = firstX; x < lastX; ++x) {
                const auto offset = static_cast<std::size_t>((y * width + x) * 4);
                if (!std::equal(original.begin() + static_cast<std::ptrdiff_t>(offset),
                        original.begin() + static_cast<std::ptrdiff_t>(offset + 3U),
                        other.begin() + static_cast<std::ptrdiff_t>(offset))) ++count;
            }
            return count;
        };
        passed &= differences(damaged, 0, width / 2) > 0U
            && differences(damaged, width / 2, width) == 0U;
        passed &= differences(empty, 0, width / 2) == 0U
            && differences(empty, width / 2, width) > 0U;
        // The setting must change the actual gameplay HUD on both sides.
        for (const auto* scaled : {&smallerHud, &largerHud, &largestHud}) {
            if (width <= 480 && scaled != &smallerHud) continue;
            passed &= differences(*scaled, 0, width / 2) > 0U
                && differences(*scaled, width / 2, width) > 0U;
        }
        hud.weapon = HudWeaponRole::Knife;
        passed &= pixelsFor(hud) != empty;
        for (const auto role : {HudWeaponRole::Sniper, HudWeaponRole::Shotgun, HudWeaponRole::SubmachineGun}) {
            hud.weapon = role;
            passed &= pixelsFor(hud) != empty;
            // All approved firearms have their own mesh.
            WeaponViewmodelRenderData model;
            model.visible = true; model.role = role;
            passed &= renderer.render(width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, hud, {}, model);
        }
        hud.grenadeSelected = true;
        hud.grenadePriming = true;
        hud.grenades = 1U;
        hud.grenadeFuseRemainingSeconds = gdp::gameplay::grenadeFuseSeconds;
        const auto freshFuse = pixelsFor(hud);
        hud.grenadeFuseRemainingSeconds = 0.75F;
        const auto urgentFuse = pixelsFor(hud);
        hud.grenadePriming = false;
        const auto releasedFuse = pixelsFor(hud);
        passed &= freshFuse != urgentFuse && urgentFuse != releasedFuse;
        if (capture) {
            std::filesystem::create_directories(capture);
            const auto save = [&](const auto& pixels, const std::string_view state) {
                std::vector<std::uint8_t> topDown(pixels.size());
                const auto stride = static_cast<std::size_t>(width * 4);
                for (int y = 0; y < height; ++y) std::copy_n(pixels.data()
                    + static_cast<std::size_t>(height - y - 1) * stride, stride,
                    topDown.data() + static_cast<std::size_t>(y) * stride);
                const auto path = std::filesystem::path{capture}
                    / (std::string{gdp::gameplay::languageName(language)}
                        + "-tactical-hud-" + std::string{state} + "-"
                        + std::to_string(width) + "x" + std::to_string(height) + ".bmp");
                auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32,
                    topDown.data(), width * 4);
                const bool saved = surface && SDL_SaveBMP(surface, path.string().c_str());
                SDL_DestroySurface(surface);
                return saved;
            };
            passed &= save(original, "normal") && save(smallerHud, "scale-75")
                && save(largerHud, "scale-150") && save(largestHud, "scale-200")
                && save(freshFuse, "fuse-full")
                && save(urgentFuse, "fuse-urgent");
        }
    }
    }
    if (!passed) std::cerr << "Tactical HUD resource positioning/feedback failed\n";
    return passed;
}

bool validateBossHud(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* const captureDirectory = std::getenv("GDP_BOSS_HUD_CAPTURE_DIR");
    bool passed = true;
    for (const auto language : {gdp::gameplay::Language::English, gdp::gameplay::Language::PortugueseBrazil}) {
    for (const auto dimensions : {std::array{1280, 720},
             std::array{960, 540}, std::array{640, 360}, std::array{320, 200}}) {
        const int width = dimensions[0];
        const int height = dimensions[1];
        HudRenderData hud;
        hud.language = language;
        hud.visible = true;
        hud.objective = ObjectiveRenderState::DefeatBoss;
        hud.magazineAmmo = 8U;
        hud.reserveAmmo = 36U;
        hud.experienceForNextLevel = 100U;
        hud.runTimerVisible = true;
        hud.elapsedRunTicks = 4300U;
        hud.event = HudEventState::Active;
        hud.eventLocked = true;
        hud.map.visible = true;
        hud.map.roomCount = 2U;
        hud.map.rooms[0] = {1U, {0.0F, 0.0F}, true, true, true,
            {-4.0F, -4.0F}, {4.0F, 4.0F}};
        hud.map.rooms[1] = {2U, {0.0F, -12.0F}, true, true, false,
            {-5.0F, -17.0F}, {5.0F, -7.0F}};
        hud.map.linkCount = 1U;
        hud.map.links[0].first = 0U;
        hud.map.links[0].second = 1U;
        hud.map.links[0].known = true;
        hud.map.links[0].pointCount = 2U;
        hud.map.links[0].points[0] = {0.0F, -4.0F};
        hud.map.links[0].points[1] = {0.0F, -7.0F};
        constexpr std::array boss{
            EnemyRenderData{{1.5F, 0.0F, -0.5F}, true, 666U,
                EnemyVisualRole::IndustrialRobot}};
        const auto renderPixels = [&](const HudRenderData& snapshot) {
            passed &= renderer.render(width, height, {}, boss, {}, {}, {},
                {}, {}, {}, {}, snapshot);
            std::vector<std::uint8_t> pixels(
                static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            return pixels;
        };
        const auto baseline = renderPixels(hud);
        hud.boss = {true, 200.0F, 200.0F, HudBossPhase::LongRange};
        const auto full = renderPixels(hud);
        hud.boss.health = 100.0F;
        const auto half = renderPixels(hud);
        const auto longestWarningRun = [&](const std::vector<std::uint8_t>& pixels) {
            std::array<int, 3> longest{0, 0, 0};
            for (int y = height * 3 / 4; y < height; ++y) {
                int run = 0;
                for (int x = 0; x < width; ++x) {
                    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
                    const bool warning = pixels[offset] > 200U
                        && pixels[offset + 1U] > 80U && pixels[offset + 1U] < 200U
                        && pixels[offset + 2U] < 80U;
                    run = warning ? run + 1 : 0;
                    if (run > longest[0]) {
                        longest = {run, x - run + 1, x};
                    }
                }
            }
            return longest;
        };
        const auto fullBar = longestWarningRun(full);
        const auto halfBar = longestWarningRun(half);
        passed &= fullBar[0] > 40 && std::abs(halfBar[0] * 2 - fullBar[0]) <= 1
            && std::abs(fullBar[1] + fullBar[2] + 1 - width) <= 1;
        hud.boss.health = 200.0F;
        hud.uiScalePercent = 150U;
        const auto enlarged = renderPixels(hud);
        const auto enlargedBar = longestWarningRun(enlarged);
        if (width > 480) {
            const bool enlargedAndCentered = enlargedBar[0] > fullBar[0]
                && std::abs(enlargedBar[1] + enlargedBar[2] + 1 - width) <= 1;
            if (!enlargedAndCentered) {
                std::cerr << "Boss HUD scale " << width << "x" << height
                    << ": original=" << fullBar[0] << " enlarged="
                    << enlargedBar[0] << " edges=" << enlargedBar[1]
                    << "," << enlargedBar[2] << '\n';
            }
            passed &= enlargedAndCentered;
        }
        hud.uiScalePercent = 100U;
        hud.boss.health = 100.0F;
        // Long Campaign timer text and F3 must not cover or move the central bar.
        HudRenderData campaignHud = hud;
        campaignHud.boss.health = 200.0F;
        campaignHud.campaignMode = true;
        campaignHud.campaignFloor = 2U;
        campaignHud.campaignFloorCount = 3U;
        campaignHud.campaignThreatPercent = 110U;
        passed &= longestWarningRun(renderPixels(campaignHud)) == fullBar;
        campaignHud.performanceOverlay = true;
        passed &= longestWarningRun(renderPixels(campaignHud)) == fullBar;
        hud.boss.phase = HudBossPhase::Pressure;
        const auto pressure = renderPixels(hud);
        hud.boss.health = 32.0F;
        hud.boss.phase = HudBossPhase::Pursuit;
        const auto pursuit = renderPixels(hud);
        passed &= full != baseline && half != full && pressure != half
            && pursuit != pressure;

        if (captureDirectory != nullptr) {
            const auto saveCapture = [&](const std::vector<std::uint8_t>& pixels,
                const std::string& phase) {
                std::vector<std::uint8_t> topDown(pixels.size());
                const std::size_t stride = static_cast<std::size_t>(width * 4);
                for (int row = 0; row < height; ++row) {
                    std::copy_n(pixels.data() + static_cast<std::size_t>(height - row - 1)
                        * stride, stride, topDown.data() + static_cast<std::size_t>(row)
                        * stride);
                }
                const std::filesystem::path directory{captureDirectory};
                std::filesystem::create_directories(directory);
                const auto path = directory / (std::string{gdp::gameplay::languageName(language)} + "-boss-hud-" + std::to_string(width)
                    + "x" + std::to_string(height) + "-" + phase + ".bmp");
                SDL_Surface* const surface = SDL_CreateSurfaceFrom(width, height,
                    SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                const bool saved = surface != nullptr
                    && SDL_SaveBMP(surface, path.string().c_str());
                SDL_DestroySurface(surface);
                return saved;
            };
            passed &= saveCapture(full, "range") && saveCapture(enlarged, "scale-150")
                && saveCapture(pressure, "pressure")
                && saveCapture(pursuit, "pursuit");
        }
        hud.boss.visible = false;
        passed &= renderPixels(hud) == baseline;
        hud.objective = ObjectiveRenderState::Victory;
        const auto victory = renderPixels(hud);
        hud.boss.visible = true;
        passed &= renderPixels(hud) == victory;
    }
    }
    if (!passed) {
        std::cerr << "Boss HUD health/phase/visibility rendering failed\n";
    }
    return passed;
}

bool validateCampaignExitHud(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    bool passed = true;
    const char* capture = std::getenv("GDP_CAMPAIGN_EXIT_CAPTURE_DIR");
    for (const auto language : {gdp::gameplay::Language::English, gdp::gameplay::Language::PortugueseBrazil}) {
        for (const auto dimensions : {std::array{1280, 720}, std::array{960, 540},
                 std::array{640, 360}, std::array{320, 200}}) {
            const int width = dimensions[0], height = dimensions[1];
            HudRenderData hud;
            hud.language = language;
            hud.visible = true;
            hud.campaignMode = true;
            hud.campaignFloor = 1U;
            hud.campaignFloorCount = 10U;
            hud.runTimerVisible = true;
            hud.objective = ObjectiveRenderState::DefeatBoss;
            hud.prompt = HudPrompt::ExitTerminalLocked;
            hud.map.visible = true;
            hud.map.roomCount = 2U;
            hud.map.rooms[0] = {1U, {0.0F, 0.0F}, true, true, true,
                {-4.0F, -4.0F}, {4.0F, 4.0F}};
            hud.map.rooms[1] = {2U, {0.0F, -12.0F}, true, false, false,
                {-4.0F, -16.0F}, {4.0F, -8.0F}};
            hud.map.linkCount = 1U;
            hud.map.links[0] = {0U, 1U, true};
            hud.map.links[0].pointCount = 2U;
            hud.map.links[0].points[0] = {0.0F, -4.0F};
            hud.map.links[0].points[1] = {0.0F, -8.0F};
            const auto pixelsFor = [&] {
                passed &= renderer.render(width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, hud);
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                return pixels;
            };
            const auto locked = pixelsFor();
            hud.objective = ObjectiveRenderState::SecureArena;
            hud.prompt = HudPrompt::CampaignMissionRequired;
            hud.map.rooms[1].marker = HudMapMarker::Mission;
            const auto arenaMission = pixelsFor();
            hud.objective = ObjectiveRenderState::RecoverKey;
            const auto keyMission = pixelsFor();
            hud.objective = ObjectiveRenderState::ActivateTerminal;
            const auto terminalActivation = pixelsFor();
            hud.objective = ObjectiveRenderState::ClearAmbush;
            const auto terminalClear = pixelsFor();
            hud.objective = ObjectiveRenderState::ActivateExit;
            hud.prompt = HudPrompt::NextFloorTerminal;
            hud.map.rooms[1].marker = HudMapMarker::Exit;
            const auto ready = pixelsFor();
            hud.campaignFloor = 10U;
            hud.prompt = HudPrompt::FinishCampaignTerminal;
            const auto final = pixelsFor();
            hud.infoOverlay = true;
            hud.campaignFloor = 2U;
            hud.campaignMission = ObjectiveRenderState::ActivateTerminal;
            hud.objective = ObjectiveRenderState::ActivateTerminal;
            hud.prompt = HudPrompt::CampaignMissionRequired;
            hud.map.rooms[1].marker = HudMapMarker::Mission;
            const auto missionPending = pixelsFor();
            hud.campaignRouteMission = true;
            const auto routeNone = pixelsFor();
            hud.infoOverlay = false;
            const auto routeHiddenNone = pixelsFor();
            hud.campaignKeyRecovered = true;
            hud.campaignArenaSecured = true;
            const auto routeHiddenComplete = pixelsFor();
            hud.infoOverlay = true;
            hud.campaignKeyRecovered = false;
            hud.campaignArenaSecured = false;
            hud.campaignKeyRecovered = true;
            const auto routeKeyFirst = pixelsFor();
            hud.campaignKeyRecovered = false;
            hud.campaignArenaSecured = true;
            const auto routeArenaFirst = pixelsFor();
            hud.campaignKeyRecovered = true;
            hud.campaignMissionCompleted = true;
            const auto routeComplete = pixelsFor();
            hud.campaignRouteMission = false;
            hud.campaignKeyRecovered = false;
            hud.campaignArenaSecured = false;
            hud.campaignMissionCompleted = false;
            hud.campaignMission = ObjectiveRenderState::ClearAmbush;
            hud.objective = ObjectiveRenderState::ClearAmbush;
            const auto ambushPending = pixelsFor();
            hud.campaignBossDefeated = true;
            const auto bossFirst = pixelsFor();
            hud.campaignBossDefeated = false;
            hud.campaignMissionCompleted = true;
            const auto missionFirst = pixelsFor();
            hud.campaignBossDefeated = true;
            hud.campaignExitReady = true;
            hud.objective = ObjectiveRenderState::ActivateExit;
            hud.prompt = HudPrompt::NextFloorTerminal;
            hud.map.rooms[1].marker = HudMapMarker::Exit;
            const auto exitChecklist = pixelsFor();
            passed &= locked != arenaMission && arenaMission != keyMission
                && keyMission != terminalActivation
                && terminalActivation != terminalClear
                && terminalClear != ready && ready != final
                && final != missionPending && missionPending != routeNone
                && routeNone != routeKeyFirst && routeNone != routeArenaFirst
                && routeNone != routeComplete && routeKeyFirst != routeArenaFirst
                && routeKeyFirst != routeComplete && routeArenaFirst != routeComplete
                && routeHiddenNone == routeHiddenComplete
                && routeComplete != ambushPending
                && ambushPending != bossFirst && bossFirst != missionFirst
                && missionFirst != exitChecklist;
            if (capture) {
                const auto save = [&](const auto& pixels, const std::string& state) {
                    std::vector<std::uint8_t> topDown(pixels.size());
                    const auto stride = static_cast<std::size_t>(width * 4);
                    for (int row = 0; row < height; ++row) std::copy_n(pixels.data()
                        + static_cast<std::size_t>(height - row - 1) * stride, stride,
                        topDown.data() + static_cast<std::size_t>(row) * stride);
                    std::filesystem::create_directories(capture);
                    const auto path = std::filesystem::path{capture}
                        / (std::string{gdp::gameplay::languageName(language)} + "-exit-" + state
                            + "-" + std::to_string(width) + "x" + std::to_string(height) + ".bmp");
                    auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32,
                        topDown.data(), width * 4);
                    const bool saved = surface && SDL_SaveBMP(surface, path.string().c_str());
                    SDL_DestroySurface(surface);
                    return saved;
                };
                passed &= save(locked, "locked") && save(arenaMission, "arena-mission")
                    && save(keyMission, "key-mission")
                    && save(terminalActivation, "terminal-activation")
                    && save(terminalClear, "terminal-clear") && save(ready, "ready")
                    && save(final, "final")
                    && save(missionPending, "mission-pending-tab")
                    && save(routeNone, "route-0-of-2-tab")
                    && save(routeKeyFirst, "route-key-1-of-2-tab")
                    && save(routeArenaFirst, "route-arena-1-of-2-tab")
                    && save(routeComplete, "route-2-of-2-tab")
                    && save(ambushPending, "ambush-pending-tab")
                    && save(bossFirst, "boss-first-tab")
                    && save(missionFirst, "mission-first-tab")
                    && save(exitChecklist, "exit-ready-tab");
            }
        }
    }
    if (!passed) std::cerr << "Campaign exit HUD rendering failed\n";
    return passed;
}

bool validateNewWeaponVisuals(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* capture = std::getenv("GDP_NEW_WEAPON_CAPTURE_DIR");
    const int width = capture ? 1280 : 320;
    const int height = capture ? 720 : 200;
    bool passed = true;
    const auto readFrame = [&] {
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    const auto save = [&](const std::vector<std::uint8_t>& pixels, const std::string& name) {
        if (!capture) return true;
        std::filesystem::create_directories(capture);
        std::vector<std::uint8_t> topDown(pixels.size());
        const auto stride = static_cast<std::size_t>(width * 4);
        for (int y = 0; y < height; ++y) std::copy_n(
            pixels.data() + static_cast<std::size_t>(height - y - 1) * stride,
            stride, topDown.data() + static_cast<std::size_t>(y) * stride);
        auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32,
            topDown.data(), width * 4);
        const auto path = std::filesystem::path{capture} / (name + ".bmp");
        const bool saved = surface && SDL_SaveBMP(surface, path.string().c_str());
        SDL_DestroySurface(surface);
        return saved;
    };
    for (const auto role : {HudWeaponRole::Sniper, HudWeaponRole::Shotgun,
             HudWeaponRole::SubmachineGun}) {
        const std::string name = role == HudWeaponRole::Sniper ? "sniper"
            : role == HudWeaponRole::Shotgun ? "shotgun" : "smg";
        CameraData camera;
        WeaponViewmodelRenderData model{.visible = true, .role = role, .visualSeed = 1717U};
        for (const bool potato : {false, true}) {
            HudRenderData quality;
            quality.detailedMaterials = !potato;
            quality.worldRenderScalePercent = potato ? 50U : 100U;
            quality.graphicsQuality = potato ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
            passed &= renderer.render(width, height, camera, {}, {}, {}, {}, {}, {}, {}, {}, quality);
            const auto background = readFrame();
            passed &= renderer.render(width, height, camera, {}, {}, {}, {}, {}, {}, {}, {}, quality, {}, model);
            const auto actual = readFrame();
            const auto stats = renderer.lastFrameStats();
            passed &= actual != background && stats.drawCalls > 0U && stats.lowSpecShader == potato;
            passed &= save(actual, name + (potato ? "-potato" : "-gameplay"));
            model.role = HudWeaponRole::Carbine;
            passed &= renderer.render(width, height, camera, {}, {}, {}, {}, {}, {}, {}, {}, quality, {}, model);
            passed &= actual != readFrame();
            model.role = role;
        }
        const std::array pickup{PickupRenderData{{0.0F, 0.0F, 0.0F},
            role == HudWeaponRole::Sniper ? PickupVisualRole::DroppedSniper
                : role == HudWeaponRole::Shotgun ? PickupVisualRole::DroppedShotgun
                    : PickupVisualRole::DroppedSubmachineGun,
            123U, 1.0F, role == HudWeaponRole::SubmachineGun ? 120.0F : 30.0F}};
        camera.position = {1.2F, 1.0F, 1.8F};
        camera.forward = wkc::core::normalized(wkc::core::Vec3{-1.2F, -0.9F, -1.8F});
        passed &= renderer.render(width, height, camera);
        const auto floor = readFrame();
        passed &= renderer.render(width, height, camera, {}, pickup);
        const auto drop = readFrame();
        passed &= floor != drop && save(drop, name + "-ground");
    }
    // A grenade with only its dark housing visible still changes pixels, but
    // fails the visual contract: its orange armor must read while held.
    {
        CameraData camera;
        HudRenderData quality;
        passed &= renderer.render(width, height, camera, {}, {}, {}, {},
            {}, {}, {}, {}, quality);
        const auto baseline = readFrame();
        WeaponViewmodelRenderData held{
            .visible = true, .grenadeSelected = true,
            .grenadeVisible = true, .role = HudWeaponRole::Pistol};
        passed &= renderer.render(width, height, camera, {}, {}, {}, {},
            {}, {}, {}, {}, quality, {}, held);
        const auto pixels = readFrame();
        std::size_t visibleOrange = 0U;
        for (std::size_t index = 0U; index < pixels.size(); index += 4U) {
            if (pixels[index] > 70U
                && pixels[index] > pixels[index + 1U] + 18U
                && pixels[index + 1U] > pixels[index + 2U] + 6U
                && (pixels[index] != baseline[index]
                    || pixels[index + 1U] != baseline[index + 1U]
                    || pixels[index + 2U] != baseline[index + 2U])) {
                ++visibleOrange;
            }
        }
        if (visibleOrange < 8U) {
            std::cerr << "Held grenade lost its orange armor: pixels="
                << visibleOrange << '\n';
            passed = false;
        }
    }
    if (capture) {
        for (const bool potato : {false, true}) {
            HudRenderData quality;
            quality.detailedMaterials = !potato;
            quality.worldRenderScalePercent = potato ? 50U : 100U;
            quality.graphicsQuality = potato
                ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
            for (const float fov : {60.0F, 100.0F}) {
                CameraData camera;
                camera.verticalFieldOfViewDegrees = fov;
                const std::string suffix = std::string{potato ? "potato" : "medium"}
                    + "-fov" + std::to_string(static_cast<int>(fov));
                for (const auto& [role, name] : std::array{
                         std::pair{HudWeaponRole::Pistol, "pistol"},
                         std::pair{HudWeaponRole::Knife, "knife"},
                         std::pair{HudWeaponRole::Sniper, "sniper"},
                         std::pair{HudWeaponRole::Shotgun, "shotgun"},
                         std::pair{HudWeaponRole::SubmachineGun, "smg"}}) {
                    WeaponViewmodelRenderData weapon{
                        .visible = true, .role = role, .visualSeed = 1717U};
                    passed &= renderer.render(width, height, camera, {}, {}, {}, {},
                        {}, {}, {}, {}, quality, {}, weapon);
                    passed &= save(readFrame(), std::string{name} + "-" + suffix);
                }
                WeaponViewmodelRenderData rifle{
                    .visible = true, .role = HudWeaponRole::Carbine,
                    .visualSeed = 1717U};
                passed &= renderer.render(width, height, camera, {}, {}, {}, {},
                    {}, {}, {}, {}, quality, {}, rifle);
                passed &= save(readFrame(), "rifle-" + suffix);
                WeaponViewmodelRenderData grenade{
                    .visible = true, .grenadeSelected = true,
                    .grenadeVisible = true, .role = HudWeaponRole::Pistol};
                passed &= renderer.render(width, height, camera, {}, {}, {}, {},
                    {}, {}, {}, {}, quality, {}, grenade);
                passed &= save(readFrame(), "grenade-hand-" + suffix);
                grenade.grenadePriming = true;
                grenade.grenadePinProgress = 1.0F;
                grenade.grenadeChargeStrength = 1.0F;
                passed &= renderer.render(width, height, camera, {}, {}, {}, {},
                    {}, {}, {}, {}, quality, {}, grenade);
                passed &= save(readFrame(), "grenade-held-after-pin-" + suffix);
            }
        }
    }
    if (!passed) std::cerr << "Approved new weapon silhouettes failed pixel validation\n";
    return passed;
}

bool validateSniperScope(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* capture = std::getenv("GDP_SNIPER_SCOPE_CAPTURE_DIR");
    bool passed = true;
    const std::vector<std::array<int, 2>> sizes = capture
        ? std::vector<std::array<int, 2>>{{1280, 720}, {960, 600}, {320, 200}}
        : std::vector<std::array<int, 2>>{{320, 200}, {384, 240}, {480, 200}, {200, 320}};
    for (const auto dimensions : sizes) for (const bool potato : {false, true})
    for (const float fov : {60.0F, 100.0F}) {
        const int width = dimensions[0], height = dimensions[1];
        const int cx = width / 2, cy = height / 2;
        const float radius = 0.44F * static_cast<float>(std::min(width, height));
        HudRenderData hud;
        hud.worldRenderScalePercent = potato ? 50U : 100U;
        hud.detailedMaterials = !potato;
        hud.graphicsQuality = potato ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
        CameraData camera;
        camera.verticalFieldOfViewDegrees = fov * 0.35F;
        camera.position = {1.8F, 1.62F, 4.0F};
        camera.forward = wkc::core::normalized(wkc::core::Vec3{0.0F, -0.05F, -1.0F});
        WeaponViewmodelRenderData model{.visible = true, .role = HudWeaponRole::Sniper,
            .visualSeed = 1717U, .aimStrength = 1.0F};
        const auto read = [&] {
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            return pixels;
        };
        const auto frame = [&](const WeaponViewmodelRenderData& weapon, const HudRenderData& ui) {
            passed &= renderer.render(width, height, camera, {}, {}, {}, {}, {}, {}, {}, {}, ui, {}, weapon);
            return read();
        };
        const auto reference = frame({}, hud);
        const auto worldStats = renderer.lastFrameStats();
        const auto scope = frame(model, hud);
        const auto scopedStats = renderer.lastFrameStats();
        // No additional world rendering; viewmodel is absent at complete aim.
        passed &= scopedStats.drawCalls == worldStats.drawCalls
            && scopedStats.triangles == worldStats.triangles;
        const auto offset = [&](int x, int y) { return static_cast<std::size_t>((y * width + x) * 4); };
        const auto isDark = [&](int x, int y) {
            const auto i = offset(x, y);
            return scope[i] <= 3U && scope[i + 1U] <= 3U && scope[i + 2U] <= 3U;
        };
        passed &= isDark(2, 2) && isDark(width - 3, height - 3);
        // Equal radius along both axes, including portrait and ultrawide fixtures.
        const int inner = static_cast<int>(radius) - 10;
        const int outer = static_cast<int>(radius) + 5;
        for (const auto xy : {std::array{cx + inner, cy + 8}, std::array{cx + 8, cy + inner},
                 std::array{cx - inner, cy - 8}, std::array{cx - 8, cy - inner}}) {
            const auto i = offset(xy[0], xy[1]);
            passed &= std::equal(scope.begin() + static_cast<std::ptrdiff_t>(i),
                scope.begin() + static_cast<std::ptrdiff_t>(i + 3U),
                reference.begin() + static_cast<std::ptrdiff_t>(i));
        }
        passed &= isDark(cx + outer, cy + 4) && isDark(cx + 4, cy + outer);
        // White standard crosshair is not also painted over the optical reticle.
        const auto reticleDarkens = [&](int x, int y) {
            const auto i = offset(x, y);
            return scope[i] <= 8U && scope[i + 1U] <= 8U && scope[i + 2U] <= 8U
                && scope[i] + 8U < reference[i]
                && scope[i + 1U] + 8U < reference[i + 1U]
                && scope[i + 2U] + 8U < reference[i + 2U];
        };
        passed &= reticleDarkens(cx + 8, cy) && reticleDarkens(cx, cy + 8);
        const auto center = offset(cx, cy);
        passed &= scope[center] > 160U && scope[center + 1U] > 160U;

        model.aimStrength = 0.5F;
        const auto partial = frame(model, hud);
        passed &= partial != scope && partial != reference;
        model.aimStrength = 0.0F;
        const auto hip = frame(model, hud);
        // Hip-fire sniper has no standard crosshair. Compare the center to
        // the same world with no viewmodel and an explicitly selected sniper.
        auto sniperHud = hud;
        sniperHud.weapon = HudWeaponRole::Sniper;
        const auto noHipReticle = frame({}, sniperHud);
        const auto sameCenter = [&](const std::vector<std::uint8_t>& a,
                                    const std::vector<std::uint8_t>& b) {
            for (int y = cy - 8; y < cy + 8; ++y) for (int x = cx - 8; x < cx + 8; ++x) {
                const auto i = offset(x, y);
                if (!std::equal(a.begin() + static_cast<std::ptrdiff_t>(i),
                        a.begin() + static_cast<std::ptrdiff_t>(i + 3U),
                        b.begin() + static_cast<std::ptrdiff_t>(i))) return false;
            }
            return true;
        };
        passed &= sameCenter(hip, noHipReticle) && !sameCenter(hip, reference);
        model.aimStrength = 1.0F;
        model.reloadProgress = 0.4F;
        const auto reload = frame(model, hud);
        model.aimStrength = 0.0F;
        passed &= reload == frame(model, hud);
        model.reloadProgress = 0.0F;
        model.aimStrength = 1.0F;
        model.equipStrength = 0.4F;
        const auto equip = frame(model, hud);
        model.aimStrength = 0.0F;
        passed &= equip == frame(model, hud);
        model.equipStrength = 0.0F;
        model.aimStrength = 1.0F;
        for (int menu = 0; menu < 7; ++menu) {
            auto ui = hud;
            if (menu == 0) ui.paused = true;
            if (menu == 1) ui.mainMenu = true;
            if (menu == 2) ui.settingsMenu = true;
            if (menu == 3) ui.controlsMenu = true;
            if (menu == 4) ui.listMenu.active = true;
            if (menu == 5) ui.objective = ObjectiveRenderState::Victory;
            if (menu == 6) ui.objective = ObjectiveRenderState::Defeat;
            const auto disabled = frame(model, ui);
            model.aimStrength = 0.0F;
            passed &= disabled == frame(model, ui);
            model.aimStrength = 1.0F;
        }
        for (const auto role : {HudWeaponRole::Pistol, HudWeaponRole::Carbine,
                 HudWeaponRole::SubmachineGun, HudWeaponRole::Shotgun, HudWeaponRole::Knife}) {
            model.role = role;
            model.aimStrength = 1.0F;
            const auto other = frame(model, hud);
            model.aimStrength = 0.0F;
            const bool stable = other == frame(model, hud);
            // A longer weapon or raised knife may cross the small center
            // fixture. Check that it does not enable the sniper's dark mask.
            const auto corner = offset(2, 2);
            const bool unmasked = std::equal(other.begin() + static_cast<std::ptrdiff_t>(corner),
                other.begin() + static_cast<std::ptrdiff_t>(corner + 3U),
                reference.begin() + static_cast<std::ptrdiff_t>(corner));
            passed &= stable && unmasked;
        }
        model.role = HudWeaponRole::Sniper;
        // Return to aim, then release: no stale shader mode left on UI or next frame.
        model.aimStrength = 1.0F;
        passed &= scope == frame(model, hud);
        model.aimStrength = 0.0F;
        passed &= hip == frame(model, hud);
        if (capture) {
            std::filesystem::create_directories(capture);
            const auto save = [&](const std::vector<std::uint8_t>& pixels, const std::string& suffix) {
                std::vector<std::uint8_t> topDown(pixels.size());
                const auto stride = static_cast<std::size_t>(width * 4);
                for (int y = 0; y < height; ++y) std::copy_n(pixels.data()
                    + static_cast<std::size_t>(height - y - 1) * stride, stride,
                    topDown.data() + static_cast<std::size_t>(y) * stride);
                auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32,
                    topDown.data(), width * 4);
                const auto path = std::filesystem::path{capture}
                    / ("sniper-scope-v2-" + std::to_string(width) + "x" + std::to_string(height)
                        + (potato ? "-potato" : "-medium") + "-fov" + std::to_string(int(fov))
                        + "-" + suffix + ".bmp");
                const bool saved = surface && SDL_SaveBMP(surface, path.string().c_str());
                SDL_DestroySurface(surface);
                return saved;
            };
            passed &= save(scope, "aim") && save(hip, "hip");
            hud.visible = true; hud.weapon = HudWeaponRole::Sniper;
            hud.magazineAmmo = 5U; hud.reserveAmmo = 3U;
            model.aimStrength = 1.0F;
            passed &= save(frame(model, hud), "hud");
        }
        if (!passed) {
            std::cerr << "Sniper scope failed: " << width << 'x' << height
                << " potato=" << potato << " fov=" << fov << '\n';
            return false;
        }
    }
    return passed;
}

} // namespace

int main(const int argc, char** argv)
{
    const bool reloadOnly = argc == 2 && std::string{argv[1]} == "--reload-only";
    const bool scopeOnly = argc == 2 && std::string{argv[1]} == "--scope-only";
    const bool bodyOnly = argc == 2 && std::string{argv[1]} == "--player-body-only";
    const bool handsOnly = argc == 2 && std::string{argv[1]} == "--player-hands-only";
    const bool campaignHudOnly = argc == 2
        && std::string{argv[1]} == "--campaign-hud-only";
    const bool tacticalHudOnly = argc == 2
        && std::string{argv[1]} == "--tactical-hud-only";
    const bool grenadeBlastProfile = argc == 2
        && std::string{argv[1]} == "--grenade-blast-profile";
    const bool grenadeSmokeCapture = argc == 2
        && std::string{argv[1]} == "--grenade-smoke-capture";
    const bool workerLimbCapture = argc == 2
        && std::string{argv[1]} == "--worker-limb-capture";
    const bool enemyReferenceCapture = argc == 2
        && std::string{argv[1]} == "--enemy-reference-capture";
    const bool storageRackCapture = argc == 2
        && std::string{argv[1]} == "--storage-rack-capture";
    const bool cafeteriaCapture = argc == 2
        && std::string{argv[1]} == "--cafeteria-furniture-capture";
    const bool cafeteriaLongCapture = argc == 2
        && std::string{argv[1]} == "--cafeteria-long-capture";
    const bool blackoutCapture = argc == 2
        && std::string{argv[1]} == "--room-blackout-capture";
    const bool laboratoryCapture = argc == 2
        && std::string{argv[1]} == "--laboratory-capture";
    const bool technicalCapture = argc == 2
        && std::string{argv[1]} == "--technical-capture";
    const bool technicalLCapture = argc == 2
        && std::string{argv[1]} == "--technical-l-capture";
    const bool securityCapture = argc == 2
        && std::string{argv[1]} == "--security-capture";
    const bool securityTCapture = argc == 2
        && std::string{argv[1]} == "--security-t-capture";
    const bool centralHubCapture = argc == 2
        && std::string{argv[1]} == "--central-hub-capture";
    const bool supplyPickupCapture = argc == 2
        && std::string{argv[1]} == "--supply-pickup-capture";
    const bool supplyCrateCapture = argc == 2
        && std::string{argv[1]} == "--supply-crate-capture";
    const bool canisterCapture = argc == 2
        && std::string{argv[1]} == "--canister-capture";
    const bool barricadeCapture = argc == 2
        && std::string{argv[1]} == "--barricade-capture";
    const bool centralHubShadowCapture = argc == 2
        && std::string{argv[1]} == "--central-hub-shadow-capture";
    const bool storageShadowCapture = argc == 2
        && std::string{argv[1]} == "--storage-shadow-capture";
    const bool cafeteriaShadowCapture = argc == 2
        && std::string{argv[1]} == "--cafeteria-shadow-capture";
    const bool securityShadowCapture = argc == 2
        && std::string{argv[1]} == "--security-shadow-capture";
    const bool laboratoryShadowCapture = argc == 2
        && std::string{argv[1]} == "--laboratory-shadow-capture";
    const bool centralHubBlackoutCapture = argc == 2
        && std::string{argv[1]} == "--central-hub-blackout-capture";
    const bool bossColumnsCapture = argc == 2
        && std::string{argv[1]} == "--boss-columns-capture";
    const bool combatArenaCapture = argc == 2
        && std::string{argv[1]} == "--combat-arena-capture";
    const bool industrialCorridorCapture = argc == 2
        && std::string{argv[1]} == "--industrial-corridor-capture";
    const bool industrialRoomCapture = argc == 2
        && std::string{argv[1]} == "--industrial-room-capture";
    // Smallest supported interface fixture: enough room for map and corner resources.
    // A 160x100 buffer intentionally collapses the minimap to avoid covering the HUD.
    constexpr int width = 320;
    constexpr int height = 200;

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
        1280,
        720,
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
        constexpr std::array animatedWorkerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 111,
                gdp::game::visual::EnemyVisualRole::Worker, true,
                gdp::game::visual::EnemyAnimationState::Attack, 15U},
        };
        constexpr std::array hitWorkerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 111,
                gdp::game::visual::EnemyVisualRole::Worker, true,
                gdp::game::visual::EnemyAnimationState::Hit, 15U, 0.0F, 1.0F},
        };
        constexpr std::array fallenWorkerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, false, 111,
                gdp::game::visual::EnemyVisualRole::Worker, true,
                gdp::game::visual::EnemyAnimationState::Dead, 15U},
        };
        constexpr std::array fallingWorkerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, false, 111,
                gdp::game::visual::EnemyVisualRole::Worker, true,
                gdp::game::visual::EnemyAnimationState::Dead, 15U, 0.0F, 0.0F,
                gdp::game::visual::EnemyAnimationState::Attack, 0.5F},
        };
        constexpr std::array rotatedWorkerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 111,
                gdp::game::visual::EnemyVisualRole::Worker, true,
                gdp::game::visual::EnemyAnimationState::Idle, 0U, 90.0F},
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
            gdp::game::visual::PickupRenderData{
                {1.80F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Weapon},
            gdp::game::visual::PickupRenderData{
                {2.30F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Special},
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
        } else if (grenadeBlastProfile) {
            passed = profileGrenadeBlast(renderer);
        } else if (grenadeSmokeCapture) {
            passed = captureGrenadeSmoke(renderer);
        } else if (workerLimbCapture) {
            passed = captureWorkerLimbVariation(renderer);
        } else if (enemyReferenceCapture) {
            passed = captureEnemyReferenceModels(renderer);
        } else if (storageRackCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Storage);
        } else if (cafeteriaCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Cafeteria);
        } else if (cafeteriaLongCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::CafeteriaLong);
        } else if (blackoutCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Cafeteria, true)
                && captureRoomPreview(renderer, RoomPreviewKind::Storage, true);
        } else if (laboratoryCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Laboratory);
        } else if (technicalCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Technical);
        } else if (technicalLCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::TechnicalL);
        } else if (securityCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Security);
        } else if (securityTCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::SecurityT);
        } else if (centralHubCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::CentralHub);
        } else if (supplyPickupCapture) {
            passed = captureSupplyPickups(renderer);
        } else if (supplyCrateCapture) {
            passed = captureSupplyCrates(renderer);
        } else if (canisterCapture) {
            passed = captureCanisters(renderer);
        } else if (barricadeCapture) {
            passed = captureBarricades(renderer);
        } else if (centralHubShadowCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::CentralHub,
                false, true);
        } else if (storageShadowCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Storage,
                false, true);
        } else if (cafeteriaShadowCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Cafeteria,
                false, true);
        } else if (securityShadowCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Security,
                false, true);
        } else if (laboratoryShadowCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Laboratory,
                false, true);
        } else if (centralHubBlackoutCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::CentralHub, true);
        } else if (bossColumnsCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Boss);
        } else if (combatArenaCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Combat);
        } else if (industrialCorridorCapture) {
            passed = captureIndustrialCorridorPreview(renderer);
        } else if (industrialRoomCapture) {
            passed = captureRoomPreview(renderer, RoomPreviewKind::Industrial);
        } else if (bodyOnly) {
            passed = validatePlayerBody(renderer);
        } else if (handsOnly) {
            passed = validatePlayerHands(renderer);
        } else if (campaignHudOnly) {
            passed = validateCampaignExitHud(renderer);
        } else if (tacticalHudOnly) {
            passed = validateTacticalHud(renderer);
        } else if (reloadOnly) {
            passed = validateWeaponReloads(renderer);
        } else if (scopeOnly) {
            passed = validateSniperScope(renderer) && validateNewWeaponVisuals(renderer);
        } else {
            passed = validateTerminalVisual(renderer) && passed;
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
            std::array<gdp::world::RoomArchitecturePart, 1> rackParts{
                gdp::world::RoomArchitecturePart{
                    {{-0.7F, 0.0F, -1.0F}, {0.7F, 2.0F, 0.0F}},
                    {0.39F, 0.35F, 0.30F}, true, false, true,
                    gdp::world::StorageRackPart::Upright},
            };
            gdp::game::visual::RoomRenderData rackRoom;
            rackRoom.materialKey = storageMaterialKey;
            rackRoom.architectureParts = rackParts;
            rackRoom.customArchitecture = true;
            const bool renderedTexturedRack = renderer.render(
                width, height, {}, {}, {}, {}, {}, rackRoom);
            std::vector<std::uint8_t> texturedRackPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                texturedRackPixels.data());
            rackParts[0].storageRackPart = gdp::world::StorageRackPart::Shelf;
            const bool renderedShelfRack = renderer.render(
                width, height, {}, {}, {}, {}, {}, rackRoom);
            std::vector<std::uint8_t> shelfRackPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                shelfRackPixels.data());
            rackParts[0].storageRack = false;
            const bool renderedGenericRack = renderer.render(
                width, height, {}, {}, {}, {}, {}, rackRoom);
            std::vector<std::uint8_t> genericRackPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                genericRackPixels.data());
            std::size_t rackChangedChannels = 0U;
            std::size_t rackRoleChangedChannels = 0U;
            for (std::size_t index = 0U; index < texturedRackPixels.size(); ++index) {
                rackChangedChannels += texturedRackPixels[index]
                    != genericRackPixels[index];
                rackRoleChangedChannels += texturedRackPixels[index]
                    != shelfRackPixels[index];
            }
            const bool storageRackMaterialVisible = renderedTexturedRack
                && renderedShelfRack && renderedGenericRack
                && rackChangedChannels > 100U
                && rackRoleChangedChannels > 100U;

            const bool renderedCombatVariant = renderer.prepareCombatRoomMaterials(13579)
                && renderer.render(width, height);
            std::vector<std::uint8_t> combatPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, combatPixels.data());

            const bool renderedCafeteriaVariant
                = renderer.prepareCafeteriaRoomMaterials(97531)
                && renderer.render(width, height);
            const std::uint64_t cafeteriaMaterialKey
                = renderer.activeRoomMaterialKey();
            std::vector<std::uint8_t> cafeteriaPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, cafeteriaPixels.data());
            std::array<gdp::world::RoomArchitecturePart, 1> furnitureParts{
                gdp::world::RoomArchitecturePart{
                    {{-0.7F, 0.0F, -1.0F}, {0.7F, 2.0F, 0.0F}},
                    {0.49F, 0.50F, 0.48F}, true, false, false,
                    gdp::world::StorageRackPart::None,
                    gdp::world::CafeteriaFurniturePart::Steel},
            };
            gdp::game::visual::RoomRenderData furnitureRoom;
            furnitureRoom.materialKey = cafeteriaMaterialKey;
            furnitureRoom.architectureParts = furnitureParts;
            furnitureRoom.customArchitecture = true;
            const bool renderedTexturedFurniture = renderer.render(
                width, height, {}, {}, {}, {}, {}, furnitureRoom);
            std::vector<std::uint8_t> texturedFurniturePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                texturedFurniturePixels.data());
            furnitureParts[0].cafeteriaFurniturePart
                = gdp::world::CafeteriaFurniturePart::None;
            const bool renderedGenericFurniture = renderer.render(
                width, height, {}, {}, {}, {}, {}, furnitureRoom);
            std::vector<std::uint8_t> genericFurniturePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                genericFurniturePixels.data());
            std::size_t furnitureChangedChannels = 0U;
            for (std::size_t index = 0U; index < texturedFurniturePixels.size(); ++index) {
                furnitureChangedChannels += texturedFurniturePixels[index]
                    != genericFurniturePixels[index];
            }

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
            gdp::game::visual::RoomLightingRenderData flashlightLighting
                = proceduralLighting;
            flashlightLighting.flashlightEnabled = true;
            flashlightLighting.flashlightPosition = {0.0F, 1.62F, 4.0F};
            flashlightLighting.flashlightDirection = {0.0F, 0.0F, -1.0F};
            const bool renderedFlashlight = renderer.render(
                width, height, {}, {}, {}, {}, flashlightLighting);
            std::vector<std::uint8_t> flashlightPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                flashlightPixels.data());
            gdp::game::visual::RoomLightingRenderData darkLighting
                = proceduralLighting;
            darkLighting.lightCount = 0U;
            darkLighting.ambientIntensity = 0.05F;
            const bool renderedDarkReference = renderer.render(
                width, height, {}, {}, {}, {}, darkLighting);
            std::vector<std::uint8_t> darkPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                darkPixels.data());
            // Fixture meshes must remain visible when the shader light budget
            // is exhausted, as when looking into the next room from a corridor.
            auto fixtureBudgetLighting = darkLighting;
            fixtureBudgetLighting.visibleFixturesPopulated = true;
            fixtureBudgetLighting.visibleFixtures.assign(
                proceduralLighting.lights.begin(),
                proceduralLighting.lights.begin() + 2);
            const bool renderedTwoFixtures = renderer.render(
                width, height, {}, {}, {}, {}, fixtureBudgetLighting);
            std::vector<std::uint8_t> twoFixturePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                twoFixturePixels.data());
            fixtureBudgetLighting.visibleFixtures.assign(
                proceduralLighting.lights.begin(),
                proceduralLighting.lights.begin() + proceduralLighting.lightCount);
            const bool renderedAllFixtures = renderer.render(
                width, height, {}, {}, {}, {}, fixtureBudgetLighting);
            std::vector<std::uint8_t> allFixturePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                allFixturePixels.data());
            std::size_t extraFixturePixels = 0U;
            for (std::size_t index = 0U; index < baseline.size(); index += 4U) {
                const int difference = std::abs(
                    static_cast<int>(allFixturePixels[index])
                    - static_cast<int>(twoFixturePixels[index]));
                if (difference >= 12) {
                    ++extraFixturePixels;
                }
            }
            passed &= renderedTwoFixtures && renderedAllFixtures
                && extraFixturePixels > 20U;
            if (extraFixturePixels <= 20U) {
                std::cerr << "Visible fixture meshes still follow the shader light budget\n";
            }
            darkLighting.flashlightEnabled = true;
            darkLighting.flashlightPosition = {0.0F, 1.62F, 4.0F};
            darkLighting.flashlightDirection = {0.0F, 0.0F, -1.0F};
            const bool renderedDarkFlashlight = renderer.render(
                width, height, {}, {}, {}, {}, darkLighting);
            std::vector<std::uint8_t> darkFlashlightPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                darkFlashlightPixels.data());
            std::size_t darkBeamPixels = 0U;
            for (std::size_t index = 0U; index < darkPixels.size(); index += 4U) {
                if (static_cast<int>(darkFlashlightPixels[index])
                        - static_cast<int>(darkPixels[index]) >= 20) {
                    ++darkBeamPixels;
                }
            }
            gdp::game::visual::RoomLightingRenderData neighbourLighting
                = proceduralLighting;
            neighbourLighting.ambientIntensity = 0.006F;
            neighbourLighting.lightCount = 1U;
            neighbourLighting.lights[0].position = {20.0F, 3.5F, -4.0F};
            neighbourLighting.lights[0].intensity = 4.0F;
            neighbourLighting.lights[0].range = 50.0F;
            neighbourLighting.lights[0].sourceRoomBounds = {
                {18.0F, 0.0F, -6.0F}, {22.0F, 4.0F, -2.0F}};
            neighbourLighting.lights[0].boundedToRoom = true;
            const bool renderedBoundedNeighbour = renderer.render(
                width, height, {}, {}, {}, {}, neighbourLighting);
            std::vector<std::uint8_t> boundedNeighbourPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                boundedNeighbourPixels.data());
            neighbourLighting.lights[0].boundedToRoom = false;
            const bool renderedUnboundedNeighbour = renderer.render(
                width, height, {}, {}, {}, {}, neighbourLighting);
            std::vector<std::uint8_t> unboundedNeighbourPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                unboundedNeighbourPixels.data());
            std::size_t neighbourLeakPixels = 0U;
            for (std::size_t index = 0U; index < baseline.size(); index += 4U) {
                if (unboundedNeighbourPixels[index]
                    > boundedNeighbourPixels[index] + 8U) {
                    ++neighbourLeakPixels;
                }
            }
            gdp::game::visual::RoomLightingRenderData ambientRegionLighting
                = proceduralLighting;
            ambientRegionLighting.lightCount = 0U;
            ambientRegionLighting.ambientIntensity = 0.25F;
            const bool renderedBrightRoom = renderer.render(
                width, height, {}, {}, {}, {}, ambientRegionLighting);
            std::vector<std::uint8_t> brightRoomPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                brightRoomPixels.data());
            ambientRegionLighting.ambientRegionCount = 1U;
            ambientRegionLighting.ambientRegions[0] = {
                {{-100.0F, -10.0F, -100.0F},
                    {100.0F, 20.0F, 100.0F}},
                {0.70F, 0.78F, 0.88F},
                0.006F,
            };
            const bool renderedDarkRoomRegion = renderer.render(
                width, height, {}, {}, {}, {}, ambientRegionLighting);
            std::vector<std::uint8_t> darkRoomRegionPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                darkRoomRegionPixels.data());
            std::size_t roomAmbientOverridePixels = 0U;
            for (std::size_t index = 0U; index < baseline.size(); index += 4U) {
                if (brightRoomPixels[index]
                    > darkRoomRegionPixels[index] + 8U) {
                    ++roomAmbientOverridePixels;
                }
            }

            gdp::game::visual::HudRenderData potatoQuality;
            potatoQuality.graphicsQuality
                = gdp::game::visual::HudGraphicsQuality::Potato;
            potatoQuality.worldRenderScalePercent = 50U;
            potatoQuality.detailedMaterials = false;
            gdp::game::visual::RoomLightingRenderData potatoLighting
                = proceduralLighting;
            potatoLighting.lightCount = 2U;
            potatoLighting.shadowLightCount = 0U;
            potatoLighting.shadowOccluderCount = 0U;
            const bool renderedPotatoQuality = renderer.render(
                width, height, {}, {}, {}, {}, potatoLighting,
                {}, {}, {}, {}, potatoQuality);
            const gdp::game::visual::RendererFrameStats potatoFrameStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> potatoQualityPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                potatoQualityPixels.data());
            gdp::game::visual::RoomLightingRenderData potatoFlashlightLighting
                = potatoLighting;
            potatoFlashlightLighting.flashlightEnabled = true;
            potatoFlashlightLighting.flashlightPosition = {0.0F, 1.62F, 4.0F};
            potatoFlashlightLighting.flashlightDirection = {0.0F, 0.0F, -1.0F};
            const bool renderedPotatoFlashlight = renderer.render(
                width, height, {}, {}, {}, {}, potatoFlashlightLighting,
                {}, {}, {}, {}, potatoQuality);
            std::vector<std::uint8_t> potatoFlashlightPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                potatoFlashlightPixels.data());
            gdp::game::visual::RoomLightingRenderData potatoReferenceLighting
                = potatoLighting;
            potatoReferenceLighting.shadowOccluderCount = 1U;
            const bool renderedPotatoReference = renderer.render(
                width, height, {}, {}, {}, {}, potatoReferenceLighting,
                {}, {}, {}, {}, potatoQuality);
            const gdp::game::visual::RendererFrameStats potatoReferenceStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> potatoReferencePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                potatoReferencePixels.data());

            gdp::game::visual::RoomLightingRenderData damagedFixtureLighting
                = proceduralLighting;
            damagedFixtureLighting.lights[0].intensity *= 0.45F;
            damagedFixtureLighting.lights[0].fixtureState
                = gdp::game::visual::PropVisualState::Damaged;
            damagedFixtureLighting.lights[0].fixtureEmissionScale = 0.45F;
            const bool renderedDamagedFixture = renderer.render(
                width, height, {}, {}, {}, {}, damagedFixtureLighting);
            std::vector<std::uint8_t> damagedFixturePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                damagedFixturePixels.data());

            gdp::game::visual::RoomLightingRenderData destroyedFixtureLighting
                = damagedFixtureLighting;
            destroyedFixtureLighting.lights[0].intensity = 0.0F;
            destroyedFixtureLighting.lights[0].fixtureState
                = gdp::game::visual::PropVisualState::Destroyed;
            destroyedFixtureLighting.lights[0].fixtureEmissionScale = 0.0F;
            const bool renderedDestroyedFixture = renderer.render(
                width, height, {}, {}, {}, {}, destroyedFixtureLighting);
            std::vector<std::uint8_t> destroyedFixturePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                destroyedFixturePixels.data());

            gdp::game::visual::RoomLightingRenderData shadowBaselineLighting
                = proceduralLighting;
            shadowBaselineLighting.shadowLightCount = 0U;
            const bool renderedShadowBaseline = renderer.render(
                width, height, {}, {}, {}, {}, shadowBaselineLighting);
            std::vector<std::uint8_t> shadowBaselinePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                shadowBaselinePixels.data());

            gdp::game::visual::RoomLightingRenderData shadowedLighting
                = shadowBaselineLighting;
            shadowedLighting.shadowLightCount = shadowedLighting.lightCount;
            shadowedLighting.shadowOccluderCount = 1U;
            shadowedLighting.shadowOccluders[0].bounds = {
                {-1.10F, 0.0F, -2.80F}, {1.10F, 0.70F, -1.60F}};
            renderer.setShadowShaderSpecializationEnabled(false);
            const bool renderedFullShadowReference = renderer.render(
                width, height, {}, {}, {}, {}, shadowedLighting);
            const gdp::game::visual::RendererFrameStats fullShadowReferenceStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> fullShadowReferencePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                fullShadowReferencePixels.data());
            renderer.setShadowShaderSpecializationEnabled(true);
            const bool renderedDynamicShadows = renderer.render(
                width, height, {}, {}, {}, {}, shadowedLighting);
            const gdp::game::visual::RendererFrameStats specializedShadowStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> shadowedPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                shadowedPixels.data());
            const bool shadowSpecializationPreservedPixels
                = shadowedPixels == fullShadowReferencePixels
                && fullShadowReferenceStats.shadowShaderOccluderCapacity == 8U
                && specializedShadowStats.shadowShaderOccluderCapacity == 3U;

            gdp::game::visual::RoomLightingRenderData clampedShadowLighting
                = shadowedLighting;
            clampedShadowLighting.shadowLightCount
                = static_cast<std::uint32_t>(
                    gdp::game::visual::maxShadowCastingLights);
            const bool renderedClampedShadowBudget = renderer.render(
                width, height, {}, {}, {}, {}, clampedShadowLighting);
            std::vector<std::uint8_t> clampedShadowPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                clampedShadowPixels.data());

            gdp::game::visual::RoomLightingRenderData shadow6Lighting
                = shadowedLighting;
            shadow6Lighting.shadowOccluderCount = 4U;
            for (std::size_t index = 1U; index < 4U; ++index) {
                const float offset = static_cast<float>(index) * 0.25F;
                shadow6Lighting.shadowOccluders[index].bounds = {
                    {-1.10F + offset, 0.0F, -2.80F},
                    {1.10F + offset, 0.70F, -1.60F}};
            }
            const bool renderedShadow6Variant = renderer.render(
                width, height, {}, {}, {}, {}, shadow6Lighting);
            const gdp::game::visual::RendererFrameStats shadow6Stats
                = renderer.lastFrameStats();

            gdp::game::visual::RoomLightingRenderData shadow8Lighting
                = shadow6Lighting;
            shadow8Lighting.shadowOccluderCount = 7U;
            for (std::size_t index = 4U; index < 7U; ++index) {
                const float offset = static_cast<float>(index) * 0.25F;
                shadow8Lighting.shadowOccluders[index].bounds = {
                    {-1.10F + offset, 0.0F, -2.80F},
                    {1.10F + offset, 0.70F, -1.60F}};
            }
            const bool renderedShadow8Variant = renderer.render(
                width, height, {}, {}, {}, {}, shadow8Lighting);
            const gdp::game::visual::RendererFrameStats shadow8Stats
                = renderer.lastFrameStats();

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
            renderer.setSurfaceInstancingEnabled(false);
            const bool renderedCorridorReference = renderer.render(
                width, height, corridorCamera, {}, {}, {}, {}, {}, corridorDoors,
                corridorGeometry, connectedRoomGeometry);
            const gdp::game::visual::RendererFrameStats corridorReferenceStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> corridorReferencePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                corridorReferencePixels.data());
            renderer.setSurfaceInstancingEnabled(true);
            const bool renderedCorridor = renderer.render(
                width, height, corridorCamera, {}, {}, {}, {}, {}, corridorDoors,
                corridorGeometry, connectedRoomGeometry);
            const gdp::game::visual::RendererFrameStats corridorInstancedStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> corridorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                corridorPixels.data());
            const auto instancingComparison = compareInstancingPixels(corridorPixels, corridorReferencePixels);
            const bool corridorInstancingPreservedPixels = instancingComparison.equivalent;
            const bool corridorInstancingReducedDraws
                = corridorInstancedStats.drawCalls < corridorReferenceStats.drawCalls
                && corridorInstancedStats.triangles
                    == corridorReferenceStats.triangles
                && corridorInstancedStats.instancedDrawCalls > 0U
                && corridorInstancedStats.renderedInstances
                    > corridorInstancedStats.instancedDrawCalls
                && corridorInstancedStats.avoidedDrawCalls
                    == corridorReferenceStats.drawCalls
                        - corridorInstancedStats.drawCalls;
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

            gdp::game::visual::HudRenderData resultReplayHud{
                gdp::game::visual::ObjectiveRenderState::Victory};
            resultReplayHud.resultMenuSelection
                = gdp::game::visual::HudResultMenuSelection::ReplaySeed;
            const bool renderedResultReplay = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, resultReplayHud);
            std::vector<std::uint8_t> resultReplayPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                resultReplayPixels.data());

            gdp::game::visual::HudRenderData campaignResultHud{
                gdp::game::visual::ObjectiveRenderState::Victory};
            campaignResultHud.campaignMode = true;
            campaignResultHud.campaignFloor = 1U;
            campaignResultHud.campaignFloorCount = 3U;
            campaignResultHud.campaignAdvanceAvailable = true;
            const bool renderedCampaignResult = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                campaignResultHud);
            std::vector<std::uint8_t> campaignResultPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                campaignResultPixels.data());

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
                .weapon = gdp::game::visual::HudWeaponRole::Carbine,
                .weaponOverdrive = true,
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

            gdp::game::visual::HudRenderData mapReferenceHud = functionalHud;
            mapReferenceHud.objective
                = gdp::game::visual::ObjectiveRenderState::None;
            const bool renderedMapReference = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mapReferenceHud);
            std::vector<std::uint8_t> mapReferencePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mapReferencePixels.data());

            gdp::game::visual::HudRenderData mapHud = mapReferenceHud;
            mapHud.map.visible = true;
            mapHud.map.roomCount = 3U;
            mapHud.map.rooms[0] = {1U, {0.0F, 0.0F}, true, true, true,
                {-4.0F, -4.0F}, {4.0F, 4.0F}};
            mapHud.map.rooms[1] = {2U, {0.0F, -12.0F}, true, false, false,
                {-4.0F, -16.0F}, {4.0F, -8.0F}};
            mapHud.map.rooms[2] = {3U, {12.0F, -12.0F}, false, false, false,
                {8.0F, -16.0F}, {16.0F, -8.0F}};
            mapHud.map.linkCount = 2U;
            mapHud.map.links[0] = {0U, 1U, true};
            mapHud.map.links[0].points[0] = {0.0F, -2.0F};
            mapHud.map.links[0].points[1] = {0.0F, -10.0F};
            mapHud.map.links[0].pointCount = 2U;
            mapHud.map.links[1] = {1U, 2U, false};
            const bool renderedMapHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mapHud);
            std::vector<std::uint8_t> mapHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mapHudPixels.data());

            const auto renderMapMarker = [&](const std::size_t roomIndex,
                                             const gdp::game::visual::HudMapMarker marker) {
                auto marked = mapHud;
                marked.map.rooms[roomIndex].marker = marker;
                const bool rendered = renderer.render(width, height, {}, {}, {},
                    {}, {}, {}, {}, {}, {}, marked);
                std::vector<std::uint8_t> pixels(baseline.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    pixels.data());
                return std::pair{rendered, std::move(pixels)};
            };
            const auto [renderedMissionMap, missionMapPixels]
                = renderMapMarker(1U, gdp::game::visual::HudMapMarker::Mission);
            const auto [renderedBossMap, bossMapPixels]
                = renderMapMarker(1U, gdp::game::visual::HudMapMarker::Boss);
            const auto [renderedExitMap, exitMapPixels]
                = renderMapMarker(1U, gdp::game::visual::HudMapMarker::Exit);
            const auto [renderedHiddenMarkerMap, hiddenMarkerMapPixels]
                = renderMapMarker(2U, gdp::game::visual::HudMapMarker::Mission);
            const bool mapMarkersDistinct = renderedMissionMap && renderedBossMap
                && renderedExitMap && renderedHiddenMarkerMap
                && missionMapPixels != mapHudPixels
                && bossMapPixels != missionMapPixels
                && exitMapPixels != bossMapPixels
                && hiddenMarkerMapPixels == mapHudPixels;

            gdp::game::visual::HudRenderData narrowRoomMapHud = mapHud;
            narrowRoomMapHud.map.rooms[0].minimum.x = -2.0F;
            narrowRoomMapHud.map.rooms[0].maximum.x = 2.0F;
            const bool renderedNarrowRoomMapHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, narrowRoomMapHud);
            std::vector<std::uint8_t> narrowRoomMapHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                narrowRoomMapHudPixels.data());

            gdp::game::visual::HudRenderData bentMapHud = mapHud;
            bentMapHud.map.links[0].points[1] = {4.0F, -2.0F};
            bentMapHud.map.links[0].points[2] = {4.0F, -10.0F};
            bentMapHud.map.links[0].points[3] = {0.0F, -10.0F};
            bentMapHud.map.links[0].pointCount = 4U;
            const bool renderedBentMapHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, bentMapHud);
            std::vector<std::uint8_t> bentMapHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                bentMapHudPixels.data());

            mapHud.map.rooms[1].visited = true;
            mapHud.map.rooms[2].known = true;
            mapHud.map.links[1].known = true;
            const bool renderedExploredMapHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mapHud);
            std::vector<std::uint8_t> exploredMapHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                exploredMapHudPixels.data());

            gdp::game::visual::HudRenderData infoHud = functionalHud;
            infoHud.infoOverlay = true;
            infoHud.runSeed = 12345U;
            infoHud.score = 2300U;
            infoHud.enemiesKilled = 7U;
            infoHud.eventsCompleted = 2U;
            infoHud.lootCollected = 3U;
            const bool renderedInfoHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, infoHud);
            std::vector<std::uint8_t> infoHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                infoHudPixels.data());

            infoHud.runSeed = 54321U;
            const bool renderedAlternateSeed = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, infoHud);
            std::vector<std::uint8_t> alternateSeedPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                alternateSeedPixels.data());

            gdp::game::visual::HudRenderData spreadHud = functionalHud;
            spreadHud.weaponSpread = 1.0F;
            const bool renderedSpreadHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, spreadHud);
            std::vector<std::uint8_t> spreadHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                spreadHudPixels.data());

            gdp::game::visual::HudRenderData weakPointHud = functionalHud;
            weakPointHud.hitFeedback
                = gdp::game::visual::HudHitFeedback::WeakKill;
            const bool renderedWeakPointHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, weakPointHud);
            std::vector<std::uint8_t> weakPointHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                weakPointHudPixels.data());

            gdp::game::visual::HudRenderData performanceHud = functionalHud;
            performanceHud.performanceOverlay = true;
            performanceHud.graphicsQuality
                = gdp::game::visual::HudGraphicsQuality::Potato;
            performanceHud.worldRenderScalePercent = 50U;
            performanceHud.detailedMaterials = false;
            performanceHud.performanceFramesPerSecond = 58U;
            performanceHud.averageFrameMilliseconds = 17U;
            performanceHud.percentile95FrameMilliseconds = 22U;
            performanceHud.worstFrameMilliseconds = 31U;
            performanceHud.averageUpdateMilliseconds = 2U;
            performanceHud.averageRenderMilliseconds = 7U;
            performanceHud.averagePresentMilliseconds = 8U;
            performanceHud.averageGpuMilliseconds = 6U;
            performanceHud.gpuTimingAvailable = true;
            performanceHud.framesOverBudgetPercent = 15U;
            performanceHud.worldGenerationMilliseconds = 12U;
            performanceHud.assetPreparationMilliseconds = 84U;
            performanceHud.performanceDrawCalls = 93U;
            performanceHud.performanceTriangles = 1'116U;
            performanceHud.performanceVisibleEnemies = 6U;
            performanceHud.performanceVisibleProps = 4U;
            performanceHud.performanceVisibleRooms = 3U;
            performanceHud.performanceActiveLights = 8U;
            performanceHud.performanceShadowLights = 2U;
            performanceHud.performanceShadowOccluders = 8U;
            performanceHud.performanceMeshCacheEntries = 34U;
            performanceHud.performanceTextureCacheEntries = 27U;
            performanceHud.performanceUniformUploads = 180U;
            performanceHud.performanceUniformSkips = 420U;
            performanceHud.performanceTextureUnitActivations = 15U;
            performanceHud.performanceTextureBinds = 28U;
            performanceHud.performanceTextureBindSkips = 90U;
            performanceHud.performanceMeshBinds = 12U;
            performanceHud.performanceMeshBindSkips = 81U;
            performanceHud.performanceLowSpecShader = true;
            performanceHud.performanceBackfaceCulledDraws = 72U;
            performanceHud.performanceCullStateChanges = 4U;
            performanceHud.performanceInstancedDrawCalls = 9U;
            performanceHud.performanceRenderedInstances = 42U;
            performanceHud.performanceAvoidedDrawCalls = 33U;
            performanceHud.performanceShadowShaderOccluderCapacity = 3U;
            const bool renderedPerformanceOverlay = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, performanceHud);
            const gdp::game::visual::RendererFrameStats performanceFrameStats
                = renderer.lastFrameStats();
            const bool measuredGpuFrame
                = performanceFrameStats.gpuTimingAvailable
                && std::isfinite(performanceFrameStats.gpuMilliseconds)
                && performanceFrameStats.gpuMilliseconds >= 0.0;
            std::vector<std::uint8_t> performanceOverlayPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                performanceOverlayPixels.data());

            gdp::game::visual::HudRenderData timedHud = functionalHud;
            timedHud.runTimerVisible = true;
            timedHud.elapsedRunTicks = 4'567U;
            const bool renderedRunTimer = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, timedHud);
            std::vector<std::uint8_t> runTimerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                runTimerPixels.data());

            gdp::game::visual::HudRenderData campaignTimerHud = timedHud;
            campaignTimerHud.campaignMode = true;
            campaignTimerHud.campaignFloor = 2U;
            campaignTimerHud.campaignFloorCount = 3U;
            campaignTimerHud.campaignThreatPercent = 110U;
            const bool renderedCampaignTimer = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                campaignTimerHud);
            std::vector<std::uint8_t> campaignTimerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                campaignTimerPixels.data());

            gdp::game::visual::HudRenderData speedrunRecordHud = timedHud;
            speedrunRecordHud.speedrunMode = true;
            speedrunRecordHud.speedrunRecordVisible = true;
            speedrunRecordHud.bestSpeedrunTicks = 4'200U;
            const bool renderedSpeedrunRecord = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, speedrunRecordHud);
            std::vector<std::uint8_t> speedrunRecordPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                speedrunRecordPixels.data());

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

            gdp::game::visual::HudRenderData controlsHud;
            controlsHud.controlsMenu = true;
            const bool renderedControls = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, controlsHud);
            std::vector<std::uint8_t> controlsPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                controlsPixels.data());

            gdp::game::visual::HudRenderData mainMenuContinueHud = mainMenuHud;
            mainMenuContinueHud.mainMenuSelection
                = gdp::game::visual::HudMainMenuSelection::ContinueCampaign;
            mainMenuContinueHud.campaignContinueAvailable = true;
            mainMenuContinueHud.campaignContinueFloor = 2U;
            const bool renderedMainMenuContinue = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                mainMenuContinueHud);
            std::vector<std::uint8_t> mainMenuContinuePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuContinuePixels.data());

            gdp::game::visual::HudRenderData mainMenuCampaignHud = mainMenuHud;
            mainMenuCampaignHud.mainMenuSelection
                = gdp::game::visual::HudMainMenuSelection::Campaign;
            const bool renderedMainMenuCampaign = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                mainMenuCampaignHud);
            std::vector<std::uint8_t> mainMenuCampaignPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuCampaignPixels.data());

            gdp::game::visual::HudRenderData mainMenuBestSeedHud = mainMenuHud;
            mainMenuBestSeedHud.mainMenuSelection
                = gdp::game::visual::HudMainMenuSelection::BestSeed;
            mainMenuBestSeedHud.bestSeedAvailable = true;
            mainMenuBestSeedHud.bestSpeedrunTicks = 4'200U;
            const bool renderedMainMenuBestSeed = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                mainMenuBestSeedHud);
            std::vector<std::uint8_t> mainMenuBestSeedPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuBestSeedPixels.data());

            mainMenuHud.mainMenuSelection
                = gdp::game::visual::HudMainMenuSelection::ExitGame;
            const bool renderedMainMenuExit = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mainMenuHud);
            std::vector<std::uint8_t> mainMenuExitPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuExitPixels.data());

            mainMenuHud.exitConfirmation = true;
            const bool renderedExitConfirmation = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mainMenuHud);
            std::vector<std::uint8_t> exitConfirmationPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                exitConfirmationPixels.data());

            mainMenuHud.exitConfirmationSelection
                = gdp::game::visual::HudExitConfirmationSelection::ExitGame;
            const bool renderedExitConfirmationSelected = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mainMenuHud);
            std::vector<std::uint8_t> exitConfirmationSelectedPixels(
                baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                exitConfirmationSelectedPixels.data());

            gdp::game::visual::HudRenderData settingsHud;
            settingsHud.settingsMenu = true;
            const bool renderedSettings = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, settingsHud);
            std::vector<std::uint8_t> settingsPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                settingsPixels.data());

            settingsHud.settingsSelection
                = gdp::game::visual::HudSettingsSelection::VerticalSync;
            settingsHud.verticalSync = false;
            const bool renderedSettingsAdjusted = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, settingsHud);
            std::vector<std::uint8_t> settingsAdjustedPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                settingsAdjustedPixels.data());

            const bool renderedWorker = renderer.prepareWorkerVisual(111)
                && renderer.render(width, height, {}, workerOnly);
            std::vector<std::uint8_t> workerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, workerPixels.data());

            const bool renderedAnimatedWorker
                = renderer.render(width, height, {}, animatedWorkerOnly);
            std::vector<std::uint8_t> animatedWorkerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                animatedWorkerPixels.data());

            const bool renderedHitWorker
                = renderer.render(width, height, {}, hitWorkerOnly);
            std::vector<std::uint8_t> hitWorkerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                hitWorkerPixels.data());

            const bool renderedFallenWorker
                = renderer.render(width, height, {}, fallenWorkerOnly);
            std::vector<std::uint8_t> fallenWorkerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                fallenWorkerPixels.data());

            const bool renderedFallingWorker
                = renderer.render(width, height, {}, fallingWorkerOnly);
            std::vector<std::uint8_t> fallingWorkerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                fallingWorkerPixels.data());

            const bool renderedRotatedWorker
                = renderer.render(width, height, {}, rotatedWorkerOnly);
            std::vector<std::uint8_t> rotatedWorkerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                rotatedWorkerPixels.data());

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

            std::array storageBoxProps{
                gdp::game::visual::PropRenderData{
                    {0.0F, 0.0F, -1.2F}, {0.80F, 0.88F, 0.80F},
                    gdp::game::visual::PropVisualRole::StorageBox,
                    gdp::game::visual::PropVisualState::Intact, 0U},
            };
            gdp::game::visual::RoomRenderData storageBoxRoom;
            storageBoxRoom.customArchitecture = true;
            (void)renderer.render(width, height, {}, {}, {}, {}, {}, storageBoxRoom);
            std::vector<std::uint8_t> emptyBoxPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                emptyBoxPixels.data());
            const bool renderedOliveBox = renderer.render(
                width, height, {}, {}, {}, storageBoxProps, {}, storageBoxRoom);
            std::vector<std::uint8_t> oliveBoxPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                oliveBoxPixels.data());
            storageBoxProps.front().visualSeed = 1U;
            const bool renderedCharcoalBox = renderer.render(
                width, height, {}, {}, {}, storageBoxProps, {}, storageBoxRoom);
            std::vector<std::uint8_t> charcoalBoxPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                charcoalBoxPixels.data());
            if (oliveBoxPixels == charcoalBoxPixels) {
                std::cerr << "Storage box variants rendered identical frames"
                    << " olive=" << renderedOliveBox
                    << " charcoal=" << renderedCharcoalBox
                    << " visible=" << (oliveBoxPixels != emptyBoxPixels) << '\n';
            }
            storageBoxProps.front().role
                = gdp::game::visual::PropVisualRole::TableBox;
            storageBoxProps.front().theme
                = gdp::world::RoomTheme::Maintenance;
            const bool renderedTableBox = renderer.render(
                width, height, {}, {}, {}, storageBoxProps, {}, storageBoxRoom);
            std::vector<std::uint8_t> tableBoxPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                tableBoxPixels.data());
            storageBoxProps.front().theme
                = gdp::world::RoomTheme::Laboratory;
            const bool renderedLaboratoryTableBox = renderer.render(
                width, height, {}, {}, {}, storageBoxProps, {}, storageBoxRoom);
            std::vector<std::uint8_t> laboratoryTableBoxPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                laboratoryTableBoxPixels.data());
            std::array supplyCrateProps{
                gdp::game::visual::PropRenderData{
                    {0.0F, 0.0F, -1.2F}, {0.78F, 0.72F, 0.78F},
                    gdp::game::visual::PropVisualRole::Crate,
                    gdp::game::visual::PropVisualState::Intact, 20261114U,
                    gdp::world::RoomTheme::Storage,
                    gdp::game::visual::PickupVisualRole::Ammo},
            };
            const bool renderedStorageCrate = renderer.render(
                width, height, {}, {}, {}, supplyCrateProps, {}, storageBoxRoom);
            std::vector<std::uint8_t> storageCratePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                storageCratePixels.data());
            supplyCrateProps.front().theme = gdp::world::RoomTheme::Security;
            const bool renderedSecurityCrate = renderer.render(
                width, height, {}, {}, {}, supplyCrateProps, {}, storageBoxRoom);
            std::vector<std::uint8_t> securityCratePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                securityCratePixels.data());
            supplyCrateProps.front().crateDrop
                = gdp::game::visual::PickupVisualRole::Health;
            const bool renderedHealthCrate = renderer.render(
                width, height, {}, {}, {}, supplyCrateProps, {}, storageBoxRoom);
            std::vector<std::uint8_t> healthCratePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                healthCratePixels.data());
            if (!renderedStorageCrate || !renderedSecurityCrate
                || !renderedHealthCrate
                || storageCratePixels == securityCratePixels
                || securityCratePixels == healthCratePixels) {
                std::cerr << "Supply crate pixel variants: rendered="
                    << renderedStorageCrate << '/' << renderedSecurityCrate
                    << '/' << renderedHealthCrate << " theme="
                    << (storageCratePixels != securityCratePixels)
                    << " contents="
                    << (securityCratePixels != healthCratePixels) << '\n';
            }

            const bool renderedImpactBaseline = renderer.render(width, height);
            std::vector<std::uint8_t> impactBaselinePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                impactBaselinePixels.data());
            constexpr std::array impacts{
                gdp::game::visual::ImpactRenderData{
                    {-0.22F, 1.62F, 3.0F}, {0.0F, 0.0F, 1.0F},
                    gdp::game::visual::ImpactVisualMaterial::Metal, 1.0F, 81U},
                gdp::game::visual::ImpactRenderData{
                    {0.0F, 1.62F, 3.0F}, {0.0F, 0.0F, 1.0F},
                    gdp::game::visual::ImpactVisualMaterial::Composite, 1.0F, 82U},
                gdp::game::visual::ImpactRenderData{
                    {0.22F, 1.62F, 3.0F}, {0.0F, 0.0F, 1.0F},
                    gdp::game::visual::ImpactVisualMaterial::Flesh, 1.0F, 83U},
            };
            const bool renderedImpacts = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, impacts);
            std::vector<std::uint8_t> impactPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                impactPixels.data());
            constexpr std::array bulletMarks{
                gdp::game::visual::ImpactRenderData{
                    {-0.22F, 1.62F, 3.0F}, {0.0F, 0.0F, 1.0F},
                    gdp::game::visual::ImpactVisualMaterial::Metal, 1.0F, 84U,
                    gdp::game::visual::ImpactVisualKind::BulletMark},
            };
            const bool renderedBulletMark = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, bulletMarks);
            std::vector<std::uint8_t> bulletMarkPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                bulletMarkPixels.data());

            constexpr gdp::game::visual::WeaponViewmodelRenderData pistolViewmodel{
                .visible = true,
                .role = gdp::game::visual::HudWeaponRole::Pistol,
                .visualSeed = 1717U,
            };
            const bool renderedPistolViewmodel = renderer.render(
                width, height,
                {}, // camera
                {}, // enemies
                {}, // pickups
                {}, // props
                {}, // lighting
                {}, // room
                {}, // doors
                {}, // corridors
                {}, // connected rooms
                {}, // HUD
                {}, // impacts
                pistolViewmodel);
            std::vector<std::uint8_t> pistolViewmodelPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                pistolViewmodelPixels.data());

            constexpr gdp::game::visual::WeaponViewmodelRenderData carbineViewmodel{
                .visible = true,
                .role = gdp::game::visual::HudWeaponRole::Carbine,
                .visualSeed = 1717U,
            };
            const bool renderedCarbineViewmodel = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                carbineViewmodel);
            std::vector<std::uint8_t> carbineViewmodelPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                carbineViewmodelPixels.data());

            constexpr gdp::game::visual::WeaponViewmodelRenderData knifeViewmodel{
                .visible = true,
                .role = gdp::game::visual::HudWeaponRole::Knife,
                .meleeStrength = 0.5F,
                .visualSeed = 1717U,
            };
            const bool renderedKnifeViewmodel = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                knifeViewmodel);
            std::vector<std::uint8_t> knifeViewmodelPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                knifeViewmodelPixels.data());

            struct WeaponFootprint {
                std::size_t pixels{0U};
                int minX{width};
                int minY{height};
                int maxX{0};
                int maxY{0};
            };
            const auto weaponFootprint = [&](const float fieldOfView,
                const gdp::game::visual::HudWeaponRole role,
                WeaponFootprint& footprint) {
                gdp::game::visual::CameraData fovCamera;
                fovCamera.verticalFieldOfViewDegrees = fieldOfView;
                const gdp::game::visual::WeaponViewmodelRenderData weapon{
                    .visible = true, .role = role, .visualSeed = 1717U};
                if (!renderer.render(width, height, fovCamera)) {
                    return false;
                }
                std::vector<std::uint8_t> background(baseline.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    background.data());
                if (!renderer.render(width, height, fovCamera, {}, {}, {}, {},
                        {}, {}, {}, {}, {}, {}, weapon)) {
                    return false;
                }
                std::vector<std::uint8_t> frame(baseline.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    frame.data());
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const std::size_t pixel = static_cast<std::size_t>(
                            (y * width + x) * 4);
                        bool changed = false;
                        for (std::size_t channel = 0U; channel < 3U; ++channel) {
                            changed |= std::abs(static_cast<int>(frame[pixel + channel])
                                - static_cast<int>(background[pixel + channel])) > 3;
                        }
                        if (changed) {
                            ++footprint.pixels;
                            footprint.minX = std::min(footprint.minX, x);
                            footprint.minY = std::min(footprint.minY, y);
                            footprint.maxX = std::max(footprint.maxX, x);
                            footprint.maxY = std::max(footprint.maxY, y);
                        }
                    }
                }
                return true;
            };
            bool renderedFovViewmodels = true;
            bool fovFootprintsStable = true;
            for (const auto role : {gdp::game::visual::HudWeaponRole::Pistol,
                    gdp::game::visual::HudWeaponRole::Carbine,
                    gdp::game::visual::HudWeaponRole::Knife,
                    gdp::game::visual::HudWeaponRole::Sniper,
                    gdp::game::visual::HudWeaponRole::Shotgun,
                    gdp::game::visual::HudWeaponRole::SubmachineGun}) {
                WeaponFootprint narrow;
                WeaponFootprint wide;
                renderedFovViewmodels &= weaponFootprint(60.0F, role, narrow)
                    && weaponFootprint(100.0F, role, wide);
                const bool stable = narrow.pixels > 20U
                    && wide.pixels > 20U
                    && std::abs(static_cast<int>(narrow.pixels)
                        - static_cast<int>(wide.pixels))
                        <= static_cast<int>(narrow.pixels / 10U + 2U)
                    && std::abs(narrow.minX - wide.minX) <= 3
                    && std::abs(narrow.minY - wide.minY) <= 3
                    && std::abs(narrow.maxX - wide.maxX) <= 3
                    && std::abs(narrow.maxY - wide.maxY) <= 3;
                fovFootprintsStable &= stable;
                if (!stable) {
                    std::cerr << "Viewmodel FOV footprint changed: role="
                        << static_cast<int>(role) << " pixels=" << narrow.pixels
                        << '/' << wide.pixels << " bounds=" << narrow.minX << ','
                        << narrow.minY << ',' << narrow.maxX << ',' << narrow.maxY
                        << '/' << wide.minX << ',' << wide.minY << ',' << wide.maxX
                        << ',' << wide.maxY << '\n';
                }
            }

            constexpr gdp::game::visual::WeaponViewmodelRenderData firingViewmodel{
                .visible = true,
                .role = gdp::game::visual::HudWeaponRole::Pistol,
                .recoilStrength = 1.0F,
                .muzzleFlashStrength = 1.0F,
                .overdrive = true,
                .visualSeed = 1717U,
            };
            const bool renderedFiringViewmodel = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                firingViewmodel);
            std::vector<std::uint8_t> firingViewmodelPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                firingViewmodelPixels.data());

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
            std::size_t flashlightChangedChannels = 0;
            std::size_t potatoQualityChangedChannels = 0;
            std::size_t damagedFixtureChangedChannels = 0;
            std::size_t destroyedFixtureChangedChannels = 0;
            std::size_t dynamicShadowChangedChannels = 0;
            std::size_t alertLightingChangedChannels = 0;
            std::size_t doorStateChangedChannels = 0;
            std::size_t doorMotionChangedChannels = 0;
            std::size_t corridorChangedChannels = 0;
            std::size_t connectedRoomMaterialChangedChannels = 0;
            std::size_t cornerCyanPixels = 0;
            std::size_t hudChangedChannels = 0;
            std::size_t resultReplayChangedChannels = 0;
            std::size_t campaignResultChangedChannels = 0;
            std::size_t resultSelectionChangedChannels = 0;
            std::size_t functionalHudChangedChannels = 0;
            std::size_t mapHudChangedChannels = 0;
            std::size_t mapRouteChangedChannels = 0;
            std::size_t mapFootprintChangedChannels = 0;
            std::size_t mapExplorationChangedChannels = 0;
            std::size_t spreadHudChangedChannels = 0;
            std::size_t weakPointHudChangedChannels = 0;
            std::size_t performanceOverlayChangedChannels = 0;
            std::size_t runTimerChangedChannels = 0;
            std::size_t campaignTimerChangedChannels = 0;
            std::size_t speedrunRecordChangedChannels = 0;
            std::size_t pauseMenuChangedChannels = 0;
            std::size_t mainMenuChangedChannels = 0;
            std::size_t mainMenuContinueChangedChannels = 0;
            std::size_t mainMenuCampaignChangedChannels = 0;
            std::size_t mainMenuBestSeedChangedChannels = 0;
            std::size_t mainMenuSelectionChangedChannels = 0;
            std::size_t exitConfirmationChangedChannels = 0;
            std::size_t exitConfirmationSelectionChangedChannels = 0;
            std::size_t settingsMenuChangedChannels = 0;
            std::size_t settingsSelectionChangedChannels = 0;
            std::size_t workerChangedChannels = 0;
            std::size_t enemyAnimationChangedChannels = 0;
            std::size_t enemyHitChangedChannels = 0;
            std::size_t enemyDeathTransitionChangedChannels = 0;
            std::size_t enemyFacingChangedChannels = 0;
            std::size_t guardChangedChannels = 0;
            std::size_t officerChangedChannels = 0;
            std::size_t engineerChangedChannels = 0;
            std::size_t industrialRobotChangedChannels = 0;
            std::size_t pickupChangedChannels = 0;
            std::size_t propChangedChannels = 0;
            std::size_t impactChangedChannels = 0;
            std::size_t bulletMarkChangedChannels = 0;
            std::size_t pistolViewmodelChangedChannels = 0;
            std::size_t carbineViewmodelChangedChannels = 0;
            std::size_t knifeViewmodelChangedChannels = 0;
            std::size_t firingViewmodelChangedChannels = 0;
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
                    if (flashlightPixels[index + channel]
                        > lightingPixels[index + channel]) {
                        ++flashlightChangedChannels;
                    }
                    if (potatoQualityPixels[index + channel]
                        != lightingPixels[index + channel]) {
                        ++potatoQualityChangedChannels;
                    }
                    if (damagedFixturePixels[index + channel]
                        != lightingPixels[index + channel]) {
                        ++damagedFixtureChangedChannels;
                    }
                    if (destroyedFixturePixels[index + channel]
                        != damagedFixturePixels[index + channel]) {
                        ++destroyedFixtureChangedChannels;
                    }
                    if (shadowedPixels[index + channel]
                        != shadowBaselinePixels[index + channel]) {
                        ++dynamicShadowChangedChannels;
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
                    if (resultReplayPixels[index + channel]
                        != hudPixels[index + channel]) {
                        ++resultReplayChangedChannels;
                    }
                    if (campaignResultPixels[index + channel]
                        != hudPixels[index + channel]) {
                        ++campaignResultChangedChannels;
                    }
                    if (functionalHudPixels[index + channel]
                        != centralHubPixels[index + channel]) {
                        ++functionalHudChangedChannels;
                    }
                    if (mapHudPixels[index + channel]
                        != mapReferencePixels[index + channel]) {
                        ++mapHudChangedChannels;
                    }
                    if (bentMapHudPixels[index + channel]
                        != mapHudPixels[index + channel]) {
                        ++mapRouteChangedChannels;
                    }
                    if (narrowRoomMapHudPixels[index + channel]
                        != mapHudPixels[index + channel]) {
                        ++mapFootprintChangedChannels;
                    }
                    if (exploredMapHudPixels[index + channel]
                        != mapHudPixels[index + channel]) {
                        ++mapExplorationChangedChannels;
                    }
                    if (spreadHudPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++spreadHudChangedChannels;
                    }
                    if (weakPointHudPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++weakPointHudChangedChannels;
                    }
                    if (performanceOverlayPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++performanceOverlayChangedChannels;
                    }
                    if (runTimerPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++runTimerChangedChannels;
                    }
                    if (campaignTimerPixels[index + channel]
                        != runTimerPixels[index + channel]) {
                        ++campaignTimerChangedChannels;
                    }
                    if (speedrunRecordPixels[index + channel]
                        != runTimerPixels[index + channel]) {
                        ++speedrunRecordChangedChannels;
                    }
                    if (pauseMenuPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++pauseMenuChangedChannels;
                    }
                    if (mainMenuPixels[index + channel] != baseline[index + channel]) {
                        ++mainMenuChangedChannels;
                    }
                    if (mainMenuContinuePixels[index + channel]
                        != mainMenuPixels[index + channel]) {
                        ++mainMenuContinueChangedChannels;
                    }
                    if (mainMenuCampaignPixels[index + channel]
                        != mainMenuPixels[index + channel]) {
                        ++mainMenuCampaignChangedChannels;
                    }
                    if (mainMenuBestSeedPixels[index + channel]
                        != mainMenuPixels[index + channel]) {
                        ++mainMenuBestSeedChangedChannels;
                    }
                    if (mainMenuExitPixels[index + channel]
                        != mainMenuPixels[index + channel]) {
                        ++mainMenuSelectionChangedChannels;
                    }
                    if (exitConfirmationPixels[index + channel]
                        != mainMenuExitPixels[index + channel]) {
                        ++exitConfirmationChangedChannels;
                    }
                    if (exitConfirmationSelectedPixels[index + channel]
                        != exitConfirmationPixels[index + channel]) {
                        ++exitConfirmationSelectionChangedChannels;
                    }
                    if (settingsPixels[index + channel]
                        != baseline[index + channel]) {
                        ++settingsMenuChangedChannels;
                    }
                    if (settingsAdjustedPixels[index + channel]
                        != settingsPixels[index + channel]) {
                        ++settingsSelectionChangedChannels;
                    }
                    if (workerPixels[index + channel] != centralHubPixels[index + channel]) {
                        ++workerChangedChannels;
                    }
                    if (animatedWorkerPixels[index + channel]
                        != workerPixels[index + channel]) {
                        ++enemyAnimationChangedChannels;
                    }
                    if (hitWorkerPixels[index + channel]
                        != workerPixels[index + channel]) {
                        ++enemyHitChangedChannels;
                    }
                    if (fallingWorkerPixels[index + channel]
                        != fallenWorkerPixels[index + channel]) {
                        ++enemyDeathTransitionChangedChannels;
                    }
                    if (rotatedWorkerPixels[index + channel]
                        != workerPixels[index + channel]) {
                        ++enemyFacingChangedChannels;
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
                    if (impactPixels[index + channel]
                        != impactBaselinePixels[index + channel]) {
                        ++impactChangedChannels;
                    }
                    if (bulletMarkPixels[index + channel]
                        != impactBaselinePixels[index + channel]) {
                        ++bulletMarkChangedChannels;
                    }
                    if (pistolViewmodelPixels[index + channel]
                        != impactBaselinePixels[index + channel]) {
                        ++pistolViewmodelChangedChannels;
                    }
                    if (carbineViewmodelPixels[index + channel]
                        != pistolViewmodelPixels[index + channel]) {
                        ++carbineViewmodelChangedChannels;
                    }
                    if (knifeViewmodelPixels[index + channel]
                        != pistolViewmodelPixels[index + channel]) {
                        ++knifeViewmodelChangedChannels;
                    }
                    if (firingViewmodelPixels[index + channel]
                        != pistolViewmodelPixels[index + channel]) {
                        ++firingViewmodelChangedChannels;
                    }
                    if (pixels[index + channel] != propPixels[index + channel]) {
                        ++enemyChangedChannels;
                    }
                }
            }
            passed = !renderer.gpuVendor().empty()
                && !renderer.gpuRenderer().empty()
                && !renderer.gpuVersion().empty()
                && renderedRoomVariant && repeatedRoom
                && roomPixels == repeatedRoomPixels && roomChangedChannels > 100U
                && renderedStorageVariant && themeChangedChannels > 100U
                && storageRackMaterialVisible
                && renderedCombatVariant && combatChangedChannels > 100U
                && renderedCafeteriaVariant && cafeteriaChangedChannels > 100U
                && renderedTexturedFurniture && renderedGenericFurniture
                && furnitureChangedChannels > 100U
                && renderedLaboratoryVariant && laboratoryChangedChannels > 100U
                && renderedTechnicalVariant && technicalChangedChannels > 100U
                && renderedSecurityVariant && securityChangedChannels > 100U
                && renderedCentralHubVariant && centralHubChangedChannels > 100U
                && renderedProceduralLighting && lightingChangedChannels > 100U
                && renderedFlashlight && flashlightChangedChannels > 30U
                && renderedDarkReference && renderedDarkFlashlight
                && darkBeamPixels > 500U
                && renderedBoundedNeighbour && renderedUnboundedNeighbour
                && neighbourLeakPixels > 500U
                && renderedBrightRoom && renderedDarkRoomRegion
                && roomAmbientOverridePixels > 500U
                && renderedInfoHud && infoHudPixels != functionalHudPixels
                && renderedPotatoQuality && potatoQualityChangedChannels > 100U
                && renderedPotatoFlashlight
                && potatoFlashlightPixels != potatoQualityPixels
                && potatoFrameStats.lowSpecShader
                && renderedPotatoReference
                && !potatoReferenceStats.lowSpecShader
                && potatoReferencePixels == potatoQualityPixels
                && renderedDamagedFixture
                && damagedFixtureChangedChannels > 100U
                && renderedDestroyedFixture
                && destroyedFixtureChangedChannels > 100U
                && renderedShadowBaseline && renderedFullShadowReference
                && renderedDynamicShadows && shadowSpecializationPreservedPixels
                && dynamicShadowChangedChannels > 64U
                && renderedClampedShadowBudget
                && clampedShadowPixels == shadowedPixels
                && renderedShadow6Variant
                && shadow6Stats.shadowShaderOccluderCapacity == 6U
                && renderedShadow8Variant
                && shadow8Stats.shadowShaderOccluderCapacity == 8U
                && renderedAlertLighting && alertLightingChangedChannels > 100U
                && renderedTranslatedDoor && renderedLockedDoor && renderedMovingDoor
                && doorStateChangedChannels > 100U
                && doorMotionChangedChannels > 100U
                && renderedOpenDoor && renderedCorridorReference
                && renderedCorridor && corridorInstancingPreservedPixels
                && corridorInstancingReducedDraws
                && corridorChangedChannels > 100U
                && renderedConnectedRoomMaterial
                && connectedRoomMaterialChangedChannels > 100U
                && renderedCornerCorridor && cornerCyanPixels > 20U
                && renderedHud && hudChangedChannels > 100U
                && renderedResultReplay
                && resultReplayChangedChannels > 100U
                && renderedCampaignResult
                && campaignResultChangedChannels > 100U
                && renderedResultExit && resultSelectionChangedChannels > 100U
                && renderedFunctionalHud && functionalHudChangedChannels > 500U
                && renderedMapReference && renderedMapHud
                && mapMarkersDistinct
                && renderedBentMapHud && mapRouteChangedChannels > 5U
                && renderedNarrowRoomMapHud && mapFootprintChangedChannels > 5U
                && renderedExploredMapHud && mapHudChangedChannels > 100U
                && mapExplorationChangedChannels > 5U
                && renderedSpreadHud && spreadHudChangedChannels > 20U
                && renderedWeakPointHud && weakPointHudChangedChannels > 20U
                && renderedPerformanceOverlay
                && performanceOverlayChangedChannels > 500U
                && performanceFrameStats.drawCalls > 0U
                && performanceFrameStats.triangles
                    >= performanceFrameStats.drawCalls
                // Reference enemy assets are prebuilt GPU meshes and bypass
                // the procedural mesh cache; mesh binds and enemy pixel gates
                // below still verify that geometry rendered.
                && (performanceFrameStats.meshCacheEntries > 0U
                    || (std::getenv("GDP_ENEMY_REFERENCE_MODELS") == nullptr
                        || std::string_view{std::getenv("GDP_ENEMY_REFERENCE_MODELS")}
                            != "0"))
                && performanceFrameStats.textureCacheEntries > 0U
                && performanceFrameStats.uniformUploads > 0U
                && performanceFrameStats.uniformSkips > 0U
                && performanceFrameStats.textureUnitActivations > 0U
                && performanceFrameStats.textureBinds > 0U
                && performanceFrameStats.textureBindSkips > 0U
                && performanceFrameStats.meshBinds > 0U
                && performanceFrameStats.meshBindSkips > 0U
                && performanceFrameStats.backfaceCulledDraws > 0U
                && performanceFrameStats.cullStateChanges > 0U
                && performanceFrameStats.instancedDrawCalls > 0U
                && performanceFrameStats.renderedInstances
                    > performanceFrameStats.instancedDrawCalls
                && performanceFrameStats.avoidedDrawCalls > 0U
                && performanceFrameStats.shadowShaderOccluderCapacity == 0U
                && measuredGpuFrame
                && renderedRunTimer && runTimerChangedChannels > 50U
                && renderedCampaignTimer && campaignTimerChangedChannels > 50U
                && renderedSpeedrunRecord
                && speedrunRecordChangedChannels > 50U
                && renderedPauseMenu && pauseMenuChangedChannels > 500U
                && renderedMainMenu && mainMenuChangedChannels > 500U
                && renderedControls && controlsPixels != mainMenuPixels
                && renderedAlternateSeed && alternateSeedPixels != infoHudPixels
                && renderedMainMenuContinue
                && mainMenuContinueChangedChannels > 100U
                && renderedMainMenuCampaign
                && mainMenuCampaignChangedChannels > 100U
                && renderedMainMenuBestSeed
                && mainMenuBestSeedChangedChannels > 100U
                && renderedMainMenuExit && mainMenuSelectionChangedChannels > 100U
                && renderedExitConfirmation
                && exitConfirmationChangedChannels > 500U
                && renderedExitConfirmationSelected
                && exitConfirmationSelectionChangedChannels > 100U
                && renderedSettings && settingsMenuChangedChannels > 500U
                && renderedSettingsAdjusted
                && settingsSelectionChangedChannels > 100U
                && renderedWorker && workerChangedChannels > 100U
                && renderedAnimatedWorker && enemyAnimationChangedChannels > 100U
                && renderedHitWorker && enemyHitChangedChannels > 100U
                && renderedFallenWorker && renderedFallingWorker
                && enemyDeathTransitionChangedChannels > 100U
                && renderedRotatedWorker && enemyFacingChangedChannels > 100U
                && renderedGuard && guardChangedChannels > 100U
                && renderedOfficer && officerChangedChannels > 100U
                && renderedEngineer && engineerChangedChannels > 100U
                && renderedIndustrialRobot && industrialRobotChangedChannels > 100U
                && renderedPickups && pickupChangedChannels > 100U
                && renderedProps && propChangedChannels > 100U
                && renderedOliveBox && renderedCharcoalBox
                && oliveBoxPixels != emptyBoxPixels
                && charcoalBoxPixels != emptyBoxPixels
                && oliveBoxPixels != charcoalBoxPixels
                && renderedTableBox && renderedLaboratoryTableBox
                && tableBoxPixels != charcoalBoxPixels
                && tableBoxPixels != laboratoryTableBoxPixels
                && renderedStorageCrate && renderedSecurityCrate
                && renderedHealthCrate
                && storageCratePixels != securityCratePixels
                && securityCratePixels != healthCratePixels
                && renderedImpactBaseline && renderedImpacts
                && impactChangedChannels > 10U
                && renderedBulletMark && bulletMarkChangedChannels > 10U
                && renderedPistolViewmodel
                && pistolViewmodelChangedChannels > 100U
                && renderedCarbineViewmodel
                && carbineViewmodelChangedChannels > 100U
                && renderedKnifeViewmodel
                && knifeViewmodelChangedChannels > 100U
                && renderedFovViewmodels && fovFootprintsStable
                && renderedFiringViewmodel
                && firingViewmodelChangedChannels > 100U
                && renderedEnemies && brightest > darkest + 8U && enemyChangedChannels > 100U
                && glGetError() == GL_NO_ERROR;
            if (!passed) {
                std::cerr << "Framebuffer lacks expected scene or enemy contrast"
                          << " | corridor rendered=" << renderedCorridor
                          << " openDoor=" << renderedOpenDoor
                          << " changed=" << corridorChangedChannels
                          << " cornerCyan=" << cornerCyanPixels
                          << " doorState=" << doorStateChangedChannels
                          << " fixture=" << damagedFixtureChangedChannels
                          << '/' << destroyedFixtureChangedChannels
                          << " shadow=" << dynamicShadowChangedChannels
                          << " shadowRef=" << renderedFullShadowReference
                          << " shadowExact=" << shadowSpecializationPreservedPixels
                          << " shadowCap="
                          << fullShadowReferenceStats.shadowShaderOccluderCapacity
                          << '/' << specializedShadowStats.shadowShaderOccluderCapacity
                          << " impact=" << impactChangedChannels
                          << " bulletMark=" << bulletMarkChangedChannels
                          << " pistol=" << pistolViewmodelChangedChannels
                          << " carbine=" << carbineViewmodelChangedChannels
                          << " knife=" << knifeViewmodelChangedChannels
                          << " fovStable=" << fovFootprintsStable
                          << " firing=" << firingViewmodelChangedChannels
                          << " impactRendered=" << renderedImpacts
                          << " enemyContrast=" << unsigned(brightest) - unsigned(darkest)
                          << " enemyChanged=" << enemyChangedChannels
                          << " worker=" << workerChangedChannels
                          << " guard=" << guardChangedChannels
                          << " officer=" << officerChangedChannels
                          << " engineer=" << engineerChangedChannels
                          << " robot=" << industrialRobotChangedChannels << '\n';
                std::cerr << "UI gates: map=" << mapHudChangedChannels
                          << " route=" << mapRouteChangedChannels << " footprint=" << mapFootprintChangedChannels
                          << " exploration=" << mapExplorationChangedChannels
                          << " timer=" << runTimerChangedChannels << " campaign=" << campaignTimerChangedChannels
                          << " record=" << speedrunRecordChangedChannels
                          << " main=" << mainMenuChangedChannels << " settings=" << settingsMenuChangedChannels
                          << " selection=" << settingsSelectionChangedChannels
                          << " hud=" << functionalHudChangedChannels << '\n';
                std::cerr << "Renderer invariants: roomExact=" << (roomPixels == repeatedRoomPixels)
                          << " potatoExact=" << (potatoReferencePixels == potatoQualityPixels)
                          << " shadowClampExact=" << (clampedShadowPixels == shadowedPixels)
                          << " instancingEquivalent=" << corridorInstancingPreservedPixels
                          << " instancingReduced=" << corridorInstancingReducedDraws
                          << " instancingDelta=" << instancingComparison.maximumDelta
                          << " instancingChanged=" << instancingComparison.changedChannels
                          << " gpuTiming=" << measuredGpuFrame
                          << " gpuSamples=" << performanceFrameStats.gpuTimingAvailable
                          << " uniformSkips=" << performanceFrameStats.uniformSkips
                          << " textureSkips=" << performanceFrameStats.textureBindSkips
                          << " meshSkips=" << performanceFrameStats.meshBindSkips
                          << " darkBeam=" << darkBeamPixels << '\n';
            }
            // Exercise the gameplay HUD at real drawable sizes after the small
            // fixed-scene checks; these snapshots do not change combat state.
            passed = validateBossHud(renderer) && passed;
            passed = validateCampaignExitHud(renderer) && passed;
            passed = validateFrontendMenus(renderer) && passed;
            passed = validateHighResolutionSettingsTransition(renderer) && passed;
            passed = validateTacticalHud(renderer) && passed;
            passed = validateNewWeaponVisuals(renderer) && passed;
            passed = validateSniperScope(renderer) && passed;
            passed = validatePlayerBody(renderer) && passed;
            passed = validateInstancingTolerance() && passed;
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
