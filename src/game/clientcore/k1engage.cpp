// Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
// Updated for KotOR:CE v0.2 on 2026-11-24; see docs/LOCAL_CHANGES.md.
#include "port/k1engage.h"
#include <set>
#if KOTOR1 && defined(K2_UI_PC)
#include "k1engagestate.h"
#include "resgff.h"
#include "nwstrigger.h"
#include "k1npctype.h"
#include "k1groundmove.h"
#include "port/k1jkanav.h"
#include "port/k1jkaai.h"
#include "port/aslglapi.h"
#include "k1sabercontact.h"
#include "k1attackdeflection.h"
#include "k1jkasabermarks.h"
#include "AURORA/aurimage.h"
#include "appmanager.h"
#include "clientexoapp.h"
#include "serverexoapp.h"
#include "guiingame.h"
#include "swgui.h"
#include "swguimaininterfacepc2.h"
#include "swguimainmenu.h"
#include "swguisaveload.h"
#include "swguiingame.h"
#include "swguiingameequip.h"
#include "nwccreature.h"
#include "nwcmodule.h"
#include "nwcarea.h"
#include "swplayercontrol.h"
#include "partytables.h"
#include "nwsparty.h"
#include "factionmanager.h"
#include "nwscreature.h"
#include "nwsarea.h"
#include "swsroom.h"
#include "swroomsurfacemesh.h"
#include "nwseffect.h"
#include "nwseffectlist.h"
#include "pathfindinfo.h"
#include "nwscombatround.h"
#include "nwscreaturestats.h"
#include "nwsitem.h"
#include "nwcitem.h"
#include "nwcvisualeffect.h"
#include "nwsaiaction.h"
#include "nwscriptconst.h"
#include "strref.h"
#include "clientoptions.h"
#include "serverexoappinternal.h"
#include "serveraimaster.h"
#include "ClientExoAppInternal.h"
#include "nwcanimbase.h"
#include "nwccreatureappearance.h"
#include "exoresman.h"
#include "restype.h"
#include "CSWVisibilityNode.h"
#include "AURORA/aurmodel.h"
#include "AURORA/auremitter.h"
#include "AURORA/aurscene.h"
#include "AURORA/aurscenemanager.h"
#include "aurcamera.h"
#include "AURORA/auranim.h"
#include "AURORA/aurbehavior.h"
#include "AURORA/aurglcore.h"
#include "AURORA/aurgui.h"
#include "AURORA/aurtexman.h"
#include "AURORA/aurStringFont.h"
#include "math3d.h"
#include "include/k1yellowswordhooks.h"
#include "include/k1swordalignment.h"
#include "exoinput.h"
#include "exosound.h"
#include "soundprioritygroup.h"
#include "swguidrawable.h"
#include "CExoArrayListImpl.h"
#include "CExoLinkedList.h"
#include "worldtimer.h"
#include "nwccreaturestats.h"
#include "nwcreaturelevelstats.h"
#include "nwsrules.h"
#include "twodimarrays.h"
#include "twodimarray.h"
#include "nwitem.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <chrono>
#include <random>

struct SDL_RWops;
CAurObject* NewCAurObject(char*,char*,SDL_RWops*,SDL_RWops*);

int GetOffset64(int nOffset,int nType);
void AurGUIGetScreenSize(int& width,int& height);
extern int g_bMouseCameraRotateToggle;
extern int g_bMouseCameraRotateToggleIos;
extern int g_bLeftMouseDown;
bool IsGamePaused();
extern float CameraPersonalSpace;
extern CExoSound* g_pExoSound;

namespace {
#include "include/k1swordgeometry.inc"
bool SingleSwordType(int type) {
    return type==BASE_ITEM_LONG_SWORD || type==BASE_ITEM_SHORT_SWORD || type==BASE_ITEM_VIBRO_SWORD || type==BASE_ITEM_VIBRO_BLADE;
}
bool DoubleSwordType(int type) {
    return type==BASE_ITEM_DOUBLE_BLADED_SWORD || type==BASE_ITEM_VIBRO_DOUBLE_BLADE || type==BASE_ITEM_WOOKIE_WARBLADE;
}
bool SwordType(int type){return SingleSwordType(type) || DoubleSwordType(type);}
bool SaberType(int type){return type==BASE_ITEM_LIGHTSABER || type==BASE_ITEM_SHORT_LIGHTSABER || type==BASE_ITEM_DOUBLE_BLADED_LIGHTSABER;}
bool SingleMeleeType(int type){return SingleSwordType(type) || type==BASE_ITEM_LIGHTSABER || type==BASE_ITEM_SHORT_LIGHTSABER || type==BASE_ITEM_STUN_BATON || type==BASE_ITEM_GAMMOREAN_BATTLEAXE;}
const SwordGeometry* SwordModelGeometry(Gob* model) {
    if(model && model->m_pModel)for(const auto& geometry:swordGeometry)
        if(!_stricmp(model->m_pModel->m_szName,geometry.resource))return &geometry;
    return nullptr;
}
const char* PoiseWeaponResource(Gob* model) {
    if(!model || !model->m_pModel)return nullptr;
    return (model->m_pModel->m_nFlags & Model::kModelLightsaber) || SwordModelGeometry(model)?model->m_pModel->m_szName:"w_Lghtsbr_001";
}
bool ActorSwordPair(CSWSCreature* actor) {
    if(!actor || !actor->m_pInventory)return false;
    auto* right=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
    auto* left=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON);
    return right && left && right!=left && SingleMeleeType(right->m_baseItem) && SingleMeleeType(left->m_baseItem) &&
        (SwordType(right->m_baseItem) || SwordType(left->m_baseItem));
}
bool ActorDoubleSword(CSWSCreature* actor) {
    auto* item=actor && actor->m_pInventory?actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON):nullptr;
    return item && DoubleSwordType(item->m_baseItem);
}
void DrawSaberSurfaceMarks(CAurScene* scene);
void SeedNativeCameraOrbit(CAurCamera* camera,CSWCCreature* actor,const Vector& eye);
constexpr float parryMarkerExtraRange=.75f;
constexpr float parryAimRadiusHeight=.45f;
bool ParryAimWithinArea(float right,float up,int height) {
    return std::hypot(right,up)<=height*parryAimRadiusHeight;
}
K1EngageState state;
unsigned long long actorId = OBJECT_INVALID;
Gob* loadedBody = nullptr; // identity only; actor lookup owns the model lifetime
bool rigBodyRequested=false;
CSWCCreature* rigBodyOwner=nullptr; // identity only; lookups validate lifetime
unsigned long long rigBodyActor=OBJECT_INVALID;
bool RigModeEnabled() {const char* p=std::getenv("K1_ENGAGE_ANATOMICAL_RIG");return p&&!strcmp(p,"1");}
bool IsSourceBody(Gob* body) {return body && body->GetPart(const_cast<char*>("torsoMid_g")) && body->GetPart(const_cast<char*>("rradiusX"));}
CSWGuiMainInterface* hudOwner = nullptr;
unsigned int poiseMeterEpoch=0; // isolated capture-only fixture, never normal gameplay
const char* currentAttack = nullptr;
const char* currentBase = nullptr;
const char* currentOverlay = nullptr;
int swingDirection = 0;
Gob* poseProbeBody=nullptr;
const char* poseProbeReference=nullptr;
float poseProbeTime=0;
int bridgedPoses=0;
int anatomicalPoses=0;bool anatomicalChain=true;float anatomicalEdgeError=0;
bool hitConsumed = false;
unsigned int staffContactBlades=0;
int staffContactSerial=-1;
unsigned long long staffContactWeapon=OBJECT_INVALID;
int earlyGuardSerial=-1;float earlyGuardTime=0,earlyGuardLength=0;
bool hadBlade = false;
bool retainedPlanningPose = false;
bool nativeActionBody = false;
bool nativeEdgeAwaitInterior=false;
bool playerNativeEffectSampled=false; // opt-in installed effect probe only
float previousBladeTime=0;
unsigned long long previousBladeWeapon=OBJECT_INVALID;
unsigned long long poseRevision=0,contactRevision=0;
int resumeFrames = 0;
bool keyW=false,keyS=false,keyA=false,keyD=false,keyShift=false;
int requestedDirection=0;
bool requestedBackLeft=false;
bool swingBackLeft=false;
int staffArcTrial=-1;
int attackSerial=0;
long long playerAttackStartedAt=-1;
int victorySwingSerial=-1;
float aimYaw=0,aimPitch=0;
float nativeCameraHeadHeight=0,nativeCameraFollowHeight=0;
// User-reviewed framing adjustment for Engage's closer orbit, in metres.
constexpr float engageCameraHeightTrim=-.25f;
constexpr float engageOrbitRadius=3.0f;
// Leave clearance below the collision volume, not just below the camera eye.
constexpr float engageCameraFloorMargin=.20f;
int viewPitchPoses=0;float maximumViewPitch=0;
float legsYaw=0;
struct BodyPose {Part* part;Vector position;Quaternion orientation;};
struct GripPose {
    Vector hilt,left,center,leftCenter;
    Quaternion hiltQ,leftQ,rightGuide,leftGuide;
    bool joined=false;
};
struct PointCorrection {Quaternion rotation;float radius=0;Vector offset;bool linear=false;};
struct NativeGaitPlayback {
    std::shared_ptr<CSWCAnimBase> owner;
    Gob* body=nullptr;
    std::string resource,clip;
    std::vector<BodyPose> pose;
    float time=0,length=0;
};
// Transition history belongs to a body, not to the selected player. The
// player's aliases preserve the existing input/ownership path; NPC drivers
// pass their own playback state to the same pose solver.
struct PosePlayback {
    NativeGaitPlayback nativeGait;
    GripPose lastGrip,gripFrom,gripCorrection;
    PointCorrection hiltCorrection,leftCorrection;
    std::vector<BodyPose> lastPose,blendFrom;
    std::vector<Quaternion> rotationFrom;
    Vector sourceGapCorrection;
    float sourceContactError=0;
    bool blendPending=false;
    float blendTime=.15f;
    bool guardKnown=false,guardHeld=false,guardTransition=false;
    bool stanceTransition=false;
    unsigned int blockSerial=0;
    int swingSoundSerial=-1;
} playerPlayback;
struct UtilityPoseJoint { const char* name; Quaternion bindDelta; };
struct UtilityBlendJoint { Part* part; Quaternion start, raw, shown; };
struct UtilityPoseBlend {
    Gob* body = nullptr;
    std::vector<UtilityBlendJoint> joints;
    float elapsed = 0.f;
    bool toEngage = false;
};
struct NpcPlayback {
    unsigned long long id=OBJECT_INVALID;
    CSWCCreature* owner=nullptr;
    Gob* body=nullptr;
    std::string nativeResource="n_sithsoldier";
    PosePlayback pose;
    UtilityPoseBlend utilityBlend;
    std::vector<BodyPose> nativeIdle,staffGuard;
    unsigned int authoredHeadReported=0;
    bool staff=false,dual=false,red=false,blue=false;
    int blueFrom=-1;
    int redFrom=-1;
    unsigned long long target=OBJECT_INVALID;
    unsigned int contactBlades=0;
    std::vector<K1SaberContact::Blade> previousBlades;
    float legsYaw=0,facingYaw=0;
    float previousFacingYaw=0,facingRemainder=.05f;
    bool walking=false,backLeft=false;
    Vector lastMotion;
    long long updated=0,movingUntil=0;
    unsigned short moveGroup=0;
    bool hasMove=false,intentWalk=false,visible=false;
    bool meleeAttackWalk=false,attackWalkOK=true;
    int attackWalkSamples=0,attackWalkMoving=0,attackWalkCooldown=0,attackWalkExits=0,attackWalkChase=0;
    float attackWalkMaxSpeed=0;
    std::shared_ptr<K1JkaNav::Navigator> navigator;
    K1JkaNav::State navigation;
    Vector groundVelocity,previousPhysics,currentPhysics;
    float physicsRemainder=0;
    long long nextSteer=0;
    bool navigationFailed=false;
    int navigationTicks=0;
    Vector goal,lastSeen,targetSample;
    long long nextThink=0,nextPath=0,lastSeenAt=0,targetSampleAt=0;
    long long evadeUntil=0;
    int strafeSide=1,pathRequests=0;
    K1JkaAI::JediState jedi;
    K1JkaAI::Commands commands;
    bool sourceJediCommands=false;
    bool sourceBackpedal=false;
    int sourceLateralSamples=0;
    int idleSamples=0,continuousLayers=0,gripSamples=0;
    int closeGuardMoving=0,closeGuardStill=0;
    bool parityOK=true;
    float idleError=0,maximumLegYaw=0,maximumGripError=0;
    const char* decision="hold";
    bool requested=false,attacking=false,hit=false,bladeValid=false;
    bool ranged=false,rifle=false;
    int weaponClass=-1;
    int chainDecisionSerial=-1;bool continueChain=true;
    bool parrying=false,closeGuard=false;
    long long threatSince=0,parryStart=0,parryUntil=0,parryReady=0;
    const char* parry="j_parry_up";
    const char* parryOverlay="y_parry_up";
    int guardBlocks=0;
    int health=0;
    const char* reaction=nullptr;
    long long reactionStarted=0;
    int direction=0,serial=0,chainSwings=0,completedChains=0,chainLinks=0;
    int bladeSamples=0,sweeps=0,sceneVisits=0;
    float closestTip=999.f;
    float maximumReach=-999.f,targetNear=999.f;
    const char* base=nullptr;
    const char* overlay=nullptr;
    long long nextAttack=0,attackStartedAt=-1;
    Vector position,bladeBase,bladeTip;
    float bladeTime=0;
    unsigned long long revision=0,contactRevision=0;
};
bool NpcWalking(const NpcPlayback& npc) {
    return npc.requested && (npc.parrying || npc.closeGuard || npc.intentWalk || (!npc.ranged && npc.meleeAttackWalk));
}
bool NpcGuarding(const NpcPlayback& npc) {
    return npc.requested && !npc.attacking && (npc.parrying || npc.closeGuard);
}
void ReconcileSandboxMovement(CSWSCreature* actor) {
    if(!actor)return;
    bool speedEffect=false;
    for(int i=0;i<actor->m_effects.m_count;++i) {
        const auto type=actor->m_effects.m_pArray[i]->m_subType;
        speedEffect|=type==CSWSEffectListHandler::EFFECT_MOVEMENT_SPEED_INCREASE || type==CSWSEffectListHandler::EFFECT_MOVEMENT_SPEED_DECREASE;
    }
    // Only speed-effect handlers write this factor after initialization. A low
    // cached factor with no remaining provider is stale, not a live debuff.
    if(!speedEffect && actor->GetMovementRateFactor(0)<.9999f) {
        fprintf(stderr,"K1_SANDBOX_MOVEMENT_RECOVER stale_factor=%g live_speed_effects=0\n",actor->GetMovementRateFactor(0));
        actor->SetMovementRateFactor(1.f);
    }
    static unsigned long long previousActor=OBJECT_INVALID;static float previousFactor=-1;
    const float factor=actor->GetMovementRateFactor(0);
    if(previousActor!=actor->m_objectId || std::fabs(previousFactor-factor)>.0001f) {
        fprintf(stderr,"K1_SANDBOX_MOVEMENT_RATE actor=%llu factor=%g slowed=%d limited=%d walk=%g run=%g\n",actor->m_objectId,factor,actor->m_bSlowed,actor->m_bMovementSpeedLimited,actor->m_pStats->m_fWalkRate,actor->m_pStats->m_fRunRate);
        if(factor<1)for(int i=0;i<actor->m_effects.m_count;++i) {
            auto* effect=actor->m_effects.m_pArray[i];
            if(effect->m_subType==CSWSEffectListHandler::EFFECT_MOVEMENT_SPEED_INCREASE || effect->m_subType==CSWSEffectListHandler::EFFECT_MOVEMENT_SPEED_DECREASE)
                fprintf(stderr,"K1_SANDBOX_MOVEMENT_EFFECT id=%llu type=%u value=%d duration=%g duration_type=%u\n",effect->m_effectId,unsigned(effect->m_subType),effect->GetInteger(0),effect->m_fDuration,unsigned(effect->m_durationTypeBits));
        }
        previousActor=actor->m_objectId;previousFactor=factor;
    }
}
CSWSCreature* ServerActor();
float NativeMoveSpeed(CSWSCreature* actor,bool walk) {
    // Native rates are millimetres/second; client/physics positions are metres.
    if(std::getenv("K1_ENGAGE_SANDBOX") && actor && actor==ServerActor())ReconcileSandboxMovement(actor);
    return actor?(walk?actor->GetWalkRate():actor->GetRunRate())*.001f:0.f;
}
CSWSCreature* ServerActor();
bool NativeSandboxActor(CSWSCreature* actor);
float EngageMoveSpeed(CSWSCreature* actor,bool walk) {
    auto* player=ServerActor();
    if(!actor || !player || !player->m_pStats) return 0;
    if(actor==player || (actor && (NativeSandboxActor(actor) || std::getenv("K1_ENGAGE_STORY")))) return NativeMoveSpeed(actor,walk);
    // Match the native player's base rates on both sides, applying each NPC's
    // own KotOR movement effects rather than copying the player's buffs.
    const float base=walk?player->m_pStats->m_fWalkRate:player->m_pStats->m_fRunRate;
    const float rate=base*actor->GetMovementRateFactor(1);
    return walk?rate:std::max(1.f,rate);
}
// Keep close pursuit walking when the player backs out of immediate swing reach.
constexpr float meleeWalkExtraDistance=.75f;
std::unordered_map<unsigned long long,NpcPlayback> npcPlayback;
bool ArenaDarkJedi() {return std::getenv("K1_ENGAGE_DARK_JEDI")!=nullptr;}
std::unordered_set<unsigned long long> sandboxNativeActors;
bool NativeSandboxActor(CSWSCreature* actor){return actor && sandboxNativeActors.count(actor->m_objectId)!=0;}
#include "k1npccombatpolicy.h"
bool NativeConfiguredNpc(CSWSCreature* actor){
    if(!actor || actor==ServerActor())return false;
    if(NativeSandboxActor(actor))return true;
    // Keep the separate authored arena fixtures on their established path.
    for(const auto& type:k1DarkJediTypes)if(!_stricmp(actor->m_sTag.CStr(),type.tag))return false;
    return std::getenv("K1_ENGAGE_STORY") || NpcCombatPolicy(actor).matched;
}
bool NativeNpcBluntType(int type) {
    return type==BASE_ITEM_STUN_BATON || type==BASE_ITEM_QUARTER_STAFF || type==BASE_ITEM_GHAFFI_STICK || type==BASE_ITEM_GAMMOREAN_BATTLEAXE;
}
CSWSItem* NativeNpcRightItem(CSWSCreature* actor) {
    return actor && actor->m_pInventory?actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON):nullptr;
}
bool NativeNpcMeleeEquipped(CSWSCreature* actor) {
    auto* item=NativeNpcRightItem(actor);
    return item && (SingleMeleeType(item->m_baseItem) || DoubleSwordType(item->m_baseItem) ||
        item->m_baseItem==BASE_ITEM_DOUBLE_BLADED_LIGHTSABER || NativeNpcBluntType(item->m_baseItem));
}
bool NativeNpcBlunt(CSWCCreature* actor) {
    if(!actor || (actor->GetServerCreature()!=ServerActor() && !NativeConfiguredNpc(actor->GetServerCreature())))return false;
    auto* item=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[4]);
    return item && NativeNpcBluntType(item->m_baseItem);
}
bool NativeNpcSolidBlade(CSWCCreature* actor,K1SaberContact::Blade& blade,int hand=0);
int NpcWeaponClass(CSWSCreature* actor) {
    auto* client=actor?actor->GetClientCreature():nullptr;
    return client && client->GetAnimationBase()?client->GetAnimationBase()->m_nWeaponClass:0;
}
struct NativeNpcRig {std::string native,single,staff,dual,red,blue,purple;};
std::unordered_map<unsigned long long,int> sandboxStances;
std::unordered_map<unsigned long long,CCAppearanceInfo> sandboxAppearances;
std::vector<NativeNpcRig> nativeNpcRigs;
std::vector<NativeNpcRig> wearableRigs;
bool NativeNpcAssets(const char* resource) {
    return resource && g_pExoResMan->Exists(CResRef(resource),RESTYPE_MDL,static_cast<unsigned int*>(nullptr)) &&
        g_pExoResMan->Exists(CResRef(resource),RESTYPE_MDX,static_cast<unsigned int*>(nullptr));
}
void LoadBodyRigTable(const char* resource,std::vector<NativeNpcRig>& rows) {
    rows.clear();
    if(!g_pExoResMan->Exists(CResRef(resource),RESTYPE_2DA,static_cast<unsigned int*>(nullptr)))return;
    C2DA table(CResRef(resource),0);if(!table.Load2DArray())return;
    for(int row=0;row<table.m_nRows && row<256;++row) {
        CExoString native,single,staff,dual;
        if(table.GetCExoStringEntry(row,CExoString("native"),&native) &&
           table.GetCExoStringEntry(row,CExoString("single"),&single) &&
           table.GetCExoStringEntry(row,CExoString("staff"),&staff) &&
           table.GetCExoStringEntry(row,CExoString("dual"),&dual))
            rows.push_back({native.CStr(),single.CStr(),staff.CStr(),dual.CStr()});
        if(!rows.empty() && rows.back().native==native.CStr()) {
            CExoString red,blue,purple;
            if(table.GetCExoStringEntry(row,CExoString("red"),&red))rows.back().red=red.CStr();
            if(table.GetCExoStringEntry(row,CExoString("blue"),&blue))rows.back().blue=blue.CStr();
            if(table.GetCExoStringEntry(row,CExoString("purple"),&purple))rows.back().purple=purple.CStr();
        }
    }
}
void LoadNativeNpcRigs(){LoadBodyRigTable("k1npcrigs",nativeNpcRigs);}
const NativeNpcRig* WearableRig(const char* native) {
    if(!native)return nullptr;
    if(wearableRigs.empty() && g_pExoResMan)LoadBodyRigTable("k1armorrigs",wearableRigs);
    for(const auto& r:wearableRigs)if(!_stricmp(native,r.native.c_str()))return &r;
    return nullptr;
}
const char* WearableNativeResource(const char* resource) {
    if(!resource)return nullptr;
    if(wearableRigs.empty() && g_pExoResMan)LoadBodyRigTable("k1armorrigs",wearableRigs);
    for(const auto& r:wearableRigs)for(const auto* name:{&r.native,&r.single,&r.red,&r.blue,&r.purple,&r.dual,&r.staff})
        if(!_stricmp(resource,name->c_str()))return r.native.c_str();
    return nullptr;
}
const char* PlayerNativeResource(Gob* body) {
    const char* resource=body&&body->m_pModel?WearableNativeResource(body->m_pModel->m_szName):nullptr;
    return resource?resource:"pmbbl";
}
const K1NpcType& NpcType(CSWSCreature* actor) {
    for(const auto& type:k1DarkJediTypes) if(actor && !_stricmp(actor->m_sTag.CStr(),type.tag)) return type;
    return k1DarkJediTypes[0];
}
bool ActorSingleSword(CSWSCreature* actor) {
    auto* item=actor && actor->m_pInventory?actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON):nullptr;
    return item && SingleSwordType(item->m_baseItem);
}
bool StaffNpc(CSWSCreature* actor) {
    if(NativeConfiguredNpc(actor)) {
        auto* item=NativeNpcRightItem(actor);
        return item && (DoubleSwordType(item->m_baseItem) || item->m_baseItem==BASE_ITEM_DOUBLE_BLADED_LIGHTSABER ||
            item->m_baseItem==BASE_ITEM_QUARTER_STAFF || item->m_baseItem==BASE_ITEM_GHAFFI_STICK);
    }
    return actor && (ActorDoubleSword(actor) || (!ActorSingleSword(actor) && !ActorSwordPair(actor) && NpcType(actor).staff));
}
bool DualNpc(CSWSCreature* actor) {
    if(NativeConfiguredNpc(actor)) {
        auto* right=NativeNpcRightItem(actor);
        auto* left=actor->m_pInventory?actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON):nullptr;
        return right && left && right!=left &&
            (SingleMeleeType(right->m_baseItem) || right->m_baseItem==BASE_ITEM_STUN_BATON) &&
            (SingleMeleeType(left->m_baseItem) || left->m_baseItem==BASE_ITEM_STUN_BATON);
    }
    return actor && (ActorSwordPair(actor) || (!ActorSingleSword(actor) && !ActorDoubleSword(actor) && NpcType(actor).dual));
}
bool RedNpc(CSWSCreature* actor) {return actor && !StaffNpc(actor) && !DualNpc(actor) && ((NativeSandboxActor(actor) && sandboxStances.count(actor->m_objectId)?sandboxStances[actor->m_objectId]:NativeConfiguredNpc(actor)?NpcCombatPolicy(actor).stance:NpcType(actor).red?1:NpcType(actor).blue?2:0)==1);}
bool BlueNpc(CSWSCreature* actor) {return actor && !StaffNpc(actor) && !DualNpc(actor) && ((NativeSandboxActor(actor) && sandboxStances.count(actor->m_objectId)?sandboxStances[actor->m_objectId]:NativeConfiguredNpc(actor)?NpcCombatPolicy(actor).stance:NpcType(actor).red?1:NpcType(actor).blue?2:0)==2);}
const char* NpcRigResource(const char* native,CSWSCreature* actor=nullptr) {
    if(NativeConfiguredNpc(actor)) {
        if(nativeNpcRigs.empty())LoadNativeNpcRigs();
        if(!NativeNpcMeleeEquipped(actor))return nullptr;
        for(const auto& rig:nativeNpcRigs)if(!_stricmp(native,rig.native.c_str())) {
            const char* selected=(StaffNpc(actor)?rig.staff:DualNpc(actor)?rig.dual:RedNpc(actor)?rig.red:BlueNpc(actor)?rig.blue:rig.single).c_str();
            return NativeNpcAssets(selected)?selected:nullptr;
        }
        return nullptr;
    }
    if(!_stricmp(native,"n_darkjedim")) {
        if(StaffNpc(actor))return "k1jadstaff";
        if(DualNpc(actor))return "k1jaddual";
        if(RedNpc(actor) || BlueNpc(actor))return NpcType(actor).rig;
    }
    if(!_stricmp(native,"n_sithsoldier")) return StaffNpc(actor) || DualNpc(actor)?nullptr:"k1jasith";
    if(!_stricmp(native,"n_darkjedim")) return "k1jadark";
    return nullptr;
}
struct SandboxActor {bool friendly=false;int commandable=1,passive=0;unsigned int meleeHits=0,dualDamageHands=0;};
std::unordered_map<unsigned long long,SandboxActor> sandboxActors;
bool sandboxAi=true,sandboxTargetPlayer=true,sandboxSession=false,sandboxReady=false,sandboxMenuOpen=false,sandboxTogglePending=false;
unsigned long long sandboxArea=OBJECT_INVALID,lastSandboxSpawn=OBJECT_INVALID;
bool SandboxIdle(CSWSCreature* actor){return actor && sandboxActors.count(actor->m_objectId) && !sandboxAi;}
bool SandboxActorActive(CSWSCreature* actor){return actor && !actor->GetDead() && !SandboxIdle(actor);}
CSWSCreature* SandboxTarget(CSWSCreature* actor,CSWSCreature* player) {
    if(!actor || !player || !SandboxActorActive(actor))return nullptr;
    if(!sandboxActors.count(actor->m_objectId))return actor->GetReputation(player->m_objectId,0)<=10?player:nullptr;
    auto* area=static_cast<CSWSArea*>(actor->GetArea());if(!area || area!=player->GetArea())return nullptr;
    CSWSCreature* best=nullptr;float distance=40.f;
    for(int i=0;i<area->m_aObjectsByX.m_count;++i) {
        auto* other=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);
        if(!other || other==actor || other->GetDead() || actor->GetReputation(other->m_objectId,0)>10)continue;
        const float d=magnitude(actor->m_position-other->m_position);
        if(d<distance){best=other;distance=d;}
    }
    return best;
}
void SandboxTick();
void StartNativeSandboxCombat(CSWSCreature* actor) {
    auto* player=ServerActor();
    if(!NativeSandboxActor(actor) || !player || !sandboxAi || NativeNpcMeleeEquipped(actor) ||
       actor->GetDead() || !actor->m_bCommandable || actor->m_bPassiveAttackBehaviour ||
       actor->m_actionList.m_pInternal->m_nCount || !K1EngageEnemyAIActive(player->m_objectId))return;
    if(auto* target=SandboxTarget(actor,player))
        actor->AddAttackActions(target->m_objectId,0,1,0,0,0,0,1,SERVER_ANIMATION_ATTACK,1500,1,0,0,4,0);
}
void PresentationProbeTick();
void SandboxToggle();
void SandboxSetAi(bool enabled);
void SandboxUnload(unsigned long long areaId);
struct SandboxNativeTemplate {std::string label,module,source,creature,faction,variantKey,indicator,model,look;int typeOrder=0;};
struct SandboxNativeGroup {std::string label;std::vector<SandboxNativeTemplate> variants;};
std::vector<SandboxNativeGroup> sandboxNativeGroups;
void LoadSandboxNativeCatalog() {
    LoadNativeNpcRigs();
    sandboxNativeGroups.clear();
    if(!g_pExoResMan->Exists(CResRef("k1npcs"),RESTYPE_2DA,static_cast<unsigned int*>(nullptr)))return;
    C2DA table(CResRef("k1npcs"),0);if(!table.Load2DArray())return;
    for(int row=0;row<table.m_nRows && row<4096;++row) {
        CExoString label,module,source,creature;
        if(!table.GetCExoStringEntry(row,CExoString("label"),&label) ||
           !table.GetCExoStringEntry(row,CExoString("module"),&module) ||
           !table.GetCExoStringEntry(row,CExoString("source"),&source) ||
           !table.GetCExoStringEntry(row,CExoString("template"),&creature) ||
           !g_pExoResMan->Exists(CResRef(creature),RESTYPE_UTC,static_cast<unsigned int*>(nullptr)))continue;
        if(sandboxNativeGroups.empty() || sandboxNativeGroups.back().label!=label.CStr())
            sandboxNativeGroups.push_back({label.CStr(),{}});
        sandboxNativeGroups.back().variants.push_back({label.CStr(),module.CStr(),source.CStr(),creature.CStr()});
        auto& entry=sandboxNativeGroups.back().variants.back();
        auto read=[&](const char* key,std::string& result){CExoString value;if(table.GetCExoStringEntry(row,CExoString(key),&value))result=value.CStr();};
        read("faction",entry.faction);read("variant",entry.variantKey);read("indicator",entry.indicator);read("model",entry.model);read("look",entry.look);
        table.GetINTEntry(row,CExoString("typeorder"),&entry.typeOrder);
    }
    fprintf(stderr,"K1_SANDBOX_NATIVE_CATALOG groups=%zu\n",sandboxNativeGroups.size());
}
CSWSCreature* SandboxSpawn(int stance,bool friendly,CExoString& message,int group=-1,int variant=0,bool hostile=false,const SandboxNativeTemplate* appearance=nullptr,int color=0,int saberStance=-1);
struct ArenaForceState {
    unsigned long long actor=OBJECT_INVALID;
    long long next=0;
    int nextSpell=FORCE_POWER_CHOKE,lastSpell=-1,queued=0,queuedAggression=0;
} arenaForce;
std::unordered_map<unsigned long long,ArenaForceState> sandboxForces;
struct MeleePoise {
    // Mutual same-opponent episode; cleared by any non-clash resolution.
    unsigned long long clashOpponent=OBJECT_INVALID,clashRight=OBJECT_INVALID,clashLeft=OBJECT_INVALID;
    int clashes=0,clashDecision=0;
    bool clashParry=false,clashSwing=false;
    long long clashGuardUntil=0;
    int percent=100,capacity=100;
    long long recoverAt=0,quietUntil=0,nextTick=0,parriedUntil=0;
    long long blockStarted=0;
    int blockReaction=-1;
    unsigned int blockSerial=0;
    bool wasSwinging=false,wasEligible=false;
    int parryRollSerial=-1,parryRoll=0,parryAttempts=0;
    unsigned long long parryRollAttacker=OBJECT_INVALID;
    bool parryRollSuccess=false;
    bool staffTiming=false,dualTiming=false;
    int singleTiming=0; // Yellow=0, Red=1, Blue=2, Purple=3.
};
struct BlockObservation {
    long long started=-1;
    int reaction=-1,samples=0,settled=0;
    float maximumGrip=0;
    bool finite=true,moving=false;
};
std::unordered_map<unsigned long long,BlockObservation> blockObservations;
constexpr long long poiseQuietUs=2000000,poiseTickUs=100000;
constexpr long long poiseStunUs=2500000;
constexpr int poiseRecoveryPercent=25;
constexpr long long blockReactionUs=250000;
bool StaffPoiseActor(CSWSCreature* actor);
bool DualPoiseActor(CSWSCreature* actor);
int SinglePoiseStance(CSWSCreature* actor);
int GuardPoiseDamage(CSWSCreature* actor){
    if(StaffPoiseActor(actor))return 11;
    if(DualPoiseActor(actor))return 13;
    const int points[]={20,35,15,25};return points[SinglePoiseStance(actor)];
}
int ParriedPoiseLoss(CSWSCreature* actor){
    if(StaffPoiseActor(actor))return 35;
    if(DualPoiseActor(actor))return 20;
    const int points[]={25,20,15,25};return points[SinglePoiseStance(actor)];
}
int ParryPoiseReward(CSWSCreature* actor){
    if(StaffPoiseActor(actor))return 25;
    if(DualPoiseActor(actor))return 15;
    const int points[]={20,15,15,25};return points[SinglePoiseStance(actor)];
}
int ClashPoiseDamage(CSWSCreature*){return 10;}
long long PoiseQuietInterval(const MeleePoise& poise){return poise.dualTiming?3000000:(poise.staffTiming?2500000:(poise.singleTiming==2?1000000:poiseQuietUs));}
long long PoiseTickInterval(const MeleePoise& poise){return (poise.staffTiming || poise.dualTiming)?500000:(poise.singleTiming==1?200000:poiseTickUs);}
void SelectPoiseTiming(MeleePoise& poise,bool staff,long long now,bool dual=false,int single=0) {
    if(poise.staffTiming==staff && poise.dualTiming==dual && poise.singleTiming==single) return;
    const auto oldQuiet=PoiseQuietInterval(poise);poise.staffTiming=staff;poise.dualTiming=dual;poise.singleTiming=single;
    if(poise.quietUntil) poise.quietUntil=std::max(0LL,poise.quietUntil+PoiseQuietInterval(poise)-oldQuiet);
    poise.wasEligible=false;poise.nextTick=std::max(poise.quietUntil,now+PoiseTickInterval(poise));
}
void DelayPoiseRegeneration(MeleePoise& poise,long long now) {
    poise.quietUntil=now+PoiseQuietInterval(poise);poise.nextTick=poise.quietUntil;
}
void RecoverBrokenPoise(MeleePoise& poise,long long now) {
    poise.percent=poiseRecoveryPercent;poise.recoverAt=0;poise.wasSwinging=false;
    // The breaking block has no recovery delay, but later hits during stun do.
    poise.quietUntil=std::max(now,poise.quietUntil);
    poise.nextTick=std::max(poise.quietUntil,now+PoiseTickInterval(poise));
    poise.wasEligible=false;
}
int AdvancePoiseRegeneration(MeleePoise& poise,long long now,bool swinging,bool eligible) {
    const auto tick=PoiseTickInterval(poise);
    // Include the end of the swing, not only its initial click or damage window.
    if(swinging || poise.wasSwinging) DelayPoiseRegeneration(poise,now);
    poise.wasSwinging=swinging;
    if(poise.recoverAt || swinging || !eligible) {
        poise.wasEligible=false;
        poise.nextTick=std::max(poise.quietUntil,now+tick);return 0;
    }
    if(!poise.wasEligible && now>=poise.quietUntil) poise.nextTick=std::max(poise.nextTick,now+tick);
    poise.wasEligible=true;
    if(poise.percent>=poise.capacity) {poise.nextTick=std::max(poise.quietUntil,now+tick);return 0;}
    if(!poise.nextTick) poise.nextTick=std::max(poise.quietUntil,now+tick);
    if(now<poise.quietUntil || now<poise.nextTick) return 0;
    const long long ticks=1+(now-poise.nextTick)/tick;
    const int gain=int(std::min<long long>(poise.capacity-poise.percent,ticks));
    poise.percent+=gain;poise.nextTick+=ticks*tick;return gain;
}
bool PoiseRegenerationClockProbe() {
    bool ok=true;
    auto check=[&](bool value,const char* name){if(!value){fprintf(stderr,"K1_POISE_REGEN_FAIL clock=%s\n",name);ok=false;}};
    MeleePoise p;p.percent=50;DelayPoiseRegeneration(p,1000000);
    check(!AdvancePoiseRegeneration(p,2999999,false,true) && p.percent==50,"before_two_seconds");
    check(AdvancePoiseRegeneration(p,3000000,false,true)==1 && p.percent==51,"first_tick_at_two");
    check(!AdvancePoiseRegeneration(p,3099999,false,true) && p.percent==51,"no_fractional_tick");
    check(AdvancePoiseRegeneration(p,3100000,false,true)==1 && p.percent==52,"tenth_second_tick");
    DelayPoiseRegeneration(p,4600000);
    check(!AdvancePoiseRegeneration(p,6599999,false,true),"hit_resets_delay");
    check(AdvancePoiseRegeneration(p,6600000,false,true)==1,"hit_delays_full_two");
    p=MeleePoise();p.percent=40;DelayPoiseRegeneration(p,0);
    check(!AdvancePoiseRegeneration(p,4000000,false,false) && p.percent==40,"running_no_gain_or_backlog");
    check(!AdvancePoiseRegeneration(p,5000000,false,true),"stop_running_no_instant_tick");
    check(!AdvancePoiseRegeneration(p,5099999,false,true),"stop_running_tenth_second");
    check(AdvancePoiseRegeneration(p,5100000,false,true)==1,"walking_first_tick");
    p=MeleePoise();p.percent=50;
    check(!AdvancePoiseRegeneration(p,3000000,true,true),"swing_blocks");
    check(!AdvancePoiseRegeneration(p,4000000,true,true),"held_swing_blocks");
    check(!AdvancePoiseRegeneration(p,5000000,false,true),"swing_end_resets_delay");
    check(!AdvancePoiseRegeneration(p,6999999,false,true),"after_swing_delay");
    check(AdvancePoiseRegeneration(p,7000000,false,true)==1,"after_swing_first_tick");
    p.percent=99;check(AdvancePoiseRegeneration(p,7100000,false,true)==1 && p.percent==100,"cap_100");
    p.percent=75;p.recoverAt=10000000;
    check(!AdvancePoiseRegeneration(p,9500000,false,true) && p.percent==75,"broken_guard_no_regen");
    p=MeleePoise();p.percent=0;p.recoverAt=2500000;RecoverBrokenPoise(p,2500000);
    check(p.percent==25 && !AdvancePoiseRegeneration(p,2500000,false,true),"recover_at_twenty_five");
    check(!AdvancePoiseRegeneration(p,2599999,false,true) && AdvancePoiseRegeneration(p,2600000,false,true)==1 && p.percent==26,"recover_immediate_interval");
    p=MeleePoise();p.percent=0;p.recoverAt=2500000;DelayPoiseRegeneration(p,2200000);RecoverBrokenPoise(p,2500000);
    check(!AdvancePoiseRegeneration(p,4199999,false,true) && AdvancePoiseRegeneration(p,4200000,false,true)==1,"recover_respects_stun_hit");
    p=MeleePoise();p.percent=0;RecoverBrokenPoise(p,2500000);
    check(!AdvancePoiseRegeneration(p,3000000,false,false) && p.percent==25,"recover_running_blocks");
    check(!AdvancePoiseRegeneration(p,3100000,false,true) && AdvancePoiseRegeneration(p,3200000,false,true)==1,"recover_running_no_bank");
    p=MeleePoise();p.percent=0;RecoverBrokenPoise(p,2500000);
    check(!AdvancePoiseRegeneration(p,2600000,true,true) && !AdvancePoiseRegeneration(p,2700000,false,true) &&
        !AdvancePoiseRegeneration(p,4699999,false,true) && AdvancePoiseRegeneration(p,4700000,false,true)==1,"recover_swing_delays");
    p=MeleePoise();p.percent=50;SelectPoiseTiming(p,true,0);DelayPoiseRegeneration(p,1000000);
    check(!AdvancePoiseRegeneration(p,3499999,false,true) && p.percent==50,"staff_quiet_boundary");
    check(AdvancePoiseRegeneration(p,3500000,false,true)==1 && p.percent==51,"staff_first_tick_2_5");
    check(!AdvancePoiseRegeneration(p,3999999,false,true) && AdvancePoiseRegeneration(p,4000000,false,true)==1,"staff_half_second_tick");
    check(!AdvancePoiseRegeneration(p,5000000,false,false),"staff_running_blocks");
    check(!AdvancePoiseRegeneration(p,6000000,false,true) && !AdvancePoiseRegeneration(p,6499999,false,true) && AdvancePoiseRegeneration(p,6500000,false,true)==1,"staff_running_no_backlog");
    check(!AdvancePoiseRegeneration(p,7000000,true,true) && !AdvancePoiseRegeneration(p,7100000,false,true) &&
        !AdvancePoiseRegeneration(p,9599999,false,true) && AdvancePoiseRegeneration(p,9600000,false,true)==1,"staff_swing_end_delay");
    p.percent=0;p.recoverAt=0;p.quietUntil=0;RecoverBrokenPoise(p,10000000);
    check(p.percent==25 && !AdvancePoiseRegeneration(p,10000000,false,true) && !AdvancePoiseRegeneration(p,10499999,false,true) && AdvancePoiseRegeneration(p,10500000,false,true)==1,"staff_stun_first_half_second");
    p.percent=99;check(AdvancePoiseRegeneration(p,12500000,false,true)==1 && p.percent==100,"staff_cap_100");
    p=MeleePoise();p.percent=50;DelayPoiseRegeneration(p,1000000);SelectPoiseTiming(p,true,1500000);
    check(p.quietUntil==3500000 && PoiseTickInterval(p)==500000,"equip_staff_preserves_activity");
    SelectPoiseTiming(p,false,1600000);
    check(p.quietUntil==3000000 && PoiseTickInterval(p)==100000 && !AdvancePoiseRegeneration(p,2999999,false,true) && AdvancePoiseRegeneration(p,3000000,false,true)==1,"return_yellow_policy");
    if(ok) fprintf(stderr,"K1_STAFF_POISE_CLOCK_PASS quiet_us=2500000 tick_us=500000 switches=1 eligibility=1 stun=1\n");
    if(ok) fprintf(stderr,"K1_POISE_REGEN_CLOCK_PASS\n");
    return ok;
}
std::unordered_map<unsigned long long,MeleePoise> meleePoise;
void ConfigureNpcPoise(CSWSCreature* actor) {
    if(!actor || actor==ServerActor())return;
    auto& p=meleePoise[actor->m_objectId];
    const int cap=NpcCombatPolicy(actor).poise;
    if(p.capacity!=cap){p.capacity=cap;p.percent=std::min(p.percent,cap);}
}

void RewardParryPoise(MeleePoise& p,int points){p.percent=std::min(p.capacity,p.percent+points);}
void ClearClashEpisode(unsigned long long id) {
    auto it=meleePoise.find(id);if(it==meleePoise.end())return;
    const auto other=it->second.clashOpponent;
    auto clear=[](unsigned long long actor,MeleePoise& p){
        if(p.clashParry){auto npc=npcPlayback.find(actor);if(npc!=npcPlayback.end()){
            npc->second.parrying=false;
            // Release the extra wait on contact/interruption, retaining normal guard timing.
            npc->second.nextAttack=std::min(npc->second.nextAttack,npc->second.parryStart+350000);
        }}
        p.clashOpponent=OBJECT_INVALID;p.clashes=p.clashDecision=0;p.clashParry=p.clashSwing=false;p.clashGuardUntil=0;
    };
    clear(id,it->second);
    auto peer=meleePoise.find(other);
    if(peer!=meleePoise.end() && peer->second.clashOpponent==id)clear(other,peer->second);
}
int ClashParryBonus(unsigned long long a,unsigned long long b) {
    auto it=meleePoise.find(a);
    return it!=meleePoise.end() && it->second.clashOpponent==b?5*std::max(0,it->second.clashes-1):0;
}
int ClashParryChance(const MeleePoise& p){return std::min(100,p.clashes*5);}
bool ChooseClashParry(MeleePoise& p,int roll) {
    if(!p.clashes)return false;
    if(p.clashDecision!=p.clashes){p.clashDecision=p.clashes;p.clashParry=roll>=1 && roll<=ClashParryChance(p);}
    return p.clashParry;
}
void ValidateClashEpisodes(long long now);
const char* npcParryPercentVariable="K1_ENGAGE_PARRY_PERCENT";
int NpcDefaultParryPercent(CSWSCreature* actor) {
    for(const auto& type:k1DarkJediTypes)if(actor && !_stricmp(actor->m_sTag.CStr(),type.tag))return type.parryPercent;
    return NpcCombatPolicy(actor).parry;
}
int NpcParryPercent(CSWSCreature* actor) {
    // Native per-creature/template local: absent means the supported soldier's
    // default20; explicit0 disables parries,100 guarantees eligible parries.
    auto* variable=actor?actor->m_scriptVars.MatchIndex(CExoString(npcParryPercentVariable),CSWSScriptVarTable::VARTYPE_INTEGER,0):nullptr;
    if(variable)return std::max(0,std::min(100,variable->m_nInt));
    return NpcDefaultParryPercent(actor);
}
bool NpcParryRollPasses(int percent,int roll) {
    return roll>=1 && roll<=100 && roll<=percent;
}
bool NpcContactParry(CSWSCreature* defender,int swingSerial,unsigned long long attacker=OBJECT_INVALID) {
    auto& poise=meleePoise[defender->m_objectId];
    const int percent=NpcParryPercent(defender);
    if(poise.parryRollSerial!=swingSerial || poise.parryRollAttacker!=attacker) {
        poise.parryRollAttacker=attacker;
        poise.parryRollSerial=swingSerial;poise.parryRoll=g_pRules->RollDice(1,100);
        ++poise.parryAttempts;
        poise.parryRollSuccess=NpcParryRollPasses(percent,poise.parryRoll);
        fprintf(stderr,"K1_NPC_PARRY_ROLL actor=%llu swing=%d percent=%d roll=%d success=%d\n",
            defender->m_objectId,swingSerial,percent,poise.parryRoll,NpcParryRollPasses(percent,poise.parryRoll));
    }
    return poise.parryRollSuccess;
}
bool NpcParryPolicyProbe(CSWSCreature* actor) {
    const CExoString key(npcParryPercentVariable);
    auto* previous=actor->m_scriptVars.MatchIndex(key,CSWSScriptVarTable::VARTYPE_INTEGER,0);
    const bool hadVariable=previous!=nullptr;const int savedValue=previous?previous->m_nInt:0;
    const auto found=meleePoise.find(actor->m_objectId);const bool hadState=found!=meleePoise.end();
    const MeleePoise savedState=hadState?found->second:MeleePoise();
    actor->m_scriptVars.DestroyInt(key);bool ok=NpcParryPercent(actor)==NpcDefaultParryPercent(actor);
    const int settings[]={-10,0,20,25,100,130},expected[]={0,0,20,25,100,100};
    for(int i=0;i<6;++i) {
        actor->m_scriptVars.SetInt(key,settings[i],0);ok=ok && NpcParryPercent(actor)==expected[i];
        int accepted=0;
        for(int roll=1;roll<=100;++roll) accepted+=NpcParryRollPasses(NpcParryPercent(actor),roll);
        ok=ok && accepted==expected[i];
    }
    ok=ok && !NpcParryRollPasses(100,0) && !NpcParryRollPasses(100,101);
    actor->m_scriptVars.SetInt(key,20,0);meleePoise[actor->m_objectId]=MeleePoise();
    const bool first=NpcContactParry(actor,1000000);
    const bool second=NpcContactParry(actor,1000000);
    ok=ok && first==second && meleePoise[actor->m_objectId].parryAttempts==1;
    NpcContactParry(actor,1000001);ok=ok && meleePoise[actor->m_objectId].parryAttempts==2;
    if(hadState) meleePoise[actor->m_objectId]=savedState;else meleePoise.erase(actor->m_objectId);
    if(hadVariable) actor->m_scriptVars.SetInt(key,savedValue,0);else actor->m_scriptVars.DestroyInt(key);
    fprintf(stderr,"K1_NPC_PARRY_POLICY_%s default=%d enumeration=100 clamps=1 explicit_zero=1 once_per_swing=1\n",ok?"PASS":"FAIL",NpcDefaultParryPercent(actor));
    return ok;
}
int ActiveBlockReaction(unsigned long long actor,long long now) {
    const auto found=meleePoise.find(actor);
    if(state.mode!=K1EngageState::Mode::Active || found==meleePoise.end()) return -1;
    const auto& poise=found->second;
    return !poise.recoverAt && now>=poise.blockStarted && now-poise.blockStarted<blockReactionUs?poise.blockReaction:-1;
}
constexpr long long parryAttackDelayUs=1000000;
bool ParryAttackReady(unsigned long long actor,long long now) {
    const auto found=meleePoise.find(actor);
    return found==meleePoise.end() || now>=found->second.parriedUntil;
}
void PoiseRegenerationState(CSWSCreature* actor,long long now,bool& swinging,bool& eligible);
bool HasMeleePoise(CSWSCreature* actor) {
    auto* client=actor?actor->GetClientCreature():nullptr;
    auto* animation=client?client->GetAnimationBase():nullptr;
    if(!animation || !actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON)) return false;
    const int weapon=animation->m_nWeaponClass;
    return weapon>=CSWCAnimBase::WEAPON_CLASS_STUN_BATON && weapon<=CSWCAnimBase::WEAPON_CLASS_DUAL_SABER;
}
bool PoiseBroken(CSWSCreature* actor) {
    const auto found=actor?meleePoise.find(actor->m_objectId):meleePoise.end();
    return found!=meleePoise.end() && found->second.recoverAt!=0;
}
void UpdateMeleePoise() {
    if(meleePoise.empty()) return;
    auto* server=g_pAppManager->m_pServerExoApp;
    if(!server || !server->m_pInternal) return;
    const auto now=server->GetWorldTimer()->GetSnapshotTime();
    ValidateClashEpisodes(now);
    for(auto it=meleePoise.begin();it!=meleePoise.end();) {
        auto* actor=server->GetCreatureByGameObjectID(it->first);
        if(!actor || actor->GetDead()) {it=meleePoise.erase(it);continue;}
        ConfigureNpcPoise(actor);
        auto& poise=it->second;
        SelectPoiseTiming(poise,StaffPoiseActor(actor),now,DualPoiseActor(actor),SinglePoiseStance(actor));
        if(poise.recoverAt && now>=poise.recoverAt && actor->m_nState==EFFECT_STATE_NONE) {
            RecoverBrokenPoise(poise,now);
            fprintf(stderr,"K1_POISE_RECOVER actor=%llu percent=%d\n",it->first,poise.percent);
        }
        bool swinging=false,eligible=false;PoiseRegenerationState(actor,now,swinging,eligible);
        const int gain=AdvancePoiseRegeneration(poise,now,swinging,eligible);
        if(gain) fprintf(stderr,"K1_POISE_REGEN actor=%llu percent=%d gain=%d now=%lld quiet_until=%lld\n",
            it->first,poise.percent,gain,now,poise.quietUntil);
        ++it;
    }
}
struct PreparedNavigation {
    std::shared_ptr<K1JkaNav::Navigator> graph;
    CSWSArea* area=nullptr;
    float space=0,height=0;
    int uses=0;
    unsigned long long generation=0;
};
std::unordered_map<unsigned long long,PreparedNavigation> npcNavigators;
unsigned long long navigationGeneration=0,navigationUnloads=0;
struct NpcBolt {
    unsigned long long owner=OBJECT_INVALID,weapon=OBJECT_INVALID,area=OBJECT_INVALID;
    Vector position,velocity;
    long long updated=0,expires=0;
};
std::vector<NpcBolt> npcBolts;
void UpdateNpcPlayback();
bool NativeStatusAnimation(CSWSCreature* actor);
void ReleaseNpcPlayback();
bool NpcOwnsBody(Gob* body);
void StopNpcMove(NpcPlayback& npc,CSWSCreature* enemy);
bool PoseNpcBody(Gob* body,float delta);
void TraceNpcBlades(CAurScene* scene);
void TraceNpcBolts(CAurScene* scene);
void BindFrame(Part* part,Vector& position,Quaternion& orientation);
GripPose &lastGrip=playerPlayback.lastGrip,&gripFrom=playerPlayback.gripFrom;
GripPose &gripCorrection=playerPlayback.gripCorrection;
PointCorrection &hiltCorrection=playerPlayback.hiltCorrection,&leftCorrection=playerPlayback.leftCorrection;
std::vector<BodyPose> &lastPose=playerPlayback.lastPose,&blendFrom=playerPlayback.blendFrom;
std::vector<BodyPose> nativeIdlePose;
std::vector<BodyPose> nativeStaffGuardPose;
std::vector<BodyPose> nativeDualGuardPose;
UtilityPoseBlend utilityBlend;
int utilityBlendsCompleted[2] = {};
std::vector<UtilityPoseJoint> CaptureUtilityPose(Gob* body);
GripPose ReadGrip(Gob* body);
UtilityPoseBlend& UtilityBlendFor(Gob* body) {
    for(auto& entry:npcPlayback)if(entry.second.utilityBlend.body==body)return entry.second.utilityBlend;
    return utilityBlend;
}
void BeginUtilityBlend(Gob* body, const std::vector<UtilityPoseJoint>& source, bool toEngage,UtilityPoseBlend& blend=utilityBlend);
void CacheNativeIdle(Gob* body,const char* resource="pmbbl",std::vector<BodyPose>* destination=nullptr,const char* animation="pause1");
std::vector<Quaternion>& rotationFrom=playerPlayback.rotationFrom;
bool& blendPending=playerPlayback.blendPending;
float& blendTime=playerPlayback.blendTime;
// Only the opt-in connected model probe sets these diagnostic coordinates.
int rigDiagnosticState=-1,rigDiagnosticFrame=-1;
K1GroundMove movement;
void PoiseRegenerationState(CSWSCreature* actor,long long now,bool& swinging,bool& eligible) {
    swinging=false;eligible=false;
    if(state.mode==K1EngageState::Mode::Exploration || IsGamePaused() || !HasMeleePoise(actor) || NativeStatusAnimation(actor)) return;
    if(actor==ServerActor()) {
        swinging=state.attacking;
        const bool moving=keyW!=keyS || keyA!=keyD || movement.Speed()>.2f;
        const auto* client=actor->GetClientCreature();
        const int animation=client?client->m_currentAnimation:-1;
        const bool nativeRun=nativeActionBody && (animation==CSWCAnimBase::CLIENT_ANIMATION_RUN ||
            animation==CSWCAnimBase::CLIENT_ANIMATION_RUN_DUAL_SABER || animation==CSWCAnimBase::CLIENT_ANIMATION_RUN_INJURED ||
            animation==CSWCAnimBase::CLIENT_ANIMATION_RUN_RIFLE || animation==CSWCAnimBase::CLIENT_ANIMATION_RUN_STUN_BATON ||
            animation==CSWCAnimBase::CLIENT_ANIMATION_RUN_SINGLE_SABER);
        eligible=!nativeRun && (!moving || keyShift || state.ForceWalk());
    } else {
        auto found=npcPlayback.find(actor->m_objectId);
        if(found==npcPlayback.end()) return; // no reliable gait while native effects own this NPC
        const auto& npc=found->second;swinging=npc.attacking && !npc.ranged;
        const bool moving=now<npc.movingUntil || magnitude(npc.groundVelocity)>.2f;
        eligible=!moving || NpcWalking(npc);
    }
}
void NotePoiseActivity(CSWSCreature* actor,const char* reason) {
    ConfigureNpcPoise(actor);
    if(actor && strcmp(reason,"swing") && strcmp(reason,"parry_guard") && strcmp(reason,"clash"))ClearClashEpisode(actor->m_objectId);
    if(actor && !strcmp(reason,"swing")) {
        auto& p=meleePoise[actor->m_objectId];
        if(p.clashSwing)ClearClashEpisode(actor->m_objectId);
        if(p.clashes)p.clashSwing=true;
    }
    if(!actor || state.mode==K1EngageState::Mode::Exploration || !HasMeleePoise(actor)) return;
    auto& poise=meleePoise[actor->m_objectId];
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    SelectPoiseTiming(poise,StaffPoiseActor(actor),now,DualPoiseActor(actor),SinglePoiseStance(actor));
    DelayPoiseRegeneration(poise,now);
    if(std::getenv("K1_ENGAGE_POISE_REGEN_TEST")) fprintf(stderr,"K1_POISE_ACTIVITY actor=%llu reason=%s now=%lld\n",actor->m_objectId,reason,now);
}
void SpendMeleePoise(CSWSCreature* actor,unsigned long long attackerId,int points,const char* reason) {
    NotePoiseActivity(actor,reason);
    auto& poise=meleePoise[actor->m_objectId];
    poise.percent=std::max(0,poise.percent-points);
    fprintf(stderr,"K1_POISE_HIT actor=%llu percent=%d\n",actor->m_objectId,poise.percent);
    if(poise.percent || poise.recoverAt) return;
    ClearClashEpisode(actor->m_objectId);
    constexpr float stunSeconds=float(poiseStunUs)/1000000.f;
    poise.recoverAt=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime()+poiseStunUs;
    poise.quietUntil=0;poise.wasSwinging=false;
    auto* effect=new CGameEffect(1);
    effect->m_subType=CSWSEffectListHandler::EFFECT_SETSTATE;
    effect->m_durationTypeBits=CGameEffect::DURATION_TYPE_TEMPORARY;
    effect->m_fDuration=stunSeconds;effect->SetCreator(attackerId);
    effect->SetInteger(0,EFFECT_STATE_STUNNED);
    // Physical poise failure only; native Force immunity rules stay intact.
    effect->SetInteger(1,1);actor->ApplyEffect(effect,0,1);
    // A guard break rewards the attacker with a fresh stance-sized chain.
    // Keep the current swing, accepted contacts and attack serial intact.
    auto chainOwner=npcPlayback.find(attackerId);
    if(K1EngageOwnsPlayer(attackerId))state.chainSwings=0;
    else if(chainOwner!=npcPlayback.end())chainOwner->second.chainSwings=0;
    fprintf(stderr,"K1_CHAIN_POISE_RESET attacker=%llu target=%llu\n",attackerId,actor->m_objectId);
    fprintf(stderr,"K1_POISE_BREAK actor=%llu native_state=%d duration=%g\n",actor->m_objectId,actor->m_nState,stunSeconds);
}
void ApplyClashPoise(CSWSCreature* a,CSWSCreature* b) {
    if(meleePoise[a->m_objectId].clashOpponent!=b->m_objectId)ClearClashEpisode(a->m_objectId);
    if(meleePoise[b->m_objectId].clashOpponent!=a->m_objectId)ClearClashEpisode(b->m_objectId);
    const int count=std::min(100,meleePoise[a->m_objectId].clashes+1);
    auto record=[&](CSWSCreature* actor,CSWSCreature* other){
        auto& p=meleePoise[actor->m_objectId];p.clashOpponent=other->m_objectId;p.clashes=count;
        p.clashDecision=0;p.clashParry=p.clashSwing=false;p.clashGuardUntil=0;
        auto weapon=[&](int slot){auto* item=actor->m_pInventory?actor->m_pInventory->GetItemInSlot(slot):nullptr;return item?item->m_objectId:OBJECT_INVALID;};
        p.clashRight=weapon(CSWInventory::SLOT_RIGHTWEAPON);p.clashLeft=weapon(CSWInventory::SLOT_LEFTWEAPON);
    };
    record(a,b);record(b,a);
    // Snapshot both contributions before a possible break changes either actor.
    const int lossB=ClashPoiseDamage(a),lossA=ClashPoiseDamage(b);
    SpendMeleePoise(b,a->m_objectId,lossB,"clash");
    SpendMeleePoise(a,b->m_objectId,lossA,"clash");
    fprintf(stderr,"K1_CLASH_POISE a=%llu b=%llu loss_a=%d loss_b=%d count=%d\n",a->m_objectId,b->m_objectId,lossA,lossB,count);
}
struct MeleeContact {
    unsigned long long attacker,target;int serial;
    long long at=0;
    long long wallAt=0;
    Vector hitPoint,bladeMotion,bladeDirection;
    Part* hitPart=nullptr; // consumed at scene sampling only; never dereferenced after queuing
    int dismemberRow=-1;
    int blade=-1;
    bool poiseOnly=false;
    // Dual contacts retain their own native item identity during scene resolution.
    unsigned long long dualWeapon=OBJECT_INVALID;
    bool offhand=false;
};
int SampleDismemberment(CSWSCreature* attacker,CSWSCreature* target,const MeleeContact& contact);
bool CutSaberContact(CSWSCreature* attacker,const MeleeContact& contact);
struct CutTexture {std::string part,resource;};
std::vector<CutTexture> CutTextures(Gob* model);
std::string CutBasePartName(const std::string& name);
void UpdateDismemberCorpses(CAurScene* rendered=nullptr);
void ClearDismemberArea(unsigned long long areaId);
bool DismemberArenaProbe(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* enemy);
bool RecordNpcDismemberWitness(CSWSCreature* actor,int group,int variant);
int NpcDismemberProbeTick();
const MeleeContact* damageContact=nullptr;
bool ContactTimingEnabled(){static const bool enabled=std::getenv("K1_ENGAGE_CONTACT_TIMING")!=nullptr;return enabled;}
long long ContactWallUs(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct ContactEffectClock {long long world=0,wall=0,contactWorld=0,contactWall=0;unsigned long long attacker=OBJECT_INVALID,target=OBJECT_INVALID;int serial=0,hand=-1;};
std::unordered_map<unsigned long long,ContactEffectClock> contactEffectClocks;
struct ContactResponseClock {long long wall=0,world=0;unsigned int serial=0;bool attacking=false,native=false,interrupted=false;};
std::unordered_map<unsigned long long,ContactResponseClock> contactBlockClocks;
ContactResponseClock contactGuardClock;
std::vector<MeleeContact> sceneMeleeContacts;
unsigned long long meleeClashes=0;
unsigned long long meleeDamageContacts=0;
long long lastClashGap=0;
unsigned long long directionalParries=0;
unsigned long long npcTimedParries=0;
int parryProbeDirection=1;
bool parryRenderRequested=false;
bool parryRenderFailed=false;
long long parrySuccessUntil=0;
int parryRenderChecks=0;
struct ClashExpiryObservation {int playerHP=0,enemyHP=0,playerBase=0,enemyBase=0;} clashExpiryObservation;
void ResolveMeleeContacts(const std::vector<MeleeContact>& contacts,long long sampleTime=-1);
std::vector<unsigned long long> encounter;
std::unordered_map<unsigned long long, long long> enemyRepositionAfter;
Vector previousBase, previousTip;
void IntegrationTick();
void ArenaTick();
const char* attacks[] = {"y_top", "y_tlbr", "y_trbl", "y_lr", "y_rl", "y_bltr", "y_brtl"};
const char* fullAttacks[] = {"s_top", "s_tlbr", "s_trbl", "s_lr", "s_rl", "s_bltr", "s_brtl"};
const char* blockFull[]={"j_block_up","j_block_ur","j_block_ul","j_block_lr","j_block_ll"};
const char* blockUpper[]={"y_block_up","y_block_ur","y_block_ul","y_block_lr","y_block_ll"};
int BlockReactionForAttack(int direction) {
    // Defender-facing sides, matching the existing incoming-cut/parry mapping.
    const int reaction[]={0,1,2,1,2,4,3};
    return direction>=0 && direction<7?reaction[direction]:-1;
}
const char* BlockReference(const char* overlay) {
    for(int i=0;overlay && i<5;++i) if(!strcmp(overlay,blockUpper[i])) return blockFull[i];
    return nullptr;
}
int IncomingBlockReaction(CSWSCreature* attacker) {
    if(!attacker) return -1;
    if(K1EngageOwnsPlayer(attacker->m_objectId) && state.attacking) return BlockReactionForAttack(swingDirection);
    const auto found=npcPlayback.find(attacker->m_objectId);
    return found!=npcPlayback.end() && found->second.attacking && !found->second.ranged?
        BlockReactionForAttack(found->second.direction):-1;
}
void QueueBlockReaction(CSWSCreature* defender,int reaction) {
    if(!defender || reaction<0 || reaction>=5 || PoiseBroken(defender) || NativeStatusAnimation(defender)) return;
    auto* client=defender->GetClientCreature();
    auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
    bool full=false,upper=false;
    if(body) for(int i=0;i<body->m_pModel->m_animations.m_count;++i) {
        const char* name=body->m_pModel->m_animations.m_pData[i]->m_szName;
        full=full || !strcmp(name,blockFull[reaction]);upper=upper || !strcmp(name,blockUpper[reaction]);
    }
    if(!full || !upper) {fprintf(stderr,"K1_BLOCK_REACTION_MISSING actor=%llu reaction=%d\n",defender->m_objectId,reaction);return;}
    auto& poise=meleePoise[defender->m_objectId];
    poise.blockReaction=reaction;poise.blockStarted=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    ++poise.blockSerial;
    if(ContactTimingEnabled())contactBlockClocks[defender->m_objectId]={ContactWallUs(),poise.blockStarted,poise.blockSerial,false,false};
    fprintf(stderr,"K1_BLOCK_REACTION_CONTACT actor=%llu serial=%u clip=%s start=%lld\n",
        defender->m_objectId,poise.blockSerial,blockUpper[reaction],poise.blockStarted);
}
void ObserveBlockPose(Gob* body,PosePlayback& pose,const char* overlay,unsigned long long actor,bool moving) {
    if(ContactTimingEnabled() && BlockReference(overlay)) {
        auto found=contactBlockClocks.find(actor);
        if(found!=contactBlockClocks.end()) {
            fprintf(stderr,"K1_CONTACT_BLOCK_POSE actor=%llu wall_us=%lld world_us=%lld clip=%s\n",actor,ContactWallUs()-found->second.wall,
                g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime()-found->second.world,overlay);
            contactBlockClocks.erase(found);
        }
    }
    if(!std::getenv("K1_ENGAGE_PARRY_TEST") || !BlockReference(overlay)) return;
    const auto& poise=meleePoise[actor];auto& observation=blockObservations[actor];
    if(observation.started!=poise.blockStarted) {observation=BlockObservation();observation.started=poise.blockStarted;}
    observation.reaction=poise.blockReaction;++observation.samples;
    observation.moving=observation.moving || moving;
    observation.settled+=pose.blendTime>=.14f;
    observation.maximumGrip=std::max(observation.maximumGrip,pose.sourceContactError);
    for(const auto& p:pose.lastPose) observation.finite=observation.finite &&
        K1SaberContact::Finite(p.part->m_vLocalPosition) && std::isfinite(dot(p.part->m_qLocalOrientation,p.part->m_qLocalOrientation));
    float time=0,length=0;
    auto* creature=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(actor);
    auto* client=creature?creature->GetClientCreature():nullptr;
    const bool independent=client && client->GetAnimationBase() && client->GetAnimationBase()->m_nWeaponClass==CSWCAnimBase::WEAPON_CLASS_DUAL_SABER;
    observation.finite=observation.finite && body->GetAnimation(const_cast<char*>(overlay),&time,&length) &&
        time>=0 && time<length && pose.lastGrip.joined!=independent;
    if(observation.samples==1) fprintf(stderr,"K1_BLOCK_REACTION_POSE actor=%llu clip=%s joined=%d grip=%g finite=%d\n",
        actor,overlay,pose.lastGrip.joined,pose.sourceContactError,observation.finite);
}
// Separate player/NPC melee rates. Contact, chaining and pose calibration remain
// in clip time, so slowing playback preserves their alignment with the blade.
constexpr float npcMeleeSwingRate=.85f;
constexpr float playerMeleeSwingRate=1.f;
bool LiveMeleeRate(Gob* body,const char* clip,float expected) {
    if(!body || !clip) return false;
    for(int i=0;i<body->m_paAnimations.m_count;++i) {
        auto* run=body->m_paAnimations.m_pData[i];
        if(!strcmp(run->m_pAnimation->m_szName,clip)) return std::fabs(run->m_fSpeed-expected)<.000001f;
    }
    return false;
}
// Intentional NPC choice bias: overhead4, upper diagonals3 each, sides3 each,
// rising cuts1 each. Native weapon damage and ranged fire rates are unchanged.
int NpcMeleeDirectionForRoll(int roll) {
    const int weights[]={4,3,3,3,3,1,1};
    if(roll<0 || roll>=18) return 0;
    for(int direction=0;direction<7;++direction) {
        if(roll<weights[direction]) return direction;
        roll-=weights[direction];
    }
    return 0;
}
const char* bases[] = {"j_idle","j_guard","j_walk","j_run","j_backwalk","j_backrun",
    "j_leftwalk","j_rightwalk","j_leftrun","j_rightrun"};
// Owned JKA animation.cfg: S2 wind-up end, followed by the A2 swing segment end.
const float hitWindows[7][2] = {{.23076923f,.43076923f},{.24242424f,.60242424f},
    {.22222222f,.58222222f},{.23863636f,.55863636f},{.24f,.54f},
    {.23863636f,.53863636f},{.22727273f,.46727273f}};
constexpr int maxChainSwings = 3;
constexpr float chainFollowThrough = .06f;
constexpr float chainClickLead = .14f;
bool SwingSoundDue(int serial,int& played,float time,float start) {
    if(time<start || played==serial) return false;
    played=serial;return true;
}
void SaberContactVisual(CSWSCreature* actor,CSWSCreature* target,const char* kind);
void EngageWeaponSound(CSWSCreature* actor,CSWSCreature* target,const char* kind) {
    if(!strcmp(kind,"clash") || !strcmp(kind,"guard") || !strcmp(kind,"parry"))SaberContactVisual(actor,target,kind);
    if(!actor || (!K1EngageOwnsPlayer(actor->m_objectId) && !K1EngageOwnsEnemy(actor->m_objectId))) return;
    auto* client=actor?actor->GetClientCreature():nullptr;
    auto* item=actor && actor->m_pInventory?actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON):nullptr;
    if(!client || !item || (!item->IsLightsaber() && !SwordType(item->m_baseItem) &&
        !((actor==ServerActor() || NativeConfiguredNpc(actor)) && NativeNpcBluntType(item->m_baseItem)))) return;
    const bool trace=std::getenv("K1_ENGAGE_SOUND_TEST")!=nullptr;
    if(trace) fprintf(stderr,"K1_ENGAGE_SOUND_BEGIN kind=%s actor=%llu\n",kind,actor->m_objectId);
    int result=0;
    if(!strcmp(kind,"hit")) {
        auto* victim=target?target->GetClientCreature():nullptr;
        if(victim) {
            CSWCCombatAttackData data;
            data.m_attackerObjectId=g_pAppManager->m_pClientExoApp->ServerToClientObjectId(actor->m_objectId);
            // Native PlayHitSound resolves this recovered field as its target.
            data.m_weaponObjectId=g_pAppManager->m_pClientExoApp->ServerToClientObjectId(target->m_objectId);
            data.m_nWeaponAttackType=1;data.m_nAttackResult=1;
            data.m_rangedAttackGroup.m_bRangedAttack=0;
            result=client->PlayHitSound(&data,0);
        }
    } else {
        const char* column=!strcmp(kind,"swing")?"SwingShort":(!strcmp(kind,"parry")?"Parry":"Clash");
        result=client->PlaySwingSound(CExoString(column),nullptr,!strcmp(kind,"swing")?3:2);
    }
    if(trace) fprintf(stderr,"K1_ENGAGE_SOUND_END kind=%s actor=%llu result=%d\n",kind,actor->m_objectId,result);
}

bool StaffPrototype() { return std::getenv("K1_ENGAGE_STAFF_PROTOTYPE")!=nullptr; }
bool DualPrototype() { return std::getenv("K1_ENGAGE_DUAL_PROTOTYPE")!=nullptr; }
void QueueAttackDirection() {
    int forward=int(keyW)-int(keyS),right=int(keyD)-int(keyA);
    requestedDirection=K1EngageState::AttackDirection(float(-forward),float(right));
    requestedBackLeft=forward<0 && right<0;
    state.attackRequested=true;
    fprintf(stderr,"K1_ENGAGE direction forward=%d side=%d clip=%s\n",
        forward,right,attacks[requestedDirection]);
}

CSWCCreature* Actor() {
    return g_pAppManager && g_pAppManager->m_pClientExoApp
        ? g_pAppManager->m_pClientExoApp->GetPlayerCreature() : nullptr;
}
CSWSCreature* ServerActor() { CSWCCreature* actor = Actor(); return actor ? actor->GetServerCreature() : nullptr; }
bool DualMode() {
    auto* actor=ServerActor();
    if(!actor || actor->GetDead() || !actor->m_pInventory ||
       (!DualPrototype() && !std::getenv("K1_ENGAGE_SANDBOX") && !ActorSwordPair(actor))) return false;
    auto* right=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
    auto* left=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON);
    auto saber=[](auto* item){return item && SingleMeleeType(item->m_baseItem);};
    return saber(right) && saber(left) && right!=left;
}
bool StaffMode() {
    auto* actor=ServerActor();
    auto* item=actor && actor->m_pInventory?actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON):nullptr;
    return item && (((StaffPrototype() || K1EngageArenaLoadoutActive()) && item->m_baseItem==BASE_ITEM_DOUBLE_BLADED_LIGHTSABER) || DoubleSwordType(item->m_baseItem) || item->m_baseItem==BASE_ITEM_QUARTER_STAFF || item->m_baseItem==BASE_ITEM_GHAFFI_STICK);
}
bool StaffPoiseActor(CSWSCreature* actor){return actor && ((actor==ServerActor() && StaffMode()) || StaffNpc(actor));}
bool DualPoiseActor(CSWSCreature* actor){return actor && ((actor==ServerActor() && DualMode()) || DualNpc(actor));}
int redSelection=-1;
int redChainFrom=-1;
int blueChainFrom=-1;
bool PurpleMode() {return !StaffMode() && !DualMode() && (redSelection>=0?redSelection==3:std::getenv("K1_ENGAGE_PURPLE")!=nullptr);}
bool BlueMode() {return !StaffMode() && !DualMode() && (redSelection>=0?redSelection==2:!PurpleMode() && std::getenv("K1_ENGAGE_BLUE")!=nullptr);}
bool RedMode() {return !StaffMode() && !DualMode() && (redSelection>=0?redSelection==1:!PurpleMode() && !BlueMode() && std::getenv("K1_ENGAGE_RED")!=nullptr);}
float PlayerMeleeRate(){return StaffMode()?1.10f:DualMode()?.85f:PurpleMode()?1.30f:BlueMode()?1.15f:playerMeleeSwingRate;}
int PlayerChainLimit(){return StaffMode() || BlueMode() || PurpleMode()?4:maxChainSwings;}
Vector PoiseNumberColor(const Vector& defaultColor) {
    if(StaffMode() || DualMode())return defaultColor;
    return PurpleMode()?Vector(.75f,.3f,1.f):BlueMode()?Vector(.2f,.5f,1.f):
        RedMode()?Vector(1.f,.2f,.2f):Vector(1.f,1.f,.15f);
}
int SinglePoiseStance(CSWSCreature* actor) {
    if(!actor)return 0;
    if(actor==ServerActor())return PurpleMode()?3:BlueMode()?2:RedMode()?1:0;
    return BlueNpc(actor)?2:RedNpc(actor)?1:0;
}
bool UsesRedAttacks() {return RedMode() || PurpleMode();}
const char* SingleStanceResource(int selection) {
    const char* resources[]={"k1jarig","k1jared","k1jablue","k1japurp"};
    return selection>=0 && selection<4?resources[selection]:nullptr;
}
int NextSingleStance(bool hasBlue,bool hasPurple) {
    return PurpleMode()?0:BlueMode()?(hasPurple?3:0):RedMode()?(hasBlue?2:hasPurple?3:0):1;
}
bool PurpleAuthoredGuard(bool guarding,bool attacking,bool blocked) {
    return PurpleMode() && guarding && !attacking && !blocked;
}
const char* PurpleGuardWalkClip(bool guarding,bool attacking,bool blocked,bool moving,bool backward) {
    if(!moving || !PurpleAuthoredGuard(guarding,attacking,blocked))return nullptr;
    return backward?"j_purpbackwalk":"j_purpwalk";
}
CCAppearanceInfo LiveAppearance(CSWCCreature* actor);
void NativePlayerModel(CSWCCreature* actor,CCAppearanceInfo& appearance,CResRef& model,CResRef* texture=nullptr) {
    CExoString type;
    g_pRules->m_p2DArrays->m_pAppearance->GetCExoStringEntry(appearance.core.m_nAppearanceId,CExoString("modeltype"),&type);
    if(type=="B")actor->m_pCreatureAppearance->GetBTypeModelAndTextureResRef(&appearance,&model,texture);
    else {CExoString race;g_pRules->m_p2DArrays->m_pAppearance->GetCExoStringEntry(appearance.core.m_nAppearanceId,CExoString("race"),&race);model=race;}
}
const char* PlayerRigForNative(const char* native) {
    if(auto* r=WearableRig(native)) {
        const auto& style=DualMode()?r->dual:StaffMode()?r->staff:PurpleMode()?r->purple:BlueMode()?r->blue:RedMode()?r->red:r->single;
        if(!style.empty())return style.c_str();
    }
    return DualMode()?"k1jadual":StaffMode()?"k1jastaff":SingleStanceResource(PurpleMode()?3:BlueMode()?2:RedMode()?1:0);
}
const char* PlayerRigResource() {
    auto* actor=Actor();
    if(actor && actor->m_pCreatureAppearance && g_pRules && g_pRules->m_p2DArrays) {
        auto appearance=LiveAppearance(actor);CResRef native;NativePlayerModel(actor,appearance,native);
        return PlayerRigForNative(native.GetResRefStr());
    }
    return PlayerRigForNative("pmbbl");
}
bool SingleStanceResourceName(const char* name) {
    for(int i=0;i<4;++i)if(!_stricmp(name,SingleStanceResource(i)))return true;
    for(const auto& r:wearableRigs)for(const auto* s:{&r.single,&r.red,&r.blue,&r.purple})if(!_stricmp(name,s->c_str()))return true;
    return false;
}
// Strong S3/A3 clip boundaries in animation seconds. Yellow tuning stays intact.
constexpr float redHitWindows[7][2]={{.73333333f,1.38333333f},{.73333333f,1.16666667f},
    {.46666667f,.73333333f},{.83333333f,1.23333333f},{.73333333f,1.f},
    {.7f,1.06666667f},{.56666667f,.93333333f}};
#include "include/k1redchain.inc"
#include "include/k1bluechain.inc"
#include "include/k1bluewindows.inc"
#include "include/k1bluespins.inc"
#include "include/k1redspins.inc"
bool BlueSpinChain(int from,int to) {
    return from>=0 && from<7 && to>=0 && to<7 && blueSpinChains[from][to];
}
bool RedSpinChain(int from,int to) {
    return from>=0 && from<7 && to>=0 && to<7 && redSpinChains[from][to];
}
bool PlayerSpinChain() {
    return state.attacking && ((BlueMode() && BlueSpinChain(blueChainFrom,swingDirection)) ||
        (UsesRedAttacks() && RedSpinChain(redChainFrom,swingDirection)));
}
bool NpcSpinChain(const NpcPlayback& npc) {
    return npc.attacking && ((npc.blue && BlueSpinChain(npc.blueFrom,npc.direction)) ||
        (npc.red && RedSpinChain(npc.redFrom,npc.direction)));
}
const char* PlayerAttackClip(bool upper) {
    if(BlueMode() && blueChainFrom>=0) {
        const auto& clip=blueChains[blueChainFrom][swingDirection];
        return upper && !BlueSpinChain(blueChainFrom,swingDirection)?clip.upper:clip.full;
    }
    if(UsesRedAttacks() && redChainFrom>=0) {
        const auto& clip=redChains[redChainFrom][swingDirection];
        return upper && !RedSpinChain(redChainFrom,swingDirection)?clip.upper:clip.full;
    }
    return upper?attacks[swingDirection]:fullAttacks[swingDirection];
}
const char* NpcAttackClip(const NpcPlayback& npc,bool upper) {
    if(npc.blue && npc.blueFrom>=0) {
        const auto& clip=blueChains[npc.blueFrom][npc.direction];
        return upper && !BlueSpinChain(npc.blueFrom,npc.direction)?clip.upper:clip.full;
    }
    if(npc.red && npc.redFrom>=0) {
        const auto& clip=redChains[npc.redFrom][npc.direction];
        return upper && !RedSpinChain(npc.redFrom,npc.direction)?clip.upper:clip.full;
    }
    return upper?attacks[npc.direction]:fullAttacks[npc.direction];
}
const char* AttackFullReference(const char* upper) {
    for(int i=0;i<7;++i)if(!strcmp(upper,attacks[i]))return fullAttacks[i];
    for(const auto& row:redChains)for(const auto& clip:row)if(!strcmp(upper,clip.upper))return clip.full;
    for(const auto& row:blueChains)for(const auto& clip:row)if(!strcmp(upper,clip.upper))return clip.full;
    return nullptr;
}
constexpr float dualHitWindows[7][2]={{.25f,.55f},{.25f,.60f},{.25f,.55f},{.25f,.55f},
    {.25f,.55f},{.25f,.55f},{.30f,.60f}};
int dualContactSerial=-1;
unsigned int dualContactHands=0;
float dualWalkLowerPositionError=0,dualWalkLowerRotationError=0;
int dualWalkLowerSamples=0;
bool dualGuardAfterNative=false;
// Staff timings come from its S7/A7 source clip boundaries. Completed stance
// timings remain in hitWindows and are never retuned by this selection path.
constexpr float staffHitWindows[7][2]={{.4f,.7f},{.25f,.55f},{.25f,.55f},{.25f,.55f},
    {.25f,.55f},{.25f,.55f},{.25f,.55f}};
float NpcHitStart(const NpcPlayback& npc) {if(npc.blue)return npc.blueFrom>=0?blueChains[npc.blueFrom][npc.direction].start:blueHitWindows[npc.direction][0]; return npc.red?(npc.redFrom>=0?redChains[npc.redFrom][npc.direction].start:redHitWindows[npc.direction][0]):(npc.dual?dualHitWindows[npc.direction][0]:(npc.staff?staffHitWindows[npc.direction][0]:hitWindows[npc.direction][0]));}
float NpcHitEnd(const NpcPlayback& npc) {if(npc.blue)return npc.blueFrom>=0?blueChains[npc.blueFrom][npc.direction].end:blueHitWindows[npc.direction][1]; return npc.red?(npc.redFrom>=0?redChains[npc.redFrom][npc.direction].end:redHitWindows[npc.direction][1]):(npc.dual?dualHitWindows[npc.direction][1]:(npc.staff?staffHitWindows[npc.direction][1]:hitWindows[npc.direction][1]));}
int NpcChainLimit(const NpcPlayback& npc) {return npc.staff?k1DarkJediTypes[1].chainLimit:(npc.blue?4:maxChainSwings);}
float NpcSwingRate(const NpcPlayback& npc) {
    return npc.dual?k1DarkJediTypes[2].swingRate:npc.staff?k1DarkJediTypes[1].swingRate:
        npc.red?k1DarkJediTypes[3].swingRate:npc.blue?k1DarkJediTypes[4].swingRate:npcMeleeSwingRate;
}
const K1NpcType* RegisteredNpcType(const NpcPlayback& npc) {
    auto* actor=npc.owner?npc.owner->GetServerCreature():nullptr;
    if(NativeConfiguredNpc(actor)){if(RedNpc(actor))return &k1DarkJediTypes[3];if(BlueNpc(actor))return &k1DarkJediTypes[4];}
    for(const auto& type:k1DarkJediTypes)if(actor && !_stricmp(actor->m_sTag.CStr(),type.tag))return &type;
    return nullptr;
}
long long NpcAttackRest(const NpcPlayback& npc){auto* type=RegisteredNpcType(npc);return type?type->attackRestUs:150000;}
bool NpcChainAccepted(NpcPlayback& npc) {
    if(npc.chainDecisionSerial!=npc.serial) {
        npc.chainDecisionSerial=npc.serial;auto* type=RegisteredNpcType(npc);
        const int chance=type?type->chainContinuePercent:100;
        npc.continueChain=chance==100 || g_pRules->RollDice(1,100)<=chance;
        fprintf(stderr,"K1_NPC_CHAIN_DECISION actor=%llu serial=%d chance=%d accepted=%d\n",npc.id,npc.serial,chance,npc.continueChain);
    }
    return npc.continueChain;
}
float PlayerHitStart(int direction) {if(BlueMode())return blueChainFrom>=0?blueChains[blueChainFrom][direction].start:blueHitWindows[direction][0]; return UsesRedAttacks()?(redChainFrom>=0?redChains[redChainFrom][direction].start:redHitWindows[direction][0]):(DualMode()?dualHitWindows[direction][0]:(StaffMode()?staffHitWindows[direction][0]:hitWindows[direction][0]));}
float PlayerHitEnd(int direction) {if(BlueMode())return blueChainFrom>=0?blueChains[blueChainFrom][direction].end:blueHitWindows[direction][1]; return UsesRedAttacks()?(redChainFrom>=0?redChains[redChainFrom][direction].end:redHitWindows[direction][1]):(DualMode()?dualHitWindows[direction][1]:(StaffMode()?staffHitWindows[direction][1]:hitWindows[direction][1]));}
// Contact includes the same short follow-through already allowed before a chain
// handoff. Keep strike-end timing for chain input, guard cancellation and posing.
float ContactEnd(float strikeEnd) {return strikeEnd+chainFollowThrough;}
float StaffContactEnd(int direction) {
    // W+D/D now continue the forward turn for120ms before authored return.
    // Contact follows that visible motion; chain/guard still use strike end.
    return staffHitWindows[direction][1]+((direction==1 || direction==3)?.12f:chainFollowThrough);
}
bool ContactTimingProbe() {
    bool ok=true;int cases=0;
    for(const auto* windows:{hitWindows,staffHitWindows}) for(int d=0;d<7;++d) {
        const float start=windows[d][0],strikeEnd=windows[d][1],end=windows==staffHitWindows?StaffContactEnd(d):ContactEnd(strikeEnd);
        float lo=0,hi=1;
        auto active=[&](float a,float b) {++cases;return K1SaberContact::ClipTime(a,b,start,end,lo,hi);};
        ok=active(start,start) && ok;
        ok=!active(start-.02f,start-.01f) && ok;
        ok=active(strikeEnd+.01f,end-.01f) && ok;
        ok=!active(end,end) && !active(end+.01f,end+.02f) && ok;
        ok=!active(end,start) && ok;
        ok=active(start-.1f,end+.1f) && lo>0 && hi<1 && ok;
        // A long frame crosses both excluded and included geometry. The clipped
        // sweep must reject windup/late recovery, but retain follow-through hits.
        K1SaberContact::Blade a={{-1,0,0},{-1,0,1}},b={{1,0,0},{1,0,1}};
        const float from=start-.1f,to=end+.1f;
        auto at=[&](float t) {const float x=-1+2*(t-from)/(to-from);return K1SaberContact::Triangle{{x,-.1f,.4f},{x,.1f,.4f},{x,0,.6f}};};
        auto x=K1SaberContact::Between(a,b,lo),y=K1SaberContact::Between(a,b,hi);
        ok=!K1SaberContact::SweepCell(x,y,at(start-.05f)) &&
            K1SaberContact::SweepCell(x,y,at((strikeEnd+end)*.5f)) &&
            !K1SaberContact::SweepCell(x,y,at(end+.05f)) && ok;
    }
    fprintf(stderr,"K1_MELEE_CONTACT_TIMING_%s windows=14 boundary_cases=%d follow_through=%g staff_extended_tail=.12 chain_guard_unchanged=1\n",ok?"PASS":"FAIL",cases,chainFollowThrough);
    return ok;
}
bool Supported(CSWCCreature* actor) {
    if(DualMode() && actor==Actor() && actor && actor->GetAnimationBase())
        return actor->GetAnimationBase()->m_nWeaponClass==CSWCAnimBase::WEAPON_CLASS_DUAL_SABER;
    if(StaffMode() && actor==Actor() && actor && actor->GetAnimationBase())
        return actor->GetAnimationBase()->m_nWeaponClass==CSWCAnimBase::WEAPON_CLASS_TWO_HANDED_SABER;
    return actor && actor->GetServerCreature() && !actor->GetServerCreature()->GetDead()
        && actor->GetServerCreature()->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON)
        && SingleMeleeType(actor->GetServerCreature()->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON)->m_baseItem)
        && actor->GetAnimationBase()
        && (actor->GetAnimationBase()->m_nWeaponClass == CSWCAnimBase::WEAPON_CLASS_SINGLE_SABER || actor->GetAnimationBase()->m_nWeaponClass == CSWCAnimBase::WEAPON_CLASS_STUN_BATON);
}
void MaintainSaberCombat(CSWCCreature* actor) {
    CSWSCreature* server=actor->GetServerCreature();server->SetCombatState(1,1);
    if(server->m_pCombatRound && server->m_pCombatRound->m_bSpellCastRound) return;
    CSWCItem* item=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[4]);
    if(item && (SaberType(item->m_baseItem) ||
        (NativeConfiguredNpc(server) && item->m_baseItem==BASE_ITEM_QUARTER_STAFF)) && !item->m_bPowered) item->PowerItem(1,0,1,0);
    if((DualMode() && actor==Actor()) || (NativeConfiguredNpc(server) && DualNpc(server))) {
        auto* left=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[5]);
        if(left && SaberType(left->m_baseItem) && !left->m_bPowered) left->PowerItem(1,0,1,0);
    }
}
bool Spotted(bool trace=false) {
    CSWSCreature* actor = ServerActor();
    if (!actor || !actor->GetArea()) return true; // fail closed during loads
    CSWSArea* area = static_cast<CSWSArea*>(actor->GetArea());
    for (int i = 0; i < area->m_aObjectsByX.m_count; ++i) {
        CSWSCreature* npc = g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);
        if(trace && npc && npc!=actor && !npc->GetDead()) fprintf(stderr,"K1_ENGAGE_THREAT_SCAN npc=%llu reputation=%d native_combat=%d target=%llu attempted=%llu objects=%d\n",
            npc->m_objectId,npc->GetReputation(actor->m_objectId,0),npc->m_bCombatState,npc->m_attackTargetId,npc->m_attemptedAttackTargetId,npc->m_actionList.m_pInternal->m_nCount);
        if (!npc || npc == actor || npc->GetDead() || SandboxIdle(npc)) continue;
        // A sandbox fight still needs the existing combat activation path when
        // the player is excluded. Detect the actual opposing NPC, not a threat
        // to the player, and keep native alignment/target policy independent.
        auto sandbox=sandboxActors.find(npc->m_objectId);
        auto* opponent=sandboxSession && !sandboxTargetPlayer && sandbox!=sandboxActors.end() && !sandbox->second.friendly?SandboxTarget(npc,actor):nullptr;
        if(!opponent && npc->GetReputation(actor->m_objectId,0)>10)continue;
        auto report=[&](const char* why) {
            if(state.mode!=K1EngageState::Mode::Exploration && std::find(encounter.begin(),encounter.end(),npc->m_objectId)==encounter.end())
                encounter.push_back(npc->m_objectId);
            if(trace) fprintf(stderr,"K1_ENGAGE_THREAT npc=%llu reason=%s round=%d objects=%d\n",npc->m_objectId,why,
                npc->m_pCombatRound?npc->m_pCombatRound->m_bRoundStarted:-1,npc->m_actionList.m_pInternal->m_nCount);
            return true;
        };
        if(opponent) {
            auto* seen=npc->GetVisibleListElement(opponent->m_objectId);
            if(seen && (seen->m_flags & 1))return report("sandbox_opponents");
        }
        const auto owned=npcPlayback.find(npc->m_objectId);
        if(owned!=npcPlayback.end() && owned->second.hasMove && owned->second.navigation.active)
            return report("engage_pursuit");
        CSWVisibilityNode* visible = npc->GetVisibleListElement(actor->m_objectId);
        if (visible && (visible->m_flags & 1)) return report("sight");
        if(npc->m_bCombatState && (npc->m_attemptedAttackTargetId==actor->m_objectId
            || npc->m_oidAttemptedMovementTarget==actor->m_objectId)) return report("native_pursuit");
        auto pursuing=[&](CSWSCombatRoundAction* action) {
            return action && action->m_oidTarget==actor->m_objectId &&
                (action->m_nActionType==CSWSCombatRoundAction::TYPE_ATTACK || action->m_nActionType==CSWSCombatRoundAction::TYPE_FEAT_ATTACK);
        };
        CSWSCombatRound* round=npc->m_pCombatRound;
        if(round) {
            if(round->m_bRoundStarted && pursuing(round->m_pCurrentAction)) return report("executing_round");
            if(npc->GetAction(AIACTION_COMBAT) && round->m_pRoundActionList) {
                auto* list=round->m_pRoundActionList->m_pInternal;
                for(CExoLinkedListNode* node=list->m_pHead;node;list->GetNext(node))
                    if(pursuing(static_cast<CSWSCombatRoundAction*>(list->GetAtPos(node)))) return report("queued_round");
            }
        }
        CSWSObjectActionNode* attack=npc->GetAction(AIACTION_PHYSICAL_ATTACKS);
        if(attack && attack->m_params[1].m_n==actor->m_objectId) return report("attack_action");
        for(int action : {AIACTION_CHECK_MOVE_TO_OBJECT,AIACTION_CHECK_MOVE_TO_OBJECT_RADIUS}) {
            CSWSObjectActionNode* move=npc->GetAction(action);
            if(move && move->m_params[0].m_n==actor->m_objectId) return report("move_action");
        }
    }
    return false;
}
void StopOverlays(Gob* body) {
    if (!body) return;
    for (const char* name : attacks) body->PlayAnimation(const_cast<char*>(name), 1, AnimRun::kAnimRemove, 0);
    for (const char* name : fullAttacks) body->PlayAnimation(const_cast<char*>(name), 1, AnimRun::kAnimRemove, 0);
    for(const auto& row:redChains)for(const auto& clip:row) {
        body->PlayAnimation(const_cast<char*>(clip.full),1,AnimRun::kAnimRemove,0);
        body->PlayAnimation(const_cast<char*>(clip.upper),1,AnimRun::kAnimRemove,0);
    }
    for(const auto& row:blueChains)for(const auto& clip:row) {
        body->PlayAnimation(const_cast<char*>(clip.full),1,AnimRun::kAnimRemove,0);
        body->PlayAnimation(const_cast<char*>(clip.upper),1,AnimRun::kAnimRemove,0);
    }
    for (const char* name : bases) body->PlayAnimation(const_cast<char*>(name), 1, AnimRun::kAnimRemove, 0);
    for(const char* name:{"j_purpwalk","j_purpbackwalk"})body->PlayAnimation(const_cast<char*>(name),1,AnimRun::kAnimRemove,0);
    for(const char* name:blockUpper) body->PlayAnimation(const_cast<char*>(name),1,AnimRun::kAnimRemove,0);
    body->PlayAnimation(const_cast<char*>("y_guard"), 1, AnimRun::kAnimRemove, 0);
    body->PlayAnimation(const_cast<char*>("y_ready"), 1, AnimRun::kAnimRemove, 0);
    body->PlayAnimation(const_cast<char*>("y_idle"), 1, AnimRun::kAnimRemove, 0);
    currentBase=currentOverlay=nullptr;
}
void ReleaseAnimations(Gob* body) {
    playerPlayback.stanceTransition=false;
    playerPlayback.nativeGait={};
    nativeIdlePose.clear();nativeStaffGuardPose.clear();nativeDualGuardPose.clear();
    StopOverlays(body);
    body->RemoveAddInAnimations();
    // Native idle/walk controllers are sparse. Restore the native bind values
    // before they take ownership, including JKA's translated pelvis and torso.
    // Reset must follow removal: it resolves remaining runs against native models.
    body->Reset();
    lastPose.clear();blendFrom.clear();blendPending=false;blendTime=.15f;
}
CCAppearanceInfo LiveAppearance(CSWCCreature* actor) {
    CCAppearanceInfo appearance=actor->m_appearanceInfo;
    auto* cached=actor->m_pCreatureAppearance;
    appearance.core.m_bTypeVariant=cached->m_cachedBTypeVariant;
    appearance.core.m_bTypeNumber=cached->m_cachedBTypeNumber;
    for(int i=0;i<6;++i) appearance.core.m_aBodyParts[i]=cached->m_aCachedBodyParts[i];
    appearance.core.m_oidHeadItem=cached->m_oidHeadItem;appearance.core.m_bArmorEquipped=cached->m_bArmorEquipped;
    appearance.core.m_nAppearanceId=cached->m_nAppearanceId;appearance.core.m_nHeadNumber=cached->m_nHeadNumber;
    appearance.core.m_nGender=cached->m_nGender;appearance.core.m_nPhenotype=cached->m_nPhenotype;
    appearance.core.m_nSkinColor=cached->m_nSkinColor;appearance.core.m_nHairColor=cached->m_nHairColor;
    appearance.core.m_nTattooColor1=cached->m_nTattooColor1;appearance.core.m_nTattooColor2=cached->m_nTattooColor2;
    appearance.core.m_tailVariant=cached->m_cachedTailVariant;appearance.core.m_wingVariant=cached->m_cachedWingVariant;
    appearance.core.m_bUseBackupHead=cached->m_bUseBackupHead;appearance.core.m_race=cached->m_race;
    return appearance;
}
bool RebindRigFollowCamera(CSWCCreature* actor,CSWCModule* module,CAurCamera* camera,int previousType,const Vector& eye,const Quaternion& orientation) {
    if(!actor || actor!=Actor() || !module || !camera || module->m_pCamera!=camera)return false;
    auto* target=static_cast<CAurObject*>(actor->GetCameraFollowPart());
    if(!target)return false;
    auto* behavior=camera->GetBehavior(-1);
    if(behavior && behavior->GetType()==BEHAVIOR_CAMERA_COMBAT) {
        module->m_cSavedCameraSettings.m_pTarget=target;
        return true;
    }
    if(behavior && behavior->GetType()!=BEHAVIOR_CAMERA_ON_A_STICK)return false;
    if(!behavior && previousType!=BEHAVIOR_CAMERA_ON_A_STICK && module->m_nCameraMode!=CClientOptions::CAMERA_MODE_ON_A_STICK)return false;
    auto* follow=behavior?static_cast<CSWCameraOnAStick*>(behavior):new CSWCameraOnAStick(camera->AsCAurObject(),target);
    if(!behavior)camera->SetBehavior(follow);
    follow->m_nFollowingBody=actor->GetFollowingBody();
    follow->m_fCameraOffset=actor->GetHeadHeightOffset()+actor->GetCameraOffset();
    follow->ChangeTarget(target,0);
    module->m_cSavedCameraSettings.m_pTarget=target;
    camera->SetPosition(eye);camera->SetOrientation(orientation);
    follow->m_vLastCameraPosition=eye;follow->m_vLastTargetPosition=follow->GetTargetHeadPosition();
    fprintf(stderr,"K1_RIG_CAMERA_REBOUND recreated=%d target_live=%d\n",behavior?0:1,follow->m_pTargetObject.m_pObject!=nullptr);
    return true;
}
bool RebuildOwnedRig(CSWCCreature* actor,bool engage) {
    if(!actor || !actor->m_pCreatureAppearance) return false;
    Gob* old=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
    if(engage && rigBodyRequested && rigBodyOwner==actor && IsSourceBody(old) && old->m_pModel && !_stricmp(old->m_pModel->m_szName,PlayerRigResource())) return true;
    if(engage) {
        if(!old || !old->m_pModel || (!IsSourceBody(old) && !WearableRig(old->m_pModel->m_szName) && _stricmp(old->m_pModel->m_szName,"pmbbl"))) return false;
        if(old->m_pAddInModel && old!=loadedBody) return false;
        if(!g_pExoResMan->Exists(CResRef(PlayerRigResource()),RESTYPE_MDL,static_cast<unsigned int*>(nullptr)) ||
           !g_pExoResMan->Exists(CResRef(PlayerRigResource()),RESTYPE_MDX,static_cast<unsigned int*>(nullptr))) {
            fprintf(stderr,"K1_ENGAGE_RIG_UNAVAILABLE missing_local_candidate\n");return false;
        }
    }
    auto* cameraModule=g_pAppManager->m_pClientExoApp->GetModule();
    auto* camera=cameraModule?cameraModule->m_pCamera:nullptr;
    auto* cameraBehavior=camera?camera->GetBehavior(-1):nullptr;
    const int cameraType=cameraBehavior?cameraBehavior->GetType():-1;
    const Vector cameraEye=camera?camera->GetPosition():Vector();
    const Quaternion cameraOrientation=camera?camera->GetOrientation():Quaternion();
    CCAppearanceInfo appearance=LiveAppearance(actor);
    CSWCCreatureAppearance* cached=actor->m_pCreatureAppearance;
    CResRef expected,texture,stored,storedTexture;
    NativePlayerModel(actor,appearance,expected,&texture);
    NativePlayerModel(actor,actor->m_appearanceInfo,stored,&storedTexture);
    fprintf(stderr,"K1_ENGAGE_RIG_APPEARANCE live=%s cached=%s stored=%s texture=%s\n",old&&old->m_pModel?old->m_pModel->m_szName:"none",expected.GetResRefStr(),stored.GetResRefStr(),texture.GetResRefStr());
    if(engage && !WearableRig(expected.GetResRefStr()) && _stricmp(expected.GetResRefStr(),"pmbbl")) return false;
    if(old==loadedBody && old) ReleaseAnimations(old);
    utilityBlend = {};
    // Native appearance replacement destroys the old Part tree. Drop identities first.
    loadedBody=nullptr;nativeIdlePose.clear();nativeStaffGuardPose.clear();nativeDualGuardPose.clear();lastPose.clear();blendFrom.clear();rotationFrom.clear();blendPending=false;
    currentBase=currentOverlay=nullptr;poseProbeBody=nullptr;poseProbeReference=nullptr;
    rigBodyRequested=engage;rigBodyOwner=engage?actor:nullptr;
    rigBodyActor=engage&&actor->GetServerCreature()?actor->GetServerCreature()->m_objectId:OBJECT_INVALID;
    auto rebuild=[&]() {
        actor->m_pCreatureAppearance->m_nAppearanceId=0xffff;
        int result=actor->SetAppearance(&appearance,0x0f,1,0);
        CSWCModule* module=g_pAppManager->m_pClientExoApp->GetModule();
        if(module && module->m_pArea) actor->AttachToScene(module->m_pArea->m_pScene);
        return result!=0;
    };
    bool ok=rebuild();Gob* fresh=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
    ok=ok && fresh && (engage?IsSourceBody(fresh):!IsSourceBody(fresh));
    if(!ok && engage) {
        rigBodyRequested=false;rigBodyOwner=nullptr;rigBodyActor=OBJECT_INVALID;
        bool rollback=rebuild();fresh=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        fprintf(stderr,"K1_ENGAGE_RIG_ROLLBACK result=%d\n",rollback);
    }
    if(fresh)RebindRigFollowCamera(actor,cameraModule,camera,cameraType,cameraEye,cameraOrientation);
    fprintf(stderr,"K1_ENGAGE_RIG_%s requested=%d actor=%llu model=%s\n",ok?"REBUILT":"FAILED",engage,actor->GetServerCreature()?actor->GetServerCreature()->m_objectId:OBJECT_INVALID,
        fresh&&fresh->m_pModel?fresh->m_pModel->m_szName:"none");
    return ok;
}
void Plan(bool retainPose = false) {
    for(auto& entry:meleePoise)ClearClashEpisode(entry.first);
    dualGuardAfterNative=false;
    if (state.mode == K1EngageState::Mode::Exploration) return;
    if(K1EngageEngaged()) nativeEdgeAwaitInterior=true;
    unsigned long long previous=rigBodyRequested?rigBodyActor:actorId;
    CGameObject* old = previous != OBJECT_INVALID ? g_pAppManager->m_pClientExoApp->GetGameObject(previous) : nullptr;
    CSWCCreature* actor = old ? old->AsSWCCreature() : Actor();
    Gob* body = actor ? actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY, 1) : nullptr;
    retainedPlanningPose = retainPose && body && body == loadedBody && currentBase &&
        K1EngageOwnsAnimation(body);
    const bool finishingSwing = retainedPlanningPose && state.attacking;
    if (!retainedPlanningPose) {
        if(rigBodyRequested && rigBodyOwner==actor) RebuildOwnedRig(actor,false);
        else if (body && body == loadedBody) ReleaseAnimations(body);
        rigBodyRequested=false;rigBodyOwner=nullptr;rigBodyActor=OBJECT_INVALID;
        loadedBody = nullptr;
        nativeIdlePose.clear();nativeStaffGuardPose.clear();nativeDualGuardPose.clear();
        legsYaw=0;
    }
    state.Plan();
    for(auto& entry:meleePoise) entry.second.blockReaction=-1;
    state.attacking = finishingSwing;
    if (!finishingSwing) currentAttack = nullptr;
    hadBlade = false;resumeFrames=0;
    keyW=keyS=keyA=keyD=keyShift=false;movement.Clear();
    g_bMouseCameraRotateToggle = 0; g_bMouseCameraRotateToggleIos = 0; g_bLeftMouseDown = 0;
    fprintf(stderr, "K1_ENGAGE mode=planning\n");
}
bool LoadAnimations(Gob* body) {
    if(IsSourceBody(body)) {
        if(loadedBody==body) return true;
        playerPlayback.nativeGait={};
        if(body->m_pAddInModel) return false;
        body->EndLookAtAnimate();body->PlayAnimation(nullptr,1,0,0);body->Reset();
        CacheNativeIdle(body,PlayerNativeResource(body));
        if(StaffMode()) CacheNativeIdle(body,PlayerNativeResource(body),&nativeStaffGuardPose,"g3r1");
        if(DualMode()) {
            const auto ready=Actor()->GetAnimationBase()->GetAnimationName(CSWCAnimBase::CLIENT_ANIMATION_DUAL_SABER_READY);
            CacheNativeIdle(body,PlayerNativeResource(body),&nativeDualGuardPose,ready.CStr());
            fprintf(stderr,"K1_DUAL_NATIVE_READY clip=%s class=%d\n",ready.CStr(),Actor()->GetAnimationBase()->m_nWeaponClass);
        }
        loadedBody=body;lastPose.clear();blendFrom.clear();rotationFrom.clear();blendPending=false;blendTime=.1f;
        playerPlayback.stanceTransition=false;
        currentBase=currentOverlay=nullptr;
        fprintf(stderr,"K1_ENGAGE model=%s source_rig=1\n",body->m_pModel->m_szName);return true;
    }
    if (loadedBody == body && body->m_pAddInModel) return true;
    // This calibration is for the isolated fixture's actual native bind skeleton.
    if (strcmp(body->m_pModel->m_szName,"PMBBL") && strcmp(body->m_pModel->m_szName,"pmbbl")) return false;
    // Do not replace another script's add-in or leak its reference.
    if (body->m_pAddInModel) return false;
    if (!body->LoadAddInAnimations(const_cast<char*>("k1yellow"))) return false;
    fprintf(stderr,"K1_ENGAGE model=%s\n",body->m_pModel->m_szName);
    // K1's normal add-in loader preserves IDs. Use its name-to-ID service on this actor.
    body->m_pAddInModel->SynchronizeTree(body->m_pModel);
    for (int i = 0; i < body->m_pAddInModel->m_animations.m_count; ++i)
        body->m_pAddInModel->m_animations.m_pData[i]->SynchronizeTree(body->m_pModel);
    loadedBody = nullptr;
    body->EndLookAtAnimate();
    body->PlayAnimation(nullptr,1,0,0);
    body->Reset();
    CacheNativeIdle(body);
    loadedBody = body;
    lastPose.clear();blendFrom.clear();blendPending=false;blendTime=.15f;
    currentBase=currentOverlay=nullptr;
    return true;
}
void Overlay(Gob* body, const char* name, bool once) {
    float time, length;
    if (!body->GetAnimation(const_cast<char*>(name), &time, &length))
        body->PlayAnimation(const_cast<char*>(name), 1,
            AnimRun::kAnimOverlay | AnimRun::kAnimNoBlendIn | (once ? AnimRun::kAnimOnce : 0), 0);
}
const char* Direction() {
    return attacks[requestedDirection];
}
Vector PartPointWorld(Part* part,Vector point) {
    Vector p;Quaternion q;part->GetWorld(p,q);float scale=part->m_pGob?part->m_pGob->m_fObjectScale:1.f;
    for(Part* current=part;current;current=current->m_pParent) scale*=current->m_scale.m_fScale;
    return p+(q.xdir()*point.x+q.ydir()*point.y+q.zdir()*point.z)*scale;
}
Vector SwordWorldPoint(Gob* weapon,float z) {
    return weapon->m_vPosition+weapon->m_qOrientation.zdir()*(z*weapon->m_fObjectScale);
}
bool SolidSwordBlades(Gob* weapon,std::vector<K1SaberContact::Blade>& blades) {
    const auto* geometry=SwordModelGeometry(weapon);
    if(!geometry)return false;
    blades.push_back({SwordWorldPoint(weapon,geometry->gripHigh),SwordWorldPoint(weapon,geometry->tipHigh)});
    if(geometry->doubleBlade)blades.push_back({SwordWorldPoint(weapon,geometry->gripLow),SwordWorldPoint(weapon,geometry->tipLow)});
    return true;
}
bool Blade(CSWCCreature* actor, Vector& base, Vector& tip) {
    if(NativeNpcBlunt(actor)) {
        K1SaberContact::Blade blade;if(!NativeNpcSolidBlade(actor,blade))return false;
        base=blade.base;tip=blade.tip;return true;
    }
    CSWCItem* item=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[4]);
    if(!item || (!item->m_bPowered && !SwordType(item->m_baseItem))) return false;
    Gob* weapon = actor->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY, 1);
    if(!weapon || !weapon->m_pPart) return false;
    if(SwordType(item->m_baseItem)) {
        std::vector<K1SaberContact::Blade> blades;
        if(!SolidSwordBlades(weapon,blades))return false;
        base=blades[0].base;tip=blades[0].tip;return true;
    }
    auto find=[&](auto&& self,Part* p)->bool {
        if(auto* node=p->m_pNode->AsMdlNodeLightsaber()) if(p->m_bDrawEnabled && node->m_paPositions) {
            // Native cap ordinals 1/2 delimit the central blade, excluding the
            // soft glow caps (0/3) and the wider luminous ribbon (4..7).
            auto a=node->m_paPositions[1],b=node->m_paPositions[2];
            base=PartPointWorld(p,Vector(a.x,a.y,a.z));tip=PartPointWorld(p,Vector(b.x,b.y,b.z));
            if(magnitude(tip-base)>.0001f) return true;
        }
        for(int i=0;i<p->m_nChildren;++i) if(self(self,p->m_paChildren[i])) return true;
        return false;
    };
    return find(find,weapon->m_pPart);
}
float NominalBladeLength(CSWCCreature* actor) {
    if(NativeNpcBlunt(actor)) {
        K1SaberContact::Blade blade;return NativeNpcSolidBlade(actor,blade)?magnitude(blade.tip-blade.base):0;
    }
    Gob* weapon=actor->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
    if(!weapon || !weapon->m_pPart) return 0;
    if(const auto* geometry=SwordModelGeometry(weapon))return std::max(geometry->tipHigh-geometry->gripHigh,
        geometry->doubleBlade?geometry->gripLow-geometry->tipLow:0.f)*weapon->m_fObjectScale;
    float length=0;
    auto visit=[&](auto&& self,Part* p)->void {
        if(auto* node=p->m_pNode->AsMdlNodeLightsaber()) if(node->m_paPositions) {
            const auto a=node->m_paPositions[1],b=node->m_paPositions[2];
            length=std::max(length,magnitude(Vector(b.x-a.x,b.y-a.y,b.z-a.z))*weapon->m_fObjectScale);
        }
        for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
    };visit(visit,weapon->m_pPart);
    return length;
}
void CollisionPartTriangles(Part* root,std::vector<K1SaberContact::Triangle>& triangles,bool staticRoom=false,std::vector<Part*>* parts=nullptr) {
    if(!root) return;
    auto collect=[&](auto&& self,Part* p)->void {
        if(p->AsPartTriMesh() && (p->m_bDrawEnabled || staticRoom) && !p->m_pNode->AsMdlNodeLightsaber()) {
            auto* node=static_cast<MdlNodeTriMesh*>(p->m_pNode);
            if(node->m_bRender && node->m_nVertexCount && node->m_paVertices) {
                List<Vector> vertices;bool valid=true;
                if(auto* skin=p->AsPartSkin()) valid=skin->GetSkinnedWorldVertices(vertices);
                else for(unsigned int i=0;i<node->m_nVertexCount;++i)
                    vertices.Add(PartPointWorld(p,Vector(node->m_paVertices[i*3],node->m_paVertices[i*3+1],node->m_paVertices[i*3+2])));
                if(valid) for(int i=0;i<node->m_faces.m_count;++i) {
                    const auto& f=node->m_faces.m_pData[i];int a=f.m_anVertIndices[0],b=f.m_anVertIndices[1],c=f.m_anVertIndices[2];
                    if(a>=0 && b>=0 && c>=0 && a<vertices.m_count && b<vertices.m_count && c<vertices.m_count &&
                        K1SaberContact::Finite(vertices.m_pData[a]) && K1SaberContact::Finite(vertices.m_pData[b]) && K1SaberContact::Finite(vertices.m_pData[c]))
                        {triangles.push_back({vertices.m_pData[a],vertices.m_pData[b],vertices.m_pData[c]});if(parts)parts->push_back(p);}
                }
            }
        }
        for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
    };collect(collect,root);
}
void CollisionTriangles(Gob* model,std::vector<K1SaberContact::Triangle>& triangles,std::vector<Part*>* parts=nullptr) {
    if(model) CollisionPartTriangles(model->m_pPart,triangles,false,parts);
}
bool NativeNpcSolidBlade(CSWCCreature* actor,K1SaberContact::Blade& blade,int hand) {
    auto* weapon=actor->GetModel(hand?CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY:CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
    if(!weapon && !hand) {
        // Native Gamorreans carry the axe as body geometry rather than a
        // separate wielded item model. Use that actual posed mesh and hook.
        auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        auto* mesh=body?body->GetPart(const_cast<char*>("waraxe_001")):nullptr;
        auto* hook=body?body->GetPart(const_cast<char*>("weapondummy")):nullptr;
        auto* item=NativeNpcRightItem(actor->GetServerCreature());
        if(mesh && hook && item && item->m_baseItem==BASE_ITEM_GAMMOREAN_BATTLEAXE) {
            std::vector<K1SaberContact::Triangle> triangles;CollisionPartTriangles(mesh,triangles);
            Vector base=PartPointWorld(hook,Vector()),axis=normalize(PartPointWorld(hook,Vector(0,0,1))-base);
            float extent=0;for(const auto& triangle:triangles)for(const auto& point:{triangle.a,triangle.b,triangle.c})extent=std::max(extent,dot(point-base,axis));
            if(extent>.0001f){blade={base,base+axis*extent};return true;}
        }
    }
    if(!weapon || !weapon->m_pPart){if(std::getenv("K1_NATIVE_NPC_TEST"))fprintf(stderr,"K1_NATIVE_WEAPON_MISSING actor=%llu hand=%d\n",actor->m_objectId,hand);return false;}
    // Native staff parts can telescope when powered. Measure the current drawn
    // geometry, not its retracted bind bounds or a substituted lightsaber length.
    std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(weapon,triangles);
    float extent=0;
    const Vector axis=normalize(weapon->m_qOrientation.zdir());
    for(const auto& triangle:triangles)for(const auto& point:{triangle.a,triangle.b,triangle.c})
        extent=std::max(extent,dot(point-weapon->m_vPosition,axis));
    if(extent<=.0001f){fprintf(stderr,"K1_NATIVE_WEAPON_EXTENT actor=%llu model=%s triangles=%zu extent=%g\n",actor->m_objectId,weapon->m_pModel?weapon->m_pModel->m_szName:"none",triangles.size(),extent);return false;}
    blade={weapon->m_vPosition,weapon->m_vPosition+axis*extent};return true;
}
bool ContactDamage(CSWSCreature* actor, CSWSCreature* target,bool checkGuard=true) {
    if(checkGuard && K1EngageGuardContact(actor->m_objectId,target->m_objectId)) return true;
    CSWSCombatRound* round = actor->m_pCombatRound;
    // Casts and queued item powers retain ownership of their native round.
    if (!round || round->m_bRoundStarted || round->m_bSpellCastRound) {
        fprintf(stderr,"K1_MELEE_DAMAGE_REJECT actor=%llu target=%llu native_round=%d spell_round=%d\n",actor->m_objectId,target->m_objectId,round?round->m_bRoundStarted:-1,round?round->m_bSpellCastRound:-1);
        return false;
    }
    ClearClashEpisode(actor->m_objectId);
    NotePoiseActivity(target,"hit");
    unsigned char oldAttack = round->m_currentAttack;
    int oldCount = round->m_nOnHandAttacks;
    int oldOffhand = round->GetOffHandAttack();
    const bool dual=damageContact && damageContact->dualWeapon!=OBJECT_INVALID;
    const bool offhand=dual && damageContact->offhand;
    round->m_currentAttack = 0; round->m_nOnHandAttacks = 1; round->SetOffHandAttack(offhand?1:0);
    CSWSCombatAttackData* attack = round->GetAttack(0);
    attack->ClearAttackData(); attack->m_targetObjectId = target->m_objectId;
    attack->m_nAttackResult = 1; attack->m_attack.m_bAutomaticHit = 1;
    attack->m_nWeaponAttackType = static_cast<unsigned char>(round->GetWeaponAttackType());
    actor->ResolveDamage(target, 0, 0);
    actor->ResolvePostMeleeDamage(target);
    int damage = attack->GetBaseDamage();
    if(dual) fprintf(stderr,"K1_DUAL_NATIVE_DAMAGE hand=%d weapon=%llu damage=%d native_offhand=%d\n",offhand?1:0,damageContact->dualWeapon,damage,round->GetOffHandAttack());
    if(StaffMode() && actor==ServerActor()) {
        auto* item=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
        auto* base=item?g_pRules->GetBaseItem(item->m_baseItem):nullptr;
        if(base) fprintf(stderr,"K1_STAFF_NATIVE_DAMAGE item=%d dice=%d die=%d strength=%d offhand=0 damage=%d automatic_hit=1\n",item->m_baseItem,base->m_nNumDice,base->m_nDieToRoll,actor->m_pStats->GetSTRStat(),damage);
    }

    actor->SignalMeleeDamage(target->m_objectId, 1);
    EngageWeaponSound(actor,target,"hit");
    round->m_currentAttack = oldAttack; round->m_nOnHandAttacks = oldCount; round->SetOffHandAttack(oldOffhand);
    fprintf(stderr, "K1_ENGAGE contact attacker=%llu target=%llu damage=%d roll=none\n", actor->m_objectId, target->m_objectId, damage);
    return true;
}
#include "include/k1contactmotion.h"
std::vector<K1SaberContact::Blade> StaffBlades(CSWCCreature* actor) {
    std::vector<K1SaberContact::Blade> blades;
    if(NativeNpcBlunt(actor)) {
        K1SaberContact::Blade blade;if(NativeNpcSolidBlade(actor,blade))blades.push_back(blade);
        return blades;
    }
    auto* item=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[4]);
    auto* weapon=actor->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
    if(!item || (!item->m_bPowered && !SwordType(item->m_baseItem)) || !weapon || !weapon->m_pPart) return blades;
    if(SwordType(item->m_baseItem)){SolidSwordBlades(weapon,blades);return blades;}
    auto visit=[&](auto&& self,Part* part)->void {
        if(auto* node=part->m_pNode->AsMdlNodeLightsaber()) if(part->m_bDrawEnabled && node->m_paPositions) {
            auto a=node->m_paPositions[1],b=node->m_paPositions[2];
            K1SaberContact::Blade blade={PartPointWorld(part,Vector(a.x,a.y,a.z)),PartPointWorld(part,Vector(b.x,b.y,b.z))};
            bool duplicate=false;
            for(const auto& prior:blades) if(magnitude(prior.base-blade.base)<.0001f && magnitude(prior.tip-blade.tip)<.0001f) duplicate=true;
            if(!duplicate && magnitude(blade.tip-blade.base)>.0001f) blades.push_back(blade);
        }
        for(int i=0;i<part->m_nChildren;++i) self(self,part->m_paChildren[i]);
    };visit(visit,weapon->m_pPart);
    return blades;
}
void TraceStaffBlades(CSWCCreature* actor,float time) {
    static std::vector<K1SaberContact::Blade> previous;
    auto current=StaffBlades(actor);
    if(current.empty()) {hadBlade=false;return;}
    unsigned long long weapon=actor->m_aEquippedItemIds[4];
    if(staffContactSerial!=attackSerial || staffContactWeapon!=weapon) {
        staffContactSerial=attackSerial;staffContactWeapon=weapon;staffContactBlades=0;
    }
    bool reset=!hadBlade || weapon!=previousBladeWeapon || time<previousBladeTime || previous.size()!=current.size();
    if(!reset) for(size_t i=0;i<current.size();++i) reset=reset || magnitude(current[i].base-previous[i].base)>2.f;
    if(reset) {previous=current;previousBladeTime=time;}
    hadBlade=true;previousBladeWeapon=weapon;
    const float from=previousBladeTime,to=time,start=PlayerHitStart(swingDirection),end=StaffContactEnd(swingDirection);
    float lo=0,hi=1;
    const bool active=K1SaberContact::ClipTime(from,to,start,end,lo,hi);
    auto* server=actor->GetServerCreature();
    if(SwingSoundDue(attackSerial,playerPlayback.swingSoundSerial,time,start)) EngageWeaponSound(server,nullptr,"swing");
    auto* area=server?static_cast<CSWSArea*>(server->GetArea()):nullptr;
    struct StaffContact {MeleeContact contact;float phase,length;int blade,triangles;};
    std::vector<StaffContact> found;
    for(size_t blade=0;blade<current.size();++blade) {
        if(staffContactBlades&(1u<<blade)) continue;
        CSWSCreature* first=nullptr;float firstPhase=2;int triangleCount=0;MeleeContact selected={};
        Vector base=current[blade].base,tip=current[blade].tip;
        auto begin=K1SaberContact::Between(previous[blade],current[blade],lo),finish=K1SaberContact::Between(previous[blade],current[blade],hi);
    if (active && lo<=hi && area) for (int i = 0; i < area->m_aObjectsByX.m_count; ++i) {
        CSWSCreature* target = g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);
        if (!target || target == server || target->GetDead() || target->GetReputation(server->m_objectId, 0) > 10) continue;
        unsigned long long clientId = g_pAppManager->m_pClientExoApp->ServerToClientObjectId(target->m_objectId);
        CGameObject* object = g_pAppManager->m_pClientExoApp->GetGameObject(clientId);
        CSWCCreature* client = object ? object->AsSWCCreature() : nullptr;
        Gob* model = client ? client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY, 1) : nullptr;
        if (!model || magnitude(model->m_vPosition-server->m_position)>10.f+model->m_fMinSphereRadius) continue;
        std::vector<K1SaberContact::Triangle> triangles;std::vector<Part*> parts;CollisionTriangles(model,triangles,&parts);
        CollisionTriangles(client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1),triangles,&parts);
        const bool targetCasting=target->m_pCombatRound && target->m_pCombatRound->m_bSpellCastRound;
        if(NativeStatusAnimation(target) || targetCasting) {
            static int serial=-1;static unsigned long long traced=OBJECT_INVALID;
            if(serial!=attackSerial || traced!=target->m_objectId) {
                serial=attackSerial;traced=target->m_objectId;
                fprintf(stderr,"K1_FORCE_TARGET_COLLISION actor=%llu serial=%d target=%llu state=%d casting=%d model=%s triangles=%zu\n",server->m_objectId,attackSerial,target->m_objectId,target->m_nState,targetCasting,model->m_pModel?model->m_pModel->m_szName:"none",triangles.size());
            }
        }
        const auto relative=RelativeContactStart(target->m_objectId,previous[blade],!reset);
        begin=K1SaberContact::Between(relative,current[blade],lo);finish=K1SaberContact::Between(relative,current[blade],hi);
        int steps=K1SaberContact::Steps(begin,finish);bool hit=false;
        for(int step=0;step<steps && !hit;++step) {
            float a=float(step)/steps,b=float(step+1)/steps;
            auto x=K1SaberContact::Between(begin,finish,a),y=K1SaberContact::Between(begin,finish,b);
            for(size_t ti=0;ti<triangles.size();++ti) if(K1SaberContact::SweepCell(x,y,triangles[ti])) {
                if(a<firstPhase) {
                    firstPhase=a;first=target;triangleCount=int(triangles.size());
                    selected={server->m_objectId,target->m_objectId,attackSerial};selected.blade=int(blade);
                    if(K1SaberContact::SweepPoint(x,y,triangles[ti],selected.hitPoint)) {
                        selected.hitPart=parts[ti];selected.bladeDirection=y.tip-y.base;
                        const float fraction=std::max(0.f,std::min(1.f,dot(selected.hitPoint-y.base,y.tip-y.base)/dot(y.tip-y.base,y.tip-y.base)));
                        const auto wx=K1SaberContact::Between(previous[blade],current[blade],lo+(hi-lo)*(a)),wy=K1SaberContact::Between(previous[blade],current[blade],lo+(hi-lo)*(b));
                        selected.bladeDirection=wy.tip-wy.base;
                        selected.bladeMotion=(wy.base-wx.base)*(1-fraction)+(wy.tip-wx.tip)*fraction;
                    }
                }
                hit=true;break;
            }
        }
    }
        if(first) found.push_back({selected,firstPhase,magnitude(tip-base),int(blade),triangleCount});
    }
    std::stable_sort(found.begin(),found.end(),[](const StaffContact& a,const StaffContact& b){return a.phase<b.phase;});
    for(auto& hit:found) {
        auto& contact=hit.contact;contact.poiseOnly=hitConsumed;
        staffContactBlades|=1u<<hit.blade;
        auto* target=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(contact.target);
        if(!contact.poiseOnly) contact.dismemberRow=SampleDismemberment(server,target,contact);
        contact.hitPart=nullptr;
        sceneMeleeContacts.push_back(contact);hitConsumed=true;
        fprintf(stderr,"K1_ENGAGE_BLADE_CONTACT serial=%d time=%g phase=%g length=%g triangles=%d pose=%llu\n",attackSerial,time,hit.phase,hit.length,hit.triangles,poseRevision);
        fprintf(stderr,"K1_STAFF_BLADE_CONTACT serial=%d blade=%d count=%zu time=%g poise_only=%d\n",attackSerial,hit.blade,current.size(),time,contact.poiseOnly);
    }
    previous=current;previousBladeTime=time;
}

#include "include/k1dualstance.h"
struct VisualBlade {K1SaberContact::Blade blade;int hand=0;};
std::vector<VisualBlade> PoweredSaberBlades(CSWCCreature* actor) {
    std::vector<VisualBlade> result;if(!actor)return result;
    auto* server=actor->GetServerCreature();if(!server || !server->m_pInventory)return result;
    for(int hand=0;hand<2;++hand) {
        auto* native=server->m_pInventory->GetItemInSlot(hand?CSWInventory::SLOT_LEFTWEAPON:CSWInventory::SLOT_RIGHTWEAPON);
        auto* client=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[4+hand]);
        if(!native || !native->IsLightsaber() || !client || !client->m_bPowered)continue;
        if(!hand)for(const auto& blade:StaffBlades(actor))result.push_back({blade,hand});
        else {K1SaberContact::Blade blade;if(DualBlade(actor,hand,blade))result.push_back({blade,hand});}
    }
    return result;
}
void SaberContactVisual(CSWSCreature* actor,CSWSCreature* target,const char* kind) {
    auto* a=actor?actor->GetClientCreature():nullptr;auto* b=target?target->GetClientCreature():nullptr;
    if(!a || !b || !a->m_pArea || a->m_pArea!=b->m_pArea)return;
    auto x=PoweredSaberBlades(a),y=PoweredSaberBlades(b);
    if(x.empty() && y.empty())return;
    // Include the opposing solid weapon for saber-versus-sword blocks.
    auto fallback=[](CSWCCreature* c,std::vector<VisualBlade>& out){Vector p,q;if(out.empty() && Blade(c,p,q))out.push_back({{p,q},0});};
    fallback(a,x);fallback(b,y);if(x.empty() || y.empty())return;
    Vector point;float nearest=1e30f;
    for(const auto& u:x)for(const auto& v:y) {
        // Closest points on the two currently drawn finite blade segments.
        Vector d=u.blade.tip-u.blade.base,e=v.blade.tip-v.blade.base,r=u.blade.base-v.blade.base;
        float aa=dot(d,d),bb=dot(d,e),cc=dot(e,e),dd=dot(d,r),ee=dot(e,r);
        if(aa<1e-10f || cc<1e-10f)continue;
        float den=aa*cc-bb*bb,s=den>1e-10f?std::max(0.f,std::min(1.f,(bb*ee-cc*dd)/den)):0.f;
        float t=std::max(0.f,std::min(1.f,(bb*s+ee)/cc));s=std::max(0.f,std::min(1.f,(bb*t-dd)/aa));
        Vector p=u.blade.base+d*s,q=v.blade.base+e*t;float gap=magnitude(p-q);
        if(gap<nearest){nearest=gap;point=(p+q)*.5f;}
    }
    if(nearest==1e30f || !K1SaberContact::Finite(point))return;
    auto* effect=new CSWCVisualEffect(1);
    if(!effect->LoadEffect(VFX_COM_SPARKS_PARRY_METAL)){delete effect;return;}
    int lights=0,emitters=0;
    if(auto* model=effect->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,0)) {
        auto visit=[&](auto&& self,Part* p)->void {
            lights+=p->m_pNode->AsMdlNodeLight()!=nullptr;emitters+=p->m_pNode->AsMdlNodeEmitter()!=nullptr;
            for(int i=0;i<p->m_nChildren;++i)self(self,p->m_paChildren[i]);
        };visit(visit,model->m_pPart);
    }
    effect->AttachToScene(a->m_pArea->m_pScene);effect->m_bImpactOnly=1;effect->m_bDeleteWhenFinished=1;
    effect->SetPosition(point);effect->SetAnimation(1);
    // This is K1's HitContactEvent/ShowLightSaberContactVisual payload:
    // v_parrspr_imp contains two spark emitters and its authored animated light.
    fprintf(stderr,"K1_SABER_CONTACT_VFX kind=%s effect=4011 lights=%d emitters=%d gap=%g position=%g,%g,%g\n",kind,lights,emitters,nearest,point.x,point.y,point.z);
}

struct PlayerHitAudit {int raw=0,active=0,queued=0,damaged=0,blocked=0,clashed=0,rejected=0,dropped=0;float first=1e9f,last=-1;};
std::unordered_map<int,PlayerHitAudit> playerHitAudits;
void TraceBlade(CSWCCreature* actor,float time) {
    if(DualMode()) {TraceDualBlades(actor,time);return;}
    if(StaffMode()) {TraceStaffBlades(actor,time);return;}
    Vector base, tip;
    if (!Blade(actor, base, tip)) {hadBlade=false;return;}
    unsigned long long weapon=actor->m_aEquippedItemIds[4];
    const bool reset=!hadBlade || weapon!=previousBladeWeapon || time<previousBladeTime || magnitude(base-previousBase)>2.f;
    if (reset) {
        previousBase=base;previousTip=tip;previousBladeTime=time;
    }
    hadBlade=true;previousBladeWeapon=weapon;
    float from=previousBladeTime,to=time;
    float start=PlayerHitStart(swingDirection),end=ContactEnd(PlayerHitEnd(swingDirection));
    float lo=0,hi=1;
    bool active=K1SaberContact::ClipTime(from,to,start,end,lo,hi);
    K1SaberContact::Blade previous={previousBase,previousTip},current={base,tip};
    auto begin=K1SaberContact::Between(previous,current,lo),finish=K1SaberContact::Between(previous,current,hi);
    CSWSCreature* server = actor->GetServerCreature();
    if(SwingSoundDue(attackSerial,playerPlayback.swingSoundSerial,time,start)) EngageWeaponSound(server,nullptr,"swing");
    CSWSArea* area = server ? static_cast<CSWSArea*>(server->GetArea()) : nullptr;
    CSWSCreature* first=nullptr;float firstPhase=2;int triangleCount=0;MeleeContact selected={};
    const bool audit=std::getenv("K1_ENGAGE_MELEE_HIT_TEST")!=nullptr;
    if (area && ((active && lo<=hi && !hitConsumed) || audit)) for (int i = 0; i < area->m_aObjectsByX.m_count; ++i) {
        CSWSCreature* target = g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);
        if (!target || target == server || target->GetDead() || target->GetReputation(server->m_objectId, 0) > 10) continue;
        unsigned long long clientId = g_pAppManager->m_pClientExoApp->ServerToClientObjectId(target->m_objectId);
        CGameObject* object = g_pAppManager->m_pClientExoApp->GetGameObject(clientId);
        CSWCCreature* client = object ? object->AsSWCCreature() : nullptr;
        Gob* model = client ? client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY, 1) : nullptr;
        if (!model || magnitude(model->m_vPosition-server->m_position)>10.f+model->m_fMinSphereRadius) continue;
        std::vector<K1SaberContact::Triangle> triangles;std::vector<Part*> parts;CollisionTriangles(model,triangles,&parts);
        CollisionTriangles(client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1),triangles,&parts);
        const auto relative=RelativeContactStart(target->m_objectId,previous,!reset);
        begin=K1SaberContact::Between(relative,current,lo);finish=K1SaberContact::Between(relative,current,hi);
        if(audit) {
            bool raw=false;const int n=K1SaberContact::Steps(relative,current);
            for(int j=0;j<n && !raw;++j)for(const auto& t:triangles)
                if(K1SaberContact::SweepCell(K1SaberContact::Between(relative,current,float(j)/n),K1SaberContact::Between(previous,current,float(j+1)/n),t)){raw=true;break;}
            if(raw){auto& a=playerHitAudits[attackSerial];++a.raw;a.first=std::min(a.first,time);a.last=std::max(a.last,time);}
        }
        if(!active || lo>hi || hitConsumed)continue;
        int steps=K1SaberContact::Steps(begin,finish);bool hit=false;
        for(int step=0;step<steps && !hit;++step) {
            float a=float(step)/steps,b=float(step+1)/steps;
            auto x=K1SaberContact::Between(begin,finish,a),y=K1SaberContact::Between(begin,finish,b);
            for(size_t ti=0;ti<triangles.size();++ti) if(K1SaberContact::SweepCell(x,y,triangles[ti])) {
                if(a<firstPhase) {
                    firstPhase=a;first=target;triangleCount=int(triangles.size());
                    selected={server->m_objectId,target->m_objectId,attackSerial};
                    if(K1SaberContact::SweepPoint(x,y,triangles[ti],selected.hitPoint)) {
                        selected.hitPart=parts[ti];selected.bladeDirection=y.tip-y.base;
                        const float fraction=std::max(0.f,std::min(1.f,dot(selected.hitPoint-y.base,y.tip-y.base)/dot(y.tip-y.base,y.tip-y.base)));
                        const auto wx=K1SaberContact::Between(previous,current,lo+(hi-lo)*(a)),wy=K1SaberContact::Between(previous,current,lo+(hi-lo)*(b));
                        selected.bladeDirection=wy.tip-wy.base;
                        selected.bladeMotion=(wy.base-wx.base)*(1-fraction)+(wy.tip-wx.tip)*fraction;
                    }
                }
                if(audit)++playerHitAudits[attackSerial].active;
                hit=true;break;
            }
        }
    }
    if(first) {
        if(audit)++playerHitAudits[attackSerial].queued;
        if(RedMode())fprintf(stderr,"K1_RED_CONTACT_WINDOW serial=%d clip=%s time=%g start=%g end=%g\n",attackSerial,PlayerAttackClip(false),time,start,end);
        selected.dismemberRow=SampleDismemberment(server,first,selected);selected.hitPart=nullptr;
        sceneMeleeContacts.push_back(selected);
        hitConsumed=true;
        fprintf(stderr,"K1_ENGAGE_BLADE_CONTACT serial=%d time=%g phase=%g length=%g triangles=%d pose=%llu\n",attackSerial,time,firstPhase,magnitude(tip-base),triangleCount,poseRevision);
    }
    previousBase=base;previousTip=tip;previousBladeTime=time;
}
} // namespace

bool K1EngageSandboxBlocksPlayerTarget(unsigned long long attacker,unsigned long long target) {
    return sandboxSession && !sandboxTargetPlayer && sandboxActors.count(attacker) &&
        ServerActor() && ServerActor()->m_objectId==target;
}
bool K1EngageSandboxAppearance(CSWCCreature* actor,CCAppearanceInfo* appearance) {
    auto* server=actor?actor->GetServerCreature():nullptr;
    if(!server)return false;
    auto found=sandboxAppearances.find(server->m_objectId);if(found==sandboxAppearances.end())return false;
    auto headItem=appearance->core.m_oidHeadItem;
    *appearance=found->second;appearance->core.m_oidHeadItem=headItem;
    return true;
}
bool K1EngageCloseSandboxMenu() {
    if(!sandboxMenuOpen)return false;
    SandboxToggle();return true;
}
bool K1EngageEnabled() {
    static bool enabled = [] { const char* value = std::getenv("K1_ENGAGE_PROTOTYPE"); return value && strcmp(value, "1") == 0; }();
    return enabled;
}
bool K1EngageUseBodyRig(CSWCCreature* actor,const CResRef& original) {
    if(!RigModeEnabled() || !rigBodyRequested || !actor || actor!=rigBodyOwner) return false;
    const char* name=original.GetResRefStr();return WearableRig(name) || !_stricmp(name,"pmbbl");
}
const char* K1EngageBodyRigResource(CSWCCreature* actor,const CResRef& original) {
    for(auto& entry:npcPlayback) {
        auto& npc=entry.second;
        if(npc.requested && npc.owner==actor) {
            const char* resource=NpcRigResource(original.GetResRefStr(),actor->GetServerCreature());
            if(resource)npc.nativeResource=original.GetResRefStr();
            return resource;
        }
    }
    if(!K1EngageUseBodyRig(actor,original))return nullptr;
    const char* resource=PlayerRigForNative(original.GetResRefStr());
    return resource;
}
void K1EngageBeforeBodyUnload(CSWCCreature* actor) {
    if(!actor || actor!=rigBodyOwner || !actor->GetAnimationBase())return;
    auto* old=actor->GetAnimationBase()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
    // Native unload precedes the replacement LoadModel callback. Clear pointers
    // here, before the tree dies or its address can be reused. Internal stance
    // rebuilds already clear loadedBody and retain their existing blend path.
    if(old && old==loadedBody && IsSourceBody(old) && WearableNativeResource(old->m_pModel->m_szName)) {
        ReleaseAnimations(old);loadedBody=nullptr;playerPlayback={};
        nativeIdlePose.clear();nativeStaffGuardPose.clear();nativeDualGuardPose.clear();
        state.ClearInputs();state.attacking=false;state.chainSwings=0;++attackSerial;
        currentBase=currentOverlay=nullptr;hadBlade=false;ClearClashEpisode(actor->GetServerCreature()->m_objectId);
        sceneMeleeContacts.erase(std::remove_if(sceneMeleeContacts.begin(),sceneMeleeContacts.end(),[&](const MeleeContact& c){return c.attacker==actor->GetServerCreature()->m_objectId;}),sceneMeleeContacts.end());
        fprintf(stderr,"K1_ARMOR_NATIVE_REPLACE outgoing_cancelled=1\n");
    }
}
bool K1EngageOwnsEnemy(unsigned long long id) {
    const auto found=npcPlayback.find(id);
    return found!=npcPlayback.end() && found->second.body && found->second.requested;
}
bool K1EngageEnemyForceWalk(unsigned long long id) {
    const auto found=npcPlayback.find(id);
    return found!=npcPlayback.end() && NpcWalking(found->second);
}
bool K1EngageEnemyAttack(CSWSCreature* enemy,unsigned long long target) {
    if(!enemy || !K1EngageEnemyAIActive(ServerActor()?ServerActor()->m_objectId:OBJECT_INVALID) || SandboxIdle(enemy) || enemy->GetDead() || enemy->m_bInParty ||
       enemy->m_bPlayerCharacter || enemy->m_bPassiveAttackBehaviour) return false;
    auto* victim=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(target);
    if(!victim || enemy->GetArea()!=victim->GetArea() || enemy->GetReputation(target,0)>10) return false;
    if(!K1EngageOwnsPlayer(target) && !sandboxActors.count(enemy->m_objectId)) return false;
    if(K1EngageOwnsEnemy(enemy->m_objectId)) return true;
    // Hold native physical markers during the initial client/model handoff too.
    // The direct port is currently bound to this explicitly supported body.
    CExoString resource;
    g_pRules->m_p2DArrays->m_pAppearance->GetCExoStringEntry(enemy->m_appearance.m_headId,
        CTwoDimArrays::COL2DA_APPEARANCETABLE_RACE,&resource);
    if(NativeConfiguredNpc(enemy)) {
        auto* client=enemy->GetClientCreature();
        auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
        return body && body->m_pModel && !body->m_pAddInModel && NativeNpcMeleeEquipped(enemy) &&
            NpcRigResource(body->m_pModel->m_szName,enemy)!=nullptr;
    }
    return NpcRigResource(resource.CStr(),enemy)!=nullptr;
}
bool K1EngageEnemyMovementOwned(unsigned long long id) {
    if(K1EngageOwnsEnemy(id)) return true;
    auto* player=ServerActor();
    return player && K1EngageEnemyAttack(g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(id),player->m_objectId);
}
bool K1EngageEnemyNativeControl(CSWSCreature* enemy) {
    if(!NativeConfiguredNpc(enemy))return false;
    if(!NativeNpcMeleeEquipped(enemy))return true;
    if(K1EngageOwnsEnemy(enemy->m_objectId))return false;
    auto* client=enemy->GetClientCreature();
    auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
    return !body || !body->m_pModel || body->m_pAddInModel || !NpcRigResource(body->m_pModel->m_szName,enemy);
}
float K1EngageEnemyMeleeRange(CSWSCreature* enemy) {
    const auto found=enemy?npcPlayback.find(enemy->m_objectId):npcPlayback.end();
    if(found==npcPlayback.end() || !found->second.body) return 1.85f;
    const auto& npc=found->second;
    const float blade=NominalBladeLength(npc.owner);
    if(blade<=0) return 1.85f;
    auto* upper=npc.body->GetPart(const_cast<char*>("rbicep_g"));
    auto* elbow=npc.body->GetPart(const_cast<char*>("Rforearm_g"));
    auto* wrist=npc.body->GetPart(const_cast<char*>("Rhand_g"));
    if(!upper || !elbow || !wrist) return 1.85f;
    Vector a,b,c;Quaternion unused;BindFrame(upper,a,unused);BindFrame(elbow,b,unused);BindFrame(wrist,c,unused);
    return std::min(1.85f,(magnitude(b-a)+magnitude(c-b))*npc.body->m_fObjectScale+blade);
}
bool K1EngageActive() { return K1EngageEnabled() && state.mode == K1EngageState::Mode::Active && !IsGamePaused(); }
bool K1EngageEngaged() { return K1EngageEnabled() && state.mode == K1EngageState::Mode::Active; }
bool K1EngagePlanning() { return K1EngageEnabled() && state.mode == K1EngageState::Mode::Planning; }
bool K1EngageArenaLoadoutActive() {
    static const bool arena = [] {
        const char* value = std::getenv("K1_ENGAGE_ARENA");
        return value && !strcmp(value, "1");
    }();
    if (!arena || !K1EngageEnabled()) return false;
    auto* module = g_pAppManager->m_pClientExoApp->GetModule();
    return module && !strcmp(module->m_sModuleName.CStr(), "k1arena");
}
bool NpcCombatAreaActive() {
    auto* module=g_pAppManager->m_pClientExoApp->GetModule();
    return K1EngageArenaLoadoutActive() || (K1EngageEnabled() && std::getenv("K1_ENGAGE_STORY") && module && module->m_pArea);
}
bool K1EngageEnemyAIActive(unsigned long long target) {
    static const bool enabled = std::getenv("K1_ENGAGE_ENEMY_AI") != nullptr;
    return enabled && K1EngageEnabled() && !IsGamePaused() &&
        (K1EngageEngaged() || K1EngagePlanning()) && NpcCombatAreaActive() && K1EngageOwnsPlayer(target);
}
bool K1EngageEnemyCanReposition(unsigned long long enemy) {
    const auto found = enemyRepositionAfter.find(enemy);
    return found == enemyRepositionAfter.end() ||
        g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime() >= found->second;
}
void K1EngageEnemyRepositioned(unsigned long long enemy) {
    enemyRepositionAfter[enemy] = g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime() + 650000;
}
bool K1EngageEnemyFacesSwing(CSWSCreature* enemy) {
    if (!state.attacking || !loadedBody || !currentOverlay) return false;
    CSWSCreature* player = ServerActor();
    if (!player) return false;
    Vector offset = enemy->m_position - player->m_position;
    offset.z = 0.f;
    const float distance = magnitude(offset);
    if (distance < .001f || distance > 2.6f ||
        offset.x * std::cos(aimYaw) + offset.y * std::sin(aimYaw) < distance * .45f) return false;
    float time = 0.f, length = 0.f;
    return loadedBody->GetAnimation(const_cast<char*>(currentOverlay), &time, &length) &&
        time >= PlayerHitStart(swingDirection) - .15f && time <= PlayerHitEnd(swingDirection);
}
bool K1EngageForceWalk() { return K1EngageActive() && state.ForceWalk(); }
bool K1EngageMouse(bool left, bool down) {
    if(std::getenv("K1_ENGAGE_CONTROL_TEST")) fprintf(stderr,"K1_ENGAGE_RAW_MOUSE left=%d down=%d active=%d\n",left,down,K1EngageActive());
    if(!K1EngageActive()) return false;
    if(ContactTimingEnabled() && !left) {
        if(down){contactGuardClock={ContactWallUs(),g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime(),0,state.attacking,nativeActionBody};
            fprintf(stderr,"K1_CONTACT_GUARD_INPUT attacking=%d native=%d mode=%d\n",state.attacking,nativeActionBody,int(state.mode));}
        else contactGuardClock={};
    }
    if(nativeActionBody) {
        if((DualPrototype() || std::getenv("K1_ENGAGE_SANDBOX")) && !left)dualGuardAfterNative=down;
        return true;
    }
    if(left && down) {
        auto* actor=ServerActor();
        const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
        if(actor && !ParryAttackReady(actor->m_objectId,now)) {
            state.Mouse(left,down);state.attackRequested=state.holdChain=false;return true;
        }
    }
    if(left && down && !state.leftHeld) {
        if(!state.attacking) {
            state.holdChain=true;
            if(!state.attackRequested) QueueAttackDirection();
        } else if(state.chainSwings<PlayerChainLimit() && loadedBody && currentOverlay) {
            float time=0,length=0;
            float handoff=PlayerHitEnd(swingDirection)+chainFollowThrough;
            if(loadedBody->GetAnimation(const_cast<char*>(currentOverlay),&time,&length)) {
                state.holdChain=time<=handoff;
                if(!state.attackRequested && time>=handoff-chainClickLead && time<length)
                    QueueAttackDirection();
            }
        }
    }
    return state.Mouse(left,down);
}
bool K1EngageOwnsPlayer(unsigned long long id) { CSWSCreature* actor = K1EngageEnabled() ? ServerActor() : nullptr; return actor && actor->m_objectId == id; }
std::unordered_map<unsigned long long,long long> utilityReleaseAt;
bool K1EngageFastUtility(CSWSCreature* actor) {
    return K1EngageEnabled() && actor && (K1EngagePlanning() || K1EngageEngaged()) &&
        (actor==ServerActor() || npcPlayback.count(actor->m_objectId));
}
bool K1EngageProtectForceCast(CSWSCreature* actor) {
    if(!K1EngageFastUtility(actor))return false;
    auto* spell=g_pRules->GetSpell(actor->m_castSpellId);
    return actor->m_pCombatRound && actor->m_pCombatRound->m_bSpellCastRound && spell &&
        (spell->m_nUserType==CSWSpell::USERTYPE_FORCEPOWER || spell->m_nUserType==CSWSpell::USERTYPE_MASTER);
}
void K1EngageUtilityCompleted(CSWSCreature* actor) {
    if(!K1EngageFastUtility(actor))return;
    auto* round=actor->m_pCombatRound;
    if(round && round->m_bSpellCastRound) {
        // The native executor has released its current spell/item action.
        // End its spent round, preserving the pending round-action list.
        round->EndCombatRound(0);
        utilityReleaseAt[actor->m_objectId]=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
        fprintf(stderr,"K1_UTILITY_FAST_RELEASE actor=%llu\n",actor->m_objectId);
    }
}
static bool NativeUtilityAction(int action) {
    return action == AIACTION_CAST_SPELL || action == AIACTION_ITEM_CAST_SPELL ||
        action == AIACTION_SET_MINE || action == AIACTION_DISARM_MINE ||
        action == AIACTION_RECOVER_MINE || action == AIACTION_FLAG_MINE ||
        action == AIACTION_EXAMINE_MINE || action == AIACTION_HEAL ||
        action == AIACTION_PLAY_ANIMATION || action == AIACTION_USE_OBJECT ||
        action == AIACTION_PICK_UP_ITEM || action == AIACTION_DROP_ITEM ||
        action == AIACTION_OPEN_DOOR || action == AIACTION_CLOSE_DOOR ||
        action == AIACTION_UNLOCK_OBJECT || action == AIACTION_LOCK_OBJECT ||
        action == AIACTION_REST || action == AIACTION_ANIMAL_EMPATHY ||
        action == AIACTION_PICK_POCKET || action == AIACTION_COUNTER_SPELL;
}
static bool YieldPlayerNative(CSWCCreature* actor);
static bool NativeUtilityQueued(CSWSCreature* actor) {
    if (!actor) return false;
    // Nonblocking/depleted item actions can pop before their native animation
    // ends. The active one-shot has already left the pending animation queue;
    // retain its native body until the client returns to a looping animation.
    // Otherwise the owned rig cannot clear a stale native inject/dialog state,
    // and SetPrecalculatedPosition keeps rejecting movement after the last kit.
    auto* client=actor->GetClientCreature();
    auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
    if(client && body && !K1EngageOwnsAnimation(body) &&
       (client->m_nFireForgetCount>0 || client->AnimationFireAndForget(client->m_currentAnimation))) return true;
    if (auto* round = actor->m_pCombatRound) {
        // The round's spell flag can outlive the cast action and its animation.
        // Engage owns movement again as soon as the native action queue releases it.
        if (round->m_pRoundActionList && actor->GetNodeById(AIACTION_COMBAT)) {
            auto* head = round->m_pRoundActionList->m_pInternal->m_pHead;
            if (head) {
                const auto type = static_cast<CSWSCombatRoundAction*>(head->m_pData)->m_nActionType;
                if (type == CSWSCombatRoundAction::TYPE_CAST_SPELL ||
                    type == CSWSCombatRoundAction::TYPE_ITEM_SPELL) return true;
            }
        }
    }
    for (auto* node = actor->m_actionList.m_pInternal->m_pHead; node; node = node->m_pNext) {
        if (NativeUtilityAction(static_cast<CSWSObjectActionNode*>(node->m_pData)->m_nActionId))
            return true;
    }
    return false;
}
static void CompleteReleasedUtilityRound(CSWSCreature* actor) {
    if(!actor || NativeUtilityQueued(actor) || NativeStatusAnimation(actor))return;
    auto* round=actor->m_pCombatRound;
    if(round && round->m_bSpellCastRound && (!round->m_pRoundActionList || !round->m_pRoundActionList->m_pInternal->m_pHead)) {
        round->EndCombatRound(0);
        fprintf(stderr,"K1_UTILITY_ROUND_RELEASE actor=%llu started=%d spell=%d\n",actor->m_objectId,round->m_bRoundStarted,round->m_bSpellCastRound);
    }
}
bool K1EngageHoldNativeAction(unsigned long long id, int action) {
    // A queued NPC utility waits for the source swing, then UpdateNpcPlayback
    // restores the native body before allowing the action executor to proceed.
    const auto npc=npcPlayback.find(id);
    if(npc!=npcPlayback.end() && npc->second.requested &&
       (NativeUtilityAction(action) || (action==AIACTION_COMBAT &&
        NativeUtilityQueued(g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(id))))) return true;
    if ((!NativeUtilityAction(action) && action != AIACTION_COMBAT) ||
        (!K1EngagePlanning() && !K1EngageEngaged()) || !K1EngageOwnsPlayer(id)) return false;
    if (action == AIACTION_COMBAT && !NativeUtilityQueued(ServerActor())) return false;
    return K1EngagePlanning() || !nativeActionBody;
}
bool K1EngageCanMovePastCombatMarker(CSWSCreature* actor) {
    if (!actor || !K1EngageActive() || nativeActionBody ||
        !K1EngageOwnsPlayer(actor->m_objectId)) return false;
    const auto* head = actor->m_actionList.m_pInternal->m_pHead;
    if (!head || head->m_pNext || !head->m_pData ||
        static_cast<const CSWSObjectActionNode*>(head->m_pData)->m_nActionId != AIACTION_COMBAT)
        return false;
    // An empty combat driver can remain queued while the native round winds down.
    const auto* round = actor->m_pCombatRound;
    return round && (!round->m_pRoundActionList ||
                     !round->m_pRoundActionList->m_pInternal->m_pHead);
}
void K1EngageDetected(bool automatic) {
    if(automatic && sandboxSession && !sandboxAi)return;
    if (K1EngageEnabled() && state.DetectCombat()) {encounter.clear();fprintf(stderr,"K1_ENGAGE mode=planning reason=enemy\n");}
}
void K1EngageSuspend() { if (K1EngageEngaged() || retainedPlanningPose) Plan(); }
bool K1EngagePauseInput(bool engageFromPlanning) {
    if (!K1EngageEngaged() && !K1EngagePlanning()) return false;
    if (K1EngageEngaged()) {
        Plan(true);
    } else if (engageFromPlanning) {
        auto* app = g_pAppManager->m_pClientExoApp;
        auto* gui = app->GetInGameGui();
        if (gui && !gui->m_nGlobalDialogState && !gui->m_bInGameMenuShown &&
            !gui->m_pGuiManager->m_modalPanels.m_count &&
            gui->m_swGuiToggleState != CGuiInGame::SWGUI_STATE_COVERED &&
            app->m_pInternal->m_inputClass == INPUTCLASS_GAME)
            K1EngageEnter();
    }
    return true;
}
float K1EngagePlanningTurn() {
    return K1EngagePlanning() ? float(int(keyD) - int(keyA)) : 0.f;
}
float K1EngageCameraDelta(float delta) {
    if (!K1EngagePlanning()) return delta;
    return float(g_pAppManager->m_pClientExoApp->GetPauseTimer()->GetSnapshotTimeDifference()) / 1000000.f;
}
bool K1EngageEnter() {
    if (!K1EngageEnabled()) return false;
    // R/default action and the HUD can request Engage again. An active mode is
    // already owned; do not run the failed-entry rollback and rebuild its body.
    if (K1EngageEngaged()) return true;
    CSWCCreature* actor = Actor();
    if (!Supported(actor)) return false;
    if(NativeStatusAnimation(actor->GetServerCreature()) && !YieldPlayerNative(actor)) return false;
    // Capture the normal appearance before the Engage-only rig changes its pose.
    nativeCameraHeadHeight=actor->GetHeadHeightOffset();
    nativeCameraFollowHeight=nativeCameraHeadHeight+actor->GetCameraOffset();
    if(!nativeActionBody && RigModeEnabled() && !RebuildOwnedRig(actor,true)) return false;
    Gob* body = actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY, 1);
    if (!body || (!nativeActionBody && !LoadAnimations(body))) {
        if(rigBodyRequested) RebuildOwnedRig(actor,false);
        fprintf(stderr,"K1_ENGAGE missing animation model\n");return false;
    }
    if(state.DetectCombat()) encounter.clear();
    const bool finishingSwing = retainedPlanningPose && state.attacking;
    if (!state.Engage(true)) {if(rigBodyRequested) RebuildOwnedRig(actor,false);return false;}
    retainedPlanningPose = false;
    state.attacking = finishingSwing;
    actorId = actor->m_objectId; actor->SetCombatMode(1);
    MaintainSaberCombat(actor);
    aimYaw=std::atan2(actor->m_orientation.y,actor->m_orientation.x);aimPitch=0;movement.Clear();
    g_pAppManager->m_pClientExoApp->SetCombatMode(1);
    g_pAppManager->m_pClientExoApp->SetAutoPaused(0, 0);
    g_pAppManager->m_pClientExoApp->SetPausedByCombat(0, 0, 0);
    CClientExoAppInternal* internal=g_pAppManager->m_pClientExoApp->m_pInternal;
    if(internal->m_pPlayerControl) internal->m_pPlayerControl->SetEnabled(1);
    fprintf(stderr,"K1_ENGAGE resume_request client=%d server=%d pending=%d request=%d auto=%d\n",IsGamePaused(),g_pAppManager->m_pServerExoApp->GetPauseState(2),int(internal->m_bTogglePauseAtEndOfFrame),internal->m_combatPauseRequest,internal->m_bAutoPaused);
    g_bMouseCameraRotateToggle = 1; g_bLeftMouseDown = 0; g_bMouseCameraRotateToggleIos = 0;
    auto* mouseGui=g_pAppManager->m_pClientExoApp->GetSWGuiManager();
    if(mouseGui->m_storedMousePos.x<0) mouseGui->StoreCurrentMousePosition();
    resumeFrames=3; // SetPausedByCombat applies the request at end of frame.
    fprintf(stderr,"K1_ENGAGE mode=active actor=%llu\n",actorId);return true;
}
bool K1EngageTryDisengage() {
    if (!K1EngageEnabled()) return true;
    if (Spotted(true)) { fprintf(stderr,"K1_ENGAGE disengage=denied reason=sight_or_pursuit\n"); return false; }
    auto* module=g_pAppManager->m_pClientExoApp->GetModule();
    const Vector exitEye=module&&module->m_pCamera?module->m_pCamera->GetPosition():Vector();
    ReleaseNpcPlayback();
    victorySwingSerial=-1;
    Plan();state.Disengage(false);actorId=OBJECT_INVALID;encounter.clear();nativeActionBody=false;
    meleePoise.clear();
    enemyRepositionAfter.clear();
    if(CSWCCreature* actor=Actor()) {
        actor->GetServerCreature()->SetCombatMode(0,0);actor->SetCombatMode(0);
    }
    g_pAppManager->m_pClientExoApp->SetCombatMode(0);
    // Native combat can end during the final swing. Its follow target then
    // belongs to the outgoing rig, so appearance restoration invalidates it.
    // SetCombatMode(0) is already a no-op in that case: rebind through the native
    // service after the native body exists, preserving the displayed orbit.
    if(auto* actor=Actor())if(module&&module->m_pCamera&&actor->GetCameraFollowPart()){
        auto* behavior=module->m_pCamera->GetBehavior(-1);
        if(!behavior||behavior->GetType()==BEHAVIOR_CAMERA_ON_A_STICK){
            module->SetCameraFollow(actor->m_objectId,0);
            SeedNativeCameraOrbit(module->m_pCamera,actor,exitEye);
        }
    }
    fprintf(stderr,"K1_ENGAGE mode=exploration\n");return true;
}
static bool CanCancelQueuedActions(CSWSCreature* actor) {
    return state.mode != K1EngageState::Mode::Exploration && !nativeActionBody &&
        actor && actor->m_bCommandable == 1 && NativeUtilityQueued(actor);
}
bool K1EngageCancelQueuedActions() {
    if (!K1EngageEnabled()) return false;
    CSWSCreature* actor = ServerActor();
    if (!CanCancelQueuedActions(actor)) return false;
    g_pAppManager->m_pServerExoApp->ClearAllActions(actor->m_objectId);
    return true;
}
bool K1EngageMayClearCombat() {
    return !K1EngageEnabled() || state.mode==K1EngageState::Mode::Exploration || !Spotted();
}
static bool EncounterWon(CSWSCreature* actor) {
    if (!actor || Spotted() || encounter.empty()) return false;
    for (auto id : encounter) {
        CSWSCreature* enemy = g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(id);
        if (enemy && !enemy->GetDead() && enemy->GetArea() == actor->GetArea()) return false;
    }
    return true;
}
static bool YieldPlayerNative(CSWCCreature* actor) {
    if(actor && actor->GetServerCreature())ClearClashEpisode(actor->GetServerCreature()->m_objectId);
    if(nativeActionBody) return true;
    dualGuardAfterNative=DualMode() && state.rightHeld;
    auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
    const auto outgoing=CaptureUtilityPose(body);
    if(rigBodyRequested) {
        if(!RebuildOwnedRig(actor,false)) return false;
    } else if(body && body==loadedBody) {
        ReleaseAnimations(body);loadedBody=nullptr;
    }
    state.ClearInputs();retainedPlanningPose=false;currentAttack=nullptr;hadBlade=false;
    movement.Clear();nativeActionBody=true;
    if(contactGuardClock.wall)contactGuardClock.interrupted=true;
    BeginUtilityBlend(actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1),outgoing,false);
    return true;
}
void K1EngageUpdate(float) {
    if (!K1EngageEnabled()) return;
    PresentationProbeTick();
    ArenaTick();
    SandboxTick();
    if(sandboxMenuOpen)return;
    IntegrationTick();
    // Check the completed attack/movement state even on early native-ownership returns.
    struct PoiseUpdate {~PoiseUpdate(){UpdateMeleePoise();}} poiseUpdate;
    UpdateNpcPlayback();
    UpdateDismemberCorpses();
    CSWCCreature* actor = Actor();
    if(actor && state.mode!=K1EngageState::Mode::Exploration && NativeStatusAnimation(actor->GetServerCreature())) {
        if(EncounterWon(actor->GetServerCreature())) {K1EngageTryDisengage();return;}
        if(!YieldPlayerNative(actor)) Plan();
        if(std::getenv("K1_ENGAGE_FORCE_TARGET_TEST") && actor->GetServerCreature()->m_nState==EFFECT_STATE_CHOKE) {
            auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
            const auto clip=actor->GetAnimationBase()->GetAnimationName(CSWCAnimBase::CLIENT_ANIMATION_CHOKE);
            float time=0,length=0;
            if(!playerNativeEffectSampled && body && body->GetAnimation(const_cast<char*>(clip.CStr()),&time,&length) && time>0) {
                const bool native=!IsSourceBody(body) && !body->m_pAddInModel && !K1EngageOwnsAnimation(body);
                fprintf(stderr,"K1_PLAYER_NATIVE_EFFECT_%s state=%d clip=%s time=%g\n",native?"PASS":"FAIL",
                    actor->GetServerCreature()->m_nState,clip.CStr(),time);playerNativeEffectSampled=native;
            }
        }
        return;
    }
    if (K1EngagePlanning()) {
        if (actor && EncounterWon(actor->GetServerCreature())) {
            K1EngageTryDisengage();
        } else if (retainedPlanningPose) {
            auto* gui = g_pAppManager->m_pClientExoApp->GetInGameGui();
            Gob* body = actor ? actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY, 1) : nullptr;
            if (!body || !K1EngageOwnsAnimation(body) || !gui || gui->m_nGlobalDialogState ||
                gui->m_swGuiToggleState == CGuiInGame::SWGUI_STATE_COVERED) {
                Plan();
                return;
            }
            float time = 0.f, length = 0.f;
            if (state.attacking && currentOverlay &&
                body->GetAnimation(const_cast<char*>(currentOverlay), &time, &length) && time < length)
                return;
            state.attacking = false;
            currentAttack = nullptr;
            if (currentOverlay || strcmp(currentBase, "j_idle")) {
                blendPending = true;
                StopOverlays(body);
                currentBase = "j_idle";
                Overlay(body, currentBase, false);
            }
        }
        return;
    }
    if (state.mode != K1EngageState::Mode::Active) return;
    CGuiInGame* gui = g_pAppManager->m_pClientExoApp->GetInGameGui();
    if (resumeFrames && IsGamePaused()) { --resumeFrames;return; }
    resumeFrames=0;
    if (!actor || actor->m_objectId != actorId || !Supported(actor) || !gui || gui->m_nGlobalDialogState
        || gui->m_pGuiManager->m_modalPanels.m_count || gui->m_swGuiToggleState==CGuiInGame::SWGUI_STATE_COVERED
        || g_pAppManager->m_pClientExoApp->m_pInternal->m_inputClass!=INPUTCLASS_GAME || IsGamePaused()) {
        fprintf(stderr,"K1_ENGAGE suspend_reason actor=%d changed=%d supported=%d dialog=%d modal=%d paused=%d\n",
            actor!=nullptr,actor ? actor->m_objectId!=actorId : 1,Supported(actor),gui?gui->m_nGlobalDialogState:-1,
            gui?gui->m_pGuiManager->m_modalPanels.m_count:-1,IsGamePaused());
        fprintf(stderr,"K1_ENGAGE suspend_pause server=%d pending=%d request=%d\n",g_pAppManager->m_pServerExoApp->GetPauseState(2),int(g_pAppManager->m_pClientExoApp->m_pInternal->m_bTogglePauseAtEndOfFrame),g_pAppManager->m_pClientExoApp->m_pInternal->m_combatPauseRequest);
        Plan();return;
    }
    // Custom saber swings do not start native attack rounds. Keep the native
    // combat lifetime alive so its eight-second timeout cannot power down us.
    MaintainSaberCombat(actor);
    const bool nativeQueued = NativeUtilityQueued(actor->GetServerCreature());
    if (nativeActionBody && nativeQueued) return;
    // Sight/pursuit loss permits the explicit Disengage button. Automatic exit is
    // victory over known participants, not a transient empty native action queue.
    const bool victory=EncounterWon(actor->GetServerCreature());
    if(victory) {
        state.attackRequested=state.holdChain=false;
        if(!state.attacking){K1EngageTryDisengage();return;}
    }
    if (nativeActionBody) {
        const auto outgoing = CaptureUtilityPose(actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY, 1));
        // The native action and its one-shot have finished, but its nominal
        // round timer can still reject source melee damage after control returns.
        // Complete only that empty spell round through the native service; never
        // end an executing/queued utility or create a native follow-up attack.
        CompleteReleasedUtilityRound(actor->GetServerCreature());
        nativeActionBody = false;
        if (RigModeEnabled() && !RebuildOwnedRig(actor, true)) { Plan(); return; }
        Gob* incoming = actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY, 1);
        if (!incoming || !LoadAnimations(incoming)) { Plan(); return; }
        if(DualMode())state.rightHeld=dualGuardAfterNative;
        dualGuardAfterNative=false;
        BeginUtilityBlend(incoming, outgoing, true);
    }
    Gob* body = actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY, 1);
    if(RigModeEnabled() && IsSourceBody(body) && body->m_pModel &&
       _stricmp(body->m_pModel->m_szName,PlayerRigResource())) {
        const bool stanceSwitch=SingleStanceResourceName(body->m_pModel->m_szName) && SingleStanceResourceName(PlayerRigResource());
        struct SavedJoint {std::string name;Vector position;Quaternion orientation;};
        std::vector<SavedJoint> shown;
        const bool guardHeld=state.rightHeld;
        GripPose shownGrip;
        if(stanceSwitch) {
            shownGrip=ReadGrip(body);shownGrip.joined=playerPlayback.lastGrip.joined;
            auto capture=[&](auto&& self,Part* part)->void {
                if(part->m_pNode->m_pTree!=body->m_pModel)return;
                shown.push_back({part->m_pNode->TranslateNodeName(),part->m_vLocalPosition,part->m_qLocalOrientation});
                for(int i=0;i<part->m_nChildren;++i)self(self,part->m_paChildren[i]);
            };capture(capture,body->m_pPart);
        }
        // Drop the outgoing swing and contacts before replacing its rig.
        state.ClearInputs();state.attacking=false;state.chainSwings=0;
        redChainFrom=-1;
        blueChainFrom=-1;
        if(stanceSwitch)state.rightHeld=guardHeld;
        currentAttack=nullptr;hadBlade=false;staffContactBlades=0;
        auto outgoing=[&](const MeleeContact& contact){return contact.attacker==actor->GetServerCreature()->m_objectId;};
        sceneMeleeContacts.erase(std::remove_if(sceneMeleeContacts.begin(),sceneMeleeContacts.end(),outgoing),sceneMeleeContacts.end());
        ClearClashEpisode(actor->GetServerCreature()->m_objectId);
        if(!RebuildOwnedRig(actor,true)) {Plan();return;}
        body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(stanceSwitch) {
            if(!body || !LoadAnimations(body)){Plan();return;}
            // Rebind values by name; no pointers survive native appearance replacement.
            for(const auto& joint:shown)if(auto* part=body->GetPart(const_cast<char*>(joint.name.c_str()))) {
                part->m_vLocalPosition=joint.position;part->m_qLocalOrientation=joint.orientation;
                lastPose.push_back({part,joint.position,joint.orientation});
            }
            playerPlayback.lastGrip=shownGrip;
            playerPlayback.guardKnown=true;playerPlayback.guardHeld=state.Guarding();
            playerPlayback.stanceTransition=true;blendPending=true;
            fprintf(stderr,"K1_STANCE_BLEND guard=%d joints=%zu\n",state.Guarding(),lastPose.size());
        }
    }
    if (!body || !LoadAnimations(body)) { Plan();return; }
    const auto attackNow=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    if(!ParryAttackReady(actor->GetServerCreature()->m_objectId,attackNow)) {
        state.attacking=state.attackRequested=state.holdChain=false;state.chainSwings=0;hadBlade=false;
    }
    g_bMouseCameraRotateToggle = 1;
    float attackTime=0, length=0;
    const float chainHandoff = PlayerHitEnd(swingDirection) + chainFollowThrough;
    if(victory && victorySwingSerial!=attackSerial) {
        victorySwingSerial=attackSerial;
        fprintf(stderr,"K1_VICTORY_SWING_WAIT serial=%d clip=%s\n",attackSerial,currentOverlay?currentOverlay:"none");
    }
    if (state.attacking) {
        if (!currentOverlay || !body->GetAnimation(const_cast<char*>(currentOverlay), &attackTime, &length) || attackTime >= length) {
            if (state.attackRequested && state.chainSwings < PlayerChainLimit() && !nativeQueued) {
                // A click accepted on the last pose still counts if the clip
                // expires before the input is consumed on the next update.
                attackTime = std::max(attackTime, chainHandoff);
            } else {
                // The pose switch below removes the outgoing overlay.
                state.attacking=false;hadBlade=false;
                state.attackRequested=false;
                state.chainSwings=0;
            }
        }
    }
    if(victory && !state.attacking) {
        fprintf(stderr,"K1_VICTORY_SWING_COMPLETE serial=%d time=%g length=%g\n",attackSerial,attackTime,length);
        victorySwingSerial=-1;
        K1EngageTryDisengage();return;
    }
    if (nativeQueued) {
        state.attackRequested = state.holdChain = false;
        if (!state.attacking) {
            if(!YieldPlayerNative(actor)) Plan();
            return;
        }
    }
    // Guard can take over once the damaging slice ends; recovery need not
    // play to the end. Native queued powers/items retain their ownership.
    if(state.attacking && state.rightHeld && !nativeQueued && !victory && attackTime>=PlayerHitEnd(swingDirection) &&
       (!hadBlade||previousBladeTime>=PlayerHitEnd(swingDirection))) {
        state.attacking=state.attackRequested=state.holdChain=false;
        state.chainSwings=0;hadBlade=false;
        earlyGuardSerial=attackSerial;earlyGuardTime=attackTime;earlyGuardLength=length;
        fprintf(stderr,"K1_GUARD_EARLY serial=%d time=%g end=%g length=%g\n",attackSerial,attackTime,PlayerHitEnd(swingDirection),length);
    }
    // Give the outgoing cut its damage window and a short follow-through.
    const bool chain = state.attacking && state.chainSwings<PlayerChainLimit() &&
        attackTime>=chainHandoff &&
        (state.attackRequested || state.holdChain);
    if (chain || (!state.attacking && state.attackRequested)) {
        if(!state.attackRequested) QueueAttackDirection();
        if(chain && std::getenv("K1_ENGAGE_CONTROL_TEST"))
            fprintf(stderr,"K1_ENGAGE_CHAIN outgoing=%d incoming=%d time=%g\n",
                swingDirection,requestedDirection,attackTime);
        if(currentOverlay) body->PlayAnimation(const_cast<char*>(currentOverlay),1,AnimRun::kAnimRemove,0);
        redChainFrom=UsesRedAttacks() && chain?swingDirection:-1;
        blueChainFrom=BlueMode() && chain?swingDirection:-1;
        currentOverlay=nullptr;swingDirection=requestedDirection;currentAttack=PlayerAttackClip(true);hitConsumed=false;hadBlade=false;
        if(RedMode() && redChainFrom>=0)fprintf(stderr,"K1_RED_CHAIN_SELECT from=%d to=%d clip=%s transition=%s start=%g end=%g\n",redChainFrom,swingDirection,currentAttack,redChains[redChainFrom][swingDirection].transition,PlayerHitStart(swingDirection),PlayerHitEnd(swingDirection));
        swingBackLeft=requestedBackLeft;
        state.chainSwings=chain?state.chainSwings+1:1;
        state.holdChain=state.leftHeld;
        state.attackRequested=false;state.attacking=true;attackTime=0;
        playerAttackStartedAt=attackNow;
        meleePoise[actor->GetServerCreature()->m_objectId].blockReaction=-1;
        NotePoiseActivity(actor->GetServerCreature(),"swing");
        ++attackSerial;
        fprintf(stderr,"K1_ENGAGE attack=%s\n",currentAttack);
    }
    bool moving=keyW!=keyS || keyA!=keyD || movement.x*movement.x+movement.y*movement.y>.04f;
    bool walk=keyShift||state.ForceWalk();
    const char* base=state.Guarding() ? "j_guard" : "j_idle";
    if(moving) {
        if(keyW!=keyS) base=keyS ? (walk?"j_backwalk":"j_backrun") : (walk?"j_walk":"j_run");
        else base=walk?"j_walk":"j_run";
    }
    const auto& poise=meleePoise[actor->GetServerCreature()->m_objectId];
    const int block=state.attacking?-1:ActiveBlockReaction(actor->GetServerCreature()->m_objectId,attackNow);
    const bool blockRestart=block>=0 && playerPlayback.blockSerial!=poise.blockSerial;
    const char* purpleGuardWalk=PurpleGuardWalkClip(state.Guarding(),state.attacking,block>=0,moving,keyS && !keyW);
    if(purpleGuardWalk)base=purpleGuardWalk;
    const char* overlay=state.attacking ? PlayerAttackClip(moving)
        : (block>=0?blockUpper[block]:(state.Guarding() && moving && !purpleGuardWalk ? "y_guard" : nullptr));
    float liveTime=0,liveLength=0;
    bool baseChanged=currentBase!=base || !body->GetAnimation(const_cast<char*>(base),&liveTime,&liveLength);
    bool overlayMissing=overlay && !body->GetAnimation(const_cast<char*>(overlay),&liveTime,&liveLength);
    if(baseChanged || currentOverlay!=overlay || overlayMissing || blockRestart) {
        blendPending=true;
        if(currentOverlay) body->PlayAnimation(const_cast<char*>(currentOverlay),1,AnimRun::kAnimRemove,0);
        if(baseChanged) {
            if(currentBase) body->PlayAnimation(const_cast<char*>(currentBase),1,AnimRun::kAnimRemove,0);
            Overlay(body,base,false);currentBase=base;
        }
        currentOverlay=overlay;
        if(overlay) body->PlayAnimation(const_cast<char*>(overlay),block>=0?0.f:(state.attacking?PlayerMeleeRate():1.f),
            AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn|(state.attacking?AnimRun::kAnimOnce:0),attackTime);
        fprintf(stderr,"K1_ENGAGE pose base=%s overlay=%s rate=%g\n",base,overlay?overlay:"none",state.attacking?PlayerMeleeRate():1.f);
    }
    if(block>=0) {
        float t=0,len=0;
        if(body->GetAnimation(const_cast<char*>(overlay),&t,&len)) {
            body->SetAnimationTime(const_cast<char*>(overlay),std::min(float(attackNow-poise.blockStarted)/1000000.f,std::max(0.f,len-.0001f)));
            if(blockRestart) fprintf(stderr,"K1_BLOCK_REACTION_PLAY actor=%llu serial=%u clip=%s base=%s\n",
                actor->GetServerCreature()->m_objectId,poise.blockSerial,overlay,base);
            playerPlayback.blockSerial=poise.blockSerial;
        }
    }
}
void K1EngageCycleStance() {
    if(!K1EngageEnabled() || !std::getenv("K1_ENGAGE_SANDBOX") || sandboxMenuOpen ||
       StaffMode() || DualMode() || !Supported(Actor()))return;
    const bool hasBlue=g_pExoResMan->Exists(CResRef("k1jablue"),RESTYPE_MDL,static_cast<unsigned int*>(nullptr)) &&
        g_pExoResMan->Exists(CResRef("k1jablue"),RESTYPE_MDX,static_cast<unsigned int*>(nullptr));
    const bool hasPurple=g_pExoResMan->Exists(CResRef("k1japurp"),RESTYPE_MDL,static_cast<unsigned int*>(nullptr)) &&
        g_pExoResMan->Exists(CResRef("k1japurp"),RESTYPE_MDX,static_cast<unsigned int*>(nullptr));
    const int next=NextSingleStance(hasBlue,hasPurple);
    const char* resource=SingleStanceResource(next);
    if(!g_pExoResMan->Exists(CResRef(resource),RESTYPE_MDL,static_cast<unsigned int*>(nullptr)) ||
       !g_pExoResMan->Exists(CResRef(resource),RESTYPE_MDX,static_cast<unsigned int*>(nullptr))) {
        fprintf(stderr,"K1_STANCE_UNAVAILABLE resource=%s\n",resource);return;
    }
    // Existing rig replacement cancels outgoing swings and contacts.
    redSelection=next;
    fprintf(stderr,"K1_STANCE_SELECTED red=%d blue=%d purple=%d resource=%s\n",RedMode(),BlueMode(),PurpleMode(),PlayerRigResource());
}
void K1EngageKey(int code,bool down) {
    if(!K1EngageEnabled()) return;
    if(code==39 && down){sandboxTogglePending=true;return;}
    if(sandboxMenuOpen && down)return;
    switch(code) {case 26:keyW=down;break;case 22:keyS=down;break;case 4:keyA=down;break;
        case 7:keyD=down;break;case 225:keyShift=down;break;}
}
void K1EngageReleaseHeldInput() {
    keyW=keyS=keyA=keyD=keyShift=false;movement.Clear();
    state.rightHeld=state.leftHeld=state.holdChain=state.attackRequested=false;
    dualGuardAfterNative=false;contactGuardClock={};
}
bool K1EngageLook(int dx,int dy) {
    if(!K1EngageActive()) return false;
    float sensitivity=.11f*3.141592653589793f/180.f;
    aimYaw-=dx*sensitivity;
    aimPitch=std::max(-1.22f,std::min(1.22f,aimPitch-dy*sensitivity));
    return true;
}
bool K1EngageControl(CSWPlayerControl* control,float dt) {
    if(!K1EngageEnabled() || state.mode==K1EngageState::Mode::Exploration) return false;
    CSWCCreature* actor=Actor();
    if(std::getenv("K1_ENGAGE_CONTROL_TEST")) {
        static int count=0;
        if(++count%30==0) fprintf(stderr,"K1_ENGAGE_CONTROL_STATE control=%llu actor=%llu enabled=%d active=%d keys=%d%d%d%d walk=%d dt=%g\n",control->m_objectId,actor?actor->m_objectId:0,control->m_bEnabled,K1EngageActive(),keyW,keyS,keyA,keyD,keyShift,dt);
    }
    if(!actor || control->m_objectId!=actor->m_objectId) return false;
    if(nativeActionBody) {movement.Clear();return true;}
    if(!K1EngageActive() || !control->m_bEnabled || actor->GetServerCreature()->GetDead()) {movement.Clear();return true;}
    Vector facing(std::cos(aimYaw),std::sin(aimYaw),0);
    actor->SetOrientation(facing);actor->SetDesiredWalkOrientation(facing);actor->SetDesiredStationaryOrientation(facing);
    actor->GetServerCreature()->SetOrientation(facing);
    bool walk=keyShift||state.ForceWalk();control->SetPlayerWalking(walk);
    float dx,dy;
    movement.Step(float(int(keyW)-int(keyS)),float(int(keyD)-int(keyA)),aimYaw,
        NativeMoveSpeed(actor->GetServerCreature(),walk),dt,dx,dy);
    // JKA runs/walks while its render controller turns the legs toward movement;
    // the unused RUNSTRAFE ranges in the owned GLA are identical right-moving poses.
    float forward=float(int(keyW)-int(keyS)),right=float(int(keyD)-int(keyA));
    float targetYaw=0;
    if(forward || right) targetYaw=forward<0 ? std::atan2(right,-forward) : std::atan2(-right,forward);
    // Intentional prototype limit after gameplay review: retain the JKA spine
    // controller, but limit the additional live movement turn to +/-45 degrees.
    targetYaw=std::max(-.7853981634f,std::min(.7853981634f,targetYaw));
    float yawStep=5.f*3.141592653589793f/180.f*std::min(dt,.1f)*60.f;
    legsYaw+=std::max(-yawStep,std::min(yawStep,targetYaw-legsYaw));
    Vector next=actor->m_position+Vector(dx,dy,0);
    if((dx!=0 || dy!=0) && !actor->SetPrecalculatedPosition(next)) movement.Clear();
    float front=movement.x*facing.x+movement.y*facing.y;
    float side=movement.x*facing.y-movement.y*facing.x;
    actor->SetSWAnimationSpeed(front,side);
    return true;
}
static Quaternion EngageOrbit(float pivotHeight) {
    // Positive pitch looks up and lowers the eye by radius * sin(pitch).
    const float clearance=CameraPersonalSpace+engageCameraFloorMargin;
    const float availableDrop=std::max(0.f,pivotHeight-clearance);
    const float upLimit=std::asin(std::min(1.f,availableDrop/engageOrbitRadius));
    aimPitch=std::min(aimPitch,upLimit);
    constexpr float halfPi=1.5707963267948966f;
    return Quaternion(Vector(0,0,1),aimYaw-halfPi)*Quaternion(Vector(1,0,0),halfPi+aimPitch);
}

bool K1EngageCamera(CSWCameraOnAStick* camera) {
    if(!K1EngageActive() || !camera->m_pTargetObject.m_pObject || !camera->m_pOwner->GetScene()) return false;
    CSWCCreature* actor=Actor();if(!actor) return false;
    // Native on-a-stick eye height is target/camera offset plus style HEIGHT.
    const float pivotHeight=nativeCameraFollowHeight+camera->m_fHeight+engageCameraHeightTrim;
    Quaternion q=EngageOrbit(pivotHeight);
    Vector target=actor->m_position+Vector(0,0,pivotHeight);
    Vector eye=target+q.zdir()*engageOrbitRadius;
    eye=camera->Control_HitCheckCamera(eye);
    camera->m_pOwner->SetPosition(eye);camera->m_pOwner->SetOrientation(q);
    camera->m_vLastCameraPosition=eye;camera->m_vLastTargetPosition=target;
    camera->m_fTurn=0;camera->m_fTiltAccel=0;
    camera->m_bMouseRotating=1;
    return true;
}
bool K1EngageSuppressEdgeCamera(int edgeX,int edgeY,bool validPointer) {
    if(!K1EngageEnabled()) return false;
    if(K1EngageActive()) return true;
    if(nativeEdgeAwaitInterior && std::getenv("K1_ENGAGE_CONTROL_TEST")) {
        auto* gui=g_pAppManager->m_pClientExoApp->GetSWGuiManager();
        fprintf(stderr,"K1_CAMERA_EDGE_GATE edges=%d,%d cursor=%d,%d modal=%d valid=%d clear=%d\n",
            edgeX,edgeY,gui->m_currentMousePos.x,gui->m_currentMousePos.y,gui->m_modalPanels.m_count,
            validPointer,validPointer && !edgeX && !edgeY);
    }
    if(validPointer && !edgeX && !edgeY) nativeEdgeAwaitInterior=false;
    return nativeEdgeAwaitInterior;
}
bool K1EngageCamera(CSWBehaviorCameraCombat* camera) {
    if(!K1EngageActive() || !camera->m_pOwner || !camera->m_pOwner->GetScene()) return false;
    CSWCCreature* actor=Actor();if(!actor) return false;
    // Match CalculateDesiredLocation's neutral height, retaining mouse orbit.
    const float pivotHeight=nativeCameraHeadHeight+camera->m_fHeight+engageCameraHeightTrim;
    Quaternion q=EngageOrbit(pivotHeight);
    Vector target=actor->m_position+Vector(0,0,pivotHeight);
    Vector eye=camera->DetermineSafePosition(actor,target+q.zdir()*engageOrbitRadius,actor->m_position);
    camera->m_pOwner->SetPosition(eye);camera->m_pOwner->SetOrientation(q);
    camera->m_fTurn=camera->m_fMouseTurn=0;camera->m_bMouseRotating=1;
    return true;
}

namespace {
constexpr float cameraBlendDuration = .28f;
struct EngageCameraBlend {
    CAurCamera* camera = nullptr;
    CAurScene* scene = nullptr;
    Vector shownEye, targetEye, startOffset;
    Quaternion shownOrientation, targetOrientation, startOrientation;
    bool active = false;
    bool applied = false;
    float elapsed = cameraBlendDuration;
} cameraBlend;
int cameraHandoffTrace=0;

Vector BlendCameraOrbit(const Vector& start, const Vector& end, float t) {
    const float startRadius = std::hypot(start.x, start.y);
    const float endRadius = std::hypot(end.x, end.y);
    if (startRadius < .001f || endRadius < .001f)
        return start + (end - start) * t;
    const float startAngle = std::atan2(start.y, start.x);
    const float turn = std::remainder(std::atan2(end.y, end.x) - startAngle, 6.283185307f);
    const float angle = startAngle + turn * t;
    const float radius = startRadius + (endRadius - startRadius) * t;
    return Vector(radius * std::cos(angle), radius * std::sin(angle),
                  start.z + (end.z - start.z) * t);
}
void SeedNativeCameraOrbit(CAurCamera* camera,CSWCCreature* actor,const Vector& eye) {
    auto* behavior=camera->GetBehavior(-1);
    Vector toward=actor->m_position-eye;toward.z=0;
    if(!behavior || magnitude(toward)<.001f) return;
    const float yaw=Yaw(normalize(toward));
    // Engage's local +Z is the orbit axis; native cameras use local +Y.
    // Reusing the pitched Engage quaternion can reverse native horizontal yaw.
    if(behavior->GetType()==BEHAVIOR_CAMERA_COMBAT) {
        auto* combat=static_cast<CSWBehaviorCameraCombat*>(behavior);
        camera->SetPosition(eye);camera->SetOrientation(YawPitchRoll(yaw,combat->m_fPitch,0));
        combat->m_fTurn=combat->m_fMouseTurn=0;combat->m_acceleration.SetVelocity(0);
        combat->m_acceleration.m_velocity=Vector();
    } else if(behavior->GetType()==BEHAVIOR_CAMERA_ON_A_STICK) {
        auto* follow=static_cast<CSWCameraOnAStick*>(behavior);
        follow->ResetLastPosition(eye,YawPitchRoll(yaw,follow->m_fPresetPitch,0));
        if(follow->m_pTargetObject.m_pObject) follow->m_vLastTargetPosition=follow->GetTargetHeadPosition();
        follow->m_fTurn=follow->m_fTurnAccel=follow->m_fTiltAccel=0;
        follow->m_splitAcceleration.SetVelocity(0);
        follow->m_splitAcceleration.m_velocity=Vector();
        follow->m_lookAtTargetId=OBJECT_INVALID;
    }
}
}

void K1EngageRestoreCameraTarget(CAurCamera* camera) {
    auto& blend = cameraBlend;
    if (blend.camera != camera ||
        blend.scene != camera->AsCAurObject()->GetScene()) return;
    if(blend.active && !K1EngageActive()) {
        if(auto* actor=Actor()) SeedNativeCameraOrbit(camera,actor,blend.shownEye);
        if(std::getenv("K1_ENGAGE_CONTROL_TEST")) {
            cameraHandoffTrace=24;
            auto* behavior=camera->GetBehavior(-1);
            fprintf(stderr,"K1_CAMERA_HANDOFF_SEED type=%d eye=%g,%g,%g yaw=%g\n",
                behavior?behavior->GetType():-1,blend.shownEye.x,blend.shownEye.y,blend.shownEye.z,Yaw(camera->GetOrientation()));
        }
        blend.applied=false;
        return;
    }
    if(!blend.applied) return;
    blend.applied = false;
    // Keep the presentation pose available for HUD projection and picking.
    // Restore the controller's pose before it runs, unless it has been replaced.
    const Vector eye = camera->GetPosition();
    const Quaternion orientation = camera->GetOrientation();
    if (eye.x == blend.shownEye.x && eye.y == blend.shownEye.y && eye.z == blend.shownEye.z)
        camera->SetPosition(blend.targetEye);
    if (orientation.w == blend.shownOrientation.w && orientation.x == blend.shownOrientation.x &&
        orientation.y == blend.shownOrientation.y && orientation.z == blend.shownOrientation.z)
        camera->SetOrientation(blend.targetOrientation);
}

void K1EngagePresentCamera(CAurCamera* camera, float delta) {
    if (!K1EngageEnabled()) return;
    auto& blend = cameraBlend;
    CAurBehavior* behavior = camera->GetBehavior(-1);
    CAurScene* scene = camera->AsCAurObject()->GetScene();
    CSWCCreature* actor = Actor();
    if (!actor || !scene || !behavior ||
        (behavior->GetType() != BEHAVIOR_CAMERA_ON_A_STICK &&
         behavior->GetType() != BEHAVIOR_CAMERA_COMBAT)) {
        blend = {};
        return;
    }
    const bool active = K1EngageActive();
    const Vector pivot = actor->m_position;
    const bool trace = std::getenv("K1_ENGAGE_CONTROL_TEST") != nullptr;
    if(trace && cameraHandoffTrace>0) {
        --cameraHandoffTrace;
        const Vector offset=camera->GetPosition()-pivot;
        auto* gui=g_pAppManager->m_pClientExoApp->GetSWGuiManager();
        float turn=0,velocity=0;
        if(behavior->GetType()==BEHAVIOR_CAMERA_COMBAT) {
            auto* native=static_cast<CSWBehaviorCameraCombat*>(behavior);turn=native->m_fTurn;velocity=native->m_acceleration.m_velocity.y;
        } else if(behavior->GetType()==BEHAVIOR_CAMERA_ON_A_STICK) {
            auto* native=static_cast<CSWCameraOnAStick*>(behavior);turn=native->m_fTurnAccel;velocity=native->m_splitAcceleration.m_velocity.y;
        }
        fprintf(stderr,"K1_CAMERA_HANDOFF_FRAME left=%d type=%d offset=%g,%g,%g yaw=%g turn=%g request=%g cursor=%d,%d screen=%d,%d\n",
            cameraHandoffTrace,behavior->GetType(),offset.x,offset.y,offset.z,Yaw(camera->GetOrientation()),turn,velocity,
            gui->m_currentMousePos.x,gui->m_currentMousePos.y,gui->m_nScreenWidth,gui->m_nScreenHeight);
    }
    if (blend.camera != camera || blend.scene != scene) {
        blend = {};
        blend.camera = camera;
        blend.scene = scene;
        blend.active = active;
    } else if (blend.active != active) {
        blend.active = active;
        blend.startOffset = blend.shownEye - pivot;
        blend.startOrientation = blend.shownOrientation;
        blend.elapsed = 0.f;
        if (trace) fprintf(stderr, "K1_ENGAGE_CAMERA_BLEND active=%d begin\n", active);
    } else if (blend.elapsed < cameraBlendDuration) {
        blend.elapsed = std::min(cameraBlendDuration, blend.elapsed + std::max(0.f, std::min(delta, .05f)));
        if (trace && blend.elapsed == cameraBlendDuration)
            fprintf(stderr, "K1_ENGAGE_CAMERA_BLEND active=%d complete\n", active);
    }

    blend.targetEye = blend.shownEye = camera->GetPosition();
    blend.targetOrientation = blend.shownOrientation = camera->GetOrientation();
    if (blend.elapsed >= cameraBlendDuration) return;

    const float t = blend.elapsed / cameraBlendDuration;
    const float weight = t * t * (3.f - 2.f * t);
    blend.shownEye = pivot + BlendCameraOrbit(blend.startOffset, blend.targetEye - pivot, weight);
    if (t > 0.f) {
        if (behavior->GetType() == BEHAVIOR_CAMERA_COMBAT) {
            auto* combat = static_cast<CSWBehaviorCameraCombat*>(behavior);
            blend.shownEye = combat->DetermineSafePosition(actor, blend.shownEye, pivot);
        } else {
            auto* follow = static_cast<CSWCameraOnAStick*>(behavior);
            if (follow->m_pTargetObject.m_pObject)
                blend.shownEye = follow->Control_HitCheckCamera(blend.shownEye);
        }
    }
    blend.shownOrientation = Interpolate(blend.startOrientation, blend.targetOrientation, weight);
    camera->SetPosition(blend.shownEye);
    camera->SetOrientation(blend.shownOrientation);
    blend.applied = true;
}

bool K1EngageOwnsAnimation(Gob* model) {
    if(NpcOwnsBody(model)) return true;
    if((!K1EngageEngaged() && !retainedPlanningPose) || model!=loadedBody) return false;
    CSWSCreature* actor=ServerActor();
    return actor && !actor->GetDead() && !NativeStatusAnimation(actor);
}
bool K1EngageAlignSword(Gob* weapon) {
    if(!weapon || !SwordModelGeometry(weapon) || K1EngagePoiseBladePercent(weapon)>=0)return false;
    auto held=[&](CSWCCreature* owner) {
        if(!owner || (owner->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,0)!=weapon &&
                      owner->GetModel(CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY,0)!=weapon))return false;
        return K1EngageOwnsAnimation(owner->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,0));
    };
    bool owned=held(Actor());
    if(!owned)for(const auto& entry:npcPlayback)if(held(entry.second.owner)){owned=true;break;}
    if(!owned)return false;
    // Reuse the stance's actual full/upper selection, including spinning chain
    // links. Each sword keeps its own fresh hand-hook sample (both dual hands).
    const char* base=nullptr;const char* overlay=nullptr;
    const char* attackFull=nullptr;const char* attackUpper=nullptr;
    Gob* body=nullptr;bool attacking=false,guarding=false,yellow=false,purple=false,supported=false,rightHand=false;
    int attackDirection=0;
    auto* player=Actor();
    if(player && (player->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,0)==weapon ||
                  player->GetModel(CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY,0)==weapon)) {
        body=player->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,0);
        base=currentBase;overlay=currentOverlay;attacking=state.attacking;guarding=state.Guarding();
        rightHand=player->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,0)==weapon;
        yellow=!RedMode() && !BlueMode() && !PurpleMode() && !DualMode() && !StaffMode();
        purple=PurpleMode();supported=true;attackDirection=swingDirection;
        if(attacking){attackFull=PlayerAttackClip(false);attackUpper=PlayerAttackClip(true);}
    } else for(const auto& entry:npcPlayback) {
        const auto& npc=entry.second;
        if(npc.owner && (npc.owner->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,0)==weapon ||
                         npc.owner->GetModel(CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY,0)==weapon)) {
            body=npc.body;base=npc.base;overlay=npc.overlay;attacking=npc.attacking;guarding=NpcGuarding(npc);
            rightHand=npc.owner->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,0)==weapon;
            yellow=!npc.red && !npc.blue && !npc.dual && !npc.staff && !npc.ranged;
            supported=!npc.ranged;attackDirection=npc.direction;
            if(attacking){attackFull=NpcAttackClip(npc,false);attackUpper=NpcAttackClip(npc,true);}
            break;
        }
    }
    bool edgeAligned=false;
    float relaxedIdleRoll=0.f;
    if(supported && body) {
        const char* source=nullptr;const char* clock=nullptr;
        int cuttingDirection=0;
        if(attacking && overlay && attackDirection>=0 && attackDirection<7 &&
           ((attackFull && !strcmp(overlay,attackFull)) || (attackUpper && !strcmp(overlay,attackUpper)))) {
            source=attackFull;clock=overlay;cuttingDirection=attackDirection;
        } else if(!purple && guarding && base && (!overlay || !strcmp(overlay,"y_guard"))) {
            if(strstr(base,"walk")){source="j_walk";clock=base;}
            else if(!strcmp(base,"j_guard") || !strcmp(base,"j_idle")){source="j_guard";clock=overlay?overlay:base;}
        } else if(!purple && !attacking && !guarding && !overlay && base &&
                  (!strcmp(base,"j_idle") || strstr(base,"walk"))) {
            // Match corrected relaxed idle while walking with guard released.
            // Purple's authored non-attacking presentation stays unchanged.
            relaxedIdleRoll=-1.5707963268f;
        }
        float time=0,length=0;Quaternion correction;
        if(clock && body->GetAnimation(const_cast<char*>(clock),&time,&length)) {
            if(yellow && rightHand && !SwordModelGeometry(weapon)->doubleBlade &&
               K1YellowSwordHooks::Correction(source,time,correction))
                weapon->m_qOrientation=normalize(weapon->m_qOrientation*correction);
            edgeAligned=K1SwordAlignment::Align(weapon->m_qOrientation,normalize(body->m_qOrientation),cuttingDirection);
        }
    }
    // The attachment supplies a fresh hook orientation before every call.
    // Roll around the existing blade axis; do not move the grip or blade tips.
    if(!edgeAligned)weapon->m_qOrientation=normalize(weapon->m_qOrientation*Quaternion(Vector(0,0,1),1.5707963268f+relaxedIdleRoll));
    return true;
}
void K1EngagePreparePose(Gob* model) {
    if(!K1EngageOwnsAnimation(model)) return;
    model->EndLookAtAnimate();
    // A native spell can temporarily own the body. Remove its remaining run when
    // saber control resumes; evaluate no native pose beneath the JKA controllers.
    for(int i=model->m_paAnimations.m_count-1;i>=0;--i) {
        char* name=model->m_paAnimations.m_pData[i]->m_pAnimation->m_szName;
        if(strncmp(name,"j_",2) && strncmp(name,"y_",2) && strncmp(name,"s_",2))
            model->PlayAnimation(name,1,AnimRun::kAnimRemove,0);
    }
}
namespace {
// Same target conversion used by NewController::Control; reference sampling
// must also work before that reference animation has ever been played.
int ControllerTarget(const NewController& c,const Part* part) {
    return c.m_target.m_nOffset & NewController::kTargetUnconverted?
        GetOffset64(c.m_target.m_nOffset & NewController::kTargetOffsetMask,part->m_pNode->m_nFlags):c.m_target.m_nOffset;
}
MdlNode* FindPoseNode(MdlNode* node,int id) {
    if(node->m_nNodeID==id) return node;
    for(int i=0;i<node->m_listChildren.m_count;++i)
        if(MdlNode* found=FindPoseNode(node->m_listChildren.m_pData[i],id)) return found;
    return nullptr;
}
bool BridgeTorso(Gob* body,const char* reference,float time) {
    Part* pelvis=body->GetPart(const_cast<char*>("pelvis_g"));
    Part* torso=body->GetPart(const_cast<char*>("torso_g"));
    if(!pelvis || !torso || !body->m_pAddInModel) return false;
    MdlNode* node=nullptr;
    for(int i=0;i<body->m_pAddInModel->m_animations.m_count;++i) {
        Animation* animation=body->m_pAddInModel->m_animations.m_pData[i];
        if(!strcmp(animation->m_szName,reference)) {node=FindPoseNode(animation->m_pMdlNode,pelvis->m_nNodeID);break;}
    }
    if(!node) return false;
    Vector offset;Quaternion referenceQ;bool position=false,orientation=false;
    for(int i=0;i<node->m_listControllers.m_count;++i) {
        NewController& c=node->m_listControllers.m_pData[i];
        if(ControllerTarget(c,pelvis)==K2_OFFSETOF(Part,m_vLocalPosition)) position=c.GetVectorValue(time,node,offset)!=0;
        if(ControllerTarget(c,pelvis)==K2_OFFSETOF(Part,m_qLocalOrientation)) orientation=c.GetQuaternionValue(time,node,referenceQ)!=0;
    }
    if(!position || !orientation) return false;
    Vector referenceP=pelvis->m_pNode->m_vPosition+offset;
    Quaternion rotation=pelvis->m_qLocalOrientation*Quaternion(referenceQ.w,-referenceQ.x,-referenceQ.y,-referenceQ.z);
    Vector relative=torso->m_vLocalPosition-referenceP;
    // PMBBL's torso and pelvis are siblings. Reconnect the JKA torso's reference pelvis
    // to this frame's locomotion pelvis before K12 updates skin, head and weapon transforms.
    torso->m_vLocalPosition=pelvis->m_vLocalPosition+rotation.xdir()*relative.x+rotation.ydir()*relative.y+rotation.zdir()*relative.z;
    // Keep the source torso aim. JKA compensates for the legs' motion offset;
    // applying the whole pelvis rotation here folds/twists the spine twice.
    return true;
}
}
namespace {
Quaternion Inverse(const Quaternion& q) {return Quaternion(q.w,-q.x,-q.y,-q.z);}
float WrapMotionAngle(float a) {
    const float pi=3.14159265358979323846f;
    while(a>pi) a-=2*pi;
    while(a< -pi) a+=2*pi;
    return a;
}
bool MotionSpine(Gob* body,Part* pelvis,Part* lower,Part* middle,Part* upper,float yaw,float viewPitch=0) {
    Part* motion=body->GetPart(const_cast<char*>("j_motion"));
    if(!motion) return false;
    const char* reference=nullptr;float time=0,length=0;
    for(int i=body->m_paAnimations.m_count-1;i>=0;--i) {
        char* name=body->m_paAnimations.m_pData[i]->m_pAnimation->m_szName;
        if(!strcmp(name,"y_guard")) reference="j_guard";
        else if(BlockReference(name)) reference=BlockReference(name);
        else if(!strcmp(name,"y_pistol_fire")) reference="j_pistol_fire";
        else if(!strcmp(name,"y_rifle_fire")) reference="j_rifle_fire";
        else if(!strcmp(name,"y_parry_up")) reference="j_parry_up";
        else if(!strcmp(name,"y_parry_ul")) reference="j_parry_ul";
        else if(!strcmp(name,"y_parry_ur")) reference="j_parry_ur";
        else if(!strcmp(name,"y_parry_ll")) reference="j_parry_ll";
        else if(!strcmp(name,"y_parry_lr")) reference="j_parry_lr";
        else reference=AttackFullReference(name);
        if(reference) {body->GetAnimation(name,&time,&length);break;}
    }
    Vector angles;
    if(reference) {
        Quaternion referenceQ;bool found=false;
        for(int i=0;i<body->m_pModel->m_animations.m_count;++i) {
            Animation* a=body->m_pModel->m_animations.m_pData[i];
            if(strcmp(a->m_szName,reference)) continue;
            MdlNode* n=FindPoseNode(a->m_pMdlNode,pelvis->m_nNodeID);
            if(n) for(int j=0;j<n->m_listControllers.m_count;++j) {
                NewController& c=n->m_listControllers.m_pData[j];
                if(ControllerTarget(c,pelvis)==K2_OFFSETOF(Part,m_qLocalOrientation))
                    found=c.GetQuaternionValue(time,n,referenceQ)!=0;
            }
            break;
        }
        if(!found) {fprintf(stderr,"K1_RIG_MOTION_REFERENCE_FAIL clip=%s\n",reference);return false;}
        Vector position;Quaternion pelvisQ,motionQ;
        pelvis->GetWorld(position,pelvisQ);motion->GetWorld(position,motionQ);
        Quaternion inverse=Inverse(normalize(body->m_qOrientation));
        Quaternion mixed=normalize((inverse*pelvisQ)*Inverse(normalize(referenceQ))*(inverse*motionQ));
        Vector forward=mixed.ydir(),right=mixed.xdir();
        angles.x=-std::atan2(forward.z,std::sqrt(forward.x*forward.x+forward.y*forward.y));
        angles.y=WrapMotionAngle(std::atan2(forward.y,forward.x)-1.5707963267948966f);
        angles.z=std::atan2(right.z,std::sqrt(right.x*right.x+right.y*right.y));
    }
    // Pinned JKA SP cg_motionBoneComp=2: compensate mixed pelvis/Motion before
    // distributing view angles. The unweighted Motion node's bind orientation
    // retains the exact raw source spine-axis calibration (not an axis guess).
    yaw=std::max(-1.0471975512f,std::min(1.0471975512f,yaw));
    const float pitch=WrapMotionAngle(viewPitch-angles.x),turn=WrapMotionAngle(-yaw-angles.y),roll=WrapMotionAngle(-angles.z);
    pelvis->m_qLocalOrientation=normalize(Quaternion(Vector(0,0,1),yaw)*pelvis->m_qLocalOrientation);
    Part* joints[]={lower,middle,upper};const float py[]={.4f,.4f,.2f},yr[]={.45f,.35f,.2f};
    for(int i=0;i<3;++i) {
        Quaternion axes=normalize(motion->m_pNode->m_qOrientation);
        Quaternion correction=Quaternion(axes.zdir(),roll*yr[i])
            *Quaternion(axes.ydir(),pitch*py[i])*Quaternion(axes.xdir(),turn*yr[i]);
        joints[i]->m_qLocalOrientation=normalize(joints[i]->m_qLocalOrientation*correction);
    }
    return true;
}
void ConnectTorso(Gob* body) {
    Part* p=body->GetPart(const_cast<char*>("pelvis_g"));
    Part* t=body->GetPart(const_cast<char*>("torso_g"));
    if(p && t && t->m_pParent==p) return; // the candidate has a real spine parent
    if(p && t) {
        Vector anchor=t->m_pNode->m_vPosition-p->m_pNode->m_vPosition;
        Quaternion q=p->m_qLocalOrientation*Inverse(p->m_pNode->m_qOrientation);
        t->m_vLocalPosition=p->m_vLocalPosition+q.xdir()*anchor.x+q.ydir()*anchor.y+q.zdir()*anchor.z;
    }
}
void TurnSpine(Gob* body,float yaw,const char* reference=nullptr,float time=0) {
    Part* p=body->GetPart(const_cast<char*>("pelvis_g"));
    Part* t=body->GetPart(const_cast<char*>("torso_g"));
    Part* u=body->GetPart(const_cast<char*>("torsoUpr_g"));
    if(!p || !t || !u) return;
    if(Part* middle=body->GetPart(const_cast<char*>("torsoMid_g"))) {
        // Camera pitch is positive looking up; JKA view pitch is positive down.
        static const bool viewAim=std::getenv("K1_ENGAGE_VIEW_PITCH")!=nullptr;
        float pitch=viewAim && K1EngageActive() && body==loadedBody?-aimPitch:0;
        if(viewAim && K1EngageActive() && body==loadedBody) {++viewPitchPoses;maximumViewPitch=std::max(maximumViewPitch,std::fabs(pitch));}
        if(MotionSpine(body,p,t,middle,u,yaw,pitch)) return;
        // JKA's render controller limits movement yaw to +/-60 and distributes
        // aim compensation over all three source spine joints (45/35/20).
        yaw=std::max(-1.0471975512f,std::min(1.0471975512f,yaw));
        Part* joints[]={p,t,middle,u};Quaternion original[4];Vector position;
        for(int i=0;i<4;++i) joints[i]->GetWorld(position,original[i]);
        const float shares[]={1,.55f,.20f,0};
        for(int i=0;i<4;++i) {
            Quaternion parent;
            if(joints[i]->m_pParent) joints[i]->m_pParent->GetWorld(position,parent);
            else parent=body->m_qOrientation;
            Quaternion desired=Quaternion(body->m_qOrientation.zdir(),yaw*shares[i])*original[i];
            joints[i]->m_qLocalOrientation=normalize(Inverse(normalize(parent))*desired);
        }
        return;
    }
    // JKA spreads aim compensation through lower/upper lumbar and thoracic
    // (45/35/20). K1 has two spine joints: keep the last share at the shoulder
    // and head roots, preserving their aim without a single 90-degree waist fold.
    Quaternion referencePelvis=p->m_qLocalOrientation;
    if(reference && body->m_pAddInModel) {
        for(int i=0;i<body->m_pAddInModel->m_animations.m_count;++i) {
            Animation* a=body->m_pAddInModel->m_animations.m_pData[i];
            if(strcmp(a->m_szName,reference)) continue;
            MdlNode* n=FindPoseNode(a->m_pMdlNode,p->m_nNodeID);
            if(n) for(int j=0;j<n->m_listControllers.m_count;++j) {
                NewController& c=n->m_listControllers.m_pData[j];
                if(ControllerTarget(c,p)==K2_OFFSETOF(Part,m_qLocalOrientation)) c.GetQuaternionValue(time,n,referencePelvis);
            }
            break;
        }
    }
    Quaternion oldUpper=t->m_qLocalOrientation*u->m_qLocalOrientation;
    p->m_qLocalOrientation=Quaternion(Vector(0,0,1),yaw)*p->m_qLocalOrientation;
    Quaternion difference=p->m_qLocalOrientation*Inverse(referencePelvis);
    t->m_qLocalOrientation=Interpolate(Quaternion(1,0,0,0),difference,.55f)*t->m_qLocalOrientation;
    // torsoUpr is the chest/thoracic target. Its source already includes the
    // skipped upper-lumbar joint; it carries the final 35+20 percent together.
    Quaternion newUpper=oldUpper;
    u->m_qLocalOrientation=Inverse(t->m_qLocalOrientation)*newUpper;
    for(const char* name:{"lcollar_dum","rcollar_dum","headhook"})
        if(Part* part=body->GetPart(const_cast<char*>(name)))
            part->m_qLocalOrientation=Inverse(newUpper)*oldUpper*part->m_qLocalOrientation;
    ConnectTorso(body);
}
Vector Rotate(const Quaternion& q,const Vector& v) {return q.xdir()*v.x+q.ydir()*v.y+q.zdir()*v.z;}
void BindFrame(Part* part,Vector& position,Quaternion& orientation) {
    if(part->m_pParent) BindFrame(part->m_pParent,position,orientation);
    else {position=Vector();orientation=Quaternion();}
    position=position+Rotate(orientation,part->m_pNode->m_vPosition);
    orientation=normalize(orientation*part->m_pNode->m_qOrientation);
}
std::unordered_map<std::string,std::shared_ptr<CSWCAnimBase>>& WearablePoseDonors() {
    static std::unordered_map<std::string,std::shared_ptr<CSWCAnimBase>> donors;return donors;
}
void RetargetWearableAuthoredPose(Gob* body,Part* joint,Vector& position,Quaternion& orientation) {
    const char* native=body&&body->m_pModel?WearableNativeResource(body->m_pModel->m_szName):nullptr;
    if(!native || !_stricmp(native,"pmbbl") || !joint)return;
    // Match the separate wearable generator's bind-relative orientation and
    // height-scaled position transfer. Original PMBBL tuning is untouched.
    const char* resource=PlayerRigForNative("pmbbl");
    auto& owner=WearablePoseDonors()[resource];
    if(!owner){owner=std::make_shared<CSWCAnimBase>();if(!owner->LoadModel(CResRef(resource),CSWCAnimBase::ANIM_MODEL_BODY)){owner.reset();return;}}
    auto* source=owner->GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
    auto* original=source?source->GetPart(joint->m_pNode->TranslateNodeName()):nullptr;
    if(!original)return;
    Vector targetHead,targetHip,sourceHead,sourceHip;Quaternion unused;
    auto* th=body->GetPart(const_cast<char*>("headhook"));auto* tp=body->GetPart(const_cast<char*>("pelvis_g"));
    auto* sh=source->GetPart(const_cast<char*>("headhook"));auto* sp=source->GetPart(const_cast<char*>("pelvis_g"));
    if(!th||!tp||!sh||!sp)return;
    BindFrame(th,targetHead,unused);BindFrame(tp,targetHip,unused);BindFrame(sh,sourceHead,unused);BindFrame(sp,sourceHip,unused);
    const float height=sourceHead.z-sourceHip.z;
    if(height>.1f)position=position*((targetHead.z-targetHip.z)/height);
    orientation=normalize(joint->m_pNode->m_qOrientation*Inverse(original->m_pNode->m_qOrientation)*orientation);
}
void CaptureNativePose(Gob* body,Gob* native,std::vector<BodyPose>& idle) {
    idle.clear();
    // Retarget the native stance in model space across the extra spine joints.
    // Keep the adapted rig's bind offsets wherever the parent has changed.
    auto capture=[&](auto&& self,Part* part,Quaternion parentPose)->void {
        if(part->m_pNode->m_pTree!=body->m_pModel) return;
        char* name=part->m_pNode->TranslateNodeName();
        Part* original=native->GetPart(name);
        Vector position=part->m_pNode->m_vPosition;
        Quaternion rotation=part->m_pNode->m_qOrientation;
        if(original) {
            Vector unused;
            Quaternion nativeBind,targetBind,nativePose;
            BindFrame(original,unused,nativeBind);
            BindFrame(part,unused,targetBind);
            original->GetWorld(unused,nativePose);
            rotation=normalize(Inverse(parentPose)*nativePose*Inverse(nativeBind)*targetBind);
            if(part->m_pParent && original->m_pParent &&
                !strcmp(part->m_pParent->m_pNode->TranslateNodeName(),
                        original->m_pParent->m_pNode->TranslateNodeName())) {
                Quaternion nativeParent,targetParent;
                BindFrame(original->m_pParent,unused,nativeParent);
                BindFrame(part->m_pParent,unused,targetParent);
                position=position+Rotate(Inverse(targetParent)*nativeParent,
                    original->m_vLocalPosition-original->m_pNode->m_vPosition);
            }
        }
        idle.push_back({part,position,rotation});
        Quaternion pose=normalize(parentPose*rotation);
        for(int i=0;i<part->m_nChildren;++i) self(self,part->m_paChildren[i],pose);
    };
    capture(capture,body->m_pPart,Quaternion());
}
#include "include/k1guardheadedits.inc"
#include "include/k1yellowguardarms.inc"
#include "include/k1npcguardedits.inc"
void CacheNativeIdle(Gob* body,const char* resource,std::vector<BodyPose>* destination,const char* animation) {
    auto& idle=destination?*destination:nativeIdlePose;
    idle.clear();
    CSWCAnimBase nativeOwner;
    if(!nativeOwner.LoadModel(CResRef(resource),CSWCAnimBase::ANIM_MODEL_BODY)) return;
    Gob* native=nativeOwner.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
    if(!native) return;
    native->m_bAlwaysRendering=true;
    native->EnableForceAnimations();
    if(!native->PlayAnimation(const_cast<char*>(animation),0,AnimRun::kAnimNoBlendIn,.13f)) return;
    native->Animate(.000001f);
    CaptureNativePose(body,native,idle);
    if(destination==&nativeStaffGuardPose) {
        const auto& head=authoredGuardHeads[3][0];
        for(auto& pose:idle) if(!strcmp(pose.part->m_pNode->TranslateNodeName(),"headhook")) {
            pose.position=Vector(head.px,head.py,head.pz);
            pose.orientation=normalize(Quaternion(head.qw,head.qx,head.qy,head.qz));
            RetargetWearableAuthoredPose(body,pose.part,pose.position,pose.orientation);
        }
    }
}
#include "include/k1yellowlocomotion.h"
Quaternion BendFrame(Vector direction,Vector normal) {
    Vector x=normalize(direction),z=normalize(normal-x*dot(x,normal)),y=normalize(cross(z,x));
    return normalize(Matrix(x,y,z).getquaternion());
}
void SetWorldOrientation(Part* part,Quaternion orientation) {
    Vector p;Quaternion parent;
    if(part->m_pParent) part->m_pParent->GetWorld(p,parent);
    else parent=part->m_pGob->m_qOrientation;
    part->m_qLocalOrientation=normalize(Inverse(normalize(parent))*orientation);
}
void ModelFrame(Gob* body,Part* part,Vector& position,Quaternion& orientation) {
    part->GetWorld(position,orientation);
    Quaternion inverse=Inverse(normalize(body->m_qOrientation));
    position=Rotate(inverse,position-body->m_vPosition)/body->m_fObjectScale;
    orientation=normalize(inverse*orientation);
}
std::vector<UtilityPoseJoint> CaptureUtilityPose(Gob* body) {
    std::vector<UtilityPoseJoint> pose;
    if (!body) return pose;
    const char* joints[] = {
        "pelvis_g", "torso_g", "torsoMid_g", "torsoUpr_g", "neck_g",
        "lcollar_g", "rcollar_g", "lbicep_g", "rbicep_g", "Lforearm_g", "Rforearm_g",
        "Lhand_g", "Rhand_g", "rhand", "lhand",
        "lfemur_g", "rfemur_g", "ltibia_g", "rtibia_g", "lfoot_g", "rfoot_g",
    };
    for (const char* name : joints) {
        Part* part = body->GetPart(const_cast<char*>(name));
        if (!part || part->m_pNode->m_pTree != body->m_pModel) continue;
        Vector unused;
        Quaternion bind, shown;
        BindFrame(part, unused, bind);
        ModelFrame(body, part, unused, shown);
        pose.push_back({name, normalize(shown * Inverse(bind))});
    }
    return pose;
}

void BeginUtilityBlend(Gob* body, const std::vector<UtilityPoseJoint>& source, bool toEngage,UtilityPoseBlend& blend) {
    blend = {};
    if (!body || !body->m_pPart || source.empty()) return;
    auto capture = [&](auto&& self, Part* part, Quaternion parentStart) -> void {
        if (part->m_pNode->m_pTree != body->m_pModel) return;
        Quaternion start = part->m_qLocalOrientation;
        for (const auto& joint : source) {
            if (strcmp(joint.name, part->m_pNode->TranslateNodeName()) != 0) continue;
            Vector unused;
            Quaternion bind;
            BindFrame(part, unused, bind);
            start = normalize(Inverse(parentStart) * joint.bindDelta * bind);
            blend.joints.push_back({part, start, part->m_qLocalOrientation, start});
            break;
        }
        const Quaternion modelStart = normalize(parentStart * start);
        for (int i = 0; i < part->m_nChildren; ++i)
            self(self, part->m_paChildren[i], modelStart);
    };
    capture(capture, body->m_pPart, Quaternion());
    if (blend.joints.empty()) return;
    blend.body = body;
    blend.toEngage = toEngage;
    // Seed the newly attached model even if it hasn't evaluated a clip yet.
    for (const auto& joint : blend.joints) joint.part->m_qLocalOrientation = joint.start;
    if (std::getenv("K1_ENGAGE_CONTROL_TEST"))
        fprintf(stderr, "K1_ENGAGE_UTILITY_BLEND begin engage=%d joints=%zu\n",
            toEngage, blend.joints.size());
}

void ApplyUtilityBlend(Gob* body, float delta) {
    auto& blend=UtilityBlendFor(body);
    if (blend.body != body) return;
    constexpr float duration = .16f;
    if (blend.elapsed >= duration) {
        if(&blend==&utilityBlend)++utilityBlendsCompleted[blend.toEngage];
        if (std::getenv("K1_ENGAGE_CONTROL_TEST"))
            fprintf(stderr, "K1_ENGAGE_UTILITY_BLEND complete engage=%d\n", blend.toEngage);
        blend = {};
        return;
    }
    const float t = blend.elapsed / duration;
    const float weight = t * t * (3.f - 2.f * t);
    for (auto& joint : blend.joints) {
        joint.raw = joint.part->m_qLocalOrientation;
        joint.shown = normalize(Interpolate(joint.start, joint.raw, weight));
        joint.part->m_qLocalOrientation = joint.shown;
    }
    blend.elapsed += std::max(0.f, std::min(delta, .05f));
}
Quaternion ArmGuide(Gob* body,bool left) {
    Vector a,b,c;Quaternion q;
    ModelFrame(body,body->GetPart(const_cast<char*>(left?"lbicep_g":"rbicep_g")),a,q);
    ModelFrame(body,body->GetPart(const_cast<char*>(left?"Lforearm_g":"Rforearm_g")),b,q);
    ModelFrame(body,body->GetPart(const_cast<char*>(left?"Lhand_g":"Rhand_g")),c,q);
    Vector x=normalize(c-a),y=b-a-x*dot(b-a,x);
    if(magnitude(y)<.00001f) y=Vector(0,0,1)-x*x.z;
    y=normalize(y);Vector z=normalize(cross(x,y));
    return normalize(Matrix(x,y,z).getquaternion());
}
GripPose ReadGrip(Gob* body) {
    GripPose pose;
    ModelFrame(body,body->GetPart(const_cast<char*>("rhand")),pose.hilt,pose.hiltQ);
    ModelFrame(body,body->GetPart(const_cast<char*>("lhand")),pose.left,pose.leftQ);
    Quaternion helper=body->GetPart(const_cast<char*>("lhand"))->m_qLocalOrientation;
    pose.leftQ=normalize(pose.leftQ*Inverse(helper));
    pose.rightGuide=ArmGuide(body,false);pose.leftGuide=ArmGuide(body,true);
    Vector right;Quaternion q;
    ModelFrame(body,body->GetPart(const_cast<char*>("rbicep_g")),right,q);
    ModelFrame(body,body->GetPart(const_cast<char*>("lbicep_g")),pose.leftCenter,q);
    pose.center=(right+pose.leftCenter)*.5f;
    return pose;
}
Vector ArcPoint(Vector from,Vector fromCenter,Vector to,Vector toCenter,Vector center,float alpha) {
    Vector a=from-fromCenter,b=to-toCenter;float la=magnitude(a),lb=magnitude(b);
    if(la<.00001f || lb<.00001f) return from*(1-alpha)+to*alpha;
    Vector an=a/la,bn=b/lb;Quaternion turn;
    if(dot(an,bn)<-.99999f) {
        Vector axis=cross(an,Vector(0,0,1));
        if(magnitude(axis)<.00001f) axis=cross(an,Vector(0,1,0));
        turn=Quaternion(normalize(axis),3.1415926536f);
    } else turn=normalize(RotationArc(an,bn));
    return center+Rotate(normalize(Interpolate(Quaternion(),turn,alpha)),an)*(la*(1-alpha)+lb*alpha);
}
PointCorrection InitialPointCorrection(Vector from,Vector fromCenter,Vector to,Vector toCenter) {
    Vector a=from-fromCenter,b=to-toCenter;float la=magnitude(a),lb=magnitude(b);
    PointCorrection result;result.offset=a-b;result.radius=la-lb;
    if(la<.00001f || lb<.00001f) {result.linear=true;return result;}
    a=a/la;b=b/lb;
    if(dot(a,b)<-.99999f) {
        Vector axis=cross(b,Vector(0,0,1));
        if(magnitude(axis)<.00001f) axis=cross(b,Vector(0,1,0));
        result.rotation=Quaternion(normalize(axis),3.1415926536f);
    } else result.rotation=normalize(RotationArc(b,a));
    return result;
}
Vector CorrectPoint(const PointCorrection& correction,Vector to,Vector toCenter,Vector center,float alpha) {
    Vector direction=to-toCenter;float length=magnitude(direction);
    if(correction.linear || length<.00001f) return center+direction+correction.offset*(1-alpha);
    return center+Rotate(normalize(Interpolate(correction.rotation,Quaternion(),alpha)),direction/length)*(length+correction.radius*(1-alpha));
}
bool SolveArm(Gob* body,bool left,Vector goal,Quaternion wristQ,const Quaternion& guide) {
    Part* shoulder=body->GetPart(const_cast<char*>(left?"lbicep_g":"rbicep_g"));
    Part* elbow=body->GetPart(const_cast<char*>(left?"Lforearm_g":"Rforearm_g"));
    Part* wrist=body->GetPart(const_cast<char*>(left?"Lhand_g":"Rhand_g"));
    if(!shoulder || !elbow || !wrist) return false;
    Vector a,b,c;Quaternion q;shoulder->GetWorld(a,q);elbow->GetWorld(b,q);wrist->GetWorld(c,q);
    float upper=magnitude(b-a),lower=magnitude(c-b),distance=magnitude(goal-a);
    if(distance<.00001f) return false;
    Vector d=(goal-a)/distance;
    float along=std::max(std::fabs(upper-lower)+.0001f,std::min(upper+lower-.0001f,distance));
    Quaternion worldGuide=normalize(body->m_qOrientation*guide);
    // Transport the entire orthonormal frame to the actual wrist direction.
    // Projecting its pole alone becomes unstable when that pole points at the wrist.
    Quaternion transport=normalize(RotationArc(worldGuide.xdir(),d));
    if(rigDiagnosticState>=1000 && std::getenv("K1_ENGAGE_RIG_DIAG"))
        fprintf(stderr,"K1_RIG_TIMELINE_GUIDE sequence=%d frame=%d left=%d dot=%.9g\n",rigDiagnosticState-1000,rigDiagnosticFrame,left,dot(worldGuide.xdir(),d));
    Vector pole=Rotate(transport,worldGuide.ydir());
    pole=pole-d*dot(pole,d);
    float poleLength=magnitude(pole);
    if(magnitude(pole)<.00001f) pole=Vector(0,0,1)-d*d.z;
    if(magnitude(pole)<.00001f) pole=Vector(1,0,0)-d*d.x;
    pole=normalize(pole);
    float axial=(upper*upper-lower*lower+along*along)/(2*along);
    Vector targetElbow=a+d*axial+pole*std::sqrt(std::max(0.f,upper*upper-axial*axial));
    if(rigDiagnosticState>=0 && std::getenv("K1_ENGAGE_RIG_DIAG"))
        fprintf(stderr,"K1_RIG_IK state=%d frame=%d left=%d pole_length=%.9g pole=%.9g,%.9g,%.9g distance=%.9g along=%.9g elbow=%.9g,%.9g,%.9g target=%.9g,%.9g,%.9g wrist=%.9g,%.9g,%.9g\n",rigDiagnosticState,rigDiagnosticFrame,left,poleLength,pole.x,pole.y,pole.z,distance,along,b.x,b.y,b.z,targetElbow.x,targetElbow.y,targetElbow.z,c.x,c.y,c.z);
    Vector ba,bb,bc;Quaternion bq,eq,cq;
    BindFrame(shoulder,ba,bq);BindFrame(elbow,bb,eq);BindFrame(wrist,bc,cq);
    Vector bindNormal=normalize(cross(bb-ba,bc-bb));
    Vector normal=normalize(cross(pole,d));
    Quaternion bodyQ=normalize(body->m_qOrientation);
    Quaternion upperQ=BendFrame(targetElbow-a,normal)*Inverse(BendFrame(bb-ba,bindNormal))*bq;
    Quaternion lowerQ=BendFrame(a+d*along-targetElbow,normal)*Inverse(BendFrame(bc-bb,bindNormal))*eq;
    // Bind frames are model-local; target frames above are already world-space.
    // Remove only their reference axes, retaining the native anatomical roll.
    SetWorldOrientation(shoulder,normalize(upperQ));
    SetWorldOrientation(elbow,normalize(lowerQ));
    Quaternion parent;elbow->GetWorld(b,parent);wrist->m_qLocalOrientation=normalize(Inverse(normalize(parent))*normalize(wristQ));
    return true;
}
void PlaceGrip(Gob* body,GripPose pose,bool joined) {
    Part* rh=body->GetPart(const_cast<char*>("rhand"));Part* lh=body->GetPart(const_cast<char*>("lhand"));
    Quaternion rq=normalize(pose.hiltQ*Inverse(rh->m_qLocalOrientation));
    Vector offsets[2]={Rotate(rq,rh->m_vLocalPosition),Rotate(pose.leftQ,lh->m_vLocalPosition)};
    Vector centers[2];float near[2],far[2];
    for(int side=0;side<2;++side) {
        Vector a,b,c;Quaternion q;
        ModelFrame(body,body->GetPart(const_cast<char*>(side?"lbicep_g":"rbicep_g")),a,q);
        ModelFrame(body,body->GetPart(const_cast<char*>(side?"Lforearm_g":"Rforearm_g")),b,q);
        ModelFrame(body,body->GetPart(const_cast<char*>(side?"Lhand_g":"Rhand_g")),c,q);
        float upper=magnitude(b-a),lower=magnitude(c-b);
        near[side]=std::fabs(upper-lower)+.0002f;far[side]=upper+lower-.0002f;
        centers[side]=a+offsets[side]+(side?Rotate(pose.hiltQ,Vector(0,0,.11f)):Vector());
    }
    // A single hilt is projected into both reachable arm volumes, rather than
    // correcting one wrist after independently blending both arm rotations.
    for(int i=0;i<12;++i) for(int side=0;side<(joined?2:1);++side) {
        Vector d=pose.hilt-centers[side];float length=magnitude(d);
        if(length>.00001f) pose.hilt=centers[side]+d*(std::max(near[side],std::min(far[side],length))/length);
    }
    Vector right=pose.hilt-offsets[0];
    Vector left=(joined?pose.hilt-Rotate(pose.hiltQ,Vector(0,0,.11f)):pose.left)-offsets[1];
    float scale=body->m_fObjectScale;Quaternion q=normalize(body->m_qOrientation);
    SolveArm(body,false,body->m_vPosition+Rotate(q,right)*scale,normalize(q*rq),pose.rightGuide);
    SolveArm(body,true,body->m_vPosition+Rotate(q,left)*scale,normalize(q*pose.leftQ),pose.leftGuide);
}
Vector& sourceGapCorrection=playerPlayback.sourceGapCorrection;
float& sourceContactError=playerPlayback.sourceContactError;
bool SourceHandPosition(Gob* body,bool left,Vector goal) {
    Part* shoulder=body->GetPart(const_cast<char*>(left?"lbicep_g":"rbicep_g"));
    Part* elbow=body->GetPart(const_cast<char*>(left?"Lforearm_g":"Rforearm_g"));
    Part* end=body->GetPart(const_cast<char*>(left?"lhand":"rhand"));
    Part* wrist=body->GetPart(const_cast<char*>(left?"Lhand_g":"Rhand_g"));
    if(!shoulder || !elbow || !end || !wrist) return false;
    Vector a,b,c,marker;Quaternion uq,lq,wq,q;
    shoulder->GetWorld(a,uq);elbow->GetWorld(b,lq);wrist->GetWorld(c,wq);end->GetWorld(marker,q);
    // Solve the actual wrist, retaining its world orientation. The independent
    // saber marker is not the forearm endpoint: its offset can reverse the
    // apparent elbow plane near extension and changes as the forearm rotates.
    goal=goal-(marker-c);
    float upper=magnitude(b-a),lower=magnitude(c-b),distance=magnitude(goal-a);
    if(distance<.00001f || upper<.00001f || lower<.00001f) return false;
    Vector d=(goal-a)/distance,oldD=normalize(c-a);
    float used=std::max(std::fabs(upper-lower)+.00001f,std::min(upper+lower-.00001f,distance));
    Vector pole=b-a-oldD*dot(b-a,oldD);
    if(rigDiagnosticState>=1000 && std::getenv("K1_ENGAGE_RIG_DIAG"))
        fprintf(stderr,"K1_STAFF_GRIP_GEOMETRY action=%d frame=%d left=%d pole_length=%g distance=%g reach=%g pole=%g,%g,%g\n",rigDiagnosticState-1000,rigDiagnosticFrame,left,magnitude(pole),distance,upper+lower,pole.x,pole.y,pole.z);
    if(magnitude(pole)<.00001f) pole=uq.ydir()-oldD*dot(uq.ydir(),oldD);
    if(magnitude(pole)<.00001f) return false;
    Quaternion transport=normalize(RotationArc(oldD,d));
    pole=Rotate(transport,normalize(pole));
    pole=normalize(pole-d*dot(pole,d));
    float along=(upper*upper-lower*lower+used*used)/(2*used);
    Vector nextElbow=a+d*along+pole*std::sqrt(std::max(0.f,upper*upper-along*along));
    Vector nextEnd=a+d*used;
    // Transport one bend plane for the whole chain. Independent shortest arcs
    // add different axial twists to upper/forearm even when their endpoints fit.
    Vector normal=normalize(cross(pole,d));
    auto hinge=[&](Vector from,Vector to) {
        from=normalize(Rotate(transport,from));to=normalize(to);
        float angle=std::atan2(dot(normal,cross(from,to)),dot(from,to));
        return normalize(Quaternion(normal,angle)*transport);
    };
    SetWorldOrientation(shoulder,normalize(hinge(b-a,nextElbow-a)*uq));
    SetWorldOrientation(elbow,normalize(hinge(c-b,nextEnd-nextElbow)*lq));
    SetWorldOrientation(wrist,wq);
    return true;
}
void SourceGripTransition(Gob* body,Vector localGap) {
    Part* right=body->GetPart(const_cast<char*>("rhand"));
    Part* left=body->GetPart(const_cast<char*>("lhand"));
    if(!right || !left) return;
    Vector h,l;Quaternion authoredWeapon,lq;right->GetWorld(h,authoredWeapon);left->GetWorld(l,lq);
    Vector gap=Rotate(authoredWeapon,localGap)*body->m_fObjectScale;
    // The equal split minimizes the sum of squared hand displacements. Anchoring
    // the right arm forced the entire transition error into the left elbow.
    Vector shared=h+(l-h-gap)*.5f;
    auto keepWeapon=[&]() {
        Vector p;Quaternion parent;right->m_pParent->GetWorld(p,parent);
        // JKA's independent, unskinned weapon tag keeps its source orientation.
        right->m_qLocalOrientation=normalize(Inverse(normalize(parent))*authoredWeapon);
    };
    if(!SourceHandPosition(body,false,shared)) return;
    keepWeapon();
    for(int i=0;i<20;++i) {
        Quaternion currentWeapon;right->GetWorld(h,currentWeapon);
        Vector goal=h+gap;
        if(!SourceHandPosition(body,true,goal)) return;
        left->GetWorld(l,lq);Vector error=l-goal;
        if(magnitude(error)<.00001f) return;
        if(!SourceHandPosition(body,false,h+error)) return;
        keepWeapon();
    }
}
void StaffGripTransition(Gob* body,const GripPose& target) {
    // The staff can enter a block from an authored free-hand recovery. Carry
    // both already-blended hand trajectories in model space; rotating the old
    // metre-wide release gap in the new hilt frame drives a wrist past reach.
    Part* right=body->GetPart(const_cast<char*>("rhand"));
    Part* left=body->GetPart(const_cast<char*>("lhand"));
    const Quaternion facing=normalize(body->m_qOrientation);
    const float scale=body->m_fObjectScale;
    const Quaternion weapon=normalize(facing*target.hiltQ);
    const Vector gap=Rotate(facing,target.left-target.hilt)*scale;
    Vector shared=body->m_vPosition+Rotate(facing,target.hilt)*scale;
    Vector centers[2];float near[2],far[2];
    for(int side=0;side<2;++side) {
        Vector a,b,c,marker;Quaternion q;
        body->GetPart(const_cast<char*>(side?"lbicep_g":"rbicep_g"))->GetWorld(a,q);
        body->GetPart(const_cast<char*>(side?"Lforearm_g":"Rforearm_g"))->GetWorld(b,q);
        body->GetPart(const_cast<char*>(side?"Lhand_g":"Rhand_g"))->GetWorld(c,q);
        (side?left:right)->GetWorld(marker,q);
        const float upper=magnitude(b-a),lower=magnitude(c-b);
        near[side]=std::fabs(upper-lower)+.00001f;
        far[side]=upper+lower-.00001f;
        centers[side]=a+(marker-c)-(side?gap:Vector());
    }
    for(int i=0;i<24;++i) for(int side=0;side<2;++side) {
        Vector d=shared-centers[side];float length=magnitude(d);
        if(length>.00001f) shared=centers[side]+d*(std::max(near[side],std::min(far[side],length))/length);
    }
    auto keepWeapon=[&]() {
        Vector p;Quaternion parent;right->m_pParent->GetWorld(p,parent);
        right->m_qLocalOrientation=normalize(Inverse(normalize(parent))*weapon);
    };
    keepWeapon();
    if(!SourceHandPosition(body,false,shared)) return;
    keepWeapon();
    SourceHandPosition(body,true,shared+gap);
}
// Rotate the chest as a unit so the two arms and saber share the correction.
void CalibrateCut(Gob* body,int direction,float time,bool moving,bool walking,bool backLeft) {
    if(!IsSourceBody(body)) return;
    float roll=0, lift=0;
    switch(direction) {
    case 3: roll=moving?(walking?8.f:22.f):0.f; break; // Flatten the left-to-right clip.
    case 5: roll=moving?-13.f:0.f; lift=moving?10.f:6.f; break; // Raise the rising clips.
    case 6:
        // The swapped clip sweeps more steeply over the leftward gait.
        if(moving) roll=backLeft?(walking?0.f:5.f):14.f;
        lift=moving?10.f:6.f;
        break;
    default: return;
    }
    float start=hitWindows[direction][0], end=hitWindows[direction][1];
    float blend=std::max(0.f,std::min(1.f,std::min(time/start,1.f-(time-end)/.18f)));
    blend=blend*blend*(3-2*blend);
    if(blend==0) return;

    Part* chest=body->GetPart(const_cast<char*>("torsoUpr_g"));
    Vector position;
    Quaternion orientation;
    chest->GetWorld(position,orientation);
    constexpr float radiansPerDegree=3.1415926536f/180.f;
    Quaternion facing=normalize(body->m_qOrientation);
    Quaternion correction=Quaternion(facing.xdir(),lift*blend*radiansPerDegree)*
        Quaternion(facing.ydir(),roll*blend*radiansPerDegree);
    SetWorldOrientation(chest,normalize(correction*orientation));
}

void RefineRunPose(Gob* body,bool backward,float delta,PosePlayback& playback=playerPlayback) {
    const auto& lastPose=playback.lastPose;
    const bool blendPending=playback.blendPending;
    Part* pelvis=body->GetPart(const_cast<char*>("pelvis_g"));
    Part* chest=body->GetPart(const_cast<char*>("torsoUpr_g"));
    // These thresholds retain the planted portion of the calibrated run clips.
    const float liftStart=backward?.003f:-.04f;
    const float lift=std::max(0.f,pelvis->m_vLocalPosition.z-liftStart);
    const float ramp=std::min(1.f,lift/.02f);
    pelvis->m_vLocalPosition.z-=lift*(backward?.50f:.55f)*ramp*ramp*(3-2*ramp);
    // The backward clip also has a sharp landing key. Ease that last drop
    // with a small upward allowance so the correction can't push a sole down.
    if(backward && !blendPending && delta>0.f) {
        for(const BodyPose& previous:lastPose) {
            if(previous.part!=pelvis) continue;
            const float drop=previous.position.z-pelvis->m_vLocalPosition.z;
            pelvis->m_vLocalPosition.z+=std::min(.022f,std::max(0.f,drop-2.1f*delta));
            break;
        }
    }

    Vector position;
    Quaternion chestOrientation;
    chest->GetWorld(position,chestOrientation);
    for(bool left:{false,true}) {
        Part* upper=body->GetPart(const_cast<char*>(left?"lbicep_g":"rbicep_g"));
        Part* elbow=body->GetPart(const_cast<char*>(left?"Lforearm_g":"Rforearm_g"));
        Vector shoulderPosition,elbowPosition;
        Quaternion shoulderOrientation,unused;
        upper->GetWorld(shoulderPosition,shoulderOrientation);
        elbow->GetWorld(elbowPosition,unused);
        const Vector outward=chestOrientation.xdir()*(left?-1.f:1.f);
        const Vector arm=normalize(elbowPosition-shoulderPosition);
        const Vector closer=normalize(arm-outward*(.38f*std::max(0.f,dot(arm,outward))));
        SetWorldOrientation(upper,normalize(RotationArc(arm,closer)*shoulderOrientation));
        if (backward && left) {
            Part* wrist = body->GetPart(const_cast<char*>("Lhand_g"));
            if (!wrist) continue;
            Vector wristPosition;
            Quaternion elbowOrientation;
            elbow->GetWorld(elbowPosition, elbowOrientation);
            wrist->GetWorld(wristPosition, unused);
            const Vector forearm = normalize(wristPosition - elbowPosition);
            const float inward = -dot(forearm, outward);
            if (inward <= 0.f) continue;
            // Ease the inward part of the swing away from the ribs. The wrist
            // and forearm twist helper follow the elbow's rotation.
            const float correction = .20f * std::min(1.f, inward / .35f);
            const Vector cleared = normalize(forearm + outward * correction);
            SetWorldOrientation(elbow, normalize(RotationArc(forearm, cleared) * elbowOrientation));
            if (std::getenv("K1_ENGAGE_RIG_DIAG")) {
                Vector adjusted;
                wrist->GetWorld(adjusted, unused);
                fprintf(stderr, "K1_BACKRUN_FOREARM outward=%g length_error=%g\n",
                    dot(adjusted - wristPosition, outward),
                    std::fabs(magnitude(adjusted - elbowPosition) - magnitude(wristPosition - elbowPosition)));
            }
        }
    }
}

constexpr float guardBlendDuration=.24f;
void PrepareGuardBlend(PosePlayback& playback,bool guarding,bool relaxed) {
    if(!playback.guardKnown) {
        playback.guardKnown=true;playback.guardHeld=guarding;
    } else if(playback.guardHeld!=guarding) {
        playback.guardHeld=guarding;playback.guardTransition=relaxed;
        playback.blendPending=true;
    } else if(playback.blendPending || !relaxed) playback.guardTransition=false;
}
void SmoothBody(Gob* body,float delta,bool twoHanded,bool nativeIdle=false,PosePlayback& playback=playerPlayback) {
    auto &lastGrip=playback.lastGrip,&gripFrom=playback.gripFrom,&gripCorrection=playback.gripCorrection;
    auto &hiltCorrection=playback.hiltCorrection,&leftCorrection=playback.leftCorrection;
    auto &lastPose=playback.lastPose,&blendFrom=playback.blendFrom;
    auto& rotationFrom=playback.rotationFrom;
    auto& sourceGapCorrection=playback.sourceGapCorrection;
    auto& sourceContactError=playback.sourceContactError;
    auto& blendPending=playback.blendPending;
    auto& blendTime=playback.blendTime;
    bool anatomical=body->GetPart(const_cast<char*>("torsoMid_g"))!=nullptr;
    const bool sourceRig=body->GetPart(const_cast<char*>("rradiusX"))!=nullptr;
    const float ordinaryDuration=sourceRig?.14f:(anatomical?.20f:.15f);
    float duration=(playback.guardTransition || playback.stanceTransition)?guardBlendDuration:ordinaryDuration;
    GripPose target=ReadGrip(body);
    Vector rawSourceGap=Rotate(Inverse(target.hiltQ),target.left-target.hilt);
    auto traceGrip=[&](const char* stage,const GripPose& p) {
        if(sourceRig && rigDiagnosticState>=1000 && std::getenv("K1_ENGAGE_RIG_DIAG") &&
           (!strcmp(stage,"source") || !strcmp(stage,"body_blend") || !strcmp(stage,"final"))) {
            auto dump=[&](auto&& self,Part* part)->void {
                if(part->m_pNode->m_pTree!=body->m_pModel) return;
                const Vector& v=part->m_vLocalPosition;const Quaternion& q=part->m_qLocalOrientation;
                if(strstr(part->m_pNode->TranslateNodeName(),"bicep") || strstr(part->m_pNode->TranslateNodeName(),"forearm") || strstr(part->m_pNode->TranslateNodeName(),"hand"))
                    fprintf(stderr,"K1_RIG_STAGE sequence=%d frame=%d stage=%s bone=%s pose=%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",rigDiagnosticState-1000,rigDiagnosticFrame,stage,part->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);
                for(int i=0;i<part->m_nChildren;++i) self(self,part->m_paChildren[i]);
            };dump(dump,body->m_pPart);
        }
        if(rigDiagnosticState>=1000 && std::getenv("K1_ENGAGE_RIG_DIAG"))
            fprintf(stderr,"K1_RIG_BLEND sequence=%d frame=%d stage=%s hilt=%.9g,%.9g,%.9g center=%.9g,%.9g,%.9g guide=%.9g,%.9g,%.9g,%.9g\n",rigDiagnosticState-1000,rigDiagnosticFrame,stage,p.hilt.x,p.hilt.y,p.hilt.z,p.center.x,p.center.y,p.center.z,p.rightGuide.w,p.rightGuide.x,p.rightGuide.y,p.rightGuide.z);
    };
    traceGrip("source",target);
    if(twoHanded && !sourceRig) target.left=target.hilt-Rotate(target.hiltQ,Vector(0,0,.11f));
    if(blendPending) {
        blendFrom=lastPose;gripFrom=lastGrip;blendTime=0;blendPending=false;
        // A rebuilt body has no prior pose/grip to blend from. Its empty grip
        // must not pull the first attack's two hands toward a zero-length gap.
        if(sourceRig) sourceGapCorrection=blendFrom.empty()?Vector():
            Rotate(Inverse(gripFrom.hiltQ),gripFrom.left-gripFrom.hilt)-rawSourceGap;
        if(anatomical) {
            hiltCorrection=InitialPointCorrection(gripFrom.hilt,gripFrom.center,target.hilt,target.center);
            leftCorrection=InitialPointCorrection(gripFrom.left,gripFrom.leftCenter,target.left,target.leftCenter);
            rotationFrom.clear();
            for(const BodyPose& old:blendFrom)
                rotationFrom.push_back(normalize(old.orientation*Inverse(normalize(old.part->m_qLocalOrientation))));
            gripCorrection.hiltQ=normalize(gripFrom.hiltQ*Inverse(target.hiltQ));
            gripCorrection.leftQ=normalize(gripFrom.leftQ*Inverse(target.leftQ));
            gripCorrection.rightGuide=normalize(gripFrom.rightGuide*Inverse(target.rightGuide));
            gripCorrection.leftGuide=normalize(gripFrom.leftGuide*Inverse(target.leftGuide));
        }
    }
    // Appearance replacement may stall a frame. Do not spend most of the
    // new stance blend on that first displayed pose.
    const bool redRig=!_stricmp(body->m_pModel->m_szName,"k1jared") || !_stricmp(body->m_pModel->m_szName,"k1jadred") || !_stricmp(body->m_pModel->m_szName,"k1japurp") ||
        (body==loadedBody && WearableNativeResource(body->m_pModel->m_szName) && (RedMode() || PurpleMode()));
    const float blendStep=(playback.stanceTransition || redRig)?std::min(delta,.025f):delta;
    blendTime=std::min(duration,blendTime+std::max(0.f,blendStep));
    if(blendTime>=duration)playback.stanceTransition=false;
    float alpha=blendTime/duration;
    alpha=alpha*alpha*(3-2*alpha);
    float lowerAlpha=std::min(1.f,blendTime/ordinaryDuration);
    lowerAlpha=lowerAlpha*lowerAlpha*(3-2*lowerAlpha);
    Part* pelvis=body->GetPart(const_cast<char*>("pelvis_g"));Part* torso=body->GetPart(const_cast<char*>("torso_g"));
    Quaternion relative=normalize(Inverse(pelvis->m_qLocalOrientation)*torso->m_qLocalOrientation);
    const BodyPose* oldPelvis=nullptr;const BodyPose* oldTorso=nullptr;
    for(const BodyPose& old:blendFrom) {
        if(old.part==pelvis) oldPelvis=&old;
        if(old.part==torso) oldTorso=&old;
    }
    if(alpha<1) for(size_t i=0;i<blendFrom.size();++i) {
        const BodyPose& old=blendFrom[i];
        bool upper=false;
        for(Part* p=old.part;p && !upper;p=p->m_pParent)upper=p==torso;
        const float jointAlpha=playback.guardTransition && !upper?lowerAlpha:alpha;
        old.part->m_vLocalPosition=old.position*(1-jointAlpha)+old.part->m_vLocalPosition*jointAlpha;
        // Fade one initial correction over the moving source trajectory. A fresh
        // shortest path to a moving endpoint can reverse midway through a swing.
        old.part->m_qLocalOrientation=anatomical?
            normalize(Interpolate(rotationFrom[i],Quaternion(),jointAlpha)*old.part->m_qLocalOrientation):
            normalize(Interpolate(old.orientation,old.part->m_qLocalOrientation,jointAlpha));
    }
    if(!anatomical && alpha<1 && oldPelvis && oldTorso) {
        Quaternion previous=normalize(Inverse(oldPelvis->orientation)*oldTorso->orientation);
        torso->m_qLocalOrientation=normalize(pelvis->m_qLocalOrientation*Interpolate(previous,relative,alpha));
    }
    if(!nativeIdle) ConnectTorso(body);
    bool blending=alpha<1 && !blendFrom.empty();
    bool joined=twoHanded && (!blending || gripFrom.joined);
    bool filtered=sourceRig && joined && !blending && !lastPose.empty() && delta>0;
    if(filtered) {
        // Display damping for retargeted arms, independent of attack clocks.
        // Retain 25% at 60Hz; explicit clip transitions already have their blend.
        float follow=1-std::pow(.25f,std::min(delta,.1f)*60.f);
        Part* left=body->GetPart(const_cast<char*>("lcollar_g"));
        Part* right=body->GetPart(const_cast<char*>("rcollar_g"));
        for(const BodyPose& old:lastPose) {
            bool arm=false;
            for(Part* p=old.part;p;p=p->m_pParent) if(p==left || p==right) {arm=true;break;}
            if(arm) old.part->m_qLocalOrientation=normalize(Interpolate(old.orientation,old.part->m_qLocalOrientation,follow));
        }
    }
    if(blending) {
        if(anatomical) {
            GripPose current=ReadGrip(body);
            traceGrip("body_blend",current);
            target.hilt=CorrectPoint(hiltCorrection,target.hilt,target.center,current.center,alpha);
            target.left=CorrectPoint(leftCorrection,target.left,target.leftCenter,current.leftCenter,alpha);
        } else {
            target.hilt=gripFrom.hilt*(1-alpha)+target.hilt*alpha;
            target.left=gripFrom.left*(1-alpha)+target.left*alpha;
        }
        if(anatomical) {
            target.hiltQ=normalize(Interpolate(gripCorrection.hiltQ,Quaternion(),alpha)*target.hiltQ);
            target.leftQ=normalize(Interpolate(gripCorrection.leftQ,Quaternion(),alpha)*target.leftQ);
            target.rightGuide=normalize(Interpolate(gripCorrection.rightGuide,Quaternion(),alpha)*target.rightGuide);
            target.leftGuide=normalize(Interpolate(gripCorrection.leftGuide,Quaternion(),alpha)*target.leftGuide);
        } else {
            target.hiltQ=normalize(Interpolate(gripFrom.hiltQ,target.hiltQ,alpha));
            target.leftQ=normalize(Interpolate(gripFrom.leftQ,target.leftQ,alpha));
            target.rightGuide=normalize(Interpolate(gripFrom.rightGuide,target.rightGuide,alpha));
            target.leftGuide=normalize(Interpolate(gripFrom.leftGuide,target.leftGuide,alpha));
        }
    }
    traceGrip("target",target);
    // The source rig already has solved wrist, helper and independent tag tracks.
    // The legacy grip solver would overwrite those and force an unsupported gap.
    if(!sourceRig && (twoHanded || blending)) PlaceGrip(body,target,joined);
    Vector sourceGap=rawSourceGap+sourceGapCorrection*(1-alpha);
    if(sourceRig && joined && (blending || filtered)) {
        if(blending && (!_stricmp(body->m_pModel->m_szName,"k1jastaff") || !_stricmp(body->m_pModel->m_szName,"k1jadstaff"))) {
            sourceGap=Rotate(Inverse(target.hiltQ),target.left-target.hilt);
            StaffGripTransition(body,target);
        } else SourceGripTransition(body,sourceGap);
    }
    lastGrip=ReadGrip(body);lastGrip.joined=joined;
    sourceContactError=sourceRig && joined?magnitude(Rotate(Inverse(lastGrip.hiltQ),lastGrip.left-lastGrip.hilt)-sourceGap):0;
    if(sourceRig) traceGrip("final",lastGrip);
    if(sourceRig && rigDiagnosticState>=1000 && std::getenv("K1_ENGAGE_RIG_DIAG")) {
        Vector expected=joined?sourceGap:Rotate(Inverse(target.hiltQ),target.left-target.hilt);
        Vector actual=Rotate(Inverse(lastGrip.hiltQ),lastGrip.left-lastGrip.hilt);
        fprintf(stderr,"K1_RIG_SOURCE_GRIP sequence=%d frame=%d blending=%d error=%.9g expected=%.9g,%.9g,%.9g actual=%.9g,%.9g,%.9g\n",rigDiagnosticState-1000,rigDiagnosticFrame,blending,magnitude(actual-expected),expected.x,expected.y,expected.z,actual.x,actual.y,actual.z);
    }
    lastPose.clear();
    auto collect=[&](auto&& self,Part* part)->void {
        // Attached head/weapon models retain their own ownership and bind data.
        if(part->m_pNode->m_pTree!=body->m_pModel) return;
        lastPose.push_back({part,part->m_vLocalPosition,part->m_qLocalOrientation});
        for(int i=0;i<part->m_nChildren;++i) self(self,part->m_paChildren[i]);
    };collect(collect,body->m_pPart);
}
void ConnectedPoseProbe(Gob* body) {
    bool anatomical=body && body->GetPart(const_cast<char*>("torsoMid_g"));
    bool ok=body && (body->m_pAddInModel || anatomical);int frames=0,states=0;
    float seamError=0,minHead=10,maxStep=0,drift=0;
    FILE* poses=fopen(anatomical?"rig-connected.txt":"yellow-connected.txt","w");
    FILE* phases=anatomical?fopen("rig-connected-phase.txt","w"):nullptr;
    for(const char* base:{"j_idle","j_guard","j_walk","j_run","j_backwalk","j_backrun"}) {
        bool moving=strcmp(base,"j_idle") && strcmp(base,"j_guard");
        for(int attack=-1;ok && attack<7;++attack) for(float yaw:{-1.5707963268f,0.f,1.5707963268f}) for(float fraction:{.1f,.5f,.9f}) {
            body->PlayAnimation(nullptr,1,0,0);
            ok=body->PlayAnimation(const_cast<char*>(base),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,0);
            float time=0,length=0;body->GetAnimation(const_cast<char*>(base),&time,&length);
            body->SetAnimationTime(const_cast<char*>(base),length*fraction);
            const char* overlay=attack<0 ? (moving?"y_guard":nullptr) : (moving?attacks[attack]:fullAttacks[attack]);
            const char* reference=attack<0?"j_guard":fullAttacks[attack];
            if(overlay) {
                ok=ok && body->PlayAnimation(const_cast<char*>(overlay),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,0);
                body->GetAnimation(const_cast<char*>(overlay),&time,&length);time=length*fraction;
                body->SetAnimationTime(const_cast<char*>(overlay),time);
            }
            blendPending=true;std::vector<BodyPose> settled;
            for(int frame=0;ok && frame<14;++frame) {
                rigDiagnosticState=states;rigDiagnosticFrame=frame;
                std::vector<BodyPose> before=lastPose;
                poseProbeBody=body;poseProbeReference=overlay && overlay[0]=='y'?reference:nullptr;poseProbeTime=time;
                body->Animate(.000001f);poseProbeBody=nullptr;poseProbeReference=nullptr;
                TurnSpine(body,yaw,overlay && overlay[0]=='y'?reference:nullptr,time);SmoothBody(body,1.f/60,overlay || !strcmp(base,"j_guard"));
                for(BodyPose& p:lastPose) {p.position=p.part->m_vLocalPosition;p.orientation=p.part->m_qLocalOrientation;}
                ++frames;
                if(phases) fprintf(phases,"%d %d %d %d %.9g\n",states,frame,int(overlay || !strcmp(base,"j_guard")),lastGrip.joined,blendTime);
                Part* pelvis=body->GetPart(const_cast<char*>("pelvis_g"));Part* torso=body->GetPart(const_cast<char*>("torso_g"));
                Vector span=torso->m_vLocalPosition-pelvis->m_vLocalPosition;
                Vector rest=torso->m_pNode->m_vPosition-pelvis->m_pNode->m_vPosition;
                float error=anatomical?magnitude(torso->m_vLocalPosition-torso->m_pNode->m_vPosition):std::fabs(magnitude(span)-magnitude(rest));seamError=std::max(seamError,error);
                Vector head;Quaternion hq;body->GetPart(const_cast<char*>("headhook"))->GetWorld(head,hq);minHead=std::min(minHead,head.z);
                for(size_t i=0;i<lastPose.size();++i) {
                    const BodyPose& p=lastPose[i];const Quaternion& q=p.orientation;
                    float norm=q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z;
                    bool valid=std::isfinite(norm)&&std::fabs(norm-1)<.002f&&std::isfinite(p.position.x)&&std::isfinite(p.position.y)&&std::isfinite(p.position.z);
                    if(!valid) fprintf(stderr,"K1_ENGAGE_CONNECTED_INVALID state=%d frame=%d base=%s overlay=%s bone=%s norm=%g\n",states,frame,base,overlay?overlay:"none",p.part->m_pNode->TranslateNodeName(),norm);
                    ok=ok&&valid;
                    if(i<before.size() && before[i].part==p.part) {
                        const Quaternion& b=before[i].orientation;
                        float dot=std::fabs(q.w*b.w+q.x*b.x+q.y*b.y+q.z*b.z);
                        float step=2*std::acos(std::min(1.f,dot));
                        if(step>maxStep && std::getenv("K1_ENGAGE_RIG_DIAG"))
                            fprintf(stderr,"K1_RIG_MAX_STEP state=%d frame=%d base=%s overlay=%s bone=%s radians=%.9g\n",states,frame,base,overlay?overlay:"none",p.part->m_pNode->TranslateNodeName(),step);
                        maxStep=std::max(maxStep,step);
                    }
                    if(frame==12) settled.push_back(p);
                    if(frame==13 && i<settled.size()) {
                        Vector d=p.position-settled[i].position;drift=std::max(drift,magnitude(d));
                    }
                    if(poses && (frame==0 || frame==4 || frame==13 || std::getenv("K1_ENGAGE_RIG_DIAG")))
                        fprintf(poses,"%d %d %s %s %g %g %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",states,frame,base,overlay?overlay:"none",fraction,yaw,p.part->m_pNode->TranslateNodeName(),p.position.x,p.position.y,p.position.z,q.x,q.y,q.z,q.w);
                }
            }
            ++states;
        }
    }
    if(poses) fclose(poses);
    if(phases) fclose(phases);
    rigDiagnosticState=rigDiagnosticFrame=-1;
    // Exercise actual native culling. The old adapter modified an unevaluated
    // pose repeatedly; keep an upper overlay active so this tests that failure.
    body->PlayAnimation(nullptr,1,0,0);
    body->PlayAnimation(const_cast<char*>("j_run"),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,.2f);
    body->PlayAnimation(const_cast<char*>("y_top"),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,.3f);
    body->Animate(.000001f);
    Part* torso=body->GetPart(const_cast<char*>("torso_g"));Vector saved=torso->m_vLocalPosition;
    body->m_bAlwaysRendering=false;body->m_bInViewVolume=false;body->m_cForcePose=0;body->m_nDynamicLights=0;body->m_bAnimatedOnce=true;
    poseProbeBody=body;poseProbeReference="s_top";poseProbeTime=.3f;
    for(int i=0;i<30;++i) body->Animate(.001f);
    poseProbeBody=nullptr;poseProbeReference=nullptr;
    Vector difference=torso->m_vLocalPosition-saved;float culled=magnitude(difference);
    body->m_bAlwaysRendering=true;body->m_bInViewVolume=true;
    ok=ok&&seamError<.00001f&&minHead>.7f&&maxStep<.7f&&drift<.00001f&&culled<.00001f;
    fprintf(stderr,"K1_ENGAGE_CONNECTED_%s states=%d frames=%d seam=%g head_min=%g angular_step=%g drift=%g culled=%g\n",ok?"PASS":"FAIL",states,frames,seamError,minHead,maxStep,drift,culled);
    lastPose.clear();blendFrom.clear();blendPending=false;
}
void RigTimelineProbe(Gob* body) {
    body->PlayAnimation(nullptr,1,0,0);body->Reset();
    lastPose.clear();blendFrom.clear();blendPending=false;lastGrip=GripPose();
    const char* base=nullptr;const char* overlay=nullptr;float yaw=0;
    bool ok=true;float maxStep=0,maxGrip=0,maxSourceContact=0,minHead=10;int frames=0,sequence=0,activeFrames=0;
    FILE* poses=fopen("rig-timeline.txt","w");FILE* phases=fopen("rig-timeline-phase.txt","w");
    auto tick=[&](const char* nextBase,const char* nextOverlay,float time,bool twoHanded,bool active,float desiredYaw) {
        if(base!=nextBase || overlay!=nextOverlay) {
            if(overlay) body->PlayAnimation(const_cast<char*>(overlay),1,AnimRun::kAnimRemove,0);
            if(base!=nextBase) {
                if(base) body->PlayAnimation(const_cast<char*>(base),1,AnimRun::kAnimRemove,0);
                Overlay(body,nextBase,false);
            }
            if(nextOverlay) body->PlayAnimation(const_cast<char*>(nextOverlay),1,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,time);
            base=nextBase;overlay=nextOverlay;blendPending=true;
        }
        std::vector<BodyPose> before=lastPose;
        body->Animate(1.f/60);
        float change=5.f*3.1415926536f/180; yaw+=std::max(-change,std::min(change,desiredYaw-yaw));
        rigDiagnosticState=1000+sequence;rigDiagnosticFrame=frames;
        TurnSpine(body,yaw);SmoothBody(body,1.f/60,twoHanded);
        maxSourceContact=std::max(maxSourceContact,sourceContactError);
        rigDiagnosticState=-1;rigDiagnosticFrame=-1;
        if(active) {++activeFrames;ok=ok&&lastGrip.joined;}
        if(lastGrip.joined) {
            GripPose p=ReadGrip(body);float gap=magnitude(p.left-(p.hilt-Rotate(p.hiltQ,Vector(0,0,.11f))));maxGrip=std::max(maxGrip,gap);
        }
        Vector head;Quaternion q;body->GetPart(const_cast<char*>("headhook"))->GetWorld(head,q);minHead=std::min(minHead,head.z);
        for(size_t i=0;i<lastPose.size();++i) {
            const BodyPose& p=lastPose[i];
            if(i<before.size() && before[i].part==p.part) {
                float step=2*std::acos(std::min(1.f,std::fabs(dot(before[i].orientation,p.orientation))));
                if(step>maxStep) {maxStep=step;fprintf(stderr,"K1_RIG_TIMELINE_STEP sequence=%d frame=%d base=%s overlay=%s bone=%s radians=%g\n",sequence,frames,base,overlay?overlay:"none",p.part->m_pNode->TranslateNodeName(),step);}
            }
            if(poses) fprintf(poses,"%d %d %s %s %g %g %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",sequence,frames,base,overlay?overlay:"none",time,yaw,p.part->m_pNode->TranslateNodeName(),p.position.x,p.position.y,p.position.z,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w);
        }
        if(phases) fprintf(phases,"%d %d %d %d %d\n",sequence,frames,twoHanded,lastGrip.joined,active);
        ++frames;
    };
    for(int i=0;i<30;++i) tick("j_idle",nullptr,0,false,false,0);
    for(int mode=0;mode<4;++mode) for(int direction=0;direction<7;++direction) {
        ++sequence;
        float length=0;
        for(int i=0;i<body->m_pModel->m_animations.m_count;++i) {
            Animation* a=body->m_pModel->m_animations.m_pData[i];if(!strcmp(a->m_szName,fullAttacks[direction])) length=a->m_fLength;
        }
        for(float t=0;t<length-1.f/60;t+=1.f/60) {
            bool moving=mode==1 || (mode==2 && t<.35f) || (mode==3 && t>=.15f && t<.6f);
            const char* gait=mode==2 || (mode==3 && t>=.35f)?"j_walk":"j_run";
            bool active=t>=hitWindows[direction][0] && t<hitWindows[direction][1];
            tick(moving?gait:"j_idle",moving?attacks[direction]:fullAttacks[direction],t,true,active,moving?(direction%2?-1.0471975512f:1.0471975512f):0);
        }
        for(int i=0;i<22;++i) tick("j_idle",nullptr,0,false,false,0);
        for(int i=0;i<18;++i) tick("j_guard",nullptr,0,true,false,0);
        for(int i=0;i<18;++i) tick("j_walk","y_guard",0,true,false,.8f);
        for(int i=0;i<22;++i) tick("j_idle",nullptr,0,false,false,0);
    }
    if(poses) fclose(poses);if(phases) fclose(phases);
    ok=ok&&maxStep<.7f&&maxGrip<.003f&&minHead>.7f&&activeFrames>0;
    bool contact=std::isfinite(maxSourceContact) && maxSourceContact<.0002f;
    fprintf(stderr,"K1_RIG_SOURCE_CONTACT_%s maximum=%g\n",contact?"PASS":"FAIL",maxSourceContact);
    ok=ok && contact;
    fprintf(stderr,"K1_RIG_TIMELINE_%s sequences=%d frames=%d active_frames=%d angular_step=%g grip=%g head_min=%g\n",ok?"PASS":"FAIL",sequence,frames,activeFrames,maxStep,maxGrip,minHead);
    body->PlayAnimation(nullptr,1,0,0);lastPose.clear();blendFrom.clear();blendPending=false;
}
bool RigResourceRoundTrip(CSWGuiScene& scene) {
    // Component lifecycle only: native animation-base resource ownership. This
    // does not exercise creature appearance, attached head/items, menu or victory.
    int index=scene.m_aModels.m_count;
    Gob* oracle=static_cast<Gob*>(scene.AddModel(CExoString("pmbbl"),-1));
    bool ok=oracle!=nullptr;int cycles=0,compared=0;float maxPosition=0,maxWorldPosition=0;double maxAngle=0;
    if(oracle) {oracle->AttachToScene(nullptr);oracle->m_bAlwaysRendering=true;oracle->EnableForceAnimations();}
    FILE* trace=fopen("rig-resource-restoration.txt","w");ok=ok&&trace;
    {
        CSWCAnimBase owner;
        for(int cycle=0;ok && cycle<14;++cycle) {
            ok=owner.LoadModel(CResRef("k1jarig"),CSWCAnimBase::ANIM_MODEL_BODY)!=0;
            Gob* source=owner.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
            if(!ok || !source) {ok=false;break;}
            source->AttachToScene(nullptr);source->m_bAlwaysRendering=true;source->EnableForceAnimations();
            ok=source->PlayAnimation(const_cast<char*>(fullAttacks[cycle%7]),0,AnimRun::kAnimNoBlendIn,.3f)!=0;
            source->Animate(.000001f);lastPose.clear();blendFrom.clear();rotationFrom.clear();blendPending=true;
            SmoothBody(source,1.f/60,true);
            // Clear every cached Part reference before native ownership deletes it.
            lastPose.clear();blendFrom.clear();rotationFrom.clear();blendPending=false;
            owner.UnloadModel(CSWCAnimBase::ANIM_MODEL_BODY);
            ok=ok && owner.GetModel(CSWCAnimBase::ANIM_MODEL_BODY)==nullptr;
            ok=ok && owner.LoadModel(CResRef("pmbbl"),CSWCAnimBase::ANIM_MODEL_BODY)!=0;
            Gob* restored=owner.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
            if(!ok || !restored) {ok=false;break;}
            restored->AttachToScene(nullptr);restored->m_bAlwaysRendering=true;restored->EnableForceAnimations();
            ok=ok&&!restored->m_pAddInModel&&!restored->GetPart(const_cast<char*>("rradiusX"))&&!restored->GetPart(const_cast<char*>("torsoMid_g"));
            for(const char* clip:{"pause1","walk","run"}) for(float time:{0.f,.13f,.55f}) {
                oracle->PlayAnimation(nullptr,1,0,0);oracle->Reset();
                restored->PlayAnimation(nullptr,1,0,0);restored->Reset();
                ok=ok && oracle->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,time)!=0;
                ok=ok && restored->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,time)!=0;
                oracle->Animate(.000001f);restored->Animate(.000001f);int records=0;
                auto compare=[&](auto&& self,Part* p)->void {
                    Part* other=restored->GetPart(p->m_pNode->TranslateNodeName());
                    if(!other) {ok=false;return;}
                    Vector delta=other->m_vLocalPosition-p->m_vLocalPosition;
                    maxPosition=std::max(maxPosition,magnitude(delta));
                    const Quaternion& a=p->m_qLocalOrientation;const Quaternion& b=other->m_qLocalOrientation;
                    double ab=double(a.w)*b.w+double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z;
                    double aa=double(a.w)*a.w+double(a.x)*a.x+double(a.y)*a.y+double(a.z)*a.z;
                    double bb=double(b.w)*b.w+double(b.x)*b.x+double(b.y)*b.y+double(b.z)*b.z;
                    double angle=aa>0&&bb>0?2*std::acos(std::min(1.,std::fabs(ab)/std::sqrt(aa*bb))):999;
                    Vector wp,wo;Quaternion wq; p->GetWorld(wp,wq);other->GetWorld(wo,wq);
                    maxWorldPosition=std::max(maxWorldPosition,magnitude(wo-wp));maxAngle=std::max(maxAngle,angle);
                    ok=ok&&std::isfinite(angle);++records;++compared;
                    if(trace) fprintf(trace,"%d %s %.9g %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",cycle,clip,time,p->m_pNode->TranslateNodeName(),delta.x,delta.y,delta.z,angle,wo.x-wp.x,wo.y-wp.y,wo.z-wp.z);
                    for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
                };compare(compare,oracle->m_pPart);ok=ok&&records==61;
            }
            owner.UnloadModel(CSWCAnimBase::ANIM_MODEL_BODY);++cycles;
        }
    }
    if(trace) fclose(trace);if(oracle) scene.RemoveModel(index,1);
    ok=ok&&cycles==14&&compared==14*9*61&&maxPosition<.0002f&&maxWorldPosition<.0002f&&maxAngle<.002;
    fprintf(stderr,"K1_RIG_RESOURCE_%s cycles=%d comparisons=%d local_position=%g world_position=%g angle=%g actor_attachments_tested=0\n",ok?"PASS":"FAIL",cycles,compared,maxPosition,maxWorldPosition,maxAngle);
    return ok;
}
bool CollisionSkinProbe(CSWGuiScene& scene) {
    bool ok=K1SaberContact::Probe() && ContactTimingProbe();
    fprintf(stderr,"K1_RIG_SWEEP_MATH_%s\n",ok?"PASS":"FAIL");
    FILE* file=fopen("skin-collision-expected.txt","r");int cases=0,total=0,planned=0;float error=0;
    if(!file || fscanf(file,"%d",&planned)!=1) ok=false;
    for(int i=0;ok && i<planned;++i) {
        char model[32],clip[32];float t,yaw,scale,x,y,z;int count=0;
        if(fscanf(file,"%31s %31s %f %f %f %f %f %f %d",model,clip,&t,&yaw,&scale,&x,&y,&z,&count)!=9) {ok=false;break;}
        int index=scene.m_aModels.m_count;Gob* body=static_cast<Gob*>(scene.AddModel(CExoString(model),-1));
        if(!body) {ok=false;break;}
        body->AttachToScene(nullptr);body->m_bAlwaysRendering=true;body->EnableForceAnimations();
        body->SetPosition(Vector(x,y,z));body->SetOrientation(Quaternion(Vector(0,0,1),yaw));body->m_fObjectScale=scale;
        ok=body->PlayAnimation(clip,0,AnimRun::kAnimNoBlendIn,t)!=0;body->m_nLastAnimateTick=-1;body->Animate(.000001f);
        char previous[64]="",mesh[64];List<Vector> vertices;
        for(int j=0;ok && j<count;++j) {
            int vertex;
            if(fscanf(file,"%63s %d %f %f %f",mesh,&vertex,&x,&y,&z)!=5) {ok=false;break;}
            if(strcmp(mesh,previous)) {
                Part* part=body->GetPart(mesh);ok=part && part->AsPartSkin() && part->AsPartSkin()->GetSkinnedWorldVertices(vertices);strcpy(previous,mesh);
            }
            if(!ok || vertex<0 || vertex>=vertices.m_count) {ok=false;break;}
            float e=magnitude(vertices.m_pData[vertex]-Vector(x,y,z));error=std::max(error,e);++total;
            if(!std::isfinite(e) || e>.0002f) {fprintf(stderr,"K1_RIG_SKIN_DIFF model=%s clip=%s mesh=%s vertex=%d error=%g\n",model,clip,mesh,vertex,e);ok=false;}
        }
        scene.RemoveModel(index,1);++cases;
    }
    if(file) fclose(file);ok=ok && cases==12 && total>20000;
    fprintf(stderr,"K1_RIG_COLLISION_SKIN_%s cases=%d vertices=%d maximum=%g\n",ok?"PASS":"FAIL",cases,total,error);
    return ok;
}
void NpcAnimationProbe(CSWGuiScene& scene) {
    const int first=scene.m_aModels.m_count;
    Gob* bodies[2]={};PosePlayback playback[2];bool source=true,independent=true;
    for(int i=0;i<2;++i) {
        bodies[i]=static_cast<Gob*>(scene.AddModel(CExoString("k1jasith"),-1));
        if(!bodies[i]) {source=independent=false;break;}
        bodies[i]->AttachToScene(nullptr);bodies[i]->m_bAlwaysRendering=true;bodies[i]->EnableForceAnimations();
        bodies[i]->SetPosition(Vector(float(i)*3.f,0,0));
    }
    FILE* expected=fopen("npc-rig-expected.txt","r");int comparisons=0;
    char clip[32],bone[64],previous[32]="";float t,x,y,z,qx,qy,qz,qw,previousTime=-1;
    float maximumPosition=0,maximumAngle=0,maximumGrip=0;
    source=source && expected;
    while(source && fscanf(expected,"%31s %f %63s %f %f %f %f %f %f %f",clip,&t,bone,&x,&y,&z,&qx,&qy,&qz,&qw)==10) {
        if(strcmp(clip,previous) || t!=previousTime) {
            bodies[0]->PlayAnimation(nullptr,1,0,0);bodies[0]->Reset();
            source=bodies[0]->PlayAnimation(clip,0,AnimRun::kAnimNoBlendIn,t)!=0;
            bodies[0]->m_nLastAnimateTick=-1;bodies[0]->Animate(.000001f);
            strcpy(previous,clip);previousTime=t;
        }
        Vector position;Quaternion rotation;
        if(!bodies[0]->GetPartLocalPosition(bone,&position,&rotation)) {source=false;break;}
        const float pe=magnitude(position-Vector(x,y,z));
        const Quaternion target(qw,qx,qy,qz);
        const float angle=2*std::acos(std::min(1.f,std::fabs(dot(normalize(rotation),normalize(target)))));
        maximumPosition=std::max(maximumPosition,pe);maximumAngle=std::max(maximumAngle,angle);
        source=source && pe<.0002f && angle<.002f;++comparisons;
    }
    if(expected) fclose(expected);
    source=source && comparisons>100000;
    fprintf(stderr,"K1_NPC_RIG_SOURCE_%s comparisons=%d position=%g angle=%g\n",source?"PASS":"FAIL",comparisons,maximumPosition,maximumAngle);
    const char* clips[]={"j_idle","j_guard","s_top","j_walk","s_lr","s_tlbr","j_idle"};
    int selected[2]={-1,-1};int frames=0;
    for(int frame=0;independent && frame<280;++frame) {
        for(int i=0;i<2;++i) {
            const int other=1-i;
            const auto otherHistory=playback[other].lastPose;
            const float otherTime=playback[other].blendTime;
            const int next=(frame+(i?17:0))/40%7;
            if(next!=selected[i]) {
                bodies[i]->PlayAnimation(nullptr,1,0,0);
                independent=bodies[i]->PlayAnimation(const_cast<char*>(clips[next]),0,AnimRun::kAnimNoBlendIn,0)!=0;
                playback[i].blendPending=true;selected[i]=next;
            }
            bodies[i]->m_nLastAnimateTick=-1;bodies[i]->Animate(1.f/60);
            TurnSpine(bodies[i],0);
            SmoothBody(bodies[i],1.f/60,next==1 || next==2 || next==4 || next==5,false,playback[i]);
            maximumGrip=std::max(maximumGrip,playback[i].sourceContactError);
            independent=independent && playback[i].sourceContactError<.0002f &&
                playback[other].blendTime==otherTime && playback[other].lastPose.size()==otherHistory.size();
            for(const auto& pose:playback[i].lastPose) independent=independent && pose.part->m_pGob==bodies[i] &&
                std::isfinite(dot(pose.orientation,pose.orientation));
            for(size_t j=0;independent && j<otherHistory.size();++j) independent=
                magnitude(playback[other].lastPose[j].position-otherHistory[j].position)==0 &&
                dot(playback[other].lastPose[j].orientation,otherHistory[j].orientation)>.99999f;
        }
        ++frames;
    }
    fprintf(stderr,"K1_NPC_PLAYBACK_%s frames=%d grip=%g\n",independent?"PASS":"FAIL",frames,maximumGrip);
    while(scene.m_aModels.m_count>first) scene.RemoveModel(scene.m_aModels.m_count-1,1);
}
void RigCandidateProbe(CSWGuiScene& scene) {
    if(std::getenv("K1_ENGAGE_NPC_PROBE")) {NpcAnimationProbe(scene);return;}
    int index=scene.m_aModels.m_count;
    if(std::getenv("K1_ENGAGE_NATIVE_GRIP")) {
        Gob* native=static_cast<Gob*>(scene.AddModel(CExoString("pmbbl"),-1));bool ok=native!=nullptr;int clips=0;
        if(native) {
            native->AttachToScene(nullptr);native->m_bAlwaysRendering=true;native->EnableForceAnimations();
            for(const char* name:{"g1r1","g2r1","g3r1","g4r1"}) {
                native->PlayAnimation(nullptr,1,0,0);native->Reset();
                bool played=native->PlayAnimation(const_cast<char*>(name),0,AnimRun::kAnimNoBlendIn,0)!=0;
                native->Animate(.000001f);char path[64];snprintf(path,sizeof(path),"native-grip-%s.txt",name);FILE* file=fopen(path,"w");int records=0;
                if(file && played) {
                    auto dump=[&](auto&& self,Part* p)->void {
                        Vector v=p->m_vLocalPosition;Quaternion q=p->m_qLocalOrientation;
                        fprintf(file,"%s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",p->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);++records;
                        for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
                    };dump(dump,native->m_pPart);
                }
                if(file) fclose(file);ok=ok&&played&&records==61;++clips;
            }
            FILE* reference=fopen("native-arm-reference.txt","w");int samples=0,referenceClips=0;
            for(const char* name:{"pause1","walk","run","g2r1","g3r1","g4r1","c2a1","c2a2","c2a3","c2a4","c2a5","c3a1","c3a2","c3a3","c3a4","c3a5","c4a1","c4a2","c4a3","c4a4","c4a5"}) {
                native->PlayAnimation(nullptr,1,0,0);native->Reset();
                bool played=native->PlayAnimation(const_cast<char*>(name),0,AnimRun::kAnimNoBlendIn,0)!=0;
                float time=0,length=0;played=played&&native->GetAnimation(const_cast<char*>(name),&time,&length);
                if(!played || !reference) {ok=false;continue;}
                ++referenceClips;int frames=std::max(1,int(std::ceil(length*30)));
                for(int sample=0;sample<=frames;++sample) {
                    float t=length*sample/frames;native->SetAnimationTime(const_cast<char*>(name),t);native->Animate(.000001f);
                    auto dump=[&](auto&& self,Part* p)->void {
                        Vector v=p->m_vLocalPosition;Quaternion q=p->m_qLocalOrientation;
                        fprintf(reference,"%d 13 %s none %.9g 0 %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",samples,name,t,p->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);
                        for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
                    };dump(dump,native->m_pPart);++samples;
                }
            }
            if(reference) fclose(reference);ok=ok&&referenceClips==21&&samples>500;
            fprintf(stderr,"K1_NATIVE_ARM_REFERENCE clips=%d poses=%d\n",referenceClips,samples);
            scene.RemoveModel(index,1);
        }
        // The NPC retarget must use the NPC's actual native bind/grip,
        // not the player body's calibration. Keep the old oracle intact.
        Gob* npc=static_cast<Gob*>(scene.AddModel(CExoString("n_sithsoldier"),-1));
        bool npcOk=npc!=nullptr;int npcClips=0;
        if(npc) {
            npc->AttachToScene(nullptr);npc->m_bAlwaysRendering=true;npc->EnableForceAnimations();
            for(const char* name:{"bind","g1r1","g2r1","g3r1","g4r1"}) {
                npc->PlayAnimation(nullptr,1,0,0);npc->Reset();
                const bool bind=!strcmp(name,"bind");
                const bool played=bind || npc->PlayAnimation(const_cast<char*>(name),0,AnimRun::kAnimNoBlendIn,0)!=0;
                if(!bind) npc->Animate(.000001f);
                char path[96];snprintf(path,sizeof(path),"npc-native-sith-%s.txt",name);
                FILE* file=fopen(path,"w");int records=0;
                if(file && played) {
                    auto dump=[&](auto&& self,Part* p)->void {
                        const Vector v=p->m_vLocalPosition;const Quaternion q=p->m_qLocalOrientation;
                        fprintf(file,"%s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",p->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);++records;
                        for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
                    };dump(dump,npc->m_pPart);
                }
                if(file) fclose(file);
                npcOk=npcOk && played && records==67;++npcClips;
            }
            FILE* motion=fopen("npc-native-sith-motion.txt","w");int poses=0;
            for(const char* name:{"pause1","walk","run","g2r1","g3r1","g4r1","c2a1","c2a2","c2a3","c2a4","c2a5","c3a1","c3a2","c3a3","c3a4","c3a5","c4a1","c4a2","c4a3","c4a4","c4a5"}) {
                npc->PlayAnimation(nullptr,1,0,0);npc->Reset();
                bool played=npc->PlayAnimation(const_cast<char*>(name),0,AnimRun::kAnimNoBlendIn,0)!=0;
                float time=0,length=0;played=played && npc->GetAnimation(const_cast<char*>(name),&time,&length);
                if(!played || !motion) {npcOk=false;continue;}
                const int frames=std::max(1,int(std::ceil(length*30)));
                for(int frame=0;frame<=frames;++frame) {
                    const float t=length*frame/frames;npc->SetAnimationTime(const_cast<char*>(name),t);npc->Animate(.000001f);
                    auto dump=[&](auto&& self,Part* p)->void {
                        const Vector v=p->m_vLocalPosition;const Quaternion q=p->m_qLocalOrientation;
                        fprintf(motion,"%d 13 %s none %.9g 0 %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",poses,name,t,p->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);
                        for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
                    };dump(dump,npc->m_pPart);++poses;
                }
            }
            if(motion) fclose(motion);npcOk=npcOk && poses>500;
            fprintf(stderr,"K1_NPC_NATIVE_MOTION poses=%d\n",poses);
            scene.RemoveModel(index,1);
        }
        fprintf(stderr,"K1_NPC_NATIVE_GRIP_%s clips=%d\n",npcOk?"PASS":"FAIL",npcClips);
        ok=ok && npcOk;
        fprintf(stderr,"K1_NATIVE_GRIP_%s clips=%d\n",ok?"PASS":"FAIL",clips);return;
    }
    if(!CollisionSkinProbe(scene)) return;
    Gob* body=static_cast<Gob*>(scene.AddModel(CExoString("k1jarig"),-1));
    bool ok=body!=nullptr;float positionError=0;double angleError=0;int bindRecords=0;
    if(body) {body->AttachToScene(nullptr);body->m_bAlwaysRendering=true;body->EnableForceAnimations();}
    auto compare=[&](const char* bone,Vector expected,Quaternion eq) {
        Vector p;Quaternion q;
        if(!body->GetPartLocalPosition(const_cast<char*>(bone),&p,&q)) return false;
        float pe=magnitude(p-expected);double d=std::fabs(double(q.w)*eq.w+double(q.x)*eq.x+double(q.y)*eq.y+double(q.z)*eq.z);
        double a=2*std::acos(std::min(1.,d/std::sqrt(double(dot(q,q))*dot(eq,eq))));
        positionError=std::max(positionError,pe);angleError=std::max(angleError,a);
        if(pe>.0002f || !std::isfinite(a) || a>.002) {fprintf(stderr,"K1_RIG_CANDIDATE_DIFF bone=%s position=%g radians=%g\n",bone,pe,a);return false;}
        return true;
    };
    FILE* file=fopen("rig-bind-expected.txt","r");char line[512],bone[64],clip[32],previous[32]="";
    float t,x,y,z,qx,qy,qz,qw,last=-1;
    if(!file) ok=false;
    while(ok && fgets(line,sizeof(line),file)) {
        if(sscanf(line,"%63s %f %f %f %f %f %f %f",bone,&x,&y,&z,&qx,&qy,&qz,&qw)!=8) {ok=false;break;}
        ok=compare(bone,Vector(x,y,z),Quaternion(qw,qx,qy,qz));++bindRecords;
    }
    if(file) fclose(file);
    const bool armRig=body && body->GetPart(const_cast<char*>("rradiusX"));
    const bool legRig=body && body->GetPart(const_cast<char*>("rfemurX"));
    const bool motionRig=body && body->GetPart(const_cast<char*>("j_motion"));
    const int expectedNodes=motionRig?70:legRig?69:armRig?66:62;
    ok=ok && (!motionRig || legRig);
    ok=ok && (!legRig || (body->GetPart(const_cast<char*>("lfemurX")) && body->GetPart(const_cast<char*>("TorsoLegs"))));
    ok=ok&&bindRecords==expectedNodes;
    fprintf(stderr,"K1_RIG_BIND_%s records=%d position=%g angle=%g\n",ok?"PASS":"FAIL",bindRecords,positionError,angleError);
    std::vector<Part*> parts;
    if(body) {
        auto collect=[&](auto&& self,Part* p)->void {parts.push_back(p);for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);};collect(collect,body->m_pPart);
    }
    FILE* palette=fopen("rig-native-bind.txt","w");bool palettes=palette && parts.size()==size_t(expectedNodes);int skins=0;
    for(Part* p:parts) if(p->m_pNode->m_nFlags==NODE_SKIN) {
        auto* skin=static_cast<MdlNodeSkin*>(p->m_pNode);int used=0;++skins;
        for(int i=0;i<skin->m_nBoneCount && i<int(parts.size());++i) if(skin->m_paBoneMap[i]>=0) {
            ++used;Vector v=skin->m_lTBoneRefInv.m_pData[i];Quaternion q=skin->m_lQBoneRefInv.m_pData[i];
            if(palette) fprintf(palette,"%s %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",p->m_pNode->TranslateNodeName(),parts[i]->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);
        }
        palettes=palettes&&used<=16;fprintf(stderr,"K1_RIG_PALETTE mesh=%s used=%d\n",p->m_pNode->TranslateNodeName(),used);
    }
    if(palette) fclose(palette);
    palettes=palettes&&skins==(legRig?4:3);fprintf(stderr,"K1_RIG_PALETTE_%s\n",palettes?"PASS":"FAIL");
    if(ok && motionRig) {
        bool coldPass=true;int cold=0;float coldPosition=0,coldAngle=0;
        for(int i=-1;i<7;++i) {
            const char* reference=i<0?"j_guard":fullAttacks[i];const char* overlay=i<0?"y_guard":attacks[i];
            float at=i<0?.05f:(hitWindows[i][0]+hitWindows[i][1])*.5f;
            for(int j=0;j<body->m_pModel->m_animations.m_count;++j) {
                Animation* a=body->m_pModel->m_animations.m_pData[j];if(strcmp(a->m_szName,reference)) continue;
                MdlNode* n=FindPoseNode(a->m_pMdlNode,body->GetPart(const_cast<char*>("pelvis_g"))->m_nNodeID);
                if(n) for(int k=0;k<n->m_listControllers.m_count;++k) {
                    const auto& c=n->m_listControllers.m_pData[k];
                    if((c.m_target.m_nOffset & NewController::kTargetUnconverted) && ControllerTarget(c,body->GetPart(const_cast<char*>("pelvis_g")))==K2_OFFSETOF(Part,m_qLocalOrientation)) ++cold;
                }
            }
            auto sample=[&]() {
                body->PlayAnimation(nullptr,1,0,0);body->Reset();
                bool result=body->PlayAnimation(const_cast<char*>("j_walk"),0,AnimRun::kAnimNoBlendIn,.13f)!=0;
                result=body->PlayAnimation(const_cast<char*>(overlay),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,at)!=0 && result;
                body->m_nLastAnimateTick=-1;body->Animate(.000001f);
                return MotionSpine(body,body->GetPart(const_cast<char*>("pelvis_g")),body->GetPart(const_cast<char*>("torso_g")),body->GetPart(const_cast<char*>("torsoMid_g")),body->GetPart(const_cast<char*>("torsoUpr_g")),.785398f,.35f) && result;
            };
            coldPass=sample() && coldPass;std::vector<BodyPose> first;
            for(const char* name:{"pelvis_g","torso_g","torsoMid_g","torsoUpr_g"}) {
                Part* p=body->GetPart(const_cast<char*>(name));first.push_back({p,p->m_vLocalPosition,p->m_qLocalOrientation});
            }
            // Convert that reference through normal native playback, then repeat
            // exactly the same mixed pose. Cold and warm reads must agree.
            body->PlayAnimation(nullptr,1,0,0);body->Reset();
            coldPass=body->PlayAnimation(const_cast<char*>(reference),0,AnimRun::kAnimNoBlendIn,at)!=0 && coldPass;
            body->m_nLastAnimateTick=-1;body->Animate(.000001f);coldPass=sample() && coldPass;
            for(const auto& p:first) {
                coldPosition=std::max(coldPosition,magnitude(p.part->m_vLocalPosition-p.position));
                coldAngle=std::max(coldAngle,2*std::acos(std::min(1.f,std::fabs(dot(normalize(p.orientation),normalize(p.part->m_qLocalOrientation))))));
            }
        }
        coldPass=coldPass && cold==8 && coldPosition<.000002f && coldAngle<.002f;
        fprintf(stderr,"K1_RIG_COLD_REFERENCE_%s cases=8 initially_unconverted=%d position=%g angle=%g\n",coldPass?"PASS":"FAIL",cold,coldPosition,coldAngle);ok=ok && coldPass;
        body->PlayAnimation(nullptr,1,0,0);body->Reset();
    }
    file=fopen("rig-expected.txt","r");if(!file) ok=false;int poses=0,records=0;
    while(ok && fgets(line,sizeof(line),file)) {
        if(sscanf(line,"%31s %f %63s %f %f %f %f %f %f %f",clip,&t,bone,&x,&y,&z,&qx,&qy,&qz,&qw)!=10) {ok=false;break;}
        if(strcmp(clip,previous)||t!=last) {
            body->PlayAnimation(nullptr,1,0,0);
            ok=body->PlayAnimation(clip,0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,t)!=0;
            body->Animate(.000001f);strcpy(previous,clip);last=t;++poses;
        }
        ok=ok&&compare(bone,Vector(x,y,z),Quaternion(qw,qx,qy,qz));++records;
    }
    if(file) fclose(file);
    ok=ok&&palettes&&poses==2289&&records==(motionRig?119637:legRig?117348:armRig?114576:105420);
    fprintf(stderr,"K1_RIG_MODEL_%s poses=%d records=%d position=%g angle=%g\n",ok?"PASS":"FAIL",poses,records,positionError,angleError);
    if(std::getenv("K1_ENGAGE_RIG_SOURCE_ONLY")) {
        fprintf(stderr,"K1_RIG_SOURCE_%s nodes=%d poses=%d records=%d\n",ok?"PASS":"FAIL",expectedNodes,poses,records);
        if(body) scene.RemoveModel(index,1);
        return;
    }
    for(int view=0;ok && motionRig && view<2;++view) {
        FILE* expected=fopen(view?"rig-view-expected.txt":"rig-motion-expected.txt","r");
        if(view && !expected) continue; // optional reference in the aim-test profile
        bool pass=expected!=nullptr;int cases=0;double maximum=0;
        FILE* posed=view?fopen("rig-view-poses.txt","w"):nullptr;
        char base[32],overlay[32];float bt,ot,yaw,objectYaw,viewPitch=0;
        while(pass && fscanf(expected,"%31s %31s %f %f %f %f",base,overlay,&bt,&ot,&yaw,&objectYaw)==6) {
            if(view && fscanf(expected,"%f",&viewPitch)!=1) {pass=false;break;}
            body->PlayAnimation(nullptr,1,0,0);
            body->SetOrientation(Quaternion(Vector(0,0,1),objectYaw));
            pass=body->PlayAnimation(base,0,AnimRun::kAnimNoBlendIn,bt)!=0;
            if(strcmp(overlay,"none")) pass=pass && body->PlayAnimation(overlay,0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,ot)!=0;
            body->m_nLastAnimateTick=-1;body->Animate(.000001f);
            if(view) MotionSpine(body,body->GetPart(const_cast<char*>("pelvis_g")),body->GetPart(const_cast<char*>("torso_g")),body->GetPart(const_cast<char*>("torsoMid_g")),body->GetPart(const_cast<char*>("torsoUpr_g")),yaw,viewPitch);
            else TurnSpine(body,yaw);
            if(posed) {
                auto dump=[&](auto&& self,Part* p)->void {
                    if(p->m_pNode->m_pTree!=body->m_pModel) return;
                    const Vector& v=p->m_vLocalPosition;const Quaternion& q=p->m_qLocalOrientation;
                    fprintf(posed,"%d 0 %s %s %g %g %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",cases,base,overlay,ot,viewPitch,p->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);
                    for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
                };dump(dump,body->m_pPart);
            }
            for(int j=0;j<4;++j) {
                if(fscanf(expected,"%63s %f %f %f %f",bone,&qw,&qx,&qy,&qz)!=5) {pass=false;break;}
                Part* part=body->GetPart(bone);if(!part) {pass=false;break;}
                Quaternion q=part->m_qLocalOrientation,eq(qw,qx,qy,qz);
                double d=std::fabs(double(q.w)*eq.w+double(q.x)*eq.x+double(q.y)*eq.y+double(q.z)*eq.z);
                double a=2*std::acos(std::min(1.,d/std::sqrt(double(dot(q,q))*dot(eq,eq))));
                maximum=std::max(maximum,a);
                if(!std::isfinite(a) || a>=.002) fprintf(stderr,"K1_RIG_MOTION_DIFF case=%d base=%s overlay=%s bone=%s time=%g radians=%g actual=%g,%g,%g,%g expected=%g,%g,%g,%g\n",cases,base,overlay,bone,ot,a,q.w,q.x,q.y,q.z,eq.w,eq.x,eq.y,eq.z);
                pass=pass && std::isfinite(a) && a<.002;
            }
            ++cases;
        }
        if(expected) fclose(expected);
        if(posed) fclose(posed);
        pass=pass && cases==(view?1620:324);
        fprintf(stderr,"K1_RIG_%s_%s cases=%d maximum_angle=%g\n",view?"VIEW":"MOTION",pass?"PASS":"FAIL",cases,maximum);
        body->PlayAnimation(nullptr,1,0,0);body->SetOrientation(Quaternion());ok=ok && pass;
    }
    if(ok) ok=RigResourceRoundTrip(scene);
    if(ok) ConnectedPoseProbe(body);
    if(ok) RigTimelineProbe(body);
    if(body) scene.RemoveModel(index,1);
}
}
void K1EngageRestoreUtilityPose(Gob* body) {
    auto& blend=UtilityBlendFor(body);
    if (blend.body != body) return;
    for (const auto& joint : blend.joints) {
        const Quaternion& current = joint.part->m_qLocalOrientation;
        if (current.w == joint.shown.w && current.x == joint.shown.x &&
            current.y == joint.shown.y && current.z == joint.shown.z)
            joint.part->m_qLocalOrientation = joint.raw;
    }
}
bool ApplyStaffGuard(Gob* body,bool walking,const std::vector<BodyPose>& guard=nativeStaffGuardPose) {
    if(guard.empty()) return false;
    Part* torso=body->GetPart(const_cast<char*>("torso_g"));
    Part* pelvis=body->GetPart(const_cast<char*>("pelvis_g"));
    if(!torso || !pelvis) return false;
    Quaternion nativePelvis;
    for(const auto& pose:guard) if(pose.part==pelvis) nativePelvis=pose.orientation;
    for(const auto& pose:guard) {
        bool upper=!walking;
        for(Part* p=pose.part;p && !upper;p=p->m_pParent) upper=p==torso;
        if(!upper) continue;
        pose.part->m_vLocalPosition=pose.position;
        pose.part->m_qLocalOrientation=pose.orientation;
        // Retain the native model-space ready orientation while the JKA pelvis
        // turns/bobs beneath it. Do not apply the source's spine correction twice.
        if(walking && pose.part==torso) pose.part->m_qLocalOrientation=
            normalize(Inverse(pelvis->m_qLocalOrientation)*nativePelvis*pose.orientation);
        if(&guard==&nativeStaffGuardPose && !strcmp(pose.part->m_pNode->TranslateNodeName(),"headhook")) {
            static bool checked[2]={};
            if(!checked[walking]) {
                const auto& head=authoredGuardHeads[3][0];
                Vector expectedP(head.px,head.py,head.pz);Quaternion expectedQ=normalize(Quaternion(head.qw,head.qx,head.qy,head.qz));
                RetargetWearableAuthoredPose(body,pose.part,expectedP,expectedQ);
                float error=1-std::fabs(dot(normalize(pose.part->m_qLocalOrientation),expectedQ));
                fprintf(stderr,"K1_STAFF_AUTHORED_HEAD_%s walking=%d rotation_error=%g\n",error<.00001f?"PASS":"FAIL",walking,error);
                checked[walking]=true;
            }
        }
    }
    return true;
}
void ClearStaffStrafe(Gob* body,float yaw) {
    // The ordinary single-saber gait is retained, but its forearm points the
    // staff's second blade into a leg when the pelvis turns for strafing.
    // Measured against both native skinned legs over complete walk/run cycles.
    Part* arm=body->GetPart(const_cast<char*>("Rforearm_g"));
    if(!arm) return;
    const float weight=std::min(1.f,std::fabs(yaw)/.7853981634f);
    const float degrees=yaw>=0?-10.f:-15.f;
    Vector position;Quaternion orientation;arm->GetWorld(position,orientation);
    SetWorldOrientation(arm,normalize(Quaternion(normalize(body->m_qOrientation).ydir(),degrees*weight*.01745329252f)*orientation));
}

void K1EngagePose(Gob* body,float delta) {
    if(PoseNpcBody(body,delta)) return;
    if(body==poseProbeBody && poseProbeReference) {BridgeTorso(body,poseProbeReference,poseProbeTime);return;}
    if(!K1EngageOwnsAnimation(body)) { ApplyUtilityBlend(body, delta); return; }
    const bool spinChain=PlayerSpinChain();
    const bool nativeGait=!spinChain && ApplyYellowNativeGait(body,currentBase,currentOverlay!=nullptr,delta,PlayerNativeResource(body),playerPlayback,StaffMode(),DualMode());
    const char* blockReference=BlockReference(currentOverlay);
    if(currentOverlay && currentOverlay[0]=='y') {
        float time,length;
        if(body->GetAnimation(const_cast<char*>(currentOverlay),&time,&length)
            && BridgeTorso(body,blockReference?blockReference:(state.attacking?PlayerAttackClip(false):"j_guard"),time)) ++bridgedPoses;
    }
    float time=0,length=0;const char* reference=nullptr;
    if(currentOverlay && currentOverlay[0]=='y' && body->GetAnimation(const_cast<char*>(currentOverlay),&time,&length))
        reference=blockReference?blockReference:(state.attacking?PlayerAttackClip(false):"j_guard");
    const bool nativeIdle=!currentOverlay && !state.attacking && !state.Guarding() &&
        currentBase && !strcmp(currentBase,"j_idle") && !nativeIdlePose.empty();
    if(nativeIdle) {
        for(const BodyPose& pose:nativeIdlePose) {
            pose.part->m_vLocalPosition=pose.position;
            pose.part->m_qLocalOrientation=pose.orientation;
        }
    } else if(!spinChain) {
        TurnSpine(body,legsYaw,reference,time);
    }
    const bool staffWalk=StaffMode() && currentBase && strstr(currentBase,"walk");
    const bool staffReady=StaffMode() && !state.attacking && !blockReference && state.Guarding();
    if(staffReady) ApplyStaffGuard(body,staffWalk);
    const bool dualReady=DualMode() && !state.attacking && !blockReference && state.Guarding();
    std::vector<BodyPose> dualLowerBefore;
    const bool dualSteadyWalk=DualMode() && !state.attacking && !blockReference && currentBase && strstr(currentBase,"walk");
    if(dualSteadyWalk && std::getenv("K1_ENGAGE_DUAL_TEST")) {
        for(const char* name:{"pelvis_g","lthigh_g","lshin_g","lfoot_g","lfootT_g","rthigh_g","rshin_g","rfoot_g","rfootT_g"})
            if(auto* part=body->GetPart(const_cast<char*>(name)))dualLowerBefore.push_back({part,part->m_vLocalPosition,part->m_qLocalOrientation});
    }
    if(dualReady) ApplyStaffGuard(body,currentBase && strstr(currentBase,"walk"),nativeDualGuardPose);
    if(!nativeGait && IsSourceBody(body) && !currentOverlay && !state.attacking && !state.Guarding() && currentBase) {
        const bool backward=!strcmp(currentBase,"j_backrun");
        if(backward || !strcmp(currentBase,"j_run")) RefineRunPose(body,backward,delta);
    }
    if(!PurpleMode() && !BlueMode() && !RedMode() && !DualMode() && !StaffMode() && state.attacking && currentOverlay &&
       body->GetAnimation(const_cast<char*>(currentOverlay),&time,&length))
        CalibrateCut(body,swingDirection,time,currentOverlay[0]=='y',
            currentBase && strstr(currentBase,"walk"),swingBackLeft);
    if(StaffMode() && !state.attacking && !state.Guarding() && !blockReference && !currentOverlay &&
       currentBase && (strstr(currentBase,"walk") || strstr(currentBase,"run")))
        ClearStaffStrafe(body,legsYaw);
    // Source-rig grip smoothing follows the changing authored hand separation;
    // it does not pin both hands together during a released staff swing.
    // For duals, blend toward the final native grip/torso target in both guard
    // directions; post-blend assignment would snap on entry and release.
    if(dualSteadyWalk) StabilizeDualWalkingTorso(body,state.Guarding()?nativeDualGuardPose:nativeIdlePose);
    if(DualMode() && !state.attacking && !blockReference && currentBase &&
       (strstr(currentBase,"walk") || strstr(currentBase,"run")))ApplyDualLocomotionGrip(body);
    PrepareGuardBlend(playerPlayback,state.Guarding(),!state.attacking && !blockReference);
    // Select the saved headhook pose before the existing whole-body blend, so
    // entering/leaving guard and changing gait retain their normal transitions.
    if(!PurpleMode() && !BlueMode() && state.Guarding() && !state.attacking && !blockReference && currentBase &&
       (strstr(currentBase,"walk") || !strcmp(currentBase,"j_guard") || !strcmp(currentBase,"j_idle"))) {
        const int stance=DualMode()?2:StaffMode()?3:RedMode()?1:0;
        const auto& head=authoredGuardHeads[stance][strstr(currentBase,"walk")?1:0];
        if(auto* hook=body->GetPart(const_cast<char*>("headhook"))) {
            hook->m_vLocalPosition=Vector(head.px,head.py,head.pz);
            hook->m_qLocalOrientation=normalize(Quaternion(head.qw,head.qx,head.qy,head.qz));
            RetargetWearableAuthoredPose(body,hook,hook->m_vLocalPosition,hook->m_qLocalOrientation);
        }
        if(stance==0 && strstr(currentBase,"walk")) {
            for(const auto& edit:authoredYellowGuardWalk) {
                if(auto* joint=body->GetPart(const_cast<char*>(edit.name))) {
                    joint->m_vLocalPosition=Vector(edit.px,edit.py,edit.pz);
                    joint->m_qLocalOrientation=normalize(Quaternion(edit.qw,edit.qx,edit.qy,edit.qz));
                    RetargetWearableAuthoredPose(body,joint,joint->m_vLocalPosition,joint->m_qLocalOrientation);
                }
            }
        }
    }
    SmoothBody(body,delta,DualMode()?false:(StaffMode()?(staffReady || state.attacking || blockReference):(state.attacking || state.Guarding() || blockReference)),nativeIdle);
    if(dualReady && ContactTimingEnabled() && contactGuardClock.wall) {
        fprintf(stderr,"K1_CONTACT_GUARD_POSE wall_us=%lld world_us=%lld input_attacking=%d input_native=%d native_intervened=%d\n",ContactWallUs()-contactGuardClock.wall,
            g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime()-contactGuardClock.world,contactGuardClock.attacking,contactGuardClock.native,contactGuardClock.interrupted);
        contactGuardClock={};
    }
    for(const auto& before:dualLowerBefore) {
        // Entering walking blends from the prior gait. Preserve that transition;
        // compare the normal gait after its existing source-rig blend settles.
        if(playerPlayback.blendTime>=.14f) {
            dualWalkLowerPositionError=std::max(dualWalkLowerPositionError,magnitude(before.part->m_vLocalPosition-before.position));
            dualWalkLowerRotationError=std::max(dualWalkLowerRotationError,1.f-std::fabs(dot(normalize(before.part->m_qLocalOrientation),normalize(before.orientation))));
        }
    }
    if(!dualLowerBefore.empty() && playerPlayback.blendTime>=.14f)++dualWalkLowerSamples;
    if(auto* player=ServerActor()) ObserveBlockPose(body,playerPlayback,currentOverlay,player->m_objectId,
        currentBase && (strstr(currentBase,"walk") || strstr(currentBase,"run")));
    if(IsSourceBody(body)) {
        Part* parent=body->GetPart(const_cast<char*>("pelvis_g"));
        for(const char* name:{"torso_g","torsoMid_g","torsoUpr_g"}) {
            Part* joint=body->GetPart(const_cast<char*>(name));
            anatomicalChain=anatomicalChain && joint && joint->m_pParent==parent;
            if(joint) anatomicalEdgeError=std::max(anatomicalEdgeError,
                std::fabs(magnitude(joint->m_vLocalPosition)-magnitude(joint->m_pNode->m_vPosition)));
            parent=joint;
        }
        ++anatomicalPoses;
    }
    for(BodyPose& p:lastPose) {p.position=p.part->m_vLocalPosition;p.orientation=p.part->m_qLocalOrientation;}
    ApplyUtilityBlend(body, delta);
    ++poseRevision;
    if(std::getenv("K1_ENGAGE_RIG_DIAG")) {
        static FILE* poses=fopen("alignment-live-poses.txt","w");
        static FILE* clock=fopen("alignment-live-clock.txt","w");
        static float elapsed=0;elapsed+=delta;
        float time=0,length=0;if(currentOverlay) body->GetAnimation(const_cast<char*>(currentOverlay),&time,&length);
        if(clock) fprintf(clock,"%d %llu %.9g %.9g %.9g\n",attackSerial,poseRevision,elapsed,delta,aimPitch);
        if(poses) for(const auto& pose:lastPose) {
            const Vector& p=pose.part->m_vLocalPosition;const Quaternion& q=pose.part->m_qLocalOrientation;
            fprintf(poses,"%d %llu %s %s %g %g %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",attackSerial,poseRevision,currentBase?currentBase:"none",currentOverlay?currentOverlay:"none",time,legsYaw,pose.part->m_pNode->TranslateNodeName(),p.x,p.y,p.z,q.x,q.y,q.z,q.w);
        }
    }
}
void K1EngageAfterSceneRender(CAurScene* scene) {
    UpdateDismemberCorpses(scene);
    sceneMeleeContacts.clear();
    struct ContactMotionFrame {bool valid;~ContactMotionFrame(){if(valid)contactBodyRoots.swap(contactCurrentRoots);}} motionFrame{BeginContactBodyFrame(scene)};
    // Both sweeps must see the same posed scene before either applies damage.
    struct ResolveContacts {
        long long started=ContactTimingEnabled()?ContactWallUs():0;
        ~ResolveContacts(){
            const auto resolve=started?ContactWallUs():0;const auto count=sceneMeleeContacts.size();
            ResolveMeleeContacts(sceneMeleeContacts);sceneMeleeContacts.clear();
            if(started) {
                static long long previous=0;const auto ended=ContactWallUs();
                if(ended-started>20000 || (previous && started-previous>100000))fprintf(stderr,"K1_CONTACT_FRAME trace_us=%lld resolve_us=%lld frame_gap_us=%lld contacts=%zu npcs=%zu mode=%d paused=%d\n",
                    resolve-started,ended-resolve,previous?started-previous:0,count,npcPlayback.size(),int(state.mode),IsGamePaused());
                previous=started;
            }
        }
    } resolveContacts;
    TraceNpcBlades(scene);
    TraceNpcBolts(scene);
    if(!K1EngageActive() || !state.attacking || !currentOverlay) {hadBlade=false;return;}
    CSWCCreature* actor=Actor();Gob* body=loadedBody;
    if(!actor || !body || body->m_pScene!=scene || !K1EngageOwnsAnimation(body) || poseRevision==contactRevision) return;
    contactRevision=poseRevision;
    float time=0,length=0;
    if(body->GetAnimation(const_cast<char*>(currentOverlay),&time,&length)) {
        if(staffArcTrial>=0 && std::getenv("K1_ENGAGE_STAFF_ARC_TEST")) {
            auto* camera=g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera;
            const auto blades=StaffBlades(actor);
            for(size_t i=0;camera && i<blades.size();++i) {
                const auto& blade=blades[i];
                const Vector a=camera->MapToScreen(blade.base,true),b=camera->MapToScreen(blade.tip,true);
                const Vector center=camera->MapToScreen(actor->m_position+Vector(std::cos(aimYaw)*1.5f,std::sin(aimYaw)*1.5f,1.2f),true);
                fprintf(stderr,"K1_STAFF_ARC_SAMPLE trial=%d serial=%d direction=%d blade=%zu time=%.9g length=%.9g yaw=%g pitch=%g legs=%g world=%g,%g,%g,%g,%g,%g screen=%g,%g,%g,%g,%g,%g center=%g,%g,%g\n",
                    staffArcTrial,attackSerial,swingDirection,i,time,length,aimYaw,aimPitch,legsYaw,
                    blade.base.x,blade.base.y,blade.base.z,blade.tip.x,blade.tip.y,blade.tip.z,
                    a.x,a.y,a.z,b.x,b.y,b.z,center.x,center.y,center.z);
                const Vector forward(std::cos(aimYaw),std::sin(aimYaw),0),d=blade.tip-blade.base;
                const float denominator=dot(d,forward);
                const float u=std::fabs(denominator)>.00001f?dot(actor->m_position+forward*.9f-blade.base,forward)/denominator:-1.f;
                const Vector front=u>=0 && u<=1?camera->MapToScreen(blade.base+d*u,true):Vector(-1,-1,-1);
                Vector h,r,l;Quaternion q;
                body->GetPart(const_cast<char*>("rhand"))->GetWorld(h,q);
                body->GetPart(const_cast<char*>("rbicep_g"))->GetWorld(r,q);
                body->GetPart(const_cast<char*>("lbicep_g"))->GetWorld(l,q);
                const Vector sh=camera->MapToScreen(h,true),shoulder=l-r;
                fprintf(stderr,"K1_STAFF_ARC_GEOMETRY trial=%d serial=%d time=%.9g blade=%zu actor=%g,%g,%g hand=%g,%g,%g chest_yaw=%g front_u=%g front=%g,%g,%g hilt_screen=%g,%g,%g\n",
                    staffArcTrial,attackSerial,time,i,actor->m_position.x,actor->m_position.y,actor->m_position.z,h.x,h.y,h.z,
                    std::atan2(-shoulder.x,shoulder.y),u,front.x,front.y,front.z,sh.x,sh.y,sh.z);
            }
        }
        if(std::getenv("K1_ENGAGE_RIG_DIAG")) {
            Vector base,tip,h;Quaternion hand;
            auto* camera=g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera;
            if(camera && Blade(actor,base,tip)) {
                body->GetPart(const_cast<char*>("rhand"))->GetWorld(h,hand);
                Quaternion inverse=Inverse(normalize(body->m_qOrientation));
                Vector a=Rotate(inverse,base-body->m_vPosition),b=Rotate(inverse,tip-body->m_vPosition);
                Vector sa=camera->MapToScreen(base,true),sb=camera->MapToScreen(tip,true);
                Vector eye=Rotate(inverse,camera->GetPosition()-body->m_vPosition);
                float axisError=std::acos(std::max(-1.f,std::min(1.f,dot(normalize(tip-base),normalize(hand).zdir()))));
                fprintf(stderr,"K1_AIM_TRACE %d %d %s %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",
                    attackSerial,swingDirection,currentBase?currentBase:"none",time,legsYaw,keyShift,aimPitch,a.x,a.y,a.z,b.x,b.y,b.z,sa.x,sa.y,sa.z,sb.x,sb.y,sb.z,axisError,eye.x,eye.y,eye.z);
            }
        }
        TraceBlade(actor,time);
    }
    else hadBlade=false;
}
namespace {
NpcPlayback* FindNpcBody(Gob* body) {
    if(!body) return nullptr;
    for(auto& entry:npcPlayback) if(entry.second.body==body && entry.second.requested) return &entry.second;
    return nullptr;
}
bool NpcOwnsBody(Gob* body) {
    // Scene-less native pose samplers are not actor rigs. In particular, their
    // destructors call PlayAnimation while npcPlayback.clear() destroys entries;
    // do not re-enter that container from an unrelated helper Gob's teardown.
    if(!body || body->m_nExternalDataType!=CGameObject::OBJECT_TYPE_CREATURE || !body->m_pExternalData)return false;
    return K1EngageEnabled() && state.mode!=K1EngageState::Mode::Exploration && FindNpcBody(body);
}
bool SwapNpcRig(NpcPlayback& npc,bool engage) {
    const auto outgoing=CaptureUtilityPose(npc.body);npc.utilityBlend={};
    auto* owner=npc.owner;
    if(!owner || !owner->m_pCreatureAppearance) return false;
    if(!engage) {
        StopNpcMove(npc,owner->GetServerCreature());
        npc.attacking=npc.parrying=npc.closeGuard=false;
        npc.chainSwings=0;npc.threatSince=0;
    }
    CCAppearanceInfo appearance=LiveAppearance(owner);
    npc.body=nullptr;npc.pose={};npc.nativeIdle.clear();npc.staffGuard.clear();npc.previousBlades.clear();npc.contactBlades=0;npc.base=npc.overlay=nullptr;npc.bladeValid=false;npc.authoredHeadReported=0;
    npc.updated=npc.movingUntil=0;npc.legsYaw=0;npc.lastMotion=Vector();
    npc.previousPhysics=owner->GetServerCreature()->m_position;npc.physicsRemainder=.05f;npc.nextSteer=0;
    npc.position=npc.previousPhysics; // Native status/root motion is not a new source gait sample.
    npc.requested=engage;
    owner->m_pCreatureAppearance->m_nAppearanceId=0xffff;
    bool ok=owner->SetAppearance(&appearance,0x0f,1,0)!=0;
    auto* module=g_pAppManager->m_pClientExoApp->GetModule();
    if(module && module->m_pArea) owner->AttachToScene(module->m_pArea->m_pScene);
    Gob* fresh=owner->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
    ok=ok && fresh && (engage?IsSourceBody(fresh):!IsSourceBody(fresh));
    if(ok) {
        // Track the live native body while a utility/status owns animation, so
        // the same combat intent/timers can resume after its actual completion.
        npc.body=fresh;
        if(!engage && NativeUtilityQueued(owner->GetServerCreature()) && std::getenv("K1_ENGAGE_DARK_JEDI_TEST")) {
            auto delta=[](const std::vector<UtilityPoseJoint>& pose){float result=0;for(const auto& p:pose)result=std::max(result,2.f*std::acos(std::min(1.f,std::fabs(normalize(p.bindDelta).w))));return result;};
            fprintf(stderr,"K1_NPC_HANDOFF_RAW actor=%llu source_joints=%zu source_delta=%g native_delta=%g runs=%d\n",npc.id,outgoing.size(),delta(outgoing),delta(CaptureUtilityPose(fresh)),fresh->m_paAnimations.m_count);
        }
        if(engage) {
            fresh->PlayAnimation(nullptr,1,0,0);fresh->Reset();
            npc.facingYaw=std::atan2(owner->m_orientation.y,owner->m_orientation.x);
            npc.previousFacingYaw=npc.facingYaw;
            CacheNativeIdle(fresh,npc.nativeResource.c_str(),&npc.nativeIdle);
            if(npc.staff) CacheNativeIdle(fresh,npc.nativeResource.c_str(),&npc.staffGuard,"g3r1");
            if(npc.dual) CacheNativeIdle(fresh,npc.nativeResource.c_str(),&npc.staffGuard,"g4r1");
        }
        BeginUtilityBlend(fresh,outgoing,engage,npc.utilityBlend);
        if(!engage && NativeUtilityQueued(owner->GetServerCreature()) && std::getenv("K1_ENGAGE_DARK_JEDI_TEST")) {
            const auto seeded=CaptureUtilityPose(fresh);float error=0;int matched=0;
            for(const auto& a:outgoing)for(const auto& b:seeded)if(!strcmp(a.name,b.name)) {
                ++matched;error=std::max(error,2.f*std::acos(std::min(1.f,std::fabs(dot(normalize(a.bindDelta),normalize(b.bindDelta))))));
            }
            const bool pass=matched>=12 && error<.003f;
            fprintf(stderr,"K1_NPC_HANDOFF_SEED_%s actor=%llu joints=%d max_angle=%g\n",pass?"PASS":"FAIL",npc.id,matched,error);
            if(!pass)ok=false;
        }
    }
    fprintf(stderr,"K1_NPC_RIG_%s actor=%llu engage=%d model=%s\n",ok?"READY":"FAIL",npc.id,engage,
        fresh&&fresh->m_pModel?fresh->m_pModel->m_szName:"none");
    return ok;
}
void ReleaseNpcPlayback() {
    // HUD shutdown can follow server/client destruction. There are no live
    // actor rigs to restore once either owner has gone away.
    if(!g_pAppManager || !g_pAppManager->m_pServerExoApp || !g_pAppManager->m_pClientExoApp) {
        npcPlayback.clear();return;
    }
    for(auto& entry:npcPlayback) {
        auto* enemy=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(entry.first);
        auto& npc=entry.second;
        if(npc.requested && enemy && enemy->GetClientCreature()==npc.owner &&
            npc.owner->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1)==npc.body) SwapNpcRig(npc,false);
    }
    npcPlayback.clear();
}
bool NativeStatusAnimation(CSWSCreature* actor) {
    // K1's native knockdown handler sets commandability/animation but, unlike
    // K2, does not add a SETSTATE knockdown marker. The dual player must yield
    // its pose to that native action until its own removal/get-up completes.
    if(actor && actor==ServerActor() && DualMode() && !actor->m_bCommandable &&
       (actor->m_nAnimation==SERVER_ANIMATION_KNOCKDOWN_FROM_FRONT || actor->m_nAnimation==SERVER_ANIMATION_KNOCKDOWN_FROM_BEHIND))return true;
    return actor && (actor->m_nState!=EFFECT_STATE_NONE || actor->m_ambientAnimationState==14);
}
void StopNpcMove(NpcPlayback& npc,CSWSCreature* enemy) {
    (void)enemy;
    npc.hasMove=false;npc.navigation.stop();npc.groundVelocity=Vector();
}
K1JkaNav::Point JkaPoint(Vector v) {
    return {v.x/K1GroundMove::UnitScale,v.y/K1GroundMove::UnitScale,v.z/K1GroundMove::UnitScale};
}
Vector KotorPoint(K1JkaNav::Point p) {
    return Vector(p.x,p.y,p.z)*K1GroundMove::UnitScale;
}
// Collision/map binding only. It never asks K12 to plan or execute an AI path.
struct JkaWorld:K1JkaNav::World {
    CSWSCreature* actor;
    CSWSArea* area;
    int rejectedOrigins=0;
    float clearanceSpace=-1,clearanceHeight=-1;
    unsigned long long geometryQueries=0;
    explicit JkaWorld(CSWSCreature* a,CSWSArea* map=nullptr):actor(a),area(map?map:static_cast<CSWSArea*>(a->GetArea())){}
    int random(int low,int high)override {return low-1+g_pRules->RollDice(1,high-low+1);}
    K1JkaNav::Contact trace(K1JkaNav::Point a,K1JkaNav::Point b,bool ignoreCharacters)override {
        const Vector from=KotorPoint(a),to=KotorPoint(b),delta=to-from;
        auto* path=actor->m_pPathfindInfo;
        const auto savedIgnore=path->m_bIgnoreCrePath;
        auto* savedActive=area->m_pActivePathfindInfo;
        path->m_bIgnoreCrePath=1;
        CWalkHitInfo hit={};hit.m_hitObjectId=OBJECT_INVALID;hit.m_hitPosition=from;
        auto* server=g_pAppManager->m_pServerExoApp;
        auto geometry=[&](const Vector& end,CWalkHitInfo* info)->bool {
            ++geometryQueries;
            // Native walk-check includes Nonwalk for collision detection, but
            // its adjacency walker requires an origin on an adjacency-backed
            // face. Graph boundary points can otherwise select a Nonwalk face.
            auto* room=area->GetRoom(from,0,0);
            auto* mesh=room?room->m_pSurfaceMesh:nullptr;
            const int triangle=mesh?mesh->FindTriangle(from,1,end-from):-1;
            if(!mesh || triangle<0 || triangle>=mesh->m_nWalkableTriangleCount) {
                ++rejectedOrigins;
                if(std::getenv("K1_JKA_TRACE_QUERY") && rejectedOrigins<=8)
                    fprintf(stderr,"K1_JKA_ORIGIN_REJECT actor=%llu triangle=%d walkable=%d from=%.9g,%.9g,%.9g\n",
                        actor->m_objectId,triangle,mesh?mesh->m_nWalkableTriangleCount:0,from.x,from.y,from.z);
                return false;
            }
            if(std::getenv("K1_JKA_TRACE_QUERY") && geometryQueries<=200) fprintf(stderr,
                "K1_JKA_QUERY actor=%llu from=%g,%g,%g to=%g,%g,%g radius=%g height=%g\n",
                actor->m_objectId,from.x,from.y,from.z,end.x,end.y,end.z,path->m_fPersonalSpace,path->m_fHeight);
            // Bind the already loaded map explicitly. The server convenience
            // wrapper uses the module's current area; actor area assignment is
            // not complete yet during geometry loading.
            area->m_pActivePathfindInfo=path;
            return area->TestDirectLine(from,end,
                clearanceSpace<0?path->m_fPersonalSpace:clearanceSpace,clearanceHeight<0?path->m_fHeight:clearanceHeight,1,info)==1;
        };
        const bool clear=geometry(to,&hit);
        float first=1.f;
        K1JkaNav::Contact result;result.clear=clear;result.position=b;
        if(!clear) {
            // The walkmesh primitive can report an outside-room result without
            // a hit fraction. Recover its first safe boundary for gi.trace.
            float low=0,high=1;
            for(int i=0;i<10;++i) {
                const float middle=(low+high)*.5f;
                CWalkHitInfo test={};test.m_hitObjectId=OBJECT_INVALID;
                if(geometry(from+delta*middle,&test)) low=middle;else high=middle;
            }
            first=low;result.fraction=low;result.position=JkaPoint(from+delta*low);result.id=hit.m_hitObjectId;
            Vector normal=hit.m_hitNormal;
            if(dot(normal,normal)<.00001f) normal=dot(delta,delta)>.00001f?normalize(delta*-1.f):Vector(1,0,0);
            result.normal={normal.x,normal.y,normal.z};
        }
        path->m_bIgnoreCrePath=savedIgnore;area->m_pActivePathfindInfo=savedActive;
        if(ignoreCharacters || dot(delta,delta)<.0000001f) return result;
        // JKA gi.trace needs the closest hit, unlike K12's first-by-x creature
        // walk test. Bind native personal spaces as upright collision cylinders.
        const float length2=delta.x*delta.x+delta.y*delta.y;
        if(length2<.0000001f) return result;
        for(int i=0;i<area->m_aObjectsByX.m_count;++i) {
            auto* other=server->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);
            if(!other || other==actor || other->GetDead() || other->m_bIgnoreCrePath) continue;
            const Vector offset=from-other->m_position;
            if(std::fabs(offset.z)>std::max(path->m_fHeight,other->m_pPathfindInfo->m_fHeight)) continue;
            const float radius=path->m_fCrePersonalSpace+other->m_pPathfindInfo->m_fCrePersonalSpace+.01f;
            const float projection=offset.x*delta.x+offset.y*delta.y;
            const float c=offset.x*offset.x+offset.y*offset.y-radius*radius;
            const float determinant=projection*projection-length2*c;
            if(determinant<0) continue;
            const float fraction=c<=0?0.f:(-projection-std::sqrt(determinant))/length2;
            if(fraction<0 || fraction>first) continue;
            // Separating from an existing overlap must remain possible.
            if(c<=0 && projection>=0) continue;
            first=fraction;result.fraction=fraction;result.clear=false;result.character=true;result.id=other->m_objectId;
            const Vector contact=from+delta*fraction;
            Vector normal(contact.x-other->m_position.x,contact.y-other->m_position.y,0);
            if(dot(normal,normal)<.000001f) normal=normalize(delta*-1.f);else normal=normalize(normal);
            result.position=JkaPoint(contact);result.normal={normal.x,normal.y,normal.z};
            const float r=other->m_pPathfindInfo->m_fCrePersonalSpace;
            result.minimum=JkaPoint(other->m_position-Vector(r,r,.1f));
            result.maximum=JkaPoint(other->m_position+Vector(r,r,other->m_pPathfindInfo->m_fHeight));
            Vector velocity;
            const auto owned=npcPlayback.find(other->m_objectId);
            if(owned!=npcPlayback.end()) velocity=owned->second.groundVelocity;
            else if(other==ServerActor()) velocity=Vector(movement.x,movement.y,0);
            result.velocity=JkaPoint(velocity);
        }
        return result;
    }
};
std::shared_ptr<K1JkaNav::Navigator> BuildNavigation(JkaWorld& world) {
    std::vector<K1JkaNav::Point> points;
    auto add=[&](Vector p) {
        const auto point=JkaPoint(p);
        for(const auto& other:points) if(std::fabs(point.x-other.x)<3.f &&
            std::fabs(point.y-other.y)<3.f && std::fabs(point.z-other.z)<3.f) return;
        points.push_back(point);
    };
    for(int room=0;room<world.area->m_nRoomCount;++room) {
        auto* mesh=world.area->m_paRooms[room].m_pSurfaceMesh;
        if(!mesh) continue;
        for(int t=0;t<mesh->m_nWalkableTriangleCount;++t) {
            int ia,ib,ic;mesh->CSWCollisionMesh::GetTriangleVertices(t,&ia,&ib,&ic);
            Vector a,b,c;mesh->GetVertex(ia,&a);mesh->GetVertex(ib,&b);mesh->GetVertex(ic,&c);
            const Vector center=(a+b+c)*(1.f/3);
            add(center);
            // Keep connectivity anchors inside their owning walkable face.
            // One centimetre exceeds the native FindTriangle fallback nudge
            // (1mm) and the JKA/KotOR float round-trip at these map coordinates.
            for(const Vector edge:{(a+b)*.5f,(b+c)*.5f,(c+a)*.5f}) {
                const Vector inward=center-edge;const float distance=magnitude(inward);
                if(distance>.000001f) add(edge+inward*std::min(1.f,.01f/distance));
            }
        }
    }
    auto navigator=std::make_shared<K1JkaNav::Navigator>();
    const bool ok=navigator->build(points,world);
    fprintf(stderr,"K1_JKA_NAV_GRAPH_%s actor=%llu points=%d nodes=%d edges=%d source=Raven_AStar\n",
        ok?"PASS":"FAIL",world.actor->m_objectId,int(points.size()),navigator->nodes(),navigator->edges());
    return ok?navigator:nullptr;
}
// Campaign script spawns can arrive after area preload. Prepare the existing
// cache for their native cylinder before taking ownership; never move them on
// a graph validated for a smaller actor.
bool PrepareStoryNpcNavigation(CSWSCreature* actor) {
    auto* area=actor && actor->GetArea()?static_cast<CSWSArea*>(actor->GetArea()):nullptr;
    if(!area || !actor->m_pPathfindInfo)return false;
    auto cached=npcNavigators.find(area->m_objectId);
    const float space=actor->m_pPathfindInfo->m_fPersonalSpace,height=actor->m_pPathfindInfo->m_fHeight;
    if(cached!=npcNavigators.end() && cached->second.area==area && cached->second.space>=space && cached->second.height>=height)return true;
    JkaWorld world(actor);world.clearanceSpace=space;world.clearanceHeight=height;
    if(cached!=npcNavigators.end()){world.clearanceSpace=std::max(space,cached->second.space);world.clearanceHeight=std::max(height,cached->second.height);}
    auto graph=BuildNavigation(world);if(!graph)return false;
    PreparedNavigation prepared;prepared.graph=graph;prepared.area=area;
    prepared.space=world.clearanceSpace;prepared.height=world.clearanceHeight;prepared.generation=++navigationGeneration;
    npcNavigators[area->m_objectId]=prepared;
    fprintf(stderr,"K1_STORY_NPC_NAV_PREPARED actor=%llu space=%g height=%g phase=admission\n",actor->m_objectId,prepared.space,prepared.height);
    return true;
}
bool LoadNpcNavigation(NpcPlayback& npc,CSWSCreature* enemy,JkaWorld& world) {
    auto cached=npcNavigators.find(world.area->m_objectId);
    if(cached!=npcNavigators.end() && cached->second.area==world.area &&
        cached->second.space+.00001f>=enemy->m_pPathfindInfo->m_fPersonalSpace &&
        cached->second.height+.00001f>=enemy->m_pPathfindInfo->m_fHeight) {
        // A map cleared for a larger cylinder is conservative for a smaller
        // one. Never reuse it for a wider/taller actor than was validated.
        npc.navigator=cached->second.graph;
        fprintf(stderr,"K1_JKA_NAV_CACHE_HIT actor=%llu area=%llu use=%d space=%g/%g height=%g/%g\n",
            npc.id,world.area->m_objectId,++cached->second.uses,enemy->m_pPathfindInfo->m_fPersonalSpace,cached->second.space,
            enemy->m_pPathfindInfo->m_fHeight,cached->second.height);
        return true;
    }
    npc.navigationFailed=true;
    fprintf(stderr,"K1_JKA_NAV_CACHE_FAIL actor=%llu area=%llu reason=missing_or_unsupported_footprint runtime_build=0\n",npc.id,world.area->m_objectId);
    return false;
}
void MoveNpcWithJka(NpcPlayback& npc,CSWSCreature* enemy,float dt,long long now) {
    const bool walk=NpcWalking(npc);
    enemy->m_bRunning=!walk;
    {
        const float cap=EngageMoveSpeed(enemy,walk);
        const float speed=magnitude(npc.groundVelocity);
        if(speed>cap) npc.groundVelocity=npc.groundVelocity*(cap/speed);
        Vector steering=KotorPoint(npc.navigation.velocity);
        const float length=magnitude(steering);
        if(length>cap) npc.navigation.velocity=JkaPoint(steering*(cap/length));
    }
    if(dt<=0 || npc.navigationFailed) return;
    if(!npc.hasMove) {
        npc.physicsRemainder=std::min(.05f,npc.physicsRemainder+dt);
        const float alpha=npc.physicsRemainder/.05f;
        const Vector shown=npc.previousPhysics*(1-alpha)+enemy->m_position*alpha;
        npc.owner->SetPosition(shown);npc.owner->m_desiredPosition=shown;
        return;
    }
    JkaWorld world(enemy);
    if(!npc.navigator && !LoadNpcNavigation(npc,enemy,world)) return;
    auto& nav=npc.navigation;
    nav.position=JkaPoint(enemy->m_position);
    nav.radius=enemy->m_pPathfindInfo->m_fCrePersonalSpace/K1GroundMove::UnitScale;
    nav.height=enemy->m_pPathfindInfo->m_fHeight/K1GroundMove::UnitScale;
    nav.maxSpeed=EngageMoveSpeed(enemy,walk)/K1GroundMove::UnitScale;
    nav.enemy=npc.target;
    bool route=true;
    if(now>=npc.nextSteer) {
        nav.velocity=JkaPoint(npc.groundVelocity);
        route=npc.navigator->move(nav,JkaPoint(npc.goal),.08f/K1GroundMove::UnitScale,world,now/1000);
        npc.nextSteer=now+100000;
    }
    const Vector desired=KotorPoint(nav.velocity);
    const float yaw=npc.facingYaw,cap=nav.maxSpeed*K1GroundMove::UnitScale;
    const Vector facing(std::cos(yaw),std::sin(yaw),0);
    const Vector from=enemy->m_position;auto position=JkaPoint(from);
    auto wish=nav.velocity;
    if(npc.sourceJediCommands) {
        // Jedi_Move reverses the navigator's commands for retreat. Timed
        // strafes replace the right command, as in Jedi_ApplyTimers. Native
        // collision is still used by the source slide mover, never native AI.
        if(npc.commands.forward<0) {wish.x=-wish.x;wish.y=-wish.y;wish.z=-wish.z;}
        else if(!npc.commands.forward) wish=K1JkaNav::Point();
        if(npc.commands.right) {
            const Vector right(facing.y,-facing.x,0);
            const Vector forwardWish=KotorPoint(wish);
            Vector command=facing*dot(forwardWish,facing)+right*(cap*(npc.commands.right/127.f));
            const float length=magnitude(command);
            if(length>cap) command=command*(cap/length);
            wish=JkaPoint(command);
        }
    }
    if(npc.physicsRemainder==0) npc.previousPhysics=from;
    npc.physicsRemainder+=dt;
    bool stepped=false;
    while(npc.physicsRemainder>=.05f) {
        npc.previousPhysics=KotorPoint(position);
        auto velocity=JkaPoint(npc.groundVelocity);
        K1JkaNav::groundMove(position,velocity,wish,.05f,world,nav.maxSpeed);
        npc.groundVelocity=KotorPoint(velocity);
        npc.physicsRemainder-=.05f;stepped=true;
    }
    Vector next=KotorPoint(position);
    // Physical validation is separate from route/steering authority. Never
    // hand a failed JKA route back to a native AI movement action.
    const auto physical=world.trace(JkaPoint(from),JkaPoint(next),false);
    if(std::getenv("K1_ENGAGE_ENEMY_AI_TEST") && ++npc.navigationTicks%15==0)
        fprintf(stderr,"K1_JKA_MOTION actor=%llu position=%g,%g goal=%g,%g desired=%g,%g motor=%g,%g physical=%d hit=%llu normal=%g,%g avoid=%d side=%d path=%d\n",
            npc.id,from.x,from.y,npc.goal.x,npc.goal.y,desired.x,desired.y,npc.groundVelocity.x,npc.groundVelocity.y,
            physical.clear,physical.id,physical.normal.x,physical.normal.y,nav.avoidances,nav.lastAvoidSide,int(nav.path.size()));
    if(stepped) {
        next.z=g_pAppManager->m_pServerExoApp->ComputeHeight(world.area->m_resRef,next,1);
        enemy->SetPosition(next,1,1,0);
        // SetPosition updates spatial ownership, not native subarea scripts.
        // Preserve entry/exit and crossed-trigger events for combat pmove too.
        enemy->UpdateSubareasOnMoveTo(from,next,0,nullptr,0);
    }
    // JKA's NPC pmove runs twice per100ms think. Interpolate the displayed
    // body between those physics samples instead of snapping it at20Hz.
    const float alpha=npc.physicsRemainder/.05f;
    const Vector displayed=npc.previousPhysics*(1-alpha)+enemy->m_position*alpha;
    npc.owner->SetPosition(displayed);npc.owner->m_desiredPosition=displayed;
    npc.owner->SetSWAnimationSpeed(dot(npc.groundVelocity,facing),
        npc.groundVelocity.x*facing.y-npc.groundVelocity.y*facing.x);
    if(!route && std::getenv("K1_ENGAGE_ENEMY_AI_TEST")) fprintf(stderr,
        "K1_JKA_NAV_BLOCKED actor=%llu searches=%d avoidances=%d\n",npc.id,nav.searches,nav.avoidances);
}
bool NpcStepClear(CSWSCreature* enemy,const Vector& goal) {
    CWalkHitInfo hit={};hit.m_hitObjectId=OBJECT_INVALID;
    return g_pAppManager->m_pServerExoApp->TestDirectLine(enemy->m_objectId,
        static_cast<CSWSArea*>(enemy->GetArea())->m_resRef,enemy->m_position,goal,
        enemy->m_pPathfindInfo->m_fPersonalSpace,enemy->m_pPathfindInfo->m_fHeight,1,&hit)==1;
}
void NpcMoveGoal(NpcPlayback& npc,CSWSCreature* enemy,const Vector& goal,bool walk,long long now) {
    if(npc.hasMove && magnitude(goal-npc.goal)<.35f && npc.intentWalk==walk) return;
    if(now<npc.nextPath) return;
    if(!npc.hasMove && npc.physicsRemainder>=.05f) {npc.physicsRemainder=0;npc.previousPhysics=enemy->m_position;}
    npc.goal=goal;npc.intentWalk=walk;npc.hasMove=true;npc.navigation.active=true;
    npc.nextPath=now+200000;++npc.pathRequests;
    if(std::getenv("K1_ENGAGE_ENEMY_AI_TEST")) fprintf(stderr,
        "K1_NPC_NAV actor=%llu decision=%s goal=%g,%g,%g request=%d source=OpenJK_STEER native_move_queued=%d\n",
        npc.id,npc.decision,goal.x,goal.y,goal.z,npc.pathRequests,enemy->GetNodeById(AIACTION_MOVE_TO_POINT)!=nullptr);
}
void ThinkNpcCombat(NpcPlayback& npc,CSWSCreature* enemy,CSWSCreature* target,long long now,bool detected) {
    auto& force=sandboxActors.count(enemy->m_objectId)?sandboxForces[enemy->m_objectId]:arenaForce;
    if(now<npc.nextThink) return;
    const float sampleTime=npc.targetSampleAt?float(now-npc.targetSampleAt)/1000000.f:0;
    Vector velocity=sampleTime>0?(target->m_position-npc.targetSample)/sampleTime:Vector();
    if(magnitude(velocity)>10.f) velocity=Vector(); // teleport/re-entry is not target velocity
    npc.targetSample=target->m_position;npc.targetSampleAt=now;npc.nextThink=now+100000;
    auto* area=static_cast<CSWSArea*>(enemy->GetArea());
    Vector impact;unsigned long long blocker=OBJECT_INVALID;
    npc.visible=detected && area->ClearLineOfSight(enemy->m_position+Vector(0,0,1.5f),
        target->m_position+Vector(0,0,1.5f),&impact,&blocker,enemy->m_objectId,target->m_objectId,1,0);
    if(npc.visible) {npc.lastSeen=target->m_position;npc.lastSeenAt=now;}
    const Vector offset=target->m_position-enemy->m_position;
    const float distance=std::sqrt(offset.x*offset.x+offset.y*offset.y);
    const Vector toward=distance>.001f?Vector(offset.x/distance,offset.y/distance,0):enemy->m_orientation;
    const Vector side(-toward.y,toward.x,0);
    const float reach=K1EngageEnemyMeleeRange(enemy);
    const bool shot=npc.visible && K1EngageEnemyHasClearShot(enemy,target);
    const bool capable=enemy->m_bCommandable && (enemy->m_nAIState & 0x84)==0x84;
    // Local arena Force policy; JKA remains responsible for grounded melee and
    // movement. Powers execute through KotOR's native cost/effect/animation path.
    if(ArenaDarkJedi() && enemy->m_objectId==force.actor && capable && npc.visible &&
        !npc.ranged && !npc.attacking && !npc.parrying && !npc.reaction && !NativeUtilityQueued(enemy) &&
        ParryAttackReady(enemy->m_objectId,now) && !target->GetDead() && distance<=8.f && now>=force.next) {
        const int spell=enemy->GetCurrentHitPoints(0)<enemy->GetMaxHitPoints(0)/2?
            FORCE_POWER_DRAIN_LIFE:force.nextSpell;
        int slot=0;
        while(slot<enemy->m_pStats->m_numClasses &&
            (!enemy->m_pStats->m_classInfo[slot].m_knownSpells[0].Contains(spell) ||
             !enemy->m_pStats->GetSpellUsesLeft(spell,slot,0,0))) ++slot;
        if(slot<enemy->m_pStats->m_numClasses && enemy->AddCastSpellActions(spell,slot,0,0,0,
            target->m_position,target->m_objectId,0,0,1,0,0,0,-1,0xff,0)) {
            ClearClashEpisode(enemy->m_objectId);
            force.next=now+8000000;
            force.nextSpell=spell==FORCE_POWER_CHOKE?FORCE_POWER_FORCE_PUSH:FORCE_POWER_CHOKE;
            force.lastSpell=spell;
            force.queuedAggression=npc.jedi.aggression;
            ++force.queued;StopNpcMove(npc,enemy);npc.decision="native_force";
            fprintf(stderr,"K1_DARK_JEDI_FORCE_QUEUE actor=%llu target=%llu spell=%d count=%d force=%d aggression=%d roam=%lld\n",
                enemy->m_objectId,target->m_objectId,spell,force.queued,enemy->m_pStats->m_nCurrentForcePoints,
                npc.jedi.aggression,npc.jedi.roamTime);
            return;
        }
        force.next=now+1000000; // Recheck readiness; normal melee continues.
    }
    const char* previous=npc.decision;npc.decision="hold";
    npc.sourceJediCommands=false;
    bool move=false,walk=npc.parrying;Vector goal;
    auto sideGoal=[&](float retreat,float lateral,bool requireShot)->bool {
        for(int sign:{npc.strafeSide,-npc.strafeSide}) {
            const Vector candidate=enemy->m_position-toward*retreat+side*(lateral*sign);
            if(!NpcStepClear(enemy,candidate)) continue;
            if(requireShot) {
                // Test the future lane with the same ally/world rules as firing.
                Vector eyeHit;unsigned long long eyeBlock=OBJECT_INVALID;
                if(!area->ClearLineOfSight(candidate+Vector(0,0,1.5f),target->m_position+Vector(0,0,1.5f),
                    &eyeHit,&eyeBlock,enemy->m_objectId,target->m_objectId,1,0)) continue;
            }
            npc.strafeSide=sign;goal=candidate;return true;
        }
        return false;
    };
    if(!capable || npc.reaction || NativeUtilityQueued(enemy)) npc.decision="reaction_or_utility";
    else if(!npc.visible) {
        // Persist a last-seen goal, without tracking an unseen player's new position.
        // Search lifetime here is a local grounded prototype policy.
        if(npc.lastSeenAt && now-npc.lastSeenAt<5000000 && magnitude(npc.lastSeen-enemy->m_position)>.3f) {
            goal=npc.lastSeen;move=true;npc.decision="search_last_seen";
        }
    } else if(npc.ranged) {
        if(!shot && sideGoal(0,1.2f,true)) {move=true;npc.decision="gunner_find_shot";}
        else if(shot && distance<3.f && sideGoal(1.1f,.45f,true)) {move=true;npc.decision="gunner_make_space";}
        else if(!shot) {goal=npc.lastSeen-toward*3.f;move=true;npc.decision="gunner_hunt";}
        else npc.decision="gunner_stand_shoot";
    } else {
        struct JediBindings final:K1JkaAI::Bindings {
            CSWSCreature* actor;Vector facing;
            explicit JediBindings(CSWSCreature* value):actor(value),facing(value->m_orientation){}
            int random(int low,int high)override{return low+g_pRules->RollDice(1,high-low+1)-1;}
            bool moveDirClear(int forward,int right)override {
                // NPC_MoveDirClear binding: test grounded command direction
                // against the owned KotOR map. No native AI action is emitted.
                const Vector side(facing.y,-facing.x,0);
                Vector direction=facing*(forward/127.f)+side*(right/127.f);
                return NpcStepClear(actor,actor->m_position+direction*(32.f*K1GroundMove::UnitScale));
            }
        } bindings(enemy);
        K1JkaAI::JediInput input;
        input.now=now/1000;
        // Jedi_SetEnemyInfo: 300ms target prediction, saber-tip distance. The
        // existing retargeted reach replaces JKA's blade+actor dimensions.
        input.enemyDistance=int((magnitude(target->m_position+velocity*.3f-enemy->m_position)-reach)/K1GroundMove::UnitScale);
        input.parrying=npc.parrying;
        auto* targetBody=target->GetClientCreature();
        input.enemySaber=targetBody && targetBody->GetAnimationBase() &&
            (targetBody->GetAnimationBase()->m_nWeaponClass==CSWCAnimBase::WEAPON_CLASS_SINGLE_SABER ||
             DualNpc(target) ||
             (targetBody==Actor() && (StaffMode() || DualMode())));
        auto* targetWeapon=targetBody?g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(targetBody->m_aEquippedItemIds[4]):nullptr;
        input.enemySaberActive=targetWeapon && (targetWeapon->m_bPowered || SwordType(targetWeapon->m_baseItem));
        const auto targetNpc=npcPlayback.find(target->m_objectId);
        input.enemyShooting=target==ServerActor()?state.attacking:
            targetNpc!=npcPlayback.end() && targetNpc->second.attacking;
        npc.commands=K1JkaAI::mediumJedi(npc.jedi,input,bindings);
        npc.sourceJediCommands=true;
        walk=npc.commands.walking || npc.parrying;
        move=npc.commands.forward!=0 || npc.commands.right!=0;
        goal=target->m_position;
        npc.decision=npc.commands.forward<0?"jka_jedi_retreat":npc.commands.forward>0?"jka_jedi_advance":
            npc.commands.right?"jka_jedi_strafe":"jka_jedi_hold";
    }
    if(move) NpcMoveGoal(npc,enemy,goal,walk,now);
    else {StopNpcMove(npc,enemy);npc.intentWalk=walk;}
    if(previous!=npc.decision && std::getenv("K1_ENGAGE_ENEMY_AI_TEST"))
        fprintf(stderr,"K1_ENEMY_AI actor=%llu ranged=%d decision=%s distance=%g aggression=%d\n",
            npc.id,npc.ranged,npc.decision,distance,npc.jedi.aggression);
}
bool CommitClashParry(NpcPlayback& npc,long long now,int roll) {
    auto& p=meleePoise[npc.id];
    if(!ChooseClashParry(p,roll))return false;
    npc.parry="j_guard";npc.parryOverlay="y_guard";npc.parrying=true;
    // Wait for one incoming swing; two actors choosing guard cannot deadlock.
    npc.parryStart=now;npc.parryUntil=now+2000000;p.clashGuardUntil=npc.parryUntil;
    npc.nextAttack=npc.parryUntil;return true;
}
void ValidateClashEpisodes(long long now) {
    auto* server=g_pAppManager->m_pServerExoApp;
    for(auto& entry:meleePoise) {
        auto& p=entry.second;if(!p.clashes)continue;
        auto* actor=server->GetCreatureByGameObjectID(entry.first);
        auto* other=server->GetCreatureByGameObjectID(p.clashOpponent);
        auto npc=npcPlayback.find(entry.first);
        auto weapon=[&](int slot){auto* item=actor && actor->m_pInventory?actor->m_pInventory->GetItemInSlot(slot):nullptr;return item?item->m_objectId:OBJECT_INVALID;};
        const bool swinging=K1EngageOwnsPlayer(entry.first)?state.attacking:(npc!=npcPlayback.end() && npc->second.attacking);
        if(!actor || !other || actor->GetDead() || other->GetDead() || actor->GetArea()!=other->GetArea() ||
           NativeUtilityQueued(actor) || NativeStatusAnimation(actor) || !HasMeleePoise(actor) ||
           (actor!=ServerActor() && npc==npcPlayback.end()) ||
           weapon(CSWInventory::SLOT_RIGHTWEAPON)!=p.clashRight || weapon(CSWInventory::SLOT_LEFTWEAPON)!=p.clashLeft ||
           (npc!=npcPlayback.end() && npc->second.target!=p.clashOpponent) ||
           (p.clashSwing && !swinging) || (p.clashParry && p.clashGuardUntil && now>=p.clashGuardUntil)) {
            ClearClashEpisode(entry.first);continue;
        }
        if(swinging)p.clashSwing=true;
    }
}
void UpdateNpcPlayback() {
    auto* server=g_pAppManager->m_pServerExoApp;
    auto* player=ServerActor();
    for(auto it=npcPlayback.begin();it!=npcPlayback.end();) {
        auto* enemy=server->GetCreatureByGameObjectID(it->first);
        auto& npc=it->second;
        const bool same=enemy && enemy->GetClientCreature()==npc.owner &&
            npc.owner->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1)==npc.body &&
            npc.staff==StaffNpc(enemy) && npc.dual==DualNpc(enemy) && npc.red==RedNpc(enemy) && npc.blue==BlueNpc(enemy) &&
            (!NativeConfiguredNpc(enemy) || npc.weaponClass==NpcWeaponClass(enemy));
        const bool release=!same || !player || player->GetDead() || enemy->GetDead() ||
            enemy->m_bPassiveAttackBehaviour || (!enemy->m_bCommandable && !NativeStatusAnimation(enemy)) || enemy->m_bInParty ||
            !SandboxTarget(enemy,player) ||
            enemy->GetArea()!=player->GetArea() || state.mode==K1EngageState::Mode::Exploration;
        if(release) {
            // An armor/body update can recreate an owned native-catalog rig
            // through the appearance hook before this tick sees the new Gob.
            if(NativeConfiguredNpc(enemy) && npc.requested && enemy->GetClientCreature()==npc.owner) {
                auto* current=npc.owner->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
                if(IsSourceBody(current))npc.body=current;
                if(npc.body==current)SwapNpcRig(npc,false);
                it=npcPlayback.erase(it);continue;
            }
            if(std::getenv("K1_ENGAGE_ENEMY_AI_TEST")) {
                auto* current=enemy?enemy->GetClientCreature():nullptr;
                auto* body=current?current->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
                fprintf(stderr,"K1_NPC_OWNER_CHANGE actor=%llu server=%d same_client=%d same_body=%d source=%d model=%s mode=%d\n",
                    npc.id,enemy!=nullptr,current==npc.owner,same,IsSourceBody(body),
                    body&&body->m_pModel?body->m_pModel->m_szName:"none",int(state.mode));
            }
            if(same && npc.requested) SwapNpcRig(npc,false);
            it=npcPlayback.erase(it);
        } else if(npc.requested && (NativeStatusAnimation(enemy) || (NativeUtilityQueued(enemy) && !npc.attacking))) {
            SwapNpcRig(npc,false);
            fprintf(stderr,"K1_NPC_NATIVE_RELEASE actor=%llu state=%d reason=%s\n",npc.id,enemy->m_nState,
                NativeStatusAnimation(enemy)?"status_effect":"utility");
            ++it;
        } else ++it;
    }
    if(!player || !player->GetArea() || !K1EngageEnemyAIActive(player->m_objectId)) return;
    auto* area=static_cast<CSWSArea*>(player->GetArea());
    const long long now=server->GetWorldTimer()->GetSnapshotTime();
    for(int i=0;i<area->m_aObjectsByX.m_count;++i) {
        auto* enemy=server->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);
        auto* opponent=SandboxTarget(enemy,player);
        if(enemy && npcPlayback.find(enemy->m_objectId)!=npcPlayback.end() &&
            std::getenv("K1_ENGAGE_ENEMY_AI_TEST") && !enemy->HasTargetBeenDetected(player->m_objectId,1))
            fprintf(stderr,"K1_NPC_NO_DETECTION actor=%llu\n",enemy->m_objectId);
        if(!enemy || enemy==player || enemy->GetDead() || enemy->m_bInParty || enemy->m_bPlayerCharacter ||
            enemy->m_bPassiveAttackBehaviour || (!enemy->m_bCommandable && !NativeStatusAnimation(enemy)) ||
            !opponent) continue;
        if(NativeConfiguredNpc(enemy) && !NativeNpcMeleeEquipped(enemy))continue;
        auto* controllerPlayer=player;
        auto* target=opponent;
        const bool detected=enemy->HasTargetBeenDetected(target->m_objectId,1)!=0;
        auto* owner=enemy->GetClientCreature();
        if(!owner) continue;
        auto found=npcPlayback.find(enemy->m_objectId);
        if(found!=npcPlayback.end() && !found->second.requested) {
            if(!detected || NativeUtilityQueued(enemy) || NativeStatusAnimation(enemy)) continue;
            const auto* round=enemy->m_pCombatRound;
            // A completed spell round can retain its bookkeeping timer after
            // all native actions/one-shots end. It must not hold locomotion.
            // Real physical rounds keep their existing ownership boundary.
            if(round->m_bRoundStarted && !round->m_bSpellCastRound) continue;
            CompleteReleasedUtilityRound(enemy);
            auto& npc=found->second;
            if(!SwapNpcRig(npc,true)) {SwapNpcRig(npc,false);npcPlayback.erase(found);continue;}
        }
        if(found==npcPlayback.end()) {
            if(!detected || NativeUtilityQueued(enemy) || NativeStatusAnimation(enemy)) continue;
            Gob* body=owner->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
            const int weaponClass=owner->GetAnimationBase()?owner->GetAnimationBase()->m_nWeaponClass:0;
            const bool gunner=weaponClass==CSWCAnimBase::WEAPON_CLASS_SINGLE_BLASTER || weaponClass==CSWCAnimBase::WEAPON_CLASS_RIFLE;
            const char* rig=body && body->m_pModel?NpcRigResource(body->m_pModel->m_szName,enemy):nullptr;
            if(!rig ||
                body->m_pAddInModel || enemy->m_pCombatRound->m_bRoundStarted || (!Supported(owner) && !NativeNpcBlunt(owner) && !gunner && !StaffNpc(enemy) && !DualNpc(enemy))) continue;
            if(!g_pExoResMan->Exists(CResRef(rig),RESTYPE_MDL,static_cast<unsigned int*>(nullptr)) ||
                !g_pExoResMan->Exists(CResRef(rig),RESTYPE_MDX,static_cast<unsigned int*>(nullptr))) continue;
            if(std::getenv("K1_ENGAGE_STORY") && !PrepareStoryNpcNavigation(enemy))continue;
            ConfigureNpcPoise(enemy);
            auto& npc=npcPlayback[enemy->m_objectId];npc.id=enemy->m_objectId;npc.owner=owner;npc.position=enemy->m_position;
            npc.nativeResource=body->m_pModel->m_szName;
            npc.weaponClass=weaponClass;
            npc.health=enemy->GetCurrentHitPoints(0);npc.staff=StaffNpc(enemy);npc.dual=DualNpc(enemy);npc.red=RedNpc(enemy);npc.blue=BlueNpc(enemy);
            if(std::getenv("K1_ENGAGE_ENEMY_AI_TEST") && !gunner && !std::getenv("K1_JKA_DEFAULT_PROFILE_PROBE") &&
                !std::getenv("K1_ENGAGE_DARK_JEDI_TEST")) {
                // The combat/navigation fixture selects the source offensive
                // aggression state; normal play starts at the owned profile's3.
                // Source defensive ramp/timer boundaries are tested separately.
                npc.jedi.aggression=6;
                fprintf(stderr,"K1_JKA_JEDI_FIXTURE aggression=6 default_profile=3\n");
            }
            npc.facingYaw=std::atan2(enemy->m_orientation.y,enemy->m_orientation.x);
            npc.previousFacingYaw=npc.facingYaw;
            npc.ranged=gunner;npc.rifle=weaponClass==CSWCAnimBase::WEAPON_CLASS_RIFLE;
            // Finish the old physical action's movement handoff once. Future
            // movement belongs to this actor's persistent combat intent.
            if(auto* move=enemy->GetNodeById(AIACTION_MOVE_TO_POINT)) {
                const auto group=move->m_nGroupId;
                enemy->ClearAction(move,1);enemy->RemoveGroup(group);
                enemy->m_pPathfindInfo->ResetAtEndOfPath();
            }
            CompleteReleasedUtilityRound(enemy);
            if(!SwapNpcRig(npc,true)) {SwapNpcRig(npc,false);npcPlayback.erase(enemy->m_objectId);continue;}
            found=npcPlayback.find(enemy->m_objectId);
            // The scripted poise contact fixture holds attacks, including the
            // first update after native stun releases/recreates this controller.
            if(std::getenv("K1_ENGAGE_POISE_TEST") || std::getenv("K1_ENGAGE_CLASH_TEST") ||
                std::getenv("K1_ENGAGE_PARRY_TEST")) found->second.nextAttack=now+60000000;
        }
        auto& npc=found->second;
        if(npc.target!=target->m_objectId) {
            ClearClashEpisode(npc.id);
            if(npc.target!=OBJECT_INVALID){npc.attacking=npc.parrying=false;npc.chainSwings=0;npc.bladeValid=false;++npc.serial;npc.pose.blendPending=true;}
            npc.target=target->m_objectId;
        }
        const float dt=npc.updated?std::max(0.f,std::min(.1f,float(now-npc.updated)/1000000.f)):0;
        MaintainSaberCombat(owner);
        npc.health=enemy->GetCurrentHitPoints(0);
        // Weapon damage does not interrupt owned enemy swings or locomotion.
        // Native powers/status effects still yield through their ownership gate.
        const char* reaction=nullptr;
        if(reaction!=npc.reaction) {
            npc.reactionStarted=now;npc.reaction=reaction;
            fprintf(stderr,"K1_NPC_REACTION actor=%llu state=%d clip=%s\n",npc.id,enemy->m_nState,reaction?reaction:"none");
        }
        if(reaction) {npc.attacking=false;npc.parrying=false;npc.bladeValid=false;}
        float time=0,length=0;
        const bool attackFinished=npc.attacking && (!npc.overlay || !npc.body->GetAnimation(const_cast<char*>(npc.overlay),&time,&length) || time>=length-.015f);
        const Vector offset=target->m_position-enemy->m_position;
        const float distance=std::sqrt(offset.x*offset.x+offset.y*offset.y);
        const float attackRange=npc.ranged?enemy->MaxAttackRange(target->m_objectId,0,0):K1EngageEnemyMeleeRange(enemy);
        const bool closeGuard=!npc.ranged && detected && enemy->m_bCommandable &&
            (enemy->m_nAIState & 0x84)==0x84 &&
            distance<=attackRange+enemy->m_pPathfindInfo->m_fCrePersonalSpace+meleeWalkExtraDistance;
        if(closeGuard!=npc.closeGuard) {
            npc.closeGuard=closeGuard;
            fprintf(stderr,"K1_NPC_CLOSE_GUARD actor=%llu raised=%d distance=%g\n",npc.id,closeGuard,distance);
        }
        const bool npcThreat=target!=controllerPlayer && npcPlayback.count(target->m_objectId) && npcPlayback[target->m_objectId].attacking && distance<3.f && dot(enemy->m_orientation,offset)>=0;
        const bool threat=detected && !reaction && !npc.ranged && !npc.attacking && (target==controllerPlayer?K1EngageEnemyFacesSwing(enemy):npcThreat);
        if(!threat) npc.threatSince=0;
        else if(!npc.threatSince) npc.threatSince=now;
        auto& clash=meleePoise[npc.id];
        if(clash.clashes && !npc.attacking && !npc.ranged && detected && !NativeUtilityQueued(enemy) &&
           !NativeStatusAnimation(enemy) && enemy->m_bCommandable && (enemy->m_nAIState & 0x84)==0x84 && distance<=attackRange && now>=npc.nextAttack &&
           clash.clashDecision!=clash.clashes) {
            const int roll=g_pRules->RollDice(1,100);
            CommitClashParry(npc,now,roll);
            fprintf(stderr,"K1_CLASH_PARRY_DECISION actor=%llu count=%d chance=%d roll=%d parry=%d\n",npc.id,clash.clashes,ClashParryChance(clash),roll,clash.clashParry);
        }
        if(!clash.clashes && threat && !npc.parrying && now>=npc.parryReady && now-npc.threatSince>=90000 && (enemy->m_nAIState & 0x84)==0x84) {
            npc.parry="j_guard";npc.parrying=true;
            npc.parryOverlay="y_guard";
            npc.parryStart=now;npc.parryUntil=now+350000;npc.parryReady=now+800000;
            npc.nextAttack=std::max(npc.nextAttack,npc.parryUntil);
            fprintf(stderr,"K1_NPC_PARRY actor=%llu clip=%s\n",npc.id,npc.parry);
        }
        if(npc.parrying && now>=npc.parryUntil) npc.parrying=false;
        ThinkNpcCombat(npc,enemy,target,now,detected);
        if(std::getenv("K1_ENGAGE_ENEMY_AI_TEST") && !npc.attacking && now>=npc.nextAttack) {
            CWalkHitInfo hit={};hit.m_hitObjectId=OBJECT_INVALID;
            const Vector ahead=enemy->m_position+Vector(offset.x/distance,offset.y/distance,0)*.1f;
            const int direct=server->TestDirectLine(enemy->m_objectId,area->m_resRef,enemy->m_position,ahead,
                enemy->m_pPathfindInfo->m_fPersonalSpace,enemy->m_pPathfindInfo->m_fHeight,1,&hit);
            fprintf(stderr,"K1_NPC_ATTACK_PREDICATES actor=%llu distance=%g range=%g utility=%d detected=%d base=%s overlay=%s time=%g length=%g space=%g,%g target_space=%g,%g direct=%d blocker=%llu\n",
                npc.id,distance,attackRange,NativeUtilityQueued(enemy),
                enemy->HasTargetBeenDetected(target->m_objectId,1),npc.base?npc.base:"none",npc.overlay?npc.overlay:"none",time,length,
                enemy->m_pPathfindInfo->m_fPersonalSpace,enemy->m_pPathfindInfo->m_fCrePersonalSpace,
                target->m_pPathfindInfo->m_fPersonalSpace,target->m_pPathfindInfo->m_fCrePersonalSpace,direct,hit.m_hitObjectId);
        }
        if(distance>.01f && !reaction && npc.visible) {
            const float desired=std::atan2(offset.y,offset.x);
            // NPC_UpdateAngles uses the profile parameter inside its source
            // decay formula; yawSpeed is not a degrees-per-second rate.
            constexpr float degrees=180.f/3.1415926536f;
            npc.facingRemainder+=dt;
            while(npc.facingRemainder>=.05f) {
                npc.previousFacingYaw=npc.facingYaw;
                npc.facingYaw=K1JkaAI::updateYaw(npc.facingYaw*degrees,desired*degrees,npc.ranged?70.f:60.f)/degrees;
                npc.facingRemainder-=.05f;
            }
            const Vector facing(std::cos(npc.facingYaw),std::sin(npc.facingYaw),0);
            const float turn=std::atan2(std::sin(npc.facingYaw-npc.previousFacingYaw),std::cos(npc.facingYaw-npc.previousFacingYaw));
            const float presented=npc.previousFacingYaw+turn*(npc.facingRemainder/.05f);
            const Vector shown(std::cos(presented),std::sin(presented),0);
            enemy->SetOrientation(facing);owner->SetOrientation(shown);
            owner->SetDesiredWalkOrientation(shown);owner->SetDesiredStationaryOrientation(shown);
        }
        if(npc.ranged) MoveNpcWithJka(npc,enemy,dt,now);
        bool attackStarted=false;
        const bool canAttack=npc.visible && enemy->m_bCommandable && !reaction && !npc.parrying && ParryAttackReady(npc.id,now) && distance<=attackRange && !NativeUtilityQueued(enemy) &&
            (enemy->m_nAIState & 0x84)==0x84 &&
            K1EngageEnemyHasClearShot(enemy,target) &&
            (distance<.01f || dot(enemy->m_orientation,offset/distance)>.5f);
        const bool chain=canAttack && !npc.ranged && npc.attacking && npc.chainSwings<NpcChainLimit(npc) &&
            time>=NpcHitEnd(npc)+chainFollowThrough && NpcChainAccepted(npc);
        if(attackFinished && !chain) {
            if(std::getenv("K1_ENGAGE_ENEMY_AI_TEST")) fprintf(stderr,"K1_NPC_SWING_TRACE actor=%llu serial=%d poses=%llu scene=%d blades=%d sweeps=%d closest_tip=%g reach=%g target_near=%g\n",
                npc.id,npc.serial,npc.revision,npc.sceneVisits,npc.bladeSamples,npc.sweeps,npc.closestTip,npc.maximumReach,npc.targetNear);
            if(!npc.ranged) {
                if(npc.chainSwings==NpcChainLimit(npc)) ++npc.completedChains;
                fprintf(stderr,"K1_NPC_CHAIN_END actor=%llu swings=%d cap=%d\n",npc.id,npc.chainSwings,NpcChainLimit(npc));
                npc.nextAttack=now+NpcAttackRest(npc);
            }
            npc.attacking=false;npc.chainSwings=0;npc.bladeValid=false;
        }
        if(canAttack && (chain || (!npc.attacking && now>=npc.nextAttack))) {
            auto& episode=meleePoise[npc.id];
            if(episode.clashSwing)ClearClashEpisode(npc.id);
            if(episode.clashes)episode.clashSwing=true;
            if(chain) {
                ++npc.chainLinks;
                fprintf(stderr,"K1_NPC_CHAIN actor=%llu swing=%d outgoing=%d time=%g handoff=%g\n",
                    npc.id,npc.chainSwings+1,npc.direction,time,NpcHitEnd(npc)+chainFollowThrough);
            }
            npc.chainSwings=chain?npc.chainSwings+1:1;
            npc.redFrom=npc.red && chain?npc.direction:-1;
            npc.blueFrom=npc.blue && chain?npc.direction:-1;
            npc.direction=std::getenv("K1_ENGAGE_CLASH_TEST")?0:
                (std::getenv("K1_ENGAGE_PARRY_TEST")?parryProbeDirection:
                    (npc.ranged?g_pRules->RollDice(1,7)-1:NpcMeleeDirectionForRoll(g_pRules->RollDice(1,18)-1)));
            npc.attacking=true;npc.hit=false;npc.bladeValid=false;npc.contactBlades=0;npc.previousBlades.clear();
            npc.attackStartedAt=now;
            meleePoise[npc.id].blockReaction=-1;
            if(!npc.ranged) NotePoiseActivity(enemy,"swing");
            ++npc.serial;time=0;
            if(npc.red && npc.redFrom>=0)fprintf(stderr,"K1_RED_NPC_CHAIN_SELECT from=%d to=%d clip=%s transition=%s\n",npc.redFrom,npc.direction,NpcAttackClip(npc,false),redChains[npc.redFrom][npc.direction].transition);
            attackStarted=true;
            if(npc.ranged) npc.nextAttack=now+(npc.rifle?350000:400000); // Owned JKA weapons.dat firetime.
            npc.bladeSamples=npc.sweeps=npc.sceneVisits=0;npc.closestTip=999.f;
            npc.maximumReach=-999.f;npc.targetNear=999.f;
            fprintf(stderr,"K1_NPC_ATTACK actor=%llu serial=%d direction=%d\n",npc.id,npc.serial,npc.direction);
        }
        if(!npc.ranged) {
            const bool wasWalking=npc.meleeAttackWalk;
            if(npc.attacking) npc.meleeAttackWalk=true;
            else if(distance>attackRange+enemy->m_pPathfindInfo->m_fCrePersonalSpace+meleeWalkExtraDistance) npc.meleeAttackWalk=false;
            if(wasWalking!=npc.meleeAttackWalk) {
                npc.nextSteer=now;
                if(!npc.meleeAttackWalk) ++npc.attackWalkExits;
                fprintf(stderr,"K1_NPC_ATTACK_WALK actor=%llu walking=%d attacking=%d distance=%g reach=%g walk_radius=%g\n",
                    npc.id,npc.meleeAttackWalk,npc.attacking,distance,attackRange,
                    attackRange+enemy->m_pPathfindInfo->m_fCrePersonalSpace+meleeWalkExtraDistance);
            }
            MoveNpcWithJka(npc,enemy,dt,now);
            const float speed=magnitude(npc.groundVelocity)/K1GroundMove::UnitScale;
            if(npc.meleeAttackWalk) {
                ++npc.attackWalkSamples;
                if(speed>.1f) ++npc.attackWalkMoving;
                if(!npc.attacking && !reaction && !npc.parrying) ++npc.attackWalkCooldown;
                npc.attackWalkMaxSpeed=std::max(npc.attackWalkMaxSpeed,speed);
                npc.attackWalkOK=npc.attackWalkOK && !enemy->m_bRunning && K1EngageEnemyForceWalk(npc.id) &&
                    speed<=EngageMoveSpeed(enemy,true)/K1GroundMove::UnitScale+.001f;
            } else if(npc.attackWalkExits>0 && speed>EngageMoveSpeed(enemy,true)/K1GroundMove::UnitScale+.1f && !NpcWalking(npc)) ++npc.attackWalkChase;
        }
        const Vector motion=enemy->m_position-npc.position;
        npc.updated=now;
        if(magnitude(motion)>.00001f) {npc.lastMotion=motion;npc.movingUntil=now+80000;}
        // Replicated path positions can be stationary between updates. Retain
        // the gait briefly so one empty sample doesn't restart idle/run/attack.
        const bool moving=now<npc.movingUntil;
        npc.walking=NpcWalking(npc);
        npc.position=enemy->m_position;
        const char* ready=npc.ranged?(npc.rifle?"j_rifle_ready":"j_pistol_ready"):"j_idle";
        const float forward=dot(npc.lastMotion,enemy->m_orientation);
        const float right=npc.lastMotion.x*enemy->m_orientation.y-npc.lastMotion.y*enemy->m_orientation.x;
        if(npc.sourceJediCommands) npc.sourceBackpedal=K1JkaAI::backpedal(npc.sourceBackpedal,npc.commands);
        const bool backward=npc.sourceJediCommands?npc.sourceBackpedal:forward<0;
        npc.backLeft=moving && backward && right<0;
        float targetYaw=moving?(backward?std::atan2(right,-forward):std::atan2(-right,forward)):0.f;
        targetYaw=std::max(-.7853981634f,std::min(.7853981634f,targetYaw));
        const float yawStep=5.f*3.141592653589793f/180.f*dt*60.f;
        npc.legsYaw+=std::max(-yawStep,std::min(yawStep,targetYaw-npc.legsYaw));
        npc.maximumLegYaw=std::max(npc.maximumLegYaw,std::fabs(npc.legsYaw));
        // Use the player's forward/back gait plus its bounded JKA leg turn.
        // The owned RUNSTRAFE ranges are unused, duplicate source poses.
        const char* gait=backward?(npc.walking?"j_backwalk":"j_backrun"):(npc.walking?"j_walk":"j_run");
        if(moving && npc.sourceJediCommands && !npc.commands.forward && npc.commands.right) {
            ++npc.sourceLateralSamples;
            npc.parityOK=npc.parityOK && !backward;
        }
        const bool guarding=NpcGuarding(npc);
        const auto& poise=meleePoise[npc.id];
        const int block=npc.attacking?-1:ActiveBlockReaction(npc.id,now);
        const bool blockRestart=block>=0 && npc.pose.blockSerial!=poise.blockSerial;
        const char* base=reaction?reaction:(moving?gait:(guarding?"j_guard":ready));
        const char* overlay=npc.attacking?(npc.ranged?
            (npc.rifle?(moving?"y_rifle_fire":"j_rifle_fire"):(moving?"y_pistol_fire":"j_pistol_fire")):
            NpcAttackClip(npc,moving)):
            (block>=0?blockUpper[block]:(guarding && moving?"y_guard":nullptr));
        float baseTime=0,baseLength=0;
        const bool baseChanged=npc.base!=base || !npc.body->GetAnimation(const_cast<char*>(base),&baseTime,&baseLength);
        const bool changed=attackStarted || baseChanged || npc.overlay!=overlay || blockRestart;
        if(changed && std::getenv("K1_ENGAGE_ENEMY_AI_TEST"))
            fprintf(stderr,"K1_NPC_LAYER_CHANGE actor=%llu base=%s overlay=%s old_base=%s motion=%g move_state=%d running=%d attack_time=%g source_cmd=%d forward=%d right=%d\n",
                npc.id,base,overlay?overlay:"none",npc.base?npc.base:"none",magnitude(motion),enemy->m_nMoveState,enemy->m_bRunning,time,
                npc.sourceJediCommands,npc.commands.forward,npc.commands.right);
        if(changed) {
            npc.pose.blendPending=true;
            if(npc.overlay) npc.body->PlayAnimation(const_cast<char*>(npc.overlay),1,AnimRun::kAnimRemove,0);
            if(baseChanged) {
                if(npc.base) npc.body->PlayAnimation(const_cast<char*>(npc.base),1,AnimRun::kAnimRemove,0);
                if(reaction) npc.body->PlayAnimation(const_cast<char*>(base),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,0);
                else Overlay(npc.body,base,false);
            }
            if(overlay) npc.body->PlayAnimation(const_cast<char*>(overlay),block>=0?0.f:(guarding?1.f:(npc.ranged?1.f:NpcSwingRate(npc))),
                AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn|(guarding?0:AnimRun::kAnimOnce),guarding?0:time);
            if(!baseChanged) {
                float preserved=0,len=0;
                npc.parityOK=npc.parityOK && npc.body->GetAnimation(const_cast<char*>(base),&preserved,&len) &&
                    std::fabs(preserved-baseTime)<.0001f;
                ++npc.continuousLayers;
            }
            npc.base=base;npc.overlay=overlay;
        }
        if(block>=0) {
            float t=0,len=0;
            if(npc.body->GetAnimation(const_cast<char*>(overlay),&t,&len)) {
                npc.body->SetAnimationTime(const_cast<char*>(overlay),std::min(float(now-poise.blockStarted)/1000000.f,std::max(0.f,len-.0001f)));
                if(blockRestart) fprintf(stderr,"K1_BLOCK_REACTION_PLAY actor=%llu serial=%u clip=%s base=%s\n",npc.id,poise.blockSerial,overlay,base);
                npc.pose.blockSerial=poise.blockSerial;
            }
        } else if(npc.parrying) {
            float t=0,len=0;
            const char* held=moving?overlay:base;
            if(held && npc.body->GetAnimation(const_cast<char*>(held),&t,&len))
                npc.body->SetAnimationTime(const_cast<char*>(held),std::min(float(now-npc.parryStart)/1000000.f,std::max(0.f,len-.0001f)));
        }
        if(reaction) {
            float t=0,len=0;
            if(npc.body->GetAnimation(const_cast<char*>(reaction),&t,&len)) {
                float elapsed=float(now-npc.reactionStarted)/1000000.f;
                npc.body->SetAnimationTime(const_cast<char*>(reaction),std::min(elapsed,std::max(0.f,len-.0001f)));
            }
        }
    }
}
// NPC head_g is below Hturn_g/neck, not below the external-head hook. Solve its
// current parent frame, including native head tracking, rather than copying a
// local quaternion that only works when that extra chain is in its bind pose.
bool NpcGuardHeadFrame(Gob* body,Vector& position,Quaternion& orientation) {
    auto* hook=body->GetPart(const_cast<char*>("headhook"));
    auto* head=body->GetPart(const_cast<char*>("head_g"));
    if(!hook || !head || head->m_pNode->m_pTree!=body->m_pModel)return false;
    Vector hookBind,headBind,hookWorld;Quaternion hookBindQ,headBindQ,hookQ;
    BindFrame(hook,hookBind,hookBindQ);BindFrame(head,headBind,headBindQ);
    hook->GetWorld(hookWorld,hookQ);
    const Quaternion frame=normalize(hookQ*Inverse(normalize(hookBindQ)));
    position=hookWorld+Rotate(frame,(headBind-hookBind)*body->m_fObjectScale);
    orientation=normalize(frame*headBindQ);
    return true;
}
void AlignNpcGuardHead(Gob* body) {
    Vector target;Quaternion orientation;
    if(!NpcGuardHeadFrame(body,target,orientation))return;
    auto* head=body->GetPart(const_cast<char*>("head_g"));
    Vector parentPosition=body->m_vPosition;Quaternion parentQ=body->m_qOrientation;
    if(head->m_pParent)head->m_pParent->GetWorld(parentPosition,parentQ);
    float scale=body->m_fObjectScale;
    for(Part* p=head->m_pParent;p;p=p->m_pParent)scale*=p->m_scale.m_fScale;
    if(std::fabs(scale)<.000001f)return;
    head->m_vLocalPosition=Rotate(Inverse(normalize(parentQ)),target-parentPosition)/scale;
    SetWorldOrientation(head,orientation);
}
bool PoseNpcBody(Gob* body,float delta) {
    auto* npc=NpcOwnsBody(body)?FindNpcBody(body):nullptr;
    if(!npc) return false;
    const bool spinChain=NpcSpinChain(*npc);
    const bool nativeGait=!spinChain && !npc->ranged &&
        ApplyYellowNativeGait(body,npc->base,npc->overlay!=nullptr,delta,npc->nativeResource.c_str(),npc->pose,npc->staff,npc->dual);
    float time=0,length=0;
    const bool overlay=npc->overlay && body->GetAnimation(const_cast<char*>(npc->overlay),&time,&length);
    const bool guarding=NpcGuarding(*npc);
    const char* blockReference=BlockReference(npc->overlay);
    if(guarding && npc->closeGuard && !blockReference) {
        if(overlay) ++npc->closeGuardMoving;else ++npc->closeGuardStill;
        npc->parityOK=npc->parityOK && npc->walking &&
            !strcmp(overlay?npc->overlay:npc->base,overlay?"y_guard":"j_guard");
    }
    const char* reference=blockReference?blockReference:(guarding?"j_guard":(npc->ranged?(npc->rifle?"j_rifle_fire":"j_pistol_fire"):NpcAttackClip(*npc,false)));
    if(overlay && npc->overlay[0]=='y') BridgeTorso(body,reference,time);
    const bool nativeIdle=!overlay && !npc->attacking && !guarding && npc->base &&
        !strcmp(npc->base,"j_idle") && !npc->nativeIdle.empty();
    if(nativeIdle) for(const auto& pose:npc->nativeIdle) {
        pose.part->m_vLocalPosition=pose.position;pose.part->m_qLocalOrientation=pose.orientation;
    }
    else if(!spinChain)TurnSpine(body,npc->legsYaw,overlay?reference:nullptr,time);
    if(overlay && npc->attacking && !npc->ranged && !npc->staff && !npc->dual && !npc->red && !npc->blue && !guarding) CalibrateCut(body,npc->direction,time,npc->overlay[0]=='y',npc->walking,npc->backLeft);
    if(!nativeGait && !overlay && npc->base && (!strcmp(npc->base,"j_run") || !strcmp(npc->base,"j_backrun")))
        RefineRunPose(body,!strcmp(npc->base,"j_backrun"),delta,npc->pose);
    if(npc->staff && guarding && !blockReference) ApplyStaffGuard(body,npc->base && strstr(npc->base,"walk"),npc->staffGuard);
    if(npc->staff && !overlay && !guarding && !npc->attacking && npc->base &&
       (strstr(npc->base,"walk") || strstr(npc->base,"run"))) ClearStaffStrafe(body,npc->legsYaw);
    // Native dual grips/guard are the target pose, not a post-blend override.
    // Blend and cache the same pose that will actually be displayed.
    const bool lateDualBaseline=npc->dual && std::getenv("K1_DUAL_LATE_GRIP_BASELINE");
    bool authoredHead=false;
    auto blendNpc=[&] {
        if(guarding && !npc->attacking && !blockReference && !npc->ranged && npc->base &&
           (strstr(npc->base,"walk") || !strcmp(npc->base,"j_guard") || !strcmp(npc->base,"j_idle"))) {
            const bool walking=strstr(npc->base,"walk")!=nullptr;
            for(const auto& edit:authoredNpcGuardPoses) {
                if(edit.walking!=walking || _stricmp(edit.rig,body->m_pModel->m_szName))continue;
                if(!strcmp(edit.bone,"head_g"))continue; // old generated tables
                if(auto* joint=body->GetPart(const_cast<char*>(edit.bone))) {
                    joint->m_vLocalPosition=Vector(edit.px,edit.py,edit.pz);
                    joint->m_qLocalOrientation=normalize(Quaternion(edit.qw,edit.qx,edit.qy,edit.qz));
                    authoredHead=authoredHead || !strcmp(edit.bone,"headhook");
                }
            }
            if(authoredHead)AlignNpcGuardHead(body);
        }
        PrepareGuardBlend(npc->pose,guarding,!npc->attacking && !blockReference && !npc->ranged);
        SmoothBody(body,delta,(npc->attacking || guarding || blockReference) && !npc->ranged && !npc->dual,nativeIdle,npc->pose);
    };
    if(lateDualBaseline)blendNpc();
    if(npc->dual && guarding && !blockReference)ApplyStaffGuard(body,npc->base && strstr(npc->base,"walk"),npc->staffGuard);
    if(npc->dual && !npc->attacking && !blockReference && npc->base) {
        const bool walking=strstr(npc->base,"walk")!=nullptr;
        if(walking)StabilizeDualWalkingTorso(body,guarding?npc->staffGuard:npc->nativeIdle);
        if(!guarding && (walking || strstr(npc->base,"run")))ApplyDualMovingGrip(body,npc->staffGuard,walking);
    }
    if(!lateDualBaseline)blendNpc();
    const float settled=npc->pose.guardTransition?guardBlendDuration:.14f;
    if(authoredHead && npc->pose.blendTime>=settled) {
        const unsigned bit=npc->base && strstr(npc->base,"walk")?2:1;
        if(!(npc->authoredHeadReported&bit)) {
            Vector target,actual;Quaternion expected,q;
            if(NpcGuardHeadFrame(body,target,expected)) {
                body->GetPart(const_cast<char*>("head_g"))->GetWorld(actual,q);
                const float pe=magnitude(actual-target),qe=1-std::fabs(dot(normalize(q),expected));
                fprintf(stderr,"K1_NPC_AUTHORED_HEAD_%s actor=%llu rig=%s walking=%d position_error=%g rotation_error=%g\n",pe<.00002f && qe<.00001f?"PASS":"FAIL",npc->id,body->m_pModel->m_szName,bit==2,pe,qe);
                npc->authoredHeadReported|=bit;
            }
        }
    }
    ++npc->revision;
    ObserveBlockPose(body,npc->pose,npc->overlay,npc->id,npc->base && (strstr(npc->base,"walk") || strstr(npc->base,"run")));
    if(npc->pose.lastGrip.joined) {
        ++npc->gripSamples;
        npc->maximumGripError=std::max(npc->maximumGripError,npc->pose.sourceContactError);
    }
    for(const auto& pose:npc->pose.lastPose) npc->parityOK=npc->parityOK &&
        std::isfinite(dot(pose.orientation,pose.orientation)) &&
        std::isfinite(pose.position.x) && std::isfinite(pose.position.y) && std::isfinite(pose.position.z);
    if(nativeIdle && npc->pose.blendTime>=.14f) {
        ++npc->idleSamples;
        for(const auto& pose:npc->nativeIdle) {
            npc->idleError=std::max(npc->idleError,magnitude(pose.part->m_vLocalPosition-pose.position));
            npc->parityOK=npc->parityOK &&
                std::fabs(dot(normalize(pose.part->m_qLocalOrientation),normalize(pose.orientation)))>.99999f;
        }
    }
    ApplyUtilityBlend(body,delta);
    return true;
}
bool NpcRangedDamage(CSWSCreature* enemy,CSWSCreature* target) {
    auto* round=enemy->m_pCombatRound;
    if(!round || round->m_bRoundStarted || round->m_bSpellCastRound) return false;
    NotePoiseActivity(target,"hit"); // confirmed bolt contact, including fully absorbed damage
    const auto oldAttack=round->m_currentAttack;const int oldCount=round->m_nOnHandAttacks,oldOffhand=round->GetOffHandAttack();
    round->m_currentAttack=0;round->m_nOnHandAttacks=1;round->SetOffHandAttack(0);
    auto* attack=round->GetAttack(0);attack->ClearAttackData();
    attack->m_targetObjectId=target->m_objectId;attack->m_bRangedAttack=1;
    attack->m_nAttackResult=1;attack->m_attack.m_bAutomaticHit=1;
    attack->m_nWeaponAttackType=CSWSCombatRound::WEAPON_ATTACK_TYPE_ONHAND;
    enemy->ResolveDamage(target,0,0);enemy->ResolvePostRangedDamage(target);
    const int damage=attack->GetBaseDamage();
    attack->ResolveAttackToRangedSubAttack(0,0,1,1,0);attack->ResolveTopSubAttackToAttack();
    enemy->SignalRangedDamage(target->m_objectId);attack->RemoveTopSubAttack();
    round->m_currentAttack=oldAttack;round->m_nOnHandAttacks=oldCount;round->SetOffHandAttack(oldOffhand);
    fprintf(stderr,"K1_NPC_RANGED_DAMAGE actor=%llu target=%llu damage=%d roll=none\n",enemy->m_objectId,target->m_objectId,damage);
    return true;
}
void FireNpcGun(NpcPlayback& npc,CSWSCreature* target) {
    auto* enemy=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(npc.id);
    npc.hit=true;
    if(!K1EngageEnemyHasClearShot(enemy,target)) {
        fprintf(stderr,"K1_NPC_SHOT_CANCEL actor=%llu reason=blocked_lane\n",npc.id);return;
    }
    Gob* weapon=npc.owner->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
    Part* muzzle=weapon?weapon->GetPart(const_cast<char*>("bullethook")):nullptr;
    if(!muzzle) {fprintf(stderr,"K1_NPC_SHOT_FAIL actor=%llu reason=missing_muzzle\n",npc.id);return;}
    Vector start;Quaternion orientation;muzzle->GetWorld(start,orientation);
    Vector aim=target->m_position+Vector(0,0,1.f);
    auto* player=Actor();Gob* body=player?player->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
    if(body) if(auto* impact=body->GetPart(const_cast<char*>("Impact"))) impact->GetWorld(aim,orientation);
    Vector direction=normalize(aim-start);
    // Pistol uses BRYAR_PISTOL_VEL; ordinary SP rifle NPCs use the normal
    // BLASTER_NPC_VEL_CUT. Damage is always the equipped KotOR weapon's.
    const float speed=(npc.rifle?2300.f*.5f:1800.f)*K1GroundMove::UnitScale;
    Vector end=start+direction*(8192.f*K1GroundMove::UnitScale),wall;
    unsigned long long blocker=OBJECT_INVALID;
    auto* area=static_cast<CSWSArea*>(enemy->GetArea());
    if(!area->ClearLineOfSight(start,end,&wall,&blocker,enemy->m_objectId,OBJECT_INVALID,1,0)) end=wall;
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    auto* item=static_cast<CSWSItem*>(enemy->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON));
    if(!item) return;
    npcBolts.push_back({enemy->m_objectId,item->m_objectId,area->m_objectId,start,direction*speed,now,
        now+static_cast<long long>(magnitude(end-start)/speed*1000000)});
    // Fixed-point native bolt visual: authority below advances a straight
    // projectile and tests actual contact, rather than homing or rolling to hit.
    enemy->BroadcastSafeProjectile(enemy->m_objectId,OBJECT_INVALID,start,end,
        static_cast<unsigned int>(magnitude(end-start)/speed*1000),4,0,item->GetBaseItem()->m_nAmmunitionType,0,0);
    fprintf(stderr,"K1_NPC_SHOT actor=%llu serial=%d rifle=%d speed=%g\n",npc.id,npc.serial,npc.rifle,speed);
}
void TraceNpcBolts(CAurScene* scene) {
    if(npcBolts.empty()) return;
    auto* server=g_pAppManager->m_pServerExoApp;
    auto* module=g_pAppManager->m_pClientExoApp->GetModule();
    if(!module || !module->m_pArea || module->m_pArea->m_pScene!=scene) return;
    const auto now=server->GetWorldTimer()->GetSnapshotTime();
    for(auto it=npcBolts.begin();it!=npcBolts.end();) {
        auto* enemy=server->GetCreatureByGameObjectID(it->owner);
        if(!enemy || !enemy->GetArea() || static_cast<CSWSArea*>(enemy->GetArea())->m_objectId!=it->area || now>it->expires) {
            it=npcBolts.erase(it);continue;
        }
        if(now<=it->updated) {++it;continue;}
        Vector end=it->position+it->velocity*(float(now-it->updated)/1000000.f),wall;
        auto* area=static_cast<CSWSArea*>(enemy->GetArea());unsigned long long blocker=OBJECT_INVALID;
        const bool clear=area->ClearLineOfSight(it->position,end,&wall,&blocker,enemy->m_objectId,OBJECT_INVALID,1,0)!=0;
        if(!clear) end=wall;
        CSWSCreature* victim=nullptr;float first=2;
        for(int i=0;i<area->m_aObjectsByX.m_count;++i) {
            auto* target=server->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);
            if(!target || target==enemy || target->GetDead()) continue;
            auto* client=target->GetClientCreature();
            if(!client) continue;
            std::vector<K1SaberContact::Triangle> triangles;
            CollisionTriangles(client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1),triangles);
            CollisionTriangles(client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1),triangles);
            for(const auto& triangle:triangles) if(K1SaberContact::SegmentTriangle(it->position,end,triangle)) {
                const Vector normal=cross(triangle.b-triangle.a,triangle.c-triangle.a);
                const float denominator=dot(end-it->position,normal);
                const float fraction=std::fabs(denominator)>1e-10f?
                    std::max(0.f,std::min(1.f,dot(triangle.a-it->position,normal)/denominator)):0.f;
                if(fraction<first) {first=fraction;victim=target;}
            }
        }
        if(victim) {
            auto* weapon=enemy->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
            const bool hostile=enemy->GetReputation(victim->m_objectId,0)<=10;
            const bool sameWeapon=weapon && weapon->m_objectId==it->weapon;
            const bool damaged=hostile && sameWeapon && NpcRangedDamage(enemy,victim);
            fprintf(stderr,"K1_NPC_BOLT_IMPACT actor=%llu target=%llu hostile=%d same_weapon=%d damage=%d\n",
                it->owner,victim->m_objectId,hostile,sameWeapon,damaged);
            it=npcBolts.erase(it);
        } else if(!clear) it=npcBolts.erase(it);
        else {it->position=end;it->updated=now;++it;}
    }
}
void TraceNpcBlades(CAurScene* scene) {
    auto* controller=ServerActor();
    if(!controller || !K1EngageEnemyAIActive(controller->m_objectId))return;
    for(auto& entry:npcPlayback) {
        auto& npc=entry.second;
        auto* target=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(npc.target);
        auto* player=target?target->GetClientCreature():nullptr;
        if(!target || target->GetDead() || !player)continue;
        if(!npc.requested || !npc.body || npc.body->m_pScene!=scene || npc.revision==npc.contactRevision) continue;
        npc.contactRevision=npc.revision;
        ++npc.sceneVisits;
        float time=0,length=0;Vector base,tip;
        auto* attacker=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(npc.id);
        if(!attacker || (attacker->m_nAIState & 0x84)!=0x84) {npc.bladeValid=false;continue;}
        if(npc.ranged) {
            if(npc.attacking && !npc.hit && npc.overlay && npc.body->GetAnimation(const_cast<char*>(npc.overlay),&time,&length)) FireNpcGun(npc,target);
            continue;
        }
        if(!npc.attacking || !npc.overlay || !npc.body->GetAnimation(const_cast<char*>(npc.overlay),&time,&length)) {npc.bladeValid=false;continue;}
        std::vector<K1SaberContact::Blade> beams;
        if(npc.dual) {beams.resize(2);for(int hand=0;hand<2;++hand)DualBlade(npc.owner,hand,beams[hand]);}
        else if(npc.staff) beams=StaffBlades(npc.owner);
        else if(Blade(npc.owner,base,tip)) beams.push_back({base,tip});
        if(beams.empty()) {npc.bladeValid=false;npc.previousBlades.clear();continue;}
        const float previousTime=npc.bladeTime;
        for(size_t blade=0;blade<beams.size();++blade) {
        if(npc.dual && magnitude(beams[blade].tip-beams[blade].base)<.0001f)continue;
        base=beams[blade].base;tip=beams[blade].tip;
        npc.bladeTime=previousTime;
        ++npc.bladeSamples;
        npc.closestTip=std::min(npc.closestTip,magnitude(tip-(target->m_position+Vector(0,0,1.f))));
        auto begin=(npc.staff || npc.dual) && npc.previousBlades.size()==beams.size()?npc.previousBlades[blade]:K1SaberContact::Blade{npc.bladeBase,npc.bladeTip},end=beams[blade];
        const bool reset=!npc.bladeValid || ((npc.staff || npc.dual) && npc.previousBlades.size()!=beams.size()) || time<npc.bladeTime || magnitude(base-begin.base)>2.f;
        if(reset) {begin=end;npc.bladeTime=time;}
        const auto worldPrevious=begin,worldCurrent=end;
        begin=RelativeContactStart(target->m_objectId,begin,!reset);
        const float start=NpcHitStart(npc),finish=npc.staff?StaffContactEnd(npc.direction):ContactEnd(NpcHitEnd(npc));
        if(SwingSoundDue(npc.serial,npc.pose.swingSoundSerial,time,start)) EngageWeaponSound(attacker,nullptr,"swing");
        float lo=0,hi=1;
        if(((npc.staff || npc.dual)?!(npc.contactBlades&(1u<<blade)):!npc.hit) && K1SaberContact::ClipTime(npc.bladeTime,time,start,finish,lo,hi)) {
            ++npc.sweeps;
            auto* enemy=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(npc.id);
            Vector forward=normalize(target->m_position-enemy->m_position);
            npc.maximumReach=std::max(npc.maximumReach,std::max(dot(base-enemy->m_position,forward),dot(tip-enemy->m_position,forward)));
            if(time>npc.bladeTime) {
                const auto raw=begin;
                begin=K1SaberContact::Between(raw,end,lo);end=K1SaberContact::Between(raw,end,hi);
            }
            std::vector<K1SaberContact::Triangle> triangles;std::vector<Part*> parts;
            CollisionTriangles(player->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1),triangles,&parts);
            CollisionTriangles(player->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1),triangles,&parts);
            if(std::getenv("K1_ENGAGE_ENEMY_AI_TEST") && npc.sweeps==1) {
                float near=999.f,far=-999.f;
                for(const auto& triangle:triangles) for(const Vector& p:{triangle.a,triangle.b,triangle.c}) {
                    near=std::min(near,dot(p-enemy->m_position,forward));far=std::max(far,dot(p-enemy->m_position,forward));
                }
                npc.targetNear=near;
                fprintf(stderr,"K1_NPC_SWEEP_GEOMETRY actor=%llu serial=%d triangles=%zu target_forward=%g,%g blade_forward=%g,%g body_y=%g,%g server_forward=%g,%g time=%g\n",
                    npc.id,npc.serial,triangles.size(),near,far,dot(base-enemy->m_position,forward),dot(tip-enemy->m_position,forward),
                    npc.body->m_qOrientation.ydir().x,npc.body->m_qOrientation.ydir().y,enemy->m_orientation.x,enemy->m_orientation.y,time);
            }
            const int steps=K1SaberContact::Steps(begin,end);bool hit=false;
            MeleeContact contact={npc.id,target->m_objectId,npc.serial};
            contact.blade=(npc.staff || npc.dual)?int(blade):-1;contact.poiseOnly=npc.dual?false:npc.hit;
            if(npc.dual) {
                auto* item=attacker->m_pInventory->GetItemInSlot(blade?CSWInventory::SLOT_LEFTWEAPON:CSWInventory::SLOT_RIGHTWEAPON);
                if(!item)continue;
                contact.dualWeapon=item->m_objectId;contact.offhand=blade!=0;
            }
            for(int step=0;step<steps && !hit;++step) for(size_t ti=0;ti<triangles.size();++ti) {
                const auto x=K1SaberContact::Between(begin,end,float(step)/steps),y=K1SaberContact::Between(begin,end,float(step+1)/steps);
                if(K1SaberContact::SweepCell(x,y,triangles[ti])) {
                    if(K1SaberContact::SweepPoint(x,y,triangles[ti],contact.hitPoint)) {
                        contact.hitPart=parts[ti];contact.bladeDirection=y.tip-y.base;
                        const float fraction=std::max(0.f,std::min(1.f,dot(contact.hitPoint-y.base,y.tip-y.base)/dot(y.tip-y.base,y.tip-y.base)));
                        const auto wx=K1SaberContact::Between(worldPrevious,worldCurrent,lo+(hi-lo)*(float(step)/steps)),wy=K1SaberContact::Between(worldPrevious,worldCurrent,lo+(hi-lo)*(float(step+1)/steps));
                        contact.bladeDirection=wy.tip-wy.base;
                        contact.bladeMotion=(wy.base-wx.base)*(1-fraction)+(wy.tip-wx.tip)*fraction;
                    }
                    hit=true;break;
                }
            }
            if(hit) {
                if(ContactTimingEnabled())contact.wallAt=ContactWallUs();
                npc.hit=true;if(npc.staff || npc.dual) npc.contactBlades|=1u<<blade;
                if(npc.dual)fprintf(stderr,"K1_NPC_DUAL_CONTACT actor=%llu serial=%d hand=%zu weapon=%llu\n",npc.id,npc.serial,blade,contact.dualWeapon);
                if(npc.staff) fprintf(stderr,"K1_NPC_STAFF_CONTACT actor=%llu serial=%d blade=%zu poise_only=%d\n",npc.id,npc.serial,blade,contact.poiseOnly);
                auto* enemy=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(npc.id);
                if(enemy) {if(!contact.poiseOnly) contact.dismemberRow=SampleDismemberment(enemy,target,contact);contact.hitPart=nullptr;sceneMeleeContacts.push_back(contact);}
            }
        }
        }
        npc.bladeBase=beams[0].base;npc.bladeTip=beams[0].tip;npc.previousBlades=beams;npc.bladeTime=time;npc.bladeValid=true;
    }
}
bool ContactSwingCurrent(const MeleeContact& contact) {
    if(K1EngageOwnsPlayer(contact.attacker)) return state.attacking && contact.serial==attackSerial;
    auto found=npcPlayback.find(contact.attacker);
    return found!=npcPlayback.end() && found->second.attacking && !found->second.ranged && found->second.serial==contact.serial;
}
void EndClashChain(unsigned long long id,long long now) {
    if(K1EngageOwnsPlayer(id)) {
        state.attacking=state.attackRequested=state.holdChain=false;state.chainSwings=0;
        hitConsumed=true;hadBlade=false;blendPending=true;
    } else {
        auto found=npcPlayback.find(id);
        if(found!=npcPlayback.end()) {
            auto& npc=found->second;npc.attacking=false;npc.chainSwings=0;
            npc.hit=true;npc.bladeValid=false;npc.pose.blendPending=true;npc.nextAttack=now+150000;
        }
    }
}
bool ContactActorsValid(const MeleeContact& contact) {
    auto* server=g_pAppManager->m_pServerExoApp;
    auto* attacker=server->GetCreatureByGameObjectID(contact.attacker);
    auto* target=server->GetCreatureByGameObjectID(contact.target);
    if(contact.dualWeapon!=OBJECT_INVALID) {
        auto* item=attacker && attacker->m_pInventory?attacker->m_pInventory->GetItemInSlot(contact.offhand?CSWInventory::SLOT_LEFTWEAPON:CSWInventory::SLOT_RIGHTWEAPON):nullptr;
        if(!(attacker==ServerActor()?DualMode():DualNpc(attacker)) || !item || item->m_objectId!=contact.dualWeapon) return false;
    }
    const auto owned=npcPlayback.find(contact.attacker);
    if(owned!=npcPlayback.end() && owned->second.target!=contact.target)return false;
    return attacker && target && !attacker->GetDead() && !target->GetDead() && attacker->GetArea()==target->GetArea() &&
        (K1EngageOwnsPlayer(contact.attacker) || K1EngageOwnsEnemy(contact.attacker));
}
bool ContactCanDeflect(const MeleeContact& contact,long long now) {
    auto* server=g_pAppManager->m_pServerExoApp;
    auto* attacker=server->GetCreatureByGameObjectID(contact.attacker);
    auto* target=server->GetCreatureByGameObjectID(contact.target);
    if(!attacker || !target || !HasMeleePoise(attacker) || !HasMeleePoise(target) ||
        PoiseBroken(attacker) || PoiseBroken(target) || NativeStatusAnimation(attacker) || NativeStatusAnimation(target)) return false;
    if(K1EngageOwnsPlayer(contact.target)) return state.attacking &&
        K1AttackDeflection::Eligible(playerAttackStartedAt,now);
    auto found=npcPlayback.find(contact.target);
    return found!=npcPlayback.end() && found->second.attacking && !found->second.ranged &&
        K1AttackDeflection::Eligible(found->second.attackStartedAt,now);
}
void ResolveMeleeContacts(const std::vector<MeleeContact>& contacts,long long sampleTime) {
    auto* server=g_pAppManager->m_pServerExoApp;
    if(!server) return;
    if(state.mode==K1EngageState::Mode::Exploration)return;
    if(IsGamePaused()) return;
    const auto now=sampleTime>=0?sampleTime:server->GetWorldTimer()->GetSnapshotTime();
    std::vector<MeleeContact> accepted;
    for(auto contact:contacts) {
        if(!ContactSwingCurrent(contact) || !ContactActorsValid(contact)) {
            if(std::getenv("K1_ENGAGE_MELEE_HIT_TEST") && K1EngageOwnsPlayer(contact.attacker))++playerHitAudits[contact.serial].dropped;
            continue;
        }
        const bool duplicate=std::any_of(accepted.begin(),accepted.end(),[&](const MeleeContact& old){
            return old.attacker==contact.attacker && old.serial==contact.serial && old.blade==contact.blade;});
        if(duplicate)continue;
        contact.at=now;
        if(ContactTimingEnabled() && !contact.wallAt)contact.wallAt=ContactWallUs();
        accepted.push_back(contact);
    }
    std::vector<unsigned long long> clashed;
    auto cancelled=[&](unsigned long long id){return std::find(clashed.begin(),clashed.end(),id)!=clashed.end();};
    for(const auto& contact:accepted) {
        if(contact.poiseOnly || cancelled(contact.attacker) || cancelled(contact.target) ||
           !ContactActorsValid(contact) || !ContactCanDeflect(contact,now))continue;
        auto* attacker=server->GetCreatureByGameObjectID(contact.attacker);
        auto* target=server->GetCreatureByGameObjectID(contact.target);
        clashed.push_back(contact.attacker);clashed.push_back(contact.target);
        if(std::getenv("K1_ENGAGE_MELEE_HIT_TEST")) {
            if(K1EngageOwnsPlayer(contact.attacker))++playerHitAudits[contact.serial].clashed;
            else if(K1EngageOwnsPlayer(contact.target))++playerHitAudits[attackSerial].clashed;
        }
        EndClashChain(contact.attacker,now);EndClashChain(contact.target,now);
        ApplyClashPoise(attacker,target);EngageWeaponSound(attacker,target,"clash");
        ++meleeClashes;lastClashGap=0;
        fprintf(stderr,"K1_MELEE_DEFLECTION a=%llu b=%llu serial=%d blade=%d eligibility_us=%lld delay_us=0\n",
            contact.attacker,contact.target,contact.serial,contact.blade,K1AttackDeflection::windowUs);
    }
    for(const auto& contact:accepted) {
        if(cancelled(contact.attacker) || cancelled(contact.target) || !ContactActorsValid(contact)) continue;
        if(contact.poiseOnly && !ContactSwingCurrent(contact)) continue;
        auto* attacker=server->GetCreatureByGameObjectID(contact.attacker);
        auto* target=server->GetCreatureByGameObjectID(contact.target);
        // Accepted body hits enter the existing native damage service immediately.
        if(!attacker || !target || attacker->GetDead() || target->GetDead()) continue;
        if(meleePoise[contact.attacker].clashOpponent!=contact.target)ClearClashEpisode(contact.attacker);
        if(meleePoise[contact.target].clashOpponent!=contact.attacker)ClearClashEpisode(contact.target);
        // A parry or native status cancels all remaining dual-hand contacts,
        // including a second hand already queued during this scene update.
        if(contact.dualWeapon!=OBJECT_INVALID &&
           (!ParryAttackReady(contact.attacker,now) || PoiseBroken(attacker) || NativeStatusAnimation(attacker))) continue;
        const bool blocked=K1EngageGuardContact(contact.attacker,contact.target);
        damageContact=&contact;
        const bool damaged=!blocked && !contact.poiseOnly && ContactDamage(attacker,target,true);
        damageContact=nullptr;
        if(std::getenv("K1_ENGAGE_MELEE_HIT_TEST") && K1EngageOwnsPlayer(contact.attacker)) {
            auto& a=playerHitAudits[contact.serial];a.damaged+=damaged;a.blocked+=blocked;a.rejected+=!damaged&&!blocked;
        }
        if(damaged){++meleeDamageContacts;auto member=sandboxActors.find(contact.attacker);if(member!=sandboxActors.end()){
            ++member->second.meleeHits;if(contact.dualWeapon!=OBJECT_INVALID)member->second.dualDamageHands|=1u<<(contact.offhand?1:0);
        }}
        if(K1EngageOwnsPlayer(contact.attacker)) {
            if(!blocked && !damaged && contact.serial==attackSerial && (!contact.poiseOnly || !hitConsumed)) {
                if(!contact.poiseOnly) hitConsumed=false;
                // A busy native power round refused this hit. No accepted
                // damage/block has consumed it; permit a later physical retry.
                if(contact.blade>=0 && staffContactSerial==contact.serial)
                    staffContactBlades&=~(1u<<contact.blade);
                if(contact.dualWeapon!=OBJECT_INVALID && dualContactSerial==contact.serial)
                    dualContactHands&=~(1u<<(contact.offhand?1:0));
            }
        }
        else {
            auto found=npcPlayback.find(contact.attacker);
            if(found!=npcPlayback.end() && (found->second.staff || found->second.dual) && !blocked && !damaged && contact.serial==found->second.serial &&
               (!contact.poiseOnly || !found->second.hit)) {
                if(!contact.poiseOnly)found->second.hit=false;
                if(contact.blade>=0)found->second.contactBlades&=~(1u<<contact.blade);
            }
            fprintf(stderr,"K1_NPC_CONTACT actor=%llu serial=%d blocked=%d damage=%d\n",contact.attacker,contact.serial,blocked,damaged);
        }
        fprintf(stderr,"K1_MELEE_RESOLVED actor=%llu serial=%d delay_us=%lld\n",contact.attacker,contact.serial,now-contact.at);
    }
}
int UncoveredParryProbeCut();
bool ClashEpisodeProbe(CSWSCreature* player,CSWSCreature* enemy);
bool ClashContactProbe(CSWSCreature* player,CSWSCreature* enemy) {
    auto found=npcPlayback.find(enemy->m_objectId);if(found==npcPlayback.end()) return false;
    const auto savedState=state;const auto savedNpc=found->second;
    const auto savedAttackStart=playerAttackStartedAt;
    const auto savedPlayer=meleePoise[player->m_objectId],savedEnemy=meleePoise[enemy->m_objectId];
    const int playerHP=player->GetCurrentHitPoints(0),enemyHP=enemy->GetCurrentHitPoints(0);
    const long long sample=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    bool ok=true;
    MeleeContact p={player->m_objectId,enemy->m_objectId,attackSerial};
    MeleeContact n={enemy->m_objectId,player->m_objectId,found->second.serial};
    for(int order=0;order<2;++order) {
        state.attacking=true;state.chainSwings=3;state.attackRequested=state.holdChain=true;
        auto& npc=found->second;npc.attacking=true;npc.ranged=false;npc.chainSwings=3;
        meleePoise[player->m_objectId]=MeleePoise();meleePoise[enemy->m_objectId]=MeleePoise();
        MeleeContact first=order?n:p;
        playerAttackStartedAt=sample;npc.attackStartedAt=sample;
        const auto before=meleeClashes;
        ResolveMeleeContacts({first,first},sample);
        ok=ok && meleeClashes==before+1 &&
            meleePoise[player->m_objectId].percent==100-ClashPoiseDamage(enemy) &&
            meleePoise[enemy->m_objectId].percent==100-ClashPoiseDamage(player) &&
            !state.attacking && !state.chainSwings && !state.attackRequested && !state.holdChain &&
            !npc.attacking && !npc.chainSwings && npc.hit && !npc.bladeValid &&
            player->GetCurrentHitPoints(0)==playerHP && enemy->GetCurrentHitPoints(0)==enemyHP;
    }
    // Expired attack windows dispatch body hits immediately, without a clash.
    state.attacking=true;state.rightHeld=false;found->second.attacking=true;
    playerAttackStartedAt=sample-500001;found->second.attackStartedAt=sample-500001;
    meleePoise[player->m_objectId]=MeleePoise();meleePoise[enemy->m_objectId]=MeleePoise();
    const auto beforeClashes=meleeClashes;
    const auto beforeDamage=meleeDamageContacts;
    clashExpiryObservation={playerHP,enemyHP,player->GetCurrentHitPoints(1),enemy->GetCurrentHitPoints(1)};
    ResolveMeleeContacts({p,p},sample);
    ok=ok && meleeClashes==beforeClashes && meleeDamageContacts==beforeDamage+1;
    ResolveMeleeContacts({n,n},sample+1);
    ResolveMeleeContacts({},sample+1000000);
    ok=ok && meleeClashes==beforeClashes && meleeDamageContacts==beforeDamage+2 &&
        meleePoise[player->m_objectId].percent==100 && meleePoise[enemy->m_objectId].percent==100;
    // SignalMeleeDamage queues native EVENT_APPLY_EFFECT. The arena fixture
    // observes the resulting HP loss after server updates, outside this call.
    state.attacking=false;state.rightHeld=true;
    found->second.attacking=true;found->second.ranged=false;
    // This assertion is an ordinary block, deliberately outside the selected
    // parry quadrant even when the wider dial now selects this fixture enemy.
    found->second.direction=UncoveredParryProbeCut();
    meleePoise[player->m_objectId]=MeleePoise();meleePoise[enemy->m_objectId]=MeleePoise();
    ResolveMeleeContacts({n,n});
    ok=ok && meleePoise[player->m_objectId].percent==100-GuardPoiseDamage(enemy) && meleePoise[enemy->m_objectId].percent==100 &&
        found->second.attacking && player->GetCurrentHitPoints(0)==playerHP;
    state=savedState;found->second=savedNpc;playerAttackStartedAt=savedAttackStart;
    meleePoise[player->m_objectId]=savedPlayer;meleePoise[enemy->m_objectId]=savedEnemy;
    ok=ClashEpisodeProbe(player,enemy) && ok;
    fprintf(stderr,"K1_MELEE_CLASH_PROBE_%s orders=2 timed=1 eligibility_us=500000 immediate_body_hits=2 duplicate=1 all_clash_poise=10 hp=unchanged chains=0\n",ok?"PASS":"FAIL");
    return ok;
}
} // namespace
void K1EngagePreloadNavigation(CSWCArea* clientArea) {
    if(!clientArea || !K1EngageEnabled() || !std::getenv("K1_ENGAGE_ENEMY_AI") || !NpcCombatAreaActive()) return;
    auto* area=clientArea->GetServerArea();
    if(!area) return;
    npcNavigators.erase(area->m_objectId);
    // Client control-object assignment follows area geometry loading. The
    // authoritative player/path information is already available on the server.
    auto* anchor=g_pAppManager->m_pServerExoApp->GetPlayerCreature();
    if(!anchor || !anchor->m_pPathfindInfo) {
        fprintf(stderr,"K1_JKA_NAV_PRELOAD_FAIL area=%llu reason=actor_not_ready actor=%llu actor_area=%llu\n",
            area->m_objectId,anchor?anchor->m_objectId:OBJECT_INVALID,anchor&&anchor->GetArea()?static_cast<CSWSArea*>(anchor->GetArea())->m_objectId:OBJECT_INVALID);
        return;
    }
    fprintf(stderr,"K1_JKA_NAV_PRELOAD_BEGIN area=%llu phase=area_load\n",area->m_objectId);
    const auto started=std::chrono::steady_clock::now();
    JkaWorld world(anchor,area);
    auto graph=BuildNavigation(world);
    if(!graph) {fprintf(stderr,"K1_JKA_NAV_PRELOAD_FAIL area=%llu reason=graph\n",area->m_objectId);return;}
    PreparedNavigation prepared;
    prepared.graph=graph;prepared.area=area;
    prepared.space=anchor->m_pPathfindInfo->m_fPersonalSpace;
    prepared.height=anchor->m_pPathfindInfo->m_fHeight;
    prepared.generation=++navigationGeneration;
    npcNavigators[area->m_objectId]=prepared;
    const auto milliseconds=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
    fprintf(stderr,"K1_JKA_NAV_PRELOAD_PASS area=%llu phase=area_load geometry_queries=%llu rejected_origins=%d space=%g height=%g generation=%llu build_ms=%lld\n",
        area->m_objectId,world.geometryQueries,world.rejectedOrigins,prepared.space,prepared.height,prepared.generation,static_cast<long long>(milliseconds));
}
void K1EngageUnloadNavigation(unsigned long long areaId) {
    contactBodyRoots.clear();contactCurrentRoots.clear();
    SandboxUnload(areaId);
    ClearDismemberArea(areaId);
    if(areaId==OBJECT_INVALID){
        navigationUnloads+=npcNavigators.size();npcNavigators.clear();
        fprintf(stderr,"K1_JKA_NAV_SHUTDOWN_CLEAN\n");return;
    }
    if(npcNavigators.erase(areaId)) {++navigationUnloads;fprintf(stderr,"K1_JKA_NAV_CACHE_UNLOAD area=%llu\n",areaId);}
}
namespace {
// Guard quadrants clockwise: upper-right, lower-right, lower-left, upper-left.
// Incoming directions clockwise: up, UR, right, LR, down, LL, left, UL.
// Rising cuts follow the user's explicit motion-based lower-quadrant pairings.
int ParryIncomingForAttack(int direction) {
    const int incoming[]={0,1,7,2,6,5,3};
    return direction>=0 && direction<7?incoming[direction]:-1;
}
bool ParryCoversIncoming(int quadrant,int incoming) {
    const unsigned int coverage[]={0x07,0x18,0x30,0xc1};
    return quadrant>=0 && quadrant<4 && incoming>=0 && incoming<8 &&
        (coverage[quadrant] & (1u<<incoming))!=0;
}
int ParrySectorForAttack(int direction) {
    const int sectors[]={0,0,3,0,3,2,1};
    return direction>=0 && direction<7?sectors[direction]:-1;
}
int ParrySector(float right,float up,float deadZone) {
    if(!std::isfinite(right) || !std::isfinite(up) || std::hypot(right,up)<deadZone) return -1;
    // Cardinal-axis ties choose an upper guard for horizontal aim and the
    // right-hand guard for vertical aim; overlap belongs to coverage, not input.
    return up>=0 ? (right>=0?0:3) : (right>=0?1:2);
}
struct ParryAim {
    unsigned long long target=OBJECT_INVALID;
    int sector=-1;
    float right=0,up=0,score=0;
};
ParryAim ParryAimAt(CSWSCreature* enemy) {
    ParryAim aim;
    auto* player=ServerActor();auto* app=g_pAppManager->m_pClientExoApp;
    auto* module=app?app->GetModule():nullptr;
    auto* camera=module?module->m_pCamera:nullptr;
    if(!K1EngageActive() || !player || player->GetDead() || nativeActionBody || !camera ||
        !HasMeleePoise(player) || PoiseBroken(player) || NativeStatusAnimation(player) ||
        !enemy || enemy->GetDead() || enemy->GetArea()!=player->GetArea() || enemy->GetReputation(player->m_objectId,0)>10) return aim;
    auto found=npcPlayback.find(enemy->m_objectId);
    if(found==npcPlayback.end() || found->second.ranged || !HasMeleePoise(enemy) || NativeStatusAnimation(enemy)) return aim;
    const Vector to=enemy->m_position-player->m_position;
    const float distance=std::hypot(to.x,to.y);
    const float range=K1EngageEnemyMeleeRange(enemy)+player->m_pPathfindInfo->m_fCrePersonalSpace+parryMarkerExtraRange;
    if(distance>range || distance<.01f || std::fabs(to.z)>1.f || dot(player->m_orientation,to)<distance*.5f ||
        !K1EngageEnemyHasClearShot(enemy,player)) return aim;
    auto* body=found->second.body;
    auto* chest=body?body->GetPart(const_cast<char*>("torsoUpr_g")):nullptr;
    if(!chest) return aim;
    Vector reference;Quaternion unused;BindFrame(chest,reference,unused);
    // A bind-height anchor follows position without bobbing with the enemy's
    // current swing, so the selected guard does not twitch with torso animation.
    const Vector anchor=enemy->m_position+Vector(0,0,reference.z*body->m_fObjectScale);
    const Vector screen=camera->MapToScreen(anchor,true);
    int width=0,height=0;AurGUIGetScreenSize(width,height);
    if(width<=0 || height<=0 || !K1SaberContact::Finite(screen)) return aim;
    aim.right=(.5f-screen.x)*width;aim.up=(screen.y-.5f)*height;
    if(!ParryAimWithinArea(aim.right,aim.up,height)) return aim;
    aim.target=enemy->m_objectId;aim.score=std::hypot(aim.right,aim.up)+distance*.01f;
    aim.sector=ParrySector(aim.right,aim.up,8.f*height/720.f);
    return aim;
}
ParryAim SelectedParryAim() {
    ParryAim best;
    for(const auto& entry:npcPlayback) {
        auto* enemy=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(entry.first);
        const auto aim=ParryAimAt(enemy);
        if(aim.target!=OBJECT_INVALID && (best.target==OBJECT_INVALID || aim.score<best.score)) best=aim;
    }
    return best;
}
int UncoveredParryProbeCut() {
    const auto aim=SelectedParryAim();
    for(int direction=0;direction<7;++direction)
        if(!ParryCoversIncoming(aim.sector,ParryIncomingForAttack(direction))) return direction;
    return 0;
}
bool ParryRulesProbe() {
    bool ok=ParrySector(0,0,8)==-1 && ParrySector(7.99f,0,8)==-1 && ParrySector(8,0,8)==0 &&
        ParrySector(NAN,20,8)==-1 && ParrySectorForAttack(-1)==-1 && ParrySectorForAttack(7)==-1;
    for(int i=0;i<4;++i) {
        const float a=(i+.5f)*1.5707963268f;
        ok=ok && ParrySector(40*std::sin(a),40*std::cos(a),8)==i;
    }
    int played=-1;
    const bool soundGate=!SwingSoundDue(1,played,.229f,.23f) && SwingSoundDue(1,played,.23f,.23f) &&
        !SwingSoundDue(1,played,.25f,.23f) && !SwingSoundDue(1,played,.1f,.23f) &&
        !SwingSoundDue(2,played,.1f,.23f) && SwingSoundDue(2,played,.23f,.23f);
    fprintf(stderr,"K1_ENGAGE_SOUND_GATE_%s windup=1 repeated_frame=1 rewind=1 next_chain=1\n",soundGate?"PASS":"FAIL");
    ok=ok && soundGate;
    const int required[]={0,0,3,0,3,2,1};
    for(int attack=0;attack<7;++attack) ok=ok && ParrySectorForAttack(attack)==required[attack];
    const int blockRequired[]={0,1,2,1,2,4,3};
    for(int attack=0;attack<7;++attack) ok=ok && BlockReactionForAttack(attack)==blockRequired[attack];
    ok=ok && BlockReactionForAttack(-1)==-1 && BlockReactionForAttack(7)==-1;
    const unsigned int allowedGuards[]={9,1,1,2,6,4,8,8};
    for(int incoming=0;incoming<8;++incoming) for(int guard=0;guard<4;++guard)
        ok=ok && ParryCoversIncoming(guard,incoming)==((allowedGuards[incoming] & (1u<<guard))!=0);
    ok=ok && !ParryCoversIncoming(-1,0) && !ParryCoversIncoming(4,0) && !ParryCoversIncoming(0,8);
    int counts[7]={};for(int roll=0;roll<18;++roll) ++counts[NpcMeleeDirectionForRoll(roll)];
    const int expected[]={4,3,3,3,3,1,1};
    bool weights=true;for(int i=0;i<7;++i) weights=weights && counts[i]==expected[i];
    fprintf(stderr,"K1_NPC_CUT_WEIGHTS_%s top=4 upper=3,3 sides=3,3 rising=1,1 total=18 rate=%g\n",weights?"PASS":"FAIL",npcMeleeSwingRate);
    ok=ok && weights;
    fprintf(stderr,"K1_PARRY_RULES_%s sectors=4 cuts=7 overhead_overlap=1 bottom_overlap=1 dead_zone=1 invalid=1\n",ok?"PASS":"FAIL");
    return ok;
}
void ApplySuccessfulParry(CSWSCreature* attacker,CSWSCreature* defender) {
    const int bonus=ClashParryBonus(attacker->m_objectId,defender->m_objectId);
    ClearClashEpisode(attacker->m_objectId);ClearClashEpisode(defender->m_objectId);
    const int block=IncomingBlockReaction(attacker);
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    EndClashChain(attacker->m_objectId,now);
    if(defender==ServerActor()) {
        parrySuccessUntil=ContactWallUs()+300000;
        if(std::getenv("K1_ENGAGE_PARRY_TEST"))parryRenderRequested=true;
        fprintf(stderr,"K1_PARRY_SUCCESS_DIAL duration_ms=300 defender=%llu\n",defender->m_objectId);
    }
    auto& recoveringPoise=meleePoise[defender->m_objectId];
    if(!recoveringPoise.wasEligible || now<recoveringPoise.quietUntil) NotePoiseActivity(defender,"parry_guard");
    auto& defenderPoise=meleePoise[defender->m_objectId];
    const int before=defenderPoise.percent;
    RewardParryPoise(defenderPoise,ParryPoiseReward(defender));
    fprintf(stderr,"K1_PARRY_POISE_REWARD actor=%llu before=%d after=%d gain=%d\n",
        defender->m_objectId,before,defenderPoise.percent,defenderPoise.percent-before);
    SpendMeleePoise(attacker,defender->m_objectId,ParriedPoiseLoss(attacker)+bonus,"parried");
    fprintf(stderr,"K1_CLASH_PARRY_BONUS attacker=%llu defender=%llu bonus=%d\n",attacker->m_objectId,defender->m_objectId,bonus);
    auto& poise=meleePoise[attacker->m_objectId];poise.parriedUntil=now+parryAttackDelayUs;
    auto npc=npcPlayback.find(attacker->m_objectId);
    if(npc!=npcPlayback.end()) npc->second.nextAttack=std::max(npc->second.nextAttack,poise.parriedUntil);
    QueueBlockReaction(defender,block);
    EngageWeaponSound(attacker,defender,"parry");
    fprintf(stderr,"K1_PARRY_LOCKOUT actor=%llu start=%lld until=%lld\n",attacker->m_objectId,now,poise.parriedUntil);
}
bool ClashEpisodeProbe(CSWSCreature* player,CSWSCreature* enemy) {
    const auto savedState=state;const auto savedNpc=npcPlayback[enemy->m_objectId];
    const auto savedPlayer=meleePoise[player->m_objectId],savedEnemy=meleePoise[enemy->m_objectId];
    const int savedSelection=redSelection;redSelection=0;
    bool ok=true;
    auto reset=[&]{ClearClashEpisode(player->m_objectId);ClearClashEpisode(enemy->m_objectId);
        meleePoise[player->m_objectId]=MeleePoise();meleePoise[enemy->m_objectId]=MeleePoise();};
    reset();
    for(int i=1;i<=5;++i){ApplyClashPoise(player,enemy);
        ok=ok && meleePoise[player->m_objectId].percent==100-10*i && meleePoise[enemy->m_objectId].percent==100-10*i &&
            ClashParryBonus(player->m_objectId,enemy->m_objectId)==5*(i-1);}
    // Real parry service consumes the episode before applying its base loss.
    ApplySuccessfulParry(player,enemy);
    ok=ok && meleePoise[player->m_objectId].percent==5 && !meleePoise[player->m_objectId].clashes && !meleePoise[enemy->m_objectId].clashes;
    for(const char* interruption:{"guard_hit","damage","hit"}) {
        reset();ApplyClashPoise(player,enemy);ApplyClashPoise(player,enemy);
        NotePoiseActivity(enemy,interruption);
        ok=ok && !ClashParryBonus(player->m_objectId,enemy->m_objectId) && !meleePoise[player->m_objectId].clashes;
    }
    reset();ApplyClashPoise(player,enemy);NotePoiseActivity(player,"swing");NotePoiseActivity(player,"swing");
    ok=ok && !meleePoise[enemy->m_objectId].clashes; // missed swing followed by another swing
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    reset();ApplyClashPoise(player,enemy);
    const int oldStatus=enemy->m_nState;enemy->m_nState=EFFECT_STATE_STUNNED;
    ValidateClashEpisodes(now);enemy->m_nState=oldStatus;
    ok=ok && !meleePoise[player->m_objectId].clashes;
    reset();ApplyClashPoise(player,enemy);npcPlayback[enemy->m_objectId].target=enemy->m_objectId;
    ValidateClashEpisodes(now);npcPlayback[enemy->m_objectId].target=savedNpc.target;
    ok=ok && !meleePoise[player->m_objectId].clashes;
    reset();ApplyClashPoise(player,enemy);meleePoise[player->m_objectId].clashRight=OBJECT_INVALID;
    ValidateClashEpisodes(now);ok=ok && !meleePoise[enemy->m_objectId].clashes;
    for(int count:{0,1,2,5,20,21}) {
        int successes=0;
        for(int roll=1;roll<=100;++roll){MeleePoise p;p.clashes=count;
            const bool chosen=ChooseClashParry(p,roll);successes+=chosen;
            ok=ok && ChooseClashParry(p,101-roll)==chosen;}
        ok=ok && successes==std::min(100,count*5);
    }
    reset();redSelection=savedSelection;state=savedState;npcPlayback[enemy->m_objectId]=savedNpc;
    meleePoise[player->m_objectId]=savedPlayer;meleePoise[enemy->m_objectId]=savedEnemy;
    fprintf(stderr,"K1_CLASH_EPISODE_%s five_cost=50 yellow_parry=45 consumed=1 reset=guard,damage,miss,status,opponent,equipment chance=0,5,10,25,100 cached=1\n",ok?"PASS":"FAIL");
    return ok;
}
bool ClashNpcPairProbe(CSWSCreature* a,CSWSCreature* b) {
    const auto aid=a->m_objectId,bid=b->m_objectId;
    const auto savedA=npcPlayback[aid],savedB=npcPlayback[bid];
    const auto pa=meleePoise[aid],pb=meleePoise[bid];
    const Vector facingA=a->m_orientation,facingB=b->m_orientation;
    const int hpA=a->GetCurrentHitPoints(0),hpB=b->GetCurrentHitPoints(0);
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    auto* varA=a->m_scriptVars.MatchIndex(CExoString(npcParryPercentVariable),CSWSScriptVarTable::VARTYPE_INTEGER,0);
    auto* varB=b->m_scriptVars.MatchIndex(CExoString(npcParryPercentVariable),CSWSScriptVarTable::VARTYPE_INTEGER,0);
    const bool hadA=varA!=nullptr,hadB=varB!=nullptr;const int oldA=varA?varA->m_nInt:0,oldB=varB?varB->m_nInt:0;
    a->m_scriptVars.SetInt(CExoString(npcParryPercentVariable),0,0);b->m_scriptVars.SetInt(CExoString(npcParryPercentVariable),0,0);
    bool ok=true;
    for(int order=0;order<2;++order) {
        auto* attacker=order?b:a;auto* defender=order?a:b;
        const auto attackId=attacker->m_objectId,defendId=defender->m_objectId;
        ClearClashEpisode(aid);ClearClashEpisode(bid);
        meleePoise[aid]=MeleePoise();meleePoise[bid]=MeleePoise();
        auto& x=npcPlayback[attackId];auto& y=npcPlayback[defendId];
        x.target=defendId;y.target=attackId;x.ranged=y.ranged=false;
        a->SetOrientation(b->m_position-a->m_position);b->SetOrientation(a->m_position-b->m_position);
        for(int i=1;i<=5;++i) {
            x.attacking=y.attacking=true;x.parrying=y.parrying=false;
            ++x.serial;++y.serial;x.attackStartedAt=y.attackStartedAt=now;
            MeleeContact c={attackId,defendId,x.serial};
            const auto before=meleeClashes;ResolveMeleeContacts({c,c},now);
            ok=ok && meleeClashes==before+1 && meleePoise[aid].percent==100-10*i && meleePoise[bid].percent==100-10*i;
        }
        const auto parries=npcTimedParries;
        ok=CommitClashParry(y,now,25) && ok;
        x.attacking=true;++x.serial;y.closeGuard=true;
        MeleeContact next={attackId,defendId,x.serial};ResolveMeleeContacts({next,next},now);
        ok=ok && npcTimedParries==parries+1 && !x.attacking && !y.attacking &&
            meleePoise[attackId].percent==50-ParriedPoiseLoss(attacker)-20 &&
            !meleePoise[aid].clashes && !meleePoise[bid].clashes;
    }
    ok=ok && a->GetCurrentHitPoints(0)==hpA && b->GetCurrentHitPoints(0)==hpB;
    ClearClashEpisode(aid);ClearClashEpisode(bid);
    meleePoise[aid]=MeleePoise();meleePoise[bid]=MeleePoise();ApplyClashPoise(a,b);
    ok=CommitClashParry(npcPlayback[aid],now,1) && CommitClashParry(npcPlayback[bid],now,1) && ok;
    ValidateClashEpisodes(now+2000000);
    ok=ok && !meleePoise[aid].clashes && !meleePoise[bid].clashes && !npcPlayback[aid].parrying && !npcPlayback[bid].parrying;
    ClearClashEpisode(aid);ClearClashEpisode(bid);
    npcPlayback[aid]=savedA;npcPlayback[bid]=savedB;meleePoise[aid]=pa;meleePoise[bid]=pb;
    a->SetOrientation(facingA);b->SetOrientation(facingB);
    if(hadA)a->m_scriptVars.SetInt(CExoString(npcParryPercentVariable),oldA,0);else a->m_scriptVars.DestroyInt(CExoString(npcParryPercentVariable));
    if(hadB)b->m_scriptVars.SetInt(CExoString(npcParryPercentVariable),oldB,0);else b->m_scriptVars.DestroyInt(CExoString(npcParryPercentVariable));
    fprintf(stderr,"K1_CLASH_NPC_PAIR_%s orders=2 clashes=5 duplicate=1 chance=25 ordinary_chance=0 parry_bonus=20 mutual_guard_timeout=1 hp=unchanged\n",ok?"PASS":"FAIL");
    return ok;
}
bool DirectionalParry(CSWSCreature* attacker,CSWSCreature* defender) {
    if(!attacker || !defender || !K1EngageOwnsPlayer(defender->m_objectId)) return false;
    auto found=npcPlayback.find(attacker->m_objectId);
    if(found==npcPlayback.end() || !found->second.attacking || found->second.ranged || PoiseBroken(attacker)) return false;
    // The reticle selects one display target. Another nearby enemy must not
    // veto a correctly aimed guard against the blade that actually contacts us.
    const auto aim=ParryAimAt(attacker);
    if(aim.target!=attacker->m_objectId || !ParryCoversIncoming(aim.sector,ParryIncomingForAttack(found->second.direction))) {
        const auto selected=SelectedParryAim();
        fprintf(stderr,"K1_PARRY_BLOCK_REASON attacker=%llu serial=%d direction=%d sector=%d eligible=%d aim=%g,%g display_target=%llu display_sector=%d\n",
            attacker->m_objectId,found->second.serial,found->second.direction,aim.sector,aim.target==attacker->m_objectId,
            aim.right,aim.up,selected.target,selected.sector);
        return false;
    }
    const int direction=found->second.direction;
    ApplySuccessfulParry(attacker,defender);
    ++directionalParries;
    fprintf(stderr,"K1_DIRECTIONAL_PARRY attacker=%llu defender=%llu attack=%d sector=%d poise=%d,%d chain=0\n",
        attacker->m_objectId,defender->m_objectId,direction,aim.sector,meleePoise[attacker->m_objectId].percent,meleePoise[defender->m_objectId].percent);
    return true;
}
} // namespace
bool K1EngageBlocksMelee(unsigned long long attackerId, unsigned long long targetId) {
    auto* defender=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(targetId);
    if(!HasMeleePoise(defender) || PoiseBroken(defender) || NativeStatusAnimation(defender)) return false;
    const auto found=npcPlayback.find(targetId);
    if(found!=npcPlayback.end() && NpcGuarding(found->second) && (K1EngageOwnsPlayer(attackerId) || K1EngageOwnsEnemy(attackerId))) {
        auto* target=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(targetId);
        auto* attacker=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(attackerId);
        if(target && attacker && target->GetReputation(attackerId,0)<=10 && (target->m_nAIState & 0x84)==0x84 && dot(target->m_orientation,attacker->m_position-target->m_position)>=0) {
            ++found->second.guardBlocks;
            fprintf(stderr,"K1_NPC_GUARD_BLOCK actor=%llu attacker=%llu\n",targetId,attackerId);return true;
        }
    }
    if (!K1EngageActive() || !state.Guarding() || !K1EngageOwnsPlayer(targetId)) return false;
    CSWSCreature* actor = ServerActor();
    if (actor && actor->m_pCombatRound && actor->m_pCombatRound->m_bRoundStarted) return false;
    CSWSCreature* attacker = g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(attackerId);
    if (!attacker || !actor || attacker->GetArea()!=actor->GetArea()) return false;
    Vector to = attacker->m_position - actor->m_position;
    bool front = actor->m_orientation.x*to.x + actor->m_orientation.y*to.y >= 0;
    if (front) fprintf(stderr,"K1_ENGAGE block attacker=%llu target=%llu\n",attackerId,targetId);
    return front;
}
bool K1EngageGuardContact(unsigned long long attackerId,unsigned long long targetId) {
    if(!K1EngageBlocksMelee(attackerId,targetId)) return false;
    if(meleePoise[attackerId].clashOpponent!=targetId)ClearClashEpisode(attackerId);
    if(meleePoise[targetId].clashOpponent!=attackerId)ClearClashEpisode(targetId);
    auto* actor=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(targetId);
    auto* attacker=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(attackerId);
    if(DirectionalParry(attacker,actor)) return true;
    const auto npc=npcPlayback.find(targetId);
    if(npc!=npcPlayback.end() && npc->second.parrying && !npc->second.ranged &&
        attacker && (K1EngageOwnsPlayer(attackerId)?state.attacking:(npcPlayback.count(attackerId) && npcPlayback[attackerId].attacking)) && HasMeleePoise(attacker) && !PoiseBroken(attacker) &&
        ((meleePoise[targetId].clashOpponent==attackerId && meleePoise[targetId].clashParry) ||
         NpcContactParry(actor,K1EngageOwnsPlayer(attackerId)?attackSerial:npcPlayback[attackerId].serial,attackerId))) {
        ApplySuccessfulParry(attacker,actor);++npcTimedParries;
        fprintf(stderr,"K1_NPC_TIMED_PARRY attacker=%llu defender=%llu poise=%d,%d chain=0\n",
            attackerId,targetId,meleePoise[attackerId].percent,meleePoise[targetId].percent);
        return true;
    }
    // A baton can intercept with a successful parry, but cannot absorb a hit
    // through an ordinary held guard. Leave HP/poise damage to native contact.
    auto* guardingItem=NativeNpcRightItem(actor);
    if(guardingItem && guardingItem->m_baseItem==BASE_ITEM_STUN_BATON)return false;
    const int block=IncomingBlockReaction(attacker);
    const int poiseDamage=GuardPoiseDamage(attacker);
    SpendMeleePoise(actor,attackerId,poiseDamage,"guard_hit");
    if(StaffPoiseActor(attacker)) fprintf(stderr,"K1_STAFF_GUARD_DAMAGE attacker=%llu target=%llu points=%d\n",attackerId,targetId,poiseDamage);
    QueueBlockReaction(actor,block);
    EngageWeaponSound(attacker,actor,"guard");
    // The breaking hit was guarded; following hits find the guard disabled.
    return true;
}
void K1EngagePoiseDamaged(CSWSObject* target) {
    if(K1EngageEnabled() && target) NotePoiseActivity(target->AsSWSCreature(),"damage");
}
bool K1EngageIncomingMelee(CSWSCreature* attacker, CSWSObject* target) {
    return K1EngageEnabled() && target && K1EngageOwnsPlayer(target->m_objectId)
        && attacker && attacker->m_objectId != target->m_objectId;
}
bool K1EngageIncomingContact(CSWSCreature* attacker, CSWSObject* target) {
    if (!attacker || !target || attacker->GetArea()!=target->GetArea()) return false;
    // NPCs retain native swing timing for this prototype. Recheck reach/facing at impact
    // so leaving the swing's range is a miss, independently of defense/accuracy rolls.
    Vector delta=target->m_position-attacker->m_position;
    float squared=delta.x*delta.x+delta.y*delta.y;
    return squared<=4.0f && std::fabs(delta.z)<=1.5f &&
        attacker->m_orientation.x*delta.x+attacker->m_orientation.y*delta.y>=0;
}
void CSWGuiMainInterface::OnEngageHover(CSWGuiControl* control) {
    if (control->m_tail.nCurrentEventArg && control->m_tail.bEnabled)
        m_pGuiManager->PlayGuiSound(GUISOUND_ENTERED_DEFAULT);
}
extern "C" void k1host_trace_poise_shader(const char* stage);
extern "C" void* k1host_save_preview_program();
extern "C" void k1host_restore_preview_program(void* saved);
extern "C" float k1host_get_alpha_test_reference();
extern "C" void k1host_restore_alpha_test_reference(float reference);
extern "C" int k1host_get_texture_environment_mode();
struct PreviewTextureState {
    int active=0,texture[4][2],cachedTexture[6];unsigned int cachedStage;
    int blendSrc=0,blendDst=0,depthMask=0,programId=0;bool blend=false;void* program=nullptr;
    bool enabled[4][2];float color[4];bool restored=false;
    PreviewTextureState() {
        program=k1host_save_preview_program();
        ASLgl::glGetIntegerv(0x8B8D,&programId); // GL_CURRENT_PROGRAM
        ASLgl::glGetIntegerv(GL_BLEND_SRC,&blendSrc);ASLgl::glGetIntegerv(GL_BLEND_DST,&blendDst);
        ASLgl::glGetIntegerv(0x0B72,&depthMask); // GL_DEPTH_WRITEMASK
        blend=ASLgl::glIsEnabled(GL_BLEND)!=0;
        cachedStage=GLRender::cm_uiCurrentStage;
        for(int unit=0;unit<6;++unit) cachedTexture[unit]=GLRender::cm_nGLCurrentTID[unit];
        ASLgl::glGetIntegerv(0x84E0,&active);ASLgl::glGetFloatv(GL_CURRENT_COLOR,color); // GL_ACTIVE_TEXTURE
        for(int unit=0;unit<4;++unit) {
            ASLgl::glActiveTexture(GL_TEXTURE0+unit);
            ASLgl::glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture[unit][0]);
            ASLgl::glGetIntegerv(0x8514,&texture[unit][1]); // GL_TEXTURE_BINDING_CUBE_MAP
            enabled[unit][0]=ASLgl::glIsEnabled(GL_TEXTURE_2D)!=0;
            enabled[unit][1]=ASLgl::glIsEnabled(GL_TEXTURE_CUBE_MAP)!=0;
        }
        ASLgl::glActiveTexture(active);
    }
    void Restore() {
        if(restored) return;restored=true;
        k1host_restore_preview_program(program);
        ASLgl::glBlendFunc(blendSrc,blendDst);
        if(blend) ASLgl::glEnable(GL_BLEND);else ASLgl::glDisable(GL_BLEND);
        ASLgl::glDepthMask(depthMask!=0);
        for(int unit=0;unit<4;++unit) {
            ASLgl::glActiveTexture(GL_TEXTURE0+unit);
            for(int target=0;target<2;++target) {
                const auto cap=target?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;
                ASLgl::glBindTexture(cap,texture[unit][target]);
                if(enabled[unit][target]) ASLgl::glEnable(cap);else ASLgl::glDisable(cap);
            }
        }
        ASLgl::glActiveTexture(active);ASLgl::glColor4f(color[0],color[1],color[2],color[3]);
        // Raw GL restoration must also restore Aurora's binding cache.
        // Otherwise later materials can skip a bind using the preview's ID.
        GLRender::cm_uiCurrentStage=cachedStage;
        for(int unit=0;unit<6;++unit) GLRender::cm_nGLCurrentTID[unit]=cachedTexture[unit];
    }
    ~PreviewTextureState(){Restore();}
};

namespace {
void DrawSaberSurfaceMarks(CAurScene* scene) {
    auto* module=g_pAppManager->m_pClientExoApp->GetModule();
    if(!K1EngageEnabled() || !module || !module->m_pArea || module->m_pArea->m_pScene!=scene) return;
    struct Mark {
        SafePointer<CAurObject> owner;
        bool world=false;
        K1JkaSaberMarks::Polygon polygon;
        long long born=0;float color[3];
    };
    struct Trail {SafePointer<CAurObject> surface;CAurRoom* room=nullptr;Vector point;unsigned long long weapon=OBJECT_INVALID;bool valid=false;long long soundNext=0;};
    static std::vector<Mark> marks;
    static std::unordered_map<unsigned long long,Trail> trails;
    static SafePointer<CAurScene> previousScene;
    static long long previousTime=-1;
    static unsigned int textures[2]={};
    static Trail fixtureTrail;
    static int fixtureFrames=0;
    static Vector fixtureOrigin;
    static bool fixtureDrawn=false;
    static bool surfacesProbed=false;
    static bool glowExpired=false,scorchExpired=false;
    static bool testedContactSound=false;
    static int diagnosticSamples=0;
    static const bool mathOk=[] {
        bool ok=K1JkaSaberMarks::Probe();
        fprintf(stderr,"K1_JKA_SABER_MARK_MATH_%s\n",ok?"PASS":"FAIL");return ok;
    }();
    if(!mathOk) return;
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    if(previousScene.m_pObject!=scene || now<previousTime) {
        marks.clear();trails.clear();fixtureTrail=Trail();fixtureFrames=0;fixtureDrawn=false;surfacesProbed=false;glowExpired=scorchExpired=false;
        previousScene=scene;previousTime=-1;
    }
    const bool sample=now!=previousTime;previousTime=now;
    if(sample && std::getenv("K1_ENGAGE_SURFACE_MARK_TEST") && diagnosticSamples++<8)
        fprintf(stderr,"K1_JKA_SABER_MARK_SAMPLE active=%d actor=%d\n",K1EngageActive(),Actor()!=nullptr);
    // VFX randomness belongs to this client effect, never native gameplay or
    // shared CRT random state. The fixed local seed also makes probes repeatable.
    static std::minstd_rand visualRandom(1);
    auto random=[] {return std::generate_canonical<float,24>(visualRandom);};
    if(sample) {
        auto contact=[&](CSWCCreature* actor,Trail& trail,int fixture=0,const Vector* from=nullptr,const Vector* to=nullptr,int lane=0) {
            if(!actor) return;
            const auto blades=PoweredSaberBlades(actor);
            const int hand=lane<int(blades.size())?blades[lane].hand:0;
            auto* server=actor->GetServerCreature();
            auto* item=server && server->m_pInventory?server->m_pInventory->GetItemInSlot(hand?CSWInventory::SLOT_LEFTWEAPON:CSWInventory::SLOT_RIGHTWEAPON):nullptr;
            // Gate the equipped item's native type, not a luminous mesh (the
            // poise HUD can display a saber even when an ordinary sword is held).
            if(!item || !item->IsLightsaber()) {trail.valid=false;return;}
            Vector base,tip;
            if(lane>=int(blades.size())) {if(fixture) fprintf(stderr,"K1_JKA_SABER_MARK_FIXTURE_NO_BLADE\n");trail.valid=false;return;}
            base=blades[lane].blade.base;tip=blades[lane].blade.tip;
            if(from && to) {base=*from;tip=*to;}
            else if(fixture) {
                if(!fixtureFrames) fixtureOrigin=actor->m_position+actor->m_orientation*1.6f;
                base=fixtureOrigin+Vector(.02f*fixtureFrames,0,.3f);
                tip=base-Vector(0,0,.6f);++fixtureFrames;
            }
            CHitInfo hit(0,CHitInfo::kMaterialAllButTrigger,0);
            hit.m_bSkipCharactersAndItems=1;hit.m_bHonorIgnoreHitCheck=1;
            Vector point;
            const int hitResult=scene->HitCheck(base,tip,&point,&hit);
            if(fixture) fprintf(stderr,"K1_JKA_SABER_MARK_FIXTURE frame=%d hit=%d normal=%g,%g,%g room=%d object=%d\n",
                fixtureFrames,hitResult,hit.m_vHitNormal.x,hit.m_vHitNormal.y,hit.m_vHitNormal.z,hit.m_pHitRoom!=nullptr,hit.m_hitObject.m_pObject!=nullptr);
            if(!hitResult) {
                if(trail.valid && std::getenv("K1_ENGAGE_SURFACE_MARK_TEST"))
                    fprintf(stderr,"K1_JKA_SABER_MARK_MISS_RESET_PASS fixture=%d\n",fixture);
                trail.valid=false;return;
            }
            CAurObject* surface=hit.m_hitObject.m_pObject;
            auto* room=surface?nullptr:static_cast<CAurRoom*>(hit.m_pHitRoom);
            Gob* gob=dynamic_cast<Gob*>(surface);
            const bool world=room && room->m_pNodeData;
            if(gob && magnitude(hit.m_vHitNormal)<.5f && hit.m_pszHitPartName) {
                std::vector<K1SaberContact::Triangle> hitPart;
                CollisionPartTriangles(gob->GetPart(hit.m_pszHitPartName),hitPart);
                const bool recovered=K1JkaSaberMarks::HitNormal(point,hitPart,hit.m_vHitNormal);
                if(fixture) fprintf(stderr,"K1_JKA_SABER_MARK_NORMAL_%s part=%s triangles=%zu\n",
                    recovered?"PASS":"FAIL",hit.m_pszHitPartName,hitPart.size());
            }
            if(!K1SaberContact::Finite(hit.m_vHitNormal) || magnitude(hit.m_vHitNormal)<.5f) {trail.valid=false;return;}
            if(fixture) fprintf(stderr,"K1_JKA_SABER_MARK_RECEIVER room=%s optional_gob=%d meshes=%d world=%d\n",
                room?room->m_szName:"none",room&&room->m_pObject!=nullptr,
                world?room->m_pNodeData->m_meshes.m_count:0,world);
            const auto weapon=actor->m_aEquippedItemIds[4+hand];
            if((gob || world) && (fixture==0 || fixture==2) && now>=trail.soundNext && g_pExoSound) {
                const char* name="jkscorch1";
                const bool trace=std::getenv("K1_ENGAGE_SURFACE_MARK_TEST")!=nullptr;
                // Loose WAV resources share a native audio-cache ID here. One
                // original JKA cue avoids cross-variant reuse without changing
                // the general sound cache or other combat audio.
                constexpr unsigned char volume=20; // faint:20/127, per voice; keep SFX slider intact
                if(trace) fprintf(stderr,"K1_JKA_SCORCH_SOUND_BEGIN actor=%llu resource=%s volume=%u now=%lld due=%lld fixture=%d\n",
                    actor->m_objectId,name,unsigned(volume),now,trail.soundNext,fixture);
                const int length=g_pExoSound->Play3DOneShotSound(CResRef(name),point,0,PRIORITY_GROUP_COMBAT,0,volume,0,0);
                // JKA uses a100ms debounce and replaces CHAN_ITEM. K12 one-shots
                // have no such replacement channel: wait for the clip to finish.
                trail.soundNext=now+K1JkaSaberMarks::SoundInterval(length);
                if(trace && length>0) testedContactSound=true;
                if(trace) fprintf(stderr,"K1_JKA_SCORCH_SOUND_%s actor=%llu resource=%s length_ms=%d next=%lld\n",
                    length>0?"PASS":"FAIL",actor->m_objectId,name,length,trail.soundNext);
            }
            const Vector previous=gob?gob->m_vPosition+Rotate(gob->m_qOrientation,trail.point*gob->m_fObjectScale):trail.point;
            const bool sameReceiver=trail.surface.m_pObject==surface && trail.room==room;
            const bool continuous=K1JkaSaberMarks::Connect(trail.valid,sameReceiver,trail.weapon==weapon,previous,point);
            const bool firstContact=!trail.valid || !sameReceiver || trail.weapon!=weapon || magnitude(point-previous)>=1.f;
            if((gob || world) && (continuous || firstContact)) {
                std::vector<K1SaberContact::Triangle> triangles;
                if(gob) CollisionTriangles(gob,triangles);
                else for(int i=0;i<room->m_pNodeData->m_meshes.m_count;++i)
                {
                    auto* part=room->m_pNodeData->m_meshes.m_pData[i];
                    if(fixture && fixtureFrames==2) {
                        auto* node=part->m_pNode->AsMdlNodeTriMesh();
                        fprintf(stderr,"K1_JKA_SABER_MARK_MESH part=%s draw=%d render=%d vertices=%d raw=%d faces=%d skin=%d\n",
                            part->Name(),part->m_bDrawEnabled,node?node->m_bRender:0,node?node->m_nVertexCount:0,
                            node&&node->m_paVertices,node?node->m_faces.m_count:0,part->AsPartSkin()!=nullptr);
                    }
                    CollisionPartTriangles(part,triangles,true);
                }
                // A one-frame contact still burns the actual hit point. Give
                // the projector a tiny tangent footprint, never bridge a miss
                // or a receiver/weapon change into an invented long swipe.
                Vector tangent=cross(hit.m_vHitNormal,Vector(0,0,1));
                if(magnitude(tangent)<.001f)tangent=cross(hit.m_vHitNormal,Vector(0,1,0));
                tangent=normalize(tangent)*.00025f;
                auto polygons=K1JkaSaberMarks::Create(continuous?previous:point-tangent,continuous?point:point+tangent,hit.m_vHitNormal,triangles,random);
                if(fixture) fprintf(stderr,"K1_JKA_SABER_MARK_PROJECTION triangles=%zu fragments=%zu\n",triangles.size(),polygons.size());
                const Quaternion inverse=gob?Inverse(normalize(gob->m_qOrientation)):Quaternion();
                for(auto& polygon:polygons) {
                    Mark mark;mark.owner=surface;mark.world=world;mark.born=now;mark.polygon=std::move(polygon);
                    mark.color[0]=(215+random()*40)/255.f;mark.color[1]=(96+random()*32)/255.f;mark.color[2]=random()*15/255.f;
                    if(gob) for(auto& v:mark.polygon) v.point=Rotate(inverse,v.point-gob->m_vPosition)*(1.f/gob->m_fObjectScale);
                    marks.push_back(std::move(mark));
                }
                if(!polygons.empty() && std::getenv("K1_ENGAGE_SURFACE_MARK_TEST"))
                    fprintf(stderr,"K1_JKA_SABER_MARK_CONTACT actor=%llu triangles=%zu fragments=%zu length=%g fixture=%d world=%d\n",
                        actor->m_objectId,triangles.size(),polygons.size(),magnitude(point-previous),fixture,world);
            }
            trail.surface=surface;trail.room=room;
            trail.point=gob?Rotate(Inverse(normalize(gob->m_qOrientation)),point-gob->m_vPosition)*(1.f/gob->m_fObjectScale):point;
            trail.weapon=weapon;trail.valid=gob || world;
        };
        auto liveContact=[&](CSWCCreature* actor) {
            const auto blades=PoweredSaberBlades(actor);
            // Independent continuity per blade: offhand and opposite staff
            // tips must never be joined into one scar across empty space.
            for(int lane=0;lane<4;++lane) {
                auto& trail=trails[actor->m_objectId*4+lane];
                if(lane<int(blades.size()))contact(actor,trail,0,nullptr,nullptr,lane);
                else trail.valid=false;
            }
        };
        if(auto* actor=Actor()) {
            liveContact(actor);
            // Installed-data synthetic full-segment contact fixture. It exercises
            // the real floor mesh/normal/projection/draw without moving actors or
            // claiming the fixture's ray is a live animated blade.
            const bool fixtureBladesReady=PoweredSaberBlades(actor).size()==size_t(DualMode() || StaffMode()?2:1);
            if(std::getenv("K1_ENGAGE_SURFACE_MARK_TEST") && K1EngageActive() && fixtureBladesReady && fixtureFrames<5)
                contact(actor,fixtureTrail,true);
            if(std::getenv("K1_ENGAGE_SURFACE_MARK_TEST") && fixtureDrawn && !surfacesProbed) {
                auto probe=[&](const std::vector<K1SaberContact::Triangle>& triangles,CAurObject* expected,bool wall) {
                    for(const auto& triangle:triangles) {
                        Vector normal=cross(triangle.b-triangle.a,triangle.c-triangle.a);
                        if(magnitude(normal)<1e-8f) continue;
                        normal=normalize(normal);if(wall && std::fabs(normal.z)>.25f) continue;
                        Vector center=(triangle.a+triangle.b+triangle.c)*(1.f/3.f);
                        Vector tangent=normalize(triangle.b-triangle.a);
                        for(float side:{1.f,-1.f}) {
                            Vector a=center+normal*(side*.06f),b=center-normal*(side*.06f),point;
                            CHitInfo hit(0,CHitInfo::kMaterialAllButTrigger,0);hit.m_bSkipCharactersAndItems=1;hit.m_bHonorIgnoreHitCheck=1;
                            if(!scene->HitCheck(a,b,&point,&hit) || (expected && hit.m_hitObject.m_pObject!=expected) ||
                                (!expected && !hit.m_pHitRoom) || magnitude(point-center)>.01f ||
                                (wall && std::fabs(hit.m_vHitNormal.z)>.25f)) continue;
                            const size_t before=marks.size();Trail testTrail;
                            contact(actor,testTrail,true,&a,&b);
                            a=a+tangent*.02f;b=b+tangent*.02f;
                            contact(actor,testTrail,true,&a,&b);
                            if(marks.size()>before) return true;
                        }
                    }
                    return false;
                };
                bool wall=false,object=false;int props=0;
                {
                    auto* item=actor->GetServerCreature()->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
                    auto* client=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[4]);
                    const int original=item->m_baseItem;const auto powered=client->m_bPowered;
                    auto* offhandClient=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[5]);
                    const auto offhandPowered=offhandClient?offhandClient->m_bPowered:0;
                    // These classification cases isolate the right weapon;
                    // a legitimate powered offhand must not invalidate them.
                    if(offhandClient)offhandClient->m_bPowered=0;
                    bool ok=true;int swords=0,sabers=0;
                    Vector a=fixtureOrigin+Vector(0,0,.3f),b=a-Vector(0,0,.6f);
                    Vector c=a+Vector(.02f,0,0),d=b+Vector(.02f,0,0);
                    // Keep the actual luminous model equipped: classification
                    // must reject swords even when a saber-shaped mesh exists.
                    for(int type:{BASE_ITEM_LONG_SWORD,BASE_ITEM_VIBRO_SWORD,BASE_ITEM_SHORT_SWORD,BASE_ITEM_DOUBLE_BLADED_SWORD}) {
                        item->m_baseItem=type;Trail test;size_t count=marks.size();
                        contact(actor,test,false,&a,&b);contact(actor,test,false,&c,&d);
                        ok &= marks.size()==count && !test.valid && test.soundNext==0;++swords;
                    }
                    for(int type:{BASE_ITEM_LIGHTSABER,BASE_ITEM_DOUBLE_BLADED_LIGHTSABER,BASE_ITEM_SHORT_LIGHTSABER}) {
                        item->m_baseItem=type;Trail test;size_t count=marks.size();
                        contact(actor,test,true,&a,&b);contact(actor,test,true,&c,&d);
                        ok &= marks.size()>count && test.valid;++sabers;
                    }
                    item->m_baseItem=original;client->m_bPowered=0;
                    Trail off;size_t count=marks.size();contact(actor,off,false,&a,&b);contact(actor,off,false,&c,&d);
                    ok &= marks.size()==count && !off.valid && off.soundNext==0;
                    client->m_bPowered=powered;
                    if(offhandClient)offhandClient->m_bPowered=offhandPowered;
                    fprintf(stderr,"K1_JKA_SABER_MARK_WEAPON_%s swords=%d sabers=%d unpowered=1 native_item_restored=1\n",ok?"PASS":"FAIL",swords,sabers);
                    const auto blades=PoweredSaberBlades(actor);bool lanesOk=!blades.empty();int offhand=0;
                    for(int lane=0;lane<int(blades.size());++lane) {
                        Trail laneTrail;const size_t first=marks.size();
                        contact(actor,laneTrail,1,&a,&b,lane);
                        // The first exact surface hit must stamp immediately,
                        // even if that blade misses on the very next frame.
                        lanesOk=lanesOk && marks.size()>first && laneTrail.valid;
                        offhand+=blades[lane].hand==1;
                    }
                    fprintf(stderr,"K1_JKA_SABER_MARK_LANES_%s blades=%zu offhand=%d first_contact=1\n",lanesOk?"PASS":"FAIL",blades.size(),offhand);
                }
                auto* nativeScene=static_cast<Scene*>(scene);
                for(int i=0;i<nativeScene->m_rooms.m_count && !wall;++i) {
                    auto* room=nativeScene->m_rooms.m_pData[i];if(!room->m_pNodeData) continue;
                    std::vector<K1SaberContact::Triangle> triangles;
                    for(int m=0;m<room->m_pNodeData->m_meshes.m_count;++m)
                        CollisionPartTriangles(room->m_pNodeData->m_meshes.m_pData[m],triangles,true);
                    wall=probe(triangles,nullptr,true);
                }
                for(int i=0;i<nativeScene->Objects().m_count;++i) {
                    auto* gob=nativeScene->Objects().m_pData[i];
                    if(!gob->m_pModel || !(gob->m_pModel->m_nFlags&(Model::kModelPlaceable|Model::kModelDoor))) continue;
                    ++props;if(object) continue;
                    std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(gob,triangles);
                    object=probe(triangles,gob,false);
                }
                // This arena has no native placeables. A transient, client-only
                // fixture uses a model read from the installed placeables table;
                // no server/story object or saved game is created.
                if(!props) {
                    auto* table=g_pRules->m_p2DArrays->m_pPlaceables;
                    int hitCheck=0;table->GetINTEntry(0,"HitCheck",&hitCheck);
                    for(int row=0;row<table->m_nRows && !object;++row) {
                        CExoString model;
                        if(!table->GetINTEntry(row,"HitCheck",&hitCheck) || !hitCheck ||
                            !table->GetCExoStringEntry(row,"ModelName",&model) || model.IsEmpty()) continue;
                        std::unique_ptr<CAurObject> fixture(NewCAurObject(model.CStr(),const_cast<char*>("k1marktest"),nullptr,nullptr));
                        auto* gob=dynamic_cast<Gob*>(fixture.get());
                        if(!gob || !gob->m_pModel) continue;
                        const auto flags=gob->m_pModel->m_nFlags;
                        gob->m_pModel->m_nFlags|=Model::kModelPlaceable|Model::kModelItem;
                        gob->SetPosition(fixtureOrigin+Vector(0,4,.15f));
                        gob->SetIgnoreHitCheck(false);gob->ProcessGob();gob->SetScene(nativeScene);
                        std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(gob,triangles);
                        const size_t first=marks.size();object=probe(triangles,gob,false);++props;
                        bool attached=object;
                        gob->SetPosition(gob->m_vPosition+Vector(.3f,.2f,.1f));
                        gob->SetOrientation(Quaternion(Vector(0,0,1),.4f));
                        for(size_t i=first;i<marks.size();++i) for(const auto& v:marks[i].polygon) {
                            const Vector world=gob->m_vPosition+Rotate(gob->m_qOrientation,v.point*gob->m_fObjectScale);
                            const Vector local=Rotate(Inverse(normalize(gob->m_qOrientation)),world-gob->m_vPosition)*(1.f/gob->m_fObjectScale);
                            attached &= K1SaberContact::Finite(world) && magnitude(local-v.point)<.0001f;
                        }
                        gob->SetScene(nullptr);gob->m_pModel->m_nFlags=flags;fixture.reset();
                        bool cleared=true;for(size_t i=first;i<marks.size();++i) cleared &= !marks[i].owner.m_pObject;
                        object=object && attached && cleared;
                        fprintf(stderr,"K1_JKA_SABER_MARK_PROP_%s model=%s triangles=%zu attached=%d owner_cleared=%d\n",
                            object?"PASS":"FAIL",model.CStr(),triangles.size(),attached,cleared);
                        break;
                    }
                }
                fprintf(stderr,"K1_JKA_SABER_MARK_SURFACES_%s wall=%d prop=%d prop_candidates=%d\n",
                    wall && object?"PASS":"FAIL",wall,object,props);
                surfacesProbed=true;
            }
            if(std::getenv("K1_ENGAGE_SURFACE_MARK_TEST") && fixtureDrawn && K1EngageActive() && !testedContactSound) {
                // Exercise the original payload against a real installed floor hit,
                // sharing the actor's real cooldown and world clock. This is
                // explicitly a synthetic ray, not animated-blade coverage.
                Vector a=fixtureOrigin+Vector(0,0,.3f),b=a-Vector(0,0,.6f);
                auto& trail=trails[actor->m_objectId*4];
                contact(actor,trail,2,&a,&b);trail.valid=false;
            }
        }
        for(auto& entry:npcPlayback) if(entry.second.requested && entry.second.owner)
            liveContact(entry.second.owner);
    }
    marks.erase(std::remove_if(marks.begin(),marks.end(),[&](const Mark& mark) {
        if(std::getenv("K1_ENGAGE_SURFACE_MARK_TEST")) {
            const auto age=now-mark.born;
            if(!glowExpired && age>=1500000 && age<=10000000) {
                fprintf(stderr,"K1_JKA_SABER_MARK_GLOW_EXPIRED_PASS age_us=%lld glow=%g scorch_retained=1\n",age,K1JkaSaberMarks::Glow(age));glowExpired=true;
            }
            if(!scorchExpired && age>10000000) {
                fprintf(stderr,"K1_JKA_SABER_MARK_SCORCH_EXPIRED_PASS age_us=%lld\n",age);scorchExpired=true;
            }
        }
        return (!mark.world && !mark.owner.m_pObject) || now-mark.born>10000000;
    }),marks.end());
    if(std::getenv("K1_ENGAGE_SURFACE_MARK_ONLY") && fixtureDrawn && surfacesProbed &&
       testedContactSound && glowExpired && scorchExpired) {
        if(FILE* f=fopen("saber-mark-input.txt","w")){fputs("quit:0\n",f);fclose(f);}
        fprintf(stderr,"K1_JKA_SABER_MARK_ONLY_PASS\n");
    }
    // JKA's mark pool is bounded; evict old paired scorch/glow fragments together.
    if(marks.size()>256) marks.erase(marks.begin(),marks.end()-256);
    if(marks.empty()) return;
    const bool capture=std::getenv("K1_ENGAGE_SURFACE_MARK_TEST") && !fixtureDrawn && fixtureFrames>=5;
    int viewport[4]={};std::vector<unsigned char> before;
    float minX=1e9f,minY=1e9f,maxX=-1e9f,maxY=-1e9f;
    auto worldPoint=[](const Mark& mark,const K1JkaSaberMarks::Vertex& v) {
        auto* gob=static_cast<Gob*>(mark.owner.m_pObject);
        return mark.world?v.point:gob->m_vPosition+Rotate(gob->m_qOrientation,v.point*gob->m_fObjectScale);
    };
    auto read=[&](std::vector<unsigned char>& pixels) {
        pixels.resize(size_t(viewport[2])*viewport[3]*3);int alignment=0;
        ASLgl::glGetIntegerv(0x0D05,&alignment);ASLgl::glPixelStorei(0x0D05,1);
        ASLgl::glReadPixels(viewport[0],viewport[1],viewport[2],viewport[3],GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
        ASLgl::glPixelStorei(0x0D05,alignment);
    };
    if(capture) {
        ASLgl::glGetIntegerv(GL_VIEWPORT,viewport);read(before);
        for(const auto& mark:marks) for(const auto& v:mark.polygon) {
            const Vector p=module->m_pCamera->MapToScreen(worldPoint(mark,v),true);
            minX=std::min(minX,p.x*viewport[2]-3);maxX=std::max(maxX,p.x*viewport[2]+3);
            minY=std::min(minY,(1-p.y)*viewport[3]-3);maxY=std::max(maxY,(1-p.y)*viewport[3]+3);
        }
    }
    PreviewTextureState saved;
    // K1's Push/PopAttrib imports are no-ops. Preserve every state changed by
    // this pass explicitly, including the host shader's shadowed alpha test.
    const unsigned int caps[]={GL_LIGHTING,GL_CULL_FACE,GL_DEPTH_TEST,0x8037};
    bool enabled[4];for(int i=0;i<4;++i) enabled[i]=ASLgl::glIsEnabled(caps[i])!=0;
    float offsetFactor=0,offsetUnits=0;
    ASLgl::glGetFloatv(0x8038,&offsetFactor);ASLgl::glGetFloatv(0x2A00,&offsetUnits);
    const float alphaReference=k1host_get_alpha_test_reference();
    ASLgl::glActiveTexture(GL_TEXTURE0);
    const int textureEnvironment=k1host_get_texture_environment_mode();
    if(!textureEnvironment) {fprintf(stderr,"K1_JKA_SABER_MARK_TEXTURE_ENV_FAIL\n");return;}
    for(int unit=0;unit<4;++unit) {
        ASLgl::glActiveTexture(GL_TEXTURE0+unit);ASLgl::glDisable(GL_TEXTURE_2D);ASLgl::glDisable(GL_TEXTURE_CUBE_MAP);
    }
    ASLgl::glActiveTexture(GL_TEXTURE0);
    GLRender::pfuncEnablePixelShaders(GLRender::kPSFixedFunction,0,0,0,0);
    ASLgl::glUseProgram(0);
    ASLgl::glDisable(GL_LIGHTING);ASLgl::glDisable(GL_CULL_FACE);ASLgl::glDisable(0x0BC0); // GL_ALPHA_TEST
    ASLgl::glEnable(GL_DEPTH_TEST);ASLgl::glDepthMask(false);ASLgl::glEnable(GL_BLEND);
    ASLgl::glEnable(0x8037);ASLgl::glPolygonOffset(-1.f,-2.f); // GL_POLYGON_OFFSET_FILL
    ASLgl::glEnable(GL_TEXTURE_2D);ASLgl::glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    for(int stage=0;stage<2;++stage) {
        if(!textures[stage]) {
            int width=0,height=0,bpp=0;
            auto* image=ImageReadTGA(const_cast<char*>(stage?"jka-marks/glow.tga":"jka-marks/rivet.tga"),&width,&height,&bpp);
            if(!image || width<=0 || height<=0 || (bpp!=3 && bpp!=4)) {
                delete[] image;fprintf(stderr,"K1_JKA_SABER_MARK_TEXTURE_FAIL stage=%d\n",stage);continue;
            }
            ASLgl::glGenTextures(1,&textures[stage]);ASLgl::glBindTexture(GL_TEXTURE_2D,textures[stage]);
            ASLgl::glTexImage2D(GL_TEXTURE_2D,0,bpp==4?GL_RGBA:GL_RGB,width,height,0,bpp==4?GL_RGBA:GL_RGB,GL_UNSIGNED_BYTE,image);
            ASLgl::glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            ASLgl::glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            ASLgl::glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,stage?GL_REPEAT:0x812f);
            ASLgl::glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,stage?GL_REPEAT:0x812f);
            delete[] image;
            fprintf(stderr,"K1_JKA_SABER_MARK_TEXTURE_PASS stage=%d width=%d height=%d bpp=%d\n",stage,width,height,bpp);
        }
        ASLgl::glBindTexture(GL_TEXTURE_2D,textures[stage]);
        ASLgl::glBlendFunc(stage?1:GL_DST_COLOR,stage?1:GL_SRC_COLOR); // GL_ONE
        for(const auto& mark:marks) {
            const float glow=K1JkaSaberMarks::Glow(now-mark.born);
            if(stage && glow==0) continue;
            auto* gob=dynamic_cast<Gob*>(mark.owner.m_pObject);if(!mark.world && !gob) continue;
            ASLgl::glColor4f(stage?mark.color[0]*glow:1,stage?mark.color[1]*glow:1,stage?mark.color[2]*glow:1,1);
            GLRender::BeginImmediatePrimitive(Aurora::kPrimTriangles);
            auto vertex=[&](const K1JkaSaberMarks::Vertex& v) {
                Vector p=mark.world?v.point:gob->m_vPosition+Rotate(gob->m_qOrientation,v.point*gob->m_fObjectScale);
                ASLgl::glTexCoord2f(v.s,v.t);ASLgl::glVertex4f(p.x,p.y,p.z,1);
            };
            for(size_t i=1;i+1<mark.polygon.size();++i) {vertex(mark.polygon[0]);vertex(mark.polygon[i]);vertex(mark.polygon[i+1]);}
            ASLgl::glEnd();
        }
    }
    ASLgl::glActiveTexture(GL_TEXTURE0);
    if(textureEnvironment) ASLgl::glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,textureEnvironment);
    ASLgl::glPolygonOffset(offsetFactor,offsetUnits);
    for(int i=0;i<4;++i) {if(enabled[i]) ASLgl::glEnable(caps[i]);else ASLgl::glDisable(caps[i]);}
    k1host_restore_alpha_test_reference(alphaReference);
    saved.Restore();
    if(std::getenv("K1_ENGAGE_SURFACE_MARK_TEST") && !fixtureDrawn && fixtureFrames>=5) {
        int program=0;ASLgl::glGetIntegerv(0x8B8D,&program);
        bool restored=program==saved.programId && k1host_save_preview_program()==saved.program &&
            GLRender::cm_uiCurrentStage==saved.cachedStage;
        for(int i=0;i<6;++i) restored &= GLRender::cm_nGLCurrentTID[i]==saved.cachedTexture[i];
        for(int i=0;i<4;++i) restored &= (ASLgl::glIsEnabled(caps[i])!=0)==enabled[i];
        float restoredFactor=0,restoredUnits=0;
        ASLgl::glGetFloatv(0x8038,&restoredFactor);ASLgl::glGetFloatv(0x2A00,&restoredUnits);
        restored &= restoredFactor==offsetFactor && restoredUnits==offsetUnits && k1host_get_alpha_test_reference()==alphaReference;
        ASLgl::glActiveTexture(GL_TEXTURE0);restored &= k1host_get_texture_environment_mode()==textureEnvironment;
        for(int unit=0;unit<4;++unit) {
            ASLgl::glActiveTexture(GL_TEXTURE0+unit);
            for(int target=0;target<2;++target) {
                const auto cap=target?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;int texture=0;
                ASLgl::glGetIntegerv(target?0x8514:GL_TEXTURE_BINDING_2D,&texture);
                restored &= texture==saved.texture[unit][target] && (ASLgl::glIsEnabled(cap)!=0)==saved.enabled[unit][target];
            }
        }
        ASLgl::glActiveTexture(saved.active);
        std::vector<unsigned char> after;read(after);size_t inside=0,outside=0;
        for(int y=0;y<viewport[3];++y) for(int x=0;x<viewport[2];++x) {
            const size_t i=(size_t(y)*viewport[2]+x)*3;
            if(before[i]==after[i] && before[i+1]==after[i+1] && before[i+2]==after[i+2]) continue;
            if(x>=minX && x<=maxX && y>=minY && y<=maxY) ++inside;else ++outside;
        }
        const bool ok=restored && textures[0] && textures[1] && !marks.empty() && inside>0 && outside==0;
        fprintf(stderr,"K1_JKA_SABER_MARK_INSTALLED_%s fixture=floor fragments=%zu state_restored=%d inside=%zu outside=%zu\n",ok?"PASS":"FAIL",marks.size(),restored,inside,outside);
        fixtureDrawn=true;
    }
}
}
void K1EngageRenderSaberMarks(CAurScene* scene) { DrawSaberSurfaceMarks(scene); }

void K1ParryReticle::Draw(float delta) {
    if(!K1EngageActive()) return;
    int width=0,height=0;AurGUIGetScreenSize(width,height);
    if(width<=0 || height<=0) return;
    const auto aim=SelectedParryAim();
    target=aim.target;sector=aim.sector;guarding=state.Guarding();
    const bool parrySuccess=ContactWallUs()<parrySuccessUntil;
    // Flush native image batches before the untextured control, then preserve
    // the same texture/program state already scoped by the saber HUD preview.
    CAurGUIImage::FlushBuffer(CAurGUIImage::cm_fAlpha);
    PreviewTextureState saved;
    const bool lighting=ASLgl::glIsEnabled(GL_LIGHTING)!=0,depth=ASLgl::glIsEnabled(GL_DEPTH_TEST)!=0;
    const bool capture=parryRenderRequested && std::getenv("K1_ENGAGE_PARRY_TEST");
    int frame[4]={};std::vector<unsigned char> before;
    auto read=[&](std::vector<unsigned char>& data) {
        data.resize(size_t(frame[2])*frame[3]*3);
        int pack=0;ASLgl::glGetIntegerv(0x0D05,&pack);ASLgl::glPixelStorei(0x0D05,1);
        ASLgl::glReadPixels(frame[0],frame[1],frame[2],frame[3],GL_RGB,GL_UNSIGNED_BYTE,data.data());
        ASLgl::glPixelStorei(0x0D05,pack);
    };
    if(capture) {
        ASLgl::glGetIntegerv(GL_VIEWPORT,frame);read(before);
        int face=0;ASLgl::glGetIntegerv(0x0B46,&face); // GL_FRONT_FACE
        fprintf(stderr,"K1_PARRY_DRAW_STATE program=%d cull=%d front=%x depth=%d viewport=%d,%d,%d,%d\n",
            saved.programId,ASLgl::glIsEnabled(GL_CULL_FACE)!=0,face,depth,frame[0],frame[1],frame[2],frame[3]);
        k1host_trace_poise_shader("parry_before");
    }
    if(!AurGUISetupViewport(0,0,width,height,CSWGuiDrawable::NO_COLOURING,false,1.f)) return;
    for(int unit=0;unit<4;++unit) {
        ASLgl::glActiveTexture(GL_TEXTURE0+unit);
        ASLgl::glDisable(GL_TEXTURE_2D);ASLgl::glDisable(GL_TEXTURE_CUBE_MAP);
    }
    ASLgl::glActiveTexture(GL_TEXTURE0);
    ASLgl::glDisable(GL_LIGHTING);ASLgl::glDisable(GL_DEPTH_TEST);ASLgl::glDepthMask(false);
    ASLgl::glEnable(GL_BLEND);ASLgl::glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    const float scale=height/720.f,cx=width*.5f,cy=height*.5f;
    auto vertex=[&](float x,float y){ASLgl::glVertex4f(x/width,1.f-y/height,0,1);};
    // Screen Y is inverted above; keep the native GUI's counter-clockwise faces.
    auto triangle=[&](float ax,float ay,float bx,float by,float dx,float dy){vertex(ax,ay);vertex(dx,dy);vertex(bx,by);};
    GLRender::BeginImmediatePrimitive(Aurora::kPrimTriangles);
    auto disk=[&](float radius,float r,float g,float b) {
        ASLgl::glColor4f(r,g,b,1);
        for(int i=0;i<16;++i) {
            const float a=i*6.283185307f/16,b=(i+1)*6.283185307f/16;
            triangle(cx,cy,cx+radius*std::cos(a),cy+radius*std::sin(a),cx+radius*std::cos(b),cy+radius*std::sin(b));
        }
    };
    disk(3.f*scale,0,0,0);disk(1.8f*scale,1,1,1);
    if(target!=OBJECT_INVALID || parrySuccess) for(int side=0;side<4;++side) {
        const bool selected=side==sector;
        if(parrySuccess || (selected && guarding)) {
            const auto& yellow=CSWGuiManager::COLOR_YELLOW;
            ASLgl::glColor4f(yellow.x,yellow.y,yellow.z,1.f);
        }
        else if(selected) ASLgl::glColor4f(.25f,.7f,1.f,.95f);
        else ASLgl::glColor4f(.2f,.55f,.8f,.5f);
        const float start=side*1.5707963268f+.09f,end=start+1.3907963268f;
        for(int i=0;i<10;++i) {
            const float a=start+(end-start)*i/10,b=start+(end-start)*(i+1)/10;
            const float r0=17.f*scale,r1=20.f*scale;
            const float ax=cx+r0*std::sin(a),ay=cy-r0*std::cos(a);
            const float bx=cx+r1*std::sin(a),by=cy-r1*std::cos(a);
            const float dx=cx+r1*std::sin(b),dy=cy-r1*std::cos(b);
            const float ex=cx+r0*std::sin(b),ey=cy-r0*std::cos(b);
            triangle(ax,ay,bx,by,dx,dy);triangle(ax,ay,dx,dy,ex,ey);
        }
    }
    ASLgl::glEnd();
    AurGUICloseViewport();
    if(lighting) ASLgl::glEnable(GL_LIGHTING);else ASLgl::glDisable(GL_LIGHTING);
    if(depth) ASLgl::glEnable(GL_DEPTH_TEST);else ASLgl::glDisable(GL_DEPTH_TEST);
    saved.Restore();
    if(capture) {
        std::vector<unsigned char> after;read(after);
        int outside=0,changed=0;
        const float pixelScale=float(frame[3])/height;
        for(int y=0;y<frame[3];++y) for(int x=0;x<frame[2];++x) {
            const size_t at=(size_t(y)*frame[2]+x)*3;
            if(before[at]==after[at] && before[at+1]==after[at+1] && before[at+2]==after[at+2]) continue;
            ++changed;
            if(std::hypot(x-frame[2]*.5f,y-frame[3]*.5f)>22.f*scale*pixelScale) ++outside;
        }
        auto pixel=[&](float x,float y,int channel) {
            const int px=std::max(0,std::min(frame[2]-1,int(x*frame[2]/width)));
            const int py=std::max(0,std::min(frame[3]-1,frame[3]-1-int(y*frame[3]/height)));
            return int(after[(size_t(py)*frame[2]+px)*3+channel]);
        };
        const bool dot=pixel(cx,cy,0)>220 && pixel(cx,cy,1)>220 && pixel(cx,cy,2)>220;
        bool highlight=true;
        if(parrySuccess)for(int side=0;side<4;++side) {
            const float angle=(side+.5f)*1.5707963268f;
            const float x=cx+18.5f*scale*std::sin(angle),y=cy-18.5f*scale*std::cos(angle);
            highlight=highlight && pixel(x,y,0)>220 && pixel(x,y,1)>220 && pixel(x,y,2)<40;
        }
        if(target!=OBJECT_INVALID && sector>=0) {
            const float angle=(sector+.5f)*1.5707963268f;
            const float x=cx+18.5f*scale*std::sin(angle),y=cy-18.5f*scale*std::cos(angle);
            const int r=pixel(x,y,0),g=pixel(x,y,1),b=pixel(x,y,2);
            highlight=highlight && ((parrySuccess || guarding)?(r>220 && g>220 && b<40):(b>150 && b>r*1.5f));
        }
        int program=0;ASLgl::glGetIntegerv(0x8B8D,&program);
        bool cache=GLRender::cm_uiCurrentStage==saved.cachedStage && k1host_save_preview_program()==saved.program && program==saved.programId;
        for(int unit=0;unit<6;++unit) cache=cache && GLRender::cm_nGLCurrentTID[unit]==saved.cachedTexture[unit];
        const bool ok=outside==0 && changed>0 && dot && highlight && cache;
        fprintf(stderr,"K1_PARRY_RENDER_%s sector=%d guard=%d success=%d screen=%d,%d dot=%d highlight=%d outside=%d changed=%d cache=%d\n",
            ok?"PASS":"FAIL",sector,guarding,parrySuccess,width,height,dot,highlight,outside,changed,cache);
        parryRenderFailed=parryRenderFailed || !ok;++parryRenderChecks;parryRenderRequested=false;
    }
    if(std::getenv("K1_ENGAGE_PARRY_TEST")) {
        static int previous=-9;static unsigned long long previousTarget=0;
        if(previous!=sector || previousTarget!=target) {
            fprintf(stderr,"K1_PARRY_RETICLE target=%llu sector=%d guard=%d screen=%d,%d aim=%g,%g\n",target,sector,guarding,width,height,aim.right,aim.up);
            previous=sector;previousTarget=target;
        }
    }
}
int K1EngagePoiseBladePercent(Gob* model) {
    if(!model || !hudOwner) return -1;
    if(model->m_pScene==hudOwner->m_poiseWeapon.m_scene.m_pScene) return hudOwner->m_poiseWeapon.percent;
    if(model->m_pScene==hudOwner->m_poiseOffhand.m_scene.m_pScene) return hudOwner->m_poiseOffhand.percent;
    return -1;
}
bool K1EngagePoiseSolidRange(Part* part,float& leftX,float& rightX) {
    if(!part || !hudOwner || K1EngagePoiseBladePercent(part->m_pGob)<0)return false;
    Gob* model=part->m_pGob;const auto* geometry=SwordModelGeometry(model);
    if(!geometry)return false;
    auto& view=model->m_pScene==hudOwner->m_poiseWeapon.m_scene.m_pScene?hudOwner->m_poiseWeapon:hudOwner->m_poiseOffhand;
    auto* camera=view.m_scene.m_pCamera;if(!camera)return false;
    const float fraction=std::max(0,std::min(100,view.percent))*.01f;
    // Project the complete grip box, not just its centerline: perspective must
    // never shave off the crossguard or pommel at zero poise.
    float gripLeft=1e30f,gripRight=-1e30f;
    for(float x:{geometry->gripXLow,geometry->gripXHigh})for(float y:{geometry->gripYLow,geometry->gripYHigh})for(float z:{geometry->gripLow,geometry->gripHigh}) {
        const Vector point=model->m_vPosition+(model->m_qOrientation.xdir()*x+model->m_qOrientation.ydir()*y+model->m_qOrientation.zdir()*z)*model->m_fObjectScale;
        const float screen=camera->MapToScreen(point,false).x;
        gripLeft=std::min(gripLeft,screen);gripRight=std::max(gripRight,screen);
    }
    const float a=camera->MapToScreen(SwordWorldPoint(model,geometry->tipLow),false).x;
    const float b=camera->MapToScreen(SwordWorldPoint(model,geometry->tipHigh),false).x;
    const float fullLeft=std::min(gripLeft,std::min(a,b)),fullRight=std::max(gripRight,std::max(a,b));
    const bool positiveRight=b>a;
    leftX=geometry->doubleBlade || !positiveRight?gripLeft+(fullLeft-gripLeft)*fraction:fullLeft;
    rightX=geometry->doubleBlade || positiveRight?gripRight+(fullRight-gripRight)*fraction:fullRight;
    return std::isfinite(leftX) && std::isfinite(rightX) && rightX>leftX;
}
bool DoubleBladePoiseWeapon() {
    auto* actor=ServerActor();
    auto* item=actor && actor->m_pInventory?actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON):nullptr;
    return item && item->m_baseItem==BASE_ITEM_DOUBLE_BLADED_LIGHTSABER;
}
bool K1EngagePoiseHiltScale(Part* part,Vector& scale) {
    if(!part || K1EngagePoiseBladePercent(part->m_pGob)<0 || !DoubleBladePoiseWeapon()) return false;
    auto* mesh=part->m_pNode->AsMdlNodeTriMesh();
    if(!mesh || part->m_pNode->AsMdlNodeLightsaber()) return false;
    bool bladeParent=false;
    for(int i=0;i<part->m_nChildren;++i) bladeParent|=part->m_paChildren[i]->m_pNode->AsMdlNodeLightsaber()!=nullptr;
    if(!bladeParent) return false;
    const Vector size=mesh->m_vMax-mesh->m_vMin;
    scale=Vector(.85f,.85f,.85f);
    if(size.z>=size.x && size.z>=size.y) scale.z=1.14f;
    else if(size.y>=size.x) scale.y=1.14f;
    else scale.x=1.14f;
    return true;
}
Part* PoisePreviewBlade(Part* part) {
    if(!part || K1EngagePoiseBladePercent(part->m_pGob)<0 || !DoubleBladePoiseWeapon()) return nullptr;
    auto* node=part->m_pNode->AsMdlNodeLightsaber();
    // Native staff blades also have ordinary textured planes alongside their
    // lightsaber nodes. Match those by material and nearest blade midpoint;
    // the hilt uses a different material and never enters this crop.
    if(!node) {
        auto* mesh=part->m_pNode->AsMdlNodeTriMesh();
        if(!mesh || !part->m_pParent) return nullptr;
        const Vector center=PartPointWorld(part,(mesh->m_vMin+mesh->m_vMax)*.5f);
        Part* nearest=nullptr;float distance=1e30f;
        for(int i=0;i<part->m_pParent->m_nChildren;++i) {
            auto* sibling=part->m_pParent->m_paChildren[i];
            auto* blade=sibling->m_pNode->AsMdlNodeLightsaber();
            if(!blade || !blade->m_paPositions || std::strcmp(mesh->m_szTexture,blade->m_szTexture)!=0) continue;
            const auto a=blade->m_paPositions[1],b=blade->m_paPositions[2];
            const float d=magnitude(PartPointWorld(sibling,Vector(a.x+b.x,a.y+b.y,a.z+b.z)*.5f)-center);
            if(d<distance){distance=d;nearest=sibling;}
        }
        if(!nearest) return nullptr;
        part=nearest;node=part->m_pNode->AsMdlNodeLightsaber();
    }
    return node->m_paPositions?part:nullptr;
}
bool PoiseBladeWorldOffset(Part* blade,Vector& offset) {
    if(!blade || !blade->m_pParent) return false;
    Part* hilt=blade->m_pParent;Vector scale;
    if(!K1EngagePoiseHiltScale(hilt,scale)) return false;
    auto* mesh=hilt->m_pNode->AsMdlNodeTriMesh();
    const Vector center=(mesh->m_vMin+mesh->m_vMax)*.5f;
    const auto a=blade->m_pNode->AsMdlNodeLightsaber()->m_paPositions[1];
    const Vector anchor=blade->m_vLocalPosition+Rotate(blade->m_qLocalOrientation,Vector(a.x,a.y,a.z)*blade->m_scale.m_fScale);
    const Vector d=anchor-center;
    const Vector adjusted=center+Vector(d.x*scale.x,d.y*scale.y,d.z*scale.z);
    offset=PartPointWorld(hilt,adjusted)-PartPointWorld(hilt,anchor);
    return true;
}
bool K1EngagePoiseBladeOffset(Part* part,Vector& offset) {
    Vector world;
    if(!PoiseBladeWorldOffset(PoisePreviewBlade(part),world)) return false;
    Vector position;Quaternion orientation;part->GetWorld(position,orientation);
    float scale=part->m_pGob->m_fObjectScale;
    for(Part* p=part;p;p=p->m_pParent) scale*=p->m_scale.m_fScale;
    if(std::fabs(scale)<1e-6f) return false;
    offset=Rotate(Inverse(orientation),world)/scale;
    return true;
}
bool K1EngagePoiseBladeRange(Part* part,float& baseX,float& tipX) {
    part=PoisePreviewBlade(part);
    if(!part) return false;
    auto* camera=hudOwner->m_poiseWeapon.m_scene.m_pCamera;
    if(!camera) return false;
    auto* node=part->m_pNode->AsMdlNodeLightsaber();
    Vector offset(0,0,0);PoiseBladeWorldOffset(part,offset);
    // The two native blades point in opposite directions. Project their actual
    // powered emitter/tip through the preview camera, not a shared local bound.
    const auto a=node->m_paPositions[1],b=node->m_paPositions[2];
    baseX=camera->MapToScreen(PartPointWorld(part,Vector(a.x,a.y,a.z))+offset,false).x;
    tipX=camera->MapToScreen(PartPointWorld(part,Vector(b.x,b.y,b.z))+offset,false).x;
    return std::isfinite(baseX) && std::isfinite(tipX) && std::fabs(tipX-baseX)>1.f;
}
void K1PoiseWeaponView::Draw(float delta) {
    // Diagnostic-only alternating preview epochs; actual poise stays untouched.
    static unsigned int renderFrame=0;
    const bool renderProbe=std::getenv("K1_ENGAGE_POISE_RENDER_TEST")!=nullptr;
    const bool meterProbe=std::getenv("K1_ENGAGE_POISE_METER_TEST")!=nullptr;
    if(renderProbe) {
        ++renderFrame;
        if(meterProbe) poiseMeterEpoch=renderFrame/120;
        if(renderFrame%120==118) {
            if(FILE* f=fopen("poise-input.txt","w")) {
                fprintf(f,"shot:poise-%s-%u\n",meterProbe?"meter":"render",renderFrame);fclose(f);
            }
            fprintf(stderr,"K1_POISE_RENDER_EPOCH frame=%u visible=%d\n",renderFrame,meterProbe || (renderFrame/120)%2==0);
        }
        const unsigned int endFrame=meterProbe?1200:720;
        if(renderFrame>=endFrame) {
            if(renderFrame==endFrame) fprintf(stderr,"K1_POISE_RENDER_CAPTURE_PASS epochs=%d\n",meterProbe?10:6);
            if(FILE* f=fopen("poise-input.txt","w")){fputs("quit:0\n",f);fclose(f);}
        }
        if(!meterProbe && (renderFrame/120)%2) return;
    }
    // Timing probes use clock/contact instrumentation; synchronous multi-megabyte
    // GPU readbacks delay the next observer frame. Render checks run separately.
    const bool dualTrace=DualMode() && std::getenv("K1_ENGAGE_DUAL_TEST") && (draws<1 || lastDrawnPercent!=percent);
    const bool trace=dualTrace || (std::getenv("K1_ENGAGE_POISE_TEST") && !std::getenv("K1_ENGAGE_POISE_REGEN_TEST") &&
        (draws<2 || lastDrawnPercent!=percent || (renderProbe && renderFrame%120==118)));
    auto renderTrace=[&](const char* stage) {
        k1host_trace_poise_shader(stage);
        int depthMask=0,depthFunc=0,blendSrc=0,blendDst=0,clientUnit=0,matrixMode=0;
        ASLgl::glGetIntegerv(0x0B72,&depthMask);ASLgl::glGetIntegerv(0x0B74,&depthFunc);
        ASLgl::glGetIntegerv(GL_BLEND_SRC,&blendSrc);ASLgl::glGetIntegerv(GL_BLEND_DST,&blendDst);
        ASLgl::glGetIntegerv(0x84E1,&clientUnit);ASLgl::glGetIntegerv(0x0BA0,&matrixMode);
        fprintf(stderr,"K1_POISE_FIXED stage=%s depth=%d:%d:%x blend=%d:%x:%x lighting=%d alpha=%d cull=%d client_unit=%x matrix=%x\n",stage,
            ASLgl::glIsEnabled(GL_DEPTH_TEST),depthMask,depthFunc,ASLgl::glIsEnabled(GL_BLEND),blendSrc,blendDst,
            ASLgl::glIsEnabled(GL_LIGHTING),ASLgl::glIsEnabled(0x0BC0),ASLgl::glIsEnabled(GL_CULL_FACE),clientUnit,matrixMode);
        int active=0,fbo=0,viewport[4],textures[4],enabled[4];float color[4],ambient[4];
        ASLgl::glGetIntegerv(0x84E0,&active);ASLgl::glGetIntegerv(0x8CA6,&fbo);
        ASLgl::glGetIntegerv(0x0BA2,viewport);ASLgl::glGetFloatv(0x0B00,color);ASLgl::glGetFloatv(0x0B53,ambient);
        for(int i=0;i<4;++i){ASLgl::glActiveTexture(0x84C0+i);ASLgl::glGetIntegerv(0x8069,&textures[i]);enabled[i]=ASLgl::glIsEnabled(0x0DE1);}
        ASLgl::glActiveTexture(active);
        std::vector<unsigned char> pixels(64*64*3);ASLgl::glReadPixels(320,360,64,64,0x1907,0x1401,pixels.data());
        long chroma=0,light=0;for(size_t i=0;i<pixels.size();i+=3){chroma+=std::abs(int(pixels[i])-pixels[i+1])+std::abs(int(pixels[i+1])-pixels[i+2]);light+=pixels[i]+pixels[i+1]+pixels[i+2];}
        fprintf(stderr,"K1_POISE_RENDER_TRACE stage=%s percent=%d fbo=%d viewport=%d,%d,%d,%d active=%x units=%d:%d,%d:%d,%d:%d,%d:%d color=%g,%g,%g ambient=%g,%g,%g saturation=%d:%g chroma=%ld light=%ld vp=%d fp=%d cache=%u:%d,%d,%d,%d\n",
            stage,percent,fbo,viewport[0],viewport[1],viewport[2],viewport[3],active,textures[0],enabled[0],textures[1],enabled[1],textures[2],enabled[2],textures[3],enabled[3],
            color[0],color[1],color[2],ambient[0],ambient[1],ambient[2],GLRender::m_bSaturationEnabled,GLRender::m_fSaturation,chroma,light,ASLgl::glIsEnabled(0x8620),ASLgl::glIsEnabled(0x8804),
            GLRender::cm_uiCurrentStage,GLRender::cm_nGLCurrentTID[0],GLRender::cm_nGLCurrentTID[1],GLRender::cm_nGLCurrentTID[2],GLRender::cm_nGLCurrentTID[3]);
    };
    if(trace) renderTrace("before");
    if(trace && hudOwner) {
        const bool bannerHidden=!hudOwner->m_lblCombatModeMsg.m_tail.bVisible &&
            !hudOwner->m_lblCombatModeMsg2.m_tail.bVisible && !hudOwner->m_lblCombatMsgBg.m_tail.bVisible;
        fprintf(stderr,"K1_POISE_BANNER_MASK_%s percent=%d\n",bannerHidden?"PASS":"FAIL",percent);
    }
    if(!m_scene.m_pScene || !m_scene.m_pCamera) return;
    // Native Scene::Render's attribute push/pop does not restore the other
    // fixed-function texture units on this host. Saber materials leave unit2
    // enabled, contaminating the next world's texture combination.
    PreviewTextureState previewTextureState;
    auto readFrame=[](const int* box,unsigned char* pixels) {
        // Diagnostic RGB rows may be unaligned at resolutions such as1366 wide.
        int alignment=0;ASLgl::glGetIntegerv(0x0D05,&alignment);
        ASLgl::glPixelStorei(0x0D05,1);
        ASLgl::glReadPixels(box[0],box[1],box[2],box[3],GL_RGB,GL_UNSIGNED_BYTE,pixels);
        ASLgl::glPixelStorei(0x0D05,alignment);
    };
    int frameViewport[4]={};std::vector<unsigned char> frameBefore;
    if(trace) {
        ASLgl::glGetIntegerv(GL_VIEWPORT,frameViewport);
        frameBefore.resize(size_t(frameViewport[2])*frameViewport[3]*3);
        readFrame(frameViewport,frameBefore.data());
    }
    m_scene.m_pScene->Animate(delta);
    if(!AurGUISetupViewport(m_extent.x,m_extent.y,m_extent.w,m_extent.h,m_scene.m_color,true,1.f)) return;
    int viewport[4],oldBox[4];
    ASLgl::glGetIntegerv(0x0BA2,viewport); // GL_VIEWPORT, actual scaled UI rectangle
    ASLgl::glGetIntegerv(0x0C10,oldBox); // GL_SCISSOR_BOX
    const bool clipped=ASLgl::glIsEnabled(0x0C11)!=0;
    ASLgl::glEnable(0x0C11);
    // Preview blade draws scope depletion; solid swords retain their grip span.
    ASLgl::glScissor(viewport[0],viewport[1],viewport[2],viewport[3]);
    m_scene.m_pCamera->RenderScene();
    ++draws;
    if(std::getenv("K1_ENGAGE_POISE_TEST") && (lastDrawnPercent!=percent || draws%60==0)) {
        int box[4];ASLgl::glGetIntegerv(0x0C10,box);
        fprintf(stderr,"K1_POISE_METER_DRAW percent=%d viewport=%d,%d hilt=1 actual=%d draw=%u\n",
            percent,viewport[2],viewport[3],box[3],draws);
    }
    lastDrawnPercent=percent;
    ASLgl::glScissor(oldBox[0],oldBox[1],oldBox[2],oldBox[3]);
    if(!clipped) ASLgl::glDisable(0x0C11);
    AurGUICloseViewport();
    previewTextureState.Restore();
    if(trace) {
        renderTrace("after");
        std::vector<unsigned char> frameAfter(frameBefore.size());
        readFrame(frameViewport,frameAfter.data());
        size_t outsideChanged=0,insideChanged=0;
        int chromaMinX=frameViewport[2],chromaMaxX=-1,chromaMinY=frameViewport[3],chromaMaxY=-1;size_t chromaPixels=0;
        for(int y=0;y<frameViewport[3];++y) for(int x=0;x<frameViewport[2];++x) {
            size_t i=(size_t(y)*frameViewport[2]+x)*3;
            if(frameBefore[i]==frameAfter[i] && frameBefore[i+1]==frameAfter[i+1] && frameBefore[i+2]==frameAfter[i+2]) continue;
            const int sx=x+frameViewport[0],sy=y+frameViewport[1];
            if(sx>=viewport[0] && sx<viewport[0]+viewport[2] && sy>=viewport[1] && sy<viewport[1]+viewport[3]) {
                ++insideChanged;
                const int r=frameAfter[i],g=frameAfter[i+1],b=frameAfter[i+2];
                if(std::max({r,g,b})>80 && std::max({r,g,b})-std::min({r,g,b})>40) {
                    ++chromaPixels;chromaMinX=std::min(chromaMinX,x);chromaMaxX=std::max(chromaMaxX,x);
                    chromaMinY=std::min(chromaMinY,y);chromaMaxY=std::max(chromaMaxY,y);
                }
            }
            else ++outsideChanged;
        }
        bool cacheRestored=GLRender::cm_uiCurrentStage==previewTextureState.cachedStage;
        for(int unit=0;unit<6;++unit) cacheRestored &= GLRender::cm_nGLCurrentTID[unit]==previewTextureState.cachedTexture[unit];
        int programId=0,depthMask=0,blendSrc=0,blendDst=0;
        ASLgl::glGetIntegerv(0x8B8D,&programId);ASLgl::glGetIntegerv(0x0B72,&depthMask);
        ASLgl::glGetIntegerv(GL_BLEND_SRC,&blendSrc);ASLgl::glGetIntegerv(GL_BLEND_DST,&blendDst);
        const bool drawingRestored=programId==previewTextureState.programId && depthMask==previewTextureState.depthMask &&
            blendSrc==previewTextureState.blendSrc && blendDst==previewTextureState.blendDst &&
            (ASLgl::glIsEnabled(GL_BLEND)!=0)==previewTextureState.blend &&
            k1host_save_preview_program()==previewTextureState.program;
        fprintf(stderr,"K1_POISE_RENDER_ISOLATION_%s percent=%d outside_changed=%zu inside_changed=%zu cache_restored=%d drawing_restored=%d\n",
            !outsideChanged && cacheRestored && drawingRestored?"PASS":"FAIL",percent,outsideChanged,insideChanged,cacheRestored,drawingRestored);
        if(dualTrace)fprintf(stderr,"K1_DUAL_METER_RENDER_%s hand=%d percent=%d colored_pixels=%zu x=%d,%d y=%d,%d\n",
            !outsideChanged && cacheRestored && drawingRestored?"PASS":"FAIL",hudOwner && this==&hudOwner->m_poiseOffhand?1:0,percent,
            chromaPixels,chromaMinX,chromaMaxX,chromaMinY,chromaMaxY);
    }
}
void AurGUIGetScreenSize(int& width,int& height);
float PoisePreviewBounds(Gob* model,Vector& low,Vector& high,Vector& beamMidpoint) {
    model->GetBoundingBox(low,high);float bladeLength=0.f;
    auto visit=[&](auto&& self,Part* part)->void {
        if(auto* node=part->m_pNode->AsMdlNodeLightsaber()) if(node->m_paPositions) {
            for(int i=0;i<8;++i) {
                const auto p=node->m_paPositions[i];const auto v=PartPointWorld(part,Vector(p.x,p.y,p.z));
                low.x=std::min(low.x,v.x);low.y=std::min(low.y,v.y);low.z=std::min(low.z,v.z);
                high.x=std::max(high.x,v.x);high.y=std::max(high.y,v.y);high.z=std::max(high.z,v.z);
            }
            const auto a=node->m_paPositions[1],b=node->m_paPositions[2];
            const Vector base=PartPointWorld(part,Vector(a.x,a.y,a.z)),tip=PartPointWorld(part,Vector(b.x,b.y,b.z));
            const float length=magnitude(tip-base);
            if(length>bladeLength){bladeLength=length;beamMidpoint=(base+tip)*.5f;}
        }
        for(int i=0;i<part->m_nChildren;++i) self(self,part->m_paChildren[i]);
    };if(model->m_pPart) visit(visit,model->m_pPart);
    if(const auto* geometry=SwordModelGeometry(model)) {
        bladeLength=geometry->tipHigh-geometry->gripHigh;
        beamMidpoint=Vector(0,0,(geometry->tipHigh+geometry->gripHigh)*.5f);
    }
    return bladeLength;
}
Quaternion PoisePreviewOrientation(const Vector& size) {
    Vector axis(0,0,1);
    if(size.x>size.y && size.x>size.z) axis=Vector(1,0,0);
    else if(size.y>size.z) axis=Vector(0,1,0);
    const Vector horizontal(1,0,0);Quaternion q;
    if(dot(axis,horizontal)<.999f) q=Quaternion(normalize(cross(axis,horizontal)),1.5707963268f);
    return q;
}
Quaternion PoiseWeaponOrientation(Gob* model,const Vector& size) {
    const Quaternion horizontal=PoisePreviewOrientation(size);
    return SwordModelGeometry(model)?normalize(horizontal*Quaternion(Vector(0,0,1),1.5707963268f)):horizontal;
}
float PoiseWeaponFrameLength(Gob* model,const Vector& size) {
    const float length=std::max(.3f,std::max(size.x,std::max(size.y,size.z)));
    if(!SwordModelGeometry(model))return length;
    const Quaternion orientation=PoiseWeaponOrientation(model,size);
    const Vector x=orientation.xdir(),y=orientation.ydir(),z=orientation.zdir();
    const float width=std::fabs(x.x)*size.x+std::fabs(y.x)*size.y+std::fabs(z.x)*size.z;
    const float height=std::fabs(x.z)*size.x+std::fabs(y.z)*size.y+std::fabs(z.z)*size.z;
    const float depth=std::fabs(x.y)*size.x+std::fabs(y.y)*size.y+std::fabs(z.y)*size.z;
    const float aspect=420.f/74.f,lens=2.f*std::tan(12.5f*.01745329252f)*aspect*.86f;
    return std::max(length,std::max(width,height*aspect)+depth*lens*.5f);
}
float PoiseStaffSizeRatio(const K1PoiseWeaponView& view) {
    if(!DoubleBladePoiseWeapon() || view.singleModelLength<=0 || view.singleBladeLength<=0 || view.bladeLength<=0) return 1.f;
    const float lens=2.f*std::tan(12.5f*.01745329252f)*(420.f/74.f)*.86f;
    // Match effective depth at the blade, not just camera distance. Authored
    // bounds recenter the single saber's beam closer to the preview camera.
    const float desiredDistance=(view.singleModelLength/lens+view.singleBladeDepth)*view.bladeLength/view.singleBladeLength-view.bladeDepth;
    return desiredDistance>0?.81f*view.modelLength/lens/desiredDistance:1.f;
}
void FramePoiseSaber(K1PoiseWeaponView& view) {
    if(!view.m_scene.m_pCamera || view.m_extent.h<=0) return;
    const float aspect=float(view.m_extent.w)/view.m_extent.h;
    auto* camera=view.m_scene.m_pCamera;
    camera->SetViewAngle(25.f);
    camera->SetPosition(Vector(0,-view.modelLength/(2.f*std::tan(12.5f*.01745329252f)*aspect*.86f),0));
    camera->SetOrientation(Quaternion(Vector(1,0,0),1.5707963268f));
}
void LayoutPoiseHud(CSWGuiMainInterface* hud) {
    int width=0,height=0;AurGUIGetScreenSize(width,height);
    if(width<=0 || height<=0) return;
    auto& view=hud->m_poiseWeapon;auto& label=hud->m_poisePercent;
    const float scale=height/720.f;
    const auto swordScale=[](const CExoString& resource) {
        for(const auto& geometry:swordGeometry)if(!_stricmp(resource.CStr(),geometry.resource))
            return geometry.doubleBlade?1.2f:.9f;
        return 1.f;
    };
    // Resize the entire sword preview, including its hilt. Keeping the viewport
    // aspect preserves native framing and grip cropping; saber sizing is unchanged.
    const float ratio=swordScale(view.resource);
    const int w=std::min(width,int(std::lround(420.f*scale*PoiseStaffSizeRatio(view)*ratio))),
              h=int(std::lround(74.f*scale*ratio));
    const bool dual=DualMode();
    const int row=dual?int(std::lround(24.f*scale)):0;
    auto& offhand=hud->m_poiseOffhand;
    const float offhandRatio=swordScale(offhand.resource);
    const int offhandW=std::min(width,int(std::lround(420.f*scale*PoiseStaffSizeRatio(view)*offhandRatio))),
              offhandH=int(std::lround(74.f*scale*offhandRatio));
    if(view.layoutWidth==width && view.layoutHeight==height && view.m_extent.w==w && view.m_extent.h==h &&
       offhand.m_extent.w==offhandW && offhand.m_extent.h==offhandH &&
       offhand.m_extent.y==int(std::lround(6.f*scale))+row) return;
    view.layoutWidth=width;view.layoutHeight=height;
    const int x=(width-w)/2,y=int(std::lround(6.f*scale));
    view.SetExtent(CSWGuiExtent(x,y,w,h));
    offhand.SetExtent(CSWGuiExtent((width-offhandW)/2,y+row,offhandW,offhandH));
    offhand.layoutWidth=width;offhand.layoutHeight=height;
    // Native K1 strings cache their creation-time scale. Reflow this local label
    // when display size changes, keeping its box large enough for a complete line.
    if(label.m_text.m_pString) label.m_text.m_pString->SetFontScale(height*1.3f/768.f);
    const int labelY=y+row+h-int(std::lround(44.f*scale));
    label.SetExtent(CSWGuiExtent(x,labelY,w,int(std::ceil(40.f*scale))));
    const int textHeight=std::max(label.m_extent.h,label.m_text.GetIdealHeight()+int(std::ceil(6.f*scale)));
    label.SetExtent(CSWGuiExtent(x,labelY,w,textHeight));
    FramePoiseSaber(view);
    FramePoiseSaber(offhand);
}
void UpdateDualPoiseSaber(CSWGuiMainInterface* hud,CSWCCreature* client,int percent) {
    auto& view=hud->m_poiseOffhand;
    view.percent=percent;
    auto* live=client->GetModel(CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY,1);
    if(!live || !live->m_pModel) {view.m_tail.bVisible=false;return;}
    const CExoString resource(live->m_pModel->m_szName);
    if(view.resource!=resource) {
        while(view.m_scene.m_aModels.m_count)view.m_scene.RemoveModel(0,1);
        auto* model=static_cast<Gob*>(view.m_scene.AddModel(resource,-1));
        if(!model){view.m_tail.bVisible=false;return;}
        view.resource=resource;view.m_scene.m_pScene->SetAmbient(Vector(1,1,1));
        model->m_bAlwaysRendering=true;
        const int powered=model->PlayAnimation(const_cast<char*>("powered"),0,AnimRun::kAnimNoBlendIn,0.f);model->Animate(.0001f);
        Vector low,high,midpoint;view.bladeLength=PoisePreviewBounds(model,low,high,midpoint);
        const auto q=PoiseWeaponOrientation(model,high-low);
        model->SetOrientation(q);model->SetPosition(Rotate(q,(low+high)*-.5f));
        view.sourceModelLength=PoiseWeaponFrameLength(model,high-low);
        if(std::getenv("K1_ENGAGE_DUAL_TEST"))fprintf(stderr,"K1_DUAL_OFFHAND_PREVIEW model=%s powered=%d blade=%g bounds=%g,%g,%g scene_bound=%d camera_bound=%d\n",resource.CStr(),powered,view.bladeLength,
            (high-low).x,(high-low).y,(high-low).z,model->m_pScene==view.m_scene.m_pScene,
            static_cast<Gob*>(view.m_scene.m_pCamera->AsCAurObject())->m_pScene==view.m_scene.m_pScene);
    }
    // Both rows share the main meter's framing and poise value, while each
    // preview owns its actual equipped hilt/crystal model and render state.
    view.modelLength=hud->m_poiseWeapon.modelLength;
    if(SwordModelGeometry(live)) {
        view.modelLength=std::max(view.modelLength,view.sourceModelLength);
    }
    FramePoiseSaber(view);
}
void UpdatePoiseHud(CSWGuiMainInterface* hud) {
    auto* actor=ServerActor();
    auto* client=Actor();
    const bool visible=state.mode!=K1EngageState::Mode::Exploration && HasMeleePoise(actor);
    hud->m_poiseWeapon.m_tail.bVisible=visible;
    hud->m_poiseOffhand.m_tail.bVisible=visible && DualMode();
    hud->m_poisePercent.m_tail.bVisible=visible;
    if(!visible || !client) return;
    LayoutPoiseHud(hud);
    const int percent=meleePoise[actor->m_objectId].percent;
    hud->m_poiseWeapon.percent=percent;
    char text[16];std::snprintf(text,sizeof(text),"%d%%",percent);
    hud->m_poisePercent.m_text.m_params.SetText(CExoString(text));
    hud->m_poisePercent.m_text.m_params.bHilight=false;
    hud->m_poisePercent.m_text.m_params.m_fAlpha=1.f;
    auto& numberParams=hud->m_poisePercent.m_text.m_params;
    numberParams.SetColor(PoiseNumberColor(numberParams.m_defaultColor));
    if(std::getenv("K1_ENGAGE_POISE_TEST")) {
        static int last=-1,lastWidth=0,lastHeight=0;
        auto& view=hud->m_poiseWeapon;
        if(last!=percent || lastWidth!=view.layoutWidth || lastHeight!=view.layoutHeight) {
            auto& label=hud->m_poisePercent;auto* string=label.m_text.m_pString;
            fprintf(stderr,"K1_POISE_TEXT_LAYOUT text=%s box=%d,%d,%d,%d ideal_height=%d font_height=%d lines=%d visible=%d\n",text,
                label.m_extent.x,label.m_extent.y,label.m_extent.w,label.m_extent.h,label.m_text.GetIdealHeight(),
                label.m_text.GetFontHeight(),string?string->GetNumLines():-1,label.m_tail.bVisible);
            const bool fit=string && string->GetNumLines()==1 && label.m_text.GetIdealHeight()<=label.m_extent.h;
            fprintf(stderr,"K1_POISE_TEXT_FIT_%s percent=%d screen=%d,%d meter=%d,%d,%d,%d\n",fit?"PASS":"FAIL",percent,
                view.layoutWidth,view.layoutHeight,view.m_extent.x,view.m_extent.y,view.m_extent.w,view.m_extent.h);
            last=percent;lastWidth=view.layoutWidth;lastHeight=view.layoutHeight;
        }
    }
    auto* live=client->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
    if(!live || !live->m_pModel) return;
    auto& view=hud->m_poiseWeapon;
    if(DualMode()) UpdateDualPoiseSaber(hud,client,percent);
    if(DoubleBladePoiseWeapon() && view.singleModelLength<=0.f) {
        // Measure the established blue single-saber framing through the same
        // native loader/bounds path; never depend on which weapon loaded first.
        const int index=view.m_scene.m_aModels.m_count;
        auto* reference=static_cast<Gob*>(view.m_scene.AddModel(CExoString("w_Lghtsbr_001"),-1));
        if(reference) {
            reference->m_bAlwaysRendering=true;
            reference->PlayAnimation(const_cast<char*>("powered"),0,AnimRun::kAnimNoBlendIn,0.f);reference->Animate(.0001f);
            Vector low,high,midpoint;view.singleBladeLength=PoisePreviewBounds(reference,low,high,midpoint);
            const Vector size=high-low;view.singleModelLength=std::max(.3f,std::max(size.x,std::max(size.y,size.z)));
            view.singleBladeDepth=Rotate(PoisePreviewOrientation(size),midpoint-(low+high)*.5f).y;
            view.m_scene.RemoveModel(index,1);
            fprintf(stderr,"K1_POISE_SINGLE_REFERENCE length=%g blade=%g depth=%g\n",view.singleModelLength,view.singleBladeLength,view.singleBladeDepth);
        }
    }
    // Native saber resources encode both the hilt type and current crystal variant.
    // Native solid swords retain their actual model and a grip-bounded crop.
    const CExoString resource(PoiseWeaponResource(live));
    if(view.resource==resource) return;
    while(view.m_scene.m_aModels.m_count) view.m_scene.RemoveModel(0,1);
    auto* model=static_cast<Gob*>(view.m_scene.AddModel(resource,-1));
    if(!model) return;
    view.resource=resource;
    view.m_scene.m_pScene->SetAmbient(Vector(1,1,1));
    model->m_bAlwaysRendering=true;
    model->PlayAnimation(const_cast<char*>("powered"),0,AnimRun::kAnimNoBlendIn,0.f);
    model->Animate(.0001f);
    Vector low,high,midpoint;view.bladeLength=PoisePreviewBounds(model,low,high,midpoint);
    const Vector size=high-low;
    const Quaternion q=PoiseWeaponOrientation(model,size);
    model->SetOrientation(q);
    model->SetPosition(Rotate(q,(low+high)*-.5f));
    view.bladeDepth=Rotate(q,midpoint-(low+high)*.5f).y;
    const float height=PoiseWeaponFrameLength(model,size);
    view.sourceModelLength=height;
    view.modelLength=height;LayoutPoiseHud(hud);FramePoiseSaber(view);
    if(DualMode()) UpdateDualPoiseSaber(hud,client,percent);
    fprintf(stderr,"K1_POISE_HUD model=%s height=%g blade=%g depth=%g size_ratio=%g percent=%d\n",resource.CStr(),height,view.bladeLength,view.bladeDepth,PoiseStaffSizeRatio(view),percent);
}

void K1EngageHud(CSWGuiMainInterface* hud) {
    if (!K1EngageEnabled()) return;
    if (hudOwner != hud) {
        hudOwner = hud;
        const CSWGuiExtent extent(hud->m_extent.w/2-70, hud->m_extent.h-190,140,34);
        CSWGuiTextParams label(hud->m_tbPause.m_text.m_params);
        label.SetText(CExoString("Engage"));
        label.SetAlignment(SWGUIALIGN_CENTER | SWGUIALIGN_MIDDLE);
        // Use the stock K1 confirmation button's normal and highlighted frames.
        const CSWGuiBorderParams border(CResRef("boxline1"), CResRef("boxline2"),
            CResRef(), 2, 6, 1.0f, Vector(.934f,1.0f,.934f), 9);
        const CSWGuiBorderParams highlight(CResRef("boxline3"), CResRef("boxline4"),
            CResRef(), 2, 6, 1.0f, Vector(.871f,1.0f,.871f), 9);
        hud->m_btnEngage.Initialize(extent,label,border,highlight);
        // The font's visible glyphs sit two pixels below the text box center.
        CSWGuiExtent labelExtent = hud->m_btnEngage.m_text.m_extent;
        labelExtent.y -= 2;
        hud->m_btnEngage.m_text.SetExtent(labelExtent);
        // Like the other PC HUD buttons, highlight on hover without taking focus.
        hud->m_btnEngage.m_tail.bFocusOnHover = false;
        hud->m_btnEngage.AddEvent(SWGUIEVENT_FOCUS,hud,
            static_cast<void(CSWGuiObject::*)(CSWGuiControl*)>(&CSWGuiMainInterface::OnEngageHover));
        hud->m_btnEngage.AddEvent(INPUTEVENT_GUI_A,hud,
            static_cast<void(CSWGuiObject::*)(CSWGuiControl*)>(&CSWGuiMainInterface::OnFlourishButtonPressed));
        hud->AddControl(&hud->m_btnEngage);
        hud->m_poiseWeapon.SetExtent(CSWGuiExtent(0,0,420,74));
        hud->m_poiseWeapon.m_tail.bIgnoreMouse=true;
        hud->AddControl(&hud->m_poiseWeapon);
        hud->m_poiseOffhand.SetExtent(CSWGuiExtent(0,0,420,74));
        hud->m_poiseOffhand.m_tail.bIgnoreMouse=true;
        hud->AddControl(&hud->m_poiseOffhand);
        label.SetText(CExoString("100%"));
        const CSWGuiBorderParams noBorder(CResRef(),CResRef(),CResRef(),0,0,0.f,Vector(1,1,1),0);
        hud->m_poisePercent.Initialize(CSWGuiExtent(0,74,420,40),label,noBorder,1.f);
        hud->m_poisePercent.m_tail.bIgnoreMouse=true;
        hud->AddControl(&hud->m_poisePercent);
        hud->m_parryReticle.m_tail.bIgnoreMouse=true;
        hud->AddControl(&hud->m_parryReticle);
    }
    hud->m_btnEngage.m_tail.bVisible = state.mode == K1EngageState::Mode::Planning;
    UpdatePoiseHud(hud);
    int reticleWidth=0,reticleHeight=0;AurGUIGetScreenSize(reticleWidth,reticleHeight);
    hud->m_parryReticle.SetExtent(CSWGuiExtent(0,0,reticleWidth,reticleHeight));
    hud->m_parryReticle.m_tail.bVisible=K1EngageActive();
    const bool enabled=Supported(Actor());
    if (hud->m_btnEngage.m_tail.bEnabled != enabled)
        hud->m_btnEngage.SetEnabled(enabled);
    const bool canCancel = CanCancelQueuedActions(ServerActor());
    hud->m_btnClearAll.m_tail.bVisible = state.mode != K1EngageState::Mode::Exploration &&
        (canCancel || !Spotted());
    hud->m_btnClearAll.m_nToolTipStrRef = canCancel ? STRREF_TIP_CLEAR_COMBAT_ACTION : STRREF_TIP_DISENGAGE;
}
void K1EngageHudDestroyed(CSWGuiMainInterface* hud) {
    if (hudOwner==hud) {
        WearablePoseDonors().clear();
        playerPlayback.nativeGait={};
        ReleaseNpcPlayback();
        cameraBlend = {};
        enemyRepositionAfter.clear();
        utilityBlend = {};
        retainedPlanningPose = false;
        npcBolts.clear();
        meleePoise.clear();
        parrySuccessUntil=0;
        blockObservations.clear();
        contactBlockClocks.clear();contactEffectClocks.clear();contactGuardClock={};
        nativeActionBody = false;
        nativeIdlePose.clear();nativeStaffGuardPose.clear();nativeDualGuardPose.clear();
        hudOwner=nullptr;loadedBody=nullptr;currentBase=currentOverlay=currentAttack=nullptr;state=K1EngageState();
        keyW=keyS=keyA=keyD=keyShift=false;movement.Clear();
        encounter.clear();
    }
}
namespace {
bool CheckRigActorOwnership(CSWCCreature* actor,bool source,const CResRef& texture,const std::string& headName,const std::string& weaponName,const char* stage,bool wearable=false) {
    Gob* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);bool ok=body && IsSourceBody(body)==source;int nodes=0;
    if(body) {
        auto count=[&](auto&& self,Part* p)->void {
            if(p->m_pNode->m_pTree!=body->m_pModel) return;
            ++nodes;for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
        };count(count,body->m_pPart);
        bool legRig=body->GetPart(const_cast<char*>("rfemurX"))!=nullptr;
        bool motionRig=body->GetPart(const_cast<char*>("j_motion"))!=nullptr;
        ok=ok && (wearable?nodes>=50:nodes==(source?(motionRig?70:legRig?69:66):61)) && !body->m_pAddInModel;
        if(source && legRig) ok=ok && body->GetPart(const_cast<char*>("lfemurX")) && (wearable || body->GetPart(const_cast<char*>("TorsoLegs")));
    }
    auto attached=[&](unsigned char slot,const char* hook,const std::string& expected) {
        Gob* child=actor->GetModel(slot,1);
        if(expected.empty()) return child==nullptr || (wearable && body && child->m_pScene!=body->m_pScene);
        if(!body || !child || !child->m_pModel || expected!=child->m_pModel->m_szName || !child->m_pBehavior) return false;
        auto* attachment=static_cast<CAurBehaviorAttach*>(child->m_pBehavior->AsBehaviorAttach());
        return attachment && attachment->m_pAttachedPart==body->GetPart(const_cast<char*>(hook));
    };
    bool head=attached(CSWCAnimBase::ANIM_MODEL_HEAD,"headhook",headName);
    bool weapon=attached(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,"rhand",weaponName);
    bool material=actor->GetAnimationBase() && actor->GetAnimationBase()->m_resModelTexture==texture;
    if(wearable && !material && body && body->m_pModel && actor->GetAnimationBase()) {
        const char* native=WearableNativeResource(body->m_pModel->m_szName);
        material=native && !_stricmp(native,texture.GetResRefStr()) &&
            !_stricmp(actor->GetAnimationBase()->m_resModelTexture.GetResRefStr(),body->m_pModel->m_szName);
    }
    ok=ok&&head&&weapon&&material;
    fprintf(stderr,"K1_ENGAGE_RIG_ACTOR_%s stage=%s source=%d nodes=%d head_present=%d head_link=%d weapon_link=%d texture=%d\n",ok?"PASS":"FAIL",stage,source,nodes,!headName.empty(),head,weapon,material);
    return ok;
}
bool CheckRigActorNativePose(CSWCCreature* actor,const char* stage) {
    Gob* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);CSWCAnimBase oracleOwner;
    bool ok=body&&!IsSourceBody(body)&&oracleOwner.LoadModel(CResRef("pmbbl"),CSWCAnimBase::ANIM_MODEL_BODY);
    Gob* oracle=oracleOwner.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);int count=0;float pe=0;double qe=0;
    if(ok&&oracle) {
        bool always=body->m_bAlwaysRendering;body->m_bAlwaysRendering=true;body->EnableForceAnimations();
        oracle->m_bAlwaysRendering=true;oracle->EnableForceAnimations();body->EndLookAtAnimate();
        for(const char* clip:{"pause1","walk","run"}) for(float time:{.13f,.55f}) {
            for(Gob* model:{body,oracle}) {
                model->PlayAnimation(nullptr,1,0,0);model->Reset();
                ok=ok&&model->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,time)!=0;
                // The installed actor is attached to a scene and may already have
                // evaluated this tick. The detached oracle has no tick gate.
                model->m_nLastAnimateTick=-1;model->Animate(.000001f);
            }
            auto compare=[&](auto&& self,Part* p)->void {
                if(p->m_pNode->m_pTree!=oracle->m_pModel) return;
                Part* other=body->GetPart(p->m_pNode->TranslateNodeName());
                if(!other) {ok=false;return;}
                pe=std::max(pe,magnitude(other->m_vLocalPosition-p->m_vLocalPosition));
                const Quaternion& a=p->m_qLocalOrientation;const Quaternion& b=other->m_qLocalOrientation;
                double ab=double(a.w)*b.w+double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z;
                double aa=double(a.w)*a.w+double(a.x)*a.x+double(a.y)*a.y+double(a.z)*a.z;
                double bb=double(b.w)*b.w+double(b.x)*b.x+double(b.y)*b.y+double(b.z)*b.z;
                double angle=aa>0&&bb>0?2*std::acos(std::min(1.,std::fabs(ab)/std::sqrt(aa*bb))):999;
                if(magnitude(other->m_vLocalPosition-p->m_vLocalPosition)>.0002f || angle>.002)
                    fprintf(stderr,"K1_ENGAGE_RIG_NATIVE_DIFF stage=%s clip=%s time=%g bone=%s position=%g angle=%g owns=%d\n",stage,clip,time,p->m_pNode->TranslateNodeName(),magnitude(other->m_vLocalPosition-p->m_vLocalPosition),angle,K1EngageOwnsAnimation(body));
                qe=std::max(qe,angle);ok=ok&&std::isfinite(angle);++count;
                for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
            };compare(compare,oracle->m_pPart);
        }
        body->PlayAnimation(nullptr,1,0,0);body->Reset();body->m_bAlwaysRendering=always;
        actor->SetAnimation(actor->GetLoopingAnimation(),0,0);
    }
    ok=ok&&count==366&&pe<.0002f&&qe<.002;
    fprintf(stderr,"K1_ENGAGE_RIG_NATIVE_%s stage=%s comparisons=%d position=%g angle=%g\n",ok?"PASS":"FAIL",stage,count,pe,qe);
    return ok;
}
void ControlProbeTick(int& phase,int& ticks,unsigned long long opponentId) {
    static int speedStage=0;static long long speedStart=0;
    static unsigned long long speedEffect=0;static float originalSpeed=1;
    static bool ok=true;static Vector previous;static float yaw=0;static int serial=0,hp=0;
    static float nativeCombatEyeHeight=0;
    static Gob* beforePlanning = nullptr;
    static float planningYaw = 0.f, planningLeft = 0.f, planningSwingTime = 0.f;
    static Vector castPosition;
    static Vector exitOrbit;
    static int castDoneTick = 0;
    static int castTargetHp = 0;
    static long long victoryDeadline=0;
    static unsigned char casterSlot = 0;
    static bool missSwing=false;
    static CResRef nativeTexture;static std::string nativeHead,nativeWeapon;
    CSWCCreature* actor=Actor();if(!actor) return;
    auto check=[&](bool pass,const char* test) {ok=ok&&pass;fprintf(stderr,"K1_ENGAGE_CONTROL_%s %s\n",pass?"CHECK":"FAIL",test);};
    auto send=[](const char* text) {FILE* file=fopen("control-input.txt","w");if(file){fputs(text,file);fclose(file);}};
    static const char* directions[]={
        "keydown:26\n+2:ldown:640,360\n+4:lup:640,360\n+10:keyup:26\n",
        "keydown:26\nkeydown:4\n+2:ldown:640,360\n+4:lup:640,360\n+10:keyup:26\n+10:keyup:4\n",
        "keydown:26\nkeydown:7\n+2:ldown:640,360\n+4:lup:640,360\n+10:keyup:26\n+10:keyup:7\n",
        "keydown:4\n+2:ldown:640,360\n+4:lup:640,360\n+10:keyup:4\n",
        "keydown:7\n+2:ldown:640,360\n+4:lup:640,360\n+10:keyup:7\n",
        "keydown:22\nkeydown:4\n+2:ldown:640,360\n+4:lup:640,360\n+10:keyup:22\n+10:keyup:4\n",
        "keydown:22\nkeydown:7\n+2:ldown:640,360\n+4:lup:640,360\n+10:keyup:22\n+10:keyup:7\n"};
    auto swing=[&](int trial) {
        std::string command=trial/7==1?"keydown:225\n":"keyup:225\n";
        command+=directions[trial%7];
        if(trial/7<2 && std::getenv("K1_ENGAGE_RIG_DIAG")) {
            size_t at=0;
            while((at=command.find("+10:",at))!=std::string::npos) command.replace(at,4,"+45:");
        }
        if(trial/7==2) {
            size_t at=0;
            while((at=command.find("+10:",at))!=std::string::npos) command.replace(at,4,"+3:");
        }
        send(command.c_str());
    };
    if(phase==2 && speedStage<2) {
        auto* player=actor->GetServerCreature();
        const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
        if(!speedStage) {
            originalSpeed=player->GetMovementRateFactor(0);
            player->SetMovementRateFactor(.125f);ReconcileSandboxMovement(player);
            check(player->GetMovementRateFactor(0)==1.f,"orphan_slow_factor_recovers");
            player->SetMovementRateFactor(originalSpeed);
            auto* effect=new CGameEffect(1);effect->m_subType=CSWSEffectListHandler::EFFECT_MOVEMENT_SPEED_DECREASE;
            // Planning runs at .005 world speed; a short native duration keeps
            // this expiry check inside the existing controller wall-time budget.
            effect->m_durationTypeBits=CGameEffect::DURATION_TYPE_TEMPORARY;effect->m_fDuration=.002f;
            effect->SetInteger(0,50);speedEffect=effect->m_effectId;player->ApplyEffect(effect,0,1);
            ReconcileSandboxMovement(player);
            check(std::fabs(player->GetMovementRateFactor(0)-originalSpeed*.5f)<.0001f,"live_slow_effect_preserved");
            speedStart=now;speedStage=1;return;
        }
        bool present=false;for(int i=0;i<player->m_effects.m_count;++i)present|=player->m_effects.m_pArray[i]->m_effectId==speedEffect;
        if(present && now-speedStart<3000000)return;
        check(!present && std::fabs(player->GetMovementRateFactor(0)-originalSpeed)<.0001f,"native_slow_expiry_restores_speed");
        speedStage=2;
    }
    if(phase==2) {
        CSWSCreature* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(opponentId);
        npc->ClearAllActions(1);g_pAppManager->m_pServerExoApp->GetServerAIMaster()->SetAILevel(npc,-1);
        // Keep the target alive through the21 real swing checks and utility
        // playback; phase51 explicitly kills it to verify victory restoration.
        npc->m_hitPoints=1000;npc->SetCurrentHitPoints(npc->GetMaxHitPoints(0));
        npc->AddToVisibleList(actor->GetServerCreature()->m_objectId,1,0,0,0);
        actor->SetPrecalculatedPosition(Vector(200,100,0));
        if(RigModeEnabled()) {
            nativeTexture=actor->GetAnimationBase()->m_resModelTexture;
            Gob* head=actor->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1);Gob* weapon=actor->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
            nativeHead=head&&head->m_pModel?head->m_pModel->m_szName:"";nativeWeapon=weapon&&weapon->m_pModel?weapon->m_pModel->m_szName:"";
        }
        Gob* before=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        {
            // Sample the unmodified native camera service before rig ownership.
            CSWBehaviorCameraCombat nativeCamera(g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->AsCAurObject());
            float unusedYaw=0;
            nativeCombatEyeHeight=nativeCamera.CalculateDesiredLocation(actor,actor,0,unusedYaw).z-actor->m_position.z;
        }
        bool entered=K1EngageEnter();
        if(std::getenv("K1_ENGAGE_EXPECT_MISSING_RIG")) {
            check(RigModeEnabled() && !entered && !rigBodyRequested && !rigBodyOwner,
                "missing_rig_rejects_ownership");
            check(before==actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1) && K1EngagePlanning() && !IsGamePaused(),
                "missing_rig_preserves_native_body_and_slow_planning");
            check(CheckRigActorOwnership(actor,false,nativeTexture,nativeHead,nativeWeapon,"missing"),"missing_rig_preserves_native_attachments");
            check(CheckRigActorNativePose(actor,"missing"),"missing_rig_preserves_native_pose");
            fprintf(stderr,"K1_ENGAGE_MISSING_RIG_%s\nK1_ENGAGE_CONTROL_%s\n",ok?"PASS":"FAIL",ok?"PASS":"FAIL");
            phase=-1;return;
        }
        check(entered,"enter");phase=10;ticks=0;return;
    }
    if(phase==10) {
        keyShift=true;state.rightHeld=true;dualGuardAfterNative=true;
        K1EngageReleaseHeldInput();
        check(!keyShift && !state.ForceWalk() && !dualGuardAfterNative,"focus_loss_clears_walk_guard_and_native_hold");
        sandboxMenuOpen=true;keyShift=true;K1EngageKey(225,false);sandboxMenuOpen=false;
        check(!keyShift,"menu_key_release_does_not_latch_shift");
        auto* moduleCamera=g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera;
        auto* combatCamera=static_cast<CSWBehaviorCameraCombat*>(moduleCamera->GetBehavior(BEHAVIOR_CAMERA_COMBAT));
        float savedPitch=aimPitch;aimPitch=0;
        bool placed=combatCamera && K1EngageCamera(combatCamera);
        float actualHeight=moduleCamera->GetPosition().z-actor->m_position.z;
        fprintf(stderr,"K1_CAMERA_HEIGHT native=%g engage=%g captured_head=%g style=%g\n",nativeCombatEyeHeight,actualHeight,nativeCameraHeadHeight,combatCamera?combatCamera->m_fHeight:0);
        check(placed && std::fabs(actualHeight-(nativeCombatEyeHeight+engageCameraHeightTrim))<.002f,"level_engage_camera_uses_reviewed_native_height_trim");
        const bool lookedUp=K1EngageLook(0,-2000) && combatCamera && K1EngageCamera(combatCamera);
        const Vector raisedEye=moduleCamera->GetPosition()-actor->m_position;
        const float horizontalDistance=std::hypot(raisedEye.x,raisedEye.y);
        const float desiredDistance=engageOrbitRadius*std::cos(aimPitch);
        fprintf(stderr,"K1_CAMERA_UP_LIMIT pitch=%g eye_height=%g horizontal=%g desired=%g\n",
            aimPitch,raisedEye.z,horizontalDistance,desiredDistance);
        check(lookedUp && aimPitch>0.f && aimPitch<1.22f &&
            raisedEye.z>=CameraPersonalSpace+engageCameraFloorMargin-.002f &&
            horizontalDistance>=desiredDistance-.15f,"upward_aim_preserves_camera_clearance_and_distance");
        aimPitch=savedPitch;
        if(combatCamera) {
            const auto eye=moduleCamera->GetPosition();const auto orientation=moduleCamera->GetOrientation();
            const float savedYaw=aimYaw;
            bool stable=true;int reversed=0;
            for(float yawOffset:{0.f,1.1f,3.f}) for(float pitch:{-.4f,0.f,.4f}) {
                aimYaw=savedYaw+yawOffset;aimPitch=pitch;
                const auto q=EngageOrbit(nativeCameraHeadHeight+combatCamera->m_fHeight+engageCameraHeightTrim);
                const Vector offset=q.zdir()*engageOrbitRadius;
                const Vector sample=actor->m_position+offset;
                moduleCamera->SetOrientation(q);
                float unused=0;
                Vector old=combatCamera->CalculateDesiredLocation(actor,actor,0,unused)-actor->m_position;
                if(old.x*offset.x+old.y*offset.y<0) ++reversed;
                SeedNativeCameraOrbit(moduleCamera,actor,sample);
                Vector next=combatCamera->CalculateDesiredLocation(actor,actor,0,unused)-actor->m_position;
                const float cosine=(next.x*offset.x+next.y*offset.y)/
                    std::max(.001f,std::hypot(next.x,next.y)*std::hypot(offset.x,offset.y));
                stable=stable && cosine>.9999f;
            }
            fprintf(stderr,"K1_CAMERA_HANDOFF_REFERENCE cases=9 legacy_reversed=%d\n",reversed);
            check(stable,"native_camera_handoff_retains_orbit_side_at_all_pitch_signs");
            aimYaw=savedYaw;aimPitch=savedPitch;
            moduleCamera->SetPosition(eye);moduleCamera->SetOrientation(orientation);
        }
        if(Gob* w=actor->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1)) {
            auto dump=[&](auto&& self,Part* p)->void {
                if(auto* n=p->m_pNode->AsMdlNodeLightsaber()) {
                    fprintf(stderr,"K1_BLADE_GEOMETRY part=%s scale=%g draw=%d vertices=%d\n",p->Name(),p->m_scale.m_fScale,p->m_bDrawEnabled,n->m_nVertexCount);
                    for(int i=0;i<8 && n->m_paPositions;++i) fprintf(stderr,"K1_BLADE_VERTEX %d %g %g %g\n",i,n->m_paPositions[i].x,n->m_paPositions[i].y,n->m_paPositions[i].z);
                }
                for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
            };dump(dump,w->m_pPart);
        }
        if(RigModeEnabled()) check(CheckRigActorOwnership(actor,true,nativeTexture,nativeHead,nativeWeapon,"engaged"),"source_rig_actor_ownership");
        Gob* owned=loadedBody;const char* baseBefore=currentBase;size_t posesBefore=lastPose.size();
        bool repeated=K1EngageEnter() && K1EngageEnter();
        auto* input=g_pAppManager->m_pClientExoApp->m_pInternal;
        auto targetBefore=input->m_lastTarget;
        CSWSCreature* opponent=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(opponentId);
        if(opponent && opponent->GetClientCreature()) input->m_lastTarget=opponent->GetClientCreature()->m_objectId;
        input->HandleInputEvent(INPUTEVENT_DEFAULT_ACTION,1);
        input->HandleInputEvent(INPUTEVENT_DEFAULT_ACTION,1);
        check(repeated && loadedBody==owned && currentBase==baseBefore && lastPose.size()==posesBefore &&
            actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1)==owned,"repeat_default_action_keeps_engage_body_and_pose");
        // Leave the hostile target selected for the raw R-key input below.
        K2_UNUSED(targetBefore);
        int count=actor->m_nFireForgetCount;unsigned short animation=actor->GetCurrentAnimation();
        Gob* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);int runs=body->m_paAnimations.m_count;
        g_pAppManager->m_pClientExoApp->PlayerFlourishWeapons();
        check(count==actor->m_nFireForgetCount && animation==actor->GetCurrentAnimation() && runs==body->m_paAnimations.m_count,
            "native_flourish_cannot_interrupt_engage");
        previous=actor->m_position;yaw=aimYaw;send("keydown:21\n+2:keyup:21\n+3:keydown:26\n");
    }
    if(phase==11) {
        check(K1EngageEngaged() && loadedBody==actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1) &&
            (!RigModeEnabled() || IsSourceBody(loadedBody)),"raw_r_keeps_engage_body_ownership");
        Vector delta=actor->m_position-previous;
        check(delta.x*std::cos(yaw)+delta.y*std::sin(yaw)>.8f,"forward_uses_native_collision_commit");
        check(std::fabs(movement.Speed()-NativeMoveSpeed(actor->GetServerCreature(),false))<.02f,"native_run_speed");
        fprintf(stderr,"K1_PLAYER_NATIVE_SPEED run=%g expected=%g\n",movement.Speed(),NativeMoveSpeed(actor->GetServerCreature(),false));
        previous=actor->m_position;send("keyup:26\nkeydown:7\n");
    }
    if(phase==12) {
        Vector delta=actor->m_position-previous;
        check(delta.x*std::sin(yaw)-delta.y*std::cos(yaw)>.8f,"right_strafe");
        check(std::fabs(actor->m_orientation.x-std::cos(yaw))<.001f&&std::fabs(actor->m_orientation.y-std::sin(yaw))<.001f,"strafe_keeps_aim");
        check(legsYaw<-.7f && currentBase && !strcmp(currentBase,"j_run"),"right_strafe_uses_jka_run_with_right_leg_yaw");
        check(std::fabs(legsYaw)<=.785399f,"live_leg_turn_within_45_degrees");
        previous=actor->m_position;send("keyup:7\nkeydown:26\nkeydown:4\n");
    }
    if(phase==13) {
        Vector delta=actor->m_position-previous;
        check(magnitude(delta)>.8f && magnitude(delta)<5.f,"diagonal_displacement_bound");
        previous=actor->m_position;send("keyup:4\nrdown:640,360\n");
    }
    if(phase==14) {
        check(K1EngageForceWalk() && g_pAppManager->m_pClientExoApp->m_pInternal->m_pPlayerControl->GetPlayerWalking(),"raw_right_mouse_forces_native_walk_flag");
        check(movement.Speed()<=NativeMoveSpeed(actor->GetServerCreature(),true)+.001f,"guard_walk_speed");
        check(std::fabs(movement.Speed()-NativeMoveSpeed(actor->GetServerCreature(),true))<.02f,"native_walk_speed");
        fprintf(stderr,"K1_PLAYER_NATIVE_SPEED walk=%g expected=%g\n",movement.Speed(),NativeMoveSpeed(actor->GetServerCreature(),true));
        send("keyup:26\nrup:640,360\nrel:80,-20\n");
    }
    if(phase==15) {
        check(std::fabs(aimYaw-yaw)>.05f,"raw_mouse_changes_aim");
        Vector cameraForward=g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->GetOrientation().zdir()*-1.f;
        check(cameraForward.x*std::cos(aimYaw)+cameraForward.y*std::sin(aimYaw)>.9f,"camera_points_along_aim");
        serial=attackSerial;swing(0);
    }
    if(phase>=16 && phase<=36) {
        if(ticks<60 || state.attacking) return;
        int trial=phase-16,index=trial%7;
        // Trial order is W, W+A, W+D, A, D, S+A, S+D.
        static const int expectedSwings[]={0,2,1,4,3,6,5};
        check(requestedDirection==expectedSwings[index] && swingDirection==expectedSwings[index] &&
            attackSerial==serial+1,"held_keys_dispatch_swapped_swing");
        fprintf(stderr,"K1_ENGAGE_CONTROL_DIRECTION mode=%d input=%d swing=%d serial=%d\n",
            trial/7,index,swingDirection,attackSerial);
        if(trial<20) {serial=attackSerial;swing(trial+1);}
    }
    if(phase==37) {
        if(std::getenv("K1_ENGAGE_VIEW_PITCH")) check(viewPitchPoses>0 && maximumViewPitch>.01f,"live_camera_pitch_reaches_engage_pose_controller");
        CSWCItem* item=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[4]);
        fprintf(stderr,"K1_SABER_TIMEOUT_DIAG server_combat=%d timer=%d client_combat=%d powered=%d\n",actor->GetServerCreature()->m_bCombatState,actor->GetServerCreature()->m_nCombatTimer,actor->m_bInCombat,item?item->m_bPowered:-1);
        check(actor->GetServerCreature()->m_bCombatState && actor->GetServerCreature()->m_nCombatTimer>0 && actor->m_bInCombat && item && item->m_bPowered,
            "engage_retains_combat_and_saber_power_without_enemy_hits");
        Vector bladeBase,bladeTip;
        // Owned cap ordinals1/2 measure .926997m; exclude the soft glow caps.
        check(Blade(actor,bladeBase,bladeTip) && magnitude(bladeTip-bladeBase)>.92f && magnitude(bladeTip-bladeBase)<.94f,
            "collision_blade_matches_native_central_beam_length");
        std::vector<K1SaberContact::Triangle> posed;CollisionTriangles(actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1),posed);
        bool surfaceHit=false,airMiss=!posed.empty();float highest=-1e9f;
        for(const auto& triangle:posed) {
            highest=std::max(highest,std::max(triangle.a.z,std::max(triangle.b.z,triangle.c.z)));
            Vector n=cross(triangle.b-triangle.a,triangle.c-triangle.a);
            if(!surfaceHit && magnitude(n)>.000001f) {
                Vector center=(triangle.a+triangle.b+triangle.c)/3.f;n=normalize(n)*.005f;
                surfaceHit=K1SaberContact::SegmentTriangle(center-n,center+n,triangle);
            }
        }
        Vector air=actor->m_position+Vector(0,0,highest-actor->m_position.z+1);
        for(const auto& triangle:posed) if(K1SaberContact::SegmentTriangle(air,air+Vector(1,0,0),triangle)) airMiss=false;
        check(surfaceHit && airMiss,"installed_posed_body_surface_hit_and_air_miss");
        if(item) {
            item->PowerItem(0,0,0,1);
            check(!Blade(actor,bladeBase,bladeTip),"unpowered_saber_has_no_damage_segment");
            MaintainSaberCombat(actor);
            check(item->m_bPowered!=0,"saber_reactivates_without_incoming_damage");
        }
        if(RigModeEnabled()) {
            fprintf(stderr,"K1_ENGAGE_LIVE_SPINE poses=%d chain=%d edge_error=%g\n",anatomicalPoses,anatomicalChain,anatomicalEdgeError);
            check(anatomicalPoses>0 && anatomicalChain && anatomicalEdgeError<.0002f,"live_anatomical_spine_connected");
        } else check(bridgedPoses>0,"live_torso_follows_locomotion_pelvis");
        check(currentBase && !strcmp(currentBase,"j_idle") && !currentOverlay,"neutral_idle_after_released_guard_and_attacks");
        bool nativeIdleSettled=!nativeIdlePose.empty();
        for(const BodyPose& pose:nativeIdlePose) {
            nativeIdleSettled=nativeIdleSettled &&
                magnitude(pose.part->m_vLocalPosition-pose.position)<.0002f &&
                std::fabs(dot(normalize(pose.part->m_qLocalOrientation),normalize(pose.orientation)))>.999999f;
        }
        check(nativeIdleSettled,"standing_recovery_preserves_native_body_idle");
        CSWSCreature* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(opponentId);
        aimYaw=0;movement.Clear();actor->SetPrecalculatedPosition(Vector(209.5888f,100.0049f,0));
        npc->SetPosition(Vector(210.5888f,100.0049f,0),0,0,0);hp=npc->GetCurrentHitPoints(0);serial=attackSerial;
    }
    if(phase==38) send("ldown:640,360\n+2:lup:640,360\n");
    if(phase==39) {
        static int chainProbe=0;static bool earlyGuardProbe=false;
        if(chainProbe==2) {
            // This is a playback-duration deadline, not an input window.
            // Allow the same clip coverage at the selected melee rate.
            if(ticks<int(std::ceil(155.f/PlayerMeleeRate()))) return;
            check(attackSerial==serial+3 && !state.attacking && state.leftHeld,
                "holding_attack_stops_after_three_swings");
            serial=attackSerial;ticks=0;chainProbe=3;
            send("lup:640,360\n+3:ldown:640,360\n+5:lup:640,360\n");
            return;
        }
        if(ticks<90 || state.attacking) return;
        if(chainProbe==3) {
            if(!earlyGuardProbe){
                check(attackSerial==serial+1 && !state.leftHeld && !state.attackRequested,"fresh_press_after_cap_starts_new_chain");
                serial=attackSerial;earlyGuardProbe=true;ticks=0;
                send("ldown:640,360\n+2:lup:640,360\n+12:rdown:640,360\n");return;
            }
            check(earlyGuardSerial==serial+1 && earlyGuardTime>=PlayerHitEnd(swingDirection) &&
                earlyGuardTime<earlyGuardLength-.05f && state.Guarding(),"guard_cancels_recovery_after_damage_window_before_clip_end");
            send("rup:640,360\n");
        } else if(chainProbe==1) {
            check(attackSerial==serial+2 && swingDirection==6 && swingBackLeft && !state.attackRequested,
                "early_tap_ignored_and_followthrough_tap_chains_in_requested_direction");
            serial=attackSerial;ticks=0;chainProbe=2;
            send("ldown:640,360\n");
            return;
        } else {
            CSWSCreature* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(opponentId);
            if(!missSwing) {
                check(attackSerial==serial+1 && hitConsumed && npc->GetCurrentHitPoints(0)<hp,"real_swing_geometry_applies_weapon_damage");
                int after=npc->GetCurrentHitPoints(0);TraceBlade(actor,.4f);TraceBlade(actor,.4f);
                check(npc->GetCurrentHitPoints(0)==after,"same_swing_cannot_apply_damage_twice");
                npc->SetPosition(actor->m_position+Vector(5,0,0),0,0,0);
                hp=npc->GetCurrentHitPoints(0);serial=attackSerial;missSwing=true;ticks=0;
                send("ldown:640,360\n+2:lup:640,360\n");return;
            }
            check(attackSerial==serial+1 && !hitConsumed && npc->GetCurrentHitPoints(0)==hp,"out_of_reach_swing_misses_without_damage");
            chainProbe=1;serial=attackSerial;ticks=0;
            send("ldown:640,360\n+2:lup:640,360\n"
                "+12:keydown:7\n+13:ldown:640,360\n+15:lup:640,360\n+16:keyup:7\n"
                "+48:keydown:22\n+48:keydown:4\n+50:ldown:640,360\n+52:lup:640,360\n"
                "+53:keyup:22\n+53:keyup:4\n");
            return;
        }
    }
    if(phase==40) send("key:41\n");
    if(phase==41) {
        check(K1EngagePlanning(),"raw_escape_suspends_engage");
        if(RigModeEnabled()) {
            check(CheckRigActorOwnership(actor,false,nativeTexture,nativeHead,nativeWeapon,"menu"),"menu_restores_native_body_and_attachments");
            check(CheckRigActorNativePose(actor,"menu"),"menu_restores_native_pose");
        }
        send("key:41\n");
    }
    if(phase==42) {
        check(K1EngagePlanning() && !IsGamePaused(),"raw_escape_return_retains_slow_planning");
        auto* hud=g_pAppManager->m_pClientExoApp->GetInGameGui()->m_pMainInterface;
        hud->m_btnEngage.m_tail.nCurrentEventArg=1;hud->OnFlourishButtonPressed(&hud->m_btnEngage);
    }
    if(phase==43) {
        if(RigModeEnabled()) check(CheckRigActorOwnership(actor,true,nativeTexture,nativeHead,nativeWeapon,"reengaged"),"source_rig_reentry_ownership");
        check(K1EngageActive() && g_pAppManager->m_pClientExoApp->m_pInternal->m_pPlayerControl->m_bEnabled,
            "raw_escape_then_engage_restores_active_controls");
        check(g_pAppManager->m_pServerExoApp->GetWorldTimer()->m_nSnapshotScale==CWorldTimer::SnapshotScaleUnit,
            "engage_restores_full_world_speed");
        previous=actor->m_position;send("keydown:7\nrel:80,0\n+12:keyup:7\n");
    }
    if(phase==44) {
        check(magnitude(actor->m_position-previous)>.15f,"raw_movement_works_after_escape_and_engage");
        Vector forward=g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->GetOrientation().zdir()*-1.f;
        check(forward.x*std::cos(aimYaw)+forward.y*std::sin(aimYaw)>.9f,"raw_mouse_aims_camera_after_escape_and_engage");
        if(!RigModeEnabled()) {
            fprintf(stderr,"K1_ENGAGE_CONTROL_%s\n",ok?"PASS":"FAIL");
            if(std::getenv("K2_HIDDEN")) send("quit:0\n");
            phase=-1;return;
        }
        beforePlanning = loadedBody;
        aimPitch=.35f;
        exitOrbit=EngageOrbit(nativeCameraHeadHeight+engageCameraHeightTrim).zdir();exitOrbit.z=0;exitOrbit.Normalize();
        send("ldown:640,360\n+2:lup:640,360\n+9:key:44\n");
    }
    if(phase==45) {
        Vector orbit=g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->GetPosition()-actor->m_position;
        orbit.z=0;orbit.Normalize();
        fprintf(stderr,"K1_CAMERA_PLANNING_ORBIT dot=%g expected=%g,%g actual=%g,%g\n",dot(orbit,exitOrbit),exitOrbit.x,exitOrbit.y,orbit.x,orbit.y);
        check(dot(orbit,exitOrbit)>.98f,"planning_keeps_pitched_engage_orbit_side");
        const bool savedEdgeGate=nativeEdgeAwaitInterior;
        nativeEdgeAwaitInterior=true;
        check(K1EngageSuppressEdgeCamera(0,0,false) && K1EngageSuppressEdgeCamera(-1,-1,true),
            "unavailable_pointer_does_not_rearm_edge_rotation");
        check(!K1EngageSuppressEdgeCamera(0,0,true) && !K1EngageSuppressEdgeCamera(-1,-1,true),
            "valid_interior_pointer_rearms_normal_edge_rotation");
        nativeEdgeAwaitInterior=savedEdgeGate;
        check(K1EngagePlanning() && !IsGamePaused() &&
            g_pAppManager->m_pServerExoApp->GetWorldTimer()->m_nSnapshotScale==1,
            "raw_space_enters_slow_planning");
        float length = 0.f;
        check(retainedPlanningPose && loadedBody && loadedBody == beforePlanning && state.attacking &&
            currentOverlay && loadedBody->GetAnimation(const_cast<char*>(currentOverlay), &planningSwingTime, &length) &&
            planningSwingTime > .06f && planningSwingTime < length && !lastPose.empty(),
            "space_retains_live_swing_and_pose");
        planningYaw = Yaw(g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->GetOrientation());
        send("keydown:4\n+22:keyup:4\n");
    }
    if(phase==46) {
        const float yawAfterA = Yaw(g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->GetOrientation());
        // Native Yaw returns degrees; WrapMotionAngle and this threshold use radians.
        planningLeft = WrapMotionAngle((yawAfterA - planningYaw)*.01745329252f);
        planningYaw = yawAfterA;
        float time = 0.f, length = 0.f;
        check(loadedBody && currentOverlay &&
            loadedBody->GetAnimation(const_cast<char*>(currentOverlay), &time, &length) && time > planningSwingTime,
            "retained_swing_advances_in_slow_motion");
        send("keydown:7\n+22:keyup:7\n");
    }
    if(phase==47) {
        const float yawAfterD = Yaw(g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->GetOrientation());
        const float planningRight = WrapMotionAngle((yawAfterD - planningYaw)*.01745329252f);
        fprintf(stderr,"K1_ENGAGE_PLANNING_CAMERA a=%g d=%g\n",planningLeft,planningRight);
        check(std::fabs(planningLeft) > .05f && std::fabs(planningRight) > .05f && planningLeft * planningRight < 0.f,
            "planning_ad_rotates_camera_both_directions");
        CSWSCreature* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(opponentId);
        CSWSCreature* player=actor->GetServerCreature();
        npc->SetPosition(player->m_position + Vector(3.f,0,0), 1, 1, 0);
        auto& menu=g_pAppManager->m_pClientExoApp->GetInGameGui()->m_pMainInterface->m_targetActionMenu;
        menu.PopulateMenus(actor, 0, npc->GetClientCreature());
        bool weaponAction=false;
        for (const auto& column : menu.m_actions) {
            for (int i=0; i<column.m_count; ++i) {
                const auto callback=column.m_pArray[i].m_pfnAction;
                weaponAction |= callback==&CSWCCreature::ActionMenuAttack ||
                    callback==&CSWCCreature::ActionMenuUseAttackFeat;
            }
        }
        check(!weaponAction,"planning_filters_weapon_actions");
        casterSlot = 0;
        while (casterSlot < player->m_pStats->m_numClasses &&
               !player->m_pStats->GetSpellUsesLeft(FORCE_POWER_CHOKE, casterSlot, 0, 0)) ++casterSlot;
        castTargetHp = npc->GetCurrentHitPoints(0);
        check(casterSlot < player->m_pStats->m_numClasses &&
            player->AddCastSpellActions(FORCE_POWER_CHOKE, casterSlot, 0, 0, 0,
            npc->m_position, opponentId, 1, 0, 1, 0, 0, 0, -1, 0xff, 0) != 0,"planning_queues_choke");
    }
    if(phase==48) {
        CSWSCreature* player=actor->GetServerCreature();
        check(K1EngagePlanning() && NativeUtilityQueued(player) && !nativeActionBody &&
            !player->m_pCombatRound->m_bSpellCastRound && state.attacking,
            "planning_holds_choke_while_swing_advances");
        auto* hud=g_pAppManager->m_pClientExoApp->GetInGameGui()->m_pMainInterface;
        K1EngageHud(hud);
        check(hud->m_btnClearAll.m_tail.bVisible && Spotted(),"clear_queue_visible_while_spotted");
        hud->m_btnClearAll.m_tail.nCurrentEventArg=1;
        hud->OnClearAllButtonPressed(&hud->m_btnClearAll);
        check(K1EngagePlanning() && !NativeUtilityQueued(player) && state.attacking,
            "clear_queue_cancels_choke_without_disengaging");
        CSWSCreature* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(opponentId);
        check(player->AddCastSpellActions(FORCE_POWER_CHOKE, casterSlot, 0, 0, 0,
            npc->m_position, opponentId, 1, 0, 1, 0, 0, 0, -1, 0xff, 0) != 0,"queue_choke_again");
        send("key:44\n");
    }
    if(phase==49) {
        if(ticks==30)
            check(K1EngageActive() && state.attacking && !nativeActionBody,"space_engages_and_finishes_retained_swing");
        const bool nativeCast=nativeActionBody && !state.attacking &&
            !IsSourceBody(actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1)) &&
            actor->GetServerCreature()->m_pCombatRound->m_bSpellCastRound;
        if (!nativeCast && ticks < 105) return;
        check(nativeCast,"swing_finishes_then_choke_uses_native_body");
        castPosition=actor->m_position;
        castDoneTick=0;
        send("keydown:7\n");
    }
    if(phase==50) {
        CSWSCreature* player=actor->GetServerCreature();
        const bool castPending=NativeUtilityQueued(player);
        if (castPending && ticks < 240 && magnitude(actor->m_position-castPosition)<.02f) return;
        if (!castPending && !castDoneTick) castDoneTick=ticks;
        const float moved=magnitude(actor->m_position-castPosition);
        if (!castPending && moved<.02f && ticks-castDoneTick<6) return;
        fprintf(stderr,"K1_ENGAGE_CAST_MOVEMENT pending=%d moved=%g frames_after_cast=%d spell_round=%d\n",
            castPending,moved,castDoneTick?ticks-castDoneTick:-1,player->m_pCombatRound->m_bSpellCastRound);
        check(!castPending && !nativeActionBody && moved>=.02f && ticks-castDoneTick<=6,
            "held_movement_resumes_after_native_cast");
        check(utilityBlendsCompleted[0]>0,"saber_pose_blends_into_native_cast");
        serial=attackSerial;
        send("keyup:7\n+1:ldown:640,360\n+3:lup:640,360\n");
        CSWSCreature* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(opponentId);
        const int castLevel=player->m_pStats->GetClassLevel(player->m_nCastClassIndex, 1);
        fprintf(stderr,"K1_ENGAGE_ARENA_CHOKE caster_level=%d save_dc=%d target_hp_delta=%d\n",
            castLevel,player->CalculateSpellSaveDC(FORCE_POWER_CHOKE),castTargetHp-npc->GetCurrentHitPoints(0));
        check(castLevel>=12,"choke_uses_boosted_jedi_class_level");
    }
    if(phase==51) {
        check(utilityBlendsCompleted[1]>0 && attackSerial==serial+1,
            "native_cast_blends_back_while_followup_saber_starts");
        exitOrbit=g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->GetPosition()-actor->m_position;
        exitOrbit.z=0;exitOrbit.Normalize();
        CSWSCreature* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(opponentId);
        float time=0,length=0;auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        check(state.attacking&&body&&currentOverlay&&body->GetAnimation(const_cast<char*>(currentOverlay),&time,&length)&&time<length,
            "victory_fixture_kills_during_swing");
        victoryDeadline=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime()+static_cast<long long>((length-time)/PlayerMeleeRate()*1000000.f);
        npc->ClearAllActions(1);npc->SetCurrentHitPoints(0);
    }
    if(phase==52) {
        if(state.mode!=K1EngageState::Mode::Exploration&&ticks<240)return;
        check(g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime()+40000>=victoryDeadline,
            "victory_finishes_swing_before_controller_restore");
        check(state.mode==K1EngageState::Mode::Exploration && !K1EngageEngaged(),"victory_restores_exploration");
        Vector orbit=g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->GetPosition()-actor->m_position;
        orbit.z=0;orbit.Normalize();
        fprintf(stderr,"K1_CAMERA_VICTORY_ORBIT dot=%g expected=%g,%g actual=%g,%g\n",dot(orbit,exitOrbit),exitOrbit.x,exitOrbit.y,orbit.x,orbit.y);
        check(dot(orbit,exitOrbit)>.98f,"victory_keeps_last_combat_orbit_side");
        check(g_pAppManager->m_pServerExoApp->GetWorldTimer()->m_nSnapshotScale==CWorldTimer::SnapshotScaleUnit,
            "victory_restores_full_world_speed");
        check(CheckRigActorOwnership(actor,false,nativeTexture,nativeHead,nativeWeapon,"victory"),"victory_restores_native_body_and_attachments");
        check(CheckRigActorNativePose(actor,"victory"),"victory_restores_native_pose");
        fprintf(stderr,"K1_ENGAGE_CONTROL_%s\n",ok?"PASS":"FAIL");
        if(std::getenv("K2_HIDDEN")) send("quit:0\n");
        phase=-1;return;
    }
    ++phase;ticks=0;
}
unsigned long long arenaSaberIds[2]={OBJECT_INVALID,OBJECT_INVALID};
unsigned long long arenaOffhandSaberId=OBJECT_INVALID;
bool EquipFixtureWeapon(CSWSCreature* owner, const char* resource,unsigned int equipSlot=CSWInventory::SLOT_RIGHTWEAPON) {
    CSWSItem* item=new CSWSItem(OBJECT_INVALID);
    if(!item->LoadFromTemplate(CResRef(resource))) {delete item;return false;}
    if(owner==ServerActor()) {
        auto* base=g_pRules->GetBaseItem(item->m_baseItem);int granted=0;
        for(unsigned char i=0;i<base->m_nRequiredFeats;++i) {
            unsigned short feat=base->GetRequiredFeat(i);
            if(!owner->m_pStats->HasFeat(feat)) {owner->m_pStats->AddFeat(feat);++granted;}
            if(auto* client=owner->GetClientCreature()) if(client->m_pStats && !client->m_pStats->HasFeat(feat)) client->m_pStats->m_lstFeats.Add(feat);
        }
        unsigned int slot=equipSlot;
        bool eligible=owner->CanEquipItem(item,&slot,0,0,0)!=CSWSCreature::CAN_EQUIP_NO;
        fprintf(stderr,"%s_EQUIP_%s baseitem=%d dice=%d die=%d required_feats=%d granted=%d strength=%d\n",base->m_nItemType==ITEM_TYPE_DOUBLE_LIGHTSABER?"K1_STAFF":"K1_FIXTURE",eligible?"PASS":"FAIL",item->m_baseItem,base->m_nNumDice,base->m_nDieToRoll,base->m_nRequiredFeats,granted,owner->m_pStats->GetSTRStat());
        if(!eligible) {delete item;return false;}
    }
    item->m_flags &= ~4;
    if(!owner->AcquireItem(&item,OBJECT_INVALID,OBJECT_INVALID,1)) return false;
    // RunEquip transfers the incoming item out of the backpack and returns the
    // outgoing weapon to it. Low-level EquipItem only updates equipment slots:
    // leaving the same item in both containers lets a later menu swap merge it
    // with itself, delete it and leave a dangling repository ID.
    return owner->RunEquip(item->m_objectId,equipSlot,1)!=0;
}
bool EquipFixtureSaber(CSWSCreature* owner) {
    return EquipFixtureWeapon(owner, "g_w_lghtsbr01");
}
bool AddArenaSwords(CSWSCreature* owner) {
    // Ordinary native inventory items: use the equipment menu to choose them.
    for(const char* name:{"g_w_lngswrd01","g_w_lngswrd01","g_w_shortswrd01","g_w_vbroswrd01",
                         "g_w_vbroshort01","g_w_dblswrd001","g_w_vbrdblswd01"}) {
        auto* item=new CSWSItem(OBJECT_INVALID);
        if(!item->LoadFromTemplate(CResRef(name))){delete item;return false;}
        auto* base=g_pRules->GetBaseItem(item->m_baseItem);
        if(base)for(unsigned char i=0;i<base->m_nRequiredFeats;++i) {
            const auto feat=base->GetRequiredFeat(i);
            if(!owner->m_pStats->HasFeat(feat))owner->m_pStats->AddFeat(feat);
            if(auto* client=owner->GetClientCreature())if(client->m_pStats && !client->m_pStats->HasFeat(feat))client->m_pStats->m_lstFeats.Add(feat);
        }
        item->m_flags&=~4;
        if(!owner->AcquireItem(&item,OBJECT_INVALID,OBJECT_INVALID,1)){delete item;return false;}
    }
    fprintf(stderr,"K1_SWORD_INVENTORY_READY native_items=7 single_dual_double=1\n");return true;
}
bool CheckArenaSaberInventory(CSWSCreature* owner,const char* stage) {
    auto* repository=owner->GetItemRepository(1);
    bool ok=true;
    std::vector<unsigned long long> ids(std::begin(arenaSaberIds),std::end(arenaSaberIds));
    if(std::getenv("K1_ENGAGE_SANDBOX") && arenaOffhandSaberId!=OBJECT_INVALID)ids.push_back(arenaOffhandSaberId);
    for(auto id:ids) {
        auto* item=g_pAppManager->m_pServerExoApp->GetItemByGameObjectID(id);
        int backpack=0;
        for(unsigned int i=0;i<repository->ItemListGetCount();++i)
            backpack+=repository->ItemListGetItemObjectID(i)==id;
        const int equipped=item?owner->m_pInventory->GetItemInInventory(item):0;
        ok=ok && item && item->m_possessorId==owner->m_objectId && item->m_stackCount==1 && backpack+equipped==1;
    }
    fprintf(stderr,"K1_SABER_INVENTORY_%s stage=%s sabers=2 unique_ownership=1\n",ok?"PASS":"FAIL",stage);
    return ok;
}
bool GrantSandboxEquipmentPrerequisites(CSWSCreature* player) {
    auto* client=player?player->GetClientCreature():nullptr;
    if(!client || !client->m_pStats)return false;
    // Grant only to the isolated sandbox player, never to spawned native NPCs.
    // Universal proficiency is K1's existing class/alignment/feat override;
    // ordinary category feats also appear correctly in the character sheet.
    const unsigned short feats[]={FEAT_ARMOUR_PROF_LIGHT,FEAT_ARMOUR_PROF_MEDIUM,
        FEAT_ARMOUR_PROF_HEAVY,FEAT_IMPLANT_LEVEL_1,FEAT_IMPLANT_LEVEL_2,
        FEAT_IMPLANT_LEVEL_3,FEAT_JEDI_DEFENSE,FEAT_PROFICIENCY_ALL};
    bool ok=true;
    for(auto feat:feats) {
        if(!player->m_pStats->HasFeat(feat))player->m_pStats->AddFeat(feat);
        if(!client->m_pStats->HasFeat(feat))client->m_pStats->m_lstFeats.Add(feat);
        ok=ok && player->m_pStats->HasFeat(feat) && client->m_pStats->HasFeat(feat);
    }
    fprintf(stderr,"K1_SANDBOX_EQUIPMENT_PREREQS_%s feats=8 server_client=1\n",ok?"PASS":"FAIL");
    return ok;
}
bool GrantArenaLoadout(CSWSCreature* player) {
    CSWCCreature* client = player->GetClientCreature();
    if (!client || !client->m_pStats || player->m_pStats->m_numClasses == 0) return false;
    auto* stats = player->m_pStats;
    auto* clientStats = client->m_pStats;
    unsigned char slot = 0;
    while (slot < stats->m_numClasses &&
           (stats->GetClass(slot) < CLASS_TYPE_JEDIGUARDIAN ||
            stats->GetClass(slot) > CLASS_TYPE_JEDISENTINEL)) ++slot;
    if (slot == stats->m_numClasses) {
        // Keep the arena character's levels while making the native Force menu available.
        slot = stats->m_numClasses - 1;
        stats->SetClass(slot, CLASS_TYPE_JEDICONSULAR);
        clientStats->SetClass(slot, CLASS_TYPE_JEDICONSULAR);
    }
    int powers = 0;
    for (int id = 0; id < g_pRules->m_pSpellArray->m_nCount; ++id) {
        CSWSpell* spell = g_pRules->GetSpell(id);
        // Match K1's native level-up eligibility by spell user type.
        if (!spell || (spell->m_nUserType != CSWSpell::USERTYPE_FORCEPOWER &&
                       spell->m_nUserType != CSWSpell::USERTYPE_MASTER)) continue;
        stats->AddKnownSpell(slot, id);
        clientStats->AddKnownSpell(slot, id);
        ++powers;
    }
    int baseForce = 0;
    for (unsigned char i = 0; i < stats->GetLevel(0); ++i) {
        if (auto* level = stats->GetLevelStats(i)) {
            level->m_nForcePointGain = 200;
            baseForce += 200;
        }
    }
    stats->m_nBaseForcePoints = static_cast<unsigned short>(baseForce);
    constexpr unsigned char testJediLevel = 12;
    constexpr unsigned char testWisdom = 20;
    constexpr unsigned char testCharisma = 18;
    stats->SetWISBase(std::max(testWisdom, stats->m_nWISBase));
    stats->SetCHABase(std::max(testCharisma, stats->m_nCHABase));
    // Add real level records so K1's HP and Force calculations can walk them.
    while (stats->GetClassLevel(slot, 0) < testJediLevel && stats->GetLevel(0) < 20) {
        auto* level = new CSWLevelStats();
        level->m_nClass = stats->GetClass(slot);
        level->m_nSkillPoints = stats->m_nSkillPoints;
        stats->LevelUp(level, 1);
    }
    const unsigned char jediLevel = stats->GetClassLevel(slot, 0);
    stats->SetExperience(std::max(static_cast<unsigned int>(stats->m_nExperience),
        static_cast<unsigned int>(g_pRules->m_aExpTable[stats->GetLevel(0) - 1])), 0);
    clientStats->SetClassLevel(slot, jediLevel);
    clientStats->m_nExperience = stats->m_nExperience;
    clientStats->m_abScoresBase[ABILITY_WISDOM] = stats->m_nWISBase;
    clientStats->m_abScoresBase[ABILITY_CHARISMA] = stats->m_nCHABase;
    clientStats->m_abScores[ABILITY_WISDOM] = stats->GetWISStat();
    clientStats->m_abScores[ABILITY_CHARISMA] = stats->GetCHAStat();
    stats->m_nCurrentForcePoints = player->GetMaxForcePoints();
    stats->m_nTemporaryForcePoints = 10000;
    stats->SetSkillRank(SKILL_DEMOLITIONS, 20);
    clientStats->m_pSkillRanksBase[SKILL_DEMOLITIONS] = 20;
    clientStats->UpdateAbilityModifiedSkillRanks();

    const char* templates[] = {
        "g_w_adhsvgren001", "g_w_cryobgren001", "g_w_firegren001", "g_w_flashgren001",
        "g_w_fraggren01", "g_w_iongren01", "g_w_poisngren01", "g_w_sonicgren01",
        "g_w_stungren01", "g_w_thermldet01",
        "g_i_trapkit001", "g_i_trapkit002", "g_i_trapkit003", "g_i_trapkit004",
        "g_i_trapkit005", "g_i_trapkit006", "g_i_trapkit007", "g_i_trapkit008",
        "g_i_trapkit009", "g_i_trapkit010", "g_i_trapkit011", "g_i_trapkit012",
        "g_i_trapkit01", "g_i_trapkit02", "g_i_trapkit03", "g_i_trapkit04",
    };
    int granted = 0;
    for (const char* name : templates) {
        CSWSItem* item = new CSWSItem(OBJECT_INVALID);
        if (!item->LoadFromTemplate(CResRef(name))) {
            delete item;
            fprintf(stderr, "K1_ENGAGE_ARENA_ITEM_MISSING template=%s\n", name);
            continue;
        }
        item->m_flags &= ~4;
        item->m_stackCount = std::max(1, std::min(20, int(g_pRules->GetBaseItem(item->m_baseItem)->m_nStacking)));
        if (player->AcquireItem(&item, OBJECT_INVALID, OBJECT_INVALID, 1)) ++granted;
        else fprintf(stderr, "K1_ENGAGE_ARENA_ITEM_REJECTED template=%s\n", name);
    }
    CExoArrayList<CSWGuiInterfaceAction> hostilePowers;
    clientStats->GetMenuInfo(hostilePowers, 1, stats->m_nCurrentForcePoints + stats->m_nTemporaryForcePoints,
                            0, player->GetForceItemMask(), 9.f);
    bool chokeAvailable = false;
    for (int i = 0; i < hostilePowers.m_count; ++i) {
        const auto& action = hostilePowers.m_pArray[i];
        if (action.m_nActionId == (FORCE_POWER_CHOKE | CSWGuiInterfaceAction::ACTION_ID_SPELL))
            chokeAvailable = (action.m_nFlags & 1) != 0;
    }
    const bool ok = powers > 0 && chokeAvailable && jediLevel >= testJediLevel &&
        stats->GetWISStat() >= testWisdom && stats->GetCHAStat() >= testCharisma &&
        stats->GetNumberKnownSpells(slot, 0) >= powers &&
        clientStats->GetNumberKnownSpells(slot, 0) >= powers &&
        granted == sizeof(templates) / sizeof(templates[0]) && stats->m_nCurrentForcePoints > 0;
    fprintf(stderr, "K1_ENGAGE_ARENA_LOADOUT_%s powers=%d items=%d force=%d temporary=%d choke=%d jedi_level=%d wisdom=%d charisma=%d\n",
        ok ? "PASS" : "FAIL", powers, granted, stats->m_nCurrentForcePoints,
        stats->m_nTemporaryForcePoints, chokeAvailable, jediLevel, stats->GetWISStat(), stats->GetCHAStat());
    return ok;
}
CSWSCreature* SpawnArenaOpponent(CSWSCreature* actor,const Vector& spawn,bool gunner,bool rifle,bool attack=true,const char* fixture=nullptr) {
    auto* npc=new CSWSCreature(OBJECT_INVALID,0);
    const char* selected=std::getenv("K1_ENGAGE_NPC_TYPE");
    const K1NpcType* type=&k1DarkJediTypes[0];
    for(const auto& candidate:k1DarkJediTypes) if(selected && !strcmp(selected,candidate.id)) type=&candidate;
    bool dark=!fixture && ArenaDarkJedi() && !gunner;
    if(fixture)for(const auto& candidate:k1DarkJediTypes)if(!strcmp(fixture,candidate.creature)) {
        dark=true;type=&candidate;break;
    }
    if(dark && !fixture) fixture=type->creature;
    if(!npc->LoadFromTemplate(CResRef(fixture?fixture:(dark?"k1darktest":"k1arenafoe")),0) ||
        (!dark && !EquipFixtureWeapon(npc,gunner?(rifle?"g_w_blstrrfl001":"g_w_blstrpstl001"):"g_w_lghtsbr01"))) {
        delete npc;return nullptr;
    }
    if(dark && !strcmp(fixture,"k1darkstaff") && !EquipFixtureWeapon(npc,"g_w_drkjdisbr002")) {delete npc;return nullptr;}
    if(dark && !strcmp(fixture,"k1darkdual") && !EquipFixtureWeapon(npc,"g_w_lghtsbr02",CSWInventory::SLOT_LEFTWEAPON)) {delete npc;return nullptr;}
    if(dark) {
        fprintf(stderr,"K1_NPC_TYPE_SPAWN id=%s actor=%llu template=%s tag=%s\n",NpcType(npc).id,npc->m_objectId,fixture,npc->m_sTag.CStr());
        arenaForce={};arenaForce.actor=npc->m_objectId;
        arenaForce.next=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime()+3000000;
        fprintf(stderr,"K1_DARK_JEDI_SPAWN actor=%llu appearance=%d hp=%d force=%d level=%d\n",
            npc->m_objectId,npc->m_appearance.m_headId,npc->GetCurrentHitPoints(0),npc->m_pStats->m_nCurrentForcePoints,npc->m_pStats->GetLevel(0));
    }
    Vector facing=actor->m_position-spawn;facing.z=0;
    const float length=magnitude(facing);
    npc->SetOrientation(length>.001f?facing*(1.f/length):Vector(1,0,0));
    npc->AddToArea(static_cast<CSWSArea*>(actor->GetArea()),spawn.x,spawn.y,spawn.z,1,1);
    fprintf(stderr,"K1_NATIVE_MOVEMENT player_walk=%g player_run=%g npc_walk=%g npc_run=%g units=m_per_s\n",
        NativeMoveSpeed(actor,true),NativeMoveSpeed(actor,false),NativeMoveSpeed(npc,true),NativeMoveSpeed(npc,false));
    fprintf(stderr,"K1_ENGAGE_MOVEMENT npc_walk=%g npc_run=%g units=m_per_s policy=native_player_base\n",
        EngageMoveSpeed(npc,true),EngageMoveSpeed(npc,false));
    if(attack) {
        npc->AddToVisibleList(actor->m_objectId,1,0,0,0);
        npc->AddAttackActions(actor->m_objectId,0,1,0,0,0,0,1,SERVER_ANIMATION_ATTACK,1500,1,0,0,4,0);
    }
    return npc;
}
void SandboxRegister(CSWSCreature* actor,bool friendly) {
    actor->m_bPlot=0;actor->m_bMin1HP=0;actor->m_bDestroyable=1;
    if(!sandboxActors.emplace(actor->m_objectId,SandboxActor{friendly,int(actor->m_bCommandable),int(actor->m_bPassiveAttackBehaviour)}).second)return;
    auto& force=sandboxForces[actor->m_objectId];force={};force.actor=actor->m_objectId;
    force.next=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime()+3000000;
}
void SandboxSetAi(bool enabled) {
    if(sandboxAi==enabled)return;sandboxAi=enabled;
    if(!enabled && K1EngageEngaged())Plan();
    auto* server=g_pAppManager->m_pServerExoApp;
    for(auto& entry:sandboxActors)if(auto* actor=server->GetCreatureByGameObjectID(entry.first)) {
        if(actor->GetDead())continue;
        if(!enabled) {
            auto owned=npcPlayback.find(entry.first);
            if(owned!=npcPlayback.end()){if(owned->second.requested)SwapNpcRig(owned->second,false);npcPlayback.erase(owned);}
            actor->ClearAllActions(1);actor->m_pCombatRound->EndCombatRound(0);actor->SetCombatState(0,1);
            actor->m_bPassiveAttackBehaviour=1;actor->m_bCommandable=0;actor->SetAnimation(SERVER_ANIMATION_PAUSE);
        } else {
            actor->m_bCommandable=entry.second.commandable;actor->m_bPassiveAttackBehaviour=entry.second.passive;
        }
    }
    if(!enabled && K1EngagePlanning() && !Spotted())K1EngageTryDisengage();
    fprintf(stderr,"K1_SANDBOX_AI enabled=%d actors=%zu\n",enabled,sandboxActors.size());
}
void SandboxSetTargetPlayer(bool enabled) {
    if(sandboxTargetPlayer==enabled)return;
    sandboxTargetPlayer=enabled;
    // Discard already queued native attacks/powers and Engage playback immediately.
    // Native faction reputation queries below enforce the same policy for scripts.
    if(K1EngageEngaged())Plan();
    auto* server=g_pAppManager->m_pServerExoApp;
    for(auto& entry:sandboxActors)if(auto* actor=server->GetCreatureByGameObjectID(entry.first)) {
        auto owned=npcPlayback.find(entry.first);
        if(owned!=npcPlayback.end()){if(owned->second.requested)SwapNpcRig(owned->second,false);npcPlayback.erase(owned);}
        actor->ClearAllActions(1);actor->m_pCombatRound->EndCombatRound(0);actor->SetCombatState(0,1);
    }
    fprintf(stderr,"K1_SANDBOX_TARGET_PLAYER enabled=%d actors=%zu\n",enabled,sandboxActors.size());
}
void SandboxCosmeticInfo(CCAppearanceInfo& appearance,CSWSCreature* source) {
    auto& a=appearance.core;auto* s=source->m_pStats;
    a.m_nAppearanceId=source->m_appearance.m_headId;a.m_nGender=s->m_nGender;a.m_nPhenotype=s->m_nPhenotype;
    a.m_nHeadNumber=s->m_nAppearanceHead;a.m_nSkinColor=s->m_nColorSkin;a.m_nHairColor=s->m_nColorHair;
    a.m_nTattooColor1=s->m_nColorTattoo1;a.m_nTattooColor2=s->m_nColorTattoo2;a.m_bUseBackupHead=s->m_nUseBackupHead;
    a.m_bArmorEquipped=0;a.m_bTypeVariant=0;a.m_bTypeNumber=1;
    if(auto* armor=source->m_pInventory->GetItemInSlot(CSWInventory::SLOT_BODY)){a.m_bArmorEquipped=1;a.m_bTypeVariant=armor->m_nBodyVariation;a.m_bTypeNumber=armor->m_nTextureVar;}
}
void SandboxSaberColor(CSWSCreature* actor,int color) {
    if(color<1 || color>5)return;
    for(int slot:{CSWInventory::SLOT_RIGHTWEAPON,CSWInventory::SLOT_LEFTWEAPON})if(auto* item=actor->m_pInventory->GetItemInSlot(slot))
        if(SaberType(item->m_baseItem))item->m_nModelVariation=static_cast<unsigned char>(color);
}
CSWSCreature* SandboxSpawn(int stance,bool friendly,CExoString& message,int group,int variant,bool hostile,const SandboxNativeTemplate* appearance,int color,int saberStance) {
    auto* player=ServerActor();auto* server=g_pAppManager->m_pServerExoApp;
    if(!sandboxReady || !player || player->GetDead() || !player->GetArea() || (!server->GetFactionManager() || !server->GetFactionManager()->GetFaction(2)) || stance<0 || stance>=int(sizeof(k1DarkJediTypes)/sizeof(k1DarkJediTypes[0]))){message=CExoString("Cannot spawn here.");return nullptr;}
    Vector forward=player->m_orientation;forward.z=0;if(magnitude(forward)<.01f){message=CExoString("Cannot determine facing.");return nullptr;}forward=normalize(forward);
    JkaWorld world(player);auto* area=static_cast<CSWSArea*>(player->GetArea());Vector spawn;bool clear=false;
    for(float distance:{3.f,3.5f,4.f}) {
        spawn=player->m_position+forward*distance;
        clear=area->PositionWalkable(spawn) && world.trace(JkaPoint(spawn),JkaPoint(spawn+Vector(.002f,0,0)),true).clear && world.trace(JkaPoint(player->m_position),JkaPoint(spawn),false).clear;
        if(clear)for(int i=0;i<area->m_aObjectsByX.m_count;++i){auto* other=server->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);if(other && !other->GetDead() && magnitude(other->m_position-spawn)<1.2f){clear=false;break;}}
        if(clear)break;
    }
    if(!clear){message=CExoString("Not enough room ahead. Move to a clear spot.");fprintf(stderr,"K1_SANDBOX_SPAWN_REJECT reason=space\n");return nullptr;}
    const SandboxNativeTemplate* native=nullptr;
    if(group>=0) {
        if(group>=int(sandboxNativeGroups.size()) || variant<0 || variant>=int(sandboxNativeGroups[group].variants.size())) {
            message=CExoString("NPC selection unavailable.");return nullptr;
        }
        native=&sandboxNativeGroups[group].variants[variant];
    }
    CSWSCreature* actor=nullptr;
    if(native) {
        // Native templates must never pass through the synthetic arena loadout.
        actor=new CSWSCreature(OBJECT_INVALID,0);
        if(!actor->LoadFromTemplate(CResRef(native->creature.c_str()),0)){delete actor;actor=nullptr;}
        if(actor) {
            ConfigureNpcPoise(actor);
            sandboxNativeActors.insert(actor->m_objectId);
            sandboxStances[actor->m_objectId]=saberStance<0?NpcCombatPolicy(actor).stance:saberStance;
            SandboxSaberColor(actor,color);
            if(appearance) {
                std::unique_ptr<CSWSCreature> look(new CSWSCreature(OBJECT_INVALID,0));
                if(!look->LoadFromTemplate(CResRef(appearance->creature.c_str()),0)){sandboxNativeActors.erase(actor->m_objectId);sandboxStances.erase(actor->m_objectId);delete actor;message=CExoString("Appearance resources unavailable.");return nullptr;}
                CCAppearanceInfo info={};info.core.m_race=0xff;info.core.m_oidHeadItem=OBJECT_INVALID;
                SandboxCosmeticInfo(info,look.get());sandboxAppearances[actor->m_objectId]=info;
                actor->m_appearance.m_headId=info.core.m_nAppearanceId;
                actor->m_pStats->m_nAppearanceType=info.core.m_nAppearanceId;
                actor->UpdateAppearanceDependantInfo(1);
            }
            actor->SetOrientation(-forward);
            actor->AddToArea(area,spawn.x,spawn.y,spawn.z,1,1);
            JkaWorld nativeWorld(actor);
            auto prepared=npcNavigators.find(area->m_objectId);
            const auto* path=actor->m_pPathfindInfo;
            const bool fits=nativeWorld.trace(JkaPoint(spawn),JkaPoint(spawn+Vector(.002f,0,0)),true).clear;
            const bool needsGraph=prepared==npcNavigators.end() || prepared->second.area!=area ||
                prepared->second.space+.00001f<path->m_fPersonalSpace || prepared->second.height+.00001f<path->m_fHeight;
            auto graph=fits && needsGraph?BuildNavigation(nativeWorld):nullptr;
            if(!fits || (needsGraph && !graph)) {
                sandboxNativeActors.erase(actor->m_objectId);sandboxStances.erase(actor->m_objectId);sandboxAppearances.erase(actor->m_objectId);actor->RemoveFromArea(0);delete actor;
                message=CExoString("NPC footprint cannot navigate this location.");return nullptr;
            }
            if(graph) {
                PreparedNavigation next;next.graph=graph;next.area=area;
                next.space=path->m_fPersonalSpace;next.height=path->m_fHeight;
                next.generation=++navigationGeneration;npcNavigators[area->m_objectId]=next;
                fprintf(stderr,"K1_NATIVE_NPC_NAV_PREPARED actor=%llu space=%g height=%g phase=spawn\n",actor->m_objectId,next.space,next.height);
            }
            fprintf(stderr,"K1_SANDBOX_NATIVE_SPAWN actor=%llu source=%s/%s appearance=%d hp=%d level=%d force=%d\n",
                actor->m_objectId,native->module.c_str(),native->source.c_str(),actor->m_appearance.m_headId,
                actor->GetCurrentHitPoints(0),actor->m_pStats->GetLevel(0),actor->m_pStats->m_nCurrentForcePoints);
        }
    } else actor=SpawnArenaOpponent(player,spawn,false,false,false,k1DarkJediTypes[stance].creature);
    if(!actor){message=CExoString("NPC resources unavailable.");return nullptr;}
    if(friendly)server->GetFactionManager()->GetFaction(2)->AddMember(actor->m_objectId,0);
    else if(native && hostile && server->GetFactionManager()->GetFaction(1))server->GetFactionManager()->GetFaction(1)->AddMember(actor->m_objectId,0);
    SandboxRegister(actor,friendly);lastSandboxSpawn=actor->m_objectId;
    // Seed mutual native visibility, retaining native faction reputation for
    // both controller selection and ordinary KotOR targeting/powers.
    actor->AddToVisibleList(player->m_objectId,1,0,0,0);player->AddToVisibleList(actor->m_objectId,1,0,0,0);
    for(const auto& entry:sandboxActors)if(auto* other=server->GetCreatureByGameObjectID(entry.first))if(other!=actor && !other->GetDead()){actor->AddToVisibleList(other->m_objectId,1,0,0,0);other->AddToVisibleList(actor->m_objectId,1,0,0,0);}
    if(!sandboxAi){actor->m_bCommandable=0;actor->m_bPassiveAttackBehaviour=1;actor->ClearAllActions(1);actor->SetCombatState(0,1);actor->SetAnimation(SERVER_ANIMATION_PAUSE);}
    message=CExoString(native?(friendly?"Spawned NPC (friendly).":hostile?"Spawned NPC (hostile).":"Spawned NPC (native faction)."):friendly?"Spawned friendly Dark Jedi.":"Spawned hostile Dark Jedi.");
    fprintf(stderr,"K1_SANDBOX_SPAWN actor=%llu type=%s friendly=%d ai=%d reputation=%d distance=%g facing=%g\n",actor->m_objectId,native?native->creature.c_str():NpcType(actor).id,friendly,sandboxAi,actor->GetReputation(player->m_objectId,0),magnitude(spawn-player->m_position),dot(actor->m_orientation,normalize(player->m_position-spawn)));
    return actor;
}
#include "k1sandboxpanel.h"
void SandboxToggle() {
    auto* app=g_pAppManager->m_pClientExoApp;auto* gui=app->GetInGameGui();
    if(sandboxPanel){sandboxPanel->SetToBeDeleted(1);return;}
    if(!sandboxReady || !K1EngageArenaLoadoutActive() || !gui || gui->m_nGlobalDialogState || gui->m_bInGameMenuShown || gui->m_pGuiManager->m_modalPanels.m_count || app->m_pInternal->m_inputClass!=INPUTCLASS_GAME)return;
    sandboxSession=true;
    auto* player=ServerActor();auto* area=static_cast<CSWSArea*>(player->GetArea());
    for(int i=0;i<area->m_aObjectsByX.m_count;++i){auto* actor=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);if(actor && actor!=player && !actor->GetDead() && (!strcmp(actor->m_sTag.CStr(),k1DarkJediTypes[0].tag) || !strcmp(actor->m_sTag.CStr(),k1DarkJediTypes[1].tag)))SandboxRegister(actor,actor->GetReputation(player->m_objectId,0)>89);}
    LoadSandboxNativeCatalog();
    auto* panel=new K1SandboxPanel(gui->m_pGuiManager);sandboxPanel=panel;
    if(K1EngageEngaged())Plan(true);
    state.ClearInputs();keyW=keyS=keyA=keyD=keyShift=false;movement.Clear();g_bMouseCameraRotateToggle=0;g_bLeftMouseDown=0;
    sandboxMenuOpen=true;app->SetPausedByCombat(1,0,0);gui->m_pGuiManager->AddPanel(panel,1,1);
    fprintf(stderr,"K1_SANDBOX_MENU open=1\n");
}
void SandboxProbeTick() {
    static int phase=0,ticks=0;static unsigned long long hostileId=OBJECT_INVALID,friendlyId=OBJECT_INVALID;
    static Vector playerOrigin,hostileOrigin,friendlyOrigin;static unsigned long long damageBefore=0;
    if(phase==0 && !sandboxReady)return;
    if(phase<0)return;++ticks;
    auto* server=g_pAppManager->m_pServerExoApp;auto* player=ServerActor();auto* gui=g_pAppManager->m_pClientExoApp->GetInGameGui();
    auto send=[](const char* input){if(FILE* f=fopen("sandbox-input.txt","w")){fputs(input,f);fclose(f);}};
    auto fail=[&](const char* reason){fprintf(stderr,"K1_SANDBOX_FAIL phase=%d reason=%s\n",phase,reason);phase=-1;send("quit:0\n");};
    auto button=[&](int index){auto* control=&sandboxPanel->buttons[index];sandboxPanel->SetActiveControl(control,0);sandboxPanel->HandleInputEvent(INPUTEVENT_GUI_A,1);};
    auto* hostile=server->GetCreatureByGameObjectID(hostileId);auto* friendly=server->GetCreatureByGameObjectID(friendlyId);
    if(phase==0 && ticks>=30) {
        static int switched=0;static bool waiting=false;
        static CResRef texture;static std::string headName;
        if(switched<4 && !std::getenv("K1_ENGAGE_NEW_SPECIES_TEST")) {
            auto* client=Actor();
            if(!waiting) {
                if(!CheckArenaSaberInventory(player,"before_menu")){fail("switch_inventory_ownership");return;}
                texture=client->GetAnimationBase()->m_resModelTexture;
                auto* head=client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1);
                headName=head&&head->m_pModel?head->m_pModel->m_szName:"";
                auto* item=server->GetItemByGameObjectID(arenaSaberIds[switched%2]);
                auto* old=player->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
                if(!item || !old){fail("switch_inventory");return;}
                {
                    auto* panel=new CSWGuiInGameEquip(gui->m_pGuiManager);
                    panel->SetCharacter(client);panel->UpdateInventory();
                    panel->OnEnterSlot(&panel->m_abtnSlot[CSWGuiInGameEquip::GUI_SLOT_WEAP_R]);
                    panel->m_abtnSlot[CSWGuiInGameEquip::GUI_SLOT_WEAP_R].m_tail.nCurrentEventArg=1;
                    panel->OnSelectSlot(&panel->m_abtnSlot[CSWGuiInGameEquip::GUI_SLOT_WEAP_R]);
                    bool selected=false;
                    for(int i=0;i<panel->m_lbItems.m_aControls.m_count;++i) {
                        auto* row=static_cast<CSWGuiInGameItemEntry*>(panel->m_lbItems.m_aControls.m_pArray[i]);
                        if(server->ClientToServerObjectId(row->m_oidItem)!=item->m_objectId)continue;
                        row->m_tail.nCurrentEventArg=1;row->m_tail.nCurrentEventType=INPUTEVENT_GUI_A;
                        fprintf(stderr,"K1_SABER_MENU_SELECT switch=%d item=%llu\n",switched,item->m_objectId);
                        panel->OnItemSelected(row);
                        panel->m_btnEquip.m_tail.nCurrentEventArg=1;
                        panel->OnOKPressed(&panel->m_btnEquip);selected=true;break;
                    }
                    delete panel;
                    if(!selected){fail("switch_menu_row");return;}
                }
                waiting=true;ticks=0;return;
            }
            auto* equipped=player->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
            if(!Supported(client) || !equipped || equipped->m_objectId!=arenaSaberIds[switched%2]){if(ticks>180)fail("switch_replication");return;}
            if(!CheckArenaSaberInventory(player,"after_menu")){fail("switch_inventory_ownership");return;}
            if(!K1EngageEnter()){fail("switch_enter");return;}
            auto* body=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
            auto* weapon=client->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
            if(!body || !body->m_pModel || _stricmp(body->m_pModel->m_szName,PlayerRigResource()) ||
               StaffMode()!=(switched%2==1) || !weapon || !weapon->m_pModel ||
               !CheckRigActorOwnership(client,true,texture,headName,weapon->m_pModel->m_szName,"saber_switch")){fail("switch_stance_or_appearance");return;}
            const std::string weaponName=weapon->m_pModel->m_szName;
            Plan();
            if(!CheckRigActorOwnership(client,false,texture,headName,weaponName,"saber_switch_restore")){fail("switch_restore");return;}
            ++switched;waiting=false;ticks=0;
            if(switched==4)fprintf(stderr,"K1_SABER_SWITCH_PASS switches=4 single=2 staff=2 outfit=1 head=1 native_restore=1\n");
            if(switched==4 && std::getenv("K1_ENGAGE_SABER_SWITCH_TEST")){phase=-1;send("quit:0\n");}
            return;
        }
        playerOrigin=player->m_position;send("key:39\n");phase=1;ticks=0;return;
    }
    if(phase==1) {
        if(!sandboxPanel || !IsGamePaused()){if(ticks>120)fail("menu_open_or_pause");return;}
        static bool browserChecked=false;
        if(!browserChecked){int result=SandboxBrowserProbe(sandboxPanel);if(result<0){fail("browser_preview_or_scroll");return;}if(!result)return;browserChecked=true;}
        if(std::getenv("K1_ENGAGE_NEW_SPECIES_TEST")){fprintf(stderr,"K1_SANDBOX_NEW_SPECIES_PASS\n");SandboxToggle();phase=-1;send("quit:0\n");return;}
        auto* panel=sandboxPanel;bool hit=true;const auto box=panel->m_extent;
        panel->SelectCustom();
        for(int i=0;i<K1SandboxPanel::Count;++i){auto e=panel->buttons[i].m_extent;hit=hit && panel->HitCheckMouse(box.x+e.x+e.w/2,box.y+e.y+e.h/2)==&panel->buttons[i];}
        fprintf(stderr,"K1_SANDBOX_HIT_CHECK hit=%d flags=%d modal=%d movement=%g\n",hit,panel->m_stateFlags,gui->m_pGuiManager->m_modalPanels.m_count,magnitude(player->m_position-playerOrigin));
        if(!hit || gui->m_pGuiManager->m_modalPanels.m_count!=1 || magnitude(player->m_position-playerOrigin)>.01f){fail("native_menu_hit_or_lock");return;}
        button(K1SandboxPanel::Ai);if(sandboxAi){fail("ai_button");return;}
        button(K1SandboxPanel::Spawn);hostileId=lastSandboxSpawn;hostile=server->GetCreatureByGameObjectID(hostileId);
        if(!hostile || sandboxActors.size()!=1 || hostile->GetReputation(player->m_objectId,0)>10 || StaffNpc(hostile)){fail("hostile_yellow_spawn");return;}
        const auto count=sandboxActors.size();button(K1SandboxPanel::Spawn);if(sandboxActors.size()!=count){fail("blocked_spawn_overlap");return;}
        Vector direction=player->m_orientation;player->SetOrientation(Vector(-direction.y,direction.x,0));
        button(K1SandboxPanel::StanceRight);button(K1SandboxPanel::Team);button(K1SandboxPanel::Spawn);
        friendlyId=lastSandboxSpawn;friendly=server->GetCreatureByGameObjectID(friendlyId);player->SetOrientation(direction);
        if(!friendly || friendly==hostile || !StaffNpc(friendly) || friendly->GetReputation(player->m_objectId,0)<90 || hostile->GetReputation(friendlyId,0)>10 || friendly->GetReputation(hostileId,0)>10){fail("friendly_staff_factions");return;}
        hostileOrigin=hostile->m_position;friendlyOrigin=friendly->m_position;
        fprintf(stderr,"K1_SANDBOX_UI_PASS modal=1 buttons=6 blocked_spawn=1 hostile_yellow=1 friendly_staff=1\n");
        panel->SelectCustom();
        for(int i=0;i<4;++i)button(K1SandboxPanel::ColorRight);
        button(K1SandboxPanel::CombatRight);
        send("key:41\n");phase=2;ticks=0;return;
    }
    if(phase==2 && ticks>=150) {
        if(sandboxPanel || sandboxMenuOpen || IsGamePaused() || K1EngagePlanning() || K1EngageEngaged() || !hostile || !friendly ||
           hostile->m_actionList.m_pInternal->m_nCount || friendly->m_actionList.m_pInternal->m_nCount ||
           magnitude(hostile->m_position-hostileOrigin)>.01f || magnitude(friendly->m_position-friendlyOrigin)>.01f || !npcPlayback.empty()) {fail("ai_off_idle_no_engage");return;}
        fprintf(stderr,"K1_SANDBOX_IDLE_PASS frames=150 movement=0 actions=0 engage=0\n");
        if(std::getenv("K1_ENGAGE_FONT_TEST")) {
            fprintf(stderr,"K1_FONT_UI_PASS menu=1 buttons=6 repeated=1 idle=1\n");
            phase=-1;send("quit:0\n");return;
        }
        send("key:39\n");phase=3;ticks=0;return;
    }
    if(phase==3) {
        if(!sandboxPanel){if(ticks>120)fail("reopen");return;}
        if(sandboxPanel->faction!=3 || sandboxPanel->type!=0 || sandboxPanel->variant!=0 || sandboxPanel->saberColor!=4 || sandboxPanel->combatStance!=1 || !sandboxPanel->friendly){fail("reopen_selection_memory");return;}
        if(gui->m_bInGameMenuShown){fail("escape_opened_options");return;}
        fprintf(stderr,"K1_SANDBOX_MENU_MEMORY_PASS faction=1 npc=1 variant=1 alignment=1 escape_priority=1\n");
        // Make the two NPCs each other's nearest hostile in this controlled
        // combat fixture. UI placement/facing was already tested independently.
        friendly->SetPosition(hostile->m_position+Vector(0,1.15f,0),1,1,0);
        player->SetPosition(playerOrigin-normalize(hostile->m_position-playerOrigin)*4.f,1,1,0);player->SetCurrentHitPoints(1000);
        hostile->m_hitPoints=1000;hostile->SetCurrentHitPoints(1000);friendly->m_hitPoints=1000;friendly->SetCurrentHitPoints(1000);
        if(SandboxTarget(hostile,player)!=nullptr){fail("disabled_target_selection");return;}
        button(K1SandboxPanel::TargetPlayer);
        if(sandboxTargetPlayer || hostile->GetReputation(player->m_objectId,0)<=10 || hostile->GetReputation(friendlyId,0)>10){fail("target_player_off_reputation");return;}
        button(K1SandboxPanel::Ai);fprintf(stderr,"K1_SANDBOX_OPPONENTS player_distance=%g ally_distance=%g hostile_target=%llu friendly_target=%llu\n",magnitude(player->m_position-hostile->m_position),magnitude(friendly->m_position-hostile->m_position),SandboxTarget(hostile,player)?SandboxTarget(hostile,player)->m_objectId:OBJECT_INVALID,SandboxTarget(friendly,player)?SandboxTarget(friendly,player)->m_objectId:OBJECT_INVALID);if(!sandboxAi || SandboxTarget(hostile,player)!=friendly || SandboxTarget(friendly,player)!=hostile){fail("enabled_opponents");return;}
        const Vector spectator=player->m_position;player->SetPosition(hostile->m_position+Vector(.2f,0,0),1,1,0);
        if(SandboxTarget(hostile,player)!=friendly){fail("target_player_off_nearest");return;}
        button(K1SandboxPanel::TargetPlayer);
        if(!sandboxTargetPlayer || SandboxTarget(hostile,player)!=player){fail("target_player_on_nearest");return;}
        button(K1SandboxPanel::TargetPlayer);
        if(SandboxTarget(hostile,player)!=friendly || hostile->m_actionList.m_pInternal->m_nCount || friendly->m_actionList.m_pInternal->m_nCount){fail("target_player_off_queue_clear");return;}
        player->SetPosition(spectator,1,1,0);
        // Isolate physical contacts from the independent periodic Force fixture.
        // Force/native utility handoffs have their own installed-data probes.
        auto now=server->GetWorldTimer()->GetSnapshotTime();
        sandboxForces[hostileId].next=sandboxForces[friendlyId].next=now+60000000;
        fprintf(stderr,"K1_SANDBOX_TARGET_POLICY_PASS off_nearest=1 on_nearest=1 queue_clear=1 native_reputation=1\n");
        button(K1SandboxPanel::Close);phase=4;ticks=0;return;
    }
    if(phase==4) {
        if(sandboxPanel || IsGamePaused() || !K1EngagePlanning()){if(ticks>180)fail("hostile_auto_detection");return;}
        damageBefore=meleeDamageContacts;send("key:44\n");phase=5;ticks=0;return;
    }
    if(phase==5) {
        const auto a=npcPlayback.find(hostileId),b=npcPlayback.find(friendlyId);
        // The menu walk changes native RNG consumption. Verify real contacts
        // in each direction without making two synchronized test actors defeat
        // repeated timed deflections before the deadline. Gameplay stays intact.
        if(a!=npcPlayback.end() && b!=npcPlayback.end()) {
            const auto now=server->GetWorldTimer()->GetSnapshotTime();
            static bool secondDirection=false;
            if(sandboxActors[hostileId].meleeHits && !secondDirection) {
                secondDirection=true;b->second.nextAttack=now;b->second.parryReady=now;
            }
            auto& hold=sandboxActors[hostileId].meleeHits? a->second:b->second;
            hold.nextAttack=now+60000000;hold.parryReady=now+60000000;
        }
        if(a!=npcPlayback.end() && b!=npcPlayback.end() && a->second.target==friendlyId && b->second.target==hostileId && meleeDamageContacts>damageBefore && sandboxActors[hostileId].meleeHits>0 && sandboxActors[friendlyId].meleeHits>0) {
            if(player->GetCurrentHitPoints(0)<1000){fail("target_player_off_damage");return;}
            // Force casting/status effects legitimately yield to native movement.
            // Verify ownership after both NPCs return, within the existing deadline.
            if(!K1EngageEnemyMovementOwned(hostileId) || !K1EngageEnemyMovementOwned(friendlyId)){
                if(ticks>1800)fail("both_teams_movement_ownership");return;
            }
            fprintf(stderr,"K1_SANDBOX_COMBAT_PASS hostile_target=friendly friendly_target=hostile native_melee_hits=%llu friendly_hits=%u hostile_hits=%u\n",meleeDamageContacts-damageBefore,sandboxActors[friendlyId].meleeHits,sandboxActors[hostileId].meleeHits);
            send("key:39\n");phase=6;ticks=0;return;
        }
        if(ticks%120==0)fprintf(stderr,"K1_SANDBOX_COMBAT_WAIT active=%d a=%d b=%d a_target=%llu b_target=%llu hits=%llu\n",K1EngageActive(),a!=npcPlayback.end(),b!=npcPlayback.end(),a!=npcPlayback.end()?a->second.target:OBJECT_INVALID,b!=npcPlayback.end()?b->second.target:OBJECT_INVALID,meleeDamageContacts-damageBefore);
        if(ticks>1800)fail("npc_vs_npc_damage");return;
    }
    if(phase==6) {
        if(!sandboxPanel){if(ticks>120)fail("open_during_combat");return;}
        button(K1SandboxPanel::Ai);if(sandboxAi){fail("disable_midfight");return;}
        button(K1SandboxPanel::Close);phase=7;ticks=0;return;
    }
    if(phase==7 && ticks>=240) {
        if(sandboxPanel || IsGamePaused() || K1EngageEngaged() || K1EngagePlanning() || !npcPlayback.empty() ||
           hostile->m_actionList.m_pInternal->m_nCount || friendly->m_actionList.m_pInternal->m_nCount){fail("midfight_idle_cleanup");return;}
        CSWGuiSaveLoadEntry entry;entry.LoadData(CExoString("000003 - Game2"));
        if(!entry.m_bSaveExists || entry.m_bCorrupt){fail("reload_fixture_save");return;}
        auto* app=g_pAppManager->m_pClientExoApp;
        auto* panel=new CSWGuiSaveLoad(gui->m_pGuiManager,0,0);gui->m_pGuiManager->AddPanel(panel,2,1);
        const auto accepted=gui->m_pSoloModeQueryBox->m_bAccepted;gui->m_pSoloModeQueryBox->m_bAccepted=1;
        phase=8;ticks=0;fprintf(stderr,"K1_SANDBOX_RELOAD_BEGIN\n");panel->LoadGame(&entry);
        if(app->GetInGameGui()==gui)gui->m_pSoloModeQueryBox->m_bAccepted=accepted;
        return;
    }
    if(phase==8 && !K1EngageArenaLoadoutActive()) {
        if(!sandboxActors.empty() || !sandboxForces.empty() || sandboxReady){fail("unload_roster");return;}
        server->SetMoveToModuleString(CExoString("k1arena"));server->SetMoveToModulePending(1);phase=9;ticks=0;return;
    }
    if(phase==9 && sandboxReady){send("key:39\n");phase=10;ticks=0;return;}
    if(phase==10) {
        if(!sandboxPanel){if(ticks>180)fail("reload_menu");return;}
        if(!sandboxActors.empty()){fail("reload_stale_roster");return;}
        sandboxPanel->SelectCustom();
        button(K1SandboxPanel::Spawn);if(sandboxActors.size()!=1 || sandboxAi){fail("reload_spawn");return;}
        button(K1SandboxPanel::Close);phase=11;ticks=0;return;
    }
    if(phase==11 && ticks>=150) {
        if(sandboxPanel || IsGamePaused() || K1EngagePlanning() || !npcPlayback.empty()){fail("reload_idle");return;}
        fprintf(stderr,"K1_SANDBOX_PASS menu=1 repeated=1 factions=1 placement=1 ai_off_idle=1 npc_combat=1 cleanup=1 reload=1\n");phase=-1;send("quit:0\n");return;
    }
}
#include "include/k1sandboxdualprobe.h"
void SandboxUnload(unsigned long long areaId) {
    if(sandboxArea==OBJECT_INVALID || (areaId!=OBJECT_INVALID && areaId!=sandboxArea))return;
    fprintf(stderr,"K1_SANDBOX_UNLOAD actors=%zu\n",sandboxActors.size());
    sandboxActors.clear();sandboxNativeActors.clear();sandboxForces.clear();sandboxStances.clear();sandboxAppearances.clear();sandboxReady=false;sandboxArea=OBJECT_INVALID;
    sandboxResume=false;sandboxRestorePause=-1;
    if(sandboxPanel)sandboxPanel->SetToBeDeleted(1);
}
#include "include/k1yellowgaitprobe.h"
#include "include/k1presentationprobe.h"
#include "include/k1redmotionprobe.h"
#include "include/k1redprobe.h"
#include "include/k1nativenpcprobe.h"
#include "include/k1armorprobe.h"
#include "include/k1npcpolicyprobe.h"
#include "include/k1meleehitprobe.h"
#include "include/k1grenademineprobe.h"
void SandboxTick() {
    if(sandboxTogglePending){sandboxTogglePending=false;SandboxToggle();}
    if(!ServerActor() || !ServerActor()->GetArea())return;
    if(std::getenv("K1_GRENADE_MINE_TEST"))GrenadeMineProbeTick();
    else if(std::getenv("K1_ENGAGE_MELEE_HIT_TEST"))MeleeHitProbeTick();
    else if(std::getenv("K1_ENGAGE_NPC_POLICY_TEST"))NpcCombatPolicyProbeTick();
    else if(std::getenv("K1_ENGAGE_ARMOR_TEST"))ArmorProbeTick();
    else if(std::getenv("K1_NATIVE_NPC_TEST"))NativeNpcProbeTick();
    else if(std::getenv("K1_RED_TEST"))RedProbeTick();
    else if(std::getenv("K1_YELLOW_NATIVE_GAIT_TEST"))YellowNativeGaitProbeTick();
    else if(std::getenv("K1_ENGAGE_SANDBOX_DUAL_TEST"))SandboxDualProbeTick();
    else if(std::getenv("K1_ENGAGE_SANDBOX_TEST"))SandboxProbeTick();
    if(!sandboxReady && sandboxSession && K1EngageArenaLoadoutActive())sandboxReady=true;
    if(!sandboxReady)return;
    if(std::getenv("K1_PORTABLE_SMOKE")) {
        static bool checked=false;
        if(!checked){checked=true;const bool ok=PurpleMode() && !StaffMode() && !DualMode() && !strcmp(PlayerRigResource(),"k1japurp");
            fprintf(stderr,"K1_PORTABLE_PURPLE_%s rig=%s rate=%g\n",ok?"PASS":"FAIL",PlayerRigResource(),PlayerMeleeRate());}
    }
    auto* server=g_pAppManager->m_pServerExoApp;auto* player=ServerActor();auto* area=static_cast<CSWSArea*>(player->GetArea());
    if(sandboxArea==OBJECT_INVALID)sandboxArea=area->m_objectId;
    if(sandboxArea!=area->m_objectId || !K1EngageArenaLoadoutActive()){sandboxActors.clear();sandboxNativeActors.clear();sandboxForces.clear();sandboxStances.clear();sandboxAppearances.clear();sandboxReady=false;sandboxArea=OBJECT_INVALID;if(sandboxPanel)sandboxPanel->SetToBeDeleted(1);return;}
    for(auto it=sandboxActors.begin();it!=sandboxActors.end();) {
        auto* actor=server->GetCreatureByGameObjectID(it->first);
        if(!actor){sandboxNativeActors.erase(it->first);sandboxForces.erase(it->first);sandboxStances.erase(it->first);sandboxAppearances.erase(it->first);it=sandboxActors.erase(it);continue;}
        actor->m_bPlot=0;actor->m_bMin1HP=0;actor->m_bDestroyable=1;
        if(!sandboxAi && !actor->GetDead()){if(actor->m_actionList.m_pInternal->m_nCount)actor->ClearAllActions(1);actor->SetCombatState(0,1);if(actor->m_nState==EFFECT_STATE_NONE)actor->SetAnimation(SERVER_ANIMATION_PAUSE);}
        if(sandboxAi)StartNativeSandboxCombat(actor);
        ++it;
    }
    // A close can precede application of the open request. Wait for the native
    // pending request, then restore exactly once; do not overwrite that queue.
    if(sandboxRestorePause>=0 && !sandboxPanel && !g_pAppManager->m_pClientExoApp->m_pInternal->m_bTogglePauseAtEndOfFrame) {
        if((server->GetPauseState(2)!=0)==(sandboxRestorePause!=0))sandboxRestorePause=-1;
        else g_pAppManager->m_pClientExoApp->SetPausedByCombat(sandboxRestorePause,0,0);
    }
    if(sandboxResume && sandboxRestorePause<0 && !sandboxPanel && !IsGamePaused()){sandboxResume=false;if(K1EngagePlanning())K1EngageEnter();}
    if(sandboxSession && sandboxAi && sandboxRestorePause<0 && !sandboxMenuOpen && state.mode==K1EngageState::Mode::Exploration && Spotted())g_pAppManager->m_pClientExoApp->SetAutoPaused(1,CClientExoAppInternal::PAUSE_MODE_ENEMY_SIGHTED);
}
// Opt-in installed-data probe: real item actions, native clips and return to
// each actor's Engage rig. Player trials resume from planning; also check a
// native Force cast. All items/effects exist only in this unsaved arena.
void UtilityProbeTick(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* npc) {
    static int forceObserved=0,forceSpends=0;
    static int trial=0,nativeSamples=0,beforeHP=0,beforeCount=0,beforeTriggers=0;
    static int playerMedkitUses=0;
    static unsigned long long itemId=OBJECT_INVALID;
    static Vector medkitRecoveryOrigin;
    auto* server=g_pAppManager->m_pServerExoApp;
    auto* client=player->GetClientCreature();
    auto quit=[] {if(FILE* f=fopen("utility-input.txt","w")) {fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* why) {fprintf(stderr,"K1_NATIVE_UTILITY_FAIL trial=%d reason=%s\n",trial,why);phase=-1;quit();};
    auto triggerCount=[&] {
        int count=0;auto* area=static_cast<CSWSArea*>(player->GetArea());
        for(int i=0;i<area->m_aObjectsByX.m_count;++i) {
            auto* object=server->GetGameObject(area->m_aObjectsByX.m_pArray[i]);
            if(object && object->m_objectType==CGameObject::OBJECT_TYPE_TRIGGER) ++count;
        }
        return count;
    };
    if(!npc) {fail("missing_opponent");return;}
    if(phase==2) {
        npc->ClearAllActions(1);npc->m_hitPoints=1000;npc->SetCurrentHitPoints(1000);
        player->GetClientCreature()->SetPrecalculatedPosition(Vector(200,100,0));
        npc->SetPosition(Vector(204,100,0),1,1,0);
        npc->AddToVisibleList(player->m_objectId,1,0,0,0);
        if(!K1EngageEnter()) {fail("setup");return;}
        player->m_pStats->SetSkillRank(SKILL_TREAT_INJURY,20);
        npc->m_pStats->SetSkillRank(SKILL_TREAT_INJURY,20);
        npc->m_pStats->SetSkillRank(SKILL_DEMOLITIONS,20);
        phase=59;ticks=0;return;
    }
    auto owned=npcPlayback.find(npc->m_objectId);
    if(owned!=npcPlayback.end() && owned->second.reaction && !strcmp(owned->second.reaction,"j_pain1")) {
        fail("utility_triggered_source_pain");return;
    }
    if(owned!=npcPlayback.end() && phase!=59) {
        const auto hold=server->GetWorldTimer()->GetSnapshotTime()+2000000;
        owned->second.nextThink=hold;owned->second.nextAttack=hold;StopNpcMove(owned->second,npc);
    }
    if(phase==59) {
        if(owned!=npcPlayback.end() && owned->second.navigator && K1EngageOwnsEnemy(npc->m_objectId) &&
            K1EngageOwnsAnimation(client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1))) {
            if(std::getenv("K1_ENGAGE_PLAYER_WEAPON")) {
                std::vector<K1SaberContact::Blade> segments;
                if(StaffMode())segments=StaffBlades(client);
                else {Vector p,q;if(Blade(client,p,q))segments.push_back({p,q});}
                if(!Supported(client) || segments.empty() || magnitude(segments[0].tip-segments[0].base)<.1f){fail("player_weapon_geometry");return;}
                fprintf(stderr,"K1_PLAYER_MELEE_SUPPORT_PASS base=%d class=%d staff=%d segments=%zu length=%g rig=%s\n",NativeNpcRightItem(player)->m_baseItem,client->GetAnimationBase()->m_nWeaponClass,StaffMode(),segments.size(),magnitude(segments[0].tip-segments[0].base),PlayerRigResource());
            }
            phase=60;ticks=0;
        } else if(ticks>240) fail("initial_ownership");
        return;
    }
    auto* actor=trial%2?npc:player;
    auto* target=trial%2?player:npc;
    const int kind=trial/2;
    const char* names[]={"grenade","mine","medpac","choke"};
    if(phase==60) {
        // Test prompt animation startup on a fresh native utility round. A
        // second kit still respects KotOR's one-cast-per-round cooldown.
        if(trial==4 && playerMedkitUses==1 && player->m_pCombatRound->m_nCastsThisRound>0) {
            if(ticks==1) fprintf(stderr,"K1_MEDKIT_NATIVE_COOLDOWN_WAIT\n");
            if(ticks>240) fail("native_cooldown_timeout");
            return;
        }
        const char* items[]={"g_w_fraggren01","g_i_trapkit001","g_i_medeqpmnt01"};
        CSWSItem* item=nullptr;
        if(kind<3) {
            item=new CSWSItem(OBJECT_INVALID);
            if(!item->LoadFromTemplate(CResRef(items[kind]))) {delete item;fail("item_template");return;}
            item->m_flags &= ~4;
            if(!actor->AcquireItem(&item,OBJECT_INVALID,OBJECT_INVALID,1)) {delete item;fail("item_inventory");return;}
            // Cover both a remaining stack and its final-use destruction.
            if(kind==2) item->m_stackCount=actor==player && !playerMedkitUses?2:1;
            itemId=item->m_objectId;beforeCount=item->m_stackCount;beforeTriggers=triggerCount();nativeSamples=0;
        }
        bool queued=false;
        if(trial%2==0) Plan(true);
        if(kind==0) {
            beforeHP=target->GetCurrentHitPoints(0);
            queued=actor->UseItem(itemId,0,0,target->m_objectId,target->m_position,0,0)!=0;
        } else if(kind==1) {
            if(actor==player && hudOwner) {
                auto& actions=hudOwner->m_personalActions[3];actions.SetSize(0);
                client->GetMines(actions);
                const unsigned int wanted=static_cast<unsigned int>(g_pAppManager->m_pClientExoApp->ServerToClientObjectId(itemId))|CSWGuiInterfaceAction::ACTION_ID_ITEM;
                for(int i=0;i<actions.m_count;++i)if(actions.m_pArray[i].m_nActionId==wanted) {
                    actions.m_pArray[i].m_nFlags|=1;hudOwner->m_personalActionIds[3]=wanted;
                    auto* options=g_pAppManager->m_pClientExoApp->GetClientOptions();
                    const auto tutorials=options->m_bTutorialWindows;options->m_bTutorialWindows=0;
                    hudOwner->DoPersonalAction(3,0);options->m_bTutorialWindows=tutorials;queued=true;break;
                }
                fprintf(stderr,"K1_MINE_UI_QUEUE requested=%llu entries=%d combat=%d\n",itemId,actions.m_count,actor->m_bCombatState);
            } else queued=actor->UseItem(itemId,0,0,OBJECT_INVALID,actor->m_position+actor->m_orientation*.25f,0,0)!=0;
        } else if(kind==2) {
            actor->SetCurrentHitPoints(actor->GetMaxHitPoints(0)-60);beforeHP=actor->GetCurrentHitPoints(0);
            if(actor==player) {
                client->PartyMenuUseItem(static_cast<unsigned int>(g_pAppManager->m_pClientExoApp->ServerToClientObjectId(itemId)),client);
                queued=true; // The native input message is checked after the planning hold.
            } else queued=actor->UseItem(itemId,0,0,actor->m_objectId,actor->m_position,0,0)!=0;
        } else {
            int slot=0;
            while(slot<actor->m_pStats->m_numClasses && !actor->m_pStats->GetSpellUsesLeft(FORCE_POWER_CHOKE,slot,0,0)) ++slot;
            beforeHP=target->GetCurrentHitPoints(0);beforeCount=actor->m_pStats->m_nCurrentForcePoints;nativeSamples=0;
            queued=slot<actor->m_pStats->m_numClasses && actor->AddCastSpellActions(FORCE_POWER_CHOKE,slot,0,0,0,
                target->m_position,target->m_objectId,1,0,1,0,0,0,-1,0xff,0)!=0;
            if(queued)queued=actor->AddCastSpellActions(FORCE_POWER_CHOKE,slot,0,0,0,
                target->m_position,target->m_objectId,1,0,1,0,0,0,-1,0xff,0)!=0;
            forceObserved=beforeCount;forceSpends=0;
        }
        fprintf(stderr,"K1_NATIVE_UTILITY_QUEUE actor=%llu kind=%s queued=%d\n",actor->m_objectId,names[kind],queued);
        if(!queued) {fail("queue");return;}
        phase=64;ticks=0;return;
    }
    if(phase==64) {
        if(trial%2==0) {
            if(ticks<15) return;
            auto* queuedItem=server->GetItemByGameObjectID(itemId);
            if(kind==3?actor->m_pStats->m_nCurrentForcePoints<beforeCount:
                !queuedItem || queuedItem->m_stackCount<beforeCount) {fail("consumed_in_planning");return;}
            if(!K1EngagePlanning() || !NativeUtilityQueued(actor) || !K1EngageEnter()) {fail("planning_resume");return;}
        }
        phase=61;ticks=0;return;
    }
    auto* body=actor->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
    if(kind==3 && (phase==61 || phase==62) && actor->m_pStats->m_nCurrentForcePoints<forceObserved) {
        forceObserved=actor->m_pStats->m_nCurrentForcePoints;++forceSpends;
        if(forceSpends==2) {
            const auto gap=server->GetWorldTimer()->GetSnapshotTime()-utilityReleaseAt[actor->m_objectId];
            if(!utilityReleaseAt.count(actor->m_objectId) || gap<0 || gap>250000){fail("queued_force_gap");return;}
            fprintf(stderr,"K1_FORCE_QUEUE_GAP_PASS casts=2 gap_us=%lld\n",gap);
        }
    }
    if(phase==61) {
        if(ticks<=35) {
            auto* head=actor->m_actionList.m_pInternal->m_pHead;
            auto* action=head?static_cast<CSWSObjectActionNode*>(head->m_pData):nullptr;
            fprintf(stderr,"K1_UTILITY_START trial=%d tick=%d native=%d server_anim=%d action=%d round=%d paused=%d timer=%d casts=%d client_anim=%d ff_left=%d ff_count=%d\n",
                trial,ticks,nativeActionBody,actor->m_nAnimation,action?int(action->m_nActionId):-1,
                actor->m_pCombatRound->m_bRoundStarted,actor->m_pCombatRound->m_bRoundPaused,
                actor->m_pCombatRound->m_nPauseTimer,actor->m_pCombatRound->m_nCastsThisRound,
                actor->GetClientCreature()->m_currentAnimation,actor->GetClientCreature()->m_nFireForgetTimeLeftMs,
                actor->GetClientCreature()->m_nFireForgetCount);
        }
        const int animations[][2]={
            {CSWCAnimBase::CLIENT_ANIMATION_THROW_GRENADE_1,CSWCAnimBase::CLIENT_ANIMATION_THROW_GRENADE_2},
            {CSWCAnimBase::CLIENT_ANIMATION_SET_MINE,CSWCAnimBase::CLIENT_ANIMATION_SET_MINE},
            {CSWCAnimBase::CLIENT_ANIMATION_INJECT,CSWCAnimBase::CLIENT_ANIMATION_INJECT},
            {CSWCAnimBase::CLIENT_ANIMATION_CAST_OUT_2,CSWCAnimBase::CLIENT_ANIMATION_CAST_OUT_2}};
        CExoString clipName;bool playing=false;float at=0,length=0;
        if(body) for(int animation:animations[kind]) {
            auto name=actor->GetClientCreature()->GetAnimationBase()->GetAnimationName(animation);
            if(body->GetAnimation(const_cast<char*>(name.CStr()),&at,&length)) {playing=true;clipName=name;}
        }
        if(playing) {
            if(trial%2==0 && ticks>6) {fail("planning_start_latency");return;}
            if(IsSourceBody(body) || K1EngageOwnsAnimation(body) || body->m_pAddInModel) {fail("non_native_animation_owner");return;}
            ++nativeSamples;
            if(kind==3) {
                const int health=actor->GetCurrentHitPoints(0),animation=actor->m_nAnimation;
                const auto spell=actor->m_castSpellId;
                auto* damage=new CGameEffect(1);damage->m_subType=CSWSEffectListHandler::EFFECT_DAMAGE;
                damage->m_durationTypeBits=CGameEffect::DURATION_TYPE_INSTANTANEOUS;damage->SetCreator(target->m_objectId);
                damage->SetNumIntegers(22);for(int i=0;i<15;++i)damage->SetInteger(i,-1);
                damage->SetInteger(7,50);damage->SetInteger(14,50);damage->SetInteger(17,3);
                damage->SetInteger(19,1);damage->SetInteger(20,1);actor->ApplyEffect(damage,0,1);
                if(actor->GetCurrentHitPoints(0)>=health || actor->m_nAnimation!=animation ||
                   actor->m_castSpellId!=spell || !NativeUtilityQueued(actor)){fail("damage_cancelled_cast");return;}
                fprintf(stderr,"K1_FORCE_HIT_PRESERVE_PASS actor=%llu spell=%u animation=%d hp_loss=%d\n",actor->m_objectId,unsigned(spell),animation,health-actor->GetCurrentHitPoints(0));
            }
            fprintf(stderr,"K1_NATIVE_UTILITY_CLIP actor=%llu kind=%s clip=%s time=%g native=1 start_frames=%d\n",actor->m_objectId,names[kind],clipName.CStr(),at,ticks);
            if(trial==4) {
                medkitRecoveryOrigin=client->m_position;
                if(FILE* f=fopen("utility-input.txt","w")) {fputs("keydown:7\n",f);fclose(f);}
            }
            phase=62;
        } else if(ticks>360) fail("native_clip");
        return;
    }
    if(phase==62) {
        if(ticks%30==0) {
            auto* c=actor->GetClientCreature();
            auto* remaining=server->GetItemByGameObjectID(itemId);
            fprintf(stderr,"K1_UTILITY_RECOVERY trial=%d tick=%d item=%d count=%d pending=%d native=%d anim=%d ff_count=%d ff_left=%d started=%d cast_round=%d pause=%d\n",
                trial,ticks,remaining!=nullptr,remaining?remaining->m_stackCount:0,NativeUtilityQueued(actor),nativeActionBody,
                c->m_currentAnimation,c->m_nFireForgetCount,c->m_nFireForgetTimeLeftMs,actor->m_bItemCastStarted,
                actor->m_pCombatRound->m_bSpellCastRound,actor->m_pCombatRound->m_nPauseTimer);
            for(auto* node=actor->m_actionList.m_pInternal->m_pHead;node;node=node->m_pNext)
                fprintf(stderr,"K1_UTILITY_RECOVERY_ACTION id=%d\n",static_cast<CSWSObjectActionNode*>(node->m_pData)->m_nActionId);
        }
        if(ticks>600) {fail("pending_return_timeout");return;}
        if(NativeUtilityQueued(actor)) {
            if(!body || IsSourceBody(body) || K1EngageOwnsAnimation(body)) {fail("utility_interrupted");return;}
            if(trial==4 && magnitude(client->m_position-medkitRecoveryOrigin)>.01f) {fail("movement_during_native_inject");return;}
            ++nativeSamples;
        } else if(body && K1EngageOwnsAnimation(body)) {
            auto* item=server->GetItemByGameObjectID(itemId);
            const bool consumed=kind==3?actor->m_pStats->m_nCurrentForcePoints<beforeCount:!item || item->m_stackCount<beforeCount;
            const bool result=kind==0?target->GetCurrentHitPoints(0)<beforeHP:
                kind==1?triggerCount()>beforeTriggers:kind==2?actor->GetCurrentHitPoints(0)>beforeHP:target->GetCurrentHitPoints(0)<beforeHP;
            if(!consumed || !result || nativeSamples<2 || (kind==3 && forceSpends!=2)) {fail("utility_result");return;}
            fprintf(stderr,"K1_NATIVE_UTILITY_RETURN_PASS actor=%llu kind=%s samples=%d consumed=1 result=1\n",actor->m_objectId,names[kind],nativeSamples);
            if(kind==2 && ((beforeCount==1?item!=nullptr:!item || item->m_stackCount!=beforeCount-1) ||
                actor->GetClientCreature()->AnimationDialog(actor->GetClientCreature()->m_currentAnimation))) {
                fail("depleted_medkit_or_animation_not_released");return;
            }
            if(trial==4) {
                medkitRecoveryOrigin=client->m_position;
                phase=65;ticks=0;return;
            }
            if(++trial<7) {phase=60;ticks=0;} else {npc->SetCurrentHitPoints(0);phase=63;ticks=0;}
        } else if(ticks>600) fail("return_timeout");
        return;
    }
    if(phase==65) {
        const float moved=magnitude(client->m_position-medkitRecoveryOrigin);
        if(moved>=.05f || ticks>=6) {
            fprintf(stderr,"K1_LAST_MEDKIT_MOVEMENT_%s before_count=%d moved=%g frames=%d native=%d pending=%d held=%d enabled=%d engaged=%d\n",
                moved>=.05f?"PASS":"FAIL",beforeCount,moved,ticks,nativeActionBody,NativeUtilityQueued(player),keyD,
                g_pAppManager->m_pClientExoApp->m_pInternal->m_pPlayerControl->m_bEnabled,client->m_engagedState);
            for(auto* n=player->m_actionList.m_pInternal->m_pHead;n;n=n->m_pNext)
                fprintf(stderr,"K1_LAST_MEDKIT_ACTION id=%d\n",static_cast<CSWSObjectActionNode*>(n->m_pData)->m_nActionId);
            if(FILE* f=fopen("utility-input.txt","w")) {fputs("keyup:7\n",f);fclose(f);}
            if(moved<.05f) {fail("last_medkit_movement");return;}
            if(++playerMedkitUses==2) ++trial;
            phase=60;ticks=0;
        }
        return;
    }
    if(phase==63) {
        if(state.mode==K1EngageState::Mode::Exploration && !IsSourceBody(body)) {
            fprintf(stderr,"K1_NATIVE_UTILITY_PASS trials=8\n");phase=-1;quit();
        } else if(ticks>120) fail("victory_restore");
    }
}
// Isolated installed-data contract: real guard contacts/native effects on each
// actor, repeated break/recovery, planning ownership and ranged exclusion.
void PoiseRegenerationProbeTick(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* npc) {
    static long long began=0,firstTick=0,deadline=0;
    static int expected=0,swingSamples=0;static Vector position;
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    auto& p=meleePoise[player->m_objectId];
    auto input=[](const char* commands){if(FILE* f=fopen("poise-input.txt","w")){fputs(commands,f);fclose(f);}};
    auto fail=[&](const char* why){fprintf(stderr,"K1_POISE_REGEN_FAIL phase=%d reason=%s percent=%d now=%lld quiet=%lld next=%lld\n",phase,why,p.percent,now,p.quietUntil,p.nextTick);phase=-1;input("quit:0\n");};
    auto found=npcPlayback.find(npc->m_objectId);
    if(found!=npcPlayback.end()) {
        found->second.nextThink=found->second.nextAttack=now+60000000;
        found->second.attacking=false;StopNpcMove(found->second,npc);
    }
    if(ticks>900) {fail("timeout");return;}
    if(phase==80 && ticks>=60) {
        if(found==npcPlayback.end() || !PoiseRegenerationClockProbe()) {fail("setup_or_clock");return;}
        state.ClearInputs();movement.Clear();aimYaw=0;
        player->GetClientCreature()->SetPrecalculatedPosition(Vector(200,100,0));
        npc->SetPosition(Vector(201.25f,100,0),1,1,0);
        player->SetOrientation(Vector(1,0,0));npc->SetOrientation(Vector(-1,0,0));
        p.percent=100;
        const bool savedNative=nativeActionBody;auto* client=player->GetClientCreature();
        const auto savedAnimation=client->m_currentAnimation;
        bool swinging=false,eligible=false;
        nativeActionBody=true;client->m_currentAnimation=CSWCAnimBase::CLIENT_ANIMATION_RUN;
        PoiseRegenerationState(player,now,swinging,eligible);const bool runBlocked=!eligible;
        client->m_currentAnimation=CSWCAnimBase::CLIENT_ANIMATION_WALK;
        PoiseRegenerationState(player,now,swinging,eligible);const bool walkAllowed=eligible;
        nativeActionBody=savedNative;client->m_currentAnimation=savedAnimation;
        if(!runBlocked || !walkAllowed) {fail("native_run_adapter");return;}
        fprintf(stderr,"K1_POISE_REGEN_NATIVE_GAIT_ADAPTER_PASS\n");
        K1EngageMouse(false,true);
        const auto quiet=p.quietUntil;
        K1EngageBlocksMelee(npc->m_objectId,player->m_objectId);
        K1EngageBlocksMelee(npc->m_objectId,player->m_objectId);
        if(p.quietUntil!=quiet) {fail("query_resets_delay");return;}
        if(!ContactDamage(npc,player) || p.percent!=80 || p.quietUntil!=now+poiseQuietUs) {fail("guard_activity");return;}
        began=now;expected=80;phase=81;ticks=0;
    } else if(phase==81) {
        if(now<began+poiseQuietUs && p.percent!=80) {fail("early_guard_regen");return;}
        if(p.percent==81 && !firstTick) {
            if(now-began<poiseQuietUs || now-began>poiseQuietUs+100000) {fail("first_tick_time");return;}
            firstTick=now;fprintf(stderr,"K1_POISE_REGEN_DELAY_PASS elapsed=%lld guarding=%d\n",now-began,state.Guarding());
        }
        if(p.percent==82) {
            if(!firstTick || now-firstTick<50000 || now-firstTick>150000) {fail("tick_interval");return;}
            fprintf(stderr,"K1_POISE_REGEN_TICK_PASS interval=%lld gain=1\n",now-firstTick);
            auto* damage=new CGameEffect(1);damage->m_subType=CSWSEffectListHandler::EFFECT_DAMAGE;
            damage->m_durationTypeBits=CGameEffect::DURATION_TYPE_INSTANTANEOUS;damage->SetCreator(npc->m_objectId);
            damage->SetNumIntegers(22);for(int i=0;i<15;++i) damage->SetInteger(i,-1);
            damage->SetInteger(7,1);damage->SetInteger(14,1);damage->SetInteger(17,3);
            damage->SetInteger(19,1);damage->SetInteger(20,1);
            const int hp=player->GetCurrentHitPoints(0);player->ApplyEffect(damage,0,1);
            if(player->GetCurrentHitPoints(0)!=hp-1 || p.quietUntil!=now+poiseQuietUs) {fail("native_damage_activity");return;}
            fprintf(stderr,"K1_POISE_REGEN_DAMAGE_PASS hp_delta=1\n");
            began=now;phase=82;ticks=0;
        } else if(p.percent!=80 && p.percent!=81) {fail("discrete_gain");return;}
    } else if(phase==82 && now-began>=1000000) {
        if(p.percent!=82 || !ContactDamage(npc,player) || p.percent!=62 || p.quietUntil!=now+poiseQuietUs) {fail("repeat_guard_reset");return;}
        fprintf(stderr,"K1_POISE_REGEN_REPEAT_HIT_PASS gap=%lld\n",now-began);
        began=now;phase=83;ticks=0;
    } else if(phase==83) {
        if(now<began+poiseQuietUs && p.percent!=62) {fail("repeated_hit_early_gain");return;}
        if(p.percent==63) {
            if(now-began<poiseQuietUs) {fail("repeat_hit_delay");return;}
            JkaWorld lane(player);
            auto contact=lane.trace(JkaPoint(player->m_position),JkaPoint(player->m_position+Vector(6,0,0)),false);
            fprintf(stderr,"K1_POISE_REGEN_RUN_LANE clear=%d blocker=%llu npc=%llu player=%g,%g enemy=%g,%g\n",
                contact.clear,contact.id,npc->m_objectId,player->m_position.x,player->m_position.y,npc->m_position.x,npc->m_position.y);
            if(!contact.clear && contact.character && contact.id==npc->m_objectId) {
                const Vector aside(198,100,0);
                if(!static_cast<CSWSArea*>(npc->GetArea())->PositionWalkable(aside)) {fail("lane_fixture_destination");return;}
                npc->SetPosition(aside,1,1,0);
                if(found!=npcPlayback.end()) {
                    found->second.position=found->second.previousPhysics=found->second.currentPhysics=aside;
                    found->second.physicsRemainder=0;
                }
                contact=lane.trace(JkaPoint(player->m_position),JkaPoint(player->m_position+Vector(6,0,0)),false);
                fprintf(stderr,"K1_POISE_REGEN_RUN_LANE_RECHECK clear=%d blocker=%llu\n",contact.clear,contact.id);
            }
            if(!contact.clear) {fail("running_lane_obstructed");return;}
            K1EngageMouse(false,false);position=player->m_position;
            input("keydown:26\n");began=now;expected=p.percent;phase=84;ticks=0;
        } else if(p.percent!=62) {fail("repeat_hit_gain");return;}
    } else if(phase==84 && now-began>=1200000) {
        const float distance=magnitude(player->m_position-position);
        fprintf(stderr,"K1_POISE_REGEN_RUN_SAMPLE w=%d shift=%d guard=%d percent=%d expected=%d distance=%g speed=%g\n",
            keyW,keyShift,state.ForceWalk(),p.percent,expected,distance,movement.Speed());
        if(!keyW || keyShift || p.percent!=expected || movement.Speed()<=3.3f || distance<1.f) {fail("running_regenerated_or_no_run");return;}
        fprintf(stderr,"K1_POISE_REGEN_RUN_PASS percent=%d distance=%g speed=%g\n",p.percent,distance,movement.Speed());
        input("keyup:26\nkeydown:22\nkeydown:225\n");position=player->m_position;began=now;phase=85;ticks=0;
    } else if(phase==85 && now-began>=1200000) {
        const float distance=magnitude(player->m_position-position);
        // The keyboard bridge starts eligibility on the following frame; a1.2s
        // sample contains11 or12 completed0.1s ticks, depending on frame phase.
        if(!keyS || !keyShift || p.percent<expected+11 || p.percent>expected+12 || movement.Speed()>3.21f || distance<1.f) {fail("shift_walk_regen");return;}
        fprintf(stderr,"K1_POISE_REGEN_WALK_PASS gain=%d distance=%g speed=%g\n",p.percent-expected,distance,movement.Speed());
        input("keyup:22\nkeyup:225\n");began=now;expected=p.percent;phase=86;ticks=0;
    } else if(phase==86 && p.percent!=expected) {
        // ArenaTick observes the previous completed update; wait for the actual
        // standing tick instead of asserting immediately before its update runs.
        if(keyW || keyS || keyShift || movement.Speed()>.2f || p.percent!=expected+1 ||
            now-began<poiseTickUs || now-began>1500000) {fail("standing_regen");return;}
        fprintf(stderr,"K1_POISE_REGEN_STILL_PASS gain=%d\n",p.percent-expected);
        npc->SetPosition(Vector(198,100,0),1,1,0);expected=p.percent;
        K1EngageMouse(true,true);K1EngageMouse(true,false);swingSamples=0;phase=87;ticks=0;
    } else if(phase==87) {
        if(p.percent!=expected) {fail("swing_gained_poise");return;}
        if(state.attacking) ++swingSamples;
        else if(swingSamples) {
            deadline=p.quietUntil;
            if(deadline<=now+poiseQuietUs-100000) {fail("swing_end_delay");return;}
            fprintf(stderr,"K1_POISE_REGEN_SWING_PASS samples=%d quiet=%lld\n",swingSamples,deadline);
            phase=88;ticks=0;
        }
    } else if(phase==88) {
        if(now<deadline && p.percent!=expected) {fail("after_swing_early_gain");return;}
        if(p.percent==expected+1) {
            if(now<deadline) {fail("after_swing_delay");return;}
            fprintf(stderr,"K1_POISE_REGEN_AFTER_SWING_PASS\n");phase=89;ticks=0;
        } else if(p.percent!=expected) {fail("after_swing_gain");return;}
    } else if(phase==89 && p.percent==100) {
        if(meleePoise[npc->m_objectId].percent!=100) {fail("npc_no_recovery");return;}
        fprintf(stderr,"K1_POISE_REGEN_LIVE_PASS delay_us=%lld interval_us=%lld points=1 npc=100 cap=100\n",poiseQuietUs,poiseTickUs);
        npc->SetCurrentHitPoints(0);phase=75;ticks=0;
    }
}
void PoiseProbeTick(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* npc) {
    static int trial=0,hits=0,nativeSamples=0,health=0;
    static long long began=0,recoveryObserved=0,recoveryDeadline=0;
    static bool planningTested=false,clashTested=false,exposedHit=false;
    auto* server=g_pAppManager->m_pServerExoApp;
    auto quit=[] {if(FILE* f=fopen("poise-input.txt","w")){fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* why){fprintf(stderr,"K1_ENGAGE_POISE_FAIL trial=%d phase=%d reason=%s\n",trial,phase,why);phase=-1;quit();};
    if(!npc) {fail("missing_opponent");return;}
    if(phase>=80) {PoiseRegenerationProbeTick(phase,ticks,player,npc);return;}
    auto* defender=trial%2?npc:player;
    auto* attacker=trial%2?player:npc;
    const auto now=server->GetWorldTimer()->GetSnapshotTime();
    if(phase==2) {
        npc->ClearAllActions(1);npc->m_hitPoints=1000;npc->SetCurrentHitPoints(1000);
        player->GetClientCreature()->SetPrecalculatedPosition(Vector(200,100,0));
        npc->SetPosition(Vector(204,100,0),1,1,0);
        player->SetOrientation(Vector(1,0,0));npc->SetOrientation(Vector(-1,0,0));
        npc->AddToVisibleList(player->m_objectId,1,0,0,0);
        if(!K1EngageEnter()) {fail("enter");return;}
        aimYaw=0;movement.Clear(); // match the fixture's explicit +X facing
        phase=70;ticks=0;return;
    }
    if(std::getenv("K1_ENGAGE_POISE_RENDER_TEST")) {
        auto owned=npcPlayback.find(npc->m_objectId);
        // Acquire the real loaded-area cache before holding the diagnostic scene.
        if(owned!=npcPlayback.end() && !owned->second.navigator) {
            owned->second.jedi.aggression=6;return;
        }
        if(owned!=npcPlayback.end()) {
            owned->second.nextThink=now+3000000;owned->second.nextAttack=now+3000000;
            owned->second.attacking=false;StopNpcMove(owned->second,npc);
        }
        if(std::getenv("K1_ENGAGE_POISE_METER_TEST") && poiseMeterEpoch<10) {
            static int equipmentStage=-1,verifiedStage=-1;
            static unsigned int settled=0;
            const int stage=int(poiseMeterEpoch);
            const int values[]={100,75,50,25,0,100,100,100,100,100};
            const char* items[]={nullptr,nullptr,nullptr,nullptr,nullptr,"g_w_shortswrd01",
                "g_w_lghtsbr05","g_w_shortsbr01","g_w_shortsbr05","g_w_lghtsbr01"};
            if(equipmentStage!=stage) {
                if(stage>=5) {
                    K1EngageSuspend(); // unsupported weapons are tested in planning, not source combat
                    if(!EquipFixtureWeapon(player,items[stage])) {fail("meter_equipment");return;}
                }
                equipmentStage=stage;settled=0;
            }
            meleePoise[player->m_objectId].percent=values[stage]; // render-only fixture, not a guard-break assertion
            DelayPoiseRegeneration(meleePoise[player->m_objectId],now); // hold capture-only presentation values
            if(++settled==45) {
                auto* live=player->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
                if(!hudOwner || !live || !live->m_pModel) {fail("meter_no_model");return;}
                const bool saber=(live->m_pModel->m_nFlags & Model::kModelLightsaber)!=0;
                const CExoString expected(saber?live->m_pModel->m_szName:"w_Lghtsbr_001");
                auto& view=hudOwner->m_poiseWeapon;
                if(!HasMeleePoise(player) || view.resource!=expected || view.percent!=values[stage] || !view.m_tail.bVisible) {fail("meter_selection");return;}
                if(saber) {
                    auto* preview=view.m_scene.m_aModels.m_count?static_cast<Gob*>(view.m_scene.m_aModels.m_pArray[0]):nullptr;
                    bool match=preview && preview->m_pModel==live->m_pModel;int hilts=0;
                    auto compareHilt=[&](auto&& self,Part* p)->void {
                        auto* mesh=p->m_pNode->AsMdlNodeTriMesh();
                        // Hilt texture names retain their base variant even on
                        // crystal variants. Identify the actual blade parent.
                        bool bladeParent=false;
                        for(int i=0;i<p->m_nChildren;++i) bladeParent|=p->m_paChildren[i]->m_pNode->AsMdlNodeLightsaber()!=nullptr;
                        if(mesh && bladeParent) {
                            ++hilts;
                            auto* other=preview?preview->GetPart(p->m_pNode->TranslateNodeName()):nullptr;
                            Vector a=(mesh->m_vMin+mesh->m_vMax)*.5f,b=a;const Vector size=mesh->m_vMax-mesh->m_vMin;
                            if(size.z>=size.x && size.z>=size.y){a.z=mesh->m_vMin.z;b.z=mesh->m_vMax.z;}
                            else if(size.y>=size.x){a.y=mesh->m_vMin.y;b.y=mesh->m_vMax.y;}
                            else {a.x=mesh->m_vMin.x;b.x=mesh->m_vMax.x;}
                            const float liveLength=magnitude(PartPointWorld(p,b)-PartPointWorld(p,a));
                            const float previewLength=other?magnitude(PartPointWorld(other,b)-PartPointWorld(other,a)):0;
                            const bool same=other && other->m_pNode==p->m_pNode && std::fabs(liveLength-previewLength)<1e-4f;
                            fprintf(stderr,"K1_POISE_HILT_MATCH_%s stage=%d model=%s hilt=%s same_mesh=%d live_length=%g preview_length=%g\n",same?"PASS":"FAIL",stage,live->m_pModel->m_szName,p->m_pNode->TranslateNodeName(),other && other->m_pNode==p->m_pNode,liveLength,previewLength);
                            match=match && same;
                        }
                        for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
                    };compareHilt(compareHilt,live->m_pPart);
                    if(!match || !hilts){fail("meter_hilt_match");return;}
                }
                verifiedStage=stage;
                fprintf(stderr,"K1_POISE_METER_EQUIP_PASS stage=%d live=%s preview=%s fallback=%d percent=%d\n",
                    stage,live->m_pModel->m_szName,view.resource.CStr(),!saber,view.percent);
                if(verifiedStage==9) fprintf(stderr,"K1_POISE_METER_CAPTURE_PASS stages=10\n");
            }
        }
        return;
    }
    auto owned=npcPlayback.find(npc->m_objectId);
    if(owned!=npcPlayback.end() && (phase!=70 || trial>0)) {
        owned->second.nextThink=now+3000000;owned->second.nextAttack=now+3000000;
        owned->second.attacking=false;StopNpcMove(owned->second,npc);
    }
    if(phase==70) {
        auto* body=defender->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(owned==npcPlayback.end() || (trial==0 && !owned->second.navigator) || !K1EngageOwnsAnimation(body)) {
            if(ticks>240) fail("source_ownership");return;
        }
        // Exercise the real load-time graph cache before placing the contact
        // fixture; holding at spawn inside reach would never request navigation.
        StopNpcMove(owned->second,npc);
        owned->second.nextThink=now+3000000;owned->second.nextAttack=now+3000000;
        owned->second.attacking=false;
        const Vector contact(201.25f,100,0);
        npc->SetPosition(contact,1,1,0);owned->second.position=contact;
        owned->second.previousPhysics=contact;owned->second.physicsRemainder=0;
        const int expected=meleePoise[defender->m_objectId].percent;
        // Native recovery is checked at exactly25 below; later idle time can now
        // legitimately regenerate before the next contact trial begins.
        if((trial<2?expected!=100:expected<25 || expected>100) || !HasMeleePoise(defender)) {fail("initial_poise");return;}
        state.attacking=false;K1EngageMouse(false,true);
        aimYaw=0;movement.Clear();
        player->SetOrientation(Vector(1,0,0));npc->SetOrientation(Vector(-1,0,0));
        owned->second.parrying=false;owned->second.closeGuard=true;
        if(!clashTested) {
            clashTested=true;
            if(!ClashContactProbe(player,npc)) {fail("clash_contact_dispatch");return;}
            phase=73;ticks=0;return;
        }
        if(!K1EngageBlocksMelee(attacker->m_objectId,defender->m_objectId)) {fail("front_guard");return;}
        // A predicate query must never spend poise.
        K1EngageBlocksMelee(attacker->m_objectId,defender->m_objectId);
        if(meleePoise[defender->m_objectId].percent!=expected) {fail("predicate_spent");return;}
        health=defender->GetCurrentHitPoints(0);hits=0;nativeSamples=0;
        phase=71;ticks=0;return;
    }
    if(phase==73 && ticks>=30) {
        if(player->GetCurrentHitPoints(0)>=clashExpiryObservation.playerHP || npc->GetCurrentHitPoints(0)>=clashExpiryObservation.enemyHP) {
            fail("clash_expiry_native_damage");return;
        }
        fprintf(stderr,"K1_MELEE_EXPIRY_NATIVE_PASS player_loss=%d enemy_loss=%d events=2\n",
            clashExpiryObservation.playerHP-player->GetCurrentHitPoints(0),clashExpiryObservation.enemyHP-npc->GetCurrentHitPoints(0));
        player->SetCurrentHitPoints(clashExpiryObservation.playerBase);npc->SetCurrentHitPoints(clashExpiryObservation.enemyBase);
        meleePoise[player->m_objectId]=MeleePoise();meleePoise[npc->m_objectId]=MeleePoise();
        phase=70;ticks=0;return;
    }
    if(phase==71 && ticks>=12) {
        const int initial=meleePoise[defender->m_objectId].percent;
        fprintf(stderr,"K1_POISE_CONTACT_STATE trial=%d active=%d held=%d guard=%d eligible=%d broken=%d native=%d round=%d commandable=%d mode=%d front=%g aim=%g\n",
            trial,K1EngageActive(),state.rightHeld,state.Guarding(),HasMeleePoise(defender),PoiseBroken(defender),NativeStatusAnimation(defender),
            defender->m_pCombatRound->m_bRoundStarted,defender->m_bCommandable,int(state.mode),
            dot(defender->m_orientation,attacker->m_position-defender->m_position),aimYaw);
        const int loss=GuardPoiseDamage(attacker);
        if(initial<=loss){
            if(K1EngageOwnsPlayer(attacker->m_objectId))state.chainSwings=PlayerChainLimit();
            else npcPlayback[attacker->m_objectId].chainSwings=3;
        }
        if(!ContactDamage(attacker,defender)) {fail("contact");return;}
        if(initial<=loss){
            const int remaining=K1EngageOwnsPlayer(attacker->m_objectId)?state.chainSwings:npcPlayback[attacker->m_objectId].chainSwings;
            if(remaining!=0){fail("poise_break_chain_counter");return;}
            fprintf(stderr,"K1_CHAIN_BREAK_RESET_PASS attacker=%llu previous=%d current=0\n",attacker->m_objectId,K1EngageOwnsPlayer(attacker->m_objectId)?PlayerChainLimit():maxChainSwings);
        }
        ++hits;const int expected=std::max(0,initial-loss);
        if(meleePoise[defender->m_objectId].percent!=expected || defender->GetCurrentHitPoints(0)!=health) {fail("one_loss_per_block");return;}
        fprintf(stderr,"K1_POISE_CONTACT_PASS actor=%llu trial=%d hits=%d percent=%d hp_unchanged=1\n",defender->m_objectId,trial,hits,expected);
        if(expected) {ticks=0;return;}
        if(!PoiseBroken(defender) || defender->m_nState!=EFFECT_STATE_STUNNED ||
            K1EngageBlocksMelee(attacker->m_objectId,defender->m_objectId)) {fail("guard_break_state");return;}
        began=now;exposedHit=false;phase=72;ticks=0;return;
    }
    if(phase==72) {
        if(now-began<2500000 && defender->m_nState!=EFFECT_STATE_STUNNED) {fail("native_stun_ended_early");return;}
        auto* client=defender->GetClientCreature();auto* body=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        // Native ambient state2 maps to10058 in CSWSCreature::SetAnimation.
        const auto clip=client->GetAnimationBase()->GetAnimationName(10058);
        float time=0,length=0;
        if(defender->m_nState==EFFECT_STATE_STUNNED && body && !IsSourceBody(body) && !body->m_pAddInModel &&
            !K1EngageOwnsAnimation(body) && body->GetAnimation(const_cast<char*>(clip.CStr()),&time,&length) && time>0) {
            ++nativeSamples;
            if(nativeSamples==1) fprintf(stderr,"K1_POISE_NATIVE_STUN actor=%llu clip=%s time=%g\n",defender->m_objectId,clip.CStr(),time);
        }
        if(trial==2 && !planningTested && ticks>=10) {
            K1EngagePauseInput();planningTested=true;phase=77;ticks=0;return;
        }
        if(!exposedHit && trial>=2 && now-began>=poiseStunUs-600000) {
            if(!ContactDamage(attacker,defender)) {fail("exposed_contact");return;}
            exposedHit=true;
        }
        if(!PoiseBroken(defender)) {
            if(nativeSamples<2 || meleePoise[defender->m_objectId].percent!=25 ||
                (trial<2?defender->GetCurrentHitPoints(0)!=health:defender->GetCurrentHitPoints(0)>=health) ||
                now-began<2500000 || defender->m_nState!=EFFECT_STATE_NONE) {fail("recovery_or_vulnerability");return;}
            fprintf(stderr,"K1_POISE_RECOVERY_PASS actor=%llu trial=%d samples=%d elapsed_us=%lld percent=25 vulnerable=%d\n",
                defender->m_objectId,trial,nativeSamples,now-began,trial>=2);
            recoveryObserved=now;recoveryDeadline=meleePoise[defender->m_objectId].nextTick;
            phase=78;ticks=0;
        } else if(ticks>300) fail("native_stun_timeout");
        return;
    }
    if(phase==77 && ticks>=30) {
        if(!K1EngagePlanning() || !PoiseBroken(defender) || meleePoise[defender->m_objectId].percent!=0 ||
            defender->m_nState!=EFFECT_STATE_STUNNED) {fail("planning_reset_or_ownership");return;}
        if(!K1EngageEnter()) {fail("planning_reentry");return;}
        fprintf(stderr,"K1_POISE_PLANNING_PASS native_stun_retained=1 percent=0\n");phase=72;ticks=0;return;
    }
    if(phase==78) {
        const auto& p=meleePoise[defender->m_objectId];
        if(now<recoveryDeadline && p.percent!=25) {fail("recovery_early_tick");return;}
        if(p.percent>25) {
            if(p.percent!=26 || now<recoveryDeadline || now-recoveryDeadline>100000 ||
                (trial<2 && now-recoveryObserved>PoiseTickInterval(p)+100000)) {fail("recovery_tick");return;}
            fprintf(stderr,"K1_POISE_RECOVERY_REGEN_PASS trial=%d elapsed_us=%lld hit_delay=%d percent=26\n",trial,now-recoveryObserved,trial>=2);
            if(++trial<4) {phase=70;ticks=0;return;}
            if(!EquipFixtureWeapon(npc,"g_w_blstrpstl001")) {fail("ranged_fixture");return;}
            phase=74;ticks=0;
        } else if(ticks>180) fail("recovery_regen_timeout");
        return;
    }
    if(phase==74 && ticks>=60) {
        if(HasMeleePoise(npc) || K1EngageGuardContact(player->m_objectId,npc->m_objectId)) {fail("ranged_has_poise");return;}
        fprintf(stderr,"K1_POISE_RANGED_PASS actor=%llu\n",npc->m_objectId);
        if(!EquipFixtureSaber(npc)) {fail("clash_saber_fixture");return;}
        phase=76;ticks=0;return;
    }
    if(phase==76) {
        // Server equip precedes client animation weapon-class replication.
        if(!HasMeleePoise(npc)) {if(ticks>180) fail("clash_saber_not_ready");return;}
        auto activeNpc=npcPlayback.find(npc->m_objectId);
        if(activeNpc==npcPlayback.end()) {fail("clash_low_owner");return;}
        state.attacking=true;state.chainSwings=2;
        activeNpc->second.attacking=true;activeNpc->second.ranged=false;activeNpc->second.chainSwings=2;
        meleePoise[player->m_objectId].percent=10;meleePoise[npc->m_objectId].percent=6;
        DelayPoiseRegeneration(meleePoise[player->m_objectId],now);DelayPoiseRegeneration(meleePoise[npc->m_objectId],now);
        const int playerHP=player->GetCurrentHitPoints(0),npcHP=npc->GetCurrentHitPoints(0);
        MeleeContact intercepted={npc->m_objectId,player->m_objectId,activeNpc->second.serial};
        playerAttackStartedAt=now;activeNpc->second.attackStartedAt=now;
        ResolveMeleeContacts({intercepted,intercepted});
        if(meleePoise[player->m_objectId].percent!=0 || meleePoise[npc->m_objectId].percent!=0 ||
            !PoiseBroken(player) || !PoiseBroken(npc) ||
            state.attacking || state.chainSwings || activeNpc->second.attacking || activeNpc->second.chainSwings ||
            player->GetCurrentHitPoints(0)!=playerHP || npc->GetCurrentHitPoints(0)!=npcHP) {fail("clash_low_poise");return;}
        fprintf(stderr,"K1_MELEE_CLASH_LOW_PASS poise=0,0 native_stun=1 hp=unchanged chains=0,0\n");
        phase=79;ticks=0;began=now;
    }
    if(phase==79) {
        if(ticks==2 && (player->m_nState!=EFFECT_STATE_STUNNED || npc->m_nState!=EFFECT_STATE_STUNNED)) {fail("clash_native_stun");return;}
        if(ticks>=2 && !PoiseBroken(player) && !PoiseBroken(npc)) {
            if(meleePoise[player->m_objectId].percent<25 || meleePoise[npc->m_objectId].percent<25) {fail("clash_low_recovery");return;}
            fprintf(stderr,"K1_MELEE_CLASH_STUN_RECOVER_PASS percent=25\n");
            if(std::getenv("K1_ENGAGE_POISE_REGEN_TEST")) {phase=80;ticks=0;return;}
            npc->SetCurrentHitPoints(0);phase=75;ticks=0;
        }
        return;
    }
    if(phase==75 && ticks>=30) {
        if(state.mode!=K1EngageState::Mode::Exploration || !meleePoise.empty()) {fail("victory_cleanup");return;}
        fprintf(stderr,"K1_POISE_PASS trials=4 native_stun=1 exposed_damage=1 recovery=1\n");phase=-1;quit();
    }
}
void ClashArenaProbeTick(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* enemy) {
    static int trial=0,playerHP=0,enemyHP=0;
    static long long started=0;
    static unsigned long long before=0;
    static bool playerSent=false;
    auto quit=[] {if(FILE* f=fopen("clash-input.txt","w")){fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* why){fprintf(stderr,"K1_ENGAGE_CLASH_FAIL trial=%d phase=%d reason=%s\n",trial,phase,why);phase=-1;quit();};
    if(!enemy) {fail("missing_enemy");return;}
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    if(phase==2) {
        enemy->m_hitPoints=1000;enemy->SetCurrentHitPoints(1000);
        if(!K1EngageEnter()) {fail("enter");return;}
        phase=110;ticks=0;return;
    }
    auto found=npcPlayback.find(enemy->m_objectId);
    if(phase==113) {
        if(state.mode==K1EngageState::Mode::Exploration) {
            if(!sceneMeleeContacts.empty()) {fail("cleanup");return;}
            fprintf(stderr,"K1_MELEE_CLASH_LIVE_PASS trials=2 timed=1 eligibility_us=500000 delay_us=0 hp=unchanged poise=90,90 chains=0,0 cleanup=1\n");
            phase=-1;quit();
        } else if(ticks>180) fail("victory_cleanup");
        return;
    }
    if(found==npcPlayback.end()) {if(ticks>240) fail("source_owner");return;}
    auto& npc=found->second;
    npc.nextThink=now+60000000;StopNpcMove(npc,enemy);
    npc.parryReady=now+60000000;npc.parrying=false;
    if(phase==110) {
        if(!npc.navigator) {
            JkaWorld world(enemy);
            if(!LoadNpcNavigation(npc,enemy,world)) {fail("cached_navigation");return;}
        }
        npc.nextAttack=now+60000000;npc.attacking=false;npc.chainSwings=0;
        state.attacking=state.attackRequested=state.holdChain=false;state.chainSwings=0;
        state.leftHeld=state.rightHeld=false;movement.Clear();
        keyW=keyS=keyA=keyD=keyShift=false;aimYaw=0;
        player->SetPosition(Vector(200,100,0),1,1,0);
        player->GetClientCreature()->SetPrecalculatedPosition(Vector(200,100,0));
        enemy->SetPosition(Vector(201.1f,100,0),1,1,0);
        player->SetOrientation(Vector(1,0,0));enemy->SetOrientation(Vector(-1,0,0));
        npc.facingYaw=npc.previousFacingYaw=3.1415926536f;
        npc.position=enemy->m_position;npc.previousPhysics=npc.position;npc.physicsRemainder=0;
        player->ClearAllActions(1);enemy->ClearAllActions(1);
        player->m_pCombatRound->EndCombatRound(1);enemy->m_pCombatRound->EndCombatRound(1);
        player->AddToVisibleList(enemy->m_objectId,1,0,0,0);enemy->AddToVisibleList(player->m_objectId,1,0,0,0);
        meleePoise[player->m_objectId]=MeleePoise();meleePoise[enemy->m_objectId]=MeleePoise();
        sceneMeleeContacts.clear();phase=111;ticks=0;return;
    }
    if(phase==111) {
        npc.nextAttack=now+60000000;
        if(ticks<60) return;
        playerHP=player->GetCurrentHitPoints(0);enemyHP=enemy->GetCurrentHitPoints(0);
        before=meleeClashes;started=now;playerSent=trial==0;
        if(playerSent) {K1EngageMouse(true,true);K1EngageMouse(true,false);npc.nextAttack=now+250000;}
        else npc.nextAttack=now;
        fprintf(stderr,"K1_MELEE_CLASH_LIVE_START trial=%d first=%s offset_us=250000\n",trial,trial?"npc":"player");
        phase=112;ticks=0;return;
    }
    if(phase==112) {
        if(!playerSent && now-started>=250000) {K1EngageMouse(true,true);K1EngageMouse(true,false);playerSent=true;}
        if(meleeClashes>before) {
            if(meleeClashes!=before+1 || state.attacking || state.chainSwings || npc.attacking || npc.chainSwings ||
                meleePoise[player->m_objectId].percent!=90 || meleePoise[enemy->m_objectId].percent!=90 ||
                player->GetCurrentHitPoints(0)!=playerHP || enemy->GetCurrentHitPoints(0)!=enemyHP) {fail("result");return;}
            fprintf(stderr,"K1_MELEE_CLASH_LIVE_CONTACT_PASS trial=%d gap_us=%lld player_hp=%d enemy_hp=%d\n",trial,lastClashGap,playerHP,enemyHP);
            npc.nextAttack=now+60000000;
            if(++trial<2) {phase=110;ticks=0;return;}
            enemy->SetCurrentHitPoints(0);phase=113;ticks=0;return;
        }
        if(now-started>2500000) {
            fprintf(stderr,"K1_MELEE_CLASH_LIVE_TRACE trial=%d player_serial=%d npc_serial=%d pending=%zu hp=%d,%d\n",
                trial,attackSerial,npc.serial,sceneMeleeContacts.size(),player->GetCurrentHitPoints(0),enemy->GetCurrentHitPoints(0));
            fail("no_geometric_clash");
        }
    }
}
void ParryArenaProbeTick(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* enemy) {
    static Vector blockPosition;
    static int playerPoiseBefore=100;
    static bool hadParrySetting=false,playerRateChecked=false,npcRateChecked=false,zeroChanceChecked=false;
    static int savedParrySetting=0;
    static int trial=0,stable=0,playerHP=0,enemyHP=0,matches=0,blocks=0,bare=0;
    static long long started=0;
    static unsigned long long before=0;
    static bool eligibility=false;
    static bool npcCooldown=false,playerCooldown=false;
    static long long cooldownUntil=0;
    static int cooldownSerial=0,blockedSamples=0;
    static unsigned long long beforeNpcParries=0;
    auto quit=[] {if(FILE* f=fopen("parry-input.txt","w")){fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* why){fprintf(stderr,"K1_ENGAGE_PARRY_FAIL trial=%d phase=%d reason=%s\n",trial,phase,why);phase=-1;quit();};
    if(!enemy) {fail("missing_enemy");return;}
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    if(phase==2) {
        if(!ParryRulesProbe() || !NpcParryPolicyProbe(enemy)) {fail("rules");return;}
        auto* setting=enemy->m_scriptVars.MatchIndex(CExoString(npcParryPercentVariable),CSWSScriptVarTable::VARTYPE_INTEGER,0);
        hadParrySetting=setting!=nullptr;savedParrySetting=setting?setting->m_nInt:0;
        enemy->m_hitPoints=1000;enemy->SetCurrentHitPoints(1000);
        if(!K1EngageEnter()) {fail("enter");return;}
        phase=120;ticks=0;return;
    }
    auto found=npcPlayback.find(enemy->m_objectId);
    const int direction=trial<7?trial:(trial==10?0:1);
    const int wanted=trial==7?3:(trial==8?-1:(trial==10?3:ParrySectorForAttack(direction)));
    const bool raised=trial!=9,shouldParry=trial<7 || trial>=10;
    const bool baton=NativeNpcRightItem(player) && NativeNpcRightItem(player)->m_baseItem==BASE_ITEM_STUN_BATON;
    if(phase==124) {
        if(state.mode==K1EngageState::Mode::Exploration && ticks>=3) {
            if(std::getenv("K1_ENGAGE_PARRY_PLAYER_ONLY")) {
                if(!sceneMeleeContacts.empty() || !hudOwner || hudOwner->m_parryReticle.m_tail.bVisible || parryRenderFailed ||
                   parryRenderChecks<12 || matches!=9 || blocks!=2 || bare!=1 || !npcCooldown || !npcRateChecked){fail("player_only_cleanup");return;}
                fprintf(stderr,"K1_DUAL_PLAYER_PARRY_PASS cases=12 matched=9 blocks=2 unguarded=1 npc_cooldown=1 render=1 cleanup=1\n");phase=-1;quit();return;
            }
            if(!sceneMeleeContacts.empty() || !hudOwner || hudOwner->m_parryReticle.m_tail.bVisible ||
                parryRenderFailed || parryRenderChecks<12 || matches!=9 || blocks!=(baton?0:2) || bare!=(baton?3:1) || !npcCooldown || !playerCooldown ||
                !playerRateChecked || !npcRateChecked || !zeroChanceChecked) {fail("cleanup_or_counts");return;}
            fprintf(stderr,"K1_DIRECTIONAL_PARRY_LIVE_PASS matched=7 overhead_overlap=1 wrong=1 neutral=1 unguarded=1 break=1 npc_cooldown=1 player_cooldown=1 render=%d cleanup=1\n",parryRenderChecks);
            if(baton)fprintf(stderr,"K1_BATON_PARRY_ONLY_PASS matched=9 failed_guard_hits=2 unguarded_hits=1\n");
            phase=-1;quit();
        } else if(ticks>180) fail("victory_cleanup");
        return;
    }
    if(phase==123) {
        if(!npcRateChecked && found!=npcPlayback.end() && found->second.attacking && found->second.overlay) {
            if(!LiveMeleeRate(found->second.body,found->second.overlay,npcMeleeSwingRate)) {fail("npc_playback_rate");return;}
            npcRateChecked=true;fprintf(stderr,"K1_NPC_SWING_RATE_PASS rate=%g\n",npcMeleeSwingRate);
        }
        const int playerPoise=meleePoise[player->m_objectId].percent,enemyPoise=meleePoise[enemy->m_objectId].percent;
        const bool parried=directionalParries>before;
        const bool touched=parried || playerPoise!=playerPoiseBefore || player->GetCurrentHitPoints(0)<playerHP;
        if(touched) {
            if(shouldParry) {
                const bool stopped=found==npcPlayback.end()?trial==11:!found->second.attacking && !found->second.chainSwings;
                if(!parried || directionalParries!=before+1 || playerPoise!=std::min(100,playerPoiseBefore+ParryPoiseReward(player)) || enemyPoise!=(trial==11?0:75) || !stopped ||
                    player->GetCurrentHitPoints(0)!=playerHP || enemy->GetCurrentHitPoints(0)!=enemyHP ||
                    (trial==11 && (enemy->m_nState!=EFFECT_STATE_STUNNED || !PoiseBroken(enemy)))) {fail("parry_result");return;}
                ++matches;
                if(trial==2 || trial==3) fprintf(stderr,"K1_PARRY_REWARD_PLAYER_PASS before=%d after=%d capped=%d\n",playerPoiseBefore,playerPoise,trial==3);
            } else if(raised && !baton) {
                if(parried || playerPoise!=80 || enemyPoise!=100 || player->GetCurrentHitPoints(0)!=playerHP ||
                    found==npcPlayback.end() || !found->second.attacking) {fail("wrong_side_block");return;}
                ++blocks;
            } else {
                if(parried || playerPoise!=100 || enemyPoise!=100 || player->GetCurrentHitPoints(0)>=playerHP) {fail("unguarded_hit");return;}
                ++bare;
            }
            fprintf(stderr,"K1_PARRY_CONTACT_PASS trial=%d attack=%d selected=%d guarded=%d parry=%d poise=%d,%d hp=%d,%d\n",
                trial,direction,wanted,raised,parried,playerPoise,enemyPoise,player->GetCurrentHitPoints(0),enemy->GetCurrentHitPoints(0));
            if(trial==0) {
                cooldownUntil=meleePoise[enemy->m_objectId].parriedUntil;cooldownSerial=found->second.serial;blockedSamples=0;
                if(ParryAttackReady(enemy->m_objectId,cooldownUntil-1) || !ParryAttackReady(enemy->m_objectId,cooldownUntil)) {fail("npc_cooldown_boundary");return;}
                phase=125;ticks=0;return;
            }
            if(found!=npcPlayback.end()) {found->second.nextAttack=now+60000000;found->second.attacking=false;}
            if(trial==1) {keyS=keyShift=true;blockPosition=player->m_position;}
            started=now;phase=129;ticks=0;return;
        }
        if(now-started>2500000) {
            const auto aim=ParryAimAt(enemy);
            fprintf(stderr,"K1_PARRY_NO_CONTACT attack=%d selected=%d aim=%g,%g samples=%d sweeps=%d nearest=%g\n",direction,aim.sector,
                aim.right,aim.up,found==npcPlayback.end()?-1:found->second.bladeSamples,found==npcPlayback.end()?-1:found->second.sweeps,
                found==npcPlayback.end()?-1.f:found->second.closestTip);
            fail("no_body_contact");
        }
        return;
    }
    if(phase==125) {
        if(found==npcPlayback.end()) {fail("npc_cooldown_owner");return;}
        auto& npc=found->second;npc.nextThink=now+60000000;StopNpcMove(npc,enemy);
        // Force the usual attack schedule ready to prove the independent
        // per-actor gate, rather than just measuring nextAttack's deadline.
        npc.nextAttack=now;
        if(now<cooldownUntil) {
            if(npc.attacking || npc.serial!=cooldownSerial || ParryAttackReady(npc.id,now)) {fail("npc_early_attack");return;}
            ++blockedSamples;
        } else if(npc.attacking && npc.serial==cooldownSerial+1) {
            if(now>cooldownUntil+100000 || blockedSamples<2) {fail("npc_late_attack");return;}
            fprintf(stderr,"K1_PARRY_NPC_COOLDOWN_PASS blocked=%d elapsed_us=%lld boundary=1000000\n",blockedSamples,now-(cooldownUntil-parryAttackDelayUs));
            const auto& observation=blockObservations[player->m_objectId];
            if(observation.reaction!=0 || !observation.settled || !observation.finite || observation.maximumGrip>=.0002f) {fail("first_block_pose");return;}
            npcCooldown=true;npc.nextAttack=now+60000000;npc.attacking=false;
            ++trial;phase=120;ticks=0;return;
        } else if(now>cooldownUntil+500000) fail("npc_restart_missing");
        return;
    }
    if(phase==126) {
        if(PoiseBroken(enemy) || NativeStatusAnimation(enemy) || found==npcPlayback.end()) {if(ticks>240) fail("npc_stun_recovery");return;}
        auto& npc=found->second;npc.nextThink=npc.nextAttack=now+60000000;StopNpcMove(npc,enemy);
        npc.attacking=false;npc.parrying=false;npc.parryReady=0;npc.threatSince=0;
        state.ClearInputs();movement.Clear();K1EngageMouse(false,false);
        meleePoise[player->m_objectId]=MeleePoise();meleePoise[enemy->m_objectId]=MeleePoise();
        meleePoise[enemy->m_objectId].percent=40;DelayPoiseRegeneration(meleePoise[enemy->m_objectId],now);
        enemy->m_scriptVars.SetInt(CExoString(npcParryPercentVariable),100,0);
        beforeNpcParries=npcTimedParries;playerHP=player->GetCurrentHitPoints(0);enemyHP=enemy->GetCurrentHitPoints(0);
        K1EngageMouse(true,true);K1EngageMouse(true,false);started=now;phase=127;ticks=0;return;
    }
    if(phase==127) {
        if(found==npcPlayback.end()) {fail("npc_timed_owner");return;}
        auto& npc=found->second;npc.nextThink=npc.nextAttack=now+60000000;StopNpcMove(npc,enemy);
        if(!playerRateChecked && state.attacking && currentOverlay) {
            if(!LiveMeleeRate(loadedBody,currentOverlay,PlayerMeleeRate())) {fail("player_playback_rate");return;}
            playerRateChecked=true;fprintf(stderr,"K1_PLAYER_SWING_RATE_PASS rate=%g npc_rate=%g ratio=%g\n",
                PlayerMeleeRate(),npcMeleeSwingRate,PlayerMeleeRate()/npcMeleeSwingRate);
        }
        if(npcTimedParries>beforeNpcParries) {
            if(npcTimedParries!=beforeNpcParries+1 || state.attacking || state.chainSwings ||
                meleePoise[player->m_objectId].percent!=100-ParriedPoiseLoss(player) || meleePoise[enemy->m_objectId].percent!=40+ParryPoiseReward(enemy) ||
                player->GetCurrentHitPoints(0)!=playerHP || enemy->GetCurrentHitPoints(0)!=enemyHP) {fail("npc_timed_result");return;}
            fprintf(stderr,"K1_PARRY_REWARD_NPC_PASS before=40 after=%d\n",meleePoise[enemy->m_objectId].percent);
            cooldownUntil=meleePoise[player->m_objectId].parriedUntil;cooldownSerial=attackSerial;blockedSamples=0;
            if(ParryAttackReady(player->m_objectId,cooldownUntil-1) || !ParryAttackReady(player->m_objectId,cooldownUntil)) {fail("player_cooldown_boundary");return;}
            npc.parryReady=now+60000000;phase=128;ticks=0;return;
        }
        if(now-started>2500000) fail("npc_timed_parry_missing");
        return;
    }
    if(phase==128) {
        if(found==npcPlayback.end()) {fail("player_cooldown_npc_owner");return;}
        if(found!=npcPlayback.end()) {
            found->second.nextThink=found->second.nextAttack=now+60000000;found->second.parryReady=now+60000000;
            StopNpcMove(found->second,enemy);
        }
        if(now<cooldownUntil) {
            if(state.attacking || attackSerial!=cooldownSerial || ParryAttackReady(player->m_objectId,now)) {fail("player_early_attack");return;}
            ++blockedSamples;
        } else if(state.attacking && attackSerial==cooldownSerial+1) {
            if(now>cooldownUntil+100000 || blockedSamples<2) {fail("player_late_attack");return;}
            fprintf(stderr,"K1_PARRY_PLAYER_COOLDOWN_PASS blocked=%d elapsed_us=%lld boundary=1000000 input=fresh_clicks\n",blockedSamples,now-(cooldownUntil-parryAttackDelayUs));
            const auto& observation=blockObservations[enemy->m_objectId];
            if(observation.reaction!=0 || !observation.settled || !observation.finite || observation.maximumGrip>=.0002f) {fail("npc_block_pose");return;}
            fprintf(stderr,"K1_BLOCK_REACTION_NPC_PASS samples=%d settled=%d maximum_grip=%g\n",observation.samples,observation.settled,observation.maximumGrip);
            playerCooldown=true;
            enemy->m_scriptVars.SetInt(CExoString(npcParryPercentVariable),0,0);
            // Start the zero-chance trial independently of the previous reward.
            meleePoise[enemy->m_objectId].percent=50;DelayPoiseRegeneration(meleePoise[enemy->m_objectId],now);
            found->second.parryReady=0;found->second.threatSince=0;found->second.parrying=false;
            started=now;phase=130;ticks=0;return;
        } else if(now>cooldownUntil+500000) {fail("player_restart_missing");return;}
        K1EngageMouse(true,true);K1EngageMouse(true,false);
        return;
    }
    if(phase==130) {
        if(found==npcPlayback.end()) {fail("zero_chance_owner");return;}
        auto& npc=found->second;npc.nextThink=npc.nextAttack=now+60000000;StopNpcMove(npc,enemy);
        const auto& defender=meleePoise[enemy->m_objectId];
        if(defender.percent!=50) {
            if(defender.percent!=30 || npcTimedParries!=beforeNpcParries+1 || defender.parryRollSerial!=attackSerial ||
                defender.parryRollSuccess || player->GetCurrentHitPoints(0)!=playerHP || enemy->GetCurrentHitPoints(0)!=enemyHP) {fail("zero_chance_contact");return;}
            fprintf(stderr,"K1_NPC_PARRY_ZERO_CONTACT_PASS percent=0 poise=50,30 normal_block=1\n");zeroChanceChecked=true;
            if(hadParrySetting) enemy->m_scriptVars.SetInt(CExoString(npcParryPercentVariable),savedParrySetting,0);
            else enemy->m_scriptVars.DestroyInt(CExoString(npcParryPercentVariable));
            enemy->SetCurrentHitPoints(0);phase=124;ticks=0;return;
        }
        if(now-started>2500000) fail("zero_chance_contact_missing");
        return;
    }
    if(phase==129) {
        if(found!=npcPlayback.end()) {
            found->second.nextThink=found->second.nextAttack=now+60000000;StopNpcMove(found->second,enemy);
        }
        if(now-started<400000) return;
        const auto& poise=meleePoise[player->m_objectId];
        if(raised && (!baton || shouldParry)) {
            const auto& observation=blockObservations[player->m_objectId];
            const int block=BlockReactionForAttack(direction);
            if(poise.blockReaction!=block || observation.started!=poise.blockStarted || observation.reaction!=block ||
                !observation.settled || !observation.finite || observation.maximumGrip>=.0002f ||
                BlockReference(currentOverlay) || ActiveBlockReaction(player->m_objectId,poise.blockStarted+249999)!=block ||
                ActiveBlockReaction(player->m_objectId,poise.blockStarted+250000)!=-1) {fail("block_pose_or_return");return;}
            fprintf(stderr,"K1_BLOCK_REACTION_PLAYER_PASS trial=%d block=%s samples=%d settled=%d maximum_grip=%g return=1 boundary=250000\n",
                trial,blockFull[block],observation.samples,observation.settled,observation.maximumGrip);
            if(trial==1) {
                const float distance=magnitude(player->m_position-blockPosition);
                if(!observation.moving || distance<.3f) {fail("walking_block");return;}
                fprintf(stderr,"K1_BLOCK_REACTION_WALK_PASS distance=%g\n",distance);keyS=keyShift=false;movement.Clear();
            }
        } else if(poise.blockReaction!=-1) {fail("unguarded_block_animation");return;}
        if(++trial<12) {phase=120;ticks=0;return;}
        if(std::getenv("K1_ENGAGE_PARRY_PLAYER_ONLY")){enemy->SetCurrentHitPoints(0);phase=124;ticks=0;return;}
        phase=126;ticks=0;return;
    }
    if(found==npcPlayback.end()) {if(ticks>240) fail("source_owner");return;}
    auto& npc=found->second;
    arenaForce.next=now+60000000;
    npc.nextThink=now+60000000;StopNpcMove(npc,enemy);npc.parryReady=now+60000000;npc.parrying=false;
    npc.nextAttack=now+60000000;
    if(phase==120) {
        if(!npc.navigator) {JkaWorld world(enemy);if(!LoadNpcNavigation(npc,enemy,world)) {fail("cached_navigation");return;}}
        npc.attacking=false;npc.chainSwings=0;state.ClearInputs();movement.Clear();
        keyW=keyS=keyA=keyD=keyShift=false;aimYaw=0;aimPitch=0;
        player->SetPosition(Vector(200,100,0),1,1,0);player->GetClientCreature()->SetPrecalculatedPosition(Vector(200,100,0));
        enemy->SetPosition(Vector(201.05f,100,0),1,1,0);
        player->SetOrientation(Vector(1,0,0));enemy->SetOrientation(Vector(-1,0,0));
        npc.facingYaw=npc.previousFacingYaw=3.1415926536f;npc.position=enemy->m_position;
        npc.previousPhysics=npc.position;npc.physicsRemainder=0;
        player->ClearAllActions(1);enemy->ClearAllActions(1);
        player->m_pCombatRound->EndCombatRound(1);enemy->m_pCombatRound->EndCombatRound(1);
        player->AddToVisibleList(enemy->m_objectId,1,0,0,0);enemy->AddToVisibleList(player->m_objectId,1,0,0,0);
        meleePoise[player->m_objectId]=MeleePoise();meleePoise[enemy->m_objectId]=MeleePoise();
        if(trial==11) meleePoise[enemy->m_objectId].percent=25;
        sceneMeleeContacts.clear();K1EngageMouse(false,raised);
        stable=0;phase=121;ticks=0;return;
    }
    if(phase==121) {
        if(ticks<45) return;
        const auto aim=ParryAimAt(enemy);
        int w=0,h=0;AurGUIGetScreenSize(w,h);
        const float radius=wanted<0?0.f:40.f*h/720.f;
        const float angle=wanted<0?0.f:(wanted+.5f)*1.5707963268f;
        const float x=radius*std::sin(angle),y=radius*std::cos(angle);
        const float error=std::hypot(x-aim.right,y-aim.up);
        if(aim.target==enemy->m_objectId && aim.sector==wanted && error<4.f*h/720.f) ++stable;
        else {
            stable=0;
            K1EngageLook(int(std::lround((x-aim.right)*.3f)),int(std::lround(-(y-aim.up)*.3f)));
        }
        if(stable>=3) {
            if(!eligibility) {
                bool ok=true;const Vector original=enemy->m_position;
                // Use actual camera projection and eligibility at off-center
                // positions on both sides, including beyond the old aim cone.
                // The near fixture's camera distance/front gate can exclude
                // targets before the wider screen boundary: test that predicate
                // directly as well, without weakening the front/range gates.
                int expandedSides=0;
                const float boundary=h*parryAimRadiusHeight;
                const bool boundaryOk=ParryAimWithinArea(boundary,0,h) &&
                    !ParryAimWithinArea(boundary+.01f,0,h) &&
                    ParryAimWithinArea(0,-boundary,h) && !ParryAimWithinArea(0,-boundary-.01f,h);
                const float aimDistance=std::hypot(original.x-player->m_position.x,original.y-player->m_position.y);
                for(int sign:{-1,1}) {
                    bool expanded=false;
                    for(int degrees=5;degrees<=60;++degrees) {
                        const float angle=sign*degrees*.01745329252f;
                        enemy->SetPosition(player->m_position+Vector(std::cos(angle)*aimDistance,std::sin(angle)*aimDistance,0),1,1,0);
                        const auto off=ParryAimAt(enemy);
                        const float radius=std::hypot(off.right,off.up);
                        if(radius>h*.2f && radius<=h*parryAimRadiusHeight && off.target==enemy->m_objectId) expanded=true;
                    }
                    expandedSides+=expanded;
                }
                ok=ok && expandedSides==2 && boundaryOk;
                fprintf(stderr,"K1_PARRY_AIM_AREA_%s radius_height=%g expanded_sides=%d boundary=%d\n",
                    ok?"PASS":"FAIL",parryAimRadiusHeight,expandedSides,boundaryOk);
                enemy->SetPosition(original,1,1,0);
                const float oldRange=K1EngageEnemyMeleeRange(enemy)+player->m_pPathfindInfo->m_fCrePersonalSpace;
                const Vector toward=normalize(original-player->m_position);
                enemy->SetPosition(player->m_position+toward*(oldRange+.5f),1,1,0);
                ok=ok && ParryAimAt(enemy).target==enemy->m_objectId;
                enemy->SetPosition(player->m_position+toward*(oldRange+parryMarkerExtraRange+.01f),1,1,0);
                ok=ok && ParryAimAt(enemy).target==OBJECT_INVALID;
                enemy->SetPosition(Vector(210,100,0),1,1,0);ok=ok && ParryAimAt(enemy).target==OBJECT_INVALID;
                enemy->SetPosition(Vector(199,100,0),1,1,0);ok=ok && ParryAimAt(enemy).target==OBJECT_INVALID;
                enemy->SetPosition(original,1,1,0);
                npc.ranged=true;ok=ok && ParryAimAt(enemy).target==OBJECT_INVALID;npc.ranged=false;
                const bool native=nativeActionBody;nativeActionBody=true;ok=ok && ParryAimAt(enemy).target==OBJECT_INVALID;nativeActionBody=native;
                const auto mode=state.mode;state.mode=K1EngageState::Mode::Planning;ok=ok && ParryAimAt(enemy).target==OBJECT_INVALID;state.mode=mode;
                if(!ok) {fail("eligibility");return;}
                eligibility=true;fprintf(stderr,"K1_PARRY_ELIGIBILITY_PASS range=1 extra_range=%g beyond=1 rear=1 ranged=1 native=1 planning=1\n",parryMarkerExtraRange);
            }
            parryRenderRequested=true;
            if(trial==1 || trial==7 || trial==8) if(FILE* f=fopen("parry-input.txt","w")){fprintf(f,"shot:parry-%d\n",trial);fclose(f);}
            phase=122;ticks=0;return;
        }
        if(ticks>240) {fprintf(stderr,"K1_PARRY_AIM_TRACE target=%llu sector=%d desired=%d aim=%g,%g error=%g\n",aim.target,aim.sector,wanted,aim.right,aim.up,error);fail("aim_selection");}
    } else if(phase==122) {
        if(parryRenderRequested) {if(ticks>120) fail("reticle_not_drawn");return;}
        if(parryRenderFailed || !hudOwner || hudOwner->m_parryReticle.sector!=wanted || hudOwner->m_parryReticle.guarding!=raised) {fail("reticle_render");return;}
        before=directionalParries;playerHP=player->GetCurrentHitPoints(0);enemyHP=enemy->GetCurrentHitPoints(0);
        playerPoiseBefore=trial==2?50:(trial==3?95:100);
        meleePoise[player->m_objectId].percent=playerPoiseBefore;
        DelayPoiseRegeneration(meleePoise[player->m_objectId],now);
        if(trial==11) {
            meleePoise[enemy->m_objectId].percent=25;
            DelayPoiseRegeneration(meleePoise[enemy->m_objectId],now);
        }
        parryProbeDirection=direction;started=now;npc.nextAttack=now;
        phase=123;ticks=0;
    }
}
// Real controller/camera capture, with isolated opponent decisions suppressed.
void StaffArcProbeTick(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* enemy) {
    static int trial=0,serial=0;static bool sawAttack=false;
    auto quit=[] {if(FILE* f=fopen("staff-arc-input.txt","w")) {fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* why) {fprintf(stderr,"K1_STAFF_ARC_FAIL phase=%d trial=%d reason=%s\n",phase,trial,why);staffArcTrial=-1;phase=-1;quit();};
    if(!enemy || !player) {fail("missing_actor");return;}
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    arenaForce.next=now+60000000;
    if(phase==2) {
        if(!K1EngageEnter()) {fail("entry");return;}
        enemy->SetCurrentHitPoints(10000);phase=750;ticks=0;return;
    }
    const auto found=npcPlayback.find(enemy->m_objectId);
    if(found!=npcPlayback.end()) {
        auto& npc=found->second;npc.nextThink=npc.nextAttack=now+60000000;
        npc.attacking=npc.parrying=npc.closeGuard=false;StopNpcMove(npc,enemy);
    }
    enemy->ClearAllActions(1);enemy->m_pCombatRound->EndCombatRound(1);
    if(phase==750) {
        if(ticks==1) {
            state.ClearInputs();movement.Clear();keyW=keyS=keyA=keyD=keyShift=false;
            player->SetPosition(Vector(200,100,0),1,1,0);
            if(auto* client=player->GetClientCreature()) client->SetPrecalculatedPosition(player->m_position);
            player->SetOrientation(Vector(1,0,0));aimYaw=0;aimPitch=0;
            enemy->SetPosition(Vector(204,100,0),1,1,0);
            player->ClearAllActions(1);player->m_pCombatRound->EndCombatRound(1);
        }
        if(ticks<45) return;
        if(!loadedBody || !StaffMode() || StaffBlades(Actor()).size()!=2 || state.attacking) {
            if(ticks>240) fail("pose_not_ready");return;
        }
        // Exercise real direction selection, then release direction for the
        // standing trial or keep Shift/movement for the walking trial.
        const int d=trial%7;const bool moving=trial>=7;
        keyW=d==0 || d==1 || d==2;keyS=d==5 || d==6;
        keyA=d==2 || d==4 || d==6;keyD=d==1 || d==3 || d==5;
        serial=attackSerial;staffArcTrial=trial;
        K1EngageMouse(true,true);K1EngageMouse(true,false);
        if(!moving) keyW=keyS=keyA=keyD=false;
        keyShift=moving && trial<14;sawAttack=false;phase=751;ticks=0;
        fprintf(stderr,"K1_STAFF_ARC_BEGIN trial=%d direction=%d walking=%d running=%d\n",trial,d,keyShift,trial>=14);return;
    }
    if(attackSerial>serial && state.attacking) sawAttack=true;
    if(sawAttack && !state.attacking) {
        fprintf(stderr,"K1_STAFF_ARC_END trial=%d serial=%d\n",trial,attackSerial);
        staffArcTrial=-1;state.ClearInputs();movement.Clear();keyW=keyS=keyA=keyD=keyShift=false;
        if(++trial==21) {fprintf(stderr,"K1_STAFF_ARC_PASS trials=21\n");phase=-1;quit();return;}
        phase=750;ticks=0;return;
    }
    if(ticks>240) fail("single_swing_did_not_finish");
}
// Opt-in installed-data contact probe. Move only the diagnostic weapon for each
// sample; use the actual powered beams and native posed enemy triangles.
void StaffContactProbeTick(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* enemy) {
    static int trial=0,beforeHP=0;
    static int chainSerial=0,idleFrames=0;
    static long long recoveryStart=0,recoveryTick=0;
    static int forceHitTrial=0,forceHitHP=0;
    static long long forceHitStart=0;
    if(trial>=6) trial=0;
    const int directions[]={0,1,3};const int direction=directions[trial/2],beam=trial%2;
    auto quit=[] {if(FILE* f=fopen("staff-contact-input.txt","w")) {fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* why) {fprintf(stderr,"K1_STAFF_CONTACT_FAIL phase=%d trial=%d reason=%s\n",phase,trial,why);phase=-1;quit();};
    if(!enemy) {fail("missing_enemy");return;}
    const long long now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    arenaForce.next=now+60000000;
    if(phase==2) {
        if(!ContactTimingProbe() || !PoiseRegenerationClockProbe() || !K1EngageEnter()) {fail("entry_or_timing");return;}
        enemy->SetCurrentHitPoints(1000);enemy->m_bPlot=0;
        enemy->SetPosition(player->m_position+Vector(1.2f,0,0),1,1,0);
        phase=700;ticks=0;return;
    }
    auto owned=npcPlayback.find(enemy->m_objectId);
    if(owned!=npcPlayback.end()) {
        auto& npc=owned->second;npc.nextThink=npc.nextAttack=now+60000000;
        npc.attacking=npc.parrying=npc.closeGuard=false;StopNpcMove(npc,enemy);
    }
    if(phase<706) {
        player->ClearAllActions(1);enemy->ClearAllActions(1);
        player->m_pCombatRound->EndCombatRound(1);enemy->m_pCombatRound->EndCombatRound(1);
    }
    if(phase<702){state.attacking=false;state.attackRequested=state.holdChain=false;}
    if(phase==702) {
        enemy->SetPosition(player->m_position+Vector(6,0,0),1,1,0);
        chainSerial=attackSerial;idleFrames=0;K1EngageMouse(true,false);K1EngageMouse(true,true);
        phase=703;ticks=0;return;
    }
    if(phase==703) {
        if(PlayerChainLimit()!=4 || std::fabs(PlayerMeleeRate()-1.10f)>.00001f ||
           std::fabs(playerMeleeSwingRate-1.f)>.00001f || std::fabs(npcMeleeSwingRate-.85f)>.00001f) {fail("stance_policy");return;}
        const int swings=attackSerial-chainSerial;
        if(swings>4 || state.chainSwings>4) {fail("fifth_swing");return;}
        if(state.attacking && currentOverlay && !LiveMeleeRate(loadedBody,currentOverlay,1.10f)) {fail("live_staff_rate");return;}
        if(swings==4 && !state.attacking && ++idleFrames>=20) {
            K1EngageMouse(true,false);
            const auto& p=meleePoise[player->m_objectId];
            if(!p.staffTiming || PoiseQuietInterval(p)!=2500000 || PoiseTickInterval(p)!=500000) {fail("live_staff_clock");return;}
            fprintf(stderr,"K1_STAFF_BALANCE_PASS rate=1.10 chain=4 hold=1 npc_rate=0.85 yellow_rate=1 quiet_us=2500000 tick_us=500000\n");
            const int playerHP=player->GetCurrentHitPoints(0),enemyHP=enemy->GetCurrentHitPoints(0);
            meleePoise[player->m_objectId].percent=100;meleePoise[enemy->m_objectId].percent=50;
            ApplySuccessfulParry(player,enemy);
            if(meleePoise[player->m_objectId].percent!=65 || meleePoise[enemy->m_objectId].percent!=70 || state.attacking ||
               meleePoise[player->m_objectId].parriedUntil!=now+parryAttackDelayUs) {fail("staff_parry_penalty");return;}
            ApplySuccessfulParry(enemy,player);
            if(meleePoise[enemy->m_objectId].percent!=45 || meleePoise[player->m_objectId].percent!=90 ||
               player->GetCurrentHitPoints(0)!=playerHP || enemy->GetCurrentHitPoints(0)!=enemyHP) {fail("default_parry_penalty_or_hp");return;}
            fprintf(stderr,"K1_STAFF_PARRY_PENALTY_PASS staff_loss=35 npc_loss=25 staff_gain=25 npc_gain=20 cooldown_us=%lld hp_unchanged=1\n",parryAttackDelayUs);
            meleePoise[player->m_objectId].percent=50;NotePoiseActivity(player,"staff_regen_probe");
            recoveryStart=now;recoveryTick=meleePoise[player->m_objectId].nextTick;phase=704;ticks=0;return;
        }
        if(ticks>600){fail("four_swing_chain_timeout");return;}return;
    }
    if(phase==704 || phase==705) {
        const int expected=phase==704?50:51;const auto& p=meleePoise[player->m_objectId];
        if(now<recoveryTick && p.percent!=expected){fail("staff_early_regen");return;}
        if(p.percent>expected) {
            if(p.percent!=expected+1 || now<recoveryTick || now-recoveryTick>100000){fail("staff_regen_cadence");return;}
            if(phase==704) {recoveryTick=p.nextTick;phase=705;ticks=0;return;}
            if(now-recoveryStart<3000000){fail("staff_recovery_delay");return;}
            fprintf(stderr,"K1_STAFF_REGEN_LIVE_PASS quiet_us=2500000 tick_us=500000 percent=52 elapsed_us=%lld\n",now-recoveryStart);
            if(std::getenv("K1_ENGAGE_FORCE_CONTACT_TEST")){phase=706;ticks=0;return;}
            fprintf(stderr,"K1_STAFF_CONTACT_PASS beams=2 repeated=1 unpowered=1 windup=1 recovery=1 native_damage=1 directions=3 extended_tail=1\n");
            phase=-1;quit();return;
        }
        if(ticks>240){fail("staff_regen_timeout");return;}return;
    }
    if(phase>=706) {
        const int spells[]={FORCE_POWER_CHOKE,FORCE_POWER_HOLD,FORCE_POWER_FORCE_WHIRLWIND};
        const int effects[]={EFFECT_STATE_CHOKE,EFFECT_STATE_PARALYZED,EFFECT_STATE_WHIRLWIND};
        if(phase==706) {
            player->ClearAllActions(1);enemy->ClearAllActions(1);
            player->m_pCombatRound->EndCombatRound(1);enemy->m_pCombatRound->EndCombatRound(1);
            state.ClearInputs();movement.Clear();player->m_pStats->SetWISBase(100);
            enemy->m_hitPoints=1000;enemy->SetCurrentHitPoints(1000);
            enemy->SetPosition(player->m_position+Vector(1.25f,0,0),1,1,0);
            auto* caster=forceHitTrial<3?player:enemy;auto* victim=forceHitTrial<3?enemy:player;
            const int spell=forceHitTrial<3?spells[forceHitTrial]:FORCE_POWER_CHOKE;
            unsigned char slot=0;while(slot<caster->m_pStats->m_numClasses && !caster->m_pStats->GetSpellUsesLeft(spell,slot,0,0))++slot;
            if(slot>=caster->m_pStats->m_numClasses || !caster->AddCastSpellActions(spell,slot,0,0,0,victim->m_position,victim->m_objectId,1,0,1,0,0,0,-1,0xff,0)){fail("force_contact_cast");return;}
            forceHitStart=now;phase=707;ticks=0;return;
        }
        if(phase==707) {
            auto* actor=player->GetClientCreature();auto* victim=enemy->GetClientCreature();
            auto* targetBody=victim?victim->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
            auto* playerBody=actor?actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
            const bool affected=forceHitTrial<3 && enemy->m_nState==effects[forceHitTrial];
            const bool casting=forceHitTrial==3 && enemy->m_pCombatRound->m_bSpellCastRound;
            const bool ready=actor && playerBody && IsSourceBody(playerBody) && !NativeUtilityQueued(player) &&
                !player->m_pCombatRound->m_bRoundStarted && !player->m_pCombatRound->m_bSpellCastRound;
            if(actor && playerBody && IsSourceBody(playerBody) && !NativeUtilityQueued(player) && !ready) {
                fail("force_contact_source_returned_with_busy_round");return;
            }
            if(targetBody && !IsSourceBody(targetBody) && (affected||casting) && ready && StaffBlades(actor).size()==2) {
                std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(targetBody,triangles);
                auto* weapon=actor->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
                const Vector position=weapon->m_vPosition;const Quaternion orientation=weapon->m_qOrientation;
                const auto savedState=state;const size_t count=sceneMeleeContacts.size();
                weapon->m_vPosition=Vector();weapon->m_qOrientation=Quaternion(Vector(0,1,0),1.5707963268f);
                auto beams=StaffBlades(actor);bool found=false;
                for(const auto& t:triangles) {
                    if(magnitude(cross(t.b-t.a,t.c-t.a))<1e-6f)continue;
                    weapon->m_vPosition=(t.a+t.b+t.c)/3.f-(beams[0].base+beams[0].tip)*.5f;
                    state.attacking=true;hitConsumed=false;hadBlade=false;++attackSerial;
                    TraceStaffBlades(actor,PlayerHitStart(swingDirection)+.03f);
                    if(sceneMeleeContacts.size()>count){found=true;break;}
                }
                const auto before=meleeDamageContacts;forceHitHP=enemy->GetCurrentHitPoints(0);
                if(found)ResolveMeleeContacts(std::vector<MeleeContact>(sceneMeleeContacts.begin()+count,sceneMeleeContacts.end()),now);
                weapon->m_vPosition=position;weapon->m_qOrientation=orientation;state=savedState;hadBlade=false;
                sceneMeleeContacts.resize(count);
                if(!found || meleeDamageContacts!=before+1){fail("force_contact_not_delivered");return;}
                fprintf(stderr,"K1_STAFF_FORCE_CONTACT_ACCEPTED trial=%d kind=%s state=%d triangles=%zu native_hits=1\n",forceHitTrial,casting?"casting":"affected",enemy->m_nState,triangles.size());
                phase=708;ticks=0;return;
            }
            if(ticks%60==0)fprintf(stderr,"K1_STAFF_FORCE_CONTACT_WAIT trial=%d state=%d casting=%d ready=%d source=%d round=%d spell=%d\n",forceHitTrial,enemy->m_nState,casting,ready,playerBody&&IsSourceBody(playerBody),player->m_pCombatRound->m_bRoundStarted,player->m_pCombatRound->m_bSpellCastRound);
            if(now-forceHitStart>15000000){fail("force_contact_state_timeout");return;}return;
        }
        if(phase==708 && ticks>=20) {
            if(enemy->GetCurrentHitPoints(0)>=forceHitHP){fail("force_contact_no_health_loss");return;}
            fprintf(stderr,"K1_STAFF_FORCE_CONTACT_DAMAGE_PASS trial=%d before=%d after=%d\n",forceHitTrial,forceHitHP,enemy->GetCurrentHitPoints(0));phase=709;ticks=0;return;
        }
        if(phase==709) {
            if(forceHitTrial<3 && NativeStatusAnimation(enemy)){if(now-forceHitStart>25000000)fail("force_contact_effect_expiry");return;}
            if(++forceHitTrial<4){phase=706;ticks=0;return;}
            fprintf(stderr,"K1_STAFF_FORCE_CONTACT_PASS effects=3 caster=1 native_hits=4 health_loss=1\n");
            fprintf(stderr,"K1_STAFF_CONTACT_PASS beams=2 repeated=1 unpowered=1 windup=1 recovery=1 native_damage=1 directions=3 extended_tail=1\n");
            phase=-1;quit();return;
        }
        return;
    }
    if(phase==701) {
        if(ticks<20) return;
        int after=enemy->GetCurrentHitPoints(0);
        if(after>=beforeHP) {fail("native_damage_not_delivered");return;}
        fprintf(stderr,"K1_STAFF_CONTACT_DAMAGE_PASS direction=%d blade=%d before=%d after=%d\n",direction,beam,beforeHP,after);
        if(++trial==6) {
            phase=702;ticks=0;return;
        }
        phase=700;ticks=0;return;
    }
    if(ticks<45) return;
    auto* actor=player->GetClientCreature();auto* victim=enemy->GetClientCreature();
    auto* weapon=actor?actor->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1):nullptr;
    auto* item=actor?g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[4]):nullptr;
    if(!weapon || !item || !item->m_bPowered || StaffBlades(actor).size()!=2 || !victim || owned==npcPlayback.end()) {
        if(ticks>240) fail("native_models_not_ready");return;
    }
    std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(victim->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1),triangles);
    const Vector oldPosition=weapon->m_vPosition;const Quaternion oldOrientation=weapon->m_qOrientation;
    weapon->m_qOrientation=Quaternion(Vector(0,1,0),1.5707963268f);weapon->m_vPosition=Vector();
    auto beams=StaffBlades(actor);bool located=false;
    // Place one beam on a measured outer body surface, rejecting positions that
    // touch the other beam. Never enlarge the native target or blade.
    auto placeBeam=[&](int beamIndex) {
        for(const auto& t:triangles) {
            const Vector center=(t.a+t.b+t.c)/3.f;
            if(center.z-enemy->m_position.z<.9f || center.z-enemy->m_position.z>1.7f) continue;
            weapon->m_vPosition=center-(beams[beamIndex].base+beams[beamIndex].tip)*.5f;
            auto actual=StaffBlades(actor);bool hit[2]={false,false};
            for(int b=0;b<2;++b) for(const auto& face:triangles) hit[b]=hit[b] || K1SaberContact::SegmentTriangle(actual[b].base,actual[b].tip,face);
            if(hit[beamIndex] && !hit[1-beamIndex]) return true;
        }
        return false;
    };
    located=placeBeam(beam);
    bool ok=located;const size_t queued=sceneMeleeContacts.size();
    swingDirection=direction;++attackSerial;
    auto sample=[&](float t) {hadBlade=false;hitConsumed=false;TraceStaffBlades(actor,t);return sceneMeleeContacts.size()-queued;};
    if(located) {
        ok=sample(PlayerHitStart(direction)-.01f)==0 && ok;
        ok=sample(StaffContactEnd(direction)+.01f)==0 && ok;
        item->m_bPowered=0;ok=sample(PlayerHitStart(direction)+.01f)==0 && ok;item->m_bPowered=1;
        const float follow=StaffContactEnd(direction)-.01f;
        ok=sample(follow)==1 && hitConsumed && sceneMeleeContacts.back().target==enemy->m_objectId && ok;
        TraceStaffBlades(actor,follow+.001f);ok=sceneMeleeContacts.size()==queued+1 && ok;
    }
    if(trial==0 && ok) {
        sceneMeleeContacts.resize(queued);
        const auto savedState=state;const auto savedPoise=meleePoise[enemy->m_objectId];
        const int savedAI=enemy->m_nAIState;const Vector savedFacing=enemy->m_orientation;
        auto& npc=owned->second;const bool savedGuard=npc.closeGuard;
        const int hp=enemy->GetCurrentHitPoints(0);const auto damageBefore=meleeDamageContacts;
        enemy->m_nAIState|=0x84;enemy->SetOrientation(normalize(player->m_position-enemy->m_position));
        npc.closeGuard=true;npc.parrying=npc.attacking=false;
        meleePoise[enemy->m_objectId]=MeleePoise();
        state.attacking=true;++attackSerial;hitConsumed=false;
        for(int b=0;b<2;++b) {
            ok=placeBeam(b) && ok;hadBlade=false;TraceStaffBlades(actor,StaffContactEnd(direction)-.01f+b*.001f);
        }
        ok=sceneMeleeContacts.size()==queued+2 && ok;
        if(sceneMeleeContacts.size()==queued+2) {
            auto a=sceneMeleeContacts[queued],b=sceneMeleeContacts[queued+1];
            ok=a.blade!=b.blade && !a.poiseOnly && b.poiseOnly && ok;
            ResolveMeleeContacts({a,a,b,b},now);
            ok=meleePoise[enemy->m_objectId].percent==78 && enemy->GetCurrentHitPoints(0)==hp && meleeDamageContacts==damageBefore && ok;
            TraceStaffBlades(actor,StaffContactEnd(direction)-.005f);
            ok=sceneMeleeContacts.size()==queued+2 && ok;
        }
        fprintf(stderr,"K1_STAFF_TWO_BLADE_POISE_%s percent=%d damage=%d hp_unchanged=%d repeated_blades_consumed=%d\n",ok?"PASS":"FAIL",meleePoise[enemy->m_objectId].percent,100-meleePoise[enemy->m_objectId].percent,enemy->GetCurrentHitPoints(0)==hp,staffContactBlades==3);
        sceneMeleeContacts.resize(queued);npc.closeGuard=false;
        state.attacking=true;++attackSerial;hitConsumed=false;
        for(int b=0;b<2;++b){ok=placeBeam(b) && ok;hadBlade=false;TraceStaffBlades(actor,StaffContactEnd(direction)-.01f+b*.001f);}
        if(sceneMeleeContacts.size()==queued+2) ResolveMeleeContacts({sceneMeleeContacts[queued],sceneMeleeContacts[queued+1]},now);
        else ok=false;
        ok=meleeDamageContacts==damageBefore+1 && ok;
        fprintf(stderr,"K1_STAFF_TWO_BLADE_HEALTH_%s native_hits=%llu\n",ok?"PASS":"FAIL",meleeDamageContacts-damageBefore);
        sceneMeleeContacts.resize(queued);state.attacking=true;++attackSerial;hitConsumed=false;
        auto* round=player->m_pCombatRound;
        const auto savedSpell=round->m_bSpellCastRound;const auto savedRound=round->m_bRoundStarted;
        round->m_bRoundStarted=0;round->m_bSpellCastRound=1;
        const auto beforeBusy=meleeDamageContacts;
        ok=placeBeam(0) && ok;hadBlade=false;
        TraceStaffBlades(actor,PlayerHitStart(direction)+.02f);
        const bool foundBusyContact=sceneMeleeContacts.size()==queued+1;
        if(foundBusyContact) ResolveMeleeContacts({sceneMeleeContacts.back()},now);
        const bool rejected=foundBusyContact && !hitConsumed && meleeDamageContacts==beforeBusy;
        sceneMeleeContacts.resize(queued);round->m_bSpellCastRound=0;
        TraceStaffBlades(actor,PlayerHitStart(direction)+.03f);
        const bool retry=sceneMeleeContacts.size()==queued+1;
        if(retry) ResolveMeleeContacts({sceneMeleeContacts.back()},now);
        ok=rejected && retry && meleeDamageContacts==beforeBusy+1 && ok;
        const auto afterRetry=sceneMeleeContacts.size();TraceStaffBlades(actor,PlayerHitStart(direction)+.04f);
        ok=sceneMeleeContacts.size()==afterRetry && ok;
        fprintf(stderr,"K1_STAFF_BUSY_ROUND_RETRY_%s rejected=%d retried=%d native_hits=%llu no_repeat=%d\n",ok?"PASS":"FAIL",rejected,retry,meleeDamageContacts-beforeBusy,sceneMeleeContacts.size()==afterRetry);
        round->m_bSpellCastRound=savedSpell;round->m_bRoundStarted=savedRound;
        state=savedState;meleePoise[enemy->m_objectId]=savedPoise;npc.closeGuard=savedGuard;
        enemy->m_nAIState=savedAI;enemy->SetOrientation(savedFacing);
    }
    sceneMeleeContacts.resize(queued);weapon->m_vPosition=oldPosition;weapon->m_qOrientation=oldOrientation;
    hadBlade=false;hitConsumed=true;
    if(!ok) {fail("beam_geometry_or_contact_gate");return;}
    fprintf(stderr,"K1_STAFF_CONTACT_BEAM_PASS direction=%d blade=%d other_misses=1 follow_through=1\n",direction,beam);
    beforeHP=enemy->GetCurrentHitPoints(0);
    if(!ContactDamage(player,enemy,false)) {fail("native_damage_queue");return;}
    phase=701;ticks=0;
}
// Isolated Dark Jedi test: autonomous decisions, native Force animation/cost,
// source melee recovery, normal planning and the production replacement path.
// Opt-in installed-data contract: use this actual NPC body/weapon and the
// production sweep/queue/damage services; no enlarged collision regions.
bool NpcStaffContract(NpcPlayback& npc,CSWSCreature* player,CSWSCreature* enemy) {
    auto* body=npc.body;auto* weapon=npc.owner->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
    bool ok=npc.staff && body && weapon && !_stricmp(weapon->m_pModel->m_szName,"w_dblsbr_002") && StaffBlades(npc.owner).size()==2 &&
        !strcmp(body->m_pModel->m_szName,NpcType(enemy).rig) && NpcChainLimit(npc)==4 && NpcSwingRate(npc)==.85f;
    if(!ok)return false;
    const auto saved=npc;const auto savedState=state;const auto savedPoise=meleePoise;
    body->m_bAlwaysRendering=true;body->EnableForceAnimations();
    int poses=0;float maxGrip=0;
    for(const char* clip:{"s_top","s_tlbr","s_trbl","s_lr","s_rl","s_bltr","s_brtl",
                         "j_block_up","j_block_ur","j_block_ul","j_block_lr","j_block_ll"}) {
        body->PlayAnimation(nullptr,1,AnimRun::kAnimRemove,0);body->Reset();
        // Names are compared by content; pointers are not an animation identity.
        npc.direction=0;for(int d=0;d<7;++d)if(!strcmp(clip,fullAttacks[d]))npc.direction=d;
        npc.base="j_idle";npc.overlay=clip;npc.attacking=clip[0]=='s';npc.closeGuard=npc.parrying=false;npc.pose={};
        body->PlayAnimation(const_cast<char*>(clip),NpcSwingRate(npc),AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,0);
        float at=0,length=0;if(!body->GetAnimation(const_cast<char*>(clip),&at,&length) || length<=0){ok=false;continue;}
        for(float fraction:{0.f,.25f,.5f,.75f,.999f}) {
            body->SetAnimationTime(const_cast<char*>(clip),length*fraction);body->m_cForcePose=1;body->m_nLastAnimateTick=body->m_pScene->m_nAnimateTick-1;body->Animate(.0001f);
            ok=ok && !npc.pose.lastPose.empty();
            ++poses;maxGrip=std::max(maxGrip,npc.pose.sourceContactError);
            for(const auto& pose:npc.pose.lastPose)ok=ok && K1SaberContact::Finite(pose.position) && std::isfinite(dot(pose.orientation,pose.orientation));
        }
    }
    ok=ok && poses==60 && std::isfinite(maxGrip) && maxGrip<.003f;
    npc.attacking=true;npc.direction=0;npc.overlay="s_top";npc.base="j_idle";
    body->PlayAnimation(nullptr,1,AnimRun::kAnimRemove,0);body->Reset();
    body->PlayAnimation(const_cast<char*>("s_top"),NpcSwingRate(npc),AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,.45f);body->m_cForcePose=1;body->m_nLastAnimateTick=body->m_pScene->m_nAnimateTick-1;body->Animate(.0001f);
    const auto position=weapon->m_vPosition;const auto orientation=weapon->m_qOrientation;
    weapon->m_qOrientation=Quaternion(Vector(0,1,0),1.5707963268f);
    std::vector<K1SaberContact::Triangle> triangles;
    CollisionTriangles(Actor()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1),triangles);
    const size_t before=sceneMeleeContacts.size();npc.hit=false;npc.contactBlades=0;++npc.serial;
    state.attacking=state.rightHeld=false;
    for(int blade=0;blade<2;++blade) {
        bool found=false;
        for(const auto& face:triangles) {
            if(magnitude(cross(face.b-face.a,face.c-face.a))<1e-6f)continue;
            weapon->m_vPosition=Vector();const auto beams=StaffBlades(npc.owner);
            if(beams.size()!=2)break;
            weapon->m_vPosition=(face.a+face.b+face.c)/3.f-(beams[blade].base+beams[blade].tip)*.5f;
            const auto actual=StaffBlades(npc.owner);bool touched[2]={false,false};
            for(int b=0;b<2;++b)for(const auto& triangle:triangles)touched[b]|=K1SaberContact::SegmentTriangle(actual[b].base,actual[b].tip,triangle);
            if(!touched[blade] || touched[1-blade])continue;
            npc.bladeValid=false;++npc.revision;float tt=0,ll=0;bool has=body->GetAnimation(const_cast<char*>(npc.overlay),&tt,&ll);
            fprintf(stderr,"K1_NPC_STAFF_PROBE_GATE blade=%d triangles=%zu touched=%d,%d ai=%d active=%d requested=%d scene=%d has=%d time=%g length=%g\n",blade,triangles.size(),touched[0],touched[1],enemy->m_nAIState,K1EngageEnemyAIActive(player->m_objectId),npc.requested,body->m_pScene!=nullptr,has,tt,ll);
            TraceNpcBlades(body->m_pScene);
            found=sceneMeleeContacts.size()==before+blade+1;break;
        }
        ok=ok && found;
    }
    const auto damageBefore=meleeDamageContacts;
    if(sceneMeleeContacts.size()==before+2) {
        const auto a=sceneMeleeContacts[before],b=sceneMeleeContacts[before+1];
        ok=ok && a.blade!=b.blade && !a.poiseOnly && b.poiseOnly;
        ResolveMeleeContacts({a,a,b,b},g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime());
        ok=ok && meleeDamageContacts==damageBefore+1;
    } else ok=false;
    sceneMeleeContacts.resize(before);weapon->m_vPosition=position;weapon->m_qOrientation=orientation;
    const auto facing=player->m_orientation;
    player->SetOrientation(normalize(enemy->m_position-player->m_position));
    npc.attacking=false;state.rightHeld=true;meleePoise[player->m_objectId]=MeleePoise();
    const int hp=player->GetCurrentHitPoints(0);
    const bool first=K1EngageGuardContact(enemy->m_objectId,player->m_objectId);
    const bool second=K1EngageGuardContact(enemy->m_objectId,player->m_objectId);
    ok=ok && first && second && meleePoise[player->m_objectId].percent==78 && player->GetCurrentHitPoints(0)==hp;
    player->SetOrientation(facing);
    NotePoiseActivity(enemy,"staff_npc_contract");
    ok=ok && meleePoise[enemy->m_objectId].staffTiming && PoiseTickInterval(meleePoise[enemy->m_objectId])==500000;
    meleePoise[enemy->m_objectId].percent=100;ApplySuccessfulParry(enemy,player);
    ok=ok && meleePoise[enemy->m_objectId].percent==65;
    state=savedState;meleePoise=savedPoise;npc=saved;
    ok=SwapNpcRig(npc,true) && ok;
    fprintf(stderr,"K1_NPC_STAFF_CONTRACT_%s poses=%d max_grip=%g physical_blades=2 native_hits=%llu rate=.85 chain=4 parried_loss=35 red_native_model=1\n",ok?"PASS":"FAIL",poses,maxGrip,meleeDamageContacts-damageBefore);
    return ok;
}
void DarkJediProbeTick(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* enemy) {
    if(DismemberArenaProbe(phase,ticks,player,enemy))return;
    static int rounds=0,completed=0,nativeSamples=0,beforeForce=0;
    static int clearFrame=0;
    static long long clearTime=0;
    static bool meleeAfterCast=false;
    auto quit=[] {if(FILE* f=fopen("dark-jedi-input.txt","w")) {fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* reason) {fprintf(stderr,"K1_DARK_JEDI_FAIL phase=%d reason=%s\n",phase,reason);phase=-1;quit();};
    if(!enemy) {fail("missing_actor");return;}
    if(phase==2) {
        if(K1EngageActive()) Plan(true);
        if(!K1EngagePlanning()) K1EngageDetected();
        enemy->AddToVisibleList(player->m_objectId,1,0,0,0);
        beforeForce=enemy->m_pStats->m_nCurrentForcePoints;
        completed=nativeSamples=clearFrame=0;clearTime=0;meleeAfterCast=false;phase=150;ticks=0;return;
    }
    if(phase==150) {
        if(ticks<30) return;
        if(!K1EngagePlanning() || arenaForce.queued || NativeUtilityQueued(enemy)) {fail("planning_cast");return;}
        fprintf(stderr,"K1_DARK_JEDI_PLANNING_PASS actor=%llu\n",enemy->m_objectId);
        if(!K1EngageEnter()) {fail("enter");return;}
        enemy->SetPosition(player->m_position+player->m_orientation*1.2f,1,1,0);
        phase=152;ticks=0;return;
    }
    auto* client=enemy->GetClientCreature();
    auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
    if(phase==151) {
        if(state.mode==K1EngageState::Mode::Exploration && body && !IsSourceBody(body) &&
            !_stricmp(body->m_pModel->m_szName,"n_darkjedim") && !K1EngageOwnsEnemy(enemy->m_objectId)) {
            fprintf(stderr,"K1_DARK_JEDI_PASS rounds=%d native_victory=1\n",rounds);phase=-1;quit();
        } else if(ticks>240) fail("victory_restoration");
        return;
    }
    if(ticks>1500) {fail("autonomous_cast_or_melee_timeout");return;}
    if(arenaForce.queued>completed && !NativeUtilityQueued(enemy) && !clearFrame) {
        clearFrame=ticks;clearTime=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
        fprintf(stderr,"K1_NPC_FORCE_NATIVE_CLEAR actor=%llu spell=%d tick=%d round=%d spell_round=%d paused=%d timer=%d anim=%d\n",
            enemy->m_objectId,arenaForce.lastSpell,ticks,enemy->m_pCombatRound->m_bRoundStarted,
            enemy->m_pCombatRound->m_bSpellCastRound,enemy->m_pCombatRound->m_bRoundPaused,
            enemy->m_pCombatRound->m_nPauseTimer,client?client->m_currentAnimation:-1);
    }
    if(body && !IsSourceBody(body) && arenaForce.queued>completed) {
        if(_stricmp(body->m_pModel->m_szName,"n_darkjedim") || body->m_pAddInModel) {fail("wrong_native_body");return;}
        for(int i=0;i<body->m_paAnimations.m_count;++i)
            if(!strncmp(body->m_paAnimations.m_pData[i]->m_pAnimation->m_szName,"cast",4)) ++nativeSamples;
    }
    const auto found=npcPlayback.find(enemy->m_objectId);
    if(found==npcPlayback.end() || !K1EngageOwnsEnemy(enemy->m_objectId)) return;
    static unsigned long long policyActor=OBJECT_INVALID;
    if(policyActor!=enemy->m_objectId) {
        policyActor=enemy->m_objectId;const auto& type=NpcType(enemy);
        int accepted=0;for(int roll=1;roll<=100;++roll)accepted+=NpcParryRollPasses(NpcParryPercent(enemy),roll);
        const bool policy=NpcParryPolicyProbe(enemy) && accepted==(type.staff?20:25) &&
            NpcAttackRest(found->second)==(type.staff?150000:250000) && type.chainContinuePercent==(type.staff?100:85);
        fprintf(stderr,"K1_NPC_TYPE_POLICY_%s id=%s parry=%d continuation=%d rest_us=%lld\n",policy?"PASS":"FAIL",type.id,accepted,type.chainContinuePercent,NpcAttackRest(found->second));
        if(!policy){fail("type_policy");return;}
    }
    static unsigned long long contractActor=OBJECT_INVALID;
    if(StaffNpc(enemy) && contractActor!=enemy->m_objectId) {
        contractActor=enemy->m_objectId;
        if(!NpcStaffContract(found->second,player,enemy)){fail("staff_contract");return;}
        return;
    }
    const auto& source=found->second;
    if(source.staff && source.attacking && source.overlay && !LiveMeleeRate(source.body,source.overlay,NpcSwingRate(source))) {fail("staff_live_rate");return;}
    if(!std::isfinite(source.maximumGripError) || source.maximumGripError>.003f) {fail("source_grip");return;}
    if(completed && source.attacking && source.hit && source.gripSamples>0) meleeAfterCast=true;
    if(arenaForce.queued>completed && !NativeUtilityQueued(enemy)) {
        const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
        fprintf(stderr,"K1_NPC_FORCE_RECOVERY actor=%llu spell=%d frames=%d elapsed_us=%lld aggression=%d roam=%lld\n",
            enemy->m_objectId,arenaForce.lastSpell,ticks-clearFrame,now-clearTime,source.jedi.aggression,source.jedi.roamTime);
        if(ticks-clearFrame>6 || source.jedi.aggression<arenaForce.queuedAggression) {fail("post_cast_recovery_or_intent_reset");return;}
        fprintf(stderr,"K1_NPC_FORCE_RECOVERY_PASS actor=%llu spell=%d frames=%d aggression_before=%d aggression_after=%d\n",
            enemy->m_objectId,arenaForce.lastSpell,ticks-clearFrame,arenaForce.queuedAggression,source.jedi.aggression);
        const int remaining=enemy->m_pStats->m_nCurrentForcePoints;
        if(nativeSamples<2 || remaining>=beforeForce || strcmp(body->m_pModel->m_szName,NpcType(enemy).rig)) {fail("cast_cost_or_return");return;}
        fprintf(stderr,"K1_DARK_JEDI_CAST_PASS actor=%llu spell=%d force_before=%d force_after=%d samples=%d\n",
            enemy->m_objectId,arenaForce.lastSpell,beforeForce,remaining,nativeSamples);
        ++completed;beforeForce=remaining;nativeSamples=clearFrame=0;clearTime=0;
    }
    if(completed>=2 && meleeAfterCast) {
        fprintf(stderr,"K1_DARK_JEDI_ROUND_PASS round=%d actor=%llu casts=%d grip=%g\n",++rounds,enemy->m_objectId,completed,source.maximumGripError);
        enemy->SetCurrentHitPoints(0);phase=rounds==2?151:100;ticks=0;
    }
}
#include "include/k1dualprobe.h"

void ArenaTick() {
    static const bool requested=[] {const char* p=std::getenv("K1_ENGAGE_ARENA");return p && !strcmp(p,"1");}();
    static const bool enemyProbe=std::getenv("K1_ENGAGE_ENEMY_AI_TEST") != nullptr;
    static const bool gunner=std::getenv("K1_ENGAGE_ARENA_GUNNER") != nullptr;
    static const bool rifle=std::getenv("K1_ENGAGE_ARENA_RIFLE") != nullptr;
    static const bool navProbe=std::getenv("K1_ENGAGE_NPC_NAV_TEST") != nullptr;
    static int phase=0,ticks=0;
    static int navigationReloadStage=0;
    static bool attackWalkFixtureDone=false;
    static long long replacementAt=0;
    static int replacements=0;
    static unsigned long long retiredId=OBJECT_INVALID;
    static std::vector<unsigned long long> retiredClients;
    static unsigned long long reloadGeneration=0,reloadUnloads=0;
    static unsigned long long opponentId=OBJECT_INVALID;static int guardHP=0;
    static unsigned long long laneBlockerId=OBJECT_INVALID;
    static int forceTrial=0,reactionSerial=0,reactionGripSamples=0;
    static bool recoveryPlaced=false;
    static const int forceSpells[]={FORCE_POWER_CHOKE,FORCE_POWER_HOLD,FORCE_POWER_FORCE_WHIRLWIND};
    static const int forceStates[]={EFFECT_STATE_CHOKE,EFFECT_STATE_PARALYZED,EFFECT_STATE_WHIRLWIND};
    static const int forceAnimations[]={CSWCAnimBase::CLIENT_ANIMATION_CHOKE,CSWCAnimBase::CLIENT_ANIMATION_PARALYZED,CSWCAnimBase::CLIENT_ANIMATION_WHIRLWIND};
    static float npcPlanningTime=0;
    static int npcPlanningSerial=0;
    static int navRequests=0;
    static float navDistance=0,navClearance=1000;
    static long long planningWorldStart=0, planningUiStart=0;
    static Vector planningPosition;
    if(!requested || ++ticks<((std::getenv("K1_ENGAGE_POISE_TEST") && phase>=70) || (std::getenv("K1_ENGAGE_UTILITY_TEST") && phase>=59) ||
        (std::getenv("K1_ENGAGE_DARK_JEDI_TEST") && phase>=150) ||
        (std::getenv("K1_ENGAGE_STAFF_CONTACT_TEST") && phase>=700) ||
        (std::getenv("K1_ENGAGE_STAFF_ARC_TEST") && phase>=750) ||
        (std::getenv("K1_ENGAGE_DUAL_TEST") && phase>=760) ||
        (std::getenv("K1_ENGAGE_CLASH_TEST") && phase>=110) || (std::getenv("K1_ENGAGE_PARRY_TEST") && phase>=120) ||
        (enemyProbe && (phase==4 || (navProbe && phase==63))) ? 1 : 30)) return;
    CSWSCreature* actor=ServerActor();
    CClientExoApp* app=g_pAppManager->m_pClientExoApp;
    CServerExoApp* server=g_pAppManager->m_pServerExoApp;
    if(!actor || !actor->GetArea() || !app->GetModule() || app->m_pInternal->m_bAreaNotReadyToDisplay) return;

    if(std::getenv("K1_ENGAGE_SURFACE_MARK_ONLY") && phase==2) {
        // Reuse installed-data receiver/render fixtures without conflating
        // their result with the broad NPC animation-parity suite.
        ReleaseNpcPlayback();
        if(auto* enemy=server->GetCreatureByGameObjectID(opponentId)) {enemy->RemoveFromArea(0);delete enemy;}
        opponentId=OBJECT_INVALID;encounter.clear();
        if(!K1EngageEnter()){fprintf(stderr,"K1_JKA_SABER_MARK_SETUP_FAIL\n");phase=-1;return;}
        phase=200;ticks=0;return;
    }

    if(std::getenv("K1_ENGAGE_DUAL_TEST") && (phase==2 || phase>=760)) {
        DualPlayableProbeTick(phase,ticks,actor,server->GetCreatureByGameObjectID(opponentId));return;
    }
    if(std::getenv("K1_ENGAGE_STAFF_ARC_TEST") && (phase==2 || phase>=750)) {
        StaffArcProbeTick(phase,ticks,actor,server->GetCreatureByGameObjectID(opponentId));return;
    }
    if(std::getenv("K1_ENGAGE_STAFF_CONTACT_TEST") && (phase==2 || phase>=700)) {
        StaffContactProbeTick(phase,ticks,actor,server->GetCreatureByGameObjectID(opponentId));return;
    }
    if(std::getenv("K1_ENGAGE_DARK_JEDI_TEST") && (phase==2 || phase>=150)) {
        DarkJediProbeTick(phase,ticks,actor,server->GetCreatureByGameObjectID(opponentId));return;
    }
    const bool corpseProbe=std::getenv("K1_ENGAGE_CORPSE_TEST")!=nullptr;
    if(corpseProbe && phase==2) {
        if(!K1EngageEnter()) {fprintf(stderr,"K1_CORPSE_SETUP_FAIL\n");phase=-1;return;}
        // Establish native/custom ownership and encounter membership before
        // the first death, just as subsequent replacement rounds do.
        guardHP=actor->GetCurrentHitPoints(0);
        phase=101;replacementAt=0;ticks=0;return;
    }
    if(corpseProbe && phase==92 && strcmp(app->GetModule()->m_sModuleName.CStr(),"k1arena")) {
        bool gone=true;
        int reused=0;
        for(auto id:retiredClients) {
            auto* current=app->GetCreatureByGameObjectID(id);
            if(current) {
                auto* native=current->GetServerCreature();
                auto* model=current->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
                // Native save loading reuses object IDs. Immediate removal is
                // checked at UnloadArea; a new live actor is not an old corpse.
                const bool live=native && !native->GetDead() && current->m_pArea &&
                    current->m_pArea==app->GetPlayerCreature()->m_pArea &&
                    !current->m_pArea->GetIsObjectCorpse(id);
                gone &= live;
                if(live) ++reused;
                fprintf(stderr,"K1_CORPSE_RELOAD_OBJECT id=%llu area=%llu current_area=%llu native=%d dead=%d tag=%s model=%s position=%g,%g,%g\n",
                    id,current->m_pArea?current->m_pArea->m_objectId:OBJECT_INVALID,
                    app->GetPlayerCreature()->m_pArea->m_objectId,native!=nullptr,native?native->GetDead():-1,
                    native?native->m_sTag.CStr():"none",model&&model->m_pModel?model->m_pModel->m_szName:"none",
                    current->m_position.x,current->m_position.y,current->m_position.z);
            }
        }
        fprintf(stderr,"K1_CORPSE_RELOAD_%s old_bodies=%zu reused_live_ids=%d\n",gone?"PASS":"FAIL",retiredClients.size(),reused);
        if(!gone) {phase=-1;return;}
        server->SetMoveToModuleString(CExoString("k1arena"));server->SetMoveToModulePending(1);phase=93;return;
    }
    if(corpseProbe && phase==93 && !strcmp(app->GetModule()->m_sModuleName.CStr(),"k1arena")) {
        auto* area=app->GetPlayerCreature()->m_pArea;
        fprintf(stderr,"K1_CORPSE_RETURN_%s count=%d\n",area && !area->m_retainedCorpseIds.m_count?"PASS":"FAIL",area?area->m_retainedCorpseIds.m_count:-1);
        phase=-1;return;
    }
    if(phase==90 && navigationUnloads>reloadUnloads && strcmp(app->GetModule()->m_sModuleName.CStr(),"k1arena")) {
        // The isolated seed save starts in its original module. Replay the
        // same arena transfer used by phase0 on the first fixture load.
        server->SetMoveToModuleString(CExoString("k1arena"));server->SetMoveToModulePending(1);
        fprintf(stderr,"K1_JKA_NAV_RELOAD_TRANSFER module=k1arena\n");phase=91;return;
    }
    if(phase==90 || phase==91) {
        if(strcmp(app->GetModule()->m_sModuleName.CStr(),"k1arena")) return;
        if(navigationGeneration<=reloadGeneration) return;
        const auto ready=npcNavigators.find(static_cast<CSWSArea*>(actor->GetArea())->m_objectId);
        if(navigationUnloads<=reloadUnloads || ready==npcNavigators.end() || ready->second.uses!=0) {
            fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL reload_cache_lifetime\n");phase=-1;return;
        }
        fprintf(stderr,"K1_JKA_NAV_RELOAD_READY generation=%llu unloads=%llu\n",navigationGeneration,navigationUnloads);
        phase=1;ticks=0;opponentId=OBJECT_INVALID;navigationReloadStage=2;
    }
    if(opponentId!=OBJECT_INVALID && !IsGamePaused() && !enemyProbe && !std::getenv("K1_ENGAGE_CONTROL_TEST")) {
        CSWSCreature* npc=server->GetCreatureByGameObjectID(opponentId);
        if(npc && !npc->GetDead() && !actor->GetDead() && npc->m_actionList.m_pInternal->m_nCount==0)
            npc->AddAttackActions(actor->m_objectId,0,1,0,0,0,0,1,SERVER_ANIMATION_ATTACK,1500,1,0,0,4,0);
    }
    if(phase<0) return;
    if(std::getenv("K1_ENGAGE_PARRY_TEST") && (phase==2 || phase>=120)) {
        ParryArenaProbeTick(phase,ticks,actor,server->GetCreatureByGameObjectID(opponentId));return;
    }
    if(std::getenv("K1_ENGAGE_CLASH_TEST") && (phase==2 || phase>=110)) {
        ClashArenaProbeTick(phase,ticks,actor,server->GetCreatureByGameObjectID(opponentId));return;
    }
    if(std::getenv("K1_ENGAGE_POISE_TEST") && (phase==2 || phase>=70)) {
        PoiseProbeTick(phase,ticks,actor,server->GetCreatureByGameObjectID(opponentId));return;
    }
    if(std::getenv("K1_ENGAGE_UTILITY_TEST") && (phase==2 || phase>=59)) {
        UtilityProbeTick(phase,ticks,actor,server->GetCreatureByGameObjectID(opponentId));return;
    }
    if(std::getenv("K1_ENGAGE_CONTROL_TEST") && (phase==2 || phase>=10)) {
        if (phase==2) {
            const long long worldElapsed=server->GetWorldTimer()->GetSnapshotTime()-planningWorldStart;
            const long long uiElapsed=app->GetPauseTimer()->GetSnapshotTime()-planningUiStart;
            const float drift=magnitude(actor->m_position-planningPosition);
            const bool slow=K1EngagePlanning() && !IsGamePaused() &&
                server->GetWorldTimer()->m_nSnapshotScale==1 && app->GetAnimationTimer()->m_nSnapshotScale==1 &&
                worldElapsed*1000>uiElapsed*3 && worldElapsed*1000<uiElapsed*7 && drift<.001f;
            fprintf(stderr,"K1_ENGAGE_PLANNING_%s world_us=%lld ui_us=%lld drift=%g\n",
                slow?"CHECK":"FAIL",worldElapsed,uiElapsed,drift);
        }
        ControlProbeTick(phase,ticks,opponentId);return;
    }
    if(phase==0) {
        // Dismiss companions in this throwaway in-memory session before changing modules.
        CSWPartyTable* party=server->GetPartyTable();
        while(party->m_nNumPartyMembers>0) {
            int index=party->m_partyMemberIndex[party->m_nNumPartyMembers-1];
            CSWSCreature* companion=server->GetCreatureByGameObjectID(party->m_partyMemberID[index]);
            if(companion) {for(int i=0;i<14;++i) companion->m_scripts[i]="";companion->RemoveFromArea(0);}
            if(!party->RemoveMember(index)) break;
        }
        server->SetMoveToModuleString(CExoString("k1arena"));server->SetMoveToModulePending(1);
        fprintf(stderr,"K1_ENGAGE_ARENA loading=k1arena\n");phase=1;ticks=0;return;
    }
    if(strcmp(app->GetModule()->m_sModuleName.CStr(),"k1arena")) return;
    // Interactive rounds and the repeat fixture share this lifecycle. Existing
    // one-shot control/navigation probes retain their independent completion path.
    if(phase==200 || (sandboxSession && (phase==100 || phase==101)))return;
    if(phase==100 || phase==101) {
        auto* npc=server->GetCreatureByGameObjectID(opponentId);
        auto* gui=app->GetInGameGui();
        if(actor->GetDead() || !gui || gui->m_nGlobalDialogState ||
            gui->m_pGuiManager->m_modalPanels.m_count ||
            gui->m_swGuiToggleState==CGuiInGame::SWGUI_STATE_COVERED || IsGamePaused()) return;
        const auto now=app->GetPauseTimer()->GetSnapshotTime();
        if(phase==101) {
            const bool owned=npc && K1EngageOwnsEnemy(opponentId);
            if(owned && (actor->GetCurrentHitPoints(0)<guardHP || (corpseProbe && ticks>=60))) {
                const bool clean=retiredId==OBJECT_INVALID || !server->GetCreatureByGameObjectID(retiredId);
                bool corpses=true;
                for(auto id:retiredClients) {
                    auto* corpse=app->GetCreatureByGameObjectID(id);
                    corpses &= corpse && corpse->m_pArea && corpse->m_pArea->GetIsObjectCorpse(id) &&
                        !corpse->m_bPendingRemoval && corpse->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
                }
                fprintf(stderr,"K1_ARENA_CORPSES_%s retained=%zu\n",corpses?"PASS":"FAIL",retiredClients.size());
                if(!corpses) {phase=-1;return;}
                fprintf(stderr,"K1_ARENA_ROUND_CHECK_%s round=%d owned=%d damage=%d retired=%d\n",
                    clean?"PASS":"FAIL",replacements+1,owned,guardHP-actor->GetCurrentHitPoints(0),clean);
                if(!clean) {phase=-1;return;}
                npc->SetCurrentHitPoints(0);phase=100;replacementAt=0;ticks=0;
                if(replacements>=2) fprintf(stderr,"K1_ARENA_REPEAT_DAMAGE_PASS rounds=%d\n",replacements);
            } else if(ticks>=600) {fprintf(stderr,"K1_ENGAGE_ARENA_FAIL replacement_damage\n");phase=-1;}
            return;
        }
        if(npc && !npc->GetDead()) {replacementAt=0;return;}
        if(state.mode!=K1EngageState::Mode::Exploration || K1EngageOwnsEnemy(opponentId)) return;
        if(!replacementAt) {replacementAt=now+2000000;return;}
        if(now<replacementAt) return;
        if(enemyProbe && replacements>=(corpseProbe?6:2)) {
            fprintf(stderr,"K1_ARENA_REPEAT_PASS replacements=%d final_restored=1\n",replacements);
            if(corpseProbe) {
                CSWGuiSaveLoadEntry entry;entry.LoadData(CExoString("000003 - Game2"));
                if(!entry.m_bSaveExists || entry.m_bCorrupt) {fprintf(stderr,"K1_CORPSE_RELOAD_FAIL fixture\n");phase=-1;return;}
                auto* panel=new CSWGuiSaveLoad(gui->m_pGuiManager,0,0);
                gui->m_pGuiManager->AddPanel(panel,2,1);
                const auto accepted=gui->m_pSoloModeQueryBox->m_bAccepted;
                gui->m_pSoloModeQueryBox->m_bAccepted=1;
                phase=92;panel->LoadGame(&entry);
                if(app->GetInGameGui()==gui) gui->m_pSoloModeQueryBox->m_bAccepted=accepted;
                return;
            }
            phase=-1;return;
        }
        // Choose a clear, walkable point around the player's current position,
        // rather than spawning into a player who has moved onto the old anchor.
        Vector spawn;bool clear=false;JkaWorld world(actor);
        for(int i=0;i<8 && !clear;++i) {
            const float angle=std::atan2(actor->m_orientation.y,actor->m_orientation.x)+i*.785398163f;
            spawn=actor->m_position+Vector(std::cos(angle)*3.f,std::sin(angle)*3.f,0);
            clear=static_cast<CSWSArea*>(actor->GetArea())->PositionWalkable(spawn) &&
                world.trace(JkaPoint(spawn),JkaPoint(spawn+Vector(.002f,0,0)),true).clear &&
                world.trace(JkaPoint(actor->m_position),JkaPoint(spawn),false).clear;
        }
        if(!clear) return; // Retry when space opens; never force a wall/actor overlap.
        auto* next=SpawnArenaOpponent(actor,spawn,gunner,rifle);
        if(!next) {fprintf(stderr,"K1_ENGAGE_ARENA_FAIL replacement_template\n");phase=-1;return;}
        retiredId=opponentId;
        if(npc) {
            retiredClients.push_back(app->ServerToClientObjectId(opponentId));
            npc->ClearAllActions(1);
            server->GetServerAIMaster()->AddEventDeltaTime(0,0,actor->m_objectId,
                opponentId,CServerAIMaster::EVENT_DESTROY_OBJECT,nullptr);
        }
        opponentId=next->m_objectId;++replacements;replacementAt=0;
        app->SetAutoPaused(1,CClientExoAppInternal::PAUSE_MODE_ENEMY_SIGHTED);
        fprintf(stderr,"K1_ARENA_REPLACEMENT round=%d old=%llu npc=%llu planning=%d position=%g,%g,%g\n",
            replacements+1,retiredId,opponentId,K1EngagePlanning(),spawn.x,spawn.y,spawn.z);
        if(enemyProbe) {
            if(!K1EngagePlanning() || !K1EngageEnter()) {
                fprintf(stderr,"K1_ENGAGE_ARENA_FAIL replacement_planning\n");phase=-1;return;
            }
            // Start the damage assertion in established melee reach; production
            // replacement placement remains three metres away.
            if(!gunner) next->SetPosition(actor->m_position+actor->m_orientation*1.1f,1,1,0);
            guardHP=actor->GetCurrentHitPoints(0);phase=101;ticks=0;
            if(std::getenv("K1_ENGAGE_DARK_JEDI_TEST")) phase=2;
        }
        return;
    }
    if(phase==1) {
        if(enemyProbe && !K1JkaAI::probe()) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL jka_jedi_source_probe\n");phase=-1;return;}
        if(navProbe && !K1JkaNav::probe()) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL jka_source_probe\n");phase=-1;return;}
        if(std::getenv("K1_JKA_DEFAULT_PROFILE_PROBE")) {
            JkaWorld regression(actor);
            const auto contact=regression.trace(JkaPoint(Vector(116.1964149475f,108.4818267822f,0)),
                JkaPoint(Vector(116.093f,109.175f,0)),true);
            const bool safe=!contact.clear && regression.rejectedOrigins>0;
            fprintf(stderr,"K1_JKA_BOUNDARY_REGRESSION_%s rejected_origins=%d\n",safe?"PASS":"FAIL",regression.rejectedOrigins);
            if(!safe) {phase=-1;return;}
        }
        // The native inventory retains the unequipped saber for equipment-menu switching.
        bool ok=StaffPrototype()?EquipFixtureSaber(actor):EquipFixtureWeapon(actor,"g_w_dblsbr001");
        if(auto* item=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON)) arenaSaberIds[StaffPrototype()?0:1]=item->m_objectId;
        ok=(StaffPrototype()?EquipFixtureWeapon(actor,"g_w_dblsbr001"):EquipFixtureSaber(actor)) && ok;
        if(auto* item=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON)) arenaSaberIds[StaffPrototype()?1:0]=item->m_objectId;
        if(DualPrototype()) {
            ok=EquipFixtureWeapon(actor,"g_w_lghtsbr03",CSWInventory::SLOT_LEFTWEAPON) && ok;
            auto* item=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON);
            arenaOffhandSaberId=item?item->m_objectId:OBJECT_INVALID;
        } else if(std::getenv("K1_ENGAGE_SANDBOX")) {
            auto* item=new CSWSItem(OBJECT_INVALID);
            if(!item->LoadFromTemplate(CResRef("g_w_lghtsbr03"))){delete item;ok=false;}
            else {item->m_flags&=~4;if(actor->AcquireItem(&item,OBJECT_INVALID,OBJECT_INVALID,1))arenaOffhandSaberId=item->m_objectId;else ok=false;}
        }
        if(std::getenv("K1_ENGAGE_SANDBOX")) {
            ok=GrantSandboxEquipmentPrerequisites(actor)&&ok;
            ok=AddArenaSwords(actor)&&ok;
        }
        ok=GrantArenaLoadout(actor)&&ok;
        if(const char* weapon=std::getenv("K1_ENGAGE_PLAYER_WEAPON"))ok=EquipFixtureWeapon(actor,weapon)&&ok;
        if(enemyProbe) for(unsigned char level=0;level<actor->m_pStats->GetLevel(0);++level)
            if(auto* entry=actor->m_pStats->GetLevelStats(level)) entry->m_nHitPointGain=100;
        const Vector spawn = gunner ? actor->m_position + actor->m_orientation * 2.f :
            Vector(190.471207f,99.885262f,0);
        sandboxReady=true;
        if(std::getenv("K1_ENGAGE_SANDBOX")) {
            sandboxSession=true;actor->SetCurrentHitPoints(actor->GetMaxHitPoints(0));phase=200;ticks=0;
            fprintf(stderr,"K1_SANDBOX_READY initial_npcs=0 loadout=%d\n",ok);return;
        }
        CSWSCreature* npc=SpawnArenaOpponent(actor,spawn,gunner,rifle,!enemyProbe);
        if(!npc) {fprintf(stderr,"K1_ENGAGE_ARENA_FAIL opponent_template\n");phase=-1;return;}

        actor->SetCurrentHitPoints(actor->GetMaxHitPoints(0));
        opponentId=npc->m_objectId;
        fprintf(stderr,"K1_ENGAGE_ARENA_%s module=%s actor=%llu npc=%llu position=%.4f,%.4f,%.4f party=%d\n",
            ok?"PASS":"FAIL",app->GetModule()->m_sModuleName.CStr(),actor->m_objectId,npc->m_objectId,
            actor->m_position.x,actor->m_position.y,actor->m_position.z,server->GetPartyTable()->m_nNumPartyMembers);
        // Start in planning with the player locked and the arena running slowly.
        app->SetAutoPaused(1,CClientExoAppInternal::PAUSE_MODE_ENEMY_SIGHTED);
        if (std::getenv("K1_ENGAGE_CONTROL_TEST")) {
            planningPosition=actor->m_position;
            planningWorldStart=server->GetWorldTimer()->GetSnapshotTime();
            planningUiStart=app->GetPauseTimer()->GetSnapshotTime();
            if (FILE* input=fopen("control-input.txt","w")) {
                fputs("keydown:26\n+15:keyup:26\n",input);
                fclose(input);
            }
        }
        if(enemyProbe || std::getenv("K1_ENGAGE_ARENA_TEST")) {phase=2;ticks=0;actor->SetCurrentHitPoints(1000);}
        else phase=100;
    } else if (enemyProbe) {
        CSWSCreature* npc = server->GetCreatureByGameObjectID(opponentId);
        if (!npc) { fprintf(stderr, "K1_ENGAGE_ENEMY_AI_FAIL missing_opponent\n"); phase=-1; return; }
        auto attack = [&]() {
            npc->AddToVisibleList(actor->m_objectId,1,0,0,0);
            npc->AddAttackActions(actor->m_objectId,0,1,0,0,0,0,1,SERVER_ANIMATION_ATTACK,1500,1,0,0,4,0);
        };
        if (phase==2) {
            if (!K1EngageEnter()) { fprintf(stderr, "K1_ENGAGE_ENEMY_AI_FAIL enter\n"); phase=-1; return; }
            npc->SetPosition(actor->m_position + actor->m_orientation * (gunner ? 2.f : 5.f),1,1,0);
            planningPosition=npc->m_position;
            guardHP=actor->GetCurrentHitPoints(0);
            attack();
            if(navProbe) {
                const Vector playerStart(204,103,actor->m_position.z),enemyStart(198,100,npc->m_position.z);
                if(!NpcStepClear(actor,playerStart) || !NpcStepClear(npc,enemyStart)) {
                    fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_fixture_positions\n");phase=-1;return;
                }
                actor->SetPosition(playerStart,1,1,0);
                actor->GetClientCreature()->SetPosition(playerStart);
                npc->SetPosition(enemyStart,1,1,0);
                phase=60;ticks=0;return;
            }
            phase=3;ticks=0;
        } else if(phase==60 && ticks>=60) {
            auto owned=npcPlayback.find(npc->m_objectId);
            if(owned==npcPlayback.end()) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_no_owner\n");phase=-1;return;}
            navRequests=owned->second.pathRequests;
            // Exercise source pursuit, independently of the source's random
            // multi-second defensive aggression ramp (covered by its probe).
            owned->second.jedi.aggression=6;
            const Vector moved(203,97,actor->m_position.z);
            JkaWorld fixtureWorld(actor);
            const auto destination=fixtureWorld.trace(JkaPoint(moved),JkaPoint(moved+Vector(.002f,0,0)),false);
            // This fixture deliberately relocates the target. Validate the
            // destination, not a walking segment the target will not execute.
            if(!static_cast<CSWSArea*>(actor->GetArea())->PositionWalkable(moved) || !destination.clear) {
                fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_target_position hit=%llu\n",destination.id);phase=-1;return;
            }
            actor->SetPosition(moved,1,1,0);actor->GetClientCreature()->SetPosition(moved);
            navDistance=magnitude(moved-npc->m_position);phase=61;ticks=0;
        } else if(phase==61 && ticks>=150) {
            auto owned=npcPlayback.find(npc->m_objectId);
            const float distance=magnitude(actor->m_position-npc->m_position);
            const bool tracked=owned!=npcPlayback.end() && owned->second.pathRequests>navRequests &&
                owned->second.pathRequests-navRequests<15 && distance<navDistance*.6f;
            fprintf(stderr,"K1_NPC_NAV_TRACK_%s before=%g after=%g requests=%d\n",tracked?"PASS":"FAIL",
                navDistance,distance,owned==npcPlayback.end()?-1:owned->second.pathRequests-navRequests);
            if(!tracked) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_track\n");phase=-1;return;}
            phase=62;ticks=0;
        } else if(phase==62) {
            npc->ClearAllActions(1);
            const Vector playerAt(204,100,actor->m_position.z),enemyAt(198,100,npc->m_position.z);
            actor->SetPosition(playerAt,1,1,0);actor->GetClientCreature()->SetPosition(playerAt);
            npc->SetPosition(enemyAt,1,1,0);
            auto* blocker=new CSWSCreature(OBJECT_INVALID,0);
            if(!blocker->LoadFromTemplate(CResRef("k1arenafoe"),0)) {
                delete blocker;fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_blocker_template\n");phase=-1;return;
            }
            blocker->m_bPassiveAttackBehaviour=1;
            blocker->m_pPathfindInfo->m_fPersonalSpace=1.f;
            blocker->m_pPathfindInfo->m_fCrePersonalSpace=1.f;
            blocker->AddToArea(static_cast<CSWSArea*>(actor->GetArea()),201,100,playerAt.z,1,1);
            laneBlockerId=blocker->m_objectId;
            if(NpcStepClear(npc,Vector(202.8f,100,playerAt.z))) {
                fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_obstruction_not_active\n");phase=-1;return;
            }
            navClearance=1000;phase=63;ticks=0;
        } else if(phase==63) {
            auto* blocker=server->GetCreatureByGameObjectID(laneBlockerId);
            if(!blocker) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_blocker_lost\n");phase=-1;return;}
            navClearance=std::min(navClearance,magnitude(npc->m_position-blocker->m_position));
            if(ticks>=300) {
                const float distance=magnitude(actor->m_position-npc->m_position);
                const bool routed=distance<2.f && navClearance>=1.1f;
                fprintf(stderr,"K1_NPC_NAV_OBSTACLE_%s distance=%g clearance=%g\n",routed?"PASS":"FAIL",distance,navClearance);
                blocker->RemoveFromArea(0);
                if(!routed) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_obstacle\n");phase=-1;return;}
                auto owned=npcPlayback.find(npc->m_objectId);
                if(owned==npcPlayback.end()) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_strafe_owner\n");phase=-1;return;}
                // A sustained lateral goal exercises the actual actor controller,
                // including the player's forward gait + 45-degree leg turn.
                npc->ClearAllActions(1);
                const auto now=server->GetWorldTimer()->GetSnapshotTime();
                const Vector lateral(-npc->m_orientation.y,npc->m_orientation.x,0);
                const Vector goal=npc->m_position+lateral*1.8f;
                if(!NpcStepClear(npc,goal)) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_strafe_position\n");phase=-1;return;}
                owned->second.nextThink=now+1200000;owned->second.nextAttack=now+2000000;
                owned->second.sourceJediCommands=false; // isolated route/gait goal, not a tactical command
                owned->second.maximumLegYaw=0;owned->second.nextPath=now;
                NpcMoveGoal(owned->second,npc,goal,true,now);
                phase=64;ticks=0;
            }
        } else if(phase==64 && ticks>=90) {
            const auto owned=npcPlayback.find(npc->m_objectId);
            const float yaw=owned==npcPlayback.end()?0:owned->second.maximumLegYaw;
            const bool turned=yaw>=.75f && yaw<=.785399f;
            fprintf(stderr,"K1_NPC_NAV_STRAFE_%s yaw=%g\n",turned?"PASS":"FAIL",yaw);
            if(!turned) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL nav_strafe\n");phase=-1;return;}
            // Exercise the imported A* against actual static arena geometry,
            // separately from the creature obstacle handled by STEER.
            bool worldRoute=false;int checked=0;
            if(owned!=npcPlayback.end() && owned->second.navigator) {
                auto& navigator=*owned->second.navigator;JkaWorld world(npc);
                const auto start=JkaPoint(npc->m_position);
                const int stride=std::max(1,navigator.nodes()/64);
                for(int node=1;node<=navigator.nodes() && !worldRoute;node+=stride) {
                    const auto goal=navigator.node(node);
                    const Vector offset=KotorPoint(goal)-npc->m_position;
                    if(magnitude(offset)<2.f || magnitude(offset)>15.f || std::fabs(offset.z)>.25f ||
                        world.trace(start,goal,true).clear) continue;
                    ++checked;K1JkaNav::State sample;sample.position=start;
                    sample.radius=npc->m_pPathfindInfo->m_fPersonalSpace/K1GroundMove::UnitScale;
                    const bool found=navigator.move(sample,goal,2.f,world,
                        server->GetWorldTimer()->GetSnapshotTime()/1000);
                    if(found && sample.searches>0 && sample.path.size()>1) {
                        worldRoute=true;
                        fprintf(stderr,"K1_JKA_WORLD_ROUTE_PASS nodes=%d path=%d searches=%d\n",
                            navigator.nodes(),int(sample.path.size()),sample.searches);
                    }
                }
            }
            if(!worldRoute) fprintf(stderr,"K1_JKA_WORLD_ROUTE_NOT_TESTED reachable_blocked_pair_not_found candidates=%d\n",checked);
            fprintf(stderr,"K1_NPC_NAV_PASS\n");npc->SetCurrentHitPoints(0);phase=12;ticks=0;
        } else if (phase==3 && ticks>=(std::getenv("K1_JKA_DEFAULT_PROFILE_PROBE")?750:150)) {
            const float displacement=magnitude(npc->m_position-planningPosition);
            fprintf(stderr, "K1_ENEMY_AI_FIXTURE ranged=%d initial_move=%g player_hp=%d\n",
                gunner, displacement, actor->GetCurrentHitPoints(0));
            if (displacement<.35f) { fprintf(stderr, "K1_ENGAGE_ENEMY_AI_FAIL no_move\n"); phase=-1; return; }
            if(navigationReloadStage==2) {
                const auto ready=npcNavigators.find(static_cast<CSWSArea*>(actor->GetArea())->m_objectId);
                if(ready==npcNavigators.end() || ready->second.uses<1) {
                    fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL reloaded_cache_unused\n");phase=-1;return;
                }
                fprintf(stderr,"K1_JKA_NAV_RELOAD_PASS generation=%llu moved=%g\n",ready->second.generation,displacement);
                navigationReloadStage=3;npc->SetCurrentHitPoints(0);phase=12;ticks=0;return;
            }
            if (gunner) { phase=6;ticks=0;return; }
            npc->ClearAllActions(1);
            npc->m_pCombatRound->EndCombatRound(1);
            // This phase checks physical contact/damage, independently of
            // source tactics at the range boundary. The former fixed1.55m
            // position can exceed this body's calculated1.47m melee reach.
            const float contactDistance=K1EngageEnemyMeleeRange(npc)*.8f;
            npc->SetPosition(actor->m_position + actor->m_orientation * contactDistance,1,1,0);
            fprintf(stderr,"K1_NPC_CONTACT_FIXTURE distance=%g reach=%g\n",contactDistance,K1EngageEnemyMeleeRange(npc));
            K1EngageMouse(true,true);
            K1EngageMouse(true,false);
            phase=4;ticks=0;
        } else if (phase==4) {
            if (K1EngageEnemyFacesSwing(npc)) {
                attack();phase=5;ticks=0;
            } else if (ticks>60) {
                fprintf(stderr, "K1_ENGAGE_ENEMY_AI_FAIL no_player_swing\n"); phase=-1;
            }
        } else if (phase==5 && ticks>=120) {
            actor->SetCurrentHitPoints(actor->GetMaxHitPoints(0));
            phase=6;ticks=0;guardHP=actor->GetCurrentHitPoints(0);
        } else if (phase==6) {
            if(!gunner) {
                const auto owned=npcPlayback.find(npc->m_objectId);
                if(owned==npcPlayback.end() || !owned->second.completedChains || !owned->second.attacking) {
                    if(ticks>=480) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL no_three_swing_chain\n");phase=-1;}
                    return;
                }
                fprintf(stderr,"K1_NPC_CHAIN_PASS completed=%d links=%d cap=%d\n",owned->second.completedChains,owned->second.chainLinks,maxChainSwings);
            }
            if(actor->GetCurrentHitPoints(0)<guardHP) {
                fprintf(stderr,"K1_ENEMY_AI_DAMAGE_PASS ranged=%d before=%d after=%d\n",
                    gunner,guardHP,actor->GetCurrentHitPoints(0));
                if(!gunner && !attackWalkFixtureDone) {
                    auto owned=npcPlayback.find(npc->m_objectId);
                    if(owned==npcPlayback.end() || !owned->second.attacking) {
                        fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL attack_walk_owner\n");phase=-1;return;
                    }
                    const auto now=server->GetWorldTimer()->GetSnapshotTime();
                    const Vector side(-npc->m_orientation.y,npc->m_orientation.x,0);
                    const Vector goal=npc->m_position+side*.65f;
                    if(!NpcStepClear(npc,goal)) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL attack_walk_goal\n");phase=-1;return;}
                    owned->second.nextThink=now+1000000;owned->second.nextPath=now;
                    owned->second.sourceJediCommands=false;owned->second.nextSteer=now;
                    // Deliberately request a run goal: active melee policy must
                    // override its speed and gait while the real swing finishes.
                    NpcMoveGoal(owned->second,npc,goal,false,now);
                    phase=32;ticks=0;return;
                }
                npc->ClearAllActions(1);npc->m_pCombatRound->EndCombatRound(1);
                npc->m_pCombatRound->RemoveAllActions();
                if(!gunner) {
                    auto owned=npcPlayback.find(npc->m_objectId);
                    if(owned==npcPlayback.end()) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL guard_owner\n");phase=-1;return;}
                    const auto now=server->GetWorldTimer()->GetSnapshotTime();
                    owned->second.attacking=false;owned->second.nextAttack=now+3000000;
                    owned->second.parryReady=0;owned->second.threatSince=0;owned->second.guardBlocks=0;
                    // This assertion measures blocking a connected swing, not
                    // a successful timed strafe away from the scripted contact.
                    StopNpcMove(owned->second,npc);owned->second.nextThink=now+3000000;
                    enemyRepositionAfter[npc->m_objectId]=now+3000000;
                    const Vector guardPosition=actor->m_position+actor->m_orientation*1.25f;
                    npc->SetPosition(guardPosition,1,1,0);
                    owned->second.position=guardPosition;owned->second.previousPhysics=guardPosition;owned->second.physicsRemainder=0;
                    guardHP=npc->GetCurrentHitPoints(0);
                    K1EngageMouse(true,true);K1EngageMouse(true,false);
                    phase=20;ticks=0;return;
                }
                // Put a same-faction noncombatant in the actual firing lane.
                auto* blocker=new CSWSCreature(OBJECT_INVALID,0);
                if(!blocker->LoadFromTemplate(CResRef("k1arenafoe"),0)) {
                    delete blocker;fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL blocker_template\n");phase=-1;return;
                }
                npc->SetPosition(actor->m_position+actor->m_orientation*4.f,1,1,0);
                const Vector midpoint=(actor->m_position+npc->m_position)*.5f;
                blocker->AddToArea(static_cast<CSWSArea*>(actor->GetArea()),midpoint.x,midpoint.y,midpoint.z,1,1);
                laneBlockerId=blocker->m_objectId;
                blocker->m_bPassiveAttackBehaviour=1;
                fprintf(stderr,"K1_ENEMY_AI_LANE_SETUP ally=%llu reputation=%d\n",
                    laneBlockerId,npc->GetReputation(laneBlockerId,0));
                planningPosition=npc->m_position;attack();phase=7;ticks=0;
            } else if(ticks>=480) {
                fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL no_damage ranged=%d hp=%d distance=%g\n",
                    gunner,actor->GetCurrentHitPoints(0),magnitude(npc->m_position-actor->m_position));phase=-1;
            } else if(!npc->m_pCombatRound->m_bRoundStarted && npc->m_actionList.m_pInternal->m_nCount==0) {
                attack();
            }
        } else if(phase==32) {
            auto owned=npcPlayback.find(npc->m_objectId);
            if(owned==npcPlayback.end()) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL attack_walk_lost\n");phase=-1;return;}
            if(ticks>=30 && owned->second.attackWalkMoving>3 && owned->second.attackWalkCooldown>0) {
                if(!owned->second.attackWalkOK) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL attack_walk_speed\n");phase=-1;return;}
                StopNpcMove(owned->second,npc);
                const float previousRadius=K1EngageEnemyMeleeRange(npc)+npc->m_pPathfindInfo->m_fCrePersonalSpace;
                const Vector near=actor->m_position+actor->m_orientation*(previousRadius+.375f);
                JkaWorld placement(npc);
                // This is a fixture teleport, not a walking segment through
                // the player. Validate the destination cylinder independently.
                if(!static_cast<CSWSArea*>(npc->GetArea())->PositionWalkable(near) ||
                    !placement.trace(JkaPoint(near),JkaPoint(near+Vector(.002f,0,0)),false).clear) {
                    fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL attack_walk_band_goal\n");phase=-1;return;
                }
                npc->SetPosition(near,1,1,0);owned->second.position=near;owned->second.previousPhysics=near;
                const auto now=server->GetWorldTimer()->GetSnapshotTime();
                owned->second.physicsRemainder=0;owned->second.nextThink=now+2000000;owned->second.nextAttack=now+2000000;
                // Hold a completed swing outside the old radius; only the new
                // retention band can keep this run-intent opponent walking.
                owned->second.attacking=false;owned->second.intentWalk=false;
                const Vector guardGoal=near+Vector(-actor->m_orientation.y,actor->m_orientation.x,0)*.45f;
                if(!NpcStepClear(npc,guardGoal)) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL close_guard_goal\n");phase=-1;return;}
                NpcMoveGoal(owned->second,npc,guardGoal,true,now);
                phase=34;ticks=0;
            } else if(ticks>=180) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL attack_walk_moving_or_cooldown\n");phase=-1;}
        } else if(phase==34 && ticks>=30) {
            auto owned=npcPlayback.find(npc->m_objectId);
            const float distance=magnitude(npc->m_position-actor->m_position);
            const float previousRadius=K1EngageEnemyMeleeRange(npc)+npc->m_pPathfindInfo->m_fCrePersonalSpace;
            const bool retained=owned!=npcPlayback.end() && owned->second.meleeAttackWalk &&
                !owned->second.attacking && !npc->m_bRunning && K1EngageEnemyForceWalk(npc->m_objectId) &&
                distance>previousRadius && distance<previousRadius+meleeWalkExtraDistance;
            fprintf(stderr,"K1_NPC_WALK_BAND_%s distance=%g old_radius=%g extra=%g\n",retained?"PASS":"FAIL",
                distance,previousRadius,meleeWalkExtraDistance);
            if(!retained) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL attack_walk_band\n");phase=-1;return;}
            const bool guarded=owned->second.closeGuard && NpcGuarding(owned->second) &&
                owned->second.closeGuardMoving>0 && owned->second.closeGuardStill>0;
            fprintf(stderr,"K1_NPC_CLOSE_GUARD_%s moving=%d still=%d\n",guarded?"PASS":"FAIL",
                owned->second.closeGuardMoving,owned->second.closeGuardStill);
            if(!guarded) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL close_guard_pose\n");phase=-1;return;}
            const Vector far=actor->m_position+actor->m_orientation*4.f;
            JkaWorld placement(npc);
            if(!static_cast<CSWSArea*>(npc->GetArea())->PositionWalkable(far) ||
                !placement.trace(JkaPoint(far),JkaPoint(far+Vector(.002f,0,0)),false).clear) {
                fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL attack_walk_chase_goal\n");phase=-1;return;
            }
            npc->SetPosition(far,1,1,0);owned->second.position=far;owned->second.previousPhysics=far;
            owned->second.physicsRemainder=0;owned->second.nextThink=0;owned->second.nextSteer=0;
            // Test release of the forced-walk policy against explicit run intent.
            // Source Jedi timers may independently request a walking strafe.
            const auto chaseAt=server->GetWorldTimer()->GetSnapshotTime();
            owned->second.nextThink=chaseAt+1000000;owned->second.nextPath=0;
            owned->second.sourceJediCommands=false;
            NpcMoveGoal(owned->second,npc,actor->m_position,false,chaseAt);
            phase=33;ticks=0;
        } else if(phase==33) {
            auto owned=npcPlayback.find(npc->m_objectId);
            if(owned!=npcPlayback.end() && owned->second.attackWalkChase>0) {
                const auto& sample=owned->second;
                fprintf(stderr,"K1_NPC_ATTACK_WALK_%s samples=%d moving=%d cooldown=%d chase=%d max_speed=%g\n",
                    sample.attackWalkOK?"PASS":"FAIL",sample.attackWalkSamples,sample.attackWalkMoving,
                    sample.attackWalkCooldown,sample.attackWalkChase,sample.attackWalkMaxSpeed);
                if(!sample.attackWalkOK) {phase=-1;return;}
                attackWalkFixtureDone=true;phase=6;ticks=0;
            } else if(ticks>=180) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL attack_walk_chase_resume\n");phase=-1;}
        } else if(phase==7 && ticks>=150) {
            const float displacement=magnitude(npc->m_position-planningPosition);
            fprintf(stderr,"K1_ENEMY_AI_LANE_%s moved=%g\n",displacement>.7f?"PASS":"FAIL",displacement);
            if(displacement<=.7f) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL blocked_lane\n");phase=-1;return;}
            auto* blocker=server->GetCreatureByGameObjectID(laneBlockerId);
            if(blocker) blocker->RemoveFromArea(0);
            phase=10;ticks=0;
        } else if(phase==20 && ticks>=90) {
            const auto owned=npcPlayback.find(npc->m_objectId);
            const bool blocked=owned!=npcPlayback.end() && owned->second.guardBlocks>0 && npc->GetCurrentHitPoints(0)==guardHP;
            fprintf(stderr,"K1_NPC_GUARD_%s before=%d after=%d\n",blocked?"PASS":"FAIL",guardHP,npc->GetCurrentHitPoints(0));
            if(!blocked) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL guard\n");phase=-1;return;}
            phase=21;ticks=0;
        } else if(phase==21) {
            recoveryPlaced=false;
            const auto previous=npcPlayback.find(npc->m_objectId);
            reactionSerial=previous!=npcPlayback.end()?previous->second.serial:0;
            reactionGripSamples=previous!=npcPlayback.end()?previous->second.gripSamples:0;
            unsigned char slot=0;
            while(slot<actor->m_pStats->m_numClasses && !actor->m_pStats->GetSpellUsesLeft(forceSpells[forceTrial],slot,0,0)) ++slot;
            npc->m_hitPoints=1000;npc->SetCurrentHitPoints(npc->GetMaxHitPoints(0));
            const bool queued=slot<actor->m_pStats->m_numClasses && actor->AddCastSpellActions(forceSpells[forceTrial],slot,0,0,0,
                npc->m_position,npc->m_objectId,1,0,1,0,0,0,-1,0xff,0)!=0;
            if(!queued) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL reaction_cast_queue\n");phase=-1;return;}
            phase=22;ticks=0;
        } else if(phase==22) {
            if(npc->m_nState==forceStates[forceTrial]) {
                phase=23;ticks=0;
            } else if(ticks>=480) {
                fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL reaction_no_choke state=%d\n",npc->m_nState);phase=-1;
            }
        } else if(phase==23 && ticks>=30) {
            auto* client=npc->GetClientCreature();auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
            const auto clip=client->GetAnimationBase()->GetAnimationName(forceAnimations[forceTrial]);float t=0,len=0;
            const bool reacting=body && !IsSourceBody(body) && !K1EngageOwnsEnemy(npc->m_objectId) &&
                body->GetAnimation(const_cast<char*>(clip.CStr()),&t,&len) && t>0;
            fprintf(stderr,"K1_NPC_FORCE_REACTION_%s spell=%d state=%d clip=%s time=%g native=%d\n",
                reacting?"PASS":"FAIL",forceSpells[forceTrial],npc->m_nState,clip.CStr(),t,body&&!IsSourceBody(body));
            if(!reacting) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL reaction_pose\n");phase=-1;return;}
            if(std::getenv("K1_ENGAGE_FORCE_TARGET_TEST")) {
                std::vector<K1SaberContact::Triangle> targetTriangles;CollisionTriangles(body,targetTriangles);
                CollisionTriangles(client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1),targetTriangles);
                fprintf(stderr,"K1_FORCE_NATIVE_GEOMETRY_%s kind=affected spell=%d state=%d triangles=%zu\n",targetTriangles.empty()?"FAIL":"PASS",forceSpells[forceTrial],npc->m_nState,targetTriangles.size());
                if(targetTriangles.empty()){fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL force_target_geometry\n");phase=-1;return;}
            }
            // Exercise actual effect expiry, not a fixture-only bulk removal
            // that can leave linked state records behind.
            phase=24;ticks=0;
        } else if(phase==24 && ticks>=120) {
            const auto owned=npcPlayback.find(npc->m_objectId);
            if(NativeStatusAnimation(npc) && ticks<900) {
                auto* body=npc->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
                if(K1EngageOwnsEnemy(npc->m_objectId) || IsSourceBody(body) || K1EngageOwnsAnimation(body) ||
                    (owned!=npcPlayback.end() && (owned->second.requested || owned->second.attacking ||
                        owned->second.chainSwings || owned->second.parrying || owned->second.closeGuard))) {
                    fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL reaction_interrupted\n");phase=-1;
                }
                return;
            }
            if(!recoveryPlaced && owned!=npcPlayback.end() && owned->second.requested) {
                // Effect recovery is an animation/ownership check. Keep the
                // completed attack in established reach, independently of the
                // source integer-distance versus native float-reach boundary.
                const Vector contact=actor->m_position+actor->m_orientation*1.25f;
                JkaWorld placement(npc);
                if(!static_cast<CSWSArea*>(npc->GetArea())->PositionWalkable(contact) ||
                    !placement.trace(JkaPoint(contact),JkaPoint(contact+Vector(.002f,0,0)),false).clear) {
                    fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL recovery_contact_fixture\n");phase=-1;return;
                }
                StopNpcMove(owned->second,npc);npc->SetPosition(contact,1,1,0);
                owned->second.position=contact;owned->second.previousPhysics=contact;owned->second.physicsRemainder=0;
                const auto now=server->GetWorldTimer()->GetSnapshotTime();
                owned->second.nextThink=now+3000000;owned->second.nextAttack=now;recoveryPlaced=true;
                fprintf(stderr,"K1_NPC_RECOVERY_CONTACT_FIXTURE spell=%d distance=1.25\n",forceSpells[forceTrial]);
            }
            const bool resumed=!NativeStatusAnimation(npc) && owned!=npcPlayback.end() && !owned->second.reaction &&
                owned->second.serial>reactionSerial && !owned->second.attacking && owned->second.continuousLayers>0 &&
                owned->second.gripSamples>reactionGripSamples+10 && K1EngageOwnsEnemy(npc->m_objectId);
            if(!resumed && ticks<900) return;
            fprintf(stderr,"K1_NPC_FORCE_RECOVER_%s state=%d\n",resumed?"PASS":"FAIL",npc->m_nState);
            if(!resumed) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL reaction_recover\n");phase=-1;return;}
            if(std::getenv("K1_ENGAGE_FORCE_TARGET_TEST") && ++forceTrial<3) {phase=21;ticks=0;return;}
            // Source tactics need not naturally idle for a particular sample
            // count. Finish this attack, then explicitly hold a settled idle
            // before checking the unchanged native-idle/grip thresholds.
            StopNpcMove(owned->second,npc);
            const auto idleAt=server->GetWorldTimer()->GetSnapshotTime();
            owned->second.nextThink=idleAt+6000000;
            owned->second.nextAttack=idleAt+6000000;
            // Native idle is now an out-of-guard-range state. Keep this explicit
            // idle fixture outside proximity guard rather than suppressing it.
            const Vector idlePosition=actor->m_position+actor->m_orientation*4.f;
            JkaWorld idleWorld(npc);
            if(!static_cast<CSWSArea*>(npc->GetArea())->PositionWalkable(idlePosition) ||
                !idleWorld.trace(JkaPoint(idlePosition),JkaPoint(idlePosition+Vector(.002f,0,0)),false).clear) {
                fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL idle_guard_clearance\n");phase=-1;return;
            }
            npc->SetPosition(idlePosition,1,1,0);owned->second.position=idlePosition;
            owned->second.previousPhysics=idlePosition;owned->second.physicsRemainder=0;
            phase=29;ticks=0;
        } else if(phase==29) {
            const auto owned=npcPlayback.find(npc->m_objectId);
            // Attack completion writes the normal150ms cooldown. Maintain this
            // fixture's hold until the newly acquired body's idle samples exist.
            if(owned!=npcPlayback.end()) {
                const auto idleAt=server->GetWorldTimer()->GetSnapshotTime();
                owned->second.nextAttack=idleAt+6000000;owned->second.nextThink=idleAt+6000000;
                StopNpcMove(owned->second,npc);
            }
            if(owned!=npcPlayback.end() && !owned->second.attacking && owned->second.idleSamples>10) {
                phase=10;ticks=0;
            } else if(ticks>=300) {
                fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL settled_idle_coverage\n");phase=-1;
            }
        } else if(phase==10) {
            if(!gunner) {
                const auto owned=npcPlayback.find(npc->m_objectId);
                const bool parity=owned!=npcPlayback.end() && owned->second.parityOK &&
                    owned->second.idleSamples>10 && owned->second.continuousLayers>0 &&
                    owned->second.idleError<.00002f && owned->second.maximumLegYaw<=.785399f &&
                    owned->second.gripSamples>10 && owned->second.maximumGripError<.0002f;
                fprintf(stderr,"K1_NPC_PARITY_%s idle=%d layers=%d idle_error=%g yaw=%g grip_samples=%d grip_error=%g source_lateral=%d\n",parity?"PASS":"FAIL",
                    owned==npcPlayback.end()?0:owned->second.idleSamples,owned==npcPlayback.end()?0:owned->second.continuousLayers,
                    owned==npcPlayback.end()?0:owned->second.idleError,owned==npcPlayback.end()?0:owned->second.maximumLegYaw,
                    owned==npcPlayback.end()?0:owned->second.gripSamples,owned==npcPlayback.end()?0:owned->second.maximumGripError,
                    owned==npcPlayback.end()?0:owned->second.sourceLateralSamples);
                if(!parity) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL animation_parity\n");phase=-1;return;}
            }
            npc->ClearAllActions(1);npc->m_pCombatRound->EndCombatRound(1);
            npc->m_pCombatRound->RemoveAllActions();npc->SetCombatState(0,0);
            npc->RemoveFromVisibleList(actor->m_objectId);
            // Exercise the real action boundary without advancing perception:
            // a queued attack must complete without starting a round once unseen.
            // AddAttackActions normally queues a deferred combat-round action;
            // use its decoded physical-action fields for this synchronous check.
            CSWSObjectActionNode unseenAttack;
            unseenAttack.m_args.m_bAttackCutscene=0;
            unseenAttack.m_args.m_attackQueuedTargetId=actor->m_objectId;
            unseenAttack.m_args.m_attackKind=1;
            unseenAttack.m_args.m_attackAnimationId=SERVER_ANIMATION_ATTACK;
            unseenAttack.m_args.m_attackPauseTime=1500;
            unseenAttack.m_args.m_attackNumAttacks=1;
            unseenAttack.m_args.m_attackFeatId=0;
            const bool lost=!npc->HasTargetBeenDetected(actor->m_objectId,1) &&
                npc->AIActionPhysicalAttacks(&unseenAttack)==CSWSObject::ACTION_COMPLETE &&
                !npc->m_pCombatRound->m_bRoundStarted;
            fprintf(stderr,"K1_ENEMY_AI_UNSEEN_%s\n",lost?"PASS":"FAIL");
            npc->ClearAllActions(1);
            if(!lost) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL unseen_attack\n");phase=-1;return;}
            // Clearing KotOR's action queue no longer cancels JKA pursuit.
            // Verify the encounter stays owned while that pursuit is active,
            // then exercise the driver's unseen timeout without perception
            // reacquiring the visible fixture target between these calls.
            auto pursuit=npcPlayback.find(npc->m_objectId);
            if(pursuit!=npcPlayback.end()) {
                const auto goalAt=server->GetWorldTimer()->GetSnapshotTime()+200001;
                pursuit->second.nextThink=goalAt; // end the preceding idle fixture's hold
                pursuit->second.lastSeen=actor->m_position;
                pursuit->second.lastSeenAt=goalAt;
                NpcMoveGoal(pursuit->second,npc,actor->m_position,false,goalAt);
            }
            const bool chasing=pursuit!=npcPlayback.end() && pursuit->second.hasMove && pursuit->second.navigation.active;
            if(!chasing || K1EngageTryDisengage()) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL pursuit_released_early\n");phase=-1;return;}
            if(pursuit!=npcPlayback.end()) {
                const auto unseenAt=server->GetWorldTimer()->GetSnapshotTime()+5200002;
                ThinkNpcCombat(pursuit->second,npc,actor,unseenAt,false);
            }
            fprintf(stderr,"K1_NPC_PURSUIT_LIFETIME_PASS held=%d timeout_ms=5000 clock=component_input\n",chasing);
            const bool inactive=K1EngageTryDisengage() && !K1EngageEnemyAIActive(actor->m_objectId);
            fprintf(stderr,"K1_ENEMY_AI_RELEASE_%s\n",inactive?"PASS":"FAIL");
            if(!inactive) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL release\n");phase=-1;return;}
            actor->SetCurrentHitPoints(actor->GetMaxHitPoints(0));
            npc->AddToVisibleList(actor->m_objectId,1,0,0,0);
            app->SetAutoPaused(1,CClientExoAppInternal::PAUSE_MODE_ENEMY_SIGHTED);
            const bool restored=K1EngageEnter() && K1EngageEnemyAIActive(actor->m_objectId);
            fprintf(stderr,"K1_ENEMY_AI_REENTER_%s\n",restored?"PASS":"FAIL");
            if(!restored) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL reenter\n");phase=-1;return;}
            guardHP=actor->GetCurrentHitPoints(0);attack();phase=11;ticks=0;
        } else if(phase==11) {
            if(actor->GetCurrentHitPoints(0)<guardHP) {
                fprintf(stderr,"K1_ENEMY_AI_LIFECYCLE_PASS ranged=%d before=%d after=%d\n",
                    gunner,guardHP,actor->GetCurrentHitPoints(0));
                K1EngagePauseInput(false);phase=25;ticks=0;
            } else if(ticks>=480) {
                fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL reacquired_damage ranged=%d\n",gunner);phase=-1;
            } else if(!npc->m_pCombatRound->m_bRoundStarted && npc->m_actionList.m_pInternal->m_nCount==0) {
                attack();
            }
        } else if(phase==25) {
            const auto owned=npcPlayback.find(npc->m_objectId);float length=0;
            if(owned==npcPlayback.end() || !owned->second.attacking || !owned->second.overlay ||
                !owned->second.body->GetAnimation(const_cast<char*>(owned->second.overlay),&npcPlanningTime,&length)) {
                fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL planning_ownership\n");phase=-1;return;
            }
            npcPlanningSerial=owned->second.serial;
            planningWorldStart=server->GetWorldTimer()->GetSnapshotTime();
            planningUiStart=app->GetPauseTimer()->GetSnapshotTime();
            phase=26;ticks=0;
        } else if(phase==26 && ticks>=120) {
            const auto owned=npcPlayback.find(npc->m_objectId);float time=0,length=0;
            const long long world=server->GetWorldTimer()->GetSnapshotTime()-planningWorldStart;
            const long long ui=app->GetPauseTimer()->GetSnapshotTime()-planningUiStart;
            const bool slow=K1EngagePlanning() && !IsGamePaused() && owned!=npcPlayback.end() &&
                owned->second.serial==npcPlanningSerial && owned->second.attacking &&
                owned->second.overlay && owned->second.body->GetAnimation(const_cast<char*>(owned->second.overlay),&time,&length) &&
                time>npcPlanningTime && time-npcPlanningTime<.03f &&
                world*1000>ui*3 && world*1000<ui*7 && !npc->m_pCombatRound->m_bRoundStarted;
            fprintf(stderr,"K1_NPC_PLANNING_%s world_us=%lld ui_us=%lld clip_delta=%g\n",slow?"PASS":"FAIL",world,ui,time-npcPlanningTime);
            if(!slow) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL planning_clock\n");phase=-1;return;}
            K1EngageEnter();
            if(!gunner) {
                if(!GrantArenaLoadout(npc)) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL npc_cast_loadout\n");phase=-1;return;}
                if(std::getenv("K1_ENGAGE_FORCE_TARGET_TEST")) {
                    // Rendering needs an applied effect, not a random successful
                    // defence. K1 caps effect save bonuses at20 and rolls1..20.
                    // Only this unsaved caster is changed; native save rules run.
                    npc->m_pStats->SetWISBase(100);
                    const int maximumSave=int(actor->m_pStats->GetFortSavingThrow(1))+40;
                    const int dc=npc->CalculateSpellSaveDC(FORCE_POWER_CHOKE);
                    fprintf(stderr,"K1_PLAYER_EFFECT_SAVE_FIXTURE dc=%d maximum_save=%d\n",dc,maximumSave);
                    if(dc<=maximumSave) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL player_effect_save_fixture\n");phase=-1;return;}
                }
                if(!npc->AddCastSpellActions(FORCE_POWER_CHOKE,0,0,0,0,
                    actor->m_position,actor->m_objectId,1,0,1,0,0,0,-1,0xff,0)) {
                    fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL npc_cast_queue\n");phase=-1;return;
                }
                phase=27;ticks=0;
            } else {npc->SetCurrentHitPoints(0);phase=12;ticks=0;}
        } else if(phase==27) {
            auto* client=npc->GetClientCreature();auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
            const char* nativeClip=nullptr;
            if(body) for(int i=0;i<body->m_paAnimations.m_count;++i) {
                const char* name=body->m_paAnimations.m_pData[i]->m_pAnimation->m_szName;
                for(int animation:{CSWCAnimBase::CLIENT_ANIMATION_CAST_OUT_1,CSWCAnimBase::CLIENT_ANIMATION_CAST_OUT_2,
                    CSWCAnimBase::CLIENT_ANIMATION_CAST_OUT_3,CSWCAnimBase::CLIENT_ANIMATION_CAST_OUT_LOOP_1,
                    CSWCAnimBase::CLIENT_ANIMATION_CAST_OUT_LOOP_2,CSWCAnimBase::CLIENT_ANIMATION_CAST_OUT_LOOP_3}) {
                    auto expected=client->GetAnimationBase()->GetAnimationName(animation);
                    if(!strcmp(name,expected.CStr())) nativeClip=name;
                }
            }
            if(body && nativeClip && !IsSourceBody(body) && !K1EngageOwnsEnemy(npc->m_objectId) && npc->m_pCombatRound->m_bSpellCastRound) {
                if(std::getenv("K1_ENGAGE_FORCE_TARGET_TEST")) {
                    std::vector<K1SaberContact::Triangle> targetTriangles;CollisionTriangles(body,targetTriangles);
                    CollisionTriangles(client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1),targetTriangles);
                    fprintf(stderr,"K1_FORCE_NATIVE_GEOMETRY_%s kind=casting triangles=%zu\n",targetTriangles.empty()?"FAIL":"PASS",targetTriangles.size());
                    if(targetTriangles.empty()){fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL force_caster_geometry\n");phase=-1;return;}
                }
                const Vector contact=actor->m_position+actor->m_orientation*1.25f;
                JkaWorld placement(npc);
                if(!static_cast<CSWSArea*>(npc->GetArea())->PositionWalkable(contact) ||
                    !placement.trace(JkaPoint(contact),JkaPoint(contact+Vector(.002f,0,0)),false).clear) {
                    fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL npc_cast_resume_placement\n");phase=-1;return;
                }
                npc->SetPosition(contact,1,1,0);
                fprintf(stderr,"K1_NPC_UTILITY_CONTACT_FIXTURE distance=1.25 reach=%g\n",K1EngageEnemyMeleeRange(npc));
                fprintf(stderr,"K1_NPC_UTILITY_NATIVE_PASS clip=%s\n",nativeClip);phase=28;ticks=0;
            } else if(ticks>=480) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL npc_cast_native\n");phase=-1;}
        } else if(phase==28) {
            const auto owned=npcPlayback.find(npc->m_objectId);
            if(!NativeUtilityQueued(npc) && owned!=npcPlayback.end() && owned->second.attacking) {
                fprintf(stderr,"K1_NPC_UTILITY_RESUME_PASS\n");
                if(std::getenv("K1_ENGAGE_FORCE_TARGET_TEST")) {phase=35;ticks=0;}
                else {npc->SetCurrentHitPoints(0);phase=12;ticks=0;}
            } else if(ticks>=480) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL npc_cast_resume\n");phase=-1;}
        } else if(phase==35) {
            if(playerNativeEffectSampled) {npc->SetCurrentHitPoints(0);phase=12;ticks=0;}
            else if(ticks>=480) {fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL player_effect_not_observed state=%d health=%d\n",actor->m_nState,actor->GetCurrentHitPoints(0));phase=-1;}
            else if(ticks%60==0) fprintf(stderr,"K1_PLAYER_EFFECT_WAIT state=%d health=%d npc_round=%d native_queued=%d\n",
                actor->m_nState,actor->GetCurrentHitPoints(0),npc->m_pCombatRound->m_bSpellCastRound,NativeUtilityQueued(npc));
        } else if(phase==12 && ticks>=60) {
            auto* client=npc->GetClientCreature();
            auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
            const bool restored=body && !IsSourceBody(body) && !K1EngageOwnsEnemy(npc->m_objectId) &&
                state.mode==K1EngageState::Mode::Exploration;
            fprintf(stderr,"K1_NPC_VICTORY_RESTORE_%s ranged=%d\n",restored?"PASS":"FAIL",gunner);
            if(!restored) fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL victory_restore\n");
            if(restored && std::getenv("K1_JKA_DEFAULT_PROFILE_PROBE") && navigationReloadStage==0) {
                reloadGeneration=navigationGeneration;reloadUnloads=navigationUnloads;
                navigationReloadStage=1;phase=90;ticks=0;
                fprintf(stderr,"K1_JKA_NAV_RELOAD_BEGIN generation=%llu\n",reloadGeneration);
                // Use the native in-game load path. The startup-only helper
                // creates a server and is invalid while an old scene is live.
                CSWGuiSaveLoadEntry entry;
                entry.LoadData(CExoString("000003 - Game2"));
                if(!entry.m_bSaveExists || entry.m_bCorrupt) {
                    fprintf(stderr,"K1_ENGAGE_ENEMY_AI_FAIL reload_fixture_save\n");phase=-1;return;
                }
                auto* gui=app->GetInGameGui();
                auto* panel=new CSWGuiSaveLoad(gui->m_pGuiManager,0,0);
                gui->m_pGuiManager->AddPanel(panel,2,1);
                const auto accepted=gui->m_pSoloModeQueryBox->m_bAccepted;
                gui->m_pSoloModeQueryBox->m_bAccepted=1; // explicit unsaved fixture reload
                fprintf(stderr,"K1_JKA_NAV_RELOAD_REQUEST mode=ingame\n");
                panel->LoadGame(&entry);
                if(app->GetInGameGui()==gui) gui->m_pSoloModeQueryBox->m_bAccepted=accepted;
                return;
            }
            phase=restored && !navProbe && !std::getenv("K1_JKA_DEFAULT_PROFILE_PROBE")?100:-1;
            replacementAt=0;ticks=0;
        }
    } else if(phase==2) {
        bool entered=K1EngageEnter();fprintf(stderr,"K1_ENGAGE_ARENA_TEST enter=%d\n",entered);phase=3;ticks=0;
    } else if(phase==3) {
        K1EngageMouse(false,true);guardHP=actor->GetCurrentHitPoints(0);phase=4;ticks=0;
    } else if(phase==4) {
        CSWSCreature* npc=server->GetCreatureByGameObjectID(opponentId);
        Vector delta=npc ? npc->m_position-actor->m_position : Vector(0,0,0);
        float distance=npc ? std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z) : -1;
        if(ticks%60==0) fprintf(stderr,"K1_ENGAGE_ARENA_TEST guard=%d force_walk=%d round=%d spell=%d distance=%g hp=%d\n",state.Guarding(),K1EngageForceWalk(),actor->m_pCombatRound->m_bRoundStarted,actor->m_pCombatRound->m_bSpellCastRound,distance,actor->GetCurrentHitPoints(0));
        if(ticks>=600) {
            bool pass=npc && distance<2.1f && state.Guarding() && actor->GetCurrentHitPoints(0)==guardHP
                && app->m_pInternal->m_pPlayerControl->GetPlayerWalking();
            fprintf(stderr,"K1_ENGAGE_ARENA_GUARD_%s\n",pass?"PASS":"FAIL");phase=-1;
        }
    }
}
// Opt-in installed-data fixture. It changes only this unsaved process, never a save file.
void IntegrationTick() {
    static const bool requested=[] {const char* p=std::getenv("K1_ENGAGE_INTEGRATION");return p && !strcmp(p,"1");}();
    if(!requested) return;
    static int phase=0,wait=0;static unsigned long long npcId=OBJECT_INVALID;
    static int hpBefore=0;static bool ok=true;
    if(phase<0 || ++wait<30) return;
    CSWCCreature* client=Actor();CSWSCreature* actor=ServerActor();
    CGuiInGame* gui=g_pAppManager->m_pClientExoApp->GetInGameGui();
    if(!actor || !client || !actor->GetArea() || !gui || !gui->m_pMainInterface || gui->m_nGlobalDialogState) return;
    auto check=[&](bool pass,const char* name) {ok=ok&&pass;fprintf(stderr,"K1_ENGAGE_CHECK_%s %s\n",pass?"PASS":"FAIL",name);};
    CSWSCreature* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(npcId);
    if(phase==0) {
        check(EquipFixtureSaber(actor),"owned_saber_equipped");
        npc=new CSWSCreature(OBJECT_INVALID,0);
        if(!npc->LoadFromTemplate(CResRef("n_sithsoldier001"),0)) {
            delete npc;check(false,"fixture_template_available");phase=-1;return;
        }
        for(int i=0;i<14;++i) npc->m_scripts[i]="";
        npc->m_bPlot=0;npc->SetCurrentHitPoints(1000);check(EquipFixtureSaber(npc),"npc_saber_equipped");
        Vector p=actor->m_position+actor->m_orientation*.9f;
        npc->SetOrientation(actor->m_orientation*-1.f);
        npc->AddToArea(static_cast<CSWSArea*>(actor->GetArea()),p.x,p.y,p.z,1,1);
        g_pAppManager->m_pServerExoApp->GetServerAIMaster()->SetAILevel(npc,-1);
        npcId=npc->m_objectId;actor->SetCurrentHitPoints(1000);
        fprintf(stderr,"K1_ENGAGE_FIXTURE actor=%llu npc=%llu\n",actor->m_objectId,npcId);
    } else if(!npc) {check(false,"fixture_lifetime");phase=-1;return;}
    else if(phase==1) {
        npc->AddToVisibleList(actor->m_objectId,1,0,0,0);
        check(Supported(client),"single_saber_supported");
        g_pAppManager->m_pClientExoApp->SetAutoPaused(1,CClientExoAppInternal::PAUSE_MODE_ENEMY_SIGHTED);
        K1EngageHud(gui->m_pMainInterface);
        check(state.mode==K1EngageState::Mode::Planning,"enemy_detection_planning");
        check(gui->m_pMainInterface->m_btnEngage.m_tail.bVisible,"engage_visible");
    } else if(phase==2) {
        check(!IsGamePaused() && K1EngagePlanning(),"enemy_planning_keeps_world_running");
        K1EngageHud(gui->m_pMainInterface);
        check(!gui->m_pMainInterface->m_btnClearAll.m_tail.bVisible,"disengage_hidden_during_hostile_sight");
        for(int attempt=0;attempt<3;++attempt) {
            gui->m_pMainInterface->m_btnClearAll.m_tail.nCurrentEventArg=1;
            gui->m_pMainInterface->OnClearAllButtonPressed(&gui->m_pMainInterface->m_btnClearAll);
            g_pAppManager->m_pClientExoApp->m_pInternal->HandleInputEvent(INPUTEVENT_CANCEL_COMBAT,1);
        }
        check(K1EngagePlanning() && !IsGamePaused() && gui->m_pMainInterface->m_btnEngage.m_tail.bVisible,
            "repeated_hud_and_global_disengage_cannot_escape_planning");
        g_pAppManager->m_pClientExoApp->m_pInternal->HandleInputEvent(INPUTEVENT_PAUSE,1);
        check(K1EngageActive() && g_bMouseCameraRotateToggle,"space_engage_callback_camera");
    } else if(phase==3) {
        fprintf(stderr,"K1_ENGAGE_FIXTURE_ROUND started=%d spell=%d\n",actor->m_pCombatRound->m_bRoundStarted,actor->m_pCombatRound->m_bSpellCastRound);
        K1EngageMouse(false,true);check(K1EngageForceWalk(),"guard_forces_walk");
        check(K1EngageBlocksMelee(npcId,actor->m_objectId),"front_guard");
        npc->AddToVisibleList(actor->m_objectId,1,0,0,0);
        check(!K1EngageTryDisengage() && K1EngageActive(),"disengage_denied_seen_by_npc");
        hpBefore=actor->GetCurrentHitPoints(0);
        CSWSCombatRound* r=npc->m_pCombatRound;r->ClearAllAttacks();r->m_currentAttack=0;r->m_nOnHandAttacks=1;r->SetOffHandAttack(0);
        npc->ResolveMeleeAttack(actor,0,0,0,0,0,0);npc->SignalMeleeDamage(actor->m_objectId,1);
    } else if(phase==4) {
        check(actor->GetCurrentHitPoints(0)==hpBefore,"frontal_guard_zero_damage");
        K1EngageMouse(false,false);check(!K1EngageForceWalk(),"guard_release_run_eligible");
        hpBefore=actor->GetCurrentHitPoints(0);
        CSWSCombatRound* r=npc->m_pCombatRound;r->ClearAllAttacks();r->m_currentAttack=0;r->m_nOnHandAttacks=1;r->SetOffHandAttack(0);
        npc->ResolveMeleeAttack(actor,0,0,0,0,0,0);npc->SignalMeleeDamage(actor->m_objectId,1);
    } else if(phase==5) {
        check(actor->GetCurrentHitPoints(0)<hpBefore,"unguarded_melee_weapon_damage");
        hpBefore=npc->GetCurrentHitPoints(0);
        check(ContactDamage(actor,npc),"player_contact_damage_service");
    } else if(phase==6) {
        check(npc->GetCurrentHitPoints(0)<hpBefore,"player_weapon_damage_applied");
        K1EngageMouse(true,true);
    } else if(phase==7) {
        // Repeat operation after a full swing; direct click input is consumed by Engage.
        check(K1EngageMouse(false,true),"right_mouse_consumed");
        g_pAppManager->m_pClientExoApp->m_pInternal->HandleInputEvent(INPUTEVENT_GUI,1);
        check(gui->m_swGuiToggleState==CGuiInGame::SWGUI_STATE_COVERED,"actual_options_menu_opened");
    } else if(phase==8) {
        check(K1EngagePlanning() && !state.rightHeld,"menu_suspends_engage");
        g_pAppManager->m_pClientExoApp->m_pInternal->HandleInputEvent(INPUTEVENT_GUI,1);
    } else if(phase==9) {
        check(!IsGamePaused() && K1EngagePlanning() && gui->m_swGuiToggleState!=CGuiInGame::SWGUI_STATE_COVERED,
            "return_from_menu_keeps_world_paused");
        g_pAppManager->m_pClientExoApp->m_pInternal->m_pPlayerControl->SetEnabled(0);
        check(K1EngageEnter() && g_pAppManager->m_pClientExoApp->m_pInternal->m_pPlayerControl->m_bEnabled,"reenter_restores_player_control");
    } else if(phase==10) {
        CSWBehaviorCameraCombat* camera=static_cast<CSWBehaviorCameraCombat*>(g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->GetBehavior(BEHAVIOR_CAMERA_COMBAT));
        check(camera!=nullptr,"native_combat_camera_installed");
        K1EngageLook(80,0);if(camera) camera->Control(1.f/60);
        Vector forward=g_pAppManager->m_pClientExoApp->GetModule()->m_pCamera->GetOrientation().zdir()*-1.f;
        check(forward.x*std::cos(aimYaw)+forward.y*std::sin(aimYaw)>.9f,"combat_camera_tracks_mouse_after_menu");
        Gob* body=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        body->PlayAnimation(const_cast<char*>("walk"),1,0,0);
        body->PlayAnimation(const_cast<char*>("g2r1"),1,AnimRun::kAnimOverlay,0);
        body->PlayAnimation(nullptr,1,0,0);
        actor->m_pCombatRound->m_bSpellCastRound=1;
        body->PlayAnimation(const_cast<char*>("walk"),1,0,0);
        actor->m_pCombatRound->m_bSpellCastRound=0;
        body->Animate(.000001f);
        bool exclusive=body->m_paAnimations.m_count>0;
        for(int i=0;i<body->m_paAnimations.m_count;++i) {
            const char* name=body->m_paAnimations.m_pData[i]->m_pAnimation->m_szName;
            exclusive=exclusive && (!strncmp(name,"j_",2)||!strncmp(name,"y_",2)||!strncmp(name,"s_",2));
        }
        check(exclusive,"native_walk_and_ready_do_not_contribute_to_body_pose");
        npc->RemoveFromVisibleList(actor->m_objectId);
        npc->AddAttackActions(actor->m_objectId,0,1,0,0,0,0,1,SERVER_ANIMATION_ATTACK,1500,1,0,0,4,0);
        check(!K1EngageTryDisengage(),"unseen_chasing_enemy_still_prevents_disengage");
    } else if(phase==11) {
        npc->ClearAllActions(1);
        // ClearAllActions cancels pursuit but does not finish an already executing
        // combat round. The fixture must end that attack before claiming combat ceased.
        npc->m_pCombatRound->EndCombatRound(1);
        npc->m_pCombatRound->RemoveAllActions();
        npc->SetCombatState(0,0);
        npc->RemoveFromVisibleList(actor->m_objectId);
        check(K1EngageTryDisengage(),"disengage_after_sight_and_pursuit_end");
        npc->AddToVisibleList(actor->m_objectId,1,0,0,0);
        check(K1EngageEnter(),"second_combat_reentry");
    } else if(phase==12) {
        npc->SetCurrentHitPoints(0);
    } else if(phase==13) {
        check(npc->GetDead() && state.mode==K1EngageState::Mode::Exploration
            && !g_pAppManager->m_pClientExoApp->GetCombatMode(),"last_enemy_death_restores_exploration");
        g_pAppManager->m_pClientExoApp->SetPausedByCombat(1,0,0);
        fprintf(stderr,"K1_ENGAGE_INTEGRATION_%s\n",ok?"PASS":"FAIL");phase=-1;return;
    }
    ++phase;wait=0;
}
}
#include "k1dismember.inc"
#include "include/k1contacttiming.inc"
void StaffAnimationProbe(CSWGuiScene& scene) {
    const int index=scene.m_aModels.m_count;
    auto* body=static_cast<Gob*>(scene.AddModel(CExoString("k1jastaff"),-1));
    bool ok=body!=nullptr && ContactTimingProbe();
    int poses=0,records=0;float positionError=0,angleError=0;
    if(!body) {fprintf(stderr,"K1_STAFF_COMPONENT_FAIL missing_model\n");return;}
    body->AttachToScene(nullptr);body->m_bAlwaysRendering=true;body->EnableForceAnimations();
    FILE* bind=fopen("staff-native-bind.txt","w");
    std::vector<Part*> parts;
    auto collect=[&](auto&& self,Part* p)->void {parts.push_back(p);for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);};
    collect(collect,body->m_pPart);
    int palettes=0;
    for(Part* p:parts) if(p->m_pNode->m_nFlags==NODE_SKIN) {
        auto* skin=static_cast<MdlNodeSkin*>(p->m_pNode);int used=0;
        for(int i=0;i<skin->m_nBoneCount && i<int(parts.size());++i) if(skin->m_paBoneMap[i]>=0) {
            ++used;Vector v=skin->m_lTBoneRefInv.m_pData[i];Quaternion q=skin->m_lQBoneRefInv.m_pData[i];
            if(bind) fprintf(bind,"%s %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",p->m_pNode->TranslateNodeName(),parts[i]->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);
        }
        ok=ok && used<=16;++palettes;
    }
    if(bind) fclose(bind);else ok=false;
    ok=ok && palettes>0 && parts.size()==70;
    FILE* file=fopen("staff-expected.txt","r");ok=ok && file;
    char line[512],clip[32],bone[64],previous[32]="";float t=0,last=-1,x,y,z,qx,qy,qz,qw;
    while(file && fgets(line,sizeof(line),file)) {
        if(sscanf(line,"%31s %f %63s %f %f %f %f %f %f %f",clip,&t,bone,&x,&y,&z,&qx,&qy,&qz,&qw)!=10) {ok=false;break;}
        if(strcmp(clip,previous) || last!=t) {
            body->PlayAnimation(nullptr,1,0,0);body->Reset();
            ok=(body->PlayAnimation(clip,0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,t)!=0) && ok;
            body->m_nLastAnimateTick=-1;body->Animate(.000001f);strcpy(previous,clip);last=t;++poses;
        }
        Vector p;Quaternion q;
        if(!body->GetPartLocalPosition(bone,&p,&q)) {ok=false;break;}
        positionError=std::max(positionError,magnitude(p-Vector(x,y,z)));
        angleError=std::max(angleError,2*std::acos(std::min(1.f,std::fabs(dot(normalize(q),normalize(Quaternion(qw,qx,qy,qz)))))));
        ++records;
    }
    if(file) fclose(file);
    ok=ok && poses>1500 && records>70000 && positionError<.00001f && angleError<.002f;
    fprintf(stderr,"K1_STAFF_SOURCE_%s poses=%d records=%d position=%g angle=%g\n",ok?"PASS":"FAIL",poses,records,positionError,angleError);
    // Independent original-JKA hilt-local offsets, before body compaction.
    // Source playback matching alone cannot detect a consistently wrong grip.
    FILE* authored=fopen("staff-grip-expected.txt","r");
    bool gripOk=authored!=nullptr;int held=0,released=0,isHeld=0;float authoredError=0;
    while(authored && fscanf(authored,"%31s %f %d %f %f %f",clip,&t,&isHeld,&x,&y,&z)==6) {
        body->PlayAnimation(nullptr,1,0,0);body->Reset();
        gripOk=(body->PlayAnimation(clip,0,AnimRun::kAnimNoBlendIn,t)!=0) && gripOk;
        body->m_nLastAnimateTick=-1;body->Animate(.000001f);
        if(isHeld) {
            const auto actual=ReadGrip(body);
            const Vector localGap=Rotate(Inverse(actual.hiltQ),actual.left-actual.hilt);
            const float error=magnitude(localGap-Vector(x,y,z));
            if(error>authoredError && error>=.0002f) fprintf(stderr,"K1_STAFF_AUTHORED_GRIP_SAMPLE clip=%s time=%.9g error=%g actual=%g,%g,%g expected=%g,%g,%g\n",clip,t,error,localGap.x,localGap.y,localGap.z,x,y,z);
            authoredError=std::max(authoredError,error);
            ++held;
        } else ++released;
    }
    if(authored) fclose(authored);
    gripOk=gripOk && held==455 && released==314 && authoredError<.0002f;
    fprintf(stderr,"K1_STAFF_AUTHORED_GRIP_%s held=%d released=%d hilt_local_error=%g\n",gripOk?"PASS":"FAIL",held,released,authoredError);
    ok=ok && gripOk;
    // Compare to the completed stance's live resource, never to stale exported
    // locomotion notes. Its asset and stance-specific code remain untouched.
    auto* original=static_cast<Gob*>(scene.AddModel(CExoString("k1jarig"),-1));
    bool locomotion=original!=nullptr;int locomotionFrames=0,locomotionRecords=0;
    float locomotionPosition=0,locomotionAngle=0;
    if(original) {
        original->AttachToScene(nullptr);original->m_bAlwaysRendering=true;original->EnableForceAnimations();
        for(const char* name:{"j_walk","j_backwalk","j_run","j_backrun","j_leftwalk","j_rightwalk","j_leftrun","j_rightrun"}) {
            original->PlayAnimation(nullptr,1,0,0);
            locomotion=(original->PlayAnimation(const_cast<char*>(name),0,AnimRun::kAnimNoBlendIn,0)!=0) && locomotion;
            float ignored=0,clipLength=0;
            locomotion=(original->GetAnimation(const_cast<char*>(name),&ignored,&clipLength)!=0) && locomotion;
            for(int frame=0;frame<=20;++frame) {
                const float at=clipLength*float(frame)/20.f;
                for(Gob* model:{body,original}) {
                    model->PlayAnimation(nullptr,1,0,0);model->Reset();
                    locomotion=(model->PlayAnimation(const_cast<char*>(name),0,AnimRun::kAnimNoBlendIn,at)!=0) && locomotion;
                    model->m_nLastAnimateTick=-1;model->Animate(.000001f);
                }
                for(Part* part:parts) {
                    Part* reference=original->GetPart(part->m_pNode->TranslateNodeName());
                    if(!reference) {locomotion=false;continue;}
                    if(magnitude(part->m_vLocalPosition-reference->m_vLocalPosition)>.01f && frame==0)
                        fprintf(stderr,"K1_STAFF_LOCOMOTION_DIFF clip=%s bone=%s actual=%g,%g,%g original=%g,%g,%g\n",name,part->m_pNode->TranslateNodeName(),part->m_vLocalPosition.x,part->m_vLocalPosition.y,part->m_vLocalPosition.z,reference->m_vLocalPosition.x,reference->m_vLocalPosition.y,reference->m_vLocalPosition.z);
                    locomotionPosition=std::max(locomotionPosition,magnitude(part->m_vLocalPosition-reference->m_vLocalPosition));
                    locomotionAngle=std::max(locomotionAngle,2*std::acos(std::min(1.f,std::fabs(dot(normalize(part->m_qLocalOrientation),normalize(reference->m_qLocalOrientation))))));
                    ++locomotionRecords;
                }
                ++locomotionFrames;
            }
        }
        scene.RemoveModel(index+1,1);
    }
    locomotion=locomotion && locomotionFrames==168 && locomotionRecords==11760 && locomotionPosition<.00001f && locomotionAngle<.002f;
    fprintf(stderr,"K1_STAFF_LOCOMOTION_%s frames=%d records=%d position=%g angle=%g\n",locomotion?"PASS":"FAIL",locomotionFrames,locomotionRecords,locomotionPosition,locomotionAngle);
    ok=ok && locomotion;
    bool timing=true;
    for(int direction=0;direction<7;++direction) {
        body->PlayAnimation(nullptr,1,0,0);float at=0,length=0;
        timing=(body->PlayAnimation(const_cast<char*>(fullAttacks[direction]),0,AnimRun::kAnimNoBlendIn,0)!=0) && timing;
        timing=(body->GetAnimation(const_cast<char*>(fullAttacks[direction]),&at,&length)!=0) && timing;
        timing=timing && std::fabs(length-(direction==0?.95f:.9f))<.00001f &&
            staffHitWindows[direction][0]>0 && staffHitWindows[direction][0]<staffHitWindows[direction][1] && staffHitWindows[direction][1]<length;
    }
    fprintf(stderr,"K1_STAFF_TIMING_%s directions=7\n",timing?"PASS":"FAIL");ok=ok && timing;
    // Native staff beam geometry for offline self-clearance checks. This does
    // not change the contact/damage service or attach another combat weapon.
    {
        const int weaponIndex=scene.m_aModels.m_count;
        auto* weapon=static_cast<Gob*>(scene.AddModel(CExoString("w_dblsbr_001"),-1));
        int beams=0;
        std::vector<std::pair<Vector,Vector>> unique;
        FILE* geometry=fopen("staff-weapon-geometry.txt","w");
        if(weapon) {
            weapon->AttachToScene(nullptr);weapon->m_vPosition=Vector();weapon->m_qOrientation=Quaternion();
            weapon->m_bAlwaysRendering=true;weapon->EnableForceAnimations();
            weapon->PlayAnimation(const_cast<char*>("powered"),0,AnimRun::kAnimNoBlendIn,0.f);
            weapon->Animate(.000001f);
            auto visit=[&](auto&& self,Part* p)->void {
                if(auto* node=p->m_pNode->AsMdlNodeLightsaber()) if(node->m_paPositions) {
                    auto a=node->m_paPositions[1],b=node->m_paPositions[2];
                    Vector base=PartPointWorld(p,Vector(a.x,a.y,a.z)),tip=PartPointWorld(p,Vector(b.x,b.y,b.z));
                    if(magnitude(tip-base)<.1f) return;
                    for(const auto& segment:unique) if(magnitude(base-segment.first)<.00001f && magnitude(tip-segment.second)<.00001f) return;
                    unique.push_back({base,tip});
                    if(geometry) fprintf(geometry,"%s %.9g %.9g %.9g %.9g %.9g %.9g\n",p->m_pNode->TranslateNodeName(),base.x,base.y,base.z,tip.x,tip.y,tip.z);
                    ++beams;
                }
                for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
            };visit(visit,weapon->m_pPart);
            scene.RemoveModel(weaponIndex,1);
        }
        if(geometry) fclose(geometry);
        fprintf(stderr,"K1_STAFF_WEAPON_GEOMETRY_%s beams=%d\n",beams==2?"PASS":"FAIL",beams);
    }
    CacheNativeIdle(body);CacheNativeIdle(body,"pmbbl",&nativeStaffGuardPose,"g3r1");
    ok=ok && nativeIdlePose.size()==70 && nativeStaffGuardPose.size()==70;
    lastPose.clear();blendFrom.clear();rotationFrom.clear();blendPending=false;lastGrip=GripPose();
    const char* prior=nullptr;const char* priorOverlay=nullptr;bool priorReady=false;float maxStep=0,maxGrip=0,maxEdge=0,maxLowerPosition=0,maxLowerAngle=0;int frames=0,readyFrames=0;float maxSourceContact=0;
    FILE* dump=fopen("staff-connected-poses.txt","w");
    Vector nativeGap;
    body->PlayAnimation(nullptr,1,0,0);body->Reset();ApplyStaffGuard(body,false);
    auto grip=ReadGrip(body);nativeGap=Rotate(Inverse(grip.hiltQ),grip.left-grip.hilt);
    // Both signs of every residual leg direction, locomotion, attack/recovery,
    // guard release and repeated ownership of the native stationary idle.
    for(int round=0;round<3;++round) for(int action=0;action<54;++action) {
        const char* base=action==0?"j_idle":action==1?"j_guard":action==2?"j_walk":action==3?"j_backwalk":
            action==4?"j_run":action==5?"j_backrun":action==6?"s_top":action==8?"j_walk":action==9?"j_backwalk":action==10?"j_run":
            (action==12 || action==14 || action==15 || action==16)?"j_walk":action==13?"j_backwalk":"j_idle";
        const bool newAttack=action>=18 && action<39,blocking=action>=39;
        const int direction=newAttack?(action-18)/3:0;
        const int block=blocking?(action-39)/3:0;
        const int gait=newAttack?(action-18)%3:blocking?(action-39)%3:0;
        if(newAttack) base=gait==0?fullAttacks[direction]:gait==1?"j_walk":"j_run";
        if(blocking) base=gait==0?"j_idle":gait==1?"j_walk":"j_backwalk";
        const char* overlayName=blocking?blockUpper[block]:newAttack?(gait?attacks[direction]:nullptr):
            (action>=8 && action<=10)?"y_top":nullptr;
        const char* reference=blocking?blockFull[block]:newAttack?fullAttacks[direction]:"s_top";
        const bool overlay=overlayName!=nullptr,attacking=action==6 || (action>=8 && action<=10) || newAttack;
        const bool idle=action==0 || action==7 || action==11 || action==17;
        const bool ready=(action>=1 && action<=3) || action==14 || action==16;
        const bool walking=action==2 || action==3 || (action>=12 && action<=16) || ((newAttack || blocking) && gait==1);
        const float yaw=round==0?-.785398f:round==1?.785398f:0;
        float ignored=0,baseLength=0;
        body->PlayAnimation(nullptr,1,0,0);
        ok=(body->PlayAnimation(const_cast<char*>(base),0,AnimRun::kAnimNoBlendIn,0)!=0) && ok;
        ok=(body->GetAnimation(const_cast<char*>(base),&ignored,&baseLength)!=0) && ok;
        float overlayLength=0;
        if(overlayName) {
            ok=(body->PlayAnimation(const_cast<char*>(overlayName),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,0)!=0) && ok;
            ok=(body->GetAnimation(const_cast<char*>(overlayName),&ignored,&overlayLength)!=0) && ok;
        }
        float length=blocking?.25f:attacking?(overlayName?overlayLength:baseLength):(walking?baseLength:.6f);
        const int count=int(length*60)+14;
        if(prior!=base || priorOverlay!=overlayName || priorReady!=ready) {blendPending=true;prior=base;priorOverlay=overlayName;priorReady=ready;}
        for(int frame=0;frame<count;++frame) {
            const auto before=lastPose;
            body->PlayAnimation(nullptr,1,0,0);body->Reset();
            float at=std::min(length,float(frame)/60.f);
            float baseAt=at;
            if((!attacking || overlay) && baseLength>0) baseAt=std::fmod(float(frame)/60.f,baseLength);
            if(blocking) at=std::min(at,std::max(0.f,overlayLength-.0001f));
            ok=(body->PlayAnimation(const_cast<char*>(base),0,AnimRun::kAnimNoBlendIn,baseAt)!=0) && ok;
            if(overlay) ok=(body->PlayAnimation(const_cast<char*>(overlayName),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,at)!=0) && ok;
            body->m_nLastAnimateTick=-1;body->Animate(.000001f);
            if(idle) for(const auto& p:nativeIdlePose) {p.part->m_vLocalPosition=p.position;p.part->m_qLocalOrientation=p.orientation;}
            else {
                // BridgeTorso belongs to the old sibling-pelvis add-in rig.
                // This source body has a connected spine and no add-in model.
                if(overlay && !IsSourceBody(body)) ok=BridgeTorso(body,reference,at) && ok;
                TurnSpine(body,yaw,overlay?reference:nullptr,at);
                if(ready) {
                    std::vector<BodyPose> lower;
                    for(Part* part:parts) {
                        bool upper=false;
                        for(Part* p=part;p;p=p->m_pParent) if(!strcmp(p->m_pNode->TranslateNodeName(),"torso_g")) {upper=true;break;}
                        if(!upper) lower.push_back({part,part->m_vLocalPosition,part->m_qLocalOrientation});
                    }
                    ok=ApplyStaffGuard(body,walking) && ok;
                    if(walking) for(const auto& p:lower) {
                        maxLowerPosition=std::max(maxLowerPosition,magnitude(p.part->m_vLocalPosition-p.position));
                        maxLowerAngle=std::max(maxLowerAngle,2*std::acos(std::min(1.f,std::fabs(dot(normalize(p.part->m_qLocalOrientation),normalize(p.orientation))))));
                    }
                }
                if(!overlay && (action==4 || action==5)) RefineRunPose(body,action==5,1.f/60);
                if(!overlay && !ready && (strstr(base,"walk") || strstr(base,"run"))) ClearStaffStrafe(body,yaw);
            }
            if(std::getenv("K1_ENGAGE_RIG_DIAG") &&
               ((action==21 && frame>=26 && frame<=32) || (action==24 && frame>=2 && frame<=11) ||
                (action==39 && frame<=10))) {
                rigDiagnosticState=1000+action;rigDiagnosticFrame=frame;
            }
            SmoothBody(body,1.f/60,ready || attacking || blocking,idle);
            rigDiagnosticState=rigDiagnosticFrame=-1;
            maxSourceContact=std::max(maxSourceContact,sourceContactError);
            if(ready && frame>=14) {
                auto p=ReadGrip(body);maxGrip=std::max(maxGrip,magnitude(Rotate(Inverse(p.hiltQ),p.left-p.hilt)-nativeGap));++readyFrames;
            }
            for(size_t j=0;j<lastPose.size();++j) {
                const auto& p=lastPose[j];const auto& q=p.orientation;
                ok=ok && std::isfinite(dot(q,q)) && std::fabs(dot(q,q)-1)<.002f;
                const char* name=p.part->m_pNode->TranslateNodeName();
                if(!strcmp(name,"torso_g") || !strcmp(name,"torsoMid_g") || !strcmp(name,"torsoUpr_g"))
                    maxEdge=std::max(maxEdge,magnitude(p.position-p.part->m_pNode->m_vPosition));
                if(j<before.size() && before[j].part==p.part)
                    {
                    float step=2*std::acos(std::min(1.f,std::fabs(dot(normalize(before[j].orientation),normalize(q)))));
                    if(step>maxStep && step>=.7f) fprintf(stderr,"K1_STAFF_STEP_FAIL action=%d frame=%d base=%s overlay=%s bone=%s radians=%g\n",action,frame,base,overlayName?overlayName:"none",name,step);
                    maxStep=std::max(maxStep,step);
                }
                if(dump) fprintf(dump,"%d %d %s %s %g %g %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",round*54+action,frame,base,overlayName?overlayName:"none",at,yaw,name,p.position.x,p.position.y,p.position.z,q.x,q.y,q.z,q.w);
            }
            if(idle && frame>=14) for(const auto& p:nativeIdlePose)
                ok=ok && magnitude(p.part->m_vLocalPosition-p.position)<.00001f &&
                    2*std::acos(std::min(1.f,std::fabs(dot(normalize(p.part->m_qLocalOrientation),normalize(p.orientation)))))<.002f;
            ++frames;
        }
    }
    if(dump) fclose(dump);
    ok=ok && maxSourceContact<.0002f && maxStep<.7f && maxGrip<.003f && maxEdge<.00001f && maxLowerPosition<.000001f && maxLowerAngle<.002f;
    fprintf(stderr,"K1_STAFF_SOURCE_CONTACT_%s error=%g\n",maxSourceContact<.0002f?"PASS":"FAIL",maxSourceContact);
    fprintf(stderr,"K1_STAFF_GUARD_LOWER_%s position=%g angle=%g\n",maxLowerPosition<.000001f && maxLowerAngle<.002f?"PASS":"FAIL",maxLowerPosition,maxLowerAngle);
    fprintf(stderr,"K1_STAFF_CONNECTED_%s frames=%d ready=%d grip=%g spine_position=%g step=%g\n",ok?"PASS":"FAIL",frames,readyFrames,maxGrip,maxEdge,maxStep);
    lastPose.clear();blendFrom.clear();rotationFrom.clear();nativeIdlePose.clear();nativeStaffGuardPose.clear();nativeDualGuardPose.clear();blendPending=false;
    scene.RemoveModel(index,1);
    fprintf(stderr,"K1_STAFF_COMPONENT_%s\n",ok?"PASS":"FAIL");
}
void K1EngageModelProbe(CSWGuiScene& scene) {
    const char* requested=std::getenv("K1_ENGAGE_PROBE");
    if (!K1EngageEnabled() || !requested || strcmp(requested,"1")) return;
    if(StaffPrototype()) {StaffAnimationProbe(scene);return;}
    DismemberModelProbe(scene);
    if(std::getenv("K1_ENGAGE_RIG_CANDIDATE")) {RigCandidateProbe(scene);return;}
    FILE* file=fopen("yellow-expected.txt","r");
    int index=scene.m_aModels.m_count;
    Gob* model=file ? static_cast<Gob*>(scene.AddModel(CExoString("pmbbl"),-1)) : nullptr;
    bool ok=model && model->LoadAddInAnimations(const_cast<char*>("k1yellow"));
    if (ok) {
        model->AttachToScene(nullptr);model->m_bAlwaysRendering=true;model->EnableForceAnimations();
        model->m_pAddInModel->SynchronizeTree(model->m_pModel);
        for(int i=0;i<model->m_pAddInModel->m_animations.m_count;++i)
            model->m_pAddInModel->m_animations.m_pData[i]->SynchronizeTree(model->m_pModel);
    }
    if(ok) {
        FILE* bind=fopen("yellow-bind.txt","w");
        std::vector<Part*> parts;
        auto collect=[&](auto&& self,Part* p)->void {parts.push_back(p);for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);};
        collect(collect,model->m_pPart);
        if(bind) {
            for(Part* p:parts) if(p->m_pNode->m_nFlags==NODE_SKIN) {
                auto* skin=static_cast<MdlNodeSkin*>(p->m_pNode);
                for(int i=0;i<skin->m_nBoneCount && i<int(parts.size());++i) if(skin->m_paBoneMap[i]>=0) {
                    Vector v=skin->m_lTBoneRefInv.m_pData[i];Quaternion q=skin->m_lQBoneRefInv.m_pData[i];
                    fprintf(bind,"%s %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",p->m_pNode->TranslateNodeName(),parts[i]->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);
                }
            }
            fclose(bind);
        }
    }
    char line[512],clip[32],bone[64],previous[32]="";
    float t=0,last=-1,x,y,z,qx,qy,qz,qw;int records=0,poses=0;double maxAngle=0;float maxPosition=0;
    while(ok && fgets(line,sizeof(line),file)) {
        if(sscanf(line,"%31s %f %63s %f %f %f %f %f %f %f",clip,&t,bone,&x,&y,&z,&qx,&qy,&qz,&qw)!=10) {ok=false;break;}
        if(strcmp(previous,clip) || t!=last) {
            model->PlayAnimation(nullptr,1,0,0);
            ok=model->PlayAnimation(clip,0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,t);
            model->Animate(.000001f);strcpy(previous,clip);last=t;++poses;
        }
        Vector p;Quaternion q;
        if(!model->GetPartLocalPosition(bone,&p,&q)) {ok=false;break;}
        float pe=std::max(std::fabs(p.x-x),std::max(std::fabs(p.y-y),std::fabs(p.z-z)));
        double dot=double(q.x)*qx+double(q.y)*qy+double(q.z)*qz+double(q.w)*qw;
        double len=std::sqrt((double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w)*
                            (double(qx)*qx+double(qy)*qy+double(qz)*qz+double(qw)*qw));
        double angle=len>0 ? 2*std::acos(std::min(1.0,std::fabs(dot)/len))*180/3.141592653589793 : 999;
        maxPosition=std::max(maxPosition,pe);maxAngle=std::max(maxAngle,angle);++records;
        if(!std::isfinite(angle)||pe>.0002f||angle>.1) {
            fprintf(stderr,"K1_ENGAGE_POSE_FAIL clip=%s time=%g bone=%s position=%g angle=%g\n",clip,t,bone,pe,angle);ok=false;
        }
    }
    if(file) fclose(file);
    ok=ok && poses==2289 && records==103131;
    fprintf(stderr,"K1_ENGAGE_MODEL_%s poses=%d records=%d position=%g angle=%g\n",ok?"PASS":"FAIL",poses,records,maxPosition,maxAngle);
    file=ok ? fopen("yellow-layered.txt","r") : nullptr;
    int expectedPoses=0,expectedRecords=0;
    if(!file || !fgets(line,sizeof(line),file) || sscanf(line,"%d %d",&expectedPoses,&expectedRecords)!=2) ok=false;
    char base[32],oldBase[32]="",reference[32];float bt=0,oldBt=-1,oldOt=-1;poses=records=0;
    while(ok && fgets(line,sizeof(line),file)) {
        if(sscanf(line,"%31s %f %31s %f %63s %f %f %f %f %f %f %f",base,&bt,clip,&t,bone,&x,&y,&z,&qx,&qy,&qz,&qw)!=12) {ok=false;break;}
        if(strcmp(oldBase,base) || strcmp(previous,clip) || bt!=oldBt || t!=oldOt) {
            model->PlayAnimation(nullptr,1,0,0);
            ok=model->PlayAnimation(base,0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,bt)
                && model->PlayAnimation(clip,0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,t);
            if(!strcmp(clip,"y_guard")) strcpy(reference,"j_guard");
            else {strcpy(reference,clip);reference[0]='s';}
            poseProbeBody=model;poseProbeReference=reference;poseProbeTime=t;
            model->Animate(.000001f);
            poseProbeBody=nullptr;poseProbeReference=nullptr;
            strcpy(oldBase,base);strcpy(previous,clip);oldBt=bt;oldOt=t;++poses;
        }
        Vector p;Quaternion q;
        if(!model->GetPartLocalPosition(bone,&p,&q)) {ok=false;break;}
        float pe=std::max(std::fabs(p.x-x),std::max(std::fabs(p.y-y),std::fabs(p.z-z)));
        double dot=double(q.x)*qx+double(q.y)*qy+double(q.z)*qz+double(q.w)*qw;
        double len=std::sqrt((double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w)*
                            (double(qx)*qx+double(qy)*qy+double(qz)*qz+double(qw)*qw));
        double angle=len>0 ? 2*std::acos(std::min(1.0,std::fabs(dot)/len))*180/3.141592653589793 : 999;
        maxPosition=std::max(maxPosition,pe);maxAngle=std::max(maxAngle,angle);++records;
        if(!std::isfinite(angle)||pe>.0002f||angle>.1) {
            fprintf(stderr,"K1_ENGAGE_LAYER_FAIL base=%s overlay=%s bone=%s position=%g angle=%g\n",base,clip,bone,pe,angle);ok=false;
        }
    }
    if(file) fclose(file);
    ok=ok && expectedPoses>0 && expectedRecords>0 && poses==expectedPoses && records==expectedRecords;
    fprintf(stderr,"K1_ENGAGE_LAYER_%s poses=%d records=%d position=%g angle=%g\n",ok?"PASS":"FAIL",poses,records,maxPosition,maxAngle);
    if(ok) ConnectedPoseProbe(model);
    // A fresh native body is the oracle: sparse native controllers must give the
    // same pose after an add-in is removed as on a body that never played it.
    Gob* native=static_cast<Gob*>(scene.AddModel(CExoString("pmbbl"),-1));
    bool restored=model && native;float retained=0;int compared=0;
    if(native) {
        native->AttachToScene(nullptr);native->m_bAlwaysRendering=true;native->EnableForceAnimations();
        native->PlayAnimation(const_cast<char*>("g2r1"),0,AnimRun::kAnimNoBlendIn,0);native->Animate(.000001f);
        FILE* grip=fopen("yellow-native-grip.txt","w");
        if(grip) {
            auto dump=[&](auto&& self,Part* p)->void {
                Vector v=p->m_vLocalPosition;Quaternion q=p->m_qLocalOrientation;
                fprintf(grip,"%s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",p->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);
                for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
            };dump(dump,native->m_pPart);fclose(grip);
        }
        FILE* motion=fopen("yellow-native-motion.txt","w");int sample=0;
        if(motion) {
            for(const char* name:{"pause1","walk","run","g2r1","c2a1","c2a2","c2a3","c2a4","c2a5"}) {
                native->PlayAnimation(nullptr,1,0,0);native->Reset();
                if(!native->PlayAnimation(const_cast<char*>(name),0,AnimRun::kAnimNoBlendIn,0)) continue;
                float time,length;native->GetAnimation(const_cast<char*>(name),&time,&length);
                for(float t=0;t<length;t+=1.f/30) {
                    native->SetAnimationTime(const_cast<char*>(name),t);native->Animate(.000001f);
                    auto dump=[&](auto&& self,Part* p)->void {
                        Vector v=p->m_vLocalPosition;Quaternion q=p->m_qLocalOrientation;
                        fprintf(motion,"%d 13 %s none %g 0 %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",sample,name,t,p->m_pNode->TranslateNodeName(),v.x,v.y,v.z,q.x,q.y,q.z,q.w);
                        for(int i=0;i<p->m_nChildren;++i) self(self,p->m_paChildren[i]);
                    };dump(dump,native->m_pPart);++sample;
                }
            }
            fclose(motion);
        }
    }
    for(int cycle=0;restored && cycle<7;++cycle) {
        if(!model->m_pAddInModel) {
            restored=model->LoadAddInAnimations(const_cast<char*>("k1yellow"));
            if(!restored) break;
            model->m_pAddInModel->SynchronizeTree(model->m_pModel);
            for(int i=0;i<model->m_pAddInModel->m_animations.m_count;++i)
                model->m_pAddInModel->m_animations.m_pData[i]->SynchronizeTree(model->m_pModel);
        }
        model->PlayAnimation(nullptr,1,0,0);
        model->PlayAnimation(const_cast<char*>(fullAttacks[cycle]),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,hitWindows[cycle][1]);
        model->Animate(.000001f);
        ReleaseAnimations(model);
        native->PlayAnimation(nullptr,1,0,0);native->Reset();
        for(const char* name:{"pause1","walk"}) {
            native->PlayAnimation(nullptr,1,0,0);
            model->PlayAnimation(nullptr,1,0,0);
            bool played=native->PlayAnimation(const_cast<char*>(name),0,AnimRun::kAnimNoBlendIn,.13f)
                && model->PlayAnimation(const_cast<char*>(name),0,AnimRun::kAnimNoBlendIn,.13f);
            native->Animate(.000001f);model->Animate(.000001f);
            auto compare=[&](auto&& self,Part* part)->void {
                if(part->m_nNodeID>=0) {
                    Part* other=model->GetPart(part->m_pNode->TranslateNodeName());
                    if(other) {
                        Vector d=other->m_vLocalPosition-part->m_vLocalPosition;
                        float error=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);retained=std::max(retained,error);++compared;
                        if(error>.0002f) fprintf(stderr,"K1_ENGAGE_RESTORE_DIFF cycle=%d clip=%s bone=%s position=%g\n",cycle,name,part->m_pNode->TranslateNodeName(),error);
                    }
                }
                for(int i=0;i<part->m_nChildren;++i) self(self,part->m_paChildren[i]);
            };
            compare(compare,native->m_pPart);restored=restored&&played;
        }
    }
    restored=restored&&retained<.0002f&&compared>0;
    fprintf(stderr,"K1_ENGAGE_RESTORE_%s records=%d position=%g\n",restored?"PASS":"FAIL",compared,retained);
    if(native) scene.RemoveModel(index+1,1);
    if(model) scene.RemoveModel(index,1);
}
#endif
