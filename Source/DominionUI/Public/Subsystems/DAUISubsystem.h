#pragma once

#include "Accessibility/DAAccessibilitySettings.h"
#include "CommonUserWidget.h"
#include "Navigation/DAUIScreenRouter.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ViewModels/DAUIViewModels.h"

#include "DAUISubsystem.generated.h"

class APlayerController;
class UCommonActivatableWidgetStack;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** Strong root layout which owns one real CommonUI stack per frozen layer. */
UCLASS(BlueprintType)
class DOMINIONUI_API UDARootUILayout final : public UCommonUserWidget
{
    GENERATED_BODY()
public:
    UCommonActivatableWidgetStack* GetLayerStack(EDAUIScreenLayer Layer) const;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY(Transient) TObjectPtr<UCommonActivatableWidgetStack> MenuStack;
    UPROPERTY(Transient) TObjectPtr<UCommonActivatableWidgetStack> HUDStack;
    UPROPERTY(Transient) TObjectPtr<UCommonActivatableWidgetStack> PanelStack;
    UPROPERTY(Transient) TObjectPtr<UCommonActivatableWidgetStack> ModalStack;
};

/** Production local-player owner for the router, services, root stacks, inputs and accessibility adapters. */
UCLASS()
class DOMINIONUI_API UDAUISubsystem final : public ULocalPlayerSubsystem,
    public IDAFounderHUDLiveSource, public IDACommandHUDLiveSource
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    bool InitializePlayerUI(APlayerController* PlayerController, FString& OutError);
    bool AttachRootLayout(UDARootUILayout* InRootLayout, APlayerController* PlayerController, FString& OutError);
    bool EnterScreen(FName ScreenId, FString& OutError);
    bool ToggleOverlay(FName OverlayId, FString& OutError);
    /** Production Enhanced Input dispatch boundary; applies persisted Press/Hold/Toggle semantics. */
    bool DispatchAccessibleAction(FName ActionId, const FString& PayloadJson,
        bool bTriggered, FString& OutError);
    void SetCommandExecutor(UDAUICommandService::FDAExecutor Executor);
    bool SetGameplayCommandEndpoint(UObject* Endpoint, FString& OutError);

    UDAUIScreenRouter* GetRouter() const { return Router; }
    UDAUICommandService* GetCommandService() const { return CommandService; }
    UDAUIViewModelProvider* GetViewModelProvider() const { return ViewModelProvider; }
    UDARootUILayout* GetRootLayout() const { return RootLayout; }
    UDAInputAccessibilityAdapter* GetInputAccessibility() const { return InputAccessibility; }
    UDAUIVisualAccessibilityAdapter* GetVisualAccessibility() const { return VisualAccessibility; }
    UDACameraAccessibilityAdapter* GetCameraAccessibility() const { return CameraAccessibility; }
    UDAGameplayAccessibilityAdapter* GetGameplayAccessibility() const { return GameplayAccessibility; }
    UDATutorialAccessibilityAdapter* GetTutorialAccessibility() const { return TutorialAccessibility; }
    UObject* GetGameplayCommandEndpoint() const { return GameplayCommandEndpoint.Get(); }
    virtual bool CaptureFounderHUDState(FDAFounderHUDSource& OutState, FString& OutError) const override;
    virtual bool CaptureCommandHUDState(FDACommandHUDSource& OutState, FString& OutError) const override;
private:
    bool ResetRouter(FString& OutError);
    void TeardownPlayerUI();
    void HandlePlayerControllerChanged(APlayerController* PlayerController);
    void HandleAccessibilityPolicyChanged(const FDAAccessibilityRuntimePolicy& Policy);
    bool DispatchDefaultCommand(const FDAUICommandRequest& Request, FString& OutError);
    bool BindEnhancedInput(UEnhancedInputComponent& Input, FString& OutError);
    void HandleInputAction(const FInputActionValue& Value, FName ActionId,
        int32 PayloadIndex, bool bControllerBinding);

    UPROPERTY(Transient) TObjectPtr<UDAUIScreenRouter> Router;
    UPROPERTY(Transient) TObjectPtr<UDAUICommandService> CommandService;
    UPROPERTY(Transient) TObjectPtr<UDAUIViewModelProvider> ViewModelProvider;
    UPROPERTY(Transient) TObjectPtr<UDARootUILayout> RootLayout;
    UPROPERTY(Transient) TObjectPtr<APlayerController> BoundPlayerController;
    UPROPERTY(Transient) TObjectPtr<UDAUIVisualAccessibilityAdapter> VisualAccessibility;
    UPROPERTY(Transient) TObjectPtr<UDACameraAccessibilityAdapter> CameraAccessibility;
    UPROPERTY(Transient) TObjectPtr<UDAInputAccessibilityAdapter> InputAccessibility;
    UPROPERTY(Transient) TObjectPtr<UDAGameplayAccessibilityAdapter> GameplayAccessibility;
    UPROPERTY(Transient) TObjectPtr<UDATutorialAccessibilityAdapter> TutorialAccessibility;
    UPROPERTY(Transient) TWeakObjectPtr<UObject> GameplayCommandEndpoint;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UInputAction>> RuntimeInputActions;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UInputMappingContext>> RuntimeInputContexts;
    TArray<uint32> InputBindingHandles;
    FDelegateHandle PlayerControllerChangedHandle;
    FDelegateHandle AccessibilityChangedHandle;
};
