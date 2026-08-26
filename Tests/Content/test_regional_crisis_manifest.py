import hashlib
import json
import pathlib
import os
import stat
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Content/DA/Manifests/RegionalCrisisCampaign.json"
SCHEMA = ROOT / "Content/DA/Manifests/RegionalCrisisCampaign.schema.json"


def canonical_fingerprint(document):
    material = dict(document)
    material.pop("fingerprint", None)
    canonical = json.dumps(material, ensure_ascii=False, separators=(",", ":"), sort_keys=True)
    return hashlib.sha1(canonical.encode("utf-8")).hexdigest()


class RegionalCrisisManifestTests(unittest.TestCase):
    def setUp(self):
        self.document = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def test_manifest_identity_and_fingerprint_are_deterministic(self):
        self.assertEqual(self.document["schemaVersion"], 1)
        self.assertEqual(self.document["campaignId"], "campaign.vertical_slice.regional_crisis")
        self.assertRegex(self.document["fingerprint"], r"^[0-9a-f]{40}$")
        self.assertEqual(self.document["fingerprint"], "1bc31247330a8bc0af7103aaa8b70b51d8cd5d7a")
        self.assertEqual(self.document["fingerprint"], canonical_fingerprint(self.document))
        mutated = json.loads(json.dumps(self.document))
        mutated["events"][0]["title"] += "!"
        self.assertNotEqual(canonical_fingerprint(mutated), self.document["fingerprint"])

    def test_exact_six_world_events_and_expected_asset_paths(self):
        expected = [
            ("event.foundry_shortage", "Foundry Shortage", "/Game/DA/Events/E_FoundryShortage"),
            ("event.grid_strain", "Grid Strain", "/Game/DA/Events/E_GridStrain"),
            ("event.housing_surge", "Housing Surge", "/Game/DA/Events/E_HousingSurge"),
            ("event.green_line", "Green Line", "/Game/DA/Events/E_GreenLine"),
            ("event.corridor_failure", "Corridor Failure", "/Game/DA/Events/E_CorridorFailure"),
            ("event.migration_wave", "Migration Wave", "/Game/DA/Events/E_MigrationWave"),
        ]
        self.assertEqual(
            [(row["id"], row["title"], row["assetPath"]) for row in self.document["events"]],
            expected,
        )

    def test_exact_ten_regional_quests_and_no_fake_currency_rewards(self):
        expected = [
            ("quest.tals_reservoir", "Tal's Reservoir"),
            ("quest.frontier_claim", "Frontier Claim"),
            ("quest.hold_the_ridge", "Hold the Ridge"),
            ("quest.first_contract", "First Contract"),
            ("quest.maras_numbers", "Mara's Numbers"),
            ("quest.green_line", "Green Line"),
            ("quest.empty_shift", "Empty Shift"),
            ("quest.price_of_silence", "The Price of Silence"),
            ("quest.human_override", "Human Override"),
            ("quest.foundry_shortage", "Foundry Shortage"),
        ]
        expected_paths = [
            "/Game/DA/Quests/Q_TalsReservoir", "/Game/DA/Quests/Q_FrontierClaim",
            "/Game/DA/Quests/Q_HoldTheRidge", "/Game/DA/Quests/Q_FirstContract",
            "/Game/DA/Quests/Q_MarasNumbers", "/Game/DA/Quests/Q_GreenLine",
            "/Game/DA/Quests/Q_EmptyShift", "/Game/DA/Quests/Q_PriceOfSilence",
            "/Game/DA/Quests/Q_HumanOverride", "/Game/DA/Quests/Q_FoundryShortage",
        ]
        self.assertEqual([(row["id"], row["title"]) for row in self.document["quests"]], expected)
        self.assertEqual([row["assetPath"] for row in self.document["quests"]], expected_paths)
        for quest in self.document["quests"]:
            self.assertNotIn("rewards", quest)
            self.assertNotIn("currency", json.dumps(quest).lower())

    def test_foundry_shortage_uses_only_frozen_threshold_timing_and_price_stages(self):
        foundry = self.document["events"][0]
        self.assertEqual(foundry["trigger"], {
            "metric": "forgeweave.resource_hunger", "comparison": "greater_than", "threshold": 70,
        })
        self.assertEqual(foundry["warningDurationWorldTicks"], 2)
        self.assertEqual(
            [(stage["id"], stage["priceModifier"]) for stage in foundry["stages"]],
            [("shortage_warning", 0.20), ("market_spike", 0.35),
             ("ecological_dispute", 0.35), ("emergency_overdrive", 0.60)],
        )
        self.assertEqual([row["id"] for row in foundry["resolutions"]], [
            "industrial_support", "eden_restriction", "brokered_compact",
            "market_exploitation", "collapse",
        ])
        for resolution in foundry["resolutions"]:
            self.assertIn(resolution["recoveryWorldTicks"], range(4, 9))
            for field in ("marketModifier", "tradeRouteCapacityDelta", "ecologyDelta",
                          "relationshipId", "relationshipMetric", "relationshipDelta",
                          "resourceHungerDelta", "historyTags", "citizenOutcomes"):
                self.assertIn(field, resolution)
            self.assertEqual(set(resolution["citizenOutcomes"]), {
                "citizen.neutral.tal_arden", "citizen.forgeweave.mara_kest",
                "citizen.eden.ori_sen",
            })

    def test_canonical_citizen_ids_are_used_by_every_quest_and_resolution(self):
        authored = json.dumps(self.document, sort_keys=True)
        self.assertNotIn("citizen.synara.tal_arden", authored)
        self.assertNotIn("citizen.forgeweave.mara_vek", authored)
        self.assertIn("citizen.neutral.tal_arden", authored)
        self.assertIn("citizen.forgeweave.mara_kest", authored)
        self.assertIn("citizen.eden.ori_sen", authored)

    def test_primary_asset_scan_and_cook_rules_cover_real_generated_packages(self):
        config = (ROOT / "Config/DefaultGame.ini").read_text(encoding="utf-8")
        self.assertIn('PrimaryAssetType="DARegionalWorldEvent"', config)
        self.assertIn('AssetBaseClass="/Script/DominionWorld.DARegionalWorldEventDefinition"', config)
        self.assertIn('PrimaryAssetType="DARegionalQuest"', config)
        self.assertIn('AssetBaseClass="/Script/DominionWorld.DARegionalQuestDefinition"', config)
        self.assertIn('(Path="/Game/DA/Events")', config)
        self.assertIn('(Path="/Game/DA/Quests")', config)
        self.assertGreaterEqual(config.count("CookRule=AlwaysCook"), 6)

    def test_schema_is_strict_at_every_authored_record_boundary(self):
        schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        self.assertFalse(schema["additionalProperties"])
        for definition in schema["$defs"].values():
            if definition.get("type") == "object":
                self.assertFalse(definition["additionalProperties"])

    def test_repository_contains_no_checked_in_placeholder_uassets(self):
        expected_package_stems = {
            pathlib.PurePosixPath(row["assetPath"]).name
            for row in self.document["events"] + self.document["quests"]
        }
        checked_in = {path.stem for path in (ROOT / "Content").rglob("*.uasset")}
        self.assertTrue(expected_package_stems.isdisjoint(checked_in))

    def test_precook_generates_regional_packages_before_validate_only(self):
        script = ROOT / "Build/Scripts/PreCookRegionalCrisis.sh"
        graph = (ROOT / "Build/Graph/VerticalSliceRegionalCrisis.xml").read_text(encoding="utf-8")
        self.assertIn("PreCookRegionalCrisis.sh", graph)
        self.assertLess(graph.index("GenerateRegionalCrisisAssets"), graph.index("ValidateRegionalCrisisAssets"))

        with tempfile.TemporaryDirectory() as directory:
            directory = pathlib.Path(directory)
            log = directory / "calls.log"
            fake_editor = directory / "fake-editor"
            fake_editor.write_text(
                "#!/usr/bin/env sh\nprintf '%s\\n' \"$*\" >> \"$DA_TEST_EDITOR_LOG\"\n",
                encoding="utf-8",
            )
            fake_editor.chmod(fake_editor.stat().st_mode | stat.S_IXUSR)
            environment = dict(os.environ, UNREAL_EDITOR_CMD=str(fake_editor), DA_TEST_EDITOR_LOG=str(log))
            subprocess.run(["bash", str(script), "DominionAscendant.uproject"], cwd=ROOT,
                           env=environment, check=True)
            calls = log.read_text(encoding="utf-8").splitlines()
            self.assertEqual(len(calls), 2)
            self.assertIn("-run=DARegionalCrisisContent", calls[0])
            self.assertNotIn("-ValidateOnly", calls[0])
            self.assertIn("-run=DARegionalCrisisContent", calls[1])
            self.assertIn("-ValidateOnly", calls[1])

    def test_all_events_author_exact_persistent_lifecycle_graphs(self):
        expected_triggers = {
            "event.foundry_shortage": ("forgeweave.resource_hunger", "greater_than", 70),
            "event.grid_strain": ("city.power_reserve_percent", "less_than", 5),
            "event.housing_surge": ("city.attractiveness_and_vacancy", "greater_than", 75),
            "event.green_line": ("eden.ecological_balance", "less_than", 70),
            "event.corridor_failure": ("route.transit_wear", "greater_than", 75),
            "event.migration_wave": ("region.neighbor_stability", "less_than", 30),
        }
        for event in self.document["events"]:
            trigger = event["trigger"]
            self.assertEqual((trigger["metric"], trigger["comparison"], trigger["threshold"]),
                             expected_triggers[event["id"]])
            self.assertIn("initialStageId", event)
            self.assertIn("ignoredResolutionId", event)
            self.assertTrue(event["stages"])
            stage_ids = {stage["id"] for stage in event["stages"]}
            resolution_ids = {resolution["id"] for resolution in event["resolutions"]}
            self.assertIn(event["initialStageId"], stage_ids)
            self.assertIn(event["ignoredResolutionId"], resolution_ids)
            for stage in event["stages"]:
                self.assertTrue(stage["nextStageIds"])
                self.assertTrue(set(stage["nextStageIds"]).issubset(stage_ids | resolution_ids))

    def test_all_quests_author_runtime_nodes_edges_and_choice_specific_outcomes(self):
        expected_triggers = {
            "quest.tals_reservoir": "reservoir_repair_requested",
            "quest.frontier_claim": "unclaimed_resource_zone_discovered",
            "quest.hold_the_ridge": "frontier_claim_encounter_escalated",
            "quest.first_contract": "trade.active",
            "quest.maras_numbers": "forgeweave.trust>30|trade.delivery_fulfilled",
            "quest.green_line": "event.green_line",
            "quest.empty_shift": "forgeweave.resource_hunger>60",
            "quest.price_of_silence": "mara_numbers_quiet",
            "quest.human_override": "synara.dependency>50|automation_incident",
            "quest.foundry_shortage": "event.foundry_shortage",
        }
        for quest in self.document["quests"]:
            self.assertEqual(quest["trigger"], expected_triggers[quest["id"]])
            self.assertEqual(set(quest["choiceOutcomeTags"]), set(quest["choices"]))
            nodes = {node["id"]: node for node in quest["nodes"]}
            self.assertIn("start", nodes)
            self.assertIn("choice", nodes)
            self.assertEqual(nodes["start"]["type"], "start")
            self.assertEqual(nodes["choice"]["type"], "choice")
            self.assertEqual({edge["branch"] for edge in nodes["choice"]["edges"]},
                             set(quest["choices"]))
            for choice in quest["choices"]:
                resolution_id = f"resolution.{choice}"
                self.assertIn(resolution_id, nodes)
                self.assertEqual(nodes[resolution_id]["type"], "resolution")


if __name__ == "__main__":
    unittest.main()
