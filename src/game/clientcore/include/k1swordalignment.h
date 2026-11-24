#pragma once

namespace K1SwordAlignment {
// Native solid swords have their blade axis along Z and their flat normal along
// X. A saber's round beam hid this axial orientation. Keep the authored shaft
// direction and grip, but put the sword's edge in the intended cutting plane.
inline bool Align(Quaternion& orientation,const Quaternion& facing,int direction) {
    if(direction<0 || direction>6)return false;
    // Yellow's swapped A clip and both backward diagonals need the opposite
    // blade facing. Reverse the normal to flip only axial roll by180 degrees.
    constexpr float normalX[]={1,1,1,0,0,-1,-1};
    constexpr float normalZ[]={0,1,-1,1,-1,1,-1};
    const Quaternion raw=normalize(orientation);
    const Vector axis=raw.zdir();
    Vector normal=facing.xdir()*normalX[direction]+facing.zdir()*normalZ[direction];
    normal=normal-axis*dot(normal,axis);
    if(dot(normal,normal)<.000001f)return false;
    normal=normalize(normal);
    const float roll=std::atan2(dot(raw.ydir(),normal),dot(raw.xdir(),normal));
    orientation=normalize(raw*Quaternion(Vector(0,0,1),roll));
    return true;
}
}
