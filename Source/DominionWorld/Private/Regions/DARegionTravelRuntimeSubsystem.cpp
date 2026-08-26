#include "Regions/DARegionTravelRuntimeSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Subsystems/SubsystemCollection.h"
#include "World/DAWorldAsset.h"

namespace
{
    class FDAProductionRegionRuntimeLoader final : public IDARegionRuntimeLoader
    {
    public:
        virtual bool LoadRegion(UWorld* World, const FName RequestId,
            const FDARegionState& Region,
            TWeakObjectPtr<ULevelStreaming>& OutStreamingLevel) override
        {
            OutStreamingLevel.Reset();
            const FString PackageName = Region.MapAssetPath.GetLongPackageName();
            if (World == nullptr || RequestId.IsNone() || PackageName.IsEmpty()
                || !FPackageName::DoesPackageExist(PackageName))
            {
                return false;
            }
            bool bLoadAccepted = false;
            TSoftObjectPtr<UWorld> MapAsset(Region.MapAssetPath);
            ULevelStreamingDynamic* Streaming = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
                World, MapAsset, FVector::ZeroVector, FRotator::ZeroRotator, bLoadAccepted,
                FString(), ULevelStreamingDynamic::StaticClass(), false);
            if (!bLoadAccepted || Streaming == nullptr)
            {
                return false;
            }
            Streaming->SetShouldBeLoaded(true);
            Streaming->SetShouldBeVisible(true);
            Streaming->SetShouldBlockOnLoad(true);
            World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
            if (Streaming->GetLoadedLevel() == nullptr)
            {
                Streaming->SetShouldBeLoaded(false);
                Streaming->SetIsRequestingUnloadAndRemoval(true);
                World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
                return false;
            }
            OutStreamingLevel = Streaming;
            return true;
        }

        virtual bool UnloadRegion(UWorld* World,
            const TWeakObjectPtr<ULevelStreaming> StreamingLevel) override
        {
            ULevelStreaming* Streaming = StreamingLevel.Get();
            if (Streaming == nullptr)
            {
                return true;
            }
            if (World == nullptr)
            {
                return false;
            }
            Streaming->SetShouldBeVisible(false);
            Streaming->SetShouldBeLoaded(false);
            Streaming->SetIsRequestingUnloadAndRemoval(true);
            World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
            return Streaming->GetLoadedLevel() == nullptr;
        }

        virtual bool SpawnLocalActor(UWorld* World, const FDARegionState& Region,
            const FDARegionActorState& ActorState,
            TWeakObjectPtr<AActor>& OutActor) override
        {
            OutActor.Reset();
            if (World == nullptr || ActorState.ActorId.IsNone()
                || ActorState.DefinitionId.IsNone())
            {
                return false;
            }
            AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), ActorState.Transform);
            if (Actor == nullptr)
            {
                return false;
            }
            Actor->Tags.AddUnique(FName(*(TEXT("region:") + Region.RegionId.ToString())));
            Actor->Tags.AddUnique(FName(*(TEXT("actor:") + ActorState.ActorId.ToString())));
            Actor->Tags.AddUnique(FName(*(TEXT("definition:") + ActorState.DefinitionId.ToString())));
            OutActor = Actor;
            return true;
        }

        virtual bool SpawnWorldAsset(UWorld* World, UDAWorldStateSubsystem& Authority,
            const FDARegionState& Region, const FDAWorldAssetRecord& Asset,
            TWeakObjectPtr<AActor>& OutActor) override
        {
            OutActor.Reset();
            if (World == nullptr)
            {
                return false;
            }
            const FVector Location(
                static_cast<double>(Asset.GridOrigin.X) * 400.0,
                static_cast<double>(Asset.GridOrigin.Y) * 400.0,
                0.0);
            const FRotator Rotation(0.0, static_cast<double>(Asset.Rotation) * 90.0, 0.0);
            ADAWorldAsset* Actor = World->SpawnActor<ADAWorldAsset>(
                ADAWorldAsset::StaticClass(), Location, Rotation);
            if (Actor == nullptr
                || !Actor->InitializeFromCampaignAuthority(Authority, Asset.WorldAssetId))
            {
                if (Actor != nullptr) Actor->Destroy();
                return false;
            }
            Actor->Tags.AddUnique(FName(*(TEXT("region:") + Region.RegionId.ToString())));
            OutActor = Actor;
            return true;
        }
    };
}

void UDARegionTravelRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UDAWorldStateSubsystem>();
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        WorldStateSubsystem = GameInstance->GetSubsystem<UDAWorldStateSubsystem>();
    }
    RuntimeLoader = MakeShared<FDAProductionRegionRuntimeLoader>();
    FString Error;
    if (!WorldStateSubsystem.IsValid()
        || !WorldStateSubsystem->RegisterTravelRuntime(this, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Region travel runtime registration failed: %s"), *Error);
    }
}

void UDARegionTravelRuntimeSubsystem::Deinitialize()
{
    if (WorldStateSubsystem.IsValid())
    {
        WorldStateSubsystem->UnregisterTravelRuntime(this);
    }
    TArray<FName> Regions;
    SpawnedRegionActors.GetKeys(Regions);
    for (const FName RegionId : Regions)
    {
        UnloadRegionRepresentation(RegionId);
    }
    SpawnedRegionActors.Reset();
    RuntimeLoader.Reset();
    LoadedStreamingLevel.Reset();
    LoadedRequestId = NAME_None;
    LoadedRegionId = NAME_None;
    WorldStateSubsystem.Reset();
    Super::Deinitialize();
}

bool UDARegionTravelRuntimeSubsystem::SnapshotPersistentDelta(
    const FDARegionState& SourceRegion, FDARegionPersistentDelta& OutDelta)
{
    FString Error;
    if (SourceRegion.RegionId.IsNone() || !SourceRegion.PersistentDelta.Validate(Error)
        || SourceRegion.PersistentDelta.Revision == MAX_int64)
    {
        return false;
    }
    // WorldAsset gameplay state already commits directly to the campaign aggregate. The
    // regional delta owns only region-local actors/tags and advances once per handoff.
    OutDelta = SourceRegion.PersistentDelta;
    ++OutDelta.Revision;
    return OutDelta.Validate(Error);
}

bool UDARegionTravelRuntimeSubsystem::UnloadRegionRepresentation(const FName RegionId)
{
    if (RegionId.IsNone())
    {
        return false;
    }
    if (TArray<TWeakObjectPtr<AActor>>* Actors = SpawnedRegionActors.Find(RegionId))
    {
        for (const TWeakObjectPtr<AActor>& ActorRef : *Actors)
        {
            AActor* Actor = ActorRef.Get();
            if (IsValid(Actor))
            {
                Actor->Destroy();
            }
        }
        SpawnedRegionActors.Remove(RegionId);
    }
    if (LoadedRegionId == RegionId)
    {
        if (!RuntimeLoader.IsValid()
            || !RuntimeLoader->UnloadRegion(GetWorld(), LoadedStreamingLevel))
        {
            return false;
        }
        LoadedStreamingLevel.Reset();
        LoadedRequestId = NAME_None;
        LoadedRegionId = NAME_None;
    }
    return true;
}

bool UDARegionTravelRuntimeSubsystem::EnsureRegionLoaded(
    const FName RequestId, const FName RegionId)
{
    const FDACampaignSnapshot* Authority = WorldStateSubsystem.IsValid()
        ? &WorldStateSubsystem->GetPersistentCampaign() : nullptr;
    const FDARegionState* Region = Authority == nullptr
        ? nullptr : Authority->WorldState.FindRegion(RegionId);
    TWeakObjectPtr<ULevelStreaming> StreamingLevel;
    if (!RequestId.IsNone() && Region != nullptr && !Region->MapAssetPath.IsNull()
        && RuntimeLoader.IsValid()
        && RuntimeLoader->LoadRegion(GetWorld(), RequestId, *Region, StreamingLevel))
    {
        LoadedRequestId = RequestId;
        LoadedRegionId = RegionId;
        LoadedStreamingLevel = StreamingLevel;
        return true;
    }
    return false;
}

bool UDARegionTravelRuntimeSubsystem::EnsureRegionReconstructed(
    const FName RequestId, const FDARegionState& Region,
    const FDACampaignSnapshot& Campaign)
{
    if (!WorldStateSubsystem.IsValid()
        || RequestId.IsNone() || RequestId != LoadedRequestId
        || Region.RegionId.IsNone() || Region.RegionId != LoadedRegionId)
    {
        return false;
    }
    const FDACampaignSnapshot& Authority = WorldStateSubsystem->GetPersistentCampaign();
    const FDARegionState* CanonicalRegion = Authority.WorldState.FindRegion(Region.RegionId);
    FString Error;
    if (CanonicalRegion == nullptr
        || CanonicalRegion->PersistentDelta.Revision != Region.PersistentDelta.Revision
        || !Campaign.Validate(Error) || !Authority.Validate(Error))
    {
        return false;
    }

    // A retry reconstructs from scratch, preventing duplicate runtime owners.
    if (TArray<TWeakObjectPtr<AActor>>* Existing = SpawnedRegionActors.Find(Region.RegionId))
    {
        for (const TWeakObjectPtr<AActor>& ActorRef : *Existing)
        {
            AActor* Actor = ActorRef.Get();
            if (IsValid(Actor)) Actor->Destroy();
        }
        SpawnedRegionActors.Remove(Region.RegionId);
    }

    TArray<TWeakObjectPtr<AActor>> Spawned;
    const auto RollBackSpawned = [&Spawned]()
    {
        for (const TWeakObjectPtr<AActor>& SpawnedRef : Spawned)
        {
            AActor* SpawnedActor = SpawnedRef.Get();
            if (IsValid(SpawnedActor)) SpawnedActor->Destroy();
        }
    };
    for (const FDARegionActorState& ActorState : CanonicalRegion->PersistentDelta.LocalActors)
    {
        TWeakObjectPtr<AActor> Actor;
        if (!RuntimeLoader.IsValid()
            || !RuntimeLoader->SpawnLocalActor(GetWorld(), *CanonicalRegion, ActorState, Actor))
        {
            RollBackSpawned();
            return false;
        }
        Spawned.Add(Actor);
    }
    for (const FDAWorldAssetRecord& Asset : Authority.WorldAssets)
    {
        if (!AssetBelongsToRegion(Asset, *CanonicalRegion))
        {
            continue;
        }
        TWeakObjectPtr<AActor> Actor;
        if (!RuntimeLoader.IsValid()
            || !RuntimeLoader->SpawnWorldAsset(GetWorld(), *WorldStateSubsystem,
                *CanonicalRegion, Asset, Actor))
        {
            RollBackSpawned();
            return false;
        }
        Spawned.Add(Actor);
    }
    SpawnedRegionActors.Add(Region.RegionId, MoveTemp(Spawned));
    return true;
}

void UDARegionTravelRuntimeSubsystem::SetRuntimeLoader(
    TSharedRef<IDARegionRuntimeLoader> InRuntimeLoader)
{
    RuntimeLoader = MoveTemp(InRuntimeLoader);
}

bool UDARegionTravelRuntimeSubsystem::AssetBelongsToRegion(
    const FDAWorldAssetRecord& Asset, const FDARegionState& Region)
{
    if (Asset.CityId == Region.RegionId || Region.SettlementIds.Contains(Asset.CityId))
    {
        return true;
    }
    return Region.RegionId == TEXT("region.synara_frontier")
        && Asset.CityId == TEXT("player_capital");
}
