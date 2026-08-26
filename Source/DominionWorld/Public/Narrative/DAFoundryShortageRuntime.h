#pragma once

#include "Campaign/DARegionalCrisisCampaignState.h"
#include "Content/DARegionalCrisisContent.h"

struct FDACampaignSnapshot;
class UDADiplomacySystem;

enum class EDAFoundryShortageActionResult : uint8
{
    Applied,
    AlreadyApplied,
    Rejected
};

/** Deterministic World-Tick runtime for the canonical Foundry Shortage event. */
class DOMINIONWORLD_API FDAFoundryShortageRuntime
{
public:
    static bool ProcessWorldTick(const FDARegionalCrisisManifest& Manifest, int64 WorldTick,
        FDACampaignSnapshot& Campaign, UDADiplomacySystem& Diplomacy, FString& OutError);
    static EDAFoundryShortageActionResult Resolve(const FDARegionalCrisisManifest& Manifest,
        FGuid ActionId, EDAFoundryShortageResolution Resolution, int64 WorldTick,
        FDACampaignSnapshot& Campaign, UDADiplomacySystem& Diplomacy, FString& OutError);
    static FName EvaluateDialogueVariant(FName ConditionId, const FDACampaignSnapshot& Campaign);
    static bool EvaluateDialogueCondition(FName ConditionId, const FDACampaignSnapshot& Campaign);
};
