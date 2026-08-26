#include "AI/DARivalCityPlanner.h"

namespace
{
    const TArray<FDARivalBuildCandidate>& GetCandidateCatalog()
    {
        static const TArray<FDARivalBuildCandidate> Catalog = {
            { TEXT("forgeweave.worker_arcology"), FIntPoint(2, 2), 14.f, 4.f, 20.f, 0.f, 0.f, 0.f, 2.f },
            { TEXT("forgeweave.production_directorate"), FIntPoint(2, 2), 18.f, 6.f, 0.f, 8.f, 0.f, 2.f, 3.f },
            { TEXT("forgeweave.industrial_exchange"), FIntPoint(2, 2), 16.f, 5.f, 0.f, 4.f, 0.f, 0.f, 2.f },
            { TEXT("forgeweave.infinite_foundry"), FIntPoint(3, 3), 28.f, 10.f, 0.f, 15.f, 0.f, 1.f, 6.f },
            { TEXT("forgeweave.freight_furnace"), FIntPoint(1, 2), 12.f, 4.f, 0.f, 3.f, 8.f, 0.f, 2.f },
            { TEXT("forgeweave.smog_reclaimer"), FIntPoint(2, 2), 14.f, 5.f, 0.f, 0.f, 12.f, 0.f, 2.f }
        };
        return Catalog;
    }

    uint32 MixDeterministic(const uint32 Value)
    {
        uint32 Mixed = Value;
        Mixed ^= Mixed >> 16;
        Mixed *= 0x7feb352dU;
        Mixed ^= Mixed >> 15;
        Mixed *= 0x846ca68bU;
        Mixed ^= Mixed >> 16;
        return Mixed;
    }

    uint32 MakeTieKey(
        const int32 CampaignSeed,
        const int64 WorldTick,
        const int32 CardIndex,
        const int32 X,
        const int32 Y)
    {
        uint32 Value = static_cast<uint32>(CampaignSeed);
        Value = MixDeterministic(Value ^ static_cast<uint32>(WorldTick));
        Value = MixDeterministic(Value ^ static_cast<uint32>(WorldTick >> 32));
        Value = MixDeterministic(Value ^ static_cast<uint32>(CardIndex + 1));
        Value = MixDeterministic(Value ^ static_cast<uint32>((Y + 1) * 257 + X + 1));
        return Value;
    }
}

const TArray<FName>& FDARivalCityPlanner::GetVerticalSliceBuildPool()
{
    return FDAForgeweaveCityState::GetVerticalSliceBuildPool();
}

bool FDARivalCityPlanner::IsVerticalSliceBuildCard(const FName CardDefinitionId)
{
    return FDAForgeweaveCityState::IsVerticalSliceBuildCard(CardDefinitionId);
}

const FDARivalBuildCandidate* FDARivalCityPlanner::FindBuildCandidate(const FName CardDefinitionId)
{
    return GetCandidateCatalog().FindByPredicate(
        [CardDefinitionId](const FDARivalBuildCandidate& Candidate)
        {
            return Candidate.CardDefinitionId == CardDefinitionId;
        });
}

FDARivalCandidateScore FDARivalCityPlanner::ScoreCandidate(
    const FDARivalBuildCandidate& Candidate,
    const FDARivalPlannerContext& Context,
    const FDAPlacementResult& Placement)
{
    FDARivalCandidateScore Score;
    Score.Housing = FMath::Max(0.f, Context.HousingShortage) * FMath::Max(0.f, Candidate.HousingCapacity);
    Score.Output = FMath::Max(0.f, Context.OutputShortage) * FMath::Max(0.f, Candidate.IndustrialOutput);
    Score.ResourceHunger = FMath::Max(0.f, Context.ResourceHunger) * FMath::Max(0.f, Candidate.ResourceHungerMitigation);
    Score.Defense = FMath::Max(0.f, Context.DefensePressure) * FMath::Max(0.f, Candidate.DefenseValue);

    const bool bFiniteCost = FMath::IsFinite(Candidate.DeploymentCapital) && Candidate.DeploymentCapital >= 0.f;
    const bool bAffordable = bFiniteCost
        && FMath::IsFinite(Context.AvailableCapital)
        && Context.AvailableCapital >= Candidate.DeploymentCapital
        && FMath::IsFinite(Candidate.ProductionThroughputCost)
        && Candidate.ProductionThroughputCost >= 0.f
        && FMath::IsFinite(Context.AvailableProductionThroughput)
        && Context.AvailableProductionThroughput >= Candidate.ProductionThroughputCost;
    if (bAffordable)
    {
        Score.Affordability = 20.f + FMath::Clamp(
            Context.AvailableCapital - Candidate.DeploymentCapital,
            0.f,
            20.f);
    }

    Score.PlacementFailure = Placement.FailureReason;
    if (Placement.bCanPlace)
    {
        Score.Placement = 25.f;
    }

    Score.bEligible = bAffordable && Placement.bCanPlace;
    Score.Total = Score.Housing
        + Score.Output
        + Score.ResourceHunger
        + Score.Defense
        + Score.Affordability
        + Score.Placement;
    Score.bEligible = Score.bEligible && FMath::IsFinite(Score.Total);
    return Score;
}

FDARivalPlannerDecision FDARivalCityPlanner::ChooseConstruction(
    const FDAForgeweaveCityState& State,
    const FDACityGridSubsystem& Grid,
    const int64 WorldTick,
    const float DefenseStrength) const
{
    FDARivalPlannerContext Context;
    Context.HousingShortage = static_cast<float>(FMath::Max(0, State.Population - State.HousingCapacity));
    Context.OutputShortage = FMath::Max(0.f, State.DesiredIndustrialOutput - State.ActiveIndustrialThroughput);
    Context.ResourceHunger = FMath::Clamp(State.ResourceHunger, 0.f, 100.f);
    Context.DefensePressure = FMath::Max(0.f, State.DefensePressure - FMath::Max(0.f, DefenseStrength));
    Context.AvailableCapital = State.Capital;
    Context.AvailableProductionThroughput = State.ProductionReserve;

    FDARivalPlannerDecision Best;
    float BestScore = -1.f;
    bool bAnyAffordable = false;
    bool bAnyUtilityPossible = false;
    bool bAnyPlacementPossible = false;

    const TArray<FDARivalBuildCandidate>& Catalog = GetCandidateCatalog();
    for (int32 CardIndex = 0; CardIndex < Catalog.Num(); ++CardIndex)
    {
        const FDARivalBuildCandidate& Candidate = Catalog[CardIndex];
        if (!State.AvailableCardIds.IsEmpty() && !State.AvailableCardIds.Contains(Candidate.CardDefinitionId))
        {
            continue;
        }

        bAnyAffordable = bAnyAffordable
            || (State.Capital >= Candidate.DeploymentCapital
                && State.ProductionReserve >= Candidate.ProductionThroughputCost);
        const bool bUtilityPossible = State.UtilityDemand + Candidate.UtilityDemand <= State.UtilitySupply + KINDA_SMALL_NUMBER;
        bAnyUtilityPossible = bAnyUtilityPossible || bUtilityPossible;

        for (int32 Y = 0; Y < State.GridHeight; ++Y)
        {
            for (int32 X = 0; X < State.GridWidth; ++X)
            {
                FDACardPlacementRequest Request;
                Request.AssetId = FDAWorldAssetId(FGuid(
                    static_cast<uint32>(State.CampaignSeed),
                    static_cast<uint32>(WorldTick),
                    static_cast<uint32>(CardIndex + 1),
                    static_cast<uint32>(Y * State.GridWidth + X + 1)));
                Request.Origin = FIntPoint(X, Y);
                Request.Footprint = Candidate.Footprint;
                Request.RequiredUtilityLayerMask = 1;

                FDAPlacementResult Placement = Grid.ValidatePlacement(Request);
                if (!bUtilityPossible)
                {
                    Placement = FDAPlacementResult::Failure(EDAPlacementFailureReason::UtilityUnavailable);
                }
                bAnyPlacementPossible = bAnyPlacementPossible || Placement.bCanPlace;

                const FDARivalCandidateScore Score = ScoreCandidate(Candidate, Context, Placement);
                if (!Score.bEligible)
                {
                    continue;
                }

                const uint32 TieKey = MakeTieKey(State.CampaignSeed, WorldTick, CardIndex, X, Y);
                if (Score.Total > BestScore + KINDA_SMALL_NUMBER
                    || (FMath::IsNearlyEqual(Score.Total, BestScore) && TieKey < Best.DeterministicTieKey))
                {
                    Best.Type = EDARivalDecisionType::Construct;
                    Best.CardDefinitionId = Candidate.CardDefinitionId;
                    Best.Origin = Request.Origin;
                    Best.Footprint = Candidate.Footprint;
                    Best.CapitalCost = Candidate.DeploymentCapital;
                    Best.ProductionThroughputCost = Candidate.ProductionThroughputCost;
                    Best.UtilityDemand = Candidate.UtilityDemand;
                    Best.HousingCapacity = Candidate.HousingCapacity;
                    Best.IndustrialOutput = Candidate.IndustrialOutput;
                    Best.ResourceHungerMitigation = Candidate.ResourceHungerMitigation;
                    Best.DefenseValue = Candidate.DefenseValue;
                    Best.Score = Score.Total;
                    Best.DeterministicTieKey = TieKey;
                    BestScore = Score.Total;
                }
            }
        }
    }

    if (Best.Type == EDARivalDecisionType::None)
    {
        if (!bAnyAffordable)
        {
            Best.CrisisExplanation = TEXT("crisis.unaffordable");
        }
        else if (!bAnyUtilityPossible)
        {
            Best.CrisisExplanation = TEXT("crisis.utility_capacity");
        }
        else if (!bAnyPlacementPossible)
        {
            Best.CrisisExplanation = TEXT("crisis.no_valid_placement");
        }
    }
    return Best;
}
