#pragma once

#include "Presentation/DAPresentationEventAdapterComponent.h"
#include "Subsystems/WorldSubsystem.h"

#include "DAPresentationBindingWorldSubsystem.generated.h"

class AActor;
class UActorComponent;
class UDAPresentationRegistrySubsystem;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDAPresentationPlanReadyNative,
    FName, const FDAPresentationPlaybackPlan&);

/**
 * Shipped lifecycle owner for read-only gameplay-to-presentation adapters.
 * It discovers map, dynamically spawned, and streamed authoritative components without
 * adding state to gameplay records, and tears adapters down when their source leaves the world.
 */
UCLASS()
class DOMINIONWORLD_API UDAPresentationBindingWorldSubsystem final
    : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    UFUNCTION(BlueprintPure, Category = "Presentation|Binding")
    int32 GetBoundComponentCount() const { return BoundAdapters.Num(); }

    UPROPERTY(BlueprintAssignable, Category = "Presentation|Events")
    FDAPresentationPlanReady OnPresentationPlanReady;

    FDAPresentationPlanReadyNative OnPresentationPlanReadyNative;

private:
    void RefreshBindings();
    void BindActor(AActor& Actor);
    bool BindComponent(UActorComponent& Component);
    void PruneBindings();
    void HandleActorSpawned(AActor* Actor);

    UFUNCTION()
    void HandlePresentationPlanReady(FName Channel, FDAPresentationPlaybackPlan Plan);

    UPROPERTY(Transient)
    TWeakObjectPtr<UDAPresentationRegistrySubsystem> Registry;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UDAPresentationEventAdapterComponent>> OwnedAdapters;

    TMap<TWeakObjectPtr<UActorComponent>, TWeakObjectPtr<UDAPresentationEventAdapterComponent>>
        BoundAdapters;
    FDelegateHandle ActorSpawnedHandle;
};
