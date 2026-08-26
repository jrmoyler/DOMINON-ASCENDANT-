#include "DAContentManifestCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Cards/DADeckRules.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentManifest.h"
#include "Content/DAContentRegistrySubsystem.h"
#include "Content/DAStarterDeckDefinition.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    bool SaveGeneratedAsset(UObject* Source, const FString& PackageName, TArray<FText>& Errors)
    {
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            Errors.Add(FText::FromString(FString::Printf(TEXT("Could not create package %s."), *PackageName)));
            return false;
        }

        const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
        UObject* Asset = StaticDuplicateObject(Source, Package, FName(*AssetName));
        if (!Asset)
        {
            Errors.Add(FText::FromString(FString::Printf(TEXT("Could not generate asset %s."), *PackageName)));
            return false;
        }

        Asset->SetFlags(RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Asset);
        Package->MarkPackageDirty();

        const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
        {
            Errors.Add(FText::FromString(FString::Printf(TEXT("Could not save generated asset %s."), *Filename)));
            return false;
        }
        return true;
    }
}

UDAContentManifestCommandlet::UDAContentManifestCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UDAContentManifestCommandlet::Main(const FString& Params)
{
    FModuleManager::Get().LoadModule(TEXT("DominionGameplay"));

    FString ManifestPath;
    FParse::Value(*Params, TEXT("Manifest="), ManifestPath);
    if (ManifestPath.IsEmpty())
    {
        ManifestPath = FDAContentManifestPipeline::GetCanonicalManifestPath();
    }

    TArray<FText> Errors;
    FDAVerticalSliceContentManifest Manifest;
    FDABuiltManifestContent Built;
    if (!FDAContentManifestPipeline::LoadFile(ManifestPath, Manifest, Errors)
        || !FDAContentManifestPipeline::BuildRuntimeContent(Manifest, Built, Errors))
    {
        for (const FText& Error : Errors) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString()); }
        return 1;
    }

    UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
    Registry->RebuildFromDefinitions(Built.Definitions, Errors);
    FDADeckRules::Validate(Built.Deck, *Registry, Errors);
    if (!Errors.IsEmpty())
    {
        for (const FText& Error : Errors) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString()); }
        return 1;
    }

    if (FParse::Param(*Params, TEXT("ValidateOnly")))
    {
        UE_LOG(LogTemp, Display, TEXT("Validated 64 definitions and the 60-instance Synara starter deck."));
        return 0;
    }

    for (UDA_CardDefinition* Definition : Built.Definitions)
    {
        SaveGeneratedAsset(Definition, FDAContentManifestPipeline::GetGeneratedPackageName(*Definition), Errors);
    }
    SaveGeneratedAsset(Built.DeckAsset, TEXT("/Game/DA/Decks/DA_Deck_SynaraStarter60"), Errors);

    for (const FText& Error : Errors) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString()); }
    if (!Errors.IsEmpty())
    {
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("Generated 64 card definition Data Assets and DA_Deck_SynaraStarter60."));
    return 0;
}
