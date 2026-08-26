#!/usr/bin/env python3
"""Deterministic Task 27 benchmark plan generator and honest UE runner.

This tool never synthesizes performance numbers. It emits NOT_READY when UE is
absent and accepts CAPTURED only when a caller supplies complete measurements.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import pathlib
import re
import shutil
import subprocess
import sys
import uuid


SCENE_KEYS = {
    "$schema", "schemaVersion", "sceneId", "generatorVersion", "deterministicSeed",
    "gameplayAssetCount", "gameplayAssetClass", "citizensByLOD", "weatherActive",
    "alliedSquadCount", "enemySquadCount", "vehicleCount", "constructionEffectCount",
    "constructionEffectAsset", "damagedBuildingCount", "modeTransition",
}
BUDGET_KEYS = {
    "$schema", "schemaVersion", "resolution", "qualityPreset", "referenceHardware",
    "targetAverageFramesPerSecond", "minimumWorstCaseFramesPerSecond",
    "severeDevelopmentCycleHitchMilliseconds", "maximumModeTransitionSeconds",
    "minimumFrameSamples", "developmentCycles", "enabledEventCount",
    "maximumSaveBytes", "maximumSaveGrowthBytes",
}
EVENT_NAMES = ("prepare", "automation", "cook")
CLAIM_KEYS = {"performanceTargetsPassed", "soakPassed", "cookPassed"}
MEASUREMENT_KEYS = {
    "frameSampleCount", "averageFramesPerSecond", "worstFrameFramesPerSecond",
    "worstDevelopmentCycleMilliseconds", "modeTransitionSeconds", "developmentCycles",
    "enabledEventCount", "playerEconomyDevelopmentCycles", "tradeWorldTicks",
    "forgeweaveWorldTicks", "utilityTopologyNodeCount", "utilityTopologyResolved",
    "populationStayedNonnegative", "eventEmissionCount", "eventTransitionCount",
    "eventRunawayDetected", "staleRequiredQuestDetected", "requiredQuestResolutionFailed",
    "initialSaveBytes", "finalSaveBytes", "repeatabilityDigestA", "repeatabilityDigestB",
}
HARDWARE_KEYS = {"cpu", "gpu", "systemMemoryGiB", "storage"}
SHA256_PATTERN = re.compile(r"[0-9a-f]{64}")
CANONICAL_SCENE_ID = "benchmark.vertical_slice.synara_capital"
DEFINITION_IDS = (
    "special.founder_hall", "synara.adaptive_habitat", "universal.microgrid_station",
    "universal.water_reclaimer", "synara.cognitive_operations_tower",
    "synara.autonomous_exchange", "universal.corner_exchange",
    "synara.agency_forum", "fusion.autonomous_factory", "synara.guardian_drone_cohort",
)
PRESENTATION_ASSETS = {
    "special.founder_hall": "/Game/Buildings/Synara/Primary01/DA_Presentation_AdaptiveHabitat.DA_Presentation_AdaptiveHabitat",
    "synara.adaptive_habitat": "/Game/Buildings/Synara/Primary01/DA_Presentation_AdaptiveHabitat.DA_Presentation_AdaptiveHabitat",
    "universal.microgrid_station": "/Game/Buildings/Universal/Primary43/DA_Presentation_MicrogridStation.DA_Presentation_MicrogridStation",
    "universal.water_reclaimer": "/Game/Buildings/Universal/Primary44/DA_Presentation_WaterReclaimer.DA_Presentation_WaterReclaimer",
    "synara.cognitive_operations_tower": "/Game/Buildings/Synara/Primary03/DA_Presentation_CognitiveOperationsTower.DA_Presentation_CognitiveOperationsTower",
    "synara.autonomous_exchange": "/Game/Buildings/Synara/Primary05/DA_Presentation_AutonomousExchange.DA_Presentation_AutonomousExchange",
    "universal.corner_exchange": "/Game/Buildings/Universal/Primary41/DA_Presentation_CornerExchange.DA_Presentation_CornerExchange",
    "synara.agency_forum": "/Game/Buildings/Synara/Primary11/DA_Presentation_AgencyForum.DA_Presentation_AgencyForum",
    "fusion.autonomous_factory": "/Game/Buildings/Fusion/Primary50/DA_Presentation_AutonomousFactory.DA_Presentation_AutonomousFactory",
    "synara.guardian_drone_cohort": "/Game/Buildings/Synara/Primary01/DA_Presentation_AdaptiveHabitat.DA_Presentation_AdaptiveHabitat",
}


def _read_json(path: pathlib.Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream, parse_constant=lambda value: (_ for _ in ()).throw(
            ValueError(f"non-finite JSON number {value!r} is forbidden")))
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain one JSON object")
    return value


def _write_json(path: pathlib.Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _exact_keys(value: dict, expected: set[str], label: str) -> None:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    if set(value) != expected:
        raise ValueError(f"{label} keys differ: expected {sorted(expected)}, got {sorted(value)}")


def _integer(value: object, label: str, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise ValueError(f"{label} must be an integer >= {minimum}")
    return value


def _finite_number(value: object, label: str, minimum: float = 0.0,
                   exclusive: bool = False) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) \
            or not math.isfinite(value) or (value <= minimum if exclusive else value < minimum):
        comparison = ">" if exclusive else ">="
        raise ValueError(f"{label} must be a finite number {comparison} {minimum}")
    return float(value)


def _sha256(value: object, label: str) -> str:
    if not isinstance(value, str) or SHA256_PATTERN.fullmatch(value) is None:
        raise ValueError(f"{label} must be exactly 64 lowercase hexadecimal characters")
    return value


def load_and_validate_scene(path: pathlib.Path | str) -> dict:
    path = pathlib.Path(path)
    scene = _read_json(path)
    _exact_keys(scene, SCENE_KEYS, "scene")
    expected_scalars = {
        "$schema": "VerticalSliceBenchmark.scene.schema.json",
        "schemaVersion": 1,
        "sceneId": "benchmark.vertical_slice.synara_capital",
        "generatorVersion": 1,
        "gameplayAssetCount": 50,
        "gameplayAssetClass": "/Script/DominionGameplay.DAWorldAsset",
        "weatherActive": True,
        "alliedSquadCount": 3,
        "enemySquadCount": 3,
        "vehicleCount": 1,
        "constructionEffectCount": 1,
        "constructionEffectAsset": "/Game/VFX/Synara/DA_VFX_Synara_Construction.DA_VFX_Synara_Construction",
        "damagedBuildingCount": 1,
    }
    for key, expected in expected_scalars.items():
        if type(expected) is int and (isinstance(scene[key], bool)
                                      or not isinstance(scene[key], int)):
            raise ValueError(f"scene.{key} must be an integer")
        if scene[key] != expected:
            raise ValueError(f"scene.{key} must equal {expected!r}")
    _integer(scene["deterministicSeed"], "scene.deterministicSeed", 1)
    rows = scene["citizensByLOD"]
    if not isinstance(rows, list) or len(rows) != 4:
        raise ValueError("scene.citizensByLOD must contain exactly four rows")
    for lod, row in enumerate(rows):
        _exact_keys(row, {"lod", "count", "representation"}, f"citizensByLOD[{lod}]")
        if row["lod"] != lod or isinstance(row["count"], bool) \
                or not isinstance(row["count"], int) or row["count"] <= 0:
            raise ValueError("citizen LOD rows must be ordered 0..3 with positive counts")
    expected_lods = [
        {"lod": 0, "count": 30, "representation": "full"},
        {"lod": 1, "count": 30, "representation": "detailed"},
        {"lod": 2, "count": 40, "representation": "aggregated"},
        {"lod": 3, "count": 20, "representation": "strategic"},
    ]
    if rows != expected_lods:
        raise ValueError("scene must represent exactly 30/30/40/20 citizens at LOD0/1/2/3")
    if scene["modeTransition"] != ["City", "Command", "Founder"]:
        raise ValueError("scene mode transition must be City -> Command -> Founder")
    return scene


def load_and_validate_budget(path: pathlib.Path | str) -> dict:
    path = pathlib.Path(path)
    budget = _read_json(path)
    _exact_keys(budget, BUDGET_KEYS, "budget")
    expected = {
        "$schema": "VerticalSliceBenchmark.budget.schema.json",
        "schemaVersion": 1,
        "resolution": {"width": 1920, "height": 1080},
        "qualityPreset": "High",
        "targetAverageFramesPerSecond": 60.0,
        "minimumWorstCaseFramesPerSecond": 30.0,
        "severeDevelopmentCycleHitchMilliseconds": 100.0,
        "maximumModeTransitionSeconds": 1.5,
        "developmentCycles": 1000,
        "enabledEventCount": 6,
    }
    for key, value in expected.items():
        if budget[key] != value:
            raise ValueError(f"budget.{key} must equal {value!r}")
    _integer(budget["minimumFrameSamples"], "budget.minimumFrameSamples", 600)
    _integer(budget["maximumSaveBytes"], "budget.maximumSaveBytes", 1)
    _integer(budget["maximumSaveGrowthBytes"], "budget.maximumSaveGrowthBytes")
    hardware = budget["referenceHardware"]
    _exact_keys(hardware, HARDWARE_KEYS, "budget.referenceHardware")
    expected_hardware = {
        "cpu": "Ryzen 5 5600 or equivalent",
        "gpu": "RTX 3060 or RX 6700 XT class",
        "systemMemoryGiB": 16,
        "storage": "SSD",
    }
    if hardware != expected_hardware:
        raise ValueError("budget reference hardware must retain the exact v1.1 reference class")
    return budget


def _stable_guid(seed: int, category: str, index: int) -> str:
    namespace = uuid.UUID("da270000-0000-0000-0000-000000001701")
    return str(uuid.uuid5(namespace, f"{seed}:{category}:{index}"))


def build_plan(scene: dict, budget: dict) -> dict:
    assets = []
    for index in range(scene["gameplayAssetCount"]):
        definition_id = DEFINITION_IDS[index % len(DEFINITION_IDS)]
        assets.append({
            "worldAssetId": _stable_guid(scene["deterministicSeed"], "asset", index),
            "definitionId": definition_id,
            "presentationAsset": PRESENTATION_ASSETS[definition_id],
            "grid": {"x": index % 10, "y": index // 10},
            "constructionState": "Damaged" if index == 1 else "Operational",
            "constructionEffect": index == 0,
        })
    citizens = []
    offset = 0
    for row in scene["citizensByLOD"]:
        for _ in range(row["count"]):
            citizens.append({
                "citizenId": f"citizen.benchmark.{offset:03d}",
                "lod": row["lod"],
                "representation": row["representation"],
                "visibleActorRequired": row["lod"] <= 1,
            })
            offset += 1
    squads = [
        {"squadId": f"squad.allied.{index + 1}", "side": "allied"}
        for index in range(scene["alliedSquadCount"])
    ] + [
        {"squadId": f"squad.enemy.{index + 1}", "side": "enemy"}
        for index in range(scene["enemySquadCount"])
    ]
    return {
        "schemaVersion": 1,
        "sceneId": scene["sceneId"],
        "generatorVersion": scene["generatorVersion"],
        "deterministicSeed": scene["deterministicSeed"],
        "gameplayAssetClass": scene["gameplayAssetClass"],
        "assets": assets,
        "citizens": citizens,
        "weather": {"active": True},
        "squads": squads,
        "vehicles": [{"vehicleId": "vehicle.allied.1"}],
        "constructionEffectAsset": scene["constructionEffectAsset"],
        "modeTransition": scene["modeTransition"],
        "resolution": budget["resolution"],
        "qualityPreset": budget["qualityPreset"],
        "developmentCycles": budget["developmentCycles"],
        "enabledEventCount": budget["enabledEventCount"],
        "budgets": {
            "targetAverageFramesPerSecond": budget["targetAverageFramesPerSecond"],
            "minimumWorstCaseFramesPerSecond": budget["minimumWorstCaseFramesPerSecond"],
            "severeDevelopmentCycleHitchMilliseconds": budget["severeDevelopmentCycleHitchMilliseconds"],
            "maximumModeTransitionSeconds": budget["maximumModeTransitionSeconds"],
            "minimumFrameSamples": budget["minimumFrameSamples"],
            "maximumSaveBytes": budget["maximumSaveBytes"],
            "maximumSaveGrowthBytes": budget["maximumSaveGrowthBytes"],
        },
    }


def _command(argv: list[str], exit_code: int | None = None) -> dict:
    return {"argv": argv, "exitCode": exit_code}


def _empty_evidence(scene_id: str, plan_sha256: str | None) -> dict:
    return {
        "$schema": "PerformanceEvidence.schema.json",
        "schemaVersion": 1,
        "sceneId": scene_id,
        "planSha256": plan_sha256,
        "state": "NOT_READY",
        "readinessReasons": ["UE benchmark execution has not produced a measured capture."],
        "hardware": None,
        "commands": {
            "prepare": _command([]),
            "automation": _command([]),
            "cook": _command([]),
        },
        "measurements": None,
        "claims": {
            "performanceTargetsPassed": False,
            "soakPassed": False,
            "cookPassed": False,
        },
    }


def generate_plan(scene_path: pathlib.Path | str, budget_path: pathlib.Path | str,
                  plan_output: pathlib.Path | str, evidence_output: pathlib.Path | str) -> dict:
    scene = load_and_validate_scene(scene_path)
    budget = load_and_validate_budget(budget_path)
    plan = build_plan(scene, budget)
    encoded = (json.dumps(plan, indent=2, sort_keys=True) + "\n").encode("utf-8")
    plan_output = pathlib.Path(plan_output)
    plan_output.parent.mkdir(parents=True, exist_ok=True)
    plan_output.write_bytes(encoded)
    evidence = _empty_evidence(scene["sceneId"], hashlib.sha256(encoded).hexdigest())
    _write_json(pathlib.Path(evidence_output), evidence)
    return plan


def _validate_measurements(measurements: dict, budget: dict) -> tuple[bool, bool]:
    _exact_keys(measurements, MEASUREMENT_KEYS, "measurements")
    frame_count = _integer(measurements["frameSampleCount"], "measurements.frameSampleCount")
    average_fps = _finite_number(
        measurements["averageFramesPerSecond"], "measurements.averageFramesPerSecond", exclusive=True)
    worst_fps = _finite_number(
        measurements["worstFrameFramesPerSecond"], "measurements.worstFrameFramesPerSecond", exclusive=True)
    worst_cycle_ms = _finite_number(measurements["worstDevelopmentCycleMilliseconds"],
                                    "measurements.worstDevelopmentCycleMilliseconds")
    transitions = measurements["modeTransitionSeconds"]
    if not isinstance(transitions, list) or len(transitions) != 3:
        raise ValueError("measurements require exactly three mode transitions")
    transition_values = [
        _finite_number(value, f"measurements.modeTransitionSeconds[{index}]")
        for index, value in enumerate(transitions)
    ]
    integer_fields = (
        "developmentCycles", "enabledEventCount", "playerEconomyDevelopmentCycles",
        "tradeWorldTicks", "forgeweaveWorldTicks", "utilityTopologyNodeCount",
        "eventEmissionCount", "eventTransitionCount", "initialSaveBytes", "finalSaveBytes",
    )
    integers = {name: _integer(measurements[name], f"measurements.{name}")
                for name in integer_fields}
    boolean_fields = (
        "utilityTopologyResolved", "populationStayedNonnegative", "eventRunawayDetected",
        "staleRequiredQuestDetected", "requiredQuestResolutionFailed",
    )
    for name in boolean_fields:
        if not isinstance(measurements[name], bool):
            raise ValueError(f"measurements.{name} must be a boolean")
    digest_a = _sha256(measurements["repeatabilityDigestA"],
                       "measurements.repeatabilityDigestA")
    digest_b = _sha256(measurements["repeatabilityDigestB"],
                       "measurements.repeatabilityDigestB")
    performance = (
        frame_count >= budget["minimumFrameSamples"]
        and average_fps >= budget["targetAverageFramesPerSecond"]
        and worst_fps >= budget["minimumWorstCaseFramesPerSecond"]
        and worst_cycle_ms <= budget["severeDevelopmentCycleHitchMilliseconds"]
        and max(transition_values) < budget["maximumModeTransitionSeconds"]
    )
    soak = (
        integers["developmentCycles"] == budget["developmentCycles"]
        and integers["enabledEventCount"] == budget["enabledEventCount"]
        and integers["playerEconomyDevelopmentCycles"] == budget["developmentCycles"]
        and integers["tradeWorldTicks"]
            == budget["developmentCycles"] // 5
        and integers["forgeweaveWorldTicks"]
            == budget["developmentCycles"] // 5
        and integers["utilityTopologyNodeCount"] >= 2
        and measurements["utilityTopologyResolved"]
        and measurements["populationStayedNonnegative"]
        and integers["eventEmissionCount"] <= budget["enabledEventCount"]
        and not measurements["eventRunawayDetected"]
        and not measurements["staleRequiredQuestDetected"]
        and not measurements["requiredQuestResolutionFailed"]
        and integers["finalSaveBytes"] <= budget["maximumSaveBytes"]
        and integers["finalSaveBytes"] - integers["initialSaveBytes"]
            <= budget["maximumSaveGrowthBytes"]
        and digest_a == digest_b
    )
    return performance, soak


def _derive_claims(evidence: dict, budget: dict) -> dict:
    performance, soak = _validate_measurements(evidence["measurements"], budget)
    exact_hardware = evidence["hardware"] == budget["referenceHardware"]
    return {
        "performanceTargetsPassed": performance and exact_hardware,
        "soakPassed": soak,
        "cookPassed": evidence["commands"]["cook"]["exitCode"] == 0,
    }


def _validate_evidence(evidence: dict, budget: dict,
                       plan_path: pathlib.Path | str | None = None,
                       allow_candidate: bool = False) -> dict:
    _exact_keys(evidence, {
        "$schema", "schemaVersion", "sceneId", "planSha256", "state", "readinessReasons",
        "hardware", "commands", "measurements", "claims",
    }, "evidence")
    if evidence["$schema"] != "PerformanceEvidence.schema.json" or evidence["schemaVersion"] != 1:
        raise ValueError("unsupported evidence schema")
    if evidence["sceneId"] != CANONICAL_SCENE_ID:
        raise ValueError(f"evidence.sceneId must equal {CANONICAL_SCENE_ID!r}")
    if not isinstance(evidence["state"], str) \
            or evidence["state"] not in {"NOT_RUN", "NOT_READY", "CAPTURED", "CANDIDATE"}:
        raise ValueError("invalid evidence state")
    if evidence["state"] == "CANDIDATE" and not allow_candidate:
        raise ValueError("candidate evidence is internal and cannot be consumed as final evidence")
    plan_digest = evidence["planSha256"]
    if evidence["state"] in {"CAPTURED", "CANDIDATE"}:
        _sha256(plan_digest, "evidence.planSha256")
        if plan_path is None:
            raise ValueError("captured evidence validation requires the authoritative plan path")
    elif plan_digest is not None:
        _sha256(plan_digest, "evidence.planSha256")
    if plan_path is not None:
        actual_digest = hashlib.sha256(pathlib.Path(plan_path).read_bytes()).hexdigest()
        if plan_digest != actual_digest:
            raise ValueError("evidence.planSha256 does not match the generated plan bytes")
    reasons = evidence["readinessReasons"]
    if not isinstance(reasons, list) or not all(isinstance(reason, str) and reason for reason in reasons):
        raise ValueError("evidence.readinessReasons must contain only non-empty strings")
    if set(evidence["commands"]) != set(EVENT_NAMES):
        raise ValueError("evidence commands must record preparation, Automation, and cook")
    for name in EVENT_NAMES:
        _exact_keys(evidence["commands"][name], {"argv", "exitCode"}, f"commands.{name}")
        argv = evidence["commands"][name]["argv"]
        exit_code = evidence["commands"][name]["exitCode"]
        if not isinstance(argv, list) or not all(isinstance(value, str) for value in argv):
            raise ValueError(f"commands.{name}.argv must be an array of strings")
        if exit_code is not None:
            _integer(exit_code, f"commands.{name}.exitCode")
    _exact_keys(evidence["claims"], CLAIM_KEYS, "claims")
    if not all(isinstance(value, bool) for value in evidence["claims"].values()):
        raise ValueError("evidence claims must be booleans")
    if evidence["state"] == "CANDIDATE":
        _exact_keys(evidence["hardware"], HARDWARE_KEYS, "hardware")
        if not isinstance(evidence["hardware"]["cpu"], str) or not evidence["hardware"]["cpu"] \
                or not isinstance(evidence["hardware"]["gpu"], str) or not evidence["hardware"]["gpu"] \
                or not isinstance(evidence["hardware"]["storage"], str) or not evidence["hardware"]["storage"]:
            raise ValueError("candidate hardware strings must be non-empty")
        _integer(evidence["hardware"]["systemMemoryGiB"], "hardware.systemMemoryGiB", 1)
        _validate_measurements(evidence["measurements"], budget)
        if not reasons or any(evidence["claims"].values()) \
                or any(evidence["commands"][name]["argv"]
                       or evidence["commands"][name]["exitCode"] is not None
                       for name in EVENT_NAMES):
            raise ValueError("candidate evidence must await command reconciliation without claims")
    elif evidence["state"] != "CAPTURED":
        if evidence["hardware"] is not None or evidence["measurements"] is not None \
                or any(evidence["claims"].values()) or not reasons:
            raise ValueError("uncaptured evidence must be a clean NOT_READY/NOT_RUN payload")
    else:
        _exact_keys(evidence["hardware"], HARDWARE_KEYS, "hardware")
        if not isinstance(evidence["hardware"]["cpu"], str) or not evidence["hardware"]["cpu"] \
                or not isinstance(evidence["hardware"]["gpu"], str) or not evidence["hardware"]["gpu"] \
                or not isinstance(evidence["hardware"]["storage"], str) or not evidence["hardware"]["storage"]:
            raise ValueError("captured hardware strings must be non-empty")
        _integer(evidence["hardware"]["systemMemoryGiB"], "hardware.systemMemoryGiB", 1)
        if reasons:
            raise ValueError("captured evidence cannot contain readiness reasons")
        if any(not evidence["commands"][name]["argv"]
               or evidence["commands"][name]["exitCode"] != 0 for name in EVENT_NAMES):
            raise ValueError("captured evidence requires successful Automation and cook commands")
        derived = _derive_claims(evidence, budget)
        if evidence["claims"] != derived:
            raise ValueError("captured claims do not reconcile with measurements, hardware, and commands")
    return evidence


def load_and_validate_evidence(evidence_path: pathlib.Path | str,
                               budget_path: pathlib.Path | str,
                               plan_path: pathlib.Path | str | None = None) -> dict:
    return _validate_evidence(
        _read_json(pathlib.Path(evidence_path)), load_and_validate_budget(budget_path), plan_path)


def _reset_not_ready(evidence_output: pathlib.Path, scene_id: str, plan_sha256: str,
                     prepare: dict, automation: dict, cook: dict, reasons: list[str]) -> None:
    evidence = _empty_evidence(scene_id, plan_sha256)
    evidence["commands"] = {"prepare": prepare, "automation": automation, "cook": cook}
    evidence["readinessReasons"] = reasons
    _write_json(evidence_output, evidence)


def run_ue(scene_path: pathlib.Path, budget_path: pathlib.Path, plan_output: pathlib.Path,
           evidence_output: pathlib.Path, project: pathlib.Path, editor: str,
           storage_class: str = "UNVERIFIED") -> int:
    scene = load_and_validate_scene(scene_path)
    budget = load_and_validate_budget(budget_path)
    generate_plan(scene_path, budget_path, plan_output, evidence_output)
    plan_sha256 = hashlib.sha256(plan_output.read_bytes()).hexdigest()
    candidate_output = evidence_output.with_name(evidence_output.name + ".candidate")
    candidate_output.unlink(missing_ok=True)
    prepare_argv = [editor, str(project), "-run=DAPresentationContent", "-unattended", "-nop4"]
    automation_argv = [
        editor, str(project), "-unattended", "-nop4",
        "-ResX=1920", "-ResY=1080", "-windowed",
        "-ExecCmds=Automation RunTests Dominion.Performance;Quit",
        "-TestExit=Automation Test Queue Empty", f"-DABenchmarkPlan={plan_output}",
        f"-DABenchmarkBudget={budget_path}",
        f"-DABenchmarkEvidenceCandidate={candidate_output}",
    ]
    if storage_class != "UNVERIFIED":
        automation_argv.append(f"-DABenchmarkStorage={storage_class}")
    cook_argv = [editor, str(project), "-run=cook", "-targetplatform=Windows", "-unattended", "-nop4"]
    resolved_editor = shutil.which(editor)
    if resolved_editor is None:
        _reset_not_ready(evidence_output, scene["sceneId"], plan_sha256,
            _command(prepare_argv, 127), _command(automation_argv, 127),
            _command(cook_argv, 127), [
            "UE5.8 UnrealEditor-Cmd is unavailable; preparation, Automation, and cook were not executable.",
            "No FPS, hitch, transition, soak, save-size, cook, or hardware claim was produced.",
        ])
        return 127
    prepare_argv[0] = resolved_editor
    automation_argv[0] = resolved_editor
    cook_argv[0] = resolved_editor
    prepare = subprocess.run(prepare_argv, cwd=project.parent, check=False)
    if prepare.returncode != 0:
        _reset_not_ready(evidence_output, scene["sceneId"], plan_sha256,
            _command(prepare_argv, prepare.returncode), _command(automation_argv),
            _command(cook_argv),
            ["Presentation asset preparation did not complete; no capture was accepted."])
        return prepare.returncode
    automation = subprocess.run(automation_argv, cwd=project.parent, check=False)
    if automation.returncode != 0:
        _reset_not_ready(evidence_output, scene["sceneId"], plan_sha256,
            _command(prepare_argv, 0), _command(automation_argv, automation.returncode),
            _command(cook_argv),
            ["UE Automation did not complete successfully; no capture was accepted."])
        candidate_output.unlink(missing_ok=True)
        return automation.returncode
    cook = subprocess.run(cook_argv, cwd=project.parent, check=False)
    if cook.returncode != 0:
        _reset_not_ready(evidence_output, scene["sceneId"], plan_sha256,
            _command(prepare_argv, 0), _command(automation_argv, 0),
            _command(cook_argv, cook.returncode),
            ["Cook did not complete successfully; no capture was accepted."])
        candidate_output.unlink(missing_ok=True)
        return cook.returncode
    try:
        candidate = _read_json(candidate_output)
        _validate_evidence(candidate, budget, plan_output, allow_candidate=True)
        candidate["state"] = "CAPTURED"
        candidate["readinessReasons"] = []
        candidate["commands"] = {
            "prepare": _command(prepare_argv, prepare.returncode),
            "automation": _command(automation_argv, automation.returncode),
            "cook": _command(cook_argv, cook.returncode),
        }
        candidate["claims"] = _derive_claims(candidate, budget)
        _validate_evidence(candidate, budget, plan_output)
    except (OSError, ValueError, json.JSONDecodeError, KeyError, TypeError) as error:
        _reset_not_ready(evidence_output, scene["sceneId"], plan_sha256,
            _command(prepare_argv, 0), _command(automation_argv, 0), _command(cook_argv, 0),
            [f"Captured candidate failed full validation: {error}"])
        candidate_output.unlink(missing_ok=True)
        return 3
    _write_json(evidence_output, candidate)
    candidate_output.unlink(missing_ok=True)
    return 0


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    for name in ("validate", "generate", "run"):
        sub = subparsers.add_parser(name)
        sub.add_argument("--scene", type=pathlib.Path, required=True)
        sub.add_argument("--budget", type=pathlib.Path, required=True)
        if name in {"generate", "run"}:
            sub.add_argument("--plan-output", type=pathlib.Path, required=True)
            sub.add_argument("--evidence-output", type=pathlib.Path, required=True)
        if name == "run":
            sub.add_argument("--project", type=pathlib.Path, required=True)
            sub.add_argument("--editor", default="UnrealEditor-Cmd")
            sub.add_argument("--storage-class", choices=("UNVERIFIED", "SSD"),
                             default="UNVERIFIED",
                             help="operator-verified benchmark storage class; defaults fail-closed")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        if args.command == "validate":
            load_and_validate_scene(args.scene)
            load_and_validate_budget(args.budget)
            return 0
        if args.command == "generate":
            generate_plan(args.scene, args.budget, args.plan_output, args.evidence_output)
            return 0
        return run_ue(args.scene, args.budget, args.plan_output, args.evidence_output,
                      args.project, args.editor, args.storage_class)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(error, file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
