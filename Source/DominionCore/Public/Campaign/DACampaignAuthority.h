#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "DACampaignAuthority.generated.h"

struct FDACampaignSnapshot;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class DOMINIONCORE_API UDACampaignAuthority : public UInterface
{
    GENERATED_BODY()
};

/**
 * Module-safe transaction boundary for gameplay facades that need to mutate a campaign.
 * Implementations own the aggregate; callers may only copy the current value and publish
 * a fully validated candidate with compare-and-swap revisions.
 */
class DOMINIONCORE_API IDACampaignAuthority
{
    GENERATED_BODY()

public:
    virtual ~IDACampaignAuthority() = default;
    virtual const FDACampaignSnapshot& GetPersistentCampaign() const = 0;
    virtual bool TryCommitPersistentCampaign(
        const FDACampaignSnapshot& Candidate,
        int64 ExpectedNarrativeRevision,
        int64 ExpectedSignalRevision,
        int64 ExpectedWorldTick) = 0;
};
