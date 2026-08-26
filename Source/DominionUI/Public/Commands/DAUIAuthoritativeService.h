#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Interface.h"

#include "DAUIAuthoritativeService.generated.h"

/** Typed records owned by the feature services delivered in Tasks 23-25. */
USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIConquestRouteAuthorityRecord
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid RouteAssetId;
    UPROPERTY(BlueprintReadOnly) float Progress = 0.f;
    UPROPERTY(BlueprintReadOnly) float CapitalReward = 0.f;
    UPROPERTY(BlueprintReadOnly) float InsightReward = 0.f;
    UPROPERTY(BlueprintReadOnly) TArray<FName> Reasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUILeaderResolutionAuthorityRecord
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName ResolutionId;
    UPROPERTY(BlueprintReadOnly) FName LeaderId;
    UPROPERTY(BlueprintReadOnly) FName Status;
    UPROPERTY(BlueprintReadOnly) TArray<FName> Reasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIAscensionRewardAuthorityRecord
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName RewardId;
    UPROPERTY(BlueprintReadOnly) FName Status;
    UPROPERTY(BlueprintReadOnly) TArray<FName> Reasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIAuthoritativeFeatureSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int64 Revision = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FName> ResearchIds;
    UPROPERTY(BlueprintReadOnly) TArray<FName> ResearchReasons;
    UPROPERTY(BlueprintReadOnly) TArray<FDAUIConquestRouteAuthorityRecord> ConquestRoutes;
    UPROPERTY(BlueprintReadOnly) TArray<FDAUILeaderResolutionAuthorityRecord> LeaderResolutions;
    UPROPERTY(BlueprintReadOnly) TArray<FDAUIAscensionRewardAuthorityRecord> AscensionRewards;
    UPROPERTY(BlueprintReadOnly) TArray<FName> HistoryTags;
    UPROPERTY(BlueprintReadOnly) TMap<FName, double> MetricValues;
    UPROPERTY(BlueprintReadOnly) TArray<FName> FounderInteractionIds;
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONUI_API UDAUIAuthoritativeFeatureService : public UInterface
{
    GENERATED_BODY()
};

class DOMINIONUI_API IDAUIAuthoritativeFeatureService
{
    GENERATED_BODY()
public:
    virtual bool ExecuteAuthoritativeUICommand(FName CommandId, FName SourceScreenId,
        const FString& PayloadJson, FString& OutError) = 0;
    virtual bool CaptureAuthoritativeUIState(
        FDAUIAuthoritativeFeatureSnapshot& OutState, FString& OutError) const = 0;
};

/** The only Task-21 integration point for authorities implemented by later tasks.
 * Missing registration is reported as ServiceUnavailable and can never be treated as success.
 */
UCLASS()
class DOMINIONUI_API UDAUIAuthoritativeFeatureRegistrySubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    bool RegisterAuthoritativeService(UObject* Service, FString& OutError);
    void UnregisterAuthoritativeService(UObject* Service);
    bool ExecuteRegisteredCommand(FName CommandId, FName SourceScreenId,
        const FString& PayloadJson, FString& OutError) const;
    bool CaptureRegisteredState(FDAUIAuthoritativeFeatureSnapshot& OutState, FString& OutError) const;
private:
    IDAUIAuthoritativeFeatureService* Resolve(FString& OutError) const;
    UPROPERTY(Transient) TWeakObjectPtr<UObject> RegisteredService;
};
