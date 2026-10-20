#include "game/visual/PlayerRigAsset.hpp"

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
} // namespace

int main(const int argc, const char* const argv[])
{
    try {
        expect(argc == 2,"expected Player rig asset path");
        const std::filesystem::path path{argv[1]};
        std::string reason;
        const auto asset = gdp::game::visual::loadPlayerRigAsset(path,&reason);
        expect(asset.has_value(),reason.c_str());
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
        const auto draw = gdp::game::visual::samplePlayerRigPalette(*asset,idle,
            {.role=gdp::game::visual::HudWeaponRole::Carbine,
                .equipTransitionActive=true,.equipTransitionProgress=0.5F});
        const auto pistol = gdp::game::visual::samplePlayerRigPalette(*asset,idle,
            {.role=gdp::game::visual::HudWeaponRole::Pistol,
                .equipTransitionActive=true,.equipTransitionProgress=0.5F});
        expect(differs(rest,walk) && differs(rest,crouch) && differs(rest,draw),
            "Player rig clips do not change the rest pose");
        expect(!differs(rest,pistol),
            "primary back draw must not play when drawing the pistol");
        std::ifstream input(path,std::ios::binary);
        std::vector<char> bytes{std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        expect(bytes.size() > 128U,"Player rig asset unexpectedly small");
        const auto damaged = std::filesystem::temp_directory_path()
            / "iron_seed_invalid_player_rig.isplayer";
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
