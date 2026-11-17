# OpenJK navigation dependencies

Source: JACoders/OpenJK revision 260c59c2907187af555a676fe0cc798893bf7757.
Upstream: https://github.com/JACoders/OpenJK/tree/260c59c2907187af555a676fe0cc798893bf7757/code

These Raven graph/container/vector files are imported source under GPL-2.0-only,
with original notices intact. COPYING contains upstream's license. K12's existing
license, credits and other third-party notices are preserved.

The K1 adapter in src/port/k1jkanav.cpp uses the original ragl::graph_vs A* and
ported g_navigator.cpp STEER algorithms. Engine-specific entity, map, collision,
clock and movement bindings are documented in docs/DESIGN.md. The .nav file loader,
JKA BSP dependencies and game renderer are not imported. No game assets belong here.

The grounded PM_Friction/PM_Accelerate/PM_ClipVelocity/PM_SlideMove branch is
ported in the same adapter from code/game/bg_pmove.cpp and bg_slidemove.cpp.
src/port/k1jkaai.cpp ports the ordinary medium-cultist aggression/strafe/timer and
applicable combat-distance branches of code/game/AI_Jedi.cpp. Its explicit
profile limitations are not a claim to port that entire AI translation unit.
The same module also ports bg_pmove.cpp's backpedal command rule and
NPC_utils.cpp's ordinary NPC_UpdateAngles yaw branch, with K12 float orientation
and display interpolation replacing final usercmd angle packing.
The source-reference blob manifest and original/fixed failures are retained in
ignored platforms/out/k1-engage-prototype/jka-direct-nav/.

Local changes to these dependency files, if required by the host compiler, must
be listed here and recorded in the ignored source/diff manifest. At import they
are byte-for-byte copies of the pinned Git blobs.
