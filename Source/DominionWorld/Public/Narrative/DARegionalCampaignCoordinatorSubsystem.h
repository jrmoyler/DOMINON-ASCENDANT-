#pragma once

#include "CoreMinimal.h"
#include "Narrative/DAQuestRuntime.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DARegionalCampaignCoordinatorSubsystem.generated.h"

class UDARegionalCrisisContentRegistrySubsystem;
struct FDARegionalCrisisManifest;

/** Pure campaign transaction used by the world authority before its single commit. */
struct DOMINIONWORLD_API FDARegionalCampaignCoordinatorRuntime
{
    static bool EvaluateEventTrigger(const struct FDARegionalWorldEventEntry& Entry,
        const FDACampaignSnapshot& Campaign);
    static bool EvaluateQuestTrigger(const struct FDARegionalQuestEntry& Entry,
        const FDACampaignSnapshot& Campaign);
    static bool ApplyEventResolutionEffects(const struct FDARegionalWorldEventEntry& Event,
        const struct FDARegionalCrisisResolutionDefinition& Resolution, int64 WorldTick,
        FDACampaignSnapshot& Candidate, FString& OutError);
    static bool ApplyFoundryQuestResolution(const FDARegionalCrisisManifest& Manifest,
        const FGuid& ActionId, EDAFoundryShortageResolution Resolution,
        int64 WorldTick, FDACampaignSnapshot& Candidate, FString& OutError);
};

/** Drives all canonical regional events and quests from the committed campaign clock and signals. */
UCLASS()
class DOMINIONWORLD_API UDARegionalCampaignCoordinatorSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Evaluates every one of the six events and ten quests against the current authority. */
    bool SynchronizeNow();
    EDAQuestRuntimeResult SubmitQuestChoice(FName QuestId, FName ChoiceId);

private:
    void HandleWorldTickStateCommitted(FDACommittedCampaignSnapshot CommittedState);

    UPROPERTY(Transient) TObjectPtr<UDARegionalCrisisContentRegistrySubsystem> ContentRegistry;
    UPROPERTY(Transient) TWeakObjectPtr<UDAWorldStateSubsystem> WorldStateSubsystem;
    FDelegateHandle WorldStateCommittedHandle;
    bool bSynchronizing = false;
};
