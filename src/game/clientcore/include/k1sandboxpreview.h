// Standalone native creature preview. Never inserted into an area or registered
// as a combat actor; the scene and its attachments are owned by the modal.
class K1SandboxPreviewView final:public CSWGui3DSceneView {
public:
    bool friendly=false;
};
class K1SandboxPreview {
public:
    K1SandboxPreviewView view;
    std::unique_ptr<CSWCCreature> creature;
    K1SandboxPreview() {
        view.m_tail.bIgnoreMouse=true;
        view.m_scene.m_pScene->SpawnRoom(const_cast<char*>("gui3D_room"),Vector(0,0,0),Quaternion(0,0,0,1));
    }
    void Clear() {
        // Attachments must leave before their parent creature is destroyed.
        while(view.m_scene.m_aModels.m_count)view.m_scene.RemoveModel(view.m_scene.m_aModels.m_count-1,1);
        creature.reset();view.m_tail.bVisible=false;
    }
    ~K1SandboxPreview(){Clear();}
    bool Set(CSWSCreature* source,CSWSCreature* look=nullptr) {
        Clear();if(!source || !source->m_pStats)return false;
        auto* alignment=static_cast<Gob*>(view.m_scene.AddModel(CExoString("charrec_light"),-1));
        if(!alignment)return false;
        alignment->PlayAnimation(const_cast<char*>(view.friendly?"good":"evil"),1.f,2,0.f);
        // Keep the original alignment lights/geometry. Unlist only the hostile
        // effect's smoke while retaining Evil_stream, which also uses fx_Smoke.
        int smoke=0,preserved=0;
        auto suppress=[&](auto&& self,Part* part)->void {
            if(!part)return;
            auto* emitter=part->m_pNode->AsMdlNodeEmitter();
            const bool sparkle=emitter && (!_stricmp(part->Name(),"Sparkles") || !_stricmp(part->Name(),"Sparks"));
            const bool darkSmoke=emitter && !view.friendly && !_stricmp(emitter->m_szTexture,"fx_Smoke") && _stricmp(part->Name(),"Evil_stream");
            if(sparkle || darkSmoke) {
                static_cast<Scene*>(view.m_scene.m_pScene)->UnlistSpecialParts(part);++smoke;
            }
            else if(emitter)++preserved;
            for(int child=0;child<part->m_nChildren;++child)self(self,part->m_paChildren[child]);
        };suppress(suppress,alignment->m_pPart);
        if(std::getenv("K1_ENGAGE_SANDBOX_TEST")) {
            static bool logged[2]={false,false};
            if(!logged[view.friendly]) {
                fprintf(stderr,"K1_SANDBOX_ALIGNMENT_PASS friendly=%d animation=%s smoke_emitters_removed=%d glow_emitters_retained=%d native_light=1\n",view.friendly,view.friendly?"good":"evil",smoke,preserved);
                logged[view.friendly]=true;
            }
        }
        creature=std::make_unique<CSWCCreature>();
        auto* stats=source->m_pStats;
        CCAppearanceInfo appearance=*reinterpret_cast<CCAppearanceInfo*>(creature->m_pCreatureAppearance);
        auto& a=appearance.core;
        a.m_nAppearanceId=source->m_appearance.m_headId;
        a.m_nGender=stats->m_nGender;a.m_nPhenotype=stats->m_nPhenotype;
        a.m_nHeadNumber=stats->m_nAppearanceHead;
        a.m_nSkinColor=stats->m_nColorSkin;a.m_nHairColor=stats->m_nColorHair;
        a.m_nTattooColor1=stats->m_nColorTattoo1;a.m_nTattooColor2=stats->m_nColorTattoo2;
        a.m_bUseBackupHead=stats->m_nUseBackupHead;
        a.m_oidHeadItem=OBJECT_INVALID;
        a.m_bArmorEquipped=0;
        if(auto* armor=source->m_pInventory->GetItemInSlot(CSWInventory::SLOT_BODY)) {
            a.m_bArmorEquipped=1;a.m_bTypeVariant=armor->m_nBodyVariation;a.m_bTypeNumber=armor->m_nTextureVar;
        }
        if(look)SandboxCosmeticInfo(appearance,look);
        creature->m_pStats->m_nAppearanceType=a.m_nAppearanceId;
        creature->m_pStats->m_alignmentGoodEvil=stats->m_nAlignment;
        if(!creature->SetAppearance(&appearance,0x0f,1,0))return false;
        creature->AttachToScene(view.m_scene.m_pScene);
        creature->SetPosition(Vector(0,0,0));creature->SetOrientation(Vector(0,-1,0));
        auto* body=creature->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
        if(!body || !body->m_pModel)return false;
        for(auto* part:{body,creature->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1)})if(part) {
            for(const char* clip:{"good","evil","neutral"})part->PlayAnimation(const_cast<char*>(clip),1.f,AnimRun::kAnimRemove,0.f);
            if(!part->PlayAnimation(const_cast<char*>("pause1"),1.f,AnimRun::kAnimNoBlendIn,0.f))
                part->PlayAnimation(const_cast<char*>("cpause1"),1.f,AnimRun::kAnimNoBlendIn,0.f);
        }
        for(int slot:{CSWInventory::SLOT_HEAD,CSWInventory::SLOT_RIGHTWEAPON,CSWInventory::SLOT_LEFTWEAPON}) {
            auto* item=source->m_pInventory->GetItemInSlot(slot);if(!item)continue;
            if(slot==CSWInventory::SLOT_RIGHTWEAPON && item->m_baseItem==BASE_ITEM_GAMMOREAN_BATTLEAXE && body->GetPart(const_cast<char*>("waraxe_001")))continue;
            if(slot==CSWInventory::SLOT_LEFTWEAPON && item==source->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON))continue;
            auto* base=g_pRules->GetBaseItem(item->m_baseItem);if(!base)continue;
            Gob* parent=body;const char* hook=slot==CSWInventory::SLOT_RIGHTWEAPON?"rhand":"lhand";
            if(slot==CSWInventory::SLOT_HEAD) {
                parent=creature->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1);hook="GoggleHook";
                if(!parent || !parent->GetPart(const_cast<char*>(hook)))continue;
            }
            // A full-body mask/weapon model uses native attachment hooks; armor
            // is already part of SetAppearance's body rebuild above.
            CResRef resource=base->GetModelResRef(item->m_nModelVariation,a.m_nGender?'F':'M');
            if(!g_pExoResMan->Exists(resource,RESTYPE_MDL,static_cast<unsigned int*>(nullptr)))resource=base->m_defaultModelResRef;
            CExoString name;resource.CopyToString(name);
            if(auto* model=view.m_scene.AddModel(name,-1)) {
                model->AttachToObject(parent,const_cast<char*>(hook),0);
                if(slot==CSWInventory::SLOT_HEAD)model->PlayAnimation(const_cast<char*>("default"),1.f,0,0.f);
                else {
                    float weaponScale=1.f;
                    if(!g_pRules->m_p2DArrays->m_pAppearance->GetFLOATEntry(a.m_nAppearanceId,CExoString("WEAPONSCALE"),&weaponScale))weaponScale=1.f;
                    model->SetObjectScale(weaponScale,1);
                    if(SaberType(item->m_baseItem))model->PlayAnimation(const_cast<char*>("powered"),1.f,AnimRun::kAnimNoBlendIn,0.f);
                }
            }
        }
        // Use native deformed geometry, not a shared humanoid-height guess.
        // This includes the attached head, masks and solid held weapons.
        view.m_scene.m_pScene->Animate(0.f);
        std::vector<K1SaberContact::Triangle> triangles;
        CollisionPartTriangles(body->m_pPart,triangles);
        if(auto* head=creature->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1))CollisionPartTriangles(head->m_pPart,triangles);
        for(int i=0;i<view.m_scene.m_aModels.m_count;++i)if(auto* object=static_cast<Gob*>(view.m_scene.GetModel(i))) {
            if(object==alignment)continue; // Lighting effects do not frame the creature.
            CollisionPartTriangles(object->m_pPart,triangles);
            auto bladeBounds=[&](auto&& self,Part* part)->void {
                if(!part)return;
                if(auto* node=part->m_pNode->AsMdlNodeLightsaber())if(node->m_paPositions) {
                    const auto& a=node->m_paPositions[1];const auto& b=node->m_paPositions[2];
                    const Vector x=PartPointWorld(part,Vector(a.x,a.y,a.z)),y=PartPointWorld(part,Vector(b.x,b.y,b.z));
                    triangles.push_back({x,y,y});
                }
                for(int child=0;child<part->m_nChildren;++child)self(self,part->m_paChildren[child]);
            };bladeBounds(bladeBounds,object->m_pPart);
        }
        if(triangles.empty())return false;
        Vector low=triangles[0].a,high=low;
        for(const auto& triangle:triangles)for(const auto& p:{triangle.a,triangle.b,triangle.c}) {
            low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);low.z=std::min(low.z,p.z);
            high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);high.z=std::max(high.z,p.z);
        }
        const float aspect=float(view.m_extent.w)/std::max(1,view.m_extent.h);
        // Fit actual projected vertices to 94% of the window instead of adding
        // broad AABB padding, keeping the full body and held weapons visible.
        float distance=.5f;const float tangent=std::tan(16.f*3.141592654f/180.f);
        for(const auto& triangle:triangles)for(const auto& p:{triangle.a,triangle.b,triangle.c}) {
            distance=std::max(distance,std::fabs(p.z-(low.z+high.z)*.5f)/(tangent*.94f)-(p.y-low.y));
            distance=std::max(distance,std::fabs(p.x-(low.x+high.x)*.5f)/(tangent*aspect*.94f)-(p.y-low.y));
        }
        const Vector eye((low.x+high.x)*.5f,low.y-distance,(low.z+high.z)*.5f);
        view.m_scene.m_pCamera->SetPosition(eye);
        view.m_scene.m_pCamera->SetOrientation(Quaternion(.70710678f,.70710678f,0,0));
        view.m_scene.m_pCamera->SetViewAngle(32.f);
        float maximum=0;
        for(const auto& triangle:triangles)for(const auto& p:{triangle.a,triangle.b,triangle.c}) {
            const float depth=p.y-eye.y;
            maximum=std::max(maximum,std::fabs(p.z-eye.z)/(depth*std::tan(16.f*3.141592654f/180.f)));
            maximum=std::max(maximum,std::fabs(p.x-eye.x)/(depth*std::tan(16.f*3.141592654f/180.f)*aspect));
        }
        if(!std::isfinite(maximum) || maximum>=.99f)return false;
        if(std::getenv("K1_ENGAGE_SANDBOX_TEST"))fprintf(stderr,"K1_SANDBOX_PREVIEW_FIT_PASS model=%s triangles=%zu normalized_extent=%g\n",body->m_pModel->m_szName,triangles.size(),maximum);
        view.m_tail.bVisible=true;
        return true;
    }
};
