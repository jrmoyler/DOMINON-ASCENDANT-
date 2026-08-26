#pragma once

#include "CoreMinimal.h"
#include "Manifest/DAUIManifest.h"
#include "UObject/Object.h"
#include "Widgets/DAGrayboxWidgets.h"

#include "DAUIScreenRouter.generated.h"

class UCommonActivatableWidgetStack;
class UInputMappingContext;
class ULocalPlayer;
class APlayerController;

UENUM(BlueprintType)
enum class EDAUIInputDevice : uint8
{
    Keyboard,
    Controller
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUICommandRequest
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName CommandId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SourceScreenId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString PayloadJson;
};

/** Reflected gameplay/application mutation endpoint used after UI-only routes are exhausted. */
UINTERFACE(BlueprintType)
class DOMINIONUI_API UDAGameplayUICommandEndpoint : public UInterface
{
    GENERATED_BODY()
};

class DOMINIONUI_API IDAGameplayUICommandEndpoint
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, Category="Dominion|UI|Commands")
    bool ExecuteGameplayUICommand(const FDAUICommandRequest& Request, FString& OutError);
};

/** UObject-tracked mutation boundary implemented by gameplay/application services. */
UCLASS(BlueprintType)
class DOMINIONUI_API UDAUICommandService : public UObject
{
    GENERATED_BODY()
public:
    using FDAExecutor = TFunction<bool(const FDAUICommandRequest&, FString&)>;
    virtual bool ExecuteCommand(const FDAUICommandRequest& Request, FString& OutError);
    void SetNativeExecutor(FDAExecutor InExecutor) { Executor = MoveTemp(InExecutor); }
private:
    FDAExecutor Executor;
};

/** Stable-ID CommonUI navigation and service-command router. */
UCLASS(BlueprintType)
class DOMINIONUI_API UDAUIScreenRouter final : public UObject
{
    GENERATED_BODY()
public:
    bool Initialize(const FDAUIManifest& InManifest, TArray<FText>& OutErrors);
    bool InitializeCanonical(TArray<FText>& OutErrors);
    void Shutdown();
    void SetCommandService(UDAUICommandService* InCommandService) { CommandService = InCommandService; }
    void SetViewModelProvider(UDAUIViewModelProvider* InProvider);
    void SetInputAccessibilityAdapter(UDAInputAccessibilityAdapter* InAdapter)
    { InputAccessibilityAdapter = InAdapter; }

    UFUNCTION(BlueprintCallable, Category="Dominion|UI|Navigation") bool EnterScreen(FName ScreenId);
    UFUNCTION(BlueprintCallable, Category="Dominion|UI|Navigation") bool NavigateReplacingHistory(FName ScreenId);
    UFUNCTION(BlueprintCallable, Category="Dominion|UI|Navigation") bool ExitActiveScreen();
    UFUNCTION(BlueprintCallable, Category="Dominion|UI|Navigation") bool HandleBack();

    UFUNCTION(BlueprintPure, Category="Dominion|UI|Navigation") FName GetActiveScreenId() const;
    UFUNCTION(BlueprintPure, Category="Dominion|UI|Navigation") FName GetCurrentFocusTarget() const;
    UFUNCTION(BlueprintPure, Category="Dominion|UI|Navigation") EDAUIScreenInputMode GetCurrentInputMode() const;
    UFUNCTION(BlueprintPure, Category="Dominion|UI|Navigation") FName GetCurrentInputContextId() const;

    bool IsActionReachable(FName ActionId, EDAUIInputDevice Device) const;
    bool DispatchAction(FName ActionId, const FString& PayloadJson, FString& OutError);
    UDAActivatableScreen* ActivateCurrent(UCommonActivatableWidgetStack& LayerStack);
    bool ApplyCurrentInputContext(ULocalPlayer& LocalPlayer, FString& OutError);
    bool ApplyCurrentInputMode(APlayerController& PlayerController) const;
    void RegisterLayerStack(EDAUIScreenLayer Layer, UCommonActivatableWidgetStack* LayerStack);
    void RegisterRuntimeInputContext(FName ScreenId, UInputMappingContext* InputContext);
    void BindPlayerContext(ULocalPlayer* LocalPlayer, APlayerController* PlayerController);
    bool ToggleOverlay(FName OverlayId, FString& OutError);
    UDAActivatableScreen* GetActiveScreenWidget() const
    { return RouteWidgets.IsEmpty() ? nullptr : RouteWidgets.Last().Widget.Get(); }
    FName GetActiveOverlayId() const { return ActiveOverlayId; }
    const FDAUIOverlayReadModel& GetActiveOverlayReadModel() const { return ActiveOverlayReadModel; }
    const FDAUIManifest& GetManifest() const { return Manifest; }

private:
    const FDAUIScreenDescriptor* GetActiveDescriptor() const;
    UDAActivatableScreen* ActivateDescriptor(
        const FDAUIScreenDescriptor& Screen, UCommonActivatableWidgetStack& LayerStack);
    void HandleProjectionInvalidated();
    bool ClearLayerRoutes(EDAUIScreenLayer Layer);
    void ApplyPostNavigationState();

    struct FDAUIRouteWidget
    {
        EDAUIScreenLayer Layer = EDAUIScreenLayer::Panel;
        TWeakObjectPtr<UDAActivatableScreen> Widget;
    };

    UPROPERTY(Transient) FDAUIManifest Manifest;
    UPROPERTY(Transient) TArray<FName> ScreenStack;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> ActiveInputContext;
    TWeakObjectPtr<UDAUICommandService> CommandService;
    TWeakObjectPtr<UDAUIViewModelProvider> ViewModelProvider;
    TWeakObjectPtr<UDAInputAccessibilityAdapter> InputAccessibilityAdapter;
    FDelegateHandle ProjectionInvalidatedHandle;
    TWeakObjectPtr<UDAOverlayWidget> ActiveOverlayWidget;
    FName ActiveOverlayId;
    FDAUIOverlayReadModel ActiveOverlayReadModel;
    TArray<FDAUIRouteWidget> RouteWidgets;
    TMap<EDAUIScreenLayer, TWeakObjectPtr<UCommonActivatableWidgetStack>> LayerStacks;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UInputMappingContext>> RuntimeInputContexts;
    TWeakObjectPtr<ULocalPlayer> BoundLocalPlayer;
    TWeakObjectPtr<APlayerController> BoundPlayerController;
    bool bInitialized = false;
};
