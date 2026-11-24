// Opt-in installed-data test using the existing native equipment/spawn controls.
bool SandboxDualEquipMenu(CSWCCreature* actor,int slot,unsigned long long item) {
    auto* gui=g_pAppManager->m_pClientExoApp->GetInGameGui();
    auto* server=g_pAppManager->m_pServerExoApp;
    auto* panel=new CSWGuiInGameEquip(gui->m_pGuiManager);
    panel->SetCharacter(actor);panel->UpdateInventory();
    panel->OnEnterSlot(&panel->m_abtnSlot[slot]);
    panel->m_abtnSlot[slot].m_tail.nCurrentEventArg=1;panel->OnSelectSlot(&panel->m_abtnSlot[slot]);
    bool selected=false;
    for(int i=0;i<panel->m_lbItems.m_aControls.m_count;++i) {
        auto* row=static_cast<CSWGuiInGameItemEntry*>(panel->m_lbItems.m_aControls.m_pArray[i]);
        if(server->ClientToServerObjectId(row->m_oidItem)!=item)continue;
        row->m_tail.nCurrentEventArg=1;row->m_tail.nCurrentEventType=INPUTEVENT_GUI_A;
        panel->OnItemSelected(row);panel->m_btnEquip.m_tail.nCurrentEventArg=1;
        panel->OnOKPressed(&panel->m_btnEquip);selected=true;break;
    }
    delete panel;return selected;
}
void SandboxDualProbeTick() {
    static int phase=0,ticks=0,step=0;static bool waiting=false,sawNative=false,startedCombat=false,aiVerified=false;
    static unsigned long long hostileId=OBJECT_INVALID,friendlyId=OBJECT_INVALID;
    static CResRef texture;static std::string headName;
    static int queueHP=0;
    static int forceBefore=0,forceCount=0;static bool sawCasterNative=false;
    static int clashAiPhase=0;static long long clashAiAt=0;
    if(phase<0 || !sandboxReady)return;++ticks;
    auto* player=ServerActor();auto* client=Actor();auto* server=g_pAppManager->m_pServerExoApp;
    auto* hostile=server->GetCreatureByGameObjectID(hostileId);
    auto* friendly=server->GetCreatureByGameObjectID(friendlyId);
    auto send=[](const char* text){if(FILE* f=fopen("dual-sandbox-input.txt","w")){fputs(text,f);fclose(f);}};
    auto fail=[&](const char* reason){fprintf(stderr,"K1_SANDBOX_DUAL_FAIL phase=%d step=%d reason=%s\n",phase,step,reason);phase=-1;send("quit:0\n");};
    auto button=[&](int index){auto* control=&sandboxPanel->buttons[index];sandboxPanel->SetActiveControl(control,0);sandboxPanel->HandleInputEvent(INPUTEVENT_GUI_A,1);};
    if(!player || !client){fail("player");return;}
    const auto now=server->GetWorldTimer()->GetSnapshotTime();
    if(phase==0 && ticks>=30) {
        if(!waiting) {
            if(step==0){texture=client->GetAnimationBase()->m_resModelTexture;auto* head=client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1);headName=head->m_pModel->m_szName;}
            const bool offhand=step==2 || step==5;
            const auto item=offhand?arenaOffhandSaberId:arenaSaberIds[step==0 || step==3?1:0];
            if(!SandboxDualEquipMenu(client,offhand?CSWGuiInGameEquip::GUI_SLOT_WEAP_L:CSWGuiInGameEquip::GUI_SLOT_WEAP_R,item)){fail("native_menu_item");return;}
            waiting=true;ticks=0;return;
        }
        const bool dual=step==2 || step==5,staff=step==0 || step==3;
        if(DualMode()!=dual || StaffMode()!=staff || !Supported(client)){if(ticks>180)fail("menu_replication");return;}
        if(!CheckArenaSaberInventory(player,"dual_menu") || !K1EngageEnter()){fail("inventory_or_enter");return;}
        auto* body=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        auto* weapon=client->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
        if(!body || _stricmp(body->m_pModel->m_szName,PlayerRigResource()) || !weapon ||
           !CheckRigActorOwnership(client,true,texture,headName,weapon->m_pModel->m_szName,"sandbox_dual_menu")){fail("rig_or_appearance");return;}
        phase=1;ticks=0;return;
    }
    if(phase==1 && ticks>=20) {
        if(DualMode()) {
            K1SaberContact::Blade a,b;
            if(!DualBlade(client,0,a) || !DualBlade(client,1,b) || magnitude(a.base-b.base)<.01f){fail("independent_player_blades");return;}
            auto* right=player->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
            auto* left=player->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON);
            MeleeContact r={player->m_objectId,OBJECT_INVALID,0};r.dualWeapon=right->m_objectId;
            MeleeContact l=r;l.offhand=true;l.dualWeapon=left->m_objectId;
            bool cuts=CutSaberContact(player,r)&&CutSaberContact(player,l);
            const int oldR=right->m_baseItem,oldL=left->m_baseItem;
            left->m_baseItem=BASE_ITEM_LONG_SWORD;cuts=cuts&&!CutSaberContact(player,l)&&CutSaberContact(player,r);left->m_baseItem=oldL;
            right->m_baseItem=BASE_ITEM_LONG_SWORD;cuts=cuts&&!CutSaberContact(player,r)&&CutSaberContact(player,l);right->m_baseItem=oldR;
            auto* visual=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(client->m_aEquippedItemIds[5]);
            const auto powered=visual->m_bPowered;visual->m_bPowered=0;cuts=cuts&&!CutSaberContact(player,l);visual->m_bPowered=powered;
            l.dualWeapon=right->m_objectId;cuts=cuts&&!CutSaberContact(player,l);
            fprintf(stderr,"K1_DISMEMBER_CONTACT_WEAPON_%s dual=2 mixed=2 unpowered=1 mismatch=1 restored=1\n",cuts?"PASS":"FAIL");
            if(!cuts){fail("dismember_contact_weapon");return;}
            if(!hudOwner){fail("hud_owner");return;}
            UpdatePoiseHud(hudOwner);
            if(!hudOwner->m_poiseOffhand.m_tail.bVisible){fail("paired_hud");return;}
        }
        fprintf(stderr,"K1_SANDBOX_DUAL_MENU_PASS step=%d dual=%d staff=%d\n",step,DualMode(),StaffMode());
        Plan();waiting=false;
        if(++step<6){phase=0;ticks=0;return;}
        send("key:39\n");phase=2;ticks=0;return;
    }
    if(phase==2) {
        if(!sandboxPanel){if(ticks>120)fail("spawn_menu");return;}
        sandboxPanel->SelectCustom();
        if(sandboxAi)button(K1SandboxPanel::Ai);
        button(K1SandboxPanel::StanceRight);button(K1SandboxPanel::StanceRight);
        if(sandboxPanel->stance!=2){fail("dual_selector");return;}
        button(K1SandboxPanel::Spawn);hostileId=lastSandboxSpawn;hostile=server->GetCreatureByGameObjectID(hostileId);
        const Vector facing=player->m_orientation;player->SetOrientation(Vector(-facing.y,facing.x,0));
        button(K1SandboxPanel::Team);button(K1SandboxPanel::Spawn);friendlyId=lastSandboxSpawn;friendly=server->GetCreatureByGameObjectID(friendlyId);
        player->SetOrientation(facing);
        if(!hostile || !friendly || hostile==friendly || !DualNpc(hostile) || !DualNpc(friendly) ||
           friendly->GetReputation(player->m_objectId,0)<90 || hostile->GetReputation(friendlyId,0)>10){fail("dual_spawn_factions");return;}
        for(auto* actor:{hostile,friendly}) {
            auto* right=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
            auto* left=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON);
            if(!right || !left || right==left){fail("npc_native_weapons");return;}
        }
        button(K1SandboxPanel::Close);phase=3;ticks=0;return;
    }
    if(phase==3 && ticks>=150) {
        if(sandboxPanel || IsGamePaused() || !npcPlayback.empty() || hostile->m_actionList.m_pInternal->m_nCount || friendly->m_actionList.m_pInternal->m_nCount){fail("dual_idle_ai_off");return;}
        hostile->SetPosition(Vector(200,100,0),1,1,0);friendly->SetPosition(Vector(201.2f,100,0),1,1,0);
        player->SetPosition(Vector(207,100,0),1,1,0);client->SetPrecalculatedPosition(player->m_position);
        for(auto* actor:{hostile,friendly}){actor->m_hitPoints=10000;actor->SetCurrentHitPoints(10000);sandboxForces[actor->m_objectId].next=now+60000000;}
        send("key:39\n");phase=4;ticks=0;return;
    }
    if(phase==4) {
        if(!sandboxPanel){if(ticks>120)fail("combat_menu");return;}
        button(K1SandboxPanel::Ai);button(K1SandboxPanel::Close);phase=5;ticks=0;return;
    }
    if(phase==5) {
        if(!startedCombat && K1EngagePlanning()){send("key:44\n");startedCombat=true;return;}
        if(!startedCombat || IsGamePaused()){if(ticks>180)fail("combat_unpause");return;}
        auto a=npcPlayback.find(hostileId),b=npcPlayback.find(friendlyId);
        bool owned=a!=npcPlayback.end() && b!=npcPlayback.end() && a->second.requested && b->second.requested && a->second.dual && b->second.dual;
        if(owned)for(auto* actor:{hostile,friendly}) {
            auto* body=actor->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
            K1SaberContact::Blade right,left;
            if(!body || _stricmp(body->m_pModel->m_szName,"k1jaddual") || !DualBlade(actor->GetClientCreature(),0,right) || !DualBlade(actor->GetClientCreature(),1,left)){fail("dual_npc_pose_or_blades");return;}
            const auto found=npcPlayback.find(actor->m_objectId);
            if(found->second.staffGuard.empty()){fail("dual_npc_native_guard");return;}
            if(ticks%30==0) {
                std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(body,triangles);
                if(triangles.size()<100){fail("dual_npc_skin");return;}
                for(const auto& face:triangles)if(!K1SaberContact::Finite(face.a) || magnitude(face.a-actor->m_position)>3.f){fail("dual_npc_skin_bounds");return;}
            }
        }
        if(!aiVerified && owned && a->second.serial>0 && b->second.serial>0) {
            if(clashAiPhase==0) {
                if(!ClashNpcPairProbe(hostile,friendly)){fail("clash_npc_pair");return;}
                ClearClashEpisode(hostileId);ClearClashEpisode(friendlyId);
                meleePoise[hostileId]=MeleePoise();meleePoise[friendlyId]=MeleePoise();
                ApplyClashPoise(hostile,friendly);
                // Deterministic100% boundary through the actual next AI update;
                // 5/10/25% boundaries are exhaustively checked over all100 rolls.
                for(auto* actor:{hostile,friendly}) {
                    auto& p=meleePoise[actor->m_objectId];p.clashes=20;
                    auto& n=npcPlayback[actor->m_objectId];n.attacking=n.parrying=false;n.bladeValid=false;
                    n.nextAttack=now;n.nextThink=now+4000000;StopNpcMove(n,actor);
                    sandboxForces[actor->m_objectId].next=now+60000000;
                }
                clashAiPhase=1;clashAiAt=now;return;
            }
            if(clashAiPhase==1) {
                const auto& x=meleePoise[hostileId];const auto& y=meleePoise[friendlyId];
                if(x.clashDecision==20 && y.clashDecision==20 && a->second.parrying && b->second.parrying && !a->second.attacking && !b->second.attacking) {
                    fprintf(stderr,"K1_CLASH_NPC_AI_GUARD_PASS chance=100 actors=2 native_tick=1 no_swing=1\n");
                    clashAiPhase=2;clashAiAt=now;
                } else if(now-clashAiAt>1000000)fail("clash_ai_guard");
                return;
            }
            if(clashAiPhase==2) {
                if(meleePoise[hostileId].clashes || meleePoise[friendlyId].clashes) {
                    if(now-clashAiAt>3000000)fail("clash_ai_guard_deadlock");return;
                }
                if(a->second.parrying || b->second.parrying){fail("clash_ai_guard_release");return;}
                fprintf(stderr,"K1_CLASH_NPC_AI_RELEASE_PASS native_tick=1 mutual_guard_timeout=1\n");clashAiPhase=3;
            }
            fprintf(stderr,"K1_SANDBOX_DUAL_AI_PASS a_serial=%d b_serial=%d dual_bodies=2 native_blades=4\n",a->second.serial,b->second.serial);
            // Reciprocal swings may legitimately clash without health damage.
            // Verify native per-hand damage against a non-attacking target.
            friendly->m_bCommandable=0;friendly->m_bPassiveAttackBehaviour=1;
            friendly->ClearAllActions(1);friendly->m_pCombatRound->EndCombatRound(1);
            sandboxActors[hostileId].dualDamageHands=0;aiVerified=true;ticks=0;return;
        }
        const unsigned hands=sandboxActors[hostileId].dualDamageHands;
        if(aiVerified && a!=npcPlayback.end() && a->second.requested && hands==3 && friendly->GetCurrentHitPoints(0)<10000) {
            fprintf(stderr,"K1_SANDBOX_DUAL_COMBAT_PASS a_hands=%u b_hands=%u a_hp=%d b_hp=%d\n",sandboxActors[hostileId].dualDamageHands,sandboxActors[friendlyId].dualDamageHands,hostile->GetCurrentHitPoints(0),friendly->GetCurrentHitPoints(0));
            auto* round=hostile->m_pCombatRound;
            if(!ParryAttackReady(hostileId,now) || PoiseBroken(hostile) || !round || round->m_bRoundStarted || round->m_bSpellCastRound)return;
            // Two contacts in the same scene share the native attack scratch
            // slot. Verify two independent native effects, plus deduplication.
            auto& playback=a->second;playback.attacking=true;++playback.serial;
            MeleeContact right={hostileId,friendlyId,playback.serial};right.blade=0;
            right.dualWeapon=hostile->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON)->m_objectId;
            auto left=right;left.blade=1;left.offhand=true;
            left.dualWeapon=hostile->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON)->m_objectId;
            const auto count=meleeDamageContacts;queueHP=friendly->GetCurrentHitPoints(0);
            fprintf(stderr,"K1_NPC_DUAL_QUEUE_BEGIN hp=%d serial=%d\n",queueHP,playback.serial);
            ResolveMeleeContacts({right,left,right,left},now);
            if(meleeDamageContacts-count!=2){fail("dual_npc_same_scene_queue");return;}
            playback.attacking=false;playback.hit=true;playback.contactBlades=3;playback.nextAttack=now+60000000;
            phase=55;ticks=0;return;
        }
        if(ticks>1800)fail("dual_npc_both_hand_damage");return;
    }
    if(phase==55 && ticks>=5) {
        const int loss=queueHP-friendly->GetCurrentHitPoints(0);
        if(loss<=0){fail("dual_npc_queue_health");return;}
        fprintf(stderr,"K1_NPC_DUAL_QUEUE_END health_loss=%d contacts=2 duplicate_input=1\n",loss);
        auto* effect=new CGameEffect(1);effect->m_subType=CSWSEffectListHandler::EFFECT_SETSTATE;
        effect->m_durationTypeBits=CGameEffect::DURATION_TYPE_TEMPORARY;effect->m_fDuration=.35f;
        effect->SetInteger(0,EFFECT_STATE_KNOCKBACK);effect->SetCreator(player->m_objectId);hostile->ApplyEffect(effect,0,1);
        phase=6;ticks=0;return;
    }
    if(phase==6) {
        auto a=npcPlayback.find(hostileId);auto* body=hostile->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(body && !IsSourceBody(body) && NativeStatusAnimation(hostile))sawNative=true;
        if(sawNative && !NativeStatusAnimation(hostile) && a!=npcPlayback.end() && a->second.requested && body && !_stricmp(body->m_pModel->m_szName,"k1jaddual")) {
            auto& force=sandboxForces[hostileId];force.next=now;force.nextSpell=FORCE_POWER_FORCE_PUSH;
            forceCount=force.queued;forceBefore=hostile->m_pStats->m_nCurrentForcePoints;
            phase=60;ticks=0;return;
        }
        if(ticks>300)fail("dual_npc_native_recovery");return;
    }
    if(phase==60) {
        auto* body=hostile->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        const auto& force=sandboxForces[hostileId];auto owned=npcPlayback.find(hostileId);
        if(body && !IsSourceBody(body) && NativeUtilityQueued(hostile))sawCasterNative=true;
        if(sawCasterNative && force.queued>forceCount && !NativeUtilityQueued(hostile) && owned!=npcPlayback.end() && owned->second.requested && body && !_stricmp(body->m_pModel->m_szName,"k1jaddual")) {
            auto* round=hostile->m_pCombatRound;
            fprintf(stderr,"K1_NPC_RETURN_ROUND actor=%llu started=%d spell=%d empty=%d\n",hostileId,round->m_bRoundStarted,round->m_bSpellCastRound,
                !round->m_pRoundActionList || !round->m_pRoundActionList->m_pInternal->m_pHead);
            if(round->m_bRoundStarted || round->m_bSpellCastRound){fail("post_force_stale_round");return;}
            K1SaberContact::Blade right,left;
            if(force.lastSpell!=FORCE_POWER_FORCE_PUSH || hostile->m_pStats->m_nCurrentForcePoints>=forceBefore ||
               !DualBlade(hostile->GetClientCreature(),0,right) || !DualBlade(hostile->GetClientCreature(),1,left)){fail("dual_npc_force_cost_or_grips");return;}
            fprintf(stderr,"K1_SANDBOX_DUAL_FORCE_PASS spell=%d cost=%d native_cast=1 dual_return=1\n",force.lastSpell,forceBefore-hostile->m_pStats->m_nCurrentForcePoints);
            auto& playback=owned->second;playback.attacking=true;++playback.serial;
            MeleeContact rightContact={hostileId,friendlyId,playback.serial};rightContact.blade=0;
            rightContact.dualWeapon=hostile->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON)->m_objectId;
            auto leftContact=rightContact;leftContact.blade=1;leftContact.offhand=true;
            leftContact.dualWeapon=hostile->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON)->m_objectId;
            queueHP=friendly->GetCurrentHitPoints(0);const auto count=meleeDamageContacts;
            fprintf(stderr,"K1_NPC_POST_FORCE_DAMAGE_BEGIN hp=%d\n",queueHP);
            ResolveMeleeContacts({rightContact,leftContact},now);
            if(meleeDamageContacts-count!=2){fail("post_force_melee_rejected");return;}
            playback.attacking=false;playback.hit=true;playback.contactBlades=3;playback.nextAttack=now+60000000;
            phase=61;ticks=0;return;
        }
        if(ticks>900)fail("dual_npc_force_handoff");return;
    }
    if(phase==61 && ticks>=5) {
        const int loss=queueHP-friendly->GetCurrentHitPoints(0);
        if(loss<=0){fail("post_force_health_delayed");return;}
        fprintf(stderr,"K1_NPC_POST_FORCE_DAMAGE_END health_loss=%d frames=5 native_contacts=2\n",loss);
        send("key:39\n");phase=7;ticks=0;return;
    }
    if(phase==7) {
        if(!sandboxPanel){if(ticks>120)fail("cleanup_menu");return;}
        button(K1SandboxPanel::Ai);button(K1SandboxPanel::Close);phase=8;ticks=0;return;
    }
    if(phase==8 && ticks>=150) {
        if(sandboxPanel || IsGamePaused() || sandboxAi || !npcPlayback.empty()){fail("dual_ai_off_cleanup");return;}
        for(auto* actor:{hostile,friendly})if(IsSourceBody(actor->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1))){fail("dual_npc_native_restore");return;}
        fprintf(stderr,"K1_SANDBOX_DUAL_PASS menu_switches=6 npc_dual=2 native_hands=3 native_recovery=1 native_force=1 ai_off_cleanup=1\n");phase=-1;send("quit:0\n");
    }
}
