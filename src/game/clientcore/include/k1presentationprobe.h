bool GuardBlendPoseProbe() {
    bool pass=true;int edges=0,reversals=0,lowerSamples=0;float maxLower=0,maxSettled=0;
    for(const char* resource:{"k1jarig","k1jastaff","k1jadual","k1jadark","k1jadstaff","k1jaddual"}) {
        CSWCAnimBase owners[2];Gob* bodies[2]={};PosePlayback playback[2];
        std::vector<BodyPose> guards[2];
        const bool staff=strstr(resource,"staff")!=nullptr,dual=!strcmp(resource,"k1jadual") || !strcmp(resource,"k1jaddual");
        for(int i=0;i<2;++i) {
            if(!owners[i].LoadModel(CResRef(resource),CSWCAnimBase::ANIM_MODEL_BODY))return false;
            bodies[i]=owners[i].GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
            bodies[i]->m_bAlwaysRendering=true;bodies[i]->EnableForceAnimations();
            if(staff || dual)CacheNativeIdle(bodies[i],strstr(resource,"jadark") || !strcmp(resource,"k1jadstaff") || !strcmp(resource,"k1jaddual")?"n_darkjedim":"pmbbl",&guards[i],staff?"g3r1":"g4r1");
        }
        for(bool moving:{false,true}) {
            playback[0]={};playback[1]={};
            const bool states[]={false,true,false,true,false,true,false};
            for(int segment=0;segment<7;++segment) {
                const bool guard=states[segment];const int frames=segment==3 || segment==4?5:30;
                const char* base=moving?"j_walk":guard?"j_guard":"j_idle";
                for(int i=0;i<2;++i) {
                    bodies[i]->PlayAnimation(nullptr,1,AnimRun::kAnimRemove,0);
                    bodies[i]->PlayAnimation(const_cast<char*>(base),1,AnimRun::kAnimNoBlendIn,0);
                    if(moving && guard)bodies[i]->PlayAnimation(const_cast<char*>("y_guard"),1,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,0);
                    playback[i].blendPending=true;
                }
                for(int frame=0;frame<frames;++frame)for(int i=0;i<2;++i) {
                    auto* body=bodies[i];body->m_nLastAnimateTick=-1;body->Animate(1.f/60);
                    if(moving && guard)BridgeTorso(body,"j_guard",frame/60.f);
                    if(guard && (staff || dual))ApplyStaffGuard(body,moving,guards[i]);
                    if(!i)PrepareGuardBlend(playback[i],guard,true);
                    SmoothBody(body,1.f/60,!dual && guard,false,playback[i]);
                    if(i)for(const char* bone:{"pelvis_g","lthigh_g","lshin_g","lfoot_g","rthigh_g","rshin_g","rfoot_g"}) {
                        auto* a=bodies[0]->GetPart(const_cast<char*>(bone));auto* b=body->GetPart(const_cast<char*>(bone));
                        if(a && b) {
                            const float error=magnitude(a->m_vLocalPosition-b->m_vLocalPosition)+1.f-std::fabs(dot(normalize(a->m_qLocalOrientation),normalize(b->m_qLocalOrientation)));
                            maxLower=std::max(maxLower,error);++lowerSamples;
                        }
                    }
                    if(!i && segment && frame==10)pass=playback[0].guardTransition && playback[0].blendTime>.14f && playback[0].blendTime<guardBlendDuration && pass;
                    if(i && frame==29)for(const char* bone:{"rhand","lhand"}) {
                        auto* a=bodies[0]->GetPart(const_cast<char*>(bone));auto* b=body->GetPart(const_cast<char*>(bone));
                        if(a && b)maxSettled=std::max(maxSettled,1.f-std::fabs(dot(normalize(a->m_qLocalOrientation),normalize(b->m_qLocalOrientation))));
                    }
                }
                if(segment){++edges;if(segment==4 || segment==5)++reversals;}
            }
            // A new attack must use ordinary timing even if guard was lowering.
            PrepareGuardBlend(playback[0],false,false);pass=!playback[0].guardTransition && pass;
        }
    }
    pass=pass && edges==72 && reversals==24 && lowerSamples>10000 && maxLower<.0001f && maxSettled<.002f;
    fprintf(stderr,"K1_GUARD_BLEND_%s models=6 edges=%d reversals=%d lower_samples=%d lower_error=%g settled_error=%g duration=.24 attack_timing=ordinary\n",pass?"PASS":"FAIL",edges,reversals,lowerSamples,maxLower,maxSettled);
    return pass;
}
bool DualPlayerGuardProbe() {
    auto* actor=Actor();if(!actor || !DualMode() || !loadedBody)return false;
    auto* body=loadedBody;auto savedState=state;auto savedPlayback=playerPlayback;
    const char* savedBase=currentBase;const char* savedOverlay=currentOverlay;float savedYaw=legsYaw;
    playerPlayback={};currentBase=currentOverlay=nullptr;state.ClearInputs();
    body->PlayAnimation(nullptr,1,AnimRun::kAnimRemove,0);
    Quaternion previous[2];bool had=false,lastGuard=false;float maxStep=0;int transitions=0;bool pass=true;
    for(int cycle=0;cycle<3;++cycle)for(bool moving:{false,true})for(bool guard:{false,true,false}) {
        bool guardChanged=had && guard!=lastGuard;lastGuard=guard;
        state.rightHeld=guard;state.attacking=false;legsYaw=moving?.45f:0.f;
        const char* base=moving?"j_walk":guard?"j_guard":"j_idle";
        const char* overlay=moving && guard?"y_guard":nullptr;
        if(currentOverlay)body->PlayAnimation(const_cast<char*>(currentOverlay),1,AnimRun::kAnimRemove,0);
        if(currentBase!=base) {
            if(currentBase)body->PlayAnimation(const_cast<char*>(currentBase),1,AnimRun::kAnimRemove,0);
            body->PlayAnimation(const_cast<char*>(base),1,AnimRun::kAnimNoBlendIn,0);
        }
        currentBase=base;currentOverlay=overlay;playerPlayback.blendPending=true;
        if(overlay)body->PlayAnimation(const_cast<char*>(overlay),1,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,0);
        for(int frame=0;frame<30;++frame) {
            body->m_nLastAnimateTick=-1;body->Animate(1.f/60);
            for(int hand=0;hand<2;++hand)if(auto* grip=body->GetPart(const_cast<char*>(hand?"lhand":"rhand"))) {
                Vector p;Quaternion q;grip->GetWorld(p,q);
                if(guardChanged && frame==0) {
                    maxStep=std::max(maxStep,2*std::acos(std::min(1.f,std::fabs(dot(normalize(q),normalize(previous[hand]))))));
                    ++transitions;
                }
                previous[hand]=q;
            }
            had=true;
            if(guard && frame>=20)for(const auto& native:nativeDualGuardPose) {
                const char* name=native.part->m_pNode->TranslateNodeName();
                if(_stricmp(name,"Rforearm_g") && _stricmp(name,"Lforearm_g") && _stricmp(name,"rhand") && _stricmp(name,"lhand"))continue;
                pass=magnitude(native.part->m_vLocalPosition-native.position)<.0002f &&
                    std::fabs(dot(normalize(native.part->m_qLocalOrientation),normalize(native.orientation)))>.99999f && pass;
            }
        }
    }
    state=savedState;playerPlayback=savedPlayback;currentBase=savedBase;currentOverlay=savedOverlay;legsYaw=savedYaw;
    pass=pass && transitions==24 && maxStep<.9f;
    fprintf(stderr,"K1_DUAL_PLAYER_GUARD_%s hand_transitions=%d max_step=%g native_settled=%d\n",pass?"PASS":"FAIL",transitions,maxStep,pass);
    return pass;
}

bool DualNpcTransitionProbe(unsigned long long id) {
    auto found=npcPlayback.find(id);if(found==npcPlayback.end())return false;
    NpcPlayback saved=found->second;CSWCAnimBase owner;
    if(!owner.LoadModel(CResRef("k1jaddual"),CSWCAnimBase::ANIM_MODEL_BODY))return false;
    auto* body=owner.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);body->m_bAlwaysRendering=true;body->EnableForceAnimations();
    body->SetExternalData(saved.owner);body->SetExternalDataType(CGameObject::OBJECT_TYPE_CREATURE);
    auto& npc=found->second;npc.body=body;npc.pose={};npc.utilityBlend={};
    CacheNativeIdle(body,"n_darkjedim",&npc.nativeIdle);CacheNativeIdle(body,"n_darkjedim",&npc.staffGuard,"g4r1");
    Quaternion previous[2];bool had=false;float maxWorld=0,maxLocal=0,maxGait=0;int transitions=0,gaitSamples=0;
    Quaternion oldLocal[2];
    for(int cycle=0;cycle<3;++cycle) {
      had=false;body->m_qOrientation=Quaternion(Vector(0,0,1),float(cycle)*1.3f);npc.legsYaw=float(cycle-1)*.7853981634f;
      for(int segment=0;segment<21;++segment) {
        bool guard=segment==2 || segment==3 || segment==11;
        bool attack=(segment>=4 && segment<=10) || (segment>=13 && segment<=19);
        bool running=segment>=12 && segment<=19;
        npc.attacking=attack;npc.parrying=guard;npc.closeGuard=false;npc.walking=!running;
        npc.direction=attack?(segment>=13?segment-13:segment-4):0;
        npc.base=segment==1?"j_idle":segment==2?"j_guard":running?"j_run":"j_walk";
        npc.overlay=attack?attacks[npc.direction]:guard && segment!=2?"y_guard":nullptr;
        npc.pose.blendPending=true;
        body->PlayAnimation(nullptr,1,AnimRun::kAnimRemove,0);
        body->PlayAnimation(const_cast<char*>(npc.base),1,AnimRun::kAnimNoBlendIn,0);
        if(npc.overlay)body->PlayAnimation(const_cast<char*>(npc.overlay),1,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,0);
        for(int frame=0;frame<90;++frame) {
            body->m_nLastAnimateTick=-1;body->Animate(1.f/60);
            for(int hand=0;hand<2;++hand)if(auto* grip=body->GetPart(const_cast<char*>(hand?"lhand":"rhand"))) {
                Vector p;Quaternion q;grip->GetWorld(p,q);
                if(had && frame==0) {
                    float angle=2*std::acos(std::min(1.f,std::fabs(dot(normalize(q),normalize(previous[hand])))));
                    float local=2*std::acos(std::min(1.f,std::fabs(dot(normalize(grip->m_qLocalOrientation),normalize(oldLocal[hand])))));
                    maxWorld=std::max(maxWorld,angle);maxLocal=std::max(maxLocal,local);++transitions;
                    if(angle>1.f || local>1.f)fprintf(stderr,"K1_DUAL_TRANSITION_JUMP cycle=%d segment=%d hand=%d world=%g local=%g\n",cycle,segment,hand,angle,local);
                }
                if(had && frame>0 && !attack) {
                    float gaitAngle=2*std::acos(std::min(1.f,std::fabs(dot(normalize(q),normalize(previous[hand])))));
                    maxGait=std::max(maxGait,gaitAngle);++gaitSamples;
                }
                previous[hand]=q;oldLocal[hand]=grip->m_qLocalOrientation;
            }
            had=true;
        }
      }
    }
    npc=saved;
    fprintf(stderr,"K1_DUAL_TRANSITION_MEASURE transitions=%d max_world=%g max_local=%g gait_samples=%d max_gait=%g\n",transitions,maxWorld,maxLocal,gaitSamples,maxGait);
    return std::isfinite(maxWorld) && maxWorld<.9f && maxLocal<.7f && maxGait<1.f;
}
void PresentationProbeTick() {
    if(!std::getenv("K1_PRESENTATION_TEST"))return;
    static int stage=0,ticks=0,round=0;static unsigned long long id=OBJECT_INVALID;
    if(stage<0)return;
    auto send=[](const char* s){if(FILE* f=fopen("presentation-input.txt","w")){fputs(s,f);fclose(f);}};
    auto fail=[&](const char* why){fprintf(stderr,"K1_PRESENTATION_FAIL stage=%d reason=%s\n",stage,why);stage=-1;send("quit:0\n");};
    auto* app=g_pAppManager->m_pClientExoApp;auto* internal=app->m_pInternal;
    if(stage==0) {
        if(!sandboxReady || ++ticks<30)return;
        if(!K1EngageEnter()){fail("engage");return;}
        if(!SandboxDualEquipMenu(Actor(),CSWGuiInGameEquip::GUI_SLOT_WEAP_L,arenaOffhandSaberId)){fail("dual_player_equipment");return;}
        CExoString message;auto* npc=SandboxSpawn(2,false,message);if(!npc){fail("spawn");return;}
        id=npc->m_objectId;SandboxSetAi(true);stage=1;ticks=0;return;
    }
    if(stage==1) {
        ++ticks;auto found=npcPlayback.find(id);
        if(found==npcPlayback.end() || !found->second.requested){if(ticks>240)fail("npc_ready");return;}
        if(ticks<50)return;
        if(!DualMode() || !K1EngageEnter() || !loadedBody || _stricmp(loadedBody->m_pModel->m_szName,"k1jadual")) {if(ticks>240)fail("dual_player_ready");return;}
        if(!GuardBlendPoseProbe()){fail("guard_blend");return;}
        if(!DualPlayerGuardProbe()){fail("dual_player_guard");return;}
        if(!DualNpcTransitionProbe(id)){fail("dual_transition");return;}
        fprintf(stderr,"K1_MENU_PHASE quit_requested\n");
        app->SetEndGamePending(1,0,1);stage=2;ticks=0;return;
    }
    if(stage==2) {
        ++ticks;
        auto* menu=internal->m_pMainMenuPanel;
        if(internal->m_pModule || !menu || !internal->m_pSWGuiManager->PanelExists(menu)) {
            if(ticks>240)fail("menu_not_ready");return;
        }
        if(ticks<60)return;
        if(menu->m_nDrawFrames<5 || !menu->m_bInputReady || !npcPlayback.empty() || hudOwner || state.mode!=K1EngageState::Mode::Exploration) {fail("menu_state");return;}
        fprintf(stderr,"K1_MENU_RETURN_PASS round=%d draws=%d ready=%d active_npcs=%zu hud=%d state=%d\n",round,menu->m_nDrawFrames,menu->m_bInputReady,npcPlayback.size(),hudOwner!=nullptr,int(state.mode));
        if(round==0){send("shot:presentation-menu0\n");stage=3;ticks=0;return;}
        fprintf(stderr,"K1_PRESENTATION_PASS menu_returns=2 dual_transitions=120\n");
        send("shot:presentation-menu1\n+5:quit:0\n");stage=-1;return;
    }
    if(stage==3) {
        if(++ticks<5)return;
        CSWGuiSaveLoadEntry entry;entry.LoadData(CExoString("000003 - Game2"));
        if(!entry.m_bSaveExists || entry.m_bCorrupt){fail("reload_fixture");return;}
        // Use the native main-menu load mode, not the in-game confirmation mode.
        auto* panel=new CSWGuiSaveLoad(internal->m_pSWGuiManager,0,1);
        internal->m_pSWGuiManager->AddPanel(panel,2,1);
        internal->m_pMainMenuPanel->SetToBeDeleted(0);panel->LoadGame(&entry);
        stage=4;ticks=0;return;
    }
    if(stage==4) {
        if(!internal->m_pModule || !internal->m_pModule->m_pArea || internal->m_bAreaNotReadyToDisplay) {if(++ticks>600)fail("reload_ready");return;}
        if(++ticks<60)return;
        round=1;app->SetEndGamePending(1,0,1);stage=2;ticks=0;return;
    }
}
