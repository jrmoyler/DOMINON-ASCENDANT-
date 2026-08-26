#pragma once

#include "CoreMinimal.h"
#include "Economy/DAEconomyTypes.h"
#include "UObject/Object.h"

#include "DASystemicPressureSystem.generated.h"

struct FDACampaignSnapshot;

UENUM(BlueprintType)
enum class EDASystemicPressureThreshold : uint8
{
    Dependency25 = 25,
    Dependency50 = 50,
    Dependency70 = 70,
    Dependency85 = 85,
    Dependency100 = 100
};

/**
 * The narrative layer binds to this event to respond to threshold changes.
 * This system deliberately does not create or launch quests.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FDASystemicPressureChanged,
    EDASystemicPressureThreshold /* Threshold */,
    float /* PreviousDependency */,
    float /* NewDependency */);

UCLASS()
class DOMINIONSIMULATION_API UDASystemicPressureSystem final : public UObject
{
    GENERATED_BODY()

public:
    FDASystemicPressureChanged OnSystemicPressureChanged;

    void SetDependency(float NewDependency);
    float GetDependency() const { return Dependency; }
    bool ApplyDependencyReason(FDACampaignSnapshot& Campaign, FName ActionId, double Delta, int64 WorldTick);
    float GetDependency(const FDACampaignSnapshot& Campaign) const;

private:
    float Dependency = 0.f;
};
