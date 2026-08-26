#include "Diplomacy/DADiplomacySystem.h"
#include "Save/DACampaignSaveGame.h"

bool UDADiplomacySystem::ApplyReason(
    FDADiplomaticRelationship& Relationship,
    const EDADiplomaticMetric Metric,
    const FName SourceTag,
    const float Magnitude,
    const int64 WorldTick,
    const FName MutationId) const
{
    FString ValidationError;
    if (!Relationship.Validate(ValidationError)
        || SourceTag.IsNone()
        || MutationId.IsNone()
        || !FMath::IsFinite(Magnitude)
        || Magnitude == 0.f
        || WorldTick < 0
        || Relationship.ReasonLedger.ContainsByPredicate([MutationId](const FDADiplomaticReason& Reason)
        {
            return Reason.MutationId == MutationId;
        }))
    {
        return false;
    }

    FDADiplomaticRelationship Candidate = Relationship;
    float* Aggregate = nullptr;
    switch (Metric)
    {
    case EDADiplomaticMetric::Trust:
        Aggregate = &Candidate.Trust;
        break;
    case EDADiplomaticMetric::Respect:
        Aggregate = &Candidate.Respect;
        break;
    case EDADiplomaticMetric::Fear:
        Aggregate = &Candidate.Fear;
        break;
    case EDADiplomaticMetric::Dependence:
        Aggregate = &Candidate.Dependence;
        break;
    case EDADiplomaticMetric::Grievance:
        Aggregate = &Candidate.Grievance;
        break;
    case EDADiplomaticMetric::Compatibility:
        Aggregate = &Candidate.Compatibility;
        break;
    default:
        return false;
    }

    if (Aggregate == nullptr || !FMath::IsFinite(*Aggregate + Magnitude))
    {
        return false;
    }

    FDADiplomaticReason Reason;
    Reason.MutationId = MutationId;
    Reason.SourceTag = SourceTag;
    Reason.Metric = Metric;
    Reason.Magnitude = Magnitude;
    Reason.WorldTick = WorldTick;
    *Aggregate += Magnitude;
    Candidate.ReasonLedger.Add(MoveTemp(Reason));

    if (!Candidate.Validate(ValidationError))
    {
        return false;
    }
    Relationship = MoveTemp(Candidate);
    return true;
}

bool UDADiplomacySystem::ApplyReason(FDACampaignSnapshot& Campaign, const FName RelationshipId,
    const EDADiplomaticMetric Metric, const FName SourceTag, const float Magnitude,
    const int64 WorldTick, const FName MutationId) const
{
    FDADiplomaticRelationship* Relationship = Campaign.WorldState.Diplomacy.FindRelationship(RelationshipId);
    if (Relationship == nullptr)
    {
        FDADiplomaticRelationship NewRelationship; NewRelationship.RelationshipId = RelationshipId;
        Campaign.WorldState.Diplomacy.Relationships.Add(NewRelationship);
        Relationship = Campaign.WorldState.Diplomacy.FindRelationship(RelationshipId);
    }
    return Relationship != nullptr && ApplyReason(*Relationship, Metric, SourceTag, Magnitude, WorldTick, MutationId);
}

float UDADiplomacySystem::GetAggregate(
    const FDADiplomaticRelationship& Relationship,
    const EDADiplomaticMetric Metric) const
{
    switch (Metric)
    {
    case EDADiplomaticMetric::Trust:
        return Relationship.Trust;
    case EDADiplomaticMetric::Respect:
        return Relationship.Respect;
    case EDADiplomaticMetric::Fear:
        return Relationship.Fear;
    case EDADiplomaticMetric::Dependence:
        return Relationship.Dependence;
    case EDADiplomaticMetric::Grievance:
        return Relationship.Grievance;
    case EDADiplomaticMetric::Compatibility:
        return Relationship.Compatibility;
    default:
        return 0.f;
    }
}
