// Focused changed-contract checks; never replay the whole NPC catalog.
void NpcCombatPolicyProbeTick() {
    static int phase=0,ticks=0;static unsigned long long witness=OBJECT_INVALID;
    static std::string original;static int playerHp=0;
    if(phase<0 || !std::getenv("K1_ENGAGE_NPC_POLICY_TEST"))return;
    auto* player=ServerActor();auto* app=g_pAppManager->m_pClientExoApp;
    auto* server=g_pAppManager->m_pServerExoApp;
    const bool story=std::getenv("K1_ENGAGE_STORY")!=nullptr;
    if(!player || !player->GetArea() || !app->GetModule() || app->m_pInternal->m_bAreaNotReadyToDisplay || (!story && !sandboxReady))return;
    ++ticks;
    auto finish=[&](bool pass,const char* reason){
        fprintf(stderr,"K1_NPC_COMBAT_POLICY_%s reason=%s\n",pass?"PASS":"FAIL",reason);
        if(FILE* f=fopen("npc-policy-input.txt","w")){fputs("quit:0\n",f);fclose(f);}phase=-1;
    };
    if(phase==0) {
        for(int hp:{1,49,50,74,75,1000}) {
            const int cap=hp<50?60:hp<75?80:100,parry=hp<50?10:hp<75?15:20;
            const auto policy=NpcHpPolicy(hp);MeleePoise p;p.capacity=policy.poise;p.percent=p.capacity-1;
            p.wasEligible=true;p.nextTick=1;
            if(policy.poise!=cap || policy.parry!=parry || policy.stance!=0 || AdvancePoiseRegeneration(p,10000000,false,true)!=1 || p.percent!=cap ||
               AdvancePoiseRegeneration(p,20000000,false,true)!=0){finish(false,"tier_or_regeneration_cap");return;}
            p.percent=cap-1;RewardParryPoise(p,10);if(p.percent!=cap){finish(false,"parry_reward_cap");return;}
            p.percent=0;RecoverBrokenPoise(p,20000001);if(p.percent!=25){finish(false,"recovery");return;}
        }
        struct Case {const char* alias;int cap,parry,stance;};
        const Case cases[]={
            {"k1n45d3eedf72d4",80,15,2},{"k1n40897c9408e8",80,15,1},{"k1ne0668c2cdf2c",100,25,2},
            {"k1n8d8b6170eb87",100,35,1},{"k1n8de7837209da",100,30,0},{"k1n795eeb2d530e",100,25,0},
            {"k1n96e8066a4cba",100,20,0},{"k1n7a5c25458f9f",100,35,2},{"k1n81c8e53eacf3",60,10,1},
            {"k1nd0fe2c97b8e7",100,25,0},{"k1n725d1d4fb4e7",60,10,0}};
        C2DA table(CResRef("k1npccombat"),0);if(!table.Load2DArray()){finish(false,"policy_table");return;}
        for(const auto& c:cases) {
            std::unique_ptr<CSWSCreature> actor(new CSWSCreature(OBJECT_INVALID,0));
            if(!actor->LoadFromTemplate(CResRef(c.alias),0)){finish(false,"fixture_load");return;}
            const int hp=actor->GetMaxHitPoints(0),force=actor->GetMaxForcePoints(),feats=actor->m_pStats->m_feats.m_count;
            auto p=NpcCombatPolicy(actor.get());ConfigureNpcPoise(actor.get());
            if(p.poise!=c.cap || p.parry!=c.parry || p.stance!=c.stance || NpcParryPercent(actor.get())!=c.parry ||
               meleePoise[actor->m_objectId].percent!=c.cap){finish(false,"native_override_or_initial_capacity");return;}
            int successes=0;for(int roll=1;roll<=100;++roll)successes+=NpcParryRollPasses(c.parry,roll);
            if(successes!=c.parry){finish(false,"parry_probability");return;}
            const int attempts=meleePoise[actor->m_objectId].parryAttempts;
            const bool first=NpcContactParry(actor.get(),10001,player->m_objectId);
            if(NpcContactParry(actor.get(),10001,player->m_objectId)!=first || meleePoise[actor->m_objectId].parryAttempts!=attempts+1){finish(false,"cached_parry_roll");return;}
            CExoString source,module;
            for(int i=0;i<table.m_nRows;++i){CExoString alias;table.GetCExoStringEntry(i,CExoString("alias"),&alias);if(!_stricmp(alias.CStr(),c.alias)){
                table.GetCExoStringEntry(i,CExoString("source"),&source);table.GetCExoStringEntry(i,CExoString("module"),&module);break;}}
            actor->m_scriptVars.SetString(CExoString("K1_ENGAGE_SOURCE"),source);
            actor->m_scriptVars.SetString(CExoString("K1_ENGAGE_SOURCE_MODULE"),module);
            p=NpcCombatPolicy(actor.get());
            if(p.parry!=c.parry || p.stance!=c.stance || !NativeConfiguredNpc(actor.get())){finish(false,"story_source_identity");return;}
            CResGFF saved;CResStruct top;
            if(!saved.CreateGFFFile(&top,CExoString("UTC "),CExoString("V3.2")) || !actor->SaveCreature(&saved,&top)){finish(false,"native_save");return;}
            std::unique_ptr<CSWSCreature> loaded(new CSWSCreature(OBJECT_INVALID,0));
            if(!loaded->LoadCreature(&saved,&top,1)){finish(false,"native_reload");return;}
            const auto restored=NpcCombatPolicy(loaded.get());
            if(loaded->m_scriptVars.GetString(CExoString("K1_ENGAGE_SOURCE"))!=source ||
               loaded->m_scriptVars.GetString(CExoString("K1_ENGAGE_SOURCE_MODULE"))!=module ||
               restored.poise!=c.cap || restored.parry!=c.parry || restored.stance!=c.stance ||
               loaded->GetMaxHitPoints(0)!=hp || loaded->GetMaxForcePoints()!=force || loaded->m_pStats->m_feats.m_count!=feats){finish(false,"serialized_policy_and_native_stats");return;}
            actor->m_scriptVars.DestroyString(CExoString("K1_ENGAGE_SOURCE"));
            actor->m_scriptVars.DestroyString(CExoString("K1_ENGAGE_SOURCE_MODULE"));
            const auto legacy=NpcCombatPolicy(actor.get(),module.CStr());
            if(!legacy.matched || legacy.parry!=c.parry || legacy.stance!=c.stance){
                auto* r=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);auto* l=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON);
                fprintf(stderr,"K1_NPC_POLICY_LEGACY_DETAIL alias=%s module=%s tag=%s hp=%d level=%d gender=%d cls=%d right=%d left=%d matched=%d parry=%d stance=%d\n",c.alias,module.CStr(),actor->m_sTag.CStr(),actor->GetMaxHitPoints(0),actor->m_pStats->GetLevel(0),actor->m_pStats->m_nGender,actor->m_pStats->GetClass(0),r?int(r->m_baseItem):-1,l?int(l->m_baseItem):-1,legacy.matched,legacy.parry,legacy.stance);
                finish(false,"old_save_loadout_identity");return;
            }
            actor->m_scriptVars.SetString(CExoString("K1_ENGAGE_SOURCE"),source);
            actor->m_scriptVars.SetString(CExoString("K1_ENGAGE_SOURCE_MODULE"),module);
            if((actor->m_appearance.m_headId==68 && (!RedNpc(actor.get()) || StaffNpc(actor.get()) || DualNpc(actor.get()))) ||
               (std::string(c.alias)=="k1n8de7837209da" && (!StaffNpc(actor.get()) || RedNpc(actor.get()) || BlueNpc(actor.get())))){
                finish(false,"compatible_stance_priority");return;
            }
            meleePoise.erase(actor->m_objectId);meleePoise.erase(loaded->m_objectId);
            fprintf(stderr,"K1_NPC_POLICY_CASE_PASS alias=%s cap=%d parry=%d stance=%d serialized=1 native_stats=1\n",c.alias,c.cap,c.parry,c.stance);
        }
        fprintf(stderr,"K1_NPC_POLICY_COMPONENT_PASS boundaries=6 sources=11 serialization=11 legacy=11 module_collision=1\n");
        if(story){phase=10;ticks=0;return;}
        SandboxToggle();phase=1;ticks=0;return;
    }
    if(phase==1 && ticks>=3) {
        if(!sandboxPanel){finish(false,"menu_open");return;}
        auto* p=sandboxPanel;p->SelectFaction(3);
        int type=-1;for(int i=0;i<int(p->browser[3].types.size());++i)if(p->browser[3].types[i].label=="Dark Jedi (Lightsaber)")type=i;
        if(type<0){finish(false,"menu_type");return;}p->SelectType(type);p->variant=2;p->UpdatePreview();
        if(p->combatStance!=2){finish(false,"menu_blue_default");return;}
        p->variant=3;p->UpdatePreview();if(p->combatStance!=1){finish(false,"menu_red_default");return;}
        p->Click(&p->buttons[K1SandboxPanel::CombatRight]);
        p->Click(&p->buttons[K1SandboxPanel::Team]);
        if(p->combatStance!=2){finish(false,"manual_stance_alignment");return;}
        SandboxToggle();phase=2;ticks=0;return;
    }
    if(phase==2 && ticks>=3){SandboxToggle();phase=3;ticks=0;return;}
    if(phase==3 && ticks>=3){
        if(!sandboxPanel || sandboxPanel->variant!=3 || sandboxPanel->combatStance!=2){finish(false,"menu_memory");return;}
        auto* panel=sandboxPanel;panel->Click(&panel->buttons[K1SandboxPanel::Spawn]);
        auto* spawned=server->GetCreatureByGameObjectID(lastSandboxSpawn);
        if(!spawned || !NativeSandboxActor(spawned) || !BlueNpc(spawned) || meleePoise[spawned->m_objectId].capacity!=80 || NpcParryPercent(spawned)!=15){finish(false,"manual_stance_spawn");return;}
        const auto id=spawned->m_objectId;
        sandboxActors.erase(id);sandboxNativeActors.erase(id);sandboxStances.erase(id);sandboxAppearances.erase(id);sandboxForces.erase(id);meleePoise.erase(id);
        spawned->RemoveFromArea(0);delete spawned;
        fprintf(stderr,"K1_NPC_POLICY_MENU_PASS defaults=blue,red manual_blue=1 alignment=1 reopen=1 spawn=1 cap=80 parry=15\n");
        SandboxToggle();finish(true,"tiers_overrides_serialization_menu");return;
    }
    if(phase==10) {
        if(K1EngageArenaLoadoutActive() || !NpcCombatAreaActive() || sandboxSession){finish(false,"story_not_arena");return;}
        if(!EquipFixtureSaber(player) || !K1EngageEnter()){if(ticks<60)return;finish(false,"story_engage");return;}
        playerHp=player->GetCurrentHitPoints(0);
        auto* actor=new CSWSCreature(OBJECT_INVALID,0);
        if(!actor->LoadFromTemplate(CResRef("k1n81c8e53eacf3"),0)){delete actor;finish(false,"story_witness");return;}
        actor->m_scriptVars.SetString(CExoString("K1_ENGAGE_SOURCE"),CExoString("c_gammorean"));
        actor->m_scriptVars.SetString(CExoString("K1_ENGAGE_SOURCE_MODULE"),CExoString("global"));
        for(int i=0;i<14;++i)actor->m_scripts[i]=""; // Suppress only this throwaway witness's scripts.
        Vector forward=normalize(player->m_orientation);Vector spawn=player->m_position+forward*2.f;
        auto* area=static_cast<CSWSArea*>(player->GetArea());
        if(!area->PositionWalkable(spawn)){delete actor;finish(false,"story_witness_space");return;}
        actor->m_bCommandable=1;actor->m_bPassiveAttackBehaviour=0;
        actor->SetOrientation(forward*-1.f);actor->AddToArea(area,spawn.x,spawn.y,spawn.z,1,1);
        server->GetFactionManager()->GetFaction(1)->AddMember(actor->m_objectId,0);
        actor->AddToVisibleList(player->m_objectId,1,0,0,0);player->AddToVisibleList(actor->m_objectId,1,0,0,0);
        witness=actor->m_objectId;phase=11;ticks=0;return;
    }
    if(phase==11) {
        auto* actor=server->GetCreatureByGameObjectID(witness);auto it=npcPlayback.find(witness);
        if(actor && it!=npcPlayback.end() && it->second.requested && it->second.red && it->second.serial>=1 && it->second.bladeSamples>0) {
            if(!K1EngageEnemyAIActive(player->m_objectId) || sandboxNativeActors.count(witness) || NpcParryPercent(actor)!=10 || meleePoise[witness].capacity!=60){finish(false,"story_live_policy");return;}
            original=it->second.nativeResource;
            actor->m_bPassiveAttackBehaviour=1;actor->m_bCommandable=0;actor->ClearAllActions(1);
            phase=12;ticks=0;return;
        }
        if(ticks>900){finish(false,"story_native_admission");return;}
    }
    if(phase==12 && ticks>=3) {
        auto* actor=server->GetCreatureByGameObjectID(witness);auto* client=actor?actor->GetClientCreature():nullptr;
        auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
        if(!actor || !body || !body->m_pModel || _stricmp(body->m_pModel->m_szName,original.c_str()) || npcPlayback.count(witness)){
            finish(false,"story_native_restore");return;
        }
        meleePoise.erase(witness);actor->RemoveFromArea(0);delete actor;
        fprintf(stderr,"K1_STORY_COMBAT_PASS module=%s native_source=1 red_stance=1 cap=60 parry=10 swing=1 restore=1 no_arena_loadout=1\n",app->GetModule()->m_sModuleName.CStr());
        finish(true,"story_native_combat");
    }
}
