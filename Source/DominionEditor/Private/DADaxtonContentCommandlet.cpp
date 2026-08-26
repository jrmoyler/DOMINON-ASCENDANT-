#include "DADaxtonContentCommandlet.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Boss/Daxton/DADaxtonEncounter.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"

namespace
{
    bool SaveLeader(UDADaxtonLeaderDefinition* Source, const FString& PackageName,
        TArray<FText>& Errors)
    {
        UPackage* Package = CreatePackage(*PackageName);
        UObject* Asset = Package == nullptr ? nullptr : StaticDuplicateObject(
            Source, Package, FName(*FPackageName::GetLongPackageAssetName(PackageName)));
        if (Asset == nullptr)
        {
            Errors.Add(FText::FromString(TEXT("Could not create Daxton Leader package ") + PackageName));
            return false;
        }
        CastChecked<UDADaxtonLeaderDefinition>(Asset)->bRuntimeManifestFallback = false;
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
        {
            Errors.Add(FText::FromString(TEXT("Could not save Daxton Leader package ") + Filename));
            return false;
        }
        return true;
    }

    UClass* ExpectedClass(const FName ClassName)
    {
        return ClassName == TEXT("SkeletalMesh") ? USkeletalMesh::StaticClass()
            : ClassName == TEXT("AnimSequence") ? UAnimSequence::StaticClass() : nullptr;
    }

    UObject* LoadExpectedImport(const FDADaxtonCharacterImport& Import)
    {
        const FString ObjectPath = Import.AssetPath + TEXT(".")
            + FPackageName::GetLongPackageAssetName(Import.AssetPath);
        return StaticLoadObject(ExpectedClass(Import.AssetClass), nullptr, *ObjectPath);
    }

    bool ValidateRegistryAsset(const FString& PackagePath, UClass* Expected,
        TArray<FText>& Errors)
    {
        const FString ObjectPath = PackagePath + TEXT(".")
            + FPackageName::GetLongPackageAssetName(PackagePath);
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry")).Get();
        const TArray<FString> ScanPaths = {FPackageName::GetLongPackagePath(PackagePath)};
        Registry.ScanPathsSynchronous(ScanPaths, true);
        const FAssetData AssetData = Registry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
        UObject* Loaded = AssetData.IsValid() ? AssetData.GetAsset() : nullptr;
        if (!AssetData.IsValid() || AssetData.PackageName != FName(*PackagePath)
            || Expected == nullptr || AssetData.AssetClassPath != Expected->GetClassPathName()
            || Loaded == nullptr || Loaded->GetClass() != Expected)
        {
            Errors.Add(FText::FromString(TEXT("NoRegistry: exact cook-visible Daxton asset is absent or wrong class: ")
                + PackagePath));
            return false;
        }
        return true;
    }

    bool ValidateInstalledImports(const FDADaxtonContentManifest& Manifest, TArray<FText>& Errors)
    {
        bool bValid = true;
        for (const FDADaxtonCharacterImport& Import : Manifest.CharacterImports)
        {
            UObject* Asset = LoadExpectedImport(Import);
            if (Asset == nullptr || Asset->GetClass() != ExpectedClass(Import.AssetClass))
            {
                Errors.Add(FText::FromString(TEXT("NoCache: exact Daxton character import is absent or wrong class: ")
                    + Import.AssetPath));
                bValid = false;
            }
            bValid &= ValidateRegistryAsset(Import.AssetPath,
                ExpectedClass(Import.AssetClass), Errors);
        }
        return bValid;
    }

    bool ValidateInstalledLeader(const FDADaxtonContentManifest& Manifest,
        TArray<FText>& Errors)
    {
        return ValidateRegistryAsset(Manifest.LeaderAssetPath,
            UDADaxtonLeaderDefinition::StaticClass(), Errors);
    }

    bool ImportCharacterSources(const FDADaxtonContentManifest& Manifest, TArray<FText>& Errors)
    {
        TArray<UAssetImportTask*> Tasks;
        for (const FDADaxtonCharacterImport& Import : Manifest.CharacterImports)
        {
            const FString Source = FPaths::ConvertRelativePathToFull(
                FPaths::Combine(FPaths::ProjectDir(), Import.SourcePath));
            if (!FPaths::FileExists(Source))
            {
                Errors.Add(FText::FromString(TEXT("MissingSource: supply the real authored Daxton FBX before generation: ")
                    + Import.SourcePath));
                continue;
            }
            UAssetImportTask* Task = NewObject<UAssetImportTask>();
            Task->Filename = Source;
            Task->DestinationPath = FPackageName::GetLongPackagePath(Import.AssetPath);
            Task->DestinationName = FPackageName::GetLongPackageAssetName(Import.AssetPath);
            Task->bAutomated = true;
            Task->bReplaceExisting = true;
            Task->bSave = true;
            Tasks.Add(Task);
        }
        if (Tasks.Num() != Manifest.CharacterImports.Num()) return false;
        FAssetToolsModule::GetModule().Get().ImportAssetTasks(Tasks);
        return ValidateInstalledImports(Manifest, Errors);
    }
}

UDADaxtonContentCommandlet::UDADaxtonContentCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UDADaxtonContentCommandlet::Main(const FString& Params)
{
    FModuleManager::Get().LoadModule(TEXT("DominionGameplay"));
    FString ManifestPath;
    FParse::Value(*Params, TEXT("Manifest="), ManifestPath);
    if (ManifestPath.IsEmpty()) ManifestPath = FDADaxtonContentPipeline::GetCanonicalManifestPath();
    FDADaxtonContentManifest Manifest;
    TArray<FText> Errors;
    if (!FDADaxtonContentPipeline::LoadFile(ManifestPath, Manifest, Errors))
    {
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return 1;
    }

    if (FParse::Param(*Params, TEXT("ValidateOnly")))
    {
        const FString LeaderObjectPath = Manifest.LeaderAssetPath + TEXT(".")
            + FPackageName::GetLongPackageAssetName(Manifest.LeaderAssetPath);
        const UDADaxtonLeaderDefinition* Leader = LoadObject<UDADaxtonLeaderDefinition>(nullptr, *LeaderObjectPath);
        const bool bValid = ValidateInstalledImports(Manifest, Errors)
            && ValidateInstalledLeader(Manifest, Errors)
            && FDADaxtonContentPipeline::ValidateGeneratedCache(Manifest, Leader, Errors);
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return bValid ? 0 : 2;
    }

    UDADaxtonLeaderDefinition* Leader = nullptr;
    if (!FDADaxtonContentPipeline::BuildLeaderDefinition(Manifest, Leader, Errors)
        || !ImportCharacterSources(Manifest, Errors)
        || !SaveLeader(Leader, Manifest.LeaderAssetPath, Errors)
        || !ValidateInstalledLeader(Manifest, Errors))
    {
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return 1;
    }
    for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
    return Errors.IsEmpty() ? 0 : 1;
}
