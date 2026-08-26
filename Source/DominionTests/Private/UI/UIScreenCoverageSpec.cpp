#include "Accessibility/DAAccessibilitySettings.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Manifest/DAUIGeneratedMetadata.h"
#include "Manifest/DAUIManifest.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Navigation/DAUIScreenRouter.h"
#include "ViewModels/DAUIViewModels.h"
#include "Widgets/DAGrayboxWidgets.h"

BEGIN_DEFINE_SPEC(FDAUIScreenCoverageSpec, "Dominion.UI.ScreenCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAUIScreenCoverageSpec)

void FDAUIScreenCoverageSpec::Define()
{
    It("uses per-binding payload indices and renders every overlay authority row", [this]()
    {
        FDAUIManifest Manifest; TArray<FText> Errors;
        TestTrue("Manifest loads", FDAUIManifest::LoadCanonical(Manifest, Errors));
        UDAUIScreenRouter* Router = NewObject<UDAUIScreenRouter>();
        TestTrue("Router initializes", Router->Initialize(Manifest, Errors));
        UDAActivatableScreen* City = NewObject<UDACityHUDWidget>();
        City->ConfigureScreen(*Manifest.FindScreen(TEXT("city_hud")), Router);
        FDAUIScreenProjection Projection;
        FDAUIIndexedActionPayloads& Indexed =
            Projection.IndexedActionPayloads.Add(TEXT("city.select_card"));
        Indexed.Values = {
            TEXT("{\"cardInstanceId\":\"00000001-00000002-00000003-00000004\"}"),
            TEXT("{\"cardInstanceId\":\"00000005-00000006-00000007-00000008\"}")};
        City->SetProjection(Projection);
        const FString SecondPayload = City->BuildPayloadForAction(
            TEXT("city.select_card"), 1, false);
        TestTrue("Second binding selects the second authoritative record",
            SecondPayload.Contains(TEXT("00000005-00000006-00000007-00000008")));
        TestTrue("Binding index is consumed once by the payload builder",
            SecondPayload.Contains(TEXT("\"bindingIndex\":1")));
        const FString ControllerSecondPayload = City->BuildPayloadForAction(
            TEXT("city.select_card"), 1, true);
        TestTrue("Controller index one targets the same exact second record",
            ControllerSecondPayload.Contains(TEXT("00000005-00000006-00000007-00000008"))
            && ControllerSecondPayload.Contains(TEXT("\"bindingIndex\":1")));

        UDAOverlayWidget* Overlay = NewObject<UDAOverlayWidget>();
        FDAUIOverlayReadModel Rows; Rows.bAuthoritative = true;
        Rows.RenderedValues = {TEXT("row.one value reason"), TEXT("row.two edge reason")};
        Overlay->ConfigureOverlay(TEXT("employment"), Rows, Router);
        Overlay->TakeWidget();
        TestEqual("Every authoritative overlay record receives a widget row",
            Overlay->GetDataSlotCount(), 2);
    });

    It("loads exactly the 27 frozen source-backed surfaces and eight city overlays", [this]()
    {
        FDAUIManifest Manifest;
        TArray<FText> Errors;
        TestTrue("Canonical UI manifest loads", FDAUIManifest::LoadCanonical(Manifest, Errors));
        TestEqual("Canonical semantic fingerprint is pinned", Manifest.Fingerprint,
            FString(TEXT("bb3699cbe41260b76a64e24ff6204236768f6a5c")));
        FDAUIManifest SemanticMutation = Manifest;
        SemanticMutation.Screens[0].BackTarget = TEXT("settings");
        TArray<FText> MutationErrors;
        TestFalse("Self-consistent runtime semantic mutation is rejected", SemanticMutation.Validate(MutationErrors));
        FDAUIManifest AccessibilityMutation = Manifest;
        AccessibilityMutation.AccessibilityOptions[2].MinimumJson = TEXT("0.5");
        MutationErrors.Reset();
        TestFalse("Frozen accessibility default/range semantics cannot be rewritten",
            AccessibilityMutation.Validate(MutationErrors));
        const TArray<FName> ExpectedScreens = {
            TEXT("main_menu"), TEXT("new_campaign"), TEXT("settings"), TEXT("accessibility"), TEXT("pause"),
            TEXT("city_hud"), TEXT("founder_hud"), TEXT("command_hud"), TEXT("card_collection"),
            TEXT("deck_builder"), TEXT("card_inspect"), TEXT("blueprint_crafting"), TEXT("building_inspect"),
            TEXT("citizen_inspect"), TEXT("faction_panel"), TEXT("diplomacy"), TEXT("treaty_builder"),
            TEXT("world_map"), TEXT("quest_journal"), TEXT("history_timeline"), TEXT("research"),
            TEXT("city_metrics"), TEXT("conquest_dashboard"), TEXT("leader_resolution"),
            TEXT("ascension_reward"), TEXT("save_load"), TEXT("returning_player_recap")};
        const TArray<FName> ExpectedOverlays = {TEXT("power"), TEXT("water"), TEXT("data"),
            TEXT("employment"), TEXT("housing"), TEXT("happiness"), TEXT("dependency"), TEXT("adjacency")};
        TestEqual("Exact screen count", Manifest.Screens.Num(), ExpectedScreens.Num());
        TestEqual("Exact overlay count", Manifest.Overlays.Num(), ExpectedOverlays.Num());
        UDAUIScreenRouter* Router = NewObject<UDAUIScreenRouter>();
        TestTrue("Router initializes for source widget registry", Router->Initialize(Manifest, Errors));
        UDAUIViewModelProvider* SourceProvider = NewObject<UDAUIViewModelProvider>();
        SourceProvider->SetAuthoritativeSnapshot(FDACampaignSnapshot{});
        for (int32 Index = 0; Index < ExpectedScreens.Num() && Index < Manifest.Screens.Num(); ++Index)
        {
            TestEqual("Frozen screen order and ID", Manifest.Screens[Index].Id, ExpectedScreens[Index]);
            UClass* SourceClass = Manifest.Screens[Index].WidgetClass.TryLoadClass<UDAActivatableScreen>();
            TestTrue("Source widget class resolves", SourceClass != nullptr);
            if (SourceClass != nullptr)
            {
                UDAActivatableScreen* Widget = NewObject<UDAActivatableScreen>(GetTransientPackage(), SourceClass);
                Widget->ConfigureScreen(Manifest.Screens[Index], Router);
                FDAUIScreenProjection SourceProjection;
                TestTrue("Source projection builds", SourceProvider->BuildScreen(Manifest.Screens[Index], SourceProjection));
                Widget->SetProjection(SourceProjection);
                Widget->TakeWidget();
                TArray<FName> ExpectedActions = Manifest.Screens[Index].CampaignCriticalActions;
                ExpectedActions.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
                TestTrue("Every manifest action has a functional CommonButton registry entry",
                    Widget->GetRegisteredActionIds() == ExpectedActions);
                TestEqual("Every declared authoritative channel has a concrete data slot",
                    Widget->GetDataSlotCount(), Manifest.Screens[Index].DataChannels.Num());
                TestNotNull("Entry focus resolves to an actual child widget", Widget->GetResolvedFocusWidget());
                for (const FName ActionId : ExpectedActions)
                    TestNotNull("Registered action is a concrete CommonUI button", Widget->FindActionButton(ActionId));
                TestEqual("Source screen renders one real value per authoritative channel",
                    Widget->GetRenderedDataValues().Num(), Manifest.Screens[Index].DataChannels.Num());
                if (Manifest.Screens[Index].Id == TEXT("accessibility"))
                {
                    TestEqual("Accessibility screen constructs every frozen option control",
                        Widget->GetAccessibilityOptionControlCount(), Manifest.AccessibilityOptions.Num());
                    TestTrue("Concrete subtitle option control mutates the draft",
                        Widget->CycleAccessibilityOption(TEXT("subtitles")));
                    TestTrue("Accessibility apply payload contains the changed concrete value",
                        Widget->BuildPayloadForAction(TEXT("accessibility.apply")).Contains(TEXT("\"subtitles\":false")));
                }
                for (const FString& Value : Widget->GetRenderedDataValues())
                    TestFalse("Rendered values are not raw channel identifiers",
                        Manifest.Screens[Index].DataChannels.Contains(FName(*Value)));
            }
        }
        for (int32 Index = 0; Index < ExpectedOverlays.Num() && Index < Manifest.Overlays.Num(); ++Index)
            TestEqual("Frozen overlay order and ID", Manifest.Overlays[Index].Id, ExpectedOverlays[Index]);
    });

    It("rejects duplicate JSON object keys and clears caller error state before every load", [this]()
    {
        const FString DuplicatePath = FPaths::CreateTempFilename(
            *FPaths::ProjectSavedDir(), TEXT("DuplicateUIManifest"), TEXT(".json"));
        TestTrue("Duplicate fixture writes", FFileHelper::SaveStringToFile(
            TEXT("{\"schemaVersion\":1,\"schemaVersion\":1}"), *DuplicatePath));
        FDAUIManifest Manifest; TArray<FText> Errors = {FText::FromString(TEXT("stale"))};
        TestFalse("Duplicate keys reject before DTO parsing", FDAUIManifest::LoadFile(DuplicatePath, Manifest, Errors));
        TestTrue("Load replaces stale caller errors with the duplicate-key diagnostic",
            Errors.Num() == 1 && !Errors[0].ToString().Contains(TEXT("stale")));
        IFileManager::Get().Delete(*DuplicatePath);

        const FString MalformedPath = FPaths::CreateTempFilename(
            *FPaths::ProjectSavedDir(), TEXT("MalformedUIManifest"), TEXT(".json"));
        TestTrue("Malformed strict-shape fixture writes", FFileHelper::SaveStringToFile(
            TEXT("{\"schemaVersion\":1}"), *MalformedPath));
        Errors.Reset();
        TestFalse("Malformed strict shape rejects", FDAUIManifest::LoadFile(MalformedPath, Manifest, Errors));
        TestTrue("Every rejected manifest reports a clear diagnostic", !Errors.IsEmpty());
        IFileManager::Get().Delete(*MalformedPath);
    });

    It("builds every frozen keyboard/controller route as real Enhanced Input mappings", [this]()
    {
        FDAUIManifest Manifest; TArray<FText> Errors;
        TestTrue("Canonical UI manifest loads", FDAUIManifest::LoadCanonical(Manifest, Errors));
        for (const FDAUICampaignActionDescriptor& Descriptor : Manifest.CampaignCriticalActions)
        {
            UInputAction* Action = NewObject<UInputAction>();
            UInputMappingContext* Context = NewObject<UInputMappingContext>();
            FString Error; TArray<FKey> Keyboard; TArray<FKey> Controller;
            TestTrue("Frozen tokens resolve to typed keyboard/controller keys",
                FDAUIGeneratedCache::GetExpectedInputKeys(Descriptor, Keyboard, Controller, Error));
            TestTrue("Generator mapping helper populates real Enhanced Input objects",
                FDAUIGeneratedCache::MapFrozenAction(*Context, *Action, Descriptor, Error));
            const bool bDirectional = Descriptor.KeyboardInput == TEXT("ArrowLeftRight")
                || Descriptor.ControllerInput == TEXT("DPadLeftRight")
                || Descriptor.ControllerInput == TEXT("RightStick");
            TestEqual("Directional actions use Axis1D and all others use Boolean",
                Action->ValueType, bDirectional ? EInputActionValueType::Axis1D : EInputActionValueType::Boolean);
            TSet<FKey> ActualKeys;
            for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
                if (Mapping.Action == Action) ActualKeys.Add(Mapping.Key);
            for (const FKey Key : Keyboard)
            {
                TestFalse("Keyboard route is not a gamepad key", Key.IsGamepadKey());
                TestTrue("Real mapping contains the keyboard route", ActualKeys.Contains(Key));
            }
            for (const FKey Key : Controller)
            {
                TestTrue("Controller route is a gamepad key", Key.IsGamepadKey());
                TestTrue("Real mapping contains the controller route", ActualKeys.Contains(Key));
            }
            TSet<FKey> ExpectedKeys;
            for (const FKey Key : Keyboard) ExpectedKeys.Add(Key);
            for (const FKey Key : Controller) ExpectedKeys.Add(Key);
            TestEqual("Real mapping contains only the exact frozen routes", ActualKeys.Num(), ExpectedKeys.Num());
            if (Descriptor.KeyboardInput == TEXT("ArrowLeftRight")
                || Descriptor.ControllerInput == TEXT("DPadLeftRight"))
                TestTrue("Negative directional route has an explicit modifier",
                    Context->GetMappings().ContainsByPredicate([](const FEnhancedActionKeyMapping& Mapping)
                        { return (Mapping.Key == EKeys::Left || Mapping.Key == EKeys::Gamepad_DPad_Left)
                            && !Mapping.Modifiers.IsEmpty(); }));
            const TArray<FDAUIInputBindingDescriptor> Bindings =
                FDAUIGeneratedCache::GetBindingDescriptors(Descriptor, Error);
            TestEqual("Each frozen key has its own payload-preserving input action",
                Bindings.Num(), Keyboard.Num() + Controller.Num());
            TSet<FSoftObjectPath> BindingPaths;
            for (const FDAUIInputBindingDescriptor& Binding : Bindings)
            {
                BindingPaths.Add(Binding.AssetPath);
                UInputAction* PerKeyAction = NewObject<UInputAction>();
                UInputMappingContext* PerKeyContext = NewObject<UInputMappingContext>();
                TestTrue("Per-key descriptor builds one exact callback-preserving mapping",
                    FDAUIGeneratedCache::MapFrozenBinding(
                        *PerKeyContext, *PerKeyAction, Descriptor, Binding, Error));
                TestEqual("Per-key mapping contains no extra keys", PerKeyContext->GetMappings().Num(), 1);
                if (PerKeyContext->GetMappings().Num() == 1)
                    TestEqual("Per-key mapping preserves its exact frozen key",
                        PerKeyContext->GetMappings()[0].Key, Binding.Key);
                TestFalse("Per-key semantic hash is nonempty",
                    FDAUIGeneratedCache::BindingSemanticHash(Descriptor, Binding).IsEmpty());
            }
            TestEqual("Per-key input action paths are unique", BindingPaths.Num(), Bindings.Num());
            if (Descriptor.KeyboardInput == TEXT("1-0"))
            {
                TestEqual("Numeric route exposes ten keyboard payload indices plus controller activation", Bindings.Num(), 11);
                for (int32 NumberIndex = 0; NumberIndex < 10; ++NumberIndex)
                    TestEqual("Numeric payload index is deterministic", Bindings[NumberIndex].PayloadIndex, NumberIndex);
            }
        }
    });

    It("builds typed HUD and reason-ledger projections from immutable authoritative records", [this]()
    {
        FDACampaignSnapshot Campaign;
        Campaign.LiveSignals.Capital = 14.25;
        Campaign.LiveSignals.Insight = 3.5;
        Campaign.LiveSignals.Influence = 2.0;
        Campaign.LiveSignals.Population = 24;
        Campaign.WorldState.CurrentWorldTick = 3;
        Campaign.WorldState.ClockAuthority.bCaptured = true;
        Campaign.WorldState.ClockAuthority.CurrentWorldTick = 3;
        Campaign.WorldState.ClockAuthority.CurrentDevelopmentCycle = 17;
        FDACampaignCitizenSignal SelectedCitizen; SelectedCitizen.CitizenId = TEXT("citizen.test.selected");
        Campaign.LiveSignals.Citizens.Add(SelectedCitizen);
        Campaign.SynaraState.ApplyDependencyReason(TEXT("reason.test.automation"), 7.0, 1);
        FDAQuestSaveState ActiveQuest;
        ActiveQuest.QuestId = TEXT("quest.test.objective");
        ActiveQuest.CurrentNodeId = TEXT("build_habitat");
        ActiveQuest.ProgressState = EDAQuestProgressState::Active;
        Campaign.NarrativeState.QuestStates.Add(ActiveQuest);

        const FDACityHUDViewModel City = FDAUIViewModelFactory::BuildCityHUD(Campaign);
        TestEqual("Capital is projected without UI recomputation", City.Wallets.Capital, 14.25);
        TestEqual("Population is projected", City.Population, 24);
        TestEqual("City HUD carries canonical World Tick authority", City.CurrentWorldTick, 3LL);
        TestEqual("City HUD carries canonical Development Cycle authority", City.CurrentDevelopmentCycle, 17LL);
        TestEqual("Dependency is projected", City.Dependency, 7.0);
        TestEqual("Dependency reasons are preserved", City.DependencyReasons.Num(), 1);
        TestEqual("Current objective is typed", City.ObjectiveId, FName(TEXT("quest.test.objective.build_habitat")));

        FDADiplomaticRelationship Relationship;
        Relationship.RelationshipId = TEXT("relationship.synara.forgeweave");
        FDADiplomaticReason Reason;
        Reason.MutationId = TEXT("reason.trade.test"); Reason.SourceTag = TEXT("trade.delivery");
        Reason.Metric = EDADiplomaticMetric::Trust; Reason.Magnitude = 2.f; Reason.WorldTick = 1;
        Relationship.Trust = 2.f; Relationship.ReasonLedger.Add(Reason);
        Campaign.WorldState.Diplomacy.Relationships.Add(Relationship);
        const FDADiplomacyPanelViewModel Diplomacy = FDAUIViewModelFactory::BuildDiplomacy(Campaign);
        TestEqual("Diplomacy exposes the authoritative reason ledger", Diplomacy.Relationships[0].Reasons.Num(), 1);

        FDACaptureRecord Capture; Capture.History = {TEXT("reason.conquest.test")};
        Campaign.OperationConflict.CaptureRecords.Add(Capture);
        UDAUIViewModelProvider* Provider = NewObject<UDAUIViewModelProvider>();
        int32 ProjectionRefreshes = 0;
        Provider->OnProjectionInvalidated.AddLambda([&ProjectionRefreshes]() { ++ProjectionRefreshes; });
        FDAFounderHUDSource FounderSource; FounderSource.Health = 81.f; FounderSource.InteractionId = TEXT("interact.test");
        FDACommandHUDSource CommandSource; CommandSource.CommandPoints = 4; CommandSource.Sovereignty = 0.6f;
        Provider->SetFounderHUDSource(FounderSource); Provider->SetCommandHUDSource(CommandSource);
        Provider->SetStableSelection(TEXT("citizen_inspect"), TEXT("citizen.test.selected"));
        const int32 BeforeCampaignPublication = ProjectionRefreshes;
        Provider->SetAuthoritativeSnapshot(Campaign);
        TestEqual("Authoritative campaign publication invalidates active projections", ProjectionRefreshes,
            BeforeCampaignPublication + 1);
        FDAUIManifest Manifest; TArray<FText> Errors;
        TestTrue("Manifest loads for provider coverage", FDAUIManifest::LoadCanonical(Manifest, Errors));
        for (const FDAUIScreenDescriptor& Screen : Manifest.Screens)
        {
            FDAUIScreenProjection Projection;
            TestTrue("Every frozen screen receives a typed authoritative projection",
                Provider->BuildScreen(Screen, Projection));
            const bool bLateAuthority = Screen.Id == TEXT("research")
                || Screen.Id == TEXT("conquest_dashboard")
                || Screen.Id == TEXT("leader_resolution")
                || Screen.Id == TEXT("ascension_reward");
            const bool bNeedsRegisteredLiveHUD = Screen.Id == TEXT("founder_hud")
                || Screen.Id == TEXT("command_hud");
            if (bLateAuthority || bNeedsRegisteredLiveHUD)
            {
                TestFalse("Unregistered typed service cannot masquerade as authority",
                    Projection.bAuthoritative);
                if (bLateAuthority)
                    TestTrue("Unavailable later-task projection renders an explicit diagnostic",
                        !Projection.RenderedValues.IsEmpty()
                        && Projection.RenderedValues[0].Contains(TEXT("ServiceUnavailable")));
            }
            else TestTrue("Projection is marked authoritative", Projection.bAuthoritative);
            TestTrue("Projection preserves exact declared channels", Projection.DataChannels == Screen.DataChannels);
            if (Screen.Id == TEXT("founder_hud"))
            {
                TestEqual("Founder HUD uses the authoritative typed source", Projection.FounderHUD.Founder.Health, 81.f);
                TestTrue("Founder health channel renders the typed health rather than a generic revision",
                    Projection.RenderedValues[0].Contains(TEXT("81")));
            }
            if (Screen.Id == TEXT("command_hud"))
            {
                TestEqual("Command HUD uses the authoritative typed source", Projection.CommandHUD.Command.CommandPoints, 4);
                TestTrue("Command points channel renders the typed CP value",
                    Projection.RenderedValues[5].Contains(TEXT("4")));
            }
            if (Screen.Id == TEXT("citizen_inspect"))
                TestEqual("Inspect screens resolve the caller's stable selected ID",
                    Projection.Citizen.Citizen.CitizenId, FName(TEXT("citizen.test.selected")));
        }
        const FDAConquestDashboardViewModel Conquest = FDAUIViewModelFactory::BuildConquest(Campaign);
        TestTrue("Conquest exposes outcome reasons", Conquest.OutcomeReasons.Contains(TEXT("reason.conquest.test")));
        TSet<EDAUIOverlayKind> OverlayKinds;
        for (const FDAUIOverlayDescriptor& Overlay : Manifest.Overlays)
        {
            FDAUIOverlayReadModel ReadModel;
            TestTrue("Every frozen overlay receives an authoritative projection",
                Provider->BuildOverlay(Overlay, true, ReadModel));
            TestTrue("Overlay retains color-independent marker policy", ReadModel.bColorIndependentMarkers);
            TestEqual("Overlay marker shape is canonical", ReadModel.MarkerShape, Overlay.MarkerShape);
            OverlayKinds.Add(ReadModel.Kind);
        }
        TestEqual("Eight overlays use eight distinct authoritative model kinds", OverlayKinds.Num(), 8);

        FDACampaignCitizenSignal PayloadCitizen; PayloadCitizen.CitizenId = TEXT("citizen.payload.stable");
        Campaign.LiveSignals.Citizens = {PayloadCitizen};
        FDAWorldAssetRecord PayloadAsset; PayloadAsset.WorldAssetId = FGuid(1, 2, 3, 4);
        Campaign.WorldAssets = {PayloadAsset};
        FDACaptureRecord PayloadRoute; PayloadRoute.WorldAssetId = FGuid(5, 6, 7, 8);
        Campaign.OperationConflict.CaptureRecords = {PayloadRoute};
        Provider->SetStableSelection(TEXT("citizen_inspect"), PayloadCitizen.CitizenId);
        Provider->SetAuthoritativeSnapshot(Campaign);
        FDAUIScreenProjection CitizenProjection;
        TestTrue("Citizen payload projection builds", Provider->BuildScreen(
            *Manifest.FindScreen(TEXT("citizen_inspect")), CitizenProjection));
        TestTrue("A nonempty citizen selection is never overwritten by a building ID",
            CitizenProjection.ActionPayloads[FName(TEXT("citizen.assign_job"))].Contains(TEXT("citizen.payload.stable")));
        FDAUIScreenProjection ConquestProjection;
        TestTrue("Conquest payload projection builds", Provider->BuildScreen(
            *Manifest.FindScreen(TEXT("conquest_dashboard")), ConquestProjection));
        TestFalse("Campaign conflict records cannot impersonate the unregistered conquest service",
            ConquestProjection.bAuthoritative);
        TestFalse("Unavailable conquest payload does not synthesize a route asset ID",
            ConquestProjection.ActionPayloads[FName(TEXT("conquest.choose_route"))].Contains(
                PayloadRoute.WorldAssetId.ToString(EGuidFormats::DigitsWithHyphens)));
        const int32 BeforeSetterRefresh = ProjectionRefreshes;
        FounderSource.Health = 63.f;
        Provider->SetFounderHUDSource(FounderSource);
        TestEqual("Runtime founder-source setters invalidate live projections", ProjectionRefreshes,
            BeforeSetterRefresh + 1);
    });

    It("validates every frozen accessibility setting and rejects out-of-range persisted state", [this]()
    {
        FDAAccessibilitySettings Settings = FDAAccessibilitySettings::MakeDefaults();
        FString Error;
        TestTrue("Frozen defaults validate", Settings.Validate(Error));
        TestEqual("All hold-toggle actions are explicit", Settings.HoldToggleModes.Num(), 5);
        Settings.TextScale = 2.01f;
        TestFalse("Out-of-range text scaling is rejected", Settings.Validate(Error));
        Settings = FDAAccessibilitySettings::MakeDefaults();
        Settings.bReducedMotion = true;
        Settings.bReducedFlash = true;
        const FDAUIEffectsPolicy Effects = FDAUIEffectsPolicy::FromSettings(Settings);
        TestFalse("Reduced motion disables nonessential motion", Effects.bAllowNonEssentialMotion);
        TestFalse("Reduced flash disables full-intensity flash", Effects.bAllowFullIntensityFlash);
        TestEqual("Reduced motion suppresses camera shake", Effects.CameraShakeScale, 0.f);
        const FDAAccessibilityRuntimePolicy Policy = FDAAccessibilityRuntimePolicy::FromSettings(Settings);
        TestEqual("Typed policy reads text scale", Policy.EffectiveTextScale, Settings.TextScale);
        TestEqual("Typed policy reads FOV", Policy.EffectiveFieldOfView, Settings.FieldOfView);
        TestEqual("Typed policy reads subtitle state", Policy.bRenderSubtitles, Settings.bSubtitles);
        TestEqual("Typed policy reads speaker labels", Policy.bRenderSpeakerLabels, Settings.bSpeakerLabels);
        TestEqual("Typed policy reads marker independence", Policy.bUseColorIndependentMarkers,
            Settings.bColorIndependentMarkers);
        TestEqual("Typed policy preserves color-vision preset", Policy.Settings.ColorVisionPreset,
            Settings.ColorVisionPreset);
        TestEqual("Typed policy preserves subtitle background", Policy.Settings.SubtitleBackgroundOpacity,
            Settings.SubtitleBackgroundOpacity);
        TestEqual("Typed policy preserves camera shake", Policy.Settings.CameraShake, Settings.CameraShake);
        TestEqual("Typed policy preserves keyboard map", Policy.Settings.KeyboardBindings.Num(),
            Settings.KeyboardBindings.Num());
        TestEqual("Typed policy preserves controller map", Policy.Settings.ControllerBindings.Num(),
            Settings.ControllerBindings.Num());
        TestFalse("Reduced motion suppresses motion blur", Policy.bAllowMotionBlur);
        TestTrue("Reduced flash is exposed to typed consumers", Policy.bAllowReducedFlashOnly);
        TestEqual("Typed policy preserves hold/toggle readback", Policy.Settings.HoldToggleModes.Num(), 5);
        TestEqual("Typed policy preserves aim assist", Policy.Settings.AimAssist, Settings.AimAssist);
        TestEqual("Typed policy preserves build snap", Policy.Settings.BuildSnapStrength, Settings.BuildSnapStrength);
        TestEqual("Typed policy preserves Tactical Pause", Policy.Settings.bTacticalPause, Settings.bTacticalPause);
        TestEqual("Typed policy preserves tutorial recall", Policy.Settings.bTutorialRecall, Settings.bTutorialRecall);
        TestEqual("Typed policy preserves tooltip mode", Policy.Settings.TooltipMode, Settings.TooltipMode);
        UDAUIVisualAccessibilityAdapter* VisualAdapter = NewObject<UDAUIVisualAccessibilityAdapter>();
        UDACameraAccessibilityAdapter* CameraAdapter = NewObject<UDACameraAccessibilityAdapter>();
        UDAInputAccessibilityAdapter* InputAdapter = NewObject<UDAInputAccessibilityAdapter>();
        UDAGameplayAccessibilityAdapter* GameplayAdapter = NewObject<UDAGameplayAccessibilityAdapter>();
        UDATutorialAccessibilityAdapter* TutorialAdapter = NewObject<UDATutorialAccessibilityAdapter>();
        VisualAdapter->ApplyAccessibilityPolicy(Policy); CameraAdapter->ApplyAccessibilityPolicy(Policy);
        InputAdapter->ApplyAccessibilityPolicy(Policy); GameplayAdapter->ApplyAccessibilityPolicy(Policy);
        TutorialAdapter->ApplyAccessibilityPolicy(Policy);
        TestEqual("UI adapter owns subtitle readback", VisualAdapter->GetPolicy().bRenderSubtitles, Settings.bSubtitles);
        TestEqual("Camera adapter owns FOV readback", CameraAdapter->GetPolicy().EffectiveFieldOfView, Settings.FieldOfView);
        TestEqual("Input adapter owns hold/toggle readback", InputAdapter->GetPolicy().Settings.HoldToggleModes.Num(), 5);
        TestEqual("Gameplay adapter owns assist readback", GameplayAdapter->GetPolicy().Settings.AimAssist, Settings.AimAssist);
        TestEqual("Tutorial adapter owns tooltip readback", TutorialAdapter->GetPolicy().Settings.TooltipMode, Settings.TooltipMode);
        TestEqual("Slate adapter applies text scale", VisualAdapter->GetAppliedTextScale(), Settings.TextScale);
        TestTrue("Subtitle adapter applies speaker labels", VisualAdapter->ShouldRenderSpeakerLabels());
        TestFalse("Post-process adapter suppresses motion blur", CameraAdapter->ShouldAllowMotionBlur());
        TestEqual("Camera adapter applies shake suppression", CameraAdapter->GetAppliedShakeScale(), 0.f);
        TestEqual("Input adapter applies direct Press semantics to placement",
            InputAdapter->GetAppliedMode(TEXT("city.place_building")), EDAHoldToggleMode::Press);
        TestTrue("First direct placement press is consumed",
            InputAdapter->ConsumeAccessibleAction(TEXT("city.place_building"), true));
        TestTrue("Second direct placement press is consumed",
            InputAdapter->ConsumeAccessibleAction(TEXT("city.place_building"), true));
        FDAAccessibilitySettings LegacyDirectSettings = Settings;
        LegacyDirectSettings.HoldToggleModes.Add(TEXT("city.place_building"), EDAHoldToggleMode::Toggle);
        InputAdapter->ApplyAccessibilityPolicy(FDAAccessibilityRuntimePolicy::FromSettings(LegacyDirectSettings));
        TestTrue("Persisted legacy Toggle cannot suppress a first direct placement",
            InputAdapter->ConsumeAccessibleAction(TEXT("city.place_building"), true));
        TestTrue("Persisted legacy Toggle cannot suppress a second direct placement",
            InputAdapter->ConsumeAccessibleAction(TEXT("city.place_building"), true));
        FDAAccessibilitySettings ToggleSettings = Settings;
        ToggleSettings.HoldToggleModes.Add(TEXT("ui.tactical_pause"), EDAHoldToggleMode::Toggle);
        TestTrue("Genuinely stateful toggle setting validates", ToggleSettings.Validate(Error));
        InputAdapter->ApplyAccessibilityPolicy(FDAAccessibilityRuntimePolicy::FromSettings(ToggleSettings));
        TestTrue("First stateful toggle press enables its persisted state",
            InputAdapter->ConsumeAccessibleAction(TEXT("ui.tactical_pause"), true));
        TestTrue("Released stateful toggle retains its state",
            InputAdapter->ConsumeAccessibleAction(TEXT("ui.tactical_pause"), false));
        TestFalse("Second stateful toggle press disables its persisted state",
            InputAdapter->ConsumeAccessibleAction(TEXT("ui.tactical_pause"), true));
        TestTrue("Hold actions directly consume their trigger state",
            InputAdapter->ConsumeAccessibleAction(TEXT("command.issue_order"), true));
        TestFalse("Released hold action is inactive",
            InputAdapter->ConsumeAccessibleAction(TEXT("command.issue_order"), false));
        TestTrue("Press actions dispatch their trigger pulse",
            InputAdapter->ConsumeAccessibleAction(TEXT("founder.activate_ability"), true));
        TestFalse("Press actions do not remain active after release",
            InputAdapter->ConsumeAccessibleAction(TEXT("founder.activate_ability"), false));
        FDAUIManifest Manifest;
        TArray<FText> ManifestErrors;
        TestTrue("Canonical input graph loads", FDAUIManifest::LoadCanonical(Manifest, ManifestErrors));
        const FDAUIScreenDescriptor* CommandScreen = Manifest.FindScreen(TEXT("command_hud"));
        const FDAUICampaignActionDescriptor* CommandAction = Manifest.FindAction(TEXT("command.issue_order"));
        FString MappingError;
        const TArray<FDAUIInputBindingDescriptor> CommandBindings = CommandAction == nullptr
            ? TArray<FDAUIInputBindingDescriptor>{}
            : FDAUIGeneratedCache::GetBindingDescriptors(*CommandAction, MappingError);
        UInputMappingContext* FrozenContext = NewObject<UInputMappingContext>();
        if (CommandScreen != nullptr && !CommandBindings.IsEmpty())
        {
            const FString RuntimeName = CommandBindings[0].BindingId.ToString().Replace(TEXT("."), TEXT("_"));
            UInputAction* RuntimeAction = NewObject<UInputAction>(FrozenContext, FName(*RuntimeName));
            FrozenContext->MapKey(RuntimeAction, CommandBindings[0].Key);
            UInputMappingContext* PlayerContext = InputAdapter->BuildPlayerMappingContext(
                *FrozenContext, Manifest, *CommandScreen, MappingError);
            TestTrue("Persisted stable action ID resolves the runtime binding object name",
                PlayerContext != nullptr && PlayerContext->GetMappings().Num() == 1);
            if (PlayerContext != nullptr && PlayerContext->GetMappings().Num() == 1)
                TestTrue("Canonical Hold mode installs a one-shot real hold trigger",
                    PlayerContext->GetMappings()[0].Triggers.ContainsByPredicate(
                        [](const TObjectPtr<UInputTrigger>& Trigger)
                        { const UInputTriggerHold* Hold = Cast<UInputTriggerHold>(Trigger.Get()); return Hold != nullptr && Hold->bIsOneShot; }));
        }
        TestTrue("Gameplay adapter applies Tactical Pause", GameplayAdapter->IsTacticalPauseEnabled());
        TestEqual("Gameplay adapter applies build snap", GameplayAdapter->GetBuildSnapStrength(), Settings.BuildSnapStrength);
        TestTrue("Tutorial adapter applies recall", TutorialAdapter->IsRecallEnabled());
        Settings = FDAAccessibilitySettings::MakeDefaults();
        Settings.HoldToggleModes.Remove(TEXT("command.issue_order"));
        TestFalse("Missing hold-toggle state is rejected", Settings.Validate(Error));
        Settings = FDAAccessibilitySettings::MakeDefaults();
        Settings.TooltipMode = static_cast<EDATooltipMode>(255);
        TestFalse("Out-of-range tooltip mode is rejected", Settings.Validate(Error));
        Settings = FDAAccessibilitySettings::MakeDefaults();
        Settings.KeyboardBindings.Add(TEXT("city.place_building"), EKeys::Gamepad_FaceButton_Bottom);
        TestFalse("Keyboard remaps reject controller keys", Settings.Validate(Error));
        Settings = FDAAccessibilitySettings::MakeDefaults();
        Settings.ControllerBindings.Add(TEXT("city.place_building"), EKeys::SpaceBar);
        TestFalse("Controller remaps reject keyboard keys", Settings.Validate(Error));
    });
}
