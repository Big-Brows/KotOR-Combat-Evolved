// Focused native damage/trigger integration; no roster replay.
void GrenadeMineProbeTick() {
    static int stage=0,ticks=0,trial=0,hp=0,beforeCount=0;
    static unsigned long long targetId=OBJECT_INVALID,mineId=OBJECT_INVALID,itemId=OBJECT_INVALID;
    static Vector origin;
    if(stage<0 || !sandboxReady)return;
    ++ticks;auto* server=g_pAppManager->m_pServerExoApp;auto* player=ServerActor();
    auto quit=[&](bool ok,const char* why){fprintf(stderr,"K1_GRENADE_MINE_%s reason=%s trials=%d\n",ok?"PASS":"FAIL",why,trial);stage=-1;if(FILE* f=fopen("grenade-mine-input.txt","w")){fputs("quit:0\n",f);fclose(f);}};
    if(!player){quit(false,"player");return;}
    auto* area=static_cast<CSWSArea*>(player->GetArea());
    if(stage==0) {
        SandboxSetAi(false);CExoString message;auto* target=SandboxSpawn(0,false,message);
        if(!target){quit(false,"spawn");return;}targetId=target->m_objectId;
        origin=player->m_position;stage=1;ticks=0;return;
    }
    auto* target=server->GetCreatureByGameObjectID(targetId);
    if(!target){quit(false,"target");return;}
    if(ticks>1800){quit(false,"timeout");return;}
    if(stage==1) {
        if(ticks<5 || !target->GetClientCreature())return;
        target->m_hitPoints=1000;target->SetCurrentHitPoints(1000);player->m_hitPoints=1000;player->SetCurrentHitPoints(1000);
        target->SetPosition(origin+Vector(4,0,0),1,1,0);target->GetClientCreature()->SetPrecalculatedPosition(target->m_position);
        auto* item=new CSWSItem(OBJECT_INVALID);
        if(!item->LoadFromTemplate(CResRef("g_w_fraggren01")) || !player->AcquireItem(&item,OBJECT_INVALID,OBJECT_INVALID,1)){quit(false,"grenade_item");return;}
        item->m_flags&=~4;itemId=item->m_objectId;beforeCount=item->m_stackCount;hp=target->GetCurrentHitPoints(0);
        if(!K1EngageEnter() || !player->UseItem(itemId,0,0,targetId,target->m_position,0,0)){quit(false,"grenade_queue");return;}
        stage=2;ticks=0;return;
    }
    if(stage==2) {
        if(target->GetCurrentHitPoints(0)>=hp)return;
        if(NativeUtilityQueued(player))return;
        auto* remaining=server->GetItemByGameObjectID(itemId);
        if(remaining && remaining->m_stackCount>=beforeCount){quit(false,"grenade_consumption");return;}
        fprintf(stderr,"K1_GRENADE_NATIVE_DAMAGE_PASS damage=%d consumed=1\n",hp-target->GetCurrentHitPoints(0));
        player->SetPosition(origin+Vector(0,-12,0),1,1,0);player->GetClientCreature()->SetPrecalculatedPosition(player->m_position);
        SandboxSetAi(true);K1EngageEnter();stage=3;ticks=0;return;
    }
    if(stage==3) {
        // Let delayed native explosion effects from the preceding trial expire.
        auto owned=npcPlayback.find(targetId);
        if(owned==npcPlayback.end() || !owned->second.owner)return;
        auto& npc=owned->second;StopNpcMove(npc,target);
        npc.nextThink=npc.nextAttack=server->GetWorldTimer()->GetSnapshotTime()+600000000;
        if(ticks<180)return;
        const Vector start=origin+Vector(-3,0,0);
        target->SetPosition(start,1,1,0);target->GetClientCreature()->SetPrecalculatedPosition(start);
        target->m_currentSubareaIds.m_count=0;npc.position=npc.previousPhysics=start;npc.physicsRemainder=0;
        target->SetCurrentHitPoints(1000);hp=target->GetCurrentHitPoints(0);
        auto* mine=new CSWSTrigger(OBJECT_INVALID);mine->SetCreator(player->m_objectId);
        mine->m_trapType=3;mine->m_bTrap=1;mine->m_bTrapOneShot=1;mine->m_nFaction=player->m_pStats->m_factionId;
        g_pRules->m_p2DArrays->m_pTraps->GetCExoStringEntry(3,CExoString("TrapScript"),&mine->m_scriptSlots[4]);
        mine->m_scriptSlots[1]=CExoString("");mine->m_scriptSlots[0]=CExoString("");
        mine->SetPosition(origin,1,1,0);mine->CreateNewGeometry(1.f,origin,area);mine->AddToArea(area,origin.x,origin.y,origin.z,1);area->m_mineObjects.Add(mine->m_objectId);mineId=mine->m_objectId;
        if(trial==3){mine->SetCreator(targetId);mine->m_nFaction=target->m_pStats->m_factionId;}
        if((mine->GetCanFireMineOnObject(targetId,0)!=0)!=(trial!=3)){quit(false,"faction_policy");return;}
        sandboxForces[targetId].next=server->GetWorldTimer()->GetSnapshotTime()+600000000;
        npc.nextThink=npc.nextAttack=server->GetWorldTimer()->GetSnapshotTime()+600000000;
        npc.nextPath=0;NpcMoveGoal(npc,target,origin+Vector(3,0,0),trial==0,server->GetWorldTimer()->GetSnapshotTime());
        if(trial==2) {
            // Both endpoints outside: exercise crossed-list handling on a large native move.
            const Vector end=origin+Vector(3,0,0);target->SetPosition(end,1,1,0);
            // Return value reports membership change, not crossed events queued.
            target->UpdateSubareasOnMoveTo(start,end,0,nullptr,0);
            StopNpcMove(npc,target);
        }
        fprintf(stderr,"K1_MINE_TEST_START trial=%d mine=%llu start=%g,%g goal=%g,%g active=%d\n",trial,mineId,start.x,start.y,npc.goal.x,npc.goal.y,npc.hasMove);
        stage=4;ticks=0;return;
    }
    if(stage==4) {
        if(!K1EngageEngaged())K1EngageEnter(); // Native mine-detected auto-pause is preserved.
        auto owned=npcPlayback.find(targetId);
        if(owned==npcPlayback.end()){quit(false,"movement_owner");return;}
        auto& npc=owned->second;const auto now=server->GetWorldTimer()->GetSnapshotTime();
        npc.nextThink=npc.nextAttack=now+600000000;
        auto* mine=server->GetGameObject(mineId);
        if(ticks%120==0)fprintf(stderr,"K1_MINE_TEST_MOVE trial=%d position=%g,%g mine=%d active=%d goal=%g,%g\n",trial,target->m_position.x,target->m_position.y,mine!=nullptr,npc.hasMove,npc.goal.x,npc.goal.y);
        if(trial<3) {
            if(mine)return;
            if(target->GetCurrentHitPoints(0)>=hp){if(ticks<15)return;quit(false,"mine_damage");return;}
        } else {
            if(target->m_position.x<origin.x+2.f)return;
            if(!mine || target->GetCurrentHitPoints(0)!=hp){fprintf(stderr,"K1_MINE_FRIENDLY_STATE exists=%d hp=%d baseline=%d\n",mine!=nullptr,target->GetCurrentHitPoints(0),hp);quit(false,"friendly_mine");return;}
            auto* trap=mine->AsSWSTrigger();trap->RemoveFromArea();delete trap;
        }
        fprintf(stderr,"K1_MINE_CROSSING_PASS trial=%d walk=%d running=%d crossed_only=%d friendly=%d damage=%d removed=%d\n",trial,trial==0,target->m_bRunning,trial==2,trial==3,hp-target->GetCurrentHitPoints(0),trial!=3);
        StopNpcMove(npc,target);++trial;
        if(trial==4){SandboxSetAi(false);quit(true,"native_grenade_walk_run_crossed_friendly");return;}
        stage=3;ticks=0;
    }
}
