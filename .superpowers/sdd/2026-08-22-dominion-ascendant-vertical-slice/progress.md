# SDD ledger — plan: docs/superpowers/plans/2026-08-22-dominion-ascendant-vertical-slice.md

## Environment handoff
- Canon/spec package created in ChatGPT on 2026-08-22.
- No implementation task is marked complete.
- Execution must begin at Task 1.
- Binding spec: docs/bibles/DOMINION_ASCENDANT_Vertical_Slice_Production_Spec_v1.1.md
- Engine baseline: Unreal Engine 5.8.
- Do not infer that any UE build or test has passed before running it in the Codex workspace.

## Progress

## Preflight interface and consistency scan

| Producer task | Consumer task(s) | Shared file or interface | Finding / ruling |
|---|---|---|---|
| 1 | 2–28 | UE module boundaries and `DominionTests` | Compatible; downstream module references follow the one-way map. |
| 2 | 3, 4, 5, 19 | `UDA_CardDefinition`, stable `DefinitionId`, registry | Compatible; definitions precede instance, placement, lifecycle, and authored content. |
| 3 | 5, 6, 19, 25 | `FCardInstance`, collection/deck ownership and provenance | Compatible; all consumers operate on instance IDs. |
| 4 | 5, 8, 9, 17, 20 | grid reservations and placement reason codes | Compatible; pure simulation grid is the shared authority. |
| 5 | 6, 8, 9, 15, 18, 20, 25 | `FDAWorldAssetRecord` and construction state | Compatible; later tasks mutate the record rather than actor-only state. |
| 6 | 18, 23, 25, 28 | campaign snapshot/versioned persistence | Compatible; later state additions must extend DTOs/migrations. |
| 7 | 8, 10, 11, 17, 22, 25, 27 | Development Cycle and World Tick cadence | Compatible; 30 active seconds/cycle and five cycles/tick are consistent. |
| 8 | 9, 10, 11, 17, 22, 23, 25, 27 | city wallet/output and maintenance formulas | Compatible; trace output is consumed by UI/content only. |
| 9 | 10, 17, 20, 27 | utility/road/adjacency state | Compatible; service systems must not query presentation. |
| 10 | 11, 20, 22, 27 | citizens, jobs, migration, named IDs | Compatible; later quests reference persistent `CitizenID`s. |
| 11 | 20, 22, 23 | factions, Dependency, pressure events | Compatible; narrative subscribes instead of direct quest launch. |
| 12 | 13, 14, 15, 21, 27 | Founder and play-mode state | Compatible; all modes retain one world/pawn identity. |
| 13 | 14, 15, 24 | combat attributes, damage/status types | Compatible; command, structure, and boss state consume C++ combat state. |
| 14 | 15, 23, 24, 27 | squads, morale, CP, control zones | Compatible; force route and Daxton encounter use shared tactical data. |
| 15 | 18, 23, 24, 25, 28 | structural/capture/surrender persistent outcomes | Compatible; save and quest systems use the same world asset records. |
| 16 | 17, 18, 22, 23, 25, 28 | regions, trade, diplomacy and relationship reasons | Compatible; route/event systems consume genuine regional state. |
| 17 | 22, 23, 27 | Forgeweave planner and Resource Hunger | Compatible; crisis and conquest build on the same rival state. |
| 18 | 20, 22, 23, 25, 28 | quest graphs, history and promise ledgers | Compatible; authored content remains data-driven. |
| 19 | 20, 21, 25, 26 | frozen content IDs/counts/deck | Compatible; later content modifies only frozen definitions or presentation bindings. |
| 20 | 22, 23, 28 | first-hour/Nia quest state | Compatible; Nia Champion hook remains deferred until later crisis. |
| 21 | 26, 28 | 27 UI surfaces, overlays, controller/accessibility registry | Compatible; presentation reads view-model/service state only. |
| 22 | 23, 24, 28 | events and Foundry Shortage outcomes | Compatible; changes routes/Leader dialogue via history/state. |
| 23 | 24, 25, 28 | conquest meters and route history | Compatible; Daxton and Ascension consume route outcome. |
| 24 | 25, 28 | Daxton Leader outcome and encounter state | Compatible; Ascension persists it atomically. |
| 25 | 26, 27, 28 | Ascension/relic/doctrine/fusion state | Compatible; presentation and release tests consume completed authoritative transaction. |
| 26 | 27, 28 | presentation coverage manifest | Compatible; no gameplay ownership moves into assets. |
| 27 | 28 | simulation LOD/benchmark and soak results | Compatible; release gate aggregates results. |

| Task | Internal consistency check | Finding |
|---|---|---|
| 1 | test → module rules → module load | Consistent. |
| 2 | validation test → definition/registry implementation | Consistent. |
| 3 | deck tests → persistent instances/deterministic draw | Consistent. |
| 4 | placement tests → pure grid and codes | Consistent. |
| 5 | lifecycle test → persistent record/construction callbacks | Consistent. |
| 6 | round trip/migration tests → DTO/migration/transactional save | Consistent. |
| 7 | deterministic time tests → accumulated simulation clock | Consistent. |
| 8 | formula tests → traceable resolver/boundedness | Consistent. |
| 9 | graph tests → cached graph/spatial hash | Consistent. |
| 10 | workforce/migration tests → deterministic citizen systems | Consistent. |
| 11 | Dependency tests → factions/threshold signals | Consistent. |
| 12 | mode transition test → unified Founder/City/Command controller | Consistent. |
| 13 | damage/downed tests → GAS foundation | Consistent. |
| 14 | command/morale tests → squads/cover/CP | Consistent. |
| 15 | module/capture/surrender tests → persistent damage outcomes | Consistent. |
| 16 | trade test → regional state/travel/diplomacy | Consistent. |
| 17 | build-pool/soak tests → rival planner/resource hunger | Consistent. |
| 18 | graph/asset-loss/promise tests → narrative runtime | Consistent. |
| 19 | count/deck tests → frozen authored definitions | Consistent. |
| 20 | first-hour test → persistent objective bindings | Consistent. |
| 21 | UI/controller tests → frozen screens/accessibility | Consistent. |
| 22 | event trigger/ignored-event tests → systemic crisis | Consistent. |
| 23 | meter/four-route tests → conquest state | Consistent. |
| 24 | phase/Leader matrix tests → systemic encounter | Consistent. |
| 25 | atomic Ascension/save-reload tests → transaction/unlocks | Consistent. |
| 26 | coverage manifest → presentation bindings | Consistent. |
| 27 | soak/benchmark tests → simulation LOD/performance | Consistent. |
| 28 | fixtures/regression/release evidence → final gate | Consistent. |

## Rulings

- Ruling: The supplied remote was an empty repository, so a local handoff-only root commit (`6beae69`) was created before the implementation branch/worktree. — This preserves the binding plan/spec in version control without implementing on `main`. — Cost if wrong: the eventual PR includes a bootstrap commit in its ancestry.
- Ruling: This workspace has no Unreal Engine 5.8 editor, Windows build batch scripts, or valid `.uasset` authoring runtime. Implementers must write the requested UE C++/Automation artifacts test-first, record the exact blocked UE command and expected failure, and must not fabricate passing engine or binary-asset results. — This preserves real UE targets and avoids false verification. — Cost if wrong: UE compilation, Automation execution, cooked assets, performance captures, and controller playtests remain unverified until a UE 5.8 Windows runner executes them.
- Ruling: The plan remains executable as a source implementation program despite the absent UE runtime; independent reviewers must gate specification and code quality after each task, while marking unavailable engine verification explicitly rather than treating it as passing. — This follows the user's continuous-execution request without misrepresenting evidence. — Cost if wrong: a later engine integration may expose compile/API/asset defects that require rework.
- Ruling: Task 5 must add a `ConstructionCycles` field to `UDA_CardDefinition` even though Task 2 lists only minimum fields, because Task 5 expressly requires construction to complete after the definition's count. — The later task's explicit consumer contract binds the earlier extensible schema. — Cost if wrong: a future content schema revision might choose a different timing representation and require migration.
- Ruling: Because the vertical slice is Windows-PC-only, Task 6's replacement path will use a Windows same-directory replacement primitive with backup semantics, plus startup recovery, rather than composing delete/rename operations through `IPlatformFile`. — This is the smallest path that satisfies the required atomic-replacement intent while retaining a testable fallback/recovery policy. — Cost if wrong: non-Windows editor/tooling builds need an explicit tested replacement adapter before support is claimed.
- Ruling: Task 8's boundedness test must not invent v0.8 card-specific output/cost tuning that is only authored in Task 19. It will prove boundedness using explicit scenario inputs and target-range assertions, while Task 19/27/28 will bind campaign tuning to real authored content. — This keeps formula tests honest and preserves future content as the source of facility tuning. — Cost if wrong: a later authored-content integration test may need tighter first-hour balance adjustments.
- Ruling: Tasks 19–26 cannot honestly create valid Unreal binary `.uasset`, `.umap`, texture, audio, animation, Niagara, or UI packages without a UE 5.8 Editor/content toolchain. They will deliver source-controlled canonical manifests, runtime/editor import or generation code, validation tests, and binding/coverage contracts; no fake binary packages will be fabricated. — This preserves verifiable content authority and makes missing editor generation explicit. — Cost if wrong: the actual packaged maps, widgets, media, controller navigation, art/audio/VFX, and presentation proof remain ungenerated and require a UE 5.8 content-authoring pass before release-candidate status.

## Active task

- Task 1: started — base `5dec6b0` — implementer `/root/task_01_impl` — brief `task-1-brief.md`.
- Task 1: complete (commits `5dec6b0..2534cbd`, review clean; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 2: started — base `2534cbd` — implementer `/root/task_02_impl` — brief `task-2-brief.md`.
- Task 2: fix round 1/5 started — open: reject duplicate primary IDs; remove public test-only registry seam; isolate duplicate-ID test.
- Task 2: fix round 1/5 (3 addressed, 0 open — duplicate card definitions are no longer addressable; test seam removed; fixtures isolated; commits `12e90a4..2f0de9b`).
- Task 2: complete (commits `2534cbd..2f0de9b`, review clean after fix round 1; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 3: started — base `2f0de9b` — implementer `/root/task_03_impl` — brief `task-3-brief.md`.
- Task 3: minor (deferred): `FDADeckState` holds a non-owning collection pointer; Task 6 save/load must bind safely after reconstruction and final review must assess lifetime hardening.
- Task 3: complete (commits `2f0de9b..b1252cd`, review clean; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 4: started — base `b1252cd` — implementer `/root/task_04_impl` — brief `task-4-brief.md`.
- Task 4: complete (commits `b1252cd..e73a336`, review clean; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 5: started — base `e73a336` — implementer `/root/task_05_impl` — brief `task-5-brief.md`.
- Task 5: fix round 1/5 started — open: definition-owned construction cycles; enforce card-instance/world-asset relationship; callback/reconstruction coverage.
- Task 5: fix round 1/5 (3 addressed, 0 open — construction timing derives from card definitions, instance linkage enforced, callback/reconstruction cases covered; commits `bf1a45e..48b96d9`).
- Task 5: complete (commits `e73a336..48b96d9`, review clean after fix round 1; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 6: started — base `48b96d9` — implementer `/root/task_06_impl` — brief `task-6-brief.md`.
- Task 6: fix round 1/5 started — open: crash-safe atomic replacement/recovery, full-envelope checksum, public v1 load migration, transaction boundary coverage, integral schema versions.
- Task 6: fix round 1/5 (5 addressed, 1 new Important open — duplicate local `Card` identifier in save migration spec; commits `a3e548b..b4aa1e1`).
- Task 6: fix round 2/5 started — open: rename duplicate local test identifier to restore C++ compilation.
- Task 6: fix round 2/5 (1 addressed, 0 open — SaveMigrationSpec local identifier collision resolved; commits `b4aa1e1..5d5717e`).
- Task 6: complete (commits `48b96d9..5d5717e`, review clean after two fix rounds; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 7: started — base `5d5717e` — implementer `/root/task_07_impl` — brief `task-7-brief.md`.
- Task 7: complete (commits `5d5717e..e259b90`, review clean; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 8: started — base `e259b90` — implementer `/root/task_08_impl` — brief `task-8-brief.md`.
- Task 8: fix round 1/5 (1 addressed, 0 open — boundedness fixture no longer claims unsupported v0.8 authored tuning; commits `c998983..f26daff`).
- Task 8: complete (commits `e259b90..f26daff`, review clean after fix round 1; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 9: started — base `f26daff` — implementer `/root/task_09_impl` — brief `task-9-brief.md`.
- Task 9: complete (commits `f26daff..a83bcd5`, review clean; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 10: started — base `a83bcd5` — implementer `/root/task_10_impl` — brief `task-10-brief.md`.
- Task 10: fix round 1/5 (1 addressed, 0 open — persistent job IDs cleared/rebuilt; commits `b64effa..04dc1eb`).
- Task 10: complete (commits `a83bcd5..04dc1eb`, review clean after fix round 1; UE Automation remains environment-blocked).
- Task 11: fix round 1/5 (2 addressed, 0 open — full v0.8 Dependency terms and isolated tests; commits `280d756..1696e8b`).
- Task 11: complete (commits `04dc1eb..1696e8b`, review clean after fix round 1; local static verification passed; UE Automation remains environment-blocked).
- Task 12: started — base `1696e8b` — implementer `/root/task_12_impl` — brief `task-12-brief.md`.
- Task 12: fix round 1/5 (1 addressed, 0 open — Command Mode uses the canonical 0.20 simulation multiplier; commits `e2c4343..cf9fa95`).
- Task 12: complete (commits `1696e8b..cf9fa95`, review clean after fix round 1; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 13: started — base `cf9fa95` — implementer `/root/task_13_impl` — brief `task-13-brief.md`.
- Task 13: fix round 1/5 started — open: use real ASC/GameplayEffect test fixture; eliminate direct damage-mutation bypass; load one external ability-definition source; make Downed expiry authoritative and tested; validate tag-based damage channels; add required GameplayEffect callback include.
- Task 13: fix round 1/5 (3 addressed, 3 Important still open — fixture registration, production authority/timer driver for Downed expiry, and valid packaged JSON staging; commits `7b93eb0..e2e480d`).
- Task 13: fix round 2/5 started — open: registered world actor/component fixture; server-authoritative runtime Downed expiry driver; configure NonUFS staging in project packaging settings.
- Task 13: fix round 2/5 (3 addressed, 0 open — registered world fixture, authority-ticked Downed expiry, and valid packaging settings; commits `e2e480d..9cebebc`).
- Task 13: complete (commits `cf9fa95..9cebebc`, review clean after two fix rounds; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 14: started — base `9cebebc` — implementer `/root/task_14_impl` — brief `task-14-brief.md`.
- Task 14: fix round 1/5 started — open: authoritative immutable direct-control ownership/classification with regressions; source-aware cover unregister/re-registration; valid GameInstanceSubsystem test fixture.
- Task 14: fix round 1/5 (3 addressed, 2 new Important open — weak-pointer incomplete type and direct-order state not reset on release/unregister; commits `be23ef7..51864aa`).
- Task 14: fix round 2/5 started — open: compile-safe subsystem ownership representation and tested doctrine fallback/reset for release and unregister.
- Task 14: fix round 2/5 (2 addressed, 0 open — compile-safe ownership and shared tested doctrine fallback; commits `51864aa..9aab837`).
- Task 14: complete (commits `9cebebc..9aab837`, review clean after two fix rounds; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 15: started — base `9aab837` — implementer `/root/task_15_impl` — brief `task-15-brief.md`.
- Task 15: fix round 1/5 started — open: persist/rebind structural/capture/surrender DTOs by stable IDs with save/reload tests; separate integrity state from functional module disablement; authorize Gift recipients; revalidate active capture role/presence/security each advance.
- Task 15: fix round 1/5 (4 addressed, 4 new Important open — Salvage aggregate consistency, single-snapshot transaction authority, cached array-element pointer lifetime, and shared Core eligibility validation; commits `48e5ff4..e5d8bf9`).
- Task 15: fix round 2/5 started — open: aggregate Salvage structural mutation/round-trip; snapshot-owned capture/surrender transactions; ID-only re-resolution after array growth; canonical eight-definition eligibility in Core/save validation.
- Task 15: fix round 2/5 (4 addressed, 0 open — aggregate Salvage, snapshot-authoritative mutations, stable-ID re-resolution, and shared Core eligibility; commits `e5d8bf9..2364da7`).
- Task 15: complete (commits `9aab837..2364da7`, review clean after two fix rounds; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 16: started — base `2364da7` — implementer `/root/task_16_impl` — brief `task-16-brief.md`.
- Task 16: fix round 1/5 started — open: route travel time through simulation-clock authority/listeners; integrate world campaign state into canonical save/migration/validation with interrupted-handoff round-trip; replay ephemeral load/reconstruction after restore; valid subsystem lifecycle fixture; atomic destination goods receipt/demand mutation.
- Task 16: fix round 1/5 (5 addressed, 5 new blockers open — canonical JSON key casing, partial-travel restart tick overshoot, missing development-cycle broadcasts, overflow/enum validation, and remaining invalid subsystem fixtures; commits `6c80537..2b529fb`).
- Task 16: fix round 2/5 started — open: centralized reflected JSON keys/migration test; remaining-tick travel resume; normal cycle/world-tick event ordering; checked int64 validation and stage range; reusable valid GameInstance fixtures across touched suite.
- Task 16: fix round 2/5 (5 addressed, 1 new Important open — checkpoint test/persistence listeners rely on undefined multicast delegate order; commits `2b529fb..d11dbbd`).
- Task 16: fix round 3/5 started — open: explicit post-commit world-state event used for safe persistence/checkpoint capture.
- Task 16: fix round 3/5 (1 addressed, 0 open — immutable post-commit world-state snapshots remove delegate-order assumptions; commits `d11dbbd..a0a03f6`).
- Task 16: complete (commits `2364da7..a0a03f6`, review clean after three fix rounds; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 17: started — base `a0a03f6` — implementer `/root/task_17_impl` — brief `task-17-brief.md`.
- Task 17: fix round 1/5 started — open: prevent clock/world divergence on tick rejection; bind repair/trade/defense decisions to canonical authorities; use canonical 32×32/8m Ironheart grid; make unexplained-deadlock guard reachable/tested; require fully empty uninitialized Forgeweave state; lossless/restorable v4 tick migration bounds.
- Task 17: fix round 1/5 (6 addressed, 5 new blockers open — precommit veto/reentrancy protocol, canonical campaign repair/reconstruction integration, lexical exact tick migration, durable atomic transaction reconciliation, and natural-tick overflow/nonfinite guards; commits `c17ca86..3e73e91`).
- Task 17: fix round 2/5 started — open: monotonic veto+abort+reentrancy guard; shared campaign records and real repair/cover reconstruction; raw exact v4 tick parsing; reconciled transaction evidence/tamper rejection; checked clock arithmetic and finite accumulation.
- Task 17: fix round 2/5 (5 addressed, 3 Important open — repair reconciliation rejects intervening damage, wire-format change needs schema v6/v5 migration, and inverse/bijective authority checks missing for cover/build/order; commits `3e73e91..2c92813`).
- Task 17: fix round 3/5 started — open: damage-aware repair reconciliation; v6 schema with nontrivial v5 promotion; actor/decision/transaction bijection and stale-cover unregister.
- Task 17: fix round 3/5 (3 addressed, 4 Important open — case-sensitive planner order namespace, cross-region/full-transform actor inverse validation, destructive malformed-v5 damage discard, and non-transactional cover rollback; commits `2c92813..f118798`).
- Task 17: fix round 4/5 started — open: authority-derived planner order inverse; all-region exact-transform construction/defense bijection; validate complete historical v5 shape before promotion; preflight/transactional cover reconstruction.
- Task 17: fix round 4/5 (2 addressed, 2 Important still open — inverse trade membership still prefix-gated, actor inverse still definition-gated instead of canonical-ID union; commits `f118798..9a351fb`).
- Task 17: fix round 5/5 started — open: validate every spot order/delivery against authority-derived membership; actor inverse keyed by union of canonical planner actor/building/WorldAsset IDs with full tuple checks.
- Task 17: fix round 5/5 (2 addressed, 0 open — authority-derived trade and actor identity bijections; commits `9a351fb..87a5379`).
- Task 17: complete (commits `a0a03f6..87a5379`, review clean after five fix rounds; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 18: started — base `87a5379` — implementer `/root/task_18_impl` — brief `task-18-brief.md`.
- Task 18: fix round 1/5 started — open: consolidate campaign history authority; persist definition fingerprints; strict narrative enum/state/definition validation; semantic action replay records; executable typed node payloads and explicit Choice selection; mutate/validate full campaign aggregate.
- Task 18: fix round 1/5 (6 addressed, 4 blockers open — inverse promise/action provenance, narrative action→top-level history reconciliation, legal ordered completed paths, and platform-stable fingerprint encoding; commits `74e3b3c..829d582`).
- Task 18: fix round 2/5 started — open: bidirectional promise/action identity including legacy provenance; action/history reconciliation; path-valid completion trace; UTF-8/canonical-double definition fingerprint.
- Task 18: fix round 2/5 (4 addressed, 3 Important open — result semantics allow simultaneous conflict/fulfillment and noncanonical tag; v8 migration assigns legacy provenance campaign-globally; inactive nonfinite payload fields enter fingerprints; commits `829d582..4aa99e0`).
- Task 18: fix round 3/5 started — open: exact canonical result tag semantics; per-promise legacy provenance proof; finite/canonical inactive payload encoding and native-v8 fingerprint validation before refresh.
- Task 18: fix round 3/5 (3 addressed, 0 open — exact result semantics, per-promise legacy provenance, and canonical fingerprint payloads; commits `4aa99e0..c0e8c2a`).
- Task 18: complete (commits `87a5379..c0e8c2a`, review clean after three fix rounds; local static verification passed; UE 5.8 Automation remains environment-blocked).
- Task 19: started — base `c0e8c2a` — implementer `/root/task_19_impl` — brief `task-19-brief.md`.
- Task 19: fix round 1/5 started — open: preserve authored presence for every balance field and Founder Hall housing 24; stable primary asset IDs by DefinitionId; manifest-authoritative generated cache completeness/fingerprint; strict production parser/schema contract.
- Task 19: fix round 1/5 (4 addressed, 3 Important open — combat/tags/upgrade branches omitted from author authority/cache parity; fingerprint line-ending instability; public construction authorship mutator; commits `f16d374..7e0664f`).
- Task 19: fix round 2/5 started — open: canonical authored combat/tags/upgrade data and parity; normalized checkout-independent manifest fingerprint; restrict authored mutation to manifest mapper/import path.
- Task 19: fix round 2/5 (3 addressed, 0 open — canonical combat/tags/upgrades, stable fingerprints, and mapper-only authored mutation; commits `7e0664f..c62a53f`).
- Task 19: complete (commits `c0e8c2a..c62a53f`, review clean after two fix rounds; static manifest suite 9/9 and diff verification passed; UE 5.8 generation/Automation remains environment-blocked; zero fake `.uasset` files).
- Task 20: started — base `c62a53f` — implementer `/root/task_20_impl` — brief `task-20-brief.md`.
- Task 20: fix round 1/5 started — open: canonical Nia/asset/utility/job/faction bindings and production progression API; exact authored nine graphs/history/rewards without invented values; cross-language fingerprint plus full semantic/cache parity; tamper-safe schema v10 migration/invariants/replay/Human Override authority; real-signal exhaustive tests.
- Task 20: fix round 1/5 (5 groups partially addressed, 8 blocker groups open — recomputed frozen semantic/cache parity; payload-exact unlock/binding save invariants; Replacement aggregate/replay reconciliation; shared real authority mutation; exact bound signal gates; canonical causal Human Override; production cache wiring/full authored graph; typed migration and full-chain tests; commits `3f09fa7..e1d0d66`).
- Task 20: fix round 2/5 started — open: close all frozen content, save authority, systemic integration, causal eligibility, runtime wiring, graph completeness, migration typing, and exhaustive progression/tamper test gaps.
- Task 20: fix round 2/5 (major content/cache improvements, 8 blocker groups still open — causal Human Override transition; real not shadow systemic authorities; missing save invariants/path checks; spoofable gates; replay ticks; v9 default migration; production coordinator; targeted regressions; commits `e1d0d66..c0a7b2b`).
- Task 20: fix round 3/5 started — open: causal story/crisis ledger; mutate real campaign diplomacy/faction/dependency/employment/economy authorities; fingerprinted/timed binding and branch invariants; canonical facility/world-map/audit gates; tick-exact replay; authentic v9 fixture; production coordinator; exhaustive blocker tests.
- Task 20: fix round 3/5 (many addressed, 8 blockers open — coordinator duplicates campaign/city authority; v8→v9→v10 migration chain; no production invocation/save/clock path; fabricated map/audit provenance; story replay ordering; exact binding timing; generated-cache GC/path safety; audit proof identity replay; commits `c0a7b2b..dc14e1b`).
- Task 20: fix round 4/5 started — open: shared world campaign owner/participant and city/citizen synchronization; repair migration chain; clock-bound production coordinator APIs; typed map/audit authorities; replay ordering; canonical binding tick derivation; GC-safe exact-package cache; proof-ID exact replay.
- Task 20: fix round 4/5 (many addressed, 4 blockers open — live economy/citizen/utility state still split/unsaved; no production bootstrap/explicit-action ingress; v9 binding-field rejection conditional/authentic legacy fixture gap; nine canonical quest fingerprints not pinned in Core validation; commits `dc14e1b..e95a59b`).
- Task 20: fix round 5/5 started — open: persist and share canonical city/utility/citizen signals with automatic commit events; auto-bootstrap plus reflected production action ingress; unconditional historical field rejection and authentic legacy fingerprint chain; fixed Core fingerprint policy for all nine definitions/bindings/rewards.
- Task 20: fix round 5/5 (most addressed, 3 Important open at breaker — development-cycle live-signal commits conflict with staged world ticks/restore loses sub-tick state; general campaign CAS bypasses signal revision and lacks overflow guards; v8/v9 migration does not reject future `liveSignals`; commits `e95a59b..f17d2d8`).
- Task 20: parked — staged world-tick protocol currently rejects boundary-cycle signal commits and restore omits cycle/accumulator authority. — Ruling: real and load-bearing for simulation/UI consistency; Task 21 must first add transactional staged-cycle integration and exact clock-state restore before consuming live signals. — Cost if wrong: economy progression diverges across natural/strategic/save paths.
- Task 20: parked — general campaign CAS can bypass `LiveSignals.MutationRevision` and signal increments can overflow. — Ruling: real and load-bearing; Task 21 must harden all campaign commit paths with monotonic checked signal revisions before UI binds to them. — Cost if wrong: stale or forged signal state can be published.
- Task 20: parked — historical v8/v9 migration can silently discard future `liveSignals`. — Ruling: real and load-bearing for release save compatibility; Task 21 must reject any future-field presence before current DTO conversion and add raw source-version regressions. — Cost if wrong: future state can be laundered through an old schema.
- Task 20: complete (commits `c62a53f..f17d2d8`, 3 parked at round-5 breaker with explicit carry-forward rulings; static Task19+20 suite 35/35 and diff verification passed; UE 5.8 generation/Automation remains environment-blocked; zero fake `.uasset` files).
- Task 21: started — base `f17d2d8` — implementer `/root/task_21_impl` — brief `task-21-brief.md`; includes three load-bearing Task 20 breaker rulings as mandatory prerequisites.
- Task 21: fix round 1/5 started — open: clock must veto/abort on signal/economy overflow and campaign CAS must pin exact live clock authority; functional focusable/action-bound widgets and real input mappings; router owns CommonUI stacks/focus/back/layers/input; accessibility consumers/broadcast; fingerprinted exact cache validation; bound full viewmodels/reason ledgers and overlay runtime; exact manifest semantics; tracked command-service lifetime.
- Task 21: fix round 1/5 (many addressed, 9 blocker groups open — CommonUI AddWidget compile; actual Enhanced/CommonUI bindings/bootstrap/back; real stack/player/router tests; rendered/refreshed complete projections; typed conquest/overlay models; real accessibility consumers/reload; exact cache action hashes/parity; owned services; double→int64 clock overflow; packaged manifest/cook; commits `807f27a..341a02d`).
- Task 21: fix round 2/5 started — open: compile-safe CommonUI container API and pre-add setup; production UI subsystem/input actions/back; functional data/action rendering and refresh; typed overlays/conquest; concrete accessibility adapters; exact cache provenance; strong production ownership; safe integer clock math; package staging/cooking.
- Task 21: fix round 2/5 (many addressed, 9 blocker groups open — no production executor/initial screen; typed models not rendered/fed; wrong generic target payloads; accessibility adapters lack side effects; manifest not staged as NonUFS; numeric key identity loss/collisions; no real back input; incomplete BP/action semantic cache checks; production input/button/startup tests shallow; commits `341a02d..d0bda8c`).
- Task 21: fix round 3/5 started — open: default router/gameplay command bridge and bootstrap; authoritative rendered typed models/payloads; concrete camera/input/gameplay accessibility effects; Content-relative staged manifest; unique per-binding input identity/back; editor semantic cache introspection; real production-path interaction tests.
- Task 21: fix round 3/5 (several addressed, 6 blocker groups open — no production endpoint; broad/incomplete typed surfaces/payloads/live subscriptions; trigger/collision/remap/back input semantics; incomplete accessibility system integration; startup hard-requires absent generated cache; tests not true integration; commits `d0bda8c..0c4a967`).
- Task 21: fix round 4/5 started — fresh implementer required by breaker escalation; open: authoritative gameplay endpoint/orchestrator; screen-specific complete typed projections and executable payloads; collision-free pressed input/remap/back graph; concrete subsystem accessibility consumers; manifest-native startup fallback; production-style integration/commandlet/cache tests.
- Task 21: fix round 4/5 (many infrastructure pieces addressed, 9 blocker groups open — endpoint local/no-op authorities; impossible placement; incomplete/index-insensitive payloads; BackTarget ignored; missing live projection fields/selections/leader/overlay rows; readback-only accessibility/remap; job capacity accounting; fake travel handoff; unsafe treaty parse and shallow endpoint tests; commits `0c4a967..50147c1`).
- Task 21: fix round 5/5 started — fresh final implementer; open: canonical-service-only commands and fail-closed deferred services; real placement/travel/job mutations; index-complete validated payloads; exact back graph; complete live models/rows; downstream accessibility side effects/remap; robust parsers and production mutation tests.
- Task 21: fix round 5/5 (many addressed, 6 blocker groups open at breaker — placement fabricates claimed grid/zero construction timing; HUD sources can be unregistered/stale; accessibility fanout is readback-only/non-atomic; history/metrics/founder false success; rotation/craft validation; incomplete indexed/treaty payloads and invalid production tests; commits `50147c1..d033cf7`).
- Task 21: parked — placement path fabricates claims and can commit unreconstructable zero-cycle assets. — Ruling: real and load-bearing; Task 22 must bind placement to persisted canonical grid claims and fail if timing is unauthored, except where v0.8 explicitly supplies an authoritative construction/lifecycle duration. — Cost if wrong: invalid WorldAssets enter saves.
- Task 21: parked — live HUD source registration/staleness and accessibility side effects/rollback are incomplete. — Ruling: real and load-bearing for event warnings; Task 22 must require registered live sources, clear expired authority, and make accessibility fanout transactional with concrete runtime consumers. — Cost if wrong: UI presents stale state or partially applies settings.
- Task 21: parked — history/metrics/founder interaction can report false success. — Ruling: real; Task 22 must consume typed selections/interaction IDs through authoritative handlers or fail closed. — Cost if wrong: input confirms actions that do nothing.
- Task 21: parked — payload validation/indexing/treaty mutations are incomplete. — Ruling: real; Task 22 must validate before narrowing/multiplication, bound quantities, make per-device indices record-complete, and generate fresh treaty mutation IDs. — Cost if wrong: wrong targets, overflow, or duplicate treaty failures.
- Task 21: parked — endpoint production tests contain invalid facilities/diagnostics. — Ruling: real; Task 22 must replace them with snapshot-valid fixtures and exact error assertions before relying on them. — Cost if wrong: UI integration remains statically green but behaviorally unproven.
- Task 21: complete (commits `f17d2d8..d033cf7`, 5 parked groups at round-5 breaker with mandatory Task 22 carry-forward; static suites 52/52 and diff verification passed; UE 5.8 UI generation/Automation remains environment-blocked; no fake `.uasset` files).
- Task 22: started — base `d033cf7` — implementer `/root/task_22_impl` — brief `task-22-brief.md`; includes all Task 21 breaker rulings as mandatory prerequisites.
- Task 22: fix round 1/5 started — open: resolved transaction validation must not freeze mutable simulation; canonical Tal/Mara IDs; production coordinator/runtime for all ten quests/six events and outcome-specific dialogue; exact cache scope/systems/installed validation/cook/path parity; complete live Founder/Command HUD; concrete remaining accessibility consumers; controller cycling/reachability for all cards/abilities/squads.
- Task 22: fix round 1/5 (many addressed, 7 blocker groups open — no generated cache/pre-cook hook and ValidateOnly empty; Foundry quest choice bypasses crisis transaction; generic/wrong triggers/incomplete 6-event/10-quest lifecycles; outcome dialogue not consumed/choice-specific; scope/systems not frozen; cycle-selected card activation rejected; hold/toggle and build snap no-op; commits `aed4603..e0efd36`).
- Task 22: fix round 2/5 started — open: regional pre-cook generation/native fallback validation; atomic Foundry quest+crisis resolution; manifest-driven exact event/quest graphs/triggers/lifecycles; production dialogue condition service; full semantic pins; selected activation; input trigger modes and fractional snap.
- Task 22: fix round 2/5 (5 addressed, 2 Important open — non-Foundry triggers/effects remain hard-coded instead of manifest-driven; press/hold/toggle mappings and fractional build snap are not behaviorally effective; commits `e0efd36..9a0e6a1`).
- Task 22: fix round 3/5 started — open: interpret all manifest trigger/effect contracts through canonical campaign authorities; implement functional press/hold/toggle input identity/state and fractional placement snapping with behavioral regressions; append exact covering test evidence to the report.
- Task 22: fix round 3/5 (1 addressed, 2 Important open — report evidence is current, but ignored event-backed quests can become active after their event is terminal and cannot complete; default Toggle on direct building placement suppresses every second legitimate placement; commits `9a0e6a1..4ef5ffd`).
- Task 22: fix round 4/5 started — fresh implementer escalation; open: make ignored event-backed quest lifecycle terminal/finishable from authored state; assign/consume Toggle only for genuinely stateful actions so direct placement dispatches every valid press.
- Task 22: fix round 4/5 (3 addressed, 0 open — ignored event-backed quests terminate idempotently; direct placement dispatches every press while Toggle remains stateful-only; exact verification evidence recorded; commits `4ef5ffd..6749c72`).
- Task 22: complete (commits `d033cf7..6749c72`, review clean after four fix rounds; independent static suite 64/64 and diff verification passed; UE 5.8 generation/Automation remains environment-blocked; zero fake `.uasset` files).
- Task 23: started — base `6749c72` — implementer `/root/task23_impl` — brief `task-23-brief.md`.
- Task 23: minor (deferred): report incorrectly says Task 24 adds the 25th quest; binding plan assigns Convergence Authority to Task 25.
- Task 23: fix round 1/5 started — open: connect conquest synchronization/completion to a production authoritative campaign/world entry point; bind every mutation to unique canonical evidence and reject reuse; re-prove persisted route resolution against canonical route gates with forged-save regressions.
- Task 23: fix round 1/5 (3 addressed, 3 Important open — production wiring exists but lacks a successful authoritative-owner completion regression; per-reason clamping diverges from signed aggregate diplomacy and can veto valid ticks; later worker evidence can invalidate an earlier legitimate consumed source; commits `c29bd01..4686b2e`).
- Task 23: fix round 2/5 started — open: prove successful route commit through production owner; make relationship projection aggregate-equivalent for signed histories; validate immutable worker evidence against the exact historical source rather than the current preferred source.
- Task 23: fix round 2/5 (3 addressed, 1 Important open — production success, signed histories, and immutable worker sources are fixed; history-first joint-crisis evidence is retroactively invalidated/double-counted if a canonical crisis record appears later; commits `4686b2e..c718fff`).
- Task 23: fix round 3/5 started — open: make joint-crisis history and later canonical crisis evidence order-independent, exact-once, and canonical without rewriting immutable prior ledger entries.
- Task 23: fix round 3/5 (most addressed, 1 Important open — same-tick history-first evidence can still be retroactively treated as crisis-aware because tick alone does not encode within-tick order, vetoing a valid campaign; commits `c718fff..9273d2a`).
- Task 23: fix round 4/5 started — fresh implementer escalation; open: encode or derive stable same-tick causal ordering for joint-history/crisis evidence and cover it through full campaign validation/world-tick preparation.
- Task 23: fix round 4/5 (causal model/migration addressed, 2 Important open — current v15 load does not require explicit integer causal-proof field before permissive struct conversion; crisis-first save/reload lacks a subsequent authoritative-owner tick regression; commits `9273d2a..a59ab07`).
- Task 23: fix round 5/5 started — fresh final implementer; open: require exact integer causal proof on every current-schema resolution record with missing/fractional tamper rejection; prove crisis-first save/reload survives later owner tick.
- Task 23: fix round 5/5 (proof presence/types and crisis-first retick addressed, 1 Important open at breaker — a Unicode-escaped duplicate JSON key can bypass raw-token duplicate detection; commits `a59ab07..28fd856`).
- Task 23: parked — Unicode-escaped duplicate `jointCrisisHistoryRevisionAtResolution` keys can bypass current-v15 raw duplicate detection. — Ruling: real and load-bearing for save integrity; Task 24 must replace literal-token counting with decoded object-member duplicate detection (or reject duplicate decoded keys during parsing) and add escaped-key/container tamper regressions before adding Daxton state to the save. — Cost if wrong: an ambiguous checksummed v15 document can enter permissive conversion and undermine causal-proof uniqueness.
- Task 23: complete (commits `6749c72..28fd856`, 1 parked at round-5 breaker; independent static suite 68/68 and diff verification passed; UE 5.8 Automation remains environment-blocked; zero fake `.uasset` files).
- Task 24: started — base `28fd856` — implementer `/root/task24_impl` — brief `task-24-brief.md`; includes Task 23 escaped-key save-integrity breaker ruling as mandatory prerequisite.
- Task 24: fix round 1/5 started — open: production owner/CAS entry for every action with live-signal revisions; complete canonical effect replay and tamper-proof resolved saves; Phase-I equivalent non-damage path; end-to-end reachability/save/failure tests; normal asset-registry/cook integration.
- Task 24: fix round 1/5 (all prior findings addressed, 1 Important open — resolved encounter validation pins the entire campaign projection and blocks legitimate later citizen/job/history/economy/relationship evolution; commits `de9df8b..f810243`).
- Task 24: fix round 2/5 started — open: scope terminal replay to immutable encounter-owned effects/baselines while allowing unrelated canonical post-resolution changes; retain tamper detection and add post-resolution owner mutation/save-load regressions.
- Task 24: fix round 2/5 (evolution lock addressed, 2 Important open — terminal relationship eligibility is self-authenticating and can be coherently rewritten without canonical reason-ledger anchors; post-evolution tamper matrix omits cover, reinforcement, signal baseline, removed assignment, Leader state, and relationship proof; commits `f810243..7b27ec6`).
- Task 24: fix round 3/5 started — open: anchor terminal Trust/Respect/Grievance to exact canonical relationship-reason prefix/provenance at resolution; complete post-evolution recomputed-checksum tamper matrix for all encounter-owned facts.
- Task 24: fix round 3/5 (proof/tamper matrix addressed, 1 Important open — same-tick post-resolution relationship suffix is rejected, and v16 migration incorrectly folds all reasons at the resolved tick into the prefix; commits `7b27ec6..345a17a`).
- Task 24: fix round 4/5 started — fresh implementer escalation; open: use persisted ordered prefix for v17 regardless of same-tick suffix, and derive authentic v16 prefix by ordered replay match to terminal projection with resolved same-tick migration fixture.
- Task 24: fix round 4/5 (1 addressed, 0 open — v17 accepts ordered same-tick suffixes; v16 migration derives a unique replay-valid terminal prefix and rejects no-match/ambiguity; commits `345a17a..f191c76`).
- Task 24: complete (commits `28fd856..f191c76`, review clean after four fix rounds; Task23 escaped-key prerequisite closed; independent static suite 54/54 and diff verification passed; UE5.8 Automation/cook remains environment-blocked; zero fake `.uasset` files).
- Task 25: started — base `f191c76` — implementer `/root/task25_impl` — brief `task-25-brief.md`.
- Task 25: fix round 1/5 started — prior implementer unavailable after session resumption; fresh implementer `/root/task25_fix1`; open: authored/runtime Ascension sequence; production Factory workforce/throughput/adjacency wiring; deterministic multi-cycle/multi-Factory pressure; valid high-demand placement and crafting; exact non-Legendary Asset Replication eligibility; persistent 20-slot Founder Hall relic state.
- Task 25: fix round 1/5 (5 addressed, 1 Important open — runtime sequence projection/composite generator exist, but required shot sources and generated packages are absent, so no usable authored `CS_ForgeweaveAscension` is currently producible; commits `b57bc4d..7023794`).
- Task 25: fix round 2/5 started — open: replace absent external-shot dependency with a complete source-authored five-beat Ascension sequence/generation contract that produces a usable composite when UE is available, while preserving the no-fabricated-`.uasset` environment ruling.
- Task 25: fix round 2/5 (1 addressed, 0 open — complete five-beat source/schema, child-shot/composite generation, isolated generation/validation runner, and exact static coverage; commits `7023794..63e98d9`).
- Task 25: complete (commits `f191c76..63e98d9`, review clean after two fix rounds; independent static suites 58/58 content and 18/18 UI passed; UE 5.8 compilation/Automation/generation/cook remains environment-blocked; zero fabricated `.uasset` files).
- Task 26: started — base `63e98d9` — implementer `/root/task26_impl` — brief `task-26-brief.md`.
- Task 26: fix round 1/5 started — open: subscribe runtime presentation adapters to real construction/damage/capture delegates; generate/import and validate actual renderable/audio/sequence asset classes rather than descriptor-only packages; enforce SFX minimum `>=60` with derived totals/cache parity; carry committed target owner through capture signage stages; surface degraded generated-cache diagnostics.
- Task 26: fix round 1/5 (3 Important and 2 Minor addressed, 2 Important open — adapter can subscribe but has no production construction/initialization/binding lifecycle; material/Niagara generation ignores authored palette/template contents and validates only default parent/class; commits `f596605..97a0a67`).
- Task 26: fix round 2/5 started — open: ship automatic production delegate binding lifecycle; generate and validate source-derived material palettes and Niagara system/template content.
- Task 26: fix round 2/5 (1 addressed, 1 Important open — shipped world-subsystem bootstrap/lifecycle is complete; Niagara validation can incorrectly combine the expected emitter identity with a sprite renderer found on a different emitter; commits `97a0a67..2b1529b`).
- Task 26: fix round 3/5 started — open: require expected Niagara emitter identity, enabled state, target/loop behavior, and renderer content on the same emitter, with cross-emitter bypass regression.
- Task 26: fix round 3/5 (same-emitter identity/enabled/CPU/renderer addressed, 1 Important open — production adapter still derives one-shot from `UNiagaraSystem::IsLooping()` and supplies that system-wide result to every emitter view; regression fake does not exercise this adapter; commits `2b1529b..8111eb4`).
- Task 26: fix round 4/5 started — fresh implementer escalation; open: persist/read exact one-shot lifecycle on the selected emitter itself and regression-test the real production adapter so another emitter or system-wide lifecycle cannot satisfy it.
- Task 26: fix round 4/5 (1 addressed, 0 open — selected emitter now authors and independently re-reads exact `Self/Once` lifecycle; production mapper rejects system/sibling lifecycle substitution; commits `8111eb4..8a38beb`).
- Task 26: complete (commits `63e98d9..8a38beb`, review clean after four fix rounds; static Content 69/69 and UI 18/18 passed; exact 50 primary/25 VFX/9 music/12 ambient/>=60 SFX contracts and 288 real-class artifact plans; UE 5.8 compilation/generation/Asset Registry/cook remains environment-blocked; zero fabricated `.uasset` files).
- Task 27: started — base `8a38beb` — implementer `/root/task27_impl` — brief `task-27-brief.md`.
- Task 27: fix round 1/5 started — open: fail-closed captured-evidence validation/claim derivation for finite typed measurements, exact hardware, digests, plan hash and command exits; executable runtime benchmark workload/capture/evidence writer; nonempty canonical utility resolution plus per-cycle event-runaway and required-quest-staleness soak guards.
- Task 27: fix round 1/5 (runtime benchmark and canonical soak guards addressed, 1 Important open — captured evidence can omit `planSha256` when validation is called without a plan path, and `sceneId` is not validated; commits `efcf30d..7efd115`).
- Task 27: fix round 2/5 started — open: require and validate the exact plan digest and canonical scene ID for every `CAPTURED` payload, including ordinary validation without an explicit plan path; add focused false-pass regressions.
- Task 27: fix round 2/5 (1 addressed, 0 open — every captured/candidate payload now requires canonical scene identity and exact authoritative plan reconciliation; commits `7efd115..5da35e5`).
- Task 27: complete (commits `8a38beb..5da35e5`, review clean after two fix rounds; 8 Performance, 69 Content, 18 UI, and 3 native harnesses passed; UE 5.8 Automation/cook/reference-hardware capture remains environment-blocked and checked evidence is honestly `NOT_READY`).
- Task 28: started — base `5da35e5` — implementer `/root/task28_impl` — brief `task-28-brief.md`; release signoff must remain fail-closed/`NOT_READY` wherever UE5.8, reference hardware, route videos, controller observation, or uncoached onboarding evidence cannot be produced in this environment.
- Task 28: fix round 1/5 started — open: regenerate/byte-bind current fixture plan and re-hash every linked artifact for `READY`, require structured boolean human/defect attestations and run/build binding; produce distinct Loyalty through legitimate production actions with causal proof; make every checkpoint conditional, sidecar-visible, and comprehensive over canonical state; preserve verified partial Automation evidence while remaining `NOT_READY`.
- Task 28: fix round 1/5 (all findings addressed, 0 open — current-plan and linked-artifact revalidation, literal structured attestations, causal production Loyalty/damage, byte-identical full canonical checkpoints, and partial evidence retention; commits `a81a79e..db85b77`).
- Task 28: complete (commits `5da35e5..db85b77`, review clean after one fix round; Release 17/17, Content 69/69, UI 18/18, Performance 8/8, and 3 portable native harnesses passed; release state honestly remains `NOT_READY` because Task 27, UE5.8, reference hardware, videos, controller observation, onboarding, and defect-triage evidence are unavailable).
- Final whole-branch review: NO-SHIP at `db85b77` — open: definite Founder type/private-deck compile failures; incomplete canonical new-campaign bootstrap; city/citizen/job/migration simulation not production-driven or persisted; missing production UI feature/travel/Founder ability/first-hour ingress integrations; region damage/capture detached from the world owner; incomplete card/deck/world save invariants; missing content/build save versions.
- Final whole-branch fix pass: started — one cohesive implementer `/root/final_integration_fixes`; all seven finding groups must be fixed and regression-covered before the single scoped final re-review.
- Final whole-branch fix pass: commit `7c2bf71`; portable Content 69/UI 18/Performance 8/Release 25 and 3 native harnesses passed; scoped re-review closed compile blockers and canonical city ownership but found 7 Important residuals.
- Final scoped re-review adjudication: residuals accepted as real — exact 40/12/8 wallet and opening hand; actual travel/load reconstruction; four authored functional Founder abilities; monotonic combat CAS token; rival construction excluded from player collection; deployed-card zone transition; lossless v18→v19 utility/facility migration. A bounded corrective pass is required; no second broad review will be opened.
- Final scoped re-review adjudication: all 7 accepted residuals corrected in `2430bc0` and covered by 15 focused semantic contracts plus native production-path Automation sources; no further broad review opened per the one-rereview rule.
- Final controller verification: PASS for Content 69/69, UI 18/18, Performance 8/8, Release 32/32, three portable native harnesses, 34 JSON parses, benchmark/release validators, `git diff --check`, blocker scan, and zero `.uasset`; UnrealBuildTool and UnrealEditor-Cmd remain unavailable at exit 127, so performance/release evidence correctly remains `NOT_READY` pending UE5.8/hardware/human gates.
