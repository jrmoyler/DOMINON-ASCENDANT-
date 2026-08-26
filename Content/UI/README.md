# Task 21 UI authority

`Content/UI/Manifests/VerticalSliceUI.json` is the packaged runtime authority for the exact 27 player-facing surfaces, eight city overlays, input contexts, controller graph, visual direction, and frozen accessibility option catalogue. Packaging stages `UI/Manifests` as NonUFS so the runtime reads these exact bytes through `ProjectContentDir`. No hand-authored binary placeholder is committed.

The runtime uses generated Widget Blueprints only when their provenance metadata, canonical manifest fingerprint, IDs, paths, source classes, and real Enhanced Input mappings all match exactly. If the cache is missing or stale, startup uses the native source fallback: source `UCommonActivatableWidget`, `UInputAction`, and per-active-screen `UInputMappingContext` objects built from the packaged manifest. A generated cache is therefore an optimization, never a startup prerequisite.

Generate editor Widget Blueprint, `UInputAction`, `UInputMappingContext`, and metadata caches at the exact `/Game/UI/Screens`, `/Game/UI/HUD`, `/Game/UI/Overlays`, and `/Game/UI/Input` paths with Unreal Engine 5.8:

```text
UnrealEditor-Cmd.exe DominionAscendant.uproject -run=DAUIAsset -unattended -nop4
```

Validate the entire installed cache and scan the `/Game/UI` namespace without writing assets with `-ValidateOnly`. The generator is idempotent for its own exact metadata and refuses stale or foreign objects. `Build/Scripts/PreCookUI.sh` is the optional pre-cook entry point; a BuildGraph pipeline can invoke the `GenerateUIOptionalCache` node in `Build/Graph/VerticalSliceUI.xml` before cooking. Packaging always cooks `/Game/UI`; generated `.uasset` files are caches, while the fingerprinted staged Content JSON manifest and C++ classes remain authoritative.
