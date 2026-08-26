#pragma once

#include "Abilities/GameplayAbility.h"
#include "Combat/DAFounderAbilityDefinitions.h"
#include "GameplayTagContainer.h"

#include "DAFounderGameplayAbilities.generated.h"

/** Native scoped Founder ability: validates charge, applies the authored status, and completes. */
UCLASS(Abstract)
class DOMINIONGAMEPLAY_API UDAFounderGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
    virtual void BeginDestroy() override;

protected:
    void Configure(FGameplayTag InAbilityTag);
    bool ResolveAuthoredDefinition(FDAFounderAbilityDefinition& OutDefinition) const;
    bool ResolveScopedTargets(const FGameplayAbilityActorInfo& ActorInfo,
        const FDAFounderAbilityDefinition& Definition,
        TArray<UAbilitySystemComponent*>& OutTargets) const;
    bool ApplyTimedTag(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
        FGameplayTag Tag, float DurationSeconds) const;
    void RegisterDroneBarrierCover(const FGameplayAbilityActorInfo& ActorInfo,
        float DurationSeconds);
    void RemoveDroneBarrierCover();

    FGameplayTag AbilityIdTag;
    FName ActiveDeployableCoverId;
    FTimerHandle CoverRemovalTimer;
};

UCLASS()
class DOMINIONGAMEPLAY_API UDAFounderPrecisionScanAbility final : public UDAFounderGameplayAbility
{
    GENERATED_BODY()
public:
    UDAFounderPrecisionScanAbility();
};

UCLASS()
class DOMINIONGAMEPLAY_API UDAFounderDroneBarrierAbility final : public UDAFounderGameplayAbility
{
    GENERATED_BODY()
public:
    UDAFounderDroneBarrierAbility();
};

UCLASS()
class DOMINIONGAMEPLAY_API UDAFounderOrchestrationMarkAbility final : public UDAFounderGameplayAbility
{
    GENERATED_BODY()
public:
    UDAFounderOrchestrationMarkAbility();
};

UCLASS()
class DOMINIONGAMEPLAY_API UDAFounderCoordinatedOverrideAbility final : public UDAFounderGameplayAbility
{
    GENERATED_BODY()
public:
    UDAFounderCoordinatedOverrideAbility();
};
