#include "DAUIAssetCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Manifest/DAUIManifest.h"
#include "Manifest/DAUIGeneratedMetadata.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "Widgets/DAGrayboxWidgets.h"

namespace
{
    bool SaveTopLevelAsset(UPackage* Package, UObject* Asset, FString& OutError)
    {
        if (Package == nullptr || Asset == nullptr) return false;
        Asset->SetFlags(RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Asset);
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
        {
            OutError = TEXT("Could not save generated UI cache: ") + Filename;
            return false;
        }
        return true;
    }

    bool GenerateWidget(const FSoftObjectPath& AssetPath, const FSoftClassPath& ParentClassPath, FString& OutError)
    {
        const FString PackageName = AssetPath.GetLongPackageName();
        UObject* ExistingObject = AssetPath.TryLoad();
        UWidgetBlueprint* Existing = Cast<UWidgetBlueprint>(ExistingObject);
        if (ExistingObject != nullptr && Existing == nullptr)
        { OutError = TEXT("Refusing to overwrite foreign UI asset: ") + AssetPath.ToString(); return false; }
        UPackage* Package = Existing != nullptr ? Existing->GetPackage() : CreatePackage(*PackageName);
        const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
        UClass* ParentClass = ParentClassPath.TryLoadClass<UDAActivatableScreen>();
        if (Package == nullptr || ParentClass == nullptr)
        {
            OutError = TEXT("Could not resolve generated widget package or source class: ") + PackageName;
            return false;
        }
        if (Existing != nullptr && Existing->ParentClass != ParentClass)
        { OutError = TEXT("Refusing to rewrite stale/foreign Widget Blueprint parent: ") + AssetPath.ToString(); return false; }
        if (Existing != nullptr && (!Existing->UbergraphPages.IsEmpty() || !Existing->FunctionGraphs.IsEmpty()
            || !Existing->MacroGraphs.IsEmpty() || !Existing->DelegateSignatureGraphs.IsEmpty()
            || (Existing->SimpleConstructionScript != nullptr
                && !Existing->SimpleConstructionScript->GetAllNodes().IsEmpty())
            || (Existing->WidgetTree != nullptr && Existing->WidgetTree->RootWidget != nullptr)))
        { OutError = TEXT("Refusing to rewrite a generated Widget Blueprint with foreign semantic content: ")
            + AssetPath.ToString(); return false; }
        UBlueprint* Blueprint = Existing != nullptr ? Existing : FKismetEditorUtilities::CreateBlueprint(
            ParentClass, Package, AssetName, BPTYPE_Normal, UWidgetBlueprint::StaticClass(),
            UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("DAUIAssetCommandlet"));
        if (Blueprint == nullptr)
        {
            OutError = TEXT("Could not create generated Widget Blueprint: ") + PackageName;
            return false;
        }
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        return SaveTopLevelAsset(Package, Blueprint, OutError);
    }

    UInputAction* GenerateInputAction(const FDAUICampaignActionDescriptor& Descriptor,
        const FDAUIInputBindingDescriptor& Binding, FString& OutError)
    {
        const FSoftObjectPath AssetPath(Binding.AssetPath);
        UObject* ExistingObject = AssetPath.TryLoad();
        UInputAction* Action = Cast<UInputAction>(ExistingObject);
        if (ExistingObject != nullptr && Action == nullptr)
        { OutError = TEXT("Refusing to overwrite foreign input action: ") + AssetPath.ToString(); return nullptr; }
        if (Action == nullptr)
        {
            const FString PackageName = AssetPath.GetLongPackageName();
            UPackage* Package = CreatePackage(*PackageName);
            Action = NewObject<UInputAction>(Package, FName(*FPackageName::GetLongPackageAssetName(PackageName)),
                RF_Public | RF_Standalone);
        }
        Action->ValueType = FDAUIGeneratedCache::GetExpectedValueType(Descriptor);
        Action->Triggers.Reset();
        Action->Modifiers.Reset();
        Action->bConsumeInput = true;
        Action->bConsumesActionAndAxisMappings = false;
        Action->bTriggerWhenPaused = false;
        Action->bReserveAllMappings = false;
        Action->AccumulationBehavior = EInputActionAccumulationBehavior::TakeHighestAbsoluteValue;
        return SaveTopLevelAsset(Action->GetPackage(), Action, OutError) ? Action : nullptr;
    }

    bool GenerateInputContext(const FDAUIInputContextDescriptor& Descriptor, const FDAUIManifest& Manifest,
        const TMap<FName, UInputAction*>& BindingsById, FString& OutError)
    {
        const FString PackageName = Descriptor.AssetPath.GetLongPackageName();
        UObject* ExistingObject = Descriptor.AssetPath.TryLoad();
        UInputMappingContext* Context = Cast<UInputMappingContext>(ExistingObject);
        if (ExistingObject != nullptr && Context == nullptr)
        { OutError = TEXT("Refusing to overwrite foreign input context: ") + Descriptor.AssetPath.ToString(); return false; }
        UPackage* Package = Context != nullptr ? Context->GetPackage() : CreatePackage(*PackageName);
        const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
        if (Context == nullptr && Package != nullptr)
            Context = NewObject<UInputMappingContext>(Package, AssetName, RF_Public | RF_Standalone);
        if (Context == nullptr)
        {
            OutError = TEXT("Could not create generated input context: ") + PackageName;
            return false;
        }
        Context->UnmapAll();
        for (const FDAUICampaignActionDescriptor& Action : Manifest.CampaignCriticalActions)
        {
            const bool bUsesContext = Action.Surfaces.ContainsByPredicate([&Manifest, &Descriptor](const FName SurfaceId)
            { const FDAUIScreenDescriptor* Screen = Manifest.FindScreen(SurfaceId); return Screen != nullptr && Screen->InputContextId == Descriptor.Id; });
            if (!bUsesContext) continue;
            const TArray<FDAUIInputBindingDescriptor> Bindings =
                FDAUIGeneratedCache::GetBindingDescriptors(Action, OutError);
            if (!OutError.IsEmpty()) return false;
            for (const FDAUIInputBindingDescriptor& Binding : Bindings)
            {
                UInputAction* const* InputAction = BindingsById.Find(Binding.BindingId);
                if (InputAction == nullptr || *InputAction == nullptr)
                { OutError = TEXT("Generated per-key input action is missing from the generation pass."); return false; }
                if (!FDAUIGeneratedCache::MapFrozenBinding(
                    *Context, **InputAction, Action, Binding, OutError)) return false;
            }
        }
        return SaveTopLevelAsset(Package, Context, OutError);
    }

    bool GenerateMetadata(const FDAUIManifest& Manifest, FString& OutError)
    {
        const FSoftObjectPath Path(FDAUIGeneratedCache::MetadataObjectPath());
        UObject* ExistingObject = Path.TryLoad();
        UDAUIGeneratedManifestMetadata* Metadata = Cast<UDAUIGeneratedManifestMetadata>(ExistingObject);
        if (ExistingObject != nullptr && Metadata == nullptr)
        { OutError = TEXT("Refusing to overwrite foreign generated UI metadata."); return false; }
        if (Metadata == nullptr)
        {
            UPackage* Package = CreatePackage(*Path.GetLongPackageName());
            Metadata = NewObject<UDAUIGeneratedManifestMetadata>(Package,
                FName(*FPackageName::GetLongPackageAssetName(Path.GetLongPackageName())), RF_Public | RF_Standalone);
        }
        Metadata->SourceFingerprint = Manifest.Fingerprint; Metadata->ManifestContentHash = Manifest.Fingerprint;
        Metadata->Screens.Reset(); Metadata->Overlays.Reset(); Metadata->InputContexts.Reset(); Metadata->InputActions.Reset();
        for (const FDAUIScreenDescriptor& Screen : Manifest.Screens)
        { auto& Row = Metadata->Screens.Emplace_GetRef(); Row.Id = Screen.Id; Row.AssetPath = Screen.AssetPath;
          Row.SourceClass = Screen.WidgetClass; Row.SemanticHash = FDAUIGeneratedCache::ScreenSemanticHash(Screen); }
        for (const FDAUIOverlayDescriptor& Overlay : Manifest.Overlays)
        { auto& Row = Metadata->Overlays.Emplace_GetRef(); Row.Id = Overlay.Id; Row.AssetPath = Overlay.AssetPath;
          Row.SourceClass = Overlay.WidgetClass; Row.SemanticHash = FDAUIGeneratedCache::OverlaySemanticHash(Overlay); }
        for (const FDAUICampaignActionDescriptor& Action : Manifest.CampaignCriticalActions)
        {
            const TArray<FDAUIInputBindingDescriptor> Bindings =
                FDAUIGeneratedCache::GetBindingDescriptors(Action, OutError);
            if (!OutError.IsEmpty()) return false;
            for (const FDAUIInputBindingDescriptor& Binding : Bindings)
            {
                auto& Row = Metadata->InputActions.Emplace_GetRef();
                Row.Id = Binding.BindingId; Row.ActionId = Action.Id; Row.AssetPath = Binding.AssetPath;
                Row.ValueType = static_cast<uint8>(FDAUIGeneratedCache::GetExpectedValueType(Action));
                Row.Key = Binding.Key; Row.PayloadIndex = Binding.PayloadIndex;
                Row.bController = Binding.bController; Row.bNegate = Binding.bNegate;
                Row.SemanticHash = FDAUIGeneratedCache::BindingSemanticHash(Action, Binding);
            }
        }
        for (const FDAUIInputContextDescriptor& Context : Manifest.InputContexts)
        {
            auto& Row = Metadata->InputContexts.Emplace_GetRef(); Row.ContextId = Context.Id; Row.AssetPath = Context.AssetPath;
            for (const FDAUICampaignActionDescriptor& Action : Manifest.CampaignCriticalActions)
                if (Action.Surfaces.ContainsByPredicate([&Manifest, &Context](const FName SurfaceId)
                    { const FDAUIScreenDescriptor* Screen = Manifest.FindScreen(SurfaceId); return Screen != nullptr && Screen->InputContextId == Context.Id; }))
                {
                    const TArray<FDAUIInputBindingDescriptor> Bindings =
                        FDAUIGeneratedCache::GetBindingDescriptors(Action, OutError);
                    if (!OutError.IsEmpty()) return false;
                    for (const FDAUIInputBindingDescriptor& Binding : Bindings)
                        Row.MappingSignatures.Add(Binding.BindingId.ToString() + TEXT("|")
                            + Binding.Key.ToString() + TEXT("|") + FString::FromInt(Binding.PayloadIndex));
                }
        }
        if (!FDAUIGeneratedCache::ValidateMetadata(Manifest, *Metadata, OutError)) return false;
        return SaveTopLevelAsset(Metadata->GetPackage(), Metadata, OutError);
    }

    bool PreflightGeneration(const FDAUIManifest& Manifest, FString& OutError)
    {
        TSet<FSoftObjectPath> AllowedAssets;
        AllowedAssets.Add(FSoftObjectPath(FDAUIGeneratedCache::MetadataObjectPath()));
        for (const FDAUIScreenDescriptor& Screen : Manifest.Screens) AllowedAssets.Add(Screen.AssetPath);
        for (const FDAUIOverlayDescriptor& Overlay : Manifest.Overlays) AllowedAssets.Add(Overlay.AssetPath);
        for (const FDAUIInputContextDescriptor& Context : Manifest.InputContexts) AllowedAssets.Add(Context.AssetPath);
        for (const FDAUICampaignActionDescriptor& Action : Manifest.CampaignCriticalActions)
        {
            TArray<FDAUIInputBindingDescriptor> Bindings =
                FDAUIGeneratedCache::GetBindingDescriptors(Action, OutError);
            if (!OutError.IsEmpty()) return false;
            for (const FDAUIInputBindingDescriptor& Binding : Bindings) AllowedAssets.Add(Binding.AssetPath);
        }
        TArray<FAssetData> NamespaceAssets;
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
            .Get().GetAssetsByPath(FName(TEXT("/Game/UI")), NamespaceAssets, true, false);
        for (const FAssetData& Asset : NamespaceAssets)
            if (!AllowedAssets.Contains(Asset.GetSoftObjectPath()))
            { OutError = TEXT("Foreign asset occupies the generated /Game/UI namespace: ")
                + Asset.GetSoftObjectPath().ToString(); return false; }
        const auto ValidateTrackedWidget = [&OutError](const FSoftObjectPath& Path,
            const FSoftClassPath& ExpectedParent)
        {
            UObject* ExistingObject = Path.TryLoad();
            if (ExistingObject == nullptr) return true;
            UWidgetBlueprint* Existing = Cast<UWidgetBlueprint>(ExistingObject);
            UClass* Parent = ExpectedParent.TryLoadClass<UDAActivatableScreen>();
            if (Existing == nullptr || Parent == nullptr || Existing->ParentClass != Parent
                || !Existing->UbergraphPages.IsEmpty() || !Existing->FunctionGraphs.IsEmpty()
                || !Existing->MacroGraphs.IsEmpty() || !Existing->DelegateSignatureGraphs.IsEmpty()
                || (Existing->SimpleConstructionScript != nullptr
                    && !Existing->SimpleConstructionScript->GetAllNodes().IsEmpty())
                || (Existing->WidgetTree != nullptr && Existing->WidgetTree->RootWidget != nullptr))
            { OutError = TEXT("Tracked generated Widget Blueprint has foreign semantic content: ")
                + Path.ToString(); return false; }
            return true;
        };
        for (const FDAUIScreenDescriptor& Screen : Manifest.Screens)
            if (!ValidateTrackedWidget(Screen.AssetPath, Screen.WidgetClass)) return false;
        for (const FDAUIOverlayDescriptor& Overlay : Manifest.Overlays)
            if (!ValidateTrackedWidget(Overlay.AssetPath, Overlay.WidgetClass)) return false;
        UObject* MetadataObject = FSoftObjectPath(FDAUIGeneratedCache::MetadataObjectPath()).TryLoad();
        if (MetadataObject != nullptr)
        {
            const UDAUIGeneratedManifestMetadata* Metadata = Cast<UDAUIGeneratedManifestMetadata>(MetadataObject);
            if (Metadata == nullptr || !FDAUIGeneratedCache::ValidateMetadata(Manifest, *Metadata, OutError))
            { if (OutError.IsEmpty()) OutError = TEXT("Generated metadata path contains a foreign asset."); return false; }
            return true;
        }
        for (const FDAUIScreenDescriptor& Screen : Manifest.Screens)
            if (Screen.AssetPath.TryLoad() != nullptr)
            { OutError = TEXT("Untracked screen asset occupies a generated cache path."); return false; }
        for (const FDAUIOverlayDescriptor& Overlay : Manifest.Overlays)
            if (Overlay.AssetPath.TryLoad() != nullptr)
            { OutError = TEXT("Untracked overlay asset occupies a generated cache path."); return false; }
        for (const FDAUIInputContextDescriptor& Context : Manifest.InputContexts)
            if (Context.AssetPath.TryLoad() != nullptr)
            { OutError = TEXT("Untracked input context occupies a generated cache path."); return false; }
        for (const FDAUICampaignActionDescriptor& Action : Manifest.CampaignCriticalActions)
        {
            const TArray<FDAUIInputBindingDescriptor> Bindings =
                FDAUIGeneratedCache::GetBindingDescriptors(Action, OutError);
            if (!OutError.IsEmpty()) return false;
            for (const FDAUIInputBindingDescriptor& Binding : Bindings)
                if (Binding.AssetPath.TryLoad() != nullptr)
                { OutError = TEXT("Untracked input action occupies a generated cache path."); return false; }
        }
        return true;
    }
}

UDAUIAssetCommandlet::UDAUIAssetCommandlet()
{
    IsClient = false; IsEditor = true; IsServer = false; LogToConsole = true;
}

int32 UDAUIAssetCommandlet::Main(const FString& Params)
{
    FModuleManager::Get().LoadModule(TEXT("DominionUI"));
    FString ManifestPath;
    FParse::Value(*Params, TEXT("Manifest="), ManifestPath);
    if (ManifestPath.IsEmpty()) ManifestPath = FDAUIManifest::GetCanonicalPath();
    FDAUIManifest Manifest; TArray<FText> Errors;
    if (!FDAUIManifest::LoadFile(ManifestPath, Manifest, Errors))
    {
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return 1;
    }
    if (FParse::Param(*Params, TEXT("ValidateOnly")))
    {
        FString CacheError;
        if (!PreflightGeneration(Manifest, CacheError)
            || !FDAUIGeneratedCache::ValidateInstalledCache(Manifest, CacheError))
        { UE_LOG(LogTemp, Error, TEXT("%s"), *CacheError); return 1; }
        UE_LOG(LogTemp, Display, TEXT("Validated exact fingerprint/ID/path/class/action parity for the generated UI cache."));
        return 0;
    }
    FString Error;
    if (!PreflightGeneration(Manifest, Error))
    { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return 1; }
    // Write the exact provenance marker first.  A failed partial run can then be resumed,
    // while a cache with no matching marker is still treated as foreign and never overwritten.
    if (!GenerateMetadata(Manifest, Error))
    { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return 1; }
    for (const FDAUIScreenDescriptor& Screen : Manifest.Screens)
        if (!GenerateWidget(Screen.AssetPath, Screen.WidgetClass, Error))
        { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return 1; }
    for (const FDAUIOverlayDescriptor& Overlay : Manifest.Overlays)
        if (!GenerateWidget(Overlay.AssetPath, Overlay.WidgetClass, Error))
        { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return 1; }
    TMap<FName, UInputAction*> BindingsById;
    for (const FDAUICampaignActionDescriptor& Action : Manifest.CampaignCriticalActions)
    {
        const TArray<FDAUIInputBindingDescriptor> Bindings =
            FDAUIGeneratedCache::GetBindingDescriptors(Action, Error);
        if (!Error.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return 1; }
        for (const FDAUIInputBindingDescriptor& Binding : Bindings)
        {
            UInputAction* Generated = GenerateInputAction(Action, Binding, Error);
            if (Generated == nullptr) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return 1; }
            BindingsById.Add(Binding.BindingId, Generated);
        }
    }
    for (const FDAUIInputContextDescriptor& Context : Manifest.InputContexts)
        if (!GenerateInputContext(Context, Manifest, BindingsById, Error))
        { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return 1; }
    if (!GenerateMetadata(Manifest, Error))
    { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return 1; }
    if (!FDAUIGeneratedCache::ValidateInstalledCache(Manifest, Error))
    { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return 1; }
    UE_LOG(LogTemp, Display, TEXT("Generated Task 21 UI caches at canonical Content/UI paths."));
    return 0;
}
