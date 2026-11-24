# KotOR:CE local K1 Engage prototype

## Lossless portable model streams

Portable packages can opt into KLM1 descriptors for loose MDL/MDX resources using
data/lazy-models-v1. A descriptor contains only three lowercase SHA-256 values:
native base, KDP1 delta and reconstructed target. Its sibling .k1models directory
contains deduplicated base/delta inputs. No machine-specific paths are embedded.
The existing resource directory enumeration and CExoFile -> SDL stream path remain
in use. Raw models and packages without the marker retain their existing behavior.

The Windows stream adapter validates fixed descriptor syntax, rejects reparse
paths, checks input hashes, and inspects each delta target size before applying it.
It writes exact reconstructed chunks to a delete-on-close temporary file, checking
the complete target hash before exposing the seekable stream. Errors release all
handles/buffers and never expose the descriptor as model data. Empty MDX is valid.

A mutex serializes reconstruction within the process. Source/patch inputs are
limited to 64 MiB each, targets to 32 chunks of at most 16 MiB. MSDelta requires
additional internal scratch memory. There is no additional expanded-model RAM
cache: native CRes demand/release and resident caching remain authoritative, and
active streams are never evicted. OS file caching remains reclaimable. This changes
storage and loading only; animation data, fitting, weights, bind transforms,
attachment hooks, dismemberment meshes and gameplay tuning are byte-preserved.

The backend is compiled separately from K12's Win32 compatibility/no-exception
dialect. Native model/pose probes are opt-in smoke checks, not gameplay paths.
Repeated loading of the largest 75,216,621-byte model (22 opens, two simultaneous
stream lifetimes) peaked at 28,499,968 bytes working set in the standalone probe;
that is not a bound on total game memory. Focused raw/compact native animation
probe times were 28.99/31.35 seconds on this machine, not a general benchmark.

## Red motion, guard and JKA directional chains

Red uses separate player/NPC resources. Its first attack retains the S3/A3/R3
sequence. Subsequent cuts select the owned T3 transition from the outgoing end
quadrant to the incoming start quadrant, followed by A3/R3; matching quadrants
start A3 directly. The preserved OpenJK SP bg_panimate.cpp transition table supplies
all49 selectors, including the bottom-quadrant fallback. Existing local swing rates,
three-swing limit, NPC chain chance and poise policy remain intentional differences
from full JKA gameplay rules. No JKA special moves or balance replacement is implied.

The exporter retargets source components once and builds the full-body/upper-body
variants through the existing model writer, with fixed initial-correction blends.
The engine timing contract in include/k1redchain.inc is verified against both exports.
Player and per-NPC outgoing-direction state selects clips, contact windows and
full-pose references for moving layers. Transition time is excluded from damage;
existing strike follow-through and contact cancellation services remain active.
Fixed sides0/2 weapon tags avoid the older deformed-tag rotation flips. Completed
yellow/staff/dual resources and specific tuning remain unchanged.

Red idle guard uses the original first-export j_guard/y_guard controllers. Later
marker-spacing, wrist, finger, elbow and helper-frame alignment corrections were
reverted at user request. The generator isolates the original retarget convention
for guard while retaining the corrected attack/transition sampler. Player guard
controllers are checked against the preserved first export; player/NPC loaded
control samples verify both full and upper guard clips. Native-grip alignment
assertions were removed because they conflict with the requested original pose.
Yellow/red switching snapshots displayed local joint values by name through native
appearance replacement, then seeds the existing240ms per-body pose/grip blend.
Held guard stays held; outgoing swings/contacts are canceled. Reversals start from
the displayed pose. Red/cross-stance blend clocks advance at most25ms per displayed
frame to tolerate rebuild stalls. Native running and ordinary completed playback
retain their established services and timing.
## Portable Windows startup

The product name is KotOR:CE, with KotOR-CE used in Windows filenames. The
portable build opts into K2P_PRODUCT_NAME to set the K1 SDL window title; the
launcher has matching dialog and Windows product metadata. K12 engine names,
configuration keys, upstream attribution and the checkout directory stay intact.

The native launcher in `platforms/launcher` calls the existing K12 game-directory
service for KotOR discovery. Its JKA helper shares Steam library discovery and
reads GOG registrations; manual folder selection is the fallback. The launcher
starts the same K1 engine with the existing sandbox/staff environment selectors,
without replacing combat, loaders or object ownership.

Portable preparation uses the Windows MSDelta API with bounded 16 MiB chunks,
relative source-file ranges and SHA-256 validation. An explicit package recipe
creates patches from the accepted local assets; setup reconstructs them from
verified owned-data inputs into a locked `user-data` profile. Atomic resource
writes and an atomic initial-save-directory installation support interrupted
setup and preserve existing saves. The launcher refreshes the isolated SAVES
alias after relocation and refuses a package placed inside either game install.

Privacy compilation is opt-in and uses a separate binary tree. MSVC path mapping,
deterministic compilation and disabled private debug output apply to the engine
and SDL sub-build. The developer preset is unchanged. Package and binary scans
exclude owner identifiers, populated Windows user paths and CodeView/PDB records;
runtime logs/settings stay outside the fresh archive. Usage, exact checks and
version/redistribution limitations are in `docs/PORTABLE.md`.

## Custom combat utilities and player blunt weapons

Owned custom-combat actors retain native Force execution, cost, targeting and
impact scripts. Plain damage skips concentration cancellation and damage flinch
while a Force cast owns the native spell round. Death and status effects retain
their native services. Recovery is capped at100ms; completed spell/item executors
end the spent round through `EndCombatRound(0)` before processing pending actions.
Both native item-animation completion paths use this release. Active casts and
queued action lists are not bulk-cleared. Target/personal GUI handlers preserve
queues while Engage is active/planning. Mine menu entries translate server item
IDs to client IDs before the native input message performs placement.

Player admission includes native baton, quarterstaff, Gaffi and battleaxe categories.
The existing NPC solid-weapon sampler supplies their actual drawn mesh extent;
no powered-saber stand-in is required. Native wield types select single/staff
services without editing authored stance assets. A baton may successfully parry;
if its parry fails, held guard returns false and native contact applies damage.
These changes do not implement unarmed or creature-natural player combat.

## Local saber contact feedback and sandbox movement recovery

Custom clash, guard and parry resolutions reuse native `CSWCVisualEffect` row4011
(`v_parrspr_imp`), the payload used by KotOR's own saber HitContactEvent. Its two
spark emitters and authored animated light use normal client VFX lifetime services.
The position comes from closest points on the actual drawn weapon segments, with
powered-saber gating. No shared model or animation is modified.
Successful player parries flash the full dial yellow for300ms on a steady clock;
existing directional/guard selection remains underneath and GL state is restored.

Surface burns sample powered sabers in both hands and both staff tips, with an
independent trail per blade. First contact stamps the actual native hit point so
a one-frame touch can leave a mark. Continuous scars retain native receiver/normal
checks and cannot bridge misses, different weapons/receivers or teleports.

Sandbox player movement repairs a cached sub-normal movement factor only when no
native speed-increase/decrease effect remains to provide it. This is deliberate
sandbox recovery policy, not an original-engine matching claim. Live slow effects
remain effective and expire through native services. Rate changes log providers
for diagnosing unreproduced slowdowns. Focus loss releases custom held input;
menu key-up events are accepted to prevent a latched Shift walk.

## Sandbox menu and NPC ownership

Wearable-body adaptation extends the player appearance path through
`k1armorrigs.2da`. It selects six separate style resources by the native appearance
body model, including all male/female small/medium/large armor, robe, clothing and
underwear families, unique humanoid companion clothes and native armor disguises.
PMBBL keeps its six original authored resources. The existing native NPC rig table
and NPC stance admission remain separate and retain their completed behavior.

`prepare-k1-wearable-rigs.py` audits owned appearance/base-item/item data and invokes
the existing native-body transfer with player donors. Original meshes, UVs,
materials, non-spine skin weights and bind frames remain native; palette splitting
preserves all faces, with exported inverse-bind and animation-key roundtrip checks.
The new resources and provenance stay ignored. `prepare-k1-wearable-checks.py`
finalizes a verified manifest and native equipment witnesses. Launcher deployment
checks hashes and authored donor identity under the existing isolated-profile lock.

Runtime appearance selection uses native B-type model/texture lookup, or the native
race model for disguise bodies. Native equipment and appearance updates rebuild
the appropriate source body and retain head/weapon attachment ownership. External
body replacement invalidates outgoing pose/contact identities in the native
creature body-unload callback before deletion (the later load callback is too
late, including when the allocator reuses a body address);
ordinary stance rebuilds keep their existing blend path. Idle, guard and running
samplers use that body's native reference. Saved authored guard coordinates are
converted through bind frames and height for new bodies; PMBBL tuning is unchanged.
Native utility/status handoffs and original-model restoration use the same engine
services. Existing dismemberment mappings resolve adapted models to their native
body names; adding further cut-mesh families is a separate task.

The native unnamed-humanoid catalog, separate body/style resources, indexed-bone
preservation and native ownership boundary are documented in
[MELEE_NPC_COVERAGE.md](MELEE_NPC_COVERAGE.md). This extends the sandbox only;
the historical Dark Jedi-only catalog description below records its foundation.

The current three-column faction/type/variant browser is documented in
[SANDBOX_SPAWN_MENU.md](SANDBOX_SPAWN_MENU.md). Its generated metadata references
unchanged UTC templates through independent stat and cosmetic aliases. A runtime
appearance callback applies body/head/armor cosmetics while retaining source
stats and equipment. Sandbox-only maps select compatible Red/Blue body rigs;
registered sandbox actors clear Plot/Min1HP protection. Native list boxes own their rows, the modal owns its preview
scene, and existing spawn services own arena actors. The global target-player
policy is sandbox-scoped and enforced by reputation queries plus native attack
and spell admission. The existing threat scan recognizes an opposing NPC pair
when player targeting is disabled, preserving combat activation without making
the player a target. The description below is the earlier fixture-menu design.

Top-row0 opens a native modal CSWGuiPanel in the isolated arena. The panel reuses
KotOR controls, fonts and borders; opening locks movement and holds the native
pause. Closing restores the prior pause/planning/active state after pending pause
requests finish. Other menus/dialogs retain their input ownership. The NPC catalog
currently exposes Dark Jedi with separate Yellow/Double-blade/Dual-saber stance selection.
Sandbox startup leaves the established arena empty and suppresses its automatic
replacement loop; ordinary arena launches retain that loop until the menu is used.

Spawn reuses the existing UTC loader/loadout and stance catalog. Native walkmesh,
JKA collision traces and creature spacing validate3-4m ahead; the existing spawn
service faces the actor toward the player. Hostile uses the existing template's
faction. Friendly uses native faction2 (Friendly_1): owned repute.2da gives100
toward the player and mutual0 toward hostile NPCs. Player dynamic faction sharing
was tested and rejected because it produced neutral50 reputation.

The sandbox roster records original commandable/passive flags and per-actor Force
cooldowns. NPC target selection uses native hostile reputation, allowing the same
JKA controller, navigation, pose, contact, guard/parry and KotOR damage/Force paths
to target another NPC. Movement ownership includes friendly controlled actors;
unregistered actors retain the original player-target admission. Parry rolls are
cached by attacker ID plus swing serial. No stance assets or tuning are changed.

AI Off releases source rigs/controllers, clears native actions/combat, restores
native idle and disables commandability/passive attack initiation for roster
members. Threat detection ignores those members. AI On restores their saved
flags. The native area-unload hook clears roster IDs and Force state before IDs
can be reused; returning to the arena permits fresh spawns. These sandbox controls
currently apply to arena actors, not campaign-wide NPC editing or party membership.

## Separate Dark Jedi fighting types

The local K1NpcType catalog assigns stable IDs `dark-jedi`, `dark-jedi-staff` and `dark-jedi-dual`,
with distinct UTC/tag/Engage-body resources. The staff entry uses k1darkstaff,
k1arena_darkstaff and k1jadstaff; native appearance remains n_darkjedim. Identity
comes from each actor's template tag, independently of the player's stance or a
global body-name replacement. The arena spawn selection preserves that identity
on replacement. The sandbox menu selects these entries as Dark Jedi stances.

The staff type selects its own source hit windows,92% playback, chain4 and staff
poise policy through existing NPC controller/pose/contact services. Yellow stays
82%/3 and retains its assets and calibration. Staff attacks skip yellow-only cut
calibration, use authored hand releases and staff grip transitions. Native Dark
Jedi g3r1 supplies the side-held guard; moving guard uses its upper pose over the
existing walk. Idle/native Force/status/death ownership remains with the existing
services. Both actual staff blades are swept independently with serial/blade
consumption, one native health hit per swing and13 guarded poise per blade.
Rejected staff NPC contacts release only unaccepted consumption for later retry.
Staff poise policy remains identical to the player's95%-speed staff; only NPC
speed differs. Equip the owned g_w_drkjdisbr002 template (native baseitem9,
modelvariation2/w_dblsbr_002) rather than the player blue template.
Per-type parry defaults are yellow25% and staff20%; native per-creature integer
overrides still win, including explicit zero. Yellow samples85% continuation once
per outgoing swing serial and rests250ms after a chain. Staff retains100% eligible
continuation and150ms rest, without adding a random roll to its decision stream.
Other actors keep the prior defaults; JKA aggression/movement values are unchanged.

NPC source/native body replacement previously attached the native bind pose
before its cast animation evaluated. Capture the outgoing bind-relative model
rotations and seed/blend the replacement through the existing160ms utility
transition service. Each NpcPlayback owns its context; the player retains its
original default context. Clear the context before replacing its body, restore
raw rotations before animation, and apply the blend after native/source pose
evaluation. Context lifetime follows NPC ownership. The native handoff probe
requires common seeded joints within0.003rad (16 observed), for all4 casts in a two-life run.

prepare-k1-dark-staff.py reuses the staff solver with the native Dark Jedi bind
skeleton, exports a separate body, applies the reviewed Dark Jedi torso weights
with preserved neck boundaries, and creates the separate fixture UTC. Original
mesh/material/face data and44 unaffected clips are checked through round-trip;
all ten completed body files are hash-protected. It does not close the player's
backpack opening on the Dark Jedi. Owned inputs/outputs remain ignored.
The installed Dark Jedi probe samples7 attacks/5 blocks at5 times, requiring
nonempty finite poses and measured grip, independently places both real beams
on native target triangles, and verifies one accepted native HP hit,26 guard
poise and30 parried loss. Sampling explicitly removes owned runs and invalidates
only the probe actor's last animation tick to exercise the real pose path outside
scene animation. Two real Force casts per life, restored melee, replacement and
native victory/death restoration remain distinct integration checks. These
controlled checks do not establish visual acceptance or full campaign support.

## Melee contact after native utilities

Staff blade consumption follows accepted damage or guard contact. A rejected
primary hit releases its blade bit for a later sample in the same swing; it does
not turn a rejected native round into damage or permit a second accepted health
hit. A secondary unguarded contact also remains retryable until a primary hit
has actually succeeded. Accepted per-blade guard contacts remain consumed.

On player native-to-Engage ownership handoff, once NativeUtilityQueued is false,
an empty leftover spell round completes through EndCombatRound(0). Actual native
actions, one-shot animations and queued round actions remain protected. This
aligns restored saber control with native damage readiness without changing
target immunity, Force effects, weapon stats or native damage calculation.

## PC software cursor

With hardware mouse rendering disabled, K1's PC GUI draws the original owned PC
cursor bitmaps and hotspots through Aurora. The platform cursor decoder retains
unscaled RGBA pixels from the same DIB decode used by SDL's hardware cursor.
The GUI requests reserved pc_cursor_NN textures; Aurora supplies their headers
and raster data through its existing upload, release and reload lifecycle. No
generated texture files or separate rendering pass are required. Nearest integer
scaling matches the hardware cursor policy; the existing mouse-look visibility
gate remains. Missing original art keeps the previous gui_mp software fallback.
The opt-in CursorProbe compares all92 actual cursor draws against decoded source
pixels at three sizes and checks transparent background/hotspot/visibility behavior.

## Dismemberment (sandbox or explicit local opt-in)

Local policy revision (2026-11-23): a qualifying fatal powered-lightsaber region
hit is guaranteed to detach its selected part; the historical resource-table
probabilities are no longer rolled. Existing native fatality, saber-only,
region/direction, resource and Tusken no-leg gates remain. The original22 native
damage integers are preserved; private marker/row/serial plus normalized horizontal
blade motion are copied as additional integers through the existing native effect
queue. Old diagnostic25-integer payloads remain supported. Detached parts travel
1.25–1.75 metres along the contact sweep, without the old random outward yaw;
vertical/degenerate sweeps use attack direction/facing as a stable fallback.
Native projectile acceleration, final-pose freezing, floor alignment and cleanup
remain in use. Detached Gob objects and their cosmetic head/weapon attachments
have shadows disabled via TurnOffShadows; retained corpse shadow settings remain
unchanged. This avoids full-skeleton shadow proxies without editing shared models
or changing the engine's global shadow behavior.

Native NPC finger ownership follows the complete bicep/forearm/hand bone ancestry,
including KotOR's RaFngr/LaFngr names. Wrist candidate selection rejects planes
leaving wholly finger-weighted triangles on the retained body; the detached model
must preserve those triangles. This matters for the Aqualish curled left fingers.
Both sides retain native skin weights and animation tracks. Body appearance updates
can replace the retained wound materials with clothing textures, so corpse update
and render preparation restore dismember_cap/dismember_glow to jkcap/jkcapgl.
Fatal-case probes verify retained as well as detached cap materials. The explicit
Tusken no-leg policy remains in both generation and runtime table/contact selection.

The original24 cut pairs are retained. The unnamed NPC catalog adds310 cut pairs
for39 native body/armor models. Tusken right/left legs are deliberately excluded;
their arms, hands, waist and head remain eligible. Both table loading and contact
selection enforce this policy, including stale rows. All source stats, feats and
abilities remain native. Ordinary fatal powered-lightsaber contact and the existing
probability/direction policy are retained.

Local cut-pair preparation preserves native skeletons/supermodels for KotOR death
playback. Intersections interpolate original UV/normal/skin attributes; cap rims
reuse intersection weights. Player head is a separate attachment model. Place its
neck plane beyond the full native attachment rim, rejecting non-neck influences
and excessive clearance. The soldier waist uses the native torso joint above its
belt opening. All cut pairs and original JKA caps/glow textures live only in ignored
generated/profile output. Offline interpolation can change the native
posed surface by 17.81mm; connected cap checks do not establish visual acceptance.

The NPC generator reuses the cut interpolation/cap services and validated native
palette partitioner. Source surface area, serialized inverse binds, original joints,
supermodels and native animation node/channel/key values are checked. It preserves
native tangent data. Open garment contours receive bounded local cap closures,
recorded in ignored diagnostics. This is custom geometry; visual acceptance is not
established by geometric checks. The reader's editor-only animated flag must be set
when exporting original animation trees; packed overflow quaternions follow K12's
native normalization. No completed stance assets or tuning are edited.

A separate k1npccuts.2da references native bodies and their bone-local cut resources.
Runtime maps each of the five NPC style rigs back to its native body. Differently
named or whole-body meshes use their native skin palette to identify the contacted
anatomical surface, followed by local bone regions. Separate cosmetic heads remain
whole attachments to the detached neck/waist; both wielded weapons follow their
respective detached hand/arm or upper body. The existing native fatal-effect,
appearance rebuild, death playback, frozen pose, flight, revival and area cleanup
services own this work. The sandbox launcher validates and copies the assets under
its existing profile lock; generation and deployment must run sequentially.

The local adapter retains the doubled-threshold probability gate from pinned
OpenJK. User tuning sets all supported actor/part thresholds to45 (nominal90%,
approximately90.9% with the retained floored roll). Local killing-contact regions replace its cap-distance and
blade/motion-angle eligibility, as requested by the user.
The host reads bone-local cap anchors/resources and counterpart thresholds through
the native 2DA loader. Real posed-skin triangle witnesses supply the contact.
Native equipped lightsaber type and powered blade are required; coplanar or
otherwise unlocated contact deals ordinary damage without inventing a cut.
Earlier player/stormtrooper/reborn threshold tuning is superseded by this setting.
Debug/realistic overrides and accumulated-location-damage modes are excluded.

Only the accepted melee contact can append a cut marker/row to its exact native
EFFECT_DAMAGE. Preserve all 22 original integers when expanding the payload;
native CopyEffect owns the queued copy. Guard, parry and simultaneous-hit
cancellation happen before damage. Only the actual native fatal gate records the
cut, preventing a later Force/grenade death from inheriting an old saber witness.
Normal feature-off launches load no cut table and own no detached objects.

Active victory retains the player's current saber overlay until its loaded
animation reaches its end. Clear queued/held chaining and suppress early guard
recovery cancellation while all known enemies are dead. Then use the existing
Disengage path to release source ownership and restore the native controller,
body, camera and pose. Non-victory chaining/guard and native utilities keep their
existing rules. Native status/menu interruptions retain their existing priority.
The real-kill diagnostic measures the remaining clip time at death, checks the
handoff deadline and native pose restoration across21 kills. The controller
diagnostic also waits on the loaded clip before its victory restoration checks.

The model diagnostic can export native Dark Jedi bind bones, posed triangles and
per-triangle-centre CutRegion classifications to the ignored profile. The local
region guide uses these measured data, with actual formula radii and selectable
front/side views. Surface colouring is a sampled guide; outer scan envelopes do
not remove actual mesh, anatomical or swing-direction gates. Animated in-game
bones move the regions; the static bind guide is not a live collision capture.

The native creature loader selects retained resources, while KotOR retains the
real death animation. A native-stunned victim already on its ordinary body needs
an explicit appearance rebuild and scene reattachment. After completed native
scene rendering in a death clip, freeze the detached model at the last valid
sample of that same native clip. Bake attachment ancestry into its world root,
rebase its local root to identity, and align the final-pose cap with the actual
sever point. The retained corpse continues its ordinary native death playback.
Original body/attachment material resource names are applied with native per-part
texture references; original JKA cap/glow parts are exempt. Additive glow uses
native TXI/material support, not a new renderer. Visible detached head/hilt
attachments determine floor clearance. Native per-part draw flags hide carried
originals (Gob EnableRendering is K2-only). The severed hilt's blade is disabled.

Detached parts reuse the arithmetic factored from MoveProjectileAccelerating,
without becoming gameplay projectiles or dealing damage. Walkmesh landing leaves
15mm clearance beneath visible assembly bounds. Each cut chooses an outward
heading within about66 degrees of the cut's radial direction, a0.25–0.45m throw,
and up to15 degrees of ground-plane turn around the cap pivot. Apply that turn
in world space to preserve the final native pose's inclination; no random-axis
tumble is added. Cosmetic randomness uses a
separate sequence from gameplay rolls; there is no ragdoll/bounce simulation.
A client owner/area identity map retains parts with native persistent
corpses. Area unload and identity changes release children before their limb.
Revival removes the record/parts, then rebuilds and reattaches the intact native
body. Existing death/retained-corpse services remain authoritative.

Native diagnostics separate 48-model/1,440 death-pose components, real posed-skin
selection with synthetic blade motion, and explicitly selected real native lethal
damage/ownership tests. They verify all enemy locations, materials, stun/revival,
nonlethal/unmarked rejection, persistence, landing and actual reload cleanup.
Player temporary death, saved severed corpses and visual acceptance are not yet
established. Current results, build identities and next action are in STATUS.md.

The accepted CCD surface witness on the killing hit is authoritative. Hand
eligibility requires an arm surface and a short wrist/distal-hand capsule
(10cm proximal allowance, radius75% of forearm length capped at20cm);
shoulder/chest hits cannot select a remote hand. Arm capsules follow the native
shoulder/elbow/wrist, with radius100% of the longer arm segment capped at33cm.
The native torso mesh includes proximal arm and leg skin. Its shoulder pocket
uses a27cm sphere with12cm proximal/inward allowances, measured along the upper
arm and the opposite-shoulder lateral axis. This includes deltoids/outer pecs;
the central sternum cannot become a remote hand/arm. Leg cuts require a local knee-to-ankle capsule
on the actual torso surface, with25cm radius. When capsules overlap, choose the
closer leg only within that bounded radius. Torso cuts use a pelvis-centred
capsule, from8cm below to8cm above pelvis along the native spine axis. Retain
the radius (180% of spine segment length capped at40cm) but bound axial reach
to18cm below/above pelvis, adding pelvic coverage and trimming upper chest.
Head retains actual head-surface
bounds with4cm allowance. No later sweep or nearest-part fallback is introduced.
These widened local regions are intentional user tuning; actual hit witnesses
and swing restrictions remain required. Torso and head require their own actual surfaces and local
bounds. Missing/ambiguous regions fail closed, without a nearest-part fallback.
Queue marker,row,swing serial while preserving22 original damage integers. Roll
once at the actual native fatal gate, only for a valid local region; a per-victim
record prevents rerolls until revival/identity retirement/unload. Nonlethal hits
and later corpse sweeps cannot grant cuts. Immediate health timing is unchanged.
Earlier angle/late-sweep approaches are superseded; failures remain in ignored
runtime evidence. Native unload also guards missing server state.

User policy (2026-11-09): arm cuts require non-horizontal swing indices; hands,
waist and head require either horizontal swing. Killing-contact selection uses
this filter and the actual current swing serial. Source
gate remains; all supported parts, including head/waist/legs, now have nominal90%
by explicit user tuning.
Rigid eye/eyelid TRIMESH descendants of head_g are partitioned with the head,
while the full guide skeleton is retained for native death animation. The cutter
rejects unclassified visible rigid attachments rather than copying them into
unrelated severed parts.
Player right-click guard may interrupt recovery once the active damage window
ends, using the existing pose transition. Native utilities retain priority. A
poise break resets the attacker's chain count to zero, preserving the outgoing
attack serial/accepted contacts and allowing three subsequent chained swings.

Current chance tuning (2026-11-09) supersedes the earlier enemy arm increase:
all24 table rows use threshold45. These are chances conditional on a valid local
killing contact and permitted direction, not a percentage of all kills.

## Native rigid backpack binding

The PMBBL backpack and rear hardware are detached native skin components bound
rigidly to torsoUpr_g. The adapted height-based spine field had assigned their
lower vertices to pelvis/lower spine, stretching an accessory that the native
asset keeps rigid. Restore those native weights only in the isolated Engage rig;
keep geometry, bind frames, other weights and animation tracks unchanged. No
pelvis-height adjustment is required. Native skin palettes/influence limits remain.

Preserve the current MDL/MDX pair when appending clips. Older generator output
predates this correction; after regeneration, identify the native rigid components
and reapply the weight postprocess before installation. The ignored backpack-skin
recipe/mapping applies to the recorded current topology, not arbitrary new bodies.
The ordinary native body is unchanged. Broader timeline grip/angular failures
remain identical before/after; this fixes backpack shape rather than claiming
whole-character deformation-free playback.

The PMBBL torso also has a 16-vertex open back rim normally hidden by that rigid
accessory. Stance motion can expose it. The existing staff closure triangulates
that rim with 14 faces using its original vertices, UVs and skin weights. Apply
the same closure to Yellow/Red/Blue/Purple; duals and staff already contain it.
Equal skin weights on welded rim copies keep the cap attached under every pose.
The offline fill-k1-back-panels.py postprocess preserves all original faces and
animation tracks, verifies binary roundtrip and can install verified components
with updated identity hashes. Staff regeneration accepts an already-closed seed.
Native Dark Jedi robes lack this opening and need no backpack-specific patch.

## JKA saber surface marks

The isolated launcher reads owned JKA base/assets1.pk3, extracts rivetmark.tga
and saberdamageglow.jpg into ignored profile/jka-marks, and records hashes.
The JPEG is decoded to bottom-up TGA for the existing image reader; no artwork
is authored or replaced with KotOR textures. No owned assets enter tracked files.

K1JkaSaberMarks adapts pinned OpenJK260c59c2907187af555a676fe0cc798893bf7757
CG_CreateSaberMarks, R_ChopPolyBehindPlane/R_AddMarkFragments and CG_AddMarks.
Source mark width, UV/color distributions, scorch blend and additive orange
glow/fade are retained with the existing calibrated JKA-to-K12 unit conversion.
K12 triangle winding is used at the host boundary. Glow lasts1.5 world seconds
(full for0.5, fading for1); scorch lasts10. Pool bound256 paired fragments and
same-receiver/weapon continuity with miss/teleport resets are local host policies.
Visual randomness has a separate local generator and cannot consume native dice.

The native equipped item must be a lightsaber (base types8/9/10), and its blade
must be powered; a luminous mesh alone cannot authorize marks or contact audio.
The full main-hand blade is queried independently of damage windows,
for the player and owned combat NPCs. Native Scene::HitCheck chooses the surface.
Static rooms can lack Gobs: use their nodedata render meshes, whose visibility
does not use the character-tree draw flag. Static marks remain in world space.
Props use actual posed triangles and store points in object-local coordinates.
PartTriMesh's precise native hit intentionally clears the normal; recover it
only from the reported hit part's face containing the actual hit point, never
from a box or a made-up direction. Safe object/scene ownership drops stale marks.
This adds surface presentation only, without damage, reach or physical cutting.

Render after the native saber bucket and before framebuffer post-processing,
sharing the world's depth and camera. Scope shader/texture bindings and Aurora's
binding cache; floor readback requires changed pixels inside projected bounds
and zero outside. K1's attribute push/pop imports are no-ops: explicitly restore
lighting/cull/depth/offset, the shader-shadowed alpha test, texture environment,
all touched texture units and the saved program/cache; verify those states too.
Smoke also covers actual animated-blade hits, static floor/
wall projection, a transient installed-table prop's rigid attachment/deletion,
miss resets and glow/scorch expiry. The prop exists only in the hidden client
fixture; it creates no server/story/save object. Human appearance remains a
separate acceptance check.

Surface contact audio uses original JKA saberhitwall1.mp3 from owned
base/assets0.pk3. The short jkscorch1.wav resource alias preserves the bytes;
the existing native WAV/miniaudio path already sniffs MP3. Extraction/provenance
remain ignored. Native spatial one-shots originate at actual scenery contacts,
at per-voice volume20/127, retaining the user's SFX slider. Quiet volume is local
policy. JKA's100ms minimum debounce is retained; wait at least the clip's native
duration as well, since these one-shots do not replace JKA's CHAN_ITEM voice.
No new cues are emitted during a miss, with swords, or with an unpowered saber.
Stationary valid contact may cue again when ready; no movement/attack is required.
Existing scars finish their original lifetime after an equipment change.
The smoke fixture covers the original clip on installed scenery contacts, validates native
resource/result/volume and non-overlap, and adversarially retains a powered saber
mesh while native item types are temporarily swords. Item/power state is restored
within that render call; there is no persisted inventory or save edit.
Labeled synthetic floor rays ensure audio coverage when the battle has few
natural contacts; they share the real actor/world cooldown. Live blade/miss
coverage stays separately required, and normal play injects no rays or items.
Use one cue deliberately: diagnostics found that the current native sound cache
keys these loose files by the same resource ID and can reuse another active
variant. No general cache/key change is made here. Payload name/size and a native
cache miss are required by the fixture; original byte hashes remain in ignored
provenance. Earlier three-clip experiments and the collision trace are retained.

## Melee poise prototype

Local clash policy (2026-11-23): every accepted saber deflection spends10 poise
on each actor, regardless of stance, through the existing native break/stun path.
Duplicate contacts retain the resolver's existing suppression. Each actor keeps
the same opponent and consecutive clash count in its existing poise state.
The immediate subsequent successful parry consumes the mutual episode and adds
5*(count-1) to the existing attacker-stance parry penalty. The defender's existing
parry reward is unchanged. A single clash grants no bonus; five grant20.
Damage, ordinary blocks, native utilities/status, opponent/equipment changes,
planning and an intervening missed swing clear both sides. A continued attack
chain after a miss also clears the episode at the next swing boundary.

After a clash, an NPC makes one next-action decision at5*count percent, capped100.
A successful decision selects its existing timed guard instead of an attack;
an eligible contact then parries without a second ordinary percentage roll.
Failed decisions keep the swing path and are not rerolled on every update.
Ordinary NPC parries outside episodes retain their native local/fixture setting.
The guard waits at most two world seconds so two NPCs choosing guard cannot wait
forever; an incoming miss or any other episode interruption also cancels it.
Stance animation assets and underlying KotOR weapon/character data are unchanged.
This supersedes the historical Red-only clash contribution described below.

### Weapon audio

Engage's imported motion clips have no native SwingShort/HitClash/HitParry
animation events, and custom mesh contacts bypass the usual client attack-event
callbacks. Route source-owned saber cues into the existing native services:
CSWCCreature::PlaySwingSound uses weaponsounds.2da SwingShort0..2, Clash0..1 or
Parry0..1; PlayHitSound uses the native appearance/armor/shield material selection.
These swing/guard/parry/body-hit cues copy no audio assets; surface contact uses
the owned JKA clips described above. Native spatial playback, SFX volume, priority,
pitch and resource loading are reused. The hit adapter converts server IDs to
client IDs just as native client message/object paths do.

A serial gate emits one swing cue as its clip enters the cutting window,
including a whiff. Gait/layer switches and repeated scene samples cannot restart
that cue. Actual ordinary guards play Clash, successful parries play Parry,
reciprocal simultaneous-hit resolution plays one Clash, and accepted open-body
damage plays the material hit cue. Held possible trades wait for their existing
damage/clash resolution; they do not play a premature body-hit cue. Predicate
queries, misses and canceled contacts have no impact cue. Native/unowned actors,
utility actions and ranged shots retain their existing sound paths.

Smoke tests enable K1_ENGAGE_SOUND_TEST. Source cue scopes are paired with the
native 3D one-shot resource/result trace; the launcher requires exactly one
positive native playback-service result per cue, plus coverage for the relevant
actual contact fixture. The swing timing/once-per-serial gate is tested separately.
This verifies resource/service integration, not human audibility or mix quality.

### Contact block reactions

Actual blocked contacts and successful parries enqueue a defender reaction on the
existing per-actor poise state. The incoming Engage attack selects the animation,
independently of the player's selected guard quadrant: overhead -> top; the two
upper diagonals and horizontal cuts -> the corresponding mirrored upper block;
rising cuts -> the corresponding existing lower incoming quadrant. Guard/parry
costs and coverage stay unchanged. Contacts lacking an owned Engage attack
direction retain their existing native/generic guard path.

The five authored source clips are BOTH_P1_S1_T_/TR/TL/BL/BR. Pinned OpenJK
code/game/bg_panimate.cpp saberMoveData maps LS_PARRY_LR to BL and LS_PARRY_LL
to BR; lower animation suffixes are not literal defender sides. New j_block_*/
y_block_* aliases use those verified move meanings for both rigs. The player
clips use the existing owned-data retarget solver; NPC aliases reuse the existing
retargeted parry clips with that lower-side mapping. Append checks require all
existing geometry and animations unchanged. Assets and tooling stay ignored.

Playback uses an upper-body overlay over the current gait or guard, the full
source clip for the existing pelvis/spine bridge, and shared grip/pose smoothing.
The source clips contain two frames at20fps. A local250ms reaction plays their
100ms range and holds the final pose for the remainder, then blends back; this
presentation hold is not claimed as a JKA default. World-clock sampling preserves
slow planning. Repeated contacts restart even the same reaction through a serial.
An attack cancels the defender's reaction without adding a new attack lockout.
Planning clears pending reactions; native actions/status ownership take priority,
and a guard-breaking hit uses native stun instead. Teardown clears actor state.

The installed parry fixture observes actual evaluated reaction overlays, all five
player sides, ordinary blocks versus perfect parries, a walking contact reaction,
NPC timed counter-parry, grip/finite poses, reaction expiry and return to guard.
These checks do not establish full connected-skin or visual acceptance of the
new poses; those remain distinct from source/clip/lifecycle correctness.

Player directional parries extend the existing actual guard-contact service.
An active melee guard can parry only its selected visible hostile melee enemy,
within that enemy's calculated reach plus the player's personal space and0.75m, in front
and through the existing clear-shot service. Native utility/status ownership,
broken guard, ranged equipment and planning exclude the selection. A stable
bind-height chest anchor projects into the current camera; the crosshair's
offset from that anchor chooses one of four quadrants, with an8px
dead zone at720p and a45%-of-screen-height targeting radius. The anchor does not
follow live torso bobbing. Closest screen aim selects the enemy.
The extra0.75m is marker visibility; parries still require the existing actual
posed-body melee contact. Weapon geometry, movement and hit reach are unchanged.

Coverage follows the user's four-section policy; overlaps are explicit masks.
Rising cuts use their authored travel direction for the requested lower pairings:

| Defender guard | Covered incoming cuts |
| --- | --- |
| upper-right | overhead, overhead-left, horizontal-left |
| upper-left | overhead, overhead-right, horizontal-right |
| lower-left | rising up-right (bottom-left to top-right), directly from below |
| lower-right | rising up-left (bottom-right to top-left), directly from below |

Both upper guards are tested live against the overhead cut. Both lower masks
accept the straight-bottom incoming sector in rule tests; the current seven-clip
NPC pool has no separate straight-bottom animation, and this change adds none.
Horizontal axis ties select an upper quadrant; vertical axis ties select the
right quadrant. The neutral dead zone still gives an ordinary block.

Only actual blocked contacts spend poise. On a correct player guard, end the
attacker's swing/chain through the existing interruption service, reset the
defender's quiet timer, restore10 defender poise points capped at100, and spend25
attacker poise. This immediate parry reward is separate from passive regeneration;
its usual running/swinging/recent-contact restrictions and quiet timer remain.
Zero uses the same native stun/25% recovery service. Wrong/neutral guards retain
ordinary defender20 loss; unguarded hits retain native HP damage. An NPC's
existing timed parry (not its ordinary close guard) now applies the same result
to a real incoming player swing. This reuses the existing NPC reaction window;
it adds no directional NPC aiming AI. Attack-vs-attack timed deflections retain their
separate harmless chain-reset rule. No new animation assets.

NPC timed-parry contact additionally requires a percentage roll. The supported
Sith soldier defaults to20. An optional native creature/template VarTable row
with Name=K1_ENGAGE_PARRY_PERCENT, Type=1(integer), Value=0..100 overrides it;
absent retains20, explicit0 disables,100 guarantees an otherwise eligible parry,
and out-of-range values clamp. Existing CSWSScriptVarTable loading/storage is
reused; future type/skill selection can set that creature value without changing
the guard/contact implementation. This does not add a NWScript command or UI.
The usual350ms timed window, reaction delay, cooldown, melee/front/status gates
still apply. At the first eligible blocked contact, roll native1..100 once for
that incoming player attack serial and cache its outcome in per-actor poise
state, independently of body ownership. Repeated queries cannot reroll the same
swing. Failure falls through to an ordinary guard block (default20 points;
staff-player contacts use13 per blade) with its block
animation; success spends attacker25 (30 for the staff player), with defender10
reward and1s interruption.
Tests enumerate all100 rolls at0/20/100, clamps/absence, cache repeated queries,
and use temporary per-creature100/0 overrides for real success/failure contacts.
Those overrides are restored before the isolated fixture exits.

ApplySuccessfulParry records a1-second world-time attack deadline on the parried
actor's existing MeleePoise state, independent of body ownership. Player mouse
requests and playback starts both gate on it; NPC starts gate on it as well as
their ordinary attack schedule. NPC nextAttack is extended to that deadline.
Movement, guard and native utility actions remain available; attack presses
during recovery are discarded. Planning/body swaps do not clear the deadline;
encounter/HUD teardown clears it with the existing actor state. A native poise
stun can keep a fighter disabled longer than this minimum attack lockout.

The HUD appends a mouse-ignoring native control: a center dot during active
Engage, plus four arc sections only for an eligible close target. Selection is
blue without guard and native CSWGuiManager::COLOR_YELLOW with guard (the same
constant K1 buttons use). Dial outer radius is20px at720p, reduced from26px;
the center dot is unchanged. Resolution-scaled geometry uses native
GUI counter-clockwise faces. The shader/texture/cache scope already used by the
saber preview is shared with this untextured draw; lighting/depth are restored.
ParryProbe uses raw look input and real posed contacts for seven matched cuts,
opposite-upper overhead overlap, wrong/neutral/unguarded cases and attacker stun.
Twelve framebuffer checks verify
dot/highlight pixels, no changes outside the dial and restored shader/cache state;
eligibility adapters cover range/rear/ranged/native/planning. Victory hides the
control. Ordinary poise, preview, clash and enemy tests run separately.
The live fixture also forces an NPC's ordinary attack timer ready while its
parry deadline is active, then observes the first permitted restart. A real
NPC timed parry against a player swing tests repeated fresh player presses
during recovery and restart afterward. Both check the1us-before/exact deadline
boundary on the state generated by the actual parry. Range checks include a
point0.5m beyond the old marker limit and one beyond the new limit.

Player and NPC posed blade sweeps collect body contacts before resolving damage
in K1EngageAfterSceneRender. Contacts are accepted only for current live swing
serials. The local basic timed-deflection experiment replaces the earlier spatial
blade-region experiment. No melee contact persists between scene updates; the
former reciprocal body-hit timeout remains removed. Single, dual and staff body
sweeps use their preserved original geometry and subdivision services again.

Each actual player swing start records attackNow in playerAttackStartedAt;
each NPC swing start records now in NpcPlayback::attackStartedAt. Both include
automatic chain links. The clock is world time, respecting hard pause and planning
speed. K1AttackDeflection::Eligible accepts nonnegative starts whose age is
0..500000us inclusive; missing/future/older starts are ineligible. Only the
defender's swing age matters. The defender must still be swinging, and existing
live ownership, melee-poise, broken-poise and native-status exclusions apply.
There is no new facing, matching attack direction, input-edge, blade-position
or one-use eligibility gate. Existing attack controls/AI still decide when an
attack can start; this does not add new directional aiming AI.

An accepted incoming body contact against an eligible defender immediately stops
both chains and buffered follow-ups, consumes the existing contacts and reuses
pose blending, weapon Clash sound and NPC150ms restart cooldown. Held player
attack retains the existing fresh-press restart behavior after cancellation; the
eligibility check itself does not require a fresh press. Each Red participant
deals10 poise points to its opponent once per resolved deflection. Purple's
inherited Red animation does not grant that bonus. Existing native guard-break
effects and regeneration remain, and a deflection causes no HP damage.
Per-update duplicate contacts are deduplicated; there is no additional per-attack
deflection counter. Stopping both swings naturally ends their active eligibility.

Outside the window, body contacts enter native guard/parry/damage in the same
scene update. Native EVENT_APPLY_EFFECT scheduling remains asynchronous. This
adds no hit waiting period and does not promise synchronous death presentation.
Ranged bolts and native powers retain their existing paths. The runtime marker
is K1_MELEE_DEFLECTION eligibility_us=500000 delay_us=0. Diagnostic fixtures
supply actual start timestamps rather than synthetic spatial interception flags.
Component checks and installed-data limitations are recorded in STATUS.md.
The earlier experiment/evidence is preserved in the ignored checkpoint under
platforms/out/timed-deflection and platforms/out/defensive-interception.

Poise is keyed by server actor independently of source body playback, so native
utility/status body swaps and planning do not reset it. An equipped main-hand
melee class starts at100; ordinary blocked melee contacts subtract20. Predicate
queries never spend it. The breaking contact remains blocked; further contacts
find an exposed defender. Native incoming contact is checked before spending,
and the source sweep consumes only once per swing through its existing hit flag.

Break applies native EFFECT_SETSTATE/STUNNED for2.5 world seconds. The effect's
native immunity-bypass parameter is local to this physical guard-break effect;
Force-power immunity rules are unchanged. Native body/animation ownership and
temporary-effect removal handle incapacitation; after expiry/clear state poise
resets to25. Its first eligible recovery tick follows after0.1s without the usual
quiet-period wait. The breaking block clears its own quiet deadline; later hits
during stun retain their normal two-second deadline. Running, new swings and
status still prevent gains, with no banked ticks. Victory/disengage/HUD teardown clear the
encounter state; ranged/unarmed actors have no poise. This does not expand the
prototype's existing body/weapon/controller support.

Default melee actors regenerate1 point per0.1 world seconds, capped at100,
after2 quiet seconds. Actual guard contacts and confirmed body/bolt hits reset
the deadline; accepted native positive damage effects also reset it before
immunity/reduction. Predicate queries and whiffs do not. Each source swing start
records activity and the completed per-frame attack state keeps the deadline
anchored to swing end, including chains/interruption. A scoped end-of-update pass
handles early native-ownership returns without awarding a tick before new swings.

Still/Shift-walking/guarding permit recovery. Player inputs plus ground velocity
gate source movement; native utility run animations also block it. NPC controller
gait/velocity apply the same rules; an NPC with unknown native movement ownership
cannot regenerate. Running/ineligible states discard partial tick progress and
cannot bank catch-up gains. If quiet time already elapsed, resuming eligibility
starts a fresh0.1-second tick; continuously eligible actors first tick at the
two-second deadline. These timers use the native world clock, including slow
planning; hard pause cannot award ticks. Automatic guard-break25% reset
remains separate, and passive ticks are disabled during broken-guard stun/status.

PoiseRegenProbe combines exact synthetic clock boundaries with real guard contact,
native damage and player run/walk/swing input. GPU readback diagnostics run in a
separate PoiseProbe because they measurably delayed the next observer frame; no
gameplay timing thresholds are changed. Contact tests use each hit's actual prior
poise (regen can raise it between trials), preserving the stance-selected loss and immediate
native recovery25 checks. Clean player/NPC recoveries and exposed stun hits check
both immediate and delayed post-stun ticks. Scripted poise fixtures hold NPC attack
starts at controller creation, including native stun restoration, so a fresh real
swing cannot accidentally invalidate an idle-recovery assertion.
Capture-only meter fixtures hold their artificial values
by explicitly postponing their recovery clock; they do not validate gameplay.

The HUD extension owns an independent saber model, never the live weapon. Equipped
sabers use their actual native resource/variant (hilt and crystal color); other
melee weapons use stock blue w_Lghtsbr_001. Native powered playback and blade
billboard vertices supplement authored hilt bounds for horizontal top-center framing.
Only blade draws belonging to this HUD scene use a depletion scissor. Single-blade
presentation retains its existing projected-vertex crop. Double blades project
each powered emitter/tip through the preview camera and keep the segment adjacent
to that emitter, so opposing tips retract toward the hilt together. Native staff
resources also contain ordinary blade planes: these opt in by matching a sibling
lightsaber node's material and nearest world-space blade midpoint. The hilt's
different material is excluded. Each draw restores the preceding scissor; at zero
the affected blade draws are skipped. Hilt meshes always
draw fully, even at zero. Shared nodes, textures and live saber geometry are intact.
The percentage and viewport remain native GUI controls. Resolution-aware layout
keeps the single blade's420x74 base viewport and centers the label below. Staff
width uses81% of the uniform scale matching the established yellow blade's
projected length, following the user's two successive10% reductions.
Measure a temporary powered blue single saber through the native preview loader,
cache its authored/dynamic bounds, beam length and recentered beam depth, then
remove it before drawing. The staff width ratio accounts for beam-depth offsets
as well as world lengths; fitting equal world units alone misses the single's
closer beam position. Viewport width is capped at the screen width. Height,
label position, single/short framing and camera FOV remain unchanged.
All previews use86% horizontal framing; the former whole-staff
fullness adjustment was reverted at the user's request. The staff HUD hilt alone
now draws14% longer and15% thinner around its native bounds center, as a deliberate
appearance adjustment based on the user's screenshots. PartTriMesh scopes this
matrix push/pop within its own draw; restore occurs before native PartDraw visits
the blade children, so they do not inherit the hilt's length/thickness scale.
Each HUD blade and matching ordinary blade plane receives a translation from its
native powered emitter to that emitter transformed by the centered hilt scale.
Convert the world-space delta into each plane's local frame for a scoped draw
translation; project the same moved emitter/tip for depletion. Beam length and
thickness stay native. Opt-in checks compare actual draw projection against the
moved crop endpoints. Framing and shared/native gameplay transforms are unchanged.
Single/yellow and live weapon draws do not opt in. Shared model data is untouched.
Opt-in hilt diagnostics check pre-adjustment native modelview scales and projection
pixel aspect. The meter fixture also compares the equipped and preview hilt's exact
native node identity and transformed long-axis length. Identify hilts by their
lightsaber-node children rather than texture suffix, since crystal variants retain
base hilt texture names. Shared model geometry and live weapons remain untouched.
Horizontal aspect sets
camera distance. Cached native font scale reflows on size changes and the label
box covers its measured ideal height (the old fixed70x25 box clipped fullscreen
text). Meter drawing masks/restores native combat-message labels and backdrop,
without clearing their messages/timers. Original functions remain available.
The K1 PC roomless render path enables blending around lightsaber model draws,
then restores its previous enable state. The world uses DoLightSaberBucket to
establish that prerequisite; without it the preview draws the glow texture's dark
rectangle. Native blade blend factors, geometry, textures and world pass stay intact.
The preview also scopes fixed-function texture-unit bindings/enables, active unit
and color, together with Aurora's stage/binding cache: native scene attribute
restoration alone leaves an empty unit enabled, and GL-only restoration leaves
the cache pointing at the preview. PoiseProbe compares state and framebuffer
pixels outside the meter to detect leakage; diagnostics stay in ignored output.
These initial checks missed later GUI corruption. The Windows attribute push/pop
is a no-op, so the preview also restores blend enable/factors, depth-write mask
and both host shader owner and actual program through the existing shader service.
It does not change the global attribute stack. The capture-only PoiseRenderProbe
alternates six preview epochs on a stationary isolated scene without changing
real poise; completed on/off frames must match outside the weapon viewport and in
the percentage label. The ordinary four-cycle PoiseProbe runs separately with its
original assertions. Exact comparisons and the user's earlier failed screenshot
are preserved; current visual acceptance remains a user check.
The empty poise update returns before querying a not-yet-created server timer,
fixing the preserved initial startup crash. Four native break/recovery cycles and
broader installed regressions pass; exact identity and visual limits: STATUS.md.

## Direct OpenJK source port — current navigation boundary

The user's direct-port instruction supersedes the earlier behavior-only choice.
Pinned source: JACoders/OpenJK `260c59c2907187af555a676fe0cc798893bf7757`.
Original GPL notices and license are retained in `third_party/openjk/` and the
ported translation units; K12's existing license/credits are preserved.

`src/port/k1jkanav.cpp` compiles Raven's unmodified `ragl::graph_vs` A* and CVec
math, with source-ported STEER Seek/Stop/AvoidCollisions/DeActivate and grounded
PM_Friction/PM_Accelerate/PM_ClipVelocity/PM_SlideMove. Explicit world, clock,
random, actor and collision bindings replace JKA globals. KotOR walkable triangle
centres/edge midpoints supply graph positions; trace-clear edges replace JKA's
map-authored .nav input. Edge samples are inset toward their owning face centroid;
exact outer-edge midpoints can select a Nonwalk face with no adjacency data.
The adapter validates the selected origin face before invoking the native walker.
Graph capacities are adapted to this map. Actor steering
runs at100ms and NPC ground physics at50ms, matching NPC.cpp; display positions
interpolate between physics samples. This is a grounded subset, not the complete
JKA navigator/pmove: regions, danger costs, path ETA, doors, steps, water/vehicles,
airborne actions and arbitrary maps have not been established. Jump/crouch remain
excluded by the user. Exhausted/missing routes fail and hold without KotOR AI fallback.

`JkaWorld` binds static map/door collision to the explicit K12 area's direct-line primitive, saving
and restoring its path context. It finds the earliest live actor-cylinder contact
instead of reusing K12's first-in-X-order actor trace. Source slide response keeps
tangential velocity at contacts. Native AI movement actions are suppressed for the
supported Engage soldier, including initial model acquisition. K12 continues to
own actor lifetime, rendering, animation resources, effects and weapon damage.

The successful client area-geometry load path prepares the graph before play.
It uses the native player's already available footprint with an explicit loaded
server area; player area/control assignment is not required for geometric queries.
Graph construction is absent from combat updates. Per-area cached graphs survive
combat/utility ownership changes and are invalidated by CSWCArea::UnloadArea.
A graph cleared for a wider/taller cylinder may serve a smaller actor; larger
actors or missing/stale graphs fail explicitly without runtime rebuilding or
native AI fallback. Normal non-prototype loading is gated out before server-area
attachment. Installed checks cover original boundary rejection, prepare-before-play
ordering, reuse, actual save reload/arena transfer and fresh cache acquisition.

`src/port/k1jkaai.cpp` ports the ordinary medium-cultist profile's applicable
aggression, roam/noStrafe timers, side-selection and advance/hold/retreat source
branches. The adapter supplies source-unit predicted saber-tip distance and map
checks. Jedi_Move's reverse commands and timer strafes feed the source ground
mover. This is an explicitly incomplete AI source port: attack selection,
saber-event/evasion decisions, Force/special moves, gunner/squad/search branches
still need source integration. Existing gunner decisions and frontal guard/hit
reactions are prototype policies. No full JKA AI or animation parity claim.

Backpedal selection also uses bg_pmove.cpp's command rule: negative forward
sets the backward gait, forward or pure lateral input clears it, and zero input
retains it while decelerating. This prevents a pure source strafe's tiny physical
heading variations from repeatedly swapping forward/back animation clips.
NPC_utils.cpp's ordinary NPC_UpdateAngles yaw branch runs at50ms, with displayed
orientation interpolated between samples. Profile yawSpeed60/70 is a parameter
in `(60 + yawSpeed*3)*0.05`, not a direct60/70deg/s turning rate. The host float
orientation API replaces final JKA usercmd angle packing. Vehicles, emplaced
guns, Force Speed and non-trooper gunner overrides remain outside this profile.

The isolated navigation fixture exercises source pursuit at aggression6; normal
play retains the owned medium profile's initial aggression3. Source timer/decision
boundaries are tested separately. The old bespoke melee_evade_swing assertion is
superseded by source-command assertions, not evidence of source evasion parity.
Native utility playback remains an integration boundary; out-of-range utility
movement and in-flight projectile concurrency are not yet established.

The user's melee-walking policy overrides movement from the first swing, retaining
walk speed/gait through close cooldowns. After a swing ends beyond reach plus the
NPC's personal-space radius and an extra0.75m, normal source pursuit can run again.
The added distance retains walking when the player slowly backs away; it does not
increase attack reach. Existing steering
and ground velocity are capped on entry; the native force-walk flag and animation
use the same decision. Attack cadence remains independent.
Supported melee NPCs also raise guard inside this same close-range radius before
their first swing. Between attacks they use j_guard while still and y_guard over
their walking gait; close guard forces walking and uses the existing frontal
block. They lower guard for their own attacks, and leave it outside the radius or
during native utility/status ownership. This adds no directional guard/poise rules.
Player movement reads native `GetWalkRate/GetRunRate`, converting their mm/s to
metres/s. The user chose the player's native base rates for both sides (loaded
arena player:walk3.2/run5.4m/s); this intentionally overrides the soldier's native
walk1.7m/s. NPCs use the player's native base stats and their own KotOR movement
factor, so player buffs do not transfer to enemies. This covers supported melee
and ranged NPCs. Native ownership/exploration movement remains untouched.
JKA backward/swing speed penalties are removed. JKA acceleration, collision and
controls remain, with a host cap before displacement for both walk and run;
turning acceleration must not exceed the native-selected speed. The default
source ground mover is unchanged when no host cap is passed. Navigation,
avoidance and tactical decisions can still slow or stop an NPC.
This is an intentional customization rather than an original JKA rule.

## Soldier animation parity and persistent combat goals — prior behavior milestone

The isolated Sith-soldier driver now uses its own native pause pose through the
same bind-aware idle transfer as the player. Forward/back source gaits and the
player's bounded leg controller replace the unused source strafe clips. A short
world-clock hold bridges empty position samples. Attack-layer changes preserve
the active gait clock. Cut calibration receives walking/back-left state; guard
uses the player's reviewed `j_guard`/`y_guard` pose. Each body retains independent
transition/grip history and native idle references, cleared before appearance
replacement. Source assets and the original soldier model remain unchanged.

Owned physical-action markers yield before native duel spacing. The per-NPC
combat driver maintains its movement goal independently of attack clips, using
grounded JKA Jedi/stormtrooper decisions for pressure, retreat, timed strafes,
evasion, target prediction, firing lanes and a last-seen goal. It retains a route
until the target goal materially changes. Only its own action group is cancelled
on guard/reaction/utility/ownership handoff. K12's walkmesh, path executor and
collision were the map-navigation services at that superseded checkpoint; no
OpenJK implementation had yet been imported. Active movement goals also prevent
Disengage while pursuit continues.

Medium-cultist/stormtrooper yaw rates and Jedi timer ranges are source-derived.
Think/replan intervals, goal hysteresis, last-seen lifetime, geometry-scaled combat
spacing and prototype frontal guard remain deliberate local policies. Existing
KotOR weapon stats, damage and native utility actions remain authoritative.
The behavior port does not yet cover full JKA chains, special moves/Force choices,
other body types, companions or campaign activation. Live grip/idle/controller
checks and route tests are distinct from full connected-skin and visual acceptance.

## Full humanoid NPC port — in progress

Owned melee enemies may chain at the player's hit-window end plus follow-through,
up to their three-swing cap, independently of the staff player's four-swing cap.
Each continuation rechecks range, visibility,
facing, clear contact lane, commandability and native utility/status ownership.
It resets per-swing contact/blade state and uses the existing pending pose blend,
including when two successive random directions match. The third swing completes
before the existing150ms enemy attack cooldown; ranged fire cadence is unchanged.
The enemy arena probe requires a completed capped chain before its contact and
movement/guard/lifecycle checks proceed.

The user's local profile uses player Yellow/Red1.0, Blue1.15, Purple1.30, staff1.10
and duals0.85; NPC Yellow/staff0.85, Red/Blue1.0 and duals0.75.
Attack/contact/chain windows stay in clip time, native utility
playback and ranged fire cadence retain their existing rates. Melee NPC direction
weights are4 overhead,3 each upper diagonal,3 each side,1 each rising cut.
The existing native18-sided choice maps to those weights: upper cuts total55.6%
(previously62.5%), horizontals33.3%, rising cuts11.1%. The full18-choice mapping is tested exactly;
these are intentional local preferences, not claimed OpenJK defaults.

Spinning links use their complete authored body pose instead of an upper-body
layer over locomotion. The preserved JKA SP PM_SpinningSaberAnim classification
selects16 Blue pairs and nine Red pairs. Red/Purple players and Red NPCs now share
the existing Blue handling: suppress native gait replacement and live spine aim
correction during the spin, retaining authored pelvis/leg rotation. Physical
movement continues through the normal controller; normal moving links still use
upper layers. Classifiers do not alter animation tracks or contact windows.

The authorized target is source-ported grounded JKA humanoid combat behavior and
animation, with KotOR stats, damage, surrounding systems and utility actions.
Jump/crouch are excluded. The earlier behavior-only choice has been superseded;
the current imported-source boundary and remaining work are described above.

`PosePlayback` owns transition/grip history per body. Player aliases preserve
the existing input path; NPC callers pass independent state to the same pose
solver. Native model replacement now resolves an actor-specific rig resource.
The first generated NPC candidate preserves the owned Sith soldier's geometry,
integrated head and materials while adapting its own bind/weights. It does not
substitute the player mesh. Generated assets and calibration remain ignored.

The opt-in arena single-saber soldier now owns its clip clock and blade sweeps.
The native physical-action boundary hands owned attacks to that playback instead
of starting a native weapon round. Native area/path services still execute moves.
NPC clip events use the shared posed-triangle contact and native damage service.
Native appearance services rebuild and restore head/equipment/material ownership.
The power/combat-lifetime service keeps the NPC saber active while owned.

Component source playback and independent two-body transitions passed. After the
preserved two-fix stop and further user approval, the live damage/resumption and
death/victory probe passed. Native creature-use movement accepts 0.1 m extra range;
owned saber approaches now request a precise standoff point through the existing
path executor. Attack range uses bind geometry and nominal blade length, so pose
and ignition animation cannot shrink its threshold after a movement completes.
The full port is not accepted. Pistol ready/fire clips and straight bolts use
owned JKA cadence/speed and the native muzzle. Posed body triangles and native
world obstacles determine contact; KotOR weapon stats determine damage. Rifle
clip selection and installed damage/lane/planning/restoration pass. Weapon-swap/casting,
moving-target CCD and visual-impact lifetime boundaries remain incomplete.

Source guard poses force walking and block frontal melee. Directional parry clips
remain available for later guard behavior. Reaction timing
is local prototype policy. All native status states and prone recovery yield
body/animation ownership to KotOR for both player and supported NPCs. JKA effect
fallbacks for choke/stun/sleep/knockdown/get-up are removed. JKA pain reactions are
disabled for owned enemies: weapon hits apply damage without a flinch animation
or interruption of their source swing. Spell/grenade damage cannot select them
either. Normal acquisition resumes after native
effects finish. Player incapacitation cannot prevent encounter victory, and
entering Engage during an effect retains native ownership. ForceTargetProbe casts
Choke, Stasis (native HOLD29) and Whirlwind, checking actual native clips/expiry.
Victory/release/HUD teardown
restore native ownership. Planning uses the scaled world clock. Other bodies,
chains, natural search, projectile/utility concurrency and campaign behavior remain
unfinished. Component playback does not establish full connected-skin or visual
acceptance. See STATUS.md for results and exact next action.

NPC native utility actions now wait at the existing action executor while a source
swing owns the body. Once the swing ends, the native appearance service restores
the original rig and releases ownership before executing the utility. Reacquisition
waits for native actions/status/active one-shots to finish. A completed spell
round's remaining bookkeeping time does not hold locomotion; physical weapon
rounds retain their existing ownership boundary. This reuses the
player's ownership boundary rather than attempting to play KotOR casting clips
on the JKA-adapted rig. The live cast-pose assertion specifically requires a native
casting clip, not an idle animation present during a spell round.
The NPC controller retains its live native-body identity and tactical timers
while animation is yielded, rather than erasing/recreating its combat intent.
Stale weapon/guard/chain poses are cleared and native bodies cannot use owned
guard, locomotion or blade tracing. Actor death, area/owner changes and combat
exit still release the controller. Reacquisition samples current facing/position and
binds fresh model parts without resetting JKA aggression/roam/strafe timers.

The same boundary covers spell/item use (including grenades and medpacs), mines,
healing, scripted animation, rest and utility interactions. Queued actions cannot
execute on an owned JKA rig. After release, both pending fire-and-forget entries
and the currently active native one-shot keep the original body until the client
returns to a looping animation, even if their action has already popped. The
active entry has left the pending queue. In particular, destroying the last
medkit can end its item action before inject finishes; reclaiming the source rig
then would strand a native dialog-animation flag and block movement. Wait for
the native lifecycle to clear it rather than clearing or bypassing movement gates.
An already-started source swing can finish before a utility action executes; a
received native status takes ownership immediately. UtilityProbe uses real native
item APIs on player and NPC and verifies throwgren/setmine/inject playback, item
consumption, damage/trap creation/healing, and return to their Engage rigs. It
also uses the player item-menu message path for medkits with2 then1 remaining,
holds movement through the native animation and requires automatic movement
recovery within6 frames after ownership returns, without a follow-up action.

## Arena humanoid enemy tactics

Interactive melee EnemyAI sessions now select the isolated k1darktest template,
derived from the owned n_darkjedim UTC. Story scripts/conversation are removed;
native appearance20, class/known powers and g_w_drkjdisbr001 equipment remain.
The body-specific k1jadark rig uses the existing retarget pipeline, original
geometry and explicit palette partitioning. Native-body/Engage-rig lookup now
supports both N_DarkJediM and the existing N_SithSoldier; idle calibration uses
the corresponding original resource. No global original-model override.

The local arena Force policy queues Choke/Force Push alternately between source
swings, or Drain Life below half health. It starts after3 world seconds, then
spaces successful queue requests by8 seconds. Known-spell and native affordability
checks precede the normal cast queue; blocked/exhausted actors continue melee.
Detection, clear sight, commandability, parry recovery and an8m selection range
gate casting. This is a local test choice policy alongside the existing grounded
JKA movement/melee port, not a port of JKA's full Force decision tree. Native
KotOR spell ownership, animation, cost, saves and effects remain authoritative.
Per-opponent cadence survives rig release and resets only for a new arena spawn.

Legacy smoke/control/gunner fixtures retain their soldier for comparable results.
DarkJediProbe explicitly tests the new profile: automatic native casts and Force
costs, source melee recovery, planning and native victory restoration across two
opponents using the production replacement path. Full body appearance remains
a separate visual acceptance check.

Interactive arena rounds now reuse the native creature template/equipment path.
After the tracked opponent dies and Engage restores exploration/native ownership,
the arena waits two seconds on the UI clock, validates a clear point three metres
around the player, creates one replacement and requests normal planning. Menus,
pause, player death and other modules prevent spawning; unavailable space retries
without forcing placement. The prior server creature is retired using the native
deferred destroy event after ownership release; its client corpse stays in the
area until unload. Weapon profile is preserved; player HP,
Force points and inventory are not replenished between rounds. Existing component,
control, navigation and reload probes keep their one-shot lifecycle; ordinary enemy
smoke checks additionally exercise two replacements, damage and native cleanup.

K1 desktop deaths retain their client bodies independently of appearance fade
timers. CSWCArea owns a dynamic session-only corpse list instead of evicting the
oldest body from the legacy four-slot ring. Corpse selection and hidden loot-bag
parent lookup use that list. UnloadArea removes retained objects from client AI
and the object array and empties the list; bodies are not serialized back into
the area. Native server death, loot transfer, script handling and AI removal still
run. Living-object scripted destruction and other build targets retain their
existing behavior. More corpses use more rendering memory until the area unloads.
The opt-in `-CorpseProbe -Smoke` EnemyAI arena test retains six client bodies,
checks server retirement, then uses native save loading and arena reentry. Each
old object must be absent immediately after unload. Saved-game IDs may be reused
for living actors afterward, so post-load checks distinguish those new actors
from retained corpses and require an empty corpse list on returning to the arena.

The opt-in `-EnemyAI` fixture intercepts tactical choices at K1's server
physical attack action. Native target detection and action groups still
own pursuit; owned clips/contact events replace physical round execution.
Saber fighters close to weapon range
and can evade a visible incoming swing. Gunners seek a clear firing lane
and step back when crowded. Reposition requests use K1's walkmesh checks
and movement queue. A short per-enemy cooldown uses the scaled world clock.

The gate requires Engage or slow planning against the player in the isolated arena.
It excludes companions, droids, passive attacks, and cutscene attacks.
Eligibility comes from the native appearance, with a cached skeleton check
for full-body models. It can be determined before client replication.
OpenJK's single-player Jedi and stormtrooper AI informed the behavior.
Implementation and tests are local K12 code.

Melee engagement range is capped at 1.85 m, inside the existing 2 m impact
contact test, so a fighter returns within reach after evading. This is local
prototype policy, not a claimed JKA constant. The gunner's lane test excludes
living allied/neutral personal-space volumes as well as native world obstacles.
The installed fixture verifies an allied blocker, native HP loss and repeated
Engage release/re-entry. Detection-loss is tested synchronously at the physical
action boundary; natural occlusion and search are not established. Native
higher-level spell/feat selection remains; its animation handoff needs verification.
The launcher locks its writable profile to keep concurrent processes from
overwriting each other's diagnostics and test configuration.

The original adapted candidate was a 66-node body with three pelvis-parented spine joints
and four JKA arm twist helpers. Native lower-bicep twist joints remain part of the
actual forearm parent chain and receive source-helper correction. Helper pivots
use full bind-chain displacement. Paired donor weights adapt upper-arm mass while
preserving original geometry/world bind frames and native palette/influence limits.
Source wrist/helper tracks remain independent of unskinned weapon tags. Transition
grip displacement is shared between both arms; source timing is not slowed.

The opt-in `-Arena -AnatomicalRig` path selects the local candidate only for the
owned PMBBL player fixture. Native creature appearance services rebuild the body,
head, weapon links and material state; no global PMBBL override or Steam writes.
Cached live appearance fields are used because the actor request record can be
stale. Pose references are cleared before body destruction. Menu/victory restore
the original body; component and installed actor pose comparisons pass separately.
A scene-attached actor must receive an explicit fresh evaluation in diagnostic
comparisons, unlike a detached oracle with no scene tick gate.

Connected stress and installed ownership pass, but full-body skin and dense angular
checks still fail. This is not a deformation-free implementation or full Ghoul2/AI
integration. Legacy adapter remains for the older default prototype. The arena AI
work above is separate from this rig checkpoint. Current measurements/settings/next action: STATUS.md.
Prior architecture checkpoint: ignored
`platforms/out/k1-engage-prototype/rig-candidate/design-before-actor-checkpoint.md`.

Approved project-local SciPy supports offline connected weight diagnosis only;
no game dependency. Exact-simplex graph-colored fits preserve non-spine mass
and cap final influences. Source/offline-layer training and continuous connected
validation remain distinct. Edge-only optimization is insufficient: Torso triangle
area checks reject fitted candidates. The approved thigh-helper extension and lossless skin partition are now implemented. No source
render-smoothing stage has been implemented.

The approved leg extension uses69 nodes and four skins. TorsoLegs is a face partition
with shared original-index mapping; every duplicated vertex retains identical skin
weights. Logical original-mesh reconstruction is diagnostic only; real native
palettes remain separately verified (15/15/9/11). Original arm/body joins use consistent
adapted weights. Original-pivot rig is current; source-proportioned pivot experiment
is rejected. Source sampling now includes JKA's default per-bone unsquash before
child propagation. The separately verified default2 Motion/spine controller is now implemented through
an unweighted j_motion part, giving70 nodes. Its animated quaternion stores source
Motion deviation; its bind quaternion retains exact raw source spine-axis calibration.
The live overlay selects the matching full-clip pelvis reference and actual playback
clock before JKA pitch/yaw/roll shares are applied. Original66/69-node fallbacks stay
available. Source-based references are distinct from retail executable verification.


Gameplay-review adjustments (2026-10-31): live movement yaw is capped45deg before
the existing source spine controller; its component-reference equations remain.
Engage entry is idempotent while active, preventing default-action/R requests from
triggering native-body rollback. Native flourish requests are gated during Engage.
Guard-only asset correction aligns the actual native pelvis/neck direction to the
JKA source while preserving limb locals, grip and head world orientation. Source
guard animation remains the foundation; native KotOR animation is not mixed in.
Settled arm display rotations use elapsed-time-adjusted damping and the existing
source-grip contact solve. Explicit transitions and attack clocks remain separate.
Current numeric/visual limits and evidence identities are recorded in STATUS.md.


Engage contact correction (2026-10-31): source hand contact solves the anatomical
wrist endpoint, retaining wrist world orientation and the independent tag offset.
Native skin rendering now exposes a K1-only nonvirtual posed-world-vertex helper;
it reuses native bone gathering, inverse binds and packed weights/palette indices.
The owning scene's completed render calls the Engage contact hook once per fresh
player pose. Native central beam endpoints sweep against posed hostile body/head
triangles with5deg angular subdivision, clipped to source attack hit windows.
Damage still uses native KotOR damage services, once per swing. Target-history CCD
and level-wall occlusion are not established. Incoming NPC contact retains the
earlier native timing/reach rule; this is not full JKA combat/AI integration.
Native combat lifetime is maintained while active because custom swings bypass
native attack rounds; native item power recovers without requiring enemy damage.
Missing animation runs self-heal through existing animation services. Spell rounds
retain ownership. These hooks are opt-in and leave normal exploration native.

The experimental -ElbowRig selector uses a separate ignored elbow-arena/profile
and preserves the original prototype profile. Engine services and runtime code are
shared. Only attack retargeting follows the actual source elbow plane/span, retaining
the calibrated guide near extension; idle, guard and gait pose sets are unchanged.
Offline paired-position refinement uses the existing arm/wrist solves when ordinary
feedback fails hand contact. Native bind/geometry, source, resource and installed
ownership checks pass separately from the retained skin/angular failures. This is
an opt-in visual experiment, not a replacement baseline or acceptance of the rig.

The separate -AimPitch option requires -ElbowRig. It supplies negative camera pitch
to the existing JKA Motion/spine equations before40/40/20 distribution. Previous
pose behavior remains when the option is absent; animation curves/assets unchanged.
The independent view-input oracle covers1620 cases through the full camera pitch
range. This corrects a missing controller input, not directional cut-plane geometry.

Engage camera neutral height uses native head/follow offset plus the current
camera style's height, rather than a fixed1.45m pivot. Capture normal appearance
offsets before rig ownership; retain existing mouse orbit and collision services.
The native combat-camera service supplies the independent installed height reference.
After screenshot review, both Engage camera routes subtract0.25m from that native
height for the closer orbit's framing. The explicit trim is a user-directed policy;
it does not change native camera services or animation assets.

Leaving active Engage seeds the native camera from the last displayed eye around
the actor, preserving its horizontal orbit side. Native combat projects local+Y
while Engage orbits along local+Z; reusing the pitched Engage quaternion reverses
the native heading for some pitch signs. Seed native yaw and follow desired
orientation/position, clear stale turn acceleration and follow look-at intent,
then retain the existing presentation blend. This covers planning and victory
handoffs while preserving native A/D rotation afterward. Installed diagnostics
compare nine pitch/yaw combinations and actual planning/victory transitions.
Native combat may clear while the final swing is still playing,restoring follow
onto the outgoing Engage body's Part tree. Rebuilding the native body invalidates
that watched target,and the follow behavior removes itself. A repeated native
SetCombatMode(0) returns early and cannot repair it. After Disengage rebuilds the
native body and clears combat,use the native SetCameraFollow service to rebind
missing/on-a-stick behavior to the current player follow Part,then reseed the
existing orbit from the pre-handoff eye. Preserve dialog/free-look ownership.
The lethal diagnostic checks all21 post-victory targets and exercises actual
native walking and configured camera rotation after two victories before reentry.
The native input service can also turn continuously when the revealed cursor
lands at a screen edge. Leaving active Engage inhibits that automatic input
until the pointer returns inside, without blocking A/D or right-drag controls.
Engage captures its cursor position before camera ownership. Both native turn
integrators clear their previous input as well as their current velocity.
The edge sampler reports validity separately from edge axes: zero axes during
relative mode, modal UI, focus loss or outside-window rejection cannot rearm the
handoff latch. Only a valid interior pointer sample does so.

Engage reference sampling must resolve NewController target offsets even before
native playback converts them to this build's Part layout. Use the same GetOffset64
mapping as NewController::Control, without modifying the controller. Warm source
component tests previously concealed this cold-start dependency. The candidate
probe now compares8 cold versus warm mixed reference poses before its full source
sweep. Live diagnostic input holds walking/running through active cuts; normal
manual input is unchanged. Runtime cut-plane IK trials were rejected for wrist/
finger compression and are absent from the active path. Further cut calibration
awaits visual review of the repaired source compensation; current limits are in STATUS.md.

Planning utility startup must not inherit invisible native turn one-shots from
Engage's animation ownership. CSWCCreature::AddFireForgetAnimation suppresses only
payload-free player turnleft/turnright clips while planning or active Engage;
native orientation continues to update. Other native one-shots remain queued.
The utility handoff still waits for the current saber swing to finish and uses
the existing native body/pose blend, action queue, costs and effect timings.
The isolated utility probe holds player actions in planning before resuming and
checks native startup within6 frames for in-range idle trials, alongside costs,
effects, NPC utilities, ownership recovery and victory restoration.

## Staff stance integration

K1_ENGAGE_STAFF_PROTOTYPE equips the staff in the established Dark Jedi arena.
It enables the same existing NPC AI, native utilities, poise/parries, contact
damage, chaining and dismemberment. No replacement arena or combat loop exists.
The original k1jarig and completed stance-specific calibration/refinement remain
unchanged; the player resource selector chooses the separate k1jastaff when this
opt-in mode is active and the actual player weapon is double-bladed. NPC single
saber eligibility remains on its existing path.

The player staff selects playback1.10 and a four-swing chain cap through the
existing controller. Yellow/Red/duals have three-swing caps; Blue/Purple/staff
have four. NPC staff playback is0.85. Both click
and held chaining use the selected player cap. Source clip/hit-window times,
native utility animation rates and movement speeds are unchanged.

Each physical staff blade can contact once per swing, tracked by swing serial,
weapon and a blade mask. Trace both beams independently, order same-frame hits,
and retain blade identity through the existing queue's duplicate suppression.
The first accepted contact retains the normal native health-damage opportunity;
later blade contacts are guard-poise-only. They cannot add health damage, create
extra reciprocal clashes, or survive interruption of the original swing. A guarded
staff blade spends11 defender poise, so two normal blocks spend22. Parry chance,
defender reward and break effects are unchanged; parrying cancels remaining contacts.
When the staff player is parried, spend35 poise rather than the default25.

Staff-player passive recovery uses2.5 quiet world seconds and1 point every0.5s.
Timing policy remains per actor; NPC/yellow use2s and0.1s. Activity, running,
swinging, status, cap and no-banked-tick rules are shared. Equipment-policy changes
preserve the last activity deadline and restart partial tick progress. Native
guard-break stun/reset stays2.5s/25%; its special immediate recovery resumes on
the first eligible0.5s staff tick, while subsequent stun hits retain their delay.
The staff contact probe covers both real beam contacts, duplicate suppression,
26 guard loss without HP loss, one native HP hit for two open contacts, actual
four-held-swing cap/0.95 playback, staff30/default25 parry penalties, and
live/synthetic recovery boundaries. The
existing yellow control/recovery probes remain independent regression checks.

The staff recipe reads the accepted body and preserves all its locomotion and
the unaffected locomotion/idle clips. Binary import does not set the exporter's animated
subtree flags, so the recipe restores that metadata before export and checks both
track/key counts and values on read-back. Native playback compares all8 full
walk/run cycles with the original resource. The recipe adds only the staff
MDL/MDX plus expected-pose diagnostics to the same ignored profile; it does not
copy modules, saves or configuration. Completed stance assets are read-only inputs.

The current native ready-pose transfer applies g3r1 during standing guard and
right-click guard-walking upper-body motion. Ordinary Shift-walk uses the completed
single-blade full-body walk without this guard override. Its pelvis-orientation compensation leaves the existing
single-blade walking lower body untouched, verified separately. Running reuses the
unchanged existing run refinement. Normal idle remains pause1; source-grip blending
follows authored attack hand releases. All7 staff attacks use owned S7/A7/R7 sequences and5 block reactions use P7
through the existing block/parry path. Staff-only timing is0.4-0.7s overhead,
0.25-0.55s otherwise for the strike segment. Contact now also includes the
existing60ms source-clock follow-through for yellow/NPC melee and most staff
cuts. Staff W+D/D continue contact for120ms after the strike, through0.67s,
matching their revised visible forward turn. Chain/guard/pose boundaries still
use the original strike ends. Shared time clipping
rejects windup/late-return contacts, including frames crossing either boundary.
Player/NPC speeds, chain count and damage calculation remain unchanged.
Single-blade calibration is not applied to staff, and completed stance functions,
constants and assets stay intact.

Staff attack posture is intentionally compacted before the existing retarget/IK
solve: first reduce skeletal excursion toward JKA's staff-ready locals to60%,
then halve pelvis/lumbar/thoracic/Motion absolute lean while retaining axial turn.
Staff directional review separates arm excursion from the compact trunk: the six
reviewed cuts use80% preparation,75% strike and85% finishing arm locals with100ms
ramps. S+A retains80% through release, then fades over100ms to its accepted60%
strike/finish. Overhead retains60% arm excursion, adds35deg forward-axis roll to
bring the cut toward vertical, and a20deg downward native-right-axis finishing
pitch. The measured source-blade continuation sign lifted the hands instead;
the downward sign was verified through paired IK before export. Other arm-subtree
roll is-30deg for W+A,-10deg for D and15deg for S+D. Positive roll had flattened W+A in
camera view; source/body-local angular proxies did not establish its screen angle.
Both arms/hilt follow the corrected plane through the existing paired-grip service.
W+D/A/D also retain their bounded15deg last-strike-axis arm continuation.

Use captured native shoulder-heading knots from the prior staff asset7b7b6356
with the real Motion/spine controller applied. W+D/D now continue35/25deg beyond
strike-end heading before easing into authored return. A/S+A add20/40deg rearward
preparation, fading during the first100ms of strike. Distribute all added yaw
40/40/20 across the existing three source spine joints, without restoring excessive
lean. Center S+D's upper pass using a35deg yaw envelope over0.36..0.53s through
the same spine/shoulder chain. The rejected arm-only centering forced the arm
solver against fixed shoulders and added a0.941rad forearm step; coherent turning
reduced it to0.571rad on the prior25deg candidate. The farther1.25m camera plane
exposed its remaining offset;35deg puts the crossing near48..49% screen width
across still/walk/run. Earlier easing and hand-target-shift studies were not adopted.
Lower-body/Motion poses, D/A clips and S+A's accepted later finish stay intact.
These are intentional local tuning values, not claims about original-JKA behavior.

The opt-in StaffArcProbe uses the same installed-data arena and existing controller,
records both native blades, front-plane intersections, actual camera projection,
hilt position and shoulder heading during21 single-click attacks (still/walk/run).
It suppresses opponent decisions only inside the diagnostic. Saved-data contracts
check the six requested directional behaviors in the current16:9 test setup;
standing/walking comparisons use the old captured build, while running checks
use explicit geometric targets because an old running baseline was not captured.
Numerical acceptance does not replace the user's visual review or erase retained
strict grip/arm/block/surface failures. Chaining remains a separately reported
unresolved staff behavior, not fixed by this animation correction.
Hand ownership is classified on the original authored motion, not the compacted
pose. Preserve the original right-hilt-local hand gap and rotate it with the
compacted right-hand frame before paired IK. Damping both arms independently
otherwise moved the offhand off the hilt and reclassified340 held samples as
released. Retain the original455 held/314 released samples and100ms approach to
attachment. A separate native diagnostic compares actual held-hand offsets to
original-JKA hilt-local expectations, independently of the exported pose oracle.
Preserve authored weapon-tag local transforms: independently damping them crossed
a shortest-arc branch and introduced an overhead attachment flip. Recomposition
carries the shoulders/arms/tags with the compact source posture. The staff-only
Torso back panel adds14 triangles on an existing16-vertex opening, using the rim's
native positions, UVs and weights. Original faces/weights/vertices are preserved;
completed body resources are untouched. Geometric incidence accounts for native
UV-split vertices, and connected cap checks run separately from logical-skin
reconstruction (which intentionally uses original face topology). Attack timing/input mapping and every non-attack clip remain
unchanged. Ordinary strafing adds a staff-only right-forearm tilt about the body's
forward axis (10deg left/15deg right), fading with movement yaw before smoothing.
It never runs during guard, blocks or attacks. This keeps the completed gait data
and shared run/spine services intact while clearing the longer lower blade from
native skinned legs. Checks compare both powered native blade centerlines with
both leg surfaces; sampled distance is diagnostic, not a glow-volume guarantee.

The new directions interpolate source key rotations before rigid retargeting;
the previous matrix interpolation sheared intermediate bone/tag frames. Previously
passing overhead/block sampling is preserved. Fixed tag sides0/2 follow pinned
OpenJK260c59c mdx_format.h and G2_ProcessSurfaceBolt2. Contact classification uses
the owned hilt and palm dimensions: hands away from the hilt use independent arm
targets, while held grips and blocks retain strict paired constraints. The staff
recipe preserves successful old solves and uses a bounded adaptive SVD fallback
only for failed near-extension poses. No new tool or game dependency is installed.

Staff released-hand targets approach paired attachment over100ms, using the same
wrist compatibility projection as contact poses. This prevents a discrete change
from shoulder-scaled independent reach to hilt contact. Strict held grip remains.
For W+A's final free-hand recovery only, interpolate left-chain local rotations
from the last held sample to the authored end with a quintic curve. A longer
target fade alone retained a0.453rad native upper-arm step; the recovery curve
reduces the largest measured release step from0.527 to0.240rad. Held keys,
right-hand source keys and the accepted strike remain intact. Runtime grip
correction redistributes arm rotation and shifts the displayed hilt up to2.4cm
during recovery; its exact-path preservation diagnostic still fails. The striking
path differs by less than0.4mm. This is a measured limitation for visual review,
not a claim of exact preservation or deformation-free motion.

The separate staff-editor authoring scene reuses the local KotOR model/texture
reader and armature builder. It bakes five native base block clips into six
ordinary Blender Actions, including a settled native side-guard pose from the
zero-yaw connected capture (state109/frame25). Head and hilt follow the actual
headhook/rhand bones; display-only cylinders show both measured powered blade
centerlines. Textures are packed. It neither deploys nor exports modified clips.
Runtime Motion/aim/grip correction is excluded from these editable base clips.
The importer assumes30fps regardless of scene FPS; a process-local120fps setting
preserves every native key. Reapplying bind matrices after lengthening its1mm
display bones avoids float-axis precision loss at the collar/fingers. Native key
and skin comparisons are independent of the importer's object-animation path.
The normal generator never writes the editor, and its builder refuses overwrite.
The attack-editor extension opens that existing scene and saves a separate file
with seven current full-body s_* attack Actions. It decodes native local keys by
name and bakes model-space targets onto the existing rig at120fps, preserving
inherited Actions. It makes quaternion component keys hemisphere-continuous;
q/-q sign switches preserve keyed poses but otherwise introduce between-key spins
in Blender. Native key and rigid-SLERP midpoint comparisons verify the bake, not
live controller behavior. The extension refuses overwrite and never deploys clips.

The separate swing-review proposal reuses the staff generator's solver/bootstrap
with isolated diagnostic destinations, stopping before its export/deployment
section. Preserve the s_* context identity: source-span pole guidance and arm
warm starts depend on it. A review_* identity silently bypassed those services;
independent W+A preservation checks detected it and the draft is retained.
Seven candidates keep their pre-swing/strike clocks, add0.18s of continued axial
and arm motion from the strike-end pose, then resume the authored R7 return with
a fading correction. This is an animation proposal, not an engine inertia model
or a deployed gameplay timing change. W's vertical calibration is confined to
0.38..0.70s, gone before the continuation/return; A/S+A preparation is enlarged,
S+D centering begins earlier, and W+A's accepted strike stays exact. W+D reuses
the released-only offhand recovery correction after a measured return spike.
Standing movies use full clips; forward-walk movies reuse j_walk, upper ownership
and the existing MotionSpine formula. Arm display damping, arbitrary movement,
chains and live state transitions are not simulated. Skin diagnostics retain
regional compression/stretch warnings; visual approval precedes any integration.
During staff clip transitions, StaffGripTransition uses the existing blended hand
trajectories in model space and projects the common hilt into both arms' reachable
volumes. It preserves the intended weapon orientation. The completed source-grip
transition implementation and its selection for other stances remain unchanged.

Export/source/grip checks pass, but connected local angular steps remain above
0.7rad, maximum1.1155rad on the compact-posture build. The original1.412rad upper-block forearm jump is corrected.
Remaining local weapon attachment counterrotation differs from visible world
rotation; source diagonal arms themselves also turn quickly. Neither observation
waives the strict check or proves deformation-free playback. Elbow surface-area
compression remains diagnostic evidence for visual review. Live player blocks,
Dark Jedi and native utility tests pass; the broader parry test stops at a missing
existing Dark Jedi block clip. That completed NPC asset was not modified. Full
two-blade hit coverage, angular acceptance and visual acceptance remain pending.

Staff checks cover source poses, full-cycle locomotion preservation, guard lower
body, transitions, grip, native skin binding and seams. Existing Dark Jedi and
utility tests cover the active opponent and native power/item handoffs. Surface
strain and the failed fixed-position directional killing-contact test remain
recorded for staff-specific follow-up; do not alter a completed stance to hide a
new stance's failure. Visual acceptance and complete staff hit coverage are pending.


Staff contact coverage (2026-11-11): enumerate draw-enabled native central beam
endpoints and deduplicate core/glow copies into two actual staff blades. Sweep
both through the existing posed body/head triangles, selecting earliest contact
across beams and submitting one native contact per swing. Preserve guard, parry,
clash, dismemberment and native damage queues. The isolated fixture grants only
missing baseitem-required feats and checks native CanEquipItem; it does not invent
attribute requirements or replace GetDamageRoll. The opt-in StaffContactProbe
positions each beam separately against actual native skin, checks excluded timing
and unpowered rejection, then confirms native damage delivery. Six trials cover
both beams for overhead and the extended W+D/D tails, including real contact at
0.66s that the old0.61s cutoff excluded. It is a controlled
integration test, not proof that every directional swing should hit a fixed target.
Target-history CCD and wall occlusion remain outside this correction.


The later stock-reset preview removes manual source shaping. Targeted twist
proposals derive from that preserved pose JSON without regenerating other clips.
Measure shoulder yaw relative to the pelvis in the actual preview ownership
frame, then reduce axial rotation over selected spine joints with a quintic phase
envelope. Preserve local positions and all arm/hilt tracks; rigid carriage keeps
grip spacing. An optional walk_poses variant allows walking-only adjustments.
Undo the existing MotionSpine local compensation while storing that variant so
reapplying the ordinary preview ownership produces the verified intended yaw.
Comparison movies can use a preserved proposal as their left-hand baseline;
these fields are preview tooling only, with no runtime asset deployment.


The isolated staff weight review keeps the stock-reset pose file byte-identical.
It redistributes only spine influence mass by neighboring actual spine bind
heights while freezing existing single pelvis/chest attachments and preserving
non-spine mass. Native influence/palette limits and actual partitioned-mesh seam
pairs remain explicit checks. Historical unsplit Torso seam indices are invalid
for this mesh. Blender preview vertices map by bind position plus original weight
signature; original/new weights switch for before/after rendering with the same
pose curves. The copied scene stores the trial weights. Independent native skin
comparison uses the new weight JSON with original inverse binds; it is not a
runtime-export test. Existing compression warnings remain part of visual review.


Yellow weight previews apply the same spine-mass redistribution independently to
actual player and Dark Jedi assets. They retain each body's own existing yellow
animation keys, binds and geometry. Original cross-skin boundaries with differing
influences (Dark Jedi torso/neck) are frozen on both sides rather than introducing
head-weight changes. The importer and established native sampler/pose baker create
15 ordinary Actions per copy, including the existing forward-walk ownership bridge.
Source fidelity checks target60Hz baked keys; arbitrary interpolated frames and
runtime smoothing are separate. Powered blade cylinders ignore only the owned
blade node's zero resting scale and are never collision geometry or exports.


The approved torso weighting is now exported only into k1jastaff. The staff
recipe uses k1staff_weights.apply_staff_torso_weights after its animation solve,
with the actual accepted body bind hierarchy. Existing animation curves and
completed stance services remain unchanged. Export preservation checks compare
skin weights to the approved staff snapshot and retain strict equality for other
fields. StaffMode already gates body selection on the equipped double-bladed
lightsaber; k1jarig/k1yellow and NPC resources are not reweighted. The weight-only
patch preserves the previous generation identity by reference, backs up both
native files and manifest, and updates the staff launcher hash. Native contact,
camera and arena checks cover the installed pair separately from preview fidelity.


Original-attack integration supersedes the older tuned staff attack tracks. Seven
full/seven upper clips are exported directly from the accepted original-JKA
preview poses; all non-attack clips and approved body weights remain unchanged.
The staff generator now sends authored joined source poses directly to the
established grip/retarget solver, omitting compaction, alignment rolls, synthetic
turns/follow-through and custom free-arm recovery. Native export validation uses
bind defaults for omitted constant root controllers. Isolated regeneration checks
its exact rootdummy-subtree controller coverage plus default top-level nodes;
this differs from an all-node preview count without changing the pose contract.
Runtime camera/aim/ownership/transition controls and native KotOR guard selection
remain separate from the source attack data.

## Dual-lightsaber selection and sandbox

DualPrototype or sandbox chooses k1jadual only when the native right and left
slots contain distinct single/short lightsabers. The native dual-saber animation
class remains authoritative. Completed yellow/staff selectors, assets and tuning
are preserved. The new body comes from the established independent-arm solver,
using owned S6/A6/R6 attacks, P6 parries, dual ready/walk/run clips and rigid local
rotation interpolation. Completed source mesh/weights are retained; connected
skin bounds and native attachments are tested separately from visual acceptance.
Yellow attack cut calibration is excluded by the dual selection path; ordinary
locomotion uses the same established full gait and run-refinement services.

The user-selected normal dual guard comes from native pmbbl g4r1, resolved with
CLIENT_ANIMATION_DUAL_SABER_READY through CacheNativeIdle and the existing native-ready transfer service, selected only
for dual guard. Its two hands stay independent during smoothing; the source
hand-gap projection must not displace the native grip. Walking transfers only
the upper guard while keeping the gait below it. All eight full walk/run clips
are inherited verbatim from completed k1jarig, including their Motion/spine tracks
and clocks. No hybrid upper/lower gait is generated. Independent native marker
and finger grips follow each held saber during locomotion after the shared run
refinement/smoothing, without replacing the gait's wrist/arm motion.
Walking holds the three spine model orientations at native standing reference,
retaining pelvis/leg turn and relative arm swing. Dual attacks also bypass the
single-hilt arm filter and relative-gap projection. The dual top cut rotates the
right arm inward around its shoulder with a boundary fade; the earlier diagonal
correction remains. Other non-locomotion keys and mesh weights are preserved. Export-controller comparisons and settled same-frame guard checks
separate gait identity from the intentional transition out of a preceding run.

The earlier g2r1 selection was a local identification error: it is the single-saber
ready animation. K1 native knockdown also has no SETSTATE marker, unlike the K2
handler; the dual player yields on native knockdown animation/commandability and
resumes only after native get-up. Its held RMB intent survives the handoff, with
release/re-press events still honored. Shutdown releases NPC bookkeeping without
accessing actor services after the server/client owners have been destroyed.

The PC HUD owns a second K1PoiseWeaponView for the actual left-hand resource.
Both scenes receive the same actor poise value and use the existing native
blade crop/render-state service. Equal viewports/camera framing put the offhand
24 scaled pixels below the main row, moving the shared percentage below both.
The second row hides outside dual mode/Engage. No live weapon geometry is edited.
GPU measurements cover full/half/zero/refilled poise and state restoration;
native glow color bounds are approximate pixel evidence, not a visual waiver.

k1dualstance.h traces each powered native weapon's real beam through the existing
posed-body/head triangle and time-clipping services. It submits at most one
contact per hand per swing to the existing ordered melee queue. Unlike a staff's
two beams on one weapon, the two independent weapons each have a native damage
opportunity. This is the initial local dual policy, not a JKA balance claim.
The inherited player playback rate/three-link cap and normal poise policy are
provisional; no completed stance is retuned. Native GetDamageRoll receives the
contact's offhand flag, retaining item/upgrades and native offhand strength rules.

Contact history uses client weapon IDs for model ownership. Queued events carry
server item IDs and the hand, allowing native slot validation before delivery.
Equipment changes, parry lockout and native status invalidate remaining dual hits;
a busy native utility round releases the hand's consumed bit for a physical retry.
Guard/parry/clash, dismemberment, object ownership, utility/status handoffs and
restoration continue through the existing services. The same generic NPC controller
selects a `dual` path for the registered new type; no alternate combat engine is added.

The dual NPC owns k1darkdual/k1arena_darkdual/k1jaddual. Generation reuses the same
independent-arm solver against the original Dark Jedi mesh and completed full
k1jadark gait. Native stats/powers are inherited from the existing fixture; the
new type uses the shared medium Jedi policy and dual rate/chain cap. NPC pose and
native-ready caches stay per actor. The paired native weapons retain hand0/1
history and consumed bits; each gets a health opportunity using its server item ID
and native offhand flag. Staff's second-blade poise-only policy is unchanged.
Source/native Force and status handoffs, factions, roster and cleanup use existing
services. Dual and other supported melee participants use the shared immediate
timed-deflection path described above.

Sandbox inventory supplies another single saber without replacing its starting
stance. Native equipment switching determines the player stance, and the existing
spawn arrows cycle all three registered NPC styles. Manifest-verified player/NPC
components are copied only after acquiring the existing exclusive profile lock.

k1dualprobe.h is an opt-in installed-data test: 21 live input-driven attacks,
powered native attachments/skin bounds, moving guard/held-chain cap and both
hands' actual beam contacts across all seven timing windows. It checks excluded
windup/recovery, unpowered rejection, deduplication, busy-round retry, cancellation
of a queued contact after parry and native health delivery/restoration. Controlled
weapon placements test contact plumbing rather than natural reach of each cut.
They do not establish campaign compatibility or visual/feel acceptance.
Natural top distance cases additionally verify one native damage delivery per
actual hand contact,including both contacts at the measured normal distances.
k1sandboxdualprobe.h exercises native equipment transitions,dual NPC AI/skin/guard,
independent hand damage,same-scene native effect snapshots with duplicate input,
native status/Force recovery and AI-off cleanup. The existing sandbox regression
continues to cover yellow/staff gameplay and area reload.

The dual player closes the concealed back opening using the reviewed staff
back-panel service: 14 added faces reuse the 16 existing rim vertices, UVs and
weights. Original faces and animation tracks remain intact. Completed stance
assets and the separate dual NPC body are unchanged.

After real native utility work ends, player and NPC handoffs release an empty
stale spell round through EndCombatRound(0) before source control resumes.
Active utility, native status and physical rounds retain their ownership.
Optional ContactTiming logs measure contact-to-health and rendered guard/block
response without changing native damage scheduling. The later timed-deflection
experiment retains immediate contact resolution, as described above.


Balance revision (2026-11-18): dual player/NPC playback is 0.85, chain cap 3,
each blocked blade costs 13 opposing poise, being parried costs 20, and a
successful parry grants 5. Dual recovery waits 3 quiet seconds and gains 1 point
per 0.5 seconds. Staff player/NPC playback is 0.97, each blocked blade costs 11,
being parried costs 35, and a successful parry grants 15. Staff recovery and its
four-swing cap remain unchanged. Single-blade tuning remains unchanged.
Successful parries during active passive recovery preserve its tick/deadline
and add the reward immediately, capped at 100; other activity rules still apply.

## Native swords and solid-model poise previews

Native base-item wielding remains authoritative. One-handed swords share the
single-blade selection path, two equipped single weapons use duals, and native
double swords use staff. Existing native damage, sounds, equipment ownership and
swept-contact queues are reused. Solid swords use measured guard-to-tip segments
instead of lightsaber emitters and do not depend on powered state. Existing Dark
Jedi rigs can follow sword wield style; unsupported NPC body/style combinations
retain native control rather than borrowing another creature's body.

prepare-k1-sword-geometry.py derives guard-rim, tip and central-grip bounds for27
owned native sword resources. Only these geometric markers enter source, not
mesh/resource data; original MDL/MDX hashes and native-data checks remain ignored.
The poise view loads the equipped sword resource, including each offhand model.
PartTriMesh applies a scoped screen crop only for these independent HUD previews.
Project the full grip box to retain the crossguard and pommel under perspective;
interpolate the blade cut toward the tips as poise increases. Double-ended swords
deplete both ends toward the central grip. At zero the grip remains; at100 the
whole original model draws. Restore scissor box/enabled state after each mesh.
Existing lightsaber blade/hilt render paths and all stance assets remain intact.

The isolated sandbox adds seven ordinary sword templates to its native inventory,
including two long swords for paired testing. Actual game inventory acquisition,
equipping and rendered results are user-tested; component checks use stubbed
engine/GUI services and separately verify owned native resources offline.

Sword attachment roll is an instance correction after the native attachment's
fresh GetWorld sample, applied in creation, dependent-position refresh and regular
control. Gate by actual held-model identity and Engage animation ownership; roll
only around the local shaft, keeping the grip and swept collision segments fixed.
The UI preview has its own90-degree roll and source framing length per row; fit
both flat-blade height and perspective depth while retaining the grip-box crop.

Player rig replacement must also hand off native camera targets. The old follow
part is destroyed with its body, and the native on-a-stick controller clears itself
when that target disappears. Capture view/type before replacement, ChangeTarget
to the new follow part (or recreate the cleared controller), restore the displayed
eye/rotation and refresh saved camera targets. The combat camera's controller can
remain; its saved follow target must still be refreshed. Other camera modes are
left under their existing owners. This fixes our appearance-replacement lifecycle
without changing native camera target-deletion behavior.

Directional player parries evaluate ParryAimAt for the contacting attacker.
Current successful-parry rewards (2026-11-18 revision): yellow 20,
double-blade 25 and dual 15, for both player and NPC. These supersede the
earlier balance-revision reward values above; other tuning is unchanged.
SelectedParryAim chooses a single reticle display target only; a different
nearby display target cannot veto an otherwise eligible directional parry.
Existing front/range/aim-area/dead-zone/direction coverage rules remain intact
for all stances. K1_PARRY_BLOCK_REASON records rejected aim eligibility and
direction, including the independent display target, for manual-session diagnosis.

### Native running across saber stances

Yellow, staff and dual player/melee NPC running samples native run on an unattached
CSWCAnimBase model owned by that body's PosePlayback. CaptureNativePose is the
unchanged model-space retarget math extracted from CacheNativeIdle; it maps each
sample onto the current adapted rig, including the extra spine joints. Sampling
uses the native clip length and a wrapping clock, decreasing for backward input.
This follows K12's existing WALK_BACKWARD reverse-playback policy; running uses
the same deliberate reverse policy. No original executable equivalence is claimed.

The existing j_* movement-base names remain selection/layer bookkeeping. Their
run locals are replaced before the established torso/yaw/transition services:
full native locals for free movement, pelvis ancestor chain and legs beneath an
upper overlay. The ancestor chain includes rootdummy, which owns the native run's
vertical bounce; excluding it previously flattened running while swinging.
Shift walking, guard-walking and other walking retain the previous JKA path.
The current +/-45-degree direction policy, attack/guard layers, speeds and final
native head animation remain active. Native runs bypass the JKA-specific run-refinement
pass; ranged NPCs keep their existing path. Sampler ownership
and clock reset with model rebuild, native-action handoff or HUD/area cleanup;
no native model is attached to the scene and no generated assets are required.

Run selection uses CSWCAnimBase::GetAnimationName with the native client animation
IDs: RUN_STUN_BATON/runST for double-blades and RUN_DUAL_SABER/runds for duals,
matching ServerToClientAnimation's powered-saber families. Player equipment mode
and each NPC's own stance pass the selection into the same sampler. Yellow
deliberately retains its accepted standard RUN/run rather than changing to runSS.
Changing the resolved clip resets the sampler's phase/length through its existing
transition path. Reverse running and swing root carry use the selected clip.

YellowNativeGaitProbe compares player/Dark Jedi/Sith retarget orientations against
fresh native models over forward/reverse cycles, verifies exact untouched upper
locals outside the locomotion root/legs, then drives 120 live movement/overlay cases
across yellow, staff and dual equipment selections.
It checks native leg locals after the existing 0.14-second transition settles,
current direction yaw, native movement speed and native equipment-menu
switches through staff/dual and back. A paired run/run+swing test verifies equal
native root bounce and pelvis world positions over repeated cycles. Visual gait
quality remains user acceptance. The legacy probe flag name is retained.

### Head rollback and dual presentation

Custom head targeting, forward alignment and torso-pitch corrections are removed.
Head transforms again come from existing animations/native behavior. The original
Engage animation-ownership services remain intact. Blender review exports resolve
current stance/native sources without modifying or deploying any animation.

Dual guard, stable torso and native grip adjustments precede SmoothBody for
both players and NPCs, so cached poses match displayed poses and entry/release
uses the shared transition blend. No animation assets change.

Guard entry/release now selects a 240ms upper-body transition for all three
stances, with per-body guard history alongside the existing pose history. A
reversal captures the last displayed pose. Root/pelvis/legs keep their ordinary
blend duration; attacks and block reactions also retain ordinary timing.
Guard gameplay eligibility remains immediate; this changes presentation only.
PresentationProbe checks all six player/NPC source models, rapid reversals,
unchanged lower-body locals and settled hand targets, alongside live dual
guard/NPC transitions and menu teardown/reload.

Native animation samplers have no creature external data and must not enter
NPC ownership lookup during destruction. PC K1 Gob destruction removes runs
unconditionally with kAnimRemove; gameplay ownership cannot block teardown.
This prevents map re-entry while sampler destruction clears NPC playback on
Quit. PresentationProbe exercises two native menu returns, a save reload,
menu draw/input readiness, nonblack screenshots and dual hand transitions.

### Target movement in melee sweeps

Melee narrow phase keeps previous/current rendered body roots for creatures in
its current scene. Each attacker's previous blade is expressed in the target's
current rigid frame before the existing clip-time and triangle sweeps. This
includes target translation/turning that could otherwise pass through a blade
between rendered samples. Player single, staff, dual and NPC melee use the same
service. Native posed body/head triangles and active windows remain unchanged;
no wider collision volume is introduced. Animation deformation within a frame
still uses the current native skin pose.

History is ignored for the first blade sample, different bodies/models/scenes,
scale changes and target movement above2m; area unload clears it. Unmoved roots
return the original blade without an extra transform. The snapshot commits after
both combatant sweeps and immediate resolution, so they use one rendered scene.
World-space blade motion remains separate for native contact direction and limb
throwing; target-relative motion is collision-only.

The opt-in MeleeHitProbe checks all three sandbox poise tiers and a parry override
in the actual label, live Purple/Blue/Red chain rates, native damage and once-per-
swing handling. Its native-mesh moving-target regression demonstrates an old
world-only sweep miss and tests the corrected hit, clip exclusion, identity reset,
teleport, stationary and rotation boundaries. MeleeHitMoving exercises running
upper-body layers; MeleeHitOpponentAi audits damage, blocks/parries, clashes,
rejections and dropped contacts with an active opponent, plus Blue NPC playback.


### Grenade landing and combat mine events

Custom combat keeps the native grenade flight duration, impact event and item
consumption. Grenade-type projectile11 uses one ballistic segment instead of its
four-segment bounce program when K1Engage is enabled. Thrown grenade item impacts
use the selected location with an invalid object target so a moving victim cannot
move the explosion after release. Other projectile programs retain their paths.

Combat NPC physics calls CSWSCreature::UpdateSubareasOnMoveTo after updating the
server position, as native movement does. Both passed-list and computed-list paths
retain crossed-only triggers even when neither endpoint belongs to a subarea.
Native OnEnterMine handles faction/reputation, immunity, scripts and one-shot
removal. Detection auto-pause remains native. The focused GrenadeMineProbe launcher
uses the isolated save profile and checks native grenade damage, walk/run/crossed
mine entry and friendly passage without replaying the roster.
