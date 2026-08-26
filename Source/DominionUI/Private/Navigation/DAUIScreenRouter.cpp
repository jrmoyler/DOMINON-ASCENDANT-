#include "Navigation/DAUIScreenRouter.h"

#include "Widgets/CommonActivatableWidgetContainer.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Manifest/DAUIGeneratedMetadata.h"

bool UDAUICommandService::ExecuteCommand(const FDAUICommandRequest& Request, FString& OutError)
{
    if (!Executor)
    {
        OutError = TEXT("UI command service has no authoritative executor.");
        return false;
    }
    return Executor(Request, OutError);
}

void UDAUIScreenRouter::SetViewModelProvider(UDAUIViewModelProvider* InProvider)
{
    if (ViewModelProvider.IsValid() && ProjectionInvalidatedHandle.IsValid())
        ViewModelProvider->OnProjectionInvalidated.Remove(ProjectionInvalidatedHandle);
    ViewModelProvider = InProvider; ProjectionInvalidatedHandle.Reset();
    if (InProvider != nullptr)
        ProjectionInvalidatedHandle = InProvider->OnProjectionInvalidated.AddUObject(
            this, &UDAUIScreenRouter::HandleProjectionInvalidated);
}

bool UDAUIScreenRouter::Initialize(const FDAUIManifest& InManifest, TArray<FText>& OutErrors)
{
    Shutdown(); OutErrors.Reset();
    if (!InManifest.Validate(OutErrors)) return false;
    FString InputGraphError;
    if (!FDAUIGeneratedCache::ValidateActiveScreenInputGraph(InManifest, InputGraphError))
    {
        OutErrors.Add(FText::FromString(InputGraphError));
        return false;
    }
    Manifest = InManifest;
    ScreenStack.Reset();
    RouteWidgets.Reset();
    bInitialized = true;
    return true;
}

void UDAUIScreenRouter::Shutdown()
{
    if (ViewModelProvider.IsValid() && ProjectionInvalidatedHandle.IsValid())
        ViewModelProvider->OnProjectionInvalidated.Remove(ProjectionInvalidatedHandle);
    ProjectionInvalidatedHandle.Reset();
    for (int32 Index = RouteWidgets.Num() - 1; Index >= 0; --Index)
        if (UDAActivatableScreen* Widget = RouteWidgets[Index].Widget.Get())
        {
            if (const TWeakObjectPtr<UCommonActivatableWidgetStack>* Stack = LayerStacks.Find(RouteWidgets[Index].Layer))
                if (Stack->IsValid()) Stack->Get()->RemoveWidget(*Widget);
        }
    if (ActiveOverlayWidget.IsValid())
        if (const TWeakObjectPtr<UCommonActivatableWidgetStack>* Stack = LayerStacks.Find(EDAUIScreenLayer::HUD))
            if (Stack->IsValid()) Stack->Get()->RemoveWidget(*ActiveOverlayWidget.Get());
    if (BoundLocalPlayer.IsValid())
        if (UEnhancedInputLocalPlayerSubsystem* Input =
            BoundLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            if (ActiveInputContext != nullptr) Input->RemoveMappingContext(ActiveInputContext);
    ScreenStack.Reset(); RouteWidgets.Reset(); LayerStacks.Reset(); RuntimeInputContexts.Reset(); ActiveInputContext = nullptr;
    ActiveOverlayWidget.Reset(); ActiveOverlayId = NAME_None; ActiveOverlayReadModel = {};
    BoundLocalPlayer.Reset(); BoundPlayerController.Reset(); ViewModelProvider.Reset(); CommandService.Reset();
    InputAccessibilityAdapter.Reset();
    bInitialized = false;
}

void UDAUIScreenRouter::HandleProjectionInvalidated()
{
    if (!ViewModelProvider.IsValid()) return;
    const int32 Count = FMath::Min(ScreenStack.Num(), RouteWidgets.Num());
    for (int32 Index = 0; Index < Count; ++Index)
        if (UDAActivatableScreen* Widget = RouteWidgets[Index].Widget.Get())
            if (const FDAUIScreenDescriptor* Screen = Manifest.FindScreen(ScreenStack[Index]))
            {
                FDAUIScreenProjection Projection;
                if (!ViewModelProvider->BuildScreen(*Screen, Projection)) continue;
                Widget->SetProjection(Projection);
                if (UDACityHUDWidget* City = Cast<UDACityHUDWidget>(Widget)) City->SetViewModel(Projection.CityHUD);
                if (UDAFounderHUDWidget* Founder = Cast<UDAFounderHUDWidget>(Widget)) Founder->SetViewModel(Projection.FounderHUD);
                if (UDACommandHUDWidget* Command = Cast<UDACommandHUDWidget>(Widget)) Command->SetViewModel(Projection.CommandHUD);
            }
    if (ActiveOverlayWidget.IsValid() && !ActiveOverlayId.IsNone())
        if (const FDAUIOverlayDescriptor* Overlay = Manifest.FindOverlay(ActiveOverlayId))
        {
            FDAUIOverlayReadModel ReadModel;
            const bool bMarkers = ActiveOverlayReadModel.bColorIndependentMarkers;
            if (ViewModelProvider->BuildOverlay(*Overlay, bMarkers, ReadModel))
            {
                ActiveOverlayReadModel = ReadModel;
                ActiveOverlayWidget->ConfigureOverlay(ActiveOverlayId, ReadModel, this);
            }
        }
}

bool UDAUIScreenRouter::InitializeCanonical(TArray<FText>& OutErrors)
{
    FDAUIManifest Loaded;
    return FDAUIManifest::LoadCanonical(Loaded, OutErrors) && Initialize(Loaded, OutErrors);
}

bool UDAUIScreenRouter::EnterScreen(const FName ScreenId)
{
    const FDAUIScreenDescriptor* Descriptor = Manifest.FindScreen(ScreenId);
    if (!bInitialized || Descriptor == nullptr) return false;
    if (!ScreenStack.IsEmpty() && ScreenStack.Last() == ScreenId)
        return !RouteWidgets.IsEmpty() && RouteWidgets.Last().Widget.IsValid();
    const TWeakObjectPtr<UCommonActivatableWidgetStack>* Registered = LayerStacks.Find(Descriptor->Layer);
    UCommonActivatableWidgetStack* Stack = Registered == nullptr ? nullptr : Registered->Get();
    if (Stack == nullptr) return false;
    ScreenStack.Add(ScreenId);
    FDAUIRouteWidget& Route = RouteWidgets.Emplace_GetRef(); Route.Layer = Descriptor->Layer;
    Route.Widget = ActivateDescriptor(*Descriptor, *Stack);
    if (!Route.Widget.IsValid())
    { RouteWidgets.Pop(EAllowShrinking::No); ScreenStack.Pop(EAllowShrinking::No); return false; }
    const auto RollbackEnter = [this, Stack]()
    {
        if (!RouteWidgets.IsEmpty())
            if (UDAActivatableScreen* FailedWidget = RouteWidgets.Last().Widget.Get())
                Stack->RemoveWidget(*FailedWidget);
        if (!RouteWidgets.IsEmpty()) RouteWidgets.Pop(EAllowShrinking::No);
        if (!ScreenStack.IsEmpty()) ScreenStack.Pop(EAllowShrinking::No);
        FString RestoreError;
        if (!ScreenStack.IsEmpty())
        {
            if (BoundLocalPlayer.IsValid()) ApplyCurrentInputContext(*BoundLocalPlayer.Get(), RestoreError);
            if (BoundPlayerController.IsValid()) ApplyCurrentInputMode(*BoundPlayerController.Get());
            if (!RouteWidgets.IsEmpty())
                if (UDAActivatableScreen* Previous = RouteWidgets.Last().Widget.Get())
                    if (UWidget* Focus = Previous->GetResolvedFocusWidget()) Focus->SetFocus();
        }
        else
        {
            if (BoundLocalPlayer.IsValid())
                if (UEnhancedInputLocalPlayerSubsystem* Input =
                    BoundLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                    if (ActiveInputContext != nullptr) Input->RemoveMappingContext(ActiveInputContext);
            ActiveInputContext = nullptr;
            if (BoundPlayerController.IsValid())
            {
                FInputModeGameOnly Mode; BoundPlayerController->SetInputMode(Mode);
                BoundPlayerController->SetShowMouseCursor(false);
            }
        }
        return false;
    };
    FString InputError;
    if ((BoundLocalPlayer.IsValid() && !ApplyCurrentInputContext(*BoundLocalPlayer.Get(), InputError))
        || (BoundPlayerController.IsValid() && !ApplyCurrentInputMode(*BoundPlayerController.Get())))
        return RollbackEnter();
    UWidget* DesiredFocus = Route.Widget->GetResolvedFocusWidget();
    if (DesiredFocus == nullptr) return RollbackEnter();
    DesiredFocus->SetFocus();
    return true;
}

bool UDAUIScreenRouter::ExitActiveScreen()
{
    if (!bInitialized || ScreenStack.IsEmpty()) return false;
    const FDAUIScreenDescriptor* Active = GetActiveDescriptor();
    if (Active == nullptr) return false;
    if (Active->BackTarget == TEXT("__exit__"))
    {
        const bool bCleared = ClearLayerRoutes(Active->Layer);
        ApplyPostNavigationState();
        return bCleared;
    }
    if (Manifest.FindScreen(Active->BackTarget) == nullptr) return false;
    return NavigateReplacingHistory(Active->BackTarget);
}

bool UDAUIScreenRouter::ClearLayerRoutes(const EDAUIScreenLayer Layer)
{
    bool bRemoved = false;
    for (int32 Index = RouteWidgets.Num() - 1; Index >= 0; --Index)
    {
        if (RouteWidgets[Index].Layer != Layer) continue;
        if (UDAActivatableScreen* Widget = RouteWidgets[Index].Widget.Get())
            if (const TWeakObjectPtr<UCommonActivatableWidgetStack>* Stack = LayerStacks.Find(Layer))
                if (Stack->IsValid()) Stack->Get()->RemoveWidget(*Widget);
        RouteWidgets.RemoveAt(Index);
        ScreenStack.RemoveAt(Index);
        bRemoved = true;
    }
    return bRemoved;
}

void UDAUIScreenRouter::ApplyPostNavigationState()
{
    if (ScreenStack.IsEmpty())
    {
        if (BoundLocalPlayer.IsValid())
            if (UEnhancedInputLocalPlayerSubsystem* Input =
                BoundLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                if (ActiveInputContext != nullptr) Input->RemoveMappingContext(ActiveInputContext);
        ActiveInputContext = nullptr;
        if (BoundPlayerController.IsValid())
        {
            FInputModeGameOnly Mode; BoundPlayerController->SetInputMode(Mode);
            BoundPlayerController->SetShowMouseCursor(false);
        }
        return;
    }
    FString InputError;
    if (BoundLocalPlayer.IsValid()) ApplyCurrentInputContext(*BoundLocalPlayer.Get(), InputError);
    if (BoundPlayerController.IsValid()) ApplyCurrentInputMode(*BoundPlayerController.Get());
    if (!RouteWidgets.IsEmpty()) if (UDAActivatableScreen* Revealed = RouteWidgets.Last().Widget.Get())
    {
        if (UWidget* DesiredFocus = Revealed->GetResolvedFocusWidget()) DesiredFocus->SetFocus();
        else Revealed->SetFocus();
    }
}

bool UDAUIScreenRouter::NavigateReplacingHistory(const FName ScreenId)
{
    if (!bInitialized || Manifest.FindScreen(ScreenId) == nullptr) return false;
    TArray<EDAUIScreenLayer> UsedLayers;
    for (const FDAUIRouteWidget& Route : RouteWidgets) UsedLayers.AddUnique(Route.Layer);
    for (const EDAUIScreenLayer Layer : UsedLayers) ClearLayerRoutes(Layer);
    ApplyPostNavigationState();
    return EnterScreen(ScreenId);
}

void UDAUIScreenRouter::RegisterLayerStack(
    const EDAUIScreenLayer Layer, UCommonActivatableWidgetStack* LayerStack)
{
    if (LayerStack == nullptr) LayerStacks.Remove(Layer);
    else LayerStacks.Add(Layer, LayerStack);
}

void UDAUIScreenRouter::RegisterRuntimeInputContext(
    const FName ScreenId, UInputMappingContext* InputContext)
{
    if (InputContext == nullptr) RuntimeInputContexts.Remove(ScreenId);
    else RuntimeInputContexts.Add(ScreenId, InputContext);
}

void UDAUIScreenRouter::BindPlayerContext(
    ULocalPlayer* LocalPlayer, APlayerController* PlayerController)
{
    BoundLocalPlayer = LocalPlayer; BoundPlayerController = PlayerController;
}

bool UDAUIScreenRouter::ToggleOverlay(const FName OverlayId, FString& OutError)
{
    const TWeakObjectPtr<UCommonActivatableWidgetStack>* Registered = LayerStacks.Find(EDAUIScreenLayer::HUD);
    UCommonActivatableWidgetStack* Stack = Registered == nullptr ? nullptr : Registered->Get();
    if (Stack == nullptr) { OutError = TEXT("HUD layer stack is not registered."); return false; }
    if (ActiveOverlayId == OverlayId && ActiveOverlayWidget.IsValid())
    {
        Stack->RemoveWidget(*ActiveOverlayWidget.Get());
        ActiveOverlayWidget.Reset(); ActiveOverlayId = NAME_None; ActiveOverlayReadModel = {}; return true;
    }
    const FDAUIOverlayDescriptor* Overlay = Manifest.FindOverlay(OverlayId);
    if (Overlay == nullptr || !ViewModelProvider.IsValid())
    { OutError = TEXT("Overlay requires a frozen descriptor and authoritative view-model provider."); return false; }
    bool bColorIndependentMarkers = true;
    if (UWorld* World = Stack->GetWorld()) if (UGameInstance* GameInstance = World->GetGameInstance())
        if (const UDAAccessibilitySettingsSubsystem* Accessibility =
            GameInstance->GetSubsystem<UDAAccessibilitySettingsSubsystem>())
            bColorIndependentMarkers = Accessibility->GetRuntimePolicy().bUseColorIndependentMarkers;
    FDAUIOverlayReadModel NewReadModel;
    if (!ViewModelProvider->BuildOverlay(*Overlay, bColorIndependentMarkers, NewReadModel))
    { OutError = TEXT("Authoritative overlay projection failed."); return false; }
    FString CacheError;
    UClass* OverlayClass = FDAUIGeneratedCache::ValidateInstalledCache(Manifest, CacheError)
        ? FSoftClassPath(Overlay->AssetPath.ToString() + TEXT("_C")).TryLoadClass<UDAOverlayWidget>() : nullptr;
    if (OverlayClass == nullptr) OverlayClass = Overlay->WidgetClass.TryLoadClass<UDAOverlayWidget>();
    if (OverlayClass == nullptr) { OutError = TEXT("Frozen overlay source class is unavailable."); return false; }
    if (ActiveOverlayWidget.IsValid())
        Stack->RemoveWidget(*ActiveOverlayWidget.Get());
    ActiveOverlayWidget.Reset(); ActiveOverlayId = NAME_None; ActiveOverlayReadModel = {};
    UDAOverlayWidget* Widget = Stack->AddWidget<UDAOverlayWidget>(
        TSubclassOf<UCommonActivatableWidget>(OverlayClass),
        [this, OverlayId, &NewReadModel, Stack](UDAOverlayWidget& Instance)
        {
            Instance.ConfigureOverlay(OverlayId, NewReadModel, this);
            if (UWorld* World = Stack->GetWorld())
                if (UGameInstance* GameInstance = World->GetGameInstance())
                    if (UDAAccessibilitySettingsSubsystem* Accessibility =
                        GameInstance->GetSubsystem<UDAAccessibilitySettingsSubsystem>())
                    {
                        FString AccessibilityError;
                        Accessibility->RegisterConsumer(&Instance, AccessibilityError);
                    }
        });
    if (Widget == nullptr) { OutError = TEXT("Overlay stack rejected the widget."); return false; }
    ActiveOverlayReadModel = MoveTemp(NewReadModel);
    ActiveOverlayWidget = Widget; ActiveOverlayId = OverlayId; OutError.Reset(); return true;
}

bool UDAUIScreenRouter::HandleBack()
{
    if (!ActiveOverlayId.IsNone() && ActiveOverlayWidget.IsValid())
    {
        FString Error;
        return ToggleOverlay(ActiveOverlayId, Error);
    }
    return ExitActiveScreen();
}

FName UDAUIScreenRouter::GetActiveScreenId() const
{
    return ScreenStack.IsEmpty() ? NAME_None : ScreenStack.Last();
}

const FDAUIScreenDescriptor* UDAUIScreenRouter::GetActiveDescriptor() const
{
    return Manifest.FindScreen(GetActiveScreenId());
}

FName UDAUIScreenRouter::GetCurrentFocusTarget() const
{
    const FDAUIScreenDescriptor* Screen = GetActiveDescriptor();
    return Screen == nullptr ? NAME_None : Screen->EntryFocusTarget;
}

EDAUIScreenInputMode UDAUIScreenRouter::GetCurrentInputMode() const
{
    const FDAUIScreenDescriptor* Screen = GetActiveDescriptor();
    return Screen == nullptr ? EDAUIScreenInputMode::GameOnly : Screen->InputMode;
}

FName UDAUIScreenRouter::GetCurrentInputContextId() const
{
    const FDAUIScreenDescriptor* Screen = GetActiveDescriptor();
    return Screen == nullptr ? NAME_None : Screen->InputContextId;
}

bool UDAUIScreenRouter::IsActionReachable(const FName ActionId, const EDAUIInputDevice Device) const
{
    const FDAUICampaignActionDescriptor* Action = Manifest.FindAction(ActionId);
    return Action != nullptr && !Action->bRequiresHover
        && (Device == EDAUIInputDevice::Keyboard ? !Action->KeyboardInput.IsEmpty() : !Action->ControllerInput.IsEmpty());
}

bool UDAUIScreenRouter::DispatchAction(const FName ActionId, const FString& PayloadJson, FString& OutError)
{
    const FDAUICampaignActionDescriptor* Action = Manifest.FindAction(ActionId);
    if (Action == nullptr || Action->bRequiresHover)
    {
        OutError = TEXT("Unknown or hover-only campaign UI action.");
        return false;
    }
    const FName ActiveId = GetActiveScreenId();
    if (ActiveId.IsNone() || !Action->Surfaces.Contains(ActiveId))
    {
        OutError = TEXT("Campaign UI action is not available on the active surface.");
        return false;
    }
    if (!CommandService.IsValid())
    {
        OutError = TEXT("No authoritative UI command service is registered.");
        return false;
    }
    FDAUICommandRequest Request;
    Request.ActionId = Action->Id;
    Request.CommandId = Action->CommandId;
    Request.SourceScreenId = ActiveId;
    Request.PayloadJson = PayloadJson;
    return CommandService->ExecuteCommand(Request, OutError);
}

bool UDAUIScreenRouter::ApplyCurrentInputContext(ULocalPlayer& LocalPlayer, FString& OutError)
{
    const FDAUIScreenDescriptor* Screen = GetActiveDescriptor();
    const FDAUIInputContextDescriptor* Context = Screen == nullptr ? nullptr
        : Manifest.FindInputContext(Screen->InputContextId);
    UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer.GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (Input != nullptr && ActiveInputContext != nullptr)
    {
        Input->RemoveMappingContext(ActiveInputContext);
        ActiveInputContext = nullptr;
    }
    UInputMappingContext* LoadedFrozen = Screen == nullptr ? nullptr
        : RuntimeInputContexts.FindRef(Screen->Id);
    if (Input == nullptr || Context == nullptr || LoadedFrozen == nullptr)
    {
        OutError = TEXT("Active UI input context could not be applied.");
        return false;
    }
    UInputMappingContext* Loaded = LoadedFrozen;
    if (InputAccessibilityAdapter.IsValid())
    {
        Loaded = InputAccessibilityAdapter->BuildPlayerMappingContext(
            *LoadedFrozen, Manifest, *Screen, OutError);
        if (Loaded == nullptr) return false;
    }
    Input->AddMappingContext(Loaded, Context->Priority);
    ActiveInputContext = Loaded;
    return true;
}

bool UDAUIScreenRouter::ApplyCurrentInputMode(APlayerController& PlayerController) const
{
    const FDAUIScreenDescriptor* Screen = GetActiveDescriptor();
    if (Screen == nullptr) return false;
    if (Screen->InputMode == EDAUIScreenInputMode::GameOnly)
    {
        FInputModeGameOnly Mode; PlayerController.SetInputMode(Mode); PlayerController.SetShowMouseCursor(false);
    }
    else if (Screen->InputMode == EDAUIScreenInputMode::UIOnly)
    {
        FInputModeUIOnly Mode; PlayerController.SetInputMode(Mode); PlayerController.SetShowMouseCursor(true);
    }
    else
    {
        FInputModeGameAndUI Mode; PlayerController.SetInputMode(Mode); PlayerController.SetShowMouseCursor(true);
    }
    return true;
}

UDAActivatableScreen* UDAUIScreenRouter::ActivateCurrent(UCommonActivatableWidgetStack& LayerStack)
{
    const FDAUIScreenDescriptor* Screen = GetActiveDescriptor();
    if (Screen == nullptr) return nullptr;
    if (!RouteWidgets.IsEmpty() && RouteWidgets.Last().Widget.IsValid())
        return RouteWidgets.Last().Widget.Get();
    RegisterLayerStack(Screen->Layer, &LayerStack);
    UDAActivatableScreen* Activated = ActivateDescriptor(*Screen, LayerStack);
    if (Activated != nullptr)
        if (UWidget* DesiredFocus = Activated->GetResolvedFocusWidget()) DesiredFocus->SetFocus();
    if (!RouteWidgets.IsEmpty()) RouteWidgets.Last().Widget = Activated;
    return Activated;
}

UDAActivatableScreen* UDAUIScreenRouter::ActivateDescriptor(
    const FDAUIScreenDescriptor& Screen, UCommonActivatableWidgetStack& LayerStack)
{
    const FSoftClassPath GeneratedClassPath(Screen.AssetPath.ToString() + TEXT("_C"));
    UClass* Loaded = FDAUIGeneratedCache::CanUseGeneratedScreen(Manifest, Screen)
        ? GeneratedClassPath.TryLoadClass<UDAActivatableScreen>() : nullptr;
    if (Loaded == nullptr) Loaded = Screen.WidgetClass.TryLoadClass<UDAActivatableScreen>();
    if (Loaded == nullptr) return nullptr;
    UDAActivatableScreen* DominionScreen = LayerStack.AddWidget<UDAActivatableScreen>(
        TSubclassOf<UCommonActivatableWidget>(Loaded),
        [this, &Screen, &LayerStack](UDAActivatableScreen& Instance)
        {
            Instance.ConfigureScreen(Screen, this);
            if (ViewModelProvider.IsValid())
            {
                FDAUIScreenProjection Projection;
                if (ViewModelProvider->BuildScreen(Screen, Projection))
                {
                    Instance.SetProjection(Projection);
                    if (UDACityHUDWidget* City = Cast<UDACityHUDWidget>(&Instance))
                        City->SetViewModel(Projection.CityHUD);
                    if (UDAFounderHUDWidget* Founder = Cast<UDAFounderHUDWidget>(&Instance))
                        Founder->SetViewModel(Projection.FounderHUD);
                    if (UDACommandHUDWidget* Command = Cast<UDACommandHUDWidget>(&Instance))
                        Command->SetViewModel(Projection.CommandHUD);
                }
            }
            if (UWorld* World = LayerStack.GetWorld())
                if (UGameInstance* GameInstance = World->GetGameInstance())
                    if (UDAAccessibilitySettingsSubsystem* Accessibility =
                        GameInstance->GetSubsystem<UDAAccessibilitySettingsSubsystem>())
                    {
                        FString AccessibilityError;
                        Accessibility->RegisterConsumer(&Instance, AccessibilityError);
                    }
        });
    if (DominionScreen != nullptr)
    {
        UWidget* DesiredFocus = DominionScreen->GetResolvedFocusWidget();
        if (DesiredFocus == nullptr)
        {
            LayerStack.RemoveWidget(*DominionScreen); return nullptr;
        }
    }
    return DominionScreen;
}
