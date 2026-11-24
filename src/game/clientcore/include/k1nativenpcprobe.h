// Opt-in installed-data checks for native sandbox templates and body handoffs.
// No saves, quest scripts, combat stat changes or original-data writes.
bool NativeNpcSkinBindsValid(Gob* body) {
    // Validate the inverse binds actually consumed by renderer/contact skinning.
    // Finite deformed vertices alone do not catch a translated chest.
    std::vector<Part*> joints;
    auto collect=[&](auto&& self,Part* p)->void {
        if(!p || p->m_pNode->m_pTree!=body->m_pModel)return;
        joints.push_back(p);
        for(int i=0;i<p->m_nChildren;++i)self(self,p->m_paChildren[i]);
    };collect(collect,body->m_pPart);
    float maximum=0,angle=0;int checked=0;
    for(auto* mesh:joints)if(mesh->AsPartSkin()) {
        auto* node=static_cast<MdlNodeSkin*>(mesh->m_pNode);
        if(node->m_nBoneCount>int(joints.size()) || node->m_lTBoneRefInv.m_count<node->m_nBoneCount ||
           node->m_lQBoneRefInv.m_count<node->m_nBoneCount)return false;
        Vector meshP;Quaternion meshQ;BindFrame(mesh,meshP,meshQ);
        for(int i=0;i<node->m_nBoneCount;++i) {
            Vector boneP;Quaternion boneQ;BindFrame(joints[i],boneP,boneQ);
            const Vector expected=Rotate(Inverse(boneQ),meshP-boneP);
            maximum=std::max(maximum,magnitude(expected-node->m_lTBoneRefInv.m_pData[i]));
            const Quaternion rotation=normalize(Inverse(boneQ)*meshQ);
            angle=std::max(angle,1.f-std::fabs(dot(rotation,normalize(node->m_lQBoneRefInv.m_pData[i]))));
            ++checked;
        }
    }
    const bool pass=checked>0 && maximum<.0002f && angle<.00001f;
    fprintf(stderr,"K1_NATIVE_NPC_SKIN_BIND_%s model=%s matrices=%d position_error=%g rotation_error=%g\n",
        pass?"PASS":"FAIL",body->m_pModel->m_szName,checked,maximum,angle);
    return pass;
}
int NativeNpcCombatProbeTick() {
    static int phase=0,ticks=0,hp=0;
    static unsigned long long id=OBJECT_INVALID;
    static Vector origin;
    static std::string native;
    ++ticks;
    auto* server=g_pAppManager->m_pServerExoApp;auto* player=ServerActor();
    if(phase==0) {
        int group=-1,variant=-1;
        for(int i=0;i<int(sandboxNativeGroups.size());++i)for(int j=0;j<int(sandboxNativeGroups[i].variants.size());++j) {
            const auto& entry=sandboxNativeGroups[i].variants[j];
            if(entry.module=="kas_m25aa_s" && entry.source=="kas25_mandalor01"){group=i;variant=j;}
        }
        if(group<0)return -1;
        SandboxSetAi(true);if(!K1EngageEnter())return -1;
        CExoString message;auto* actor=SandboxSpawn(0,false,message,group,variant,true);
        if(!actor)return -1;
        id=actor->m_objectId;origin=actor->m_position;hp=player->GetCurrentHitPoints(0);
        phase=1;ticks=0;return 0;
    }
    auto* actor=server->GetCreatureByGameObjectID(id);
    auto found=npcPlayback.find(id);
    if(!actor || player->GetDead())return -1;
    if(found!=npcPlayback.end() && found->second.requested) {
        const auto& npc=found->second;
        native=npc.nativeResource;
        const float moved=magnitude(actor->m_position-origin);
        if(npc.chainLinks>0 && npc.serial>=2 && npc.bladeSamples>0 && moved>.25f &&
           sandboxActors[id].meleeHits>0 && player->GetCurrentHitPoints(0)<hp) {
            fprintf(stderr,"K1_NATIVE_NPC_COMBAT_PASS body=%s moved=%g chains=%d hits=%u player_hp=%d/%d\n",
                native.c_str(),moved,npc.chainLinks,sandboxActors[id].meleeHits,player->GetCurrentHitPoints(0),hp);
            SandboxSetAi(false);
            auto* client=actor->GetClientCreature();auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
            if(!body || !body->m_pModel || _stricmp(body->m_pModel->m_szName,native.c_str()) || npcPlayback.count(id))return -1;
            sandboxActors.erase(id);sandboxNativeActors.erase(id);sandboxForces.erase(id);meleePoise.erase(id);
            actor->RemoveFromArea(0);delete actor;K1EngageTryDisengage();
            return 1;
        }
    }
    if(ticks>1200) {
        fprintf(stderr,"K1_NATIVE_NPC_COMBAT_TIMEOUT actor=%llu owned=%d hp=%d/%d native_state=%d flags=%d/%d\n",
            id,found!=npcPlayback.end(),player->GetCurrentHitPoints(0),hp,actor->m_nAIState,int(actor->m_bCommandable),int(actor->m_bPassiveAttackBehaviour));
        return -1;
    }
    return 0;
}
void NativeNpcProbeTick() {
    static int phase=0,ticks=0,group=0,variant=0,loaded=0,swapped=0,fallback=0;
    static unsigned long long live=OBJECT_INVALID;
    if(phase<0 || !sandboxReady)return;
    ++ticks;
    auto* server=g_pAppManager->m_pServerExoApp;
    auto send=[](const char* text){if(FILE* f=fopen("native-npc-input.txt","w")){fputs(text,f);fclose(f);}};
    auto fail=[&](const char* why){fprintf(stderr,"K1_NATIVE_NPC_FAIL group=%d variant=%d reason=%s\n",group,variant,why);phase=-1;send("quit:0\n");};
    if(phase==0) {
        if(ticks<30)return;
        LoadSandboxNativeCatalog();
        if(sandboxNativeGroups.empty() || nativeNpcRigs.empty()){fail("missing_catalog_or_rigs");return;}
        if(!K1EngageEnter()){fail("player_enter");return;}
        SandboxSetAi(false);
        phase=1;ticks=0;return;
    }
    if(group>=int(sandboxNativeGroups.size())) {
        const int cuts=NpcDismemberProbeTick();
        if(cuts<0){fail("npc_dismemberment");return;}if(!cuts)return;
        const int combat=NativeNpcCombatProbeTick();
        if(combat<0){fail("autonomous_combat");return;}
        if(!combat)return;
        if(!npcPlayback.empty() || !sandboxActors.empty() || !sandboxNativeActors.empty()){fail("cleanup");return;}
        fprintf(stderr,"K1_NATIVE_NPC_PASS loaded=%d swapped=%d native_fallback=%d groups=%zu rigs=%zu\n",
            loaded,swapped,fallback,sandboxNativeGroups.size(),nativeNpcRigs.size());
        phase=-1;send("quit:0\n");return;
    }
    if(phase==1) {
        CExoString message;
        auto* actor=SandboxSpawn(0,false,message,group,variant);
        if(!actor){fail("spawn");return;}
        live=actor->m_objectId;phase=2;ticks=0;return;
    }
    if(ticks<4)return;
    auto* actor=server->GetCreatureByGameObjectID(live);
    auto* client=actor?actor->GetClientCreature():nullptr;
    auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
    if(!actor || !client || !body || !body->m_pModel) {
        if(ticks>180)fail("replicated_native_body");return;
    }
    auto* serverWeapon=NativeNpcRightItem(actor);
    auto* clientWeapon=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(client->m_aEquippedItemIds[4]);
    if(!serverWeapon){fail("unarmed_template");return;}
    if(!clientWeapon || clientWeapon->GetServerItem()!=serverWeapon ||
       NpcWeaponClass(actor)==0 || NpcWeaponClass(actor)==CSWCAnimBase::WEAPON_CLASS_NATURAL) {
        if(ticks>180)fail("replicated_native_weapon");return;
    }
    const auto& entry=sandboxNativeGroups[group].variants[variant];
    const std::string native=body->m_pModel->m_szName;
    const CResRef texture=client->GetAnimationBase()->m_resModelTexture;
    auto* head=client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1);
    const std::string headName=head && head->m_pModel?head->m_pModel->m_szName:"";
    const auto right=client->m_aEquippedItemIds[4],left=client->m_aEquippedItemIds[5];
    const int hp=actor->GetCurrentHitPoints(0),maxHP=actor->GetMaxHitPoints(0),force=actor->m_pStats->m_nCurrentForcePoints;
    const int appearance=actor->m_appearance.m_headId,level=actor->m_pStats->GetLevel(0);
    std::vector<unsigned short> feats;
    for(int i=0;i<actor->m_pStats->m_feats.m_count;++i)feats.push_back(actor->m_pStats->m_feats.m_pArray[i]);
    if(SaberType(serverWeapon->m_baseItem) && !StaffNpc(actor) && !DualNpc(actor))sandboxStances[live]=loaded%3;
    const bool hasRig=NpcRigResource(native.c_str(),actor)!=nullptr && !body->m_pAddInModel;
    const int weapon=NpcWeaponClass(actor);
    const bool melee=Supported(client) || NativeNpcBlunt(client) || StaffNpc(actor) || DualNpc(actor);
    if(hasRig && melee) {
        if(!K1EngageEnter()){fail("pose_ownership_mode");return;}
        auto& npc=npcPlayback[live];npc.id=live;npc.owner=client;npc.body=body;npc.nativeResource=native;
        npc.position=actor->m_position;npc.staff=StaffNpc(actor);npc.dual=DualNpc(actor);
        npc.red=RedNpc(actor);npc.blue=BlueNpc(actor);
        npc.weaponClass=weapon;
        if(!SwapNpcRig(npc,true)){fail("source_handoff");return;}
        auto* source=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(!source || !IsSourceBody(source)){fail("source_body");return;}
        if(!NativeNpcSkinBindsValid(source)){fail("skin_bind_mismatch");return;}
        if(!RecordNpcDismemberWitness(actor,group,variant)){fail("source_cut_coverage");return;}
        const char* cuts[]={"s_top","s_tlbr","s_trbl","s_lr","s_rl","s_bltr","s_brtl"};
        for(int direction=0;direction<7;++direction) {
            npc.direction=direction;npc.redFrom=npc.red?direction:-1;npc.blueFrom=npc.blue?direction:-1;
            const char* clip=NpcAttackClip(npc,false);npc.base=clip;npc.attacking=true;
            source->PlayAnimation(nullptr,1,AnimRun::kAnimRemove,0);
            if(!source->PlayAnimation(const_cast<char*>(clip),1,AnimRun::kAnimNoBlendIn,0)){fail("attack_clip");return;}
            source->m_nLastAnimateTick=-1;source->Animate(.3f);
            std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(source,triangles);
            if(triangles.empty()){fail("posed_body_mesh");return;}
            for(const auto& t:triangles)for(const auto& p:{t.a,t.b,t.c})
                if(!K1SaberContact::Finite(p)){fail("nonfinite_pose");return;}
        }
        MaintainSaberCombat(client);
        for(int slot:{CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY})
            if(auto* weaponBody=client->GetModel(slot,1)){weaponBody->m_nLastAnimateTick=-1;weaponBody->Animate(.5f);}
        if(npc.dual) {
            K1SaberContact::Blade a,b;
            if(!DualBlade(client,0,a) || !DualBlade(client,1,b)){fail("dual_weapon_geometry");return;}
        } else if(npc.staff) {
            if(StaffBlades(client).empty()){fail("staff_weapon_geometry");return;}
        } else {
            Vector base,tip;if(!Blade(client,base,tip) || magnitude(tip-base)<.001f){fprintf(stderr,"K1_NATIVE_WEAPON_DIAG native_blunt=%d server_base=%d client_base=%d weapon_model=%d\n",NativeNpcBlunt(client),serverWeapon->m_baseItem,clientWeapon->m_baseItem,client->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1)!=nullptr);fail("single_weapon_geometry");return;}
        }
        if(!SwapNpcRig(npc,false)){fail("native_restore");return;}
        if(npc.red || npc.blue)fprintf(stderr,"K1_NATIVE_NPC_STANCE_PASS stance=%s body=%s clips=7 restored=1\n",npc.red?"Red":"Blue",native.c_str());
        npcPlayback.erase(live);++swapped;
    } else {
        ++fallback;
        // Ranged/unequipped actors deliberately retain their native controller.
        // A melee actor with an unhandled body is an implementation failure.
        if(melee && !hasRig){fail("unsupported_melee_body");return;}
    }
    body=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
    head=client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1);
    const std::string restoredHead=head && head->m_pModel?head->m_pModel->m_szName:"";
    if(!body || !body->m_pModel || _stricmp(body->m_pModel->m_szName,native.c_str()) ||
       restoredHead!=headName || !(client->GetAnimationBase()->m_resModelTexture==texture) ||
       client->m_aEquippedItemIds[4]!=right || client->m_aEquippedItemIds[5]!=left ||
       actor->m_appearance.m_headId!=appearance || actor->GetCurrentHitPoints(0)!=hp ||
       actor->GetMaxHitPoints(0)!=maxHP || actor->m_pStats->m_nCurrentForcePoints!=force ||
       actor->m_pStats->GetLevel(0)!=level || actor->m_pStats->m_feats.m_count!=int(feats.size())){fail("native_data_or_appearance_changed");return;}
    for(int i=0;i<int(feats.size());++i)if(feats[i]!=actor->m_pStats->m_feats.m_pArray[i]){fail("feat_changed");return;}
    fprintf(stderr,"K1_NATIVE_NPC_CASE_PASS template=%s source=%s/%s body=%s weapon=%d melee=%d hp=%d level=%d force=%d feats=%zu\n",
        entry.creature.c_str(),entry.module.c_str(),entry.source.c_str(),native.c_str(),weapon,melee,hp,level,force,feats.size());
    if(!RecordNpcDismemberWitness(actor,group,variant)){fail("native_cut_coverage");return;}
    ++loaded;
    sandboxActors.erase(live);sandboxNativeActors.erase(live);sandboxForces.erase(live);meleePoise.erase(live);
    sandboxStances.erase(live);sandboxAppearances.erase(live);
    actor->RemoveFromArea(0);delete actor;live=OBJECT_INVALID;
    if(++variant>=int(sandboxNativeGroups[group].variants.size())){variant=0;++group;}
    phase=1;ticks=0;
}
