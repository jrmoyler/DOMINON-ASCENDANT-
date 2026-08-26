#pragma once

#include "CoreMinimal.h"
#include "Navigation/DAUIScreenRouter.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DAUICommandEndpoint.generated.h"

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIGridCoordinatePayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double X = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double Y = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FootprintX = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FootprintY = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) uint8 Rotation = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIWorldDestinationPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Destination = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TargetActorId;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUITreatyTermPayload
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TermId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Metric;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude = 0.f;
};

/** Complete reflected payload envelope. Validation selects the exact fields required by CommandId. */
USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIAuthoritativeCommandPayload
{
    GENERATED_BODY()

    static bool ParseAndValidate(FName CommandId, const FString& Json,
        FDAUIAuthoritativeCommandPayload& Out, FString& OutError);

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid CardInstanceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid ReplacementCardInstanceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid WorldAssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid RouteAssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CardDefinitionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BlueprintId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDAUIGridCoordinatePayload GridCoordinate;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CitizenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName JobId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid FacilityWorldAssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SquadId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Order;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDAUIWorldDestinationPayload OrderTarget;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RelationshipId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDAUITreatyTermPayload> TreatyTerms;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName QuestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HistoryRecordId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ResearchId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MetricId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ResolutionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RewardId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SaveSlotId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CampaignPresetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName InteractionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AbilityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Axis = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUICommandAuditRecord
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName CommandId;
    UPROPERTY(BlueprintReadOnly) FName SourceScreenId;
    UPROPERTY(BlueprintReadOnly) int64 WorldTick = 0;
    UPROPERTY(BlueprintReadOnly) FString PayloadJson;
};

/** Production application orchestrator installed into every local-player UI by default. */
UCLASS()
class DOMINIONUI_API UDAUICommandEndpointSubsystem final
    : public UGameInstanceSubsystem, public IDAGameplayUICommandEndpoint
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual bool ExecuteGameplayUICommand_Implementation(
        const FDAUICommandRequest& Request, FString& OutError) override;

    const TArray<FDAUICommandAuditRecord>& GetAuditRecords() const { return AuditRecords; }

private:
    bool CommitCampaign(class UDAWorldStateSubsystem& World,
        const struct FDACampaignSnapshot& Candidate, FString& OutError) const;
    bool ExecuteCampaignCommand(const FDAUICommandRequest& Request,
        const FDAUIAuthoritativeCommandPayload& Payload, FString& OutError);
    void RecordSuccess(const FDAUICommandRequest& Request);

    UPROPERTY(Transient) TArray<FDAUICommandAuditRecord> AuditRecords;
};
