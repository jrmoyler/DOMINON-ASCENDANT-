#pragma once

#include "AbilitySystemComponent.h"

#include "DAAbilitySystemComponent.generated.h"

class UDACombatAttributeSet;

UCLASS(BlueprintType, ClassGroup = (Dominion), meta = (BlueprintSpawnableComponent))
class DOMINIONGAMEPLAY_API UDAAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()

public:
    UDAAbilitySystemComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
