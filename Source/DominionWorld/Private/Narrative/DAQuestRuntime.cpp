#include "Narrative/DAQuestRuntime.h"

namespace
{
    bool IsTerminalQuestNode(const EDAQuestNodeType Type)
    {
        return Type == EDAQuestNodeType::Failure || Type == EDAQuestNodeType::Resolution;
    }

    bool IsWorldAssetDestroyed(const FDACampaignSnapshot& Campaign, const FDAQuestSaveState& State, const FName BindingId)
    {
        const FDAQuestWorldAssetBinding* Binding = State.FindWorldAssetBinding(BindingId);
        const FDAWorldAssetRecord* Asset = Binding != nullptr ? Campaign.FindWorldAssetRecord(Binding->WorldAssetId) : nullptr;
        return Asset == nullptr || Asset->StructuralIntegrity <= 0.f || Asset->ConstructionState == EDAConstructionState::Ruined;
    }

    bool EdgeMatches(const FDAQuestEdgeDefinition& Edge, const FDACampaignSnapshot& Campaign, const FDAQuestSaveState& State)
    {
        switch (Edge.Condition)
        {
        case EDAQuestEdgeCondition::Always: return true;
        case EDAQuestEdgeCondition::WorldAssetAvailable: return !IsWorldAssetDestroyed(Campaign, State, Edge.WorldAssetBindingId);
        case EDAQuestEdgeCondition::WorldAssetDestroyed: return IsWorldAssetDestroyed(Campaign, State, Edge.WorldAssetBindingId);
        default: return false;
        }
    }

    bool CompareValue(const double Actual, const FDAQuestConditionPayload& Condition)
    {
        switch (Condition.Comparison)
        {
        case EDAQuestComparison::Equal: return FMath::IsNearlyEqual(Actual, Condition.ExpectedValue);
        case EDAQuestComparison::NotEqual: return !FMath::IsNearlyEqual(Actual, Condition.ExpectedValue);
        case EDAQuestComparison::GreaterOrEqual: return Actual >= Condition.ExpectedValue;
        case EDAQuestComparison::LessOrEqual: return Actual <= Condition.ExpectedValue;
        default: return false;
        }
    }

    bool PayloadSatisfied(const FDAQuestNodeDefinition& Node, const FDAQuestSaveState& State,
        const FDAQuestEvaluationContext& Context, const FDACampaignSnapshot& Campaign)
    {
        switch (Node.Payload.Variant)
        {
        case EDAQuestPayloadVariant::None:
            return true;
        case EDAQuestPayloadVariant::Timer:
            return Context.WorldTick >= State.CurrentNodeEnteredWorldTick
                && Context.WorldTick - State.CurrentNodeEnteredWorldTick >= Node.Payload.Timer.DurationWorldTicks;
        case EDAQuestPayloadVariant::WorldAsset:
            return !IsWorldAssetDestroyed(Campaign, State, Node.Payload.WorldAssetBindingId);
        case EDAQuestPayloadVariant::World:
        case EDAQuestPayloadVariant::Citizen:
        case EDAQuestPayloadVariant::Faction:
        case EDAQuestPayloadVariant::Economy:
        case EDAQuestPayloadVariant::Relationship:
        {
            double Actual = 0.0;
            return Context.TryGetMetric(Node.Payload.Variant, Node.Payload.Condition.EvaluationKey, Actual)
                && CompareValue(Actual, Node.Payload.Condition);
        }
        default:
            return false;
        }
    }

    bool IncrementRevision(int64& InOutRevision)
    {
        if (InOutRevision == MAX_int64) return false;
        ++InOutRevision;
        return true;
    }

    bool QuestManifestMatches(const FDAQuestSaveState& State, const FDAQuestDefinitionManifest& Manifest)
    {
        return State.DefinitionVersion == Manifest.Version
            && State.DefinitionManifest.DefinitionFingerprint == Manifest.DefinitionFingerprint;
    }

    bool EventManifestMatches(const FDAWorldEventSaveState& State, const FDAWorldEventDefinitionManifest& Manifest)
    {
        return State.DefinitionVersion == Manifest.Version
            && State.DefinitionManifest.DefinitionFingerprint == Manifest.DefinitionFingerprint;
    }

    EDAQuestRuntimeResult ApplyQuestTransition(const FDAQuestDefinitionManifest& Manifest,
        const FDAQuestEdgeDefinition& Selected, const FDAQuestEvaluationContext& Context,
        FDACampaignSnapshot& InOutCampaign)
    {
        const FDAQuestNodeDefinition* Target = Manifest.FindNode(Selected.TargetNodeId);
        if (Target == nullptr) return EDAQuestRuntimeResult::InvalidDefinition;
        FDACampaignSnapshot Candidate = InOutCampaign;
        FDAQuestSaveState* State = Candidate.NarrativeState.FindQuestState(Manifest.QuestId);
        if (State == nullptr) return EDAQuestRuntimeResult::InvalidState;
        FDAQuestNodeTransitionRecord& Transition = State->NodeTransitionRecords.Emplace_GetRef();
        Transition.CompletedNodeId = State->CurrentNodeId;
        Transition.EnteredNodeId = Target->NodeId;
        Transition.WorldTick = Context.WorldTick;
        State->CompletedNodeIds.AddUnique(State->CurrentNodeId);
        State->CurrentNodeId = Target->NodeId;
        State->LastTransitionWorldTick = Context.WorldTick;
        State->CurrentNodeEnteredWorldTick = Context.WorldTick;
        if (Target->Type == EDAQuestNodeType::Resolution) State->ProgressState = EDAQuestProgressState::Completed;
        else if (Target->Type == EDAQuestNodeType::Failure) State->ProgressState = EDAQuestProgressState::Failed;
        if (!IncrementRevision(Candidate.NarrativeState.MutationRevision)) return EDAQuestRuntimeResult::InvalidState;
        FString Error;
        if (!Candidate.Validate(Error)) return EDAQuestRuntimeResult::InvalidState;
        InOutCampaign = MoveTemp(Candidate);
        return EDAQuestRuntimeResult::Applied;
    }
}

const TArray<EDAQuestNodeType>& FDAQuestDefinition::GetSupportedNodeTypes()
{
    static const TArray<EDAQuestNodeType> Types = {
        EDAQuestNodeType::Start, EDAQuestNodeType::Dialogue, EDAQuestNodeType::Objective,
        EDAQuestNodeType::Investigation, EDAQuestNodeType::Build, EDAQuestNodeType::Deliver,
        EDAQuestNodeType::Explore, EDAQuestNodeType::Combat, EDAQuestNodeType::Defend,
        EDAQuestNodeType::Capture, EDAQuestNodeType::Choice, EDAQuestNodeType::Wait,
        EDAQuestNodeType::Timer, EDAQuestNodeType::WorldCondition, EDAQuestNodeType::CitizenCondition,
        EDAQuestNodeType::FactionCondition, EDAQuestNodeType::EconomyCondition,
        EDAQuestNodeType::RelationshipCondition, EDAQuestNodeType::EventTrigger,
        EDAQuestNodeType::Reward, EDAQuestNodeType::Failure, EDAQuestNodeType::Resolution};
    return Types;
}

const FDAQuestNodeDefinition* FDAQuestDefinition::FindNode(const FName NodeId) const
{
    return Nodes.FindByPredicate([NodeId](const FDAQuestNodeDefinition& Node) { return Node.NodeId == NodeId; });
}

FDAQuestDefinitionManifest FDAQuestDefinition::BuildManifest() const
{
    FDAQuestDefinitionManifest Manifest;
    Manifest.QuestId = QuestId;
    Manifest.SourceDefinitionId = SourceDefinitionId;
    Manifest.Version = Version;
    Manifest.StartNodeId = StartNodeId;
    Manifest.RequiredWorldAssetBindingIds = RequiredWorldAssetBindingIds;
    Manifest.Nodes = Nodes;
    Manifest.RefreshFingerprint();
    return Manifest;
}

bool FDAQuestDefinition::Validate(FString& OutError) const { return BuildManifest().Validate(OutError); }

const FDAWorldEventStageDefinition* FDAWorldEventDefinition::FindStage(const FName StageId) const
{
    return Stages.FindByPredicate([StageId](const FDAWorldEventStageDefinition& Stage) { return Stage.StageId == StageId; });
}

FDAWorldEventDefinitionManifest FDAWorldEventDefinition::BuildManifest() const
{
    FDAWorldEventDefinitionManifest Manifest;
    Manifest.EventId = EventId;
    Manifest.SourceDefinitionId = SourceDefinitionId;
    Manifest.Version = Version;
    Manifest.Scope = Scope;
    Manifest.InitialStageId = InitialStageId;
    Manifest.Stages = Stages;
    Manifest.RefreshFingerprint();
    return Manifest;
}

bool FDAWorldEventDefinition::Validate(FString& OutError) const { return BuildManifest().Validate(OutError); }

EDAQuestRuntimeResult FDAQuestRuntime::StartQuest(const FDAQuestDefinition& Definition,
    const TArray<FDAQuestWorldAssetBinding>& WorldAssetBindings, const int64 WorldTick,
    FDACampaignSnapshot& InOutCampaign)
{
    const FDAQuestDefinitionManifest Manifest = Definition.BuildManifest();
    FString Error;
    if (WorldTick < 0 || !Manifest.Validate(Error)) return EDAQuestRuntimeResult::InvalidDefinition;
    if (!InOutCampaign.Validate(Error)) return EDAQuestRuntimeResult::InvalidState;
    TSet<FName> BindingIds;
    for (const FDAQuestWorldAssetBinding& Binding : WorldAssetBindings)
    {
        if (Binding.BindingId.IsNone() || BindingIds.Contains(Binding.BindingId) || !Binding.WorldAssetId.IsValid()
            || InOutCampaign.FindWorldAssetRecord(Binding.WorldAssetId) == nullptr)
        {
            return EDAQuestRuntimeResult::InvalidState;
        }
        BindingIds.Add(Binding.BindingId);
    }
    if (BindingIds.Num() != Manifest.RequiredWorldAssetBindingIds.Num()) return EDAQuestRuntimeResult::InvalidState;
    for (const FName Required : Manifest.RequiredWorldAssetBindingIds)
    {
        if (!BindingIds.Contains(Required)) return EDAQuestRuntimeResult::InvalidState;
    }
    if (const FDAQuestSaveState* Existing = InOutCampaign.NarrativeState.FindQuestState(Manifest.QuestId))
    {
        if (Existing->DefinitionVersion != Manifest.Version) return EDAQuestRuntimeResult::InvalidState;
        if (!QuestManifestMatches(*Existing, Manifest)) return EDAQuestRuntimeResult::DefinitionMismatch;
        if (Existing->WorldAssetBindings.Num() != WorldAssetBindings.Num()) return EDAQuestRuntimeResult::InvalidState;
        for (const FDAQuestWorldAssetBinding& Binding : WorldAssetBindings)
        {
            const FDAQuestWorldAssetBinding* ExistingBinding = Existing->FindWorldAssetBinding(Binding.BindingId);
            if (ExistingBinding == nullptr || ExistingBinding->WorldAssetId != Binding.WorldAssetId)
                return EDAQuestRuntimeResult::InvalidState;
        }
        return EDAQuestRuntimeResult::AlreadyApplied;
    }
    FDACampaignSnapshot Candidate = InOutCampaign;
    FDAQuestSaveState State;
    State.QuestId = Manifest.QuestId;
    State.DefinitionManifest = Manifest;
    State.DefinitionVersion = Manifest.Version;
    State.CurrentNodeId = Manifest.StartNodeId;
    State.StartedWorldTick = WorldTick;
    State.LastTransitionWorldTick = WorldTick;
    State.CurrentNodeEnteredWorldTick = WorldTick;
    State.WorldAssetBindings = WorldAssetBindings;
    State.WorldAssetBindings.Sort([](const FDAQuestWorldAssetBinding& Left, const FDAQuestWorldAssetBinding& Right)
    {
        return Left.BindingId.LexicalLess(Right.BindingId);
    });
    Candidate.NarrativeState.QuestStates.Add(MoveTemp(State));
    Candidate.NarrativeState.QuestStates.Sort([](const FDAQuestSaveState& Left, const FDAQuestSaveState& Right)
    {
        return Left.QuestId.LexicalLess(Right.QuestId);
    });
    if (!IncrementRevision(Candidate.NarrativeState.MutationRevision) || !Candidate.Validate(Error))
        return EDAQuestRuntimeResult::InvalidState;
    InOutCampaign = MoveTemp(Candidate);
    return EDAQuestRuntimeResult::Applied;
}

EDAQuestRuntimeResult FDAQuestRuntime::EvaluateCurrentNode(const FDAQuestDefinition& Definition,
    const FDAQuestEvaluationContext& EvaluationContext, FDACampaignSnapshot& InOutCampaign,
    FName& OutSelectedBranchTag)
{
    OutSelectedBranchTag = NAME_None;
    const FDAQuestDefinitionManifest Manifest = Definition.BuildManifest();
    FString Error;
    if (!Manifest.Validate(Error)) return EDAQuestRuntimeResult::InvalidDefinition;
    if (!InOutCampaign.Validate(Error)) return EDAQuestRuntimeResult::InvalidState;
    const FDAQuestSaveState* Existing = InOutCampaign.NarrativeState.FindQuestState(Manifest.QuestId);
    if (Existing == nullptr) return EDAQuestRuntimeResult::NotFound;
    if (!QuestManifestMatches(*Existing, Manifest)) return EDAQuestRuntimeResult::DefinitionMismatch;
    if (Existing->ProgressState != EDAQuestProgressState::Active
        || EvaluationContext.WorldTick < Existing->LastTransitionWorldTick)
    {
        return EDAQuestRuntimeResult::InvalidState;
    }
    const FDAQuestNodeDefinition* CurrentNode = Manifest.FindNode(Existing->CurrentNodeId);
    if (CurrentNode == nullptr || IsTerminalQuestNode(CurrentNode->Type)) return EDAQuestRuntimeResult::InvalidState;
    if (CurrentNode->Type == EDAQuestNodeType::Choice) return EDAQuestRuntimeResult::RequiresChoice;
    if (!PayloadSatisfied(*CurrentNode, *Existing, EvaluationContext, InOutCampaign)) return EDAQuestRuntimeResult::Waiting;

    TArray<const FDAQuestEdgeDefinition*> Matches;
    for (const FDAQuestEdgeDefinition& Edge : CurrentNode->Edges)
    {
        if (EdgeMatches(Edge, InOutCampaign, *Existing)) Matches.Add(&Edge);
    }
    if (Matches.IsEmpty()) return EDAQuestRuntimeResult::Waiting;
    if (Matches.Num() != 1) return EDAQuestRuntimeResult::AmbiguousTransition;
    const FDAQuestEdgeDefinition& Selected = *Matches[0];
    const EDAQuestRuntimeResult Result = ApplyQuestTransition(Manifest, Selected, EvaluationContext, InOutCampaign);
    if (Result == EDAQuestRuntimeResult::Applied) OutSelectedBranchTag = Selected.BranchTag;
    return Result;
}

EDAQuestRuntimeResult FDAQuestRuntime::SelectChoice(const FDAQuestDefinition& Definition, const FName BranchTag,
    const FDAQuestEvaluationContext& EvaluationContext, FDACampaignSnapshot& InOutCampaign)
{
    const FDAQuestDefinitionManifest Manifest = Definition.BuildManifest();
    FString Error;
    if (BranchTag.IsNone() || !Manifest.Validate(Error)) return EDAQuestRuntimeResult::InvalidDefinition;
    if (!InOutCampaign.Validate(Error)) return EDAQuestRuntimeResult::InvalidState;
    const FDAQuestSaveState* Existing = InOutCampaign.NarrativeState.FindQuestState(Manifest.QuestId);
    if (Existing == nullptr) return EDAQuestRuntimeResult::NotFound;
    if (!QuestManifestMatches(*Existing, Manifest)) return EDAQuestRuntimeResult::DefinitionMismatch;
    if (Existing->ProgressState != EDAQuestProgressState::Active
        || EvaluationContext.WorldTick < Existing->LastTransitionWorldTick)
    {
        return EDAQuestRuntimeResult::InvalidState;
    }
    const FDAQuestNodeDefinition* Current = Manifest.FindNode(Existing->CurrentNodeId);
    if (Current == nullptr || Current->Type != EDAQuestNodeType::Choice) return EDAQuestRuntimeResult::InvalidState;
    const FDAQuestEdgeDefinition* Selected = Current->Edges.FindByPredicate(
        [BranchTag](const FDAQuestEdgeDefinition& Edge) { return Edge.BranchTag == BranchTag; });
    if (Selected == nullptr || !EdgeMatches(*Selected, InOutCampaign, *Existing)) return EDAQuestRuntimeResult::InvalidState;
    return ApplyQuestTransition(Manifest, *Selected, EvaluationContext, InOutCampaign);
}

EDAQuestRuntimeResult FDAQuestRuntime::AbandonQuest(
    const FDAQuestDefinition& Definition, FDACampaignSnapshot& InOutCampaign)
{
    const FDAQuestDefinitionManifest Manifest = Definition.BuildManifest();
    FString Error;
    if (!Manifest.Validate(Error)) return EDAQuestRuntimeResult::InvalidDefinition;
    if (!InOutCampaign.Validate(Error)) return EDAQuestRuntimeResult::InvalidState;
    const FDAQuestSaveState* Existing = InOutCampaign.NarrativeState.FindQuestState(Manifest.QuestId);
    if (Existing == nullptr) return EDAQuestRuntimeResult::NotFound;
    if (!QuestManifestMatches(*Existing, Manifest)) return EDAQuestRuntimeResult::DefinitionMismatch;
    if (Existing->ProgressState == EDAQuestProgressState::Abandoned)
        return EDAQuestRuntimeResult::AlreadyApplied;
    const FDAQuestNodeDefinition* Current = Manifest.FindNode(Existing->CurrentNodeId);
    if (Existing->ProgressState != EDAQuestProgressState::Active || Current == nullptr
        || IsTerminalQuestNode(Current->Type)) return EDAQuestRuntimeResult::InvalidState;
    FDACampaignSnapshot Candidate = InOutCampaign;
    FDAQuestSaveState* State = Candidate.NarrativeState.FindQuestState(Manifest.QuestId);
    if (State == nullptr) return EDAQuestRuntimeResult::InvalidState;
    State->ProgressState = EDAQuestProgressState::Abandoned;
    if (!IncrementRevision(Candidate.NarrativeState.MutationRevision) || !Candidate.Validate(Error))
        return EDAQuestRuntimeResult::InvalidState;
    InOutCampaign = MoveTemp(Candidate);
    return EDAQuestRuntimeResult::Applied;
}

EDAWorldEventRuntimeResult FDAWorldEventRuntime::StartEvent(const FDAWorldEventDefinition& Definition,
    const int64 WorldTick, FDACampaignSnapshot& InOutCampaign)
{
    const FDAWorldEventDefinitionManifest Manifest = Definition.BuildManifest();
    FString Error;
    if (WorldTick < 0 || !Manifest.Validate(Error)) return EDAWorldEventRuntimeResult::InvalidDefinition;
    if (!InOutCampaign.Validate(Error)) return EDAWorldEventRuntimeResult::InvalidState;
    if (const FDAWorldEventSaveState* Existing = InOutCampaign.NarrativeState.FindEventState(Manifest.EventId))
    {
        if (Existing->DefinitionVersion != Manifest.Version) return EDAWorldEventRuntimeResult::InvalidState;
        return EventManifestMatches(*Existing, Manifest)
            ? EDAWorldEventRuntimeResult::AlreadyApplied
            : EDAWorldEventRuntimeResult::DefinitionMismatch;
    }
    FDACampaignSnapshot Candidate = InOutCampaign;
    FDAWorldEventSaveState State;
    State.EventId = Manifest.EventId;
    State.DefinitionManifest = Manifest;
    State.DefinitionVersion = Manifest.Version;
    State.CurrentStageId = Manifest.InitialStageId;
    State.StartedWorldTick = WorldTick;
    State.LastTransitionWorldTick = WorldTick;
    Candidate.NarrativeState.EventStates.Add(MoveTemp(State));
    Candidate.NarrativeState.EventStates.Sort([](const FDAWorldEventSaveState& Left, const FDAWorldEventSaveState& Right)
    {
        return Left.EventId.LexicalLess(Right.EventId);
    });
    if (!IncrementRevision(Candidate.NarrativeState.MutationRevision) || !Candidate.Validate(Error))
        return EDAWorldEventRuntimeResult::InvalidState;
    InOutCampaign = MoveTemp(Candidate);
    return EDAWorldEventRuntimeResult::Applied;
}

EDAWorldEventRuntimeResult FDAWorldEventRuntime::AdvanceEvent(const FDAWorldEventDefinition& Definition,
    const FName NextStageId, const int64 WorldTick, FDACampaignSnapshot& InOutCampaign)
{
    const FDAWorldEventDefinitionManifest Manifest = Definition.BuildManifest();
    FString Error;
    if (!Manifest.Validate(Error)) return EDAWorldEventRuntimeResult::InvalidDefinition;
    if (!InOutCampaign.Validate(Error)) return EDAWorldEventRuntimeResult::InvalidState;
    const FDAWorldEventSaveState* Existing = InOutCampaign.NarrativeState.FindEventState(Manifest.EventId);
    if (Existing == nullptr) return EDAWorldEventRuntimeResult::NotFound;
    if (!EventManifestMatches(*Existing, Manifest)) return EDAWorldEventRuntimeResult::DefinitionMismatch;
    const FDAWorldEventStageDefinition* Current = Manifest.FindStage(Existing->CurrentStageId);
    const FDAWorldEventStageDefinition* Next = Manifest.FindStage(NextStageId);
    if (Existing->ProgressState != EDAWorldEventProgressState::Active || WorldTick < Existing->LastTransitionWorldTick
        || Current == nullptr || Next == nullptr || !Current->AllowedNextStageIds.Contains(NextStageId))
    {
        return EDAWorldEventRuntimeResult::InvalidState;
    }
    FDACampaignSnapshot Candidate = InOutCampaign;
    FDAWorldEventSaveState* State = Candidate.NarrativeState.FindEventState(Manifest.EventId);
    if (State == nullptr) return EDAWorldEventRuntimeResult::InvalidState;
    State->CompletedStageIds.AddUnique(State->CurrentStageId);
    State->CurrentStageId = NextStageId;
    State->LastTransitionWorldTick = WorldTick;
    State->ProgressState = Next->bResolution ? EDAWorldEventProgressState::Resolved : EDAWorldEventProgressState::Active;
    if (!IncrementRevision(Candidate.NarrativeState.MutationRevision) || !Candidate.Validate(Error))
        return EDAWorldEventRuntimeResult::InvalidState;
    InOutCampaign = MoveTemp(Candidate);
    return EDAWorldEventRuntimeResult::Applied;
}
