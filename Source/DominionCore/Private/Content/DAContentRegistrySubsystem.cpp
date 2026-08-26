#include "Content/DAContentRegistrySubsystem.h"

#include "Content/DACardDefinition.h"
#include "Content/DACivilizationDefinition.h"
#include "Content/DAContentManifest.h"
#include "Content/DAStarterDeckDefinition.h"
#include "Engine/AssetManager.h"

#define LOCTEXT_NAMESPACE "DAContentRegistry"

const FPrimaryAssetType UDA_CivilizationDefinition::AssetType(TEXT("CivilizationDefinition"));

void UDAContentRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    RebuildRegistry();

#if !UE_BUILD_SHIPPING
    TArray<FText> Errors;
    if (!ValidateRegistry(Errors))
    {
        for (const FText& Error : Errors)
        {
            UE_LOG(LogTemp, Error, TEXT("Content registry validation: %s"), *Error.ToString());
        }

        ensureMsgf(false, TEXT("Content registry validation failed. See log for details."));
    }
#endif
}

UDA_CardDefinition* UDAContentRegistrySubsystem::GetCardDefinition(const FPrimaryAssetId& AssetId) const
{
    const TSoftObjectPtr<UDA_CardDefinition>* Definition = CardDefinitionsByPrimaryAssetId.Find(AssetId);
    return Definition ? Definition->LoadSynchronous() : nullptr;
}

UDA_CardDefinition* UDAContentRegistrySubsystem::GetCardDefinition(const FName DefinitionId) const
{
    const TSoftObjectPtr<UDA_CardDefinition>* Definition = CardDefinitionsByStableId.Find(DefinitionId);
    return Definition ? Definition->LoadSynchronous() : nullptr;
}

bool UDAContentRegistrySubsystem::ValidateRegistry(TArray<FText>& Errors) const
{
    const int32 InitialErrorCount = Errors.Num();
    Errors.Append(IndexingErrors);

    TSet<FName> SeenDefinitionIds;
    for (const TSoftObjectPtr<UDA_CardDefinition>& SoftDefinition : RegisteredCardDefinitions)
    {
        UDA_CardDefinition* Definition = SoftDefinition.LoadSynchronous();
        if (!Definition)
        {
            Errors.Add(LOCTEXT("UnreadableCardDefinition", "A registered card definition could not be loaded for validation."));
            continue;
        }

        Definition->Validate(Errors);

        if (!Definition->DefinitionId.IsNone() && SeenDefinitionIds.Contains(Definition->DefinitionId))
        {
            Errors.Add(FText::Format(
                LOCTEXT("DuplicateCardDefinitionId", "Duplicate card definition ID '{0}' found in the content registry."),
                FText::FromName(Definition->DefinitionId)));
        }
        else
        {
            SeenDefinitionIds.Add(Definition->DefinitionId);
        }
    }

    return Errors.Num() == InitialErrorCount;
}

bool UDAContentRegistrySubsystem::RebuildFromDefinitions(
    const TArray<UDA_CardDefinition*>& Definitions,
    TArray<FText>& Errors)
{
    RegisteredCardDefinitions.Reset();
    CardDefinitionsByStableId.Reset();
    CardDefinitionsByPrimaryAssetId.Reset();
    CivilizationDefinitionsByStableId.Reset();
    IndexingErrors.Reset();
    RuntimeManifestDefinitions.Reset();
    StarterCollectionTemplate = FDACollectionState{};
    StarterDeckTemplate = FDADeckState{};
    StarterContentFingerprint.Reset();
    bHasStarterCampaignContent = false;

    for (UDA_CardDefinition* Definition : Definitions)
    {
        if (Definition)
        {
            RuntimeManifestDefinitions.Add(Definition);
            IndexCardDefinition(Definition, Definition->GetPrimaryAssetId());
        }
        else
        {
            IndexingErrors.Add(LOCTEXT("NullRuntimeDefinition", "A null runtime manifest definition was supplied."));
        }
    }

    const int32 InitialErrorCount = Errors.Num();
    ValidateRegistry(Errors);
    return Errors.Num() == InitialErrorCount;
}

bool UDAContentRegistrySubsystem::RebuildFromGeneratedCache(
    const TArray<UDA_CardDefinition*>& CandidateDefinitions,
    const UDA_DeckDefinition* CandidateDeck,
    bool& bOutUsedGeneratedCache,
    TArray<FText>& Errors)
{
    const int32 InitialErrorCount = Errors.Num();
    bOutUsedGeneratedCache = false;
    RegisteredCardDefinitions.Reset();
    CardDefinitionsByStableId.Reset();
    CardDefinitionsByPrimaryAssetId.Reset();
    CivilizationDefinitionsByStableId.Reset();
    IndexingErrors.Reset();
    RuntimeManifestDefinitions.Reset();

    FDAVerticalSliceContentManifest Manifest;
    FDABuiltManifestContent Canonical;
    if (!FDAContentManifestPipeline::LoadCanonical(Manifest, Errors)
        || !FDAContentManifestPipeline::BuildRuntimeContent(Manifest, Canonical, Errors))
    {
        return false;
    }

    TArray<FText> CacheDiagnostics;
    bOutUsedGeneratedCache = FDAContentManifestPipeline::ValidateGeneratedCache(
        Manifest,
        CandidateDefinitions,
        CandidateDeck,
        CacheDiagnostics);
    if (!bOutUsedGeneratedCache && (!CandidateDefinitions.IsEmpty() || CandidateDeck != nullptr))
    {
        UE_LOG(LogTemp, Warning, TEXT("Generated content cache is partial or stale; using canonical manifest fallback."));
        for (const FText& Diagnostic : CacheDiagnostics)
        {
            UE_LOG(LogTemp, Warning, TEXT("Generated content cache: %s"), *Diagnostic.ToString());
        }
    }

    const TArray<UDA_CardDefinition*>& SelectedDefinitions = bOutUsedGeneratedCache
        ? CandidateDefinitions
        : Canonical.Definitions;
    if (!RebuildFromDefinitions(SelectedDefinitions, Errors))
    {
        return false;
    }
    StarterCollectionTemplate = Canonical.Collection;
    StarterDeckTemplate = Canonical.Deck;
    StarterDeckTemplate.BindCollection(StarterCollectionTemplate);
    StarterContentFingerprint = Manifest.SourceFingerprint;
    bHasStarterCampaignContent = StarterCollectionTemplate.Instances.Num() == FDADeckState::RequiredDeckSize
        && StarterDeckTemplate.GetInstanceIds().Num() == FDADeckState::RequiredDeckSize;
    if (!bHasStarterCampaignContent)
    {
        Errors.Add(LOCTEXT("InvalidStarterCampaignContent",
            "Canonical manifest did not produce the required 60-instance starter collection/deck."));
        return false;
    }
    return Errors.Num() == InitialErrorCount;
}

bool UDAContentRegistrySubsystem::BuildStarterCampaignContent(
    FDACollectionState& OutCollection,
    FDADeckState& OutDeck,
    FString& OutError) const
{
    if (!bHasStarterCampaignContent || StarterContentFingerprint.IsEmpty())
    {
        OutError = TEXT("Content registry has no validated authored starter campaign output.");
        return false;
    }
    OutCollection = StarterCollectionTemplate;
    OutDeck = StarterDeckTemplate;
    OutDeck.BindCollection(OutCollection);
    if (OutCollection.Instances.Num() != FDADeckState::RequiredDeckSize
        || OutDeck.GetInstanceIds().Num() != FDADeckState::RequiredDeckSize)
    {
        OutError = TEXT("Authored starter campaign output lost its exact 60-card invariant.");
        return false;
    }
    return true;
}

void UDAContentRegistrySubsystem::RebuildRegistry()
{
    UAssetManager& AssetManager = UAssetManager::Get();
    TArray<FPrimaryAssetId> CardAssetIds;
    AssetManager.GetPrimaryAssetIdList(UDA_CardDefinition::AssetType, CardAssetIds);
    TArray<UDA_CardDefinition*> CandidateDefinitions;
    for (const FPrimaryAssetId& AssetId : CardAssetIds)
    {
        const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
        UDA_CardDefinition* Definition = Cast<UDA_CardDefinition>(AssetPath.TryLoad());
        if (Definition)
        {
            CandidateDefinitions.Add(Definition);
        }
    }

    TArray<FPrimaryAssetId> DeckAssetIds;
    AssetManager.GetPrimaryAssetIdList(UDA_DeckDefinition::AssetType, DeckAssetIds);
    UDA_DeckDefinition* CandidateDeck = nullptr;
    if (DeckAssetIds.Num() == 1)
    {
        CandidateDeck = Cast<UDA_DeckDefinition>(AssetManager.GetPrimaryAssetPath(DeckAssetIds[0]).TryLoad());
    }

    TArray<FText> ManifestErrors;
    bool bUsedGeneratedCache = false;
    RebuildFromGeneratedCache(CandidateDefinitions, CandidateDeck, bUsedGeneratedCache, ManifestErrors);
    IndexingErrors.Append(ManifestErrors);

    TArray<FPrimaryAssetId> CivilizationAssetIds;
    AssetManager.GetPrimaryAssetIdList(UDA_CivilizationDefinition::AssetType, CivilizationAssetIds);
    for (const FPrimaryAssetId& AssetId : CivilizationAssetIds)
    {
        const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
        UDA_CivilizationDefinition* Definition = Cast<UDA_CivilizationDefinition>(AssetPath.TryLoad());
        if (!Definition)
        {
            IndexingErrors.Add(FText::Format(
                LOCTEXT("UnableToLoadCivilizationDefinition", "Civilization primary asset '{0}' could not be loaded."),
                FText::FromString(AssetId.ToString())));
            continue;
        }

        if (Definition->DefinitionId.IsNone())
        {
            IndexingErrors.Add(LOCTEXT("EmptyCivilizationDefinitionId", "Civilization definition has an empty stable DefinitionId."));
            continue;
        }

        if (CivilizationDefinitionsByStableId.Contains(Definition->DefinitionId))
        {
            IndexingErrors.Add(FText::Format(
                LOCTEXT("DuplicateCivilizationDefinitionId", "Duplicate civilization definition ID '{0}' found in the content registry."),
                FText::FromName(Definition->DefinitionId)));
            continue;
        }

        CivilizationDefinitionsByStableId.Add(Definition->DefinitionId, Definition);
    }
}

void UDAContentRegistrySubsystem::IndexCardDefinition(UDA_CardDefinition* Definition, const FPrimaryAssetId& AssetId)
{
    if (!Definition)
    {
        IndexingErrors.Add(LOCTEXT("NullCardDefinition", "A null card definition was supplied to the content registry."));
        return;
    }

    RegisteredCardDefinitions.Add(Definition);

    if (Definition->DefinitionId.IsNone())
    {
        return;
    }

    if (CardDefinitionsByStableId.Contains(Definition->DefinitionId))
    {
        IndexingErrors.Add(FText::Format(
            LOCTEXT("DuplicateIndexedCardDefinitionId", "Duplicate card definition ID '{0}' was rejected while indexing the content registry."),
            FText::FromName(Definition->DefinitionId)));
        return;
    }

    CardDefinitionsByStableId.Add(Definition->DefinitionId, Definition);
    CardDefinitionsByPrimaryAssetId.Add(AssetId, Definition);
}

#undef LOCTEXT_NAMESPACE
