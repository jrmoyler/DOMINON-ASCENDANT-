#include "DAPresentationContentCommandlet.h"
#include "DAPresentationNiagaraEmitterValidation.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraTypes.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "Presentation/DAPresentationContent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "Engine/StaticMesh.h"
#include "UObject/MetaData.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
    constexpr TCHAR GeneratorId[] = TEXT("DAPresentationContent.v1");

    FString ObjectPath(const FString& PackagePath)
    {
        return PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
    }

    struct FDAMaterialArtifactContent
    {
        FLinearColor BaseColor;
        FLinearColor EmissiveColor;
        float Metallic = 0.f;
        float Roughness = 0.f;
    };

    struct FDANiagaraArtifactContent
    {
        FString TemplateAssetPath;
        FName ExpectedEmitterName;
        FString SimulationTarget;
        FString RendererClass;
        FString LifecycleMode;
        FString LoopBehavior;
        int32 MinimumEmitterCount = 0;
        int32 BurstCount = 0;
        float LifetimeSeconds = 0.f;
        FVector2f SpriteSize = FVector2f::ZeroVector;
        float GameplayRadiusMeters = 0.f;
        FBox Bounds = FBox(ForceInit);
        float WarmupSeconds = 0.f;
        bool bAutoDeactivate = false;
    };

    bool ParsePayload(const FString& Payload, TSharedPtr<FJsonObject>& Out)
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
        return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
    }

    bool ReadLinearColor(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        FLinearColor& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values)
            || Values == nullptr || Values->Num() != 4) return false;
        double Channels[4] = {};
        for (int32 Index = 0; Index < 4; ++Index)
            if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetNumber(Channels[Index]))
                return false;
        Out = FLinearColor(Channels[0], Channels[1], Channels[2], Channels[3]);
        return true;
    }

    bool ParseMaterialContent(
        const FDAPresentationContentManifest::FArtifactRequirement& Requirement,
        FDAMaterialArtifactContent& Out)
    {
        TSharedPtr<FJsonObject> Root;
        double Metallic = 0.0;
        double Roughness = 0.0;
        if (!ParsePayload(Requirement.SourceContentPayload, Root)
            || !ReadLinearColor(Root, TEXT("baseColor"), Out.BaseColor)
            || !ReadLinearColor(Root, TEXT("emissiveColor"), Out.EmissiveColor)
            || !Root->TryGetNumberField(TEXT("metallic"), Metallic)
            || !Root->TryGetNumberField(TEXT("roughness"), Roughness))
            return false;
        Out.Metallic = static_cast<float>(Metallic);
        Out.Roughness = static_cast<float>(Roughness);
        const auto Unit = [](const float Value)
        {
            return FMath::IsFinite(Value) && Value >= 0.f && Value <= 1.f;
        };
        return Unit(Out.BaseColor.R) && Unit(Out.BaseColor.G)
            && Unit(Out.BaseColor.B) && Unit(Out.BaseColor.A)
            && Unit(Out.EmissiveColor.R) && Unit(Out.EmissiveColor.G)
            && Unit(Out.EmissiveColor.B) && Unit(Out.EmissiveColor.A)
            && Unit(Out.Metallic) && Unit(Out.Roughness);
    }

    bool ParseNiagaraContent(
        const FDAPresentationContentManifest::FArtifactRequirement& Requirement,
        FDANiagaraArtifactContent& Out)
    {
        TSharedPtr<FJsonObject> Root;
        const TSharedPtr<FJsonObject>* Emitter = nullptr;
        const TSharedPtr<FJsonObject>* Lifecycle = nullptr;
        const TSharedPtr<FJsonObject>* Recipe = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* SpriteSize = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Bounds = nullptr;
        double MinimumEmitterCount = 0.0;
        double BurstCount = 0.0;
        double LifetimeSeconds = 0.0;
        double Radius = 0.0;
        double WarmupSeconds = 0.0;
        FString ExpectedEmitterName;
        if (!ParsePayload(Requirement.SourceContentPayload, Root)
            || !Root->TryGetObjectField(TEXT("emitter"), Emitter) || Emitter == nullptr
            || !(*Emitter)->TryGetObjectField(TEXT("lifecycle"), Lifecycle)
            || Lifecycle == nullptr
            || !Root->TryGetObjectField(TEXT("definitionParameters"), Recipe) || Recipe == nullptr
            || !(*Emitter)->TryGetStringField(TEXT("templateAssetPath"), Out.TemplateAssetPath)
            || !(*Emitter)->TryGetStringField(TEXT("expectedEmitterName"), ExpectedEmitterName)
            || !(*Emitter)->TryGetStringField(TEXT("simulationTarget"), Out.SimulationTarget)
            || !(*Emitter)->TryGetStringField(TEXT("rendererClass"), Out.RendererClass)
            || !(*Lifecycle)->TryGetStringField(TEXT("mode"), Out.LifecycleMode)
            || !(*Lifecycle)->TryGetStringField(TEXT("loopBehavior"), Out.LoopBehavior)
            || !(*Emitter)->TryGetNumberField(TEXT("minimumEmitterCount"), MinimumEmitterCount)
            || !(*Emitter)->TryGetNumberField(TEXT("burstCount"), BurstCount)
            || !(*Emitter)->TryGetNumberField(TEXT("lifetimeSeconds"), LifetimeSeconds)
            || !(*Emitter)->TryGetArrayField(TEXT("spriteSize"), SpriteSize)
            || SpriteSize == nullptr || SpriteSize->Num() != 2
            || !Root->TryGetArrayField(TEXT("bounds"), Bounds)
            || Bounds == nullptr || Bounds->Num() != 6
            || !Root->TryGetNumberField(TEXT("warmupSeconds"), WarmupSeconds)
            || !Root->TryGetBoolField(TEXT("autoDeactivate"), Out.bAutoDeactivate)
            || !(*Recipe)->TryGetNumberField(TEXT("gameplayRadiusMeters"), Radius))
            return false;
        double SpriteX = 0.0;
        double SpriteY = 0.0;
        if (!(*SpriteSize)[0]->TryGetNumber(SpriteX)
            || !(*SpriteSize)[1]->TryGetNumber(SpriteY)) return false;
        double BoundValues[6] = {};
        for (int32 Index = 0; Index < 6; ++Index)
            if (!(*Bounds)[Index]->TryGetNumber(BoundValues[Index])) return false;
        Out.MinimumEmitterCount = static_cast<int32>(MinimumEmitterCount);
        Out.ExpectedEmitterName = FName(*ExpectedEmitterName);
        Out.BurstCount = static_cast<int32>(BurstCount);
        Out.LifetimeSeconds = static_cast<float>(LifetimeSeconds);
        Out.SpriteSize = FVector2f(SpriteX, SpriteY);
        Out.GameplayRadiusMeters = static_cast<float>(Radius);
        Out.Bounds = FBox(FVector(BoundValues[0], BoundValues[1], BoundValues[2]),
            FVector(BoundValues[3], BoundValues[4], BoundValues[5]));
        Out.WarmupSeconds = static_cast<float>(WarmupSeconds);
        return !Out.TemplateAssetPath.IsEmpty() && !Out.ExpectedEmitterName.IsNone()
            && Out.SimulationTarget == TEXT("CPUSim")
            && Out.LifecycleMode == TEXT("Self") && Out.LoopBehavior == TEXT("Once")
            && !Out.RendererClass.IsEmpty() && Out.MinimumEmitterCount > 0
            && Out.BurstCount > 0 && Out.LifetimeSeconds > 0.f
            && Out.SpriteSize.X > 0.f && Out.SpriteSize.Y > 0.f
            && Out.GameplayRadiusMeters > 0.f && Out.Bounds.IsValid
            && Out.WarmupSeconds >= 0.f;
    }

    FString NormalizeNiagaraLifecycleToken(FString Value)
    {
        int32 SeparatorIndex = INDEX_NONE;
        if (Value.FindLastChar(TEXT(':'), SeparatorIndex))
            Value = Value.Mid(SeparatorIndex + 1);
        if (Value.FindLastChar(TEXT('.'), SeparatorIndex))
            Value = Value.Mid(SeparatorIndex + 1);
        Value.ReplaceInline(TEXT(" "), TEXT(""));
        Value.ReplaceInline(TEXT("_"), TEXT(""));
        Value.ReplaceInline(TEXT("-"), TEXT(""));
        Value.ToLowerInline();
        return Value;
    }

    struct FDANiagaraLifecycleInputView
    {
        FString Name;
        FString EnumValue;

        bool HasIdentity(const FString& Expected) const
        {
            return NormalizeNiagaraLifecycleToken(Name)
                == NormalizeNiagaraLifecycleToken(Expected);
        }
        bool HasEnumValue(const FString& Expected) const
        {
            return NormalizeNiagaraLifecycleToken(EnumValue)
                == NormalizeNiagaraLifecycleToken(Expected);
        }
    };

    struct FDANiagaraLifecycleModuleView
    {
        FString ScriptName;
        bool bEnabled = false;
        TArray<FDANiagaraLifecycleInputView> Inputs;

        bool IsEnabled() const { return bEnabled; }
        bool HasIdentity(const FString& Expected) const
        {
            return NormalizeNiagaraLifecycleToken(ScriptName)
                == NormalizeNiagaraLifecycleToken(Expected);
        }
        const TArray<FDANiagaraLifecycleInputView>& GetInputs() const
        {
            return Inputs;
        }
    };

    TArray<FDANiagaraLifecycleModuleView> ReadPersistedEmitterLifecycleModules(
        UNiagaraSystem& System, const FName EmitterName)
    {
        TArray<FDANiagaraLifecycleModuleView> Result;
        FNiagaraExternalEditContext Context(&System);
        const FNiagaraExt_StackItemReference EmitterReference(
            &System, EmitterName, NAME_None, NAME_None);
        FNiagaraExt_EmitterTopology Topology;
        UNiagaraExternalEditUtilities::GetEmitterTopology(
            EmitterReference, Topology, Context);
        if (Context.Errors.Num() > 0 || Topology.EmitterName != EmitterName)
            return Result;

        for (const FNiagaraExt_ModuleTopology& Module
            : Topology.EmitterUpdateScript.Modules)
        {
            FDANiagaraLifecycleModuleView& View = Result.Emplace_GetRef();
            View.ScriptName = Module.ModuleScript == nullptr
                ? FString() : Module.ModuleScript->GetName();
            View.bEnabled = Module.Enabled;
            if (!View.HasIdentity(TEXT("EmitterState"))) continue;

            const int32 ErrorCount = Context.Errors.Num();
            const FNiagaraExt_StackItemReference ModuleReference(
                &System, EmitterName, Topology.EmitterUpdateScript.ScriptName,
                Module.ModuleName);
            FNiagaraExt_ModuleInputValues Values;
            UNiagaraExternalEditUtilities::GetModuleInputValues(
                ModuleReference, Values, Context);
            if (Context.Errors.Num() != ErrorCount)
            {
                Result.Reset();
                return Result;
            }
            for (const FNiagaraExt_StackInputValueEntry& Entry : Values.Inputs)
            {
                const FNiagaraExt_StackInputData_Enum* EnumValue =
                    Entry.Value.GetPtr<FNiagaraExt_StackInputData_Enum>();
                if (EnumValue == nullptr) continue;
                FDANiagaraLifecycleInputView& Input = View.Inputs.Emplace_GetRef();
                Input.Name = Entry.Name.ToString();
                Input.EnumValue = EnumValue->EnumName.ToString();
            }
        }
        return Result;
    }

    bool AuthorEmitterLifecycleEnum(UNiagaraSystem& System, const FName EmitterName,
        const FString& ExpectedInputName, const FString& ExpectedEnumValue,
        TArray<FText>& Errors)
    {
        FNiagaraExternalEditContext Context(&System);
        const FNiagaraExt_StackItemReference EmitterReference(
            &System, EmitterName, NAME_None, NAME_None);
        FNiagaraExt_EmitterTopology Topology;
        UNiagaraExternalEditUtilities::GetEmitterTopology(
            EmitterReference, Topology, Context);
        if (Context.HasErrors() || Topology.EmitterName != EmitterName)
        {
            Errors.Add(FText::FromString(TEXT("Could not resolve selected Niagara emitter: ")
                + EmitterName.ToString()));
            return false;
        }

        const FNiagaraExt_ModuleTopology* LifecycleModule = nullptr;
        for (const FNiagaraExt_ModuleTopology& Module
            : Topology.EmitterUpdateScript.Modules)
        {
            const FString ScriptName = Module.ModuleScript == nullptr
                ? FString() : Module.ModuleScript->GetName();
            if (!Module.Enabled
                || NormalizeNiagaraLifecycleToken(ScriptName)
                    != NormalizeNiagaraLifecycleToken(TEXT("EmitterState")))
                continue;
            if (LifecycleModule != nullptr)
            {
                Errors.Add(FText::FromString(
                    TEXT("Selected Niagara emitter has ambiguous lifecycle modules: ")
                    + EmitterName.ToString()));
                return false;
            }
            LifecycleModule = &Module;
        }
        if (LifecycleModule == nullptr)
        {
            Errors.Add(FText::FromString(
                TEXT("Selected Niagara emitter has no enabled EmitterState module: ")
                + EmitterName.ToString()));
            return false;
        }

        const FNiagaraExt_StackItemReference ModuleReference(
            &System, EmitterName, Topology.EmitterUpdateScript.ScriptName,
            LifecycleModule->ModuleName);
        FNiagaraExt_ModuleInputValues Values;
        UNiagaraExternalEditUtilities::GetModuleInputValues(
            ModuleReference, Values, Context);
        if (Context.HasErrors())
        {
            Errors.Add(FText::FromString(
                TEXT("Could not read selected Niagara emitter lifecycle inputs: ")
                + EmitterName.ToString()));
            return false;
        }

        for (const FNiagaraExt_StackInputValueEntry& Entry : Values.Inputs)
        {
            if (NormalizeNiagaraLifecycleToken(Entry.Name.ToString())
                != NormalizeNiagaraLifecycleToken(ExpectedInputName))
                continue;
            const FNiagaraExt_StackInputData_Enum* Existing =
                Entry.Value.GetPtr<FNiagaraExt_StackInputData_Enum>();
            UEnum* Enum = Existing == nullptr ? nullptr : Existing->Enum.Get();
            const int64 Value = Enum == nullptr
                ? INDEX_NONE : Enum->GetValueByNameString(ExpectedEnumValue);
            const FName EnumName = Value == INDEX_NONE
                ? NAME_None : Enum->GetNameByValue(Value);
            if (Enum == nullptr || EnumName.IsNone())
            {
                Errors.Add(FText::FromString(
                    TEXT("Could not encode selected Niagara emitter lifecycle value: ")
                    + ExpectedEnumValue));
                return false;
            }

            FNiagaraExt_StackInputValue AuthoredValue;
            FNiagaraExt_StackInputData_Enum& EnumData =
                AuthoredValue.InitializeAs<FNiagaraExt_StackInputData_Enum>();
            EnumData.Enum = Enum;
            EnumData.EnumName = EnumName;
            FNiagaraExt_StackItemReference InputReference = ModuleReference;
            InputReference.SetInput(Entry.Name);
            UNiagaraExternalEditUtilities::SetStackInputData(
                InputReference, AuthoredValue, Context);
            if (Context.HasErrors())
            {
                Errors.Add(FText::FromString(
                    TEXT("Could not author selected Niagara emitter lifecycle input: ")
                    + ExpectedInputName));
                return false;
            }
            return true;
        }

        Errors.Add(FText::FromString(
            TEXT("Selected Niagara emitter lifecycle input is missing: ")
            + ExpectedInputName));
        return false;
    }

    bool AuthorEmitterLifecycle(UNiagaraSystem& System,
        const FDANiagaraArtifactContent& Expected, TArray<FText>& Errors)
    {
        // Use a fresh external-edit context for the second write. LifeCycleMode
        // gates LoopBehavior visibility/editability in EmitterState.
        return AuthorEmitterLifecycleEnum(System, Expected.ExpectedEmitterName,
                TEXT("LifeCycleMode"), Expected.LifecycleMode, Errors)
            && AuthorEmitterLifecycleEnum(System, Expected.ExpectedEmitterName,
                TEXT("LoopBehavior"), Expected.LoopBehavior, Errors);
    }

    class FDANiagaraEmitterValidationView
    {
    public:
        FDANiagaraEmitterValidationView(const FNiagaraEmitterHandle& InHandle,
            const DA::Presentation::Validation::FEmitterLifecycle& InLifecycle)
            : Handle(&InHandle), Lifecycle(InLifecycle)
        {
        }

        bool IsValid() const { return Handle != nullptr && Handle->IsValid(); }
        bool IsEnabled() const { return Handle != nullptr && Handle->GetIsEnabled(); }
        bool HasIdentity(const FName Expected) const
        {
            return Handle != nullptr && Handle->GetName() == Expected;
        }
        bool HasSimulationTarget(const FString& Expected) const
        {
            const FVersionedNiagaraEmitterData* Data = Handle == nullptr
                ? nullptr
                : const_cast<FNiagaraEmitterHandle*>(Handle)->GetEmitterData();
            return Data != nullptr && Expected == TEXT("CPUSim")
                && Data->SimTarget == ENiagaraSimTarget::CPUSim;
        }
        bool IsOneShot() const
        {
            return Lifecycle.IsExactOneShot();
        }
        bool HasEnabledRenderer(const FString& Expected) const
        {
            bool bMatches = false;
            if (Handle == nullptr) return false;
            Handle->ForEachEnabledRendererWithIndex(
                [&Expected, &bMatches](
                    const UNiagaraRendererProperties* Renderer, const int32)
                {
                    if (Renderer != nullptr
                        && Renderer->GetClass()->GetName() == Expected)
                        bMatches = true;
                });
            return bMatches;
        }

    private:
        const FNiagaraEmitterHandle* Handle = nullptr;
        DA::Presentation::Validation::FEmitterLifecycle Lifecycle;
    };

    bool HasExpectedNiagaraEmitterContent(const UNiagaraSystem& System,
        const FDANiagaraArtifactContent& Expected,
        const bool bRequiresExactLifecycle = true)
    {
        TArray<FDANiagaraEmitterValidationView> Emitters;
        Emitters.Reserve(System.GetEmitterHandles().Num());
        UNiagaraSystem& MutableSystem = const_cast<UNiagaraSystem&>(System);
        const auto ReadModules = [&MutableSystem](const FName EmitterName)
        {
            return ReadPersistedEmitterLifecycleModules(MutableSystem, EmitterName);
        };
        for (const FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
        {
            const auto Lifecycle = DA::Presentation::Validation::ReadEmitterLifecycle(
                Handle.GetName(), ReadModules, FString(TEXT("EmitterState")),
                FString(TEXT("LifeCycleMode")), FString(TEXT("LoopBehavior")),
                Expected.LifecycleMode, Expected.LoopBehavior);
            Emitters.Emplace(Handle, Lifecycle);
        }
        return DA::Presentation::Validation::HasExpectedEmitterContent(
            Emitters, Expected.ExpectedEmitterName, Expected.SimulationTarget,
            Expected.RendererClass, bRequiresExactLifecycle);
    }

    bool NearlyEqual(const FLinearColor& Left, const FLinearColor& Right)
    {
        return Left.Equals(Right, KINDA_SMALL_NUMBER);
    }

    bool SourceEqual(const FDAPresentationDefinitionSource& Left,
        const FDAPresentationDefinitionSource& Right)
    {
        return Left.Id == Right.Id && Left.Kind == Right.Kind && Left.Faction == Right.Faction
            && Left.DisplayName == Right.DisplayName && Left.AssetPath == Right.AssetPath
            && Left.RecipePayload == Right.RecipePayload
            && Left.RecipeFingerprint == Right.RecipeFingerprint
            && Left.Artifacts == Right.Artifacts;
    }

    bool IsOwnedPackage(const FDAPresentationDefinitionSource& Source)
    {
        UObject* Existing = StaticLoadObject(UDAPresentationDefinition::StaticClass(), nullptr,
            *ObjectPath(Source.AssetPath));
        if (Existing == nullptr)
            return !FPackageName::DoesPackageExist(Source.AssetPath);
        UPackage* Package = Existing->GetOutermost();
        UMetaData* Meta = Package == nullptr ? nullptr : Package->GetMetaData();
        return Meta != nullptr
            && FString(Meta->GetValue(Existing, TEXT("DA.Generator"))) == GeneratorId
            && FString(Meta->GetValue(Existing, TEXT("DA.PresentationId")))
                == Source.Id.ToString();
    }

    bool SaveDefinition(const FDAPresentationDefinitionSource& Source,
        const FString& Fingerprint, TArray<FText>& Errors)
    {
        if (!IsOwnedPackage(Source))
        {
            Errors.Add(FText::FromString(TEXT("Foreign presentation package blocks generation: ")
                + Source.AssetPath));
            return false;
        }
        UPackage* Package = CreatePackage(*Source.AssetPath);
        const FName AssetName(*FPackageName::GetLongPackageAssetName(Source.AssetPath));
        UDAPresentationDefinition* Asset = FindObject<UDAPresentationDefinition>(
            Package, *AssetName.ToString());
        const bool bCreated = Asset == nullptr;
        if (Asset == nullptr)
            Asset = NewObject<UDAPresentationDefinition>(
                Package, AssetName, RF_Public | RF_Standalone);
        if (Asset == nullptr)
        {
            Errors.Add(FText::FromString(TEXT("Could not create presentation definition: ")
                + Source.AssetPath));
            return false;
        }
        Asset->Definition = Source;
        Asset->Definition.bGeneratedCache = true;
        Asset->SourceFingerprint = Fingerprint;
        Asset->bRuntimeManifestFallback = false;
        UMetaData* Meta = Package->GetMetaData();
        Meta->SetValue(Asset, TEXT("DA.Generator"), GeneratorId);
        Meta->SetValue(Asset, TEXT("DA.SourceFingerprint"), *Fingerprint);
        Meta->SetValue(Asset, TEXT("DA.RecipeFingerprint"), *Source.RecipeFingerprint);
        Meta->SetValue(Asset, TEXT("DA.PresentationId"), *Source.Id.ToString());
        if (bCreated) FAssetRegistryModule::AssetCreated(Asset);
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(
            Source.AssetPath, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
        {
            Errors.Add(FText::FromString(TEXT("Could not save presentation definition: ")
                + Filename));
            return false;
        }
        return true;
    }

    bool IsOwnedArtifact(
        const FDAPresentationContentManifest::FArtifactRequirement& Requirement)
    {
        if (Requirement.bExternalGenerator) return true;
        UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr,
            *ObjectPath(Requirement.AssetPath));
        if (Existing == nullptr)
            return !FPackageName::DoesPackageExist(Requirement.AssetPath);
        UPackage* Package = Existing->GetOutermost();
        UMetaData* Meta = Package == nullptr ? nullptr : Package->GetMetaData();
        return Meta != nullptr
            && FString(Meta->GetValue(Existing, TEXT("DA.Generator"))) == GeneratorId
            && FString(Meta->GetValue(Existing, TEXT("DA.PresentationId")))
                == Requirement.DefinitionId.ToString()
            && FString(Meta->GetValue(Existing, TEXT("DA.ArtifactRole")))
                == Requirement.Role.ToString();
    }

    bool SaveObjectPackage(UObject& Asset, const FString& PackagePath,
        TArray<FText>& Errors)
    {
        UPackage* Package = Asset.GetOutermost();
        FAssetRegistryModule::AssetCreated(&Asset);
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(
            PackagePath, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, &Asset, *Filename, Args))
        {
            Errors.Add(FText::FromString(TEXT("Could not save presentation artifact: ")
                + Filename));
            return false;
        }
        return true;
    }

    bool DecodeWaveSource(const FString& RelativeSource, FString& OutFilename,
        TArray<FText>& Errors)
    {
        FString Encoded;
        if (!FFileHelper::LoadFileToString(Encoded, *(FPaths::ProjectDir() / RelativeSource)))
        {
            Errors.Add(FText::FromString(TEXT("Could not read encoded audio source: ")
                + RelativeSource));
            return false;
        }
        TArray<uint8> Bytes;
        if (!FBase64::Decode(Encoded.TrimStartAndEnd(), Bytes))
        {
            Errors.Add(FText::FromString(TEXT("Could not decode audio source: ")
                + RelativeSource));
            return false;
        }
        OutFilename = FPaths::ProjectIntermediateDir()
            / TEXT("PresentationImports/NeutralPulse.wav");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutFilename), true);
        if (!FFileHelper::SaveArrayToFile(Bytes, *OutFilename))
        {
            Errors.Add(FText::FromString(TEXT("Could not stage decoded audio source: ")
                + OutFilename));
            return false;
        }
        return true;
    }

    UObject* ImportArtifact(const FString& PackagePath, const FString& Filename)
    {
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        Task->Filename = FPaths::ConvertRelativePathToFull(Filename);
        Task->DestinationPath = FPackageName::GetLongPackagePath(PackagePath);
        Task->DestinationName = FPackageName::GetLongPackageAssetName(PackagePath);
        Task->bAutomated = true;
        Task->bReplaceExisting = true;
        Task->bSave = false;
        TArray<UAssetImportTask*> Tasks = {Task};
        FAssetToolsModule::GetModule().Get().ImportAssetTasks(Tasks);
        return StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath(PackagePath));
    }

    UMaterialInstanceConstant* BuildMaterialArtifact(UPackage& Package,
        const FName AssetName,
        const FDAPresentationContentManifest::FArtifactRequirement& Requirement,
        TArray<FText>& Errors)
    {
        FDAMaterialArtifactContent Content;
        if (!ParseMaterialContent(Requirement, Content))
        {
            Errors.Add(FText::FromString(TEXT("Invalid authored material content for ")
                + Requirement.DefinitionId.ToString()));
            return nullptr;
        }

        const FName ParentName(*(AssetName.ToString() + TEXT("_Parent")));
        UMaterial* Parent = FindObject<UMaterial>(&Package, *ParentName.ToString());
        const bool bCreatedParent = Parent == nullptr;
        if (Parent != nullptr)
        {
            UMetaData* ExistingMeta = Package.GetMetaData();
            if (FString(ExistingMeta->GetValue(Parent, TEXT("DA.Generator"))) != GeneratorId
                || FString(ExistingMeta->GetValue(Parent, TEXT("DA.PresentationId")))
                    != Requirement.DefinitionId.ToString()
                || FString(ExistingMeta->GetValue(Parent, TEXT("DA.ArtifactRole")))
                    != TEXT("material_parent"))
            {
                Errors.Add(FText::FromString(
                    TEXT("Foreign material parent blocks presentation generation: ")
                    + Parent->GetPathName()));
                return nullptr;
            }
        }
        if (Parent == nullptr)
            Parent = NewObject<UMaterial>(&Package, ParentName, RF_Public | RF_Standalone);
        if (Parent == nullptr) return nullptr;

        Parent->Modify();
        Parent->PreEditChange(nullptr);
        UMaterialEditingLibrary::DeleteAllMaterialExpressions(Parent);
        UMaterialExpressionVectorParameter* BaseColor =
            Cast<UMaterialExpressionVectorParameter>(
                UMaterialEditingLibrary::CreateMaterialExpression(Parent,
                    UMaterialExpressionVectorParameter::StaticClass(), -500, -180));
        UMaterialExpressionVectorParameter* Emissive =
            Cast<UMaterialExpressionVectorParameter>(
                UMaterialEditingLibrary::CreateMaterialExpression(Parent,
                    UMaterialExpressionVectorParameter::StaticClass(), -500, -60));
        UMaterialExpressionScalarParameter* Metallic =
            Cast<UMaterialExpressionScalarParameter>(
                UMaterialEditingLibrary::CreateMaterialExpression(Parent,
                    UMaterialExpressionScalarParameter::StaticClass(), -500, 60));
        UMaterialExpressionScalarParameter* Roughness =
            Cast<UMaterialExpressionScalarParameter>(
                UMaterialEditingLibrary::CreateMaterialExpression(Parent,
                    UMaterialExpressionScalarParameter::StaticClass(), -500, 180));
        if (BaseColor == nullptr || Emissive == nullptr || Metallic == nullptr
            || Roughness == nullptr)
        {
            Errors.Add(FText::FromString(TEXT("Could not create authored material graph for ")
                + Requirement.DefinitionId.ToString()));
            return nullptr;
        }
        BaseColor->ParameterName = TEXT("DA.BaseColor");
        BaseColor->DefaultValue = Content.BaseColor;
        Emissive->ParameterName = TEXT("DA.EmissiveColor");
        Emissive->DefaultValue = Content.EmissiveColor;
        Metallic->ParameterName = TEXT("DA.Metallic");
        Metallic->DefaultValue = Content.Metallic;
        Roughness->ParameterName = TEXT("DA.Roughness");
        Roughness->DefaultValue = Content.Roughness;
        bool bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(
            BaseColor, TEXT(""), MP_BaseColor);
        bConnected &= UMaterialEditingLibrary::ConnectMaterialProperty(
            Emissive, TEXT(""), MP_EmissiveColor);
        bConnected &= UMaterialEditingLibrary::ConnectMaterialProperty(
            Metallic, TEXT(""), MP_Metallic);
        bConnected &= UMaterialEditingLibrary::ConnectMaterialProperty(
            Roughness, TEXT(""), MP_Roughness);
        Parent->PostEditChange();
        UMaterialEditingLibrary::RecompileMaterial(Parent);
        if (!bConnected)
        {
            Errors.Add(FText::FromString(TEXT("Could not connect authored material graph for ")
                + Requirement.DefinitionId.ToString()));
            return nullptr;
        }
        if (bCreatedParent) FAssetRegistryModule::AssetCreated(Parent);
        UMetaData* ParentMeta = Package.GetMetaData();
        ParentMeta->SetValue(Parent, TEXT("DA.Generator"), GeneratorId);
        ParentMeta->SetValue(Parent, TEXT("DA.PresentationId"),
            *Requirement.DefinitionId.ToString());
        ParentMeta->SetValue(Parent, TEXT("DA.ArtifactRole"), TEXT("material_parent"));
        ParentMeta->SetValue(Parent, TEXT("DA.SourceSha1"), *Requirement.SourceSha1);
        ParentMeta->SetValue(Parent, TEXT("DA.SourceContentFingerprint"),
            *Requirement.SourceContentFingerprint);

        UMaterialInstanceConstant* Material = FindObject<UMaterialInstanceConstant>(
            &Package, *AssetName.ToString());
        if (Material == nullptr)
            Material = NewObject<UMaterialInstanceConstant>(
                &Package, AssetName, RF_Public | RF_Standalone);
        if (Material == nullptr) return nullptr;
        UMaterialEditingLibrary::SetMaterialInstanceParent(Material, Parent);
        UMaterialEditingLibrary::ClearAllMaterialInstanceParameters(Material);
        UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Material,
            TEXT("DA.BaseColor"), Content.BaseColor,
            EMaterialParameterAssociation::GlobalParameter);
        UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Material,
            TEXT("DA.EmissiveColor"), Content.EmissiveColor,
            EMaterialParameterAssociation::GlobalParameter);
        UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Material,
            TEXT("DA.Metallic"), Content.Metallic,
            EMaterialParameterAssociation::GlobalParameter);
        UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Material,
            TEXT("DA.Roughness"), Content.Roughness,
            EMaterialParameterAssociation::GlobalParameter);
        UMaterialEditingLibrary::UpdateMaterialInstance(Material);
        return Material;
    }

    UNiagaraSystem* BuildNiagaraArtifact(UPackage& Package, const FName AssetName,
        UObject* Existing,
        const FDAPresentationContentManifest::FArtifactRequirement& Requirement,
        TArray<FText>& Errors)
    {
        FDANiagaraArtifactContent Content;
        if (!ParseNiagaraContent(Requirement, Content))
        {
            Errors.Add(FText::FromString(TEXT("Invalid authored Niagara content for ")
                + Requirement.DefinitionId.ToString()));
            return nullptr;
        }
        UNiagaraSystem* Template = LoadObject<UNiagaraSystem>(
            nullptr, *Content.TemplateAssetPath);
        if (Template == nullptr
            || Template->GetEmitterHandles().Num() < Content.MinimumEmitterCount
            || !HasExpectedNiagaraEmitterContent(
                *Template, Content, /*bRequiresExactLifecycle=*/false))
        {
            Errors.Add(FText::FromString(TEXT("Niagara sprite-burst template is unavailable or empty: ")
                + Content.TemplateAssetPath));
            return nullptr;
        }
        if (Existing != nullptr)
        {
            const FName StaleName = MakeUniqueObjectName(GetTransientPackage(),
                Existing->GetClass(), TEXT("DA_StalePresentationSystem"));
            Existing->ClearFlags(RF_Public | RF_Standalone);
            Existing->SetFlags(RF_Transient);
            if (!Existing->Rename(*StaleName.ToString(), GetTransientPackage(),
                    REN_DontCreateRedirectors | REN_NonTransactional))
            {
                Errors.Add(FText::FromString(TEXT("Could not replace owned Niagara artifact: ")
                    + Requirement.AssetPath));
                return nullptr;
            }
        }
        UNiagaraSystem* System = Cast<UNiagaraSystem>(StaticDuplicateObject(
            Template, &Package, AssetName));
        if (System == nullptr) return nullptr;
        System->SetFlags(RF_Public | RF_Standalone);
        System->Modify();
        System->PreEditChange(nullptr);
        if (!AuthorEmitterLifecycle(*System, Content, Errors)) return nullptr;
        FNiagaraUserRedirectionParameterStore& Parameters = System->GetExposedParameters();
        const FNiagaraVariable Radius(FNiagaraTypeDefinition::GetFloatDef(),
            TEXT("User.DA.GameplayRadiusMeters"));
        const FNiagaraVariable Burst(FNiagaraTypeDefinition::GetIntDef(),
            TEXT("User.DA.BurstCount"));
        const FNiagaraVariable Lifetime(FNiagaraTypeDefinition::GetFloatDef(),
            TEXT("User.DA.LifetimeSeconds"));
        const FNiagaraVariable Size(FNiagaraTypeDefinition::GetVec2Def(),
            TEXT("User.DA.SpriteSize"));
        Parameters.SetParameterValue<float>(Content.GameplayRadiusMeters, Radius, true);
        Parameters.SetParameterValue<int32>(Content.BurstCount, Burst, true);
        Parameters.SetParameterValue<float>(Content.LifetimeSeconds, Lifetime, true);
        Parameters.SetParameterValue<FVector2f>(Content.SpriteSize, Size, true);
        System->SetFixedBounds(Content.Bounds);
        System->SetWarmupTime(Content.WarmupSeconds);
        System->PostEditChange();
        System->WaitForCompilationComplete(true, false);
        if (!HasExpectedNiagaraEmitterContent(*System, Content))
        {
            Errors.Add(FText::FromString(
                TEXT("Duplicated Niagara artifact did not persist the selected-emitter lifecycle: ")
                + Requirement.AssetPath));
            return nullptr;
        }
        return System;
    }

    bool SaveArtifact(
        const FDAPresentationContentManifest::FArtifactRequirement& Requirement,
        const FDAPresentationContentManifest& Manifest, TArray<FText>& Errors)
    {
        if (Requirement.bExternalGenerator) return true;
        if (!IsOwnedArtifact(Requirement))
        {
            Errors.Add(FText::FromString(TEXT("Foreign presentation artifact blocks generation: ")
                + Requirement.AssetPath));
            return false;
        }
        UPackage* Package = CreatePackage(*Requirement.AssetPath);
        const FName AssetName(*FPackageName::GetLongPackageAssetName(Requirement.AssetPath));
        UObject* Asset = FindObject<UObject>(Package, *AssetName.ToString());
        if (Requirement.AssetClass == TEXT("StaticMesh"))
        {
            Asset = ImportArtifact(Requirement.AssetPath,
                FPaths::ProjectDir() / Requirement.SourcePath);
        }
        else if (Requirement.AssetClass == TEXT("SoundWave"))
        {
            FString WaveFilename;
            if (!DecodeWaveSource(Requirement.SourcePath, WaveFilename, Errors)) return false;
            Asset = ImportArtifact(Requirement.AssetPath, WaveFilename);
        }
        else if (Requirement.AssetClass == TEXT("MaterialInstanceConstant"))
        {
            Asset = BuildMaterialArtifact(*Package, AssetName, Requirement, Errors);
        }
        else if (Requirement.AssetClass == TEXT("NiagaraSystem"))
        {
            Asset = BuildNiagaraArtifact(*Package, AssetName, Asset, Requirement, Errors);
        }
        else if (Requirement.AssetClass == TEXT("SoundCue"))
        {
            const FDAPresentationContentManifest::FArtifactRequirement* WaveRequirement =
                Manifest.ArtifactRequirements.FindByPredicate([&Requirement](const auto& Row)
                {
                    return Row.DefinitionId == Requirement.DefinitionId
                        && Row.Role == TEXT("wave");
                });
            USoundWave* Wave = WaveRequirement == nullptr ? nullptr : Cast<USoundWave>(
                StaticLoadObject(USoundWave::StaticClass(), nullptr,
                    *ObjectPath(WaveRequirement->AssetPath)));
            USoundCue* Cue = Cast<USoundCue>(Asset);
            if (Cue == nullptr)
                Cue = NewObject<USoundCue>(Package, AssetName,
                    RF_Public | RF_Standalone);
            if (Cue != nullptr && Wave != nullptr)
            {
                USoundNodeWavePlayer* Player = Cast<USoundNodeWavePlayer>(Cue->FirstNode);
                if (Player == nullptr)
                    Player = Cue->ConstructSoundNode<USoundNodeWavePlayer>();
                Player->SetSoundWave(Wave);
                Cue->FirstNode = Player;
            }
            Asset = Cue;
        }
        if (Asset == nullptr || Asset->GetClass()->GetName() != Requirement.AssetClass)
        {
            Errors.Add(FText::FromString(TEXT("Could not create presentation artifact class ")
                + Requirement.AssetClass + TEXT(" at ") + Requirement.AssetPath));
            return false;
        }
        UMetaData* Meta = Package->GetMetaData();
        Meta->SetValue(Asset, TEXT("DA.Generator"), GeneratorId);
        Meta->SetValue(Asset, TEXT("DA.SourceFingerprint"), *Manifest.Fingerprint);
        Meta->SetValue(Asset, TEXT("DA.SourceSha1"), *Requirement.SourceSha1);
        if (!Requirement.SourceContentFingerprint.IsEmpty())
        {
            Meta->SetValue(Asset, TEXT("DA.SourceContentFingerprint"),
                *Requirement.SourceContentFingerprint);
            Meta->SetValue(Asset, TEXT("DA.SourceContent"),
                *Requirement.SourceContentPayload);
        }
        Meta->SetValue(Asset, TEXT("DA.PresentationId"), *Requirement.DefinitionId.ToString());
        Meta->SetValue(Asset, TEXT("DA.ArtifactRole"), *Requirement.Role.ToString());
        FDAPresentationDefinitionSource CanonicalDefinition;
        if (!FDAPresentationContentPipeline::Resolve(Manifest, Requirement.DefinitionKind,
                Requirement.DefinitionId, CanonicalDefinition))
        {
            Errors.Add(FText::FromString(TEXT("Artifact has no canonical definition: ")
                + Requirement.DefinitionId.ToString()));
            return false;
        }
        Meta->SetValue(Asset, TEXT("DA.RecipeFingerprint"),
            *CanonicalDefinition.RecipeFingerprint);
        return SaveObjectPackage(*Asset, Requirement.AssetPath, Errors);
    }

    bool HasExpectedArtifactContent(const UObject& Asset,
        const FDAPresentationContentManifest::FArtifactRequirement& Requirement)
    {
        if (Requirement.AssetClass == TEXT("StaticMesh"))
            return Cast<UStaticMesh>(&Asset) != nullptr
                && CastChecked<UStaticMesh>(&Asset)->GetNumLODs() > 0;
        if (Requirement.AssetClass == TEXT("MaterialInstanceConstant"))
        {
            FDAMaterialArtifactContent Expected;
            const UMaterialInstanceConstant* Material =
                Cast<UMaterialInstanceConstant>(&Asset);
            UMaterial* Parent = Material == nullptr ? nullptr
                : Cast<UMaterial>(Material->Parent);
            if (Material == nullptr || Parent == nullptr
                || Parent == UMaterial::GetDefaultMaterial(MD_Surface)
                || !ParseMaterialContent(Requirement, Expected)
                || UMaterialEditingLibrary::GetNumMaterialExpressions(Parent) < 4
                || UMaterialEditingLibrary::GetMaterialPropertyInputNode(
                    Parent, MP_BaseColor) == nullptr
                || UMaterialEditingLibrary::GetMaterialPropertyInputNode(
                    Parent, MP_EmissiveColor) == nullptr
                || UMaterialEditingLibrary::GetMaterialPropertyInputNode(
                    Parent, MP_Metallic) == nullptr
                || UMaterialEditingLibrary::GetMaterialPropertyInputNode(
                    Parent, MP_Roughness) == nullptr)
                return false;
            const FLinearColor BaseColor =
                UMaterialEditingLibrary::GetMaterialInstanceVectorParameterValue(
                    const_cast<UMaterialInstanceConstant*>(Material), TEXT("DA.BaseColor"),
                    EMaterialParameterAssociation::GlobalParameter);
            const FLinearColor Emissive =
                UMaterialEditingLibrary::GetMaterialInstanceVectorParameterValue(
                    const_cast<UMaterialInstanceConstant*>(Material), TEXT("DA.EmissiveColor"),
                    EMaterialParameterAssociation::GlobalParameter);
            const float Metallic =
                UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(
                    const_cast<UMaterialInstanceConstant*>(Material), TEXT("DA.Metallic"),
                    EMaterialParameterAssociation::GlobalParameter);
            const float Roughness =
                UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(
                    const_cast<UMaterialInstanceConstant*>(Material), TEXT("DA.Roughness"),
                    EMaterialParameterAssociation::GlobalParameter);
            return NearlyEqual(BaseColor, Expected.BaseColor)
                && NearlyEqual(Emissive, Expected.EmissiveColor)
                && FMath::IsNearlyEqual(Metallic, Expected.Metallic)
                && FMath::IsNearlyEqual(Roughness, Expected.Roughness)
                && NearlyEqual(
                    UMaterialEditingLibrary::GetMaterialDefaultVectorParameterValue(
                        Parent, TEXT("DA.BaseColor")), Expected.BaseColor)
                && NearlyEqual(
                    UMaterialEditingLibrary::GetMaterialDefaultVectorParameterValue(
                        Parent, TEXT("DA.EmissiveColor")), Expected.EmissiveColor)
                && FMath::IsNearlyEqual(
                    UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(
                        Parent, TEXT("DA.Metallic")), Expected.Metallic)
                && FMath::IsNearlyEqual(
                    UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(
                        Parent, TEXT("DA.Roughness")), Expected.Roughness);
        }
        if (Requirement.AssetClass == TEXT("NiagaraSystem"))
        {
            FDANiagaraArtifactContent Expected;
            const UNiagaraSystem* System = Cast<UNiagaraSystem>(&Asset);
            if (System == nullptr || !ParseNiagaraContent(Requirement, Expected)
                || System->GetEmitterHandles().Num() < Expected.MinimumEmitterCount
                || !HasExpectedNiagaraEmitterContent(*System, Expected)
                || System->GetRendererDrawOrder().Num() < 1
                || !System->GetFixedBounds().GetCenter().Equals(
                    Expected.Bounds.GetCenter(), KINDA_SMALL_NUMBER)
                || !System->GetFixedBounds().GetExtent().Equals(
                    Expected.Bounds.GetExtent(), KINDA_SMALL_NUMBER)
                || !FMath::IsNearlyEqual(System->GetWarmupTime(),
                    Expected.WarmupSeconds))
                return false;
            const FNiagaraUserRedirectionParameterStore& Parameters =
                System->GetExposedParameters();
            const FNiagaraVariable Radius(FNiagaraTypeDefinition::GetFloatDef(),
                TEXT("User.DA.GameplayRadiusMeters"));
            const FNiagaraVariable Burst(FNiagaraTypeDefinition::GetIntDef(),
                TEXT("User.DA.BurstCount"));
            const FNiagaraVariable Lifetime(FNiagaraTypeDefinition::GetFloatDef(),
                TEXT("User.DA.LifetimeSeconds"));
            const FNiagaraVariable Size(FNiagaraTypeDefinition::GetVec2Def(),
                TEXT("User.DA.SpriteSize"));
            return Parameters.IndexOf(Radius) != INDEX_NONE
                && Parameters.IndexOf(Burst) != INDEX_NONE
                && Parameters.IndexOf(Lifetime) != INDEX_NONE
                && Parameters.IndexOf(Size) != INDEX_NONE
                && FMath::IsNearlyEqual(Parameters.GetParameterValue<float>(Radius),
                    Expected.GameplayRadiusMeters)
                && Parameters.GetParameterValue<int32>(Burst) == Expected.BurstCount
                && FMath::IsNearlyEqual(Parameters.GetParameterValue<float>(Lifetime),
                    Expected.LifetimeSeconds)
                && Parameters.GetParameterValue<FVector2f>(Size).Equals(
                    Expected.SpriteSize, KINDA_SMALL_NUMBER);
        }
        if (Requirement.AssetClass == TEXT("SoundWave"))
            return Cast<USoundWave>(&Asset) != nullptr
                && CastChecked<USoundWave>(&Asset)->Duration > 0.f;
        if (Requirement.AssetClass == TEXT("SoundCue"))
            return Cast<USoundCue>(&Asset) != nullptr
                && CastChecked<USoundCue>(&Asset)->FirstNode != nullptr;
        return Requirement.AssetClass == TEXT("LevelSequence")
            && Asset.GetClass()->GetName() == TEXT("LevelSequence");
    }

    bool ValidateRegistry(const FDAPresentationContentManifest& Manifest,
        TArray<FText>& Errors)
    {
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry")).Get();
        TArray<FString> PackagePaths;
        for (const FDAPresentationDefinitionSource& Source : Manifest.AllDefinitions())
            PackagePaths.AddUnique(FPackageName::GetLongPackagePath(Source.AssetPath));
        for (const FDAPresentationContentManifest::FArtifactRequirement& Requirement
            : Manifest.ArtifactRequirements)
            PackagePaths.AddUnique(FPackageName::GetLongPackagePath(Requirement.AssetPath));
        Registry.ScanPathsSynchronous(PackagePaths, true);

        TArray<UDAPresentationDefinition*> Loaded;
        for (const FDAPresentationDefinitionSource& Source : Manifest.AllDefinitions())
        {
            const FAssetData AssetData = Registry.GetAssetByObjectPath(
                FSoftObjectPath(ObjectPath(Source.AssetPath)));
            UDAPresentationDefinition* Asset = Cast<UDAPresentationDefinition>(
                AssetData.IsValid() ? AssetData.GetAsset() : nullptr);
            UPackage* Package = Asset == nullptr ? nullptr : Asset->GetOutermost();
            UMetaData* Meta = Package == nullptr ? nullptr : Package->GetMetaData();
            if (Asset == nullptr || Meta == nullptr
                || !SourceEqual(Asset->Definition, Source)
                || !Asset->Definition.bGeneratedCache
                || Asset->bRuntimeManifestFallback
                || Asset->SourceFingerprint != Manifest.Fingerprint
                || FString(Meta->GetValue(Asset, TEXT("DA.Generator"))) != GeneratorId
                || FString(Meta->GetValue(Asset, TEXT("DA.SourceFingerprint")))
                    != Manifest.Fingerprint
                || FString(Meta->GetValue(Asset, TEXT("DA.RecipeFingerprint")))
                    != Source.RecipeFingerprint
                || FString(Meta->GetValue(Asset, TEXT("DA.PresentationId")))
                    != Source.Id.ToString())
            {
                Errors.Add(FText::FromString(TEXT("Asset Registry presentation coverage failed: ")
                    + Source.AssetPath));
                return false;
            }
            Loaded.Add(Asset);
        }
        for (const FDAPresentationContentManifest::FArtifactRequirement& Requirement
            : Manifest.ArtifactRequirements)
        {
            const FAssetData AssetData = Registry.GetAssetByObjectPath(
                FSoftObjectPath(ObjectPath(Requirement.AssetPath)));
            UObject* Asset = AssetData.IsValid() ? AssetData.GetAsset() : nullptr;
            UPackage* Package = Asset == nullptr ? nullptr : Asset->GetOutermost();
            UMetaData* Meta = Package == nullptr ? nullptr : Package->GetMetaData();
            FDAPresentationDefinitionSource CanonicalDefinition;
            const bool bDefinitionResolved = Requirement.bExternalGenerator
                || FDAPresentationContentPipeline::Resolve(Manifest,
                    Requirement.DefinitionKind, Requirement.DefinitionId,
                    CanonicalDefinition);
            const bool bOwnedMetadataValid = Requirement.bExternalGenerator
                || (Meta != nullptr
                    && FString(Meta->GetValue(Asset, TEXT("DA.Generator"))) == GeneratorId
                    && FString(Meta->GetValue(Asset, TEXT("DA.SourceFingerprint")))
                        == Manifest.Fingerprint
                    && FString(Meta->GetValue(Asset, TEXT("DA.SourceSha1")))
                        == Requirement.SourceSha1
                    && (Requirement.SourceContentFingerprint.IsEmpty()
                        || (FString(Meta->GetValue(Asset,
                                TEXT("DA.SourceContentFingerprint")))
                                == Requirement.SourceContentFingerprint
                            && FString(Meta->GetValue(Asset, TEXT("DA.SourceContent")))
                                == Requirement.SourceContentPayload))
                    && FString(Meta->GetValue(Asset, TEXT("DA.PresentationId")))
                        == Requirement.DefinitionId.ToString()
                    && FString(Meta->GetValue(Asset, TEXT("DA.ArtifactRole")))
                        == Requirement.Role.ToString()
                    && FString(Meta->GetValue(Asset, TEXT("DA.RecipeFingerprint")))
                        == CanonicalDefinition.RecipeFingerprint);
            if (Asset == nullptr
                || Asset->GetClass()->GetName() != Requirement.AssetClass
                || !HasExpectedArtifactContent(*Asset, Requirement)
                || !bDefinitionResolved
                || !bOwnedMetadataValid)
            {
                Errors.Add(FText::FromString(
                    TEXT("Asset Registry presentation artifact coverage failed: ")
                    + Requirement.AssetPath + TEXT(" expected ") + Requirement.AssetClass));
                return false;
            }
        }
        bool bUsedGeneratedCache = false;
        return FDAPresentationContentPipeline::ValidateGeneratedCache(
            Manifest, Loaded, bUsedGeneratedCache, Errors) && bUsedGeneratedCache;
    }
}

UDAPresentationContentCommandlet::UDAPresentationContentCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UDAPresentationContentCommandlet::Main(const FString& Params)
{
    FDAPresentationContentManifest Manifest;
    TArray<FText> Errors;
    if (!FDAPresentationContentPipeline::LoadCanonical(Manifest, Errors))
    {
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return 1;
    }

    const bool bValidateOnly = FParse::Param(*Params, TEXT("ValidateOnly"));
    if (!bValidateOnly)
    {
        // Refuse every foreign target before mutating the first package.
        for (const FDAPresentationDefinitionSource& Source : Manifest.AllDefinitions())
        {
            if (!IsOwnedPackage(Source))
            {
                Errors.Add(FText::FromString(
                    TEXT("Foreign presentation package blocks preflight: ")
                    + Source.AssetPath));
                break;
            }
        }
        for (const FDAPresentationContentManifest::FArtifactRequirement& Requirement
            : Manifest.ArtifactRequirements)
        {
            if (!IsOwnedArtifact(Requirement))
            {
                Errors.Add(FText::FromString(
                    TEXT("Foreign presentation artifact blocks preflight: ")
                    + Requirement.AssetPath));
                break;
            }
        }
    }
    if (!bValidateOnly && Errors.IsEmpty())
    {
        for (const FDAPresentationDefinitionSource& Source : Manifest.AllDefinitions())
        {
            if (!SaveDefinition(Source, Manifest.Fingerprint, Errors)) break;
        }
        for (const FDAPresentationContentManifest::FArtifactRequirement& Requirement
            : Manifest.ArtifactRequirements)
        {
            if (!Errors.IsEmpty() || !SaveArtifact(Requirement, Manifest, Errors)) break;
        }
    }
    if (Errors.IsEmpty())
        ValidateRegistry(Manifest, Errors);
    for (const FText& Error : Errors)
        UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
    return Errors.IsEmpty() ? 0 : 1;
}
