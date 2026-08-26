#include "Combat/DADamageExecution.h"

#include "AbilitySystemComponent.h"
#include "Combat/DACombatAttributeSet.h"
#include "GameplayEffect.h"

UDADamageExecution::UDADamageExecution()
{
}

void UDADamageExecution::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    const UAbilitySystemComponent* TargetAbilitySystem = ExecutionParams.GetTargetAbilitySystemComponent();
    const UDACombatAttributeSet* Target = TargetAbilitySystem != nullptr
        ? TargetAbilitySystem->GetSet<UDACombatAttributeSet>()
        : nullptr;
    if (Target == nullptr)
    {
        return;
    }

    const FGameplayEffectSpec& EffectSpec = ExecutionParams.GetOwningSpec();
    float RawDamage = 0.f;
    EDADamageChannel Channel = EDADamageChannel::Kinetic;
    if (!TryReadDamageSpec(EffectSpec, RawDamage, Channel))
    {
        return;
    }

    const FDADamageResolution Resolution = ResolveDamage(*Target, RawDamage, Channel);
    if (Resolution.GuardDamage > 0.f)
    {
        OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
            UDACombatAttributeSet::GetGuardAttribute(), EGameplayModOp::Additive, -Resolution.GuardDamage));
    }
    if (Resolution.HealthDamage > 0.f)
    {
        OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
            UDACombatAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, -Resolution.HealthDamage));
    }
}

bool UDADamageExecution::TryReadDamageSpec(
    const FGameplayEffectSpec& EffectSpec,
    float& OutRawDamage,
    EDADamageChannel& OutChannel)
{
    OutRawDamage = EffectSpec.GetSetByCallerMagnitude(DADamageTags::Damage(), false, -1.f);
    if (OutRawDamage < 0.f)
    {
        return false;
    }

    const TPair<FGameplayTag, EDADamageChannel> ChannelTags[] = {
        { DADamageTags::Kinetic(), EDADamageChannel::Kinetic },
        { DADamageTags::Thermal(), EDADamageChannel::Thermal },
        { DADamageTags::Arc(), EDADamageChannel::Arc },
        { DADamageTags::Bio(), EDADamageChannel::Bio },
        { DADamageTags::Cyber(), EDADamageChannel::Cyber },
        { DADamageTags::Structural(), EDADamageChannel::Structural }
    };

    int32 ChannelCount = 0;
    for (const TPair<FGameplayTag, EDADamageChannel>& ChannelTag : ChannelTags)
    {
        const float ChannelMarker = EffectSpec.GetSetByCallerMagnitude(ChannelTag.Key, false, -1.f);
        if (ChannelMarker < 0.f)
        {
            continue;
        }

        if (!FMath::IsNearlyEqual(ChannelMarker, 1.f))
        {
            return false;
        }

        OutChannel = ChannelTag.Value;
        ++ChannelCount;
    }

    return ChannelCount == 1;
}

FDADamageResolution UDADamageExecution::ResolveDamage(
    const UDACombatAttributeSet& Target,
    const float RawDamage,
    const EDADamageChannel Channel)
{
    FDADamageResolution Resolution;
    Resolution.FinalDamage = FMath::Max(0.f, RawDamage)
        * (1.f - Target.GetDamageResistance(Channel));
    Resolution.GuardDamage = FMath::Min(Target.GetGuard(), Resolution.FinalDamage);
    Resolution.HealthDamage = Resolution.FinalDamage - Resolution.GuardDamage;
    return Resolution;
}
