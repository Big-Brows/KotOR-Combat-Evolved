#pragma once

// Preview-authored hook changes are relative to the original animation. This
// preserves native rig proportions and the existing procedural pose services.
namespace K1YellowSwordHooks {
struct Key { float frame,value; };
struct Curve { const Key* keys;unsigned count; };
struct Clip { const char* name;Curve baseline[4],edited[4]; };
#include "k1yellowswordhooks.inc"
inline float Sample(const Curve& curve,float frame) {
    if(frame<=curve.keys[0].frame)return curve.keys[0].value;
    for(unsigned i=1;i<curve.count;++i)if(frame<curve.keys[i].frame) {
        const auto& a=curve.keys[i-1];const auto& b=curve.keys[i];
        const float t=(frame-a.frame)/(b.frame-a.frame);
        return a.value+(b.value-a.value)*t;
    }
    return curve.keys[curve.count-1].value;
}
inline Quaternion Rotation(const Curve* curves,float frame) {
    return normalize(Quaternion(Sample(curves[0],frame),Sample(curves[1],frame),
        Sample(curves[2],frame),Sample(curves[3],frame)));
}
inline bool Correction(const char* name,float seconds,Quaternion& correction) {
    if(!name)return false;
    for(const auto& clip:clips)if(!strcmp(name,clip.name)) {
        const float frame=1.f+seconds*30.f;
        const Quaternion baseline=Rotation(clip.baseline,frame);
        correction=normalize(Quaternion(baseline.w,-baseline.x,-baseline.y,-baseline.z)*Rotation(clip.edited,frame));
        return true;
    }
    return false;
}
}
