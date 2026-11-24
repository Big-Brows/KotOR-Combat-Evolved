// Last rendered body roots. Animation skinning remains native; this history
// only accounts for rigid target movement during a blade's sampled interval.
struct ContactBodyRoot {Gob* body;Model* model;CAurScene* scene;Vector position;Quaternion rotation;float scale;};
std::unordered_map<unsigned long long,ContactBodyRoot> contactBodyRoots,contactCurrentRoots;
bool BeginContactBodyFrame(CAurScene* scene) {
    auto* actor=ServerActor();auto* client=Actor();
    auto* body=client?client->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
    if(!actor || !actor->GetArea() || !body || body->m_pScene!=scene){contactCurrentRoots.clear();return false;}
    contactCurrentRoots.clear();auto* area=static_cast<CSWSArea*>(actor->GetArea());
    for(int i=0;i<area->m_aObjectsByX.m_count;++i) {
        auto* target=g_pAppManager->m_pServerExoApp->GetCreatureByGameObjectID(area->m_aObjectsByX.m_pArray[i]);
        auto* owner=target?target->GetClientCreature():nullptr;
        auto* model=owner?owner->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
        if(model && model->m_pModel && model->m_pScene==scene)
            contactCurrentRoots[target->m_objectId]={model,model->m_pModel,static_cast<Scene*>(scene),model->m_vPosition,model->m_qOrientation,model->m_fObjectScale};
    }
    return true;
}
K1SaberContact::Blade RelativeContactStart(unsigned long long target,const K1SaberContact::Blade& previous,bool valid=true) {
    auto old=contactBodyRoots.find(target),now=contactCurrentRoots.find(target);
    if(!valid || old==contactBodyRoots.end() || now==contactCurrentRoots.end())return previous;
    const auto& a=old->second;const auto& b=now->second;
    if(a.body!=b.body || a.model!=b.model || a.scene!=b.scene || std::fabs(a.scale-b.scale)>.00001f)return previous;
    const float moved=magnitude(b.position-a.position),turn=std::fabs(dot(normalize(a.rotation),normalize(b.rotation)));
    if(!std::isfinite(moved+turn) || moved>2.f || (moved<.000001f && turn>.9999999f))return previous;
    return K1SaberContact::TargetRelative(previous,a.position,a.rotation,b.position,b.rotation);
}
