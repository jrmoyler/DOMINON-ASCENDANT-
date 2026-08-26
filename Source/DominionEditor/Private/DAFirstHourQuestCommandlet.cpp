#include "DAFirstHourQuestCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Content/DAFirstHourQuestContent.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    bool SaveAsset(UObject* Source, const FString& PackageName, TArray<FText>& Errors)
    {
        UPackage* Package = CreatePackage(*PackageName);
        if (Package == nullptr) { Errors.Add(FText::FromString(TEXT("Could not create ") + PackageName)); return false; }
        UObject* Asset = StaticDuplicateObject(Source, Package, FName(*FPackageName::GetLongPackageAssetName(PackageName)));
        if (Asset == nullptr) { Errors.Add(FText::FromString(TEXT("Could not duplicate ") + PackageName)); return false; }
        Asset->SetFlags(RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Asset);
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
        { Errors.Add(FText::FromString(TEXT("Could not save ") + Filename)); return false; }
        return true;
    }
}

UDAFirstHourQuestCommandlet::UDAFirstHourQuestCommandlet()
{
    IsClient = false; IsEditor = true; IsServer = false; LogToConsole = true;
}

int32 UDAFirstHourQuestCommandlet::Main(const FString& Params)
{
    FModuleManager::Get().LoadModule(TEXT("DominionWorld"));
    FString ManifestPath;
    FParse::Value(*Params, TEXT("Manifest="), ManifestPath);
    if (ManifestPath.IsEmpty()) ManifestPath = FDAFirstHourQuestPipeline::GetCanonicalManifestPath();
    FDAFirstHourQuestManifest Manifest;
    TArray<FText> Errors;
    TArray<UDA_FirstHourQuestDefinition*> Quests;
    UDA_CitizenDefinition* Citizen = nullptr;
    if (!FDAFirstHourQuestPipeline::LoadFile(ManifestPath, Manifest, Errors)
        || !FDAFirstHourQuestPipeline::BuildAssets(Manifest, Quests, Citizen, Errors))
    {
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return 1;
    }
    if (FParse::Param(*Params, TEXT("ValidateOnly")))
    {
        UE_LOG(LogTemp, Display, TEXT("Validated exactly nine first-hour quests and Nia Vale (%s)."), *Manifest.Fingerprint);
        return 0;
    }
    for (int32 Index = 0; Index < Quests.Num(); ++Index) SaveAsset(Quests[Index], Manifest.Quests[Index].AssetPath, Errors);
    SaveAsset(Citizen, Manifest.Citizen.AssetPath, Errors);
    for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
    if (!Errors.IsEmpty()) return 1;
    UE_LOG(LogTemp, Display, TEXT("Generated nine quest assets and DA_Citizen_NiaVale from %s."), *Manifest.Fingerprint);
    return 0;
}
