# Task 27 performance handoff

`VerticalSliceBenchmark.scene.json` is the deterministic scene source. The
generator expands it into 50 stable WorldAsset placements, 120 stable CitizenID
rows across LOD0/1/2/3, six squads, one vehicle, active weather, one
construction effect, one damaged building, and the City → Command → Founder
sequence. It does not create or impersonate a `.uasset` map.

`VerticalSliceBenchmark.budget.json` defines the 1080p High reference hardware
and acceptance budgets. A severe Development Cycle hitch is explicitly 100 ms.
No scalability tuning is authorized until a real capture identifies a
bottleneck.

Generate and validate the source without Unreal:

```text
python3 Build/Tools/PerformanceBenchmarkTool.py validate \
  --scene Build/Performance/VerticalSliceBenchmark.scene.json \
  --budget Build/Performance/VerticalSliceBenchmark.budget.json

python3 Build/Tools/PerformanceBenchmarkTool.py generate \
  --scene Build/Performance/VerticalSliceBenchmark.scene.json \
  --budget Build/Performance/VerticalSliceBenchmark.budget.json \
  --plan-output Saved/Benchmark/VerticalSliceBenchmark.plan.json \
  --evidence-output Saved/Benchmark/Task27PerformanceEvidence.json
```

On a UE5.8 Windows machine, use `run` with the same four paths plus `--project
DominionAscendant.uproject`. The runner first executes the canonical
`DAPresentationContent` generator, then Automation without `-nullrhi`, then
cook. Automation materializes a transient world from the plan, applies
1920x1080 High, measures real engine-frame and City → Command → Founder timers,
and runs the canonical owner soak twice. It writes only a `CANDIDATE` sidecar;
the runner reconciles the exact plan SHA-256 and observed command exits, derives
all claims, fully validates the payload, and only then writes `CAPTURED`.

Storage type is not portable through UE's platform API. It therefore defaults
to `UNVERIFIED`, which can produce a capture but can never pass the exact
reference-hardware claim. On a physically verified SSD machine, pass
`--storage-class SSD`; CPU, GPU, and memory remain process-detected. The runner
fails closed: absent UE is exit 127/`NOT_READY`, and any preparation,
Automation, cook, candidate, hash, measurement, or claim mismatch resets a
clean `NOT_READY` payload. `PerformanceEvidence.schema.json` is the Task 28
handoff contract. The checked-in Task 27 evidence intentionally contains no
measurements or passing claims.
