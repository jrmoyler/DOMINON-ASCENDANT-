#include "AI/DAForgeweaveStrategy.h"
#include "AI/DARivalCityPlanner.h"
#include "Misc/AutomationTest.h"
#include "Regions/DARegionState.h"
#include "Save/DACampaignSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDAForgeweaveSoakSpec,
    "Dominion.World.ForgeweaveAI.OneHundredWorldTickSoak",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDAForgeweaveSoakSpec::RunTest(const FString&)
{
    UDAForgeweaveStrategy* Strategy = NewObject<UDAForgeweaveStrategy>();
    FDACampaignSnapshot Campaign;
    Campaign.WorldState.bInitialized = true;
    Campaign.WorldState.CurrentRegionId = TEXT("region.synara_frontier");
    Campaign.WorldState.Regions = FDARegionSeedCatalog::MakeVerticalSliceRegions();
    FDAForgeweaveCityState& State = Campaign.WorldState.Forgeweave;
    State = FDAForgeweaveCityState::MakeVerticalSliceInitialState(1701, 0);
    State.Capital = 60.f;
    State.Population = 30;
    State.HousingCapacity = 8;
    State.DesiredIndustrialOutput = 30.f;
    State.ActiveIndustrialThroughput = 4.f;
    State.MaterialScarcity = 25.f;
    State.UtilitySupply = 48.f;

    FDATradeWorldState& Trade = Campaign.WorldState.Trade;
    FDARegionalTradeInventory Eden;
    Eden.RegionId = TEXT("region.eden_basin");
    Eden.Stock.Add(TEXT("resource.regenerative_materials"), 1000);
    Trade.Inventories.Add(Eden);
    FDARegionalTradeInventory IronheartInventory;
    IronheartInventory.RegionId = TEXT("region.ironheart");
    IronheartInventory.Stock.Add(TEXT("resource.regenerative_materials"), 0);
    Trade.Inventories.Add(IronheartInventory);
    FDATradeRouteState Route;
    Route.RouteId = TEXT("route.eden_ironheart_relief");
    Route.SourceRegionId = Eden.RegionId;
    Route.DestinationRegionId = IronheartInventory.RegionId;
    Route.CapacityPerWorldTick = 10;
    Trade.Routes.Add(Route);

    int32 ConstructionCount = 0;
    int32 RepairCount = 0;
    int32 TradeCount = 0;
    int32 DefenseCount = 0;
    int32 MaxUnexplainedIdleTicks = 0;

    for (int64 WorldTick = 1; WorldTick <= 100; ++WorldTick)
    {
        if (WorldTick % 19 == 0)
        {
            FDAForgeweaveBuildingState* Repairable = State.Buildings.FindByPredicate(
                [&Campaign](const FDAForgeweaveBuildingState& Building)
                {
                    return Campaign.OperationConflict.FindStructuralDamageRecord(Building.WorldAssetId) != nullptr;
                });
            if (Repairable != nullptr)
            {
                FDAWorldAssetRecord* Asset = Campaign.FindWorldAssetRecord(Repairable->WorldAssetId);
                FDAStructuralDamageRecord* Damage = Campaign.OperationConflict.FindStructuralDamageRecord(Repairable->WorldAssetId);
                if (Asset != nullptr && Damage != nullptr)
                {
                    Asset->StructuralIntegrity = 45.f;
                    Asset->ConstructionState = EDAConstructionState::Damaged;
                    Damage->Modules[0].CurrentHealth = 45.f;
                    Damage->Modules[0].State = EDAStructureDamageState::Damaged;
                }
            }
        }
        if (WorldTick % 17 == 0)
        {
            State.MaterialScarcity = 82.f;
        }
        if (WorldTick % 23 == 0)
        {
            State.DefensePressure = 80.f;
        }

        Trade.LastProcessedWorldTick = WorldTick;
        Trade.Routes[0].CapacityWorldTick = WorldTick;
        Trade.Routes[0].ReservedCapacityThisTick = 0;
        const FDAForgeweaveTickResult Result = Strategy->ProcessWorldTick(Campaign, WorldTick);
        TestTrue(*FString::Printf(TEXT("World Tick %lld commits atomically"), WorldTick), Result.bCommitted);
        if (!Result.bCommitted)
        {
            return false;
        }

        ConstructionCount += Result.Decision.Type == EDARivalDecisionType::Construct ? 1 : 0;
        RepairCount += Result.Decision.Type == EDARivalDecisionType::Repair ? 1 : 0;
        TradeCount += Result.Decision.Type == EDARivalDecisionType::Trade ? 1 : 0;
        DefenseCount += Result.Decision.Type == EDARivalDecisionType::Fortify ? 1 : 0;
        MaxUnexplainedIdleTicks = FMath::Max(MaxUnexplainedIdleTicks, State.ConsecutiveUnexplainedIdleWorldTicks);

        FString ValidationError;
        TestTrue(*FString::Printf(TEXT("World Tick %lld retains a valid durable city"), WorldTick), State.Validate(ValidationError));
        TestTrue(*FString::Printf(TEXT("World Tick %lld retains valid trade authority"), WorldTick), Trade.Validate(ValidationError));
        TestTrue(*FString::Printf(TEXT("World Tick %lld retains valid shared campaign authority"), WorldTick), Campaign.Validate(ValidationError));
        TestFalse(*FString::Printf(TEXT("World Tick %lld has no invalid placement"), WorldTick), Result.bInvalidPlacement);
        TestFalse(*FString::Printf(TEXT("World Tick %lld has no impossible utility state"), WorldTick), Result.bImpossibleUtilityState);
        TestFalse(*FString::Printf(TEXT("World Tick %lld has no negative impossible economy"), WorldTick), Result.bNegativeImpossibleEconomy);
        TestTrue(*FString::Printf(TEXT("World Tick %lld keeps Hunger bounded"), WorldTick), State.ResourceHunger >= 0.f && State.ResourceHunger <= 100.f);
        TestTrue(*FString::Printf(TEXT("World Tick %lld construction stays in pool"), WorldTick),
            Result.Decision.Type != EDARivalDecisionType::Construct
                || FDARivalCityPlanner::IsVerticalSliceBuildCard(Result.Decision.CardDefinitionId));
        TestTrue(*FString::Printf(TEXT("World Tick %lld has no unexplained deadlock beyond the grace bound"), WorldTick),
            Result.Decision.Type != EDARivalDecisionType::None
                || !Result.Decision.CrisisExplanation.IsNone()
                || State.ConsecutiveUnexplainedIdleWorldTicks <= 10);
    }

    TestTrue("The soak exercises construction", ConstructionCount > 0);
    TestTrue("The soak exercises repair", RepairCount > 0);
    TestTrue("The soak exercises trade", TradeCount > 0);
    TestTrue("The soak exercises defense", DefenseCount > 0);
    TestTrue("No unexplained planner deadlock exceeds ten World Ticks", MaxUnexplainedIdleTicks <= 10);
    TestEqual("The durable state reaches exactly World Tick 100", State.LastProcessedWorldTick, 100LL);
    return true;
}
