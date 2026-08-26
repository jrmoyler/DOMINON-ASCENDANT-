#include "Diplomacy/DADiplomacySystem.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "Misc/AutomationTest.h"
#include "Regions/DARegionTravelRuntimeSubsystem.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Time/DASimulationClockSubsystem.h"
#include "Trade/DATradeSystem.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDATradeDiplomacySpec, "Dominion.World.TradeDiplomacy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDATradeDiplomacySpec)

namespace
{
    constexpr float Tolerance = 0.001f;

    const FName SynaraRegion(TEXT("region.synara_frontier"));
    const FName IronheartRegion(TEXT("region.ironheart"));
    const FName MachineComponents(TEXT("resource.machine_components"));
    const FName ForgeweaveRelationship(TEXT("relationship.synara_forgeweave"));
    const FName ForgeweaveContract(TEXT("contract.forgeweave.machine_components"));
    const FName ForgeweaveRoute(TEXT("route.synara_ironheart.freight"));

    FDADiplomacyWorldState MakeDiplomacyState()
    {
        FDADiplomacyWorldState State;
        FDADiplomaticRelationship Relationship;
        Relationship.RelationshipId = ForgeweaveRelationship;
        State.Relationships.Add(Relationship);
        return State;
    }

    FDATradeWorldState MakeTradeState(const int64 Supply, const int64 Capacity, const int64 StartWorldTick = 1)
    {
        FDATradeWorldState State;

        FDARegionalTradeInventory Inventory;
        Inventory.RegionId = SynaraRegion;
        Inventory.Stock.Add(MachineComponents, Supply);
        State.Inventories.Add(Inventory);

        FDARegionalTradeInventory DestinationInventory;
        DestinationInventory.RegionId = IronheartRegion;
        DestinationInventory.Stock.Add(MachineComponents, 0);
        State.Inventories.Add(DestinationInventory);

        FDATradeRouteState Route;
        Route.RouteId = ForgeweaveRoute;
        Route.SourceRegionId = SynaraRegion;
        Route.DestinationRegionId = IronheartRegion;
        Route.CapacityPerWorldTick = Capacity;
        State.Routes.Add(Route);

        FDATradeContractState Contract;
        Contract.ContractId = ForgeweaveContract;
        Contract.RelationshipId = ForgeweaveRelationship;
        Contract.RouteId = ForgeweaveRoute;
        Contract.SourceRegionId = SynaraRegion;
        Contract.DestinationRegionId = IronheartRegion;
        Contract.GoodId = MachineComponents;
        Contract.QuantityPerWorldTick = 10;
        Contract.StartWorldTick = StartWorldTick;
        Contract.DurationWorldTicks = 5;
        Contract.RelationshipMetric = EDADiplomaticMetric::Trust;
        Contract.RelationshipMagnitudePerDelivery = 2.f;
        Contract.RelationshipReasonSource = TEXT("trade.forgeweave.machine_components.delivered");
        State.Contracts.Add(Contract);
        return State;
    }

    class FDARecordingTravelRuntime final : public IDARegionTravelRuntime
    {
    public:
        virtual bool SnapshotPersistentDelta(
            const FDARegionState& SourceRegion,
            FDARegionPersistentDelta& OutDelta) override
        {
            ++SnapshotCalls;
            Calls.Add(TEXT("snapshot"));
            if (bFailSnapshot)
            {
                return false;
            }

            OutDelta.Revision = SourceRegion.PersistentDelta.Revision + 1;
            FDARegionActorState Actor;
            Actor.ActorId = TEXT("actor.synara.transit_terminal");
            Actor.DefinitionId = TEXT("asset.passage_terminal");
            OutDelta.LocalActors.Add(Actor);
            return true;
        }

        virtual bool UnloadRegionRepresentation(const FName RegionId) override
        {
            ++UnloadCalls;
            Calls.Add(TEXT("unload"));
            LastUnloadedRegion = RegionId;
            return !bFailUnload;
        }

        virtual bool EnsureRegionLoaded(const FName RequestId, const FName RegionId) override
        {
            ++LoadCalls;
            Calls.Add(TEXT("load"));
            LastLoadRequestId = RequestId;
            LastLoadedRegion = RegionId;
            return !bFailLoad;
        }

        virtual bool EnsureRegionReconstructed(
            const FName RequestId,
            const FDARegionState& Region,
            const FDACampaignSnapshot& Campaign) override
        {
            (void)Campaign;
            ++ReconstructCalls;
            Calls.Add(TEXT("reconstruct"));
            LastReconstructRequestId = RequestId;
            ReconstructedActorCount = Region.PersistentDelta.LocalActors.Num();
            LastReconstructedRegion = Region.RegionId;
            return !bFailReconstruct;
        }

        bool bFailSnapshot = false;
        bool bFailUnload = false;
        bool bFailLoad = false;
        bool bFailReconstruct = false;
        int32 SnapshotCalls = 0;
        int32 UnloadCalls = 0;
        int32 LoadCalls = 0;
        int32 ReconstructCalls = 0;
        int32 ReconstructedActorCount = 0;
        FName LastUnloadedRegion;
        FName LastLoadRequestId;
        FName LastLoadedRegion;
        FName LastReconstructRequestId;
        FName LastReconstructedRegion;
        TArray<FName> Calls;
    };

    class FDARecordingRegionLoader final : public IDARegionRuntimeLoader
    {
    public:
        virtual bool LoadRegion(UWorld*, const FName RequestId,
            const FDARegionState& Region,
            TWeakObjectPtr<ULevelStreaming>& OutStreamingLevel) override
        {
            ++LoadCalls;
            LastRequestId = RequestId;
            LastRegionId = Region.RegionId;
            LastMapPath = Region.MapAssetPath;
            OutStreamingLevel.Reset();
            return bAllowLoad;
        }

        virtual bool UnloadRegion(UWorld*, TWeakObjectPtr<ULevelStreaming>) override
        {
            ++UnloadCalls;
            return true;
        }

        virtual bool SpawnLocalActor(UWorld*, const FDARegionState&,
            const FDARegionActorState& ActorState,
            TWeakObjectPtr<AActor>& OutActor) override
        {
            ++LocalActorSpawnCalls;
            SpawnedLocalActorIds.Add(ActorState.ActorId);
            OutActor.Reset();
            return true;
        }

        virtual bool SpawnWorldAsset(UWorld*, UDAWorldStateSubsystem&,
            const FDARegionState&, const FDAWorldAssetRecord& Asset,
            TWeakObjectPtr<AActor>& OutActor) override
        {
            ++WorldAssetSpawnCalls;
            SpawnedWorldAssetIds.Add(Asset.WorldAssetId);
            OutActor.Reset();
            return true;
        }

        bool bAllowLoad = true;
        int32 LoadCalls = 0;
        int32 UnloadCalls = 0;
        int32 LocalActorSpawnCalls = 0;
        int32 WorldAssetSpawnCalls = 0;
        FName LastRequestId;
        FName LastRegionId;
        FSoftObjectPath LastMapPath;
        TArray<FName> SpawnedLocalActorIds;
        TArray<FGuid> SpawnedWorldAssetIds;
    };

}

void FDATradeDiplomacySpec::Define()
{
    It("seeds exactly the three regions and three settlements frozen for the vertical slice", [this]()
    {
        const TArray<FDARegionState> Regions = FDARegionSeedCatalog::MakeVerticalSliceRegions();
        TSet<FName> ActualIds;
        for (const FDARegionState& Region : Regions)
        {
            ActualIds.Add(Region.RegionId);
            for (const FName SettlementId : Region.SettlementIds)
            {
                ActualIds.Add(SettlementId);
            }
        }

        TSet<FName> ExpectedIds;
        ExpectedIds.Add(TEXT("region.synara_frontier"));
        ExpectedIds.Add(TEXT("region.ironheart"));
        ExpectedIds.Add(TEXT("region.eden_basin"));
        ExpectedIds.Add(TEXT("settlement.arden_reservoir"));
        ExpectedIds.Add(TEXT("settlement.ore_station_7"));
        ExpectedIds.Add(TEXT("settlement.river_crossing"));

        TestEqual("Exactly three region records are seeded", Regions.Num(), 3);
        TestEqual("Exactly six stable location ids are exposed", ActualIds.Num(), 6);
        bool bExactIds = ActualIds.Num() == ExpectedIds.Num();
        for (const FName ExpectedId : ExpectedIds)
        {
            bExactIds = bExactIds && ActualIds.Contains(ExpectedId);
        }
        TestTrue("The stable location set is exact", bExactIds);
    });

    It("delivers a complete five-World-Tick machine-component contract and records explainable diplomacy", [this]()
    {
        UDATradeSystem* Trade = NewObject<UDATradeSystem>();
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>();
        FDATradeWorldState TradeState = MakeTradeState(50, 10);
        FDADiplomacyWorldState DiplomacyState = MakeDiplomacyState();

        for (int64 WorldTick = 1; WorldTick <= 5; ++WorldTick)
        {
            TestEqual(
                *FString::Printf(TEXT("World Tick %lld processes once"), WorldTick),
                Trade->ProcessWorldTick(TradeState, DiplomacyState, *Diplomacy, WorldTick),
                EDATradeTickResult::Processed);
        }

        const FDATradeContractState& Contract = TradeState.Contracts[0];
        const FDADiplomaticRelationship& Relationship = DiplomacyState.Relationships[0];
        TestEqual("All fifty components are delivered", Contract.DeliveredQuantity, 50LL);
        TestEqual("All five scheduled deliveries succeed", Contract.SuccessfulDeliveryCount, 5);
        TestEqual("No scheduled delivery fails", Contract.FailedDeliveryCount, 0);
        TestTrue("The five-tick contract reaches a durable terminal state", Contract.bCompleted);
        TestEqual("Actual source inventory is consumed", TradeState.Inventories[0].Stock[MachineComponents], 0LL);
        TestEqual("Actual destination inventory receives all fifty components", TradeState.Inventories[1].Stock[MachineComponents], 50LL);
        TestEqual("Trust is the aggregate of the five real deliveries", Relationship.Trust, 10.f, Tolerance);
        TestEqual("One explainable reason exists per delivery", Relationship.ReasonLedger.Num(), 5);

        for (int32 Index = 0; Index < Relationship.ReasonLedger.Num(); ++Index)
        {
            const FDADiplomaticReason& Reason = Relationship.ReasonLedger[Index];
            TestEqual("Reason source is retained", Reason.SourceTag, FName(TEXT("trade.forgeweave.machine_components.delivered")));
            TestEqual("Reason magnitude is retained", Reason.Magnitude, 2.f, Tolerance);
            TestEqual("Reason World Tick is retained", Reason.WorldTick, static_cast<int64>(Index + 1));
        }
    });

    It("makes supply capacity diplomacy and delivery one atomic per-tick transaction", [this]()
    {
        UDATradeSystem* Trade = NewObject<UDATradeSystem>();
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>();
        FDATradeWorldState TradeState = MakeTradeState(50, 9);
        FDADiplomacyWorldState DiplomacyState = MakeDiplomacyState();

        TestEqual(
            "An under-capacity World Tick is processed as a failed attempt",
            Trade->ProcessWorldTick(TradeState, DiplomacyState, *Diplomacy, 1),
            EDATradeTickResult::Processed);
        TestEqual("Failed capacity reserves nothing", TradeState.Routes[0].ReservedCapacityThisTick, 0LL);
        TestEqual("Failed capacity consumes no inventory", TradeState.Inventories[0].Stock[MachineComponents], 50LL);
        TestEqual("Failed capacity credits no destination inventory", TradeState.Inventories[1].Stock[MachineComponents], 0LL);
        TestEqual("Failed capacity creates no delivery", TradeState.Deliveries.Num(), 0);
        TestEqual("Failed capacity mutates no relationship", DiplomacyState.Relationships[0].Trust, 0.f, Tolerance);
        TestEqual("Failed capacity creates no diplomatic reason", DiplomacyState.Relationships[0].ReasonLedger.Num(), 0);

        TradeState.Routes[0].CapacityPerWorldTick = 10;
        TestEqual(
            "A duplicate tick is rejected even after capacity changes",
            Trade->ProcessWorldTick(TradeState, DiplomacyState, *Diplomacy, 1),
            EDATradeTickResult::DuplicateOrOutOfOrder);
        TestEqual("Duplicate processing cannot replay inventory mutation", TradeState.Inventories[0].Stock[MachineComponents], 50LL);

        TestEqual(
            "The next scheduled tick can deliver",
            Trade->ProcessWorldTick(TradeState, DiplomacyState, *Diplomacy, 2),
            EDATradeTickResult::Processed);
        TestEqual("Successful delivery consumes exactly ten", TradeState.Inventories[0].Stock[MachineComponents], 40LL);
        TestEqual("Successful delivery credits exactly ten", TradeState.Inventories[1].Stock[MachineComponents], 10LL);
        TestEqual("Successful delivery reserves exactly ten", TradeState.Routes[0].ReservedCapacityThisTick, 10LL);
        TestEqual("Successful delivery has one durable record", TradeState.Deliveries.Num(), 1);
        TestEqual("Successful delivery mutates Trust exactly once", DiplomacyState.Relationships[0].Trust, 2.f, Tolerance);

        const FDATradeWorldState BeforeSkippedTick = TradeState;
        const FDADiplomacyWorldState DiplomacyBeforeSkippedTick = DiplomacyState;
        TestEqual(
            "Skipping a deterministic World Tick is rejected",
            Trade->ProcessWorldTick(TradeState, DiplomacyState, *Diplomacy, 4),
            EDATradeTickResult::DuplicateOrOutOfOrder);
        TestEqual("A skipped tick does not consume inventory", TradeState.Inventories[0].Stock[MachineComponents], BeforeSkippedTick.Inventories[0].Stock[MachineComponents]);
        TestEqual("A skipped tick does not add deliveries", TradeState.Deliveries.Num(), BeforeSkippedTick.Deliveries.Num());
        TestEqual("A skipped tick does not mutate diplomacy", DiplomacyState.Relationships[0].Trust, DiplomacyBeforeSkippedTick.Relationships[0].Trust, Tolerance);
    });

    It("finishes partially fulfilled contracts without inventing short deliveries", [this]()
    {
        UDATradeSystem* Trade = NewObject<UDATradeSystem>();
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>();
        FDATradeWorldState TradeState = MakeTradeState(25, 10);
        FDADiplomacyWorldState DiplomacyState = MakeDiplomacyState();

        for (int64 WorldTick = 1; WorldTick <= 5; ++WorldTick)
        {
            Trade->ProcessWorldTick(TradeState, DiplomacyState, *Diplomacy, WorldTick);
        }

        const FDATradeContractState& Contract = TradeState.Contracts[0];
        TestEqual("Only two whole deliveries occur", Contract.DeliveredQuantity, 20LL);
        TestEqual("Two attempts succeed", Contract.SuccessfulDeliveryCount, 2);
        TestEqual("Three attempts fail", Contract.FailedDeliveryCount, 3);
        TestEqual("Five scheduled attempts are durable", Contract.ProcessedWorldTicks.Num(), 5);
        TestEqual("The unusable remainder is not consumed", TradeState.Inventories[0].Stock[MachineComponents], 5LL);
        TestEqual("The destination receives only the two whole transfers", TradeState.Inventories[1].Stock[MachineComponents], 20LL);
        TestEqual("Diplomacy reflects only actual deliveries", DiplomacyState.Relationships[0].Trust, 4.f, Tolerance);
        TestEqual("Failed attempts add no diplomatic memories", DiplomacyState.Relationships[0].ReasonLedger.Num(), 2);
        TestTrue("The scheduled contract still reaches its terminal tick", Contract.bCompleted);
    });

    It("rejects duplicate contract ids and duplicate diplomatic mutation ids", [this]()
    {
        UDATradeSystem* Trade = NewObject<UDATradeSystem>();
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>();
        FDATradeWorldState TradeState = MakeTradeState(50, 10);
        FDADiplomacyWorldState DiplomacyState = MakeDiplomacyState();

        TestFalse("A second active contract with the same stable id is rejected", Trade->AddContract(TradeState, TradeState.Contracts[0]));

        FDADiplomaticRelationship& Relationship = DiplomacyState.Relationships[0];
        TestTrue(
            "A valid reason applies",
            Diplomacy->ApplyReason(Relationship, EDADiplomaticMetric::Respect, TEXT("quest.workers_saved"), 9.f, 7, TEXT("reason.workers_saved")));
        TestFalse(
            "The same mutation id cannot be replayed",
            Diplomacy->ApplyReason(Relationship, EDADiplomaticMetric::Respect, TEXT("quest.workers_saved"), 9.f, 7, TEXT("reason.workers_saved")));
        TestEqual("Respect is not double counted", Relationship.Respect, 9.f, Tolerance);
        TestEqual("The reason ledger remains exact", Relationship.ReasonLedger.Num(), 1);
        TestEqual("The retained source is explainable", Relationship.ReasonLedger[0].SourceTag, FName(TEXT("quest.workers_saved")));
        TestEqual("The retained magnitude is explainable", Relationship.ReasonLedger[0].Magnitude, 9.f, Tolerance);
        TestEqual("The retained tick is explainable", Relationship.ReasonLedger[0].WorldTick, 7LL);
    });

    It("aggregates all six diplomatic metrics exclusively from their reason ledger", [this]()
    {
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>();
        FDADiplomaticRelationship Relationship;
        Relationship.RelationshipId = ForgeweaveRelationship;

        const EDADiplomaticMetric Metrics[] = {
            EDADiplomaticMetric::Trust,
            EDADiplomaticMetric::Respect,
            EDADiplomaticMetric::Fear,
            EDADiplomaticMetric::Dependence,
            EDADiplomaticMetric::Grievance,
            EDADiplomaticMetric::Compatibility
        };
        const float Magnitudes[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};

        for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Metrics)); ++Index)
        {
            TestTrue(
                *FString::Printf(TEXT("Metric %d accepts an explained mutation"), Index),
                Diplomacy->ApplyReason(
                    Relationship,
                    Metrics[Index],
                    FName(*FString::Printf(TEXT("test.metric.%d"), Index)),
                    Magnitudes[Index],
                    30 + Index,
                    FName(*FString::Printf(TEXT("reason.metric.%d"), Index))));
            TestEqual(
                *FString::Printf(TEXT("Metric %d aggregate matches its reason"), Index),
                Diplomacy->GetAggregate(Relationship, Metrics[Index]),
                Magnitudes[Index],
                Tolerance);
        }

        FString ValidationError;
        TestEqual("Exactly six reasons explain six metrics", Relationship.ReasonLedger.Num(), 6);
        TestTrue("The complete relationship remains persistence-valid", Relationship.Validate(ValidationError));
    });

    It("snapshots unloads advances loads and reconstructs in the required travel order", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* WorldState = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("World subsystem comes from a live GameInstance", WorldState);
        TestNotNull("Clock subsystem comes from the same live GameInstance", Clock);
        if (WorldState == nullptr || Clock == nullptr)
        {
            return;
        }
        TestTrue("The frozen regional seed initializes", WorldState->InitializeVerticalSliceState(SynaraRegion, 10));

        FDAWorldCampaignState PersistentState = WorldState->GetPersistentState();
        FDARegionState* Ironheart = PersistentState.FindRegion(IronheartRegion);
        TestNotNull("Ironheart exists", Ironheart);
        if (Ironheart != nullptr)
        {
            FDARegionActorState ForgeActor;
            ForgeActor.ActorId = TEXT("actor.ironheart.grand_forge");
            ForgeActor.DefinitionId = TEXT("asset.grand_forge");
            Ironheart->PersistentDelta.LocalActors.Add(ForgeActor);
            ++Ironheart->PersistentDelta.Revision;
        }
        PersistentState.Trade = MakeTradeState(50, 10, 11);
        PersistentState.Trade.LastProcessedWorldTick = 10;
        PersistentState.Diplomacy = MakeDiplomacyState();
        TestTrue("A valid persistent state can be restored", WorldState->RestorePersistentState(PersistentState));

        TArray<EDATravelHandoffStage> CompletedStages;
        WorldState->OnTravelStageCompleted.AddLambda([&CompletedStages](const EDATravelHandoffStage Stage)
        {
            CompletedStages.Add(Stage);
        });
        TArray<int64> ClockListenerTicks;
        const FDelegateHandle ClockListenerHandle = Clock->OnWorldTick.AddLambda(
            [&ClockListenerTicks](const int64 WorldTick)
            {
                ClockListenerTicks.Add(WorldTick);
            });

        FDARecordingTravelRuntime Runtime;
        TestEqual(
            "Travel completes",
            WorldState->Travel(TEXT("travel.synara_to_ironheart.1"), IronheartRegion, 3, Runtime),
            EDATravelResult::Completed);

        const EDATravelHandoffStage ExpectedStages[] = {
            EDATravelHandoffStage::SourceSnapshotted,
            EDATravelHandoffStage::SourceUnloaded,
            EDATravelHandoffStage::TimeAdvanced,
            EDATravelHandoffStage::DestinationLoaded,
            EDATravelHandoffStage::DestinationReconstructed,
            EDATravelHandoffStage::Completed
        };
        TestEqual("All handoff stages complete", CompletedStages.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedStages)));
        for (int32 Index = 0; Index < CompletedStages.Num() && Index < UE_ARRAY_COUNT(ExpectedStages); ++Index)
        {
            TestEqual(*FString::Printf(TEXT("Handoff stage %d is ordered"), Index), CompletedStages[Index], ExpectedStages[Index]);
        }

        const FDAWorldCampaignState& Result = WorldState->GetPersistentState();
        const FDARegionState* SavedSynara = Result.FindRegion(SynaraRegion);
        TestNotNull("The source region remains persistent", SavedSynara);
        if (SavedSynara != nullptr)
        {
            TestEqual("The source snapshot precedes its unload", SavedSynara->PersistentDelta.LocalActors.Num(), 1);
        }
        TestEqual("Strategic travel advances exactly three World Ticks", Result.CurrentWorldTick, 13LL);
        TestEqual("The sole clock authority advances to the same World Tick", Clock->GetCurrentWorldTick(), 13LL);
        TestEqual("Another normal listener sees three skipped World Ticks", ClockListenerTicks.Num(), 3);
        if (ClockListenerTicks.Num() == 3)
        {
            TestEqual("Listener sees World Tick 11", ClockListenerTicks[0], 11LL);
            TestEqual("Listener sees World Tick 12", ClockListenerTicks[1], 12LL);
            TestEqual("Listener sees World Tick 13", ClockListenerTicks[2], 13LL);
        }
        TestEqual("Trade processes all three travel ticks", Result.Trade.Contracts[0].DeliveredQuantity, 30LL);
        TestEqual("Travel-time trade debits the real source", Result.Trade.Inventories[0].Stock[MachineComponents], 20LL);
        TestEqual("Travel-time trade credits the real destination", Result.Trade.Inventories[1].Stock[MachineComponents], 30LL);
        TestEqual("Travel-time diplomacy follows the three deliveries", Result.Diplomacy.Relationships[0].Trust, 6.f, Tolerance);
        TestEqual("Authority changes only after reconstruction", Result.CurrentRegionId, IronheartRegion);
        const FDARegionState* ReconstructedIronheart = Result.FindRegion(IronheartRegion);
        TestNotNull("Reconstructed Ironheart remains persistent", ReconstructedIronheart);
        if (ReconstructedIronheart != nullptr)
        {
            TestEqual(
                "Destination reconstructs every persistent actor, including rival World-Tick construction",
                Runtime.ReconstructedActorCount,
                ReconstructedIronheart->PersistentDelta.LocalActors.Num());
        }
        TestEqual("Load idempotence is keyed by travel request", Runtime.LastLoadRequestId, FName(TEXT("travel.synara_to_ironheart.1")));
        TestEqual("Reconstruction idempotence is keyed by travel request", Runtime.LastReconstructRequestId, FName(TEXT("travel.synara_to_ironheart.1")));
        Clock->OnWorldTick.Remove(ClockListenerHandle);
    });

    It("hands authored maps to the runtime loader and reconstructs local actors plus canonical WorldAssets", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDARegionTravelRuntimeSubsystem* Runtime =
            Fixture.GetSubsystem<UDARegionTravelRuntimeSubsystem>();
        TestNotNull("Canonical world exists", World);
        TestNotNull("Registered production travel subsystem exists", Runtime);
        if (World == nullptr || Runtime == nullptr) return;

        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        FDARegionState* Region = Candidate.WorldState.FindRegion(SynaraRegion);
        TestNotNull("Synara region exists", Region);
        if (Region == nullptr) return;
        FDARegionActorState& LocalActor = Region->PersistentDelta.LocalActors.Emplace_GetRef();
        LocalActor.ActorId = TEXT("actor.synara.persisted_loader_probe");
        LocalActor.DefinitionId = TEXT("asset.synara.persisted_loader_probe");
        ++Region->PersistentDelta.Revision;
        TestTrue("Persisted regional projection fixture restores",
            World->RestorePersistentCampaign(Candidate));

        TSharedRef<FDARecordingRegionLoader> Loader =
            MakeShared<FDARecordingRegionLoader>();
        Runtime->SetRuntimeLoader(Loader);
        const FName RequestId(TEXT("travel.runtime_loader.production_path"));
        TestTrue("EnsureRegionLoaded calls the injected platform boundary",
            Runtime->EnsureRegionLoaded(RequestId, SynaraRegion));
        TestEqual("Runtime loader is called exactly once", Loader->LoadCalls, 1);
        TestEqual("The exact authored map path reaches the loader", Loader->LastMapPath,
            FSoftObjectPath(TEXT("/Game/Maps/Regions/L_SynaraFrontier.L_SynaraFrontier")));

        const FDACampaignSnapshot& Authority = World->GetPersistentCampaign();
        const FDARegionState* CanonicalRegion = Authority.WorldState.FindRegion(SynaraRegion);
        TestNotNull("Canonical region remains addressable", CanonicalRegion);
        if (CanonicalRegion == nullptr) return;
        TestTrue("Reconstruction materializes persisted and canonical projections",
            Runtime->EnsureRegionReconstructed(RequestId, *CanonicalRegion, Authority));
        TestEqual("Every persisted local actor reaches the loader",
            Loader->LocalActorSpawnCalls, CanonicalRegion->PersistentDelta.LocalActors.Num());
        TestTrue("The persisted local actor identity materializes",
            Loader->SpawnedLocalActorIds.Contains(LocalActor.ActorId));
        TestEqual("Synara's Founder Hall materializes from canonical WorldAssets",
            Loader->WorldAssetSpawnCalls, 1);
        TestTrue("The exact Founder Hall WorldAsset identity materializes",
            Loader->SpawnedWorldAssetIds.Contains(Authority.WorldAssets[0].WorldAssetId));

        FDACampaignSnapshot MissingMap = Authority;
        MissingMap.WorldState.FindRegion(SynaraRegion)->MapAssetPath = FSoftObjectPath();
        TestTrue("A legacy/missing-map fixture can restore but remains unloadable",
            World->RestorePersistentCampaign(MissingMap));
        TestFalse("EnsureRegionLoaded fails closed without an authored map",
            Runtime->EnsureRegionLoaded(TEXT("travel.runtime_loader.missing_map"),
                SynaraRegion));
        TestEqual("A missing map never calls the loader", Loader->LoadCalls, 1);
    });

    It("resumes a persisted failed handoff without repeating snapshot unload or strategic time", [this]()
    {
        FDAGameInstanceSubsystemFixture SourceFixture;
        UDAWorldStateSubsystem* SourceWorld = SourceFixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Source world is lifecycle initialized", SourceWorld);
        if (SourceWorld == nullptr)
        {
            return;
        }
        TestTrue("The source world initializes", SourceWorld->InitializeVerticalSliceState(SynaraRegion, 20));

        FDARecordingTravelRuntime FirstRuntime;
        FirstRuntime.bFailLoad = true;
        TestEqual(
            "Destination load failure is durable",
            SourceWorld->Travel(TEXT("travel.resume.1"), IronheartRegion, 2, FirstRuntime),
            EDATravelResult::RuntimeFailure);
        TestEqual("Time advanced once before the failed load", SourceWorld->GetPersistentState().CurrentWorldTick, 22LL);
        TestEqual("Current-region authority has not changed", SourceWorld->GetPersistentState().CurrentRegionId, SynaraRegion);
        TestEqual("The resumable checkpoint is time-advanced", SourceWorld->GetPersistentState().TravelHandoff.Stage, EDATravelHandoffStage::TimeAdvanced);

        const FDACampaignSnapshot SavedDuringTravel = SourceWorld->GetPersistentCampaign();
        FDAGameInstanceSubsystemFixture RestoredFixture;
        UDAWorldStateSubsystem* RestoredWorld = RestoredFixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Restored world is lifecycle initialized", RestoredWorld);
        if (RestoredWorld == nullptr)
        {
            return;
        }
        TestTrue("The in-flight handoff restores", RestoredWorld->RestorePersistentCampaign(SavedDuringTravel));

        FDARecordingTravelRuntime ResumeRuntime;
        TestEqual(
            "The persisted handoff resumes",
            RestoredWorld->Travel(TEXT("travel.resume.1"), IronheartRegion, 2, ResumeRuntime),
            EDATravelResult::Completed);
        TestEqual("Resume does not snapshot again", ResumeRuntime.SnapshotCalls, 0);
        TestEqual("Resume does not unload again", ResumeRuntime.UnloadCalls, 0);
        TestEqual("Resume loads the destination once", ResumeRuntime.LoadCalls, 1);
        TestEqual("Resume reconstructs once", ResumeRuntime.ReconstructCalls, 1);
        TestEqual("Resume does not double-advance travel time", RestoredWorld->GetPersistentState().CurrentWorldTick, 22LL);
        TestEqual("Destination becomes current only after successful reconstruction", RestoredWorld->GetPersistentState().CurrentRegionId, IronheartRegion);

        const int32 CallsBeforeDuplicate = ResumeRuntime.Calls.Num();
        TestEqual(
            "A completed travel request is idempotent",
            RestoredWorld->Travel(TEXT("travel.resume.1"), IronheartRegion, 2, ResumeRuntime),
            EDATravelResult::AlreadyCompleted);
        TestEqual("Completed request replay performs no runtime work", ResumeRuntime.Calls.Num(), CallsBeforeDuplicate);
        TestEqual("Completed request replay advances no time", RestoredWorld->GetPersistentState().CurrentWorldTick, 22LL);
    });

    It("resumes mid-travel from only the remaining authoritative World Ticks", [this]()
    {
        FDAGameInstanceSubsystemFixture SourceFixture;
        UDAWorldStateSubsystem* SourceWorld = SourceFixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Source world exists", SourceWorld);
        if (SourceWorld == nullptr)
        {
            return;
        }

        TestTrue("Source state initializes at World Tick ten", SourceWorld->InitializeVerticalSliceState(SynaraRegion, 10));
        FDAWorldCampaignState InitialState = SourceWorld->GetPersistentState();
        InitialState.Trade = MakeTradeState(50, 10, 11);
        InitialState.Trade.LastProcessedWorldTick = 10;
        InitialState.Diplomacy = MakeDiplomacyState();
        TestTrue("Trade-enabled source state restores", SourceWorld->RestorePersistentState(InitialState));

        FDACampaignSnapshot SnapshotAfterFirstTravelTick;
        TSharedPtr<const FDACampaignSnapshot> RetainedCommittedPayload;
        bool bCaptured = false;
        const FDelegateHandle SnapshotHandle = SourceWorld->OnWorldTickStateCommitted.AddLambda(
            [&SnapshotAfterFirstTravelTick, &RetainedCommittedPayload, &bCaptured](
                const FDACommittedCampaignSnapshot CommittedState)
            {
                if (CommittedState->WorldState.CurrentWorldTick == 11)
                {
                    SnapshotAfterFirstTravelTick = *CommittedState;
                    RetainedCommittedPayload = CommittedState;
                    bCaptured = true;
                }
            });

        FDARecordingTravelRuntime SourceRuntime;
        TestEqual(
            "Original three-tick travel completes",
            SourceWorld->Travel(TEXT("travel.mid_tick.resume"), IronheartRegion, 3, SourceRuntime),
            EDATravelResult::Completed);
        SourceWorld->OnWorldTickStateCommitted.Remove(SnapshotHandle);
        TestTrue("A post-commit listener snapshots after the first travel tick", bCaptured);
        if (!bCaptured)
        {
            return;
        }
        TestEqual("Snapshot captures World Tick eleven", SnapshotAfterFirstTravelTick.WorldState.CurrentWorldTick, 11LL);
        TestEqual("Snapshot is still in the source-unloaded time-advance stage", SnapshotAfterFirstTravelTick.WorldState.TravelHandoff.Stage, EDATravelHandoffStage::SourceUnloaded);
        TestEqual("Snapshot includes exactly one delivery", SnapshotAfterFirstTravelTick.WorldState.Trade.Contracts[0].DeliveredQuantity, 10LL);
        TestTrue("The immutable event payload can outlive the synchronous callback", RetainedCommittedPayload.IsValid());
        if (RetainedCommittedPayload.IsValid())
        {
            TestEqual("Later commits cannot mutate the retained Tick-11 payload", RetainedCommittedPayload->WorldState.CurrentWorldTick, 11LL);
            TestEqual("Retained payload still has exactly one delivery", RetainedCommittedPayload->WorldState.Trade.Contracts[0].DeliveredQuantity, 10LL);
        }

        FDAGameInstanceSubsystemFixture RestoredFixture;
        UDAWorldStateSubsystem* RestoredWorld = RestoredFixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* RestoredClock = RestoredFixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Restored world exists", RestoredWorld);
        TestNotNull("Restored clock exists", RestoredClock);
        if (RestoredWorld == nullptr || RestoredClock == nullptr)
        {
            return;
        }
        TestTrue("Mid-travel listener snapshot restores", RestoredWorld->RestorePersistentCampaign(SnapshotAfterFirstTravelTick));

        TArray<int64> ResumedTicks;
        const FDelegateHandle ResumeHandle = RestoredClock->OnWorldTick.AddLambda(
            [&ResumedTicks](const int64 WorldTick)
            {
                ResumedTicks.Add(WorldTick);
            });
        FDARecordingTravelRuntime ResumeRuntime;
        TestEqual(
            "Restored travel completes",
            RestoredWorld->Travel(TEXT("travel.mid_tick.resume"), IronheartRegion, 3, ResumeRuntime),
            EDATravelResult::Completed);
        RestoredClock->OnWorldTick.Remove(ResumeHandle);

        const FDAWorldCampaignState& Result = RestoredWorld->GetPersistentState();
        TestEqual("Only the two remaining ticks are broadcast", ResumedTicks.Num(), 2);
        if (ResumedTicks.Num() == 2)
        {
            TestEqual("Resume begins at World Tick twelve", ResumedTicks[0], 12LL);
            TestEqual("Resume ends at World Tick thirteen", ResumedTicks[1], 13LL);
        }
        TestEqual("World authority stops at the original arrival tick", Result.CurrentWorldTick, 13LL);
        TestEqual("Clock authority stops at the original arrival tick", RestoredClock->GetCurrentWorldTick(), 13LL);
        TestEqual("Three deliveries occur without replaying the first", Result.Trade.Contracts[0].DeliveredQuantity, 30LL);
        TestEqual("Source debits exactly thirty", Result.Trade.Inventories[0].Stock[MachineComponents], 20LL);
        TestEqual("Destination credits exactly thirty", Result.Trade.Inventories[1].Stock[MachineComponents], 30LL);
        TestEqual("Diplomacy records exactly three deliveries", Result.Diplomacy.Relationships[0].Trust, 6.f, Tolerance);
    });

    It("rejects arithmetic overflow and out-of-range travel stages in durable state", [this]()
    {
        FDATradeWorldState OverflowingContract = MakeTradeState(50, 10);
        OverflowingContract.Contracts[0].StartWorldTick = MAX_int64 - 1;
        OverflowingContract.Contracts[0].DurationWorldTicks = 5;
        FString ValidationError;
        TestFalse("A contract whose terminal tick overflows is invalid", OverflowingContract.Validate(ValidationError));

        FDAWorldCampaignState InvalidStage;
        InvalidStage.bInitialized = true;
        InvalidStage.Regions = FDARegionSeedCatalog::MakeVerticalSliceRegions();
        InvalidStage.CurrentRegionId = SynaraRegion;
        InvalidStage.CurrentWorldTick = 1;
        InvalidStage.Trade.LastProcessedWorldTick = 1;
        InvalidStage.TravelHandoff.RequestId = TEXT("travel.invalid.stage");
        InvalidStage.TravelHandoff.SourceRegionId = SynaraRegion;
        InvalidStage.TravelHandoff.DestinationRegionId = IronheartRegion;
        InvalidStage.TravelHandoff.DepartureWorldTick = 1;
        InvalidStage.TravelHandoff.TravelWorldTicks = 1;
        InvalidStage.TravelHandoff.Stage = static_cast<EDATravelHandoffStage>(255);
        TestFalse("An out-of-range serialized travel stage is invalid", InvalidStage.Validate(ValidationError));

        FDAWorldCampaignState OverflowingArrival = InvalidStage;
        OverflowingArrival.TravelHandoff.Stage = EDATravelHandoffStage::SourceUnloaded;
        OverflowingArrival.CurrentWorldTick = MAX_int64 - 1;
        OverflowingArrival.Trade.LastProcessedWorldTick = MAX_int64 - 1;
        OverflowingArrival.TravelHandoff.DepartureWorldTick = MAX_int64 - 1;
        OverflowingArrival.TravelHandoff.TravelWorldTicks = 3;
        TestFalse("A travel arrival tick that overflows is invalid", OverflowingArrival.Validate(ValidationError));
    });

    It("leaves every transfer participant unchanged when destination quantity would overflow", [this]()
    {
        UDATradeSystem* Trade = NewObject<UDATradeSystem>();
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>();
        FDATradeWorldState TradeState = MakeTradeState(50, 10);
        TradeState.Inventories[1].Stock[MachineComponents] = MAX_int64;
        FDADiplomacyWorldState DiplomacyState = MakeDiplomacyState();

        TestEqual(
            "The scheduled tick records a failed attempt",
            Trade->ProcessWorldTick(TradeState, DiplomacyState, *Diplomacy, 1),
            EDATradeTickResult::Processed);
        TestEqual("Overflow failure consumes no source stock", TradeState.Inventories[0].Stock[MachineComponents], 50LL);
        TestEqual("Overflow failure preserves destination stock", TradeState.Inventories[1].Stock[MachineComponents], MAX_int64);
        TestEqual("Overflow failure reserves no capacity", TradeState.Routes[0].ReservedCapacityThisTick, 0LL);
        TestEqual("Overflow failure emits no delivery", TradeState.Deliveries.Num(), 0);
        TestEqual("Overflow failure mutates no diplomacy", DiplomacyState.Relationships[0].Trust, 0.f, Tolerance);
    });

    It("replays idempotent load and reconstruction after restoring a reconstruction failure", [this]()
    {
        FDAGameInstanceSubsystemFixture SourceFixture;
        UDAWorldStateSubsystem* SourceWorld = SourceFixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Source world exists", SourceWorld);
        if (SourceWorld == nullptr)
        {
            return;
        }
        TestTrue("Source world initializes", SourceWorld->InitializeVerticalSliceState(SynaraRegion, 40));

        FDARecordingTravelRuntime FailingRuntime;
        FailingRuntime.bFailReconstruct = true;
        TestEqual(
            "Reconstruction failure leaves a resumable handoff",
            SourceWorld->Travel(TEXT("travel.reconstruct.resume"), IronheartRegion, 2, FailingRuntime),
            EDATravelResult::RuntimeFailure);
        TestEqual("The failed process reached destination-loaded", SourceWorld->GetPersistentState().TravelHandoff.Stage, EDATravelHandoffStage::DestinationLoaded);

        FDACampaignSnapshot SavedState = SourceWorld->GetPersistentCampaign();
        FDAGameInstanceSubsystemFixture RestoredFixture;
        UDAWorldStateSubsystem* RestoredWorld = RestoredFixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* RestoredClock = RestoredFixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestTrue("Interrupted state restores", RestoredWorld->RestorePersistentCampaign(SavedState));
        TestEqual(
            "Restore discards ephemeral loaded/reconstructed claims",
            RestoredWorld->GetPersistentState().TravelHandoff.Stage,
            EDATravelHandoffStage::TimeAdvanced);

        FDARecordingTravelRuntime ResumeRuntime;
        TestEqual(
            "Restored travel ensures load and reconstruction again",
            RestoredWorld->Travel(TEXT("travel.reconstruct.resume"), IronheartRegion, 2, ResumeRuntime),
            EDATravelResult::Completed);
        TestEqual("Ensure-loaded is replayed once", ResumeRuntime.LoadCalls, 1);
        TestEqual("Ensure-reconstructed is replayed once", ResumeRuntime.ReconstructCalls, 1);
        TestEqual("Replay is keyed to the durable request", ResumeRuntime.LastLoadRequestId, FName(TEXT("travel.reconstruct.resume")));
        TestNotNull("Restored clock exists", RestoredClock);
        if (RestoredClock != nullptr)
        {
            TestEqual("Strategic time is not replayed", RestoredClock->GetCurrentWorldTick(), 42LL);
        }
    });
}
