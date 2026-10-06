#include "core/Seed.hpp"
#include "game/SeedDomains.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testStableDomainIds()
{
    using gdp::game::SeedDomain;
    constexpr std::array expected{
        std::pair{SeedDomain::World, std::uint64_t{1}},
        std::pair{SeedDomain::Graph, std::uint64_t{2}},
        std::pair{SeedDomain::Room, std::uint64_t{3}},
        std::pair{SeedDomain::RoomModule, std::uint64_t{4}},
        std::pair{SeedDomain::Corridor, std::uint64_t{5}},
        std::pair{SeedDomain::Props, std::uint64_t{6}},
        std::pair{SeedDomain::Lighting, std::uint64_t{7}},
        std::pair{SeedDomain::EnemySpawn, std::uint64_t{8}},
        std::pair{SeedDomain::EnemyVisual, std::uint64_t{9}},
        std::pair{SeedDomain::EnemyBody, std::uint64_t{10}},
        std::pair{SeedDomain::EnemyParts, std::uint64_t{11}},
        std::pair{SeedDomain::EnemyMaterial, std::uint64_t{12}},
        std::pair{SeedDomain::EnemyWear, std::uint64_t{13}},
        std::pair{SeedDomain::Loot, std::uint64_t{14}},
        std::pair{SeedDomain::Events, std::uint64_t{15}},
        std::pair{SeedDomain::Audio, std::uint64_t{16}},
        std::pair{SeedDomain::RoomMaterial, std::uint64_t{17}},
        std::pair{SeedDomain::NeuralWorker, std::uint64_t{18}},
        std::pair{SeedDomain::MapPlacement, std::uint64_t{19}},
        std::pair{SeedDomain::CampaignLayout, std::uint64_t{20}},
        std::pair{SeedDomain::CampaignMission, std::uint64_t{21}},
    };

    for (const auto& [domain, value] : expected) {
        expect(static_cast<std::uint64_t>(domain) == value,
            "GDP seed domain keeps its serialized numeric ID");
    }
}

void testGenericDerivationCompatibility()
{
    constexpr wkc::core::Seed root = 0x123456789ABCDEF0ULL;
    const wkc::core::Seed typed = wkc::core::deriveSeed(
        root, gdp::game::SeedDomain::Room, 42U);
    const wkc::core::Seed numeric = wkc::core::deriveSeed(
        root, wkc::core::SeedDomainId{3}, 42U);

    expect(typed == numeric,
        "typed GDP domain delegates to the generic seed mechanism");
    expect(typed == 0x75247D92702F490EULL,
        "GDP room seed golden value remains stable");
}

} // namespace

int main()
{
    testStableDomainIds();
    testGenericDerivationCompatibility();

    if (failures != 0) {
        std::cerr << failures << " seed domain test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP seed domain tests passed\n";
    return 0;
}
