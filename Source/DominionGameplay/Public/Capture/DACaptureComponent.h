#pragma once

#include "Campaign/DACampaignAuthority.h"
#include "Components/ActorComponent.h"
#include "Conflict/DAConflictRecords.h"
#include "Units/DASquadEntity.h"

#include "DACaptureComponent.generated.h"

USTRUCT(BlueprintType)
struct DOMINIONGAMEPLAY_API FDACaptureInteractionContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    FGuid InteractionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    FGuid CaptureActorId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    EDACaptureAgentRole AgentRole = EDACaptureAgentRole::Other;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    bool bActorPresent = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    bool bContested = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    bool bActiveSecurity = false;
};

USTRUCT(BlueprintType)
struct DOMINIONGAMEPLAY_API FDASurrenderContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender")
    FGuid SquadId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender")
    FName SquadDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender")
    EDAMoraleState MoraleState = EDAMoraleState::Steady;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float MilitarySovereignty = 100.f;
};

/** Read-only copy emitted after capture authority publishes; it owns no outcome state. */
USTRUCT(BlueprintType)
struct DOMINIONGAMEPLAY_API FDACapturePresentationSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName OriginalOwnerCivilizationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName CurrentOwnerCivilizationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName CapturingCivilizationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ProgressSeconds = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float RequiredSeconds = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bInProgress = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bCompleted = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bOutcomeResolved = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bRewardsGranted = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bIntegrationRequired = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDACaptureOutcome Outcome = EDACaptureOutcome::None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDACaptureStateChanged,
    FDACapturePresentationSnapshot, CommittedSnapshot);

/** Runtime interaction facade over Core-owned capture/surrender records. */
UCLASS(ClassGroup = (Dominion), BlueprintType, meta = (BlueprintSpawnableComponent))
class DOMINIONGAMEPLAY_API UDACaptureComponent final : public UActorComponent
{
    GENERATED_BODY()

public:
    static constexpr float BaseCaptureTimeSeconds = 20.f;
    static constexpr float EngineerCaptureTimeMultiplier = 0.65f;
    static constexpr float SurrenderSovereigntyThreshold = 25.f;

    bool InitializeFromCampaign(IDACampaignAuthority& InCampaignAuthority, FGuid InWorldAssetId);

    bool CanCapture(const FDACaptureInteractionContext& Context) const;
    bool BeginCapture(const FDACaptureInteractionContext& Context, FName CapturingCivilizationId);
    bool AdvanceCapture(float DeltaSeconds, const FDACaptureInteractionContext& Context);
    bool ResolveOutcome(EDACaptureOutcome Outcome, FName GiftRecipientCivilizationId);

    UFUNCTION(BlueprintPure, Category = "Capture|Presentation")
    FDACapturePresentationSnapshot GetPresentationSnapshot() const;

    /** Presentation consumes the committed snapshot and never gates or rewrites capture. */
    UPROPERTY(BlueprintAssignable, Category = "Capture|Presentation")
    FDACaptureStateChanged OnCaptureStateChanged;

    static bool CanAcceptSurrender(const FDASurrenderContext& Context, const FDAOperationConflictSnapshot& Conflict);
    static bool AcceptSurrender(const FDASurrenderContext& Context, const FDASurrenderRewardPolicy& Rewards,
        FDACampaignSnapshot& InOutCampaign);

private:
    bool ResolveAuthority(const FDAWorldAssetRecord*& OutAssetRecord,
        const FDACaptureRecord*& OutCaptureRecord) const;
    static bool AreFacilityCapturePreconditionsMet(const FDAWorldAssetRecord& AssetRecord,
        const FDACaptureRecord& CaptureRecord);
    static bool IsCaptureRoleEligible(EDACaptureAgentRole AgentRole);
    static bool IsInteractionEligible(const FDACaptureInteractionContext& Context);
    static bool AreCaptureRewardsValid(const FDACaptureRecord& Record);
    static bool AreSurrenderRewardsValid(const FDASurrenderRewardPolicy& Rewards);
    static bool IsGiftRecipientAllowed(const FDACaptureRecord& Record, FName RecipientCivilizationId);
    static void ResetActiveCapture(FDACaptureRecord& CaptureRecord);
    static void SynchronizeOwnedProjections(FDACampaignSnapshot& Campaign,
        const FDAWorldAssetRecord& AssetRecord);

    /** Non-owning; production lifetime is the GameInstance campaign subsystem. */
    IDACampaignAuthority* CampaignAuthority = nullptr;

    UPROPERTY(Transient)
    FGuid WorldAssetId;
};
