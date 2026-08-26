#include "AI/DAForgeweaveStrategy.h"
#include "AI/DARivalCityPlanner.h"
#include "City/DACityGridSubsystem.h"
#include "Combat/DACoverSubsystem.h"
#include "Damage/DAStructuralDamageComponent.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Regions/DARegionAuthorityResolver.h"
#include "Save/DASaveService.h"
#include "Time/DASimulationClockSubsystem.h"
#include "Trade/DATradeSystem.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

BEGIN_DEFINE_SPEC(FDAForgeweaveAISpec, "Dominion.World.ForgeweaveAI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAForgeweaveAISpec)

namespace
{
    constexpr float Tolerance = 0.001f;

    FDAForgeweaveCityState MakeCityState()
    {
        FDAForgeweaveCityState State;
        State.bInitialized = true;
        State.CampaignSeed = 1701;
        State.LastProcessedWorldTick = 0;
        State.GridWidth = FDAForgeweaveCityState::IronheartGridWidth;
        State.GridHeight = FDAForgeweaveCityState::IronheartGridHeight;
        State.AvailableCardIds = FDAForgeweaveCityState::GetVerticalSliceBuildPool();
        State.Capital = 80.f;
        State.ProductionReserve = 40.f;
        State.Population = 24;
        State.HousingCapacity = 8;
        State.DesiredIndustrialOutput = 24.f;
        State.ActiveIndustrialThroughput = 5.f;
        State.MaterialScarcity = 20.f;
        State.UtilitySupply = 40.f;
        return State;
    }

    FDACampaignSnapshot MakeCampaignState()
    {
        FDACampaignSnapshot Campaign;
        Campaign.WorldState.bInitialized = true;
        Campaign.WorldState.CurrentRegionId = TEXT("region.synara_frontier");
        Campaign.WorldState.Regions = FDARegionSeedCatalog::MakeVerticalSliceRegions();
        Campaign.WorldState.Forgeweave = MakeCityState();

        FDARegionalTradeInventory Eden;
        Eden.RegionId = TEXT("region.eden_basin");
        Eden.Stock.Add(TEXT("resource.regenerative_materials"), 1000);
        Campaign.WorldState.Trade.Inventories.Add(Eden);
        FDARegionalTradeInventory Ironheart;
        Ironheart.RegionId = TEXT("region.ironheart");
        Ironheart.Stock.Add(TEXT("resource.regenerative_materials"), 0);
        Campaign.WorldState.Trade.Inventories.Add(Ironheart);
        FDATradeRouteState Route;
        Route.RouteId = TEXT("route.eden_ironheart_relief");
        Route.SourceRegionId = Eden.RegionId;
        Route.DestinationRegionId = Ironheart.RegionId;
        Route.CapacityPerWorldTick = 10;
        Campaign.WorldState.Trade.Routes.Add(Route);
        return Campaign;
    }

    void PrepareCampaignTradeTick(FDACampaignSnapshot& Campaign, const int64 WorldTick)
    {
        Campaign.WorldState.Trade.LastProcessedWorldTick = WorldTick;
        for (FDATradeRouteState& Route : Campaign.WorldState.Trade.Routes)
        {
            Route.CapacityWorldTick = WorldTick;
            Route.ReservedCapacityThisTick = 0;
        }
    }

    bool DecisionUsesOnlyCanonicalPool(const FDARivalPlannerDecision& Decision)
    {
        return Decision.Type != EDARivalDecisionType::Construct
            || FDARivalCityPlanner::IsVerticalSliceBuildCard(Decision.CardDefinitionId);
    }

    class FDAForgeweaveReconstructionRuntime final : public IDARegionTravelRuntime
    {
    public:
        explicit FDAForgeweaveReconstructionRuntime(UDACoverSubsystem& InCover)
            : Cover(InCover)
            , Resolver(NewObject<UDARegionAuthorityResolver>())
        {
        }

        virtual bool SnapshotPersistentDelta(
            const FDARegionState& SourceRegion,
            FDARegionPersistentDelta& OutDelta) override
        {
            OutDelta = SourceRegion.PersistentDelta;
            if (OutDelta.Revision == MAX_int64)
            {
                return false;
            }
            ++OutDelta.Revision;
            return true;
        }

        virtual bool UnloadRegionRepresentation(FName) override { return true; }
        virtual bool EnsureRegionLoaded(FName, FName) override { return true; }

        virtual bool EnsureRegionReconstructed(
            FName,
            const FDARegionState& Region,
            const FDACampaignSnapshot& Campaign) override
        {
            if (!Resolver->ReconstructRegion(Region, Campaign, Cover))
            {
                return false;
            }
            for (const FDARegionActorState& Actor : Region.PersistentDelta.LocalActors)
            {
                if (Actor.WorldAssetId.IsValid())
                {
                    ReconstructedWorldAssets.Add(Actor.WorldAssetId);
                }
            }
            return true;
        }

        bool ReconstructedWorldAsset(const FGuid WorldAssetId) const
        {
            return ReconstructedWorldAssets.Contains(WorldAssetId);
        }

        float GetReconstructedIntegrity(const FGuid WorldAssetId) const
        {
            const FDAWorldAssetRecord* Asset = Resolver->FindReconstructedWorldAssetRecord(WorldAssetId);
            return Asset != nullptr ? Asset->StructuralIntegrity : 0.f;
        }

        float GetReconstructedModuleHealth(const FGuid WorldAssetId) const
        {
            const UDAStructuralDamageComponent* Damage = Resolver->FindStructuralDamageComponent(WorldAssetId);
            const FDAStructureModuleHealthRecord* Module = Damage != nullptr
                ? Damage->FindModule(TEXT("industrial_core"))
                : nullptr;
            return Module != nullptr ? Module->CurrentHealth : 0.f;
        }

        bool ReconstructIronheart(const FDACampaignSnapshot& Campaign)
        {
            const FDARegionState* Region = Campaign.WorldState.FindRegion(TEXT("region.ironheart"));
            return Region != nullptr && Resolver->ReconstructRegion(*Region, Campaign, Cover);
        }

        bool TracksCover(const FName CoverId) const
        {
            return Resolver->IsCoverRegisteredByResolver(CoverId);
        }

    private:
        UDACoverSubsystem& Cover;
        TStrongObjectPtr<UDARegionAuthorityResolver> Resolver;
        TSet<FGuid> ReconstructedWorldAssets;
    };
}

void FDAForgeweaveAISpec::Define()
{
    It("exposes exactly the frozen six-card construction pool and never selects an injected card", [this]()
    {
        const TArray<FName>& Pool = FDARivalCityPlanner::GetVerticalSliceBuildPool();
        const TArray<FName> Expected = {
            TEXT("forgeweave.worker_arcology"),
            TEXT("forgeweave.production_directorate"),
            TEXT("forgeweave.industrial_exchange"),
            TEXT("forgeweave.infinite_foundry"),
            TEXT("forgeweave.freight_furnace"),
            TEXT("forgeweave.smog_reclaimer")
        };

        TestEqual("The pool contains exactly six cards", Pool.Num(), 6);
        TestEqual("Pool ordering is stable", Pool, Expected);
        for (const FName CardId : Expected)
        {
            TestNotNull(*FString::Printf(TEXT("%s has deterministic planning metadata"), *CardId.ToString()),
                FDARivalCityPlanner::FindBuildCandidate(CardId));
        }
        TestFalse(
            "An out-of-scope authored card is rejected",
            FDARivalCityPlanner::IsVerticalSliceBuildCard(TEXT("forgeweave.replication_forge")));

        FDAForgeweaveCityState State = MakeCityState();
        State.AvailableCardIds = Pool;
        State.AvailableCardIds.Add(TEXT("forgeweave.replication_forge"));
        State.AvailableCardIds.Add(TEXT("synara.adaptive_habitat"));
        FString InvalidPoolError;
        TestFalse("Injected candidates cannot become durable rival authority", State.Validate(InvalidPoolError));
        FDACityGridSubsystem Grid(State.GridWidth, State.GridHeight);
        Grid.SetAllCellsClaimed(true);
        for (int32 Y = 0; Y < State.GridHeight; ++Y)
        {
            for (int32 X = 0; X < State.GridWidth; ++X)
            {
                Grid.SetCellLayerMask(FIntPoint(X, Y), 1);
            }
        }

        FDARivalCityPlanner Planner;
        const FDARivalPlannerDecision Decision = Planner.ChooseConstruction(State, Grid, 1);
        TestEqual("A construction decision is available", Decision.Type, EDARivalDecisionType::Construct);
        TestTrue("Injected candidates cannot escape the frozen pool", DecisionUsesOnlyCanonicalPool(Decision));
    });

    It("scores housing output Hunger defense affordability and placement as independent deterministic terms", [this]()
    {
        FDARivalBuildCandidate Candidate;
        Candidate.CardDefinitionId = TEXT("forgeweave.test_fixture");
        Candidate.DeploymentCapital = 20.f;
        Candidate.ProductionThroughputCost = 3.f;
        Candidate.HousingCapacity = 3.f;
        Candidate.IndustrialOutput = 4.f;
        Candidate.ResourceHungerMitigation = 5.f;
        Candidate.DefenseValue = 6.f;

        FDARivalPlannerContext Context;
        Context.HousingShortage = 2.f;
        Context.OutputShortage = 3.f;
        Context.ResourceHunger = 4.f;
        Context.DefensePressure = 5.f;
        Context.AvailableCapital = 30.f;
        Context.AvailableProductionThroughput = 10.f;

        const FDARivalCandidateScore Score = FDARivalCityPlanner::ScoreCandidate(
            Candidate,
            Context,
            FDAPlacementResult::Success());

        TestTrue("An affordable valid placement is eligible", Score.bEligible);
        TestEqual("Housing term is explicit", Score.Housing, 6.f, Tolerance);
        TestEqual("Output term is explicit", Score.Output, 12.f, Tolerance);
        TestEqual("Resource Hunger term is explicit", Score.ResourceHunger, 20.f, Tolerance);
        TestEqual("Defense term is explicit", Score.Defense, 30.f, Tolerance);
        TestEqual("Affordability term rewards the literal ten-Capital reserve", Score.Affordability, 30.f, Tolerance);
        TestEqual("Placement term is explicit", Score.Placement, 25.f, Tolerance);
        TestEqual("Total is the sum of six independently inspectable terms", Score.Total, 123.f, Tolerance);

        Context.AvailableCapital = 19.f;
        const FDARivalCandidateScore Unaffordable = FDARivalCityPlanner::ScoreCandidate(
            Candidate,
            Context,
            FDAPlacementResult::Success());
        TestFalse("An unaffordable candidate is ineligible", Unaffordable.bEligible);

        Context.AvailableCapital = 30.f;
        Context.AvailableProductionThroughput = 2.f;
        const FDARivalCandidateScore InsufficientProduction = FDARivalCityPlanner::ScoreCandidate(
            Candidate,
            Context,
            FDAPlacementResult::Success());
        TestFalse("A candidate cannot spend unavailable Production Throughput", InsufficientProduction.bEligible);

        Context.AvailableCapital = 30.f;
        Context.AvailableProductionThroughput = 10.f;
        const FDARivalCandidateScore InvalidPlacement = FDARivalCityPlanner::ScoreCandidate(
            Candidate,
            Context,
            FDAPlacementResult::Failure(EDAPlacementFailureReason::Occupied));
        TestFalse("An invalid placement is ineligible", InvalidPlacement.bEligible);
        TestEqual(
            "The canonical placement failure is preserved",
            InvalidPlacement.PlacementFailure,
            EDAPlacementFailureReason::Occupied);
    });

    It("uses campaign seed World Tick and stable card ordering to break exact score ties reproducibly", [this]()
    {
        FDAForgeweaveCityState State = MakeCityState();
        State.Population = State.HousingCapacity;
        State.DesiredIndustrialOutput = State.ActiveIndustrialThroughput;
        State.ResourceHunger = 0.f;
        State.DefensePressure = 0.f;

        FDACityGridSubsystem Grid(State.GridWidth, State.GridHeight);
        Grid.SetAllCellsClaimed(true);
        for (int32 Y = 0; Y < State.GridHeight; ++Y)
        {
            for (int32 X = 0; X < State.GridWidth; ++X)
            {
                Grid.SetCellLayerMask(FIntPoint(X, Y), 1);
            }
        }

        FDARivalCityPlanner Planner;
        const FDARivalPlannerDecision First = Planner.ChooseConstruction(State, Grid, 37);
        const FDARivalPlannerDecision Replay = Planner.ChooseConstruction(State, Grid, 37);
        TestEqual("Seeded replay picks the same card", Replay.CardDefinitionId, First.CardDefinitionId);
        TestEqual("Seeded replay picks the same cell", Replay.Origin, First.Origin);
        TestEqual("Seeded replay retains the same tie key", Replay.DeterministicTieKey, First.DeterministicTieKey);
    });

    It("uses the canonical 32 by 32 Ironheart grid and shared eight-meter cell conversion", [this]()
    {
        const FDAForgeweaveCityState State = FDAForgeweaveCityState::MakeVerticalSliceInitialState(1701, 0);
        TestEqual("Ironheart width is canonical", State.GridWidth, 32);
        TestEqual("Ironheart height is canonical", State.GridHeight, 32);
        TestEqual("Simulation consumes the same canonical width", FDACityGridSubsystem::DefaultGridWidth, State.GridWidth);
        TestEqual("Simulation consumes the same canonical height", FDACityGridSubsystem::DefaultGridHeight, State.GridHeight);
        TestEqual("Ironheart cell size is eight meters", FDAForgeweaveCityState::IronheartCellSizeMeters, 8.f, Tolerance);
        TestTrue(
            "Cell conversion is shared and uses Unreal centimeters",
            FDAForgeweaveCityState::GridCellToWorld(FIntPoint(3, 4)).Equals(FVector(2400.f, 3200.f, 0.f)));
    });

    It("requires every uninitialized world and Forgeweave field to be canonical empty", [this]()
    {
        FString Error;
        FDAForgeweaveCityState EmptyForgeweave;
        TestTrue("Default Forgeweave state is canonical empty", EmptyForgeweave.Validate(Error));
        EmptyForgeweave.Capital = 1.f;
        TestFalse("A scalar hidden in uninitialized Forgeweave state is rejected", EmptyForgeweave.Validate(Error));

        FDAWorldCampaignState EmptyWorld;
        TestTrue("Default world state is canonical empty", EmptyWorld.Validate(Error));
        EmptyWorld.Forgeweave.Capital = 1.f;
        TestFalse("Uninitialized world validation still validates nested Forgeweave authority", EmptyWorld.Validate(Error));
    });

    It("calculates bounded Resource Hunger from every frozen growth and mitigation driver", [this]()
    {
        FDAResourceHungerInputs Inputs;
        Inputs.ActiveIndustrialThroughput = 40.f;
        Inputs.MaterialScarcity = 50.f;
        Inputs.bOverdrive = true;
        Inputs.LogisticsEfficiency = 20.f;
        Inputs.Recycling = 10.f;
        Inputs.EdenRegenerativeInputs = 10.f;
        Inputs.ProductionReduction = 10.f;

        const FDAResourceHungerTick Tick = UDAForgeweaveStrategy::CalculateResourceHungerTick(50.f, Inputs);
        TestEqual("Throughput growth is traceable", Tick.ThroughputGrowth, 2.4f, Tolerance);
        TestEqual("Scarcity growth is traceable", Tick.ScarcityGrowth, 2.f, Tolerance);
        TestEqual("Overdrive growth is traceable", Tick.OverdriveGrowth, 4.f, Tolerance);
        TestEqual("Logistics mitigation is traceable", Tick.LogisticsMitigation, 0.5f, Tolerance);
        TestEqual("Recycling mitigation is traceable", Tick.RecyclingMitigation, 0.4f, Tolerance);
        TestEqual("Eden mitigation is traceable", Tick.EdenInputMitigation, 0.5f, Tolerance);
        TestEqual("Production reduction mitigation is traceable", Tick.ProductionReductionMitigation, 0.4f, Tolerance);
        TestEqual("The resulting Hunger is literal growth minus mitigation", Tick.ResultingResourceHunger, 56.6f, Tolerance);

        Inputs.ActiveIndustrialThroughput = 1000.f;
        Inputs.MaterialScarcity = 1000.f;
        const FDAResourceHungerTick UpperBound = UDAForgeweaveStrategy::CalculateResourceHungerTick(99.f, Inputs);
        TestEqual("Resource Hunger cannot exceed 100", UpperBound.ResultingResourceHunger, 100.f, Tolerance);

        Inputs = FDAResourceHungerInputs{};
        Inputs.LogisticsEfficiency = 1000.f;
        Inputs.Recycling = 1000.f;
        Inputs.EdenRegenerativeInputs = 1000.f;
        Inputs.ProductionReduction = 1000.f;
        const FDAResourceHungerTick LowerBound = UDAForgeweaveStrategy::CalculateResourceHungerTick(1.f, Inputs);
        TestEqual("Resource Hunger cannot fall below zero", LowerBound.ResultingResourceHunger, 0.f, Tolerance);
    });

    It("commits only affordable valid construction and stable-record repair decisions", [this]()
    {
        UDAForgeweaveStrategy* Strategy = NewObject<UDAForgeweaveStrategy>();

        FDACampaignSnapshot ConstructionCampaign = MakeCampaignState();
        PrepareCampaignTradeTick(ConstructionCampaign, 1);
        const float ProductionBeforeConstruction = ConstructionCampaign.WorldState.Forgeweave.ProductionReserve;
        const FDAForgeweaveTickResult Construction = Strategy->ProcessWorldTick(ConstructionCampaign, 1);
        TestTrue("Construction tick commits", Construction.bCommitted);
        TestEqual("Construction is selected for baseline shortage", Construction.Decision.Type, EDARivalDecisionType::Construct);
        TestTrue("Construction remains inside the exact pool", DecisionUsesOnlyCanonicalPool(Construction.Decision));
        TestTrue("Construction reserves a real building", ConstructionCampaign.WorldState.Forgeweave.Buildings.Num() == 1);
        TestEqual("Construction appends one canonical WorldAsset", ConstructionCampaign.WorldAssets.Num(), 1);
        TestTrue("Construction never overspends", ConstructionCampaign.WorldState.Forgeweave.Capital >= 0.f);
        TestTrue("Construction consumes earned Production Throughput", ConstructionCampaign.WorldState.Forgeweave.ProductionReserve < ProductionBeforeConstruction);

        FDAForgeweaveCityState RepairState = MakeCityState();
        TestEqual("City-only planning cannot invent a parallel repair authority", RepairState.Buildings.Num(), 0);
    });

    It("refuses construction trade and defense when their placement utility route or economy preconditions fail", [this]()
    {
        UDAForgeweaveStrategy* Strategy = NewObject<UDAForgeweaveStrategy>();

        FDACampaignSnapshot UtilityBlocked = MakeCampaignState();
        UtilityBlocked.WorldState.Forgeweave.UtilitySupply = 0.f;
        PrepareCampaignTradeTick(UtilityBlocked, 1);
        const FDAForgeweaveTickResult NoConstruction = Strategy->ProcessWorldTick(UtilityBlocked, 1);
        TestTrue("An explained utility crisis still commits time", NoConstruction.bCommitted);
        TestEqual("Impossible utility cannot construct", NoConstruction.Decision.Type, EDARivalDecisionType::None);
        TestEqual("Utility deadlock is explicitly explained", NoConstruction.Decision.CrisisExplanation, FName(TEXT("crisis.utility_capacity")));
        TestEqual("No invalid building is created", UtilityBlocked.WorldState.Forgeweave.Buildings.Num(), 0);

        FDACampaignSnapshot NoRoute = MakeCampaignState();
        NoRoute.WorldState.Forgeweave.MaterialScarcity = 80.f;
        NoRoute.WorldState.Trade.Routes.Reset();
        PrepareCampaignTradeTick(NoRoute, 1);
        const FDAForgeweaveTickResult RouteBlocked = Strategy->ProcessWorldTick(NoRoute, 1);
        TestTrue("A route-blocked tick still commits a different valid decision", RouteBlocked.bCommitted);
        TestTrue("Zero route capacity cannot produce a trade", RouteBlocked.Decision.Type != EDARivalDecisionType::Trade);

        FDACampaignSnapshot NoDefenseEconomy = MakeCampaignState();
        NoDefenseEconomy.WorldState.Forgeweave.DefensePressure = 80.f;
        NoDefenseEconomy.WorldState.Forgeweave.ActiveIndustrialThroughput = 0.f;
        NoDefenseEconomy.WorldState.Forgeweave.ProductionReserve = 0.f;
        PrepareCampaignTradeTick(NoDefenseEconomy, 1);
        const FDAForgeweaveTickResult DefenseBlocked = Strategy->ProcessWorldTick(NoDefenseEconomy, 1);
        TestTrue("An economy-blocked defense tick remains valid", DefenseBlocked.bCommitted);
        TestTrue("Defense cannot spend unavailable Production Throughput", DefenseBlocked.Decision.Type != EDARivalDecisionType::Fortify);
    });

    It("counts only validated crisis causes as explained and rejects the eleventh generic idle tick", [this]()
    {
        UDAForgeweaveStrategy* Strategy = NewObject<UDAForgeweaveStrategy>();

        FDACampaignSnapshot GenericMiss = MakeCampaignState();
        GenericMiss.WorldState.Forgeweave.DesiredIndustrialOutput = TNumericLimits<float>::Max();
        GenericMiss.WorldState.Forgeweave.ActiveIndustrialThroughput = 0.f;
        for (int64 Tick = 1; Tick <= 10; ++Tick)
        {
            PrepareCampaignTradeTick(GenericMiss, Tick);
            const FDAForgeweaveTickResult Result = Strategy->ProcessWorldTick(GenericMiss, Tick);
            TestTrue(*FString::Printf(TEXT("Generic miss %lld commits inside the grace window"), Tick), Result.bCommitted);
            TestEqual("A generic planner miss has no crisis explanation", Result.Decision.CrisisExplanation, NAME_None);
        }
        const FDAForgeweaveCityState BeforeRejectedIdle = GenericMiss.WorldState.Forgeweave;
        PrepareCampaignTradeTick(GenericMiss, 11);
        const FDAForgeweaveTickResult RejectedIdle = Strategy->ProcessWorldTick(GenericMiss, 11);
        TestFalse("The eleventh unexplained idle tick is rejected", RejectedIdle.bCommitted);
        TestEqual("Rejected idle does not advance rival authority", GenericMiss.WorldState.Forgeweave.LastProcessedWorldTick, BeforeRejectedIdle.LastProcessedWorldTick);

        FDACampaignSnapshot Explained = MakeCampaignState();
        Explained.WorldState.Forgeweave.UtilitySupply = 0.f;
        for (int64 Tick = 1; Tick <= 11; ++Tick)
        {
            PrepareCampaignTradeTick(Explained, Tick);
            const FDAForgeweaveTickResult Result = Strategy->ProcessWorldTick(Explained, Tick);
            TestTrue(*FString::Printf(TEXT("Validated utility crisis %lld commits"), Tick), Result.bCommitted);
            TestEqual("Validated utility shortage is explained", Result.Decision.CrisisExplanation, FName(TEXT("crisis.utility_capacity")));
        }
        TestEqual("Explained idling never increments unexplained deadlock", Explained.WorldState.Forgeweave.ConsecutiveUnexplainedIdleWorldTicks, 0);
    });

    It("advances durable Forgeweave state on the canonical post-commit World Tick snapshot", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("World state subsystem exists", World);
        if (World == nullptr)
        {
            return;
        }

        TestTrue("Vertical-slice world initializes", World->InitializeVerticalSliceState(TEXT("region.synara_frontier")));
        const int32 PlayerCollectionBefore =
            World->GetPersistentCampaign().CollectionState.Instances.Num();
        TSharedPtr<const FDACampaignSnapshot> Snapshot;
        const FDelegateHandle Handle = World->OnWorldTickStateCommitted.AddLambda(
            [&Snapshot](const FDACommittedCampaignSnapshot CommittedState)
            {
                Snapshot = CommittedState;
            });

        TestTrue("Canonical clock advances one strategic tick", World->AdvanceWorldTicks(1));
        World->OnWorldTickStateCommitted.Remove(Handle);
        TestTrue("A committed immutable snapshot is published", Snapshot.IsValid());
        if (!Snapshot.IsValid())
        {
            return;
        }

        TestEqual("World authority commits Tick one", Snapshot->WorldState.CurrentWorldTick, 1LL);
        TestEqual("Forgeweave durable authority commits the same tick", Snapshot->WorldState.Forgeweave.LastProcessedWorldTick, 1LL);
        TestEqual("The committed decision is durable", Snapshot->WorldState.Forgeweave.DecisionHistory.Num(), 1);
        TestEqual("Construction adds one rival asset beside the Founder Hall",
            Snapshot->WorldAssets.Num(), 2);
        TestEqual("Rival construction never enters the player collection",
            Snapshot->CollectionState.Instances.Num(), PlayerCollectionBefore);
        const FDAForgeweaveBuildingState* RivalBuilding =
            Snapshot->WorldState.Forgeweave.Buildings.IsEmpty()
                ? nullptr : &Snapshot->WorldState.Forgeweave.Buildings[0];
        const FDAWorldAssetRecord* RivalAsset = RivalBuilding == nullptr
            ? nullptr : Snapshot->FindWorldAssetRecord(RivalBuilding->WorldAssetId);
        TestTrue("Rival provenance links only regional AI and canonical WorldAsset state",
            RivalBuilding != nullptr && RivalAsset != nullptr
                && RivalBuilding->ProvenanceId.IsValid()
                && RivalBuilding->CardDefinitionId == RivalAsset->CardDefinitionId
                && !RivalAsset->CardInstanceId.IsValid());
        bool bPlayerOwnsRivalCard = false;
        for (const TPair<FGuid, FCardInstance>& Pair
            : Snapshot->CollectionState.Instances)
        {
            bPlayerOwnsRivalCard = bPlayerOwnsRivalCard
                || Pair.Value.DefinitionId.ToString().StartsWith(TEXT("forgeweave."));
        }
        TestFalse("No rival definition appears in the player-owned collection",
            bPlayerOwnsRivalCard);
        const FDARegionState* Ironheart = Snapshot->WorldState.FindRegion(TEXT("region.ironheart"));
        TestNotNull("Ironheart remains addressable", Ironheart);
        if (Ironheart != nullptr)
        {
            TestEqual("Construction is exposed to region reconstruction", Ironheart->PersistentDelta.LocalActors.Num(), 1);
            if (Ironheart->PersistentDelta.LocalActors.Num() == 1)
            {
                TestTrue("The reconstructable actor is from the exact pool",
                    FDARivalCityPlanner::IsVerticalSliceBuildCard(Ironheart->PersistentDelta.LocalActors[0].DefinitionId));
            }
        }

        FDAGameInstanceSubsystemFixture RestoredFixture;
        UDAWorldStateSubsystem* RestoredWorld = RestoredFixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Restored world state subsystem exists", RestoredWorld);
        if (RestoredWorld == nullptr)
        {
            return;
        }
        TestTrue("Committed Forgeweave campaign restores", RestoredWorld->RestorePersistentCampaign(*Snapshot));
        TestTrue("Restored canonical authority advances without replay", RestoredWorld->AdvanceWorldTicks(1));
        TestEqual("Restored Forgeweave authority reaches Tick two", RestoredWorld->GetPersistentState().Forgeweave.LastProcessedWorldTick, 2LL);
        TestEqual("Restore retains one decision and appends exactly one", RestoredWorld->GetPersistentState().Forgeweave.DecisionHistory.Num(), 2);
    });

    It("keeps canonical clock and world authority aligned when reconstruction revision rejects then recovers", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("World exists", World);
        TestNotNull("Clock exists", Clock);
        if (World == nullptr || Clock == nullptr)
        {
            return;
        }

        TestTrue("World initializes", World->InitializeVerticalSliceState(TEXT("region.synara_frontier")));
        FDAWorldCampaignState Blocked = World->GetPersistentState();
        FDARegionState* Ironheart = Blocked.FindRegion(TEXT("region.ironheart"));
        TestNotNull("Ironheart exists", Ironheart);
        if (Ironheart == nullptr)
        {
            return;
        }
        Ironheart->PersistentDelta.Revision = MAX_int64;
        TestTrue("Max-revision state restores before a construction attempt", World->RestorePersistentState(Blocked));

        TestFalse("Rejected construction does not acknowledge canonical time", World->AdvanceWorldTicks(1));
        TestEqual("Clock stays at World Tick zero", Clock->GetCurrentWorldTick(), 0LL);
        TestEqual("Clock keeps zero Development Cycles", Clock->GetCurrentDevelopmentCycle(), 0LL);
        TestEqual("World stays at World Tick zero", World->GetPersistentState().CurrentWorldTick, 0LL);
        TestEqual("Rival stays at World Tick zero", World->GetPersistentState().Forgeweave.LastProcessedWorldTick, 0LL);

        FDAWorldCampaignState Recovered = World->GetPersistentState();
        Recovered.FindRegion(TEXT("region.ironheart"))->PersistentDelta.Revision = 0;
        TestTrue("Corrected state restores without clock rollback", World->RestorePersistentState(Recovered));
        TestTrue("The same pending World Tick becomes recoverable", World->AdvanceWorldTicks(1));
        TestEqual("Clock reaches World Tick one once", Clock->GetCurrentWorldTick(), 1LL);
        TestEqual("World reaches World Tick one once", World->GetPersistentState().CurrentWorldTick, 1LL);
    });

    It("aborts a fully staged campaign when a later listener vetoes and retries without duplicate mutation", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("World exists", World);
        TestNotNull("Clock exists", Clock);
        if (World == nullptr || Clock == nullptr)
        {
            return;
        }
        TestTrue("World initializes", World->InitializeVerticalSliceState(TEXT("region.synara_frontier")));
        const FDelegateHandle LaterVeto = Clock->OnWorldTickPreCommit.AddLambda(
            [](const int64, FDAWorldTickVeto& Veto) { Veto.Reject(); });

        TestFalse("Later veto aborts after World has staged its candidate", World->AdvanceWorldTicks(1));
        TestEqual("Aborted campaign remains at Tick zero", World->GetPersistentState().CurrentWorldTick, 0LL);
        TestEqual("Aborted campaign has no durable decision", World->GetPersistentState().Forgeweave.DecisionHistory.Num(), 0);
        Clock->OnWorldTickPreCommit.Remove(LaterVeto);

        TestTrue("Retry commits the same pending World Tick", World->AdvanceWorldTicks(1));
        TestEqual("Retry advances authority once", World->GetPersistentState().CurrentWorldTick, 1LL);
        TestEqual("Retry appends one decision only", World->GetPersistentState().Forgeweave.DecisionHistory.Num(), 1);
        TestEqual("Retry creates one rival asset beside the Founder Hall only",
            World->GetPersistentCampaign().WorldAssets.Num(), 2);
    });

    It("commits repair trade and defense through durable structural trade and cover authorities", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("World exists", World);
        if (World == nullptr)
        {
            return;
        }
        TestTrue("World initializes", World->InitializeVerticalSliceState(TEXT("region.synara_frontier")));

        FDACampaignSnapshot TradeSetup = World->GetPersistentCampaign();
        TradeSetup.WorldState.Forgeweave.MaterialScarcity = 80.f;
        const FDARegionalTradeInventory* SourceBefore = TradeSetup.WorldState.Trade.FindInventory(TEXT("region.eden_basin"));
        const FDARegionalTradeInventory* SinkBefore = TradeSetup.WorldState.Trade.FindInventory(TEXT("region.ironheart"));
        TestNotNull("Relief source inventory exists", SourceBefore);
        TestNotNull("Ironheart inventory exists", SinkBefore);
        if (SourceBefore == nullptr || SinkBefore == nullptr)
        {
            return;
        }
        const int64 SourceStockBefore = SourceBefore->Stock.FindChecked(TEXT("resource.regenerative_materials"));
        const int64 SinkStockBefore = SinkBefore->Stock.FindChecked(TEXT("resource.regenerative_materials"));
        TestTrue("Trade setup restores", World->RestorePersistentCampaign(TradeSetup));
        TestTrue("Trade World Tick commits", World->AdvanceWorldTicks(1));

        const FDACampaignSnapshot AfterTrade = World->GetPersistentCampaign();
        TestEqual("Decision is Trade", AfterTrade.WorldState.Forgeweave.DecisionHistory.Last().Type, EDARivalDecisionType::Trade);
        TestEqual("Spot trade has one durable order", AfterTrade.WorldState.Trade.SpotOrders.Num(), 1);
        TestEqual("Spot trade has one durable delivery", AfterTrade.WorldState.Trade.Deliveries.Num(), 1);
        TestEqual("Source inventory is debited", AfterTrade.WorldState.Trade.FindInventory(TEXT("region.eden_basin"))->Stock[TEXT("resource.regenerative_materials")], SourceStockBefore - 10);
        TestEqual("Ironheart inventory is credited", AfterTrade.WorldState.Trade.FindInventory(TEXT("region.ironheart"))->Stock[TEXT("resource.regenerative_materials")], SinkStockBefore + 10);
        const FDAForgeweaveActionTransaction& TradeTransaction = AfterTrade.WorldState.Forgeweave.ActionTransactions.Last();
        TestEqual("Trade transaction captures source opening quantity", TradeTransaction.SourceQuantityBefore, SourceStockBefore);
        TestEqual("Trade transaction captures source closing quantity", TradeTransaction.SourceQuantityAfter, SourceStockBefore - 10);
        TestEqual("Trade transaction captures destination opening quantity", TradeTransaction.DestinationQuantityBefore, SinkStockBefore);
        TestEqual("Trade transaction captures destination closing quantity", TradeTransaction.DestinationQuantityAfter, SinkStockBefore + 10);

        FDACampaignSnapshot DefenseSetup = AfterTrade;
        DefenseSetup.WorldState.Forgeweave.MaterialScarcity = 20.f;
        DefenseSetup.WorldState.Forgeweave.DefensePressure = 80.f;
        TestTrue("Defense setup restores", World->RestorePersistentCampaign(DefenseSetup));
        TestTrue("Defense World Tick commits", World->AdvanceWorldTicks(1));
        const FDACampaignSnapshot& AfterDefense = World->GetPersistentCampaign();
        TestEqual("Decision is Fortify", AfterDefense.WorldState.Forgeweave.DecisionHistory.Last().Type, EDARivalDecisionType::Fortify);
        const FDARegionActorState* DefenseActor = AfterDefense.WorldState.FindRegion(TEXT("region.ironheart"))->PersistentDelta.LocalActors.FindByPredicate(
            [](const FDARegionActorState& Actor) { return Actor.DefinitionId == TEXT("forgeweave.defense_cover"); });
        TestNotNull("Defense creates a durable reconstruction actor", DefenseActor);
        const FName DefenseActorId = DefenseActor != nullptr ? DefenseActor->ActorId : NAME_None;
        const FTransform DefenseActorTransform = DefenseActor != nullptr ? DefenseActor->Transform : FTransform::Identity;
        const FDACampaignSnapshot DefenseAuthority = AfterDefense;
        const FDAForgeweaveActionTransaction& DefenseTransaction = AfterDefense.WorldState.Forgeweave.ActionTransactions.Last();
        TestEqual("Defense transaction captures hardened cover type", DefenseTransaction.CoverTypeId, FName(TEXT("cover.hardened")));
        TestTrue("Defense transaction captures exact transform", DefenseActor != nullptr && DefenseTransaction.ActorTransform.Equals(DefenseActor->Transform));

        const FString SaveDirectory = FPaths::Combine(
            FPaths::ProjectIntermediateDir(),
            TEXT("ForgeweaveAuthorityTests"),
            FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService SaveService(SaveDirectory);
        FDACampaignSnapshot Campaign;
        Campaign = DefenseAuthority;
        TestTrue("Authoritative trade and defense state saves", SaveService.SaveCampaign(Campaign, TEXT("authority")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(TEXT("authority"));
        TestTrue("Authoritative trade and defense state loads", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            TestEqual("Spot-order authority survives save/load", Loaded.GetValue().WorldState.Trade.SpotOrders.Num(), 1);
            TestEqual("Trade and fortify transaction openings survive save/load", Loaded.GetValue().WorldState.Forgeweave.ActionTransactions.Num(), 2);
            const FDARegionState* LoadedIronheart = Loaded.GetValue().WorldState.FindRegion(TEXT("region.ironheart"));
            TestNotNull("Loaded Ironheart remains present", LoadedIronheart);
            TestTrue("Defense actor survives save/load", LoadedIronheart != nullptr
                && LoadedIronheart->PersistentDelta.LocalActors.ContainsByPredicate(
                    [DefenseActorId](const FDARegionActorState& Actor) { return Actor.ActorId == DefenseActorId; }));
        }
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*SaveDirectory);

        UWorld* CoverWorld = NewObject<UWorld>(GetTransientPackage());
        UDACoverSubsystem* Cover = NewObject<UDACoverSubsystem>(CoverWorld);
        FDAForgeweaveReconstructionRuntime Runtime(*Cover);
        TestEqual("Travel reconstructs Ironheart", World->Travel(TEXT("travel.forgeweave.authority"), TEXT("region.ironheart"), 1, Runtime), EDATravelResult::Completed);
        TestTrue("Streamed reconstruction registers the durable fortification with cover authority", !DefenseActorId.IsNone() && Cover->FindCoverSocket(DefenseActorId) != nullptr);

        const FName ReplacementDefenseId(TEXT("forgeweave.defense.replacement"));
        FDACampaignSnapshot ReplacementDefense = DefenseAuthority;
        FDARegionState* ReplacementIronheart = ReplacementDefense.WorldState.FindRegion(TEXT("region.ironheart"));
        FDARegionActorState* ReplacementActor = ReplacementIronheart != nullptr
            ? ReplacementIronheart->PersistentDelta.LocalActors.FindByPredicate(
                [DefenseActorId](const FDARegionActorState& Actor) { return Actor.ActorId == DefenseActorId; })
            : nullptr;
        FDAForgeweaveDecisionRecord* ReplacementDecision = ReplacementDefense.WorldState.Forgeweave.DecisionHistory.FindByPredicate(
            [DefenseActorId](const FDAForgeweaveDecisionRecord& Decision)
            {
                return Decision.Type == EDARivalDecisionType::Fortify
                    && Decision.TargetBuildingId == DefenseActorId;
            });
        FDAForgeweaveActionTransaction* ReplacementTransaction = ReplacementDefense.WorldState.Forgeweave.ActionTransactions.FindByPredicate(
            [DefenseActorId](const FDAForgeweaveActionTransaction& Transaction)
            {
                return Transaction.Type == EDARivalDecisionType::Fortify
                    && Transaction.AuthorityId == DefenseActorId;
            });
        TestTrue("Replacement defense fixture resolves every canonical authority", ReplacementActor != nullptr
            && ReplacementDecision != nullptr
            && ReplacementTransaction != nullptr);
        if (ReplacementActor == nullptr || ReplacementDecision == nullptr || ReplacementTransaction == nullptr)
        {
            return;
        }
        ReplacementActor->ActorId = ReplacementDefenseId;
        ReplacementDecision->TargetBuildingId = ReplacementDefenseId;
        ReplacementTransaction->AuthorityId = ReplacementDefenseId;
        FString ReplacementError;
        TestTrue("Replacement defense fixture is canonical", ReplacementDefense.Validate(ReplacementError));

        TestTrue("External removal creates a missing tracked stale socket", Cover->UnregisterAuthoredCoverSocket(DefenseActorId));
        TestFalse("Missing tracked stale cover rejects reconstruction before candidate mutation", Runtime.ReconstructIronheart(ReplacementDefense));
        TestEqual("Missing stale rejection leaves cover authority unchanged", Cover->GetRegisteredCoverCount(), 0);
        TestNull("Missing stale rejection does not register replacement cover", Cover->FindCoverSocket(ReplacementDefenseId));
        TestTrue("Missing stale rejection leaves resolver tracking unchanged", Runtime.TracksCover(DefenseActorId));

        if (Cover->FindCoverSocket(ReplacementDefenseId) != nullptr)
        {
            Cover->UnregisterAuthoredCoverSocket(ReplacementDefenseId);
        }
        TestTrue("A wrong-source stale socket can occupy the tracked id", Cover->RegisterDeployableCover(
            DefenseActorId,
            EDACoverType::Hardened,
            DefenseActorTransform.GetLocation()));
        const FDACoverSocket* WrongSourceBefore = Cover->FindCoverSocket(DefenseActorId);
        if (WrongSourceBefore == nullptr)
        {
            AddError(TEXT("Wrong-source cover fixture must be present."));
            return;
        }
        const FDACoverSocket WrongSourceSnapshot = *WrongSourceBefore;
        TestFalse("Wrong-source tracked stale cover rejects reconstruction before candidate mutation", Runtime.ReconstructIronheart(ReplacementDefense));
        const FDACoverSocket* WrongSourceAfter = Cover->FindCoverSocket(DefenseActorId);
        TestEqual("Wrong-source stale rejection preserves exact cover count", Cover->GetRegisteredCoverCount(), 1);
        TestTrue("Wrong-source stale rejection preserves the socket exactly", WrongSourceAfter != nullptr
            && WrongSourceAfter->Matches(WrongSourceSnapshot));
        TestNull("Wrong-source stale rejection does not register replacement cover", Cover->FindCoverSocket(ReplacementDefenseId));
        TestTrue("Wrong-source stale rejection leaves resolver tracking unchanged", Runtime.TracksCover(DefenseActorId));

        if (Cover->FindCoverSocket(ReplacementDefenseId) != nullptr)
        {
            Cover->UnregisterAuthoredCoverSocket(ReplacementDefenseId);
        }
        TestTrue("Wrong-source fixture cleans up", Cover->UnregisterDeployableCover(DefenseActorId));
        TestTrue("Original authored socket restores", Cover->RegisterAuthoredCoverSocket(
            DefenseActorId,
            EDACoverType::Hardened,
            DefenseActorTransform.GetLocation()));
        TestTrue("Replacement reconstruction succeeds after stale authority is restored", Runtime.ReconstructIronheart(ReplacementDefense));
        TestTrue("Successful replacement swaps resolver cover tracking", !Runtime.TracksCover(DefenseActorId)
            && Runtime.TracksCover(ReplacementDefenseId));
        TestTrue("Rolling reconstruction back to the pre-fortify campaign succeeds", Runtime.ReconstructIronheart(AfterTrade));
        TestTrue("Rollback unregisters cover absent from the new canonical campaign", Cover->FindCoverSocket(ReplacementDefenseId) == nullptr);
    });

    It("repairs a stable WorldAsset and structural-damage record atomically", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("World exists", World);
        if (World == nullptr)
        {
            return;
        }
        TestTrue("World initializes", World->InitializeVerticalSliceState(TEXT("region.synara_frontier")));
        TestTrue("Two construction ticks commit", World->AdvanceWorldTicks(2));

        FDACampaignSnapshot DamagedCampaign = World->GetPersistentCampaign();
        FDAForgeweaveBuildingState* Foundry = DamagedCampaign.WorldState.Forgeweave.Buildings.FindByPredicate(
            [&DamagedCampaign](const FDAForgeweaveBuildingState& Building)
            {
                const FDAWorldAssetRecord* Asset = DamagedCampaign.FindWorldAssetRecord(Building.WorldAssetId);
                return Asset != nullptr && Asset->CardDefinitionId == TEXT("forgeweave.infinite_foundry");
            });
        TestNotNull("The output shortage builds an Infinite Foundry", Foundry);
        if (Foundry == nullptr)
        {
            return;
        }
        const FGuid StableWorldAssetId = Foundry->WorldAssetId;
        FDAWorldAssetRecord* DamagedAsset = DamagedCampaign.FindWorldAssetRecord(StableWorldAssetId);
        FDAStructuralDamageRecord* DamagedRecord = DamagedCampaign.OperationConflict.FindStructuralDamageRecord(StableWorldAssetId);
        TestNotNull("Building resolves canonical WorldAsset", DamagedAsset);
        TestNotNull("Building resolves canonical structural record", DamagedRecord);
        if (DamagedAsset == nullptr || DamagedRecord == nullptr || DamagedRecord->Modules.IsEmpty())
        {
            return;
        }
        DamagedAsset->StructuralIntegrity = 40.f;
        DamagedAsset->ConstructionState = EDAConstructionState::Damaged;
        DamagedRecord->Modules[0].CurrentHealth = 40.f;
        DamagedRecord->Modules[0].State = EDAStructureDamageState::Damaged;
        TestTrue("Damaged authoritative records restore", World->RestorePersistentCampaign(DamagedCampaign));
        TestTrue("Repair World Tick commits", World->AdvanceWorldTicks(1));

        const FDACampaignSnapshot& RepairedCampaign = World->GetPersistentCampaign();
        const FDAForgeweaveBuildingState* Repaired = RepairedCampaign.WorldState.Forgeweave.Buildings.FindByPredicate(
            [StableWorldAssetId](const FDAForgeweaveBuildingState& Building)
            {
                return Building.WorldAssetId == StableWorldAssetId;
            });
        TestNotNull("Repair retains stable WorldAsset identity", Repaired);
        if (Repaired != nullptr)
        {
            TestEqual("Building retains only the stable canonical reference", Repaired->WorldAssetId, StableWorldAssetId);
        }
        const FDAWorldAssetRecord* RepairedAsset = RepairedCampaign.FindWorldAssetRecord(StableWorldAssetId);
        const FDAStructuralDamageRecord* RepairedDamage = RepairedCampaign.OperationConflict.FindStructuralDamageRecord(StableWorldAssetId);
        TestNotNull("Repaired canonical asset remains present", RepairedAsset);
        TestNotNull("Repaired canonical damage remains present", RepairedDamage);
        if (RepairedAsset != nullptr && RepairedDamage != nullptr)
        {
            TestEqual("Canonical WorldAsset integrity is repaired", RepairedAsset->StructuralIntegrity, 65.f, Tolerance);
            TestEqual("Canonical structural module is repaired atomically", RepairedDamage->Modules[0].CurrentHealth, 65.f, Tolerance);
        }
        TestEqual("Decision history targets repair", RepairedCampaign.WorldState.Forgeweave.DecisionHistory.Last().Type, EDARivalDecisionType::Repair);
        const FDAForgeweaveActionTransaction& RepairTransaction = RepairedCampaign.WorldState.Forgeweave.ActionTransactions.Last();
        TestEqual("Repair transaction captures integrity opening balance", RepairTransaction.IntegrityBefore, 40.f, Tolerance);
        TestEqual("Repair transaction captures integrity closing balance", RepairTransaction.IntegrityAfter, 65.f, Tolerance);
        TestEqual("Repair transaction captures each module delta", RepairTransaction.ModuleDeltas.Num(), 1);
        if (RepairTransaction.ModuleDeltas.Num() == 1)
        {
            TestEqual("Module opening health is durable", RepairTransaction.ModuleDeltas[0].HealthBefore, 40.f, Tolerance);
            TestEqual("Module closing health is durable", RepairTransaction.ModuleDeltas[0].HealthAfter, 65.f, Tolerance);
        }

        const FDARegionActorState* ReconstructionActor = RepairedCampaign.WorldState.FindRegion(TEXT("region.ironheart"))->PersistentDelta.LocalActors.FindByPredicate(
            [StableWorldAssetId](const FDARegionActorState& Actor)
            {
                return Actor.WorldAssetId == StableWorldAssetId;
            });
        TestNotNull("Region reconstruction actor is linked to the same stable WorldAsset", ReconstructionActor);

        const FString SaveDirectory = FPaths::Combine(
            FPaths::ProjectIntermediateDir(),
            TEXT("ForgeweaveRepairAuthorityTests"),
            FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService SaveService(SaveDirectory);
        FDACampaignSnapshot Campaign;
        Campaign = RepairedCampaign;
        TestTrue("Repaired structural authority saves", SaveService.SaveCampaign(Campaign, TEXT("repair")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(TEXT("repair"));
        TestTrue("Repaired structural authority loads", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            TestEqual("Repair action transaction survives save/load", Loaded.GetValue().WorldState.Forgeweave.ActionTransactions.Last().Type, EDARivalDecisionType::Repair);
            const FDAForgeweaveBuildingState* LoadedBuilding = Loaded.GetValue().WorldState.Forgeweave.Buildings.FindByPredicate(
                [StableWorldAssetId](const FDAForgeweaveBuildingState& Building)
                {
                    return Building.WorldAssetId == StableWorldAssetId;
                });
            TestNotNull("Stable repair identity survives save/load", LoadedBuilding);
            if (LoadedBuilding != nullptr)
            {
                const FDAWorldAssetRecord* LoadedAsset = Loaded.GetValue().FindWorldAssetRecord(StableWorldAssetId);
                const FDAStructuralDamageRecord* LoadedDamage = Loaded.GetValue().OperationConflict.FindStructuralDamageRecord(StableWorldAssetId);
                TestTrue("Loaded building resolves canonical repaired records", LoadedAsset != nullptr && LoadedDamage != nullptr);
                if (LoadedAsset != nullptr && LoadedDamage != nullptr)
                {
                    TestEqual("Loaded WorldAsset retains repaired integrity", LoadedAsset->StructuralIntegrity, 65.f, Tolerance);
                    TestEqual("Loaded damage module retains repaired health", LoadedDamage->Modules[0].CurrentHealth, 65.f, Tolerance);
                }
            }
        }
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*SaveDirectory);

        UWorld* ReconstructionWorld = NewObject<UWorld>(GetTransientPackage());
        UDACoverSubsystem* Cover = NewObject<UDACoverSubsystem>(ReconstructionWorld);
        FDAForgeweaveReconstructionRuntime Runtime(*Cover);
        TestEqual("Travel streams repaired Ironheart authority", World->Travel(
            TEXT("travel.forgeweave.repair"), TEXT("region.ironheart"), 1, Runtime), EDATravelResult::Completed);
        TestTrue("Streamed reconstruction resolves the repaired stable WorldAsset", Runtime.ReconstructedWorldAsset(StableWorldAssetId));
        TestEqual("Streamed runtime applies repaired structural integrity", Runtime.GetReconstructedIntegrity(StableWorldAssetId), 65.f, Tolerance);
        TestEqual("Streamed runtime applies repaired module health", Runtime.GetReconstructedModuleHealth(StableWorldAssetId), 65.f, Tolerance);
    });

    It("validates two repairs separated by authoritative combat damage and survives save load", [this]()
    {
        UDAForgeweaveStrategy* Strategy = NewObject<UDAForgeweaveStrategy>();
        FDACampaignSnapshot Campaign = MakeCampaignState();
        for (int64 Tick = 1; Tick <= 2; ++Tick)
        {
            PrepareCampaignTradeTick(Campaign, Tick);
            TestTrue("Construction commits", Strategy->ProcessWorldTick(Campaign, Tick).bCommitted);
        }
        FDAForgeweaveBuildingState* Building = Campaign.WorldState.Forgeweave.Buildings.FindByPredicate(
            [&Campaign](const FDAForgeweaveBuildingState& Candidate)
            {
                return Campaign.OperationConflict.FindStructuralDamageRecord(Candidate.WorldAssetId) != nullptr;
            });
        TestNotNull("A modular repair target exists", Building);
        if (Building == nullptr)
        {
            return;
        }
        FDAWorldAssetRecord* Asset = Campaign.FindWorldAssetRecord(Building->WorldAssetId);
        FDAStructuralDamageRecord* Damage = Campaign.OperationConflict.FindStructuralDamageRecord(Building->WorldAssetId);
        if (Asset == nullptr || Damage == nullptr || Damage->Modules.IsEmpty())
        {
            return;
        }
        const auto ApplyCombatDamage = [Asset, Damage](const float Health)
        {
            Asset->StructuralIntegrity = Health;
            Asset->ConstructionState = Health <= 50.f ? EDAConstructionState::Damaged : EDAConstructionState::Operational;
            Damage->Modules[0].CurrentHealth = Health;
            Damage->Modules[0].State = Health <= 50.f ? EDAStructureDamageState::Damaged : EDAStructureDamageState::Operational;
        };

        ApplyCombatDamage(40.f);
        PrepareCampaignTradeTick(Campaign, 3);
        TestTrue("First repair commits", Strategy->ProcessWorldTick(Campaign, 3).bCommitted);
        ApplyCombatDamage(30.f);
        PrepareCampaignTradeTick(Campaign, 4);
        TestTrue("Second repair commits after intervening damage", Strategy->ProcessWorldTick(Campaign, 4).bCommitted);
        int32 RepairTransactionCount = 0;
        for (const FDAForgeweaveActionTransaction& Entry : Campaign.WorldState.Forgeweave.ActionTransactions)
        {
            RepairTransactionCount += Entry.Type == EDARivalDecisionType::Repair ? 1 : 0;
        }
        TestEqual("Both repairs are durable", RepairTransactionCount, 2);
        FString ValidationError;
        TestTrue("Intervening combat damage does not invalidate repair history", Campaign.Validate(ValidationError));

        const FString SaveDirectory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("ForgeweaveDamageRepairTests"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService SaveService(SaveDirectory);
        TestTrue("Damage-repair sequence saves", SaveService.SaveCampaign(Campaign, TEXT("sequence")).IsSuccess());
        TestTrue("Damage-repair sequence loads", SaveService.LoadCampaign(TEXT("sequence")).HasValue());
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*SaveDirectory);
    });
}
