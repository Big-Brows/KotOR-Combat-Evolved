# KotOR:CE

KotOR:CE is a single-player KotOR 1 combat prototype built on the K12 C/C++ engine
foundation. The current development scene is the Taris fighting-arena sandbox,
with directional melee, yellow/double-bladed fighters and native KotOR utility
systems. K12 and OpenJK credits and original notices are preserved.

## Requirements and status

Use owned KotOR 1 and Jedi Academy data. Original executables/game installations
are read-only; portable settings/saves belong in user-data beside the launcher.
This snapshot is prepared for review and has not been published or cleared as a
complete public data release. The source can build the engine and native launcher;
creating the current arena data package still needs the reviewed local authoring
inputs and game-data permission review. No game assets or personal saves are
supplied as new source files by the publication preparation.

The latest local tests established Steam startup and isolated arena operation.
Actual GOG data, different languages/revisions, Unicode paths, a clean second PC
and full campaign compatibility remain unverified. The native equipment-menu single/staff saber crash is fixed. Repeated swaps
from both starting sabers and the final single-start sandbox regression passed.
Historical randomized staff NPC-combat timeouts are not claimed generally resolved.
Known font artifacts remain; font fixes are paused.

## Build

Install the toolchain/dependencies described in BUILDING.md under their own
licenses. From a Visual Studio 2022 x64 environment, CMake preset k1-host-pc
builds the engine. platforms/tools/build-portable.cmd builds the privacy variant
and native launcher. Set KOTOR_CE_PYTHON if Python is not discovered automatically.
The source-download URLs and hashes are pinned by K12's CMake dependency files.
No compiler or installer is bundled.

A developer can set KOTOR_CE_DEPENDENCY_CACHE to a directory containing the
already fetched k2sdl2-src, k2freetype-src, k2zlib-src, miniz-src and 7zip-src
folders for an offline build. That cache is local ignored output, not source
content to upload. The build helper does not require another checkout's cache.

## Licensing and provenance

Read LICENSING.md and LICENSES. The combined engine incorporates GPL-2.0-only
OpenJK code; do not describe the whole project as CC0. K12 retains its CC0 notice.
Data licenses are separate from code licenses. There is no affiliation with or
endorsement by the original game publishers. Development design and historical
upstream notes are retained in docs/DEVELOPMENT_README.md and docs/DESIGN.md.

## Review source bundles

The matching dependency-source review archive extracts a dependencies folder.
Point KOTOR_CE_DEPENDENCY_CACHE at that folder for an offline source build; the
normal source fetches remain pinned as well. Component source notices are kept
where those sources require them, including authentic alternative-license texts.
That does not select every alternative for the combined executable.

The release packaging script anonymizes custom player display names in the
initial seed and stored history using platforms/tools/sanitize_release_seed.py.
It preserves archive lengths/offsets and refuses resource/script name collisions.
This is a privacy transformation of a separate release copy, not permission to
redistribute the underlying game data. Existing development saves are not changed.
