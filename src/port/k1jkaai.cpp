/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/
// Ported from OpenJK 260c59c code/game/AI_Jedi.cpp:
// Jedi_Aggression (ordinary enemy profile), Jedi_Strafe,
// Jedi_CombatTimersUpdate (roam/noStrafe portion), Jedi_CombatDistance
// (ordinary grounded saber profile with no usable special attacks/Force).
// Engine globals become explicit state/input/bindings. This is NOT the complete
// AI_Jedi translation unit: boss, Force, kick, saber-lock/event, taunt, airborne,
// attack selection and search branches still require their own engine bindings.
#include "k1jkaai.h"
#if KOTOR1 && defined(K2_UI_PC)
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace K1JkaAI {
namespace {
void Jedi_Aggression(JediState& self,int change) {
    self.aggression+=change;
    // TEAM_ENEMY, ordinary CLASS_REBORN: original thresholds 3..10.
    if(self.aggression>10) self.aggression=10;
    else if(self.aggression<3) self.aggression=3;
}
bool Jedi_Strafe(JediState& npc,const JediInput& input,Bindings& engine,
    int strafeTimeMin,int strafeTimeMax,int nextStrafeTimeMin,int nextStrafeTimeMax,bool walking) {
    // Cultist-destroyer and lock-won early-outs cannot apply to this profile.
    if(npc.strafeLeft<=input.now && npc.strafeRight<=input.now) {
        bool strafed=false;
        const int strafeTime=engine.random(strafeTimeMin,strafeTimeMax);
        if(engine.random(0,1)) {
            if(engine.moveDirClear(0,-127)) {npc.strafeLeft=input.now+strafeTime;strafed=true;}
            else if(engine.moveDirClear(0,127)) {npc.strafeRight=input.now+strafeTime;strafed=true;}
        } else {
            if(engine.moveDirClear(0,127)) {npc.strafeRight=input.now+strafeTime;strafed=true;}
            else if(engine.moveDirClear(0,-127)) {npc.strafeLeft=input.now+strafeTime;strafed=true;}
        }
        if(strafed) {
            npc.noStrafe=input.now+strafeTime+engine.random(nextStrafeTimeMin,nextStrafeTimeMax);
            if(walking) npc.walking=input.now+strafeTime;
            return true;
        }
    }
    return false;
}
void Jedi_CombatTimersUpdate(JediState& npc,const JediInput& input,Bindings& engine) {
    if(npc.roamTime<=input.now) {
        npc.roamTime=input.now+engine.random(2000,5000);
        if(input.enemySaber) Jedi_Aggression(npc,input.enemySaberActive?1:2);
        else {
            if(!input.enemyShooting) Jedi_Aggression(npc,1);
            if(input.enemyDistance<256) Jedi_Aggression(npc,1);
        }
    }
    if(npc.noStrafe<=input.now && npc.strafeLeft<=input.now && npc.strafeRight<=input.now) {
        if(!engine.random(0,4)) Jedi_Strafe(npc,input,engine,1000,3000,0,4000,true);
        else npc.noStrafe=input.now+engine.random(1000,3000);
    }
}
int Jedi_CombatDistance(const JediState& npc,const JediInput& input) {
    const int enemy_dist=input.enemyDistance;
    // Source branch order for the supported profile. Unbound special moves
    // are explicitly excluded above, rather than simulated with KotOR AI.
    if(enemy_dist<=-16) return -127; // Jedi_Retreat
    else if(enemy_dist<=0) {
        if(npc.aggression<4) return -127;
    } else if(enemy_dist>256) {
        if(npc.aggression>0) return 127; // no usable tactical Force power
    } else if(enemy_dist>50) {
        if(npc.aggression>5 && !input.parrying) return 127;
    } else {
        if(npc.aggression<4) return -127;
        else if(npc.aggression>5 && !input.parrying) return 127;
    }
    return 0;
}
}
Commands mediumJedi(JediState& npc,const JediInput& input,Bindings& engine) {
    Jedi_CombatTimersUpdate(npc,input,engine);
    Commands command;
    command.forward=Jedi_CombatDistance(npc,input);
    // JKA's timer application overlays lateral commands on forward movement.
    if(npc.strafeLeft>input.now) command.right=-127;
    else if(npc.strafeRight>input.now) command.right=127;
    command.walking=npc.walking>input.now;
    return command;
}
bool backpedal(bool previous,const Commands& command) {
    // bg_pmove.cpp PmoveSingle: PMF_BACKWARDS_RUN command update. Pure lateral
    // input explicitly clears it; zero input preserves it during deceleration.
    if(command.forward<0) return true;
    else if(command.forward>0 || (command.forward==0 && command.right)) return false;
    return previous;
}
float updateYaw(float currentDegrees,float targetDegrees,float yawSpeed) {
    // NPC_utils.cpp NPC_UpdateAngles ordinary grounded NPC, doYaw branch.
    // The host accepts float angles in place of JKA's final usercmd packing.
    float error=std::remainder(currentDegrees-targetDegrees,360.f);
    if(std::fabs(error)>.01f) {
        float decay=60.f+yawSpeed*3.f;
        decay*=50.f/1000.f;
        if(error<0) {error+=decay;if(error>0) error=0;}
        else {error-=decay;if(error<0) error=0;}
    }
    return targetDegrees+error;
}
bool probe() {
    struct TestBindings:Bindings {
        bool left=true,right=true;int draws=0;
        int random(int low,int)override {++draws;return low;}
        bool moveDirClear(int,int side)override {return side<0?left:right;}
    } engine;
    bool ok=true;JediState npc;JediInput input;
    // Source boundary table, including original integer-distance truncation at
    // the engine API, defensive/middle/offensive aggression and parry gating.
    for(int aggression:{3,4,5,6,10}) for(int distance:{-17,-16,-1,0,1,50,51,256,257}) {
        npc.aggression=aggression;input.enemyDistance=distance;
        const int expected=distance<=-16?-127:distance<=0?(aggression<4?-127:0):
            distance>256?127:distance>50?(aggression>5?127:0):aggression<4?-127:aggression>5?127:0;
        ok=ok && Jedi_CombatDistance(npc,input)==expected;
        input.parrying=true;
        if(distance>0 && distance<=256 && aggression>5) ok=ok && Jedi_CombatDistance(npc,input)==0;
        input.parrying=false;
    }
    npc=JediState();input=JediInput();
    auto command=mediumJedi(npc,input,engine);
    ok=ok && npc.aggression==4 && npc.roamTime==2000 && command.right==127 && command.walking;
    input.now=999;const int draws=engine.draws;command=mediumJedi(npc,input,engine);
    ok=ok && command.right==127 && engine.draws==draws;
    input.now=1000;engine.right=false;command=mediumJedi(npc,input,engine);
    ok=ok && command.right==-127 && command.walking; // original opposite-side fallback
    npc=JediState();engine.left=engine.right=false;input.now=0;
    command=mediumJedi(npc,input,engine);
    ok=ok && command.right==0 && !command.walking;
    npc.aggression=10;Jedi_Aggression(npc,2);ok=ok && npc.aggression==10;
    Jedi_Aggression(npc,-20);ok=ok && npc.aggression==3;
    ok=ok && backpedal(false,Commands{-127,0,false}) &&
        !backpedal(true,Commands{127,0,false}) && !backpedal(true,Commands{0,127,false}) &&
        backpedal(true,Commands{}) && !backpedal(false,Commands{});
    ok=ok && std::fabs(updateYaw(0,90,60)-12)<.0001f &&
        std::fabs(updateYaw(0,-90,60)+12)<.0001f &&
        std::fabs(updateYaw(179,-179,60)+179)<.0001f &&
        std::fabs(updateYaw(0,5,70)-5)<.0001f &&
        std::fabs(updateYaw(0,90,70)-13.5f)<.0001f;
    fprintf(stderr,"K1_JKA_JEDI_SOURCE_%s boundaries=45 timers=1 side_fallback=1 blocked=1 backpedal=5 yaw=5\n",ok?"PASS":"FAIL");
    return ok;
}
}
#endif
