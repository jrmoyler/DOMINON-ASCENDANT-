#pragma once

#include "CoreMinimal.h"

#include "DACampaignLiveSignals.generated.h"

UENUM(BlueprintType)
enum class EDACampaignUtilityKind : uint8
{
    Power,
    Water,
    Data,
    Logistics
};

UENUM(BlueprintType)
enum class EDACampaignUtilitySupply : uint8
{
    FullySupplied,
    MinorDeficit,
    SignificantDeficit,
    Critical,
    Offline
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACampaignCitizenSignal
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CitizenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid HomeWorldAssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName JobId;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACampaignJobOpeningSignal
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName JobId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid FacilityWorldAssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 OpenPositions = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACampaignJobAssignmentSignal
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CitizenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName JobId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid FacilityWorldAssetId;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACampaignUtilitySignal
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid WorldAssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDACampaignUtilityKind Utility = EDACampaignUtilityKind::Power;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDACampaignUtilitySupply Supply = EDACampaignUtilitySupply::Offline;
};

/** Save-owned live simulation projection. Systems may only replace it through the world authority CAS. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACampaignLiveSignalState
{
    GENERATED_BODY()

    bool Validate(FString& OutError) const;
    const FDACampaignCitizenSignal* FindCitizen(FName CitizenId) const;
    EDACampaignUtilitySupply ResolveUtility(EDACampaignUtilityKind Utility, FGuid WorldAssetId) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign|City", meta=(ClampMin="0")) int32 Population = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign|Economy") double Capital = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign|Economy") double Insight = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign|Economy") double Influence = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign|Economy", meta=(ClampMin="0")) int64 ResolvedDevelopmentCycles = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign|Citizens") TArray<FDACampaignCitizenSignal> Citizens;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign|Jobs") TArray<FDACampaignJobOpeningSignal> JobOpenings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign|Jobs") TArray<FDACampaignJobAssignmentSignal> JobAssignments;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign|Utilities") TArray<FDACampaignUtilitySignal> UtilitySignals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign", meta=(ClampMin="0")) int64 MutationRevision = 0;
};
