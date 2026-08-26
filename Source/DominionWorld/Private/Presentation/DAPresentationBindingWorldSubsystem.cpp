#include "Presentation/DAPresentationBindingWorldSubsystem.h"

#include "Capture/DACaptureComponent.h"
#include "Construction/DAConstructionComponent.h"
#include "Damage/DAStructuralDamageComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Presentation/DAPresentationRegistrySubsystem.h"

bool UDAPresentationBindingWorldSubsystem::DoesSupportWorldType(
    const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE
        || WorldType == EWorldType::GamePreview;
}

void UDAPresentationBindingWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
            Registry = GameInstance->GetSubsystem<UDAPresentationRegistrySubsystem>();
        ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateUObject(
                this, &UDAPresentationBindingWorldSubsystem::HandleActorSpawned));
        RefreshBindings();
    }
}

void UDAPresentationBindingWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    RefreshBindings();
}

void UDAPresentationBindingWorldSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld(); World != nullptr && ActorSpawnedHandle.IsValid())
        World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
    ActorSpawnedHandle.Reset();
    for (UDAPresentationEventAdapterComponent* Adapter : OwnedAdapters)
    {
        if (IsValid(Adapter))
        {
            Adapter->UnbindAll();
            Adapter->DestroyComponent();
        }
    }
    BoundAdapters.Reset();
    OwnedAdapters.Reset();
    Registry.Reset();
    Super::Deinitialize();
}

void UDAPresentationBindingWorldSubsystem::Tick(const float DeltaTime)
{
    static_cast<void>(DeltaTime);
    RefreshBindings();
}

TStatId UDAPresentationBindingWorldSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDAPresentationBindingWorldSubsystem,
        STATGROUP_Tickables);
}

void UDAPresentationBindingWorldSubsystem::RefreshBindings()
{
    UWorld* World = GetWorld();
    if (World == nullptr) return;
    if (!Registry.IsValid())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
            Registry = GameInstance->GetSubsystem<UDAPresentationRegistrySubsystem>();
    }
    PruneBindings();
    if (!Registry.IsValid() || !Registry->IsReady()) return;
    for (TActorIterator<AActor> It(World); It; ++It)
        BindActor(**It);
}

void UDAPresentationBindingWorldSubsystem::BindActor(AActor& Actor)
{
    if (Actor.GetWorld() != GetWorld() || Actor.IsActorBeingDestroyed()) return;
    TInlineComponentArray<UDAConstructionComponent*> ConstructionComponents;
    Actor.GetComponents(ConstructionComponents);
    for (UDAConstructionComponent* Component : ConstructionComponents)
        if (IsValid(Component)) BindComponent(*Component);

    TInlineComponentArray<UDAStructuralDamageComponent*> DamageComponents;
    Actor.GetComponents(DamageComponents);
    for (UDAStructuralDamageComponent* Component : DamageComponents)
        if (IsValid(Component)) BindComponent(*Component);

    TInlineComponentArray<UDACaptureComponent*> CaptureComponents;
    Actor.GetComponents(CaptureComponents);
    for (UDACaptureComponent* Component : CaptureComponents)
        if (IsValid(Component)) BindComponent(*Component);
}

bool UDAPresentationBindingWorldSubsystem::BindComponent(UActorComponent& Component)
{
    if (!Component.IsRegistered() || Component.GetWorld() != GetWorld()
        || BoundAdapters.Contains(&Component) || !Registry.IsValid())
        return false;

    AActor* Owner = Component.GetOwner();
    if (!IsValid(Owner) || Owner->IsActorBeingDestroyed()) return false;
    UDAPresentationEventAdapterComponent* Adapter =
        NewObject<UDAPresentationEventAdapterComponent>(Owner);
    Owner->AddInstanceComponent(Adapter);
    Adapter->RegisterComponent();
    if (!Adapter->Initialize(Registry.Get()))
    {
        Adapter->DestroyComponent();
        return false;
    }

    bool bBound = false;
    if (UDAConstructionComponent* Construction = Cast<UDAConstructionComponent>(&Component))
    {
        FDAWorldAssetRecord Record;
        bBound = Construction->GetPresentationRecord(Record)
            && Adapter->BindConstruction(Construction, Record.OwnerCivilizationId);
    }
    else if (UDAStructuralDamageComponent* Damage =
        Cast<UDAStructuralDamageComponent>(&Component))
    {
        FDAWorldAssetRecord Record;
        bBound = Damage->GetPresentationRecord(Record)
            && Adapter->BindDamage(Damage, Record.OwnerCivilizationId);
    }
    else if (UDACaptureComponent* Capture = Cast<UDACaptureComponent>(&Component))
    {
        const FDACapturePresentationSnapshot Snapshot = Capture->GetPresentationSnapshot();
        bBound = (!Snapshot.OriginalOwnerCivilizationId.IsNone()
                || !Snapshot.CurrentOwnerCivilizationId.IsNone())
            && Adapter->BindCapture(Capture);
    }
    if (!bBound)
    {
        Adapter->DestroyComponent();
        return false;
    }

    Adapter->OnPresentationPlanReady.AddUniqueDynamic(
        this, &UDAPresentationBindingWorldSubsystem::HandlePresentationPlanReady);
    OwnedAdapters.Add(Adapter);
    BoundAdapters.Add(&Component, Adapter);
    return true;
}

void UDAPresentationBindingWorldSubsystem::PruneBindings()
{
    UWorld* World = GetWorld();
    for (auto It = BoundAdapters.CreateIterator(); It; ++It)
    {
        UActorComponent* Component = It.Key().Get();
        UDAPresentationEventAdapterComponent* Adapter = It.Value().Get();
        const bool bLive = IsValid(Component) && Component->IsRegistered()
            && Component->GetWorld() == World && IsValid(Component->GetOwner())
            && !Component->GetOwner()->IsActorBeingDestroyed() && IsValid(Adapter);
        if (!bLive)
        {
            if (IsValid(Adapter))
            {
                Adapter->UnbindAll();
                Adapter->DestroyComponent();
            }
            OwnedAdapters.RemoveSingleSwap(Adapter);
            It.RemoveCurrent();
        }
    }
}

void UDAPresentationBindingWorldSubsystem::HandleActorSpawned(AActor* Actor)
{
    if (IsValid(Actor)) BindActor(*Actor);
}

void UDAPresentationBindingWorldSubsystem::HandlePresentationPlanReady(
    const FName Channel, const FDAPresentationPlaybackPlan Plan)
{
    OnPresentationPlanReady.Broadcast(Channel, Plan);
    OnPresentationPlanReadyNative.Broadcast(Channel, Plan);
}
