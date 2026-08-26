import json
import hashlib
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "Content/UI/Manifests/VerticalSliceUI.json"
SCHEMA_PATH = ROOT / "Content/UI/Manifests/VerticalSliceUI.schema.json"

SCREENS = [
    "main_menu", "new_campaign", "settings", "accessibility", "pause",
    "city_hud", "founder_hud", "command_hud", "card_collection",
    "deck_builder", "card_inspect", "blueprint_crafting", "building_inspect",
    "citizen_inspect", "faction_panel", "diplomacy", "treaty_builder",
    "world_map", "quest_journal", "history_timeline", "research",
    "city_metrics", "conquest_dashboard", "leader_resolution",
    "ascension_reward", "save_load", "returning_player_recap",
]

OVERLAYS = [
    "power", "water", "data", "employment", "housing", "happiness",
    "dependency", "adjacency",
]

ACCESSIBILITY = [
    "keyboard_rebinding", "controller_remapping", "text_scale", "subtitles",
    "speaker_labels", "subtitle_background", "color_independent_markers",
    "color_vision_preset", "camera_shake", "field_of_view", "motion_blur",
    "reduced_motion", "reduced_flash", "tactical_pause", "hold_toggle",
    "aim_assist", "build_snap_strength", "tutorial_recall", "tooltip_mode",
]


def canonical_json(value):
    if value is None:
        return "null"
    if value is True:
        return "true"
    if value is False:
        return "false"
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False, separators=(",", ":"))
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return str(int(value)) if value.is_integer() else format(value, ".17g")
    if isinstance(value, list):
        return "[" + ",".join(canonical_json(item) for item in value) + "]"
    if isinstance(value, dict):
        return "{" + ",".join(
            json.dumps(key, ensure_ascii=False) + ":" + canonical_json(value[key])
            for key in sorted(value)
        ) + "}"
    raise TypeError(type(value))


class VerticalSliceUIManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        cls.schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))

    def test_manifest_is_strict_schema_valid_and_has_exact_frozen_counts(self):
        self.assertEqual({
            "fingerprint", "schemaVersion", "visualDirection", "inputContexts", "screens", "overlays",
            "campaignCriticalActions", "accessibilityOptions",
        }, set(self.manifest))
        self.assertEqual(1, self.manifest["schemaVersion"])
        material = dict(self.manifest)
        material.pop("fingerprint")
        canonical = canonical_json(material)
        self.assertEqual(hashlib.sha1(canonical.encode("utf-8")).hexdigest(), self.manifest["fingerprint"])
        self.assertFalse(self.schema["additionalProperties"])
        self.assertEqual(SCREENS, [row["id"] for row in self.manifest["screens"]])
        self.assertEqual(OVERLAYS, [row["id"] for row in self.manifest["overlays"]])
        self.assertEqual(ACCESSIBILITY, [row["id"] for row in self.manifest["accessibilityOptions"]])

    def test_every_surface_has_a_source_class_exact_content_path_and_deterministic_navigation(self):
        known_ids = set(SCREENS)
        for screen in self.manifest["screens"]:
            expected_root = "/Game/UI/HUD/" if screen["id"] in {
                "city_hud", "founder_hud", "command_hud"
            } else "/Game/UI/Screens/"
            self.assertTrue(screen["assetPath"].startswith(expected_root), screen["id"])
            self.assertTrue(screen["widgetClass"].startswith("/Script/DominionUI."), screen["id"])
            self.assertTrue(screen["entryFocusTarget"], screen["id"])
            self.assertIn(screen["backTarget"], known_ids | {"__exit__"})
            self.assertIn(screen["inputMode"], {"game_only", "ui_only", "game_and_ui"})
            self.assertIn(screen["inputContextId"], {
                context["id"] for context in self.manifest["inputContexts"]
            })

    def test_nested_manifest_contract_is_closed_and_typed_without_optional_dependencies(self):
        self.assertEqual(5, len(self.manifest["inputContexts"]))
        for context in self.manifest["inputContexts"]:
            self.assertEqual({"id", "assetPath", "priority"}, set(context))
            self.assertIs(type(context["priority"]), int)
            self.assertGreaterEqual(context["priority"], 0)
        screen_keys = {
            "id", "displayName", "assetPath", "widgetClass", "layer",
            "inputContextId", "inputMode", "entryFocusTarget", "backTarget",
            "dataChannels", "campaignCriticalActions",
        }
        overlay_keys = {"id", "displayName", "assetPath", "widgetClass", "markerShape"}
        action_keys = {
            "id", "commandId", "surfaces", "keyboardInput", "controllerInput", "requiresHover",
        }
        for screen in self.manifest["screens"]:
            self.assertEqual(screen_keys, set(screen), screen["id"])
            self.assertEqual(len(screen["dataChannels"]), len(set(screen["dataChannels"])))
            self.assertEqual(len(screen["campaignCriticalActions"]), len(set(screen["campaignCriticalActions"])))
        for overlay in self.manifest["overlays"]:
            self.assertEqual(overlay_keys, set(overlay), overlay["id"])
        action_ids = set()
        command_ids = set()
        for action in self.manifest["campaignCriticalActions"]:
            self.assertEqual(action_keys, set(action), action["id"])
            self.assertNotIn(action["id"], action_ids)
            self.assertNotIn(action["commandId"], command_ids)
            self.assertEqual(len(action["surfaces"]), len(set(action["surfaces"])))
            self.assertTrue(set(action["surfaces"]).issubset(set(SCREENS)))
            action_ids.add(action["id"])
            command_ids.add(action["commandId"])
        for option in self.manifest["accessibilityOptions"]:
            expected_keys = {"id", "type", "defaultValue"}
            if option["type"] == "float":
                expected_keys |= {"minimum", "maximum"}
            elif option["type"] == "enum":
                expected_keys.add("values")
            self.assertEqual(expected_keys, set(option), option["id"])
            if option["type"] == "boolean":
                self.assertIs(type(option["defaultValue"]), bool)
            elif option["type"] == "float":
                self.assertIs(type(option["defaultValue"]), float)
                self.assertLessEqual(option["minimum"], option["defaultValue"])
                self.assertLessEqual(option["defaultValue"], option["maximum"])
            elif option["type"] == "enum":
                self.assertIn(option["defaultValue"], option["values"])
            else:
                self.assertIn(option["type"], {"binding_map", "hold_toggle_map"})

    def test_huds_bind_the_frozen_authoritative_data_channels(self):
        by_id = {row["id"]: row for row in self.manifest["screens"]}
        self.assertEqual({
            "wallets.capital", "wallets.insight", "wallets.influence", "population",
            "synara.dependency", "deck.hand", "quest.objective",
        }, set(by_id["city_hud"]["dataChannels"]))
        self.assertEqual({
            "founder.health", "founder.guard", "founder.stamina", "founder.abilities",
            "founder.interaction", "quest.objective",
        }, set(by_id["founder_hud"]["dataChannels"]))
        self.assertEqual({
            "command.selected_squads", "command.control_zones", "command.supply",
            "quest.objective", "command.enemy_known_positions", "command.command_points",
            "command.sovereignty", "founder.status", "command.tactical_alert",
        }, set(by_id["command_hud"]["dataChannels"]))

    def test_campaign_actions_have_keyboard_controller_parity_without_hover(self):
        actions = self.manifest["campaignCriticalActions"]
        self.assertGreater(len(actions), 0)
        reachable_by_surface = {screen_id: set() for screen_id in SCREENS}
        for action in actions:
            self.assertFalse(action["requiresHover"], action["id"])
            self.assertTrue(action["keyboardInput"], action["id"])
            self.assertTrue(action["controllerInput"], action["id"])
            self.assertTrue(action["commandId"].startswith("command."), action["id"])
            for screen_id in action["surfaces"]:
                reachable_by_surface[screen_id].add(action["id"])
        for screen in self.manifest["screens"]:
            self.assertEqual(set(screen["campaignCriticalActions"]), reachable_by_surface[screen["id"]])

    def test_controller_can_cycle_every_multi_record_hud_and_activate_the_stable_selection(self):
        actions = {row["id"]: row for row in self.manifest["campaignCriticalActions"]}
        expected = {
            "city_hud": ("city.card_previous", "city.card_next", "city.select_card", 10),
            "founder_hud": ("founder.ability_previous", "founder.ability_next", "founder.activate_ability", 4),
            "command_hud": ("command.squad_previous", "command.squad_next", "command.select_squad", 3),
        }
        for surface, (previous, following, activate, count) in expected.items():
            for action_id in (previous, following, activate):
                self.assertIn(action_id, actions)
                self.assertIn(surface, actions[action_id]["surfaces"])
                self.assertTrue(actions[action_id]["controllerInput"])
                self.assertFalse(actions[action_id]["requiresHover"])
            self.assertEqual(actions[previous]["controllerInput"], "DPadLeft")
            self.assertEqual(actions[following]["controllerInput"], "DPadRight")
            self.assertGreater(count, 2)

    def test_back_and_all_overlays_are_remappable_graph_actions_without_active_surface_collisions(self):
        """Removing a global graph action or reusing one active-screen key must fail this test."""
        actions = {row["id"]: row for row in self.manifest["campaignCriticalActions"]}
        self.assertIn("ui.back", actions)
        expected_overlays = {f"ui.overlay.{overlay_id}" for overlay_id in OVERLAYS}
        self.assertTrue(expected_overlays.issubset(actions))
        self.assertEqual(set(SCREENS), set(actions["ui.back"]["surfaces"]))
        self.assertEqual({"city_hud"}, {
            surface
            for action_id in expected_overlays
            for surface in actions[action_id]["surfaces"]
        })

        by_surface = {screen_id: [] for screen_id in SCREENS}
        for action in actions.values():
            for screen_id in action["surfaces"]:
                by_surface[screen_id].append(action)
        for screen_id, active_actions in by_surface.items():
            for field in ("keyboardInput", "controllerInput"):
                used = set()
                for action in active_actions:
                    tokens = {
                        "1-0": [str(index) for index in range(10)],
                        "1-4": [str(index) for index in range(1, 5)],
                        "1-3": [str(index) for index in range(1, 4)],
                        "ArrowLeftRight": ["ArrowLeft", "ArrowRight"],
                        "DPadLeftRight": ["DPadLeft", "DPadRight"],
                    }.get(action[field], [action[field]])
                    for token in tokens:
                        self.assertNotIn(token, used, f"{screen_id} {field}: {token}")
                        used.add(token)
                    self.assertNotIn("Axis", action[field], action["id"])

    def test_reason_ledger_screens_and_low_chrome_safe_playfield_are_explicit(self):
        by_id = {row["id"]: row for row in self.manifest["screens"]}
        self.assertIn("diplomacy.relationship_reason_ledger", by_id["diplomacy"]["dataChannels"])
        self.assertIn("treaty.term_reason_ledger", by_id["treaty_builder"]["dataChannels"])
        self.assertIn("conquest.outcome_reason_ledger", by_id["conquest_dashboard"]["dataChannels"])
        self.assertIn("world_map.authority_reason_ledger", by_id["world_map"]["dataChannels"])
        style = self.manifest["visualDirection"]
        self.assertEqual("industrial_civic_synara", style["tone"])
        self.assertTrue(style["lowChromeHUD"])
        self.assertTrue(style["keepCenterAndLowerMiddleClear"])
        self.assertEqual("reduced_motion", style["motionSettingId"])
        self.assertEqual("reduced_flash", style["flashSettingId"])

    def test_no_hand_authored_binary_ui_asset_is_committed(self):
        self.assertEqual([], list((ROOT / "Content/UI").rglob("*.uasset")))

    def test_packaged_build_stages_runtime_manifest_and_documents_optional_cache_precook(self):
        packaging = (ROOT / "Config/DefaultGame.ini").read_text(encoding="utf-8")
        self.assertIn('+DirectoriesToAlwaysStageAsNonUFS=(Path="UI/Manifests")', packaging)
        self.assertIn('+DirectoriesToAlwaysCook=(Path="/Game/UI")', packaging)
        engine = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8")
        self.assertIn("GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient", engine)
        self.assertIn("bEnableEnhancedInputSupport=True", engine)
        input_path = ROOT / "Config/DefaultInput.ini"
        self.assertTrue(input_path.is_file())
        input_config = input_path.read_text(encoding="utf-8")
        self.assertIn("DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput", input_config)
        self.assertIn("DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent", input_config)
        readme = (ROOT / "Content/UI/README.md").read_text(encoding="utf-8")
        self.assertIn("native source fallback", readme.lower())
        self.assertIn("BuildGraph", readme)
        self.assertTrue((ROOT / "Build/Scripts/PreCookUI.sh").is_file())

    def test_editor_generator_authors_real_actions_mappings_and_rejects_foreign_assets(self):
        source = (ROOT / "Source/DominionEditor/Private/DAUIAssetCommandlet.cpp").read_text()
        self.assertIn("NewObject<UInputAction>", source)
        self.assertIn("MapFrozenBinding", source)
        self.assertIn("Context->UnmapAll", source)
        self.assertIn("Refusing to overwrite foreign", source)
        validate_only = source[source.index('FParse::Param(*Params, TEXT("ValidateOnly"))'):]
        self.assertIn("PreflightGeneration(Manifest", validate_only)


if __name__ == "__main__":
    unittest.main()
