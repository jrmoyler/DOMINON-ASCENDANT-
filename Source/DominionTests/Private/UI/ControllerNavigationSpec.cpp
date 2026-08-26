#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Manifest/DAUIManifest.h"
#include "Misc/AutomationTest.h"
#include "Navigation/DAUIScreenRouter.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Subsystems/DAUISubsystem.h"
#include "Commands/DAUICommandEndpoint.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentRegistrySubsystem.h"
#include "Regions/DAWorldStateSubsystem.h"

namespace
{
    void RegisterTransientLayerStacks(UDAUIScreenRouter& Router,
        TArray<UCommonActivatableWidgetStack*>& OutStacks)
    {
        for (const EDAUIScreenLayer Layer : {EDAUIScreenLayer::Menu, EDAUIScreenLayer::HUD,
            EDAUIScreenLayer::Panel, EDAUIScreenLayer::Modal})
        {
            UCommonActivatableWidgetStack* Stack = NewObject<UCommonActivatableWidgetStack>();
            Stack->TakeWidget(); OutStacks.Add(Stack); Router.RegisterLayerStack(Layer, Stack);
        }
    }
}

BEGIN_DEFINE_SPEC(FDAControllerNavigationSpec, "Dominion.UI.ControllerNavigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAControllerNavigationSpec)

void FDAControllerNavigationSpec::Define()
{
    It("keeps production router command and provider services alive through garbage collection", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        FString Error;
        ULocalPlayer* LocalPlayer = Fixture.GetGameInstance()->CreateLocalPlayer(0, Error, false);
        TestNotNull("Valid local player is created", LocalPlayer);
        if (LocalPlayer == nullptr) return;
        UDAUISubsystem* UI = LocalPlayer->GetSubsystem<UDAUISubsystem>();
        TestNotNull("Production UI local-player subsystem exists", UI);
        if (UI == nullptr) return;
        TWeakObjectPtr<UDAUIScreenRouter> Router = UI->GetRouter();
        TWeakObjectPtr<UDAUICommandService> Commands = UI->GetCommandService();
        TWeakObjectPtr<UDAUIViewModelProvider> Provider = UI->GetViewModelProvider();
        CollectGarbage(RF_NoFlags);
        TestTrue("UPROPERTY retains router", Router.IsValid());
        TestTrue("UPROPERTY retains command executor", Commands.IsValid());
        TestTrue("UPROPERTY retains provider", Provider.IsValid());
        Fixture.GetGameInstance()->RemoveLocalPlayer(LocalPlayer);
    });
    It("enters exits focuses and backs through every screen deterministically", [this]()
    {
        FDAUIManifest Manifest; TArray<FText> Errors;
        TestTrue("Manifest loads", FDAUIManifest::LoadCanonical(Manifest, Errors));
        UDAUIScreenRouter* Router = NewObject<UDAUIScreenRouter>();
        TestTrue("Router initializes", Router->Initialize(Manifest, Errors));
        TArray<UCommonActivatableWidgetStack*> LayerStacks;
        RegisterTransientLayerStacks(*Router, LayerStacks);
        for (const FDAUIScreenDescriptor& Screen : Manifest.Screens)
        {
            TestTrue("Every frozen screen enters", Router->EnterScreen(Screen.Id));
            UDAActivatableScreen* ActiveWidget = Router->GetActiveScreenWidget();
            TestNotNull("Every route pushes an actual CommonUI widget instance", ActiveWidget);
            if (ActiveWidget != nullptr)
            {
                TestTrue("Pushed CommonUI widget is activated by its real stack", ActiveWidget->IsActivated());
                TestNotNull("Pushed widget resolves its manifest focus child", ActiveWidget->GetResolvedFocusWidget());
            }
            TestEqual("Entered screen is active", Router->GetActiveScreenId(), Screen.Id);
            TestEqual("Entry focus is stable", Router->GetCurrentFocusTarget(), Screen.EntryFocusTarget);
            TestEqual("Input mode is stable", Router->GetCurrentInputMode(), Screen.InputMode);
            TestTrue("Every frozen screen exits", Router->ExitActiveScreen());
            if (ActiveWidget != nullptr)
                TestFalse("Exit deactivates the popped CommonUI widget", ActiveWidget->IsActivated());
        }

        TestTrue("Main menu enters", Router->EnterScreen(TEXT("main_menu")));
        TestTrue("Settings enters above main menu", Router->EnterScreen(TEXT("settings")));
        TestTrue("Back succeeds", Router->HandleBack());
        TestEqual("Back returns to main menu", Router->GetActiveScreenId(), FName(TEXT("main_menu")));
        TestEqual("Back restores main-menu focus", Router->GetCurrentFocusTarget(), FName(TEXT("button.new_campaign")));

        TestTrue("City route replaces prior history", Router->NavigateReplacingHistory(TEXT("city_hud")));
        TestTrue("City __exit__ BackTarget clears its HUD layer", Router->HandleBack());
        TestTrue("City exit does not reveal an older route", Router->GetActiveScreenId().IsNone());
        TestTrue("Command route enters", Router->EnterScreen(TEXT("command_hud")));
        TestTrue("Command named BackTarget navigates", Router->HandleBack());
        TestEqual("Command BackTarget deterministically replaces with founder",
            Router->GetActiveScreenId(), FName(TEXT("founder_hud")));
    });

    It("constructs one strong production CommonUI stack for every frozen layer", [this]()
    {
        UDARootUILayout* Root = NewObject<UDARootUILayout>();
        Root->TakeWidget();
        for (const EDAUIScreenLayer Layer : {EDAUIScreenLayer::Menu, EDAUIScreenLayer::HUD,
            EDAUIScreenLayer::Panel, EDAUIScreenLayer::Modal})
            TestNotNull("Production root owns the layer stack", Root->GetLayerStack(Layer));
    });

    It("installs the production gameplay command endpoint by default", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        FString Error;
        ULocalPlayer* LocalPlayer = Fixture.GetGameInstance()->CreateLocalPlayer(0, Error, false);
        TestNotNull("Local player exists", LocalPlayer);
        if (LocalPlayer == nullptr) return;
        UDAUISubsystem* UI = LocalPlayer->GetSubsystem<UDAUISubsystem>();
        TestNotNull("Production UI exists", UI);
        if (UI == nullptr) return;
        TestNotNull("Default authoritative gameplay endpoint is installed", UI->GetGameplayCommandEndpoint());
        TestTrue("Default endpoint implements the reflected command contract",
            UI->GetGameplayCommandEndpoint() != nullptr
            && UI->GetGameplayCommandEndpoint()->GetClass()->ImplementsInterface(
                UDAGameplayUICommandEndpoint::StaticClass()));
        TArray<UCommonActivatableWidgetStack*> LayerStacks;
        RegisterTransientLayerStacks(*UI->GetRouter(), LayerStacks);
        TestTrue("City source route enters", UI->GetRouter()->EnterScreen(TEXT("city_hud")));
        TestTrue("Internal navigation command needs no gameplay endpoint", UI->GetRouter()->DispatchAction(
            TEXT("ui.open_founder"), TEXT("{\"source\":\"automation\"}"), Error));
        TestEqual("Default bridge opens founder HUD", UI->GetRouter()->GetActiveScreenId(), FName(TEXT("founder_hud")));
        TestTrue("Internal bridge returns to city", UI->GetRouter()->DispatchAction(
            TEXT("ui.open_city"), TEXT("{\"source\":\"automation\"}"), Error));
        TestFalse("Typed placement rejects a payload without card and grid coordinates",
            UI->GetRouter()->DispatchAction(TEXT("city.place_building"), TEXT("{}"), Error));
        TestTrue("Typed placement validation names the missing authority", Error.Contains(TEXT("cardInstanceId")));
        Fixture.GetGameInstance()->RemoveLocalPlayer(LocalPlayer);
    });

    It("parses every required compound authoritative payload field before dispatch", [this]()
    {
        FDAUIAuthoritativeCommandPayload Payload;
        FString Error;
        TestTrue("Placement requires exact card, city, coordinate, footprint and rotation",
            FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.city.place_building"),
                TEXT("{\"cardInstanceId\":\"00000001-00000002-00000003-00000004\",\"cityId\":\"player_capital\",\"gridX\":7,\"gridY\":9,\"footprintX\":2,\"footprintY\":3,\"rotation\":1}"), Payload, Error));
        TestEqual("Placement X survives reflection parse", Payload.GridCoordinate.X, 7.0);
        TestEqual("Placement Y survives reflection parse", Payload.GridCoordinate.Y, 9.0);
        TestTrue("Fractional placement survives parse until accessibility snapping",
            FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.city.place_building"),
                TEXT("{\"cardInstanceId\":\"00000001-00000002-00000003-00000004\",\"cityId\":\"player_capital\",\"gridX\":7.4,\"gridY\":9.6,\"footprintX\":2,\"footprintY\":3,\"rotation\":1}"), Payload, Error));
        TestEqual("Fractional X is not narrowed before snap", Payload.GridCoordinate.X, 7.4);
        TestFalse("Job command rejects a missing job/facility pair",
            FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.citizen.assign_job"),
                TEXT("{\"citizenId\":\"citizen.synara.nia_vale\"}"), Payload, Error));
        TestTrue("Squad order carries order, destination and optional target",
            FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.command.issue_order"),
                TEXT("{\"squadId\":\"squad.alpha\",\"order\":\"attack\",\"destination\":{\"x\":10,\"y\":20,\"z\":0},\"targetActorId\":\"enemy.1\"}"), Payload, Error));
        TestTrue("Treaty commit requires at least one typed term",
            FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.treaty.commit"),
                TEXT("{\"relationshipId\":\"relationship.synara.forgeweave\",\"treatyTerms\":[{\"termId\":\"term.relief\",\"metric\":\"trust\",\"magnitude\":2}]}"), Payload, Error));
        TestFalse("Crafting quantity cannot be omitted",
            FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.crafting.craft"),
                TEXT("{\"blueprintId\":\"synara.adaptive_habitat\"}"), Payload, Error));
        TestFalse("Order destination requires every coordinate",
            FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.command.issue_order"),
                TEXT("{\"squadId\":\"squad.alpha\",\"order\":\"move\",\"destination\":{\"x\":1,\"y\":2}}"),
                Payload, Error));
        TestFalse("Treaty array values must be objects before dereference",
            FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.treaty.commit"),
                TEXT("{\"relationshipId\":\"relationship.synara.forgeweave\",\"treatyTerms\":[7]}"),
                Payload, Error));
        TestTrue("Malformed treaty reports the object diagnostic",
            Error.Contains(TEXT("Treaty term must be a JSON object")));
        TestFalse("Rotation is validated before uint8 narrowing",
            FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.city.place_building"),
                TEXT("{\"cardInstanceId\":\"00000001-00000002-00000003-00000004\",\"cityId\":\"player_capital\",\"gridX\":7,\"gridY\":9,\"footprintX\":2,\"footprintY\":3,\"rotation\":256}"), Payload, Error));
        TestTrue("Wide rotation reports the exact range", Error.Contains(TEXT("rotation in [0,3]")));
        TestFalse("Craft quantity is bounded before int32 narrowing",
            FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.crafting.craft"),
                TEXT("{\"blueprintId\":\"synara.adaptive_habitat\",\"quantity\":2147483648}"), Payload, Error));
        TestTrue("Wide quantity reports the exact bound", Error.Contains(TEXT("between 1 and 100")));
    });

    It("commits canonical placement and job capacity while unavailable services fail closed", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDAContentRegistrySubsystem* Registry = Fixture.GetSubsystem<UDAContentRegistrySubsystem>();
        UDAUICommandEndpointSubsystem* Endpoint = Fixture.GetSubsystem<UDAUICommandEndpointSubsystem>();
        TestNotNull("World authority exists", World);
        TestNotNull("Content authority exists", Registry);
        TestNotNull("Endpoint exists", Endpoint);
        if (World == nullptr || Registry == nullptr || Endpoint == nullptr) return;

        UDA_CardDefinition* Definition = Registry->GetCardDefinition(TEXT("synara.adaptive_habitat"));
        TestNotNull("Authored placeable definition resolves", Definition);
        if (Definition != nullptr)
        {
            FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
            const FGuid CardId(11, 12, 13, 14);
            Candidate.CollectionState.AddInstanceWithId(CardId, Definition->DefinitionId,
                EDAAcquisitionSource::StarterDeck, Candidate.WorldState.CurrentWorldTick);
            Candidate.LiveSignals.Capital = 100000;
            Candidate.LiveSignals.Insight = 100000;
            Candidate.LiveSignals.Influence = 100000;
            TestTrue("Fixture campaign accepts the owned card", World->RestorePersistentCampaign(Candidate));
            FGuid WorldAssetId; FString PlacementError;
            TestTrue("Definition footprint places on the claimed canonical city grid",
                World->TryPlacePlayerWorldAsset(CardId, TEXT("player_capital"), FIntPoint(0, 0),
                    Definition->Footprint, EGridRotation::Zero, WorldAssetId, PlacementError));
            TestTrue("Placement constructs a stable persisted WorldAsset ID", WorldAssetId.IsValid()
                && World->GetPersistentCampaign().WorldAssets.ContainsByPredicate(
                    [WorldAssetId](const FDAWorldAssetRecord& Row)
                    { return Row.WorldAssetId == WorldAssetId; }));
            int32 AuthoredCycles = 0;
            const FDAWorldAssetRecord* Placed = World->GetPersistentCampaign().FindWorldAssetRecord(WorldAssetId);
            TestTrue("Placement uses explicit authored construction timing",
                Definition->TryGetConstructionCycles(AuthoredCycles) && AuthoredCycles > 0
                && Placed != nullptr && Placed->ConstructionCyclesRequired == AuthoredCycles);
            FDACampaignSnapshot Reloaded = World->GetPersistentCampaign();
            const FGuid SecondCardId(15, 16, 17, 18);
            Reloaded.CollectionState.AddInstanceWithId(SecondCardId, Definition->DefinitionId,
                EDAAcquisitionSource::StarterDeck, Reloaded.WorldState.CurrentWorldTick);
            TestTrue("Snapshot-valid canonical claims and occupancy restore", World->RestorePersistentCampaign(Reloaded));
            FGuid RejectedAssetId;
            TestFalse("Restored occupancy rejects an exact overlap",
                World->TryPlacePlayerWorldAsset(SecondCardId, TEXT("player_capital"), FIntPoint(0, 0),
                    Definition->Footprint, EGridRotation::Zero, RejectedAssetId, PlacementError));
            TestEqual("Restored overlap returns the exact grid diagnostic", PlacementError,
                FString(TEXT("Canonical claimed city grid rejected the authored footprint.")));
            FDACampaignSnapshot TamperedClaims = World->GetPersistentCampaign();
            FDACityGridClaimState* Claims = TamperedClaims.FindCityGridClaims(TEXT("player_capital"));
            if (Claims != nullptr) Claims->ClaimedCells.Remove(FIntPoint(0, 0));
            FString ClaimError;
            TestFalse("A snapshot cannot strand a placed asset outside its persisted claims",
                TamperedClaims.Validate(ClaimError));
            TestEqual("Claim tamper reports exact reconstruction authority", ClaimError,
                FString(TEXT("Player-capital WorldAsset origins must remain inside persisted canonical grid claims.")));
        }

        const FGuid PriorFacility(21, 22, 23, 24);
        const FGuid ChosenFacility(31, 32, 33, 34);
        FDACampaignSnapshot FacilityCandidate = World->GetPersistentCampaign();
        FDAWorldAssetRecord PriorAsset; PriorAsset.WorldAssetId = PriorFacility;
        PriorAsset.CardDefinitionId = TEXT("synara.cognitive_operations_tower");
        PriorAsset.CityId = TEXT("player_capital"); PriorAsset.OwnerCivilizationId = TEXT("civilization.synara");
        PriorAsset.ConstructionState = EDAConstructionState::Operational;
        PriorAsset.ConstructionCyclesRequired = 4; PriorAsset.ConstructionCyclesCompleted = 4;
        PriorAsset.GridOrigin = FIntPoint(8, 8);
        FDAWorldAssetRecord ChosenAsset = PriorAsset; ChosenAsset.WorldAssetId = ChosenFacility;
        ChosenAsset.GridOrigin = FIntPoint(12, 8);
        PriorAsset.CardInstanceId = FGuid(21, 22, 23, 25);
        ChosenAsset.CardInstanceId = FGuid(31, 32, 33, 35);
        FacilityCandidate.CollectionState.AddInstanceWithId(PriorAsset.CardInstanceId,
            PriorAsset.CardDefinitionId, EDAAcquisitionSource::Conquest,
            FacilityCandidate.WorldState.CurrentWorldTick);
        FacilityCandidate.CollectionState.AddInstanceWithId(ChosenAsset.CardInstanceId,
            ChosenAsset.CardDefinitionId, EDAAcquisitionSource::Conquest,
            FacilityCandidate.WorldState.CurrentWorldTick);
        for (const FDAWorldAssetRecord* Asset : {&PriorAsset, &ChosenAsset})
        {
            FCardInstance* Card = FacilityCandidate.CollectionState.FindInstance(Asset->CardInstanceId);
            if (Card != nullptr)
            {
                Card->WorldAssetId = Asset->WorldAssetId;
                Card->RecoveryState = EDARecoveryState::Deployed;
            }
            FDAFacilityContext& Facility =
                FacilityCandidate.CitySimulationState.Facilities.Emplace_GetRef();
            Facility.AssetRecord = *Asset;
        }
        FacilityCandidate.WorldAssets.Add(PriorAsset); FacilityCandidate.WorldAssets.Add(ChosenAsset);
        TestTrue("Valid facility WorldAsset fixtures restore", World->RestorePersistentCampaign(FacilityCandidate));
        FDACampaignCitizenSignal Citizen; Citizen.CitizenId = TEXT("citizen.capacity.test");
        Citizen.CityId = TEXT("player_capital"); Citizen.JobId = TEXT("job.prior");
        FDACampaignJobOpeningSignal Prior; Prior.JobId = TEXT("job.prior");
        Prior.CityId = TEXT("player_capital"); Prior.FacilityWorldAssetId = PriorFacility;
        Prior.OpenPositions = 0;
        FDACampaignJobOpeningSignal Chosen; Chosen.JobId = TEXT("job.chosen");
        Chosen.CityId = TEXT("player_capital"); Chosen.FacilityWorldAssetId = ChosenFacility;
        Chosen.OpenPositions = 2;
        FDACampaignJobAssignmentSignal Assignment; Assignment.CitizenId = Citizen.CitizenId;
        Assignment.JobId = Prior.JobId; Assignment.FacilityWorldAssetId = PriorFacility;
        TestTrue("Citizen signal commits", World->SubmitCitizenSignal(Citizen));
        TestTrue("Prior opening commits", World->SubmitJobOpeningSignal(Prior));
        TestTrue("Chosen opening commits", World->SubmitJobOpeningSignal(Chosen));
        TestTrue("Prior assignment commits", World->SubmitJobAssignmentSignal(Assignment));
        FDAUICommandRequest JobRequest; JobRequest.CommandId = TEXT("command.citizen.assign_job");
        JobRequest.SourceScreenId = TEXT("citizen_inspect");
        JobRequest.PayloadJson = FString::Printf(TEXT("{\"citizenId\":\"%s\",\"jobId\":\"job.chosen\",\"facilityWorldAssetId\":\"%s\"}"),
            *Citizen.CitizenId.ToString(), *ChosenFacility.ToString());
        FString Error;
        TestTrue("Endpoint atomically reassigns the citizen",
            IDAGameplayUICommandEndpoint::Execute_ExecuteGameplayUICommand(Endpoint, JobRequest, Error));
        const FDACampaignLiveSignalState& Signals = World->GetLiveSignals();
        const FDACampaignJobOpeningSignal* PriorAfter = Signals.JobOpenings.FindByPredicate(
            [PriorFacility](const FDACampaignJobOpeningSignal& Row)
            { return Row.FacilityWorldAssetId == PriorFacility; });
        const FDACampaignJobOpeningSignal* ChosenAfter = Signals.JobOpenings.FindByPredicate(
            [ChosenFacility](const FDACampaignJobOpeningSignal& Row)
            { return Row.FacilityWorldAssetId == ChosenFacility; });
        TestTrue("Prior capacity is released and chosen capacity consumed",
            PriorAfter != nullptr && ChosenAfter != nullptr
            && PriorAfter->OpenPositions == 1 && ChosenAfter->OpenPositions == 1);

        FDAUICommandRequest TravelRequest; TravelRequest.CommandId = TEXT("command.world.travel");
        TravelRequest.SourceScreenId = TEXT("world_map");
        TravelRequest.PayloadJson = TEXT("{\"regionId\":\"region.ironheart\"}");
        TestFalse("Travel cannot succeed without the registered world runtime",
            IDAGameplayUICommandEndpoint::Execute_ExecuteGameplayUICommand(Endpoint, TravelRequest, Error));
        TestTrue("Missing world runtime is explicit", Error.Contains(TEXT("ServiceUnavailable")));

        for (const TPair<FName, FString>& Unavailable : TArray<TPair<FName, FString>>{
            {TEXT("command.history.inspect"), TEXT("{\"historyRecordId\":\"history.test\"}")},
            {TEXT("command.metrics.inspect"), TEXT("{\"metricId\":\"metric.test\"}")},
            {TEXT("command.founder.interact"), TEXT("{\"interactionId\":\"interaction.test\"}")}})
        {
            FDAUICommandRequest Request; Request.CommandId = Unavailable.Key;
            Request.SourceScreenId = TEXT("automation"); Request.PayloadJson = Unavailable.Value;
            TestFalse("Typed interaction cannot report false success without a handler",
                IDAGameplayUICommandEndpoint::Execute_ExecuteGameplayUICommand(Endpoint, Request, Error));
            TestTrue("Missing typed handler is explicit", Error.Contains(TEXT("ServiceUnavailable")));
        }
    });

    It("applies every accessibility field through registered downstream runtimes", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAAccessibilityRuntimeBridgeSubsystem* Bridge =
            Fixture.GetSubsystem<UDAAccessibilityRuntimeBridgeSubsystem>();
        TestNotNull("Accessibility bridge exists", Bridge);
        if (Bridge == nullptr) return;
        UDAUIVisualAccessibilityAdapter* Visual = NewObject<UDAUIVisualAccessibilityAdapter>();
        UDACameraAccessibilityAdapter* Camera = NewObject<UDACameraAccessibilityAdapter>();
        UDAInputAccessibilityAdapter* Input = NewObject<UDAInputAccessibilityAdapter>();
        UDAGameplayAccessibilityAdapter* Gameplay = NewObject<UDAGameplayAccessibilityAdapter>();
        UDATutorialAccessibilityAdapter* Tutorial = NewObject<UDATutorialAccessibilityAdapter>();
        FString Error;
        TestTrue("Subtitle/flash runtime registers", Bridge->RegisterRuntime(Visual, Error));
        TestTrue("Camera runtime registers", Bridge->RegisterRuntime(Camera, Error));
        TestTrue("Input runtime registers", Bridge->RegisterRuntime(Input, Error));
        TestTrue("Gameplay runtime registers", Bridge->RegisterRuntime(Gameplay, Error));
        TestTrue("Tutorial runtime registers", Bridge->RegisterRuntime(Tutorial, Error));
        FDAAccessibilitySettings Settings = FDAAccessibilitySettings::MakeDefaults();
        Settings.bSubtitles = false; Settings.bSpeakerLabels = false;
        Settings.SubtitleBackgroundOpacity = 0.25f; Settings.bReducedFlash = true;
        Settings.CameraShake = 0.5f; Settings.FieldOfView = 105.f;
        Settings.bMotionBlur = false; Settings.bTacticalPause = false;
        Settings.AimAssist = 0.75f; Settings.BuildSnapStrength = 0.25f;
        Settings.bTutorialRecall = false; Settings.TooltipMode = EDATooltipMode::Advanced;
        TestTrue("Every registered downstream accepts the complete runtime policy",
            Bridge->ApplyToRegisteredRuntimes(FDAAccessibilityRuntimePolicy::FromSettings(Settings), Error));
        TestFalse("Subtitle target is mutated", Visual->ShouldRenderSubtitles());
        TestEqual("Subtitle background target is mutated", Visual->GetSubtitleBackgroundOpacity(), 0.25f);
        TestEqual("Fractional camera shake reaches the camera runtime", Camera->GetAppliedShakeScale(), 0.5f);
        TestFalse("Tactical Pause target is mutated", Gameplay->IsTacticalPauseEnabled());
        TestEqual("Aim target is mutated", Gameplay->GetAimAssist(), 0.75f);
        TestFalse("Tutorial target is mutated", Tutorial->IsRecallEnabled());
        TestEqual("Tooltip target is mutated", Tutorial->GetTooltipMode(), EDATooltipMode::Advanced);
        TestTrue("Subtitle renderer suppresses disabled cues",
            Visual->RenderSubtitleCue(TEXT("speaker.daxton"), FText::FromString(TEXT("Warning"))).IsEmpty());
        TestTrue("Reduced-flash gate still permits essential effects", Visual->ShouldPlayFlashEffect(true));
        TestFalse("Reduced-flash gate suppresses nonessential effects", Visual->ShouldPlayFlashEffect(false));
        TestEqual("Fractional shake is consumed by the camera-shake path",
            Camera->ScaleCameraShakeAmplitude(8.f), 4.f);
        TestTrue("Founder aim assist changes the raw direction fractionally",
            Gameplay->ApplyAimAssist(FVector::ForwardVector, FVector::RightVector).Y > 0.f);
        TestTrue("Placement snap strength moves a fractional coordinate toward the exact grid",
            Gameplay->SnapBuildCoordinate(FVector(1.4, 2.4, 0.0)).X < 1.4);
        TestEqual("Zero snap keeps the pointer in its unsnapped containing cell",
            UDAGameplayAccessibilityAdapter::ResolveBuildGridOrigin(FVector2D(1.9, 2.9), 0.f),
            FIntPoint(1, 2));
        TestEqual("Fractional snap observably biases the same pointer to the nearest grid cell",
            UDAGameplayAccessibilityAdapter::ResolveBuildGridOrigin(FVector2D(1.9, 2.9), 0.25f),
            FIntPoint(2, 3));
        TestFalse("Disabled tutorial recall rejects a concrete tutorial", Tutorial->CanRecallTutorial(TEXT("tutorial.city")));
        TestTrue("Advanced tooltip mode is consumed by the UI", Tutorial->ShouldRenderAdvancedTooltip());
    });

    It("dispatches every valid direct placement press through the production input boundary", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        UDAContentRegistrySubsystem* Registry = Fixture.GetSubsystem<UDAContentRegistrySubsystem>();
        TestNotNull("World authority exists", World);
        TestNotNull("Content authority exists", Registry);
        if (World == nullptr || Registry == nullptr) return;
        UDA_CardDefinition* Definition = Registry->GetCardDefinition(TEXT("synara.adaptive_habitat"));
        TestNotNull("Authored placeable definition resolves", Definition);
        if (Definition == nullptr) return;
        const FGuid FirstCardId(22, 4, 1, 1);
        const FGuid SecondCardId(22, 4, 1, 2);
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        Candidate.CollectionState.AddInstanceWithId(FirstCardId, Definition->DefinitionId,
            EDAAcquisitionSource::StarterDeck, Candidate.WorldState.CurrentWorldTick);
        Candidate.CollectionState.AddInstanceWithId(SecondCardId, Definition->DefinitionId,
            EDAAcquisitionSource::StarterDeck, Candidate.WorldState.CurrentWorldTick);
        Candidate.LiveSignals.Capital = 100000;
        Candidate.LiveSignals.Insight = 100000;
        Candidate.LiveSignals.Influence = 100000;
        TestTrue("Two placement cards restore", World->RestorePersistentCampaign(Candidate));

        FString Error;
        ULocalPlayer* LocalPlayer = Fixture.GetGameInstance()->CreateLocalPlayer(0, Error, false);
        TestNotNull("Local player exists", LocalPlayer);
        if (LocalPlayer == nullptr) return;
        UDAUISubsystem* UI = LocalPlayer->GetSubsystem<UDAUISubsystem>();
        TestNotNull("Production UI exists", UI);
        if (UI == nullptr) return;
        TArray<UCommonActivatableWidgetStack*> LayerStacks;
        RegisterTransientLayerStacks(*UI->GetRouter(), LayerStacks);
        TestTrue("City screen enters", UI->GetRouter()->EnterScreen(TEXT("city_hud")));
        UDAInputAccessibilityAdapter* InputAccessibility = UI->GetInputAccessibility();
        TestNotNull("Production input accessibility exists", InputAccessibility);
        if (InputAccessibility == nullptr)
        {
            Fixture.GetGameInstance()->RemoveLocalPlayer(LocalPlayer);
            return;
        }
        TestEqual("Canonical placement action defaults to direct Press semantics",
            InputAccessibility->GetAppliedMode(TEXT("city.place_building")),
            EDAHoldToggleMode::Press);
        const int32 AssetCountBefore = World->GetPersistentCampaign().WorldAssets.Num();
        const FString FirstPayload = FString::Printf(
            TEXT("{\"cardInstanceId\":\"%s\",\"cityId\":\"player_capital\",\"gridX\":0,\"gridY\":0,\"footprintX\":%d,\"footprintY\":%d,\"rotation\":0}"),
            *FirstCardId.ToString(EGuidFormats::DigitsWithHyphens),
            Definition->Footprint.X, Definition->Footprint.Y);
        const FString SecondPayload = FString::Printf(
            TEXT("{\"cardInstanceId\":\"%s\",\"cityId\":\"player_capital\",\"gridX\":%d,\"gridY\":0,\"footprintX\":%d,\"footprintY\":%d,\"rotation\":0}"),
            *SecondCardId.ToString(EGuidFormats::DigitsWithHyphens),
            Definition->Footprint.X + 1, Definition->Footprint.X, Definition->Footprint.Y);
        TestTrue("First direct placement press commits", UI->DispatchAccessibleAction(
            TEXT("city.place_building"), FirstPayload, true, Error));
        TestTrue("Second direct placement press commits", UI->DispatchAccessibleAction(
            TEXT("city.place_building"), SecondPayload, true, Error));
        TestEqual("No alternating press is suppressed",
            World->GetPersistentCampaign().WorldAssets.Num(), AssetCountBefore + 2);
        Fixture.GetGameInstance()->RemoveLocalPlayer(LocalPlayer);
    });

    It("reaches every campaign action by keyboard and controller and dispatches only through a service", [this]()
    {
        FDAUIManifest Manifest; TArray<FText> Errors;
        TestTrue("Manifest loads", FDAUIManifest::LoadCanonical(Manifest, Errors));
        UDAUIScreenRouter* Router = NewObject<UDAUIScreenRouter>();
        TestTrue("Router initializes", Router->Initialize(Manifest, Errors));
        TArray<UCommonActivatableWidgetStack*> LayerStacks;
        RegisterTransientLayerStacks(*Router, LayerStacks);
        for (const FDAUICampaignActionDescriptor& Action : Manifest.CampaignCriticalActions)
        {
            TestTrue("Keyboard route exists", Router->IsActionReachable(Action.Id, EDAUIInputDevice::Keyboard));
            TestTrue("Controller route exists", Router->IsActionReachable(Action.Id, EDAUIInputDevice::Controller));
            TestFalse("No action depends on hover", Action.bRequiresHover);
        }

        FString Error;
        TestTrue("City HUD enters for its campaign actions", Router->EnterScreen(TEXT("city_hud")));
        UDAUIViewModelProvider* Provider = NewObject<UDAUIViewModelProvider>();
        Provider->SetAuthoritativeSnapshot(FDACampaignSnapshot{});
        Router->SetViewModelProvider(Provider);
        TestTrue("Player input toggles a typed overlay", Router->ToggleOverlay(TEXT("power"), Error));
        TestEqual("Power overlay becomes active", Router->GetActiveOverlayId(), FName(TEXT("power")));
        TestTrue("Universal CommonUI back closes the active overlay first", Router->HandleBack());
        TestTrue("Overlay is closed without popping the screen", Router->GetActiveOverlayId().IsNone());
        TestEqual("City HUD remains active after overlay back", Router->GetActiveScreenId(), FName(TEXT("city_hud")));
        TestFalse("No simulation command executes without a service",
            Router->DispatchAction(TEXT("city.place_building"), TEXT("{\"cardId\":\"synara.adaptive_habitat\"}"), Error));
        TArray<FDAUICommandRequest> Requests;
        UDAUICommandService* Commands = NewObject<UDAUICommandService>();
        Commands->SetNativeExecutor([&Requests](const FDAUICommandRequest& Request, FString&)
        { Requests.Add(Request); return true; });
        Router->SetCommandService(Commands);
        TestTrue("Registered campaign action dispatches", Router->DispatchAction(
            TEXT("city.place_building"), TEXT("{\"cardId\":\"synara.adaptive_habitat\"}"), Error));
        TestEqual("Exactly one service command executes", Requests.Num(), 1);
        TestEqual("Manifest command identity is preserved", Requests[0].CommandId,
            FName(TEXT("command.city.place_building")));

        UDACityHUDWidget* CityWidget = NewObject<UDACityHUDWidget>();
        CityWidget->ConfigureScreen(*Manifest.FindScreen(TEXT("city_hud")), Router);
        CityWidget->TakeWidget();
        UDAActionButton* PlacementButton = CityWidget->FindActionButton(TEXT("city.place_building"));
        TestNotNull("Manifest placement route creates a concrete action button", PlacementButton);
        if (PlacementButton != nullptr)
        {
            TestFalse("Concrete buttons carry a stable non-empty label", PlacementButton->GetLabelText().IsEmpty());
            TestFalse("Concrete buttons never dispatch an anonymous empty payload",
                PlacementButton->GetPayloadJson() == TEXT("{}"));
            TestTrue("Concrete button dispatches through the tracked service", PlacementButton->Trigger());
        }
        TestEqual("Button adds exactly one service request", Requests.Num(), 2);
    });

    It("cycles and activates all ten hand records with controller-only stable targeting", [this]()
    {
        FDAUIManifest Manifest; TArray<FText> Errors;
        TestTrue("Manifest loads", FDAUIManifest::LoadCanonical(Manifest, Errors));
        const FDAUIScreenDescriptor* City = Manifest.FindScreen(TEXT("city_hud"));
        TestNotNull("City HUD descriptor exists", City);
        if (City == nullptr) return;
        FDACampaignSnapshot Campaign;
        TArray<FGuid> ControllerDeck;
        for (int32 Index = 0; Index < 10; ++Index)
        {
            const FGuid CardId(2200, 10, Index + 1, 1);
            Campaign.CollectionState.AddInstanceWithId(CardId,
                FName(*FString::Printf(TEXT("card.controller.%d"), Index)),
                EDAAcquisitionSource::StarterDeck, 0);
            ControllerDeck.Add(CardId);
        }
        Campaign.DeckState.SetInstanceIds(ControllerDeck);
        for (int32 Index = 0; Index < 10; ++Index) Campaign.DeckState.DrawForCycle();
        UDAUIViewModelProvider* Provider = NewObject<UDAUIViewModelProvider>();
        Provider->SetAuthoritativeSnapshot(Campaign);
        TSet<FGuid> Activated;
        FString Error;
        for (int32 Index = 0; Index < 10; ++Index)
        {
            TestTrue("Controller next cycles a canonical hand record",
                Provider->CycleStableSelection(TEXT("city_hud"), 1, Error));
            FDAUIScreenProjection Projection;
            TestTrue("City projection rebuilds from stable controller selection",
                Provider->BuildScreen(*City, Projection));
            const FDAUIIndexedActionPayloads* Payloads =
                Projection.IndexedActionPayloads.Find(TEXT("city.select_card"));
            FDAUIAuthoritativeCommandPayload Parsed;
            TestTrue("Controller activation payload targets the current exact record", Payloads != nullptr
                && FDAUIAuthoritativeCommandPayload::ParseAndValidate(TEXT("command.city.select_card"),
                    Payloads->ControllerValue, Parsed, Error));
            Activated.Add(Parsed.CardInstanceId);
        }
        TestEqual("All ten hand records are controller reachable without hover", Activated.Num(), 10);
        FDACampaignSnapshot SingleCardCampaign;
        const FGuid SingleCardId(2200, 10, 99, 1);
        SingleCardCampaign.CollectionState.AddInstanceWithId(SingleCardId, TEXT("card.controller.single"),
            EDAAcquisitionSource::StarterDeck, 0);
        SingleCardCampaign.DeckState.SetInstanceIds({SingleCardId});
        SingleCardCampaign.DeckState.DrawForCycle();
        UDAUIViewModelProvider* SingleProvider = NewObject<UDAUIViewModelProvider>();
        SingleProvider->SetAuthoritativeSnapshot(SingleCardCampaign);
        TestTrue("Single record controller cycle selects it",
            SingleProvider->CycleStableSelection(TEXT("city_hud"), 1, Error));
        TestTrue("Cycling the same selected record is an idempotent success",
            SingleProvider->CycleStableSelection(TEXT("city_hud"), 1, Error));
    });
}
