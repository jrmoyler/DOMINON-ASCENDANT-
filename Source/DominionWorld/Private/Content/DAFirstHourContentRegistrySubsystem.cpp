#include "Content/DAFirstHourContentRegistrySubsystem.h"

#include "Engine/AssetManager.h"

void UDAFirstHourContentRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    TArray<FText> Errors;
    bReady = RebuildRegistry(Errors);
    if (!bReady)
    {
        for (const FText& Error : Errors)
        {
            UE_LOG(LogTemp, Error, TEXT("First-hour content registry: %s"), *Error.ToString());
        }
#if !UE_BUILD_SHIPPING
        ensureMsgf(false, TEXT("First-hour canonical content could not be initialized."));
#endif
    }
}

bool UDAFirstHourContentRegistrySubsystem::RebuildRegistry(TArray<FText>& Errors)
{
    bUsedGeneratedCache = false;
    StoredQuestDefinitions.Reset();
    StoredCitizenDefinition = nullptr;
    Content = FDABuiltFirstHourContent();
    if (!FDAFirstHourQuestPipeline::LoadCanonical(Manifest, Errors))
    {
        return false;
    }

    UAssetManager& AssetManager = UAssetManager::Get();
    TArray<UDA_FirstHourQuestDefinition*> CandidateQuests;
    for (const FDAFirstHourQuestEntry& CanonicalQuest : Manifest.Quests)
    {
        const FPrimaryAssetId AssetId(TEXT("DAQuestDefinition"), CanonicalQuest.Definition.QuestId);
        const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
        CandidateQuests.Add(Cast<UDA_FirstHourQuestDefinition>(AssetPath.TryLoad()));
    }
    TArray<FPrimaryAssetId> CitizenIds;
    AssetManager.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("DACitizenDefinition")), CitizenIds);
    UDA_CitizenDefinition* CandidateCitizen = nullptr;
    if (CitizenIds.Num() == 1 && CitizenIds[0].PrimaryAssetName == Manifest.Citizen.CitizenId)
    {
        const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(CitizenIds[0]);
        CandidateCitizen = Cast<UDA_CitizenDefinition>(AssetPath.TryLoad());
    }

    TArray<FText> CacheErrors;
    bUsedGeneratedCache = FDAFirstHourQuestPipeline::ValidateGeneratedCache(
        Manifest, CandidateQuests, CandidateCitizen, CacheErrors);
    if (!bUsedGeneratedCache)
    {
        CandidateQuests.Reset();
        CandidateCitizen = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("Generated first-hour cache is absent, partial, or stale; using canonical manifest fallback."));
    }
    if (!FDAFirstHourQuestPipeline::BuildRuntimeContent(
        Manifest, CandidateQuests, CandidateCitizen, Content, Errors))
    {
        return false;
    }
    for (UDA_FirstHourQuestDefinition* Quest : Content.Quests) StoredQuestDefinitions.Add(Quest);
    StoredCitizenDefinition = Content.Citizen;
    return true;
}
