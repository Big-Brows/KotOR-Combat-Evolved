#pragma once
// SPDX-License-Identifier: GPL-2.0-only
#include <cstdint>
#include <vector>

// K12's boundary to the ported OpenJK navigator. Positions/speeds are in JKA
// units here; map and actor ownership stay in K12. No native AI path requests.
namespace K1JkaNav {
struct Point { float x=0,y=0,z=0; Point()=default; Point(float a,float b,float c):x(a),y(b),z(c){} };
struct Contact {
    bool clear=true,character=false,door=false;
    float fraction=1.f;
    std::uint64_t id=~std::uint64_t(0);
    Point position,normal,velocity,minimum,maximum;
};
struct World {
    virtual ~World()=default;
    virtual Contact trace(Point from,Point to,bool ignoreCharacters)=0;
    virtual int random(int low,int high)=0;
};
struct State {
    Point position,velocity,steering,desiredVelocity,seek,projectForward,projectSide;
    float maxSpeed=180,maxForce=150,mass=1,radius=12,height=60,speed=0,distance=0,desiredSpeed=0;
    std::uint64_t enemy=~std::uint64_t(0),ignore=~std::uint64_t(0);
    int avoidSide=1,lastAvoidSide=0;
    long long sideDebounce=0;
    std::vector<int> path;
    Point pathGoal;
    bool active=false,blocked=false;
    int searches=0,avoidances=0;
    void stop() { velocity=Point();steering=Point();path.clear();active=false; }
};
class Navigator {
public:
    Navigator();
    ~Navigator();
    Navigator(const Navigator&)=delete;
    Navigator& operator=(const Navigator&)=delete;
    bool build(const std::vector<Point>& points,World& world);
    bool move(State& actor,Point goal,float reachedRadius,World& world,long long now);
    int nodes() const;
    int edges() const;
    Point node(int index) const;
private:
    struct Impl;
    Impl* impl;
};
// Source-equivalence and route contract probe; no game data needed.
bool probe();
// Grounded PM_SlideMove/PM_ClipVelocity port, using the same collision binding.
bool slide(Point& position,Point& velocity,float seconds,World& world);
// Optional host policy caps post-acceleration speed before collision integration.
// Zero preserves source acceleration, including speed gained while turning.
bool groundMove(Point& position,Point& velocity,Point wishVelocity,float seconds,World& world,float speedLimit=0);
}
