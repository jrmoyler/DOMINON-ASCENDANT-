#include "Campaign/DADaxtonCampaignState.h"

#include "Campaign/DAConquestCampaignState.h"
#include "Save/DACampaignSaveGame.h"

namespace
{
    bool IsPercent(const float Value)
    {
        return FMath::IsFinite(Value) && Value >= 0.f && Value <= 100.f;
    }

    bool HasObjective(const FDADaxtonCampaignState& State, const EDADaxtonChoiceObjective Objective)
    {
        return State.CompletedObjectives.Contains(Objective);
    }

    const FDAWorldAssetRecord* FindGrandForge(const FDACampaignSnapshot& Campaign)
    {
        return Campaign.WorldAssets.FindByPredicate([](const FDAWorldAssetRecord& Asset)
        {
            return Asset.CardDefinitionId == TEXT("forgeweave.grand_forge");
        });
    }

    bool Near(const float Left, const float Right)
    {
        return FMath::IsNearlyEqual(Left, Right, 0.001f);
    }

    bool ModuleProofEqual(const FDADaxtonModuleProof& Left, const FDADaxtonModuleProof& Right)
    {
        return Left.ModuleId == Right.ModuleId && Near(Left.CurrentHealth, Right.CurrentHealth)
            && Near(Left.MaximumHealth, Right.MaximumHealth)
            && Left.bDisablesProduction == Right.bDisablesProduction
            && Left.DamageState == Right.DamageState;
    }

    bool CitizenProofEqual(const FDADaxtonCitizenProof& Left, const FDADaxtonCitizenProof& Right)
    {
        return Left.CitizenId == Right.CitizenId && Left.CityId == Right.CityId
            && Left.HomeWorldAssetId == Right.HomeWorldAssetId && Left.JobId == Right.JobId;
    }

    bool OpeningProofEqual(const FDADaxtonJobOpeningProof& Left,
        const FDADaxtonJobOpeningProof& Right)
    {
        return Left.JobId == Right.JobId && Left.CityId == Right.CityId
            && Left.FacilityWorldAssetId == Right.FacilityWorldAssetId
            && Left.OpenPositions == Right.OpenPositions;
    }

    bool AssignmentProofEqual(const FDADaxtonJobAssignmentProof& Left,
        const FDADaxtonJobAssignmentProof& Right)
    {
        return Left.CitizenId == Right.CitizenId && Left.JobId == Right.JobId
            && Left.FacilityWorldAssetId == Right.FacilityWorldAssetId;
    }

    template <typename T, typename Equal>
    bool ArrayProofEqual(const TArray<T>& Left, const TArray<T>& Right, Equal Equals)
    {
        if (Left.Num() != Right.Num()) return false;
        for (int32 Index = 0; Index < Left.Num(); ++Index)
            if (!Equals(Left[Index], Right[Index])) return false;
        return true;
    }

    bool ProjectionEqual(const FDADaxtonCanonicalProjection& Left,
        const FDADaxtonCanonicalProjection& Right)
    {
        return Near(Left.ProductionReserve, Right.ProductionReserve)
            && Near(Left.LogisticsEfficiency, Right.LogisticsEfficiency)
            && Near(Left.IndustrialThroughput, Right.IndustrialThroughput)
            && Near(Left.ResourceHunger, Right.ResourceHunger)
            && Near(Left.DefensePressure, Right.DefensePressure)
            && Left.bOverdrive == Right.bOverdrive
            && Near(Left.Trust, Right.Trust) && Near(Left.Respect, Right.Respect)
            && Near(Left.Grievance, Right.Grievance)
            && Left.GrandForgeWorldAssetId == Right.GrandForgeWorldAssetId
            && Near(Left.GrandForgeStructuralIntegrity, Right.GrandForgeStructuralIntegrity)
            && Left.GrandForgeConstructionState == Right.GrandForgeConstructionState
            && Left.bProductionDisabled == Right.bProductionDisabled
            && Left.LiveSignalsRevision == Right.LiveSignalsRevision
            && ArrayProofEqual(Left.GrandForgeModules, Right.GrandForgeModules, ModuleProofEqual)
            && ArrayProofEqual(Left.Citizens, Right.Citizens, CitizenProofEqual)
            && ArrayProofEqual(Left.JobOpenings, Right.JobOpenings, OpeningProofEqual)
            && ArrayProofEqual(Left.JobAssignments, Right.JobAssignments, AssignmentProofEqual)
            && Left.HistoryTags == Right.HistoryTags;
    }

    bool ReplayResolutionRelationshipPrefix(const FDADaxtonCampaignState& Daxton,
        const FDADiplomaticRelationship& Relationship, float& OutTrust,
        float& OutRespect, float& OutGrievance, FString& OutError)
    {
        if (Daxton.ResolutionRelationshipReasonCount < 0
            || Daxton.ResolutionRelationshipReasonCount
                != Daxton.ResolutionRelationshipReasonMutationIds.Num()
            || Relationship.ReasonLedger.Num() < Daxton.ResolutionRelationshipReasonCount)
        {
            OutError = TEXT("Resolved Daxton relationship provenance requires one exact canonical reason prefix.");
            return false;
        }

        float Aggregates[6] = {};
        for (int32 Index = 0; Index < Daxton.ResolutionRelationshipReasonCount; ++Index)
        {
            const FDADiplomaticReason& Reason = Relationship.ReasonLedger[Index];
            const int32 MetricIndex = static_cast<int32>(Reason.Metric);
            if (Reason.MutationId != Daxton.ResolutionRelationshipReasonMutationIds[Index]
                || Reason.WorldTick > Daxton.ResolvedWorldTick
                || MetricIndex < 0 || MetricIndex >= UE_ARRAY_COUNT(Aggregates))
            {
                OutError = TEXT("Resolved Daxton relationship reason prefix was reordered, rewritten, or moved beyond resolution.");
                return false;
            }
            Aggregates[MetricIndex] += Reason.Magnitude;
        }
        for (int32 Index = Daxton.ResolutionRelationshipReasonCount;
            Index < Relationship.ReasonLedger.Num(); ++Index)
        {
            if (Relationship.ReasonLedger[Index].WorldTick
                < Daxton.ResolvedWorldTick)
            {
                OutError = TEXT("Post-resolution relationship reasons must follow the exact prefix without predating the Daxton resolution World Tick.");
                return false;
            }
        }
        OutTrust = Aggregates[static_cast<int32>(EDADiplomaticMetric::Trust)];
        OutRespect = Aggregates[static_cast<int32>(EDADiplomaticMetric::Respect)];
        OutGrievance = Aggregates[static_cast<int32>(EDADiplomaticMetric::Grievance)];
        return true;
    }

    void AddProofHistory(FDADaxtonCanonicalProjection& Projection, const FName Tag)
    {
        Projection.HistoryTags.AddUnique(Tag);
        Projection.HistoryTags.Sort([](const FName Left, const FName Right)
            { return Left.LexicalLess(Right); });
    }

    void ReconcileProofProduction(FDADaxtonCanonicalProjection& Projection)
    {
        Projection.bProductionDisabled = Projection.GrandForgeModules.ContainsByPredicate(
            [](const FDADaxtonModuleProof& Module)
            { return Module.bDisablesProduction && Module.CurrentHealth <= 0.f; });
    }

    uint8 ProofModuleState(const FDADaxtonModuleProof& Module)
    {
        if (Module.CurrentHealth <= 0.f) return static_cast<uint8>(EDAStructureDamageState::Disabled);
        const float Percent = 100.f * Module.CurrentHealth / Module.MaximumHealth;
        return static_cast<uint8>(Percent > 50.f ? EDAStructureDamageState::Operational
            : Percent > 25.f ? EDAStructureDamageState::Damaged
            : EDAStructureDamageState::Disabled);
    }

    bool ApplyExpectedProof(const FDADaxtonCanonicalActionRecord& Record,
        FDADaxtonCanonicalProjection& Expected, EDADaxtonEncounterPhase& ExpectedPhase,
        float& ExpectedArmor, float& ExpectedHeat, float& ExpectedCoolant,
        int32& ProductionActions, int32& CoverActions, int32& ReinforcementActions,
        FString& OutError)
    {
        if (!ProjectionEqual(Record.Before, Expected) || Record.PhaseBefore != ExpectedPhase
            || !Near(Record.ArmorBefore, ExpectedArmor) || !Near(Record.HeatBefore, ExpectedHeat)
            || !Near(Record.CoolantBefore, ExpectedCoolant))
        {
            OutError = TEXT("Daxton canonical action before-proof does not chain from prior authority.");
            return false;
        }
        switch (Record.Kind)
        {
        case EDADaxtonCanonicalActionKind::AdvanceProduction:
            if (ExpectedPhase != EDADaxtonEncounterPhase::PhaseOne || Record.Strength != 0.f) return false;
            Expected.ProductionReserve += 5.f;
            Expected.IndustrialThroughput += 2.f;
            Expected.ResourceHunger = FMath::Clamp(Expected.ResourceHunger + 2.f, 0.f, 100.f);
            ++ProductionActions;
            break;
        case EDADaxtonCanonicalActionKind::DeployHardenedCover:
        {
            if (ExpectedPhase != EDADaxtonEncounterPhase::PhaseOne
                || Expected.GrandForgeModules.ContainsByPredicate([](const FDADaxtonModuleProof& Module)
                    { return Module.ModuleId == TEXT("module.hardened_cover"); })) return false;
            FDADaxtonModuleProof& Cover = Expected.GrandForgeModules.Emplace_GetRef();
            Cover.ModuleId = TEXT("module.hardened_cover");
            Cover.CurrentHealth = 100.f;
            Cover.MaximumHealth = 100.f;
            Cover.DamageState = static_cast<uint8>(EDAStructureDamageState::Operational);
            Expected.GrandForgeModules.Sort([](const auto& Left, const auto& Right)
                { return Left.ModuleId.LexicalLess(Right.ModuleId); });
            ++CoverActions;
            break;
        }
        case EDADaxtonCanonicalActionKind::ReinforceForgeGuard:
        {
            if (ExpectedPhase != EDADaxtonEncounterPhase::PhaseOne
                || Expected.LiveSignalsRevision == MAX_int64) return false;
            FDADaxtonJobOpeningProof* Opening = Expected.JobOpenings.FindByPredicate(
                [&Expected](const FDADaxtonJobOpeningProof& Candidate)
                { return Candidate.JobId == TEXT("job.forgeweave.forge_guard.reinforcement")
                    && Candidate.FacilityWorldAssetId == Expected.GrandForgeWorldAssetId; });
            if (Opening == nullptr)
            {
                Opening = &Expected.JobOpenings.Emplace_GetRef();
                Opening->JobId = TEXT("job.forgeweave.forge_guard.reinforcement");
                Opening->CityId = TEXT("city.ironheart");
                Opening->FacilityWorldAssetId = Expected.GrandForgeWorldAssetId;
            }
            Opening->OpenPositions += 4;
            Expected.JobOpenings.Sort([](const auto& Left, const auto& Right)
                { return Left.JobId != Right.JobId ? Left.JobId.LexicalLess(Right.JobId)
                    : Left.FacilityWorldAssetId.ToString().Compare(
                        Right.FacilityWorldAssetId.ToString()) < 0; });
            Expected.DefensePressure = FMath::Clamp(Expected.DefensePressure + 5.f, 0.f, 100.f);
            ++Expected.LiveSignalsRevision;
            ++ReinforcementActions;
            break;
        }
        case EDADaxtonCanonicalActionKind::CompletePhaseOneIndustrialObjective:
            if (ExpectedPhase != EDADaxtonEncounterPhase::PhaseOne || ProductionActions < 1
                || CoverActions < 1 || ReinforcementActions < 1 || ExpectedArmor != 100.f
                || ExpectedHeat != 0.f || ExpectedCoolant != 0.f) return false;
            ExpectedPhase = EDADaxtonEncounterPhase::PhaseTwo;
            Expected.bOverdrive = true;
            Expected.IndustrialThroughput *= 1.5f;
            Expected.ResourceHunger = FMath::Clamp(Expected.ResourceHunger + 10.f, 0.f, 100.f);
            ExpectedHeat = FMath::Max(ExpectedHeat, 20.f);
            ExpectedCoolant = 100.f;
            break;
        case EDADaxtonCanonicalActionKind::SystemInteraction:
            if (Record.Strength <= 0.f || !FMath::IsFinite(Record.Strength)
                || (ExpectedPhase != EDADaxtonEncounterPhase::PhaseOne
                    && ExpectedPhase != EDADaxtonEncounterPhase::PhaseTwo)
                || (ExpectedPhase == EDADaxtonEncounterPhase::PhaseOne
                    && Record.Interaction != EDADaxtonInteraction::Damage)) return false;
            switch (Record.Interaction)
            {
            case EDADaxtonInteraction::Damage:
                ExpectedArmor = FMath::Clamp(ExpectedArmor - Record.Strength, 0.f, 100.f);
                ExpectedHeat = FMath::Clamp(ExpectedHeat + Record.Strength * 0.1f, 0.f, 100.f);
                break;
            case EDADaxtonInteraction::DisableCoolant:
                ExpectedCoolant = 0.f;
                ExpectedHeat = FMath::Clamp(ExpectedHeat + 25.f, 0.f, 100.f);
                Expected.ResourceHunger = FMath::Clamp(Expected.ResourceHunger + 5.f, 0.f, 100.f);
                if (FDADaxtonModuleProof* Module = Expected.GrandForgeModules.FindByPredicate(
                    [](const auto& Candidate) { return Candidate.ModuleId == TEXT("module.coolant"); }))
                { Module->CurrentHealth = 0.f; Module->DamageState = ProofModuleState(*Module); }
                ReconcileProofProduction(Expected);
                break;
            case EDADaxtonInteraction::RedirectSupply:
                Expected.ProductionReserve = FMath::Max(0.f, Expected.ProductionReserve - Record.Strength);
                Expected.IndustrialThroughput = FMath::Max(0.f,
                    Expected.IndustrialThroughput - Record.Strength * 0.5f);
                Expected.LogisticsEfficiency = FMath::Max(0.f, Expected.LogisticsEfficiency - Record.Strength);
                Expected.ResourceHunger = FMath::Clamp(Expected.ResourceHunger + 5.f, 0.f, 100.f);
                break;
            case EDADaxtonInteraction::HackProduction:
                Expected.IndustrialThroughput = FMath::Max(0.f,
                    Expected.IndustrialThroughput - Record.Strength);
                Expected.ProductionReserve = FMath::Max(0.f,
                    Expected.ProductionReserve - Record.Strength * 0.5f);
                ExpectedHeat = FMath::Clamp(ExpectedHeat + 10.f, 0.f, 100.f);
                if (FDADaxtonModuleProof* Module = Expected.GrandForgeModules.FindByPredicate(
                    [](const auto& Candidate) { return Candidate.ModuleId == TEXT("module.production"); }))
                { Module->CurrentHealth = FMath::Max(0.f, Module->CurrentHealth - Record.Strength);
                    Module->DamageState = ProofModuleState(*Module); }
                ReconcileProofProduction(Expected);
                break;
            case EDADaxtonInteraction::WorkerShutdown:
                if (Expected.LiveSignalsRevision == MAX_int64) return false;
                Expected.IndustrialThroughput *= 0.5f;
                ExpectedHeat = FMath::Max(0.f, ExpectedHeat - 15.f);
                for (FDADaxtonCitizenProof& Citizen : Expected.Citizens)
                    if (Citizen.CityId == TEXT("city.ironheart")) Citizen.JobId = NAME_None;
                Expected.JobAssignments.RemoveAll([](const FDADaxtonJobAssignmentProof& Assignment)
                    { return Assignment.JobId.ToString().StartsWith(TEXT("job.forgeweave.")); });
                ++Expected.LiveSignalsRevision;
                AddProofHistory(Expected, TEXT("workers_protected"));
                break;
            }
            if (ExpectedPhase == EDADaxtonEncounterPhase::PhaseOne && ExpectedArmor <= 60.f)
            {
                ExpectedPhase = EDADaxtonEncounterPhase::PhaseTwo;
                Expected.bOverdrive = true;
                Expected.IndustrialThroughput *= 1.5f;
                Expected.ResourceHunger = FMath::Clamp(Expected.ResourceHunger + 10.f, 0.f, 100.f);
                ExpectedHeat = FMath::Max(ExpectedHeat, 20.f);
                ExpectedCoolant = 100.f;
            }
            break;
        case EDADaxtonCanonicalActionKind::EnterChoicePhase:
            if (ExpectedPhase != EDADaxtonEncounterPhase::PhaseTwo) return false;
            ExpectedPhase = EDADaxtonEncounterPhase::PhaseThree;
            break;
        case EDADaxtonCanonicalActionKind::CompleteChoiceObjective:
            if (ExpectedPhase != EDADaxtonEncounterPhase::PhaseThree) return false;
            switch (Record.Objective)
            {
            case EDADaxtonChoiceObjective::DefeatDaxton:
                if (ExpectedArmor != 0.f) return false;
                break;
            case EDADaxtonChoiceObjective::SaveGrandForge:
                if (Expected.GrandForgeStructuralIntegrity <= 0.f
                    || Expected.GrandForgeConstructionState == static_cast<uint8>(EDAConstructionState::Ruined)) return false;
                AddProofHistory(Expected, TEXT("grand_forge_preserved"));
                break;
            case EDADaxtonChoiceObjective::EvacuateWorkers:
                if (!Expected.HistoryTags.Contains(TEXT("workers_protected"))) return false;
                break;
            case EDADaxtonChoiceObjective::StabilizeProductionOfferUnion:
                if (Expected.HistoryTags.Contains(TEXT("broker_force"))) return false;
                Expected.bOverdrive = false;
                ExpectedHeat = FMath::Min(ExpectedHeat, 50.f);
                ExpectedCoolant = FMath::Max(ExpectedCoolant, 50.f);
                for (FDADaxtonModuleProof& Module : Expected.GrandForgeModules)
                    if (Module.ModuleId == TEXT("module.coolant") || Module.ModuleId == TEXT("module.production"))
                    { Module.CurrentHealth = FMath::Max(Module.CurrentHealth, Module.MaximumHealth * 0.6f);
                        Module.DamageState = ProofModuleState(Module); }
                ReconcileProofProduction(Expected);
                break;
            }
            break;
        case EDADaxtonCanonicalActionKind::ResolveLeader:
            if (ExpectedPhase != EDADaxtonEncounterPhase::PhaseThree) return false;
            ExpectedPhase = EDADaxtonEncounterPhase::Resolved;
            AddProofHistory(Expected, TEXT("daxton_encounter_resolved"));
            AddProofHistory(Expected, FDADaxtonAuthorityValidator::GetLeaderHistoryTag(Record.LeaderState));
            break;
        }
        if (!ProjectionEqual(Record.After, Expected) || Record.PhaseAfter != ExpectedPhase
            || !Near(Record.ArmorAfter, ExpectedArmor) || !Near(Record.HeatAfter, ExpectedHeat)
            || !Near(Record.CoolantAfter, ExpectedCoolant))
        {
            OutError = TEXT("Daxton canonical action after-proof does not replay its exact campaign effects.");
            return false;
        }
        return true;
    }

    bool ValidateResolvedEvolution(const FDADaxtonCampaignState& Daxton,
        const FDADaxtonCanonicalProjection& Terminal,
        const FDADaxtonCanonicalProjection& Current, FString& OutError)
    {
        if (Current.GrandForgeWorldAssetId != Terminal.GrandForgeWorldAssetId
            || Current.LiveSignalsRevision < Terminal.LiveSignalsRevision)
        {
            OutError = TEXT("Resolved Daxton Grand Forge identity and terminal signal baseline are immutable.");
            return false;
        }
        TSet<FName> OwnedModuleIds;
        if (Daxton.HardenedCoverDeployments > 0)
            OwnedModuleIds.Add(TEXT("module.hardened_cover"));
        if (Daxton.InteractionRecords.ContainsByPredicate(
            [](const FDADaxtonInteractionRecord& Record)
            { return Record.Interaction == EDADaxtonInteraction::DisableCoolant; }))
            OwnedModuleIds.Add(TEXT("module.coolant"));
        if (Daxton.InteractionRecords.ContainsByPredicate(
            [](const FDADaxtonInteractionRecord& Record)
            { return Record.Interaction == EDADaxtonInteraction::HackProduction; }))
            OwnedModuleIds.Add(TEXT("module.production"));
        for (const FName OwnedModuleId : OwnedModuleIds)
        {
            const FDADaxtonModuleProof* ExpectedModule = Terminal.GrandForgeModules.FindByPredicate(
                [OwnedModuleId](const FDADaxtonModuleProof& Module)
                { return Module.ModuleId == OwnedModuleId; });
            const FDADaxtonModuleProof* CurrentModule = Current.GrandForgeModules.FindByPredicate(
                [OwnedModuleId](const FDADaxtonModuleProof& Module)
                { return Module.ModuleId == OwnedModuleId; });
            if (ExpectedModule == nullptr || CurrentModule == nullptr
                || !ModuleProofEqual(*CurrentModule, *ExpectedModule))
            {
                OutError = TEXT("Resolved Daxton action-owned Grand Forge module effects cannot drift.");
                return false;
            }
        }

        TArray<FName> RequiredHistory = {
            TEXT("daxton_encounter_resolved"),
            FDADaxtonAuthorityValidator::GetLeaderHistoryTag(Daxton.LeaderState)
        };
        if (Daxton.InteractionRecords.ContainsByPredicate(
            [](const FDADaxtonInteractionRecord& Record)
            { return Record.Interaction == EDADaxtonInteraction::WorkerShutdown; }))
            RequiredHistory.Add(TEXT("workers_protected"));
        if (Daxton.CompletedObjectives.Contains(EDADaxtonChoiceObjective::SaveGrandForge))
            RequiredHistory.Add(TEXT("grand_forge_preserved"));
        static const FName RouteTags[] = {
            TEXT("broker_force"), TEXT("broker_economic"),
            TEXT("broker_influence"), TEXT("broker_alliance")
        };
        for (const FName RouteTag : RouteTags)
            if (Terminal.HistoryTags.Contains(RouteTag)) RequiredHistory.Add(RouteTag);
        for (const FName RequiredTag : RequiredHistory)
        {
            if (RequiredTag.IsNone() || !Current.HistoryTags.Contains(RequiredTag))
            {
                OutError = TEXT("Resolved Daxton route, worker, Forge, and outcome history cannot be removed.");
                return false;
            }
        }

        const FDADaxtonCanonicalActionRecord* WorkerShutdown =
            Daxton.CanonicalActionRecords.FindByPredicate(
                [](const FDADaxtonCanonicalActionRecord& Record)
                { return Record.Kind == EDADaxtonCanonicalActionKind::SystemInteraction
                    && Record.Interaction == EDADaxtonInteraction::WorkerShutdown; });
        if (WorkerShutdown != nullptr)
        {
            for (const FDADaxtonCitizenProof& BeforeCitizen : WorkerShutdown->Before.Citizens)
            {
                if (BeforeCitizen.CityId != TEXT("city.ironheart")) continue;
                const FDADaxtonCitizenProof* Protected = Current.Citizens.FindByPredicate(
                    [&BeforeCitizen](const FDADaxtonCitizenProof& Citizen)
                    { return Citizen.CitizenId == BeforeCitizen.CitizenId; });
                const FDADaxtonCitizenProof* Expected = WorkerShutdown->After.Citizens.FindByPredicate(
                    [&BeforeCitizen](const FDADaxtonCitizenProof& Citizen)
                    { return Citizen.CitizenId == BeforeCitizen.CitizenId; });
                if (Protected == nullptr || Expected == nullptr
                    || !CitizenProofEqual(*Protected, *Expected))
                {
                    OutError = TEXT("Resolved Daxton protected-worker effects cannot be reassigned or removed.");
                    return false;
                }
            }
            for (const FDADaxtonJobAssignmentProof& Removed : WorkerShutdown->Before.JobAssignments)
            {
                if (!Removed.JobId.ToString().StartsWith(TEXT("job.forgeweave."))) continue;
                if (Current.JobAssignments.ContainsByPredicate(
                    [&Removed](const FDADaxtonJobAssignmentProof& Assignment)
                    { return Assignment.CitizenId == Removed.CitizenId; }))
                {
                    OutError = TEXT("Resolved Daxton evacuated workers cannot regain an assignment.");
                    return false;
                }
            }
        }

        const FDADaxtonJobOpeningProof* TerminalReinforcement =
            Terminal.JobOpenings.FindByPredicate([](const FDADaxtonJobOpeningProof& Opening)
            { return Opening.JobId == TEXT("job.forgeweave.forge_guard.reinforcement"); });
        if (TerminalReinforcement != nullptr)
        {
            const FDADaxtonJobOpeningProof* CurrentReinforcement =
                Current.JobOpenings.FindByPredicate([](const FDADaxtonJobOpeningProof& Opening)
                { return Opening.JobId == TEXT("job.forgeweave.forge_guard.reinforcement"); });
            if (CurrentReinforcement == nullptr
                || !OpeningProofEqual(*CurrentReinforcement, *TerminalReinforcement))
            {
                OutError = TEXT("Resolved Daxton Forge Guard reinforcement authority cannot drift.");
                return false;
            }
        }
        return true;
    }
}

bool FDADaxtonCampaignState::ValidateStandalone(FString& OutError) const
{
    if (Phase == EDADaxtonEncounterPhase::Inactive)
    {
        if (StartActionId.IsValid() || StartedWorldTick != 0 || ArmorIntegrity != 0.f
            || Heat != 0.f || CoolantStability != 0.f || bPoweredArmorActive
            || ForgeGuardReinforcements != 0 || HardenedCoverDeployments != 0
            || GrandForgeProductionCycles != 0 || !PhaseOneActionIds.IsEmpty()
            || bPhaseOneIndustrialObjectiveCompleted || PhaseOneObjectiveActionId.IsValid()
            || !InteractionRecords.IsEmpty()
            || PhaseThreeActionId.IsValid() || !CompletedObjectives.IsEmpty()
            || !ObjectiveActionIds.IsEmpty() || bLeaderResolved
            || ResolutionActionId.IsValid() || ResolvedWorldTick != 0
            || ResolutionRelationshipReasonCount != 0
            || !ResolutionRelationshipReasonMutationIds.IsEmpty()
            || !CanonicalActionRecords.IsEmpty()
            || InitialCanonicalProjection.GrandForgeWorldAssetId.IsValid()
            || !InitialCanonicalProjection.GrandForgeModules.IsEmpty()
            || !InitialCanonicalProjection.Citizens.IsEmpty()
            || !InitialCanonicalProjection.JobOpenings.IsEmpty()
            || !InitialCanonicalProjection.JobAssignments.IsEmpty()
            || !InitialCanonicalProjection.HistoryTags.IsEmpty())
        {
            OutError = TEXT("Inactive Daxton authority must be canonical empty.");
            return false;
        }
        return true;
    }

    if (!StartActionId.IsValid() || StartedWorldTick < 0 || !IsPercent(ArmorIntegrity)
        || !IsPercent(Heat) || !IsPercent(CoolantStability)
        || ForgeGuardReinforcements < 0 || HardenedCoverDeployments < 0
        || GrandForgeProductionCycles < 0)
    {
        OutError = TEXT("Active Daxton encounter state requires bounded systemic meters and a durable start action.");
        return false;
    }
    if (bPhaseOneIndustrialObjectiveCompleted != PhaseOneObjectiveActionId.IsValid()
        || (Phase == EDADaxtonEncounterPhase::PhaseOne
            && (ArmorIntegrity <= 60.f || bPhaseOneIndustrialObjectiveCompleted))
        || (Phase != EDADaxtonEncounterPhase::PhaseOne
            && ArmorIntegrity > 60.f && !bPhaseOneIndustrialObjectiveCompleted))
    {
        OutError = TEXT("Daxton phase must reconcile with the exact sixty-percent Phase I boundary.");
        return false;
    }
    if (bPoweredArmorActive == (Phase == EDADaxtonEncounterPhase::Resolved))
    {
        OutError = TEXT("Powered armor must remain active through the choice phase and deactivate only at resolution.");
        return false;
    }

    TSet<FGuid> ActionIds;
    for (const FGuid ActionId : PhaseOneActionIds)
    {
        if (!ActionId.IsValid() || ActionId == StartActionId || ActionIds.Contains(ActionId))
        {
            OutError = TEXT("Daxton Phase I mechanics require unique durable action identities.");
            return false;
        }
        ActionIds.Add(ActionId);
    }
    if (PhaseOneObjectiveActionId.IsValid())
    {
        if (PhaseOneObjectiveActionId == StartActionId || ActionIds.Contains(PhaseOneObjectiveActionId))
        {
            OutError = TEXT("Daxton Phase I industrial objective requires one unique durable action.");
            return false;
        }
        ActionIds.Add(PhaseOneObjectiveActionId);
    }
    if (PhaseOneActionIds.Num() != ForgeGuardReinforcements
            + HardenedCoverDeployments + GrandForgeProductionCycles)
    {
        OutError = TEXT("Daxton Phase I mechanic counters must reconcile with their durable actions.");
        return false;
    }
    int64 PreviousTick = StartedWorldTick;
    float ReplayedArmor = 100.f;
    float ReplayedHeat = bPhaseOneIndustrialObjectiveCompleted ? 20.f : 0.f;
    float ReplayedCoolant = bPhaseOneIndustrialObjectiveCompleted ? 100.f : 0.f;
    float PreviousProductionAfter = 0.f;
    float PreviousHungerAfter = 0.f;
    bool bHasPreviousSystemRecord = false;
    bool bReplayedOverdrive = bPhaseOneIndustrialObjectiveCompleted;
    TSet<EDADaxtonInteraction> ReplayedSystemInteractions;
    for (const FDADaxtonInteractionRecord& Record : InteractionRecords)
    {
        if (!Record.ActionId.IsValid() || Record.ActionId == StartActionId
            || ActionIds.Contains(Record.ActionId)
            || static_cast<uint8>(Record.Interaction) > static_cast<uint8>(EDADaxtonInteraction::WorkerShutdown)
            || !FMath::IsFinite(Record.Strength) || Record.Strength <= 0.f
            || !IsPercent(Record.ArmorIntegrityBefore) || !IsPercent(Record.ArmorIntegrityAfter)
            || !IsPercent(Record.HeatBefore) || !IsPercent(Record.HeatAfter)
            || !IsPercent(Record.CoolantBefore) || !IsPercent(Record.CoolantAfter)
            || !FMath::IsFinite(Record.ProductionBefore) || Record.ProductionBefore < 0.f
            || !FMath::IsFinite(Record.ProductionAfter) || Record.ProductionAfter < 0.f
            || !IsPercent(Record.ResourceHungerBefore) || !IsPercent(Record.ResourceHungerAfter)
            || Record.WorldTick < PreviousTick)
        {
            OutError = TEXT("Daxton interaction audit requires unique actions, ordered time, and bounded before/after state.");
            return false;
        }
        if (!FMath::IsNearlyEqual(Record.ArmorIntegrityBefore, ReplayedArmor, 0.001f)
            || !FMath::IsNearlyEqual(Record.HeatBefore, ReplayedHeat, 0.001f)
            || !FMath::IsNearlyEqual(Record.CoolantBefore, ReplayedCoolant, 0.001f)
            || (bHasPreviousSystemRecord
                && (!FMath::IsNearlyEqual(Record.ProductionBefore, PreviousProductionAfter, 0.001f)
                    || !FMath::IsNearlyEqual(Record.ResourceHungerBefore, PreviousHungerAfter, 0.001f))))
        {
            OutError = TEXT("Daxton interaction before-state must chain exactly from the previous durable mutation.");
            return false;
        }
        float ExpectedArmor = ReplayedArmor;
        float ExpectedHeat = ReplayedHeat;
        float ExpectedCoolant = ReplayedCoolant;
        float ExpectedProduction = Record.ProductionBefore;
        float ExpectedHunger = Record.ResourceHungerBefore;
        switch (Record.Interaction)
        {
        case EDADaxtonInteraction::Damage:
            ExpectedArmor = FMath::Clamp(ExpectedArmor - Record.Strength, 0.f, 100.f);
            ExpectedHeat = FMath::Clamp(ExpectedHeat + Record.Strength * 0.1f, 0.f, 100.f);
            break;
        case EDADaxtonInteraction::DisableCoolant:
            ExpectedCoolant = 0.f;
            ExpectedHeat = FMath::Clamp(ExpectedHeat + 25.f, 0.f, 100.f);
            ExpectedHunger = FMath::Clamp(ExpectedHunger + 5.f, 0.f, 100.f);
            break;
        case EDADaxtonInteraction::RedirectSupply:
            ExpectedProduction = FMath::Max(0.f, ExpectedProduction - Record.Strength * 0.5f);
            ExpectedHunger = FMath::Clamp(ExpectedHunger + 5.f, 0.f, 100.f);
            break;
        case EDADaxtonInteraction::HackProduction:
            ExpectedProduction = FMath::Max(0.f, ExpectedProduction - Record.Strength);
            ExpectedHeat = FMath::Clamp(ExpectedHeat + 10.f, 0.f, 100.f);
            break;
        case EDADaxtonInteraction::WorkerShutdown:
            ExpectedProduction *= 0.5f;
            ExpectedHeat = FMath::Max(0.f, ExpectedHeat - 15.f);
            break;
        }
        if (!bReplayedOverdrive && ExpectedArmor <= 60.f)
        {
            bReplayedOverdrive = true;
            ExpectedProduction *= 1.5f;
            ExpectedHunger = FMath::Clamp(ExpectedHunger + 10.f, 0.f, 100.f);
            ExpectedHeat = FMath::Max(ExpectedHeat, 20.f);
            ExpectedCoolant = 100.f;
        }
        if (!FMath::IsNearlyEqual(Record.ArmorIntegrityAfter, ExpectedArmor, 0.001f)
            || !FMath::IsNearlyEqual(Record.HeatAfter, ExpectedHeat, 0.001f)
            || !FMath::IsNearlyEqual(Record.CoolantAfter, ExpectedCoolant, 0.001f)
            || !FMath::IsNearlyEqual(Record.ProductionAfter, ExpectedProduction, 0.001f)
            || !FMath::IsNearlyEqual(Record.ResourceHungerAfter, ExpectedHunger, 0.001f))
        {
            OutError = TEXT("Daxton interaction after-state must replay the exact systemic mechanic.");
            return false;
        }
        ReplayedArmor = ExpectedArmor;
        ReplayedHeat = ExpectedHeat;
        ReplayedCoolant = ExpectedCoolant;
        PreviousProductionAfter = ExpectedProduction;
        PreviousHungerAfter = ExpectedHunger;
        bHasPreviousSystemRecord = true;
        if (Record.Interaction != EDADaxtonInteraction::Damage)
            ReplayedSystemInteractions.Add(Record.Interaction);
        ActionIds.Add(Record.ActionId);
        PreviousTick = Record.WorldTick;
    }

    const bool bHasChoicePhase = Phase == EDADaxtonEncounterPhase::PhaseThree
        || Phase == EDADaxtonEncounterPhase::Resolved;
    if (bHasChoicePhase != PhaseThreeActionId.IsValid()
        || (PhaseThreeActionId.IsValid()
            && (PhaseThreeActionId == StartActionId || ActionIds.Contains(PhaseThreeActionId))))
    {
        OutError = TEXT("Daxton choice phase requires one unique durable transition action.");
        return false;
    }
    if (PhaseThreeActionId.IsValid()) ActionIds.Add(PhaseThreeActionId);

    const bool bStabilized = CompletedObjectives.Contains(
        EDADaxtonChoiceObjective::StabilizeProductionOfferUnion);
    if (bStabilized)
    {
        ReplayedHeat = FMath::Min(ReplayedHeat, 50.f);
        ReplayedCoolant = FMath::Max(ReplayedCoolant, 50.f);
    }
    if (!FMath::IsNearlyEqual(ArmorIntegrity, ReplayedArmor, 0.001f)
        || !FMath::IsNearlyEqual(Heat, ReplayedHeat, 0.001f)
        || !FMath::IsNearlyEqual(CoolantStability, ReplayedCoolant, 0.001f)
        || (Phase != EDADaxtonEncounterPhase::PhaseOne && !bReplayedOverdrive)
        || ((Phase == EDADaxtonEncounterPhase::PhaseThree || Phase == EDADaxtonEncounterPhase::Resolved)
            && ReplayedSystemInteractions.Num() < 2))
    {
        OutError = TEXT("Daxton live phase/meters must equal the replayed systemic interaction ledger.");
        return false;
    }

    if (CompletedObjectives.Num() != ObjectiveActionIds.Num())
    {
        OutError = TEXT("Daxton choice objectives must reconcile one-for-one with durable actions.");
        return false;
    }
    TSet<EDADaxtonChoiceObjective> ObjectiveSet;
    for (int32 ObjectiveIndex = 0; ObjectiveIndex < CompletedObjectives.Num(); ++ObjectiveIndex)
    {
        const EDADaxtonChoiceObjective Objective = CompletedObjectives[ObjectiveIndex];
        const FGuid ObjectiveActionId = ObjectiveActionIds[ObjectiveIndex];
        if (static_cast<uint8>(Objective) > static_cast<uint8>(EDADaxtonChoiceObjective::StabilizeProductionOfferUnion)
            || ObjectiveSet.Contains(Objective) || !ObjectiveActionId.IsValid()
            || ObjectiveActionId == StartActionId || ActionIds.Contains(ObjectiveActionId))
        {
            OutError = TEXT("Daxton choice objectives require unique members and durable action identities.");
            return false;
        }
        ObjectiveSet.Add(Objective);
        ActionIds.Add(ObjectiveActionId);
    }
    if (Phase < EDADaxtonEncounterPhase::PhaseThree && !CompletedObjectives.IsEmpty())
    {
        OutError = TEXT("Daxton choice objectives cannot complete before Phase III.");
        return false;
    }
    if (bLeaderResolved != (Phase == EDADaxtonEncounterPhase::Resolved)
        || bLeaderResolved != ResolutionActionId.IsValid()
        || (!bLeaderResolved && ResolvedWorldTick != 0)
        || (!bLeaderResolved && (ResolutionRelationshipReasonCount != 0
            || !ResolutionRelationshipReasonMutationIds.IsEmpty()))
        || (bLeaderResolved && (ResolutionRelationshipReasonCount < 0
            || ResolutionRelationshipReasonCount
                != ResolutionRelationshipReasonMutationIds.Num()))
        || (bLeaderResolved && (ResolvedWorldTick < StartedWorldTick
            || ResolutionActionId == StartActionId || ActionIds.Contains(ResolutionActionId))))
    {
        OutError = TEXT("Daxton Leader resolution requires one exact terminal phase, action, and World Tick.");
        return false;
    }
    return true;
}

bool FDADaxtonAuthorityValidator::ResolveCanonicalRoute(const FDACampaignSnapshot& Campaign,
    EDAForgeweaveRoute& OutRoute, FString& OutError)
{
    struct FRouteTag { FName Tag; EDAForgeweaveRoute Route; };
    static const FRouteTag Tags[] = {
        {TEXT("broker_force"), EDAForgeweaveRoute::Force},
        {TEXT("broker_economic"), EDAForgeweaveRoute::Economic},
        {TEXT("broker_influence"), EDAForgeweaveRoute::Influence},
        {TEXT("broker_alliance"), EDAForgeweaveRoute::Alliance}
    };
    int32 Matches = 0;
    for (const FRouteTag& Candidate : Tags)
    {
        if (Campaign.HistoryTags.Contains(Candidate.Tag))
        {
            OutRoute = Candidate.Route;
            ++Matches;
        }
    }
    if (Matches != 1)
    {
        OutError = TEXT("Daxton encounter requires exactly one canonical Broker of Ironheart route history tag.");
        return false;
    }
    if (Campaign.ConquestState.bForgeweaveResolved
        && Campaign.ConquestState.ResolvedRoute != OutRoute)
    {
        OutError = TEXT("Daxton Broker route must reconcile with the canonical completed conquest route.");
        return false;
    }
    return true;
}

FName FDADaxtonAuthorityValidator::GetLeaderHistoryTag(const EDADaxtonLeaderState State)
{
    switch (State)
    {
    case EDADaxtonLeaderState::Governor: return TEXT("daxton_governor");
    case EDADaxtonLeaderState::IndustrialAdvisor: return TEXT("daxton_industrial_advisor");
    case EDADaxtonLeaderState::AlliedForgeLord: return TEXT("daxton_allied_forge_lord");
    case EDADaxtonLeaderState::Exile: return TEXT("daxton_exiled");
    case EDADaxtonLeaderState::Prisoner: return TEXT("daxton_prisoner");
    case EDADaxtonLeaderState::Dead: return TEXT("daxton_dead");
    default: return NAME_None;
    }
}

bool FDADaxtonAuthorityValidator::CaptureCanonicalProjection(const FDACampaignSnapshot& Campaign,
    FDADaxtonCanonicalProjection& OutProjection, FString& OutError)
{
    OutProjection = FDADaxtonCanonicalProjection();
    const FDAWorldAssetRecord* Forge = FindGrandForge(Campaign);
    const FDAStructuralDamageRecord* Damage = Forge != nullptr
        ? Campaign.OperationConflict.FindStructuralDamageRecord(Forge->WorldAssetId) : nullptr;
    const FDADiplomaticRelationship* Relationship = Campaign.WorldState.Diplomacy.FindRelationship(
        TEXT("relationship.synara.forgeweave"));
    if (Forge == nullptr || Damage == nullptr || Relationship == nullptr)
    {
        OutError = TEXT("Daxton canonical proof requires relationship and Grand Forge structural authorities.");
        return false;
    }
    const FDAForgeweaveCityState& Forgeweave = Campaign.WorldState.Forgeweave;
    OutProjection.ProductionReserve = Forgeweave.ProductionReserve;
    OutProjection.LogisticsEfficiency = Forgeweave.LogisticsEfficiency;
    OutProjection.IndustrialThroughput = Forgeweave.ActiveIndustrialThroughput;
    OutProjection.ResourceHunger = Forgeweave.ResourceHunger;
    OutProjection.DefensePressure = Forgeweave.DefensePressure;
    OutProjection.bOverdrive = Forgeweave.bOverdrive;
    OutProjection.Trust = Relationship->Trust;
    OutProjection.Respect = Relationship->Respect;
    OutProjection.Grievance = Relationship->Grievance;
    OutProjection.GrandForgeWorldAssetId = Forge->WorldAssetId;
    OutProjection.GrandForgeStructuralIntegrity = Forge->StructuralIntegrity;
    OutProjection.GrandForgeConstructionState = static_cast<uint8>(Forge->ConstructionState);
    OutProjection.bProductionDisabled = Damage->bProductionDisabled;
    OutProjection.LiveSignalsRevision = Campaign.LiveSignals.MutationRevision;
    for (const FDAStructureModuleHealthRecord& Module : Damage->Modules)
    {
        FDADaxtonModuleProof& Proof = OutProjection.GrandForgeModules.Emplace_GetRef();
        Proof.ModuleId = Module.ModuleId;
        Proof.CurrentHealth = Module.CurrentHealth;
        Proof.MaximumHealth = Module.MaximumHealth;
        Proof.bDisablesProduction = Module.bDisablesProduction;
        Proof.DamageState = static_cast<uint8>(Module.State);
    }
    OutProjection.GrandForgeModules.Sort([](const auto& Left, const auto& Right)
        { return Left.ModuleId.LexicalLess(Right.ModuleId); });
    for (const FDACampaignCitizenSignal& Citizen : Campaign.LiveSignals.Citizens)
    {
        FDADaxtonCitizenProof& Proof = OutProjection.Citizens.Emplace_GetRef();
        Proof.CitizenId = Citizen.CitizenId;
        Proof.CityId = Citizen.CityId;
        Proof.HomeWorldAssetId = Citizen.HomeWorldAssetId;
        Proof.JobId = Citizen.JobId;
    }
    OutProjection.Citizens.Sort([](const auto& Left, const auto& Right)
        { return Left.CitizenId.LexicalLess(Right.CitizenId); });
    for (const FDACampaignJobOpeningSignal& Opening : Campaign.LiveSignals.JobOpenings)
    {
        FDADaxtonJobOpeningProof& Proof = OutProjection.JobOpenings.Emplace_GetRef();
        Proof.JobId = Opening.JobId;
        Proof.CityId = Opening.CityId;
        Proof.FacilityWorldAssetId = Opening.FacilityWorldAssetId;
        Proof.OpenPositions = Opening.OpenPositions;
    }
    OutProjection.JobOpenings.Sort([](const auto& Left, const auto& Right)
        { return Left.JobId != Right.JobId ? Left.JobId.LexicalLess(Right.JobId)
            : Left.FacilityWorldAssetId.ToString().Compare(
                Right.FacilityWorldAssetId.ToString()) < 0; });
    for (const FDACampaignJobAssignmentSignal& Assignment : Campaign.LiveSignals.JobAssignments)
    {
        FDADaxtonJobAssignmentProof& Proof = OutProjection.JobAssignments.Emplace_GetRef();
        Proof.CitizenId = Assignment.CitizenId;
        Proof.JobId = Assignment.JobId;
        Proof.FacilityWorldAssetId = Assignment.FacilityWorldAssetId;
    }
    OutProjection.JobAssignments.Sort([](const auto& Left, const auto& Right)
        { return Left.CitizenId.LexicalLess(Right.CitizenId); });
    OutProjection.HistoryTags = Campaign.HistoryTags;
    return true;
}

bool FDADaxtonAuthorityValidator::CanResolveLeaderState(const EDADaxtonLeaderState State,
    const FDACampaignSnapshot& Campaign, FString& OutError)
{
    const FDADaxtonCampaignState& Daxton = Campaign.DaxtonState;
    if (Daxton.Phase != EDADaxtonEncounterPhase::PhaseThree || Daxton.bLeaderResolved)
    {
        OutError = TEXT("Daxton outcome selection is available only in unresolved Phase III.");
        return false;
    }
    EDAForgeweaveRoute Route = EDAForgeweaveRoute::Force;
    if (!ResolveCanonicalRoute(Campaign, Route, OutError)) return false;
    const FDADiplomaticRelationship* Relationship = Campaign.WorldState.Diplomacy.FindRelationship(
        TEXT("relationship.synara.forgeweave"));
    if (Relationship == nullptr)
    {
        OutError = TEXT("Daxton outcome matrix requires canonical Forgeweave Trust, Respect, and Grievance.");
        return false;
    }
    const bool bDefeated = HasObjective(Daxton, EDADaxtonChoiceObjective::DefeatDaxton);
    const bool bForgeSaved = HasObjective(Daxton, EDADaxtonChoiceObjective::SaveGrandForge);
    const bool bWorkersSafe = HasObjective(Daxton, EDADaxtonChoiceObjective::EvacuateWorkers);
    const bool bUnion = HasObjective(Daxton, EDADaxtonChoiceObjective::StabilizeProductionOfferUnion);
    const bool bSystemicEnding = bForgeSaved || bWorkersSafe || bUnion;
    bool bAllowed = false;
    switch (State)
    {
    case EDADaxtonLeaderState::Governor:
        bAllowed = Route == EDAForgeweaveRoute::Influence && bWorkersSafe && bUnion
            && Relationship->Trust >= 50.f && Relationship->Respect >= 50.f
            && Relationship->Grievance < 50.f;
        break;
    case EDADaxtonLeaderState::IndustrialAdvisor:
        bAllowed = Route == EDAForgeweaveRoute::Economic && bForgeSaved && bUnion
            && Relationship->Respect >= 50.f && Relationship->Grievance < 70.f;
        break;
    case EDADaxtonLeaderState::AlliedForgeLord:
        bAllowed = Route == EDAForgeweaveRoute::Alliance && bForgeSaved && bWorkersSafe && bUnion
            && Relationship->Trust >= 65.f && Relationship->Respect >= 65.f
            && Relationship->Grievance < 50.f;
        break;
    case EDADaxtonLeaderState::Exile:
        bAllowed = (Route == EDAForgeweaveRoute::Force || Route == EDAForgeweaveRoute::Influence)
            && bDefeated && bSystemicEnding;
        break;
    case EDADaxtonLeaderState::Prisoner:
        bAllowed = Route == EDAForgeweaveRoute::Force && bDefeated && bWorkersSafe;
        break;
    case EDADaxtonLeaderState::Dead:
        bAllowed = Route == EDAForgeweaveRoute::Force && bDefeated && bSystemicEnding;
        break;
    }
    if (!bAllowed)
    {
        OutError = TEXT("Selected Daxton Leader state is not allowed by route, objectives, Trust, Respect, and Grievance.");
    }
    return bAllowed;
}

bool FDADaxtonAuthorityValidator::ValidateCampaignState(
    const FDACampaignSnapshot& Campaign, FString& OutError)
{
    const FDADaxtonCampaignState& Daxton = Campaign.DaxtonState;
    if (!Daxton.ValidateStandalone(OutError)) return false;
    if (Daxton.Phase == EDADaxtonEncounterPhase::Inactive) return true;
    EDAForgeweaveRoute Route = EDAForgeweaveRoute::Force;
    if (!ResolveCanonicalRoute(Campaign, Route, OutError)) return false;
    const FDADiplomaticRelationship* Relationship = Campaign.WorldState.Diplomacy.FindRelationship(
        TEXT("relationship.synara.forgeweave"));
    const FDAWorldAssetRecord* Forge = FindGrandForge(Campaign);
    const FDAStructuralDamageRecord* Damage = Forge != nullptr
        ? Campaign.OperationConflict.FindStructuralDamageRecord(Forge->WorldAssetId) : nullptr;
    if (Relationship == nullptr || Forge == nullptr || Damage == nullptr
        || Campaign.WorldState.Forgeweave.Population <= 0
        || !FMath::IsFinite(Campaign.WorldState.Forgeweave.ProductionReserve)
        || !FMath::IsFinite(Campaign.WorldState.Forgeweave.ActiveIndustrialThroughput)
        || !FMath::IsFinite(Campaign.WorldState.Forgeweave.ResourceHunger))
    {
        OutError = TEXT("Active Daxton state must resolve canonical relationship, Grand Forge modules, workers, supply, production, and structure.");
        return false;
    }
    FDADaxtonCanonicalProjection ActualProjection;
    if (!CaptureCanonicalProjection(Campaign, ActualProjection, OutError)) return false;
    FDADaxtonCanonicalProjection ExpectedProjection = Daxton.InitialCanonicalProjection;
    EDADaxtonEncounterPhase ExpectedPhase = EDADaxtonEncounterPhase::PhaseOne;
    float ExpectedArmor = 100.f;
    float ExpectedHeat = 0.f;
    float ExpectedCoolant = 0.f;
    int32 ProductionActions = 0;
    int32 CoverActions = 0;
    int32 ReinforcementActions = 0;
    int64 PreviousProofTick = Daxton.StartedWorldTick;
    TSet<FGuid> ProofActionIds;
    const int32 ExpectedProofCount = Daxton.PhaseOneActionIds.Num()
        + (Daxton.PhaseOneObjectiveActionId.IsValid() ? 1 : 0)
        + Daxton.InteractionRecords.Num() + (Daxton.PhaseThreeActionId.IsValid() ? 1 : 0)
        + Daxton.ObjectiveActionIds.Num() + (Daxton.ResolutionActionId.IsValid() ? 1 : 0);
    if (Daxton.CanonicalActionRecords.Num() != ExpectedProofCount
        || !Daxton.InitialCanonicalProjection.GrandForgeWorldAssetId.IsValid())
    {
        OutError = TEXT("Daxton canonical proof ledger must cover every durable encounter action exactly once.");
        return false;
    }
    for (const FDADaxtonCanonicalActionRecord& Record : Daxton.CanonicalActionRecords)
    {
        if (!Record.ActionId.IsValid() || ProofActionIds.Contains(Record.ActionId)
            || Record.WorldTick < PreviousProofTick)
        {
            OutError = TEXT("Daxton canonical proof actions require unique ids and ordered World Ticks.");
            return false;
        }
        bool bMatchesActionAuthority = false;
        switch (Record.Kind)
        {
        case EDADaxtonCanonicalActionKind::AdvanceProduction:
        case EDADaxtonCanonicalActionKind::DeployHardenedCover:
        case EDADaxtonCanonicalActionKind::ReinforceForgeGuard:
            bMatchesActionAuthority = Daxton.PhaseOneActionIds.Contains(Record.ActionId);
            break;
        case EDADaxtonCanonicalActionKind::CompletePhaseOneIndustrialObjective:
            bMatchesActionAuthority = Record.ActionId == Daxton.PhaseOneObjectiveActionId;
            break;
        case EDADaxtonCanonicalActionKind::SystemInteraction:
            bMatchesActionAuthority = Daxton.InteractionRecords.ContainsByPredicate(
                [&Record](const FDADaxtonInteractionRecord& Interaction)
                { return Interaction.ActionId == Record.ActionId
                    && Interaction.Interaction == Record.Interaction
                    && Near(Interaction.Strength, Record.Strength); });
            break;
        case EDADaxtonCanonicalActionKind::EnterChoicePhase:
            bMatchesActionAuthority = Record.ActionId == Daxton.PhaseThreeActionId;
            break;
        case EDADaxtonCanonicalActionKind::CompleteChoiceObjective:
            for (int32 Index = 0; Index < Daxton.ObjectiveActionIds.Num(); ++Index)
                bMatchesActionAuthority |= Daxton.ObjectiveActionIds[Index] == Record.ActionId
                    && Daxton.CompletedObjectives[Index] == Record.Objective;
            break;
        case EDADaxtonCanonicalActionKind::ResolveLeader:
            bMatchesActionAuthority = Record.ActionId == Daxton.ResolutionActionId
                && Record.LeaderState == Daxton.LeaderState;
            break;
        }
        if (!bMatchesActionAuthority || !ApplyExpectedProof(Record, ExpectedProjection,
            ExpectedPhase, ExpectedArmor, ExpectedHeat, ExpectedCoolant, ProductionActions,
            CoverActions, ReinforcementActions, OutError))
        {
            if (OutError.IsEmpty()) OutError = TEXT("Daxton canonical proof does not match its durable action authority.");
            return false;
        }
        ProofActionIds.Add(Record.ActionId);
        PreviousProofTick = Record.WorldTick;
    }
    float ResolutionTrust = 0.f;
    float ResolutionRespect = 0.f;
    float ResolutionGrievance = 0.f;
    if (Daxton.Phase == EDADaxtonEncounterPhase::Resolved
        && (!ReplayResolutionRelationshipPrefix(Daxton, *Relationship,
                ResolutionTrust, ResolutionRespect, ResolutionGrievance, OutError)
            || !Near(ExpectedProjection.Trust, ResolutionTrust)
            || !Near(ExpectedProjection.Respect, ResolutionRespect)
            || !Near(ExpectedProjection.Grievance, ResolutionGrievance)))
    {
        if (OutError.IsEmpty())
            OutError = TEXT("Resolved Daxton terminal relationship proof must equal its canonical reason prefix.");
        return false;
    }
    if (ProductionActions != Daxton.GrandForgeProductionCycles
        || CoverActions != Daxton.HardenedCoverDeployments
        || ReinforcementActions != Daxton.ForgeGuardReinforcements
        || ExpectedPhase != Daxton.Phase || !Near(ExpectedArmor, Daxton.ArmorIntegrity)
        || !Near(ExpectedHeat, Daxton.Heat) || !Near(ExpectedCoolant, Daxton.CoolantStability)
        || (Daxton.Phase != EDADaxtonEncounterPhase::Resolved
            && !ProjectionEqual(ExpectedProjection, ActualProjection))
        || (Daxton.Phase == EDADaxtonEncounterPhase::Resolved
            && !ValidateResolvedEvolution(Daxton, ExpectedProjection, ActualProjection, OutError)))
    {
        OutError = TEXT("Daxton terminal campaign authority must equal deterministic canonical action replay.");
        return false;
    }
    const bool bStabilized = HasObjective(Daxton,
        EDADaxtonChoiceObjective::StabilizeProductionOfferUnion);
    if (Daxton.Phase != EDADaxtonEncounterPhase::PhaseOne && !bStabilized
        && !(Daxton.Phase == EDADaxtonEncounterPhase::Resolved
            ? ExpectedProjection.bOverdrive : Campaign.WorldState.Forgeweave.bOverdrive))
    {
        OutError = TEXT("Daxton Overdrive must remain on after Phase I until systemic production stabilization.");
        return false;
    }
    if (Daxton.InteractionRecords.ContainsByPredicate([](const FDADaxtonInteractionRecord& Record)
        { return Record.Interaction == EDADaxtonInteraction::WorkerShutdown; })
        && !Campaign.HistoryTags.Contains(TEXT("workers_protected")))
    {
        OutError = TEXT("Daxton worker shutdown must reconcile with canonical protected-worker history.");
        return false;
    }
    if (Daxton.Phase != EDADaxtonEncounterPhase::Resolved
        && !Daxton.InteractionRecords.IsEmpty())
    {
        const FDADaxtonInteractionRecord& Last = Daxton.InteractionRecords.Last();
        if (!FMath::IsNearlyEqual(Campaign.WorldState.Forgeweave.ActiveIndustrialThroughput,
                Last.ProductionAfter, 0.001f)
            || !FMath::IsNearlyEqual(Campaign.WorldState.Forgeweave.ResourceHunger,
                Last.ResourceHungerAfter, 0.001f))
        {
            OutError = TEXT("Active Daxton interaction closing supply/production must equal canonical Forgeweave authority.");
            return false;
        }
    }
    if (HasObjective(Daxton, EDADaxtonChoiceObjective::DefeatDaxton) && Daxton.ArmorIntegrity != 0.f)
    {
        OutError = TEXT("Defeat Daxton objective requires zero powered-armor integrity.");
        return false;
    }
    if (HasObjective(Daxton, EDADaxtonChoiceObjective::SaveGrandForge)
        && (Forge->StructuralIntegrity <= 0.f || Forge->ConstructionState == EDAConstructionState::Ruined))
    {
        OutError = TEXT("Save Grand Forge objective requires the canonical structure to survive.");
        return false;
    }
    if (HasObjective(Daxton, EDADaxtonChoiceObjective::EvacuateWorkers)
        && (!Campaign.HistoryTags.Contains(TEXT("workers_protected"))
            || !Daxton.InteractionRecords.ContainsByPredicate(
                [](const FDADaxtonInteractionRecord& Record)
                { return Record.Interaction == EDADaxtonInteraction::WorkerShutdown; })))
    {
        OutError = TEXT("Worker evacuation objective requires canonical protected-worker history.");
        return false;
    }
    if (HasObjective(Daxton, EDADaxtonChoiceObjective::StabilizeProductionOfferUnion)
        && Daxton.Phase != EDADaxtonEncounterPhase::Resolved
        && (Campaign.WorldState.Forgeweave.bOverdrive || Daxton.Heat > 60.f
            || Daxton.CoolantStability <= 0.f))
    {
        OutError = TEXT("Production stabilization requires Overdrive off, controlled Heat, and stable Coolant.");
        return false;
    }
    if (!Daxton.bLeaderResolved) return true;

    FDACampaignSnapshot ChoiceCampaign = Campaign;
    ChoiceCampaign.DaxtonState.Phase = EDADaxtonEncounterPhase::PhaseThree;
    ChoiceCampaign.DaxtonState.bLeaderResolved = false;
    ChoiceCampaign.DaxtonState.ResolutionActionId.Invalidate();
    ChoiceCampaign.DaxtonState.ResolvedWorldTick = 0;
    FDADiplomaticRelationship* ChoiceRelationship =
        ChoiceCampaign.WorldState.Diplomacy.FindRelationship(
            TEXT("relationship.synara.forgeweave"));
    if (ChoiceRelationship == nullptr)
    {
        OutError = TEXT("Resolved Daxton outcome proof lost its canonical relationship.");
        return false;
    }
    ChoiceRelationship->Trust = ResolutionTrust;
    ChoiceRelationship->Respect = ResolutionRespect;
    ChoiceRelationship->Grievance = ResolutionGrievance;
    if (!CanResolveLeaderState(Daxton.LeaderState, ChoiceCampaign, OutError)) return false;
    const FName ExpectedTag = GetLeaderHistoryTag(Daxton.LeaderState);
    int32 OutcomeTagCount = 0;
    for (uint8 Index = 0; Index <= static_cast<uint8>(EDADaxtonLeaderState::Dead); ++Index)
        OutcomeTagCount += Campaign.HistoryTags.Contains(
            GetLeaderHistoryTag(static_cast<EDADaxtonLeaderState>(Index))) ? 1 : 0;
    if (ExpectedTag.IsNone() || OutcomeTagCount != 1 || !Campaign.HistoryTags.Contains(ExpectedTag))
    {
        OutError = TEXT("Resolved Daxton Leader state requires exactly its canonical outcome history tag.");
        return false;
    }
    return true;
}
