#pragma once

#include "CoreMinimal.h"
#include "Economy/DAEconomyTypes.h"
#include "UObject/Object.h"

#include "DAFactionSystem.generated.h"

struct FDACampaignSnapshot;

/** Persistent political pressure for a single faction. All values are normalized to 0-100. */
USTRUCT(BlueprintType)
struct DOMINIONSIMULATION_API FDAFactionState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
    FName FactionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
    FString Demand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float Support = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float Organization = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float Radicalization = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float Grievance = 0.f;
};

/** Synara's faction definitions and its v0.8 recalculated Dependency formula. */
UCLASS()
class DOMINIONSIMULATION_API UDAFactionSystem final : public UObject
{
    GENERATED_BODY()

public:
    static TArray<FDAFactionState> CreateSynaraFactions();
    static float CalculateDependency(const FDACitySimulationState& State);
    static float GetSupport(const FDACampaignSnapshot& Campaign, FName FactionId);
    static bool ApplySupportReason(FDACampaignSnapshot& Campaign, FName ActionId, FName FactionId,
        double Delta, int64 WorldTick);
};
