#include "World/DAWorldAsset.h"

#include "Capture/DACaptureComponent.h"
#include "Damage/DAStructuralDamageComponent.h"
#include "Save/DACampaignSaveGame.h"

ADAWorldAsset::ADAWorldAsset()
{
    PrimaryActorTick.bCanEverTick = false;
    StructuralDamageComponent = CreateDefaultSubobject<UDAStructuralDamageComponent>(
        TEXT("StructuralDamage"));
    CaptureComponent = CreateDefaultSubobject<UDACaptureComponent>(TEXT("Capture"));
}

void ADAWorldAsset::InitializeFromRecord(const FDAWorldAssetRecord& InRecord)
{
    WorldAssetRecord = InRecord;
}

bool ADAWorldAsset::InitializeFromCampaignAuthority(
    IDACampaignAuthority& InAuthority, const FGuid InWorldAssetId)
{
    const FDACampaignSnapshot& Campaign = InAuthority.GetPersistentCampaign();
    const FDAWorldAssetRecord* Record = Campaign.FindWorldAssetRecord(InWorldAssetId);
    if (Record == nullptr)
    {
        return false;
    }
    WorldAssetRecord = *Record;
    if (Campaign.OperationConflict.FindStructuralDamageRecord(InWorldAssetId) != nullptr
        && !StructuralDamageComponent->InitializeFromCampaign(InAuthority, InWorldAssetId))
    {
        return false;
    }
    if (Campaign.OperationConflict.FindCaptureRecord(InWorldAssetId) != nullptr
        && !CaptureComponent->InitializeFromCampaign(InAuthority, InWorldAssetId))
    {
        return false;
    }
    return true;
}

const FDAWorldAssetRecord& ADAWorldAsset::GetWorldAssetRecord() const
{
    return WorldAssetRecord;
}
