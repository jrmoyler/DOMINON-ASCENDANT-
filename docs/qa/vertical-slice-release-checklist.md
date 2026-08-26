# Dominion // Ascendant vertical-slice release checklist

Release state: **NOT_READY**

Last source-gate update: 2026-08-26
Binding target: UE 5.8, Windows PC, v1.1 vertical slice

The authoritative machine-readable result is
[`Task28ReleaseEvidence.json`](../../Build/Release/Task28ReleaseEvidence.json).
Its checked state is deliberately `NOT_READY`: route Automation, real UE save
regression, reference-hardware capture, videos, controller observation,
uncoached onboarding, and current defect triage are not available in this
environment. A source/static pass is not release signoff.

## Gate summary

| Gate | Required result | Current evidence | State |
|---|---|---|---|
| Fixture/source validation | Four strict pre-final-commit sources; no debug overrides or fake `.uasset` | [Fixture contract](../../Content/Test/Fixtures/README.md), [schema](../../Content/Test/Fixtures/VerticalSliceRouteFixture.schema.json), and [current generated plan](../../Build/Release/Task28FixturePlan.json) | PASS (source) |
| Release Automation | UE5.8 `Dominion.Release` passes | [Release tool/runner contract](../../Build/Release/README.md); local launch exits 127 because `UnrealEditor-Cmd` is absent | NOT_RUN |
| Four route runs | Force, Economic, Influence, Alliance each reach first Ascension | [Release evidence route field](../../Build/Release/Task28ReleaseEvidence.json) is `null` | NOT_RUN |
| Route aftermath | Loyalty, Daxton, history, damage, and relationship aftermath pairwise differ | `VerticalSliceReleaseSpec.cpp` records/compares all five only from committed campaign state | NOT_RUN |
| Route logs/videos | One real log and video per route | [Release evidence route field](../../Build/Release/Task28ReleaseEvidence.json) is `null`; no video is claimed | MISSING |
| Six save checkpoints | All exact checkpoints save, load, validate, and preserve authorities | [Release evidence checkpoint field](../../Build/Release/Task28ReleaseEvidence.json) is `null` | NOT_RUN |
| Canonical 1,000-cycle soak | Real owner soak passes | [Task 27 evidence](../../Build/Performance/Task27PerformanceEvidence.json) is `NOT_READY` | NOT_RUN |
| Reference performance/cook | 1920×1080 High, reference hardware, cook and measured targets pass | [Task 27 evidence](../../Build/Performance/Task27PerformanceEvidence.json) is `NOT_READY` | NOT_RUN |
| Controller-only critical path | Human-observed, no mouse, every campaign-critical action succeeds | [Release evidence controller field](../../Build/Release/Task28ReleaseEvidence.json) is `null` | NOT_OBSERVED |
| Accessibility | Controller/remap/accessibility checklist linked to the observed run | [Release evidence controller field](../../Build/Release/Task28ReleaseEvidence.json) is `null` | MISSING |
| First-hour onboarding | First-time testers complete every action with no developer coaching | [Release evidence onboarding field](../../Build/Release/Task28ReleaseEvidence.json) is `null` | NOT_OBSERVED |
| Defect gate | Blocker 0, Critical 0, High 0; every Medium accepted; Low within tolerance | [Release evidence defect field](../../Build/Release/Task28ReleaseEvidence.json) is `null` | UNTRIAGED |

## Deterministic four-route protocol

The source fixtures are:

- [Force](../../Content/Test/Fixtures/ForceRoute/RouteFixture.source.json)
- [Economic](../../Content/Test/Fixtures/EconomicRoute/RouteFixture.source.json)
- [Influence](../../Content/Test/Fixtures/InfluenceRoute/RouteFixture.source.json)
- [Alliance](../../Content/Test/Fixtures/AllianceRoute/RouteFixture.source.json)

Each contains only canonical evidence immediately before the final route
commit. It contains no resolved conquest snapshot, Ascension state, meter
override, or debug command. The plan generator validates exact fields and
minimum authority sets, hashes the source bytes, and passes that immutable plan
to `Dominion.Release.VerticalSlice`. Finalization and every later `READY`
validation regenerate the plan from current fixture bytes and require the
supplied canonical plan bytes to match exactly. The Automation test then uses
the real `UDAWorldStateSubsystem` owners for Foundry Shortage, conquest, Daxton,
first Ascension, and `FDASaveService`; it does not reimplement route eligibility.
The fixture's Loyalty field is only a capture-Gift policy. Automation creates a
valid Disabled foundry/capture opportunity and earns the route-specific increase
through `UDACaptureComponent`; it records before/after Loyalty, committed
`capture.gifted` proof, and a causal-action digest. No fixture preloads aftermath
Loyalty.

Run on a UE5.8 Windows candidate machine:

```powershell
python Build/Tools/ReleaseCandidateTool.py run --root . `
  --unreal-editor UnrealEditor-Cmd.exe `
  --run-id release-run-2026-08-26-001 `
  --build-id windows-development-candidate-001 `
  --evidence Build/Release/Task28ReleaseEvidence.json
```

For every route, retain the Automation log and a continuous capture from
fixture restore through first Ascension. The final candidate must link those
eight distinct, non-empty real files and bind each path to its SHA-256. The
reconciler requires all five aftermath fingerprints to be pairwise distinct
before it can finalize `READY`. Damage fingerprints deliberately exclude
fixture identities, so only actual structural/module consequences count.

## Save/load checkpoint protocol

The release Automation suite must pass these exact checkpoints:

1. `first_city_built`
2. `mid_nia_quest`
3. `active_foundry_shortage`
4. `mid_iron_veil`
5. `daxton_phase_transition`
6. `immediately_after_ascension`

Each checkpoint is written and loaded by `FDASaveService`, validated as a full
`FDACampaignSnapshot`, then written again. The two complete canonical `.dasave`
byte sequences must be identical. This covers collection and deck card
instances, city layout/assets, quest manifests/nodes/progress, Foundry Shortage,
Iron Veil structural damage, diplomacy, conquest, Daxton, history, Relic and
unlock state, and the post-Ascension architecture without maintaining a second
partial equality implementation. The Automation sidecar contains one exact
`{checkpoint, passed, snapshotSha256}` row per successful checkpoint; a failed
round trip is never appended. Preserve the sidecar and logs; the final candidate
must link a log for every checkpoint.

## Controller and accessibility observation

Use a clean controller profile and disconnect or physically set aside the
mouse. A human observer must record each action as a literal JSON boolean and
retain a continuous video. The attestation must identify its `runId`, `buildId`,
observer, UTC timestamp, action map, `observedByHuman: true`, `mouseUsed: false`,
and the exact `{path, sha256}` video reference:

- build;
- card inspect / 3D inspect;
- combat;
- Command mode and tactical order;
- diplomacy;
- conquest resolution;
- save and load.

Also record focus visibility, back navigation, remapped bindings, scalable
text, subtitles, reduced-flash setting, color differentiation, camera-shake
setting, and hold/toggle alternatives. Automated input-binding coverage may
support this checklist but cannot set `observedByHuman`.

## Uncoached onboarding observation

Recruit first-time testers and provide no developer coaching. The structured
JSON attestation identifies the same run/build, observer and UTC timestamp, and
contains a unique anonymized stable ID for every tester. Each tester row must
record `developerCoaching: false` and literal boolean results for whether they
independently:

- moved the Founder;
- activated Founder Hall;
- placed Adaptive Habitat;
- understood utilities;
- housed residents;
- employed Nia;
- generated Capital;
- encountered Dependency;
- researched with Insight;
- understood Influence;
- met Forgeweave and Eden;
- completed first combat;
- commanded Guardian Drones.

Any coaching or missing action keeps onboarding false. The observation log is
an external test artifact; this repository currently contains none.

## Defect signoff

Attach a structured JSON export tied to the same run/build, with attester, UTC
timestamp, counts, stable issue IDs, severity, and disposition. Signoff requires:

- Blocker = 0
- Critical = 0
- High = 0
- Medium = every remaining ID explicitly accepted
- Low = explicit confirmation that the remaining count is within polish tolerance

Accepted Medium issues: **none recorded (gate not yet triaged)**.

Accepted Low tolerance: **not recorded**.

## Finalization

After Task 27 is genuinely `CAPTURED`, all Automation/human/video/defect files
exist, and their SHA-256 values are placed in a closed candidate conforming to
[`Task28ReleaseCandidate.schema.json`](../../Build/Release/Task28ReleaseCandidate.schema.json), run:

```powershell
python Build/Tools/ReleaseCandidateTool.py finalize --root . `
  --plan Intermediate/Release/VerticalSliceRouteFixture.plan.json `
  --candidate Artifacts/Release/Task28ReleaseCandidate.json `
  --evidence Build/Release/Task28ReleaseEvidence.json
```

Only exit 0 plus machine-readable `state=READY` is release-candidate signoff.
Every validation re-hashes all links, including the plan, Automation/log,
route/save files, human attestations/videos, Task 27, and defect export. Deleting
or mutating any linked file closes a previously finalized gate. Do not hand-edit
the checked evidence or infer a pass from source tests.
