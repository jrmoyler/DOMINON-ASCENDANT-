#include "Ascension/DAAscensionContent.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const FPrimaryAssetType UDAReplicationDoctrineDefinition::AssetType(TEXT("DADoctrine"));

FPrimaryAssetId UDAReplicationDoctrineDefinition::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(AssetType, DoctrineId);
}

namespace
{
    bool ExactKeys(const TSharedPtr<FJsonObject>& Object,
        const TArray<FString>& Expected, const FString& At, TArray<FText>& Errors)
    {
        if (!Object.IsValid() || Object->Values.Num() != Expected.Num()
            || Expected.ContainsByPredicate([&Object](const FString& Key)
            { return !Object->HasField(Key); }))
        {
            Errors.Add(FText::FromString(At + TEXT(" has missing or unknown fields.")));
            return false;
        }
        return true;
    }

    bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        FString& Out, const FString& At, TArray<FText>& Errors)
    {
        if (!Object.IsValid() || !Object->TryGetStringField(Field, Out) || Out.IsEmpty())
        { Errors.Add(FText::FromString(At + TEXT(".") + Field + TEXT(" is required."))); return false; }
        return true;
    }

    bool ReadInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        int32& Out, const FString& At, TArray<FText>& Errors)
    {
        double Value = 0.0;
        if (!Object.IsValid() || !Object->TryGetNumberField(Field, Value)
            || !FMath::IsFinite(Value) || Value != FMath::RoundToDouble(Value)
            || Value < static_cast<double>(MIN_int32)
            || Value > static_cast<double>(MAX_int32))
        {
            Errors.Add(FText::FromString(At + TEXT(".") + Field
                + TEXT(" must be a finite 32-bit integer.")));
            return false;
        }
        Out = static_cast<int32>(Value);
        return true;
    }
}

FString FDAFirstAscensionContentPipeline::GetCanonicalManifestPath()
{
    return FPaths::Combine(FPaths::ProjectContentDir(),
        TEXT("DA/Manifests/FirstAscension.json"));
}

bool FDAFirstAscensionContentPipeline::LoadFile(const FString& Filename,
    FDAFirstAscensionContentManifest& OutManifest, TArray<FText>& Errors)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Filename))
    { Errors.Add(FText::FromString(TEXT("Missing First Ascension manifest: ") + Filename)); return false; }
    return ParseJson(Json, OutManifest, Errors);
}

bool FDAFirstAscensionContentPipeline::ParseJson(const FString& Json,
    FDAFirstAscensionContentManifest& OutManifest, TArray<FText>& Errors)
{
    OutManifest = {};
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !ExactKeys(Root,
        {TEXT("schemaVersion"), TEXT("contentId"), TEXT("fingerprint"), TEXT("quest"),
         TEXT("doctrine"), TEXT("fusion"), TEXT("forgeweaveDefinitionIds"),
         TEXT("buildingImports"), TEXT("cinematic")}, TEXT("root"), Errors)) return false;

    double Number = 0.0;
    FString Value;
    if (!Root->TryGetNumberField(TEXT("schemaVersion"), Number)
        || Number != 1.0
        || !ReadString(Root, TEXT("contentId"), Value, TEXT("root"), Errors)) return false;
    OutManifest.SchemaVersion = 1;
    OutManifest.ContentId = FName(*Value);
    if (!ReadString(Root, TEXT("fingerprint"), OutManifest.Fingerprint, TEXT("root"), Errors)) return false;

    const TSharedPtr<FJsonObject>* Quest = nullptr;
    if (!Root->TryGetObjectField(TEXT("quest"), Quest) || Quest == nullptr
        || !ExactKeys(*Quest, {TEXT("id"), TEXT("title"), TEXT("assetPath"),
            TEXT("completionHistory")}, TEXT("quest"), Errors)) return false;
    ReadString(*Quest, TEXT("id"), Value, TEXT("quest"), Errors); OutManifest.QuestId = FName(*Value);
    ReadString(*Quest, TEXT("title"), OutManifest.QuestTitle, TEXT("quest"), Errors);
    ReadString(*Quest, TEXT("assetPath"), OutManifest.QuestAssetPath, TEXT("quest"), Errors);
    ReadString(*Quest, TEXT("completionHistory"), Value, TEXT("quest"), Errors);
    OutManifest.QuestCompletionHistory = FName(*Value);

    const TSharedPtr<FJsonObject>* Doctrine = nullptr;
    if (!Root->TryGetObjectField(TEXT("doctrine"), Doctrine) || Doctrine == nullptr
        || !ExactKeys(*Doctrine, {TEXT("id"), TEXT("assetPath"),
            TEXT("cadenceDevelopmentCycles")}, TEXT("doctrine"), Errors)) return false;
    ReadString(*Doctrine, TEXT("id"), Value, TEXT("doctrine"), Errors); OutManifest.DoctrineId = FName(*Value);
    ReadString(*Doctrine, TEXT("assetPath"), OutManifest.DoctrineAssetPath, TEXT("doctrine"), Errors);
    if (!(*Doctrine)->TryGetNumberField(TEXT("cadenceDevelopmentCycles"), Number)
        || Number != 12.0) return false;
    OutManifest.ReplicationCadenceDevelopmentCycles = 12;

    const TSharedPtr<FJsonObject>* Fusion = nullptr;
    if (!Root->TryGetObjectField(TEXT("fusion"), Fusion) || Fusion == nullptr
        || !ExactKeys(*Fusion, {TEXT("definitionId"), TEXT("assetPath"),
            TEXT("constructionCycles"), TEXT("craftCapital"), TEXT("craftInsight"),
            TEXT("requiredCraftingFacilityId"), TEXT("utilityPower"), TEXT("utilityData"),
            TEXT("workforceRequirementModifier"), TEXT("industrialThroughputModifier"),
            TEXT("adjacentIndustrialConstructionSpeedModifier"), TEXT("dependencyPerCycle"),
            TEXT("resourceHungerPerCycle")}, TEXT("fusion"), Errors)) return false;
    ReadString(*Fusion, TEXT("definitionId"), Value, TEXT("fusion"), Errors); OutManifest.FusionDefinitionId = FName(*Value);
    ReadString(*Fusion, TEXT("assetPath"), OutManifest.FusionAssetPath, TEXT("fusion"), Errors);
    if (!ReadInteger(*Fusion, TEXT("constructionCycles"), OutManifest.ConstructionCycles,
            TEXT("fusion"), Errors)
        || !ReadInteger(*Fusion, TEXT("craftCapital"), OutManifest.CraftCapital,
            TEXT("fusion"), Errors)
        || !ReadInteger(*Fusion, TEXT("craftInsight"), OutManifest.CraftInsight,
            TEXT("fusion"), Errors)
        || !ReadInteger(*Fusion, TEXT("utilityPower"), OutManifest.UtilityPower,
            TEXT("fusion"), Errors)
        || !ReadInteger(*Fusion, TEXT("utilityData"), OutManifest.UtilityData,
            TEXT("fusion"), Errors)
        || !ReadString(*Fusion, TEXT("requiredCraftingFacilityId"), Value, TEXT("fusion"), Errors))
        return false;
    OutManifest.RequiredCraftingFacilityId = FName(*Value);
    (*Fusion)->TryGetNumberField(TEXT("workforceRequirementModifier"), Number); OutManifest.WorkforceRequirementModifier = Number;
    (*Fusion)->TryGetNumberField(TEXT("industrialThroughputModifier"), Number); OutManifest.IndustrialThroughputModifier = Number;
    (*Fusion)->TryGetNumberField(TEXT("adjacentIndustrialConstructionSpeedModifier"), Number); OutManifest.AdjacentIndustrialConstructionSpeedModifier = Number;
    (*Fusion)->TryGetNumberField(TEXT("dependencyPerCycle"), Number); OutManifest.DependencyPerCycle = Number;
    (*Fusion)->TryGetNumberField(TEXT("resourceHungerPerCycle"), Number); OutManifest.ResourceHungerPerCycle = Number;

    const TArray<TSharedPtr<FJsonValue>>* DefinitionIds = nullptr;
    if (!Root->TryGetArrayField(TEXT("forgeweaveDefinitionIds"), DefinitionIds)
        || DefinitionIds == nullptr) return false;
    for (const TSharedPtr<FJsonValue>& Entry : *DefinitionIds)
    { if (!Entry.IsValid() || !Entry->TryGetString(Value)) return false; OutManifest.ForgeweaveDefinitionIds.Add(FName(*Value)); }

    const TArray<TSharedPtr<FJsonValue>>* Imports = nullptr;
    if (!Root->TryGetArrayField(TEXT("buildingImports"), Imports) || Imports == nullptr) return false;
    for (const TSharedPtr<FJsonValue>& Entry : *Imports)
    {
        const TSharedPtr<FJsonObject> Object = Entry.IsValid() ? Entry->AsObject() : nullptr;
        if (!ExactKeys(Object, {TEXT("assetPath"), TEXT("sourcePath"), TEXT("assetClass")},
            TEXT("buildingImports[]"), Errors)) return false;
        FDAFirstAscensionBuildingImport& Import = OutManifest.BuildingImports.Emplace_GetRef();
        ReadString(Object, TEXT("assetPath"), Import.AssetPath, TEXT("buildingImports[]"), Errors);
        ReadString(Object, TEXT("sourcePath"), Import.SourcePath, TEXT("buildingImports[]"), Errors);
        ReadString(Object, TEXT("assetClass"), Value, TEXT("buildingImports[]"), Errors); Import.AssetClass = FName(*Value);
    }

    const TSharedPtr<FJsonObject>* Cinematic = nullptr;
    if (!Root->TryGetObjectField(TEXT("cinematic"), Cinematic) || Cinematic == nullptr
        || !ExactKeys(*Cinematic, {TEXT("assetPath"), TEXT("shotSourcePath"),
            TEXT("gameplayGate")}, TEXT("cinematic"), Errors)) return false;
    ReadString(*Cinematic, TEXT("assetPath"), OutManifest.CinematicAssetPath, TEXT("cinematic"), Errors);
    ReadString(*Cinematic, TEXT("shotSourcePath"), OutManifest.CinematicShotSourcePath, TEXT("cinematic"), Errors);
    if (!(*Cinematic)->TryGetBoolField(TEXT("gameplayGate"), OutManifest.bCinematicGameplayGate)) return false;
    return Errors.IsEmpty() && Validate(OutManifest, Errors);
}

bool FDAFirstAscensionContentPipeline::Validate(
    const FDAFirstAscensionContentManifest& M, TArray<FText>& Errors)
{
    const TArray<FName> ExpectedForgeweave = {
        TEXT("forgeweave.worker_arcology"), TEXT("forgeweave.forge_quarters"),
        TEXT("forgeweave.production_directorate"), TEXT("forgeweave.industrial_design_bureau"),
        TEXT("forgeweave.industrial_exchange"), TEXT("forgeweave.machine_parts_market"),
        TEXT("forgeweave.infinite_foundry"), TEXT("forgeweave.replication_forge"),
        TEXT("forgeweave.freight_furnace"), TEXT("forgeweave.smog_reclaimer"),
        TEXT("forgeweave.forge_guard"), TEXT("forgeweave.mechanist_crew"),
        TEXT("forgeweave.workers_canteen"), TEXT("forgeweave.forge_lord_daxton_rhe"),
        TEXT("forgeweave.the_grand_forge")};
    const bool bExact = M.SchemaVersion == 1
        && M.ContentId == TEXT("ascension.forgeweave.first")
        && M.Fingerprint == TEXT("a179ed0ed1e9735624dfb0aceb90c30216586c3a")
        && M.QuestId == TEXT("quest.convergence_authority")
        && M.QuestTitle == TEXT("Convergence Authority")
        && M.QuestAssetPath == TEXT("/Game/DA/Quests/Q_ConvergenceAuthority")
        && M.QuestCompletionHistory == TEXT("convergence_authority_1_of_20")
        && M.DoctrineId == TEXT("doctrine.replication")
        && M.DoctrineAssetPath == TEXT("/Game/DA/Doctrines/DA_Doctrine_Replication")
        && M.ReplicationCadenceDevelopmentCycles == 12
        && M.FusionDefinitionId == TEXT("fusion.autonomous_factory")
        && M.FusionAssetPath == TEXT("/Game/DA/Cards/Fusion/DA_Card_AutonomousFactory")
        && M.ConstructionCycles == 8
        && M.CraftCapital == 80
        && M.CraftInsight == 0
        && M.RequiredCraftingFacilityId == TEXT("forgeweave.replication_forge")
        && M.UtilityPower == 24
        && M.UtilityData == 24
        && FMath::IsNearlyEqual(M.WorkforceRequirementModifier, -0.8f)
        && FMath::IsNearlyEqual(M.IndustrialThroughputModifier, 0.25f)
        && FMath::IsNearlyEqual(M.AdjacentIndustrialConstructionSpeedModifier, 0.15f)
        && FMath::IsNearlyEqual(M.DependencyPerCycle, 0.2f)
        && FMath::IsNearlyEqual(M.ResourceHungerPerCycle, 0.15f)
        && M.ForgeweaveDefinitionIds == ExpectedForgeweave
        && M.BuildingImports.Num() == 2
        && M.BuildingImports[0].AssetPath == TEXT("/Game/Buildings/Fusion/AutonomousFactory/Meshes/SM_AutonomousFactory")
        && M.BuildingImports[0].SourcePath == TEXT("ContentSource/Buildings/Fusion/AutonomousFactory/SM_AutonomousFactory.fbx")
        && M.BuildingImports[0].AssetClass == TEXT("StaticMesh")
        && M.BuildingImports[1].AssetPath == TEXT("/Game/Buildings/Fusion/AutonomousFactory/Textures/T_AutonomousFactory")
        && M.BuildingImports[1].SourcePath == TEXT("ContentSource/Buildings/Fusion/AutonomousFactory/T_AutonomousFactory.png")
        && M.BuildingImports[1].AssetClass == TEXT("Texture2D")
        && M.CinematicAssetPath == TEXT("/Game/Cinematics/CS_ForgeweaveAscension")
        && M.CinematicShotSourcePath == TEXT("ContentSource/Cinematics/ForgeweaveAscension.shotlist.json")
        && !M.bCinematicGameplayGate;
    if (!bExact) Errors.Add(FText::FromString(TEXT("First Ascension manifest differs from frozen v1 authority.")));
    return bExact;
}
