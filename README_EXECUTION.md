# DOMINION // ASCENDANT — Codex Execution Handoff

## Objective

Execute the frozen vertical-slice implementation plan using **subagent-driven development** with review gates after every independently testable task.

## Authority Order

1. `docs/bibles/DOMINION_ASCENDANT_Vertical_Slice_Production_Spec_v1.1.md`
2. `docs/superpowers/plans/2026-08-22-dominion-ascendant-vertical-slice.md`
3. Earlier bibles v0.1–v1.0 for supporting canon

If there is a conflict, **v1.1 is binding**.

## Required workflow

1. Read the `superpowers:using-superpowers` skill.
2. Read `superpowers:using-git-worktrees`.
3. Create/verify an isolated worktree. Do not implement on main/master.
4. Read `superpowers:subagent-driven-development`.
5. Initialize the plan-specific SDD ledger.
6. Execute Tasks 1–28 continuously.
7. For every task:
   - fresh implementer subagent
   - tests first
   - implementation
   - self-review
   - task reviewer for spec compliance + code quality
   - fix/re-review if necessary
   - commit only when the task gate passes
8. Run a whole-branch review at the end.
9. Do not push, merge, publish, or deploy without explicit user approval.

## Technical baseline

- Windows PC vertical slice
- Unreal Engine **5.8**
- C++ authoritative gameplay/simulation
- Blueprints/Data Assets for authored content
- Gameplay Ability System
- Enhanced Input
- StateTree
- Mass Entity for crowd representation
- World Partition
- Niagara
- MetaSounds
- CommonUI
- Unreal Automation Tests
- Git LFS for binary assets

## Scope discipline

Do not add:
- a fourth major civilization
- multiplayer/co-op/PvP
- full 20-civilization production
- Axiom Crown
- 128×128 player city
- extra destruction targets beyond the frozen eight
- extra UI surfaces beyond the frozen set without a spec revision

The first implementation proof is **Adaptive Habitat**.
The final implementation proof is **Forgeweave Ascension → CONVERGENCE AUTHORITY: 1/20**.
