import importlib.util
import copy
import hashlib
import json
import math
import pathlib
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOL_PATH = ROOT / "Build" / "Tools" / "PerformanceBenchmarkTool.py"
SCENE_PATH = ROOT / "Build" / "Performance" / "VerticalSliceBenchmark.scene.json"
BUDGET_PATH = ROOT / "Build" / "Performance" / "VerticalSliceBenchmark.budget.json"
EVIDENCE_PATH = ROOT / "Build" / "Performance" / "Task27PerformanceEvidence.json"


def load_tool():
    spec = importlib.util.spec_from_file_location("performance_benchmark_tool", TOOL_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def captured_evidence(tool, directory):
    plan_path = pathlib.Path(directory) / "plan.json"
    evidence_path = pathlib.Path(directory) / "evidence.json"
    tool.generate_plan(SCENE_PATH, BUDGET_PATH, plan_path, evidence_path)
    budget = tool.load_and_validate_budget(BUDGET_PATH)
    evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
    digest = "a" * 64
    evidence.update({
        "state": "CAPTURED",
        "readinessReasons": [],
        "hardware": copy.deepcopy(budget["referenceHardware"]),
        "measurements": {
            "frameSampleCount": 600,
            "averageFramesPerSecond": 60.0,
            "worstFrameFramesPerSecond": 30.0,
            "worstDevelopmentCycleMilliseconds": 100.0,
            "modeTransitionSeconds": [0.6, 0.45, 0.35],
            "developmentCycles": 1000,
            "enabledEventCount": 6,
            "playerEconomyDevelopmentCycles": 1000,
            "tradeWorldTicks": 200,
            "forgeweaveWorldTicks": 200,
            "utilityTopologyNodeCount": 2,
            "utilityTopologyResolved": True,
            "populationStayedNonnegative": True,
            "eventEmissionCount": 4,
            "eventTransitionCount": 8,
            "eventRunawayDetected": False,
            "staleRequiredQuestDetected": False,
            "requiredQuestResolutionFailed": False,
            "initialSaveBytes": 1000,
            "finalSaveBytes": 1100,
            "repeatabilityDigestA": digest,
            "repeatabilityDigestB": digest,
        },
        "commands": {
            "prepare": {"argv": ["UnrealEditor-Cmd", "Prepare"], "exitCode": 0},
            "automation": {"argv": ["UnrealEditor-Cmd", "Automation"], "exitCode": 0},
            "cook": {"argv": ["UnrealEditor-Cmd", "Cook"], "exitCode": 0},
        },
        "claims": {
            "performanceTargetsPassed": True,
            "soakPassed": True,
            "cookPassed": True,
        },
    })
    evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
    return plan_path, evidence_path, evidence


class PerformanceBenchmarkContractTest(unittest.TestCase):
    def test_source_freezes_the_exact_vertical_slice_load(self):
        tool = load_tool()
        scene = tool.load_and_validate_scene(SCENE_PATH)

        self.assertEqual(scene["gameplayAssetCount"], 50)
        self.assertEqual(sum(row["count"] for row in scene["citizensByLOD"]), 120)
        self.assertEqual([row["lod"] for row in scene["citizensByLOD"]], [0, 1, 2, 3])
        self.assertTrue(scene["weatherActive"])
        self.assertEqual(scene["alliedSquadCount"], 3)
        self.assertEqual(scene["enemySquadCount"], 3)
        self.assertEqual(scene["vehicleCount"], 1)
        self.assertEqual(scene["constructionEffectCount"], 1)
        self.assertEqual(scene["constructionEffectAsset"],
                         "/Game/VFX/Synara/DA_VFX_Synara_Construction.DA_VFX_Synara_Construction")
        self.assertEqual(scene["damagedBuildingCount"], 1)
        self.assertEqual(scene["modeTransition"], ["City", "Command", "Founder"])

        with tempfile.TemporaryDirectory() as directory:
            drifted_path = pathlib.Path(directory) / "scene.json"
            drifted = copy.deepcopy(scene)
            drifted["citizensByLOD"][2] = {
                "lod": 2, "count": 39, "representation": "detailed"}
            drifted["citizensByLOD"][3]["count"] = 21
            drifted_path.write_text(json.dumps(drifted), encoding="utf-8")
            with self.assertRaises(ValueError):
                tool.load_and_validate_scene(drifted_path)

    def test_generator_is_deterministic_and_marks_uncaptured_evidence_not_ready(self):
        tool = load_tool()
        with tempfile.TemporaryDirectory() as directory:
            first = pathlib.Path(directory) / "first.json"
            second = pathlib.Path(directory) / "second.json"
            first_evidence = pathlib.Path(directory) / "first-evidence.json"
            second_evidence = pathlib.Path(directory) / "second-evidence.json"

            tool.generate_plan(SCENE_PATH, BUDGET_PATH, first, first_evidence)
            tool.generate_plan(SCENE_PATH, BUDGET_PATH, second, second_evidence)

            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(first_evidence.read_bytes(), second_evidence.read_bytes())
            plan = json.loads(first.read_text(encoding="utf-8"))
            self.assertEqual(len(plan["assets"]), 50)
            self.assertEqual(len({row["worldAssetId"] for row in plan["assets"]}), 50)
            self.assertTrue(all(row["presentationAsset"].startswith("/Game/Buildings/")
                                for row in plan["assets"]))
            self.assertEqual(sum(row["constructionState"] == "Damaged" for row in plan["assets"]), 1)
            self.assertEqual(sum(row["constructionEffect"] for row in plan["assets"]), 1)
            self.assertEqual(len(plan["citizens"]), 120)
            self.assertEqual({row["lod"] for row in plan["citizens"]}, {0, 1, 2, 3})
            self.assertEqual(sum(row["visibleActorRequired"] for row in plan["citizens"]), 60)
            self.assertEqual(len(plan["squads"]), 6)
            self.assertEqual(len(plan["vehicles"]), 1)
            evidence = json.loads(first_evidence.read_text(encoding="utf-8"))
            self.assertEqual(evidence["state"], "NOT_READY")
            self.assertIsNone(evidence["measurements"])
            self.assertFalse(evidence["claims"]["performanceTargetsPassed"])

    def test_checked_in_evidence_cannot_claim_measurements_which_were_not_run(self):
        tool = load_tool()
        evidence = tool.load_and_validate_evidence(EVIDENCE_PATH, BUDGET_PATH)

        self.assertIn(evidence["state"], {"NOT_RUN", "NOT_READY"})
        self.assertIsNone(evidence["measurements"])
        self.assertFalse(evidence["claims"]["performanceTargetsPassed"])
        self.assertEqual(evidence["commands"]["automation"]["exitCode"], 127)
        self.assertEqual(evidence["commands"]["cook"]["exitCode"], 127)
        self.assertEqual(evidence["commands"]["prepare"]["exitCode"], 127)

    def test_cli_validates_and_generates_the_same_plan(self):
        with tempfile.TemporaryDirectory() as directory:
            plan = pathlib.Path(directory) / "plan.json"
            evidence = pathlib.Path(directory) / "evidence.json"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(TOOL_PATH),
                    "generate",
                    "--scene",
                    str(SCENE_PATH),
                    "--budget",
                    str(BUDGET_PATH),
                    "--plan-output",
                    str(plan),
                    "--evidence-output",
                    str(evidence),
                ],
                cwd=ROOT,
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(json.loads(plan.read_text(encoding="utf-8"))["developmentCycles"], 1000)

    def test_captured_evidence_is_reconciled_to_plan_budget_commands_and_derived_claims(self):
        tool = load_tool()
        with tempfile.TemporaryDirectory() as directory:
            plan_path, evidence_path, evidence = captured_evidence(tool, directory)
            accepted = tool.load_and_validate_evidence(evidence_path, BUDGET_PATH, plan_path)
            self.assertEqual(accepted["state"], "CAPTURED")

            mutations = {
                "infinite average FPS": lambda row: row["measurements"].__setitem__(
                    "averageFramesPerSecond", math.inf),
                "negative hitch": lambda row: row["measurements"].__setitem__(
                    "worstDevelopmentCycleMilliseconds", -0.1),
                "negative transition": lambda row: row["measurements"][
                    "modeTransitionSeconds"].__setitem__(0, -0.1),
                "infinite transition": lambda row: row["measurements"][
                    "modeTransitionSeconds"].__setitem__(1, math.inf),
                "negative initial save": lambda row: row["measurements"].__setitem__(
                    "initialSaveBytes", -1),
                "negative final save": lambda row: row["measurements"].__setitem__(
                    "finalSaveBytes", -1),
                "fake digest": lambda row: row["measurements"].__setitem__(
                    "repeatabilityDigestA", "not-a-sha256"),
                "fake hardware": lambda row: row.__setitem__(
                    "hardware", {**row["hardware"], "gpu": "Imaginary GPU"}),
                "fake plan hash": lambda row: row.__setitem__("planSha256", "0" * 64),
                "failed Automation": lambda row: row["commands"]["automation"].__setitem__(
                    "exitCode", 1),
                "failed preparation": lambda row: row["commands"]["prepare"].__setitem__(
                    "exitCode", 1),
                "failed cook": lambda row: row["commands"]["cook"].__setitem__("exitCode", 1),
                "fake performance claim": lambda row: row["claims"].__setitem__(
                    "performanceTargetsPassed", False),
                "fake soak claim": lambda row: row["claims"].__setitem__("soakPassed", False),
                "fake cook claim": lambda row: row["claims"].__setitem__("cookPassed", False),
            }
            for label, mutate in mutations.items():
                with self.subTest(label=label):
                    candidate = copy.deepcopy(evidence)
                    mutate(candidate)
                    evidence_path.write_text(json.dumps(candidate), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        tool.load_and_validate_evidence(evidence_path, BUDGET_PATH, plan_path)

    def test_public_captured_validation_rejects_unreconciled_plan_digests(self):
        tool = load_tool()
        with tempfile.TemporaryDirectory() as directory:
            _, evidence_path, evidence = captured_evidence(tool, directory)
            with self.assertRaises(ValueError):
                tool.load_and_validate_evidence(evidence_path, BUDGET_PATH)
            for label, plan_digest in {
                    "null digest": None,
                    "wrong digest": "0" * 64,
            }.items():
                with self.subTest(label=label):
                    candidate = copy.deepcopy(evidence)
                    candidate["planSha256"] = plan_digest
                    evidence_path.write_text(json.dumps(candidate), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        tool.load_and_validate_evidence(evidence_path, BUDGET_PATH)

    def test_public_evidence_validation_rejects_noncanonical_scene_id(self):
        tool = load_tool()
        with tempfile.TemporaryDirectory() as directory:
            plan_path, evidence_path, evidence = captured_evidence(tool, directory)
            evidence["sceneId"] = "benchmark.vertical_slice.impostor"
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")

            with self.assertRaises(ValueError):
                tool.load_and_validate_evidence(evidence_path, BUDGET_PATH, plan_path)

    def test_runner_accepts_only_a_fully_validated_candidate_and_resets_mismatch(self):
        tool = load_tool()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            editor = root / "fake-editor.py"
            project = root / "DominionAscendant.uproject"
            project.write_text("{}", encoding="utf-8")
            editor.write_text(
                "#!/usr/bin/env python3\n"
                "import hashlib, json, pathlib, sys\n"
                "if '-run=cook' in sys.argv: raise SystemExit(0)\n"
                "if '-run=DAPresentationContent' in sys.argv: raise SystemExit(0)\n"
                "def value(prefix):\n"
                "  return next(x.split('=', 1)[1] for x in sys.argv if x.startswith(prefix))\n"
                "plan = pathlib.Path(value('-DABenchmarkPlan='))\n"
                "out = pathlib.Path(value('-DABenchmarkEvidenceCandidate='))\n"
                "budget = json.loads(pathlib.Path(value('-DABenchmarkBudget=')).read_text())\n"
                "digest = 'a' * 64\n"
                "measurements = {'frameSampleCount':600,'averageFramesPerSecond':60.0,"
                "'worstFrameFramesPerSecond':30.0,'worstDevelopmentCycleMilliseconds':100.0,"
                "'modeTransitionSeconds':[0.6,0.45,0.35],'developmentCycles':1000,"
                "'enabledEventCount':6,'playerEconomyDevelopmentCycles':1000,"
                "'tradeWorldTicks':200,'forgeweaveWorldTicks':200,'utilityTopologyNodeCount':2,"
                "'utilityTopologyResolved':True,'populationStayedNonnegative':True,"
                "'eventEmissionCount':4,'eventTransitionCount':8,'eventRunawayDetected':False,"
                "'staleRequiredQuestDetected':False,'requiredQuestResolutionFailed':False,"
                "'initialSaveBytes':1000,'finalSaveBytes':1100,"
                "'repeatabilityDigestA':digest,'repeatabilityDigestB':digest}\n"
                "plan_hash = hashlib.sha256(plan.read_bytes()).hexdigest()\n"
                "if (out.parent / 'CORRUPT').exists(): plan_hash = '0' * 64\n"
                "payload={'$schema':'PerformanceEvidence.schema.json','schemaVersion':1,"
                "'sceneId':'benchmark.vertical_slice.synara_capital','planSha256':plan_hash,"
                "'state':'CANDIDATE','readinessReasons':['Awaiting runner command reconciliation.'],"
                "'hardware':budget['referenceHardware'],"
                "'commands':{'prepare':{'argv':[], 'exitCode':None},"
                "'automation':{'argv':[], 'exitCode':None},'cook':{'argv':[], 'exitCode':None}},"
                "'measurements':measurements,'claims':{'performanceTargetsPassed':False,"
                "'soakPassed':False,'cookPassed':False}}\n"
                "out.write_text(json.dumps(payload))\n",
                encoding="utf-8",
            )
            editor.chmod(0o755)
            plan = root / "plan.json"
            evidence = root / "evidence.json"
            result = tool.run_ue(
                SCENE_PATH, BUDGET_PATH, plan, evidence, project, str(editor))
            accepted = json.loads(evidence.read_text(encoding="utf-8"))
            self.assertEqual(result, 0)
            self.assertEqual(accepted["state"], "CAPTURED")
            self.assertEqual(accepted["claims"], {
                "performanceTargetsPassed": True, "soakPassed": True, "cookPassed": True})
            automation_argv = accepted["commands"]["automation"]["argv"]
            self.assertIn("-ResX=1920", automation_argv)
            self.assertIn("-ResY=1080", automation_argv)
            self.assertIn("-windowed", automation_argv)
            self.assertNotIn("-nullrhi", automation_argv)

            (root / "CORRUPT").write_text("hash", encoding="utf-8")
            result = tool.run_ue(
                SCENE_PATH, BUDGET_PATH, plan, evidence, project, str(editor))
            rejected = json.loads(evidence.read_text(encoding="utf-8"))
            self.assertNotEqual(result, 0)
            self.assertEqual(rejected["state"], "NOT_READY")
            self.assertIsNone(rejected["hardware"])
            self.assertIsNone(rejected["measurements"])
            self.assertEqual(rejected["claims"], {
                "performanceTargetsPassed": False, "soakPassed": False, "cookPassed": False})


if __name__ == "__main__":
    unittest.main()
