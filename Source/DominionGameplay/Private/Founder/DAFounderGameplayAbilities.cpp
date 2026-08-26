#include "Founder/DAFounderGameplayAbilities.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Combat/DACombatAttributeSet.h"
#include "Combat/DACoverSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "Misc/Paths.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_DA_Founder_PrecisionScan, "synara.precision_scan");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_DA_Founder_DroneBarrier, "synara.drone_barrier");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_DA_Founder_OrchestrationMark, "synara.orchestration_mark");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_DA_Founder_CoordinatedOverride, "synara.coordinated_override");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_DA_Cooldown_PrecisionScan, "Cooldown.Synara.PrecisionScan");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_DA_Cooldown_DroneBarrier, "Cooldown.Synara.DroneBarrier");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_DA_Cooldown_OrchestrationMark, "Cooldown.Synara.OrchestrationMark");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_DA_Cooldown_CoordinatedOverride, "Cooldown.Synara.CoordinatedOverride");

namespace
{
    FGameplayTag CooldownTagForAbility(const FName AbilityId)
    {
        if (AbilityId == TAG_DA_Founder_PrecisionScan.GetTagName()) return TAG_DA_Cooldown_PrecisionScan;
        if (AbilityId == TAG_DA_Founder_DroneBarrier.GetTagName()) return TAG_DA_Cooldown_DroneBarrier;
        if (AbilityId == TAG_DA_Founder_OrchestrationMark.GetTagName()) return TAG_DA_Cooldown_OrchestrationMark;
        if (AbilityId == TAG_DA_Founder_CoordinatedOverride.GetTagName()) return TAG_DA_Cooldown_CoordinatedOverride;
        return FGameplayTag();
    }

    UAbilitySystemComponent* FindAbilitySystem(AActor* Actor)
    {
        if (Actor == nullptr) return nullptr;
        if (IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Actor))
        {
            return Interface->GetAbilitySystemComponent();
        }
        return Actor->FindComponentByClass<UAbilitySystemComponent>();
    }
}

void UDAFounderGameplayAbility::Configure(const FGameplayTag InAbilityTag)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    AbilityTags.AddTag(InAbilityTag);
    AbilityIdTag = InAbilityTag;
}

bool UDAFounderGameplayAbility::ResolveAuthoredDefinition(
    FDAFounderAbilityDefinition& OutDefinition) const
{
    if (!AbilityIdTag.IsValid()) return false;
    UDAFounderAbilityDefinitions* Definitions = NewObject<UDAFounderAbilityDefinitions>(
        GetTransientPackage());
    if (Definitions == nullptr || !Definitions->ImportFromJsonFile(
        FPaths::ProjectContentDir() / TEXT("DA/Abilities/FounderAbilityDefinitions.json")))
    {
        return false;
    }
    const FDAFounderAbilityDefinition* Found = Definitions->GetBaselineAbilities().FindByPredicate(
        [this](const FDAFounderAbilityDefinition& Definition)
        {
            return Definition.AbilityId == AbilityIdTag.GetTagName();
        });
    if (Found == nullptr || Found->CooldownSeconds <= 0.f || Found->DurationSeconds <= 0.f
        || !Found->GrantedStatusTag.IsValid() || Found->EffectId.IsNone())
    {
        return false;
    }
    OutDefinition = *Found;
    return true;
}

bool UDAFounderGameplayAbility::ResolveScopedTargets(
    const FGameplayAbilityActorInfo& ActorInfo,
    const FDAFounderAbilityDefinition& Definition,
    TArray<UAbilitySystemComponent*>& OutTargets) const
{
    OutTargets.Reset();
    UAbilitySystemComponent* Source = ActorInfo.AbilitySystemComponent.Get();
    AActor* Avatar = ActorInfo.AvatarActor.Get();
    UWorld* World = Avatar == nullptr ? nullptr : Avatar->GetWorld();
    if (Source == nullptr || Avatar == nullptr || World == nullptr)
    {
        return false;
    }
    if (Definition.TargetScope == EDAFounderAbilityTargetScope::Self)
    {
        OutTargets.Add(Source);
        return true;
    }
    const float RangeCentimeters = Definition.RangeMeters * 100.f;
    if (!FMath::IsFinite(RangeCentimeters) || RangeCentimeters <= 0.f)
    {
        return false;
    }
    if (Definition.TargetScope == EDAFounderAbilityTargetScope::ViewTarget)
    {
        FVector ViewLocation;
        FRotator ViewRotation;
        Avatar->GetActorEyesViewPoint(ViewLocation, ViewRotation);
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FounderAbilityTarget), false, Avatar);
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, ViewLocation,
            ViewLocation + (ViewRotation.Vector() * RangeCentimeters),
            ECC_Visibility, QueryParams))
        {
            return false;
        }
        if (UAbilitySystemComponent* Target = FindAbilitySystem(Hit.GetActor()))
        {
            OutTargets.Add(Target);
        }
        return OutTargets.Num() == 1;
    }
    if (Definition.TargetScope == EDAFounderAbilityTargetScope::AlliesInRadius)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Candidate = *It;
            const bool bFriendly = Candidate == Avatar
                || Candidate->ActorHasTag(TEXT("civilization.synara"));
            if (!bFriendly
                || FVector::DistSquared(Candidate->GetActorLocation(), Avatar->GetActorLocation())
                    > FMath::Square(RangeCentimeters))
            {
                continue;
            }
            if (UAbilitySystemComponent* Target = FindAbilitySystem(Candidate))
            {
                OutTargets.AddUnique(Target);
            }
        }
    }
    return !OutTargets.IsEmpty();
}

bool UDAFounderGameplayAbility::ApplyTimedTag(
    UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
    const FGameplayTag Tag, const float DurationSeconds) const
{
    if (!Tag.IsValid() || !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.f)
    {
        return false;
    }
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
    if (Effect == nullptr) return false;
    Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
    Effect->DurationMagnitude = FScalableFloat(DurationSeconds);
    FGameplayEffectSpec Spec(Effect, Source.MakeEffectContext(), 1.f);
    Spec.SetDuration(DurationSeconds, true);
    Spec.DynamicGrantedTags.AddTag(Tag);
    return Source.ApplyGameplayEffectSpecToTarget(Spec, &Target).IsValid();
}

bool UDAFounderGameplayAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags,
        OptionalRelevantTags) || ActorInfo == nullptr)
    {
        return false;
    }
    FDAFounderAbilityDefinition Definition;
    UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
    if (AbilitySystem == nullptr || !ResolveAuthoredDefinition(Definition)
        || AbilitySystem->HasMatchingGameplayTag(CooldownTagForAbility(Definition.AbilityId))
        || AbilitySystem->GetNumericAttribute(UDACombatAttributeSet::GetTacticalChargeAttribute())
            + KINDA_SMALL_NUMBER < Definition.TacticalChargeCost)
    {
        return false;
    }
    TArray<UAbilitySystemComponent*> Targets;
    return ResolveScopedTargets(*ActorInfo, Definition, Targets);
}

void UDAFounderGameplayAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    (void)TriggerEventData;
    FDAFounderAbilityDefinition Definition;
    UAbilitySystemComponent* AbilitySystem = ActorInfo == nullptr
        ? nullptr : ActorInfo->AbilitySystemComponent.Get();
    TArray<UAbilitySystemComponent*> Targets;
    if (AbilitySystem == nullptr || !ResolveAuthoredDefinition(Definition)
        || !ResolveScopedTargets(*ActorInfo, Definition, Targets)
        || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    bool bApplied = true;
    for (UAbilitySystemComponent* Target : Targets)
    {
        bApplied = Target != nullptr && ApplyTimedTag(*AbilitySystem, *Target,
            Definition.GrantedStatusTag, Definition.DurationSeconds) && bApplied;
    }
    const FGameplayTag CooldownTag = CooldownTagForAbility(Definition.AbilityId);
    bApplied = bApplied && ApplyTimedTag(*AbilitySystem, *AbilitySystem,
        CooldownTag, Definition.CooldownSeconds);
    if (!bApplied)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    const float Charge = AbilitySystem->GetNumericAttribute(
        UDACombatAttributeSet::GetTacticalChargeAttribute());
    AbilitySystem->SetNumericAttributeBase(
        UDACombatAttributeSet::GetTacticalChargeAttribute(),
        FMath::Max(0.f, Charge - Definition.TacticalChargeCost));
    if (Definition.EffectId == TEXT("effect.synara.drone_barrier.cover"))
    {
        RegisterDroneBarrierCover(*ActorInfo, Definition.DurationSeconds);
    }
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UDAFounderGameplayAbility::RegisterDroneBarrierCover(
    const FGameplayAbilityActorInfo& ActorInfo, const float DurationSeconds)
{
    AActor* Avatar = ActorInfo.AvatarActor.Get();
    UWorld* World = Avatar == nullptr ? nullptr : Avatar->GetWorld();
    UDACoverSubsystem* Cover = World == nullptr ? nullptr
        : World->GetSubsystem<UDACoverSubsystem>();
    if (Cover == nullptr) return;
    RemoveDroneBarrierCover();
    ActiveDeployableCoverId = FName(*FString::Printf(
        TEXT("cover.synara.drone_barrier.%u"), Avatar->GetUniqueID()));
    if (Cover->RegisterDeployableCover(ActiveDeployableCoverId,
        EDACoverType::Hardened, Avatar->GetActorLocation()))
    {
        World->GetTimerManager().SetTimer(CoverRemovalTimer, this,
            &UDAFounderGameplayAbility::RemoveDroneBarrierCover,
            DurationSeconds, false);
    }
    else
    {
        ActiveDeployableCoverId = NAME_None;
    }
}

void UDAFounderGameplayAbility::RemoveDroneBarrierCover()
{
    if (!ActiveDeployableCoverId.IsNone())
    {
        if (UWorld* World = GetWorld())
        {
            if (UDACoverSubsystem* Cover = World->GetSubsystem<UDACoverSubsystem>())
            {
                Cover->UnregisterDeployableCover(ActiveDeployableCoverId);
            }
            World->GetTimerManager().ClearTimer(CoverRemovalTimer);
        }
        ActiveDeployableCoverId = NAME_None;
    }
}

void UDAFounderGameplayAbility::BeginDestroy()
{
    RemoveDroneBarrierCover();
    Super::BeginDestroy();
}

UDAFounderPrecisionScanAbility::UDAFounderPrecisionScanAbility()
{
    Configure(TAG_DA_Founder_PrecisionScan);
}

UDAFounderDroneBarrierAbility::UDAFounderDroneBarrierAbility()
{
    Configure(TAG_DA_Founder_DroneBarrier);
}

UDAFounderOrchestrationMarkAbility::UDAFounderOrchestrationMarkAbility()
{
    Configure(TAG_DA_Founder_OrchestrationMark);
}

UDAFounderCoordinatedOverrideAbility::UDAFounderCoordinatedOverrideAbility()
{
    Configure(TAG_DA_Founder_CoordinatedOverride);
}
