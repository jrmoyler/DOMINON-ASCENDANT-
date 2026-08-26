# Task 13 report — Gameplay Ability System combat foundation

## Delivered

- Added `UDACombatAttributeSet` with the canonical Founder Health (100), Guard (50), Stamina (100), and Tactical Charge (0–100) resources.
- Added the six broad channels: Kinetic, Thermal, Arc, Bio, Cyber, and Structural.
- Added the required native status tags: Suppressed, Shielded, Burning, Disrupted, Hacked, Regenerating, Inspired, Routed, Immobilized, and Marked, plus the combat-state `Status.Downed` tag.
- Added `UDADamageExecution`, a `UGameplayEffectExecutionCalculation`. Gameplay Effects provide `Data.Damage` and numeric `Data.DamageChannel` SetByCaller values; C++ calculates resistance, Guard absorption, and Health spillover authoritatively.
- Added direct C++ combat routing through `UDAAbilitySystemComponent` for the same calculation, and a recoverable twenty-second Downed state at zero Health.
- Added a data-asset schema and source-controlled declarations for Precision Scan, Drone Barrier, Orchestration Mark, and Coordinated Override.

## TDD evidence

`Source/DominionTests/Private/Gameplay/CombatAttributesSpec.cpp` was added before every Task 13 production file. It specifies:

1. Arc damage on a shielded synthetic target uses configured resistance and consumes Guard before Health.
2. Lethal damage starts the twenty-second Downed window and does not cause permanent death.
3. All four required Synara Founder ability definitions exist.

### Initial RED attempt

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Gameplay.CombatAttributes;Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: UnrealEditor-Cmd: command not found
AUTOMATION_EXIT=127
```

The intended missing-implementation/compiler failure was not observable: UE 5.8 and its command runner are absent. This is an environment block, not an Automation pass or an observed failing assertion.

### Final GREEN attempt

The same focused command was attempted after implementation and produced the same `AUTOMATION_EXIT=127` / command-not-found result. It is environment-blocked and **not** a passing Unreal build or Automation result.

## Static verification

```text
git diff --check                         # exit 0
jq -e '.assetType == "DAFounderAbilityDefinitions" and (.definitions | length == 4)' Content/DA/Abilities/FounderAbilityDefinitions.json  # exit 0
no *.uasset exists under Content/DA/Abilities  # exit 0
```

`clang-format --version` returned exit 127 because `clang-format` is unavailable in this workspace. Static checks do not replace UHT, UBT compilation, or the focused Automation suite.

## Asset limitation

No binary `.uasset` files were fabricated. `Content/DA/Abilities/FounderAbilityDefinitions.json` is the source-controlled declaration and `Content/DA/Abilities/README.md` documents creating the corresponding `UDAFounderAbilityDefinitions` Data Asset on an installed UE 5.8 editor. The C++ schema and default declarations remain usable until that editor-authoring step.

## Remaining verification

On a Windows UE 5.8 runner, build the editor target and run:

```text
UnrealEditor-Cmd DominionAscendant.uproject -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Gameplay.CombatAttributes;Quit" -TestExit="Automation Test Queue Empty"
```

That run is required before claiming real UHT/C++ compatibility or Automation GREEN.

---

## Review-fix cycle

The review identified six gates that the original source-level specs did not establish. The corrections below replace the direct-mutation test path and are included in the follow-up commit.

### Findings resolved

1. The Automation spec now creates a real `AActor`, attaches `UDAAbilitySystemComponent`, registers `UDACombatAttributeSet`, builds an instant `UGameplayEffect` containing `UDADamageExecution`, supplies SetByCaller data, and applies the actual effect spec to the target. The Arc case asserts resistance and Guard-before-Health behavior through that path.
2. Removed `ResolveIncomingDamage` and `ApplyCombatDamage`. `UDADamageExecution::ResolveDamage` is private, and the execution output is the only damage-mutation route.
3. Removed the duplicate hardcoded Founder ability array. `UDAFounderAbilityDefinitions` now imports the single source-controlled JSON declaration (`ImportFromJsonString` at runtime and `ImportFromJsonFile` for editor import). The spec imports that exact file. `DominionAscendant.uproject` stages `Content/DA/Abilities` as non-asset content for runtime import.
4. Downed is now progressed only by `UDAAbilitySystemComponent::AdvanceCombatState`. At exactly 20 elapsed seconds it clears Downed, marks the combatant extracted, and broadcasts `OnDownedExpiredNative`; the spec checks before-expiry and expiry behavior, including that extraction is non-permanent.
5. Replaced the numeric channel ordinal with native tags (`Data.Damage.Channel.<Channel>`). `UDADamageExecution` accepts exactly one known channel tag marked `1`, rejects missing/unrelated/multiple data, and the Arc GameplayEffect test exercises the valid path.
6. Added the direct `GameplayEffectExtension.h` include required by `PostGameplayEffectExecute`'s callback data.

### Follow-up RED attempt

After rewriting the focused Automation spec and before implementing these corrections:

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Gameplay.CombatAttributes;Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: UnrealEditor-Cmd: command not found
AUTOMATION_FIX_RED_EXIT=127
```

The updated test could not reach discovery or a failing assertion because the runner is absent. This is an environment block, not a RED pass/fail result.

### Follow-up GREEN attempt

After the correction implementation, the same focused command returned:

```text
/bin/bash: UnrealEditor-Cmd: command not found
AUTOMATION_FIX_FINAL_EXIT=127
```

This remains an environment block, **not** a passing Unreal build or Automation result. Fresh static checks for this fix commit are recorded with the commit; UHT/UBT and Automation execution still require a UE 5.8 runner.

---

## Review-fix round 2

### Findings resolved

1. The Automation target is now a real `UWorld::CreateWorld` game-world actor spawned through `World->SpawnActor`. Its `UDAAbilitySystemComponent` is added through `AddInstanceComponent`, registered through `RegisterComponent`, and asserted as both an instance component and registered before the AttributeSet/effect setup.
2. `UDAAbilitySystemComponent` is now the production Downed driver. Its enabled component tick calls `AdvanceDownedRecovery` only when `GetOwner()->HasAuthority()`; the manual public advance API was removed. The spec uses `World->Tick` at 19.5 seconds and 0.5 seconds, checks server authority, and never invokes the AttributeSet helper directly.
3. Removed the invalid `AdditionalNonAssetDirectoriesToCopy` descriptor key. `Config/DefaultGame.ini` now stages the JSON using the supported packaging rule: `+DirectoriesToAlwaysStageAsNonUFS=(Path="DA/Abilities")`.

### Round 2 RED attempt

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Gameplay.CombatAttributes;Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: UnrealEditor-Cmd: command not found
AUTOMATION_ROUND2_RED_EXIT=127
```

The rewritten world/tick test could not reach test discovery because the Unreal command runner is absent. This is an environment block, not an observed failing assertion.

### Round 2 GREEN attempt

```text
/bin/bash: UnrealEditor-Cmd: command not found
AUTOMATION_ROUND2_GREEN_EXIT=127
```

This is also environment-blocked and **not** an Unreal Automation pass. UE 5.8 UHT/UBT execution remains required to validate the real-world setup and component ticking end to end.
