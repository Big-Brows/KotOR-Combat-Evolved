// Shared native running on the existing adapted saber rigs. The j_* base names
// remain movement/layer selectors; only their running pose is replaced here.
// The native sampler is unattached to the scene and owned by this body's playback.
bool YellowNativeGait(const char* base) {
    return base && (!strcmp(base,"j_run") || !strcmp(base,"j_backrun"));
}
float NativeGaitTime(float time,float length) {
    if(!std::isfinite(time) || !std::isfinite(length) || length<=0) return 0;
    time=std::fmod(time,length);
    return time<0?time+length:time;
}
bool YellowRunBasePart(Part* part,Part* pelvis,Part* left,Part* right) {
    // KotOR's run bob lives above pelvis_g, in rootdummy. The whole pelvis
    // ancestor chain belongs to locomotion even beneath an upper attack layer.
    for(Part* p=pelvis;p;p=p->m_pParent) if(p==part) return true;
    for(Part* p=part;p;p=p->m_pParent) if(p==left || p==right) return true;
    return false;
}
unsigned short NativeSaberRunAnimation(bool staff,bool dual) {
    // Match ServerToClientAnimation's powered-saber weapon families.
    return dual?CSWCAnimBase::CLIENT_ANIMATION_RUN_DUAL_SABER:
        staff?CSWCAnimBase::CLIENT_ANIMATION_RUN_STUN_BATON:CSWCAnimBase::CLIENT_ANIMATION_RUN_SINGLE_SABER;
}
bool ApplyYellowNativeGait(Gob* body,const char* base,bool upperOverlay,float delta,
                          const char* resource,PosePlayback& playback,bool staff=false,bool dual=false) {
    // Diagnostic A/B: restore the previous JKA gait/refinement path unchanged.
    if(std::getenv("K1_YELLOW_NATIVE_GAIT_BASELINE")) return false;
    auto& gait=playback.nativeGait;
    if(!YellowNativeGait(base)) {gait.clip.clear();gait.time=0;return false;}
    if(gait.body!=body || gait.resource!=resource) {
        gait={};gait.body=body;gait.resource=resource;
        gait.owner=std::make_shared<CSWCAnimBase>();
        if(!gait.owner->LoadModel(CResRef(resource),CSWCAnimBase::ANIM_MODEL_BODY)) {gait={};return false;}
    }
    Gob* native=gait.owner->GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
    if(!native) return false;
    const CExoString nativeName=gait.owner->GetAnimationName(NativeSaberRunAnimation(staff,dual));
    const char* clip=nativeName.CStr();
    if(!clip || !*clip) return false;
    const bool backward=strstr(base,"back")!=nullptr;
    native->m_bAlwaysRendering=true;native->EnableForceAnimations();
    if(gait.clip!=clip) {
        native->PlayAnimation(nullptr,1,0,0);native->Reset();
        if(!native->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,0)) return false;
        float time=0,length=0;
        if(!native->GetAnimation(const_cast<char*>(clip),&time,&length) || length<=0) return false;
        gait.clip=clip;gait.time=0;gait.length=length;
    }
    // KotOR's WALK_BACKWARD uses its forward clip at a negative rate. Reverse
    // native run the same way, retaining the facing and the existing yaw policy.
    const float step=std::isfinite(delta)?std::max(0.f,delta):0.f;
    gait.time=NativeGaitTime(gait.time+(backward?-step:step),gait.length);
    native->Reset();native->SetAnimationTime(const_cast<char*>(clip),gait.time);
    native->m_nLastAnimateTick=-1;native->Animate(.000001f);
    CaptureNativePose(body,native,gait.pose);
    Part* pelvis=body->GetPart(const_cast<char*>("pelvis_g"));
    Part* left=body->GetPart(const_cast<char*>("lthigh_g"));
    Part* right=body->GetPart(const_cast<char*>("rthigh_g"));
    for(const auto& pose:gait.pose) {
        bool lower=YellowRunBasePart(pose.part,pelvis,left,right);
        if(upperOverlay && !lower) continue;
        pose.part->m_vLocalPosition=pose.position;
        pose.part->m_qLocalOrientation=pose.orientation;
    }
    if(std::getenv("K1_YELLOW_NATIVE_GAIT_TEST")) {
        static unsigned int samples=0;
        if(samples++%120==0) fprintf(stderr,"K1_YELLOW_NATIVE_GAIT_LIVE model=%s resource=%s clip=%s reverse=%d time=%g length=%g upper_overlay=%d\n",
            body->m_pModel->m_szName,resource,clip,backward,gait.time,gait.length,upperOverlay);
    }
    return true;
}
