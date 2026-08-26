# Task 17 report — Forgeweave rival AI and Resource Hunger

## Delivered

- Added a deterministic strategic planner whose only construction definitions are the frozen six: Worker Arcology, Production Directorate, Industrial Exchange, Infinite Foundry, Freight Furnace, and Smog Reclaimer. Unexpected authored ids cannot enter durable rival authority or be selected.
- Added inspectable candidate scoring terms for housing shortage, output shortage, Resource Hunger mitigation, defense pressure, affordability, and canonical grid placement. Campaign seed, World Tick, card order, and grid coordinate produce a stable tie key.
- Added valid construction, repair, trade, and fortification transactions. Construction requires Capital, Production Throughput, utility headroom, and a real `FDACityGridSubsystem` placement; repairs use the binding proportional repair-cost formula; trade requires route capacity; fortification requires Capital and Production Throughput.
- Added bounded Resource Hunger with separate trace terms for industrial-throughput growth, material-scarcity growth, Overdrive growth, logistics mitigation, recycling mitigation, Eden regenerative inputs, and production reduction. The authoritative result is clamped to 0–100.
- Added reflected, actor-pointer-free Forgeweave building/economy/decision state to `FDAWorldCampaignState`. State validation rejects pool leakage, overlapping/out-of-bounds buildings, impossible utilities, non-finite/negative economy, unordered decisions, and unexplained idle records.
- Advanced the campaign save schema through v6. The v4→v5 migration seeds deterministic Forgeweave authority and the v5→v6 migration promotes embedded Forgeweave records into canonical campaign authorities.
- Integrated Forgeweave processing into the existing canonical World Tick candidate transaction after trade processing and before the immutable `OnWorldTickStateCommitted` snapshot. Construction is also mirrored into Ironheart's persistent actor delta for streamed-region reconstruction.
- Added a 100-World-Tick headless soak specification that exercises construction, repair, trade, and defense and fails on invalid placement, impossible utility, negative/non-finite economy, out-of-pool construction, unbounded Hunger, or unexplained planner deadlock beyond ten ticks.

## Review-blocker closure

- Added a two-phase canonical clock transition. `OnWorldTickPreCommit` stages and fully validates the next world aggregate before any strategic cycle or World Tick broadcast; the ordinary World Tick event only publishes the acknowledged candidate. A maximum Ironheart reconstruction revision now rejects without advancing clock, world, or rival authority, and the same tick succeeds after the revision is repaired.
- Replaced rival-only action effects with established durable authorities. Buildings own a stable `FDAWorldAssetRecord` and, where eligible, a matching `FDAStructuralDamageRecord`; Ironheart reconstruction actors carry the same WorldAsset id. Trade uses the seeded Eden-to-Ironheart route, endpoint inventories, durable spot orders, and delivery history. Fortification creates durable Ironheart cover actors that streamed reconstruction registers with `UDACoverSubsystem`.
- Added save/load and streamed-reconstruction coverage for trade, defense, and repaired assets. World validation reconciles every Trade, Repair, and Fortify decision against its exact authority record, so partial or tampered transactions are rejected.
- Centralized the canonical city grid in `FDACityGridMetadata`: 32x32 cells, eight meters per cell, and 800 Unreal units per cell. Simulation defaults, Forgeweave placement, persistent transforms, reconstruction validation, and v4-to-v5 migration consume the shared values.
- Restricted crisis explanations to validated affordability, utility-capacity, and placement failures. A generic planner miss has no explanation, increments the durable unexplained-idle counter, and the eleventh contiguous miss is rejected; validated explained idling remains acceptable beyond ten ticks.
- Made uninitialized world and Forgeweave aggregates fully canonical empty and always nested-validated. Added a checksummed-load corruption test proving a hidden scalar is rejected after checksum validation.
- Hardened legacy v4 strategic-time migration: JSON numeric ticks above 2^53, non-integral values, and values outside the clock-restorable `MAX_int64 / 5` range fail migration; the exact 2^53 boundary migrates without change.

## TDD evidence

`ForgeweaveAISpec.cpp` and `ForgeweaveSoakSpec.cpp` were created before the Task 17 production headers and sources. Each test names the production break it catches: pool escape, a missing score term, non-deterministic ties, a missing Hunger driver/mitigator, an invalid action transaction, loss of post-commit persistence/reconstruction, or a soak failure.

### Focused RED attempt

```text
/bin/bash: line 2: UnrealEditor-Cmd.exe: command not found
exit code 127
```

UE 5.8 is unavailable in this workspace, so test discovery and the expected missing-production-code failure were blocked. Exit 127 is not an observed Automation RED and is not a pass.

### Focused GREEN attempt

```text
/bin/bash: line 2: UnrealEditor-Cmd: command not found
AUTOMATION_TASK17_GREEN_EXIT=127
```

This is environment-blocked, not an observed Unreal Automation GREEN.

### Review RED/GREEN attempts

The review tests were authored before the blocker fixes. The focused RED and final focused GREEN commands both reached the same environment boundary:

```text
/bin/bash: line 1: UnrealEditor-Cmd.exe: command not found
AUTOMATION_TASK17_REVIEW_GREEN_EXIT=127
```

Exit 127 is neither an observed Automation RED nor GREEN. The review command targets `Dominion.World.ForgeweaveAI`, its 100-tick soak, `Dominion.Simulation.Clock`, and `Dominion.Core.Save.Migration`.

## Behavior coverage

1. Exact ordered six-card pool plus injected-card rejection.
2. Literal independent scores for housing, output, Hunger, defense, affordability, and placement; Capital/Production insufficiency and canonical placement failures are ineligible.
3. Reproducible seed/World-Tick/card/cell tie resolution.
4. Literal Resource Hunger growth/mitigation terms and 0/100 boundary clamps.
5. Positive and negative construction, repair, route-capacity trade, and Production-backed defense cases.
6. Canonical clock advancement, durable decision history, immutable post-commit snapshot, restore-without-replay, and Ironheart reconstruction actor exposure.
7. Exactly 100 contiguous World Ticks with explicit failure checks and all four action categories exercised.
8. Checksummed schema-v4 migration to deterministic Forgeweave state and nontrivial embedded-v5 promotion to canonical schema v6.

## Static verification

- All required Task 17 source and Automation files are present and non-empty.
- The single Core pool authority contains exactly six Forgeweave ids; planner and durable validation route through it.
- New reflected headers contain exactly one generated include and keep it last.
- Save schema v6, v4/v5 migrations, canonical world-state membership, post-commit publication, and every soak failure symbol are present.
- `git diff --check` is clean.
- Combined audit result: `STATIC_TASK17_CHECKS=PASS`.
- Review audit also verifies generated-include ordering, exact pool cardinality, removal of rival-only trade/material/defense scalars, shared grid metadata, pre-commit staging symbols, durable action authorities, migration guards, delimiter balance, and a clean `git diff --check`; result: `STATIC_TASK17_REVIEW_CHECKS=PASS`.

Static verification does not replace UHT, UBT, linking, or Automation execution.

## Remaining verification

On a UE 5.8-capable Windows runner, build the editor target and execute:

```text
UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.World.ForgeweaveAI+Dominion.Core.Save.Migration;Quit" -TestExit="Automation Test Queue Empty"
```

That run is required before claiming compilation or real Automation GREEN.

## Round-two review closure

- Replaced the mutable `bool&` precommit contract with the native monotonic `FDAWorldTickVeto`; listeners can only call `Reject()`, and later listeners can observe but never undo a rejection. Every clock mutation entry point is guarded throughout precommit, cycle, acknowledgement, and abort callbacks. Checked tick/cycle arithmetic and finite accumulator bounds prevent authority overflow.
- Added `OnWorldTickTransitionAborted` and made the world subsystem stage a complete `FDACampaignSnapshot`. A later veto now explicitly discards the staged campaign, leaving clock and campaign aligned and allowing the same tick to retry without duplicate assets or decisions.
- Removed embedded Forgeweave asset/damage copies. Buildings retain stable ids into canonical campaign `WorldAssets`; modular health resides only in `OperationConflict.StructuralDamageRecords`. Trade, repair, and fortify mutations append durable action transactions containing their exact economy openings/closings and authority-specific quantities, module deltas, or cover transform/type.
- Added `UDARegionAuthorityResolver`, which consumes the canonical campaign during streamed reconstruction, binds real `UDAStructuralDamageComponent` facades to repaired asset/module records, and registers durable fortifications with `UDACoverSubsystem`.
- Hardened schema-v4 time migration by extracting and checked-parsing the raw decimal `currentWorldTick` lexeme before JSON double deserialization is consulted. Decimal 2^53 remains exact; 2^53+1, fractions, exponents, and clock-unrestorable values are rejected. The migration tests never manufacture the tick with `SetNumberField`.
- Added checksummed tamper tests for trade quantities, repair integrity, and fortification cover type, plus listener-order veto, recursive mutation, abort/retry, boundary arithmetic, full-campaign save/load, and reconstructed health/cover behavior.

### Round-two RED/GREEN attempts

The round-two tests were written before their production changes. Both focused execution attempts reached the workspace tool boundary:

```text
/bin/bash: line 2: UnrealEditor-Cmd.exe: command not found
AUTOMATION_TASK17_ROUND2_RED_EXIT=127

/bin/bash: line 2: UnrealEditor-Cmd.exe: command not found
AUTOMATION_TASK17_ROUND2_GREEN_EXIT=127
```

Exit 127 is environment-blocked and is neither an observed Automation RED nor GREEN.

### Round-two static audit

- `git diff --check` is clean; no conflict markers are present.
- The canonical pool still contains exactly six ids and the reflected headers each retain one generated include.
- Removed-field scans find no embedded Forgeweave asset/damage record or obsolete mutable-bool/old snapshot delegate API.
- Migration tests contain no `SetNumberField(CurrentWorldTick)` path; raw lexical parsing, abort notification, action transactions, production reconstruction, and round-two regression symbols are all present.
- Result: `STATIC_TASK17_ROUND2_CHECKS=PASS`.

Static checks do not replace UHT, UBT, linking, or Unreal Automation. A UE 5.8 Windows runner remains required for real GREEN evidence.

## Round-three review closure

- Repair history now validates each transaction's intrinsic cost, integrity delta, and module delta without assuming that combat cannot damage the structure between repairs. The latest repair closing still reconciles to current canonical asset/module authority; damage→repair→damage→repair is covered through save/load.
- Bumped the save wire version from v5 to v6. The v5→v6 migration reads actual embedded `assetRecord`/`structuralDamage` building payloads, promotes them into `WorldAssets` and `OperationConflict.StructuralDamageRecords`, replaces buildings with stable ids, deterministically reconstructs Trade/Repair/Fortify action transactions, validates the complete campaign, and refreshes the checksum without replaying rewards.
- Added inverse/bijection validation: construction decisions, buildings, canonical assets, and reconstruction actors must agree; defense actors, Fortify decisions, and Fortify transactions are one-to-one; Forgeweave spot orders/deliveries require their exact Trade decision and transaction. Refreshed-checksum tests reject missing construction/action history and orphan construction, defense, and trade authority.
- `UDARegionAuthorityResolver` now remembers the cover ids it registered and removes authored cover absent from a later canonical reconstruction, including rollback to a pre-fortification campaign.

### Round-three TDD and verification

The damage sequence, raw nontrivial v5 migration, inverse-tamper, and cover-rollback tests preceded the production changes. Focused RED and GREEN attempts both reached the environment boundary:

```text
/bin/bash: line 2: UnrealEditor-Cmd.exe: command not found
AUTOMATION_TASK17_ROUND3_RED_EXIT=127

/bin/bash: line 2: UnrealEditor-Cmd.exe: command not found
AUTOMATION_TASK17_ROUND3_GREEN_EXIT=127
```

Exit 127 is neither an observed Automation RED nor GREEN. Static round-three checks cover schema/case continuity, generated-include ordering, balanced delimiters, migration/inverse/rollback symbols, forbidden legacy references, conflict markers, and `git diff --check`; result: `STATIC_TASK17_ROUND3_CHECKS=PASS`.

## Round-four review closure

- Removed case-sensitive planner ownership inference. Trade decisions and their exact matching transactions now define authoritative planner order membership; unmatched planner-shaped order or delivery ids are rejected with case-insensitive shape detection, including casing variants.
- Expanded construction/defense inverse validation across every region. Six-card construction actors and defense-cover actors are rejected outside Ironheart, and their stable ids, definition, region, location, rotation, and scale must match the canonical building/WorldAsset or Fortify authority. Defense also retains its canonical Ironheart cell transform.
- Made schema-v5 Forgeweave promotion validate-first and mutate-second. Every embedded asset must have the complete historical wire shape and coherent planner identity, bounds, construction state, and integrity. A present damage record must be modular-eligible and pass canonical identity/module/state/production validation; an absent record must deserialize to the canonical empty value. Checksummed malformed-v5 fixtures cover false-presence payloads, nonmodular damage, and incoherent module health.
- Made cover reconstruction externally atomic by preflighting candidate conflicts and every stale removal/source match before touching `UDACoverSubsystem`. Stale removals and candidate registrations run in deterministic stable-id order only after the complete preflight. Missing and wrong-source stale-socket regressions verify both cover contents and resolver tracking remain unchanged, even when the candidate campaign would add a replacement cover.

### Round-four TDD and verification

The casing-variant inverse, all-region/transform, malformed embedded-v5, and cover preflight tests were authored before their corresponding production changes. Focused RED and GREEN attempts reached the same workspace boundary:

```text
/bin/bash: line 1: UnrealEditor-Cmd: command not found
AUTOMATION_TASK17_ROUND4_RED_EXIT=127

/bin/bash: line 1: UnrealEditor-Cmd: command not found
AUTOMATION_TASK17_ROUND4_GREEN_EXIT=127
```

Exit 127 is environment-blocked and is neither an observed Automation RED nor GREEN. The round-four static audit verifies the exact six-card pool, case-insensitive planner-shape guards with authority-derived membership, all-region/full-transform inverse symbols, validate-before-mutate v5 promotion, cover-preflight ordering, deterministic cover mutation ordering, refreshed-checksum regression fixtures, generated-include ordering, conflict-marker absence, balanced delimiters, and `git diff --check`; results: `STATIC_TASK17_ROUND4_CHECKS=PASS` and `STATIC_TASK17_ROUND4_DELIMITERS=PASS`.

Static checks do not replace UHT, UBT, linking, or Unreal Automation. A UE 5.8 runner remains required for real GREEN evidence.

## Round-five final review closure

- Removed planner name-shape inference entirely. The current schema has one spot-order producer, Forgeweave, so every durable spot order must be a member of the exact id set derived from Trade decisions and their one-to-one action transactions. `FDATradeWorldState` already binds exactly one delivery to every spot order; campaign validation now also requires every such delivery's contract id to belong to that authority-derived set. A refreshed-checksum regression clones a valid order/delivery under unrelated `external.*` and `shipment.*` ids and proves names cannot evade inverse ownership. General recurring trade remains represented by explicit contracts; future non-Forgeweave spot-order owners require an explicit owner/type field and schema migration rather than a naming convention.
- Replaced definition-only actor inverse recognition with typed canonical-id closure. The validator builds stable authority-id and WorldAsset-id sets from decisions, action transactions, and buildings, then inspects every actor in every region that reuses either typed id, regardless of definition. Construction actors must be the sole ActorId match, sole WorldAssetId match, and sole full-tuple match; Fortify actors must likewise be the sole full match for their transaction. Arbitrary-definition duplicates of both building and defense ids in another region are covered by refreshed-checksum regressions.

### Round-five TDD and verification

The arbitrary-prefix spot-order/delivery and arbitrary-definition duplicate-id tests preceded the production changes. Focused RED and GREEN attempts both reached the environment boundary:

```text
/bin/bash: line 1: UnrealEditor-Cmd: command not found
AUTOMATION_TASK17_ROUND5_RED_EXIT=127

/bin/bash: line 1: UnrealEditor-Cmd: command not found
AUTOMATION_TASK17_ROUND5_GREEN_EXIT=127
```

Exit 127 is neither an observed Automation RED nor GREEN. The final static audit verifies zero name-shape inference in campaign validation, spot-order/authority cardinality equality, typed actor-id and WorldAsset-id recognition, exact forward counts, both refreshed-checksum regressions, balanced delimiters, conflict-marker absence, and `git diff --check`; result: `STATIC_TASK17_ROUND5_CHECKS=PASS`.

Static checks do not replace UHT, UBT, linking, or Unreal Automation. A UE 5.8 runner remains required for real GREEN evidence.
