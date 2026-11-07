#include "game/visual/PlayerRigAsset.hpp"
#include "game/visual/PlayerHandsAssets.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
void expect(const bool passed, const char* message)
{
    if (!passed) throw std::runtime_error(message);
}
bool differs(const gdp::game::visual::PlayerRigPalette& a,
    const gdp::game::visual::PlayerRigPalette& b)
{
    for (std::size_t i = 0; i < a.size(); ++i)
        for (std::size_t j = 0; j < 16U; ++j)
            if (std::abs(a[i].elements[j]-b[i].elements[j]) > .0001F)
                return true;
    return false;
}
wkc::core::Vec3 point(const wkc::core::Mat4& matrix,
    const wkc::core::Vec3 value)
{
    const auto& a = matrix.elements;
    return {a[0]*value.x+a[4]*value.y+a[8]*value.z+a[12],
        a[1]*value.x+a[5]*value.y+a[9]*value.z+a[13],
        a[2]*value.x+a[6]*value.y+a[10]*value.z+a[14]};
}
} // namespace

int main(const int argc, const char* const argv[])
{
    try {
        expect(argc == 2,"expected Player rig asset path");
        const std::filesystem::path path{argv[1]};
        std::string reason;
        const auto asset = gdp::game::visual::loadPlayerRigAsset(path,&reason);
        expect(asset.has_value(),reason.c_str());
        const bool exosuit = path.filename() == "player_exosuit.isplayer";
        expect(asset->materialCount == (exosuit ? 8U : 6U)
            && asset->partCount == (exosuit ? 5U : 4U)
            && asset->meshes.size() == asset->materialCount*asset->partCount,
            "Player rig material or part layout is invalid");
        if (exosuit) {
            std::size_t helmetTriangles = 0U;
            for (std::size_t i = 4U*asset->materialCount; i < asset->meshes.size(); ++i)
                helmetTriangles += asset->meshes[i].indices.size()/3U;
            expect(helmetTriangles > 100U,"Player exosuit helmet is missing");
        }
        std::size_t triangles = 0U;
        for (const auto& mesh : asset->meshes)
            triangles += mesh.indices.size()/3U;
        expect(triangles > 100U && triangles < 50000U,
            "Player rig triangle count outside preview budget");
        const gdp::game::visual::PlayerBodyRenderData idle{.visible=true};
        const auto rest = gdp::game::visual::samplePlayerRigPalette(*asset,idle,{});
        const auto walk = gdp::game::visual::samplePlayerRigPalette(*asset,
            {.visible=true,.gaitPhase=1.5F,.gaitWeight=1.0F},{});
        const auto crouch = gdp::game::visual::samplePlayerRigPalette(*asset,
            {.visible=true,.crouchAmount=1.0F},{});
        const auto lookDown = gdp::game::visual::samplePlayerRigPalette(*asset,
            {.visible=true,.pitchDegrees=-60.0F},{});
        const auto draw = gdp::game::visual::samplePlayerRigPalette(*asset,idle,
            {.role=gdp::game::visual::HudWeaponRole::Carbine,
                .equipTransitionActive=true,.equipTransitionProgress=0.5F});
        const auto pistol = gdp::game::visual::samplePlayerRigPalette(*asset,idle,
            {.role=gdp::game::visual::HudWeaponRole::Pistol,
                .equipTransitionActive=true,.equipTransitionProgress=0.5F});
        expect(differs(rest,walk) && differs(rest,crouch) && differs(rest,draw)
            && differs(rest,lookDown),
            "Player rig clips do not change the rest pose");
        expect(!differs(rest,pistol),
            "primary back draw must not play when drawing the pistol");
        for (const auto role : {gdp::game::visual::HudWeaponRole::Carbine,
                 gdp::game::visual::HudWeaponRole::Pistol,
                 gdp::game::visual::HudWeaponRole::Knife}) {
            const gdp::game::visual::WeaponViewmodelRenderData weapon{
                .visible=true,.role=role};
            const auto hands = gdp::game::visual::samplePlayerHandsPose(weapon);
            const auto grip = gdp::game::visual::samplePlayerRigGripPalette(hands);
            for (std::size_t side = 0; side < 2U; ++side) {
                const float sign = side == 0U ? 1.0F : -1.0F;
                const std::size_t first = side == 0U ? 6U : 13U;
                const wkc::core::Vec3 bindShoulder{sign*.29F,1.48F,0};
                const wkc::core::Vec3 bindElbow{sign*.43F,1.17F,-.02F};
                const wkc::core::Vec3 bindWrist{sign*.50F,.91F,-.12F};
                expect(wkc::core::nearlyEqual(point(grip[first],bindShoulder),
                        hands.shoulders[side],.001F)
                    && wkc::core::nearlyEqual(point(grip[first],bindElbow),
                        hands.elbows[side],.001F)
                    && wkc::core::nearlyEqual(point(grip[first+1U],bindWrist),
                        hands.wrists[side],.001F)
                    && wkc::core::nearlyEqual(point(grip[first+2U],bindWrist),
                        hands.wrists[side],.001F),
                    "Blender arm joints do not meet the weapon contact rig");
            }
        }
        std::ifstream input(path,std::ios::binary);
        std::vector<char> bytes{std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        expect(bytes.size() > 128U,"Player rig asset unexpectedly small");
        const auto damaged = std::filesystem::temp_directory_path()
            / ("iron_seed_invalid_" + path.filename().string());
        {
            std::ofstream output(damaged,std::ios::binary);
            output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()/2U));
        }
        expect(!gdp::game::visual::loadPlayerRigAsset(damaged),
            "truncated Player rig asset was accepted");
        std::filesystem::remove(damaged);
        std::cout << "Player rig asset: " << triangles
            << " triangles, six clips, gait/crouch/draw and truncation validated\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
