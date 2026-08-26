#pragma once

#include "City/DAPlacementTypes.h"
#include "Content/DAContentTypes.h"
#include "CoreMinimal.h"

struct DOMINIONSIMULATION_API FDAAdjacencyModifier
{
    FName Name;
    float Amount = 0.f;
};

// Placement-time spatial index and modifier cache. This is intentionally a plain simulation
// service: callers rebuild an affected asset after state changes; no frame update exists.
class DOMINIONSIMULATION_API FDAAdjacencySubsystem
{
public:
    static constexpr int32 SpatialBucketSize = 4;

    void AddRule(FName SourceDefinitionId, FName NeighborDefinitionId, FName ModifierName, float Amount);
    void AddTypeRule(EDACardType SourceCardType, FName NeighborDefinitionId,
        FName ModifierName, float Amount);
    void RegisterAsset(FDAWorldAssetId AssetId, FName DefinitionId, FIntPoint GridLocation,
        EDACardType CardType = EDACardType::Land,
        FIntPoint Footprint = FIntPoint(1, 1));
    void RemoveAsset(FDAWorldAssetId AssetId);

    TArray<FDAAdjacencyModifier> RebuildForAsset(FDAWorldAssetId AssetId);
    TArray<FDAAdjacencyModifier> GetCachedModifiers(FDAWorldAssetId AssetId) const;
    const TSet<FDAWorldAssetId>& GetLastInvalidatedAssetIds() const;

private:
    struct FAdjacencyAsset
    {
        FName DefinitionId;
        EDACardType CardType = EDACardType::Land;
        FIntPoint GridLocation = FIntPoint::ZeroValue;
        FIntPoint Footprint = FIntPoint(1, 1);
    };

    struct FAdjacencyRule
    {
        FName SourceDefinitionId;
        EDACardType SourceCardType = EDACardType::Land;
        bool bUsesSourceCardType = false;
        FName NeighborDefinitionId;
        FName ModifierName;
        float Amount = 0.f;
    };

    FIntPoint ToBucket(FIntPoint GridLocation) const;
    TSet<FDAWorldAssetId> FindNearbyAssetIds(FIntPoint GridLocation) const;
    void InvalidateNearby(FIntPoint GridLocation);
    void RemoveFromBucket(FDAWorldAssetId AssetId, FIntPoint GridLocation);

    TMap<FDAWorldAssetId, FAdjacencyAsset> Assets;
    TMap<FIntPoint, TSet<FDAWorldAssetId>> SpatialBuckets;
    TArray<FAdjacencyRule> Rules;
    TMap<FDAWorldAssetId, TArray<FDAAdjacencyModifier>> CachedModifiers;
    TSet<FDAWorldAssetId> LastInvalidatedAssetIds;
};
