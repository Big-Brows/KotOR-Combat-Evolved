#pragma once
#include "math3d.h"
#include <algorithm>
#include <cmath>

// Narrow phase for a thin blade's swept surface and native posed triangles.
// The luminous trail is not a damage volume; no enlarged character boxes.
namespace K1SaberContact {
// Clip a rendered pose interval to strike + follow-through. This keeps a slow
// frame from sweeping through the excluded windup/return-to-idle motion.
inline bool ClipTime(float from,float to,float start,float end,float& lo,float& hi) {
    lo=0;hi=1;
    if(!std::isfinite(from) || !std::isfinite(to) || to<from || to<start || from>=end) return false;
    if(to>from) {
        lo=std::max(0.f,std::min(1.f,(start-from)/(to-from)));
        hi=std::max(0.f,std::min(1.f,(end-from)/(to-from)));
    }
    return lo<=hi;
}
struct Triangle {Vector a,b,c;};
inline bool Finite(Vector p) {return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);}
inline bool Separated(Vector axis,const Vector* a,int na,const Vector* b,int nb) {
    float length=dot(axis,axis);if(length<1e-20f) return false;
    Vector origin=b[0];float amin=dot(axis,a[0]-origin),amax=amin,bmin=0,bmax=0;
    for(int i=1;i<na;++i) {float x=dot(axis,a[i]-origin);amin=std::min(amin,x);amax=std::max(amax,x);}
    for(int i=1;i<nb;++i) {float x=dot(axis,b[i]-origin);bmin=std::min(bmin,x);bmax=std::max(bmax,x);}
    float epsilon=.00002f*std::sqrt(length);
    return amax<bmin-epsilon || bmax<amin-epsilon;
}
inline bool SegmentTriangle(Vector a,Vector b,const Triangle& t) {
    Vector segment[]={a,b},triangle[]={t.a,t.b,t.c};
    Vector edges[]={t.b-t.a,t.c-t.b,t.a-t.c},normal=cross(edges[0],edges[1]);
    if(dot(normal,normal)<1e-18f || Separated(normal,segment,2,triangle,3)) return false;
    for(auto edge:edges) if(Separated(cross(b-a,edge),segment,2,triangle,3) ||
        Separated(cross(normal,edge),segment,2,triangle,3)) return false;
    return true;
}
inline bool Overlap(const Triangle& x,const Triangle& y) {
    Vector a[]={x.a,x.b,x.c},b[]={y.a,y.b,y.c};
    Vector ea[]={x.b-x.a,x.c-x.b,x.a-x.c},eb[]={y.b-y.a,y.c-y.b,y.a-y.c};
    Vector na=cross(ea[0],ea[1]),nb=cross(eb[0],eb[1]);
    if(dot(na,na)<1e-18f) return SegmentTriangle(x.a,x.b,y) || SegmentTriangle(x.b,x.c,y);
    if(dot(nb,nb)<1e-18f || Separated(na,a,3,b,3) || Separated(nb,a,3,b,3)) return false;
    for(auto edge:ea) {
        if(Separated(cross(na,edge),a,3,b,3)) return false;
        for(auto other:eb) if(Separated(cross(edge,other),a,3,b,3)) return false;
    }
    for(auto edge:eb) if(Separated(cross(nb,edge),a,3,b,3)) return false;
    return true;
}
struct Blade {Vector base,tip;};
inline Blade Between(const Blade& a,const Blade& b,float t) {
    Vector da=a.tip-a.base,db=b.tip-b.base;float la=magnitude(da),lb=magnitude(db);
    Vector base=a.base*(1-t)+b.base*t;
    if(la<.0001f || lb<.0001f) return {base,a.tip*(1-t)+b.tip*t};
    Vector x=da/la,y=db/lb;Quaternion arc;
    if(dot(x,y)<-.99999f) {
        Vector axis=cross(x,Vector(0,0,1));
        if(magnitude(axis)<.00001f) axis=cross(x,Vector(0,1,0));
        arc=Quaternion(normalize(axis),3.1415926536f);
    } else arc=normalize(RotationArc(x,y));
    Quaternion turn=Interpolate(Quaternion(),arc,t);
    Vector direction=turn.xdir()*(da.x/la)+turn.ydir()*(da.y/la)+turn.zdir()*(da.z/la);
    return {base,base+direction*(la*(1-t)+lb*t)};
}
inline int Steps(const Blade& a,const Blade& b) {
    Vector x=a.tip-a.base,y=b.tip-b.base;float length=magnitude(x)*magnitude(y);
    float angle=length>.000001f?std::acos(std::max(-1.f,std::min(1.f,dot(x,y)/length))):0;
    return std::max(1,int(std::ceil(angle/.0872664626f))); // <=5deg, <1mm chord error at1m
}
inline bool SweepCell(const Blade& a,const Blade& b,const Triangle& t) {
    return SegmentTriangle(a.base,a.tip,t) || SegmentTriangle(b.base,b.tip,t) ||
        Overlap({a.base,a.tip,b.tip},t) || Overlap({a.base,b.tip,b.base},t);
}
// A real point on the contacted surface, separate from the conservative SAT
// overlap predicate. Coplanar/degenerate contacts may deal damage but must not
// invent a dismemberment location.
inline bool IntersectionPoint(Vector a,Vector b,const Triangle& t,Vector& point) {
    const Vector e=t.b-t.a,f=t.c-t.a,d=b-a,h=cross(d,f);
    const float det=dot(e,h);if(std::fabs(det)<1e-10f)return false;
    const Vector s=a-t.a;const float u=dot(s,h)/det;
    const Vector q=cross(s,e);const float v=dot(d,q)/det,w=dot(f,q)/det;
    if(u<0 || v<0 || u+v>1 || w<0 || w>1)return false;
    point=a+d*w;return Finite(point);
}
inline bool SweepPoint(const Blade& a,const Blade& b,const Triangle& t,Vector& point) {
    if(IntersectionPoint(a.base,a.tip,t,point)||IntersectionPoint(b.base,b.tip,t,point))return true;
    const Triangle swept[]={{a.base,a.tip,b.tip},{a.base,b.tip,b.base}};
    for(const auto& s:swept) {
        const Vector x[]={s.a,s.b,s.c},y[]={t.a,t.b,t.c};
        for(int i=0;i<3;++i) if(IntersectionPoint(x[i],x[(i+1)%3],t,point)||
            IntersectionPoint(y[i],y[(i+1)%3],s,point))return true;
    }
    return false;
}
inline bool Probe() {
    Triangle target={{0,-.1f,.2f},{0,.1f,.2f},{0,0,.3f}};
    Blade a={{-.5f,0,0},{-.5f,0,1}},b={{.5f,0,0},{.5f,0,1}};
    if(!SweepCell(a,b,target) || SegmentTriangle(a.base,b.tip,target)) return false;
    if(SweepCell(a,b,{{0,-.1f,1.1f},{0,.1f,1.1f},{0,0,1.2f}})) return false;
    if(!SegmentTriangle({-1,0,.25f},{1,0,.25f},target)) return false;
    if(SegmentTriangle({-1,0,.5f},{1,0,.5f},target)) return false;
    if(!Overlap({{0,0,0},{1,0,0},{0,1,0}},{{.2f,.2f,0},{.3f,.2f,0},{.2f,.3f,0}})) return false;
    if(Overlap({{0,0,0},{1,0,0},{0,1,0}},{{.8f,.8f,0},{1,.8f,0},{.8f,1,0}})) return false;
    Blade start={{0,0,0},{1,0,0}},finish={{0,0,0},{0,1,0}};
    auto arcHit=[&](const Triangle& triangle) {
        int steps=Steps(start,finish);
        for(int i=0;i<steps;++i) if(SweepCell(Between(start,finish,float(i)/steps),Between(start,finish,float(i+1)/steps),triangle)) return true;
        return false;
    };
    // This contact lies outside the one-chord triangle but inside the real arc.
    if(!arcHit({{.65f,.65f,-.01f},{.67f,.65f,.01f},{.65f,.67f,.01f}})) return false;
    if(arcHit({{.8f,.8f,-.01f},{.82f,.8f,.01f},{.8f,.82f,.01f}})) return false;
    if(arcHit({{-.2f,.2f,-.01f},{-.22f,.2f,.01f},{-.2f,.22f,.01f}})) return false;
    Blade opposite=Between({{0,0,0},{0,1,0}},{{0,0,0},{0,-1,0}},1);
    if(!Finite(opposite.tip) || magnitude(opposite.tip-Vector(0,-1,0))>.00001f) return false;
    return true;
}
}
