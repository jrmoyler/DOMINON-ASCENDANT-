#include "Conflict/DAConflictRecords.h"

#include "World/DAWorldAssetRecord.h"

namespace
{
    const TArray<FName> FullModularDestructionDefinitions = {
        TEXT("synara.synthetic_fabrication_node"),
        TEXT("synara.swarm_foundry"),
        TEXT("forgeweave.infinite_foundry"),
        TEXT("forgeweave.replication_forge"),
        TEXT("forgeweave.smog_reclaimer"),
        TEXT("forgeweave.freight_furnace"),
        TEXT("forgeweave.grand_forge"),
        TEXT("fusion.autonomous_factory")
    };

    const FDAWorldAssetRecord* FindWorldAsset(
        const TArray<FDAWorldAssetRecord>& WorldAssets,
        const FGuid WorldAssetId)
    {
        return WorldAssets.FindByPredicate(
            [WorldAssetId](const FDAWorldAssetRecord& Record)
            {
                return Record.WorldAssetId == WorldAssetId;
            });
    }

    EDAStructureDamageState GetModuleState(const float CurrentHealth, const float MaximumHealth)
    {
        const float HealthPercent = MaximumHealth > 0.f ? CurrentHealth * 100.f / MaximumHealth : 0.f;
        if (HealthPercent > 50.f)
        {
            return EDAStructureDamageState::Operational;
        }
        if (HealthPercent > 25.f)
        {
            return EDAStructureDamageState::Damaged;
        }
        return EDAStructureDamageState::Disabled;
    }

    EDAConstructionState GetConstructionStateForIntegrity(const float StructuralIntegrity)
    {
        if (StructuralIntegrity <= 0.f)
        {
            return EDAConstructionState::Ruined;
        }
        if (StructuralIntegrity <= 25.f)
        {
            return EDAConstructionState::Disabled;
        }
        if (StructuralIntegrity <= 50.f)
        {
            return EDAConstructionState::Damaged;
        }
        return EDAConstructionState::Operational;
    }

    bool IsCaptureRoleEligible(const EDACaptureAgentRole Role)
    {
        return Role == EDACaptureAgentRole::Engineer
            || Role == EDACaptureAgentRole::Founder
            || Role == EDACaptureAgentRole::CaptureCapableSquad;
    }

    bool HasAllowedGiftRelationship(const FDACaptureRecord& Record, const FName CivilizationId)
    {
        const FDACaptureGiftRecipientRecord* Recipient = Record.AllowedGiftRecipients.FindByPredicate(
            [CivilizationId](const FDACaptureGiftRecipientRecord& Candidate)
            {
                return Candidate.CivilizationId == CivilizationId;
            });
        return Recipient != nullptr
            && (Recipient->Relationship == EDAGiftRecipientRelationship::Allied
                || Recipient->Relationship == EDAGiftRecipientRelationship::LocalAuthority);
    }

    bool ValidateCaptureOutcomeInvariant(
        const FDACaptureRecord& Record,
        const FDAWorldAssetRecord& Asset)
    {
        if (!Record.bOutcomeResolved)
        {
            return !Record.bOperationalUseRemoved
                && !Record.bIntegrationRequired
                && !Record.bConversionOperationSanctioned
                && (Record.OriginalOwnerCivilizationId.IsNone()
                    || Asset.OwnerCivilizationId == Record.OriginalOwnerCivilizationId);
        }

        if (Record.OriginalOwnerCivilizationId.IsNone()
            || Record.CapturingCivilizationId.IsNone())
        {
            return false;
        }

        switch (Record.Outcome)
        {
        case EDACaptureOutcome::Preserve:
            return Asset.OwnerCivilizationId == Record.CapturingCivilizationId
                && !Record.bOperationalUseRemoved
                && Record.bIntegrationRequired
                && !Record.bConversionOperationSanctioned
                && Record.History.Contains(TEXT("capture.preserved"));

        case EDACaptureOutcome::Convert:
            return Asset.OwnerCivilizationId == Record.CapturingCivilizationId
                && !Record.bOperationalUseRemoved
                && !Record.bIntegrationRequired
                && Record.bConversionOperationSanctioned
                && Record.History.Contains(TEXT("capture.conversion_sanctioned"));

        case EDACaptureOutcome::Study:
            return Asset.OwnerCivilizationId == Record.CapturingCivilizationId
                && Record.bOperationalUseRemoved
                && !Record.bIntegrationRequired
                && !Record.bConversionOperationSanctioned
                && Record.History.Contains(TEXT("capture.studied"));

        case EDACaptureOutcome::Salvage:
            return Asset.OwnerCivilizationId == Record.CapturingCivilizationId
                && Asset.StructuralIntegrity == 0.f
                && Asset.ConstructionState == EDAConstructionState::Ruined
                && Record.bOperationalUseRemoved
                && !Record.bIntegrationRequired
                && !Record.bConversionOperationSanctioned
                && Record.History.Contains(TEXT("capture.salvaged"));

        case EDACaptureOutcome::Gift:
            return Asset.OwnerCivilizationId != Record.OriginalOwnerCivilizationId
                && Asset.OwnerCivilizationId != Record.CapturingCivilizationId
                && HasAllowedGiftRelationship(Record, Asset.OwnerCivilizationId)
                && !Record.bOperationalUseRemoved
                && !Record.bIntegrationRequired
                && !Record.bConversionOperationSanctioned
                && Record.History.Contains(TEXT("capture.gifted"));

        default:
            return false;
        }
    }
}

bool FDAStructuralDamagePolicy::SupportsFullModularDestruction(const FName CardDefinitionId)
{
    return FullModularDestructionDefinitions.Contains(CardDefinitionId);
}

const TArray<FName>& FDAStructuralDamagePolicy::GetFullModularDestructionDefinitions()
{
    return FullModularDestructionDefinitions;
}

bool FDACaptureRecord::IsOutcomeInvariantValid(const FDAWorldAssetRecord& Asset) const
{
    return ValidateCaptureOutcomeInvariant(*this, Asset);
}

bool FDAConflictResourceState::IsFinite() const
{
    return FMath::IsFinite(Capital)
        && FMath::IsFinite(Insight)
        && FMath::IsFinite(Influence)
        && FMath::IsFinite(PostConflictLoyalty)
        && FMath::IsFinite(FutureSurrenderLikelihood);
}

FDAStructuralDamageRecord* FDAOperationConflictSnapshot::FindStructuralDamageRecord(const FGuid WorldAssetId)
{
    return StructuralDamageRecords.FindByPredicate(
        [WorldAssetId](const FDAStructuralDamageRecord& Record)
        {
            return Record.WorldAssetId == WorldAssetId;
        });
}

const FDAStructuralDamageRecord* FDAOperationConflictSnapshot::FindStructuralDamageRecord(const FGuid WorldAssetId) const
{
    return StructuralDamageRecords.FindByPredicate(
        [WorldAssetId](const FDAStructuralDamageRecord& Record)
        {
            return Record.WorldAssetId == WorldAssetId;
        });
}

FDACaptureRecord* FDAOperationConflictSnapshot::FindCaptureRecord(const FGuid WorldAssetId)
{
    return CaptureRecords.FindByPredicate(
        [WorldAssetId](const FDACaptureRecord& Record)
        {
            return Record.WorldAssetId == WorldAssetId;
        });
}

const FDACaptureRecord* FDAOperationConflictSnapshot::FindCaptureRecord(const FGuid WorldAssetId) const
{
    return CaptureRecords.FindByPredicate(
        [WorldAssetId](const FDACaptureRecord& Record)
        {
            return Record.WorldAssetId == WorldAssetId;
        });
}

FDASurrenderRecord* FDAOperationConflictSnapshot::FindSurrenderRecord(const FGuid SquadId)
{
    return SurrenderRecords.FindByPredicate(
        [SquadId](const FDASurrenderRecord& Record)
        {
            return Record.SquadId == SquadId;
        });
}

const FDASurrenderRecord* FDAOperationConflictSnapshot::FindSurrenderRecord(const FGuid SquadId) const
{
    return SurrenderRecords.FindByPredicate(
        [SquadId](const FDASurrenderRecord& Record)
        {
            return Record.SquadId == SquadId;
        });
}

bool FDAOperationConflictSnapshot::Validate(
    const TArray<FDAWorldAssetRecord>& WorldAssets,
    FString& OutError) const
{
    if (!Resources.IsFinite()
        || Resources.Capital < 0.f
        || Resources.Insight < 0.f
        || Resources.Influence < 0.f
        || Resources.Materials < 0
        || Resources.PostConflictLoyalty < 0.f
        || Resources.FutureSurrenderLikelihood < 0.f)
    {
        OutError = TEXT("Conflict resources must be finite and non-negative.");
        return false;
    }

    TSet<FGuid> StructuralIds;
    for (const FDAStructuralDamageRecord& Record : StructuralDamageRecords)
    {
        const FDAWorldAssetRecord* Asset = FindWorldAsset(WorldAssets, Record.WorldAssetId);
        if (!Record.WorldAssetId.IsValid()
            || StructuralIds.Contains(Record.WorldAssetId)
            || Asset == nullptr
            || Record.CardDefinitionId.IsNone()
            || Record.CardDefinitionId != Asset->CardDefinitionId
            || !FDAStructuralDamagePolicy::SupportsFullModularDestruction(Record.CardDefinitionId)
            || Asset->ConstructionState != GetConstructionStateForIntegrity(Asset->StructuralIntegrity)
            || Record.Modules.IsEmpty())
        {
            OutError = TEXT("Structural damage records require a unique valid WorldAssetId and matching definition.");
            return false;
        }
        StructuralIds.Add(Record.WorldAssetId);

        TSet<FName> ModuleIds;
        bool bExpectedProductionDisabled = Asset->StructuralIntegrity <= 0.f;
        for (const FDAStructureModuleHealthRecord& Module : Record.Modules)
        {
            const EDAStructureDamageState ExpectedState = Asset->StructuralIntegrity <= 0.f
                ? EDAStructureDamageState::Ruined
                : GetModuleState(Module.CurrentHealth, Module.MaximumHealth);
            if (Module.ModuleId.IsNone()
                || ModuleIds.Contains(Module.ModuleId)
                || !FMath::IsFinite(Module.MaximumHealth)
                || !FMath::IsFinite(Module.CurrentHealth)
                || Module.MaximumHealth <= 0.f
                || Module.CurrentHealth < 0.f
                || Module.CurrentHealth > Module.MaximumHealth
                || Module.State != ExpectedState)
            {
                OutError = TEXT("Structural module health is invalid or inconsistent with its derived state.");
                return false;
            }
            ModuleIds.Add(Module.ModuleId);
            bExpectedProductionDisabled |= Module.bDisablesProduction && Module.CurrentHealth <= 0.f;
        }
        if (Record.bProductionDisabled != bExpectedProductionDisabled)
        {
            OutError = TEXT("Structural production-disabled state does not match module/integrity authority.");
            return false;
        }
    }

    TSet<FGuid> CaptureIds;
    for (const FDACaptureRecord& Record : CaptureRecords)
    {
        const FDAWorldAssetRecord* Asset = FindWorldAsset(WorldAssets, Record.WorldAssetId);
        if (!Record.WorldAssetId.IsValid() || CaptureIds.Contains(Record.WorldAssetId) || Asset == nullptr)
        {
            OutError = TEXT("Capture records require a unique valid WorldAssetId.");
            return false;
        }
        CaptureIds.Add(Record.WorldAssetId);

        if (!FMath::IsFinite(Record.CaptureProgressSeconds)
            || !FMath::IsFinite(Record.RequiredCaptureTimeSeconds)
            || Record.CaptureProgressSeconds < 0.f
            || Record.RequiredCaptureTimeSeconds < 0.f
            || !FMath::IsFinite(Record.StudyInsightReward)
            || Record.StudyInsightReward < 0.f
            || !FMath::IsFinite(Record.SalvageCapitalReward)
            || Record.SalvageCapitalReward < 0.f
            || Record.SalvageMaterialReward < 0
            || !FMath::IsFinite(Record.GiftInfluenceReward)
            || Record.GiftInfluenceReward < 0.f
            || !FMath::IsFinite(Record.GiftLoyaltyReward)
            || Record.GiftLoyaltyReward < 0.f
            || (Record.bCaptureInProgress
                && (!Record.ActiveInteractionId.IsValid()
                    || !Record.ActiveCaptureActorId.IsValid()
                    || !IsCaptureRoleEligible(Record.ActiveCaptureRole)
                    || Record.OriginalOwnerCivilizationId.IsNone()
                    || Record.CapturingCivilizationId.IsNone()
                    || Record.RequiredCaptureTimeSeconds <= 0.f
                    || Record.CaptureProgressSeconds >= Record.RequiredCaptureTimeSeconds
                    || Record.bCaptureCompleted))
            || (!Record.bCaptureInProgress
                && (Record.ActiveInteractionId.IsValid()
                    || Record.ActiveCaptureActorId.IsValid()
                    || Record.ActiveCaptureRole != EDACaptureAgentRole::Other))
            || (Record.bCaptureCompleted
                && (Record.OriginalOwnerCivilizationId.IsNone()
                    || Record.CapturingCivilizationId.IsNone()
                    || Record.RequiredCaptureTimeSeconds <= 0.f
                    || Record.CaptureProgressSeconds != Record.RequiredCaptureTimeSeconds))
            || (Record.bOutcomeResolved && (!Record.bCaptureCompleted || Record.Outcome == EDACaptureOutcome::None))
            || (!Record.bOutcomeResolved && Record.Outcome != EDACaptureOutcome::None)
            || Record.bRewardsGranted != Record.bOutcomeResolved
            || !Record.IsOutcomeInvariantValid(*Asset))
        {
            OutError = TEXT("Capture record transaction state is invalid.");
            return false;
        }

        if (Record.bOutcomeResolved
            && Record.Outcome == EDACaptureOutcome::Salvage
            && !StructuralIds.Contains(Record.WorldAssetId))
        {
            OutError = TEXT("Resolved Salvage requires its matching structural ruin record.");
            return false;
        }

        TSet<FName> RecipientIds;
        for (const FDACaptureGiftRecipientRecord& Recipient : Record.AllowedGiftRecipients)
        {
            if (Recipient.CivilizationId.IsNone()
                || RecipientIds.Contains(Recipient.CivilizationId)
                || Recipient.Relationship == EDAGiftRecipientRelationship::Unknown)
            {
                OutError = TEXT("Capture Gift recipients require unique ids and known relationships.");
                return false;
            }
            RecipientIds.Add(Recipient.CivilizationId);
        }
    }

    TSet<FGuid> SurrenderIds;
    for (const FDASurrenderRecord& Record : SurrenderRecords)
    {
        if (!Record.SquadId.IsValid() || SurrenderIds.Contains(Record.SquadId))
        {
            OutError = TEXT("Surrender records require a unique valid SquadId.");
            return false;
        }
        SurrenderIds.Add(Record.SquadId);
        const bool bRecordHasAcceptanceAudit = Record.History.Contains(TEXT("forge_guard_surrender_accepted"));
        if (Record.bAccepted != bRecordHasAcceptanceAudit)
        {
            OutError = TEXT("Surrender acceptance flags require matching per-record audit.");
            return false;
        }
    }

    return true;
}
