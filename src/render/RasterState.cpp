#include "render/RasterState.hpp"

namespace wkc::render {

bool BackfaceCullingStateCache::needsUpdate(const bool enabled) noexcept
{
    ++stats_.requests;
    if (stateKnown_ && enabled_ == enabled) {
        ++stats_.unchangedRequests;
        return false;
    }
    enabled_ = enabled;
    stateKnown_ = true;
    ++stats_.stateChanges;
    return true;
}

void BackfaceCullingStateCache::beginFrame() noexcept
{
    stateKnown_ = false;
    stats_ = {};
}

BackfaceCullingStateStats BackfaceCullingStateCache::stats() const noexcept
{
    return stats_;
}

} // namespace wkc::render
