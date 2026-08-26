#pragma once

#include "Content/DAFirstHourQuestContent.h"
#include "Save/DACampaignSaveGame.h"

#include "DAFirstHourCampaignRuntime.generated.h"

struct FDACitySimulationState;
class FDAUtilityNetwork;

UENUM(BlueprintType)
enum class EDAFirstHourCampaignResult : uint8
{
    Applied, AlreadyApplied, PrerequisiteMissing, ConditionUnsatisfied, ChoiceRequired,
    ChoiceIneligible, InvalidManifest, InvalidState, ConflictingReplay
};

UENUM(BlueprintType)
enum class EDAFirstHourPlayerAction : uint8
{
    None, ReachFounderHall, RestoreFounderHallPower, ActivateFounderHallCore,
    InspectCustodianMarkings, InspectAdaptiveHabitatCard, PlaceAdaptiveHabitat,
    AcknowledgeUtilityExpansion, SpeakToNia, DiscoverReplacementModel,
    RetrainWorkers, EnactAutomationCap, EnterUtilityTunnel, RestoreAncientNode,
    DefeatMaintenanceDrones, InspectUnknownSymbol, ReachEdenBasin,
    InspectWaterQuality, SpeakToAmara, SpeakToOri
};

/** Concrete production signals only; authored condition names never enter from a caller. */
struct DOMINIONWORLD_API FDAFirstHourProgressionContext
{
    int64 WorldTick = 0;
    EDAFirstHourPlayerAction PlayerAction = EDAFirstHourPlayerAction::None;
    FName ChoiceBranchTag;
    /** Durable provenance: audit eligibility cannot be asserted with an ephemeral boolean. */
    FGuid VisionActionId;
    FGuid ResearchActionId;
    const FDACitySimulationState* CityState = nullptr;
    const FDAUtilityNetwork* UtilityNetwork = nullptr;
    const FDACampaignLiveSignalState* LiveSignals = nullptr;
};

/** Transactional coordinator over Task18 state and canonical campaign records. */
class DOMINIONWORLD_API FDAFirstHourCampaignRuntime
{
public:
    static EDAFirstHourCampaignResult TryStartQuest(const FDAFirstHourQuestManifest& Manifest,
        FName QuestId, const FDAFirstHourProgressionContext& Context, FDACampaignSnapshot& InOutCampaign);
    static EDAFirstHourCampaignResult AdvanceQuest(const FDAFirstHourQuestManifest& Manifest,
        FName QuestId, const FDAFirstHourProgressionContext& Context, FDACampaignSnapshot& InOutCampaign);
    static EDAFirstHourCampaignResult RecordCrisisCompletion(const FDAFirstHourQuestManifest& Manifest,
        FName CrisisQuestId, FName CompletionActionId, FGuid NarrativeActionId, int64 WorldTick,
        FDACampaignSnapshot& InOutCampaign);
    static EDAFirstHourCampaignResult CommitNiaStoryTransition(const FDAFirstHourQuestManifest& Manifest,
        FGuid SourceActionId, FName ResultStoryState, int64 WorldTick, FDACampaignSnapshot& InOutCampaign);
    static EDAFirstHourCampaignResult RecordAuditEligibilitySource(FName EligibilityId, FGuid SourceActionId,
        FName SourceActionTag, int64 WorldTick, FDACampaignSnapshot& InOutCampaign);
    static bool IsNiaChampionEligible(const FDAFirstHourQuestManifest& Manifest, const FDACampaignSnapshot& Campaign);
};
