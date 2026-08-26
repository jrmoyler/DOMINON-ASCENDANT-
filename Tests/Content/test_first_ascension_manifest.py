import hashlib
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Content/DA/Manifests/FirstAscension.json"
SCHEMA = ROOT / "Content/DA/Manifests/FirstAscension.schema.json"
SHOT_SCHEMA = ROOT / "ContentSource/Cinematics/ForgeweaveAscension.shotlist.schema.json"

EXPECTED_SHOTS = [
    {
        "beat": "systems_halt_react",
        "sequenceAssetPath": "/Game/Cinematics/ForgeweaveAscension/Shots/LS_01_SystemsHaltReact",
        "durationFrames": 90,
        "camera": {
            "startLocation": [0, -1200, 300], "endLocation": [0, -650, 240],
            "startRotation": [-6, 0, 0], "endRotation": [-3, 0, 0], "fieldOfView": 55,
        },
        "actions": [
            {"frame": 0, "actionId": "systems.halt", "targetId": "world.city_systems", "payload": "halt_nonessential_systems"},
            {"frame": 18, "actionId": "citizens.react", "targetId": "world.population", "payload": "look_toward_grand_forge"},
        ],
        "audioCues": [
            {"frame": 0, "cueId": "audio.ascension.systems_halt"},
            {"frame": 30, "cueId": "audio.ascension.crowd_reaction"},
        ],
        "vfxCues": [
            {"frame": 0, "cueId": "vfx.ascension.grid_power_falloff"},
            {"frame": 24, "cueId": "vfx.ascension.forge_glow_first_pulse"},
        ],
    },
    {
        "beat": "forge_relic_emerges",
        "sequenceAssetPath": "/Game/Cinematics/ForgeweaveAscension/Shots/LS_02_ForgeRelicEmerges",
        "durationFrames": 120,
        "camera": {
            "startLocation": [900, -450, 260], "endLocation": [260, -240, 180],
            "startRotation": [-8, 135, 0], "endRotation": [-4, 155, 0], "fieldOfView": 45,
        },
        "actions": [
            {"frame": 0, "actionId": "forge.freeze", "targetId": "forgeweave.grand_forge", "payload": "suspend_hammers_and_belts"},
            {"frame": 24, "actionId": "relic.forge.emerge", "targetId": "relic.forge", "payload": "rise_from_forge_heart"},
            {"frame": 90, "actionId": "relic.forge.lock", "targetId": "relic.forge", "payload": "hold_for_transit"},
        ],
        "audioCues": [
            {"frame": 0, "cueId": "audio.ascension.forge_rumble"},
            {"frame": 24, "cueId": "audio.ascension.relic_reveal"},
        ],
        "vfxCues": [
            {"frame": 0, "cueId": "vfx.ascension.forge_embers_suspend"},
            {"frame": 24, "cueId": "vfx.ascension.forge_relic_materialize"},
        ],
    },
    {
        "beat": "world_transit",
        "sequenceAssetPath": "/Game/Cinematics/ForgeweaveAscension/Shots/LS_03_WorldTransit",
        "durationFrames": 90,
        "camera": {
            "startLocation": [200, -200, 180], "endLocation": [0, 1600, 500],
            "startRotation": [-2, 170, 0], "endRotation": [-12, 180, 0], "fieldOfView": 60,
        },
        "actions": [
            {"frame": 0, "actionId": "world.transit.begin", "targetId": "relic.forge", "payload": "depart_ironheart"},
            {"frame": 72, "actionId": "world.transit.arrive", "targetId": "special.founder_hall", "payload": "approach_founder_hall"},
        ],
        "audioCues": [
            {"frame": 0, "cueId": "audio.ascension.transit_surge"},
            {"frame": 72, "cueId": "audio.ascension.hall_approach"},
        ],
        "vfxCues": [
            {"frame": 0, "cueId": "vfx.ascension.relic_transit_ribbon"},
            {"frame": 72, "cueId": "vfx.ascension.founder_hall_portal"},
        ],
    },
    {
        "beat": "founder_hall_receives_relic",
        "sequenceAssetPath": "/Game/Cinematics/ForgeweaveAscension/Shots/LS_04_FounderHallReceivesRelic",
        "durationFrames": 120,
        "camera": {
            "startLocation": [0, -900, 260], "endLocation": [0, -350, 190],
            "startRotation": [-5, 0, 0], "endRotation": [-2, 0, 0], "fieldOfView": 48,
        },
        "actions": [
            {"frame": 30, "actionId": "founder_hall.receive_relic", "targetId": "founder_hall.slot.01", "payload": "occupy_with_forge_relic"},
            {"frame": 78, "actionId": "founder_hall.open_hidden_chamber", "targetId": "special.founder_hall", "payload": "reveal_convergence_chamber"},
        ],
        "audioCues": [
            {"frame": 30, "cueId": "audio.ascension.hall_resonance"},
            {"frame": 78, "cueId": "audio.ascension.hidden_chamber_unlock"},
        ],
        "vfxCues": [
            {"frame": 30, "cueId": "vfx.ascension.relic_plinth_ignite"},
            {"frame": 78, "cueId": "vfx.ascension.hidden_chamber_reveal"},
        ],
    },
    {
        "beat": "unlocks",
        "sequenceAssetPath": "/Game/Cinematics/ForgeweaveAscension/Shots/LS_05_Unlocks",
        "durationFrames": 90,
        "camera": {
            "startLocation": [0, -500, 230], "endLocation": [0, -700, 300],
            "startRotation": [-3, 0, 0], "endRotation": [-8, 0, 0], "fieldOfView": 52,
        },
        "actions": [
            {"frame": 0, "actionId": "unlocks.forgeweave_cards", "targetId": "collection.forgeweave", "payload": "expose_fifteen_definitions"},
            {"frame": 30, "actionId": "unlocks.replication_doctrine", "targetId": "doctrine.replication", "payload": "enable_twelve_cycle_replication"},
            {"frame": 60, "actionId": "unlocks.convergence_authority", "targetId": "founder_hall.authority", "payload": "show_one_of_twenty"},
        ],
        "audioCues": [
            {"frame": 0, "cueId": "audio.ascension.unlock_resolution"},
            {"frame": 60, "cueId": "audio.ascension.convergence_sting"},
        ],
        "vfxCues": [
            {"frame": 0, "cueId": "vfx.ascension.forgeweave_cards_unfurl"},
            {"frame": 60, "cueId": "vfx.ascension.founder_hall_slot_one"},
        ],
    },
]


def fingerprint(document: dict) -> str:
    material = dict(document)
    material.pop("fingerprint", None)
    return hashlib.sha1(
        json.dumps(material, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


class FirstAscensionManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(MANIFEST.read_text()) if MANIFEST.exists() else {}

    def setUp(self):
        self.assertTrue(MANIFEST.exists(), "first Ascension manifest is missing")
        self.assertTrue(SCHEMA.exists(), "first Ascension schema is missing")

    def test_exact_reward_identity_and_twenty_fifth_quest(self):
        self.assertEqual(self.document["schemaVersion"], 1)
        self.assertEqual(self.document["contentId"], "ascension.forgeweave.first")
        self.assertEqual(self.document["fingerprint"], fingerprint(self.document))
        self.assertEqual(
            self.document["quest"],
            {
                "id": "quest.convergence_authority",
                "title": "Convergence Authority",
                "assetPath": "/Game/DA/Quests/Q_ConvergenceAuthority",
                "completionHistory": "convergence_authority_1_of_20",
            },
        )
        first = json.loads((ROOT / "Content/DA/Manifests/FirstHourQuests.json").read_text())
        regional = json.loads((ROOT / "Content/DA/Manifests/RegionalCrisisCampaign.json").read_text())
        conquest = json.loads((ROOT / "Content/DA/Manifests/ForgeweaveConquest.json").read_text())
        self.assertEqual(
            len(first["quests"]) + len(regional["quests"]) + len(conquest["quests"]) + 1,
            25,
        )

    def test_exact_unlocks_and_factory_values(self):
        self.assertEqual(
            self.document["doctrine"],
            {
                "id": "doctrine.replication",
                "assetPath": "/Game/DA/Doctrines/DA_Doctrine_Replication",
                "cadenceDevelopmentCycles": 12,
            },
        )
        self.assertEqual(
            self.document["fusion"],
            {
                "definitionId": "fusion.autonomous_factory",
                "assetPath": "/Game/DA/Cards/Fusion/DA_Card_AutonomousFactory",
                "constructionCycles": 8,
                "craftCapital": 80,
                "craftInsight": 0,
                "requiredCraftingFacilityId": "forgeweave.replication_forge",
                "utilityPower": 24,
                "utilityData": 24,
                "workforceRequirementModifier": -0.80,
                "industrialThroughputModifier": 0.25,
                "adjacentIndustrialConstructionSpeedModifier": 0.15,
                "dependencyPerCycle": 0.20,
                "resourceHungerPerCycle": 0.15,
            },
        )
        vertical = json.loads((ROOT / "Content/DA/Manifests/VerticalSliceContent.json").read_text())
        forge = [row["id"] for row in vertical["definitions"] if row["faction"] == "Forgeweave"]
        self.assertEqual(self.document["forgeweaveDefinitionIds"], forge)
        self.assertEqual(len(forge), 15)

    def test_real_building_sources_and_generated_cinematic_are_fail_closed(self):
        self.assertEqual(
            [row["assetPath"] for row in self.document["buildingImports"]],
            [
                "/Game/Buildings/Fusion/AutonomousFactory/Meshes/SM_AutonomousFactory",
                "/Game/Buildings/Fusion/AutonomousFactory/Textures/T_AutonomousFactory",
            ],
        )
        for row in self.document["buildingImports"]:
            self.assertTrue(row["sourcePath"].startswith("ContentSource/Buildings/Fusion/AutonomousFactory/"))
        self.assertEqual(
            self.document["cinematic"],
            {
                "assetPath": "/Game/Cinematics/CS_ForgeweaveAscension",
                "shotSourcePath": "ContentSource/Cinematics/ForgeweaveAscension.shotlist.json",
                "gameplayGate": False,
            },
        )
        shot_source = ROOT / self.document["cinematic"]["shotSourcePath"]
        self.assertTrue(shot_source.exists(), "complete cinematic source is missing")
        self.assertTrue(SHOT_SCHEMA.exists(), "cinematic source schema is missing")
        self.assertEqual(
            json.loads(shot_source.read_text()),
            {
                "schemaVersion": 1,
                "cinematicId": "cinematic.forgeweave.first_ascension",
                "displayRate": 30,
                "shots": EXPECTED_SHOTS,
            },
        )
        shot_schema = json.loads(SHOT_SCHEMA.read_text())
        self.assertFalse(shot_schema["additionalProperties"])
        self.assertEqual(
            (shot_schema["properties"]["shots"]["minItems"],
             shot_schema["properties"]["shots"]["maxItems"]),
            (5, 5),
        )
        for package in [
            self.document["quest"]["assetPath"],
            self.document["doctrine"]["assetPath"],
            self.document["fusion"]["assetPath"],
            self.document["cinematic"]["assetPath"],
            *[row["assetPath"] for row in self.document["buildingImports"]],
        ]:
            self.assertFalse((ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")).exists())

    def test_generation_is_in_the_production_cook(self):
        script = ROOT / "Build/Scripts/PreCookFirstAscension.sh"
        cinematic_script = ROOT / "Build/Scripts/GenerateFirstAscensionCinematic.sh"
        graph = ROOT / "Build/Graph/VerticalSliceForgeweaveConquest.xml"
        commandlet = ROOT / "Source/DominionEditor/Private/DAFirstAscensionContentCommandlet.cpp"
        pipeline = ROOT / "Source/DominionWorld/Private/Ascension/DAAscensionContent.cpp"
        config = (ROOT / "Config/DefaultGame.ini").read_text()
        self.assertTrue(script.exists())
        self.assertTrue(cinematic_script.exists())
        self.assertTrue(commandlet.exists())
        self.assertTrue(pipeline.exists())
        self.assertIn("-run=DAContentManifest", script.read_text())
        cinematic_script_source = cinematic_script.read_text()
        self.assertIn("-run=DAFirstAscensionContent", cinematic_script_source)
        self.assertIn("-CinematicOnly", cinematic_script_source)
        self.assertIn("-ValidateOnly", cinematic_script_source)
        self.assertIn("PreCookFirstAscension.sh", graph.read_text())
        commandlet_source = commandlet.read_text()
        for marker in [
            "MissingSource:",
            "UAssetImportTask",
            "ULevelSequence",
            "UMovieSceneCinematicShotTrack",
            "UMovieSceneCinematicShotSection",
            "ACineCameraActor",
            "UMovieSceneCameraCutTrack",
            "UMovieScene3DTransformTrack",
            "UMovieSceneSpawnTrack",
            "AddSequence",
            "AddLinearKey",
            "AddMarkedFrame",
            "GetMarkedFrames",
            "SetMarkedFramesLocked",
            "GenerateShotSequence",
            "ValidateGeneratedShot",
            "DA.CinematicPayloadFingerprint",
            "CinematicOnly",
            "ExpectedAscensionBeats",
            "durationFrames",
            "sequenceAssetPath",
            "ValidateRegistry",
            "ValidateOnly",
            "SourceFingerprint",
            "ValidateFusion",
        ]:
            self.assertIn(marker, commandlet_source)
        pipeline_source = pipeline.read_text()
        self.assertIn("ExactKeys", pipeline_source)
        self.assertIn(self.document["fingerprint"], pipeline_source)
        for path in ["/Game/DA/Doctrines", "/Game/Buildings/Fusion/AutonomousFactory", "/Game/Cinematics"]:
            self.assertIn(f'+DirectoriesToAlwaysCook=(Path="{path}")', config)


if __name__ == "__main__":
    unittest.main()
