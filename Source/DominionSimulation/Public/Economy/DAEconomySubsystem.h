#pragma once

#include "CoreMinimal.h"
#include "Economy/DAEconomyTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DAEconomySubsystem.generated.h"

class UDA_CardDefinition;

/**
 * Resolves traceable city economy state only at discrete Development Cycle
 * boundaries emitted by UDASimulationClockSubsystem.
 */
UCLASS()
class DOMINIONSIMULATION_API UDAEconomySubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    FDAFacilityOutput CalculateFacilityOutput(const FDAFacilityContext& Context) const;
    /** Adds the exact authored Autonomous Factory modifier to the canonical facility context. */
    bool ApplyAutonomousFactoryThroughput(
        FDAFacilityContext& Context, const UDA_CardDefinition& Definition) const;
    void ResolveDevelopmentCycle(FDACitySimulationState& State) const;

    float CalculateDeploymentCapital(
        EDADeploymentTier Tier,
        EDAFootprintClass Footprint,
        float ComplexityMultiplier) const;
    int32 RoundDeploymentCapitalForDisplay(float PreciseCapital) const;

};
