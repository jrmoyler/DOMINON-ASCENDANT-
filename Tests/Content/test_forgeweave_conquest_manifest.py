import hashlib
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Content/DA/Manifests/ForgeweaveConquest.json"


def canonical_fingerprint(document: dict) -> str:
    material = dict(document)
    material.pop("fingerprint", None)
    encoded = json.dumps(material, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha1(encoded).hexdigest()


class ForgeweaveConquestManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(MANIFEST.read_text(encoding="utf-8")) if MANIFEST.exists() else {}

    def setUp(self):
        self.assertTrue(MANIFEST.exists(), "Forgeweave conquest manifest is missing")

    def test_exact_five_quests_complete_the_twenty_four_authored_so_far(self):
        expected = [
            ("quest.broker_of_ironheart", "Broker of Ironheart", "/Game/DA/Quests/Q_BrokerOfIronheart"),
            ("quest.workers_signal", "The Workers' Signal", "/Game/DA/Quests/Q_WorkersSignal"),
            ("quest.supply_noose", "The Supply Noose", "/Game/DA/Quests/Q_SupplyNoose"),
            ("quest.operation_iron_veil", "Operation Iron Veil", "/Game/DA/Quests/Q_OperationIronVeil"),
            ("quest.third_foundry", "The Third Foundry", "/Game/DA/Quests/Q_ThirdFoundry"),
        ]
        actual = [(row["id"], row["title"], row["assetPath"]) for row in self.document["quests"]]
        self.assertEqual(actual, expected)
        first = json.loads((ROOT / "Content/DA/Manifests/FirstHourQuests.json").read_text())["quests"]
        regional = json.loads((ROOT / "Content/DA/Manifests/RegionalCrisisCampaign.json").read_text())["quests"]
        ascension = json.loads((ROOT / "Content/DA/Manifests/FirstAscension.json").read_text())
        self.assertEqual(len(first) + len(regional) + len(actual) + (1 if ascension["quest"] else 0), 25)

    def test_manifest_fingerprint_and_graphs_fail_closed(self):
        self.assertEqual(self.document["schemaVersion"], 1)
        self.assertEqual(self.document["campaignId"], "campaign.vertical_slice.forgeweave_conquest")
        self.assertEqual(self.document["fingerprint"], canonical_fingerprint(self.document))
        allowed_root = {"schemaVersion", "campaignId", "fingerprint", "quests"}
        self.assertEqual(set(self.document), allowed_root)
        for quest in self.document["quests"]:
            nodes = {node["id"]: node for node in quest["nodes"]}
            self.assertEqual(nodes["start"]["type"], "start")
            self.assertTrue(any(node["type"] == "resolution" for node in quest["nodes"]))
            for node in quest["nodes"]:
                for edge in node["edges"]:
                    self.assertIn(edge["target"], nodes)

    def test_routes_and_system_evidence_are_explicit(self):
        routes = self.document["quests"][0]["choices"]
        self.assertEqual(routes, ["force", "economic", "influence", "alliance"])
        by_id = {row["id"]: row for row in self.document["quests"]}
        self.assertIn("worker_endorsement", by_id["quest.workers_signal"]["systems"])
        self.assertIn("fulfilled_trade", by_id["quest.supply_noose"]["systems"])
        self.assertIn("control_zones", by_id["quest.operation_iron_veil"]["systems"])
        self.assertEqual(
            by_id["quest.third_foundry"]["requirements"],
            {
                "allianceAverageAtLeast": 80,
                "allianceComponentAtLeast": 65,
                "majorGrievanceAllowed": False,
                "thirdFoundryComplete": True,
            },
        )

    def test_assets_are_generated_not_fabricated(self):
        for quest in self.document["quests"]:
            relative = quest["assetPath"].removeprefix("/Game/") + ".uasset"
            self.assertFalse((ROOT / "Content" / relative).exists())
        script = ROOT / "Build/Scripts/PreCookForgeweaveConquest.sh"
        graph = ROOT / "Build/Graph/VerticalSliceForgeweaveConquest.xml"
        self.assertTrue(script.exists())
        self.assertTrue(graph.exists())


if __name__ == "__main__":
    unittest.main()
