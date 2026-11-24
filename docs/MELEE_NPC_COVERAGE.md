# Native unnamed humanoids in the sandbox

This local expansion follows `MELEE_NPC_HANDOFF.md` and the user's narrower
scope: unnamed armed humanoids, including Wookiees and Gamorreans, excluding Selkath and
all other unarmed NPCs. Named characters,
creatures, droids and campaign activation are outside this change. The existing
six player stances and five synthetic Dark Jedi choices remain separate.
The current faction/type browser, gameplay-variant grouping, appearance preview
and alignment/target controls are described in [SANDBOX_SPAWN_MENU.md](SANDBOX_SPAWN_MENU.md).
The synthetic choices are retained in opt-in regression menus.

## Catalog and native character data

The owned English K1 module and global UTC inventories provide 432 candidate
templates under 91 display labels. This includes mixed-loadout actors
with melee weapons in inventory, optional hostile actors and saber users with
powers. Template availability does not imply campaign hostility or a placed,
active encounter. Module/global source identities keep those variants distinct.
An explicit display-label roster in `prepare-k1-native-npcs.py` excludes proper
names. Creature/droid bodies remain excluded; the native Gamorrean body is explicitly admitted.
Templates must have an equipped weapon as well as an equipped or inventoried
melee weapon. Seven additional variants that spawn without a native weapon in this checkout are explicitly excluded without gear/feat changes; the cause is not established. Unarmed/natural-attack templates are excluded even when a sword is
present in inventory. Generic faction defaults are not proof of hostility.

Generated UTCs retain the native class/level, abilities, feats, skills, powers,
HP/Force, appearance, plot flags, inventory and equipped items. Their resource
aliases and nested item references change to prevent collisions between module
templates with identical names. Each referenced item is copied byte-for-byte
under a content-derived alias. The generator checks the complete decoded GFF
against the original, allowing only those references and the sandbox callbacks.
No extra powers, feats, levels, weapons or health are granted.

Story-specific callbacks and dialogue are cleared in the sandbox copies; native
generic `k_def_`/`k_ai_` callbacks remain. This prevents arena testing from running
quest transitions. Script-added campaign buffs/loadouts are not established by
the UTC preservation check. The original installation remains read-only.

## Animation and ownership

Forty-two native body resources have separate Yellow, Red, Blue, staff and dual
variants (210 generated rigs). The generator reuses the current NPC clip sets, preserving
native body geometry/materials, original bind frames and the actual attached or
integrated head. It adds the existing Engage helper skeleton and smooth spine
weight field; skin palettes are partitioned without removing faces. Native arm,
hand and face influences remain native. These are bind-relative transfers, not
a claim of independently authored or visually approved animation for every body.

The existing resource reader omits MDX tangent vectors and loses identity when
different bones share a name. The new generator's local reader adapter preserves
both, without editing the shared importer. The first native name match remains
the public joint; duplicate joints and their original indexed skin influences
keep distinct identities. All exported clips and controller keys are read back
and compared. Existing player/NPC assets are protected by hashes.

Generated helper nodes must have their cached world frames refreshed before
export: the writer derives inverse skin binds from those caches. The initial
export omitted that refresh and stretched the torso. The repaired exporter reads
back serialized inverse binds and checks actual rest-pose skin deformation.
Export validation checks bind frames and animation keys for all generated rigs;
the installed probe checks inverse binds for every active melee adaptation.
Stored geometry and finite pose checks alone do not validate skin deformation.

Selection follows actual server equipment, including dual sabers, sword pairs,
double weapons, quarterstaffs, gaffi sticks and batons. Ranged or unequipped
states retain native control; melee equipment can reacquire Engage through the
existing admission, utility, pose, contact, poise, native damage and release
services. The initial server suppression gate requires the same actual body and
available assets. Unsupported actors retain native attacks.

Native-catalog NPCs use their own native movement rates and effects. Spawn
preparation validates their actual collision footprint and builds a larger
navigation graph when the existing cache does not cover it, outside the combat
loop. Existing player and synthetic-NPC tuning remains unchanged.
Idle native ranged actors start combat through the existing native action queue
when an actual enemy is available. The native-control boundary also bypasses the
prototype's humanoid tactical spacing; a skeleton alone does not grant ownership.

The Gamorrean retains its native embedded axe and weapon dummy; contact uses
that mesh rather than creating a duplicate wielded model. Its native cpause1
preview idle and cdie/cdead death clips are retained. Its cut models keep
the axe with a severed right arm/hand or upper body.

Solid batons/staves/gaffi sticks use the current drawn weapon geometry from the
native grip to its positive-axis end; telescoping staff parts are measured after
their native powered animation. They do not receive invented saber beams or
extra independent health opportunities. Native powered-item and sound services
remain responsible for presentation. Original blade geometry and dual hand damage
rules continue to handle swords and sabers.

## Use and verification

Use `engage-k1.cmd -Sandbox`, then top-row **0**. Choose a faction and an
NPC name (Weapon name), then a numbered native-stat variant and appearance.
Wookiee and Gamorrean have their own factions, including Wookiee Thug under
Wookiee. Hostile/Friendly, global AI and Target Player controls use the existing
sandbox services. Equipment chooses the compatible fighting style. Selecting a
native entry never equips a synthetic replacement weapon.

Preparation uses the existing local toolchain and ignored importer resources:

```powershell
$nativeNpcBlender = Join-Path ${env:ProgramFiles(x86)} 'Steam/steamapps/common/Blender/blender.exe'
$nativeNpcPython = Join-Path (Split-Path $nativeNpcBlender) '5.3/python/bin/python.exe'
& $nativeNpcPython platforms/tools/prepare-k1-native-npcs.py
& $nativeNpcBlender --background --factory-startup --python platforms/tools/prepare-k1-native-npc-rigs.py
platforms\tools\build-k1.cmd
.\engage-k1.cmd -NativeNpcProbe
```

These commands do not install tools. Preparation writes only ignored output.
The launcher verifies the catalog/rig manifests and deploys their assets while
holding the existing exclusive arena-profile lock. It verifies the isolated
SAVES alias before starting the test. See current `STATUS.md` for actual results.

The native probe spawns every catalog entry, samples all seven melee cuts where
melee equipment is active, checks finite posed geometry and real weapon segments,
and restores the original body, head, texture, equipment, HP/Force, level and feats.
A separate phase uses the ordinary controller for a hostile Mandalorian's
approach, chain, native damage application and AI-off restoration. Direct model
checks and that autonomous encounter are different verification levels; neither
proves every campaign encounter or every power/utility sequence.

The full source/appearance/equipment matrix and asset hashes stay in ignored
`platforms/out/unnamed-melee-npcs/catalog-report.json` and `rig-report.json`.
Campaign behavior, original-executable comparisons, all script-driven equipment
changes, and visual approval of every new body are
**not tested**. Native fallback for ranged/unequipped variants is intentional;
unarmed NPCs and Selkath are excluded. No sword or saber is invented for a ranged
variant, and no new unarmed Engage stance was added.
