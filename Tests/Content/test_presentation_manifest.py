import copy
import hashlib
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Content/DA/Manifests/VerticalSlicePresentation.json"
SOURCES = {
    "buildings": ROOT / "Content/Buildings/Presentation/PrimaryAssets.json",
    "characters": ROOT / "Content/Characters/Presentation/PrimaryAssets.json",
    "vfx": ROOT / "Content/VFX/Presentation/CoreVFX.json",
    "audio": ROOT / "Content/Audio/Presentation/VerticalSliceAudio.json",
    "bindings": ROOT / "Content/Buildings/Presentation/FactionBindings.json",
    "artifacts": ROOT / "Content/DA/Manifests/PresentationArtifactSources.json",
}
TOOL = ROOT / "Build/Tools/PresentationManifestTool.py"
NIAGARA_VALIDATION_NATIVE_TEST = (
    ROOT / "Tests/Content/Native/PresentationNiagaraEmitterValidationTest.cpp"
)


class PresentationManifestTests(unittest.TestCase):
    def run_tool(self, command, project_root=ROOT):
        return subprocess.run(
            ["python3", str(TOOL), command, "--project-root", str(project_root)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )

    def load_sources(self):
        return {name: json.loads(path.read_text(encoding="utf-8")) for name, path in SOURCES.items()}

    def make_fixture(self):
        fixture = Path(tempfile.mkdtemp(prefix="presentation-manifest-"))
        self.addCleanup(shutil.rmtree, fixture)
        for relative in [
            "Content/DA/Manifests/VerticalSlicePresentation.json",
            "Content/Buildings/Presentation/PrimaryAssets.json",
            "Content/Characters/Presentation/PrimaryAssets.json",
            "Content/VFX/Presentation/CoreVFX.json",
            "Content/Audio/Presentation/VerticalSliceAudio.json",
            "Content/Buildings/Presentation/FactionBindings.json",
            "Content/DA/Manifests/PresentationArtifactSources.json",
            "ContentSource/Presentation/Geometry/PrimaryBlock.obj",
            "ContentSource/Presentation/Materials/FactionPalettes.json",
            "ContentSource/Presentation/VFX/NiagaraSystemTemplates.json",
            "ContentSource/Presentation/Audio/NeutralPulse.wav.b64",
            "ContentSource/Cinematics/ForgeweaveAscension.shotlist.json",
        ]:
            target = fixture / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, target)
        return fixture

    def refresh_fixture_fingerprint(self, fixture):
        manifest_path = fixture / "Content/DA/Manifests/VerticalSlicePresentation.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        sources = {
            name: json.loads((fixture / "Content" / relative).read_text(encoding="utf-8"))
            for name, relative in manifest["sources"].items()
        }
        material_manifest = copy.deepcopy(manifest)
        material_manifest.pop("fingerprint")
        manifest["fingerprint"] = hashlib.sha1(
            json.dumps(
                {"manifest": material_manifest, "sources": sources},
                sort_keys=True,
                separators=(",", ":"),
            ).encode("utf-8")
        ).hexdigest()
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

    def setUp(self):
        self.assertTrue(TOOL.exists(), "executable presentation manifest parser is missing")
        self.assertTrue(MANIFEST.exists(), "canonical presentation coverage manifest is missing")
        for name, source in SOURCES.items():
            self.assertTrue(source.exists(), f"{name} source recipe is missing")

    def test_validator_resolves_the_exact_frozen_presentation_scope(self):
        result = self.run_tool("validate")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            json.loads(result.stdout),
            {
                "ambientLoops": 12,
                "constructionGrammars": 3,
                "coreVfx": 25,
                "musicCues": 9,
                "primaryAssets": 50,
                "sfxEvents": 60,
                "totalGeneratedDefinitions": 156,
            },
        )

        sources = self.load_sources()
        primary = sorted(
            sources["buildings"]["assets"] + sources["characters"]["assets"],
            key=lambda row: row["number"],
        )
        self.assertEqual([row["number"] for row in primary], list(range(1, 51)))
        self.assertEqual(
            [row["displayName"] for row in primary],
            [
                "Adaptive Habitat", "Civic Autonomy Pods", "Cognitive Operations Tower",
                "Predictive Bureau", "Autonomous Exchange", "Algorithmic Market",
                "Synthetic Fabrication Node", "Swarm Foundry", "Neural Relay",
                "Orchestration Hub", "Agency Forum", "Guardian Drone Cohort",
                "Audit Sentinel", "Synara Command Vehicle", "Archon Mira Vey",
                "The Thinking Spire", "Worker Arcology", "Forge Quarters",
                "Production Directorate", "Industrial Exchange", "Infinite Foundry",
                "Replication Forge", "Freight Furnace", "Smog Reclaimer", "Forge Guard",
                "Mechanist Crew", "Forgeweave Heavy Carrier", "Daxton Rhe",
                "The Grand Forge", "Garden Commune", "Ecological Design House",
                "Harvest Market", "Regenerative Bioworks", "Living Waterway",
                "Pollinator Corridor", "Ranger Circle", "Symbiosis Keepers", "Amara Venn",
                "The Worldgarden", "Compact Residence", "Corner Exchange",
                "Research Annex", "Microgrid Station", "Water Reclaimer",
                "Transit Stop Kit", "Watch Post", "Barrier Hub", "Founder Base Character",
                "Nia Vale", "Autonomous Factory",
            ],
        )

    def test_emit_plan_is_complete_unique_and_never_claims_generated_packages_exist(self):
        result = self.run_tool("emit-plan")
        self.assertEqual(result.returncode, 0, result.stderr)
        plan = json.loads(result.stdout)
        self.assertEqual(len(plan), 156)
        self.assertEqual(len({row["id"] for row in plan}), 156)
        self.assertEqual(len({row["assetPath"] for row in plan}), 156)
        self.assertEqual(
            {row["kind"] for row in plan},
            {"PrimaryAsset", "CoreVFX", "Music", "Ambient", "SFX"},
        )
        for row in plan:
            self.assertTrue(row["assetPath"].startswith("/Game/"))
            self.assertFalse(
                (ROOT / "Content" / (row["assetPath"].removeprefix("/Game/") + ".uasset")).exists(),
                f"source coverage may not masquerade as generated package {row['assetPath']}",
            )

    def test_artifact_plan_covers_real_importable_classes_and_checked_in_sources(self):
        result = self.run_tool("emit-artifact-plan")
        self.assertEqual(result.returncode, 0, result.stderr)
        plan = json.loads(result.stdout)

        primary = [row for row in plan if row["definitionKind"] == "PrimaryAsset"]
        vfx = [row for row in plan if row["definitionKind"] == "CoreVFX"]
        audio = [row for row in plan if row["definitionKind"] in {"Music", "Ambient", "SFX"}]
        sequences = [row for row in plan if row["definitionKind"] == "Sequence"]
        self.assertEqual(len(primary), 50 * 2)
        self.assertEqual(len(vfx), 25)
        self.assertEqual(len(audio), (9 + 12 + 60) * 2)
        self.assertEqual(len(sequences), 1)
        self.assertEqual({row["role"] for row in primary}, {"mesh", "material"})
        self.assertEqual({row["assetClass"] for row in primary}, {"StaticMesh", "MaterialInstanceConstant"})
        self.assertEqual({row["assetClass"] for row in vfx}, {"NiagaraSystem"})
        self.assertEqual({row["role"] for row in audio}, {"wave", "cue"})
        self.assertEqual({row["assetClass"] for row in audio}, {"SoundWave", "SoundCue"})
        self.assertEqual(sequences[0]["assetClass"], "LevelSequence")
        self.assertEqual(
            sequences[0]["assetPath"],
            "/Game/Cinematics/CS_ForgeweaveAscension",
        )
        for row in plan:
            source = ROOT / row["sourcePath"]
            self.assertTrue(source.is_file(), f"checked-in artifact source is missing: {source}")
            self.assertEqual(
                hashlib.sha1(source.read_bytes()).hexdigest(),
                row["sourceSha1"],
                f"artifact source fingerprint drifted: {source}",
            )
            self.assertTrue(row["assetPath"].startswith("/Game/"))

        palettes = json.loads(
            (ROOT / "ContentSource/Presentation/Materials/FactionPalettes.json").read_text(
                encoding="utf-8"
            )
        )["palettes"]
        primary_by_id = {
            row["id"]: row
            for row in self.load_sources()["buildings"]["assets"]
            + self.load_sources()["characters"]["assets"]
        }
        for material in [row for row in primary if row["role"] == "material"]:
            faction = primary_by_id[material["definitionId"]]["faction"]
            self.assertEqual(
                material["sourceContent"],
                palettes[faction],
                "material generation must consume the definition faction's authored palette",
            )

        niagara_source = json.loads(
            (ROOT / "ContentSource/Presentation/VFX/NiagaraSystemTemplates.json").read_text(
                encoding="utf-8"
            )
        )
        for system in vfx:
            content = system["sourceContent"]
            self.assertEqual(content["systemTemplate"], niagara_source["systemTemplate"])
            self.assertEqual(content["emitter"], niagara_source["emitter"])
            self.assertEqual(content["parameters"], niagara_source["parameters"])
            self.assertNotIn("empty", content["systemTemplate"])
            self.assertGreater(content["emitter"]["burstCount"], 0)
            self.assertGreater(content["emitter"]["lifetimeSeconds"], 0)

    def test_artifact_plan_changes_with_palette_source_and_rejects_empty_niagara_content(self):
        fixture = self.make_fixture()
        palettes_path = fixture / "ContentSource/Presentation/Materials/FactionPalettes.json"
        palettes = json.loads(palettes_path.read_text(encoding="utf-8"))
        palettes["palettes"]["Synara"]["metallic"] = 0.91
        palettes_path.write_text(json.dumps(palettes), encoding="utf-8")

        artifact_path = fixture / "Content/DA/Manifests/PresentationArtifactSources.json"
        artifacts = json.loads(artifact_path.read_text(encoding="utf-8"))
        artifacts["primary"]["materialSourceSha1"] = hashlib.sha1(
            palettes_path.read_bytes()
        ).hexdigest()
        artifact_path.write_text(json.dumps(artifacts), encoding="utf-8")
        self.refresh_fixture_fingerprint(fixture)

        changed = self.run_tool("emit-artifact-plan", fixture)
        self.assertEqual(changed.returncode, 0, changed.stderr)
        changed_plan = json.loads(changed.stdout)
        synara_material = next(
            row
            for row in changed_plan
            if row["definitionId"] == "primary.01.adaptive_habitat"
            and row["role"] == "material"
        )
        self.assertEqual(synara_material["sourceContent"]["metallic"], 0.91)

        niagara_path = fixture / "ContentSource/Presentation/VFX/NiagaraSystemTemplates.json"
        niagara = json.loads(niagara_path.read_text(encoding="utf-8"))
        niagara["systemTemplate"] = "empty_system"
        niagara["emitter"]["burstCount"] = 0
        niagara_path.write_text(json.dumps(niagara), encoding="utf-8")
        artifacts["vfx"]["systemSourceSha1"] = hashlib.sha1(
            niagara_path.read_bytes()
        ).hexdigest()
        artifact_path.write_text(json.dumps(artifacts), encoding="utf-8")
        self.refresh_fixture_fingerprint(fixture)

        rejected = self.run_tool("validate", fixture)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("non-empty CPU sprite burst", rejected.stderr)

    def test_niagara_source_requires_selected_emitter_self_once_lifecycle(self):
        result = self.run_tool("emit-artifact-plan")
        self.assertEqual(result.returncode, 0, result.stderr)
        vfx = [
            row
            for row in json.loads(result.stdout)
            if row["definitionKind"] == "CoreVFX"
        ]
        self.assertEqual(len(vfx), 25)
        for system in vfx:
            self.assertEqual(
                system["sourceContent"]["emitter"]["lifecycle"],
                {"mode": "Self", "loopBehavior": "Once"},
            )

        for field, invalid_value in (("mode", "System"), ("loopBehavior", "Infinite")):
            with self.subTest(field=field):
                fixture = self.make_fixture()
                niagara_path = fixture / "ContentSource/Presentation/VFX/NiagaraSystemTemplates.json"
                niagara = json.loads(niagara_path.read_text(encoding="utf-8"))
                niagara["emitter"]["lifecycle"][field] = invalid_value
                niagara_path.write_text(json.dumps(niagara), encoding="utf-8")
                artifact_path = fixture / "Content/DA/Manifests/PresentationArtifactSources.json"
                artifacts = json.loads(artifact_path.read_text(encoding="utf-8"))
                artifacts["vfx"]["systemSourceSha1"] = hashlib.sha1(
                    niagara_path.read_bytes()
                ).hexdigest()
                artifact_path.write_text(json.dumps(artifacts), encoding="utf-8")
                self.refresh_fixture_fingerprint(fixture)

                rejected = self.run_tool("validate", fixture)
                self.assertNotEqual(rejected.returncode, 0)
                self.assertIn("Self/Once emitter lifecycle", rejected.stderr)

    def test_niagara_validation_cannot_combine_identity_and_renderer_across_emitters(self):
        with tempfile.TemporaryDirectory(prefix="presentation-niagara-validation-") as temp:
            executable = Path(temp) / "presentation-niagara-validation-test"
            compiled = subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I",
                    str(ROOT / "Source/DominionEditor/Private"),
                    str(NIAGARA_VALIDATION_NATIVE_TEST),
                    "-o",
                    str(executable),
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            executed = subprocess.run(
                [str(executable)],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
            self.assertEqual(executed.returncode, 0, executed.stderr)

    def test_sixty_one_unique_sfx_are_compliant_and_definition_totals_are_derived(self):
        fixture = self.make_fixture()
        audio_path = fixture / "Content/Audio/Presentation/VerticalSliceAudio.json"
        document = json.loads(audio_path.read_text(encoding="utf-8"))
        extra = copy.deepcopy(document["sfx"][-1])
        extra["id"] = "sfx.validation.sixty_one"
        extra["family"] = "validation"
        extra["assetPath"] = "/Game/Audio/SFX/Validation/DA_Presentation_SixtyOne"
        extra["recipe"]["event"] = "sixty_one"
        document["sfx"].append(extra)
        audio_path.write_text(json.dumps(document), encoding="utf-8")
        self.refresh_fixture_fingerprint(fixture)

        result = self.run_tool("validate", fixture)
        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(summary["sfxEvents"], 61)
        self.assertEqual(summary["totalGeneratedDefinitions"], 157)
        generated = self.run_tool("emit-plan", fixture)
        self.assertEqual(generated.returncode, 0, generated.stderr)
        self.assertEqual(len(json.loads(generated.stdout)), 157)

    def test_faction_construction_damage_and_capture_are_behaviorally_distinct(self):
        bindings = self.load_sources()["bindings"]
        grammars = bindings["constructionGrammars"]
        self.assertEqual([row["faction"] for row in grammars], ["Synara", "Forgeweave", "EdenCircuit"])
        self.assertEqual([len(row["stages"]) for row in grammars], [5, 5, 5])
        signatures = [
            tuple(stage["geometryHook"] for stage in row["stages"])
            for row in grammars
        ]
        self.assertEqual(len(set(signatures)), 3)
        self.assertEqual(
            signatures,
            [
                ("micro_drone_survey", "micro_drone_frame", "smart_panels_unfold", "data_seams_online", "adaptive_panels_release"),
                ("tracked_platform_grade", "crane_frame_lift", "molten_plate_cast", "weld_and_pressure_test", "gantries_begin_cycle"),
                ("root_lattice_anchor", "timber_frame_branch", "living_shell_grow", "water_mycelium_connect", "canopy_biome_balance"),
            ],
        )
        self.assertEqual(
            [row["materialHook"] for row in bindings["damageLanguages"]],
            ["fractured_ceramic_exposed_lattice", "soot_bent_steel_heat_damage", "torn_fiber_scorched_living_material"],
        )
        self.assertEqual(
            bindings["capturePolicy"],
            {
                "sourceHook": "UDACaptureComponent.OnCaptureStateChanged",
                "allowsInstantFactionRecolor": False,
                "stages": ["original_architecture", "remove_old_signage", "integration_scaffold", "install_new_signage", "integrated_operation"],
            },
        )

    def test_real_gameplay_hooks_and_skippable_cinematics_resolve_to_authored_cues(self):
        sources = self.load_sources()
        bindings = sources["bindings"]
        all_ids = {
            *[row["id"] for row in sources["vfx"]["effects"]],
            *[row["id"] for row in sources["audio"]["music"]],
            *[row["id"] for row in sources["audio"]["ambient"]],
            *[row["id"] for row in sources["audio"]["sfx"]],
        }
        self.assertEqual(bindings["constructionSourceHook"], "UDAConstructionComponent.OnStageChanged")
        self.assertEqual(bindings["damageSourceHook"], "UDAStructuralDamageComponent.OnDamageStateChanged")
        self.assertEqual(bindings["daxton"]["sourceAuthority"], "FDADaxtonCampaignState")
        self.assertEqual(bindings["ascension"]["sourceAuthority"], "FDAAscensionPresentationState")
        self.assertFalse(bindings["ascension"]["gameplayGate"])
        self.assertTrue(bindings["ascension"]["cinematicMayBeSkipped"])
        self.assertEqual(
            bindings["ascension"]["cinematicAssetPath"],
            "/Game/Cinematics/CS_ForgeweaveAscension.CS_ForgeweaveAscension",
        )
        for row in [
            *[stage for grammar in bindings["constructionGrammars"] for stage in grammar["stages"]],
            *bindings["damageLanguages"],
            *bindings["daxton"]["states"],
            *bindings["ascension"]["beats"],
        ]:
            self.assertIn(row["vfxId"], all_ids)
            self.assertIn(row["sfxId"], all_ids)

    def test_validator_rejects_source_drift_duplicate_ids_and_unknown_fields(self):
        fixture = self.make_fixture()
        mesh_path = fixture / "ContentSource/Presentation/Geometry/PrimaryBlock.obj"
        mesh_path.write_text(mesh_path.read_text(encoding="utf-8") + "\n# drift\n", encoding="utf-8")
        artifact_drift = self.run_tool("validate", fixture)
        self.assertNotEqual(artifact_drift.returncode, 0)
        self.assertIn("artifact source fingerprint drift", artifact_drift.stderr.lower())

        fixture = self.make_fixture()
        building_path = fixture / "Content/Buildings/Presentation/PrimaryAssets.json"
        document = json.loads(building_path.read_text())
        document["assets"][1]["id"] = document["assets"][0]["id"]
        building_path.write_text(json.dumps(document), encoding="utf-8")
        duplicate = self.run_tool("validate", fixture)
        self.assertNotEqual(duplicate.returncode, 0)
        self.assertIn("duplicate", duplicate.stderr.lower())

        fixture = self.make_fixture()
        binding_path = fixture / "Content/Buildings/Presentation/FactionBindings.json"
        document = json.loads(binding_path.read_text())
        document["constructionGrammars"][0]["stages"][0]["foreignGameplayState"] = True
        binding_path.write_text(json.dumps(document), encoding="utf-8")
        foreign = self.run_tool("validate", fixture)
        self.assertNotEqual(foreign.returncode, 0)
        self.assertIn("unknown", foreign.stderr.lower())

        fixture = self.make_fixture()
        manifest_path = fixture / "Content/DA/Manifests/VerticalSlicePresentation.json"
        document = copy.deepcopy(json.loads(manifest_path.read_text()))
        document["fingerprint"] = "0" * 40
        manifest_path.write_text(json.dumps(document), encoding="utf-8")
        stale = self.run_tool("validate", fixture)
        self.assertNotEqual(stale.returncode, 0)
        self.assertIn("fingerprint", stale.stderr.lower())

    def test_frozen_gameplay_and_ui_counts_are_unchanged(self):
        vertical = json.loads((ROOT / "Content/DA/Manifests/VerticalSliceContent.json").read_text())
        ui = json.loads((ROOT / "Content/UI/Manifests/VerticalSliceUI.json").read_text())
        self.assertEqual(len(vertical["definitions"]), 64)
        self.assertEqual(sum(row["quantity"] for row in vertical["starterDeck"]), 60)
        self.assertEqual(len(ui["screens"]), 27)


if __name__ == "__main__":
    unittest.main()
