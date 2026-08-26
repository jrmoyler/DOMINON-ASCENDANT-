#pragma once

#include "Combat/DADamageTypes.h"
#include "GameplayEffectExecutionCalculation.h"

#include "DADamageExecution.generated.h"

class UDACombatAttributeSet;
struct FDADamageResolution;
struct FGameplayEffectSpec;

UCLASS()
class DOMINIONGAMEPLAY_API UDADamageExecution : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()

public:
    UDADamageExecution();

    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

private:
    static bool TryReadDamageSpec(const FGameplayEffectSpec& EffectSpec, float& OutRawDamage, EDADamageChannel& OutChannel);
    static FDADamageResolution ResolveDamage(const UDACombatAttributeSet& Target, float RawDamage, EDADamageChannel Channel);
};
