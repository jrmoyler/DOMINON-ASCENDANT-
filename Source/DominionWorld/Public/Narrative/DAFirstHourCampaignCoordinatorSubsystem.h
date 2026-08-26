#pragma once

#include "CoreMinimal.h"
#include "Narrative/DAFirstHourCampaignRuntime.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DAFirstHourCampaignCoordinatorSubsystem.generated.h"

class UDAFirstHourContentRegistrySubsystem;

/** Production owner/coordinator: all first-hour signals and mutations use one canonical campaign snapshot. */
UCLASS()
class DOMINIONWORLD_API UDAFirstHourCampaignCoordinatorSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    EDAFirstHourCampaignResult TryProgress(FName QuestId, EDAFirstHourPlayerAction PlayerAction,
        FName ChoiceBranchTag, FGuid VisionActionId = FGuid(), FGuid ResearchActionId = FGuid());
    EDAFirstHourCampaignResult CommitNiaStoryTransition(FGuid SourceActionId, FName ResultStoryState);
    EDAFirstHourCampaignResult RecordCrisisCompletion(FName CrisisQuestId, FName CompletionActionId,
        FGuid NarrativeActionId);
    EDAFirstHourCampaignResult RecordAuditEligibilitySource(FName EligibilityId, FGuid SourceActionId,
        FName SourceActionTag);
    const FDACampaignSnapshot* GetCampaignSnapshot() const;

    UFUNCTION(BlueprintCallable, Category="Dominion|First Hour")
    EDAFirstHourCampaignResult SubmitPlayerAction(FName QuestId, EDAFirstHourPlayerAction PlayerAction,
        FName ChoiceBranchTag, FGuid VisionActionId, FGuid ResearchActionId);
    UFUNCTION(BlueprintCallable, Category="Dominion|First Hour")
    EDAFirstHourCampaignResult SubmitNiaStoryAction(FGuid SourceActionId, FName ResultStoryState);
    UFUNCTION(BlueprintCallable, Category="Dominion|First Hour")
    EDAFirstHourCampaignResult SubmitCrisisAction(FName CrisisQuestId, FName CompletionActionId,
        FGuid NarrativeActionId);
    UFUNCTION(BlueprintCallable, Category="Dominion|First Hour")
    EDAFirstHourCampaignResult SubmitAuditAction(FName EligibilityId, FGuid SourceActionId,
        FName SourceActionTag);
    /** Atomic UI/research ingress: records the canonical action and its typed audit proof. */
    UFUNCTION(BlueprintCallable, Category="Dominion|First Hour")
    EDAFirstHourCampaignResult SubmitResearchAction(FGuid SourceActionId);

private:
    EDAFirstHourCampaignResult TryProgressInternal(FName QuestId, EDAFirstHourPlayerAction PlayerAction,
        FName ChoiceBranchTag, FGuid VisionActionId, FGuid ResearchActionId);
    void ProgressPendingQuests();
    void HandleWorldTickStateCommitted(FDACommittedCampaignSnapshot CommittedState);

    UPROPERTY(Transient) TObjectPtr<UDAFirstHourContentRegistrySubsystem> ContentRegistry;
    UPROPERTY(Transient) TWeakObjectPtr<UDAWorldStateSubsystem> WorldStateSubsystem;
    FDelegateHandle WorldStateCommittedHandle;
    bool bProgressing = false;
};
