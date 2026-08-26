#include "Damage/DAStructuralDamageComponent.h"

#include "Save/DACampaignSaveGame.h"

bool UDAStructuralDamageComponent::InitializeFromCampaign(
    IDACampaignAuthority& InCampaignAuthority,
    const FGuid InWorldAssetId)
{
    CampaignAuthority = nullptr;
    WorldAssetId = FGuid();

    const FDACampaignSnapshot& Campaign = InCampaignAuthority.GetPersistentCampaign();
    const FDAWorldAssetRecord* InAssetRecord = Campaign.FindWorldAssetRecord(InWorldAssetId);
    const FDAStructuralDamageRecord* InDamageRecord =
        Campaign.OperationConflict.FindStructuralDamageRecord(InWorldAssetId);

    if (!InWorldAssetId.IsValid()
        || InAssetRecord == nullptr
        || InDamageRecord == nullptr
        || InAssetRecord->CardDefinitionId.IsNone()
        || InDamageRecord->CardDefinitionId != InAssetRecord->CardDefinitionId
        || !FDAStructuralDamagePolicy::SupportsFullModularDestruction(InAssetRecord->CardDefinitionId)
        || !FMath::IsFinite(InAssetRecord->StructuralIntegrity)
        || InAssetRecord->StructuralIntegrity < 0.f
        || InAssetRecord->StructuralIntegrity > 100.f
        || InDamageRecord->Modules.IsEmpty())
    {
        return false;
    }

    TSet<FName> ModuleIds;
    for (const FDAStructureModuleHealthRecord& Module : InDamageRecord->Modules)
    {
        if (Module.ModuleId.IsNone()
            || ModuleIds.Contains(Module.ModuleId)
            || !FMath::IsFinite(Module.MaximumHealth)
            || !FMath::IsFinite(Module.CurrentHealth)
            || Module.MaximumHealth <= 0.f
            || Module.CurrentHealth < 0.f
            || Module.CurrentHealth > Module.MaximumHealth)
        {
            return false;
        }
        ModuleIds.Add(Module.ModuleId);
    }

    CampaignAuthority = &InCampaignAuthority;
    WorldAssetId = InWorldAssetId;
    return true;
}

bool UDAStructuralDamageComponent::ApplyModuleDamage(const FName ModuleId, const float DamageAmount)
{
    const FDAWorldAssetRecord* CurrentAsset = nullptr;
    const FDAStructuralDamageRecord* CurrentDamage = nullptr;
    if (!ResolveRecords(CurrentAsset, CurrentDamage)
        || CurrentAsset->ConstructionState == EDAConstructionState::Ruined
        || ModuleId.IsNone()
        || !FMath::IsFinite(DamageAmount)
        || DamageAmount <= 0.f)
    {
        return false;
    }

    const FDAStructureModuleHealthRecord* ExistingModule = CurrentDamage->Modules.FindByPredicate(
        [ModuleId](const FDAStructureModuleHealthRecord& Candidate)
        {
            return Candidate.ModuleId == ModuleId;
        });
    if (ExistingModule == nullptr || ExistingModule->CurrentHealth <= 0.f)
    {
        return false;
    }

    const FDACampaignSnapshot& Authority = CampaignAuthority->GetPersistentCampaign();
    const int64 ExpectedNarrativeRevision = Authority.NarrativeState.MutationRevision;
    const int64 ExpectedSignalRevision = Authority.LiveSignals.MutationRevision;
    const int64 ExpectedWorldTick = Authority.WorldState.CurrentWorldTick;
    FDACampaignSnapshot Candidate = Authority;
    FDAWorldAssetRecord* AssetRecord = Candidate.FindWorldAssetRecord(WorldAssetId);
    FDAStructuralDamageRecord* DamageRecord =
        Candidate.OperationConflict.FindStructuralDamageRecord(WorldAssetId);
    FDAStructureModuleHealthRecord* Module = DamageRecord == nullptr ? nullptr
        : DamageRecord->Modules.FindByPredicate([ModuleId](const FDAStructureModuleHealthRecord& Row)
            { return Row.ModuleId == ModuleId; });
    if (AssetRecord == nullptr || DamageRecord == nullptr || Module == nullptr)
    {
        return false;
    }
    Module->CurrentHealth = FMath::Max(0.f, Module->CurrentHealth - DamageAmount);
    Module->State = GetModuleState(Module->CurrentHealth, Module->MaximumHealth);
    RecalculateDerivedState(*AssetRecord, *DamageRecord);
    SynchronizeOwnedProjections(Candidate, *AssetRecord);
    FString Error;
    if (!Candidate.Validate(Error)
        || !CampaignAuthority->TryCommitPersistentCampaign(Candidate,
            ExpectedNarrativeRevision, ExpectedSignalRevision, ExpectedWorldTick))
    {
        return false;
    }
    OnDamageStateChanged.Broadcast(GetDamagePresentationState());
    return true;
}

bool UDAStructuralDamageComponent::ApplyStructuralDamage(const float DamageAmount)
{
    const FDAWorldAssetRecord* CurrentAsset = nullptr;
    const FDAStructuralDamageRecord* CurrentDamage = nullptr;
    if (!ResolveRecords(CurrentAsset, CurrentDamage)
        || CurrentAsset->ConstructionState == EDAConstructionState::Ruined
        || !FMath::IsFinite(DamageAmount)
        || DamageAmount <= 0.f)
    {
        return false;
    }

    const FDACampaignSnapshot& Authority = CampaignAuthority->GetPersistentCampaign();
    const int64 ExpectedNarrativeRevision = Authority.NarrativeState.MutationRevision;
    const int64 ExpectedSignalRevision = Authority.LiveSignals.MutationRevision;
    const int64 ExpectedWorldTick = Authority.WorldState.CurrentWorldTick;
    FDACampaignSnapshot Candidate = Authority;
    FDAWorldAssetRecord* AssetRecord = Candidate.FindWorldAssetRecord(WorldAssetId);
    FDAStructuralDamageRecord* DamageRecord =
        Candidate.OperationConflict.FindStructuralDamageRecord(WorldAssetId);
    if (AssetRecord == nullptr || DamageRecord == nullptr)
    {
        return false;
    }
    AssetRecord->StructuralIntegrity = FMath::Max(0.f, AssetRecord->StructuralIntegrity - DamageAmount);
    RecalculateDerivedState(*AssetRecord, *DamageRecord);
    SynchronizeOwnedProjections(Candidate, *AssetRecord);
    FString Error;
    if (!Candidate.Validate(Error)
        || !CampaignAuthority->TryCommitPersistentCampaign(Candidate,
            ExpectedNarrativeRevision, ExpectedSignalRevision, ExpectedWorldTick))
    {
        return false;
    }
    OnDamageStateChanged.Broadcast(GetDamagePresentationState());
    return true;
}

bool UDAStructuralDamageComponent::IsProductionEnabled() const
{
    const FDAWorldAssetRecord* AssetRecord = nullptr;
    const FDAStructuralDamageRecord* DamageRecord = nullptr;
    if (!ResolveRecords(AssetRecord, DamageRecord))
    {
        return false;
    }

    if (DamageRecord->bProductionDisabled
        || (AssetRecord->ConstructionState != EDAConstructionState::Operational
            && AssetRecord->ConstructionState != EDAConstructionState::Damaged))
    {
        return false;
    }
    return true;
}

bool UDAStructuralDamageComponent::IsRuined() const
{
    const FDAWorldAssetRecord* AssetRecord = nullptr;
    const FDAStructuralDamageRecord* DamageRecord = nullptr;
    return ResolveRecords(AssetRecord, DamageRecord)
        && AssetRecord->ConstructionState == EDAConstructionState::Ruined;
}

EDAConstructionState UDAStructuralDamageComponent::GetDamagePresentationState() const
{
    const FDAWorldAssetRecord* AssetRecord = nullptr;
    const FDAStructuralDamageRecord* DamageRecord = nullptr;
    return ResolveRecords(AssetRecord, DamageRecord)
        ? AssetRecord->ConstructionState : EDAConstructionState::Preview;
}

bool UDAStructuralDamageComponent::GetPresentationRecord(FDAWorldAssetRecord& OutRecord) const
{
    const FDAWorldAssetRecord* AssetRecord = nullptr;
    const FDAStructuralDamageRecord* DamageRecord = nullptr;
    if (!ResolveRecords(AssetRecord, DamageRecord))
    {
        return false;
    }
    OutRecord = *AssetRecord;
    return true;
}

const FDAStructureModuleHealthRecord* UDAStructuralDamageComponent::FindModule(const FName ModuleId) const
{
    const FDAWorldAssetRecord* AssetRecord = nullptr;
    const FDAStructuralDamageRecord* DamageRecord = nullptr;
    if (!ResolveRecords(AssetRecord, DamageRecord) || ModuleId.IsNone())
    {
        return nullptr;
    }

    return DamageRecord->Modules.FindByPredicate(
        [ModuleId](const FDAStructureModuleHealthRecord& Candidate)
        {
            return Candidate.ModuleId == ModuleId;
        });
}

EDAStructureDamageState UDAStructuralDamageComponent::GetModuleState(
    const float CurrentHealth,
    const float MaximumHealth)
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

bool UDAStructuralDamageComponent::ResolveRecords(
    const FDAWorldAssetRecord*& OutAssetRecord,
    const FDAStructuralDamageRecord*& OutDamageRecord) const
{
    OutAssetRecord = nullptr;
    OutDamageRecord = nullptr;
    if (CampaignAuthority == nullptr || !WorldAssetId.IsValid())
    {
        return false;
    }

    const FDACampaignSnapshot& Campaign = CampaignAuthority->GetPersistentCampaign();
    OutAssetRecord = Campaign.FindWorldAssetRecord(WorldAssetId);
    OutDamageRecord = Campaign.OperationConflict.FindStructuralDamageRecord(WorldAssetId);
    return OutAssetRecord != nullptr
        && OutDamageRecord != nullptr
        && OutDamageRecord->WorldAssetId == OutAssetRecord->WorldAssetId
        && OutDamageRecord->CardDefinitionId == OutAssetRecord->CardDefinitionId
        && FDAStructuralDamagePolicy::SupportsFullModularDestruction(OutAssetRecord->CardDefinitionId);
}

void UDAStructuralDamageComponent::SynchronizeOwnedProjections(
    FDACampaignSnapshot& Campaign, const FDAWorldAssetRecord& AssetRecord)
{
    if (AssetRecord.CardInstanceId.IsValid())
    {
        if (FCardInstance* Card = Campaign.CollectionState.FindInstance(AssetRecord.CardInstanceId))
        {
            Card->WorldAssetId = AssetRecord.WorldAssetId;
            Card->RecoveryState = AssetRecord.ConstructionState == EDAConstructionState::Ruined
                ? EDARecoveryState::Ruined : EDARecoveryState::Deployed;
        }
    }
    if (FDAFacilityContext* Facility = Campaign.CitySimulationState.Facilities.FindByPredicate(
        [&AssetRecord](const FDAFacilityContext& Row)
        { return Row.AssetRecord.WorldAssetId == AssetRecord.WorldAssetId; }))
    {
        Facility->AssetRecord = AssetRecord;
    }
}

void UDAStructuralDamageComponent::RecalculateDerivedState(
    FDAWorldAssetRecord& AssetRecord,
    FDAStructuralDamageRecord& DamageRecord)
{

    if (AssetRecord.StructuralIntegrity <= 0.f)
    {
        AssetRecord.StructuralIntegrity = 0.f;
        AssetRecord.ConstructionState = EDAConstructionState::Ruined;
        DamageRecord.bProductionDisabled = true;
        for (FDAStructureModuleHealthRecord& Module : DamageRecord.Modules)
        {
            Module.State = EDAStructureDamageState::Ruined;
        }
        return;
    }

    for (FDAStructureModuleHealthRecord& Module : DamageRecord.Modules)
    {
        Module.State = GetModuleState(Module.CurrentHealth, Module.MaximumHealth);
    }

    DamageRecord.bProductionDisabled = DamageRecord.Modules.ContainsByPredicate(
        [](const FDAStructureModuleHealthRecord& Module)
        {
            return Module.bDisablesProduction && Module.CurrentHealth <= 0.f;
        });
    if (AssetRecord.StructuralIntegrity <= 25.f)
    {
        AssetRecord.ConstructionState = EDAConstructionState::Disabled;
    }
    else if (AssetRecord.StructuralIntegrity <= 50.f)
    {
        AssetRecord.ConstructionState = EDAConstructionState::Damaged;
    }
    else
    {
        AssetRecord.ConstructionState = EDAConstructionState::Operational;
    }
}
