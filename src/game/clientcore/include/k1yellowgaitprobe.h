bool NativeRunProbeStaff(const char* target) {
    return !_stricmp(target,"k1jastaff") || !_stricmp(target,"k1jadstaff");
}
bool NativeRunProbeDual(const char* target) {
    return !_stricmp(target,"k1jadual") || !_stricmp(target,"k1jaddual");
}
const char* NativeRunProbeClip(const char* target) {
    return NativeRunProbeDual(target)?"runds":NativeRunProbeStaff(target)?"runST":"runSS";
}
bool YellowRunBounceProbe(const char* target,const char* resource) {
    CSWCAnimBase owners[2];PosePlayback playback[2];
    Gob* bodies[2]={};
    for(int i=0;i<2;++i) {
        if(!owners[i].LoadModel(CResRef(target),CSWCAnimBase::ANIM_MODEL_BODY))return false;
        bodies[i]=owners[i].GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
        bodies[i]->m_bAlwaysRendering=true;bodies[i]->EnableForceAnimations();
    }
    FILE* file=fopen("saber-run-bounce.csv","a");
    if(file)fprintf(file,"model,direction,frame,mode,bone,local_z,world_z\n");
    float rootMin[2]={1e9f,1e9f},rootMax[2]={-1e9f,-1e9f},carryError=0;
    Vector plainRoot,plainPelvis;
    for(int direction=0;direction<7;++direction) {
      playback[0]={};playback[1]={};
      for(int frame=0;frame<90;++frame)for(int mode=0;mode<2;++mode) {
        Gob* body=bodies[mode];body->PlayAnimation(nullptr,1,0,0);body->Reset();
        body->PlayAnimation(const_cast<char*>("j_run"),0,AnimRun::kAnimNoBlendIn,float(frame)*.017f);
        if(mode)body->PlayAnimation(const_cast<char*>(attacks[direction]),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,float(frame%30)*.017f);
        body->m_nLastAnimateTick=-1;body->Animate(.000001f);
        if(!ApplyYellowNativeGait(body,"j_run",mode!=0,.017f,resource,playback[mode],NativeRunProbeStaff(target),NativeRunProbeDual(target))) {
            if(file)fclose(file);return false;
        }
        if(_stricmp(playback[mode].nativeGait.clip.c_str(),NativeRunProbeClip(target))) {if(file)fclose(file);return false;}
        TurnSpine(body,0,mode?fullAttacks[direction]:nullptr,float(frame%30)*.017f);
        for(const char* bone:{"rootdummy","pelvis_g","torso_g","torsoMid_g","torsoUpr_g","headhook"})
            if(auto* part=body->GetPart(const_cast<char*>(bone))) {
                Vector p;Quaternion q;part->GetWorld(p,q);
                if(file)fprintf(file,"%s,%d,%d,%d,%s,%.9g,%.9g\n",target,direction,frame,mode,bone,part->m_vLocalPosition.z,p.z);
                if(!strcmp(bone,"rootdummy")) {
                    rootMin[mode]=std::min(rootMin[mode],p.z);rootMax[mode]=std::max(rootMax[mode],p.z);
                    if(!mode)plainRoot=p;else carryError=std::max(carryError,magnitude(p-plainRoot));
                }
                if(!strcmp(bone,"pelvis_g")) {
                    if(!mode)plainPelvis=p;else carryError=std::max(carryError,magnitude(p-plainPelvis));
                }
            }
      }
    }
    if(file)fclose(file);
    bool pass=carryError<.0002f && rootMax[0]-rootMin[0]>.08f &&
        std::fabs((rootMax[0]-rootMin[0])-(rootMax[1]-rootMin[1]))<.0002f;
    fprintf(stderr,"K1_YELLOW_RUN_BOUNCE_%s model=%s clip=%s attacks=7 free_range=%g swing_range=%g carry_error=%g\n",pass?"PASS":"FAIL",target,NativeRunProbeClip(target),rootMax[0]-rootMin[0],rootMax[1]-rootMin[1],carryError);
    return pass;
}
bool YellowNativeGaitComponentProbe(const char* target,const char* resource) {
    bool pass=true;int samples=0;float angleError=0;
    CSWCAnimBase subjectOwner,oracleOwner;
    if(!subjectOwner.LoadModel(CResRef(target),CSWCAnimBase::ANIM_MODEL_BODY) ||
       !oracleOwner.LoadModel(CResRef(resource),CSWCAnimBase::ANIM_MODEL_BODY)) return false;
    Gob* body=subjectOwner.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
    Gob* oracle=oracleOwner.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
    oracle->m_bAlwaysRendering=true;oracle->EnableForceAnimations();
    PosePlayback playback;
    for(const char* base:{"j_run","j_backrun"}) {
        playback.nativeGait={};
        for(int frame=0;frame<80;++frame) {
            body->Reset();
            if(!ApplyYellowNativeGait(body,base,false,.031f,resource,playback,NativeRunProbeStaff(target),NativeRunProbeDual(target))) return false;
            auto& gait=playback.nativeGait;
            pass=!_stricmp(gait.clip.c_str(),NativeRunProbeClip(target)) && pass;
            oracle->PlayAnimation(nullptr,1,0,0);oracle->Reset();
            pass=oracle->PlayAnimation(const_cast<char*>(NativeRunProbeClip(target)),0,AnimRun::kAnimNoBlendIn,gait.time)!=0 && pass;
            oracle->m_nLastAnimateTick=-1;oracle->Animate(.000001f);
            for(const auto& pose:gait.pose) {
                auto* original=oracle->GetPart(pose.part->m_pNode->TranslateNodeName());
                if(!original) continue;
                Vector p;Quaternion nativeBind,targetBind,nativePose,targetPose;
                BindFrame(original,p,nativeBind);BindFrame(pose.part,p,targetBind);
                original->GetWorld(p,nativePose);pose.part->GetWorld(p,targetPose);
                float error=1.f-std::fabs(dot(normalize(targetPose),normalize(nativePose*Inverse(nativeBind)*targetBind)));
                angleError=std::max(angleError,error);pass=std::isfinite(error) && error<.00001f && pass;
                ++samples;
            }
            // Preserve upper attack locals while carrying the native run root
            // and legs beneath them; the root bob is part of locomotion.
            std::vector<BodyPose> before=gait.pose;
            pass=ApplyYellowNativeGait(body,base,true,.013f,resource,playback,NativeRunProbeStaff(target),NativeRunProbeDual(target)) && pass;
            auto* pelvis=body->GetPart(const_cast<char*>("pelvis_g"));
            auto* left=body->GetPart(const_cast<char*>("lthigh_g"));auto* right=body->GetPart(const_cast<char*>("rthigh_g"));
            for(const auto& pose:before) {
                bool lower=YellowRunBasePart(pose.part,pelvis,left,right);
                if(!lower) pass=magnitude(pose.part->m_vLocalPosition-pose.position)<.00001f &&
                    std::fabs(dot(normalize(pose.part->m_qLocalOrientation),normalize(pose.orientation)))>.99999f && pass;
            }
        }
    }
    pass=NativeGaitTime(-.25f,1.f)==.75f && NativeGaitTime(2.25f,1.f)==.25f &&
        NativeGaitTime(0,0)==0 && !YellowNativeGait("j_guard") && !YellowNativeGait("j_walk") &&
        !YellowNativeGait("j_backwalk") && !YellowNativeGait(nullptr) && pass;
    fprintf(stderr,"K1_YELLOW_NATIVE_GAIT_COMPONENT_%s model=%s resource=%s clip=%s comparisons=%d orientation_error=%g reverse_loops=1 upper_preserved=1\n",pass?"PASS":"FAIL",target,resource,NativeRunProbeClip(target),samples,angleError);
    return pass;
}
void YellowNativeGaitProbeTick() {
    static int trial=0,ticks=0,stage=0,stance=0;static Vector origin;
    static bool initialized=false,started=false,sawOverlay=false;
    if(stage<0 || !sandboxReady) return;
    auto send=[](const char* input){if(FILE* f=fopen("yellow-native-input.txt","w")){fputs(input,f);fclose(f);}};
    auto fail=[&](const char* reason){fprintf(stderr,"K1_YELLOW_NATIVE_GAIT_FAIL trial=%d stage=%d reason=%s\n",trial,stage,reason);stage=-1;send("quit:0\n");};
    auto* actor=Actor();auto* server=ServerActor();
    if(!actor || !server) {fail("actor");return;}
    if(!initialized) {
        if(FILE* f=fopen("saber-run-bounce.csv","w"))fclose(f);
        for(const char* model:{"k1jarig","k1jastaff","k1jadual","k1jadark","k1jadstaff","k1jaddual"})
            if(!YellowRunBounceProbe(model,strstr(model,"k1jadark") || strstr(model,"k1jadstaff") || strstr(model,"k1jaddual")?"n_darkjedim":"pmbbl")) {fail("run_bounce");return;}
        if(!YellowNativeGaitComponentProbe("k1jarig","pmbbl") ||
           !YellowNativeGaitComponentProbe("k1jastaff","pmbbl") ||
           !YellowNativeGaitComponentProbe("k1jadual","pmbbl") ||
           !YellowNativeGaitComponentProbe("k1jadark","n_darkjedim") ||
           !YellowNativeGaitComponentProbe("k1jadstaff","n_darkjedim") ||
           !YellowNativeGaitComponentProbe("k1jaddual","n_darkjedim") ||
           !YellowNativeGaitComponentProbe("k1jasith","n_sithsoldier")) {fail("component");return;}
        origin=server->m_position;initialized=true;
    }
    static const int forward[]={1,1,0,-1,-1,-1,0,1};
    static const int right[]={0,1,1,1,0,-1,-1,-1};
    if(stage==0) {
        if(!started) {
            if(!K1EngageEnter()) {fail("enter");return;}
            state.ClearInputs();state.attacking=false;state.chainSwings=0;hadBlade=false;
            movement.Clear();legsYaw=0;
            server->SetPosition(origin,1,1,0);actor->SetPrecalculatedPosition(origin);
            keyW=forward[trial%8]>0;keyS=forward[trial%8]<0;
            keyD=right[trial%8]>0;keyA=right[trial%8]<0;
            keyShift=trial<8 || (trial>=16 && trial<32);
            if(trial>=16 && trial<24)K1EngageMouse(false,true);
            if(trial>=24)K1EngageMouse(true,true);
            ticks=0;started=true;sawOverlay=false;return;
        }
        ++ticks;sawOverlay=sawOverlay || currentOverlay!=nullptr;
        if(ticks<40)return;
        // A held attack can start a chained cut at this deadline. The existing
        // body transition deliberately blends all joints for 0.14 seconds;
        // compare exact native locals once that transition has settled.
        if(playerPlayback.blendTime<.14f) {if(ticks>240)fail("pose_blend_did_not_settle");return;}
        auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        auto& gait=playerPlayback.nativeGait;
        bool walk=trial<8 || (trial>=16 && trial<32);
        if(StaffMode()!=(stance==1) || DualMode()!=(stance==2) || !body) {fail("stance_selection");return;}
        const char* expectedRun=stance==2?"runds":stance==1?"runST":"run";
        if(walk ? !gait.clip.empty() : (gait.body!=body || _stricmp(gait.clip.c_str(),expectedRun) || gait.time<0 || gait.time>=gait.length)) {fail("native_clip_clock");return;}
        float target=forward[trial%8]<0?std::atan2(float(right[trial%8]),float(-forward[trial%8])):
            std::atan2(float(-right[trial%8]),float(forward[trial%8]));
        target=std::max(-.7853981634f,std::min(.7853981634f,target));
        if(std::fabs(legsYaw-target)>.001f){fail("direction_yaw");return;}
        auto* pelvis=body->GetPart(const_cast<char*>("pelvis_g"));
        auto* left=body->GetPart(const_cast<char*>("lthigh_g"));auto* rightLeg=body->GetPart(const_cast<char*>("rthigh_g"));
        CSWCAnimBase walkOracle;
        std::vector<BodyPose> walkPose;
        if(walk) {
            if(!walkOracle.LoadModel(CResRef(body->m_pModel->m_szName),CSWCAnimBase::ANIM_MODEL_BODY)){fail("walk_oracle");return;}
            Gob* model=walkOracle.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);model->m_bAlwaysRendering=true;model->EnableForceAnimations();
            float time=0,length=0;
            if(!body->GetAnimation(const_cast<char*>(currentBase),&time,&length) ||
               !model->PlayAnimation(const_cast<char*>(currentBase),0,AnimRun::kAnimNoBlendIn,time)){fail("walk_clip");return;}
            if(currentOverlay && body->GetAnimation(const_cast<char*>(currentOverlay),&time,&length))
                model->PlayAnimation(const_cast<char*>(currentOverlay),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,time);
            model->m_nLastAnimateTick=-1;model->Animate(.000001f);
            auto capture=[&](auto&& self,Part* part)->void {
                if(auto* source=model->GetPart(part->m_pNode->TranslateNodeName()))walkPose.push_back({part,source->m_vLocalPosition,source->m_qLocalOrientation});
                for(int i=0;i<part->m_nChildren;++i)self(self,part->m_paChildren[i]);
            };capture(capture,body->m_pPart);
        }
        for(const auto& pose:walk?walkPose:gait.pose) {
            bool lower=YellowRunBasePart(pose.part,pelvis,left,rightLeg);
            if(!lower)continue;
            Quaternion expected=pose.part==pelvis?Quaternion(Vector(0,0,1),legsYaw)*pose.orientation:pose.orientation;
            if(magnitude(pose.part->m_vLocalPosition-pose.position)>.0002f ||
               std::fabs(dot(normalize(pose.part->m_qLocalOrientation),normalize(expected)))<.99999f) {
                fprintf(stderr,"K1_YELLOW_NATIVE_GAIT_POSE_DIFF bone=%s position=%g rotation=%g blend=%g\n",pose.part->m_pNode->TranslateNodeName(),
                    magnitude(pose.part->m_vLocalPosition-pose.position),1.f-std::fabs(dot(normalize(pose.part->m_qLocalOrientation),normalize(expected))),playerPlayback.blendTime);
                fail(walk?"original_walk_legs_and_yaw":"posed_native_legs_and_yaw");return;
            }
        }
        if(std::fabs(movement.Speed()-NativeMoveSpeed(server,walk))>.02f){fail("native_movement_speed");return;}
        const Vector facing(std::cos(aimYaw),std::sin(aimYaw),0),side(std::sin(aimYaw),-std::cos(aimYaw),0);
        const Vector desired=facing*float(forward[trial%8])+side*float(right[trial%8]);
        if(dot(server->m_position-origin,desired)<.02f){fail("movement_direction");return;}
        if(trial>=16 && !sawOverlay){fail("upper_overlay_missing");return;}
        fprintf(stderr,"K1_YELLOW_NATIVE_GAIT_DIRECTION_PASS stance=%d trial=%d forward=%d right=%d walk=%d yaw=%g reverse=%d overlay=%d clip=%s\n",
            stance,trial,forward[trial%8],right[trial%8],walk,legsYaw,forward[trial%8]<0,sawOverlay,walk?currentBase:gait.clip.c_str());
        K1EngageMouse(true,false);K1EngageMouse(false,false);
        keyW=keyS=keyA=keyD=keyShift=false;state.ClearInputs();movement.Clear();started=false;
        if(++trial<40)return;
        Plan();stage=stance==0?1:stance==1?3:7;ticks=0;return;
    }
    if(stage==1 || stage==3 || stage==5 || stage==7 || stage==9) {
        if(++ticks<20)return;
        bool offhand=stage==5;
        auto item=stage==1 || stage==7?arenaSaberIds[1]:(offhand?arenaOffhandSaberId:arenaSaberIds[0]);
        if(!SandboxDualEquipMenu(actor,offhand?CSWGuiInGameEquip::GUI_SLOT_WEAP_L:CSWGuiInGameEquip::GUI_SLOT_WEAP_R,item)) {fail("stance_menu");return;}
        ++stage;ticks=0;return;
    }
    if(stage==2 || stage==4 || stage==6 || stage==8 || stage==10) {
        if(++ticks<30)return;
        bool expectedStaff=stage==2 || stage==8,expectedDual=stage==6;
        if(StaffMode()!=expectedStaff || DualMode()!=expectedDual) {fail("stance_replication");return;}
        if(stage==4 || stage==8) {++stage;ticks=0;return;}
        if(!K1EngageEnter()) {fail("stance_enter");return;}
        if(stage==10) {
            stage=20;ticks=0;return;
        }
        stance=expectedStaff?1:2;trial=0;stage=0;started=false;ticks=0;return;
    }
    if(stage==20) {
        fprintf(stderr,"K1_YELLOW_NATIVE_GAIT_PASS directions=120 stances=3 walks_preserved=1 native_run=1 reverse=1 run_attack_bounce=1 heads_unmodified=1\n");
        stage=-1;send("quit:0\n");return;
    }
}
