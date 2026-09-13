#include "render/GpuTimer.hpp"

#include <algorithm>

namespace wkc::render {

GpuTimer::~GpuTimer()
{
    reset();
}

bool GpuTimer::initialize(const GlApi& gl) noexcept
{
    reset();
    if (gl.genQueries == nullptr || gl.deleteQueries == nullptr
        || gl.beginQuery == nullptr || gl.endQuery == nullptr
        || gl.getQueryObjectuiv == nullptr
        || gl.getQueryObjectui64v == nullptr) {
        return false;
    }

    gl_ = &gl;
    gl_->genQueries(static_cast<GLsizei>(queries_.size()), queries_.data());
    if (std::any_of(queries_.begin(), queries_.end(),
            [](const GLuint query) { return query == 0U; })) {
        reset();
        return false;
    }
    return true;
}

bool GpuTimer::beginFrame() noexcept
{
    if (!isSupported() || activeQuery_) {
        return false;
    }

    const std::size_t slot = nextQuery_;
    if (pending_[slot]) {
        GLuint available = GL_FALSE;
        gl_->getQueryObjectuiv(
            queries_[slot], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available == GL_FALSE) {
            return false;
        }
        GLuint64 elapsedNanoseconds = 0U;
        gl_->getQueryObjectui64v(
            queries_[slot], GL_QUERY_RESULT, &elapsedNanoseconds);
        latestMilliseconds_ = static_cast<double>(elapsedNanoseconds) / 1'000'000.0;
        pending_[slot] = false;
    }

    gl_->beginQuery(GL_TIME_ELAPSED, queries_[slot]);
    activeQuery_ = slot;
    nextQuery_ = (slot + 1U) % queries_.size();
    return true;
}

void GpuTimer::endFrame() noexcept
{
    if (!isSupported() || !activeQuery_) {
        return;
    }
    gl_->endQuery(GL_TIME_ELAPSED);
    pending_[*activeQuery_] = true;
    activeQuery_.reset();
}

void GpuTimer::reset() noexcept
{
    if (gl_ != nullptr && activeQuery_) {
        gl_->endQuery(GL_TIME_ELAPSED);
    }
    if (gl_ != nullptr && gl_->deleteQueries != nullptr
        && std::any_of(queries_.begin(), queries_.end(),
            [](const GLuint query) { return query != 0U; })) {
        gl_->deleteQueries(
            static_cast<GLsizei>(queries_.size()), queries_.data());
    }
    gl_ = nullptr;
    queries_ = {};
    pending_ = {};
    nextQuery_ = 0U;
    activeQuery_.reset();
    latestMilliseconds_.reset();
}

bool GpuTimer::isSupported() const noexcept
{
    return gl_ != nullptr && queries_.front() != 0U;
}

std::optional<double> GpuTimer::latestMilliseconds() const noexcept
{
    return latestMilliseconds_;
}

} // namespace wkc::render
