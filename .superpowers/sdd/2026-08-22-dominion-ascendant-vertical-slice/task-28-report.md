# Task 28 implementer report

## Result

Implemented the source-level vertical-slice release harness, deterministic
pre-final-commit route fixtures, six-checkpoint save regression, strict
artifact reconciler, and human release protocols. The checked release state is
intentionally **`NOT_READY`**. Nothing in this task claims that UE5.8,
reference-hardware, route-video, controller-human, uncoached-onboarding, or
defect gates ran in this environment.

The release record consumes the exact current Task 27 bytes:

- path: `Build/Performance/Task27PerformanceEvidence.json`
- SHA-256: `5dd30298eae0244213b0aabdf7a2aabbfde52ffa9e2ec18b5460377d013ac705`
- state: `NOT_READY`

## Files

Created:

- `Source/DominionTests/Private/Release/VerticalSliceReleaseSpec.cpp`
- `Tests/Release/test_vertical_slice_release.py`
- `Build/Tools/ReleaseCandidateTool.py`
- `Build/Release/README.md`
- `Build/Release/Task28ReleaseEvidence.json`
- `Build/Release/Task28ReleaseEvidence.schema.json`
- `Build/Release/Task28ReleaseCandidate.schema.json`
- `Content/Test/Fixtures/README.md`
- `Content/Test/Fixtures/VerticalSliceRouteFixture.schema.json`
- `Content/Test/Fixtures/ForceRoute/RouteFixture.source.json`
- `Content/Test/Fixtures/EconomicRoute/RouteFixture.source.json`
- `Content/Test/Fixtures/InfluenceRoute/RouteFixture.source.json`
- `Content/Test/Fixtures/AllianceRoute/RouteFixture.source.json`
- `docs/qa/vertical-slice-release-checklist.md`

No `.uasset` exists below `Content/Test/Fixtures`. These are reviewed JSON
sources with an exact schema/tool generation contract, not invented Unreal
packages or binary saves.

## Implementation

`ReleaseCandidateTool.py` provides four executable operations:

- `plan`: strictly validates all four fixture sources and emits an immutable,
  byte-hashed fixture plan;
- `validate`: reconciles the checked Task 28 record against the exact Task 27
  file and state;
- `run`: removes any stale Automation sidecar, invokes
  `Dominion.Release.VerticalSlice`, captures its real exit/log, and remains
  `NOT_READY` after Automation because human/video/defect gates are separate;
- `finalize`: is the sole path to `READY`, and only after exact Task 27,
  Automation, route, save, controller, onboarding, video/log, and defect
  reconciliation.

The successful Automation sidecar is retained in the checked release record,
and all external evidence references carry repository-relative path plus SHA-256.
Finalization rejects missing, empty, mutated, or duplicate route log/video
artifacts. Strict JSON parsing rejects duplicate members and non-finite values.
Checked `READY` evidence is revalidated rather than trusted as an editable
boolean.

`Dominion.Release.VerticalSlice` uses `FDAGameInstanceSubsystemFixture` only to
host the real `UDAWorldStateSubsystem`; route completion, Foundry Shortage,
Daxton, first Ascension, campaign validation, and save/load all execute through
production authorities. It does not set resolved conquest or Ascension state.
It records only committed Loyalty, Daxton state, history, structural/module
damage, and relationship observations. Damage hashing excludes fixture GUIDs,
so identity differences cannot manufacture a passing aftermath.

The exact save/load checkpoints are:

1. `first_city_built`
2. `mid_nia_quest`
3. `active_foundry_shortage`
4. `mid_iron_veil`
5. `daxton_phase_transition`
6. `immediately_after_ascension`

The release checklist defines the required no-mouse human-observed controller
path, accessibility observations, all 13 uncoached first-hour actions, and the
zero Blocker/Critical/High defect gate. Automation is deliberately unable to
set those human claims.

## TDD evidence

Initial RED, before the tool/fixtures/contracts existed:

```text
$ python3 -m unittest Tests.Release.test_vertical_slice_release -v
Ran 5 tests
FAILED (failures=1, errors=4)
```

All failures were caused by the missing `Build/Tools/ReleaseCandidateTool.py`
and missing source contracts. Minimal implementation then made the five tests
GREEN.

Self-review added six contract-hardening regression cycles:

- fabricated checked `READY` was accepted: targeted test RED, then GREEN after
  exact Task 27/claims/embedded-gate validation;
- a stale valid Automation sidecar was reused after an exit-0 runner: targeted
  test RED (`5 != 3`), then GREEN after pre-run sidecar invalidation;
- damage fingerprints differed on fixture GUID alone: targeted test RED, then
  GREEN after hashing structural/module consequences instead;
- external artifacts were path-only: targeted test RED, then GREEN after the
  `{path, sha256}` contract and exact-byte reconciliation.
- the finalized checked record discarded its reconciled Automation artifact:
  targeted test RED (`KeyError: automationEvidence`), then GREEN after binding
  the exact sidecar reference into both `READY` and `NOT_READY` evidence shapes.
- the recorded runner command embedded this disposable worktree path: targeted
  test RED, then GREEN after making all project/plan/sidecar arguments stable
  repository-relative command inputs.

Final Task 28 portable suite:

```text
$ python3 -m unittest discover -s Tests/Release -p 'test_*.py'
Ran 9 tests in 0.105s
OK
```

## Verification

Fresh available Python regression suites:

```text
$ python3 -m unittest discover -s Tests/Content -p 'test_*.py'
Ran 69 tests in 0.969s
OK

$ python3 -m unittest discover -s Tests/UI -p 'test_*.py'
Ran 18 tests in 0.011s
OK

$ python3 -m unittest discover -s Tests/Performance -p 'test_*.py'
Ran 8 tests in 0.218s
OK

$ python3 -m unittest discover -s Tests/Release -p 'test_*.py'
Ran 9 tests in 0.105s
OK
```

Existing portable native contracts also compiled and ran with
`-std=c++17 -Wall -Wextra -pedantic`:

```text
Tests/Performance/SimulationLODPolicyTest.cpp                 exit 0
Tests/Performance/SoakProgressGuardTest.cpp                   exit 0
Tests/Content/Native/PresentationNiagaraEmitterValidationTest.cpp exit 0
```

Additional checks:

```text
$ python3 Build/Tools/ReleaseCandidateTool.py validate --root .
fixtures=4
state=NOT_READY
task27=NOT_READY

$ python3 Build/Tools/ReleaseCandidateTool.py plan --root . --output /tmp/da-task28-plan.json
fixtures=4
planSha256=84943b70e26bb381476fb6c3c73bd8978e90f74e9d2f1e18ceac6de775bb41a9

$ git diff --check
exit 0
```

## Unavailable UE evidence and release limitations

The required UE commands were attempted honestly:

```text
$ python3 Build/Tools/ReleaseCandidateTool.py run --root . --unreal-editor UnrealEditor-Cmd ...
state=NOT_READY
releaseAutomation.exitCode=127

$ Engine/Build/BatchFiles/Build.bat DominionAscendantEditor Win64 Development ...
/bin/bash: Engine/Build/BatchFiles/Build.bat: No such file or directory
exit 127

$ UnrealEditor-Cmd DominionAscendant.uproject ... Dominion.Release ...
/bin/bash: UnrealEditor-Cmd: command not found
exit 127
```

Consequently, the new UE Automation source was not compiled or executed here,
the four route outcomes and six UE save/load checkpoints are not claimed as
passing, and no Automation sidecar was manufactured. Source inspection also
cannot substitute for observing whether all four production routes really
produce pairwise-distinct Loyalty and damage. The harness will fail those
assertions if the production consequences are not distinct.

The following mandatory evidence is unavailable and remains null/unclaimed:

- UE5.8 `Dominion.Release` results and route/save sidecar;
- four continuous route videos and four route logs;
- reference-hardware capture and Task 27 measured/cook evidence;
- human-observed controller-only and accessibility checklist/video;
- first-time uncoached onboarding observations;
- current defect export and explicit Medium/Low disposition.

## Self-review

- Scope: only Task 28 release fixtures/tests/tooling/checklist were added.
- Authority: production campaign owners and `FDASaveService` are reused; no
  second resolved-state truth or debug bypass was introduced.
- Fixture legitimacy: closed field sets, exact minimum canonical authority
  names, pre-final-commit boundary, source byte hashes, and no binary assets.
- Evidence integrity: Task 27 exact bytes/state are consumed; stale sidecars,
  malformed `READY`, non-distinct aftermath, incomplete protocols, missing or
  mutated artifacts, and unaccepted defects fail closed.
- Honesty: checked state remains `NOT_READY`; all real-environment and human
  gaps are explicit in both JSON evidence and the QA checklist.
- Remaining risk: UE compilation and runtime behavior require the unavailable
  UE5.8 candidate environment; no portable result is represented as a release
  signoff.

## Independent-review fix round 1/5 (2026-08-26)

### Result

Addressed all four independent-review findings while preserving the checked
release result as **`NOT_READY`**. The checked record is now bound to run
`task28-local-no-ue-20260826-fix1` and build
`unbuilt-source-a81a79e-fix1`; its honest UE launch remains exit 127. The
authoritative generated plan and the exit-127 log are checked, non-empty,
digest-bound artifacts, so validation remains portable outside this worktree.

### RED/GREEN record

Focused RED was captured before implementation across the requested failure
modes:

- missing fixture sources and a stale plan were accepted by direct candidate
  reconciliation/finalization;
- a previously finalized `READY` record did not re-hash deleted or mutated
  external evidence;
- run/build and structured human/controller/onboarding/defect attestations were
  absent, and truthy strings could satisfy boolean gates;
- successful partial Automation, four-route, and six-checkpoint results were
  discarded solely because later release gates remained closed;
- source fixtures did not declare a production Loyalty action and the harness
  preloaded Force Loyalty while the other routes remained zero;
- checkpoints were listed independently of round-trip success and compared only
  selected field counts rather than complete canonical state.

The first focused run reported six failures/errors. Two additional source-level
RED cycles then proved that the harness lacked the production capture calls and
complete save-byte comparison (`2` failures), that the observed Loyalty delta
was not reconciled to the fixture Gift reward (`1` failure), that command logs
were not digest-bound (`1` failure), and that a failed finalization discarded
already verified partial evidence (`1` failure). Each focused case was rerun
GREEN before the full suite.

Final portable release suite:

```text
$ PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s Tests/Release -p 'test_*.py' -v
Ran 17 tests in 0.432s
OK
```

### Implementation changes

- `ReleaseCandidateTool.py` now regenerates the canonical fixture plan from
  current source bytes for finalization and every `READY`/partial validation,
  requires byte identity, binds the candidate and every attestation to explicit
  run/build IDs, and re-hashes every linked plan, Task 27, command log,
  Automation, route, save, human/video, onboarding, and defect artifact.
- Controller, onboarding, and defect evidence are strict structured JSON
  attestations with exact field sets, valid UTC timestamps, unique identities,
  cross-reference reconciliation, and literal booleans. Truthy strings are
  rejected.
- A valid Automation sidecar and its route/checkpoint rows and digests survive
  later `NOT_READY` gates and a rejected finalization; claims reflect only that
  verified partial evidence.
- Every fixture supplies an exact `UDACaptureComponent` Gift action policy with
  rewards `15/30/45/60`. The C++ harness constructs a valid Disabled Infinite
  Foundry capture opportunity, then calls `BeginCapture`, `AdvanceCapture`, and
  `ResolveOutcome(Gift)`. It asserts the committed `capture.gifted` record,
  before/after delta, owner/reward flags, and records a causal-action SHA-256.
  There is no direct `PostConflictLoyalty` assignment.
- Route damage now differs through legitimate production Daxton
  `HackProduction` strengths, not fixture identity. Damage fingerprints continue
  to exclude GUIDs.
- A checkpoint result is appended only after `FDASaveService` saves, loads,
  validates, re-saves, and produces byte-identical complete canonical `.dasave`
  content. Sidecar rows explicitly contain checkpoint name, literal pass, and
  snapshot SHA-256. This compares all serialized campaign authorities, including
  card/city layout, quests, crises, Iron Veil damage, relationships, conquest,
  Relic/unlocks, and post-Ascension state.
- Candidate/evidence/fixture schemas and the QA/build/fixture documentation now
  describe these contracts. `Build/Release/Task28FixturePlan.json` and
  `Build/Release/Task28ReleaseAutomation.exit127.log` preserve the checked plan
  and honest unavailable-run evidence.

### Verification

Fresh required suites after the fix:

```text
Tests/Content      69 tests, OK
Tests/UI           18 tests, OK
Tests/Performance   8 tests, OK
Tests/Release      17 tests, OK
```

Fresh portable native contracts compiled with
`g++ -std=c++17 -Wall -Wextra -pedantic` and ran successfully:

```text
Tests/Performance/SimulationLODPolicyTest.cpp                       exit 0
Tests/Performance/SoakProgressGuardTest.cpp                         exit 0
Tests/Content/Native/PresentationNiagaraEmitterValidationTest.cpp   exit 0
```

Other fresh checks:

```text
$ python3 Build/Tools/ReleaseCandidateTool.py validate --root .
fixtures=4
state=NOT_READY
task27=NOT_READY

$ python3 Build/Tools/ReleaseCandidateTool.py plan --root . --output Build/Release/Task28FixturePlan.json
fixtures=4
planSha256=7d561fddc80b3bc4afc0235447b4187f090889106cd94aac3bb87af416ca1828
```

The optional third-party `jsonschema` Python module is unavailable
(`ModuleNotFoundError`); all JSON files parse with the standard library, and the
stricter executable source validators and release regression suite pass.

### Honest unavailable UE attempts

```text
$ python3 Build/Tools/ReleaseCandidateTool.py run --root . --unreal-editor UnrealEditor-Cmd \
    --run-id task28-local-no-ue-20260826-fix1 \
    --build-id unbuilt-source-a81a79e-fix1 \
    --evidence Build/Release/Task28ReleaseEvidence.json
state=NOT_READY
releaseAutomation.exitCode=127

$ UnrealEditor-Cmd DominionAscendant.uproject ... Dominion.Release ...
/bin/bash: UnrealEditor-Cmd: command not found
exit 127

$ Engine/Build/BatchFiles/Build.bat DominionAscendantEditor Win64 Development ...
/bin/bash: Engine/Build/BatchFiles/Build.bat: No such file or directory
exit 127
```

No UE5.8 Automation result, Windows build, reference-hardware measurement,
route video, controller-human observation, uncoached onboarding result, or
triaged defect export was manufactured. Task 27 remains `NOT_READY`, so the
final release gate is correctly closed.

### Fix-round self-review

- Authority: observed Loyalty now has a durable production capture cause;
  fixtures supply only pre-action policy and no aftermath state or debug path.
- Completeness: save regression uses the full canonical serialized snapshot,
  avoiding another partial authority comparison that could drift as the save
  aggregate grows.
- Integrity: a past finalization is not trusted; fixture and linked artifact
  deletion/mutation are detected on every validation, and all claims derive
  from currently revalidated bytes.
- Evidence honesty: structured attestation contracts improve provenance but do
  not claim that unavailable humans or hardware exist. Checked evidence remains
  `NOT_READY` with Task 27 and every unavailable human gate false/null.
- Remaining risk: the C++ source cannot compile or execute until UE5.8 is
  available. The sidecar is therefore still null and route/checkpoint behavior
  remains unclaimed despite the passing portable/static contracts.
