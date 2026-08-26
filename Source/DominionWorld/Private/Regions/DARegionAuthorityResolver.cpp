#include "Regions/DARegionAuthorityResolver.h"

#include "Combat/DACoverSubsystem.h"
#include "Damage/DAStructuralDamageComponent.h"

namespace
{
    class FDAReadOnlyCampaignAuthority final : public IDACampaignAuthority
    {
    public:
        explicit FDAReadOnlyCampaignAuthority(const FDACampaignSnapshot& InCampaign)
            : Campaign(&InCampaign)
        {
        }

        virtual const FDACampaignSnapshot& GetPersistentCampaign() const override
        {
            return *Campaign;
        }

        virtual bool TryCommitPersistentCampaign(const FDACampaignSnapshot&, int64, int64, int64) override
        {
            return false;
        }

    private:
        const FDACampaignSnapshot* Campaign;
    };
}

bool UDARegionAuthorityResolver::ReconstructRegion(
    const FDARegionState& Region,
    const FDACampaignSnapshot& Campaign,
    UDACoverSubsystem& CoverAuthority)
{
    TUniquePtr<IDACampaignAuthority> Adapter =
        MakeUnique<FDAReadOnlyCampaignAuthority>(Campaign);
    if (!ReconstructRegionInternal(Region, *Adapter, CoverAuthority))
    {
        return false;
    }
    ReadOnlyCampaignAdapter = MoveTemp(Adapter);
    BoundCampaignAuthority = ReadOnlyCampaignAdapter.Get();
    return true;
}

bool UDARegionAuthorityResolver::ReconstructRegion(
    const FDARegionState& Region,
    IDACampaignAuthority& CampaignAuthority,
    UDACoverSubsystem& CoverAuthority)
{
    if (!ReconstructRegionInternal(Region, CampaignAuthority, CoverAuthority))
    {
        return false;
    }
    ReadOnlyCampaignAdapter.Reset();
    BoundCampaignAuthority = &CampaignAuthority;
    return true;
}

bool UDARegionAuthorityResolver::ReconstructRegionInternal(
    const FDARegionState& Region,
    IDACampaignAuthority& InCampaignAuthority,
    UDACoverSubsystem& CoverAuthority)
{
    const FDACampaignSnapshot& Campaign = InCampaignAuthority.GetPersistentCampaign();
    FString ValidationError;
    const FDARegionState* CanonicalRegion = Campaign.WorldState.FindRegion(Region.RegionId);
    if (Region.RegionId.IsNone()
        || CanonicalRegion == nullptr
        || CanonicalRegion->PersistentDelta.Revision != Region.PersistentDelta.Revision
        || !Campaign.Validate(ValidationError))
    {
        return false;
    }

    TMap<FGuid, TObjectPtr<UDAStructuralDamageComponent>> CandidateComponents;
    TSet<FName> CandidateCoverIds;
    TMap<FName, FVector> CandidateCoverLocations;
    for (const FDARegionActorState& Actor : CanonicalRegion->PersistentDelta.LocalActors)
    {
        if (Actor.DefinitionId == TEXT("forgeweave.defense_cover"))
        {
            CandidateCoverIds.Add(Actor.ActorId);
            CandidateCoverLocations.Add(Actor.ActorId, Actor.Transform.GetLocation());
            const FDACoverSocket* Existing = CoverAuthority.FindCoverSocket(Actor.ActorId);
            if (Existing != nullptr
                && (Existing->Source != EDACoverSource::Authored
                    || Existing->CoverType != EDACoverType::Hardened
                    || !Existing->Location.Equals(Actor.Transform.GetLocation())))
            {
                return false;
            }
        }
        if (Actor.WorldAssetId.IsValid()
            && Campaign.OperationConflict.FindStructuralDamageRecord(Actor.WorldAssetId) != nullptr)
        {
            UDAStructuralDamageComponent* Component = NewObject<UDAStructuralDamageComponent>(this);
            if (!Component->InitializeFromCampaign(InCampaignAuthority, Actor.WorldAssetId))
            {
                return false;
            }
            CandidateComponents.Add(Actor.WorldAssetId, Component);
        }
    }

    TArray<FName> StaleCoverIds;
    for (const FName PreviousCoverId : RegisteredCoverIds)
    {
        if (!CandidateCoverIds.Contains(PreviousCoverId))
        {
            const FDACoverSocket* Existing = CoverAuthority.FindCoverSocket(PreviousCoverId);
            if (Existing == nullptr || Existing->Source != EDACoverSource::Authored)
            {
                return false;
            }
            StaleCoverIds.Add(PreviousCoverId);
        }
    }
    const auto StableNameOrder = [](const FName Left, const FName Right)
    {
        return Left.ToString() < Right.ToString();
    };
    StaleCoverIds.Sort(StableNameOrder);
    TArray<FName> OrderedCandidateCoverIds = CandidateCoverIds.Array();
    OrderedCandidateCoverIds.Sort(StableNameOrder);

    // Every external mutation below is total after the read-only preflight above.
    for (const FName PreviousCoverId : StaleCoverIds)
    {
        if (!CoverAuthority.UnregisterAuthoredCoverSocket(PreviousCoverId))
        {
            return false;
        }
    }
    for (const FName CandidateCoverId : OrderedCandidateCoverIds)
    {
        if (!CoverAuthority.RegisterAuthoredCoverSocket(
            CandidateCoverId,
            EDACoverType::Hardened,
            CandidateCoverLocations.FindChecked(CandidateCoverId)))
        {
            return false;
        }
    }

    StructuralDamageComponents = MoveTemp(CandidateComponents);
    RegisteredCoverIds = MoveTemp(CandidateCoverIds);
    return true;
}

const UDAStructuralDamageComponent* UDARegionAuthorityResolver::FindStructuralDamageComponent(
    const FGuid WorldAssetId) const
{
    const TObjectPtr<UDAStructuralDamageComponent>* Component = StructuralDamageComponents.Find(WorldAssetId);
    return Component != nullptr ? Component->Get() : nullptr;
}

const FDAWorldAssetRecord* UDARegionAuthorityResolver::FindReconstructedWorldAssetRecord(
    const FGuid WorldAssetId) const
{
    return BoundCampaignAuthority != nullptr
        ? BoundCampaignAuthority->GetPersistentCampaign().FindWorldAssetRecord(WorldAssetId)
        : nullptr;
}
