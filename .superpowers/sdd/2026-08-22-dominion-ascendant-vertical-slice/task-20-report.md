# Task 20 report — canonical first-hour onboarding and Nia Vale arc

## Delivered

- `FirstHourQuests.json` is the single frozen source for exactly the nine v1.1 quests and canonical `citizen.synara.nia_vale`. Its declared SHA-1 is `2c82c47966c118441564e717007e451326bfdea4`, computed from sorted compact UTF-8 JSON after excluding only the root `fingerprint` field. The parser recomputes and verifies both the declaration and frozen value, so `-Manifest=` cannot replace canon. Nia's eligibility also pins the canonical Human Override Task 18 definition fingerprint `433693a4a1332f736fc8e4a08a565fad`.
- All card/building references are checked against Task 19's canonical manifest. Start bindings require only real assets that must already exist. Adaptive Habitat, Microgrid, Water Reclaimer, Cognitive Operations Tower, and Agency Forum bind dynamically to real campaign `WorldAssetID` records when their construction objective is reached.
- The strict schema/parser closes every authored object and freezes complete graph, typed payload, edge, binding, reward, outcome, history, asset-path, Nia, and Human Override semantics. Generated cache validation compares every projected quest/Nia field and fingerprint. `UDAFirstHourContentRegistrySubsystem` discovers the generated primary assets at game-instance startup and falls back to transient canonical-manifest assets if the exact nine-plus-Nia cache is absent, partial, or stale.
- The public progression API accepts concrete campaign state rather than caller metric names: player-capital WorldAssets with exact city/owner/construction state, persisted Power/Water supply signals, canonical Nia citizen/job/facility records, the one persisted Synara authority, typed world-map state, and Task 18 quest state. `FDACampaignLiveSignalState` now persists city population, economy wallet/cycle state, citizen roster, job openings/assignments, and utility supply identities inside the save snapshot. `UDAWorldStateSubsystem::PersistentCampaign` is the sole mutable production owner; all signal changes use its validated revision/tick CAS, emit the committed-state event, and trigger automatic quest progression. The economy subsystem is a stateless calculator committed by that owner; its former independent city state and mutable getter are removed. Reflected Blueprint-safe player choice, audit, story, crisis, citizen, job, and utility ingress derives ticks only from the canonical clock.
- Rewards are concrete exact-once records, including deterministic quest-reward card instances, blueprints, CityMode, utility systems, Operator XP, Insight, Intelligence Auditor, Axiom archive, Forgeweave diplomacy contact, and Eden trade access. Save validation maps every action to its exact quest/type/definition/content/quantity/tick/source fingerprint; card grants additionally require their deterministic GUID, unique collection membership, definition, `QuestReward` source, and acquisition tick. Objective bindings validate exact authored quest/binding/definition/path timing and their real player-capital owner/construction state.
- Replacement Model accept/modify/audit/reject use only authored semantics: Capital efficiency effects; exact `+6/-8/-5` accept deltas; exact `+2/+4` modify deltas; durable Vision-or-research action provenance for audit eligibility and Insight/Intelligence Auditor rewards; reject's Human Agency support and Ascendant approval loss; and the exact `nia_automation_accepted`, `nia_jobs_preserved`, and `nia_model_audited` histories. Full historical transactions are replay-compared at the supplied tick, not merely action IDs or forever-current aggregates.
- Agency Has a Price, Signal in the Foundation, Iron at the Border, and The Basin Speaks contain their complete canonical choices/objectives/consequences rather than terminal stubs.
- Human Override eligibility requires exact Nia story state `story.nia.human_override.supported` plus a durable causal story-transition record sourced by the unique nonlegacy `action.quest.human_override.completed` action after a genuinely completed Task 18 `quest.human_override` from `quest.human_override.v1`; the crisis record must occur at a strictly later tick and pins the matching definition fingerprint.
- `FDASynaraCampaignState` is the Core-owned authority shared by narrative, simulation adapters, save/reload, and the production coordinator. It starts all four Synara factions at canonical support 25 and persists exact reason ledgers for Dependency, faction support, Nia relationship, policy outcomes, Nia employment, and—when regional world state is initialized—the canonical `WorldState.Diplomacy` reason ledger. Historical effects reconcile their own baseline/result records, so later legitimate ledger changes remain valid.
- Start and dynamic objective bindings now persist `BindWorldTick` and the Task 18 quest-definition fingerprint. Task 18 persists an exact ordered node-transition ledger, and save validation derives every binding tick from the specific authored start/completed-node transition rather than accepting a range. Nia tower staffing and employment both require the assignment's exact `FacilityWorldAssetId`; a completed Nia job quest requires the matching unique employment record. The six-building gate loads Task 19 placeable definitions and excludes Founder Hall.
- Schema v11 validation bounds Task 20 narrative and live-signal state, requires unique effects per branching quest, reconciles baseline-plus-delta to the exact historical authority transaction, cross-checks actual Task 18 branch paths/history/reward transactions, and checks revision and crisis proof invariants. Case 8 strips every future field from historical-v9 JSON; case 9 checks transition and binding future fields independently; v10→v11 rejects populated or malformed future live signals and supplies deterministic empty defaults only to authentic v10 documents. Tests cover direct v9, direct v10, and a nontrivial raw v8 fixture carrying its authentic legacy-v1 definition fingerprint through v9/v10/v11.
- Core now owns the frozen manifest SHA and exact `QuestId → SourceDefinitionId/version/definition fingerprint` policy for all nine quests. Snapshot validation and the manifest pipeline both enforce it, so a changed graph remains invalid even if its internal fingerprint is recomputed self-consistently. Unlock/effect source records use the same Core manifest fingerprint authority.
- World-map access is a dedicated typed authority caused by the canonical `quest.signal_in_foundation` Axiom Archive action, never inferred from CityMode. Replacement audit eligibility is likewise a dedicated typed Vision/research source record; the persisted proof pins source kind, action ID, source tag, source tick, and outcome tick, all of which are checked during completed replay.
- Generated-cache validation rejects transient objects and requires each real UObject's package and object path to exactly match the manifest. Transient objects are explicitly marked as canonical-manifest fallback objects and bypass cache validation only through the fallback construction path. The registry keeps UPROPERTY strong references for both generated and fallback quest/Nia objects; Automation coverage forces GC to verify retention.
- `UDAFirstHourQuestCommandlet` generates the nine expected quest packages and `/Game/DA/Citizens/DA_Citizen_NiaVale`; no fake `.uasset` was created.

## TDD and verification evidence

Round 5 RED was observed before production edits: four new static contract groups failed for persisted sole-owner live signals, reflected production bootstrap/ingress, schema-v11 plus unconditional v9 scans, and the complete Core nine-definition fingerprint policy.

Final available GREEN:

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*manifest.py'
Ran 35 tests in 0.007s
OK

$ git diff --check
exit_code=0
```

Static coverage includes exact identity/order/paths, canonical fingerprint mutation, Task19 reference authority, dynamic construction bindings, complete first-four and final-four flows, exact Replacement effects/rewards/histories, closed schema objects, registry/coordinator wiring, the exact Human Override gate, sole live-signal ownership, reflected ingress, v11 migration, and all nine Core definition pins. The blocked C++ Automation specs additionally cover live utility-driven production progression, citizen/economy signal save/reload, a self-consistently rehashed graph rejection, actual world-owner automatic handling, GC retention, wrong transient/package cache objects, legacy-fingerprint raw-v8/direct-v9/direct-v10 migration, exact audit source replay/tamper, node binding tick/fingerprint tamper, typed world-map tamper, facility-specific tower employment, identical Human Override story replay followed by a strictly later crisis, and historical Replacement replay after a later Dependency ledger change.

## UE execution and generation caveat

Both RED and final UE attempts are environment-blocked:

```text
$ UnrealEditor-Cmd DominionAscendant.uproject ...
/bin/bash: UnrealEditor-Cmd: command not found
exit_code=127
```

This is not a UHT, UBT, commandlet, migration, or Automation pass. On the required UE5.8 runner execute:

```text
UnrealEditor-Cmd.exe DominionAscendant.uproject -run=DAFirstHourQuest -ValidateOnly
UnrealEditor-Cmd.exe DominionAscendant.uproject -run=DAFirstHourQuest
UnrealEditor-Cmd.exe DominionAscendant.uproject -unattended -nop4 -NullRHI -NoSound -ExecCmds="Automation RunTests Dominion.Content.FirstHourQuests+Dominion.Core.Save.Migration;Quit" -TestExit="Automation Test Queue Empty"
```

Do not claim generated binary assets or UE GREEN until that run succeeds.
