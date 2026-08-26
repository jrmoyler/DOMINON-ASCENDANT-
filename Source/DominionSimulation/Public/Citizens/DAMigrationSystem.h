#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "DAMigrationSystem.generated.h"

struct FDACitySimulationState;

/** Resolves the v0.8 fractional migration formulas at World Tick boundaries. */
UCLASS()
class DOMINIONSIMULATION_API UDAMigrationSystem final : public UObject
{
    GENERATED_BODY()

public:
    static float CalculateIncomingPerWorldTick(int32 Vacancy, float Attractiveness);
    static float CalculateOutgoingPerWorldTick(int32 Population, float Attractiveness);
    void ResolveWorldTick(FDACitySimulationState& State) const;
};
