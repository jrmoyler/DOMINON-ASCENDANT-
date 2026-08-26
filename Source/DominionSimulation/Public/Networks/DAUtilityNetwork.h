#pragma once

#include "City/DAPlacementTypes.h"
#include "Economy/DAEconomyTypes.h"
#include "CoreMinimal.h"

enum class EDAUtilityType : uint8
{
    Power,
    Water,
    Data,
    Logistics
};

struct DOMINIONSIMULATION_API FDAUtilityResolution
{
    EDAUtilityState State = EDAUtilityState::Offline;
    float RequestedCapacity = 0.f;
    float SuppliedCapacity = 0.f;
};

// An event-driven utility graph. Callers mutate topology or node state and request a resolution;
// it has no Tick and never scans unrelated utility components.
class DOMINIONSIMULATION_API FDAUtilityNetwork
{
public:
    void RegisterNode(EDAUtilityType UtilityType, FDAWorldAssetId AssetId, float GenerationCapacity, float DemandCapacity);
    bool ConnectNodes(EDAUtilityType UtilityType, FDAWorldAssetId FirstAssetId, FDAWorldAssetId SecondAssetId);
    bool SetNodeEnabled(EDAUtilityType UtilityType, FDAWorldAssetId AssetId, bool bEnabled);

    FDAUtilityResolution ResolveUtility(EDAUtilityType UtilityType, FDAWorldAssetId AssetId) const;

private:
    struct FDAUtilityNode
    {
        float GenerationCapacity = 0.f;
        float DemandCapacity = 0.f;
        int32 RegistrationOrder = 0;
        bool bEnabled = true;
    };

    struct FDAUtilityGraph
    {
        TMap<FDAWorldAssetId, FDAUtilityNode> Nodes;
        TMap<FDAWorldAssetId, TSet<FDAWorldAssetId>> Neighbors;
        int32 NextRegistrationOrder = 0;
    };

    FDAUtilityGraph& GetOrCreateGraph(EDAUtilityType UtilityType);
    const FDAUtilityGraph* FindGraph(EDAUtilityType UtilityType) const;
    TArray<FDAWorldAssetId> FindConnectedEnabledComponent(const FDAUtilityGraph& Graph, FDAWorldAssetId StartAssetId) const;
    static EDAUtilityState ToUtilityState(float RequestedCapacity, float SuppliedCapacity);

    TMap<EDAUtilityType, FDAUtilityGraph> Graphs;
};
