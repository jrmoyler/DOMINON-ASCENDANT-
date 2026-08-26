#pragma once

#include "CoreMinimal.h"
#include "Narrative/DANarrativeRecords.h"
#include "Save/DACampaignSaveGame.h"

enum class EDAPromiseMutationResult : uint8
{
    Applied,
    AlreadyApplied,
    RequiresConfirmation,
    StaleRevision,
    InvalidInput
};

class DOMINIONWORLD_API FDAPromiseLedger
{
public:
    explicit FDAPromiseLedger(FDACampaignSnapshot& InCampaign);

    int64 GetRevision() const;
    EDAPromiseMutationResult RegisterPromise(const FDAPromiseRecord& Promise);
    TArray<FDAPromiseRecord> GetBrokenPromises(const TArray<FName>& ActionTags) const;

    EDAPromiseMutationResult CommitAction(
        FGuid ActionId,
        const TArray<FName>& ActionTags,
        int64 WorldTick,
        int64 ExpectedRevision,
        bool bConfirmBrokenPromises);

private:
    FDACampaignSnapshot& Campaign;
};
