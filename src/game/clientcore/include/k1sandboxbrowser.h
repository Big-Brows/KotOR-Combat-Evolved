// Browsing structure points at unchanged native catalog templates. Appearances
// are actual equivalent source templates, never stat overrides on another NPC.
struct K1SandboxVariant {std::string key;std::vector<int> looks;int source=-1;};
struct K1SandboxType {std::string label;int group=-1;std::vector<K1SandboxVariant> variants;int order=0;};
struct K1SandboxFaction {std::string label;std::vector<K1SandboxType> types;};
std::vector<K1SandboxFaction> SandboxBrowser() {
    const char* names[]={"Black Vulkar","Bounty Hunter","Czerka","Dark Jedi","Genoharadan","Hidden Bek","Jedi","Mandalorian","Mercenary","Miscellaneous","Rakata","Republic","Sand People (Tuskans)","Sith","Thugs","Trandoshans","Wookiee","Gamorrean"};
    std::vector<K1SandboxFaction> result;for(auto* name:names)result.push_back({name,{}});
    if(g_pExoResMan->Exists(CResRef("k1npcmenu"),RESTYPE_2DA,static_cast<unsigned int*>(nullptr))) {
        C2DA table(CResRef("k1npcmenu"),0);
        if(table.Load2DArray())for(int row=0;row<table.m_nRows && row<16000;++row) {
            CExoString faction,label,source,look;int number=0;
            if(!table.GetCExoStringEntry(row,CExoString("faction"),&faction) || !table.GetCExoStringEntry(row,CExoString("label"),&label) ||
               !table.GetCExoStringEntry(row,CExoString("source"),&source) || !table.GetCExoStringEntry(row,CExoString("look"),&look) || !table.GetINTEntry(row,CExoString("variant"),&number))continue;
            int g=-1,s=-1,a=-1;
            for(int i=0;i<int(sandboxNativeGroups.size());++i)for(int j=0;j<int(sandboxNativeGroups[i].variants.size());++j) {
                const auto& entry=sandboxNativeGroups[i].variants[j];
                if(entry.creature==source.CStr()){g=i;s=j;}
            }
            if(g<0)continue;
            for(int j=0;j<int(sandboxNativeGroups[g].variants.size());++j)if(sandboxNativeGroups[g].variants[j].creature==look.CStr())a=j;
            if(a<0)continue;
            for(auto& f:result)if(f.label==faction.CStr()) {
                auto t=std::find_if(f.types.begin(),f.types.end(),[&](const auto& value){return value.label==label.CStr();});
                if(t==f.types.end()){f.types.push_back({label.CStr(),g,{}});t=f.types.end()-1;}
                while(int(t->variants.size())<=number)t->variants.push_back({std::to_string(t->variants.size()),{}});
                t->variants[number].source=s;t->variants[number].looks.push_back(a);
            }
        }
    } else
    for(int g=0;g<int(sandboxNativeGroups.size());++g) {
        const auto& group=sandboxNativeGroups[g];
        for(int i=0;i<int(group.variants.size());++i) {
            const auto& source=group.variants[i];
            for(auto& faction:result)if(faction.label==source.faction) {
                auto type=std::find_if(faction.types.begin(),faction.types.end(),[&](const auto& t){return t.group==g;});
                if(type==faction.types.end()){faction.types.push_back({group.label,g,{}});type=faction.types.end()-1;}
                type->order=source.typeOrder;
                auto variant=std::find_if(type->variants.begin(),type->variants.end(),[&](const auto& v){return v.key==source.variantKey;});
                if(variant==type->variants.end()){type->variants.push_back({source.variantKey,{}});variant=type->variants.end()-1;}
                bool duplicate=false;
                for(int look:variant->looks)if(group.variants[look].look==source.look)duplicate=true;
                if(!duplicate)variant->looks.push_back(i);
            }
        }
    }
    for(auto& faction:result)std::stable_sort(faction.types.begin(),faction.types.end(),[](const auto& a,const auto& b){return a.order<b.order;});
    // Preserve the five existing authored fixtures without mixing their stats
    // into native Dark Jedi variants.
    if(std::getenv("K1_ENGAGE_SANDBOX_TEST") || std::getenv("K1_ENGAGE_SANDBOX_DUAL_TEST"))
        result[3].types.insert(result[3].types.begin(),{"Dark Jedi (custom)",-1,{{"yellow",{0}},{"staff",{0}},{"dual",{0}},{"red",{0}},{"blue",{0}}}});
    return result;
}
