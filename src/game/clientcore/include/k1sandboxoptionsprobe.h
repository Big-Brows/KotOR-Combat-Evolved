// Installed-data checks: native lethal damage, appearance/stat separation,
// saber colour replication and compatible stance admission. No saves.
int SandboxOptionsProbe(K1SandboxPanel* panel) {
    static int index=0,style=0,ticks=0;static bool initialized=false;
    static std::vector<std::pair<int,int>> protectedSources;
    static unsigned long long live=OBJECT_INVALID;
    auto* server=g_pAppManager->m_pServerExoApp;auto* player=ServerActor();
    auto cleanup=[&](CSWSCreature* actor){const auto id=actor->m_objectId;sandboxActors.erase(id);sandboxNativeActors.erase(id);sandboxForces.erase(id);sandboxStances.erase(id);sandboxAppearances.erase(id);npcPlayback.erase(id);meleePoise.erase(id);actor->RemoveFromArea(0);delete actor;live=OBJECT_INVALID;ticks=0;};
    if(!initialized) {
        initialized=true;
        panel->SelectFaction(13);
        if(panel->browser[13].types.empty())return -1;
        panel->SelectType(0);panel->SetActiveControl(&panel->buttons[K1SandboxPanel::Spawn],0);
        const int originalType=panel->type;
        for(bool friendly:{true,false,true,false}) {
            if(panel->friendly!=friendly)panel->Click(&panel->buttons[K1SandboxPanel::Team]);
            auto* effect=static_cast<Gob*>(panel->preview.view.m_scene.GetModel(0));
            float time=0,length=0;
            if(panel->type!=originalType || !effect || !effect->GetAnimation(const_cast<char*>(friendly?"good":"evil"),&time,&length))return -1;
            bool valid=true;int retained=0,removed=0;
            auto check=[&](auto&& self,Part* part)->void {
                if(!part)return;
                if(auto* emitter=part->AsPartEmitter()) {
                    const bool listed=static_cast<Scene*>(panel->preview.view.m_scene.m_pScene)->m_emitters.IndexOf(emitter)>=0;
                    if(!_stricmp(part->Name(),friendly?"Good_stream":"Evil_stream")){valid &= listed;++retained;}
                    if(!_stricmp(part->Name(),"Sparkles") || !_stricmp(part->Name(),"Sparks")){valid &= !listed;++removed;}
                    if(!friendly && !_stricmp(part->Name(),"gas_evil"))valid &= !listed;
                }
                for(int i=0;i<part->m_nChildren;++i)self(self,part->m_paChildren[i]);
            };check(check,effect->m_pPart);
            if(!valid || retained!=1 || removed!=2)return -1;
        }
        fprintf(stderr,"K1_SANDBOX_LIVE_ALIGNMENT_PASS toggles=4 same_character=1 red_blue_glow=1 no_sparkles=1 no_dark_gas=1\n");
        auto* manager=panel->m_pGuiManager;
        const auto saved=manager->m_currentMousePos;
        manager->HandleMouseMove(panel->m_extent.x+panel->statsLabel.m_extent.x+10,panel->m_extent.y+panel->statsLabel.m_extent.y+10);
        manager->HandleLMouseDown(0);manager->HandleLMouseUp();
        if(!SandboxClickListRow(panel,panel->factions,4) || panel->faction!=4 || panel->SelectedType() || panel->preview.view.m_tail.bVisible)return -1;
        if(!SandboxClickListRow(panel,panel->types,0) || panel->type!=0)return -1;
        panel->Refresh();auto* factionRow=panel->factions.GetControl(4)->AsButton();auto* typeRow=panel->types.GetControl(0)->AsButton();
        if(!factionRow || !typeRow || factionRow->m_text.m_params.m_color.y!=.85f || typeRow->m_text.m_params.m_color.y!=.85f)return -1;
        manager->HandleMouseMove(saved.x,saved.y);
        fprintf(stderr,"K1_SANDBOX_DIRECT_SWITCH_PASS from_sith_stats=1 to_genoharadan=1 highlight_yellow=1\n");
        for(int g=0;g<int(sandboxNativeGroups.size());++g)for(int v=0;v<int(sandboxNativeGroups[g].variants.size());++v) {
            CSWSCreature source(OBJECT_INVALID,0);
            if(source.LoadFromTemplate(CResRef(sandboxNativeGroups[g].variants[v].creature.c_str()),0) && (source.m_bPlot || source.m_bMin1HP))protectedSources.push_back({g,v});
        }
    }
    if(index<int(protectedSources.size())) {
        if(live==OBJECT_INVALID) {
            auto selected=protectedSources[index];CExoString message;
            auto* actor=SandboxSpawn(0,false,message,selected.first,selected.second,true);if(!actor)return -1;
            if(actor->m_bPlot || actor->m_bMin1HP || !actor->m_bDestroyable){cleanup(actor);return -1;}
            live=actor->m_objectId;
            auto* damage=new CGameEffect(1);damage->m_subType=static_cast<unsigned short>(CSWSEffectListHandler::EFFECT_DAMAGE);
            damage->m_durationTypeBits=CGameEffect::DURATION_TYPE_INSTANTANEOUS;damage->SetCreator(player->m_objectId);damage->SetNumIntegers(22);
            for(int i=0;i<15;++i)damage->SetInteger(i,-1);
            damage->SetInteger(7,5000);damage->SetInteger(14,5000);damage->SetInteger(19,1);damage->SetInteger(20,1);damage->SetInteger(21,1);
            actor->ApplyEffect(damage,0,0);return 0;
        }
        auto* actor=server->GetCreatureByGameObjectID(live);if(!actor)return -1;
        if(actor->GetCurrentHitPoints(0)>0 || (!actor->GetDead() && !actor->GetDeadTemp())){if(++ticks<30)return 0;cleanup(actor);return -1;}
        cleanup(actor);++index;return 0;
    }
    if(style<7) {
        int faction=-1,type=-1;
        const char* wanted=style==5?"Dark Jedi (Double-bladed Lightsaber)":style==6?"Dark Jedi (Lightsaber + Short Lightsaber)":"Dark Jedi (Lightsaber)";
        const int selectedStyle=style<5?style%3:0,color=style%5+1;
        for(int f=0;f<int(panel->browser.size());++f)for(int t=0;t<int(panel->browser[f].types.size());++t)
            if(panel->browser[f].types[t].label==wanted){faction=f;type=t;}
        if(faction<0)return -1;
        auto& choice=panel->browser[faction].types[type];const auto& variant=choice.variants.front();
        const int appearance=variant.looks.back();
        if(live==OBJECT_INVALID) {
            CExoString message;auto* actor=SandboxSpawn(0,false,message,choice.group,variant.source,true,&sandboxNativeGroups[choice.group].variants[appearance],color,selectedStyle);
            if(!actor)return -1;live=actor->m_objectId;ticks=0;return 0;
        }
        auto* actor=server->GetCreatureByGameObjectID(live);auto* client=actor?actor->GetClientCreature():nullptr;
        auto* item=actor?NativeNpcRightItem(actor):nullptr;auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
        if(!actor || !item)return -1;
        if(!body || !body->m_pModel || !g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(client->m_aEquippedItemIds[4])){if(++ticks<180)return 0;cleanup(actor);return -1;}
        CSWSCreature expected(OBJECT_INVALID,0);expected.LoadFromTemplate(CResRef(sandboxNativeGroups[choice.group].variants[variant.source].creature.c_str()),0);
        CSWSCreature look(OBJECT_INVALID,0);look.LoadFromTemplate(CResRef(sandboxNativeGroups[choice.group].variants[appearance].creature.c_str()),0);
        auto* visual=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(client->m_aEquippedItemIds[4]);
        const char* rig=NpcRigResource(body->m_pModel->m_szName,actor);
        bool leftColor=true;
        if(auto* left=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON))if(SaberType(left->m_baseItem) && left!=item) {
            auto* leftVisual=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(client->m_aEquippedItemIds[5]);
            leftColor=leftVisual && left->m_nModelVariation==color && leftVisual->m_nModelVariation==color;
        }
        const bool ok=item->m_nModelVariation==color && visual->m_nModelVariation==color && leftColor &&
            actor->GetCurrentHitPoints(0)==expected.GetCurrentHitPoints(0) && actor->m_pStats->GetLevel(0)==expected.m_pStats->GetLevel(0) &&
            actor->m_pStats->m_nCurrentForcePoints==expected.m_pStats->m_nCurrentForcePoints && actor->m_pStats->m_feats.m_count==expected.m_pStats->m_feats.m_count &&
            actor->GetFirstName()==expected.GetFirstName() && actor->m_appearance.m_headId==look.m_appearance.m_headId &&
            client->m_pCreatureAppearance->m_nAppearanceId==look.m_appearance.m_headId && rig && NativeNpcAssets(rig) &&
            RedNpc(actor)==(selectedStyle==1) && BlueNpc(actor)==(selectedStyle==2);
        if(!ok){fprintf(stderr,"K1_SANDBOX_OPTIONS_FAIL style=%d color=%d client_color=%d rig=%s\n",style,item->m_nModelVariation,visual->m_nModelVariation,rig?rig:"none");cleanup(actor);return -1;}
        cleanup(actor);++style;return 0;
    }
    fprintf(stderr,"K1_SANDBOX_OPTIONS_PASS mortality=%zu colors=5 stances=3 weapon_styles=3 native_stats=1 appearance=1 original_target_names=1\n",protectedSources.size());return 1;
}
