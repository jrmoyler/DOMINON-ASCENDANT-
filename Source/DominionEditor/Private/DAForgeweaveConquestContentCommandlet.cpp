#include "DAForgeweaveConquestContentCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Conquest/DAConquestSystem.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"

namespace
{
    bool SaveQuest(UDARegionalQuestDefinition* Source, const FString& PackageName, TArray<FText>& Errors)
    {
        UPackage* Package = CreatePackage(*PackageName);
        UObject* Asset = Package == nullptr ? nullptr : StaticDuplicateObject(
            Source, Package, FName(*FPackageName::GetLongPackageAssetName(PackageName)));
        if (Asset == nullptr)
        { Errors.Add(FText::FromString(TEXT("Could not create ") + PackageName)); return false; }
        Asset->SetFlags(RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Asset);
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(
            PackageName, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
        { Errors.Add(FText::FromString(TEXT("Could not save ") + Filename)); return false; }
        return true;
    }
}

UDAForgeweaveConquestContentCommandlet::UDAForgeweaveConquestContentCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UDAForgeweaveConquestContentCommandlet::Main(const FString& Params)
{
    FModuleManager::Get().LoadModule(TEXT("DominionWorld"));
    FString ManifestPath;
    FParse::Value(*Params, TEXT("Manifest="), ManifestPath);
    if (ManifestPath.IsEmpty()) ManifestPath = FDAForgeweaveConquestQuestPipeline::GetCanonicalManifestPath();
    FDAForgeweaveConquestQuestManifest Manifest;
    TArray<FText> Errors;
    TArray<UDARegionalQuestDefinition*> Quests;
    if (!FDAForgeweaveConquestQuestPipeline::LoadFile(ManifestPath, Manifest, Errors)
        || !FDAForgeweaveConquestQuestPipeline::BuildAssets(Manifest, Quests, Errors)) return 1;
    if (FParse::Param(*Params, TEXT("ValidateOnly")))
    {
        Quests.Reset();
        for (const FDARegionalQuestEntry& Entry : Manifest.Quests)
            Quests.Add(LoadObject<UDARegionalQuestDefinition>(nullptr,
                *(Entry.AssetPath + TEXT(".") + FPackageName::GetLongPackageAssetName(Entry.AssetPath))));
        if (Quests.Contains(nullptr))
        {
            Errors.Add(FText::FromString(TEXT("NoCache: conquest quest packages are absent; run generation before ValidateOnly/cook.")));
            for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
            return 2;
        }
        const bool bValid = FDAForgeweaveConquestQuestPipeline::ValidateGeneratedCache(Manifest, Quests, Errors);
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return bValid ? 0 : 1;
    }
    for (int32 Index = 0; Index < Quests.Num(); ++Index)
    {
        Quests[Index]->bRuntimeManifestFallback = false;
        SaveQuest(Quests[Index], Manifest.Quests[Index].AssetPath, Errors);
    }
    for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
    return Errors.IsEmpty() ? 0 : 1;
}
