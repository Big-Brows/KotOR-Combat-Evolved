# KotOR:CE portable Windows arena candidate

## Compact model storage (local candidate)

`package-portable.py --compact-models` opts a new package into lossless on-demand
MDL/MDX loading. Alternatively, `prepare-compact-portable.py --package EXISTING
--output NEW` reuses an existing verified package and current built executables.
Use a fresh ignored output directory. Existing v0.2 artifacts remain unchanged.

The launcher stores small model descriptors and content-addressed source/delta
inputs under user-data. K12 reconstructs requested models into temporary files
that are deleted when their resource stream closes. Keep the entire portable
folder together, including user-data/override/.k1models. Original game installs
remain read-only. Corrupt model inputs fail validation when requested; setup
continues validating owned source hashes and non-model resources as before.

Measured candidate: about 2.8 GiB including generated data, versus 16.2 GiB for
the existing v0.2 setup. Download compression is substantially unchanged. Temporary
disk space is also needed for actively opening models. This does not reduce K12's
existing parsed-model or rendering memory requirements.

Verification: 2,778 reconstructed model hashes match the current release. Native
pose comparison covers 23 rigs (all player styles, existing NPC styles and male/
female armor examples), 69 animation samples and 5,229 identical part transforms.
This is focused storage/loader validation, not a full campaign compatibility test.

The project/product name is **KotOR:CE**. Windows filenames use **KotOR-CE**
because a colon is not allowed in a filename. K12 credits and internal engine/
configuration identifiers remain intact; the existing checkout folder stays in
place so its build caches and local work continue to resolve.

Extract the complete portable ZIP to a writable folder outside both game
installations. Double-click **KotOR-CE.exe**. Keep `bin`, `data` and `licenses`
beside it. The launcher EXE is not a standalone replacement for those files.

Owned KotOR 1 and Jedi Academy installations are required. The launcher reuses
K12's Steam library and GOG registry discovery for KotOR 1, and searches Steam
libraries and GOG registrations for Jedi Academy. If discovery fails, select
the KotOR folder and Jedi Academy's **GameData** folder when prompted. Neither
original executable runs. Setup opens the installations only for reading.

The v0.2 Purple single-saber player starts in the existing Taris sandbox arena.
Press top-row **0** for Spawn Menu. Gameplay and font assets are unchanged;
the reported font defects remain paused.

Settings, saves and `game.log` are created in **user-data**, beside the launcher.
An exclusive lock prevents two launches of the same portable profile. Move that
directory with your copy to retain progress. The save alias is refreshed after a
move. Setup never overwrites an existing arena fixture-save directory. Exclude
`user-data` from any fresh package you share: runtime settings and logs contain
paths belonging to the computer that ran it.

## Data preparation and limits

The v0.2 package contains bounded, SHA-256-checked data patches for the current validated NPC, armor and stance manifests. First-run setup
validates selected source ranges in the owned installations and reconstructs the
unchanged local arena resources, module and initial fixture. Patches do not
include the original executables, loose game resources, personal configuration,
private symbols, screenshots or development logs. Writes use temporary files;
the initial save directory is installed only after preparation completes.

This first candidate pins the source data ranges used by the available English
Steam installations. Automatic **discovery** of a GOG registration is tested with
synthetic layouts. An actual GOG installation, different languages/patch levels,
Unicode installation paths, another PC and full campaign compatibility are
**not tested**. Different data versions fail validation rather than silently
producing different assets. Broader version support needs evidence from those
owned installations; discovery alone does not establish it.

This is a local portable candidate, not a published release. Game-derived
patches do not by themselves settle redistribution rights. Review those patches,
the fixture metadata, all dependency notices and corresponding-source obligations
before publication. No publication is authorized or performed by these tools.

## Building and checking

From the checkout root, with the installed VS2022 C++ tools and existing cached
dependency sources:

```cmd
platforms\tools\build-portable.cmd
platforms\tools\build-portable.cmd --test-discovery
```

The developer build/profile remain separate. The new engine is K1/PC x64
RelWithDebInfo in `platforms/out/k1-private-pc`; fixes/QoL/miniaudio/
KEEP_GOING_LINK remain on, mobile/ASan off. `K2P_PRIVATE_PATHS=ON` removes local
source paths and private debugging output. The launcher uses static MSVC runtime;
the v0.2 game requires the Microsoft Visual C++ v14 x64 runtime installed separately; no Microsoft DLLs are bundled.
No tools or dependencies are installed/downloaded by this helper.

Use the Python interpreter recorded as `Python3_EXECUTABLE` in the existing
`platforms/out/k1-host-pc/CMakeCache.txt`, substituting paths to owned installs:

```text
<python> platforms/tools/package-portable.py --game-dir <kotor-folder> --jka-dir <jka-GameData-folder> --reuse-release <v0.1-Windows-ZIP> --output platforms/out/KotOR-CE-v0.2-windows-x86_64
<python> platforms/tools/verify-portable.py --package platforms/out/KotOR-CE-v0.2-windows-x86_64 --game-dir <kotor-folder> --jka-dir <jka-GameData-folder>
```

Quote paths containing spaces. Packaging requires the currently prepared arena
assets in the ignored development profile; it does **not** yet recreate authoring
inputs from a fresh source checkout. It accepts a new ignored output directory,
uses an explicit resource allowlist, tests every delta round trip and scans for
personal identifiers/paths. It never copies the unknown untracked executable.

Verification creates a separate relocated test copy beneath ignored
`platforms/out/launcher-audit`, leaving the deliverable fresh. Expected result:
`verification.json` reports `passed: true`, all manifest-listed reconstructed assets match,
missing/wrong/corrupt data is refused, existing saves survive setup, lock and
missing-engine failures are handled, two relocated arena startups pass and
original configuration/save hashes are unchanged. A second run needs a new audit
directory (`--audit-dir platforms/out/<new-audit>`); prior evidence is preserved.
The branded build also verifies the actual SDL window title is KotOR:CE.
Synthetic discovery prints
`DISCOVERY_PASS`. Public executables must have no personal identifier/path hits
and no PE CodeView/PDB records. PE optimization records are not private symbols.

For diagnostics, native launcher options are `--detect`, `--prepare-only`,
`--smoke`, `--game-dir PATH` and `--jka-dir PATH`. Diagnostic modes return exit
codes without message boxes: 0 success; 2 invalid arguments; 3 launcher path;
4 KotOR data missing/wrong game; 5 JKA missing; 7 writable-profile/lock failure;
8 engine missing; 9 smoke timeout; 10 engine error; 11 preparation/version error.
`--detect` checks discovery only. `--smoke` is a bounded hidden arena check;
the verifier also checks the engine's ready/equipment/failure markers.


## v0.2 release reuse

The v0.2 launcher mirrors play-purple-k1.cmd through the existing portable
launcher: single lightsaber, Purple/Red selection enabled, sandbox and existing
combat options. Launcher file/product version is0.2.0. The Microsoft runtime
remains an external requirement, matching the published v0.1 package.

Pass --reuse-release <verified-v0.1-Windows-ZIP> to package-portable.py with a new
output directory. Unchanged patches/notices are reused; current generated assets
are accepted from the existing preparation manifests. Obsolete NPC templates,
diagnostic text and user-data are excluded. Every reused/new patch is reconstructed
and compared to its exact target. The matching source update is reviewed separately;
GitHub automatic archives contain repository contents, not missing dependency caches.
Unchanged dependency source remains available with the v0.1 release.
