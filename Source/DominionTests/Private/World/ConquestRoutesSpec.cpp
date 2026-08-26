#include "Conquest/DAConquestSystem.h"
#include "Diplomacy/DADiplomacySystem.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Save/DASaveService.h"

namespace
{
    FDAQuestSaveState& AddCompletedQuest(FDACampaignSnapshot& Campaign, const FName QuestId)
    {
        FDAQuestSaveState& Quest = Campaign.NarrativeState.QuestStates.Emplace_GetRef();
        Quest.QuestId = QuestId;
        Quest.ProgressState = EDAQuestProgressState::Completed;
        return Quest;
    }

    FDAQuestSaveState& AddValidCompletedQuest(FDACampaignSnapshot& Campaign, const FName QuestId)
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
        return Quest;
    }

    FDARegionState& AddRegion(FDACampaignSnapshot& Campaign, const FName RegionId, const FName OwnerId)
    {
        FDARegionState& Region = Campaign.WorldState.Regions.Emplace_GetRef();
        Region.RegionId = RegionId;
        Region.OwnerId = OwnerId;
        return Region;
    }

    FDADiplomaticRelationship& AddForgeweaveRelationship(FDACampaignSnapshot& Campaign,
        const float Trust, const float Respect, const float Dependence, const float Compatibility,
        const float Grievance = 0.f)
    {
        FDADiplomaticRelationship& Relationship = Campaign.WorldState.Diplomacy.Relationships.Emplace_GetRef();
        Relationship.RelationshipId = TEXT("relationship.synara.forgeweave");
        Relationship.Trust = Trust;
        Relationship.Respect = Respect;
        Relationship.Dependence = Dependence;
        Relationship.Compatibility = Compatibility;
        Relationship.Grievance = Grievance;
        return Relationship;
    }

    void AddCompletedContract(FDACampaignSnapshot& Campaign, const TCHAR* ContractId, const int32 Deliveries)
    {
        FDATradeContractState& Contract = Campaign.WorldState.Trade.Contracts.Emplace_GetRef();
        Contract.ContractId = FName(ContractId);
        Contract.RelationshipId = TEXT("relationship.synara.forgeweave");
        Contract.RouteId = TEXT("route.synara_ironheart_freight");
        Contract.SourceRegionId = TEXT("region.synara_frontier");
        Contract.DestinationRegionId = TEXT("region.ironheart");
        Contract.GoodId = TEXT("good.machine_components");
        Contract.SuccessfulDeliveryCount = Deliveries;
        Contract.DeliveredQuantity = Deliveries * 10;
        Contract.bCompleted = true;
        for (int32 Index = 0; Index < Deliveries; ++Index)
        {
            FDATradeDeliveryRecord& Delivery = Campaign.WorldState.Trade.Deliveries.Emplace_GetRef();
            Delivery.DeliveryId = FName(*FString::Printf(TEXT("delivery.%s.%d"), ContractId, Index));
            Delivery.ContractId = Contract.ContractId;
            Delivery.GoodId = Contract.GoodId;
            Delivery.Quantity = 10;
            Delivery.WorldTick = Index + 1;
        }
    }

    void AddDiplomaticReason(FDADiplomaticRelationship& Relationship,
        const EDADiplomaticMetric Metric, const float Magnitude, const FName MutationId,
        const int64 WorldTick)
    {
        FDADiplomaticReason& Reason = Relationship.ReasonLedger.Emplace_GetRef();
        Reason.MutationId = MutationId;
        Reason.SourceTag = TEXT("test.canonical_authority");
        Reason.Metric = Metric;
        Reason.Magnitude = Magnitude;
        Reason.WorldTick = WorldTick;
        float* Aggregate = Metric == EDADiplomaticMetric::Trust ? &Relationship.Trust
            : Metric == EDADiplomaticMetric::Respect ? &Relationship.Respect
            : Metric == EDADiplomaticMetric::Compatibility ? &Relationship.Compatibility : nullptr;
        if (Aggregate != nullptr) *Aggregate += Magnitude;
    }

    void AddCrisisResolutionRecord(FDACampaignSnapshot& Campaign,
        const EDAFoundryShortageResolution Resolution, const int32 Identity)
    {
        Campaign.RegionalCrisis.Resolution = Resolution;
        FDAFoundryShortageResolutionRecord& Record =
            Campaign.RegionalCrisis.ResolutionRecords.Emplace_GetRef();
        Record.ActionId = FGuid(23, 55, Identity, 1);
        Record.Resolution = Resolution;
        Record.WorldTick = Campaign.WorldState.CurrentWorldTick;
        Record.JointCrisisHistoryRevisionAtResolution = Campaign.ConquestState.FindMutationRevision(
            TEXT("conquest.alliance.joint_crisis_success"));
    }

    FDACampaignSnapshot MakeBaseCampaign()
    {
        FDACampaignSnapshot Campaign;
        Campaign.WorldState.bInitialized = true;
        Campaign.WorldState.CurrentWorldTick = 20;
        Campaign.WorldState.Trade.LastProcessedWorldTick = 20;
        AddRegion(Campaign, TEXT("region.ironheart"), TEXT("civilization.forgeweave"));
        AddForgeweaveRelationship(Campaign, 0.f, 0.f, 0.f, 0.f);
        return Campaign;
    }

    void AddForceEvidence(FDACampaignSnapshot& Campaign)
    {
        FDARegionState* Ironheart = Campaign.WorldState.FindRegion(TEXT("region.ironheart"));
        Ironheart->OwnerId = TEXT("civilization.synara");
        Ironheart->PersistentDelta.StateTags = {
            TEXT("control_zone.synara.iron_gate"), TEXT("control_zone.synara.grand_forge")};
        for (int32 Index = 0; Index < 2; ++Index)
        {
            FDAWorldAssetRecord& Asset = Campaign.WorldAssets.Emplace_GetRef();
            Asset.WorldAssetId = FGuid(23, 1, 1, Index + 1);
            Asset.CardDefinitionId = Index == 0 ? TEXT("forgeweave.heavy_carrier") : TEXT("forgeweave.command_bastion");
            Asset.OwnerCivilizationId = TEXT("civilization.synara");
            FDACaptureRecord& Capture = Campaign.OperationConflict.CaptureRecords.Emplace_GetRef();
            Capture.WorldAssetId = Asset.WorldAssetId;
            Capture.OriginalOwnerCivilizationId = TEXT("civilization.forgeweave");
            Capture.CapturingCivilizationId = TEXT("civilization.synara");
            Capture.bCaptureCompleted = true;
            Capture.bOutcomeResolved = true;
            Capture.Outcome = EDACaptureOutcome::Preserve;
        }
        Campaign.HistoryTags.Add(TEXT("forgeweave_elite_defeated"));
        AddCompletedQuest(Campaign, TEXT("quest.operation_iron_veil"));
        Campaign.HistoryTags.Add(TEXT("daxton_encounter_resolved"));
        Campaign.OperationConflict.Resources.PostConflictLoyalty = 45.f;
        FDAWorldAssetRecord& Shield = Campaign.WorldAssets.Emplace_GetRef();
        Shield.WorldAssetId = FGuid(23, 1, 2, 1);
        Shield.CardDefinitionId = TEXT("forgeweave.city_shield");
        Shield.StructuralIntegrity = 20.f;
        FDAStructuralDamageRecord& Damage = Campaign.OperationConflict.StructuralDamageRecords.Emplace_GetRef();
        Damage.WorldAssetId = Shield.WorldAssetId;
        Damage.CardDefinitionId = Shield.CardDefinitionId;
        Damage.bProductionDisabled = true;
    }

    void AddEconomicEvidence(FDACampaignSnapshot& Campaign)
    {
        for (int32 Index = 0; Index < 4; ++Index)
            AddCompletedContract(Campaign, *FString::Printf(TEXT("contract.forge.%d"), Index), 1);
        AddRegion(Campaign, TEXT("region.freight_corridor"), TEXT("civilization.synara"));
        FDADiplomaticRelationship* Forge = Campaign.WorldState.Diplomacy.FindRelationship(
            TEXT("relationship.synara.forgeweave"));
        Forge->Dependence = 70.f;
        AddCrisisResolutionRecord(Campaign, EDAFoundryShortageResolution::MarketExploitation, 1);
        AddCompletedQuest(Campaign, TEXT("quest.supply_noose"));
        Campaign.HistoryTags.Add(TEXT("daxton_restructuring_resolved"));
    }

    void AddInfluenceEvidence(FDACampaignSnapshot& Campaign)
    {
        AddCompletedQuest(Campaign, TEXT("quest.workers_signal"));
        Campaign.HistoryTags = {TEXT("grand_forge_preserved"), TEXT("mara_evidence_exposed"),
            TEXT("mara_numbers_worker_coalition"), TEXT("workers_protected")};
        AddCrisisResolutionRecord(Campaign, EDAFoundryShortageResolution::BrokeredCompact, 2);
        Campaign.SynaraState.FactionSupport.FindOrAdd(TEXT("faction.synara.human_agency")) = 70.0;
    }

    void AddAllianceEvidence(FDACampaignSnapshot& Campaign)
    {
        FDADiplomaticRelationship* Forge = Campaign.WorldState.Diplomacy.FindRelationship(
            TEXT("relationship.synara.forgeweave"));
        AddDiplomaticReason(*Forge, EDADiplomaticMetric::Trust, 85.f,
            TEXT("reason.alliance.trust"), Campaign.WorldState.CurrentWorldTick);
        AddDiplomaticReason(*Forge, EDADiplomaticMetric::Respect, 85.f,
            TEXT("reason.alliance.respect"), Campaign.WorldState.CurrentWorldTick);
        AddDiplomaticReason(*Forge, EDADiplomaticMetric::Compatibility, 85.f,
            TEXT("reason.alliance.compatibility"), Campaign.WorldState.CurrentWorldTick);
        Forge->Grievance = 0.f;
        AddCrisisResolutionRecord(Campaign, EDAFoundryShortageResolution::BrokeredCompact, 3);
        AddCompletedQuest(Campaign, TEXT("quest.third_foundry"));
        Campaign.HistoryTags = {TEXT("forge_relic_voluntary_transfer"), TEXT("joint_forgeweave_crisis_success")};
    }
}

BEGIN_DEFINE_SPEC(FDAConquestRoutesSpec, "Dominion.World.Conquest.Routes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAConquestRoutesSpec)

void FDAConquestRoutesSpec::Define()
{
    It("projects conquest evidence and resolves only through the authoritative production campaign owner", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production campaign owner exists", World);
        if (World == nullptr) return;

        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.HistoryTags.AddUnique(TEXT("forgeweave_elite_defeated"));
        Candidate.HistoryTags.Sort([](const FName Left, const FName Right){ return Left.LexicalLess(Right); });
        TestTrue("Canonical evidence restores through the owner", World->RestorePersistentCampaign(Candidate));
        TestTrue("Normal production tick commits the conquest projection", World->AdvanceWorldTicks(1));

        const FDACampaignSnapshot& Committed = World->GetPersistentCampaign();
        TestEqual("Production tick consumes elite evidence once", Committed.ConquestState.MilitarySovereignty, 80.0);
        TestNotNull("Production owner records the exact canonical mutation",
            Committed.ConquestState.FindMutation(TEXT("conquest.force.elite_defeated")));

        FString Error;
        TestFalse("Production completion rejects unmet systemic gates",
            World->CompleteForgeweaveConquestRoute(FGuid(23, 1, 1, 1), EDAForgeweaveRoute::Force, Error));
        TestFalse("Rejected production completion commits no shadow resolution",
            World->GetPersistentCampaign().ConquestState.bForgeweaveResolved);
    });

    It("keeps history-first joint-crisis evidence valid when the crisis resolves later in the same owner tick", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production campaign owner exists", World);
        if (World == nullptr) return;

        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        Candidate.HistoryTags.AddUnique(TEXT("joint_forgeweave_crisis_success"));
        Candidate.HistoryTags.Sort([](const FName Left, const FName Right){ return Left.LexicalLess(Right); });
        TestTrue("History-first crisis fixture restores through the owner",
            World->RestorePersistentCampaign(Candidate));
        TestTrue("Owner tick persists joint history before player resolution", World->AdvanceWorldTicks(1));

        const FDACampaignSnapshot& HistoryCommitted = World->GetPersistentCampaign();
        const FDAConquestMeterMutation* JointHistory = HistoryCommitted.ConquestState.FindMutation(
            TEXT("conquest.alliance.joint_crisis_success"));
        TestTrue("Joint history owns the saturated immutable row", JointHistory != nullptr
            && JointHistory->Delta == 25.0
            && JointHistory->WorldTick == HistoryCommitted.WorldState.CurrentWorldTick);

        const FGuid CrisisActionId(23, 4, 1, 1);
        TestEqual("A later same-tick crisis action commits through the owner",
            World->ResolveFoundryShortage(CrisisActionId, EDAFoundryShortageResolution::IndustrialSupport),
            EDAFoundryShortageActionResult::Applied);
        const FDACampaignSnapshot& Resolved = World->GetPersistentCampaign();
        FString Error;
        TestTrue("The full same-tick campaign validates", Resolved.Validate(Error));
        TestEqual("The immutable history-first row remains saturated",
            Resolved.ConquestState.AllianceReadiness, 25.0);
        TestTrue("The exact crisis record proves it observed the history revision",
            Resolved.RegionalCrisis.ResolutionRecords.Num() == 1
            && Resolved.RegionalCrisis.ResolutionRecords[0].ActionId == CrisisActionId
            && Resolved.RegionalCrisis.ResolutionRecords[0].JointCrisisHistoryRevisionAtResolution
                == Resolved.ConquestState.FindMutationRevision(
                    TEXT("conquest.alliance.joint_crisis_success")));

        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
            TEXT("ConquestSameTickCausality"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory);
        TestTrue("Same-tick causal proof saves", Saves.SaveCampaign(Resolved, TEXT("history-first")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = Saves.LoadCampaign(TEXT("history-first"));
        TestTrue("Same-tick causal proof reloads", Loaded.HasValue());
        if (Loaded.HasValue())
            TestEqual("Reload preserves the immutable saturated row",
                Loaded.GetValue().ConquestState.AllianceReadiness, 25.0);
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*Directory);

        const int64 SaturatedRevision = Resolved.ConquestState.MutationRevision;
        TestTrue("Normal world-tick preparation accepts the same-tick campaign",
            World->AdvanceWorldTicks(1));
        TestEqual("Later owner ticks do not duplicate saturated crisis evidence",
            World->GetPersistentCampaign().ConquestState.MutationRevision,
            SaturatedRevision);
    });

    It("reloads crisis-first same-tick causality before a subsequent authoritative owner tick", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production campaign owner exists", World);
        if (World == nullptr) return;

        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.WorldState.Forgeweave.ResourceHunger = 80.f;
        TestTrue("Crisis-first fixture restores through the owner", World->RestorePersistentCampaign(Candidate));
        TestTrue("Owner advances to the shared evidence tick", World->AdvanceWorldTicks(1));
        const FGuid CrisisActionId(23, 5, 2, 1);
        TestEqual("Crisis action commits before joint history on the same tick",
            World->ResolveFoundryShortage(CrisisActionId, EDAFoundryShortageResolution::IndustrialSupport),
            EDAFoundryShortageActionResult::Applied);

        Candidate = World->GetPersistentCampaign();
        Candidate.HistoryTags.AddUnique(TEXT("joint_forgeweave_crisis_success"));
        Candidate.HistoryTags.Sort([](const FName Left, const FName Right){ return Left.LexicalLess(Right); });
        FString Error;
        TestTrue("Later same-tick joint history synchronizes", FDAConquestSystem::Synchronize(Candidate, Error));
        TestTrue("Crisis-first campaign validates before persistence", Candidate.Validate(Error));
        const FDAConquestMeterMutation* CrisisMutation = Candidate.ConquestState.FindMutation(
            FName(*(TEXT("conquest.alliance.crisis.") + CrisisActionId.ToString(EGuidFormats::Digits))));
        const FDAConquestMeterMutation* JointHistory = Candidate.ConquestState.FindMutation(
            TEXT("conquest.alliance.joint_crisis_success"));
        TestTrue("Crisis-first rows contribute exactly twenty-five", CrisisMutation != nullptr
            && JointHistory != nullptr && CrisisMutation->Delta == 16.25
            && JointHistory->Delta == 8.75 && Candidate.ConquestState.AllianceReadiness == 25.0);
        TestTrue("Crisis-first record persists the absent-history boundary", Candidate.RegionalCrisis.ResolutionRecords.Num() == 1
            && Candidate.RegionalCrisis.ResolutionRecords[0].ActionId == CrisisActionId
            && Candidate.RegionalCrisis.ResolutionRecords[0].JointCrisisHistoryRevisionAtResolution == 0);

        const FString Directory = FPaths::Combine(FPaths::ProjectIntermediateDir(),
            TEXT("ConquestCrisisFirstReload"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
        FDASaveService Saves(Directory);
        TestTrue("Crisis-first causal campaign saves", Saves.SaveCampaign(Candidate, TEXT("crisis-first")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = Saves.LoadCampaign(TEXT("crisis-first"));
        TestTrue("Crisis-first causal campaign reloads", Loaded.HasValue());
        if (Loaded.HasValue())
        {
            FDACampaignSnapshot Reloaded = Loaded.GetValue();
            TestTrue("Reloaded crisis-first campaign validates", Reloaded.Validate(Error));
            TestEqual("Reload preserves exact crisis contribution", Reloaded.ConquestState.AllianceReadiness, 25.0);
            const int64 SaturatedRevision = Reloaded.ConquestState.MutationRevision;
            const int64 StableProof = Reloaded.RegionalCrisis.ResolutionRecords[0]
                .JointCrisisHistoryRevisionAtResolution;
            TestTrue("Reloaded campaign restores through the authoritative owner",
                World->RestorePersistentCampaign(Reloaded));
            TestTrue("Subsequent authoritative world tick commits", World->AdvanceWorldTicks(1));
            const FDACampaignSnapshot& Reticked = World->GetPersistentCampaign();
            TestTrue("Subsequent owner state validates", Reticked.Validate(Error));
            TestEqual("Subsequent tick retains exact crisis contribution",
                Reticked.ConquestState.AllianceReadiness, 25.0);
            TestEqual("Subsequent tick creates no duplicate mutation",
                Reticked.ConquestState.MutationRevision, SaturatedRevision);
            TestTrue("Subsequent tick preserves the exact causal boundary",
                Reticked.RegionalCrisis.ResolutionRecords.Num() == 1
                && Reticked.RegionalCrisis.ResolutionRecords[0].ActionId == CrisisActionId
                && Reticked.RegionalCrisis.ResolutionRecords[0]
                    .JointCrisisHistoryRevisionAtResolution == StableProof
                && StableProof == 0);
        }
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*Directory);
    });

    It("atomically publishes a legitimate route completion through the production campaign owner", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production campaign owner exists", World);
        if (World == nullptr) return;

        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        FDADiplomaticRelationship* Forge = Candidate.WorldState.Diplomacy.FindRelationship(
            TEXT("relationship.synara.forgeweave"));
        if (Forge == nullptr)
            Forge = &AddForgeweaveRelationship(Candidate, 0.f, 0.f, 0.f, 0.f);
        AddDiplomaticReason(*Forge, EDADiplomaticMetric::Trust, 85.f,
            TEXT("reason.production.alliance.trust"), Candidate.WorldState.CurrentWorldTick);
        AddDiplomaticReason(*Forge, EDADiplomaticMetric::Respect, 85.f,
            TEXT("reason.production.alliance.respect"), Candidate.WorldState.CurrentWorldTick);
        AddDiplomaticReason(*Forge, EDADiplomaticMetric::Compatibility, 85.f,
            TEXT("reason.production.alliance.compatibility"), Candidate.WorldState.CurrentWorldTick);
        AddValidCompletedQuest(Candidate, TEXT("quest.third_foundry"));
        Candidate.HistoryTags.AddUnique(TEXT("forge_relic_voluntary_transfer"));
        Candidate.HistoryTags.AddUnique(TEXT("joint_forgeweave_crisis_success"));
        Candidate.HistoryTags.Sort([](const FName Left, const FName Right){ return Left.LexicalLess(Right); });
        TestTrue("Legitimate canonical gates restore through the owner",
            World->RestorePersistentCampaign(Candidate));

        int32 PublishedCount = 0;
        TSharedPtr<const FDACampaignSnapshot> Published;
        const FDelegateHandle Handle = World->OnWorldTickStateCommitted.AddLambda(
            [&PublishedCount, &Published](const FDACommittedCampaignSnapshot Snapshot)
            {
                ++PublishedCount;
                Published = Snapshot;
            });
        const FGuid ActionId(23, 2, 1, 1);
        FString Error;
        TestTrue("Production owner commits eligible Alliance route",
            World->CompleteForgeweaveConquestRoute(ActionId, EDAForgeweaveRoute::Alliance, Error));
        World->OnWorldTickStateCommitted.Remove(Handle);

        const FDACampaignSnapshot& Committed = World->GetPersistentCampaign();
        TestEqual("Successful completion publishes exactly once", PublishedCount, 1);
        TestTrue("Published state is the atomically committed authority", Published.IsValid()
            && Published->ConquestState.bForgeweaveResolved
            && Published->ConquestState.ResolutionActionId == ActionId);
        TestTrue("Owner persists the exact Alliance route and history", Committed.ConquestState.bForgeweaveResolved
            && Committed.ConquestState.ResolvedRoute == EDAForgeweaveRoute::Alliance
            && Committed.HistoryTags.Contains(TEXT("forgeweave_allied")));
    });

    It("projects signed diplomatic reasons from the same clamped canonical aggregate", [this]()
    {
        FDACampaignSnapshot Campaign = MakeBaseCampaign();
        FDADiplomaticRelationship& Forge = Campaign.WorldState.Diplomacy.Relationships[0];
        AddDiplomaticReason(Forge, EDADiplomaticMetric::Trust, -10.f,
            TEXT("reason.signed.trust.loss"), Campaign.WorldState.CurrentWorldTick);
        AddDiplomaticReason(Forge, EDADiplomaticMetric::Trust, 25.f,
            TEXT("reason.signed.trust.gain"), Campaign.WorldState.CurrentWorldTick);
        FString Error;
        TestTrue("Signed canonical history synchronizes", FDAConquestSystem::Synchronize(Campaign, Error));
        TestEqual("Canonical Trust aggregate is fifteen", Campaign.ConquestState.AllianceComponents.Trust, 15.0);
        TestEqual("Readiness uses Clamp of the signed sum", Campaign.ConquestState.AllianceReadiness, 3.75);
        const FDAConquestMeterMutation* Gain = Campaign.ConquestState.FindMutation(
            TEXT("conquest.alliance.reason.reason.signed.trust.gain"));
        TestTrue("The effective reason retains exact canonical identity", Gain != nullptr
            && Gain->SourceId == TEXT("reason.signed.trust.gain") && Gain->Delta == 3.75);
    });

    It("projects joint-crisis history and later crisis resolution in either order without double counting", [this]()
    {
        FDACampaignSnapshot HistoryFirst = MakeBaseCampaign();
        HistoryFirst.HistoryTags.Add(TEXT("joint_forgeweave_crisis_success"));
        FString Error;
        TestTrue("History-first evidence synchronizes", FDAConquestSystem::Synchronize(HistoryFirst, Error));
        TestEqual("History-first reaches the saturated crisis component once",
            HistoryFirst.ConquestState.AllianceReadiness, 25.0);
        ++HistoryFirst.WorldState.CurrentWorldTick;
        AddCrisisResolutionRecord(HistoryFirst, EDAFoundryShortageResolution::IndustrialSupport, 31);
        TestTrue("Later canonical crisis resolution remains valid",
            FDAConquestSystem::Synchronize(HistoryFirst, Error));
        TestEqual("Later crisis evidence cannot double count saturated readiness",
            HistoryFirst.ConquestState.AllianceReadiness, 25.0);
        TestEqual("History-first consumes one effective crisis source",
            HistoryFirst.ConquestState.Mutations.FilterByPredicate([](const FDAConquestMeterMutation& Row)
            {
                return Row.Route == EDAForgeweaveRoute::Alliance;
            }).Num(), 1);

        FDACampaignSnapshot CrisisFirst = MakeBaseCampaign();
        AddCrisisResolutionRecord(CrisisFirst, EDAFoundryShortageResolution::IndustrialSupport, 32);
        TestTrue("Crisis-first evidence synchronizes", FDAConquestSystem::Synchronize(CrisisFirst, Error));
        TestEqual("Non-collapse crisis contributes sixty-five over four",
            CrisisFirst.ConquestState.AllianceReadiness, 16.25);
        CrisisFirst.HistoryTags.Add(TEXT("joint_forgeweave_crisis_success"));
        TestTrue("Later joint history synchronizes", FDAConquestSystem::Synchronize(CrisisFirst, Error));
        TestEqual("History supplies only the remaining canonical marginal",
            CrisisFirst.ConquestState.AllianceReadiness, 25.0);
        TestTrue("Crisis-first same-tick campaign passes full validation", CrisisFirst.Validate(Error));
        const int32 RevisionAfterSaturation = CrisisFirst.ConquestState.MutationRevision;
        TestTrue("Repeated synchronization is exact-once", FDAConquestSystem::Synchronize(CrisisFirst, Error));
        TestEqual("Saturated duplicate evidence creates no mutation",
            CrisisFirst.ConquestState.MutationRevision, RevisionAfterSaturation);
    });

    It("rejects replay-coherent crisis double-count tampering above the canonical saturation", [this]()
    {
        FDACampaignSnapshot Campaign = MakeBaseCampaign();
        Campaign.HistoryTags.Add(TEXT("joint_forgeweave_crisis_success"));
        FString Error;
        TestTrue("Canonical history synchronizes", FDAConquestSystem::Synchronize(Campaign, Error));
        ++Campaign.WorldState.CurrentWorldTick;
        AddCrisisResolutionRecord(Campaign, EDAFoundryShortageResolution::IndustrialSupport, 33);

        const FDAFoundryShortageResolutionRecord& Record = Campaign.RegionalCrisis.ResolutionRecords[0];
        const FName Source(*Record.ActionId.ToString(EGuidFormats::Digits));
        FDAConquestMeterMutation& Forged = Campaign.ConquestState.Mutations.Emplace_GetRef();
        Forged.MutationId = FName(*(TEXT("conquest.alliance.crisis.") + Source.ToString()));
        Forged.Route = EDAForgeweaveRoute::Alliance;
        Forged.Meter = EDAConquestMeter::AllianceReadiness;
        Forged.SourceAuthority = TEXT("campaign.regional_crisis_resolution.alliance");
        Forged.SourceId = Source;
        Forged.Delta = 16.25;
        Forged.Result = 41.25;
        Forged.WorldTick = Campaign.WorldState.CurrentWorldTick;
        Campaign.ConquestState.AllianceReadiness = 41.25;
        Campaign.ConquestState.AllianceComponents.Trust = 65.0;
        Campaign.ConquestState.AllianceWeight = 41.25;
        Campaign.ConquestState.MutationRevision = 2;
        FDAConquestRouteWeightRecord& Weight = Campaign.ConquestState.RouteWeightHistory.Emplace_GetRef();
        Weight.WorldTick = Campaign.WorldState.CurrentWorldTick;
        Weight.Revision = 2;
        Weight.Alliance = 41.25;
        TestTrue("Tamper fixture remains internally replay-coherent",
            Campaign.ConquestState.Validate(Error));
        TestFalse("Canonical synchronization rejects retroactive double counting",
            FDAConquestSystem::Synchronize(Campaign, Error));
    });

    It("caps each default economic action at fifteen autonomy and requires fulfilled delivery authority", [this]()
    {
        FDACampaignSnapshot Campaign = MakeBaseCampaign();
        AddCompletedContract(Campaign, TEXT("contract.forge.one"), 1);
        AddCompletedContract(Campaign, TEXT("contract.forge.two"), 1);
        FString Error;
        TestTrue("Canonical fulfilled contracts synchronize", FDAConquestSystem::Synchronize(Campaign, Error));
        TestEqual("Two contracts remove exactly thirty autonomy", Campaign.ConquestState.EconomicAutonomy, 70.0);
        for (const FDAConquestMeterMutation& Mutation : Campaign.ConquestState.Mutations)
            if (Mutation.Meter == EDAConquestMeter::EconomicAutonomy)
                TestTrue("No default action removes more than fifteen", Mutation.Delta >= -15.0);

        FDACampaignSnapshot Unfulfilled = MakeBaseCampaign();
        FDATradeContractState& Contract = Unfulfilled.WorldState.Trade.Contracts.Emplace_GetRef();
        Contract.ContractId = TEXT("contract.paper_only");
        Contract.RelationshipId = TEXT("relationship.synara.forgeweave");
        Contract.DestinationRegionId = TEXT("region.ironheart");
        TestTrue("Paper contract synchronizes without leverage", FDAConquestSystem::Synchronize(Unfulfilled, Error));
        TestEqual("Unfulfilled contract changes nothing", Unfulfilled.ConquestState.EconomicAutonomy, 100.0);
    });

    It("never converts stored Influence into Civic Legitimacy damage", [this]()
    {
        FDACampaignSnapshot Campaign = MakeBaseCampaign();
        Campaign.OperationConflict.Resources.Influence = 100000.f;
        FString Error;
        TestTrue("Stored currency fixture synchronizes", FDAConquestSystem::Synchronize(Campaign, Error));
        TestEqual("Stored Influence alone leaves legitimacy intact", Campaign.ConquestState.CivicLegitimacy, 100.0);
        TestEqual("Stored Influence creates no influence route weight", Campaign.ConquestState.InfluenceWeight, 0.0);
    });

    It("derives force pressure from control zones captures elite defeat and shield command state", [this]()
    {
        FDACampaignSnapshot Campaign = MakeBaseCampaign();
        AddForceEvidence(Campaign);
        FString Error;
        TestTrue("Canonical military evidence synchronizes", FDAConquestSystem::Synchronize(Campaign, Error));
        TestEqual("Systemic military evidence reaches surrender", Campaign.ConquestState.MilitarySovereignty, 0.0);
        TestTrue("Force route weight records real hooks", Campaign.ConquestState.ForceWeight >= 100.0);
    });

    It("enforces the eighty average sixty-five floor Third Foundry and Major Grievance gates", [this]()
    {
        FDACampaignSnapshot Campaign = MakeBaseCampaign();
        AddAllianceEvidence(Campaign);
        FString Error;
        TestTrue("Alliance evidence synchronizes", FDAConquestSystem::Synchronize(Campaign, Error));
        TestEqual("Readiness is the exact component average", Campaign.ConquestState.AllianceReadiness, 88.75);
        TestTrue("Eligible union passes every hard gate", FDAConquestSystem::CanCompleteRoute(
            EDAForgeweaveRoute::Alliance, Campaign, Error));

        ++Campaign.WorldState.CurrentWorldTick;
        AddDiplomaticReason(Campaign.WorldState.Diplomacy.Relationships[0],
            EDADiplomaticMetric::Respect, -21.f, TEXT("reason.alliance.respect.lowered"),
            Campaign.WorldState.CurrentWorldTick);
        TestTrue("Lowered component resynchronizes", FDAConquestSystem::Synchronize(Campaign, Error));
        TestFalse("A component below sixty-five blocks union", FDAConquestSystem::CanCompleteRoute(
            EDAForgeweaveRoute::Alliance, Campaign, Error));
        ++Campaign.WorldState.CurrentWorldTick;
        AddDiplomaticReason(Campaign.WorldState.Diplomacy.Relationships[0],
            EDADiplomaticMetric::Respect, 21.f, TEXT("reason.alliance.respect.restored"),
            Campaign.WorldState.CurrentWorldTick);
        Campaign.HistoryTags.Add(TEXT("major_grievance.ironheart_civilian_damage"));
        TestTrue("Major grievance fixture resynchronizes", FDAConquestSystem::Synchronize(Campaign, Error));
        TestFalse("Any Major Grievance blocks union", FDAConquestSystem::CanCompleteRoute(
            EDAForgeweaveRoute::Alliance, Campaign, Error));
    });

    It("completes Force Economic Influence and Alliance in four independent campaign fixtures", [this]()
    {
        struct FRouteFixture
        {
            EDAForgeweaveRoute Route;
            TFunction<void(FDACampaignSnapshot&)> AddEvidence;
        };
        const TArray<FRouteFixture> Fixtures = {
            {EDAForgeweaveRoute::Force, AddForceEvidence},
            {EDAForgeweaveRoute::Economic, AddEconomicEvidence},
            {EDAForgeweaveRoute::Influence, AddInfluenceEvidence},
            {EDAForgeweaveRoute::Alliance, AddAllianceEvidence},
        };
        for (int32 Index = 0; Index < Fixtures.Num(); ++Index)
        {
            FDACampaignSnapshot Campaign = MakeBaseCampaign();
            Fixtures[Index].AddEvidence(Campaign);
            FString Error;
            TestTrue("Independent route evidence synchronizes", FDAConquestSystem::Synchronize(Campaign, Error));
            TestTrue("Independent route completes", FDAConquestSystem::CompleteRoute(
                FGuid(23, 23, Index + 1, 1), Fixtures[Index].Route, Campaign, Error));
            TestTrue("Resolution is durable and route-specific", Campaign.ConquestState.bForgeweaveResolved
                && Campaign.ConquestState.ResolvedRoute == Fixtures[Index].Route);
        }
    });

    It("re-proves every persisted route resolution from canonical campaign gates", [this]()
    {
        static const EDAForgeweaveRoute Routes[] = {EDAForgeweaveRoute::Force,
            EDAForgeweaveRoute::Economic, EDAForgeweaveRoute::Influence, EDAForgeweaveRoute::Alliance};
        static const FName RouteTags[] = {TEXT("forgeweave_forced"), TEXT("forgeweave_economic_union"),
            TEXT("forgeweave_influence_transfer"), TEXT("forgeweave_allied")};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Routes); ++Index)
        {
            FDACampaignSnapshot Forged = MakeBaseCampaign();
            Forged.ConquestState.bForgeweaveResolved = true;
            Forged.ConquestState.ResolvedRoute = Routes[Index];
            Forged.ConquestState.ResolutionActionId = FGuid(23, 99, Index + 1, 1);
            Forged.ConquestState.ResolvedWorldTick = Forged.WorldState.CurrentWorldTick;
            Forged.HistoryTags.Add(RouteTags[Index]);
            Forged.HistoryTags.Sort([](const FName Left, const FName Right){ return Left.LexicalLess(Right); });
            FString Error;
            TestFalse("A route tag cannot forge its missing canonical completion gates",
                FDAConquestAuthorityValidator::ValidateResolvedRoute(Forged, Error));
        }
    });
}
