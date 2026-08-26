# Vertical-slice presentation source

`Content/DA/Manifests/VerticalSlicePresentation.json` is the frozen v1.1 coverage
root. It fingerprints the v0.6 primary building and character recipes, the
twenty-five VFX recipes, the nine music/twelve ambient/sixty distinct SFX
recipes, and the gameplay-to-presentation bindings as one unit.

The packaged JSON is the source-resolvable runtime fallback. It is never Asset
Registry evidence. `DAPresentationContent` converts all 156 rows into
`UDAPresentationDefinition` packages at their declared `/Game` paths; only a
complete cache with exact source and per-recipe fingerprints is accepted.
`-ValidateOnly` reads every saved package through the Asset Registry.

Construction listens to `UDAConstructionComponent::OnStageChanged`. Structural
damage and capture expose committed-snapshot delegates. Daxton and Ascension
plans are derived from their existing campaign/projection records. Capture
keeps original architecture while removing signage, showing integration
scaffolds, and installing new signage. Cinematic skip only advances transient
playback and cannot mutate those records.

Run source validation without Unreal:

```bash
Build/Tools/PresentationManifestTool.py validate
Build/Tools/PresentationManifestTool.py emit-plan
```

On a UE 5.8 authoring runner:

```bash
Build/Scripts/PreCookPresentation.sh DominionAscendant.uproject
```

No generated `.uasset` is checked in or implied by these sources.
