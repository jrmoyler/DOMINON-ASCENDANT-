#include "Content/DARegionalCrisisContent.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Narrative/DAFoundryShortageRuntime.h"
#include "Narrative/DARegionalCampaignCoordinatorSubsystem.h"
#include "Narrative/DARegionalDialogueConditionSubsystem.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Save/DASaveService.h"

BEGIN_DEFINE_SPEC(FDARegionalCrisisSpec, "Dominion.Content.RegionalCrisis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDARegionalCrisisSpec)

void FDARegionalCrisisSpec::Define()
{
    It("strictly parses the exact authored event and quest authority", [this]()
    {
        FDARegionalCrisisManifest Manifest;
        TArray<FText> Errors;
        TestTrue("Canonical manifest loads", FDARegionalCrisisPipeline::LoadCanonical(Manifest, Errors));
        TestEqual("Exactly six world events", Manifest.Events.Num(), 6);
        TestEqual("Exactly ten regional quests", Manifest.Quests.Num(), 10);
        TestEqual("Foundry Shortage is the first frozen event", Manifest.Events[0].EventId,
            FName(TEXT("event.foundry_shortage")));
        TestEqual("Resource Hunger trigger is strictly greater than 70",
            Manifest.Events[0].TriggerThreshold, 70.0);
        TestEqual("Warning timing is the explicit two World Ticks", Manifest.Events[0].WarningDurationWorldTicks, 2);
        TestEqual("Initial price modifier is +20%", Manifest.Events[0].Stages[0].PriceModifier, 0.20);
        TestEqual("Market spike is +35%", Manifest.Events[0].Stages[1].PriceModifier, 0.35);
        TestEqual("Emergency escalation is +60%", Manifest.Events[0].Stages[3].PriceModifier, 0.60);

        FString CanonicalJson;
        TestTrue("Canonical JSON can be read", FFileHelper::LoadFileToString(
            CanonicalJson, *FDARegionalCrisisPipeline::GetCanonicalManifestPath()));
        CanonicalJson.ReplaceInline(TEXT("\"campaignId\":"), TEXT("\"unknownField\":true,\"campaignId\":"));
        FDARegionalCrisisManifest Rejected;
        TArray<FText> StrictErrors;
        TestFalse("Unknown fields fail closed", FDARegionalCrisisPipeline::ParseJson(
            CanonicalJson, Rejected, StrictErrors));
        FDARegionalCrisisManifest MutatedSystems = Manifest;
        MutatedSystems.Events[1].Systems.Add(TEXT("foreign.system"));
        TestFalse("Frozen semantic validation rejects event-system mutation despite supplied fingerprints",
            FDARegionalCrisisPipeline::Validate(MutatedSystems, StrictErrors));
        FDARegionalCrisisManifest MutatedGraph = Manifest;
        MutatedGraph.Quests[4].Nodes[1].Edges[0].Target = TEXT("resolution.foreign");
        TestFalse("Frozen semantic validation rejects quest-edge mutation despite supplied fingerprints",
            FDARegionalCrisisPipeline::Validate(MutatedGraph, StrictErrors));
    });

    It("keeps generated cache and canonical runtime fallback in full semantic parity", [this]()
    {
        FDARegionalCrisisManifest Manifest;
        TArray<FText> Errors;
        TestTrue("Canonical manifest loads", FDARegionalCrisisPipeline::LoadCanonical(Manifest, Errors));
        TArray<UDARegionalWorldEventDefinition*> Events;
        TArray<UDARegionalQuestDefinition*> Quests;
        TestTrue("Transient runtime fallback builds", FDARegionalCrisisPipeline::BuildAssets(
            Manifest, Events, Quests, Errors));
        TestEqual("Fallback builds all events", Events.Num(), 6);
        TestEqual("Fallback builds all quests", Quests.Num(), 10);
        TestFalse("Transient fallback is never accepted as generated cache",
            FDARegionalCrisisPipeline::ValidateGeneratedCache(Manifest, Events, Quests, Errors));
        for (int32 Index = 0; Index < Events.Num(); ++Index)
        {
            TestEqual("Event semantic projection matches manifest", Events[Index]->Event.EventId,
                Manifest.Events[Index].EventId);
            TestEqual("Event fallback carries source fingerprint", Events[Index]->SourceFingerprint,
                Manifest.Fingerprint);
        }
    });

    It("evaluates event and quest triggers from authored trigger fields instead of definition IDs", [this]()
    {
        FDARegionalCrisisManifest Manifest;
        TArray<FText> Errors;
        TestTrue("Canonical manifest loads", FDARegionalCrisisPipeline::LoadCanonical(Manifest, Errors));
        FDACampaignSnapshot Campaign;
        Campaign.WorldState.Forgeweave.ResourceHunger = 71.f;
        FDARegionalWorldEventEntry Event = Manifest.Events[0];
        Event.EventId = TEXT("event.test_manifest_driven");
        TestTrue("A renamed event still follows its authored metric/comparison/threshold",
            FDARegionalCampaignCoordinatorRuntime::EvaluateEventTrigger(Event, Campaign));
        Event.TriggerThreshold = 71.0;
        TestFalse("Strict greater-than comparison rejects equality",
            FDARegionalCampaignCoordinatorRuntime::EvaluateEventTrigger(Event, Campaign));

        FDARegionalQuestEntry Quest = Manifest.Quests[6];
        Quest.QuestId = TEXT("quest.test_manifest_driven");
        Quest.Trigger = TEXT("forgeweave.resource_hunger>70|automation_incident");
        TestTrue("A renamed quest evaluates its authored OR expression",
            FDARegionalCampaignCoordinatorRuntime::EvaluateQuestTrigger(Quest, Campaign));
        Campaign.WorldState.Forgeweave.ResourceHunger = 70.f;
        TestFalse("Authored quest strict comparison rejects equality",
            FDARegionalCampaignCoordinatorRuntime::EvaluateQuestTrigger(Quest, Campaign));
        Campaign.HistoryTags.Add(TEXT("automation_incident"));
        TestTrue("Authored quest history predicate satisfies the alternate branch",
            FDARegionalCampaignCoordinatorRuntime::EvaluateQuestTrigger(Quest, Campaign));
    });

    It("applies every authored ignored-event authority in the same event transaction", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("World authority exists", World);
        if (World == nullptr) return;
        FDARegionalCrisisManifest Manifest;
        TArray<FText> Errors;
        TestTrue("Canonical manifest loads", FDARegionalCrisisPipeline::LoadCanonical(Manifest, Errors));
        const FDARegionalWorldEventEntry* Grid = Manifest.FindEvent(TEXT("event.grid_strain"));
        TestNotNull("Grid Strain definition exists", Grid);
        if (Grid == nullptr) return;
        const FDARegionalCrisisResolutionDefinition* Blackout = Grid->FindResolution(TEXT("blackout"));
        TestNotNull("Blackout resolution exists", Blackout);
        if (Blackout == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        const FDATradeRouteState* BeforeRoute =
            Candidate.WorldState.Trade.FindRoute(TEXT("route.eden_ironheart_relief"));
        TestNotNull("Canonical relief route exists", BeforeRoute);
        if (BeforeRoute == nullptr) return;
        const int64 CapacityBefore = BeforeRoute->CapacityPerWorldTick;
        const float HungerBefore = Candidate.WorldState.Forgeweave.ResourceHunger;
        FString Error;
        TestTrue("Manifest-authored blackout effects apply atomically",
            FDARegionalCampaignCoordinatorRuntime::ApplyEventResolutionEffects(
                *Grid, *Blackout, Candidate.WorldState.CurrentWorldTick, Candidate, Error));
        const FDATradeRouteState* AfterRoute =
            Candidate.WorldState.Trade.FindRoute(TEXT("route.eden_ironheart_relief"));
        const FDADiplomaticRelationship* Eden =
            Candidate.WorldState.Diplomacy.FindRelationship(TEXT("relationship.synara.eden"));
        TestTrue("Trade capacity consumes the authored delta", AfterRoute != nullptr
            && AfterRoute->CapacityPerWorldTick == CapacityBefore - 10);
        TestEqual("Ecology consumes the authored delta", Candidate.WorldState.Ecology.Balance, 97.0);
        TestEqual("Resource Hunger consumes the authored delta",
            Candidate.WorldState.Forgeweave.ResourceHunger, HungerBefore + 5.f);
        TestTrue("Diplomacy consumes the authored relationship reason", Eden != nullptr
            && Eden->Grievance == 6.f && Eden->ReasonLedger.Num() == 1);
        TestTrue("History consumes the authored outcome tag", Candidate.HistoryTags.Contains(TEXT("grid_blackout")));
        TestTrue("The complete effect projection remains snapshot-valid", Candidate.Validate(Error));
    });

    It("runs a non-Foundry authored trigger graph through its ignored resolution lifecycle", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDARegionalCampaignCoordinatorSubsystem* Coordinator =
            Fixture.GetSubsystem<UDARegionalCampaignCoordinatorSubsystem>();
        if (World == nullptr || Coordinator == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        FDACampaignUtilitySignal Power;
        Power.WorldAssetId = FGuid(22, 22, 3, 1);
        Power.Utility = EDACampaignUtilityKind::Power;
        Power.Supply = EDACampaignUtilitySupply::Critical;
        Candidate.LiveSignals.UtilitySignals.Add(Power);
        TestTrue("Critical reserve fixture restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Manifest trigger starts Grid Strain", Coordinator->SynchronizeNow());
        const FDAWorldEventSaveState* Started =
            World->GetPersistentCampaign().NarrativeState.FindEventState(TEXT("event.grid_strain"));
        TestTrue("Authored initial stage persists", Started != nullptr
            && Started->CurrentStageId == TEXT("grid_warning"));
        TestTrue("Authored warning duration advances", World->AdvanceWorldTicks(3));
        const FDAWorldEventSaveState* Brownouts =
            World->GetPersistentCampaign().NarrativeState.FindEventState(TEXT("event.grid_strain"));
        TestTrue("Authored next-stage edge persists", Brownouts != nullptr
            && Brownouts->CurrentStageId == TEXT("brownouts"));
        TestTrue("Authored ignored duration resolves", World->AdvanceWorldTicks(2));
        const FDACampaignSnapshot& Resolved = World->GetPersistentCampaign();
        const FDAWorldEventSaveState* Blackout =
            Resolved.NarrativeState.FindEventState(TEXT("event.grid_strain"));
        const FDATradeRouteState* Route =
            Resolved.WorldState.Trade.FindRoute(TEXT("route.eden_ironheart_relief"));
        TestTrue("Ignored resolution is durable", Blackout != nullptr
            && Blackout->ProgressState == EDAWorldEventProgressState::Resolved
            && Blackout->CurrentStageId == TEXT("blackout"));
        TestTrue("Resolved lifecycle includes the authored real effects", Route != nullptr
            && Route->CapacityPerWorldTick == 0
            && Resolved.WorldState.Ecology.Balance == 97.0
            && Resolved.HistoryTags.Contains(TEXT("grid_blackout")));
    });

    It("terminally abandons an event-backed quest at its authored ignored resolution", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDARegionalCampaignCoordinatorSubsystem* Coordinator =
            Fixture.GetSubsystem<UDARegionalCampaignCoordinatorSubsystem>();
        if (World == nullptr || Coordinator == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        TestTrue("Ecology trigger reason applies", Candidate.WorldState.Ecology.ApplyReason(
            TEXT("ecology.green_line.ignored_trigger"), TEXT("automation"), -31.0,
            Candidate.WorldState.CurrentWorldTick));
        TestTrue("Green Line trigger fixture restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Green Line event and quest synchronize", Coordinator->SynchronizeNow());
        const FDAQuestSaveState* ActiveQuest =
            World->GetPersistentCampaign().NarrativeState.FindQuestState(TEXT("quest.green_line"));
        TestTrue("Event-backed quest begins at its authored choice", ActiveQuest != nullptr
            && ActiveQuest->ProgressState == EDAQuestProgressState::Active
            && ActiveQuest->CurrentNodeId == TEXT("choice"));

        TestTrue("Authored ignored timeline commits", World->AdvanceWorldTicks(4));
        const FDACampaignSnapshot& Ignored = World->GetPersistentCampaign();
        const FDAWorldEventSaveState* IgnoredEvent =
            Ignored.NarrativeState.FindEventState(TEXT("event.green_line"));
        const FDAQuestSaveState* IgnoredQuest =
            Ignored.NarrativeState.FindQuestState(TEXT("quest.green_line"));
        TestTrue("Manifest ignored resolution remains the terminal event authority", IgnoredEvent != nullptr
            && IgnoredEvent->ProgressState == EDAWorldEventProgressState::Resolved
            && IgnoredEvent->CurrentStageId == TEXT("boundary_failure"));
        TestTrue("Matching quest is terminal without inventing an ignored choice", IgnoredQuest != nullptr
            && IgnoredQuest->ProgressState == EDAQuestProgressState::Abandoned
            && IgnoredQuest->CurrentNodeId == TEXT("choice"));
        const int64 ResolutionRevision = Ignored.NarrativeState.MutationRevision;
        const double IgnoredEcology = Ignored.WorldState.Ecology.Balance;
        TestEqual("A player choice cannot reopen the terminal event", Coordinator->SubmitQuestChoice(
            TEXT("quest.green_line"), TEXT("full_boundary")), EDAQuestRuntimeResult::InvalidState);
        TestTrue("Repeated synchronization is idempotent", Coordinator->SynchronizeNow());
        const FDACampaignSnapshot& Stable = World->GetPersistentCampaign();
        const FDAWorldEventSaveState* StableEvent =
            Stable.NarrativeState.FindEventState(TEXT("event.green_line"));
        TestTrue("Terminal event is not reopened or duplicated", StableEvent != nullptr
            && StableEvent->ProgressState == EDAWorldEventProgressState::Resolved
            && StableEvent->CurrentStageId == TEXT("boundary_failure")
            && Stable.NarrativeState.MutationRevision == ResolutionRevision);
        TestEqual("Ignored event effects remain exactly once", Stable.WorldState.Ecology.Balance, IgnoredEcology);
    });

    It("resolves an event-triggered quest through the matching authored event resolution", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDARegionalCampaignCoordinatorSubsystem* Coordinator =
            Fixture.GetSubsystem<UDARegionalCampaignCoordinatorSubsystem>();
        if (World == nullptr || Coordinator == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        TestTrue("Ecology trigger reason applies", Candidate.WorldState.Ecology.ApplyReason(
            TEXT("ecology.green_line.trigger"), TEXT("automation"), -31.0,
            Candidate.WorldState.CurrentWorldTick));
        TestTrue("Green Line trigger fixture restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Green Line event and quest synchronize", Coordinator->SynchronizeNow());
        TestEqual("Authored quest choice resolves", Coordinator->SubmitQuestChoice(
            TEXT("quest.green_line"), TEXT("full_boundary")), EDAQuestRuntimeResult::Applied);
        const FDACampaignSnapshot& Resolved = World->GetPersistentCampaign();
        const FDAWorldEventSaveState* Event =
            Resolved.NarrativeState.FindEventState(TEXT("event.green_line"));
        const FDAQuestSaveState* Quest =
            Resolved.NarrativeState.FindQuestState(TEXT("quest.green_line"));
        const FDATradeRouteState* Route =
            Resolved.WorldState.Trade.FindRoute(TEXT("route.eden_ironheart_relief"));
        TestTrue("Event graph reaches the matching authored resolution", Event != nullptr
            && Event->ProgressState == EDAWorldEventProgressState::Resolved
            && Event->CurrentStageId == TEXT("full_boundary"));
        TestTrue("Quest graph reaches its authored resolution", Quest != nullptr
            && Quest->ProgressState == EDAQuestProgressState::Completed
            && Quest->CurrentNodeId == TEXT("resolution.full_boundary"));
        TestTrue("Quest and event share the complete authored authority effects", Route != nullptr
            && Route->CapacityPerWorldTick == 5
            && Resolved.WorldState.Ecology.Balance == 79.0
            && Resolved.HistoryTags.Contains(TEXT("green_line_full_boundary")));
    });

    It("warns once above Resource Hunger 70 and escalates market authority by committed World Tick", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("World authority exists", World);
        if (World == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.WorldState.Forgeweave.ResourceHunger = 71.f;
        TestTrue("Authored trigger fixture restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Trigger tick commits", World->AdvanceWorldTicks(1));
        TestEqual("Warning stage is persisted", World->GetPersistentCampaign().RegionalCrisis.FoundryStage,
            EDAFoundryShortageStage::ShortageWarning);
        TestEqual("Warning is emitted exactly once", World->GetPersistentCampaign().RegionalCrisis.WarningEmissionCount, 1);
        TestEqual("Machine-components price modifier is +20%",
            World->GetPersistentCampaign().WorldState.Trade.GetMarketPriceModifier(
                TEXT("good.machine_components")), 0.20);
        TestTrue("Second warning World Tick commits", World->AdvanceWorldTicks(1));
        TestEqual("No duplicate warning is emitted", World->GetPersistentCampaign().RegionalCrisis.WarningEmissionCount, 1);
        TestTrue("Market-spike tick commits", World->AdvanceWorldTicks(1));
        TestEqual("Market-spike modifier is +35%", World->GetPersistentCampaign().WorldState.Trade.GetMarketPriceModifier(
            TEXT("good.machine_components")), 0.35);
        TestTrue("Ecological-dispute tick commits", World->AdvanceWorldTicks(1));
        TestTrue("Emergency-overdrive tick commits", World->AdvanceWorldTicks(1));
        TestEqual("Emergency modifier is +60%", World->GetPersistentCampaign().WorldState.Trade.GetMarketPriceModifier(
            TEXT("good.machine_components")), 0.60);
        TestEqual("Committed clock owns the event stage tick",
            World->GetPersistentCampaign().RegionalCrisis.LastTransitionWorldTick,
            World->GetPersistentCampaign().WorldState.CurrentWorldTick);
    });

    It("persists and reloads the ignored escalation and collapse resolution", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDARegionalCampaignCoordinatorSubsystem* Coordinator =
            Fixture.GetSubsystem<UDARegionalCampaignCoordinatorSubsystem>();
        if (World == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        TestTrue("Trigger fixture restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Ignored event timeline commits", World->AdvanceWorldTicks(7));
        TestEqual("Ignored event resolves as collapse", World->GetPersistentCampaign().RegionalCrisis.Resolution,
            EDAFoundryShortageResolution::Collapse);
        TestEqual("Ignored path remains exactly-once", World->GetPersistentCampaign().RegionalCrisis.WarningEmissionCount, 1);
        if (Coordinator != nullptr) TestTrue("Ignored resolution synchronizes through Task18", Coordinator->SynchronizeNow());
        const FDAQuestSaveState* IgnoredQuest =
            World->GetPersistentCampaign().NarrativeState.FindQuestState(TEXT("quest.foundry_shortage"));
        TestTrue("Ignored collapse terminally abandons its player-choice quest", IgnoredQuest != nullptr
            && IgnoredQuest->ProgressState == EDAQuestProgressState::Abandoned);

        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("RegionalCrisisSaveTests"),
            FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory);
        TestTrue("Escalated campaign saves", Saves.SaveCampaign(World->GetPersistentCampaign(), TEXT("ignored")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = Saves.LoadCampaign(TEXT("ignored"));
        TestTrue("Escalated campaign reloads", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            TestEqual("Resolution survives reload", Loaded.GetValue().RegionalCrisis.Resolution,
                EDAFoundryShortageResolution::Collapse);
            TestEqual("Warning count survives reload", Loaded.GetValue().RegionalCrisis.WarningEmissionCount, 1);
        }
        IFileManager::Get().DeleteDirectory(*Directory, false, true);
    });

    It("keeps an intrinsic historical resolution audit valid while live authorities evolve", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        if (World == nullptr) return;
        FDACampaignSnapshot Trigger = World->GetPersistentCampaign();
        Trigger.WorldState.Forgeweave.ResourceHunger = 80.f;
        TestTrue("Trigger fixture restores", World->RestorePersistentCampaign(Trigger));
        TestTrue("Warning commits", World->AdvanceWorldTicks(1));
        TestEqual("Resolution commits", World->ResolveFoundryShortage(
            FGuid(22, 44, 1, 9), EDAFoundryShortageResolution::IndustrialSupport),
            EDAFoundryShortageActionResult::Applied);
        const FDAFoundryShortageResolutionRecord Historical =
            World->GetPersistentCampaign().RegionalCrisis.ResolutionRecords[0];
        TestTrue("Natural World Ticks pass the authored recovery window", World->AdvanceWorldTicks(7));
        const FDACampaignSnapshot& Evolved = World->GetPersistentCampaign();
        TestTrue("Resource Hunger can evolve after its historical after-value",
            !FMath::IsNearlyEqual(Evolved.WorldState.Forgeweave.ResourceHunger,
                Historical.ResourceHungerAfter));
        FString ValidationError;
        TestTrue("Historical audit remains intrinsically valid", Evolved.Validate(ValidationError));

        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
            TEXT("RegionalCrisisEvolution"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory);
        TestTrue("Evolved campaign saves", Saves.SaveCampaign(Evolved, TEXT("evolved")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = Saves.LoadCampaign(TEXT("evolved"));
        TestTrue("Evolved campaign reloads", Loaded.HasValue());
        if (Loaded.HasValue()) TestEqual("Historical action identity survives reload",
            Loaded.GetValue().RegionalCrisis.ResolutionRecords[0].ActionId, Historical.ActionId);
        IFileManager::Get().DeleteDirectory(*Directory, false, true);
    });

    It("commits four authored resolutions atomically without fabricated quest currency", [this]()
    {
        struct FExpected
        {
            EDAFoundryShortageResolution Resolution;
            int64 TradeCapacity;
            double Ecology;
            float ResourceHungerDelta;
        };
        const TArray<FExpected> Cases = {
            {EDAFoundryShortageResolution::IndustrialSupport, 30, 92.0, -30.f},
            {EDAFoundryShortageResolution::EdenRestriction, 0, 100.0, -10.f},
            {EDAFoundryShortageResolution::BrokeredCompact, 20, 100.0, -20.f},
            {EDAFoundryShortageResolution::MarketExploitation, 35, 85.0, 5.f},
        };
        for (int32 Index = 0; Index < Cases.Num(); ++Index)
        {
            FDAGameInstanceSubsystemFixture Fixture;
            UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
            if (World == nullptr) continue;
            FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
            Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
            Candidate.LiveSignals.Capital = 1234.0;
            Candidate.LiveSignals.Insight = 234.0;
            Candidate.LiveSignals.Influence = 34.0;
            TestTrue("Resolution fixture restores", World->RestorePersistentCampaign(Candidate));
            TestTrue("Foundry warning commits", World->AdvanceWorldTicks(1));
            const double CapitalBefore = World->GetPersistentCampaign().LiveSignals.Capital;
            const double InsightBefore = World->GetPersistentCampaign().LiveSignals.Insight;
            const double InfluenceBefore = World->GetPersistentCampaign().LiveSignals.Influence;
            const float HungerBefore = World->GetPersistentCampaign().WorldState.Forgeweave.ResourceHunger;
            const FGuid ActionId(22, Index + 1, 2200 + Index, 1);
            TestEqual("First resolution applies", World->ResolveFoundryShortage(ActionId, Cases[Index].Resolution),
                EDAFoundryShortageActionResult::Applied);
            const FDACampaignSnapshot& After = World->GetPersistentCampaign();
            const FDAQuestSaveState* FoundryQuest =
                After.NarrativeState.FindQuestState(TEXT("quest.foundry_shortage"));
            TestTrue("The same committed snapshot completes the authored Task18 quest", FoundryQuest != nullptr
                && FoundryQuest->ProgressState == EDAQuestProgressState::Completed);
            TestTrue("The crisis action ID is the narrative action ID in that same snapshot",
                After.NarrativeState.ActionRecords.ContainsByPredicate([ActionId](const FDANarrativeActionRecord& Record)
                    { return Record.ActionId == ActionId; }));
            const FDATradeRouteState* Route = After.WorldState.Trade.FindRoute(TEXT("route.eden_ironheart_relief"));
            TestTrue("Trade route receives exact authored effect", Route != nullptr
                && Route->CapacityPerWorldTick == Cases[Index].TradeCapacity);
            TestEqual("Ecology receives exact authored effect", After.WorldState.Ecology.Balance, Cases[Index].Ecology);
            TestEqual("Resource Hunger receives exact authored effect",
                After.WorldState.Forgeweave.ResourceHunger,
                FMath::Clamp(HungerBefore + Cases[Index].ResourceHungerDelta, 0.f, 100.f));
            TestEqual("Capital is not a fake quest reward", After.LiveSignals.Capital, CapitalBefore);
            TestEqual("Insight is not a fake quest reward", After.LiveSignals.Insight, InsightBefore);
            TestEqual("Influence is not a fake quest reward", After.LiveSignals.Influence, InfluenceBefore);
            TestEqual("Exact replay is idempotent", World->ResolveFoundryShortage(ActionId, Cases[Index].Resolution),
                EDAFoundryShortageActionResult::AlreadyApplied);
            TestEqual("Reused action ID with different intent fails closed", World->ResolveFoundryShortage(
                ActionId, EDAFoundryShortageResolution::Collapse), EDAFoundryShortageActionResult::Rejected);
        }
    });

    It("persists Tal Mara and Ori outcomes for later Daxton and Amara dialogue", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        if (World == nullptr) return;
        const FDACampaignSnapshot& Initial = World->GetPersistentCampaign();
        TestNotNull("Tal has a canonical citizen signal", Initial.LiveSignals.FindCitizen(TEXT("citizen.neutral.tal_arden")));
        TestNotNull("Mara has a canonical citizen signal", Initial.LiveSignals.FindCitizen(TEXT("citizen.forgeweave.mara_kest")));
        TestNotNull("Ori has a canonical citizen signal", Initial.LiveSignals.FindCitizen(TEXT("citizen.eden.ori_sen")));
        FDACampaignSnapshot Candidate = Initial;
        Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        TestTrue("Story fixture restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Warning commits", World->AdvanceWorldTicks(1));
        TestEqual("Brokered resolution applies", World->ResolveFoundryShortage(
            FGuid(22, 9, 9, 1), EDAFoundryShortageResolution::BrokeredCompact),
            EDAFoundryShortageActionResult::Applied);
        const FDACampaignSnapshot& After = World->GetPersistentCampaign();
        TestTrue("Tal outcome is durable", After.RegionalCrisis.CitizenOutcomes.Contains(TEXT("citizen.neutral.tal_arden")));
        TestTrue("Mara outcome is durable", After.RegionalCrisis.CitizenOutcomes.Contains(TEXT("citizen.forgeweave.mara_kest")));
        TestTrue("Ori outcome is durable", After.RegionalCrisis.CitizenOutcomes.Contains(TEXT("citizen.eden.ori_sen")));
        TestTrue("Daxton dialogue condition reads durable outcomes",
            FDAFoundryShortageRuntime::EvaluateDialogueCondition(TEXT("dialogue.daxton.foundry_followup"), After));
        TestTrue("Amara dialogue condition reads durable outcomes",
            FDAFoundryShortageRuntime::EvaluateDialogueCondition(TEXT("dialogue.amara.foundry_followup"), After));
        TestEqual("Daxton gets Mara's outcome-specific variant",
            FDAFoundryShortageRuntime::EvaluateDialogueVariant(TEXT("dialogue.daxton.foundry_followup"), After),
            FName(TEXT("mara_audited_transition")));
        TestEqual("Amara gets Ori's outcome-specific variant",
            FDAFoundryShortageRuntime::EvaluateDialogueVariant(TEXT("dialogue.amara.foundry_followup"), After),
            FName(TEXT("ori_engineered_mitigation")));
        TestTrue("Mara outcome is also an exact persistent history condition",
            After.HistoryTags.Contains(TEXT("mara_audited_transition")));
        TestTrue("Ori outcome is also an exact persistent history condition",
            After.HistoryTags.Contains(TEXT("ori_engineered_mitigation")));
    });

    It("registers later dialogue queries with distinct Mara and Green Line variants", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDARegionalDialogueConditionSubsystem* Dialogue =
            Fixture.GetSubsystem<UDARegionalDialogueConditionSubsystem>();
        TestNotNull("Regional dialogue condition authority registers", Dialogue);
        if (World == nullptr || Dialogue == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.HistoryTags.AddUnique(TEXT("mara_numbers_public"));
        Candidate.HistoryTags.AddUnique(TEXT("green_line_full_boundary"));
        Candidate.HistoryTags.Sort([](FName A, FName B){ return A.LexicalLess(B); });
        TestTrue("Outcome-specific dialogue fixture restores", World->RestorePersistentCampaign(Candidate));
        TestEqual("Daxton public variant is exact", Dialogue->EvaluateVariant(
            TEXT("dialogue.daxton.foundry_followup")), FName(TEXT("daxton.mara.public")));
        TestEqual("Amara boundary variant is exact", Dialogue->EvaluateVariant(
            TEXT("dialogue.amara.foundry_followup")), FName(TEXT("amara.green_line.full_boundary")));
        Candidate = World->GetPersistentCampaign();
        Candidate.HistoryTags.Remove(TEXT("mara_numbers_public"));
        Candidate.HistoryTags.Remove(TEXT("green_line_full_boundary"));
        Candidate.HistoryTags.AddUnique(TEXT("mara_numbers_quiet"));
        Candidate.HistoryTags.AddUnique(TEXT("green_line_engineered_mitigation"));
        Candidate.HistoryTags.Sort([](FName A, FName B){ return A.LexicalLess(B); });
        TestTrue("Alternate outcome fixture restores", World->RestorePersistentCampaign(Candidate));
        TestEqual("Daxton quiet variant differs", Dialogue->EvaluateVariant(
            TEXT("dialogue.daxton.foundry_followup")), FName(TEXT("daxton.mara.quiet")));
        TestEqual("Amara mitigation variant differs", Dialogue->EvaluateVariant(
            TEXT("dialogue.amara.foundry_followup")), FName(TEXT("amara.green_line.engineered_mitigation")));
    });

    It("coordinates regional definitions through Task18 persisted runtimes on campaign commits", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDARegionalCampaignCoordinatorSubsystem* Coordinator =
            Fixture.GetSubsystem<UDARegionalCampaignCoordinatorSubsystem>();
        TestNotNull("Regional production coordinator exists", Coordinator);
        if (World == nullptr || Coordinator == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        TestTrue("Coordinator trigger fixture restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Foundry campaign tick commits", World->AdvanceWorldTicks(1));
        TestTrue("Coordinator consumes committed campaign state", Coordinator->SynchronizeNow());
        const FDACampaignSnapshot& Started = World->GetPersistentCampaign();
        const FDAWorldEventSaveState* Event =
            Started.NarrativeState.FindEventState(TEXT("event.foundry_shortage"));
        const FDAQuestSaveState* Quest =
            Started.NarrativeState.FindQuestState(TEXT("quest.foundry_shortage"));
        TestNotNull("Task18 event state persists", Event);
        TestNotNull("Task18 quest state persists", Quest);
        if (Event != nullptr) TestEqual("Event stage follows the real crisis authority",
            Event->CurrentStageId, FName(TEXT("shortage_warning")));
        if (Quest != nullptr) TestEqual("Quest runtime reaches its authored choice",
            Quest->CurrentNodeId, FName(TEXT("choice")));
    });

    It("rejects tampered crisis transaction projections before save", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        if (World == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        TestTrue("Tamper fixture restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Warning commits", World->AdvanceWorldTicks(1));
        TestEqual("Resolution commits", World->ResolveFoundryShortage(FGuid(22, 8, 8, 8),
            EDAFoundryShortageResolution::IndustrialSupport), EDAFoundryShortageActionResult::Applied);
        FDACampaignSnapshot Tampered = World->GetPersistentCampaign();
        Tampered.RegionalCrisis.ResolutionRecords[0].ResourceHungerAfter += 1.f;
        FString Error;
        TestFalse("Tampered transaction fails snapshot validation", Tampered.Validate(Error));
        FDACampaignSnapshot MissingCrisis = World->GetPersistentCampaign();
        MissingCrisis.RegionalCrisis = FDARegionalCrisisCampaignState{};
        TestFalse("Completed Foundry quest cannot survive Resolution None", MissingCrisis.Validate(Error));
        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("RegionalCrisisTamper"));
        TestFalse("Tampered transaction cannot be serialized as a valid campaign",
            FDASaveService(Directory).SaveCampaign(Tampered, TEXT("tampered")).IsSuccess());
    });
}
