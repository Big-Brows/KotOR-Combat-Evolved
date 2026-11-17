#pragma once
#include <algorithm>
#include <cmath>

// Ground-only JKA movement expressed in K12 units. Reference behavior: PM_CmdScale,
// PM_Friction, PM_Accelerate in pinned OpenJK 260c59c code/game/bg_pmove.cpp;
// Host-supplied KotOR movement rates replace JKA speed/command/saber modifiers.
struct K1GroundMove {
    static constexpr float UnitScale = .0298100045596346f; // existing humanoid size calibration
    float x=0,y=0;
    void Clear() { x=0;y=0; }
    float Speed() const { return std::sqrt(x*x+y*y); }
    void Step(float forward,float right,float yaw,float cap,float dt,float& moveX,float& moveY) {
        moveX=moveY=0;
        if(!std::isfinite(dt)||dt<=0) return;
        if(!std::isfinite(cap)||cap<=0) {Clear();return;}
        dt=std::min(dt,.1f);
        int count=std::max(1,int(std::ceil(dt/.008f)));float h=dt/count;
        float length=std::sqrt(forward*forward+right*right);
        float maximum=std::max(std::fabs(forward),std::fabs(right));
        float wishSpeed=cap*maximum,wx=0,wy=0;
        if(length>0) {wx=(std::cos(yaw)*forward+std::sin(yaw)*right)/length;wy=(std::sin(yaw)*forward-std::cos(yaw)*right)/length;}
        for(int i=0;i<count;++i) {
            float speed=Speed();
            if(speed<UnitScale) Clear();
            else {float next=std::max(0.f,speed-std::max(speed,100.f*UnitScale)*6.f*h);x*=next/speed;y*=next/speed;}
            float add=wishSpeed-(x*wx+y*wy);
            if(add>0) {float amount=std::min(add,12.f*h*wishSpeed);x+=amount*wx;y+=amount*wy;}
            const float actual=Speed();
            if(actual>cap) {x*=cap/actual;y*=cap/actual;}
            moveX+=x*h;moveY+=y*h;
        }
    }
};
