#include "Capture/DACaptureComponent.h"

#include "Save/DACampaignSaveGame.h"

namespace
{
    const FName ForgeGuardDefinitionId(TEXT("forgeweave.forge_guard"));
    const FName SurrenderAcceptedHistoryTag(TEXT("forge_guard_surrender_accepted"));

    bool CommitCandidate(IDACampaignAuthority* Authority, const FDACampaignSnapshot& Candidate,
        const int64 ExpectedNarrativeRevision, const int64 ExpectedSignalRevision,
        const int64 ExpectedWorldTick)
    {
        FString Error;
        return Authority != nullptr && Candidate.Validate(Error)
            && Authority->TryCommitPersistentCampaign(Candidate, ExpectedNarrativeRevision,
                ExpectedSignalRevision, ExpectedWorldTick);
    }
}

bool UDACaptureComponent::InitializeFromCampaign(
    IDACampaignAuthority& InCampaignAuthority,
    const FGuid InWorldAssetId)
{
    CampaignAuthority = nullptr;
    WorldAssetId = FGuid();

    const FDACampaignSnapshot& Authority = InCampaignAuthority.GetPersistentCampaign();
    const FDAWorldAssetRecord* InAssetRecord = Authority.FindWorldAssetRecord(InWorldAssetId);
    const FDACaptureRecord* InCaptureRecord =
        Authority.OperationConflict.FindCaptureRecord(InWorldAssetId);
    if (InAssetRecord == nullptr || InCaptureRecord == nullptr)
    {
        return false;
    }

    const bool bActiveIdentityValid = InCaptureRecord->ActiveInteractionId.IsValid()
        && InCaptureRecord->ActiveCaptureActorId.IsValid()
        && IsCaptureRoleEligible(InCaptureRecord->ActiveCaptureRole);
    const bool bActiveIdentityClear = !InCaptureRecord->ActiveInteractionId.IsValid()
        && !InCaptureRecord->ActiveCaptureActorId.IsValid()
        && InCaptureRecord->ActiveCaptureRole == EDACaptureAgentRole::Other;
    if (!InWorldAssetId.IsValid()
        || InAssetRecord->WorldAssetId != InWorldAssetId
        || InCaptureRecord->WorldAssetId != InWorldAssetId
        || InAssetRecord->CardDefinitionId.IsNone()
        || InAssetRecord->OwnerCivilizationId.IsNone()
        || !FMath::IsFinite(InAssetRecord->StructuralIntegrity)
        || InAssetRecord->StructuralIntegrity < 0.f
        || InAssetRecord->StructuralIntegrity > 100.f
        || !FMath::IsFinite(InCaptureRecord->CaptureProgressSeconds)
        || !FMath::IsFinite(InCaptureRecord->RequiredCaptureTimeSeconds)
        || InCaptureRecord->CaptureProgressSeconds < 0.f
        || InCaptureRecord->RequiredCaptureTimeSeconds < 0.f
        || (InCaptureRecord->bCaptureInProgress
            && (!bActiveIdentityValid
                || InCaptureRecord->CapturingCivilizationId.IsNone()
                || InCaptureRecord->RequiredCaptureTimeSeconds <= 0.f
                || InCaptureRecord->CaptureProgressSeconds >= InCaptureRecord->RequiredCaptureTimeSeconds
                || InCaptureRecord->bCaptureCompleted))
        || (!InCaptureRecord->bCaptureInProgress && !bActiveIdentityClear)
        || (InCaptureRecord->bCaptureCompleted
            && (InCaptureRecord->CapturingCivilizationId.IsNone()
                || InCaptureRecord->RequiredCaptureTimeSeconds <= 0.f
                || InCaptureRecord->CaptureProgressSeconds != InCaptureRecord->RequiredCaptureTimeSeconds))
        || (InCaptureRecord->bOutcomeResolved && !InCaptureRecord->bCaptureCompleted)
        || (InCaptureRecord->bOutcomeResolved && InCaptureRecord->Outcome == EDACaptureOutcome::None)
        || (!InCaptureRecord->bOutcomeResolved && InCaptureRecord->Outcome != EDACaptureOutcome::None)
        || InCaptureRecord->bRewardsGranted != InCaptureRecord->bOutcomeResolved
        || !InCaptureRecord->IsOutcomeInvariantValid(*InAssetRecord)
        || !AreCaptureRewardsValid(*InCaptureRecord))
    {
        return false;
    }

    TSet<FName> RecipientIds;
    for (const FDACaptureGiftRecipientRecord& Recipient : InCaptureRecord->AllowedGiftRecipients)
    {
        if (Recipient.CivilizationId.IsNone()
            || RecipientIds.Contains(Recipient.CivilizationId)
            || Recipient.Relationship == EDAGiftRecipientRelationship::Unknown)
        {
            return false;
        }
        RecipientIds.Add(Recipient.CivilizationId);
    }

    if (InCaptureRecord->OriginalOwnerCivilizationId.IsNone())
    {
        const int64 ExpectedNarrativeRevision = Authority.NarrativeState.MutationRevision;
        const int64 ExpectedSignalRevision = Authority.LiveSignals.MutationRevision;
        const int64 ExpectedWorldTick = Authority.WorldState.CurrentWorldTick;
        FDACampaignSnapshot Candidate = Authority;
        FDACaptureRecord* CandidateCapture =
            Candidate.OperationConflict.FindCaptureRecord(InWorldAssetId);
        if (CandidateCapture == nullptr)
        {
            return false;
        }
        CandidateCapture->OriginalOwnerCivilizationId = InAssetRecord->OwnerCivilizationId;
        if (!CommitCandidate(&InCampaignAuthority, Candidate, ExpectedNarrativeRevision,
            ExpectedSignalRevision, ExpectedWorldTick))
        {
            return false;
        }
    }

    CampaignAuthority = &InCampaignAuthority;
    WorldAssetId = InWorldAssetId;
    return true;
}

bool UDACaptureComponent::CanCapture(const FDACaptureInteractionContext& Context) const
{
    const FDAWorldAssetRecord* AssetRecord = nullptr;
    const FDACaptureRecord* CaptureRecord = nullptr;
    return ResolveAuthority(AssetRecord, CaptureRecord)
        && !CaptureRecord->bCaptureInProgress
        && !CaptureRecord->bCaptureCompleted
        && !CaptureRecord->bOutcomeResolved
        && AreFacilityCapturePreconditionsMet(*AssetRecord, *CaptureRecord)
        && IsInteractionEligible(Context);
}

bool UDACaptureComponent::BeginCapture(
    const FDACaptureInteractionContext& Context,
    const FName CapturingCivilizationId)
{
    if (CampaignAuthority == nullptr || CapturingCivilizationId.IsNone()
        || !IsInteractionEligible(Context))
    {
        return false;
    }

    const FDACampaignSnapshot& Authority = CampaignAuthority->GetPersistentCampaign();
    const int64 ExpectedNarrativeRevision = Authority.NarrativeState.MutationRevision;
    const int64 ExpectedSignalRevision = Authority.LiveSignals.MutationRevision;
    const int64 ExpectedWorldTick = Authority.WorldState.CurrentWorldTick;
    FDACampaignSnapshot Candidate = Authority;
    FDAWorldAssetRecord* AssetRecord = Candidate.FindWorldAssetRecord(WorldAssetId);
    FDACaptureRecord* CaptureRecord =
        Candidate.OperationConflict.FindCaptureRecord(WorldAssetId);
    if (AssetRecord == nullptr || CaptureRecord == nullptr
        || CaptureRecord->bCaptureInProgress || CaptureRecord->bCaptureCompleted
        || CaptureRecord->bOutcomeResolved
        || !AreFacilityCapturePreconditionsMet(*AssetRecord, *CaptureRecord)
        || CapturingCivilizationId == AssetRecord->OwnerCivilizationId)
    {
        return false;
    }

    CaptureRecord->CapturingCivilizationId = CapturingCivilizationId;
    CaptureRecord->ActiveInteractionId = Context.InteractionId;
    CaptureRecord->ActiveCaptureActorId = Context.CaptureActorId;
    CaptureRecord->ActiveCaptureRole = Context.AgentRole;
    CaptureRecord->CaptureProgressSeconds = 0.f;
    CaptureRecord->RequiredCaptureTimeSeconds = Context.AgentRole == EDACaptureAgentRole::Engineer
        ? BaseCaptureTimeSeconds * EngineerCaptureTimeMultiplier
        : BaseCaptureTimeSeconds;
    CaptureRecord->bCaptureInProgress = true;
    if (!CommitCandidate(CampaignAuthority, Candidate, ExpectedNarrativeRevision,
        ExpectedSignalRevision, ExpectedWorldTick))
    {
        return false;
    }
    OnCaptureStateChanged.Broadcast(GetPresentationSnapshot());
    return true;
}

bool UDACaptureComponent::AdvanceCapture(
    const float DeltaSeconds,
    const FDACaptureInteractionContext& Context)
{
    if (CampaignAuthority == nullptr)
    {
        return false;
    }
    const FDACampaignSnapshot& Authority = CampaignAuthority->GetPersistentCampaign();
    const int64 ExpectedNarrativeRevision = Authority.NarrativeState.MutationRevision;
    const int64 ExpectedSignalRevision = Authority.LiveSignals.MutationRevision;
    const int64 ExpectedWorldTick = Authority.WorldState.CurrentWorldTick;
    FDACampaignSnapshot Candidate = Authority;
    FDAWorldAssetRecord* AssetRecord = Candidate.FindWorldAssetRecord(WorldAssetId);
    FDACaptureRecord* CaptureRecord =
        Candidate.OperationConflict.FindCaptureRecord(WorldAssetId);
    if (AssetRecord == nullptr || CaptureRecord == nullptr || !CaptureRecord->bCaptureInProgress)
    {
        return false;
    }

    const bool bInteractionMatches = Context.InteractionId == CaptureRecord->ActiveInteractionId
        && Context.CaptureActorId == CaptureRecord->ActiveCaptureActorId
        && Context.AgentRole == CaptureRecord->ActiveCaptureRole;
    if (!bInteractionMatches
        || !IsInteractionEligible(Context)
        || !AreFacilityCapturePreconditionsMet(*AssetRecord, *CaptureRecord))
    {
        ResetActiveCapture(*CaptureRecord);
        if (CommitCandidate(CampaignAuthority, Candidate, ExpectedNarrativeRevision,
            ExpectedSignalRevision, ExpectedWorldTick))
        {
            OnCaptureStateChanged.Broadcast(GetPresentationSnapshot());
        }
        return false;
    }

    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f)
    {
        return false;
    }

    CaptureRecord->CaptureProgressSeconds = FMath::Min(
        CaptureRecord->CaptureProgressSeconds + DeltaSeconds,
        CaptureRecord->RequiredCaptureTimeSeconds);
    const bool bCompleted = CaptureRecord->CaptureProgressSeconds + KINDA_SMALL_NUMBER
        >= CaptureRecord->RequiredCaptureTimeSeconds;
    if (bCompleted)
    {
        CaptureRecord->CaptureProgressSeconds = CaptureRecord->RequiredCaptureTimeSeconds;
        CaptureRecord->bCaptureInProgress = false;
        CaptureRecord->bCaptureCompleted = true;
        CaptureRecord->ActiveInteractionId = FGuid();
        CaptureRecord->ActiveCaptureActorId = FGuid();
        CaptureRecord->ActiveCaptureRole = EDACaptureAgentRole::Other;
    }
    if (!CommitCandidate(CampaignAuthority, Candidate, ExpectedNarrativeRevision,
        ExpectedSignalRevision, ExpectedWorldTick))
    {
        return false;
    }
    OnCaptureStateChanged.Broadcast(GetPresentationSnapshot());
    return bCompleted;
}

bool UDACaptureComponent::ResolveOutcome(
    const EDACaptureOutcome Outcome,
    const FName GiftRecipientCivilizationId)
{
    if (CampaignAuthority == nullptr)
    {
        return false;
    }
    const FDACampaignSnapshot& Authority = CampaignAuthority->GetPersistentCampaign();
    const int64 ExpectedNarrativeRevision = Authority.NarrativeState.MutationRevision;
    const int64 ExpectedSignalRevision = Authority.LiveSignals.MutationRevision;
    const int64 ExpectedWorldTick = Authority.WorldState.CurrentWorldTick;
    FDACampaignSnapshot Candidate = Authority;
    FDAWorldAssetRecord* AssetRecord = Candidate.FindWorldAssetRecord(WorldAssetId);
    FDACaptureRecord* CaptureRecord =
        Candidate.OperationConflict.FindCaptureRecord(WorldAssetId);
    if (AssetRecord == nullptr || CaptureRecord == nullptr)
    {
        return false;
    }
    FDAOperationConflictSnapshot& Conflict = Candidate.OperationConflict;
    const FDAConflictResourceState& Resources = Conflict.Resources;
    if (!CaptureRecord->bCaptureCompleted
        || CaptureRecord->bOutcomeResolved
        || CaptureRecord->bRewardsGranted
        || CaptureRecord->CapturingCivilizationId.IsNone()
        || Outcome == EDACaptureOutcome::None
        || !Resources.IsFinite()
        || Resources.Capital < 0.f
        || Resources.Insight < 0.f
        || Resources.Influence < 0.f
        || Resources.Materials < 0
        || Resources.PostConflictLoyalty < 0.f
        || Resources.FutureSurrenderLikelihood < 0.f
        || !AreCaptureRewardsValid(*CaptureRecord))
    {
        return false;
    }
    if (Outcome == EDACaptureOutcome::Gift
        && !IsGiftRecipientAllowed(*CaptureRecord, GiftRecipientCivilizationId))
    {
        return false;
    }

    FDAWorldAssetRecord NewAsset = *AssetRecord;
    FDACaptureRecord NewCapture = *CaptureRecord;
    FDAConflictResourceState NewResources = Resources;
    FDAStructuralDamageRecord* DamageRecord = nullptr;
    FDAStructuralDamageRecord NewDamage;
    FName NewOwner = CaptureRecord->CapturingCivilizationId;
    bool bRemoveOperationalUse = false;
    bool bRequireIntegration = false;
    bool bSanctionConversion = false;
    FName HistoryTag;

    switch (Outcome)
    {
    case EDACaptureOutcome::Preserve:
        bRequireIntegration = true;
        HistoryTag = TEXT("capture.preserved");
        break;
    case EDACaptureOutcome::Convert:
        bSanctionConversion = true;
        HistoryTag = TEXT("capture.conversion_sanctioned");
        break;
    case EDACaptureOutcome::Study:
        bRemoveOperationalUse = true;
        NewResources.Insight += CaptureRecord->StudyInsightReward;
        HistoryTag = TEXT("capture.studied");
        break;
    case EDACaptureOutcome::Salvage:
        DamageRecord = Conflict.FindStructuralDamageRecord(WorldAssetId);
        if (DamageRecord == nullptr
            || DamageRecord->WorldAssetId != WorldAssetId
            || DamageRecord->CardDefinitionId != AssetRecord->CardDefinitionId
            || !FDAStructuralDamagePolicy::SupportsFullModularDestruction(DamageRecord->CardDefinitionId)
            || DamageRecord->Modules.IsEmpty()
            || CaptureRecord->SalvageMaterialReward > MAX_int32 - NewResources.Materials)
        {
            return false;
        }
        for (const FDAStructureModuleHealthRecord& Module : DamageRecord->Modules)
        {
            if (Module.ModuleId.IsNone()
                || !FMath::IsFinite(Module.MaximumHealth)
                || !FMath::IsFinite(Module.CurrentHealth)
                || Module.MaximumHealth <= 0.f
                || Module.CurrentHealth < 0.f
                || Module.CurrentHealth > Module.MaximumHealth)
            {
                return false;
            }
        }
        bRemoveOperationalUse = true;
        NewAsset.ConstructionState = EDAConstructionState::Ruined;
        NewAsset.StructuralIntegrity = 0.f;
        NewDamage = *DamageRecord;
        NewDamage.bProductionDisabled = true;
        for (FDAStructureModuleHealthRecord& Module : NewDamage.Modules)
        {
            Module.State = EDAStructureDamageState::Ruined;
        }
        NewResources.Capital += CaptureRecord->SalvageCapitalReward;
        NewResources.Materials += CaptureRecord->SalvageMaterialReward;
        HistoryTag = TEXT("capture.salvaged");
        break;
    case EDACaptureOutcome::Gift:
        NewOwner = GiftRecipientCivilizationId;
        NewResources.Influence += CaptureRecord->GiftInfluenceReward;
        NewResources.PostConflictLoyalty += CaptureRecord->GiftLoyaltyReward;
        HistoryTag = TEXT("capture.gifted");
        break;
    default:
        return false;
    }

    NewAsset.OwnerCivilizationId = NewOwner;
    NewCapture.Outcome = Outcome;
    NewCapture.bOperationalUseRemoved = bRemoveOperationalUse;
    NewCapture.bIntegrationRequired = bRequireIntegration;
    NewCapture.bConversionOperationSanctioned = bSanctionConversion;
    NewCapture.bOutcomeResolved = true;
    NewCapture.bRewardsGranted = true;
    NewCapture.History.Add(HistoryTag);
    if (!NewResources.IsFinite() || !NewCapture.IsOutcomeInvariantValid(NewAsset))
    {
        return false;
    }

    *AssetRecord = MoveTemp(NewAsset);
    *CaptureRecord = MoveTemp(NewCapture);
    Conflict.Resources = MoveTemp(NewResources);
    if (DamageRecord != nullptr)
    {
        *DamageRecord = MoveTemp(NewDamage);
    }
    Candidate.HistoryTags.AddUnique(HistoryTag);
    Candidate.HistoryTags.Sort([](const FName Left, const FName Right)
        { return Left.LexicalLess(Right); });
    SynchronizeOwnedProjections(Candidate, *AssetRecord);
    if (!CommitCandidate(CampaignAuthority, Candidate, ExpectedNarrativeRevision,
        ExpectedSignalRevision, ExpectedWorldTick))
    {
        return false;
    }
    OnCaptureStateChanged.Broadcast(GetPresentationSnapshot());
    return true;
}

FDACapturePresentationSnapshot UDACaptureComponent::GetPresentationSnapshot() const
{
    FDACapturePresentationSnapshot Result;
    const FDAWorldAssetRecord* AssetRecord = nullptr;
    const FDACaptureRecord* CaptureRecord = nullptr;
    if (!ResolveAuthority(AssetRecord, CaptureRecord)) return Result;
    Result.OriginalOwnerCivilizationId = CaptureRecord->OriginalOwnerCivilizationId;
    Result.CurrentOwnerCivilizationId = AssetRecord->OwnerCivilizationId;
    Result.CapturingCivilizationId = CaptureRecord->CapturingCivilizationId;
    Result.ProgressSeconds = CaptureRecord->CaptureProgressSeconds;
    Result.RequiredSeconds = CaptureRecord->RequiredCaptureTimeSeconds;
    Result.bInProgress = CaptureRecord->bCaptureInProgress;
    Result.bCompleted = CaptureRecord->bCaptureCompleted;
    Result.bOutcomeResolved = CaptureRecord->bOutcomeResolved;
    Result.bRewardsGranted = CaptureRecord->bRewardsGranted;
    Result.bIntegrationRequired = CaptureRecord->bIntegrationRequired;
    Result.Outcome = CaptureRecord->Outcome;
    return Result;
}

bool UDACaptureComponent::CanAcceptSurrender(
    const FDASurrenderContext& Context,
    const FDAOperationConflictSnapshot& Conflict)
{
    const FDASurrenderRecord* Record = Conflict.FindSurrenderRecord(Context.SquadId);
    const bool bMoraleEligible = Context.MoraleState == EDAMoraleState::Breaking
        || Context.MoraleState == EDAMoraleState::Rout;
    return Record != nullptr
        && Record->SquadId.IsValid()
        && !Record->bAccepted
        && !Record->History.Contains(SurrenderAcceptedHistoryTag)
        && Context.SquadDefinitionId == ForgeGuardDefinitionId
        && bMoraleEligible
        && FMath::IsFinite(Context.MilitarySovereignty)
        && Context.MilitarySovereignty >= 0.f
        && Context.MilitarySovereignty <= SurrenderSovereigntyThreshold;
}

bool UDACaptureComponent::AcceptSurrender(
    const FDASurrenderContext& Context,
    const FDASurrenderRewardPolicy& Rewards,
    FDACampaignSnapshot& InOutCampaign)
{
    FDACampaignSnapshot Candidate = InOutCampaign;
    FDAOperationConflictSnapshot& InOutConflict = Candidate.OperationConflict;
    if (!CanAcceptSurrender(Context, InOutConflict)
        || !AreSurrenderRewardsValid(Rewards)
        || !InOutConflict.Resources.IsFinite()
        || InOutConflict.Resources.Capital < 0.f
        || InOutConflict.Resources.Insight < 0.f
        || InOutConflict.Resources.Influence < 0.f
        || InOutConflict.Resources.Materials < 0
        || InOutConflict.Resources.PostConflictLoyalty < 0.f
        || InOutConflict.Resources.FutureSurrenderLikelihood < 0.f)
    {
        return false;
    }

    FDASurrenderRecord* Record = InOutConflict.FindSurrenderRecord(Context.SquadId);
    if (Record == nullptr)
    {
        return false;
    }
    FDASurrenderRecord NewRecord = *Record;
    FDAConflictResourceState NewResources = InOutConflict.Resources;
    NewResources.Influence += Rewards.Influence;
    NewResources.PostConflictLoyalty += Rewards.Loyalty;
    NewResources.FutureSurrenderLikelihood += Rewards.FutureSurrenderLikelihood;
    if (!NewResources.IsFinite())
    {
        return false;
    }
    NewRecord.bAccepted = true;
    NewRecord.History.AddUnique(SurrenderAcceptedHistoryTag);
    *Record = MoveTemp(NewRecord);
    InOutConflict.Resources = MoveTemp(NewResources);
    Candidate.HistoryTags.AddUnique(SurrenderAcceptedHistoryTag);
    Candidate.HistoryTags.Sort([](const FName Left, const FName Right)
        { return Left.LexicalLess(Right); });
    FString Error;
    if (!Candidate.Validate(Error))
    {
        return false;
    }
    InOutCampaign = MoveTemp(Candidate);
    return true;
}

bool UDACaptureComponent::ResolveAuthority(
    const FDAWorldAssetRecord*& OutAssetRecord,
    const FDACaptureRecord*& OutCaptureRecord) const
{
    OutAssetRecord = nullptr;
    OutCaptureRecord = nullptr;
    if (CampaignAuthority == nullptr || !WorldAssetId.IsValid())
    {
        return false;
    }
    const FDACampaignSnapshot& Campaign = CampaignAuthority->GetPersistentCampaign();
    OutAssetRecord = Campaign.FindWorldAssetRecord(WorldAssetId);
    OutCaptureRecord = Campaign.OperationConflict.FindCaptureRecord(WorldAssetId);
    return OutAssetRecord != nullptr
        && OutCaptureRecord != nullptr
        && OutAssetRecord->WorldAssetId == WorldAssetId
        && OutCaptureRecord->WorldAssetId == WorldAssetId;
}

bool UDACaptureComponent::AreFacilityCapturePreconditionsMet(
    const FDAWorldAssetRecord& AssetRecord,
    const FDACaptureRecord& CaptureRecord)
{
    return !CaptureRecord.bCaptureCompleted
        && !CaptureRecord.bOutcomeResolved
        && AssetRecord.ConstructionState == EDAConstructionState::Disabled
        && AssetRecord.StructuralIntegrity >= 10.f;
}

bool UDACaptureComponent::IsCaptureRoleEligible(const EDACaptureAgentRole AgentRole)
{
    return AgentRole == EDACaptureAgentRole::Engineer
        || AgentRole == EDACaptureAgentRole::Founder
        || AgentRole == EDACaptureAgentRole::CaptureCapableSquad;
}

bool UDACaptureComponent::IsInteractionEligible(const FDACaptureInteractionContext& Context)
{
    return Context.InteractionId.IsValid()
        && Context.CaptureActorId.IsValid()
        && IsCaptureRoleEligible(Context.AgentRole)
        && Context.bActorPresent
        && !Context.bContested
        && !Context.bActiveSecurity;
}

bool UDACaptureComponent::AreCaptureRewardsValid(const FDACaptureRecord& Record)
{
    return FMath::IsFinite(Record.StudyInsightReward)
        && Record.StudyInsightReward >= 0.f
        && FMath::IsFinite(Record.SalvageCapitalReward)
        && Record.SalvageCapitalReward >= 0.f
        && Record.SalvageMaterialReward >= 0
        && FMath::IsFinite(Record.GiftInfluenceReward)
        && Record.GiftInfluenceReward >= 0.f
        && FMath::IsFinite(Record.GiftLoyaltyReward)
        && Record.GiftLoyaltyReward >= 0.f;
}

bool UDACaptureComponent::AreSurrenderRewardsValid(const FDASurrenderRewardPolicy& Rewards)
{
    return FMath::IsFinite(Rewards.Influence)
        && Rewards.Influence >= 0.f
        && FMath::IsFinite(Rewards.Loyalty)
        && Rewards.Loyalty >= 0.f
        && FMath::IsFinite(Rewards.FutureSurrenderLikelihood)
        && Rewards.FutureSurrenderLikelihood >= 0.f;
}

bool UDACaptureComponent::IsGiftRecipientAllowed(
    const FDACaptureRecord& Record,
    const FName RecipientCivilizationId)
{
    if (RecipientCivilizationId.IsNone()
        || RecipientCivilizationId == Record.CapturingCivilizationId
        || RecipientCivilizationId == Record.OriginalOwnerCivilizationId)
    {
        return false;
    }
    const FDACaptureGiftRecipientRecord* Recipient = Record.AllowedGiftRecipients.FindByPredicate(
        [RecipientCivilizationId](const FDACaptureGiftRecipientRecord& Candidate)
        { return Candidate.CivilizationId == RecipientCivilizationId; });
    return Recipient != nullptr
        && (Recipient->Relationship == EDAGiftRecipientRelationship::Allied
            || Recipient->Relationship == EDAGiftRecipientRelationship::LocalAuthority);
}

void UDACaptureComponent::ResetActiveCapture(FDACaptureRecord& CaptureRecord)
{
    CaptureRecord.CapturingCivilizationId = NAME_None;
    CaptureRecord.ActiveInteractionId = FGuid();
    CaptureRecord.ActiveCaptureActorId = FGuid();
    CaptureRecord.ActiveCaptureRole = EDACaptureAgentRole::Other;
    CaptureRecord.CaptureProgressSeconds = 0.f;
    CaptureRecord.RequiredCaptureTimeSeconds = 0.f;
    CaptureRecord.bCaptureInProgress = false;
}

void UDACaptureComponent::SynchronizeOwnedProjections(
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
    const int32 ExistingFacilityIndex = Campaign.CitySimulationState.Facilities.IndexOfByPredicate(
        [&AssetRecord](const FDAFacilityContext& Row)
        { return Row.AssetRecord.WorldAssetId == AssetRecord.WorldAssetId; });
    if (AssetRecord.OwnerCivilizationId != TEXT("civilization.synara"))
    {
        if (ExistingFacilityIndex != INDEX_NONE)
        {
            Campaign.CitySimulationState.Facilities.RemoveAt(ExistingFacilityIndex);
        }
        return;
    }
    if (ExistingFacilityIndex != INDEX_NONE)
    {
        Campaign.CitySimulationState.Facilities[ExistingFacilityIndex].AssetRecord = AssetRecord;
    }
    else if (Campaign.CitySimulationState.bInitialized)
    {
        FDAFacilityContext& Facility = Campaign.CitySimulationState.Facilities.Emplace_GetRef();
        Facility.AssetRecord = AssetRecord;
        Facility.FacilityType = EDAFacilityType::Industrial;
    }
}
