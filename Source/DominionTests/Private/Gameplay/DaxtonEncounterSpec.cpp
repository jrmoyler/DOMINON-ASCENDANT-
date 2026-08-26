#include "Boss/Daxton/DADaxtonEncounter.h"
#include "Campaign/DADaxtonCampaignState.h"
#include "Dom/JsonObject.h"
#include "Diplomacy/DADiplomacySystem.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Save/DACampaignSaveGame.h"
#include "Save/DASaveJsonFields.h"
#include "Save/DASaveSchema.h"
#include "Save/DASaveService.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    void LinkFixtureAsset(FDACampaignSnapshot& Campaign, FDAWorldAssetRecord& Asset,
        const FGuid CardInstanceId)
    {
        const bool bCanonicalFixture = Campaign.WorldState.ClockAuthority.bCaptured
            || Campaign.CitySimulationState.bInitialized
            || Campaign.DeckState.GetInstanceIds().Num() == FDADeckState::RequiredDeckSize;
        if (!bCanonicalFixture) return;
        Asset.CardInstanceId = CardInstanceId;
        Campaign.CollectionState.AddInstanceWithId(CardInstanceId, Asset.CardDefinitionId,
            EDAAcquisitionSource::Conquest, Campaign.WorldState.CurrentWorldTick);
        if (FCardInstance* Card = Campaign.CollectionState.FindInstance(CardInstanceId))
        {
            Card->WorldAssetId = Asset.WorldAssetId;
            Card->RecoveryState = EDARecoveryState::Deployed;
        }
    }

    void AddSortedHistory(FDACampaignSnapshot& Campaign, const FName Tag)
    {
        Campaign.HistoryTags.AddUnique(Tag);
        Campaign.HistoryTags.Sort([](const FName Left, const FName Right)
        {
            return Left.LexicalLess(Right);
        });
    }

    void AddRelationshipReason(FDADiplomaticRelationship& Relationship,
        const EDADiplomaticMetric Metric, const float Magnitude, const FName MutationId,
        const int64 WorldTick)
    {
        if (Magnitude == 0.f) return;
        FDADiplomaticReason& Reason = Relationship.ReasonLedger.Emplace_GetRef();
        Reason.MutationId = MutationId;
        Reason.SourceTag = TEXT("test.daxton.canonical_relationship");
        Reason.Metric = Metric;
        Reason.Magnitude = Magnitude;
        Reason.WorldTick = WorldTick;
        float* Aggregate = Metric == EDADiplomaticMetric::Trust ? &Relationship.Trust
            : Metric == EDADiplomaticMetric::Respect ? &Relationship.Respect
            : Metric == EDADiplomaticMetric::Grievance ? &Relationship.Grievance : nullptr;
        if (Aggregate != nullptr) *Aggregate += Magnitude;
    }

    void AddGrandForgeAuthority(FDACampaignSnapshot& Campaign)
    {
        FDAWorldAssetRecord& Forge = Campaign.WorldAssets.Emplace_GetRef();
        Forge.WorldAssetId = FGuid(24, 1, 1, 1);
        Forge.CardDefinitionId = TEXT("forgeweave.grand_forge");
        Forge.CityId = TEXT("city.ironheart");
        Forge.OwnerCivilizationId = TEXT("civilization.forgeweave");
        Forge.ConstructionState = EDAConstructionState::Operational;
        Forge.StructuralIntegrity = 100.f;
        LinkFixtureAsset(Campaign, Forge, FGuid(24, 1, 1, 2));

        FDAStructuralDamageRecord& Damage =
            Campaign.OperationConflict.StructuralDamageRecords.Emplace_GetRef();
        Damage.WorldAssetId = Forge.WorldAssetId;
        Damage.CardDefinitionId = Forge.CardDefinitionId;
        Damage.Modules = {
            FDAStructureModuleHealthRecord(TEXT("module.coolant"), 100.f, true),
            FDAStructureModuleHealthRecord(TEXT("module.production"), 100.f, true),
            FDAStructureModuleHealthRecord(TEXT("module.structure"), 100.f, false)
        };

        FDACampaignCitizenSignal* Worker = Campaign.LiveSignals.Citizens.FindByPredicate(
            [](const FDACampaignCitizenSignal& Citizen)
            { return Citizen.CitizenId == TEXT("citizen.forgeweave.mara_kest"); });
        if (Worker == nullptr)
        {
            Worker = &Campaign.LiveSignals.Citizens.Emplace_GetRef();
            Worker->CitizenId = TEXT("citizen.forgeweave.mara_kest");
        }
        Worker->CityId = TEXT("city.ironheart");
        Worker->JobId = TEXT("job.forgeweave.grand_forge.worker");
        FDACitizenRecord* CityWorker = Campaign.CitySimulationState.Citizens.FindByPredicate(
            [](const FDACitizenRecord& Citizen)
            { return Citizen.CitizenId == TEXT("citizen.forgeweave.mara_kest"); });
        if (Campaign.CitySimulationState.bInitialized && CityWorker != nullptr)
        {
            CityWorker->CityId = Worker->CityId;
            CityWorker->JobId = Worker->JobId;
        }
        FDACampaignJobOpeningSignal& Opening = Campaign.LiveSignals.JobOpenings.Emplace_GetRef();
        Opening.JobId = Worker->JobId;
        Opening.CityId = Worker->CityId;
        Opening.FacilityWorldAssetId = Forge.WorldAssetId;
        Opening.OpenPositions = 4;
        FDACampaignJobAssignmentSignal& Assignment = Campaign.LiveSignals.JobAssignments.Emplace_GetRef();
        Assignment.CitizenId = Worker->CitizenId;
        Assignment.JobId = Worker->JobId;
        Assignment.FacilityWorldAssetId = Forge.WorldAssetId;
        if (Campaign.CitySimulationState.bInitialized)
        {
            FDAJobOpening& CityOpening = Campaign.CitySimulationState.JobOpenings.Emplace_GetRef();
            CityOpening.JobId = Opening.JobId;
            CityOpening.CityId = Opening.CityId;
            CityOpening.FacilityWorldAssetId = Opening.FacilityWorldAssetId;
            CityOpening.OpenPositions = Opening.OpenPositions;
            FDAJobAssignment& CityAssignment =
                Campaign.CitySimulationState.JobAssignments.Emplace_GetRef();
            CityAssignment.CitizenId = Assignment.CitizenId;
            CityAssignment.JobId = Assignment.JobId;
            CityAssignment.FacilityWorldAssetId = Assignment.FacilityWorldAssetId;
            CityAssignment.MatchQuality = EDAJobMatchQuality::Acceptable;
            CityAssignment.OutputMultiplier = 0.85f;
        }
        ++Campaign.LiveSignals.MutationRevision;
    }

    bool PrepareProductionEncounter(UDAWorldStateSubsystem& World, const FName RouteTag,
        const float Trust, const float Respect, const float Grievance)
    {
        FDACampaignSnapshot Campaign = World.GetPersistentCampaign();
        Campaign.HistoryTags.RemoveAll([](const FName Tag)
        {
            const FString Text = Tag.ToString();
            return Text.StartsWith(TEXT("broker_")) || Text.StartsWith(TEXT("daxton_"))
                || Tag == TEXT("workers_protected") || Tag == TEXT("grand_forge_preserved");
        });
        TArray<FGuid> RemovedGrandForgeCards;
        Campaign.WorldAssets.RemoveAll([&RemovedGrandForgeCards](const FDAWorldAssetRecord& Asset)
        {
            if (Asset.CardDefinitionId != TEXT("forgeweave.grand_forge")) return false;
            if (Asset.CardInstanceId.IsValid()) RemovedGrandForgeCards.Add(Asset.CardInstanceId);
            return true;
        });
        for (const FGuid CardId : RemovedGrandForgeCards)
        {
            Campaign.CollectionState.Instances.Remove(CardId);
        }
        Campaign.OperationConflict.StructuralDamageRecords.RemoveAll(
            [](const FDAStructuralDamageRecord& Damage)
            { return Damage.CardDefinitionId == TEXT("forgeweave.grand_forge"); });
        Campaign.LiveSignals.JobOpenings.RemoveAll([](const FDACampaignJobOpeningSignal& Opening)
        { return Opening.JobId.ToString().StartsWith(TEXT("job.forgeweave.grand_forge")); });
        Campaign.LiveSignals.JobAssignments.RemoveAll([](const FDACampaignJobAssignmentSignal& Assignment)
        { return Assignment.JobId.ToString().StartsWith(TEXT("job.forgeweave.grand_forge")); });
        Campaign.CitySimulationState.JobOpenings.RemoveAll([](const FDAJobOpening& Opening)
        { return Opening.JobId.ToString().StartsWith(TEXT("job.forgeweave.grand_forge")); });
        Campaign.CitySimulationState.JobAssignments.RemoveAll([](const FDAJobAssignment& Assignment)
        { return Assignment.JobId.ToString().StartsWith(TEXT("job.forgeweave.grand_forge")); });
        FDADiplomaticRelationship* Relationship = Campaign.WorldState.Diplomacy.FindRelationship(
            TEXT("relationship.synara.forgeweave"));
        if (Relationship == nullptr)
        {
            Relationship = &Campaign.WorldState.Diplomacy.Relationships.Emplace_GetRef();
            Relationship->RelationshipId = TEXT("relationship.synara.forgeweave");
        }
        Relationship->Trust = 0.f;
        Relationship->Respect = 0.f;
        Relationship->Fear = 0.f;
        Relationship->Dependence = 0.f;
        Relationship->Grievance = 0.f;
        Relationship->Compatibility = 0.f;
        Relationship->ReasonLedger.Reset();
        AddRelationshipReason(*Relationship, EDADiplomaticMetric::Trust, Trust,
            TEXT("reason.daxton.trust"), Campaign.WorldState.CurrentWorldTick);
        AddRelationshipReason(*Relationship, EDADiplomaticMetric::Respect, Respect,
            TEXT("reason.daxton.respect"), Campaign.WorldState.CurrentWorldTick);
        AddRelationshipReason(*Relationship, EDADiplomaticMetric::Grievance, Grievance,
            TEXT("reason.daxton.grievance"), Campaign.WorldState.CurrentWorldTick);
        Campaign.WorldState.Forgeweave.Population = FMath::Max(60, Campaign.WorldState.Forgeweave.Population);
        Campaign.WorldState.Forgeweave.ProductionReserve = 50.f;
        Campaign.WorldState.Forgeweave.ActiveIndustrialThroughput = 20.f;
        Campaign.WorldState.Forgeweave.ResourceHunger = 10.f;
        Campaign.WorldState.Forgeweave.LogisticsEfficiency = 80.f;
        Campaign.WorldState.Forgeweave.bOverdrive = false;
        AddGrandForgeAuthority(Campaign);
        AddSortedHistory(Campaign, RouteTag);
        return World.RestorePersistentCampaign(Campaign);
    }

    bool RoundTripDaxton(const FDACampaignSnapshot& Campaign, const FString& Label)
    {
        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
            TEXT("DaxtonOwnerTests"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory);
        const bool bSaved = Saves.SaveCampaign(Campaign, Label).IsSuccess();
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = Saves.LoadCampaign(Label);
        FString Error;
        const bool bValid = bSaved && Loaded.HasValue()
            && Loaded.GetValue().DaxtonState.Phase == Campaign.DaxtonState.Phase
            && Loaded.GetValue().DaxtonState.bLeaderResolved == Campaign.DaxtonState.bLeaderResolved
            && Loaded.GetValue().DaxtonState.LeaderState == Campaign.DaxtonState.LeaderState
            && Loaded.GetValue().DaxtonState.PhaseOneObjectiveActionId
                == Campaign.DaxtonState.PhaseOneObjectiveActionId
            && Loaded.GetValue().DaxtonState.CanonicalActionRecords.Num()
                == Campaign.DaxtonState.CanonicalActionRecords.Num()
            && Loaded.GetValue().Validate(Error);
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*Directory);
        return bValid;
    }

    bool SerializeJson(const TSharedRef<FJsonObject>& Object, FString& OutJson)
    {
        OutJson.Reset();
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
        return FJsonSerializer::Serialize(Object, Writer);
    }

    FString FixtureChecksum(const double Version, const TSharedRef<FJsonObject>& Campaign)
    {
        const TSharedRef<FJsonObject> Material = MakeShared<FJsonObject>();
        Material->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
        if (Version >= 19.0)
        {
            Material->SetNumberField(FDASaveJsonFields::ContentVersion,
                FDASaveSchema::CurrentContentVersion);
            Material->SetNumberField(FDASaveJsonFields::BuildVersion,
                FDASaveSchema::CurrentBuildVersion);
        }
        Material->SetObjectField(FDASaveJsonFields::Campaign, Campaign);
        FString Json;
        SerializeJson(Material, Json);
        const FTCHARToUTF8 Utf8(*Json);
        return FString::Printf(TEXT("%08X"), FCrc::MemCrc32(Utf8.Get(), Utf8.Length()));
    }

    bool RewriteChecksummedDaxtonCampaign(const FString& Directory, const FString& Slot,
        TFunctionRef<bool(FJsonObject&)> Mutate)
    {
        const FString Path = FPaths::Combine(Directory, Slot + TEXT(".dasave"));
        FString Document;
        TSharedPtr<FJsonObject> Root;
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!FFileHelper::LoadFileToString(Document, *Path)) return false;
        const TSharedRef<TJsonReader<>> LoadedReader = TJsonReaderFactory<>::Create(Document);
        if (!FJsonSerializer::Deserialize(LoadedReader, Root) || !Root.IsValid()
            || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
            || Campaign == nullptr || !Campaign->IsValid() || !Mutate(*Campaign->Get())) return false;
        Root->SetStringField(FDASaveJsonFields::Checksum,
            FixtureChecksum(Root->GetNumberField(FDASaveJsonFields::SchemaVersion), Campaign->ToSharedRef()));
        return SerializeJson(Root.ToSharedRef(), Document)
            && FFileHelper::SaveStringToFile(Document, *Path,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    FDACampaignSnapshot MakeEncounterCampaign(
        const FName RouteTag = TEXT("broker_alliance"),
        const float Trust = 80.f,
        const float Respect = 80.f,
        const float Grievance = 0.f)
    {
        FDACampaignSnapshot Campaign;
        Campaign.WorldState.bInitialized = true;
        Campaign.WorldState.CurrentWorldTick = 40;
        Campaign.WorldState.Forgeweave.bInitialized = true;
        Campaign.WorldState.Forgeweave.Population = 60;
        Campaign.WorldState.Forgeweave.ProductionReserve = 50.f;
        Campaign.WorldState.Forgeweave.ActiveIndustrialThroughput = 20.f;
        Campaign.WorldState.Forgeweave.ResourceHunger = 10.f;
        Campaign.WorldState.Forgeweave.LogisticsEfficiency = 80.f;

        FDADiplomaticRelationship& Relationship =
            Campaign.WorldState.Diplomacy.Relationships.Emplace_GetRef();
        Relationship.RelationshipId = TEXT("relationship.synara.forgeweave");
        Relationship.Trust = Trust;
        Relationship.Respect = Respect;
        Relationship.Grievance = Grievance;

        FDAWorldAssetRecord& Forge = Campaign.WorldAssets.Emplace_GetRef();
        Forge.WorldAssetId = FGuid(24, 1, 1, 1);
        Forge.CardDefinitionId = TEXT("forgeweave.grand_forge");
        Forge.CityId = TEXT("city.ironheart");
        Forge.OwnerCivilizationId = TEXT("civilization.forgeweave");
        Forge.ConstructionState = EDAConstructionState::Operational;
        Forge.StructuralIntegrity = 100.f;

        FDAStructuralDamageRecord& Damage =
            Campaign.OperationConflict.StructuralDamageRecords.Emplace_GetRef();
        Damage.WorldAssetId = Forge.WorldAssetId;
        Damage.CardDefinitionId = Forge.CardDefinitionId;
        Damage.Modules = {
            FDAStructureModuleHealthRecord(TEXT("module.coolant"), 100.f, true),
            FDAStructureModuleHealthRecord(TEXT("module.production"), 100.f, true),
            FDAStructureModuleHealthRecord(TEXT("module.structure"), 100.f, false)
        };

        FDACampaignCitizenSignal& Worker = Campaign.LiveSignals.Citizens.Emplace_GetRef();
        Worker.CitizenId = TEXT("citizen.forgeweave.mara_kest");
        Worker.CityId = TEXT("city.ironheart");
        Worker.JobId = TEXT("job.forgeweave.grand_forge.worker");
        AddSortedHistory(Campaign, RouteTag);
        return Campaign;
    }

    void ReachPhaseThree(FDACampaignSnapshot& Campaign)
    {
        FString Error;
        FDADaxtonEncounter::Start(FGuid(24, 2, 1, 1), Campaign, Error);
        FDADaxtonEncounter::ApplySystemInteraction(
            FGuid(24, 2, 1, 2), EDADaxtonInteraction::Damage, 40.f, Campaign, Error);
        FDADaxtonEncounter::ApplySystemInteraction(
            FGuid(24, 2, 1, 3), EDADaxtonInteraction::DisableCoolant, 1.f, Campaign, Error);
        FDADaxtonEncounter::ApplySystemInteraction(
            FGuid(24, 2, 1, 4), EDADaxtonInteraction::RedirectSupply, 10.f, Campaign, Error);
        FDADaxtonEncounter::ApplySystemInteraction(
            FGuid(24, 2, 1, 5), EDADaxtonInteraction::HackProduction, 10.f, Campaign, Error);
        FDADaxtonEncounter::ApplySystemInteraction(
            FGuid(24, 2, 1, 6), EDADaxtonInteraction::WorkerShutdown, 1.f, Campaign, Error);
        FDADaxtonEncounter::ApplySystemInteraction(
            FGuid(24, 2, 1, 7), EDADaxtonInteraction::Damage, 60.f, Campaign, Error);
        FDADaxtonEncounter::EnterPhaseThree(FGuid(24, 2, 1, 8), Campaign, Error);
    }

    bool BuildResolvedAlliance(UDAWorldStateSubsystem& World)
    {
        FString Error;
        return PrepareProductionEncounter(World, TEXT("broker_alliance"), 80.f, 80.f, 0.f)
            && World.StartDaxtonEncounter(FGuid(24, 30, 1, 1), Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 30, 1, 2), EDADaxtonInteraction::Damage, 40.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 30, 1, 3), EDADaxtonInteraction::DisableCoolant, 1.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 30, 1, 4), EDADaxtonInteraction::RedirectSupply, 10.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 30, 1, 5), EDADaxtonInteraction::HackProduction, 10.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 30, 1, 6), EDADaxtonInteraction::WorkerShutdown, 1.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 30, 1, 7), EDADaxtonInteraction::Damage, 60.f, Error)
            && World.EnterDaxtonChoicePhase(FGuid(24, 30, 1, 8), Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 30, 1, 9),
                EDADaxtonChoiceObjective::DefeatDaxton, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 30, 1, 10),
                EDADaxtonChoiceObjective::SaveGrandForge, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 30, 1, 11),
                EDADaxtonChoiceObjective::EvacuateWorkers, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 30, 1, 12),
                EDADaxtonChoiceObjective::StabilizeProductionOfferUnion, Error)
            && World.ResolveDaxtonLeaderState(FGuid(24, 30, 1, 13),
                EDADaxtonLeaderState::AlliedForgeLord, Error);
    }

    bool BuildResolvedAllianceWithPhaseOneSystems(UDAWorldStateSubsystem& World)
    {
        FString Error;
        return PrepareProductionEncounter(World, TEXT("broker_alliance"), 80.f, 80.f, 0.f)
            && World.StartDaxtonEncounter(FGuid(24, 31, 1, 1), Error)
            && World.AdvanceDaxtonGrandForgeProduction(FGuid(24, 31, 1, 2), Error)
            && World.DeployDaxtonHardenedCover(FGuid(24, 31, 1, 3), Error)
            && World.ReinforceDaxtonForgeGuard(FGuid(24, 31, 1, 4), Error)
            && World.CompleteDaxtonPhaseOneIndustrialObjective(FGuid(24, 31, 1, 5), Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 31, 1, 6),
                EDADaxtonInteraction::DisableCoolant, 1.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 31, 1, 7),
                EDADaxtonInteraction::RedirectSupply, 10.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 31, 1, 8),
                EDADaxtonInteraction::HackProduction, 10.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 31, 1, 9),
                EDADaxtonInteraction::WorkerShutdown, 1.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 31, 1, 10),
                EDADaxtonInteraction::Damage, 100.f, Error)
            && World.EnterDaxtonChoicePhase(FGuid(24, 31, 1, 11), Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 31, 1, 12),
                EDADaxtonChoiceObjective::DefeatDaxton, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 31, 1, 13),
                EDADaxtonChoiceObjective::SaveGrandForge, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 31, 1, 14),
                EDADaxtonChoiceObjective::EvacuateWorkers, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 31, 1, 15),
                EDADaxtonChoiceObjective::StabilizeProductionOfferUnion, Error)
            && World.ResolveDaxtonLeaderState(FGuid(24, 31, 1, 16),
                EDADaxtonLeaderState::AlliedForgeLord, Error);
    }

    bool ApplyUnrelatedPostResolutionEvolution(UDAWorldStateSubsystem& World)
    {
        FDACampaignCitizenSignal Citizen;
        Citizen.CitizenId = TEXT("citizen.post_daxton.archivist");
        Citizen.CityId = TEXT("city.ironheart");
        Citizen.JobId = TEXT("job.post_daxton.archive");
        if (!World.SubmitCitizenSignal(Citizen)) return false;

        FDACampaignJobOpeningSignal Opening;
        Opening.JobId = Citizen.JobId;
        Opening.CityId = Citizen.CityId;
        Opening.FacilityWorldAssetId = FGuid(24, 1, 1, 1);
        Opening.OpenPositions = 2;
        if (!World.SubmitJobOpeningSignal(Opening)) return false;

        FDACampaignJobAssignmentSignal Assignment;
        Assignment.CitizenId = Citizen.CitizenId;
        Assignment.JobId = Citizen.JobId;
        Assignment.FacilityWorldAssetId = Opening.FacilityWorldAssetId;
        if (!World.SubmitJobAssignmentSignal(Assignment)) return false;

        if (!World.AdvanceWorldTicks(1)) return false;
        FDACampaignSnapshot Candidate = World.GetPersistentCampaign();
        AddSortedHistory(Candidate, TEXT("post_daxton_civic_festival"));
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>(GetTransientPackage());
        if (Diplomacy == nullptr || !Diplomacy->ApplyReason(Candidate,
            TEXT("relationship.synara.forgeweave"), EDADiplomaticMetric::Trust,
            TEXT("post_daxton.policy_dispute"), -30.f,
            Candidate.WorldState.CurrentWorldTick, TEXT("reason.post_daxton.policy_dispute"))) return false;
        const FDACampaignSnapshot& Authority = World.GetPersistentCampaign();
        return World.TryCommitPersistentCampaign(Candidate,
            Authority.NarrativeState.MutationRevision, Authority.LiveSignals.MutationRevision,
            Authority.WorldState.CurrentWorldTick);
    }

    TSharedPtr<FJsonObject> ObjectField(FJsonObject& Object, const TCHAR* Field)
    {
        const TSharedPtr<FJsonObject>* Value = nullptr;
        return Object.TryGetObjectField(Field, Value) && Value != nullptr ? *Value : nullptr;
    }

    bool TamperGrandForgeModule(FJsonObject& Campaign, const TCHAR* ModuleId)
    {
        const TSharedPtr<FJsonObject> Conflict = ObjectField(Campaign, TEXT("operationConflict"));
        const TArray<TSharedPtr<FJsonValue>>* DamageRecords = nullptr;
        if (!Conflict.IsValid() || !Conflict->TryGetArrayField(TEXT("structuralDamageRecords"), DamageRecords)
            || DamageRecords == nullptr) return false;
        for (const TSharedPtr<FJsonValue>& DamageValue : *DamageRecords)
        {
            const TSharedPtr<FJsonObject> Damage = DamageValue.IsValid() ? DamageValue->AsObject() : nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
            if (!Damage.IsValid() || Damage->GetStringField(TEXT("cardDefinitionId")) != TEXT("forgeweave.grand_forge")
                || !Damage->TryGetArrayField(TEXT("modules"), Modules) || Modules == nullptr) continue;
            for (const TSharedPtr<FJsonValue>& ModuleValue : *Modules)
            {
                const TSharedPtr<FJsonObject> Module = ModuleValue.IsValid() ? ModuleValue->AsObject() : nullptr;
                if (Module.IsValid() && Module->GetStringField(TEXT("moduleId")) == ModuleId)
                {
                    Module->SetNumberField(TEXT("currentHealth"), 17.0);
                    Module->SetStringField(TEXT("state"), TEXT("Disabled"));
                    return true;
                }
            }
        }
        return false;
    }

    bool TamperWorker(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject> Signals = ObjectField(Campaign, TEXT("liveSignals"));
        const TArray<TSharedPtr<FJsonValue>>* Citizens = nullptr;
        if (!Signals.IsValid() || !Signals->TryGetArrayField(TEXT("citizens"), Citizens)
            || Citizens == nullptr) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Citizens)
        {
            const TSharedPtr<FJsonObject> Citizen = Value.IsValid() ? Value->AsObject() : nullptr;
            if (Citizen.IsValid() && Citizen->GetStringField(TEXT("citizenId"))
                == TEXT("citizen.forgeweave.mara_kest"))
            {
                Citizen->SetStringField(TEXT("jobId"), TEXT("job.forgeweave.grand_forge.worker"));
                return true;
            }
        }
        return false;
    }

    bool TamperReinforcementOpening(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject> Signals = ObjectField(Campaign, TEXT("liveSignals"));
        const TArray<TSharedPtr<FJsonValue>>* Openings = nullptr;
        if (!Signals.IsValid() || !Signals->TryGetArrayField(TEXT("jobOpenings"), Openings)
            || Openings == nullptr) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Openings)
        {
            const TSharedPtr<FJsonObject> Opening = Value.IsValid() ? Value->AsObject() : nullptr;
            if (Opening.IsValid() && Opening->GetStringField(TEXT("jobId"))
                == TEXT("job.forgeweave.forge_guard.reinforcement"))
            {
                Opening->SetNumberField(TEXT("openPositions"), 5.0);
                return true;
            }
        }
        return false;
    }

    bool TamperLiveSignalBaseline(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject> Signals = ObjectField(Campaign, TEXT("liveSignals"));
        if (!Signals.IsValid()) return false;
        Signals->SetNumberField(TEXT("mutationRevision"), 0.0);
        return true;
    }

    bool RestoreRemovedWorkerAssignment(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject> Signals = ObjectField(Campaign, TEXT("liveSignals"));
        const TArray<TSharedPtr<FJsonValue>>* Assignments = nullptr;
        if (!Signals.IsValid() || !Signals->TryGetArrayField(TEXT("jobAssignments"), Assignments)
            || Assignments == nullptr) return false;
        TArray<TSharedPtr<FJsonValue>> Rewritten = *Assignments;
        const TSharedRef<FJsonObject> Assignment = MakeShared<FJsonObject>();
        Assignment->SetStringField(TEXT("citizenId"), TEXT("citizen.forgeweave.mara_kest"));
        Assignment->SetStringField(TEXT("jobId"), TEXT("job.forgeweave.grand_forge.worker"));
        Assignment->SetStringField(TEXT("facilityWorldAssetId"),
            FGuid(24, 1, 1, 1).ToString(EGuidFormats::Digits));
        Rewritten.Add(MakeShared<FJsonValueObject>(Assignment));
        Signals->SetArrayField(TEXT("jobAssignments"), Rewritten);
        return true;
    }

    bool TamperLeaderStateField(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject> Daxton = ObjectField(Campaign, TEXT("daxtonState"));
        if (!Daxton.IsValid()) return false;
        Daxton->SetStringField(TEXT("leaderState"), TEXT("Governor"));
        return true;
    }

    void SetProjectionRelationship(FJsonObject& Projection)
    {
        Projection.SetNumberField(TEXT("trust"), 90.0);
        Projection.SetNumberField(TEXT("respect"), 90.0);
        Projection.SetNumberField(TEXT("grievance"), 0.0);
    }

    bool TamperEmbeddedTerminalRelationshipProof(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject> Daxton = ObjectField(Campaign, TEXT("daxtonState"));
        if (!Daxton.IsValid()) return false;
        const TSharedPtr<FJsonObject> Initial = ObjectField(*Daxton, TEXT("initialCanonicalProjection"));
        const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
        if (!Initial.IsValid() || !Daxton->TryGetArrayField(TEXT("canonicalActionRecords"), Records)
            || Records == nullptr) return false;
        SetProjectionRelationship(*Initial);
        for (const TSharedPtr<FJsonValue>& Value : *Records)
        {
            const TSharedPtr<FJsonObject> Record = Value.IsValid() ? Value->AsObject() : nullptr;
            const TSharedPtr<FJsonObject> Before = Record.IsValid()
                ? ObjectField(*Record, TEXT("before")) : nullptr;
            const TSharedPtr<FJsonObject> After = Record.IsValid()
                ? ObjectField(*Record, TEXT("after")) : nullptr;
            if (!Before.IsValid() || !After.IsValid()) return false;
            SetProjectionRelationship(*Before);
            SetProjectionRelationship(*After);
        }
        return true;
    }

    bool TamperResolutionRelationshipPrefix(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject> Daxton = ObjectField(Campaign, TEXT("daxtonState"));
        const TArray<TSharedPtr<FJsonValue>>* MutationIds = nullptr;
        if (!Daxton.IsValid()
            || !Daxton->TryGetArrayField(
                TEXT("resolutionRelationshipReasonMutationIds"), MutationIds)
            || MutationIds == nullptr || MutationIds->IsEmpty()) return false;
        TArray<TSharedPtr<FJsonValue>> Rewritten = *MutationIds;
        Rewritten[0] = MakeShared<FJsonValueString>(
            TEXT("reason.daxton.rewritten_provenance"));
        Daxton->SetArrayField(
            TEXT("resolutionRelationshipReasonMutationIds"), Rewritten);
        return true;
    }

    bool TamperAllianceRoute(FJsonObject& Campaign)
    {
        const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
        if (!Campaign.TryGetArrayField(TEXT("historyTags"), Tags) || Tags == nullptr) return false;
        TArray<TSharedPtr<FJsonValue>> Rewritten = *Tags;
        for (int32 Index = 0; Index < Rewritten.Num(); ++Index)
        {
            FString Tag;
            if (Rewritten[Index].IsValid() && Rewritten[Index]->TryGetString(Tag)
                && Tag == TEXT("broker_alliance"))
            {
                Rewritten[Index] = MakeShared<FJsonValueString>(TEXT("broker_force"));
                Campaign.SetArrayField(TEXT("historyTags"), Rewritten);
                return true;
            }
        }
        return false;
    }

    bool ReplaceHistoryTag(FJsonObject& Campaign, const TCHAR* From, const TCHAR* To)
    {
        const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
        if (!Campaign.TryGetArrayField(TEXT("historyTags"), Tags) || Tags == nullptr) return false;
        TArray<TSharedPtr<FJsonValue>> Rewritten = *Tags;
        for (int32 Index = 0; Index < Rewritten.Num(); ++Index)
        {
            FString Tag;
            if (Rewritten[Index].IsValid() && Rewritten[Index]->TryGetString(Tag) && Tag == From)
            {
                Rewritten[Index] = MakeShared<FJsonValueString>(To);
                Campaign.SetArrayField(TEXT("historyTags"), Rewritten);
                return true;
            }
        }
        return false;
    }
}

BEGIN_DEFINE_SPEC(FDADaxtonEncounterSpec, "Dominion.Gameplay.Daxton.Encounter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDADaxtonEncounterSpec)

void FDADaxtonEncounterSpec::Define()
{
    It("starts Phase I from canonical route relationship Forge modules workers supply production and structure", [this]()
    {
        FDACampaignSnapshot Campaign = MakeEncounterCampaign();
        FString Error;
        TestTrue("Canonical encounter starts",
            FDADaxtonEncounter::Start(FGuid(24, 10, 1, 1), Campaign, Error));
        TestEqual("Phase I is exact", Campaign.DaxtonState.Phase, EDADaxtonEncounterPhase::PhaseOne);
        TestTrue("Powered armor is active", Campaign.DaxtonState.bPoweredArmorActive);
        TestEqual("Powered armor starts at full integrity", Campaign.DaxtonState.ArmorIntegrity, 100.f);

        const float ProductionBefore = Campaign.WorldState.Forgeweave.ProductionReserve;
        TestTrue("Grand Forge production loop advances",
            FDADaxtonEncounter::AdvanceGrandForgeProduction(FGuid(24, 10, 1, 2), Campaign, Error));
        TestTrue("Canonical production reserve changes",
            Campaign.WorldState.Forgeweave.ProductionReserve > ProductionBefore);
        TestTrue("Hardened cover deploys",
            FDADaxtonEncounter::DeployHardenedCover(FGuid(24, 10, 1, 3), Campaign, Error));
        TestTrue("Forge Guard reinforces",
            FDADaxtonEncounter::ReinforceForgeGuard(FGuid(24, 10, 1, 4), Campaign, Error));
        TestEqual("One production cycle is persisted", Campaign.DaxtonState.GrandForgeProductionCycles, 1);
        TestEqual("One cover deployment is persisted", Campaign.DaxtonState.HardenedCoverDeployments, 1);
        TestEqual("One reinforcement is persisted", Campaign.DaxtonState.ForgeGuardReinforcements, 1);
    });

    It("enters Overdrive exactly at the 60 percent Phase I boundary and exposes all five systemic interactions", [this]()
    {
        FDACampaignSnapshot Campaign = MakeEncounterCampaign();
        FString Error;
        TestTrue("Encounter starts", FDADaxtonEncounter::Start(FGuid(24, 11, 1, 1), Campaign, Error));
        TestTrue("Damage is applied", FDADaxtonEncounter::ApplySystemInteraction(
            FGuid(24, 11, 1, 2), EDADaxtonInteraction::Damage, 40.f, Campaign, Error));
        TestEqual("Phase I completes at sixty percent", Campaign.DaxtonState.ArmorIntegrity, 60.f);
        TestEqual("Phase II begins", Campaign.DaxtonState.Phase, EDADaxtonEncounterPhase::PhaseTwo);
        TestTrue("Canonical Forgeweave production is overdriven", Campaign.WorldState.Forgeweave.bOverdrive);
        TestTrue("Heat activates", Campaign.DaxtonState.Heat > 0.f);
        TestEqual("Coolant activates", Campaign.DaxtonState.CoolantStability, 100.f);

        const EDADaxtonInteraction Interactions[] = {
            EDADaxtonInteraction::Damage,
            EDADaxtonInteraction::DisableCoolant,
            EDADaxtonInteraction::RedirectSupply,
            EDADaxtonInteraction::HackProduction,
            EDADaxtonInteraction::WorkerShutdown
        };
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Interactions); ++Index)
        {
            TestTrue("Every authored Phase II interaction mutates the same state",
                FDADaxtonEncounter::ApplySystemInteraction(
                    FGuid(24, 11, 2, Index + 1), Interactions[Index], 5.f, Campaign, Error));
        }
        for (const EDADaxtonInteraction Interaction : Interactions)
        {
            TestTrue("Every authored interaction is audited", Campaign.DaxtonState.InteractionRecords.ContainsByPredicate(
                [Interaction](const FDADaxtonInteractionRecord& Record)
                {
                    return Record.Interaction == Interaction;
                }));
        }
        TestEqual("Coolant can be disabled", Campaign.DaxtonState.CoolantStability, 0.f);
        TestTrue("Resource Hunger escalates on canonical Forgeweave state",
            Campaign.WorldState.Forgeweave.ResourceHunger > 10.f);
        TestTrue("Worker shutdown is durable campaign history",
            Campaign.HistoryTags.Contains(TEXT("workers_protected")));
    });

    It("cannot resolve at zero health and requires playable Phase III choice objectives", [this]()
    {
        FDACampaignSnapshot Campaign = MakeEncounterCampaign();
        ReachPhaseThree(Campaign);
        TestEqual("Phase III begins", Campaign.DaxtonState.Phase, EDADaxtonEncounterPhase::PhaseThree);
        TestEqual("Health can be zero without ending", Campaign.DaxtonState.ArmorIntegrity, 0.f);
        TestFalse("Zero health has not resolved the leader", Campaign.DaxtonState.bLeaderResolved);

        FString Error;
        const EDADaxtonChoiceObjective Objectives[] = {
            EDADaxtonChoiceObjective::DefeatDaxton,
            EDADaxtonChoiceObjective::SaveGrandForge,
            EDADaxtonChoiceObjective::EvacuateWorkers,
            EDADaxtonChoiceObjective::StabilizeProductionOfferUnion
        };
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Objectives); ++Index)
        {
            TestTrue("Every Phase III objective is playable", FDADaxtonEncounter::CompleteChoiceObjective(
                FGuid(24, 12, 1, Index + 1), Objectives[Index], Campaign, Error));
        }
        TestEqual("Four distinct objectives are persisted", Campaign.DaxtonState.CompletedObjectives.Num(), 4);
        TestTrue("Alliance outcome resolves after systemic objectives", FDADaxtonEncounter::ResolveLeaderState(
            FGuid(24, 12, 2, 1), EDADaxtonLeaderState::AlliedForgeLord, Campaign, Error));
        TestEqual("Leader state is persistent", Campaign.DaxtonState.LeaderState,
            EDADaxtonLeaderState::AlliedForgeLord);
        TestTrue("Encounter resolves", Campaign.DaxtonState.bLeaderResolved);
    });

    It("commits the durable Phase I industrial objective through the production owner", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production world owner exists", World);
        if (World == nullptr) return;
        TestTrue("Canonical encounter authorities restore", PrepareProductionEncounter(
            *World, TEXT("broker_alliance"), 80.f, 80.f, 0.f));

        int32 PublishCount = 0;
        const FDelegateHandle Handle = World->OnWorldTickStateCommitted.AddLambda(
            [&PublishCount](const FDACommittedCampaignSnapshot&) { ++PublishCount; });
        const int64 InitialSignalRevision = World->GetLiveSignals().MutationRevision;
        FString Error;
        TestTrue("Owner starts Phase I", World->StartDaxtonEncounter(
            FGuid(24, 20, 1, 1), Error));
        TestTrue("Phase I save-loads", RoundTripDaxton(World->GetPersistentCampaign(), TEXT("phase-one")));
        TestTrue("Owner advances canonical production", World->AdvanceDaxtonGrandForgeProduction(
            FGuid(24, 20, 1, 2), Error));
        TestTrue("Owner deploys canonical hardened cover", World->DeployDaxtonHardenedCover(
            FGuid(24, 20, 1, 3), Error));
        TestTrue("Owner reinforces through canonical live jobs", World->ReinforceDaxtonForgeGuard(
            FGuid(24, 20, 1, 4), Error));
        TestTrue("Owner completes the non-damage Phase I objective",
            World->CompleteDaxtonPhaseOneIndustrialObjective(FGuid(24, 20, 1, 5), Error));

        const FDACampaignSnapshot& Committed = World->GetPersistentCampaign();
        TestEqual("Alternative objective enters Phase II", Committed.DaxtonState.Phase,
            EDADaxtonEncounterPhase::PhaseTwo);
        TestEqual("Alternative objective preserves full armor", Committed.DaxtonState.ArmorIntegrity, 100.f);
        TestTrue("Alternative objective is durable", Committed.DaxtonState.bPhaseOneIndustrialObjectiveCompleted);
        TestEqual("Phase I objective action is exact", Committed.DaxtonState.PhaseOneObjectiveActionId,
            FGuid(24, 20, 1, 5));
        TestEqual("One live-signal mutation is committed", Committed.LiveSignals.MutationRevision,
            InitialSignalRevision + 1);
        const FDAStructuralDamageRecord* Damage = Committed.OperationConflict.FindStructuralDamageRecord(
            FGuid(24, 1, 1, 1));
        TestTrue("Cover is a canonical Grand Forge module", Damage != nullptr
            && Damage->Modules.ContainsByPredicate([](const FDAStructureModuleHealthRecord& Module)
                { return Module.ModuleId == TEXT("module.hardened_cover"); }));
        TestTrue("Reinforcement is a canonical job opening", Committed.LiveSignals.JobOpenings.ContainsByPredicate(
            [](const FDACampaignJobOpeningSignal& Opening)
            { return Opening.JobId == TEXT("job.forgeweave.forge_guard.reinforcement")
                && Opening.OpenPositions == 4; }));
        FString ValidationError;
        TestTrue("Every committed mutation validates", Committed.Validate(ValidationError));
        TestTrue("Alternative Phase II save-loads", RoundTripDaxton(Committed, TEXT("phase-two-objective")));
        TestEqual("Each successful call publishes once", PublishCount, 5);
        World->OnWorldTickStateCommitted.Remove(Handle);
    });

    It("rejects duplicate and failed production actions atomically", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production world owner exists", World);
        if (World == nullptr) return;
        TestTrue("Canonical encounter authorities restore", PrepareProductionEncounter(
            *World, TEXT("broker_alliance"), 80.f, 80.f, 0.f));
        FDACampaignSnapshot Exhausted = World->GetPersistentCampaign();
        Exhausted.LiveSignals.MutationRevision = MAX_int64;
        TestTrue("Exhausted signal revision fixture restores before encounter start",
            World->RestorePersistentCampaign(Exhausted));
        int32 PublishCount = 0;
        const FDelegateHandle Handle = World->OnWorldTickStateCommitted.AddLambda(
            [&PublishCount](const FDACommittedCampaignSnapshot&) { ++PublishCount; });
        FString Error;
        TestFalse("Action before encounter start is rejected", World->DeployDaxtonHardenedCover(
            FGuid(24, 21, 1, 1), Error));
        TestEqual("Rejected action does not publish", PublishCount, 0);
        TestTrue("Start commits", World->StartDaxtonEncounter(FGuid(24, 21, 1, 2), Error));
        const FDACampaignSnapshot Started = World->GetPersistentCampaign();
        TestFalse("Duplicate start is rejected", World->StartDaxtonEncounter(
            FGuid(24, 21, 1, 2), Error));
        TestEqual("Duplicate leaves durable record count unchanged",
            World->GetPersistentCampaign().DaxtonState.CanonicalActionRecords.Num(),
            Started.DaxtonState.CanonicalActionRecords.Num());
        TestEqual("Only start publishes", PublishCount, 1);

        TestTrue("Damage enters Phase II", World->ApplyDaxtonInteraction(
            FGuid(24, 21, 1, 3), EDADaxtonInteraction::Damage, 40.f, Error));
        const FDACampaignSnapshot BeforeFailure = World->GetPersistentCampaign();
        const int32 BeforePublish = PublishCount;
        TestFalse("Worker shutdown cannot overflow the signal revision", World->ApplyDaxtonInteraction(
            FGuid(24, 21, 1, 4), EDADaxtonInteraction::WorkerShutdown, 1.f, Error));
        const FDACampaignSnapshot& AfterFailure = World->GetPersistentCampaign();
        TestEqual("Failed shutdown keeps the revision", AfterFailure.LiveSignals.MutationRevision,
            BeforeFailure.LiveSignals.MutationRevision);
        TestEqual("Failed shutdown keeps the interaction ledger",
            AfterFailure.DaxtonState.InteractionRecords.Num(),
            BeforeFailure.DaxtonState.InteractionRecords.Num());
        TestEqual("Failed shutdown keeps canonical jobs", AfterFailure.LiveSignals.JobAssignments.Num(),
            BeforeFailure.LiveSignals.JobAssignments.Num());
        TestEqual("Failed shutdown does not publish", PublishCount, BeforePublish);
        World->OnWorldTickStateCommitted.Remove(Handle);
    });

    It("reaches four route-aware Leader outcomes end to end through the production owner", [this]()
    {
        struct FCase
        {
            const TCHAR* RouteTag;
            float Trust;
            float Respect;
            float Grievance;
            EDADaxtonLeaderState State;
            bool bSaveForge;
            bool bEvacuate;
            bool bUnion;
        };
        const FCase Cases[] = {
            {TEXT("broker_influence"), 70.f, 60.f, 10.f, EDADaxtonLeaderState::Governor, false, true, true},
            {TEXT("broker_economic"), 55.f, 70.f, 20.f, EDADaxtonLeaderState::IndustrialAdvisor, true, false, true},
            {TEXT("broker_alliance"), 80.f, 80.f, 0.f, EDADaxtonLeaderState::AlliedForgeLord, true, true, true},
            {TEXT("broker_force"), 20.f, 60.f, 40.f, EDADaxtonLeaderState::Prisoner, false, true, false}
        };
        for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(Cases); ++CaseIndex)
        {
            const FCase& Case = Cases[CaseIndex];
            FDAGameInstanceSubsystemFixture Fixture;
            UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
            TestNotNull("Production world owner exists", World);
            if (World == nullptr) continue;
            TestTrue("Route authorities restore", PrepareProductionEncounter(
                *World, FName(Case.RouteTag), Case.Trust, Case.Respect, Case.Grievance));
            int32 PublishCount = 0;
            const FDelegateHandle Handle = World->OnWorldTickStateCommitted.AddLambda(
                [&PublishCount](const FDACommittedCampaignSnapshot&) { ++PublishCount; });
            FString Error;
            int32 Action = 1;
            const auto Next = [&Action, CaseIndex]() { return FGuid(24, 22 + CaseIndex, 1, Action++); };
            TestTrue("Owner starts encounter", World->StartDaxtonEncounter(Next(), Error));
            TestTrue("Phase I save-loads", RoundTripDaxton(World->GetPersistentCampaign(), TEXT("phase-one")));
            TestTrue("Owner crosses 60 percent armor boundary", World->ApplyDaxtonInteraction(
                Next(), EDADaxtonInteraction::Damage, 40.f, Error));
            TestTrue("Phase II save-loads", RoundTripDaxton(World->GetPersistentCampaign(), TEXT("phase-two")));
            TestTrue("Owner disables coolant", World->ApplyDaxtonInteraction(
                Next(), EDADaxtonInteraction::DisableCoolant, 1.f, Error));
            TestTrue("Owner redirects supply", World->ApplyDaxtonInteraction(
                Next(), EDADaxtonInteraction::RedirectSupply, 10.f, Error));
            TestTrue("Owner hacks production", World->ApplyDaxtonInteraction(
                Next(), EDADaxtonInteraction::HackProduction, 10.f, Error));
            const int64 WorkerRevisionBefore = World->GetLiveSignals().MutationRevision;
            TestTrue("Owner protects workers", World->ApplyDaxtonInteraction(
                Next(), EDADaxtonInteraction::WorkerShutdown, 1.f, Error));
            TestEqual("Worker shutdown increments live signals exactly once",
                World->GetLiveSignals().MutationRevision, WorkerRevisionBefore + 1);
            TestTrue("Owner reduces armor to zero", World->ApplyDaxtonInteraction(
                Next(), EDADaxtonInteraction::Damage, 60.f, Error));
            TestTrue("Owner enters choice phase", World->EnterDaxtonChoicePhase(Next(), Error));
            TestTrue("Phase III save-loads", RoundTripDaxton(World->GetPersistentCampaign(), TEXT("phase-three")));
            TestTrue("Defeat objective commits", World->CompleteDaxtonChoiceObjective(
                Next(), EDADaxtonChoiceObjective::DefeatDaxton, Error));
            if (Case.bSaveForge) TestTrue("Save Forge objective commits", World->CompleteDaxtonChoiceObjective(
                Next(), EDADaxtonChoiceObjective::SaveGrandForge, Error));
            if (Case.bEvacuate) TestTrue("Evacuate objective commits", World->CompleteDaxtonChoiceObjective(
                Next(), EDADaxtonChoiceObjective::EvacuateWorkers, Error));
            if (Case.bUnion) TestTrue("Union objective commits", World->CompleteDaxtonChoiceObjective(
                Next(), EDADaxtonChoiceObjective::StabilizeProductionOfferUnion, Error));
            TestTrue("Exact route-aware outcome commits", World->ResolveDaxtonLeaderState(
                Next(), Case.State, Error));
            const FDACampaignSnapshot& Resolved = World->GetPersistentCampaign();
            TestEqual("Exact Leader state persists", Resolved.DaxtonState.LeaderState, Case.State);
            FString ValidationError;
            TestTrue("Resolved snapshot validates", Resolved.Validate(ValidationError));
            TestTrue("Resolved outcome save-loads", RoundTripDaxton(Resolved, TEXT("resolved")));
            const int32 BeforeDuplicatePublish = PublishCount;
            TestFalse("Duplicate resolution is rejected", World->ResolveDaxtonLeaderState(
                Resolved.DaxtonState.ResolutionActionId, Case.State, Error));
            TestEqual("Duplicate resolution does not publish", PublishCount, BeforeDuplicatePublish);
            World->OnWorldTickStateCommitted.Remove(Handle);
        }
    });

    It("allows unrelated canonical campaign evolution after resolved Daxton proof", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production world owner exists", World);
        if (World == nullptr) return;
        TestTrue("Alliance resolves through production owner", BuildResolvedAlliance(*World));
        TestEqual("Resolution anchors the exact canonical relationship prefix",
            World->GetPersistentCampaign().DaxtonState.ResolutionRelationshipReasonCount, 2);
        TestEqual("Resolution persists both canonical reason mutation ids",
            World->GetPersistentCampaign().DaxtonState.ResolutionRelationshipReasonMutationIds.Num(), 2);
        if (World->GetPersistentCampaign().DaxtonState
            .ResolutionRelationshipReasonMutationIds.Num() == 2)
        {
            TestEqual("Trust reason is the first protected mutation",
                World->GetPersistentCampaign().DaxtonState
                    .ResolutionRelationshipReasonMutationIds[0],
                FName(TEXT("reason.daxton.trust")));
            TestEqual("Respect reason is the second protected mutation",
                World->GetPersistentCampaign().DaxtonState
                    .ResolutionRelationshipReasonMutationIds[1],
                FName(TEXT("reason.daxton.respect")));
        }

        FDACampaignCitizenSignal Citizen;
        Citizen.CitizenId = TEXT("citizen.post_daxton.archivist");
        Citizen.CityId = TEXT("city.ironheart");
        Citizen.JobId = TEXT("job.post_daxton.archive");
        TestTrue("Unrelated citizen signal commits after resolution", World->SubmitCitizenSignal(Citizen));
        FDACampaignJobOpeningSignal Opening;
        Opening.JobId = Citizen.JobId;
        Opening.CityId = Citizen.CityId;
        Opening.FacilityWorldAssetId = FGuid(24, 1, 1, 1);
        Opening.OpenPositions = 2;
        TestTrue("Unrelated job opening commits after resolution", World->SubmitJobOpeningSignal(Opening));
        FDACampaignJobAssignmentSignal Assignment;
        Assignment.CitizenId = Citizen.CitizenId;
        Assignment.JobId = Citizen.JobId;
        Assignment.FacilityWorldAssetId = Opening.FacilityWorldAssetId;
        TestTrue("Unrelated job assignment commits after resolution",
            World->SubmitJobAssignmentSignal(Assignment));

        const int64 PreEvolutionTick = World->GetCurrentWorldTick();
        const float PreEvolutionReserve =
            World->GetPersistentState().Forgeweave.ProductionReserve;
        TestTrue("Canonical economy and Forgeweave evolution advances", World->AdvanceWorldTicks(1));

        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        AddSortedHistory(Candidate, TEXT("post_daxton_civic_festival"));
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>(GetTransientPackage());
        TestNotNull("Canonical diplomacy service exists", Diplomacy);
        TestTrue("Unrelated relationship reason applies through canonical diplomacy",
            Diplomacy != nullptr && Diplomacy->ApplyReason(Candidate,
                TEXT("relationship.synara.forgeweave"), EDADiplomaticMetric::Trust,
                TEXT("post_daxton.policy_dispute"), -30.f,
                Candidate.WorldState.CurrentWorldTick, TEXT("reason.post_daxton.policy_dispute")));
        const FDACampaignSnapshot& Authority = World->GetPersistentCampaign();
        TestTrue("Owner CAS commits unrelated history and relationship reason",
            World->TryCommitPersistentCampaign(Candidate,
                Authority.NarrativeState.MutationRevision, Authority.LiveSignals.MutationRevision,
                Authority.WorldState.CurrentWorldTick));

        const FDACampaignSnapshot& Evolved = World->GetPersistentCampaign();
        FString ValidationError;
        TestTrue("Evolved resolved campaign validates", Evolved.Validate(ValidationError));
        TestTrue("Evolved resolved campaign save-loads", RoundTripDaxton(
            Evolved, TEXT("resolved-post-evolution")));
        TestTrue("Unrelated citizen remains assigned", Evolved.LiveSignals.JobAssignments.ContainsByPredicate(
            [&Citizen](const FDACampaignJobAssignmentSignal& Row)
            { return Row.CitizenId == Citizen.CitizenId && Row.JobId == Citizen.JobId; }));
        TestTrue("Unrelated history persists",
            Evolved.HistoryTags.Contains(TEXT("post_daxton_civic_festival")));
        TestEqual("World economy advances beyond encounter terminal tick",
            Evolved.WorldState.CurrentWorldTick, PreEvolutionTick + 1);
        TestEqual("Forgeweave evolution consumes the new World Tick",
            Evolved.WorldState.Forgeweave.LastProcessedWorldTick,
            Evolved.WorldState.CurrentWorldTick);
        TestTrue("Forgeweave reserve evolves beyond terminal encounter projection",
            !FMath::IsNearlyEqual(Evolved.WorldState.Forgeweave.ProductionReserve,
                PreEvolutionReserve));
        const FDADiplomaticRelationship* EvolvedRelationship =
            Evolved.WorldState.Diplomacy.FindRelationship(
                TEXT("relationship.synara.forgeweave"));
        TestNotNull("Evolved relationship remains canonical", EvolvedRelationship);
        if (EvolvedRelationship != nullptr)
        {
            TestEqual("Relationship reason evolves beyond terminal proof",
                EvolvedRelationship->Trust, 50.f);
            TestEqual("Later canonical reason appends beyond the protected prefix",
                EvolvedRelationship->ReasonLedger.Num(), 3);
        }
        TestEqual("Post-resolution evolution cannot move the protected reason boundary",
            Evolved.DaxtonState.ResolutionRelationshipReasonCount, 2);
    });

    It("rejects encounter-owned tampering after unrelated resolved evolution", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production world owner exists", World);
        if (World == nullptr) return;
        TestTrue("Resolved Alliance fixture deploys cover and reinforcement through production entry points",
            BuildResolvedAllianceWithPhaseOneSystems(*World));
        TestTrue("Unrelated post-resolution evolution commits", ApplyUnrelatedPostResolutionEvolution(*World));
        const FDACampaignSnapshot Resolved = World->GetPersistentCampaign();
        FString ValidationError;
        TestTrue("Resolved source is canonical", Resolved.Validate(ValidationError));

        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
            TEXT("DaxtonTamperTests"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory);
        struct FTamperCase
        {
            const TCHAR* Slot;
            bool (*Mutate)(FJsonObject&);
        };
        const FTamperCase Cases[] = {
            {TEXT("workers"), &TamperWorker},
            {TEXT("worker-assignment"), &RestoreRemovedWorkerAssignment},
            {TEXT("production-module"), [](FJsonObject& Campaign)
                { return TamperGrandForgeModule(Campaign, TEXT("module.production")); }},
            {TEXT("coolant-module"), [](FJsonObject& Campaign)
                { return TamperGrandForgeModule(Campaign, TEXT("module.coolant")); }},
            {TEXT("cover-module"), [](FJsonObject& Campaign)
                { return TamperGrandForgeModule(Campaign, TEXT("module.hardened_cover")); }},
            {TEXT("reinforcement-opening"), &TamperReinforcementOpening},
            {TEXT("live-signal-baseline"), &TamperLiveSignalBaseline},
            {TEXT("worker-proof-history"), [](FJsonObject& Campaign)
                { return ReplaceHistoryTag(Campaign, TEXT("workers_protected"),
                    TEXT("workers_unprotected")); }},
            {TEXT("union-route"), &TamperAllianceRoute},
            {TEXT("leader-state-field"), &TamperLeaderStateField},
            {TEXT("terminal-relationship-proof"), &TamperEmbeddedTerminalRelationshipProof},
            {TEXT("terminal-relationship-prefix"), &TamperResolutionRelationshipPrefix},
            {TEXT("leader-outcome"), [](FJsonObject& Campaign)
                { return ReplaceHistoryTag(Campaign, TEXT("daxton_allied_forge_lord"),
                    TEXT("daxton_governor")); }}
        };
        for (const FTamperCase& Case : Cases)
        {
            TestTrue("Resolved authority saves", Saves.SaveCampaign(Resolved, Case.Slot).IsSuccess());
            TestTrue("Canonical authority is tampered behind a refreshed checksum",
                RewriteChecksummedDaxtonCampaign(Directory, Case.Slot, Case.Mutate));
            const TResult<FDACampaignSnapshot, FDASaveError> Loaded = Saves.LoadCampaign(Case.Slot);
            TestFalse("Replayed canonical proof rejects tampering", Loaded.HasValue());
            if (!Loaded.HasValue()) TestEqual("Recomputed-checksum tamper is invalid current data",
                Loaded.GetError().Code, EDASaveErrorCode::InvalidDocument);
        }
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*Directory);
    });
}
