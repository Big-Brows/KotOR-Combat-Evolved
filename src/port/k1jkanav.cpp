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
// K12 grounded port of OpenJK 260c59c g_navigator.cpp STEER and Raven A*,
// bg_slidemove.cpp PM_SlideMove and bg_pmove.cpp PM_ClipVelocity.
// Entity/map/collision/clock bindings replace engine globals; source algorithms
// and imported graph/math code retain GPL-2.0-only. See third_party/openjk/.

#include "k1jkanav.h"
#if KOTOR1 && defined(K2_UI_PC)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>
#define FINAL_BUILD
// Host binding required by ratl string comparisons (graph math is unchanged).
static int Q_stricmp(const char* a,const char* b) { return _stricmp(a,b); }
#include "../../third_party/openjk/Ravl/CVec.cpp"
#include "../../third_party/openjk/Ragl/graph_vs.h"

namespace K1JkaNav {
namespace {
CVec3 vec(Point p) { return CVec3(p.x,p.y,p.z); }
Point point(const CVec3& v) { return Point(v[0],v[1],v[2]); }
struct SSteerUser {
    CVec3 mPosition,mVelocity,mSteering,mDesiredVelocity,mSeekLocation,mProjectFwd,mProjectSide;
    float mMaxSpeed,mMaxForce,mMass,mSpeed,mDistance=0,mDesiredSpeed=0;
    explicit SSteerUser(const State& s):mPosition(vec(s.position)),mVelocity(vec(s.velocity)),
        mSteering(0.f),mDesiredVelocity(0.f),mSeekLocation(vec(s.seek)),mProjectFwd(0.f),mProjectSide(0.f),
        mMaxSpeed(s.maxSpeed),mMaxForce(s.maxForce),mMass(s.mass),mSpeed(vec(s.velocity).Len()) {}
};

// STEER::Seek: body retained from g_navigator.cpp; only the entity/pool lookup
// is replaced by an explicit steer-user argument.
float Seek(SSteerUser& suser,const CVec3& pos,float slowingDistance=0,float weight=1,float desiredSpeed=0) {
    suser.mSeekLocation = pos;
    suser.mDesiredVelocity = suser.mSeekLocation - suser.mPosition;
    if (fabsf(suser.mDesiredVelocity[2]) < 10.0f) suser.mDesiredVelocity[2] = 0.0f;
    suser.mDistance = suser.mDesiredVelocity.SafeNorm();
    if (suser.mDistance>0.0f) {
        suser.mDesiredSpeed = (desiredSpeed!=0.0f)?(desiredSpeed):(suser.mMaxSpeed);
        if (slowingDistance!=0.0f && suser.mDistance < slowingDistance)
            suser.mDesiredSpeed *= (suser.mDistance/slowingDistance);
        suser.mDesiredVelocity *= suser.mDesiredSpeed;
    } else {
        suser.mDesiredSpeed = 0.0f;
        suser.mDesiredVelocity.Clear();
    }
    suser.mSteering += ((suser.mDesiredVelocity - suser.mVelocity) * weight);
    return suser.mDistance;
}
void Stop(SSteerUser& suser) {
    suser.mDesiredVelocity.Clear();suser.mDistance=0;suser.mDesiredSpeed=0;
    suser.mSteering += suser.mDesiredVelocity-suser.mVelocity;
}

// g_navigator.cpp TestCollision. Entity tests are supplied by the K12 adapter;
// unsupported cinematic/breakable/flying policies are not fabricated here.
bool TestCollision(State& actor,SSteerUser& suser,const CVec3& ProjectVelocity,
                   float ProjectSpeed,ESide Side,float weight,World& world,long long now) {
    const Contact hit=world.trace(point(suser.mPosition),
        point(Side==Side_None?suser.mProjectFwd:suser.mProjectSide),false);
    bool Safe=hit.clear || hit.id==actor.ignore || hit.door;
    CVec3 ContactNormal(vec(hit.normal)),ContactPoint(vec(hit.position));
    if(!Safe && Side==Side_None) {
        if(hit.character) {
            CVec3 ContactVelocity(vec(hit.velocity));
            const float ContactSpeed=ContactVelocity.Len();
            if(ContactSpeed>0.01f) {
                if(ContactSpeed<ProjectSpeed) {
                    CVec3 MyDirection(ProjectVelocity),ContactDirection(ContactVelocity);
                    ContactDirection.Norm();MyDirection.Norm();
                    const float DirectionSimilarity=fabsf(MyDirection.Dot(ContactDirection));
                    if(DirectionSimilarity>0.5f) {
                        suser.mDesiredVelocity=suser.mVelocity;
                        suser.mDesiredVelocity.Truncate(ContactSpeed);
                        suser.mSteering+=((suser.mDesiredVelocity-ProjectVelocity)*DirectionSimilarity);
                        actor.ignore=hit.id;Safe=true;
                    }
                }
            } else {
                const CVec3 Next(suser.mSeekLocation),AbsMin(vec(hit.minimum)),AbsMax(vec(hit.maximum));
                if(Next>AbsMin && Next<AbsMax) {
                    suser.mSteering-=ProjectVelocity;actor.ignore=hit.id;Safe=true;
                }
            }
        }
        if(!Safe && ContactNormal[2]>0.f && ContactNormal[2]<.7f) Safe=true;
        if(!Safe) {
            CVec3 ProjectDirection(ProjectVelocity);ProjectDirection.Norm();
            ContactNormal.Cross(CVec3::mZ);
            if(actor.lastAvoidSide!=Side_None && actor.sideDebounce<now) {
                actor.lastAvoidSide=Side_None;actor.sideDebounce=now+world.random(500,2000);
            }
            if(actor.lastAvoidSide==Side_Right ||
                (actor.lastAvoidSide==Side_None && ContactNormal.Dot(ProjectDirection)<0.f)) {
                ContactNormal*=-1.f;actor.lastAvoidSide=Side_Right;
            } else actor.lastAvoidSide=Side_Left;
            ContactNormal[2]=0.f;
            ContactNormal*=(ProjectSpeed*weight*.5f);ContactPoint+=ContactNormal;
            // Preserve the upstream third-argument (slowingDistance) call.
            Seek(suser,ContactPoint,weight);
        } else if(Side==Side_None) actor.lastAvoidSide=Side_None;
    }
    return !Safe;
}
float AvoidCollisions(State& actor,SSteerUser& suser,World& world,long long now) {
    // g_navigator.cpp STEER::AvoidCollisions, with entity selection bound to
    // the explicit enemy id and world trace callback.
    actor.ignore=actor.enemy;
    CVec3 ProjectedSteering(suser.mSteering),ProjectedVelocity(suser.mVelocity);
    float ProjectedSpeed=suser.mSpeed;
    const float Newtons=ProjectedSteering.Truncate(suser.mMaxForce);
    if(Newtons>1E-10f) {
        ProjectedSteering/=suser.mMass;ProjectedVelocity+=ProjectedSteering;
        ProjectedSpeed=ProjectedVelocity.Truncate(suser.mMaxSpeed);
    }
    if(ProjectedSpeed>0.01f) {
        CVec3 ProjectVelocitySide(ProjectedVelocity);
        ProjectVelocitySide.Reposition(CVec3::mZero,actor.avoidSide==Side_Left?40.f:-40.f);
        suser.mProjectFwd=suser.mPosition+ProjectedVelocity;
        suser.mProjectSide=suser.mPosition+ProjectVelocitySide*.3f;
        const bool HitFront=TestCollision(actor,suser,ProjectedVelocity,ProjectedSpeed,Side_None,1.f,world,now);
        const bool HitSide=TestCollision(actor,suser,ProjectedVelocity,ProjectedSpeed,
            static_cast<ESide>(actor.avoidSide),.5f,world,now);
        if(!HitSide) actor.avoidSide=actor.avoidSide==Side_Left?Side_Right:Side_Left;
        if(HitFront || HitSide) {++actor.avoidances;return ProjectedSpeed;}
    }
    return 0;
}
void DeActivate(State& actor,SSteerUser& suser) {
    // g_navigator.cpp STEER::DeActivate phase I. The engine adapter performs
    // the source ground mover and collision integration, rather than AI walking.
    const float Newtons=suser.mSteering.Truncate(suser.mMaxForce);
    if(Newtons>1E-10f) {
        suser.mSteering/=suser.mMass;suser.mVelocity+=suser.mSteering;
        suser.mSpeed=suser.mVelocity.Truncate(suser.mMaxSpeed);
    }
    actor.velocity=point(suser.mVelocity);actor.steering=point(suser.mSteering);
    actor.desiredVelocity=point(suser.mDesiredVelocity);actor.seek=point(suser.mSeekLocation);
    actor.projectForward=point(suser.mProjectFwd);actor.projectSide=point(suser.mProjectSide);
    actor.speed=suser.mSpeed;actor.distance=suser.mDistance;actor.desiredSpeed=suser.mDesiredSpeed;
}
struct Node { CVec3 position; };
struct Edge { int a=0,b=0;float distance=0; };
// Larger storage capacities adapt KotOR meshes; the imported graph search is
// unchanged. Capacity exhaustion fails explicitly, never delegates to K12 AI.
constexpr int MaxNodes=4096,MaxEdges=MaxNodes*8,MaxNeighbors=20;
using Graph=ragl::graph_vs<Node,MaxNodes,Edge,MaxEdges,MaxNeighbors>;
struct GraphUser:Graph::user {
    Graph& graph;World& world;
    GraphUser(Graph& g,World& w):graph(g),world(w){}
    bool can_be_invalid(const Edge&)const override{return true;}
    bool is_valid(Edge& e,int)const override {
        return world.trace(point(graph.get_node(e.a).position),point(graph.get_node(e.b).position),true).clear;
    }
    float cost(const Node& a,const Node& b)const override{return a.position.Dist(b.position);}
    float cost(const Edge& e,const Node&)const override{return e.distance;}
    bool on_same_floor(const Node& a,const Node& b)const override{return fabsf(a.position[2]-b.position[2])<60.f;}
    void setup_edge(Edge& e,int a,int b,bool,const Node& na,const Node& nb,bool)override {
        e.a=a;e.b=b;e.distance=na.position.Dist(nb.position);
    }
};
}
struct Navigator::Impl {
    Graph graph;
    int nearest(CVec3 at,World& world) {
        std::vector<std::pair<float,int>> candidates;
        for(int i=1;i<=graph.size_nodes();++i) if(graph.node_has_neighbors(i))
            candidates.emplace_back(at.Dist2(graph.get_node(i).position),i);
        std::sort(candidates.begin(),candidates.end());
        for(const auto& c:candidates) if(world.trace(point(at),point(graph.get_node(c.second).position),true).clear) return c.second;
        return 0;
    }
};
// PM_ClipVelocity from bg_pmove.cpp: wall-sticking and player-only slope policy
// do not apply to the grounded NPC binding. The source overclip is preserved.
static CVec3 ClipVelocity(const CVec3& input,const CVec3& normal) {
    float backoff=input.Dot(normal);
    if(backoff<0) backoff*=1.001f;else backoff/=1.001f;
    return input-normal*backoff;
}
bool slide(Point& position,Point& velocity,float seconds,World& world) {
    // bg_slidemove.cpp PM_SlideMove ground/no-gravity branch. Four bumps,
    // five planes, duplicate-plane nudge, double-plane crease and triple-plane
    // stop are retained. Gravity, vehicle impacts, wall-running and jump/step
    // policy remain outside this ground-only adapter.
    CVec3 at(vec(position)),speed(vec(velocity));
    CVec3 planes[5];planes[0]=CVec3::mZ;planes[1]=speed;planes[1].SafeNorm();
    int numplanes=2;float time_left=seconds;bool clipped=false;
    for(int bumpcount=0;bumpcount<4;++bumpcount) {
        const CVec3 end=at+speed*time_left;
        const auto trace=world.trace(point(at),point(end),false);
        const float fraction=trace.clear?1.f:std::max(0.f,std::min(1.f,trace.fraction));
        if(fraction>0) at=vec(trace.position);
        if(fraction==1.f) break;
        clipped=true;time_left-=time_left*fraction;
        if(numplanes>=5) {speed.Clear();break;}
        CVec3 normal(vec(trace.normal));normal.SafeNorm();
        int i=0;
        for(;i<numplanes;++i) if(normal.Dot(planes[i])>.99f) {speed+=normal;break;}
        if(i<numplanes) continue;
        planes[numplanes++]=normal;
        for(i=0;i<numplanes;++i) {
            const float into=speed.Dot(planes[i]);
            if(into>=.1f) continue;
            CVec3 clipVelocity=ClipVelocity(speed,planes[i]);
            for(int j=0;j<numplanes;++j) {
                if(j==i || clipVelocity.Dot(planes[j])>=.1f) continue;
                clipVelocity=ClipVelocity(clipVelocity,planes[j]);
                if(clipVelocity.Dot(planes[i])>=0) continue;
                CVec3 dir(planes[i]);dir.Cross(planes[j]);dir.SafeNorm();
                clipVelocity=dir*dir.Dot(speed);
                for(int k=0;k<numplanes;++k) if(k!=i && k!=j && clipVelocity.Dot(planes[k])<.1f) {
                    velocity=Point();position=point(at);return true;
                }
            }
            speed=clipVelocity;break;
        }
    }
    position=point(at);velocity=point(speed);return clipped;
}
bool groundMove(Point& position,Point& velocity,Point wishVelocity,float seconds,World& world,float speedLimit) {
    // Original bg_pmove.cpp PM_Friction + PM_Accelerate, ground-only/no slick,
    // no water/knockback branch. NPCs use STEER's moveDir and speed, not the
    // player's command scaling or saber slowdown. NPC.cpp runs pmove at50ms.
    CVec3 speed(vec(velocity));const float length=speed.Len();
    if(length<1.f) speed.Clear();
    else {
        const float control=length<100.f?100.f:length;
        const float newspeed=std::max(0.f,length-control*6.f*seconds)/length;
        speed*=newspeed;
    }
    CVec3 wishdir(vec(wishVelocity));const float wishspeed=wishdir.SafeNorm();
    const float currentspeed=speed.Dot(wishdir);
    const float addspeed=wishspeed-currentspeed;
    if(addspeed>0) {
        float accelspeed=(12.f*seconds)*wishspeed;
        if(accelspeed>addspeed) accelspeed=addspeed;
        speed+=wishdir*accelspeed;
    }
    // Explicit host walking policy; source PM_Accelerate only limits the
    // component along wishdir, so a turn can retain extra lateral velocity.
    if(speedLimit>0) speed.Truncate(speedLimit);
    velocity=point(speed);return slide(position,velocity,seconds,world);
}
Navigator::Navigator():impl(new Impl){}
Navigator::~Navigator(){delete impl;}
int Navigator::nodes()const{return impl->graph.size_nodes();}
int Navigator::edges()const{return impl->graph.size_edges();}
Point Navigator::node(int index)const{return point(impl->graph.get_node(index).position);}
bool Navigator::build(const std::vector<Point>& points,World& world) {
    auto& graph=impl->graph;graph.clear();
    if(points.empty() || points.size()>=MaxNodes) return false;
    for(auto p:points) graph.insert_node({vec(p)});
    for(int a=1;a<=graph.size_nodes();++a) {
        std::vector<std::pair<float,int>> neighbors;
        const auto at=graph.get_node(a).position;
        for(int b=a+1;b<=graph.size_nodes();++b) {
            const float d=at.Dist2(graph.get_node(b).position);
            if(d>0.f && d<=200.f*200.f) neighbors.emplace_back(d,b);
        }
        std::sort(neighbors.begin(),neighbors.end());
        for(const auto& n:neighbors) {
            if(graph.get_node_neighbors(a).size()>=MaxNeighbors-1) break;
            if(graph.get_node_neighbors(n.second).size()>=MaxNeighbors-1) continue;
            if(graph.size_edges()>=MaxEdges-2) return false;
            if(!world.trace(point(at),point(graph.get_node(n.second).position),true).clear) continue;
            graph.connect_node({a,n.second,std::sqrt(n.first)},a,n.second);
        }
    }
    return graph.size_edges()>0;
}
bool Navigator::move(State& actor,Point goal,float reachedRadius,World& world,long long now) {
    SSteerUser suser(actor);const CVec3 destination(vec(goal));
    actor.active=true;actor.blocked=false;
    if(suser.mPosition.Dist2(destination)<reachedRadius*reachedRadius) {
        actor.path.clear();Stop(suser);DeActivate(actor,suser);return true;
    }
    if(world.trace(actor.position,goal,true).clear) {
        actor.path.clear();Seek(suser,destination,reachedRadius*2.f);
    } else {
        auto& graph=impl->graph;
        const int target=impl->nearest(destination,world);
        if(!target) {actor.path.clear();Stop(suser);DeActivate(actor,suser);return false;}
        if(actor.path.empty() || actor.path.front()!=target) {
            const int start=impl->nearest(suser.mPosition,world);
            if(!start) {Stop(suser);DeActivate(actor,suser);return false;}
            Graph::search search(start,target);GraphUser user(graph,world);
            graph.astar(search,user);++actor.searches;actor.path.clear();
            if(!search.success()) {Stop(suser);DeActivate(actor,suser);return false;}
            for(search.path_begin();!search.path_end();search.path_inc()) actor.path.push_back(search.path_at());
        }
        // NAV's source path is end-to-start. Consume reached points and skip
        // only a directly traceable next segment; terrain traces protect ledges.
        while(actor.path.size()>1) {
            const auto next=graph.get_node(actor.path.back()).position;
            const auto after=graph.get_node(actor.path[actor.path.size()-2]).position;
            if(suser.mPosition.Dist2(next)<actor.radius*actor.radius*9.f &&
                world.trace(actor.position,point(after),true).clear) actor.path.pop_back();
            else break;
        }
        const auto next=graph.get_node(actor.path.back()).position;
        Seek(suser,next,actor.path.size()==1?70.f:0.f);
    }
    actor.blocked=AvoidCollisions(actor,suser,world,now)>0;
    DeActivate(actor,suser);return true;
}
bool probe() {
    struct TestWorld:World {
        bool wall=false;
        Contact trace(Point from,Point to,bool) override {
            Contact c;c.position=to;
            if(wall && (from.x-50.f)*(to.x-50.f)<0.f) {
                const float f=(50.f-from.x)/(to.x-from.x);
                const float y=from.y+(to.y-from.y)*f;
                if(std::fabs(y)<40.f) {
                    c.clear=false;c.fraction=f;c.position=Point(50,y,0);c.normal=Point(from.x<50?-1.f:1.f,0,0);
                }
            }
            return c;
        }
        int random(int low,int)override{return low;}
    } world;
    State state;Navigator navigator;
    bool ok=navigator.build({Point(0,0,0),Point(20,60,0),Point(80,60,0),Point(100,0,0)},world);
    ok=ok && navigator.move(state,Point(100,0,0),1.f,world,0);
    // Original Seek + DeActivate force limits: first150, then180 units/s.
    ok=ok && std::fabs(state.velocity.x-150.f)<.0001f && std::fabs(state.velocity.y)<.0001f;
    navigator.move(state,Point(100,0,0),1.f,world,100);
    ok=ok && std::fabs(state.velocity.x-180.f)<.0001f;
    state.position=Point(100,0,0);navigator.move(state,Point(100,0,0),1.f,world,200);
    ok=ok && std::fabs(state.velocity.x-30.f)<.0001f;
    navigator.move(state,Point(100,0,0),1.f,world,300);
    ok=ok && vec(state.velocity).Len()<.0001f;
    state=State();world.wall=true;
    ok=ok && navigator.move(state,Point(100,0,0),1.f,world,400) && state.searches==1 && state.path.size()>=2;
    const bool sourceRoute=state.path.size()>=2;
    bool safe=true;
    for(int frame=0;frame<300 && safe && vec(state.position).Dist(CVec3(100,0,0))>2.f;++frame) {
        navigator.move(state,Point(100,0,0),1.f,world,400+frame*16);
        const Point next=point(vec(state.position)+vec(state.velocity)*(1.f/60.f));
        safe=world.trace(state.position,next,false).clear;state.position=next;
    }
    const float distance=vec(state.position).Dist(CVec3(100,0,0));
    ok=ok && sourceRoute && safe && distance<2.f;
    Point slideAt(49,0,0),slideSpeed(100,50,0);
    const bool clipped=slide(slideAt,slideSpeed,.04f,world);
    ok=ok && clipped && slideAt.x<=50.001f && slideAt.y>1.8f && std::fabs(slideSpeed.y-50.f)<.001f;
    world.wall=false;Point walkingAt,walkingVelocity;
    for(int step=0;step<20;++step) groundMove(walkingAt,walkingVelocity,Point(45,0,0),.05f,world);
    // This is the source50ms NPC result. An8ms player mover produces a crawl.
    ok=ok && std::fabs(walkingVelocity.x-27.f)<.001f && std::fabs(walkingAt.x-27.f)<.001f;
    Point cappedAt,cappedVelocity(90,90,0),sourceAt,sourceVelocity(90,90,0);
    groundMove(cappedAt,cappedVelocity,Point(126,0,0),.05f,world,126.f);
    groundMove(sourceAt,sourceVelocity,Point(126,0,0),.05f,world);
    const float cappedSpeed=vec(cappedVelocity).Len(),sourceSpeed=vec(sourceVelocity).Len();
    const bool walkCap=std::fabs(cappedSpeed-126.f)<.001f &&
        std::fabs(vec(cappedAt).Len()-6.3f)<.001f && sourceSpeed>140.f;
    ok=ok && walkCap;
    fprintf(stderr,"K1_JKA_WALK_LIMIT_%s capped=%g source=%g distance=%g\n",
        walkCap?"PASS":"FAIL",cappedSpeed,sourceSpeed,vec(cappedAt).Len());
    fprintf(stderr,"K1_JKA_NAV_SOURCE_%s nodes=%d edges=%d searches=%d distance=%g safe=%d\n",
        ok?"PASS":"FAIL",navigator.nodes(),navigator.edges(),state.searches,distance,safe);
    return ok;
}
}
#endif
