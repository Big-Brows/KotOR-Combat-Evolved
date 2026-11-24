// Opt-in owned-data wearable/controller probe. Uses native equipment and
// appearance replication; no saves or original-install writes.
struct ArmorProbeCase {std::string native,item;int appearance=0,variant=0,disguise=0,sex=0;};
bool ArmorProbeRestorePose(CSWCCreature* actor,const char* resource) {
    auto* body=actor->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);CSWCAnimBase reference;
    if(!body||IsSourceBody(body)||!body->m_pModel||_stricmp(body->m_pModel->m_szName,resource)||
       !reference.LoadModel(CResRef(resource),CSWCAnimBase::ANIM_MODEL_BODY))return false;
    auto* original=reference.GetModel(CSWCAnimBase::ANIM_MODEL_BODY);bool ok=original!=nullptr;
    float error=0;int samples=0;
    for(const char* clip:{"pause1","walk","run"}) {
        for(auto* model:{body,original}) {
            model->m_bAlwaysRendering=true;model->EnableForceAnimations();model->EndLookAtAnimate();
            model->PlayAnimation(nullptr,1,0,0);model->Reset();
            if(!model->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,.25f))return false;
            model->m_nLastAnimateTick=-1;model->Animate(.000001f);
        }
        auto compare=[&](auto&& self,Part* a,Part* b)->void {
            if(a->m_pNode->m_pTree!=body->m_pModel || b->m_pNode->m_pTree!=original->m_pModel)return;
            if(strcmp(a->Name(),b->Name()) || a->m_nChildren!=b->m_nChildren){ok=false;return;}
            error=std::max(error,magnitude(a->m_vLocalPosition-b->m_vLocalPosition)+
                1-std::fabs(dot(normalize(a->m_qLocalOrientation),normalize(b->m_qLocalOrientation))));++samples;
            for(int i=0;i<a->m_nChildren;++i)self(self,a->m_paChildren[i],b->m_paChildren[i]);
        };compare(compare,body->m_pPart,original->m_pPart);
    }
    body->PlayAnimation(nullptr,1,0,0);body->Reset();actor->SetAnimation(actor->GetLoopingAnimation(),0,0);
    fprintf(stderr,"K1_ARMOR_NATIVE_%s model=%s samples=%d error=%g\n",ok&&error<.0002f?"PASS":"FAIL",resource,samples,error);
    return ok&&samples>0&&error<.0002f;
}
void ArmorProbeTick() {
    if(!sandboxReady)return;
    static int phase=0,ticks=0,index=0,style=0,total=0,poses=0,stuns=0;
    static std::vector<ArmorProbeCase> cases;
    static CSWSCreatureAppearanceInfo originalAppearance;
    static unsigned short originalType=0;static unsigned char originalSex=0,originalPhenotype=0;
    static unsigned long long originalArmor=OBJECT_INVALID;
    static unsigned long long originalRight=OBJECT_INVALID,originalLeft=OBJECT_INVALID;
    static int originalSelection=-1;static std::vector<int> originalBaseStats;
    static std::vector<unsigned short> originalFeats;
    static std::unordered_map<std::string,unsigned long long> items;
    static CResRef nativeTexture;static std::string headName,weaponName;
    static std::vector<CutTexture> nativeMaterials;
    static bool sawStun=false;
    static bool hotDone=false;static int hotStep=0;static unsigned long long swapSerial=0;
    if(phase<0)return;++ticks;
    auto* player=ServerActor();auto* client=Actor();auto* server=g_pAppManager->m_pServerExoApp;
    if(!player||!client)return;
    auto baseStats=[&]{auto* s=player->m_pStats;return std::vector<int>{s->GetLevel(0),s->m_nSTRBase,s->m_nDEXBase,s->m_nCONBase,s->m_nINTBase,s->m_nWISBase,s->m_nCHABase};};
    auto quit=[] {if(FILE* f=fopen("armor-input.txt","w")){fputs("quit:0\n",f);fclose(f);}};
    auto fail=[&](const char* why){fprintf(stderr,"K1_ARMOR_FAIL phase=%d body=%d style=%d reason=%s\n",phase,index,style,why);phase=-1;quit();};
    auto unequip=[&](int slot){auto* item=player->m_pInventory->GetItemInSlot(slot);return !item||player->RunUnequip(item->m_objectId,OBJECT_INVALID,0,nullptr,1)!=0;};
    auto chooseWeapons=[&] {
        if(!unequip(CSWInventory::SLOT_LEFTWEAPON))return false;
        const auto wanted=arenaSaberIds[style==5?1:0];
        auto* right=player->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
        if((!right||right->m_objectId!=wanted) && !player->RunEquip(wanted,CSWInventory::SLOT_RIGHTWEAPON,1))return false;
        if(style==4 && !player->RunEquip(arenaOffhandSaberId,CSWInventory::SLOT_LEFTWEAPON,1))return false;
        redSelection=style<4?style:0;return true;
    };
    if(phase==0) {
        if(ticks<30)return;SandboxSetAi(false);
        C2DA table(CResRef("k1armorcheck"),0);if(!table.Load2DArray()){fail("case_table");return;}
        for(int row=0;row<table.m_nRows;++row) {
            ArmorProbeCase c;CExoString native,item;
            table.GetCExoStringEntry(row,CExoString("native"),&native);table.GetCExoStringEntry(row,CExoString("item"),&item);
            c.native=native.CStr();c.item=item.CStr();
            table.GetINTEntry(row,CExoString("appearance"),&c.appearance);table.GetINTEntry(row,CExoString("variant"),&c.variant);
            table.GetINTEntry(row,CExoString("disguise"),&c.disguise);table.GetINTEntry(row,CExoString("sex"),&c.sex);cases.push_back(c);
        }
        if(cases.size()!=76 || arenaSaberIds[0]==OBJECT_INVALID || arenaSaberIds[1]==OBJECT_INVALID || arenaOffhandSaberId==OBJECT_INVALID){fail("fixture_inventory");return;}
        originalAppearance=player->m_appearance;originalType=player->m_pStats->m_nAppearanceType;
        originalSex=player->m_pStats->m_nGender;originalPhenotype=player->m_pStats->m_nPhenotype;
        if(auto* item=player->m_pInventory->GetItemInSlot(CSWInventory::SLOT_BODY))originalArmor=item->m_objectId;
        if(auto* item=player->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON))originalRight=item->m_objectId;
        if(auto* item=player->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON))originalLeft=item->m_objectId;
        originalSelection=redSelection;originalBaseStats=baseStats();
        for(int i=0;i<player->m_pStats->m_feats.m_count;++i)originalFeats.push_back(player->m_pStats->m_feats.m_pArray[i]);
        // The geometry fixture must also wear restricted Revan/Star Forge robes.
        // Native eligibility explicitly supports this feat; grant only inside
        // this opt-in probe and remove it with all fixture feats on restoration.
        if(!player->m_pStats->HasFeat(FEAT_PROFICIENCY_ALL))player->m_pStats->AddFeat(FEAT_PROFICIENCY_ALL);
        if(client->m_pStats && !client->m_pStats->HasFeat(FEAT_PROFICIENCY_ALL))client->m_pStats->m_lstFeats.Add(FEAT_PROFICIENCY_ALL);
        phase=1;ticks=0;return;
    }
    if(index>=int(cases.size())) {
        Plan();if(!unequip(CSWInventory::SLOT_BODY)){fail("restore_unequip");return;}
        player->m_pStats->m_nAppearanceType=originalType;player->m_pStats->m_nGender=originalSex;player->m_pStats->m_nPhenotype=originalPhenotype;
        player->m_appearance=originalAppearance;player->UpdateAppearanceDependantInfo(1);
        if(originalArmor!=OBJECT_INVALID && !player->RunEquip(originalArmor,CSWInventory::SLOT_BODY,1)){fail("restore_armor");return;}
        auto* right=player->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON);
        if(!unequip(CSWInventory::SLOT_LEFTWEAPON) || (originalRight!=OBJECT_INVALID&&(!right||right->m_objectId!=originalRight)&&!player->RunEquip(originalRight,CSWInventory::SLOT_RIGHTWEAPON,1)) ||
           (originalLeft!=OBJECT_INVALID&&!player->RunEquip(originalLeft,CSWInventory::SLOT_LEFTWEAPON,1))){fail("restore_weapons");return;}
        redSelection=originalSelection;
        std::vector<unsigned short> extra;
        for(int i=0;i<player->m_pStats->m_feats.m_count;++i){const auto f=player->m_pStats->m_feats.m_pArray[i];if(std::find(originalFeats.begin(),originalFeats.end(),f)==originalFeats.end())extra.push_back(f);}
        for(auto f:extra){player->m_pStats->RemoveFeat(f);if(client->m_pStats)client->m_pStats->m_lstFeats.Remove(f);}
        if(baseStats()!=originalBaseStats || player->m_pStats->m_feats.m_count!=int(originalFeats.size())){fail("restore_stats_or_feats");return;}
        fprintf(stderr,"K1_ARMOR_PASS bodies=76 styles=6 cases=%d poses=%d native_status=%d feats_restored=1\n",total,poses,stuns);
        phase=-1;quit();return;
    }
    const auto& c=cases[index];
    if(phase==1) {
        hotDone=false;
        Plan();if(!unequip(CSWInventory::SLOT_BODY)){fail("unequip_body");return;}
        // Real disguise armors apply their own native appearance effect. Other
        // cases select the native sex/size row before equipping the actual item.
        player->m_pStats->m_nAppearanceType=c.disguise?originalType:c.appearance;
        player->m_appearance.m_headId=player->m_pStats->m_nAppearanceType;
        player->m_pStats->m_nGender=c.sex;player->m_appearance.m_nGender=c.sex;
        player->UpdateAppearanceDependantInfo(1);
        if(!c.item.empty() && c.item!="****") {
            auto found=items.find(c.item);
            if(found==items.end()) {
                if(!EquipFixtureWeapon(player,c.item.c_str(),CSWInventory::SLOT_BODY)){fail("equip_native_armor");return;}
                auto* item=player->m_pInventory->GetItemInSlot(CSWInventory::SLOT_BODY);if(!item){fail("armor_slot");return;}
                items[c.item]=item->m_objectId;
            } else if(!player->RunEquip(found->second,CSWInventory::SLOT_BODY,1)){fail("reequip_native_armor");return;}
        }
        style=0;if(!chooseWeapons()){fail("equip_weapon");return;}phase=2;ticks=0;return;
    }
    if(phase==2) {
        // This synthetic probe advances hundreds of frames in a few real ms.
        // Use the native paused-request budget, then let client AI finish the
        // existing request; do not force an appearance or bypass replication.
        if(client->m_nAppearanceRequestTimer>1)client->m_nAppearanceRequestTimer=1;
        auto appearance=LiveAppearance(client);CResRef expected;NativePlayerModel(client,appearance,expected);
        auto* nativeBody=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(_stricmp(expected.GetResRefStr(),c.native.c_str()) || !Supported(client) || !nativeBody ||
           IsSourceBody(nativeBody) || !nativeBody->m_pModel || _stricmp(nativeBody->m_pModel->m_szName,c.native.c_str())) {
            if(ticks>240){fprintf(stderr,"K1_ARMOR_EXPECTED native=%s actual=%s body=%s source=%d supported=%d class=%d appearance=%d type=%d server_appearance=%d disguised=%d\n",c.native.c_str(),expected.GetResRefStr(),nativeBody&&nativeBody->m_pModel?nativeBody->m_pModel->m_szName:"none",IsSourceBody(nativeBody),Supported(client),client->GetAnimationBase()?client->GetAnimationBase()->m_nWeaponClass:-1,appearance.core.m_nAppearanceId,appearance.core.m_bTypeVariant,player->m_pStats->m_nAppearanceType,player->m_bDisguised);fail("native_replication");}return;
        }
        nativeTexture=client->GetAnimationBase()->m_resModelTexture;
        nativeMaterials=CutTextures(client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1));
        auto* head=client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1);headName=head&&head->m_pModel&&head->m_pScene==nativeBody->m_pScene?head->m_pModel->m_szName:"";
        auto* weapon=client->GetModel(CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);weaponName=weapon&&weapon->m_pModel?weapon->m_pModel->m_szName:"";
        if(!K1EngageEnter()){fail("enter");return;}phase=3;ticks=0;return;
    }
    if(phase==3) {
        if(ticks<3)return;
        auto* body=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(!body||!body->m_pModel||_stricmp(body->m_pModel->m_szName,PlayerRigResource())||
           !CheckRigActorOwnership(client,true,nativeTexture,headName,weaponName,"wearable",true)||!NativeNpcSkinBindsValid(body)){fail("body_identity_bind_or_attachment");return;}
        body->m_bAlwaysRendering=true;body->EnableForceAnimations();
        for(const auto& actual:CutTextures(body)) {
            std::string base=CutBasePartName(actual.part);auto duplicate=base.find("_k1dup");if(duplicate!=std::string::npos)base.resize(duplicate);
            bool matches=false;
            for(const auto& expected:nativeMaterials)if(!_stricmp(base.c_str(),expected.part.c_str())&&!_stricmp(actual.resource.c_str(),expected.resource.c_str()))matches=true;
            // The preserved PMBBL donor combines native torso/leg meshes into
            // TorsoLegs. Its material must still match a native body material;
            // generated armor rigs retain per-part names and use the strict check.
            if(c.native=="pmbbl" && !_stricmp(base.c_str(),"TorsoLegs"))
                for(const auto& expected:nativeMaterials)if(!_stricmp(actual.resource.c_str(),expected.resource.c_str()))matches=true;
            if(!matches){fprintf(stderr,"K1_ARMOR_MATERIAL_FAIL part=%s resource=%s\n",actual.part.c_str(),actual.resource.c_str());fail("native_material");return;}
        }
        for(int d=-1;d<7;++d)for(float fraction:{.1f,.5f,.9f}) {
            state.attacking=d>=0;state.rightHeld=d<0;swingDirection=std::max(0,d);playerPlayback={};
            currentBase=d<0?"j_guard":"j_idle";currentOverlay=d<0?nullptr:fullAttacks[d];
            body->PlayAnimation(nullptr,1,0,0);body->Reset();
            const char* clip=d<0?"j_guard":fullAttacks[d];
            if(!body->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,0)){fail("clip");return;}
            float time=0,length=0;if(!body->GetAnimation(const_cast<char*>(clip),&time,&length)){fail("clip_length");return;}
            body->SetAnimationTime(const_cast<char*>(clip),fraction*length);body->m_nLastAnimateTick=-1;body->Animate(.000001f);
            std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(body,triangles);
            if(triangles.size()<100 || playerPlayback.sourceContactError>.005f){fail("posed_skin_or_grip");return;}
            for(const auto& face:triangles)for(const auto& v:{face.a,face.b,face.c})if(!K1SaberContact::Finite(v)||magnitude(v-player->m_position)>6.f){fail("posed_bounds");return;}
            ++poses;
        }
        for(int gait=0;gait<4;++gait) {
            const bool guarding=gait==1;const char* base=gait<2?"j_walk":gait==2?"j_run":"j_backrun";
            state.attacking=false;state.rightHeld=guarding;currentBase=base;currentOverlay=guarding?"y_guard":nullptr;playerPlayback={};
            body->PlayAnimation(nullptr,1,0,0);body->Reset();
            if(!body->PlayAnimation(const_cast<char*>(base),0,AnimRun::kAnimNoBlendIn,0)){fail("gait_clip");return;}
            if(guarding&&!body->PlayAnimation(const_cast<char*>("y_guard"),0,AnimRun::kAnimOverlay|AnimRun::kAnimNoBlendIn,0)){fail("walking_guard_clip");return;}
            float time=0,length=0;body->GetAnimation(const_cast<char*>(base),&time,&length);
            for(float fraction:{.1f,.5f,.9f}) {
                body->SetAnimationTime(const_cast<char*>(base),fraction*length);body->m_nLastAnimateTick=-1;body->Animate(.15f);
                if(gait>=2 && playerPlayback.nativeGait.resource!=c.native){fail("native_gait_body");return;}
                std::vector<K1SaberContact::Triangle> triangles;CollisionTriangles(body,triangles);
                if(triangles.size()<100||playerPlayback.sourceContactError>.005f){fail("gait_skin_or_grip");return;}
                for(const auto& face:triangles)for(const auto& v:{face.a,face.b,face.c})if(!K1SaberContact::Finite(v)||magnitude(v-player->m_position)>6.f){fail("gait_bounds");return;}
                ++poses;
            }
        }
        state.attacking=state.rightHeld=false;currentOverlay=nullptr;currentBase="j_idle";
        body->PlayAnimation(nullptr,1,0,0);body->PlayAnimation(const_cast<char*>("j_idle"),1,AnimRun::kAnimNoBlendIn,0);
        if(style==0 && !hotDone && (c.native=="pmbcm" || c.native=="pfbcm")) {
            state.rightHeld=true;currentBase="j_guard";body->PlayAnimation(const_cast<char*>("j_guard"),1,AnimRun::kAnimNoBlendIn,0);
            K1EngageCycleStance();hotStep=1;phase=6;ticks=0;return;
        }
        if(style==0){
            auto* effect=new CGameEffect(1);effect->m_subType=CSWSEffectListHandler::EFFECT_SETSTATE;effect->m_durationTypeBits=CGameEffect::DURATION_TYPE_TEMPORARY;
            effect->m_fDuration=.25f;effect->SetInteger(0,EFFECT_STATE_STUNNED);effect->SetInteger(1,1);player->ApplyEffect(effect,0,1);
            sawStun=false;phase=4;ticks=0;return;
        }
        phase=5;ticks=0;return;
    }
    if(phase==4) {
        auto* body=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(NativeStatusAnimation(player) && body && !IsSourceBody(body))sawStun=true;
        if(sawStun && !NativeStatusAnimation(player) && !nativeActionBody && body && IsSourceBody(body)){++stuns;phase=5;ticks=0;return;}
        if(ticks>240)fail("native_status_handoff");return;
    }
    if(phase==5) {
        Plan();if(!ArmorProbeRestorePose(client,c.native.c_str())){fail("native_restore");return;}
        fprintf(stderr,"K1_ARMOR_CASE_PASS native=%s style=%d texture=%s head=%s native_restore=1\n",c.native.c_str(),style,nativeTexture.GetResRefStr(),headName.c_str());++total;
        if(++style<6){if(!chooseWeapons()){fail("next_weapon");return;}phase=2;ticks=0;return;}
        if(c.native=="pmbcm" || c.native=="pfbcm") {
            if(!K1EngageEnter()){fail("swap_enter");return;}
            state.attacking=true;state.chainSwings=2;++attackSerial;swapSerial=attackSerial;
            currentOverlay=fullAttacks[0];auto* source=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
            source->PlayAnimation(const_cast<char*>(fullAttacks[0]),1,AnimRun::kAnimNoBlendIn,0);
            if(!EquipFixtureWeapon(player,"g_a_clothes02",CSWInventory::SLOT_BODY)){fail("live_equipment_change");return;}
            phase=7;ticks=0;return;
        }
        ++index;phase=1;ticks=0;return;
    }
    if(phase==6) {
        if(ticks<12)return;
        auto* body=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(!body || _stricmp(body->m_pModel->m_szName,PlayerRigResource()) ||
           !CheckRigActorOwnership(client,true,nativeTexture,headName,weaponName,"wearable_hot",true)){fail("hot_stance_switch");return;}
        if(hotStep++<4){K1EngageCycleStance();ticks=0;return;}
        fprintf(stderr,"K1_ARMOR_HOT_SWITCH_PASS native=%s edges=4 head=1 texture=1\n",c.native.c_str());
        hotDone=true;phase=3;ticks=0;return;
    }
    if(phase==7) {
        if(client->m_nAppearanceRequestTimer>1)client->m_nAppearanceRequestTimer=1;
        auto appearance=LiveAppearance(client);CResRef expected;NativePlayerModel(client,appearance,expected);
        auto* body=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(ticks<6 || !_stricmp(expected.GetResRefStr(),c.native.c_str()) || !body || !IsSourceBody(body) || _stricmp(body->m_pModel->m_szName,PlayerRigResource())) {
            if(ticks>240)fail("equipment_change_replication");return;
        }
        if(state.attacking || state.chainSwings || std::any_of(sceneMeleeContacts.begin(),sceneMeleeContacts.end(),[&](const MeleeContact& contact){return contact.attacker==player->m_objectId && contact.serial==int(swapSerial);})){fail("equipment_change_outgoing_swing");return;}
        auto texture=client->GetAnimationBase()->m_resModelTexture;
        if(!CheckRigActorOwnership(client,true,texture,headName,weaponName,"wearable_swap",true)){fail("equipment_change_attachment");return;}
        fprintf(stderr,"K1_ARMOR_EQUIPMENT_SWAP_PASS native=%s incoming=%s cancelled=1 head=1 texture=1\n",c.native.c_str(),expected.GetResRefStr());
        Plan();++index;phase=1;ticks=0;return;
    }
}
