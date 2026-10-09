#include "game/visual/PlayerHandsAssets.hpp"
#include "gameplay/Weapon.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace gdp::game::visual {
namespace {
using namespace wkc::core;
using wkc::render::MeshData;
constexpr float pi = std::numbers::pi_v<float>;
constexpr float gloveScale = 2.4F;
// Near-pass proportions account for the established enlarged weapon space.
// Shoulders remain below the frame instead of shortening a reaching arm in air.
constexpr float upperLength = .56F, foreLength = .46F;
constexpr Vec3 bindShoulder{0,-upperLength-foreLength,0}, bindElbow{0,-foreLength,0};
constexpr std::array<float,5> digitX{.0385F,.013F,-.013F,-.0365F,.039F};
constexpr std::array<float,5> digitLength{.064F,.071F,.066F,.054F,.056F};
constexpr std::array<float,3> segmentFraction{.44F,.33F,.23F};
constexpr Vec3 palmContact{0,.132F,-.0624F};
float safe(const float x) noexcept { return std::isfinite(x) ? std::clamp(x,0.0F,1.0F) : 0.0F; }
float ease(const float x, const float a, const float b) noexcept
{
    const float t = std::clamp((x-a)/(b-a),0.0F,1.0F);
    return t*t*(3-2*t);
}
Vec3 point(const Mat4& m, const Vec3 p) noexcept
{
    const auto& a=m.elements;
    return {a[0]*p.x+a[4]*p.y+a[8]*p.z+a[12],
        a[1]*p.x+a[5]*p.y+a[9]*p.z+a[13], a[2]*p.x+a[6]*p.y+a[10]*p.z+a[14]};
}
Vec3 inverseNearPoint(const Mat4& m, const Vec3 p) noexcept
{
    // The shared presentation frame has only rotation and uniform positive scale.
    const auto& a=m.elements;
    const Vec3 v=p-Vec3{a[12],a[13],a[14]};
    const float scaleSquared=a[0]*a[0]+a[1]*a[1]+a[2]*a[2];
    return {dot(v,{a[0],a[1],a[2]})/scaleSquared,
        dot(v,{a[4],a[5],a[6]})/scaleSquared,dot(v,{a[8],a[9],a[10]})/scaleSquared};
}
Mat4 basis(const Vec3 x, const Vec3 y, const Vec3 z) noexcept
{
    return {{x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,0,0,0,1}};
}
Mat4 alongY(const Vec3 v) noexcept
{
    const Vec3 y=normalized(v);
    const Vec3 x=normalized(cross(std::abs(y.z) < .9F ? Vec3{0,0,1} : Vec3{1,0,0},y));
    return basis(x,y,cross(x,y));
}
Mat4 alongYWithRoll(const Vec3 direction, const Vec3 referenceX) noexcept
{
    const Vec3 y=normalized(direction);
    const Vec3 transverse=referenceX-y*dot(referenceX,y);
    if(length(transverse)<.0001F) return alongY(direction);
    const Vec3 x=normalized(transverse);
    return basis(x,y,cross(x,y));
}
Vec3 column(const Mat4& m, const unsigned int index) noexcept
{
    return {m.elements[index*4U],m.elements[index*4U+1U],m.elements[index*4U+2U]};
}
Mat4 limitWrist(const Mat4& desired, const Vec3 forearm) noexcept
{
    const Vec3 y=column(desired,1U);
    const float angle=std::acos(std::clamp(dot(y,forearm),-1.0F,1.0F));
    constexpr float limit=30*pi/180;
    if(angle<=limit) return desired;
    Vec3 axis=normalized(cross(y,forearm));
    if(length(axis)<.0001F) axis=column(desired,0U);
    const float cosine=std::cos(angle-limit),sine=std::sin(angle-limit);
    const auto rotate=[&](const Vec3 p) {
        return p*cosine+cross(axis,p)*sine+axis*dot(axis,p)*(1-cosine);
    };
    return basis(rotate(column(desired,0U)),rotate(y),rotate(column(desired,2U)));
}
struct ArmChain { Vec3 shoulder, elbow; };
ArmChain solveArm(const Mat4& frame, const Vec3 wrist, const bool dominant) noexcept
{
    const Vec3 anchor=inverseNearPoint(frame,
        dominant ? Vec3{.22F,-.35F,.22F} : Vec3{-.02F,-.35F,.24F});
    constexpr float reach=upperLength+foreLength-.004F;
    Vec3 shoulder=anchor;
    const float originalDistance=length(wrist-anchor);
    if(originalDistance>reach) shoulder=wrist+normalized(anchor-wrist)*reach;
    // Keep the sleeve root below the near-pass lower edge even at the most
    // distant contact. Project onto the reachable sphere/edge-plane circle,
    // not into the camera or an arbitrarily shortened forearm.
    const auto& a=frame.elements;
    const Vec3 n{a[1]+.675F*a[2],a[5]+.675F*a[6],a[9]+.675F*a[10]};
    const Vec3 unit=normalized(n);
    const float offset=a[13]+.675F*a[14];
    constexpr float edge=-.16F;
    if(dot(n,shoulder)+offset>edge) {
        const float k=std::clamp((edge-offset-dot(n,wrist))/length(n),-reach,reach);
        Vec3 tangent=(anchor-wrist)-unit*dot(anchor-wrist,unit);
        if(length(tangent)<.0001F) tangent=column(alongY(unit),0U);
        shoulder=wrist+unit*k+normalized(tangent)*std::sqrt(std::max(0.0F,reach*reach-k*k));
    }
    const Vec3 direction=normalized(wrist-shoulder);
    const float distance=std::clamp(length(wrist-shoulder),
        std::abs(upperLength-foreLength)+.001F,reach);
    shoulder=wrist-direction*distance;
    const float along=(upperLength*upperLength-foreLength*foreLength+distance*distance)/(2*distance);
    const float bend=std::sqrt(std::max(0.0F,upperLength*upperLength-along*along));
    // A stable body-side pole avoids an elbow-plane flip when the desired
    // palm axis crosses the shoulder/wrist line. Wrist limits handle contact
    // orientation; the arm must not choose a new side at that singularity.
    const Vec3 down=inverseNearPoint(frame,{dominant ? .34F : -.24F,-.52F,.24F})-shoulder;
    const Vec3 pole=down-direction*dot(down,direction);
    return {shoulder,shoulder+direction*along+normalized(pole)*bend};
}
Vec3 digitRoot(const unsigned int digit, const unsigned int side) noexcept
{
    constexpr std::array<float,5> rootY{.073F,.078F,.075F,.065F,.021F};
    return Vec3{digitX[digit]*(side == 0U ? 1.0F : -1.0F),rootY[digit],0}*gloveScale;
}
struct Ring {
    float y, x, z;
    unsigned int joint, parent;
    float weight;
    float centerX{0}, centerZ{0};
};

void surface(MeshData& mesh, const Vec3 origin, const std::vector<Ring>& rings,
    const unsigned int sides=12U, const float scale=1.0F, const float roundness=1.0F)
{
    const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::size_t i=0;i<rings.size();++i) for (unsigned int s=0;s<=sides;++s) {
        const auto r=rings[i];
        const auto before=rings[i==0 ? i : i-1U],after=rings[i+1U<rings.size() ? i+1U : i];
        const float dy=std::max(.0001F,after.y-before.y);
        const float angle=float(s)/float(sides)*2*pi;
        const float cosine=std::cos(angle),sine=std::sin(angle);
        const float x=std::copysign(std::pow(std::abs(cosine),roundness),cosine);
        const float z=std::copysign(std::pow(std::abs(sine),roundness),sine);
        const float nx=std::copysign(std::pow(std::abs(cosine),2-roundness),cosine)/r.x;
        const float nz=std::copysign(std::pow(std::abs(sine),2-roundness),sine)/r.z;
        const float slope=nx*(x*(after.x-before.x)+after.centerX-before.centerX)/dy
            +nz*(z*(after.z-before.z)+after.centerZ-before.centerZ)/dy;
        mesh.vertices.push_back({origin+Vec3{x*r.x+r.centerX,r.y,z*r.z+r.centerZ}*scale,
            normalized({nx,-slope,nz}),{float(r.joint)*2+r.weight,float(r.parent)},r.joint});
    }
    for (unsigned int r=0;r+1U<rings.size();++r) for (unsigned int s=0;s<sides;++s) {
        const auto a=base+r*(sides+1U)+s,b=a+sides+1U;
        mesh.indices.insert(mesh.indices.end(),{a,b,a+1U,a+1U,b,b+1U});
    }
    for (const auto end:{0U,static_cast<unsigned int>(rings.size()-1U)}) {
        const auto r=rings[end];
        const auto cap=static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({origin+Vec3{r.centerX,r.y,r.centerZ}*scale,{0,end==0U ? -1.0F : 1.0F,0},
            {float(r.joint)*2+r.weight,float(r.parent)},r.joint});
        for (unsigned int s=0;s<sides;++s) {
            const auto a=base+end*(sides+1U)+s;
            if(end==0U) mesh.indices.insert(mesh.indices.end(),{cap,a,a+1U});
            else mesh.indices.insert(mesh.indices.end(),{cap,a+1U,a});
        }
    }
}

// Split the outer fabric from the leather grip surface, not separate overlapping
// mitten meshes. The skin encoding stays in the existing engine vertex layout.
void gloveSurface(std::array<MeshData,playerHandsMaterialCount>& meshes,
    const Vec3 origin,const std::vector<Ring>& rings,const unsigned int sides,
    const float roundness=1.0F)
{
    MeshData source;
    surface(source,origin,rings,sides,gloveScale,roundness);
    std::array<std::vector<std::uint32_t>,2> remaps;
    for(auto& remap:remaps) remap.assign(source.vertices.size(),UINT32_MAX);
    for(std::size_t i=0;i<source.indices.size();i+=3U) {
        const auto a=source.vertices[source.indices[i]],b=source.vertices[source.indices[i+1U]],
            c=source.vertices[source.indices[i+2U]];
        const bool dorsal=(a.normal.z+b.normal.z+c.normal.z)>0.15F;
        const std::size_t material=dorsal ? 0U : 1U;
        auto& output=meshes[material];
        for(std::size_t k=0;k<3U;++k) {
            const auto index=source.indices[i+k];
            auto& mapped=remaps[material][index];
            if(mapped==UINT32_MAX) {
                mapped=static_cast<std::uint32_t>(output.vertices.size());
                output.vertices.push_back(source.vertices[index]);
            }
            output.indices.push_back(mapped);
        }
    }
}

struct Contacts {
    Vec3 grip, support, magazine, mechanism;
};
Contacts contacts(const HudWeaponRole role) noexcept
{
    // Surface contacts, not wrist origins. They are authored against the
    // existing grip/fore-end/magazine/operating-piece geometry in weapon space.
    // Rotating a palm must not move that contact off the piece it is holding.
    switch(role) {
    case HudWeaponRole::Pistol:
        return {{.080F,-.213F,-.133F},{-.076F,-.242F,-.125F},
            {-.061F,-.267F,-.101F},{0,.120F,-.180F}};
    case HudWeaponRole::Knife:
        return {{.010F,-.038F,-.100F},{-.43F,-.27F,.05F},{},{}};
    case HudWeaponRole::Sniper:
        return {{.071F,-.184F,-.138F},{-.030F,-.072F,.360F},
            {-.065F,-.210F,.095F},{.244F,-.070F,-.080F}};
    case HudWeaponRole::Shotgun:
        return {{.071F,-.180F,-.213F},{-.030F,-.120F,.410F},
            {-.029F,-.100F,.063F},{-.030F,-.120F,.410F}};
    case HudWeaponRole::SubmachineGun:
        return {{.065F,-.205F,-.11F},{-.035F,-.080F,.39F},
            {-.070F,-.222F,.20F},{-.154F,-.019F,-.17F}};
    default:
        return {{.078F,-.192F,-.108F},{-.040F,-.090F,.370F},
            {-.077F,-.220F,.225F},{-.134F,-.017F,-.170F}};
    }
}
} // namespace

std::array<MeshData,playerHandsMaterialCount> makePlayerHandsMeshes(const bool includeSupportHand)
{
    std::array<MeshData,playerHandsMaterialCount> meshes;
    for(unsigned int side=0;side<(includeSupportHand ? 2U : 1U);++side) {
        const unsigned int b=side*static_cast<unsigned int>(playerHandBonesPerSide);
        // Rounded continuous sleeve with blended elbow and wrist, not rigid boxes.
        surface(meshes[0],{},{{-1.02F,.185F,.155F,b,b,1}, {-.8022F,.178F,.150F,b,b,1},
            {-.5689F,.165F,.140F,b+1,b,.1F}, {-.5067F,.158F,.133F,b+1,b,.3F},
            {-.46F,.163F,.138F,b+1,b,.5F}, {-.4155F,.155F,.130F,b+1,b,.85F},
            {-.3265F,.143F,.121F,b+1,b+1,1}, {-.1855F,.128F,.104F,b+1,b+1,1},
            {-.0705F,.106F,.092F,b+1,b+1,1}, {-.022F,.092F,.082F,b+2,b+1,.25F}},16U);
        surface(meshes[2],{},{{-.04F,.040F,.034F,b+2,b+1,.25F},
            {-.021F,.043F,.035F,b+2,b+1,.6F}, {-.003F,.034F,.027F,b+2,b+1,1}},12U,gloveScale);
        const float mirror=side==0U ? 1.0F : -1.0F;
        gloveSurface(meshes,{},{{-.015F,.032F,.021F,b+2,b+1,.65F},
            {.006F,.035F,.023F,b+2,b+2,1}, {.023F,.044F,.026F,b+2,b+2,1,mirror*.003F,-.001F},
            {.042F,.050F,.027F,b+2,b+2,1,mirror*.002F,-.002F},
            {.061F,.049F,.024F,b+2,b+2,1}, {.076F,.045F,.021F,b+2,b+2,1},
            {.086F,.038F,.014F,b+2,b+2,1}},16U,.70F);
        // The thumb's thenar pad blends into the palm, not a sphere at its root.
        gloveSurface(meshes,Vec3{mirror*.030F,.010F,-.012F}*gloveScale,
            {{0,.012F,.008F,b+2,b+2,1}, {.008F,.019F,.013F,b+2,b+2,1},
             {.020F,.023F,.015F,b+2,b+2,1}, {.034F,.019F,.012F,b+2,b+2,1},
             {.044F,.009F,.006F,b+2,b+2,1}},12U);
        // A thin flexible dorsal patch and cuff trim; no rigid sci-fi armor.
        surface(meshes[2],{0,0,.024F*gloveScale},{{.026F,.030F,.002F,b+2,b+2,1},
            {.047F,.039F,.0035F,b+2,b+2,1},{.068F,.031F,.002F,b+2,b+2,1}},12U,gloveScale,.70F);
        for(unsigned int d=0;d<5U;++d) {
            const Vec3 origin=digitRoot(d,side);
            std::vector<Ring> rings;
            float y=0;
            const float radius=d==4U ? .016F : d==3U ? .011F : .013F;
            for(unsigned int j=0;j<3U;++j) {
                const unsigned int joint=b+3U+d*3U+j, parent=j==0U ? b+2U : joint-1U;
                const float len=digitLength[d]*segmentFraction[j];
                rings.push_back({y,radius*(1-.13F*float(j)),radius*.80F,joint,parent,.45F});
                rings.push_back({y+len*.24F,radius*(1-.12F*float(j)),radius*.82F,joint,parent,.9F});
                rings.push_back({y+len*.67F,radius*(1-.17F*float(j)),radius*.77F,joint,parent,1});
                y+=len;
            }
            rings.push_back({y,radius*.36F,radius*.32F,b+3U+d*3U+2U,b+3U+d*3U+2U,1});
            gloveSurface(meshes,origin,rings,12U);
        }
    }
    for(auto& mesh:meshes) {
        mesh.bounds={{-.20F,-1.03F,-.17F},{.20F,.37F,.17F}};
        // The existing camera-space weapon basis reflects Z (left handed).
        // Like the weapon assets, use two-sided rasterization for this near pass.
        mesh.backfaceCullingSafe=false;
    }
    return meshes;
}

namespace {
struct HandTarget {
    Vec3 surfaceContact{}, rotation{};
    Vec3 localContact{palmContact};
    float partRotationZ{0};
    float thumbOpposition{.75F};
    std::array<float,5> flex{};
    HandContact contact{HandContact::Support};
};
HandTarget target(const Vec3 surfaceContact,const HandContact contact,const Vec3 rotation={}) noexcept
{
    std::array<float,5> flex{};
    float thumbOpposition=.75F;
    switch(contact) {
    case HandContact::Grip: flex={.43F,.83F,.88F,.86F,.48F}; thumbOpposition=.40F; break;
    case HandContact::Support: flex={.53F,.65F,.68F,.64F,.45F}; break;
    case HandContact::Magazine: flex={.88F,.96F,.98F,.91F,.82F}; break;
    case HandContact::Fetch: flex={.12F,.18F,.16F,.15F,.20F}; break;
    case HandContact::Mechanism: flex={.62F,.74F,.81F,.84F,.66F}; break;
    case HandContact::Shell: flex={.86F,.92F,.96F,.96F,.55F}; thumbOpposition=.08F; break;
    case HandContact::Ready: flex={.10F,.18F,.16F,.22F,.15F}; break;
    }
    HandTarget result;
    result.surfaceContact=surfaceContact; result.rotation=rotation;
    result.thumbOpposition=thumbOpposition; result.flex=flex; result.contact=contact;
    if(contact==HandContact::Shell) {
        // Cartridges are held at the index pad, not pressed into the middle of
        // the palm. This is the same articulated digit used by the skin rig.
        Mat4 index=translationMatrix(digitRoot(0U,1U))*rotationZMatrix(.0375F);
        for(unsigned int j=0;j<3U;++j) {
            index=index*rotationXMatrix(-(j==0U ? 1.12F : j==1U ? 1.32F : .63F)*flex[0]);
            const float len=digitLength[0]*segmentFraction[j]*gloveScale;
            if(j==2U) result.localContact=point(index,{0,len*.80F,-.012F*gloveScale});
            else index=index*translationMatrix({0,len,0});
        }
    }
    return result;
}
HandTarget blend(const HandTarget& a,const HandTarget& b,const float t) noexcept
{
    if(t<=0) return a;
    if(t>=1) return b;
    HandTarget r;
    r.surfaceContact=a.surfaceContact*(1-t)+b.surfaceContact*t;
    r.localContact=a.localContact*(1-t)+b.localContact*t;
    r.rotation=a.rotation*(1-t)+b.rotation*t;
    r.partRotationZ=a.partRotationZ*(1-t)+b.partRotationZ*t;
    r.thumbOpposition=a.thumbOpposition*(1-t)+b.thumbOpposition*t;
    for(std::size_t i=0;i<5U;++i) r.flex[i]=a.flex[i]*(1-t)+b.flex[i]*t;
    r.contact=t<.5F ? a.contact : b.contact;
    return r;
}
HandTarget reach(const HandTarget& from,const HandTarget& to,const float progress,
    const Vec3 clearance) noexcept
{
    const float u=safe(progress),t=ease(u,0,1);
    auto result=blend(from,to,t);
    if(u<=0 || u>=1) return result;
    // A hand leaves the receiver toward the body before approaching a new
    // contact. Zero endpoint velocity prevents a visible kink when the hand
    // starts following a magazine, pump or bolt on the next phase.
    const float arc=4*t*(1-t);
    result.surfaceContact=result.surfaceContact+clearance*(arc*arc);
    // Turn the palm while travelling, then close the fingers only on arrival.
    // Releasing the old object happens before departing it. Neither motion
    // changes the authoritative contact while a moving part is held.
    const float turn=ease(u,0,.80F);
    result.rotation=from.rotation*(1-turn)+to.rotation*turn;
    result.partRotationZ=from.partRotationZ*(1-turn)+to.partRotationZ*turn;
    const auto open=target({},HandContact::Fetch);
    constexpr std::array<float,5> releaseEnd{.40F,.36F,.33F,.30F,.42F};
    constexpr std::array<float,5> gripStart{.66F,.62F,.58F,.55F,.69F};
    constexpr std::array<float,5> gripEnd{1.0F,.96F,.92F,.90F,1.0F};
    for(std::size_t i=0;i<result.flex.size();++i) {
        const float opening=ease(u,0,releaseEnd[i]);
        const float closing=ease(u,gripStart[i],gripEnd[i]);
        const float transit=std::min({from.flex[i],to.flex[i],open.flex[i]});
        result.flex[i]=from.flex[i]*(1-opening)+transit*opening;
        result.flex[i]=result.flex[i]*(1-closing)+to.flex[i]*closing;
    }
    const float looseThumb=std::max({from.thumbOpposition,to.thumbOpposition,open.thumbOpposition});
    const float thumbRelease=ease(u,0,releaseEnd[4]);
    const float thumbGrip=ease(u,gripStart[4],gripEnd[4]);
    result.thumbOpposition=from.thumbOpposition*(1-thumbRelease)+looseThumb*thumbRelease;
    result.thumbOpposition=result.thumbOpposition*(1-thumbGrip)+to.thumbOpposition*thumbGrip;
    return result;
}
HandTarget boltTarget(const Contacts& c,const WeaponReloadPose& reload) noexcept
{
    constexpr Vec3 pivot{.096F,.015F,-.08F};
    const auto transform=translationMatrix(reload.mechanismOffset)*translationMatrix(pivot)
        *rotationZMatrix(reload.mechanismRotationDegrees.z*pi/180)*translationMatrix(pivot*-1);
    auto result=target(point(transform,c.mechanism),HandContact::Mechanism,{0,0,-12});
    result.partRotationZ=reload.mechanismRotationDegrees.z;
    return result;
}
} // namespace

WeaponMaterialImages makeUsedPlayerHandsMaterialImages(const PlayerHandsMaterial material)
{
    const bool cloth=material==PlayerHandsMaterial::Cloth;
    const bool leather=material==PlayerHandsMaterial::Leather;
    const int size=cloth || leather ? 512 : 256;
    WeaponMaterialImages images;
    for(auto* image:{&images.albedo,&images.normal,&images.properties}) {
        image->width=size; image->height=size;
        image->rgba.resize(static_cast<std::size_t>(size*size*4));
    }
    const auto byte=[](const int v) {return static_cast<std::uint8_t>(std::clamp(v,0,255));};
    for(int y=0;y<size;++y) for(int x=0;x<size;++x) {
        const auto hash=static_cast<std::uint32_t>(x)*73856093U
            ^ static_cast<std::uint32_t>(y)*19349663U;
        const int grain=int((hash^(hash>>11U))&7U)-3;
        const int weave=(x/3+y/3)%2==0 ? 2 : -2;
        const int worn=int(((static_cast<unsigned int>(x/24)*83492791U
            ^ static_cast<unsigned int>(y/24)*19349663U)>>4U)&3U);
        const int edge=std::min(std::abs(x-size/8),std::abs(x-size*7/8));
        const bool seam=edge<2 || (edge<6 && y%18<8);
        const float u=float(x)/float(size),v=float(y)/float(size);
        const float wrinkle=std::sin(v*42*pi+std::sin(u*6*pi)*.55F)
            *(.35F+.65F*std::pow(std::abs(2*u-1),3.0F));
        const bool jointCrease=std::abs(v-.44F)<.012F || std::abs(v-.77F)<.012F;
        const int tone=(cloth ? 70 : leather ? 49 : 57)+grain+worn
            +(cloth ? weave : 0)+int(wrinkle*(cloth ? 6 : 3))
            +(seam ? (leather ? 12 : 18) : 0)-(jointCrease ? 6 : 0);
        const auto i=static_cast<std::size_t>((y*size+x)*4);
        images.albedo.rgba[i]=byte(tone-2);
        images.albedo.rgba[i+1U]=byte(tone);
        images.albedo.rgba[i+2U]=byte(tone+1);
        images.normal.rgba[i]=byte(128+(cloth ? weave*2 : grain));
        images.normal.rgba[i+1U]=byte(128+grain+(seam ? 3 : 0)+int(wrinkle*5));
        images.normal.rgba[i+2U]=255;
        images.properties.rgba[i]=byte((leather ? 211 : 233)+grain);
        images.properties.rgba[i+1U]=0; // All approved glove materials are NONMETAL.
        images.properties.rgba[i+2U]=seam ? 242 : 250;
        for(auto* image:{&images.albedo,&images.normal,&images.properties}) image->rgba[i+3U]=255;
    }
    return images;
}

PlayerHandsPose samplePlayerHandsPose(const WeaponViewmodelRenderData& weapon) noexcept
{
    PlayerHandsPose pose;
    const auto frame=sampleWeaponPresentationTransform(weapon);
    const Contacts c=contacts(weapon.role);
    const float p=safe(weapon.reloadProgress);
    const auto reload=sampleWeaponReloadPose(weapon.role,p,weapon.reloadRoundCount);
    auto rightIdle=target(c.grip,HandContact::Grip);
    const bool knife=weapon.role==HudWeaponRole::Knife;
    if(knife) {
        rightIdle.flex={.77F,.83F,.85F,.83F,.55F};
        rightIdle.thumbOpposition=.75F;
    }
    // Long guns are cradled from below, not pinched against a receiver side.
    const Vec3 supportRotation=weapon.role==HudWeaponRole::Pistol || knife
        ? Vec3{} : Vec3{0,65,0};
    const auto leftIdle=target(c.support,knife ? HandContact::Ready : HandContact::Support,supportRotation);
    auto right=rightIdle,left=leftIdle;
    if(!knife && p>0 && p<1) {
        if(weapon.role==HudWeaponRole::Shotgun) {
            const float n=float(std::clamp(weapon.reloadRoundCount,1U,32U));
            const float duration=n*gameplay::shotgunShellReloadSeconds+gameplay::shotgunReadySeconds;
            const float end=n*gameplay::shotgunShellReloadSeconds/duration;
            if(p<end) {
                const float count=p*duration/gameplay::shotgunShellReloadSeconds;
                const float phase=std::fmod(count,1.0F),insert=ease(phase,0,.86F);
                const Vec3 start{-.13F,-.22F,-.10F};
                auto shell=target(c.magazine+start*(1-insert),HandContact::Shell,{3,-12,8});
                // Reset between shells, but never fetch a fictitious final shell.
                const auto next=target(c.magazine+(count>=n-1 ? Vec3{} : start),
                    HandContact::Shell,{3,-12,8});
                shell=blend(shell,next,ease(phase,.87F,1));
                left=reach(leftIdle,shell,p/(.096F/duration),{-.06F,-.025F,-.05F});
            } else {
                const auto port=target(c.magazine,HandContact::Shell,{3,-12,8});
                const auto pump=target(c.support+reload.mechanismOffset,HandContact::Support,supportRotation);
                left=reach(port,pump,(p-end)/(.072F/duration),{-.045F,-.03F,-.07F});
            }
        } else {
            const bool sniper=weapon.role==HudWeaponRole::Sniper;
            const bool smg=weapon.role==HudWeaponRole::SubmachineGun;
            const float insertionEnd=smg ? .71F : .78F;
            const Vec3 magazineRotation{0,12,-8};
            const auto magazine=target(c.magazine+reload.magazineOffset,
                HandContact::Magazine,magazineRotation);
            const float reachStart=sniper ? .19F : .02F;
            const float reachEnd=sniper ? .28F : .14F;
            left=reach(leftIdle,magazine,(p-reachStart)/(reachEnd-reachStart),
                {-.07F,-.04F,-.08F});
            if(p>=gameplay::magazineReleaseProgress && p<.53F) {
                const auto released=sampleWeaponReloadPose(weapon.role,gameplay::magazineReleaseProgress-.00001F);
                const auto outgoing=target(c.magazine+released.magazineOffset,HandContact::Magazine,magazineRotation);
                const auto fetch=target(c.magazine+Vec3{-.18F,-.44F,-.15F},HandContact::Fetch,{6,0,-15});
                left=reach(outgoing,fetch,(p-gameplay::magazineReleaseProgress)
                    /(.46F-gameplay::magazineReleaseProgress),{-.07F,-.04F,-.07F});
                const auto incoming=sampleWeaponReloadPose(weapon.role,.53F);
                left=reach(left,target(c.magazine+incoming.magazineOffset,HandContact::Magazine,
                    magazineRotation),(p-.46F)/(.53F-.46F),{-.035F,-.025F,-.05F});
            } else if(p>=.53F && p<insertionEnd) {
                left=magazine;
            } else if(p>=insertionEnd) {
                const auto seated=target(c.magazine,HandContact::Magazine,magazineRotation);
                if(sniper) {
                    left=reach(seated,leftIdle,(p-.78F)/(.80F-.78F),{-.035F,-.02F,-.05F});
                } else {
                    const auto mechanism=target(c.mechanism+reload.mechanismOffset,HandContact::Mechanism,
                        weapon.role==HudWeaponRole::Pistol ? Vec3{0,-82,0} : Vec3{0,0,60});
                    const bool pistol=weapon.role==HudWeaponRole::Pistol;
                    const float mechanismReached=pistol ? .85F : smg ? .77F : .85F;
                    left=reach(seated,mechanism,(p-insertionEnd)/(mechanismReached-insertionEnd),
                        {-.06F,-.025F,-.09F});
                    // Release the spring-loaded slide/charging handle before
                    // returning to support. The pump, unlike these, remains
                    // held throughout its full manual return stroke above.
                    auto released=left;
                    released.flex=target(c.mechanism,HandContact::Fetch).flex;
                    left=blend(left,released,ease(p,pistol ? .89F : smg ? .85F : .91F,
                        pistol ? .91F : smg ? .87F : .93F));
                    const float returnStart=pistol ? .91F : smg ? .87F : .93F;
                    left=reach(left,leftIdle,(p-returnStart)/(1-returnStart),
                        {-.055F,-.02F,-.075F});
                }
            }
            if(sniper) {
                const auto bolt=boltTarget(c,reload);
                if(p<.26F) {
                    right=reach(rightIdle,bolt,(p-.015F)/(.07F-.015F),{.05F,.02F,-.065F});
                    right=reach(right,rightIdle,(p-.20F)/(.26F-.20F),{.04F,.02F,-.06F});
                } else if(p>=.80F) {
                    right=reach(rightIdle,bolt,(p-.80F)/(.85F-.80F),{.05F,.02F,-.065F});
                    right=reach(right,rightIdle,(p-.97F)/(1-.97F),{.04F,.02F,-.06F});
                }
            }
        }
    }
    pose.rightContact=right.contact; pose.leftContact=left.contact;
    const std::array targets{right,left};
    for(unsigned int side=0;side<2U;++side) {
        const unsigned int b=side*static_cast<unsigned int>(playerHandBonesPerSide);
        const bool dominant=side==0U;
        const auto& t=targets[side];
        // The knife's four knuckles run along its handle; firearms' knuckles
        // run down their pistol grips. Reusing the latter left the knife
        // balanced on a couple of fingertips instead of inside a fist.
        const Mat4 palmBasis=dominant && knife ? basis({0,0,1},{-1,0,0},{0,-1,0})
            : dominant ? basis({0,1,0},{0,0,1},{1,0,0}) : basis({0,-1,0},{0,0,1},{-1,0,0});
        const Mat4 desired=rotationZMatrix(t.partRotationZ*pi/180)
            *palmBasis
            *rotationZMatrix(t.rotation.z*pi/180)*rotationYMatrix(t.rotation.y*pi/180)
            *rotationXMatrix(t.rotation.x*pi/180);
        // The piece contact (palm or index pad) is authoritative, not a wrist
        // detached from the object. Bounded bending rotates about this contact.
        const Vec3 contact=t.surfaceContact;
        Mat4 orientation=desired;
        Vec3 wrist=contact-point(desired,t.localContact);
        for(unsigned int iteration=0;iteration<16U;++iteration) {
            wrist=contact-point(orientation,t.localContact);
            const auto chain=solveArm(frame,wrist,dominant);
            const auto limited=limitWrist(desired,normalized(wrist-chain.elbow));
            // Under-relax the contact/IK feedback. A full-step solver could
            // oscillate when a large equip/melee root also moved the contact.
            const Vec3 y=normalized(column(orientation,1U)+column(limited,1U));
            const Vec3 xSum=column(orientation,0U)+column(limited,0U);
            const Vec3 x=normalized(xSum-y*dot(xSum,y));
            orientation=basis(x,y,cross(x,y));
        }
        wrist=contact-point(orientation,t.localContact);
        const auto chain=solveArm(frame,wrist,dominant);
        const Vec3 shoulder=chain.shoulder,elbow=chain.elbow;
        const Mat4 hand=translationMatrix(wrist)*orientation;
        const Vec3 handX=column(orientation,0U);
        const auto fore=alongYWithRoll(wrist-elbow,handX);
        pose.shoulders[side]=shoulder; pose.elbows[side]=elbow; pose.wrists[side]=wrist;
        pose.contactPoints[side]=contact;
        pose.fingerFlexion[side]=t.flex;
        pose.skin[b]=translationMatrix(shoulder)*alongYWithRoll(elbow-shoulder,column(fore,0U))
            *translationMatrix(bindShoulder*-1);
        pose.skin[b+1]=translationMatrix(elbow)*fore
            *translationMatrix(bindElbow*-1);
        pose.skin[b+2]=hand;
        for(unsigned int d=0;d<5U;++d) {
            const Vec3 root=digitRoot(d,side);
            Mat4 joint=hand*translationMatrix(root);
            if(d==4U) joint=joint*rotationZMatrix((dominant ? -1.0F : 1.0F)*t.thumbOpposition)
                *rotationYMatrix((dominant ? 1.0F : -1.0F)*.22F);
            // Tiny anatomical splay prevents four equal, parallel tube fingers.
            else joint=joint*rotationZMatrix((dominant ? 1.0F : -1.0F)
                *(float(d)-1.5F)*.025F);
            float bindY=0;
            for(unsigned int j=0;j<3U;++j) {
                const float angle=(d==4U ? .65F : j==0U ? 1.12F : j==1U ? 1.32F : .63F)*t.flex[d];
                joint=joint*rotationXMatrix(-angle);
                pose.skin[b+3U+d*3U+j]=joint*translationMatrix((root+Vec3{0,bindY,0})*-1);
                const float len=digitLength[d]*segmentFraction[j]*gloveScale;
                joint=joint*translationMatrix({0,len,0}); bindY+=len;
            }
        }
    }
    return pose;
}

Vec3 deformPlayerHandsVertex(const wkc::render::Vertex& vertex, const PlayerHandsPose& pose) noexcept
{
    const auto joint=static_cast<std::size_t>(std::clamp(std::floor(vertex.uv.x*.5F),0.0F,float(playerHandsBoneCount-1U)));
    const auto parent=static_cast<std::size_t>(std::clamp(vertex.uv.y,0.0F,float(playerHandsBoneCount-1U)));
    const float weight=std::clamp(vertex.uv.x-float(joint)*2,0.0F,1.0F);
    return point(pose.skin[joint],vertex.position)*weight+point(pose.skin[parent],vertex.position)*(1-weight);
}
} // namespace gdp::game::visual
