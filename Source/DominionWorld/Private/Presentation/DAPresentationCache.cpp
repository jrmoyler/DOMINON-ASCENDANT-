#include "Presentation/DAPresentationContent.h"

namespace
{
    bool DefinitionEqual(const FDAPresentationDefinitionSource& Left,
        const FDAPresentationDefinitionSource& Right)
    {
        return Left.Id == Right.Id && Left.Kind == Right.Kind && Left.Faction == Right.Faction
            && Left.DisplayName == Right.DisplayName && Left.AssetPath == Right.AssetPath
            && Left.RecipePayload == Right.RecipePayload
            && Left.RecipeFingerprint == Right.RecipeFingerprint
            && Left.Artifacts == Right.Artifacts;
    }

    void AddCacheError(TArray<FText>& Errors, const TCHAR* Message)
    {
        Errors.Add(FText::FromString(Message));
    }
}

FPrimaryAssetId UDAPresentationDefinition::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("DAPresentation"), Definition.Id);
}

TArray<FDAPresentationDefinitionSource> FDAPresentationContentManifest::AllDefinitions() const
{
    TArray<FDAPresentationDefinitionSource> Result;
    Result.Reserve(PrimaryAssets.Num() + CoreVfx.Num() + Music.Num() + Ambient.Num() + Sfx.Num());
    Result.Append(PrimaryAssets);
    Result.Append(CoreVfx);
    Result.Append(Music);
    Result.Append(Ambient);
    Result.Append(Sfx);
    return Result;
}

const FDAConstructionPresentationGrammar* FDAPresentationContentManifest::FindConstructionGrammar(
    const FName Faction) const
{
    return ConstructionGrammars.FindByPredicate([Faction](const FDAConstructionPresentationGrammar& Row)
    { return Row.Faction == Faction; });
}

const FDADamagePresentationLanguage* FDAPresentationContentManifest::FindDamageLanguage(
    const FName Faction) const
{
    return DamageLanguages.FindByPredicate([Faction](const FDADamagePresentationLanguage& Row)
    { return Row.Faction == Faction; });
}

bool FDAPresentationContentPipeline::Resolve(const FDAPresentationContentManifest& Manifest,
    const EDAPresentationDefinitionKind Kind, const FName Id,
    FDAPresentationDefinitionSource& OutDefinition)
{
    const TArray<FDAPresentationDefinitionSource>* Rows = nullptr;
    switch (Kind)
    {
    case EDAPresentationDefinitionKind::PrimaryAsset: Rows = &Manifest.PrimaryAssets; break;
    case EDAPresentationDefinitionKind::CoreVFX: Rows = &Manifest.CoreVfx; break;
    case EDAPresentationDefinitionKind::Music: Rows = &Manifest.Music; break;
    case EDAPresentationDefinitionKind::Ambient: Rows = &Manifest.Ambient; break;
    case EDAPresentationDefinitionKind::SFX: Rows = &Manifest.Sfx; break;
    default: return false;
    }
    const FDAPresentationDefinitionSource* Found = Rows->FindByPredicate(
        [Id](const FDAPresentationDefinitionSource& Row) { return Row.Id == Id; });
    if (Found == nullptr) return false;
    OutDefinition = *Found;
    OutDefinition.bGeneratedCache = false;
    return true;
}

bool FDAPresentationContentPipeline::BuildRuntimeFallback(
    const FDAPresentationContentManifest& Manifest,
    TArray<UDAPresentationDefinition*>& OutDefinitions, TArray<FText>& OutErrors)
{
    OutDefinitions.Reset();
    if (!Validate(Manifest, OutErrors)) return false;
    for (const FDAPresentationDefinitionSource& Source : Manifest.AllDefinitions())
    {
        UDAPresentationDefinition* Definition = NewObject<UDAPresentationDefinition>(
            GetTransientPackage());
        Definition->Definition = Source;
        Definition->Definition.bGeneratedCache = false;
        Definition->SourceFingerprint = Manifest.Fingerprint;
        Definition->bRuntimeManifestFallback = true;
        OutDefinitions.Add(Definition);
    }
    return true;
}

bool FDAPresentationContentPipeline::ValidateGeneratedCache(
    const FDAPresentationContentManifest& Manifest,
    const TArray<UDAPresentationDefinition*>& Definitions, bool& bOutUsedGeneratedCache,
    TArray<FText>& OutErrors)
{
    bOutUsedGeneratedCache = false;
    const TArray<FDAPresentationDefinitionSource> Sources = Manifest.AllDefinitions();
    if (Definitions.Num() != Sources.Num())
    {
        AddCacheError(OutErrors, TEXT("Generated presentation cache is partial."));
        return false;
    }
    TMap<FName, const UDAPresentationDefinition*> ById;
    for (const UDAPresentationDefinition* Definition : Definitions)
    {
        if (Definition == nullptr || Definition->Definition.Id.IsNone()
            || ById.Contains(Definition->Definition.Id))
        {
            AddCacheError(OutErrors,
                TEXT("Generated presentation cache has null or duplicate rows."));
            return false;
        }
        ById.Add(Definition->Definition.Id, Definition);
    }
    for (const FDAPresentationDefinitionSource& Source : Sources)
    {
        const UDAPresentationDefinition* const* Candidate = ById.Find(Source.Id);
        if (Candidate == nullptr || *Candidate == nullptr
            || (*Candidate)->bRuntimeManifestFallback
            || !(*Candidate)->Definition.bGeneratedCache
            || (*Candidate)->SourceFingerprint != Manifest.Fingerprint
            || !DefinitionEqual((*Candidate)->Definition, Source))
        {
            AddCacheError(OutErrors,
                TEXT("Generated presentation cache diverges from canonical source."));
            return false;
        }
    }
    bOutUsedGeneratedCache = true;
    return true;
}
