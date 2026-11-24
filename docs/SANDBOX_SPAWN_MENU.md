# Sandbox spawn menu

Top-row **0** opens the native modal popup. Choose a faction in the left list,
then an NPC type in the middle list. Both lists support the native scrollbar,
mouse wheel and selection controls. Factions and types follow the requested
ordering. Guard templates using Czerka models appear under Czerka; other Guards
appear under Thugs. Lavin Thug is retained under Thugs, and Trandoshan appears
under Trandoshans. Wookiee and Gamorrean are dedicated categories; Wookiee
Thug belongs under Wookiee.

Faction and type lists listen for native selection-change notifications, not
only row activation. Repeated notifications for the current selection preserve
its details. The mouse regression performs native mouse-down/up at hit-tested
row coordinates, including rows reached by scrolling; it does not call the
panel's selection handler directly.

The middle list uses NPC name (Weapon name). The right column has numbered
Variant 1, Variant 2, etc., a full-body idle preview, shared appearance choices,
native level, HP, Force, abilities and feat count. All native stat differences
remain variants, including distinct feat sets. Appearance changes copy only
cosmetic body/head/armor information; the stat template supplies combat data and
weapons. Spawned target names retain the original name without the weapon suffix.

Lightsabers expose Native/Blue/Red/Green/Yellow/Purple colour choices. Single
sabers support existing Yellow, Red and Blue stances; double blades and pairs
retain Double-blade and Dual Sabers styles. Preview actors loop pause1 idle,
with the original native dark-side glow for Hostile and light-side glow for
Friendly. The named Evil_stream red glow is retained even though it shares the
fx_Smoke texture with smoke. Other hostile smoke/gas emitters and both Sparks and
Sparkles are unlisted from this private scene. Friendly keeps its native blue
glow without bottom sparkles. Alignment clicks immediately rebuild the same
character preview with the selected effect. The native GUI scene has
projected-vertex framing at 94% of the preview window, including blade bounds and is never added to arena AI or combat queues.

Escape closes the menu before opening options. Reopening restores faction, NPC,
variant, appearance, colour, stance, alignment, scroll positions, focused control
and placement message. Text uses the native blue; selected faction/NPC rows stay yellow. Clicking either
list immediately replaces dependent details.

Alignment has exactly **Hostile** and **Friendly**. These use native factions
1 and 2 respectively for sandbox spawns. There is no native-faction menu choice.
Spawn retains the existing navigation/collision/spacing checks and reserves a
separate message region for rejected placement. Closing restores the prior pause
state. NPC AI affects the entire sandbox roster. TARGET PLAYER is also global:
No excludes the player from NPC target selection and native attack/spell actions,
clears queued actions and releases current melee controllers. Yes restores the
normal native reputation relationship. Opposing NPCs can still fight through the
existing combat activation, controller, contact and native damage services.
The policy does not edit saved faction tables or campaign actors.

## Grouping and verification

prepare-k1-weapon-menu.py uses the approved categorize-k1-npc-roster.py grouping:
faction, original name and equipped weapon combination identify each middle-list
entry. Native stat/loadout differences become variants, each sharing its type's
appearance pool. No source UTC/UTI is edited. The deployable k1npcmenu.2da references
separate native stat and appearance aliases. Current menu: 394 source templates,
161 entries, 311 stat variants and 699 variant/appearance combinations. Bouncer
and three Aqualish Thugs remain temporarily omitted. The complete native catalog
now has 432 templates and 91 original labels.

Sandbox actors clear Plot/Min1HP protection and remain destroyable so they can
die. This intentional runtime policy applies only to registered sandbox actors;
campaign templates and original game files remain unchanged.

Checks: platforms/tools/build-k1.cmd,
platforms/tools/launch-k1-engage-prototype.ps1 -SandboxProbe, and
platforms/tools/launch-k1-engage-prototype.ps1 -NativeNpcProbe.
The sandbox probe exercises 729 native preview combinations plus five legacy
fixtures, GUI-manager mouse events/scrolling, immediate faction switching, yellow
selections, Escape/reopen memory, lethal damage to all 36 protected templates,
five saber colours, all compatible weapon styles, original target names, native
stats, placement, AI, targeting, contact damage and reload cleanup.
The native probe loads every catalog template, checks rig swaps/native fallbacks,
and tests Red/Blue attack clips and restoration. New Red/Blue rigs reuse existing
styles without changing completed stance assets or tuning. The physical-contact
fixture separates attack directions and defers periodic Force scheduling to avoid
synchronized deflections; production tuning is unchanged.
Visual acceptance and original-executable matching are not claimed.

## NPC dismemberment

All implemented unnamed NPC bodies have cut resources through the existing fatal
powered-lightsaber system. Tusken legs cannot be severed; Tusken arms, hands, waist
and head retain support. Other bodies retain all eight regions. Powered lightsaber contacts in an eligible region have a100% cut chance on a
fatal hit. Blaster/Force damage does not sever limbs. Detached parts follow the
swing contact direction and do not cast shadows.

After native catalog/rig preparation, run prepare-k1-npc-dismemberment.py through
the documented Blender background workflow. It reads owned models and writes only
platforms/out/npc-dismemberment. Its final report covers42 bodies,334 cut pairs and
668 models. Sandbox startup verifies and deploys the table/models under its profile
lock. Finish preparation before launching; do not regenerate the same files during
deployment. Native character/item data and completed stance assets remain intact.

platforms/tools/launch-k1-engage-prototype.ps1 -NpcDismemberProbe checks catalog
coverage, posed cut models, region witnesses, actual native fatal effects and
cleanup. Its isolated stale-table fixture also proves both Tusken leg rows are
rejected, then restores the normal table. The earlier original model/death/revival/physical-contact and full sandbox
regressions passed before this expansion. Current new-species checks are listed
in STATUS.md. Visual acceptance and original
executable comparison remain not tested; see STATUS.md for exact evidence.

The focused -WookieeGamorreanProbe checks only the two new faction lists and
all30new appearance/variant combinations, without replaying existing NPCs or
unrelated option/combat fixtures. Native Gamorrean preview uses cpause1 and its
embedded axe, without a duplicate attachment. Species-specific evidence is
recorded under platforms/out/wookiee-gamorrean; see STATUS.md for actual results.

## NPC combat settings

Native template HP selects a60point poise cap/10%parry chance at1-49HP,
80points/15% at50-74HP, and100points/20% at75+HP. Single-weapon NPCs default to
Yellow unless overridden; actual dual and double/staff weapons keep compatible
styles. Gamorreans defaultRed. The requested named variant exceptions are in
platforms/tools/k1-npc-combat-rules.json; preparation resolves current menu variant
numbers into source/module identities and verifies unambiguous legacy-save keys.
Native HP/Force/feats/abilities/items stay intact. Regeneration/parry rewards stop
at the capacity; existing25point stun recovery and stance damage/timing remain.
Clash-specific escalating parry decisions remain separate from the base chance.

Selecting a new stat variant restores its configured default stance. Manual saber
stance choices survive appearance/alignment refresh and menu close/reopen, and
are honored by Spawn. The same policy applies to supported story NPCs through
play-story-k1.cmd, without a sandbox menu, arena loadout or changed quest callbacks.
Original source/module locals persist through native saves; old saves without
those locals use native class/gender/equipment and other identifying fields.
Unrecognizable/ambiguous changed records receive HP defaults instead of guesses.
Ranged actors retain native gun control; melee settings apply when a melee weapon
is equipped. See STATUS.md for focused verification and campaign coverage limits.

The detail stats now display **Max Poise** (the configured60/80/100point cap,
shown as a percentage of the original100point scale) and **Parry Chance** (the
effective configured base chance, including explicit per-actor overrides).
Clash-specific escalating parry decisions remain separate. These values update
with the stat variant and also apply to legacy synthetic menu fixtures. Existing
native stats, appearance selectors and blue menu text are retained.
