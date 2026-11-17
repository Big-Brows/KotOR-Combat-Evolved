// Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
// Native modal controls for the isolated K1 sandbox. Included by k1engage.cpp.
class K1SandboxPanel final : public CSWGuiPanel {
public:
    enum Action {NpcLeft,NpcRight,StanceLeft,StanceRight,Team,Ai,Spawn,Close,Count};
    CSWGuiButton buttons[Count];
    CSWGuiLabel npcLabel,stanceLabel,title,status;
    int stance=0;bool friendly=false,wasActive=false,wasPaused=false;
    unsigned long long areaId=OBJECT_INVALID;
    explicit K1SandboxPanel(CSWGuiManager* manager):CSWGuiPanel(manager) {
        wasActive=K1EngageEngaged();wasPaused=g_pAppManager->m_pServerExoApp->GetPauseState(2)!=0;
        if(ServerActor() && ServerActor()->GetArea())areaId=static_cast<CSWSArea*>(ServerActor()->GetArea())->m_objectId;
        int width=0,height=0;AurGUIGetScreenSize(width,height);
        const float scale=std::max(.65f,height/720.f);const int w=int(480*scale),h=int(400*scale);
        m_stateFlags|=1; // Coordinates already use the native physical screen, not a640x480 layout.
        SetExtent(CSWGuiExtent((width-w)/2,(height-h)/2,w,h));
        const CSWGuiBorderParams normal(CResRef("boxline1"),CResRef("boxline2"),CResRef(),2,6,1.f,Vector(.934f,1,.934f),9);
        const CSWGuiBorderParams selected(CResRef("boxline3"),CResRef("boxline4"),CResRef(),2,6,1.f,Vector(.871f,1,.871f),9);
        const CSWGuiBorderParams frame(CResRef("boxline1"),CResRef("boxline2"),CResRef("blackfill"),2,6,1.f,Vector(.934f,1,.934f),9);
        m_pBackground->Initialize(CSWGuiExtent(0,0,w,h),frame);
        auto text=g_pAppManager->m_pClientExoApp->GetInGameGui()->m_pMainInterface->m_tbPause.m_text.m_params;
        text.SetAlignment(SWGUIALIGN_CENTER|SWGUIALIGN_MIDDLE);
        auto rect=[&](int x,int y,int rw,int rh){return CSWGuiExtent(int(x*scale),int(y*scale),int(rw*scale),int(rh*scale));};
        const CSWGuiBorderParams none(CResRef(),CResRef(),CResRef(),0,0,0,Vector(1,1,1),0);
        auto label=[&](CSWGuiLabel& control,const char* name,int y){text.SetText(CExoString(name));control.Initialize(rect(75,y,330,38),text,none,1.f);control.m_tail.bIgnoreMouse=true;AddControl(&control);};
        label(title,"Spawn Menu",14);label(npcLabel,"NPC: Dark Jedi",74);label(stanceLabel,"",125);label(status,"",337);
        const char* names[]={"<",">","<",">","","","Spawn","Close"};
        const int xs[]={26,412,26,412,75,75,75,250};const int ys[]={74,74,125,125,184,235,290,290};
        for(int i=0;i<Count;++i){text.SetText(CExoString(names[i]));buttons[i].Initialize(rect(xs[i],ys[i],i<4?42:(i==Team||i==Ai?330:155),38),text,normal,selected);buttons[i].m_tail.bFocusOnHover=false;buttons[i].AddEvent(INPUTEVENT_GUI_A,this,static_cast<void(CSWGuiObject::*)(CSWGuiControl*)>(&K1SandboxPanel::Click));AddControl(&buttons[i]);}
        // One base NPC currently; stance selection is independent and expandable.
        buttons[NpcLeft].SetEnabled(0);buttons[NpcRight].SetEnabled(0);Refresh();SetActiveControl(&buttons[Spawn],0);
    }
    void Refresh(){stanceLabel.m_text.m_params.SetText(CExoString(stance?"Stance: Double-blade":"Stance: Yellow"));buttons[Team].m_text.m_params.SetText(CExoString(friendly?"Alignment: Friendly":"Alignment: Hostile"));buttons[Ai].m_text.m_params.SetText(CExoString(sandboxAi?"NPC AI: On":"NPC AI: Off"));}
    void Click(CSWGuiControl* control){
        if(control==&buttons[Close]){SetToBeDeleted(1);return;}
        if(control==&buttons[StanceLeft]||control==&buttons[StanceRight])stance=1-stance;
        if(control==&buttons[Team])friendly=!friendly;
        if(control==&buttons[Ai])SandboxSetAi(!sandboxAi);
        if(control==&buttons[Spawn]){CExoString message;SandboxSpawn(stance,friendly,message);status.m_text.m_params.SetText(message);}
        Refresh();
    }
    void HandleInputEvent(int type,int value) override {if(type==INPUTEVENT_GUI_B && value){SetToBeDeleted(1);return;}CSWGuiPanel::HandleInputEvent(type,value);}
    void Draw(float delta) override {
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
K1SandboxPanel* sandboxPanel=nullptr;
bool sandboxResume=false;
int sandboxRestorePause=-1;
void K1SandboxPanel::OnPanelRemoved(){CSWGuiPanel::OnPanelRemoved();sandboxMenuOpen=false;if(sandboxReady && sandboxArea==areaId && K1EngageArenaLoadoutActive()){sandboxResume=wasActive;sandboxRestorePause=wasPaused?1:0;}}
K1SandboxPanel::~K1SandboxPanel(){if(sandboxPanel==this)sandboxPanel=nullptr;}
