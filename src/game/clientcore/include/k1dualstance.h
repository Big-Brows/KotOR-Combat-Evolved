// Included inside the Engage implementation namespace. Dual-specific selection
// reuses native weapon geometry, posed skin, contact queue and damage services.
void SetWorldOrientation(Part* part,Quaternion orientation);
bool DualWalkSpineReference(Part* joint,const std::vector<BodyPose>& pose,Quaternion& orientation) {
    std::vector<Part*> chain;
    for(Part* part=joint;part;part=part->m_pParent)chain.push_back(part);
    orientation=Quaternion();
    for(auto it=chain.rbegin();it!=chain.rend();++it) {
        auto found=std::find_if(pose.begin(),pose.end(),[&](const BodyPose& p){return p.part==*it;});
        if(found==pose.end())return false;
        orientation=normalize(orientation*found->orientation);
    }
    return true;
}
void StabilizeDualWalkingTorso(Gob* body,const std::vector<BodyPose>& reference) {
    if(!body)return;
    // Retain the gait's pelvis, legs and arm swing. Hold only the three spine
    // rotations in the standing model frame, independent of locomotion yaw.
    for(const char* name:{"torso_g","torsoMid_g","torsoUpr_g"}) {
        Part* joint=body->GetPart(const_cast<char*>(name));Quaternion orientation;
        if(joint && DualWalkSpineReference(joint,reference,orientation))
            SetWorldOrientation(joint,normalize(body->m_qOrientation*orientation));
    }
}
void ApplyDualMovingGrip(Gob* body,const std::vector<BodyPose>& reference,bool walking) {
    if(!body || reference.empty())return;
    // Keep the established full gait and its wrist motion. Only the native
    // weapon attachment/finger grip follows each independent held saber.
    for(const auto& pose:reference) {
        const char* name=pose.part->m_pNode->TranslateNodeName();
        if(!strcmp(name,"lhand") || !strcmp(name,"rhand") || strstr(name,"Fngr") || strstr(name,"Thumb")) {
            pose.part->m_vLocalPosition=pose.position;
            pose.part->m_qLocalOrientation=pose.orientation;
        }
    }
    if(!walking)return;
    for(bool left:{false,true}) {
        Part* wrist=body->GetPart(const_cast<char*>(left?"Lhand_g":"Rhand_g"));
        Part* grip=body->GetPart(const_cast<char*>(left?"lhand":"rhand"));
        if(!wrist || !grip)continue;
        Vector wristP;Quaternion wristQ;
        wrist->GetWorld(wristP,wristQ);
            // The gait's wrist basis was authored for one weapon. Rotate the
            // actual wrist (including fingers and attachment), never just the
            // weapon marker, so both independent grips point ahead of the legs.
            Vector gripP;Quaternion gripQ;
            grip->GetWorld(gripP,gripQ);
            const Vector forward=body->m_qOrientation.ydir();
            const Vector desired=normalize(forward+Vector(0,0,.35f));
            SetWorldOrientation(wrist,normalize(RotationArc(gripQ.zdir(),desired)*wristQ));
    }
}
void ApplyDualLocomotionGrip(Gob* body) {
    if(!state.Guarding())ApplyDualMovingGrip(body,nativeDualGuardPose,currentBase && strstr(currentBase,"walk"));
}
bool DualBlade(CSWCCreature* actor,int hand,K1SaberContact::Blade& blade) {
    auto* item=g_pAppManager->m_pClientExoApp->GetItemByGameObjectID(actor->m_aEquippedItemIds[4+hand]);
    if(item && (actor==Actor() || NativeSandboxActor(actor->GetServerCreature())) && NativeNpcBluntType(item->m_baseItem))
        return NativeNpcSolidBlade(actor,blade,hand);
    auto* weapon=actor->GetModel(hand?CSWCAnimBase::ANIM_MODEL_LEFT_ITEM_BODY:CSWCAnimBase::ANIM_MODEL_RIGHT_ITEM_BODY,1);
    if(!item || (!item->m_bPowered && !SwordType(item->m_baseItem)) || !weapon || !weapon->m_pPart) return false;
    if(SwordType(item->m_baseItem)) {
        std::vector<K1SaberContact::Blade> blades;
        if(!SolidSwordBlades(weapon,blades))return false;
        blade=blades[0];return true;
    }
    bool found=false;
    auto visit=[&](auto&& self,Part* part)->void {
        if(!found) if(auto* node=part->m_pNode->AsMdlNodeLightsaber()) if(part->m_bDrawEnabled && node->m_paPositions) {
            auto a=node->m_paPositions[1],b=node->m_paPositions[2];
            blade={PartPointWorld(part,Vector(a.x,a.y,a.z)),PartPointWorld(part,Vector(b.x,b.y,b.z))};
            found=magnitude(blade.tip-blade.base)>.0001f;
        }
        for(int i=0;i<part->m_nChildren;++i) self(self,part->m_paChildren[i]);
    };visit(visit,weapon->m_pPart);
    return found;
}

void TraceDualBlades(CSWCCreature* actor,float time) {
    struct History {K1SaberContact::Blade blade;float time=0;unsigned long long weapon=OBJECT_INVALID;bool valid=false;};
    static History previous[2];
    if(dualContactSerial!=attackSerial) {dualContactSerial=attackSerial;dualContactHands=0;previous[0].valid=previous[1].valid=false;}
    auto* server=actor->GetServerCreature();
    auto* area=server?static_cast<CSWSArea*>(server->GetArea()):nullptr;
    const float start=PlayerHitStart(swingDirection),end=ContactEnd(PlayerHitEnd(swingDirection));
    if(SwingSoundDue(attackSerial,playerPlayback.swingSoundSerial,time,start)) EngageWeaponSound(server,nullptr,"swing");
    struct Hit {MeleeContact contact;float phase;};
    std::vector<Hit> hits;
    for(int hand=0;hand<2;++hand) {
        auto& prior=previous[hand];K1SaberContact::Blade blade;
        if(!DualBlade(actor,hand,blade)) {prior.valid=false;continue;}
        const auto weapon=actor->m_aEquippedItemIds[4+hand];
        auto* nativeItem=server->m_pInventory->GetItemInSlot(hand?CSWInventory::SLOT_LEFTWEAPON:CSWInventory::SLOT_RIGHTWEAPON);
        if(!nativeItem) {prior.valid=false;continue;}
        const bool reset=!hadBlade || !prior.valid || prior.weapon!=weapon || time<prior.time || magnitude(blade.base-prior.blade.base)>2.f;
        if(reset)
            prior={blade,time,weapon,true};
        float lo=0,hi=1;
        const bool active=K1SaberContact::ClipTime(prior.time,time,start,end,lo,hi);
        auto begin=K1SaberContact::Between(prior.blade,blade,lo),finish=K1SaberContact::Between(prior.blade,blade,hi);
        MeleeContact selected={};float firstPhase=2;
        if(active && area && !(dualContactHands&(1u<<hand))) for(int i=0;i<area->m_aObjectsByX.m_count;++i) {
            auto* target=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);
            if(!target || target==server || target->GetDead() || target->GetReputation(server->m_objectId,0)>10) continue;
            auto* client=target->GetClientCreature();
            auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
            if(!body || magnitude(body->m_vPosition-server->m_position)>10.f+body->m_fMinSphereRadius)continue;
            std::vector<K1SaberContact::Triangle> triangles;std::vector<Part*> parts;
            CollisionTriangles(body,triangles,&parts);
            CollisionTriangles(client->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1),triangles,&parts);
            const auto relative=RelativeContactStart(target->m_objectId,prior.blade,!reset);
            begin=K1SaberContact::Between(relative,blade,lo);finish=K1SaberContact::Between(relative,blade,hi);
            const int steps=K1SaberContact::Steps(begin,finish);
            bool hit=false;
            for(int step=0;step<steps && !hit;++step) {
                const float a=float(step)/steps,b=float(step+1)/steps;
                auto x=K1SaberContact::Between(begin,finish,a),y=K1SaberContact::Between(begin,finish,b);
                for(size_t ti=0;ti<triangles.size();++ti) if(K1SaberContact::SweepCell(x,y,triangles[ti])) {
                    if(a<firstPhase) {
                        firstPhase=a;selected={server->m_objectId,target->m_objectId,attackSerial};
                        selected.blade=hand;selected.offhand=hand!=0;selected.dualWeapon=nativeItem->m_objectId;
                        if(K1SaberContact::SweepPoint(x,y,triangles[ti],selected.hitPoint)) {
                            selected.hitPart=parts[ti];selected.bladeDirection=y.tip-y.base;
                            const float fraction=std::max(0.f,std::min(1.f,dot(selected.hitPoint-y.base,y.tip-y.base)/dot(y.tip-y.base,y.tip-y.base)));
                            const auto wx=K1SaberContact::Between(prior.blade,blade,lo+(hi-lo)*a),wy=K1SaberContact::Between(prior.blade,blade,lo+(hi-lo)*b);
                            selected.bladeDirection=wy.tip-wy.base;
                            selected.bladeMotion=(wy.base-wx.base)*(1-fraction)+(wy.tip-wx.tip)*fraction;
                        }
                    }
                    hit=true;break;
                }
            }
        }
        if(firstPhase<=1) hits.push_back({selected,firstPhase});
        prior={blade,time,weapon,true};
    }
    std::stable_sort(hits.begin(),hits.end(),[](const Hit& a,const Hit& b){return a.phase<b.phase;});
    for(auto& hit:hits) {
        auto& contact=hit.contact;
        if(ContactTimingEnabled())contact.wallAt=ContactWallUs();
        auto* target=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(contact.target);
        contact.dismemberRow=SampleDismemberment(server,target,contact);contact.hitPart=nullptr;
        dualContactHands|=1u<<contact.blade;
        sceneMeleeContacts.push_back(contact);hitConsumed=true;
        fprintf(stderr,"K1_DUAL_CONTACT hand=%d serial=%d weapon=%llu time=%g phase=%g\n",contact.blade,attackSerial,contact.dualWeapon,time,hit.phase);
    }
    hadBlade=true;
    previousBladeTime=time;
}
