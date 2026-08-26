#pragma once

#include "Campaign/DADaxtonCampaignState.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "DADaxtonEncounter.generated.h"

struct FDACampaignSnapshot;

USTRUCT(BlueprintType)
struct DOMINIONGAMEPLAY_API FDADaxtonContentPhase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PhaseId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Mechanics;
};

USTRUCT(BlueprintType)
struct DOMINIONGAMEPLAY_API FDADaxtonCharacterImport
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString AssetPath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourcePath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AssetClass;
};

struct DOMINIONGAMEPLAY_API FDADaxtonContentManifest
{
    int32 SchemaVersion = 0;
    FName ContentId;
    FString Fingerprint;
    FString LeaderAssetPath;
    TArray<FDADaxtonContentPhase> Phases;
    TArray<EDADaxtonLeaderState> LeaderStates;
    TArray<FDADaxtonCharacterImport> CharacterImports;
};

UCLASS(BlueprintType)
class DOMINIONGAMEPLAY_API UDADaxtonLeaderDefinition final : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName LeaderId = TEXT("leader.daxton_rhe");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName EncounterId;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<EDADaxtonLeaderState> SupportedStates;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SourceFingerprint;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly) bool bRuntimeManifestFallback = false;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

/** Frozen source/import boundary. Character packages are imported from artist FBX, never synthesized. */
class DOMINIONGAMEPLAY_API FDADaxtonContentPipeline
{
public:
    static FString GetCanonicalManifestPath();
    static bool LoadCanonical(FDADaxtonContentManifest& OutManifest, TArray<FText>& Errors);
    static bool LoadFile(const FString& Filename, FDADaxtonContentManifest& OutManifest,
        TArray<FText>& Errors);
    static bool ParseJson(const FString& Json, FDADaxtonContentManifest& OutManifest,
        TArray<FText>& Errors);
    static bool Validate(const FDADaxtonContentManifest& Manifest, TArray<FText>& Errors);
    static bool BuildLeaderDefinition(const FDADaxtonContentManifest& Manifest,
        UDADaxtonLeaderDefinition*& OutLeader, TArray<FText>& Errors);
    static bool ValidateGeneratedCache(const FDADaxtonContentManifest& Manifest,
        const UDADaxtonLeaderDefinition* Leader, TArray<FText>& Errors);
};

/** Stateless façade: every mutation lands directly on FDACampaignSnapshot::DaxtonState or another canonical authority. */
class DOMINIONGAMEPLAY_API FDADaxtonEncounter
{
public:
    static bool Start(FGuid ActionId, FDACampaignSnapshot& InOutCampaign, FString& OutError);
    static bool AdvanceGrandForgeProduction(FGuid ActionId,
        FDACampaignSnapshot& InOutCampaign, FString& OutError);
    static bool DeployHardenedCover(FGuid ActionId,
        FDACampaignSnapshot& InOutCampaign, FString& OutError);
    static bool ReinforceForgeGuard(FGuid ActionId,
        FDACampaignSnapshot& InOutCampaign, FString& OutError);
    static bool CompletePhaseOneIndustrialObjective(FGuid ActionId,
        FDACampaignSnapshot& InOutCampaign, FString& OutError);
    static bool ApplySystemInteraction(FGuid ActionId, EDADaxtonInteraction Interaction,
        float Strength, FDACampaignSnapshot& InOutCampaign, FString& OutError);
    static bool EnterPhaseThree(FGuid ActionId,
        FDACampaignSnapshot& InOutCampaign, FString& OutError);
    static bool CompleteChoiceObjective(FGuid ActionId, EDADaxtonChoiceObjective Objective,
        FDACampaignSnapshot& InOutCampaign, FString& OutError);
    static bool CanResolveLeaderState(EDADaxtonLeaderState State,
        const FDACampaignSnapshot& Campaign, FString& OutError);
    static bool ResolveLeaderState(FGuid ActionId, EDADaxtonLeaderState State,
        FDACampaignSnapshot& InOutCampaign, FString& OutError);
};
