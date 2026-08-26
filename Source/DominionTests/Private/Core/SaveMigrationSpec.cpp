#include "Boss/Daxton/DADaxtonEncounter.h"
#include "Conquest/DAConquestSystem.h"
#include "Diplomacy/DADiplomacySystem.h"
#include "Save/DASaveSchema.h"
#include "Save/DASaveJsonFields.h"
#include "Save/DASaveService.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Narrative/DAQuestRuntime.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    void AddDaxtonMigrationHistory(FDACampaignSnapshot& Campaign, const FName Tag)
    {
        Campaign.HistoryTags.AddUnique(Tag);
        Campaign.HistoryTags.Sort([](const FName Left, const FName Right)
        {
            return Left.LexicalLess(Right);
        });
    }

    void AddDaxtonMigrationReason(FDADiplomaticRelationship& Relationship,
        const EDADiplomaticMetric Metric, const float Magnitude, const FName MutationId,
        const int64 WorldTick)
    {
        FDADiplomaticReason& Reason = Relationship.ReasonLedger.Emplace_GetRef();
        Reason.MutationId = MutationId;
        Reason.SourceTag = TEXT("test.daxton.v16_migration");
        Reason.Metric = Metric;
        Reason.Magnitude = Magnitude;
        Reason.WorldTick = WorldTick;
        float* Aggregate = Metric == EDADiplomaticMetric::Trust ? &Relationship.Trust
            : Metric == EDADiplomaticMetric::Respect ? &Relationship.Respect
            : Metric == EDADiplomaticMetric::Grievance ? &Relationship.Grievance : nullptr;
        if (Aggregate != nullptr) *Aggregate += Magnitude;
    }

    bool ResolveDaxtonMigrationAlliance(UDAWorldStateSubsystem& World)
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
        AddDaxtonMigrationReason(*Relationship, EDADiplomaticMetric::Trust, 80.f,
            TEXT("reason.daxton.v16.trust"), Campaign.WorldState.CurrentWorldTick);
        AddDaxtonMigrationReason(*Relationship, EDADiplomaticMetric::Respect, 80.f,
            TEXT("reason.daxton.v16.respect"), Campaign.WorldState.CurrentWorldTick);

        Campaign.WorldState.Forgeweave.Population = FMath::Max(
            60, Campaign.WorldState.Forgeweave.Population);
        Campaign.WorldState.Forgeweave.ProductionReserve = 50.f;
        Campaign.WorldState.Forgeweave.ActiveIndustrialThroughput = 20.f;
        Campaign.WorldState.Forgeweave.ResourceHunger = 10.f;
        Campaign.WorldState.Forgeweave.LogisticsEfficiency = 80.f;
        Campaign.WorldState.Forgeweave.bOverdrive = false;

        FDAWorldAssetRecord& Forge = Campaign.WorldAssets.Emplace_GetRef();
        Forge.WorldAssetId = FGuid(24, 40, 1, 1);
        Forge.CardDefinitionId = TEXT("forgeweave.grand_forge");
        Forge.CityId = TEXT("city.ironheart");
        Forge.OwnerCivilizationId = TEXT("civilization.forgeweave");
        Forge.ConstructionState = EDAConstructionState::Operational;
        Forge.StructuralIntegrity = 100.f;
        Forge.CardInstanceId = FGuid(24, 40, 1, 2);
        if (!Campaign.CollectionState.AddInstanceWithId(Forge.CardInstanceId,
            Forge.CardDefinitionId, EDAAcquisitionSource::Conquest,
            Campaign.WorldState.CurrentWorldTick)) return false;
        FCardInstance* ForgeCard = Campaign.CollectionState.FindInstance(Forge.CardInstanceId);
        if (ForgeCard == nullptr) return false;
        ForgeCard->WorldAssetId = Forge.WorldAssetId;
        ForgeCard->RecoveryState = EDARecoveryState::Deployed;
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
        Worker.CitizenId = TEXT("citizen.forgeweave.v16_migration_worker");
        Worker.CityId = TEXT("city.ironheart");
        Worker.JobId = TEXT("job.forgeweave.grand_forge.worker");
        FDACitizenRecord& CityWorker = Campaign.CitySimulationState.Citizens.Emplace_GetRef();
        CityWorker.CitizenId = Worker.CitizenId;
        CityWorker.CityId = Worker.CityId;
        CityWorker.JobId = Worker.JobId;
        FDACampaignJobOpeningSignal& Opening = Campaign.LiveSignals.JobOpenings.Emplace_GetRef();
        Opening.JobId = Worker.JobId;
        Opening.CityId = Worker.CityId;
        Opening.FacilityWorldAssetId = Forge.WorldAssetId;
        Opening.OpenPositions = 4;
        FDACampaignJobAssignmentSignal& Assignment =
            Campaign.LiveSignals.JobAssignments.Emplace_GetRef();
        Assignment.CitizenId = Worker.CitizenId;
        Assignment.JobId = Worker.JobId;
        Assignment.FacilityWorldAssetId = Forge.WorldAssetId;
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
        ++Campaign.LiveSignals.MutationRevision;
        AddDaxtonMigrationHistory(Campaign, TEXT("broker_alliance"));
        if (!World.RestorePersistentCampaign(Campaign)) return false;

        FString Error;
        return World.StartDaxtonEncounter(FGuid(24, 40, 2, 1), Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 40, 2, 2),
                EDADaxtonInteraction::Damage, 40.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 40, 2, 3),
                EDADaxtonInteraction::DisableCoolant, 1.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 40, 2, 4),
                EDADaxtonInteraction::RedirectSupply, 10.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 40, 2, 5),
                EDADaxtonInteraction::HackProduction, 10.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 40, 2, 6),
                EDADaxtonInteraction::WorkerShutdown, 1.f, Error)
            && World.ApplyDaxtonInteraction(FGuid(24, 40, 2, 7),
                EDADaxtonInteraction::Damage, 60.f, Error)
            && World.EnterDaxtonChoicePhase(FGuid(24, 40, 2, 8), Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 40, 2, 9),
                EDADaxtonChoiceObjective::DefeatDaxton, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 40, 2, 10),
                EDADaxtonChoiceObjective::SaveGrandForge, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 40, 2, 11),
                EDADaxtonChoiceObjective::EvacuateWorkers, Error)
            && World.CompleteDaxtonChoiceObjective(FGuid(24, 40, 2, 12),
                EDADaxtonChoiceObjective::StabilizeProductionOfferUnion, Error)
            && World.ResolveDaxtonLeaderState(FGuid(24, 40, 2, 13),
                EDADaxtonLeaderState::AlliedForgeLord, Error);
    }

    bool CommitSameTickDaxtonReason(UDAWorldStateSubsystem& World,
        const FName MutationId, const float Magnitude)
    {
        FDACampaignSnapshot Candidate = World.GetPersistentCampaign();
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>(GetTransientPackage());
        if (Diplomacy == nullptr || !Diplomacy->ApplyReason(Candidate,
            TEXT("relationship.synara.forgeweave"), EDADiplomaticMetric::Trust,
            TEXT("post_daxton.v16_migration"), Magnitude,
            Candidate.WorldState.CurrentWorldTick, MutationId)) return false;
        const FDACampaignSnapshot& Authority = World.GetPersistentCampaign();
        return World.TryCommitPersistentCampaign(Candidate,
            Authority.NarrativeState.MutationRevision, Authority.LiveSignals.MutationRevision,
            Authority.WorldState.CurrentWorldTick);
    }

    void AddValidCompletedConquestQuest(FDACampaignSnapshot& Campaign, const FName QuestId)
    {
        FDAQuestSaveState& Quest = Campaign.NarrativeState.QuestStates.Emplace_GetRef();
        Quest.QuestId = QuestId;
        Quest.DefinitionManifest.QuestId = QuestId;
        Quest.DefinitionManifest.SourceDefinitionId = FName(*(TEXT("test.definition.") + QuestId.ToString()));
        Quest.DefinitionManifest.StartNodeId = TEXT("start");
        FDAQuestNodeDefinition& Start = Quest.DefinitionManifest.Nodes.Emplace_GetRef();
        Start.NodeId = TEXT("start");
        Start.Type = EDAQuestNodeType::Start;
        Start.SourceDefinitionId = TEXT("test.node.start");
        FDAQuestEdgeDefinition& Edge = Start.Edges.Emplace_GetRef();
        Edge.BranchTag = TEXT("complete");
        Edge.TargetNodeId = TEXT("resolution");
        FDAQuestNodeDefinition& Resolution = Quest.DefinitionManifest.Nodes.Emplace_GetRef();
        Resolution.NodeId = TEXT("resolution");
        Resolution.Type = EDAQuestNodeType::Resolution;
        Resolution.SourceDefinitionId = TEXT("test.node.resolution");
        Quest.DefinitionManifest.RefreshFingerprint();
        Quest.CurrentNodeId = TEXT("resolution");
        Quest.ProgressState = EDAQuestProgressState::Completed;
        Quest.CompletedNodeIds.Add(TEXT("start"));
        FDAQuestNodeTransitionRecord& Transition = Quest.NodeTransitionRecords.Emplace_GetRef();
        Transition.CompletedNodeId = TEXT("start");
        Transition.EnteredNodeId = TEXT("resolution");
    }

    bool SerializeFixtureJson(const TSharedRef<FJsonObject>& Object, FString& OutJson)
    {
        OutJson.Reset();
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
        return FJsonSerializer::Serialize(Object, Writer);
    }

    FString CalculateFixtureChecksum(const double SchemaVersion,
        const TSharedRef<FJsonObject>& Campaign,
        const int32 ContentVersion = FDASaveSchema::CurrentContentVersion,
        const int32 BuildVersion = FDASaveSchema::CurrentBuildVersion)
    {
        const TSharedRef<FJsonObject> ChecksumMaterial = MakeShared<FJsonObject>();
        ChecksumMaterial->SetNumberField(FDASaveJsonFields::SchemaVersion, SchemaVersion);
        if (SchemaVersion >= 19.0)
        {
            ChecksumMaterial->SetNumberField(FDASaveJsonFields::ContentVersion,
                ContentVersion);
            ChecksumMaterial->SetNumberField(FDASaveJsonFields::BuildVersion,
                BuildVersion);
        }
        ChecksumMaterial->SetObjectField(FDASaveJsonFields::Campaign, Campaign);

        FString MaterialJson;
        SerializeFixtureJson(ChecksumMaterial, MaterialJson);
        const FTCHARToUTF8 Utf8Material(*MaterialJson);
        return FString::Printf(TEXT("%08X"), FCrc::MemCrc32(Utf8Material.Get(), Utf8Material.Length()));
    }

    bool RecomputeRawEnvelopeChecksum(FString& InOutDocument)
    {
        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InOutDocument);
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
            || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
            || Campaign == nullptr || !Campaign->IsValid())
        {
            return false;
        }
        const FString Checksum = CalculateFixtureChecksum(
            Root->GetNumberField(FDASaveJsonFields::SchemaVersion), Campaign->ToSharedRef());
        const FString Prefix = FString::Printf(TEXT("\"%s\":\""), FDASaveJsonFields::Checksum);
        const int32 PrefixIndex = InOutDocument.Find(Prefix, ESearchCase::CaseSensitive);
        if (PrefixIndex == INDEX_NONE)
        {
            return false;
        }
        const int32 ValueStart = PrefixIndex + Prefix.Len();
        const int32 ValueEnd = InOutDocument.Find(TEXT("\""), ESearchCase::CaseSensitive,
            ESearchDir::FromStart, ValueStart);
        if (ValueEnd == INDEX_NONE)
        {
            return false;
        }
        InOutDocument.RemoveAt(ValueStart, ValueEnd - ValueStart, EAllowShrinking::No);
        InOutDocument.InsertAt(ValueStart, Checksum);
        return true;
    }

    /** Produces an authentic schema v9 payload: v10 fields are absent, not merely empty current-schema defaults. */
    void StripV10NarrativeFields(const TSharedRef<FJsonObject>& Campaign)
    {
        Campaign->RemoveField(TEXT("synaraState"));
        Campaign->RemoveField(TEXT("liveSignals"));
        const TSharedPtr<FJsonObject>* Narrative = nullptr;
        if (!Campaign->TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative) || Narrative == nullptr) return;
        for (const TCHAR* Field : {TEXT("questContentUnlockRecords"), TEXT("questObjectiveAssetBindings"),
            TEXT("questCrisisCompletionRecords"), TEXT("questContentEffectRecords"),
            TEXT("citizenStoryStates"), TEXT("citizenStoryTransitionRecords"), TEXT("worldMapUnlockRecords"),
            TEXT("worldMapAuthorityRecords"), TEXT("auditEligibilitySourceRecords"),
            TEXT("questEligibilityProofRecords")}) (*Narrative)->RemoveField(Field);
        const TArray<TSharedPtr<FJsonValue>>* QuestStates = nullptr;
        if ((*Narrative)->TryGetArrayField(TEXT("questStates"), QuestStates) && QuestStates != nullptr)
            for (const TSharedPtr<FJsonValue>& QuestValue : *QuestStates)
                if (QuestValue.IsValid() && QuestValue->Type == EJson::Object)
                {
                    QuestValue->AsObject()->RemoveField(TEXT("nodeTransitionRecords"));
                    const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
                    if (QuestValue->AsObject()->TryGetArrayField(TEXT("worldAssetBindings"), Bindings) && Bindings != nullptr)
                        for (const TSharedPtr<FJsonValue>& Binding : *Bindings)
                            if (Binding.IsValid() && Binding->Type == EJson::Object)
                            {
                                Binding->AsObject()->RemoveField(TEXT("bindWorldTick"));
                                Binding->AsObject()->RemoveField(TEXT("questDefinitionFingerprint"));
                            }
                }
    }

    bool RewriteActiveEnvelopeVersion(
        const FString& SaveDirectory,
        const FString& Slot,
        const double SchemaVersion,
        const bool bRemoveHistoryTags,
        const TOptional<FString> OverrideWorldTickLexeme = TOptional<FString>())
    {
        const FString ActivePath = FPaths::Combine(SaveDirectory, Slot + TEXT(".dasave"));
        FString SaveDocument;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(SaveDocument, *ActivePath))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            return false;
        }

        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr || !Campaign->IsValid())
        {
            return false;
        }

        if (bRemoveHistoryTags)
        {
            (*Campaign)->RemoveField(FDASaveJsonFields::HistoryTags);
        }
        if (SchemaVersion < 3.0)
        {
            (*Campaign)->RemoveField(FDASaveJsonFields::OperationConflict);
        }
        if (SchemaVersion < 4.0)
        {
            (*Campaign)->RemoveField(FDASaveJsonFields::WorldState);
        }
        if (SchemaVersion < 5.0)
        {
            const TSharedPtr<FJsonObject>* WorldState = nullptr;
            if ((*Campaign)->TryGetObjectField(FDASaveJsonFields::WorldState, WorldState)
                && WorldState != nullptr && WorldState->IsValid())
            {
                (*WorldState)->RemoveField(FDASaveJsonFields::Forgeweave);
            }
        }
        if (SchemaVersion < 7.0)
        {
            (*Campaign)->RemoveField(FDASaveJsonFields::NarrativeState);
        }
        if (SchemaVersion == 8.0 || SchemaVersion == 9.0) StripV10NarrativeFields(Campaign->ToSharedRef());
        if (SchemaVersion == 10.0) (*Campaign)->RemoveField(TEXT("liveSignals"));
        if (SchemaVersion == 11.0)
        {
            (*Campaign)->RemoveField(TEXT("cityGridClaims"));
            (*Campaign)->RemoveField(TEXT("regionalCrisis"));
            const TSharedPtr<FJsonObject>* World = nullptr;
            const TSharedPtr<FJsonObject>* Trade = nullptr;
            if ((*Campaign)->TryGetObjectField(FDASaveJsonFields::WorldState, World)
                && World != nullptr && World->IsValid())
            {
                (*World)->RemoveField(TEXT("ecology"));
                if ((*World)->TryGetObjectField(TEXT("trade"), Trade)
                    && Trade != nullptr && Trade->IsValid())
                    (*Trade)->RemoveField(TEXT("marketPriceModifiers"));
            }
        }
        if (SchemaVersion < 14.0)
        {
            (*Campaign)->RemoveField(FDASaveJsonFields::ConquestState);
        }
        if (SchemaVersion < 16.0)
        {
            (*Campaign)->RemoveField(TEXT("daxtonState"));
        }
        else if (SchemaVersion < 17.0)
        {
            const TSharedPtr<FJsonObject>* Daxton = nullptr;
            if ((*Campaign)->TryGetObjectField(TEXT("daxtonState"), Daxton)
                && Daxton != nullptr && Daxton->IsValid())
            {
                (*Daxton)->RemoveField(TEXT("resolutionRelationshipReasonCount"));
                (*Daxton)->RemoveField(TEXT("resolutionRelationshipReasonMutationIds"));
            }
        }
        if (SchemaVersion < 18.0)
        {
            (*Campaign)->RemoveField(FDASaveJsonFields::AscensionState);
            const TFunction<void(const TSharedPtr<FJsonObject>&)> RemoveReplicationProvenance =
                [&RemoveReplicationProvenance](const TSharedPtr<FJsonObject>& Object)
                {
                    if (!Object.IsValid()) return;
                    Object->RemoveField(TEXT("sourceCardInstanceId"));
                    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
                    {
                        if (!Pair.Value.IsValid()) continue;
                        if (Pair.Value->Type == EJson::Object)
                            RemoveReplicationProvenance(Pair.Value->AsObject());
                        else if (Pair.Value->Type == EJson::Array)
                            for (const TSharedPtr<FJsonValue>& Entry : Pair.Value->AsArray())
                                if (Entry.IsValid() && Entry->Type == EJson::Object)
                                    RemoveReplicationProvenance(Entry->AsObject());
                    }
                };
            RemoveReplicationProvenance(*Campaign);
        }
        if (SchemaVersion < 19.0)
        {
            (*Campaign)->RemoveField(FDASaveJsonFields::CitySimulationState);
            (*Campaign)->RemoveField(TEXT("campaignMutationRevision"));
            const TSharedPtr<FJsonObject>* Deck = nullptr;
            if ((*Campaign)->TryGetObjectField(TEXT("deckState"), Deck)
                && Deck != nullptr && Deck->IsValid())
            {
                (*Deck)->RemoveField(TEXT("deployed"));
            }
            const TSharedPtr<FJsonObject>* World = nullptr;
            if ((*Campaign)->TryGetObjectField(FDASaveJsonFields::WorldState, World)
                && World != nullptr && World->IsValid())
            {
                const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
                if ((*World)->TryGetArrayField(TEXT("regions"), Regions)
                    && Regions != nullptr)
                {
                    for (const TSharedPtr<FJsonValue>& Region : *Regions)
                    {
                        if (Region.IsValid() && Region->AsObject().IsValid())
                            Region->AsObject()->RemoveField(TEXT("mapAssetPath"));
                    }
                }
                const TSharedPtr<FJsonObject>* Forgeweave = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
                if ((*World)->TryGetObjectField(TEXT("forgeweave"), Forgeweave)
                    && Forgeweave != nullptr && Forgeweave->IsValid()
                    && (*Forgeweave)->TryGetArrayField(TEXT("buildings"), Buildings)
                    && Buildings != nullptr)
                {
                    for (const TSharedPtr<FJsonValue>& Building : *Buildings)
                    {
                        if (!Building.IsValid() || !Building->AsObject().IsValid()) continue;
                        Building->AsObject()->RemoveField(TEXT("provenanceId"));
                        Building->AsObject()->RemoveField(TEXT("cardDefinitionId"));
                    }
                }
            }
            Root->RemoveField(FDASaveJsonFields::ContentVersion);
            Root->RemoveField(FDASaveJsonFields::BuildVersion);
        }
        Root->SetNumberField(FDASaveJsonFields::SchemaVersion, SchemaVersion);
        Root->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(SchemaVersion, Campaign->ToSharedRef()));

        if (!SerializeFixtureJson(Root.ToSharedRef(), SaveDocument))
        {
            return false;
        }
        if (OverrideWorldTickLexeme.IsSet())
        {
            const auto ReplaceNumericField = [&SaveDocument, &OverrideWorldTickLexeme](const TCHAR* FieldName)
            {
                const FString Prefix = FString::Printf(TEXT("\"%s\":"), FieldName);
                const int32 PrefixIndex = SaveDocument.Find(Prefix, ESearchCase::CaseSensitive);
                if (PrefixIndex == INDEX_NONE)
                {
                    return false;
                }
                int32 ValueStart = PrefixIndex + Prefix.Len();
                while (ValueStart < SaveDocument.Len() && FChar::IsWhitespace(SaveDocument[ValueStart]))
                {
                    ++ValueStart;
                }
                int32 ValueEnd = ValueStart;
                while (ValueEnd < SaveDocument.Len()
                    && SaveDocument[ValueEnd] != TEXT(',')
                    && SaveDocument[ValueEnd] != TEXT('}'))
                {
                    ++ValueEnd;
                }
                SaveDocument.RemoveAt(ValueStart, ValueEnd - ValueStart, EAllowShrinking::No);
                SaveDocument = SaveDocument.Left(ValueStart)
                    + OverrideWorldTickLexeme.GetValue()
                    + SaveDocument.Mid(ValueStart);
                return true;
            };
            if (!ReplaceNumericField(FDASaveJsonFields::CurrentWorldTick)
                || !ReplaceNumericField(FDASaveJsonFields::LastProcessedWorldTick))
            {
                return false;
            }

            TSharedPtr<FJsonObject> RawRoot;
            const TSharedRef<TJsonReader<>> RawReader = TJsonReaderFactory<>::Create(SaveDocument);
            if (!FJsonSerializer::Deserialize(RawReader, RawRoot) || !RawRoot.IsValid())
            {
                return false;
            }
            const TSharedPtr<FJsonObject>* RawCampaign = nullptr;
            if (!RawRoot->TryGetObjectField(FDASaveJsonFields::Campaign, RawCampaign)
                || RawCampaign == nullptr || !RawCampaign->IsValid())
            {
                return false;
            }
            RawRoot->SetStringField(
                FDASaveJsonFields::Checksum,
                CalculateFixtureChecksum(SchemaVersion, RawCampaign->ToSharedRef()));
            if (!SerializeFixtureJson(RawRoot.ToSharedRef(), SaveDocument)
                || !ReplaceNumericField(FDASaveJsonFields::CurrentWorldTick)
                || !ReplaceNumericField(FDASaveJsonFields::LastProcessedWorldTick))
            {
                return false;
            }
        }
        return FFileHelper::SaveStringToFile(SaveDocument, *ActivePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool RewriteAsLegacySchemaV7(const FString& SaveDirectory, const FString& Slot)
    {
        const FString ActivePath = FPaths::Combine(SaveDirectory, Slot + TEXT(".dasave"));
        FString SaveDocument;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(SaveDocument, *ActivePath)) return false;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        const TSharedPtr<FJsonObject>* Conflict = nullptr;
        const TSharedPtr<FJsonObject>* Narrative = nullptr;
        if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr
            || !(*Campaign)->TryGetObjectField(FDASaveJsonFields::OperationConflict, Conflict) || Conflict == nullptr
            || !(*Campaign)->TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative) || Narrative == nullptr)
        {
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>* CurrentActions = nullptr;
        TArray<TSharedPtr<FJsonValue>> AppliedIds;
        if ((*Narrative)->TryGetArrayField(FDASaveJsonFields::ActionRecords, CurrentActions) && CurrentActions != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *CurrentActions)
            {
                AppliedIds.Add(MakeShared<FJsonValueString>(Value->AsObject()->GetStringField(TEXT("actionId"))));
            }
        }
        (*Narrative)->SetArrayField(FDASaveJsonFields::AppliedActionIds, MoveTemp(AppliedIds));
        (*Narrative)->RemoveField(FDASaveJsonFields::ActionRecords);
        const TArray<TSharedPtr<FJsonValue>>* PromiseRecords = nullptr;
        if ((*Narrative)->TryGetArrayField(TEXT("promiseRecords"), PromiseRecords) && PromiseRecords != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *PromiseRecords)
            {
                Value->AsObject()->RemoveField(TEXT("resolutionActionId"));
                Value->AsObject()->RemoveField(TEXT("bLegacyResolutionWithoutAction"));
                Value->AsObject()->RemoveField(TEXT("legacyResolutionSourceSchemaVersion"));
            }
        }
        TArray<TSharedPtr<FJsonValue>> ConflictHistory;
        ConflictHistory.Add(MakeShared<FJsonValueString>(TEXT("forge_guard_surrender_accepted")));
        (*Conflict)->SetArrayField(FDASaveJsonFields::HistoryTags, MoveTemp(ConflictHistory));
        TArray<TSharedPtr<FJsonValue>> LegacyCampaignHistory;
        LegacyCampaignHistory.Add(MakeShared<FJsonValueString>(TEXT("action.legacy.fulfill")));
        (*Campaign)->SetArrayField(FDASaveJsonFields::HistoryTags, MoveTemp(LegacyCampaignHistory));
        (*Campaign)->RemoveField(FDASaveJsonFields::ConquestState);
        (*Campaign)->RemoveField(FDASaveJsonFields::DaxtonState);
        (*Campaign)->RemoveField(FDASaveJsonFields::CitySimulationState);
        Root->RemoveField(FDASaveJsonFields::ContentVersion);
        Root->RemoveField(FDASaveJsonFields::BuildVersion);
        Root->SetNumberField(FDASaveJsonFields::SchemaVersion, 7.0);
        Root->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(7.0, Campaign->ToSharedRef()));
        return SerializeFixtureJson(Root.ToSharedRef(), SaveDocument)
            && FFileHelper::SaveStringToFile(SaveDocument, *ActivePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool CampaignFieldExistsOnDisk(
        const FString& SaveDirectory,
        const FString& Slot,
        const TCHAR* FieldName,
        bool& bOutExists)
    {
        FString SaveDocument;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(
                SaveDocument,
                *FPaths::Combine(SaveDirectory, Slot + TEXT(".dasave"))))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            return false;
        }

        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
            || Campaign == nullptr || !Campaign->IsValid())
        {
            return false;
        }

        bOutExists = (*Campaign)->HasField(FieldName);
        return true;
    }

    bool TamperActionTransactionField(
        const FString& SaveDirectory,
        const FString& Slot,
        const int32 TransactionIndex,
        const TCHAR* FieldName,
        const TOptional<double> NumberValue,
        const TOptional<FString> StringValue)
    {
        const FString ActivePath = FPaths::Combine(SaveDirectory, Slot + TEXT(".dasave"));
        FString SaveDocument;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(SaveDocument, *ActivePath))
        {
            return false;
        }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            return false;
        }
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        const TSharedPtr<FJsonObject>* WorldState = nullptr;
        const TSharedPtr<FJsonObject>* Forgeweave = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Transactions = nullptr;
        if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
            || Campaign == nullptr
            || !(*Campaign)->TryGetObjectField(FDASaveJsonFields::WorldState, WorldState)
            || WorldState == nullptr
            || !(*WorldState)->TryGetObjectField(FDASaveJsonFields::Forgeweave, Forgeweave)
            || Forgeweave == nullptr
            || !(*Forgeweave)->TryGetArrayField(TEXT("actionTransactions"), Transactions)
            || Transactions == nullptr
            || !Transactions->IsValidIndex(TransactionIndex))
        {
            return false;
        }
        const TSharedPtr<FJsonObject> Transaction = (*Transactions)[TransactionIndex]->AsObject();
        if (!Transaction.IsValid())
        {
            return false;
        }
        if (NumberValue.IsSet())
        {
            Transaction->SetNumberField(FieldName, NumberValue.GetValue());
        }
        else if (StringValue.IsSet())
        {
            Transaction->SetStringField(FieldName, StringValue.GetValue());
        }
        else
        {
            return false;
        }
        Root->SetStringField(
            FDASaveJsonFields::Checksum,
            CalculateFixtureChecksum(FDASaveSchema::CurrentSchemaVersion, Campaign->ToSharedRef()));
        return SerializeFixtureJson(Root.ToSharedRef(), SaveDocument)
            && FFileHelper::SaveStringToFile(
                SaveDocument,
                *ActivePath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool RewriteChecksummedCampaign(
        const FString& SaveDirectory,
        const FString& Slot,
        TFunctionRef<bool(FJsonObject&)> MutateCampaign)
    {
        const FString ActivePath = FPaths::Combine(SaveDirectory, Slot + TEXT(".dasave"));
        FString SaveDocument;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(SaveDocument, *ActivePath))
        {
            return false;
        }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!FJsonSerializer::Deserialize(Reader, Root)
            || !Root.IsValid()
            || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
            || Campaign == nullptr
            || !MutateCampaign(*Campaign->Get()))
        {
            return false;
        }
        Root->SetStringField(
            FDASaveJsonFields::Checksum,
            CalculateFixtureChecksum(Root->GetNumberField(FDASaveJsonFields::SchemaVersion), Campaign->ToSharedRef()));
        return SerializeFixtureJson(Root.ToSharedRef(), SaveDocument)
            && FFileHelper::SaveStringToFile(SaveDocument, *ActivePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool RewriteAsEmbeddedSchemaV5(const FString& SaveDirectory, const FString& Slot)
    {
        const bool bRewritten = RewriteChecksummedCampaign(
            SaveDirectory,
            Slot,
            [](FJsonObject& Campaign)
            {
                const TSharedPtr<FJsonObject>* WorldState = nullptr;
                const TSharedPtr<FJsonObject>* Forgeweave = nullptr;
                const TSharedPtr<FJsonObject>* Conflict = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* DamageRecords = nullptr;
                if (!Campaign.TryGetObjectField(FDASaveJsonFields::WorldState, WorldState)
                    || WorldState == nullptr
                    || !(*WorldState)->TryGetObjectField(FDASaveJsonFields::Forgeweave, Forgeweave)
                    || Forgeweave == nullptr
                    || !(*Forgeweave)->TryGetArrayField(TEXT("buildings"), Buildings)
                    || !Campaign.TryGetArrayField(FDASaveJsonFields::WorldAssets, Assets)
                    || !Campaign.TryGetObjectField(FDASaveJsonFields::OperationConflict, Conflict)
                    || Conflict == nullptr
                    || !(*Conflict)->TryGetArrayField(FDASaveJsonFields::StructuralDamageRecords, DamageRecords))
                {
                    return false;
                }

                TSet<FString> EmbeddedIds;
                for (const TSharedPtr<FJsonValue>& BuildingValue : *Buildings)
                {
                    const TSharedPtr<FJsonObject> Building = BuildingValue->AsObject();
                    FString WorldAssetId;
                    if (!Building.IsValid() || !Building->TryGetStringField(TEXT("worldAssetId"), WorldAssetId))
                    {
                        return false;
                    }
                    const TSharedPtr<FJsonValue>* AssetValue = Assets->FindByPredicate(
                        [&WorldAssetId](const TSharedPtr<FJsonValue>& Candidate)
                        {
                            FString CandidateId;
                            return Candidate->AsObject().IsValid()
                                && Candidate->AsObject()->TryGetStringField(TEXT("worldAssetId"), CandidateId)
                                && CandidateId == WorldAssetId;
                        });
                    if (AssetValue == nullptr)
                    {
                        return false;
                    }
                    Building->SetObjectField(TEXT("assetRecord"), (*AssetValue)->AsObject());
                    Building->RemoveField(TEXT("worldAssetId"));
                    const TSharedPtr<FJsonValue>* DamageValue = DamageRecords->FindByPredicate(
                        [&WorldAssetId](const TSharedPtr<FJsonValue>& Candidate)
                        {
                            FString CandidateId;
                            return Candidate->AsObject().IsValid()
                                && Candidate->AsObject()->TryGetStringField(TEXT("worldAssetId"), CandidateId)
                                && CandidateId == WorldAssetId;
                        });
                    Building->SetBoolField(TEXT("bHasStructuralDamageRecord"), DamageValue != nullptr);
                    if (DamageValue != nullptr)
                    {
                        Building->SetObjectField(TEXT("structuralDamage"), (*DamageValue)->AsObject());
                    }
                    else
                    {
                        Building->SetObjectField(TEXT("structuralDamage"), MakeShared<FJsonObject>());
                    }
                    EmbeddedIds.Add(WorldAssetId);
                }
                TArray<TSharedPtr<FJsonValue>> RemainingAssets = Assets->FilterByPredicate(
                    [&EmbeddedIds](const TSharedPtr<FJsonValue>& Value)
                    {
                        FString Id;
                        return !Value->AsObject().IsValid()
                            || !Value->AsObject()->TryGetStringField(TEXT("worldAssetId"), Id)
                            || !EmbeddedIds.Contains(Id);
                    });
                TArray<TSharedPtr<FJsonValue>> RemainingDamage = DamageRecords->FilterByPredicate(
                    [&EmbeddedIds](const TSharedPtr<FJsonValue>& Value)
                    {
                        FString Id;
                        return !Value->AsObject().IsValid()
                            || !Value->AsObject()->TryGetStringField(TEXT("worldAssetId"), Id)
                            || !EmbeddedIds.Contains(Id);
                    });
                Campaign.SetArrayField(FDASaveJsonFields::WorldAssets, MoveTemp(RemainingAssets));
                (*Conflict)->SetArrayField(FDASaveJsonFields::StructuralDamageRecords, MoveTemp(RemainingDamage));
                (*Forgeweave)->RemoveField(TEXT("actionTransactions"));
                return !EmbeddedIds.IsEmpty();
            });
        if (!bRewritten)
        {
            return false;
        }

        const FString ActivePath = FPaths::Combine(SaveDirectory, Slot + TEXT(".dasave"));
        FString SaveDocument;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(SaveDocument, *ActivePath))
        {
            return false;
        }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!FJsonSerializer::Deserialize(Reader, Root)
            || !Root.IsValid()
            || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
            || Campaign == nullptr)
        {
            return false;
        }
        Root->SetNumberField(FDASaveJsonFields::SchemaVersion, 5.0);
        (*Campaign)->RemoveField(FDASaveJsonFields::ConquestState);
        (*Campaign)->RemoveField(FDASaveJsonFields::DaxtonState);
        (*Campaign)->RemoveField(FDASaveJsonFields::CitySimulationState);
        Root->RemoveField(FDASaveJsonFields::ContentVersion);
        Root->RemoveField(FDASaveJsonFields::BuildVersion);
        Root->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(5.0, Campaign->ToSharedRef()));
        return SerializeFixtureJson(Root.ToSharedRef(), SaveDocument)
            && FFileHelper::SaveStringToFile(SaveDocument, *ActivePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    TSharedPtr<FJsonObject> GetForgeweaveJson(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject>* World = nullptr;
        const TSharedPtr<FJsonObject>* Forgeweave = nullptr;
        return Campaign.TryGetObjectField(FDASaveJsonFields::WorldState, World)
            && World != nullptr
            && (*World)->TryGetObjectField(FDASaveJsonFields::Forgeweave, Forgeweave)
            && Forgeweave != nullptr
            ? *Forgeweave
            : nullptr;
    }

    bool RemoveLastActionTransactionJson(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject> Forgeweave = GetForgeweaveJson(Campaign);
        const TArray<TSharedPtr<FJsonValue>>* Transactions = nullptr;
        if (!Forgeweave.IsValid()
            || !Forgeweave->TryGetArrayField(TEXT("actionTransactions"), Transactions)
            || Transactions == nullptr
            || Transactions->IsEmpty())
        {
            return false;
        }
        TArray<TSharedPtr<FJsonValue>> Mutated = *Transactions;
        Mutated.Pop();
        Forgeweave->SetArrayField(TEXT("actionTransactions"), MoveTemp(Mutated));
        return true;
    }

    bool RemoveFirstConstructionDecisionJson(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject> Forgeweave = GetForgeweaveJson(Campaign);
        const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
        if (!Forgeweave.IsValid()
            || !Forgeweave->TryGetArrayField(TEXT("decisionHistory"), Decisions)
            || Decisions == nullptr)
        {
            return false;
        }
        TArray<TSharedPtr<FJsonValue>> Mutated = *Decisions;
        const int32 Index = Mutated.IndexOfByPredicate(
            [](const TSharedPtr<FJsonValue>& Value)
            {
                FString CardId;
                return Value->AsObject().IsValid()
                    && Value->AsObject()->TryGetStringField(TEXT("cardDefinitionId"), CardId)
                    && CardId.StartsWith(TEXT("forgeweave."));
            });
        if (Index == INDEX_NONE)
        {
            return false;
        }
        Mutated.RemoveAt(Index);
        Forgeweave->SetArrayField(TEXT("decisionHistory"), MoveTemp(Mutated));
        return true;
    }

    bool AddUnownedRenamedSpotOrderJson(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject>* World = nullptr;
        const TSharedPtr<FJsonObject>* Trade = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Orders = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Deliveries = nullptr;
        if (!Campaign.TryGetObjectField(FDASaveJsonFields::WorldState, World)
            || World == nullptr
            || !(*World)->TryGetObjectField(FDASaveJsonFields::Trade, Trade)
            || Trade == nullptr
            || !(*Trade)->TryGetArrayField(TEXT("spotOrders"), Orders)
            || !(*Trade)->TryGetArrayField(TEXT("deliveries"), Deliveries)
            || Orders == nullptr || Orders->IsEmpty() || Deliveries == nullptr)
        {
            return false;
        }
        const TSharedPtr<FJsonObject> Order = MakeShared<FJsonObject>(*(*Orders)[0]->AsObject());
        Order->SetStringField(TEXT("orderId"), TEXT("external.relief.request.999"));
        Order->SetNumberField(TEXT("worldTick"), 1.0);
        const TSharedPtr<FJsonObject> Delivery = MakeShared<FJsonObject>();
        Delivery->SetStringField(TEXT("deliveryId"), TEXT("shipment.external.relief.999"));
        Delivery->SetStringField(TEXT("contractId"), TEXT("external.relief.request.999"));
        Delivery->SetStringField(TEXT("goodId"), TEXT("resource.regenerative_materials"));
        Delivery->SetNumberField(TEXT("quantity"), Order->GetNumberField(TEXT("quantity")));
        Delivery->SetNumberField(TEXT("worldTick"), 1.0);
        TArray<TSharedPtr<FJsonValue>> MutatedOrders = *Orders;
        MutatedOrders.Add(MakeShared<FJsonValueObject>(Order));
        TArray<TSharedPtr<FJsonValue>> MutatedDeliveries = *Deliveries;
        MutatedDeliveries.Add(MakeShared<FJsonValueObject>(Delivery));
        (*Trade)->SetArrayField(TEXT("spotOrders"), MoveTemp(MutatedOrders));
        (*Trade)->SetArrayField(TEXT("deliveries"), MoveTemp(MutatedDeliveries));
        return true;
    }

    bool AddDefenseActorOutsideIronheartJson(FJsonObject& Campaign)
    {
        const TSharedPtr<FJsonObject>* World = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
        if (!Campaign.TryGetObjectField(FDASaveJsonFields::WorldState, World)
            || World == nullptr
            || !(*World)->TryGetArrayField(TEXT("regions"), Regions)
            || Regions == nullptr)
        {
            return false;
        }
        TSharedPtr<FJsonObject> DefenseActor;
        for (const TSharedPtr<FJsonValue>& RegionValue : *Regions)
        {
            const TSharedPtr<FJsonObject> Region = RegionValue->AsObject();
            FString RegionId;
            const TSharedPtr<FJsonObject>* Delta = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Actors = nullptr;
            if (!Region.IsValid()
                || !Region->TryGetStringField(TEXT("regionId"), RegionId)
                || RegionId != TEXT("region.ironheart")
                || !Region->TryGetObjectField(TEXT("persistentDelta"), Delta)
                || Delta == nullptr
                || !(*Delta)->TryGetArrayField(TEXT("localActors"), Actors)
                || Actors == nullptr)
            {
                continue;
            }
            for (const TSharedPtr<FJsonValue>& ActorValue : *Actors)
            {
                const TSharedPtr<FJsonObject> Actor = ActorValue->AsObject();
                FString Definition;
                if (Actor.IsValid()
                    && Actor->TryGetStringField(TEXT("definitionId"), Definition)
                    && Definition == TEXT("forgeweave.defense_cover"))
                {
                    DefenseActor = MakeShared<FJsonObject>(*Actor);
                    DefenseActor->SetStringField(TEXT("actorId"), TEXT("forgeweave.defense.outside_ironheart"));
                    break;
                }
            }
        }
        if (!DefenseActor.IsValid())
        {
            return false;
        }
        for (const TSharedPtr<FJsonValue>& RegionValue : *Regions)
        {
            const TSharedPtr<FJsonObject> Region = RegionValue->AsObject();
            FString RegionId;
            const TSharedPtr<FJsonObject>* Delta = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Actors = nullptr;
            if (Region.IsValid()
                && Region->TryGetStringField(TEXT("regionId"), RegionId)
                && RegionId != TEXT("region.ironheart")
                && Region->TryGetObjectField(TEXT("persistentDelta"), Delta)
                && Delta != nullptr
                && (*Delta)->TryGetArrayField(TEXT("localActors"), Actors)
                && Actors != nullptr)
            {
                TArray<TSharedPtr<FJsonValue>> Mutated = *Actors;
                Mutated.Add(MakeShared<FJsonValueObject>(DefenseActor));
                (*Delta)->SetArrayField(TEXT("localActors"), MoveTemp(Mutated));
                return true;
            }
        }
        return false;
    }

    bool AddArbitraryDefinitionDuplicateActorIdJson(
        FJsonObject& Campaign,
        const bool bDuplicateDefenseId)
    {
        const TSharedPtr<FJsonObject>* World = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
        if (!Campaign.TryGetObjectField(FDASaveJsonFields::WorldState, World)
            || World == nullptr
            || !(*World)->TryGetArrayField(TEXT("regions"), Regions)
            || Regions == nullptr)
        {
            return false;
        }

        TSharedPtr<FJsonObject> Duplicate;
        for (const TSharedPtr<FJsonValue>& RegionValue : *Regions)
        {
            const TSharedPtr<FJsonObject> Region = RegionValue->AsObject();
            FString RegionId;
            const TSharedPtr<FJsonObject>* Delta = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Actors = nullptr;
            if (!Region.IsValid()
                || !Region->TryGetStringField(TEXT("regionId"), RegionId)
                || RegionId != TEXT("region.ironheart")
                || !Region->TryGetObjectField(TEXT("persistentDelta"), Delta)
                || Delta == nullptr
                || !(*Delta)->TryGetArrayField(TEXT("localActors"), Actors)
                || Actors == nullptr)
            {
                continue;
            }
            for (const TSharedPtr<FJsonValue>& ActorValue : *Actors)
            {
                const TSharedPtr<FJsonObject> Actor = ActorValue->AsObject();
                FString Definition;
                if (!Actor.IsValid() || !Actor->TryGetStringField(TEXT("definitionId"), Definition))
                {
                    continue;
                }
                const bool bIsDefense = Definition == TEXT("forgeweave.defense_cover");
                if (bIsDefense == bDuplicateDefenseId)
                {
                    Duplicate = MakeShared<FJsonObject>(*Actor);
                    Duplicate->SetStringField(TEXT("definitionId"), TEXT("ambient.non_planner_decoy"));
                    Duplicate->SetStringField(TEXT("worldAssetId"), TEXT("00000000000000000000000000000000"));
                    break;
                }
            }
        }
        if (!Duplicate.IsValid())
        {
            return false;
        }

        for (const TSharedPtr<FJsonValue>& RegionValue : *Regions)
        {
            const TSharedPtr<FJsonObject> Region = RegionValue->AsObject();
            FString RegionId;
            const TSharedPtr<FJsonObject>* Delta = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Actors = nullptr;
            if (Region.IsValid()
                && Region->TryGetStringField(TEXT("regionId"), RegionId)
                && RegionId != TEXT("region.ironheart")
                && Region->TryGetObjectField(TEXT("persistentDelta"), Delta)
                && Delta != nullptr
                && (*Delta)->TryGetArrayField(TEXT("localActors"), Actors)
                && Actors != nullptr)
            {
                TArray<TSharedPtr<FJsonValue>> Mutated = *Actors;
                Mutated.Add(MakeShared<FJsonValueObject>(Duplicate));
                (*Delta)->SetArrayField(TEXT("localActors"), MoveTemp(Mutated));
                return true;
            }
        }
        return false;
    }

    bool AddArbitraryDefinitionDuplicateBuildingIdJson(FJsonObject& Campaign)
    {
        return AddArbitraryDefinitionDuplicateActorIdJson(Campaign, false);
    }

    bool AddArbitraryDefinitionDuplicateDefenseIdJson(FJsonObject& Campaign)
    {
        return AddArbitraryDefinitionDuplicateActorIdJson(Campaign, true);
    }

    enum class EV5EmbeddedTamper : uint8
    {
        NonEmptyAbsentDamage,
        NonModularDamage,
        IncoherentModule
    };

    bool TamperEmbeddedV5Forgeweave(FJsonObject& Campaign, const EV5EmbeddedTamper Tamper)
    {
        const TSharedPtr<FJsonObject> Forgeweave = GetForgeweaveJson(Campaign);
        const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
        if (!Forgeweave.IsValid()
            || !Forgeweave->TryGetArrayField(TEXT("buildings"), Buildings)
            || Buildings == nullptr)
        {
            return false;
        }

        TSharedPtr<FJsonObject> ModularDamage;
        if (Tamper == EV5EmbeddedTamper::NonModularDamage)
        {
            for (const TSharedPtr<FJsonValue>& BuildingValue : *Buildings)
            {
                const TSharedPtr<FJsonObject> Building = BuildingValue->AsObject();
                bool bHasDamage = false;
                const TSharedPtr<FJsonObject>* Damage = nullptr;
                if (Building.IsValid()
                    && Building->TryGetBoolField(TEXT("bHasStructuralDamageRecord"), bHasDamage)
                    && bHasDamage
                    && Building->TryGetObjectField(TEXT("structuralDamage"), Damage)
                    && Damage != nullptr)
                {
                    ModularDamage = MakeShared<FJsonObject>(**Damage);
                    break;
                }
            }
            if (!ModularDamage.IsValid())
            {
                return false;
            }
        }

        for (const TSharedPtr<FJsonValue>& BuildingValue : *Buildings)
        {
            const TSharedPtr<FJsonObject> Building = BuildingValue->AsObject();
            const TSharedPtr<FJsonObject>* Asset = nullptr;
            bool bHasDamage = false;
            if (!Building.IsValid()
                || !Building->TryGetObjectField(TEXT("assetRecord"), Asset)
                || Asset == nullptr
                || !Building->TryGetBoolField(TEXT("bHasStructuralDamageRecord"), bHasDamage))
            {
                continue;
            }
            FString DefinitionId;
            FString WorldAssetId;
            if (!(*Asset)->TryGetStringField(TEXT("cardDefinitionId"), DefinitionId)
                || !(*Asset)->TryGetStringField(TEXT("worldAssetId"), WorldAssetId))
            {
                return false;
            }
            const bool bIsModular = DefinitionId == TEXT("forgeweave.infinite_foundry")
                || DefinitionId == TEXT("forgeweave.freight_furnace")
                || DefinitionId == TEXT("forgeweave.smog_reclaimer");
            if (Tamper == EV5EmbeddedTamper::NonEmptyAbsentDamage && !bHasDamage)
            {
                const TSharedRef<FJsonObject> Damage = MakeShared<FJsonObject>();
                Damage->SetStringField(TEXT("worldAssetId"), WorldAssetId);
                Damage->SetStringField(TEXT("cardDefinitionId"), DefinitionId);
                Building->SetObjectField(TEXT("structuralDamage"), Damage);
                return true;
            }
            if (Tamper == EV5EmbeddedTamper::NonModularDamage && !bIsModular)
            {
                ModularDamage->SetStringField(TEXT("worldAssetId"), WorldAssetId);
                ModularDamage->SetStringField(TEXT("cardDefinitionId"), DefinitionId);
                Building->SetBoolField(TEXT("bHasStructuralDamageRecord"), true);
                Building->SetObjectField(TEXT("structuralDamage"), ModularDamage.ToSharedRef());
                return true;
            }
            if (Tamper == EV5EmbeddedTamper::IncoherentModule && bHasDamage)
            {
                const TSharedPtr<FJsonObject>* Damage = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
                if (!Building->TryGetObjectField(TEXT("structuralDamage"), Damage)
                    || Damage == nullptr
                    || !(*Damage)->TryGetArrayField(TEXT("modules"), Modules)
                    || Modules == nullptr
                    || Modules->IsEmpty()
                    || !(*Modules)[0]->AsObject().IsValid())
                {
                    return false;
                }
                (*Modules)[0]->AsObject()->SetNumberField(TEXT("currentHealth"), 100000.0);
                return true;
            }
        }
        return false;
    }

    bool AddOrphanConstructionAuthorityJson(FJsonObject& Campaign)
    {
        const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
        const TSharedPtr<FJsonObject>* World = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
        if (!Campaign.TryGetArrayField(FDASaveJsonFields::WorldAssets, Assets)
            || Assets == nullptr
            || !Campaign.TryGetObjectField(FDASaveJsonFields::WorldState, World)
            || World == nullptr
            || !(*World)->TryGetArrayField(TEXT("regions"), Regions)
            || Regions == nullptr)
        {
            return false;
        }
        constexpr TCHAR OrphanGuid[] = TEXT("11111111111111111111111111111111");
        for (const TSharedPtr<FJsonValue>& RegionValue : *Regions)
        {
            const TSharedPtr<FJsonObject> Region = RegionValue->AsObject();
            FString RegionId;
            const TSharedPtr<FJsonObject>* Delta = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Actors = nullptr;
            if (!Region.IsValid()
                || !Region->TryGetStringField(TEXT("regionId"), RegionId)
                || RegionId != TEXT("region.ironheart")
                || !Region->TryGetObjectField(TEXT("persistentDelta"), Delta)
                || Delta == nullptr
                || !(*Delta)->TryGetArrayField(TEXT("localActors"), Actors)
                || Actors == nullptr)
            {
                continue;
            }
            for (const TSharedPtr<FJsonValue>& ActorValue : *Actors)
            {
                const TSharedPtr<FJsonObject> Actor = ActorValue->AsObject();
                FString Definition;
                FString AssetId;
                if (!Actor.IsValid()
                    || !Actor->TryGetStringField(TEXT("definitionId"), Definition)
                    || Definition == TEXT("forgeweave.defense_cover")
                    || !Definition.StartsWith(TEXT("forgeweave."))
                    || !Actor->TryGetStringField(TEXT("worldAssetId"), AssetId))
                {
                    continue;
                }
                const TSharedPtr<FJsonValue>* AssetValue = Assets->FindByPredicate(
                    [&AssetId](const TSharedPtr<FJsonValue>& Value)
                    {
                        FString Candidate;
                        return Value->AsObject().IsValid()
                            && Value->AsObject()->TryGetStringField(TEXT("worldAssetId"), Candidate)
                            && Candidate == AssetId;
                    });
                if (AssetValue == nullptr)
                {
                    return false;
                }
                const TSharedPtr<FJsonObject> OrphanActor = MakeShared<FJsonObject>(*Actor);
                OrphanActor->SetStringField(TEXT("actorId"), TEXT("forgeweave.asset.orphan"));
                OrphanActor->SetStringField(TEXT("worldAssetId"), OrphanGuid);
                const TSharedPtr<FJsonObject> OrphanAsset = MakeShared<FJsonObject>(*(*AssetValue)->AsObject());
                OrphanAsset->SetStringField(TEXT("worldAssetId"), OrphanGuid);
                TArray<TSharedPtr<FJsonValue>> MutatedActors = *Actors;
                MutatedActors.Add(MakeShared<FJsonValueObject>(OrphanActor));
                TArray<TSharedPtr<FJsonValue>> MutatedAssets = *Assets;
                MutatedAssets.Add(MakeShared<FJsonValueObject>(OrphanAsset));
                (*Delta)->SetArrayField(TEXT("localActors"), MoveTemp(MutatedActors));
                Campaign.SetArrayField(FDASaveJsonFields::WorldAssets, MoveTemp(MutatedAssets));
                return true;
            }
        }
        return false;
    }
}

BEGIN_DEFINE_SPEC(FDASaveMigrationSpec, "Dominion.Core.Save.Migration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

    FString TestSaveDirectory;
END_DEFINE_SPEC(FDASaveMigrationSpec)

void FDASaveMigrationSpec::Define()
{
    BeforeEach([this]()
    {
        TestSaveDirectory = FPaths::Combine(
            FPaths::ProjectIntermediateDir(),
            TEXT("DominionSaveMigrationTests"),
            FGuid::NewGuid().ToString(EGuidFormats::Digits));
    });

    AfterEach([this]()
    {
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*TestSaveDirectory);
    });

    It("loads a checksummed schema-v1 Adaptive Habitat through the public migration path", [this]()
    {
        const FString Slot = TEXT("version-one");
        FDASaveService SaveService(TestSaveDirectory);

        FDACampaignSnapshot Snapshot;
        FCardInstance Card;
        Card.InstanceId = FGuid(1, 2, 3, 4);
        Card.DefinitionId = FName(TEXT("synara.adaptive_habitat"));
        Card.MasteryXp = 725;
        Card.WorldAssetId = FGuid(5, 6, 7, 8);
        Snapshot.CollectionState.Instances.Add(Card.InstanceId, Card);
        Snapshot.DeckState.SetInstanceIds({ Card.InstanceId });

        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = Card.WorldAssetId;
        Asset.CardInstanceId = Card.InstanceId;
        Asset.CardDefinitionId = Card.DefinitionId;
        Asset.CityId = FName(TEXT("player_capital"));
        Asset.GridOrigin = FIntPoint(14, 22);
        Asset.Rotation = 1;
        Asset.ConstructionState = EDAConstructionState::Frame;
        Asset.ConstructionCyclesCompleted = 1;
        Asset.ConstructionCyclesRequired = 3;
        Asset.StructuralIntegrity = 83.5f;
        Snapshot.WorldAssets.Add(Asset);
        Snapshot.HistoryTags.Add(FName(TEXT("history.v2_only")));

        TestTrue("Current fixture saves before conversion to v1", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());
        TestTrue("Fixture becomes checksummed schema v1 without HistoryTags", RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 1.0, true));

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestTrue("Public load migrates schema v1", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            return;
        }

        const FDACampaignSnapshot& Loaded = LoadResult.GetValue();
        const FGuid CardId(1, 2, 3, 4);
        const FCardInstance* LoadedCard = Loaded.CollectionState.FindInstance(CardId);
        TestTrue("Migrated card remains present", LoadedCard != nullptr);
        TestEqual("Migration adds empty history", Loaded.HistoryTags.Num(), 0);
        TestEqual("Deck membership is preserved", Loaded.DeckState.GetInstanceIds().Num(), 1);
        TestEqual("Migrated world asset remains present", Loaded.WorldAssets.Num(), 1);
        if (LoadedCard != nullptr)
        {
            TestEqual("Mastery is preserved", LoadedCard->MasteryXp, 725);
            TestEqual("Card identity is preserved", LoadedCard->InstanceId, CardId);
        }
        if (Loaded.DeckState.GetInstanceIds().Num() == 1)
        {
            TestEqual("Deck identity is preserved", Loaded.DeckState.GetInstanceIds()[0], CardId);
        }
        if (Loaded.WorldAssets.Num() == 1)
        {
            TestEqual("City is preserved", Loaded.WorldAssets[0].CityId, FName(TEXT("player_capital")));
            TestEqual("Integrity is preserved", Loaded.WorldAssets[0].StructuralIntegrity, 83.5f);
            TestEqual("Construction progress is preserved", Loaded.WorldAssets[0].ConstructionCyclesCompleted, 1);
            TestTrue("Construction state is preserved", Loaded.WorldAssets[0].ConstructionState == EDAConstructionState::Frame);
        }
    });

    It("rejects a changed schema version when the checksum covers the original version", [this]()
    {
        const FString Slot = TEXT("schema-corruption");
        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Current campaign fixture saves", SaveService.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess());

        const FString ActivePath = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        FString SaveDocument;
        TSharedPtr<FJsonObject> Root;
        TestTrue("Saved envelope is readable", FFileHelper::LoadFileToString(SaveDocument, *ActivePath));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        TestTrue("Saved envelope is valid JSON", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        if (!Root.IsValid())
        {
            return;
        }

        Root->SetNumberField(FDASaveJsonFields::SchemaVersion, 1.0);
        TestTrue("Corrupted envelope serializes", SerializeFixtureJson(Root.ToSharedRef(), SaveDocument));
        TestTrue("Corrupted envelope overwrites active fixture", FFileHelper::SaveStringToFile(SaveDocument, *ActivePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestFalse("Changed schema version is rejected", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            TestTrue("Version corruption is a checksum mismatch", LoadResult.GetError().Code == EDASaveErrorCode::ChecksumMismatch);
        }
    });

    It("migrates a checksummed schema-v2 campaign to an empty durable operation-conflict aggregate", [this]()
    {
        const FString Slot = TEXT("version-two-conflict");
        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Current fixture saves before conversion to v2", SaveService.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess());
        TestTrue("Fixture becomes checksummed schema v2 without conflict state", RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 2.0, false));

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestTrue("Public load migrates schema v2", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            return;
        }

        const FDAOperationConflictSnapshot& Conflict = LoadResult.GetValue().OperationConflict;
        TestEqual("Migration creates no structural records", Conflict.StructuralDamageRecords.Num(), 0);
        TestEqual("Migration creates no capture records", Conflict.CaptureRecords.Num(), 0);
        TestEqual("Migration creates no surrender records", Conflict.SurrenderRecords.Num(), 0);
        TestEqual("Migration creates no campaign history", LoadResult.GetValue().HistoryTags.Num(), 0);
        TestEqual("Migration creates zero Insight", Conflict.Resources.Insight, 0.f);
    });

    It("migrates schema v3 to an explicit empty regional-world aggregate", [this]()
    {
        TestEqual("Canonical First Ascension authority uses schema v18",
            FDASaveService::CurrentSchemaVersion, 19);
        const FString Slot = TEXT("version-three-world");
        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Current fixture saves before conversion to v3", SaveService.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess());

        bool bSerializedWorldStateExists = false;
        TestTrue(
            "The current fixture is readable before rewrite",
            CampaignFieldExistsOnDisk(TestSaveDirectory, Slot, FDASaveJsonFields::WorldState, bSerializedWorldStateExists));
        TestTrue("Serialization writes the standardized worldState key", bSerializedWorldStateExists);

        TestTrue("Fixture becomes checksummed schema v3 without world state", RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 3.0, false));

        bool bWorldStateExists = true;
        TestTrue(
            "The rewritten fixture remains readable before migration",
            CampaignFieldExistsOnDisk(TestSaveDirectory, Slot, FDASaveJsonFields::WorldState, bWorldStateExists));
        TestFalse("The actual reflected worldState key is absent on disk before migration", bWorldStateExists);

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestTrue("Public load migrates schema v3", LoadResult.HasValue());
        if (LoadResult.HasValue())
        {
            TestFalse("Migration creates an explicit uninitialized regional aggregate", LoadResult.GetValue().WorldState.bInitialized);
            TestEqual("Migrated world has no region records", LoadResult.GetValue().WorldState.Regions.Num(), 0);
        }
    });

    It("migrates schema v6 to an explicit empty canonical narrative aggregate", [this]()
    {
        const FString Slot = TEXT("version-six-narrative");
        FDASaveService SaveService(TestSaveDirectory);
        FDACampaignSnapshot Snapshot;
        Snapshot.HistoryTags.Add(TEXT("founder_hall_awake"));
        TestTrue("Current fixture saves before conversion to v6", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());
        TestTrue("Fixture becomes checksummed schema v6 without narrative state", RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 6.0, false));

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestTrue("Public load migrates schema v6", LoadResult.HasValue());
        if (LoadResult.HasValue())
        {
            const FDANarrativeCampaignState& Narrative = LoadResult.GetValue().NarrativeState;
            TestEqual("Migration creates no quest records", Narrative.QuestStates.Num(), 0);
            TestEqual("Migration creates no event records", Narrative.EventStates.Num(), 0);
            TestEqual("Migration creates no promise records", Narrative.PromiseRecords.Num(), 0);
            TestEqual("Migration creates no semantic action records", Narrative.ActionRecords.Num(), 0);
            TestEqual("Migration starts at revision zero", Narrative.MutationRevision, 0LL);
            TestEqual("Existing canonical history remains intact", LoadResult.GetValue().HistoryTags, TArray<FName>({TEXT("founder_hall_awake")}));
        }
    });

    It("migrates schema-v7 duplicate conflict history and ActionID-only idempotency without inventing semantics", [this]()
    {
        const FString Slot = TEXT("version-seven-authorities");
        FDACampaignSnapshot Snapshot;
        FDASurrenderRecord Surrender;
        Surrender.SquadId = FGuid(31, 32, 33, 34);
        Surrender.bAccepted = true;
        Surrender.History.Add(TEXT("forge_guard_surrender_accepted"));
        Snapshot.OperationConflict.SurrenderRecords.Add(Surrender);
        Snapshot.HistoryTags = {TEXT("action.legacy.fulfill"), TEXT("forge_guard_surrender_accepted")};
        FDAPromiseRecord LegacyPromise;
        LegacyPromise.PromiseId = FGuid(35, 36, 37, 38);
        LegacyPromise.PromiseDefinitionId = TEXT("promise.legacy.resolved");
        LegacyPromise.PromiserId = TEXT("leader.legacy");
        LegacyPromise.ConflictActionTags = {TEXT("action.legacy.conflict")};
        LegacyPromise.FulfillmentActionTags = {TEXT("action.legacy.fulfill")};
        LegacyPromise.State = EDAPromiseState::Fulfilled;
        LegacyPromise.CreatedWorldTick = 10;
        LegacyPromise.ResolvedWorldTick = 12;
        LegacyPromise.ResolutionActionTag = TEXT("action.legacy.fulfill");
        LegacyPromise.ResolutionActionId = FGuid(41, 42, 43, 44);
        Snapshot.NarrativeState.PromiseRecords.Add(LegacyPromise);
        FDANarrativeActionRecord LegacyAction;
        LegacyAction.ActionId = LegacyPromise.ResolutionActionId;
        LegacyAction.NormalizedActionTags = {TEXT("action.legacy.fulfill")};
        LegacyAction.WorldTick = 12;
        LegacyAction.FulfilledPromiseIds = {LegacyPromise.PromiseId};
        Snapshot.NarrativeState.ActionRecords.Add(LegacyAction);

        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Current authority fixture saves", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());
        TestTrue("Fixture rewrites to schema-v7 duplicate authorities", RewriteAsLegacySchemaV7(TestSaveDirectory, Slot));
        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestTrue("Schema-v7 authority fixture migrates", LoadResult.HasValue());
        if (!LoadResult.HasValue()) return;
        const FDACampaignSnapshot& Loaded = LoadResult.GetValue();
        TestEqual("Canonical action and promoted surrender tags survive", Loaded.HistoryTags,
            TArray<FName>({TEXT("action.legacy.fulfill"), TEXT("forge_guard_surrender_accepted")}));
        TestEqual("One legacy semantic record survives", Loaded.NarrativeState.ActionRecords.Num(), 1);
        if (Loaded.NarrativeState.ActionRecords.Num() == 1)
        {
            const FDANarrativeActionRecord& Action = Loaded.NarrativeState.ActionRecords[0];
            TestEqual("Stable ActionID survives", Action.ActionId, LegacyAction.ActionId);
            TestTrue("Unknown schema-v7 semantics are explicit", Action.bLegacyIdentityOnly);
            TestEqual("Migration invents no action tags", Action.NormalizedActionTags.Num(), 0);
        }
        TestEqual("Resolved promise survives", Loaded.NarrativeState.PromiseRecords.Num(), 1);
        if (Loaded.NarrativeState.PromiseRecords.Num() == 1)
        {
            const FDAPromiseRecord& Promise = Loaded.NarrativeState.PromiseRecords[0];
            TestTrue("Schema-v7 unlinked resolution has explicit provenance", Promise.bLegacyResolutionWithoutAction);
            TestEqual("Legacy resolution names its source schema", Promise.LegacyResolutionSourceSchemaVersion, 7);
            TestFalse("Migration does not invent a promise ActionID link", Promise.ResolutionActionId.IsValid());
        }
    });

    It("promotes a native schema-v8 semantic result to an exact promise link", [this]()
    {
        const FString Slot = TEXT("version-eight-linked-promise");
        FDACampaignSnapshot Snapshot;
        Snapshot.HistoryTags = {TEXT("action.v8.fulfill")};
        FDAPromiseRecord Promise;
        Promise.PromiseId = FGuid(51, 52, 53, 54);
        Promise.PromiseDefinitionId = TEXT("promise.v8.linked");
        Promise.PromiserId = TEXT("leader.v8");
        Promise.ConflictActionTags = {TEXT("action.v8.conflict")};
        Promise.FulfillmentActionTags = {TEXT("action.v8.fulfill")};
        Promise.State = EDAPromiseState::Fulfilled;
        Promise.CreatedWorldTick = 4;
        Promise.ResolvedWorldTick = 8;
        Promise.ResolutionActionTag = TEXT("action.v8.fulfill");
        Promise.ResolutionActionId = FGuid(55, 56, 57, 58);
        Snapshot.NarrativeState.PromiseRecords.Add(Promise);
        FDANarrativeActionRecord Action;
        Action.ActionId = Promise.ResolutionActionId;
        Action.NormalizedActionTags = {TEXT("action.v8.fulfill")};
        Action.WorldTick = 8;
        Action.FulfilledPromiseIds = {Promise.PromiseId};
        Snapshot.NarrativeState.ActionRecords.Add(Action);

        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Linked v8 fixture saves canonically", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());
        TestTrue("V9-only promise fields are removed with a valid checksum", RewriteChecksummedCampaign(
            TestSaveDirectory,
            Slot,
            [](FJsonObject& Campaign)
            {
                const TSharedPtr<FJsonObject>* Narrative = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* Promises = nullptr;
                if (!Campaign.TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative)
                    || Narrative == nullptr
                    || !(*Narrative)->TryGetArrayField(TEXT("promiseRecords"), Promises)
                    || Promises == nullptr || Promises->Num() != 1)
                {
                    return false;
                }
                const TSharedPtr<FJsonObject> PromiseJson = (*Promises)[0]->AsObject();
                PromiseJson->RemoveField(TEXT("resolutionActionId"));
                PromiseJson->RemoveField(TEXT("bLegacyResolutionWithoutAction"));
                PromiseJson->RemoveField(TEXT("legacyResolutionSourceSchemaVersion"));
                return true;
            }));
        TestTrue("Fixture becomes native schema v8", RewriteActiveEnvelopeVersion(
            TestSaveDirectory, Slot, 8.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(Slot);
        TestTrue("Native v8 semantic result migrates", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            const FDAPromiseRecord& Migrated = Loaded.GetValue().NarrativeState.PromiseRecords[0];
            TestEqual("Migration links the exact semantic ActionID", Migrated.ResolutionActionId, Action.ActionId);
            TestFalse("Modern semantic result is not mislabeled legacy", Migrated.bLegacyResolutionWithoutAction);
        }
    });

    It("rejects a mixed-era schema-v8 orphan instead of borrowing unrelated legacy provenance", [this]()
    {
        const FString Slot = TEXT("version-eight-mixed-orphan");
        FDACampaignSnapshot Snapshot;
        Snapshot.HistoryTags = {TEXT("action.v8.first"), TEXT("action.v8.second")};
        for (int32 Index = 0; Index < 2; ++Index)
        {
            FDAPromiseRecord Promise;
            Promise.PromiseId = FGuid(60 + Index, 1, 2, 3);
            Promise.PromiseDefinitionId = FName(*FString::Printf(TEXT("promise.v8.%d"), Index));
            Promise.PromiserId = TEXT("leader.v8");
            Promise.ConflictActionTags = {FName(*FString::Printf(TEXT("action.v8.conflict.%d"), Index))};
            Promise.FulfillmentActionTags = {Index == 0 ? FName(TEXT("action.v8.first")) : FName(TEXT("action.v8.second"))};
            Promise.State = EDAPromiseState::Fulfilled;
            Promise.CreatedWorldTick = 1;
            Promise.ResolvedWorldTick = 10 + Index;
            Promise.ResolutionActionTag = Promise.FulfillmentActionTags[0];
            Promise.ResolutionActionId = FGuid(70 + Index, 4, 5, 6);
            Snapshot.NarrativeState.PromiseRecords.Add(Promise);
            FDANarrativeActionRecord Action;
            Action.ActionId = Promise.ResolutionActionId;
            Action.NormalizedActionTags = Promise.FulfillmentActionTags;
            Action.WorldTick = Promise.ResolvedWorldTick;
            Action.FulfilledPromiseIds = {Promise.PromiseId};
            Snapshot.NarrativeState.ActionRecords.Add(Action);
        }

        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Mixed-era fixture saves canonically", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());
        TestTrue("One unrelated legacy identity and one orphan are checksummed", RewriteChecksummedCampaign(
            TestSaveDirectory,
            Slot,
            [](FJsonObject& Campaign)
            {
                const TSharedPtr<FJsonObject>* Narrative = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
                if (!Campaign.TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative)
                    || Narrative == nullptr
                    || !(*Narrative)->TryGetArrayField(FDASaveJsonFields::ActionRecords, Actions)
                    || Actions == nullptr || Actions->Num() != 2)
                {
                    return false;
                }
                const TSharedPtr<FJsonObject> Legacy = (*Actions)[0]->AsObject();
                Legacy->SetArrayField(TEXT("normalizedActionTags"), TArray<TSharedPtr<FJsonValue>>());
                Legacy->SetNumberField(TEXT("worldTick"), 0.0);
                Legacy->SetArrayField(TEXT("fulfilledPromiseIds"), TArray<TSharedPtr<FJsonValue>>());
                Legacy->SetArrayField(TEXT("breachedPromiseIds"), TArray<TSharedPtr<FJsonValue>>());
                Legacy->SetBoolField(TEXT("bLegacyIdentityOnly"), true);
                (*Actions)[1]->AsObject()->SetArrayField(
                    TEXT("fulfilledPromiseIds"), TArray<TSharedPtr<FJsonValue>>());
                return true;
            }));
        TestTrue("Mixed-era fixture becomes schema v8", RewriteActiveEnvelopeVersion(
            TestSaveDirectory, Slot, 8.0, false));
        TestFalse("Unrelated legacy identity cannot legitimize an orphan promise",
            SaveService.LoadCampaign(Slot).HasValue());
    });

    It("rejects a checksummed schema-v8 stale definition fingerprint before refresh", [this]()
    {
        const FString Slot = TEXT("version-eight-stale-fingerprint");
        FDACampaignSnapshot Snapshot;
        FDAQuestNodeDefinition Start;
        Start.NodeId = TEXT("start");
        Start.Type = EDAQuestNodeType::Start;
        Start.SourceDefinitionId = TEXT("quest.v8.start");
        FDAQuestEdgeDefinition Begin;
        Begin.BranchTag = TEXT("Begin");
        Begin.TargetNodeId = TEXT("resolution");
        Start.Edges.Add(Begin);
        FDAQuestNodeDefinition Resolution;
        Resolution.NodeId = TEXT("resolution");
        Resolution.Type = EDAQuestNodeType::Resolution;
        Resolution.SourceDefinitionId = TEXT("quest.v8.resolution");
        FDAQuestSaveState State;
        State.QuestId = TEXT("quest.v8.fingerprint");
        State.DefinitionVersion = 1;
        State.CurrentNodeId = Start.NodeId;
        State.DefinitionManifest.QuestId = State.QuestId;
        State.DefinitionManifest.SourceDefinitionId = TEXT("quest.source.v8.fingerprint");
        State.DefinitionManifest.Version = 1;
        State.DefinitionManifest.StartNodeId = Start.NodeId;
        State.DefinitionManifest.Nodes = {Start, Resolution};
        State.DefinitionManifest.RefreshFingerprint();
        Snapshot.NarrativeState.QuestStates.Add(State);

        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Definition fingerprint fixture saves", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());
        TestTrue("Stored fingerprint is tampered behind a valid checksum", RewriteChecksummedCampaign(
            TestSaveDirectory,
            Slot,
            [](FJsonObject& Campaign)
            {
                const TSharedPtr<FJsonObject>* Narrative = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* Quests = nullptr;
                const TSharedPtr<FJsonObject>* Manifest = nullptr;
                if (!Campaign.TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative)
                    || Narrative == nullptr
                    || !(*Narrative)->TryGetArrayField(TEXT("questStates"), Quests)
                    || Quests == nullptr || Quests->Num() != 1
                    || !(*Quests)[0]->AsObject()->TryGetObjectField(TEXT("definitionManifest"), Manifest)
                    || Manifest == nullptr)
                {
                    return false;
                }
                (*Manifest)->SetStringField(TEXT("definitionFingerprint"), TEXT("stale-but-checksummed"));
                return true;
            }));
        TestTrue("Tampered definition fixture becomes schema v8", RewriteActiveEnvelopeVersion(
            TestSaveDirectory, Slot, 8.0, false));
        TestFalse("Migration does not bless a stale v8 fingerprint", SaveService.LoadCampaign(Slot).HasValue());
    });

    It("migrates schema v4 to deterministic durable Forgeweave authority", [this]()
    {
        const FString Slot = TEXT("version-four-forgeweave");
        FDASaveService SaveService(TestSaveDirectory);
        FDACampaignSnapshot Snapshot;
        Snapshot.WorldState.bInitialized = true;
        Snapshot.WorldState.CurrentWorldTick = 12;
        Snapshot.WorldState.CurrentRegionId = TEXT("region.synara_frontier");
        Snapshot.WorldState.Trade.LastProcessedWorldTick = 12;
        FDARegionState Synara;
        Synara.RegionId = TEXT("region.synara_frontier");
        Synara.OwnerId = TEXT("civilization.synara");
        Snapshot.WorldState.Regions.Add(Synara);
        FDARegionState Ironheart;
        Ironheart.RegionId = TEXT("region.ironheart");
        Ironheart.OwnerId = TEXT("civilization.forgeweave");
        Snapshot.WorldState.Regions.Add(Ironheart);
        Snapshot.WorldState.Forgeweave = FDAForgeweaveCityState::MakeVerticalSliceInitialState(1701, 12);

        TestTrue("Current Forgeweave fixture saves", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());
        TestTrue("Fixture becomes checksummed schema v4 without Forgeweave state", RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 4.0, false));

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestTrue("Public load migrates schema v4", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            return;
        }

        const FDAForgeweaveCityState& Forgeweave = LoadResult.GetValue().WorldState.Forgeweave;
        TestTrue("Initialized worlds receive initialized rival authority", Forgeweave.bInitialized);
        TestEqual("Rival authority aligns to the saved canonical tick", Forgeweave.LastProcessedWorldTick, 12LL);
        TestEqual("Migration seeds exactly the six-card pool", Forgeweave.AvailableCardIds.Num(), 6);
        TestEqual("Migration seed is deterministic", Forgeweave.CampaignSeed, 1701);
        TestEqual("Migration seeds canonical Ironheart width", Forgeweave.GridWidth, 32);
        TestEqual("Migration seeds canonical Ironheart height", Forgeweave.GridHeight, 32);
    });

    It("migrates the largest exactly representable v4 World Tick without changing it", [this]()
    {
        const FString Slot = TEXT("version-four-exact-tick");
        FDASaveService SaveService(TestSaveDirectory);
        FDACampaignSnapshot Snapshot;
        Snapshot.WorldState.bInitialized = true;
        Snapshot.WorldState.CurrentRegionId = TEXT("region.ironheart");
        Snapshot.WorldState.Forgeweave = FDAForgeweaveCityState::MakeVerticalSliceInitialState(1701, 0);
        FDARegionState Ironheart;
        Ironheart.RegionId = TEXT("region.ironheart");
        Ironheart.OwnerId = TEXT("civilization.forgeweave");
        Snapshot.WorldState.Regions.Add(Ironheart);
        TestTrue("Current fixture saves", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());

        TestTrue("Fixture becomes schema v4 at the exact JSON boundary",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 4.0, false, FString(TEXT("9007199254740992"))));
        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestTrue("Exactly representable v4 tick migrates", LoadResult.HasValue());
        if (LoadResult.HasValue())
        {
            TestEqual("World Tick is preserved exactly", LoadResult.GetValue().WorldState.CurrentWorldTick, 9007199254740992LL);
            TestEqual("Forgeweave authority is seeded at the exact tick", LoadResult.GetValue().WorldState.Forgeweave.LastProcessedWorldTick, 9007199254740992LL);
        }
    });

    It("promotes a nontrivial embedded schema-v5 Forgeweave campaign into canonical schema-v6 authorities", [this]()
    {
        const FString Slot = TEXT("version-five-embedded-forgeweave");
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        if (World == nullptr || !World->InitializeVerticalSliceState(TEXT("region.synara_frontier")))
        {
            AddError(TEXT("World fixture must initialize."));
            return;
        }
        FDACampaignSnapshot Setup = World->GetPersistentCampaign();
        Setup.WorldState.Forgeweave.MaterialScarcity = 80.f;
        TestTrue("Trade setup restores", World->RestorePersistentCampaign(Setup));
        TestTrue("Trade commits", World->AdvanceWorldTicks(1));
        Setup = World->GetPersistentCampaign();
        Setup.WorldState.Forgeweave.MaterialScarcity = 20.f;
        Setup.WorldState.Forgeweave.DefensePressure = 80.f;
        TestTrue("Defense setup restores", World->RestorePersistentCampaign(Setup));
        TestTrue("Defense commits", World->AdvanceWorldTicks(1));
        Setup = World->GetPersistentCampaign();
        Setup.WorldState.Forgeweave.DefensePressure = 0.f;
        TestTrue("Construction setup restores", World->RestorePersistentCampaign(Setup));
        TestTrue("Two construction ticks commit", World->AdvanceWorldTicks(2));
        Setup = World->GetPersistentCampaign();
        FDAForgeweaveBuildingState* Modular = Setup.WorldState.Forgeweave.Buildings.FindByPredicate(
            [&Setup](const FDAForgeweaveBuildingState& Building)
            {
                return Setup.OperationConflict.FindStructuralDamageRecord(Building.WorldAssetId) != nullptr;
            });
        TestNotNull("Embedded v5 fixture has a modular building", Modular);
        if (Modular == nullptr)
        {
            return;
        }
        FDAWorldAssetRecord* Asset = Setup.FindWorldAssetRecord(Modular->WorldAssetId);
        FDAStructuralDamageRecord* Damage = Setup.OperationConflict.FindStructuralDamageRecord(Modular->WorldAssetId);
        if (Asset == nullptr || Damage == nullptr)
        {
            return;
        }
        Asset->StructuralIntegrity = 40.f;
        Asset->ConstructionState = EDAConstructionState::Damaged;
        Damage->Modules[0].CurrentHealth = 40.f;
        Damage->Modules[0].State = EDAStructureDamageState::Damaged;
        Setup.OperationConflict.Resources.Capital = 13.f;
        Setup.OperationConflict.Resources.Insight = 7.f;
        TestTrue("Repair setup restores", World->RestorePersistentCampaign(Setup));
        TestTrue("Repair commits", World->AdvanceWorldTicks(1));

        const FDACampaignSnapshot Expected = World->GetPersistentCampaign();
        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Canonical fixture saves", SaveService.SaveCampaign(Expected, Slot).IsSuccess());
        TestTrue("Fixture rewrites to the real embedded v5 wire shape", RewriteAsEmbeddedSchemaV5(TestSaveDirectory, Slot));
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(Slot);
        TestTrue("Public load migrates embedded v5", Loaded.HasValue());
        if (!Loaded.HasValue())
        {
            return;
        }
        TestEqual("Every embedded building becomes one canonical asset", Loaded.GetValue().WorldAssets.Num(), Expected.WorldAssets.Num());
        TestEqual("Every embedded modular record becomes canonical", Loaded.GetValue().OperationConflict.StructuralDamageRecords.Num(), Expected.OperationConflict.StructuralDamageRecords.Num());
        TestEqual("Decision history is preserved", Loaded.GetValue().WorldState.Forgeweave.DecisionHistory.Num(), Expected.WorldState.Forgeweave.DecisionHistory.Num());
        TestEqual("Trade repair and fortify histories gain durable transactions", Loaded.GetValue().WorldState.Forgeweave.ActionTransactions.Num(), 3);
        TestEqual("Conflict Capital is not duplicated", Loaded.GetValue().OperationConflict.Resources.Capital, 13.f);
        TestEqual("Conflict Insight is not duplicated", Loaded.GetValue().OperationConflict.Resources.Insight, 7.f);
        FString ValidationError;
        TestTrue("Migrated nontrivial campaign is canonical", Loaded.GetValue().Validate(ValidationError));
    });

    It("rejects checksummed schema-v5 embedded authority before destructive promotion", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        if (World == nullptr
            || !World->InitializeVerticalSliceState(TEXT("region.synara_frontier"))
            || !World->AdvanceWorldTicks(2))
        {
            AddError(TEXT("Embedded-v5 malformed fixture must construct both ordinary and modular buildings."));
            return;
        }
        const FDACampaignSnapshot Authority = World->GetPersistentCampaign();
        struct FEmbeddedTamperCase
        {
            const TCHAR* Slot;
            EV5EmbeddedTamper Tamper;
        };
        const FEmbeddedTamperCase Cases[] = {
            {TEXT("v5-nonempty-absent-damage"), EV5EmbeddedTamper::NonEmptyAbsentDamage},
            {TEXT("v5-nonmodular-damage"), EV5EmbeddedTamper::NonModularDamage},
            {TEXT("v5-incoherent-module"), EV5EmbeddedTamper::IncoherentModule}
        };

        FDASaveService SaveService(TestSaveDirectory);
        for (const auto& Case : Cases)
        {
            TestTrue("Canonical source fixture saves", SaveService.SaveCampaign(Authority, Case.Slot).IsSuccess());
            TestTrue("Source fixture becomes raw embedded schema v5", RewriteAsEmbeddedSchemaV5(TestSaveDirectory, Case.Slot));
            TestTrue("Malformed embedded authority is written with a refreshed v5 checksum", RewriteChecksummedCampaign(
                TestSaveDirectory,
                Case.Slot,
                [&Case](FJsonObject& Campaign)
                {
                    return TamperEmbeddedV5Forgeweave(Campaign, Case.Tamper);
                }));
            const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(Case.Slot);
            TestFalse("Malformed embedded v5 authority is rejected", Loaded.HasValue());
            if (!Loaded.HasValue())
            {
                TestEqual("Malformed embedded v5 is a migration failure", Loaded.GetError().Code, EDASaveErrorCode::MigrationFailed);
            }
        }
    });

    It("rejects v4 World Ticks beyond exact JSON integer and clock-restorable bounds", [this]()
    {
        const FString InvalidTicks[] = {
            TEXT("9007199254740993"),
            FString::Printf(TEXT("%lld"), (MAX_int64 / 5) + 1),
            TEXT("1.0"),
            TEXT("1e3")
        };
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(InvalidTicks); ++Index)
        {
            const FString Slot = FString::Printf(TEXT("version-four-invalid-tick-%d"), Index);
            FDASaveService SaveService(TestSaveDirectory);
            FDACampaignSnapshot Snapshot;
            TestTrue("Current empty fixture saves", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());
            TestTrue("Checksummed invalid v4 fixture is written",
                RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 4.0, false, InvalidTicks[Index]));

            const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
            TestFalse("Invalid v4 strategic tick is rejected", LoadResult.HasValue());
            if (!LoadResult.HasValue())
            {
                TestTrue("Precision or restoration failure is a migration error",
                    LoadResult.GetError().Code == EDASaveErrorCode::MigrationFailed);
            }
        }
    });

    It("rejects a checksummed non-canonical uninitialized Forgeweave aggregate", [this]()
    {
        const FString Slot = TEXT("noncanonical-empty-forgeweave");
        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Canonical empty campaign saves", SaveService.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess());

        const FString ActivePath = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        FString SaveDocument;
        TSharedPtr<FJsonObject> Root;
        TestTrue("Saved envelope is readable", FFileHelper::LoadFileToString(SaveDocument, *ActivePath));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        TestTrue("Saved envelope is JSON", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        if (!Root.IsValid())
        {
            return;
        }

        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        const TSharedPtr<FJsonObject>* WorldState = nullptr;
        const TSharedPtr<FJsonObject>* Forgeweave = nullptr;
        TestTrue("Campaign exists", Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) && Campaign != nullptr);
        TestTrue("World state exists", Campaign != nullptr && (*Campaign)->TryGetObjectField(FDASaveJsonFields::WorldState, WorldState) && WorldState != nullptr);
        TestTrue("Forgeweave state exists", WorldState != nullptr && (*WorldState)->TryGetObjectField(FDASaveJsonFields::Forgeweave, Forgeweave) && Forgeweave != nullptr);
        if (Campaign == nullptr || Forgeweave == nullptr)
        {
            return;
        }

        (*Forgeweave)->SetNumberField(FDASaveJsonFields::Capital, 1.0);
        Root->SetStringField(FDASaveJsonFields::Checksum,
            CalculateFixtureChecksum(FDASaveSchema::CurrentSchemaVersion, Campaign->ToSharedRef()));
        TestTrue("Checksummed malformed empty state serializes", SerializeFixtureJson(Root.ToSharedRef(), SaveDocument));
        TestTrue("Checksummed malformed empty state is written",
            FFileHelper::SaveStringToFile(SaveDocument, *ActivePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestFalse("Malformed uninitialized authority is rejected after checksum validation", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            TestTrue("Malformed empty state is an invalid document", LoadResult.GetError().Code == EDASaveErrorCode::InvalidDocument);
        }
    });

    It("rejects checksummed tampering of Forgeweave trade repair and fortify transaction records", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("World exists", World);
        if (World == nullptr)
        {
            return;
        }
        TestTrue("World initializes", World->InitializeVerticalSliceState(TEXT("region.synara_frontier")));

        FDACampaignSnapshot TradeSetup = World->GetPersistentCampaign();
        TradeSetup.WorldState.Forgeweave.MaterialScarcity = 80.f;
        TestTrue("Trade setup restores", World->RestorePersistentCampaign(TradeSetup));
        TestTrue("Trade commits", World->AdvanceWorldTicks(1));

        FDACampaignSnapshot DefenseSetup = World->GetPersistentCampaign();
        DefenseSetup.WorldState.Forgeweave.MaterialScarcity = 20.f;
        DefenseSetup.WorldState.Forgeweave.DefensePressure = 80.f;
        TestTrue("Defense setup restores", World->RestorePersistentCampaign(DefenseSetup));
        TestTrue("Defense commits", World->AdvanceWorldTicks(1));

        FDACampaignSnapshot ConstructionSetup = World->GetPersistentCampaign();
        ConstructionSetup.WorldState.Forgeweave.DefensePressure = 0.f;
        TestTrue("Construction setup restores", World->RestorePersistentCampaign(ConstructionSetup));
        TestTrue("Two construction ticks commit", World->AdvanceWorldTicks(2));
        FDACampaignSnapshot RepairSetup = World->GetPersistentCampaign();
        const FDAForgeweaveBuildingState* Repairable = RepairSetup.WorldState.Forgeweave.Buildings.FindByPredicate(
            [&RepairSetup](const FDAForgeweaveBuildingState& Building)
            {
                return RepairSetup.OperationConflict.FindStructuralDamageRecord(Building.WorldAssetId) != nullptr;
            });
        TestNotNull("A modular building exists", Repairable);
        if (Repairable == nullptr)
        {
            return;
        }
        FDAWorldAssetRecord* Asset = RepairSetup.FindWorldAssetRecord(Repairable->WorldAssetId);
        FDAStructuralDamageRecord* Damage = RepairSetup.OperationConflict.FindStructuralDamageRecord(Repairable->WorldAssetId);
        if (Asset == nullptr || Damage == nullptr)
        {
            return;
        }
        Asset->StructuralIntegrity = 40.f;
        Asset->ConstructionState = EDAConstructionState::Damaged;
        Damage->Modules[0].CurrentHealth = 40.f;
        Damage->Modules[0].State = EDAStructureDamageState::Damaged;
        TestTrue("Repair setup restores", World->RestorePersistentCampaign(RepairSetup));
        TestTrue("Repair commits", World->AdvanceWorldTicks(1));

        const FDACampaignSnapshot Authority = World->GetPersistentCampaign();
        const int32 TradeIndex = Authority.WorldState.Forgeweave.ActionTransactions.IndexOfByPredicate(
            [](const FDAForgeweaveActionTransaction& Record) { return Record.Type == EDARivalDecisionType::Trade; });
        const int32 DefenseIndex = Authority.WorldState.Forgeweave.ActionTransactions.IndexOfByPredicate(
            [](const FDAForgeweaveActionTransaction& Record) { return Record.Type == EDARivalDecisionType::Fortify; });
        const int32 RepairIndex = Authority.WorldState.Forgeweave.ActionTransactions.IndexOfByPredicate(
            [](const FDAForgeweaveActionTransaction& Record) { return Record.Type == EDARivalDecisionType::Repair; });
        TestTrue("All three action transactions exist", TradeIndex != INDEX_NONE && DefenseIndex != INDEX_NONE && RepairIndex != INDEX_NONE);

        FDACampaignSnapshot TransformTamper = Authority;
        FDARegionState* TransformIronheart = TransformTamper.WorldState.FindRegion(TEXT("region.ironheart"));
        FDARegionActorState* ConstructionActor = TransformIronheart != nullptr
            ? TransformIronheart->PersistentDelta.LocalActors.FindByPredicate(
                [](const FDARegionActorState& Actor)
                {
                    return FDAForgeweaveCityState::IsVerticalSliceBuildCard(Actor.DefinitionId);
                })
            : nullptr;
        TestNotNull("A construction actor exists for full-transform tampering", ConstructionActor);
        if (ConstructionActor != nullptr)
        {
            ConstructionActor->Transform.SetScale3D(FVector(2.f, 1.f, 1.f));
            FString Error;
            TestFalse("Construction scale must equal the canonical grid transform", TransformTamper.Validate(Error));

            FDACampaignSnapshot RotationTamper = Authority;
            FDARegionState* RotationIronheart = RotationTamper.WorldState.FindRegion(TEXT("region.ironheart"));
            FDARegionActorState* RotationActor = RotationIronheart != nullptr
                ? RotationIronheart->PersistentDelta.LocalActors.FindByPredicate(
                    [ActorId = ConstructionActor->ActorId](const FDARegionActorState& Actor)
                    {
                        return Actor.ActorId == ActorId;
                    })
                : nullptr;
            if (RotationActor != nullptr)
            {
                RotationActor->Transform.SetRotation(FQuat(FRotator(0.f, 45.f, 0.f)));
                Error.Reset();
                TestFalse("Construction rotation must equal the canonical grid transform", RotationTamper.Validate(Error));
            }
        }

        FDACampaignSnapshot DefenseTransformTamper = Authority;
        FDARegionState* DefenseTransformIronheart = DefenseTransformTamper.WorldState.FindRegion(TEXT("region.ironheart"));
        FDARegionActorState* DefenseTransformActor = DefenseTransformIronheart != nullptr
            ? DefenseTransformIronheart->PersistentDelta.LocalActors.FindByPredicate(
                [](const FDARegionActorState& Actor)
                {
                    return Actor.DefinitionId == TEXT("forgeweave.defense_cover");
                })
            : nullptr;
        FDAForgeweaveActionTransaction* DefenseTransformTransaction = DefenseTransformTamper.WorldState.Forgeweave.ActionTransactions.FindByPredicate(
            [](const FDAForgeweaveActionTransaction& Transaction)
            {
                return Transaction.Type == EDARivalDecisionType::Fortify;
            });
        if (DefenseTransformActor != nullptr && DefenseTransformTransaction != nullptr)
        {
            const FTransform ShiftedDefense(
                FRotator(0.f, 45.f, 0.f),
                DefenseTransformActor->Transform.GetLocation() + FVector(100.f, 0.f, 0.f),
                FVector(2.f, 1.f, 1.f));
            DefenseTransformActor->Transform = ShiftedDefense;
            DefenseTransformTransaction->ActorTransform = ShiftedDefense;
            FString Error;
            TestFalse("Defense actor and transaction cannot jointly drift from the canonical Ironheart transform", DefenseTransformTamper.Validate(Error));
        }

        struct FTamperCase
        {
            FString Slot;
            int32 Index;
            const TCHAR* Field;
            TOptional<double> Number;
            TOptional<FString> String;
        };
        const FTamperCase Cases[] = {
            {TEXT("tamper-trade-transaction"), TradeIndex, TEXT("sourceQuantityAfter"), 999.0, {}},
            {TEXT("tamper-repair-transaction"), RepairIndex, TEXT("integrityAfter"), 99.0, {}},
            {TEXT("tamper-fortify-transaction"), DefenseIndex, TEXT("coverTypeId"), {}, FString(TEXT("cover.partial"))}
        };
        FDASaveService SaveService(TestSaveDirectory);
        for (const FTamperCase& Case : Cases)
        {
            TestTrue("Valid authority fixture saves", SaveService.SaveCampaign(Authority, Case.Slot).IsSuccess());
            TestTrue("Action record is tampered with a refreshed checksum", TamperActionTransactionField(
                TestSaveDirectory, Case.Slot, Case.Index, Case.Field, Case.Number, Case.String));
            const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(Case.Slot);
            TestFalse("Checksummed partial action mutation is rejected", Loaded.HasValue());
            if (!Loaded.HasValue())
            {
                TestEqual("Action tamper is an invalid campaign", Loaded.GetError().Code, EDASaveErrorCode::InvalidDocument);
            }
        }

        struct FInverseTamperCase
        {
            FString Slot;
            bool (*Mutate)(FJsonObject&);
        };
        const FInverseTamperCase InverseCases[] = {
            {TEXT("tamper-missing-action-transaction"), &RemoveLastActionTransactionJson},
            {TEXT("tamper-missing-construction-decision"), &RemoveFirstConstructionDecisionJson},
            {TEXT("tamper-orphan-construction-authority"), &AddOrphanConstructionAuthorityJson},
            {TEXT("tamper-unowned-renamed-spot-order"), &AddUnownedRenamedSpotOrderJson},
            {TEXT("tamper-defense-outside-ironheart"), &AddDefenseActorOutsideIronheartJson},
            {TEXT("tamper-arbitrary-definition-building-id"), &AddArbitraryDefinitionDuplicateBuildingIdJson},
            {TEXT("tamper-arbitrary-definition-defense-id"), &AddArbitraryDefinitionDuplicateDefenseIdJson}
        };
        for (const FInverseTamperCase& Case : InverseCases)
        {
            TestTrue("Valid inverse fixture saves", SaveService.SaveCampaign(Authority, Case.Slot).IsSuccess());
            TestTrue("Inverse authority is tampered with a refreshed checksum", RewriteChecksummedCampaign(
                TestSaveDirectory,
                Case.Slot,
                Case.Mutate));
            const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(Case.Slot);
            TestFalse("Orphan or missing inverse authority is rejected", Loaded.HasValue());
            if (!Loaded.HasValue())
            {
                TestEqual("Inverse tamper is an invalid campaign", Loaded.GetError().Code, EDASaveErrorCode::InvalidDocument);
            }
        }
    });

    It("rejects conflict-state tampering because it is inside the campaign checksum", [this]()
    {
        const FString Slot = TEXT("conflict-checksum");
        FDASaveService SaveService(TestSaveDirectory);
        FDACampaignSnapshot Snapshot;
        Snapshot.OperationConflict.Resources.Insight = 40.f;
        TestTrue("Conflict fixture saves", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());

        const FString ActivePath = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        FString SaveDocument;
        TSharedPtr<FJsonObject> Root;
        TestTrue("Conflict envelope is readable", FFileHelper::LoadFileToString(SaveDocument, *ActivePath));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        TestTrue("Conflict envelope is valid JSON", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        if (!Root.IsValid())
        {
            return;
        }

        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        const TSharedPtr<FJsonObject>* Conflict = nullptr;
        const TSharedPtr<FJsonObject>* Resources = nullptr;
        TestTrue("Campaign payload exists", Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) && Campaign != nullptr);
        TestTrue("Conflict payload exists", Campaign != nullptr && (*Campaign)->TryGetObjectField(FDASaveJsonFields::OperationConflict, Conflict) && Conflict != nullptr);
        TestTrue("Conflict resources exist", Conflict != nullptr && (*Conflict)->TryGetObjectField(FDASaveJsonFields::Resources, Resources) && Resources != nullptr);
        if (Resources == nullptr)
        {
            return;
        }

        (*Resources)->SetNumberField(FDASaveJsonFields::Insight, 999.0);
        TestTrue("Tampered envelope serializes without refreshing checksum", SerializeFixtureJson(Root.ToSharedRef(), SaveDocument));
        TestTrue("Tampered envelope overwrites active fixture", FFileHelper::SaveStringToFile(SaveDocument, *ActivePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestFalse("Tampered conflict payload is rejected", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            TestTrue("Conflict tampering is a checksum mismatch", LoadResult.GetError().Code == EDASaveErrorCode::ChecksumMismatch);
        }
    });

    It("rejects a checksummed load whose modular structural definition is not Core-eligible", [this]()
    {
        const FString Slot = TEXT("non-eligible-structural-load");
        FDASaveService SaveService(TestSaveDirectory);
        FDACampaignSnapshot Snapshot;
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(501, 502, 503, 504);
        Asset.CardDefinitionId = TEXT("synara.synthetic_fabrication_node");
        Asset.OwnerCivilizationId = TEXT("synara");
        Asset.ConstructionState = EDAConstructionState::Operational;
        Asset.StructuralIntegrity = 100.f;
        Snapshot.WorldAssets.Add(Asset);

        FDAStructuralDamageRecord Damage;
        Damage.WorldAssetId = Asset.WorldAssetId;
        Damage.CardDefinitionId = Asset.CardDefinitionId;
        Damage.Modules = { FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true) };
        Snapshot.OperationConflict.StructuralDamageRecords.Add(Damage);
        TestTrue("Eligible structural fixture saves", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());

        const FString ActivePath = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        FString SaveDocument;
        TSharedPtr<FJsonObject> Root;
        TestTrue("Structural envelope is readable", FFileHelper::LoadFileToString(SaveDocument, *ActivePath));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        TestTrue("Structural envelope is valid JSON", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        if (!Root.IsValid())
        {
            return;
        }

        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        const TSharedPtr<FJsonObject>* Conflict = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* WorldAssets = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* StructuralRecords = nullptr;
        TestTrue("Campaign payload exists", Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) && Campaign != nullptr);
        TestTrue("World assets exist", Campaign != nullptr && (*Campaign)->TryGetArrayField(FDASaveJsonFields::WorldAssets, WorldAssets) && WorldAssets != nullptr);
        TestTrue("Conflict payload exists", Campaign != nullptr && (*Campaign)->TryGetObjectField(FDASaveJsonFields::OperationConflict, Conflict) && Conflict != nullptr);
        TestTrue("Structural records exist", Conflict != nullptr && (*Conflict)->TryGetArrayField(FDASaveJsonFields::StructuralDamageRecords, StructuralRecords) && StructuralRecords != nullptr);
        if (Campaign == nullptr || WorldAssets == nullptr || WorldAssets->Num() != 1
            || StructuralRecords == nullptr || StructuralRecords->Num() != 1)
        {
            return;
        }

        (*WorldAssets)[0]->AsObject()->SetStringField(FDASaveJsonFields::CardDefinitionId, TEXT("synara.adaptive_habitat"));
        (*StructuralRecords)[0]->AsObject()->SetStringField(FDASaveJsonFields::CardDefinitionId, TEXT("synara.adaptive_habitat"));
        Root->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(FDASaveSchema::CurrentSchemaVersion, Campaign->ToSharedRef()));
        TestTrue("Checksummed non-eligible envelope serializes", SerializeFixtureJson(Root.ToSharedRef(), SaveDocument));
        TestTrue("Checksummed non-eligible envelope overwrites active fixture", FFileHelper::SaveStringToFile(SaveDocument, *ActivePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestFalse("Non-eligible modular damage is rejected after checksum validation", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            TestTrue("Non-eligible structural load is an invalid document", LoadResult.GetError().Code == EDASaveErrorCode::InvalidDocument);
        }
    });

    It("rejects a non-integral schema version before migration dispatch", [this]()
    {
        const FString Slot = TEXT("fractional-version");
        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Current fixture saves before fractional corruption", SaveService.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess());
        TestTrue("Checksummed fractional-version fixture is written", RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 1.5, false));

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(Slot);
        TestFalse("Fractional schema version is rejected", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            TestTrue("Fractional schema version is an unsupported schema", LoadResult.GetError().Code == EDASaveErrorCode::UnsupportedSchema);
        }
    });

    It("promotes only canonical empty schema-v9 Task20 defaults and rejects injected content", [this]()
    {
        const FString CleanSlot = TEXT("schema-nine-task20-clean");
        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Current empty fixture saves", SaveService.SaveCampaign(FDACampaignSnapshot(), CleanSlot).IsSuccess());
        TestTrue("Fixture becomes checksummed schema v9", RewriteActiveEnvelopeVersion(TestSaveDirectory, CleanSlot, 9.0, false));
        TestTrue("Canonical empty v9 fields promote", SaveService.LoadCampaign(CleanSlot).HasValue());

        const FString InjectedSlot = TEXT("schema-nine-task20-injected");
        TestTrue("Second empty fixture saves", SaveService.SaveCampaign(FDACampaignSnapshot(), InjectedSlot).IsSuccess());
        TestTrue("Second fixture becomes schema v9", RewriteActiveEnvelopeVersion(TestSaveDirectory, InjectedSlot, 9.0, false));
        const FString ActivePath = FPaths::Combine(TestSaveDirectory, InjectedSlot + TEXT(".dasave"));
        FString SaveDocument; TSharedPtr<FJsonObject> Root;
        TestTrue("Injected fixture reads", FFileHelper::LoadFileToString(SaveDocument, *ActivePath));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SaveDocument);
        TestTrue("Injected fixture parses", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        const TSharedPtr<FJsonObject>* Campaign = nullptr; const TSharedPtr<FJsonObject>* Narrative = nullptr;
        if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr
            || !(*Campaign)->TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative) || Narrative == nullptr) return;
        TArray<TSharedPtr<FJsonValue>> Injected;
        const TSharedRef<FJsonObject> FakeUnlock = MakeShared<FJsonObject>();
        FakeUnlock->SetStringField(TEXT("actionId"), TEXT("reward.injected")); Injected.Add(MakeShared<FJsonValueObject>(FakeUnlock));
        (*Narrative)->SetArrayField(TEXT("questContentUnlockRecords"), MoveTemp(Injected));
        Root->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(9.0, Campaign->ToSharedRef()));
        TestTrue("Injected fixture serializes", SerializeFixtureJson(Root.ToSharedRef(), SaveDocument));
        TestTrue("Injected fixture writes", FFileHelper::SaveStringToFile(SaveDocument, *ActivePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const TResult<FDACampaignSnapshot, FDASaveError> Rejected = SaveService.LoadCampaign(InjectedSlot);
        TestFalse("Injected v10 content is rejected", Rejected.HasValue());
        if (!Rejected.HasValue()) TestEqual("Injection fails migration", Rejected.GetError().Code, EDASaveErrorCode::MigrationFailed);

        const FString WrongShapeSlot = TEXT("schema-nine-task20-wrong-shape");
        TestTrue("Wrong-shape fixture saves", SaveService.SaveCampaign(FDACampaignSnapshot(), WrongShapeSlot).IsSuccess());
        TestTrue("Wrong-shape fixture becomes schema v9", RewriteActiveEnvelopeVersion(TestSaveDirectory, WrongShapeSlot, 9.0, false));
        const FString WrongShapePath = FPaths::Combine(TestSaveDirectory, WrongShapeSlot + TEXT(".dasave"));
        TestTrue("Wrong-shape fixture reads", FFileHelper::LoadFileToString(SaveDocument, *WrongShapePath));
        const TSharedRef<TJsonReader<>> WrongShapeReader = TJsonReaderFactory<>::Create(SaveDocument);
        TestTrue("Wrong-shape fixture parses", FJsonSerializer::Deserialize(WrongShapeReader, Root) && Root.IsValid());
        Campaign = nullptr; Narrative = nullptr;
        if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr
            || !(*Campaign)->TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative) || Narrative == nullptr) return;
        (*Narrative)->SetObjectField(TEXT("questContentUnlockRecords"), MakeShared<FJsonObject>());
        Root->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(9.0, Campaign->ToSharedRef()));
        TestTrue("Wrong-shape fixture serializes", SerializeFixtureJson(Root.ToSharedRef(), SaveDocument));
        TestTrue("Wrong-shape fixture writes", FFileHelper::SaveStringToFile(SaveDocument, *WrongShapePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const TResult<FDACampaignSnapshot, FDASaveError> WrongShapeRejected = SaveService.LoadCampaign(WrongShapeSlot);
        TestFalse("Wrong-shaped default is rejected", WrongShapeRejected.HasValue());
        if (!WrongShapeRejected.HasValue()) TestEqual("Wrong shape fails migration", WrongShapeRejected.GetError().Code, EDASaveErrorCode::MigrationFailed);
    });

    It("migrates a raw schema v8 nontrivial v8 campaign through the historical v9 boundary", [this]()
    {
        const FString Slot = TEXT("raw-v8-through-v11");
        FDASaveService SaveService(TestSaveDirectory);
        FDACampaignSnapshot Snapshot;
        Snapshot.HistoryTags = {TEXT("founder_hall_awake")};
        FDAQuestDefinition HistoricalQuest;
        HistoricalQuest.QuestId = TEXT("quest.test.v8_nontrivial");
        HistoricalQuest.SourceDefinitionId = TEXT("quest.test.v8_nontrivial.v1");
        HistoricalQuest.StartNodeId = TEXT("start");
        FDAQuestNodeDefinition Start;
        Start.NodeId = TEXT("start"); Start.Type = EDAQuestNodeType::Start;
        Start.SourceDefinitionId = TEXT("quest.test.v8_nontrivial.start");
        FDAQuestEdgeDefinition Edge; Edge.TargetNodeId = TEXT("resolution"); Start.Edges.Add(Edge);
        FDAQuestNodeDefinition Resolution;
        Resolution.NodeId = TEXT("resolution"); Resolution.Type = EDAQuestNodeType::Resolution;
        Resolution.SourceDefinitionId = TEXT("quest.test.v8_nontrivial.resolution");
        HistoricalQuest.Nodes = {Start, Resolution};
        TestEqual("Nontrivial quest starts", FDAQuestRuntime::StartQuest(HistoricalQuest, {}, 2, Snapshot),
            EDAQuestRuntimeResult::Applied);
        FDAQuestEvaluationContext Evaluation; Evaluation.WorldTick = 5; FName Branch;
        TestEqual("Nontrivial quest completes", FDAQuestRuntime::EvaluateCurrentNode(
            HistoricalQuest, Evaluation, Snapshot, Branch), EDAQuestRuntimeResult::Applied);
        TestTrue("Nontrivial current fixture saves", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());
        TestTrue("Fixture becomes authentic raw schema v8", RewriteActiveEnvelopeVersion(
            TestSaveDirectory, Slot, 8.0, false));
        // The raw v8 fixture carries the legacy v1 definition fingerprint that existed at that boundary.
        const FString ActivePath = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        FString RawDocument; TSharedPtr<FJsonObject> RawRoot;
        TestTrue("Raw v8 fixture reads", FFileHelper::LoadFileToString(RawDocument, *ActivePath));
        const TSharedRef<TJsonReader<>> RawReader = TJsonReaderFactory<>::Create(RawDocument);
        TestTrue("Raw v8 fixture parses", FJsonSerializer::Deserialize(RawReader, RawRoot) && RawRoot.IsValid());
        const TSharedPtr<FJsonObject>* RawCampaign = nullptr; const TSharedPtr<FJsonObject>* RawNarrative = nullptr;
        if (!RawRoot.IsValid() || !RawRoot->TryGetObjectField(FDASaveJsonFields::Campaign, RawCampaign)
            || RawCampaign == nullptr || !(*RawCampaign)->TryGetObjectField(FDASaveJsonFields::NarrativeState, RawNarrative)
            || RawNarrative == nullptr) return;
        const TArray<TSharedPtr<FJsonValue>>* RawQuests = nullptr;
        if (!(*RawNarrative)->TryGetArrayField(TEXT("questStates"), RawQuests) || RawQuests == nullptr || RawQuests->IsEmpty()) return;
        (*RawQuests)[0]->AsObject()->GetObjectField(TEXT("definitionManifest"))->SetStringField(
            TEXT("definitionFingerprint"), HistoricalQuest.BuildManifest().ComputeLegacyFingerprintV1());
        RawRoot->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(8.0, (*RawCampaign)->ToSharedRef()));
        TestTrue("Legacy fingerprint fixture serializes", SerializeFixtureJson(RawRoot.ToSharedRef(), RawDocument));
        TestTrue("Legacy fingerprint fixture writes", FFileHelper::SaveStringToFile(
            RawDocument, *ActivePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(Slot);
        TestTrue("Nontrivial v8 migrates through v9/v10 to schema v11 live signals", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            TestTrue("Nontrivial history survives multi-step migration",
                Loaded.GetValue().HistoryTags.Contains(TEXT("founder_hall_awake")));
            const FDAQuestSaveState* Quest = Loaded.GetValue().NarrativeState.FindQuestState(
                HistoricalQuest.QuestId);
            TestTrue("Nontrivial completed quest survives multi-step migration",
                Quest != nullptr && Quest->ProgressState == EDAQuestProgressState::Completed
                    && Quest->NodeTransitionRecords.Num() == 1
                    && Quest->NodeTransitionRecords[0].WorldTick == 5);
        }
    });

    It("migrates authentic schema v10 defaults and rejects injected schema v11 live signals", [this]()
    {
        FDASaveService SaveService(TestSaveDirectory);
        const FString ValidSlot = TEXT("schema-ten-to-eleven");
        TestTrue("Current fixture saves", SaveService.SaveCampaign(FDACampaignSnapshot(), ValidSlot).IsSuccess());
        TestTrue("Fixture becomes authentic v10 without schema-v11 fields",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, ValidSlot, 10.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(ValidSlot);
        TestTrue("Authentic v10 receives deterministic schema v11 live signals", Loaded.HasValue());
        if (Loaded.HasValue()) TestEqual("Migrated live revision starts at zero",
            Loaded.GetValue().LiveSignals.MutationRevision, int64(0));

        const FString InjectedSlot = TEXT("schema-ten-live-injected");
        TestTrue("Injected fixture saves", SaveService.SaveCampaign(FDACampaignSnapshot(), InjectedSlot).IsSuccess());
        TestTrue("Injected fixture becomes v10", RewriteActiveEnvelopeVersion(
            TestSaveDirectory, InjectedSlot, 10.0, false));
        const FString Path = FPaths::Combine(TestSaveDirectory, InjectedSlot + TEXT(".dasave"));
        FString Document; TSharedPtr<FJsonObject> Root;
        TestTrue("Injected v10 reads", FFileHelper::LoadFileToString(Document, *Path));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
        TestTrue("Injected v10 parses", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr) return;
        const TSharedRef<FJsonObject> InjectedLive = MakeShared<FJsonObject>();
        InjectedLive->SetNumberField(TEXT("population"), 1.0);
        (*Campaign)->SetObjectField(TEXT("liveSignals"), InjectedLive);
        Root->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(10.0, (*Campaign)->ToSharedRef()));
        TestTrue("Injected v10 serializes", SerializeFixtureJson(Root.ToSharedRef(), Document));
        TestTrue("Injected v10 writes", FFileHelper::SaveStringToFile(
            Document, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const TResult<FDACampaignSnapshot, FDASaveError> Rejected = SaveService.LoadCampaign(InjectedSlot);
        TestFalse("Populated future live signals are rejected", Rejected.HasValue());
        if (!Rejected.HasValue()) TestEqual("Future field rejection is a migration failure",
            Rejected.GetError().Code, EDASaveErrorCode::MigrationFailed);
    });

    It("migrates authentic schema v11 into persisted grid and regional crisis authorities", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        if (World == nullptr) return;
        FDASaveService SaveService(TestSaveDirectory);
        const FString Slot = TEXT("schema-eleven-to-twelve");
        TestTrue("Current initialized fixture saves", SaveService.SaveCampaign(
            World->GetPersistentCampaign(), Slot).IsSuccess());
        TestTrue("Fixture becomes authentic schema v11", RewriteActiveEnvelopeVersion(
            TestSaveDirectory, Slot, 11.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(Slot);
        TestTrue("Authentic v11 receives schema v12 regional authorities", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            TestNotNull("Player-capital claims are materialized",
                Loaded.GetValue().FindCityGridClaims(TEXT("player_capital")));
            TestFalse("Foundry Shortage begins inactive", Loaded.GetValue().RegionalCrisis.bTriggered);
            TestNotNull("Tal is migrated as a canonical citizen",
                Loaded.GetValue().LiveSignals.FindCitizen(TEXT("citizen.neutral.tal_arden")));
            TestNotNull("Mara is migrated as a canonical citizen",
                Loaded.GetValue().LiveSignals.FindCitizen(TEXT("citizen.forgeweave.mara_kest")));
            TestNotNull("Ori is migrated as a canonical citizen",
                Loaded.GetValue().LiveSignals.FindCitizen(TEXT("citizen.eden.ori_sen")));
        }
    });

    It("migrates schema v12 Foundry choices to the frozen graph fingerprint and shared action proof", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        if (World == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        TestTrue("Foundry migration fixture restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Foundry warning commits", World->AdvanceWorldTicks(1));
        const FGuid ActionId(12, 13, 22, 1);
        TestEqual("Player choice commits", World->ResolveFoundryShortage(
            ActionId, EDAFoundryShortageResolution::BrokeredCompact),
            EDAFoundryShortageActionResult::Applied);
        FDASaveService SaveService(TestSaveDirectory);
        const FString Slot = TEXT("schema-twelve-regional-graph");
        TestTrue("Current resolved fixture saves", SaveService.SaveCampaign(
            World->GetPersistentCampaign(), Slot).IsSuccess());
        TestTrue("Fixture envelope becomes schema v12", RewriteActiveEnvelopeVersion(
            TestSaveDirectory, Slot, 12.0, false));
        const FString Path = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        FString Document; TSharedPtr<FJsonObject> Root;
        TestTrue("Schema-v12 fixture reads", FFileHelper::LoadFileToString(Document, *Path));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
        TestTrue("Schema-v12 fixture parses", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        const TSharedPtr<FJsonObject>* CampaignJson = nullptr;
        const TSharedPtr<FJsonObject>* CrisisJson = nullptr;
        const TSharedPtr<FJsonObject>* NarrativeJson = nullptr;
        if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, CampaignJson)
            || CampaignJson == nullptr
            || !(*CampaignJson)->TryGetObjectField(TEXT("regionalCrisis"), CrisisJson) || CrisisJson == nullptr
            || !(*CampaignJson)->TryGetObjectField(FDASaveJsonFields::NarrativeState, NarrativeJson) || NarrativeJson == nullptr) return;
        constexpr const TCHAR* V12Fingerprint = TEXT("b7a7c0e15121b4db0a06514d0b06d37e7edf16bc");
        (*CrisisJson)->SetStringField(TEXT("manifestFingerprint"), V12Fingerprint);
        const TArray<TSharedPtr<FJsonValue>>* ResolutionRecords = nullptr;
        if ((*CrisisJson)->TryGetArrayField(TEXT("resolutionRecords"), ResolutionRecords) && ResolutionRecords != nullptr)
            for (const TSharedPtr<FJsonValue>& Value : *ResolutionRecords)
                if (Value.IsValid() && Value->Type == EJson::Object)
                    Value->AsObject()->SetStringField(TEXT("manifestFingerprint"), V12Fingerprint);
        (*NarrativeJson)->SetArrayField(TEXT("actionRecords"), TArray<TSharedPtr<FJsonValue>>());
        Root->SetStringField(FDASaveJsonFields::Checksum,
            CalculateFixtureChecksum(12.0, (*CampaignJson)->ToSharedRef()));
        TestTrue("Schema-v12 fixture serializes", SerializeFixtureJson(Root.ToSharedRef(), Document));
        TestTrue("Schema-v12 fixture writes", FFileHelper::SaveStringToFile(
            Document, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(Slot);
        TestTrue("Schema-v12 Foundry graph migrates", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            TestEqual("Crisis fingerprint migrates", Loaded.GetValue().RegionalCrisis.ManifestFingerprint,
                FString(TEXT("1bc31247330a8bc0af7103aaa8b70b51d8cd5d7a")));
            TestTrue("Shared narrative action proof is synthesized",
                Loaded.GetValue().NarrativeState.ActionRecords.ContainsByPredicate(
                    [ActionId](const FDANarrativeActionRecord& Record){ return Record.ActionId == ActionId; }));
        }
    });

    It("rejects every raw schema v8 and v9 liveSignals field before current DTO parsing", [this]()
    {
        FDASaveService SaveService(TestSaveDirectory);
        const auto WriteInjectedFixture = [this, &SaveService](const FString& Slot, const double Version,
            const TSharedPtr<FJsonValue>& LiveValue)
        {
            if (!SaveService.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess()
                || !RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, Version, false)) return false;
            const FString Path = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
            FString Document; TSharedPtr<FJsonObject> Root;
            if (!FFileHelper::LoadFileToString(Document, *Path)) return false;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
                || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr) return false;
            (*Campaign)->SetField(TEXT("liveSignals"), LiveValue);
            Root->SetStringField(FDASaveJsonFields::Checksum,
                CalculateFixtureChecksum(Version, (*Campaign)->ToSharedRef()));
            if (!SerializeFixtureJson(Root.ToSharedRef(), Document)) return false;
            return FFileHelper::SaveStringToFile(
                Document, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        };

        for (const int32 Version : {8, 9})
        {
            const TSharedRef<FJsonObject> Populated = MakeShared<FJsonObject>();
            Populated->SetNumberField(TEXT("population"), 17.0);
            const FString PopulatedSlot = FString::Printf(TEXT("schema-%d-live-populated"), Version);
            TestTrue("Populated future fixture writes", WriteInjectedFixture(
                PopulatedSlot, static_cast<double>(Version), MakeShared<FJsonValueObject>(Populated)));
            const TResult<FDACampaignSnapshot, FDASaveError> PopulatedResult = SaveService.LoadCampaign(PopulatedSlot);
            TestFalse("A populated future liveSignals object is rejected", PopulatedResult.HasValue());
            if (!PopulatedResult.HasValue()) TestEqual("Populated future field fails migration",
                PopulatedResult.GetError().Code, EDASaveErrorCode::MigrationFailed);

            const FString MalformedSlot = FString::Printf(TEXT("schema-%d-live-malformed"), Version);
            TestTrue("Malformed future fixture writes", WriteInjectedFixture(
                MalformedSlot, static_cast<double>(Version), MakeShared<FJsonValueString>(TEXT("malformed"))));
            const TResult<FDACampaignSnapshot, FDASaveError> MalformedResult = SaveService.LoadCampaign(MalformedSlot);
            TestFalse("A malformed future liveSignals value is rejected", MalformedResult.HasValue());
            if (!MalformedResult.HasValue()) TestEqual("Malformed future field fails migration",
                MalformedResult.GetError().Code, EDASaveErrorCode::MigrationFailed);
        }
    });

    It("migrates authentic schema v13 to deterministic empty conquest authority and rejects injected v14 state", [this]()
    {
        FDASaveService SaveService(TestSaveDirectory);
        const FString ValidSlot = TEXT("schema-thirteen-conquest");
        TestTrue("Current fixture saves", SaveService.SaveCampaign(FDACampaignSnapshot(), ValidSlot).IsSuccess());
        TestTrue("Fixture becomes authentic schema v13", RewriteActiveEnvelopeVersion(
            TestSaveDirectory, ValidSlot, 13.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = SaveService.LoadCampaign(ValidSlot);
        TestTrue("Authentic v13 receives deterministic conquest defaults", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            TestEqual("Military defaults to one hundred", Loaded.GetValue().ConquestState.MilitarySovereignty, 100.0);
            TestEqual("Economic defaults to one hundred", Loaded.GetValue().ConquestState.EconomicAutonomy, 100.0);
            TestEqual("Civic defaults to one hundred", Loaded.GetValue().ConquestState.CivicLegitimacy, 100.0);
            TestEqual("Alliance defaults to zero", Loaded.GetValue().ConquestState.AllianceReadiness, 0.0);
            TestEqual("Route history starts empty", Loaded.GetValue().ConquestState.RouteWeightHistory.Num(), 0);
        }

        const FString InjectedSlot = TEXT("schema-thirteen-injected-conquest");
        TestTrue("Injected fixture saves", SaveService.SaveCampaign(FDACampaignSnapshot(), InjectedSlot).IsSuccess());
        TestTrue("Injected fixture becomes v13", RewriteActiveEnvelopeVersion(
            TestSaveDirectory, InjectedSlot, 13.0, false));
        const FString Path = FPaths::Combine(TestSaveDirectory, InjectedSlot + TEXT(".dasave"));
        FString Document; TSharedPtr<FJsonObject> Root;
        TestTrue("Injected v13 reads", FFileHelper::LoadFileToString(Document, *Path));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
        TestTrue("Injected v13 parses", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr) return;
        const TSharedRef<FJsonObject> Injected = MakeShared<FJsonObject>();
        Injected->SetNumberField(TEXT("militarySovereignty"), 0.0);
        (*Campaign)->SetObjectField(FDASaveJsonFields::ConquestState, Injected);
        Root->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(13.0, (*Campaign)->ToSharedRef()));
        TestTrue("Injected v13 serializes", SerializeFixtureJson(Root.ToSharedRef(), Document));
        TestTrue("Injected v13 writes", FFileHelper::SaveStringToFile(
            Document, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const TResult<FDACampaignSnapshot, FDASaveError> Rejected = SaveService.LoadCampaign(InjectedSlot);
        TestFalse("Schema v13 cannot inject schema-v14 conquest authority", Rejected.HasValue());
        if (!Rejected.HasValue()) TestEqual("Future field is a migration failure",
            Rejected.GetError().Code, EDASaveErrorCode::MigrationFailed);
    });

    It("rejects checksummed current-schema meter tampering that breaks the route ledger replay", [this]()
    {
        FDASaveService SaveService(TestSaveDirectory);
        const FString Slot = TEXT("conquest-meter-tamper");
        TestTrue("Current fixture saves", SaveService.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess());
        const FString Path = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        FString Document; TSharedPtr<FJsonObject> Root;
        TestTrue("Current fixture reads", FFileHelper::LoadFileToString(Document, *Path));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
        TestTrue("Current fixture parses", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        const TSharedPtr<FJsonObject>* Conquest = nullptr;
        if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr
            || !(*Campaign)->TryGetObjectField(FDASaveJsonFields::ConquestState, Conquest) || Conquest == nullptr) return;
        (*Conquest)->SetNumberField(TEXT("economicAutonomy"), 0.0);
        Root->SetStringField(FDASaveJsonFields::Checksum,
            CalculateFixtureChecksum(FDASaveSchema::CurrentSchemaVersion, (*Campaign)->ToSharedRef()));
        TestTrue("Tampered fixture serializes", SerializeFixtureJson(Root.ToSharedRef(), Document));
        TestTrue("Tampered fixture writes", FFileHelper::SaveStringToFile(
            Document, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const TResult<FDACampaignSnapshot, FDASaveError> Rejected = SaveService.LoadCampaign(Slot);
        TestFalse("Recomputed envelope checksum cannot hide conquest ledger tampering", Rejected.HasValue());
        if (!Rejected.HasValue()) TestEqual("Semantic tamper is an invalid current document",
            Rejected.GetError().Code, EDASaveErrorCode::InvalidDocument);
    });

    It("rejects a replay-coherent conquest mutation with no canonical source authority", [this]()
    {
        FDACampaignSnapshot Snapshot;
        FDAConquestMeterMutation& Mutation = Snapshot.ConquestState.Mutations.Emplace_GetRef();
        Mutation.MutationId = TEXT("conquest.economic.contract.contract.fabricated");
        Mutation.Route = EDAForgeweaveRoute::Economic;
        Mutation.Meter = EDAConquestMeter::EconomicAutonomy;
        Mutation.SourceAuthority = TEXT("trade.contract");
        Mutation.SourceId = TEXT("contract.fabricated");
        Mutation.Delta = -15.0;
        Mutation.Result = 85.0;
        Snapshot.ConquestState.EconomicAutonomy = 85.0;
        Snapshot.ConquestState.EconomicWeight = 15.0;
        Snapshot.ConquestState.MutationRevision = 1;
        FDAConquestRouteWeightRecord& Weight = Snapshot.ConquestState.RouteWeightHistory.Emplace_GetRef();
        Weight.Revision = 1;
        Weight.Economic = 15.0;
        FString Error;
        TestTrue("Fabricated fixture is internally replay-coherent", Snapshot.ConquestState.Validate(Error));
        FDASaveService SaveService(TestSaveDirectory);
        const FDASaveResult Result = SaveService.SaveCampaign(Snapshot, TEXT("fabricated-conquest-source"));
        TestFalse("A route ledger cannot become an authority for a nonexistent contract", Result.IsSuccess());
        TestEqual("Fabricated source fails semantic snapshot validation", Result.Error.Code,
            EDASaveErrorCode::SerializationFailed);
    });

    It("rejects a checksummed replay-coherent duplicate consumption of one canonical conquest source", [this]()
    {
        FDACampaignSnapshot Snapshot;
        Snapshot.HistoryTags.Add(TEXT("forgeweave_elite_defeated"));
        FDAConquestMeterMutation& Mutation = Snapshot.ConquestState.Mutations.Emplace_GetRef();
        Mutation.MutationId = TEXT("conquest.force.elite_defeated");
        Mutation.Route = EDAForgeweaveRoute::Force;
        Mutation.Meter = EDAConquestMeter::MilitarySovereignty;
        Mutation.SourceAuthority = TEXT("campaign.history");
        Mutation.SourceId = TEXT("forgeweave_elite_defeated");
        Mutation.Delta = -20.0;
        Mutation.Result = 80.0;
        Snapshot.ConquestState.MilitarySovereignty = 80.0;
        Snapshot.ConquestState.ForceWeight = 20.0;
        Snapshot.ConquestState.MutationRevision = 1;
        FDAConquestRouteWeightRecord& Weight = Snapshot.ConquestState.RouteWeightHistory.Emplace_GetRef();
        Weight.Revision = 1;
        Weight.Force = 20.0;

        FDASaveService SaveService(TestSaveDirectory);
        const FString Slot = TEXT("duplicate-conquest-source");
        TestTrue("One canonical source consumption saves", SaveService.SaveCampaign(Snapshot, Slot).IsSuccess());
        const FString Path = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        FString Document; TSharedPtr<FJsonObject> Root;
        TestTrue("Canonical source fixture reads", FFileHelper::LoadFileToString(Document, *Path));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
        TestTrue("Canonical source fixture parses", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        const TSharedPtr<FJsonObject>* Conquest = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Mutations = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* History = nullptr;
        if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr
            || !(*Campaign)->TryGetObjectField(FDASaveJsonFields::ConquestState, Conquest) || Conquest == nullptr
            || !(*Conquest)->TryGetArrayField(TEXT("mutations"), Mutations) || Mutations == nullptr || Mutations->Num() != 1
            || !(*Conquest)->TryGetArrayField(TEXT("routeWeightHistory"), History) || History == nullptr || History->Num() != 1) return;

        const TSharedRef<FJsonObject> DuplicateMutation = MakeShared<FJsonObject>();
        DuplicateMutation->Values = (*Mutations)[0]->AsObject()->Values;
        DuplicateMutation->SetStringField(TEXT("mutationId"), TEXT("conquest.force.elite_defeated.duplicate"));
        DuplicateMutation->SetNumberField(TEXT("result"), 60.0);
        TArray<TSharedPtr<FJsonValue>> ForgedMutations = *Mutations;
        ForgedMutations.Add(MakeShared<FJsonValueObject>(DuplicateMutation));
        (*Conquest)->SetArrayField(TEXT("mutations"), ForgedMutations);

        const TSharedRef<FJsonObject> DuplicateWeight = MakeShared<FJsonObject>();
        DuplicateWeight->Values = (*History)[0]->AsObject()->Values;
        DuplicateWeight->SetNumberField(TEXT("revision"), 2.0);
        DuplicateWeight->SetNumberField(TEXT("force"), 40.0);
        TArray<TSharedPtr<FJsonValue>> ForgedHistory = *History;
        ForgedHistory.Add(MakeShared<FJsonValueObject>(DuplicateWeight));
        (*Conquest)->SetArrayField(TEXT("routeWeightHistory"), ForgedHistory);
        (*Conquest)->SetNumberField(TEXT("militarySovereignty"), 60.0);
        (*Conquest)->SetNumberField(TEXT("forceWeight"), 40.0);
        (*Conquest)->SetNumberField(TEXT("mutationRevision"), 2.0);
        Root->SetStringField(FDASaveJsonFields::Checksum,
            CalculateFixtureChecksum(FDASaveSchema::CurrentSchemaVersion, (*Campaign)->ToSharedRef()));
        TestTrue("Duplicate-source fixture serializes with a recomputed checksum",
            SerializeFixtureJson(Root.ToSharedRef(), Document));
        TestTrue("Duplicate-source fixture overwrites canonical save", FFileHelper::SaveStringToFile(
            Document, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

        const TResult<FDACampaignSnapshot, FDASaveError> Rejected = SaveService.LoadCampaign(Slot);
        TestFalse("One canonical source cannot be consumed twice", Rejected.HasValue());
        if (!Rejected.HasValue()) TestEqual("Duplicate source is semantic document tampering",
            Rejected.GetError().Code, EDASaveErrorCode::InvalidDocument);
    });

    It("rejects checksummed route-tag-only Forgeweave resolutions for every route", [this]()
    {
        static const EDAForgeweaveRoute Routes[] = {EDAForgeweaveRoute::Force,
            EDAForgeweaveRoute::Economic, EDAForgeweaveRoute::Influence, EDAForgeweaveRoute::Alliance};
        static const FName RouteTags[] = {TEXT("forgeweave_forced"), TEXT("forgeweave_economic_union"),
            TEXT("forgeweave_influence_transfer"), TEXT("forgeweave_allied")};
        FDASaveService SaveService(TestSaveDirectory);
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Routes); ++Index)
        {
            const FString Slot = FString::Printf(TEXT("forged-conquest-resolution-%d"), Index);
            TestTrue("Unresolved route fixture saves", SaveService.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess());
            const FString Path = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
            FString Document; TSharedPtr<FJsonObject> Root;
            TestTrue("Unresolved route fixture reads", FFileHelper::LoadFileToString(Document, *Path));
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
            TestTrue("Unresolved route fixture parses", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr) return;

            FDAConquestCampaignState ForgedState;
            ForgedState.bForgeweaveResolved = true;
            ForgedState.ResolvedRoute = Routes[Index];
            ForgedState.ResolutionActionId = FGuid(23, 77, Index + 1, 1);
            const TSharedRef<FJsonObject> ForgedConquest = MakeShared<FJsonObject>();
            TestTrue("Forged conquest state converts to persisted JSON", FJsonObjectConverter::UStructToJsonObject(
                FDAConquestCampaignState::StaticStruct(), &ForgedState, ForgedConquest, 0, 0));
            (*Campaign)->SetObjectField(FDASaveJsonFields::ConquestState, ForgedConquest);
            TArray<TSharedPtr<FJsonValue>> ForgedHistory;
            ForgedHistory.Add(MakeShared<FJsonValueString>(RouteTags[Index].ToString()));
            (*Campaign)->SetArrayField(FDASaveJsonFields::HistoryTags, ForgedHistory);
            Root->SetStringField(FDASaveJsonFields::Checksum,
                CalculateFixtureChecksum(FDASaveSchema::CurrentSchemaVersion, (*Campaign)->ToSharedRef()));
            TestTrue("Forged route serializes with a recomputed checksum",
                SerializeFixtureJson(Root.ToSharedRef(), Document));
            TestTrue("Forged route overwrites canonical save", FFileHelper::SaveStringToFile(
                Document, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

            const TResult<FDACampaignSnapshot, FDASaveError> Rejected = SaveService.LoadCampaign(Slot);
            TestFalse("Route history cannot replace canonical completion gates", Rejected.HasValue());
            if (!Rejected.HasValue()) TestEqual("Forged resolution is semantic document tampering",
                Rejected.GetError().Code, EDASaveErrorCode::InvalidDocument);
        }
    });

    It("persists both same-tick crisis orders and rejects a checksummed forged causal proof", [this]()
    {
        FDAGameInstanceSubsystemFixture HistoryFixture;
        UDAWorldStateSubsystem* HistoryWorld = HistoryFixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("History-first production owner exists", HistoryWorld);
        if (HistoryWorld == nullptr) return;
        FDACampaignSnapshot HistoryCandidate = HistoryWorld->GetPersistentCampaign();
        HistoryCandidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        HistoryCandidate.HistoryTags.AddUnique(TEXT("joint_forgeweave_crisis_success"));
        HistoryCandidate.HistoryTags.Sort(
            [](const FName Left, const FName Right){ return Left.LexicalLess(Right); });
        TestTrue("History-first fixture restores", HistoryWorld->RestorePersistentCampaign(HistoryCandidate));
        TestTrue("History-first owner tick commits", HistoryWorld->AdvanceWorldTicks(1));
        TestEqual("History-first same-tick crisis resolves", HistoryWorld->ResolveFoundryShortage(
            FGuid(23, 4, 2, 1), EDAFoundryShortageResolution::IndustrialSupport),
            EDAFoundryShortageActionResult::Applied);

        FDASaveService Saves(TestSaveDirectory);
        const FString HistorySlot = TEXT("same-tick-history-first-causal-proof");
        TestTrue("History-first proof saves", Saves.SaveCampaign(
            HistoryWorld->GetPersistentCampaign(), HistorySlot).IsSuccess());
        const FString HistoryPath = FPaths::Combine(TestSaveDirectory, HistorySlot + TEXT(".dasave"));
        FString Document;
        TSharedPtr<FJsonObject> Root;
        TestTrue("History-first save reads", FFileHelper::LoadFileToString(Document, *HistoryPath));
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
        TestTrue("History-first save parses", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        const TSharedPtr<FJsonObject>* Crisis = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
        if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
            || Campaign == nullptr || !(*Campaign)->TryGetObjectField(TEXT("regionalCrisis"), Crisis)
            || Crisis == nullptr || !(*Crisis)->TryGetArrayField(TEXT("resolutionRecords"), Records)
            || Records == nullptr || Records->Num() != 1) return;
        (*Records)[0]->AsObject()->SetNumberField(
            TEXT("jointCrisisHistoryRevisionAtResolution"), 0.0);
        Root->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(
            FDASaveSchema::CurrentSchemaVersion, (*Campaign)->ToSharedRef()));
        TestTrue("Forged causal proof serializes with recomputed checksum",
            SerializeFixtureJson(Root.ToSharedRef(), Document));
        TestTrue("Forged causal proof overwrites the save", FFileHelper::SaveStringToFile(
            Document, *HistoryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const TResult<FDACampaignSnapshot, FDASaveError> Tampered = Saves.LoadCampaign(HistorySlot);
        TestFalse("A recomputed checksum cannot reorder same-tick evidence", Tampered.HasValue());
        if (!Tampered.HasValue()) TestEqual("Forged causal proof is invalid current data",
            Tampered.GetError().Code, EDASaveErrorCode::InvalidDocument);

        FDAGameInstanceSubsystemFixture CrisisFixture;
        UDAWorldStateSubsystem* CrisisWorld = CrisisFixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Crisis-first production owner exists", CrisisWorld);
        if (CrisisWorld == nullptr) return;
        FDACampaignSnapshot CrisisCandidate = CrisisWorld->GetPersistentCampaign();
        CrisisCandidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        TestTrue("Crisis-first fixture restores", CrisisWorld->RestorePersistentCampaign(CrisisCandidate));
        TestTrue("Crisis-first owner tick commits", CrisisWorld->AdvanceWorldTicks(1));
        TestEqual("Crisis-first same-tick crisis resolves", CrisisWorld->ResolveFoundryShortage(
            FGuid(23, 4, 3, 1), EDAFoundryShortageResolution::IndustrialSupport),
            EDAFoundryShortageActionResult::Applied);
        CrisisCandidate = CrisisWorld->GetPersistentCampaign();
        CrisisCandidate.HistoryTags.AddUnique(TEXT("joint_forgeweave_crisis_success"));
        CrisisCandidate.HistoryTags.Sort(
            [](const FName Left, const FName Right){ return Left.LexicalLess(Right); });
        FString Error;
        TestTrue("Crisis-first history synchronizes in the same tick",
            FDAConquestSystem::Synchronize(CrisisCandidate, Error));
        TestTrue("Crisis-first same-tick campaign fully validates", CrisisCandidate.Validate(Error));
        TestTrue("Crisis-first same-tick campaign restores", CrisisWorld->RestorePersistentCampaign(CrisisCandidate));

        const FString AuthenticSlot = TEXT("authentic-v14-same-tick-crisis-first");
        TestTrue("Crisis-first current proof saves", Saves.SaveCampaign(
            CrisisWorld->GetPersistentCampaign(), AuthenticSlot).IsSuccess());
        const FString AuthenticPath = FPaths::Combine(TestSaveDirectory, AuthenticSlot + TEXT(".dasave"));
        Root.Reset(); Document.Reset();
        TestTrue("Crisis-first current save reads", FFileHelper::LoadFileToString(Document, *AuthenticPath));
        const TSharedRef<TJsonReader<>> AuthenticReader = TJsonReaderFactory<>::Create(Document);
        TestTrue("Crisis-first current save parses",
            FJsonSerializer::Deserialize(AuthenticReader, Root) && Root.IsValid());
        Campaign = nullptr; Crisis = nullptr; Records = nullptr;
        if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
            || Campaign == nullptr || !(*Campaign)->TryGetObjectField(TEXT("regionalCrisis"), Crisis)
            || Crisis == nullptr || !(*Crisis)->TryGetArrayField(TEXT("resolutionRecords"), Records)
            || Records == nullptr || Records->Num() != 1) return;
        (*Records)[0]->AsObject()->RemoveField(TEXT("jointCrisisHistoryRevisionAtResolution"));
        (*Campaign)->RemoveField(FDASaveJsonFields::DaxtonState);
        (*Campaign)->RemoveField(FDASaveJsonFields::CitySimulationState);
        Root->RemoveField(FDASaveJsonFields::ContentVersion);
        Root->RemoveField(FDASaveJsonFields::BuildVersion);
        Root->SetNumberField(FDASaveJsonFields::SchemaVersion, 14.0);
        Root->SetStringField(FDASaveJsonFields::Checksum,
            CalculateFixtureChecksum(14.0, (*Campaign)->ToSharedRef()));
        TestTrue("Authentic schema-v14 causal predecessor serializes",
            SerializeFixtureJson(Root.ToSharedRef(), Document));
        TestTrue("Authentic schema-v14 causal predecessor writes", FFileHelper::SaveStringToFile(
            Document, *AuthenticPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const TResult<FDACampaignSnapshot, FDASaveError> Migrated = Saves.LoadCampaign(AuthenticSlot);
        TestTrue("Authentic schema-v14 crisis-first order migrates", Migrated.HasValue());
        if (Migrated.HasValue()) TestEqual("Migration preserves saturated readiness",
            Migrated.GetValue().ConquestState.AllianceReadiness, 25.0);

        const FString InjectedSlot = TEXT("injected-v14-same-tick-causal-proof");
        TestTrue("Current injection source saves", Saves.SaveCampaign(
            CrisisWorld->GetPersistentCampaign(), InjectedSlot).IsSuccess());
        TestTrue("Current injection source becomes raw schema v14",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, InjectedSlot, 14.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Injected = Saves.LoadCampaign(InjectedSlot);
        TestFalse("Schema v14 cannot inject the schema-v15 causal field", Injected.HasValue());
        if (!Injected.HasValue()) TestEqual("Future causal field is a migration failure",
            Injected.GetError().Code, EDASaveErrorCode::MigrationFailed);
    });

    It("requires an exact causal proof field on every checksummed current-v15 crisis resolution", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production campaign owner exists", World);
        if (World == nullptr) return;

        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        TestTrue("Causal-proof fixture restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Causal-proof fixture advances to its resolution tick", World->AdvanceWorldTicks(1));
        TestEqual("Causal-proof fixture resolves", World->ResolveFoundryShortage(
            FGuid(23, 5, 1, 1), EDAFoundryShortageResolution::IndustrialSupport),
            EDAFoundryShortageActionResult::Applied);

        enum class ECausalProofTamper : uint8
        {
            Missing,
            Fractional,
            WrongType
        };
        struct FCausalProofTamperCase
        {
            const TCHAR* Slot;
            ECausalProofTamper Tamper;
        };
        const FCausalProofTamperCase Cases[] = {
            {TEXT("current-v15-missing-causal-proof"), ECausalProofTamper::Missing},
            {TEXT("current-v15-fractional-causal-proof"), ECausalProofTamper::Fractional},
            {TEXT("current-v15-wrong-type-causal-proof"), ECausalProofTamper::WrongType}
        };

        FDASaveService Saves(TestSaveDirectory);
        for (const FCausalProofTamperCase& Case : Cases)
        {
            TestTrue("Canonical current-v15 source saves", Saves.SaveCampaign(
                World->GetPersistentCampaign(), Case.Slot).IsSuccess());
            const FString Path = FPaths::Combine(TestSaveDirectory, FString(Case.Slot) + TEXT(".dasave"));
            FString Document;
            TSharedPtr<FJsonObject> Root;
            TestTrue("Current-v15 source reads", FFileHelper::LoadFileToString(Document, *Path));
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
            TestTrue("Current-v15 source parses", FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            const TSharedPtr<FJsonObject>* Crisis = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
            if (!Root.IsValid() || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !(*Campaign)->TryGetObjectField(TEXT("regionalCrisis"), Crisis)
                || Crisis == nullptr || !(*Crisis)->TryGetArrayField(TEXT("resolutionRecords"), Records)
                || Records == nullptr || Records->Num() != 1 || !(*Records)[0].IsValid()
                || (*Records)[0]->Type != EJson::Object) return;

            const TSharedPtr<FJsonObject> Record = (*Records)[0]->AsObject();
            switch (Case.Tamper)
            {
            case ECausalProofTamper::Missing:
                Record->RemoveField(TEXT("jointCrisisHistoryRevisionAtResolution"));
                break;
            case ECausalProofTamper::Fractional:
                Record->SetNumberField(TEXT("jointCrisisHistoryRevisionAtResolution"), 0.5);
                break;
            case ECausalProofTamper::WrongType:
                Record->SetStringField(TEXT("jointCrisisHistoryRevisionAtResolution"), TEXT("0"));
                break;
            }
            Root->SetStringField(FDASaveJsonFields::Checksum, CalculateFixtureChecksum(
                FDASaveSchema::CurrentSchemaVersion, (*Campaign)->ToSharedRef()));
            TestTrue("Tampered current-v15 source serializes with a recomputed checksum",
                SerializeFixtureJson(Root.ToSharedRef(), Document));
            TestTrue("Tampered current-v15 source overwrites the save", FFileHelper::SaveStringToFile(
                Document, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

            const TResult<FDACampaignSnapshot, FDASaveError> Rejected = Saves.LoadCampaign(Case.Slot);
            TestFalse("Current-v15 resolution cannot omit or coerce its causal proof", Rejected.HasValue());
            if (!Rejected.HasValue()) TestEqual("Malformed causal proof is invalid current data",
                Rejected.GetError().Code, EDASaveErrorCode::InvalidDocument);
        }
    });

    It("keeps an immutable coalition worker mutation valid after stronger worker evidence arrives", [this]()
    {
        FDACampaignSnapshot Snapshot;
        AddValidCompletedConquestQuest(Snapshot, TEXT("quest.workers_signal"));
        Snapshot.HistoryTags = {TEXT("mara_numbers_worker_coalition"), TEXT("workers_protected")};
        FDAConquestMeterMutation& Mutation = Snapshot.ConquestState.Mutations.Emplace_GetRef();
        Mutation.MutationId = TEXT("conquest.influence.worker_endorsement");
        Mutation.Route = EDAForgeweaveRoute::Influence;
        Mutation.Meter = EDAConquestMeter::CivicLegitimacy;
        Mutation.SourceAuthority = TEXT("campaign.history");
        Mutation.SourceId = TEXT("mara_numbers_worker_coalition");
        Mutation.Delta = -20.0;
        Mutation.Result = 80.0;
        Snapshot.ConquestState.CivicLegitimacy = 80.0;
        Snapshot.ConquestState.InfluenceWeight = 20.0;
        Snapshot.ConquestState.MutationRevision = 1;
        FDAConquestRouteWeightRecord& Weight = Snapshot.ConquestState.RouteWeightHistory.Emplace_GetRef();
        Weight.Revision = 1;
        Weight.Influence = 20.0;

        FDASaveService SaveService(TestSaveDirectory);
        const FDASaveResult Result = SaveService.SaveCampaign(
            Snapshot, TEXT("immutable-worker-evidence"));
        TestTrue("Later protected-worker evidence does not rewrite valid coalition history", Result.IsSuccess());

        FDACampaignSnapshot Fabricated = Snapshot;
        Fabricated.ConquestState.Mutations[0].SourceId = TEXT("worker_evidence.fabricated");
        TestFalse("A fabricated worker source remains rejected", SaveService.SaveCampaign(
            Fabricated, TEXT("fabricated-worker-evidence")).IsSuccess());
    });

    It("rejects recomputed-checksum Unicode-escaped duplicate decoded keys and containers", [this]()
    {
        struct FTamperCase
        {
            const TCHAR* Slot;
            const TCHAR* Literal;
            const TCHAR* Replacement;
        };
        const FTamperCase Cases[] = {
            {TEXT("escaped-duplicate-conquest-key"), TEXT("\"conquestState\":"),
                TEXT("\"conquestState\":{},\"conquestStat\\u0065\":" )},
            {TEXT("escaped-duplicate-crisis-container"), TEXT("\"regionalCrisis\":"),
                TEXT("\"regionalCrisis\":{},\"regionalCrisi\\u0073\":" )},
            {TEXT("escaped-duplicate-resolution-container"), TEXT("\"resolutionRecords\":[]"),
                TEXT("\"resolutionRecords\":[{}],\"resolutionRecord\\u0073\":[]")}
        };

        FDASaveService Saves(TestSaveDirectory);
        for (const FTamperCase& Case : Cases)
        {
            TestTrue("Canonical duplicate-key source saves",
                Saves.SaveCampaign(FDACampaignSnapshot(), Case.Slot).IsSuccess());
            const FString Path = FPaths::Combine(
                TestSaveDirectory, FString(Case.Slot) + TEXT(".dasave"));
            FString Document;
            TestTrue("Duplicate-key source reads", FFileHelper::LoadFileToString(Document, *Path));
            TestTrue("Literal source property exists", Document.Contains(Case.Literal));
            Document = Document.Replace(Case.Literal, Case.Replacement, ESearchCase::CaseSensitive);
            TestTrue("Effective last-wins payload has a recomputed checksum",
                RecomputeRawEnvelopeChecksum(Document));
            TestTrue("Escaped duplicate writes", FFileHelper::SaveStringToFile(
                Document, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

            const TResult<FDACampaignSnapshot, FDASaveError> Rejected = Saves.LoadCampaign(Case.Slot);
            TestFalse("Decoded duplicate object member is rejected before last-wins parsing",
                Rejected.HasValue());
            if (!Rejected.HasValue()) TestEqual("Decoded duplicate is invalid JSON authority",
                Rejected.GetError().Code, EDASaveErrorCode::InvalidDocument);
        }
    });

    It("rejects a recomputed-checksum escaped duplicate current-schema causal proof key", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production campaign owner exists", World);
        if (World == nullptr) return;
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        TestTrue("Crisis campaign restores", World->RestorePersistentCampaign(Candidate));
        TestTrue("Crisis reaches resolution tick", World->AdvanceWorldTicks(1));
        TestEqual("Crisis resolution applies", World->ResolveFoundryShortage(
            FGuid(24, 15, 1, 1), EDAFoundryShortageResolution::IndustrialSupport),
            EDAFoundryShortageActionResult::Applied);

        const FString Slot = TEXT("escaped-duplicate-v15-causal-proof");
        FDASaveService Saves(TestSaveDirectory);
        TestTrue("Canonical proof source saves",
            Saves.SaveCampaign(World->GetPersistentCampaign(), Slot).IsSuccess());
        const FString Path = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        FString Document;
        TestTrue("Proof source reads", FFileHelper::LoadFileToString(Document, *Path));
        const FString Literal = TEXT("\"jointCrisisHistoryRevisionAtResolution\":");
        const int32 PropertyIndex = Document.Find(Literal, ESearchCase::CaseSensitive);
        TestTrue("Canonical causal key exists", PropertyIndex != INDEX_NONE);
        if (PropertyIndex == INDEX_NONE) return;
        const int32 ValueStart = PropertyIndex + Literal.Len();
        int32 ValueEnd = ValueStart;
        while (ValueEnd < Document.Len()
            && Document[ValueEnd] != TEXT(',') && Document[ValueEnd] != TEXT('}')) ++ValueEnd;
        const FString CanonicalValue = Document.Mid(ValueStart, ValueEnd - ValueStart);
        const FString Duplicate = Literal + TEXT("9007199254740992,")
            + TEXT("\"jointCrisisHistoryRevisionAtResolutio\\u006e\":") + CanonicalValue;
        Document.RemoveAt(PropertyIndex, ValueEnd - PropertyIndex, EAllowShrinking::No);
        Document.InsertAt(PropertyIndex, Duplicate);
        TestTrue("Effective causal payload checksum is recomputed", RecomputeRawEnvelopeChecksum(Document));
        TestTrue("Escaped proof duplicate writes", FFileHelper::SaveStringToFile(
            Document, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const TResult<FDACampaignSnapshot, FDASaveError> Rejected = Saves.LoadCampaign(Slot);
        TestFalse("Decoded causal proof duplicate is rejected", Rejected.HasValue());
        if (!Rejected.HasValue()) TestEqual("Decoded causal duplicate is invalid current data",
            Rejected.GetError().Code, EDASaveErrorCode::InvalidDocument);
    });

    It("migrates authentic schema-v15 Daxton defaults and rejects injected future Leader authority", [this]()
    {
        FDASaveService Saves(TestSaveDirectory);
        const FString AuthenticSlot = TEXT("authentic-v15-daxton-default");
        TestTrue("Current default saves", Saves.SaveCampaign(
            FDACampaignSnapshot(), AuthenticSlot).IsSuccess());
        TestTrue("Current default becomes authentic schema v15",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, AuthenticSlot, 15.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Migrated = Saves.LoadCampaign(AuthenticSlot);
        TestTrue("Authentic schema-v15 default migrates", Migrated.HasValue());
        if (Migrated.HasValue())
        {
            TestEqual("Daxton remains unresolved after migration",
                Migrated.GetValue().DaxtonState.Phase, EDADaxtonEncounterPhase::Inactive);
            TestFalse("No Leader outcome is invented", Migrated.GetValue().DaxtonState.bLeaderResolved);
        }

        const FString InjectedSlot = TEXT("injected-v15-daxton-state");
        TestTrue("Second current default saves", Saves.SaveCampaign(
            FDACampaignSnapshot(), InjectedSlot).IsSuccess());
        TestTrue("Second default becomes schema v15",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, InjectedSlot, 15.0, false));
        TestTrue("Future Daxton authority is injected with matching checksum",
            RewriteChecksummedCampaign(TestSaveDirectory, InjectedSlot,
                [](FJsonObject& Campaign)
                {
                    const TSharedRef<FJsonObject> Daxton = MakeShared<FJsonObject>();
                    Daxton->SetStringField(TEXT("phase"), TEXT("Resolved"));
                    Campaign.SetObjectField(TEXT("daxtonState"), Daxton);
                    return true;
                }));
        const TResult<FDACampaignSnapshot, FDASaveError> Injected = Saves.LoadCampaign(InjectedSlot);
        TestFalse("Schema v15 cannot inject schema-v16 Leader authority", Injected.HasValue());
        if (!Injected.HasValue()) TestEqual("Future Leader authority is a migration failure",
            Injected.GetError().Code, EDASaveErrorCode::MigrationFailed);
    });

    It("migrates a resolved schema-v16 same-tick relationship suffix and preserves later owner evolution", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production world owner exists", World);
        if (World == nullptr) return;
        TestTrue("Alliance resolves through the production owner",
            ResolveDaxtonMigrationAlliance(*World));
        const int64 ResolutionTick =
            World->GetPersistentCampaign().DaxtonState.ResolvedWorldTick;
        TestEqual("Resolution uses the current World Tick", ResolutionTick,
            World->GetCurrentWorldTick());
        TestTrue("Canonical unrelated reason appends later in the same World Tick",
            CommitSameTickDaxtonReason(*World,
                TEXT("reason.post_daxton.v16.same_tick"), -30.f));

        const FDACampaignSnapshot& SameTick = World->GetPersistentCampaign();
        FString ValidationError;
        TestTrue("Current schema accepts the ordered same-tick suffix",
            SameTick.Validate(ValidationError));
        TestEqual("Resolution prefix remains exactly two reasons",
            SameTick.DaxtonState.ResolutionRelationshipReasonCount, 2);
        const FDADiplomaticRelationship* SameTickRelationship =
            SameTick.WorldState.Diplomacy.FindRelationship(
                TEXT("relationship.synara.forgeweave"));
        TestTrue("Same-tick suffix follows the protected ordered prefix",
            SameTickRelationship != nullptr
                && SameTickRelationship->ReasonLedger.Num() == 3
                && SameTickRelationship->ReasonLedger[2].MutationId
                    == TEXT("reason.post_daxton.v16.same_tick")
                && SameTickRelationship->ReasonLedger[2].WorldTick == ResolutionTick);

        FDASaveService Saves(TestSaveDirectory);
        const FString Slot = TEXT("authentic-v16-resolved-same-tick-suffix");
        TestTrue("Resolved same-tick workflow saves as current schema",
            Saves.SaveCampaign(SameTick, Slot).IsSuccess());
        TestTrue("Resolved workflow becomes an authentic schema v16 document",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 16.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Migrated = Saves.LoadCampaign(Slot);
        TestTrue("Resolved same-tick schema v16 migrates", Migrated.HasValue());
        if (!Migrated.HasValue()) return;

        const FDACampaignSnapshot& Loaded = Migrated.GetValue();
        TestEqual("Migration derives the unique two-reason terminal prefix",
            Loaded.DaxtonState.ResolutionRelationshipReasonCount, 2);
        TestTrue("Migration preserves exact prefix identities in canonical order",
            Loaded.DaxtonState.ResolutionRelationshipReasonMutationIds.Num() == 2
                && Loaded.DaxtonState.ResolutionRelationshipReasonMutationIds[0]
                    == TEXT("reason.daxton.v16.trust")
                && Loaded.DaxtonState.ResolutionRelationshipReasonMutationIds[1]
                    == TEXT("reason.daxton.v16.respect"));
        const FDADiplomaticRelationship* LoadedRelationship =
            Loaded.WorldState.Diplomacy.FindRelationship(
                TEXT("relationship.synara.forgeweave"));
        TestTrue("Migration preserves the same-tick suffix after the prefix",
            LoadedRelationship != nullptr && LoadedRelationship->ReasonLedger.Num() == 3
                && LoadedRelationship->ReasonLedger[2].MutationId
                    == TEXT("reason.post_daxton.v16.same_tick"));
        ValidationError.Reset();
        TestTrue("Migrated same-tick campaign fully validates", Loaded.Validate(ValidationError));
        TestTrue("Migrated campaign restores to the production owner",
            World->RestorePersistentCampaign(Loaded));
        TestTrue("Production owner evolves beyond the migrated resolution tick",
            World->AdvanceWorldTicks(1));
        TestEqual("Further owner evolution advances one World Tick",
            World->GetCurrentWorldTick(), ResolutionTick + 1);
    });

    It("rejects schema-v16 Daxton prefix migration with no matching terminal projection", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production world owner exists", World);
        if (World == nullptr) return;
        TestTrue("Alliance resolves through the production owner",
            ResolveDaxtonMigrationAlliance(*World));
        TestTrue("One same-tick suffix commits canonically",
            CommitSameTickDaxtonReason(*World,
                TEXT("reason.post_daxton.v16.no_match"), -30.f));

        FDASaveService Saves(TestSaveDirectory);
        const FString Slot = TEXT("tampered-v16-daxton-prefix-no-match");
        TestTrue("Canonical source saves", Saves.SaveCampaign(
            World->GetPersistentCampaign(), Slot).IsSuccess());
        TestTrue("Canonical source becomes schema v16",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 16.0, false));
        TestTrue("Protected reason is tampered while relationship aggregates remain explained",
            RewriteChecksummedCampaign(TestSaveDirectory, Slot,
                [](FJsonObject& Campaign)
                {
                    const TSharedPtr<FJsonObject>* WorldState = nullptr;
                    const TSharedPtr<FJsonObject>* Diplomacy = nullptr;
                    const TArray<TSharedPtr<FJsonValue>>* Relationships = nullptr;
                    if (!Campaign.TryGetObjectField(TEXT("worldState"), WorldState)
                        || WorldState == nullptr
                        || !(*WorldState)->TryGetObjectField(TEXT("diplomacy"), Diplomacy)
                        || Diplomacy == nullptr
                        || !(*Diplomacy)->TryGetArrayField(TEXT("relationships"), Relationships)
                        || Relationships == nullptr) return false;
                    for (const TSharedPtr<FJsonValue>& RelationshipValue : *Relationships)
                    {
                        const TSharedPtr<FJsonObject> Relationship =
                            RelationshipValue.IsValid() ? RelationshipValue->AsObject() : nullptr;
                        const TArray<TSharedPtr<FJsonValue>>* Reasons = nullptr;
                        if (!Relationship.IsValid()
                            || Relationship->GetStringField(TEXT("relationshipId"))
                                != TEXT("relationship.synara.forgeweave")
                            || !Relationship->TryGetArrayField(TEXT("reasonLedger"), Reasons)
                            || Reasons == nullptr) continue;
                        for (const TSharedPtr<FJsonValue>& ReasonValue : *Reasons)
                        {
                            const TSharedPtr<FJsonObject> Reason =
                                ReasonValue.IsValid() ? ReasonValue->AsObject() : nullptr;
                            if (Reason.IsValid()
                                && Reason->GetStringField(TEXT("mutationId"))
                                    == TEXT("reason.daxton.v16.trust"))
                            {
                                Reason->SetNumberField(TEXT("magnitude"), 79.0);
                                Relationship->SetNumberField(TEXT("trust"), 49.0);
                                return true;
                            }
                        }
                    }
                    return false;
                }));
        const TResult<FDACampaignSnapshot, FDASaveError> Rejected = Saves.LoadCampaign(Slot);
        TestFalse("Migration rejects a ledger with no terminal prefix match", Rejected.HasValue());
        if (!Rejected.HasValue()) TestEqual("No-match provenance is a migration failure",
            Rejected.GetError().Code, EDASaveErrorCode::MigrationFailed);
    });

    It("rejects ambiguous schema-v16 Daxton relationship prefix boundaries", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production world owner exists", World);
        if (World == nullptr) return;
        TestTrue("Alliance resolves through the production owner",
            ResolveDaxtonMigrationAlliance(*World));
        TestTrue("First same-tick suffix commits canonically",
            CommitSameTickDaxtonReason(*World,
                TEXT("reason.post_daxton.v16.ambiguous.subtract"), -30.f));
        TestTrue("Second same-tick suffix restores the resolution projection",
            CommitSameTickDaxtonReason(*World,
                TEXT("reason.post_daxton.v16.ambiguous.restore"), 30.f));
        FString ValidationError;
        TestTrue("Explicit v17 prefix disambiguates the current campaign",
            World->GetPersistentCampaign().Validate(ValidationError));

        FDASaveService Saves(TestSaveDirectory);
        const FString Slot = TEXT("ambiguous-v16-daxton-prefix-boundary");
        TestTrue("Ambiguous legacy source saves while v17 prefix is explicit",
            Saves.SaveCampaign(World->GetPersistentCampaign(), Slot).IsSuccess());
        TestTrue("Ambiguous source becomes schema v16",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, Slot, 16.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Rejected = Saves.LoadCampaign(Slot);
        TestFalse("Migration rejects two eligible terminal prefix boundaries", Rejected.HasValue());
        if (!Rejected.HasValue()) TestEqual("Ambiguous provenance is a migration failure",
            Rejected.GetError().Code, EDASaveErrorCode::MigrationFailed);
    });

    It("migrates authentic schema-v16 relationship defaults and rejects injected v17 provenance", [this]()
    {
        FDASaveService Saves(TestSaveDirectory);
        const FString AuthenticSlot = TEXT("authentic-v16-daxton-relationship-default");
        TestTrue("Current default saves", Saves.SaveCampaign(
            FDACampaignSnapshot(), AuthenticSlot).IsSuccess());
        TestTrue("Current default becomes authentic schema v16",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, AuthenticSlot, 16.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Migrated = Saves.LoadCampaign(AuthenticSlot);
        TestTrue("Authentic schema-v16 default migrates", Migrated.HasValue());
        if (Migrated.HasValue())
        {
            TestEqual("Inactive relationship prefix remains empty",
                Migrated.GetValue().DaxtonState.ResolutionRelationshipReasonCount, 0);
            TestTrue("Inactive relationship ids remain empty",
                Migrated.GetValue().DaxtonState.ResolutionRelationshipReasonMutationIds.IsEmpty());
        }

        const FString InjectedSlot = TEXT("injected-v16-daxton-relationship-prefix");
        TestTrue("Second current default saves", Saves.SaveCampaign(
            FDACampaignSnapshot(), InjectedSlot).IsSuccess());
        TestTrue("Second default becomes schema v16",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, InjectedSlot, 16.0, false));
        TestTrue("Future relationship provenance is injected with matching checksum",
            RewriteChecksummedCampaign(TestSaveDirectory, InjectedSlot,
                [](FJsonObject& Campaign)
                {
                    const TSharedPtr<FJsonObject>* Daxton = nullptr;
                    if (!Campaign.TryGetObjectField(TEXT("daxtonState"), Daxton)
                        || Daxton == nullptr || !Daxton->IsValid()) return false;
                    (*Daxton)->SetNumberField(TEXT("resolutionRelationshipReasonCount"), 1.0);
                    (*Daxton)->SetArrayField(TEXT("resolutionRelationshipReasonMutationIds"),
                        {MakeShared<FJsonValueString>(TEXT("reason.injected.future"))});
                    return true;
                }));
        const TResult<FDACampaignSnapshot, FDASaveError> Injected = Saves.LoadCampaign(InjectedSlot);
        TestFalse("Schema v16 cannot inject schema-v17 relationship provenance", Injected.HasValue());
        if (!Injected.HasValue()) TestEqual("Future relationship provenance is a migration failure",
            Injected.GetError().Code, EDASaveErrorCode::MigrationFailed);
    });

    It("rejects recomputed-checksum Daxton Leader state without canonical outcome proof", [this]()
    {
        const FString Slot = TEXT("tampered-daxton-leader-outcome");
        FDASaveService Saves(TestSaveDirectory);
        TestTrue("Canonical inactive Daxton state saves",
            Saves.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess());
        TestTrue("Leader state is forged behind a recomputed checksum",
            RewriteChecksummedCampaign(TestSaveDirectory, Slot,
                [](FJsonObject& Campaign)
                {
                    const TSharedPtr<FJsonObject>* Daxton = nullptr;
                    if (!Campaign.TryGetObjectField(TEXT("daxtonState"), Daxton)
                        || Daxton == nullptr || !Daxton->IsValid()) return false;
                    (*Daxton)->SetStringField(TEXT("phase"), TEXT("Resolved"));
                    (*Daxton)->SetBoolField(TEXT("bLeaderResolved"), true);
                    (*Daxton)->SetStringField(TEXT("leaderState"), TEXT("Governor"));
                    (*Daxton)->SetStringField(TEXT("startActionId"), TEXT("00000018000000100000000100000001"));
                    (*Daxton)->SetStringField(TEXT("resolutionActionId"), TEXT("00000018000000100000000100000002"));
                    return true;
                }));
        const TResult<FDACampaignSnapshot, FDASaveError> Rejected = Saves.LoadCampaign(Slot);
        TestFalse("Leader outcome requires canonical route relationship and objectives", Rejected.HasValue());
        if (!Rejected.HasValue()) TestEqual("Forged Leader outcome is semantic document tampering",
            Rejected.GetError().Code, EDASaveErrorCode::InvalidDocument);
    });

    It("migrates authentic schema-v17 Ascension defaults and rejects injected v18 authority", [this]()
    {
        FDASaveService Saves(TestSaveDirectory);
        const FString AuthenticSlot = TEXT("authentic-v17-ascension-default");
        TestTrue("Current default saves", Saves.SaveCampaign(
            FDACampaignSnapshot(), AuthenticSlot).IsSuccess());
        TestTrue("Default becomes authentic schema v17",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, AuthenticSlot, 17.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Migrated = Saves.LoadCampaign(AuthenticSlot);
        TestTrue("Authentic schema-v17 default migrates", Migrated.HasValue());
        if (Migrated.HasValue()) TestFalse("Migrated authority remains inactive",
            Migrated.GetValue().AscensionState.bForgeweaveAscended);

        const FString InjectedSlot = TEXT("injected-v17-ascension-authority");
        TestTrue("Second default saves", Saves.SaveCampaign(
            FDACampaignSnapshot(), InjectedSlot).IsSuccess());
        TestTrue("Second default becomes schema v17",
            RewriteActiveEnvelopeVersion(TestSaveDirectory, InjectedSlot, 17.0, false));
        TestTrue("Future authority is injected with matching checksum",
            RewriteChecksummedCampaign(TestSaveDirectory, InjectedSlot,
                [](FJsonObject& Campaign)
                {
                    const TSharedRef<FJsonObject> Ascension = MakeShared<FJsonObject>();
                    Ascension->SetBoolField(TEXT("bForgeweaveAscended"), false);
                    Campaign.SetObjectField(TEXT("ascensionState"), Ascension);
                    return true;
                }));
        const TResult<FDACampaignSnapshot, FDASaveError> Injected = Saves.LoadCampaign(InjectedSlot);
        TestFalse("Schema v17 cannot inject schema-v18 authority", Injected.HasValue());
        if (!Injected.HasValue()) TestEqual("Future Ascension is a migration failure",
            Injected.GetError().Code, EDASaveErrorCode::MigrationFailed);
    });

    It("rejects recomputed-checksum tampering of canonical Ascension fields", [this]()
    {
        FDASaveService Saves(TestSaveDirectory);
        const FString Slot = TEXT("tampered-ascension-authority");
        TestTrue("Canonical inactive Ascension saves",
            Saves.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess());
        TestTrue("Inactive Ascension reward is forged behind a refreshed checksum",
            RewriteChecksummedCampaign(TestSaveDirectory, Slot,
                [](FJsonObject& Campaign)
                {
                    const TSharedPtr<FJsonObject>* Ascension = nullptr;
                    if (!Campaign.TryGetObjectField(TEXT("ascensionState"), Ascension)
                        || Ascension == nullptr || !Ascension->IsValid()) return false;
                    (*Ascension)->SetNumberField(TEXT("convergenceAuthority"), 1.0);
                    return true;
                }));
        const TResult<FDACampaignSnapshot, FDASaveError> Rejected = Saves.LoadCampaign(Slot);
        TestFalse("Forged Ascension authority is rejected", Rejected.HasValue());
        if (!Rejected.HasValue()) TestEqual("Forged Ascension is semantic tampering",
            Rejected.GetError().Code, EDASaveErrorCode::InvalidDocument);
    });

    It("writes and enforces explicit content and build compatibility versions", [this]()
    {
        FDASaveService Saves(TestSaveDirectory);
        const auto ForgeCompatibilityVersion = [this, &Saves](const FString& Slot,
            const int32 ContentVersion, const int32 BuildVersion)
        {
            if (!Saves.SaveCampaign(FDACampaignSnapshot(), Slot).IsSuccess()) return false;
            const FString Path = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
            FString Document;
            TSharedPtr<FJsonObject> Root;
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!FFileHelper::LoadFileToString(Document, *Path)) return false;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Document);
            if (!FJsonSerializer::Deserialize(Reader, Root)
                || !Root.IsValid()
                || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid()) return false;
            Root->SetNumberField(FDASaveJsonFields::ContentVersion, ContentVersion);
            Root->SetNumberField(FDASaveJsonFields::BuildVersion, BuildVersion);
            Root->SetStringField(FDASaveJsonFields::Checksum,
                CalculateFixtureChecksum(FDASaveSchema::CurrentSchemaVersion,
                    Campaign->ToSharedRef(), ContentVersion, BuildVersion));
            return SerializeFixtureJson(Root.ToSharedRef(), Document)
                && FFileHelper::SaveStringToFile(Document, *Path,
                    FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        };

        const FString ValidSlot = TEXT("schema-v19-version-envelope");
        TestTrue("Current envelope saves", Saves.SaveCampaign(
            FDACampaignSnapshot(), ValidSlot).IsSuccess());
        FString ValidDocument;
        TSharedPtr<FJsonObject> ValidRoot;
        const FString ValidPath = FPaths::Combine(
            TestSaveDirectory, ValidSlot + TEXT(".dasave"));
        TestTrue("Current envelope reads", FFileHelper::LoadFileToString(
            ValidDocument, *ValidPath));
        const TSharedRef<TJsonReader<>> ValidReader = TJsonReaderFactory<>::Create(ValidDocument);
        TestTrue("Current envelope parses", FJsonSerializer::Deserialize(
            ValidReader, ValidRoot) && ValidRoot.IsValid());
        if (ValidRoot.IsValid())
        {
            TestEqual("Content version is explicit",
                static_cast<int32>(ValidRoot->GetNumberField(FDASaveJsonFields::ContentVersion)),
                FDASaveSchema::CurrentContentVersion);
            TestEqual("Build version is explicit",
                static_cast<int32>(ValidRoot->GetNumberField(FDASaveJsonFields::BuildVersion)),
                FDASaveSchema::CurrentBuildVersion);
        }

        TestTrue("Future content envelope is checksummed",
            ForgeCompatibilityVersion(TEXT("future-content"),
                FDASaveSchema::CurrentContentVersion + 1,
                FDASaveSchema::CurrentBuildVersion));
        const TResult<FDACampaignSnapshot, FDASaveError> FutureContent =
            Saves.LoadCampaign(TEXT("future-content"));
        TestFalse("Future content version is rejected", FutureContent.HasValue());
        if (!FutureContent.HasValue()) TestEqual("Future content is unsupported",
            FutureContent.GetError().Code, EDASaveErrorCode::UnsupportedSchema);

        TestTrue("Mismatched build envelope is checksummed",
            ForgeCompatibilityVersion(TEXT("mismatched-build"),
                FDASaveSchema::CurrentContentVersion,
                FDASaveSchema::CurrentBuildVersion + 1));
        const TResult<FDACampaignSnapshot, FDASaveError> MismatchedBuild =
            Saves.LoadCampaign(TEXT("mismatched-build"));
        TestFalse("Mismatched build version is rejected", MismatchedBuild.HasValue());
        if (!MismatchedBuild.HasValue()) TestEqual("Mismatched build is unsupported",
            MismatchedBuild.GetError().Code, EDASaveErrorCode::UnsupportedSchema);
    });

    It("migrates an authentic schema-v18 production campaign into persisted city authority", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production world exists", World);
        if (World == nullptr) return;
        const FDAWorldAssetRecord* FounderHall =
            World->GetPersistentCampaign().WorldAssets.FindByPredicate(
                [](const FDAWorldAssetRecord& Asset)
                { return Asset.CardDefinitionId == TEXT("special.founder_hall"); });
        TestNotNull("Canonical Founder Hall exists", FounderHall);
        if (FounderHall == nullptr) return;
        const FGuid FounderHallId = FounderHall->WorldAssetId;
        FDACampaignUtilitySignal Utility;
        Utility.WorldAssetId = FounderHallId;
        Utility.Utility = EDACampaignUtilityKind::Power;
        Utility.Supply = EDACampaignUtilitySupply::SignificantDeficit;
        TestTrue("Legacy source persists a nonempty utility projection",
            World->SubmitUtilitySignal(Utility));
        FDASaveService Saves(TestSaveDirectory);
        const FString Slot = TEXT("schema-v18-to-v19-city-authority");
        TestTrue("Canonical source saves", Saves.SaveCampaign(
            World->GetPersistentCampaign(), Slot).IsSuccess());
        TestTrue("Source becomes authentic v18", RewriteActiveEnvelopeVersion(
            TestSaveDirectory, Slot, 18.0, false));
        const TResult<FDACampaignSnapshot, FDASaveError> Migrated = Saves.LoadCampaign(Slot);
        TestTrue("V18 campaign migrates", Migrated.HasValue());
        if (Migrated.HasValue())
        {
            FString Error;
            TestTrue("Migrated city is initialized",
                Migrated.GetValue().CitySimulationState.bInitialized);
            TestEqual("Migrated deck remains exact",
                Migrated.GetValue().DeckState.GetInstanceIds().Num(),
                FDADeckState::RequiredDeckSize);
            TestEqual("Nonempty utility authority is preserved exactly",
                Migrated.GetValue().CitySimulationState.UtilitySignals.Num(), 1);
            if (Migrated.GetValue().CitySimulationState.UtilitySignals.Num() == 1)
            {
                const FDACampaignUtilitySignal& MigratedUtility =
                    Migrated.GetValue().CitySimulationState.UtilitySignals[0];
                TestEqual("Utility keeps exact WorldAsset identity",
                    MigratedUtility.WorldAssetId, FounderHallId);
                TestEqual("Utility keeps exact kind", MigratedUtility.Utility,
                    EDACampaignUtilityKind::Power);
                TestEqual("Utility keeps exact supply state", MigratedUtility.Supply,
                    EDACampaignUtilitySupply::SignificantDeficit);
            }
            const FDAFacilityContext* Facility =
                Migrated.GetValue().CitySimulationState.Facilities.FindByPredicate(
                    [FounderHallId](const FDAFacilityContext& Row)
                    { return Row.AssetRecord.WorldAssetId == FounderHallId; });
            TestNotNull("Founder Hall facility is reconstructed", Facility);
            if (Facility != nullptr)
            {
                TestEqual("Facility retains WorldAsset identity",
                    Facility->AssetRecord.WorldAssetId, FounderHallId);
                TestEqual("Facility retains card linkage",
                    Facility->AssetRecord.CardDefinitionId,
                    FName(TEXT("special.founder_hall")));
                TestTrue("Facility retains its CardInstance linkage",
                    Facility->AssetRecord.CardInstanceId.IsValid());
                TestEqual("Facility retains player ownership",
                    Facility->AssetRecord.OwnerCivilizationId,
                    FName(TEXT("civilization.synara")));
                TestEqual("Facility retains city identity", Facility->AssetRecord.CityId,
                    FName(TEXT("player_capital")));
                TestEqual("Facility retains grid identity", Facility->AssetRecord.GridOrigin,
                    FIntPoint(15, 15));
                TestEqual("Facility type is reconstructed from its definition",
                    Facility->FacilityType, EDAFacilityType::Infrastructure);
                TestEqual("Facility deployment cost is reconstructed exactly",
                    Facility->DeploymentCapital, 0.f);
                TestEqual("Facility authored maintenance is reconstructed exactly",
                    Facility->AuthoredMaintenanceCapitalPerCycle, 0.f);
                TestEqual("Facility Capital output is reconstructed exactly",
                    Facility->BaseOutput.Capital, 1.f);
                TestEqual("Facility Insight output is reconstructed exactly",
                    Facility->BaseOutput.Insight, 0.15f);
                TestEqual("Facility Influence output is reconstructed exactly",
                    Facility->BaseOutput.Influence, 0.1f);
                TestEqual("Facility staffing is reconstructed exactly",
                    Facility->StaffingPercent, 100.f);
                TestFalse("Founder Hall is not fabricated as automated",
                    Facility->bAutomated);
                TestEqual("Facility demand multiplier is reconstructed exactly",
                    Facility->DemandMultiplier, 1.f);
                TestEqual("Facility utility state derives from persisted signals",
                    Facility->UtilityState, EDAUtilityState::SignificantDeficit);
                TestEqual("Facility condition derives from intact authority",
                    Facility->MaintenanceCondition, EDAMaintenanceCondition::Healthy);
                TestEqual("Facility condition output is reconstructed exactly",
                    Facility->ConditionOutputMultiplier, 1.f);
                TestEqual("Facility has no invented standard modifiers",
                    Facility->StandardModifiers.Num(), 0);
                TestEqual("Non-Wonder has no invented bespoke rate",
                    Facility->BespokeWonderMaintenanceRate, 0.f);
            }
            TestTrue("Migrated aggregate validates", Migrated.GetValue().Validate(Error));
        }
    });
}
