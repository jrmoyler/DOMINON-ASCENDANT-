#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Combat/DADamageTypes.h"
#include "GameplayTagContainer.h"

#include "DACombatAttributeSet.generated.h"

#define DA_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_MULTICAST_DELEGATE(FDAOnDownedExpiredNative);

UCLASS(BlueprintType)
class DOMINIONGAMEPLAY_API UDACombatAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UDACombatAttributeSet();

    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Resources")
    FGameplayAttributeData Health;
    DA_ATTRIBUTE_ACCESSORS(UDACombatAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Resources")
    FGameplayAttributeData Guard;
    DA_ATTRIBUTE_ACCESSORS(UDACombatAttributeSet, Guard)

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Resources")
    FGameplayAttributeData Stamina;
    DA_ATTRIBUTE_ACCESSORS(UDACombatAttributeSet, Stamina)

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Resources")
    FGameplayAttributeData TacticalCharge;
    DA_ATTRIBUTE_ACCESSORS(UDACombatAttributeSet, TacticalCharge)

    UFUNCTION(BlueprintCallable, Category = "Combat|Damage")
    void SetDamageResistance(EDADamageChannel Channel, float Resistance);

    UFUNCTION(BlueprintPure, Category = "Combat|Damage")
    float GetDamageResistance(EDADamageChannel Channel) const;

    UFUNCTION(BlueprintCallable, Category = "Combat|Status")
    void AddStatusTag(FGameplayTag StatusTag);

    UFUNCTION(BlueprintCallable, Category = "Combat|Status")
    void RemoveStatusTag(FGameplayTag StatusTag);

    UFUNCTION(BlueprintPure, Category = "Combat|Status")
    bool HasStatusTag(FGameplayTag StatusTag) const;

    UFUNCTION(BlueprintCallable, Category = "Combat|Status")
    void SetSynthetic(bool bInSynthetic);

    UFUNCTION(BlueprintPure, Category = "Combat|Status")
    bool IsSynthetic() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Downed")
    bool IsDowned() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Downed")
    float GetDownedRecoveryRemainingSeconds() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Downed")
    bool IsPermanentlyDead() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Downed")
    bool IsExtracted() const;

    FDAOnDownedExpiredNative OnDownedExpiredNative;

private:
    friend class UDAAbilitySystemComponent;

    void AdvanceDownedRecovery(float DeltaSeconds);
    void EnterDownedState();

    UPROPERTY(EditAnywhere, Category = "Combat|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    TMap<EDADamageChannel, float> DamageResistances;

    UPROPERTY(VisibleAnywhere, Category = "Combat|Status")
    FGameplayTagContainer StatusTags;

    bool bSynthetic = false;
    bool bPermanentlyDead = false;
    bool bExtracted = false;
    float DownedRecoveryRemainingSeconds = 0.f;
};

#undef DA_ATTRIBUTE_ACCESSORS
