#pragma once
#include "swguicontrol.h"

// Local PC HUD extension; owns only an independent preview, never the live weapon.
class K1PoiseWeaponView : public CSWGui3DSceneView {
public:
    int percent=100;
    int layoutWidth=0,layoutHeight=0;
    float modelLength=1.f;
    float sourceModelLength=1.f;
    float bladeLength=0.f;
    float singleModelLength=0.f,singleBladeLength=0.f;
    float bladeDepth=0.f,singleBladeDepth=0.f;
    unsigned int draws=0;
    int lastDrawnPercent=-1;
    CExoString resource;
    virtual void Draw(float delta);
};

// Native HUD control; draws a small untextured reticle without owning game assets.
class K1ParryReticle : public CSWGuiControl {
public:
    int sector=-1;
    unsigned long long target=~0ULL;
    bool guarding=false;
    virtual void Draw(float delta);
};
