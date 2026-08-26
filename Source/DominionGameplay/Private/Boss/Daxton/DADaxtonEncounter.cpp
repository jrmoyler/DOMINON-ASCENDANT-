#include "Boss/Daxton/DADaxtonEncounter.h"

#include "Campaign/DAConquestCampaignState.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Save/DACampaignSaveGame.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr TCHAR FrozenFingerprint[] = TEXT("c752e7f2c882cc4acb0091c685009415c6e8d208");
    constexpr TCHAR LeaderPackage[] = TEXT("/Game/DA/Leaders/DA_Leader_DaxtonRhe");

    TSet<FString> Keys(std::initializer_list<const TCHAR*> Values)
    {
        TSet<FString> Result;
        for (const TCHAR* Value : Values) Result.Add(Value);
        return Result;
    }

    bool ExactKeys(const TSharedPtr<FJsonObject>& Object, const FString& At,
        const TSet<FString>& Required, TArray<FText>& Errors)
    {
        if (!Object.IsValid())
        {
            Errors.Add(FText::FromString(At + TEXT(" must be an object.")));
            return false;
        }
        bool bValid = true;
        for (const FString& Key : Required)
        {
            if (!Object->HasField(Key))
            {
                Errors.Add(FText::FromString(At + TEXT(" is missing '") + Key + TEXT("'.")));
                bValid = false;
            }
        }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
        {
            if (!Required.Contains(Pair.Key))
            {
                Errors.Add(FText::FromString(At + TEXT(" has unknown key '") + Pair.Key + TEXT("'.")));
                bValid = false;
            }
        }
        return bValid;
    }

    bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key,
        const FString& At, FString& Out, TArray<FText>& Errors)
    {
        if (!Object.IsValid() || !Object->TryGetStringField(Key, Out) || Out.IsEmpty())
        {
            Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" must be a non-empty string.")));
            return false;
        }
        return true;
    }

    bool ReadStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key,
        const FString& At, TArray<FString>& Out, TArray<FText>& Errors)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Key, Values) || Values == nullptr)
        {
            Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" must be an array.")));
            return false;
        }
        bool bValid = true;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Text;
            if (!Value.IsValid() || !Value->TryGetString(Text) || Text.IsEmpty())
            {
                Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" entries must be strings.")));
                bValid = false;
            }
            else Out.Add(MoveTemp(Text));
        }
        return bValid;
    }

    bool ParseLeaderState(const FString& Text, EDADaxtonLeaderState& Out)
    {
        if (Text == TEXT("Governor")) Out = EDADaxtonLeaderState::Governor;
        else if (Text == TEXT("IndustrialAdvisor")) Out = EDADaxtonLeaderState::IndustrialAdvisor;
        else if (Text == TEXT("AlliedForgeLord")) Out = EDADaxtonLeaderState::AlliedForgeLord;
        else if (Text == TEXT("Exile")) Out = EDADaxtonLeaderState::Exile;
        else if (Text == TEXT("Prisoner")) Out = EDADaxtonLeaderState::Prisoner;
        else if (Text == TEXT("Dead")) Out = EDADaxtonLeaderState::Dead;
        else return false;
        return true;
    }

    FDAWorldAssetRecord* FindGrandForge(FDACampaignSnapshot& Campaign)
    {
        return Campaign.WorldAssets.FindByPredicate([](const FDAWorldAssetRecord& Asset)
        {
            return Asset.CardDefinitionId == TEXT("forgeweave.grand_forge");
        });
    }

    const FDAWorldAssetRecord* FindGrandForge(const FDACampaignSnapshot& Campaign)
    {
        return Campaign.WorldAssets.FindByPredicate([](const FDAWorldAssetRecord& Asset)
        {
            return Asset.CardDefinitionId == TEXT("forgeweave.grand_forge");
        });
    }

    bool IsActionUnused(const FDADaxtonCampaignState& State, const FGuid ActionId)
    {
        return ActionId.IsValid() && State.StartActionId != ActionId
            && State.PhaseOneObjectiveActionId != ActionId
            && State.PhaseThreeActionId != ActionId
            && State.ResolutionActionId != ActionId
            && !State.PhaseOneActionIds.Contains(ActionId)
            && !State.ObjectiveActionIds.Contains(ActionId)
            && !State.InteractionRecords.ContainsByPredicate([ActionId](const FDADaxtonInteractionRecord& Record)
            {
                return Record.ActionId == ActionId;
            });
    }

    void AddHistory(FDACampaignSnapshot& Campaign, const FName Tag)
    {
        Campaign.HistoryTags.AddUnique(Tag);
        Campaign.HistoryTags.Sort([](const FName Left, const FName Right)
        {
            return Left.LexicalLess(Right);
        });
    }

    EDAStructureDamageState ModuleState(const FDAStructureModuleHealthRecord& Module)
    {
        if (Module.CurrentHealth <= 0.f) return EDAStructureDamageState::Disabled;
        const float Percent = 100.f * Module.CurrentHealth / Module.MaximumHealth;
        return Percent > 50.f ? EDAStructureDamageState::Operational
            : Percent > 25.f ? EDAStructureDamageState::Damaged
            : EDAStructureDamageState::Disabled;
    }

    void ReconcileProductionDisabled(FDAStructuralDamageRecord& Damage)
    {
        Damage.bProductionDisabled = Damage.Modules.ContainsByPredicate(
            [](const FDAStructureModuleHealthRecord& Module)
            {
                return Module.bDisablesProduction && Module.CurrentHealth <= 0.f;
            });
    }

    bool ValidateStartAuthorities(const FDACampaignSnapshot& Campaign, FString& OutError)
    {
        EDAForgeweaveRoute Route = EDAForgeweaveRoute::Force;
        const FDAWorldAssetRecord* Forge = FindGrandForge(Campaign);
        const FDAStructuralDamageRecord* Damage = Forge != nullptr
            ? Campaign.OperationConflict.FindStructuralDamageRecord(Forge->WorldAssetId) : nullptr;
        const bool bHasForgeWorker = Campaign.LiveSignals.Citizens.ContainsByPredicate(
            [](const FDACampaignCitizenSignal& Citizen)
            {
                return Citizen.CityId == TEXT("city.ironheart") && !Citizen.JobId.IsNone();
            });
        if (!FDADaxtonAuthorityValidator::ResolveCanonicalRoute(Campaign, Route, OutError)) return false;
        if (Campaign.WorldState.Diplomacy.FindRelationship(TEXT("relationship.synara.forgeweave")) == nullptr
            || Forge == nullptr || Damage == nullptr || Damage->Modules.Num() < 2
            || Forge->StructuralIntegrity <= 0.f || Forge->ConstructionState == EDAConstructionState::Ruined
            || Campaign.WorldState.Forgeweave.Population <= 0 || !bHasForgeWorker
            || !FMath::IsFinite(Campaign.WorldState.Forgeweave.ProductionReserve)
            || !FMath::IsFinite(Campaign.WorldState.Forgeweave.ActiveIndustrialThroughput)
            || !FMath::IsFinite(Campaign.WorldState.Forgeweave.ResourceHunger)
            || !Damage->Modules.ContainsByPredicate([](const FDAStructureModuleHealthRecord& Module)
                { return Module.ModuleId == TEXT("module.coolant"); })
            || !Damage->Modules.ContainsByPredicate([](const FDAStructureModuleHealthRecord& Module)
                { return Module.ModuleId == TEXT("module.production"); }))
        {
            OutError = TEXT("Daxton start requires canonical route, relationship, Grand Forge modules, workers, supply, production, and structure.");
            return false;
        }
        return true;
    }

    bool BeginCanonicalProof(const FGuid ActionId, const EDADaxtonCanonicalActionKind Kind,
        const float Strength, const FDACampaignSnapshot& Campaign,
        FDADaxtonCanonicalActionRecord& OutRecord, FString& OutError)
    {
        OutRecord = FDADaxtonCanonicalActionRecord();
        OutRecord.ActionId = ActionId;
        OutRecord.Kind = Kind;
        OutRecord.Strength = Strength;
        OutRecord.WorldTick = Campaign.WorldState.CurrentWorldTick;
        OutRecord.PhaseBefore = Campaign.DaxtonState.Phase;
        OutRecord.ArmorBefore = Campaign.DaxtonState.ArmorIntegrity;
        OutRecord.HeatBefore = Campaign.DaxtonState.Heat;
        OutRecord.CoolantBefore = Campaign.DaxtonState.CoolantStability;
        return FDADaxtonAuthorityValidator::CaptureCanonicalProjection(
            Campaign, OutRecord.Before, OutError);
    }

    bool FinishCanonicalProof(FDADaxtonCampaignState& State,
        const FDACampaignSnapshot& Campaign, FDADaxtonCanonicalActionRecord& Record,
        FString& OutError)
    {
        Record.PhaseAfter = State.Phase;
        Record.ArmorAfter = State.ArmorIntegrity;
        Record.HeatAfter = State.Heat;
        Record.CoolantAfter = State.CoolantStability;
        if (!FDADaxtonAuthorityValidator::CaptureCanonicalProjection(
            Campaign, Record.After, OutError)) return false;
        State.CanonicalActionRecords.Add(MoveTemp(Record));
        return true;
    }
}

FPrimaryAssetId UDADaxtonLeaderDefinition::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("DALeader"), LeaderId);
}

FString FDADaxtonContentPipeline::GetCanonicalManifestPath()
{
    return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DA/Manifests/DaxtonEncounter.json"));
}

bool FDADaxtonContentPipeline::LoadCanonical(
    FDADaxtonContentManifest& OutManifest, TArray<FText>& Errors)
{
    return LoadFile(GetCanonicalManifestPath(), OutManifest, Errors);
}

bool FDADaxtonContentPipeline::LoadFile(const FString& Filename,
    FDADaxtonContentManifest& OutManifest, TArray<FText>& Errors)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Filename))
    {
        Errors.Add(FText::FromString(TEXT("Could not read Daxton manifest: ") + Filename));
        return false;
    }
    return ParseJson(Json, OutManifest, Errors);
}

bool FDADaxtonContentPipeline::ParseJson(const FString& Json,
    FDADaxtonContentManifest& Out, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num();
    Out = FDADaxtonContentManifest();
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        Errors.Add(FText::FromString(TEXT("Daxton manifest is not valid JSON.")));
        return false;
    }
    const TSet<FString> RootKeys = Keys({TEXT("schemaVersion"), TEXT("contentId"),
        TEXT("fingerprint"), TEXT("leaderAssetPath"), TEXT("phases"),
        TEXT("leaderStates"), TEXT("characterImports")});
    ExactKeys(Root, TEXT("manifest"), RootKeys, Errors);
    double Version = 0.0;
    if (!Root->TryGetNumberField(TEXT("schemaVersion"), Version)
        || Version != static_cast<double>(static_cast<int32>(Version)))
        Errors.Add(FText::FromString(TEXT("manifest.schemaVersion must be an integer.")));
    else Out.SchemaVersion = static_cast<int32>(Version);
    FString Text;
    if (ReadString(Root, TEXT("contentId"), TEXT("manifest"), Text, Errors)) Out.ContentId = FName(*Text);
    ReadString(Root, TEXT("fingerprint"), TEXT("manifest"), Out.Fingerprint, Errors);
    ReadString(Root, TEXT("leaderAssetPath"), TEXT("manifest"), Out.LeaderAssetPath, Errors);

    const TArray<TSharedPtr<FJsonValue>>* Phases = nullptr;
    if (!Root->TryGetArrayField(TEXT("phases"), Phases) || Phases == nullptr)
        Errors.Add(FText::FromString(TEXT("manifest.phases must be an array.")));
    else for (int32 Index = 0; Index < Phases->Num(); ++Index)
    {
        const FString At = FString::Printf(TEXT("phases[%d]"), Index);
        const TSharedPtr<FJsonObject> Object = (*Phases)[Index].IsValid()
            && (*Phases)[Index]->Type == EJson::Object ? (*Phases)[Index]->AsObject() : nullptr;
        const TSet<FString> PhaseKeys = Keys({TEXT("id"), TEXT("mechanics")});
        ExactKeys(Object, At, PhaseKeys, Errors);
        FDADaxtonContentPhase& Phase = Out.Phases.Emplace_GetRef();
        if (ReadString(Object, TEXT("id"), At, Text, Errors)) Phase.PhaseId = FName(*Text);
        TArray<FString> Mechanics;
        ReadStringArray(Object, TEXT("mechanics"), At, Mechanics, Errors);
        for (const FString& Mechanic : Mechanics) Phase.Mechanics.Add(FName(*Mechanic));
    }

    TArray<FString> States;
    ReadStringArray(Root, TEXT("leaderStates"), TEXT("manifest"), States, Errors);
    for (const FString& StateText : States)
    {
        EDADaxtonLeaderState State = EDADaxtonLeaderState::Governor;
        if (!ParseLeaderState(StateText, State))
            Errors.Add(FText::FromString(TEXT("manifest has an unknown Leader state.")));
        else Out.LeaderStates.Add(State);
    }

    const TArray<TSharedPtr<FJsonValue>>* Imports = nullptr;
    if (!Root->TryGetArrayField(TEXT("characterImports"), Imports) || Imports == nullptr)
        Errors.Add(FText::FromString(TEXT("manifest.characterImports must be an array.")));
    else for (int32 Index = 0; Index < Imports->Num(); ++Index)
    {
        const FString At = FString::Printf(TEXT("characterImports[%d]"), Index);
        const TSharedPtr<FJsonObject> Object = (*Imports)[Index].IsValid()
            && (*Imports)[Index]->Type == EJson::Object ? (*Imports)[Index]->AsObject() : nullptr;
        const TSet<FString> ImportKeys = Keys({TEXT("assetPath"), TEXT("sourcePath"), TEXT("assetClass")});
        ExactKeys(Object, At, ImportKeys, Errors);
        FDADaxtonCharacterImport& Import = Out.CharacterImports.Emplace_GetRef();
        ReadString(Object, TEXT("assetPath"), At, Import.AssetPath, Errors);
        ReadString(Object, TEXT("sourcePath"), At, Import.SourcePath, Errors);
        if (ReadString(Object, TEXT("assetClass"), At, Text, Errors)) Import.AssetClass = FName(*Text);
    }
    return Errors.Num() == Before && Validate(Out, Errors);
}

bool FDADaxtonContentPipeline::Validate(
    const FDADaxtonContentManifest& Manifest, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num();
    static const FName PhaseIds[] = {TEXT("phase.the_forge_lord"), TEXT("phase.overdrive"), TEXT("phase.the_choice")};
    static const TArray<FName> PhaseMechanics[] = {
        {TEXT("powered_industrial_armor"), TEXT("forge_guard_reinforcement"), TEXT("hardened_cover"), TEXT("grand_forge_production")},
        {TEXT("production_speed"), TEXT("heat"), TEXT("coolant_stability"), TEXT("resource_hunger")},
        {TEXT("defeat_daxton"), TEXT("save_grand_forge"), TEXT("evacuate_workers"), TEXT("stabilize_production_offer_union")}
    };
    static const FString AssetPaths[] = {
        TEXT("/Game/Characters/Daxton/Meshes/SK_DaxtonRhe"),
        TEXT("/Game/Characters/Daxton/Animations/A_Daxton_PoweredArmor"),
        TEXT("/Game/Characters/Daxton/Animations/A_Daxton_Overdrive"),
        TEXT("/Game/Characters/Daxton/Animations/A_Daxton_Choice")
    };
    static const FString SourcePaths[] = {
        TEXT("ContentSource/Characters/Daxton/DaxtonRhe.fbx"),
        TEXT("ContentSource/Characters/Daxton/DaxtonRhe_PoweredArmor.fbx"),
        TEXT("ContentSource/Characters/Daxton/DaxtonRhe_Overdrive.fbx"),
        TEXT("ContentSource/Characters/Daxton/DaxtonRhe_Choice.fbx")
    };
    if (Manifest.SchemaVersion != 1 || Manifest.ContentId != TEXT("encounter.daxton_rhe.final")
        || Manifest.Fingerprint != FrozenFingerprint || Manifest.LeaderAssetPath != LeaderPackage
        || Manifest.Phases.Num() != UE_ARRAY_COUNT(PhaseIds)
        || Manifest.LeaderStates.Num() != 6 || Manifest.CharacterImports.Num() != UE_ARRAY_COUNT(AssetPaths))
        Errors.Add(FText::FromString(TEXT("Daxton manifest identity, counts, paths, or fingerprint are not frozen.")));
    for (int32 Index = 0; Index < Manifest.Phases.Num(); ++Index)
        if (Index >= UE_ARRAY_COUNT(PhaseIds)
            || PhaseIds[Index] != Manifest.Phases[Index].PhaseId
            || Manifest.Phases[Index].Mechanics != PhaseMechanics[Index])
            Errors.Add(FText::FromString(TEXT("Daxton exact phase order/mechanics diverge.")));
    for (int32 Index = 0; Index < Manifest.LeaderStates.Num(); ++Index)
        if (Manifest.LeaderStates[Index] != static_cast<EDADaxtonLeaderState>(Index))
            Errors.Add(FText::FromString(TEXT("Daxton exact six Leader states diverge.")));
    for (int32 Index = 0; Index < Manifest.CharacterImports.Num(); ++Index)
    {
        const FDADaxtonCharacterImport& Import = Manifest.CharacterImports[Index];
        const FName ExpectedClass = Index == 0 ? FName(TEXT("SkeletalMesh")) : FName(TEXT("AnimSequence"));
        if (Index >= UE_ARRAY_COUNT(AssetPaths)
            || Import.AssetPath != AssetPaths[Index] || Import.SourcePath != SourcePaths[Index]
            || Import.AssetClass != ExpectedClass || FPackageName::IsShortPackageName(Import.AssetPath))
            Errors.Add(FText::FromString(TEXT("Daxton character imports require exact package, source, and class identities.")));
    }
    return Errors.Num() == Before;
}

bool FDADaxtonContentPipeline::BuildLeaderDefinition(const FDADaxtonContentManifest& Manifest,
    UDADaxtonLeaderDefinition*& OutLeader, TArray<FText>& Errors)
{
    OutLeader = nullptr;
    if (!Validate(Manifest, Errors)) return false;
    OutLeader = NewObject<UDADaxtonLeaderDefinition>(GetTransientPackage());
    OutLeader->EncounterId = Manifest.ContentId;
    OutLeader->SupportedStates = Manifest.LeaderStates;
    OutLeader->SourceFingerprint = Manifest.Fingerprint;
    OutLeader->bRuntimeManifestFallback = true;
    return true;
}

bool FDADaxtonContentPipeline::ValidateGeneratedCache(const FDADaxtonContentManifest& Manifest,
    const UDADaxtonLeaderDefinition* Leader, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num();
    if (!Validate(Manifest, Errors) || Leader == nullptr
        || Leader->LeaderId != TEXT("leader.daxton_rhe") || Leader->EncounterId != Manifest.ContentId
        || Leader->SupportedStates != Manifest.LeaderStates
        || Leader->SourceFingerprint != Manifest.Fingerprint || Leader->bRuntimeManifestFallback)
        Errors.Add(FText::FromString(TEXT("Generated Daxton Leader cache diverges from the canonical source manifest.")));
    return Errors.Num() == Before;
}

bool FDADaxtonEncounter::Start(const FGuid ActionId,
    FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    if (!ActionId.IsValid() || InOutCampaign.DaxtonState.Phase != EDADaxtonEncounterPhase::Inactive)
    {
        OutError = TEXT("Daxton encounter requires one valid unused start action and inactive state.");
        return false;
    }
    if (!ValidateStartAuthorities(InOutCampaign, OutError)) return false;
    FDADaxtonCampaignState& State = InOutCampaign.DaxtonState;
    if (!FDADaxtonAuthorityValidator::CaptureCanonicalProjection(
        InOutCampaign, State.InitialCanonicalProjection, OutError)) return false;
    State.Phase = EDADaxtonEncounterPhase::PhaseOne;
    State.StartActionId = ActionId;
    State.StartedWorldTick = InOutCampaign.WorldState.CurrentWorldTick;
    State.ArmorIntegrity = 100.f;
    State.CoolantStability = 0.f;
    State.Heat = 0.f;
    State.bPoweredArmorActive = true;
    return true;
}

bool FDADaxtonEncounter::AdvanceGrandForgeProduction(const FGuid ActionId,
    FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    if (!IsActionUnused(InOutCampaign.DaxtonState, ActionId)
        || InOutCampaign.DaxtonState.Phase != EDADaxtonEncounterPhase::PhaseOne
        || !InOutCampaign.DaxtonState.InteractionRecords.IsEmpty())
    {
        OutError = TEXT("Grand Forge production loop is available only in Phase I with a unique action.");
        return false;
    }
    FDADaxtonCanonicalActionRecord Proof;
    if (!BeginCanonicalProof(ActionId, EDADaxtonCanonicalActionKind::AdvanceProduction,
        0.f, InOutCampaign, Proof, OutError)) return false;
    ++InOutCampaign.DaxtonState.GrandForgeProductionCycles;
    InOutCampaign.DaxtonState.PhaseOneActionIds.Add(ActionId);
    FDAForgeweaveCityState& Forgeweave = InOutCampaign.WorldState.Forgeweave;
    Forgeweave.ProductionReserve += 5.f;
    Forgeweave.ActiveIndustrialThroughput += 2.f;
    Forgeweave.ResourceHunger = FMath::Clamp(Forgeweave.ResourceHunger + 2.f, 0.f, 100.f);
    return FinishCanonicalProof(InOutCampaign.DaxtonState, InOutCampaign, Proof, OutError);
}

bool FDADaxtonEncounter::DeployHardenedCover(const FGuid ActionId,
    FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    if (!IsActionUnused(InOutCampaign.DaxtonState, ActionId)
        || InOutCampaign.DaxtonState.Phase != EDADaxtonEncounterPhase::PhaseOne
        || InOutCampaign.DaxtonState.HardenedCoverDeployments != 0)
    {
        OutError = TEXT("Hardened cover deployment is available only in Phase I with a unique action.");
        return false;
    }
    FDADaxtonCanonicalActionRecord Proof;
    if (!BeginCanonicalProof(ActionId, EDADaxtonCanonicalActionKind::DeployHardenedCover,
        0.f, InOutCampaign, Proof, OutError)) return false;
    FDAWorldAssetRecord* Forge = FindGrandForge(InOutCampaign);
    FDAStructuralDamageRecord* Damage = Forge != nullptr
        ? InOutCampaign.OperationConflict.FindStructuralDamageRecord(Forge->WorldAssetId) : nullptr;
    if (Damage == nullptr)
    {
        OutError = TEXT("Hardened cover requires the canonical Grand Forge structural service.");
        return false;
    }
    Damage->Modules.Add(FDAStructureModuleHealthRecord(TEXT("module.hardened_cover"), 100.f, false));
    Damage->Modules.Sort([](const auto& Left, const auto& Right)
        { return Left.ModuleId.LexicalLess(Right.ModuleId); });
    ++InOutCampaign.DaxtonState.HardenedCoverDeployments;
    InOutCampaign.DaxtonState.PhaseOneActionIds.Add(ActionId);
    return FinishCanonicalProof(InOutCampaign.DaxtonState, InOutCampaign, Proof, OutError);
}

bool FDADaxtonEncounter::ReinforceForgeGuard(const FGuid ActionId,
    FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    if (!IsActionUnused(InOutCampaign.DaxtonState, ActionId)
        || InOutCampaign.DaxtonState.Phase != EDADaxtonEncounterPhase::PhaseOne
        || InOutCampaign.LiveSignals.MutationRevision == MAX_int64)
    {
        OutError = TEXT("Forge Guard reinforcement is available only in Phase I with a unique action.");
        return false;
    }
    FDADaxtonCanonicalActionRecord Proof;
    if (!BeginCanonicalProof(ActionId, EDADaxtonCanonicalActionKind::ReinforceForgeGuard,
        0.f, InOutCampaign, Proof, OutError)) return false;
    const FDAWorldAssetRecord* Forge = FindGrandForge(InOutCampaign);
    if (Forge == nullptr) return false;
    FDACampaignJobOpeningSignal* Opening = InOutCampaign.LiveSignals.JobOpenings.FindByPredicate(
        [Forge](const FDACampaignJobOpeningSignal& Candidate)
        { return Candidate.JobId == TEXT("job.forgeweave.forge_guard.reinforcement")
            && Candidate.FacilityWorldAssetId == Forge->WorldAssetId; });
    if (Opening == nullptr)
    {
        Opening = &InOutCampaign.LiveSignals.JobOpenings.Emplace_GetRef();
        Opening->JobId = TEXT("job.forgeweave.forge_guard.reinforcement");
        Opening->CityId = TEXT("city.ironheart");
        Opening->FacilityWorldAssetId = Forge->WorldAssetId;
    }
    Opening->OpenPositions += 4;
    InOutCampaign.LiveSignals.JobOpenings.Sort([](const auto& Left, const auto& Right)
        { return Left.JobId != Right.JobId ? Left.JobId.LexicalLess(Right.JobId)
            : Left.FacilityWorldAssetId.ToString().Compare(
                Right.FacilityWorldAssetId.ToString()) < 0; });
    ++InOutCampaign.LiveSignals.MutationRevision;
    InOutCampaign.WorldState.Forgeweave.DefensePressure = FMath::Clamp(
        InOutCampaign.WorldState.Forgeweave.DefensePressure + 5.f, 0.f, 100.f);
    ++InOutCampaign.DaxtonState.ForgeGuardReinforcements;
    InOutCampaign.DaxtonState.PhaseOneActionIds.Add(ActionId);
    return FinishCanonicalProof(InOutCampaign.DaxtonState, InOutCampaign, Proof, OutError);
}

bool FDADaxtonEncounter::CompletePhaseOneIndustrialObjective(const FGuid ActionId,
    FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    FDADaxtonCampaignState& State = InOutCampaign.DaxtonState;
    if (!IsActionUnused(State, ActionId) || State.Phase != EDADaxtonEncounterPhase::PhaseOne
        || State.GrandForgeProductionCycles < 1 || State.HardenedCoverDeployments < 1
        || State.ForgeGuardReinforcements < 1 || State.ArmorIntegrity <= 60.f
        || !State.InteractionRecords.IsEmpty())
    {
        OutError = TEXT("Phase I industrial objective requires production, cover, reinforcement, and a unique action.");
        return false;
    }
    FDADaxtonCanonicalActionRecord Proof;
    if (!BeginCanonicalProof(ActionId,
        EDADaxtonCanonicalActionKind::CompletePhaseOneIndustrialObjective,
        0.f, InOutCampaign, Proof, OutError)) return false;
    State.bPhaseOneIndustrialObjectiveCompleted = true;
    State.PhaseOneObjectiveActionId = ActionId;
    State.Phase = EDADaxtonEncounterPhase::PhaseTwo;
    FDAForgeweaveCityState& Forgeweave = InOutCampaign.WorldState.Forgeweave;
    Forgeweave.bOverdrive = true;
    Forgeweave.ActiveIndustrialThroughput *= 1.5f;
    Forgeweave.ResourceHunger = FMath::Clamp(Forgeweave.ResourceHunger + 10.f, 0.f, 100.f);
    State.Heat = FMath::Max(State.Heat, 20.f);
    State.CoolantStability = 100.f;
    return FinishCanonicalProof(State, InOutCampaign, Proof, OutError);
}

bool FDADaxtonEncounter::ApplySystemInteraction(const FGuid ActionId,
    const EDADaxtonInteraction Interaction, const float Strength,
    FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    FDADaxtonCampaignState& State = InOutCampaign.DaxtonState;
    if (!IsActionUnused(State, ActionId) || !FMath::IsFinite(Strength) || Strength <= 0.f
        || (State.Phase != EDADaxtonEncounterPhase::PhaseOne
            && State.Phase != EDADaxtonEncounterPhase::PhaseTwo)
        || (State.Phase == EDADaxtonEncounterPhase::PhaseOne
            && Interaction != EDADaxtonInteraction::Damage)
        || static_cast<uint8>(Interaction) > static_cast<uint8>(EDADaxtonInteraction::WorkerShutdown))
    {
        OutError = TEXT("Daxton interaction is invalid for the current phase, strength, or action identity.");
        return false;
    }
    if (Interaction == EDADaxtonInteraction::WorkerShutdown
        && InOutCampaign.LiveSignals.MutationRevision == MAX_int64)
    {
        OutError = TEXT("Worker shutdown cannot overflow canonical live-signal revision.");
        return false;
    }
    FDADaxtonCanonicalActionRecord Proof;
    if (!BeginCanonicalProof(ActionId, EDADaxtonCanonicalActionKind::SystemInteraction,
        Strength, InOutCampaign, Proof, OutError)) return false;
    Proof.Interaction = Interaction;

    FDADaxtonInteractionRecord Record;
    Record.ActionId = ActionId;
    Record.Interaction = Interaction;
    Record.Strength = Strength;
    Record.ArmorIntegrityBefore = State.ArmorIntegrity;
    Record.HeatBefore = State.Heat;
    Record.CoolantBefore = State.CoolantStability;
    Record.ProductionBefore = InOutCampaign.WorldState.Forgeweave.ActiveIndustrialThroughput;
    Record.ResourceHungerBefore = InOutCampaign.WorldState.Forgeweave.ResourceHunger;
    Record.WorldTick = InOutCampaign.WorldState.CurrentWorldTick;

    FDAForgeweaveCityState& Forgeweave = InOutCampaign.WorldState.Forgeweave;
    FDAWorldAssetRecord* Forge = FindGrandForge(InOutCampaign);
    FDAStructuralDamageRecord* Damage = Forge != nullptr
        ? InOutCampaign.OperationConflict.FindStructuralDamageRecord(Forge->WorldAssetId) : nullptr;
    switch (Interaction)
    {
    case EDADaxtonInteraction::Damage:
        State.ArmorIntegrity = FMath::Clamp(State.ArmorIntegrity - Strength, 0.f, 100.f);
        State.Heat = FMath::Clamp(State.Heat + Strength * 0.1f, 0.f, 100.f);
        break;
    case EDADaxtonInteraction::DisableCoolant:
        State.CoolantStability = 0.f;
        State.Heat = FMath::Clamp(State.Heat + 25.f, 0.f, 100.f);
        Forgeweave.ResourceHunger = FMath::Clamp(Forgeweave.ResourceHunger + 5.f, 0.f, 100.f);
        if (Damage != nullptr)
        {
            if (FDAStructureModuleHealthRecord* Module = Damage->Modules.FindByPredicate(
                [](const FDAStructureModuleHealthRecord& Candidate)
                { return Candidate.ModuleId == TEXT("module.coolant"); }))
            {
                Module->CurrentHealth = 0.f;
                Module->State = EDAStructureDamageState::Disabled;
                ReconcileProductionDisabled(*Damage);
            }
        }
        break;
    case EDADaxtonInteraction::RedirectSupply:
        Forgeweave.ProductionReserve = FMath::Max(0.f, Forgeweave.ProductionReserve - Strength);
        Forgeweave.ActiveIndustrialThroughput = FMath::Max(0.f,
            Forgeweave.ActiveIndustrialThroughput - Strength * 0.5f);
        Forgeweave.LogisticsEfficiency = FMath::Max(0.f, Forgeweave.LogisticsEfficiency - Strength);
        Forgeweave.ResourceHunger = FMath::Clamp(Forgeweave.ResourceHunger + 5.f, 0.f, 100.f);
        break;
    case EDADaxtonInteraction::HackProduction:
        Forgeweave.ActiveIndustrialThroughput = FMath::Max(0.f,
            Forgeweave.ActiveIndustrialThroughput - Strength);
        Forgeweave.ProductionReserve = FMath::Max(0.f, Forgeweave.ProductionReserve - Strength * 0.5f);
        State.Heat = FMath::Clamp(State.Heat + 10.f, 0.f, 100.f);
        if (Damage != nullptr)
        {
            if (FDAStructureModuleHealthRecord* Module = Damage->Modules.FindByPredicate(
                [](const FDAStructureModuleHealthRecord& Candidate)
                { return Candidate.ModuleId == TEXT("module.production"); }))
            {
                Module->CurrentHealth = FMath::Max(0.f, Module->CurrentHealth - Strength);
                Module->State = ModuleState(*Module);
                ReconcileProductionDisabled(*Damage);
            }
        }
        break;
    case EDADaxtonInteraction::WorkerShutdown:
        Forgeweave.ActiveIndustrialThroughput *= 0.5f;
        State.Heat = FMath::Max(0.f, State.Heat - 15.f);
        for (FDACampaignCitizenSignal& Citizen : InOutCampaign.LiveSignals.Citizens)
            if (Citizen.CityId == TEXT("city.ironheart")) Citizen.JobId = NAME_None;
        InOutCampaign.LiveSignals.JobAssignments.RemoveAll(
            [](const FDACampaignJobAssignmentSignal& Assignment)
            {
                return Assignment.JobId.ToString().StartsWith(TEXT("job.forgeweave."));
            });
        ++InOutCampaign.LiveSignals.MutationRevision;
        AddHistory(InOutCampaign, TEXT("workers_protected"));
        break;
    }

    if (State.Phase == EDADaxtonEncounterPhase::PhaseOne && State.ArmorIntegrity <= 60.f)
    {
        State.Phase = EDADaxtonEncounterPhase::PhaseTwo;
        Forgeweave.bOverdrive = true;
        Forgeweave.ActiveIndustrialThroughput *= 1.5f;
        Forgeweave.ResourceHunger = FMath::Clamp(Forgeweave.ResourceHunger + 10.f, 0.f, 100.f);
        State.Heat = FMath::Max(State.Heat, 20.f);
        State.CoolantStability = 100.f;
    }

    Record.ArmorIntegrityAfter = State.ArmorIntegrity;
    Record.HeatAfter = State.Heat;
    Record.CoolantAfter = State.CoolantStability;
    Record.ProductionAfter = Forgeweave.ActiveIndustrialThroughput;
    Record.ResourceHungerAfter = Forgeweave.ResourceHunger;
    State.InteractionRecords.Add(MoveTemp(Record));
    return FinishCanonicalProof(State, InOutCampaign, Proof, OutError);
}

bool FDADaxtonEncounter::EnterPhaseThree(const FGuid ActionId,
    FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    FDADaxtonCampaignState& State = InOutCampaign.DaxtonState;
    if (!IsActionUnused(State, ActionId) || State.Phase != EDADaxtonEncounterPhase::PhaseTwo)
    {
        OutError = TEXT("Daxton Phase III requires a unique transition action from Phase II.");
        return false;
    }
    TSet<EDADaxtonInteraction> SystemInteractions;
    for (const FDADaxtonInteractionRecord& Record : State.InteractionRecords)
        if (Record.Interaction != EDADaxtonInteraction::Damage) SystemInteractions.Add(Record.Interaction);
    if (SystemInteractions.Num() < 2)
    {
        OutError = TEXT("Daxton cannot enter the choice phase solely because powered-armor health reached zero.");
        return false;
    }
    FDADaxtonCanonicalActionRecord Proof;
    if (!BeginCanonicalProof(ActionId, EDADaxtonCanonicalActionKind::EnterChoicePhase,
        0.f, InOutCampaign, Proof, OutError)) return false;
    State.Phase = EDADaxtonEncounterPhase::PhaseThree;
    State.PhaseThreeActionId = ActionId;
    return FinishCanonicalProof(State, InOutCampaign, Proof, OutError);
}

bool FDADaxtonEncounter::CompleteChoiceObjective(const FGuid ActionId,
    const EDADaxtonChoiceObjective Objective, FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    FDADaxtonCampaignState& State = InOutCampaign.DaxtonState;
    if (!IsActionUnused(State, ActionId) || State.Phase != EDADaxtonEncounterPhase::PhaseThree
        || State.CompletedObjectives.Contains(Objective)
        || static_cast<uint8>(Objective) > static_cast<uint8>(EDADaxtonChoiceObjective::StabilizeProductionOfferUnion))
    {
        OutError = TEXT("Daxton choice objective requires Phase III, a unique action, and an incomplete exact objective.");
        return false;
    }
    const FDAWorldAssetRecord* Forge = FindGrandForge(InOutCampaign);
    FDADaxtonCanonicalActionRecord Proof;
    if (!BeginCanonicalProof(ActionId, EDADaxtonCanonicalActionKind::CompleteChoiceObjective,
        0.f, InOutCampaign, Proof, OutError)) return false;
    Proof.Objective = Objective;
    switch (Objective)
    {
    case EDADaxtonChoiceObjective::DefeatDaxton:
        if (State.ArmorIntegrity != 0.f)
        {
            OutError = TEXT("Defeat Daxton requires powered-armor integrity to reach zero.");
            return false;
        }
        break;
    case EDADaxtonChoiceObjective::SaveGrandForge:
        if (Forge == nullptr || Forge->StructuralIntegrity <= 0.f
            || Forge->ConstructionState == EDAConstructionState::Ruined)
        {
            OutError = TEXT("Save Grand Forge requires the canonical structure to survive.");
            return false;
        }
        AddHistory(InOutCampaign, TEXT("grand_forge_preserved"));
        break;
    case EDADaxtonChoiceObjective::EvacuateWorkers:
        if (!State.InteractionRecords.ContainsByPredicate([](const FDADaxtonInteractionRecord& Record)
            { return Record.Interaction == EDADaxtonInteraction::WorkerShutdown; })
            || !InOutCampaign.HistoryTags.Contains(TEXT("workers_protected")))
        {
            OutError = TEXT("Evacuate workers requires the canonical worker shutdown and protection authority.");
            return false;
        }
        break;
    case EDADaxtonChoiceObjective::StabilizeProductionOfferUnion:
    {
        EDAForgeweaveRoute Route = EDAForgeweaveRoute::Force;
        if (!FDADaxtonAuthorityValidator::ResolveCanonicalRoute(InOutCampaign, Route, OutError)
            || Route == EDAForgeweaveRoute::Force)
        {
            if (OutError.IsEmpty()) OutError = TEXT("Force route cannot offer a production union.");
            return false;
        }
        InOutCampaign.WorldState.Forgeweave.bOverdrive = false;
        State.Heat = FMath::Min(State.Heat, 50.f);
        State.CoolantStability = FMath::Max(State.CoolantStability, 50.f);
        FDAWorldAssetRecord* MutableForge = FindGrandForge(InOutCampaign);
        FDAStructuralDamageRecord* Damage = MutableForge != nullptr
            ? InOutCampaign.OperationConflict.FindStructuralDamageRecord(MutableForge->WorldAssetId) : nullptr;
        if (Damage != nullptr)
        {
            for (FDAStructureModuleHealthRecord& Module : Damage->Modules)
                if (Module.ModuleId == TEXT("module.coolant") || Module.ModuleId == TEXT("module.production"))
                {
                    Module.CurrentHealth = FMath::Max(Module.CurrentHealth, Module.MaximumHealth * 0.6f);
                    Module.State = ModuleState(Module);
                }
            ReconcileProductionDisabled(*Damage);
        }
        break;
    }
    }
    State.CompletedObjectives.Add(Objective);
    State.ObjectiveActionIds.Add(ActionId);
    return FinishCanonicalProof(State, InOutCampaign, Proof, OutError);
}

bool FDADaxtonEncounter::CanResolveLeaderState(const EDADaxtonLeaderState State,
    const FDACampaignSnapshot& Campaign, FString& OutError)
{
    return FDADaxtonAuthorityValidator::CanResolveLeaderState(State, Campaign, OutError);
}

bool FDADaxtonEncounter::ResolveLeaderState(const FGuid ActionId, const EDADaxtonLeaderState State,
    FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    if (!IsActionUnused(InOutCampaign.DaxtonState, ActionId)
        || !CanResolveLeaderState(State, InOutCampaign, OutError))
    {
        if (OutError.IsEmpty()) OutError = TEXT("Daxton resolution action is invalid or already consumed.");
        return false;
    }
    FDACampaignSnapshot Candidate = InOutCampaign;
    FDADaxtonCanonicalActionRecord Proof;
    if (!BeginCanonicalProof(ActionId, EDADaxtonCanonicalActionKind::ResolveLeader,
        0.f, InOutCampaign, Proof, OutError)) return false;
    Proof.LeaderState = State;
    Candidate.DaxtonState.Phase = EDADaxtonEncounterPhase::Resolved;
    Candidate.DaxtonState.bLeaderResolved = true;
    Candidate.DaxtonState.LeaderState = State;
    Candidate.DaxtonState.ResolutionActionId = ActionId;
    Candidate.DaxtonState.ResolvedWorldTick = Candidate.WorldState.CurrentWorldTick;
    const FDADiplomaticRelationship* ResolutionRelationship =
        Candidate.WorldState.Diplomacy.FindRelationship(
            TEXT("relationship.synara.forgeweave"));
    if (ResolutionRelationship == nullptr)
    {
        OutError = TEXT("Daxton resolution cannot anchor a missing canonical Forgeweave relationship.");
        return false;
    }
    Candidate.DaxtonState.ResolutionRelationshipReasonCount =
        ResolutionRelationship->ReasonLedger.Num();
    Candidate.DaxtonState.ResolutionRelationshipReasonMutationIds.Reset(
        ResolutionRelationship->ReasonLedger.Num());
    for (const FDADiplomaticReason& Reason : ResolutionRelationship->ReasonLedger)
    {
        Candidate.DaxtonState.ResolutionRelationshipReasonMutationIds.Add(
            Reason.MutationId);
    }
    Candidate.DaxtonState.bPoweredArmorActive = false;
    AddHistory(Candidate, TEXT("daxton_encounter_resolved"));
    AddHistory(Candidate, FDADaxtonAuthorityValidator::GetLeaderHistoryTag(State));
    if (!FinishCanonicalProof(Candidate.DaxtonState, Candidate, Proof, OutError)) return false;
    if (!FDADaxtonAuthorityValidator::ValidateCampaignState(Candidate, OutError)) return false;
    InOutCampaign = MoveTemp(Candidate);
    return true;
}
