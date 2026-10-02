#include "game/visual/PlayerBodyAssets.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace gdp::game::visual {
namespace {
using namespace wkc::core;
constexpr float pi = std::numbers::pi_v<float>;
constexpr float thighLength = 0.44F, calfLength = 0.43F;

bool finite(const Vec3 p) noexcept
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
float safe(const float x, const float low, const float high) noexcept
{
    return std::isfinite(x) ? std::clamp(x, low, high) : 0.0F;
}
float approach(const float x, const float target, const float step) noexcept
{
    return x + std::clamp(target - x, -step, step);
}
Vec3 transform(const Mat4& m, const Vec3 p) noexcept
{
    const auto& a = m.elements;
    return {a[0]*p.x+a[4]*p.y+a[8]*p.z+a[12],
        a[1]*p.x+a[5]*p.y+a[9]*p.z+a[13],
        a[2]*p.x+a[6]*p.y+a[10]*p.z+a[14]};
}
Mat4 alignDown(const Vec3 direction) noexcept
{
    // Shortest quaternion from the bind bone (-Y) to the posed bone.
    const Vec3 target = normalized(direction);
    const float cosine = std::clamp(-target.y, -1.0F, 1.0F);
    if (cosine < -0.9999F) return rotationXMatrix(pi);
    const Vec3 axis = cross({0.0F, -1.0F, 0.0F}, target);
    const float inverse = 1.0F / std::sqrt(2.0F * (1.0F + cosine));
    const float x = axis.x*inverse, y = axis.y*inverse, z = axis.z*inverse;
    const float w = (1.0F + cosine)*inverse;
    return {{1-2*(y*y+z*z), 2*(x*y+z*w), 2*(x*z-y*w), 0,
        2*(x*y-z*w), 1-2*(x*x+z*z), 2*(y*z+x*w), 0,
        2*(x*z+y*w), 2*(y*z-x*w), 1-2*(x*x+y*y), 0,
        0,0,0,1}};
}
Mat4 bone(const Vec3 bindStart, const Vec3 posedStart, const Vec3 posedEnd) noexcept
{
    return translationMatrix(posedStart) * alignDown(posedEnd - posedStart)
        * translationMatrix(bindStart * -1.0F);
}

struct Ring { float y, radiusX, radiusZ, centerZ; };
using Meshes = std::array<wkc::render::MeshData, playerBodyMaterialCount>;

// Closed rounded surfaces. For these GDP-only meshes UV holds two bone indices
// and a blend weight; the GDP vertex shader derives cloth UV from bind position.
void surface(wkc::render::MeshData& mesh, const float centerX,
    const std::vector<Ring>& rings, const unsigned int leg, const bool trousers,
    const unsigned int fixedBone = 0U, const bool capTop = true)
{
    constexpr std::uint32_t sides = 16U;
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto skin = [=](const float y) {
        const unsigned int thigh = leg == 0U ? 1U : 4U;
        const unsigned int calf = thigh + 1U;
        if (!trousers) return Vec2{2.0F * static_cast<float>(fixedBone) + 1.0F,
            static_cast<float>(fixedBone)};
        if (y > 0.89F) return Vec2{2.0F * static_cast<float>(thigh)
            + std::clamp((1.04F-y)/0.15F, 0.0F, 1.0F), 0.0F};
        return Vec2{2.0F * static_cast<float>(calf)
            + std::clamp((0.65F-y)/0.18F, 0.0F, 1.0F), static_cast<float>(thigh)};
    };
    for (const auto& ring : rings) {
        for (std::uint32_t s = 0; s <= sides; ++s) {
            const float angle = static_cast<float>(s) / static_cast<float>(sides) * 2.0F*pi;
            const float x = std::cos(angle), z = std::sin(angle);
            mesh.vertices.push_back({{centerX + x*ring.radiusX, ring.y,
                ring.centerZ + z*ring.radiusZ}, normalized({x/ring.radiusX, 0.08F, z/ring.radiusZ}),
                skin(ring.y), fixedBone});
        }
    }
    for (std::uint32_t r = 0; r + 1U < rings.size(); ++r) {
        for (std::uint32_t s = 0; s < sides; ++s) {
            const auto a = base+r*(sides+1U)+s, b = a+sides+1U;
            mesh.indices.insert(mesh.indices.end(), {a,b,a+1U,a+1U,b,b+1U});
        }
    }
    for (const auto end : {0U, static_cast<unsigned int>(rings.size()-1U)}) {
        if (!capTop && end != 0U) continue;
        const auto& ring = rings[end];
        const auto cap = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({{centerX,ring.y,ring.centerZ},
            {0.0F,end == 0U ? -1.0F : 1.0F,0.0F},skin(ring.y),fixedBone});
        for (std::uint32_t s = 0; s < sides; ++s) {
            const auto a = base+end*(sides+1U)+s;
            if (end == 0U) mesh.indices.insert(mesh.indices.end(), {cap,a,a+1U});
            else mesh.indices.insert(mesh.indices.end(), {cap,a+1U,a});
        }
    }
}
} // namespace

void updatePlayerBodyMotion(PlayerBodyMotion& motion, const Vec3 previousFeet,
    const Vec3 currentFeet, const float yawDegrees, const bool grounded,
    const bool alive, const float seconds) noexcept
{
    if (!finite(previousFeet) || !finite(currentFeet) || !std::isfinite(yawDegrees)
        || !std::isfinite(seconds) || seconds <= 0.0F || seconds > 0.25F) return;
    if (!std::isfinite(motion.phaseRadians) || !std::isfinite(motion.weight)
        || !std::isfinite(motion.forward) || !std::isfinite(motion.right)
        || !std::isfinite(motion.airborne)) motion = {};
    const Vec3 delta = currentFeet - previousFeet;
    const float distance = length({delta.x,0.0F,delta.z});
    if (!std::isfinite(distance) || distance > 0.50F || std::abs(delta.y) > 0.50F || !alive) {
        motion = {}; return;
    }
    const bool walking = grounded && distance > 1.0e-5F;
    motion.weight = approach(safe(motion.weight,0,1),walking ? 1.0F : 0.0F,7.0F*seconds);
    motion.airborne = approach(safe(motion.airborne,0,1),grounded ? 0.0F : 1.0F,8.0F*seconds);
    if (!walking) return;
    motion.phaseRadians = std::fmod(motion.phaseRadians + distance/3.20F*2.0F*pi,2.0F*pi);
    const float yaw = std::remainder(yawDegrees,360.0F)*pi/180.0F;
    const Vec3 forward{std::cos(yaw),0,std::sin(yaw)}, right{-forward.z,0,forward.x};
    motion.forward = approach(safe(motion.forward,-1,1),dot(delta,forward)/distance,12.0F*seconds);
    motion.right = approach(safe(motion.right,-1,1),dot(delta,right)/distance,12.0F*seconds);
}

PlayerBodyPose samplePlayerBodyPose(const PlayerBodyRenderData& body) noexcept
{
    PlayerBodyPose pose;
    const Vec3 feet = finite(body.feetPosition) ? body.feetPosition : Vec3{};
    const float yaw = std::isfinite(body.yawDegrees) ? std::remainder(body.yawDegrees,360.0F) : -90.0F;
    pose.root = translationMatrix(feet) * rotationYMatrix(-(yaw+90.0F)*pi/180.0F)
        * translationMatrix({0,0,0.09F});
    const float crouch = safe(body.crouchAmount,0,1), air = safe(body.airborneStrength,0,1);
    const float weight = safe(body.gaitWeight,0,1)*(1-air);
    const float phase = std::isfinite(body.gaitPhase) ? std::remainder(body.gaitPhase,2.0F*pi) : 0;
    const float forward = safe(body.forwardMovement,-1,1), right = safe(body.rightMovement,-1,1);
    // Eyes naturally sit ahead of the waist. Keep the upper body behind them,
    // instead of covering the legs with an artificial waist disk looking down.
    const Vec3 pelvisOffset{0,-0.02F-0.50F*crouch-0.07F*air,0.17F+0.11F*crouch};
    pose.skin[0] = translationMatrix(pelvisOffset);
    for (unsigned int leg = 0U; leg < 2U; ++leg) {
        const float x = leg == 0U ? -0.15F : 0.15F;
        const float step = std::sin(phase+static_cast<float>(leg)*pi)*weight;
        const Vec3 bindHip{x,1.0F,0}, bindKnee{x,0.56F,0}, bindAnkle{x,0.13F,0};
        const Vec3 hip = bindHip + pelvisOffset;
        Vec3 ankle{x+step*0.12F*right,0.13F+0.18F*air+std::max(0.0F,step)*0.10F,
            -step*0.18F*forward-0.06F*air};
        const Vec3 direction = normalized(ankle - hip);
        const float distance = std::clamp(length(ankle-hip),0.15F,thighLength+calfLength-0.005F);
        ankle = hip + direction*distance;
        const float along = (thighLength*thighLength-calfLength*calfLength+distance*distance)/(2*distance);
        const float bend = std::sqrt(std::max(0.0F,thighLength*thighLength-along*along));
        const Vec3 kneeForward = normalized(Vec3{0,0,-1} - direction*dot({0,0,-1},direction));
        const Vec3 knee = hip + direction*along + kneeForward*bend;
        pose.hips[leg] = hip; pose.knees[leg] = knee; pose.ankles[leg] = ankle;
        const unsigned int first = leg == 0U ? 1U : 4U;
        pose.skin[first] = bone(bindHip,hip,knee);
        pose.skin[first+1U] = bone(bindKnee,knee,ankle);
        pose.skin[first+2U] = translationMatrix(ankle-bindAnkle);
    }
    return pose;
}

std::array<wkc::render::MeshData, playerBodyMaterialCount> makePlayerBodyMeshes()
{
    Meshes meshes;
    surface(meshes[0],0,{{0.90F,.255F,.14F,0},{0.98F,.26F,.15F,0},
        {1.06F,.235F,.14F,0},{1.20F,.23F,.14F,.04F},
        {1.42F,.22F,.14F,.10F}},0,false,0U,false);
    for (unsigned int leg = 0U; leg < 2U; ++leg) {
        const float x = leg == 0U ? -0.15F : 0.15F;
        const unsigned int bootBone = leg == 0U ? 3U : 6U;
        surface(meshes[0],x,{{.14F,.078F,.08F,0},{.22F,.084F,.085F,0},
            {.31F,.095F,.095F,0},{.43F,.103F,.105F,0},{.51F,.11F,.115F,-.004F},
            {.57F,.115F,.12F,-.004F},{.65F,.12F,.125F,0},{.76F,.13F,.14F,0},
            {.87F,.14F,.145F,0},{.97F,.145F,.14F,0},{1.03F,.12F,.115F,0}},leg,true,0U,false);
        // Rounded knee reinforcement and raised seams, not metal armor.
        surface(meshes[1],x,{{.47F,.093F,.035F,-.093F},
            {.52F,.114F,.038F,-.094F},{.61F,.105F,.037F,-.096F},
            {.65F,.081F,.03F,-.092F}},leg,true);
        surface(meshes[2],x,{{.045F,.088F,.155F,-.055F},
            {.09F,.09F,.15F,-.058F},{.135F,.083F,.13F,-.048F},
            {.17F,.076F,.085F,-.002F},{.21F,.074F,.078F,0}},leg,false,bootBone);
        surface(meshes[3],x,{{0,.089F,.16F,-.055F},{.016F,.094F,.165F,-.055F},
            {.045F,.093F,.162F,-.055F}},leg,false,bootBone);
        // Toe cap and ankle cuff in leather; small parallel lace ridges.
        for (unsigned int lace = 0U; lace < 4U; ++lace) {
            const float y = .117F + static_cast<float>(lace)*.016F;
            surface(meshes[3],x,{{y,.051F,.008F,-.11F},
                {y+.006F,.051F,.008F,-.11F}},leg,false,bootBone);
        }
    }
    for (auto& mesh : meshes) {
        mesh.bounds = {{-0.40F,-0.01F,-0.25F},{0.40F,1.43F,0.25F}};
        // Outward winding, rigid/positive two-bone blends. Interior cloth must
        // not appear through the open waist at steep pitch / wide FOV.
        mesh.backfaceCullingSafe = true;
    }
    return meshes;
}

WeaponMaterialImages makeUsedPlayerBodyMaterialImages(const PlayerBodyMaterial material)
{
    const bool cloth = material == PlayerBodyMaterial::Cloth;
    const bool reinforcement = material == PlayerBodyMaterial::Reinforcement;
    const bool leather = material == PlayerBodyMaterial::Leather;
    const int resolution = cloth ? 512 : leather || reinforcement ? 256 : 128;
    WeaponMaterialImages images;
    for (auto* image : {&images.albedo,&images.normal,&images.properties}) {
        image->width = resolution; image->height = resolution;
        image->rgba.resize(static_cast<std::size_t>(resolution*resolution*4));
    }
    for (int y = 0; y < resolution; ++y) for (int x = 0; x < resolution; ++x) {
        const auto noise = static_cast<std::uint32_t>(x)*73856093U
            ^ static_cast<std::uint32_t>(y)*19349663U;
        const int grain = static_cast<int>((noise^(noise>>11U))&7U)-3;
        const int weave = cloth || reinforcement ? ((x/2+y/2)%2 == 0 ? 3 : -3) : 0;
        const bool seam = cloth && (x%128 < 2 || (x%128 < 5 && y%9 < 4));
        const int tone = (cloth ? 110 : reinforcement ? 78 : leather ? 57 : 30)
            + grain + weave + (seam ? 14 : 0);
        const auto offset = static_cast<std::size_t>((y*resolution+x)*4);
        const auto byte = [](const int n) {return static_cast<std::uint8_t>(std::clamp(n,0,255));};
        images.albedo.rgba[offset] = byte(tone-4);
        images.albedo.rgba[offset+1U] = byte(tone);
        images.albedo.rgba[offset+2U] = byte(tone+1);
        images.normal.rgba[offset] = byte(128+weave);
        images.normal.rgba[offset+1U] = byte(128+grain+(seam ? 5 : 0));
        images.normal.rgba[offset+2U] = 255U;
        images.properties.rgba[offset] = byte((leather ? 200 : 232)+grain);
        images.properties.rgba[offset+1U] = 0U; // Cloth/leather/rubber never metal.
        images.properties.rgba[offset+2U] = 250U;
        for (auto* image : {&images.albedo,&images.normal,&images.properties})
            image->rgba[offset+3U] = 255U;
    }
    return images;
}

Vec3 deformPlayerBodyVertex(const wkc::render::Vertex& vertex, const PlayerBodyPose& pose) noexcept
{
    const auto first = static_cast<std::size_t>(std::clamp(std::floor(vertex.uv.x*.5F),0.0F,6.0F));
    const auto second = static_cast<std::size_t>(std::clamp(vertex.uv.y,0.0F,6.0F));
    const float weight = std::clamp(vertex.uv.x-2.0F*static_cast<float>(first),0.0F,1.0F);
    return transform(pose.root,transform(pose.skin[first],vertex.position)*weight
        + transform(pose.skin[second],vertex.position)*(1-weight));
}
} // namespace gdp::game::visual
