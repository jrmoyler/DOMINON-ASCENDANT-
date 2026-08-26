#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "World/DARegionalWorldState.h"

#include "DADiplomacySystem.generated.h"

struct FDACampaignSnapshot;

UCLASS()
class DOMINIONWORLD_API UDADiplomacySystem final : public UObject
{
    GENERATED_BODY()

public:
    bool ApplyReason(
        FDADiplomaticRelationship& Relationship,
        EDADiplomaticMetric Metric,
        FName SourceTag,
        float Magnitude,
        int64 WorldTick,
        FName MutationId) const;

    float GetAggregate(const FDADiplomaticRelationship& Relationship, EDADiplomaticMetric Metric) const;
    bool ApplyReason(FDACampaignSnapshot& Campaign, FName RelationshipId, EDADiplomaticMetric Metric,
        FName SourceTag, float Magnitude, int64 WorldTick, FName MutationId) const;
};
