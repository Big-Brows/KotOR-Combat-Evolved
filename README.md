# KotOR:CE

KotOR:CE is this project's name. It builds on the K12 C/C++ engine foundation;
upstream credits, licenses and technical identifiers are retained. The upstream
description and platform claims below describe K12, not additional verification
of this local project.

A local compact portable candidate now supports lossless on-demand model storage;
see [portable instructions](docs/PORTABLE.md) for measured size and verification.

## Local wearable stance support

The isolated K1 sandbox supports native male/female wearable armor, robes and
clothing across Yellow, Red, Blue, Purple, Dual and Double-blade stances. The sandbox
player receives all armor/implant prerequisites and native universal proficiency
for class/alignment/feat restrictions. Native
appearance, textures, heads and item stats are retained. Generated owned-data rigs
stay local under `platforms/out/armor-rigs`; the prototype launcher verifies and
deploys them automatically when the finalized manifest is present. See
[STATUS.md](STATUS.md) for coverage and verification, and run
`platforms/tools/launch-k1-engage-prototype.ps1 -ArmorProbe` for the full body/stance check.

In sandbox combat, clashes, blocks and parries use native KotOR sparks and light;
a successful player parry flashes the full dial yellow. Surface burns cover both
saber hands and staff tips, including brief first contacts. Focus loss clears held
movement/guard input, and orphaned sandbox slowdown factors recover automatically
without removing active native slow effects.

Player stances support all normal K1 melee categories, including stun batons,
Gaffi sticks, quarterstaves and Gamorrean battleaxes. Batons defend by parrying;
ordinary held guards admit damage. Custom-combat Force casts survive plain damage,
queued powers/abilities avoid the spent-round wait, and mines work through the
action bar while retaining native placement rules.

## K12 foundation

Byte-matching decompilation of the Android ports of the first and second Knights of the Unc Representative Democracy games.

## Platform Status

All platforms work with original assets, as well as the Android OBBs. The desktop builds find an installed Steam/GOG/disc copy on their own and share its saves (they can also take environment variables `K2_ASSET_DIR=<game folder>` / `K2_USER_DIR=<saves folder>`).

To play using the OBBs instead, set `K2_OBB_DIR=<folder with the OBBs>` (both games pick its own `main.*` / `patch.*` files by package name, so one folder can hold K1's and K2's), and run from a writable folder (or `K2_DATA_DIR=<dir>`) where the ini and saves will go. Android reads the OBBs from the usual `Android/obb/<package>/` folder, iOS from the app's Documents folder.

| Configuration | Graphics | Testing Status |
| --- | --- | --- |
| ARM64 Android | OpenGL ES 2.0 + gl4es | K2 has been tested with TSLRCM from start to credits. K1 has been completed through Dantooine. |
| 64-bit Windows (MinGW Cross) | OpenGL 2.1 (compatibility context) | Only basic manual tests performed. |
| 64-bit Windows (VS2022) | OpenGL 2.1 (compatibility context) | Automated map rotation burn-in tests with ASan; K1 has been completed through Dantooine, K2 has not been tested beyond early portions of Peragus, minigames, and some save load smell tests. |
| MacOS x86_64/AArch64 | OpenGL 2.1 (legacy context) | Primarily used for regression tests and 4K PC UI tests. |
| 64-bit Linux | OpenGL 2.1 (compatibility context) | Only basic tests performed. |
| ARM64 iOS | OpenGL ES 2.0 + gl4es | Only basic tests performed. |

### Original Assets

You can purchase the games from one of these links. You **must** own a valid license for the game you are compiling for, no assets are provided.

**K1**:
 - [Steam](https://store.steampowered.com/app/32370/STAR_WARS_Knights_of_the_Old_Republic/)
 - [GOG](https://www.gog.com/en/game/star_wars_knights_of_the_old_republic)
 - [Google Play](https://play.google.com/store/apps/details?id=com.aspyr.swkotor)

**K2**:
 - [Steam](https://store.steampowered.com/app/208580/STAR_WARS_Knights_of_the_Old_Republic_II__The_Sith_Lords/)
 - [GOG](https://www.gog.com/en/game/star_wars_knights_of_the_old_republic_ii_the_sith_lords)
 - [Google Play](https://play.google.com/store/apps/details?id=com.aspyr.swkotorii)

The final binary requires one of the following:
 * an installed PC/Mac copy (Steam, GOG, disc). The desktop builds find it on their own, or take `K2_ASSET_DIR=<folder with chitin.key>`.
 * the Android OBB files: K2's `main.213.com.aspyr.swkotorii.obb` + `patch.14.com.aspyr.swkotorii.obb`, or K1's `main.53.com.aspyr.swkotor.obb` + `patch.53.com.aspyr.swkotor.obb`. These are required for the touch UI, and on Android and iOS. The desktop builds take the folder they are in as `K2_OBB_DIR=<folder>`.
 * K1's touch UI also needs the original K1 APK's `assets/iosdialog.otf`.

## K1/K2 interoperation

Both games build from the one `src/` tree, which is K2's engine. Wherever K1 differs (structs, signatures, whole function bodies, K1-only functions) the code branches on `#if KOTOR1`.

For matching, `tools/cc.sh` with `EXTRA_CFLAGS=-DKOTOR1` builds the K1 objects with K1 CFLAGS and `tools/k1/k1match.py --all --build` scores them against `libKOTOR.so`.

## Restored Engine Features

Things the Android builds cut or never had, brought back from the PC, Xbox or Switch versions:

- **PC UI** (`K2P_UI=pc`): the PC layouts and art from the game archives instead of the touch re-layouts, the PC HUD with its menu bar and per-box arrows, the PC main menu with Quit, the PC conversation and computer screens, mouse hover, hardware `.cur` cursors and right-drag mouse look.
- **Xbox UI** (`K2P_UI=xbox`): the console HUD and action bar, the console Pazaak and swoop-upgrade screens and the console name keyboard. Some creative liberties were taken, such as saves still having named slots.
- **Controller rumble**: `rumble.2da` is loaded and drives SDL rumble or haptics (or the phone's vibrator on Android).
- **Framebuffer effects for K1**: the PC's Force Push / stealth distortion and Force Speed motion blur, all cut on the Android port.
- **Soft shadows** (both games): the PC's image-space soft-shadow pass, also cut on both Android ports.
- **Anti-aliasing**: uses SSAA, MSAA/other AA is not implemented currently.
- **Mobile optimizations**: Optimizations specific to the mobile ports (size-over-distance culls past 15m, dangly mesh physics removal, forced 720p, reduced particle birthrates, reduced draw distances and added fog) are behind ifdefs.
- **Display options**: Screen Resolution and a Fullscreen setting (native resolution, notch-aware on macOS) on the desktop.
- **K1 Achievements on Desktop**: Shows the Google Play/Game Center achievements on desktop K1 as a corner bark bubble.

New additions:

- **Controller options page**: button prompts (Automatic / Xbox / PlayStation / Switch / GameCube), Swap A/B and X/Y, Disable Right Stick Pitch (K2), Faster Movement (K1 Android's 1.3x run speed), and Achievements on/off (K1).
- **Text Size** (K2): 100%-250% for conversations, menus and the HUD text.
- **Steam integration**: finds Steam/GOG installs and shares their saves, uses K2's Steam Cloud saves folder, mounts K2 Steam Workshop mods, and reports K2 achievements to Steam (local achievements otherwise).
- **HUD scaling**: target reticles and area-map markers scale with the screen.

## Replaced Dependencies

- **FMOD** -> [miniaudio](https://github.com/mackron/miniaudio) 0.11.25 (Unlicense or MIT-0), behind the engine's FMOD API (`platforms/audio/`). Steam Audio is planned for occlusion/HRTF, not integrated yet.
- **RAD Bink** -> k2bink, a C99 port of [unbikit](https://github.com/blennies/unbikit) (MIT or Apache-2.0)
- **gles2-bc** -> [gl4es](https://github.com/ptitSeb/gl4es) (MIT) on Android and iOS; the desktops use OpenGL 2.1.
- **JNI stuff, Google Play, Firebase and Crashlytics** -> rewritten
- **Steamworks SDK** -> none: a `steam_api` library is used if one sits beside the game, otherwise the running Steam client's `steamclient` is loaded directly.

## Building

See [BUILDING.md](BUILDING.md)

For this checkout's configured Windows K1 test build, run `launch-k1.cmd` from Command Prompt
or double-click it in Explorer. It uses the existing isolated profile under
`platforms/out/k1-test-user`; the build and profile must already exist. The launcher uses Windows
PowerShell internally with a process-only execution-policy override. Use `launch-k1.cmd -Smoke`
for the bounded hidden startup check. See [STATUS.md](STATUS.md) for the local baseline and checks.

For OBS capture issues with an invisible local pointer, K1 PC mode supports
`EnableHardwareMouse=0` under `[Graphics Options]` in the isolated profile's
`swkotor.ini`. The software cursor now reuses the original PC bitmap artwork and
click hotspots when available. `engage-k1.cmd -CursorProbe` runs the bounded
cursor pixel/scaling check in the existing isolated profile. Actual OBS capture
still needs a visual check on the user's setup.

The retired single-strike main-menu demonstration and its launcher have been
removed. The current combat work uses `engage-k1.cmd` and the existing arena.
Build the configured Windows K1 target with `platforms\tools\build-k1.cmd`.

For the local **dual-lightsaber player prototype**, double-click
`play-dual-k1.cmd` (equivalent to `engage-k1.cmd -DualPrototype`). It opens the
existing arena with two equipped native sabers and the JKA dual animation body.
Press Space to enter Engage; use WASD for movement/attack direction, LMB to
attack (hold to chain), RMB to guard and Shift to walk. Add `-Sandbox` for the
empty arena and top-row 0 spawn menu, including dual-saber NPCs.
The dual stance is selected while both saber hands are equipped in this launch
mode or the existing sandbox. Current yellow playback rate/three-swing cap are provisional shared policy;
each hand can contact once per swing using its own native weapon damage.
Right-click uses KotOR's native dual guard. Unguarded walking/running reuse the
established full locomotion and run refinement; guard-walking retains the normal
walk below the native guard. Walking spine rotation stays facing the standing
reference while the legs and arms animate. Dual attacks bypass the single-hilt
arm filter; the top cut brings the main blade inward toward the other blade's
contact lane. Both held sabers appear in stacked poise
meters, with the offhand directly below the main row and a shared depletion value.
This local prototype is separate from the portable package. Animation appearance
and gameplay feel need user testing; see STATUS.md for automated verification.
The launcher verifies the isolated save alias and component hashes before copying
the generated dual bodies into the writable arena profile under its exclusive lock. Existing local
retarget tooling/data are required to regenerate it with
`platforms/tools/prepare-k1-dual-prototype.py`; add `-- --npc` to that Blender
invocation to generate the separate Dark Jedi dual body/template. Game assets
remain untracked. All supported melee weapons now use basic timed attack deflection: incoming
melee contact during the defender's first0.5 seconds of an active swing clashes
immediately. There is no reciprocal-hit waiting interval.
For response troubleshooting, launch `play-dual-k1.cmd -Sandbox -ContactTiming`;
the isolated profile log records contact-to-health and guard/block response times.
`engage-k1.cmd -DualPlayerParryProbe` checks the 12 player parry/block cases.

In KotOR mode, A/D move sideways and Z/C turn the camera. The arena launcher
applies this layout to its isolated profile; Engage keeps its existing controls.

Head presentation uses the existing animations and native behavior. Custom
crosshair/target tracking and forward-head corrections have been removed.
For local Blender editing, `platforms/tools/export-k1-animation-review.py`
exports the current stance and native animation sources to ignored
`platforms/out/animation-head-review`, with source resources and a clip index.

The completed yellow, double-blade and dual stances keep their established walking, including Shift and
guard-walk, and use KotOR's native running on the existing rigs (runSS for single
blades, runST for staff, runds for duals). Backward running reverses the
native cycle. The run's root bounce continues beneath moving swings. Directional
leg turns, movement speeds and attack/guard layers remain active.
Double-blade uses the native `runST` family and dual uses `runds`, including NPCs;
yellow retains the accepted standard `run`.
Dual guard entry/release and moving grips use the existing pose blend for players
and NPCs. Use `engage-k1.cmd -PresentationProbe` to check these transitions,
and two native quit-to-menu cycles with a save reload between them.
Use `engage-k1.cmd -Sandbox` to try yellow, or
`engage-k1.cmd -YellowNativeGaitProbe` for the bounded 120-case all-stance walk/run,
running-swing bounce, direction, reverse-loop and upper-layer checks.
For a direct comparison, `engage-k1.cmd -Sandbox -YellowNativeGaitBaseline`
uses the previous JKA running for that launch only.

For the red single-saber stance, run `play-red-k1.cmd` (or
`engage-k1.cmd -RedPrototype`). In the sandbox, click the **middle mouse button** to cycle yellow/red
while a single saber is equipped. Double-blade and dual equipment
retain their own stance; the single-saber choice returns when re-equipped.
Holding guard while middle-clicking keeps guard raised and smoothly blends
between yellow and red, including reversals during the blend.
The authored Red Dark Jedi remains available in regression fixtures. Red uses strong guard
and seven directional attacks. Chained cuts use JKA's red T3 links selected
from the outgoing/incoming directions, with direct strikes when their quadrants
match. Red retains the local three-swing cap and excludes link time from damage.
It uses its own contact windows, yellow's
existing gameplay services/tuning, and the same native KotOR forward/reverse
running, including bounce beneath attacks. `engage-k1.cmd -RedProbe` runs the
instrumented red checks. Generated resources stay in ignored local output;
prepare player and NPC variants with `platforms/tools/prepare-k1-red-prototype.py`
using the existing Blender/owned-JKA workflow before first use.
For the local portable Windows arena candidate, double-click **KotOR-CE.exe**
in the extracted package. It detects owned Steam/GOG KotOR 1 and Jedi Academy
installations, prepares the current arena data and keeps settings/saves beside
the launcher. See [docs/PORTABLE.md](docs/PORTABLE.md) for build, verification
and data-version limitations. Font fixes remain paused.

For the double-bladed stance under development, run `engage-k1.cmd -StaffPrototype`.
The test arena gives you both a single-bladed and a double-bladed lightsaber.
Equip either through the equipment menu to select its matching stance automatically.
The existing outfit and face are preserved. `-StaffPrototype` selects the starting
weapon; both weapons and stances are available in either arena launch mode.
Run `engage-k1.cmd -SaberSwitchProbe` for four native equipment-menu weapon/stance
switches, unique inventory ownership and appearance checks; add `-StaffPrototype`
to test a double-blade starting loadout.
To use the NPC combat rules in the campaign, run `play-story-k1.cmd` (or
`engage-k1.cmd -Story`). It opens native New Game/Load menus in the separate Engage
save profile, without an arena transfer or arena loadout. Original saves are not
imported or overwritten. Supported NPCs share the sandbox HP-based poise/parry
rules and configured variant stances; native stats, items, quests and unsupported
combat states remain native. This path has a focused Endar Spire combat check;
a full campaign playthrough is not claimed. See [STATUS.md](STATUS.md).

For manual NPC testing in the same arena, run
`engage-k1.cmd -StaffPrototype -Sandbox` (omit `-StaffPrototype` for the yellow starting
player stance). Press top-row **0** to open/close Spawn Menu. Choose a faction
in the left list and an NPC type in the middle list. The right column provides
variant and appearance arrows, a full-body preview, native stats plus Max Poise
and Parry Chance, Hostile/Friendly
alignment and Spawn. Entries group original NPC names by weapon, with numbered
native stat variants and shared cosmetic appearances. Spawned target names omit
the weapon suffix. Lightsabers offer colour and compatible stance controls;
sandbox enemies can die and reopening preserves menu state. Armed unnamed
Wookiees and Gamorreans have dedicated categories, including Wookiee Thug under
Wookiee. Selkath, all other unarmed NPCs,
named characters, creatures and droids are excluded. NPC AI
On/Off affects all sandbox NPCs; Off leaves them idle without automatic Engage.
**TARGET PLAYER: No** globally excludes the player while allowing opposing NPC
teams to fight; **Yes** restores normal targeting. The menu has no Native Faction
option. Lists scroll when needed. Manual naming and grouping details are in
[docs/SANDBOX_SPAWN_MENU.md](docs/SANDBOX_SPAWN_MENU.md).
Spawns face you about3m ahead; blocked space is refused. Move before spawning
another. Dismemberment uses the existing fatal powered-lightsaber rules across the
implemented NPC roster. Tusken legs cannot be severed; their other regions remain
enabled. Cut assets are prepared with `platforms/tools/prepare-k1-npc-dismemberment.py` through the
existing Blender workflow, then verified/deployed automatically in Sandbox.
This mode starts with an empty arena and replaces automatic enemy
respawning with manual spawns. It retains the existing isolated profile and combat
services. Close/Escape also closes the menu, restoring the previous pause state.
`engage-k1.cmd -StaffPrototype -SandboxProbe` runs the native menu, placement,
faction, NPC-vs-NPC combat, AI-off and area-reload checks.
`engage-k1.cmd -NativeNpcProbe` checks the installed native catalog, body/head
restoration, character data, weapon geometry and an autonomous melee encounter.
Preparation and coverage limits are in [docs/MELEE_NPC_COVERAGE.md](docs/MELEE_NPC_COVERAGE.md).
The sandbox inventory includes a second single saber. Equip a single saber in
each hand through the native equipment menu to select dual; equipping the
double-blade selects staff. `play-dual-k1.cmd -Sandbox` starts with dual equipped.
`engage-k1.cmd -DualSandboxProbe` checks native equipment transitions, dual NPC
spawning/AI, both native weapon damage effects, Force casting, recovery and cleanup.
The local test profile has HD lettering for all18 original font resources,
including small-text, dialogue, menu and credits variants. It repacks RaymanGT's
KMRP Original HD glyphs (SIL OFL1.1), retaining native text sizes and spacing.
Two legacy atlases need separate glyph rectangles to avoid their shared edge
gutter; measured rounded ASCII widths remain unchanged at tested display scales.
HUD artwork/icons/borders are unchanged. No KMRP patcher or HD UI Menu Pack is
installed. `engage-k1.cmd -StaffPrototype -FontProbe` runs the focused native
font image/metadata and menu checks, independently of the full combat probe.
The local recipe is `platforms/tools/prepare-k1-hd-font.py --help` (`--resource`
selects a font). Generated assets, license and provenance remain in ignored
`platforms/out/k1-engage-prototype/hd-fonts/all-fonts` and its parent folder.
To fight the separate double-bladed Dark Jedi type, run
`engage-k1.cmd -StaffPrototype -NpcType dark-jedi-staff`.
Omit `-StaffPrototype` to use the yellow player stance against that NPC.
The existing yellow Dark Jedi remains the default `-NpcType dark-jedi`.
Both types use the same arena, respawn, Force and combat services. The staff NPC
uses92% swing playback, four-swing chains and20% parries; the yellow NPC uses
82%/three and25% parries, with85% chain continuation and250ms post-chain rest.
Staff retains100% eligible continuation and150ms rest. It equips the original
Dark Jedi red double-saber model; staff poise tuning matches the player's.
`engage-k1.cmd -NpcType dark-jedi-staff -Smoke` runs the installed NPC checks.
It uses the established Dark Jedi arena/profile, including active AI, powers,
poise/parries, damage and dismemberment. Staff chaining remains a reported open
issue. The new player resource is
`k1jastaff`; the completed single-blade resource and stance tuning remain intact.
Walking/running reuse the completed single-blade animation tracks. Shift-walking
uses the ordinary full-body walk. Hold RMB for KotOR's side-held guard; moving
guard keeps that same lower-body walk under the double-blade upper guard.
Running/attacks may release hands. All seven directional attacks now use staff S7/A7/R7 clips, with five P7 block
reactions for ordinary guards and parries. Attacks now retain the original JKA
source motions shown in the accepted preview, with waist/chest skin weights
adjusted only on the staff body. The staff body also closes a verified opening
behind the backpack. The same panel closure now covers all player stances;
original guard and attack animations are preserved. Strict angular/skin
diagnostics still have failures; visual acceptance is pending. Both native blades
feed the swept-contact and KotOR damage services. Yellow/player/NPC melee contact
includes follow-through, excluding windup and late recovery. Staff W+D/D retain
the existing120ms contact follow-through.
`engage-k1.cmd -StaffArcProbe` records21 real-camera attacks across standing,
Shift-walking and running. It does not establish visual acceptance.
`engage-k1.cmd -StaffContactProbe` checks both actual blades, timing rejection,
per-blade guard contacts, one health hit per swing, four-swing staff chaining,
staff recovery timing, equipment eligibility and native damage in the same arena.
`engage-k1.cmd -StaffForceContactProbe` adds actual choke, stasis, whirlwind and
enemy-casting contact/damage checks, including completed-spell ownership handoff.
`engage-k1.cmd -StaffProbe` checks pose/grip and locomotion preservation;
`engage-k1.cmd -StaffPrototype -Smoke` runs the existing Dark Jedi integration test.
The preparation recipe adds only generated staff assets to the existing override:
`platforms/tools/prepare-k1-staff-prototype.py`. It does not clone scenes/saves/config.
`platforms/tools/check-k1-staff-skin.py` runs surface diagnostics (Blender background).

The separate Blender block editor is prepared by
`platforms/tools/prepare-k1-staff-editor.py` using the installed Blender in
background mode. Open the ignored output
`platforms/out/k1-engage-prototype/staff-editor/kotor_staff_blocks.blend` and choose
the KotOR side guard or one of five blocks in the Action Editor. Textures are
packed; editing needs no installed add-on or script execution. The builder refuses
to overwrite an existing editor file. These are base block clips; live aim/spine/
grip transitions are excluded, and exporting edits into the game is not connected.
For all seven current staff attacks, run
`platforms/tools/prepare-k1-staff-attacks-editor.py` in Blender background mode.
It copies the existing editor into the separate ignored
`staff-editor/attacks/kotor_staff_attacks.blend`, retaining guard/block Actions.
Choose attack Actions10..16; W uses frames1..115, other attacks1..109 at120fps.
Windup/strike/recovery markers identify each phase. The source editor is untouched.

Archived preview-only swing revisions are under the ignored
`platforms/out/k1-engage-prototype/staff-swing-review/candidate-05/review.html`.
The player offers seven labelled current/proposed standing and forward-walk
comparisons, plus an editable review scene. Those added-motion proposals were not
deployed; they add0.18 source seconds of finishing motion. The current installed
attacks instead match the original-JKA preview under `staff-swing-review/weights-02`,
with its approved staff-only weight distribution. Yellow/Dark Jedi weight trials
remain preview-only. See STATUS.md for current native verification and visual limits.
Preparation/render tools are `prepare-k1-staff-swing-review.py` and
`render-k1-staff-swing-review.py` under `platforms/tools`; an explicit
`K1_STAFF_REVIEW_ATTEMPT` selects a new ignored review folder without overwriting.

The animation candidate still has skin and angular diagnostic failures. Use the
isolated arena for gameplay-distance visual review; see STATUS.md for measured
results and remaining issues.

For the current local animation candidate, run `engage-k1.cmd -Arena -AnatomicalRig` from Command Prompt in this
checkout. It loads a cloned test character into a separate Taris arena module and profile;
the installed game and regular saves are not writable destinations. Click Engage, use
WASD with left click for directional saber swings. Tap again near the end of the
cut or hold left click to chain up to three swings total for Yellow/Red/duals,
or four for Blue/Purple/staff. After reaching the stance's cap,
release and press again once the character recovers. Hold right click to guard
and force walking. Shift still walks. Space pauses and restores the planning cursor; use Engage to
resume. Escape/menu return stays paused until Engage. Disengage appears only after the
opponent loses sight and stops pursuing; defeating the combat participants automatically
restores exploration controls. No jump or
crouch. Active movement uses grounded JKA-style controls and retargeted JKA locomotion.
While standing still, the character returns to KotOR's neutral idle after a swing or
when guard is released. Full JKA combat and enemy/companion AI integration remain
unfinished. This prototype is calibrated for the arena's
PMBBL body and a standard single saber. See STATUS.md for verified
checks and limitations; docs/DESIGN.md describes the combat boundaries.
The candidate limits additional movement-related leg turn to45 degrees, corrects
guard posture to the source JKA torso direction, and lightly smooths Engage arm
poses while preserving hand contact. R/default action keeps an already-active
Engage body intact; native weapon flourishes are suppressed during Engage.
Engage saber contact uses the displayed central blade against animated body/head
geometry during attack hit windows. Active Engage maintains saber power without
requiring an enemy hit. Remaining arm/skin issues still need visual review.
Powered lightsabers also leave the original JKA orange glow/scorch on contacted
floors, walls and props, independently of attack damage. The launcher requires
your owned Steam JKA base/assets0.pk3 and assets1.pk3 and prepares the original
textures and quiet contact sounds in the ignored test profile. Swords and
unpowered sabers create neither new scars nor scorching cues. Glow fades over1.5
game seconds; the darker mark lasts10.
An experimental source-elbow pose test uses `engage-k1.cmd -Arena -AnatomicalRig -ElbowRig`
with its own profile and test saves. It retains known skin/animation failures and
awaits in-game visual review; the default prototype remains available.
Add `-AimPitch` to that command to test camera pitch driving the Engage pose.
Add `-EnemyAI` to test a Dark Jedi with native Force powers, player-style idle/gait/guard refinements
and the source-ported JKA grounded navigator/steering/movement. The full review command is
`engage-k1.cmd -Arena -AnatomicalRig -ElbowRig -AimPitch -EnemyAI`.
Add `-Gunner` for a pistol or `-Gunner -Rifle` for a rifle. This remains an isolated,
body-specific prototype; the combat AI source port is unfinished. The existing
generated Dark Jedi rig/template must be present in this ignored profile. Gunner
and legacy smoke fixtures retain the soldier rig. KotOR supplies map
and collision data; owned movement uses the imported Raven A* graph, ported JKA
steering and grounded slide movement, with no native AI path-executor fallback.
The arena graph is prepared during area loading and reused across Engage changes;
area unload removes it. Unsupported query origins are rejected before walkmesh
adjacency access, addressing the traced Engage crash. Loading takes roughly three
additional seconds on the current test machine; combat updates do not rebuild it.
Engage melee NPCs walk while attacking and between nearby swings; normal pursuit
can run again once the swing finishes and the target pulls away. The walking area
includes an extra0.75m to accommodate backing away without immediately triggering a run.
Player and supported Engage NPCs use the player's native KotOR base walk/run
rates (3.2/5.4m/s in the arena), with each actor's own movement effects.
Force powers, grenades, mines, healing and status effects use KotOR's native
animations and original bodies for both sides, then return to Engage. JKA
animation playback is reserved for movement and weapon combat.
The Dark Jedi alternates Choke and Force Push between melee attacks, and selects
Drain Life when below half health. A local eight-second casting cadence uses
native known powers, Force-point costs, saves and effects; it does not grant
unlimited Force. Each replacement is a fresh Dark Jedi. Add `-DarkJediProbe -Smoke`
to the full review command for the isolated autonomous casting/recovery check.
Melee guard now has poise:100% initially, with stance-specific damage on blocks,
and a2.5-second native stun at zero before recovery to25%. For Yellow, after2 seconds without
swinging or incoming/guard hits, regenerate1 point every0.1 seconds while still,
Shift-walking or guarding. Running and swinging prevent regeneration. Existing
guard-break recovery starts ticking immediately if eligible; further hits or swings
still restart the quiet delay. Enemy Yellow/Red/dual attacks can chain up to three
swings; Blue/staff up to four,
using the player's follow-through timing and pose smoothing. An incoming melee
hit deflects immediately if the defender is still swinging and started that swing
within the last0.5 seconds (including the endpoint). Every actual chained swing
opens a new eligibility window. No facing, matching direction, blade interception,
fresh-press or separate one-use requirement is added. Both chains stop and the
deflection deals no health damage. Outside eligibility, hits enter normal
guard/damage handling in the same scene update, without waiting. Each Red participant
deals10 poise points to its opponent once per resolved clash. Ordinary
guard depletion still causes native guard-break stun. A larger horizontal saber replaces the combat-message strip at the
top center, with the percentage centered underneath and scaled for the display.
Swords display their actual equipped models, with the blade draining toward the
hilt; double swords drain both ends toward the central grip. Equipping a saber matches its hilt and crystal
color. Only the blade drains from the right; the hilt remains at zero. Ranged/unarmed actors
have no poise. Existing supported Engage body/weapon limits still apply.
Engage shows a center dot. Near a visible melee enemy, a compact four-section
dial shows your aim quadrant relative to their upper body. Its targeting area extends
45% of the screen height from the crosshair, so looking near the enemy is enough. While holding
right-click guard, the selected section uses KotOR's yellow option highlight.
Both upper sections cover overhead; upper-right also covers overhead-left and
horizontal-left, and upper-left covers overhead-right and horizontal-right.
Lower-left covers rising up-right and lower-right covers rising up-left;
both lower coverage rules include a straight strike from below.
Match the incoming cut from your perspective
(their overhead-left cut needs your upper-right guard) to parry: you regain
stance-specific poise, capped at100%, and lose no HP; the attacker loses their
stance's parried-poise penalty and their chain stops. Wrong-side or neutral guard
retains the incoming attacker's ordinary block-poise cost. An attacker depleted
to zero is stunned through the existing poise system. The dial now appears0.75m
farther out; a parry still needs an actual melee hit. A parried fighter cannot
start another weapon attack for1 world second; movement and guard remain usable.
Attack clicks during recovery are discarded, so release and press again after it.
NPCs use their existing timed-parry window with the same stance-aware penalties
and rewards; ordinary raised guard remains a normal block.
The Sith soldier has a20% chance on an eligible timed-parry contact, rolled once
per incoming swing. Per-creature native VarTable integer K1_ENGAGE_PARRY_PERCENT
overrides this from0 to100, so future enemy templates can have different skill levels.
On actual guarded contact, either fighter plays the matching JKA top, upper-left,
upper-right, lower-left or lower-right block reaction, then blends back to guard.
This also applies to successful parries. Walking continues underneath the reaction;
poise-break stun and native powers/items retain priority.
Player melee playback is Yellow/Red1.0, Blue1.15, Purple1.30, staff1.10 and duals0.85.
NPC playback is Yellow/staff0.85, Red/Blue1.0 and duals0.75. NPC overhead/upper-diagonal/side
choices have weights4/3/3/3/3 versus1/1 for rising cuts: upper attacks total55.6%,
horizontal33.3% and rising11.1%. Native KotOR weapon sounds now accompany each
actual swing, guarded/parried contact, simultaneous-hit clash and body hit.
Interactive arena sessions spawn a fresh opponent about two seconds after victory,
then return to planning. Press Space or Engage to begin the next fight. Replacements
keep the selected saber/pistol/rifle profile; defeated opponents leave combat, but
their bodies remain until the area is left and reloaded. K1 desktop corpses also
stay past the original body-count limit; leaving the area releases those models.
GPL source/provenance is preserved under `third_party/openjk/`. See STATUS.md for
the source decision slices implemented, tests, and remaining behavior/visual limits.
Engage's level camera height uses native KotOR settings with a 0.25m lower framing trim.
Live source compensation now resolves reference controllers before first playback.
This improves moving swings; remaining directional offsets await visual review. See STATUS.md.
See [STATUS.md](STATUS.md) for export instructions and verification limits.
In the custom combat mode, thrown grenades use a single flight and detonate at
the selected landing point. Combat NPC movement dispatches native mine entry
and crossing events; native faction and mine-immunity rules still apply.

Guard can replace saber recovery after the active cut ends. Breaking an opponent's
poise resets your chain counter, allowing three subsequent swings. After the last
enemy dies, the current swing finishes before KotOR's native controller returns;
held guard or queued chaining does not shorten that victory follow-through. In the opt-in
cut preview, vertical/diagonal swings permit arm cuts and horizontal swings permit
hand/waist/head cuts, subject to existing probabilities and an actual killing hit
in that part's local region. Detached parts immediately freeze at the matching
KotOR death clip's final pose, throw outward and turn slightly along the ground
plane without added tilt; eyes stay with
the head.

The dismemberment preview preserves immediate damage and rolls once on the
killing contact, without later corpse-cut retries. See STATUS.md for tests. Launch:

```cmd
engage-k1.cmd -Arena -AnatomicalRig -ElbowRig -AimPitch -EnemyAI -Dismember
```

This uses original JKA cut textures and part probability thresholds while
keeping KotOR damage and death animations. Current prepared bodies are the test
player, Dark Jedi and soldier; generated resources stay in the ignored profile.
All supported player/Dark Jedi/soldier parts now use nominal90% cut chances,
including head, waist and legs. Broader local regions include the native torso
mesh's shoulder and leg skin. Actual local killing contact and swing restrictions
remain required; this is not a90% chance on every kill.
Detached parts/corpses remain until area reload; revival restores a whole body.
Visual acceptance is pending. Omitting -Dismember keeps the existing behavior.
See STATUS.md for preparation, isolated-save verification and test limits.

## Contributing

The only contributions accepted are playtesting, issue submissions, and matching improvements; monetary contributions cannot be accepted.

## Progress

`python3 tools/progress.py`

```
engine          : 14,337 funcs / 5,009,668 bytes  (of 20,881/6,121,816 raw dynsym)
transcribed     : 13990 funcs / 4,960,228 bytes  (99.0131% of engine)
exact-matched   : 12187 funcs / 2,667,008 bytes  (53.2372% of engine)  <- COVERAGE
match rate      : 12187/13990 funcs (87.1%), 2,667,008/4,960,228 bytes (53.8%)  <- of what we transcribed
```

Blue stance prototype: run play-blue-k1.cmd after generating the separate player
and NPC blue components with platforms/tools/prepare-k1-blue-prototype.py using
the existing Blender workflow (add -- --npc for the NPC export). In the sandbox,
middle mouse cycles the installed single-blade Yellow, Red, Blue and Purple stances;
the authored Blue Dark Jedi remains in regression fixtures. Completed stance assets and tuning are retained. In-game visual
acceptance remains pending user testing; see STATUS.md for verification details.

Blue and Red's JKA spinning chain links use their authored full-body animations while
movement physics continues. Nonspinning links retain the ordinary moving upper
layer. Purple inherits Red's spin handling. In-game verification remains user-owned.

Custom Purple player stance: run `play-purple-k1.cmd`. It uses the saved
`purple_idle_guard.blend` and `purple_guard_walking.blend` poses with Red's attacks
and chaining. Generate its separate assets with
`platforms/tools/prepare-k1-purple-prototype.py` through the existing Blender
workflow. Purple is included in middle-mouse cycling when its assets are installed.
The source Blender files and their original key timing are preserved. Build and
offline verification passed; in-game testing remains user-owned.

Current stance poise tuning applies to the player and matching NPC styles.
Block damage is determined by the attacker; parried loss and parry gain use
each participant's own stance. Every regeneration tick restores one point.

| Stance | Block damage | Parried loss | Parry gain | Regen delay | Tick interval |
| --- | ---: | ---: | ---: | ---: | ---: |
| Yellow | 20 | 25 | 20 | 2 s | 0.1 s |
| Red | 35 | 20 | 15 | 2 s | 0.2 s |
| Blue | 15 | 15 | 15 | 1 s | 0.1 s |
| Purple | 25 | 25 | 25 | 2 s | 0.1 s |
| Staff | 11 | 35 | 25 | 2.5 s | 0.5 s |
| Duals | 13 | 20 | 15 | 3 s | 0.5 s |

The player poise percentage uses yellow/red/blue/purple text for the corresponding
single-blade stance. Staff and duals retain the default text color. Player attack
playback is Yellow/Red100%, Blue115%, Purple130%, staff110% and duals85%. NPC playback
is Yellow/staff85%, Red/Blue100% and duals75%. Blue/Purple/staff allow four chained
swings and Yellow/Red/duals allow three. A resolved clash inflicts10 poise points
per Red participant on its opponent, with no extra health damage.

Native sword combat is available in the sandbox. Equip long/short/vibro swords
through the regular equipment menu; seven test swords are included in its
inventory, including two long swords for dual wielding. Single swords cycle
Yellow/Red/Blue/Purple with middle mouse. Paired single weapons use duals and
double-bladed swords use the double-blade stance. Each hand's actual model appears
in the poise meter; at0% its hilt remains visible. Weapons retain native damage
and equipment rules. Build/offline checks passed; in-game testing is user-owned.

Sword attachment roll now aligns the blade flats in Engage poses without changing
the authored guard. Sword meter models show their flat sides, with framing for
the full blade and crossguard. Stance swaps rebind the camera to the replacement
body while preserving the displayed view.

Yellow single swords also use the player's selectively edited handle rotations
for idle guard, guard walking and all seven attacks. Original intermediate keys
and timing are preserved. The changes apply through the sword attachment service
to the player and supported Yellow NPCs, independently of lightsaber poses.
Sword edge alignment also covers Red, Blue, double-blade and both dual hands,
including Red/Blue full-body chain clips. It aligns the edge with the attack's
vertical, horizontal or diagonal plane while preserving shaft direction and grip.
The corrected guard-released idle facing continues through walking/backwalking;
running and preview orientations retain their existing behavior. Purple receives
the Red attack correction only; all its non-attacking orientations stay as authored.
The A and both backward-diagonal cuts use the opposite blade facing established
while tuning Yellow. Yellow's saved hook edits remain exclusive to Yellow.

Solid-sword poise meters scale the entire weapon: double-bladed swords120%,
single swords and both dual rows90% of their previous presentation. Lightsaber
meter sizing and live weapon models are unchanged.

Sword-equipped animation editing copies are generated offline by
`platforms/tools/export-k1-sword-stance-pack.py` using installed Blender. The
ignored `platforms/out/sword-stance-edit-pack` contains six stance files with
idle guard, guard walking and full-body attack actions, including all49 chain
links for Red, Blue and Purple. Select actions in Blender's Action Editor;
consult the accompanying editing instructions for frame ranges. Native headhook
and authored controller times are retained; original editors/assets are preserved.
All actions share the armature's slot identity so the Action Editor dropdown
assigns animation correctly. For an older pack, select its Action Slot manually
or use the repaired copies in the pack's `slot-fixed` directory. The offline
`platforms/tools/repair-k1-sword-action-slots.py` preserves existing editor files.
