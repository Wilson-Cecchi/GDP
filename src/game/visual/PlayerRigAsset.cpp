#include "game/visual/PlayerRigAsset.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <numbers>
#include <span>
#include <string_view>

namespace gdp::game::visual {
namespace {

constexpr std::array<std::string_view, playerRigClipCount> requiredClips{
    "Idle", "Walk", "Jog", "Crouch", "Jump", "Draw_Primary_Back"};

class Reader {
public:
    explicit Reader(const std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    [[nodiscard]] bool bytes(const std::size_t count, std::string& out)
    {
        if (count > bytes_.size()-cursor_) return false;
        out.assign(reinterpret_cast<const char*>(bytes_.data()+cursor_), count);
        cursor_ += count;
        return true;
    }
    [[nodiscard]] bool u32(std::uint32_t& out)
    {
        if (4U > bytes_.size()-cursor_) return false;
        out = static_cast<std::uint32_t>(bytes_[cursor_])
            | static_cast<std::uint32_t>(bytes_[cursor_+1U]) << 8U
            | static_cast<std::uint32_t>(bytes_[cursor_+2U]) << 16U
            | static_cast<std::uint32_t>(bytes_[cursor_+3U]) << 24U;
        cursor_ += 4U;
        return true;
    }
    [[nodiscard]] bool f32(float& out)
    {
        std::uint32_t bits{};
        if (!u32(bits)) return false;
        out = std::bit_cast<float>(bits);
        return std::isfinite(out);
    }
    [[nodiscard]] bool done() const noexcept { return cursor_ == bytes_.size(); }
private:
    std::span<const std::uint8_t> bytes_;
    std::size_t cursor_{0U};
};

bool readVertex(Reader& reader, wkc::render::Vertex& vertex)
{
    return reader.f32(vertex.position.x) && reader.f32(vertex.position.y)
        && reader.f32(vertex.position.z) && reader.f32(vertex.normal.x)
        && reader.f32(vertex.normal.y) && reader.f32(vertex.normal.z)
        && reader.f32(vertex.uv.x) && reader.f32(vertex.uv.y);
}

void setReason(std::string* const reason, const std::string_view message)
{
    if (reason != nullptr) *reason = message;
}

PlayerRigPalette blend(const PlayerRigPalette& a, const PlayerRigPalette& b,
    const float weight) noexcept
{
    PlayerRigPalette out{};
    const float t = std::clamp(weight, 0.0F, 1.0F);
    for (std::size_t bone = 0; bone < out.size(); ++bone)
        for (std::size_t element = 0; element < 16U; ++element)
            out[bone].elements[element] = a[bone].elements[element]*(1.0F-t)
                + b[bone].elements[element]*t;
    return out;
}

PlayerRigPalette sample(const PlayerRigClip& clip, const float progress) noexcept
{
    if (clip.frames.empty()) {
        PlayerRigPalette identity{};
        identity.fill(wkc::core::Mat4::identity());
        return identity;
    }
    const float position = std::clamp(progress, 0.0F, 1.0F)
        * static_cast<float>(clip.frames.size()-1U);
    const auto first = static_cast<std::size_t>(position);
    const auto second = std::min(first+1U,clip.frames.size()-1U);
    return blend(clip.frames[first],clip.frames[second],
        position-static_cast<float>(first));
}

const PlayerRigClip& clip(const PlayerRigAsset& asset,
    const std::string_view name) noexcept
{
    for (const auto& candidate : asset.clips)
        if (candidate.name == name) return candidate;
    return asset.clips[0]; // The loader requires all six names.
}

} // namespace

std::optional<PlayerRigAsset> loadPlayerRigAsset(
    const std::filesystem::path& path, std::string* const reason)
{
    std::error_code error;
    const auto fileSize = std::filesystem::file_size(path,error);
    if (error || fileSize < 20U || fileSize > 32U*1024U*1024U) {
        setReason(reason,"Player rig file size is invalid"); return std::nullopt;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { setReason(reason,"cannot open Player rig"); return std::nullopt; }
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
    if (bytes.size() != fileSize) {
        setReason(reason,"Player rig file size is invalid"); return std::nullopt;
    }
    Reader reader(bytes);
    std::string magic;
    std::uint32_t boneCount{}, meshCount{}, clipCount{};
    if (!reader.bytes(8U,magic) || magic != "ISPLYR01"
        || !reader.u32(boneCount) || !reader.u32(meshCount) || !reader.u32(clipCount)
        || boneCount != playerRigBoneCount || meshCount != playerRigMaterialCount
        || clipCount != playerRigClipCount) {
        setReason(reason,"Player rig header or counts are invalid"); return std::nullopt;
    }
    PlayerRigAsset asset;
    std::array<bool, playerRigMaterialCount> materials{};
    for (std::uint32_t meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
        std::uint32_t material{}, vertexCount{}, indexCount{};
        if (!reader.u32(material) || !reader.u32(vertexCount)
            || !reader.u32(indexCount) || material >= playerRigMaterialCount
            || materials[material] || vertexCount == 0U || vertexCount > 200000U
            || indexCount == 0U || indexCount > 600000U || indexCount%3U != 0U) {
            setReason(reason,"Player rig mesh header is invalid"); return std::nullopt;
        }
        materials[material] = true;
        auto& mesh = asset.meshes[material];
        mesh.vertices.resize(vertexCount);
        wkc::core::Vec3 low{std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
        wkc::core::Vec3 high{-low.x,-low.y,-low.z};
        for (auto& vertex : mesh.vertices) {
            if (!readVertex(reader,vertex)) {
                setReason(reason,"Player rig vertex is truncated or nonfinite");
                return std::nullopt;
            }
            const float first = std::floor(vertex.uv.x*.5F);
            const float weight = vertex.uv.x-2.0F*first;
            if (first < 0.0F || first >= static_cast<float>(playerRigBoneCount)
                || vertex.uv.y < 0.0F
                || vertex.uv.y >= static_cast<float>(playerRigBoneCount)
                || std::floor(vertex.uv.y) != vertex.uv.y
                || weight < -0.0001F || weight > 1.0001F) {
                setReason(reason,"Player rig skin influence is invalid");
                return std::nullopt;
            }
            low.x = std::min(low.x,vertex.position.x);
            low.y = std::min(low.y,vertex.position.y);
            low.z = std::min(low.z,vertex.position.z);
            high.x = std::max(high.x,vertex.position.x);
            high.y = std::max(high.y,vertex.position.y);
            high.z = std::max(high.z,vertex.position.z);
        }
        mesh.indices.resize(indexCount);
        for (auto& index : mesh.indices)
            if (!reader.u32(index) || index >= vertexCount) {
                setReason(reason,"Player rig triangle index is invalid");
                return std::nullopt;
            }
        mesh.bounds = {low,high};
        mesh.backfaceCullingSafe = true;
        if (!wkc::render::validateMeshData(mesh)) {
            setReason(reason,"Player rig mesh failed validation");
            return std::nullopt;
        }
    }
    for (std::uint32_t clipIndex = 0; clipIndex < clipCount; ++clipIndex) {
        std::string paddedName;
        std::uint32_t frameCount{}, startFrame{};
        if (!reader.bytes(32U,paddedName)
            || !reader.u32(frameCount) || !reader.u32(startFrame)
            || frameCount == 0U || frameCount > 240U || startFrame > 240U) {
            setReason(reason,"Player rig clip header is invalid");
            return std::nullopt;
        }
        const auto end = paddedName.find('\0');
        if (end == std::string::npos) {
            setReason(reason,"Player rig clip name is unterminated");
            return std::nullopt;
        }
        auto& destination = asset.clips[clipIndex];
        destination.name = paddedName.substr(0U,end);
        destination.startFrame = startFrame;
        destination.frames.resize(frameCount);
        for (auto& frame : destination.frames)
            for (auto& matrix : frame)
                for (float& component : matrix.elements)
                    if (!reader.f32(component)) {
                        setReason(reason,"Player rig matrix is truncated or nonfinite");
                        return std::nullopt;
                    }
    }
    if (!reader.done()) {
        setReason(reason,"Player rig has unexpected trailing data");
        return std::nullopt;
    }
    for (const auto required : requiredClips) {
        if (std::count_if(asset.clips.begin(),asset.clips.end(),
                [required](const auto& item) { return item.name == required; }) != 1) {
            setReason(reason,"Player rig clip set is incomplete");
            return std::nullopt;
        }
    }
    return asset;
}

PlayerRigPalette samplePlayerRigPalette(const PlayerRigAsset& asset,
    const PlayerBodyRenderData& body,
    const WeaponViewmodelRenderData& weapon) noexcept
{
    PlayerRigPalette result = sample(clip(asset,"Idle"),0.0F);
    const float weight = std::isfinite(body.gaitWeight)
        ? std::clamp(body.gaitWeight,0.0F,1.0F) : 0.0F;
    const float phase = std::isfinite(body.gaitPhase)
        ? std::fmod(std::fmod(body.gaitPhase,2.0F*std::numbers::pi_v<float>)
            + 2.0F*std::numbers::pi_v<float>,2.0F*std::numbers::pi_v<float>)
        : 0.0F;
    float progress = phase/(2.0F*std::numbers::pi_v<float>);
    if (std::isfinite(body.forwardMovement) && body.forwardMovement < -0.25F)
        progress = 1.0F-progress;
    else if (std::isfinite(body.rightMovement)
        && std::abs(body.rightMovement) > 0.25F
        && (!std::isfinite(body.forwardMovement)
            || std::abs(body.forwardMovement) < 0.25F))
        progress = std::fmod(progress+(body.rightMovement > 0.0F ? 0.25F : 0.75F),1.0F);
    if (weight > 0.0F) {
        const auto walk = sample(clip(asset,"Walk"),progress);
        const auto jog = sample(clip(asset,"Jog"),progress);
        const float jogBlend = std::clamp((weight-0.75F)*4.0F,0.0F,1.0F);
        result = blend(result,blend(walk,jog,jogBlend),weight);
    }
    if (std::isfinite(body.crouchAmount) && body.crouchAmount > 0.0F)
        result = blend(result,sample(clip(asset,"Crouch"),1.0F),body.crouchAmount);
    if (std::isfinite(body.airborneStrength) && body.airborneStrength > 0.0F)
        result = blend(result,sample(clip(asset,"Jump"),0.5F),body.airborneStrength);
    const bool primary = weapon.role == HudWeaponRole::Carbine
        || weapon.role == HudWeaponRole::Sniper
        || weapon.role == HudWeaponRole::Shotgun
        || weapon.role == HudWeaponRole::SubmachineGun;
    if (weapon.equipTransitionActive && primary && !weapon.grenadeSelected
        && std::isfinite(weapon.equipTransitionProgress))
        result = blend(result,sample(clip(asset,"Draw_Primary_Back"),
            weapon.equipTransitionProgress),1.0F);
    return result;
}

} // namespace gdp::game::visual
