#pragma once

#include "AI/DARivalCityPlanner.h"
#include "CoreMinimal.h"
#include "Save/DACampaignSaveGame.h"
#include "UObject/Object.h"

#include "DAForgeweaveStrategy.generated.h"

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAResourceHungerInputs
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave|Resource Hunger")
    float ActiveIndustrialThroughput = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave|Resource Hunger")
    float MaterialScarcity = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave|Resource Hunger")
    bool bOverdrive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave|Resource Hunger")
    float LogisticsEfficiency = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave|Resource Hunger")
    float Recycling = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave|Resource Hunger")
    float EdenRegenerativeInputs = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forgeweave|Resource Hunger")
    float ProductionReduction = 0.f;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAResourceHungerTick
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave|Resource Hunger")
    float ThroughputGrowth = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave|Resource Hunger")
    float ScarcityGrowth = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave|Resource Hunger")
    float OverdriveGrowth = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave|Resource Hunger")
    float LogisticsMitigation = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave|Resource Hunger")
    float RecyclingMitigation = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave|Resource Hunger")
    float EdenInputMitigation = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave|Resource Hunger")
    float ProductionReductionMitigation = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forgeweave|Resource Hunger")
    float ResultingResourceHunger = 0.f;
};

struct DOMINIONWORLD_API FDAForgeweaveTickResult
{
    bool bCommitted = false;
    bool bInvalidPlacement = false;
    bool bImpossibleUtilityState = false;
    bool bNegativeImpossibleEconomy = false;
    FDARivalPlannerDecision Decision;
    FDAResourceHungerTick ResourceHunger;
    FString FailureReason;
};

/** World-Tick-only Forgeweave strategy. A tick is copied, validated, then atomically committed. */
UCLASS()
class DOMINIONWORLD_API UDAForgeweaveStrategy final : public UObject
{
    GENERATED_BODY()

public:
    static FDAForgeweaveCityState MakeVerticalSliceInitialState(int32 CampaignSeed, int64 InitialWorldTick);
    static FDAResourceHungerTick CalculateResourceHungerTick(
        float CurrentResourceHunger,
        const FDAResourceHungerInputs& Inputs);

    FDAForgeweaveTickResult ProcessWorldTick(FDACampaignSnapshot& Campaign, int64 WorldTick) const;
};
