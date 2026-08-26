#include "AI/DAForgeweaveStrategy.h"
#include "Misc/Crc.h"
#include "Trade/DATradeSystem.h"

namespace
{
    constexpr float TradeCapitalCost = 5.f;
    constexpr float FortifyCapitalCost = 4.f;
    const FName IronheartRegionId(TEXT("region.ironheart"));
    const FName RegenerativeMaterialsId(TEXT("resource.regenerative_materials"));
    const FName EdenReliefRouteId(TEXT("route.eden_ironheart_relief"));
    const FName DefenseCoverDefinitionId(TEXT("forgeweave.defense_cover"));

    float NonNegativeFinite(const float Value)
    {
        return FMath::IsFinite(Value) ? FMath::Max(0.f, Value) : 0.f;
    }

    FDAResourceHungerInputs MakeHungerInputs(const FDAForgeweaveCityState& State)
    {
        FDAResourceHungerInputs Inputs;
        Inputs.ActiveIndustrialThroughput = State.ActiveIndustrialThroughput;
        Inputs.MaterialScarcity = State.MaterialScarcity;
        Inputs.bOverdrive = State.bOverdrive;
        Inputs.LogisticsEfficiency = State.LogisticsEfficiency;
        Inputs.Recycling = State.Recycling;
        Inputs.EdenRegenerativeInputs = State.EdenRegenerativeInputs;
        Inputs.ProductionReduction = State.ProductionReduction;
        return Inputs;
    }

    bool BuildCanonicalGrid(
        const FDACampaignSnapshot& Campaign,
        const FDAForgeweaveCityState& State,
        FDACityGridSubsystem& OutGrid)
    {
        OutGrid.SetAllCellsClaimed(true);
        for (int32 Y = 0; Y < State.GridHeight; ++Y)
        {
            for (int32 X = 0; X < State.GridWidth; ++X)
            {
                OutGrid.SetCellLayerMask(FIntPoint(X, Y), 1);
            }
        }

        for (int32 Index = 0; Index < State.Buildings.Num(); ++Index)
        {
            const FDAForgeweaveBuildingState& Building = State.Buildings[Index];
            const FDAWorldAssetRecord* Asset = Campaign.FindWorldAssetRecord(Building.WorldAssetId);
            if (Asset == nullptr
                || !OutGrid.ReserveFootprint(
                    FDAWorldAssetId(Asset->WorldAssetId),
                    Asset->GridOrigin,
                    Building.Footprint,
                    EGridRotation::Zero))
            {
                return false;
            }
        }
        return true;
    }

    FDAForgeweaveDecisionRecord MakeRecord(const FDARivalPlannerDecision& Decision, const int64 WorldTick)
    {
        FDAForgeweaveDecisionRecord Record;
        Record.WorldTick = WorldTick;
        Record.Type = Decision.Type;
        Record.CardDefinitionId = Decision.CardDefinitionId;
        Record.TargetBuildingId = Decision.TargetBuildingId;
        Record.Origin = Decision.Origin;
        Record.CapitalSpent = Decision.CapitalCost;
        Record.ProductionSpent = Decision.ProductionThroughputCost;
        Record.CrisisExplanation = Decision.CrisisExplanation;
        return Record;
    }

    EDAStructureDamageState GetModuleState(const float CurrentHealth, const float MaximumHealth)
    {
        if (CurrentHealth > MaximumHealth * 0.5f)
        {
            return EDAStructureDamageState::Operational;
        }
        if (CurrentHealth > MaximumHealth * 0.25f)
        {
            return EDAStructureDamageState::Damaged;
        }
        return EDAStructureDamageState::Disabled;
    }

    EDAConstructionState GetConstructionState(const float Integrity)
    {
        if (Integrity <= 0.f)
        {
            return EDAConstructionState::Ruined;
        }
        if (Integrity <= 25.f)
        {
            return EDAConstructionState::Disabled;
        }
        if (Integrity <= 50.f)
        {
            return EDAConstructionState::Damaged;
        }
        return EDAConstructionState::Operational;
    }

    float GetDefenseStrength(const FDARegionState* Ironheart)
    {
        if (Ironheart == nullptr)
        {
            return 0.f;
        }
        int32 CoverCount = 0;
        for (const FDARegionActorState& Actor : Ironheart->PersistentDelta.LocalActors)
        {
            CoverCount += Actor.DefinitionId == DefenseCoverDefinitionId ? 1 : 0;
        }
        return static_cast<float>(CoverCount) * 10.f;
    }
}

FDAForgeweaveCityState UDAForgeweaveStrategy::MakeVerticalSliceInitialState(
    const int32 CampaignSeed,
    const int64 InitialWorldTick)
{
    return FDAForgeweaveCityState::MakeVerticalSliceInitialState(CampaignSeed, InitialWorldTick);
}

FDAResourceHungerTick UDAForgeweaveStrategy::CalculateResourceHungerTick(
    const float CurrentResourceHunger,
    const FDAResourceHungerInputs& Inputs)
{
    FDAResourceHungerTick Tick;
    Tick.ThroughputGrowth = NonNegativeFinite(Inputs.ActiveIndustrialThroughput) * 0.06f;
    Tick.ScarcityGrowth = NonNegativeFinite(Inputs.MaterialScarcity) * 0.04f;
    Tick.OverdriveGrowth = Inputs.bOverdrive ? 4.f : 0.f;
    Tick.LogisticsMitigation = NonNegativeFinite(Inputs.LogisticsEfficiency) * 0.025f;
    Tick.RecyclingMitigation = NonNegativeFinite(Inputs.Recycling) * 0.04f;
    Tick.EdenInputMitigation = NonNegativeFinite(Inputs.EdenRegenerativeInputs) * 0.05f;
    Tick.ProductionReductionMitigation = NonNegativeFinite(Inputs.ProductionReduction) * 0.04f;

    const float Growth = Tick.ThroughputGrowth + Tick.ScarcityGrowth + Tick.OverdriveGrowth;
    const float Mitigation = Tick.LogisticsMitigation
        + Tick.RecyclingMitigation
        + Tick.EdenInputMitigation
        + Tick.ProductionReductionMitigation;
    Tick.ResultingResourceHunger = FMath::Clamp(
        NonNegativeFinite(CurrentResourceHunger) + Growth - Mitigation,
        0.f,
        100.f);
    return Tick;
}

FDAForgeweaveTickResult UDAForgeweaveStrategy::ProcessWorldTick(
    FDACampaignSnapshot& Campaign,
    const int64 WorldTick) const
{
    FDAForgeweaveTickResult Result;
    FString ValidationError;
    FDAForgeweaveCityState& State = Campaign.WorldState.Forgeweave;
    if (!State.bInitialized
        || State.LastProcessedWorldTick == MAX_int64
        || WorldTick != State.LastProcessedWorldTick + 1
        || Campaign.WorldState.Trade.LastProcessedWorldTick != WorldTick
        || !State.Validate(ValidationError)
        || !Campaign.WorldState.Trade.Validate(ValidationError)
        || !Campaign.OperationConflict.Validate(Campaign.WorldAssets, ValidationError))
    {
        Result.FailureReason = ValidationError.IsEmpty()
            ? TEXT("Forgeweave ticks must be contiguous and begin from valid initialized state.")
            : ValidationError;
        return Result;
    }

    FDACampaignSnapshot CandidateCampaign = Campaign;
    FDAForgeweaveCityState& Candidate = CandidateCampaign.WorldState.Forgeweave;
    FDATradeWorldState& TradeAuthority = CandidateCampaign.WorldState.Trade;
    FDARegionState* IronheartAuthority = CandidateCampaign.WorldState.FindRegion(IronheartRegionId);
    if (IronheartAuthority == nullptr)
    {
        Result.FailureReason = TEXT("Forgeweave requires canonical Ironheart region authority.");
        return Result;
    }

    const float EarnedCapital = 4.f + Candidate.ActiveIndustrialThroughput * 0.35f;
    const float EarnedProduction = 1.f + Candidate.ActiveIndustrialThroughput * 0.5f;
    if (!FMath::IsFinite(EarnedCapital)
        || !FMath::IsFinite(EarnedProduction)
        || Candidate.Capital > TNumericLimits<float>::Max() - EarnedCapital
        || Candidate.ProductionReserve > TNumericLimits<float>::Max() - EarnedProduction)
    {
        Result.bNegativeImpossibleEconomy = true;
        Result.FailureReason = TEXT("Forgeweave economy accumulation overflowed.");
        return Result;
    }
    Candidate.Capital += EarnedCapital;
    Candidate.ProductionReserve += EarnedProduction;
    Candidate.MaterialScarcity = FMath::Clamp(
        Candidate.MaterialScarcity + Candidate.ActiveIndustrialThroughput * 0.08f,
        0.f,
        100.f);
    Result.ResourceHunger = CalculateResourceHungerTick(Candidate.ResourceHunger, MakeHungerInputs(Candidate));
    Candidate.ResourceHunger = Result.ResourceHunger.ResultingResourceHunger;

    FDACityGridSubsystem Grid(Candidate.GridWidth, Candidate.GridHeight);
    if (!BuildCanonicalGrid(CandidateCampaign, Candidate, Grid))
    {
        Result.bInvalidPlacement = true;
        Result.FailureReason = TEXT("A durable Forgeweave footprint cannot be reconstructed on the canonical grid.");
        return Result;
    }

    FDARivalPlannerDecision Decision;
    TOptional<FDAForgeweaveActionTransaction> ActionTransaction;

    FDAForgeweaveBuildingState* DamagedBuilding = Candidate.Buildings.FindByPredicate(
        [&CandidateCampaign](const FDAForgeweaveBuildingState& Building)
        {
            const FDAWorldAssetRecord* Asset = CandidateCampaign.FindWorldAssetRecord(Building.WorldAssetId);
            return Asset != nullptr
                && Asset->StructuralIntegrity < 99.999f
                && CandidateCampaign.OperationConflict.FindStructuralDamageRecord(Building.WorldAssetId) != nullptr;
        });
    if (DamagedBuilding != nullptr)
    {
        FDAWorldAssetRecord* DamagedAsset = CandidateCampaign.FindWorldAssetRecord(DamagedBuilding->WorldAssetId);
        FDAStructuralDamageRecord* DamageRecord = CandidateCampaign.OperationConflict.FindStructuralDamageRecord(DamagedBuilding->WorldAssetId);
        if (DamagedAsset == nullptr || DamageRecord == nullptr)
        {
            Result.FailureReason = TEXT("Forgeweave repair authority could not resolve its stable campaign records.");
            return Result;
        }
        const float RepairPercent = FMath::Min(25.f, 100.f - DamagedAsset->StructuralIntegrity);
        const float RepairCost = DamagedBuilding->DeploymentCapital * (RepairPercent / 100.f) * 0.5f;
        if (Candidate.Capital >= RepairCost)
        {
            ActionTransaction.Emplace();
            ActionTransaction->WorldTick = WorldTick;
            ActionTransaction->Type = EDARivalDecisionType::Repair;
            ActionTransaction->AuthorityId = DamagedBuilding->BuildingId;
            ActionTransaction->CapitalBefore = Candidate.Capital;
            ActionTransaction->ProductionBefore = Candidate.ProductionReserve;
            ActionTransaction->IntegrityBefore = DamagedAsset->StructuralIntegrity;
            Decision.Type = EDARivalDecisionType::Repair;
            Decision.TargetBuildingId = DamagedBuilding->BuildingId;
            Decision.CapitalCost = RepairCost;
            Candidate.Capital -= RepairCost;
            DamagedAsset->StructuralIntegrity += RepairPercent;
            DamagedAsset->ConstructionState = GetConstructionState(DamagedAsset->StructuralIntegrity);
            for (FDAStructureModuleHealthRecord& Module : DamageRecord->Modules)
            {
                FDAForgeweaveModuleRepairDelta Delta;
                Delta.ModuleId = Module.ModuleId;
                Delta.HealthBefore = Module.CurrentHealth;
                Module.CurrentHealth = FMath::Min(Module.MaximumHealth, Module.CurrentHealth + RepairPercent);
                Module.State = GetModuleState(Module.CurrentHealth, Module.MaximumHealth);
                Delta.HealthAfter = Module.CurrentHealth;
                ActionTransaction->ModuleDeltas.Add(Delta);
            }
            DamageRecord->bProductionDisabled = DamageRecord->Modules.ContainsByPredicate(
                [](const FDAStructureModuleHealthRecord& Module)
                {
                    return Module.bDisablesProduction && Module.CurrentHealth <= 0.f;
                });
            ActionTransaction->CapitalAfter = Candidate.Capital;
            ActionTransaction->ProductionAfter = Candidate.ProductionReserve;
            ActionTransaction->IntegrityAfter = DamagedAsset->StructuralIntegrity;
        }
    }

    if (Decision.Type == EDARivalDecisionType::None
        && Candidate.MaterialScarcity >= 70.f
        && Candidate.Capital >= TradeCapitalCost)
    {
        const FDATradeRouteState* RouteBefore = TradeAuthority.FindRoute(EdenReliefRouteId);
        const FDARegionalTradeInventory* SourceBefore = RouteBefore != nullptr
            ? TradeAuthority.FindInventory(RouteBefore->SourceRegionId)
            : nullptr;
        const FDARegionalTradeInventory* DestinationBefore = RouteBefore != nullptr
            ? TradeAuthority.FindInventory(RouteBefore->DestinationRegionId)
            : nullptr;
        const int64 SourceOpening = SourceBefore != nullptr ? SourceBefore->Stock.FindRef(RegenerativeMaterialsId) : 0;
        const int64 DestinationOpening = DestinationBefore != nullptr ? DestinationBefore->Stock.FindRef(RegenerativeMaterialsId) : 0;
        const FName OrderId(*FString::Printf(TEXT("order.forgeweave.tick.%lld"), WorldTick));
        if (UDATradeSystem::ExecuteSpotTrade(
            TradeAuthority,
            OrderId,
            EdenReliefRouteId,
            RegenerativeMaterialsId,
            10,
            WorldTick))
        {
            ActionTransaction.Emplace();
            ActionTransaction->WorldTick = WorldTick;
            ActionTransaction->Type = EDARivalDecisionType::Trade;
            ActionTransaction->AuthorityId = OrderId;
            ActionTransaction->CapitalBefore = Candidate.Capital;
            ActionTransaction->ProductionBefore = Candidate.ProductionReserve;
            ActionTransaction->SourceQuantityBefore = SourceOpening;
            ActionTransaction->DestinationQuantityBefore = DestinationOpening;
            Decision.Type = EDARivalDecisionType::Trade;
            Decision.TargetBuildingId = OrderId;
            Decision.CapitalCost = TradeCapitalCost;
            Candidate.Capital -= TradeCapitalCost;
            Candidate.MaterialScarcity = FMath::Max(0.f, Candidate.MaterialScarcity - 15.f);
            Candidate.EdenRegenerativeInputs += 2.f;
            const FDATradeRouteState* RouteAfter = TradeAuthority.FindRoute(EdenReliefRouteId);
            const FDARegionalTradeInventory* SourceAfter = RouteAfter != nullptr
                ? TradeAuthority.FindInventory(RouteAfter->SourceRegionId)
                : nullptr;
            const FDARegionalTradeInventory* DestinationAfter = RouteAfter != nullptr
                ? TradeAuthority.FindInventory(RouteAfter->DestinationRegionId)
                : nullptr;
            ActionTransaction->SourceQuantityAfter = SourceAfter != nullptr ? SourceAfter->Stock.FindRef(RegenerativeMaterialsId) : 0;
            ActionTransaction->DestinationQuantityAfter = DestinationAfter != nullptr ? DestinationAfter->Stock.FindRef(RegenerativeMaterialsId) : 0;
            ActionTransaction->CapitalAfter = Candidate.Capital;
            ActionTransaction->ProductionAfter = Candidate.ProductionReserve;
        }
    }

    if (Decision.Type == EDARivalDecisionType::None
        && Candidate.DefensePressure - GetDefenseStrength(IronheartAuthority) >= 60.f
        && Candidate.Capital >= FortifyCapitalCost
        && Candidate.ProductionReserve >= 2.f)
    {
        if (IronheartAuthority->PersistentDelta.Revision == MAX_int64)
        {
            Result.FailureReason = TEXT("Ironheart reconstruction authority cannot advance beyond maximum revision.");
            return Result;
        }
        Decision.Type = EDARivalDecisionType::Fortify;
        Decision.CapitalCost = FortifyCapitalCost;
        Decision.ProductionThroughputCost = 2.f;
        Decision.TargetBuildingId = FName(*FString::Printf(TEXT("forgeweave.defense.tick_%lld"), WorldTick));
        ActionTransaction.Emplace();
        ActionTransaction->WorldTick = WorldTick;
        ActionTransaction->Type = EDARivalDecisionType::Fortify;
        ActionTransaction->AuthorityId = Decision.TargetBuildingId;
        ActionTransaction->CapitalBefore = Candidate.Capital;
        ActionTransaction->ProductionBefore = Candidate.ProductionReserve;
        Candidate.Capital -= FortifyCapitalCost;
        Candidate.ProductionReserve -= Decision.ProductionThroughputCost;
        FDARegionActorState DefenseActor;
        DefenseActor.ActorId = Decision.TargetBuildingId;
        DefenseActor.DefinitionId = DefenseCoverDefinitionId;
        DefenseActor.Transform = FTransform(FRotator::ZeroRotator, FDAForgeweaveCityState::GridCellToWorld(FIntPoint(16, 16)));
        IronheartAuthority->PersistentDelta.LocalActors.Add(DefenseActor);
        ++IronheartAuthority->PersistentDelta.Revision;
        ActionTransaction->CapitalAfter = Candidate.Capital;
        ActionTransaction->ProductionAfter = Candidate.ProductionReserve;
        ActionTransaction->ActorTransform = DefenseActor.Transform;
        ActionTransaction->CoverTypeId = TEXT("cover.hardened");
    }

    if (Decision.Type == EDARivalDecisionType::None)
    {
        FDARivalCityPlanner Planner;
        Decision = Planner.ChooseConstruction(Candidate, Grid, WorldTick, GetDefenseStrength(IronheartAuthority));
        if (Decision.Type == EDARivalDecisionType::Construct)
        {
            FDACardPlacementRequest Request;
            Request.AssetId = FDAWorldAssetId(FGuid(
                static_cast<uint32>(Candidate.CampaignSeed),
                static_cast<uint32>(WorldTick),
                Decision.DeterministicTieKey,
                1));
            Request.Origin = Decision.Origin;
            Request.Footprint = Decision.Footprint;
            Request.RequiredUtilityLayerMask = 1;
            const FDAPlacementResult Placement = Grid.ValidatePlacement(Request);
            if (!Placement.bCanPlace
                || Candidate.UtilityDemand + Decision.UtilityDemand > Candidate.UtilitySupply + KINDA_SMALL_NUMBER
                || !Grid.ReserveFootprint(Request.AssetId, Request.Origin, Request.Footprint, EGridRotation::Zero))
            {
                Result.bInvalidPlacement = !Placement.bCanPlace;
                Result.bImpossibleUtilityState = Candidate.UtilityDemand + Decision.UtilityDemand > Candidate.UtilitySupply + KINDA_SMALL_NUMBER;
                Result.FailureReason = TEXT("The selected Forgeweave construction failed canonical placement or utility validation.");
                return Result;
            }

            FDAForgeweaveBuildingState Building;
            Building.BuildingId = FName(*FString::Printf(
                TEXT("forgeweave.asset.tick_%lld.%s"),
                WorldTick,
                *Decision.CardDefinitionId.ToString()));
            Building.WorldAssetId = Request.AssetId.Value;
            const FString ProvenanceSeed = TEXT("DA.Forgeweave.PlannerProvenance|")
                + Building.WorldAssetId.ToString(EGuidFormats::Digits);
            Building.ProvenanceId = FGuid(
                FCrc::StrCrc32(*(ProvenanceSeed + TEXT("|A"))),
                FCrc::StrCrc32(*(ProvenanceSeed + TEXT("|B"))),
                FCrc::StrCrc32(*(ProvenanceSeed + TEXT("|C"))),
                FCrc::StrCrc32(*(ProvenanceSeed + TEXT("|D"))));
            Building.CardDefinitionId = Decision.CardDefinitionId;
            Building.Footprint = Decision.Footprint;
            Building.DeploymentCapital = Decision.CapitalCost;
            Building.UtilityDemand = Decision.UtilityDemand;

            FDAWorldAssetRecord AssetRecord;
            AssetRecord.WorldAssetId = Building.WorldAssetId;
            AssetRecord.CardInstanceId.Invalidate();
            AssetRecord.CardDefinitionId = Decision.CardDefinitionId;
            AssetRecord.CityId = TEXT("settlement.ore_station_7");
            AssetRecord.GridOrigin = Decision.Origin;
            AssetRecord.ConstructionState = EDAConstructionState::Operational;
            AssetRecord.StructuralIntegrity = 100.f;
            AssetRecord.OwnerCivilizationId = TEXT("civilization.forgeweave");
            CandidateCampaign.WorldAssets.Add(AssetRecord);
            if (FDAStructuralDamagePolicy::SupportsFullModularDestruction(Decision.CardDefinitionId))
            {
                FDAStructuralDamageRecord DamageRecord;
                DamageRecord.WorldAssetId = Building.WorldAssetId;
                DamageRecord.CardDefinitionId = Decision.CardDefinitionId;
                DamageRecord.Modules.Add(FDAStructureModuleHealthRecord(TEXT("industrial_core"), 100.f, true));
                CandidateCampaign.OperationConflict.StructuralDamageRecords.Add(DamageRecord);
            }
            Decision.TargetBuildingId = Building.BuildingId;
            if (IronheartAuthority->PersistentDelta.Revision == MAX_int64
                || IronheartAuthority->PersistentDelta.LocalActors.ContainsByPredicate(
                    [&Building](const FDARegionActorState& Actor) { return Actor.ActorId == Building.BuildingId; }))
            {
                Result.FailureReason = TEXT("Ironheart reconstruction authority rejected the stable construction actor.");
                return Result;
            }
            FDARegionActorState Actor;
            Actor.ActorId = Building.BuildingId;
            Actor.DefinitionId = AssetRecord.CardDefinitionId;
            Actor.WorldAssetId = AssetRecord.WorldAssetId;
            Actor.Transform = FTransform(FRotator::ZeroRotator, FDAForgeweaveCityState::GridCellToWorld(AssetRecord.GridOrigin));
            IronheartAuthority->PersistentDelta.LocalActors.Add(Actor);
            ++IronheartAuthority->PersistentDelta.Revision;
            Candidate.Buildings.Add(Building);
            Candidate.Capital -= Decision.CapitalCost;
            Candidate.ProductionReserve -= Decision.ProductionThroughputCost;
            Candidate.UtilityDemand += Decision.UtilityDemand;
            Candidate.HousingCapacity += FMath::RoundToInt(Decision.HousingCapacity);
            Candidate.ActiveIndustrialThroughput += Decision.IndustrialOutput;
            if (Decision.CardDefinitionId == TEXT("forgeweave.freight_furnace"))
            {
                Candidate.LogisticsEfficiency += Decision.ResourceHungerMitigation;
            }
            else if (Decision.CardDefinitionId == TEXT("forgeweave.smog_reclaimer"))
            {
                Candidate.Recycling += Decision.ResourceHungerMitigation;
            }
        }
    }

    if (Decision.Type == EDARivalDecisionType::None)
    {
        if (Decision.CrisisExplanation.IsNone())
        {
            ++Candidate.ConsecutiveUnexplainedIdleWorldTicks;
        }
        else
        {
            Candidate.ConsecutiveUnexplainedIdleWorldTicks = 0;
        }
    }
    else
    {
        Candidate.ConsecutiveUnexplainedIdleWorldTicks = 0;
    }

    Candidate.Capital = FMath::Max(0.f, Candidate.Capital);
    Candidate.ProductionReserve = FMath::Max(0.f, Candidate.ProductionReserve);
    Candidate.LastProcessedWorldTick = WorldTick;
    Candidate.DecisionHistory.Add(MakeRecord(Decision, WorldTick));
    if (ActionTransaction.IsSet())
    {
        Candidate.ActionTransactions.Add(ActionTransaction.GetValue());
    }
    CandidateCampaign.WorldState.CurrentWorldTick = WorldTick;

    Result.bNegativeImpossibleEconomy = !FMath::IsFinite(Candidate.Capital)
        || Candidate.Capital < 0.f
        || !FMath::IsFinite(Candidate.ProductionReserve)
        || Candidate.ProductionReserve < 0.f;
    Result.bImpossibleUtilityState = !FMath::IsFinite(Candidate.UtilityDemand)
        || Candidate.UtilityDemand < 0.f
        || Candidate.UtilityDemand > Candidate.UtilitySupply + KINDA_SMALL_NUMBER;
    if (Result.bNegativeImpossibleEconomy
        || Result.bImpossibleUtilityState
        || Candidate.ConsecutiveUnexplainedIdleWorldTicks > 10
        || !CandidateCampaign.Validate(ValidationError))
    {
        Result.FailureReason = ValidationError.IsEmpty()
            ? TEXT("Forgeweave post-tick validation rejected an impossible state or unexplained deadlock.")
            : ValidationError;
        return Result;
    }

    Campaign = MoveTemp(CandidateCampaign);
    Result.Decision = MoveTemp(Decision);
    Result.bCommitted = true;
    return Result;
}
