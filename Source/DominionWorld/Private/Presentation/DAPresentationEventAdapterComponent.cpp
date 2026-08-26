#include "Presentation/DAPresentationEventAdapterComponent.h"

#include "Presentation/DAPresentationRegistrySubsystem.h"

bool UDAPresentationEventAdapterComponent::Initialize(
    UDAPresentationRegistrySubsystem* InRegistry)
{
    if (InRegistry == nullptr || !InRegistry->IsReady())
    {
        return false;
    }
    Registry = InRegistry;
    return true;
}

bool UDAPresentationEventAdapterComponent::BindConstruction(
    UDAConstructionComponent* InComponent, const FName InFaction)
{
    if (!Registry.IsValid() || InComponent == nullptr || InFaction.IsNone()) return false;
    if (UDAConstructionComponent* Previous = ConstructionComponent.Get())
        Previous->OnStageChanged.RemoveDynamic(
            this, &UDAPresentationEventAdapterComponent::HandleConstructionStageChanged);
    ConstructionComponent = InComponent;
    ConstructionFaction = InFaction;
    InComponent->OnStageChanged.AddUniqueDynamic(
        this, &UDAPresentationEventAdapterComponent::HandleConstructionStageChanged);
    return true;
}

bool UDAPresentationEventAdapterComponent::BindDamage(
    UDAStructuralDamageComponent* InComponent, const FName InFaction)
{
    if (!Registry.IsValid() || InComponent == nullptr || InFaction.IsNone()) return false;
    FDAWorldAssetRecord CommittedRecord;
    if (!InComponent->GetPresentationRecord(CommittedRecord)) return false;
    if (UDAStructuralDamageComponent* Previous = DamageComponent.Get())
        Previous->OnDamageStateChanged.RemoveDynamic(
            this, &UDAPresentationEventAdapterComponent::HandleDamageStateChanged);
    DamageComponent = InComponent;
    DamageFaction = InFaction;
    InComponent->OnDamageStateChanged.AddUniqueDynamic(
        this, &UDAPresentationEventAdapterComponent::HandleDamageStateChanged);
    return true;
}

bool UDAPresentationEventAdapterComponent::BindCapture(UDACaptureComponent* InComponent)
{
    if (!Registry.IsValid() || InComponent == nullptr) return false;
    if (UDACaptureComponent* Previous = CaptureComponent.Get())
        Previous->OnCaptureStateChanged.RemoveDynamic(
            this, &UDAPresentationEventAdapterComponent::HandleCaptureStateChanged);
    CaptureComponent = InComponent;
    InComponent->OnCaptureStateChanged.AddUniqueDynamic(
        this, &UDAPresentationEventAdapterComponent::HandleCaptureStateChanged);
    return true;
}

void UDAPresentationEventAdapterComponent::UnbindAll()
{
    if (UDAConstructionComponent* Component = ConstructionComponent.Get())
        Component->OnStageChanged.RemoveDynamic(
            this, &UDAPresentationEventAdapterComponent::HandleConstructionStageChanged);
    if (UDAStructuralDamageComponent* Component = DamageComponent.Get())
        Component->OnDamageStateChanged.RemoveDynamic(
            this, &UDAPresentationEventAdapterComponent::HandleDamageStateChanged);
    if (UDACaptureComponent* Component = CaptureComponent.Get())
        Component->OnCaptureStateChanged.RemoveDynamic(
            this, &UDAPresentationEventAdapterComponent::HandleCaptureStateChanged);
    ConstructionComponent.Reset();
    DamageComponent.Reset();
    CaptureComponent.Reset();
}

void UDAPresentationEventAdapterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindAll();
    Super::EndPlay(EndPlayReason);
}

void UDAPresentationEventAdapterComponent::HandleConstructionStageChanged(
    const EDAConstructionState CommittedState)
{
    UDAPresentationRegistrySubsystem* ReadyRegistry = Registry.Get();
    if (ReadyRegistry != nullptr && ReadyRegistry->BuildConstructionPlan(
            ConstructionFaction, CommittedState, LastConstructionPlan))
        OnPresentationPlanReady.Broadcast(TEXT("construction"), LastConstructionPlan);
}

void UDAPresentationEventAdapterComponent::HandleDamageStateChanged(
    const EDAConstructionState CommittedState)
{
    UDAPresentationRegistrySubsystem* ReadyRegistry = Registry.Get();
    UDAStructuralDamageComponent* Component = DamageComponent.Get();
    FDAWorldAssetRecord CommittedRecord;
    if (ReadyRegistry != nullptr && Component != nullptr
        && Component->GetPresentationRecord(CommittedRecord)
        && CommittedRecord.ConstructionState == CommittedState
        && ReadyRegistry->BuildDamagePlan(DamageFaction, CommittedRecord, LastDamagePlan))
        OnPresentationPlanReady.Broadcast(TEXT("damage"), LastDamagePlan);
}

void UDAPresentationEventAdapterComponent::HandleCaptureStateChanged(
    const FDACapturePresentationSnapshot CommittedSnapshot)
{
    UDAPresentationRegistrySubsystem* ReadyRegistry = Registry.Get();
    if (ReadyRegistry != nullptr && ReadyRegistry->BuildCaptureSnapshotPlan(
            CommittedSnapshot, LastCapturePlan))
        OnPresentationPlanReady.Broadcast(TEXT("capture"), LastCapturePlan);
}
