import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def source(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


def function_body(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[brace : index + 1]
    raise AssertionError(f"unterminated function: {signature}")


class FinalIntegrationContractTests(unittest.TestCase):
    """Portable guard for production seams that previously existed only in tests."""

    def test_authored_starter_manifest_is_exactly_sixty_unique_instances(self):
        manifest = json.loads(
            source("Content/DA/Manifests/VerticalSliceContent.json")
        )
        copies = sum(row["quantity"] for row in manifest["starterDeck"])
        self.assertEqual(copies, 60)
        self.assertEqual(manifest["expectedCounts"]["starterInstances"], 60)
        self.assertEqual(
            len({row["definitionId"] for row in manifest["starterDeck"]}),
            len(manifest["starterDeck"]),
        )

    def test_production_bootstrap_uses_registry_authored_starter_output(self):
        registry = source(
            "Source/DominionCore/Public/Content/DAContentRegistrySubsystem.h"
        )
        world = source(
            "Source/DominionWorld/Private/Regions/DAWorldStateSubsystem.cpp"
        )
        self.assertIn("BuildStarterCampaignContent", registry)
        self.assertIn("BuildStarterCampaignContent", world)
        self.assertIn("CitySimulationState", world)
        self.assertIn("FounderHall", world)

    def test_city_simulation_is_a_core_owned_persisted_snapshot_member(self):
        campaign = source("Source/DominionCore/Public/Save/DACampaignSaveGame.h")
        core_state = source(
            "Source/DominionCore/Public/Simulation/DACitySimulationState.h"
        )
        world = source(
            "Source/DominionWorld/Private/Regions/DAWorldStateSubsystem.cpp"
        )
        self.assertIn("FDACitySimulationState CitySimulationState", campaign)
        self.assertIn("struct DOMINIONCORE_API FDACitySimulationState", core_state)
        self.assertIn("ResolveDevelopmentCycle(Candidate.CitySimulationState)", world)
        self.assertIn("ResolveAssignments(Candidate.CitySimulationState)", world)
        self.assertIn("ResolveWorldTick(Candidate.CitySimulationState)", world)

    def test_shipped_authoritative_ui_and_travel_services_register(self):
        ui_header = source(
            "Source/DominionUI/Public/Commands/DAUIAuthoritativeFeatureSubsystem.h"
        )
        ui_cpp = source(
            "Source/DominionUI/Private/Commands/DAUIAuthoritativeFeatureSubsystem.cpp"
        )
        travel_header = source(
            "Source/DominionWorld/Public/Regions/DARegionTravelRuntimeSubsystem.h"
        )
        travel_cpp = source(
            "Source/DominionWorld/Private/Regions/DARegionTravelRuntimeSubsystem.cpp"
        )
        self.assertIn("IDAUIAuthoritativeFeatureService", ui_header)
        self.assertIn("RegisterAuthoritativeService(this", ui_cpp)
        self.assertIn("IDARegionTravelRuntimeService", travel_header)
        self.assertIn("RegisterTravelRuntime(this", travel_cpp)

    def test_founder_has_real_asc_and_four_granted_abilities(self):
        header = source(
            "Source/DominionGameplay/Public/Founder/DAFounderCharacter.h"
        )
        cpp = source(
            "Source/DominionGameplay/Private/Founder/DAFounderCharacter.cpp"
        )
        abilities = source(
            "Source/DominionGameplay/Public/Founder/DAFounderGameplayAbilities.h"
        )
        self.assertIn("IAbilitySystemInterface", header)
        self.assertIn("AbilitySystemComponent", header)
        self.assertEqual(cpp.count("GiveAbility("), 4)
        for name in (
            "PrecisionScan",
            "DroneBarrier",
            "OrchestrationMark",
            "CoordinatedOverride",
        ):
            self.assertIn(name, abilities)

    def test_combat_components_commit_through_campaign_authority(self):
        authority = source(
            "Source/DominionCore/Public/Campaign/DACampaignAuthority.h"
        )
        structural = source(
            "Source/DominionGameplay/Private/Damage/DAStructuralDamageComponent.cpp"
        )
        capture = source(
            "Source/DominionGameplay/Private/Capture/DACaptureComponent.cpp"
        )
        self.assertIn("TryCommitPersistentCampaign", authority)
        for implementation in (structural, capture):
            self.assertIn("TryCommitPersistentCampaign", implementation)
            self.assertNotIn("UDACampaignSaveGame", implementation)

    def test_snapshot_and_save_envelope_enforce_final_invariants(self):
        validation = source(
            "Source/DominionCore/Private/Save/DACampaignSaveGame.cpp"
        )
        schema = source("Source/DominionCore/Public/Save/DASaveSchema.h")
        fields = source("Source/DominionCore/Public/Save/DASaveJsonFields.h")
        service = source("Source/DominionCore/Private/Save/DASaveService.cpp")
        self.assertIn("ValidateCardOwnershipGraph", validation)
        self.assertIn("CurrentSchemaVersion = 19", schema)
        self.assertIn("ContentVersion", fields)
        self.assertIn("BuildVersion", fields)
        self.assertIn("contentVersion", service)
        self.assertIn("buildVersion", service)

    def test_known_compile_blockers_are_absent(self):
        roots = [ROOT / "Source"]
        cpp_text = "\n".join(
            path.read_text(encoding="utf-8")
            for root in roots
            for path in root.rglob("*")
            if path.suffix in {".h", ".cpp"}
        )
        self.assertNotIn("ADFounderCharacter", cpp_text)
        self.assertNotIn("DeckState.Hand", cpp_text)

    def test_production_bootstrap_opens_exact_wallet_and_hand(self):
        world = source(
            "Source/DominionWorld/Private/Regions/DAWorldStateSubsystem.cpp"
        )
        bootstrap = function_body(
            world, "bool UDAWorldStateSubsystem::InitializeVerticalSliceState("
        )
        compact = re.sub(r"\s+", "", bootstrap)
        self.assertIn("FDAWalletValues(40.f,12.f,8.f)", compact)
        self.assertIn("DeckState.DrawOpeningHand()", compact)
        self.assertNotIn("Resources.Materials=12", compact)

    def test_travel_runtime_loads_authored_map_and_materializes_delta(self):
        region = source("Source/DominionCore/Public/World/DARegionalWorldState.h")
        runtime_header = source(
            "Source/DominionWorld/Public/Regions/DARegionTravelRuntimeSubsystem.h"
        )
        runtime_cpp = source(
            "Source/DominionWorld/Private/Regions/DARegionTravelRuntimeSubsystem.cpp"
        )
        self.assertIn("MapAssetPath", region)
        self.assertIn("IDARegionRuntimeLoader", runtime_header)
        load = function_body(
            runtime_cpp, "bool UDARegionTravelRuntimeSubsystem::EnsureRegionLoaded("
        )
        reconstruct = function_body(
            runtime_cpp,
            "bool UDARegionTravelRuntimeSubsystem::EnsureRegionReconstructed(",
        )
        self.assertIn("LoadRegion", load)
        self.assertLess(load.index("LoadRegion"), load.index("LoadedRequestId = RequestId"))
        self.assertIn("FPackageName::DoesPackageExist", runtime_cpp)
        self.assertIn("LoadLevelInstanceBySoftObjectPtr", runtime_cpp)
        self.assertIn("PersistentDelta.LocalActors", reconstruct)
        self.assertIn("SpawnLocalActor", reconstruct)
        self.assertIn("SpawnWorldAsset", reconstruct)

    def test_founder_abilities_have_exact_cooldowns_and_timed_scoped_effects(self):
        manifest = json.loads(
            source("Content/DA/Abilities/FounderAbilityDefinitions.json")
        )
        cooldowns = {
            row["abilityId"]: row["cooldownSeconds"]
            for row in manifest["definitions"]
        }
        self.assertEqual(
            cooldowns,
            {
                "synara.precision_scan": 8,
                "synara.drone_barrier": 18,
                "synara.orchestration_mark": 12,
                "synara.coordinated_override": 60,
            },
        )
        cpp = source(
            "Source/DominionGameplay/Private/Founder/DAFounderGameplayAbilities.cpp"
        )
        self.assertNotIn("AddLooseGameplayTag", cpp)
        self.assertIn("ResolveAuthoredDefinition", cpp)
        self.assertIn("Definition.CooldownSeconds", cpp)
        self.assertIn("ApplyGameplayEffectSpecToTarget", cpp)
        self.assertIn("ResolveScopedTargets", cpp)
        self.assertIn("DurationSeconds", cpp)
        self.assertIn("RegisterDeployableCover", cpp)
        self.assertIn("UnregisterDeployableCover", cpp)

    def test_campaign_revision_is_universal_monotonic_cas_token(self):
        campaign = source("Source/DominionCore/Public/Save/DACampaignSaveGame.h")
        save_owner = source("Source/DominionCore/Private/Save/DACampaignSaveGame.cpp")
        world_owner = source(
            "Source/DominionWorld/Private/Regions/DAWorldStateSubsystem.cpp"
        )
        self.assertIn("CampaignMutationRevision", campaign)
        for implementation in (save_owner, world_owner):
            commit = function_body(
                implementation,
                (
                    "bool UDACampaignSaveGame::TryCommitPersistentCampaign("
                    if "UDACampaignSaveGame::TryCommitPersistentCampaign" in implementation
                    else "bool UDAWorldStateSubsystem::TryCommitPersistentCampaign("
                ),
            )
            self.assertIn(
                "Candidate.CampaignMutationRevision", commit,
            )
            self.assertRegex(commit, r"\+\+.*CampaignMutationRevision|CampaignMutationRevision\s*\+\s*1")

    def test_rival_construction_never_enters_player_collection(self):
        strategy = source("Source/DominionWorld/Private/AI/DAForgeweaveStrategy.cpp")
        construct_start = strategy.index(
            "if (Decision.Type == EDARivalDecisionType::Construct)"
        )
        construct_end = strategy.index(
            "if (Decision.Type == EDARivalDecisionType::None)", construct_start + 1
        )
        construct = strategy[construct_start:construct_end]
        self.assertNotIn("CollectionState.AddInstanceWithId", construct)
        self.assertNotIn("CollectionState.FindInstance", construct)
        self.assertIn("ProvenanceId", construct)
        self.assertIn("CardInstanceId.Invalidate", construct)

    def test_deployed_zone_is_part_of_atomic_deck_partition(self):
        deck = source("Source/DominionCore/Public/Cards/DADeckState.h")
        validation = source(
            "Source/DominionCore/Private/Save/DACampaignSaveGame.cpp"
        )
        world = source(
            "Source/DominionWorld/Private/Regions/DAWorldStateSubsystem.cpp"
        )
        self.assertIn("GetDeployed", deck)
        self.assertIn("TryDeployFromHand", deck)
        self.assertIn("TryRecoverDeployedInstance", deck)
        self.assertIn("&Deck.GetDeployed()", validation)
        placement = function_body(
            world, "bool UDAWorldStateSubsystem::TryPlacePlayerWorldAsset("
        )
        self.assertIn("TryDeployFromHand", placement)
        self.assertIn("TryRecoverDeployedInstance", world)

    def test_v18_migration_reconstructs_complete_facilities_and_utilities(self):
        city = source("Source/DominionCore/Public/Simulation/DACitySimulationState.h")
        service = source("Source/DominionCore/Private/Save/DASaveService.cpp")
        synthesize = function_body(
            service,
            "bool SynthesizeV19CitySimulationState(FDACampaignSnapshot& Campaign, FString& OutError)\n    {",
        )
        self.assertIn("AuthoredMaintenanceCapitalPerCycle", city)
        self.assertIn("City.UtilitySignals = Campaign.LiveSignals.UtilitySignals", synthesize)
        self.assertIn("BuildRuntimeContent", synthesize)
        for field in (
            "FacilityType",
            "DeploymentCapital",
            "AuthoredMaintenanceCapitalPerCycle",
            "BaseOutput.Capital",
            "BaseOutput.Insight",
            "BaseOutput.Influence",
            "UtilityState",
            "MaintenanceCondition",
        ):
            self.assertIn(field, synthesize)


if __name__ == "__main__":
    unittest.main()
