#include "Presentation/DAPresentationRegistrySubsystem.h"

#include "Engine/AssetManager.h"

void UDAPresentationRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    TArray<UDAPresentationDefinition*> Generated;
    TArray<FSoftObjectPath> Paths;
    UAssetManager::Get().GetPrimaryAssetPathList(
        FPrimaryAssetType(TEXT("DAPresentation")), Paths);
    for (const FSoftObjectPath& Path : Paths)
    {
        if (UDAPresentationDefinition* Definition =
            Cast<UDAPresentationDefinition>(Path.TryLoad()))
            Generated.Add(Definition);
    }
    TArray<FText> Errors;
    const bool bRebuilt = RebuildFromCanonicalAndGenerated(Generated, Errors);
    for (const FText& Error : Errors)
    {
        if (bRebuilt) UE_LOG(LogTemp, Warning, TEXT("%s"), *Error.ToString());
        else UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
    }
}

void UDAPresentationRegistrySubsystem::Deinitialize()
{
    StrongDefinitions.Reset();
    Manifest = FDAPresentationContentManifest();
    bReady = false;
    bUsedGeneratedCache = false;
    DegradedCacheWarnings.Reset();
    Super::Deinitialize();
}

bool UDAPresentationRegistrySubsystem::RebuildFromCanonicalAndGenerated(
    const TArray<UDAPresentationDefinition*>& GeneratedDefinitions,
    TArray<FText>& OutErrors)
{
    StrongDefinitions.Reset();
    Manifest = FDAPresentationContentManifest();
    bReady = false;
    bUsedGeneratedCache = false;
    DegradedCacheWarnings.Reset();
    if (!FDAPresentationContentPipeline::LoadCanonical(Manifest, OutErrors))
        return false;

    bool bCacheValid = false;
    TArray<FText> CacheErrors;
    if (!GeneratedDefinitions.IsEmpty())
        bCacheValid = FDAPresentationContentPipeline::ValidateGeneratedCache(
            Manifest, GeneratedDefinitions, bUsedGeneratedCache, CacheErrors);
    if (bCacheValid)
    {
        for (UDAPresentationDefinition* Definition : GeneratedDefinitions)
            StrongDefinitions.Add(Definition);
    }
    else
    {
        bUsedGeneratedCache = false;
        for (const FText& CacheError : CacheErrors)
        {
            const FText Warning = FText::FromString(
                TEXT("Generated presentation cache rejected; canonical fallback active: ")
                + CacheError.ToString());
            DegradedCacheWarnings.Add(Warning);
            OutErrors.Add(Warning);
        }
        TArray<UDAPresentationDefinition*> Fallback;
        if (!FDAPresentationContentPipeline::BuildRuntimeFallback(
            Manifest, Fallback, OutErrors)) return false;
        for (UDAPresentationDefinition* Definition : Fallback)
            StrongDefinitions.Add(Definition);
    }
    bReady = StrongDefinitions.Num() == Manifest.AllDefinitions().Num();
    return bReady;
}

bool UDAPresentationRegistrySubsystem::Resolve(
    const EDAPresentationDefinitionKind Kind, const FName Id,
    FDAPresentationDefinitionSource& OutDefinition) const
{
    if (!bReady) return false;
    const TObjectPtr<UDAPresentationDefinition>* Found =
        StrongDefinitions.FindByPredicate([Kind, Id](
            const TObjectPtr<UDAPresentationDefinition>& Candidate)
        {
            return Candidate != nullptr && Candidate->Definition.Kind == Kind
                && Candidate->Definition.Id == Id;
        });
    if (Found == nullptr || Found->Get() == nullptr) return false;
    OutDefinition = (*Found)->Definition;
    OutDefinition.bGeneratedCache = bUsedGeneratedCache;
    return true;
}

bool UDAPresentationRegistrySubsystem::BuildConstructionPlan(const FName Faction,
    const EDAConstructionState State, FDAPresentationPlaybackPlan& OutPlan) const
{
    return bReady && FDAPresentationRuntimeProjection::BuildConstruction(
        Manifest, Faction, State, OutPlan);
}

bool UDAPresentationRegistrySubsystem::BuildDamagePlan(const FName Faction,
    const FDAWorldAssetRecord& Asset, FDAPresentationPlaybackPlan& OutPlan) const
{
    return bReady && FDAPresentationRuntimeProjection::BuildDamage(
        Manifest, Faction, Asset, OutPlan);
}

bool UDAPresentationRegistrySubsystem::BuildCapturePlan(
    const FDAWorldAssetRecord& Asset, const FDACaptureRecord& Capture,
    FDAPresentationPlaybackPlan& OutPlan) const
{
    return bReady && FDAPresentationRuntimeProjection::BuildCapture(
        Manifest, Asset, Capture, OutPlan);
}

bool UDAPresentationRegistrySubsystem::BuildCaptureSnapshotPlan(
    const FDACapturePresentationSnapshot& Snapshot,
    FDAPresentationPlaybackPlan& OutPlan) const
{
    return bReady && FDAPresentationRuntimeProjection::BuildCapture(
        Manifest, Snapshot, OutPlan);
}

bool UDAPresentationRegistrySubsystem::BuildDaxtonPlan(
    const FDADaxtonCampaignState& Daxton, FDAPresentationPlaybackPlan& OutPlan) const
{
    return bReady && FDAPresentationRuntimeProjection::BuildDaxton(
        Manifest, Daxton, OutPlan);
}

bool UDAPresentationRegistrySubsystem::BuildAscensionPlan(
    const FDAAscensionPresentationState& Ascension,
    FDAPresentationPlaybackPlan& OutPlan) const
{
    return bReady && FDAPresentationRuntimeProjection::BuildAscension(
        Manifest, Ascension, OutPlan);
}

void UDAPresentationRegistrySubsystem::SkipPlaybackPlan(
    FDAPresentationPlaybackPlan& InOutPlan) const
{
    FDAPresentationRuntimeProjection::Skip(InOutPlan);
}
