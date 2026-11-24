# KotOR:CE local modifications

Modification date: 2026-11-16. Prepared for the public alias Big-Brows.
Foundation: K12 commit 1308a613dc8ff5631ebed0dfe202c0793d0e5374.
Original authorship, copyright statements and component licenses are retained.

Local work adds the isolated Taris combat sandbox, directional single/staff
stances, native NPC/utility services, spawn controls, owned-data portable launcher,
privacy build and test/packaging tools. The latest correction uses native RunEquip
for inventory/equipment transfers and grants fixture saber proficiency before
equipping. This fixes duplicate backpack/equipment ownership and the native-menu
single-saber crash. Four alternating native-menu swaps from both starting sabers
verify eight unique-location checks, stance rigs, outfit/head and native restore.

Publication preparation keeps the latest gameplay code and adds neutral path
examples/diagnostics, no private symbols/history, a neutral seed-player name,
selected license notices, independent build instructions and an external Microsoft
runtime prerequisite. The Windows privacy configuration omits unused SDL
GLES/EGL/offscreen/Vulkan paths and retains desktop OpenGL. These are local
modifications, not claims about original-game behavior or upstream guarantees.

Font assets/tuning and completed stance assets/tuning are not changed by this
release preparation. Full campaign, actual GOG data and another PC remain untested.
Code notices do not establish rights to game-derived assets. No publication has
occurred; separate local review reports contain exact payload manifests and holds.


## 2026-11-24 v0.2 test build

Update custom combat, the faction/weapon/variant sandbox menu, native NPC roster,
armor stance rigs and dismemberment integration. Include HP-based poise/parry
policy, stance speed and moving-target contact fixes, and native grenade/mine
combat handling. Reuse the portable owned-data patch launcher with Purple startup,
version0.2.0, validated current asset manifests and v0.1 patch/license reuse.
Remove inherited identifying diagnostic paths. Preserve published desktop-only
SDL dependency selection, licenses, notices and upstream credits. New local
customizations are distinct from original game behavior; see the included source
and current design documentation for test scope and limits.


Compact release update (2026-11-24): portable MDL/MDX data remains compressed and
reconstructs exact bytes through K12 streams on demand. Reuse native resource
ownership and cache; serialize bounded reconstruction into delete-on-close files.
Animation fitting, assets and combat tuning remain unchanged. Verification covers
2778 target model hashes, 23 rigs/69 samples/5229 matching posed transforms, repeat
loading, relocation, saves and failure paths. Measured installed footprint about
2.8 GiB. See PORTABLE.md and DESIGN.md for behavior and test limits.
