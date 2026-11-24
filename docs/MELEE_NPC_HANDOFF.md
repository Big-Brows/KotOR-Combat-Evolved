# Handoff: expand melee NPC support in KotOR:CE

Prepared from the current checkout for the requested next chat. This handoff is
documentation, not an implementation of the additional NPCs. Read AGENTS.md and
the CURRENT STATUS.md before acting. Later user instructions and actual source
take precedence over historical notes, including this document.

## 1. Objective and boundaries

The user wants all melee NPCs implemented through the existing Engage combat,
animation and AI services. Preserve the six working player stances, existing
NPC styles, native surrounding systems and the fixes described below.

The preceding discussion primarily concerned humanoid/sentient NPCs, excluding
droids and creatures. It did not settle a new creature/droid animation or AI
design. Start the humanoid coverage audit immediately; clarify that boundary
before implementing nonhumanoid combat. Do not silently call a humanoid-only
implementation complete for an explicitly broader request.

"All melee NPCs" must not mean only the short list of permanently melee-only
named characters. Include unnamed enemy variants, optional hostile characters,
weapon-dependent opponents and saber users who also cast powers. Mixed-loadout
NPCs need correct transitions between native utility/ranged control and owned
melee control. Do not convert everyone into a Dark Jedi, remove their powers,
invent weapons, change factions or replace unrelated campaign AI wholesale.
Unarmed, quarterstaff, stun-baton, creature and droid attacks are not automatically
covered by current sword/saber services. Audit them explicitly.

Keep the Taris arena and its manual spawn menu as the first reproducible test
surface. Campaign-wide activation is a separate integration boundary that must
be made explicit, tested and connected before claiming campaign coverage.

## 2. Actual checkout and safety

- Engine foundation: K12, HEAD `1308a613dc8ff5631ebed0dfe202c0793d0e5374`.
- Current branch: `codex/timed-attack-deflection`. Extensive local modifications
  and untracked source are intentional. The commit alone does not contain the
  prototype. Preserve all existing changes, including deletions.
- Continue in this SAME checkout. A fresh clone/worktree will not automatically
  contain the untracked implementation, ignored generated assets or user scenes.
- Remote verified read-only: `origin` is `https://github.com/shinyquagsire23/K12.git`.
  No pull, rebase, remote updates, staging, commits, PRs or publication authorized.
- Windows 11 / PowerShell, K1 PC x64, CMake preset `k1-host-pc`, RelWithDebInfo,
  miniaudio; engine fixes/QoL/KEEP_GOING_LINK enabled, mobile/ASan disabled.
- Build: `platforms\tools\build-k1.cmd`. Uses installed VS2022 x64 C++ tools;
  output `platforms/out/k1-host-pc/windows/k1.exe`. Read BUILDING.md first.
- Original K1 under the Steam `steamapps/common/swkotor` directory is read-only.
  Owned JKA data are also source inputs, not writable deployment targets.
- Writable arena profile: `platforms/out/k1-engage-prototype/elbow-arena/profile`.
  Launcher checks its isolated SAVES alias. Verify it before any save/load test.
- Generated assets, extracted data, source scenes, logs and binaries belong in
  ignored `platforms/out`. Verify ignore status before generating output.
- Keep the sibling former Rust project intact. Do not transplant it or resume
  its old milestones. No tool installation or large architecture replacement
  without user authorization. No subagents without explicit authorization.
- Check five-hour usage before extended work and at coherent milestones; stop
  starting work at <=10% remaining. Usage at this handoff's start was96% remaining.
- Unknown pre-existing `powershell.exe` and incidental PDBs in the root are not
  tools to execute, package or delete. Keep private paths out of shared docs.

For native builds/launches, sandbox escalation was previously needed for PDB,
executable and process access. Request only the specific required action. A
running game can lock k1.exe and produce LNK1168; do not terminate a user's game
or overwrite live profile assets to get past that lock. Asset deployment uses
the launcher's existing exclusive profile lock. Do not bypass it.

## 3. Read these sources first

| Source | Purpose |
| --- | --- |
| `src/game/clientcore/k1engage.cpp` | Main integration, player/NPC ownership, pose composition, decision loop, contacts, guards, poise, utility handoffs, sandbox |
| `src/game/clientcore/include/k1npctype.h` | Registered NPC identities and style policy |
| `src/game/clientcore/include/k1engagestate.h` | Exploration/Planning/Active and input policy |
| `src/port/k1engage.h` | Public engine integration boundary |
| `src/port/k1jkaai.{h,cpp}` | Grounded medium-Jedi AI decision port |
| `src/port/k1jkanav.{h,cpp}`, `include/k1groundmove.h` | A*, steering, world traces and grounded movement |
| `src/game/servercore/nwscreatureaicombat.cpp` | Native attack/movement interception and damage handoff |
| `src/game/servercore/nwseffectlist.cpp` | Queued native damage-effect application and contact timing observations |
| `src/libsource/AURORA/auranim.cpp`, `aurmodel.cpp`, `aurbehavior.cpp` | Fresh pose hook, animation ownership/removal, native attachments, teardown |
| `include/k1sabercontact.h`, `k1dualstance.h` | Swept physical contact and independent dual weapons |
| `include/k1attackdeflection.h` | CURRENT half-second deflection eligibility |
| `include/k1yellowlocomotion.h` | Native run sampling/retargeting beneath existing layers |
| `include/k1redchain.inc`, `k1bluechain.inc`, `k1redspins.inc`, `k1bluespins.inc`, `k1bluewindows.inc` | Per-style chain clips, timings and full-body spin selection |
| `include/k1swordgeometry.inc`, `k1swordalignment.h`, `k1yellowswordhooks.{h,inc}` | Solid-sword geometry, final blade roll and authored Yellow hook edits |
| `include/k1guardheadedits.inc`, `k1yellowguardarms.inc`, `k1npcguardedits.inc`, `k1staffguardhead.inc` | Approved later authored pose edits; preserve them |
| `include/k1sandboxpanel.h`, `k1sandboxdualprobe.h`, `k1dualprobe.h`, `k1yellowgaitprobe.h`, `k1presentationprobe.h` | Existing interactive/test services |
| `platforms/tools/launch-k1-engage-prototype.ps1` | Current profile preparation, deployment, flags and probe assertions |

Paths beginning `include/` in the table are relative to `src/game/clientcore`.
Use symbol searches instead of relying on old line numbers. Read MODLOG.md and
relevant docs/DESIGN.md sections, but expect older balance/history prose to be
superseded. In particular, historical ordinary `run` selection, world-forward
head claims, old dual/staff rates and old clash descriptions are not current.
The root HANDOVER.md contains useful historical context but is not this checkpoint.

## 4. What NPC support actually exists

This is not a general all-humanoid controller yet. `NpcRigResource` recognizes
only original `n_darkjedim` and `n_sithsoldier` body resources. A shared skeleton
or a melee weapon is insufficient to pass those gates.

For `n_darkjedim`, supported sword equipment can select staff or dual rigs;
registered Red/Blue identities select their own rigs; otherwise Yellow is used.
`n_sithsoldier` maps to `k1jasith`, but staff/dual combinations are rejected.
Unsupported bodies/styles keep native control. Do not replace a Wookiee, female
body, alien, armor appearance or attached-head character with the Dark Jedi mesh.

Creation/resumption is driven by `UpdateNpcPlayback` and `SwapNpcRig`:

- Require actual server/client owners, area, detected target, valid body/assets,
  appropriate weapon style, no incompatible add-in model or active native round.
- Exclude dead, party/player, passive-attack and inappropriate uncommandable actors.
- Maintain per-actor `NpcPlayback`, `PosePlayback`, native idle/guard caches,
  navigation, target, chain/deflection clocks and contact histories.
- `NpcType` currently selects by tag, defaulting to the Yellow Dark Jedi policy.
  An unregistered NPC is not thereby a fully supported new type.
- `nativeResource` is currently assigned with a hard-coded Dark-Jedi-or-Sith
  choice. Generalizing only the admission whitelist would sample the wrong native
  animations and break appearance restoration. Address this dependency together.
- `K1EngageEnemyAIActive` still requires the arena module/loadout, enabled enemy
  AI, an owned player and Active or Planning mode. This is an intentional current
  test boundary, not evidence that campaign enemies already use the new combat.
- Initial native physical attack suppression also calls `NpcRigResource`; keep
  admission, server suppression and client ownership consistent. Never suppress
  native combat for an actor that cannot acquire a working custom controller.

### Registered identities and effective current policy

| Style | Tag / UTC / rig | NPC rate | Chain cap | Default parry | Continue chain | Rest after attack |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Yellow | `k1arena_dark` / `k1darktest` / `k1jadark` | .85 | 3 |25%|85%|250ms|
| Staff | `k1arena_darkstaff` / `k1darkstaff` / `k1jadstaff` | .85 | 4 |20%|100%|150ms|
| Dual | `k1arena_darkdual` / `k1darkdual` / `k1jaddual` | .75 | 3 |25%|85%|250ms|
| Red | `k1arena_darkred` / `k1darkred` / `k1jadred` |1.00|3|25%|85%|250ms|
| Blue | `k1arena_darkblue` / `k1darkblue` / `k1jadblue` | .85 |4|25%|85%|250ms|

There is no registered Purple NPC or Purple NPC rig. Purple is currently a
player-only style. Unregistered supported actors use20% parry,100% chain
continuation and150ms rest unless current code says otherwise. Native integer
local `K1_ENGAGE_PARRY_PERCENT` overrides parry probability, clamped0..100.
Each incoming attacker/swing gets a stable roll; do not reroll every blade/frame.

## 5. Current AI and movement

`ThinkNpcCombat` thinks every100ms of world time. It uses native detection and
line-of-sight, target-velocity sampling with teleport rejection, and a local
five-second last-seen search goal. Ordinary melee movement calls
`K1JkaAI::mediumJedi`: persistent aggression/strafe/timers and grounded combat
distance behavior. Target prediction is300ms and reach uses actual retargeted
arm lengths plus native weapon length, capped at1.85m with an existing fallback.

Normal source aggression starts at3. Some diagnostics deliberately start it at6;
do not copy fixture aggression into normal behavior. Direction selection is a
separate local seven-cut policy, weights `[4,3,3,3,3,1,1]` (total18), not a full
port of JKA attack selection. Continued chains use the registered probability.

The source is a limited port from OpenJK revision
`260c59c2907187af555a676fe0cc798893bf7757`, not the complete AI_Jedi translation
unit. Boss, special Force, kick, saber-lock, taunt, airborne and other branches
remain omitted or locally bound. Force actions below use KotOR, not JKA effects.
Preserve GPL notices and `third_party/openjk/README.md` attribution.

Navigation reuses Raven A* and ported STEER, bound to the owned KotOR walkmesh.
Grounded friction/acceleration/slide movement and source yaw/backpedal rules are
in the existing adapters. World traces also test native creature cylinders and
choose the closest collision. No jump/crouch subsystem is requested.

Navigation graphs are preloaded/cached by area and footprint. `LoadNpcNavigation`
fails rather than building a missing graph in the combat loop. A graph validated
for a smaller actor cannot be reused for a larger actor. New Wookiee/alien sizes
need explicit footprint, doorway, collision and height verification.

`EngageMoveSpeed` intentionally matches the native player's base walk/run rates
for NPCs, applying each NPC's own native movement effects (not the player's buffs).
Established nominal rates are3.2/5.4m/s. Close guards/parries and attack-walking
use the shared walking policy; chase has a0.75m additional walk range.

The ranged branch has its own find-shot/make-space/hunt/stand-fire decisions.
Current initial gunner recognition is single blaster or rifle, not every native
ranged style. Preserve it when integrating mixed-loadout melee characters.

Sandbox target selection supports friendly-versus-hostile NPC combat as well as
player combat. Do not assume every target is the player. Audit player-global
inputs in generalization (for example `input.enemyShooting=state.attacking`) and
single shared `arenaForce` fallback before extending them to arbitrary actors.

## 6. Ownership, Force powers, damage and contacts

Keep native KotOR stats, item upgrades, damage rolls, effects, action queues,
factions, scripts and object ownership. Physical weapon/body contact replaces
the prototype's native melee hit-roll initiation; do not add duplicate damage.

Player and NPC native utility/status work temporarily releases the adapted rig.
Force powers, grenades, mines, healing, knockdown/get-up and death retain native
actions/animations. `NativeUtilityQueued`, `NativeStatusAnimation`,
`YieldPlayerNative`, `CompleteReleasedUtilityRound`, `SwapNpcRig` and release
services are the existing boundary. In K1, knockdown does not always have a
SETSTATE marker: dual-player commandability/native knockdown checks matter.
Held guard intent must survive native handoff and still honor release/re-press.

Arena Force policy is intentionally local: eligible NPCs use native spell costs,
effects and animations, alternate Choke/Push and choose Drain Life below half
health, with an eight-second successful-cast interval and retry when unavailable.
Do not grant these powers to ordinary melee soldiers or label it a complete
campaign Force AI. Preserve target identity and per-actor scheduling.

Fresh poses are adapted through `K1EngagePose` only when animation controllers
were evaluated. Reapplying pelvis/torso transforms to a stale pose compounds
them. Scene/revision checks prevent repeated contact samples.

Saber contacts use actual native weapon beam endpoints; solid swords use measured
guard-to-tip bounds. Sweeps hit actual posed body AND separate head triangles,
clipped to the current stance/chain's damage window. Reuse the ordered contact
queue, serial/revision deduplication, native damage delivery and dismemberment.

- Staff: two beams on one weapon; later blade contacts can be poise-only after
  the health opportunity. Do not accidentally grant dual independent health hits.
- Dual: separate actual weapon IDs and hand flags; each hand has its own native
  damage opportunity. Preserve native offhand strength/item rules, server versus
  client ID distinction, busy-utility retry and invalidation after equipment change.
- Do not flatten chain contact windows or include transition/windup time as damage.
  `ContactEnd` normally adds0.06s follow-through; staff has selected0.12s tails.
- Existing native damage effect application is asynchronous. Do not call a queued
  contact a health change until `nwseffectlist` actually applies it.

### Latest deflection rule: do not restore the old experiments

`k1attackdeflection.h` and `ContactCanDeflect` define the CURRENT rule: an incoming
melee BODY contact can deflect if the defender is still swinging and that swing
started0..500000 world microseconds ago, inclusive. Every actual initial swing
and automatic chain link resets that actor's clock.

No extra blade intersection, facing/direction, fresh-input or separate one-use
eligibility gate. Contacts resolve immediately; no deferred reciprocal-hit queue
or waiting for the half-second window to expire. Both chains stop on deflection;
existing clash audio, Red poise contributions, native guard breaks and NPC
restart policy stay in place. Ineligible contacts go directly to guard/damage.
The spatial-interception experiment was removed; its snapshots are historical.

## 7. Current player stances, tuning and preserved fixes

| Player style | Resource | Player rate | Chain cap | Principal source |
| --- | --- | ---: | ---: | --- |
| Yellow | `k1jarig` |1.00|3|JKA medium/S2 directional cuts |
| Red | `k1jared` |1.00|3|JKA strong/S3, quadrant-selected T3 links |
| Blue | `k1jablue` |1.00|4|JKA fast/S1, T1 links and own timing/spin metadata |
| Purple | `k1japurp` |1.20|4|User-authored idle/guard walk plus preserved Red cuts/chains |
| Double-blade/staff | `k1jastaff` |1.10|4|Staff S7/A7/R7/P7 source family plus native ready pose |
| Dual | `k1jadual` | .85|3|Independent-arm S6/A6/R6/P6 family plus native dual ready |

Single swords use Yellow/Red/Blue/Purple. Two eligible single weapons, including
supported mixed saber/sword pairs, select dual. Native double swords select
staff. `SingleSwordType` currently covers long/short/vibro sword/vibroblade;
`DoubleSwordType` covers double sword/vibro double blade/Wookiee warblade.
This is not generic quarterstaff/stun-baton/unarmed support.

| Style | Opposing poise loss on guard | Own loss when parried | Successful parry reward | Quiet / tick |
| --- | ---: | ---: | ---: | --- |
| Yellow |20|25|20|2s / .1s |
| Red |35|20|15|2s / .2s |
| Blue |15|15|15|1s / .1s |
| Purple |25|25|25|2s / .1s |
| Staff |11|35|25|2.5s / .5s |
| Dual |13|20|15|3s / .5s |

Poise is capped at100, passive ticks restore one point, break stun2.5s and
recovery25%. Existing swing/run eligibility and quiet-time gates remain. Parry
rewards during active recovery add immediately without resetting its clock.
Each Red participant in a resolved reciprocal clash contributes10 opposing poise
once; Purple is excluded. Use current functions, not old balance tables.
Player directional parry evaluates the ACTUAL contacting attacker, not whichever
opponent currently owns the display reticle. Retain overlapping quadrants and
existing front/range/aim rules. Parried attackers have the existing1s restart gate.

Important presentation fixes to preserve:

1. Native runs: single styles now use `runSS`, staff `runST`, dual `runds`.
   Backward running reverses the selected cycle. Ordinary Shift/guard walks keep
   the accepted JKA paths except Purple's authored guarded walk. Rootdummy/pelvis
   ancestry carries native vertical bounce underneath running swings. Do not
   restore ordinary `run` from an older document.
2. Red has49 from/to links, selecting direct cuts when quadrants match. Nine
   spinning links require full-body playback, as do the corresponding Purple
   links; retain their authored pelvis/legs and exempt them from ordinary native
   gait/spine correction. Blue has separate spin metadata; do not borrow Red's.
3. Purple is a separate resource, with authored idle/guard walk/backwalk. Its
   repaired walk removes only proven stale interleaved template half-frame spine
   keys, retaining authored keys, legs and132 inherited Red clips. Do not resample
   it uniformly or overwrite it with Red's guard. Earlier150% speed is obsolete.
4. Guard entry/release and held-guard stance cycling use the existing0.24s guard/
   stance blend; ordinary source-rig blend is0.14s. Moving dual guard/torso/grip
   adjustments must precede `SmoothBody`, so the cached pose is the displayed pose.
   Post-blend overrides caused instant saber flips and guard snapping.
5. Native dual ready is `g4r1`, staff ready `g3r1`; `g2r1` is single-saber ready.
   Dual hands remain independent; do not apply the single-hilt gap projection.
   Preserve dual top/diagonal calibration and stable walking torso/grips.
6. The global crosshair/target/forward head-correction experiment was reverted.
   HOWEVER, later user-authored guard/head/arm edits ARE now intentionally active
   through the generated tables listed above. NPC guard transfer explicitly aligns
   the visible skinned head_g with the authored headhook reference. Preserve this
   later work; do not delete it because an old handoff says all head changes gone.
7. All six player bodies have the reviewed backpack opening closure (14 added
   faces reusing rim vertices/weights); preserve existing skin and remaining faces.
   New bodies require their own actual mesh/skin checks, not blind topology copying.
8. Appearance replacement rebinds/recreates the native follow controller, preserves
   displayed eye/rotation and refreshes saved camera targets. Do not reintroduce
   broken camera-follow pointers when adding NPC/player swaps.
9. Solid-sword attachment alignment affects held INSTANCE transforms after native
   refresh, not weapon meshes or collision/grip positions.27 native sword models
   have measured geometry; independent HUD previews retain hilts at zero poise.
   Sword meter scales are90% for single/dual and120% for double swords.
10. Yellow has saved per-clip rhand edits, cutting-plane roll and selected180-degree
    attack facing reversals. Its relaxed idle AND walk/backwalk use the corrected
    -90-degree orientation. Red/Blue/staff/dual alignment follows each selected
    attack/chain; Purple takes Red attack alignment but keeps its nonattack facing.
    Reuse `K1EngageAlignSword` and selectors; do not impose one constant rotation.
11. Native sampler teardown must not query NPC maps being destroyed. `NpcOwnsBody`
    rejects scene-less/non-creature samplers; K1 PC Gob destruction force-removes
    runs with `AnimRun::kAnimRemove`. This fixed Quit-to-menu's black-screen crash.

## 8. Installed animation inventory and editing provenance

Read-only inventory generated for this handoff by parsing the CURRENT profile's
MDLs; it does not regenerate, install or edit assets:
`platforms/out/melee-npc-handoff/animation-inventory.json`.
It contains every installed clip name, duration, supermodel and MDL SHA-256.

| Resource | Installed clips | Role |
| --- | ---: | --- |
| k1jarig / k1jastaff / k1jadual |35 each|Yellow/staff/dual player |
| k1jared / k1jablue |133 each|Red/Blue player |
| k1japurp |135|Purple player |
| k1jadark |60|Yellow Dark Jedi NPC |
| k1jadstaff / k1jaddual |70 each|Staff/dual Dark Jedi NPC |
| k1jadred / k1jadblue |158 each|Red/Blue Dark Jedi NPC |
| k1jasith |70|Supported Sith soldier NPC |

All12 inventoried resources currently report NULL supermodel. Native runtime
sampling still reads original models; an installed custom clip does not prove it
is the final gameplay pose. Do not substitute `k1japurple` for actual `k1japurp`.

Common animation families:

- Bases: `j_idle`, `j_guard`, `j_walk`, `j_run`, `j_backwalk`, `j_backrun`,
  `j_leftwalk`, `j_rightwalk`, `j_leftrun`, `j_rightrun`.
- Seven attacks: suffixes `top, tlbr, trbl, lr, rl, bltr, brtl`; `s_` full body,
  `y_` upper overlay. Source direction selection is in K1EngageState and the NPC
  weighted selector; do not reinterpret clip names as input directions blindly.
- Player blocks: `j_block_` / `y_block_` with `up, ur, ul, lr, ll`; held overlay
  `y_guard`. NPCs also have `j_parry_` / `y_parry_` directional families.
- Red/Purple links: `s_r00..s_r66` and `y_r00..y_r66`,49 each.
- Blue links: `s_b00..s_b66` and `y_b00..y_b66`,49 each.
- Purple-only guarded locomotion: `j_purpwalk`, `j_purpbackwalk`.
- NPC resources include ready/fire (`pistol`, `rifle`), choke, knockdown, get-up,
  stun, pain1..3 and death1..3 families where listed in the full inventory.
  Their existence does not authorize overriding native Force/status/death ownership.

Generators use the existing Blender/importer/native binary pipeline:
`prepare-k1-{staff,dual,red,blue,purple}-prototype.py` and
`prepare-k1-dark-staff.py` under `platforms/tools`. Inspect dependencies before
running: regeneration can overwrite later authored/deployed changes. Source
scenes and protected stance resources must be preserved with hash snapshots.

Relevant later tools include `apply-k1-guard-head-edits.py`,
`apply-k1-staff-head-edit.py`, `apply-k1-yellow-guard-arms.py`,
`prepare-k1-npc-guard-edits.py`, `apply-k1-yellow-sword-hooks.py`,
`fill-k1-back-panels.py`, and sword geometry/export/slot-repair tools.

Existing local Blender is the Steam Blender installation (5.3 at last export).
Existing KotOR importer is under `platforms/out/kotor-animation-import/tools`;
the read-only KEY/BIF helper is
`platforms/out/k1-engage-prototype/dark-jedi/inspect-owned.py`. No Python package
or Blender add-on installation is required merely to reuse these local tools.

The old `platforms/out/animation-head-review.zip` (11 scenes/1201 clip entries)
predates Red/Blue/Purple and swords. It is NOT the complete current editing pack.
Newer head editors/evidence are in `stance-head-edit-pack`,
`stance-head-original-keys`, `stance-head-guard-walk`, `stance-head-applied`,
`npc-guard-edits`, `staff-guard-edit`, `yellow-guard-edits`, `red-guard-edit`.
Sword editors are in `platforms/out/sword-stance-edit-pack/slot-fixed` (six
Blender files and action-ranges.csv). Prefer slot-fixed copies: earlier action
dropdowns selected mismatched slots and looked motionless despite keyed actions.
Preserve original curve times, interpolation and handles. Never silently save
over the user's source scenes or deploy unreviewed edits.

## 9. NPC roster audit: do not treat the earlier chat list as a specification

Earlier lists were a human-readable research aid, not an audited mapping from
placed NPCs to supported custom melee rigs. Strict melee equipment is also not
the same question as custom melee capability.

Read-only data evidence is in `platforms/out/enemy-catalog`:
`index.json` inventories module UTC names/factions/appearances; `melee.txt` lists
selected named templates' equipped/inventory base-item types. `scan.py`,
`global.py`, `placement.py`, `melee.py` show how it was obtained. These include
noncombat/cutscene/unused templates; faction defaults alone are not proof of
hostility. The previous broad list also grouped variants rather than enumerating
every placed/spawned NPC. Do not infer Force capability from a top-level empty
SpecAbilityList; check native class/known-spell data and scripts.

Confirmed melee-only default loadout candidates previously identified: Dia,
Redros, Marl, Gadon Thek, Kaeira, Sherruk, Gorwooken, Warleader Garn, Luo,
Loremaster Gjarshi, Keeper Orsaa, Researcher Ll'awa. This is NOT the full target
roster and not an audit of all script-driven changes. Freyyr has encounter
versions with grenades; Brejik, Zaerdra, Kandon, Selven, Ice, Jagi, Xor, Dern,
Dehno and Chuundar have ranged options. Saber users may also cast native powers.

Build a coverage matrix from actual placed creatures, encounter tables and
script spawns, including named and unnamed enemies, optional hostility and
equipment changes. Record template/tag, native appearance/body/head, rig family,
size/footprint, equipped weapon styles, ranged/utility capability, required
animations and verification status. Classify support explicitly as working,
native fallback, unimplemented or excluded-with-user-agreement.

## 10. Commands, evidence and limits

From the project root:

```cmd
platforms\tools\build-k1.cmd
engage-k1.cmd -Sandbox
engage-k1.cmd -StaffPrototype -Sandbox
play-dual-k1.cmd -Sandbox
play-red-k1.cmd
play-blue-k1.cmd
play-purple-k1.cmd
```

Space enters Engage; LMB attacks/hold chains; RMB guards/forces walk; Shift walks.
Top-row0 toggles the native Spawn Menu. It supports Friendly/Hostile and AI On/Off;
spawn placement refuses blocked space. The five registered styles are available;
base-NPC expansion remains unfinished. Middle mouse cycles installed single
Yellow/Red/Blue/Purple styles and blends while preserving held guard. Native
equipment selects staff/dual independently. Planning runs at0.5% speed with player
movement locked. Preserve camera/pause/menu state across transitions.

Existing installed-data launchers, inspect current scripts before using:

```cmd
engage-k1.cmd -PresentationProbe
engage-k1.cmd -YellowNativeGaitProbe
engage-k1.cmd -SandboxProbe
engage-k1.cmd -DualSandboxProbe
engage-k1.cmd -DualProbe
engage-k1.cmd -RedProbe
engage-k1.cmd -Sandbox -ContactTiming
```

Expected exit0/PASS only for their documented covered contracts; preserve a
failure rather than weakening assertions to claim success. The legacy gait
probe drives120 Yellow/staff/dual movement/overlay cases, not every new style,
body, campaign encounter or latest deflection path. Component tests with native
world services stubbed are not installed-game compatibility proofs.

Current STATUS.md records the latest timed-deflection component harness:
`platforms/out/timed-deflection/build-component-test.cmd`,420 cases; desktop build
passed. Its actual-source tests cover inclusive0..500000us, expiry, chains,
duplicates, cancellation, break/guard/damage and rejection states. Native world,
clock and damage services are stubbed. Installed-game deflection/feel and original
game comparisons are NOT TESTED; no game was launched for that checkpoint.

Further evidence directories (read relevant reports/logs, not every file):

- `platforms/out/all-sword-facing`:1833 Yellow samples,308 stance/hand/pose
  cases,490 chain selections;402 required clips across six player resources.
- `sword-presentation-fixes`, `swords`, `yellow-sword-edits`,
  `yellow-sword-facing-tuning`, `yellow-sword-transfer-review` under platforms/out.
- `red-stance`, `blue-stance`, `purple-stance`, `red-spin-fix`, `stance-balance`,
  `swing-rates-red-clash`, `back-panel-all-stances` under platforms/out.
- `guard-blend`, `npc-guard-edits`, `presentation-fixes`, `head-revert`:
  historical installed-data guards/menu/gait checks; head-specific assertions
  from removed experiments are historical, not contracts to restore.
- `platforms/out/dual-stance`: dual contact/equipment/native ownership evidence.
  Earlier broad native-round damage/ownership fixtures had unresolved failures;
  do not claim they are fixed by passing presentation or stubbed component tests.

Root STATUS.md may describe the latest focused task while MODLOG.md and source
contain additional local work from other chats. This handoff checked both; use
actual code and asset inventory to resolve conflicts. Never overwrite another
chat's local implementation simply to make the old status narrative match.

## 11. Suggested implementation order and completion contract

1. Read this handoff/AGENTS/current STATUS; inspect the exact working tree and
   current code. Establish a dedicated local branch if implementation needs it,
   preserving all local work. Snapshot current assets/tests and define one
   current STATUS checklist. Do not regenerate completed stances as preparation.
2. Produce the actual NPC coverage matrix. Start with an ordinary humanoid
   single-sword enemy of an unsupported body/head family and its native restore
   path. Retain its real mesh, appearance, equipment, stats and faction.
3. Generalize descriptor/rig/native-source selection consistently across
   `NpcRigResource`, client admission, server suppression, restoration, pose
   caches, weapon geometry, navigation and cleanup. Reuse services; avoid
   parallel per-NPC combat engines or a blanket appearance whitelist bypass.
4. Validate one complete new NPC through approach, walking/running, seven cuts,
   chains, guard/parry/deflection, native damage, Force/status if applicable,
   death, menu return and area reload. Include actual skin, separate-head and
   hand/weapon geometry checks. Keep source files and prior stances unchanged.
5. Extend family by family: different humanoid sizes/sexes/armor/head layouts,
   Wookiees/aliens, single/dual/double weapons, optional hostility, NPC-vs-NPC,
   mixed ranged/melee switches. Define unsupported unarmed/baton/staff behaviors
   instead of pretending a saber rig automatically handles them.
6. Make the campaign activation boundary an explicit decision and implementation;
   then test native detection/party/story/quest scripts and restoration on real
   areas. Arena success alone does not fulfill campaign-wide melee support.
7. Re-run focused existing regressions at changed integration boundaries, log
   the full coverage matrix and remaining exclusions, and request user visual
   checks only for observations instrumentation cannot establish.

Completion means every agreed in-scope melee NPC family has real admission,
appropriate animations/weapon contacts/AI, native utility and restore behavior,
and documented installed-data verification. No silently unsupported bodies,
duplicate native/custom attacks, zero-damage fake contacts, mesh substitution,
stuck native rounds, broken appearance/camera cleanup or untested broad claims.
Do not retune working stances, restore removed head tracking, import a second
engine, rebuild unrelated systems or publish anything as part of this work.

## 12. What this handoff task itself verified

Read current code, build/launcher documentation, current STATUS and recent
MODLOG; recorded actual branch/HEAD/remote and parsed all12 installed custom body
MDLs into the ignored complete animation inventory. No game assets, engine source,
user Blender scenes or native installation were changed by this task. No game
launch or engine rebuild was required for this documentation-only handoff.
Focused documentation/reference checks passed. A whole-tree whitespace check
also reported a pre-existing blank line at the end of k1engage.cpp; this handoff
did not modify that engine file or discard unrelated work to remove the warning.

Suggested first prompt in the new chat:

> Implement the melee NPC expansion described in docs/MELEE_NPC_HANDOFF.md.
> Read AGENTS.md and the current STATUS.md first. Preserve the current local
> work, six player stances, authored pose edits, native sword support and timed
> deflection. Begin with the actual coverage audit and one complete unsupported
> humanoid family through the existing NPC services, then expand systematically.
> Do not stop at a plan or claim all-NPC coverage from the existing Dark Jedi rigs.
