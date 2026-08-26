#include "Campaign/DADaxtonCampaignState.h"
#include "Capture/DACaptureComponent.h"
#include "Conquest/DAConquestSystem.h"
#include "Diplomacy/DADiplomacySystem.h"
#include "Dom/JsonObject.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Save/DASaveService.h"
#include "Save/DACampaignSaveGame.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    constexpr const TCHAR* ReleaseFixturePlanArgument = TEXT("DAReleaseFixturePlan=");
    constexpr const TCHAR* ReleaseAutomationEvidenceArgument = TEXT("DAReleaseAutomationEvidence=");
    constexpr const TCHAR* ReleaseRunIdArgument = TEXT("DAReleaseRunId=");
    constexpr const TCHAR* ReleaseBuildIdArgument = TEXT("DAReleaseBuildId=");

    void LinkReleaseAsset(FDACampaignSnapshot& Campaign, FDAWorldAssetRecord& Asset,
        const FGuid CardInstanceId)
    {
        Asset.CardInstanceId = CardInstanceId;
        Campaign.CollectionState.AddInstanceWithId(CardInstanceId, Asset.CardDefinitionId,
            EDAAcquisitionSource::Conquest, Campaign.WorldState.CurrentWorldTick);
        if (FCardInstance* Card = Campaign.CollectionState.FindInstance(CardInstanceId))
        {
            Card->WorldAssetId = Asset.WorldAssetId;
            Card->RecoveryState = Asset.ConstructionState == EDAConstructionState::Ruined
                ? EDARecoveryState::Ruined : EDARecoveryState::Deployed;
        }
        if (Campaign.CitySimulationState.bInitialized
            && Asset.OwnerCivilizationId == TEXT("civilization.synara"))
        {
            FDAFacilityContext& Facility =
                Campaign.CitySimulationState.Facilities.Emplace_GetRef();
            Facility.AssetRecord = Asset;
        }
    }

    struct FDAReleaseRouteFixture
    {
        FString RouteName;
        EDAForgeweaveRoute Route = EDAForgeweaveRoute::Force;
        FString SourcePath;
        FString SourceSha256;
        FName BrokerTag;
        float Trust = 0.f;
        float Respect = 0.f;
        float Grievance = 0.f;
        FName LoyaltyTargetDefinition;
        FName LoyaltyGiftRecipient;
        float LoyaltyGiftReward = 0.f;
        EDADaxtonLeaderState LeaderState = EDADaxtonLeaderState::Prisoner;
        bool bSaveGrandForge = false;
        bool bEvacuateWorkers = false;
        bool bOfferProductionUnion = false;
    };

    struct FDAReleaseRouteAftermath
    {
        FString RouteName;
        FString SourceSha256;
        float LoyaltyBefore = 0.f;
        float Loyalty = 0.f;
        FString LoyaltyActionSha256;
        EDADaxtonLeaderState LeaderState = EDADaxtonLeaderState::Governor;
        FString HistorySha256;
        FString DamageSha256;
        FString RelationshipSha256;
        bool bReachedFirstAscension = false;
    };

    struct FDAReleaseCheckpointResult
    {
        FString Checkpoint;
        bool bPassed = false;
        FString SnapshotSha256;
    };

    FString Sha256Bytes(const TArray<uint8>& Bytes)
    {
        uint8 Digest[32] = {};
        FSHA256 Hasher;
        Hasher.Update(Bytes.GetData(), static_cast<uint64>(Bytes.Num()));
        Hasher.Final();
        Hasher.GetHash(Digest);
        FString Result;
        Result.Reserve(64);
        for (const uint8 Byte : Digest) Result += FString::Printf(TEXT("%02x"), Byte);
        return Result;
    }

    FString Sha256String(const FString& Value)
    {
        FTCHARToUTF8 Utf8(*Value);
        TArray<uint8> Bytes;
        Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
        return Sha256Bytes(Bytes);
    }

    bool LoadFileSha256(const FString& Filename, FString& OutSha256)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *Filename)) return false;
        OutSha256 = Sha256Bytes(Bytes);
        return true;
    }

    bool ParseRoute(const FString& Text, EDAForgeweaveRoute& OutRoute)
    {
        if (Text == TEXT("Force")) OutRoute = EDAForgeweaveRoute::Force;
        else if (Text == TEXT("Economic")) OutRoute = EDAForgeweaveRoute::Economic;
        else if (Text == TEXT("Influence")) OutRoute = EDAForgeweaveRoute::Influence;
        else if (Text == TEXT("Alliance")) OutRoute = EDAForgeweaveRoute::Alliance;
        else return false;
        return true;
    }

    bool ParseLeaderState(const FString& Text, EDADaxtonLeaderState& OutState)
    {
        if (Text == TEXT("Prisoner")) OutState = EDADaxtonLeaderState::Prisoner;
        else if (Text == TEXT("IndustrialAdvisor")) OutState = EDADaxtonLeaderState::IndustrialAdvisor;
        else if (Text == TEXT("Governor")) OutState = EDADaxtonLeaderState::Governor;
        else if (Text == TEXT("AlliedForgeLord")) OutState = EDADaxtonLeaderState::AlliedForgeLord;
        else return false;
        return true;
    }

    FString LeaderStateName(const EDADaxtonLeaderState State)
    {
        switch (State)
        {
        case EDADaxtonLeaderState::Prisoner: return TEXT("Prisoner");
        case EDADaxtonLeaderState::IndustrialAdvisor: return TEXT("IndustrialAdvisor");
        case EDADaxtonLeaderState::Governor: return TEXT("Governor");
        case EDADaxtonLeaderState::AlliedForgeLord: return TEXT("AlliedForgeLord");
        case EDADaxtonLeaderState::Exile: return TEXT("Exile");
        case EDADaxtonLeaderState::Dead: return TEXT("Dead");
        default: return TEXT("Unknown");
        }
    }

    bool LoadFixturePlan(const FString& Filename, FString& OutPlanSha256,
        TArray<FDAReleaseRouteFixture>& OutFixtures, FString& OutError)
    {
        FString Document;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(Document, *Filename)
            || !LoadFileSha256(Filename, OutPlanSha256)
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Document), Root)
            || !Root.IsValid())
        {
            OutError = TEXT("Release fixture plan could not be loaded.");
            return false;
        }
        double SchemaVersion = 0.0;
        FString AutomationTest;
        const TArray<TSharedPtr<FJsonValue>>* Fixtures = nullptr;
        if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion)
            || SchemaVersion != 1.0
            || !Root->TryGetStringField(TEXT("automationTest"), AutomationTest)
            || AutomationTest != TEXT("Dominion.Release.VerticalSlice")
            || !Root->TryGetArrayField(TEXT("fixtures"), Fixtures)
            || Fixtures == nullptr || Fixtures->Num() != 4)
        {
            OutError = TEXT("Release fixture plan identity/count is invalid.");
            return false;
        }
        static const TCHAR* ExpectedRoutes[] = {TEXT("Force"), TEXT("Economic"), TEXT("Influence"), TEXT("Alliance")};
        OutFixtures.Reset(4);
        for (int32 Index = 0; Index < Fixtures->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> Row = (*Fixtures)[Index].IsValid()
                ? (*Fixtures)[Index]->AsObject() : nullptr;
            const TSharedPtr<FJsonObject>* Relationship = nullptr;
            const TSharedPtr<FJsonObject>* LoyaltyAction = nullptr;
            const TSharedPtr<FJsonObject>* Outcome = nullptr;
            FDAReleaseRouteFixture Fixture;
            double Trust = 0.0;
            double Respect = 0.0;
            double Grievance = 0.0;
            FString BrokerTag;
            FString LoyaltySystem;
            FString LoyaltyOutcome;
            FString LoyaltyTargetDefinition;
            FString LoyaltyRecipient;
            double LoyaltyReward = 0.0;
            FString LeaderState;
            if (!Row.IsValid()
                || !Row->TryGetStringField(TEXT("route"), Fixture.RouteName)
                || Fixture.RouteName != ExpectedRoutes[Index]
                || !ParseRoute(Fixture.RouteName, Fixture.Route)
                || !Row->TryGetStringField(TEXT("source"), Fixture.SourcePath)
                || !Row->TryGetStringField(TEXT("sourceSha256"), Fixture.SourceSha256)
                || !Row->TryGetStringField(TEXT("brokerTag"), BrokerTag)
                || !Row->TryGetObjectField(TEXT("relationship"), Relationship)
                || Relationship == nullptr || !Relationship->IsValid()
                || !(*Relationship)->TryGetNumberField(TEXT("trust"), Trust)
                || !(*Relationship)->TryGetNumberField(TEXT("respect"), Respect)
                || !(*Relationship)->TryGetNumberField(TEXT("grievance"), Grievance)
                || !Row->TryGetObjectField(TEXT("loyaltyAction"), LoyaltyAction)
                || LoyaltyAction == nullptr || !LoyaltyAction->IsValid()
                || !(*LoyaltyAction)->TryGetStringField(TEXT("system"), LoyaltySystem)
                || LoyaltySystem != TEXT("UDACaptureComponent")
                || !(*LoyaltyAction)->TryGetStringField(TEXT("outcome"), LoyaltyOutcome)
                || LoyaltyOutcome != TEXT("Gift")
                || !(*LoyaltyAction)->TryGetStringField(
                    TEXT("targetDefinition"), LoyaltyTargetDefinition)
                || !(*LoyaltyAction)->TryGetStringField(TEXT("recipient"), LoyaltyRecipient)
                || !(*LoyaltyAction)->TryGetNumberField(TEXT("giftLoyaltyReward"), LoyaltyReward)
                || LoyaltyReward <= 0.0 || LoyaltyReward > 100.0
                || !Row->TryGetObjectField(TEXT("daxtonOutcome"), Outcome)
                || Outcome == nullptr || !Outcome->IsValid()
                || !(*Outcome)->TryGetStringField(TEXT("leaderState"), LeaderState)
                || !ParseLeaderState(LeaderState, Fixture.LeaderState)
                || !(*Outcome)->TryGetBoolField(TEXT("saveGrandForge"), Fixture.bSaveGrandForge)
                || !(*Outcome)->TryGetBoolField(TEXT("evacuateWorkers"), Fixture.bEvacuateWorkers)
                || !(*Outcome)->TryGetBoolField(TEXT("offerProductionUnion"), Fixture.bOfferProductionUnion))
            {
                OutError = FString::Printf(TEXT("Release fixture plan row %d is invalid."), Index);
                return false;
            }
            FString ActualSourceSha256;
            const FString SourceFile = FPaths::ConvertRelativePathToFull(
                FPaths::ProjectDir(), Fixture.SourcePath);
            if (!LoadFileSha256(SourceFile, ActualSourceSha256)
                || ActualSourceSha256 != Fixture.SourceSha256)
            {
                OutError = FString::Printf(TEXT("%s source bytes do not match the generated plan."), *Fixture.RouteName);
                return false;
            }
            Fixture.BrokerTag = FName(*BrokerTag);
            Fixture.Trust = static_cast<float>(Trust);
            Fixture.Respect = static_cast<float>(Respect);
            Fixture.Grievance = static_cast<float>(Grievance);
            Fixture.LoyaltyTargetDefinition = FName(*LoyaltyTargetDefinition);
            Fixture.LoyaltyGiftRecipient = FName(*LoyaltyRecipient);
            Fixture.LoyaltyGiftReward = static_cast<float>(LoyaltyReward);
            OutFixtures.Add(Fixture);
        }
        return true;
    }

    void AddHistory(FDACampaignSnapshot& Campaign, const FName Tag)
    {
        Campaign.HistoryTags.AddUnique(Tag);
        Campaign.HistoryTags.Sort([](const FName Left, const FName Right)
        {
            return Left.LexicalLess(Right);
        });
    }

    FDAQuestSaveState& AddQuest(FDACampaignSnapshot& Campaign, const FName QuestId,
        const bool bCompleted)
    {
        FDAQuestSaveState& Quest = Campaign.NarrativeState.QuestStates.Emplace_GetRef();
        Quest.QuestId = QuestId;
        Quest.DefinitionVersion = 1;
        Quest.DefinitionManifest.QuestId = QuestId;
        Quest.DefinitionManifest.SourceDefinitionId = FName(*(TEXT("test.release.definition.") + QuestId.ToString()));
        Quest.DefinitionManifest.StartNodeId = TEXT("start");
        FDAQuestNodeDefinition& Start = Quest.DefinitionManifest.Nodes.Emplace_GetRef();
        Start.NodeId = TEXT("start");
        Start.Type = EDAQuestNodeType::Start;
        Start.SourceDefinitionId = TEXT("test.release.node.start");
        FDAQuestEdgeDefinition& Edge = Start.Edges.Emplace_GetRef();
        Edge.BranchTag = TEXT("advance");
        Edge.TargetNodeId = bCompleted ? FName(TEXT("resolution")) : FName(TEXT("objective"));
        if (!bCompleted)
        {
            FDAQuestNodeDefinition& Objective = Quest.DefinitionManifest.Nodes.Emplace_GetRef();
            Objective.NodeId = TEXT("objective");
            Objective.Type = EDAQuestNodeType::Objective;
            Objective.SourceDefinitionId = TEXT("test.release.node.objective");
            FDAQuestEdgeDefinition& ObjectiveEdge = Objective.Edges.Emplace_GetRef();
            ObjectiveEdge.BranchTag = TEXT("complete");
            ObjectiveEdge.TargetNodeId = TEXT("resolution");
        }
        FDAQuestNodeDefinition& Resolution = Quest.DefinitionManifest.Nodes.Emplace_GetRef();
        Resolution.NodeId = TEXT("resolution");
        Resolution.Type = EDAQuestNodeType::Resolution;
        Resolution.SourceDefinitionId = TEXT("test.release.node.resolution");
        Quest.DefinitionManifest.RefreshFingerprint();
        Quest.CurrentNodeId = bCompleted ? FName(TEXT("resolution")) : FName(TEXT("objective"));
        Quest.ProgressState = bCompleted ? EDAQuestProgressState::Completed : EDAQuestProgressState::Active;
        Quest.CompletedNodeIds.Add(TEXT("start"));
        FDAQuestNodeTransitionRecord& Transition = Quest.NodeTransitionRecords.Emplace_GetRef();
        Transition.CompletedNodeId = TEXT("start");
        Transition.EnteredNodeId = Quest.CurrentNodeId;
        return Quest;
    }

    FDARegionState& EnsureRegion(FDACampaignSnapshot& Campaign, const FName RegionId,
        const FName OwnerId)
    {
        FDARegionState* Existing = Campaign.WorldState.FindRegion(RegionId);
        if (Existing != nullptr)
        {
            Existing->OwnerId = OwnerId;
            return *Existing;
        }
        FDARegionState& Region = Campaign.WorldState.Regions.Emplace_GetRef();
        Region.RegionId = RegionId;
        Region.OwnerId = OwnerId;
        return Region;
    }

    FDADiplomaticRelationship& ResetForgeweaveRelationship(FDACampaignSnapshot& Campaign,
        const FDAReleaseRouteFixture& Fixture)
    {
        FDADiplomaticRelationship* Relationship = Campaign.WorldState.Diplomacy.FindRelationship(
            TEXT("relationship.synara.forgeweave"));
        if (Relationship == nullptr)
        {
            Relationship = &Campaign.WorldState.Diplomacy.Relationships.Emplace_GetRef();
            Relationship->RelationshipId = TEXT("relationship.synara.forgeweave");
        }
        Relationship->Trust = 0.f;
        Relationship->Respect = 0.f;
        Relationship->Fear = 0.f;
        Relationship->Dependence = 0.f;
        Relationship->Compatibility = 0.f;
        Relationship->Grievance = 0.f;
        Relationship->ReasonLedger.Reset();
        const auto AddReason = [&Campaign, Relationship](const EDADiplomaticMetric Metric,
            const float Magnitude, const FName MutationId)
        {
            if (Magnitude == 0.f) return;
            FDADiplomaticReason& Reason = Relationship->ReasonLedger.Emplace_GetRef();
            Reason.MutationId = MutationId;
            Reason.SourceTag = TEXT("test.release.fixture.canonical");
            Reason.Metric = Metric;
            Reason.Magnitude = Magnitude;
            Reason.WorldTick = Campaign.WorldState.CurrentWorldTick;
            if (Metric == EDADiplomaticMetric::Trust) Relationship->Trust += Magnitude;
            else if (Metric == EDADiplomaticMetric::Respect) Relationship->Respect += Magnitude;
            else if (Metric == EDADiplomaticMetric::Grievance) Relationship->Grievance += Magnitude;
            else if (Metric == EDADiplomaticMetric::Compatibility) Relationship->Compatibility += Magnitude;
        };
        AddReason(EDADiplomaticMetric::Trust, Fixture.Trust,
            FName(*(TEXT("reason.release.") + Fixture.RouteName.ToLower() + TEXT(".trust"))));
        AddReason(EDADiplomaticMetric::Respect, Fixture.Respect,
            FName(*(TEXT("reason.release.") + Fixture.RouteName.ToLower() + TEXT(".respect"))));
        AddReason(EDADiplomaticMetric::Grievance, Fixture.Grievance,
            FName(*(TEXT("reason.release.") + Fixture.RouteName.ToLower() + TEXT(".grievance"))));
        if (Fixture.Route == EDAForgeweaveRoute::Alliance)
            AddReason(EDADiplomaticMetric::Compatibility, 85.f,
                TEXT("reason.release.alliance.compatibility"));
        return *Relationship;
    }

    void AddGrandForgeAuthority(FDACampaignSnapshot& Campaign, const int32 RouteIndex)
    {
        TSet<FGuid> RemovedForgeIds;
        TSet<FGuid> RemovedForgeCardIds;
        for (const FDAWorldAssetRecord& Asset : Campaign.WorldAssets)
            if (Asset.CardDefinitionId == TEXT("forgeweave.grand_forge"))
            {
                RemovedForgeIds.Add(Asset.WorldAssetId);
                if (Asset.CardInstanceId.IsValid()) RemovedForgeCardIds.Add(Asset.CardInstanceId);
            }
        Campaign.WorldAssets.RemoveAll([](const FDAWorldAssetRecord& Asset)
        { return Asset.CardDefinitionId == TEXT("forgeweave.grand_forge"); });
        for (const FGuid CardId : RemovedForgeCardIds)
            Campaign.CollectionState.Instances.Remove(CardId);
        Campaign.OperationConflict.StructuralDamageRecords.RemoveAll(
            [&RemovedForgeIds](const FDAStructuralDamageRecord& Damage)
            { return RemovedForgeIds.Contains(Damage.WorldAssetId); });
        Campaign.LiveSignals.JobOpenings.RemoveAll([](const FDACampaignJobOpeningSignal& Opening)
        { return Opening.JobId == TEXT("job.forgeweave.grand_forge.worker"); });
        Campaign.LiveSignals.JobAssignments.RemoveAll([](const FDACampaignJobAssignmentSignal& Assignment)
        { return Assignment.JobId == TEXT("job.forgeweave.grand_forge.worker"); });
        Campaign.CitySimulationState.JobOpenings.RemoveAll([](const FDAJobOpening& Opening)
        { return Opening.JobId == TEXT("job.forgeweave.grand_forge.worker"); });
        Campaign.CitySimulationState.JobAssignments.RemoveAll([](const FDAJobAssignment& Assignment)
        { return Assignment.JobId == TEXT("job.forgeweave.grand_forge.worker"); });
        const FGuid ForgeId(28, RouteIndex + 1, 100, 1);
        FDAWorldAssetRecord& Forge = Campaign.WorldAssets.Emplace_GetRef();
        Forge.WorldAssetId = ForgeId;
        Forge.CardDefinitionId = TEXT("forgeweave.grand_forge");
        Forge.CityId = TEXT("city.ironheart");
        Forge.OwnerCivilizationId = TEXT("civilization.forgeweave");
        Forge.ConstructionState = EDAConstructionState::Operational;
        Forge.StructuralIntegrity = 100.f;
        LinkReleaseAsset(Campaign, Forge, FGuid(28, RouteIndex + 1, 100, 2));
        FDAStructuralDamageRecord& Damage =
            Campaign.OperationConflict.StructuralDamageRecords.Emplace_GetRef();
        Damage.WorldAssetId = ForgeId;
        Damage.CardDefinitionId = Forge.CardDefinitionId;
        Damage.Modules = {
            FDAStructureModuleHealthRecord(TEXT("module.coolant"), 100.f, true),
            FDAStructureModuleHealthRecord(TEXT("module.production"), 100.f, true),
            FDAStructureModuleHealthRecord(TEXT("module.structure"), 100.f, false)};
        FDACampaignCitizenSignal* Worker = Campaign.LiveSignals.Citizens.FindByPredicate(
            [](const FDACampaignCitizenSignal& Row)
            { return Row.CitizenId == TEXT("citizen.forgeweave.mara_kest"); });
        if (Worker == nullptr)
        {
            Worker = &Campaign.LiveSignals.Citizens.Emplace_GetRef();
            Worker->CitizenId = TEXT("citizen.forgeweave.mara_kest");
        }
        Worker->CityId = TEXT("city.ironheart");
        Worker->JobId = TEXT("job.forgeweave.grand_forge.worker");
        FDACitizenRecord* CityWorker = Campaign.CitySimulationState.Citizens.FindByPredicate(
            [](const FDACitizenRecord& Row)
            { return Row.CitizenId == TEXT("citizen.forgeweave.mara_kest"); });
        if (CityWorker != nullptr)
        {
            CityWorker->CityId = Worker->CityId;
            CityWorker->JobId = Worker->JobId;
        }
        FDACampaignJobOpeningSignal& Opening = Campaign.LiveSignals.JobOpenings.Emplace_GetRef();
        Opening.JobId = Worker->JobId;
        Opening.CityId = Worker->CityId;
        Opening.FacilityWorldAssetId = ForgeId;
        Opening.OpenPositions = 4;
        FDACampaignJobAssignmentSignal& Assignment = Campaign.LiveSignals.JobAssignments.Emplace_GetRef();
        Assignment.CitizenId = Worker->CitizenId;
        Assignment.JobId = Worker->JobId;
        Assignment.FacilityWorldAssetId = ForgeId;
        FDAJobOpening& CityOpening = Campaign.CitySimulationState.JobOpenings.Emplace_GetRef();
        CityOpening.JobId = Opening.JobId;
        CityOpening.CityId = Opening.CityId;
        CityOpening.FacilityWorldAssetId = Opening.FacilityWorldAssetId;
        CityOpening.OpenPositions = Opening.OpenPositions;
        FDAJobAssignment& CityAssignment = Campaign.CitySimulationState.JobAssignments.Emplace_GetRef();
        CityAssignment.CitizenId = Assignment.CitizenId;
        CityAssignment.JobId = Assignment.JobId;
        CityAssignment.FacilityWorldAssetId = Assignment.FacilityWorldAssetId;
        CityAssignment.MatchQuality = EDAJobMatchQuality::Acceptable;
        CityAssignment.OutputMultiplier = 0.85f;
        ++Campaign.LiveSignals.MutationRevision;
    }

    void AddCompletedContract(FDACampaignSnapshot& Campaign, const int32 Index)
    {
        FDATradeContractState& Contract = Campaign.WorldState.Trade.Contracts.Emplace_GetRef();
        Contract.ContractId = FName(*FString::Printf(TEXT("contract.release.forge.%d"), Index));
        Contract.RelationshipId = TEXT("relationship.synara.forgeweave");
        Contract.RouteId = TEXT("route.synara_ironheart_freight");
        Contract.SourceRegionId = TEXT("region.synara_frontier");
        Contract.DestinationRegionId = TEXT("region.ironheart");
        Contract.GoodId = TEXT("good.machine_components");
        Contract.SuccessfulDeliveryCount = 1;
        Contract.DeliveredQuantity = 10;
        Contract.bCompleted = true;
        FDATradeDeliveryRecord& Delivery = Campaign.WorldState.Trade.Deliveries.Emplace_GetRef();
        Delivery.DeliveryId = FName(*FString::Printf(TEXT("delivery.release.forge.%d"), Index));
        Delivery.ContractId = Contract.ContractId;
        Delivery.GoodId = Contract.GoodId;
        Delivery.Quantity = 10;
        Delivery.WorldTick = Campaign.WorldState.CurrentWorldTick;
    }

    bool AddRouteEvidence(UDAWorldStateSubsystem& World, const FDAReleaseRouteFixture& Fixture,
        const int32 RouteIndex, FString& OutError)
    {
        if (Fixture.Route == EDAForgeweaveRoute::Economic
            || Fixture.Route == EDAForgeweaveRoute::Influence
            || Fixture.Route == EDAForgeweaveRoute::Alliance)
        {
            FDACampaignSnapshot CrisisCandidate = World.GetPersistentCampaign();
            CrisisCandidate.WorldState.Forgeweave.ResourceHunger = 80.f;
            if (!World.RestorePersistentCampaign(CrisisCandidate) || !World.AdvanceWorldTicks(1))
            {
                OutError = TEXT("Foundry Shortage source authority could not reach its real warning state.");
                return false;
            }
            const EDAFoundryShortageResolution Resolution = Fixture.Route == EDAForgeweaveRoute::Economic
                ? EDAFoundryShortageResolution::MarketExploitation
                : EDAFoundryShortageResolution::BrokeredCompact;
            if (World.ResolveFoundryShortage(FGuid(28, RouteIndex + 1, 1, 1), Resolution)
                != EDAFoundryShortageActionResult::Applied)
            {
                OutError = TEXT("Foundry Shortage source authority did not resolve through its campaign owner.");
                return false;
            }
        }

        FDACampaignSnapshot Campaign = World.GetPersistentCampaign();
        EnsureRegion(Campaign, TEXT("region.ironheart"), TEXT("civilization.forgeweave"));
        FDADiplomaticRelationship& Relationship = ResetForgeweaveRelationship(Campaign, Fixture);
        AddHistory(Campaign, Fixture.BrokerTag);
        AddGrandForgeAuthority(Campaign, RouteIndex);
        Campaign.WorldState.Forgeweave.Population = FMath::Max(60, Campaign.WorldState.Forgeweave.Population);
        Campaign.WorldState.Forgeweave.ProductionReserve = 50.f;
        Campaign.WorldState.Forgeweave.ActiveIndustrialThroughput = 20.f;
        Campaign.WorldState.Forgeweave.ResourceHunger = 10.f;
        Campaign.WorldState.Forgeweave.LogisticsEfficiency = 80.f;

        switch (Fixture.Route)
        {
        case EDAForgeweaveRoute::Force:
        {
            FDARegionState& Ironheart = EnsureRegion(Campaign, TEXT("region.ironheart"), TEXT("civilization.synara"));
            Ironheart.PersistentDelta.StateTags = {
                TEXT("control_zone.synara.iron_gate"), TEXT("control_zone.synara.grand_forge")};
            for (int32 Index = 0; Index < 2; ++Index)
            {
                FDAWorldAssetRecord& Asset = Campaign.WorldAssets.Emplace_GetRef();
                Asset.WorldAssetId = FGuid(28, 1, 200, Index + 1);
                Asset.CardDefinitionId = Index == 0
                    ? TEXT("forgeweave.heavy_carrier") : TEXT("forgeweave.command_bastion");
                Asset.OwnerCivilizationId = TEXT("civilization.synara");
                Asset.CityId = TEXT("city.ironheart");
                Asset.ConstructionState = EDAConstructionState::Operational;
                Asset.StructuralIntegrity = 100.f;
                LinkReleaseAsset(Campaign, Asset,
                    FGuid(28, 1, 201, Index + 1));
                FDACaptureRecord& Capture = Campaign.OperationConflict.CaptureRecords.Emplace_GetRef();
                Capture.WorldAssetId = Asset.WorldAssetId;
                Capture.OriginalOwnerCivilizationId = TEXT("civilization.forgeweave");
                Capture.CapturingCivilizationId = TEXT("civilization.synara");
                Capture.RequiredCaptureTimeSeconds = 20.f;
                Capture.CaptureProgressSeconds = 20.f;
                Capture.bCaptureCompleted = true;
                Capture.bOutcomeResolved = true;
                Capture.bRewardsGranted = true;
                Capture.bIntegrationRequired = true;
                Capture.Outcome = EDACaptureOutcome::Preserve;
                Capture.History.Add(TEXT("capture.preserved"));
            }
            AddHistory(Campaign, TEXT("forgeweave_elite_defeated"));
            AddQuest(Campaign, TEXT("quest.operation_iron_veil"), true);
            AddHistory(Campaign, TEXT("daxton_encounter_resolved"));
            AddHistory(Campaign, TEXT("daxton_surrendered"));
            FDAWorldAssetRecord* GrandForge = Campaign.WorldAssets.FindByPredicate(
                [](const FDAWorldAssetRecord& Asset)
                { return Asset.CardDefinitionId == TEXT("forgeweave.grand_forge"); });
            FDAStructuralDamageRecord* Damage = GrandForge == nullptr ? nullptr
                : Campaign.OperationConflict.FindStructuralDamageRecord(GrandForge->WorldAssetId);
            if (GrandForge == nullptr || Damage == nullptr)
            {
                OutError = TEXT("Force fixture lost its canonical Grand Forge damage authority.");
                return false;
            }
            GrandForge->StructuralIntegrity = 80.f;
            Damage->Modules[2].CurrentHealth = 80.f;
            break;
        }
        case EDAForgeweaveRoute::Economic:
            for (int32 Index = 0; Index < 4; ++Index) AddCompletedContract(Campaign, Index);
            EnsureRegion(Campaign, TEXT("region.freight_corridor"), TEXT("civilization.synara"));
            Relationship.Dependence = 70.f;
            {
                FDADiplomaticReason& Dependence = Relationship.ReasonLedger.Emplace_GetRef();
                Dependence.MutationId = TEXT("reason.release.economic.dependence");
                Dependence.SourceTag = TEXT("test.release.fixture.canonical");
                Dependence.Metric = EDADiplomaticMetric::Dependence;
                Dependence.Magnitude = 70.f;
                Dependence.WorldTick = Campaign.WorldState.CurrentWorldTick;
            }
            AddQuest(Campaign, TEXT("quest.supply_noose"), true);
            AddHistory(Campaign, TEXT("daxton_restructuring_resolved"));
            break;
        case EDAForgeweaveRoute::Influence:
            AddQuest(Campaign, TEXT("quest.workers_signal"), true);
            AddHistory(Campaign, TEXT("grand_forge_preserved"));
            AddHistory(Campaign, TEXT("mara_evidence_exposed"));
            AddHistory(Campaign, TEXT("mara_numbers_worker_coalition"));
            AddHistory(Campaign, TEXT("workers_protected"));
            {
                const double Current = Campaign.SynaraState.FactionSupport.FindRef(
                    TEXT("faction.synara.human_agency"));
                if (!FMath::IsNearlyEqual(Current, 70.0)
                    && !Campaign.SynaraState.ApplyFactionSupportReason(
                        TEXT("action.release.influence.human_agency"),
                        TEXT("faction.synara.human_agency"), 70.0 - Current,
                        Campaign.WorldState.CurrentWorldTick))
                {
                    OutError = TEXT("Influence fixture could not commit canonical faction support.");
                    return false;
                }
            }
            break;
        case EDAForgeweaveRoute::Alliance:
            AddQuest(Campaign, TEXT("quest.third_foundry"), true);
            AddHistory(Campaign, TEXT("forge_relic_voluntary_transfer"));
            AddHistory(Campaign, TEXT("joint_forgeweave_crisis_success"));
            break;
        }
        return World.RestorePersistentCampaign(Campaign)
            || (OutError = TEXT("Pre-final-commit fixture failed full campaign validation."), false);
    }

    bool ApplyPostConflictLoyaltyAction(UDAWorldStateSubsystem& World,
        const FDAReleaseRouteFixture& Fixture, const int32 RouteIndex,
        float& OutLoyaltyBefore, FString& OutActionSha256, FString& OutError)
    {
        UDACampaignSaveGame* CampaignOwner = NewObject<UDACampaignSaveGame>();
        CampaignOwner->Snapshot = World.GetPersistentCampaign();
        const FGuid CapturedAssetId(28, RouteIndex + 1, 400, 1);
        if (CampaignOwner->Snapshot.FindWorldAssetRecord(CapturedAssetId) != nullptr
            || CampaignOwner->Snapshot.OperationConflict.FindCaptureRecord(CapturedAssetId) != nullptr)
        {
            OutError = TEXT("Release capture action collided with an existing authority record.");
            return false;
        }

        FDAWorldAssetRecord& Asset = CampaignOwner->Snapshot.WorldAssets.Emplace_GetRef();
        Asset.WorldAssetId = CapturedAssetId;
        Asset.CardDefinitionId = Fixture.LoyaltyTargetDefinition;
        Asset.OwnerCivilizationId = TEXT("civilization.forgeweave");
        Asset.CityId = TEXT("city.ironheart");
        Asset.ConstructionState = EDAConstructionState::Disabled;
        Asset.StructuralIntegrity = 10.f;
        LinkReleaseAsset(CampaignOwner->Snapshot, Asset,
            FGuid(28, RouteIndex + 1, 400, 2));

        FDACaptureRecord& Capture =
            CampaignOwner->Snapshot.OperationConflict.CaptureRecords.Emplace_GetRef();
        Capture.WorldAssetId = CapturedAssetId;
        Capture.OriginalOwnerCivilizationId = Asset.OwnerCivilizationId;
        Capture.GiftLoyaltyReward = Fixture.LoyaltyGiftReward;
        Capture.AllowedGiftRecipients.Add(FDACaptureGiftRecipientRecord(
            Fixture.LoyaltyGiftRecipient, EDAGiftRecipientRelationship::LocalAuthority));

        FString ValidationError;
        if (!CampaignOwner->Snapshot.Validate(ValidationError))
        {
            OutError = TEXT("Release capture input is not a valid pre-action campaign: ")
                + ValidationError;
            return false;
        }

        OutLoyaltyBefore = CampaignOwner->Snapshot.OperationConflict.Resources.PostConflictLoyalty;
        UDACaptureComponent* CaptureAuthority = NewObject<UDACaptureComponent>();
        FDACaptureInteractionContext Interaction;
        Interaction.InteractionId = FGuid(28, RouteIndex + 1, 401, 1);
        Interaction.CaptureActorId = FGuid(28, RouteIndex + 1, 401, 2);
        Interaction.AgentRole = EDACaptureAgentRole::Founder;
        Interaction.bActorPresent = true;
        if (!CaptureAuthority->InitializeFromCampaign(*CampaignOwner, CapturedAssetId)
            || !CaptureAuthority->BeginCapture(Interaction, TEXT("civilization.synara"))
            || !CaptureAuthority->AdvanceCapture(
                UDACaptureComponent::BaseCaptureTimeSeconds, Interaction)
            || !CaptureAuthority->ResolveOutcome(
                EDACaptureOutcome::Gift, Fixture.LoyaltyGiftRecipient))
        {
            OutError = TEXT("Production capture authority rejected the route Loyalty action.");
            return false;
        }

        const FDAWorldAssetRecord* CommittedAsset =
            CampaignOwner->Snapshot.FindWorldAssetRecord(CapturedAssetId);
        const FDACaptureRecord* CommittedCapture =
            CampaignOwner->Snapshot.OperationConflict.FindCaptureRecord(CapturedAssetId);
        const float LoyaltyAfter =
            CampaignOwner->Snapshot.OperationConflict.Resources.PostConflictLoyalty;
        if (CommittedAsset == nullptr || CommittedCapture == nullptr
            || CommittedAsset->OwnerCivilizationId != Fixture.LoyaltyGiftRecipient
            || CommittedCapture->Outcome != EDACaptureOutcome::Gift
            || !CommittedCapture->bCaptureCompleted
            || !CommittedCapture->bOutcomeResolved
            || !CommittedCapture->bRewardsGranted
            || !CommittedCapture->History.Contains(TEXT("capture.gifted"))
            || !CampaignOwner->Snapshot.HistoryTags.Contains(TEXT("capture.gifted"))
            || !FMath::IsNearlyEqual(
                LoyaltyAfter - OutLoyaltyBefore, Fixture.LoyaltyGiftReward, 0.001f))
        {
            OutError = TEXT("Production capture result lacks exact causal Loyalty proof.");
            return false;
        }

        const FString CausalMaterial = FString::Printf(
            TEXT("%s|%s|%s|%s|%0.3f|%0.3f|%0.3f|%d|%d|%d|capture.gifted"),
            *CapturedAssetId.ToString(EGuidFormats::Digits),
            *CommittedAsset->CardDefinitionId.ToString(),
            *CommittedCapture->OriginalOwnerCivilizationId.ToString(),
            *CommittedAsset->OwnerCivilizationId.ToString(),
            OutLoyaltyBefore, CommittedCapture->GiftLoyaltyReward, LoyaltyAfter,
            CommittedCapture->bCaptureCompleted ? 1 : 0,
            CommittedCapture->bOutcomeResolved ? 1 : 0,
            CommittedCapture->bRewardsGranted ? 1 : 0);
        OutActionSha256 = Sha256String(CausalMaterial);
        if (OutActionSha256.Len() != 64
            || !World.RestorePersistentCampaign(CampaignOwner->Snapshot))
        {
            OutError = TEXT("Production capture result could not commit through the campaign owner.");
            return false;
        }
        return true;
    }

    bool RoundTripCheckpoint(const FDACampaignSnapshot& Campaign, const FString& Checkpoint,
        FDAReleaseCheckpointResult& OutResult, FString& OutError)
    {
        OutResult = {};
        OutResult.Checkpoint = Checkpoint;
        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
            TEXT("ReleaseSaveCheckpoints"), Checkpoint,
            FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory);
        const FDASaveResult Saved = Saves.SaveCampaign(Campaign, Checkpoint);
        TResult<FDACampaignSnapshot, FDASaveError> Loaded = Saves.LoadCampaign(Checkpoint);
        FString ValidationError;
        const FString SourcePath = FPaths::Combine(Directory, Checkpoint + TEXT(".dasave"));
        const FString ReloadedSlot = Checkpoint + TEXT("_reloaded");
        const FString ReloadedPath = FPaths::Combine(Directory, ReloadedSlot + TEXT(".dasave"));
        FDASaveResult ReloadedSave;
        if (Saved.IsSuccess() && Loaded.HasValue()
            && Loaded.GetValue().Validate(ValidationError))
        {
            ReloadedSave = Saves.SaveCampaign(Loaded.GetValue(), ReloadedSlot);
        }
        TArray<uint8> SourceBytes;
        TArray<uint8> ReloadedBytes;
        OutResult.bPassed = Saved.IsSuccess() && Loaded.HasValue()
            && ValidationError.IsEmpty() && ReloadedSave.IsSuccess()
            && FFileHelper::LoadFileToArray(SourceBytes, *SourcePath)
            && FFileHelper::LoadFileToArray(ReloadedBytes, *ReloadedPath)
            && SourceBytes == ReloadedBytes;
        if (OutResult.bPassed) OutResult.SnapshotSha256 = Sha256Bytes(SourceBytes);
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*Directory);
        if (!OutResult.bPassed)
        {
            FString Detail = ValidationError;
            if (!Saved.IsSuccess()) Detail = Saved.Error.Message;
            else if (!Loaded.HasValue()) Detail = Loaded.GetError().Message;
            else if (!ReloadedSave.IsSuccess()) Detail = ReloadedSave.Error.Message;
            else if (Detail.IsEmpty()) Detail = TEXT("canonical save bytes changed after reload");
            OutError = FString::Printf(TEXT("Save checkpoint %s failed: %s"),
                *Checkpoint, *Detail);
        }
        return OutResult.bPassed;
    }

    FDACampaignSnapshot MakeFirstCityCheckpoint()
    {
        FDACampaignSnapshot Campaign;
        FCardInstance Card;
        Card.InstanceId = FGuid(28, 50, 1, 1);
        Card.DefinitionId = TEXT("synara.adaptive_habitat");
        Card.WorldAssetId = FGuid(28, 50, 1, 2);
        Campaign.CollectionState.Instances.Add(Card.InstanceId, Card);
        Campaign.DeckState.SetInstanceIds({Card.InstanceId});
        FDAWorldAssetRecord& Habitat = Campaign.WorldAssets.Emplace_GetRef();
        Habitat.WorldAssetId = Card.WorldAssetId;
        Habitat.CardInstanceId = Card.InstanceId;
        Habitat.CardDefinitionId = Card.DefinitionId;
        Habitat.CityId = TEXT("player_capital");
        Habitat.GridOrigin = FIntPoint(14, 22);
        Habitat.ConstructionState = EDAConstructionState::Operational;
        Habitat.ConstructionCyclesCompleted = 3;
        Habitat.ConstructionCyclesRequired = 3;
        Habitat.StructuralIntegrity = 100.f;
        Habitat.OwnerCivilizationId = TEXT("civilization.synara");
        AddHistory(Campaign, TEXT("history.first_city_built"));
        return Campaign;
    }

    FString HistoryFingerprint(const FDACampaignSnapshot& Campaign)
    {
        FString Material;
        for (const FName Tag : Campaign.HistoryTags) Material += Tag.ToString() + TEXT("\n");
        return Sha256String(Material);
    }

    FString DamageFingerprint(const FDACampaignSnapshot& Campaign)
    {
        TArray<FString> Rows;
        for (const FDAStructuralDamageRecord& Damage : Campaign.OperationConflict.StructuralDamageRecords)
        {
            const FDAWorldAssetRecord* Asset = Campaign.WorldAssets.FindByPredicate(
                [&Damage](const FDAWorldAssetRecord& Candidate)
                { return Candidate.WorldAssetId == Damage.WorldAssetId; });
            FString Row = FString::Printf(TEXT("%s|%0.3f|%d|%s"),
                *Damage.CardDefinitionId.ToString(),
                Asset == nullptr ? -1.f : Asset->StructuralIntegrity,
                Asset == nullptr ? -1 : static_cast<int32>(Asset->ConstructionState),
                Damage.bProductionDisabled ? TEXT("disabled") : TEXT("enabled"));
            for (const FDAStructureModuleHealthRecord& Module : Damage.Modules)
                Row += FString::Printf(TEXT("|%s:%0.3f:%d"), *Module.ModuleId.ToString(),
                    Module.CurrentHealth, static_cast<int32>(Module.State));
            Rows.Add(Row);
        }
        Rows.Sort();
        return Sha256String(FString::Join(Rows, TEXT("\n")));
    }

    FString RelationshipFingerprint(const FDACampaignSnapshot& Campaign)
    {
        const FDADiplomaticRelationship* Relationship =
            Campaign.WorldState.Diplomacy.FindRelationship(TEXT("relationship.synara.forgeweave"));
        if (Relationship == nullptr) return Sha256String(TEXT("missing"));
        FString Material = FString::Printf(TEXT("%0.3f|%0.3f|%0.3f|%0.3f|%0.3f"),
            Relationship->Trust, Relationship->Respect, Relationship->Dependence,
            Relationship->Compatibility, Relationship->Grievance);
        for (const FDADiplomaticReason& Reason : Relationship->ReasonLedger)
            Material += FString::Printf(TEXT("|%s:%d:%0.3f"), *Reason.MutationId.ToString(),
                static_cast<int32>(Reason.Metric), Reason.Magnitude);
        return Sha256String(Material);
    }

    template <typename TValue>
    bool IsPairwiseDistinct(const TArray<TValue>& Values)
    {
        TSet<TValue> Unique;
        for (const TValue& Value : Values) Unique.Add(Value);
        return Unique.Num() == Values.Num();
    }

    bool ExecuteRoute(const FDAReleaseRouteFixture& RouteFixture, const int32 RouteIndex,
        FDAReleaseRouteAftermath& OutAftermath,
        TArray<FDAReleaseCheckpointResult>& InOutCheckpoints, FString& OutError)
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        if (World == nullptr)
        {
            OutError = TEXT("Production campaign owner is unavailable.");
            return false;
        }
        if (!AddRouteEvidence(*World, RouteFixture, RouteIndex, OutError)
            || !ApplyPostConflictLoyaltyAction(*World, RouteFixture, RouteIndex,
                OutAftermath.LoyaltyBefore, OutAftermath.LoyaltyActionSha256, OutError)
            || !World->CompleteForgeweaveConquestRoute(
                FGuid(28, RouteIndex + 1, 10, 1), RouteFixture.Route, OutError)
            || !World->StartDaxtonEncounter(FGuid(28, RouteIndex + 1, 20, 1), OutError)
            || !World->ApplyDaxtonInteraction(FGuid(28, RouteIndex + 1, 20, 2),
                EDADaxtonInteraction::Damage, 40.f, OutError))
            return false;

        if (!InOutCheckpoints.ContainsByPredicate([](const FDAReleaseCheckpointResult& Result)
            { return Result.Checkpoint == TEXT("daxton_phase_transition"); }))
        {
            FDAReleaseCheckpointResult Result;
            if (!RoundTripCheckpoint(World->GetPersistentCampaign(),
                TEXT("daxton_phase_transition"), Result, OutError)) return false;
            InOutCheckpoints.Add(MoveTemp(Result));
        }
        if (!World->ApplyDaxtonInteraction(FGuid(28, RouteIndex + 1, 20, 3),
                EDADaxtonInteraction::DisableCoolant, 1.f, OutError)
            || !World->ApplyDaxtonInteraction(FGuid(28, RouteIndex + 1, 20, 4),
                EDADaxtonInteraction::RedirectSupply, 10.f, OutError)
            || !World->ApplyDaxtonInteraction(FGuid(28, RouteIndex + 1, 20, 5),
                EDADaxtonInteraction::HackProduction, 10.f + RouteIndex * 10.f, OutError)
            || !World->ApplyDaxtonInteraction(FGuid(28, RouteIndex + 1, 20, 6),
                EDADaxtonInteraction::WorkerShutdown, 1.f, OutError)
            || !World->ApplyDaxtonInteraction(FGuid(28, RouteIndex + 1, 20, 7),
                EDADaxtonInteraction::Damage, 60.f, OutError)
            || !World->EnterDaxtonChoicePhase(FGuid(28, RouteIndex + 1, 20, 8), OutError)
            || !World->CompleteDaxtonChoiceObjective(FGuid(28, RouteIndex + 1, 20, 9),
                EDADaxtonChoiceObjective::DefeatDaxton, OutError))
            return false;
        int32 ChoiceAction = 10;
        if (RouteFixture.bSaveGrandForge && !World->CompleteDaxtonChoiceObjective(
            FGuid(28, RouteIndex + 1, 20, ChoiceAction++),
            EDADaxtonChoiceObjective::SaveGrandForge, OutError)) return false;
        if (RouteFixture.bEvacuateWorkers && !World->CompleteDaxtonChoiceObjective(
            FGuid(28, RouteIndex + 1, 20, ChoiceAction++),
            EDADaxtonChoiceObjective::EvacuateWorkers, OutError)) return false;
        if (RouteFixture.bOfferProductionUnion && !World->CompleteDaxtonChoiceObjective(
            FGuid(28, RouteIndex + 1, 20, ChoiceAction++),
            EDADaxtonChoiceObjective::StabilizeProductionOfferUnion, OutError)) return false;
        if (!World->ResolveDaxtonLeaderState(FGuid(28, RouteIndex + 1, 20, ChoiceAction++),
                RouteFixture.LeaderState, OutError)
            || !World->CompleteFirstAscension(FGuid(28, RouteIndex + 1, 30, 1), OutError))
            return false;
        if (!InOutCheckpoints.ContainsByPredicate([](const FDAReleaseCheckpointResult& Result)
            { return Result.Checkpoint == TEXT("immediately_after_ascension"); }))
        {
            FDAReleaseCheckpointResult Result;
            if (!RoundTripCheckpoint(World->GetPersistentCampaign(),
                TEXT("immediately_after_ascension"), Result, OutError)) return false;
            InOutCheckpoints.Add(MoveTemp(Result));
        }
        const FDACampaignSnapshot& Campaign = World->GetPersistentCampaign();
        OutAftermath.RouteName = RouteFixture.RouteName;
        OutAftermath.SourceSha256 = RouteFixture.SourceSha256;
        OutAftermath.Loyalty = Campaign.OperationConflict.Resources.PostConflictLoyalty;
        OutAftermath.LeaderState = Campaign.DaxtonState.LeaderState;
        OutAftermath.HistorySha256 = HistoryFingerprint(Campaign);
        OutAftermath.DamageSha256 = DamageFingerprint(Campaign);
        OutAftermath.RelationshipSha256 = RelationshipFingerprint(Campaign);
        OutAftermath.bReachedFirstAscension = Campaign.AscensionState.bForgeweaveAscended
            && Campaign.AscensionState.ConvergenceAuthority == 1
            && Campaign.AscensionState.RelicIds.Contains(TEXT("relic.forge"));
        return true;
    }

    bool WriteAutomationEvidence(const FString& Filename, const FString& RunId,
        const FString& BuildId, const FString& PlanSha256,
        const TArray<FDAReleaseRouteAftermath>& Routes,
        const TArray<FDAReleaseCheckpointResult>& Checkpoints)
    {
        if (Filename.IsEmpty()) return true;
        const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetNumberField(TEXT("schemaVersion"), 1);
        Root->SetStringField(TEXT("runId"), RunId);
        Root->SetStringField(TEXT("buildId"), BuildId);
        Root->SetStringField(TEXT("fixturePlanSha256"), PlanSha256);
        TArray<TSharedPtr<FJsonValue>> RouteRows;
        for (const FDAReleaseRouteAftermath& Route : Routes)
        {
            const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("route"), Route.RouteName);
            Row->SetStringField(TEXT("sourceSha256"), Route.SourceSha256);
            Row->SetBoolField(TEXT("reachedFirstAscension"), Route.bReachedFirstAscension);
            const TSharedRef<FJsonObject> Aftermath = MakeShared<FJsonObject>();
            Aftermath->SetNumberField(TEXT("loyaltyBefore"), Route.LoyaltyBefore);
            Aftermath->SetNumberField(TEXT("loyalty"), Route.Loyalty);
            Aftermath->SetStringField(TEXT("loyaltyActionSha256"), Route.LoyaltyActionSha256);
            Aftermath->SetStringField(TEXT("daxtonState"), LeaderStateName(Route.LeaderState));
            Aftermath->SetStringField(TEXT("historySha256"), Route.HistorySha256);
            Aftermath->SetStringField(TEXT("damageSha256"), Route.DamageSha256);
            Aftermath->SetStringField(TEXT("relationshipSha256"), Route.RelationshipSha256);
            Row->SetObjectField(TEXT("aftermath"), Aftermath);
            RouteRows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Root->SetArrayField(TEXT("routes"), RouteRows);
        TArray<TSharedPtr<FJsonValue>> CheckpointRows;
        for (const FDAReleaseCheckpointResult& Checkpoint : Checkpoints)
        {
            const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("checkpoint"), Checkpoint.Checkpoint);
            Row->SetBoolField(TEXT("passed"), Checkpoint.bPassed);
            Row->SetStringField(TEXT("snapshotSha256"), Checkpoint.SnapshotSha256);
            CheckpointRows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Root->SetArrayField(TEXT("saveCheckpoints"), CheckpointRows);
        FString Document;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Document);
        if (!FJsonSerializer::Serialize(Root, Writer)) return false;
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        return FFileHelper::SaveStringToFile(Document, *Filename,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

BEGIN_DEFINE_SPEC(FDAVerticalSliceReleaseSpec, "Dominion.Release.VerticalSlice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAVerticalSliceReleaseSpec)

void FDAVerticalSliceReleaseSpec::Define()
{
    It("runs four production-owner routes to first Ascension and six save checkpoints without manufacturing release signoff", [this]()
    {
        FString FixturePlanPath;
        FString RunId;
        FString BuildId;
        TestTrue("Release fixture plan is supplied by the validated source generator",
            FParse::Value(FCommandLine::Get(), ReleaseFixturePlanArgument, FixturePlanPath));
        TestTrue("Release evidence is bound to an explicit run identity",
            FParse::Value(FCommandLine::Get(), ReleaseRunIdArgument, RunId));
        TestTrue("Release evidence is bound to an explicit build identity",
            FParse::Value(FCommandLine::Get(), ReleaseBuildIdArgument, BuildId));
        if (FixturePlanPath.IsEmpty() || RunId.IsEmpty() || BuildId.IsEmpty()) return;
        FString PlanSha256;
        FString Error;
        TArray<FDAReleaseRouteFixture> Fixtures;
        TestTrue("Exact deterministic fixture plan loads and reconciles source bytes",
            LoadFixturePlan(FixturePlanPath, PlanSha256, Fixtures, Error));
        if (Fixtures.Num() != 4) return;

        TArray<FDAReleaseCheckpointResult> PassedCheckpoints;
        const auto CaptureCheckpoint = [this, &PassedCheckpoints, &Error](
            const TCHAR* Assertion, const FDACampaignSnapshot& Campaign,
            const FString& Checkpoint)
        {
            FDAReleaseCheckpointResult Result;
            const bool bPassed = RoundTripCheckpoint(Campaign, Checkpoint, Result, Error);
            TestTrue(Assertion, bPassed);
            if (bPassed) PassedCheckpoints.Add(MoveTemp(Result));
            return bPassed;
        };
        FDACampaignSnapshot FirstCity = MakeFirstCityCheckpoint();
        if (!CaptureCheckpoint(TEXT("First city built round-trips"),
            FirstCity, TEXT("first_city_built"))) return;
        FDACampaignSnapshot MidNia = FirstCity;
        AddQuest(MidNia, TEXT("quest.nia_needs_a_job"), false);
        if (!CaptureCheckpoint(TEXT("Mid-Nia quest round-trips"),
            MidNia, TEXT("mid_nia_quest"))) return;

        {
            FDAGameInstanceSubsystemFixture FoundryFixture;
            UDAWorldStateSubsystem* World = FoundryFixture.GetSubsystem<UDAWorldStateSubsystem>();
            TestNotNull("Foundry checkpoint has the canonical campaign owner", World);
            if (World == nullptr) return;
            FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
            Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
            TestTrue("Foundry trigger candidate restores", World->RestorePersistentCampaign(Candidate));
            TestTrue("Foundry warning becomes active through a real World Tick", World->AdvanceWorldTicks(1));
            if (!CaptureCheckpoint(TEXT("Active Foundry Shortage round-trips"),
                World->GetPersistentCampaign(), TEXT("active_foundry_shortage"))) return;
        }

        FDACampaignSnapshot MidIron;
        AddQuest(MidIron, TEXT("quest.operation_iron_veil"), false);
        FDAWorldAssetRecord& DamagedShield = MidIron.WorldAssets.Emplace_GetRef();
        DamagedShield.WorldAssetId = FGuid(28, 60, 1, 1);
        DamagedShield.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
        DamagedShield.OwnerCivilizationId = TEXT("civilization.forgeweave");
        DamagedShield.CityId = TEXT("city.ironheart");
        DamagedShield.ConstructionState = EDAConstructionState::Operational;
        DamagedShield.StructuralIntegrity = 65.f;
        FDAStructuralDamageRecord& MidIronDamage =
            MidIron.OperationConflict.StructuralDamageRecords.Emplace_GetRef();
        MidIronDamage.WorldAssetId = DamagedShield.WorldAssetId;
        MidIronDamage.CardDefinitionId = DamagedShield.CardDefinitionId;
        MidIronDamage.Modules = {
            FDAStructureModuleHealthRecord(TEXT("module.shield_emitter"), 100.f, true)};
        MidIronDamage.Modules[0].CurrentHealth = 65.f;
        MidIronDamage.Modules[0].State = EDAStructureDamageState::Damaged;
        if (!CaptureCheckpoint(TEXT("Mid-Iron Veil round-trips"),
            MidIron, TEXT("mid_iron_veil"))) return;

        TArray<FDAReleaseRouteAftermath> Routes;
        for (int32 Index = 0; Index < Fixtures.Num(); ++Index)
        {
            FDAReleaseRouteAftermath Aftermath;
            const bool bCompleted = ExecuteRoute(Fixtures[Index], Index, Aftermath,
                PassedCheckpoints, Error);
            TestTrue(*FString::Printf(TEXT("%s reaches first Ascension through production owners: %s"),
                *Fixtures[Index].RouteName, *Error), bCompleted);
            if (bCompleted)
            {
                TestTrue(*FString::Printf(TEXT("%s committed its exact Leader outcome"),
                    *Fixtures[Index].RouteName), Aftermath.LeaderState == Fixtures[Index].LeaderState);
                TestTrue(*FString::Printf(TEXT("%s committed the 1/20 Ascension"),
                    *Fixtures[Index].RouteName), Aftermath.bReachedFirstAscension);
                Routes.Add(Aftermath);
            }
        }
        TestEqual("All six exact save checkpoints pass", PassedCheckpoints.Num(), 6);
        TestEqual("All four route runs produced aftermath", Routes.Num(), 4);
        if (Routes.Num() != 4 || PassedCheckpoints.Num() != 6) return;

        TArray<float> Loyalty;
        TArray<uint8> LeaderStates;
        TArray<FString> History;
        TArray<FString> Damage;
        TArray<FString> Relationship;
        for (const FDAReleaseRouteAftermath& Route : Routes)
        {
            Loyalty.Add(Route.Loyalty);
            LeaderStates.Add(static_cast<uint8>(Route.LeaderState));
            History.Add(Route.HistorySha256);
            Damage.Add(Route.DamageSha256);
            Relationship.Add(Route.RelationshipSha256);
        }
        const bool bDistinctLoyalty = IsPairwiseDistinct(Loyalty);
        const bool bDistinctLeaders = IsPairwiseDistinct(LeaderStates);
        const bool bDistinctHistory = IsPairwiseDistinct(History);
        const bool bDistinctDamage = IsPairwiseDistinct(Damage);
        const bool bDistinctRelationships = IsPairwiseDistinct(Relationship);
        TestTrue("Route Loyalty aftermath is pairwise distinct", bDistinctLoyalty);
        TestTrue("Route Daxton aftermath is pairwise distinct", bDistinctLeaders);
        TestTrue("Route history aftermath is pairwise distinct", bDistinctHistory);
        TestTrue("Route damage aftermath is pairwise distinct", bDistinctDamage);
        TestTrue("Route relationship aftermath is pairwise distinct", bDistinctRelationships);

        FString AutomationEvidencePath;
        FParse::Value(FCommandLine::Get(), ReleaseAutomationEvidenceArgument, AutomationEvidencePath);
        const bool bAutomationPass = bDistinctLoyalty && bDistinctLeaders && bDistinctHistory
            && bDistinctDamage && bDistinctRelationships
            && PassedCheckpoints.Num() == 6
            && PassedCheckpoints.ContainsByPredicate([](const FDAReleaseCheckpointResult& Result)
                { return !Result.bPassed || Result.SnapshotSha256.Len() != 64; }) == false;
        TestTrue("Passing route/save Automation writes an evidence sidecar",
            !bAutomationPass || WriteAutomationEvidence(
                AutomationEvidencePath, RunId, BuildId, PlanSha256, Routes, PassedCheckpoints));
        AddInfo(TEXT("Controller-only critical path, route videos, uncoached onboarding, reference hardware, and defect triage are intentionally outside Automation and remain fail-closed in the release reconciler."));
    });
}
