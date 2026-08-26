import copy
import hashlib
import json
import pathlib
import unittest


ROOT = pathlib.Path(__file__).parents[2]
MANIFEST_PATH = ROOT / "Content/DA/Manifests/FirstHourQuests.json"
SCHEMA_PATH = ROOT / "Content/DA/Manifests/FirstHourQuests.schema.json"
CARD_MANIFEST_PATH = ROOT / "Content/DA/Manifests/VerticalSliceContent.json"
PIPELINE_SOURCE = ROOT / "Source/DominionWorld/Private/Content/DAFirstHourQuestContent.cpp"
RUNTIME_SOURCE = ROOT / "Source/DominionWorld/Private/Narrative/DAFirstHourCampaignRuntime.cpp"
SAVE_SOURCE = ROOT / "Source/DominionCore/Private/Save/DACampaignSaveGame.cpp"
NARRATIVE_SOURCE = ROOT / "Source/DominionCore/Private/Narrative/DANarrativeRecords.cpp"
REGISTRY_SOURCE = ROOT / "Source/DominionWorld/Private/Content/DAFirstHourContentRegistrySubsystem.cpp"
COORDINATOR_HEADER = ROOT / "Source/DominionWorld/Public/Narrative/DAFirstHourCampaignCoordinatorSubsystem.h"
COORDINATOR_SOURCE = ROOT / "Source/DominionWorld/Private/Narrative/DAFirstHourCampaignCoordinatorSubsystem.cpp"
CORE_RECORDS_HEADER = ROOT / "Source/DominionCore/Public/Narrative/DANarrativeRecords.h"
CORE_SAVE_HEADER = ROOT / "Source/DominionCore/Public/Save/DACampaignSaveGame.h"
SAVE_SCHEMA_HEADER = ROOT / "Source/DominionCore/Public/Save/DASaveSchema.h"
WORLD_HEADER = ROOT / "Source/DominionWorld/Public/Regions/DAWorldStateSubsystem.h"
WORLD_SOURCE = ROOT / "Source/DominionWorld/Private/Regions/DAWorldStateSubsystem.cpp"
ECONOMY_HEADER = ROOT / "Source/DominionSimulation/Public/Economy/DAEconomySubsystem.h"
ECONOMY_SOURCE = ROOT / "Source/DominionSimulation/Private/Economy/DAEconomySubsystem.cpp"
MIGRATION_TEST_SOURCE = ROOT / "Source/DominionTests/Private/Core/SaveMigrationSpec.cpp"
MIGRATION_SOURCE = ROOT / "Source/DominionCore/Private/Save/DASaveService.cpp"
CONTENT_HEADER = ROOT / "Source/DominionWorld/Public/Content/DAFirstHourQuestContent.h"
QUEST_HEADER = ROOT / "Source/DominionCore/Public/Narrative/DAQuestTypes.h"


def canonical_fingerprint(document):
    material = copy.deepcopy(document)
    material.pop("fingerprint")
    encoded = json.dumps(material, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha1(encoded).hexdigest()


class FirstHourQuestManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(MANIFEST_PATH.read_text())
        cls.schema = json.loads(SCHEMA_PATH.read_text())
        cls.card_ids = {row["id"] for row in json.loads(CARD_MANIFEST_PATH.read_text())["definitions"]}
        cls.by_id = {quest["id"]: quest for quest in cls.manifest["quests"]}

    def test_exact_frozen_quest_and_citizen_identity(self):
        expected = [
            ("quest.wake_the_hall", "Wake the Hall", "/Game/DA/Quests/Q_WakeTheHall"),
            ("quest.a_place_to_stay", "A Place to Stay", "/Game/DA/Quests/Q_APlaceToStay"),
            ("quest.power_water_people", "Power, Water, People", "/Game/DA/Quests/Q_PowerWaterPeople"),
            ("quest.nia_needs_a_job", "Nia Needs a Job", "/Game/DA/Quests/Q_NiaNeedsAJob"),
            ("quest.replacement_model", "The Replacement Model", "/Game/DA/Quests/Q_ReplacementModel"),
            ("quest.agency_has_a_price", "Agency Has a Price", "/Game/DA/Quests/Q_AgencyHasAPrice"),
            ("quest.signal_in_foundation", "Signal in the Foundation", "/Game/DA/Quests/Q_SignalInFoundation"),
            ("quest.iron_at_border", "Iron at the Border", "/Game/DA/Quests/Q_IronAtBorder"),
            ("quest.basin_speaks", "The Basin Speaks", "/Game/DA/Quests/Q_BasinSpeaks"),
        ]
        self.assertEqual([(q["id"], q["title"], q["assetPath"]) for q in self.manifest["quests"]], expected)
        self.assertEqual(self.manifest["citizen"]["id"], "citizen.synara.nia_vale")
        self.assertEqual(self.manifest["citizen"]["assetPath"], "/Game/DA/Citizens/DA_Citizen_NiaVale")

    def test_fingerprint_is_canonical_json_excluding_only_fingerprint(self):
        self.assertEqual(self.manifest["fingerprint"], canonical_fingerprint(self.manifest))
        reordered = {key: self.manifest[key] for key in reversed(self.manifest)}
        self.assertEqual(canonical_fingerprint(reordered), self.manifest["fingerprint"])
        mutated = copy.deepcopy(self.manifest)
        mutated["quests"][0]["title"] += "!"
        self.assertNotEqual(canonical_fingerprint(mutated), self.manifest["fingerprint"])

    def test_every_definition_binding_and_reward_uses_task19_authority(self):
        definition_ids = set()
        for quest in self.manifest["quests"]:
            definition_ids.update(binding["definitionId"] for binding in quest["worldAssetBindings"])
            definition_ids.update(reward["definitionId"] for reward in quest["rewards"] if "definitionId" in reward)
            for outcome in quest["outcomes"].values():
                definition_ids.update(reward["definitionId"] for reward in outcome["rewards"] if "definitionId" in reward)
        self.assertTrue(definition_ids)
        self.assertEqual(definition_ids - self.card_ids, set())

    def test_constructed_objective_assets_are_dynamic_not_start_requirements(self):
        dynamic = {
            "quest.a_place_to_stay": {"adaptive_habitat"},
            "quest.power_water_people": {"microgrid_station", "water_reclaimer"},
            "quest.nia_needs_a_job": {"cognitive_operations_tower"},
        }
        for quest_id, binding_ids in dynamic.items():
            rows = {row["bindingId"]: row for row in self.by_id[quest_id]["worldAssetBindings"]}
            self.assertEqual({key for key, row in rows.items() if row["bindWhen"] == "Objective"}, binding_ids)
            self.assertTrue(all(rows[key]["requireOperational"] for key in binding_ids if key != "adaptive_habitat"))
        self.assertFalse({row["bindingId"]: row for row in self.by_id["quest.a_place_to_stay"]["worldAssetBindings"]}["adaptive_habitat"]["requireOperational"])
        self.assertNotIn("evaluationKey", MANIFEST_PATH.read_text())

    def test_first_four_objectives_and_exact_rewards_are_authored(self):
        q1 = self.by_id["quest.wake_the_hall"]
        self.assertEqual([node["condition"] for node in q1["nodes"]],
                         ["NewCampaign", "FounderReached", "FounderHallPowered", "ExplicitChoice", "CustodianMarkingsInspected", "FounderHallOnline", "Resolved"])
        self.assertEqual(q1["completionHistoryTags"], ["founder_hall_awake"])
        self.assertEqual([(r["type"], r.get("definitionId")) for r in q1["rewards"]],
                         [("CityMode", None), ("CardInstance", "synara.adaptive_habitat")])
        q2 = self.by_id["quest.a_place_to_stay"]
        self.assertEqual([node["condition"] for node in q2["nodes"]],
                         ["PrerequisitesMet", "AdaptiveHabitatInspected", "AdaptiveHabitatPlaced", "HabitatConstructionComplete", "NiaPresent", "Resolved"])
        self.assertEqual({r["definitionId"] for r in q2["rewards"]},
                         {"universal.microgrid_station", "universal.water_reclaimer"})
        q3 = self.by_id["quest.power_water_people"]
        self.assertEqual([node["condition"] for node in q3["nodes"]],
                         ["HabitatOccupied", "BoundAssetOperational", "BoundAssetOperational", "ExplicitChoice", "UtilityExpansionAcknowledged", "HabitatPowerFullySupplied", "HabitatWaterFullySupplied", "Resolved"])
        q4 = self.by_id["quest.nia_needs_a_job"]
        self.assertEqual([node["condition"] for node in q4["nodes"]],
                         ["PrerequisitesMet", "NiaSpokenTo", "BoundAssetOperational", "TowerHalfStaffed", "NiaAssignedToTower", "Resolved"])
        self.assertEqual([(r["type"], r.get("definitionId")) for r in q4["rewards"]],
                         [("OperatorXp", None), ("CardInstance", "universal.corner_exchange")])

    def test_replacement_model_has_only_authored_deltas_histories_and_rewards(self):
        q5 = self.by_id["quest.replacement_model"]
        self.assertEqual(q5["startCondition"], "AutonomousExchangeAvailable")
        self.assertEqual(set(q5["outcomes"]), {"accept", "modify", "audit", "reject"})
        accept, modify, audit, reject = (q5["outcomes"][name] for name in ("accept", "modify", "audit", "reject"))
        self.assertEqual(accept["historyTags"], ["nia_automation_accepted"])
        self.assertEqual((accept["dependencyDelta"], accept["humanAgencySupportDelta"], accept["niaTrustDelta"]), (6, -8, -5))
        self.assertEqual(accept["semanticEffects"], ["effect.capital_efficiency.increased"])
        self.assertEqual(modify["historyTags"], ["nia_jobs_preserved"])
        self.assertEqual((modify["dependencyDelta"], modify["niaTrustDelta"]), (2, 4))
        self.assertNotIn("humanAgencySupportDelta", modify)
        self.assertEqual(modify["semanticEffects"], ["effect.capital_efficiency.smaller_increase"])
        self.assertEqual(audit["historyTags"], ["nia_model_audited"])
        self.assertEqual(audit["eligibilityAny"], ["Vision", "ResearchAction"])
        self.assertEqual([r["type"] for r in audit["rewards"]], ["InsightReward", "IntelligenceAuditorPath"])
        self.assertEqual(reject["semanticEffects"], ["effect.human_agency.supported", "effect.ascendants.approval_lost"])
        self.assertFalse(any(tag.startswith("quest.replacement_model.") for outcome in q5["outcomes"].values() for tag in outcome["historyTags"]))

    def test_final_four_quests_are_full_canonical_flows(self):
        agency = self.by_id["quest.agency_has_a_price"]
        self.assertEqual(agency["startCondition"], "DependencyAbove25")
        self.assertEqual(set(agency["outcomes"]), {"agency_forum", "retrain", "automation_cap", "reject"})
        self.assertEqual(agency["outcomes"]["reject"]["historyTags"], ["agency_petition_rejected"])
        self.assertTrue(all(o["historyTags"] == ["agency_petition_supported"] for key, o in agency["outcomes"].items() if key != "reject"))
        signal = self.by_id["quest.signal_in_foundation"]
        self.assertEqual(signal["startCondition"], "SixPlayerBuildings")
        self.assertEqual([n["condition"] for n in signal["nodes"]],
                         ["PrerequisitesMet", "EnteredUtilityTunnel", "AncientNodeRestored", "MaintenanceDronesDefeated", "UnknownSymbolInspected", "Resolved"])
        self.assertEqual([r["type"] for r in signal["rewards"]], ["AxiomArchiveFragment"])
        iron = self.by_id["quest.iron_at_border"]
        self.assertEqual(set(iron["outcomes"]), {"accept", "refuse", "favorable_terms", "defer"})
        self.assertEqual(iron["outcomes"]["accept"]["historyTags"], ["forge_trade_supported"])
        basin = self.by_id["quest.basin_speaks"]
        self.assertEqual([n["condition"] for n in basin["nodes"]],
                         ["ForgeweaveContact", "EdenBasinReached", "WaterQualityInspected", "AmaraSpokenTo", "OriSpokenTo", "Resolved"])
        self.assertEqual(basin["completionHistoryTags"], ["eden_watershed_supported"])
        self.assertEqual([r["type"] for r in basin["rewards"]], ["EdenTradeAccess"])

    def test_schema_is_closed_at_every_authored_object(self):
        self.assertFalse(self.schema["additionalProperties"])
        self.assertEqual(self.schema["properties"]["quests"]["minItems"], 9)
        self.assertEqual(self.schema["properties"]["quests"]["maxItems"], 9)
        quest_keys = {"id", "title", "sourceDefinitionId", "assetPath", "prerequisiteQuestIds", "startCondition",
                      "worldAssetBindings", "nodes", "completionHistoryTags", "rewards", "outcomes"}
        for quest in self.manifest["quests"]:
            self.assertEqual(set(quest), quest_keys)
            for node in quest["nodes"]:
                expected = {"id", "type", "sourceDefinitionId", "condition", "edges"}
                if "bindingId" in node:
                    expected.add("bindingId")
                self.assertEqual(set(node), expected)

    def test_human_override_gate_is_exact(self):
        self.assertEqual(self.manifest["citizen"]["championEligibility"], {
            "requiredStoryState": "story.nia.human_override.supported",
            "requiredCrisisQuestId": "quest.human_override",
            "requiredCrisisSourceDefinitionId": "quest.human_override.v1",
            "requiredCrisisDefinitionFingerprint": "433693a4a1332f736fc8e4a08a565fad",
            "requiredCrisisCompletionActionId": "action.quest.human_override.completed",
            "championDefinitionId": "champion.nia_vale.human_override",
        })

    def test_wake_optional_exploration_and_habitat_lifecycle_are_complete(self):
        wake = self.by_id["quest.wake_the_hall"]
        wake_conditions = [node["condition"] for node in wake["nodes"]]
        self.assertIn("CustodianMarkingsInspected", wake_conditions)
        self.assertTrue(any(node["type"] == "Choice" and
                            {edge["branchTag"] for edge in node["edges"]} == {"explore_markings", "continue_activation"}
                            for node in wake["nodes"]))
        habitat = self.by_id["quest.a_place_to_stay"]
        self.assertEqual([node["condition"] for node in habitat["nodes"]], [
            "PrerequisitesMet", "AdaptiveHabitatInspected", "AdaptiveHabitatPlaced",
            "HabitatConstructionComplete", "NiaPresent", "Resolved",
        ])
        utilities = self.by_id["quest.power_water_people"]
        self.assertIn("UtilityExpansionAcknowledged", [node["condition"] for node in utilities["nodes"]])

    def test_production_source_has_full_projection_and_no_shadow_authority_inputs(self):
        pipeline = PIPELINE_SOURCE.read_text()
        runtime = RUNTIME_SOURCE.read_text()
        save = SAVE_SOURCE.read_text()
        self.assertIn("ComputeCanonicalFingerprint", pipeline)
        self.assertIn("QuestEntriesEqual", pipeline)
        self.assertIn("CitizenDefinitionsEqual", pipeline)
        self.assertIn("ValidateExactFrozenProjection", pipeline)
        self.assertNotIn("FactionStates", runtime)
        self.assertNotIn("SystemicPressure", runtime)
        self.assertIn("Campaign.SynaraState", runtime)
        self.assertIn("CollectionState", save)
        self.assertIn("ValidateFirstHourUnlockRecord", save)

    def test_round_two_save_and_runtime_guards_are_present(self):
        runtime = RUNTIME_SOURCE.read_text()
        save = SAVE_SOURCE.read_text()
        narrative = NARRATIVE_SOURCE.read_text()
        self.assertIn("WorldMap", runtime)
        self.assertIn("AxiomArchiveFragment", runtime)
        self.assertIn("IsCanonicalTask19Building", runtime)
        self.assertIn("FacilityWorldAssetId", runtime)
        self.assertIn("DeterministicRewardGuid", save)
        self.assertIn("AcquisitionSource != EDAAcquisitionSource::QuestReward", save)
        self.assertIn("BaselineDependency", narrative)
        self.assertIn("BaselineHumanAgencySupport", narrative)
        self.assertIn("BaselineNiaTrust", narrative)
        registry = REGISTRY_SOURCE.read_text()
        self.assertIn("GetPrimaryAssetPath", registry)
        self.assertIn("ValidateGeneratedCache", registry)
        self.assertIn("BuildRuntimeContent", registry)
        self.assertIn("canonical manifest fallback", registry)

    def test_round_three_uses_one_campaign_authority_and_historical_ledgers(self):
        records = CORE_RECORDS_HEADER.read_text()
        save = CORE_SAVE_HEADER.read_text()
        runtime = RUNTIME_SOURCE.read_text()
        narrative = NARRATIVE_SOURCE.read_text()
        self.assertIn("FDASynaraCampaignState SynaraState", save)
        self.assertNotIn("FDASynaraCampaignAuthority SynaraAuthority", records)
        self.assertIn("FactionSupport", records)
        self.assertIn("DependencyReasons", records)
        self.assertIn("CitizenRelationshipReasons", records)
        self.assertIn("PolicyReasons", records)
        self.assertIn("CitizenEmployment", records)
        self.assertIn("BaselineDependency", narrative)
        self.assertNotIn("SynaraAuthority.Dependency != Effect.ResultDependency", narrative)
        self.assertIn("ApplyDependencyReason", runtime)
        self.assertIn("WorldState.Diplomacy", runtime)

    def test_round_three_requires_durable_story_audit_and_strict_later_crisis(self):
        records = CORE_RECORDS_HEADER.read_text()
        runtime = RUNTIME_SOURCE.read_text()
        narrative = NARRATIVE_SOURCE.read_text()
        self.assertIn("CitizenStoryTransitionRecords", records)
        self.assertIn("SourceActionId", records)
        self.assertIn("story.nia.human_override.supported", narrative)
        self.assertIn("Action->WorldTick <= Quest->LastTransitionWorldTick", narrative)
        self.assertIn("Crisis.WorldTick <= Action->WorldTick", narrative)
        self.assertIn("CommitNiaStoryTransition", runtime)
        self.assertNotIn("CitizenStoryStates.Add(TEXT(\"citizen.synara.nia_vale\")", runtime)

    def test_round_three_binding_cache_gate_and_replay_contracts_are_wired(self):
        records = CORE_RECORDS_HEADER.read_text()
        runtime = RUNTIME_SOURCE.read_text()
        pipeline = PIPELINE_SOURCE.read_text()
        self.assertIn("BindWorldTick", records)
        self.assertIn("QuestDefinitionFingerprint", records)
        self.assertIn("RequiredBindingKeys", SAVE_SOURCE.read_text())
        self.assertIn("GetPathName", pipeline)
        self.assertIn("WorldMapAuthorityRecords", records)
        self.assertIn("QuestEligibilityProofRecords", records)
        self.assertIn("Record.WorldTick == Tick", runtime)
        self.assertIn("FacilityWorldAssetId", runtime)

    def test_round_three_has_production_coordinator_and_authentic_v9_fixture(self):
        self.assertTrue(COORDINATOR_HEADER.exists())
        coordinator = COORDINATOR_SOURCE.read_text() + COORDINATOR_HEADER.read_text()
        self.assertIn("UDAFirstHourContentRegistrySubsystem", coordinator)
        self.assertIn("FDACampaignSnapshot", coordinator)
        self.assertIn("TryProgress", coordinator)
        self.assertIn("UDAWorldStateSubsystem", coordinator)
        migration = MIGRATION_TEST_SOURCE.read_text()
        self.assertIn("StripV10NarrativeFields", migration)
        self.assertIn("authentic schema v9", migration)

    def test_round_four_coordinator_uses_world_owned_campaign_and_clock(self):
        header = COORDINATOR_HEADER.read_text()
        source = COORDINATOR_SOURCE.read_text()
        world = WORLD_HEADER.read_text() + WORLD_SOURCE.read_text()
        self.assertIn("TWeakObjectPtr<UDAWorldStateSubsystem>", header)
        self.assertNotIn("FDACampaignSnapshot CampaignSnapshot", header)
        self.assertNotIn("FDACitySimulationState CityState", header)
        self.assertNotIn("int64 WorldTick", header)
        self.assertIn("TryCommitPersistentCampaign", world)
        self.assertIn("GetLiveSignals", world)
        self.assertIn("OnWorldTickStateCommitted", source)
        self.assertIn("HandleWorldTickStateCommitted", source)
        self.assertIn("ProgressPendingQuests", source)
        self.assertIn("GetPersistentCampaign", source)
        self.assertIn("CommitNiaStoryTransition", header)
        self.assertIn("RecordCrisisCompletion", header)

    def test_round_four_migration_preserves_historical_v9_boundary(self):
        source = MIGRATION_SOURCE.read_text()
        tests = MIGRATION_TEST_SOURCE.read_text()
        self.assertIn("StripFutureFieldsForHistoricalV9", source)
        case8 = source[source.index("case 8:"):source.index("case 9:")]
        self.assertIn("StripFutureFieldsForHistoricalV9", case8)
        self.assertNotIn("FDACampaignSnapshot PromotedSnapshot", case8)
        self.assertIn("raw schema v8", tests)
        self.assertIn("nontrivial v8", tests)

    def test_round_four_uses_typed_map_and_audit_authorities(self):
        records = CORE_RECORDS_HEADER.read_text()
        runtime = RUNTIME_SOURCE.read_text()
        self.assertIn("FDAWorldMapAuthorityRecord", records)
        self.assertIn("FDAAuditEligibilitySourceRecord", records)
        self.assertIn("WorldMapAuthorityRecords", records)
        self.assertIn("AuditEligibilitySourceRecords", records)
        self.assertNotIn("Reward.Type == EDAQuestContentUnlockType::CityMode)\n        {\n            FDAWorldMap", runtime)
        self.assertIn("Proof->SourceActionId == Context.VisionActionId", runtime)
        self.assertIn("Proof->SourceActionId == Context.ResearchActionId", runtime)

    def test_round_four_story_replay_precedes_monotonic_rejection(self):
        runtime = RUNTIME_SOURCE.read_text()
        body = runtime[runtime.index("CommitNiaStoryTransition"):runtime.index("IsNiaChampionEligible")]
        self.assertLess(body.index("ExistingTransition"), body.index("WorldTick <="))
        self.assertIn("ExistingTransition->WorldTick == WorldTick", body)

    def test_round_four_bindings_use_exact_node_transition_ticks(self):
        records = CORE_RECORDS_HEADER.read_text()
        save = SAVE_SOURCE.read_text()
        self.assertIn("FDAQuestNodeTransitionRecord", records)
        self.assertIn("NodeTransitionRecords", records)
        self.assertIn("ExpectedBindWorldTick", save)
        self.assertIn("Binding.BindWorldTick != ExpectedBindWorldTick", save)

    def test_round_four_generated_registry_has_strong_refs_and_rejects_transients(self):
        registry = REGISTRY_SOURCE.read_text()
        content = CONTENT_HEADER.read_text() + PIPELINE_SOURCE.read_text()
        self.assertIn("StoredQuestDefinitions", registry)
        self.assertIn("StoredCitizenDefinition", registry)
        self.assertIn("bRuntimeManifestFallback", content)
        self.assertIn("GetOutermost() == GetTransientPackage()", content)
        self.assertIn("generated cache object is transient", content)
        self.assertIn("Asset->GetPathName() == PackagePath + TEXT(\".\") + AssetName", content)

    def test_round_five_live_signals_have_one_persistent_owner_and_transactional_ingress(self):
        save = CORE_SAVE_HEADER.read_text()
        world = WORLD_HEADER.read_text() + WORLD_SOURCE.read_text()
        economy = ECONOMY_HEADER.read_text() + ECONOMY_SOURCE.read_text()
        coordinator = COORDINATOR_SOURCE.read_text()
        self.assertIn("FDACampaignLiveSignalState LiveSignals", save)
        self.assertNotIn("TryCommitLiveSignals", world)
        self.assertIn("SubmitCitizenSignal", world)
        self.assertIn("SubmitJobAssignmentSignal", world)
        self.assertIn("SubmitUtilitySignal", world)
        self.assertNotIn("GetCanonicalCityState", world)
        self.assertNotIn("GetCanonicalUtilityNetwork", world)
        self.assertNotIn("FDACitySimulationState CitySimulationState", economy)
        self.assertNotIn("GetMutableCitySimulationState", economy)
        self.assertIn("ResolveDevelopmentCycle", economy)
        self.assertIn("LiveSignals", coordinator)

    def test_round_five_bootstrap_and_reflected_action_ingress(self):
        world = WORLD_HEADER.read_text() + WORLD_SOURCE.read_text()
        coordinator = COORDINATOR_HEADER.read_text() + COORDINATOR_SOURCE.read_text()
        runtime = (ROOT / "Source/DominionWorld/Public/Narrative/DAFirstHourCampaignRuntime.h").read_text()
        self.assertIn("InitializeVerticalSliceState", world)
        self.assertIn("if (!PersistentCampaign.WorldState.bInitialized)", world)
        self.assertIn("UENUM(BlueprintType)\nenum class EDAFirstHourPlayerAction", runtime)
        self.assertIn("UENUM(BlueprintType)\nenum class EDAFirstHourCampaignResult", runtime)
        self.assertIn("UFUNCTION(BlueprintCallable", coordinator)
        self.assertIn("SubmitPlayerAction", coordinator)
        self.assertIn("SubmitNiaStoryAction", coordinator)
        self.assertIn("SubmitCrisisAction", coordinator)
        self.assertNotIn("int64 WorldTick", COORDINATOR_HEADER.read_text())

    def test_round_five_schema_v11_and_unconditional_v9_future_scan(self):
        self.assertIn("CurrentSchemaVersion = 19", SAVE_SCHEMA_HEADER.read_text())
        source = MIGRATION_SOURCE.read_text()
        self.assertIn("case 10:", source)
        case9 = source[source.index("case 9:"):source.index("case 10:")]
        self.assertNotIn("if (TransitionValue == nullptr) continue", case9)
        self.assertIn("worldAssetBindings", case9)
        tests = MIGRATION_TEST_SOURCE.read_text()
        self.assertIn("ComputeLegacyFingerprintV1", tests)
        self.assertIn("schema v11 live signals", tests)

    def test_round_five_core_freezes_all_nine_definition_fingerprints(self):
        records = CORE_RECORDS_HEADER.read_text()
        narrative = NARRATIVE_SOURCE.read_text()
        pipeline = PIPELINE_SOURCE.read_text()
        self.assertIn("FDAFirstHourFrozenPolicy", records)
        for quest_id in self.by_id:
            self.assertIn(quest_id, narrative)
        self.assertIn("ValidatePinnedQuestDefinition", narrative)
        self.assertIn("ValidatePinnedQuestDefinition", pipeline)
        self.assertIn(self.manifest["fingerprint"], narrative)


if __name__ == "__main__":
    unittest.main()
