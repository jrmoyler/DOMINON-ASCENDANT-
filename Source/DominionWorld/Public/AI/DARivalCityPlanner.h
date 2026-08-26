#pragma once

#include "City/DACityGridSubsystem.h"
#include "CoreMinimal.h"
#include "World/DAForgeweaveCityState.h"

struct DOMINIONWORLD_API FDARivalBuildCandidate
{
    FName CardDefinitionId;
    FIntPoint Footprint = FIntPoint(1, 1);
    float DeploymentCapital = 0.f;
    float ProductionThroughputCost = 0.f;
    float HousingCapacity = 0.f;
    float IndustrialOutput = 0.f;
    float ResourceHungerMitigation = 0.f;
    float DefenseValue = 0.f;
    float UtilityDemand = 0.f;
};

struct DOMINIONWORLD_API FDARivalPlannerContext
{
    float HousingShortage = 0.f;
    float OutputShortage = 0.f;
    float ResourceHunger = 0.f;
    float DefensePressure = 0.f;
    float AvailableCapital = 0.f;
    float AvailableProductionThroughput = 0.f;
};

struct DOMINIONWORLD_API FDARivalCandidateScore
{
    bool bEligible = false;
    float Housing = 0.f;
    float Output = 0.f;
    float ResourceHunger = 0.f;
    float Defense = 0.f;
    float Affordability = 0.f;
    float Placement = 0.f;
    float Total = 0.f;
    EDAPlacementFailureReason PlacementFailure = EDAPlacementFailureReason::None;
};

struct DOMINIONWORLD_API FDARivalPlannerDecision
{
    EDARivalDecisionType Type = EDARivalDecisionType::None;
    FName CardDefinitionId;
    FName TargetBuildingId;
    FIntPoint Origin = FIntPoint::ZeroValue;
    FIntPoint Footprint = FIntPoint(1, 1);
    float CapitalCost = 0.f;
    float ProductionThroughputCost = 0.f;
    float UtilityDemand = 0.f;
    float HousingCapacity = 0.f;
    float IndustrialOutput = 0.f;
    float ResourceHungerMitigation = 0.f;
    float DefenseValue = 0.f;
    float Score = 0.f;
    uint32 DeterministicTieKey = MAX_uint32;
    FName CrisisExplanation;
};

/** Strategic construction scorer. It has no mutable state and never expands the frozen pool. */
class DOMINIONWORLD_API FDARivalCityPlanner
{
public:
    static const TArray<FName>& GetVerticalSliceBuildPool();
    static bool IsVerticalSliceBuildCard(FName CardDefinitionId);
    static const FDARivalBuildCandidate* FindBuildCandidate(FName CardDefinitionId);

    static FDARivalCandidateScore ScoreCandidate(
        const FDARivalBuildCandidate& Candidate,
        const FDARivalPlannerContext& Context,
        const FDAPlacementResult& Placement);

    FDARivalPlannerDecision ChooseConstruction(
        const FDAForgeweaveCityState& State,
        const FDACityGridSubsystem& Grid,
        int64 WorldTick,
        float DefenseStrength = 0.f) const;
};
