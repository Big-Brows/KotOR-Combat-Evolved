// Opt-in installed-data coverage, posed models and actual native fatal effects.
// No saves; uses the existing sandbox ownership and native damage services.
struct NpcCutWitness {int group,variant;};
std::map<std::string,NpcCutWitness> npcCutWitnesses;
bool RecordNpcDismemberWitness(CSWSCreature* actor,int group,int variant) {
    if(!std::getenv("K1_ENGAGE_NPC_CUT_TEST"))return true;
    auto* client=actor?actor->GetClientCreature():nullptr;if(!client)return false;
    const std::string body=NativeCutBody(client);std::set<std::string> codes;
    for(const auto& d:CutDefinitions())if(!_stricmp(body.c_str(),d.body.c_str())) {
        if(!CutResources(d))return false;codes.insert(d.code);
    }
    const bool tusken=!_stricmp(body.c_str(),"n_tusken");
    if(codes.size()!=(tusken?6:8) || (tusken && (codes.count("rl")||codes.count("ll"))))return false;
    if(auto* head=client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1)) {
        std::vector<K1SaberContact::Triangle> triangles;std::vector<Part*> parts;CollisionTriangles(head,triangles,&parts);
        if(!triangles.empty()) {
            const auto& t=triangles.front();
            if(CutRegion(client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1),head,parts.front(),(t.a+t.b+t.c)/3.f)!="he")return false;
        }
    }
    npcCutWitnesses[body]={group,variant};
    fprintf(stderr,"K1_NPC_CUT_COVERAGE_PASS template=%s body=%s regions=%zu\n",sandboxNativeGroups[group].variants[variant].creature.c_str(),body.c_str(),codes.size());
    return true;
}
bool NpcCutRegionModelCheck(Gob* body,const std::string& native) {
    body->PlayAnimation(nullptr,1,0,0);body->Reset();body->Animate(.000001f);
    std::vector<K1SaberContact::Triangle> triangles;std::vector<Part*> parts;CollisionTriangles(body,triangles,&parts);
    if(triangles.empty())return false;
    bool external=false;for(const auto& d:CutDefinitions())if(d.body==native && d.code=="he")external |= d.wholeHead;
    for(const char* code:{"ra","rh","rl","la","lh","ll","wa","he"}) {
        const bool forbidden=native=="n_tusken" && (code[1]=='l');
        if(code[0]=='h' && external)continue;
        Vector point,a,b;
        const bool right=code[0]=='r';
        if(code[1]=='a' && code[0]!='w') {
            if(!CutBone(body,right?"rbicep_g":"lbicep_g",a)||!CutBone(body,right?"Rforearm_g":"Lforearm_g",b))return false;
            point=(a+b)*.5f;
        } else if(code[1]=='h') {
            if(!CutBone(body,right?"Rhand_g":"Lhand_g",point))return false;
        } else if(code[1]=='l') {
            if(!CutBone(body,right?"rshin_g":"lshin_g",a)||!CutBone(body,right?"rfoot_g":"lfoot_g",b))return false;
            point=a+(b-a)*.3f;
        } else if(!CutBone(body,code[0]=='w'?"pelvis_g":"head_g",point))return false;
        std::vector<std::pair<float,int>> nearest;
        for(int i=0;i<int(triangles.size());++i) {
            const auto& t=triangles[i];const Vector v=(t.a+t.b+t.c)/3.f-point;nearest.push_back({dot(v,v),i});
        }
        std::sort(nearest.begin(),nearest.end());bool found=false;
        for(int n=0;n<std::min(64,int(nearest.size()));++n) {
            const int i=nearest[n].second;const auto& t=triangles[i];const auto result=CutRegion(body,nullptr,parts[i],(t.a+t.b+t.c)/3.f);
            if(forbidden && (result=="rl" || result=="ll"))return false;
            found |= result==code;
        }
        if(!forbidden && !found){fprintf(stderr,"K1_NPC_CUT_REGION_FAIL body=%s cut=%s\n",native.c_str(),code);return false;}
    }
    fprintf(stderr,"K1_NPC_CUT_REGIONS_PASS body=%s tusken_legs=%d\n",native.c_str(),native!="n_tusken");return true;
}
bool NpcCutModelCheck(const CutDefinition& d,int& samples,float& gap) {
    const char* currentResource="source";const char* currentClip="none";
    auto failed=[&](int line){fprintf(stderr,"K1_NPC_CUT_DIAGNOSTIC body=%s cut=%s resource=%s clip=%s line=%d samples=%d\n",d.body.c_str(),d.code.c_str(),currentResource,currentClip,line,samples);return false;};
    std::unique_ptr<Gob> oracle(static_cast<Gob*>(NewCAurObject(const_cast<char*>(d.body.c_str()),nullptr,nullptr,nullptr)));
    if(!oracle)return failed(__LINE__);oracle->EnableForceAnimations();
    static std::set<std::string> regionBodies;
    if(!regionBodies.count(d.body)) {
        if(!NpcCutRegionModelCheck(oracle.get(),d.body))return failed(__LINE__);
        regionBodies.insert(d.body);
    }
    for(const auto& resource:{d.retained,d.detached}) {
        currentResource=resource.c_str();
        std::unique_ptr<Gob> model(static_cast<Gob*>(NewCAurObject(const_cast<char*>(resource.c_str()),nullptr,nullptr,nullptr)));
        if(!model || !NativeNpcSkinBindsValid(model.get()))return failed(__LINE__);model->EnableForceAnimations();
        int clips=0;
        for(const char* clip:{"die","die1","die3","dead","dead1","dead3","cdie","cdead"}) {
            currentClip=clip;
            oracle->PlayAnimation(nullptr,1,0,0);oracle->Reset();
            const bool exists=oracle->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,0)!=0;
            model->PlayAnimation(nullptr,1,0,0);model->Reset();
            if((model->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,0)!=0)!=exists)return failed(__LINE__);
            if(!exists)continue;++clips;float time=0,length=0;
            if(!oracle->GetAnimation(const_cast<char*>(clip),&time,&length))return failed(__LINE__);
            for(float fraction:{0.f,.5f,1.f}) {
                for(auto* object:{oracle.get(),model.get()}) {
                    object->PlayAnimation(nullptr,1,0,0);object->Reset();
                    if(!object->PlayAnimation(const_cast<char*>(clip),0,AnimRun::kAnimNoBlendIn,length*fraction))return failed(__LINE__);
                    object->Animate(.000001f);
                }
                bool valid=true;float maxPosition=0,maxRotation=0;std::vector<Vector> rim,skin;
                auto check=[&](auto&& self,Part* p)->void {
                    if(auto* ref=oracle->GetPart(p->Name())) {
                        maxPosition=std::max(maxPosition,magnitude(p->m_vLocalPosition-ref->m_vLocalPosition));
                        valid &= maxPosition<.0001f;
                        maxRotation=std::max(maxRotation,1.f-std::fabs(dot(normalize(p->m_qLocalOrientation),normalize(ref->m_qLocalOrientation))));
                        valid &= maxRotation<.0001f;
                    }
                    if(auto* s=p->AsPartSkin())if(p->m_bDrawEnabled && static_cast<MdlNodeTriMesh*>(p->m_pNode)->m_bRender) {
                        List<Vector> vertices;valid &= s->GetSkinnedWorldVertices(vertices);
                        if(_strnicmp(p->Name(),"dismember_glow",14)) {
                            auto& output=!_strnicmp(p->Name(),"dismember_cap",13)?rim:skin;
                            for(int i=0;i<vertices.m_count;++i){valid &= K1SaberContact::Finite(vertices.m_pData[i]);output.push_back(vertices.m_pData[i]);}
                        }
                    }
                    for(int i=0;i<p->m_nChildren;++i)self(self,p->m_paChildren[i]);
                };check(check,model->m_pPart);
                if(!valid || rim.empty() || (skin.empty() && !(d.wholeHead && resource==d.detached))){fprintf(stderr,"K1_NPC_CUT_POSE_DETAIL valid=%d position=%g rotation=%g rim=%zu skin=%zu fraction=%g\n",valid,maxPosition,maxRotation,rim.size(),skin.size(),fraction);return failed(__LINE__);}
                if(!skin.empty())for(const auto& v:rim) {
                    float nearest=1e6f;for(const auto& p:skin)nearest=std::min(nearest,magnitude(v-p));gap=std::max(gap,nearest);
                    if(nearest>.0002f)return failed(__LINE__);
                }
                ++samples;
            }
            float terminal=0,reference=0;
            if(!FreezeFinalCutPose(model.get(),clip,terminal)||!FreezeFinalCutPose(oracle.get(),clip,reference)||std::fabs(terminal-reference)>.0001f)return failed(__LINE__);
        }
        if(!clips)return failed(__LINE__);
    }
    return true;
}
int NpcDismemberProbeTick() {
    if(!std::getenv("K1_ENGAGE_NPC_CUT_TEST"))return 1;
    static std::vector<int> rows,liveRows;static int index=0,liveIndex=0,ticks=0,samples=0;
    static float gap=0;static bool initialized=false,modelsDone=false,done=false;
    static unsigned long long live=OBJECT_INVALID,cutClient=OBJECT_INVALID;static bool applied=false;
    if(done)return 1;
    auto fail=[&](const char* why){fprintf(stderr,"K1_NPC_CUT_FAIL model=%d live=%d reason=%s\n",index,liveIndex,why);return -1;};
    if(!initialized) {
        initialized=true;SandboxSetAi(false);
        std::set<std::string> bodies;
        for(int i=0;i<int(CutDefinitions().size());++i) {
            const auto& d=CutDefinitions()[i];if(_strnicmp(d.retained.c_str(),"k1c",3))continue;
            rows.push_back(i);bodies.insert(d.body);
            if(npcCutWitnesses.count(d.body))liveRows.push_back(i);
            if(!_stricmp(d.body.c_str(),"n_tusken") && (d.code=="rl" || d.code=="ll"))return fail("tusken_legs_enabled");
        }
        if(rows.size()!=334 || bodies.size()!=42 || npcCutWitnesses.empty())return fail("catalog_count");
    }
    if(index<int(rows.size())) {
        const auto& d=CutDefinitions()[rows[index]];
        if(!NpcCutModelCheck(d,samples,gap)){fprintf(stderr,"K1_NPC_CUT_MODEL_FAIL body=%s cut=%s gap=%g\n",d.body.c_str(),d.code.c_str(),gap);return fail("posed_model");}
        fprintf(stderr,"K1_NPC_CUT_PAIR_PASS body=%s cut=%s\n",d.body.c_str(),d.code.c_str());++index;return 0;
    }
    if(!modelsDone){modelsDone=true;fprintf(stderr,"K1_NPC_CUT_MODELS_PASS bodies=42 pairs=334 models=668 death_samples=%d maximum_gap=%g tusken_legs=0\n",samples,gap);}
    auto* server=g_pAppManager->m_pServerExoApp;
    if(liveIndex<int(liveRows.size())) {
        const int row=liveRows[liveIndex];const auto& d=CutDefinitions()[row];
        if(live==OBJECT_INVALID) {
            auto w=npcCutWitnesses.at(d.body);CExoString message;
            auto* actor=SandboxSpawn(0,false,message,w.group,w.variant);if(!actor)return fail("spawn");
            live=actor->m_objectId;cutClient=OBJECT_INVALID;applied=false;ticks=0;return 0;
        }
        ++ticks;auto* actor=server->GetCreatureByGameObjectID(live);
        auto* client=cutClient!=OBJECT_INVALID?g_pAppManager->m_pClientExoApp->GetCreatureByGameObjectID(cutClient):actor?actor->GetClientCreature():nullptr;
        if(!client || (!actor && !applied)){if(ticks<180)return 0;return fail("replication");}
        if(!applied && ticks>=4) {
            applied=true;cutClient=client->m_objectId;
            auto* body=client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
            if(!body || _stricmp(NativeCutBody(client),d.body.c_str()))return fail("body_identity");
            if((liveIndex%2)==0 && NpcRigResource(d.body.c_str(),actor) && (Supported(client)||NativeNpcBlunt(client)||StaffNpc(actor)||DualNpc(actor))) {
                auto& npc=npcPlayback[live];npc.id=live;npc.owner=client;npc.body=body;npc.nativeResource=d.body;
                npc.position=actor->m_position;npc.staff=StaffNpc(actor);npc.dual=DualNpc(actor);npc.red=RedNpc(actor);npc.blue=BlueNpc(actor);npc.weaponClass=NpcWeaponClass(actor);
                if(!SwapNpcRig(npc,true))return fail("source_swap");
            }
            auto* damage=new CGameEffect(1);damage->m_subType=CSWSEffectListHandler::EFFECT_DAMAGE;damage->m_durationTypeBits=CGameEffect::DURATION_TYPE_INSTANTANEOUS;damage->SetCreator(ServerActor()->m_objectId);damage->SetNumIntegers(25);
            for(int i=0;i<15;++i)damage->SetInteger(i,-1);
            damage->SetInteger(7,5000);damage->SetInteger(14,5000);damage->SetInteger(19,1);damage->SetInteger(20,1);damage->SetInteger(21,1);
            damage->SetInteger(22,cutEffectMarker);damage->SetInteger(23,row);damage->SetInteger(24,1);
            actor->ApplyEffect(damage,0,0);return 0;
        }
        if(!applied)return 0;
        auto corpse=cutCorpses.find(cutClient);
        if(corpse==cutCorpses.end() || !corpse->second.limb || corpse->second.frozenSamples<3) {
            if(ticks<300)return 0;return fail("native_death_detachment");
        }
        auto& c=corpse->second;
        for(auto* detached:{c.limb,c.head,c.weapon,c.leftWeapon})if(detached&&detached->m_nShadows)return fail("detached_shadow");
        auto* retained=client->GetModel(d.slot,1);
        if(!retained || !CutTexturesMatch(retained,CutTextures(retained)))return fail("retained_cap_material");
        if((actor && actor->GetCurrentHitPoints(0)>0) || !c.texturesMatch || c.frozenError>.0001f || (d.wholeHead && !c.headResource.empty() && !c.head)) {
            fprintf(stderr,"K1_NPC_CUT_DEATH_DETAIL hp=%d textures=%d frozen=%g whole=%d head_source=%s head=%d\n",actor?actor->GetCurrentHitPoints(0):-1,c.texturesMatch,c.frozenError,d.wholeHead,c.headResource.c_str(),c.head!=nullptr);
            return fail("death_material_head");
        }
        if((d.code=="wa"||d.code=="la"||d.code=="lh") && !c.leftWeaponResource.empty() && !c.leftWeapon)return fail("left_weapon");
        fprintf(stderr,"K1_NPC_CUT_DEATH_PASS body=%s cut=%s whole_head=%d\n",d.body.c_str(),d.code.c_str(),d.wholeHead);
        ClearDismemberArea(OBJECT_INVALID);npcPlayback.erase(live);sandboxActors.erase(live);sandboxNativeActors.erase(live);sandboxForces.erase(live);sandboxStances.erase(live);sandboxAppearances.erase(live);meleePoise.erase(live);
        if(actor){actor->RemoveFromArea(0);delete actor;}live=OBJECT_INVALID;cutClient=OBJECT_INVALID;++liveIndex;ticks=0;return 0;
    }
    if(!cutCorpses.empty() || cutPartsCreated!=cutPartsDeleted)return fail("owned_parts_cleanup");
    fprintf(stderr,"K1_NPC_CUT_LIVE_PASS bodies=%zu deaths=%d cleanup=1 tusken_legs=0\n",npcCutWitnesses.size(),liveIndex);done=true;return 1;
}
