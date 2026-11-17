/*
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

Rules adapted from OpenJK under the GNU General Public License version 2.
Distributed WITHOUT ANY WARRANTY; see the GNU General Public License.
*/
#pragma once
#include <cmath>

// OpenJK 260c59c2907187af555a676fe0cc798893bf7757, code/game/g_combat.cpp:
// G_GetHitLocFromSurfName, G_Dismemberable, G_DoDismemberment.
// This component covers ordinary probability-enabled humanoid saber cuts.
// Host supplies real hit/cap data in JKA units and a uniform [1,100] roll.
// Debug/realistic overrides and accumulated-location-damage mode are excluded.
// Native damage/death, mesh ownership and attachments belong to the host.
namespace K1JkaDismemberment {
enum class Part { None, RightHand, LeftHand, RightArm, LeftArm, RightLeg, LeftLeg, Waist, Head };
struct Point {float x=0,y=0,z=0;};
struct Probabilities {
    float head=0,arms=0,hands=0,waist=0,legs=0;
    float For(Part part) const {
        switch(part) {
        case Part::RightHand:case Part::LeftHand:return hands;
        case Part::RightArm:case Part::LeftArm:return arms;
        case Part::RightLeg:case Part::LeftLeg:return legs;
        case Part::Waist:return waist;
        case Part::Head:return head;
        default:return 0;
        }
    }
};
struct Sample {
    Part part=Part::None;
    Point hit,cap,capAxis,motion,blade;
    float roll=100,probabilityScale=1;
    int level=3;
    bool saber=false,humanoid=false,alreadyDismembered=false;
};
inline float Dot(Point a,Point b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline bool Finite(Point p) {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool Unit(Point p) {return Finite(p)&&std::fabs(Dot(p,p)-1)<.0001f;}
inline bool Eligible(const Sample& s,const Probabilities& p) {
    if(!s.saber || !s.humanoid || s.alreadyDismembered || s.level<1 ||
        !std::isfinite(s.roll) || s.roll<1 || s.roll>100 ||
        !std::isfinite(s.probabilityScale) || s.probabilityScale<=0) return false;
    switch(s.part) {
    case Part::RightHand:case Part::LeftHand:case Part::RightArm:case Part::LeftArm:break;
    case Part::RightLeg:case Part::LeftLeg:if(s.level<2)return false;break;
    case Part::Waist:case Part::Head:if(s.level<3)return false;break;
    default:return false;
    }
    const float probability=p.For(s.part);
    if(!std::isfinite(probability) || probability<0 ||
        std::floor(s.roll*s.probabilityScale)>probability*2.f) return false;
    if(!Finite(s.hit) || !Finite(s.cap) || !Unit(s.capAxis) || !Unit(s.motion) || !Unit(s.blade)) return false;
    const Point distance={s.hit.x-s.cap.x,s.hit.y-s.cap.y,s.hit.z-s.cap.z};
    if(Dot(distance,distance)>=256.f) return false;
    const float angle=(s.part==Part::Waist || s.part==Part::Head)?.25f:.5f;
    const float motion=Dot(s.motion,s.capAxis),blade=Dot(s.blade,s.capAxis);
    return motion>-angle && motion<angle && blade>-angle && blade<angle;
}
} // namespace K1JkaDismemberment
