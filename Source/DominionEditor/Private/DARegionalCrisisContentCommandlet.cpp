#include "DARegionalCrisisContentCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Content/DARegionalCrisisContent.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"

namespace
{
    bool SaveRegionalAsset(UObject* Source, const FString& PackageName, TArray<FText>& Errors)
    {
        UPackage* Package = CreatePackage(*PackageName);
        UObject* Asset = Package == nullptr ? nullptr : StaticDuplicateObject(
            Source, Package, FName(*FPackageName::GetLongPackageAssetName(PackageName)));
        if (Asset == nullptr) { Errors.Add(FText::FromString(TEXT("Could not create ") + PackageName)); return false; }
        Asset->SetFlags(RF_Public | RF_Standalone); FAssetRegistryModule::AssetCreated(Asset); Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
        { Errors.Add(FText::FromString(TEXT("Could not save ") + Filename)); return false; }
        return true;
    }
}

UDARegionalCrisisContentCommandlet::UDARegionalCrisisContentCommandlet()
{ IsClient = false; IsEditor = true; IsServer = false; LogToConsole = true; }

int32 UDARegionalCrisisContentCommandlet::Main(const FString& Params)
{
    FModuleManager::Get().LoadModule(TEXT("DominionWorld"));
    FString ManifestPath; FParse::Value(*Params, TEXT("Manifest="), ManifestPath);
    if (ManifestPath.IsEmpty()) ManifestPath = FDARegionalCrisisPipeline::GetCanonicalManifestPath();
    FDARegionalCrisisManifest Manifest; TArray<FText> Errors;
    TArray<UDARegionalWorldEventDefinition*> Events; TArray<UDARegionalQuestDefinition*> Quests;
    if (!FDARegionalCrisisPipeline::LoadFile(ManifestPath, Manifest, Errors)
        || !FDARegionalCrisisPipeline::BuildAssets(Manifest, Events, Quests, Errors)) return 1;
    if (FParse::Param(*Params, TEXT("ValidateOnly")))
    {
        Events.Reset(); Quests.Reset();
        for (const FDARegionalWorldEventEntry& Entry : Manifest.Events)
            Events.Add(LoadObject<UDARegionalWorldEventDefinition>(nullptr,
                *(Entry.AssetPath + TEXT(".") + FPackageName::GetLongPackageAssetName(Entry.AssetPath))));
        for (const FDARegionalQuestEntry& Entry : Manifest.Quests)
            Quests.Add(LoadObject<UDARegionalQuestDefinition>(nullptr,
                *(Entry.AssetPath + TEXT(".") + FPackageName::GetLongPackageAssetName(Entry.AssetPath))));
        if (Events.Contains(nullptr) || Quests.Contains(nullptr))
        {
            Errors.Add(FText::FromString(TEXT("NoCache: regional generated packages are absent; run generation before ValidateOnly/cook.")));
            for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
            return 2;
        }
        const bool bValid = FDARegionalCrisisPipeline::ValidateGeneratedCache(Manifest, Events, Quests, Errors);
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return bValid ? 0 : 1;
    }
    for (int32 Index = 0; Index < Events.Num(); ++Index)
    { Events[Index]->bRuntimeManifestFallback = false; SaveRegionalAsset(Events[Index], Manifest.Events[Index].AssetPath, Errors); }
    for (int32 Index = 0; Index < Quests.Num(); ++Index)
    { Quests[Index]->bRuntimeManifestFallback = false; SaveRegionalAsset(Quests[Index], Manifest.Quests[Index].AssetPath, Errors); }
    for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
    return Errors.IsEmpty() ? 0 : 1;
}
