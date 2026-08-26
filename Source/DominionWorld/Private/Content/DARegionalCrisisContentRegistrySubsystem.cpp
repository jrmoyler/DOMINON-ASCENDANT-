#include "Content/DARegionalCrisisContentRegistrySubsystem.h"
#include "Misc/PackageName.h"

void UDARegionalCrisisContentRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); Events.Reset(); Quests.Reset();
    TArray<FText> Errors;
    if (!FDARegionalCrisisPipeline::LoadCanonical(Manifest, Errors)) return;
    TArray<UDARegionalWorldEventDefinition*> CandidateEvents;
    TArray<UDARegionalQuestDefinition*> CandidateQuests;
    for (const auto& Entry : Manifest.Events)
        CandidateEvents.Add(LoadObject<UDARegionalWorldEventDefinition>(nullptr,
            *(Entry.AssetPath + TEXT(".") + FPackageName::GetLongPackageAssetName(Entry.AssetPath))));
    for (const auto& Entry : Manifest.Quests)
        CandidateQuests.Add(LoadObject<UDARegionalQuestDefinition>(nullptr,
            *(Entry.AssetPath + TEXT(".") + FPackageName::GetLongPackageAssetName(Entry.AssetPath))));
    TArray<FText> CacheErrors;
    const bool bAnyCacheMissing = CandidateEvents.Contains(nullptr) || CandidateQuests.Contains(nullptr);
    bUsedGeneratedCache = FDARegionalCrisisPipeline::ValidateGeneratedCache(
        Manifest, CandidateEvents, CandidateQuests, CacheErrors);
    GeneratedCacheState = bUsedGeneratedCache ? EDARegionalGeneratedCacheState::Valid
        : bAnyCacheMissing ? EDARegionalGeneratedCacheState::NoCache
        : EDARegionalGeneratedCacheState::Stale;
    if (!bUsedGeneratedCache)
    {
        CandidateEvents.Reset(); CandidateQuests.Reset();
        if (!FDARegionalCrisisPipeline::BuildAssets(Manifest, CandidateEvents, CandidateQuests, Errors)) return;
    }
    for (auto* Event : CandidateEvents) Events.Add(Event);
    for (auto* Quest : CandidateQuests) Quests.Add(Quest);
    bReady = true;
}
