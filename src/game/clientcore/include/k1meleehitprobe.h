bool MovingTargetContactProbe(CSWSCreature* target) {
    auto* body=target->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
    std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(body,triangles);
    const Vector root=body->m_vPosition;
    const K1SaberContact::Blade blade={root+Vector(-.6f,-.6f,1.f),root+Vector(.6f,-.6f,1.f)};
    auto hits=[&](const K1SaberContact::Blade& a,const K1SaberContact::Blade& b,float lo=0.f,float hi=1.f){
        auto first=K1SaberContact::Between(a,b,lo),last=K1SaberContact::Between(a,b,hi);const int n=K1SaberContact::Steps(first,last);
        for(int i=0;i<n;++i)for(const auto& t:triangles)
            if(K1SaberContact::SweepCell(K1SaberContact::Between(first,last,float(i)/n),K1SaberContact::Between(first,last,float(i+1)/n),t))return true;
        return false;
    };
    const auto id=target->m_objectId;
    auto savedOld=contactBodyRoots,savedNow=contactCurrentRoots;
    ContactBodyRoot current={body,body->m_pModel,body->m_pScene,root,body->m_qOrientation,body->m_fObjectScale},prior=current;
    prior.position=prior.position-Vector(0,1.2f,0);contactBodyRoots[id]=prior;contactCurrentRoots[id]=current;
    const auto relative=RelativeContactStart(id,blade);
    bool ok=!hits(blade,blade) && hits(relative,blade) && hits(relative,blade,.4f,.6f) && !hits(relative,blade,0.f,.01f);
    ok=ok && !hits(RelativeContactStart(id,blade,false),blade);
    prior.body=nullptr;contactBodyRoots[id]=prior;ok=ok && !hits(RelativeContactStart(id,blade),blade);
    prior=current;prior.position=root-Vector(0,3.f,0);contactBodyRoots[id]=prior;ok=ok && !hits(RelativeContactStart(id,blade),blade);
    prior=current;contactBodyRoots[id]=prior;const auto still=RelativeContactStart(id,blade);ok=ok && magnitude(still.base-blade.base)<1e-8f;
    const K1SaberContact::Blade axis={{1,0,0},{2,0,0}};
    const auto turned=K1SaberContact::TargetRelative(axis,Vector(),Quaternion(),Vector(),Quaternion(Vector(0,0,1),1.5707963268f));
    ok=ok && magnitude(turned.tip-Vector(0,2,0))<.00001f;
    contactBodyRoots=std::move(savedOld);contactCurrentRoots=std::move(savedNow);
    fprintf(stderr,"K1_MOVING_TARGET_CONTACT_%s triangles=%zu old_sweep_missed=1 relative_hit=1 clipped_window=1 first_frame=1 model_reset=1 teleport=1 stationary=1 rotation=1\n",ok?"PASS":"FAIL",triangles.size());
    return ok;
}
// Live chained swings against one idle native damage recipient, not a roster replay.
void MeleeHitProbeTick() {
    static int stage=0,ticks=0,style=0,trial=0,firstSerial=0,lastSerial=0,baselineHp=0;
    static unsigned long long targetId=OBJECT_INVALID;
    static int queued=0,rawOnly=0,swings=0;
    static Vector anchor;const bool opponentAi=std::getenv("K1_MELEE_HIT_OPPONENT_AI")!=nullptr;const bool moving=std::getenv("K1_MELEE_HIT_MOVING")!=nullptr;
    const int selections[]={3,2,1};
    const int sequences[][4]={{0,3,5,0},{1,3,5,1},{2,4,6,2},{4,6,0,4},{0,0,0,0},{3,3,3,3},{6,6,6,6}};
    if(stage<0 || !sandboxReady)return;
    ++ticks;
    auto* server=g_pAppManager->m_pServerExoApp;auto* player=ServerActor();auto* client=Actor();
    auto quit=[&](bool ok,const char* why){fprintf(stderr,"K1_MELEE_HIT_%s reason=%s swings=%d queued=%d raw_only=%d\n",ok?"PASS":"FAIL",why,swings,queued,rawOnly);stage=-1;if(FILE* f=fopen("melee-hit-input.txt","w")){fputs("quit:0\n",f);fclose(f);}};
    if(!player || !client){quit(false,"player");return;}
    if(stage==0) {
        SandboxSetAi(false);SandboxToggle();stage=1;ticks=0;return;
    }
    if(stage==1 && ticks<3)return;
    if(stage==1 && ticks>=3) {
        if(!sandboxPanel){quit(false,"stats_panel");return;}
        struct UiCase {int faction;const char* label;int variant,cap,parry;};
        for(const auto& c:{UiCase{3,"Dark Jedi (Lightsaber)",2,80,15},UiCase{13,"Sith Lord (Lightsaber)",0,100,35},UiCase{17,"Gamorrean (Gamorrean Battleaxe)",0,60,10}}) {
            auto* p=sandboxPanel;p->SelectFaction(c.faction);int index=-1;
            for(int i=0;i<int(p->browser[c.faction].types.size());++i)if(p->browser[c.faction].types[i].label==c.label)index=i;
            if(index<0){quit(false,"stats_type");return;}p->SelectType(index);p->variant=c.variant;p->UpdatePreview();p->Refresh();
            char cap[48],parry[48];snprintf(cap,sizeof(cap),"Max Poise: %d%%",c.cap);snprintf(parry,sizeof(parry),"Parry Chance: %d%%",c.parry);
            const char* text=p->statsLabel.m_text.m_params.m_sText.CStr();
            if(!strstr(text,cap) || !strstr(text,parry) || p->statsLabel.m_text.GetIdealHeight()>p->statsLabel.m_extent.h){quit(false,"stats_values_or_layout");return;}
        }
        fprintf(stderr,"K1_SANDBOX_COMBAT_STATS_PASS tiers=3 override=35 text_fits=1\n");
        SandboxToggle();CExoString message;auto* target=SandboxSpawn(0,false,message);
        if(!target){quit(false,"target_spawn");return;}targetId=target->m_objectId;
        target->m_hitPoints=1000;target->SetCurrentHitPoints(1000);
        sandboxForces[targetId].next=server->GetWorldTimer()->GetSnapshotTime()+600000000;
        if(opponentAi)SandboxSetAi(true);
        redSelection=selections[style];if(!K1EngageEnter()){quit(false,"enter");return;}
        anchor=player->m_position;stage=2;ticks=0;return;
    }
    auto* target=server->GetCreatureByGameObjectID(targetId);
    if(!target){quit(false,"target");return;}
    if(ticks>1800){quit(false,"timeout");return;}
    if(stage==2) {
        auto* targetClient=target->GetClientCreature();
        if(ticks<5 || !targetClient || targetClient->GetServerCreature()!=target || !targetClient->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1) || state.attacking)return;
        if(trial==0 && style==0 && target->GetClientCreature() && target->GetClientCreature()->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1)) {
            static bool checked=false;
            if(!checked){if(!MovingTargetContactProbe(target)){quit(false,"moving_target_regression");return;}checked=true;}
        }

        keyW=keyS=keyA=keyD=keyShift=false;movement.Clear();state.leftHeld=state.rightHeld=state.holdChain=false;
        player->SetOrientation(Vector(1,0,0));client->SetOrientation(Vector(1,0,0));aimYaw=aimPitch=legsYaw=0;
        target->SetPosition(player->m_position+Vector(1.f,0,0),1,1,0);target->GetClientCreature()->SetPrecalculatedPosition(target->m_position);
        target->m_hitPoints=1000;target->SetCurrentHitPoints(1000);player->m_hitPoints=1000;player->SetCurrentHitPoints(1000);baselineHp=target->GetCurrentHitPoints(0);
        firstSerial=attackSerial+1;lastSerial=firstSerial+std::min(4,PlayerChainLimit())-1;
        keyW=moving;
        requestedDirection=sequences[trial][0];requestedBackLeft=false;state.attackRequested=true;
        stage=3;ticks=0;return;
    }
    if(stage==3) {
        if(moving){player->SetPosition(anchor,1,1,0);client->SetPrecalculatedPosition(anchor);target->SetPosition(anchor+Vector(1.f,0,0),1,1,0);target->GetClientCreature()->SetPrecalculatedPosition(target->m_position);}
        if(state.attacking && currentOverlay) {
            if(!LiveMeleeRate(loadedBody,currentOverlay,PlayerMeleeRate())){quit(false,"live_playback_rate");return;}
            const int index=attackSerial-firstSerial;
            if(index>=0 && attackSerial<lastSerial){requestedDirection=sequences[trial][index+1];requestedBackLeft=false;state.attackRequested=true;}
        }
        if(opponentAi && !state.attacking && attackSerial<lastSerial){requestedDirection=sequences[trial][std::max(0,attackSerial-firstSerial+1)];state.attackRequested=true;}
        if(attackSerial<lastSerial || state.attacking)return;
        int count=0;
        for(int serial=firstSerial;serial<=lastSerial;++serial) {
            const auto a=playerHitAudits[serial];++swings;queued+=a.queued;count+=a.queued;
            if(a.raw && !a.queued)++rawOnly;
            fprintf(stderr,"K1_MELEE_HIT_SWING style=%d trial=%d serial=%d raw=%d active=%d queued=%d first=%g last=%g rate=%g\n",redSelection,trial,serial,a.raw,a.active,a.queued,a.first,a.last,PlayerMeleeRate());
            fprintf(stderr,"K1_MELEE_HIT_OUTCOME serial=%d damage=%d block=%d clash=%d rejected=%d dropped=%d\n",serial,a.damaged,a.blocked,a.clashed,a.rejected,a.dropped);
            if((!opponentAi && a.queued>1) || a.damaged>1 || (a.active && !a.queued)){quit(false,"once_per_swing");return;}
        }
        const int damage=baselineHp-target->GetCurrentHitPoints(0);
        if(count && damage<=0 && !opponentAi){quit(false,"queued_native_damage");return;}
        fprintf(stderr,"K1_MELEE_HIT_CHAIN style=%d trial=%d swings=%d contacts=%d damage=%d\n",redSelection,trial,lastSerial-firstSerial+1,count,damage);
        if(++trial<7){stage=2;ticks=0;return;}
        trial=0;
        if(++style<3){redSelection=selections[style];stage=2;ticks=0;return;}
        keyW=keyS=keyA=keyD=keyShift=false;movement.Clear();
        SandboxSetAi(false);sandboxActors.erase(targetId);sandboxForces.erase(targetId);meleePoise.erase(targetId);
        target->RemoveFromArea(0);delete target;targetId=OBJECT_INVALID;
        CExoString message;auto* blue=SandboxSpawn(4,false,message);
        if(!blue){quit(false,"blue_npc_spawn");return;}targetId=blue->m_objectId;
        blue->m_hitPoints=1000;blue->SetCurrentHitPoints(1000);player->m_hitPoints=1000;player->SetCurrentHitPoints(1000);
        sandboxForces[targetId].next=server->GetWorldTimer()->GetSnapshotTime()+60000000;
        SandboxSetAi(true);K1EngageMouse(false,true);stage=4;ticks=0;return;
    }
    if(stage==4) {
        if(!K1EngageEngaged())K1EngageEnter();
        auto found=npcPlayback.find(targetId);
        if(found!=npcPlayback.end() && found->second.attacking && found->second.overlay) {
            auto& npc=found->second;
            if(!npc.blue || std::fabs(NpcSwingRate(npc)-1.f)>.00001f || !LiveMeleeRate(npc.body,npc.overlay,1.f)){quit(false,"blue_npc_live_rate");return;}
            fprintf(stderr,"K1_BLUE_NPC_RATE_PASS live_rate=1\n");
            SandboxSetAi(false);sandboxActors.erase(targetId);sandboxForces.erase(targetId);meleePoise.erase(targetId);
            target->RemoveFromArea(0);delete target;targetId=OBJECT_INVALID;
            quit(true,"chain_contact_audit");return;
        }
        if(ticks>900){quit(false,"blue_npc_attack_timeout");return;}
    }
}
