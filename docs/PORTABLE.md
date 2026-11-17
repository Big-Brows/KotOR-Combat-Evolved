# Portable Windows arena candidate

Extract the entire player ZIP into a writable folder outside the game installs,
then run KotOR-CE.exe. Keep bin, data, licenses and asset-manifest.json beside it.
Owned KotOR 1 and Jedi Academy are required. Automatic Steam/GOG discovery is
implemented; actual GOG data remains untested. Select the KotOR folder and Jedi
Academy's GameData folder if asked. The validated inputs are the tested English
Steam versions. Unsupported data is refused. Original installations are read-only.

Install the official Microsoft Visual C++ v14 x64 runtime if needed:
https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist
No Microsoft DLLs or installer are bundled. Windows 11 x64 is the tested platform.

The launcher reconstructs 184 arena resources using owned-data checks and creates
user-data beside itself for generated assets, settings, logs and saves. The save
alias is updated after relocation. Keep user-data with your own copy; exclude it
from fresh packages. It contains private local paths and player state.

The double-bladed player starts in the Taris sandbox. Top-row 0 opens Spawn Menu.
Equip the single or double saber through the native equipment menu. Repeated menu
swaps, item ownership, appearance and restoration are tested. Font defects remain
and fixes are paused. Full campaign, another PC, actual GOG data, other languages
or revisions and Unicode install paths remain untested. Historical randomized
staff NPC-combat timeouts are not claimed generally resolved.

## Build and verification

Use the installed VS2022 C++ tools, CMake, Ninja and Python described in BUILDING.md.
Run platforms/tools/build-portable.cmd. Optional KOTOR_CE_PYTHON selects Python;
KOTOR_CE_DEPENDENCY_CACHE points at the dependencies folder extracted from the
matching dependency-source archive for offline builds. The helper uses the
privacy build in platforms/out/k1-private-pc, K1/PC x64 RelWithDebInfo, engine
fixes ON, mobile/ASan OFF, miniaudio and KotOR:CE branding. Its Windows privacy
configuration disables unused SDL GLES/EGL/offscreen/Vulkan paths; desktop
OpenGL remains enabled. No separate checkout is required to compile the source.

The engine/launcher are rebuildable from corresponding source. Recreating the
exact 184 authoring outputs still requires the existing local development data;
the source archive does not claim a fresh-data authoring pipeline. Runtime
preparation from supported owned game data is a separate, tested operation.

With Python, use platforms/tools/package-portable.py --help and
platforms/tools/verify-portable.py --help. Packaging takes owned game directories
and a NEW ignored output directory; it preserves old candidates, anonymizes the
seed-player display name, checks delta round trips, and includes selected notices.
The verifier takes --package, --game-dir, --jka-dir and a NEW --audit-dir beneath
platforms/out. Expected result: passed=true, all 184 reconstruction hashes, failure
paths, save preservation, two relocated startups and original saves/config intact.

Native diagnostics: --detect, --prepare-only, --smoke, --game-dir and --jka-dir.
Diagnostic runs use exit codes instead of dialogs. A successful build/startup is
not proof of full campaign compatibility or permission to redistribute data.

## Publication scope

This remains a held local review candidate. Matching project/dependency source,
build scripts and notices must accompany an authorized binary release. Read
LICENSING.md and LICENSES. Code licenses do not license proprietary game-derived
patches, models, animation, sound, shader resources or saves. Asset provenance
and redistribution questions must be resolved before public upload.
