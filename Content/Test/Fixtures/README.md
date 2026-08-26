# Vertical-slice route fixtures

These are deterministic **source fixtures**, not save games or Unreal packages.
Each source describes the minimum canonical campaign authorities present
immediately before one final Forgeweave route commit. The release Automation
spec restores those authorities through `UDAWorldStateSubsystem`, invokes the
real conquest, Daxton, Ascension, and save owners, and records observed results.
Each fixture also supplies only a capture-Gift action policy (target, recipient,
and reward). It never supplies `PostConflictLoyalty`; Automation must perform
`BeginCapture`, `AdvanceCapture`, and `ResolveOutcome(Gift)` through
`UDACaptureComponent` before the final route commitment and prove the exact
causal increase.

Validate and generate the immutable execution plan with:

```bash
python3 Build/Tools/ReleaseCandidateTool.py plan --root . \
  --output Intermediate/Release/VerticalSliceRouteFixture.plan.json
```

The schema and tool reject resolved conquest/Ascension snapshots, debug
overrides, unknown fields, duplicate JSON keys, and fixture drift. No `.uasset`
may be authored by this source-only contract; real UE content remains governed
by the existing content commandlets and cook validation.
