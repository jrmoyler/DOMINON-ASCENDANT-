#pragma once

#include "CoreMinimal.h"
#include "Save/DACampaignSaveGame.h"

// Stateless operations over FDACampaignSnapshot::HistoryTags, the sole campaign history-tag authority.
class DOMINIONWORLD_API FDAHistoryLedger
{
public:
    static bool HasTag(const FDACampaignSnapshot& Campaign, FName HistoryTag);
    static bool RecordTags(FDACampaignSnapshot& InOutCampaign, const TArray<FName>& HistoryTags);
};
