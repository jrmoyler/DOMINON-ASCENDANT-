import collections
import json
import pathlib
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
MANIFEST_PATH = REPO_ROOT / "Content/DA/Manifests/VerticalSliceContent.json"
SCHEMA_PATH = REPO_ROOT / "Content/DA/Manifests/VerticalSliceContent.schema.json"
EXPECTED_COUNTS = {
    "Synara": 15,
    "Forgeweave": 15,
    "EdenCircuit": 15,
    "Universal": 17,
    "Fusion": 1,
    "Special": 1,
}
EXPECTED_STARTER = {
    "synara.adaptive_habitat": 3,
    "synara.civic_autonomy_pods": 3,
    "universal.compact_residence": 3,
    "universal.civic_apartments": 3,
    "synara.autonomous_exchange": 3,
    "synara.algorithmic_market": 2,
    "universal.corner_exchange": 2,
    "universal.district_market": 2,
    "synara.cognitive_operations_tower": 3,
    "synara.predictive_bureau": 2,
    "universal.research_annex": 2,
    "universal.administrative_office": 1,
    "synara.synthetic_fabrication_node": 3,
    "synara.swarm_foundry": 2,
    "universal.field_workshop": 1,
    "universal.utility_fabricator": 1,
    "synara.neural_relay": 3,
    "synara.orchestration_hub": 2,
    "universal.microgrid_station": 2,
    "universal.water_reclaimer": 1,
    "universal.transit_stop": 1,
    "universal.warehouse": 1,
    "synara.agency_forum": 3,
    "universal.community_plaza": 1,
    "universal.public_clinic": 1,
    "universal.founder_monument": 1,
    "synara.guardian_drone_cohort": 2,
    "synara.audit_sentinel": 2,
    "universal.watch_post": 1,
    "universal.barrier_hub": 1,
    "synara.archon_mira_vey": 1,
    "synara.the_thinking_spire": 1,
}
EXPECTED_AUTHORED_VALUES = {
    "synara.adaptive_habitat": {
        "deploymentCapital": 8, "craftCapital": 4, "maintenanceCapitalPerCycle": 0.04,
    },
    "synara.autonomous_exchange": {"synaraDependencyPerCycle": 0.10},
    "synara.guardian_drone_cohort": {
        "craftCapital": 8, "craftInsight": 2, "craftProductionThroughput": 5,
        "requiredCraftingFacilityId": "synara.synthetic_fabrication_node", "constructionCycles": 2,
    },
    "synara.agency_forum": {"synaraDependencyPerCycle": -0.20},
    "synara.the_thinking_spire": {
        "deploymentCapital": 220, "deploymentInsight": 30, "deploymentInfluence": 25,
        "synaraDependencyPerCycle": 0.35, "constructionCycles": 12,
    },
    "forgeweave.the_grand_forge": {
        "deploymentCapital": 240, "deploymentInsight": 20, "deploymentInfluence": 20,
        "constructionCycles": 14,
    },
    "eden.the_worldgarden": {
        "deploymentCapital": 200, "deploymentInsight": 28, "deploymentInfluence": 30,
        "constructionCycles": 14,
    },
    "fusion.autonomous_factory": {
        "deploymentInsight": 24, "craftCapital": 80, "craftInsight": 0,
        "requiredCraftingFacilityId": "forgeweave.replication_forge",
        "synaraDependencyPerCycle": 0.20,
        "forgeweaveResourceHungerPerCycle": 0.15, "workforceRequirementModifier": -0.80,
        "industrialThroughputModifier": 0.25,
        "adjacentIndustrialConstructionSpeedModifier": 0.15, "constructionCycles": 8,
        "utilityPower": 24, "utilityData": 24,
    },
    "special.founder_hall": {
        "baseCapitalPerCycle": 1, "baseInsightPerCycle": 0.15,
        "baseInfluencePerCycle": 0.10, "housingCapacity": 24,
    },
}


class VerticalSliceManifestStaticTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        cls.schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))

    def test_schema_freezes_definition_and_deck_row_counts(self):
        definitions = self.schema["properties"]["definitions"]
        deck = self.schema["properties"]["starterDeck"]
        self.assertEqual((definitions["minItems"], definitions["maxItems"]), (64, 64))
        self.assertEqual((deck["minItems"], deck["maxItems"]), (32, 32))
        self.assertEqual(self.schema["x-dominion-exactFactionCounts"], EXPECTED_COUNTS)
        self.assertEqual(self.schema["x-dominion-exactStarterInstances"], 60)
        self.assertEqual(len(set(self.schema["x-dominion-authoredValueFields"])), 21)

    def test_manifest_has_exact_frozen_counts_and_unique_identity(self):
        definitions = self.manifest["definitions"]
        self.assertEqual(len(definitions), 64)
        self.assertEqual(collections.Counter(d["faction"] for d in definitions), EXPECTED_COUNTS)
        self.assertEqual(len({d["id"] for d in definitions}), 64)
        self.assertEqual(len({d["displayName"] for d in definitions}), 64)
        self.assertEqual(sum(entry["quantity"] for entry in self.manifest["starterDeck"]), 60)

    def test_starter_deck_has_every_exact_section_22_quantity(self):
        actual = {entry["definitionId"]: entry["quantity"] for entry in self.manifest["starterDeck"]}
        self.assertEqual(actual, EXPECTED_STARTER)

    def test_manifest_rows_match_the_checked_in_schema_contract(self):
        definition_schema = self.schema["$defs"]["definition"]
        required = set(definition_schema["required"])
        allowed = set(definition_schema["properties"])
        for definition in self.manifest["definitions"]:
            self.assertFalse(required - set(definition), definition["id"])
            self.assertFalse(set(definition) - allowed, definition["id"])

        deck_schema = self.schema["$defs"]["deckEntry"]
        for entry in self.manifest["starterDeck"]:
            self.assertFalse(set(deck_schema["required"]) - set(entry), entry["definitionId"])
            self.assertFalse(set(entry) - set(deck_schema["properties"]), entry["definitionId"])

    def test_every_supplied_gameplay_value_is_declared_authored(self):
        value_fields = set(self.schema["x-dominion-authoredValueFields"])
        for definition in self.manifest["definitions"]:
            authored = set(definition["authoredValues"])
            supplied = set(definition) & value_fields
            self.assertEqual(supplied, authored, definition["id"])

        founder = next(d for d in self.manifest["definitions"] if d["id"] == "special.founder_hall")
        self.assertIn("housingCapacity", founder["authoredValues"])
        self.assertEqual(founder["housingCapacity"], 24)

    def test_tags_and_upgrade_branches_are_explicit_and_combat_is_strictly_optional(self):
        definition_schema = self.schema["$defs"]["definition"]
        self.assertIn("tags", definition_schema["required"])
        self.assertIn("upgradeBranchIds", definition_schema["required"])
        combat_schema = self.schema["$defs"]["combat"]
        self.assertFalse(combat_schema["additionalProperties"])
        self.assertEqual(
            set(combat_schema["required"]),
            {"structuralIntegrity", "armor", "cyberIntegrity", "capturable"},
        )
        for definition in self.manifest["definitions"]:
            self.assertEqual(definition["tags"], [], definition["id"])
            self.assertEqual(definition["upgradeBranchIds"], [], definition["id"])
            self.assertNotIn("combat", definition, definition["id"])

    def test_exact_v08_authored_values_and_no_invented_values(self):
        value_fields = set(self.schema["x-dominion-authoredValueFields"])
        actual = {}
        for definition in self.manifest["definitions"]:
            values = {field: definition[field] for field in value_fields if field in definition}
            if values:
                actual[definition["id"]] = values
        self.assertEqual(actual, EXPECTED_AUTHORED_VALUES)

    def test_custom_strict_schema_checks_nested_keys_and_json_types(self):
        root_allowed = set(self.schema["properties"])
        self.assertFalse(set(self.manifest) - root_allowed)
        self.assertEqual(type(self.manifest["schemaVersion"]), int)

        authority_schema = self.schema["$defs"]["authority"]
        authority = self.manifest["authority"]
        self.assertFalse(set(authority_schema["required"]) - set(authority))
        self.assertFalse(set(authority) - set(authority_schema["properties"]))
        self.assertTrue(all(type(value) is str for value in authority.values()))

        counts_schema = self.schema["$defs"]["expectedCounts"]
        counts = self.manifest["expectedCounts"]
        self.assertFalse(set(counts_schema["required"]) - set(counts))
        self.assertFalse(set(counts) - set(counts_schema["properties"]))
        self.assertTrue(all(type(value) is int for value in counts.values()))

        definition_schema = self.schema["$defs"]["definition"]
        for definition in self.manifest["definitions"]:
            for key, value in definition.items():
                expected_type = definition_schema["properties"][key].get("type")
                if expected_type == "integer":
                    self.assertEqual(type(value), int, f"{definition['id']}:{key}")
                elif expected_type == "number":
                    self.assertIn(type(value), (int, float), f"{definition['id']}:{key}")
                elif expected_type == "boolean":
                    self.assertEqual(type(value), bool, f"{definition['id']}:{key}")
                elif expected_type == "string":
                    self.assertEqual(type(value), str, f"{definition['id']}:{key}")

    def test_manifest_static_safety_constraints(self):
        definitions = self.manifest["definitions"]
        definition_ids = {definition["id"] for definition in definitions}
        self.assertTrue(all(entry["definitionId"] in definition_ids for entry in self.manifest["starterDeck"]))
        self.assertNotIn("special.founder_hall", {entry["definitionId"] for entry in self.manifest["starterDeck"]})
        self.assertTrue(all(not definition["placeable"] or definition["worldPrefab"] for definition in definitions))
        cost_fields = (
            "deploymentCapital", "deploymentInsight", "deploymentInfluence",
            "craftCapital", "craftInsight", "craftProductionThroughput",
            "maintenanceCapitalPerCycle",
        )
        self.assertTrue(all(definition[field] >= 0 for definition in definitions for field in cost_fields if field in definition))
        self.assertTrue(all(not (definition["rarity"] == "Dominion" and definition["randomCacheEligible"])
                            for definition in definitions))


if __name__ == "__main__":
    unittest.main()
