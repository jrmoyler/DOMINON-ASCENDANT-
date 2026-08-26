#pragma once

#include "CoreMinimal.h"
#include "World/DAWorldAssetRecord.h"

#include "DAConflictRecords.generated.h"

UENUM(BlueprintType)
enum class EDAStructureDamageState : uint8
{
    Operational,
    Damaged,
    Disabled,
    Ruined
};

UENUM(BlueprintType)
enum class EDACaptureAgentRole : uint8
{
    Other,
    Engineer,
    Founder,
    CaptureCapableSquad
};

UENUM(BlueprintType)
enum class EDACaptureOutcome : uint8
{
    None,
    Preserve,
    Convert,
    Study,
    Salvage,
    Gift
};

UENUM(BlueprintType)
enum class EDAGiftRecipientRelationship : uint8
{
    Unknown,
    Hostile,
    Neutral,
    Allied,
    LocalAuthority
};

/** Canonical content policy shared by persistence validation and runtime reconstruction. */
struct DOMINIONCORE_API FDAStructuralDamagePolicy
{
    static bool SupportsFullModularDestruction(FName CardDefinitionId);
    static const TArray<FName>& GetFullModularDestructionDefinitions();
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAStructureModuleHealthRecord
{
    GENERATED_BODY()

    FDAStructureModuleHealthRecord() = default;

    FDAStructureModuleHealthRecord(const FName InModuleId, const float InMaximumHealth, const bool bInDisablesProduction)
        : ModuleId(InModuleId)
        , MaximumHealth(InMaximumHealth)
        , CurrentHealth(InMaximumHealth)
        , bDisablesProduction(bInDisablesProduction)
    {
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Structural Damage")
    FName ModuleId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Structural Damage", meta = (ClampMin = "0.0"))
    float MaximumHealth = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Structural Damage", meta = (ClampMin = "0.0"))
    float CurrentHealth = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Structural Damage")
    bool bDisablesProduction = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Structural Damage")
    EDAStructureDamageState State = EDAStructureDamageState::Operational;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAStructuralDamageRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Structural Damage")
    FGuid WorldAssetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Structural Damage")
    FName CardDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Structural Damage")
    TArray<FDAStructureModuleHealthRecord> Modules;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Structural Damage")
    bool bProductionDisabled = false;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACaptureGiftRecipientRecord
{
    GENERATED_BODY()

    FDACaptureGiftRecipientRecord() = default;

    FDACaptureGiftRecipientRecord(const FName InCivilizationId, const EDAGiftRecipientRelationship InRelationship)
        : CivilizationId(InCivilizationId)
        , Relationship(InRelationship)
    {
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    FName CivilizationId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    EDAGiftRecipientRelationship Relationship = EDAGiftRecipientRelationship::Unknown;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACaptureRecord
{
    GENERATED_BODY()

    bool IsOutcomeInvariantValid(const FDAWorldAssetRecord& Asset) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    FGuid WorldAssetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    FName OriginalOwnerCivilizationId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    FName CapturingCivilizationId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    FGuid ActiveInteractionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    FGuid ActiveCaptureActorId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    EDACaptureAgentRole ActiveCaptureRole = EDACaptureAgentRole::Other;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture", meta = (ClampMin = "0.0"))
    float CaptureProgressSeconds = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture", meta = (ClampMin = "0.0"))
    float RequiredCaptureTimeSeconds = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    bool bCaptureInProgress = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    bool bCaptureCompleted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    bool bOutcomeResolved = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    EDACaptureOutcome Outcome = EDACaptureOutcome::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    bool bOperationalUseRemoved = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    bool bIntegrationRequired = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    bool bConversionOperationSanctioned = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    bool bRewardsGranted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture", meta = (ClampMin = "0.0"))
    float StudyInsightReward = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture", meta = (ClampMin = "0.0"))
    float SalvageCapitalReward = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture", meta = (ClampMin = "0"))
    int32 SalvageMaterialReward = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture", meta = (ClampMin = "0.0"))
    float GiftInfluenceReward = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture", meta = (ClampMin = "0.0"))
    float GiftLoyaltyReward = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    TArray<FDACaptureGiftRecipientRecord> AllowedGiftRecipients;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Capture")
    TArray<FName> History;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDASurrenderRewardPolicy
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Surrender", meta = (ClampMin = "0.0"))
    float Influence = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Surrender", meta = (ClampMin = "0.0"))
    float Loyalty = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Surrender", meta = (ClampMin = "0.0"))
    float FutureSurrenderLikelihood = 0.f;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDASurrenderRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Surrender")
    FGuid SquadId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Surrender")
    bool bAccepted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Surrender")
    TArray<FName> History;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAConflictResourceState
{
    GENERATED_BODY()

    bool IsFinite() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Resources")
    float Capital = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Resources")
    float Insight = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Resources")
    float Influence = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Resources")
    int32 Materials = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Post Conflict")
    float PostConflictLoyalty = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict|Post Conflict")
    float FutureSurrenderLikelihood = 0.f;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAOperationConflictSnapshot
{
    GENERATED_BODY()

    FDAStructuralDamageRecord* FindStructuralDamageRecord(FGuid WorldAssetId);
    const FDAStructuralDamageRecord* FindStructuralDamageRecord(FGuid WorldAssetId) const;
    FDACaptureRecord* FindCaptureRecord(FGuid WorldAssetId);
    const FDACaptureRecord* FindCaptureRecord(FGuid WorldAssetId) const;
    FDASurrenderRecord* FindSurrenderRecord(FGuid SquadId);
    const FDASurrenderRecord* FindSurrenderRecord(FGuid SquadId) const;
    bool Validate(const TArray<FDAWorldAssetRecord>& WorldAssets, FString& OutError) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    TArray<FDAStructuralDamageRecord> StructuralDamageRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    TArray<FDACaptureRecord> CaptureRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    TArray<FDASurrenderRecord> SurrenderRecords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    FDAConflictResourceState Resources;

};
