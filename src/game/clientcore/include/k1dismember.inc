// Engage-only adapter: native resources, effects, appearance, animation and scene
// ownership. Extracted mesh/anchor data lives in ignored k1cuts.2da, never here.
#include "twodimarray.h"
#include "k1jkadismemberment.h"
#include <map>
#include "port/crtapi.h"
#include "nwcprojectile.h"
#include "AURORA/aurmaterial.h"
#include "AURORA/aurtexman.h"
namespace {
struct CutDefinition {
    std::string body,source,anchor,retained,detached,code;
    Vector origin,axis;int slot=0;float probability=0;bool wholeHead=false;
};
std::vector<CutDefinition>& CutDefinitions() {
    static std::vector<CutDefinition> definitions;
    static bool loaded=false;
    const char* enabled=std::getenv("K1_ENGAGE_DISMEMBERMENT");
    if(!enabled || strcmp(enabled,"1"))return definitions;
    if(loaded)return definitions;
    if(!g_pExoResMan->Exists(CResRef("k1cuts"),RESTYPE_2DA,static_cast<unsigned int*>(nullptr)))return definitions;
    loaded=true;
    for(const char* resource:{"k1cuts","k1npccuts"}) {
    if(!g_pExoResMan->Exists(CResRef(resource),RESTYPE_2DA,static_cast<unsigned int*>(nullptr)))continue;
    C2DA table(CResRef(resource),1);
    if(!table.Load2DArray())return definitions;
    for(int row=0;row<table.m_nRows;++row) {
        auto text=[&](const char* column){CExoString s;table.GetCExoStringEntry(row,CExoString(column),&s);return std::string(s.CStr()?s.CStr():"");};
        auto number=[&](const char* column){float f=0;table.GetFLOATEntry(row,CExoString(column),&f);return f;};
        CutDefinition d;d.body=text("body");d.source=text("source");d.anchor=text("anchor");d.code=text("cut");
        // Explicit sandbox policy, including protection against a stale table.
        if(!_stricmp(d.body.c_str(),"n_tusken") && (d.code=="rl" || d.code=="ll")) {
            if(std::getenv("K1_ENGAGE_NPC_CUT_TEST"))fprintf(stderr,"K1_NPC_CUT_TUSKEN_LEG_REJECT cut=%s\n",d.code.c_str());
            continue;
        }
        d.retained=text("retained");d.detached=text("detached");d.slot=text("slot")=="head"?CSWCAnimBase::ANIM_MODEL_HEAD:CSWCAnimBase::ANIM_MODEL_BODY;
        d.wholeHead=number("wholehead")!=0;
        d.origin=Vector(number("px"),number("py"),number("pz"));d.axis=Vector(number("ax"),number("ay"),number("az"));d.probability=number("probability");
        if(!K1SaberContact::Finite(d.origin)||!K1SaberContact::Finite(d.axis)||std::fabs(magnitude(d.axis)-1)>.001f){definitions.clear();break;}
        definitions.push_back(d);
    }
    }
    fprintf(stderr,"K1_DISMEMBER_TABLE rows=%zu\n",definitions.size());return definitions;
}
std::vector<CutTexture> CutTextures(Gob* model) {
    std::vector<CutTexture> result;if(!model)return result;
    auto collect=[&](auto&& self,Part* p)->void {
        if(p->m_pMaterial && p->m_pMaterial->m_spr0.m_pObject) {
            auto* basic=static_cast<CAurTextureBasic*>(p->m_pMaterial->m_spr0.m_pObject->GetReferencedTexture()->AsCAurTextureBasic());
            if(basic && basic!=CAurTextureBasic::m_pNullTexture && basic->m_szResRef[0])result.push_back({p->Name(),basic->m_szResRef});
        }
        for(int i=0;i<p->m_nChildren;++i)self(self,p->m_paChildren[i]);
    };collect(collect,model->m_pPart);return result;
}
void RestoreCutCapTextures(Gob* model) {
    if(!model)return;
    for(const auto& texture:CutTextures(model))if(!_strnicmp(texture.part.c_str(),"dismember_",10)) {
        const char* expected=!_strnicmp(texture.part.c_str(),"dismember_glow",14)?"jkcapgl":"jkcap";
        if(_stricmp(texture.resource.c_str(),expected))if(auto* part=model->GetPart(const_cast<char*>(texture.part.c_str())))
            if(part->m_pMaterial) {
                if(std::getenv("K1_ENGAGE_NPC_CUT_TEST"))fprintf(stderr,"K1_DISMEMBER_CAP_RESTORED part=%s previous=%s expected=%s\n",part->Name(),texture.resource.c_str(),expected);
                part->m_pMaterial->ReplaceTexture(nullptr,const_cast<char*>(expected));
            }
    }
}
std::string CutBasePartName(const std::string& name) {
    const auto suffix=name.rfind("_p");
    if(suffix==std::string::npos || suffix+2==name.size())return name;
    for(size_t i=suffix+2;i<name.size();++i)if(name[i]<'0'||name[i]>'9')return name;
    return name.substr(0,suffix);
}
const CutTexture* CutTextureFor(const std::vector<CutTexture>& textures,const std::string& part) {
    for(const auto& texture:textures)if(texture.part==part)return &texture;
    const auto base=CutBasePartName(part);
    for(const auto& texture:textures)if(CutBasePartName(texture.part)==base)return &texture;
    return nullptr;
}
void ApplyCutTextures(Gob* model,const std::vector<CutTexture>& textures) {
    if(!model)return;
    // Retained and detached sides can require different native palette splits.
    // Every exported _pN piece inherits its original mesh's runtime material.
    auto apply=[&](auto&& self,Part* part)->void {
        if(part->m_pMaterial && _strnicmp(part->Name(),"dismember_",10))
            if(const auto* texture=CutTextureFor(textures,part->Name()))part->m_pMaterial->ReplaceTexture(nullptr,const_cast<char*>(texture->resource.c_str()));
        for(int i=0;i<part->m_nChildren;++i)self(self,part->m_paChildren[i]);
    };apply(apply,model->m_pPart);
}
bool CutTexturesMatch(Gob* model,const std::vector<CutTexture>& expected) {
    const auto actual=CutTextures(model);
    for(const auto& texture:actual) {
        if(!_strnicmp(texture.part.c_str(),"dismember_",10)) {
            if(_stricmp(texture.resource.c_str(),CutBasePartName(texture.part)=="dismember_glow"?"jkcapgl":"jkcap"))return false;
        }else {
            const auto* old=CutTextureFor(expected,texture.part);
            if(!old || _stricmp(old->resource.c_str(),texture.resource.c_str())) {
                if(std::getenv("K1_ENGAGE_NPC_CUT_TEST"))fprintf(stderr,"K1_NPC_CUT_TEXTURE_DETAIL part=%s actual=%s expected=%s\n",texture.part.c_str(),texture.resource.c_str(),old?old->resource.c_str():"missing");
                return false;
            }
        }
    }
    return !actual.empty();
}
void ReattachCutActor(CSWCCreature* client) {
    auto* module=g_pAppManager->m_pClientExoApp->GetModule();
    if(module && module->m_pArea)client->AttachToScene(module->m_pArea->m_pScene);
}
struct FrozenCutPart {Part* part;Vector position;Quaternion orientation;float scale;};
bool FreezeFinalCutPose(Gob* model,const char* clip,float& time){
    model->PlayAnimation(nullptr,1,0,0);model->Reset();model->EnableForceAnimations();
    if(!model->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,0))return false;
    float current=0,length=0;if(!model->GetAnimation(const_cast<char*>(clip),&current,&length)||length<=0)return false;
    time=std::nextafter(length,0.f);model->SetAnimationTime(const_cast<char*>(clip),time);model->Animate(.000001f);return true;
}
bool CutCapCenter(Gob* model,Vector& center){
    auto* part=model?model->GetPart(const_cast<char*>("dismember_cap")):nullptr;
    auto* skin=part?part->AsPartSkin():nullptr;List<Vector> vertices;
    if(!skin||!skin->GetSkinnedWorldVertices(vertices)||!vertices.m_count)return false;
    double x=0,y=0,z=0;for(int i=0;i<vertices.m_count;++i){x+=vertices.m_pData[i].x;y+=vertices.m_pData[i].y;z+=vertices.m_pData[i].z;}
    center=Vector(float(x/vertices.m_count),float(y/vertices.m_count),float(z/vertices.m_count));return K1SaberContact::Finite(center);
}
float CutVisualRandom(){static std::minstd_rand generator(0x51ab35);return std::generate_canonical<float,24>(generator);}
constexpr float cutThrowMin=1.25f,cutThrowMax=1.75f;
struct CutCorpse {
    CSWCCreature* owner;unsigned long long area;int row;Gob* limb=nullptr;
    Gob* head=nullptr;Gob* weapon=nullptr;Gob* leftWeapon=nullptr;Vector landing;unsigned int flightMs=0;
    long long sampledAt=0;bool landed=false;float floor=0;long long diedAt=0;bool nativeReloaded=false;
    std::string headResource,weaponResource,leftWeaponResource;float headScale=1,weaponScale=1,leftWeaponScale=1;
    std::vector<CutTexture> headTextures,weaponTextures,leftWeaponTextures;bool texturesMatch=true;
    int frozenSamples=0;float frozenError=0,finalTime=0;std::string deathClip;
    std::vector<FrozenCutPart> frozenPose;
    Quaternion launchOrientation;Vector launchCap,throwOffset,swingMotion;float spinAngle=0,flatError=0,awayDot=0,throwDistance=0;unsigned int flightTotalMs=0;
};
unsigned long long cutPartsCreated=0,cutPartsDeleted=0;
void DeleteCutParts(CutCorpse& c){cutPartsDeleted+=(c.weapon!=nullptr)+(c.leftWeapon!=nullptr)+(c.head!=nullptr)+(c.limb!=nullptr);delete c.weapon;delete c.leftWeapon;delete c.head;delete c.limb;c.weapon=c.leftWeapon=c.head=c.limb=nullptr;}
float CutLowest(Gob* model) {
    float lowest=1e6f;if(!model)return lowest;
    auto collect=[&](auto&& self,Part* part)->void {
        if(part->AsPartTriMesh() && part->m_bDrawEnabled && static_cast<MdlNodeTriMesh*>(part->m_pNode)->m_bRender){
            if(auto* s=part->AsPartSkin()){List<Vector> vv;if(s->GetSkinnedWorldVertices(vv))for(int i=0;i<vv.m_count;++i)lowest=std::min(lowest,vv.m_pData[i].z);}
            else {auto* n=static_cast<MdlNodeTriMesh*>(part->m_pNode);if(n->m_paVertices)for(unsigned int i=0;i<n->m_nVertexCount;++i)lowest=std::min(lowest,PartPointWorld(part,Vector(n->m_paVertices[i*3],n->m_paVertices[i*3+1],n->m_paVertices[i*3+2])).z);}
        }
        for(int i=0;i<part->m_nChildren;++i)self(self,part->m_paChildren[i]);
    };collect(collect,model->m_pPart);return lowest;
}
void HideCutAttachment(Gob* model) {
    if(!model)return;
    auto hide=[&](auto&& self,Part* p)->void {p->m_bDrawEnabled=false;for(int i=0;i<p->m_nChildren;++i)self(self,p->m_paChildren[i]);};
    hide(hide,model->m_pPart);
}
std::map<unsigned long long,CutCorpse> cutCorpses;
const CutDefinition* CutAt(int row) {auto& d=CutDefinitions();return row>=0 && row<int(d.size())?&d[row]:nullptr;}
const char* NativeCutBody(CSWCCreature* actor) {
    auto* b=actor?actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
    const char* name=b&&b->m_pModel?b->m_pModel->m_szName:"";
    if(const char* wearable=WearableNativeResource(name))return wearable;
    if(!_stricmp(name,"k1jarig")||!_stricmp(name,"pmbbl"))return "pmbbl";
    if(!_stricmp(name,"k1jadark")||!_stricmp(name,"n_darkjedim"))return "n_darkjedim";
    if(!_stricmp(name,"k1jasith")||!_stricmp(name,"n_sithsoldier"))return "n_sithsoldier";
    for(const auto& type:k1DarkJediTypes)if(!_stricmp(name,type.rig))return "n_darkjedim";
    if(nativeNpcRigs.empty())LoadNativeNpcRigs();
    for(const auto& rig:nativeNpcRigs)for(const auto* resource:{&rig.native,&rig.single,&rig.staff,&rig.dual,&rig.red,&rig.blue})
        if(!_stricmp(name,resource->c_str()))return rig.native.c_str();
    return name;
}
bool CutResources(const CutDefinition& d) {
    for(const auto* name:{d.retained.c_str(),d.detached.c_str()})
        if(!g_pExoResMan->Exists(CResRef(name),RESTYPE_MDL,static_cast<unsigned int*>(nullptr))||
           !g_pExoResMan->Exists(CResRef(name),RESTYPE_MDX,static_cast<unsigned int*>(nullptr)))return false;
    return true;
}
bool CutDirectionAllowed(const std::string& code,int direction) {
    if(direction<0||direction>6)return false;
    const bool horizontal=direction==3||direction==4;
    if(code=="ra"||code=="la")return !horizontal;
    if(code=="rh"||code=="lh"||code=="wa"||code=="he")return horizontal;
    return true; // Legs retain their unrestricted swing-direction policy.
}
bool CutBone(Gob* body,const char* name,Vector& out){
    auto* part=body?body->GetPart(const_cast<char*>(name)):nullptr;if(!part)return false;
    Quaternion q;part->GetWorld(out,q);return K1SaberContact::Finite(out);
}
float CutCapsuleDistance(Vector hit,Vector a,Vector b){
    const Vector line=b-a;const float ll=dot(line,line);if(ll<1e-8f)return 1e6f;
    const float t=std::max(0.f,std::min(1.f,dot(hit-a,line)/ll));return magnitude(hit-(a+line*t));
}
bool CutCapsule(Vector hit,Vector a,Vector b,float radius){return CutCapsuleDistance(hit,a,b)<=radius;}
// Native bodies do not share mesh names (some put the entire body in one skin).
// Resolve the actual contacted skin's nearest vertex through its native palette
// before using the existing bone-local anatomical regions.
unsigned NpcCutSurfaceOwner(Part* surface,Vector hit) {
    if(!surface)return 0;
    auto classify=[](const char* name)->unsigned {
        if(strlen(name)>=6 && !_strnicmp(name+2,"fngr",4)) {
            if(name[0]=='r'||name[0]=='R')return 1;
            if(name[0]=='l'||name[0]=='L')return 2;
        }
        for(const char* p:{"rbicep","rforearm","rhand","rfinger","rthumb"})if(!_strnicmp(name,p,strlen(p)))return 1;
        for(const char* p:{"lbicep","lforearm","lhand","lfinger","lthumb"})if(!_strnicmp(name,p,strlen(p)))return 2;
        for(const char* p:{"rthigh","rshin","rfoot","rtoe"})if(!_strnicmp(name,p,strlen(p)))return 4;
        for(const char* p:{"lthigh","lshin","lfoot","ltoe"})if(!_strnicmp(name,p,strlen(p)))return 8;
        for(const char* p:{"head","neck","eye","f_","jaw"})if(!_strnicmp(name,p,strlen(p)))return 16;
        return 32;
    };
    auto* skin=surface->AsPartSkin();
    if(!skin) {
        for(auto* p=surface;p;p=p->m_pParent)if(classify(p->Name())==16)return 16;
        return 32;
    }
    List<Vector> vertices;if(!skin->GetSkinnedWorldVertices(vertices)||!vertices.m_count)return 0;
    int nearest=0;float best=1e20f;
    for(int i=0;i<vertices.m_count;++i){float d=dot(vertices.m_pData[i]-hit,vertices.m_pData[i]-hit);if(d<best){best=d;nearest=i;}}
    auto* node=static_cast<MdlNodeSkin*>(surface->m_pNode);
    std::vector<Part*> parts;auto collect=[&](auto&& self,Part* p)->void{parts.push_back(p);for(int i=0;i<p->m_nChildren;++i)self(self,p->m_paChildren[i]);};
    auto* root=surface;while(root->m_pParent)root=root->m_pParent;collect(collect,root);
    const auto pool=node->m_pTree->AsModel()->m_nPool;
    auto* data=static_cast<unsigned char*>(GLRender::LockPool(pool,0,0,0x10));if(!data)return 0;
    auto* row=data+node->m_nGeomBase+nearest*node->m_nVertexStride;unsigned result=0;
    for(int i=0;i<4;++i) {
        const float weight=*reinterpret_cast<float*>(row+node->m_nBoneWeightsOfs+i*4);
        const float slot=*reinterpret_cast<float*>(row+node->m_nBoneIndicesOfs+i*4);
        if(weight<=.05f || !(slot>=0 && slot<20))continue;
        const int bone=node->m_aBoneNodeIndices[int(slot)];
        if(bone>=0 && bone<int(parts.size()))result|=classify(parts[bone]->Name());
    }
    GLRender::UnlockPool(pool);return result;
}
std::string NpcCutRegion(Gob* body,Gob* head,Part* surface,Vector hit) {
    if(head && surface->m_pGob==head)return "he";
    const unsigned owner=NpcCutSurfaceOwner(surface,hit);
    if(owner&16) {
        if(!_strnicmp(surface->Name(),"head",4)||!_strnicmp(surface->Name(),"eye",3))return "he";
        Vector neck,face,low,high;
        if((CutBone(body,"necklwr_g",neck)||CutBone(body,"headhook",neck))&&CutBone(body,"head_g",face)) {
            Vector up=face-neck;
            if(magnitude(up)<.01f && CutBone(body,"torso_g",low)&&CutBone(body,"torsoUpr_g",high))up=high-low;
            // Shoulder vertices can carry a small neck influence. That does
            // not make a chest/shoulder witness a head hit on a one-skin body.
            if(magnitude(up)>.01f && dot(hit-neck,normalize(up))>=-.02f)return "he";
        }
    }
    for(bool right:{true,false})if(owner&(right?1:2)) {
        Vector shoulder,elbow,wrist;
        if(!CutBone(body,right?"rbicep_g":"lbicep_g",shoulder)||!CutBone(body,right?"Rforearm_g":"Lforearm_g",elbow)||!CutBone(body,right?"Rhand_g":"Lhand_g",wrist))continue;
        const float length=magnitude(wrist-elbow);if(length<.01f)continue;
        const Vector distal=(wrist-elbow)/length;const float along=dot(hit-wrist,distal);
        if(along>=-.10f && along<=std::min(.28f,length)+.07f && CutCapsule(hit,wrist-distal*.1f,wrist+distal*std::min(.28f,length),std::min(.20f,length*.75f)))return right?"rh":"lh";
        const float radius=std::min(.33f,std::max(magnitude(elbow-shoulder),length));
        if(along<-.10f && (CutCapsule(hit,shoulder,elbow,radius)||CutCapsule(hit,elbow,wrist-distal*.04f,radius)))return right?"ra":"la";
    }
    std::string leg;float closest=.25f;
    bool legsAllowed=true;
    if(body->m_pModel)for(const auto& rig:nativeNpcRigs)if(!_stricmp(rig.native.c_str(),"n_tusken"))
        for(const auto* name:{&rig.native,&rig.single,&rig.staff,&rig.dual,&rig.red,&rig.blue})if(!_stricmp(body->m_pModel->m_szName,name->c_str()))legsAllowed=false;
    for(bool right:{true,false})if(legsAllowed && (owner&(right?4:8))) {
        Vector knee,ankle;if(!CutBone(body,right?"rshin_g":"lshin_g",knee)||!CutBone(body,right?"rfoot_g":"lfoot_g",ankle))continue;
        const float distance=CutCapsuleDistance(hit,knee,ankle);if(distance<=closest){closest=distance;leg=right?"rl":"ll";}
    }
    if(!leg.empty())return leg;
    if(owner&32) {
        Vector low,high,pelvis;if(!CutBone(body,"torso_g",low)||!CutBone(body,"torsoUpr_g",high)||!CutBone(body,"pelvis_g",pelvis))return "";
        const Vector up=normalize(high-low);const float height=dot(hit-pelvis,up);
        if(height>=-.18f && height<=.18f && CutCapsule(hit,pelvis-up*.08f,pelvis+up*.08f,std::min(.40f,magnitude(high-low)*1.8f)))return "wa";
    }
    return "";
}
std::string CutRegion(Gob* body,Gob* head,Part* surface,Vector hit){
    if(!body||!surface||!K1SaberContact::Finite(hit))return "";
    const char* resource=body->m_pModel?body->m_pModel->m_szName:"";
    for(const auto& rig:nativeNpcRigs)if(_stricmp(rig.native.c_str(),"n_darkjedim") && _stricmp(rig.native.c_str(),"n_sithsoldier"))
        for(const auto* name:{&rig.native,&rig.single,&rig.staff,&rig.dual,&rig.red,&rig.blue})if(!_stricmp(resource,name->c_str()))return NpcCutRegion(body,head,surface,hit);
    const char* mesh=surface->Name();
    const bool right=!_stricmp(mesh,"RArm"),left=!_stricmp(mesh,"LArm");
    if(right||left){
        Vector shoulder,elbow,wrist;
        if(!CutBone(body,right?"rbicep_g":"lbicep_g",shoulder)||!CutBone(body,right?"Rforearm_g":"Lforearm_g",elbow)||
           !CutBone(body,right?"Rhand_g":"Lhand_g",wrist))return "";
        const float forearm=magnitude(wrist-elbow);if(forearm<.01f)return "";
        const Vector distal=(wrist-elbow)/forearm;
        // Local hand capsule only: at most10cm proximal to the wrist. Surface
        // ownership also prevents a nearby chest/shoulder from becoming a hand.
        const float handLength=std::min(.28f,forearm),handRadius=std::min(.20f,forearm*.75f);
        const float along=dot(hit-wrist,distal);
        if(along>=-.10f&&along<=handLength+.07f&&CutCapsule(hit,wrist-distal*.10f,wrist+distal*handLength,handRadius))return right?"rh":"lh";
        if(along>=-.10f)return "";
        const float radius=std::min(.33f,std::max(magnitude(elbow-shoulder),forearm));
        if(CutCapsule(hit,shoulder,elbow,radius)||CutCapsule(hit,elbow,wrist-distal*.04f,radius))return right?"ra":"la";
        return "";
    }
    if(!_stricmp(mesh,"head")||!_strnicmp(mesh,"eye",3)||(head&&surface->m_pGob==head)){
        std::vector<K1SaberContact::Triangle> triangles;CollisionPartTriangles(surface,triangles);
        Vector low(1e6f,1e6f,1e6f),high(-1e6f,-1e6f,-1e6f);
        for(const auto& t:triangles)for(const auto& v:{t.a,t.b,t.c}){
            low.x=std::min(low.x,v.x);low.y=std::min(low.y,v.y);low.z=std::min(low.z,v.z);
            high.x=std::max(high.x,v.x);high.y=std::max(high.y,v.y);high.z=std::max(high.z,v.z);
        }
        if(!triangles.empty()&&hit.x>=low.x-.04f&&hit.x<=high.x+.04f&&hit.y>=low.y-.04f&&hit.y<=high.y+.04f&&hit.z>=low.z-.04f&&hit.z<=high.z+.04f)return "he";
    }
    if(!_stricmp(mesh,"torso")){
        // Native torso skin includes proximal arms and legs. The shoulder/outer
        // pec pocket stays local and keeps the sternum out of arm selection.
        std::string leg;float legDistance=.25f;
        for(bool right:{true,false}){
            Vector shoulder,elbow,opposite;
            if(CutBone(body,right?"rbicep_g":"lbicep_g",shoulder)&&CutBone(body,right?"Rforearm_g":"Lforearm_g",elbow)&&CutBone(body,right?"lbicep_g":"rbicep_g",opposite)){
                const Vector axis=normalize(elbow-shoulder);
                const Vector lateral=normalize(shoulder-opposite);
                if(dot(hit-shoulder,axis)>=-.12f&&dot(hit-shoulder,lateral)>=-.12f&&magnitude(hit-shoulder)<=.27f)return right?"ra":"la";
            }
            Vector knee,ankle;
            if(CutBone(body,right?"rshin_g":"lshin_g",knee)&&CutBone(body,right?"rfoot_g":"lfoot_g",ankle)){
                const float distance=CutCapsuleDistance(hit,knee,ankle);
                // Wider capsules overlap: select the closer leg only inside
                // the bounded local region, rather than always the right leg.
                if(distance<=legDistance){legDistance=distance;leg=right?"rl":"ll";}
            }
        }
        if(!leg.empty())return leg;
        Vector low,high,pelvis;if(!CutBone(body,"torso_g",low)||!CutBone(body,"torsoUpr_g",high)||!CutBone(body,"pelvis_g",pelvis))return "";
        const Vector up=normalize(high-low);const float height=dot(hit-pelvis,up);
        const float radius=std::min(.40f,magnitude(high-low)*1.8f);
        if(height>=-.18f&&height<=.18f&&CutCapsule(hit,pelvis-up*.08f,pelvis+up*.08f,radius))return "wa";
    }
    return "";
}
// Historical JKA boundary oracle for component comparisons; local fatal cuts
// are now guaranteed after the existing saber/region eligibility gates.
bool CutChance(float roll,float threshold){
    return std::isfinite(roll)&&roll>=1&&roll<=100&&std::isfinite(threshold)&&threshold>0&&std::floor(roll)<=threshold*2;
}
bool CutSaberContact(CSWSCreature* attacker,const MeleeContact& c) {
    const int slot=c.dualWeapon!=OBJECT_INVALID&&c.offhand?CSWInventory::SLOT_LEFTWEAPON:CSWInventory::SLOT_RIGHTWEAPON;
    auto* item=attacker&&attacker->m_pInventory?attacker->m_pInventory->GetItemInSlot(slot):nullptr;
    auto* client=attacker?attacker->GetClientCreature():nullptr;
    if(!item||!item->IsLightsaber()||!client||(c.dualWeapon!=OBJECT_INVALID&&item->m_objectId!=c.dualWeapon))return false;
    if(c.dualWeapon!=OBJECT_INVALID){K1SaberContact::Blade beam;return DualBlade(client,c.offhand?1:0,beam);}
    Vector a,b;return Blade(client,a,b);
}
int SampleDismemberment(CSWSCreature* attacker,CSWSCreature* target,const MeleeContact& c) {
    if(CutDefinitions().empty()||!c.hitPart||!attacker||!target)return -1;
    if(!CutSaberContact(attacker,c))return -1;
    auto* client=target->GetClientCreature();auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
    if(!body||cutCorpses.count(client->m_objectId))return -1;
    const auto code=CutRegion(body,client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1),c.hitPart,c.hitPoint);
    int direction=-1;auto npc=npcPlayback.find(attacker->m_objectId);
    if(K1EngageOwnsPlayer(attacker->m_objectId)&&c.serial==attackSerial)direction=swingDirection;
    else if(npc!=npcPlayback.end()&&npc->second.serial==c.serial)direction=npc->second.direction;
    if(code.empty()||!CutDirectionAllowed(code,direction)){
        fprintf(stderr,"K1_DISMEMBER_REGION_REJECT mesh=%s cut=%s direction=%d reason=%s\n",c.hitPart->Name(),code.c_str(),direction,code.empty()?"outside_local_region":"swing_direction");return -1;
    }
    const auto& defs=CutDefinitions();
    for(int row=0;row<int(defs.size());++row){const auto& d=defs[row];
        if(d.code!=code||_stricmp(d.body.c_str(),NativeCutBody(client))||!CutResources(d))continue;
        auto* slot=client->GetModel(d.slot,1);if(!slot||!slot->m_pModel||(d.slot==CSWCAnimBase::ANIM_MODEL_HEAD&&_stricmp(slot->m_pModel->m_szName,d.source.c_str())))continue;
        fprintf(stderr,"K1_DISMEMBER_REGION attacker=%llu target=%llu serial=%d mesh=%s cut=%s direction=%d fatal_policy=guaranteed\n",c.attacker,c.target,c.serial,c.hitPart->Name(),code.c_str(),direction);return row;
    }
    return -1;
}
struct CutRoll {CSWCCreature* owner;unsigned long long area,effect;};
std::map<unsigned long long,CutRoll> cutRolls;
void RegisterCutCorpse(CSWSCreature* target,int row,unsigned long long effect,Vector motion=Vector()){
    const auto* d=CutAt(row);auto* client=target?target->GetClientCreature():nullptr;
    if(!d||!client||cutCorpses.count(client->m_objectId)||_stricmp(NativeCutBody(client),d->body.c_str())||!CutResources(*d)||!target->GetArea())return;
    auto& corpse=cutCorpses.emplace(client->m_objectId,CutCorpse{client,static_cast<CSWSArea*>(target->GetArea())->m_objectId,row}).first->second;
    corpse.diedAt=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    corpse.swingMotion=motion;
    if(auto* head=client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1))if(head->m_pModel){corpse.headResource=head->m_pModel->m_szName;corpse.headScale=head->m_fObjectScale;corpse.headTextures=CutTextures(head);}
    if(auto* weapon=client->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1))if(weapon->m_pModel){corpse.weaponResource=weapon->m_pModel->m_szName;corpse.weaponScale=weapon->m_fObjectScale;corpse.weaponTextures=CutTextures(weapon);}
    if(auto* weapon=client->GetModel(CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY,1))if(weapon->m_pModel){corpse.leftWeaponResource=weapon->m_pModel->m_szName;corpse.leftWeaponScale=weapon->m_fObjectScale;corpse.leftWeaponTextures=CutTextures(weapon);}
    fprintf(stderr,"K1_DISMEMBER_FATAL actor=%llu effect=%llu cut=%s\n",target->m_objectId,effect,d->code.c_str());
}
void ClearDismemberArea(unsigned long long area) {
    for(auto it=cutRolls.begin();it!=cutRolls.end();)if(area==OBJECT_INVALID||it->second.area==area)it=cutRolls.erase(it);else ++it;
    const auto before=cutCorpses.size();
    for(auto it=cutCorpses.begin();it!=cutCorpses.end();) {
        if(area==OBJECT_INVALID||it->second.area==area){DeleteCutParts(it->second);it=cutCorpses.erase(it);}else ++it;
    }
    if(before!=cutCorpses.size())fprintf(stderr,"K1_DISMEMBER_AREA_CLEAN released=%zu remaining=%zu parts_created=%llu parts_deleted=%llu\n",before-cutCorpses.size(),cutCorpses.size(),cutPartsCreated,cutPartsDeleted);
}
void UpdateDismemberCorpses(CAurScene* rendered) {
    auto* server=g_pAppManager->m_pServerExoApp;
    if(!server||!server->m_pInternal)cutRolls.clear();
    if(!rendered)for(auto it=cutRolls.begin();it!=cutRolls.end();){
        auto* actor=server->GetCreatureByGameObjectID(it->first);
        if(!actor||actor->GetClientCreature()!=it->second.owner||(actor->GetCurrentHitPoints(0)>0&&!actor->GetDead()&&!actor->GetDeadTemp()))it=cutRolls.erase(it);else ++it;
    }
    for(auto it=cutCorpses.begin();it!=cutCorpses.end();) {
        auto& c=it->second;auto* client=g_pAppManager->m_pClientExoApp->GetCreatureByGameObjectID(it->first);
        if(client!=c.owner){DeleteCutParts(c);it=cutCorpses.erase(it);continue;}
        auto* alive=client->GetServerCreature();
        if(!rendered && alive && alive->GetCurrentHitPoints(0)>0 && !alive->GetDead() && !alive->GetDeadTemp()) {
            DeleteCutParts(c);it=cutCorpses.erase(it);
            auto appearance=LiveAppearance(client);client->m_pCreatureAppearance->m_nAppearanceId=0xffff;
            client->SetAppearance(&appearance,0x0f,1,0);ReattachCutActor(client);
            fprintf(stderr,"K1_DISMEMBER_REVIVED actor=%llu\n",client->m_objectId);continue;
        }
        const auto* d=CutAt(c.row);auto* model=client->GetModel(d->slot,1);
        if(!rendered && model && model->m_pModel && _stricmp(model->m_pModel->m_szName,d->retained.c_str()) && !IsSourceBody(model) && !c.nativeReloaded) {
            c.nativeReloaded=true;auto appearance=LiveAppearance(client);client->m_pCreatureAppearance->m_nAppearanceId=0xffff;
            const bool rebuilt=client->SetAppearance(&appearance,0x0f,1,0)!=0;ReattachCutActor(client);
            fprintf(stderr,"K1_DISMEMBER_NATIVE_RELOAD_%s actor=%llu\n",rebuilt?"PASS":"FAIL",client->m_objectId);++it;continue;
        }
        if(model && model->m_pModel && !_stricmp(model->m_pModel->m_szName,d->retained.c_str()))RestoreCutCapTextures(model);
        auto* nativeBody=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);bool nativeDeath=false;const char* deathClip=nullptr;
        if(nativeBody)for(const char* clip:{"die","die1","die3","dead","dead1","dead3","cdie","cdead"}) {float time,length;if(nativeBody->GetAnimation(const_cast<char*>(clip),&time,&length)){nativeDeath=true;deathClip=clip;break;}}
        if(rendered && nativeDeath && model && model->m_pScene==rendered && model->m_pModel && !_stricmp(model->m_pModel->m_szName,d->retained.c_str()) && !c.limb) {
            c.limb=static_cast<Gob*>(NewCAurObject(const_cast<char*>(d->detached.c_str()),nullptr,nullptr,nullptr));
            if(c.limb) {
                ++cutPartsCreated;
                const auto textures=CutTextures(model);ApplyCutTextures(c.limb,textures);
                c.texturesMatch=CutTexturesMatch(c.limb,textures);
                c.limb->TurnOffShadows();
                c.deathClip=deathClip;
                if(!FreezeFinalCutPose(c.limb,deathClip,c.finalTime)){
                    fprintf(stderr,"K1_DISMEMBER_FINAL_FAIL actor=%llu reason=clip\n",client->m_objectId);DeleteCutParts(c);++it;continue;
                }
                Vector position;Quaternion orientation;model->m_pPart->GetWorld(position,orientation);
                c.limb->m_pPart->m_vLocalPosition=Vector(0,0,0);c.limb->m_pPart->m_qLocalOrientation=Quaternion();
                c.limb->SetPosition(position);c.limb->SetOrientation(orientation);c.launchOrientation=orientation;
                c.limb->m_bBonesDirty=true;c.limb->m_fObjectScale=model->m_fObjectScale;c.limb->AttachToScene(model->m_pScene);
                Vector sourceCap,partCap;const bool capFrame=CutCapCenter(model,sourceCap)&&CutCapCenter(c.limb,partCap);
                if(capFrame){position=position+(sourceCap-partCap);c.limb->SetPosition(position);}
                Vector aligned;const float placement=capFrame&&CutCapCenter(c.limb,aligned)?magnitude(sourceCap-aligned):1e6f;
                fprintf(stderr,"K1_DISMEMBER_PLACEMENT_%s actor=%llu cap_error=%g\n",placement<.0001f?"PASS":"FAIL",client->m_objectId,placement);
                auto remember=[&](auto&& self,Part* part)->void {
                    c.frozenPose.push_back({part,part->m_vLocalPosition,part->m_qLocalOrientation,part->m_scale.m_fScale});
                    for(int i=0;i<part->m_nChildren;++i)self(self,part->m_paChildren[i]);
                };remember(remember,c.limb->m_pPart);
                fprintf(stderr,"K1_DISMEMBER_FINAL_POSE actor=%llu clip=%s time=%g\n",client->m_objectId,deathClip,c.finalTime);
                if(!capFrame){DeleteCutParts(c);++it;continue;}
                float lowest=CutLowest(c.limb);
                auto* native=client->GetServerCreature();auto* area=native?static_cast<CSWSArea*>(native->GetArea()):nullptr;
                Vector outward=c.swingMotion;outward.z=0;
                // Legacy diagnostic effects have no blade-motion payload.
                if(!K1SaberContact::Finite(outward)||magnitude(outward)<.0001f)outward=sourceCap-(native?native->m_position:position);
                outward.z=0;if(magnitude(outward)<.0001f)outward=Vector(1,0,0);
                outward=normalize(outward);const float yaw=std::atan2(outward.y,outward.x);
                const Vector side(std::cos(yaw),std::sin(yaw),0);c.awayDot=dot(side,outward);
                c.throwDistance=cutThrowMin+CutVisualRandom()*(cutThrowMax-cutThrowMin);c.throwOffset=side*c.throwDistance;c.launchCap=sourceCap;c.landing=position+c.throwOffset;
                c.spinAngle=(CutVisualRandom()*2-1)*.26179939f;
                fprintf(stderr,"K1_DISMEMBER_THROW actor=%llu yaw=%g distance=%g motion_dot=%g spin=%g shadows=%d\n",client->m_objectId,yaw,c.throwDistance,c.awayDot,c.spinAngle,c.limb->m_nShadows);
                float floor=area?g_pAppManager->m_pServerExoApp->ComputeHeight(area->m_resRef,sourceCap+c.throwOffset+Vector(0,0,3),1):position.z;
                if(!std::isfinite(floor)||std::fabs(floor-position.z)>4)floor=native?native->m_position.z:position.z;
                c.floor=floor;
                c.landing.z=position.z+floor+.015f-lowest;
                const float drop=std::max(0.f,position.z-c.landing.z);
                c.flightMs=std::max(350u,static_cast<unsigned int>(std::sqrt(2*drop/9.81f)*1000));
                c.sampledAt=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
                auto attach=[&](int slot,const char* anchor,Gob*& output){
                    const bool head=slot==CSWCAnimBase::ANIM_MODEL_HEAD;const bool left=slot==CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY;
                    const auto& resource=head?c.headResource:left?c.leftWeaponResource:c.weaponResource;if(resource.empty())return;
                    HideCutAttachment(client->GetModel(slot,1));
                    output=static_cast<Gob*>(NewCAurObject(const_cast<char*>(resource.c_str()),nullptr,nullptr,nullptr));
                    if(output){++cutPartsCreated;output->TurnOffShadows();if(head){float finalTime;FreezeFinalCutPose(output,c.deathClip.c_str(),finalTime);}const auto& textures=head?c.headTextures:left?c.leftWeaponTextures:c.weaponTextures;ApplyCutTextures(output,textures);c.texturesMatch=c.texturesMatch&&CutTexturesMatch(output,textures);output->m_fObjectScale=head?c.headScale:left?c.leftWeaponScale:c.weaponScale;output->AttachToObject(c.limb,const_cast<char*>(anchor),0);output->AttachToScene(model->m_pScene);}
                };
                if(d->code=="wa" || d->wholeHead)attach(CSWCAnimBase::ANIM_MODEL_HEAD,"headhook",c.head);
                if(d->code=="wa"||d->code=="ra"||d->code=="rh") {
                    attach(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,"rhand",c.weapon);
                    if(c.weapon){auto off=[&](auto&& self,Part* p)->void {if(p->m_pNode->AsMdlNodeLightsaber())p->m_bDrawEnabled=false;for(int i=0;i<p->m_nChildren;++i)self(self,p->m_paChildren[i]);};off(off,c.weapon->m_pPart);}
                }
                if(d->code=="wa"||d->code=="la"||d->code=="lh") {
                    attach(CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY,"lhand",c.leftWeapon);
                    if(c.leftWeapon){auto off=[&](auto&& self,Part* p)->void {if(p->m_pNode->AsMdlNodeLightsaber())p->m_bDrawEnabled=false;for(int i=0;i<p->m_nChildren;++i)self(self,p->m_paChildren[i]);};off(off,c.leftWeapon->m_pPart);}
                }
                const float bodyLowest=lowest;
                if(c.head)lowest=std::min(lowest,CutLowest(c.head));
                if(c.weapon)lowest=std::min(lowest,CutLowest(c.weapon));
                if(c.leftWeapon)lowest=std::min(lowest,CutLowest(c.leftWeapon));
                c.landing.z+=bodyLowest-lowest;
                c.flightMs=std::max(350u,static_cast<unsigned int>(std::sqrt(2*std::max(0.f,position.z-c.landing.z)/9.81f)*1000));
                c.flightTotalMs=c.flightMs;
                fprintf(stderr,"K1_DISMEMBER_DETACHED actor=%llu model=%s\n",it->first,d->detached.c_str());
            }
        }
        if(rendered&&c.limb){
            ++c.frozenSamples;
            for(const auto& pose:c.frozenPose){
                const float error=magnitude(pose.part->m_vLocalPosition-pose.position)+
                    1-std::fabs(dot(normalize(pose.part->m_qLocalOrientation),normalize(pose.orientation)))+
                    std::fabs(pose.part->m_scale.m_fScale-pose.scale);
                c.frozenError=std::max(c.frozenError,error);
            }
        }
        if(!rendered && c.limb && !c.landed) {
            const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
            const auto elapsed=static_cast<unsigned int>(std::max(0LL,now-c.sampledAt)/1000);
            const auto step=std::min(c.flightMs,std::min(elapsed,50u));
            if(step){
                const float fraction=float(c.flightTotalMs-c.flightMs+step)/float(c.flightTotalMs);
                Vector oldPivot,newPivot;const bool pivot=CutCapCenter(c.limb,oldPivot);
                // Apply a world-ground-plane turn, never a local-axis tumble.
                const Quaternion turned=normalize(Quaternion(Vector(0,0,1),c.spinAngle*fraction)*c.launchOrientation);
                c.flatError=std::max(c.flatError,std::max(std::fabs(turned.xdir().z-c.launchOrientation.xdir().z),
                    std::max(std::fabs(turned.ydir().z-c.launchOrientation.ydir().z),std::fabs(turned.zdir().z-c.launchOrientation.zdir().z))));
                c.limb->SetOrientation(turned);
                if(pivot&&CutCapCenter(c.limb,newPivot)){
                    const Vector shift=oldPivot-newPivot;c.limb->SetPosition(c.limb->GetPosition()+shift);c.landing=c.landing+shift;
                }
                const float lowest=std::min(CutLowest(c.limb),std::min(CutLowest(c.head),CutLowest(c.weapon)));
                c.landing.z=c.limb->GetPosition().z+c.floor+.015f-lowest;
                Vector velocity;Vector next=CSWCProjectile::AdvanceAccelerating(c.limb->GetPosition(),c.landing,Vector(0,0,-9.81f),c.flightMs,step,velocity);
                c.flightMs-=step;c.sampledAt=now;c.limb->SetPosition(c.flightMs?next:c.landing);}
            if(!c.flightMs){c.landed=true;fprintf(stderr,"K1_DISMEMBER_LANDED actor=%llu z=%g\n",it->first,c.landing.z);}
        }
        {
            if(d->code=="wa" || d->wholeHead)HideCutAttachment(client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1));
            if(d->code=="wa"||d->code=="la"||d->code=="lh")HideCutAttachment(client->GetModel(CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY,1));
            if(d->code=="wa"||d->code=="ra"||d->code=="rh")HideCutAttachment(client->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1));
        }
        ++it;
    }
}
constexpr int cutEffectMarker=0x4a4b4143;
}
void K1EngageTagMeleeDamage(unsigned long long attacker,unsigned long long target,CGameEffect* effect) {
    if(!damageContact||damageContact->attacker!=attacker||damageContact->target!=target||CutDefinitions().empty())return;
    if(damageContact->dismemberRow<0){
        auto* actor=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(attacker);
        if(!CutSaberContact(actor,*damageContact))return;
    }
    int old[22];for(int i=0;i<22;++i)old[i]=effect->GetInteger(i);
    effect->SetNumIntegers(28);for(int i=0;i<22;++i)effect->SetInteger(i,old[i]);
    effect->SetInteger(22,cutEffectMarker);effect->SetInteger(23,damageContact->dismemberRow);effect->SetInteger(24,damageContact->serial);
    Vector motion=damageContact->bladeMotion;motion.z=0;
    if(!K1SaberContact::Finite(motion)||magnitude(motion)<.0001f){
        auto* actor=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(attacker);
        Vector forward=actor?actor->m_orientation:Vector(1,0,0);forward.z=0;
        int direction=K1EngageOwnsPlayer(attacker)?swingDirection:(npcPlayback.count(attacker)?npcPlayback[attacker].direction:0);
        motion=direction==3?Vector(forward.y,-forward.x,0):direction==4?Vector(-forward.y,forward.x,0):forward;
    }
    if(!K1SaberContact::Finite(motion)||magnitude(motion)<.0001f)motion=Vector(1,0,0);
    motion=normalize(motion);
    effect->SetInteger(25,int(std::round(motion.x*1000000.f)));
    effect->SetInteger(26,int(std::round(motion.y*1000000.f)));
    effect->SetInteger(27,int(std::round(motion.z*1000000.f)));
}
void K1EngageFatalMeleeDamage(CSWSCreature* target,CGameEffect* effect) {
    if(!K1EngageEnabled()||!target||!effect||effect->m_numIntegers<25||effect->GetInteger(22)!=cutEffectMarker)return;
    const int row=effect->GetInteger(23);const auto* d=CutAt(row);auto* client=target->GetClientCreature();
    if(!d||!client||!target->GetArea()||cutRolls.count(target->m_objectId))return;
    cutRolls[target->m_objectId]={client,static_cast<CSWSArea*>(target->GetArea())->m_objectId,effect->m_effectId};
    Vector motion;
    if(effect->m_numIntegers>=28)motion=Vector(effect->GetInteger(25)/1000000.f,effect->GetInteger(26)/1000000.f,effect->GetInteger(27)/1000000.f);
    fprintf(stderr,"K1_DISMEMBER_ROLL_ACCEPT target=%llu effect=%llu cut=%s threshold=100 policy=guaranteed motion=%g,%g,%g\n",target->m_objectId,effect->m_effectId,d->code.c_str(),motion.x,motion.y,motion.z);
    RegisterCutCorpse(target,row,effect->m_effectId,motion);
}
const char* K1EngageDismemberModel(CSWCCreature* actor,const CResRef& original,int slot) {
    auto it=cutCorpses.find(actor->m_objectId);if(it==cutCorpses.end()||it->second.owner!=actor)return nullptr;
    const auto* d=CutAt(it->second.row);if(!d)return nullptr;
    if(slot==d->slot)return d->retained.c_str();
    if(slot==CSWCAnimBase::ANIM_MODEL_BODY)return original.GetResRefStr();
    return nullptr;
}
namespace {
void DismemberModelProbe(CSWGuiScene& scene) {
    if(!std::getenv("K1_ENGAGE_DISMEMBER_MODEL_TEST"))return;
    static bool done=false;if(done)return;done=true;
    bool ok=CutDefinitions().size()>=24;int models=0,samples=0,terminalSamples=0;float largest=0;
    bool directionPolicy=true;int policyCases=0;
    for(const char* code:{"ra","la","rh","lh","wa","he"})for(int direction=0;direction<7;++direction){
        const unsigned int allowed=(code[1]=='a'&&code[0]!='w')?0x67u:0x18u;
        directionPolicy=directionPolicy&&(CutDirectionAllowed(code,direction)==((allowed&(1u<<direction))!=0));++policyCases;
    }
    ok=ok&&directionPolicy;
    fprintf(stderr,"K1_DISMEMBER_DIRECTIONS_%s cases=%d\n",directionPolicy?"PASS":"FAIL",policyCases);
    int regionCases=0;bool regions=true;
    for(const char* resource:{"pmbbl","n_sithsoldier","n_darkjedim"}){
        const int idx=scene.m_aModels.m_count;auto* body=static_cast<Gob*>(scene.AddModel(CExoString(resource),-1));
        if(!body){regions=false;continue;}body->AttachToScene(nullptr);body->Reset();
        if(!_stricmp(resource,"n_darkjedim"))if(FILE* guide=fopen("cut-region-guide.json","w")){
            fputs("{\"model\":\"n_darkjedim\",\"pose\":\"native bind\",\"bones\":{",guide);bool comma=false;
            for(const char* name:{"rbicep_g","Rforearm_g","Rhand_g","lbicep_g","Lforearm_g","Lhand_g","rshin_g","rfoot_g","lshin_g","lfoot_g","pelvis_g","torso_g","torsoUpr_g"}){
                Vector p;if(CutBone(body,name,p)){fprintf(guide,"%s\"%s\":[%.7g,%.7g,%.7g]",comma?",":"",name,p.x,p.y,p.z);comma=true;}
            }
            fputs("},\"triangles\":[",guide);comma=false;
            std::vector<K1SaberContact::Triangle> triangles;std::vector<Part*> parts;CollisionTriangles(body,triangles,&parts);
            for(size_t i=0;i<triangles.size();++i){const auto& t=triangles[i];const auto region=CutRegion(body,nullptr,parts[i],(t.a+t.b+t.c)/3.f);
                fprintf(guide,"%s{\"mesh\":\"%s\",\"region\":\"%s\",\"v\":[[%.7g,%.7g,%.7g],[%.7g,%.7g,%.7g],[%.7g,%.7g,%.7g]]}",comma?",":"",parts[i]->Name(),region.c_str(),t.a.x,t.a.y,t.a.z,t.b.x,t.b.y,t.b.z,t.c.x,t.c.y,t.c.z);comma=true;
            }
            fputs("]}\n",guide);fclose(guide);fprintf(stderr,"K1_CUT_GUIDE_EXPORT triangles=%zu\n",triangles.size());
        }
        auto expect=[&](bool pass){regions=regions&&pass;++regionCases;};
        for(const char* side:{"r","l"}){
            const bool right=side[0]=='r';Vector shoulder,elbow,wrist,chest;
            const bool bones=CutBone(body,right?"rbicep_g":"lbicep_g",shoulder)&&CutBone(body,right?"Rforearm_g":"Lforearm_g",elbow)&&CutBone(body,right?"Rhand_g":"Lhand_g",wrist)&&CutBone(body,"torsoUpr_g",chest);
            expect(bones);if(!bones)continue;
            auto* arm=body->GetPart(const_cast<char*>(right?"RArm":"LArm"));
            auto* torso=body->GetPart(const_cast<char*>("Torso"));const Vector distal=normalize(wrist-elbow);
            expect(CutRegion(body,nullptr,arm,wrist+distal*.05f)==(right?"rh":"lh"));
            expect(CutRegion(body,nullptr,arm,shoulder)==(right?"ra":"la"));
            expect(CutRegion(body,nullptr,arm,(elbow+wrist)*.5f)==(right?"ra":"la"));
            expect(CutRegion(body,nullptr,arm,chest)!=(right?"rh":"lh"));
            expect(CutRegion(body,nullptr,torso,wrist)!=(right?"rh":"lh"));
            expect(CutRegion(body,nullptr,arm,wrist+distal*1.f).empty());
            Vector radial=cross(elbow-shoulder,distal);
            if(magnitude(radial)<.0001f)radial=cross(distal,Vector(0,0,1));
            if(magnitude(radial)<.0001f)radial=cross(distal,Vector(0,1,0));
            radial=normalize(radial);
            expect(CutRegion(body,nullptr,arm,wrist+distal*.05f+radial*(magnitude(wrist-elbow)*.31f))==(right?"rh":"lh"));
            expect(CutRegion(body,nullptr,arm,shoulder+radial*(std::max(magnitude(elbow-shoulder),magnitude(wrist-elbow))*.44f))==(right?"ra":"la"));
            expect(CutRegion(body,nullptr,arm,wrist+distal*.05f+radial*std::min(.13f,magnitude(wrist-elbow)*.45f))==(right?"rh":"lh"));
            expect(CutRegion(body,nullptr,arm,shoulder+radial*std::min(.22f,std::max(magnitude(elbow-shoulder),magnitude(wrist-elbow))*.65f))==(right?"ra":"la"));
            expect(CutRegion(body,nullptr,torso,shoulder)==(right?"ra":"la"));
            Vector knee,ankle;const bool legBones=CutBone(body,right?"rshin_g":"lshin_g",knee)&&CutBone(body,right?"rfoot_g":"lfoot_g",ankle);
            expect(legBones&&CutRegion(body,nullptr,torso,(knee+ankle)*.5f)==(right?"rl":"ll"));
            const auto center=CutRegion(body,nullptr,torso,chest);expect(center!="ra"&&center!="la"&&center!="rh"&&center!="lh");
            expect(CutRegion(body,nullptr,torso,Vector(50,50,50)).empty());
            expect(CutRegion(body,nullptr,arm,wrist+distal*.05f+radial*std::min(.18f,magnitude(wrist-elbow)*.65f))==(right?"rh":"lh"));
            expect(CutRegion(body,nullptr,arm,shoulder+radial*std::min(.30f,std::max(magnitude(elbow-shoulder),magnitude(wrist-elbow))*.90f))==(right?"ra":"la"));
            const Vector upper=normalize(elbow-shoulder),lateral=normalize((shoulder-chest)-upper*dot(shoulder-chest,upper));
            expect(CutRegion(body,nullptr,torso,shoulder+lateral*.23f)==(right?"ra":"la"));
            Vector otherKnee;const bool otherLeg=CutBone(body,right?"lshin_g":"rshin_g",otherKnee);
            const Vector legAxis=normalize(ankle-knee),legOut=normalize((knee-otherKnee)-legAxis*dot(knee-otherKnee,legAxis));
            expect(legBones&&otherLeg&&CutRegion(body,nullptr,torso,(knee+ankle)*.5f+legOut*.22f)==(right?"rl":"ll"));
            Vector opposite,pelvis,spineLow,spineHigh;
            const bool torsoBones=CutBone(body,right?"lbicep_g":"rbicep_g",opposite)&&CutBone(body,"pelvis_g",pelvis)&&CutBone(body,"torso_g",spineLow)&&CutBone(body,"torsoUpr_g",spineHigh);
            const Vector up=normalize(spineHigh-spineLow),out=normalize(shoulder-opposite),front=normalize(cross(out,up));
            expect(torsoBones&&CutRegion(body,nullptr,torso,shoulder-out*.08f-up*.05f)==(right?"ra":"la"));
            expect(torsoBones&&CutRegion(body,nullptr,torso,shoulder+up*.08f)==(right?"ra":"la"));
            expect(torsoBones&&CutRegion(body,nullptr,torso,pelvis-up*.15f+front*.14f)=="wa");
            expect(torsoBones&&CutRegion(body,nullptr,torso,pelvis+up*.24f+front*.10f)!="wa");
        }
        scene.RemoveModel(idx,1);
    }
    regions=regions&&CutChance(80.99f,40)&&!CutChance(81,40)&&!CutChance(1,0)&&CutChance(40.99f,20)&&!CutChance(41,20)&&CutChance(90.99f,45)&&!CutChance(91,45);
    ok=ok&&regions;fprintf(stderr,"K1_DISMEMBER_REGIONS_%s cases=%d chance_boundaries=7\n",regions?"PASS":"FAIL",regionCases);
    CGameEffect effect(1),clone(1);effect.SetNumIntegers(22);
    for(int i=0;i<22;++i)effect.SetInteger(i,i*17+3);
    MeleeContact witness={111,222,1};witness.dismemberRow=0;witness.bladeMotion=Vector(0,-.1f,0);damageContact=&witness;
    K1EngageTagMeleeDamage(111,223,&effect);ok=ok&&effect.m_numIntegers==22;
    K1EngageTagMeleeDamage(111,222,&effect);damageContact=nullptr;clone.CopyEffect(&effect,0);
    ok=ok&&clone.m_numIntegers==28&&clone.GetInteger(22)==cutEffectMarker&&clone.GetInteger(23)==0&&clone.GetInteger(25)==0&&clone.GetInteger(26)==-1000000&&clone.GetInteger(27)==0;
    for(int i=0;i<22;++i)ok=ok&&clone.GetInteger(i)==i*17+3;
    Vector hit;ok=ok&&K1SaberContact::IntersectionPoint(Vector(0,0,-1),Vector(0,0,1),{{-1,-1,0},{1,-1,0},{0,1,0}},hit)&&magnitude(hit)<1e-6;
    fprintf(stderr,"K1_DISMEMBER_PAYLOAD_%s native_fields=22 copied=1 mismatched_target=1\n",ok?"PASS":"FAIL");
    for(const auto& d:CutDefinitions())for(const auto& resource:{d.retained,d.detached}) {
        if(_strnicmp(d.retained.c_str(),"k1d",3))continue; // Original 24-pair regression remains stable.
        int index=scene.m_aModels.m_count;auto* body=static_cast<Gob*>(scene.AddModel(CExoString(resource.c_str()),-1));
        if(!body){ok=false;continue;}++models;
        if(!_stricmp(d.body.c_str(),"n_darkjedim")) {
            int faces=0;auto eyes=[&](auto&& self,Part* p)->void {
                if(!_strnicmp(p->Name(),"eye",3)&&p->AsPartTriMesh()&&static_cast<MdlNodeTriMesh*>(p->m_pNode)->m_bRender)
                    faces+=static_cast<MdlNodeTriMesh*>(p->m_pNode)->m_faces.m_count;
                for(int i=0;i<p->m_nChildren;++i)self(self,p->m_paChildren[i]);
            };eyes(eyes,body->m_pPart);
            const bool onDetached=d.code=="he"||d.code=="wa";
            const bool expected=(resource==d.detached)==onDetached;
            ok=ok&&faces==(expected?54:0);
            fprintf(stderr,"K1_DISMEMBER_EYES_%s model=%s faces=%d expected=%d\n",faces==(expected?54:0)?"PASS":"FAIL",resource.c_str(),faces,expected?54:0);
        }
        body->AttachToScene(nullptr);body->EnableForceAnimations();
        for(const char* clip:{"die","die1","die3","dead","dead1","dead3"})for(float fraction:{0.f,.25f,.5f,.75f,1.f}) {
            body->PlayAnimation(nullptr,1,0,0);body->Reset();
            bool played=body->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,0)!=0;
            float time=0,length=0;played=played&&body->GetAnimation(const_cast<char*>(clip),&time,&length);
            body->PlayAnimation(nullptr,1,0,0);body->Reset();
            played=played&&body->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,length*fraction)!=0;
            body->Animate(.000001f);ok=ok&&played;
            std::vector<Vector> rim,skin;
            auto collect=[&](auto&& self,Part* part)->void {
                if(auto* s=part->AsPartSkin())if(part->m_bDrawEnabled && static_cast<MdlNodeTriMesh*>(part->m_pNode)->m_bRender) {
                    List<Vector> vertices;if(!s->GetSkinnedWorldVertices(vertices))ok=false;
                    auto& dest=!strcmp(part->Name(),"dismember_cap")?rim:skin;
                    for(int i=0;i<vertices.m_count;++i){ok=ok&&K1SaberContact::Finite(vertices.m_pData[i]);dest.push_back(vertices.m_pData[i]);}
                }
                for(int i=0;i<part->m_nChildren;++i)self(self,part->m_paChildren[i]);
            };collect(collect,body->m_pPart);
            ok=ok&&!rim.empty()&&!skin.empty();
            for(Vector v:rim){float near=1e9f;for(Vector w:skin)near=std::min(near,magnitude(v-w));largest=std::max(largest,near);}
            ++samples;
        }
        // Compare the terminal pose to a separately loaded native original,
        // then advance the frozen run and require its local bones to stay put.
        const int oracleIndex=scene.m_aModels.m_count;
        const char* original=d.slot==CSWCAnimBase::ANIM_MODEL_HEAD?d.source.c_str():d.body.c_str();
        auto* oracle=static_cast<Gob*>(scene.AddModel(CExoString(original),-1));
        for(const char* terminalClip:{"die","die1","die3","dead","dead1","dead3"}){
        bool finalOk=oracle!=nullptr;float terminal=0,reference=0,finalError=0,drift=0;
        if(oracle){oracle->AttachToScene(nullptr);finalOk=FreezeFinalCutPose(body,terminalClip,terminal)&&FreezeFinalCutPose(oracle,terminalClip,reference);}
        std::vector<FrozenCutPart> pose;
        auto terminalParts=[&](auto&& self,Part* part)->void {
            pose.push_back({part,part->m_vLocalPosition,part->m_qLocalOrientation,part->m_scale.m_fScale});
            if(oracle)if(auto* ref=oracle->GetPart(part->Name()))finalError=std::max(finalError,magnitude(part->m_vLocalPosition-ref->m_vLocalPosition)+1-std::fabs(dot(normalize(part->m_qLocalOrientation),normalize(ref->m_qLocalOrientation))));
            for(int i=0;i<part->m_nChildren;++i)self(self,part->m_paChildren[i]);
        };terminalParts(terminalParts,body->m_pPart);body->Animate(.25f);body->Animate(.5f);
        for(const auto& p:pose)drift=std::max(drift,magnitude(p.part->m_vLocalPosition-p.position)+1-std::fabs(dot(normalize(p.part->m_qLocalOrientation),normalize(p.orientation))));
        finalOk=finalOk&&finalError<.00001f&&drift<.00001f;
        ++terminalSamples;ok=ok&&finalOk;fprintf(stderr,"K1_DISMEMBER_TERMINAL_%s model=%s clip=%s time=%g reference_error=%g drift=%g\n",finalOk?"PASS":"FAIL",resource.c_str(),terminalClip,terminal,finalError,drift);
        }
        if(oracle)scene.RemoveModel(oracleIndex,1);
        scene.RemoveModel(index,1);
    }
    ok=ok&&largest<.00001f&&terminalSamples==288;
    fprintf(stderr,"K1_DISMEMBER_TERMINAL_SUMMARY samples=%d\n",terminalSamples);
    fprintf(stderr,"K1_DISMEMBER_MODELS_%s models=%d native_death_samples=%d maximum_cap_gap=%g\n",ok?"PASS":"FAIL",models,samples,largest);
}
#include "k1npcdismemberprobe.h"
bool DismemberLiveContactProbe(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* enemy) {
    static int trial=0;static bool started=false;
    static int finishSerial=-1;static long long finishDeadline=0,finishDeath=0;
    static float finishTime=0,finishLength=0;
    static Vector cameraPlayerStart,cameraEyeStart,cameraTurnStart;
    static unsigned long long current=OBJECT_INVALID;static int lateCount=0,totalCuts=0;
    const bool lethal=std::getenv("K1_ENGAGE_DISMEMBER_LETHAL_TEST")!=nullptr;
    if(lethal&&current!=OBJECT_INVALID)enemy=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(current);
    auto quit=[] {if(FILE* f=fopen("dark-jedi-input.txt","w")){fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* why){fprintf(stderr,"K1_DISMEMBER_LIVE_FAIL phase=%d reason=%s\n",phase,why);phase=-1;quit();};
    if(!enemy){fail("actor_missing");return true;}
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    arenaForce.next=now+60000000;
    auto nextTrial=[&](){
        if(++trial==21){fprintf(stderr,"K1_DISMEMBER_LETHAL_DONE kills=21 cuts=%d\n",totalCuts);phase=-1;quit();return;}
        auto* next=SpawnArenaOpponent(player,player->m_position+Vector(1.05f,0,0),false,false,false,"k1darktest");
        if(!next){fail("spawn");return;}current=next->m_objectId;phase=2;ticks=0;
    };
    if(lethal&&phase>=602&&phase<=605){
        auto* module=g_pAppManager->m_pClientExoApp->GetModule();auto* camera=module->m_pCamera;
        auto* follow=static_cast<CSWCameraOnAStick*>(camera->GetBehavior(BEHAVIOR_CAMERA_ON_A_STICK));
        if(state.mode!=K1EngageState::Mode::Exploration||!follow||follow->m_pTargetObject.m_pObject!=player->GetClientCreature()->GetCameraFollowPart()){fail("post_victory_follow_lost");return true;}
        if(phase==602&&ticks>=25){
            cameraPlayerStart=player->m_position;cameraEyeStart=camera->GetPosition();
            if(FILE* f=fopen("dark-jedi-input.txt","w")){fputs("keydown:26\n+30:keyup:26\n",f);fclose(f);}
            phase=603;ticks=0;
        }else if(phase==603&&ticks>=45){
            Vector movement=player->m_position-cameraPlayerStart,eye=camera->GetPosition()-cameraEyeStart;movement.z=eye.z=0;
            const float distance=magnitude(movement);
            if(distance<.3f||dot(eye,movement)<.4f*distance*distance){fprintf(stderr,"K1_VICTORY_CAMERA_MOVE_TRACE player=%g eye=%g alignment=%g\n",distance,magnitude(eye),dot(eye,movement));fail("camera_did_not_follow_native_movement");return true;}
            fprintf(stderr,"K1_VICTORY_CAMERA_MOVE_PASS trial=%d player_distance=%g camera_distance=%g\n",trial,distance,magnitude(eye));
            cameraTurnStart=camera->GetPosition()-player->m_position;cameraTurnStart.z=0;cameraTurnStart.Normalize();
            // The documented profile uses Z/C camera rotation; A/D move.
            if(FILE* f=fopen("dark-jedi-input.txt","w")){fputs("keydown:29\n+20:keyup:29\n",f);fclose(f);}
            phase=604;ticks=0;
        }else if(phase==604&&ticks>=35){
            Vector orbit=camera->GetPosition()-player->m_position;orbit.z=0;orbit.Normalize();
            if(dot(orbit,cameraTurnStart)>.999f){fail("native_camera_turn_unresponsive");return true;}
            fprintf(stderr,"K1_VICTORY_CAMERA_TURN_PASS trial=%d orbit_dot=%g\n",trial,dot(orbit,cameraTurnStart));nextTrial();
        }
        return true;
    }
    if(phase==2){
        current=enemy->m_objectId;
        enemy->m_bPlot=1;enemy->m_hitPoints=10000;enemy->SetCurrentHitPoints(10000);
        if(!K1EngageEngaged()&&!K1EngageEnter()){fail("enter");return true;}
        enemy->SetPosition(player->m_position+Vector(1.05f,0,0),1,1,0);enemy->AddToVisibleList(player->m_objectId,1,0,0,0);
        phase=600;ticks=0;return true;
    }
    if(lethal&&phase==601&&(enemy->GetDead()||enemy->GetDeadTemp())){
        auto* client=player->GetClientCreature();auto* model=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
        if(finishSerial!=attackSerial){
            finishSerial=attackSerial;finishDeath=now;
            if(!state.attacking||!model||!currentOverlay||!model->GetAnimation(const_cast<char*>(currentOverlay),&finishTime,&finishLength)||finishTime>=finishLength){fail("victory_swing_cut_off_at_death");return true;}
            finishDeadline=now+static_cast<long long>((finishLength-finishTime)/playerMeleeSwingRate*1000000.f);
            // Exercise queued-chain and held-guard cancellation at victory.
            state.attackRequested=state.holdChain=state.rightHeld=true;
        }
        if(state.mode!=K1EngageState::Mode::Exploration){if(ticks>240)fail("victory_window_timeout");return true;}
        auto* module=g_pAppManager->m_pClientExoApp->GetModule();
        auto* camera=module->m_pCamera;
        auto* follow=static_cast<CSWCameraOnAStick*>(camera->GetBehavior(BEHAVIOR_CAMERA_ON_A_STICK));
        const bool follows=follow&&follow->m_pTargetObject.m_pObject==client->GetCameraFollowPart();
        fprintf(stderr,"K1_VICTORY_CAMERA_TARGET trial=%d type=%d combat=%d target_present=%d follows_player=%d\n",trial,camera->GetBehavior(-1)?camera->GetBehavior(-1)->GetType():-1,g_pAppManager->m_pClientExoApp->GetCombatMode(),follow&&follow->m_pTargetObject.m_pObject,follows);
        if(!follows){fail("victory_camera_lost_player_target");return true;}
        if(now+40000<finishDeadline||K1EngageOwnsAnimation(model)||!CheckRigActorNativePose(client,"finished_swing")){
            fail("victory_restored_before_swing_end");return true;
        }
        fprintf(stderr,"K1_VICTORY_SWING_PASS trial=%d serial=%d death_time=%g length=%g elapsed_us=%lld\n",trial,attackSerial,finishTime,finishLength,now-finishDeath);
        const auto id=enemy->GetClientCreature()->m_objectId;
        if(cutCorpses.count(id))++totalCuts;
        fprintf(stderr,"K1_DISMEMBER_LETHAL_TRIAL trial=%d dead=1 cut=%d health=%d\n",trial,int(cutCorpses.count(id)),enemy->GetCurrentHitPoints(0));
        if(trial<2){phase=602;ticks=0;}else nextTrial();return true;
    }
    auto found=npcPlayback.find(enemy->m_objectId);
    if(found==npcPlayback.end()){if(ticks>240)fail("source_owner");return true;}
    auto& npc=found->second;npc.nextThink=npc.nextAttack=npc.parryReady=now+60000000;StopNpcMove(npc,enemy);
    npc.attacking=npc.parrying=false;npc.chainSwings=0;
    if(lethal){npc.facingYaw=npc.previousFacingYaw=0;enemy->SetOrientation(Vector(1,0,0));}
    if(phase==600){
        if(state.attacking)return true;
        if(trial==21){fprintf(stderr,"K1_DISMEMBER_LIVE_DONE swings=%d\n",trial);phase=-1;quit();return true;}
        state.ClearInputs();movement.Clear();keyW=keyS=keyA=keyD=keyShift=false;aimYaw=0;aimPitch=0;
        player->SetPosition(Vector(200,100,0),1,1,0);player->GetClientCreature()->SetPrecalculatedPosition(Vector(200,100,0));player->SetOrientation(Vector(1,0,0));
        const float distances[]={.7f,1.f,1.3f};enemy->SetPosition(Vector(200+distances[trial/7],100,0),1,1,0);
        npc.position=npc.previousPhysics=enemy->m_position;npc.physicsRemainder=0;
        if(lethal){enemy->m_bPlot=0;enemy->SetCurrentHitPoints(1);}
        K1EngageMouse(true,true);K1EngageMouse(true,false);requestedDirection=trial%7;requestedBackLeft=false;
        fprintf(stderr,"K1_DISMEMBER_LIVE_SWING trial=%d direction=%d distance=%g\n",trial,requestedDirection,distances[trial/7]);
        started=false;phase=601;ticks=0;return true;
    }
    if(phase==601){
        started=started||state.attacking;
        if(started&&!state.attacking){if(lethal){fail("expected_native_death");return true;}++trial;phase=600;ticks=0;}
        else if(ticks>240)fail("swing_timeout");
    }
    return true;
}
bool DismemberArenaProbe(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* enemy) {
    if(std::getenv("K1_ENGAGE_DISMEMBER_CONTACT_TEST"))return DismemberLiveContactProbe(phase,ticks,player,enemy);
    if(!std::getenv("K1_ENGAGE_DISMEMBER_TEST"))return false;
    static unsigned long long clientId=OBJECT_INVALID,currentEnemy=OBJECT_INVALID;static int selected=-1,trial=0;
    static std::vector<unsigned long long> retained;static bool revived=false;static int beforeNonlethal=0;
    if(currentEnemy!=OBJECT_INVALID)enemy=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(currentEnemy);
    auto quit=[] {if(FILE* f=fopen("dark-jedi-input.txt","w")){fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* reason){fprintf(stderr,"K1_DISMEMBER_ARENA_FAIL phase=%d reason=%s\n",phase,reason);phase=-1;quit();};
    if(phase==2) {
        if(!Actor() || !enemy){if(ticks>600)fail("player_fixture_timeout");return true;}
        // Native replication advances after leaving planning. Follow the
        // established Dark Jedi fixture: enter first, then await the NPC body.
        if(!K1EngageEngaged() && !K1EngageEnter()){fail("setup");return true;}
        // The ordinary arena spawns this actor19m away. The established Dark
        // Jedi fixture brings it into relevance before requiring a client body.
        if(enemy) {enemy->SetPosition(player->m_position+player->m_orientation*1.2f,1,1,0);enemy->AddToVisibleList(player->m_objectId,1,0,0,0);}
        // ArenaTick creates the server fixture before its client body arrives.
        // Wait for replication before sampling, rather than before entering.
        if(!Actor() || !enemy || !enemy->GetClientCreature() || !enemy->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1)) {
            if(ticks>600)fail("client_fixture_timeout");return true;
        }
        clientId=enemy->GetClientCreature()->m_objectId;
        currentEnemy=enemy->m_objectId;
        const char* codes[]={"ra","rh","la","lh","rl","ll","wa","he"};selected=-1;
        const char* native=trial<8?"n_darkjedim":"n_sithsoldier";
        auto& defs=CutDefinitions();for(int i=0;i<int(defs.size());++i)if(!_stricmp(defs[i].body.c_str(),native)&&defs[i].code==codes[trial%8])selected=i;
        if(selected<0){fail("cut_resources");return true;}phase=trial==0&&!revived?490:500;ticks=0;return true;
    }
    if(phase==490 && ticks>30) {
        enemy->m_bPlot=0;enemy->m_hitPoints=1000;enemy->SetCurrentHitPoints(1000);beforeNonlethal=1000;
        // Exercise the production witness/rule adapter on real posed skin.
        // Blade motion is synthetic; source region geometry is unchanged.
        // All-location death trials below remain explicit picks.
        auto* body=enemy->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        std::vector<K1SaberContact::Triangle> triangles;std::vector<Part*> parts;
        CollisionTriangles(body,triangles,&parts);MeleeContact contact={player->m_objectId,enemy->m_objectId,attackSerial};
        int sampled=-1;
        for(size_t i=0;i<triangles.size()&&sampled<0;++i) {
            if(_stricmp(parts[i]->Name(),"RArm")&&_stricmp(parts[i]->Name(),"LArm"))continue;
            const auto& t=triangles[i];const auto& d=*CutAt(selected);auto* anchor=body->GetPart(const_cast<char*>(d.anchor.c_str()));
            Vector pos;Quaternion q;anchor->GetWorld(pos,q);Vector axis=normalize(q.xdir()*d.axis.x+q.ydir()*d.axis.y+q.zdir()*d.axis.z);
            Vector normal=cross(t.b-t.a,t.c-t.a);normal=normal-axis*dot(normal,axis);if(magnitude(normal)<1e-7f)continue;normal=normalize(normal);
            const Vector blade=normalize(cross(axis,normal)),center=(t.a+t.b+t.c)*(1.f/3.f);
            const K1SaberContact::Blade a={center-normal*.03f-blade*.15f,center-normal*.03f+blade*.15f};
            const K1SaberContact::Blade b={center+normal*.03f-blade*.15f,center+normal*.03f+blade*.15f};
            if(!K1SaberContact::SweepPoint(a,b,t,contact.hitPoint))continue;
            contact.hitPart=parts[i];contact.bladeMotion=normal*.06f;contact.bladeDirection=blade*.3f;
            const int saved=swingDirection;
            swingDirection=3; // This synthetic contact targets a hand region.
            sampled=SampleDismemberment(player,enemy,contact);
            swingDirection=saved;
        }
        if(sampled<0){fail("posed_surface_selection");return true;}
        fprintf(stderr,"K1_DISMEMBER_SURFACE_PASS row=%d real_posed_skin=1 synthetic_blade_motion=1\n",sampled);
        contact.hitPart=nullptr;contact.dismemberRow=sampled;
        damageContact=&contact;bool sent=ContactDamage(player,enemy,false);damageContact=nullptr;
        if(!sent){fail("nonlethal_queue");return true;}phase=491;ticks=0;return true;
    }
    if(phase==491) {
        if(enemy->GetCurrentHitPoints(0)==beforeNonlethal){if(ticks>120)fail("nonlethal_timeout");return true;}
        if(enemy->GetCurrentHitPoints(0)<=0||cutCorpses.count(clientId)){fail("nonlethal_cut");return true;}
        fprintf(stderr,"K1_DISMEMBER_NONLETHAL_PASS tagged=1 hp=%d\n",enemy->GetCurrentHitPoints(0));
        auto* effect=new CGameEffect(1);effect->m_subType=CSWSEffectListHandler::EFFECT_SETSTATE;
        effect->m_durationTypeBits=CGameEffect::DURATION_TYPE_TEMPORARY;effect->m_fDuration=10;
        effect->SetCreator(player->m_objectId);effect->SetInteger(0,EFFECT_STATE_STUNNED);effect->SetInteger(1,1);enemy->ApplyEffect(effect,0,1);
        phase=492;ticks=0;return true;
    }
    if(phase==492) {
        auto* body=enemy->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(IsSourceBody(body)||!(enemy->m_nState&EFFECT_STATE_STUNNED)){if(ticks>180)fail("native_stun_fixture");return true;}
        fprintf(stderr,"K1_DISMEMBER_STUN_FIXTURE_PASS body=%s\n",body->m_pModel->m_szName);phase=500;ticks=0;return true;
    }
    if(phase==500 && ticks>30) {
        if(!enemy){fail("missing_enemy");return true;}
        enemy->m_bPlot=0;enemy->SetCurrentHitPoints(1);
        MeleeContact contact={player->m_objectId,enemy->m_objectId,attackSerial};contact.dismemberRow=selected;
        contact.bladeMotion=trial%2?Vector(0,.1f,0):Vector(0,-.1f,0);
        damageContact=&contact;bool sent=ContactDamage(player,enemy,false);damageContact=nullptr;
        if(!sent){fail("native_damage_queue");return true;}
        fprintf(stderr,"K1_DISMEMBER_TEST_SELECTION synthetic_region=1 actual_native_damage=1 row=%d\n",selected);
        phase=501;ticks=0;return true;
    }
    if(phase==501) {
        auto found=cutCorpses.find(clientId);auto* client=g_pAppManager->m_pClientExoApp->GetCreatureByGameObjectID(clientId);
        auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
        if(found!=cutCorpses.end()&&found->second.limb&&body&&!IsSourceBody(body)&&!_stricmp(body->m_pModel->m_szName,CutAt(selected)->retained.c_str())) {
            fprintf(stderr,"K1_DISMEMBER_NATIVE_DEATH_PASS actor=%llu body=%s limb=%s\n",clientId,body->m_pModel->m_szName,found->second.limb->m_pModel->m_szName);
            if(!found->second.texturesMatch){fail("material_identity");return true;}
            fprintf(stderr,"K1_DISMEMBER_MATERIAL_PASS actor=%llu\n",clientId);
            if(trial==0&&!revived){
                enemy->m_bRaiseable=1;auto* effect=new CGameEffect(1);effect->m_subType=CSWSEffectListHandler::EFFECT_RESURRECTION;
                effect->m_durationTypeBits=CGameEffect::DURATION_TYPE_INSTANTANEOUS;effect->SetCreator(player->m_objectId);enemy->ApplyEffect(effect,0,1);
                phase=510;ticks=0;return true;
            }
            phase=502;ticks=0;return true;
        }
        if(ticks>240){fprintf(stderr,"K1_DISMEMBER_TRACE records=%zu model=%s dead=%d\n",cutCorpses.size(),body?body->m_pModel->m_szName:"none",enemy?enemy->GetDead():-1);fail("native_death_handoff");}
    }
    if(phase==510) {
        auto* client=enemy?enemy->GetClientCreature():nullptr;auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
        if(cutCorpses.count(clientId)||!body||!enemy||enemy->GetCurrentHitPoints(0)<1||enemy->GetDead()||!_strnicmp(body->m_pModel->m_szName,"jk",2)) {
            if(ticks>240)fail("native_revival");return true;
        }
        fprintf(stderr,"K1_DISMEMBER_REVIVAL_PASS body=%s parts_deleted=%llu\n",body->m_pModel->m_szName,cutPartsDeleted);
        revived=true;phase=500;ticks=0;return true;
    }
    if(phase==502) {
        auto found=cutCorpses.find(clientId);if(found==cutCorpses.end()){fail("lost_cut_corpse");return true;}
        const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
        if(!found->second.landed || now-found->second.diedAt<3200000){if(ticks>600)fail("death_completion_timeout");return true;}
        g_pAppManager->m_pServerExoApp->GetServerAIMaster()->AddEventDeltaTime(0,0,player->m_objectId,currentEnemy,CServerAIMaster::EVENT_DESTROY_OBJECT,nullptr);
        phase=503;ticks=0;return true;
    }
    if(phase==503) {
        auto* original=g_pAppManager->m_pClientExoApp->GetCreatureByGameObjectID(clientId);
        if(enemy || !original || !original->m_pArea || !original->m_pArea->GetIsObjectCorpse(clientId)){if(ticks>600)fail("server_retirement");return true;}
        auto found=cutCorpses.find(clientId);
        if(found==cutCorpses.end()){fail("lost_cut_corpse");return true;}
        auto& c=found->second;
        if(c.frozenSamples<2||c.frozenError>.00001f){fail("final_pose_not_frozen");return true;}
        const Vector expectedMotion=trial%2?Vector(0,1,0):Vector(0,-1,0);
        if(magnitude(c.swingMotion-expectedMotion)>.00001f||dot(c.throwOffset,expectedMotion)<cutThrowMin-.001f){fail("swing_motion_transport");return true;}
        if(c.awayDot<.999f||c.throwDistance<cutThrowMin||c.throwDistance>cutThrowMax||std::fabs(c.spinAngle)>.2618f){fail("throw_bounds");return true;}
        for(auto* detached:{c.limb,c.head,c.weapon,c.leftWeapon})if(detached&&detached->m_nShadows){fail("detached_shadow");return true;}
        if(c.flatError>.00001f){fail("throw_tilts_final_pose");return true;}
        fprintf(stderr,"K1_DISMEMBER_FLAT_PASS trial=%d maximum_tilt_error=%g\n",trial,c.flatError);
        Vector finalCap;if(!CutCapCenter(c.limb,finalCap)){fail("throw_cap");return true;}
        Vector throwError=finalCap-c.launchCap-c.throwOffset;throwError.z=0;
        if(magnitude(throwError)>.002f){fail("throw_pivot_displacement");return true;}
        fprintf(stderr,"K1_DISMEMBER_OUTWARD_PASS trial=%d displacement_error=%g\n",trial,magnitude(throwError));
        fprintf(stderr,"K1_DISMEMBER_FROZEN_PASS trial=%d samples=%d error=%g\n",trial,c.frozenSamples,c.frozenError);
        if(!c.landed){if(ticks>240)fail("landing_timeout");return true;}
        float lowest=std::min(CutLowest(c.limb),std::min(CutLowest(c.head),CutLowest(c.weapon)));
        if(!std::isfinite(lowest)||std::fabs(lowest-c.floor-.015f)>.005f){fprintf(stderr,"K1_DISMEMBER_LANDING_TRACE lowest=%g floor=%g\n",lowest,c.floor);fail("landing_geometry");return true;}
        retained.push_back(clientId);
        for(auto id:retained){auto old=cutCorpses.find(id);auto* original=g_pAppManager->m_pClientExoApp->GetCreatureByGameObjectID(id);
            if(old==cutCorpses.end()||!old->second.limb||!original||!original->m_pArea||!original->m_pArea->GetIsObjectCorpse(id)){fail("corpse_persistence");return true;}}
        fprintf(stderr,"K1_DISMEMBER_LOCATION_PASS trial=%d cut=%s lowest=%g floor=%g retained=%zu\n",trial,CutAt(selected)->code.c_str(),lowest,c.floor,retained.size());
        ++trial;
        if(trial<16){
            auto* next=SpawnArenaOpponent(player,player->m_position+player->m_orientation*1.2f,false,false,false,trial<8?"k1darktest":"k1arenafoe");
            if(!next){fail("replacement_spawn");return true;}currentEnemy=next->m_objectId;phase=2;ticks=0;return true;
        }
        auto* next=SpawnArenaOpponent(player,player->m_position+player->m_orientation*1.2f,false,false,false,"k1darktest");
        if(!next){fail("unmarked_fixture_spawn");return true;}currentEnemy=next->m_objectId;phase=520;ticks=0;return true;
    }
    if(phase==520) {
        auto* client=enemy?enemy->GetClientCreature():nullptr;
        if(!client||ticks<30){if(ticks>240)fail("unmarked_fixture");return true;}
        clientId=client->m_objectId;enemy->m_bPlot=0;enemy->SetCurrentHitPoints(1);
        // A native non-saber/utility effect has no appended cut witness.
        auto* damage=new CGameEffect(1);damage->m_subType=CSWSEffectListHandler::EFFECT_DAMAGE;
        damage->m_durationTypeBits=CGameEffect::DURATION_TYPE_INSTANTANEOUS;damage->SetCreator(player->m_objectId);
        damage->SetNumIntegers(22);for(int i=0;i<15;++i)damage->SetInteger(i,-1);
        damage->SetInteger(7,100);damage->SetInteger(14,100);damage->SetInteger(17,3);damage->SetInteger(19,1);damage->SetInteger(20,1);
        enemy->ApplyEffect(damage,0,1);phase=521;ticks=0;return true;
    }
    if(phase==521) {
        auto* client=enemy?enemy->GetClientCreature():nullptr;auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
        if(!enemy||!enemy->GetDead()||!body||IsSourceBody(body)){if(ticks>240)fail("unmarked_native_death");return true;}
        if(cutCorpses.count(clientId)||_stricmp(body->m_pModel->m_szName,"n_darkjedim")){fail("unmarked_cut");return true;}
        fprintf(stderr,"K1_DISMEMBER_UNMARKED_PASS native_dead=1 cut=0\n");
        auto* gui=g_pAppManager->m_pClientExoApp->GetInGameGui();CSWGuiSaveLoadEntry entry;entry.LoadData(CExoString("000003 - Game2"));
        if(!gui||!entry.m_bSaveExists||entry.m_bCorrupt){fail("reload_fixture");return true;}
        auto* panel=new CSWGuiSaveLoad(gui->m_pGuiManager,0,0);gui->m_pGuiManager->AddPanel(panel,2,1);
        auto* app=g_pAppManager->m_pClientExoApp;const auto accepted=gui->m_pSoloModeQueryBox->m_bAccepted;gui->m_pSoloModeQueryBox->m_bAccepted=1;
        phase=550;ticks=0;panel->LoadGame(&entry);if(app->GetInGameGui()==gui)gui->m_pSoloModeQueryBox->m_bAccepted=accepted;return true;
    }
    if(phase==550) {
        auto* app=g_pAppManager->m_pClientExoApp;
        if(!strcmp(app->GetModule()->m_sModuleName.CStr(),"k1arena")){if(ticks>600)fail("area_reload_timeout");return true;}
        if(!cutCorpses.empty()||cutPartsCreated!=cutPartsDeleted){fail("owned_parts_leaked");return true;}
        fprintf(stderr,"K1_DISMEMBER_ARENA_PASS native_cut_deaths=17 retained=16 revived=1 unmarked_deaths=1 cleanup=1 parts=%llu\n",cutPartsDeleted);phase=-1;quit();
    }
    return true;
}
}
