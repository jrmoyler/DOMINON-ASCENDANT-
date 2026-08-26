#pragma once

#include "CoreMinimal.h"
#include "Narrative/DAQuestDefinition.h"
#include "Narrative/DAWorldEventDefinition.h"
#include "Save/DACampaignSaveGame.h"

enum class EDAQuestRuntimeResult : uint8
{
    Applied,
    AlreadyApplied,
    InvalidDefinition,
    DefinitionMismatch,
    InvalidState,
    NotFound,
    Waiting,
    RequiresChoice,
    AmbiguousTransition
};

enum class EDAWorldEventRuntimeResult : uint8
{
    Applied,
    AlreadyApplied,
    InvalidDefinition,
    DefinitionMismatch,
    InvalidState,
    NotFound
};

class DOMINIONWORLD_API FDAQuestRuntime
{
public:
    static EDAQuestRuntimeResult StartQuest(
        const FDAQuestDefinition& Definition,
        const TArray<FDAQuestWorldAssetBinding>& WorldAssetBindings,
        int64 WorldTick,
        FDACampaignSnapshot& InOutCampaign);

    static EDAQuestRuntimeResult EvaluateCurrentNode(
        const FDAQuestDefinition& Definition,
        const FDAQuestEvaluationContext& EvaluationContext,
        FDACampaignSnapshot& InOutCampaign,
        FName& OutSelectedBranchTag);

    static EDAQuestRuntimeResult SelectChoice(
        const FDAQuestDefinition& Definition,
        FName BranchTag,
        const FDAQuestEvaluationContext& EvaluationContext,
        FDACampaignSnapshot& InOutCampaign);

    static EDAQuestRuntimeResult AbandonQuest(
        const FDAQuestDefinition& Definition,
        FDACampaignSnapshot& InOutCampaign);
};

class DOMINIONWORLD_API FDAWorldEventRuntime
{
public:
    static EDAWorldEventRuntimeResult StartEvent(
        const FDAWorldEventDefinition& Definition,
        int64 WorldTick,
        FDACampaignSnapshot& InOutCampaign);

    static EDAWorldEventRuntimeResult AdvanceEvent(
        const FDAWorldEventDefinition& Definition,
        FName NextStageId,
        int64 WorldTick,
        FDACampaignSnapshot& InOutCampaign);
};
