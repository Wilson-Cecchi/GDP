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
constexpr float gloveScale = 2.2F;
// Near-pass proportions account for the established enlarged weapon space.
// Shoulders remain below the frame instead of shortening a reaching arm in air.
constexpr float upperLength = .90F, foreLength = .90F;
constexpr Vec3 bindShoulder{0,-1.80F,0}, bindElbow{0,-.90F,0};
constexpr std::array<float,5> digitX{.032F,.0105F,-.0105F,-.030F,.036F};
constexpr std::array<float,5> digitLength{.091F,.098F,.091F,.075F,.077F};
constexpr std::array<float,3> segmentFraction{.44F,.33F,.23F};
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
Vec3 digitRoot(const unsigned int digit, const unsigned int side) noexcept
{
    constexpr std::array<float,5> rootY{.085F,.090F,.087F,.077F,.025F};
    return Vec3{digitX[digit]*(side == 0U ? 1.0F : -1.0F),rootY[digit],0}*gloveScale;
}
struct Ring {
    float y, x, z;
    unsigned int joint, parent;
    float weight;
    float centerX{0}, centerZ{0};
};

void surface(MeshData& mesh, const Vec3 origin, const std::vector<Ring>& rings,
    const unsigned int sides=12U, const float scale=1.0F)
{
    const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::size_t i=0;i<rings.size();++i) for (unsigned int s=0;s<=sides;++s) {
        const auto r=rings[i];
        const auto before=rings[i==0 ? i : i-1U],after=rings[i+1U<rings.size() ? i+1U : i];
        const float dy=std::max(.0001F,after.y-before.y);
        const float angle=float(s)/float(sides)*2*pi;
        const float x=std::cos(angle),z=std::sin(angle);
        const float slope=x*x*(after.x-before.x)/dy+z*z*(after.z-before.z)/dy;
        mesh.vertices.push_back({origin+Vec3{x*r.x+r.centerX,r.y,z*r.z+r.centerZ}*scale,
            normalized({x,-slope,z}),{float(r.joint)*2+r.weight,float(r.parent)},r.joint});
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
    const Vec3 origin,const std::vector<Ring>& rings,const unsigned int sides)
{
    MeshData source;
    surface(source,origin,rings,sides,gloveScale);
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
    switch(role) {
    case HudWeaponRole::Pistol:
        return {{.102F,-.213F,-.265F},{-.114F,-.237F,-.265F},
            {-.105F,-.29F,-.255F},{-.015F,.172F,-.23F}};
    case HudWeaponRole::Knife:
        return {{.095F,-.023F,-.315F},{-.43F,-.27F,.05F},{},{}};
    case HudWeaponRole::Sniper:
        return {{.102F,-.184F,-.27F},{-.129F,-.082F,.43F},
            {-.097F,-.225F,-.045F},{.268F,-.07F,-.22F}};
    case HudWeaponRole::Shotgun:
        return {{.102F,-.18F,-.345F},{-.124F,-.103F,.40F},
            {-.085F,-.16F,-.075F},{-.124F,-.103F,.40F}};
    default: // SMG's current approved geometry is the carbine, not new gun art.
        return {{.106F,-.192F,-.24F},{-.148F,-.094F,.43F},
            {-.114F,-.27F,.08F},{-.163F,-.112F,-.24F}};
    }
}
} // namespace

std::array<MeshData,playerHandsMaterialCount> makePlayerHandsMeshes()
{
    std::array<MeshData,playerHandsMaterialCount> meshes;
    for(unsigned int side=0;side<2U;++side) {
        const unsigned int b=side*static_cast<unsigned int>(playerHandBonesPerSide);
        // Rounded continuous sleeve with blended elbow and wrist, not rigid boxes.
        surface(meshes[0],{},{{-1.80F,.15F,.135F,b,b,1}, {-1.42F,.145F,.127F,b,b,1},
            {-1.08F,.13F,.112F,b+1,b,.1F}, {-.96F,.124F,.106F,b+1,b,.3F},
            {-.90F,.127F,.108F,b+1,b,.5F}, {-.81F,.119F,.100F,b+1,b,.85F},
            {-.64F,.111F,.089F,b+1,b+1,1}, {-.36F,.09F,.071F,b+1,b+1,1},
            {-.085F,.077F,.065F,b+1,b+1,1}, {-.018F,.069F,.060F,b+2,b+1,.25F}},16U);
        surface(meshes[2],{},{{-.04F,.036F,.032F,b+2,b+1,.25F},
            {-.021F,.038F,.032F,b+2,b+1,.6F}, {-.003F,.030F,.025F,b+2,b+1,1}},12U,gloveScale);
        const float mirror=side==0U ? 1.0F : -1.0F;
        gloveSurface(meshes,{},{{-.015F,.028F,.019F,b+2,b+1,.65F},
            {.006F,.030F,.020F,b+2,b+2,1}, {.024F,.038F,.023F,b+2,b+2,1,mirror*.003F,-.001F},
            {.047F,.044F,.025F,b+2,b+2,1,mirror*.002F,-.002F},
            {.069F,.043F,.022F,b+2,b+2,1}, {.087F,.039F,.019F,b+2,b+2,1},
            {.098F,.031F,.013F,b+2,b+2,1}},16U);
        // The thumb's thenar pad blends into the palm, not a sphere at its root.
        gloveSurface(meshes,Vec3{mirror*.026F,.012F,-.013F}*gloveScale,
            {{0,.010F,.007F,b+2,b+2,1}, {.009F,.018F,.012F,b+2,b+2,1},
             {.025F,.021F,.014F,b+2,b+2,1}, {.041F,.018F,.011F,b+2,b+2,1},
             {.052F,.008F,.006F,b+2,b+2,1}},12U);
        // A thin flexible dorsal patch and cuff trim; no rigid sci-fi armor.
        surface(meshes[2],{0,0,.022F*gloveScale},{{.033F,.025F,.002F,b+2,b+2,1},
            {.055F,.033F,.0035F,b+2,b+2,1},{.078F,.025F,.002F,b+2,b+2,1}},12U,gloveScale);
        for(unsigned int d=0;d<5U;++d) {
            const Vec3 origin=digitRoot(d,side);
            std::vector<Ring> rings;
            float y=0;
            const float radius=d==4U ? .014F : d==3U ? .010F : .0115F;
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
        mesh.bounds={{-.16F,-1.81F,-.15F},{.16F,.43F,.15F}};
        // The existing camera-space weapon basis reflects Z (left handed).
        // Like the weapon assets, use two-sided rasterization for this near pass.
        mesh.backfaceCullingSafe=false;
    }
    return meshes;
}

namespace {
struct HandTarget {
    Vec3 wrist{}, rotation{};
    float partRotationZ{0};
    std::array<float,5> flex{};
    HandContact contact{HandContact::Support};
};
HandTarget target(const Vec3 wrist,const HandContact contact,const Vec3 rotation={}) noexcept
{
    std::array<float,5> flex{};
    switch(contact) {
    case HandContact::Grip: flex={.43F,.94F,1.00F,.96F,.84F}; break;
    case HandContact::Support: flex={.68F,.82F,.91F,.96F,.78F}; break;
    case HandContact::Magazine: flex={.88F,.96F,.98F,.91F,.82F}; break;
    case HandContact::Fetch: flex={.12F,.18F,.16F,.15F,.20F}; break;
    case HandContact::Mechanism: flex={.62F,.74F,.81F,.84F,.66F}; break;
    case HandContact::Shell: flex={.42F,.55F,.91F,.96F,.61F}; break;
    case HandContact::Ready: flex={.10F,.18F,.16F,.22F,.15F}; break;
    }
    return {wrist,rotation,0,flex,contact};
}
HandTarget blend(const HandTarget& a,const HandTarget& b,const float t) noexcept
{
    if(t<=0) return a;
    if(t>=1) return b;
    HandTarget r;
    r.wrist=a.wrist*(1-t)+b.wrist*t;
    r.rotation=a.rotation*(1-t)+b.rotation*t;
    r.partRotationZ=a.partRotationZ*(1-t)+b.partRotationZ*t;
    for(std::size_t i=0;i<5U;++i) r.flex[i]=a.flex[i]*(1-t)+b.flex[i]*t;
    r.contact=t<.5F ? a.contact : b.contact;
    return r;
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
    if(knife) rightIdle.flex={.94F,.99F,1.00F,.97F,.86F};
    const auto leftIdle=target(c.support,knife ? HandContact::Ready : HandContact::Support);
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
                left=blend(leftIdle,shell,ease(p,0,.04F));
            } else {
                const auto port=target(c.magazine,HandContact::Shell,{3,-12,8});
                const auto pump=target(c.support+reload.mechanismOffset,HandContact::Mechanism);
                left=blend(port,pump,ease(p,end,end+.03F));
                left=blend(left,leftIdle,ease(p,.96F,1));
            }
        } else {
            const bool sniper=weapon.role==HudWeaponRole::Sniper;
            const bool smg=weapon.role==HudWeaponRole::SubmachineGun;
            const float insertionEnd=smg ? .71F : .78F;
            const Vec3 magazineRotation{0,12,-8};
            const auto magazine=target(c.magazine+reload.magazineOffset,
                HandContact::Magazine,magazineRotation);
            left=blend(leftIdle,magazine,ease(p,sniper ? .26F : .02F,sniper ? .28F : .14F));
            if(p>=gameplay::magazineReleaseProgress && p<.53F) {
                const auto released=sampleWeaponReloadPose(weapon.role,gameplay::magazineReleaseProgress-.00001F);
                const auto outgoing=target(c.magazine+released.magazineOffset,HandContact::Magazine,magazineRotation);
                const auto fetch=target(c.magazine+Vec3{-.18F,-.44F,-.15F},HandContact::Fetch,{6,0,-15});
                left=blend(outgoing,fetch,ease(p,gameplay::magazineReleaseProgress,.46F));
                const auto incoming=sampleWeaponReloadPose(weapon.role,.53F);
                left=blend(left,target(c.magazine+incoming.magazineOffset,HandContact::Magazine,magazineRotation),
                    ease(p,.46F,.53F));
            } else if(p>=.53F && p<insertionEnd) {
                left=magazine;
            } else if(p>=insertionEnd) {
                const auto seated=target(c.magazine,HandContact::Magazine,magazineRotation);
                if(sniper) {
                    left=blend(seated,leftIdle,ease(p,.78F,.80F));
                } else {
                    const auto mechanism=target(c.mechanism+reload.mechanismOffset,HandContact::Mechanism,
                        weapon.role==HudWeaponRole::Pistol ? Vec3{0,-82,0} : Vec3{0,0,60});
                    left=blend(seated,mechanism,ease(p,insertionEnd,insertionEnd+.02F));
                    left=blend(left,leftIdle,ease(p,smg ? .94F : .92F,1));
                }
            }
            if(sniper) {
                const auto bolt=boltTarget(c,reload);
                if(p<.26F) {
                    right=blend(rightIdle,bolt,ease(p,.015F,.07F));
                    right=blend(right,rightIdle,ease(p,.20F,.26F));
                    left=leftIdle;
                } else if(p>=.80F) {
                    right=blend(rightIdle,bolt,ease(p,.80F,.85F));
                    right=blend(right,rightIdle,ease(p,.97F,1));
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
        const Vec3 wrist=t.wrist;
        const Mat4 hand=translationMatrix(wrist)*rotationZMatrix(t.partRotationZ*pi/180)
            *(dominant ? basis({0,1,0},{0,0,1},{1,0,0}) : basis({0,-1,0},{0,0,1},{-1,0,0}))
            *rotationZMatrix(t.rotation.z*pi/180)*rotationYMatrix(t.rotation.y*pi/180)
            *rotationXMatrix(t.rotation.x*pi/180);
        Vec3 shoulder=inverseNearPoint(frame,dominant ? Vec3{.20F,-.50F,.24F} : Vec3{-.14F,-.50F,.24F});
        const Vec3 direction=normalized(wrist-shoulder);
        const float distance=std::clamp(length(wrist-shoulder),.09F,upperLength+foreLength-.002F);
        shoulder=wrist-direction*distance;
        const float along=(upperLength*upperLength-foreLength*foreLength+distance*distance)/(2*distance);
        const float bend=std::sqrt(std::max(0.0F,upperLength*upperLength-along*along));
        const Vec3 pole=inverseNearPoint(frame,dominant ? Vec3{.45F,-.70F,.24F} : Vec3{-.40F,-.70F,.24F})-shoulder;
        const Vec3 elbow=shoulder+direction*along+normalized(pole-direction*dot(pole,direction))*bend;
        pose.shoulders[side]=shoulder; pose.elbows[side]=elbow; pose.wrists[side]=wrist;
        pose.fingerFlexion[side]=t.flex;
        pose.skin[b]=translationMatrix(shoulder)*alongY(elbow-shoulder)*translationMatrix(bindShoulder*-1);
        pose.skin[b+1]=translationMatrix(elbow)*alongY(wrist-elbow)*translationMatrix(bindElbow*-1);
        pose.skin[b+2]=hand;
        for(unsigned int d=0;d<5U;++d) {
            const Vec3 root=digitRoot(d,side);
            Mat4 joint=hand*translationMatrix(root);
            if(d==4U) joint=joint*rotationZMatrix((dominant ? -1.0F : 1.0F)*1.12F)
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
