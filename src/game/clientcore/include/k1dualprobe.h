// Opt-in installed-data integration test. No production AI/tuning changes.
bool DualGuardWalkCheck(bool walking,float& positionError,float& rotationError) {
    auto* actor=Actor();auto* body=actor?actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
    if(!body || nativeDualGuardPose.empty())return false;
    if(strcmp(actor->GetAnimationBase()->GetAnimationName(CSWCAnimBase::CLIENT_ANIMATION_DUAL_SABER_READY).CStr(),"g4r1"))return false;
    int hands=0;
    Part* torsoRoot=body->GetPart(const_cast<char*>("torso_g"));
    for(const auto& expected:nativeDualGuardPose) {
        const char* name=expected.part->m_pNode->TranslateNodeName();
        const bool hand=!strcmp(name,"lhand") || !strcmp(name,"rhand") || !strcmp(name,"Lhand_g") || !strcmp(name,"Rhand_g");
        if(hand)++hands;
        bool upper=false;
        for(Part* p=expected.part;p;p=p->m_pParent)if(p==torsoRoot)upper=true;
        if(walking?(!upper || expected.part==torsoRoot):!hand)continue;
        positionError=std::max(positionError,magnitude(expected.part->m_vLocalPosition-expected.position));
        rotationError=std::max(rotationError,1.f-std::fabs(dot(normalize(expected.part->m_qLocalOrientation),normalize(expected.orientation))));
        if(positionError>=.002f || rotationError>=.0001f)fprintf(stderr,"K1_DUAL_NATIVE_HAND_DIFF bone=%s position=%g rotation=%g\n",name,positionError,rotationError);
    }
    if(hands!=4)return false;
    if(walking) {
        Part* torso=body->GetPart(const_cast<char*>("torso_g"));
        Vector p;
        for(int hand=0;hand<2;++hand) {
            Part* grip=body->GetPart(const_cast<char*>(hand?"lhand":"rhand"));
            std::vector<Part*> chain;
            for(Part* part=grip;part && part!=torso;part=part->m_pParent)chain.push_back(part);
            Quaternion expected;torso->GetWorld(p,expected);
            for(auto it=chain.rbegin();it!=chain.rend();++it) {
                bool found=false;
                for(const auto& pose:nativeDualGuardPose)if(pose.part==*it){expected=normalize(expected*pose.orientation);found=true;break;}
                if(!found)return false;
            }
            K1SaberContact::Blade blade;
            if(!DualBlade(actor,hand,blade) || dot(normalize(blade.tip-blade.base),expected.zdir())<.995f)return false;
        }
        // The generator independently verifies the exact completed walk keys.
        // Check that native guard application/smoothing preserves those lower
        // joints in the same rendered frame, avoiding independent clock state.
        positionError=std::max(positionError,dualWalkLowerPositionError);
        rotationError=std::max(rotationError,dualWalkLowerRotationError);
    }
    return positionError<.002f && rotationError<.0001f;
}

bool DualPoiseRowsCheck(CSWSCreature* player,int percent) {
    if(!hudOwner || !DualMode())return false;
    UpdatePoiseHud(hudOwner);
    auto& main=hudOwner->m_poiseWeapon;auto& off= hudOwner->m_poiseOffhand;
    auto* actor=player->GetClientCreature();
    auto* right=actor->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
    auto* left=actor->GetModel(CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY,1);
    auto* mainModel=main.m_scene.m_aModels.m_count?static_cast<Gob*>(main.m_scene.m_aModels.m_pArray[0]):nullptr;
    auto* offModel=off.m_scene.m_aModels.m_count?static_cast<Gob*>(off.m_scene.m_aModels.m_pArray[0]):nullptr;
    const bool ok=right && left && mainModel && offModel && main.m_tail.bVisible && off.m_tail.bVisible &&
        main.resource==CExoString(right->m_pModel->m_szName) && off.resource==CExoString(left->m_pModel->m_szName) &&
        main.m_scene.m_pScene!=off.m_scene.m_pScene && mainModel!=right && offModel!=left &&
        main.m_extent.x==off.m_extent.x && main.m_extent.w==off.m_extent.w && main.m_extent.h==off.m_extent.h &&
        off.m_extent.y>main.m_extent.y && std::fabs(main.modelLength-off.modelLength)<.000001f &&
        main.percent==percent && off.percent==percent && main.lastDrawnPercent==percent && off.lastDrawnPercent==percent &&
        K1EngagePoiseBladePercent(mainModel)==percent && K1EngagePoiseBladePercent(offModel)==percent;
    fprintf(stderr,"K1_DUAL_POISE_ROWS_%s percent=%d main=%s offhand=%s size=%d,%d y=%d,%d draws=%u,%u\n",ok?"PASS":"FAIL",percent,
        main.resource.CStr(),off.resource.CStr(),main.m_extent.w,main.m_extent.h,main.m_extent.y,off.m_extent.y,main.draws,off.draws);
    return ok;
}

void DualPlayableProbeTick(int& phase,int& ticks,CSWSCreature* player,CSWSCreature* enemy) {
    static int trial=0,serial=0,beforeHP=0,poseSamples=0;
    static int hudTrial=0;
    static int gaitTrial=0,gaitSamples=0,recoveryTrial=0,nativeFrames=0,recoveryStable=0;
    static float gaitTorsoError=0;
    static int topTrial=0;
    static unsigned long long topDamageBefore=0;
    static bool sawNative=false;
    static float guardPositionError=0,guardRotationError=0;
    static bool sawSwing=false;
    auto quit=[] {if(FILE* f=fopen("dual-input.txt","w")){fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* reason){fprintf(stderr,"K1_DUAL_PLAYABLE_FAIL phase=%d trial=%d reason=%s\n",phase,trial,reason);phase=-1;quit();};
    if(!player || !enemy){fail("actors");return;}
    const auto now=g_pAppManager->m_pServerExoApp->GetWorldTimer()->GetSnapshotTime();
    arenaForce.next=now+60000000;
    auto npc=npcPlayback.find(enemy->m_objectId);
    if(npc!=npcPlayback.end()) {
        npc->second.nextThink=npc->second.nextAttack=now+60000000;
        npc->second.attacking=npc->second.parrying=npc->second.closeGuard=false;
        StopNpcMove(npc->second,enemy);
    }
    enemy->ClearAllActions(1);enemy->m_pCombatRound->EndCombatRound(1);
    auto* actor=player->GetClientCreature();
    if(phase==2) {
        if(!K1EngageEnter()){fail("enter");return;}
        const auto savedPoise=meleePoise;const auto savedState=state;
        auto& p=meleePoise[player->m_objectId];p=MeleePoise();p.percent=50;
        SelectPoiseTiming(p,false,now,true);DelayPoiseRegeneration(p,now);
        bool balance=std::fabs(PlayerMeleeRate()-.85f)<.00001f && PlayerChainLimit()==3 &&
            GuardPoiseDamage(player)==13 && ParriedPoiseLoss(player)==20 && ParryPoiseReward(player)==15;
        balance=balance && !AdvancePoiseRegeneration(p,now+2999999,false,true) &&
            AdvancePoiseRegeneration(p,now+3000000,false,true)==1 && p.percent==51 &&
            !AdvancePoiseRegeneration(p,now+3499999,false,true) &&
            AdvancePoiseRegeneration(p,now+3500000,false,true)==1;
        p.percent=50;p.quietUntil=now-1;p.nextTick=now+123456;p.wasEligible=true;
        const auto next=p.nextTick,quiet=p.quietUntil;
        ApplySuccessfulParry(enemy,player);
        balance=balance && p.percent==65 && p.nextTick==next && p.quietUntil==quiet &&
            AdvancePoiseRegeneration(p,next,false,true)==1 && p.percent==66;
        meleePoise=savedPoise;state=savedState;
        if(!balance){fail("dual_balance_or_parry_regeneration");return;}
        fprintf(stderr,"K1_DUAL_BALANCE_PASS rate=.85 chain=3 block=13 parried=20 reward=15 quiet_us=3000000 tick_us=500000 parry_preserves_tick=1\n");
        enemy->SetCurrentHitPoints(10000);phase=760;ticks=0;return;
    }
    if(phase==760 || phase==761) {
        if(ticks==1) {
            state.ClearInputs();keyW=keyS=keyA=keyD=keyShift=false;
            player->SetPosition(Vector(200,100,0),1,1,0);
            actor->SetPrecalculatedPosition(player->m_position);
            player->SetOrientation(Vector(1,0,0));aimYaw=0;aimPitch=0;
            enemy->SetPosition(Vector(std::getenv("K1_ENGAGE_DUAL_EXIT_TEST")?203.f:210.f,100,0),1,1,0);
            player->ClearAllActions(1);player->m_pCombatRound->EndCombatRound(1);
        }
        if(ticks<35)return;
        K1SaberContact::Blade blades[2];
        auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(!DualMode() || !Supported(actor) || !body || !body->m_pModel || _stricmp(body->m_pModel->m_szName,"k1jadual") ||
           !DualBlade(actor,0,blades[0]) || !DualBlade(actor,1,blades[1])) {
            if(ticks>240)fail("dual_equipment_or_pose");return;
        }
        if(trial==0)fprintf(stderr,"K1_DUAL_EQUIPMENT_PASS class=%d right=%llu left=%llu native_models=2\n",actor->GetAnimationBase()->m_nWeaponClass,actor->m_aEquippedItemIds[4],actor->m_aEquippedItemIds[5]);
        if(std::getenv("K1_ENGAGE_DUAL_EXIT_TEST")) {
            const auto owned=npcPlayback.find(enemy->m_objectId);
            if(owned==npcPlayback.end() || !owned->second.requested) {
                if(ticks>240)fail("shutdown_fixture_empty");return;
            }
            fprintf(stderr,"K1_DUAL_SHUTDOWN_READY active_npcs=%zu source_player=1\n",npcPlayback.size());phase=-1;quit();return;
        }
        const int d=trial%7;
        keyW=d==0||d==1||d==2;keyS=d==5||d==6;
        keyA=d==2||d==4||d==6;keyD=d==1||d==3||d==5;
        serial=attackSerial;sawSwing=false;
        K1EngageMouse(true,true);K1EngageMouse(true,false);
        if(trial<7)keyW=keyS=keyA=keyD=false;
        keyShift=trial>=7 && trial<14;phase=762;ticks=0;return;
    }
    if(phase==762) {
        if(state.attacking && attackSerial==serial+1) {
            if(swingDirection!=trial%7){fail("direction_selection");return;}
            if(playerPlayback.lastGrip.joined){fail("dual_attack_uses_single_hilt_filter");return;}
            sawSwing=true;
            for(int hand=0;hand<2;++hand) {
                K1SaberContact::Blade blade;
                if(!DualBlade(actor,hand,blade) || !K1SaberContact::Finite(blade.base) || !K1SaberContact::Finite(blade.tip) ||
                   magnitude(blade.tip-blade.base)<.4f || magnitude(blade.tip-blade.base)>2.f){fail("native_blade");return;}
                auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
                auto* joint=body?body->GetPart(const_cast<char*>(hand?"lhand":"rhand")):nullptr;
                if(!joint || magnitude(blade.base-PartPointWorld(joint,Vector()))>.4f){fail("weapon_attachment");return;}
                std::vector<K1SaberContact::Triangle> skin;CollisionTriangles(body,skin);
                if(skin.size()<100){fail("skin_coverage");return;}
                for(const auto& face:skin) if(magnitude(face.a-actor->m_position)>3.f){fail("skin_bounds");return;}
            }
            ++poseSamples;
        }
        if(sawSwing && !state.attacking) {
            fprintf(stderr,"K1_DUAL_PLAYBACK_PASS trial=%d direction=%d moving=%d running=%d\n",trial,trial%7,trial>=7,trial>=14);
            keyW=keyS=keyA=keyD=keyShift=false;
            if(++trial<21){phase=761;ticks=0;return;}
            phase=780;ticks=0;return;
        }
        if(ticks>360)fail("swing_completion");return;
    }
    if(phase==780) {
        keyW=keyS=keyA=keyD=keyShift=false;state.ClearInputs();
        player->SetPosition(Vector(200,100,0),1,1,0);actor->SetPrecalculatedPosition(player->m_position);
        player->SetOrientation(Vector(1,0,0));aimYaw=aimPitch=0;
        enemy->SetPosition(Vector(200.6f+.2f*topTrial,100,0),1,1,0);enemy->SetOrientation(Vector(-1,0,0));
        enemy->SetCurrentHitPoints(10000);
        enemy->m_bCommandable=0;enemy->SetAnimation(SERVER_ANIMATION_PAUSE);
        if(ticks<30)return;
        serial=attackSerial;beforeHP=enemy->GetCurrentHitPoints(0);sawSwing=false;
        topDamageBefore=meleeDamageContacts;
        K1EngageMouse(true,true);K1EngageMouse(true,false);phase=781;ticks=0;return;
    }
    if(phase==781) {
        if(state.attacking && attackSerial==serial+1) {
            sawSwing=true;
            auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);float t=0,length=0;
            if(body && currentOverlay && body->GetAnimation(const_cast<char*>(currentOverlay),&t,&length)) {
                auto* target=enemy->GetClientCreature();std::vector<K1SaberContact::Triangle> triangles;
                CollisionTriangles(target->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1),triangles);
                CollisionTriangles(target->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1),triangles);
                for(int hand=0;hand<2;++hand) {
                    K1SaberContact::Blade blade;bool hit=false;
                    if(DualBlade(actor,hand,blade))for(const auto& face:triangles)if(K1SaberContact::SegmentTriangle(blade.base,blade.tip,face)){hit=true;break;}
                    fprintf(stderr,"K1_DUAL_TOP_PATH trial=%d hand=%d time=%g intersects=%d consumed=%u hp=%d base=%g,%g,%g tip=%g,%g,%g target=%g,%g,%g\n",topTrial,hand,t,hit,dualContactHands,enemy->GetCurrentHitPoints(0),
                        blade.base.x,blade.base.y,blade.base.z,blade.tip.x,blade.tip.y,blade.tip.z,enemy->m_position.x,enemy->m_position.y,enemy->m_position.z);
                }
            }
        }
        if(sawSwing && !state.attacking) {
            fprintf(stderr,"K1_DUAL_TOP_NATURAL trial=%d damage=%d hands=%u native_contacts=%llu\n",topTrial,beforeHP-enemy->GetCurrentHitPoints(0),dualContactHands,meleeDamageContacts-topDamageBefore);
            const int hands=int(dualContactHands&1u)+int((dualContactHands>>1)&1u);
            if(meleeDamageContacts-topDamageBefore!=hands || (hands>0)!=(enemy->GetCurrentHitPoints(0)<beforeHP)){fail("natural_top_contact_delivery");return;}
            if(topTrial>=3 && (dualContactHands!=3 || meleeDamageContacts-topDamageBefore!=2 || enemy->GetCurrentHitPoints(0)>=beforeHP)){fail("natural_top_both_native_hits");return;}
            if(++topTrial<5){phase=780;ticks=0;return;}
            enemy->m_bCommandable=1;
            enemy->SetPosition(Vector(210,100,0),1,1,0);
            dualWalkLowerPositionError=dualWalkLowerRotationError=0;dualWalkLowerSamples=0;
            K1EngageMouse(false,true);keyW=true;phase=763;ticks=0;return;
        }
        if(ticks>360)fail("top_swing_completion");return;
    }
    if(phase==763) {
        if(ticks>=25 && !DualGuardWalkCheck(true,guardPositionError,guardRotationError)) {
            fprintf(stderr,"K1_DUAL_GUARD_DIFF walking=1 position=%g rotation=%g\n",guardPositionError,guardRotationError);fail("native_guard_or_normal_walk");return;
        }
        if(ticks<90)return;
        if(!state.Guarding() || !state.ForceWalk() || dualWalkLowerSamples<45){fail("moving_guard");return;}
        fprintf(stderr,"K1_DUAL_NATIVE_GUARD_PASS walking=1 position=%g rotation=%g normal_lower=1\n",guardPositionError,guardRotationError);
        keyW=false;guardPositionError=guardRotationError=0;phase=768;ticks=0;return;
    }
    if(phase==768) {
        if(ticks<45)return;
        if(!DualGuardWalkCheck(false,guardPositionError,guardRotationError)) {
            fprintf(stderr,"K1_DUAL_GUARD_DIFF walking=0 position=%g rotation=%g\n",guardPositionError,guardRotationError);fail("native_standing_guard");return;
        }
        fprintf(stderr,"K1_DUAL_NATIVE_GUARD_PASS walking=0 position=%g rotation=%g\n",guardPositionError,guardRotationError);
        hudTrial=0;phase=769;ticks=0;return;
    }
    if(phase==769) {
        const int values[]={100,50,0,100};
        auto& poise=meleePoise[player->m_objectId];poise.percent=values[hudTrial];DelayPoiseRegeneration(poise,now);
        UpdatePoiseHud(hudOwner);
        if(ticks<15)return;
        if(!DualPoiseRowsCheck(player,values[hudTrial])){fail("paired_poise_rows");return;}
        if(++hudTrial<4){ticks=0;return;}
        K1EngageMouse(false,false);gaitTrial=0;phase=770;ticks=0;return;
    }
    if(phase==770) {
        state.ClearInputs();keyW=keyS=keyA=keyD=keyShift=false;
        player->SetPosition(Vector(200,100,0),1,1,0);actor->SetPrecalculatedPosition(player->m_position);
        player->SetOrientation(Vector(1,0,0));aimYaw=aimPitch=0;
        enemy->SetPosition(Vector(210,100,0),1,1,0);
        keyW=gaitTrial<4 && gaitTrial%2==0;keyS=gaitTrial<4 && !keyW;
        keyA=gaitTrial>=4 && gaitTrial%2==0;keyD=gaitTrial>=4 && !keyA;
        keyShift=gaitTrial<2 || (gaitTrial>=4 && gaitTrial<6);
        gaitSamples=0;gaitTorsoError=0;phase=771;ticks=0;return;
    }
    if(phase==771) {
        if(ticks>=20) {
            // Completed locomotion turns the forward gait for strafing.
            const char* clips[]={"j_walk","j_backwalk","j_run","j_backrun","j_walk","j_walk","j_run","j_run"};
            const char* wanted=clips[gaitTrial];
            if(!currentBase || strcmp(currentBase,wanted) || currentOverlay || state.Guarding() || nativeDualGuardPose.empty()) {fail("default_full_locomotion_selection");return;}
            if(strstr(wanted,"walk")) {
                auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
                for(const char* name:{"torso_g","torsoMid_g","torsoUpr_g"}) {
                    auto* joint=body->GetPart(const_cast<char*>(name));Quaternion expected,actual;Vector position;
                    if(!joint || !DualWalkSpineReference(joint,nativeIdlePose,expected)){fail("walking_torso_reference");return;}
                    joint->GetWorld(position,actual);
                    const float error=1.f-std::fabs(dot(normalize(actual),normalize(body->m_qOrientation*expected)));
                    gaitTorsoError=std::max(gaitTorsoError,error);
                    if(error>.00001f){fail("walking_torso_rotates");return;}
                }
                if(dualWalkLowerPositionError>.002f || dualWalkLowerRotationError>.0001f){fail("steady_walk_changes_legs");return;}
            }
            int attachments=0;
            for(const auto& expected:nativeDualGuardPose) {
                const char* name=expected.part->m_pNode->TranslateNodeName();
                if(strcmp(name,"lhand") && strcmp(name,"rhand") && !strstr(name,"Fngr") && !strstr(name,"Thumb"))continue;
                attachments+=!strcmp(name,"lhand") || !strcmp(name,"rhand");
                if(magnitude(expected.part->m_vLocalPosition-expected.position)>.00001f ||
                   1.f-std::fabs(dot(normalize(expected.part->m_qLocalOrientation),normalize(expected.orientation)))>.000001f){fail("locomotion_native_hand_grip");return;}
            }
            if(attachments!=2){fail("locomotion_attachment_count");return;}
            for(int hand=0;hand<2;++hand) {
                K1SaberContact::Blade blade;
                auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
                auto* grip=body->GetPart(const_cast<char*>(hand?"lhand":"rhand"));
                Vector position;Quaternion orientation;grip->GetWorld(position,orientation);
                if(!DualBlade(actor,hand,blade) || dot(normalize(blade.tip-blade.base),orientation.zdir())<.995f) {
                    fail("locomotion_physical_grip_axis");return;
                }
                if(strstr(wanted,"walk") && dot(normalize(blade.tip-blade.base),normalize(body->m_qOrientation.ydir()+Vector(0,0,.35f)))<.995f) {
                    fail("walking_physical_blade_direction");return;
                }
            }
            ++gaitSamples;
        }
        if(ticks<100)return;
        fprintf(stderr,"K1_DUAL_DEFAULT_GAIT_PASS trial=%d clip=%s samples=%d independent_native_grips=1\n",gaitTrial,currentBase,gaitSamples);
        if(strstr(currentBase,"walk"))fprintf(stderr,"K1_DUAL_WALK_TORSO_PASS trial=%d samples=%d rotation_error=%g lower_position=%g lower_rotation=%g\n",
            gaitTrial,gaitSamples,gaitTorsoError,dualWalkLowerPositionError,dualWalkLowerRotationError);
        if(++gaitTrial<8){phase=770;ticks=0;return;}
        keyW=keyS=keyA=keyD=keyShift=false;recoveryTrial=0;phase=772;ticks=0;return;
    }
    if(phase==772) {
        if(ticks<35)return;
        K1EngageMouse(false,true);sawNative=false;nativeFrames=recoveryStable=0;
        auto* effect=new CGameEffect(1);
        effect->m_subType=recoveryTrial?CSWSEffectListHandler::EFFECT_KNOCKDOWN:CSWSEffectListHandler::EFFECT_SETSTATE;
        effect->m_durationTypeBits=CGameEffect::DURATION_TYPE_TEMPORARY;effect->m_fDuration=1.f;effect->SetCreator(enemy->m_objectId);
        effect->SetInteger(0,EFFECT_STATE_KNOCKBACK);player->ApplyEffect(effect,0,1);
        fprintf(stderr,"K1_DUAL_NATIVE_EFFECT trial=%d state=%d animation=%d commandable=%d plot=%d\n",recoveryTrial,player->m_nState,player->m_nAnimation,player->m_bCommandable,player->m_bPlot);
        phase=773;ticks=0;return;
    }
    if(phase==773) {
        auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(NativeStatusAnimation(player) || nativeActionBody) {
            if(body && !IsSourceBody(body) && !K1EngageOwnsAnimation(body)){sawNative=true;++nativeFrames;}
            if(recoveryTrial==1 && nativeFrames==3){K1EngageMouse(false,false);K1EngageMouse(false,true);}
            recoveryStable=0;
        } else if(sawNative && body && IsSourceBody(body) && state.Guarding()) {
            if(++recoveryStable>=25) {
                float p=0,q=0;
                if(!DualGuardWalkCheck(false,p,q)){fail("native_recovery_dual_guard");return;}
                fprintf(stderr,"K1_DUAL_NATIVE_RECOVERY_PASS trial=%d native_frames=%d held_guard=1 ready=g4r1 position=%g rotation=%g\n",recoveryTrial,nativeFrames,p,q);
                if(++recoveryTrial<2){phase=772;ticks=0;return;}
                K1EngageMouse(false,false);serial=attackSerial;K1EngageMouse(true,true);phase=764;ticks=0;return;
            }
        }
        if(ticks>600){fprintf(stderr,"K1_DUAL_NATIVE_RECOVERY_WAIT trial=%d state=%d animation=%d commandable=%d native_frames=%d active=%d guard=%d\n",recoveryTrial,player->m_nState,player->m_nAnimation,player->m_bCommandable,nativeFrames,nativeActionBody,state.Guarding());fail("native_recovery_timeout");}return;
    }
    if(phase==764) {
        if(attackSerial>serial+PlayerChainLimit()){fail("held_chain_cap");return;}
        if(attackSerial==serial+PlayerChainLimit() && !state.attacking) {
            K1EngageMouse(true,false);fprintf(stderr,"K1_DUAL_CHAIN_GUARD_PASS chain=%d moving_guard=1\n",PlayerChainLimit());
            trial=0;phase=765;ticks=0;return;
        }
        if(ticks>650)fail("held_chain_completion");return;
    }
    if(phase==766) {
        if(ticks<20)return;
        if(enemy->GetCurrentHitPoints(0)>=beforeHP){fail("native_damage_delivery");return;}
        fprintf(stderr,"K1_DUAL_DAMAGE_PASS trial=%d hand=%d direction=%d health_loss=%d\n",trial,trial%2,trial/2,beforeHP-enemy->GetCurrentHitPoints(0));
        if(++trial<14){phase=765;ticks=0;return;}
        enemy->SetCurrentHitPoints(0);phase=767;ticks=0;return;
    }
    if(phase==767) {
        auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(ticks>2 && state.mode==K1EngageState::Mode::Exploration && body && !IsSourceBody(body) && !K1EngageOwnsAnimation(body)) {
            if(hudOwner && hudOwner->m_poiseOffhand.m_tail.bVisible){fail("offhand_hud_cleanup");return;}
            fprintf(stderr,"K1_DUAL_PLAYABLE_PASS attacks=21 hand_contacts=14 chain=3 guard=1 native_restore=1 pose_samples=%d\n",poseSamples);
            phase=-1;quit();return;
        }
        if(ticks>300)fail("native_restoration");return;
    }
    if(phase!=765)return;
    // The locomotion trials deliberately moved the player. Keep the controlled
    // geometry fixture within the production contact broad-phase range.
    if(ticks==1)enemy->SetPosition(player->m_position+player->m_orientation*1.2f,1,1,0);
    if(ticks<30)return;
    const int hand=trial%2,direction=trial/2;
    auto* weapon=actor->GetModel(hand?CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY:CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
    auto* item=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[4+hand]);
    auto* other=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[5-hand]);
    auto* victim=enemy->GetClientCreature();
    if(!weapon || !item || !other || !victim || npc==npcPlayback.end()){fail("contact_models");return;}
    std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(victim->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1),triangles);
    const auto position=weapon->m_vPosition;const auto orientation=weapon->m_qOrientation;
    const int powered=item->m_bPowered,otherPowered=other->m_bPowered;
    other->m_bPowered=0;item->m_bPowered=1;
    weapon->m_qOrientation=Quaternion(Vector(0,1,0),1.5707963268f);weapon->m_vPosition=Vector();
    K1SaberContact::Blade beam;bool located=DualBlade(actor,hand,beam);
    if(located) {
        located=false;
        for(const auto& face:triangles) {
            const auto center=(face.a+face.b+face.c)/3.f;
            if(center.z-enemy->m_position.z<.9f || center.z-enemy->m_position.z>1.6f)continue;
            weapon->m_vPosition=center-(beam.base+beam.tip)*.5f;
            K1SaberContact::Blade actual;
            if(DualBlade(actor,hand,actual) && K1SaberContact::SegmentTriangle(actual.base,actual.tip,face)){located=true;break;}
        }
    }
    const auto savedState=state;const int savedDirection=swingDirection;
    const auto queued=sceneMeleeContacts.size();
    state.attacking=true;swingDirection=direction;++attackSerial;hadBlade=false;hitConsumed=false;
    const float start=PlayerHitStart(direction),end=ContactEnd(PlayerHitEnd(direction));
    bool ok=located;
    TraceDualBlades(actor,start-.01f);ok=sceneMeleeContacts.size()==queued && ok;
    hadBlade=false;TraceDualBlades(actor,end+.01f);ok=sceneMeleeContacts.size()==queued && ok;
    item->m_bPowered=0;hadBlade=false;TraceDualBlades(actor,start+.01f);ok=sceneMeleeContacts.size()==queued && ok;
    item->m_bPowered=1;hadBlade=false;TraceDualBlades(actor,start+.02f);
    ok=sceneMeleeContacts.size()==queued+1 && ok;
    fprintf(stderr,"K1_DUAL_CONTACT_FIXTURE trial=%d located=%d triangles=%zu distance=%g queued=%zu new=%zu\n",trial,located,triangles.size(),magnitude(enemy->m_position-player->m_position),queued,sceneMeleeContacts.size()-queued);
    if(ok) {
        auto contact=sceneMeleeContacts.back();
        auto* nativeItem=player->m_pInventory->GetItemInSlot(hand?CSWInventory::SLOT_LEFTWEAPON:CSWInventory::SLOT_RIGHTWEAPON);
        ok=contact.target==enemy->m_objectId && contact.offhand==(hand!=0) && nativeItem && contact.dualWeapon==nativeItem->m_objectId;
        TraceDualBlades(actor,start+.03f);ok=sceneMeleeContacts.size()==queued+1 && ok;
        if(trial==0) {
            const auto damageBefore=meleeDamageContacts;
            auto* round=player->m_pCombatRound;const int spell=round->m_bSpellCastRound;
            round->m_bSpellCastRound=1;ResolveMeleeContacts({contact,contact},now);round->m_bSpellCastRound=spell;
            ok=ok && meleeDamageContacts==damageBefore && !(dualContactHands&(1u<<hand));
            hadBlade=false;TraceDualBlades(actor,start+.04f);
            ok=ok && sceneMeleeContacts.size()==queued+2;
            if(ok)contact=sceneMeleeContacts.back();
            const auto activeState=state;const auto playerPoise=meleePoise[player->m_objectId],enemyPoise=meleePoise[enemy->m_objectId];
            meleePoise[player->m_objectId].percent=meleePoise[enemy->m_objectId].percent=100;
            ApplySuccessfulParry(player,enemy);ResolveMeleeContacts({contact},now);
            ok=ok && meleeDamageContacts==damageBefore && !state.attacking;
            state=activeState;meleePoise[player->m_objectId]=playerPoise;meleePoise[enemy->m_objectId]=enemyPoise;
            const bool enemyAttacking=npc->second.attacking;
            npc->second.attacking=true;round->m_bSpellCastRound=1;
            ResolveMeleeContacts({contact},now);
            ok=ok && meleeDamageContacts==damageBefore && !(dualContactHands&(1u<<hand));
            ResolveMeleeContacts({},now+300001);
            ok=ok && meleeDamageContacts==damageBefore;
            npc->second.attacking=enemyAttacking;round->m_bSpellCastRound=spell;
            fprintf(stderr,"K1_DUAL_IMMEDIATE_CONTACT_%s wait_us=0 busy_retry=1 parry_cancels_contact=1 empty_update_no_replay=1\n",ok?"PASS":"FAIL");
        }
        beforeHP=enemy->GetCurrentHitPoints(0);
        ResolveMeleeContacts({contact,contact},now);
    }
    sceneMeleeContacts.resize(queued);state=savedState;swingDirection=savedDirection;hadBlade=false;
    weapon->m_vPosition=position;weapon->m_qOrientation=orientation;item->m_bPowered=powered;other->m_bPowered=otherPowered;
    if(!ok){fail("geometry_timing_repeat_or_hand_identity");return;}
    phase=766;ticks=0;
}



