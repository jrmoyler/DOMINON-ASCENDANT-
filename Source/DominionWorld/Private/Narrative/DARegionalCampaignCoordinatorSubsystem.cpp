#include "Narrative/DARegionalCampaignCoordinatorSubsystem.h"

#include "Content/DARegionalCrisisContentRegistrySubsystem.h"
#include "Diplomacy/DADiplomacySystem.h"
#include "Engine/GameInstance.h"
#include "Misc/LexFromString.h"
#include "Subsystems/SubsystemCollection.h"

namespace
{
    FName SourceId(const FName Id) { return FName(*(TEXT("regional.") + Id.ToString())); }

    FDAQuestDefinition MakeQuestDefinition(const FDARegionalQuestEntry& Entry)
    {
        FDAQuestDefinition Definition;
        Definition.QuestId = Entry.QuestId;
        Definition.SourceDefinitionId = SourceId(Entry.QuestId);
        Definition.StartNodeId = TEXT("start");
        for (const FDARegionalQuestNodeEntry& AuthoredNode : Entry.Nodes)
        {
            FDAQuestNodeDefinition Node;
            Node.NodeId = AuthoredNode.NodeId;
            Node.Type = AuthoredNode.NodeType == TEXT("start") ? EDAQuestNodeType::Start
                : AuthoredNode.NodeType == TEXT("choice") ? EDAQuestNodeType::Choice
                : EDAQuestNodeType::Resolution;
            Node.SourceDefinitionId = Definition.SourceDefinitionId;
            for (const FDARegionalQuestEdgeEntry& AuthoredEdge : AuthoredNode.Edges)
            {
                FDAQuestEdgeDefinition Edge;
                Edge.BranchTag = AuthoredEdge.Branch; Edge.TargetNodeId = AuthoredEdge.Target;
                Node.Edges.Add(Edge);
            }
            Definition.Nodes.Add(Node);
        }
        return Definition;
    }

    FDAWorldEventDefinition MakeEventDefinition(const FDARegionalWorldEventEntry& Entry)
    {
        FDAWorldEventDefinition Definition;
        Definition.EventId = Entry.EventId;
        Definition.SourceDefinitionId = SourceId(Entry.EventId);
        Definition.Scope = Entry.Scope == TEXT("regional") ? EDAWorldEventScope::Regional : EDAWorldEventScope::Local;
        Definition.InitialStageId = Entry.InitialStageId;
        for (const FDARegionalCrisisStageDefinition& AuthoredStage : Entry.Stages)
        {
            FDAWorldEventStageDefinition Stage;
            Stage.StageId = AuthoredStage.StageId; Stage.SourceDefinitionId = Definition.SourceDefinitionId;
            Stage.AllowedNextStageIds = AuthoredStage.NextStageIds;
            Definition.Stages.Add(Stage);
        }
        for (const FDARegionalCrisisResolutionDefinition& EntryResolution : Entry.Resolutions)
        {
            FDAWorldEventStageDefinition Resolution;
            Resolution.StageId = EntryResolution.ResolutionId;
            Resolution.SourceDefinitionId = Definition.SourceDefinitionId;
            Resolution.bResolution = true; Definition.Stages.Add(Resolution);
        }
        return Definition;
    }

    bool TryResolveMetric(const FName Metric, const FDACampaignSnapshot& Campaign, double& OutValue)
    {
        if (Metric == TEXT("forgeweave.resource_hunger"))
        { OutValue = Campaign.WorldState.Forgeweave.ResourceHunger; return true; }
        if (Metric == TEXT("forgeweave.trust"))
        {
            const FDADiplomaticRelationship* Relationship = Campaign.WorldState.Diplomacy.FindRelationship(
                TEXT("relationship.synara.forgeweave"));
            if (Relationship == nullptr) return false;
            OutValue = Relationship->Trust; return true;
        }
        if (Metric == TEXT("synara.dependency"))
        { OutValue = Campaign.SynaraState.Dependency; return true; }
        if (Metric == TEXT("eden.ecological_balance"))
        { OutValue = Campaign.WorldState.Ecology.Balance; return true; }
        if (Metric == TEXT("city.attractiveness_and_vacancy"))
        {
            const int32 Housing = Campaign.WorldState.Forgeweave.HousingCapacity;
            OutValue = Housing <= 0 ? (Campaign.LiveSignals.Population > 0 ? 100.0 : 0.0)
                : 100.0 * static_cast<double>(Campaign.LiveSignals.Population) / static_cast<double>(Housing);
            return true;
        }
        if (Metric == TEXT("city.power_reserve_percent"))
        {
            double Reserve = 100.0;
            for (const FDACampaignUtilitySignal& Signal : Campaign.LiveSignals.UtilitySignals)
            {
                if (Signal.Utility != EDACampaignUtilityKind::Power) continue;
                const double SignalReserve = Signal.Supply == EDACampaignUtilitySupply::FullySupplied ? 100.0
                    : Signal.Supply == EDACampaignUtilitySupply::MinorDeficit ? 75.0
                    : Signal.Supply == EDACampaignUtilitySupply::SignificantDeficit ? 50.0
                    : Signal.Supply == EDACampaignUtilitySupply::Critical ? 4.0 : 0.0;
                Reserve = FMath::Min(Reserve, SignalReserve);
            }
            OutValue = Reserve; return true;
        }
        if (Metric == TEXT("route.transit_wear"))
        {
            OutValue = 0.0;
            for (const FDATradeContractState& Contract : Campaign.WorldState.Trade.Contracts)
            {
                if (Contract.ProcessedWorldTicks.IsEmpty()) continue;
                OutValue = FMath::Max(OutValue, 100.0 * static_cast<double>(Contract.FailedDeliveryCount)
                    / static_cast<double>(Contract.ProcessedWorldTicks.Num()));
            }
            return true;
        }
        if (Metric == TEXT("region.neighbor_stability"))
        {
            OutValue = Campaign.WorldState.Regions.ContainsByPredicate([](const FDARegionState& Region)
                { return Region.PersistentDelta.StateTags.Contains(TEXT("stability.collapse")); }) ? 0.0 : 100.0;
            return true;
        }
        return false;
    }

    bool CompareMetric(const double Actual, const FName Comparison, const double Threshold)
    {
        return Comparison == TEXT("greater_than") ? Actual > Threshold
            : Comparison == TEXT("less_than") ? Actual < Threshold
            : Comparison == TEXT("greater_or_equal") ? Actual >= Threshold
            : Comparison == TEXT("less_or_equal") ? Actual <= Threshold
            : Comparison == TEXT("equal") ? Actual == Threshold
            : Comparison == TEXT("not_equal") && Actual != Threshold;
    }

    bool EvaluateQuestPredicate(FString Predicate, const FDACampaignSnapshot& Campaign)
    {
        Predicate.TrimStartAndEndInline();
        for (const TCHAR* Operator : {TEXT(">="), TEXT("<="), TEXT("=="), TEXT("!="), TEXT(">"), TEXT("<")})
        {
            const int32 At = Predicate.Find(Operator);
            if (At == INDEX_NONE) continue;
            const FString MetricText = Predicate.Left(At).TrimStartAndEnd();
            const FString ThresholdText = Predicate.Mid(At + FCString::Strlen(Operator)).TrimStartAndEnd();
            double Threshold = 0.0; double Actual = 0.0;
            if (!LexTryParseString(Threshold, *ThresholdText)
                || !TryResolveMetric(FName(*MetricText), Campaign, Actual)) return false;
            const FName Comparison = FCString::Strcmp(Operator, TEXT(">")) == 0 ? FName(TEXT("greater_than"))
                : FCString::Strcmp(Operator, TEXT("<")) == 0 ? FName(TEXT("less_than"))
                : FCString::Strcmp(Operator, TEXT(">=")) == 0 ? FName(TEXT("greater_or_equal"))
                : FCString::Strcmp(Operator, TEXT("<=")) == 0 ? FName(TEXT("less_or_equal"))
                : FCString::Strcmp(Operator, TEXT("==")) == 0 ? FName(TEXT("equal")) : FName(TEXT("not_equal"));
            return CompareMetric(Actual, Comparison, Threshold);
        }
        const FName Key(*Predicate);
        if (Predicate.StartsWith(TEXT("event.")))
        {
            const FDAWorldEventSaveState* Event = Campaign.NarrativeState.FindEventState(Key);
            return Event != nullptr && Event->ProgressState == EDAWorldEventProgressState::Active;
        }
        if (Key == TEXT("trade.active"))
            return Campaign.WorldState.Trade.Contracts.ContainsByPredicate(
                [](const FDATradeContractState& Contract){ return !Contract.bCompleted; });
        if (Key == TEXT("trade.delivery_fulfilled"))
            return Campaign.WorldState.Trade.Contracts.ContainsByPredicate(
                [](const FDATradeContractState& Contract){ return Contract.SuccessfulDeliveryCount > 0; });
        return Campaign.HistoryTags.Contains(Key);
    }

    FName FoundryStageId(const FDARegionalCrisisCampaignState& State)
    {
        if (State.Resolution != EDAFoundryShortageResolution::None)
        {
            switch (State.Resolution)
            {
            case EDAFoundryShortageResolution::IndustrialSupport: return TEXT("industrial_support");
            case EDAFoundryShortageResolution::EdenRestriction: return TEXT("eden_restriction");
            case EDAFoundryShortageResolution::BrokeredCompact: return TEXT("brokered_compact");
            case EDAFoundryShortageResolution::MarketExploitation: return TEXT("market_exploitation");
            case EDAFoundryShortageResolution::Collapse: return TEXT("collapse");
            default: break;
            }
        }
        switch (State.FoundryStage)
        {
        case EDAFoundryShortageStage::ShortageWarning: return TEXT("shortage_warning");
        case EDAFoundryShortageStage::MarketSpike: return TEXT("market_spike");
        case EDAFoundryShortageStage::EcologicalDispute: return TEXT("ecological_dispute");
        case EDAFoundryShortageStage::EmergencyOverdrive: return TEXT("emergency_overdrive");
        default: return NAME_None;
        }
    }

    void AddOutcomeTag(FDACampaignSnapshot& Campaign, const FName Tag)
    {
        if (Tag.IsNone()) return;
        Campaign.HistoryTags.AddUnique(Tag);
        Campaign.HistoryTags.Sort([](const FName A, const FName B){ return A.LexicalLess(B); });
    }

    void AddAuthoredQuestOutcome(FDACampaignSnapshot& Campaign,
        const FDARegionalQuestEntry& Entry, const FName ChoiceId)
    {
        if (Entry.OutcomeTags.Num() == 1) AddOutcomeTag(Campaign, Entry.OutcomeTags[0]);
        AddOutcomeTag(Campaign, Entry.ChoiceOutcomeTags.FindRef(ChoiceId));
    }
}

bool FDARegionalCampaignCoordinatorRuntime::EvaluateEventTrigger(
    const FDARegionalWorldEventEntry& Entry, const FDACampaignSnapshot& Campaign)
{
    double Actual = 0.0;
    return TryResolveMetric(Entry.TriggerMetric, Campaign, Actual)
        && CompareMetric(Actual, Entry.TriggerComparison, Entry.TriggerThreshold);
}

bool FDARegionalCampaignCoordinatorRuntime::EvaluateQuestTrigger(
    const FDARegionalQuestEntry& Entry, const FDACampaignSnapshot& Campaign)
{
    TArray<FString> Predicates;
    Entry.Trigger.ParseIntoArray(Predicates, TEXT("|"), true);
    return Predicates.ContainsByPredicate([&Campaign](const FString& Predicate)
        { return EvaluateQuestPredicate(Predicate, Campaign); });
}

bool FDARegionalCampaignCoordinatorRuntime::ApplyEventResolutionEffects(
    const FDARegionalWorldEventEntry& Event,
    const FDARegionalCrisisResolutionDefinition& Resolution, const int64 WorldTick,
    FDACampaignSnapshot& Candidate, FString& OutError)
{
    const FDARegionalCrisisResolutionDefinition* Authored = Event.FindResolution(Resolution.ResolutionId);
    if (Authored == nullptr
        || Event.EventId == TEXT("event.foundry_shortage")
        || WorldTick != Candidate.WorldState.CurrentWorldTick)
    { OutError = TEXT("Regional event effects require one authored non-Foundry resolution at the canonical clock."); return false; }
    const FDARegionalCrisisResolutionDefinition& Effects = *Authored;
    FDACampaignSnapshot Transaction = Candidate;
    const FString MutationStem = Event.EventId.ToString() + TEXT(".")
        + Effects.ResolutionId.ToString() + TEXT(".")
        + FString::Printf(TEXT("%lld"), static_cast<long long>(WorldTick));
    if (Effects.TradeRouteCapacityDelta != 0)
    {
        FDATradeRouteState* Route = Transaction.WorldState.Trade.FindRoute(TEXT("route.eden_ironheart_relief"));
        if (Route == nullptr
            || (Effects.TradeRouteCapacityDelta < 0 && Route->CapacityPerWorldTick < -Effects.TradeRouteCapacityDelta)
            || (Effects.TradeRouteCapacityDelta > 0 && Route->CapacityPerWorldTick > MAX_int64 - Effects.TradeRouteCapacityDelta))
        { OutError = TEXT("Authored regional trade effect cannot be applied safely."); return false; }
        Route->CapacityPerWorldTick += Effects.TradeRouteCapacityDelta;
        if (Route->ReservedCapacityThisTick > Route->CapacityPerWorldTick)
        { OutError = TEXT("Authored regional trade effect would invalidate reserved capacity."); return false; }
    }
    if (Effects.MarketModifier != 0.0)
    {
        FDAMarketPriceModifierRecord& Market = Transaction.WorldState.Trade.MarketPriceModifiers.Emplace_GetRef();
        Market.MutationId = FName(*(TEXT("market.") + MutationStem));
        Market.GoodId = TEXT("good.machine_components"); Market.SourceEventId = Event.EventId;
        Market.Modifier = Effects.MarketModifier; Market.WorldTick = WorldTick;
    }
    if (Effects.EcologyDelta != 0.0
        && !Transaction.WorldState.Ecology.ApplyReason(FName(*(TEXT("ecology.") + MutationStem)),
            Effects.ResolutionId, Effects.EcologyDelta, WorldTick))
    { OutError = TEXT("Authored regional ecology effect was rejected."); return false; }
    if (Effects.RelationshipDelta != 0.f)
    {
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>();
        if (Diplomacy == nullptr || !Diplomacy->ApplyReason(Transaction, Effects.RelationshipId,
            Effects.RelationshipMetric, Effects.ResolutionId, Effects.RelationshipDelta,
            WorldTick, FName(*(TEXT("diplomacy.") + MutationStem))))
        { OutError = TEXT("Authored regional diplomacy effect was rejected."); return false; }
    }
    Transaction.WorldState.Forgeweave.ResourceHunger = FMath::Clamp(
        Transaction.WorldState.Forgeweave.ResourceHunger + Effects.ResourceHungerDelta, 0.f, 100.f);
    for (const FName Tag : Effects.HistoryTags) AddOutcomeTag(Transaction, Tag);
    for (const TPair<FName, FName>& Outcome : Effects.CitizenOutcomes)
        AddOutcomeTag(Transaction, Outcome.Value);
    FString ValidationError;
    if (!Transaction.Validate(ValidationError))
    { OutError = TEXT("Authored regional effects failed snapshot validation: ") + ValidationError; return false; }
    Candidate = MoveTemp(Transaction); OutError.Reset(); return true;
}

bool FDARegionalCampaignCoordinatorRuntime::ApplyFoundryQuestResolution(
    const FDARegionalCrisisManifest& Manifest, const FGuid& ActionId,
    const EDAFoundryShortageResolution Resolution, const int64 WorldTick,
    FDACampaignSnapshot& Candidate, FString& OutError)
{
    const FName ChoiceId = Resolution == EDAFoundryShortageResolution::IndustrialSupport ? FName(TEXT("industrial_support"))
        : Resolution == EDAFoundryShortageResolution::EdenRestriction ? FName(TEXT("eden_restriction"))
        : Resolution == EDAFoundryShortageResolution::BrokeredCompact ? FName(TEXT("brokered_compact"))
        : Resolution == EDAFoundryShortageResolution::MarketExploitation ? FName(TEXT("market_exploitation"))
        : NAME_None;
    const FDARegionalQuestEntry* Entry = Manifest.FindQuest(TEXT("quest.foundry_shortage"));
    if (!ActionId.IsValid() || ChoiceId.IsNone() || Entry == nullptr
        || Candidate.RegionalCrisis.Resolution != Resolution
        || WorldTick != Candidate.WorldState.CurrentWorldTick)
    {
        OutError = TEXT("Foundry quest completion requires the same resolved campaign candidate and canonical clock.");
        return false;
    }
    const FDAQuestDefinition Definition = MakeQuestDefinition(*Entry);
    EDAQuestRuntimeResult StartResult = FDAQuestRuntime::StartQuest(Definition, {}, WorldTick, Candidate);
    if (StartResult == EDAQuestRuntimeResult::Applied)
    {
        FDAQuestEvaluationContext Context; Context.WorldTick = WorldTick; FName Branch;
        if (FDAQuestRuntime::EvaluateCurrentNode(Definition, Context, Candidate, Branch)
            != EDAQuestRuntimeResult::Applied)
        { OutError = TEXT("Foundry quest could not enter its authored choice node."); return false; }
    }
    else if (StartResult != EDAQuestRuntimeResult::AlreadyApplied)
    { OutError = TEXT("Foundry quest could not start from its authored graph."); return false; }
    if (Candidate.NarrativeState.ActionRecords.ContainsByPredicate(
        [&ActionId](const FDANarrativeActionRecord& Record){ return Record.ActionId == ActionId; }))
    { OutError = TEXT("Foundry action ID already exists in narrative history."); return false; }
    FDANarrativeActionRecord Record;
    Record.ActionId = ActionId; Record.WorldTick = WorldTick;
    Record.NormalizedActionTags = {TEXT("regional.foundry_shortage"), ChoiceId};
    Record.NormalizedActionTags.Sort([](FName A, FName B){ return A.LexicalLess(B); });
    Candidate.NarrativeState.ActionRecords.Add(MoveTemp(Record));
    Candidate.NarrativeState.ActionRecords.Sort([](const FDANarrativeActionRecord& A, const FDANarrativeActionRecord& B)
        { return A.ActionId.ToString(EGuidFormats::Digits) < B.ActionId.ToString(EGuidFormats::Digits); });
    ++Candidate.NarrativeState.MutationRevision;
    FDAQuestEvaluationContext Context; Context.WorldTick = WorldTick;
    if (FDAQuestRuntime::SelectChoice(Definition, ChoiceId, Context, Candidate)
        != EDAQuestRuntimeResult::Applied)
    { OutError = TEXT("Foundry quest rejected the authored crisis resolution choice."); return false; }
    AddAuthoredQuestOutcome(Candidate, *Entry, ChoiceId);
    OutError.Reset();
    return true;
}

void UDARegionalCampaignCoordinatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ContentRegistry = Collection.InitializeDependency<UDARegionalCrisisContentRegistrySubsystem>();
    Collection.InitializeDependency<UDAWorldStateSubsystem>();
    if (UGameInstance* GameInstance = GetGameInstance())
        WorldStateSubsystem = GameInstance->GetSubsystem<UDAWorldStateSubsystem>();
    if (WorldStateSubsystem.IsValid())
    {
        WorldStateCommittedHandle = WorldStateSubsystem->OnWorldTickStateCommitted.AddUObject(
            this, &UDARegionalCampaignCoordinatorSubsystem::HandleWorldTickStateCommitted);
        SynchronizeNow();
    }
}

void UDARegionalCampaignCoordinatorSubsystem::Deinitialize()
{
    if (WorldStateSubsystem.IsValid() && WorldStateCommittedHandle.IsValid())
        WorldStateSubsystem->OnWorldTickStateCommitted.Remove(WorldStateCommittedHandle);
    WorldStateCommittedHandle.Reset(); WorldStateSubsystem.Reset(); ContentRegistry = nullptr;
    Super::Deinitialize();
}

bool UDARegionalCampaignCoordinatorSubsystem::SynchronizeNow()
{
    if (bSynchronizing || ContentRegistry == nullptr || !ContentRegistry->IsReady()
        || !WorldStateSubsystem.IsValid()) return false;
    TGuardValue<bool> Guard(bSynchronizing, true);
    const FDACampaignSnapshot& Authority = WorldStateSubsystem->GetPersistentCampaign();
    const int64 ExpectedNarrativeRevision = Authority.NarrativeState.MutationRevision;
    const int64 ExpectedSignalRevision = Authority.LiveSignals.MutationRevision;
    const int64 Tick = Authority.WorldState.CurrentWorldTick;
    FDACampaignSnapshot Candidate = Authority;
    const FDARegionalCrisisManifest& Manifest = ContentRegistry->GetManifest();
    for (const FDARegionalWorldEventEntry& Entry : Manifest.Events)
    {
        const FDAWorldEventDefinition Definition = MakeEventDefinition(Entry);
        if (!FDARegionalCampaignCoordinatorRuntime::EvaluateEventTrigger(Entry, Candidate)
            && Candidate.NarrativeState.FindEventState(Entry.EventId) == nullptr) continue;
        FDAWorldEventRuntime::StartEvent(Definition, Tick, Candidate);
        if (Entry.EventId == TEXT("event.foundry_shortage"))
        {
            const FName Target = FoundryStageId(Candidate.RegionalCrisis);
            const FDAWorldEventSaveState* State = Candidate.NarrativeState.FindEventState(Entry.EventId);
            while (State != nullptr && !Target.IsNone() && State->CurrentStageId != Target
                && State->ProgressState == EDAWorldEventProgressState::Active)
            {
                FName Next = Target;
                const FDAWorldEventStageDefinition* Current = Definition.FindStage(State->CurrentStageId);
                const int32 TargetIndex = Entry.Stages.IndexOfByPredicate(
                    [Target](const FDARegionalCrisisStageDefinition& Stage){ return Stage.StageId == Target; });
                const int32 CurrentIndex = Entry.Stages.IndexOfByPredicate(
                    [State](const FDARegionalCrisisStageDefinition& Stage){ return Stage.StageId == State->CurrentStageId; });
                if (TargetIndex > CurrentIndex && Entry.Stages.IsValidIndex(CurrentIndex + 1))
                    Next = Entry.Stages[CurrentIndex + 1].StageId;
                if (Current == nullptr || !Current->AllowedNextStageIds.Contains(Next)
                    || FDAWorldEventRuntime::AdvanceEvent(Definition, Next, Tick, Candidate)
                        != EDAWorldEventRuntimeResult::Applied) break;
                State = Candidate.NarrativeState.FindEventState(Entry.EventId);
            }
        }
        else
        {
            const FDAWorldEventSaveState* State = Candidate.NarrativeState.FindEventState(Entry.EventId);
            const FDARegionalCrisisStageDefinition* AuthoredStage = State == nullptr ? nullptr
                : Entry.Stages.FindByPredicate([State](const FDARegionalCrisisStageDefinition& Stage)
                    { return Stage.StageId == State->CurrentStageId; });
            if (State != nullptr && AuthoredStage != nullptr
                && State->ProgressState == EDAWorldEventProgressState::Active
                && Tick - State->LastTransitionWorldTick >= AuthoredStage->DurationWorldTicks)
            {
                FName NextStage = Entry.IgnoredResolutionId;
                for (const FName CandidateNext : AuthoredStage->NextStageIds)
                    if (Entry.Stages.ContainsByPredicate([CandidateNext](const FDARegionalCrisisStageDefinition& Stage)
                        { return Stage.StageId == CandidateNext; })) { NextStage = CandidateNext; break; }
                FDACampaignSnapshot Transition = Candidate;
                if (FDAWorldEventRuntime::AdvanceEvent(Definition, NextStage, Tick, Transition)
                    == EDAWorldEventRuntimeResult::Applied)
                {
                    if (NextStage != Entry.IgnoredResolutionId)
                        Candidate = MoveTemp(Transition);
                    else if (const FDARegionalCrisisResolutionDefinition* Resolution = Entry.FindResolution(NextStage))
                    {
                        FString ResolutionError;
                        if (!FDARegionalCampaignCoordinatorRuntime::ApplyEventResolutionEffects(
                            Entry, *Resolution, Tick, Transition, ResolutionError)) return false;
                        Candidate = MoveTemp(Transition);
                    }
                }
            }
        }
    }
    for (const FDARegionalQuestEntry& Entry : Manifest.Quests)
    {
        const FDARegionalWorldEventEntry* TriggerEvent =
            Entry.Trigger.StartsWith(TEXT("event.")) && !Entry.Trigger.Contains(TEXT("|"))
                ? Manifest.FindEvent(FName(*Entry.Trigger)) : nullptr;
        const FDAWorldEventSaveState* TriggerEventState = TriggerEvent == nullptr ? nullptr
            : Candidate.NarrativeState.FindEventState(TriggerEvent->EventId);
        if (TriggerEvent != nullptr && TriggerEventState != nullptr
            && TriggerEventState->ProgressState == EDAWorldEventProgressState::Resolved
            && TriggerEventState->CurrentStageId == TriggerEvent->IgnoredResolutionId)
        {
            const FDAQuestDefinition Definition = MakeQuestDefinition(Entry);
            const EDAQuestRuntimeResult Started = FDAQuestRuntime::StartQuest(Definition, {}, Tick, Candidate);
            if (Started == EDAQuestRuntimeResult::Applied)
            {
                FDAQuestEvaluationContext Context; Context.WorldTick = Tick; FName Branch;
                if (FDAQuestRuntime::EvaluateCurrentNode(Definition, Context, Candidate, Branch)
                    != EDAQuestRuntimeResult::Applied) return false;
            }
            else if (Started != EDAQuestRuntimeResult::AlreadyApplied) return false;
            FDAQuestSaveState* QuestState = Candidate.NarrativeState.FindQuestState(Entry.QuestId);
            if (QuestState == nullptr) return false;
            if (QuestState->ProgressState == EDAQuestProgressState::Active)
            {
                if (FDAQuestRuntime::AbandonQuest(Definition, Candidate)
                    != EDAQuestRuntimeResult::Applied) return false;
            }
            continue;
        }
        if (!FDARegionalCampaignCoordinatorRuntime::EvaluateQuestTrigger(Entry, Candidate)
            && Candidate.NarrativeState.FindQuestState(Entry.QuestId) == nullptr) continue;
        const FDAQuestDefinition Definition = MakeQuestDefinition(Entry);
        const EDAQuestRuntimeResult Started = FDAQuestRuntime::StartQuest(Definition, {}, Tick, Candidate);
        if (Started == EDAQuestRuntimeResult::Applied)
        {
            FDAQuestEvaluationContext Context; Context.WorldTick = Tick; FName Branch;
            FDAQuestRuntime::EvaluateCurrentNode(Definition, Context, Candidate, Branch);
        }
    }
    if (Candidate.NarrativeState.MutationRevision == ExpectedNarrativeRevision) return true;
    return WorldStateSubsystem->TryCommitPersistentCampaign(
        Candidate, ExpectedNarrativeRevision, ExpectedSignalRevision, Tick);
}

EDAQuestRuntimeResult UDARegionalCampaignCoordinatorSubsystem::SubmitQuestChoice(
    const FName QuestId, const FName ChoiceId)
{
    if (bSynchronizing || ContentRegistry == nullptr || !ContentRegistry->IsReady()
        || !WorldStateSubsystem.IsValid()) return EDAQuestRuntimeResult::InvalidState;
    const FDARegionalQuestEntry* Entry = ContentRegistry->GetManifest().FindQuest(QuestId);
    if (Entry == nullptr || !Entry->Choices.Contains(ChoiceId)) return EDAQuestRuntimeResult::InvalidDefinition;
    if (QuestId == TEXT("quest.foundry_shortage")) return EDAQuestRuntimeResult::InvalidState;
    const FDACampaignSnapshot& Authority = WorldStateSubsystem->GetPersistentCampaign();
    FDACampaignSnapshot Candidate = Authority;
    FDAQuestEvaluationContext Context; Context.WorldTick = Authority.WorldState.CurrentWorldTick;
    const FDARegionalCrisisManifest& Manifest = ContentRegistry->GetManifest();
    const FDAQuestSaveState* QuestState = Candidate.NarrativeState.FindQuestState(QuestId);
    const FDAQuestNodeDefinition* CurrentNode = QuestState == nullptr ? nullptr
        : QuestState->DefinitionManifest.FindNode(QuestState->CurrentNodeId);
    if (QuestState == nullptr) return EDAQuestRuntimeResult::NotFound;
    if (QuestState->ProgressState != EDAQuestProgressState::Active || CurrentNode == nullptr
        || CurrentNode->Type != EDAQuestNodeType::Choice) return EDAQuestRuntimeResult::InvalidState;
    if (Entry->Trigger.StartsWith(TEXT("event.")) && !Entry->Trigger.Contains(TEXT("|")))
    {
        const FDARegionalWorldEventEntry* Event = Manifest.FindEvent(FName(*Entry->Trigger));
        const FName OutcomeTag = Entry->ChoiceOutcomeTags.FindRef(ChoiceId);
        const FDARegionalCrisisResolutionDefinition* Resolution = Event == nullptr ? nullptr
            : Event->Resolutions.FindByPredicate([ChoiceId, OutcomeTag](const FDARegionalCrisisResolutionDefinition& Row)
                { return Row.ResolutionId == ChoiceId || Row.HistoryTags.Contains(OutcomeTag); });
        if (Event == nullptr || Resolution == nullptr || Event->EventId == TEXT("event.foundry_shortage"))
            return EDAQuestRuntimeResult::InvalidDefinition;
        if (FDAWorldEventRuntime::AdvanceEvent(MakeEventDefinition(*Event), Resolution->ResolutionId,
            Context.WorldTick, Candidate) != EDAWorldEventRuntimeResult::Applied)
            return EDAQuestRuntimeResult::InvalidState;
        FString ResolutionError;
        if (!FDARegionalCampaignCoordinatorRuntime::ApplyEventResolutionEffects(
            *Event, *Resolution, Context.WorldTick, Candidate, ResolutionError))
            return EDAQuestRuntimeResult::InvalidState;
    }
    const EDAQuestRuntimeResult Result = FDAQuestRuntime::SelectChoice(
        MakeQuestDefinition(*Entry), ChoiceId, Context, Candidate);
    if (Result != EDAQuestRuntimeResult::Applied) return Result;
    AddAuthoredQuestOutcome(Candidate, *Entry, ChoiceId);
    return WorldStateSubsystem->TryCommitPersistentCampaign(Candidate,
        Authority.NarrativeState.MutationRevision, Authority.LiveSignals.MutationRevision,
        Authority.WorldState.CurrentWorldTick) ? Result : EDAQuestRuntimeResult::InvalidState;
}

void UDARegionalCampaignCoordinatorSubsystem::HandleWorldTickStateCommitted(
    const FDACommittedCampaignSnapshot CommittedState)
{
    if (!bSynchronizing && WorldStateSubsystem.IsValid()
        && CommittedState->WorldState.CurrentWorldTick == WorldStateSubsystem->GetCurrentWorldTick())
        SynchronizeNow();
}
