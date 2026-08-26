import copy
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOOL_PATH = ROOT / "Build" / "Tools" / "ReleaseCandidateTool.py"


def load_tool():
    spec = importlib.util.spec_from_file_location("release_candidate_tool", TOOL_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {TOOL_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def complete_candidate():
    def artifact(path, value):
        return {"path": path, "sha256": f"{value:064x}"}

    routes = []
    loyalty_rewards = [15, 30, 45, 60]
    for index, (route, leader) in enumerate(
        [
            ("Force", "Prisoner"),
            ("Economic", "IndustrialAdvisor"),
            ("Influence", "Governor"),
            ("Alliance", "AlliedForgeLord"),
        ]
    ):
        routes.append(
            {
                "route": route,
                "sourceSha256": f"{index + 1:064x}",
                "reachedFirstAscension": True,
                "log": artifact(f"Artifacts/Release/{route}.log", 40 + index),
                "video": artifact(f"Artifacts/Release/{route}.mp4", 50 + index),
                "aftermath": {
                    "loyaltyBefore": 0,
                    "loyalty": loyalty_rewards[index],
                    "loyaltyActionSha256": f"{index + 80:064x}",
                    "daxtonState": leader,
                    "historySha256": f"{index + 10:064x}",
                    "damageSha256": f"{index + 20:064x}",
                    "relationshipSha256": f"{index + 30:064x}",
                },
            }
        )
    checkpoints = [
        "first_city_built",
        "mid_nia_quest",
        "active_foundry_shortage",
        "mid_iron_veil",
        "daxton_phase_transition",
        "immediately_after_ascension",
    ]
    controller_actions = [
        "build",
        "card_inspect",
        "combat",
        "command",
        "diplomacy",
        "conquest",
        "save_load",
    ]
    onboarding_actions = [
        "move_founder",
        "activate_founder_hall",
        "place_adaptive_habitat",
        "understand_utilities",
        "house_residents",
        "employ_nia",
        "generate_capital",
        "encounter_dependency",
        "research_with_insight",
        "understand_influence",
        "meet_forgeweave_eden",
        "complete_first_combat",
        "command_guardian_drones",
    ]
    return {
        "schemaVersion": 1,
        "runId": "release-run-2026-08-26-001",
        "buildId": "windows-development-candidate-001",
        "fixturePlanSha256": "a" * 64,
        "task27Evidence": {
            "path": "Build/Performance/Task27PerformanceEvidence.json",
            "sha256": "b" * 64,
            "state": "CAPTURED",
        },
        "automationEvidence": {
            "path": "Artifacts/Release/automation.json",
            "sha256": "c" * 64,
            "exitCode": 0,
        },
        "routes": routes,
        "saveCheckpoints": [
            {
                "checkpoint": checkpoint,
                "passed": True,
                "snapshotSha256": f"{index + 90:064x}",
                "log": artifact(f"Artifacts/Release/{checkpoint}.log", 60 + index),
            }
            for index, checkpoint in enumerate(checkpoints)
        ],
        "controller": {
            "observedByHuman": True,
            "mouseUsed": False,
            "actions": {action: True for action in controller_actions},
            "checklist": artifact("Artifacts/Release/controller-checklist.md", 70),
            "video": artifact("Artifacts/Release/controller.mp4", 71),
        },
        "onboarding": {
            "uncoachedFirstTimeTesters": 5,
            "developerCoaching": False,
            "actions": {action: True for action in onboarding_actions},
            "observationLog": artifact("Artifacts/Release/onboarding-observations.md", 72),
        },
        "defects": {
            "Blocker": 0,
            "Critical": 0,
            "High": 0,
            "Medium": 1,
            "Low": 2,
            "acceptedMedium": ["DA-1001"],
            "lowWithinPolishTolerance": True,
            "report": artifact("Artifacts/Release/defects.json", 73),
        },
    }


def canonical_bytes(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def write_artifact(root, relative, content):
    path = Path(root) / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = content if isinstance(content, bytes) else canonical_bytes(content)
    path.write_bytes(payload)
    return {"path": relative, "sha256": hashlib.sha256(payload).hexdigest()}


def build_complete_release_root(temporary):
    root = Path(temporary)
    shutil.copytree(ROOT / "Content" / "Test" / "Fixtures", root / "Content" / "Test" / "Fixtures")
    task27 = {
        "schemaVersion": 1,
        "state": "CAPTURED",
        "claims": {
            "performanceTargetsPassed": True,
            "soakPassed": True,
            "cookPassed": True,
        },
    }
    task27_ref = write_artifact(root, "Build/Performance/Task27PerformanceEvidence.json", task27)
    tool = load_tool()
    plan = tool.generate_fixture_plan(root)
    plan_ref = write_artifact(
        root, "Intermediate/Release/VerticalSliceRouteFixture.plan.json", plan
    )

    candidate = complete_candidate()
    candidate["runId"] = "release-run-2026-08-26-001"
    candidate["buildId"] = "windows-development-candidate-001"
    candidate["fixturePlanSha256"] = plan_ref["sha256"]
    candidate["task27Evidence"] = {**task27_ref, "state": "CAPTURED"}
    source_by_route = {row["route"]: row["sourceSha256"] for row in plan["fixtures"]}
    for index, route in enumerate(candidate["routes"]):
        route["sourceSha256"] = source_by_route[route["route"]]
        route["log"] = write_artifact(
            root, f"Artifacts/Release/{route['route']}.log", f"route-log-{route['route']}\n".encode()
        )
        route["video"] = write_artifact(
            root, f"Artifacts/Release/{route['route']}.mp4", f"route-video-{route['route']}\n".encode()
        )
        route["aftermath"]["loyaltyBefore"] = 0
        route["aftermath"]["loyaltyActionSha256"] = f"{80 + index:064x}"
    for index, checkpoint in enumerate(candidate["saveCheckpoints"]):
        checkpoint["snapshotSha256"] = f"{90 + index:064x}"
        checkpoint["log"] = write_artifact(
            root,
            f"Artifacts/Release/{checkpoint['checkpoint']}.log",
            f"checkpoint-log-{checkpoint['checkpoint']}\n".encode(),
        )

    controller_video = write_artifact(root, "Artifacts/Release/controller.mp4", b"controller-video\n")
    candidate["controller"]["video"] = controller_video
    controller_attestation = {
        "schemaVersion": 1,
        "kind": "controller",
        "runId": candidate["runId"],
        "buildId": candidate["buildId"],
        "attestedBy": "qa-controller-01",
        "recordedAtUtc": "2026-08-26T17:00:00Z",
        "observedByHuman": True,
        "mouseUsed": False,
        "actions": candidate["controller"]["actions"],
        "video": controller_video,
    }
    candidate["controller"]["checklist"] = write_artifact(
        root, "Artifacts/Release/controller-attestation.json", controller_attestation
    )

    candidate["onboarding"]["uncoachedFirstTimeTesters"] = 2
    onboarding_attestation = {
        "schemaVersion": 1,
        "kind": "onboarding",
        "runId": candidate["runId"],
        "buildId": candidate["buildId"],
        "attestedBy": "qa-onboarding-01",
        "recordedAtUtc": "2026-08-26T18:00:00Z",
        "testers": [
            {
                "testerId": tester,
                "developerCoaching": False,
                "actions": candidate["onboarding"]["actions"],
            }
            for tester in ("first-time-001", "first-time-002")
        ],
    }
    candidate["onboarding"]["observationLog"] = write_artifact(
        root, "Artifacts/Release/onboarding-attestation.json", onboarding_attestation
    )

    defect_attestation = {
        "schemaVersion": 1,
        "kind": "defects",
        "runId": candidate["runId"],
        "buildId": candidate["buildId"],
        "attestedBy": "release-manager-01",
        "recordedAtUtc": "2026-08-26T19:00:00Z",
        "counts": {severity: candidate["defects"][severity] for severity in ("Blocker", "Critical", "High", "Medium", "Low")},
        "acceptedMedium": candidate["defects"]["acceptedMedium"],
        "lowWithinPolishTolerance": True,
        "issues": [
            {"id": "DA-1001", "severity": "Medium", "disposition": "accepted"},
            {"id": "DA-1002", "severity": "Low", "disposition": "polish_tolerance"},
            {"id": "DA-1003", "severity": "Low", "disposition": "polish_tolerance"},
        ],
    }
    candidate["defects"]["report"] = write_artifact(
        root, "Artifacts/Release/defect-attestation.json", defect_attestation
    )

    automation = {
        "schemaVersion": 1,
        "runId": candidate["runId"],
        "buildId": candidate["buildId"],
        "fixturePlanSha256": plan_ref["sha256"],
        "routes": [
            {
                key: route[key]
                for key in ("route", "sourceSha256", "reachedFirstAscension", "aftermath")
            }
            for route in candidate["routes"]
        ],
        "saveCheckpoints": [
            {
                key: checkpoint[key]
                for key in ("checkpoint", "passed", "snapshotSha256")
            }
            for checkpoint in candidate["saveCheckpoints"]
        ],
    }
    automation_ref = write_artifact(
        root, "Artifacts/Release/automation.json", automation
    )
    candidate["automationEvidence"] = {**automation_ref, "exitCode": 0}
    candidate_path = root / "Artifacts" / "Release" / "candidate.json"
    candidate_path.write_bytes(canonical_bytes(candidate))
    return root, candidate_path, root / plan_ref["path"], candidate


class VerticalSliceReleaseContractTest(unittest.TestCase):
    def test_finalize_rejects_missing_fixtures_and_a_stale_non_authoritative_plan(self):
        tool = load_tool()
        with tempfile.TemporaryDirectory() as temporary:
            root, candidate_path, plan_path, candidate = build_complete_release_root(temporary)
            shutil.rmtree(root / "Content" / "Test" / "Fixtures")
            evidence_path = root / "Build" / "Release" / "Task28ReleaseEvidence.json"
            completed = subprocess.run(
                [
                    "python3", str(TOOL_PATH), "finalize", "--root", str(root),
                    "--plan", str(plan_path), "--candidate", str(candidate_path),
                    "--evidence", str(evidence_path),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(completed.returncode, 2, completed.stdout + completed.stderr)
            self.assertIn("fixture", completed.stdout.lower())
            self.assertEqual(
                json.loads(evidence_path.read_text(encoding="utf-8"))["state"], "NOT_READY"
            )

        with tempfile.TemporaryDirectory() as temporary:
            root, _, plan_path, candidate = build_complete_release_root(temporary)
            fixture_path = (
                root / "Content" / "Test" / "Fixtures" / "ForceRoute"
                / "RouteFixture.source.json"
            )
            fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
            fixture["relationship"]["trust"] = 21
            fixture_path.write_bytes(canonical_bytes(fixture))
            with self.assertRaisesRegex(ValueError, "authoritative current fixture plan"):
                tool.reconcile_release_candidate(root, candidate, plan_path)

    def test_ready_revalidation_rehashes_every_linked_artifact_after_finalize(self):
        tool = load_tool()
        with tempfile.TemporaryDirectory() as temporary:
            root, candidate_path, plan_path, candidate = build_complete_release_root(temporary)
            evidence_path = root / "Build" / "Release" / "Task28ReleaseEvidence.json"
            completed = subprocess.run(
                [
                    "python3",
                    str(TOOL_PATH),
                    "finalize",
                    "--root",
                    str(root),
                    "--plan",
                    str(plan_path),
                    "--candidate",
                    str(candidate_path),
                    "--evidence",
                    str(evidence_path),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            self.assertEqual(
                tool.load_and_validate_release_evidence(root, evidence_path)["state"], "READY"
            )

            route_log = root / candidate["routes"][0]["log"]["path"]
            route_log.write_bytes(route_log.read_bytes() + b"mutated\n")
            with self.assertRaisesRegex(ValueError, "digest mismatch"):
                tool.load_and_validate_release_evidence(root, evidence_path)

            route_log.write_bytes(b"route-log-Force\n")
            (root / candidate["controller"]["checklist"]["path"]).unlink()
            with self.assertRaisesRegex(ValueError, "artifact is missing"):
                tool.load_and_validate_release_evidence(root, evidence_path)

    def test_candidate_requires_literal_booleans_and_matching_structured_attestations(self):
        tool = load_tool()
        candidate = complete_candidate()
        candidate["controller"]["actions"]["build"] = "true"
        with self.assertRaisesRegex(ValueError, "controller"):
            tool.validate_release_candidate(candidate)

        candidate = complete_candidate()
        candidate["onboarding"]["actions"]["move_founder"] = "yes"
        with self.assertRaisesRegex(ValueError, "onboarding"):
            tool.validate_release_candidate(candidate)

        with tempfile.TemporaryDirectory() as temporary:
            root, _, plan_path, candidate = build_complete_release_root(temporary)
            attestation_path = root / candidate["controller"]["checklist"]["path"]
            attestation = json.loads(attestation_path.read_text(encoding="utf-8"))
            attestation["runId"] = "different-run"
            candidate["controller"]["checklist"] = write_artifact(
                root, candidate["controller"]["checklist"]["path"], attestation
            )
            with self.assertRaisesRegex(ValueError, "controller attestation run/build"):
                tool.reconcile_release_candidate(root, candidate, plan_path)

        for artifact_key, boolean_path, truthy, message in (
            ("controller", ("observedByHuman",), "true", "controller attestation"),
            ("onboarding", ("testers", 0, "developerCoaching"), "false", "onboarding attestation"),
            ("defects", ("lowWithinPolishTolerance",), "true", "defect attestation"),
        ):
            with tempfile.TemporaryDirectory() as temporary:
                root, _, plan_path, candidate = build_complete_release_root(temporary)
                if artifact_key == "controller":
                    ref_key, candidate_key = "checklist", "controller"
                elif artifact_key == "onboarding":
                    ref_key, candidate_key = "observationLog", "onboarding"
                else:
                    ref_key, candidate_key = "report", "defects"
                ref = candidate[candidate_key][ref_key]
                path = root / ref["path"]
                attestation = json.loads(path.read_text(encoding="utf-8"))
                target = attestation
                for key in boolean_path[:-1]:
                    target = target[key]
                target[boolean_path[-1]] = truthy
                candidate[candidate_key][ref_key] = write_artifact(root, ref["path"], attestation)
                with self.assertRaisesRegex(ValueError, message):
                    tool.reconcile_release_candidate(root, candidate, plan_path)

    def test_automation_contract_preserves_explicit_partial_route_and_checkpoint_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "root"
            shutil.copytree(
                ROOT / "Content" / "Test" / "Fixtures",
                root / "Content" / "Test" / "Fixtures",
            )
            write_artifact(
                root,
                "Build/Performance/Task27PerformanceEvidence.json",
                {"schemaVersion": 1, "state": "NOT_READY", "claims": {}},
            )
            fake_editor = Path(temporary) / "fake-unreal-editor"
            fake_editor.write_text(
                """#!/usr/bin/env python3
import hashlib, json, pathlib, sys
args = {row.split('=', 1)[0]: row.split('=', 1)[1] for row in sys.argv if row.startswith('-DA')}
plan_path = pathlib.Path(args['-DAReleaseFixturePlan'])
plan_bytes = plan_path.read_bytes()
plan = json.loads(plan_bytes)
leaders = ['Prisoner', 'IndustrialAdvisor', 'Governor', 'AlliedForgeLord']
routes = []
for index, fixture in enumerate(plan['fixtures']):
    routes.append({'route': fixture['route'], 'sourceSha256': fixture['sourceSha256'],
        'reachedFirstAscension': True, 'aftermath': {'loyaltyBefore': 0,
        'loyalty': [15, 30, 45, 60][index], 'loyaltyActionSha256': f'{80 + index:064x}',
        'daxtonState': leaders[index], 'historySha256': f'{10 + index:064x}',
        'damageSha256': f'{20 + index:064x}', 'relationshipSha256': f'{30 + index:064x}'}})
names = ['first_city_built', 'mid_nia_quest', 'active_foundry_shortage', 'mid_iron_veil', 'daxton_phase_transition', 'immediately_after_ascension']
payload = {'schemaVersion': 1, 'runId': 'run-partial-001', 'buildId': 'build-partial-001',
    'fixturePlanSha256': hashlib.sha256(plan_bytes).hexdigest(), 'routes': routes,
    'saveCheckpoints': [{'checkpoint': name, 'passed': True, 'snapshotSha256': f'{90 + i:064x}'} for i, name in enumerate(names)]}
out = pathlib.Path(args['-DAReleaseAutomationEvidence'])
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(payload, sort_keys=True, separators=(',', ':')) + '\\n')
print('release automation complete')
""",
                encoding="utf-8",
            )
            fake_editor.chmod(os.stat(fake_editor).st_mode | 0o111)
            evidence_path = root / "Build" / "Release" / "Task28ReleaseEvidence.json"
            completed = subprocess.run(
                [
                    "python3",
                    str(TOOL_PATH),
                    "run",
                    "--root",
                    str(root),
                    "--unreal-editor",
                    str(fake_editor),
                    "--run-id",
                    "run-partial-001",
                    "--build-id",
                    "build-partial-001",
                    "--evidence",
                    str(evidence_path),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(completed.returncode, 5, completed.stdout + completed.stderr)
            evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
            self.assertEqual(evidence["state"], "NOT_READY")
            self.assertIsNotNone(evidence["automationEvidence"])
            self.assertEqual(len(evidence["routeEvidence"]), 4)
            self.assertEqual(len(evidence["saveCheckpointEvidence"]), 6)
            self.assertTrue(evidence["claims"]["automationPassed"])
            self.assertTrue(evidence["claims"]["fourRoutesPassed"])
            self.assertTrue(evidence["claims"]["saveRegressionPassed"])
            self.assertFalse(evidence["claims"]["releaseCandidateReady"])
            validated = load_tool().load_and_validate_release_evidence(root, evidence_path)
            self.assertEqual(validated["state"], "NOT_READY")

            partial_automation = copy.deepcopy(evidence["automationEvidence"])
            partial_routes = copy.deepcopy(evidence["routeEvidence"])
            partial_checkpoints = copy.deepcopy(evidence["saveCheckpointEvidence"])
            invalid_candidate = root / "Artifacts" / "Release" / "invalid-candidate.json"
            invalid_candidate.parent.mkdir(parents=True, exist_ok=True)
            invalid_candidate.write_bytes(canonical_bytes({"schemaVersion": 1}))
            finalized = subprocess.run(
                [
                    "python3", str(TOOL_PATH), "finalize", "--root", str(root),
                    "--plan", str(root / evidence["fixturePlanEvidence"]["path"]),
                    "--candidate", str(invalid_candidate), "--evidence", str(evidence_path),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(finalized.returncode, 2, finalized.stdout + finalized.stderr)
            after_finalize = json.loads(evidence_path.read_text(encoding="utf-8"))
            self.assertEqual(after_finalize["automationEvidence"], partial_automation)
            self.assertEqual(after_finalize["routeEvidence"], partial_routes)
            self.assertEqual(after_finalize["saveCheckpointEvidence"], partial_checkpoints)

    def test_route_sources_generate_production_loyalty_action_contracts(self):
        tool = load_tool()
        fixtures = tool.load_and_validate_fixtures(ROOT)
        for fixture in fixtures:
            action = fixture["loyaltyAction"]
            self.assertEqual(action["system"], "UDACaptureComponent")
            self.assertEqual(action["outcome"], "Gift")
            self.assertGreater(action["giftLoyaltyReward"], 0)
            self.assertNotIn("postConflictLoyalty", fixture)
        self.assertEqual(
            len({fixture["loyaltyAction"]["giftLoyaltyReward"] for fixture in fixtures}), 4
        )
        plan = tool.generate_fixture_plan(ROOT, fixtures)
        self.assertTrue(all("loyaltyAction" in row for row in plan["fixtures"]))

    def test_release_harness_earns_loyalty_through_capture_authority(self):
        source = (
            ROOT / "Source" / "DominionTests" / "Private" / "Release"
            / "VerticalSliceReleaseSpec.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("UDACaptureComponent", source)
        self.assertIn("BeginCapture", source)
        self.assertIn("AdvanceCapture", source)
        self.assertIn("ResolveOutcome(", source)
        self.assertIn("EDACaptureOutcome::Gift", source)
        self.assertIn("capture.gifted", source)
        self.assertIn("LoyaltyActionSha256", source)
        self.assertNotIn("PostConflictLoyalty =", source)

    def test_release_harness_hashes_complete_round_trip_save_bytes(self):
        source = (
            ROOT / "Source" / "DominionTests" / "Private" / "Release"
            / "VerticalSliceReleaseSpec.cpp"
        ).read_text(encoding="utf-8")
        checkpoint_function = source.split("bool RoundTripCheckpoint", 1)[1].split(
            "FDACampaignSnapshot MakeFirstCityCheckpoint", 1
        )[0]
        self.assertGreaterEqual(checkpoint_function.count("SaveCampaign"), 2)
        self.assertIn("SourceBytes == ReloadedBytes", checkpoint_function)
        self.assertIn("SnapshotSha256", checkpoint_function)
        self.assertIn('SetBoolField(TEXT("passed")', source)

    def test_automation_requires_explicit_successful_checkpoint_results(self):
        tool = load_tool()
        with tempfile.TemporaryDirectory() as temporary:
            root, _, plan_path, candidate = build_complete_release_root(temporary)
            automation = json.loads(
                (root / candidate["automationEvidence"]["path"]).read_text(encoding="utf-8")
            )
            tool.validate_automation_evidence(root, automation, plan_path)
            automation["saveCheckpoints"][2]["passed"] = False
            with self.assertRaisesRegex(ValueError, "active_foundry_shortage"):
                tool.validate_automation_evidence(root, automation, plan_path)

            automation = json.loads(
                (root / candidate["automationEvidence"]["path"]).read_text(encoding="utf-8")
            )
            automation["routes"][0]["aftermath"]["loyalty"] = 16
            with self.assertRaisesRegex(ValueError, "Gift reward"):
                tool.validate_automation_evidence(root, automation, plan_path)

    def test_release_harness_fingerprints_damage_consequences_not_fixture_identity(self):
        source = (
            ROOT / "Source" / "DominionTests" / "Private" / "Release"
            / "VerticalSliceReleaseSpec.cpp"
        ).read_text(encoding="utf-8")
        damage_function = source.split("FString DamageFingerprint", 1)[1].split(
            "FString RelationshipFingerprint", 1
        )[0]
        self.assertNotIn("WorldAssetId.ToString", damage_function)
        self.assertIn("StructuralIntegrity", damage_function)

    def test_four_route_sources_validate_and_generate_one_deterministic_plan(self):
        tool = load_tool()
        fixtures = tool.load_and_validate_fixtures(ROOT)

        self.assertEqual(
            [row["route"] for row in fixtures],
            ["Force", "Economic", "Influence", "Alliance"],
        )
        for fixture in fixtures:
            self.assertEqual(fixture["commitBoundary"], "IMMEDIATELY_BEFORE_FINAL_ROUTE_COMMIT")
            self.assertTrue(fixture["sourceAuthorities"])
            self.assertFalse(
                {"conquestState", "resolvedRoute", "ascensionState", "debugOverrides"}
                & set(fixture)
            )
            self.assertEqual(
                set(fixture["expectedAftermath"]),
                {"loyalty", "daxtonState", "historyTags", "damageFingerprint", "relationship"},
            )

        first = tool.generate_fixture_plan(ROOT, fixtures)
        second = tool.generate_fixture_plan(ROOT, fixtures)
        self.assertEqual(first, second)
        self.assertEqual(first["schemaVersion"], 1)
        self.assertEqual(len(first["fixtures"]), 4)
        self.assertEqual(len({row["sourceSha256"] for row in first["fixtures"]}), 4)

    def test_checked_evidence_consumes_task27_and_stays_not_ready(self):
        tool = load_tool()
        evidence = tool.load_and_validate_release_evidence(
            ROOT,
            ROOT / "Build" / "Release" / "Task28ReleaseEvidence.json",
        )

        self.assertEqual(evidence["state"], "NOT_READY")
        self.assertEqual(evidence["task27PerformanceState"], "NOT_READY")
        self.assertFalse(evidence["claims"]["releaseCandidateReady"])
        self.assertIsNone(evidence["automationEvidence"])
        self.assertIsNone(evidence["routeEvidence"])
        self.assertIsNone(evidence["saveCheckpointEvidence"])
        self.assertIsNone(evidence["controllerEvidence"])
        self.assertIsNone(evidence["onboardingEvidence"])
        self.assertIsNone(evidence["defectEvidence"])

    def test_checked_evidence_rejects_fabricated_ready_state(self):
        tool = load_tool()
        checked_path = ROOT / "Build" / "Release" / "Task28ReleaseEvidence.json"
        fabricated = json.loads(checked_path.read_text(encoding="utf-8"))
        fabricated["state"] = "READY"
        fabricated["claims"] = {key: True for key in fabricated["claims"]}
        fabricated["notReadyReasons"] = []
        fabricated["commands"]["releaseAutomation"]["exitCode"] = 0
        fabricated["routeEvidence"] = complete_candidate()["routes"]
        fabricated["saveCheckpointEvidence"] = complete_candidate()["saveCheckpoints"]
        fabricated["controllerEvidence"] = complete_candidate()["controller"]
        fabricated["onboardingEvidence"] = complete_candidate()["onboarding"]
        fabricated["defectEvidence"] = complete_candidate()["defects"]

        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "fabricated-ready.json"
            path.write_text(json.dumps(fabricated), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "READY evidence requires exact CAPTURED Task 27"):
                tool.load_and_validate_release_evidence(ROOT, path)

    def test_candidate_rejects_duplicate_aftermath_and_missing_human_evidence(self):
        tool = load_tool()
        candidate = complete_candidate()
        candidate["routes"][1]["aftermath"] = copy.deepcopy(candidate["routes"][0]["aftermath"])
        with self.assertRaisesRegex(ValueError, "route aftermath"):
            tool.validate_release_candidate(candidate)

        candidate = complete_candidate()
        candidate["controller"]["observedByHuman"] = False
        with self.assertRaisesRegex(ValueError, "controller"):
            tool.validate_release_candidate(candidate)

        candidate = complete_candidate()
        candidate["onboarding"]["uncoachedFirstTimeTesters"] = 0
        with self.assertRaisesRegex(ValueError, "onboarding"):
            tool.validate_release_candidate(candidate)

    def test_candidate_binds_every_external_evidence_file_by_sha256(self):
        tool = load_tool()
        candidate = complete_candidate()
        candidate["routes"][0]["video"] = "Artifacts/Release/Force.mp4"
        with self.assertRaisesRegex(ValueError, "Force video"):
            tool.validate_release_candidate(candidate)

    def test_candidate_requires_all_six_checkpoints_and_zero_high_severity_defects(self):
        tool = load_tool()
        candidate = complete_candidate()
        candidate["saveCheckpoints"] = candidate["saveCheckpoints"][:-1]
        with self.assertRaisesRegex(ValueError, "save checkpoint"):
            tool.validate_release_candidate(candidate)

        candidate = complete_candidate()
        candidate["defects"]["High"] = 1
        with self.assertRaisesRegex(ValueError, "defect gate"):
            tool.validate_release_candidate(candidate)

        candidate = complete_candidate()
        candidate["defects"]["acceptedMedium"] = []
        candidate["defects"]["Medium"] = 1
        with self.assertRaisesRegex(ValueError, "accepted Medium"):
            tool.validate_release_candidate(candidate)

    def test_missing_unreal_runner_records_127_without_fabricating_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            evidence_path = Path(temporary) / "release-evidence.json"
            completed = subprocess.run(
                [
                    "python3",
                    str(TOOL_PATH),
                    "run",
                    "--root",
                    str(ROOT),
                    "--unreal-editor",
                    "DefinitelyMissing-UnrealEditor-Cmd",
                    "--run-id",
                    "missing-ue-run-001",
                    "--build-id",
                    "unbuilt-source-001",
                    "--evidence",
                    str(evidence_path),
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(completed.returncode, 127, completed.stdout + completed.stderr)
            evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
            self.assertEqual(evidence["state"], "NOT_READY")
            self.assertEqual(evidence["commands"]["releaseAutomation"]["exitCode"], 127)
            self.assertEqual(
                evidence["commands"]["releaseAutomation"]["command"][1],
                "DominionAscendant.uproject",
            )
            self.assertFalse(evidence["claims"]["releaseCandidateReady"])
            self.assertIsNone(evidence["routeEvidence"])
            log_ref = evidence["commands"]["releaseAutomation"]["log"]
            self.assertEqual(set(log_ref), {"path", "sha256"})
            log_path = ROOT / log_ref["path"]
            original = log_path.read_bytes()
            try:
                log_path.write_bytes(original + b"mutated\n")
                with self.assertRaisesRegex(ValueError, "digest mismatch"):
                    tool = load_tool()
                    tool.load_and_validate_release_evidence(ROOT, evidence_path)
            finally:
                log_path.write_bytes(original)

    def test_runner_cannot_reuse_a_stale_automation_sidecar(self):
        tool = load_tool()
        plan_path = ROOT / "Intermediate" / "Release" / "VerticalSliceRouteFixture.plan.json"
        sidecar_path = ROOT / "Intermediate" / "Release" / "Task28ReleaseAutomationEvidence.json"
        plan = tool.generate_fixture_plan(ROOT)
        route_sources = {row["route"]: row["sourceSha256"] for row in plan["fixtures"]}
        automation = {
            "schemaVersion": 1,
            "fixturePlanSha256": tool._sha256_bytes(tool._canonical_bytes(plan)),
            "routes": [
                {
                    "route": route["route"],
                    "sourceSha256": route_sources[route["route"]],
                    "reachedFirstAscension": True,
                    "aftermath": route["aftermath"],
                }
                for route in complete_candidate()["routes"]
            ],
            "saveCheckpoints": tool.CHECKPOINTS,
        }
        plan_path.parent.mkdir(parents=True, exist_ok=True)
        plan_path.write_bytes(tool._canonical_bytes(plan))
        sidecar_path.write_bytes(tool._canonical_bytes(automation))
        try:
            with tempfile.TemporaryDirectory() as temporary:
                fake_editor = Path(temporary) / "fake-unreal-editor"
                fake_editor.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
                fake_editor.chmod(os.stat(fake_editor).st_mode | 0o111)
                evidence_path = Path(temporary) / "release-evidence.json"
                completed = subprocess.run(
                    [
                        "python3",
                        str(TOOL_PATH),
                        "run",
                        "--root",
                        str(ROOT),
                        "--unreal-editor",
                        str(fake_editor),
                        "--run-id",
                        "stale-sidecar-run-001",
                        "--build-id",
                        "stale-sidecar-build-001",
                        "--evidence",
                        str(evidence_path),
                    ],
                    cwd=ROOT,
                    text=True,
                    capture_output=True,
                    check=False,
                )
                self.assertEqual(completed.returncode, 3, completed.stdout + completed.stderr)
                evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
                self.assertEqual(evidence["state"], "NOT_READY")
                self.assertIn("wrote no reconciled", evidence["notReadyReasons"][0])
        finally:
            sidecar_path.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main()
