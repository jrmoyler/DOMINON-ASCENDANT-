#pragma once

#include "Conquest/DAConquestState.h"
#include "Content/DARegionalCrisisContent.h"
#include "Save/DACampaignSaveGame.h"

struct DOMINIONWORLD_API FDAForgeweaveConquestQuestManifest
{
    int32 SchemaVersion = 0;
    FName CampaignId;
    FString Fingerprint;
    TArray<FDARegionalQuestEntry> Quests;
};

/** Projects only canonical campaign authorities into the persisted conquest ledger. */
class DOMINIONWORLD_API FDAConquestSystem
{
public:
    static bool Synchronize(FDACampaignSnapshot& InOutCampaign, FString& OutError);
    static bool CanCompleteRoute(EDAForgeweaveRoute Route, const FDACampaignSnapshot& Campaign,
        FString& OutError);
    static bool CompleteRoute(FGuid ActionId, EDAForgeweaveRoute Route,
        FDACampaignSnapshot& InOutCampaign, FString& OutError);
};

/** Strict source manifest and real DataAsset generation boundary for the five conquest quests. */
class DOMINIONWORLD_API FDAForgeweaveConquestQuestPipeline
{
public:
    static FString GetCanonicalManifestPath();
    static bool LoadCanonical(FDAForgeweaveConquestQuestManifest& OutManifest, TArray<FText>& Errors);
    static bool LoadFile(const FString& Filename, FDAForgeweaveConquestQuestManifest& OutManifest,
        TArray<FText>& Errors);
    static bool ParseJson(const FString& Json, FDAForgeweaveConquestQuestManifest& OutManifest,
        TArray<FText>& Errors);
    static bool Validate(const FDAForgeweaveConquestQuestManifest& Manifest, TArray<FText>& Errors);
    static bool BuildAssets(const FDAForgeweaveConquestQuestManifest& Manifest,
        TArray<UDARegionalQuestDefinition*>& OutQuests, TArray<FText>& Errors);
    static bool ValidateGeneratedCache(const FDAForgeweaveConquestQuestManifest& Manifest,
        const TArray<UDARegionalQuestDefinition*>& Quests, TArray<FText>& Errors);
};
