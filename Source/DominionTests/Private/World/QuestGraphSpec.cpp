#include "Narrative/DAQuestDefinition.h"
#include "Narrative/DAQuestRuntime.h"
#include "Narrative/DAPromiseLedger.h"
#include "Narrative/DAWorldEventDefinition.h"
#include "Save/DASaveService.h"

#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Save/DASaveJsonFields.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <limits>

BEGIN_DEFINE_SPEC(FDAQuestGraphSpec, "Dominion.World.Narrative.QuestGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

    FString TestSaveDirectory;
END_DEFINE_SPEC(FDAQuestGraphSpec)

namespace
{
    bool SerializeJson(const TSharedRef<FJsonObject>& Object, FString& OutJson)
    {
        OutJson.Reset();
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
        return FJsonSerializer::Serialize(Object, Writer);
    }

    FString FixtureChecksum(const TSharedRef<FJsonObject>& Campaign)
    {
        const TSharedRef<FJsonObject> Material = MakeShared<FJsonObject>();
        Material->SetNumberField(FDASaveJsonFields::SchemaVersion, FDASaveService::CurrentSchemaVersion);
        Material->SetNumberField(FDASaveJsonFields::ContentVersion,
            FDASaveService::CurrentContentVersion);
        Material->SetNumberField(FDASaveJsonFields::BuildVersion,
            FDASaveService::CurrentBuildVersion);
        Material->SetObjectField(FDASaveJsonFields::Campaign, Campaign);
        FString Json;
        SerializeJson(Material, Json);
        const FTCHARToUTF8 Utf8(*Json);
        return FString::Printf(TEXT("%08X"), FCrc::MemCrc32(Utf8.Get(), Utf8.Length()));
    }

    bool TamperManifestWithValidChecksum(const FString& Directory, const FString& Slot, const bool bQuest)
    {
        const FString Path = FPaths::Combine(Directory, Slot + TEXT(".dasave"));
        FString Json;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(Json, *Path)) return false;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        const TSharedPtr<FJsonObject>* Narrative = nullptr;
        if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr
            || !(*Campaign)->TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative) || Narrative == nullptr)
        {
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>* States = nullptr;
        const TCHAR* StateField = bQuest ? TEXT("questStates") : TEXT("eventStates");
        if (!(*Narrative)->TryGetArrayField(StateField, States) || States == nullptr || States->Num() != 1) return false;
        const TSharedPtr<FJsonObject>* Manifest = nullptr;
        if (!(*States)[0]->AsObject()->TryGetObjectField(TEXT("definitionManifest"), Manifest) || Manifest == nullptr) return false;
        if (bQuest) (*Manifest)->SetStringField(TEXT("definitionFingerprint"), TEXT("checksummed-tamper"));
        else (*Manifest)->SetStringField(TEXT("scope"), TEXT("UnknownScope"));
        Root->SetStringField(FDASaveJsonFields::Checksum, FixtureChecksum(Campaign->ToSharedRef()));
        return SerializeJson(Root.ToSharedRef(), Json)
            && FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    FDAQuestNodeDefinition MakeNode(const FName NodeId, const EDAQuestNodeType Type)
    {
        FDAQuestNodeDefinition Node;
        Node.NodeId = NodeId;
        Node.Type = Type;
        Node.SourceDefinitionId = FName(*FString::Printf(TEXT("quest.source.%s"), *NodeId.ToString()));
        return Node;
    }

    FDAQuestEdgeDefinition MakeEdge(
        const FName BranchTag,
        const FName TargetNodeId,
        const EDAQuestEdgeCondition Condition = EDAQuestEdgeCondition::Always,
        const FName WorldAssetBindingId = NAME_None)
    {
        FDAQuestEdgeDefinition Edge;
        Edge.BranchTag = BranchTag;
        Edge.TargetNodeId = TargetNodeId;
        Edge.Condition = Condition;
        Edge.WorldAssetBindingId = WorldAssetBindingId;
        return Edge;
    }

    FDAQuestDefinition MakeMinimalQuest()
    {
        FDAQuestDefinition Definition;
        Definition.QuestId = TEXT("quest.synara.test_graph");
        Definition.SourceDefinitionId = TEXT("quest.source.synara.test_graph");
        Definition.Version = 1;
        Definition.StartNodeId = TEXT("start");

        FDAQuestNodeDefinition Start = MakeNode(TEXT("start"), EDAQuestNodeType::Start);
        Start.Edges.Add(MakeEdge(TEXT("Begin"), TEXT("resolution")));
        Definition.Nodes.Add(Start);
        Definition.Nodes.Add(MakeNode(TEXT("resolution"), EDAQuestNodeType::Resolution));
        return Definition;
    }

    FDAQuestDefinition MakeWorldAssetQuest()
    {
        FDAQuestDefinition Definition;
        Definition.QuestId = TEXT("quest.synara.nia.cognitive_tower");
        Definition.SourceDefinitionId = TEXT("quest.source.synara.nia.cognitive_tower");
        Definition.Version = 1;
        Definition.StartNodeId = TEXT("start");
        Definition.RequiredWorldAssetBindingIds.Add(TEXT("nia_cognitive_tower"));

        FDAQuestNodeDefinition Start = MakeNode(TEXT("start"), EDAQuestNodeType::Start);
        Start.Edges.Add(MakeEdge(TEXT("Begin"), TEXT("repair_tower")));

        FDAQuestNodeDefinition Repair = MakeNode(TEXT("repair_tower"), EDAQuestNodeType::Objective);
        Repair.Edges.Add(MakeEdge(
            TEXT("ContinueRepair"),
            TEXT("resolution"),
            EDAQuestEdgeCondition::WorldAssetAvailable,
            TEXT("nia_cognitive_tower")));
        Repair.Edges.Add(MakeEdge(
            TEXT("ReconstructOrReplace"),
            TEXT("reconstruct_or_replace"),
            EDAQuestEdgeCondition::WorldAssetDestroyed,
            TEXT("nia_cognitive_tower")));

        FDAQuestNodeDefinition Reconstruct = MakeNode(TEXT("reconstruct_or_replace"), EDAQuestNodeType::Build);
        Reconstruct.Edges.Add(MakeEdge(TEXT("Rebuilt"), TEXT("resolution")));

        Definition.Nodes = {Start, Repair, Reconstruct, MakeNode(TEXT("resolution"), EDAQuestNodeType::Resolution)};
        return Definition;
    }

    FDAWorldEventDefinition MakeWorldEvent()
    {
        FDAWorldEventDefinition Definition;
        Definition.EventId = TEXT("event.regional.foundry_shortage");
        Definition.SourceDefinitionId = TEXT("event.source.regional.foundry_shortage");
        Definition.Version = 1;
        Definition.InitialStageId = TEXT("signal");

        FDAWorldEventStageDefinition Signal;
        Signal.StageId = TEXT("signal");
        Signal.SourceDefinitionId = TEXT("event.foundry_shortage.signal");
        Signal.AllowedNextStageIds.Add(TEXT("aftermath"));

        FDAWorldEventStageDefinition Aftermath;
        Aftermath.StageId = TEXT("aftermath");
        Aftermath.SourceDefinitionId = TEXT("event.foundry_shortage.aftermath");
        Aftermath.bResolution = true;
        Definition.Stages = {Signal, Aftermath};
        return Definition;
    }

    FDAQuestEvaluationContext AtTick(const int64 WorldTick)
    {
        FDAQuestEvaluationContext Context;
        Context.WorldTick = WorldTick;
        return Context;
    }
}

void FDAQuestGraphSpec::Define()
{
    BeforeEach([this]()
    {
        TestSaveDirectory = FPaths::Combine(
            FPaths::ProjectIntermediateDir(),
            TEXT("DominionQuestSaveTests"),
            FGuid::NewGuid().ToString(EGuidFormats::Digits));
    });

    AfterEach([this]()
    {
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*TestSaveDirectory);
    });

    It("rejects an unreachable Resolution node", [this]()
    {
        FDAQuestDefinition Definition = MakeMinimalQuest();
        Definition.Nodes[0].Edges[0].TargetNodeId = TEXT("failure");
        Definition.Nodes.Add(MakeNode(TEXT("failure"), EDAQuestNodeType::Failure));

        FString Error;
        TestFalse("Unreachable Resolution fails validation", Definition.Validate(Error));
        TestTrue("Failure explains reachability", Error.Contains(TEXT("unreachable")));
    });

    It("rejects quest graph cycles", [this]()
    {
        FDAQuestDefinition Definition = MakeMinimalQuest();
        FDAQuestNodeDefinition Objective = MakeNode(TEXT("objective"), EDAQuestNodeType::Objective);
        Definition.Nodes[0].Edges[0].TargetNodeId = Objective.NodeId;
        Objective.Edges.Add(MakeEdge(TEXT("Loop"), TEXT("start")));
        Definition.Nodes.Add(Objective);

        FString Error;
        TestFalse("A cycle cannot become durable quest content", Definition.Validate(Error));
        TestTrue("Failure explains the cycle", Error.Contains(TEXT("cycle")));
    });

    It("rejects edges whose target node does not exist", [this]()
    {
        FDAQuestDefinition Definition = MakeMinimalQuest();
        Definition.Nodes[0].Edges[0].TargetNodeId = TEXT("missing_node");

        FString Error;
        TestFalse("Invalid target fails validation", Definition.Validate(Error));
        TestTrue("Failure explains the edge", Error.Contains(TEXT("edge")));
    });

    It("rejects edges outside the exact data-driven condition taxonomy", [this]()
    {
        FDAQuestDefinition Definition = MakeMinimalQuest();
        Definition.Nodes[0].Edges[0].Condition = static_cast<EDAQuestEdgeCondition>(255);

        FString Error;
        TestFalse("Unknown edge condition fails validation", Definition.Validate(Error));
        TestTrue("Failure explains the edge", Error.Contains(TEXT("edge")));
    });

    It("exposes exactly the canonical v1.0 quest node taxonomy", [this]()
    {
        const TArray<EDAQuestNodeType> Expected = {
            EDAQuestNodeType::Start,
            EDAQuestNodeType::Dialogue,
            EDAQuestNodeType::Objective,
            EDAQuestNodeType::Investigation,
            EDAQuestNodeType::Build,
            EDAQuestNodeType::Deliver,
            EDAQuestNodeType::Explore,
            EDAQuestNodeType::Combat,
            EDAQuestNodeType::Defend,
            EDAQuestNodeType::Capture,
            EDAQuestNodeType::Choice,
            EDAQuestNodeType::Wait,
            EDAQuestNodeType::Timer,
            EDAQuestNodeType::WorldCondition,
            EDAQuestNodeType::CitizenCondition,
            EDAQuestNodeType::FactionCondition,
            EDAQuestNodeType::EconomyCondition,
            EDAQuestNodeType::RelationshipCondition,
            EDAQuestNodeType::EventTrigger,
            EDAQuestNodeType::Reward,
            EDAQuestNodeType::Failure,
            EDAQuestNodeType::Resolution
        };

        TestEqual("No node type is missing or added", FDAQuestDefinition::GetSupportedNodeTypes(), Expected);
    });

    It("selects the authored ReconstructOrReplace branch when a bound WorldAsset is destroyed", [this]()
    {
        const FDAQuestDefinition Definition = MakeWorldAssetQuest();
        FString DefinitionError;
        TestTrue("Fixture definition is valid", Definition.Validate(DefinitionError));

        FDACampaignSnapshot Campaign;
        FDAWorldAssetRecord Tower;
        Tower.WorldAssetId = FGuid(11, 22, 33, 44);
        Tower.CardDefinitionId = TEXT("synara.cognitive_operations_tower");
        Tower.CityId = TEXT("player_capital");
        Tower.ConstructionState = EDAConstructionState::Operational;
        Tower.StructuralIntegrity = 100.f;
        Tower.OwnerCivilizationId = TEXT("civilization.synara");
        Campaign.WorldAssets.Add(Tower);

        FDAQuestWorldAssetBinding Binding;
        Binding.BindingId = TEXT("nia_cognitive_tower");
        Binding.WorldAssetId = Tower.WorldAssetId;
        TestEqual(
            "Quest starts against the actual campaign asset",
            FDAQuestRuntime::StartQuest(Definition, {Binding}, 12, Campaign),
            EDAQuestRuntimeResult::Applied);

        FName SelectedBranch;
        TestEqual(
            "Start advances to the repair objective",
            FDAQuestRuntime::EvaluateCurrentNode(Definition, AtTick(12), Campaign, SelectedBranch),
            EDAQuestRuntimeResult::Applied);

        FDAWorldAssetRecord* ActualTower = Campaign.FindWorldAssetRecord(Tower.WorldAssetId);
        TestNotNull("The bound real tower remains authoritative", ActualTower);
        if (ActualTower == nullptr)
        {
            return;
        }
        ActualTower->StructuralIntegrity = 0.f;
        ActualTower->ConstructionState = EDAConstructionState::Ruined;
        const int32 AssetCountBeforeAdaptation = Campaign.WorldAssets.Num();

        TestEqual(
            "Destroyed state advances through the data-authored branch",
            FDAQuestRuntime::EvaluateCurrentNode(Definition, AtTick(13), Campaign, SelectedBranch),
            EDAQuestRuntimeResult::Applied);
        TestEqual("The exact authored branch is selected", SelectedBranch, FName(TEXT("ReconstructOrReplace")));
        TestEqual("No duplicate quest-only tower is spawned", Campaign.WorldAssets.Num(), AssetCountBeforeAdaptation);
        TestEqual("The same WorldAssetID remains bound", Campaign.WorldAssets[0].WorldAssetId, Tower.WorldAssetId);

        const FDAQuestSaveState* State = Campaign.NarrativeState.FindQuestState(Definition.QuestId);
        TestNotNull("Quest state is durable in the campaign aggregate", State);
        if (State != nullptr)
        {
            TestEqual("The durable node is reconstruct-or-replace", State->CurrentNodeId, FName(TEXT("reconstruct_or_replace")));
        }
    });

    It("pins the complete authored quest and event identity even when the version is reused", [this]()
    {
        FDACampaignSnapshot Campaign;
        const FDAQuestDefinition Quest = MakeMinimalQuest();
        const FDAWorldEventDefinition Event = MakeWorldEvent();
        TestEqual("Quest starts", FDAQuestRuntime::StartQuest(Quest, {}, 4, Campaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Event starts", FDAWorldEventRuntime::StartEvent(Event, 4, Campaign), EDAWorldEventRuntimeResult::Applied);
        const int64 RevisionBefore = Campaign.NarrativeState.MutationRevision;

        FDAQuestDefinition AlteredQuestSource = Quest;
        AlteredQuestSource.SourceDefinitionId = TEXT("quest.source.same_version_replacement");
        TestEqual(
            "Same-version quest source replacement is rejected",
            FDAQuestRuntime::StartQuest(AlteredQuestSource, {}, 4, Campaign),
            EDAQuestRuntimeResult::DefinitionMismatch);

        FDAQuestDefinition AlteredQuestStart = Quest;
        AlteredQuestStart.StartNodeId = TEXT("alternate_start");
        AlteredQuestStart.Nodes[0].NodeId = AlteredQuestStart.StartNodeId;
        TestEqual(
            "Same-version authored start replacement is rejected",
            FDAQuestRuntime::StartQuest(AlteredQuestStart, {}, 4, Campaign),
            EDAQuestRuntimeResult::DefinitionMismatch);

        FDAQuestDefinition AlteredQuestEdge = Quest;
        AlteredQuestEdge.Nodes[0].Edges[0].BranchTag = TEXT("DifferentBegin");
        FName Branch;
        TestEqual(
            "Same-version edge replacement cannot evaluate an existing instance",
            FDAQuestRuntime::EvaluateCurrentNode(AlteredQuestEdge, AtTick(5), Campaign, Branch),
            EDAQuestRuntimeResult::DefinitionMismatch);

        FDAWorldEventDefinition AlteredEventStage = Event;
        AlteredEventStage.Stages[0].SourceDefinitionId = TEXT("event.source.same_version_replacement");
        TestEqual(
            "Same-version event stage replacement cannot advance an existing instance",
            FDAWorldEventRuntime::AdvanceEvent(AlteredEventStage, TEXT("aftermath"), 5, Campaign),
            EDAWorldEventRuntimeResult::DefinitionMismatch);
        TestEqual("Definition conflicts are atomic", Campaign.NarrativeState.MutationRevision, RevisionBefore);
    });

    It("rejects structurally inconsistent persisted narrative instances", [this]()
    {
        FDACampaignSnapshot Campaign;
        const FDAQuestDefinition Quest = MakeMinimalQuest();
        const FDAWorldEventDefinition Event = MakeWorldEvent();
        TestEqual("Quest starts", FDAQuestRuntime::StartQuest(Quest, {}, 1, Campaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Event starts", FDAWorldEventRuntime::StartEvent(Event, 1, Campaign), EDAWorldEventRuntimeResult::Applied);
        FString Error;
        TestTrue("Baseline snapshot validates", Campaign.Validate(Error));

        FDACampaignSnapshot InvalidEnum = Campaign;
        InvalidEnum.NarrativeState.QuestStates[0].DefinitionManifest.Nodes[0].Type = static_cast<EDAQuestNodeType>(255);
        TestFalse("Unknown reflected node enum is rejected", InvalidEnum.Validate(Error));

        FDACampaignSnapshot MissingNode = Campaign;
        MissingNode.NarrativeState.QuestStates[0].CurrentNodeId = TEXT("missing");
        TestFalse("Current node must belong to immutable definition", MissingNode.Validate(Error));

        FDACampaignSnapshot IncompleteBinding = Campaign;
        FDAQuestSaveState& BindingState = IncompleteBinding.NarrativeState.QuestStates[0];
        BindingState.DefinitionManifest.RequiredWorldAssetBindingIds.Add(TEXT("required_asset"));
        BindingState.DefinitionManifest.RefreshFingerprint();
        TestFalse("Every required binding must exist on the instance", IncompleteBinding.Validate(Error));

        FDACampaignSnapshot InvalidScope = Campaign;
        InvalidScope.NarrativeState.EventStates[0].DefinitionManifest.Scope = static_cast<EDAWorldEventScope>(255);
        TestFalse("Unknown event scope is rejected", InvalidScope.Validate(Error));

        FDACampaignSnapshot InvalidIdentity = Campaign;
        InvalidIdentity.NarrativeState.QuestStates[0].QuestId = TEXT("quest.rebound");
        TestFalse("Instance IDs must agree with the immutable manifest", InvalidIdentity.Validate(Error));

        FDACampaignSnapshot Terminal = Campaign;
        FName Branch;
        TestEqual("Quest reaches terminal", FDAQuestRuntime::EvaluateCurrentNode(Quest, AtTick(2), Terminal, Branch), EDAQuestRuntimeResult::Applied);
        Terminal.NarrativeState.QuestStates[0].ProgressState = EDAQuestProgressState::Active;
        TestFalse("Terminal node and progress state must agree", Terminal.Validate(Error));

        FDACampaignSnapshot PromiseCampaign;
        FDAPromiseRecord Promise;
        Promise.PromiseId = FGuid(7, 7, 7, 7);
        Promise.PromiseDefinitionId = TEXT("promise.validation");
        Promise.PromiserId = TEXT("leader.validation");
        Promise.ConflictActionTags = {TEXT("action.conflict")};
        Promise.FulfillmentActionTags = {TEXT("action.fulfill")};
        FDAPromiseLedger Ledger(PromiseCampaign);
        TestEqual("Promise registers", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::Applied);
        PromiseCampaign.NarrativeState.PromiseRecords[0].State = EDAPromiseState::Fulfilled;
        PromiseCampaign.NarrativeState.PromiseRecords[0].ResolvedWorldTick = 2;
        PromiseCampaign.NarrativeState.PromiseRecords[0].ResolutionActionTag = TEXT("action.conflict");
        TestFalse("Fulfilled promise requires one of its exact fulfillment tags", PromiseCampaign.Validate(Error));
    });

    It("requires explicit choice selection and rejects ambiguous automatic transitions", [this]()
    {
        FDAQuestDefinition ChoiceQuest = MakeMinimalQuest();
        FDAQuestNodeDefinition Choice = MakeNode(TEXT("choice"), EDAQuestNodeType::Choice);
        Choice.Edges = {
            MakeEdge(TEXT("Preserve"), TEXT("resolution")),
            MakeEdge(TEXT("Replace"), TEXT("replace_resolution"))};
        ChoiceQuest.Nodes[0].Edges[0].TargetNodeId = Choice.NodeId;
        ChoiceQuest.Nodes.Insert(Choice, 1);
        ChoiceQuest.Nodes.Add(MakeNode(TEXT("replace_resolution"), EDAQuestNodeType::Resolution));

        FDACampaignSnapshot ChoiceCampaign;
        FName Branch;
        TestEqual("Choice quest starts", FDAQuestRuntime::StartQuest(ChoiceQuest, {}, 1, ChoiceCampaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Start enters the choice", FDAQuestRuntime::EvaluateCurrentNode(ChoiceQuest, AtTick(1), ChoiceCampaign, Branch), EDAQuestRuntimeResult::Applied);
        TestEqual("Choice cannot auto-select", FDAQuestRuntime::EvaluateCurrentNode(ChoiceQuest, AtTick(2), ChoiceCampaign, Branch), EDAQuestRuntimeResult::RequiresChoice);
        TestEqual("Authored choice selects one exact branch", FDAQuestRuntime::SelectChoice(ChoiceQuest, TEXT("Replace"), AtTick(2), ChoiceCampaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Selected branch is durable", ChoiceCampaign.NarrativeState.QuestStates[0].CurrentNodeId, FName(TEXT("replace_resolution")));

        FDAQuestDefinition Ambiguous = MakeMinimalQuest();
        Ambiguous.Nodes[0].Edges.Add(MakeEdge(TEXT("AlsoBegin"), TEXT("resolution")));
        FDACampaignSnapshot AmbiguousCampaign;
        TestEqual("Ambiguous fixture starts", FDAQuestRuntime::StartQuest(Ambiguous, {}, 1, AmbiguousCampaign), EDAQuestRuntimeResult::Applied);
        const int64 RevisionBefore = AmbiguousCampaign.NarrativeState.MutationRevision;
        TestEqual("Two automatic matches are rejected", FDAQuestRuntime::EvaluateCurrentNode(Ambiguous, AtTick(1), AmbiguousCampaign, Branch), EDAQuestRuntimeResult::AmbiguousTransition);
        TestEqual("Ambiguity cannot mutate", AmbiguousCampaign.NarrativeState.MutationRevision, RevisionBefore);
    });

    It("evaluates reflected timer citizen faction economy and relationship payload variants", [this]()
    {
        const TArray<EDAQuestNodeType> Types = {
            EDAQuestNodeType::CitizenCondition,
            EDAQuestNodeType::FactionCondition,
            EDAQuestNodeType::EconomyCondition,
            EDAQuestNodeType::RelationshipCondition};
        const TArray<EDAQuestPayloadVariant> Variants = {
            EDAQuestPayloadVariant::Citizen,
            EDAQuestPayloadVariant::Faction,
            EDAQuestPayloadVariant::Economy,
            EDAQuestPayloadVariant::Relationship};

        for (int32 Index = 0; Index < Types.Num(); ++Index)
        {
            FDAQuestDefinition Definition = MakeMinimalQuest();
            FDAQuestNodeDefinition Condition = MakeNode(TEXT("condition"), Types[Index]);
            Condition.Payload.Variant = Variants[Index];
            Condition.Payload.Condition.EvaluationKey = TEXT("metric.test");
            Condition.Payload.Condition.Comparison = EDAQuestComparison::GreaterOrEqual;
            Condition.Payload.Condition.ExpectedValue = 10.0;
            Condition.Edges.Add(MakeEdge(TEXT("Satisfied"), TEXT("resolution")));
            Definition.Nodes[0].Edges[0].TargetNodeId = Condition.NodeId;
            Definition.Nodes.Insert(Condition, 1);

            FDACampaignSnapshot Campaign;
            FName Branch;
            FDAQuestEvaluationContext Context = AtTick(10);
            Context.SetMetric(Variants[Index], TEXT("metric.test"), 10.0);
            TestEqual("Typed condition quest starts", FDAQuestRuntime::StartQuest(Definition, {}, 10, Campaign), EDAQuestRuntimeResult::Applied);
            TestEqual("Start enters typed condition", FDAQuestRuntime::EvaluateCurrentNode(Definition, Context, Campaign, Branch), EDAQuestRuntimeResult::Applied);
            TestEqual("Typed condition resolves", FDAQuestRuntime::EvaluateCurrentNode(Definition, Context, Campaign, Branch), EDAQuestRuntimeResult::Applied);
        }

        FDAQuestDefinition TimerDefinition = MakeMinimalQuest();
        FDAQuestNodeDefinition Timer = MakeNode(TEXT("timer"), EDAQuestNodeType::Timer);
        Timer.Payload.Variant = EDAQuestPayloadVariant::Timer;
        Timer.Payload.Timer.DurationWorldTicks = 3;
        Timer.Edges.Add(MakeEdge(TEXT("Elapsed"), TEXT("resolution")));
        TimerDefinition.Nodes[0].Edges[0].TargetNodeId = Timer.NodeId;
        TimerDefinition.Nodes.Insert(Timer, 1);
        FDACampaignSnapshot TimerCampaign;
        FName TimerBranch;
        TestEqual("Timer quest starts", FDAQuestRuntime::StartQuest(TimerDefinition, {}, 20, TimerCampaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Start enters timer", FDAQuestRuntime::EvaluateCurrentNode(TimerDefinition, AtTick(20), TimerCampaign, TimerBranch), EDAQuestRuntimeResult::Applied);
        TestEqual("Timer waits before duration", FDAQuestRuntime::EvaluateCurrentNode(TimerDefinition, AtTick(22), TimerCampaign, TimerBranch), EDAQuestRuntimeResult::Waiting);
        TestEqual("Timer advances at duration", FDAQuestRuntime::EvaluateCurrentNode(TimerDefinition, AtTick(23), TimerCampaign, TimerBranch), EDAQuestRuntimeResult::Applied);
    });

    It("validates completed quest and event records as exact authored paths", [this]()
    {
        FDAQuestDefinition Quest = MakeMinimalQuest();
        Quest.Nodes[0].Edges[0].TargetNodeId = TEXT("choice");
        Quest.Nodes[1].NodeId = TEXT("left_resolution");
        FDAQuestNodeDefinition Choice = MakeNode(TEXT("choice"), EDAQuestNodeType::Choice);
        Choice.Edges = {MakeEdge(TEXT("Left"), TEXT("left_mid")), MakeEdge(TEXT("Right"), TEXT("right_mid"))};
        FDAQuestNodeDefinition Left = MakeNode(TEXT("left_mid"), EDAQuestNodeType::Objective);
        Left.Edges.Add(MakeEdge(TEXT("FinishLeft"), TEXT("left_resolution")));
        FDAQuestNodeDefinition Right = MakeNode(TEXT("right_mid"), EDAQuestNodeType::Objective);
        Right.Edges.Add(MakeEdge(TEXT("FinishRight"), TEXT("right_resolution")));
        Quest.Nodes.Add(Choice);
        Quest.Nodes.Add(Left);
        Quest.Nodes.Add(Right);
        Quest.Nodes.Add(MakeNode(TEXT("right_resolution"), EDAQuestNodeType::Resolution));

        FDACampaignSnapshot QuestCampaign;
        FName Branch;
        TestEqual("Branching quest starts", FDAQuestRuntime::StartQuest(Quest, {}, 1, QuestCampaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Start enters choice", FDAQuestRuntime::EvaluateCurrentNode(Quest, AtTick(1), QuestCampaign, Branch), EDAQuestRuntimeResult::Applied);
        TestEqual("Left choice is explicit", FDAQuestRuntime::SelectChoice(Quest, TEXT("Left"), AtTick(2), QuestCampaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Left path resolves", FDAQuestRuntime::EvaluateCurrentNode(Quest, AtTick(3), QuestCampaign, Branch), EDAQuestRuntimeResult::Applied);
        FString Error;
        TestTrue("Authored choice path validates", QuestCampaign.Validate(Error));

        FDACampaignSnapshot BranchExclusiveQuest = QuestCampaign;
        BranchExclusiveQuest.NarrativeState.QuestStates[0].CompletedNodeIds[2] = TEXT("right_mid");
        TestFalse("A node from the unselected choice branch is rejected", BranchExclusiveQuest.Validate(Error));
        FDACampaignSnapshot ReorderedQuest = QuestCampaign;
        Swap(ReorderedQuest.NarrativeState.QuestStates[0].CompletedNodeIds[0],
            ReorderedQuest.NarrativeState.QuestStates[0].CompletedNodeIds[1]);
        TestFalse("Completed quest nodes cannot be reordered", ReorderedQuest.Validate(Error));
        FDACampaignSnapshot TerminalCompletedQuest = QuestCampaign;
        TerminalCompletedQuest.NarrativeState.QuestStates[0].CompletedNodeIds.Add(TEXT("right_resolution"));
        TestFalse("A terminal quest node cannot be completed before current", TerminalCompletedQuest.Validate(Error));

        FDAWorldEventDefinition Event = MakeWorldEvent();
        Event.Stages[0].AllowedNextStageIds = {TEXT("left_stage"), TEXT("right_stage")};
        FDAWorldEventStageDefinition LeftStage;
        LeftStage.StageId = TEXT("left_stage");
        LeftStage.SourceDefinitionId = TEXT("event.path.left");
        LeftStage.AllowedNextStageIds = {TEXT("aftermath")};
        FDAWorldEventStageDefinition RightStage;
        RightStage.StageId = TEXT("right_stage");
        RightStage.SourceDefinitionId = TEXT("event.path.right");
        RightStage.AllowedNextStageIds = {TEXT("right_resolution")};
        FDAWorldEventStageDefinition RightResolution;
        RightResolution.StageId = TEXT("right_resolution");
        RightResolution.SourceDefinitionId = TEXT("event.path.right_resolution");
        RightResolution.bResolution = true;
        Event.Stages.Add(LeftStage);
        Event.Stages.Add(RightStage);
        Event.Stages.Add(RightResolution);
        FDACampaignSnapshot EventCampaign;
        TestEqual("Branching event starts", FDAWorldEventRuntime::StartEvent(Event, 1, EventCampaign), EDAWorldEventRuntimeResult::Applied);
        TestEqual("Event selects left stage", FDAWorldEventRuntime::AdvanceEvent(Event, TEXT("left_stage"), 2, EventCampaign), EDAWorldEventRuntimeResult::Applied);
        TestEqual("Event reaches left resolution", FDAWorldEventRuntime::AdvanceEvent(Event, TEXT("aftermath"), 3, EventCampaign), EDAWorldEventRuntimeResult::Applied);
        TestTrue("Authored event path validates", EventCampaign.Validate(Error));
        FDACampaignSnapshot BranchExclusiveEvent = EventCampaign;
        BranchExclusiveEvent.NarrativeState.EventStates[0].CompletedStageIds[1] = TEXT("right_stage");
        TestFalse("A stage from an unselected event branch is rejected", BranchExclusiveEvent.Validate(Error));
        FDACampaignSnapshot SkippedEvent = EventCampaign;
        SkippedEvent.NarrativeState.EventStates[0].CompletedStageIds.RemoveAt(1);
        TestFalse("Completed event stages must end immediately before current", SkippedEvent.Validate(Error));
    });

    It("uses collision-resistant UTF-8 fingerprints and canonicalizes negative zero", [this]()
    {
        FDAQuestDefinition UnicodeA = MakeMinimalQuest();
        UnicodeA.SourceDefinitionId = FName(TEXT("quest.source.世界"));
        FDAQuestDefinition UnicodeB = UnicodeA;
        UnicodeB.SourceDefinitionId = FName(TEXT("quest.source.世海"));
        const FString FingerprintA = UnicodeA.BuildManifest().DefinitionFingerprint;
        TestEqual("Unicode fingerprint is repeatable", UnicodeA.BuildManifest().DefinitionFingerprint, FingerprintA);
        TestTrue("Distinct non-ANSI source identities cannot collide",
            UnicodeB.BuildManifest().DefinitionFingerprint != FingerprintA);

        FDAQuestDefinition PositiveZero = MakeMinimalQuest();
        FDAQuestNodeDefinition Condition = MakeNode(TEXT("world_condition"), EDAQuestNodeType::WorldCondition);
        Condition.Payload.Variant = EDAQuestPayloadVariant::World;
        Condition.Payload.Condition.EvaluationKey = TEXT("metric.zero");
        Condition.Payload.Condition.Comparison = EDAQuestComparison::Equal;
        Condition.Payload.Condition.ExpectedValue = 0.0;
        Condition.Edges.Add(MakeEdge(TEXT("Zero"), TEXT("resolution")));
        PositiveZero.Nodes[0].Edges[0].TargetNodeId = Condition.NodeId;
        PositiveZero.Nodes.Insert(Condition, 1);
        FDAQuestDefinition NegativeZero = PositiveZero;
        NegativeZero.Nodes[1].Payload.Condition.ExpectedValue = -0.0;
        TestEqual("Positive and negative zero have one canonical fingerprint",
            PositiveZero.BuildManifest().DefinitionFingerprint,
            NegativeZero.BuildManifest().DefinitionFingerprint);
    });

    It("rejects nonfinite and noncanonical inactive payload fields", [this]()
    {
        FString Error;
        FDAQuestDefinition NonePayload = MakeMinimalQuest();
        NonePayload.Nodes[0].Payload.Condition.ExpectedValue = std::numeric_limits<double>::quiet_NaN();
        const FDAQuestDefinitionManifest NoneManifest = NonePayload.BuildManifest();
        TestFalse("None payload cannot hide inactive NaN", NoneManifest.Validate(Error));

        FDAQuestDefinition TimerPayload = MakeMinimalQuest();
        FDAQuestNodeDefinition Timer = MakeNode(TEXT("timer_nonfinite"), EDAQuestNodeType::Timer);
        Timer.Payload.Variant = EDAQuestPayloadVariant::Timer;
        Timer.Payload.Timer.DurationWorldTicks = 4;
        Timer.Payload.Condition.ExpectedValue = std::numeric_limits<double>::infinity();
        Timer.Edges.Add(MakeEdge(TEXT("Elapsed"), TEXT("resolution")));
        TimerPayload.Nodes[0].Edges[0].TargetNodeId = Timer.NodeId;
        TimerPayload.Nodes.Insert(Timer, 1);
        const FDAQuestDefinitionManifest TimerManifest = TimerPayload.BuildManifest();
        TestFalse("Timer payload cannot hide inactive infinity", TimerManifest.Validate(Error));

        FDAQuestDefinition AssetPayload = MakeMinimalQuest();
        AssetPayload.RequiredWorldAssetBindingIds = {TEXT("facility")};
        FDAQuestNodeDefinition Asset = MakeNode(TEXT("asset_nonfinite"), EDAQuestNodeType::Objective);
        Asset.Payload.Variant = EDAQuestPayloadVariant::WorldAsset;
        Asset.Payload.WorldAssetBindingId = TEXT("facility");
        Asset.Payload.Condition.ExpectedValue = std::numeric_limits<double>::quiet_NaN();
        Asset.Edges.Add(MakeEdge(TEXT("Operational"), TEXT("resolution")));
        AssetPayload.Nodes[0].Edges[0].TargetNodeId = Asset.NodeId;
        AssetPayload.Nodes.Insert(Asset, 1);
        const FDAQuestDefinitionManifest AssetManifest = AssetPayload.BuildManifest();
        TestFalse("WorldAsset payload cannot hide inactive NaN", AssetManifest.Validate(Error));
    });

    It("evaluates reflected WorldCondition and real WorldAsset payloads", [this]()
    {
        FDAQuestDefinition WorldQuest = MakeMinimalQuest();
        FDAQuestNodeDefinition WorldCondition = MakeNode(TEXT("world_condition"), EDAQuestNodeType::WorldCondition);
        WorldCondition.Payload.Variant = EDAQuestPayloadVariant::World;
        WorldCondition.Payload.Condition.EvaluationKey = TEXT("world.stability");
        WorldCondition.Payload.Condition.Comparison = EDAQuestComparison::GreaterOrEqual;
        WorldCondition.Payload.Condition.ExpectedValue = 5.0;
        WorldCondition.Edges.Add(MakeEdge(TEXT("Stable"), TEXT("resolution")));
        WorldQuest.Nodes[0].Edges[0].TargetNodeId = WorldCondition.NodeId;
        WorldQuest.Nodes.Insert(WorldCondition, 1);
        FDACampaignSnapshot WorldCampaign;
        FName Branch;
        FDAQuestEvaluationContext Context = AtTick(1);
        Context.SetMetric(EDAQuestPayloadVariant::World, TEXT("world.stability"), 5.0);
        TestEqual("World quest starts", FDAQuestRuntime::StartQuest(WorldQuest, {}, 1, WorldCampaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Start enters WorldCondition", FDAQuestRuntime::EvaluateCurrentNode(WorldQuest, Context, WorldCampaign, Branch), EDAQuestRuntimeResult::Applied);
        TestEqual("WorldCondition evaluates typed context", FDAQuestRuntime::EvaluateCurrentNode(WorldQuest, Context, WorldCampaign, Branch), EDAQuestRuntimeResult::Applied);

        FDAQuestDefinition AssetQuest = MakeMinimalQuest();
        AssetQuest.RequiredWorldAssetBindingIds = {TEXT("facility")};
        FDAQuestNodeDefinition AssetObjective = MakeNode(TEXT("asset_objective"), EDAQuestNodeType::Objective);
        AssetObjective.Payload.Variant = EDAQuestPayloadVariant::WorldAsset;
        AssetObjective.Payload.WorldAssetBindingId = TEXT("facility");
        AssetObjective.Edges.Add(MakeEdge(TEXT("Operational"), TEXT("resolution")));
        AssetQuest.Nodes[0].Edges[0].TargetNodeId = AssetObjective.NodeId;
        AssetQuest.Nodes.Insert(AssetObjective, 1);
        FDACampaignSnapshot AssetCampaign;
        FDAWorldAssetRecord Facility;
        Facility.WorldAssetId = FGuid(90, 91, 92, 93);
        Facility.CardDefinitionId = TEXT("synara.real_facility");
        Facility.OwnerCivilizationId = TEXT("civilization.synara");
        Facility.ConstructionState = EDAConstructionState::Operational;
        Facility.StructuralIntegrity = 100.f;
        AssetCampaign.WorldAssets.Add(Facility);
        FDAQuestWorldAssetBinding Binding;
        Binding.BindingId = TEXT("facility");
        Binding.WorldAssetId = Facility.WorldAssetId;
        TestEqual("WorldAsset quest starts", FDAQuestRuntime::StartQuest(AssetQuest, {Binding}, 1, AssetCampaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Start enters WorldAsset objective", FDAQuestRuntime::EvaluateCurrentNode(AssetQuest, AtTick(1), AssetCampaign, Branch), EDAQuestRuntimeResult::Applied);
        TestEqual("Operational real asset satisfies payload", FDAQuestRuntime::EvaluateCurrentNode(AssetQuest, AtTick(2), AssetCampaign, Branch), EDAQuestRuntimeResult::Applied);
        TestEqual("Payload evaluation does not fabricate an asset", AssetCampaign.WorldAssets.Num(), 1);
    });

    It("treats identical starts idempotently but rejects stable quest and event ID reuse", [this]()
    {
        FDACampaignSnapshot Campaign;
        FDAQuestDefinition Quest = MakeMinimalQuest();
        FDAWorldEventDefinition Event = MakeWorldEvent();
        TestEqual("First quest start applies", FDAQuestRuntime::StartQuest(Quest, {}, 5, Campaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Identical quest start is idempotent", FDAQuestRuntime::StartQuest(Quest, {}, 5, Campaign), EDAQuestRuntimeResult::AlreadyApplied);
        TestEqual("First event start applies", FDAWorldEventRuntime::StartEvent(Event, 5, Campaign), EDAWorldEventRuntimeResult::Applied);
        TestEqual("Identical event start is idempotent", FDAWorldEventRuntime::StartEvent(Event, 5, Campaign), EDAWorldEventRuntimeResult::AlreadyApplied);
        const int64 RevisionBeforeConflict = Campaign.NarrativeState.MutationRevision;

        ++Quest.Version;
        ++Event.Version;
        TestEqual("QuestID cannot be reused by another version", FDAQuestRuntime::StartQuest(Quest, {}, 6, Campaign), EDAQuestRuntimeResult::InvalidState);
        TestEqual("EventID cannot be reused by another version", FDAWorldEventRuntime::StartEvent(Event, 6, Campaign), EDAWorldEventRuntimeResult::InvalidState);
        TestEqual("Conflicting stable-ID reuse does not mutate state", Campaign.NarrativeState.MutationRevision, RevisionBeforeConflict);
        TestEqual("Quest record remains singular", Campaign.NarrativeState.QuestStates.Num(), 1);
        TestEqual("Event record remains singular", Campaign.NarrativeState.EventStates.Num(), 1);
    });

    It("round-trips active quest world-event and promise state through the canonical campaign save", [this]()
    {
        FDACampaignSnapshot Campaign;
        const FDAQuestDefinition Quest = MakeMinimalQuest();
        const FDAWorldEventDefinition Event = MakeWorldEvent();
        TestEqual("Quest start mutates canonical state", FDAQuestRuntime::StartQuest(Quest, {}, 21, Campaign), EDAQuestRuntimeResult::Applied);
        TestEqual("Event start mutates canonical state", FDAWorldEventRuntime::StartEvent(Event, 21, Campaign), EDAWorldEventRuntimeResult::Applied);
        FDAPromiseRecord Promise;
        Promise.PromiseId = FGuid(1, 3, 5, 7);
        Promise.PromiseDefinitionId = TEXT("promise.preserve_eden_watershed");
        Promise.PromiserId = TEXT("leader.amara_venn");
        Promise.ConflictActionTags.Add(TEXT("action.eden.watershed_damage"));
        Promise.FulfillmentActionTags.Add(TEXT("action.eden.watershed_restored"));
        Promise.CreatedWorldTick = 21;
        FDAPromiseLedger PromiseLedger(Campaign);
        TestEqual("Promise registration mutates canonical state", PromiseLedger.RegisterPromise(Promise), EDAPromiseMutationResult::Applied);

        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Canonical schema writes narrative state", SaveService.SaveCampaign(Campaign, TEXT("narrative-state")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(TEXT("narrative-state"));
        TestTrue("Canonical schema reloads narrative state", Loaded.HasValue());
        if (!Loaded.HasValue())
        {
            return;
        }

        const FDAQuestSaveState* QuestState = Loaded.GetValue().NarrativeState.FindQuestState(Quest.QuestId);
        const FDAWorldEventSaveState* EventState = Loaded.GetValue().NarrativeState.FindEventState(Event.EventId);
        const FDAPromiseRecord* PromiseState = Loaded.GetValue().NarrativeState.FindPromiseRecord(Promise.PromiseId);
        TestNotNull("Active quest is restored", QuestState);
        TestNotNull("Active event is restored", EventState);
        TestNotNull("Active promise is restored", PromiseState);
        if (QuestState != nullptr)
        {
            TestEqual("Quest stable ID survives", QuestState->QuestId, Quest.QuestId);
            TestEqual("Quest current node survives", QuestState->CurrentNodeId, Quest.StartNodeId);
        }
        if (EventState != nullptr)
        {
            TestEqual("Event stable ID survives", EventState->EventId, Event.EventId);
            TestEqual("Event stage survives", EventState->CurrentStageId, Event.InitialStageId);
        }
        if (PromiseState != nullptr)
        {
            TestEqual("Promise stable ID survives", PromiseState->PromiseId, Promise.PromiseId);
            TestEqual("Exact promise conflict tag survives", PromiseState->ConflictActionTags, Promise.ConflictActionTags);
            TestEqual("Promise remains active", PromiseState->State, EDAPromiseState::Active);
        }
    });

    It("rejects checksummed quest-manifest and event-scope tampering on load", [this]()
    {
        FDASaveService SaveService(TestSaveDirectory);
        FDACampaignSnapshot QuestCampaign;
        TestEqual("Quest starts", FDAQuestRuntime::StartQuest(MakeMinimalQuest(), {}, 1, QuestCampaign), EDAQuestRuntimeResult::Applied);
        TestTrue("Quest fixture saves", SaveService.SaveCampaign(QuestCampaign, TEXT("tamper-quest")).IsSuccess());
        TestTrue("Quest fingerprint is tampered behind a valid checksum",
            TamperManifestWithValidChecksum(TestSaveDirectory, TEXT("tamper-quest"), true));
        const TResult<FDACampaignSnapshot, FDASaveError> QuestLoad = SaveService.LoadCampaign(TEXT("tamper-quest"));
        TestFalse("Fingerprint tamper is rejected", QuestLoad.HasValue());
        if (!QuestLoad.HasValue()) TestEqual("Fingerprint tamper is structurally invalid", QuestLoad.GetError().Code, EDASaveErrorCode::InvalidDocument);

        FDACampaignSnapshot EventCampaign;
        TestEqual("Event starts", FDAWorldEventRuntime::StartEvent(MakeWorldEvent(), 1, EventCampaign), EDAWorldEventRuntimeResult::Applied);
        TestTrue("Event fixture saves", SaveService.SaveCampaign(EventCampaign, TEXT("tamper-event")).IsSuccess());
        TestTrue("Event scope is tampered behind a valid checksum",
            TamperManifestWithValidChecksum(TestSaveDirectory, TEXT("tamper-event"), false));
        const TResult<FDACampaignSnapshot, FDASaveError> EventLoad = SaveService.LoadCampaign(TEXT("tamper-event"));
        TestFalse("Event scope tamper is rejected", EventLoad.HasValue());
    });
}
