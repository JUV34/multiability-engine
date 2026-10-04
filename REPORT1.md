# Multi-Ability Engine Master Roadmap

Audit date: 2026-10-03. This living plan supersedes the previous exploratory
REPORT1.md. Read AGENTS.md and PROJECT_CONTEXT.md, but use verified current
source facts when those documents or REPORT_MS1.md describe an older state.
MS1-R repair completed on 2026-10-03. Historical pre-repair audit evidence is
retained where explicitly labelled; Sections 1 and 10 describe the current
repaired worktree. No MS2 or later implementation was begun.

Evidence labels:

- **CONFIRMED:** source inspection, Git evidence, or ARM compiler measurement.
- **LIKELY:** expected runtime consequence not triggered in an emulator here.
- **INFERRED:** architectural judgment derived from confirmed facts.
- **UNKNOWN:** unverified behavior or unresolved owner/game-design decision.

## 1. Current Project State

### Checkout and validation

- **CONFIRMED:** branch `feature/multiability-core`; HEAD and local
  `origin/feature/multiability-core` are `7d3d4c90ce`.
- Latest relevant committed changes:
  - `7d3d4c90ce` Add battle ability slot accessors.
  - `10c33ee838` Use BattlerHasAbility for simple battle checks.
  - `469fdfca41` Use BattlerHasAbility for Ripen berry checks.
  - `ef32f12adf` Add BattlerHasAbility helper.
- Local base: `6233cf8cb6`, `upstream/master` and `master` at inspection time.
- MS1-R is **COMPLETE, uncommitted**. Entry was the eight-file unfinished
  storage patch (60 insertions, 11 deletions). Targeted repairs remove its
  persistent array/unsupported MonData extension, retain deterministic runtime
  arrays, synchronize primary writers and restore all three capacity assertions.
  No branch change, commit or whole-file revert was performed.
- **CONFIRMED:** `make -j$(nproc)` passes with the assertions enabled.
  Exact ARM layouts and focused automated acceptance tests pass (Section 10).
- The owner's earlier mGBA smoke report concerns the pre-repair ROM. This
  iteration ran headless mGBA battle tests, not a new interactive smoke session,
  flash save/reload fixture, replay playback or cross-version link session.
- Automated test outcomes are recorded in Section 10. Test discovery must
  report actual executed tests; exit zero with no tests found is not a pass.

Current normal-build memory, **CONFIRMED**:

| Region | Used bytes | Capacity | Free bytes | Used |
|---|---:|---:|---:|---:|
| EWRAM | 226612 | 262144 | 35532 | 86.45% |
| IWRAM | 28376 | 32768 | 4392 | 86.60% |
| ROM | 26478568 | 33554432 | 7075864 | 78.91% |

Compared with the unsafe entry build: EWRAM -3656, IWRAM unchanged, ROM +168
bytes. Compared with historical scaffold output: EWRAM +32, ROM +408 bytes.
Future heap/stack/UI high-water usage remains **UNKNOWN**.

### Infrastructure actually present

- `MAX_BATTLER_ABILITIES` is 4, owned only by `include/pokemon.h`.
  The duplicate definition and unused MAX_MON_ABILITIES alias were removed.
- `GetBattlerAbilityAt` (`src/battle_util.c:4843`) still returns the traditional
  active query for slot 0 and NONE for every other slot. It ignores the array.
- `BattlerHasAbility` returns bool32, loops over the accessor, and permits
  an ABILITY_NONE match only through slot 0, preserving scaffold semantics.
- `GetBattlerAbilityInternal` still reads legacy `gBattleMons[battler].ability`
  and applies field presence, Gastro Acid, Neutralizing Gas, Ability Shield
  and Mold Breaker rules for that primary ability.
- Arrays exist only in BattlePokemon and AI_SavedBattleMon. Persistent
  BoxPokemon/Pokemon layouts match the committed baseline. MON_DATA_ABILITY1-4
  no longer exist. No extra persistence, species data, activation, learnset,
  learning UI or reminder system was introduced.
- `SetBattleMonAbility` in `src/pokemon.c`, declared in `include/pokemon.h`,
  writes canonical `.ability` and its `.abilities[0]` mirror without touching
  extras, suppression, volatile state, attribution or scripts. Fresh producers
  clear all slots before deriving/setting primary. Test-injected extras survive
  primary-only mutations and AI snapshots; queries still ignore those extras.
- Primary selection still derives from species plus secure two-bit abilityNum.
  `CreateBoxMon` uses personality bit 0 for normal ability selection when a
  second ability exists. `GetMonAbility` calls `GetAbilityBySpecies`, with
  fallback to a nonempty species ability; selector 2 supports hidden ability
  selection. This selector is not four independently equipped IDs.

## 2. Architectural Invariants

MS1-R satisfies the primary, deterministic producer and save-bound contracts
below. Contracts for enabled extras, activation and persistence remain future work.

1. `MAX_BATTLER_ABILITIES == 4`; gameplay slots are zero-based 0 through 3.
2. Infrastructure preserves traditional gameplay until an explicit milestone
   enables extras. The initial primary is species/abilityNum-derived, and
   extra runtime slots are deterministically NONE.
3. **CONFIRMED MS1-R decision: keep legacy BattlePokemon.ability canonical during migration.** Current
   queries, scripts, AI and event dispatch already consume it. Making the
   unused array canonical immediately creates unnecessary regression risk.
4. While both fields exist, `ability == abilities[0]` must hold at every
   initialization, mutation, simulation and restore boundary. MS1-R repairs
   the audited writers. A future runtime `ability + extraAbilities[3]` layout can remove
   the duplicate mirror entirely; that alternative is not implemented.
5. Persistent equipped choices, battle replacements, suppressed state and
   AI-known abilities are separate concepts. Temporary battle effects must
   not silently persist or expose unknown choices to the AI.
6. Separate raw slot lookup from active/contextual lookup. The active result
   depends on candidate ability metadata, attacker bypass, field presence and
   protection. Invalid slots must return a defined empty result.
7. Keep BattlerHasAbility as bool32. Add a slot-finder only when a real caller
   needs an index, with an explicit not-found result. BattlerHasAbilitySlot is
   not currently present and should wait for such a caller.
8. Primary writes now use SetBattleMonAbility. Proposed SetBattlerAbilityAt,
   ReplaceBattlerAbility and SwapBattlerAbilities are future names, not
   existing APIs. Set maintains state; replacement/swap also preserves the
   appropriate existing cleanup, volatile state, scripts and attribution.
9. All relevant save-capacity assertions stay enabled and pass. They are not
   obsolete because a feature is unfinished.
10. Ability identities require sufficient width: sizeof(enum Ability) is 2,
    ABILITIES_COUNT is 319 and ABILITY_NONE is zero in this checkout.
11. Activation must resume safely across battle-script yields. Scalar
    gLastUsedAbility and once-per-battler flags cannot stand in for a complete
    multiple-event schedule.
12. Internal primary-plus-extras may expose four equally replaceable gameplay
    slots later. Equal internal persistent slots are not required by the UI.
    Copy/swap, duplicates, form mapping and suppression scope require explicit
    design decisions, not assumptions from another project.

GetBattlerAbilityAt is still a suitable active-slot interface. Future extras
must evaluate their own metadata, not reuse the primary's breakability or
unsuppressability. Preserve separate ignore-Mold-Breaker semantics and inspect
recursion if suppression logic also queries other abilities.

## 3. Known Technical Risks

### Exact structure sizes and limits

**CONFIRMED:** measured using arm-none-eabi-gcc 13.2.1, normal configuration,
ARMv4T/Thumb with `-mabi=apcs-gnu`. HEAD baseline headers were isolated in
/tmp. Host-native sizes and approximate source comments were not evidence.

| Structure | HEAD bytes | Unsafe MS1 bytes | Repaired MS1-R bytes |
|---|---:|---:|---:|
| BoxPokemon | 80 | 88 | 80 |
| Pokemon | 100 | 108 | 100 |
| BattlePokemon | 140 | 148 | 148 |
| PokemonStorage | 34144 | 37536 | 34144 |
| SaveBlock1 | 15568 | 15632 | 15568 |
| SaveBlock2 | 3884 | 3884 | 3884 |
| SaveBlock3 | 4 | 4 | 4 |
| RecordedBattleSave | 4092 | 4284 | 4092 |
| AI_SavedBattleMon | 20 | 28 | 28 |

The packed ability enum still needs two bytes for more than 255 IDs. Four
slots add eight bytes, not four. Current alignment does not reduce that
growth. Each secure Pokemon substruct remains 12 bytes; the secure area is
at offset 32. The unsafe appended BoxPokemon array at offset 80 was removed.

Limits from `include/save.h`:

- SECTOR_DATA_SIZE = 3968; PC sectors 5-13 allow `9 * 3968 = 35712` bytes.
- SaveBlock3 allowance: `116 * 14 = 1624` bytes.
- SaveSector size = 4096; SECTOR_COUNTER_OFFSET = 4092, not 4104.
- SaveBlock1 limit = 15872, repaired free margin 304 bytes.
- SaveBlock2 limit = 3968, current free margin 84 bytes.

| Restored assertion | Repaired size | Limit | Byte margin | Result |
|---|---:|---:|---:|---|
| src/save.c:80 SaveBlock3FreeSpace | 4 | 1624 | +1620 | Enabled, PASS |
| src/save.c:83 PokemonStorageFreeSpace | 34144 | 35712 | +1568 | Enabled, PASS |
| src/recorded_battle.c:34 RecordedBattleSaveFreeSpace | 4092 | 4092 | 0 | Enabled, PASS |

Before editing, an independent ARM probe reconfirmed the two failing bounds
and the passing SaveBlock3 check. After repair, all three compile together
enabled in the probe and in normal/test builds. src/save.c and
src/recorded_battle.c now match HEAD; their restored assertions consequently
do not appear as remaining Git modifications.

**Historical unsafe MS1 PC truncation, now repaired:** `src/save.c:58-75` exposes only nine raw chunks.
Data after offset 35711 is not serialized. Current names begin at 36964 and
fusions at 37104: all names, wallpapers and four fusion Pokemon are outside
the save allocation. Only 405 complete BoxPokemon entries and 68 bytes of
the 406th fit; the remaining 20 bytes of that entry and the final 14 entries
do not. This is not simply a handful of bytes of remaining margin.

**Historical unsafe MS1 recorded heap overrun, now repaired:** MoveRecordedBattleToSaveData allocates
4096 bytes for savSection; RecordedBattleToSave memcpy writes 4284, overrunning
by **188 bytes**. TryCopyRecordedBattleSaveData likewise reads 4284 bytes
from a 4096-byte buffer. The special-sector functions transfer only 4092
payload bytes. Enlarged checksum offset 4280 is outside the saved payload.
Invalid replay, crash or heap corruption are **LIKELY** symptoms, not
emulator-observed here. MS1-R restores names/fusions offsets to 33604/33744
and recording checksum offset to 4088. A 4092-byte copy fits the 4096-byte
allocation with four allocation bytes spare, but zero payload growth margin.

### Risk register

| Severity | Subsystem | Evidence | Mitigation | Status |
|---|---|---|---|---|
| Critical | PC save | Historical 1824-byte overflow | Restored 34144-byte layout and assertion | RESOLVED MS1-R; actual flash round-trip still untested |
| Critical | Recorded battle | Historical payload/heap overflow | Restored 4092-byte layout and assertion | RESOLVED MS1-R; actual replay still untested |
| Critical | Ability data flow | Unsupported getters cleared primary | Removed unsupported API; restored species/selector derivation | RESOLVED MS1-R |
| High | Duplicate ability state | Historical unsynchronized writers | Canonical primary, deterministic arrays, centralized writes, regression guard | RESOLVED audited boundaries; future writers must use setter |
| High | Old saves | Unsafe MS1 files used incorrect strides and may be truncated | Baseline restored; keep unsafe files separate; explicit future version policy | No unsafe-MS1 migration; baseline compatibility measured, not fixture-tested |
| High | AI knowledge/calculation | Snapshot arrays added, scalar knowledge and contexts retained | Knowledge-aware slots and full simulation restore | BLOCKING before extra-ability release |
| Medium | SaveBlock3 assertion | Unrelated size 4 | Assertion restored, 1620-byte margin | RESOLVED MS1-R |
| Medium | RAM/ROM pressure | Section 1 measurements, only 4392 IWRAM bytes free | Budgets, heap/stack peaks, ROM definitions | Non-blocking now; future budget gate |
| Medium | Future UI memory | Windows/tasks/copies not measured | Measure actual workflows; avoid large duplicate buffers | Non-blocking until UI work |
| Medium | Controller/link layout | Runtime packet remains 148 bytes | Deterministic producer; single-record packet 152 <= 256 tested; consumers reinitialize | Primary path RESOLVED; cross-version link not validated |
| Low | Review hygiene | Report ignored by existing .gitignore | Diff whitespace fixed; tracking requires separate owner decision | Diff check PASS; report still ignored |

All current REQUEST_ALL_BATTLE callers request one mon (zero selector or one
mask bit). The historical multi-record handler comment is not a capacity
guarantee: two runtime records exceed transferBuffer[256]. No current caller
does that; MS1-R does not expand packet/stack allocations or link capacity.
Adding multi-record requests needs a separately bounded controller repair.

### Complete primary/slot-0 mutation inventory

Scanned source, headers, tests, battle script data and assembly for direct
assignments, pointer writes, copies, swaps, resets and restore paths.
References were examined separately. The table below preserves the **pre-repair
audit inventory**, including its old line numbers and defects, not current status.
All primary writes identified there now use SetBattleMonAbility. Fresh controller,
conversion, intro, switch and form producers initialize extras to NONE; form/AI
candidates inherit the repaired producers. Debug VAL_U16 ability edits and forced
test overrides use the setter. AI restoration takes canonical saved primary and
restores extras. Transform prefix copies, Ally Switch and full-struct simulation
snapshots carry both fields together and required no mechanical rewrite.

| Location/function | Classification | Result for ability == abilities[0] |
|---|---|---|
| src/pokemon.c:3397 PokemonToBattleMon, writes 3437/3444 | Initialization/copy | New getters return four zeros; later species-derived primary overwrites legacy only. **Broken** |
| src/battle_controllers.c:1443 GetBattlerMonData, REQUEST_ALL_BATTLE | Producer/struct copy | Local battleMon is uninitialized; new array never filled. Whole struct transmitted; consumer repairs primary only. **Indeterminate array** |
| src/battle_main.c:3413-3428 DoBattleIntro | Copy then primary/test write | Array retained from packet, primary derived separately. **Broken** |
| src/battle_script_commands.c:5016-5038 Cmd_switchindataupdate | Byte copy, primary/test write | Same defect at switch-in. **Broken** |
| src/battle_util.c:9387 CopyMonAbilityAndTypesToBattleMon | Initialization/replacement | Writes both from unsupported getters: synchronized but primary wrongly NONE; forced test branch writes both correctly |
| src/battle_util.c:9417 RecalcBattlerStats, called from TryBattleFormChange:8640 | Form replacement/reversion | Calls copy helper and can lose legitimate primary; behavior regression |
| src/battle_ai_switch.c:74 InitializeSwitchinCandidate | Temporary simulation | Conversion then copy helper clears natural primary unless forced test ability overrides |
| src/battle_main.c:3258 SwitchInClearSetData | Forced test write | Legacy only; can desynchronize |
| src/battle_main.c:3727 TryDoEventsBeforeFirstTurn | Forced test write | Legacy only; can desynchronize |
| src/battle_controllers.c:1499 GetBattlerMonData | Forced test write | Live legacy only, not local array |
| src/battle_util.c:3878 AbilityBattleEffects: Mummy/Lingering Aroma | Replacement | Legacy/overwrittenAbility only; can desynchronize |
| src/battle_util.c:3904-3905 Wandering Spirit | Swap | Legacy/overwrittenAbility only; both battlers can desynchronize |
| src/battle_script_commands.c:9014 Cmd_trycopyability | Role Play/Doodle copy | Legacy/overwrittenAbility only; can desynchronize |
| src/battle_script_commands.c:9183-9184 Cmd_tryswapabilities | Skill Swap | Legacy/overwrittenAbility only; can desynchronize both |
| src/battle_script_commands.c:10640 Cmd_tryoverwriteability | Worry Seed/Simple Beam-style overwrite | Legacy/overwrittenAbility only; can desynchronize |
| src/battle_script_commands.c:12545 BS_SetTracedAbility | Trace/shared staged replacement | Legacy/overwrittenAbility only; can desynchronize |
| src/battle_script_commands.c:12934 BS_TryActivateReceiver | Receiver/Power of Alchemy staged copy | Stages tracedAbility; actual write uses BS_SetTracedAbility |
| src/battle_script_commands.c:13044 BS_TryEntrainment | Copy/overwrite | Legacy/overwrittenAbility only; can desynchronize |
| src/battle_ai_util.c:255,369,417 SaveBattlerData/SetBattlerData/RestoreBattlerData | Save, temporary masked replacement, restore | Set writes both; snapshot/restore preserves all incoming state, including stale mirrors; extras unmasked |
| src/battle_ai_main.c:830-851,1112-1158 | Full simulation copies/restores | Carries both fields; candidates may already be inconsistent |
| src/battle_ai_util.c:4398/4405 AllocSaveBattleMons/FreeRestoreBattleMons | Full-array restore | Preserves consistency or inconsistency; does not repair |
| src/battle_script_commands.c:7780 Cmd_transformdataexecution | Prefix copy | Copies through offsetof(pp), now including all slots; inherits target inconsistency and implicitly expands Transform copied state |
| src/battle_anim_effects_1.c:6698 AnimTask_AllySwitchDataSwap | Whole-struct swap | Moves both fields together; does not fix incoming mismatch |
| src/battle_debug.c:1886,1584 SetUpModifyArrows/update path | Indirect pointer write | u16 editor writes legacy field only; can desynchronize |
| src/battle_main.c:3409 Safari reset; static gBattleMons initializer:164 | Zero initialization | Both zero, consistent empty state |
| GetBattlerAbilityInternal, CanBreakThroughAbility, popups/history, scalar DamageContext | Reads only | Consume legacy/scalar primary, do not repair mirrors |
| Gastro Acid, Neutralizing Gas cleanup, RemoveAbilityFlags, FaintClearSetData | Volatile/suppression changes | No direct primary assignment; no synchronization repair; flags remain per-battler/scalar |

No further direct BattlePokemon primary assignments were found in the scanned
tree. AI knowledge/form contexts/test event structs with `.ability` are not
BattlePokemon writes. Bulk and indirect copies are included above because a
search for `.ability =` alone misses them.

**CONFIRMED:** legacy primary remains canonical; runtime slot 0 now mirrors it.
Queries still ignore extras. Natural/hidden tests avoid invalid forced-ability
overrides, and form tests verify derived primary, not just ability messages.

## 4. Completed Milestones

### Environment, abstraction and committed query scaffold: COMPLETE

- Summary: original clean build; bool32 helper; selected safe equality
  migrations; four-slot accessor with extras NONE.
- Files: include/battle_util.h, src/battle_util.c,
  src/battle_hold_effects.c, src/battle_main.c, src/battle_util2.c,
  src/battle_move_resolution.c, src/battle_stat_change.c,
  src/battle_script_commands.c. Relevant commits are in Section 1.
- Verification: historical successful builds/runtime checks; current source
  confirms scaffold contract; current normal build passes.
- Remaining debt/limitations: scalar AI/damage/event contexts; no simultaneous
  activation. The scaffold did not promise persistent equipped slots.

### Historical Qwen storage Milestone 1: PARTIAL, superseded by MS1-R

- Summary: persistent/battle/saved-AI arrays, enum labels, two conversion
  changes, AI snapshots and disabled save assertions. Files listed in Section 1.
- Verification: current normal build, compiler layout probes, all three
  isolated assertion checks, mutation inventory; owner reports ordinary ROM
  play-testing. Automated test record is in Section 10.
- Debt: both hard overflows, recorded heap overrun, missing Get/Set cases,
  missing controller/creation population, desynchronization, form/AI primary
  loss, save incompatibility and trailing whitespace.
- Limitations: no working persistent slot API, active extras, event scheduler,
  learned abilities, learning/recovery UI, or multi-slot AI knowledge.
- PARTIAL is more accurate than COMPLETE WITH DEBT: required functionality
  is absent and serialized bounds are violated. Build success cannot satisfy
  those missing objective requirements.

### MS1-R repair: COMPLETE

- Removed only unfinished persistent fields/API labels; retained four battle
  slots and AI snapshots. No species tables, progression/UI, save-sector/count
  changes, save migration or extra activation were added.
- Added SetBattleMonAbility; repaired all audited direct/test/debug primary
  writes, controller packets, conversion, form/candidate derivation and AI
  restore. Existing replacement flags, suppression, scripts and attribution
  remain intact. Bulk Transform/swap/snapshot paths preserve both fields.
- Source/header repair files: include/pokemon.h, include/battle.h,
  include/battle_util.h; src/pokemon.c, src/battle_util.c,
  src/battle_controllers.c, src/battle_main.c, src/battle_script_commands.c,
  src/battle_ai_util.c, src/battle_debug.c, src/save.c, src/recorded_battle.c.
- Tests: test/pokemon.c, test/save.c, test/battle/ability_slots.c,
  test/test_runner_battle.c. The runner checks each battler's mirror before
  THEN in every battle test, extending existing mutation regression coverage.
- Every objective MS1-R acceptance criterion passes: exact persistent sizes,
  all restored assertions, normal build, tested natural/hidden producers,
  switch/form/AI primary correctness, deterministic extras, audited mirror
  boundaries, no extra persistence, clean diff and no tracked generated changes.
- Verification: 16/16 MS1-R tests; 277 passing tests across 17 file filters,
  two already-marked known failures and three TODOs; details in Section 10.
- Remaining validation: interactive debug/ROM smoke, actual flash save/reload,
  replay and link fixtures not performed. Bounds tests are not claims of those
  runtime workflows. Future multi-slot semantics/AI remain unimplemented.

## 5. Planned Milestones

MS1-R has repaired the existing patch. Next prove runtime contracts and
activation before committing to permanent equipped choices and progression
menus. Persistent work already started does not justify the unsafe schema.
AI and tests accompany each migrated runtime category, rather than waiting
until a final integration milestone.

### MS1-R: Restore a save-safe, deterministic infrastructure baseline

- **Status:** COMPLETE; implemented and verified in Section 4/10.
- **Goal:** safe scaffold development with unchanged traditional gameplay.
- **Why it exists:** historical MS1 save bounds and ability producers were broken.
- **Dependencies:** independent re-verification and explicit authorization,
  both satisfied; no new gameplay design was needed.
- **Files/subsystems likely affected:** include/pokemon.h, include/battle.h,
  include/battle_util.h; src/pokemon.c, src/battle_util.c,
  src/battle_controllers.c, src/battle_main.c, src/battle_script_commands.c,
  src/battle_ai_util.c, src/battle_debug.c, src/save.c, src/recorded_battle.c;
  focused tests.
- **Explicit non-goals:** learned/innate content, learning UI, new suppression
  policy, new persistent four-ID schema or save-sector allocation.
- **Implementation tasks:** propose repair diff first; remove unfinished
  appended BoxPokemon array/unused MonData extension through targeted edits,
  restoring baseline persistent layout; retain runtime arrays only with
  enforced producer/mutation contracts; initialize primary from existing
  derivation and extras NONE everywhere, including packets; centralize all
  primary writes with existing cleanup/scripts; repair AI/test/debug paths;
  restore all three assertions; one owner for count; fix patch whitespace.
  Do not revert entire files indiscriminately.
- **Acceptance criteria:** BoxPokemon/Pokemon 80/100 bytes; storage 34144 <=
  35712; recording 4092 <= 4092; all assertions enabled; every producer/write
  preserves the mirror if retained; extras NONE; form recalculation retains
  GetMonAbility-derived primary; scaffold query semantics unchanged;
  git diff --check clean.
- **Tests required:** natural/hidden-selector creation, conversion/controller
  entry without forced abilities, switch/form/reversion, AI save/restore and
  candidates, all listed primary mutators with mirror checks; PC final slots,
  names and fusions serialization bounds; recording allocation bounds; build
  and headless mGBA battle tests. Actual flash/replay/interactive smoke remains
  additional validation, not claimed from in-memory serialization tests.
- **Risk level:** high across persistence and producers, despite unchanged
  intended gameplay.
- **Expected rollback point:** saved pre-repair worktree plus committed
  scaffold HEAD; MS1-format disposable saves kept separately.

### MS2: Stable multi-slot runtime query and state API

- **Goal:** query four runtime abilities correctly in their battle context.
- **Why it exists:** accessor ignores extras; primary metadata is insufficient
  for candidate-slot suppression/bypass rules.
- **Dependencies:** MS1-R and explicit pilot raw/active, empty/duplicate and
  suppression contracts. Unresolved rules block affected cases.
- **Files/subsystems likely affected:** src/battle_util.c,
  include/battle_util.h, include/pokemon.h, test runner setup, AI masking
  and calculation snapshots where pilot callers use these queries.
- **Explicit non-goals:** permanent storage, learning, broad content, complete
  activation or mass migration of special accessors.
- **Implementation tasks:** bounded raw access plus candidate-metadata active
  query; preserve legacy entry points; inject extras through tests; add slot
  finder only for a real attribution/scheduler caller; verify no recursion;
  prevent AI hidden-slot leakage before prediction uses the queries.
- **Acceptance criteria:** A/B/C/D battler returns TRUE for all four, FALSE
  for absent E; invalid slot NONE; empty/duplicate behavior matches contract;
  ordinary fixtures unchanged; specified bypass/shield/suppression cases pass
  with save assertions enabled.
- **Tests required:** slot bounds, duplicates, absent battler, raw versus
  active, primary replacement, suppression and AI snapshots; build/pilot ROM.
- **Risk level:** medium/high contextual semantics.
- **Expected rollback point:** MS1-R; extras remain test-only.

### MS3: Simultaneous calculations and resumable activation

- **Goal:** multiple passive/triggered effects operate with correct order,
  messages and lifecycle.
- **Why it exists:** boolean queries do not replace scalar switches, damage
  contexts, attribution or script scheduling.
- **Dependencies:** MS2, combination/order rules per pilot category; integrate
  matching AI prediction when each category migrates.
- **Files/subsystems likely affected:** src/battle_util.c,
  src/battle_move_resolution.c, src/battle_stat_change.c,
  src/battle_end_turn.c, src/battle_main.c, src/battle_script_commands.c,
  src/battle_interface.c, include/battle.h, include/battle_util.h,
  data/battle_scripts_1.s, AI contexts and tests.
- **Explicit non-goals:** learning/storage/UI, mass species balance, implicitly
  defined special-interaction rules.
- **Implementation tasks:** migrate matrix categories; define modifier
  combinations/rounding/exclusions; resumable switch-in/end-turn/on-hit/contact/
  field/faint schedules; identify triggering battler/slot/ability; bound state;
  reset on switch/faint/end; handle abilities changing during a schedule.
- **Acceptance criteria:** four-slot fixtures demonstrate configured passive
  and triggered effects; no lost/duplicate event across script re-entry;
  attribution matches effect; deterministic damage agrees with AI prediction;
  original one-ability fixtures pass; all asserted bounds enabled.
- **Tests required:** each category, four slots, doubles, multi-hit/substitute,
  nested/interrupted scripts, mid-chain faint/replacement, queue bounds;
  build, memory and mGBA scenarios.
- **Risk level:** high; accept category batches within this milestone.
- **Expected rollback point:** MS2; extra gameplay stays test-only until proven.

### MS4: Special battle mechanics and AI knowledge

- **Goal:** deliberate copying/swapping/overwriting/suppression and informed AI.
- **Why it exists:** these mechanics already work for one primary and need
  adaptation; they are not missing vanilla implementations to add from scratch.
- **Dependencies:** MS3; owner-approved scope for every relevant matrix row.
- **Files/subsystems likely affected:** src/battle_util.c,
  src/battle_script_commands.c, src/battle_main.c, src/battle_ai_main.c,
  src/battle_ai_util.c, src/battle_ai_switch.c, src/battle_ai_record.c,
  battle state structs and special-mechanic tests.
- **Explicit non-goals:** progression, final persistence, automatic AI
  omniscience or mechanical rewriting of raw/special reads.
- **Implementation tasks:** adapt each decided interaction with APIs; preserve
  battle/permanent boundary; affected-ability volatile cleanup; known/unknown
  slot knowledge, candidate simulation and complete restore.
- **Acceptance criteria:** tests for decided rows with extras on both sides;
  correct unaffected slots, protections and attribution; no persistent battle
  replacements; AI has no unrevealed-slot leakage; complete simulation restore;
  single-ability outcomes retained.
- **Tests required:** matrix special rows, Ability Shield, re-entry, form/
  Transform, Receiver/faint, doubles/Doodle and hidden knowledge; build/mGBA.
- **Risk level:** high.
- **Expected rollback point:** accepted MS3 plus per-mechanic checkpoints.

### MS5: Compact equipped persistence and safe serialization

- **Goal:** selected sets round-trip through saves, recordings and supported
  link operations within measured limits.
- **Why it exists:** species/level alone cannot encode arbitrary replacements;
  native Pokemon growth cannot fit current recordings.
- **Dependencies:** MS1-R through MS4; measured representation proposal,
  owner save-version/new-save policy and equipped-primary contract.
- **Files/subsystems likely affected:** include/pokemon.h, src/pokemon.c,
  include/global.h, include/save.h, src/save.c, src/load_save.c,
  include/recorded_battle.h, src/recorded_battle.c, src/battle_controllers.c,
  link/trade serialization and tests.
- **Explicit non-goals:** assumed extra sectors, global u64 ID masks, learning
  menus or accepting unsupported files without detection.
- **Implementation tasks:** measure Section 7 alternatives; define encoding,
  validation, integrity and versioning; implement real Get/Set/creation/box/
  party/battle paths; bounded serialization independent of runtime structs
  where needed; migration or rejection; audit all Pokemon copies.
- **Acceptance criteria:** all equipped IDs survive getter/setter, party/PC,
  save/reload, recording/replay and supported same-version link paths; all
  limits/assertions pass; unsupported/damaged files handled deterministically;
  old fixtures follow policy; temporary replacements do not persist.
- **Tests required:** highest IDs/invalid values, full PC/fusions, interruption,
  old fixtures, maximum replay/link payloads, egg/trade/form/evolution paths;
  normal build and ARM layouts.
- **Risk level:** high; explicit format checkpoint required.
- **Expected rollback point:** MS4 without permanent extras; save backups and
  matching binaries kept before new-format writes.

### MS6: ROM ability learnsets and progression events

- **Goal:** derive eligible learning events from species and level.
- **Why it exists:** desired progression exists in neither engine nor reference.
- **Dependencies:** MS5 and Section 8 acquisition/evolution/form/recovery policy.
- **Files/subsystems likely affected:** include/pokemon.h, src/pokemon.c,
  src/data/pokemon/species_info.h, src/data/pokemon/species_info/*.h,
  src/battle_script_commands.c, src/evolution_scene.c, src/party_menu.c,
  daycare/creation paths.
- **Explicit non-goals:** every species table, final menus, global known-ID
  bitmask, arbitrary removal of existing abilityNum semantics.
- **Implementation tasks:** ROM table schema/order; all crossed levels and
  same-level events; acquisition/evolution/forms; pending choices rather than
  overwriting full slots; recovery eligibility queries and pilot data.
- **Acceptance criteria:** specified events occur exactly; multi-level/full-set
  flow loses no pending choice; refusal follows policy; absent tables preserve
  baseline behavior.
- **Tests required:** EXP, Rare/Exp Candy, evolution/forms, high-level capture/
  gift/egg/trainer creation, repeated events; build/table validation.
- **Risk level:** medium/high due multiple entry points.
- **Expected rollback point:** MS5; empty tables keep progression inactive.

### MS7: Learning/replacement/recovery and ability display UI

- **Goal:** learn, replace one, refuse and recover eligible abilities clearly.
- **Why it exists:** a fifth offered ability requires an explicit decision.
- **Dependencies:** MS6, approved recovery/primary rules, actual UI RAM budget.
- **Files/subsystems likely affected:** src/pokemon_summary_screen.c,
  src/party_menu.c, src/evolution_scene.c, src/battle_script_commands.c;
  src/move_relearner.c patterns; new ability modules only if justified.
- **Explicit non-goals:** region/story/balance rollout, copying nonexistent
  reference learning UI, committed changes from merely highlighting an option.
- **Implementation tasks:** display actual four choices/descriptions; full-set
  replace/refuse/cancel/confirm states; continue pending events; reminder query
  and approved cost; atomic selection and temporary resource cleanup.
- **Acceptance criteria:** replace/refuse/cancel preserve all uninvolved data;
  fifth choice commits only after confirmation; pending events resume; display,
  battle and persisted choices agree; repeated UI sessions stay within budget.
- **Tests required:** input-driven flows, EXP/candy/evolution and reminder,
  repeated open/exit, reload and next battle; build/mGBA visual/high-water tests.
- **Risk level:** high workflow/memory integration.
- **Expected rollback point:** MS6 with player progression menus disabled.

### MS8: Integration and release gate

- **Goal:** stable four-ability foundation ready for future content development.
- **Why it exists:** category tests need cross-mechanic and long-session checks.
- **Dependencies:** earlier milestones accepted; no undecided enabled rows.
- **Files/subsystems likely affected:** tests, validators, engine fixes,
  verification and format documentation.
- **Explicit non-goals:** new region/story, masking unsupported mechanics.
- **Implementation tasks:** close matrix gaps; full regressions; event/memory
  stress; save/replay/link and party/candy/evolution workflows.
- **Acceptance criteria:** all enabled rows covered; full required suite passes;
  assertions enabled; no tracked artifacts; normal ROM boots/plays/saves/reloads;
  memory and remaining unsupported features published.
- **Tests required:** full make check; normal build; multi-session mGBA stress;
  save/replay fixtures and supported link flows.
- **Risk level:** high integration.
- **Expected rollback point:** last accepted feature checkpoint with its matching
  save schema; older binaries are not assumed compatible.

## 6. Battle Mechanics Migration Matrix

Proposed APIs below do not exist yet. Desired behavior is left explicitly open
where game design is undecided. Matching AI predictions and attribution are
part of each runtime migration, rather than a later blanket boolean change.

| Mechanic/category | Current state | Desired multiability behavior | Helper/API | Source locations | Milestone | Test requirement |
|---|---|---|---|---|---|---|
| Simple boolean checks | Selected checks migrated, still primary-only | Existence in eligible active slots | BattlerHasAbility/GetBattlerAbilityAt | src/battle_hold_effects.c; src/battle_util.c; src/battle_main.c; src/battle_util2.c | MS2/3 | Four present IDs, absent ID, primary parity |
| Damage modifiers | Scalar DamageContext per battler and switches | DESIGN DECISION REQUIRED: stacking/rounding/exclusions | DamageContext/GetAttackerAbilitiesModifier/GetDefenderAbilitiesModifier | include/battle_util.h; src/battle_util.c:6481,6820,7400,7422 | MS3 | Combined modifiers, partners, AI parity |
| Type/move immunity | Scalar ability, one chosen outcome | DESIGN DECISION REQUIRED: multiple blockers/absorbs | CanMoveBeBlockedByTarget/CanAbilityAbsorbMove | src/battle_util.c:2182,2235,8187,8223 | MS3 | Matching multiple defenses and bypass |
| Stat modification | Scalar ability inputs | DESIGN DECISION REQUIRED: combined Contrary/Simple/reactive boosts | CompareStat/stat context | src/battle_stat_change.c; src/battle_util.c | MS3 | Caps, combinations, feedback |
| Status prevention/cure | Existing primary checks | Eligible blockers; multiple-message policy DESIGN DECISION REQUIRED | TryImmunityAbilityHealStatus/active query | src/battle_util.c:8839; src/battle_script_commands.c | MS3 | Attempts, cure/re-entry, suppression |
| Switch-in | Deterministic primary/empty extras; scalar dispatch and done flags | Resumable all-slot events; order/duplicates DESIGN DECISION REQUIRED | AbilityBattleEffects/proposed event cursor | src/battle_util.c:2960; src/battle_main.c | MS3 | Four triggers, yields, doubles |
| End-turn | Primary dispatcher/state loop | Resumable effects; order DESIGN DECISION REQUIRED | AbilityBattleEffects/end-turn state | src/battle_util.c:3515; src/battle_end_turn.c | MS3 | Heal/damage/status chain, faint |
| On-hit/after-damage | Scalar move-end/form cases | Eligible effects; conflicts DESIGN DECISION REQUIRED | AbilityBattleEffects | src/battle_util.c:3715,3775,4281,4346 | MS3 | Multi-hit, substitute, spread/faint |
| Contact abilities | Scalar selection | Trigger order/replacement conflicts DESIGN DECISION REQUIRED | Active query/proposed event+mutation API | src/battle_util.c:3775 | MS3/4 | Multiple triggers, protection, dead attacker |
| Weather/terrain/field | Scalar setters/reactivation flags | Setter conflict/order DESIGN DECISION REQUIRED | TryChangeBattleWeather/Terrain/AbilityBattleEffects | src/battle_util.c:2466,4695,4739; src/battle_script_commands.c:11900 | MS3/4 | Simultaneous setters, reactivation |
| Fainting | Scalar Receiver/Soul-Heart-related events | Eligible events/order DESIGN DECISION REQUIRED | AbilityBattleEffects/BS_TryActivateReceiver | src/battle_util.c:4396; src/battle_script_commands.c:12934,12958 | MS3/4 | Multiple faint, ally/foe, switching |
| Suppression/Gastro Acid | Per-battler volatile, primary metadata | Scope DESIGN DECISION REQUIRED | GetBattlerAbilityInternal/proposed per-slot activity | src/battle_util.c:4866; src/battle_script_commands.c (gastroAcid) | MS2/4 | Mixed suppressibility, restoration |
| Mold Breaker-like bypass | Primary breakable lookup/global context | Candidate metadata; attacker scope DESIGN DECISION REQUIRED | CanBreakThroughAbility/GetBattlerAbilityIgnoreMoldBreaker | src/battle_util.c:4821,4833 | MS2/4 | Breakable+unbreakable slots, shield/status |
| Neutralizing Gas | Volatile field scan/reactivation | Scope/lifecycle/order DESIGN DECISION REQUIRED | IsNeutralizingGasOnField/active slots | src/battle_util.c:4584,4793; src/battle_script_commands.c:13589 | MS2/4 | Multiple gas users, removal/shield |
| Copying | Primary copy uses synchronized setter; rules unchanged | Source/destination scope DESIGN DECISION REQUIRED | SetBattleMonAbility/proposed replacement API | src/battle_script_commands.c (Cmd_trycopyability, BS_SetTracedAbility) | MS4 | Invalid source/protected slots, no persistence |
| Swapping | Both primaries/mirrors updated; rules unchanged | Slot/set scope DESIGN DECISION REQUIRED | SetBattleMonAbility/proposed swap API | src/battle_script_commands.c (Cmd_tryswapabilities); src/battle_util.c (Wandering Spirit) | MS4 | Both battlers, metadata/attribution |
| Overwriting | Primary/mirror synchronized; overwrittenAbility cleanup retained | Affected slots/restore DESIGN DECISION REQUIRED | SetBattleMonAbility/proposed replacement API | src/battle_script_commands.c; src/battle_util.c | MS4 | Switch/reset/unmodifiable traits |
| AI simulation | Primary masking/restore and full slot snapshots tested; scalar knowledge | Known/unknown slots and full restore | SaveBattlerData/SetBattlerData/RestoreBattlerData | src/battle_ai_util.c; src/battle_ai_main.c; src/battle_ai_switch.c; src/battle_ai_record.c | MS1-R complete; MS2/3/4 future | Natural candidates, hidden info, restore |
| Transform/Imposter | Prefix copy carries both primary fields; regression tests pass | Copied scope/volatile/form rules DESIGN DECISION REQUIRED | Cmd_transformdataexecution/proposed load API | src/battle_script_commands.c; src/battle_util.c (Imposter) | MS1-R complete; MS4 future | Suppression, switch revert, chosen extra-copy scope |
| Form changes | Species/selector derivation restored; extras reset; unforced Mega/weather tested | Extra/learned mapping DESIGN DECISION REQUIRED | TryBattleFormChange/RecalcBattlerStats | src/battle_util.c; src/pokemon.c | MS1-R complete; MS4/6 future | Mega/weather/HP/reversion |
| Trace | Staged primary/mirror copy synchronized and tested | Slot/source scope DESIGN DECISION REQUIRED | BS_SetTracedAbility/SetBattleMonAbility | src/battle_util.c; src/battle_script_commands.c | MS1-R complete; MS4 future | Multiple eligible abilities/opponents |
| Skill Swap | Both primary mirrors synchronized and tested | Primary-only/selected/set scope DESIGN DECISION REQUIRED | Cmd_tryswapabilities/SetBattleMonAbility | src/battle_script_commands.c | MS1-R complete; MS4 future | Extras per explicit policy, shields |
| Role Play/Doodle | Primary/mirror copy synchronized; existing tests pass | Copy/ally scope DESIGN DECISION REQUIRED | Cmd_trycopyability/SetBattleMonAbility | src/battle_script_commands.c | MS1-R complete; MS4 future | Partner/protection/doubles |
| Entrainment | Primary/mirror overwrite synchronized and tested | Copy scope DESIGN DECISION REQUIRED | BS_TryEntrainment/SetBattleMonAbility | src/battle_script_commands.c | MS1-R complete; MS4 future | Restrictions/extras/shield |
| Mummy/Lingering Aroma | Primary/mirror contact overwrite synchronized and tested | Scope/order DESIGN DECISION REQUIRED | AbilityBattleEffects/SetBattleMonAbility | src/battle_util.c | MS1-R complete; MS4 future | Multiple contact events/shield |
| Receiver/Power of Alchemy | Shared staged primary/mirror inheritance synchronized and tested | Inherited slots/order DESIGN DECISION REQUIRED | BS_TryActivateReceiver/BS_SetTracedAbility | src/battle_script_commands.c | MS1-R complete; MS4 future | Ally multiple traits, copy restrictions |

## 7. Persistence and Save Strategy

### Current baseline and historical unsupported storage

MS1-R restores persistent layout to HEAD: BoxPokemon has no extra array and
the unsupported MonData labels/copy calls are gone. Runtime slots exist only
in BattlePokemon/AI_SavedBattleMon. The following paragraphs describe the
**unsafe pre-repair MS1 layout**, retained to explain why it was removed.

**CONFIRMED:** BoxPokemon appends four two-byte IDs after secure data. They
are outside Pokemon checksum/encryption coverage (`src/pokemon.c:1336-1370`).
ZeroBoxMonData clears the enlarged object. CreateBoxMon sets abilityNum but
does not populate primary/extra IDs. The new array is otherwise not written
through any functional MonData handler.

GetMonData3 delegates these identifiers to GetBoxMonData3, which starts its
return value at zero. Neither its switch nor SetBoxMonData has ABILITY1-4
cases. Reads return zero; writes do not assign the new array. Adding labels
alone is not Get/Set support, contrary to REPORT_MS1.md.

Exact increases: PokemonStorage `420 * 8 + 4 * 8 = 3392` including four fusion
Pokemon; SaveBlock1 `6 * 8 + 2 * 8 = 64` including daycare; recorded parties
contain **24 Pokemon total**, not 24 parties, and grow `24 * 8 = 192`.
All limits and margins are in Section 3.

| Field | HEAD offset | Unsafe MS1 offset | MS1-R offset |
|---|---:|---:|---:|
| BoxPokemon secure | 32 | 32 | 32 |
| Pokemon status | 80 | 88 | 80 |
| PokemonStorage boxes | 4 | 4 | 4 |
| PokemonStorage boxNames | 33604 | 36964 | 33604 |
| PokemonStorage fusions | 33744 | 37104 | 33744 |
| SaveBlock1 playerParty | 568 | 568 | 568 |
| SaveBlock1 money | 1168 | 1216 | 1168 |
| RecordedBattleSave checksum | 4088 | 4280 | 4088 |

### Old-save compatibility

MS1-R matches the pre-MS1 compiled persistent layout again. This is measured
layout parity, not an actual old-save fixture validation. Saves produced by
the unsafe MS1 binary have different strides and may already be truncated;
do not load those files as a migration test or overwrite their originals.
The incompatibility analysis below applies to that unsafe binary.

**CONFIRMED:** pre-MS1 saves have no binary compatibility guarantee. Appended
enum values leave existing values unchanged, but Pokemon array strides,
party tails, downstream SaveBlock1 fields and recorded layout changed.
Physical sector IDs and SaveBlock2/3 do not shift; contents of changed structs
do. PokemonStorage is raw struct chunk serialization, with compiled strides.

GetSaveValidStatus/CopySaveSlotData use current chunk lengths. Final SaveBlock1
chunk grows from 3664 to 3728 bytes; final PC chunk grows from 2400 to 3968.
There is no old-size decoder. Since the writer zero-fills unused sector bytes,
old files can pass enlarged checksums if added bytes are zero. Do not assume
the loader reliably rejects incompatible saves. Accepted old bytes are then
interpreted at incorrect strides/offsets.

For the first old party Pokemon, raw new ability bytes overlap old status
(four bytes), level/mail (two), and HP (two). For the first old boxed Pokemon,
they overlap the next old record's personality/OT ID. Later new-stride records
start at different addresses. These bytes are not guaranteed zero. Current
unsupported getters return zero regardless; adding handlers without a migration
would expose misinterpreted data, not fix it.

Secure Pokemon coverage remains unchanged: the first old box's secure data
may still validate, while appended IDs are unprotected by that checksum.
Later misaligned records can become bad eggs; party stats, money and daycare
can be misinterpreted despite valid sector checksums. Specific old-file runtime
symptoms are **UNKNOWN** without disposable fixtures; layout incompatibility
itself is confirmed.

No general save-version/migration mechanism was found in current save/load
code/config. `src/load_save.c:190` explicitly has a TODO for future migration;
its HP/status adaptation is not a prior-layout Pokemon decoder.

**IMPLEMENTED MS1-R policy:** baseline persistent layout restored, with no
unsafe-MS1 decoder or new encoding. Future prototype format changes should require disposable new saves
and explicit unsupported-format detection/rejection. Pre-multiability player
save support is a separate owner-approved requirement with versioned decoding
and fixture tests, not an automatic promise. MS1-format saves may already lack
truncated bytes and are not a safe migration baseline. Preserve originals.

### Alternatives evaluated; final persistent format OPEN

| Alternative | Assessment |
|---|---|
| Runtime primary+extraAbilities[3] | Removes duplicate battle state; promising runtime choice, not a persistent capacity solution; measure alignment |
| Derive primary, append three u16 extras | Six added bytes round aligned BoxPokemon to 88; still eight-byte growth and both overflows |
| Three 9-bit extra IDs | Requires 27 bits/four bytes; ordinary BoxPokemon becomes 84, storage becomes 35840 (128 over limit), recording grows 96 over full baseline |
| Four 9-bit global IDs | At least 36 bits/five bytes; normal alignment can round to eight; whole-struct packing changes ABI and still needs every bound checked |
| Existing secure unused bits | Named unused fields total 20 bits across substructs, insufficient for three unrestricted 9-bit IDs; reclaiming used fields needs explicit tradeoffs; enlarging one substruct can grow all four through NUM_SUBSTRUCT_BYTES |
| Species/level-derived extras | No save growth for fixed passives/test pilots; cannot encode arbitrary equipped replacements; eligibility and selections are distinct |
| Per-species learnset indices | Promising compact selected-set encoding; pool bounds, evolution/forms/history and table-version stability must be proven |
| Separate bounded serialized records | Promising runtime/persistence separation; needs complete serialization, integrity/versioning and round-trip consumers |
| SaveBlock3 side allocation | Free 1620 now, but 420 four-byte records alone require 1680 before party/daycare/fusions; bit packing might fit but consumes configurable space and needs identity/version policy |
| Reduce PC/record capacity | Trades box count/replay length for room; requires explicit approval and full tests, not silent truncation |
| Reallocate sectors | High-risk redesign; two normal slots and special uses already occupy configured 32 sectors; no spare sector assumed |

No new persistent encoding/redesign was implemented. The committed persistent
schema is restored, with primary runtime support verified. MS5 decides future
equipped encoding from measured constraints and approved progression rules.

## 8. Ability Progression Design

Future concept: Pokemon learn abilities by species/level progression similarly
to moves. Maximum active abilities: **4**. When full, a new ability offers
replacement of one current ability or refusal. This is absent from both this
project and the inspected Emerald-J source. Older main/innate descriptions do
not resolve the learning system's primary-slot rules.

Unresolved decisions:

- Are all four equipped slots replaceable; what primary means for traditional
  ability selection, Ability Capsule/Patch, field abilities and copy mechanics?
- Duplicate ability acceptance and stacking/merging.
- Recovery of refused/forgotten abilities and Ability Reminder costs/UI.
- Current species eligibility versus inherited/pre-evolution-only recovery.
- Evolution: retain choices, offer missed levels, learn on evolution or remap.
- Temporary/permanent form mapping and battle species changes.
- Multiple crossed levels and multiple offers at the same level.
- Captures, gifts, trainers, trades or eggs obtained above unlock levels.
- Breeding/inheritance and parent choices.
- Rare Candy/Exp Candy queues, refusal, cancellation and retries.

A naive global-ID `u64 knownAbilities` is invalid for 319 IDs. A species-pool
bitset would only be valid with proven bounded pools and stable mappings; it
is not an approved representation. Do not restore that old proposal by default.

Recovery may derive from species, level and ROM learnsets if policy permits
relearning any currently eligible entry regardless of previous refusal.
Historical/pre-evolution recovery may need bounded extra state or explicit
rules. Existing ten-bit evolutionTracker is a mechanic counter, not species
evolution history; do not repurpose it by assumption. Equipped selections still
need persistence when the same species/level permits different builds. Final
recovery and encoding design remains OPEN.

## 9. Emerald-J Reference Lessons

Read-only `references/Emerald-J`, inspected HEAD `bc4097306e` (Merge branch
'Variant-colours' into EmeraldMain). Facts below are local-source-verified;
reference runtime was not tested. No proprietary project material was used.

### Adopt

- Central active-trait questions: src/battle_util.c GetBattlerTrait/
  BattlerHasTrait, rather than scattered scalar primary assumptions.
- Event completion state: include/battle.h switchInTraitDone[MAX_MON_TRAITS]
  and endTurnTraitDone[MAX_MON_TRAITS]; integrate with our actual state machine.
- ROM fixed-passive/eligibility definitions: include/pokemon.h SpeciesInfo.innates
  and src/pokemon.c:7549 GetSpeciesInnate.

### Adapt

- Primary-plus-extras internally can coexist with customizable gameplay.
  Reference BattlePokemon retains primary and an innate array; our UI does
  not need to expose an internal implementation distinction.
- Event attribution carries battler/ability identity. Study
  src/battle_util.c:11745-11809 PushTraitStack/PullTraitStackAbility/PopTraitStack,
  but independently specify bounded scheduling and changing-slot behavior.
- Keep raw/active/contextual and AI knowledge semantics separate; preserve
  bool32 predicate instead of overloading truth values with slot numbers.

### Do not copy

- Earlier report's persistent BoxPokemon innate claim is false for this
  checkout. include/pokemon.h:260-302 BoxPokemon/Pokemon have no innate array;
  BattlePokemon and SpeciesInfo do. MON_DATA_INNATE1-3 labels exist but
  src/pokemon.c has no handler cases. PokemonToBattleMon:3667 and controller:1437
  derive battle innates from species rather than persistent equipped slots.
- The shown stack is not FIFO. Push appends at first empty; Pull/Pop selects
  the last occupied entry before it: LIFO-shaped. Full occupancy leaves no
  empty sentinel for those routines; our implementation needs explicit bounds.
- GetBattlerTrait:11647 looks up extra species traits and checks bypass but
  does not apply every primary suppression/field rule. BattlerHasInnate mixes
  battle data and species fallback. This is not evidence of uniform Gastro
  Acid/Neutralizing Gas behavior to import.
- Constants enable one innate (MAX_MON_INNATES=1), species internal capacity
  three. This does not prove unrestricted four-slot save/activation safety.
- No ability-level learnset/learning/reminder system was found. Our progression
  is independent new work, not adaptation of existing reference menus.
- Do not copy species content/balance or assume evolution recreates Pokemon.
  Follow current source flows and explicitly design form/evolution policies.

## 10. Verification Protocol

Before marking any coding milestone complete:

1. Read project instructions/context/roadmap and actual source. Record branch,
   git status, git diff --stat, git diff and relevant history. Identify prior
   changes; no automatic branch changes or commits.
2. Plan architecture/storage changes first with measured layouts and owner
   gameplay decisions. Query helpers do not authorize implicit behavior changes.
3. Run `make -j$(nproc)` with all save-capacity assertions enabled; publish
   memory report. Recheck exact ARM sizes/offsets/margins after struct/config
   changes, including all containing structs and bounded serialized buffers.
4. Run focused existing tests with a verified filter using make check, and
   assert that tests were actually discovered. Add meaningful producer/save/
   synchronization coverage where absent; include unforced natural abilities.
   Full suite is required at integration, not as a substitute for targeted tests.
5. Relevant mGBA smoke tests: entry, switching, turn/item/ability effects,
   forms, faint and reload. Record owner-reported versus agent-run verification.
6. Persistence requires PC tail/names/fusions and maximum recorded replay,
   allocation bounds, damaged/version files and interruption fixtures; keep
   original saves untouched. AI requires hidden-information and full-restore tests.
7. Run git diff --check, git diff --stat, git status --short; inspect actual
   diff; ensure generated ROM/ELF/map/object/build artifacts are not tracked.
   Publish pre-existing warnings/failures instead of hiding them.
8. Update milestone status, matrix and evidence; missing objective acceptance
   criteria mean PARTIAL/BLOCKED, even when builds and smoke tests pass.

### Historical pre-repair audit verification record

- Normal make passed: EWRAM 230268, IWRAM 28376, ROM 26478400 bytes.
- ARM diagnostic measured current and HEAD layouts. Baseline overlay included
  global.h so its relative quoted includes did not pick current pokemon.h.
- Individually tested disabled checks: SaveBlock3 passes; PC and recording
  fail. These intentional diagnostic failures confirm blockers.
- `make check -j$(nproc) TESTS="Ripen"` compiled test ROM and exited zero but
  reported **No tests found**; it provides no behavioral validation.
- `make check -j$(nproc) TESTS="Kee Berry"` passed **5/5 tests**, including
  the two-stage Ripen case in test/battle/hold_effect/kee_berry.c. That case
  forces its primary ability and does not verify new slot initialization,
  persistence, suppression scope or the form-copy regression.
- No full suite, new manual emulator session, old-save fixture load or runtime
  high-water measurement performed. Ordinary smoke tests cannot cover blockers.
- git diff --check finds existing whitespace at src/battle_util.c:9393,9396,9411
  and src/pokemon.c:3431,3435,3438; source was not edited in this audit.
- REPORT1.md is untracked and ignored by .gitignore:73, as are context/report
  documents. Ordinary git diff --stat cannot show this edit; report change is
  checked separately with --no-index. No .gitignore/staging changes authorized.
  Track the roadmap explicitly in a future documentation task for Git handoff.
- Normal and test builds produced ignored artifacts. No tracked generated
  output changed. The tracked eight-file patch remained byte-for-byte identical
  to audit entry, verified by SHA-256 of its complete Git diff:
  a2745d7f087594b08cc9dbcd25ad732344c1c64143b9668d176871b0f284d2f2.

### MS1-R implementation verification record, 2026-10-03

- Before edits: read all four project documents, attachment precautions,
  relevant source, status/full diff/stat and last 20 commits. Independently
  reconfirmed ARM sizes and missing MonData handlers. No roadmap discrepancy
  was found; proposed the concrete repair before source changes.
- Recompiled the ARM diagnostic with AUDIT_ASSERT_SAVE3, AUDIT_ASSERT_STORAGE
  and AUDIT_ASSERT_RECORDED together: all PASS. Exact repaired sizes/offsets
  are in Sections 3/7. No persistent ability storage or MonData extension remains.
- Final `make -j$(nproc)` exited 0 with all assertions enabled, producing
  pokeemerald.gba. EWRAM 226612 (86.45%), IWRAM 28376 (86.60%), ROM 26478568
  (78.91%). No final compile warnings/errors were reported.
- `make check -j$(nproc) TESTS='MS1-R'`: **16 discovered, 16 PASS**.
  Parameterized natural selectors 0/1 and hidden selector 2; poisoned conversion
  destination; natural/hidden controller REQUEST_ALL_BATTLE packet and copy;
  switch-in; unforced Mega + party reversion; weather form + in-battle reversion;
  known/unknown AI masking and full restores with an injected extra; setter
  attribution; Skill Swap, Trace, Mummy, Wandering Spirit, Receiver/Power of
  Alchemy, Entrainment, Transform; PC/recording allocation and payload bounds.
- Test harness corrections during development: initial new file combined unit
  and battle parameter macros and included AI headers before prerequisites;
  corrected module placement/include order. A subsequent attempt to disable
  the const test-runner flag was removed in favor of using valid natural battle
  fixtures. An initial Mega test requested reversion after the runner had
  already restored the party; changed it to verify that state and added an
  actual in-battle weather reversion. Those intermediate failures were fixed;
  they are not omitted from the verification history.
- Existing file-filter runs below all discovered tests and exited 0. Counts
  refer to test definitions (parameter/trial combinations run inside them).

| TESTS filter | PASS | KNOWN_FAILING | TO_DO |
|---|---:|---:|---:|
| test/pokemon.c | 28 | 1 | 0 |
| test/save.c | 6 | 0 | 0 |
| test/battle/move_effect/skill_swap.c | 4 | 0 | 0 |
| test/battle/ability/trace.c | 10 | 0 | 0 |
| test/battle/ability/mummy.c | 3 | 0 | 0 |
| test/battle/move_effect/entrainment.c | 5 | 0 | 1 |
| test/battle/move_effect/role_play.c | 4 | 0 | 0 |
| test/battle/move_effect/doodle.c | 5 | 0 | 0 |
| test/battle/move_effect/overwrite_ability.c | 6 | 0 | 0 |
| test/battle/move_effect/transform.c | 7 | 0 | 1 |
| test/battle/ability/receiver.c | 3 | 0 | 1 |
| test/battle/form_change/mega_evolution.c | 14 | 0 | 0 |
| test/battle/ability/forecast.c | 17 | 0 | 0 |
| test/battle/move_effect/ally_switch.c | 21 | 0 | 0 |
| test/battle/ai/ai_flag_predict_ability.c | 1 | 0 | 0 |
| test/battle/ai/ai_switching.c | 138 | 1 | 0 |
| test/battle/hold_effect/kee_berry.c | 5 | 0 | 0 |
| Total across 17 file runs | 277 | 2 | 3 |

- Some MS1-R unit/bounds tests also occur in the pokemon/save file runs; do
  not add these totals and claim that many distinct tests. Unexpected failures: 0.
- Existing marked known failures were actually encountered, not counted as
  passes: learnset capacity in test/pokemon.c (21 not < 20); arbitrary-terrain
  candidate damage in test/battle/ai/ai_switching.c (party index 2 rather than 1).
  Their KNOWN_FAILING declarations predate MS1-R and were not changed.
- Reproduce each row with `make check -j$(nproc) TESTS='<exact path above>'`.
  Temporary logs: /tmp/multiability-ms1r-tests.log and
  /tmp/ms1r-test-*.log. No full-suite run was performed.
- The PC bounds test checks the final box records, all names/wallpapers and
  fusion endpoint against 35712 bytes and round-trips patterned bytes across
  3968-byte chunks in memory. The recording test checks sizeof/checksum offset
  and guarded 4096-byte copy/read allocations. These are **not flash save/reload
  or replay playback tests**. No such runtime validation is claimed.
- Source audit after repair found the only direct runtime primary assignment
  in SetBattleMonAbility; remaining `.ability =` matches are AI knowledge,
  form contexts, snapshots or test event initializers. Debug's indirect ability
  writer routes to the setter. Whole-struct/prefix copies preserve both fields.
- `git diff --check`: PASS. `git diff --stat`: 14 tracked files, 174 insertions,
  29 deletions. New test/battle/ability_slots.c is untracked (220 lines), so it
  is not included in that stat. REPORT1.md remains ignored/untracked; its update
  also does not appear in ordinary Git diff. Existing .gitignore edits retained.
- Git status: M .gitignore, include/battle.h, include/battle_util.h,
  include/pokemon.h, src/battle_ai_util.c, src/battle_controllers.c,
  src/battle_debug.c, src/battle_main.c, src/battle_script_commands.c,
  src/battle_util.c, src/pokemon.c, test/pokemon.c, test/save.c,
  test/test_runner_battle.c; ?? test/battle/ability_slots.c.
- No generated artifacts were tracked or staged. The three existing
  data/mb_*.gba assets are unchanged; root ROM/ELF/map and build outputs remain
  ignored. No commits or pushes performed.

### Reproducing compiler sizes

Create a temporary diagnostic C file with these verified declarations; no
repository source changes are needed:

```c
#include "global.h"
#include "pokemon.h"
#include "battle.h"
#include "pokemon_storage_system.h"
#include "recorded_battle.h"
#include "save.h"
#define MEASURE(type) const unsigned audit_size_##type = sizeof(struct type)
MEASURE(BoxPokemon);
MEASURE(Pokemon);
MEASURE(BattlePokemon);
MEASURE(PokemonStorage);
MEASURE(SaveBlock1);
MEASURE(SaveBlock2);
MEASURE(SaveBlock3);
MEASURE(RecordedBattleSave);
MEASURE(AI_SavedBattleMon);
const unsigned audit_ability_size = sizeof(enum Ability);
const unsigned audit_ability_count = ABILITIES_COUNT;
const unsigned audit_storage_limit = SECTOR_DATA_SIZE *
    (SECTOR_ID_PKMN_STORAGE_END - SECTOR_ID_PKMN_STORAGE_START + 1);
const unsigned audit_save3_limit = SAVE_BLOCK_3_CHUNK_SIZE * NUM_SECTORS_PER_SLOT;
const unsigned audit_recorded_limit = SECTOR_COUNTER_OFFSET;
```

If saved as /tmp/multiability-audit.c, run from the project root:

```bash
arm-none-eabi-gcc -iquote include -DMODERN=1 -DTESTING=0 -DEMERALD \
  -std=gnu17 -mthumb -mthumb-interwork -mabi=apcs-gnu \
  -mtune=arm7tdmi -march=armv4t -S /tmp/multiability-audit.c \
  -o /tmp/multiability-audit.s
rg -A 1 '^audit_.*:' /tmp/multiability-audit.s
```

Use -iquote like Makefile. -I include wrongly resolves newlib's angle-bracket
strings.h to the project header. For HEAD baseline:

```bash
mkdir -p /tmp/multiability-audit-baseline
git show 7d3d4c90ce:include/pokemon.h > /tmp/multiability-audit-baseline/pokemon.h
git show 7d3d4c90ce:include/battle.h > /tmp/multiability-audit-baseline/battle.h
cp include/global.h /tmp/multiability-audit-baseline/global.h
arm-none-eabi-gcc -iquote /tmp/multiability-audit-baseline -iquote include \
  -DMODERN=1 -DTESTING=0 -DEMERALD -std=gnu17 -mthumb -mthumb-interwork \
  -mabi=apcs-gnu -mtune=arm7tdmi -march=armv4t \
  -S /tmp/multiability-audit.c -o /tmp/multiability-audit-baseline.s
rg -A 1 '^audit_.*:' /tmp/multiability-audit-baseline.s
```

Baseline probe must omit current-only member offsets. The actual audit's
larger /tmp probe guards them with AUDIT_NEW and separately guards assertions
with AUDIT_ASSERT_SAVE3/STORAGE/RECORDED. Compile each assertion individually
with -fsyntax-only to confirm whether it passes. Future branches changing
additional layout headers must overlay those too, avoiding mixed revisions.

Focused audit searches:

```bash
rg -n 'MON_DATA_ABILITY[1-4]|box\.abilities|boxMon->abilities' src include test
rg -n 'GetBattlerAbilityAt|BattlerHasAbility|MAX_BATTLER_ABILITIES' src include
rg -n '\.ability\s*=(?!=)|->ability\s*=(?!=)' src include test --pcre2
rg -n '&[^;]*\.ability|overwrittenAbility|tracedAbility' src include
rg -n 'sizeof\(struct BattlePokemon\)|offsetof\(struct BattlePokemon|SwapStructData' src
rg -n 'STATIC_ASSERT|SAVEBLOCK_CHUNK|SECTOR_COUNTER_OFFSET' src/save.c src/recorded_battle.c include/save.h
rg -n 'migration|saveVersion|SAVE_VERSION' src include
git ls-files '*.gba' '*.elf' '*.map' '*.o' 'build/*'
```

## 11. Next Recommended Task

**MS1-R status: COMPLETE. Exactly one next architectural task: plan MS2,
stable multi-slot runtime query and state API.**

The repaired baseline is ready for an MS2 design proposal, not blanket extra
activation. Decide raw/active empty/duplicate behavior, per-candidate suppression
and bypass rules, invalid bounds and AI visibility before implementing the pilot.
The existing marked terrain AI failure remains a constraint on that pilot;
do not assume complete AI prediction parity. No MS2 code was implemented here.

Additional validation before relying on player saves/replays: interactive ROM
smoke, actual PC tail/names/fusions save/reload and recorded playback on disposable
baseline-format fixtures. Keep unsafe-MS1 saves and originals separate. Never
infer runtime serialization correctness solely from compile or in-memory bounds.
