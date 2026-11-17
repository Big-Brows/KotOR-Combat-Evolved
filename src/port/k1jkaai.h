#pragma once
// SPDX-License-Identifier: GPL-2.0-only

// OpenJK grounded medium-cultist decision boundary. See k1jkaai.cpp for GPL
// notice, source locations, supported profile and omitted capabilities.
namespace K1JkaAI {
struct JediState {
    int aggression=3;
    long long roamTime=0,noStrafe=0,strafeLeft=0,strafeRight=0,walking=0;
};
struct JediInput {
    long long now=0; // milliseconds, as in JKA's level.time
    int enemyDistance=0; // predicted distance from saber tip, in JKA units
    bool enemySaber=true,enemySaberActive=true,enemyShooting=false;
    bool parrying=false;
};
struct Commands { int forward=0,right=0;bool walking=false; };
struct Bindings {
    virtual ~Bindings()=default;
    virtual int random(int low,int high)=0;
    virtual bool moveDirClear(int forward,int right)=0;
};
Commands mediumJedi(JediState& state,const JediInput& input,Bindings& engine);
bool backpedal(bool previous,const Commands& command);
float updateYaw(float currentDegrees,float targetDegrees,float profileYawSpeed);
bool probe();
}
