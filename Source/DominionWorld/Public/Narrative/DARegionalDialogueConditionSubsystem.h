#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DARegionalDialogueConditionSubsystem.generated.h"

class UDAWorldStateSubsystem;

/** Production dialogue query authority for durable regional quest and citizen outcomes. */
UCLASS()
class DOMINIONWORLD_API UDARegionalDialogueConditionSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UFUNCTION(BlueprintPure, Category="Dialogue|Regional")
    FName EvaluateVariant(FName ConditionId) const;
    UFUNCTION(BlueprintPure, Category="Dialogue|Regional")
    bool IsConditionSatisfied(FName ConditionId) const { return !EvaluateVariant(ConditionId).IsNone(); }
private:
    UPROPERTY(Transient) TWeakObjectPtr<UDAWorldStateSubsystem> WorldStateSubsystem;
};
