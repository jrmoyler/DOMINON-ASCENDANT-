#include "Networks/DAUtilityNetwork.h"

void FDAUtilityNetwork::RegisterNode(
    const EDAUtilityType UtilityType,
    const FDAWorldAssetId AssetId,
    const float GenerationCapacity,
    const float DemandCapacity)
{
    if (!AssetId.IsValid())
    {
        return;
    }

    FDAUtilityGraph& Graph = GetOrCreateGraph(UtilityType);
    FDAUtilityNode* ExistingNode = Graph.Nodes.Find(AssetId);
    if (ExistingNode != nullptr)
    {
        ExistingNode->GenerationCapacity = FMath::Max(0.f, GenerationCapacity);
        ExistingNode->DemandCapacity = FMath::Max(0.f, DemandCapacity);
        return;
    }

    FDAUtilityNode Node;
    Node.GenerationCapacity = FMath::Max(0.f, GenerationCapacity);
    Node.DemandCapacity = FMath::Max(0.f, DemandCapacity);
    Node.RegistrationOrder = Graph.NextRegistrationOrder++;
    Graph.Nodes.Add(AssetId, Node);
    Graph.Neighbors.FindOrAdd(AssetId);
}

bool FDAUtilityNetwork::ConnectNodes(
    const EDAUtilityType UtilityType,
    const FDAWorldAssetId FirstAssetId,
    const FDAWorldAssetId SecondAssetId)
{
    if (!FirstAssetId.IsValid() || !SecondAssetId.IsValid() || FirstAssetId == SecondAssetId)
    {
        return false;
    }

    FDAUtilityGraph& Graph = GetOrCreateGraph(UtilityType);
    if (!Graph.Nodes.Contains(FirstAssetId) || !Graph.Nodes.Contains(SecondAssetId))
    {
        return false;
    }

    Graph.Neighbors.FindOrAdd(FirstAssetId).Add(SecondAssetId);
    Graph.Neighbors.FindOrAdd(SecondAssetId).Add(FirstAssetId);
    return true;
}

bool FDAUtilityNetwork::SetNodeEnabled(const EDAUtilityType UtilityType, const FDAWorldAssetId AssetId, const bool bEnabled)
{
    FDAUtilityGraph* Graph = Graphs.Find(UtilityType);
    if (Graph == nullptr)
    {
        return false;
    }

    FDAUtilityNode* Node = Graph->Nodes.Find(AssetId);
    if (Node == nullptr)
    {
        return false;
    }

    Node->bEnabled = bEnabled;
    return true;
}

FDAUtilityResolution FDAUtilityNetwork::ResolveUtility(const EDAUtilityType UtilityType, const FDAWorldAssetId AssetId) const
{
    FDAUtilityResolution Resolution;
    const FDAUtilityGraph* Graph = FindGraph(UtilityType);
    if (Graph == nullptr)
    {
        return Resolution;
    }

    const FDAUtilityNode* RequestedNode = Graph->Nodes.Find(AssetId);
    if (RequestedNode == nullptr || !RequestedNode->bEnabled)
    {
        return Resolution;
    }

    Resolution.RequestedCapacity = RequestedNode->DemandCapacity;
    if (Resolution.RequestedCapacity <= 0.f)
    {
        Resolution.State = EDAUtilityState::FullySupplied;
        return Resolution;
    }

    TArray<FDAWorldAssetId> Component = FindConnectedEnabledComponent(*Graph, AssetId);
    Component.Sort([Graph](const FDAWorldAssetId& Left, const FDAWorldAssetId& Right)
    {
        return Graph->Nodes.FindChecked(Left).RegistrationOrder < Graph->Nodes.FindChecked(Right).RegistrationOrder;
    });

    float AvailableCapacity = 0.f;
    for (const FDAWorldAssetId& ComponentAssetId : Component)
    {
        AvailableCapacity += Graph->Nodes.FindChecked(ComponentAssetId).GenerationCapacity;
    }

    for (const FDAWorldAssetId& ComponentAssetId : Component)
    {
        const FDAUtilityNode& Node = Graph->Nodes.FindChecked(ComponentAssetId);
        const float SuppliedToNode = FMath::Min(AvailableCapacity, Node.DemandCapacity);
        AvailableCapacity -= SuppliedToNode;
        if (ComponentAssetId == AssetId)
        {
            Resolution.SuppliedCapacity = SuppliedToNode;
            Resolution.State = ToUtilityState(Resolution.RequestedCapacity, Resolution.SuppliedCapacity);
            return Resolution;
        }
    }

    return Resolution;
}

FDAUtilityNetwork::FDAUtilityGraph& FDAUtilityNetwork::GetOrCreateGraph(const EDAUtilityType UtilityType)
{
    return Graphs.FindOrAdd(UtilityType);
}

const FDAUtilityNetwork::FDAUtilityGraph* FDAUtilityNetwork::FindGraph(const EDAUtilityType UtilityType) const
{
    return Graphs.Find(UtilityType);
}

TArray<FDAWorldAssetId> FDAUtilityNetwork::FindConnectedEnabledComponent(
    const FDAUtilityGraph& Graph,
    const FDAWorldAssetId StartAssetId) const
{
    TArray<FDAWorldAssetId> Component;
    const FDAUtilityNode* StartNode = Graph.Nodes.Find(StartAssetId);
    if (StartNode == nullptr || !StartNode->bEnabled)
    {
        return Component;
    }

    TSet<FDAWorldAssetId> Visited;
    TArray<FDAWorldAssetId> Pending;
    Pending.Add(StartAssetId);
    Visited.Add(StartAssetId);

    while (!Pending.IsEmpty())
    {
        const FDAWorldAssetId CurrentAssetId = Pending.Pop(EAllowShrinking::No);
        Component.Add(CurrentAssetId);
        const TSet<FDAWorldAssetId>* NeighborSet = Graph.Neighbors.Find(CurrentAssetId);
        if (NeighborSet == nullptr)
        {
            continue;
        }

        for (const FDAWorldAssetId& NeighborAssetId : *NeighborSet)
        {
            const FDAUtilityNode* NeighborNode = Graph.Nodes.Find(NeighborAssetId);
            if (NeighborNode != nullptr && NeighborNode->bEnabled && !Visited.Contains(NeighborAssetId))
            {
                Visited.Add(NeighborAssetId);
                Pending.Add(NeighborAssetId);
            }
        }
    }

    return Component;
}

EDAUtilityState FDAUtilityNetwork::ToUtilityState(const float RequestedCapacity, const float SuppliedCapacity)
{
    if (RequestedCapacity <= 0.f || SuppliedCapacity >= RequestedCapacity)
    {
        return EDAUtilityState::FullySupplied;
    }

    if (SuppliedCapacity <= 0.f)
    {
        return EDAUtilityState::Offline;
    }

    const float SuppliedRatio = SuppliedCapacity / RequestedCapacity;
    if (SuppliedRatio >= 0.75f)
    {
        return EDAUtilityState::MinorDeficit;
    }

    if (SuppliedRatio >= 0.40f)
    {
        return EDAUtilityState::SignificantDeficit;
    }

    return EDAUtilityState::Critical;
}
