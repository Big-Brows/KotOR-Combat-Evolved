/*
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
Copyright (C) 2013 - 2015, OpenJK contributors

This file contains code adapted from OpenJK, free software under the GNU
General Public License version 2. It is distributed WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License with this
program; if not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include "k1sabercontact.h"
#include "k1groundmove.h"
#include <vector>

// OpenJK 260c59c2907187af555a676fe0cc798893bf7757:
// code/cgame/cg_players.cpp CG_CreateSaberMarks;
// code/rd-vanilla/tr_marks.cpp R_ChopPolyBehindPlane/R_AddMarkFragments;
// code/cgame/cg_marks.cpp CG_AddMarks.
// The host supplies K12 surface triangles instead of a Quake BSP. Keep the
// original projection, UVs, bounds and fade rules in JKA units.
namespace K1JkaSaberMarks {
constexpr float scale=K1GroundMove::UnitScale;
struct Vertex { Vector point;float s=0,t=0; };
using Polygon=std::vector<Vertex>;
inline long long SoundInterval(int lengthMs) {return std::max(100000LL,static_cast<long long>(lengthMs)*1000);}
// Host continuity boundary; do not join a miss, a different receiver/weapon,
// or a teleport into one surface scar.
inline bool Connect(bool previous,bool sameReceiver,bool sameWeapon,Vector a,Vector b) {
    const float distance=magnitude(b-a);
    return previous && sameReceiver && sameWeapon && distance>.0005f && distance<1.f;
}
// K12's precise PartTriMesh hit reports the part/point but clears its normal.
// Recover only from a face containing that actual point; never invent a normal
// from a prop's box or the attack direction.
inline bool HitNormal(Vector point,const std::vector<K1SaberContact::Triangle>& triangles,Vector& normal) {
    float best=.001f;bool found=false;
    for(const auto& triangle:triangles) {
        Vector n=cross(triangle.b-triangle.a,triangle.c-triangle.a);
        if(magnitude(n)<1e-8f) continue;n=normalize(n);
        const float distance=std::fabs(dot(point-triangle.a,n));
        if(distance>=best || !K1SaberContact::SegmentTriangle(point-n*.002f,point+n*.002f,triangle)) continue;
        best=distance;normal=n;found=true;
    }
    return found;
}
inline std::vector<Vector> Chop(const std::vector<Vector>& in,Vector normal,float dist) {
    std::vector<Vector> out;
    if(in.size()>=62) return out;
    int front=0,back=0;float d[68]={};int side[68]={};
    for(size_t i=0;i<in.size();++i) {
        d[i]=dot(in[i],normal)-dist;
        side[i]=d[i]>.5f*scale?0:(d[i]<-.5f*scale?1:2);
        front+=side[i]==0;back+=side[i]==1;
    }
    if(!front) return out;
    if(!back) return in;
    side[in.size()]=side[0];d[in.size()]=d[0];
    for(size_t i=0;i<in.size();++i) {
        if(side[i]==2) {out.push_back(in[i]);continue;}
        if(side[i]==0) out.push_back(in[i]);
        if(side[i+1]==2 || side[i+1]==side[i]) continue;
        float delta=d[i]-d[i+1],f=delta==0?0:d[i]/delta;
        out.push_back(in[i]+(in[(i+1)%in.size()]-in[i])*f);
    }
    return out;
}
template<class Random> std::vector<Polygon> Create(Vector start,Vector end,Vector normal,
        const std::vector<K1SaberContact::Triangle>& surfaces,Random random) {
    std::vector<Polygon> result;
    if(!K1SaberContact::Finite(start) || !K1SaberContact::Finite(end) ||
        !K1SaberContact::Finite(normal) || magnitude(end-start)<1e-7f || magnitude(normal)<.5f) return result;
    Vector axis1=normalize(end-start),axis0=normalize(normal),axis2=cross(axis1,axis0);
    const float radius=.65f*scale;
    Vector points[]={start-axis1*radius-axis2*radius,end+axis1*radius-axis2*radius,
        end+axis1*radius+axis2*radius,start-axis1*radius+axis2*radius};
    Vector projection=axis0*-1.f,planes[6];float distances[6];
    for(int i=0;i<4;++i) {
        planes[i]=normalize(cross(points[(i+1)%4]-points[i],projection*-1.f));
        distances[i]=dot(planes[i],points[i]);
    }
    planes[4]=projection;distances[4]=dot(projection,points[0])-32.f*scale;
    planes[5]=projection*-1.f;distances[5]=dot(planes[5],points[0])-20.f*scale;
    int count=0;
    for(const auto& triangle:surfaces) {
        Vector face=cross(triangle.b-triangle.a,triangle.c-triangle.a);
        if(magnitude(face)<1e-10f || dot(normalize(face),projection)>-.5f) continue;
        std::vector<Vector> polygon={triangle.a,triangle.b,triangle.c};
        for(int i=0;i<6 && !polygon.empty();++i) polygon=Chop(polygon,planes[i],distances[i]);
        if(polygon.size()<3 || count+int(polygon.size())>384) continue;
        Polygon mark;const Vector mid=(end+start)*.5f;
        for(Vector p:polygon) {
            const Vector delta=(p-mid)*(1.f/scale);
            mark.push_back({p,.5f+dot(delta,axis1)*(.05f+random()*.03f),
                .5f+dot(delta,axis2)*(.15f+random()*.05f)});
        }
        count+=int(mark.size());result.push_back(mark);
        if(result.size()==128) break;
    }
    return result;
}
inline float Glow(long long ageUs) {
    // CG_CreateSaberMarks backdates the glow by8500ms; total lifetime10000ms.
    if(ageUs<0 || ageUs>1500000) return 0;
    return ageUs<500000?1.f:float(1500000-ageUs)/1000000.f;
}
inline bool Probe() {
    const float u=scale;
    std::vector<K1SaberContact::Triangle> floor={
        {Vector(-10*u,-10*u,0),Vector(10*u,-10*u,0),Vector(10*u,10*u,0)},
        {Vector(-10*u,-10*u,0),Vector(10*u,10*u,0),Vector(-10*u,10*u,0)}};
    const auto marks=Create(Vector(-2*u,0,0),Vector(2*u,0,0),Vector(0,0,1),floor,[]{return .5f;});
    bool ok=!marks.empty();
    ok &= SoundInterval(0)==100000 && SoundInterval(50)==100000 && SoundInterval(100)==100000 && SoundInterval(361)==361000;
    Vector recovered;
    ok &= HitNormal(Vector(),floor,recovered) && recovered.z>.999f &&
        !HitNormal(Vector(0,0,.01f),floor,recovered) && !HitNormal(Vector(1,1,0),floor,recovered);
    for(const auto& polygon:marks) for(const auto& v:polygon)
        ok &= K1SaberContact::Finite(v.point) && std::isfinite(v.s) && std::isfinite(v.t) && std::fabs(v.point.z)<1e-6f;
    ok &= Create(Vector(),Vector(),Vector(0,0,1),floor,[]{return .5f;}).empty();
    ok &= Create(Vector(-2*u,0,5*u),Vector(2*u,0,5*u),Vector(0,0,-1),floor,[]{return .5f;}).empty();
    ok &= Glow(0)==1 && Glow(500000)==1 && std::fabs(Glow(1000000)-.5f)<1e-6f && Glow(1500000)==0;
    ok &= Connect(true,true,true,Vector(),Vector(.02f,0,0)) &&
        !Connect(false,true,true,Vector(),Vector(.02f,0,0)) &&
        !Connect(true,false,true,Vector(),Vector(.02f,0,0)) &&
        !Connect(true,true,false,Vector(),Vector(.02f,0,0)) &&
        !Connect(true,true,true,Vector(),Vector(2,0,0)) &&
        !Connect(true,true,true,Vector(),Vector());
    return ok;
}
}
