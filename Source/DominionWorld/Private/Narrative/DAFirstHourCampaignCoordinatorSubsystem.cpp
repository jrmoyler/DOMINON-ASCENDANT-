#include "Narrative/DAFirstHourCampaignCoordinatorSubsystem.h"

#include "Content/DAFirstHourContentRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"

void UDAFirstHourCampaignCoordinatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ContentRegistry = Collection.InitializeDependency<UDAFirstHourContentRegistrySubsystem>();
    Collection.InitializeDependency<UDAWorldStateSubsystem>();
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        WorldStateSubsystem = GameInstance->GetSubsystem<UDAWorldStateSubsystem>();
    }
    if (WorldStateSubsystem.IsValid())
    {
        WorldStateCommittedHandle = WorldStateSubsystem->OnWorldTickStateCommitted.AddUObject(
            this, &UDAFirstHourCampaignCoordinatorSubsystem::HandleWorldTickStateCommitted);
        ProgressPendingQuests();
    }
}

void UDAFirstHourCampaignCoordinatorSubsystem::Deinitialize()
{
    if (WorldStateSubsystem.IsValid() && WorldStateCommittedHandle.IsValid())
        WorldStateSubsystem->OnWorldTickStateCommitted.Remove(WorldStateCommittedHandle);
    WorldStateCommittedHandle.Reset();
    WorldStateSubsystem.Reset();
    ContentRegistry = nullptr;
    Super::Deinitialize();
}

const FDACampaignSnapshot* UDAFirstHourCampaignCoordinatorSubsystem::GetCampaignSnapshot() const
{
    return WorldStateSubsystem.IsValid() ? &WorldStateSubsystem->GetPersistentCampaign() : nullptr;
}

EDAFirstHourCampaignResult UDAFirstHourCampaignCoordinatorSubsystem::TryProgressInternal(const FName QuestId,
    const EDAFirstHourPlayerAction PlayerAction, const FName ChoiceBranchTag,
    const FGuid VisionActionId, const FGuid ResearchActionId)
{
    if (ContentRegistry == nullptr || !ContentRegistry->IsReady() || !WorldStateSubsystem.IsValid())
        return EDAFirstHourCampaignResult::InvalidManifest;
    const FDACampaignSnapshot& Authority = WorldStateSubsystem->GetPersistentCampaign();
    const int64 ExpectedRevision = Authority.NarrativeState.MutationRevision;
    const int64 ExpectedSignalRevision = Authority.LiveSignals.MutationRevision;
    const int64 WorldTick = WorldStateSubsystem->GetCurrentWorldTick();
    FDACampaignSnapshot Candidate = Authority;
    FDAFirstHourProgressionContext Context;
    Context.WorldTick = WorldTick;
    Context.PlayerAction = PlayerAction;
    Context.ChoiceBranchTag = ChoiceBranchTag;
    Context.LiveSignals = &Authority.LiveSignals;
    Context.VisionActionId = VisionActionId;
    Context.ResearchActionId = ResearchActionId;
    const FDAFirstHourQuestManifest& Manifest = ContentRegistry->GetManifest();
    const EDAFirstHourCampaignResult Result = Candidate.NarrativeState.FindQuestState(QuestId) == nullptr
        ? FDAFirstHourCampaignRuntime::TryStartQuest(Manifest, QuestId, Context, Candidate)
        : FDAFirstHourCampaignRuntime::AdvanceQuest(Manifest, QuestId, Context, Candidate);
    if (Result != EDAFirstHourCampaignResult::Applied) return Result;
    return WorldStateSubsystem->TryCommitPersistentCampaign(
        Candidate, ExpectedRevision, ExpectedSignalRevision, WorldTick)
        ? EDAFirstHourCampaignResult::Applied : EDAFirstHourCampaignResult::ConflictingReplay;
}

EDAFirstHourCampaignResult UDAFirstHourCampaignCoordinatorSubsystem::TryProgress(const FName QuestId,
    const EDAFirstHourPlayerAction PlayerAction, const FName ChoiceBranchTag,
    const FGuid VisionActionId, const FGuid ResearchActionId)
{
    if (bProgressing) return EDAFirstHourCampaignResult::ConflictingReplay;
    TGuardValue<bool> Guard(bProgressing, true);
    const EDAFirstHourCampaignResult Result = TryProgressInternal(
        QuestId, PlayerAction, ChoiceBranchTag, VisionActionId, ResearchActionId);
    if (Result == EDAFirstHourCampaignResult::Applied) ProgressPendingQuests();
    return Result;
}

void UDAFirstHourCampaignCoordinatorSubsystem::ProgressPendingQuests()
{
    if (ContentRegistry == nullptr || !ContentRegistry->IsReady() || !WorldStateSubsystem.IsValid()) return;
    const bool bWasProgressing = bProgressing;
    bProgressing = true;
    bool bApplied = false;
    do
    {
        bApplied = false;
        for (const FDAFirstHourQuestEntry& Quest : ContentRegistry->GetManifest().Quests)
            if (TryProgressInternal(Quest.Definition.QuestId, EDAFirstHourPlayerAction::None,
                NAME_None, FGuid(), FGuid()) == EDAFirstHourCampaignResult::Applied) bApplied = true;
    }
    while (bApplied);
    bProgressing = bWasProgressing;
}

void UDAFirstHourCampaignCoordinatorSubsystem::HandleWorldTickStateCommitted(
    const FDACommittedCampaignSnapshot CommittedState)
{
    if (!bProgressing && WorldStateSubsystem.IsValid()
        && CommittedState->WorldState.CurrentWorldTick == WorldStateSubsystem->GetCurrentWorldTick())
        ProgressPendingQuests();
}

EDAFirstHourCampaignResult UDAFirstHourCampaignCoordinatorSubsystem::CommitNiaStoryTransition(
    const FGuid SourceActionId, const FName ResultStoryState)
{
    if (ContentRegistry == nullptr || !ContentRegistry->IsReady() || !WorldStateSubsystem.IsValid())
        return EDAFirstHourCampaignResult::InvalidManifest;
    const int64 Tick = WorldStateSubsystem->GetCurrentWorldTick();
    const int64 Revision = WorldStateSubsystem->GetPersistentCampaign().NarrativeState.MutationRevision;
    const int64 SignalRevision = WorldStateSubsystem->GetLiveSignals().MutationRevision;
    FDACampaignSnapshot Candidate = WorldStateSubsystem->GetPersistentCampaign();
    const EDAFirstHourCampaignResult Result = FDAFirstHourCampaignRuntime::CommitNiaStoryTransition(
        ContentRegistry->GetManifest(), SourceActionId, ResultStoryState, Tick, Candidate);
    if (Result != EDAFirstHourCampaignResult::Applied) return Result;
    return WorldStateSubsystem->TryCommitPersistentCampaign(Candidate, Revision, SignalRevision, Tick)
        ? Result : EDAFirstHourCampaignResult::ConflictingReplay;
}

EDAFirstHourCampaignResult UDAFirstHourCampaignCoordinatorSubsystem::RecordCrisisCompletion(
    const FName CrisisQuestId, const FName CompletionActionId, const FGuid NarrativeActionId)
{
    if (ContentRegistry == nullptr || !ContentRegistry->IsReady() || !WorldStateSubsystem.IsValid())
        return EDAFirstHourCampaignResult::InvalidManifest;
    const int64 Tick = WorldStateSubsystem->GetCurrentWorldTick();
    const int64 Revision = WorldStateSubsystem->GetPersistentCampaign().NarrativeState.MutationRevision;
    const int64 SignalRevision = WorldStateSubsystem->GetLiveSignals().MutationRevision;
    FDACampaignSnapshot Candidate = WorldStateSubsystem->GetPersistentCampaign();
    const EDAFirstHourCampaignResult Result = FDAFirstHourCampaignRuntime::RecordCrisisCompletion(
        ContentRegistry->GetManifest(), CrisisQuestId, CompletionActionId, NarrativeActionId, Tick, Candidate);
    if (Result != EDAFirstHourCampaignResult::Applied) return Result;
    return WorldStateSubsystem->TryCommitPersistentCampaign(Candidate, Revision, SignalRevision, Tick)
        ? Result : EDAFirstHourCampaignResult::ConflictingReplay;
}

EDAFirstHourCampaignResult UDAFirstHourCampaignCoordinatorSubsystem::RecordAuditEligibilitySource(
    const FName EligibilityId, const FGuid SourceActionId, const FName SourceActionTag)
{
    if (!WorldStateSubsystem.IsValid()) return EDAFirstHourCampaignResult::InvalidState;
    const int64 Tick = WorldStateSubsystem->GetCurrentWorldTick();
    const int64 Revision = WorldStateSubsystem->GetPersistentCampaign().NarrativeState.MutationRevision;
    const int64 SignalRevision = WorldStateSubsystem->GetLiveSignals().MutationRevision;
    FDACampaignSnapshot Candidate = WorldStateSubsystem->GetPersistentCampaign();
    const EDAFirstHourCampaignResult Result = FDAFirstHourCampaignRuntime::RecordAuditEligibilitySource(
        EligibilityId, SourceActionId, SourceActionTag, Tick, Candidate);
    if (Result != EDAFirstHourCampaignResult::Applied) return Result;
    return WorldStateSubsystem->TryCommitPersistentCampaign(Candidate, Revision, SignalRevision, Tick)
        ? Result : EDAFirstHourCampaignResult::ConflictingReplay;
}

EDAFirstHourCampaignResult UDAFirstHourCampaignCoordinatorSubsystem::SubmitPlayerAction(
    const FName QuestId, const EDAFirstHourPlayerAction PlayerAction, const FName ChoiceBranchTag,
    const FGuid VisionActionId, const FGuid ResearchActionId)
{
    return TryProgress(QuestId, PlayerAction, ChoiceBranchTag, VisionActionId, ResearchActionId);
}

EDAFirstHourCampaignResult UDAFirstHourCampaignCoordinatorSubsystem::SubmitNiaStoryAction(
    const FGuid SourceActionId, const FName ResultStoryState)
{
    return CommitNiaStoryTransition(SourceActionId, ResultStoryState);
}

EDAFirstHourCampaignResult UDAFirstHourCampaignCoordinatorSubsystem::SubmitCrisisAction(
    const FName CrisisQuestId, const FName CompletionActionId, const FGuid NarrativeActionId)
{
    return RecordCrisisCompletion(CrisisQuestId, CompletionActionId, NarrativeActionId);
}

EDAFirstHourCampaignResult UDAFirstHourCampaignCoordinatorSubsystem::SubmitAuditAction(
    const FName EligibilityId, const FGuid SourceActionId, const FName SourceActionTag)
{
    return RecordAuditEligibilitySource(EligibilityId, SourceActionId, SourceActionTag);
}

EDAFirstHourCampaignResult UDAFirstHourCampaignCoordinatorSubsystem::SubmitResearchAction(
    const FGuid SourceActionId)
{
    if (!WorldStateSubsystem.IsValid() || !SourceActionId.IsValid())
    {
        return EDAFirstHourCampaignResult::InvalidState;
    }
    const FDACampaignSnapshot& Authority = WorldStateSubsystem->GetPersistentCampaign();
    if (const FDAAuditEligibilitySourceRecord* Existing =
        Authority.NarrativeState.AuditEligibilitySourceRecords.FindByPredicate(
            [SourceActionId](const FDAAuditEligibilitySourceRecord& Row)
            { return Row.SourceActionId == SourceActionId; }))
    {
        return Existing->EligibilityId == TEXT("ResearchAction")
            && Existing->SourceActionTag == TEXT("action.research.replacement_model.completed")
            ? EDAFirstHourCampaignResult::AlreadyApplied
            : EDAFirstHourCampaignResult::ConflictingReplay;
    }

    const int64 Tick = Authority.WorldState.CurrentWorldTick;
    const int64 NarrativeRevision = Authority.NarrativeState.MutationRevision;
    const int64 SignalRevision = Authority.LiveSignals.MutationRevision;
    FDACampaignSnapshot Candidate = Authority;
    const FName ActionTag(TEXT("action.research.replacement_model.completed"));
    Candidate.HistoryTags.AddUnique(ActionTag);
    Candidate.HistoryTags.Sort([](const FName Left, const FName Right)
        { return Left.LexicalLess(Right); });
    FDANarrativeActionRecord& Action = Candidate.NarrativeState.ActionRecords.Emplace_GetRef();
    Action.ActionId = SourceActionId;
    Action.NormalizedActionTags = {ActionTag};
    Action.WorldTick = Tick;
    Candidate.NarrativeState.ActionRecords.Sort([](const FDANarrativeActionRecord& Left,
        const FDANarrativeActionRecord& Right)
        { return Left.ActionId.ToString() < Right.ActionId.ToString(); });
    ++Candidate.NarrativeState.MutationRevision;
    const EDAFirstHourCampaignResult ProofResult =
        FDAFirstHourCampaignRuntime::RecordAuditEligibilitySource(
            TEXT("ResearchAction"), SourceActionId, ActionTag, Tick, Candidate);
    if (ProofResult != EDAFirstHourCampaignResult::Applied)
    {
        return ProofResult;
    }
    return WorldStateSubsystem->TryCommitPersistentCampaign(
        Candidate, NarrativeRevision, SignalRevision, Tick)
        ? EDAFirstHourCampaignResult::Applied
        : EDAFirstHourCampaignResult::ConflictingReplay;
}
