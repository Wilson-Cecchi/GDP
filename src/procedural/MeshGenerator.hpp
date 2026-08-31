#pragma once

#include "procedural/AssetRecipe.hpp"
#include "render/Mesh.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace wkc::procedural {

struct MeshPrimitiveRange {
    std::uint32_t firstIndex{0};
    std::uint32_t indexCount{0};
    MaterialSlot material{MaterialSlot::Primary};
    SemanticRegion semantic{SemanticRegion::Generic};
};

struct MeshBuildResult {
    wkc::render::MeshData mesh;
    std::vector<MeshPrimitiveRange> ranges;
};

[[nodiscard]] std::optional<MeshBuildResult> generateMesh(const MeshRecipe& recipe);

} // namespace wkc::procedural
