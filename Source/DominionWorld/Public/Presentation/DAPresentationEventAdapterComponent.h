#pragma once

#include "Capture/DACaptureComponent.h"
#include "Components/ActorComponent.h"
#include "Construction/DAConstructionComponent.h"
#include "Damage/DAStructuralDamageComponent.h"
#include "Presentation/DAPresentationContent.h"

#include "DAPresentationEventAdapterComponent.generated.h"

class UDAPresentationRegistrySubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDAPresentationPlanReady,
    FName, Channel, FDAPresentationPlaybackPlan, Plan);

/**
 * Read-only bridge from committed gameplay component broadcasts to presentation plans.
 * It neither delays nor mutates authoritative construction, damage, or capture records.
 */
UCLASS(ClassGroup = (Dominion), BlueprintType, meta = (BlueprintSpawnableComponent))
class DOMINIONWORLD_API UDAPresentationEventAdapterComponent final : public UActorComponent
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "Presentation|Binding")
    bool Initialize(UDAPresentationRegistrySubsystem* InRegistry);

    UFUNCTION(BlueprintCallable, Category = "Presentation|Binding")
    bool BindConstruction(UDAConstructionComponent* InComponent, FName InFaction);

    UFUNCTION(BlueprintCallable, Category = "Presentation|Binding")
    bool BindDamage(UDAStructuralDamageComponent* InComponent, FName InFaction);

    UFUNCTION(BlueprintCallable, Category = "Presentation|Binding")
    bool BindCapture(UDACaptureComponent* InComponent);

    UFUNCTION(BlueprintCallable, Category = "Presentation|Binding")
    void UnbindAll();

    const FDAPresentationPlaybackPlan& GetLastConstructionPlan() const
    { return LastConstructionPlan; }

    const FDAPresentationPlaybackPlan& GetLastDamagePlan() const
    { return LastDamagePlan; }

    const FDAPresentationPlaybackPlan& GetLastCapturePlan() const
    { return LastCapturePlan; }

    UPROPERTY(BlueprintAssignable, Category = "Presentation|Events")
    FDAPresentationPlanReady OnPresentationPlanReady;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleConstructionStageChanged(EDAConstructionState CommittedState);

    UFUNCTION()
    void HandleDamageStateChanged(EDAConstructionState CommittedState);

    UFUNCTION()
    void HandleCaptureStateChanged(FDACapturePresentationSnapshot CommittedSnapshot);

    UPROPERTY(Transient) TWeakObjectPtr<UDAPresentationRegistrySubsystem> Registry;
    UPROPERTY(Transient) TWeakObjectPtr<UDAConstructionComponent> ConstructionComponent;
    UPROPERTY(Transient) TWeakObjectPtr<UDAStructuralDamageComponent> DamageComponent;
    UPROPERTY(Transient) TWeakObjectPtr<UDACaptureComponent> CaptureComponent;
    UPROPERTY(Transient) FName ConstructionFaction;
    UPROPERTY(Transient) FName DamageFaction;
    UPROPERTY(Transient) FDAPresentationPlaybackPlan LastConstructionPlan;
    UPROPERTY(Transient) FDAPresentationPlaybackPlan LastDamagePlan;
    UPROPERTY(Transient) FDAPresentationPlaybackPlan LastCapturePlan;
};
