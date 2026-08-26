import hashlib
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Content/DA/Manifests/DaxtonEncounter.json"
SCHEMA = ROOT / "Content/DA/Manifests/DaxtonEncounter.schema.json"
PACKAGING = ROOT / "Config/DefaultGame.ini"
PRODUCTION_COOK_GRAPH = ROOT / "Build/Graph/VerticalSliceForgeweaveConquest.xml"


def canonical_fingerprint(document: dict) -> str:
    material = dict(document)
    material.pop("fingerprint", None)
    return hashlib.sha1(
        json.dumps(material, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


class DaxtonEncounterManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(MANIFEST.read_text(encoding="utf-8")) if MANIFEST.exists() else {}

    def setUp(self):
        self.assertTrue(MANIFEST.exists(), "Daxton encounter manifest is missing")
        self.assertTrue(SCHEMA.exists(), "Daxton encounter schema is missing")

    def test_frozen_identity_and_exact_generation_paths(self):
        self.assertEqual(self.document["schemaVersion"], 1)
        self.assertEqual(self.document["contentId"], "encounter.daxton_rhe.final")
        self.assertEqual(self.document["fingerprint"], canonical_fingerprint(self.document))
        self.assertEqual(
            self.document["leaderAssetPath"],
            "/Game/DA/Leaders/DA_Leader_DaxtonRhe",
        )
        self.assertEqual(
            [row["assetPath"] for row in self.document["characterImports"]],
            [
                "/Game/Characters/Daxton/Meshes/SK_DaxtonRhe",
                "/Game/Characters/Daxton/Animations/A_Daxton_PoweredArmor",
                "/Game/Characters/Daxton/Animations/A_Daxton_Overdrive",
                "/Game/Characters/Daxton/Animations/A_Daxton_Choice",
            ],
        )

    def test_character_packages_require_real_external_source_imports(self):
        expected_sources = [
            "ContentSource/Characters/Daxton/DaxtonRhe.fbx",
            "ContentSource/Characters/Daxton/DaxtonRhe_PoweredArmor.fbx",
            "ContentSource/Characters/Daxton/DaxtonRhe_Overdrive.fbx",
            "ContentSource/Characters/Daxton/DaxtonRhe_Choice.fbx",
        ]
        self.assertEqual(
            [row["sourcePath"] for row in self.document["characterImports"]],
            expected_sources,
        )
        self.assertEqual(
            [row["assetClass"] for row in self.document["characterImports"]],
            ["SkeletalMesh", "AnimSequence", "AnimSequence", "AnimSequence"],
        )
        generated_packages = [self.document["leaderAssetPath"]] + [
            row["assetPath"] for row in self.document["characterImports"]
        ]
        for package in generated_packages:
            relative = package.removeprefix("/Game/") + ".uasset"
            self.assertFalse(
                (ROOT / "Content" / relative).exists(),
                f"binary placeholder must not be checked in: {relative}",
            )

    def test_generation_is_registered_with_the_production_cook_and_asset_manager(self):
        self.assertTrue((ROOT / "Build/Scripts/PreCookDaxtonEncounter.sh").exists())
        self.assertTrue((ROOT / "Build/Graph/VerticalSliceDaxtonEncounter.xml").exists())
        packaging = PACKAGING.read_text(encoding="utf-8")
        self.assertIn('+DirectoriesToAlwaysCook=(Path="/Game/DA/Leaders")', packaging)
        self.assertIn('+DirectoriesToAlwaysCook=(Path="/Game/Characters/Daxton")', packaging)
        self.assertIn('PrimaryAssetType="DALeader"', packaging)
        self.assertIn('AssetBaseClass="/Script/DominionGameplay.DADaxtonLeaderDefinition"', packaging)
        self.assertIn('Directories=((Path="/Game/DA/Leaders"))', packaging)
        self.assertIn('CookRule=AlwaysCook', packaging)
        production_graph = PRODUCTION_COOK_GRAPH.read_text(encoding="utf-8")
        self.assertIn("PreCookDaxtonEncounter.sh", production_graph)
        self.assertIn("ValidateDaxtonEncounterAssets", production_graph)

    def test_task_does_not_change_locked_definition_or_ui_counts(self):
        vertical = json.loads(
            (ROOT / "Content/DA/Manifests/VerticalSliceContent.json").read_text()
        )
        ui = json.loads(
            (ROOT / "Content/UI/Manifests/VerticalSliceUI.json").read_text()
        )
        first = json.loads(
            (ROOT / "Content/DA/Manifests/FirstHourQuests.json").read_text()
        )
        regional = json.loads(
            (ROOT / "Content/DA/Manifests/RegionalCrisisCampaign.json").read_text()
        )
        conquest = json.loads(
            (ROOT / "Content/DA/Manifests/ForgeweaveConquest.json").read_text()
        )
        self.assertEqual(
            vertical["expectedCounts"],
            {
                "Synara": 15,
                "Forgeweave": 15,
                "EdenCircuit": 15,
                "Universal": 17,
                "Fusion": 1,
                "Special": 1,
                "total": 64,
                "starterInstances": 60,
            },
        )
        self.assertEqual(len(vertical["definitions"]), 64)
        ascension = json.loads(
            (ROOT / "Content/DA/Manifests/FirstAscension.json").read_text()
        )
        self.assertEqual(
            len(first["quests"]) + len(regional["quests"]) + len(conquest["quests"])
            + (1 if ascension["quest"] else 0),
            25,
        )
        self.assertEqual(len(regional["events"]), 6)
        self.assertEqual(len(ui["screens"]), 27)


if __name__ == "__main__":
    unittest.main()
