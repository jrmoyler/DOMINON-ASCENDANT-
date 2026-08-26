#include "Combat/DADamageTypes.h"
#include "Combat/DADamageExecution.h"
#include "Combat/DACombatAttributeSet.h"
#include "Combat/DAFounderAbilityDefinitions.h"
#include "Combat/DAStatusTags.h"
#include "Combat/DAAbilitySystemComponent.h"
#include "Combat/DACoverSubsystem.h"
#include "Founder/DAFounderCharacter.h"
#include "Founder/DAFounderGameplayAbilities.h"

#include "GameplayEffect.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    struct FDACombatTestTarget
    {
        UWorld* World = nullptr;
        AActor* Actor = nullptr;
        UDAAbilitySystemComponent* AbilitySystem = nullptr;
        UDACombatAttributeSet* Attributes = nullptr;

        ~FDACombatTestTarget()
        {
            if (World == nullptr)
            {
                return;
            }

            World->DestroyWorld(false);
            if (GEngine != nullptr)
            {
                GEngine->DestroyWorldContext(World);
            }
        }

        FDACombatTestTarget() = default;
        FDACombatTestTarget(const FDACombatTestTarget&) = delete;
        FDACombatTestTarget& operator=(const FDACombatTestTarget&) = delete;
        FDACombatTestTarget(FDACombatTestTarget&& Other) noexcept
            : World(Other.World)
            , Actor(Other.Actor)
            , AbilitySystem(Other.AbilitySystem)
            , Attributes(Other.Attributes)
        {
            Other.World = nullptr;
            Other.Actor = nullptr;
            Other.AbilitySystem = nullptr;
            Other.Attributes = nullptr;
        }

        FDACombatTestTarget& operator=(FDACombatTestTarget&&) = delete;
    };

    FDACombatTestTarget CreateCombatTestTarget()
    {
        FDACombatTestTarget Target;
        Target.World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, GetTransientPackage());
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(Target.World);
        Target.World->InitializeNewWorld(UWorld::InitializationValues()
            .AllowAudioPlayback(false)
            .CreatePhysicsScene(false)
            .CreateNavigation(false)
            .CreateAISystem(false)
            .ShouldSimulatePhysics(false)
            .SetTransactional(false));
        Target.Actor = Target.World->SpawnActor<AActor>();
        Target.Actor->SetReplicates(true);
        Target.AbilitySystem = NewObject<UDAAbilitySystemComponent>(Target.Actor);
        Target.Actor->AddInstanceComponent(Target.AbilitySystem);
        Target.AbilitySystem->RegisterComponent();
        Target.Attributes = NewObject<UDACombatAttributeSet>(Target.AbilitySystem);
        Target.AbilitySystem->AddAttributeSetSubobject(Target.Attributes);
        Target.AbilitySystem->InitAbilityActorInfo(Target.Actor, Target.Actor);
        return MoveTemp(Target);
    }

    struct FDAFounderAbilityTestTarget
    {
        UWorld* World = nullptr;
        ADAFounderCharacter* Founder = nullptr;

        ~FDAFounderAbilityTestTarget()
        {
            if (World == nullptr) return;
            World->DestroyWorld(false);
            if (GEngine != nullptr) GEngine->DestroyWorldContext(World);
        }

        FDAFounderAbilityTestTarget() = default;
        FDAFounderAbilityTestTarget(const FDAFounderAbilityTestTarget&) = delete;
        FDAFounderAbilityTestTarget& operator=(const FDAFounderAbilityTestTarget&) = delete;
        FDAFounderAbilityTestTarget(FDAFounderAbilityTestTarget&& Other) noexcept
            : World(Other.World), Founder(Other.Founder)
        {
            Other.World = nullptr;
            Other.Founder = nullptr;
        }
        FDAFounderAbilityTestTarget& operator=(FDAFounderAbilityTestTarget&&) = delete;
    };

    FDAFounderAbilityTestTarget CreateFounderAbilityTestTarget()
    {
        FDAFounderAbilityTestTarget Target;
        Target.World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
            GetTransientPackage());
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(Target.World);
        Target.World->InitializeNewWorld(UWorld::InitializationValues()
            .AllowAudioPlayback(false)
            .CreatePhysicsScene(false)
            .CreateNavigation(false)
            .CreateAISystem(false)
            .ShouldSimulatePhysics(false)
            .SetTransactional(false));
        Target.Founder = Target.World->SpawnActor<ADAFounderCharacter>();
        if (Target.Founder != nullptr && !Target.Founder->HasActorBegunPlay())
        {
            Target.Founder->DispatchBeginPlay();
        }
        return MoveTemp(Target);
    }

    UGameplayEffect* CreateDamageEffect()
    {
        UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>();
        DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
        FGameplayEffectExecutionDefinition& Execution = DamageEffect->Executions.AddDefaulted_GetRef();
        Execution.CalculationClass = UDADamageExecution::StaticClass();
        return DamageEffect;
    }

    void ApplyDamageEffect(
        UDAAbilitySystemComponent& AbilitySystem,
        const UGameplayEffect& DamageEffect,
        const float RawDamage,
        const FGameplayTag ChannelTag)
    {
        FGameplayEffectSpec Spec(&DamageEffect, AbilitySystem.MakeEffectContext(), 1.f);
        Spec.SetSetByCallerMagnitude(DADamageTags::Damage(), RawDamage);
        if (ChannelTag.IsValid())
        {
            Spec.SetSetByCallerMagnitude(ChannelTag, 1.f);
        }
        AbilitySystem.ApplyGameplayEffectSpecToSelf(Spec);
    }
}

BEGIN_DEFINE_SPEC(FDACombatAttributesSpec, "Dominion.Gameplay.CombatAttributes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDACombatAttributesSpec)

void FDACombatAttributesSpec::Define()
{
    It("applies a real Arc GameplayEffect through a spawned actor's registered ability system and resolves Guard before Health", [this]()
    {
        FDACombatTestTarget Target = CreateCombatTestTarget();
        UGameplayEffect* DamageEffect = CreateDamageEffect();
        Target.Attributes->SetHealth(100.f);
        Target.Attributes->SetGuard(50.f);
        Target.Attributes->SetSynthetic(true);
        Target.Attributes->SetDamageResistance(EDADamageChannel::Arc, 0.25f);
        Target.Attributes->AddStatusTag(DAStatusTags::Shielded());

        ApplyDamageEffect(*Target.AbilitySystem, *DamageEffect, 40.f, DADamageTags::Arc());

        TestTrue("The target is spawned into a real test world", Target.Actor != nullptr && Target.Actor->GetWorld() == Target.World);
        TestTrue("The spawned target is replication-ready", Target.Actor->GetIsReplicated());
        TestTrue("The ability system is owned by the real target actor", Target.AbilitySystem->GetOwner() == Target.Actor);
        TestTrue("The ability system is an instance component", Target.Actor->GetInstanceComponents().Contains(Target.AbilitySystem));
        TestTrue("The ability system component is registered", Target.AbilitySystem->IsRegistered());
        TestTrue("The combat attributes are registered on the ability system", Target.AbilitySystem->GetSet<UDACombatAttributeSet>() == Target.Attributes);
        TestTrue("The target is explicitly synthetic", Target.Attributes->IsSynthetic());
        TestTrue("Shielded status is present", Target.Attributes->HasStatusTag(DAStatusTags::Shielded()));
        TestEqual("Configured Arc resistance reduces 40 raw damage to 30", Target.Attributes->GetGuard(), 20.f);
        TestEqual("Guard resolves before Health", Target.Attributes->GetHealth(), 100.f);
    });

    It("rejects a GameplayEffect missing or using an invalid damage-channel tag", [this]()
    {
        FDACombatTestTarget Target = CreateCombatTestTarget();
        UGameplayEffect* DamageEffect = CreateDamageEffect();
        Target.Attributes->SetHealth(100.f);
        Target.Attributes->SetGuard(50.f);

        ApplyDamageEffect(*Target.AbilitySystem, *DamageEffect, 40.f, FGameplayTag());
        TestEqual("A missing channel tag cannot apply damage", Target.Attributes->GetGuard(), 50.f);

        ApplyDamageEffect(*Target.AbilitySystem, *DamageEffect, 40.f, DAStatusTags::Shielded());
        TestEqual("An unrelated valid tag cannot act as a damage channel", Target.Attributes->GetGuard(), 50.f);
    });

    It("expires Downed through the authoritative registered component tick and signals extraction without permanent death", [this]()
    {
        FDACombatTestTarget Founder = CreateCombatTestTarget();
        UGameplayEffect* DamageEffect = CreateDamageEffect();
        Founder.Attributes->SetHealth(100.f);
        Founder.Attributes->SetGuard(0.f);
        bool bExtractionSignalled = false;
        Founder.Attributes->OnDownedExpiredNative.AddLambda([&bExtractionSignalled]()
        {
            bExtractionSignalled = true;
        });

        ApplyDamageEffect(*Founder.AbilitySystem, *DamageEffect, 100.f, DADamageTags::Kinetic());

        TestEqual("Lethal GameplayEffect clamps Health at zero", Founder.Attributes->GetHealth(), 0.f);
        TestTrue("Zero Health applies the Downed status", Founder.Attributes->HasStatusTag(DAStatusTags::Downed()));
        TestEqual("Downed starts the canonical twenty-second recovery window", Founder.Attributes->GetDownedRecoveryRemainingSeconds(), 20.f);
        TestFalse("Downed is not an immediate permanent death", Founder.Attributes->IsPermanentlyDead());

        TestTrue("The spawned combat owner has server authority", Founder.Actor->HasAuthority());
        Founder.World->Tick(LEVELTICK_All, 19.5f);
        TestTrue("Downed remains active before its recovery window expires", Founder.Attributes->IsDowned());
        TestFalse("Extraction has not occurred before expiry", Founder.Attributes->IsExtracted());
        TestFalse("No expiry signal is emitted early", bExtractionSignalled);

        Founder.World->Tick(LEVELTICK_All, 0.5f);
        TestFalse("Downed clears when the recovery window expires", Founder.Attributes->IsDowned());
        TestTrue("Expiry causes an extraction state", Founder.Attributes->IsExtracted());
        TestTrue("Expiry emits the extraction signal", bExtractionSignalled);
        TestFalse("Extraction remains non-permanent", Founder.Attributes->IsPermanentlyDead());
    });

    It("imports the single source-controlled Founder ability definition file into a runtime data asset", [this]()
    {
        const FString SourcePath = FPaths::ProjectContentDir() / TEXT("DA/Abilities/FounderAbilityDefinitions.json");
        UDAFounderAbilityDefinitions* Definitions = NewObject<UDAFounderAbilityDefinitions>();
        TestTrue("The runtime data asset imports the source-controlled declaration", Definitions->ImportFromJsonFile(SourcePath));
        const TArray<FDAFounderAbilityDefinition>& ImportedDefinitions = Definitions->GetBaselineAbilities();

        TestEqual("The imported Founder baseline has three active abilities and one ultimate", ImportedDefinitions.Num(), 4);
        TestTrue("Precision Scan is imported", UDAFounderAbilityDefinitions::ContainsAbility(ImportedDefinitions, TEXT("synara.precision_scan")));
        TestTrue("Drone Barrier is imported", UDAFounderAbilityDefinitions::ContainsAbility(ImportedDefinitions, TEXT("synara.drone_barrier")));
        TestTrue("Orchestration Mark is imported", UDAFounderAbilityDefinitions::ContainsAbility(ImportedDefinitions, TEXT("synara.orchestration_mark")));
        TestTrue("Coordinated Override is imported", UDAFounderAbilityDefinitions::ContainsAbility(ImportedDefinitions, TEXT("synara.coordinated_override")));

        const auto FindAbility = [&ImportedDefinitions](const FName AbilityId)
        {
            return ImportedDefinitions.FindByPredicate(
                [AbilityId](const FDAFounderAbilityDefinition& Definition)
                { return Definition.AbilityId == AbilityId; });
        };
        const FDAFounderAbilityDefinition* Precision = FindAbility(TEXT("synara.precision_scan"));
        const FDAFounderAbilityDefinition* Barrier = FindAbility(TEXT("synara.drone_barrier"));
        const FDAFounderAbilityDefinition* Mark = FindAbility(TEXT("synara.orchestration_mark"));
        const FDAFounderAbilityDefinition* Override = FindAbility(TEXT("synara.coordinated_override"));
        TestNotNull("Precision Scan resolves authored behavior", Precision);
        TestNotNull("Drone Barrier resolves authored behavior", Barrier);
        TestNotNull("Orchestration Mark resolves authored behavior", Mark);
        TestNotNull("Coordinated Override resolves authored behavior", Override);
        if (Precision != nullptr)
        {
            TestEqual("Precision Scan cooldown is exact", Precision->CooldownSeconds, 8.f);
            TestEqual("Precision Scan duration is exact", Precision->DurationSeconds, 8.f);
            TestEqual("Precision Scan targets the view trace", Precision->TargetScope,
                EDAFounderAbilityTargetScope::ViewTarget);
            TestEqual("Precision Scan range is exact", Precision->RangeMeters, 40.f);
            TestEqual("Precision Scan effect is concrete", Precision->EffectId,
                FName(TEXT("effect.synara.precision_scan.reveal")));
        }
        if (Barrier != nullptr)
        {
            TestEqual("Drone Barrier cooldown is exact", Barrier->CooldownSeconds, 18.f);
            TestEqual("Drone Barrier duration is exact", Barrier->DurationSeconds, 10.f);
            TestEqual("Drone Barrier targets self", Barrier->TargetScope,
                EDAFounderAbilityTargetScope::Self);
            TestEqual("Drone Barrier effect is concrete", Barrier->EffectId,
                FName(TEXT("effect.synara.drone_barrier.cover")));
        }
        if (Mark != nullptr)
        {
            TestEqual("Orchestration Mark cooldown is exact", Mark->CooldownSeconds, 12.f);
            TestEqual("Orchestration Mark duration is exact", Mark->DurationSeconds, 12.f);
            TestEqual("Orchestration Mark targets the view trace", Mark->TargetScope,
                EDAFounderAbilityTargetScope::ViewTarget);
            TestEqual("Orchestration Mark effect is concrete", Mark->EffectId,
                FName(TEXT("effect.synara.orchestration_mark.focus")));
        }
        if (Override != nullptr)
        {
            TestEqual("Coordinated Override cooldown is exact", Override->CooldownSeconds, 60.f);
            TestEqual("Coordinated Override duration is exact", Override->DurationSeconds, 15.f);
            TestEqual("Coordinated Override targets nearby allies", Override->TargetScope,
                EDAFounderAbilityTargetScope::AlliesInRadius);
            TestEqual("Coordinated Override range is exact", Override->RangeMeters, 15.f);
            TestEqual("Coordinated Override effect is concrete", Override->EffectId,
                FName(TEXT("effect.synara.coordinated_override.inspire")));
        }
    });

    It("activates authored Drone Barrier with charge cooldown cover and timed cleanup", [this]()
    {
        FDAFounderAbilityTestTarget Target = CreateFounderAbilityTestTarget();
        TestNotNull("Founder spawns in a real gameplay world", Target.Founder);
        if (Target.Founder == nullptr) return;
        UAbilitySystemComponent* AbilitySystem =
            Target.Founder->GetAbilitySystemComponent();
        TestNotNull("Founder exposes its real ability system", AbilitySystem);
        if (AbilitySystem == nullptr) return;

        TestEqual("Founder receives all four authored abilities",
            AbilitySystem->GetActivatableAbilities().Num(), 4);
        TestEqual("Founder starts with full Tactical Charge",
            AbilitySystem->GetNumericAttribute(
                UDACombatAttributeSet::GetTacticalChargeAttribute()), 100.f);
        TestTrue("Drone Barrier activates through GAS",
            AbilitySystem->TryActivateAbilityByClass(
                UDAFounderDroneBarrierAbility::StaticClass()));
        TestEqual("Drone Barrier consumes exactly twenty Tactical Charge",
            AbilitySystem->GetNumericAttribute(
                UDACombatAttributeSet::GetTacticalChargeAttribute()), 80.f);
        const FGameplayTag Shielded = FGameplayTag::RequestGameplayTag(
            FName(TEXT("Status.Shielded")));
        const FGameplayTag BarrierCooldown = FGameplayTag::RequestGameplayTag(
            FName(TEXT("Cooldown.Synara.DroneBarrier")));
        TestTrue("Drone Barrier applies its authored timed status",
            AbilitySystem->HasMatchingGameplayTag(Shielded));
        TestTrue("Drone Barrier applies its exact cooldown gate",
            AbilitySystem->HasMatchingGameplayTag(BarrierCooldown));

        UDACoverSubsystem* Cover = Target.World->GetSubsystem<UDACoverSubsystem>();
        const FName CoverId(*FString::Printf(TEXT("cover.synara.drone_barrier.%u"),
            Target.Founder->GetUniqueID()));
        TestNotNull("Drone Barrier creates concrete deployable cover",
            Cover != nullptr ? Cover->FindCoverSocket(CoverId) : nullptr);
        Target.World->Tick(LEVELTICK_All, 10.1f);
        TestFalse("Barrier status expires at its authored duration",
            AbilitySystem->HasMatchingGameplayTag(Shielded));
        TestTrue("The 18-second cooldown remains after the 10-second barrier",
            AbilitySystem->HasMatchingGameplayTag(BarrierCooldown));
        TestTrue("Barrier cover is removed with the timed effect",
            Cover == nullptr || Cover->FindCoverSocket(CoverId) == nullptr);
    });
}
