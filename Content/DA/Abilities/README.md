# Founder ability definitions

`FounderAbilityDefinitions.json` is the single source-controlled declaration for the Synara Founder baseline. `UDAFounderAbilityDefinitions::ImportFromJsonFile` imports it into the `BaselineAbilities` array; the same importer is available at runtime through `ImportFromJsonString`.

This workspace has no Unreal Editor asset-authoring runtime. No `.uasset` binaries were fabricated. On an installed UE 5.8 editor, create a `UDAFounderAbilityDefinitions` Data Asset, invoke `Import From Json File` with this file, then save the populated asset under this directory. Gameplay Ability assets can reference the imported IDs, costs, cooldowns, and status tags.
