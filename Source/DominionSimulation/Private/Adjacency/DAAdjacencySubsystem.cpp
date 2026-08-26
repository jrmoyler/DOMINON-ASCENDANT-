#include "Adjacency/DAAdjacencySubsystem.h"

void FDAAdjacencySubsystem::AddRule(
    const FName SourceDefinitionId,
    const FName NeighborDefinitionId,
    const FName ModifierName,
    const float Amount)
{
    if (SourceDefinitionId.IsNone() || NeighborDefinitionId.IsNone() || ModifierName.IsNone())
    {
        return;
    }

    FAdjacencyRule Rule;
    Rule.SourceDefinitionId = SourceDefinitionId;
    Rule.NeighborDefinitionId = NeighborDefinitionId;
    Rule.ModifierName = ModifierName;
    Rule.Amount = Amount;
    Rules.Add(Rule);
}

void FDAAdjacencySubsystem::AddTypeRule(
    const EDACardType SourceCardType,
    const FName NeighborDefinitionId,
    const FName ModifierName,
    const float Amount)
{
    if (NeighborDefinitionId.IsNone() || ModifierName.IsNone() || !FMath::IsFinite(Amount)) return;
    if (FAdjacencyRule* Existing = Rules.FindByPredicate([SourceCardType, NeighborDefinitionId, ModifierName]
        (const FAdjacencyRule& Rule)
        {
            return Rule.bUsesSourceCardType && Rule.SourceCardType == SourceCardType
                && Rule.NeighborDefinitionId == NeighborDefinitionId
                && Rule.ModifierName == ModifierName;
        }))
    {
        Existing->Amount = Amount;
        return;
    }
    FAdjacencyRule Rule;
    Rule.SourceCardType = SourceCardType;
    Rule.bUsesSourceCardType = true;
    Rule.NeighborDefinitionId = NeighborDefinitionId;
    Rule.ModifierName = ModifierName;
    Rule.Amount = Amount;
    Rules.Add(Rule);
}

void FDAAdjacencySubsystem::RegisterAsset(
    const FDAWorldAssetId AssetId,
    const FName DefinitionId,
    const FIntPoint GridLocation,
    const EDACardType CardType,
    const FIntPoint Footprint)
{
    if (!AssetId.IsValid() || DefinitionId.IsNone()
        || Footprint.X <= 0 || Footprint.Y <= 0)
    {
        return;
    }

    if (const FAdjacencyAsset* ExistingAsset = Assets.Find(AssetId))
    {
        const FIntPoint PreviousLocation = ExistingAsset->GridLocation;
        RemoveFromBucket(AssetId, PreviousLocation);
        InvalidateNearby(PreviousLocation);
    }

    FAdjacencyAsset Asset;
    Asset.DefinitionId = DefinitionId;
    Asset.CardType = CardType;
    Asset.GridLocation = GridLocation;
    Asset.Footprint = Footprint;
    Assets.Add(AssetId, Asset);
    SpatialBuckets.FindOrAdd(ToBucket(GridLocation)).Add(AssetId);
    InvalidateNearby(GridLocation);
}

void FDAAdjacencySubsystem::RemoveAsset(const FDAWorldAssetId AssetId)
{
    const FAdjacencyAsset* ExistingAsset = Assets.Find(AssetId);
    if (ExistingAsset == nullptr)
    {
        return;
    }

    const FIntPoint PreviousLocation = ExistingAsset->GridLocation;
    RemoveFromBucket(AssetId, PreviousLocation);
    Assets.Remove(AssetId);
    CachedModifiers.Remove(AssetId);
    InvalidateNearby(PreviousLocation);
}

TArray<FDAAdjacencyModifier> FDAAdjacencySubsystem::RebuildForAsset(const FDAWorldAssetId AssetId)
{
    const FAdjacencyAsset* Asset = Assets.Find(AssetId);
    if (Asset == nullptr)
    {
        return TArray<FDAAdjacencyModifier>();
    }

    TArray<FDAAdjacencyModifier> Modifiers;
    const TSet<FDAWorldAssetId> NearbyAssetIds = FindNearbyAssetIds(Asset->GridLocation);
    for (const FDAWorldAssetId& NearbyAssetId : NearbyAssetIds)
    {
        if (NearbyAssetId == AssetId)
        {
            continue;
        }

        const FAdjacencyAsset* NearbyAsset = Assets.Find(NearbyAssetId);
        if (NearbyAsset == nullptr)
        {
            continue;
        }
        const int32 SeparationX = FMath::Max(0, FMath::Max(
            NearbyAsset->GridLocation.X
                - (Asset->GridLocation.X + Asset->Footprint.X - 1),
            Asset->GridLocation.X
                - (NearbyAsset->GridLocation.X + NearbyAsset->Footprint.X - 1)));
        const int32 SeparationY = FMath::Max(0, FMath::Max(
            NearbyAsset->GridLocation.Y
                - (Asset->GridLocation.Y + Asset->Footprint.Y - 1),
            Asset->GridLocation.Y
                - (NearbyAsset->GridLocation.Y + NearbyAsset->Footprint.Y - 1)));
        if (SeparationX > 1 || SeparationY > 1) continue;

        for (const FAdjacencyRule& Rule : Rules)
        {
            const bool bSourceMatches = Rule.bUsesSourceCardType
                ? Rule.SourceCardType == Asset->CardType
                : Rule.SourceDefinitionId == Asset->DefinitionId;
            if (bSourceMatches && Rule.NeighborDefinitionId == NearbyAsset->DefinitionId)
            {
                FDAAdjacencyModifier Modifier;
                Modifier.Name = Rule.ModifierName;
                Modifier.Amount = Rule.Amount;
                Modifiers.Add(Modifier);
            }
        }
    }

    CachedModifiers.Add(AssetId, Modifiers);
    return Modifiers;
}

TArray<FDAAdjacencyModifier> FDAAdjacencySubsystem::GetCachedModifiers(const FDAWorldAssetId AssetId) const
{
    const TArray<FDAAdjacencyModifier>* Modifiers = CachedModifiers.Find(AssetId);
    return Modifiers != nullptr ? *Modifiers : TArray<FDAAdjacencyModifier>();
}

const TSet<FDAWorldAssetId>& FDAAdjacencySubsystem::GetLastInvalidatedAssetIds() const
{
    return LastInvalidatedAssetIds;
}

FIntPoint FDAAdjacencySubsystem::ToBucket(const FIntPoint GridLocation) const
{
    return FIntPoint(
        FMath::FloorToInt(static_cast<float>(GridLocation.X) / SpatialBucketSize),
        FMath::FloorToInt(static_cast<float>(GridLocation.Y) / SpatialBucketSize));
}

TSet<FDAWorldAssetId> FDAAdjacencySubsystem::FindNearbyAssetIds(const FIntPoint GridLocation) const
{
    TSet<FDAWorldAssetId> NearbyAssetIds;
    const FIntPoint CenterBucket = ToBucket(GridLocation);
    for (int32 BucketY = CenterBucket.Y - 1; BucketY <= CenterBucket.Y + 1; ++BucketY)
    {
        for (int32 BucketX = CenterBucket.X - 1; BucketX <= CenterBucket.X + 1; ++BucketX)
        {
            const TSet<FDAWorldAssetId>* BucketAssetIds = SpatialBuckets.Find(FIntPoint(BucketX, BucketY));
            if (BucketAssetIds != nullptr)
            {
                for (const FDAWorldAssetId& BucketAssetId : *BucketAssetIds)
                {
                    NearbyAssetIds.Add(BucketAssetId);
                }
            }
        }
    }

    return NearbyAssetIds;
}

void FDAAdjacencySubsystem::InvalidateNearby(const FIntPoint GridLocation)
{
    LastInvalidatedAssetIds = FindNearbyAssetIds(GridLocation);
    for (const FDAWorldAssetId& InvalidatedAssetId : LastInvalidatedAssetIds)
    {
        CachedModifiers.Remove(InvalidatedAssetId);
    }
}

void FDAAdjacencySubsystem::RemoveFromBucket(const FDAWorldAssetId AssetId, const FIntPoint GridLocation)
{
    const FIntPoint Bucket = ToBucket(GridLocation);
    TSet<FDAWorldAssetId>* BucketAssetIds = SpatialBuckets.Find(Bucket);
    if (BucketAssetIds == nullptr)
    {
        return;
    }

    BucketAssetIds->Remove(AssetId);
    if (BucketAssetIds->IsEmpty())
    {
        SpatialBuckets.Remove(Bucket);
    }
}
