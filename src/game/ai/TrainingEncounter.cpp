#include "game/ai/TrainingEncounter.hpp"
#include "core/Random.hpp"
#include <algorithm>
#include <stdexcept>
namespace gdp::game::ai {
TrainingEncounter makeTrainingEncounter(wkc::core::Aabb bounds,
    wkc::core::Seed seed, unsigned variant)
{
    if (variant < 1 || variant > 5
        || bounds.maximum.x - bounds.minimum.x < 6
        || bounds.maximum.z - bounds.minimum.z < 6
        || bounds.maximum.y - bounds.minimum.y < 2.6F)
        throw std::runtime_error("invalid diverse training encounter bounds or variant");
    wkc::core::RandomStream random{seed};
    const wkc::core::Vec3 center{(bounds.minimum.x + bounds.maximum.x) * 0.5F,
        bounds.minimum.y, (bounds.minimum.z + bounds.maximum.z) * 0.5F};
    const float distance = std::min(3.0F,
        (bounds.maximum.z - bounds.minimum.z) * 0.5F - 0.8F);
    wkc::core::Vec3 player = center + wkc::core::Vec3{0,0,-distance};
    const wkc::core::Vec3 enemy = center + wkc::core::Vec3{random.rangeFloat(-0.15F,0.15F),0,distance};
    std::vector<wkc::core::Aabb> solids;
    float peekWidth = 0;
    if (variant == 4) {
        // Enemies do not climb. Keep the platform edge within Worker attack
        // range from an occupiable floor position; no fictitious stairs.
        const float height = random.rangeFloat(0.45F, 0.65F);
        solids.push_back({{player.x-0.45F,center.y,player.z-0.45F},
            {player.x+0.45F,center.y+height,player.z+0.45F}});
        player.y += height;
    } else {
        const float roomHalfWidth = (bounds.maximum.x - bounds.minimum.x) * 0.5F;
        const float halfWidth = std::min(random.rangeFloat(variant == 5 ? 1.0F : 0.65F,
            variant == 5 ? 1.3F : 0.95F), (roomHalfWidth - 0.4F) * 0.32F);
        const float offset = variant == 1 ? -0.45F : variant == 2 ? 0.45F : 0;
        solids.push_back({{center.x+offset-halfWidth,center.y,center.z-0.5F},
            {center.x+offset+halfWidth,center.y+2.4F,center.z+0.5F}});
        if (variant == 3 || variant == 5)
            peekWidth = std::min(roomHalfWidth - 0.4F, (halfWidth + 0.3F) * 2.6F);
    }
    gameplay::CollisionWorld collision{bounds,std::move(solids)};
    if (!collision.canOccupy(player,{.radius=0.35F,.height=1.8F})
        || !collision.canOccupy(enemy,{.radius=0.52F,.height=1.9F}))
        throw std::runtime_error("diverse training encounter has an invalid spawn");
    return {std::move(collision), player, enemy, peekWidth, variant == 4};
}
}
