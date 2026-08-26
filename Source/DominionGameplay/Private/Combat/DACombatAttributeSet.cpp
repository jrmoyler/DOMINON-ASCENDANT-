#include "Combat/DACombatAttributeSet.h"

#include "Combat/DAStatusTags.h"
#include "GameplayEffectExtension.h"

UDACombatAttributeSet::UDACombatAttributeSet()
{
    InitHealth(100.f);
    InitGuard(50.f);
    InitStamina(100.f);
    InitTacticalCharge(0.f);
}

void UDACombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute() || Attribute == GetGuardAttribute() || Attribute == GetStaminaAttribute())
    {
        NewValue = FMath::Max(0.f, NewValue);
    }
    else if (Attribute == GetTacticalChargeAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
    }
}

void UDACombatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Max(0.f, GetHealth()));
        if (GetHealth() <= 0.f)
        {
            EnterDownedState();
        }
    }
    else if (Data.EvaluatedData.Attribute == GetGuardAttribute())
    {
        SetGuard(FMath::Max(0.f, GetGuard()));
    }
    else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
    {
        SetStamina(FMath::Max(0.f, GetStamina()));
    }
    else if (Data.EvaluatedData.Attribute == GetTacticalChargeAttribute())
    {
        SetTacticalCharge(FMath::Clamp(GetTacticalCharge(), 0.f, 100.f));
    }
}

void UDACombatAttributeSet::SetDamageResistance(const EDADamageChannel Channel, const float Resistance)
{
    DamageResistances.Add(Channel, FMath::Clamp(Resistance, 0.f, 1.f));
}

float UDACombatAttributeSet::GetDamageResistance(const EDADamageChannel Channel) const
{
    const float* Resistance = DamageResistances.Find(Channel);
    return Resistance != nullptr ? *Resistance : 0.f;
}

void UDACombatAttributeSet::AddStatusTag(const FGameplayTag StatusTag)
{
    StatusTags.AddTag(StatusTag);
    if (UAbilitySystemComponent* AbilitySystem = GetOwningAbilitySystemComponent())
    {
        AbilitySystem->AddLooseGameplayTag(StatusTag);
    }
}

void UDACombatAttributeSet::RemoveStatusTag(const FGameplayTag StatusTag)
{
    StatusTags.RemoveTag(StatusTag);
    if (UAbilitySystemComponent* AbilitySystem = GetOwningAbilitySystemComponent())
    {
        AbilitySystem->RemoveLooseGameplayTag(StatusTag);
    }
}

bool UDACombatAttributeSet::HasStatusTag(const FGameplayTag StatusTag) const
{
    return StatusTags.HasTagExact(StatusTag)
        || (GetOwningAbilitySystemComponent() != nullptr && GetOwningAbilitySystemComponent()->HasMatchingGameplayTag(StatusTag));
}

void UDACombatAttributeSet::SetSynthetic(const bool bInSynthetic)
{
    bSynthetic = bInSynthetic;
}

bool UDACombatAttributeSet::IsSynthetic() const
{
    return bSynthetic;
}

bool UDACombatAttributeSet::IsDowned() const
{
    return HasStatusTag(DAStatusTags::Downed());
}

float UDACombatAttributeSet::GetDownedRecoveryRemainingSeconds() const
{
    return DownedRecoveryRemainingSeconds;
}

bool UDACombatAttributeSet::IsPermanentlyDead() const
{
    return bPermanentlyDead;
}

bool UDACombatAttributeSet::IsExtracted() const
{
    return bExtracted;
}

void UDACombatAttributeSet::AdvanceDownedRecovery(const float DeltaSeconds)
{
    if (!IsDowned())
    {
        return;
    }

    DownedRecoveryRemainingSeconds = FMath::Max(0.f, DownedRecoveryRemainingSeconds - FMath::Max(0.f, DeltaSeconds));
    if (DownedRecoveryRemainingSeconds > 0.f)
    {
        return;
    }

    RemoveStatusTag(DAStatusTags::Downed());
    bExtracted = true;
    bPermanentlyDead = false;
    OnDownedExpiredNative.Broadcast();
}

void UDACombatAttributeSet::EnterDownedState()
{
    if (IsDowned())
    {
        return;
    }

    AddStatusTag(DAStatusTags::Downed());
    DownedRecoveryRemainingSeconds = 20.f;
    bExtracted = false;
    bPermanentlyDead = false;
}
