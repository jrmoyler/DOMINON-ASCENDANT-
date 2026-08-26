#!/usr/bin/env python3
"""Fail-closed Task 28 fixture planner and release-evidence reconciler."""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path


ROUTES = ("Force", "Economic", "Influence", "Alliance")
ROUTE_DIRECTORIES = {
    "Force": "ForceRoute",
    "Economic": "EconomicRoute",
    "Influence": "InfluenceRoute",
    "Alliance": "AllianceRoute",
}
BROKER_TAGS = {
    "Force": "broker_force",
    "Economic": "broker_economic",
    "Influence": "broker_influence",
    "Alliance": "broker_alliance",
}
LEADER_STATES = {
    "Force": "Prisoner",
    "Economic": "IndustrialAdvisor",
    "Influence": "Governor",
    "Alliance": "AlliedForgeLord",
}
CHOICES = {
    "Force": (False, True, False),
    "Economic": (True, False, True),
    "Influence": (False, True, True),
    "Alliance": (True, True, True),
}
LOYALTY_REWARDS = {"Force": 15, "Economic": 30, "Influence": 45, "Alliance": 60}
REQUIRED_AUTHORITIES = {
    "Force": {
        "region.ironheart.owner.synara",
        "conflict.control_zone.iron_gate",
        "conflict.control_zone.grand_forge",
        "conflict.capture.heavy_carrier",
        "conflict.capture.command_bastion",
        "conflict.damage.grand_forge",
        "conflict.capture.gift_loyalty",
        "history.forgeweave_elite_defeated",
        "history.daxton_encounter_resolved",
        "history.daxton_surrendered",
        "quest.operation_iron_veil.completed",
    },
    "Economic": {
        "trade.contract.forge.0.completed",
        "trade.contract.forge.1.completed",
        "trade.contract.forge.2.completed",
        "trade.contract.forge.3.completed",
        "region.freight_corridor.owner.synara",
        "trade.relationship.component_dependence",
        "campaign.foundry_shortage.market_exploitation",
        "history.daxton_restructuring_resolved",
        "quest.supply_noose.completed",
        "conflict.capture.gift_loyalty",
    },
    "Influence": {
        "quest.workers_signal.completed",
        "history.workers_protected",
        "history.mara_numbers_worker_coalition",
        "history.grand_forge_preserved",
        "campaign.foundry_shortage.brokered_compact",
        "campaign.faction.human_agency.support",
        "conflict.capture.gift_loyalty",
    },
    "Alliance": {
        "diplomacy.trust.reason_ledger",
        "diplomacy.respect.reason_ledger",
        "diplomacy.compatibility.reason_ledger",
        "campaign.foundry_shortage.brokered_compact",
        "quest.third_foundry.completed",
        "history.joint_forgeweave_crisis_success",
        "history.forge_relic_voluntary_transfer",
        "conflict.capture.gift_loyalty",
    },
}
CHECKPOINTS = (
    "first_city_built",
    "mid_nia_quest",
    "active_foundry_shortage",
    "mid_iron_veil",
    "daxton_phase_transition",
    "immediately_after_ascension",
)
CONTROLLER_ACTIONS = (
    "build",
    "card_inspect",
    "combat",
    "command",
    "diplomacy",
    "conquest",
    "save_load",
)
ONBOARDING_ACTIONS = (
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
)
CLAIM_KEYS = (
    "releaseCandidateReady",
    "fourRoutesPassed",
    "saveRegressionPassed",
    "controllerPassed",
    "onboardingPassed",
    "defectGatePassed",
    "task27Passed",
    "automationPassed",
)
HEX_DIGITS = frozenset("0123456789abcdef")


def _reject_duplicate_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON member: {key}")
        result[key] = value
    return result


def _load_json(path: Path):
    try:
        return json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_reject_duplicate_pairs,
            parse_constant=lambda value: (_ for _ in ()).throw(
                ValueError(f"non-finite JSON number: {value}")
            ),
        )
    except OSError as error:
        raise ValueError(f"cannot read {path}: {error}") from error


def _canonical_bytes(value) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def _sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _sha256_file(path: Path) -> str:
    return _sha256_bytes(path.read_bytes())


def _require_exact_keys(value, expected, label):
    if not isinstance(value, dict) or set(value) != set(expected):
        actual = sorted(value) if isinstance(value, dict) else type(value).__name__
        raise ValueError(f"{label} fields must be exactly {sorted(expected)}; got {actual}")


def _require_sha256(value, label):
    if not isinstance(value, str) or len(value) != 64 or any(ch not in HEX_DIGITS for ch in value):
        raise ValueError(f"{label} must be a lowercase SHA-256")


def _require_nonempty_path(value, label):
    if not isinstance(value, str) or not value.strip() or Path(value).is_absolute() or ".." in Path(value).parts:
        raise ValueError(f"{label} must be a non-empty repository-relative path")


def _require_number(value, label, minimum=0.0, maximum=100.0):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(f"{label} must be a finite number")
    if value < minimum or value > maximum:
        raise ValueError(f"{label} must be within [{minimum}, {maximum}]")


def _require_identifier(value, label):
    if (
        not isinstance(value, str)
        or not value.strip()
        or len(value) > 128
        or any(not (ch.isalnum() or ch in "._:-") for ch in value)
    ):
        raise ValueError(f"{label} must be a stable non-empty identifier")


def _fixture_source_path(root: Path, route: str) -> Path:
    return root / "Content" / "Test" / "Fixtures" / ROUTE_DIRECTORIES[route] / "RouteFixture.source.json"


def _validate_fixture(fixture, route: str, source_path: Path):
    top_keys = {
        "schemaVersion",
        "fixtureId",
        "route",
        "commitBoundary",
        "brokerTag",
        "relationship",
        "sourceAuthorities",
        "loyaltyAction",
        "daxtonOutcome",
        "expectedAftermath",
        "generationContract",
    }
    _require_exact_keys(fixture, top_keys, f"{route} fixture")
    if fixture["schemaVersion"] != 1 or isinstance(fixture["schemaVersion"], bool):
        raise ValueError(f"{route} fixture schemaVersion must be integer 1")
    expected_id = f"fixture.release.{route.lower()}"
    if fixture["fixtureId"] != expected_id or fixture["route"] != route:
        raise ValueError(f"{route} fixture identity does not match {source_path}")
    if fixture["commitBoundary"] != "IMMEDIATELY_BEFORE_FINAL_ROUTE_COMMIT":
        raise ValueError(f"{route} fixture is not at the pre-final-commit boundary")
    if fixture["brokerTag"] != BROKER_TAGS[route]:
        raise ValueError(f"{route} fixture has the wrong canonical broker tag")

    relationship = fixture["relationship"]
    _require_exact_keys(relationship, {"trust", "respect", "grievance"}, f"{route} relationship")
    for key, value in relationship.items():
        _require_number(value, f"{route} relationship.{key}")

    authorities = fixture["sourceAuthorities"]
    if not isinstance(authorities, list) or any(not isinstance(row, str) for row in authorities):
        raise ValueError(f"{route} sourceAuthorities must be a string array")
    if len(authorities) != len(set(authorities)) or set(authorities) != REQUIRED_AUTHORITIES[route]:
        raise ValueError(f"{route} fixture must contain exactly its minimum canonical source authorities")

    loyalty_action = fixture["loyaltyAction"]
    _require_exact_keys(
        loyalty_action,
        {"system", "outcome", "targetDefinition", "recipient", "giftLoyaltyReward"},
        f"{route} Loyalty action",
    )
    if loyalty_action != {
        "system": "UDACaptureComponent",
        "outcome": "Gift",
        "targetDefinition": "forgeweave.infinite_foundry",
        "recipient": "civilization.ironheart_workers",
        "giftLoyaltyReward": LOYALTY_REWARDS[route],
    }:
        raise ValueError(f"{route} Loyalty must come from its exact production capture Gift action")

    outcome = fixture["daxtonOutcome"]
    _require_exact_keys(
        outcome,
        {"leaderState", "saveGrandForge", "evacuateWorkers", "offerProductionUnion"},
        f"{route} Daxton outcome",
    )
    expected_choices = CHOICES[route]
    if outcome["leaderState"] != LEADER_STATES[route] or tuple(
        outcome[key] for key in ("saveGrandForge", "evacuateWorkers", "offerProductionUnion")
    ) != expected_choices or any(
        not isinstance(outcome[key], bool)
        for key in ("saveGrandForge", "evacuateWorkers", "offerProductionUnion")
    ):
        raise ValueError(f"{route} fixture has a noncanonical route-aware Daxton outcome")

    aftermath = fixture["expectedAftermath"]
    _require_exact_keys(
        aftermath,
        {"loyalty", "daxtonState", "historyTags", "damageFingerprint", "relationship"},
        f"{route} aftermath contract",
    )
    if any(value != "PAIRWISE_DISTINCT" for value in aftermath.values()):
        raise ValueError(f"{route} aftermath must require pairwise-distinct observations")

    generation = fixture["generationContract"]
    _require_exact_keys(generation, {"tool", "automationTest", "binaryAssetPolicy"}, f"{route} generation")
    expected_generation = {
        "tool": "Build/Tools/ReleaseCandidateTool.py",
        "automationTest": "Dominion.Release.VerticalSlice",
        "binaryAssetPolicy": "SOURCE_ONLY_NO_FABRICATED_UASSET",
    }
    if generation != expected_generation:
        raise ValueError(f"{route} generation/validation contract drifted")
    return fixture


def load_and_validate_fixtures(root: Path):
    root = Path(root).resolve()
    fixtures = []
    for route in ROUTES:
        source_path = _fixture_source_path(root, route)
        fixtures.append(_validate_fixture(_load_json(source_path), route, source_path))
    fabricated = list((root / "Content" / "Test" / "Fixtures").rglob("*.uasset"))
    if fabricated:
        raise ValueError(f"source fixtures must not contain fabricated .uasset files: {fabricated}")
    return fixtures


def generate_fixture_plan(root: Path, fixtures=None):
    root = Path(root).resolve()
    fixtures = fixtures if fixtures is not None else load_and_validate_fixtures(root)
    rows = []
    for route, fixture in zip(ROUTES, fixtures):
        source_path = _fixture_source_path(root, route)
        rows.append(
            {
                "route": route,
                "fixtureId": fixture["fixtureId"],
                "source": source_path.relative_to(root).as_posix(),
                "sourceSha256": _sha256_file(source_path),
                "brokerTag": fixture["brokerTag"],
                "relationship": fixture["relationship"],
                "loyaltyAction": fixture["loyaltyAction"],
                "daxtonOutcome": fixture["daxtonOutcome"],
            }
        )
    schema_path = root / "Content" / "Test" / "Fixtures" / "VerticalSliceRouteFixture.schema.json"
    _load_json(schema_path)
    return {
        "schemaVersion": 1,
        "fixtureSchema": schema_path.relative_to(root).as_posix(),
        "fixtureSchemaSha256": _sha256_file(schema_path),
        "automationTest": "Dominion.Release.VerticalSlice",
        "fixtures": rows,
    }


def _task27_summary(root: Path):
    path = root / "Build" / "Performance" / "Task27PerformanceEvidence.json"
    payload = _load_json(path)
    state = payload.get("state")
    if state not in {"NOT_READY", "CAPTURED"}:
        raise ValueError("Task 27 evidence state is neither NOT_READY nor CAPTURED")
    return path, payload, _sha256_file(path)


def _empty_claims():
    return {key: False for key in CLAIM_KEYS}


def _not_ready_evidence(
    root: Path,
    command=None,
    exit_code=None,
    log=None,
    reasons=None,
    run_id=None,
    build_id=None,
    fixture_plan=None,
    automation=None,
    routes=None,
    checkpoints=None,
):
    task27_path, task27, task27_sha = _task27_summary(root)
    claims = _empty_claims()
    task27_claims = task27.get("claims", {})
    claims["task27Passed"] = task27["state"] == "CAPTURED" and all(
        task27_claims.get(key) is True
        for key in ("performanceTargetsPassed", "soakPassed", "cookPassed")
    )
    if automation is not None:
        claims["automationPassed"] = True
        claims["fourRoutesPassed"] = True
        claims["saveRegressionPassed"] = True
    return {
        "schemaVersion": 1,
        "state": "NOT_READY",
        "runId": run_id,
        "buildId": build_id,
        "fixturePlanEvidence": fixture_plan,
        "task27PerformanceState": task27["state"],
        "task27Evidence": task27_path.relative_to(root).as_posix(),
        "task27EvidenceSha256": task27_sha,
        "commands": {
            "releaseAutomation": {
                "command": command,
                "exitCode": exit_code,
                "log": log,
            }
        },
        "automationEvidence": automation,
        "routeEvidence": routes,
        "saveCheckpointEvidence": checkpoints,
        "controllerEvidence": None,
        "onboardingEvidence": None,
        "defectEvidence": None,
        "claims": claims,
        "notReadyReasons": reasons or [
            "UE5.8 release Automation has not produced reconciled route/save evidence.",
            "Task 27 performance/soak/cook evidence is NOT_READY.",
            "Reference-hardware capture, route videos, controller observation, and uncoached onboarding are absent.",
            "A current triaged defect report has not been reconciled.",
        ],
    }


def load_and_validate_release_evidence(root: Path, path: Path):
    root = Path(root).resolve()
    evidence = _load_json(Path(path))
    keys = {
        "schemaVersion",
        "state",
        "runId",
        "buildId",
        "fixturePlanEvidence",
        "task27PerformanceState",
        "task27Evidence",
        "task27EvidenceSha256",
        "commands",
        "automationEvidence",
        "routeEvidence",
        "saveCheckpointEvidence",
        "controllerEvidence",
        "onboardingEvidence",
        "defectEvidence",
        "claims",
        "notReadyReasons",
    }
    _require_exact_keys(evidence, keys, "release evidence")
    if evidence["schemaVersion"] != 1 or evidence["state"] not in {"NOT_READY", "READY"}:
        raise ValueError("release evidence identity is invalid")
    if (evidence["runId"] is None) != (evidence["buildId"] is None):
        raise ValueError("release evidence runId/buildId must both be null or both be present")
    if evidence["runId"] is not None:
        _require_identifier(evidence["runId"], "release evidence runId")
        _require_identifier(evidence["buildId"], "release evidence buildId")
    if evidence["fixturePlanEvidence"] is not None:
        _validate_artifact_ref(evidence["fixturePlanEvidence"], "fixture plan evidence")
    _require_sha256(evidence["task27EvidenceSha256"], "task27EvidenceSha256")
    task27_path, task27, task27_sha = _task27_summary(root)
    if evidence["task27Evidence"] != task27_path.relative_to(root).as_posix():
        raise ValueError("release evidence points at a noncanonical Task 27 source")
    if evidence["task27PerformanceState"] != task27["state"] or evidence["task27EvidenceSha256"] != task27_sha:
        raise ValueError("release evidence does not reconcile exact Task 27 bytes/state")
    _require_exact_keys(evidence["commands"], {"releaseAutomation"}, "release commands")
    command = evidence["commands"]["releaseAutomation"]
    _require_exact_keys(command, {"command", "exitCode", "log"}, "release Automation command")
    if command["command"] is not None and (
        not isinstance(command["command"], list) or any(not isinstance(row, str) for row in command["command"])
    ):
        raise ValueError("release Automation command must be null or a string array")
    if command["exitCode"] is not None and (
        isinstance(command["exitCode"], bool) or not isinstance(command["exitCode"], int)
    ):
        raise ValueError("release Automation exitCode must be null or an integer")
    if command["log"] is not None:
        _validate_artifact_ref(command["log"], "release Automation log")
        _require_artifact(root, command["log"]["path"], command["log"]["sha256"])
    _require_exact_keys(evidence["claims"], CLAIM_KEYS, "release claims")
    if any(not isinstance(value, bool) for value in evidence["claims"].values()):
        raise ValueError("release claims must be booleans")
    if not isinstance(evidence["notReadyReasons"], list) or any(
        not isinstance(row, str) or not row for row in evidence["notReadyReasons"]
    ):
        raise ValueError("notReadyReasons must be a string array")
    if evidence["state"] == "NOT_READY":
        if not evidence["notReadyReasons"]:
            raise ValueError("NOT_READY release evidence must explain why the gate is closed")
        if any(evidence[key] is not None for key in ("controllerEvidence", "onboardingEvidence", "defectEvidence")):
            raise ValueError("NOT_READY checked evidence cannot imply unverified human/defect observations")
        expected_claims = _empty_claims()
        task27_claims = task27.get("claims", {})
        expected_claims["task27Passed"] = task27["state"] == "CAPTURED" and all(
            task27_claims.get(key) is True
            for key in ("performanceTargetsPassed", "soakPassed", "cookPassed")
        )
        if evidence["fixturePlanEvidence"] is not None:
            plan_ref = evidence["fixturePlanEvidence"]
            _require_artifact(root, plan_ref["path"], plan_ref["sha256"])
            _require_authoritative_plan(root, root / plan_ref["path"], plan_ref["sha256"])
        if evidence["automationEvidence"] is None:
            if evidence["routeEvidence"] is not None or evidence["saveCheckpointEvidence"] is not None:
                raise ValueError("partial route/save evidence requires a reconciled Automation artifact")
        else:
            if evidence["runId"] is None or evidence["fixturePlanEvidence"] is None:
                raise ValueError("partial Automation evidence requires run/build and fixture plan identity")
            automation_ref = evidence["automationEvidence"]
            _validate_artifact_ref(automation_ref, "partial release Automation evidence", exit_code=True)
            _require_artifact(root, automation_ref["path"], automation_ref["sha256"])
            automation_payload = validate_automation_evidence(
                root, _load_json(root / automation_ref["path"]), root / evidence["fixturePlanEvidence"]["path"]
            )
            if (
                automation_payload["runId"] != evidence["runId"]
                or automation_payload["buildId"] != evidence["buildId"]
                or evidence["routeEvidence"] != automation_payload["routes"]
                or evidence["saveCheckpointEvidence"] != automation_payload["saveCheckpoints"]
            ):
                raise ValueError("partial Automation route/save evidence does not reconcile run/build bytes")
            expected_claims["automationPassed"] = True
            expected_claims["fourRoutesPassed"] = True
            expected_claims["saveRegressionPassed"] = True
        if evidence["claims"] != expected_claims:
            raise ValueError("NOT_READY claims must exactly reflect only verified partial evidence")
    else:
        task27_claims = task27.get("claims", {})
        if evidence["task27PerformanceState"] != "CAPTURED" or not all(
            task27_claims.get(key) is True
            for key in ("performanceTargetsPassed", "soakPassed", "cookPassed")
        ):
            raise ValueError("READY evidence requires exact CAPTURED Task 27 evidence and claims")
        if command["exitCode"] != 0:
            raise ValueError("READY evidence requires a successful release Automation command")
        if evidence["runId"] is None or evidence["fixturePlanEvidence"] is None:
            raise ValueError("READY evidence requires run/build and fixture plan identity")
        if not all(evidence["claims"].values()) or evidence["notReadyReasons"]:
            raise ValueError("READY evidence requires every claim and no not-ready reasons")
        reconcile_release_candidate(
            root,
            {
                "schemaVersion": 1,
                "runId": evidence["runId"],
                "buildId": evidence["buildId"],
                "fixturePlanSha256": evidence["fixturePlanEvidence"]["sha256"],
                "task27Evidence": {
                    "path": evidence["task27Evidence"],
                    "sha256": evidence["task27EvidenceSha256"],
                    "state": "CAPTURED",
                },
                "automationEvidence": evidence["automationEvidence"],
                "routes": evidence["routeEvidence"],
                "saveCheckpoints": evidence["saveCheckpointEvidence"],
                "controller": evidence["controllerEvidence"],
                "onboarding": evidence["onboardingEvidence"],
                "defects": evidence["defectEvidence"],
            },
            root / evidence["fixturePlanEvidence"]["path"],
        )
    return evidence


def _validate_artifact_ref(record, label, exit_code=False):
    expected = {"path", "sha256"} | ({"exitCode"} if exit_code else set())
    _require_exact_keys(record, expected, label)
    _require_nonempty_path(record["path"], f"{label}.path")
    _require_sha256(record["sha256"], f"{label}.sha256")
    if exit_code and (isinstance(record["exitCode"], bool) or record["exitCode"] != 0):
        raise ValueError(f"{label}.exitCode must be integer zero")


def validate_release_candidate(candidate):
    top_keys = {
        "schemaVersion",
        "runId",
        "buildId",
        "fixturePlanSha256",
        "task27Evidence",
        "automationEvidence",
        "routes",
        "saveCheckpoints",
        "controller",
        "onboarding",
        "defects",
    }
    _require_exact_keys(candidate, top_keys, "release candidate")
    if candidate["schemaVersion"] != 1 or isinstance(candidate["schemaVersion"], bool):
        raise ValueError("release candidate schemaVersion must be integer 1")
    _require_identifier(candidate["runId"], "release candidate runId")
    _require_identifier(candidate["buildId"], "release candidate buildId")
    _require_sha256(candidate["fixturePlanSha256"], "fixturePlanSha256")
    # Task 27 state is intentionally part of this closed object, unlike generic artifacts.
    _require_exact_keys(candidate["task27Evidence"], {"path", "sha256", "state"}, "Task 27 evidence")
    _require_nonempty_path(candidate["task27Evidence"]["path"], "Task 27 evidence.path")
    _require_sha256(candidate["task27Evidence"]["sha256"], "Task 27 evidence.sha256")
    if candidate["task27Evidence"]["state"] != "CAPTURED":
        raise ValueError("Task 27 evidence must be CAPTURED for release")
    _validate_artifact_ref(candidate["automationEvidence"], "release Automation evidence", exit_code=True)

    routes = candidate["routes"]
    if not isinstance(routes, list) or [row.get("route") for row in routes if isinstance(row, dict)] != list(ROUTES):
        raise ValueError("release candidate must contain the exact four ordered routes")
    aftermath_fields = (
        "loyaltyBefore",
        "loyalty",
        "loyaltyActionSha256",
        "daxtonState",
        "historySha256",
        "damageSha256",
        "relationshipSha256",
    )
    distinct_fields = (
        "loyalty",
        "loyaltyActionSha256",
        "daxtonState",
        "historySha256",
        "damageSha256",
        "relationshipSha256",
    )
    observed = {field: [] for field in distinct_fields}
    for route, row in zip(ROUTES, routes):
        _require_exact_keys(
            row,
            {"route", "sourceSha256", "reachedFirstAscension", "log", "video", "aftermath"},
            f"{route} route",
        )
        _require_sha256(row["sourceSha256"], f"{route} sourceSha256")
        if row["reachedFirstAscension"] is not True:
            raise ValueError(f"{route} did not reach first Ascension")
        _validate_artifact_ref(row["log"], f"{route} log")
        _validate_artifact_ref(row["video"], f"{route} video")
        aftermath = row["aftermath"]
        _require_exact_keys(aftermath, aftermath_fields, f"{route} aftermath")
        _require_number(aftermath["loyaltyBefore"], f"{route} aftermath loyaltyBefore")
        _require_number(aftermath["loyalty"], f"{route} aftermath loyalty")
        if aftermath["loyalty"] <= aftermath["loyaltyBefore"]:
            raise ValueError(f"{route} Loyalty must increase through a causal production action")
        _require_sha256(aftermath["loyaltyActionSha256"], f"{route} Loyalty action")
        if aftermath["daxtonState"] not in LEADER_STATES.values():
            raise ValueError(f"{route} aftermath has an unsupported Daxton state")
        for field in ("historySha256", "damageSha256", "relationshipSha256"):
            _require_sha256(aftermath[field], f"{route} aftermath {field}")
        for field in distinct_fields:
            observed[field].append(aftermath[field])
    if any(len(set(values)) != len(ROUTES) for values in observed.values()):
        raise ValueError("route aftermath must be pairwise distinct for Loyalty, Daxton, history, damage, and relationship")
    if len({row["log"]["path"] for row in routes}) != len(ROUTES) or len(
        {row["video"]["path"] for row in routes}
    ) != len(ROUTES):
        raise ValueError("four-route evidence requires distinct log and video artifacts")

    checkpoints = candidate["saveCheckpoints"]
    if not isinstance(checkpoints, list) or [row.get("checkpoint") for row in checkpoints if isinstance(row, dict)] != list(CHECKPOINTS):
        raise ValueError("save checkpoint evidence must contain the exact six ordered checkpoints")
    for row in checkpoints:
        _require_exact_keys(
            row,
            {"checkpoint", "passed", "snapshotSha256", "log"},
            f"{row.get('checkpoint')} save checkpoint",
        )
        if row["passed"] is not True:
            raise ValueError(f"save checkpoint {row['checkpoint']} did not pass")
        _require_sha256(row["snapshotSha256"], f"{row['checkpoint']} snapshotSha256")
        _validate_artifact_ref(row["log"], f"{row['checkpoint']} save checkpoint log")

    controller = candidate["controller"]
    _require_exact_keys(
        controller,
        {"observedByHuman", "mouseUsed", "actions", "checklist", "video"},
        "controller evidence",
    )
    _require_exact_keys(controller["actions"], CONTROLLER_ACTIONS, "controller actions")
    if (
        controller["observedByHuman"] is not True
        or controller["mouseUsed"] is not False
        or any(value is not True for value in controller["actions"].values())
    ):
        raise ValueError("controller evidence requires a human-observed, no-mouse complete critical path")
    _validate_artifact_ref(controller["checklist"], "controller checklist")
    _validate_artifact_ref(controller["video"], "controller video")

    onboarding = candidate["onboarding"]
    _require_exact_keys(
        onboarding,
        {"uncoachedFirstTimeTesters", "developerCoaching", "actions", "observationLog"},
        "onboarding evidence",
    )
    _require_exact_keys(onboarding["actions"], ONBOARDING_ACTIONS, "onboarding actions")
    count = onboarding["uncoachedFirstTimeTesters"]
    if (
        isinstance(count, bool)
        or not isinstance(count, int)
        or count <= 0
        or onboarding["developerCoaching"] is not False
        or any(value is not True for value in onboarding["actions"].values())
    ):
        raise ValueError("onboarding evidence requires uncoached first-time testers completing every first-hour action")
    _validate_artifact_ref(onboarding["observationLog"], "onboarding observation log")

    defects = candidate["defects"]
    defect_keys = {
        "Blocker", "Critical", "High", "Medium", "Low", "acceptedMedium", "lowWithinPolishTolerance", "report"
    }
    _require_exact_keys(defects, defect_keys, "defect evidence")
    for severity in ("Blocker", "Critical", "High", "Medium", "Low"):
        value = defects[severity]
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            raise ValueError("defect gate counts must be non-negative integers")
    if any(defects[severity] != 0 for severity in ("Blocker", "Critical", "High")):
        raise ValueError("defect gate requires zero Blocker, Critical, and High defects")
    accepted_medium = defects["acceptedMedium"]
    if not isinstance(accepted_medium, list) or len(accepted_medium) != defects["Medium"] or len(
        set(accepted_medium)
    ) != len(accepted_medium) or any(
        not isinstance(row, str) or not row for row in accepted_medium
    ):
        raise ValueError("accepted Medium evidence must explicitly list every Medium defect")
    if defects["lowWithinPolishTolerance"] is not True:
        raise ValueError("Low defects must be within explicit polish tolerance")
    _validate_artifact_ref(defects["report"], "defect report")
    return candidate


def validate_automation_evidence(root: Path, automation, plan_path: Path):
    _require_exact_keys(
        automation,
        {"schemaVersion", "runId", "buildId", "fixturePlanSha256", "routes", "saveCheckpoints"},
        "release Automation evidence",
    )
    if automation["schemaVersion"] != 1 or isinstance(automation["schemaVersion"], bool):
        raise ValueError("release Automation schemaVersion must be integer 1")
    _require_identifier(automation["runId"], "release Automation runId")
    _require_identifier(automation["buildId"], "release Automation buildId")
    _require_sha256(automation["fixturePlanSha256"], "release Automation fixturePlanSha256")
    if automation["fixturePlanSha256"] != _sha256_file(plan_path):
        raise ValueError("release Automation fixture plan digest does not reconcile")
    plan = _load_json(plan_path)
    expected_sources = {row["route"]: row["sourceSha256"] for row in plan.get("fixtures", [])}
    expected_loyalty_rewards = {
        row["route"]: row.get("loyaltyAction", {}).get("giftLoyaltyReward")
        for row in plan.get("fixtures", [])
    }
    routes = automation["routes"]
    if not isinstance(routes, list) or [row.get("route") for row in routes if isinstance(row, dict)] != list(ROUTES):
        raise ValueError("release Automation must contain the exact four ordered routes")
    aftermath_fields = (
        "loyaltyBefore",
        "loyalty",
        "loyaltyActionSha256",
        "daxtonState",
        "historySha256",
        "damageSha256",
        "relationshipSha256",
    )
    distinct_fields = aftermath_fields[1:]
    observed = {field: [] for field in distinct_fields}
    for route, row in zip(ROUTES, routes):
        _require_exact_keys(
            row,
            {"route", "sourceSha256", "reachedFirstAscension", "aftermath"},
            f"{route} Automation route",
        )
        if row["sourceSha256"] != expected_sources.get(route) or row["reachedFirstAscension"] is not True:
            raise ValueError(f"{route} Automation route did not reconcile source/Ascension")
        aftermath = row["aftermath"]
        _require_exact_keys(aftermath, aftermath_fields, f"{route} Automation aftermath")
        _require_number(aftermath["loyaltyBefore"], f"{route} Automation loyaltyBefore")
        _require_number(aftermath["loyalty"], f"{route} Automation loyalty")
        if aftermath["loyalty"] <= aftermath["loyaltyBefore"]:
            raise ValueError(f"{route} Automation Loyalty has no production-action increase")
        if not math.isclose(
            aftermath["loyalty"] - aftermath["loyaltyBefore"],
            expected_loyalty_rewards.get(route, -1),
            rel_tol=0.0,
            abs_tol=0.001,
        ):
            raise ValueError(f"{route} Automation Loyalty does not equal its capture Gift reward")
        _require_sha256(aftermath["loyaltyActionSha256"], f"{route} Automation Loyalty action")
        if aftermath["daxtonState"] != LEADER_STATES[route]:
            raise ValueError(f"{route} Automation Daxton outcome is wrong")
        for field in ("historySha256", "damageSha256", "relationshipSha256"):
            _require_sha256(aftermath[field], f"{route} Automation {field}")
        for field in distinct_fields:
            observed[field].append(aftermath[field])
    if any(len(set(values)) != len(ROUTES) for values in observed.values()):
        raise ValueError("release Automation route aftermath is not pairwise distinct")
    checkpoints = automation["saveCheckpoints"]
    if not isinstance(checkpoints, list) or [
        row.get("checkpoint") for row in checkpoints if isinstance(row, dict)
    ] != list(CHECKPOINTS):
        raise ValueError("release Automation did not report the exact six save checkpoints")
    for checkpoint, row in zip(CHECKPOINTS, checkpoints):
        _require_exact_keys(
            row, {"checkpoint", "passed", "snapshotSha256"}, f"{checkpoint} Automation checkpoint"
        )
        if row["passed"] is not True:
            raise ValueError(f"{checkpoint} Automation checkpoint did not pass")
        _require_sha256(row["snapshotSha256"], f"{checkpoint} Automation snapshotSha256")
    return automation


def _require_artifact(root: Path, relative: str, expected_sha=None):
    path = root / relative
    if not path.is_file():
        raise ValueError(f"release artifact is missing: {relative}")
    if path.stat().st_size <= 0:
        raise ValueError(f"release artifact is empty: {relative}")
    actual = _sha256_file(path)
    if expected_sha is not None and actual != expected_sha:
        raise ValueError(f"release artifact digest mismatch: {relative}")
    return actual


def _require_authoritative_plan(root: Path, plan_path: Path, expected_sha=None):
    root = Path(root).resolve()
    plan_path = Path(plan_path).resolve()
    try:
        plan_path.relative_to(root)
    except ValueError as error:
        raise ValueError("fixture plan must be inside the repository root") from error
    try:
        authoritative_bytes = _canonical_bytes(generate_fixture_plan(root))
    except ValueError as error:
        raise ValueError(f"cannot generate authoritative current fixture plan: {error}") from error
    try:
        supplied_bytes = plan_path.read_bytes()
    except OSError as error:
        raise ValueError(f"cannot read fixture plan: {error}") from error
    if supplied_bytes != authoritative_bytes:
        raise ValueError("supplied plan does not equal the authoritative current fixture plan")
    actual_sha = _sha256_bytes(authoritative_bytes)
    if expected_sha is not None and expected_sha != actual_sha:
        raise ValueError("fixture plan digest does not match the authoritative current fixture plan")
    return actual_sha


def _require_timestamp(value, label):
    if not isinstance(value, str) or not value.endswith("Z"):
        raise ValueError(f"{label} must be a non-empty UTC timestamp")
    try:
        parsed = datetime.datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as error:
        raise ValueError(f"{label} must be a valid UTC timestamp") from error
    if parsed.tzinfo is None or parsed.utcoffset() != datetime.timedelta(0):
        raise ValueError(f"{label} must be a valid UTC timestamp")


def _validate_controller_attestation(candidate, attestation):
    controller = candidate["controller"]
    _require_exact_keys(
        attestation,
        {
            "schemaVersion", "kind", "runId", "buildId", "attestedBy", "recordedAtUtc",
            "observedByHuman", "mouseUsed", "actions", "video",
        },
        "controller attestation",
    )
    if attestation["schemaVersion"] != 1 or attestation["kind"] != "controller":
        raise ValueError("controller attestation identity is invalid")
    if (
        attestation["runId"] != candidate["runId"]
        or attestation["buildId"] != candidate["buildId"]
    ):
        raise ValueError("controller attestation run/build does not match the candidate")
    _require_identifier(attestation["attestedBy"], "controller attestedBy")
    _require_timestamp(attestation["recordedAtUtc"], "controller recordedAtUtc")
    _require_exact_keys(attestation["actions"], CONTROLLER_ACTIONS, "controller attestation actions")
    if (
        attestation["observedByHuman"] is not True
        or attestation["mouseUsed"] is not False
        or any(value is not True for value in attestation["actions"].values())
        or attestation["actions"] != controller["actions"]
        or attestation["video"] != controller["video"]
    ):
        raise ValueError("controller attestation must literally confirm the candidate observation")


def _validate_onboarding_attestation(candidate, attestation):
    onboarding = candidate["onboarding"]
    _require_exact_keys(
        attestation,
        {
            "schemaVersion", "kind", "runId", "buildId", "attestedBy", "recordedAtUtc",
            "testers",
        },
        "onboarding attestation",
    )
    if attestation["schemaVersion"] != 1 or attestation["kind"] != "onboarding":
        raise ValueError("onboarding attestation identity is invalid")
    if (
        attestation["runId"] != candidate["runId"]
        or attestation["buildId"] != candidate["buildId"]
    ):
        raise ValueError("onboarding attestation run/build does not match the candidate")
    _require_identifier(attestation["attestedBy"], "onboarding attestedBy")
    _require_timestamp(attestation["recordedAtUtc"], "onboarding recordedAtUtc")
    testers = attestation["testers"]
    if not isinstance(testers, list) or len(testers) != onboarding["uncoachedFirstTimeTesters"]:
        raise ValueError("onboarding attestation tester count does not match the candidate")
    tester_ids = set()
    for tester in testers:
        _require_exact_keys(
            tester, {"testerId", "developerCoaching", "actions"}, "onboarding tester"
        )
        _require_identifier(tester["testerId"], "onboarding testerId")
        if tester["testerId"] in tester_ids:
            raise ValueError("onboarding attestation tester IDs must be unique")
        tester_ids.add(tester["testerId"])
        _require_exact_keys(tester["actions"], ONBOARDING_ACTIONS, "onboarding tester actions")
        if (
            tester["developerCoaching"] is not False
            or any(value is not True for value in tester["actions"].values())
            or tester["actions"] != onboarding["actions"]
        ):
            raise ValueError("onboarding attestation requires literal uncoached action passes")


def _validate_defect_attestation(candidate, attestation):
    defects = candidate["defects"]
    severities = ("Blocker", "Critical", "High", "Medium", "Low")
    _require_exact_keys(
        attestation,
        {
            "schemaVersion", "kind", "runId", "buildId", "attestedBy", "recordedAtUtc",
            "counts", "acceptedMedium", "lowWithinPolishTolerance", "issues",
        },
        "defect attestation",
    )
    if attestation["schemaVersion"] != 1 or attestation["kind"] != "defects":
        raise ValueError("defect attestation identity is invalid")
    if (
        attestation["runId"] != candidate["runId"]
        or attestation["buildId"] != candidate["buildId"]
    ):
        raise ValueError("defect attestation run/build does not match the candidate")
    _require_identifier(attestation["attestedBy"], "defect attestedBy")
    _require_timestamp(attestation["recordedAtUtc"], "defect recordedAtUtc")
    _require_exact_keys(attestation["counts"], severities, "defect attestation counts")
    if attestation["counts"] != {severity: defects[severity] for severity in severities}:
        raise ValueError("defect attestation counts do not match the candidate")
    if (
        attestation["acceptedMedium"] != defects["acceptedMedium"]
        or attestation["lowWithinPolishTolerance"] is not True
    ):
        raise ValueError("defect attestation disposition does not match the candidate")
    issues = attestation["issues"]
    if not isinstance(issues, list) or len(issues) != sum(defects[severity] for severity in severities):
        raise ValueError("defect attestation issue rows do not reconcile its counts")
    issue_ids = set()
    counted = {severity: 0 for severity in severities}
    medium_ids = []
    for issue in issues:
        _require_exact_keys(issue, {"id", "severity", "disposition"}, "defect issue")
        _require_identifier(issue["id"], "defect issue id")
        if issue["id"] in issue_ids or issue["severity"] not in counted:
            raise ValueError("defect attestation issues require unique IDs and known severities")
        issue_ids.add(issue["id"])
        counted[issue["severity"]] += 1
        if issue["severity"] == "Medium":
            if issue["disposition"] != "accepted":
                raise ValueError("every Medium defect must be explicitly accepted")
            medium_ids.append(issue["id"])
        elif issue["severity"] == "Low":
            if issue["disposition"] != "polish_tolerance":
                raise ValueError("every Low defect must have polish-tolerance disposition")
        elif issue["disposition"] != "open":
            raise ValueError("high-severity defect disposition is invalid")
    if counted != attestation["counts"] or medium_ids != defects["acceptedMedium"]:
        raise ValueError("defect attestation issue rows do not match counts/accepted Medium IDs")


def _validate_structured_attestations(root: Path, candidate):
    _validate_controller_attestation(
        candidate, _load_json(root / candidate["controller"]["checklist"]["path"])
    )
    _validate_onboarding_attestation(
        candidate, _load_json(root / candidate["onboarding"]["observationLog"]["path"])
    )
    _validate_defect_attestation(
        candidate, _load_json(root / candidate["defects"]["report"]["path"])
    )


def reconcile_release_candidate(root: Path, candidate, plan_path: Path):
    root = Path(root).resolve()
    authoritative_plan_sha = _require_authoritative_plan(
        root, plan_path, candidate.get("fixturePlanSha256") if isinstance(candidate, dict) else None
    )
    validate_release_candidate(candidate)
    if candidate["fixturePlanSha256"] != authoritative_plan_sha:
        raise ValueError("fixture plan digest does not match the authoritative current fixture plan")
    expected_task27 = "Build/Performance/Task27PerformanceEvidence.json"
    task27_ref = candidate["task27Evidence"]
    if task27_ref["path"] != expected_task27:
        raise ValueError("Task 27 evidence path is not canonical")
    _require_artifact(root, task27_ref["path"], task27_ref["sha256"])
    task27 = _load_json(root / task27_ref["path"])
    claims = task27.get("claims", {})
    if task27.get("state") != "CAPTURED" or not all(
        claims.get(key) is True for key in ("performanceTargetsPassed", "soakPassed", "cookPassed")
    ):
        raise ValueError("Task 27 remains NOT_READY or has incomplete measured claims")
    automation = candidate["automationEvidence"]
    _require_artifact(root, automation["path"], automation["sha256"])
    automation_payload = validate_automation_evidence(
        root, _load_json(root / automation["path"]), plan_path
    )
    if (
        automation_payload["runId"] != candidate["runId"]
        or automation_payload["buildId"] != candidate["buildId"]
    ):
        raise ValueError("release Automation run/build does not match the candidate")
    for candidate_route, automated_route in zip(candidate["routes"], automation_payload["routes"]):
        comparable = {
            key: candidate_route[key]
            for key in ("route", "sourceSha256", "reachedFirstAscension", "aftermath")
        }
        if comparable != automated_route:
            raise ValueError(f"{candidate_route['route']} candidate route differs from Automation evidence")
    automated_checkpoints = automation_payload["saveCheckpoints"]
    for candidate_checkpoint, automated_checkpoint in zip(
        candidate["saveCheckpoints"], automated_checkpoints
    ):
        comparable = {
            key: candidate_checkpoint[key]
            for key in ("checkpoint", "passed", "snapshotSha256")
        }
        if comparable != automated_checkpoint:
            raise ValueError(
                f"{candidate_checkpoint['checkpoint']} differs from Automation checkpoint evidence"
            )
    for route in candidate["routes"]:
        _require_artifact(root, route["log"]["path"], route["log"]["sha256"])
        _require_artifact(root, route["video"]["path"], route["video"]["sha256"])
    for checkpoint in candidate["saveCheckpoints"]:
        _require_artifact(root, checkpoint["log"]["path"], checkpoint["log"]["sha256"])
    _require_artifact(root, candidate["controller"]["checklist"]["path"],
        candidate["controller"]["checklist"]["sha256"])
    _require_artifact(root, candidate["controller"]["video"]["path"],
        candidate["controller"]["video"]["sha256"])
    _require_artifact(root, candidate["onboarding"]["observationLog"]["path"],
        candidate["onboarding"]["observationLog"]["sha256"])
    _require_artifact(root, candidate["defects"]["report"]["path"],
        candidate["defects"]["report"]["sha256"])
    _validate_structured_attestations(root, candidate)
    return candidate


def _ready_evidence(root: Path, candidate, plan_path: Path, command=None, log=None):
    _, task27, task27_sha = _task27_summary(root)
    claims = {key: True for key in CLAIM_KEYS}
    return {
        "schemaVersion": 1,
        "state": "READY",
        "runId": candidate["runId"],
        "buildId": candidate["buildId"],
        "fixturePlanEvidence": {
            "path": Path(plan_path).resolve().relative_to(Path(root).resolve()).as_posix(),
            "sha256": candidate["fixturePlanSha256"],
        },
        "task27PerformanceState": task27["state"],
        "task27Evidence": "Build/Performance/Task27PerformanceEvidence.json",
        "task27EvidenceSha256": task27_sha,
        "commands": {"releaseAutomation": {"command": command, "exitCode": 0, "log": log}},
        "automationEvidence": candidate["automationEvidence"],
        "routeEvidence": candidate["routes"],
        "saveCheckpointEvidence": candidate["saveCheckpoints"],
        "controllerEvidence": candidate["controller"],
        "onboardingEvidence": candidate["onboarding"],
        "defectEvidence": candidate["defects"],
        "claims": claims,
        "notReadyReasons": [],
    }


def _write_json(path: Path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(_canonical_bytes(value))


def command_plan(args):
    root = Path(args.root).resolve()
    plan = generate_fixture_plan(root)
    output = Path(args.output)
    if not output.is_absolute():
        output = root / output
    _write_json(output, plan)
    print(f"fixtures={len(plan['fixtures'])}")
    print(f"planSha256={_sha256_file(output)}")
    return 0


def command_validate(args):
    root = Path(args.root).resolve()
    fixtures = load_and_validate_fixtures(root)
    evidence = load_and_validate_release_evidence(root, root / "Build" / "Release" / "Task28ReleaseEvidence.json")
    print(f"fixtures={len(fixtures)}")
    print(f"state={evidence['state']}")
    print(f"task27={evidence['task27PerformanceState']}")
    return 0


def command_run(args):
    root = Path(args.root).resolve()
    _require_identifier(args.run_id, "run --run-id")
    _require_identifier(args.build_id, "run --build-id")
    plan_path = root / "Intermediate" / "Release" / "VerticalSliceRouteFixture.plan.json"
    automation_evidence_path = root / "Intermediate" / "Release" / "Task28ReleaseAutomationEvidence.json"
    log_path = root / "Intermediate" / "Release" / "Task28ReleaseAutomation.log"
    evidence_path = Path(args.evidence)
    if not evidence_path.is_absolute():
        evidence_path = root / evidence_path
    _write_json(plan_path, generate_fixture_plan(root))
    plan_ref = {
        "path": plan_path.relative_to(root).as_posix(),
        "sha256": _sha256_file(plan_path),
    }
    # A passing command must produce evidence for this invocation. Never let a
    # prior Automation sidecar satisfy the current release run.
    automation_evidence_path.unlink(missing_ok=True)
    command = [
        args.unreal_editor,
        "DominionAscendant.uproject",
        "-unattended",
        "-nop4",
        "-nullrhi",
        "-ExecCmds=Automation RunTests Dominion.Release;Quit",
        "-TestExit=Automation Test Queue Empty",
        f"-DAReleaseFixturePlan={plan_path.relative_to(root).as_posix()}",
        f"-DAReleaseAutomationEvidence={automation_evidence_path.relative_to(root).as_posix()}",
        f"-DAReleaseRunId={args.run_id}",
        f"-DAReleaseBuildId={args.build_id}",
    ]
    relative_log = log_path.relative_to(root).as_posix()
    initial = _not_ready_evidence(
        root,
        command,
        None,
        None,
        run_id=args.run_id,
        build_id=args.build_id,
        fixture_plan=plan_ref,
    )
    _write_json(evidence_path, initial)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        completed = subprocess.run(command, cwd=root, text=True, capture_output=True, check=False)
        output = completed.stdout + completed.stderr
        exit_code = completed.returncode
    except FileNotFoundError as error:
        output = f"{error}\n"
        exit_code = 127
    log_path.write_text(output, encoding="utf-8")
    log_ref = {"path": relative_log, "sha256": _sha256_file(log_path)}
    if exit_code != 0:
        _write_json(
            evidence_path,
            _not_ready_evidence(
                root,
                command,
                exit_code,
                log_ref,
                [
                    f"Dominion.Release Automation did not complete successfully (exit {exit_code}).",
                    "No route, save, controller, onboarding, or defect evidence was inferred from the failed command.",
                ],
                run_id=args.run_id,
                build_id=args.build_id,
                fixture_plan=plan_ref,
            ),
        )
        print("state=NOT_READY")
        print(f"releaseAutomation.exitCode={exit_code}")
        return exit_code
    if not automation_evidence_path.is_file():
        _write_json(
            evidence_path,
            _not_ready_evidence(
                root,
                command,
                0,
                log_ref,
                ["Release Automation wrote no reconciled route/save sidecar."],
                run_id=args.run_id,
                build_id=args.build_id,
                fixture_plan=plan_ref,
            ),
        )
        print("state=NOT_READY")
        return 3
    try:
        automation_payload = validate_automation_evidence(
            root, _load_json(automation_evidence_path), plan_path
        )
        if (
            automation_payload["runId"] != args.run_id
            or automation_payload["buildId"] != args.build_id
        ):
            raise ValueError("release Automation sidecar run/build does not match this invocation")
    except ValueError as error:
        _write_json(
            evidence_path,
            _not_ready_evidence(
                root,
                command,
                0,
                log_ref,
                [str(error)],
                run_id=args.run_id,
                build_id=args.build_id,
                fixture_plan=plan_ref,
            ),
        )
        print("state=NOT_READY")
        print(f"reason={error}")
        return 4
    _write_json(
        evidence_path,
        _not_ready_evidence(
            root,
            command,
            0,
            log_ref,
            [
                "Route/save Automation sidecar exists, but route videos and human controller/onboarding observations are not part of Automation.",
                "Finalize a fully linked candidate only after Task 27 and the human/defect gates are complete.",
            ],
            run_id=args.run_id,
            build_id=args.build_id,
            fixture_plan=plan_ref,
            automation={
                "path": automation_evidence_path.relative_to(root).as_posix(),
                "sha256": _sha256_file(automation_evidence_path),
                "exitCode": 0,
            },
            routes=automation_payload["routes"],
            checkpoints=automation_payload["saveCheckpoints"],
        ),
    )
    print("state=NOT_READY")
    return 5


def command_finalize(args):
    root = Path(args.root).resolve()
    plan_path = Path(args.plan)
    candidate_path = Path(args.candidate)
    evidence_path = Path(args.evidence)
    for name, value in (("plan", plan_path), ("candidate", candidate_path), ("evidence", evidence_path)):
        if not value.is_absolute():
            value = root / value
        if name == "plan":
            plan_path = value
        elif name == "candidate":
            candidate_path = value
        else:
            evidence_path = value
    try:
        candidate = _load_json(candidate_path)
        reconcile_release_candidate(root, candidate, plan_path)
    except ValueError as error:
        preserved = None
        if evidence_path.is_file():
            try:
                existing = load_and_validate_release_evidence(root, evidence_path)
                if existing["state"] == "NOT_READY":
                    preserved = existing
                    preserved["notReadyReasons"] = [
                        f"Finalization rejected: {error}",
                        *existing["notReadyReasons"],
                    ]
            except ValueError:
                preserved = None
        _write_json(
            evidence_path,
            preserved if preserved is not None
            else _not_ready_evidence(root, reasons=[str(error)]),
        )
        print("state=NOT_READY")
        print(f"reason={error}")
        return 2
    _write_json(evidence_path, _ready_evidence(root, candidate, plan_path))
    print("state=READY")
    return 0


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    plan = subparsers.add_parser("plan")
    plan.add_argument("--root", default=".")
    plan.add_argument("--output", required=True)
    plan.set_defaults(handler=command_plan)
    validate = subparsers.add_parser("validate")
    validate.add_argument("--root", default=".")
    validate.set_defaults(handler=command_validate)
    run = subparsers.add_parser("run")
    run.add_argument("--root", default=".")
    run.add_argument("--unreal-editor", default="UnrealEditor-Cmd")
    run.add_argument("--run-id", required=True)
    run.add_argument("--build-id", required=True)
    run.add_argument("--evidence", default="Build/Release/Task28ReleaseEvidence.json")
    run.set_defaults(handler=command_run)
    finalize = subparsers.add_parser("finalize")
    finalize.add_argument("--root", default=".")
    finalize.add_argument("--plan", required=True)
    finalize.add_argument("--candidate", required=True)
    finalize.add_argument("--evidence", default="Build/Release/Task28ReleaseEvidence.json")
    finalize.set_defaults(handler=command_finalize)
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    try:
        return args.handler(args)
    except ValueError as error:
        print(f"release contract error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
