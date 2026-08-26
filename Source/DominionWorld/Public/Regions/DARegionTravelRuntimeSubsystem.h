#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DARegionTravelRuntimeSubsystem.generated.h"

class ADAWorldAsset;
class ULevelStreaming;

/** Injectable platform boundary; production uses dynamic level streaming and actor spawning. */
class DOMINIONWORLD_API IDARegionRuntimeLoader
{
public:
    virtual ~IDARegionRuntimeLoader() = default;
    virtual bool LoadRegion(UWorld* World, FName RequestId, const FDARegionState& Region,
        TWeakObjectPtr<ULevelStreaming>& OutStreamingLevel) = 0;
    virtual bool UnloadRegion(UWorld* World, TWeakObjectPtr<ULevelStreaming> StreamingLevel) = 0;
    virtual bool SpawnLocalActor(UWorld* World, const FDARegionState& Region,
        const FDARegionActorState& ActorState, TWeakObjectPtr<AActor>& OutActor) = 0;
    virtual bool SpawnWorldAsset(UWorld* World, UDAWorldStateSubsystem& Authority,
        const FDARegionState& Region, const FDAWorldAssetRecord& Asset,
        TWeakObjectPtr<AActor>& OutActor) = 0;
};

/** Production streaming bridge. Runtime actors are disposable projections of the campaign owner. */
UCLASS()
class DOMINIONWORLD_API UDARegionTravelRuntimeSubsystem final
    : public UGameInstanceSubsystem, public IDARegionTravelRuntimeService
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual bool SnapshotPersistentDelta(const FDARegionState& SourceRegion,
        FDARegionPersistentDelta& OutDelta) override;
    virtual bool UnloadRegionRepresentation(FName RegionId) override;
    virtual bool EnsureRegionLoaded(FName RequestId, FName RegionId) override;
    virtual bool EnsureRegionReconstructed(FName RequestId, const FDARegionState& Region,
        const FDACampaignSnapshot& Campaign) override;

    /** Replaces only the runtime platform adapter; campaign ownership remains unchanged. */
    void SetRuntimeLoader(TSharedRef<IDARegionRuntimeLoader> InRuntimeLoader);

private:
    static bool AssetBelongsToRegion(const FDAWorldAssetRecord& Asset,
        const FDARegionState& Region);

    UPROPERTY(Transient)
    TWeakObjectPtr<UDAWorldStateSubsystem> WorldStateSubsystem;

    TMap<FName, TArray<TWeakObjectPtr<AActor>>> SpawnedRegionActors;
    TSharedPtr<IDARegionRuntimeLoader> RuntimeLoader;
    TWeakObjectPtr<ULevelStreaming> LoadedStreamingLevel;

    FName LoadedRequestId;
    FName LoadedRegionId;
};
