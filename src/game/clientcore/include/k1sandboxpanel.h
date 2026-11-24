// Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
// Updated for KotOR:CE v0.2 on 2026-11-24; see docs/LOCAL_CHANGES.md.
#include "k1sandboxbrowser.h"
#include "k1sandboxpreview.h"
struct K1SandboxMenuMemory {bool valid=false,friendly=false;int faction=-1,type=-1,variant=0,look=0,color=0,combat=0,factionTop=0,typeTop=0,focus=0;CExoString status;};
K1SandboxMenuMemory sandboxMenuMemory;
class K1SandboxList final:public CSWGuiListBox {
public:
    void SetActive(int flag) override {
        // Focus must not choose row zero or scroll the list under a click.
        // Actual mouse/keyboard selection remains handled by the native list.
        m_tail.bActive=flag;
        if(flag){const bool follow=bScrollToSelection;bScrollToSelection=0;OrganizeControls();bScrollToSelection=follow;}
    }
};
// Native modal controls for the isolated K1 sandbox.
class K1SandboxPanel final : public CSWGuiPanel {
public:
    enum Action {StanceLeft,StanceRight,AppearanceLeft,AppearanceRight,Team,Ai,TargetPlayer,Spawn,Close,ColorLeft,ColorRight,CombatLeft,CombatRight,Count};
    CSWGuiButton buttons[Count];
    K1SandboxList factions,types;
    CSWGuiLabel title,variantLabel,appearanceLabel,statsLabel,status,colorLabel,combatLabel;
    K1SandboxPreview preview;
    std::vector<K1SandboxFaction> browser;
    int faction=-1,type=-1,stance=0,npc=0,variant=0,look=0;
    int saberColor=0,combatStance=0;bool hasSaber=false,singleSaber=false,staffSaber=false,dualSaber=false;
    bool friendly=false,wasActive=false,wasPaused=false;
    float scale=1,fontScale=1;
    unsigned long long areaId=OBJECT_INVALID;
    CSWGuiBorderParams normal{CResRef(),CResRef(),CResRef(),0,0,0,Vector(1,1,1),0};
    CSWGuiBorderParams selected{CResRef(),CResRef(),CResRef(),0,0,0,Vector(1,1,1),0};
    CSWGuiBorderParams none{CResRef(),CResRef(),CResRef(),0,0,0,Vector(1,1,1),0};
    CSWGuiTextParams text;
    std::string selectedIndicator,combatTemplate;
    CSWGuiExtent Rect(int x,int y,int w,int h){return CSWGuiExtent(int(x*scale),int(y*scale),int(w*scale),int(h*scale));}
    void Button(CSWGuiButton& button,const char* name,const CSWGuiExtent& extent) {
        text.SetText(CExoString(name));button.Initialize(extent,text,normal,selected);
        button.m_tail.bFocusOnHover=false;
        button.AddEvent(INPUTEVENT_GUI_A,this,static_cast<void(CSWGuiObject::*)(CSWGuiControl*)>(&K1SandboxPanel::Click));
    }
    CSWGuiButton* Row(const char* name,int width) {
        auto* row=new CSWGuiButton();Button(*row,name,Rect(0,0,width,38));
        auto extent=row->m_extent;extent.h=std::max(extent.h,row->m_text.GetIdealHeight()+int(8*scale));row->SetExtent(extent);
        return row;
    }
    void FitLabel(CSWGuiLabel& label) {
        if(!label.m_text.m_pString)return;
        float size=fontScale;
        for(int i=0;i<8;++i) {
            label.m_text.m_pString->SetFontScale(size);label.m_text.WrapString();
            if(label.m_text.GetIdealHeight()<=label.m_extent.h)break;
            size*=.95f;
        }
    }
    void HighlightSelections() {
        auto highlight=[](CSWGuiListBox& list,int index){for(int i=0;i<list.m_aControls.m_count;++i)if(auto* row=list.GetControl(i)->AsButton()) {
            const bool chosen=i==index;row->m_text.m_params.SetColor(chosen?Vector(1.f,.85f,.05f):Vector(0.f,.66f,.98f));
            const CSWGuiColor tint=chosen?CSWGuiColor(1.f,.85f,.05f):CSWGuiColor(.934f,1.f,.934f);
            row->m_border.m_params.m_color=tint;row->m_borderHilight.m_params.m_color=tint;
        }};highlight(factions,faction);highlight(types,type);
    }
    explicit K1SandboxPanel(CSWGuiManager* manager):CSWGuiPanel(manager),browser(SandboxBrowser()) {
        wasActive=K1EngageEngaged();wasPaused=g_pAppManager->m_pServerExoApp->GetPauseState(2)!=0;
        if(ServerActor() && ServerActor()->GetArea())areaId=static_cast<CSWSArea*>(ServerActor()->GetArea())->m_objectId;
        int width=0,height=0;AurGUIGetScreenSize(width,height);
        scale=std::min(width/1160.f,height/720.f);
        fontScale=height*1.3f/768.f;
        m_stateFlags|=1;
        SetExtent(CSWGuiExtent((width-int(1120*scale))/2,(height-int(680*scale))/2,int(1120*scale),int(680*scale)));
        normal=CSWGuiBorderParams(CResRef("boxline1"),CResRef("boxline2"),CResRef(),2,6,1.f,Vector(.934f,1,.934f),9);
        selected=CSWGuiBorderParams(CResRef("boxline3"),CResRef("boxline4"),CResRef(),2,6,1.f,Vector(.871f,1,.871f),9);
        none=CSWGuiBorderParams(CResRef(),CResRef(),CResRef(),0,0,0,Vector(1,1,1),0);
        const CSWGuiBorderParams frame(CResRef("boxline1"),CResRef("boxline2"),CResRef("blackfill"),2,6,1.f,Vector(.934f,1,.934f),9);
        m_pBackground->Initialize(Rect(0,0,1120,680),frame);
        text=g_pAppManager->m_pClientExoApp->GetInGameGui()->m_pMainInterface->m_tbPause.m_text.m_params;
        text.SetDefaultColor(Vector(0.f,.66f,.98f));
        text.SetAlignment(SWGUIALIGN_CENTER|SWGUIALIGN_MIDDLE);
        auto label=[&](CSWGuiLabel& control,const char* name,int x,int y,int w,int h){text.SetText(CExoString(name));control.Initialize(Rect(x,y,w,h),text,none,1.f);control.m_tail.bIgnoreMouse=true;AddControl(&control);};
        label(title,"Spawn Menu",250,8,620,40);
        label(variantLabel,"",600,55,460,64);
        label(colorLabel,"",600,110,460,34);label(combatLabel,"",600,151,460,34);
        label(appearanceLabel,"",600,359,460,54);
        label(statsLabel,"",560,417,540,76);
        label(status,"",560,582,540,44);
        const char* names[]={"<",">","<",">","Alignment: Hostile","NPC AI: On","TARGET PLAYER: Yes","SPAWN","Close","<",">","<",">"};
        const int xs[]={558,1062,558,1062,620,20,300,760,1000,558,1062,558,1062};
        const int ys[]={66,66,367,367,500,638,638,542,10,110,110,151,151};
        const int ws[]={38,38,38,38,420,260,360,180,100,38,38,38,38};
        for(int i=0;i<Count;++i){Button(buttons[i],names[i],Rect(xs[i],ys[i],ws[i],36));AddControl(&buttons[i]);}
        preview.view.SetExtent(Rect(650,192,360,161));AddControl(&preview.view);preview.view.m_tail.bVisible=false;
        auto list=[&](CSWGuiListBox& box,int x,int w){
            box.Initialize(Rect(x,55,w,575),normal,4);box.SetSelectOnLeftClick(1);
            CSWGuiExtent scrollbar=Rect(0,0,18,575);
            box.InitializeScrollBar(scrollbar,normal,CResRef("uparrow"),CResRef("bluefill"),0);
            box.AddEvent(SWGUIEVENT_SELECTION_CHANGED,this,static_cast<void(CSWGuiObject::*)(CSWGuiControl*)>(&K1SandboxPanel::OnSelection));
            AddControl(&box);
        };
        list(factions,20,250);list(types,280,270);
        std::vector<CSWGuiControl*> rows;
        for(const auto& f:browser)rows.push_back(Row(f.label.c_str(),222));
        factions.AddControls(rows.data(),int(rows.size()),1,0,1);
        Refresh();SetActiveControl(&factions,0);
        // Activating a native list selects its first row without a change
        // notification. Start unselected so the first mouse click is observable.
        factions.SetSelectedControl(-1,0);
        if(sandboxMenuMemory.valid && sandboxMenuMemory.faction>=0 && sandboxMenuMemory.faction<int(browser.size())) {
            auto memory=sandboxMenuMemory;friendly=memory.friendly;SelectFaction(memory.faction);
            if(memory.type>=0 && memory.type<int(browser[faction].types.size())) {
                SelectType(memory.type);variant=std::min(memory.variant,int(SelectedType()->variants.size())-1);
                look=std::min(memory.look,int(SelectedType()->variants[variant].looks.size())-1);
                saberColor=memory.color;friendly=memory.friendly;UpdatePreview();combatStance=memory.combat;Refresh();
            }
            auto scroll=[](CSWGuiListBox& list,int top){list.SetTopVisible(top);list.bScrollToSelection=0;list.bAnchorTop=1;list.m_nBottomRow=-1;list.OrganizeControls();};
            scroll(factions,memory.factionTop);scroll(types,memory.typeTop);status.m_text.m_params.SetText(memory.status);
            SetActiveControl(memory.focus==0?static_cast<CSWGuiControl*>(&factions):memory.focus==1?static_cast<CSWGuiControl*>(&types):static_cast<CSWGuiControl*>(&buttons[std::min(Count-1,memory.focus-2)]),0);
        }
    }
    K1SandboxType* SelectedType(){return faction>=0 && type>=0?&browser[faction].types[type]:nullptr;}
    const SandboxNativeTemplate* Entry(){auto* t=SelectedType();return t && t->group>=0?&sandboxNativeGroups[t->group].variants[t->variants[variant].source>=0?t->variants[variant].source:t->variants[variant].looks[look]]:nullptr;}
    const SandboxNativeTemplate* AppearanceEntry(){auto* t=SelectedType();return t && t->group>=0?&sandboxNativeGroups[t->group].variants[t->variants[variant].looks[look]]:nullptr;}
    void SelectFaction(int index) {
        faction=index;type=-1;variant=look=0;preview.Clear();
        SetActiveControl(&factions,0);saberColor=combatStance=0;combatTemplate.clear();
        factions.SetSelectedControl(index,1);
        std::vector<CSWGuiControl*> rows;
        for(const auto& t:browser[faction].types)rows.push_back(Row(t.label.c_str(),242));
        types.AddControls(rows.data(),int(rows.size()),1,0,1);
        status.m_text.m_params.SetText(CExoString(""));Refresh();
    }
    void SelectType(int index){SetActiveControl(&types,0);type=index;variant=look=0;saberColor=combatStance=0;combatTemplate.clear();types.SetSelectedControl(index,1);UpdatePreview();Refresh();}
    void SelectCustom(){SelectFaction(3);SelectType(0);}
    void UpdatePreview() {
        preview.Clear();auto* t=SelectedType();if(!t)return;
        const auto* entry=Entry();stance=t->group<0?variant:0;
        const char* resource=entry?entry->creature.c_str():k1DarkJediTypes[stance].creature;
        std::unique_ptr<CSWSCreature> actor(new CSWSCreature(OBJECT_INVALID,0));
        if(!actor->LoadFromTemplate(CResRef(resource),0)) {statsLabel.m_text.m_params.SetText(CExoString("NPC resources unavailable."));return;}
        if(!entry && stance==1)EquipFixtureWeapon(actor.get(),"g_w_drkjdisbr002");
        if(!entry && stance==2)EquipFixtureWeapon(actor.get(),"g_w_lghtsbr02",CSWInventory::SLOT_LEFTWEAPON);
        auto* right=NativeNpcRightItem(actor.get());auto* left=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON);
        hasSaber=right && SaberType(right->m_baseItem);
        staffSaber=hasSaber && right->m_baseItem==BASE_ITEM_DOUBLE_BLADED_LIGHTSABER;
        dualSaber=hasSaber && left && left!=right && SaberType(left->m_baseItem);
        singleSaber=hasSaber && !staffSaber && !dualSaber;
        SandboxSaberColor(actor.get(),saberColor);
        selectedIndicator=entry?entry->indicator:"";
        if(entry) {
            auto* right=NativeNpcRightItem(actor.get());auto* left=actor->m_pInventory->GetItemInSlot(CSWInventory::SLOT_LEFTWEAPON);
            const bool staff=right && (DoubleSwordType(right->m_baseItem) || right->m_baseItem==BASE_ITEM_DOUBLE_BLADED_LIGHTSABER || right->m_baseItem==BASE_ITEM_QUARTER_STAFF || right->m_baseItem==BASE_ITEM_GHAFFI_STICK);
            const bool dual=right && left && right!=left && (SingleMeleeType(right->m_baseItem) || right->m_baseItem==BASE_ITEM_STUN_BATON) && (SingleMeleeType(left->m_baseItem) || left->m_baseItem==BASE_ITEM_STUN_BATON);
            selectedIndicator+=!NativeNpcMeleeEquipped(actor.get())?" (Native ranged)":staff?" (Stance: Double-blade)":dual?" (Stance: Dual sabers)":" (Stance: Yellow)";
        }
        auto* s=actor->m_pStats;
        if(combatTemplate!=resource){combatTemplate=resource;combatStance=NpcCombatPolicy(actor.get()).stance;}
        char stats[384];snprintf(stats,sizeof(stats),"Level: %d    HP: %d/%d    Force: %d/%d\nSTR %d  DEX %d  CON %d  INT %d  WIS %d  CHA %d\nFeats: %d    Max Poise: %d%%    Parry Chance: %d%%",s->GetLevel(0),actor->GetCurrentHitPoints(0),actor->GetMaxHitPoints(0),s->m_nCurrentForcePoints,actor->GetMaxForcePoints(),s->m_nSTRBase,s->m_nDEXBase,s->m_nCONBase,s->m_nINTBase,s->m_nWISBase,s->m_nCHABase,s->m_feats.m_count,NpcCombatPolicy(actor.get()).poise,NpcParryPercent(actor.get()));
        statsLabel.m_text.m_params.SetText(CExoString(stats));
        std::unique_ptr<CSWSCreature> appearance;
        if(auto* choice=AppearanceEntry()){appearance.reset(new CSWSCreature(OBJECT_INVALID,0));if(!appearance->LoadFromTemplate(CResRef(choice->creature.c_str()),0))appearance.reset();}
        preview.view.friendly=friendly;
        const bool ok=preview.Set(actor.get(),appearance.get());
        if(!ok)status.m_text.m_params.SetText(CExoString("Character preview unavailable."));
        fprintf(stderr,"K1_SANDBOX_PREVIEW_%s template=%s appearance=%d variant=%d look=%d level=%d hp=%d feats=%d\n",ok?"PASS":"FAIL",resource,actor->m_appearance.m_headId,variant,look,s->GetLevel(0),actor->GetCurrentHitPoints(0),s->m_feats.m_count);
    }
    void Refresh(){
        auto* t=SelectedType();const bool chosen=t!=nullptr;
        variantLabel.m_tail.bVisible=appearanceLabel.m_tail.bVisible=statsLabel.m_tail.bVisible=chosen;
        colorLabel.m_tail.bVisible=combatLabel.m_tail.bVisible=chosen && hasSaber;
        for(int i:{ColorLeft,ColorRight,CombatLeft,CombatRight}){buttons[i].m_tail.bVisible=chosen && hasSaber;buttons[i].SetEnabled(chosen && hasSaber && (i==ColorLeft || i==ColorRight || singleSaber));}
        const char* colors[]={"Native","Blue","Red","Green","Yellow","Purple"};const char* combats[]={"Yellow","Red","Blue"};
        colorLabel.m_text.m_params.SetText(CExoString((std::string("Saber colour: ")+colors[saberColor]).c_str()));
        combatLabel.m_text.m_params.SetText(CExoString((std::string("Stance: ")+(staffSaber?"Double-blade":dualSaber?"Dual Sabers":combats[combatStance])).c_str()));
        preview.view.friendly=friendly;
        for(int i:{StanceLeft,StanceRight,AppearanceLeft,AppearanceRight,Team,Spawn}){buttons[i].m_tail.bVisible=chosen;buttons[i].SetEnabled(chosen);}
        if(chosen) {
            const auto* entry=Entry();char value[512];
            const char* custom[]={"Red Lightsaber (Stance: Yellow)","Double Lightsaber (Stance: Double-blade)","Dual Lightsabers (Stance: Dual sabers)","Red Lightsaber (Stance: Red)","Red Lightsaber (Stance: Blue)"};
            std::string indicator=entry?selectedIndicator:custom[variant];
            snprintf(value,sizeof(value),"Variant %d [%d/%zu]",variant+1,variant+1,t->variants.size());variantLabel.m_text.m_params.SetText(CExoString(value));
            const auto count=t->variants[variant].looks.size();
            auto* body=preview.creature?preview.creature->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1):nullptr;
            snprintf(value,sizeof(value),"Appearance: %s [%d/%zu]",body && body->m_pModel?body->m_pModel->m_szName:(entry?entry->model.c_str():"N_DarkJediM"),look+1,count);appearanceLabel.m_text.m_params.SetText(CExoString(value));
            buttons[StanceLeft].SetEnabled(t->variants.size()>1);buttons[StanceRight].SetEnabled(t->variants.size()>1);
            buttons[AppearanceLeft].SetEnabled(count>1);buttons[AppearanceRight].SetEnabled(count>1);
            FitLabel(variantLabel);FitLabel(appearanceLabel);FitLabel(statsLabel);
        }
        buttons[Team].m_text.m_params.SetText(CExoString(friendly?"Alignment: Friendly":"Alignment: Hostile"));
        buttons[Ai].m_text.m_params.SetText(CExoString(sandboxAi?"NPC AI: On":"NPC AI: Off"));
        buttons[TargetPlayer].m_text.m_params.SetText(CExoString(sandboxTargetPlayer?"TARGET PLAYER: Yes":"TARGET PLAYER: No"));
        HighlightSelections();
    }
    void OnSelection(CSWGuiControl* control) {
        // Mouse-down can notify the old selection before mouse-up chooses the
        // new row. Ignore unchanged selections rather than resetting details.
        if(control==&factions && factions.m_nSelectedIndex>=0 && factions.m_nSelectedIndex!=faction)SelectFaction(factions.m_nSelectedIndex);
        if(control==&types && types.m_nSelectedIndex>=0 && types.m_nSelectedIndex!=type)SelectType(types.m_nSelectedIndex);
    }
    void Click(CSWGuiControl* control){
        if(control==&buttons[Close]){SetToBeDeleted(1);return;}
        for(int i=0;i<factions.m_aControls.m_count;++i)if(control==factions.GetControl(i)){SelectFaction(i);return;}
        for(int i=0;i<types.m_aControls.m_count;++i)if(control==types.GetControl(i)){SelectType(i);return;}
        auto* t=SelectedType();
        if(t && (control==&buttons[StanceLeft] || control==&buttons[StanceRight])) {
            int n=int(t->variants.size());variant=(variant+n+(control==&buttons[StanceLeft]?-1:1))%n;look=0;status.m_text.m_params.SetText(CExoString(""));UpdatePreview();
        }
        if(t && (control==&buttons[AppearanceLeft] || control==&buttons[AppearanceRight])) {
            int n=int(t->variants[variant].looks.size());look=(look+n+(control==&buttons[AppearanceLeft]?-1:1))%n;status.m_text.m_params.SetText(CExoString(""));UpdatePreview();
        }
        if(control==&buttons[Team]){friendly=!friendly;UpdatePreview();}
        if(hasSaber && (control==&buttons[ColorLeft] || control==&buttons[ColorRight])){saberColor=(saberColor+6+(control==&buttons[ColorLeft]?-1:1))%6;UpdatePreview();}
        if(singleSaber && (control==&buttons[CombatLeft] || control==&buttons[CombatRight])){combatStance=(combatStance+3+(control==&buttons[CombatLeft]?-1:1))%3;UpdatePreview();}
        if(control==&buttons[Ai])SandboxSetAi(!sandboxAi);
        if(control==&buttons[TargetPlayer])SandboxSetTargetPlayer(!sandboxTargetPlayer);
        if(t && control==&buttons[Spawn]){CExoString message;SandboxSpawn(t->group<0?variant:0,friendly,message,t->group,t->variants[variant].source>=0?t->variants[variant].source:t->variants[variant].looks[look],true,AppearanceEntry(),saberColor,combatStance);status.m_text.m_params.SetText(message);}
        Refresh();
    }
    void HandleInputEvent(int type,int value) override {if((type==INPUTEVENT_GUI_B || type==INPUTEVENT_KEY_ESCAPE || type==INPUTEVENT_GUI) && value){SetToBeDeleted(1);return;}CSWGuiPanel::HandleInputEvent(type,value);}
    void Draw(float delta) override {
        HighlightSelections();
        CSWGuiPanel::Draw(delta);
        if(!std::getenv("K1_ENGAGE_SANDBOX_TEST") || captured)return;captured=true;
        if(auto* text=title.m_text.m_pString)
            fprintf(stderr,"K1_SANDBOX_FONT name=%s pixel_height=%d lines=%d\n",
                text->GetFontTexture(),text->GetFontPixelHeight(),text->GetNumLines());
        if(std::getenv("K1_ENGAGE_FONT_TEST")) {
            static bool checked=false;
            if(!checked) {
                checked=true;bool pass=true;
                const char* fonts[]={"dialogfont10x10","dialogfont10x10a","dialogfont10x10b","dialogfont12x16",
                    "dialogfont16x16","dialogfont16x16a","dialogfont16x16b","dialogfont32x32","fnt_console",
                    "fnt_credits","fnt_creditsa","fnt_creditsb","fnt_d10x10b","fnt_d16x16","fnt_d16x16a",
                    "fnt_d16x16b","fnt_dialog16x16","fnt_galahad14"};
                for(const auto* name:fonts) {
                    CAurFont sample(const_cast<char*>(name));auto* font=sample.m_pFontInfo;auto* texture=sample.m_pTexture.m_pObject;
                    // CAurFont requests the glyph metadata only. Explicitly
                    // exercise the native image readers for this load probe.
                    if(texture)if(auto* image=texture->GetReferencedTexture()->AsCAurTextureBasic()) {
                        image->LoadImageHeader();image->LoadImage();texture->Bind();
                    }
                    const int w=texture?texture->GetWidth():0,h=texture?texture->GetHeight():0;
                    const bool ok=font && font->m_nCharCount>=127 && font->m_nCharCount<=256 && w>=1024 && h>=1024;
                    pass=pass && ok;
                    fprintf(stderr,"K1_FONT_LOAD_%s name=%s atlas=%d,%d chars=%d height=%.9g baseline=%.9g width=%.9g advance63=%.9g advance95=%.9g\n",
                        ok?"PASS":"FAIL",name,w,h,font?font->m_nCharCount:0,font?font->m_fBottom:0,font?font->m_fTop:0,font?font->m_fWidthScale:0,
                        font?(font->m_aMax[63].x-font->m_aMin[63].x)*font->m_fWidthScale*100:0,
                        font?(font->m_aMax[95].x-font->m_aMin[95].x)*font->m_fWidthScale*100:0);
                }
                fprintf(stderr,"K1_FONT_ATLASES_%s count=18\n",pass?"PASS":"FAIL");
            }
        }
        static int epoch=0;int viewport[4];ASLgl::glGetIntegerv(0x0BA2,viewport);
        const int width=viewport[2],height=viewport[3];if(width<=0||height<=0)return;
        std::vector<unsigned char> pixels(size_t(width)*height*4),rgb(size_t(width)*height*3);
        ASLgl::glReadPixels(viewport[0],viewport[1],width,height,0x1908,0x1401,pixels.data());
        for(int y=0;y<height;++y)for(int x=0;x<width;++x)for(int c=0;c<3;++c)rgb[(size_t(y)*width+x)*3+c]=pixels[(size_t(height-1-y)*width+x)*4+c];
        char name[64];snprintf(name,sizeof(name),"sandbox-menu-%d.ppm",epoch++);
        if(FILE* f=fopen(name,"wb")){fprintf(f,"P6\n%d %d\n255\n",width,height);fwrite(rgb.data(),1,rgb.size(),f);fclose(f);fprintf(stderr,"K1_SANDBOX_RENDER_CAPTURE file=%s size=%d,%d\n",name,width,height);}
    }
    bool captured=false;
    void OnPanelRemoved() override;
    ~K1SandboxPanel() override;
};
bool SandboxClickListRow(K1SandboxPanel* panel,CSWGuiListBox& list,int index) {
    if(index<0 || index>=list.m_aControls.m_count)return false;
    // Bring the target into view, then leave it unselected so mouse-up must
    // perform the selection and send the native notification.
    list.SetSelectedControl(index,0);list.SetSelectedControl(-1,0);
    auto* row=list.GetControl(index);auto e=row->m_extent;
    auto* manager=panel->m_pGuiManager;const auto saved=manager->m_currentMousePos;
    const auto flags=manager->m_nMouseStateFlags;
    const int x=panel->m_extent.x+list.m_innerExtent.x+e.x+e.w/2;
    const int y=panel->m_extent.y+list.m_innerExtent.y+e.y+e.h/2;
    auto* hit=panel->HitCheckMouse(x,y);
    const bool valid=hit==&list || hit==row;
    if(valid) {
        manager->m_currentMousePos.x=x;manager->m_currentMousePos.y=y;
        manager->m_nMouseStateFlags&=~1;manager->m_pMouseOverControl=&list;
        manager->HandleMouseMove(x,y);manager->HandleLMouseDown(0);manager->HandleLMouseUp();
    }
    manager->m_currentMousePos=saved;manager->m_nMouseStateFlags=flags;
    manager->m_pMouseOverControl=nullptr;manager->UpdateMouseMove();
    if(!valid)fprintf(stderr,"K1_SANDBOX_MOUSE_LIST_FAIL index=%d reason=hit_test\n",index);
    if(valid && list.m_nSelectedIndex!=index)fprintf(stderr,"K1_SANDBOX_MOUSE_LIST_FAIL index=%d selected=%d top=%d reason=selection_moved\n",index,list.m_nSelectedIndex,list.m_nTopRow);
    return valid && list.m_nSelectedIndex==index;
}
#include "k1sandboxoptionsprobe.h"
// Exercise full manager mouse routing and all shared appearances/variants.
int SandboxBrowserProbe(K1SandboxPanel* panel) {
    static int f=0,t=0,v=0,l=0,cases=0,types=0;static bool started=false;
    if(!started) {
        if(panel->browser.size()!=18 || panel->SelectedType() || panel->preview.view.m_tail.bVisible)return -1;
        int typeCount=0,variantCount=0;
        for(const auto& faction:panel->browser)for(const auto& type:faction.types){++typeCount;variantCount+=int(type.variants.size());}
        if(typeCount!=177 || variantCount!=336){fprintf(stderr,"K1_SANDBOX_BROWSER_FAIL reason=weapon_catalog_counts types=%d variants=%d\n",typeCount,variantCount);return -1;}
        started=true;
        if(std::getenv("K1_ENGAGE_NEW_SPECIES_TEST"))f=16;
        for(int i=0;i<30;++i)panel->factions.HandleInputEvent(SWGUIEVENT_WHEEL_DOWN,1);
        if(panel->factions.m_nTopRow<=0)return -1;
        panel->factions.SetTopVisible(0);panel->factions.OrganizeControls();
    }
    if(f>=int(panel->browser.size())) {
        if(!std::getenv("K1_ENGAGE_NEW_SPECIES_TEST")){const int options=SandboxOptionsProbe(panel);if(options<=0)return options;}
        fprintf(stderr,"K1_SANDBOX_BROWSER_PASS factions=%zu types=%d previews=%d scroll=1 mouse_selection=1 native_stats=1 no_world_spawns=1\n",panel->browser.size(),types,cases);return 1;
    }
    if(panel->faction!=f) {
        if(panel->SelectedType())panel->SetActiveControl(&panel->buttons[K1SandboxPanel::Spawn],0);
        if(!SandboxClickListRow(panel,panel->factions,f) || panel->faction!=f || panel->types.m_aControls.m_count!=int(panel->browser[f].types.size()))return -1;
        if(panel->SelectedType() || panel->preview.view.m_tail.bVisible)return -1;
        if(panel->types.m_aControls.m_count>panel->types.m_nVisibleRows) {
            for(int i=0;i<40;++i)panel->types.HandleInputEvent(SWGUIEVENT_WHEEL_DOWN,1);
            if(panel->types.m_nTopRow<=0)return -1;
            panel->types.SetTopVisible(0);panel->types.OrganizeControls();
        }
    }
    if(t>=int(panel->browser[f].types.size())){++f;t=v=l=0;return 0;}
    if(panel->type!=t){if(!SandboxClickListRow(panel,panel->types,t) || panel->type!=t)return -1;++types;}
    panel->variant=v;panel->look=l;panel->UpdatePreview();panel->Refresh();
    if(panel->variantLabel.m_text.GetIdealHeight()>panel->variantLabel.m_extent.h ||
       panel->appearanceLabel.m_text.GetIdealHeight()>panel->appearanceLabel.m_extent.h ||
       panel->statsLabel.m_text.GetIdealHeight()>panel->statsLabel.m_extent.h) {
        fprintf(stderr,"K1_SANDBOX_BROWSER_FAIL reason=text_overflow faction=%d type=%d variant=%d look=%d heights=%d/%d,%d/%d,%d/%d\n",f,t,v,l,panel->variantLabel.m_text.GetIdealHeight(),panel->variantLabel.m_extent.h,panel->appearanceLabel.m_text.GetIdealHeight(),panel->appearanceLabel.m_extent.h,panel->statsLabel.m_text.GetIdealHeight(),panel->statsLabel.m_extent.h);return -1;
    }
    auto* selected=panel->SelectedType();const auto* source=panel->AppearanceEntry();
    auto fail=[&](const char* reason){fprintf(stderr,"K1_SANDBOX_BROWSER_FAIL faction=%d type=%d variant=%d look=%d reason=%s\n",f,t,v,l,reason);return -1;};
    if(!panel->preview.creature || !panel->preview.view.m_tail.bVisible)return fail("preview_hidden");
    auto* body=panel->preview.creature->GetModel(CSWCAnimBase::ANIM_MODEL_BODY,1);
    if(!body || body->m_pScene!=panel->preview.view.m_scene.m_pScene)return fail("preview_scene");
    float idleTime=0,idleLength=0;
    if(!body->GetAnimation(const_cast<char*>("pause1"),&idleTime,&idleLength) &&
       !body->GetAnimation(const_cast<char*>("cpause1"),&idleTime,&idleLength))return fail("neutral_idle");
    for(const char* pose:{"good","evil","neutral"})if(body->GetAnimation(const_cast<char*>(pose),&idleTime,&idleLength))return fail("alignment_pose_active");
    if(source) {
        std::unique_ptr<CSWSCreature> expected(new CSWSCreature(OBJECT_INVALID,0));
        if(!expected->LoadFromTemplate(CResRef(source->creature.c_str()),0) ||
           panel->preview.creature->m_pCreatureAppearance->m_nAppearanceId!=expected->m_appearance.m_headId)return fail("appearance");
        int head=-1;const bool separateHead=g_pRules->m_p2DArrays->m_pAppearance->GetINTEntry(expected->m_appearance.m_headId,CExoString("normalhead"),&head)!=0;
        if(separateHead && head>=0 && !panel->preview.creature->GetModel(CSWCAnimBase::ANIM_MODEL_HEAD,1))return fail("head");
    }
    // Camera's -Z ray must face the model (Quaternion constructor is w,x,y,z).
    auto* camera=panel->preview.view.m_scene.m_pCamera;
    if(dot(Rotate(camera->GetOrientation(),Vector(0,0,-1)),normalize(Vector(0,0,panel->preview.creature->GetHeadHeightOffset()*.5f)-camera->GetPosition()))<.95f)return fail("camera_direction");
    if(!sandboxActors.empty())return fail("world_spawn");
    ++cases;
    if(++l>=int(selected->variants[v].looks.size())){l=0;if(++v>=int(selected->variants.size())){v=0;++t;}}
    return 0;
}
K1SandboxPanel* sandboxPanel=nullptr;
bool sandboxResume=false;
int sandboxRestorePause=-1;
void K1SandboxPanel::OnPanelRemoved(){
    auto& memory=sandboxMenuMemory;memory.valid=true;memory.faction=faction;memory.type=type;
    memory.variant=variant;memory.look=look;memory.color=saberColor;memory.combat=combatStance;memory.friendly=friendly;
    memory.factionTop=factions.m_nTopRow;memory.typeTop=types.m_nTopRow;memory.status=status.m_text.m_params.m_sText;
    memory.focus=GetActiveControl()==&types?1:0;
    for(int i=0;i<Count;++i)if(GetActiveControl()==&buttons[i])memory.focus=i+2;
    CSWGuiPanel::OnPanelRemoved();sandboxMenuOpen=false;
    if(sandboxReady && sandboxArea==areaId && K1EngageArenaLoadoutActive()){sandboxResume=wasActive;sandboxRestorePause=wasPaused?1:0;}
}
K1SandboxPanel::~K1SandboxPanel(){if(sandboxPanel==this)sandboxPanel=nullptr;}
