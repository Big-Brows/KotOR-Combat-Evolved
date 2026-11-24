// Intentional combat tuning; native character stats and quest data stay native.
struct NpcCombatSettings {int poise=100,parry=20,stance=0;bool matched=false;};
NpcCombatSettings NpcHpPolicy(int hp){NpcCombatSettings p;p.poise=hp<50?60:hp<75?80:100;p.parry=hp<50?10:hp<75?15:20;return p;}
NpcCombatSettings NpcCombatPolicy(CSWSCreature* actor,const char* moduleContext=nullptr) {
    NpcCombatSettings result;
    if(!actor || actor==ServerActor())return result;
    const int hp=actor->GetMaxHitPoints(0);
    result=NpcHpPolicy(hp);
    // Native K1 Gamorrean appearance rows (including the three native variants).
    const int appearance=actor->m_appearance.m_headId;
    if(appearance==68 || appearance==240 || appearance==241 || appearance==242)result.stance=1;
    struct Row {std::string alias,module,source,tag;int hp,level,gender,cls,right,left;NpcCombatSettings settings;};
    static std::vector<Row> rows;
    if(rows.empty() && g_pExoResMan->Exists(CResRef("k1npccombat"),RESTYPE_2DA,static_cast<unsigned int*>(nullptr))) {
        C2DA table(CResRef("k1npccombat"),0);
        if(table.Load2DArray())for(int i=0;i<table.m_nRows;++i) {
            CExoString alias,module,source,tag;Row r;
            if(!table.GetCExoStringEntry(i,CExoString("alias"),&alias) || !table.GetCExoStringEntry(i,CExoString("module"),&module) ||
               !table.GetCExoStringEntry(i,CExoString("source"),&source) || !table.GetCExoStringEntry(i,CExoString("tag"),&tag) ||
               !table.GetINTEntry(i,CExoString("hp"),&r.hp) || !table.GetINTEntry(i,CExoString("level"),&r.level) ||
               !table.GetINTEntry(i,CExoString("poise"),&r.settings.poise) || !table.GetINTEntry(i,CExoString("parry"),&r.settings.parry) ||
               !table.GetINTEntry(i,CExoString("stance"),&r.settings.stance) ||
               !table.GetINTEntry(i,CExoString("gender"),&r.gender) || !table.GetINTEntry(i,CExoString("cls"),&r.cls) ||
               !table.GetINTEntry(i,CExoString("right"),&r.right) || !table.GetINTEntry(i,CExoString("left"),&r.left))continue;
            r.settings.matched=true;r.alias=alias.CStr();r.module=module.CStr();r.source=source.CStr();r.tag=tag.CStr();rows.push_back(r);
        }
    }
    auto* origin=actor->m_scriptVars.MatchIndex(CExoString("K1_ENGAGE_SOURCE"),CSWSScriptVarTable::VARTYPE_STRING,0);
    const char* source=origin && origin->m_pString?origin->m_pString->CStr():"";
    auto* module=g_pAppManager->m_pServerExoApp->GetModule();
    CExoString moduleResource=module?module->GetModuleResourceName():CExoString("");
    CExoString savedModule=actor->m_scriptVars.GetString(CExoString("K1_ENGAGE_SOURCE_MODULE"));
    const char* moduleName=moduleContext?moduleContext:savedModule.CStr()[0]?savedModule.CStr():moduleResource.CStr();
    const Row* global=nullptr;
    if(*source)for(const auto& r:rows) {
        if(!_stricmp(source,r.alias.c_str()))return r.settings;
        if(!_stricmp(source,r.source.c_str())) {
            if(!_stricmp(moduleName,r.module.c_str()))return r.settings;
            if(r.module=="global")global=&r;
        }
    }
    if(global)return global->settings;
    auto* right=actor->m_pInventory?actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_RIGHTWEAPON):nullptr;
    auto* left=actor->m_pInventory?actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON):nullptr;
    const Row* chosen=nullptr;
    for(const auto& r:rows) {
        if(*source && !_stricmp(source,r.alias.c_str()))return r.settings;
        if(_stricmp(actor->m_sTag.CStr(),r.tag.c_str()) || hp!=r.hp || actor->m_pStats->GetLevel(0)!=r.level ||
           actor->m_pStats->m_nGender!=r.gender || actor->m_pStats->GetClass(0)!=r.cls ||
           (right?right->m_baseItem:-1)!=r.right || (left?left->m_baseItem:-1)!=r.left ||
           (_stricmp(moduleName,r.module.c_str()) && r.module!="global") || (*source && _stricmp(source,r.source.c_str())))continue;
        // Old saves may lack the source local; only adopt an unambiguous policy.
        if(chosen && (chosen->settings.parry!=r.settings.parry || chosen->settings.stance!=r.settings.stance))return result;
        chosen=&r;
    }
    return chosen?chosen->settings:result;
}
