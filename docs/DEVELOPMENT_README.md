# KotOR:CE

KotOR:CE is this project's name. It builds on the K12 C/C++ engine foundation;
upstream credits, licenses and technical identifiers are retained. The upstream
description and platform claims below describe K12, not additional verification
of this local project.

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
For manual NPC testing in the same arena, run
`engage-k1.cmd -StaffPrototype -Sandbox` (omit `-StaffPrototype` for the yellow starting
player stance). Press top-row **0** to open/close Spawn Menu. Select the Dark Jedi's
Yellow or Double-blade stance with the arrows, choose Hostile or Friendly, and
click Spawn. NPC arrows are disabled until more base NPCs are available. NPC AI
On/Off affects all sandbox NPCs; Off leaves them idle without automatic Engage.
Spawns face you about3m ahead; blocked space is refused. Move before spawning
another. This mode starts with an empty arena and replaces automatic enemy
respawning with manual spawns. It retains the existing isolated profile and combat
services. Close/Escape also closes the menu, restoring the previous pause state.
`engage-k1.cmd -StaffPrototype -SandboxProbe` runs the native menu, placement,
faction, NPC-vs-NPC combat, AI-off and area-reload checks.
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
behind the backpack. Strict angular/skin
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
cut or hold left click to chain up to three swings total. After the third swing,
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
Melee guard now has poise:100% initially,20 points lost per ordinary blocked melee hit,
and a2.5-second native stun at zero before recovery to25%. After2 seconds without
swinging or incoming/guard hits, regenerate1 point every0.1 seconds while still,
Shift-walking or guarding. Running and swinging prevent regeneration. Existing
guard-break recovery starts ticking immediately if eligible; further hits or swings
still restart the quiet delay. Enemy melee attacks can chain up to three swings,
using the player's follow-through timing and pose smoothing. Reciprocal melee hits
within0.5 seconds while both fighters swing clash: both chains stop, both fighters
retain their poise, and neither takes health damage from those hits. Ordinary
guard depletion still causes native guard-break stun. A larger horizontal saber replaces the combat-message strip at the
top center, with the percentage centered underneath and scaled for the display. Other melee
weapons use a default blue saber; equipping a saber matches its hilt and crystal
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
(their overhead-left cut needs your upper-right guard) to parry: you regain10
poise points, capped at100%, and lose no HP; the attacker loses25 poise and their chain stops. Wrong-side or
neutral guard retains the ordinary20-point defender cost. An attacker depleted
to zero is stunned through the existing poise system. The dial now appears0.75m
farther out; a parry still needs an actual melee hit. A parried fighter cannot
start another weapon attack for1 world second; movement and guard remain usable.
Attack clicks during recovery are discarded, so release and press again after it.
NPCs use their existing timed-parry window for the same25-point counter-parry
against the player and regain10 poise on success; ordinary raised guard remains a normal block.
The Sith soldier has a20% chance on an eligible timed-parry contact, rolled once
per incoming swing. Per-creature native VarTable integer K1_ENGAGE_PARRY_PERCENT
overrides this from0 to100, so future enemy templates can have different skill levels.
On actual guarded contact, either fighter plays the matching JKA top, upper-left,
upper-right, lower-left or lower-right block reaction, then blends back to guard.
This also applies to successful parries. Walking continues underneath the reaction;
poise-break stun and native powers/items retain priority.
Player melee playback is0.8528,4% faster than the NPC's unchanged0.82. NPC overhead/upper-diagonal/side
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
