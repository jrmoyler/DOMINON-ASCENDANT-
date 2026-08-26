# Task 28 release gate

`Build/Tools/ReleaseCandidateTool.py` is the executable gate. It validates the
four source fixtures, emits a deterministic plan, invokes
`Dominion.Release.VerticalSlice`, and refuses `READY` unless the final candidate
reconciles exact files and digests for all of the following:

- Task 27 is `CAPTURED` with performance, canonical soak, and cook claims true;
- all four routes reach first Ascension and have pairwise-distinct Loyalty,
  Daxton, history, damage, and relationship aftermath;
- all six save checkpoints pass;
- human-observed controller-only and uncoached first-time onboarding protocols
  pass with linked logs/videos;
- Blocker, Critical, and High defects are zero, every Medium is explicitly
  accepted, and Low defects are within an explicit polish tolerance.

Every linked fixture plan, Automation sidecar/log, route log/video, save log,
controller checklist/video, onboarding log, and defect report is a
`{path, sha256}` artifact reference. Every `READY` validation regenerates the
authoritative plan from the current fixture bytes, requires byte-identical
canonical plan content, and re-hashes every link. Missing or mutated files close
the gate even after a prior successful finalization.

The run and build identifiers bind the Automation sidecar, release candidate,
controller attestation, onboarding observations, and defect export to one
candidate execution. Controller, onboarding, and defect artifacts are strict
JSON attestations; literal JSON booleans are required (`"true"`, `"yes"`, and
other truthy strings are rejected). A successful route/save sidecar is retained
as verified partial evidence if the human, performance, or defect gates remain
`NOT_READY`.

Run the available gate:

```bash
python3 Build/Tools/ReleaseCandidateTool.py run --root . \
  --unreal-editor UnrealEditor-Cmd \
  --run-id release-run-2026-08-26-001 \
  --build-id windows-development-candidate-001 \
  --evidence Build/Release/Task28ReleaseEvidence.json
```

On a fully instrumented UE5.8/reference-hardware run, preserve the generated
fixture plan and Automation sidecar, add the linked human/video/defect evidence
to a candidate conforming to `Task28ReleaseCandidate.schema.json`, then run:

```bash
python3 Build/Tools/ReleaseCandidateTool.py finalize --root . \
  --plan Intermediate/Release/VerticalSliceRouteFixture.plan.json \
  --candidate Artifacts/Release/Task28ReleaseCandidate.json \
  --evidence Build/Release/Task28ReleaseEvidence.json
```

The four route sources describe a capture Gift policy, not a manufactured
aftermath value. Automation earns the route-specific Loyalty increase through
`UDACaptureComponent` and records a causal-action digest. Each save checkpoint
is saved, loaded, re-saved, and admitted only when the complete canonical
`.dasave` bytes are identical; its sidecar row includes `passed: true` and a
snapshot SHA-256.

The checked evidence is intentionally `NOT_READY`. It must not be hand-edited
to `READY`; the reconciler derives that state only from the exact artifacts.
