#pragma once

#include "City/DAPlacementTypes.h"
#include "CoreMinimal.h"

// Logical road connectivity for simulation clients. Topology changes are explicit; access checks
// inspect only an asset's cached adjacency list.
class DOMINIONSIMULATION_API FDARoadGraph
{
public:
    void AddNode(const FDAWorldAssetId& AssetId)
    {
        if (AssetId.IsValid())
        {
            Nodes.FindOrAdd(AssetId);
        }
    }

    bool ConnectNodes(const FDAWorldAssetId& FirstAssetId, const FDAWorldAssetId& SecondAssetId)
    {
        if (!FirstAssetId.IsValid() || !SecondAssetId.IsValid() || FirstAssetId == SecondAssetId || !Nodes.Contains(FirstAssetId) || !Nodes.Contains(SecondAssetId))
        {
            return false;
        }

        Nodes.FindChecked(FirstAssetId).Neighbors.Add(SecondAssetId);
        Nodes.FindChecked(SecondAssetId).Neighbors.Add(FirstAssetId);
        return true;
    }

    bool SetNodeEnabled(const FDAWorldAssetId& AssetId, const bool bEnabled)
    {
        FNode* Node = Nodes.Find(AssetId);
        if (Node == nullptr)
        {
            return false;
        }

        Node->bEnabled = bEnabled;
        return true;
    }

    bool HasRoadAccess(FDAWorldAssetId AssetId) const
    {
        const FNode* Node = Nodes.Find(AssetId);
        if (Node == nullptr || !Node->bEnabled)
        {
            return false;
        }

        for (const FDAWorldAssetId& NeighborAssetId : Node->Neighbors)
        {
            const FNode* Neighbor = Nodes.Find(NeighborAssetId);
            if (Neighbor != nullptr && Neighbor->bEnabled)
            {
                return true;
            }
        }

        return false;
    }

private:
    struct FNode
    {
        TSet<FDAWorldAssetId> Neighbors;
        bool bEnabled = true;
    };

    TMap<FDAWorldAssetId, FNode> Nodes;
};
