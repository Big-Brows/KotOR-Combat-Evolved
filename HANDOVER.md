# Handover — 2026-11-16

Latest checkpoint: [finger/cap and saber-clash handoff](docs/DISMEMBER_CLASH_HANDOFF.md)
(2026-11-23). Read it and STATUS.md before the historical notes below.

For the requested melee-NPC expansion, read the newer
[melee NPC handoff](docs/MELEE_NPC_HANDOFF.md) and CURRENT STATUS.md first.
The historical branch, stance roster, balance and ownership descriptions below
are not the current implementation checkpoint.

This is conversation context, not permission to resume paused work or implement
every idea below. Read AGENTS.md and STATUS.md first. Consult MODLOG.md and only
the relevant sections of docs/DESIGN.md as needed; avoid re-investigating settled
findings. Current user instructions take precedence over historical preferences.

## Where the conversation stopped

Update after the explicitly requested portable-launcher work: a local candidate
now exists in ignored `platforms/out/KotOR-CE-Portable` and its adjacent ZIP.
The product/project is now **KotOR:CE**, with **KotOR-CE.exe** as its Windows
launcher. Old Kotor-K12-named artifacts remain historical outputs. The checkout
folder remains in place; the Codex sidebar label still needs a manual rename.
Read the CURRENT STATUS.md and docs/PORTABLE.md first. Native startup, owned-data
preparation, relocation and privacy checks pass; actual GOG data/another PC and
redistribution review remain unverified. Both owned KotOR 1 and Jedi Academy are
required, as chosen by the user. The historical statements below about an
unimplemented pipeline describe the earlier checkpoint. Font fixes remain paused.

- The latest implementation task was cleanup, and it is complete. Nothing was
  staged, committed, pushed or published during the recent sandbox/font/cleanup
  work. Preserve the working tree, including deletions and new untracked source.
- Branch: `codex/tester-cleanup`. HEAD:
  `1308a613dc8ff5631ebed0dfe202c0793d0e5374`.
- The user explicitly **paused font fixes** to discuss distributing tester builds.
  Do not resume those fixes without a new request.
- Recent discussion concerns a portable, privacy-clean Windows release with
  double-click startup and eventually repeatable packaging. This is a proposed
  workstream, **not an implemented release pipeline**.
- No fresh repository, portable setup EXE, sanitized release binary or distributable
  ZIP has been created. Do not initialize a new repository inside this checkout.
- Unknown pre-existing untracked `powershell.exe` was preserved. Do not execute,
  package or remove it without establishing its provenance.

## User preferences that matter

- The user is new to releasing software. Give concrete, plain-language steps and
  exact commands. They prefer double-clickable EXEs over CMD/PowerShell workflows.
- Keep building on the existing Taris fighting-arena scene. They objected to
  recreating the scene or losing previously working combat, animations and NPCs.
  Testing should remain independent of campaign/story progress.
- Show animation previews/screenshots when a visual decision is necessary, instead
  of repeatedly requiring full game launches. They may accept small close-up
  distortions that are unobtrusive in gameplay; severe stretching, holes, detached
  body motion, separated hands and torso collapse are unacceptable.
- Do not promise deformation-free animation from a successful build or isolated
  bone checks. Connected skin, transitions and repeated native/source ownership
  changes exposed failures that individual pose tests missed.
- An earlier animation-only session allowed cause-based work beyond the two-fix
  rule. That was not a blanket exception for fonts, packaging or other tasks, nor
  permission to resume paused animation work.
- Ask before downloading/installing missing tools, including tools that would
  materially help. Existing project-local tools can be reused. Do not interpret
  prior approval of one importer/rig adaptation as approval for unrelated installs.

## Combat direction and later choices

- Single-player only. No jump or crouch. Goal: grounded JKA-style directional
  melee, movement, chains and humanoid combat AI within KotOR's surrounding systems.
  Movie Battles II inspired the feel; its entire system/source is not an approved
  dependency. User chose a behavior port for enemy AI rather than a direct OpenJK
  code import. Do not claim complete JKA parity based on the current prototype.
- Retain KotOR weapon/stat damage and upgrades; physical melee contact replaces
  hit rolls. Force powers, grenades, mines, medkits, affected-character status
  reactions and deaths use native KotOR actions/animations. JKA animations must
  not override those native utility/status actions.
- The user initially wanted pure JKA animation during integration, but later
  explicitly chose KotOR idle in Engage and KotOR's side-held double-blade guard.
  Follow those later choices, not the earlier blanket animation statement.
- Current staff attacks were reset to original JKA motion with approved body
  weights. Do not restore rejected exaggerated windups/follow-through edits or
  Movie Battles II motion. The desired motion still has preparation, strike and
  momentum through recovery without severe body distortion.
- Added Engage-only spine/arm-helper rig adaptations and adjusted skin weights
  were explicitly approved. Native bodies must be restored correctly afterward.
  Approved staff-only player weights must not silently affect yellow stance.
- Double-blade ordinary Shift-walk uses the single-blade walk; moving guard uses
  the current staff guard-walk. Hands remain on the hilt while walking; they may
  release during running or when an authored attack calls for it.
- Normal controls: A/D strafe, Z/C rotate camera, Shift walks, RMB drag returns
  the pointer to its original location. Engage uses mouse-look, LMB attack and
  RMB guard; holding guard forces walk. Both sides intentionally use the player's
  native movement speeds: walk3.2/run5.4m/s, replacing the soldier's slower walk.
- Planning runs at0.5% speed with player movement locked. Native physical attack
  options are suppressed; utility selections execute through native actions.
  Space enters Engage. Returning from menus/planning/death must preserve camera
  continuity and must not accidentally revive native melee control.
- Chains support holding LMB and have bounded length. Guard/parry interrupts and
  post-swing recovery must be smooth. User wanted chaining near the end of the
  swing as well as a modest earlier allowance, not just a longer early buffer.
- Simultaneous-hit clashes mean nearly simultaneous **body hits**, not saber
  collision. Last agreed window0.5s; reset chains, with no health or poise damage.
- Parry UI uses four overlapping yellowish quadrants and a center dot; it should
  appear near an opponent without exact crosshair alignment. Use appropriate JKA
  block reactions. Current tuning lives in code; do not restore early balance
  proposals from chat accidentally.
- Corpses remain until area reload. Dismemberment retains native death behavior;
  detached parts use the terminal death pose, fly a little away and settle flat.
  Eyes must stay with the head. Preserve current cut-direction/region rules;
  nearest-body-part selection alone must not cut a hand from a chest hit.

## Current testing entry points

From the project root:

```cmd
platforms\tools\build-k1.cmd
engage-k1.cmd -StaffPrototype -Sandbox
```

The second command opens the established arena with a staff player and manual
spawning. Omit `-StaffPrototype` for the yellow player. Top-row **0** opens
**Spawn Menu**, with KotOR lettering and a light-blue outer border. It has one
base NPC (Dark Jedi), Yellow/Double-blade/Dual sabers stance arrows, Friendly/Hostile alignment,
AI On/Off and Spawn. Spawn is about3m ahead, facing the player; occupied space is
refused. NPC arrows stay disabled until more base NPC types exist. AI Off leaves
the managed roster idle without automatic Engage. `-Sandbox` suppresses automatic
replacement; ordinary arena launches retain their replacement behavior.

NPC identities/tuning are in `src/game/clientcore/include/k1npctype.h`:

| ID | Stance | Swing playback | Chain limit | Parry chance |
| --- | --- | --- | --- | --- |
| dark-jedi | Yellow |82%|3|25%|
| dark-jedi-staff | Double-blade |92%|4|20%|

Player staff playback is95%, chain4. These are playback rates, not hit chances.
NPC types remain separate for future spawning/content. Other balance details
should be read from current code rather than copied from old chat values.

Runtime code: `src/game/clientcore/k1engage.cpp`; state helpers in its include
directory; spawn UI in `include/k1sandboxpanel.h`. Launch configuration is in
`platforms/tools/launch-k1-engage-prototype.ps1`. Do not refactor the large combat
file merely to prepare packaging.

Active profile: `platforms/out/k1-engage-prototype/elbow-arena/profile`, including
its separate `saves`. This is a development profile, not a release-file list.

## Known issues and evidence worth preserving

- **Font artifacts:** user accepted the first sharpened Spawn Menu font, then
  requested all remaining lettering. All18 font resources were replaced with HD
  glyph atlases. User then reported lines above words in several screens,
  including the main menu, and a lowercase f that looks too thick.
- Diagnosis only: `hd-fonts/glyph-artifacts/audit.json` finds ink just above six
  printable rectangles in fnt_d16x16b. Some uncompressed fonts' HD f coverage is
  heavier than native. These findings do not establish every affected runtime
  font. **No correction was installed.** User paused this work.
- User explicitly said HUD icons, graphics, frames and bars are fine; sharpen
  **lettering only**. HD UI Menu Pack was downloaded/extracted but cancelled and
  **never installed**. Do not deploy it as a font fix.
- HD font source is RaymanGT's KMRP Original HD recreation, SIL OFL1.1, pinned at
  `2497b9df802235c3c162afcbc5bd5db19db23c2f`. Only font assets were used; no KMRP
  patcher/engine code. Recipe: `platforms/tools/prepare-k1-hd-font.py`.
- **Separate NPC-vs-NPC test failure:** full `-SandboxProbe` timed out at phase5,
  `npc_vs_npc_damage`, both before and after the first font replacement. An earlier
  sandbox probe passed. Do not describe the full sandbox as currently verified
  merely because the separate `-FontProbe` passes. No fix was attempted here.
- Last cleanup verification passed normal startup, focused18-font/menu checks,
  and the staff Dark Jedi smoke test across two fights/four native Force casts,
  melee resumption and native victory restoration. Full campaign not established.
- OBS previously made the local hardware cursor disappear. The isolated profile's
  `EnableHardwareMouse=0` workaround uses restored original PC cursor artwork.
  Avoid switching it back casually; actual OBS behavior needs the user's setup.

Evidence is under ignored `platforms/out/k1-engage-prototype/`: start with
`sandbox-menu/INDEX.md`, `hd-fonts/INDEX.md`, `hd-fonts/all-fonts/INDEX.md`,
`hd-fonts/glyph-artifacts/`, and `ownership/pose-lifecycle/INDEX.md` as relevant.

## Cleanup and release context

- Cleanup removed only our retired single-strike demo and redundant generated
  output:20,079 files, about8.77GiB net. It did not touch the Steam game, Rust
  project, upstream features or stance/gameplay behavior. Current177 runtime
  assets were hash-identical. Details: ignored `platforms/out/cleanup-audit/`.
- `preview-k1-strike.cmd`, its PowerShell launcher/header and old preview profile
  are intentionally gone. The old build path under k1-strike-playback is obsolete;
  use the tracked build helper above.
- Do not delete every folder named review/preview. Current generators still read
  approved weights/poses from `staff-swing-review/weights-02`, `stock-reset-01`
  and `yellow-weight-review/dark-02`. Raw frame sequences were trimmed; required
  inputs, movies, authoring scenes and representative frames were retained.
- The user wants a release like the supplied kjk v0.18.2 ZIP: extract, double-click
  an EXE, detect/select owned game installs, prepare data, then play. The ZIP was
  inspected, not executed. It is a UX/packaging reference, not permission to import
  its Rust engine, save editor, co-op or its extra KotOR II requirement.
- Proposed foundation: portable first-run setup, reproducible asset generation,
  separate AppData settings/saves and LocalAppData caches, sanitized logs, an
  explicit package-file list and a repeatable build/test/privacy-scan command.
  Later GitHub automation could prepare draft prereleases for the user to review.
- Main blocker is not making a ZIP: current preparation depends on ignored
  generated game-derived files, historical recipes and a fixture save. A fresh
  source checkout alone does not produce a portable playable arena. Plan owned-
  data preparation and redistribution review instead of shipping the local profile.
- Current development EXE contains a Windows user-directory path and an absolute
  PDB reference. Omitting PDBs, cleaning previews or starting fresh Git history
  does not sanitize that EXE. A separate sanitized build and exact-package audit
  remain unimplemented. Never include private symbols as public release artifacts.
- User is concerned about old commit names/emails. A new repository must use a
  separate reviewed source copy without old `.git`, with a public alias/no-reply
  email. Existing history stays intact here. Keep licenses/credits; include new
  untracked source files needed to compile. Current AGENTS.md itself contains
  machine-specific paths. Instructions: `docs/FRESH_REPOSITORY.md`.

## Next agent's first action

Read the new chat's actual request and the current checkpoint. Do not start the
release pipeline, fix fonts, commit, create a repository or publish just because
those subjects appear in this handover. This handover-writing task is complete;
all future implementation needs the user's next instruction.
