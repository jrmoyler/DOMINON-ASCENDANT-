#pragma once

#include "CoreMinimal.h"
#include "Campaign/DACampaignAuthority.h"
#include "Damage/DAStructuralDamageComponent.h"
#include "Regions/DARegionState.h"
#include "UObject/Object.h"

#include "DARegionAuthorityResolver.generated.h"

class UDACoverSubsystem;

/** Rebuilds streamed-region gameplay facades from the canonical campaign aggregate. */
UCLASS()
class DOMINIONWORLD_API UDARegionAuthorityResolver final : public UObject
{
    GENERATED_BODY()

public:
    bool ReconstructRegion(
        const FDARegionState& Region,
        IDACampaignAuthority& CampaignAuthority,
        UDACoverSubsystem& CoverAuthority);

    /** Read-only compatibility path for deterministic reconstruction tests; commits are rejected. */
    bool ReconstructRegion(
        const FDARegionState& Region,
        const FDACampaignSnapshot& Campaign,
        UDACoverSubsystem& CoverAuthority);

    const UDAStructuralDamageComponent* FindStructuralDamageComponent(FGuid WorldAssetId) const;
    const FDAWorldAssetRecord* FindReconstructedWorldAssetRecord(FGuid WorldAssetId) const;
    bool IsCoverRegisteredByResolver(FName CoverId) const { return RegisteredCoverIds.Contains(CoverId); }

private:
    bool ReconstructRegionInternal(const FDARegionState& Region,
        IDACampaignAuthority& InCampaignAuthority, UDACoverSubsystem& CoverAuthority);

    IDACampaignAuthority* BoundCampaignAuthority = nullptr;
    TUniquePtr<IDACampaignAuthority> ReadOnlyCampaignAdapter;

    UPROPERTY(Transient)
    TMap<FGuid, TObjectPtr<UDAStructuralDamageComponent>> StructuralDamageComponents;

    UPROPERTY(Transient)
    TSet<FName> RegisteredCoverIds;
};
