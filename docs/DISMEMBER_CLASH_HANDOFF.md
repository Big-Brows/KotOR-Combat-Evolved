# Fresh-agent handoff — 2026-11-23

Historical stopped checkpoint. The user subsequently authorized resuming after
usage reset; consult STATUS.md for current implementation and verification.

Read AGENTS.md and STATUS.md first. This is a checkpoint, not authorization to
resume work after the usage stop. The user requested this handoff; implementation
remains pending. No subagents, staging, commits, publication or game-install writes.
Preserve all existing local edits, including unrelated changes and untracked files.

## Exact outstanding user contract

Fix fingers remaining behind when arms/hands detach, and incorrect texture on one
of the two wound ends. All currently implemented NPCs support dismemberment except
Tusken legs, which the user explicitly prohibited. Do not revisit that exclusion.

Every saber clash must cost BOTH participants10 poise in ALL stances. Track an
uninterrupted clash episode for the same opponents. The immediate next successful
parry adds5 poise damage for each clash AFTER the first: five clashes cost each50,
then yellow base25 parry becomes45. Force powers, blocks and any other interruption
break the episode. Preserve consecutive swings/clashes until an interruption.

NPCs must sometimes parry instead of performing the next clash swing. Initially0%,
then5 percentage points per completed clash: after1=5%, after2=10%, after5=25% on
the would-be sixth swing. Cap100% is the implementation assumption. Roll once per
next-action decision, not every frame. Avoid accidentally multiplying this chance
by the ordinary contact-parry chance. Preserve ordinary parry behavior outside an
episode. Cover NPC-vs-NPC as well as player-vs-NPC. No combat code for this request
has been changed yet; do not mistake the existing Red-only10 cost for completion.

## Current edits and verification limits

`platforms/tools/prepare-k1-npc-dismemberment.py`: groups now follow parent ancestry
through bicep/forearm/hand bones. Native fingers are RaFngrB_g/RaFngrT_g and similar
Rb/Rc/Rd/La etc; old rfinger/lfinger prefixes missed them. Arm/hand cuts assert
pure-finger triangles are absent from the retained side and preserved detached.
Mixed-weight finger triangles are not completely covered by that assertion.

`src/game/clientcore/include/k1dismember.inc`: surface-owner classifier recognizes
native Fngr names. RestoreCutCapTextures restores dismember_* material resources
to jkcap / jkcapgl after retained-body appearance updates. Hypothesis: appearance
replacement overwrites retained wound material while detached Gob remains correct.
Diagnostic K1_DISMEMBER_CAP_RESTORED logs actual replacement under
K1_ENGAGE_NPC_CUT_TEST. This cause has NOT yet been confirmed in a new runtime test.

`src/game/clientcore/include/k1npcdismemberprobe.h`: live corpse verification now
checks retained cap materials too (retained_cap_material failure); detached caps
were already checked. Build command `platforms/tools/build-k1.cmd` PASS, linked
windows/k1.exe; log platforms/out/clash-dismember-fixes/build.log. Runtime NOT TESTED.

Full geometry regeneration was interrupted at the usage stop. Latest observed
geometry.log ends during pmbhm, with no final completion result. Session49701
received Ctrl-C and returned exit1. A later Get-Process blender found no process.
The earlier CIM-based cleanup failed due access restrictions; subsequent Ctrl-C
resolved it. Update the older cleanup uncertainty in STATUS accordingly.
Assets/report may contain a mixture of newly regenerated and old bodies. DO NOT
deploy them until a complete regeneration and provenance verification succeed.

Do not modify the generator while it runs: it reads its own file SHA per body,
so a mid-run edit can produce misleading mixed provenance. Serialize generation
and deployment; previous work encountered file locks by overlapping them.

## Build, generation and runtime workflow

Baseline1308a613dc8ff5631ebed0dfe202c0793d0e5374; branch
codex/unnamed-melee-npcs. K1 x64 k1-host-pc RelWithDebInfo/miniaudio;
engine fixes/KEEP_GOING_LINK ON, mobile/ASan OFF, K1 controller/explicit QoL ON,
K2-only QoL OFF. Build needs the documented MSVC environment; sandbox PDB-manager
restriction may produce C1902. The prior build succeeded with reviewed escalation.

Blender is installed via Steam. Use its existing blender.exe with
`--background --factory-startup --disable-autoexec --python-exit-code 1 --python
platforms/tools/prepare-k1-npc-dismemberment.py`; set K1_NPC_CUT_REBUILD=1 for all
bodies, clear environment in finally. K1_NPC_CUT_ONLY_BODY permits focused retries.
Use installed tools only. `python` is a WindowsApps alias; actual Python is recorded
as Python3_EXECUTABLE in platforms/out/k1-host-pc/CMakeCache.txt.

Assets: platforms/out/npc-dismemberment/assets; geometry-report.json and
runtime-report.json alongside. All generated assets/logs must remain ignored.
After generator exits0 and complete provenance validates, run
`platforms/tools/launch-k1-engage-prototype.ps1 -NpcDismemberProbe`.
Launcher validates source/destination hashes, uses exclusive engage-launch.lock,
and isolated profile/SAVES under platforms/out/k1-engage-prototype/elbow-arena.
It deliberately injects stale Tusken leg rows for rejection tests then restores
the normal table in finally. Do not bypass its save-path checks or profile lock.

## Combat connection points and traps

All in src/game/clientcore/k1engage.cpp; line numbers approximate:

- ParriedPoiseLoss~519: existing base is selected by ATTACKER stance; yellow25,
  staff35, dual20, single table25/20/15/25. Preserve existing base semantics unless
  a concrete requested change requires otherwise.
- ClashPoiseDamage~530 currently only Red returns10, others0.
- MeleePoise~490/map~622; NpcParryPercent~629 reads local override or fixture default.
  NpcContactParry~638 already caches contact rolls by attacker and swing serial.
- SpendMeleePoise~796 handles break/stun and clearing attacks. ApplyRedClashPoise
  ~820 snapshots both costs before either break; preserve reciprocal correctness.
- NPC threat/parry AI~4845 enters timed guard after threat detection, then calls
  ThinkNpcCombat. Ordinary parry timing can interfere with an episode action roll.
  Need deliberate once-per-action state, not an extra roll on each tick/contact.
- EndClashChain~5349 ends an attack chain, NOT a clash episode; both clash AND
  successful-parry paths call it. Blindly resetting new episode state here loses
  the bonus before it can be consumed.
- ProcessMeleeContacts~5400 resolves accepted deflections before body hits; clash
  contacts are deduplicated. Native guard/damage later provides interruption points.
- ApplySuccessfulParry~5654 spends ParriedPoiseLoss(attacker), rewards defender,
  ends attacker chain and applies lockout. Compute/consume bonus before clearing.
- K1EngageGuardContact~5710 handles player directional parry, NPC timed parry,
  then ordinary guard. Reset episodes on non-parry interruptions; also examine
  Force actions, opponent changes, death/revival, area cleanup and equipment swaps.
- ClashContactProbe~5458 derives some expectations dynamically from damage helper;
  this alone cannot prove new policy. Other fixed poise expectations around8449+
  and existing Parry/Poise/Clash probes need review.

Required focused evidence: five clashes50 each; immediate yellow parry45;
single-clash bonus0; Force/block/other-opponent reset; no duplicate-contact charge;
reciprocal break effects; NPC5/25/100% boundaries and one roll per decision;
repeated NPC-vs-NPC episodes terminate. Existing launcher -ClashProbe, -PoiseProbe,
-ParryProbe offer integration hooks. Do not stop at an isolated helper unit test.

## Prior completed dismemberment baseline (not proof of current fixes)

Prior checkpoint snapshot: platforms/out/clash-dismember-fixes/STATUS-before.md.
Detailed evidence index: platforms/out/npc-dismemberment/INDEX.md. Prior PASS:
398 native templates/79 labels,39 native body/armor models,310 cut pairs/620 models,
11160 native death-pose samples,214 native fatal cases across27 used bodies,
selected heads, both weapons, material partitions and cleanup. Menu394 templates
excludes Bouncer3 and Aqualish temporarily. New current fixes must be retested.

Legacy regressions previously passed: -DismemberModelProbe48 models/1440 native
poses/288 terminal poses; -DismemberProbe17 deaths/revival/stun/cleanup;
-DismemberLethalProbe21 actual killing contacts/seven cuts; -SandboxProbe704 previews,
selection/memory/Escape/mortality/AI/reload. Exact commands in prior checkpoint.
Old lethal-probe camera assertion was corrected to current Z/C binding (SDL Z29);
do not revert that to A. Movement bindings were not changed.

Generator reuses prepare-k1-dismemberment.py and prepare-k1-native-npc-rigs.py.
Native animation nodes must be marked animated before export or keyed child tracks
are silently lost; preserve existing roundtrip assertions. Packed-quaternion
normalization matches K12. Keep material partition handling for *_pN chunks;
Trandoshan Head_p1 previously exposed that bug. Cosmetic head attachments must
remain whole and detached weapons tracked for cleanup. Probe corpse keys use
client IDs while NPC tracking uses server IDs; delayed replication needs multiple
render samples, not an exact one-tick check.

Twelve completed stance MDL hashes are protected by the prior animation inventory
and platforms/out/npc-dismemberment/protected-stances.json. Do not retune assets.
Native stats, feats, powers and templates remain unchanged except the already
requested sandbox mortality behavior. No original-executable comparison or user
visual acceptance established; label those NOT TESTED.

## Documentation/navigation and next action

HANDOVER.md and docs/MELEE_NPC_HANDOFF.md contain earlier milestones;
docs/SANDBOX_SPAWN_MENU.md describes the existing menu. Keep menu/preview untouched
for this task. Many existing modified/untracked files are historical authorized
work, not all changes in this request; do not reset or clean them.

Last usage observation:90% used /10% remaining. AGENTS requires stop at this level.
A fresh agent must check current five-hour usage before implementation; a new chat
does not itself reset usage. This handoff is documentation-only at the user's request.
First implementation action once authorized and usage permits: complete geometry
regeneration and runtime cap/finger verification, then implement/test the full clash
contract. No combat replacement, architectural overhaul or new stance work.
