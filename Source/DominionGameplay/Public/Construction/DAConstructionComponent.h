#pragma once

#include "City/DAWorldAssetRecord.h"
#include "Components/ActorComponent.h"

#include "DAConstructionComponent.generated.h"

class UDA_CardDefinition;
struct FCardInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDAConstructionStageChanged, EDAConstructionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDAConstructionCompleted);

UCLASS(ClassGroup = (Dominion), BlueprintType, meta = (BlueprintSpawnableComponent))
class DOMINIONGAMEPLAY_API UDAConstructionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // Binds this presentation-facing component to the authoritative persistent record and linked card instance.
    bool InitializeFromRecord(FDAWorldAssetRecord& InRecord, const FCardInstance* InCardInstance, const UDA_CardDefinition& InCardDefinition);

    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool AdvanceCycle();

    /** Production-cycle path for deterministic adjacency speed; the no-arg Blueprint API remains 1x. */
    bool AdvanceCycleWithSpeed(float ConstructionSpeedMultiplier);

    UFUNCTION(BlueprintPure, Category = "Construction")
    EDAConstructionState GetConstructionState() const;

    /** Copies the committed record used by presentation auto-binding; never grants mutation access. */
    bool GetPresentationRecord(FDAWorldAssetRecord& OutRecord) const;

    UPROPERTY(BlueprintAssignable, Category = "Construction")
    FDAConstructionStageChanged OnStageChanged;

    UPROPERTY(BlueprintAssignable, Category = "Construction")
    FDAConstructionCompleted OnConstructionCompleted;

private:
    static EDAConstructionState GetStateForCompletedCycles(int32 CompletedCycles, int32 RequiredCycles);
    static bool IsConstructionState(EDAConstructionState State);

    // Non-owning: simulation owns and persists this record.
    FDAWorldAssetRecord* Record = nullptr;
};
