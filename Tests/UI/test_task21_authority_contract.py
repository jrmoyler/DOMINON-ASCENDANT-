import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]


def source(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


class Task21AuthorityContractTests(unittest.TestCase):
    def test_endpoint_delegates_future_features_and_has_no_private_parallel_selection_state(self):
        header = source("Source/DominionUI/Public/Commands/DAUICommandEndpoint.h")
        implementation = source("Source/DominionUI/Private/Commands/DAUICommandEndpoint.cpp")
        contracts = source("Source/DominionUI/Public/Commands/DAUIAuthoritativeService.h")
        self.assertIn("UDAUIAuthoritativeFeatureRegistrySubsystem", contracts)
        self.assertIn("ExecuteRegisteredCommand", implementation)
        for parallel_state in (
            "SelectedCardInstanceId", "TrackedWorldAssetId", "SelectedConquestRouteId",
            "TrackedQuestId", "InspectedHistoryRecordId", "ActiveResearchId",
        ):
            self.assertNotIn(parallel_state, header)
        self.assertNotIn("FDAUITravelRuntime", implementation)
        self.assertIn("ServiceUnavailable", contracts)

    def test_placement_uses_world_authority_and_definition_footprints(self):
        endpoint = source("Source/DominionUI/Private/Commands/DAUICommandEndpoint.cpp")
        world_header = source("Source/DominionWorld/Public/Regions/DAWorldStateSubsystem.h")
        world_implementation = source("Source/DominionWorld/Private/Regions/DAWorldStateSubsystem.cpp")
        self.assertIn("TryPlacePlayerWorldAsset", endpoint)
        self.assertIn("TryPlacePlayerWorldAsset", world_header)
        self.assertIn("Definition->Footprint", world_implementation)
        self.assertIn("CityGridClaims", world_implementation)
        self.assertNotIn("SetAllCellsClaimed(true)", world_implementation)
        self.assertNotIn("FDACityGridSubsystem Grid;", endpoint)
        self.assertNotIn("FIntPoint(1, 1)", endpoint)

    def test_payloads_are_strict_and_indexed_without_duplicate_axis_injection(self):
        endpoint = source("Source/DominionUI/Private/Commands/DAUICommandEndpoint.cpp")
        widgets = source("Source/DominionUI/Private/Widgets/DAGrayboxWidgets.cpp")
        subsystem = source("Source/DominionUI/Private/Subsystems/DAUISubsystem.cpp")
        viewmodels = source("Source/DominionUI/Private/ViewModels/DAUIViewModels.cpp")
        self.assertIn("RequireNumberField", endpoint)
        self.assertIn("Treaty term must be a JSON object", endpoint)
        self.assertIn("BuildPayloadForAction(ActionId, PayloadIndex", subsystem)
        self.assertIn("BuildActionPayload(ActionId, Descriptor, Campaign, Out, StableSelections,", viewmodels)
        self.assertIn("const int32 PayloadIndex", widgets)
        self.assertNotIn('Payload += FString::Printf(TEXT(",\\\"axis\\\"', subsystem)

    def test_back_target_is_enforced_and_selection_updates_the_projection_authority(self):
        router = source("Source/DominionUI/Private/Navigation/DAUIScreenRouter.cpp")
        subsystem = source("Source/DominionUI/Private/Subsystems/DAUISubsystem.cpp")
        viewmodels = source("Source/DominionUI/Private/ViewModels/DAUIViewModels.cpp")
        self.assertIn("ClearLayerRoutes", router)
        self.assertIn("NavigateReplacingHistory", router)
        self.assertIn('Active->BackTarget == TEXT("__exit__")', router)
        self.assertIn("ApplySelectionCommand", subsystem)
        self.assertIn("StableSelections.Add", viewmodels)

    def test_live_hud_and_late_feature_projections_use_registered_typed_sources(self):
        header = source("Source/DominionUI/Public/ViewModels/DAUIViewModels.h")
        implementation = source("Source/DominionUI/Private/ViewModels/DAUIViewModels.cpp")
        self.assertIn("UDAFounderHUDLiveSource", header)
        self.assertIn("UDACommandHUDLiveSource", header)
        self.assertIn("RegisterFounderHUDLiveSource", header)
        self.assertIn("RegisterCommandHUDLiveSource", header)
        self.assertIn("CaptureRegisteredState", implementation)
        self.assertIn("FDAUILeaderResolutionAuthorityRecord", header)

    def test_accessibility_applies_to_registered_runtime_subsystems(self):
        header = source("Source/DominionUI/Public/Accessibility/DAAccessibilitySettings.h")
        implementation = source("Source/DominionUI/Private/Accessibility/DAAccessibilitySettings.cpp")
        widgets = source("Source/DominionUI/Private/Widgets/DAGrayboxWidgets.cpp")
        for contract in (
            "UDASubtitleAccessibilityRuntime", "UDAFlashAccessibilityRuntime",
            "UDACameraAccessibilityRuntime", "UDAGameplayAccessibilityRuntime",
            "UDATutorialAccessibilityRuntime", "UDAInputAccessibilityRuntime",
        ):
            self.assertIn(contract, header)
        self.assertIn("ApplyToRegisteredRuntimes", implementation)
        self.assertIn("ApplySubtitleAccessibility", implementation)
        self.assertIn("ApplyCameraAccessibility", implementation)
        self.assertIn("ApplyGameplayAccessibility", implementation)
        self.assertIn("ApplyTutorialAccessibility", implementation)
        self.assertIn("RollbackPolicy", implementation)
        self.assertIn('OptionId == TEXT("keyboard_rebinding")', widgets)
        self.assertNotIn('return OptionId == TEXT("keyboard_rebinding")', widgets)

    def test_job_assignment_and_world_travel_are_real_atomic_authority_mutations(self):
        endpoint = source("Source/DominionUI/Private/Commands/DAUICommandEndpoint.cpp")
        world_header = source("Source/DominionWorld/Public/Regions/DAWorldStateSubsystem.h")
        world_cpp = source("Source/DominionWorld/Private/Regions/DAWorldStateSubsystem.cpp")
        self.assertIn("SubmitJobAssignmentSignal", endpoint)
        self.assertIn("Candidate.CitySimulationState.JobAssignments", world_cpp)
        self.assertIn("ProjectCitySimulationState(Candidate, true)", world_cpp)
        self.assertIn("TravelUsingRegisteredRuntime", endpoint)
        self.assertIn("RegisterTravelRuntime", world_header)


if __name__ == "__main__":
    unittest.main()
