#include "Subsystems/DAUISubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "AbilitySystemComponent.h"
#include "Combat/DACombatAttributeSet.h"
#include "Combat/DAFounderAbilityDefinitions.h"
#include "Command/DACommandSubsystem.h"
#include "Components/Overlay.h"
#include "Commands/DAUICommandEndpoint.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Manifest/DAUIGeneratedMetadata.h"
#include "Misc/Paths.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Widgets/DAGrayboxWidgets.h"

TSharedRef<SWidget> UDARootUILayout::RebuildWidget()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DominionUILayers"));
    HUDStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(
        UCommonActivatableWidgetStack::StaticClass(), TEXT("HUDLayer"));
    MenuStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(
        UCommonActivatableWidgetStack::StaticClass(), TEXT("MenuLayer"));
    PanelStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(
        UCommonActivatableWidgetStack::StaticClass(), TEXT("PanelLayer"));
    ModalStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(
        UCommonActivatableWidgetStack::StaticClass(), TEXT("ModalLayer"));
    Root->AddChildToOverlay(HUDStack); Root->AddChildToOverlay(MenuStack);
    Root->AddChildToOverlay(PanelStack); Root->AddChildToOverlay(ModalStack);
    WidgetTree->RootWidget = Root;
    return Root->TakeWidget();
}

UCommonActivatableWidgetStack* UDARootUILayout::GetLayerStack(const EDAUIScreenLayer Layer) const
{
    switch (Layer)
    {
    case EDAUIScreenLayer::Menu: return MenuStack;
    case EDAUIScreenLayer::HUD: return HUDStack;
    case EDAUIScreenLayer::Panel: return PanelStack;
    case EDAUIScreenLayer::Modal: return ModalStack;
    default: return nullptr;
    }
}

void UDAUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Router = NewObject<UDAUIScreenRouter>(this);
    CommandService = NewObject<UDAUICommandService>(this);
    TWeakObjectPtr<UDAUISubsystem> WeakThis(this);
    CommandService->SetNativeExecutor([WeakThis](const FDAUICommandRequest& Request, FString& OutError)
    {
        UDAUISubsystem* StrongThis = WeakThis.Get();
        if (StrongThis == nullptr)
        { OutError = TEXT("Production UI command owner is no longer valid."); return false; }
        return StrongThis->DispatchDefaultCommand(Request, OutError);
    });
    ViewModelProvider = NewObject<UDAUIViewModelProvider>(this);
    VisualAccessibility = NewObject<UDAUIVisualAccessibilityAdapter>(this);
    CameraAccessibility = NewObject<UDACameraAccessibilityAdapter>(this);
    InputAccessibility = NewObject<UDAInputAccessibilityAdapter>(this);
    GameplayAccessibility = NewObject<UDAGameplayAccessibilityAdapter>(this);
    TutorialAccessibility = NewObject<UDATutorialAccessibilityAdapter>(this);
    TArray<FText> Errors;
    if (Router->InitializeCanonical(Errors))
    {
        Router->SetCommandService(CommandService);
        Router->SetViewModelProvider(ViewModelProvider);
        Router->SetInputAccessibilityAdapter(InputAccessibility);
    }
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        PlayerControllerChangedHandle = LocalPlayer->OnPlayerControllerChanged().AddUObject(
            this, &UDAUISubsystem::HandlePlayerControllerChanged);
        if (UGameInstance* GameInstance = LocalPlayer->GetGameInstance())
        {
            FString EndpointError;
            SetGameplayCommandEndpoint(
                GameInstance->GetSubsystem<UDAUICommandEndpointSubsystem>(), EndpointError);
            ViewModelProvider->BindAuthoritativeWorld(GameInstance->GetSubsystem<UDAWorldStateSubsystem>());
            if (UDAAccessibilitySettingsSubsystem* Accessibility =
                GameInstance->GetSubsystem<UDAAccessibilitySettingsSubsystem>())
            {
                FString Error;
                Accessibility->RegisterConsumer(VisualAccessibility, Error);
                Accessibility->RegisterConsumer(CameraAccessibility, Error);
                Accessibility->RegisterConsumer(InputAccessibility, Error);
                Accessibility->RegisterConsumer(GameplayAccessibility, Error);
                Accessibility->RegisterConsumer(TutorialAccessibility, Error);
                AccessibilityChangedHandle = Accessibility->OnSettingsChanged.AddUObject(
                    this, &UDAUISubsystem::HandleAccessibilityPolicyChanged);
            }
        }
        if (APlayerController* ExistingController = LocalPlayer->GetPlayerController(GetWorld()))
            HandlePlayerControllerChanged(ExistingController);
    }
}

void UDAUISubsystem::Deinitialize()
{
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        if (PlayerControllerChangedHandle.IsValid())
            LocalPlayer->OnPlayerControllerChanged().Remove(PlayerControllerChangedHandle);
    PlayerControllerChangedHandle.Reset();
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        if (UGameInstance* GameInstance = LocalPlayer->GetGameInstance())
            if (UDAAccessibilitySettingsSubsystem* Accessibility =
                GameInstance->GetSubsystem<UDAAccessibilitySettingsSubsystem>())
                if (AccessibilityChangedHandle.IsValid())
                    Accessibility->OnSettingsChanged.Remove(AccessibilityChangedHandle);
    AccessibilityChangedHandle.Reset();
    TeardownPlayerUI();
    if (ViewModelProvider != nullptr) ViewModelProvider->BindAuthoritativeWorld(nullptr);
    TutorialAccessibility = nullptr; GameplayAccessibility = nullptr; InputAccessibility = nullptr;
    CameraAccessibility = nullptr; VisualAccessibility = nullptr;
    GameplayCommandEndpoint.Reset(); ViewModelProvider = nullptr; CommandService = nullptr; Router = nullptr;
    Super::Deinitialize();
}

bool UDAUISubsystem::InitializePlayerUI(APlayerController* PlayerController, FString& OutError)
{
    if (PlayerController == nullptr)
    { OutError = TEXT("Production UI requires a valid player controller."); return false; }
    UDARootUILayout* Created = CreateWidget<UDARootUILayout>(PlayerController, UDARootUILayout::StaticClass());
    if (Created == nullptr) { OutError = TEXT("Could not create the production CommonUI root layout."); return false; }
    Created->AddToPlayerScreen(1000);
    if (AttachRootLayout(Created, PlayerController, OutError)) return true;
    Created->RemoveFromParent();
    return false;
}

bool UDAUISubsystem::AttachRootLayout(
    UDARootUILayout* InRootLayout, APlayerController* PlayerController, FString& OutError)
{
    if (Router == nullptr || InRootLayout == nullptr || PlayerController == nullptr
        || PlayerController->GetLocalPlayer() != GetLocalPlayer())
    { OutError = TEXT("Root layout must belong to this initialized local player."); return false; }
    TeardownPlayerUI();
    if (!ResetRouter(OutError)) return false;
    InRootLayout->TakeWidget();
    for (const EDAUIScreenLayer Layer : {EDAUIScreenLayer::Menu, EDAUIScreenLayer::HUD,
        EDAUIScreenLayer::Panel, EDAUIScreenLayer::Modal})
    {
        UCommonActivatableWidgetStack* Stack = InRootLayout->GetLayerStack(Layer);
        if (Stack == nullptr)
        {
            Router->Shutdown();
            OutError = TEXT("Root layout is missing a required CommonUI layer stack.");
            return false;
        }
        Router->RegisterLayerStack(Layer, Stack);
    }
    UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerController->InputComponent);
    if (Enhanced == nullptr)
    {
        Router->Shutdown();
        OutError = TEXT("Production UI requires an Enhanced Input component.");
        return false;
    }
    if (!BindEnhancedInput(*Enhanced, OutError))
    {
        Router->Shutdown();
        return false;
    }
    RootLayout = InRootLayout; BoundPlayerController = PlayerController;
    CameraAccessibility->BindPlayerController(PlayerController);
    Router->BindPlayerContext(GetLocalPlayer(), PlayerController);
    ViewModelProvider->BindRuntimePlayer(PlayerController);
    if (!ViewModelProvider->RegisterFounderHUDLiveSource(this, OutError)
        || !ViewModelProvider->RegisterCommandHUDLiveSource(this, OutError))
    { TeardownPlayerUI(); return false; }
    if (!Router->EnterScreen(TEXT("city_hud")))
    {
        TeardownPlayerUI();
        OutError = TEXT("Production UI could not enter the canonical initial City HUD.");
        return false;
    }
    OutError.Reset(); return true;
}

bool UDAUISubsystem::ResetRouter(FString& OutError)
{
    if (Router == nullptr || CommandService == nullptr || ViewModelProvider == nullptr)
    { OutError = TEXT("Production UI services are not initialized."); return false; }
    Router->Shutdown();
    TArray<FText> Errors;
    if (!Router->InitializeCanonical(Errors))
    {
        OutError = Errors.IsEmpty() ? TEXT("Canonical UI manifest could not initialize.")
            : Errors[0].ToString();
        return false;
    }
    Router->SetCommandService(CommandService);
    Router->SetViewModelProvider(ViewModelProvider);
    Router->SetInputAccessibilityAdapter(InputAccessibility);
    OutError.Reset();
    return true;
}

void UDAUISubsystem::TeardownPlayerUI()
{
    if (ViewModelProvider != nullptr)
    {
        ViewModelProvider->UnregisterFounderHUDLiveSource(this);
        ViewModelProvider->UnregisterCommandHUDLiveSource(this);
    }
    if (BoundPlayerController != nullptr)
        if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(BoundPlayerController->InputComponent))
            for (const uint32 Handle : InputBindingHandles) Enhanced->RemoveBindingByHandle(Handle);
    InputBindingHandles.Reset();
    if (Router != nullptr) Router->Shutdown();
    RuntimeInputContexts.Reset(); RuntimeInputActions.Reset();
    if (RootLayout != nullptr) RootLayout->RemoveFromParent();
    if (ViewModelProvider != nullptr) ViewModelProvider->BindRuntimePlayer(nullptr);
    if (CameraAccessibility != nullptr) CameraAccessibility->BindPlayerController(nullptr);
    BoundPlayerController = nullptr;
    RootLayout = nullptr;
}

bool UDAUISubsystem::CaptureFounderHUDState(FDAFounderHUDSource& OutState, FString& OutError) const
{
    APlayerController* Controller = BoundPlayerController;
    APawn* Pawn = Controller == nullptr ? nullptr : Controller->GetPawn();
    const UAbilitySystemComponent* AbilitySystem = Pawn == nullptr ? nullptr
        : Pawn->FindComponentByClass<UAbilitySystemComponent>();
    const UDACombatAttributeSet* Attributes = AbilitySystem == nullptr ? nullptr
        : AbilitySystem->GetSet<UDACombatAttributeSet>();
    if (Attributes == nullptr)
    { OutError = TEXT("ServiceUnavailable: registered production Founder HUD has no live combat authority."); return false; }
    OutState = {};
    OutState.Health = Attributes->GetHealth(); OutState.Guard = Attributes->GetGuard();
    OutState.Stamina = Attributes->GetStamina();
    const UDACameraModeController* Mode = Controller->FindComponentByClass<UDACameraModeController>();
    if (Mode == nullptr) Mode = Pawn->FindComponentByClass<UDACameraModeController>();
    if (Mode == nullptr)
    { OutError = TEXT("ServiceUnavailable: registered Founder HUD has no camera-mode authority."); return false; }
    OutState.PlayMode = Mode->GetCurrentMode() == EDAPlayMode::Founder ? TEXT("founder")
        : Mode->GetCurrentMode() == EDAPlayMode::Command ? TEXT("command") : TEXT("city");
    UDAFounderAbilityDefinitions* AbilityDefinitions = NewObject<UDAFounderAbilityDefinitions>();
    if (AbilityDefinitions == nullptr || !AbilityDefinitions->ImportFromJsonFile(
        FPaths::ProjectContentDir() / TEXT("DA/Abilities/FounderAbilityDefinitions.json")))
    { OutError = TEXT("ServiceUnavailable: canonical Founder ability authority is unavailable."); return false; }
    for (const FDAFounderAbilityDefinition& Ability : AbilityDefinitions->GetBaselineAbilities())
        OutState.AbilityIds.Add(Ability.AbilityId);
    UGameInstance* GameInstance = Controller->GetGameInstance();
    const UDAWorldStateSubsystem* World = GameInstance == nullptr ? nullptr
        : GameInstance->GetSubsystem<UDAWorldStateSubsystem>();
    if (World == nullptr || OutState.AbilityIds.IsEmpty())
    { OutError = TEXT("ServiceUnavailable: registered Founder HUD is incomplete."); return false; }
    const FDACampaignSnapshot& Campaign = World->GetPersistentCampaign();
    const FDACaptureRecord* ActiveCapture = Campaign.OperationConflict.CaptureRecords.FindByPredicate(
        [](const FDACaptureRecord& Record){ return Record.bCaptureInProgress && Record.ActiveInteractionId.IsValid(); });
    OutState.InteractionId = ActiveCapture != nullptr ? FName(*ActiveCapture->ActiveInteractionId.ToString())
        : !Campaign.WorldAssets.IsEmpty() ? FName(*Campaign.WorldAssets[0].WorldAssetId.ToString())
        : Campaign.WorldState.CurrentRegionId;
    OutState.CurrentWorldTick = Campaign.WorldState.CurrentWorldTick;
    OutState.CurrentDevelopmentCycle = Campaign.WorldState.ClockAuthority.CurrentDevelopmentCycle;
    if (OutState.InteractionId.IsNone())
    { OutError = TEXT("ServiceUnavailable: registered Founder HUD has no authoritative interaction target."); return false; }
    OutError.Reset(); return true;
}

bool UDAUISubsystem::CaptureCommandHUDState(FDACommandHUDSource& OutState, FString& OutError) const
{
    UGameInstance* GameInstance = BoundPlayerController == nullptr ? nullptr
        : BoundPlayerController->GetGameInstance();
    const UDACommandSubsystem* Commands = GameInstance == nullptr ? nullptr
        : GameInstance->GetSubsystem<UDACommandSubsystem>();
    if (Commands == nullptr)
    { OutError = TEXT("ServiceUnavailable: registered production Command HUD has no command authority."); return false; }
    const UDAWorldStateSubsystem* World = GameInstance->GetSubsystem<UDAWorldStateSubsystem>();
    const APawn* Pawn = BoundPlayerController == nullptr ? nullptr : BoundPlayerController->GetPawn();
    const UAbilitySystemComponent* AbilitySystem = Pawn == nullptr ? nullptr
        : Pawn->FindComponentByClass<UAbilitySystemComponent>();
    const UDACombatAttributeSet* Attributes = AbilitySystem == nullptr ? nullptr
        : AbilitySystem->GetSet<UDACombatAttributeSet>();
    if (World == nullptr || Attributes == nullptr)
    { OutError = TEXT("ServiceUnavailable: registered Command HUD lacks campaign or Founder authority."); return false; }
    const FDACampaignSnapshot& Campaign = World->GetPersistentCampaign();
    OutState = {}; OutState.CommandPoints = Commands->GetCommandPoints();
    float TotalSupply = 0.f;
    for (const UDASquadEntity* Squad : Commands->GetRegisteredSquads())
        if (Squad != nullptr)
        {
            FDACommandSquadViewModel& Row = OutState.Squads.Emplace_GetRef();
            Row.SquadId = Squad->GetFName(); Row.Morale = Squad->GetMorale(); Row.Supply = Squad->GetSupply();
            Row.bSelected = Commands->GetDirectlySelectedSquads().ContainsByPredicate(
                [Squad](const TObjectPtr<UDASquadEntity>& Selected){ return Selected.Get() == Squad; });
            Row.SuggestedOrderDestination = Squad->GetDestination();
            if (const AActor* Enemy = Squad->GetTarget()) OutState.KnownEnemyIds.AddUnique(Enemy->GetFName());
            TotalSupply += Row.Supply;
        }
    OutState.Supply = OutState.Squads.IsEmpty() ? 0.f : TotalSupply / OutState.Squads.Num();
    int32 SynaraRegions = 0;
    for (const FDARegionState& Region : Campaign.WorldState.Regions)
    {
        OutState.ControlZoneIds.Add(Region.RegionId);
        if (Region.OwnerId == TEXT("civilization.synara")) ++SynaraRegions;
        else if (!Region.OwnerId.IsNone()) OutState.KnownEnemyIds.AddUnique(Region.OwnerId);
    }
    for (const FDACaptureRecord& Capture : Campaign.OperationConflict.CaptureRecords)
        OutState.ControlZoneIds.AddUnique(FName(*Capture.WorldAssetId.ToString()));
    for (const FDASurrenderRecord& Surrender : Campaign.OperationConflict.SurrenderRecords)
        OutState.KnownEnemyIds.AddUnique(FName(*Surrender.SquadId.ToString()));
    OutState.Sovereignty = Campaign.WorldState.Regions.IsEmpty() ? 0.f
        : 100.f * static_cast<float>(SynaraRegions) / Campaign.WorldState.Regions.Num();
    OutState.FounderStatus = Attributes->GetHealth() > 0.f ? TEXT("founder.active") : TEXT("founder.incapacitated");
    const FDACaptureRecord* ActiveCapture = Campaign.OperationConflict.CaptureRecords.FindByPredicate(
        [](const FDACaptureRecord& Record){ return Record.bCaptureInProgress; });
    OutState.TacticalAlertId = ActiveCapture != nullptr ? FName(*ActiveCapture->WorldAssetId.ToString())
        : Campaign.RegionalCrisis.bTriggered
            && Campaign.RegionalCrisis.Resolution == EDAFoundryShortageResolution::None
            ? FName(TEXT("event.foundry_shortage")) : Campaign.WorldState.CurrentRegionId;
    if (const UDACameraModeController* Mode = BoundPlayerController->FindComponentByClass<UDACameraModeController>())
        OutState.PlayMode = Mode->GetCurrentMode() == EDAPlayMode::Command ? TEXT("command")
            : Mode->GetCurrentMode() == EDAPlayMode::Founder ? TEXT("founder") : TEXT("city");
    OutState.CurrentWorldTick = Campaign.WorldState.CurrentWorldTick;
    OutState.CurrentDevelopmentCycle = Campaign.WorldState.ClockAuthority.CurrentDevelopmentCycle;
    if (OutState.Squads.IsEmpty() || OutState.ControlZoneIds.IsEmpty() || OutState.KnownEnemyIds.IsEmpty()
        || OutState.FounderStatus.IsNone() || OutState.TacticalAlertId.IsNone() || OutState.PlayMode.IsNone())
    { OutError = TEXT("ServiceUnavailable: registered production Command HUD is incomplete."); return false; }
    OutError.Reset(); return true;
}

void UDAUISubsystem::HandlePlayerControllerChanged(APlayerController* PlayerController)
{
    if (PlayerController == nullptr)
    {
        TeardownPlayerUI();
        return;
    }
    FString Error;
    if (!InitializePlayerUI(PlayerController, Error))
        UE_LOG(LogTemp, Error, TEXT("Dominion production UI initialization failed: %s"), *Error);
}

void UDAUISubsystem::HandleAccessibilityPolicyChanged(const FDAAccessibilityRuntimePolicy& Policy)
{
    if (ViewModelProvider != nullptr) ViewModelProvider->SetAccessibilitySettings(Policy.Settings);
    if (Router == nullptr || RootLayout == nullptr) return;
    FString Error;
    if (BoundPlayerController != nullptr)
        if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(BoundPlayerController->InputComponent))
            if (!BindEnhancedInput(*Enhanced, Error))
            { UE_LOG(LogTemp, Error, TEXT("Accessibility input binding rebuild failed: %s"), *Error); return; }
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (!Router->ApplyCurrentInputContext(*LocalPlayer, Error))
            UE_LOG(LogTemp, Error, TEXT("Accessibility input remap failed: %s"), *Error);
    }
}

bool UDAUISubsystem::BindEnhancedInput(UEnhancedInputComponent& Input, FString& OutError)
{
    for (const uint32 Handle : InputBindingHandles) Input.RemoveBindingByHandle(Handle);
    InputBindingHandles.Reset();
    RuntimeInputActions.Reset(); RuntimeInputContexts.Reset();
    if (Router == nullptr)
    { OutError = TEXT("UI input graph requires an initialized router."); return false; }
    FString CacheError;
    const bool bUseExactGeneratedCache =
        FDAUIGeneratedCache::ValidateInstalledCache(Router->GetManifest(), CacheError);
    if (!FDAUIGeneratedCache::ValidateActiveScreenInputGraph(Router->GetManifest(), OutError)) return false;
    for (const FDAUICampaignActionDescriptor& Descriptor : Router->GetManifest().CampaignCriticalActions)
    {
        const TArray<FDAUIInputBindingDescriptor> BindingDescriptors =
            FDAUIGeneratedCache::GetBindingDescriptors(Descriptor, OutError);
        if (!OutError.IsEmpty()) return false;
        for (const FDAUIInputBindingDescriptor& DescriptorBinding : BindingDescriptors)
        {
            UInputAction* Action = bUseExactGeneratedCache
                ? Cast<UInputAction>(DescriptorBinding.AssetPath.TryLoad()) : nullptr;
            if (Action == nullptr && !bUseExactGeneratedCache)
            {
                FString ObjectName = DescriptorBinding.BindingId.ToString().Replace(TEXT("."), TEXT("_"));
                Action = NewObject<UInputAction>(this, FName(*ObjectName));
                if (Action != nullptr)
                    Action->ValueType = FDAUIGeneratedCache::GetExpectedValueType(Descriptor);
            }
            if (Action == nullptr)
            { OutError = TEXT("UI input action cache and native source fallback are both unavailable."); return false; }
            RuntimeInputActions.Add(DescriptorBinding.BindingId, Action);
            const EDAHoldToggleMode Mode = InputAccessibility == nullptr
                ? EDAHoldToggleMode::Press : InputAccessibility->GetAppliedMode(Descriptor.Id);
            const ETriggerEvent Trigger = Action->ValueType == EInputActionValueType::Boolean
                && Mode != EDAHoldToggleMode::Hold ? ETriggerEvent::Started : ETriggerEvent::Triggered;
            FEnhancedInputActionEventBinding& Binding = Input.BindAction(Action, Trigger,
                this, &UDAUISubsystem::HandleInputAction, Descriptor.Id,
                DescriptorBinding.PayloadIndex, DescriptorBinding.bController);
            InputBindingHandles.Add(Binding.GetHandle());
        }
    }
    for (const FDAUIScreenDescriptor& Screen : Router->GetManifest().Screens)
    {
        UInputMappingContext* Context = NewObject<UInputMappingContext>(
            this, FName(*(TEXT("IMC_Runtime_") + Screen.Id.ToString())));
        if (Context == nullptr)
        { OutError = TEXT("Could not allocate native active-screen input context."); return false; }
        for (const FName ActionId : Screen.CampaignCriticalActions)
        {
            const FDAUICampaignActionDescriptor* ActionDescriptor = Router->GetManifest().FindAction(ActionId);
            if (ActionDescriptor == nullptr)
            { OutError = TEXT("Active screen input context references an unknown action."); return false; }
            const TArray<FDAUIInputBindingDescriptor> Bindings =
                FDAUIGeneratedCache::GetBindingDescriptors(*ActionDescriptor, OutError);
            if (!OutError.IsEmpty()) return false;
            for (const FDAUIInputBindingDescriptor& Binding : Bindings)
            {
                const TObjectPtr<UInputAction>* Action = RuntimeInputActions.Find(Binding.BindingId);
                if (Action == nullptr || Action->Get() == nullptr
                    || !FDAUIGeneratedCache::MapFrozenBinding(
                        *Context, *Action->Get(), *ActionDescriptor, Binding, OutError)) return false;
            }
        }
        RuntimeInputContexts.Add(Screen.Id, Context);
        Router->RegisterRuntimeInputContext(Screen.Id, Context);
    }
    OutError.Reset();
    return true;
}

void UDAUISubsystem::HandleInputAction(const FInputActionValue& Value, const FName ActionId,
    const int32 PayloadIndex, const bool bControllerBinding)
{
    if (Router == nullptr) return;
    FString Error;
    FString Payload;
    if (UDAActivatableScreen* Active = Router->GetActiveScreenWidget())
        Payload = Active->BuildPayloadForAction(ActionId, PayloadIndex, bControllerBinding,
            Value.GetValueType() == EInputActionValueType::Axis1D ? Value.Get<float>() : 0.f,
            Value.GetValueType() == EInputActionValueType::Axis1D);
    if (Payload.IsEmpty())
        Payload = FString::Printf(TEXT("{\"actionId\":\"%s\"}"), *ActionId.ToString());
    DispatchAccessibleAction(ActionId, Payload, true, Error);
}

bool UDAUISubsystem::DispatchAccessibleAction(const FName ActionId, const FString& PayloadJson,
    const bool bTriggered, FString& OutError)
{
    if (Router == nullptr || InputAccessibility == nullptr)
    { OutError = TEXT("Accessible input dispatch requires initialized production UI services."); return false; }
    if (!InputAccessibility->ConsumeAccessibleAction(ActionId, bTriggered))
    { OutError.Reset(); return true; }
    return Router->DispatchAction(ActionId, PayloadJson, OutError);
}


bool UDAUISubsystem::EnterScreen(const FName ScreenId, FString& OutError)
{
    if (Router == nullptr || RootLayout == nullptr || !Router->EnterScreen(ScreenId))
    { OutError = TEXT("Production UI could not enter the requested screen."); return false; }
    OutError.Reset(); return true;
}

bool UDAUISubsystem::ToggleOverlay(const FName OverlayId, FString& OutError)
{
    return Router != nullptr && Router->ToggleOverlay(OverlayId, OutError);
}

void UDAUISubsystem::SetCommandExecutor(UDAUICommandService::FDAExecutor Executor)
{
    if (CommandService != nullptr) CommandService->SetNativeExecutor(MoveTemp(Executor));
}

bool UDAUISubsystem::SetGameplayCommandEndpoint(UObject* Endpoint, FString& OutError)
{
    if (Endpoint != nullptr && !Endpoint->GetClass()->ImplementsInterface(UDAGameplayUICommandEndpoint::StaticClass()))
    { OutError = TEXT("Gameplay UI command endpoint must implement IDAGameplayUICommandEndpoint."); return false; }
    GameplayCommandEndpoint = Endpoint;
    OutError.Reset();
    return true;
}

bool UDAUISubsystem::DispatchDefaultCommand(const FDAUICommandRequest& Request, FString& OutError)
{
    if (Router == nullptr)
    { OutError = TEXT("Production UI router is unavailable."); return false; }
    static const TMap<FName, FName> InternalScreens = {
        {TEXT("command.campaign.open_new"), TEXT("new_campaign")},
        {TEXT("command.ui.open_founder"), TEXT("founder_hud")},
        {TEXT("command.ui.open_world_map"), TEXT("world_map")},
        {TEXT("command.ui.open_city"), TEXT("city_hud")},
        {TEXT("command.ui.open_command"), TEXT("command_hud")},
        {TEXT("command.faction.open_diplomacy"), TEXT("diplomacy")},
        {TEXT("command.diplomacy.open_treaty"), TEXT("treaty_builder")}
    };
    static const TSet<FName> SelectionCommands = {
        TEXT("command.city.select_card"), TEXT("command.collection.inspect_card"),
        TEXT("command.building.track"), TEXT("command.quest.track"),
        TEXT("command.history.inspect"), TEXT("command.card.rotate"),
        TEXT("command.metrics.inspect")};
    static const TMap<FName, TPair<FName, int32>> CycleCommands = {
        {TEXT("command.city.card_previous"), TPair<FName, int32>(TEXT("city_hud"), -1)},
        {TEXT("command.city.card_next"), TPair<FName, int32>(TEXT("city_hud"), 1)},
        {TEXT("command.founder.ability_previous"), TPair<FName, int32>(TEXT("founder_hud"), -1)},
        {TEXT("command.founder.ability_next"), TPair<FName, int32>(TEXT("founder_hud"), 1)},
        {TEXT("command.command.squad_previous"), TPair<FName, int32>(TEXT("command_hud"), -1)},
        {TEXT("command.command.squad_next"), TPair<FName, int32>(TEXT("command_hud"), 1)}
    };
    if (const TPair<FName, int32>* Cycle = CycleCommands.Find(Request.CommandId))
        return ViewModelProvider != nullptr
            && ViewModelProvider->CycleStableSelection(Cycle->Key, Cycle->Value, OutError);
    if (SelectionCommands.Contains(Request.CommandId))
    {
        if (ViewModelProvider == nullptr
            || !ViewModelProvider->ApplySelectionCommand(Request.CommandId,
                Request.SourceScreenId, Request.PayloadJson, OutError)) return false;
        if (Request.CommandId == TEXT("command.collection.inspect_card"))
            return Router->EnterScreen(TEXT("card_inspect"))
                ? (OutError.Reset(), true)
                : (OutError = TEXT("Card selection succeeded but inspect routing failed."), false);
        return true;
    }
    if (Request.CommandId == TEXT("command.recap.continue"))
        return Router->EnterScreen(TEXT("city_hud"))
            ? (OutError.Reset(), true)
            : (OutError = TEXT("Returning-player recap could not route to the city."), false);
    if (const FName* Target = InternalScreens.Find(Request.CommandId))
    {
        const bool bReplaceMode = Request.CommandId == TEXT("command.ui.open_founder")
            || Request.CommandId == TEXT("command.ui.open_city")
            || Request.CommandId == TEXT("command.ui.open_command");
        if (bReplaceMode ? Router->NavigateReplacingHistory(*Target) : Router->EnterScreen(*Target))
        { OutError.Reset(); return true; }
        OutError = TEXT("Internal UI route could not enter its target screen.");
        return false;
    }
    if (Request.CommandId == TEXT("command.campaign.resume") || Request.CommandId == TEXT("command.ui.back"))
    {
        if (Router->HandleBack()) { OutError.Reset(); return true; }
        OutError = TEXT("Internal UI back route could not exit the active surface.");
        return false;
    }
    const FString Prefix = TEXT("command.ui.toggle_overlay.");
    const FString Command = Request.CommandId.ToString();
    if (Command.StartsWith(Prefix))
        return Router->ToggleOverlay(FName(*Command.RightChop(Prefix.Len())), OutError);
    UObject* Endpoint = GameplayCommandEndpoint.Get();
    if (Endpoint == nullptr)
        if (UGameInstance* GameInstance = GetLocalPlayer() == nullptr
            ? nullptr : GetLocalPlayer()->GetGameInstance())
        {
            Endpoint = GameInstance->GetSubsystem<UDAUICommandEndpointSubsystem>();
            GameplayCommandEndpoint = Endpoint;
        }
    if (Request.CommandId == TEXT("command.time.tactical_pause")
        && GameplayAccessibility != nullptr && !GameplayAccessibility->IsTacticalPauseEnabled())
    { OutError = TEXT("Tactical Pause is disabled by the active accessibility policy."); return false; }
    if (Endpoint == nullptr)
    { OutError = TEXT("Default gameplay command orchestrator could not be initialized."); return false; }
    const bool bExecuted = IDAGameplayUICommandEndpoint::Execute_ExecuteGameplayUICommand(
        Endpoint, Request, OutError);
    if (bExecuted && ViewModelProvider != nullptr)
    {
        if (Request.CommandId == TEXT("command.command.select_squad"))
        {
            FString SelectionError;
            if (!ViewModelProvider->ApplySelectionCommand(Request.CommandId,
                Request.SourceScreenId, Request.PayloadJson, SelectionError))
            { OutError = SelectionError; return false; }
        }
        ViewModelProvider->RefreshRuntimeSources();
    }
    return bExecuted;
}
