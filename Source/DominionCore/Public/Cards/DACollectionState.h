#pragma once

#include "Cards/DACardInstance.h"

#include "DACollectionState.generated.h"

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACollectionState
{
    GENERATED_BODY()

    FGuid AddInstance(
        const FName DefinitionId,
        const EDAAcquisitionSource Source,
        const int64 AcquisitionWorldTick = 0)
    {
        FCardInstance Instance;
        Instance.InstanceId = FGuid::NewGuid();
        Instance.DefinitionId = DefinitionId;
        Instance.AcquisitionSource = Source;
        Instance.AcquisitionWorldTick = AcquisitionWorldTick;
        Instances.Add(Instance.InstanceId, Instance);
        return Instance.InstanceId;
    }

    bool AddInstanceWithId(
        const FGuid InstanceId,
        const FName DefinitionId,
        const EDAAcquisitionSource Source,
        const int64 AcquisitionWorldTick = 0)
    {
        if (!InstanceId.IsValid() || DefinitionId.IsNone() || Instances.Contains(InstanceId))
        {
            return false;
        }

        FCardInstance Instance;
        Instance.InstanceId = InstanceId;
        Instance.DefinitionId = DefinitionId;
        Instance.AcquisitionSource = Source;
        Instance.AcquisitionWorldTick = AcquisitionWorldTick;
        Instances.Add(Instance.InstanceId, Instance);
        return true;
    }

    const FCardInstance* FindInstance(const FGuid InstanceId) const
    {
        return Instances.Find(InstanceId);
    }

    FCardInstance* FindInstance(const FGuid InstanceId)
    {
        return Instances.Find(InstanceId);
    }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<FGuid, FCardInstance> Instances;
};
