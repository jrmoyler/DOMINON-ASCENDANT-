# Task 8 — Economy, Staffing, Construction Cost, and Maintenance

## Changed files

- `Source/DominionSimulation/Public/Economy/DAEconomyTypes.h`
  - Adds reflected wallet, facility-context, modifier, contribution-trace, output, city-state, utility, maintenance, deployment-tier, footprint, and facility-type DTOs.
  - Keeps `FDAWorldAssetRecord` as the facility identity/lifecycle/condition authority inside each economy context.
- `Source/DominionSimulation/Public/Economy/DAEconomySubsystem.h`
  - Adds the required `CalculateFacilityOutput` and `ResolveDevelopmentCycle` interfaces plus v0.8 deployment-Capital calculation and display rounding.
  - Owns the vertical-slice city economy state and subscribes to Task 7's clock rather than exposing an economy tick.
- `Source/DominionSimulation/Private/Economy/DAEconomySubsystem.cpp`
  - Implements v0.8 staffing and utility bands, the bounded additive standard-modifier pool, traceable wallet output, construction-cost coefficients, maintenance rates, maintenance-condition multipliers, and discrete-cycle wallet resolution.
  - Binds/unbinds `UDASimulationClockSubsystem::OnDevelopmentCycle` during subsystem initialization/deinitialization. There is no independent per-frame economy path.
- `Source/DominionTests/Private/Simulation/EconomySpec.cpp`
  - Covers staffing boundaries, critical utility, positive/negative standard-modifier caps, named contribution traces, Industrial maintenance before/after condition, construction-cost calculation/rounding, and a 100-cycle explicit-input boundedness scenario.

## Formula source and exact values

The implementation uses `docs/bibles/DOMINION_ASCENDANT_Economy_Balance_Progression_Bible_v0.8.md` as its tuning authority:

- Section 9: deployment Capital = tier base x footprint modifier x complexity modifier, with the listed tier and footprint coefficients.
- Sections 19-23: base output x staffing x utility x demand x condition x bounded standard modifier stack; +60% and -60% standard bounds; staffing and utility tables.
- Sections 24-25: per-type deployment-cost maintenance rates and Healthy/Worn/Damaged/Critically Damaged multipliers of 1.0/1.15/1.4/1.8.
- Vertical Slice Production Spec v1.1 sections 45 and 93: first-hour wallet target bands and a 100-Development-Cycle no-invalid-state milestone.

The v0.8 condition-output table specifies bands rather than a single integrity-to-output interpolation. `FDAFacilityContext::ConditionOutputMultiplier` therefore carries the resolved value within that band instead of inventing another coefficient. Wonder maintenance is likewise explicitly authored through `BespokeWonderMaintenanceRate` and clamped to v0.8's 0.4%-0.8% range rather than silently choosing a midpoint.

The boundedness fixture uses synthetic `scenario.facility.*` identifiers and labels every wallet/output/deployment value as an explicit scenario input. It makes no claim about named-card or balanced-Synara tuning. Exact authored content remains the responsibility of Tasks 19/27/28.

## TDD evidence

`EconomySpec.cpp` was created before any production economy file. The production breaks caught are wrong staffing/utility branches, an unbounded modifier pool, missing or wrong trace contributions, wrong maintenance order/rate, wrong deployment coefficients/rounding, per-frame or missing cycle resolution, and an insolvent or non-finite 100-cycle explicit-input scenario.

### Initial RED attempt

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Simulation.Economy;Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: line 1: UnrealEditor-Cmd: command not found
exit_code=127
```

The intended missing-header/implementation failure could not be observed because the UE 5.8 command-line runner is absent. This is an environment block, not an observed RED result.

### Final GREEN attempt

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Simulation.Economy;Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: line 2: UnrealEditor-Cmd: command not found
UE_EXIT_CODE=127
```

This is environment-blocked and **not a passing Automation result**. No supplementary check below is presented as UE execution or a pass.

## Supplementary static evidence

```text
$ git diff --cached --check
STAGED_DIFF_CHECK_EXIT_CODE=0

$ rg -n "0\\.005f|0\\.007f|0\\.006f|0\\.008f|0\\.01f|0\\.004f|1\\.15f|1\\.4f|1\\.8f|0\\.9f|0\\.65f|0\\.25f|-0\\.6f, 0\\.6f|return 6\\.f|return 11\\.f|return 24\\.f|return 55\\.f|return 120\\.f|return 200\\.f" Source/DominionSimulation/Private/Economy/DAEconomySubsystem.cpp
FORMULA_SCAN_EXIT_CODE=0

$ rg -n "Tick\\(" Source/DominionSimulation/Public/Economy Source/DominionSimulation/Private/Economy
ECONOMY_TICK_SCAN_EXIT_CODE=1
(no independent economy Tick implementation)
```

These checks establish whitespace hygiene, source presence of the required v0.8 constants, and the lack of a per-frame economy tick only. They do not establish UHT/C++ compilation or Automation behavior.

## Concerns and assumptions

- UE 5.8 is unavailable, so UHT/C++ compatibility, subsystem initialization, delegate binding, and real Automation behavior remain unverified until the focused command runs on an installed UE runner.
- `ConditionOutputMultiplier` and demand are resolved inputs because v0.8 defines a condition output range and a conceptual demand formula, not a single universal coefficient for every facility. Their actual factor values are preserved in the output trace.
- Only standard modifiers use the +/-60% pool. Explicit Dominion/Fusion exceptions should be represented by a later, separately named effect path so standard bounds are never bypassed accidentally.
- Maintenance continues while a facility is non-producing; output availability gates production but does not erase deployment-cost upkeep. This follows the separation between v0.8 output and maintenance formulas.
- A Wonder without authored bespoke maintenance currently contributes zero Wonder upkeep; content validation should reject that configuration when Wonder definitions arrive rather than inventing a rate in simulation code.
- The boundedness test advances the real Task 7 clock by 3,000 active seconds and resolves exactly 100 discrete callbacks. It asserts literal final wallets and finite/solvent invariants, then advances to 120 cycles to check the v1.1 first-hour wallet target ranges. It is an explicit deterministic system fixture, not authored card tuning or a full player-spending model.

---

## Fix Round 1 — Honest boundedness scenario authority

Review base: `c998983a9a7ea55215f221cf5fd5d58910363741`.

### Reviewer finding addressed

The original 100-cycle fixture named the v0.8 balanced Synara block but assigned unsupported deployment costs and zero output to non-Founder facilities. That demonstrated only Founder Hall output minus invented upkeep and could be misread as a first-hour balance result.

`EconomySpec.cpp` now uses three synthetic facilities and explicit inputs only:

- initial scenario wallet: 20 Capital, 4 Insight, 4 Influence;
- Capital facility: 0.42 Capital/cycle, Retail, 20 deployment Capital;
- Insight facility: 0.04 Insight/cycle, Research, 10 deployment Capital;
- Influence facility: 0.03 Influence/cycle, Infrastructure, 10 deployment Capital.

The published v0.8 maintenance rates produce 0.27 total Capital upkeep/cycle, so the hand-derived net delta is 0.15 Capital, 0.04 Insight, and 0.03 Influence. The spec asserts the literal 100-cycle result of 35 Capital, 8 Insight, and 7 Influence plus finite/solvent state. It then advances the Task 7 clock to 120 cycles (one hour) and asserts v1.1's target ranges of 15-40 Capital, 4-12 Insight, and 4-10 Influence. These inputs are test data, not claims about Dominion card balance. Tasks 19/27/28 will bind real authored facility values.

### Fix-round TDD evidence

The revised Automation scenario was written before considering any production change. A focused RED attempt was made:

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Simulation.Economy;Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: line 2: UnrealEditor-Cmd: command not found
FIX_RED_EXIT_CODE=127
```

The runner is unavailable, so the revised scenario could not be observed failing or passing. The finding requires no production-code change: it corrects an unsupported test authority/claim while retaining coverage of the existing resolver. A final focused invocation is recorded below after the source/report correction.

### Fix-round final verification attempt

```text
$ UnrealEditor-Cmd "$(pwd)/DominionAscendant.uproject" -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests Dominion.Simulation.Economy;Quit" -TestExit="Automation Test Queue Empty"
/bin/bash: line 2: UnrealEditor-Cmd: command not found
FIX_GREEN_EXIT_CODE=127
```

This is environment-blocked and **not a passing Automation result**.

Supplementary source checks:

```text
$ git diff --check
DIFF_CHECK_EXIT_CODE=0

$ rg -n -i "synara|starter|founder hall|adaptive habitat|microgrid|water reclaimer|corner exchange|cognitive tower|autonomous exchange|neural relay|agency forum" Source/DominionTests/Private/Simulation/EconomySpec.cpp
UNSUPPORTED_NAMED_TUNING_SCAN_EXIT_CODE=1
(no named Dominion facility tuning in the boundedness spec)

$ rg -n "scenario\\.facility|35\\.f|8\\.f|7\\.f|>= 15\\.f.*<= 40\\.f|>= 4\\.f.*<= 12\\.f|>= 4\\.f.*<= 10\\.f|\\* 100\\.0|\\* 20\\.0" Source/DominionTests/Private/Simulation/EconomySpec.cpp
EXPLICIT_SCENARIO_SCAN_EXIT_CODE=0
```

These checks confirm only the source-level correction and whitespace hygiene. They do not replace UE compilation or Automation execution.
