// Opt-in installed-data verification of the new selection using existing services.
void RedProbeTick() {
    static int stage=0,ticks=0,direction=0,switches=0;
    static bool initialized=false,sawAttack=false,sawRun=false;
    static unsigned long long npcId=OBJECT_INVALID;
    static int beforeHP=0,beforePoise=0,chainSerial=0;
    static bool sawNpcAttack=false;
    static bool sawNpcLink=false;
    static GripPose previousSwitchGrip;
    static float switchStep=0,switchRotation=0;
    static GripPose previousChainGrip;
    static bool chainSampled=false;
    static float chainStep=0,chainRotation=0;
    if(stage<0 || !sandboxReady)return;
    auto quit=[] {if(FILE* f=fopen("red-input.txt","w")){fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* why){fprintf(stderr,"K1_RED_FAIL stage=%d direction=%d reason=%s\n",stage,direction,why);stage=-1;quit();};
    auto* actor=Actor();auto* server=ServerActor();
    if(!actor || !server){fail("actor");return;}
    if(!initialized) {
        if(FILE* f=fopen("red-motion.csv","w"))fclose(f);
        SandboxSetAi(false);
        for(const char* resource:{"k1jared","k1jadred"}) {
            if(!RedMotionComponentProbe(resource)){fail("motion_component");return;}
            const char* native=!strcmp(resource,"k1jared")?"pmbbl":"n_darkjedim";
            if(!YellowNativeGaitComponentProbe(resource,native) || !YellowRunBounceProbe(resource,native)){fail("native_run");return;}
            CSWCAnimBase owner;
            if(!owner.LoadModel(CResRef(resource),CSWCAnimBase::ANIM_MODEL_BODY)){fail("resource");return;}
            Gob* body=owner.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);
            body->m_bAlwaysRendering=true;body->EnableForceAnimations();
            for(int d=0;d<7;++d) for(const char* clip:{fullAttacks[d],attacks[d]}) {
                float at=0,length=0;
                if(!body->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,0) ||
                   !body->GetAnimation(const_cast<char*>(clip),&at,&length) || length<=redHitWindows[d][1]){fail("attack_length");return;}
                for(float time:{0.f,redHitWindows[d][0],redHitWindows[d][1],length}) {
                    body->Reset();body->SetAnimationTime(const_cast<char*>(clip),time);
                    body->m_nLastAnimateTick=-1;body->Animate(.000001f);
                    for(const char* bone:{"rhand","lhand","pelvis_g"}) {
                        auto* part=body->GetPart(const_cast<char*>(bone));Vector p;Quaternion q;
                        if(!part){fail("bone");return;}part->GetWorld(p,q);
                        if(!std::isfinite(p.x+p.y+p.z+dot(q,q))){fail("finite_pose");return;}
                    }
                }
                body->PlayAnimation(nullptr,1,0,0);
            }
        }
        if(!K1EngageEnter()){fail("enter");return;}
        initialized=true;ticks=0;return;
    }
    if(++ticks>1800){fail("timeout");return;}
    Gob* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
    if(stage==0) {
        if(!body || !body->m_pModel || _stricmp(body->m_pModel->m_szName,"k1jared"))return;
        if(state.attacking)return;
        static const int forward[]={1,1,1,0,0,-1,-1};
        static const int right[]={0,1,-1,1,-1,1,-1};
        keyW=forward[direction]>0;keyS=forward[direction]<0;
        keyD=right[direction]>0;keyA=right[direction]<0;keyShift=direction%2!=0;
        K1EngageMouse(true,true);stage=1;sawAttack=sawRun=false;ticks=0;return;
    }
    if(stage==1) {
        if(state.attacking) {
            sawAttack=true;
            if(swingDirection!=direction || std::fabs(PlayerHitStart(direction)-redHitWindows[direction][0])>.00001f){fail("direction_window");return;}
            if(!keyShift && playerPlayback.nativeGait.clip=="run")sawRun=true;
            K1EngageMouse(true,false);
        }
        if(!sawAttack || state.attacking)return;
        if(direction%2==0 && !sawRun){fail("live_running_attack");return;}
        fprintf(stderr,"K1_RED_ATTACK_PASS direction=%d run=%d walk=%d\n",direction,sawRun,keyShift);
        keyW=keyS=keyA=keyD=keyShift=false;movement.Clear();
        if(++direction<7){stage=0;ticks=0;return;}
        K1EngageMouse(false,true);stage=6;ticks=0;return;
    }
    if(stage==6) {
        if(ticks<40)return;
        previousSwitchGrip=ReadGrip(body);
        if(FILE* f=fopen("red-input.txt","w")){fputs("mdown:640,360\n+4:mup:640,360\n",f);fclose(f);}stage=2;ticks=0;return;
    }
    if(stage==2) {
        if(!state.Guarding() || !state.rightHeld){fail("switch_lost_guard");return;}
        if(body) {
            auto grip=ReadGrip(body);
            fprintf(stderr,"K1_RED_SWITCH_FRAME switch=%d tick=%d model=%s blend=%g step=%g rotation=%g\n",switches,ticks,body->m_pModel->m_szName,playerPlayback.blendTime,
                magnitude(grip.hilt-previousSwitchGrip.hilt),2*std::acos(std::min(1.f,std::fabs(dot(normalize(grip.hiltQ),normalize(previousSwitchGrip.hiltQ))))));
            switchStep=std::max(switchStep,magnitude(grip.hilt-previousSwitchGrip.hilt));
            switchRotation=std::max(switchRotation,2*std::acos(std::min(1.f,std::fabs(dot(normalize(grip.hiltQ),normalize(previousSwitchGrip.hiltQ))))));
            previousSwitchGrip=grip;
        }
        const bool wantRed=switches%2==1;
        if(!body || !body->m_pModel || _stricmp(body->m_pModel->m_szName,wantRed?"k1jared":"k1jarig"))return;
        if(ticks<12)return; // Holding/releasing middle must not toggle again.
        if(nativeActionBody || NativeUtilityQueued(server)){fail("stance_mouse_native_action");return;}
        if(++switches<8){keyW=switches>=4;if(FILE* f=fopen("red-input.txt","w")){fputs("mdown:640,360\n+4:mup:640,360\n",f);fclose(f);}ticks=0;return;}
        fprintf(stderr,"K1_RED_GUARD_SWITCH step=%g rotation=%g switches=8 moving=1 reversals=1 held=1\n",switchStep,switchRotation);
        if(switchStep>.12f || switchRotation>.5f){fail("switch_pose_jump");return;}
        keyW=false;movement.Clear();stage=7;ticks=0;return;
    }
    if(stage==7) {
        if(!state.Guarding()){fail("settling_lost_guard");return;}
        if(ticks<40)return;
        const auto grip=ReadGrip(body);
        if(magnitude(Rotate(Inverse(grip.hiltQ),grip.left-grip.hilt)-redProbeGuardGap)>.002f){fail("settled_guard_grip");return;}
        K1EngageMouse(false,false);
        CExoString message;auto* npc=SandboxSpawn(3,false,message);
        if(!npc || !RedNpc(npc) || strcmp(NpcRigResource("n_darkjedim",npc),"k1jadred")){fail("npc_selection");return;}
        npcId=npc->m_objectId;
        server->m_hitPoints=1000;server->SetCurrentHitPoints(1000);
        npc->m_hitPoints=1000;npc->SetCurrentHitPoints(1000);
        K1EngageMouse(false,true);stage=3;ticks=0;return;
    }
    if(stage==3) {
        if(ticks<60)return;
        if(!state.Guarding() || !body || _stricmp(body->m_pModel->m_szName,"k1jared") ||
           !currentBase || strcmp(currentBase,"j_guard")){fail("red_guard");return;}
        auto* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(npcId);
        if(!npc){fail("npc_missing");return;}
        beforeHP=server->GetCurrentHitPoints(0);beforePoise=meleePoise[server->m_objectId].percent;
        SandboxSetAi(true);stage=4;ticks=0;return;
    }
    if(stage==4) {
        auto found=npcPlayback.find(npcId);
        if(found!=npcPlayback.end() && found->second.attacking) {
            auto& npc=found->second;
            if(!npc.red || !npc.body || _stricmp(npc.body->m_pModel->m_szName,"k1jadred") ||
               std::fabs(NpcHitStart(npc)-(npc.redFrom>=0?redChains[npc.redFrom][npc.direction].start:redHitWindows[npc.direction][0]))>.00001f){fail("npc_red_attack");return;}
            sawNpcAttack=true;
            sawNpcLink=sawNpcLink || npc.redFrom>=0;
        }
        if(!sawNpcAttack || !sawNpcLink || (server->GetCurrentHitPoints(0)==beforeHP && meleePoise[server->m_objectId].percent==beforePoise))return;
        fprintf(stderr,"K1_RED_NPC_CONTACT_PASS guarded=1 hp=%d poise=%d\n",server->GetCurrentHitPoints(0),meleePoise[server->m_objectId].percent);
        SandboxSetAi(false);K1EngageMouse(false,false);
        // Disabling the final threat intentionally disengages the player.
        if(!K1EngageEnter()){fail("chain_enter");return;}
        auto* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(npcId);
        npc->SetPosition(server->m_position+server->m_orientation*1.1f,1,1,0);
        npc->GetClientCreature()->SetPrecalculatedPosition(npc->m_position);
        beforeHP=npc->GetCurrentHitPoints(0);chainSerial=attackSerial;
        K1EngageMouse(true,true);stage=5;ticks=0;return;
    }
    if(stage==5) {
        const auto grip=ReadGrip(body);
        if(chainSampled) {
            fprintf(stderr,"K1_RED_CHAIN_FRAME tick=%d serial=%d attacking=%d blend=%g base=%s overlay=%s step=%g rotation=%g\n",ticks,attackSerial,state.attacking,playerPlayback.blendTime,currentBase?currentBase:"none",currentOverlay?currentOverlay:"none",
                magnitude(grip.hilt-previousChainGrip.hilt),2*std::acos(std::min(1.f,std::fabs(dot(normalize(grip.hiltQ),normalize(previousChainGrip.hiltQ))))));
            chainStep=std::max(chainStep,magnitude(grip.hilt-previousChainGrip.hilt));
            chainRotation=std::max(chainRotation,2*std::acos(std::min(1.f,std::fabs(dot(normalize(grip.hiltQ),normalize(previousChainGrip.hiltQ))))));
        }
        chainSampled=true;previousChainGrip=grip;
        if(attackSerial<chainSerial+3 || state.attacking)return;
        K1EngageMouse(true,false);
        auto* npc=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(npcId);
        if(!npc || npc->GetCurrentHitPoints(0)>=beforeHP){fail("player_contact_damage");return;}
        fprintf(stderr,"K1_RED_CHAIN_CONTACT_PASS swings=%d damage=%d\n",attackSerial-chainSerial,beforeHP-npc->GetCurrentHitPoints(0));
        fprintf(stderr,"K1_RED_CHAIN_MOTION step=%g rotation=%g\n",chainStep,chainRotation);
        if(chainRotation>1.f){fail("chain_rotation_jump");return;}
        if(!RebuildOwnedRig(actor,false)){fail("native_restore");return;}
        body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(!body || IsSourceBody(body)){fail("native_body");return;}
        fprintf(stderr,"K1_RED_PASS attacks=7 rigs=2 native_forward_reverse=1 run_bounce=1 switching=8 held_guard=1 npc_contact=1 guard=1 chain=3 player_damage=1 native_restore=1\n");
        stage=-1;quit();
    }
}
