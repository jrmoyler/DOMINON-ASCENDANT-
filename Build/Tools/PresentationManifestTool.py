#!/usr/bin/env python3
"""Strict canonical presentation source validator and Unreal generation-plan emitter."""

import argparse
import base64
import hashlib
import io
import json
import sys
import wave
from pathlib import Path


class ContractError(ValueError):
    pass


def _load(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ContractError(f"could not read valid JSON {path}: {exc}") from exc


def _exact_keys(value, expected, where):
    if not isinstance(value, dict):
        raise ContractError(f"{where} must be an object")
    actual = set(value)
    expected = set(expected)
    unknown = sorted(actual - expected)
    missing = sorted(expected - actual)
    if unknown:
        raise ContractError(f"{where} has unknown fields: {unknown}")
    if missing:
        raise ContractError(f"{where} is missing fields: {missing}")


def _array(value, where, size=None, minimum=None):
    if not isinstance(value, list):
        raise ContractError(f"{where} must be an array")
    if size is not None and len(value) != size:
        raise ContractError(f"{where} must contain exactly {size} rows")
    if minimum is not None and len(value) < minimum:
        raise ContractError(f"{where} must contain at least {minimum} rows")
    return value


def _string(value, where, prefix=None):
    if not isinstance(value, str) or not value:
        raise ContractError(f"{where} must be a non-empty string")
    if prefix is not None and not value.startswith(prefix):
        raise ContractError(f"{where} must start with {prefix}")
    return value


def _integer(value, where, minimum=None):
    if type(value) is not int:
        raise ContractError(f"{where} must be an integer")
    if minimum is not None and value < minimum:
        raise ContractError(f"{where} must be >= {minimum}")
    return value


def _number(value, where, minimum=None, maximum=None):
    if type(value) not in (int, float):
        raise ContractError(f"{where} must be a number")
    value = float(value)
    if minimum is not None and value < minimum:
        raise ContractError(f"{where} must be >= {minimum}")
    if maximum is not None and value > maximum:
        raise ContractError(f"{where} must be <= {maximum}")
    return value


def _boolean(value, where):
    if type(value) is not bool:
        raise ContractError(f"{where} must be a boolean")
    return value


ROOT_KEYS = {"schemaVersion", "contentId", "fingerprint", "authority", "sources", "expectedCounts"}
SOURCE_PATHS = {
    "buildings": "Buildings/Presentation/PrimaryAssets.json",
    "characters": "Characters/Presentation/PrimaryAssets.json",
    "vfx": "VFX/Presentation/CoreVFX.json",
    "audio": "Audio/Presentation/VerticalSliceAudio.json",
    "bindings": "Buildings/Presentation/FactionBindings.json",
    "artifacts": "DA/Manifests/PresentationArtifactSources.json",
}
COUNTS = {
    "primaryAssets": 50,
    "coreVfx": 25,
    "musicCues": 9,
    "ambientLoops": 12,
    "minimumSfxEvents": 60,
    "constructionGrammars": 3,
    "minimumGeneratedDefinitions": 156,
}
PRIMARY_FACTIONS = {"Synara", "Forgeweave", "EdenCircuit", "Universal", "Player", "Fusion"}
KINDS = {"PrimaryAsset", "CoreVFX", "Music", "Ambient", "SFX"}
CONSTRUCTION_STATES = ["Foundation", "Frame", "Shell", "Systems", "Operational"]
ARTIFACT_CLASSES = {
    "mesh": "StaticMesh",
    "material": "MaterialInstanceConstant",
    "system": "NiagaraSystem",
    "wave": "SoundWave",
    "cue": "SoundCue",
    "sequence": "LevelSequence",
}


def _canonical_fingerprint(manifest, sources):
    root = dict(manifest)
    root.pop("fingerprint", None)
    material = {"manifest": root, "sources": sources}
    return hashlib.sha1(
        json.dumps(material, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def _validate_primary(document, where, character):
    _exact_keys(document, {"schemaVersion", "sourceId", "assets"}, where)
    if document["schemaVersion"] != 1:
        raise ContractError(f"{where}.schemaVersion must be 1")
    _string(document["sourceId"], f"{where}.sourceId", "presentation.primary.")
    rows = _array(document["assets"], f"{where}.assets")
    expected_recipe = (
        {"silhouette", "materials", "requiredVariants", "readabilityHook", "rigProfile"}
        if character
        else {"silhouette", "materials", "signatureElements", "constructionGrammar", "damageLanguage", "requiredStates"}
    )
    seen_ids = set()
    for index, row in enumerate(rows):
        label = f"{where}.assets[{index}]"
        _exact_keys(row, {"number", "id", "displayName", "faction", "kind", "assetPath", "recipe"}, label)
        number = _integer(row["number"], f"{label}.number", 1)
        if number > 50:
            raise ContractError(f"{label}.number exceeds frozen primary scope")
        expected_prefix = f"primary.{number:02d}."
        _string(row["id"], f"{label}.id")
        if row["id"] in seen_ids:
            raise ContractError(f"{where}.assets has duplicate presentation definition id {row['id']}")
        seen_ids.add(row["id"])
        if not row["id"].startswith(expected_prefix):
            raise ContractError(f"{label}.id must start with {expected_prefix}")
        _string(row["displayName"], f"{label}.displayName")
        if row["faction"] not in PRIMARY_FACTIONS:
            raise ContractError(f"{label}.faction is unknown")
        _string(row["kind"], f"{label}.kind")
        _string(row["assetPath"], f"{label}.assetPath", "/Game/Characters/" if character else "/Game/Buildings/")
        _exact_keys(row["recipe"], expected_recipe, f"{label}.recipe")
        recipe = row["recipe"]
        _string(recipe["silhouette"], f"{label}.recipe.silhouette")
        _array(recipe["materials"], f"{label}.recipe.materials", minimum=1)
        for material in recipe["materials"]:
            _string(material, f"{label}.recipe.materials[]")
        if character:
            _array(recipe["requiredVariants"], f"{label}.recipe.requiredVariants", minimum=1)
            _string(recipe["readabilityHook"], f"{label}.recipe.readabilityHook")
            _string(recipe["rigProfile"], f"{label}.recipe.rigProfile")
        else:
            _array(recipe["signatureElements"], f"{label}.recipe.signatureElements", minimum=1)
            _string(recipe["constructionGrammar"], f"{label}.recipe.constructionGrammar", "construction.")
            _string(recipe["damageLanguage"], f"{label}.recipe.damageLanguage", "damage.")
            if recipe["requiredStates"] != ["construction", "operational", "damaged", "ruined"]:
                raise ContractError(f"{label}.recipe.requiredStates is not the frozen production state set")
    return rows


def _validate_vfx(document):
    _exact_keys(document, {"schemaVersion", "sourceId", "effects"}, "vfx")
    if document["schemaVersion"] != 1 or document["sourceId"] != "presentation.core_vfx.v11":
        raise ContractError("vfx identity is not canonical")
    rows = _array(document["effects"], "vfx.effects", size=25)
    for index, row in enumerate(rows):
        label = f"vfx.effects[{index}]"
        _exact_keys(row, {"id", "faction", "family", "assetPath", "recipe"}, label)
        _string(row["id"], f"{label}.id", "vfx.")
        _string(row["faction"], f"{label}.faction")
        _string(row["family"], f"{label}.family")
        _string(row["assetPath"], f"{label}.assetPath", "/Game/VFX/")
        _exact_keys(row["recipe"], {"anticipation", "active", "resolution", "signature", "gameplayRadiusMeters"}, f"{label}.recipe")
        for key in ("anticipation", "active", "resolution", "signature"):
            _string(row["recipe"][key], f"{label}.recipe.{key}")
        radius = row["recipe"]["gameplayRadiusMeters"]
        if type(radius) not in (int, float) or type(radius) is bool or radius <= 0:
            raise ContractError(f"{label}.recipe.gameplayRadiusMeters must be positive")
    return rows


def _validate_audio(document):
    _exact_keys(document, {"schemaVersion", "sourceId", "music", "ambient", "sfx"}, "audio")
    if document["schemaVersion"] != 1 or document["sourceId"] != "presentation.audio.v11":
        raise ContractError("audio identity is not canonical")
    music = _array(document["music"], "audio.music", size=9)
    ambient = _array(document["ambient"], "audio.ambient", size=12)
    sfx = _array(document["sfx"], "audio.sfx", minimum=60)
    for kind, rows, prefix, path_prefix, recipe_keys in [
        ("music", music, "music.", "/Game/Audio/Music/", {"palette", "playback", "purpose"}),
        ("ambient", ambient, "ambient.", "/Game/Audio/Ambient/", {"layers", "spatialized", "loopSeconds"}),
        ("sfx", sfx, "sfx.", "/Game/Audio/SFX/", {"transient", "palette", "event", "variationLayers"}),
    ]:
        for index, row in enumerate(rows):
            label = f"audio.{kind}[{index}]"
            keys = {"id", "displayName", "assetPath", "recipe"} if kind != "sfx" else {"id", "family", "assetPath", "recipe"}
            _exact_keys(row, keys, label)
            _string(row["id"], f"{label}.id", prefix)
            _string(row["assetPath"], f"{label}.assetPath", path_prefix)
            _exact_keys(row["recipe"], recipe_keys, f"{label}.recipe")
            if kind == "music":
                _string(row["displayName"], f"{label}.displayName")
                _array(row["recipe"]["palette"], f"{label}.recipe.palette", minimum=1)
                _string(row["recipe"]["playback"], f"{label}.recipe.playback")
                _string(row["recipe"]["purpose"], f"{label}.recipe.purpose")
            elif kind == "ambient":
                _string(row["displayName"], f"{label}.displayName")
                _array(row["recipe"]["layers"], f"{label}.recipe.layers", minimum=1)
                _boolean(row["recipe"]["spatialized"], f"{label}.recipe.spatialized")
                _integer(row["recipe"]["loopSeconds"], f"{label}.recipe.loopSeconds", 1)
            else:
                _string(row["family"], f"{label}.family")
                _string(row["recipe"]["transient"], f"{label}.recipe.transient")
                _string(row["recipe"]["palette"], f"{label}.recipe.palette")
                _string(row["recipe"]["event"], f"{label}.recipe.event")
                if row["recipe"]["variationLayers"] != 0:
                    raise ContractError(f"{label} must count one distinct authored event before variation layers")
    return music, ambient, sfx


def _validate_bindings(document, all_ids):
    _exact_keys(
        document,
        {"schemaVersion", "sourceId", "constructionSourceHook", "damageSourceHook",
         "constructionGrammars", "damageLanguages", "capturePolicy", "daxton", "ascension"},
        "bindings",
    )
    if document["schemaVersion"] != 1 or document["sourceId"] != "presentation.bindings.v11":
        raise ContractError("binding identity is not canonical")
    if document["constructionSourceHook"] != "UDAConstructionComponent.OnStageChanged":
        raise ContractError("construction must consume UDAConstructionComponent.OnStageChanged")
    if document["damageSourceHook"] != "UDAStructuralDamageComponent.OnDamageStateChanged":
        raise ContractError("damage must consume UDAStructuralDamageComponent.OnDamageStateChanged")
    grammars = _array(document["constructionGrammars"], "bindings.constructionGrammars", size=3)
    if [row.get("faction") for row in grammars] != ["Synara", "Forgeweave", "EdenCircuit"]:
        raise ContractError("construction grammar factions or order are not frozen")
    grammar_ids = set()
    signatures = set()
    for index, grammar in enumerate(grammars):
        label = f"bindings.constructionGrammars[{index}]"
        _exact_keys(grammar, {"faction", "grammarId", "stages"}, label)
        grammar_ids.add(_string(grammar["grammarId"], f"{label}.grammarId", "construction."))
        stages = _array(grammar["stages"], f"{label}.stages", size=5)
        if [row.get("state") for row in stages] != CONSTRUCTION_STATES:
            raise ContractError(f"{label} does not cover the five authoritative construction states")
        signature = []
        for stage_index, stage in enumerate(stages):
            stage_label = f"{label}.stages[{stage_index}]"
            _exact_keys(stage, {"state", "geometryHook", "vfxId", "sfxId"}, stage_label)
            signature.append(_string(stage["geometryHook"], f"{stage_label}.geometryHook"))
            for key in ("vfxId", "sfxId"):
                if stage[key] not in all_ids:
                    raise ContractError(f"{stage_label}.{key} does not resolve: {stage[key]}")
        signatures.add(tuple(signature))
    if len(grammar_ids) != 3 or len(signatures) != 3:
        raise ContractError("construction grammars must be behaviorally distinct")

    damage = _array(document["damageLanguages"], "bindings.damageLanguages", size=3)
    if [row.get("faction") for row in damage] != ["Synara", "Forgeweave", "EdenCircuit"]:
        raise ContractError("damage language factions or order are not frozen")
    for index, row in enumerate(damage):
        label = f"bindings.damageLanguages[{index}]"
        _exact_keys(row, {"faction", "materialHook", "vfxId", "sfxId"}, label)
        _string(row["materialHook"], f"{label}.materialHook")
        for key in ("vfxId", "sfxId"):
            if row[key] not in all_ids:
                raise ContractError(f"{label}.{key} does not resolve")

    capture = document["capturePolicy"]
    _exact_keys(capture, {"sourceHook", "allowsInstantFactionRecolor", "stages"}, "bindings.capturePolicy")
    if capture["sourceHook"] != "UDACaptureComponent.OnCaptureStateChanged":
        raise ContractError("capture must consume UDACaptureComponent.OnCaptureStateChanged")
    if _boolean(capture["allowsInstantFactionRecolor"], "bindings.capturePolicy.allowsInstantFactionRecolor"):
        raise ContractError("capture presentation cannot instantly recolor ownership")
    expected_capture = [
        "original_architecture", "remove_old_signage", "integration_scaffold",
        "install_new_signage", "integrated_operation",
    ]
    if capture["stages"] != expected_capture:
        raise ContractError("capture presentation must use the frozen integration/signage sequence")

    daxton = document["daxton"]
    _exact_keys(daxton, {"sourceAuthority", "states"}, "bindings.daxton")
    if daxton["sourceAuthority"] != "FDADaxtonCampaignState":
        raise ContractError("Daxton binding must consume canonical campaign state")
    states = _array(daxton["states"], "bindings.daxton.states", size=4)
    if [row.get("phase") for row in states] != ["PhaseOne", "PhaseTwo", "PhaseThree", "Resolved"]:
        raise ContractError("Daxton phases are not frozen")
    for index, row in enumerate(states):
        label = f"bindings.daxton.states[{index}]"
        _exact_keys(row, {"phase", "geometryHook", "vfxId", "sfxId"}, label)
        _string(row["geometryHook"], f"{label}.geometryHook")
        if row["vfxId"] not in all_ids or row["sfxId"] not in all_ids:
            raise ContractError(f"{label} has unresolved cues")

    ascension = document["ascension"]
    _exact_keys(
        ascension,
        {"sourceAuthority", "cinematicAssetPath", "gameplayGate", "cinematicMayBeSkipped", "beats"},
        "bindings.ascension",
    )
    if ascension["sourceAuthority"] != "FDAAscensionPresentationState":
        raise ContractError("Ascension binding must consume its read-only presentation projection")
    if ascension["cinematicAssetPath"] != "/Game/Cinematics/CS_ForgeweaveAscension.CS_ForgeweaveAscension":
        raise ContractError("Ascension cinematic path is not canonical")
    if _boolean(ascension["gameplayGate"], "bindings.ascension.gameplayGate"):
        raise ContractError("Ascension cinematic cannot gate gameplay")
    if not _boolean(ascension["cinematicMayBeSkipped"], "bindings.ascension.cinematicMayBeSkipped"):
        raise ContractError("Ascension cinematic must be skippable")
    beats = _array(ascension["beats"], "bindings.ascension.beats", size=5)
    expected_beats = ["SystemsHaltAndReact", "ForgeRelicEmerges", "WorldTransit", "FounderHallReceivesRelic", "Unlocks"]
    if [row.get("beat") for row in beats] != expected_beats:
        raise ContractError("Ascension beats are not canonical")
    for index, row in enumerate(beats):
        label = f"bindings.ascension.beats[{index}]"
        _exact_keys(row, {"beat", "vfxId", "sfxId"}, label)
        if row["vfxId"] not in all_ids or row["sfxId"] not in all_ids:
            raise ContractError(f"{label} has unresolved cues")
    return grammars


def _validate_artifacts(document, project_root):
    _exact_keys(document, {"schemaVersion", "sourceId", "primary", "vfx", "audio", "sequences"}, "artifacts")
    if document["schemaVersion"] != 1 or document["sourceId"] != "presentation.artifact_sources.v11":
        raise ContractError("artifact source identity is not canonical")

    primary = document["primary"]
    _exact_keys(primary, {"meshSourcePath", "meshSourceSha1", "meshClass", "materialSourcePath", "materialSourceSha1", "materialClass"}, "artifacts.primary")
    if primary["meshClass"] != ARTIFACT_CLASSES["mesh"] or primary["materialClass"] != ARTIFACT_CLASSES["material"]:
        raise ContractError("primary artifact classes must be StaticMesh and MaterialInstanceConstant")
    vfx = document["vfx"]
    _exact_keys(vfx, {"systemSourcePath", "systemSourceSha1", "systemClass"}, "artifacts.vfx")
    if vfx["systemClass"] != ARTIFACT_CLASSES["system"]:
        raise ContractError("VFX artifact class must be NiagaraSystem")
    audio = document["audio"]
    _exact_keys(audio, {"waveSourcePath", "waveSourceSha1", "waveClass", "cueClass"}, "artifacts.audio")
    if audio["waveClass"] != ARTIFACT_CLASSES["wave"] or audio["cueClass"] != ARTIFACT_CLASSES["cue"]:
        raise ContractError("audio artifact classes must be SoundWave and SoundCue")

    sources = [
        (primary["meshSourcePath"], primary["meshSourceSha1"]),
        (primary["materialSourcePath"], primary["materialSourceSha1"]),
        (vfx["systemSourcePath"], vfx["systemSourceSha1"]),
        (audio["waveSourcePath"], audio["waveSourceSha1"]),
    ]
    sequences = _array(document["sequences"], "artifacts.sequences", minimum=1)
    for index, row in enumerate(sequences):
        label = f"artifacts.sequences[{index}]"
        _exact_keys(row, {"id", "assetPath", "assetClass", "sourcePath", "sourceSha1", "generatorOwner"}, label)
        _string(row["id"], f"{label}.id", "sequence.")
        _string(row["assetPath"], f"{label}.assetPath", "/Game/Cinematics/")
        if row["assetClass"] != ARTIFACT_CLASSES["sequence"]:
            raise ContractError(f"{label}.assetClass must be LevelSequence")
        _string(row["generatorOwner"], f"{label}.generatorOwner")
        sources.append((row["sourcePath"], row["sourceSha1"]))
    for source_path, expected_sha1 in sources:
        _string(source_path, "artifact source path", "ContentSource/")
        _string(expected_sha1, f"artifact source hash for {source_path}")
        path = project_root / source_path
        try:
            actual_sha1 = hashlib.sha1(path.read_bytes()).hexdigest()
        except OSError as exc:
            raise ContractError(f"artifact source is missing: {source_path}: {exc}") from exc
        if actual_sha1 != expected_sha1:
            raise ContractError(f"artifact source fingerprint drift for {source_path}: expected {expected_sha1}, found {actual_sha1}")
    mesh_text = (project_root / primary["meshSourcePath"]).read_text(encoding="utf-8")
    if "\nv " not in mesh_text or "\nf " not in mesh_text:
        raise ContractError("primary mesh source must contain OBJ vertices and faces")
    palettes = _load(project_root / primary["materialSourcePath"])
    _exact_keys(palettes, {"schemaVersion", "palettes"}, "material source")
    if palettes["schemaVersion"] != 1 or set(palettes.get("palettes", {})) != PRIMARY_FACTIONS:
        raise ContractError("material source must cover every primary faction palette")
    for faction, palette in palettes["palettes"].items():
        label = f"material source.palettes.{faction}"
        _exact_keys(
            palette,
            {"baseColor", "emissiveColor", "metallic", "roughness"},
            label,
        )
        for color_name in ("baseColor", "emissiveColor"):
            color = _array(palette[color_name], f"{label}.{color_name}", size=4)
            for channel in color:
                _number(channel, f"{label}.{color_name}[]", 0.0, 1.0)
        _number(palette["metallic"], f"{label}.metallic", 0.0, 1.0)
        _number(palette["roughness"], f"{label}.roughness", 0.0, 1.0)
    niagara = _load(project_root / vfx["systemSourcePath"])
    _exact_keys(
        niagara,
        {"schemaVersion", "systemTemplate", "emitter", "parameters", "bounds", "warmupSeconds", "autoDeactivate"},
        "Niagara source",
    )
    if niagara["schemaVersion"] != 1 or "empty" in niagara["systemTemplate"].lower():
        raise ContractError("Niagara source must define a non-empty CPU sprite burst")
    emitter = niagara["emitter"]
    _exact_keys(
        emitter,
        {"templateAssetPath", "expectedEmitterName", "simulationTarget", "rendererClass", "minimumEmitterCount", "burstCount", "lifetimeSeconds", "spriteSize", "lifecycle"},
        "Niagara source.emitter",
    )
    _string(emitter["templateAssetPath"], "Niagara source.emitter.templateAssetPath", "/Niagara/")
    _string(emitter["expectedEmitterName"], "Niagara source.emitter.expectedEmitterName")
    if emitter["simulationTarget"] != "CPUSim" or emitter["rendererClass"] != "NiagaraSpriteRendererProperties":
        raise ContractError("Niagara source must define a non-empty CPU sprite burst")
    _integer(emitter["minimumEmitterCount"], "Niagara source.emitter.minimumEmitterCount", 1)
    _integer(emitter["burstCount"], "Niagara source.emitter.burstCount", 1)
    _number(emitter["lifetimeSeconds"], "Niagara source.emitter.lifetimeSeconds", 0.001)
    for dimension in _array(emitter["spriteSize"], "Niagara source.emitter.spriteSize", size=2):
        _number(dimension, "Niagara source.emitter.spriteSize[]", 0.001)
    lifecycle = emitter["lifecycle"]
    _exact_keys(lifecycle, {"mode", "loopBehavior"}, "Niagara source.emitter.lifecycle")
    if lifecycle["mode"] != "Self" or lifecycle["loopBehavior"] != "Once":
        raise ContractError("Niagara source must define a Self/Once emitter lifecycle")
    parameters = _array(niagara["parameters"], "Niagara source.parameters", minimum=1)
    expected_parameter_names = {
        "User.DA.GameplayRadiusMeters", "User.DA.BurstCount",
        "User.DA.LifetimeSeconds", "User.DA.SpriteSize",
    }
    if {row.get("name") for row in parameters if isinstance(row, dict)} != expected_parameter_names:
        raise ContractError("Niagara template does not expose the authored generation parameters")
    for index, parameter in enumerate(parameters):
        label = f"Niagara source.parameters[{index}]"
        if parameter.get("name") == "User.DA.GameplayRadiusMeters":
            _exact_keys(parameter, {"name", "type", "source"}, label)
            if parameter["type"] != "float" or parameter["source"] != "recipe.gameplayRadiusMeters":
                raise ContractError("Niagara gameplay radius must bind the authored VFX recipe")
        else:
            _exact_keys(parameter, {"name", "type", "default"}, label)
    bounds = _array(niagara["bounds"], "Niagara source.bounds", size=6)
    for bound in bounds:
        _number(bound, "Niagara source.bounds[]")
    _number(niagara["warmupSeconds"], "Niagara source.warmupSeconds", 0.0)
    _boolean(niagara["autoDeactivate"], "Niagara source.autoDeactivate")
    try:
        encoded_wave = (project_root / audio["waveSourcePath"]).read_text(encoding="ascii").strip()
        wave_bytes = base64.b64decode(encoded_wave, validate=True)
        with wave.open(io.BytesIO(wave_bytes), "rb") as wav:
            if wav.getnchannels() != 1 or wav.getsampwidth() != 2 or wav.getnframes() <= 0:
                raise ContractError("audio waveform source must be non-empty mono 16-bit PCM")
    except (OSError, ValueError, wave.Error) as exc:
        raise ContractError(f"audio waveform source is not a valid WAV payload: {exc}") from exc
    return {"manifest": document, "palettes": palettes["palettes"], "niagara": niagara}


def load_and_validate(project_root: Path):
    manifest_path = project_root / "Content/DA/Manifests/VerticalSlicePresentation.json"
    manifest = _load(manifest_path)
    _exact_keys(manifest, ROOT_KEYS, "manifest")
    if manifest["schemaVersion"] != 1 or manifest["contentId"] != "presentation.vertical_slice.v11":
        raise ContractError("presentation manifest identity is not canonical")
    _exact_keys(
        manifest["authority"],
        {"frozenScope", "primaryAssetsAndFactionLanguage", "gameplayOwnership", "cachePolicy"},
        "manifest.authority",
    )
    for key, value in manifest["authority"].items():
        _string(value, f"manifest.authority.{key}")
    _exact_keys(manifest["sources"], SOURCE_PATHS, "manifest.sources")
    if manifest["sources"] != SOURCE_PATHS:
        raise ContractError("manifest source paths are not canonical")
    _exact_keys(manifest["expectedCounts"], COUNTS, "manifest.expectedCounts")
    if manifest["expectedCounts"] != COUNTS:
        raise ContractError("manifest expected counts are not frozen")
    fingerprint = _string(manifest["fingerprint"], "manifest.fingerprint")
    if len(fingerprint) != 40 or any(ch not in "0123456789abcdef" for ch in fingerprint):
        raise ContractError("manifest fingerprint must be lowercase SHA-1")

    sources = {
        name: _load(project_root / "Content" / relative)
        for name, relative in manifest["sources"].items()
    }
    buildings = _validate_primary(sources["buildings"], "buildings", False)
    characters = _validate_primary(sources["characters"], "characters", True)
    primary = sorted(buildings + characters, key=lambda row: row["number"])
    if [row["number"] for row in primary] != list(range(1, 51)):
        raise ContractError("primary asset numbers must resolve exactly 1 through 50")

    vfx = _validate_vfx(sources["vfx"])
    music, ambient, sfx = _validate_audio(sources["audio"])
    groups = [
        ("primary", primary), ("vfx", vfx), ("music", music), ("ambient", ambient), ("sfx", sfx)
    ]
    all_rows = [row for _, rows in groups for row in rows]
    ids = [row["id"] for row in all_rows]
    paths = [row["assetPath"] for row in all_rows]
    if len(ids) != len(set(ids)):
        raise ContractError("duplicate presentation definition id")
    if len(paths) != len(set(paths)):
        raise ContractError("duplicate generated presentation asset path")
    if len(all_rows) < COUNTS["minimumGeneratedDefinitions"]:
        raise ContractError("generated definition plan is below the frozen minimum of 156 rows")
    grammars = _validate_bindings(sources["bindings"], set(ids))
    artifacts = _validate_artifacts(sources["artifacts"], project_root)

    expected_fingerprint = _canonical_fingerprint(manifest, sources)
    if fingerprint != expected_fingerprint:
        raise ContractError(
            f"presentation fingerprint is stale: expected {expected_fingerprint}, found {fingerprint}"
        )
    return manifest, sources, groups, grammars, artifacts


def generation_plan(groups):
    kind_names = {
        "primary": "PrimaryAsset",
        "vfx": "CoreVFX",
        "music": "Music",
        "ambient": "Ambient",
        "sfx": "SFX",
    }
    return [
        {"id": row["id"], "kind": kind_names[group], "assetPath": row["assetPath"]}
        for group, rows in groups
        for row in rows
    ]


def artifact_generation_plan(groups, artifacts):
    result = []

    def add(definition, definition_kind, role, asset_class, suffix, source_path, source_sha1, source_content=None):
        row = {
            "definitionId": definition["id"],
            "definitionKind": definition_kind,
            "role": role,
            "assetClass": asset_class,
            "assetPath": definition["assetPath"] + suffix,
            "sourcePath": source_path,
            "sourceSha1": source_sha1,
            "externalGeneratorOwner": "DAPresentationContent",
        }
        if source_content is not None:
            row["sourceContent"] = source_content
            row["sourceContentFingerprint"] = hashlib.sha1(
                json.dumps(source_content, sort_keys=True, separators=(",", ":")).encode("utf-8")
            ).hexdigest()
        result.append(row)

    artifact_manifest = artifacts["manifest"]
    primary = artifact_manifest["primary"]
    for definition in groups[0][1]:
        add(definition, "PrimaryAsset", "mesh", primary["meshClass"], "_Mesh", primary["meshSourcePath"], primary["meshSourceSha1"])
        add(
            definition, "PrimaryAsset", "material", primary["materialClass"],
            "_Material", primary["materialSourcePath"], primary["materialSourceSha1"],
            artifacts["palettes"][definition["faction"]],
        )
    vfx = artifact_manifest["vfx"]
    for definition in groups[1][1]:
        source_content = dict(artifacts["niagara"])
        source_content["definitionParameters"] = definition["recipe"]
        add(
            definition, "CoreVFX", "system", vfx["systemClass"], "_System",
            vfx["systemSourcePath"], vfx["systemSourceSha1"], source_content,
        )
    audio = artifact_manifest["audio"]
    for _, definitions in groups[2:]:
        for definition in definitions:
            kind = {
                "music.": "Music", "ambient.": "Ambient", "sfx.": "SFX"
            }[next(prefix for prefix in ("music.", "ambient.", "sfx.") if definition["id"].startswith(prefix))]
            add(definition, kind, "wave", audio["waveClass"], "_Wave", audio["waveSourcePath"], audio["waveSourceSha1"])
            add(definition, kind, "cue", audio["cueClass"], "_Cue", audio["waveSourcePath"], audio["waveSourceSha1"])
    for sequence in artifact_manifest["sequences"]:
        result.append({
            "definitionId": sequence["id"],
            "definitionKind": "Sequence",
            "role": "sequence",
            "assetClass": sequence["assetClass"],
            "assetPath": sequence["assetPath"],
            "sourcePath": sequence["sourcePath"],
            "sourceSha1": sequence["sourceSha1"],
            "externalGeneratorOwner": sequence["generatorOwner"],
        })
    paths = [row["assetPath"] for row in result]
    if len(paths) != len(set(paths)):
        raise ContractError("artifact generation plan has duplicate asset paths")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("validate", "emit-plan", "emit-artifact-plan"))
    parser.add_argument("--project-root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    try:
        _, _, groups, grammars, artifacts = load_and_validate(args.project_root.resolve())
        if args.command == "validate":
            result = {
                "primaryAssets": len(groups[0][1]),
                "coreVfx": len(groups[1][1]),
                "musicCues": len(groups[2][1]),
                "ambientLoops": len(groups[3][1]),
                "sfxEvents": len(groups[4][1]),
                "constructionGrammars": len(grammars),
                "totalGeneratedDefinitions": sum(len(rows) for _, rows in groups),
            }
        elif args.command == "emit-plan":
            result = generation_plan(groups)
        else:
            result = artifact_generation_plan(groups, artifacts)
        print(json.dumps(result, sort_keys=True))
        return 0
    except ContractError as exc:
        print(f"presentation contract error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
