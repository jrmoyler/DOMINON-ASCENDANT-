#pragma once

#include "Citizens/DACitizenRecord.h"
#include "CoreMinimal.h"
#include "Simulation/DACitySimulationState.h"
#include "UObject/Object.h"

#include "DAJobSystem.generated.h"

class UDA_CardDefinition;

USTRUCT(BlueprintType)
struct DOMINIONSIMULATION_API FDAJobMatchResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizen|Job")
    EDAJobMatchQuality Quality = EDAJobMatchQuality::Unqualified;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizen|Job")
    float OutputMultiplier = 0.f;
};

/** Deterministic local job matching partitioned by city, district, class/skill and commute zone. */
UCLASS()
class DOMINIONSIMULATION_API UDAJobSystem final : public UObject
{
    GENERATED_BODY()

public:
    static int32 CalculateDefaultWorkforce(int32 Population);
    /** Applies the exact authored facility requirement modifier through the jobs authority. */
    static int32 CalculateFacilityWorkforceRequirement(
        int32 BaseRequirement, const UDA_CardDefinition& Definition);
    FDAJobMatchResult EvaluateMatch(const FDACitizenRecord& Citizen, const FDAJobOpening& Job) const;
    void ResolveAssignments(FDACitySimulationState& State) const;

    /** Frozen v1.1 named-citizen content, in canonical specification order. */
    static TArray<FDACitizenRecord> CreateNamedCitizenRoster();
};
