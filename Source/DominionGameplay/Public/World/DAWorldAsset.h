#pragma once

#include "Campaign/DACampaignAuthority.h"
#include "City/DAWorldAssetRecord.h"
#include "GameFramework/Actor.h"

#include "DAWorldAsset.generated.h"

UCLASS(BlueprintType)
class DOMINIONGAMEPLAY_API ADAWorldAsset : public AActor
{
    GENERATED_BODY()

public:
    ADAWorldAsset();

    UFUNCTION(BlueprintCallable, Category = "World Asset")
    void InitializeFromRecord(const FDAWorldAssetRecord& InRecord);

    /** Binds combat facades to the canonical owner; the actor remains a projection only. */
    bool InitializeFromCampaignAuthority(IDACampaignAuthority& InAuthority, FGuid InWorldAssetId);

    const FDAWorldAssetRecord& GetWorldAssetRecord() const;

private:
    UPROPERTY(VisibleAnywhere, Category = "World Asset")
    TObjectPtr<class UDAStructuralDamageComponent> StructuralDamageComponent;

    UPROPERTY(VisibleAnywhere, Category = "World Asset")
    TObjectPtr<class UDACaptureComponent> CaptureComponent;

    // This is an actor reconstruction snapshot. Persistent simulation records remain authoritative.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Asset", meta = (AllowPrivateAccess = "true"))
    FDAWorldAssetRecord WorldAssetRecord;
};
